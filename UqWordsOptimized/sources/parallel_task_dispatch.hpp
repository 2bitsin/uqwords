#pragma once

#include <thread>
#include <mutex>
#include <future>
#include <type_traits>
//#include <ranges>

#include "concurrent_queue.hpp"
#include "pinned_object.hpp"

struct parallel_task_dispatch: pinned_object
{
  using task_type = std::function<void(std::size_t index)>;

  using coqueue_type = concurrent_queue<task_type>;

  parallel_task_dispatch(size_t num_threads, size_t num_spins = 32)  
  : m_count   { num_threads },
    m_spins   { num_spins },
    m_index   { 0u },
    m_handles { std::make_unique_for_overwrite<std::jthread []>(num_threads) },
    m_queues  { std::make_unique_for_overwrite<coqueue_type []>(num_threads) }
  {
    std::for_each_n (m_handles.get(), m_count, [this, index = 0u] (auto& handle) mutable{
      handle = std::jthread { &parallel_task_dispatch::perform_tasks, this, m_breaks.get_token(), index++ };
    });
  }

 ~parallel_task_dispatch()
  {
    m_breaks.request_stop();
  }

  template <typename Task_type>
  requires (std::is_invocable_v<Task_type, std::size_t>)
  void enqueue(Task_type&& task)
  {
    m_active.fetch_add(1, std::memory_order::release);
    const auto number_of_iteration = m_spins * m_count ;
    const auto next_slot = m_index++ ;

    for (auto i = 0u; i < number_of_iteration; ++i) 
    {
      if (m_queues[(next_slot + i) % m_count]
        .try_push (std::forward<Task_type>(task)))
      {
        return;      
      }
    }
    m_queues[next_slot % m_count]
      .wait_push (std::forward<Task_type>(task));
  }

  template <typename Task_type, typename... Args>
  auto async(Task_type&& task, Args&&... args)
  {
    using result_type = std::invoke_result_t<std::decay_t<Task_type>, std::decay_t<Args>...>;
    using packaged_type = std::packaged_task<result_type()>;

    auto task_wrapper = std::make_shared<packaged_type> (std::bind (
      [task { std::forward<Task_type>(task) }] (Args& ... args) {
        return task (std::forward<Args>(args)...);
      },
      std::forward<Args>(args)...));

    auto task_future = task_wrapper->get_future();

    enqueue ([task_wrapper { std::move (task_wrapper) }] (std::size_t index) {
      (*task_wrapper)();
    });

    return task_future;
  }

  void wait_for_all()
  {
    for(auto i = 0; i < m_count; ++i)
      m_queues[i].wait_until_empty();
  }

  auto active() const
  {
    return m_active.load(std::memory_order::acquire);
  }

private:
  void perform_tasks(const std::stop_token& stop_token, std::size_t index)
  {
    const auto number_of_iteration = m_spins * m_count;
    
    while (!stop_token.stop_requested())
    {
      task_type current_task;
      for (auto i = 0u; i < number_of_iteration; ++i) 
      { 
        if (m_queues[(index + i) % m_count].try_pop (current_task)) 
          break;
      }
      
      if (!current_task && !m_queues[index % m_count].wait_pop (current_task))
        break;

      try
      {        
        current_task (index);
        m_active.fetch_sub(1, std::memory_order::release);
      }
      catch(const std::exception& ex)
      {
        assert(false);
        std::cerr << ex.what() << std::endl;      
      }
    }
  }

private:
  std::size_t                       m_count;
  std::size_t                       m_spins;
  std::atomic<std::size_t>          m_index;
  std::unique_ptr<std::jthread []>  m_handles;  
  std::unique_ptr<coqueue_type []>  m_queues;
  std::stop_source                  m_breaks;
  std::atomic<int>                  m_active;
};
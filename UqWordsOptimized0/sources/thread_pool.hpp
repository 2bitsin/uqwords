#pragma once

#include <thread>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>
#include <future>
#include <type_traits>

#include "concurrent_queue.hpp"
#include "pinned_object.hpp"

struct thread_pool: pinned_object
{
  using task_type = std::function<void(const std::stop_token&, size_t)>;
  using coqueue_type = concurrent_queue<task_type>;

  thread_pool(size_t num_threads, size_t num_spins = 32)  
  : m_count   { num_threads },
    m_spins   { num_spins },
    m_index   { 0u },
    m_handles { std::make_unique_for_overwrite<std::jthread []>(num_threads) },
    m_queues  { std::make_unique_for_overwrite<coqueue_type []>(num_threads) }
  {
    std::for_each_n (m_handles.get(), m_count, [this, index = 0u] (auto& handle) mutable{
      handle = std::jthread { &thread_pool::perform_tasks, this, m_breaks.get_token(), index++ };
    });
  }

 ~thread_pool()
  {
    m_breaks.request_stop();
  }

  template <typename Task_type>
  requires (std::is_invocable_v<Task_type, const std::stop_token&, size_t>)
  void task_dispatch(Task_type&& task)
  {
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
    using result_type = std::invoke_result_t<Task_type, Args...>;
    std::promise<result_type> promise;
    auto future = promise.get_future();
    task_dispatch ([&, p = std::move(promise), t = std::forward<Task_type>(task)] 
      (const std::stop_token& token, size_t index) mutable 
      {
        if constexpr (!std::is_void_v<result_type>) 
          p.set_value (t (std::forward<Args> (args)...));
        else {
          t (std::forward<Args> (args)...);
          p.set_value ();
        }
      }
    );
    return future;
  }

private:
  void perform_tasks(const std::stop_token& stop_token, size_t index)
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
      current_task (stop_token, index);
    }
  }

private:
  std::size_t                       m_count;
  std::size_t                       m_spins;
  std::atomic<std::size_t>          m_index;
  std::unique_ptr<std::jthread []>  m_handles;  
  std::unique_ptr<coqueue_type []>  m_queues;
  std::stop_source                  m_breaks;
};
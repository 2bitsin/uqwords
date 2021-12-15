#pragma once

#include <unordered_set>
#include <algorithm>
#include <ranges>
#include <vector>

#include "concurrent_queue.hpp"
#include "parallel_task_dispatch.hpp"
#include "chunk_loader.hpp"
#include "pinned_object.hpp"

template <typename _Reduce_target>
struct parallel_split_and_reduce: pinned_object
{
  using reduce_target_type = _Reduce_target;
  using reduce_merge_type = std::tuple<reduce_target_type, reduce_target_type>;

  parallel_split_and_reduce (std::uint32_t num_threads, std::uint32_t task_load_factor)
  : m_num_threads { num_threads },
    m_num_waiting { num_threads * task_load_factor },
    m_thread_pool { num_threads }
  {}

  auto apply_to_file_at_path(std::filesystem::path file_name, std::size_t block_size = 64*1024*1024)  
    -> reduce_target_type
  {
    using namespace std;
    using namespace chrono_literals;

    static constexpr auto is_future_ready = [] (auto&& f) -> bool
    {
      using namespace std;
      using namespace chrono_literals;
      return f.wait_for (0ms) == future_status::ready;
    };

    static constexpr auto future_not_ready = [] (auto&& f) -> bool
    {
      using namespace std;
      using namespace chrono_literals;
      return f.wait_for (0ms) != future_status::ready;
    };


    const auto the_chunk_size = block_size & mmap_wrapper::alignment_mask();
    chunk_loader the_chunk_loader { file_name, the_chunk_size };    
    reduce_target_type lhs, rhs;    
    deque<future<reduce_target_type>> partial_sets;
    vector<reduce_target_type> ready_partial_sets;

    while (!the_chunk_loader.empty())
    {
      m_num_waiting.acquire();

      while (!partial_sets.empty() && is_future_ready (partial_sets.front()) && ready_partial_sets.size() < 2)
      {
        ready_partial_sets.emplace_back (partial_sets.front().get());
        partial_sets.pop_front();        
      }
      
      if (ready_partial_sets.size() > 1)
      {
        auto the_future = m_thread_pool.async ([this] (auto the_sets) 
          -> reduce_target_type
        {
          m_num_waiting.release();
          return collapse_mulltiple_sets(the_sets);
        }, move (ready_partial_sets));
        continue;
      }
      

      chunk_loader::shared_chunk_type the_chunk;
      if (!(the_chunk = the_chunk_loader.next_shared(' ')))
        break;
      auto the_future = m_thread_pool.async ([this, the_chunk { std::move (the_chunk) }] () ->
        reduce_target_type
      {        
        m_num_waiting.release();        
        return reduce_chunk_to_word_set(*the_chunk);
      });

      partial_sets.emplace_back (move (the_future));      
    }

    while(partial_sets.size () > 1 )
    {            
      while (ready_partial_sets.size() < 2)
      {
        ready_partial_sets.emplace_back (partial_sets.front().get());
        partial_sets.pop_front();        
      }

      m_num_waiting.acquire();
      auto the_future = m_thread_pool.async ([this] (auto the_sets) 
        -> reduce_target_type
      {
        m_num_waiting.release();
        return collapse_mulltiple_sets(the_sets);
      }, move (ready_partial_sets));

      partial_sets.emplace_back (move (the_future));
      ready_partial_sets.reserve(10);
    } 
    return partial_sets.back().get();
  }

  auto reduce_chunk_to_word_set(const chunk_loader::chunk_type& the_chunk)
    -> reduce_target_type
  {
    using namespace std;
    reduce_target_type ws_local;
    the_chunk.split_into<string> (inserter (ws_local, ws_local.begin ()), ' ');
    return ws_local;
  }

  auto collapse_mulltiple_sets(std::vector<reduce_target_type>& the_merge)  
    -> reduce_target_type
  {
    reduce_target_type the_result;
    for (auto& item : the_merge)
      the_result.merge (move (item));
    return the_result;
  }

private:
  using semaphore_type = std::counting_semaphore<>;

  const std::size_t m_num_threads;
  semaphore_type m_num_waiting;
  parallel_task_dispatch m_thread_pool;
};

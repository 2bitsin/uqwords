#pragma once

#include <unordered_set>

#include "concurrent_queue.hpp"
#include "thread_pool.hpp"
#include "chunk_loader.hpp"

template <typename _Reduce_target>
struct parallel_split_and_reduce
{
  using reduce_target_type = _Reduce_target;

  parallel_split_and_reduce (std::uint32_t num_threads = std::thread::hardware_concurrency())
  : m_num_threads { num_threads },
    m_num_waiting { num_threads * 16u },
    m_thread_pool { num_threads }    
  {}

  auto apply_to_file_at_path(std::filesystem::path file_name, std::size_t block_size = 1024*1024)  
    -> reduce_target_type
  {
    const auto the_chunk_size = block_size & mmap_wrapper::alignment_mask();
    chunk_loader the_chunk_loader { file_name, the_chunk_size };

    while (!the_chunk_loader.empty())
    {
      m_num_waiting.acquire();
      auto the_chunk = the_chunk_loader.next_shared(' ');
      if (!the_chunk)
        break;
      m_thread_pool.enqueue ([this, the_chunk { std::move (the_chunk) }] (auto thread_index) {
        const auto ws_local = reduce_chunk_to_word_set(*the_chunk);
        m_num_waiting.release();
      });
    }
  
    return {};
  }

  auto reduce_chunk_to_word_set(const chunk_loader::chunk_type& the_chunk)
    -> reduce_target_type
  {
    using namespace std;
    reduce_target_type ws_local;
    the_chunk.split_into<string> (inserter (ws_local, ws_local.begin ()), ' ');
    return ws_local;
  }


private:
  const std::size_t m_num_threads;
  std::counting_semaphore<> m_num_waiting;
  thread_pool m_thread_pool;
};

#include <iostream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include <semaphore>
#include <ranges>
#include <list>
#include <iterator>
#include <cassert>

#include <fmt/format.h>

#include "concurrent_queue.hpp"
#include "thread_pool.hpp"
#include "chunk_loader.hpp"

struct word_set_scan
{
  using word_set_container = std::unordered_set<std::string>;

  struct merge_task_context
  {
    word_set_container m_lhs;
    word_set_container m_rhs;

    merge_task_context(word_set_container lhs, word_set_container rhs)
    : m_lhs { std::move(lhs) }
    , m_rhs { std::move(rhs) }
    {}    
  };  

  word_set_scan (std::size_t block_size = 1024*1024, std::uint32_t num_threads = std::thread::hardware_concurrency())
  : m_num_threads       { num_threads },
    m_tasks_waiting     { std::uint32_t(num_threads * 16u) },
    m_thread_pool       { num_threads },
    m_block_size        { block_size & mmap_wrapper::alignment_mask() }
  {}

  //~master_set_collector() {}
  
  static auto pop_first(auto& list)
  {
    auto item = std::move(list.front());
    list.pop_front();
    return item;
  }

  auto perform(std::filesystem::path file_name)  
    -> word_set_container
  {
    using namespace std;
    using namespace fmt;
    using namespace string_view_literals;
    using namespace string_literals;
    using namespace chrono_literals;

    chunk_loader the_chunk_loader { file_name, m_block_size };

    while (!the_chunk_loader.empty())
    {
      m_tasks_waiting.acquire();

      while (m_intermediate.size () > 1)
      {
        auto first  = m_intermediate.begin();
        auto second = std::next(first); 

        if (first->wait_for(0ms) == future_status::ready && second->wait_for(0ms) == future_status::ready)
        {
          auto context_ptr = std::make_shared<merge_task_context>(first->get(), second->get());
          m_intermediate.pop_front();
          m_intermediate.pop_front();

          m_intermediate.emplace_back(m_thread_pool.async([this] (auto context_ptr) {
            return merget_sets(*context_ptr);
          }, std::move (context_ptr)));
          continue;
        }        
        break;
      }
      auto chunk_shared = the_chunk_loader.next_shared(' ');
      if (!chunk_shared)
        break;
      m_intermediate.emplace_back(m_thread_pool.async ([this] (auto chunk_shared) {
        return reduce_chunk_to_word_set(*chunk_shared);
      }, std::move (chunk_shared)));
    }
    
    std::cout << m_intermediate.size() << " tasks remaining" << std::endl;
    while(m_intermediate.size() > 1)
    {
      auto context_ptr = std::make_shared<merge_task_context>(
        pop_first(m_intermediate).get(), 
        pop_first(m_intermediate).get());

      m_intermediate.emplace_back(m_thread_pool.async (
        [this] (auto context_ptr) { 
          return merget_sets(*context_ptr); 
        }, 
        std::move (context_ptr))); 
    }

    assert(m_intermediate.size() == 1);
    return std::move (m_intermediate.front().get());
  }

  auto reduce_chunk_to_word_set(const chunk_loader::chunk_type& the_chunk)
    -> word_set_container
  {
    word_set_container local_word_set;
    auto insert_iterator = std::inserter (local_word_set, local_word_set.begin());
    the_chunk.split_into<std::string> (insert_iterator, ' ');
    m_tasks_waiting.release();
    return local_word_set;
  }

  auto merget_sets(merge_task_context& context)
    -> word_set_container
  {
    auto result = std::move (context.m_lhs);    
    result.merge(std::move (context.m_rhs));
    return result;
  }

private:
  const std::size_t m_num_threads;
  std::counting_semaphore<> m_tasks_waiting;
  thread_pool m_thread_pool;
  const std::size_t m_block_size;
  std::list<std::future<word_set_container>> m_intermediate;
};

int main(int argc, char** argv)
{
  using namespace std;
  using namespace fmt;
  using namespace chrono;

  try
  {
    vector<filesystem::path> args{ argv, argv + argc };

    if (args.size() < 2)
      throw runtime_error(format("No file given as argument"s));
    const auto file_path = args.at(1);
    if (!filesystem::exists(file_path))
      throw runtime_error(format("File '{}' not found"s, args.at(1).string()));
    const auto file_size = filesystem::file_size(file_path);
    if (file_size < 1)
      throw runtime_error(format("File '{}' is empty!"s, args.at(1).string()));

    word_set_scan ws_scan;
    ws_scan.perform(file_path);

    return 0;
  }
  catch (const exception& ex)
  {
    cout << ex.what() << '\n';
  }
  return -1;
}
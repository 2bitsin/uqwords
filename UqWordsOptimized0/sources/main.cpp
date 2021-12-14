#include <iostream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <span>
#include <algorithm>
#include <semaphore>
#include <ranges>
#include <list>
#include <cassert>
#include <fmt/format.h>

#include "concurrent_queue.hpp"
#include "thread_pool.hpp"
#include "file_wrapper.hpp"

struct word_set_scan
{
  using word_set_container = std::unordered_set<std::string>;

  struct scan_task_context
  {
    mmap_wrapper m_mmap;
    std::string_view m_view;

    scan_task_context(mmap_wrapper mmap, std::string_view view)
    : m_mmap { std::move(mmap) }
    , m_view { std::move(view) }
    {}    
  };

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

  void perform(std::filesystem::path file_name)  
  {
    using namespace std;
    using namespace fmt;
    using namespace string_view_literals;
    using namespace string_literals;
    using namespace chrono_literals;

    auto file { file_wrapper::open (file_name, O_RDONLY) };
    
    auto bytes_remaining = file.size();    

    while (bytes_remaining > 0)
    {
      m_tasks_waiting.acquire();      
      auto bytes_to_take = std::min (bytes_remaining, m_block_size);
      auto start_here = file.size() - bytes_remaining;
      auto [handle, block_view] = file.map_string_view(start_here, start_here + bytes_to_take);
      auto last_space_off = block_view.find_last_of(' ') + 1;
      block_view = block_view.substr(0, last_space_off);
      bytes_remaining -= last_space_off;
      
      
      if (m_intermediate.size () > 1)
      {
        auto first  = m_intermediate.begin();
        auto second = std::next(first); 

        if (first->wait_for(0ms) == future_status::ready &&
            second->wait_for(0ms) == future_status::ready)
        {
          auto context_ptr = std::make_shared<merge_task_context>(std::move (first->get()), std::move (second->get()));
          m_intermediate.pop_front();
          m_intermediate.pop_front();

          m_intermediate.emplace_back(m_thread_pool.async([this] (auto context_ptr) {
            return perform_merge_task(*context_ptr);
          }, std::move (context_ptr)));
        }        
      }      

      // Using shared ptr to work around limitations of std::function later on :/
      auto context_ptr = std::make_shared<scan_task_context>(std::move (handle), std::move (block_view));
      m_intermediate.emplace_back(m_thread_pool.async ([this] (auto context_ptr) {
        return perform_set_collection_task(*context_ptr);        
      }, std::move (context_ptr)));
    }
    
    while(m_intermediate.size() > 1)
    {
      word_set_container lhs = std::move (m_intermediate.front().get());
      m_intermediate.pop_front();
      word_set_container rhs = std::move (m_intermediate.front().get());
      m_intermediate.pop_front();

      auto context_ptr = std::make_shared<merge_task_context>(std::move (lhs), std::move (rhs));

      m_intermediate.emplace_back(m_thread_pool.async (
        [this] (auto context_ptr) 
        {
          return perform_merge_task(*context_ptr);
        }, std::move (context_ptr))); 
    }

    assert(m_intermediate.size() == 1);
    auto master_set = std::move (m_intermediate.front().get());

    std::cout << master_set.size() << "\n";
  }

  auto perform_set_collection_task(scan_task_context& context)
    -> word_set_container
  {
    using namespace std;
    using namespace string_view_literals;
    using namespace string_literals;

    unordered_set<string> local_word_set;

    auto& view  = context.m_view;
    auto& mmap  = context.m_mmap;

    auto start_here = view.find_first_not_of(' ');    
    while (start_here != string_view::npos)
    {
      auto end_here = view.find_first_of(' ', start_here);
      auto word = view.substr(start_here, end_here - start_here);
      start_here = view.find_first_not_of(' ', end_here);
      local_word_set.emplace(word.begin(), word.end());      
    }
    m_tasks_waiting.release();
    return local_word_set;
  }

  auto perform_merge_task(merge_task_context& context)
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
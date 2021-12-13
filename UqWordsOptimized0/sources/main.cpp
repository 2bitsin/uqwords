#include <iostream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <chrono>
#include <span>
#include <algorithm>
#include <semaphore>
#include <ranges>

#include <fmt/core.h>

#include "concurrent_queue.hpp"
#include "thread_pool.hpp"
#include "file_wrapper.hpp"

using word_set_type = std::unordered_set<std::string>;

struct master_set_collector
{

  struct set_collection_context
  {
    mmap_wrapper m_mmap;
    std::string_view m_view;
    word_set_type m_words;  

    set_collection_context(mmap_wrapper mmap, std::string_view view)
    : m_mmap { std::move(mmap) }
    , m_view { std::move(view) }
    {}
  };

  master_set_collector (std::uint32_t num_threads = std::thread::hardware_concurrency())
  : m_pool { num_threads },
    m_tasks_waiting { std::uint32_t(num_threads * 8u) }
  {}

  //~master_set_collector() {}

  void process_file(std::filesystem::path file_name)  
  {
    using namespace std;
    using namespace fmt;
    using namespace string_view_literals;
    using namespace string_literals;
    using namespace chrono_literals;

    auto file { file_wrapper::open (file_name, O_RDONLY) };
    
    auto bytes_remaining = file.size();

    const auto block_size = (1024*1024) & mmap_wrapper::alignment_mask(); 
    const auto wait_threshold = std::thread::hardware_concurrency() * 8;

    while (bytes_remaining > 0)
    {
      m_tasks_waiting.acquire();      
      auto bytes_to_take = std::min (bytes_remaining, block_size);
      auto start_here = file.size() - bytes_remaining;
      auto [handle, block_view] = file.map_string_view(start_here, start_here + bytes_to_take);
      auto last_space_off = block_view.find_last_of(' ') + 1;
      block_view = block_view.substr(0, last_space_off);
      bytes_remaining -= last_space_off;
      
      // Using shared ptr to work around limitations of std::function later on :/
      auto context_ptr = std::make_shared<set_collection_context>(std::move (handle), std::move (block_view));

      m_result_queue.push_back (m_pool.async([this, context_ptr { std::move (context_ptr) }] () {
        perform_set_collection_task(*context_ptr);        
        m_tasks_waiting.release(); 
        return context_ptr;
      }));
    }
    word_set_type master_set;
    for(auto&& s: m_result_queue)
    {
      auto context_ptr = s.get();
      master_set.merge(context_ptr->m_words);
    }

    std::cout << master_set.size() << "\n";
  }

  void perform_set_collection_task(set_collection_context& context)
  {
    using namespace std;
    using namespace string_view_literals;
    using namespace string_literals;

    auto& view  = context.m_view;
    auto& words = context.m_words;
    auto& mmap  = context.m_mmap;

    auto start_here = view.find_first_not_of(' ');
    while (start_here != std::string_view::npos)
    {
      auto end_here = view.find_first_of(' ', start_here);
      auto word = view.substr(start_here, end_here - start_here);
      words.emplace(std::string (word.begin(), word.end()));
      start_here = view.find_first_not_of(' ', end_here);
    }

  }

private:
  std::counting_semaphore<> m_tasks_waiting;
  thread_pool m_pool;
  std::deque<std::future<std::shared_ptr<set_collection_context>>> m_result_queue;
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

    master_set_collector producer;

    producer.process_file(file_path);

    return 0;
  }
  catch (const exception& ex)
  {
    cout << ex.what() << '\n';
  }
  return -1;
}
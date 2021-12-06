#include <iostream>
#include <fstream>
#include <deque>
#include <vector>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <chrono>
#include <span>

#include <fmt/core.h>

#include "concurrent_queue.hpp"
#include "thread_pool.hpp"
#include "file_wrapper.hpp"
#include "mmap_wrapper.hpp"

struct parse_and_hash_task
{
  mmap_wrapper                      m_chunk_mapping;
  std::uint64_t                     m_mapping_offset;
  std::size_t                       m_mapping_size;
  std::uint64_t                     m_chunk_offset;
  std::size_t                       m_chunk_size;
  std::unordered_set<std::string>   m_current_word_set;
  std::future<void>                 m_future;
};

struct merge_sets_task
{
  std::vector<parse_and_hash_task*> m_sets_to_merge;
  std::unordered_set<std::string>   m_current_word_set;
  std::future<void>                 m_future;
};


int main(int argc, char** argv)
{
  using namespace std;
  using namespace string_view_literals;
  using namespace string_literals;
  using namespace chrono;

  const auto block_size { (1024*1024) & ~(mmap_wrapper::aligment() - 1) };

  using namespace fmt;
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

    thread_pool pool { std::thread::hardware_concurrency() };

    concurrent_queue<parse_and_hash_task> parse_tasks_queue;
    concurrent_queue<merge_sets_task> merge_tasks_queue;

    file_wrapper file = file_wrapper::open (file_path, O_RDONLY);

    auto data_remaining = 1ull * file.size();
    auto current_offset = 0ull;

    auto [map, span] = mmap_wrapper::map_range<uint8_t>(file, 0x637, 0x637+16, O_RDONLY, MAP_PRIVATE);

    return 0;
  }
  catch (const exception& ex)
  {
    cout << ex.what() << '\n';
  }
  return -1;
}
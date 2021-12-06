#include <iostream>
#include <fstream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <chrono>

#include <fmt/core.h>

#include "concurrent_queue.hpp"
#include "thread_pool.hpp"
#include "file_wrapper.hpp"
#include "mmap_wrapper.hpp"

int main(int argc, char** argv)
{
  using namespace std;
  using namespace string_view_literals;
  using namespace string_literals;
  using namespace chrono;

  const auto block_size { 4 *  1024u };

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

    const auto num_blocks = file_size / block_size;
    
    file_wrapper file = file_wrapper::open(file_path, O_RDONLY);

    mmap_wrapper m0 = mmap_wrapper::mmap(file, 4096, 0);
    mmap_wrapper m1 = mmap_wrapper::mmap(file, 4096, 7777);

    auto s0 = m0.as_span<uint8_t>();
    auto s1 = m1.as_span<uint8_t>();

    for(auto&& it : s0)
    {
      cout << it << ' ';
    }
    cout << "========================================\n";
    for(auto&& it : s1)
    {
      cout << it << ' ';
    }

    return 0;
  }
  catch (const exception& ex)
  {
    cout << ex.what() << '\n';
  }
  return -1;
}
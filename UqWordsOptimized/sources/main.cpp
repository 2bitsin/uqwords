#include <iostream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <algorithm>
#include <semaphore>
#include <ranges>
#include <list>
#include <iterator>
#include <cassert>

#include <fmt/format.h>

#include "parallel_split_and_reduce.hpp"

auto args_validate_file_path(const auto& args, std::size_t n)
  -> std::filesystem::path
{
  using namespace fmt;
  using namespace std;
  const std::filesystem::path file_path { args.at(1) };   
  if (args.size() < 2)
    throw runtime_error(format("No file given as argument"s));
  if (!filesystem::exists(file_path))
    throw runtime_error(format("File '{}' not found"s, file_path.string()));
  const auto file_size = filesystem::file_size(file_path);
  if (file_size < 1)
    throw runtime_error(format("File '{}' is empty!"s, file_path.string()));
  return file_path;
}

int main(int argc, char** argv)
{
  using namespace std;
  using container_type = unordered_set<string>;

  try
  {
    vector<string_view> args{ argv, argv + argc };
    const auto file_path = args_validate_file_path(args, 1);
    parallel_split_and_reduce<container_type> widget { std::thread::hardware_concurrency() };
    cout << widget.apply_to_file_at_path(file_path).size() << "\n";

    return 0;
  }
  catch (const exception& ex)
  {
    cout << ex.what() << '\n';
  }
  return -1;
}
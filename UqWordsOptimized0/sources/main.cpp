#include <iostream>
#include <fstream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <fmt/core.h>
#include <unordered_set>
#include <chrono>

#include "concurrent_queue.hpp"
#include "thread_pool.hpp"

int main(int argc, char** argv)
{
  using namespace std;
  using namespace string_view_literals;
  using namespace string_literals;
  using namespace chrono;

  using namespace fmt;
  try
  {
    vector<filesystem::path> args{ argv, argv + argc };

    if (args.size() < 2)
      throw runtime_error(format("No file given as argument"s));
    if (!filesystem::exists(args.at(1)))
      throw runtime_error(format("File '{}' not found"s, args.at(1).string()));
    if (!filesystem::file_size(args.at(1)))
      throw runtime_error(format("File '{}' is empty!"s, args.at(1).string()));


    thread_pool pool { std::thread::hardware_concurrency() };

    auto sum = 0;
    for(auto i = 0; i < 1000; ++i) {
      auto result = pool.async([] () {
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));
        return 42;
      });
      sum += result.get();
    }

    std::cout << sum << std::endl;


    //auto dt = high_resolution_clock::now() - t0;

    //cout << "dT = " << duration_cast<milliseconds>(dt).count() * 1e-3 << " s\n";

    return 0;
  }
  catch (const exception& ex)
  {
    cout << ex.what() << '\n';
  }
  return -1;
}
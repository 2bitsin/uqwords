#include <iostream>
#include <fstream>
#include <vector>
#include <string_view>
#include <filesystem>
#include <fmt/core.h>
#include <unordered_set>
#include <chrono>

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

    ifstream in_file(args.at(1), ios::binary);

    string word;
    unordered_set<string> word_set;
    auto t0 = high_resolution_clock::now();
    while (getline(in_file, word, ' ')) {
      if (word.empty())
        continue;
      word_set.emplace(move(word));
    }
    auto dt = high_resolution_clock::now() - t0;

    cout << word_set.size() << '\n';

    cout << "dT = " << duration_cast<milliseconds>(dt).count() * 1e-3 << " s\n";

    return 0;
  }
  catch (const exception& ex)
  {
    cout << ex.what() << '\n';
  }
  return -1;
}
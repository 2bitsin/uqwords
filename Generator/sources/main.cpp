#include <iostream>
#include <fstream>
#include <string_view>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <format>
#include <cctype>
#include <random>
#include <algorithm>

auto trim_file(std::string w) 
{
  auto begin = w.find_first_not_of(' ');
  auto end = w.find_last_not_of(' ');
  return w.substr(begin, end - begin);
}

auto filter_string(std::string_view sv)
{
  std::string tmp;
  tmp.reserve(sv.size());
  for (auto&& c : sv) {
    if (!isalpha(c))
      continue;
    tmp.push_back(tolower(c));
  }
  return tmp;
}

int main(int, char**)
{
  using namespace std;
  using namespace string_literals;
  using namespace string_view_literals;
  
  unordered_set<string> word_set;
  vector<string> word_table;

  ifstream wordlist_file ("words.txt");
  string word;

  while (getline(wordlist_file, word))
  {
    word = filter_string(word);
    if (word.empty())
      continue;
    auto[it, success] = word_set.emplace(word);
    if (!success) 
      continue;
    //cout << "\"" << word << "\"\n";
  }
  word_table.reserve(word_set.size());  
  for (auto it = word_set.begin(); it != word_set.end(); )
    word_table.emplace_back(std::move(word_set.extract(it++).value()));

  random_device rd;
  mt19937_64 mt_rand{ rd() };
  auto wt_part_size = (word_table.size() + 63) / 64;
  uniform_int_distribution<unsigned long long> rand_word{ 0, wt_part_size - 1 };

  cout << "Building from " << word_table.size() << " words...\n";
  
  ofstream test_file("test_case.txt", ios::binary);

  uint64_t size_so_far  { 0 };
                        //0x800000000ull
  uint64_t target_size  { 0x40000000ull  };
  uint64_t stat_update  { 0 };

  while (size_so_far < target_size)
  {
    auto j = size_so_far / (16 * 1024 * 1024);
    auto word = word_table.at((wt_part_size * j + rand_word(mt_rand)) % word_table.size());

    auto space = "                "sv.substr(0, 1 + (mt_rand() & 0x1));
    size_so_far += (space.size() + word.size());
    stat_update += (space.size() + word.size());

    test_file << word;
    test_file << space;

    word_set.emplace(std::move (word));

    if (stat_update > 1024 * 1024)
    {
      shuffle(word_table.begin(), word_table.end(), mt_rand);
      stat_update = 0;
      cout << "Progress so far : " << size_so_far << " | " << (int)((size_so_far * 100.0) / target_size) << " % | words used : " << word_set.size() << "\r";
    }

  }
  
  
  

  return 0;
}
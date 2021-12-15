#pragma once

#include <algorithm>
#include <ranges>
#include <cassert>
#include <array>
#include <memory>

struct ws_container
{
  using node_type = std::array<uint32_t, 'z' - 'a' + 1>;

  ws_container(std::size_t preallocate = (1ull*1024ull*1024ull*1024ull)/sizeof(node_type))
  : m_pool_size { preallocate },
    m_pool_next { 0 }, 
    m_pool_node { std::make_unique_for_overwrite<node_type[]>(preallocate) }
  {
    allocate_node();
  }

 ~ws_container() {};

  std::size_t allocate_node()
  {
    assert(m_pool_next < m_pool_size);
    if (m_pool_next >= m_pool_size)
      throw std::runtime_error("ws_container: pool exhausted");
    auto index = m_pool_next++;
    std::ranges::fill(m_pool_node[index], 0);
    return index;
  }

  bool emplace(std::string_view word)
  {
    if (word.size() < 1)
      return false;
    
    const auto first = word.substr(0, word.size() - 1);
    const auto last = word.back();

    auto* current_node = &m_pool_node[0];

    for(auto chr : first)
    {
      assert (chr >= 'a' && chr <= 'z');
      chr -= 'a';
      auto index = (*current_node)[chr] & 0x7fffffffu;
      if (!index) {
        assert(m_pool_next < m_pool_size);
        index = allocate_node();
        (*current_node)[chr] = index;       
      }
      current_node = &m_pool_node[index];
    }

    assert(last >= 'a' && last <= 'z');
    const auto llast = last - 'a';

    if ((*current_node)[llast] & 0x80000000u)
      return false;
    (*current_node)[llast] |= 0x80000000u;
    return true;
  }

  template <typename _Callee>
  void iterate(_Callee&& callee) const
  {
    iterate_ ("", 0, callee);
  }

  void merge(const ws_container& what)
  {
    what.iterate([&](std::string_view word) {
      emplace(word);
    });
  }

  auto size() const -> std::size_t 
  {   
    std::size_t count = 0;
    iterate([&count](std::string_view) {
      ++count;
    });
    return count;
  }

private:
  template <typename _Callee>
  void iterate_ (std::string so_far, std::size_t index, _Callee&& callee) const
  {
    auto& the_node = m_pool_node[index];
    for (auto i = 0; i < ('z' - 'a' + 1); ++i)
    {
      auto new_str = so_far;
      new_str.push_back('a' + i);
      if (the_node[i] & 0x80000000u)
        callee (new_str);

      if (the_node[i] & 0x7fffffffu)
        iterate_ (new_str, the_node[i] & 0x7fffffffu, callee);
    }
  }

  std::unique_ptr<node_type []> m_pool_node;
  std::size_t m_pool_size = 0;
  std::size_t m_pool_next = 0;
};
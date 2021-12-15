#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <filesystem>

#include "file_wrapper.hpp"

struct chunk_loader
{
  struct chunk_type
  {
    chunk_type(mmap_wrapper handle, std::string_view string)
    : m_handle { std::move (handle) },
      m_string { std::move (string) }
    {}

    auto as_string_view () const -> std::string_view 
    { 
      return m_string; 
    }

    template <typename _Transform = std::string_view, typename _It_type>
    auto split_into(_It_type out_it, char delimiter = ' ') const
    {
      auto view = as_string_view();
      auto start_here = view.find_first_not_of(delimiter);    
      while (start_here != std::string_view::npos)
      {
        auto end_here = view.find_first_of(delimiter, start_here);
        auto word = view.substr(start_here, end_here - start_here);
        start_here = view.find_first_not_of(delimiter, end_here);        
        *(out_it++) = _Transform (word);
      }
    }

  private:
    mmap_wrapper      m_handle;
    std::string_view  m_string;
  };

  using shared_chunk_type = std::shared_ptr<chunk_type>;

  chunk_loader(std::filesystem::path path, std::size_t chunk_size)
  : m_file { file_wrapper::open(path, O_RDONLY) },
    m_chunk_size { chunk_size },
    m_bytes_left { m_file.size() }  
  {}

  auto next (char delimiter = ' ') -> std::optional<chunk_type> 
  {
    if (m_bytes_left <= 0) {
      return std::nullopt;
    }

    auto bytes_to_take = std::min (m_bytes_left, m_chunk_size);
    auto start_here = m_file.size() - m_bytes_left;
    auto [handle, s_view] = m_file.map_string_view(start_here, start_here + bytes_to_take);
    auto last_space_off = s_view.find_last_of(delimiter) + 1;
    
    s_view = s_view.substr(0, last_space_off);
    m_bytes_left -= last_space_off;    

    return chunk_type { std::move (handle), std::move (s_view) };
  }

  auto next_shared (char delimiter = ' ') -> std::shared_ptr<chunk_type>
  {
    auto maybe_chunk = (*this).next(delimiter);
    if (maybe_chunk.has_value ())
      return std::make_shared<chunk_type>(std::move (maybe_chunk.value ()));
    return {};
  }

  auto bytes_left () const -> std::size_t { return m_bytes_left; }

  auto empty () const -> bool { return m_bytes_left == 0; }

private:
  file_wrapper  m_file;  
  std::size_t   m_chunk_size;
  std::uint64_t m_bytes_left;
};
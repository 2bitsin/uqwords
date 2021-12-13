#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>
#include <filesystem>
#include <string_view>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "mmap_wrapper.hpp"

struct file_wrapper
{
  file_wrapper(int fd) noexcept: m_fd{ fd } {}  
  file_wrapper() noexcept: file_wrapper{ -1 } {}

  file_wrapper(const file_wrapper&) = delete;
  file_wrapper& operator=(const file_wrapper&) = delete;

  file_wrapper(file_wrapper&& other) noexcept: m_fd{ std::exchange(other.m_fd, -1) } {}
  auto operator = (file_wrapper&& other) noexcept -> file_wrapper&
  {
    file_wrapper tmp { std::move(other) };  
    swap(tmp);
    return *this;
  }  

  ~file_wrapper()
  {
    if (m_fd >= 0)
      close(m_fd);    
  }

  void swap(file_wrapper& other) noexcept
  {
    std::swap(m_fd, other.m_fd);
  }

  template <typename... Args>
  static file_wrapper create(std::filesystem::path file_path, Args... args)
  {
    auto file_path_as_string = file_path.string();
    auto fd = ::creat(file_path_as_string.data(), args...);
    if (fd < 0)
      throw std::system_error { errno, std::system_category() };
    return file_wrapper { fd };
  }

  template <typename... Args>
  static file_wrapper open(std::filesystem::path file_path, Args ... args)
  {
    auto file_path_as_string = file_path.string();
    auto fd = ::open(file_path_as_string.data(), args...);
    if (fd < 0)
      throw std::system_error { errno, std::system_category() };
    return file_wrapper { fd };  
  }

  auto size () const -> uint64_t
  {
    struct ::stat64 st;
    if (fstat64 (m_fd, &st) < 0)
      throw std::system_error { errno, std::system_category() };
    return st.st_size;
  } 

  auto map (std::size_t size = 0, std::size_t offset = 0, int prot = PROT_READ, int flags = MAP_PRIVATE)
    -> mmap_wrapper;

  template <typename T = std::byte>
  auto map_span (std::uint64_t begin = 0, std::uint64_t end = 0, int prot = PROT_READ, int flags = MAP_PRIVATE)
    -> std::tuple<mmap_wrapper, std::span<T>>;

  template <typename T = char>
  auto map_string_view(uint64_t begin, uint64_t end, int prot = PROT_READ, int flags = MAP_SHARED)  
    -> std::tuple<mmap_wrapper, std::basic_string_view<T>>;


  auto get() const noexcept { return m_fd; }
private:
  int m_fd;
};

#include "mmap_wrapper_impl.hpp"
#include "file_wrapper_impl.hpp"

#pragma once

#include "file_wrapper.hpp"

#include <cstdint>
#include <cstddef>
#include <utility>
#include <span>

#include <sys/mman.h>

struct mmap_wrapper
{
  mmap_wrapper(const mmap_wrapper&) = delete;
  mmap_wrapper& operator=(const mmap_wrapper&) = delete;
  mmap_wrapper() noexcept: mmap_wrapper(nullptr, 0) {}

  mmap_wrapper(void* addr, size_t size) noexcept
  : m_addr { addr },
    m_size { size }
  {}

  mmap_wrapper(mmap_wrapper&& other) noexcept
  : m_addr { std::exchange (other.m_addr, nullptr) },
    m_size { std::exchange (other.m_size, 0) }  
  {}

  auto operator = (mmap_wrapper&& other) noexcept -> mmap_wrapper&
  {
    mmap_wrapper tmp { std::move (other) };
    swap (tmp);  
    return *this;
  }

  void swap (mmap_wrapper& other) noexcept
  {
    std::swap (m_addr, other.m_addr);
    std::swap (m_size, other.m_size);
  }

  ~mmap_wrapper()
  {
    if (m_addr != nullptr && m_size != 0) {
      ::munmap(m_addr, m_size);
    }    
  }

  template <typename T>
  auto as_span() const noexcept -> std::span<T>
  {
    return { reinterpret_cast<T*>(m_addr), m_size / sizeof(T) };
  }

  static auto mmap(const file_wrapper& file, size_t size = 0, off_t offset = 0, int prot = PROT_READ, int flags = MAP_PRIVATE)
    -> mmap_wrapper 
  {
    size = size ? size : file.size();
    auto mapped_addr = ::mmap (nullptr, size, prot, flags, file.get(), offset);
    if (mapped_addr == MAP_FAILED) {
      throw std::system_error { errno, std::system_category() };
    }
    return mmap_wrapper(mapped_addr, size);
  }

  static auto aligment() noexcept -> size_t
  {
    return ::sysconf(_SC_PAGESIZE);
  }

private:
  void*   m_addr { nullptr };
  size_t  m_size { 0 };
};
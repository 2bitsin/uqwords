#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>
#include <span>
#include <string_view>

#include <sys/mman.h>


struct file_wrapper;

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

  template <typename T = void>
  auto addr () const noexcept -> T* {
     return (T*)m_addr; 
  }  

  template <typename T = void>
  auto size () const noexcept -> size_t { 
    if constexpr (std::is_same_v<T, void>) {
      return m_size;
    } else {
      return m_size / sizeof(T);
    }
    return m_size; 
  }   

  template <typename T>
  auto as_span() const noexcept -> std::span<T>
  {
    return { addr<T>(), size<T>() };
  }

  static auto map(const file_wrapper& file, size_t size = 0, size_t offset = 0, int prot = PROT_READ, int flags = MAP_PRIVATE) 
    -> mmap_wrapper;

  template <typename T = std::byte>
  static auto map_span(const file_wrapper& file, uint64_t begin, uint64_t end, int prot = PROT_READ, int flags = MAP_PRIVATE) 
    -> std::tuple<mmap_wrapper, std::span<T>>;

  template <typename T = char>
  static auto map_string_view(const file_wrapper& file, uint64_t begin, uint64_t end, int prot = PROT_READ, int flags = MAP_SHARED)  
    -> std::tuple<mmap_wrapper, std::basic_string_view<T>>;

  static auto alignment_size() noexcept -> size_t
  {
    // I assume this is always power of two
    return ::sysconf(_SC_PAGESIZE);
  }

  static auto alignment_mask() noexcept -> size_t
  {
    return ~(alignment_size() - 1);
  }
private:
  void*   m_addr { nullptr };
  size_t  m_size { 0 };
};


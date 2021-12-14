#pragma once 

auto file_wrapper::map (std::size_t size, std::size_t offset, int prot, int flags)
  -> mmap_wrapper
{
  return mmap_wrapper::map(*this, size, offset, prot, flags);
}

template <typename T>
auto file_wrapper::map_span (std::uint64_t begin, std::uint64_t end, int prot, int flags)
  -> std::tuple<mmap_wrapper, std::span<T>>
{
  return mmap_wrapper::map_span<T>(*this, begin, end, prot, flags);
}

template <typename T>
auto file_wrapper::map_string_view(uint64_t begin, uint64_t end, int prot, int flags)  
  -> std::tuple<mmap_wrapper, std::basic_string_view<T>>
{
  return mmap_wrapper::map_string_view<T>(*this, begin, end, prot, flags);
}

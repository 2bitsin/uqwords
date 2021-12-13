#pragma once 

auto mmap_wrapper::map(const struct file_wrapper& file, size_t size, size_t offset, int prot, int flags) 
  -> mmap_wrapper
{
  size = size ? size : file.size();
  auto mapped_addr = ::mmap (nullptr, size, prot, flags, file.get(), offset);
  if (mapped_addr == MAP_FAILED) {
    throw std::system_error { errno, std::system_category() };
  }
  return mmap_wrapper(mapped_addr, size);
}

template <typename T>
auto mmap_wrapper::map_span(const file_wrapper& file, uint64_t begin, uint64_t end, int prot, int flags)  
  -> std::tuple<mmap_wrapper, std::span<T>>
{
  // I assume this is always power of two
  static const auto align_size { mmap_wrapper::alignment_size() };
  static const auto align_mask { mmap_wrapper::alignment_mask() };
  begin *= sizeof(T);
  end *= sizeof(T);
  const auto start_offset = begin & align_mask;
  const auto end_offset = std::min ((end + align_size - 1) & align_mask, file.size());
  const auto length = end_offset - start_offset;
  const auto span_begin = (begin - start_offset)/sizeof(T);
  const auto span_length = (end - begin)/sizeof(T);
  auto the_map = mmap_wrapper::map(file, length, start_offset, prot, flags);
  auto the_span = the_map.as_span<T>();
  return { std::move(the_map), the_span.subspan(span_begin, span_length) };
}

template <typename T>
auto mmap_wrapper::map_string_view(const file_wrapper& file, uint64_t begin, uint64_t end, int prot, int flags)  
  -> std::tuple<mmap_wrapper, std::basic_string_view<T>>
{
  auto [handle, span] = mmap_wrapper::map_span<T>(file, begin, end, prot, flags);

  return { std::move (handle), std::basic_string_view<T>{ span.data(), span.size() }};
}


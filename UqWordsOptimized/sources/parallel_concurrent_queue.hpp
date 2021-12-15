#pragma once

#include "concurrent_queue.hpp"
#include "pinned_object.hpp"

#include <ranges>

template <typename _Value_type>
struct parallel_concurrent_queue: pinned_object
{
  using value_type = _Value_type;
  using queue_type = concurrent_queue<_Value_type>;

  parallel_concurrent_queue (std::uint32_t num_threads = std::thread::hardware_concurrency(), std::size_t n_spins = 8)
  : m_num_threads { num_threads },
    m_num_retries { n_spins },
    m_item_queues { std::make_unique<queue_type[]> (num_threads) }
  {}

  template <typename _Vtype>
  [[nodiscard]]
  bool try_push(_Vtype&& value, std::size_t bucket_index = 0) 
  {
    using namespace std;
    const auto q = m_num_retries * m_num_threads;
    for(auto r : ranges::views::iota(0u, q))
    {
      const auto thread_index = (bucket_index + r) % m_num_threads;
      if (m_item_queues[thread_index].try_push(forward<_Vtype> (value)))
        return true;
    }
    return false;
  }  

  template <typename _Vtype>
  [[nodiscard]]
  bool push(_Vtype&& value, std::size_t bucket_index = 0) 
  {
    using namespace std;
    if (try_push(forward<_Vtype> (value), bucket_index))
      return true;
    const auto i = bucket_index % m_num_threads;
    return m_item_queues[i].wait_push (forward<_Vtype> (value)); 
  }  

  template <typename _Vtype>
  [[nodiscard]]
  bool try_pop(_Vtype& out, std::size_t bucket_index = 0)
  {
    using namespace std;
    const auto q = m_num_retries * m_num_threads;
    for(auto r : ranges::views::iota(0u, q))
    {
      const auto thread_index = (bucket_index + r) % m_num_threads;
      if (m_item_queues[thread_index].try_pop(out))
        return true;
    }
    return false;
  }

  template <typename _Vtype>
  [[nodiscard]]
  bool pop(_Vtype& out, std::size_t bucket_index = 0)
  {
    using namespace std;
    if (try_pop(out, bucket_index))
      return true;
    const auto i = bucket_index % m_num_threads;
    return m_item_queues[i].wait_pop (out);
  }

  [[nodiscard]]
  bool empty() const
  {
    using namespace std;
    for(const auto q : ranges::views::iota(0u, m_num_threads))
      if (!m_item_queues[q].wait_check_empty())
        return false;
    return true;
  }

private:
  const std::size_t m_num_threads;
  const std::size_t m_num_retries;
  std::unique_ptr<queue_type[]> m_item_queues;  
};
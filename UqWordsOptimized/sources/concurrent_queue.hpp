#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>

#include "pinned_object.hpp"

template <typename Item_type>
struct concurrent_queue: pinned_object
{
  template <typename Current_item_type>
  requires (std::is_convertible_v<Current_item_type, Item_type>)
  [[nodiscard]]
  bool try_push (Current_item_type&& item)
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex, std::try_to_lock };    
    if (!hold_lock)
      return false;
    m_items.emplace_back (std::forward<Current_item_type>(item));
    m_ready.notify_one ();
    return true;
  }

  [[nodiscard]]
  bool try_pop (Item_type& output_item)
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex, std::try_to_lock };    
    if (!hold_lock || m_items.empty ())
      return false;
    output_item = std::move (m_items.front ());
    m_items.pop_front ();
    return true;
  }

  [[nodiscard]]
  bool wait_check_empty () const
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };
    return m_items.empty ();
  }

  template <typename Current_item_type>  
  requires (std::is_convertible_v<Current_item_type, Item_type>)
  bool wait_push (Current_item_type&& item)  
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };
    m_items.emplace_back (std::forward<Current_item_type>(item));
    m_ready.notify_one ();
    return true;
  }

  [[nodiscard]]
  bool wait_pop (Item_type& output_item)
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };

    while (m_items.empty () && !m_done) {
      m_empty.notify_all ();
      m_ready.wait (hold_lock);
    }
    
    if (!m_done)
    {
      output_item = std::move (m_items.front ());
      m_items.pop_front ();
      return true;
    }
    
    m_empty.notify_all ();
    return false;
  }  

  void wait_until_empty()
  {
    std::unique_lock hold_lock { m_mutex };
    while (!m_items.empty ())
      m_empty.wait (hold_lock); 
  }

 ~concurrent_queue ()
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };
    m_done = true;
    m_ready.notify_all ();
  }

private:  
  std::deque<Item_type>   m_items;
  mutable std::mutex      m_mutex;
  std::condition_variable m_ready;  
  std::atomic<bool>       m_done;
  
  std::condition_variable m_empty;
};
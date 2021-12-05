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
  bool try_push (Current_item_type&& item)
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex, std::try_to_lock };    
    if (!hold_lock)
      return false;
    m_items.push_back (std::forward<Current_item_type>(item));
    m_ready.notify_one ();
    return true;
  }

  bool try_pop (Item_type& output_item)
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex, std::try_to_lock };    
    if (!hold_lock || m_items.empty ())
      return false;
    output_item = std::move (m_items.front ());
    m_items.pop_front ();
    return true;
  }

  bool wait_check_empty () const
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };
    return m_items.empty ();
  }

  template <typename Current_item_type>  
  requires (std::is_convertible_v<Current_item_type, Item_type>)
  void wait_push (Current_item_type&& item)
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };
    m_items.push_back (std::forward<Current_item_type>(item));
    m_ready.notify_one ();
  }

  bool wait_pop (Item_type& output_item)
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };

    while (m_items.empty () && !m_done) 
      m_ready.wait (hold_lock);
    
    if (!m_done)
    {
      output_item = std::move (m_items.front ());
      m_items.pop_front ();
      return true;
    }

    return false;
  }  

 ~concurrent_queue ()
  {
    std::unique_lock<std::mutex> hold_lock { m_mutex };
    m_done = true;
    m_ready.notify_all ();
  }

private:  
  std::deque<Item_type>   m_items;
  std::mutex              m_mutex;
  std::condition_variable m_ready;
  std::atomic<bool>       m_done;
};
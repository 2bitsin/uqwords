#pragma once

struct pinned_object
{
  pinned_object () = default;
  pinned_object (const pinned_object&) = delete;
  pinned_object& operator = (const pinned_object&) = delete;
  pinned_object(pinned_object&&) = delete;
  pinned_object& operator = (pinned_object&&) = delete;
};
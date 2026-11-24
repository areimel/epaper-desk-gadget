#pragma once
#if FEATURE_DASHBOARD
#include <stdint.h>

#include "FeedConfig.h"

// The feed on LittleFS (the `spiffs` partition), as /feed.json in the storage shape (calendar
// link included). Loop thread only. The calendar link is a secret: file contents are never logged.
namespace feedfile {
bool begin();  // mounts, formatting an unformatted partition; false if storage is unusable
enum class Load : uint8_t {
  Ok,          // out holds the stored feed
  Missing,     // no file, or it was corrupt/invalid and has been renamed to /feed.bad
  Unreadable,  // storage or memory failed; the file may be fine and must not be overwritten
};
// Reads and validates /feed.json into out; out is untouched unless the result is Ok.
Load load(FeedConfig& out);
// Writes /feed.tmp, checks its length, then renames it over /feed.json. On any failure returns
// false and the previous /feed.json is intact.
bool save(const FeedConfig& feed);
}  // namespace feedfile
#endif

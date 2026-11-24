#pragma once
#if FEATURE_DASHBOARD
#include "FeedConfig.h"

// The feed on LittleFS (the `spiffs` partition), as /feed.json in the storage shape (calendar
// link included). Loop thread only. The calendar link is a secret: file contents are never logged.
namespace feedfile {
bool begin();  // mounts, formatting an unformatted partition; false if storage is unusable
// Reads and validates /feed.json into out. Missing: false, out untouched. Corrupt or invalid:
// renamed to /feed.bad (replacing an older one), false, out untouched.
bool load(FeedConfig& out);
// Writes /feed.tmp, checks its length, then renames it over /feed.json. On any failure returns
// false and the previous /feed.json is intact.
bool save(const FeedConfig& feed);
}  // namespace feedfile
#endif

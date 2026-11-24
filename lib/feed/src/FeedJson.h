#pragma once
#include <ArduinoJson.h>
#include <stddef.h>

#include "FeedConfig.h"

// Feed <-> JSON. Pure: no Arduino headers, no I/O. The JSON shape is docs/api.md.
namespace feed {

struct Result {
  bool ok;
  char path[48];   // field at fault, e.g. "messages[3].text"; "" when ok or not field-specific
  char error[64];  // plain sentence for a person, e.g. "Text is too long."; "" when ok
};

// Parses and validates a whole feed document. `current` supplies the stored calendar link when
// the document omits it. On failure `out` is left exactly as it was. `alloc` is only for tests
// that measure heap use; nullptr uses the default (malloc). Not thread-safe (see FeedJsonRead.cpp).
// maxBytes is the largest document accepted: FEED_JSON_MAX for a request; the stored file may be
// URL_MAX longer because it carries the calendar link.
Result fromJson(const char* json, size_t n, const FeedConfig& current, FeedConfig& out,
                ArduinoJson::Allocator* alloc = nullptr, size_t maxBytes = limits::FEED_JSON_MAX);

// Writes the feed as JSON. redact = true is the API shape ("calendar":{"set":bool}); redact =
// false is the storage shape ("calendar":{"url":"..."}). Returns bytes written (excluding the
// NUL), or 0 if cap is too small. The storage shape is up to URL_MAX bytes longer than the API
// shape when a calendar link is set, so size buffers as FEED_JSON_MAX + URL_MAX.
size_t toJson(const FeedConfig& feed, bool redact, char* out, size_t cap);

}  // namespace feed

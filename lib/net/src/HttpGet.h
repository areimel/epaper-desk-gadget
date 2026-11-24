#pragma once
#if FEATURE_DASHBOARD
#include <stddef.h>

#include "ByteSink.h"

// One HTTPS GET into a caller's bounded buffer. Fetch task only; one call at a time (one TLS
// session). The certificate is verified against the core's built-in CA bundle.
namespace net {
struct GetResult {
  bool ok;
  int httpCode;    // HTTP status, or the HTTPClient error (< 0), or 0 before connecting
  size_t len;      // body bytes in the buffer (NUL-terminated after them when ok)
  char error[32];  // "" when ok, else e.g. "HTTP 503", "too large", "timeout", "TLS error"
};

// buf must hold cap bytes; a body of cap bytes or more is "too large" (one byte is kept for NUL).
GetResult httpsGet(const char* url, char* buf, size_t cap);

// The same GET, but the body is streamed into sink as it arrives (any size) and redirects are
// followed. len counts the bytes delivered. A sink that returns false stops the download
// ("bad data").
GetResult httpsGetTo(const char* url, ByteSink& sink);
}  // namespace net
#endif

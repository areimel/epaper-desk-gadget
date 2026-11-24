#include "Text.h"

#include <stdio.h>

namespace text {

void age(time_t now, time_t then, char* out, size_t cap) {
  if (then == 0) {
    snprintf(out, cap, "never");
    return;
  }
  long long d = static_cast<long long>(now) - static_cast<long long>(then);
  if (d < 60) {  // includes any future timestamp (clock skew)
    snprintf(out, cap, "just now");
  } else if (d < 3600) {
    snprintf(out, cap, "%lld min ago", d / 60);
  } else if (d < 86400) {
    snprintf(out, cap, "%lld h ago", d / 3600);
  } else if (d < 2 * 86400) {
    snprintf(out, cap, "yesterday");
  } else {
    snprintf(out, cap, "%lld days ago", d / 86400);
  }
}

}  // namespace text

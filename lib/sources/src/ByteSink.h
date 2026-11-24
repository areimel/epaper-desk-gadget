#pragma once
#include <stddef.h>
#include <stdint.h>

// Receives a download as it arrives, so a large body (the calendar) is never held in memory.
// Pure: the device adapts an HTTP stream to it, tests feed it fixture bytes in odd-sized chunks.
struct ByteSink {
  virtual ~ByteSink() = default;
  virtual bool write(const uint8_t* data, size_t n) = 0;  // false = stop the download
};

#pragma once
#if FEATURE_EPAPER
#include <stdint.h>

#include "Canvas.h"

// The e-paper panel. It only ever receives a finished Canvas; nothing draws into it directly.
// show() blocks for the length of the refresh (about 2-4 s full, under 1 s fast).
enum class Refresh : uint8_t { Full, Fast };

namespace panel {
void begin();  // powers the panel rail and starts the controller; the first show() is always full
// Fast is promoted to Full after FAST_LIMIT fast refreshes in a row, to clear ghosting.
void show(const Canvas& canvas, Refresh mode);
uint32_t refreshCount();  // refreshes since boot, full and fast
constexpr uint8_t FAST_LIMIT = 5;
}  // namespace panel
#endif  // FEATURE_EPAPER

#pragma once
#include <stddef.h>
#include <stdint.h>

// Every size and range limit of the feed, in one place. Firmware validation, storage sizes and
// the web app (through GET /api/state "limits") all come from here; do not copy these numbers.
// Text limits are in bytes of UTF-8, not characters.
namespace limits {
constexpr uint16_t DWELL_MIN_S = 180;  // faster page turns wear the panel
constexpr uint16_t DWELL_MAX_S = 3600;
constexpr uint16_t DWELL_DEFAULT_S = 300;
constexpr uint8_t PAGE_COUNT = 6;
constexpr uint8_t MESSAGES_MAX = 40;
constexpr size_t MESSAGE_TEXT_MAX = 140;
constexpr size_t MESSAGE_BY_MAX = 32;
constexpr uint8_t NOTE_LINES_MAX = 5;
constexpr size_t NOTE_TEXT_MAX = 40;
constexpr size_t NOTE_TITLE_MAX = 24;
constexpr size_t URL_MAX = 256;
constexpr size_t PLACE_MAX = 32;
constexpr size_t TZ_MAX = 48;
constexpr size_t FEED_JSON_MAX = 16384;  // largest accepted PUT /api/feed body and feed.json
constexpr uint8_t HN_STORIES = 9;
constexpr uint8_t HN_PER_VISIT = 3;
constexpr size_t HN_TITLE_MAX = 96;
constexpr uint8_t CAL_EVENTS_MAX = 8;
constexpr size_t CAL_TITLE_MAX = 48;
constexpr uint8_t FORECAST_DAYS = 3;
constexpr uint8_t FEED_SCHEMA = 1;
}  // namespace limits

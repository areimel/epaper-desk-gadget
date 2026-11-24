#pragma once
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

#include "Canvas.h"
#include "Limits.h"
#include "Snapshots.h"

// The GET /api/state document (docs/api.md), from a plain struct. Pure: no Arduino, so it is
// host-tested (test/test_net_state_json includes it by path) against the shape of
// device-ui/mock/fixtures/state.json. Every limit comes from Limits.h, never a literal.
namespace net {
struct StateInfo {
  const char* name;
  const char* version;
  const char* board;
  const char* ip;    // "" when offline
  const char* host;  // e.g. "desk-gadget.local"
  long long time;    // Unix seconds, UTC
  bool timeSynced;
  const char* page;  // id on the panel, or ""
  SourceStatus weather, hn, calendar;
  uint16_t calendarSkipped;
  uint32_t heapFree;
  uint32_t refreshCount;
};

inline const char* stateName(SourceState s) {
  switch (s) {
    case SourceState::Ok: return "ok";
    case SourceState::Stale: return "stale";
    case SourceState::Error: return "error";
    default: return "never";
  }
}

inline void putSource(JsonObject o, const SourceStatus& s) {
  o["state"] = stateName(s.state);
  o["updated"] = static_cast<long long>(s.updated);
  o["error"] = s.error;
}

// Returns bytes written (excluding the NUL), or 0 if out is too small or memory ran out.
inline size_t stateJson(const StateInfo& s, char* out, size_t cap) {
  JsonDocument doc;
  doc["name"] = s.name;
  doc["version"] = s.version;
  doc["board"] = s.board;
  doc["ip"] = s.ip;
  doc["host"] = s.host;
  doc["time"] = s.time;
  doc["timeSynced"] = s.timeSynced;
  doc["page"] = s.page;
  JsonObject screen = doc["screen"].to<JsonObject>();
  screen["w"] = Canvas::W;
  screen["h"] = Canvas::H;
  screen["rowBytes"] = Canvas::ROW_BYTES;
  JsonObject l = doc["limits"].to<JsonObject>();
  l["dwellMin"] = limits::DWELL_MIN_S;
  l["dwellMax"] = limits::DWELL_MAX_S;
  l["messagesMax"] = limits::MESSAGES_MAX;
  l["messageTextMax"] = limits::MESSAGE_TEXT_MAX;
  l["messageByMax"] = limits::MESSAGE_BY_MAX;
  l["noteLinesMax"] = limits::NOTE_LINES_MAX;
  l["noteTextMax"] = limits::NOTE_TEXT_MAX;
  l["noteTitleMax"] = limits::NOTE_TITLE_MAX;
  l["urlMax"] = limits::URL_MAX;
  l["placeMax"] = limits::PLACE_MAX;
  l["tzMax"] = limits::TZ_MAX;
  // Proposed by the web team, NOT in docs/api.md yet (the integrator decides): add here
  //   l["hnPerVisit"] = limits::HN_PER_VISIT;  and top-level "setup" / "refreshing" bools
  //   (two more StateInfo fields filled in ApiState.cpp from new hooks).
  JsonObject src = doc["sources"].to<JsonObject>();
  putSource(src["weather"].to<JsonObject>(), s.weather);
  putSource(src["hn"].to<JsonObject>(), s.hn);
  putSource(src["calendar"].to<JsonObject>(), s.calendar);
  doc["calendarSkipped"] = s.calendarSkipped;
  doc["heapFree"] = s.heapFree;
  doc["refreshCount"] = s.refreshCount;
  if (doc.overflowed() || measureJson(doc) >= cap) return 0;
  return serializeJson(doc, out, cap);
}
}  // namespace net

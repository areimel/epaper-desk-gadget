#include <math.h>
#include <string.h>

#include "FeedJson.h"
#include "FeedJsonCheck.h"

namespace feed {
namespace {

bool readTz(Check& c, JsonVariantConst v, FeedConfig& f) {
  JsonObjectConst o;
  if (!c.object(v, "tz", o)) return false;
  return c.text(o["iana"], "tz.iana", "Time zone", 1, limits::TZ_MAX, f.tzIana) &&
         c.text(o["posix"], "tz.posix", "Time zone", 1, limits::TZ_MAX, f.tzPosix);
}

bool readPages(Check& c, JsonVariantConst v, FeedConfig& f) {
  JsonArrayConst a;
  if (!c.array(v, "pages", a)) return false;
  if (a.size() != limits::PAGE_COUNT) return c.fail("pages", "There must be exactly %u pages.", limits::PAGE_COUNT);
  uint8_t seen = 0;  // bit per PageId; PAGE_COUNT is 6, so one byte holds them all
  static_assert(limits::PAGE_COUNT <= 8, "seen mask is one byte");
  unsigned i = 0;
  for (JsonVariantConst e : a) {
    Path p("pages", i);
    JsonObjectConst o;
    if (!c.object(e, p.s, o)) return false;
    Path idPath("pages", i, ".id");
    JsonVariantConst id = o["id"];
    if (!c.present(id, idPath.s)) return false;
    PageId page;
    if (!id.is<const char*>()) return c.fail(idPath.s, "This must be text.");
    if (!pageFromName(id.as<const char*>(), page)) return c.fail(idPath.s, "This page is not known.");
    uint8_t bit = 1u << static_cast<uint8_t>(page);
    if (seen & bit) return c.fail(idPath.s, "This page is listed twice.");
    seen |= bit;
    Path onPath("pages", i, ".on");
    if (!c.boolean(o["on"], onPath.s, f.pages[i].on)) return false;
    f.pages[i].id = page;
    i++;
  }
  return true;
}

bool readWeather(Check& c, JsonVariantConst v, FeedConfig& f) {
  if (v.isNull()) return true;  // absent or null: not set; the scratch copy is already zeroed
  JsonObjectConst o;
  if (!c.object(v, "weather", o)) return false;
  double lat, lon;
  if (!c.text(o["place"], "weather.place", "Place", 1, limits::PLACE_MAX, f.weather.place) ||
      !c.number(o["lat"], "weather.lat", -90, 90, lat, "Latitude must be between -90 and 90.") ||
      !c.number(o["lon"], "weather.lon", -180, 180, lon, "Longitude must be between -180 and 180.")) {
    return false;
  }
  JsonVariantConst u = o["units"];
  if (!c.present(u, "weather.units")) return false;
  if (!u.is<const char*>()) return c.fail("weather.units", "This must be text.");
  const char* units = u.as<const char*>();
  if (strcmp(units, "c") == 0) {
    f.weather.units = Units::Celsius;
  } else if (strcmp(units, "f") == 0) {
    f.weather.units = Units::Fahrenheit;
  } else {
    return c.fail("weather.units", "Units must be c or f.");
  }
  // Round to 4 decimals (about 11 m, plenty for a forecast). ArduinoJson parses short numbers
  // as float and can land one ulp off (-75.16 becomes -75.159996), which would be written back
  // that way; rounding snaps it to the float nearest the typed value.
  f.weather.lat = static_cast<float>(llround(lat * 1e4) / 1e4);
  f.weather.lon = static_cast<float>(llround(lon * 1e4) / 1e4);
  f.weather.set = true;
  return true;
}

// Absent or without `url` keeps the stored link; "" clears it.
bool readCalendar(Check& c, JsonVariantConst v, const FeedConfig& current, FeedConfig& f) {
  strcpy(f.calendarUrl, current.calendarUrl);
  if (v.isNull()) return true;
  JsonObjectConst o;
  if (!c.object(v, "calendar", o)) return false;
  JsonVariantConst u = o["url"];
  if (u.isNull()) return true;
  if (!c.text(u, "calendar.url", "The link", 0, limits::URL_MAX, f.calendarUrl)) return false;
  const char* s = f.calendarUrl;
  if (*s && strncmp(s, "https://", 8) != 0 && strncmp(s, "webcal://", 9) != 0) {
    return c.fail("calendar.url", "The link must start with https:// or webcal://.");
  }
  return true;
}

bool readMessages(Check& c, JsonVariantConst v, FeedConfig& f) {
  JsonArrayConst a;
  if (!c.array(v, "messages", a)) return false;
  if (a.size() > limits::MESSAGES_MAX) return c.fail("messages", "There can be at most %u messages.", limits::MESSAGES_MAX);
  unsigned i = 0;
  for (JsonVariantConst e : a) {
    Path p("messages", i);
    JsonObjectConst o;
    if (!c.object(e, p.s, o)) return false;
    Path textPath("messages", i, ".text"), byPath("messages", i, ".by");
    if (!c.text(o["text"], textPath.s, "Text", 1, limits::MESSAGE_TEXT_MAX, f.messages[i].text)) return false;
    // `by` is optional: absent or null reads as "".
    if (!o["by"].isNull() && !c.text(o["by"], byPath.s, "Name", 0, limits::MESSAGE_BY_MAX, f.messages[i].by)) return false;
    i++;
  }
  f.messageCount = static_cast<uint8_t>(i);
  return true;
}

bool readNotes(Check& c, JsonVariantConst v, FeedConfig& f) {
  JsonObjectConst o;
  if (!c.object(v, "notes", o)) return false;
  if (!c.text(o["title"], "notes.title", "Title", 0, limits::NOTE_TITLE_MAX, f.notes.title)) return false;
  JsonArrayConst a;
  if (!c.array(o["lines"], "notes.lines", a)) return false;
  if (a.size() > limits::NOTE_LINES_MAX) return c.fail("notes.lines", "There can be at most %u lines.", limits::NOTE_LINES_MAX);
  unsigned i = 0;
  for (JsonVariantConst e : a) {
    Path p("notes.lines", i);
    JsonObjectConst l;
    if (!c.object(e, p.s, l)) return false;
    Path textPath("notes.lines", i, ".text"), todoPath("notes.lines", i, ".todo"), donePath("notes.lines", i, ".done");
    NoteLine& line = f.notes.lines[i];
    if (!c.text(l["text"], textPath.s, "Text", 1, limits::NOTE_TEXT_MAX, line.text) ||
        !c.boolean(l["todo"], todoPath.s, line.todo) || !c.boolean(l["done"], donePath.s, line.done)) {
      return false;
    }
    if (!line.todo) line.done = false;
    i++;
  }
  f.notes.lineCount = static_cast<uint8_t>(i);
  return true;
}

bool isKnownKey(const char* k) {
  static const char* const KEYS[] = {"v", "dwellSec", "tz", "pages", "weather", "calendar", "messages", "notes"};
  for (const char* known : KEYS) {
    if (strcmp(k, known) == 0) return true;
  }
  return false;
}

bool readRoot(Check& c, JsonVariantConst root, const FeedConfig& current, FeedConfig& f) {
  JsonObjectConst o = root.as<JsonObjectConst>();
  long long n;
  if (!root.is<JsonObjectConst>()) return c.fail("", "The feed must be a JSON object.");
  if (!c.integer(o["v"], "v", limits::FEED_SCHEMA, limits::FEED_SCHEMA, n, "This feed version is not supported.")) return false;
  for (JsonPairConst kv : o) {
    if (!isKnownKey(kv.key().c_str())) return c.fail(kv.key().c_str(), "This setting is not known.");
  }
  char range[64];
  snprintf(range, sizeof range, "Choose a rotation time between %u and %u minutes.", limits::DWELL_MIN_S / 60,
           limits::DWELL_MAX_S / 60);
  if (!c.integer(o["dwellSec"], "dwellSec", limits::DWELL_MIN_S, limits::DWELL_MAX_S, n, range)) return false;
  f.dwellSec = static_cast<uint16_t>(n);
  return readTz(c, o["tz"], f) && readPages(c, o["pages"], f) && readWeather(c, o["weather"], f) &&
         readCalendar(c, o["calendar"], current, f) && readMessages(c, o["messages"], f) && readNotes(c, o["notes"], f);
}

}  // namespace

Result fromJson(const char* json, size_t n, const FeedConfig& current, FeedConfig& out, ArduinoJson::Allocator* alloc) {
  Result r = {true, "", ""};
  Check c{r};
  if (n > limits::FEED_JSON_MAX) {
    c.fail("", "Feed is too large.");
    return r;
  }
  // Heap: ArduinoJson copies every string out of the input, so a worst-case parse holds about
  // the input size in strings plus ~16 bytes per value; the document is freed on return. No
  // filter is used because unknown keys must be reported, which a filter would hide.
  JsonDocument doc = alloc ? JsonDocument(alloc) : JsonDocument();
  DeserializationError err = deserializeJson(doc, json, n);
  if (err) {
    c.fail("", err == DeserializationError::NoMemory ? "There is not enough memory to read the feed."
                                                     : "The feed is not valid JSON.");
    return r;
  }
  // FeedConfig is about 8 KB: far too much for the stack of a server task, so the scratch copy is
  // a function-local static. This makes fromJson non-reentrant; it is only called from one thread.
  static FeedConfig scratch;
  memset(&scratch, 0, sizeof scratch);
  if (readRoot(c, doc.as<JsonVariantConst>(), current, scratch)) memcpy(&out, &scratch, sizeof out);
  return r;
}

}  // namespace feed

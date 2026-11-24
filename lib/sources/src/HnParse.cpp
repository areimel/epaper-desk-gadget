#include "HnParse.h"

#include <ArduinoJson.h>
#include <string.h>

#include "Text.h"

namespace {

constexpr const char* URL =
    "https://hn.algolia.com/api/v1/search?tags=front_page&hitsPerPage=9"
    "&attributesToRetrieve=title,points,num_comments&attributesToHighlight=";

// null, missing, non-numeric -> 0; negatives -> 0; above 65535 -> 65535.
uint16_t count16(JsonVariantConst v) {
  if (!v.is<double>()) return 0;
  double d = v.as<double>();
  return d <= 0 ? 0 : d >= 65535 ? 65535 : static_cast<uint16_t>(d);
}

}  // namespace

size_t hnUrl(char* out, size_t cap) {
  size_t n = strlen(URL);
  if (!out || cap <= n) {
    if (out && cap) out[0] = 0;
    return 0;
  }
  memcpy(out, URL, n + 1);
  return n;
}

bool parseHn(const char* json, size_t n, HnSnap& snap) {
  if (!json) return false;
  JsonDocument filter;
  JsonObject hit = filter["hits"][0].to<JsonObject>();
  hit["title"] = true;
  hit["points"] = true;
  hit["num_comments"] = true;

  JsonDocument doc;
  if (deserializeJson(doc, json, n, DeserializationOption::Filter(filter))) return false;
  JsonArrayConst hits = doc["hits"];
  if (hits.isNull()) return false;

  HnSnap out = snap;  // status stays as it is
  out.count = 0;
  for (JsonObjectConst h : hits) {
    if (out.count >= limits::HN_STORIES) break;
    const char* title = h["title"].as<const char*>();
    if (!title) continue;
    HnStory& s = out.stories[out.count];
    if (text::fold(title, s.title, sizeof s.title) == 0) continue;
    s.points = count16(h["points"]);
    s.comments = count16(h["num_comments"]);
    out.count++;
  }
  snap = out;
  return true;
}

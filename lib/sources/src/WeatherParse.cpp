#include "WeatherParse.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>

namespace {

// The request and the parser agree on these fields; keep them together.
constexpr const char* URL_FMT =
    "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,apparent_temperature,weather_code,is_day"
    "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
    "&forecast_days=4&temperature_unit=%s&timezone=%s";
constexpr size_t DAILY_NEEDED = 1 + limits::FORECAST_DAYS;

bool unreserved(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
         c == '.';
}

void percentEncode(const char* in, char* out) {
  static const char HEX[] = "0123456789ABCDEF";
  for (; *in; in++) {
    unsigned char c = static_cast<unsigned char>(*in);
    if (unreserved(*in)) {
      *out++ = *in;
    } else {
      *out++ = '%';
      *out++ = HEX[c >> 4];
      *out++ = HEX[c & 15];
    }
  }
  *out = 0;
}

// Nearest whole degree, halves away from zero, clamped to int16.
bool whole(JsonVariantConst v, int16_t& out) {
  if (!v.is<double>()) return false;
  double r = round(v.as<double>());
  if (r > 32767) r = 32767;
  if (r < -32768) r = -32768;
  out = static_cast<int16_t>(r);
  return true;
}

uint8_t wmoCode(JsonVariantConst v) {
  if (!v.is<double>()) return 255;
  double c = v.as<double>();
  return (c >= 0 && c <= 255) ? static_cast<uint8_t>(c) : 255;
}

// Day of week (0 = Sunday) of "YYYY-MM-DD" by pure calendar arithmetic (Sakamoto's method).
bool weekday(const char* s, uint8_t& out) {
  if (!s) return false;
  int v[3] = {0, 0, 0};
  static const uint8_t LEN[3] = {4, 2, 2};
  for (int part = 0; part < 3; part++) {
    for (int i = 0; i < LEN[part]; i++, s++) {
      if (*s < '0' || *s > '9') return false;
      v[part] = v[part] * 10 + (*s - '0');
    }
    if (part < 2 && *s++ != '-') return false;
  }
  if (*s || v[1] < 1 || v[1] > 12 || v[2] < 1 || v[2] > 31) return false;
  int y = v[0], m = v[1], d = v[2];
  if (m < 3) y--;
  static const int T[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  out = static_cast<uint8_t>((y + y / 4 - y / 100 + y / 400 + T[m - 1] + d) % 7);
  return true;
}

void buildFilter(JsonDocument& filter) {
  JsonObject cur = filter["current"].to<JsonObject>();
  cur["temperature_2m"] = true;
  cur["apparent_temperature"] = true;
  cur["weather_code"] = true;
  cur["is_day"] = true;
  JsonObject daily = filter["daily"].to<JsonObject>();
  daily["time"] = true;
  daily["weather_code"] = true;
  daily["temperature_2m_max"] = true;
  daily["temperature_2m_min"] = true;
  daily["precipitation_probability_max"] = true;
  filter["error"] = true;
}

}  // namespace

size_t weatherUrl(const FeedConfig& feed, char* out, size_t cap) {
  if (!out || cap == 0) return 0;
  out[0] = 0;
  if (!feed.weather.set) return 0;
  char zone[limits::TZ_MAX * 3 + 1];
  percentEncode(feed.tzIana, zone);
  const char* unit = feed.weather.units == Units::Fahrenheit ? "fahrenheit" : "celsius";
  int n = snprintf(out, cap, URL_FMT, static_cast<double>(feed.weather.lat), static_cast<double>(feed.weather.lon),
                   unit, zone);
  if (n <= 0 || static_cast<size_t>(n) >= cap) {
    out[0] = 0;
    return 0;
  }
  return static_cast<size_t>(n);
}

bool parseWeather(const char* json, size_t n, WeatherSnap& snap) {
  if (!json) return false;
  JsonDocument filter;
  buildFilter(filter);
  JsonDocument doc;
  if (deserializeJson(doc, json, n, DeserializationOption::Filter(filter))) return false;
  if (!doc.is<JsonObjectConst>() || doc["error"].as<bool>()) return false;

  JsonVariantConst c = doc["current"];
  JsonArrayConst time = doc["daily"]["time"];
  JsonArrayConst codes = doc["daily"]["weather_code"];
  JsonArrayConst hi = doc["daily"]["temperature_2m_max"];
  JsonArrayConst lo = doc["daily"]["temperature_2m_min"];
  JsonArrayConst rain = doc["daily"]["precipitation_probability_max"];
  if (!c.is<JsonObjectConst>() || time.size() < DAILY_NEEDED || hi.size() < DAILY_NEEDED ||
      lo.size() < DAILY_NEEDED || rain.size() < DAILY_NEEDED || codes.size() < DAILY_NEEDED)
    return false;

  WeatherSnap out = snap;  // status stays as it is
  if (!whole(c["temperature_2m"], out.tempNow) || !whole(c["apparent_temperature"], out.feels) ||
      !whole(hi[0], out.hi) || !whole(lo[0], out.lo))
    return false;
  out.code = wmoCode(c["weather_code"]);
  out.isDay = c["is_day"].as<double>() != 0;
  double pct = rain[0].is<double>() ? rain[0].as<double>() : 0;
  out.rainPct = static_cast<uint8_t>(pct < 0 ? 0 : pct > 100 ? 100 : pct + 0.5);

  for (uint8_t i = 0; i < limits::FORECAST_DAYS; i++) {
    WeatherDay& d = out.days[i];
    if (!weekday(time[i + 1].as<const char*>(), d.wday) || !whole(hi[i + 1], d.hi) || !whole(lo[i + 1], d.lo))
      return false;
    d.code = wmoCode(codes[i + 1]);
  }
  snap = out;
  return true;
}

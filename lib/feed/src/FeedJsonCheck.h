#pragma once
// Private to FeedJsonRead.cpp: typed field checks that fill a Result on the first failure.
// Every method returns true when the field is valid, false (with r set) otherwise.
#include <ArduinoJson.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "FeedJson.h"

namespace feed {

// Writes "<prefix><suffix>" into a small buffer, e.g. join(p, "messages[3]", ".text").
struct Path {
  char s[48];
  Path(const char* prefix, const char* suffix = "") { snprintf(s, sizeof s, "%s%s", prefix, suffix); }
  Path(const char* prefix, unsigned index, const char* suffix = "") {
    snprintf(s, sizeof s, "%s[%u]%s", prefix, index, suffix);
  }
};

struct Check {
  Result& r;

  bool fail(const char* path, const char* fmt, ...) __attribute__((format(printf, 3, 4))) {
    r.ok = false;
    snprintf(r.path, sizeof r.path, "%s", path);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r.error, sizeof r.error, fmt, ap);
    va_end(ap);
    return false;
  }

  // Missing and null both read as "not there".
  bool present(JsonVariantConst v, const char* path) {
    return !v.isNull() || fail(path, "This is required.");
  }

  bool object(JsonVariantConst v, const char* path, JsonObjectConst& o) {
    if (!present(v, path)) return false;
    if (!v.is<JsonObjectConst>()) return fail(path, "This must be a group of settings.");
    o = v.as<JsonObjectConst>();
    return true;
  }

  bool array(JsonVariantConst v, const char* path, JsonArrayConst& a) {
    if (!present(v, path)) return false;
    if (!v.is<JsonArrayConst>()) return fail(path, "This must be a list.");
    a = v.as<JsonArrayConst>();
    return true;
  }

  bool boolean(JsonVariantConst v, const char* path, bool& out) {
    if (!present(v, path)) return false;
    if (!v.is<bool>()) return fail(path, "This must be true or false.");
    out = v.as<bool>();
    return true;
  }

  // A whole number in [lo, hi]; `rangeMsg` is the sentence for a number outside it.
  bool integer(JsonVariantConst v, const char* path, long long lo, long long hi, long long& out,
               const char* rangeMsg) {
    if (!present(v, path)) return false;
    if (!v.is<long long>()) return fail(path, "This must be a whole number.");
    out = v.as<long long>();
    return (out >= lo && out <= hi) || fail(path, "%s", rangeMsg);
  }

  bool number(JsonVariantConst v, const char* path, double lo, double hi, double& out, const char* rangeMsg) {
    if (!present(v, path)) return false;
    if (!v.is<double>()) return fail(path, "This must be a number.");
    out = v.as<double>();
    return (out >= lo && out <= hi) || fail(path, "%s", rangeMsg);
  }

  // Text of lo..hi BYTES with no control characters (that includes an embedded NUL). Copies it
  // NUL-terminated into dst, which holds at least hi + 1 bytes. `label` starts the sentences.
  bool text(JsonVariantConst v, const char* path, const char* label, size_t lo, size_t hi, char* dst) {
    if (!present(v, path)) return false;
    if (!v.is<const char*>()) return fail(path, "This must be text.");
    JsonString s = v.as<JsonString>();
    const char* p = s.c_str();
    size_t n = s.size();
    for (size_t i = 0; i < n; i++) {
      if (static_cast<unsigned char>(p[i]) < 0x20) return fail(path, "%s cannot contain control characters.", label);
    }
    if (n < lo) return fail(path, "%s cannot be empty.", label);
    if (n > hi) return fail(path, "%s is too long.", label);
    memcpy(dst, p, n);
    dst[n] = 0;
    return true;
  }
};

}  // namespace feed

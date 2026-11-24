#pragma once
// Test helper: pushes iCalendar bytes through IcsParser in chosen chunk sizes and compares the
// results of different chunkings. Header-only; include from a test_ics_* suite.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unity.h>

#include <new>

#include "IcsParse.h"

namespace icsfeed {

constexpr size_t FIXTURE_MAX = 64 * 1024;

inline void setZone(const char* tz) {
  setenv("TZ", tz, 1);
  tzset();
}

// An instant given as wall-clock time in the process zone.
inline time_t at(int y, int mo, int d, int h = 12, int mi = 0) {
  struct tm t;
  memset(&t, 0, sizeof t);
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_isdst = -1;
  return mktime(&t);
}

// Loads test/fixtures/ics/<name> (tests run from the project root) into a static buffer.
inline const uint8_t* load(const char* name, size_t& n) {
  static uint8_t buf[FIXTURE_MAX];
  char path[256];
  snprintf(path, sizeof path, "test/fixtures/ics/%s", name);
  FILE* f = fopen(path, "rb");
  TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
  n = fread(buf, 1, sizeof buf, f);
  fclose(f);
  TEST_ASSERT_TRUE_MESSAGE(n < sizeof buf, "fixture too large");
  return buf;
}

enum class Mode { Whole, Bytes, Random };

struct Result {
  CalSnap snap;
  bool ok;
};

// One parse. Random chunk sizes come from a fixed-seed LCG, from 1 to 700 bytes.
inline Result parse(const uint8_t* data, size_t n, time_t now, const char* tz, Mode mode,
                    uint32_t seed = 1) {
  alignas(IcsParser) static unsigned char storage[sizeof(IcsParser)];  // kept off the stack
  IcsParser* parser = new (storage) IcsParser(now, tz);
  size_t i = 0;
  while (i < n) {
    size_t chunk = n - i;
    if (mode == Mode::Bytes) chunk = 1;
    if (mode == Mode::Random) {
      seed = seed * 1103515245u + 12345u;
      chunk = (seed >> 8) % 700 + 1;
      if (chunk > n - i) chunk = n - i;
    }
    TEST_ASSERT_TRUE(parser->write(data + i, chunk));
    i += chunk;
  }
  Result r;
  memset(&r, 0xA5, sizeof r);  // finish() must overwrite every field it owns
  r.snap.status.state = SourceState::Never;
  r.ok = parser->finish(r.snap);
  parser->~IcsParser();
  return r;
}

inline void assertSame(const Result& a, const Result& b, const char* what) {
  TEST_ASSERT_EQUAL_MESSAGE(a.ok, b.ok, what);
  TEST_ASSERT_EQUAL_MESSAGE(a.snap.count, b.snap.count, what);
  TEST_ASSERT_EQUAL_MESSAGE(a.snap.skipped, b.snap.skipped, what);
  for (uint8_t i = 0; i < a.snap.count; i++) {
    TEST_ASSERT_EQUAL_STRING_MESSAGE(a.snap.events[i].title, b.snap.events[i].title, what);
    TEST_ASSERT_EQUAL_MESSAGE(a.snap.events[i].startMin, b.snap.events[i].startMin, what);
    TEST_ASSERT_EQUAL_MESSAGE(a.snap.events[i].endMin, b.snap.events[i].endMin, what);
  }
}

// Whole buffer, one byte at a time and three pseudo-random chunkings must agree.
inline Result parseAllWays(const uint8_t* data, size_t n, time_t now, const char* tz) {
  Result whole = parse(data, n, now, tz, Mode::Whole);
  assertSame(whole, parse(data, n, now, tz, Mode::Bytes), "1-byte chunks differ");
  for (uint32_t seed = 1; seed <= 3; seed++) {
    assertSame(whole, parse(data, n, now, tz, Mode::Random, seed), "random chunks differ");
  }
  TEST_ASSERT_EQUAL(SourceState::Never, whole.snap.status.state);  // status is never touched
  return whole;
}

// Rewrites line endings: every CR is dropped, then (crlf) each LF becomes CRLF.
inline size_t withEndings(const uint8_t* in, size_t n, uint8_t* out, size_t cap, bool crlf) {
  size_t k = 0;
  for (size_t i = 0; i < n; i++) {
    if (in[i] == '\r') continue;
    if (in[i] == '\n' && crlf && k < cap) out[k++] = '\r';
    if (k < cap) out[k++] = in[i];
  }
  return k;
}

// A fixture parsed all ways, both with LF and with CRLF line endings (git may normalise the
// committed files, so both forms are produced here); all results must agree.
inline Result fixture(const char* name, time_t now, const char* tz) {
  static uint8_t converted[2 * FIXTURE_MAX];
  size_t n;
  const uint8_t* data = load(name, n);
  size_t lf = withEndings(data, n, converted, sizeof converted, false);
  Result r = parseAllWays(converted, lf, now, tz);
  size_t crlf = withEndings(data, n, converted, sizeof converted, true);
  assertSame(r, parseAllWays(converted, crlf, now, tz), "CRLF and LF differ");
  return r;
}

inline void expectEvent(const CalSnap& s, uint8_t i, const char* title, int startMin, int endMin) {
  char msg[96];
  snprintf(msg, sizeof msg, "event %u (%s)", i, title);
  TEST_ASSERT_TRUE_MESSAGE(i < s.count, msg);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(title, s.events[i].title, msg);
  TEST_ASSERT_EQUAL_MESSAGE(startMin, s.events[i].startMin, msg);
  TEST_ASSERT_EQUAL_MESSAGE(endMin, s.events[i].endMin, msg);
}

inline bool hasTitle(const CalSnap& s, const char* title) {
  for (uint8_t i = 0; i < s.count; i++) {
    if (strcmp(s.events[i].title, title) == 0) return true;
  }
  return false;
}

}  // namespace icsfeed

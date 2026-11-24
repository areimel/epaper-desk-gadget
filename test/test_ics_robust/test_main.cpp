// Garbage, truncation, bounded-table overflow policies, a generated ~2 MB feed and a fixed-seed
// fuzz loop. Monday 2026-11-23 in America/New_York (UTC-5) unless stated.
#include <stdarg.h>
#include <time.h>
#include <unity.h>

#include "../support/IcsFeed.h"

using namespace icsfeed;

static const char* NY = "America/New_York";

void setUp() { setZone(NY); }
void tearDown() {}

// A growing input buffer for generated feeds.
struct Buf {
  static constexpr size_t CAP = 3 * 1024 * 1024;
  uint8_t* data;
  size_t n = 0;
  Buf() {
    static uint8_t storage[CAP];
    data = storage;
  }
  void add(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(reinterpret_cast<char*>(data + n), CAP - n, fmt, ap);
    va_end(ap);
    if (k > 0) n += static_cast<size_t>(k) < CAP - n ? static_cast<size_t>(k) : CAP - n - 1;
  }
};

static Result run(const Buf& b) { return parse(b.data, b.n, at(2026, 11, 23), NY, Mode::Random, 7); }
static Result run(const char* s) {
  return parse(reinterpret_cast<const uint8_t*>(s), strlen(s), at(2026, 11, 23), NY, Mode::Whole);
}

static const char* HEAD = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Google Inc//Google Calendar 70.9054//EN\r\n";

void test_state_is_small() {
  TEST_ASSERT_LESS_OR_EQUAL(6144, sizeof(IcsParser));
  TEST_ASSERT_GREATER_THAN(ics::LINE_CAP, sizeof(IcsParser));
}

void test_not_a_calendar() {
  TEST_ASSERT_FALSE(run("").ok);
  Result r = run("<!DOCTYPE html><html><head><title>404</title></head>\n<body>BEGIN:VEVENT\n"
                 "DTSTART:20261123T150000Z\nSUMMARY:x\nEND:VEVENT</body></html>\n");
  TEST_ASSERT_FALSE(r.ok);
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
  static uint8_t junk[64 * 1024];
  uint32_t seed = 99;
  for (size_t i = 0; i < sizeof junk; i++) junk[i] = static_cast<uint8_t>((seed = seed * 1103515245u + 12345u) >> 16);
  TEST_ASSERT_FALSE(parse(junk, sizeof junk, at(2026, 11, 23), NY, Mode::Random, 3).ok);
}

void test_truncated_file_keeps_complete_events() {
  size_t n;
  const uint8_t* data = load("singles.ics", n);
  const char* cut = strstr(reinterpret_cast<const char*>(data), "SUMMARY:Overnight shift");
  TEST_ASSERT_NOT_NULL(cut);
  Result r = parseAllWays(data, static_cast<size_t>(cut - reinterpret_cast<const char*>(data)),
                          at(2026, 11, 23), NY);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(4, r.snap.count);  // Late show, Standup, Review, Dentist; not Overnight
  TEST_ASSERT_FALSE(hasTitle(r.snap, "Overnight shift"));
}

void test_megabyte_line() {
  Buf b;
  b.add("%sX-JUNK:", HEAD);
  for (int i = 0; i < 1024 * 1024 / 64; i++) b.add("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789++");
  b.add("\r\nBEGIN:VEVENT\r\nDTSTART:20261123T150000Z\r\nSUMMARY:After the junk\r\nEND:VEVENT\r\n");
  Result r = run(b);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(1, r.snap.count);
  expectEvent(r.snap, 0, "After the junk", 600, 600);
  // The same megabyte with no line break at all, and no calendar: rejected.
  TEST_ASSERT_FALSE(parse(b.data + strlen(HEAD), 1024 * 1024, at(2026, 11, 23), NY, Mode::Whole).ok);
}

void test_broken_nesting() {
  Result r = run(
      "BEGIN:VCALENDAR\n"
      "BEGIN:VEVENT\nDTSTART:20261123T150000Z\nSUMMARY:Never closed\n"
      "BEGIN:VEVENT\nDTSTART:20261123T160000Z\nSUMMARY:Second\n"
      "BEGIN:VALARM\nTRIGGER:-PT10M\nSUMMARY:alarm\n"  // VALARM never closed
      "END:VEVENT\n"
      "BEGIN:VEVENT\nDTSTART:20261123T170000Z\nSUMMARY:Cut off\n");
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(1, r.snap.count);
  expectEvent(r.snap, 0, "Second", 660, 660);
}

static void master(Buf& b, int i, const char* extra) {
  b.add("BEGIN:VEVENT\r\nDTSTART;TZID=America/New_York:20261101T%02d%02d00\r\n"
        "DTEND;TZID=America/New_York:20261101T%02d%02d00\r\nRRULE:FREQ=DAILY\r\n"
        "UID:series%03d@google.com\r\nSUMMARY:Series %03d\r\n%sEND:VEVENT\r\n",
        5 + i / 6, (i % 6) * 10, 5 + i / 6, (i % 6) * 10 + 5, i, i, extra);
}

static void movedToTomorrow(Buf& b, int i) {
  b.add("BEGIN:VEVENT\r\nRECURRENCE-ID;TZID=America/New_York:20261123T%02d%02d00\r\n"
        "DTSTART;TZID=America/New_York:20261124T090000\r\nUID:series%03d@google.com\r\n"
        "SUMMARY:Moved\r\nEND:VEVENT\r\n", 5 + i / 6, (i % 6) * 10, i);
}

void test_candidate_eviction_is_counted_not_misplaced() {
  // 30 series occur today; 28 of them are moved to tomorrow. The table keeps the 24 earliest
  // candidates, all of which are then suppressed: nothing is shown, and the 6 evicted ones
  // (2 of which would really show) are reported as skipped.
  Buf b;
  b.add("%s", HEAD);
  for (int i = 0; i < 30; i++) master(b, i, "");
  for (int i = 0; i < 28; i++) movedToTomorrow(b, i);
  b.add("END:VCALENDAR\r\n");
  Result r = run(b);
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(6, r.snap.skipped);
}

void test_suppression_overflow_drops_series() {
  Buf b;
  b.add("%s", HEAD);
  master(b, 0, "");
  master(b, 1, "");
  for (int i = 2; i < 2 + ics::SUPPRESS_MAX + 1; i++) movedToTomorrow(b, i);  // one too many
  b.add("BEGIN:VEVENT\r\nDTSTART:20261123T200000Z\r\nUID:single@google.com\r\nSUMMARY:Single\r\nEND:VEVENT\r\n");
  b.add("END:VCALENDAR\r\n");
  Result r = run(b);
  TEST_ASSERT_EQUAL(1, r.snap.count);  // the single event is unaffected
  expectEvent(r.snap, 0, "Single", 900, 900);
  TEST_ASSERT_EQUAL(2, r.snap.skipped);  // both series occurrences: might have been moved
}

void test_exdate_overflow_is_uncertain() {
  Buf b;
  b.add("%s", HEAD);
  char ex[512] = "EXDATE;TZID=America/New_York:";
  for (int d = 14; d <= 25; d++) {
    if (d != 23) snprintf(ex + strlen(ex), sizeof ex - strlen(ex), "202611%02dT050000,", d);
  }
  strcat(ex, "20261114T060000,20261115T060000\r\n");  // 13 values near today: one too many
  master(b, 0, ex);
  master(b, 1, "EXDATE;TZID=Europe/Berlin:20261123T110000\r\n");  // foreign EXDATE near today
  master(b, 2, "EXDATE;TZID=America/New_York:20200101T051000\r\n");  // far away: harmless
  b.add("END:VCALENDAR\r\n");
  Result r = run(b);
  TEST_ASSERT_EQUAL(1, r.snap.count);
  expectEvent(r.snap, 0, "Series 002", 320, 325);
  TEST_ASSERT_EQUAL(2, r.snap.skipped);
}

void test_unsupported_series_variants() {
  Result r = run(
      "BEGIN:VCALENDAR\n"
      // Occurrences longer than SPAN_MAX_DAYS.
      "BEGIN:VEVENT\nDTSTART;VALUE=DATE:20261101\nDTEND;VALUE=DATE:20261111\n"
      "RRULE:FREQ=WEEKLY\nSUMMARY:Long\nEND:VEVENT\n"
      // RDATE near today, and one long ago (not counted).
      "BEGIN:VEVENT\nDTSTART:20261101T150000Z\nRDATE:20261123T150000Z\nSUMMARY:R1\nEND:VEVENT\n"
      "BEGIN:VEVENT\nDTSTART:20101101T150000Z\nRDATE:20101123T150000Z\nSUMMARY:R2\nEND:VEVENT\n"
      // RANGE=THISANDFUTURE override from last week: the series today is uncertain.
      "BEGIN:VEVENT\nDTSTART:20261101T150000Z\nRRULE:FREQ=DAILY\nUID:tf@google.com\n"
      "SUMMARY:Daily\nEND:VEVENT\n"
      "BEGIN:VEVENT\nRECURRENCE-ID;RANGE=THISANDFUTURE:20261116T150000Z\n"
      "DTSTART:20261116T160000Z\nUID:tf@google.com\nSUMMARY:Daily later\nEND:VEVENT\n"
      // Malformed DURATION near today; malformed far away (not counted).
      "BEGIN:VEVENT\nDTSTART:20261123T150000Z\nDURATION:P1W1W\nSUMMARY:Bad\nEND:VEVENT\n"
      "BEGIN:VEVENT\nDTSTART:20101123T150000Z\nDURATION:PXD\nSUMMARY:Bad old\nEND:VEVENT\n"
      "END:VCALENDAR\n");
  TEST_ASSERT_EQUAL(0, r.snap.count);
  TEST_ASSERT_EQUAL(4, r.snap.skipped);  // Long, R1, Daily (wildcard), Bad
}

void test_two_megabyte_feed() {
  Buf b;
  b.add("%s", HEAD);
  char desc[400];
  for (size_t i = 0; i < sizeof desc - 1; i++) desc[i] = static_cast<char>('a' + i % 26);
  desc[sizeof desc - 1] = '\0';
  int events = 0;
  for (int i = 0; b.n < 2 * 1024 * 1024; i++, events++) {
    int y = 2015 + i % 11, m = 1 + i % 12, d = 1 + i % 28;
    if (i % 3 == 0) {
      b.add("BEGIN:VEVENT\r\nDTSTART:%04d%02d%02dT150000Z\r\nDTEND:%04d%02d%02dT160000Z\r\n",
            y, m, d, y, m, d);
    } else if (i % 3 == 1) {
      b.add("BEGIN:VEVENT\r\nDTSTART;TZID=America/New_York:%04d%02d%02dT090000\r\n"
            "RRULE:FREQ=WEEKLY;BYDAY=MO,TH;UNTIL=%04d1231T000000Z\r\n", y, m, d, y);
    } else {
      b.add("BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:%04d%02d%02d\r\nRRULE:FREQ=DAILY;COUNT=%d\r\n",
            y, m, d, 1 + i % 40);
    }
    b.add("UID:hist%06d@google.com\r\nSUMMARY:Past %d\r\nDESCRIPTION:%s\r\n", i, i, desc);
    if (i % 50 == 0) {
      b.add("BEGIN:VALARM\r\nACTION:DISPLAY\r\nTRIGGER:-PT10M\r\nEND:VALARM\r\nX-JUNK:");
      for (int k = 0; k < 30; k++) b.add("%s", desc);
      b.add("\r\n");
    }
    b.add("END:VEVENT\r\n");
  }
  b.add("BEGIN:VEVENT\r\nDTSTART:20261123T143000Z\r\nDTEND:20261123T150000Z\r\nSUMMARY:Today 1\r\nEND:VEVENT\r\n");
  b.add("BEGIN:VEVENT\r\nDTSTART;TZID=America/New_York:20200106T080000\r\nRRULE:FREQ=WEEKLY;BYDAY=MO\r\n"
        "SUMMARY:Today 2\r\nEND:VEVENT\r\n");
  b.add("BEGIN:VEVENT\r\nDTSTART;VALUE=DATE:20261123\r\nSUMMARY:Today 3\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n");
  clock_t t0 = clock();
  Result r = run(b);
  double secs = static_cast<double>(clock() - t0) / CLOCKS_PER_SEC;
  char msg[128];
  snprintf(msg, sizeof msg, "%u bytes, %d events in %.3f s (%.1f MB/s)", static_cast<unsigned>(b.n),
           events, secs, b.n / 1048576.0 / (secs > 0 ? secs : 1e-9));
  TEST_MESSAGE(msg);
  TEST_ASSERT_TRUE(r.ok);
  TEST_ASSERT_EQUAL(3, r.snap.count);
  TEST_ASSERT_EQUAL(0, r.snap.skipped);
  expectEvent(r.snap, 0, "Today 3", -1, 1440);
  expectEvent(r.snap, 1, "Today 2", 480, 480);
  expectEvent(r.snap, 2, "Today 1", 570, 600);
  TEST_ASSERT_LESS_THAN(2.0, secs);
}

void test_fuzz() {
  static const char* const names[] = {"singles.ics", "details.ics", "allday.ics", "recur.ics",
                                      "overrides_before.ics", "foreign.ics", "many.ics", "dst.ics"};
  static uint8_t copy[FIXTURE_MAX + 64];
  uint32_t seed = 2026;
  auto rnd = [&seed]() { return (seed = seed * 1103515245u + 12345u) >> 8; };
  static const char pieces[][24] = {"\r\n", "\n ", "BEGIN:VEVENT\r\n", "END:VEVENT\r\n",
                                    "BEGIN:VALARM\n", ":", ";", "\\", "\xC3", "RRULE:FREQ=WEEKLY;",
                                    "DTSTART:2026", "EXDATE:", ",", "Z", "\0"};
  for (int iter = 0; iter < 400; iter++) {
    size_t n;
    if (iter % 4 == 0) {  // random bytes biased towards structure
      n = rnd() % 4096;
      for (size_t i = 0; i < n;) {
        if (rnd() % 3 == 0) {
          const char* p = pieces[rnd() % (sizeof pieces / sizeof pieces[0])];
          size_t k = strlen(p) ? strlen(p) : 1;
          for (size_t j = 0; j < k && i < n; j++) copy[i++] = static_cast<uint8_t>(p[j]);
        } else {
          copy[i++] = static_cast<uint8_t>(rnd());
        }
      }
    } else {  // a corrupted fixture
      const uint8_t* src = load(names[rnd() % (sizeof names / sizeof names[0])], n);
      memcpy(copy, src, n);
      int edits = static_cast<int>(rnd() % 40) + 1;
      for (int e = 0; e < edits && n > 0; e++) {
        size_t pos = rnd() % n;
        switch (rnd() % 4) {
          case 0: copy[pos] = static_cast<uint8_t>(rnd()); break;
          case 1: copy[pos] = static_cast<uint8_t>("\r\n :;,\\"[rnd() % 7]); break;
          case 2: memmove(copy + pos, copy + pos + 1, n - pos - 1); n--; break;
          default: n = pos + (n - pos) / 2; break;  // truncate
        }
      }
    }
    Result r = parse(copy, n, at(2026, 11, 23), NY, Mode::Random, static_cast<uint32_t>(iter));
    TEST_ASSERT_LESS_OR_EQUAL(limits::CAL_EVENTS_MAX, r.snap.count);
    for (uint8_t i = 0; i < r.snap.count; i++) {
      const CalEvent& e = r.snap.events[i];
      TEST_ASSERT_LESS_OR_EQUAL(limits::CAL_TITLE_MAX, strlen(e.title));
      TEST_ASSERT_TRUE(e.startMin >= -1 && e.startMin < 1440);
      TEST_ASSERT_TRUE(e.endMin >= 0 && e.endMin <= 1440);
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_state_is_small);
  RUN_TEST(test_not_a_calendar);
  RUN_TEST(test_truncated_file_keeps_complete_events);
  RUN_TEST(test_megabyte_line);
  RUN_TEST(test_broken_nesting);
  RUN_TEST(test_candidate_eviction_is_counted_not_misplaced);
  RUN_TEST(test_suppression_overflow_drops_series);
  RUN_TEST(test_exdate_overflow_is_uncertain);
  RUN_TEST(test_unsupported_series_variants);
  RUN_TEST(test_two_megabyte_feed);
  RUN_TEST(test_fuzz);
  return UNITY_END();
}

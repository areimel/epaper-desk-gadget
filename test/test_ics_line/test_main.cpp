#include <string.h>
#include <unity.h>

#include "IcsLine.h"

void setUp() {}
void tearDown() {}

// Collects the logical lines of an input, fed in chunks of `chunk` bytes.
struct Lines {
  char text[8][ics::LINE_CAP + 1];
  bool truncated[8];
  size_t len[8];
  int count = 0;
};

static ics::LineReader reader;

static void take(Lines& out) {
  if (out.count < 8) {
    memcpy(out.text[out.count], reader.text(), reader.length() + 1);
    out.len[out.count] = reader.length();
    out.truncated[out.count] = reader.truncated();
    out.count++;
  }
  reader.clear();
}

static void readAll(const char* s, size_t n, Lines& out) {
  reader.clear();
  for (size_t i = 0; i < n; i++) {
    uint8_t b = static_cast<uint8_t>(s[i]);
    if (reader.endsBefore(b)) take(out);
    reader.add(b);
  }
  if (reader.pending()) take(out);
}

static void readStr(const char* s, Lines& out) { readAll(s, strlen(s), out); }

void test_crlf_and_lf_lines() {
  Lines l;
  readStr("BEGIN:VCALENDAR\r\nVERSION:2.0\nEND:VCALENDAR", l);
  TEST_ASSERT_EQUAL(3, l.count);
  TEST_ASSERT_EQUAL_STRING("BEGIN:VCALENDAR", l.text[0]);
  TEST_ASSERT_EQUAL_STRING("VERSION:2.0", l.text[1]);
  TEST_ASSERT_EQUAL_STRING("END:VCALENDAR", l.text[2]);
}

void test_fold_with_space_and_tab() {
  Lines l;
  readStr("SUMMARY:Team\r\n  sync\r\n\tlater\r\nUID:x\r\n", l);
  TEST_ASSERT_EQUAL(2, l.count);
  TEST_ASSERT_EQUAL_STRING("SUMMARY:Team synclater", l.text[0]);
  TEST_ASSERT_EQUAL_STRING("UID:x", l.text[1]);
}

void test_fold_inside_utf8_sequence() {
  // "Café" with the two bytes of é (C3 A9) split by a CRLF fold.
  Lines l;
  readStr("SUMMARY:Caf\xC3\r\n \xA9 cr\xC3\xA8me\r\n", l);
  TEST_ASSERT_EQUAL(1, l.count);
  TEST_ASSERT_EQUAL_STRING("SUMMARY:Caf\xC3\xA9 cr\xC3\xA8me", l.text[0]);
}

void test_blank_lines_and_trailing_fold() {
  Lines l;
  readStr("A:1\r\n\r\nB:2\r\n ", l);
  // The empty line is reported as an empty line; the final fold just continues B.
  TEST_ASSERT_EQUAL(3, l.count);
  TEST_ASSERT_EQUAL_STRING("", l.text[1]);
  TEST_ASSERT_EQUAL_STRING("B:2", l.text[2]);
}

void test_overlong_line_keeps_prefix_and_sync() {
  static char big[3 * ics::LINE_CAP + 64];
  strcpy(big, "DESCRIPTION:");
  size_t n = strlen(big);
  while (n < 3 * ics::LINE_CAP) big[n++] = 'x';
  memcpy(big + n, "\r\nUID:after\r\n", 13);
  n += 13;
  Lines l;
  readAll(big, n, l);
  TEST_ASSERT_EQUAL(2, l.count);
  TEST_ASSERT_TRUE(l.truncated[0]);
  TEST_ASSERT_EQUAL(ics::LINE_CAP, l.len[0]);
  TEST_ASSERT_FALSE(l.truncated[1]);
  TEST_ASSERT_EQUAL_STRING("UID:after", l.text[1]);
}

void test_nul_bytes_dropped() {
  Lines l;
  readAll("A:\0b\r\n", 6, l);
  TEST_ASSERT_EQUAL(1, l.count);
  TEST_ASSERT_EQUAL_STRING("A:b", l.text[0]);
}

void test_split_prop_with_params() {
  char line[] = "DTSTART;TZID=\"America/New_York\";VALUE=DATE-TIME:20261123T090000";
  ics::Prop p;
  TEST_ASSERT_TRUE(ics::splitProp(line, strlen(line), p));
  TEST_ASSERT_TRUE(ics::is(p, "DTSTART"));
  TEST_ASSERT_FALSE(ics::is(p, "DTSTAR"));
  TEST_ASSERT_EQUAL_STRING("20261123T090000", p.value);
  char tz[48];
  TEST_ASSERT_TRUE(ics::param(p, "TZID", tz, sizeof tz));
  TEST_ASSERT_EQUAL_STRING("America/New_York", tz);
  TEST_ASSERT_TRUE(ics::param(p, "VALUE", tz, sizeof tz));
  TEST_ASSERT_EQUAL_STRING("DATE-TIME", tz);
  TEST_ASSERT_FALSE(ics::param(p, "RANGE", tz, sizeof tz));
}

void test_split_prop_quoted_colon_and_case() {
  char line[] = "dtstart;tzid=\"odd:zone\":20261123T090000";
  ics::Prop p;
  TEST_ASSERT_TRUE(ics::splitProp(line, strlen(line), p));
  TEST_ASSERT_TRUE(ics::is(p, "DTSTART"));
  TEST_ASSERT_EQUAL_STRING("20261123T090000", p.value);
  char tz[48];
  TEST_ASSERT_TRUE(ics::param(p, "TZID", tz, sizeof tz));
  TEST_ASSERT_EQUAL_STRING("odd:zone", tz);
  TEST_ASSERT_FALSE(ics::param(p, "TZID", tz, 4));  // does not fit
  TEST_ASSERT_TRUE(ics::hasParam(p, "TZID"));
  TEST_ASSERT_FALSE(ics::hasParam(p, "VALUE"));
}

void test_split_prop_rejects_non_property() {
  char a[] = "<html><body>Not found</body></html>";
  char b[] = "";
  ics::Prop p;
  TEST_ASSERT_FALSE(ics::splitProp(a, strlen(a), p));
  TEST_ASSERT_FALSE(ics::splitProp(b, 0, p));
}

void test_unescape_text() {
  char s[] = "Lunch\\, then\\; review\\\\notes\\nnext\\Nline\\";
  ics::unescapeText(s);
  TEST_ASSERT_EQUAL_STRING("Lunch, then; review\\notes next line", s);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crlf_and_lf_lines);
  RUN_TEST(test_fold_with_space_and_tab);
  RUN_TEST(test_fold_inside_utf8_sequence);
  RUN_TEST(test_blank_lines_and_trailing_fold);
  RUN_TEST(test_overlong_line_keeps_prefix_and_sync);
  RUN_TEST(test_nul_bytes_dropped);
  RUN_TEST(test_split_prop_with_params);
  RUN_TEST(test_split_prop_quoted_colon_and_case);
  RUN_TEST(test_split_prop_rejects_non_property);
  RUN_TEST(test_unescape_text);
  return UNITY_END();
}

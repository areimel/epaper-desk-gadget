#include <stdint.h>

#include "Text.h"

namespace text {
namespace {

constexpr uint32_t BAD = 0xFFFFFFFF;  // marks a malformed sequence

// Decodes one code point at s and advances s by the bytes consumed (at least 1, never past
// the NUL: a truncated sequence consumes only its valid prefix).
uint32_t decode(const unsigned char*& s) {
  unsigned char b = *s++;
  if (b < 0x80) return b;
  int extra;
  uint32_t cp, min;
  if (b >= 0xC2 && b <= 0xDF) { extra = 1; cp = b & 0x1F; min = 0x80; }
  else if (b >= 0xE0 && b <= 0xEF) { extra = 2; cp = b & 0x0F; min = 0x800; }
  else if (b >= 0xF0 && b <= 0xF4) { extra = 3; cp = b & 0x07; min = 0x10000; }
  else return BAD;  // stray continuation, C0/C1 overlong lead, F5+
  for (int i = 0; i < extra; i++) {
    if ((*s & 0xC0) != 0x80) return BAD;  // also true at the NUL, so we stop there
    cp = (cp << 6) | (*s++ & 0x3F);
  }
  if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return BAD;
  return cp;
}

// Builds the output, enforcing capacity, space collapsing and "?" collapsing.
struct Sink {
  char* out;
  size_t cap;  // bytes available, excluding the NUL
  size_t n = 0;
  bool lastQ = false;  // the last emitted byte was a substitution "?"

  void put(char c) {
    if (n < cap) out[n++] = c;
  }
  void byte(char c) {
    put(c);
    lastQ = false;
  }
  void space() {
    if (n > 0 && out[n - 1] != ' ') byte(' ');  // no leading space, no runs
  }
  void unknown() {
    if (!lastQ) {
      put('?');
      lastQ = true;
    }
  }
  void str(const char* s) {
    while (*s) byte(*s++);
  }
};

void emit(Sink& k, uint32_t cp) {
  if (cp == BAD) return k.unknown();
  if (cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n' || cp == 0xA0) return k.space();
  if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F)) return;  // control: dropped
  if (cp <= 0x7E || (cp >= 0xA1 && cp <= 0xFF)) return k.byte(static_cast<char>(cp));
  switch (cp) {
    case 0x2018: case 0x2019: case 0x201A: case 0x2032: return k.byte('\'');
    case 0x201C: case 0x201D: case 0x201E: case 0x2033: return k.byte('"');
    case 0x2013: case 0x2014: case 0x2212: return k.byte('-');
    case 0x2026: return k.str("...");
    case 0x2022: return k.byte('*');
    case 0x2009: case 0x200A: case 0x202F: return k.space();
    case 0x200B: case 0x200C: case 0x200D: case 0xFEFF: return;  // zero width: dropped
    default: return k.unknown();
  }
}

}  // namespace

size_t fold(const char* utf8, char* out, size_t cap) {
  if (cap == 0) return 0;
  Sink k{out, cap - 1};
  if (utf8) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(utf8);
    while (*s) emit(k, decode(s));
  }
  while (k.n > 0 && out[k.n - 1] == ' ') k.n--;  // trailing space
  out[k.n] = '\0';
  return k.n;
}

}  // namespace text

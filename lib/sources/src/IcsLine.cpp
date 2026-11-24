#include "IcsLine.h"

#include <string.h>

namespace ics {
namespace {

char upper(char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; }

}  // namespace

void LineReader::add(uint8_t b) {
  if (eol_) {
    // endsBefore() was false, so b is the space or tab of a fold: drop it and the line break.
    eol_ = false;
    if (b == ' ' || b == '\t') return;
  }
  if (b == '\n') {
    eol_ = true;
    return;
  }
  if (b == '\r' || b == 0) return;  // CRLF and bare LF look the same; NUL would end C strings
  if (len_ < LINE_CAP) {
    buf_[len_++] = static_cast<char>(b);
    buf_[len_] = '\0';
  } else {
    truncated_ = true;  // keep the prefix, drop the rest of the logical line
  }
}

void LineReader::clear() {
  len_ = 0;
  buf_[0] = '\0';
  eol_ = false;
  truncated_ = false;
}

bool splitProp(char* line, size_t len, Prop& p) {
  size_t nameEnd = 0;
  while (nameEnd < len && line[nameEnd] != ';' && line[nameEnd] != ':') nameEnd++;
  if (nameEnd == 0 || nameEnd >= len) return false;
  size_t colon = nameEnd;
  bool quoted = false;
  for (; colon < len; colon++) {
    char c = line[colon];
    if (c == '"') quoted = !quoted;
    else if (c == ':' && !quoted) break;
  }
  if (colon >= len) return false;
  p.name = line;
  p.nameLen = nameEnd;
  p.params = line + nameEnd + (line[nameEnd] == ';' ? 1 : 0);
  p.paramsLen = colon > nameEnd ? colon - nameEnd - 1 : 0;
  p.value = line + colon + 1;
  p.valueLen = len - colon - 1;
  p.value[p.valueLen] = '\0';
  return true;
}

bool equalsUpper(const char* s, size_t len, const char* upperLit) {
  size_t i = 0;
  for (; i < len && upperLit[i]; i++) {
    if (upper(s[i]) != upperLit[i]) return false;
  }
  return i == len && upperLit[i] == '\0';
}

bool equalsUpper(const char* s, const char* upperLit) { return equalsUpper(s, strlen(s), upperLit); }

bool is(const Prop& p, const char* upperName) { return equalsUpper(p.name, p.nameLen, upperName); }

bool param(const Prop& p, const char* upperKey, char* out, size_t cap) {
  const char* s = p.params;
  const char* end = p.params + p.paramsLen;
  while (s < end) {
    // One parameter: KEY=VALUE up to the next ';' outside quotes.
    const char* eq = s;
    while (eq < end && *eq != '=' && *eq != ';') eq++;
    const char* v = eq < end && *eq == '=' ? eq + 1 : eq;
    const char* vEnd = v;
    bool quoted = false;
    while (vEnd < end && (quoted || *vEnd != ';')) {
      if (*vEnd == '"') quoted = !quoted;
      vEnd++;
    }
    if (equalsUpper(s, static_cast<size_t>(eq - s), upperKey)) {
      if (vEnd > v && *v == '"') v++;
      const char* stop = (vEnd > v && vEnd[-1] == '"') ? vEnd - 1 : vEnd;
      size_t n = static_cast<size_t>(stop - v);
      if (n + 1 > cap) return false;
      memcpy(out, v, n);
      out[n] = '\0';
      return true;
    }
    s = vEnd < end ? vEnd + 1 : end;
  }
  return false;
}

void unescapeText(char* s) {
  char* w = s;
  for (const char* r = s; *r; r++) {
    if (*r != '\\') {
      *w++ = *r;
      continue;
    }
    char n = r[1];
    if (n == '\0') break;  // a lone trailing backslash is dropped
    *w++ = (n == 'n' || n == 'N') ? ' ' : n;
    r++;
  }
  *w = '\0';
}

}  // namespace ics

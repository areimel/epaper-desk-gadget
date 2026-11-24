#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FeedJson.h"

namespace feed {
namespace {

// Appends into a fixed buffer; once it is full every later call is a no-op and ok() is false.
class Out {
 public:
  Out(char* buf, size_t cap) : buf_(buf), cap_(cap) {}
  bool ok() const { return !over_; }
  size_t size() const { return n_; }

  void put(char ch) {
    if (n_ + 1 >= cap_) {  // keep one byte for the NUL
      over_ = true;
      return;
    }
    buf_[n_++] = ch;
  }
  void raw(const char* s) {
    while (*s) put(*s++);
  }
  // A JSON string. Quote, backslash and control characters are escaped; UTF-8 passes through.
  void str(const char* s) {
    put('"');
    for (; *s; s++) {
      unsigned char ch = static_cast<unsigned char>(*s);
      if (ch == '"' || ch == '\\') {
        put('\\');
        put(static_cast<char>(ch));
      } else if (ch < 0x20) {
        char e[8];
        snprintf(e, sizeof e, "\\u%04x", ch);
        raw(e);
      } else {
        put(static_cast<char>(ch));
      }
    }
    put('"');
  }
  void key(const char* k) {
    str(k);
    put(':');
  }
  void boolean(bool b) { raw(b ? "true" : "false"); }
  void uint(unsigned v) {
    char e[12];
    snprintf(e, sizeof e, "%u", v);
    raw(e);
  }
  // The shortest %g form that reads back as the same float, so 39.95f is written as 39.95.
  void number(float v) {
    char e[24];
    for (int digits = 6; digits <= 9; digits++) {
      snprintf(e, sizeof e, "%.*g", digits, static_cast<double>(v));
      if (strtof(e, nullptr) == v) break;
    }
    raw(e);
  }
  void terminate() {
    if (cap_ == 0) return;
    buf_[over_ ? 0 : n_] = 0;
  }

 private:
  char* buf_;
  size_t cap_;
  size_t n_ = 0;
  bool over_ = false;
};

void writeWeather(Out& o, const FeedConfig& f) {
  o.key("weather");
  if (!f.weather.set) {
    o.raw("null");
    return;
  }
  o.raw("{");
  o.key("place");
  o.str(f.weather.place);
  o.raw(",");
  o.key("lat");
  o.number(f.weather.lat);
  o.raw(",");
  o.key("lon");
  o.number(f.weather.lon);
  o.raw(",");
  o.key("units");
  o.str(f.weather.units == Units::Fahrenheit ? "f" : "c");
  o.raw("}");
}

void writeCalendar(Out& o, const FeedConfig& f, bool redact) {
  o.key("calendar");
  o.raw("{");
  if (redact) {
    o.key("set");
    o.boolean(f.calendarUrl[0] != 0);
  } else {
    o.key("url");
    o.str(f.calendarUrl);
  }
  o.raw("}");
}

void writeNotes(Out& o, const FeedConfig& f) {
  o.key("notes");
  o.raw("{");
  o.key("title");
  o.str(f.notes.title);
  o.raw(",");
  o.key("lines");
  o.raw("[");
  for (uint8_t i = 0; i < f.notes.lineCount; i++) {
    const NoteLine& l = f.notes.lines[i];
    if (i) o.raw(",");
    o.raw("{");
    o.key("text");
    o.str(l.text);
    o.raw(",");
    o.key("todo");
    o.boolean(l.todo);
    o.raw(",");
    o.key("done");
    o.boolean(l.todo && l.done);
    o.raw("}");
  }
  o.raw("]}");
}

}  // namespace

size_t toJson(const FeedConfig& f, bool redact, char* out, size_t cap) {
  Out o(out, cap);
  o.raw("{");
  o.key("v");
  o.uint(limits::FEED_SCHEMA);
  o.raw(",");
  o.key("dwellSec");
  o.uint(f.dwellSec);
  o.raw(",");
  o.key("tz");
  o.raw("{");
  o.key("iana");
  o.str(f.tzIana);
  o.raw(",");
  o.key("posix");
  o.str(f.tzPosix);
  o.raw("},");
  o.key("pages");
  o.raw("[");
  for (uint8_t i = 0; i < limits::PAGE_COUNT; i++) {
    if (i) o.raw(",");
    o.raw("{");
    o.key("id");
    o.str(pageName(f.pages[i].id));
    o.raw(",");
    o.key("on");
    o.boolean(f.pages[i].on);
    o.raw("}");
  }
  o.raw("],");
  writeWeather(o, f);
  o.raw(",");
  writeCalendar(o, f, redact);
  o.raw(",");
  o.key("messages");
  o.raw("[");
  for (uint8_t i = 0; i < f.messageCount; i++) {
    if (i) o.raw(",");
    o.raw("{");
    o.key("text");
    o.str(f.messages[i].text);
    o.raw(",");
    o.key("by");
    o.str(f.messages[i].by);
    o.raw("}");
  }
  o.raw("],");
  writeNotes(o, f);
  o.raw("}");
  o.terminate();
  return o.ok() ? o.size() : 0;
}

}  // namespace feed

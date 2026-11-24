#pragma once
#include <stddef.h>
#include <stdint.h>

// iCalendar content lines: byte-level unfolding and property splitting. Pure, no allocation.
namespace ics {

constexpr size_t LINE_CAP = 1024;  // longest logical line kept; longer ones keep this prefix

// Turns raw bytes into unfolded logical lines. Usage, per byte b:
//   if (r.endsBefore(b)) { handle(r.text(), r.length(), r.truncated()); r.clear(); }
//   r.add(b);
// and at end of input: if (r.pending()) { handle(...); r.clear(); }
class LineReader {
 public:
  // True when the bytes so far form a complete line that b does not continue (b is not a fold).
  bool endsBefore(uint8_t b) const { return eol_ && b != ' ' && b != '\t'; }
  void add(uint8_t b);
  bool pending() const { return eol_ || len_ > 0 || truncated_; }
  char* text() { return buf_; }  // NUL-terminated, writable (callers unescape in place)
  size_t length() const { return len_; }
  bool truncated() const { return truncated_; }
  void clear();

 private:
  char buf_[LINE_CAP + 1] = {};
  uint16_t len_ = 0;
  bool eol_ = false;  // saw LF: the next byte decides between fold and new line
  bool truncated_ = false;
};

// One content line split as NAME *(;PARAM) : VALUE. Pointers into the line; not NUL-terminated
// except value (the rest of the line).
struct Prop {
  const char* name;
  size_t nameLen;
  const char* params;  // after the first ';' up to the separating ':' (respecting quotes)
  size_t paramsLen;
  char* value;  // NUL-terminated
  size_t valueLen;
};

// False when the line has no ':' outside quotes (not a property).
bool splitProp(char* line, size_t len, Prop& p);

// Case-insensitive name test, e.g. is(p, "DTSTART").
bool is(const Prop& p, const char* upperName);

// Copies the value of parameter `upperKey` (quotes stripped) into out. False if absent or does
// not fit.
bool param(const Prop& p, const char* upperKey, char* out, size_t cap);

// True if parameter `upperKey` is present, whatever its length.
bool hasParam(const Prop& p, const char* upperKey);

// Case-insensitive comparison of a NUL-terminated string with an uppercase literal.
bool equalsUpper(const char* s, const char* upper);
bool equalsUpper(const char* s, size_t len, const char* upper);

// RFC 5545 TEXT unescaping in place: \, \; \\ kept literally, \n and \N become a space.
void unescapeText(char* s);

}  // namespace ics

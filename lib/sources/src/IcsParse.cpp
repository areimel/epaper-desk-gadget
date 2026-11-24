// Component state machine over unfolded lines. See the design note in IcsParse.h.
#include "IcsParse.h"

#include <string.h>

namespace {

// BEGIN/END values may carry trailing blanks in sloppy files.
void trimValue(ics::Prop& p) {
  while (p.valueLen > 0 && (p.value[p.valueLen - 1] == ' ' || p.value[p.valueLen - 1] == '\t')) {
    p.value[--p.valueLen] = '\0';
  }
}

}  // namespace

IcsParser::IcsParser(time_t now, const char* deviceTzIana) : day_(ics::makeDay(now, deviceTzIana)) {
  event_.reset();
}

bool IcsParser::write(const uint8_t* data, size_t n) {
  if (finished_ || (!data && n)) return false;
  for (size_t i = 0; i < n; i++) {
    const uint8_t b = data[i];
    if (reader_.endsBefore(b)) {
      onLine(reader_.text(), reader_.length(), reader_.truncated());
      reader_.clear();
    }
    reader_.add(b);
  }
  return true;
}

void IcsParser::beginEvent() {
  event_.reset();
  inEvent_ = true;
  nest_ = 0;
}

void IcsParser::onLine(char* line, size_t len, bool truncated) {
  if (!seenCalendar_) {
    // Arm on BEGIN:VCALENDAR (after an optional UTF-8 byte order mark); ignore anything before.
    if (len >= 3 && memcmp(line, "\xEF\xBB\xBF", 3) == 0) {
      line += 3;
      len -= 3;
    }
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) len--;
    seenCalendar_ = !truncated && ics::equalsUpper(line, len, "BEGIN:VCALENDAR");
    return;
  }
  ics::Prop p;
  if (!ics::splitProp(line, len, p)) return;
  if (ics::is(p, "BEGIN")) {
    trimValue(p);
    // A VEVENT cannot nest: a second BEGIN:VEVENT discards an unterminated one.
    if (ics::equalsUpper(p.value, "VEVENT")) beginEvent();
    else if (inEvent_ && nest_ < 255) nest_++;  // VALARM etc.: its properties are not the event's
    return;
  }
  if (ics::is(p, "END")) {
    trimValue(p);
    if (!inEvent_) return;
    if (ics::equalsUpper(p.value, "VEVENT")) {
      // Closes the event even if a VALARM inside it was never closed.
      event_.evaluate(day_, collector_);
      inEvent_ = false;
    } else if (ics::equalsUpper(p.value, "VCALENDAR")) {
      inEvent_ = false;  // truncated event: dropped
    } else if (nest_ > 0) {
      nest_--;
    }
    return;
  }
  if (inEvent_ && nest_ == 0) event_.property(p, truncated, day_);
}

bool IcsParser::finish(CalSnap& snap) {
  if (!finished_) {
    if (reader_.pending()) {
      onLine(reader_.text(), reader_.length(), reader_.truncated());
      reader_.clear();
    }
    inEvent_ = false;  // an event without END:VEVENT is incomplete: dropped
    finished_ = true;
  }
  collector_.finish(snap);
  return seenCalendar_;
}

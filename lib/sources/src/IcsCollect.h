#pragma once
#include <stdint.h>

#include "Limits.h"
#include "Snapshots.h"

// Today's candidate occurrences and the overrides that replace series instances, reconciled
// once the whole file has been read (masters and overrides may come in either order).
namespace ics {

constexpr uint8_t CAND_MAX = 24;
constexpr uint8_t SUPPRESS_MAX = 32;

// Identifies an occurrence start: its civil day (in the series' calendar) and, for timed
// values, its instant. Two keys match on the instant when both have one, else on the day.
struct Key {
  int32_t day;
  int64_t inst;  // NO_INSTANT for dates
};
bool keysMatch(const Key& a, const Key& b);

struct Candidate {
  char title[limits::CAL_TITLE_MAX + 1];
  int16_t startMin;  // -1: all day, or began before today
  int16_t endMin;    // 1440: runs past today
  bool allDay;
  bool fromSeries;  // an RRULE occurrence that an override may replace
  uint64_t uid;     // FNV-1a of UID; 0 = none
  Key key;          // the occurrence's original start
};

class Collector {
 public:
  void add(const Candidate& c);
  // An override for (uid, original start). wildcard: the original start could not be read, so
  // every occurrence of that series today is uncertain.
  void suppress(uint64_t uid, const Key& original, bool wildcard);
  void skip();
  // Probe mode, for an event cut off by broken structure: add() and skip() only record that
  // something would have been shown or counted; suppress() still applies, as a wildcard.
  void beginProbe();
  bool endProbe();  // true if the probed event would have touched today
  // Idempotent: a second call produces the same snapshot.
  void finish(CalSnap& snap);

 private:
  struct Suppression {
    uint64_t uid;
    Key key;
    bool wildcard;
  };
  static void addTo(uint16_t& total, uint16_t n);

  Candidate cand_[CAND_MAX];
  Suppression sup_[SUPPRESS_MAX];
  uint8_t candCount_ = 0;
  uint8_t supCount_ = 0;
  bool supOverflow_ = false;
  uint16_t evicted_ = 0;
  uint16_t skipped_ = 0;
  uint16_t dropped_ = 0;  // series occurrences removed as uncertain by finish()
  bool probe_ = false;
  bool probeHit_ = false;
};

}  // namespace ics

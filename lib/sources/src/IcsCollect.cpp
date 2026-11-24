#include "IcsCollect.h"

#include <string.h>

#include "IcsTime.h"

namespace ics {
namespace {

// Display order: all-day first, then start minute, then title (then end, for determinism).
bool before(const Candidate& a, const Candidate& b) {
  if (a.allDay != b.allDay) return a.allDay;
  if (a.startMin != b.startMin) return a.startMin < b.startMin;
  int t = strcmp(a.title, b.title);
  if (t != 0) return t < 0;
  return a.endMin < b.endMin;
}

}  // namespace

bool keysMatch(const Key& a, const Key& b) {
  if (a.inst != NO_INSTANT && b.inst != NO_INSTANT) return a.inst == b.inst;
  return a.day == b.day;
}

void Collector::addTo(uint16_t& total, uint16_t n) {
  uint32_t sum = static_cast<uint32_t>(total) + n;
  total = static_cast<uint16_t>(sum > 0xFFFF ? 0xFFFF : sum);
}

void Collector::skip() {
  if (probe_) probeHit_ = true;
  else addTo(skipped_, 1);
}

void Collector::beginProbe() {
  probe_ = true;
  probeHit_ = false;
}

bool Collector::endProbe() {
  probe_ = false;
  return probeHit_;
}

void Collector::add(const Candidate& c) {
  if (probe_) {
    probeHit_ = true;
    return;
  }
  if (candCount_ < CAND_MAX) {
    cand_[candCount_++] = c;
    return;
  }
  // Full: evict whichever of the table and c sorts last. It may only be missed, never misplaced
  // (finish() counts evictions as skipped if they could have been shown).
  uint8_t last = 0;
  for (uint8_t i = 1; i < CAND_MAX; i++) {
    if (before(cand_[last], cand_[i])) last = i;
  }
  if (evicted_ < 0xFFFF) evicted_++;
  if (before(c, cand_[last])) cand_[last] = c;
}

void Collector::suppress(uint64_t uid, const Key& original, bool wildcard) {
  if (supCount_ >= SUPPRESS_MAX) {
    supOverflow_ = true;
    return;
  }
  // An incomplete override may have been cancelled or moved: treat its series as uncertain.
  sup_[supCount_++] = Suppression{uid, original, wildcard || probe_};
}

void Collector::finish(CalSnap& snap) {
  // Remove series occurrences that an override replaced. Overflow: any of them may have been
  // moved, so none is shown.
  uint8_t kept = 0;
  for (uint8_t i = 0; i < candCount_; i++) {
    const Candidate& c = cand_[i];
    bool drop = false, uncertain = false;
    if (c.fromSeries && supOverflow_) {
      drop = uncertain = true;
    } else if (c.fromSeries && c.uid != 0) {
      for (uint8_t j = 0; j < supCount_ && !drop; j++) {
        if (sup_[j].uid != c.uid) continue;
        if (sup_[j].wildcard) drop = uncertain = true;
        else drop = keysMatch(sup_[j].key, c.key);
      }
    }
    if (uncertain) addTo(dropped_, 1);
    if (!drop) cand_[kept++] = c;
  }
  candCount_ = kept;  // removed for good, so a second finish() does not count them again
  // Insertion sort: at most CAND_MAX entries.
  for (uint8_t i = 1; i < kept; i++) {
    Candidate c = cand_[i];
    uint8_t j = i;
    while (j > 0 && before(c, cand_[j - 1])) {
      cand_[j] = cand_[j - 1];
      j--;
    }
    cand_[j] = c;
  }
  uint8_t n = kept < limits::CAL_EVENTS_MAX ? kept : limits::CAL_EVENTS_MAX;
  for (uint8_t i = 0; i < n; i++) {
    CalEvent& e = snap.events[i];
    memcpy(e.title, cand_[i].title, sizeof e.title);
    e.startMin = cand_[i].startMin;
    e.endMin = cand_[i].endMin;
  }
  for (uint8_t i = n; i < limits::CAL_EVENTS_MAX; i++) memset(&snap.events[i], 0, sizeof snap.events[i]);
  snap.count = n;
  uint16_t skipped = skipped_;
  addTo(skipped, dropped_);
  // Evicted candidates could have filled free slots: report them rather than hide them.
  if (evicted_ > 0 && n < limits::CAL_EVENTS_MAX) {
    uint16_t gap = static_cast<uint16_t>(limits::CAL_EVENTS_MAX - n);
    addTo(skipped, evicted_ < gap ? evicted_ : gap);
  }
  snap.skipped = skipped;
}

}  // namespace ics

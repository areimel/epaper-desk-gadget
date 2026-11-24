#pragma once
#include <stdint.h>

#include "FeedConfig.h"

// Decides which page is showing and when to turn to the next. Pure: no Arduino, no heap; time is
// passed in as nowMs (a wrapping uint32_t, compared only by subtraction).
class Rotation {
 public:
  void setDwell(uint32_t dwellMs) { dwell_ = dwellMs ? dwellMs : 1; }  // 0 would spin; use 1 ms

  // The enabled pages in order. Keeps the current page (and its dwell timer) if it is still
  // enabled; otherwise moves to the first enabled page. n may be 0.
  void setPages(const PageId* enabled, uint8_t n, uint32_t nowMs) {
    n_ = n > limits::PAGE_COUNT ? limits::PAGE_COUNT : n;
    for (uint8_t i = 0; i < n_; i++) pages_[i] = enabled[i];
    if (n_ == 0) {
      hasCur_ = false;
      return;
    }
    if (hasCur_ && indexOf(cur_) >= 0) {
      anchor_ = cur_;
      return;
    }
    enter(pages_[0], nowMs, true);
  }

  // True when the dwell elapsed and the page changed. The timer restarts either way.
  bool tick(uint32_t nowMs) {
    if (n_ == 0 || static_cast<uint32_t>(nowMs - since_) < dwell_) return false;
    return advance(nowMs);
  }

  // Advance now (button). True if the page changed.
  bool next(uint32_t nowMs) { return n_ != 0 && advance(nowMs); }

  // Show this page now, even if it is not enabled; the dwell restarts. An unlisted page does not
  // move the anchor, so the next turn goes to the page after the last enabled one shown.
  void show(PageId page, uint32_t nowMs) { enter(page, nowMs, indexOf(page) >= 0); }

  bool current(PageId& out) const {
    if (!hasCur_) return false;
    out = cur_;
    return true;
  }

  // How many times this page was shown before its current (or latest) showing; 0 for the first
  // showing and for a page never shown. Wraps at 255.
  uint8_t visit(PageId page) const { return visits_[static_cast<uint8_t>(page)]; }

 private:
  int indexOf(PageId page) const {
    for (uint8_t i = 0; i < n_; i++) {
      if (pages_[i] == page) return i;
    }
    return -1;
  }

  // Turn to the page after the anchor (the first enabled page if the anchor was removed).
  bool advance(uint32_t nowMs) {
    int at = hasCur_ ? indexOf(anchor_) : -1;
    PageId target = pages_[(at + 1) % n_];
    since_ = nowMs;  // restart even when there is nowhere to turn
    if (hasCur_ && target == cur_) return false;
    enter(target, nowMs, true);
    return true;
  }

  void enter(PageId page, uint32_t nowMs, bool moveAnchor) {
    uint8_t i = static_cast<uint8_t>(page);
    visits_[i] = (seen_ & (1u << i)) ? static_cast<uint8_t>(visits_[i] + 1) : 0;
    seen_ |= static_cast<uint8_t>(1u << i);
    cur_ = page;
    hasCur_ = true;
    since_ = nowMs;
    if (moveAnchor) anchor_ = page;
  }

  PageId pages_[limits::PAGE_COUNT] = {};
  uint8_t n_ = 0;
  PageId cur_ = PageId::Clock;
  PageId anchor_ = PageId::Clock;  // last enabled page shown; where the next turn starts from
  bool hasCur_ = false;
  uint8_t seen_ = 0;  // bit per PageId: shown at least once
  uint8_t visits_[limits::PAGE_COUNT] = {};
  uint32_t dwell_ = 1;
  uint32_t since_ = 0;
};

#include <stdio.h>

#include "PageCommon.h"

namespace pages {

void renderClock(const PageContext& ctx, Canvas& canvas) {
  begin(canvas, "Clock", nullptr);
  if (!ctx.timeSynced) {
    draw::emptyState(canvas, "Setting the clock", "Waiting for the network");
    return;
  }
  tm lt;
  localTime(ctx.now, lt);
  char date[40];
  snprintf(date, sizeof date, "%s %d %s", weekdayName(lt.tm_wday, true), lt.tm_mday, monthName(lt.tm_mon));
  draw::textCenter(canvas, Font::BodyBold, Canvas::W / 2, 37, date);

  char digits[8];
  const char* suffix = "";
  formatMinutes(use12h(ctx.feed), lt.tm_hour * 60 + lt.tm_min, digits, sizeof digits, &suffix);
  // am/pm sits beside the numerals; centre the pair as one block.
  constexpr int GAP = 6;
  int digitsW = draw::textWidth(Font::NumBig, digits);
  int suffixW = *suffix ? GAP + draw::textWidth(Font::BodyBold, suffix) : 0;
  int x = (Canvas::W - digitsW - suffixW) / 2;
  constexpr int baseline = 106;
  draw::text(canvas, Font::NumBig, x, baseline, digits);
  if (*suffix) draw::text(canvas, Font::BodyBold, x + digitsW + GAP, baseline, suffix);
}

}  // namespace pages

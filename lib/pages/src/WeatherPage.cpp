#include <stdio.h>

#include "Icons.h"
#include "PageCommon.h"
#include "Text.h"
#include "WmoText.h"

namespace pages {
namespace {
constexpr int RULE_Y = 78;

// "Weather" plus the place when it fits next to the data-age note without crowding.
void titleWithPlace(const PageContext& ctx, const char* note, char* out, size_t cap) {
  char place[limits::PLACE_MAX + 1];
  text::fold(ctx.feed.weather.place, place, sizeof place);
  snprintf(out, cap, "Weather - %s", place);
  int room = Canvas::W - 2 * MARGIN - 16 - draw::textWidth(Font::Small, note);
  if (!place[0] || draw::textWidth(Font::SmallBold, out) > room) snprintf(out, cap, "Weather");
}

void drawNow(const WeatherSnap& w, Canvas& canvas) {
  draw::icon(canvas, iconForWmo(w.code, w.isDay), MARGIN, 18, IconSize::S48);
  char t[8];
  snprintf(t, sizeof t, "%d", w.tempNow);
  constexpr int tempX = 58, tempBase = 57;
  // The font's minus is followed by a wide gap; pull the digits in under it.
  const char* digits = t[0] == '-' ? t + 1 : t;
  int digitsX = tempX;
  if (digits != t) {
    draw::text(canvas, Font::Num32, tempX, tempBase, "-");
    digitsX += draw::textWidth(Font::Num32, "-") - 3;
  }
  draw::text(canvas, Font::Num32, digitsX, tempBase, digits);
  int end = digitsX + draw::textWidth(Font::Num32, digits) + 3;
  end += draw::degree(canvas, Font::Num32, end, tempBase);

  int x = end + 10 > 136 ? end + 10 : 136;  // text column; wide temperatures push it right
  int avail = Canvas::W - MARGIN - x;
  char row[40];
  int base = 34;
  int used = lines(canvas, Font::BodyBold, x, base, avail, 14, 2, wmoText(w.code));
  base += (used - 1) * 14 + 14;
  snprintf(row, sizeof row, "feels %d\xB0  rain %u%%", w.feels, w.rainPct);
  bool joined = draw::textWidth(Font::Small, row) <= avail;
  if (!joined) snprintf(row, sizeof row, "feels %d\xB0", w.feels);
  draw::text(canvas, Font::Small, x, base, row);
  base += 12;
  snprintf(row, sizeof row, "H %d\xB0  L %d\xB0", w.hi, w.lo);
  draw::text(canvas, Font::Small, x, base, row);
  base += 12;
  if (!joined && base < RULE_Y - 2) {
    snprintf(row, sizeof row, "rain %u%%", w.rainPct);
    draw::text(canvas, Font::Small, x, base, row);
  }
}

void drawForecast(const WeatherSnap& w, Canvas& canvas) {
  constexpr int colW = 82;
  for (int i = 0; i < limits::FORECAST_DAYS; i++) {
    const WeatherDay& d = w.days[i];
    int x = MARGIN + i * colW;
    draw::text(canvas, Font::BodyBold, x, 96, weekdayName(d.wday, false));
    draw::icon(canvas, iconForWmo(d.code, true), x + 40, 81, IconSize::S24);
    char hl[24];
    snprintf(hl, sizeof hl, "%d\xB0/%d\xB0", d.hi, d.lo);
    draw::text(canvas, Font::BodyBold, x, 117, hl);
  }
}
}  // namespace

void renderWeather(const PageContext& ctx, Canvas& canvas) {
  const WeatherSnap& w = ctx.weather;
  if (!ctx.feed.weather.set) {
    begin(canvas, "Weather", nullptr);
    draw::emptyState(canvas, "Choose a place", "Open the web app to set weather");
    return;
  }
  char note[32];
  dataNote(ctx.feed, w.status, ctx.now, note, sizeof note);
  char title[limits::PLACE_MAX + 16];
  titleWithPlace(ctx, note, title, sizeof title);
  begin(canvas, title, note);
  if (fetchEmptyState(canvas, w.status, "Weather is loading", "Weather didn't load")) return;
  drawNow(w, canvas);
  draw::hairline(canvas, MARGIN, RULE_Y, BODY_W);
  drawForecast(w, canvas);
}

}  // namespace pages

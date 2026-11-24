#include "Pages.h"

#include "PageCommon.h"

namespace pages {

const char* title(PageId page) {
  switch (page) {
    case PageId::Clock: return "Clock";
    case PageId::Weather: return "Weather";
    case PageId::Hn: return "Hacker News";
    case PageId::Messages: return "Note to self";
    case PageId::Calendar: return "Today";
    case PageId::Notes: return "Notes";
  }
  return "";
}

bool usesClock(PageId page) { return page == PageId::Clock; }

void render(PageId page, const PageContext& ctx, Canvas& canvas) {
  switch (page) {
    case PageId::Clock: renderClock(ctx, canvas); break;
    case PageId::Weather: renderWeather(ctx, canvas); break;
    case PageId::Hn: renderHn(ctx, canvas); break;
    case PageId::Messages: renderMessages(ctx, canvas); break;
    case PageId::Calendar: renderCalendar(ctx, canvas); break;
    case PageId::Notes: renderNotes(ctx, canvas); break;
  }
}

}  // namespace pages

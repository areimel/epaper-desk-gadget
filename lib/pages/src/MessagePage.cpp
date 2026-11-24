#include <stdio.h>

#include "PageCommon.h"
#include "Text.h"

namespace pages {

void renderMessages(const PageContext& ctx, Canvas& canvas) {
  begin(canvas, "Note to self", nullptr);
  const FeedConfig& feed = ctx.feed;
  if (feed.messageCount == 0) {
    draw::emptyState(canvas, "No messages yet", "Add some in the web app");
    return;
  }
  const Message& m = feed.messages[ctx.visit % feed.messageCount];
  char body[limits::MESSAGE_TEXT_MAX + 1];
  char by[limits::MESSAGE_BY_MAX + 8];
  text::fold(m.text, body, sizeof body);
  by[0] = '\0';
  if (m.by[0]) {
    char who[limits::MESSAGE_BY_MAX + 1];
    text::fold(m.by, who, sizeof who);
    snprintf(by, sizeof by, "- %s", who);
  }
  static const Font ladder[] = {Font::Quote18, Font::Quote14, Font::BodyBold};
  const int bottom = Canvas::H - 4;
  int boxBottom = bottom;
  if (by[0]) {
    draw::textRight(canvas, Font::Small, Canvas::W - MARGIN, bottom - 2, by);
    boxBottom = bottom - 2 - 8 - 4;  // above the attribution's cap height
  }
  draw::fitted(canvas, ladder, 3, {MARGIN, BODY_TOP + 2, BODY_W, boxBottom - BODY_TOP - 2}, body);
}

}  // namespace pages

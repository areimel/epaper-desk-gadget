#include "PageCommon.h"

namespace pages {

void renderSetup(SetupState state, const char* detail, Canvas& canvas) {
  const char* head = "Connect me to WiFi";
  if (state == SetupState::Connecting) head = "Joining WiFi";
  if (state == SetupState::NoPages) head = "All pages are switched off";
  begin(canvas, "Desk Gadget", nullptr);
  bool hasDetail = detail && *detail;
  int headBase = hasDetail ? 50 : 72;
  draw::textCenter(canvas, Font::BodyBold, Canvas::W / 2, headBase, head);
  if (hasDetail) lines(canvas, Font::Small, MARGIN, headBase + 22, BODY_W, 13, 3, detail, true);
}

}  // namespace pages

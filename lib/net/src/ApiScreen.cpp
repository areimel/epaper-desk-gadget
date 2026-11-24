#include "Api.h"

#if FEATURE_DASHBOARD
#include "ApiInternal.h"

// GET /api/screen[?page=<id>]: Canvas::PACKED_BYTES of 1-bit rows. Canvas storage already is the
// wire format, so its rows() are sent directly without a copy.
namespace net {
namespace api {

void handleScreen() {
  static const uint8_t blank[Canvas::PACKED_BYTES] = {};  // in flash: .rodata
  const ApiHooks& h = hooks();
  WebServer& srv = server();
  const Canvas* canvas = nullptr;
  if (srv.hasArg("page")) {
    PageId page;
    if (!feed::pageFromName(srv.arg("page").c_str(), page)) {
      sendError(404, "There is no page with that id.", "page");
      return;
    }
    // About 4.3 KB: static, never on the loop stack. Rendered off-panel; the panel is untouched.
    static Canvas scratch;
    scratch.clear();
    if (h.renderPage) h.renderPage(page, scratch);
    canvas = &scratch;
  } else if (h.liveCanvas) {
    canvas = h.liveCanvas();
  }
  sendBytes(200, "application/octet-stream", canvas ? canvas->rows() : blank, Canvas::PACKED_BYTES);
}

}  // namespace api
}  // namespace net
#endif

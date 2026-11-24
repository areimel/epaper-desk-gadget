#include "Api.h"

#if FEATURE_DASHBOARD
#include <stdio.h>
#include <string.h>

#include "ApiInternal.h"

// The web app, embedded by scripts/embed_ui.py into $BUILD_DIR/generated/ui_files.h. Without the
// script (or with an empty device-ui/) the table is empty and "/" serves a short built-in page.
#if __has_include("ui_files.h")
#include "ui_files.h"
#else
#include <stdint.h>
struct UiFile {
  const char* path;
  const char* mime;
  const uint8_t* data;
  size_t len;
  bool gzip;
};
static const UiFile UI_FILES[1] = {{nullptr, nullptr, nullptr, 0, false}};
static const size_t UI_FILE_COUNT = 0;
static const char UI_ETAG[] = "none";
#endif

namespace {
constexpr char FALLBACK_PAGE[] =
    "<!doctype html><meta charset=utf-8><meta name=viewport content=\"width=device-width\">"
    "<title>Desk Gadget</title><p>The web app is not included in this build.</p>"
    "<p>The API is at <a href=\"/api/state\">/api/state</a>.</p>";

const UiFile* find(const char* path) {
  for (size_t i = 0; i < UI_FILE_COUNT; i++) {
    if (strcmp(UI_FILES[i].path, path) == 0) return &UI_FILES[i];
  }
  return nullptr;
}
}  // namespace

namespace net {
namespace api {

// Cache policy: "no-cache" plus an ETag from the embedded content hash. The browser revalidates
// on every load (a 304 has no body), and a firmware update with changed files changes the tag,
// so edits are never hidden by a stale cache. A max-age would save the round trips but could
// show an old app against a new API for that long.
bool serveStatic(const char* path) {
  if (strcmp(path, "/") == 0) path = "/index.html";
  WebServer& srv = server();
  const UiFile* f = find(path);
  if (!f) {
    if (UI_FILE_COUNT != 0 || strcmp(path, "/index.html") != 0) return false;
    srv.sendHeader("Cache-Control", "no-cache");
    srv.send(200, "text/html; charset=utf-8", FALLBACK_PAGE);
    return true;
  }
  char etag[40];
  snprintf(etag, sizeof etag, "\"%s-%u\"", UI_ETAG, static_cast<unsigned>(f - UI_FILES));
  srv.sendHeader("Cache-Control", "no-cache");
  srv.sendHeader("ETag", etag);
  if (srv.header("If-None-Match") == etag) {
    srv.send(304);
    return true;
  }
  if (f->gzip) srv.sendHeader("Content-Encoding", "gzip");
  srv.setContentLength(f->len);
  srv.send(200, f->mime, "");
  // The arrays are const, so they stay in memory-mapped flash; sendContent reads them in place.
  if (srv.method() != HTTP_HEAD) srv.sendContent(reinterpret_cast<const char*>(f->data), f->len);
  return true;
}

}  // namespace api
}  // namespace net
#endif

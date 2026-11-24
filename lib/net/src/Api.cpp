#include "Api.h"

#if FEATURE_DASHBOARD
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <uri/UriGlob.h>
#include <stdio.h>
#include <stdlib.h>

#include "ApiInternal.h"
#include "FeedFile.h"
#include "FeedJson.h"
#include "Fetcher.h"
#include "Log.h"
#include "Net.h"

// Request bodies arrive through WebServer's raw-handler path (HTTP_RAW_BUFLEN chunks), not
// server.arg("plain"): that path reads any Content-Length into a growing realloc buffer and
// then copies it into a String (twice the body, unbounded). Here the size is checked against a
// per-route cap at RAW_START, before anything is buffered.
namespace {
WebServer srv(80);
net::ApiHooks hk = {};
const char* host = "";
const char* name = "";
bool started = false;
char url[24] = "";

constexpr size_t SHOW_BODY_MAX = 128;
constexpr size_t ABORT_BODY_OVER = 4 * limits::FEED_JSON_MAX;  // drop the connection, do not drain

struct Body {
  char* buf = nullptr;
  size_t len = 0;
  size_t cap = 0;  // this route's limit, for the 413 message
  bool tooLarge = false;
  bool noMemory = false;
} body;

void freeBody() {
  free(body.buf);
  body = Body{};
}

void collect(size_t cap) {
  HTTPRaw& raw = srv.raw();
  if (raw.status == RAW_START) {
    freeBody();
    const int declared = srv.clientContentLength();
    body.cap = cap;
    body.tooLarge = declared < 0 || static_cast<size_t>(declared) > cap;
    if (body.tooLarge && static_cast<size_t>(declared) > ABORT_BODY_OVER) {
      net::api::sendError(413, "The request body is too large.");
      srv.client().stop();  // reading megabytes would stall the loop; the reply may be lost
      return;
    }
    if (!body.tooLarge) {
      body.buf = static_cast<char*>(malloc(declared + 1));
      body.noMemory = body.buf == nullptr;
    }
  } else if (raw.status == RAW_WRITE) {
    if (!body.buf || body.len + raw.currentSize > static_cast<size_t>(srv.clientContentLength())) return;
    memcpy(body.buf + body.len, raw.buf, raw.currentSize);
    body.len += raw.currentSize;
  } else if (raw.status == RAW_END) {
    if (body.buf) body.buf[body.len] = 0;
  } else {
    freeBody();  // RAW_ABORTED: no handler call follows
  }
}

// Answers 413/500 for a body that could not be collected; true if the handler may go on.
bool bodyOk() {
  if (body.tooLarge) {
    char msg[64];
    snprintf(msg, sizeof msg, "The request is larger than %u bytes.", static_cast<unsigned>(body.cap));
    net::api::sendError(413, msg);
  } else if (body.noMemory) {
    net::api::sendError(500, "The device is out of memory. Try again.");
  } else {
    return true;
  }
  freeBody();
  return false;
}

void sendFeed() {
  char* out = static_cast<char*>(malloc(limits::FEED_JSON_MAX + 1));
  const size_t n = out && hk.feed ? feed::toJson(*hk.feed, true, out, limits::FEED_JSON_MAX + 1) : 0;
  if (n) net::api::sendJson(200, out, n);
  else net::api::sendError(500, "The device is out of memory. Try again.");
  free(out);
}

void putFeed() {
  if (!bodyOk()) return;
  // About 8 KB: static, never on the loop stack. Holds the candidate until it is stored.
  static FeedConfig scratch;
  if (!hk.feed) {
    freeBody();
    net::api::sendError(500, "The feed is not available.");
    return;
  }
  const feed::Result r = feed::fromJson(body.buf ? body.buf : "", body.len, *hk.feed, scratch);
  freeBody();  // before save() and the reply allocate their buffers
  if (!r.ok) {
    net::api::sendError(400, r.error, r.path);
    return;
  }
  if (!feedfile::save(scratch)) {
    net::api::sendError(500, "The feed could not be saved. The previous feed is still in use.");
    return;
  }
  memcpy(hk.feed, &scratch, sizeof scratch);
  net::onFeedChanged(*hk.feed);  // affected sources refetch, the clock takes the new zone
  if (hk.onFeedSaved) hk.onFeedSaved();
  sendFeed();
}

void show() {
  if (!bodyOk()) return;
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body.buf ? body.buf : "", body.len);
  freeBody();
  const char* id = doc["page"].is<const char*>() ? doc["page"].as<const char*>() : nullptr;
  if (err || !id) {
    net::api::sendError(400, err ? "The request is not valid JSON." : "Page is missing.", "page");
    return;
  }
  PageId page;
  if (!feed::pageFromName(id, page)) {
    net::api::sendError(404, "There is no page with that id.", "page");
    return;
  }
  if (!hk.showPage || !hk.showPage(page)) {
    net::api::sendError(500, "The panel could not show that page.", "page");
    return;
  }
  char out[48];
  const int n = snprintf(out, sizeof out, "{\"page\":\"%s\"}", feed::pageName(page));
  net::api::sendJson(200, out, static_cast<size_t>(n));
}

void refresh() {
  freeBody();  // any body is ignored
  net::requestRefresh();
  net::api::sendJson(202, "{}", 2);
}

void notFound() {
  freeBody();
  const String& uri = srv.uri();
  const bool get = srv.method() == HTTP_GET || srv.method() == HTTP_HEAD;
  if (get && !uri.startsWith("/api/") && net::api::serveStatic(uri.c_str())) return;
  net::api::sendError(404, "Not found.");
}

void startServer() {
  static const char* headers[] = {"If-None-Match"};
  srv.collectHeaders(headers, 1);
  srv.on("/api/state", HTTP_GET, net::api::handleState);
  srv.on("/api/feed", HTTP_GET, sendFeed);
  srv.on("/api/feed", HTTP_PUT, putFeed, [] { collect(limits::FEED_JSON_MAX); });
  srv.on("/api/screen", HTTP_GET, net::api::handleScreen);
  srv.on("/api/show", HTTP_POST, show, [] { collect(SHOW_BODY_MAX); });
  srv.on("/api/refresh", HTTP_POST, refresh, [] { collect(0); });
  // Catch-all, registered last: without a raw handler the server would buffer a body of any
  // declared size for an unknown route or method. Here it is never kept.
  srv.on(UriGlob("/*"), HTTP_ANY, notFound, [] { collect(0); });
  srv.onNotFound(notFound);
  srv.begin();
  // With OTA compiled in, ArduinoOTA.begin() already started mDNS under the same hostname and
  // MDNS.begin() fails as "already initialised"; the responder is up either way, so the
  // service is added regardless.
  const bool own = MDNS.begin(host);
  const bool svc = MDNS.addService("http", "tcp", 80);
  LOG_I("api: listening on %s (mDNS %s.local%s%s)", url, host, own ? "" : ", shared with OTA",
        svc ? "" : ", service not added");
  started = true;
}
}  // namespace

namespace net {
namespace api {
WebServer& server() { return srv; }
const ApiHooks& hooks() { return hk; }
const char* hostname() { return host; }
const char* deviceName() { return name; }

void sendBytes(int code, const char* type, const uint8_t* data, size_t n) {
  srv.sendHeader("Cache-Control", "no-store");
  srv.setContentLength(n);
  srv.send(code, type, "");
  if (n && srv.method() != HTTP_HEAD) srv.sendContent(reinterpret_cast<const char*>(data), n);
}

void sendJson(int code, const char* json, size_t n) {
  sendBytes(code, "application/json", reinterpret_cast<const uint8_t*>(json), n);
}

void sendError(int code, const char* error, const char* path) {
  JsonDocument doc;
  doc["error"] = error ? error : "";
  doc["path"] = path ? path : "";
  char out[448];  // path (47) and error (63) with every byte escaped as \\uXXXX still fit
  const size_t n = serializeJson(doc, out, sizeof out);
  sendJson(code, out, n);
}
}  // namespace api

void beginApi(const char* hostname, const char* deviceName, const ApiHooks& hooks) {
  host = hostname ? hostname : "";
  name = deviceName ? deviceName : "";
  hk = hooks;
}

void updateApi() {
  const char* ip = ipAddress();
  if (ip[0]) snprintf(url, sizeof url, "http://%s/", ip);
  else url[0] = 0;
  if (!url[0]) return;  // the server starts once, at the first connection; it survives reconnects
  if (!started) startServer();
  srv.handleClient();
}

const char* deviceUrl() { return url; }

}  // namespace net
#endif

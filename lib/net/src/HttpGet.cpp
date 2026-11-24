#include "HttpGet.h"

#if FEATURE_DASHBOARD
#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <stdio.h>

namespace {
constexpr uint32_t TIMEOUT_MS = 10000;     // connect, TLS handshake and each read, separately
constexpr size_t TLS_HEAP_NEEDED = 40000;  // largest free block wanted before a handshake

// A Stream that keeps at most cap - 1 bytes. A longer body makes write() come up short, which
// makes HTTPClient::writeToStream stop with HTTPC_ERROR_STREAM_WRITE; `overflow` tells why.
class BoundedSink : public Stream {
 public:
  BoundedSink(char* buf, size_t cap) : buf_(buf), cap_(cap) {}
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t n) override {
    const size_t room = cap_ - 1 - len_;
    const size_t take = n < room ? n : room;
    memcpy(buf_ + len_, data, take);
    len_ += take;
    if (take < n) overflow = true;
    return take;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  size_t length() const { return len_; }
  bool overflow = false;

 private:
  char* buf_;
  size_t cap_;
  size_t len_ = 0;
};

void fail(net::GetResult& r, const char* reason) { snprintf(r.error, sizeof r.error, "%s", reason); }

// HTTPClient's negative codes as short reasons. A refused connect after a TLS error (bad or
// unverifiable certificate, handshake timeout) is reported as "TLS error".
void failCode(net::GetResult& r, int code, NetworkClientSecure& client) {
  char tls[8];
  switch (code) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
      fail(r, client.lastError(tls, sizeof tls) != 0 ? "TLS error" : "connect failed");
      break;
    case HTTPC_ERROR_READ_TIMEOUT: fail(r, "timeout"); break;
    case HTTPC_ERROR_CONNECTION_LOST: fail(r, "connection lost"); break;
    case HTTPC_ERROR_TOO_LESS_RAM: fail(r, "no memory"); break;
    default: snprintf(r.error, sizeof r.error, "HTTP error %d", code);
  }
}
}  // namespace

namespace net {

GetResult httpsGet(const char* url, char* buf, size_t cap) {
  GetResult r{};
  if (!buf || cap < 2) {
    fail(r, "no memory");
    return r;
  }
  if (ESP.getMaxAllocHeap() < TLS_HEAP_NEEDED) {
    fail(r, "no memory");
    return r;
  }
  // Both objects live only for this call, so the TLS buffers (about 40 KB) are freed on return.
  NetworkClientSecure client;
  client.useBuiltinCACertBundle();  // verify against the IDF bundle; never setInsecure()
  client.setHandshakeTimeout(TIMEOUT_MS / 1000);  // seconds
  HTTPClient http;
  http.setConnectTimeout(TIMEOUT_MS);
  http.setTimeout(TIMEOUT_MS);
  http.setReuse(false);  // Connection: close; one request per session
  http.setUserAgent("desk-gadget");
  if (!http.begin(client, url)) {
    fail(r, "bad URL");
    return r;
  }
  r.httpCode = http.GET();
  if (r.httpCode < 0) {
    failCode(r, r.httpCode, client);
  } else if (r.httpCode != HTTP_CODE_OK) {
    snprintf(r.error, sizeof r.error, "HTTP %d", r.httpCode);
  } else if (http.getSize() >= static_cast<int>(cap)) {
    fail(r, "too large");  // Content-Length says so; do not download it
  } else {
    // writeToStream de-chunks Transfer-Encoding: chunked and checks the length it read.
    BoundedSink sink(buf, cap);
    const int got = http.writeToStream(&sink);
    if (sink.overflow) fail(r, "too large");
    else if (got < 0) failCode(r, got, client);
    else {
      r.ok = true;
      r.len = sink.length();
      buf[r.len] = 0;
    }
  }
  http.end();
  return r;
}

}  // namespace net
#endif

#include "HttpGet.h"

#if FEATURE_DASHBOARD
#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <stdio.h>

namespace {
constexpr uint32_t TIMEOUT_MS = 10000;     // connect, TLS handshake and each read, separately
constexpr size_t TLS_HEAP_NEEDED = 40000;  // largest free block wanted before a handshake

// HTTPClient::writeToStream needs a Stream; these only ever receive. A short write makes it stop
// with HTTPC_ERROR_STREAM_WRITE, and `stopped` tells why.
class WriteOnly : public Stream {
 public:
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t n) override = 0;
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  size_t length() const { return len_; }
  bool stopped = false;

 protected:
  size_t len_ = 0;
};

// Keeps at most cap - 1 bytes.
class BoundedSink : public WriteOnly {
 public:
  BoundedSink(char* buf, size_t cap) : buf_(buf), cap_(cap) {}
  using WriteOnly::write;
  size_t write(const uint8_t* data, size_t n) override {
    const size_t room = cap_ - 1 - len_;
    const size_t take = n < room ? n : room;
    memcpy(buf_ + len_, data, take);
    len_ += take;
    if (take < n) stopped = true;
    return take;
  }

 private:
  char* buf_;
  size_t cap_;
};

// Passes everything on to a ByteSink.
class ForwardSink : public WriteOnly {
 public:
  explicit ForwardSink(ByteSink& sink) : sink_(sink) {}
  using WriteOnly::write;
  size_t write(const uint8_t* data, size_t n) override {
    if (stopped || !sink_.write(data, n)) {
      stopped = true;
      return 0;
    }
    len_ += n;
    return n;
  }

 private:
  ByteSink& sink_;
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

// One GET into `out`. cap > 0: refuse a declared length of cap or more before downloading.
net::GetResult request(const char* url, WriteOnly& out, size_t cap, const char* stopReason, bool redirects) {
  net::GetResult r{};
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
  if (redirects) http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) {
    fail(r, "bad URL");
    return r;
  }
  r.httpCode = http.GET();
  if (r.httpCode < 0) {
    failCode(r, r.httpCode, client);
  } else if (r.httpCode != HTTP_CODE_OK) {
    snprintf(r.error, sizeof r.error, "HTTP %d", r.httpCode);
  } else if (cap && http.getSize() >= static_cast<int>(cap)) {
    fail(r, stopReason);  // Content-Length says so; do not download it
  } else {
    // writeToStream de-chunks Transfer-Encoding: chunked and checks the length it read.
    const int got = http.writeToStream(&out);
    if (out.stopped) fail(r, stopReason);
    else if (got < 0) failCode(r, got, client);
    else r.ok = true;
    r.len = out.length();
  }
  http.end();
  return r;
}
}  // namespace

namespace net {

GetResult httpsGet(const char* url, char* buf, size_t cap) {
  if (!buf || cap < 2) {
    GetResult r{};
    fail(r, "no memory");
    return r;
  }
  BoundedSink sink(buf, cap);
  GetResult r = request(url, sink, cap, "too large", false);
  if (r.ok) buf[r.len] = 0;
  return r;
}

GetResult httpsGetTo(const char* url, ByteSink& sink) {
  ForwardSink forward(sink);
  return request(url, forward, 0, "bad data", true);
}

}  // namespace net
#endif

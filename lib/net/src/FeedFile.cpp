#include "FeedFile.h"

#if FEATURE_DASHBOARD
#include <Arduino.h>
#include <LittleFS.h>
#include <stdlib.h>

#include "FeedJson.h"
#include "Log.h"

namespace {
constexpr const char* FEED = "/feed.json";
constexpr const char* TMP = "/feed.tmp";
constexpr const char* BAD = "/feed.bad";
constexpr size_t FILE_MAX = limits::FEED_JSON_MAX + limits::URL_MAX;  // storage shape bound
bool mounted = false;

// A save that renamed /feed.json away but lost power before the new file took its place leaves
// only /feed.tmp; it is used (and validated) like /feed.json.
void recoverTmp() {
  if (!LittleFS.exists(FEED) && LittleFS.exists(TMP) && LittleFS.rename(TMP, FEED))
    LOG_W("feedfile: recovered %s from %s", FEED, TMP);
}

void quarantine(const char* why) {
  LittleFS.remove(BAD);
  const bool kept = LittleFS.rename(FEED, BAD);
  LOG_W("feedfile: %s is %s; %s", FEED, why, kept ? "kept as /feed.bad" : "could not rename it");
  if (!kept) LittleFS.remove(FEED);  // do not trip over the same file at every boot
}

// Reads the whole file into a fresh heap buffer (NUL-terminated). nullptr on any failure.
char* readAll(size_t& n, const char*& why) {
  File f = LittleFS.open(FEED, "r");
  if (!f) {
    why = "unreadable";
    return nullptr;
  }
  const size_t size = f.size();
  if (size == 0 || size > FILE_MAX) {
    why = size ? "too large" : "empty";
    return nullptr;
  }
  char* buf = static_cast<char*>(malloc(size + 1));
  if (!buf) {
    why = nullptr;  // out of memory is not the file's fault: keep it
    return nullptr;
  }
  n = f.read(reinterpret_cast<uint8_t*>(buf), size);
  if (n != size) {
    free(buf);
    why = "short";
    return nullptr;
  }
  buf[n] = 0;
  return buf;
}
}  // namespace

namespace feedfile {

bool begin() {
  // basePath, maxOpenFiles, partition label: the default 8 MB table names the data partition
  // "spiffs"; formatOnFail formats it once when it holds no LittleFS yet.
  mounted = LittleFS.begin(true, "/littlefs", 5, "spiffs");
  if (mounted) {
    LOG_I("feedfile: %u of %u bytes used", static_cast<unsigned>(LittleFS.usedBytes()),
          static_cast<unsigned>(LittleFS.totalBytes()));
  } else {
    LOG_E("feedfile: LittleFS mount failed");
  }
  return mounted;
}

bool load(FeedConfig& out) {
  if (!mounted) return false;
  recoverTmp();
  if (!LittleFS.exists(FEED)) {
    LOG_I("feedfile: no %s", FEED);
    return false;
  }
  size_t n = 0;
  const char* why = nullptr;
  char* buf = readAll(n, why);
  if (!buf) {
    if (why) quarantine(why);
    else LOG_E("feedfile: no memory to read %s", FEED);
    return false;
  }
  // The stored shape carries its own calendar link, so `out` serves as `current` too;
  // fromJson only writes `out` after the whole document validated.
  const feed::Result r = feed::fromJson(buf, n, out, out);
  free(buf);
  if (!r.ok) {
    // Path and reason only: never the value, which may be the calendar link.
    LOG_W("feedfile: invalid at '%s': %s", r.path, r.error);
    quarantine("invalid");
    return false;
  }
  LOG_I("feedfile: loaded %u bytes", static_cast<unsigned>(n));
  return true;
}

bool save(const FeedConfig& feed) {
  if (!mounted) return false;
  char* buf = static_cast<char*>(malloc(FILE_MAX + 1));
  if (!buf) {
    LOG_E("feedfile: no memory to save");
    return false;
  }
  const size_t n = feed::toJson(feed, false, buf, FILE_MAX + 1);
  bool ok = n > 0;
  if (ok) {
    File f = LittleFS.open(TMP, "w");
    ok = f && f.write(reinterpret_cast<const uint8_t*>(buf), n) == n;
    if (f) f.close();
    ok = ok && LittleFS.open(TMP, "r").size() == n;  // the bytes reached flash
  }
  free(buf);
  // LittleFS's rename replaces an existing target; if this VFS refuses, fall back to remove +
  // rename (recoverTmp() covers a power cut between the two).
  if (ok && !LittleFS.rename(TMP, FEED)) ok = LittleFS.remove(FEED) && LittleFS.rename(TMP, FEED);
  if (!ok) {
    // Keep /feed.tmp only in the unlikely case that it is now the sole copy (fallback rename
    // failed after the remove); recoverTmp() adopts it at the next boot.
    if (LittleFS.exists(FEED)) LittleFS.remove(TMP);
    LOG_E("feedfile: save failed (%u bytes)", static_cast<unsigned>(n));
    return false;
  }
  LOG_I("feedfile: saved %u bytes", static_cast<unsigned>(n));
  return true;
}

}  // namespace feedfile
#endif

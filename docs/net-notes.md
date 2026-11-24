# Network and storage notes (Team C)

Design notes for the integrator: how `lib/net`'s dashboard modules fit together, what they
assume, and what is compiled but not yet seen working on hardware. The API contract is
`docs/api.md`; this file does not restate it.

## Modules

| File | Role | Thread |
|---|---|---|
| `FetchPolicy.h` | pure: intervals, backoff 1/2/5/15 min, `nextState`, `offlineState`, `record` | fetch task |
| `Clock.*` | `beginClock`, `setTimezone`, `timeSynced` (epoch >= 2024-01-01) | loop (`timeSynced`: any) |
| `HttpGet.*` | one HTTPS GET into a bounded buffer | fetch task |
| `Fetcher.*` | the task, source table, mutex-guarded snapshots | task + loop |
| `FeedFile.*` | `/feed.json` on LittleFS (`spiffs` partition) | loop |
| `Api.*`, `ApiState.cpp`, `ApiScreen.cpp`, `StaticFiles.cpp`, `ApiInternal.h` | WebServer routes, mDNS, web app | loop |
| `StateJson.h` | pure: the `/api/state` document from a plain struct | loop |

Every new `.cpp` is wrapped in `#if FEATURE_DASHBOARD`. Pure headers (`FetchPolicy.h`,
`StateJson.h`) are tested on native by including them by relative path
(`#include "../../lib/net/src/FetchPolicy.h"`), because the native env has
`lib_ignore = board, net, store`. The test includes `Snapshots.h` / `Canvas.h` first so the
library finder pulls in `lib/sources` / `lib/epaper`.

## Wiring in `main.cpp` (order matters)

```cpp
// setup(), after board::begin() and the panel:
feed::setDefaults(feed);                 // static FeedConfig; then starter messages etc.
if (feedfile::begin() && !feedfile::load(feed)) feedfile::save(feed);  // first boot writes defaults
net::beginWifi(...); net::beginImprov(...);
net::beginOta(...);                      // if FEATURE_OTA (it may start mDNS first; handled)
net::beginClock(feed.tzPosix);
net::onFeedChanged(feed);                // weather URL + zone to the task; also calls setTimezone
net::beginFetcher();
net::beginApi(AppConfig::WIFI_HOSTNAME, AppConfig::PROJECT_NAME, hooks);
// loop():
net::updateOta(); net::updateWifi(now); net::updateImprov();
net::updateApi();                        // starts the server at the first connection
if (net::copyWeather(w) | net::copyHn(h) | net::copyCalendar(c)) /* re-render */;  // `|`, not `||`
// button hold -> net::requestRefresh()
```

After a successful `PUT /api/feed` the API itself copies the new feed into `*hooks.feed`,
calls `net::onFeedChanged(*hooks.feed)` and then `hooks.onFeedSaved()`; the hook only needs to
re-apply rotation and re-render. `onFeedSaved`, `renderPage`, `showPage` run inside
`updateApi()`, on the loop thread. Any hook may be nullptr.

## platformio.ini lines the integrator must add

```ini
[env]
extra_scripts =
    pre:scripts/platformio_config.py
    pre:scripts/embed_ui.py
```

Nothing else is required. Without the line the firmware still builds and `/` serves a short
built-in page (`StaticFiles.cpp` uses `__has_include("ui_files.h")`). `embed_ui.py` is a no-op on
native. No `lib_ignore` change: native tests reach the pure headers by path.

**Integration blocker found (not Team C's file):** `lib/sources/src/WeatherParse.cpp` declares
`static const char HEX[]`, which collides with Arduino's `#define HEX 16` (ArduinoJson includes
`Arduino.h` on the device), so the firmware fails to compile as soon as anything includes
`lib/sources`. Rename the local (e.g. `HEX_DIGITS`). The link check below was done with a
temporary, uncommitted `platformio_override.ini` that set `ARDUINOJSON_ENABLE_ARDUINO_STRING/
STREAM/PRINT/PROGMEM=0` instead.

## Fetch task

- `xTaskCreatePinnedToCore(..., 12 KB stack, priority 1, core 0)`. The loop task runs on core 1.
  12 KB: an mbedTLS handshake verifying against the certificate bundle uses roughly 6-8 KB of the
  caller's stack, plus HTTPClient and ArduinoJson frames. Each fetch log line prints
  `stack min` (`uxTaskGetStackHighWaterMark`, bytes on ESP-IDF) so this can be tuned on hardware.
- Wakes every 5 s, or at once on `requestRefresh`/`onFeedChanged` (task notification).
- No fetch while WiFi is down or before `timeSynced()`. A due source with WiFi down turns `Ok`
  into `Stale` "no network" (Never and Error are kept); it stays due, so it is fetched as soon as
  WiFi is back. A source with no settings (no weather place) stays `Never` with an empty error.
- A changed weather URL (place, units or zone) clears the weather snapshot to `Never` and starts
  a fresh backoff ladder; a fetch that was in flight with the old URL is discarded.
- Errors: `HTTP <code>`, `too large`, `timeout`, `TLS error`, `connect failed`,
  `connection lost`, `no memory`, `bad data`, `bad URL`, `no network`, `HTTP error <n>`.
- Calendar: a row in the source table with no parser; its status is always `Never`. Fill in the
  row (parser, body handling) and copy `feed.calendarUrl` in `onFeedChanged`.
- URLs are never logged (the calendar link is a secret, the weather one holds the location).

## HTTPS findings (Arduino-ESP32 3.3.12)

- CA bundle: `NetworkClientSecure::useBuiltinCACertBundle()` attaches the IDF bundle compiled
  into the core (`CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL`); no symbols to declare.
  `setCACertBundle(ptr, size)` is only for a custom bundle. `setInsecure()` is not used anywhere.
- `HTTPClient::writeToStream()` de-chunks `Transfer-Encoding: chunked` and checks the total; it
  reads identity bodies with or without Content-Length. `getString()` has no size bound (it
  grows a String to whatever arrives), so it is not used. The body goes into a `BoundedSink`
  (a `Stream` that refuses bytes past the cap); a too-long body is reported as `too large`, and a
  Content-Length over the cap is refused before reading.
- Timeouts: connect 10 s, handshake 10 s, each read 10 s (separately, so a pathological server
  could take up to about 30 s; the loop thread is never blocked by it). `setReuse(false)`.
- Each request creates and destroys its client, so the TLS buffers are freed in between. A
  request is refused with `no memory` when the largest free block is under 40 KB.

## WebServer findings

- **PUT bodies:** `server.arg("plain")` reads any Content-Length into a realloc-grown buffer and
  then copies it into a String (2x body, no cap). The API instead registers raw handlers
  (`server.on(uri, method, fn, rawFn)`): the body arrives in 1436-byte chunks, the declared
  Content-Length is checked at `RAW_START` against the route's cap (`FEED_JSON_MAX`, 128 bytes
  for `/api/show`) and only a body within the cap is copied into one exact-size heap buffer. An
  over-cap body is drained without buffering and answered 413; over 64 KB the 413 is sent at
  once and the connection closed (the browser may see a reset instead of the 413). A chunked
  request body (no Content-Length) is read as empty and fails as "not valid JSON" (400).
- **Connections:** the core's `WebServer(80)` listens with backlog 4 (`NetworkServer` default
  `max_clients`; lwIP has `TCP_LISTEN_BACKLOG 1`, accept mailbox 6). It serves one connection at
  a time and always answers `Connection: close`, so there is no keep-alive. With 6 simultaneous
  browser connections, up to 4 wait in the backlog and are served in turn; SYNs beyond that are
  dropped and the browser's TCP stack retries them (about 1 s, then 3 s later), so they arrive
  late rather than refused. A connection that opens but sends nothing (a browser pre-connect)
  holds the server for up to 5 s (`HTTP_MAX_DATA_WAIT`) before the next one is served. Lwip has
  16 sockets in total, shared with mDNS, OTA and the fetch task's TLS socket.
- **Caching:** app files get `Cache-Control: no-cache` and `ETag: "<content hash>-<index>"`; the
  hash (`UI_ETAG`, generated by `embed_ui.py` from every embedded byte) changes with any edit,
  so a firmware update is never hidden by a cached app, and a matching `If-None-Match` gets a 304
  with no body. Every `/api/*` reply has `Cache-Control: no-store`. No CORS headers (same origin).
  Gzipped entries are sent with `Content-Encoding: gzip` regardless of `Accept-Encoding` (every
  browser accepts gzip).
- **Re-entrancy:** handlers run inside `updateApi()` on the loop thread, one at a time; the
  static scratch `FeedConfig` (PUT), `Canvas` (`?page=`) and state buffer rely on that, as does
  `feed::fromJson`'s own static. `renderPage` must not call back into the API.
- **mDNS:** with OTA compiled in, `ArduinoOTA.begin()` starts mDNS first and `MDNS.begin()`
  fails ("already initialised"); the API logs that and adds `_http._tcp` either way.

## Memory (measured build sizes, wireless-paper)

| Build | RAM (static) | Flash |
|---|---|---|
| `main.cpp` as committed (nothing calls Team C code) | 60,168 B | 1,030,437 B |
| temporary wiring calling every public function, empty web app | 99,360 B | 1,315,757 B |
| same, with the 16-file web app from `desk-gadget` embedded (59,757 B) | 99,360 B | 1,376,433 B |

The embedded app lives in flash (RAM unchanged). Of the +39 KB static RAM, Team C owns about
17 KB: PUT scratch `FeedConfig` ~7.6 KB, `?page=` scratch `Canvas` ~4.3 KB, state buffer 1 KB,
source URLs 1.2 KB, published + scratch snapshots ~2.3 KB; `feed::fromJson`'s scratch adds
~7.6 KB and the temporary main's own feed/canvas/snapshots ~13 KB; the rest is core libraries.

Heap, worst case (fetch during a feed save): fetch task stack 12 KB (permanent) + TLS session
~40-45 KB (`MBEDTLS_SSL_MAX_CONTENT_LEN` 16 KB in and out, no dynamic buffers) + body 4/12 KB +
HTTPClient 1.4 KB; on the loop thread a PUT holds body <= 16 KB + ArduinoJson ~14 KB during
parse, then (body freed) `save()` 16.6 KB, then the 16 KB reply buffer. Peak about 90 KB
transient on top of static use, against roughly 250 KB free at boot. Every allocation failure
becomes an error reply (`500`) or a failed fetch (`no memory`), never a crash.

## Proposed /api/state additions (not implemented; integrator decides)

The web team asked for `limits.hnPerVisit`, `setup` (bool) and `refreshing` (bool). The frozen
contract is implemented as written; `StateJson.h` marks where they go (`hnPerVisit` is one line
from `limits::HN_PER_VISIT`; the two bools need two `StateInfo` fields and two hooks).

## Compiled but not verified on hardware (most likely wrong first)

1. TLS against Open-Meteo/Algolia with the built-in bundle, the 12 KB stack (watch `stack min`),
   the 40 KB heap gate, and error mapping (`TLS error` vs `connect failed`).
2. WebServer behaviour under the web app's parallel load (backlog, pre-connect stalls), raw PUT
   bodies and the >64 KB early 413.
3. LittleFS mount/format of the `spiffs` partition, rename-over-existing, `/feed.bad`.
4. mDNS together with ArduinoOTA; `desk-gadget.local` resolving.
5. Improv "Visit device" URL in the installer; NTP sync time; `onFeedChanged` zone switch.
6. `/api/screen` bytes matching the panel (depends on `Canvas` and the hooks).

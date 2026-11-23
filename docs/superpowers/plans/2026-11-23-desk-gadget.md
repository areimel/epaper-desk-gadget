# Desk Gadget Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the ESP32 starter template into an e-paper desk dashboard for the Heltec Wireless Paper V1.1, with a feed-editing web app served by the device.

**Architecture:** Pure, host-tested libraries (text, feed model, parsers, rotation) feed plain snapshot structs to small page views that draw into a 1-bit 250x122 canvas. A background task fetches sources; the loop thread renders, pushes the canvas to the panel and serves an HTTP API plus an embedded vanilla-JS web app. The same canvas and views compile on the host, so tests and the web app's mock server use real rendered screens.

**Tech Stack:** PlatformIO, pioarduino 55.03.312-1 (Arduino-ESP32 3.3.x), meshtastic/GxEPD2 fork (panel), U8g2 C core (canvas + fonts), ArduinoJson 7, core `WebServer`/`HTTPClient`/`LittleFS`/`ESPmDNS`, Unity (native tests), Python unittest, vanilla HTML/CSS/ES modules with `node:test`.

**Spec:** `docs/superpowers/specs/2026-11-23-desk-gadget-design.md`

This is the master plan. It fully specifies Phase 0 and the frozen contracts.
Each Phase 1 team lead writes its own task plan,
`docs/superpowers/plans/2026-11-23-desk-gadget-team-<a..e>.md`, from its charter
below and the contract headers, before its builders start.

## Global Constraints

- Every source file at or under 300 lines; anything repeated three times becomes a shared helper.
- Envs are exactly `wireless-paper` and `native`. All dependency versions pinned.
- No secrets in `config.yaml`, tests, fixtures, manifests or `index.json`. The calendar URL lives only in `feed.json` on the device and is never returned by the API.
- Every optional module's `.cpp` is wrapped in its `FEATURE_<X>` guard. Pure libraries include no Arduino headers.
- Never block waiting for USB serial. Improv must keep working (UART0 through the CP2102; no USB-CDC flags).
- The web installer must never write inside NVS; firmware parts stay separate.
- HTTPS uses the core CA bundle; `setInsecure()` is forbidden. One TLS session at a time. No fetch before NTP sync.
- Panel: full refresh on page change and at least hourly; fast refresh only for the clock minute tick, full after 5 fast. Dwell 180-3600 s, default 300.
- Text on the panel is black on white, never on dither; strokes at least 2 px.
- Flash only the CP2102 device (`10c4:ea60`, serial `0001`), re-identified with esptool before every upload. Hardware behaviour is "pending" in `TODO.md` until Alec observes it.
- Commits are local, focused, descriptive, on branch `desk-gadget`. Never push.
- Web copy: sentence case, plain verbs, "Save changes" -> "Saved", no all-caps labels, no monospace data labels.

## Review Focus

1. **Network failure mid-fetch** (WiFi drop, DNS failure, TLS error, 5xx, timeout): the previous snapshot stays with state `Stale`, the page says so, nothing hangs longer than 10 s. Test: Team C `test_fetch_policy` (backoff + state transitions with a fake transport).
2. **Hostile or odd text** (emoji, CJK, a 90-character unbroken word, empty title, control bytes): folding yields printable Latin-1, wrap never overruns the box or loops. Test: Team B `test_text_fold`, `test_text_wrap`.
3. **Bad `PUT /api/feed`** (malformed JSON, body over 16 KB, wrong types, over-limit counts, unknown page id): 400 with the field path, stored feed untouched. Test: Team B `test_feed_json`; Team C mock-request test.
4. **Missing, corrupt or old-version `feed.json` at boot**: defaults load, device does not boot-loop, corrupt file is kept as `feed.bad`. Test: Team B `test_feed_json` (version + garbage cases).
5. **Empty states** (all pages off, no messages, no notes, no location, no calendar link, zero events, one page only): each renders a helpful line; rotation with 0 or 1 pages never divides by zero or refreshes needlessly. Test: Team B `test_rotation`; golden images in Phase 2.
6. **Large or strange calendar files** (multi-MB, 10 KB description lines, DST-change day, event crossing midnight): bounded memory, correct local times. Test: Team B `test_ics_*`.

---

## Phase 0 (sequential)

### Task 0.1: Strip the template and add the Wireless Paper target

**Owner:** one Sonnet agent, reviewed by the orchestrator.

**Files:**
- Delete: `lib/ble_hid/`, `lib/display/`, `lib/board/src/BoardM5.cpp`, `src/StatusView.*`, `examples/`, `scripts/config/sections/{ble,buttons,keys,display}.py`, `scripts/tests/{test_buttons,test_macro_schema}.py`, `test/test_ble/`, `test/test_macro_*/`, `test/macro_controller_sim.h`, `test/stubs/NimBLE*.h`
- Modify: `platformio.ini`, `boards/boards.yaml`, `scripts/config/boards.py`, `scripts/config/sections/__init__.py`, `scripts/list_envs.py`, `scripts/package_firmware.py`, `scripts/tests/{support,packaging_support,test_platformio_envs,test_config_core,test_sections,test_packaging,test_packaging_secrets}.py`, `config.yaml`, `lib/board/src/Board.h`, `src/main.cpp`, `web/tests/fixtures/index.json`, `web/src/content/site.ts` (only what tests require), `.github/workflows/*.yml` (only env names), `AGENTS.md`
- Create: `lib/board/src/BoardHeltec.cpp`, `scripts/tests/fixtures/boards.yaml`

**Interfaces produced:**
- `boards.yaml` row: `wireless-paper: {label: Heltec Wireless Paper, chip: ESP32-S3, driver: heltec, button_pins: [0], led: {pin: 18, rgb: false}, power: {pin: 45, active_low: true}, epaper: {panel: fc1, mosi: 2, sck: 3, cs: 4, dc: 5, rst: 6, busy: 7}, display: true, battery: false}`
- `boards.py` emits `BOARD_DRIVER_HELTEC`, `BOARD_POWER_PIN`, `BOARD_POWER_ACTIVE_LOW`, `BOARD_EPD_{MOSI,SCK,CS,DC,RST,BUSY}`, `BOARD_EPD_PANEL_FC1` (one macro per panel value: `bn`, `fc1`, `e0213a367`). `button_count(board)` lives in `boards.py`. An unknown `driver` or `panel` raises `ConfigError` with the key path.
- `Board.h` (namespace `board`): `begin()`, `update()`, `buttonCount()`, `isPressed(i)`, `setStatusLed(bool)`, `setPeripheralPower(bool)`. No display handle, no battery.
- `platformio.ini`: `default_envs = wireless-paper`; `[env:wireless-paper]` `board = esp32-s3-devkitc-1`, `default_8MB.csv`, `flash_size = 8MB`, no PSRAM/USB-CDC flags; `build_src_filter = +<*>`; no `lib_deps` yet. `[env:native]` keeps `lib_ignore = board, net, store`.
- `config.yaml` sections: `project` (name `Desk Gadget`), `wifi` (hostname `desk-gadget`), `ota`, `gestures`. `FEATURE_DISPLAY` is gone; later tasks add `FEATURE_EPAPER`.
- `src/main.cpp`: wiring only: serial, board, WiFi + Improv, OTA, one `Gesture` on the button that logs `click`/`hold`.

- [ ] **Step 1:** Update the Python tests first for the new shape: `support.make_ctx` defaults to `wireless-paper`; packaging tests read `scripts/tests/fixtures/boards.yaml` (rows for an S3, a C3 and a classic ESP32 so the `0x1000` offset tests stay); add tests `test_unknown_driver_is_error`, `test_unknown_panel_is_error`, `test_heltec_row_emits_epd_pins` (asserts `BOARD_EPD_MOSI == 2`, `BOARD_POWER_ACTIVE_LOW == 1`, `BOARD_EPD_PANEL_FC1 == 1`).
- [ ] **Step 2:** Run `.venv/bin/python -m unittest discover -s scripts/tests -v`; the new tests fail.
- [ ] **Step 3:** Do the deletions and modifications above. Follow `docs/new-project-checklist.md`.
- [ ] **Step 4:** `.venv/bin/pio run -e wireless-paper` succeeds; `bash scripts/check.sh` passes; in `web/`: `npm ci && npm run check && npm test`.
- [ ] **Step 5:** Rewrite `AGENTS.md`: targets, the new module boundaries from the spec's code layout, and replace "no network services / runtime configuration" with "the device serves an unauthenticated HTTP API and web app on the LAN; the feed is runtime configuration stored in LittleFS". Keep every safety rule (secrets, NVS, serial, flashing, hardware claims, 300 lines).
- [ ] **Step 6:** Three commits: `Strip keyboard, M5 display and macro example from the template`; `Add Heltec Wireless Paper board, driver and env`; `Rewrite AGENTS.md for the desk gadget`.

### Task 0.2: Canvas and panel spike on real hardware

**Owner:** orchestrator (needs Alec at the panel).

**Files:**
- Create: `lib/epaper/src/Canvas.h`, `Canvas.cpp`, `Panel.h`, `Panel.cpp`, `test/test_canvas/test_main.cpp`, `src/TestCard.cpp` (temporary)
- Modify: `platformio.ini` (pin `olikraus/U8g2`, `meshtastic/GxEPD2` at a commit, `adafruit/Adafruit GFX Library`, `bblanchon/ArduinoJson`)

**Interfaces produced:**
- `Canvas`: 250x122, backed by the U8g2 C core's bitmap device (`u8g2_SetupBitmap`), so it compiles on native. `Canvas::W = 250`, `Canvas::H = 122`; `u8g2_t* g()`; `bool pixel(int x, int y) const`; `void clear()`; `size_t packRows(uint8_t* out) const` writing 122 rows x 32 bytes, MSB first, 1 = black (3904 bytes, the `/api/screen` format).
- `Panel` (`FEATURE_EPAPER`, device only): `bool begin()`; `void show(const Canvas&, Refresh mode)` with `enum class Refresh { Full, Fast }`; `uint32_t refreshCount() const`. Owns Vext power-up delay, the HSPI bus on `BOARD_EPD_*`, the landscape-to-128x250 rotation, and the 5-fast-then-full rule.

- [ ] **Step 1:** Native test `test_canvas`: draw a 1 px border and the string `Ag` in `u8g2_font_helvB10_tf` at (4, 20); assert `pixel(0,0)`, `pixel(249,121)`, `!pixel(125,60)`, `packRows` returns 3904 and byte 0 has bit 7 set. Run `.venv/bin/pio test -e native -f test_canvas`; fails (no Canvas).
- [ ] **Step 2:** Implement `Canvas`. If the U8g2 package will not build on native within the timebox (its C++ wrappers need Arduino), compile only its `csrc/` for native via a small `extra_scripts` filter. **Fallback** if that also fails: `GFXcanvas1` + `U8g2_for_Adafruit_GFX`, device-only rendering, and record that the mock server cannot be pixel-exact.
- [ ] **Step 3:** Test passes on native; `pio run -e wireless-paper` builds.
- [ ] **Step 4:** Implement `Panel` with `GxEPD2_213_FC1` and a `TestCard` (border, four corner ticks, "TOP LEFT" label, a 2 px diagonal, the build version).
- [ ] **Step 5:** Re-identify the device (`esptool chip_id` + `flash_id` on `/dev/ttyUSB0`: ESP32-S3, 8 MB), flash, show the card with a full refresh then a fast refresh that adds one line. **Alec reports what the panel shows** (orientation, cut-off columns, ghosting). Adjust offset/rotation; if wrong driver, switch `panel:` in `boards.yaml`.
- [ ] **Step 6:** Record observed behaviour in `TODO.md`. Commit: `Add 1-bit canvas and e-paper panel driver with test card`.

### Task 0.3: Freeze the shared contracts

**Owner:** orchestrator.

**Files (create):** `lib/feed/src/Limits.h`, `lib/feed/src/FeedConfig.h`, `lib/sources/src/Snapshots.h`, `lib/sources/src/ByteSink.h`, `lib/text/src/Measure.h`, `lib/epaper/src/Fonts.h`, `docs/api.md`, `device-ui/mock/fixtures/{state,feed}.json`

**Decisions these files carry:**

- `Limits.h` (namespace `limits`): `DWELL_MIN_S 180`, `DWELL_MAX_S 3600`, `DWELL_DEFAULT_S 300`, `MESSAGES_MAX 40`, `MESSAGE_TEXT_MAX 140`, `MESSAGE_BY_MAX 32`, `NOTE_LINES_MAX 5`, `NOTE_TEXT_MAX 40`, `NOTE_TITLE_MAX 24`, `URL_MAX 256`, `PLACE_MAX 32`, `TZ_MAX 48`, `FEED_JSON_MAX 16384`, `HN_STORIES 9`, `HN_PER_VISIT 3`, `HN_TITLE_MAX 96`, `CAL_EVENTS_MAX 8`, `CAL_TITLE_MAX 48`, `FORECAST_DAYS 3`, `PAGE_COUNT 6`.
- Page ids, in default order: `clock`, `weather`, `hn`, `messages`, `calendar`, `notes` (`enum class PageId : uint8_t`, plus `const char* pageName(PageId)` and `bool pageFromName(const char*, PageId&)`).
- `feed.json` shape, schema `v: 1`:
  ```json
  {"v":1,"dwellSec":300,
   "tz":{"iana":"America/New_York","posix":"EST5EDT,M3.2.0,M11.1.0"},
   "pages":[{"id":"clock","on":true},{"id":"weather","on":true}],
   "weather":{"place":"Philadelphia","lat":39.95,"lon":-75.16,"units":"f"},
   "calendar":{"url":"https://..."},
   "messages":[{"text":"You are doing better than you think.","by":"your desk"}],
   "notes":{"title":"Today","lines":[{"text":"Ship the gadget","todo":true,"done":false}]}}
  ```
  `GET /api/feed` returns `"calendar":{"set":true}` instead of the URL. On `PUT`, a missing `calendar.url` keeps the stored one; `""` clears it. `pages` must list each id exactly once.
- `FeedConfig`: a plain struct with fixed-size char arrays sized from `Limits.h`. `FeedConfig feedDefaults()`.
- `Snapshots.h`: `enum class SourceState : uint8_t { Never, Ok, Stale, Error }`; `struct SourceStatus { SourceState state; time_t updated; char error[32]; }`; `WeatherSnap` (now: `tempNow`, `feels`, `hi`, `lo` as `int16_t` whole degrees, `rainPct`, WMO `code`, `isDay`; `days[FORECAST_DAYS]` with `wday`, `hi`, `lo`, `code`); `HnSnap` (`count`, `stories[HN_STORIES]` with `title`, `points`, `comments`); `CalSnap` (`count`, `events[CAL_EVENTS_MAX]` with `title`, `startMin` (-1 = all day), `endMin`; `skipped`). Each has a `SourceStatus status`.
- `ByteSink`: `struct ByteSink { virtual bool write(const uint8_t* data, size_t n) = 0; }` (return false to abort the download). Only the ICS parser needs it; weather and HN bodies are small and parsed from a bounded buffer.
- Time: pure code takes `time_t now` as a parameter and uses libc `localtime_r`/`mktime`; native tests set `TZ` with `setenv` + `tzset`.
- `Fonts.h`: `enum class Font : uint8_t { Small, SmallBold, Body, BodyBold, Quote14, Quote18, Num32, NumBig }` mapped to `helvR08`, `helvB08`, `helvR10`, `helvB10`, `ncenB14`, `ncenB18`, `logisoso32_tn`, `logisoso42_tn`; `const uint8_t* fontData(Font)`; `int lineHeight(Font)`.
- `Measure.h`: `struct Measure { virtual int width(Font f, const char* s, size_t n) const = 0; }`. `lib/text` wraps against this; Team A provides the U8g2-backed one, Team B tests with a fixed-width fake (6 px per char).
- `docs/api.md`: the six routes from the spec with request/response JSON, status codes (`400 {"error":"...","path":"messages[3].text"}`, `413` over `FEED_JSON_MAX`), the `/api/screen` byte format, and the `/api/state` shape: `{"name","version","panel","ip","time","page","limits":{...},"sources":{"weather":{"state","updated","error"},...},"calendarSkipped","heapFree","refreshCount"}`.

- [ ] **Step 1:** Write the headers and `docs/api.md`; add a native test `test_contracts` asserting `pageFromName("hn")` round-trips for all six ids and `feedDefaults()` has six pages, all on, dwell 300.
- [ ] **Step 2:** `bash scripts/check.sh` and `pio run -e wireless-paper` pass.
- [ ] **Step 3:** Commit: `Freeze shared contracts: limits, feed shape, snapshots, API`.

After this task `platformio.ini`, `scripts/config/sections/__init__.py`, and every file listed in Task 0.3 change only through the orchestrator.

---

## Phase 1 (parallel teams, disjoint files)

Each lead (Opus) reads the spec, this plan, `AGENTS.md` and the contract
headers, writes its team plan, then dispatches builders (Sonnet) and light
helpers (Haiku). Builders work test-first and commit after each task. A team may
not edit files outside its "Owns" list; needs go to the orchestrator.

### Team A: Render

- **Owns:** `lib/epaper/src/{Draw.h,Draw.cpp,U8g2Measure.h,Icons.h,IconsData.cpp}`, `scripts/dev/make_icons.py`, `assets/icons/`, `test/test_draw/`
- **Consumes:** `Canvas`, `Fonts.h`, `Measure.h`, `lib/text` signatures below.
- **Produces:**
  - `draw::titleBar(Canvas&, const char* title, const char* right)` (14 px inverted bar, `SmallBold`)
  - `draw::text(Canvas&, Font, int x, int baselineY, const char* s)`, `textRight`, `textCenter`
  - `draw::paragraph(Canvas&, Font, Box, const char* s, int maxLines)` returning lines drawn
  - `draw::fitted(Canvas&, const Font* ladder, int n, Box, const char* s)` (largest font that fits; returns the font used)
  - `draw::icon(Canvas&, Icon, int x, int y, IconSize)` with `enum class IconSize { S24, S48 }`, `Icon iconForWmo(uint8_t code, bool isDay)`
  - `draw::checkbox(Canvas&, int x, int y, bool done)`, `draw::emptyState(Canvas&, const char* line1, const char* line2)`
  - `U8g2Measure : Measure`
- **Icons:** Weather Icons (OFL) for clear-day, clear-night, partly-cloudy, cloudy, fog, drizzle, rain, snow, thunder, unknown, at 24 and 48 px, as XBM arrays generated by `make_icons.py` (output committed, licence noted in `THIRD_PARTY_NOTICES.md`).
- **Tests:** pixel assertions on native for each helper (bar is black at y 0-13 and text is knocked out; `fitted` picks `Quote18` for a 20-char string and steps down for 140 chars; `iconForWmo` table for codes 0, 2, 3, 45, 51, 61, 71, 95, 255).

### Team B: Logic (pure, native-tested)

- **Owns:** `lib/text/src/*` (except `Measure.h`), `lib/feed/src/{FeedJson.*,Rotation.h}`, `lib/sources/src/{WeatherParse.*,HnParse.*,IcsParse.*,IcsRecur.*,WmoText.h}`, `test/test_text_*/`, `test/test_feed_json/`, `test/test_rotation/`, `test/test_weather_parse/`, `test/test_hn_parse/`, `test/test_ics_*/`, `test/fixtures/`
- **Produces:**
  - `size_t text::fold(const char* utf8, char* out, size_t cap)` (UTF-8 to Latin-1; curly quotes to straight, en/em dash to `-`, ellipsis to `...`, anything else unmappable to `?`, control bytes dropped)
  - `int text::wrap(const Measure&, Font, const char* s, int maxWidth, int maxLines, Line* out)` with `struct Line { const char* start; size_t len; bool ellipsis; }`; breaks over-long words by character
  - `void text::age(time_t now, time_t then, char* out, size_t cap)` ("just now", "12 min ago", "3 h ago", "yesterday")
  - `feed::Result feed::fromJson(const char* json, size_t n, const FeedConfig& current, FeedConfig& out)` with `struct Result { bool ok; char path[48]; char error[64]; }`; `size_t feed::toJson(const FeedConfig&, bool redact, char* out, size_t cap)`
  - `class Rotation`: `Rotation(uint32_t dwellMs)`; `void setPages(const PageId* enabled, uint8_t n)`; `bool tick(uint32_t nowMs)` (true when the page changed); `void next(uint32_t nowMs)`; `void show(PageId, uint32_t nowMs)`; `bool current(PageId&) const`; `uint8_t visit(PageId) const` (per-page visit counter, used by HN and messages to step content)
  - `bool parseWeather(const char* json, size_t n, WeatherSnap&)`; `bool parseHn(const char* json, size_t n, HnSnap&)`; `const char* wmoText(uint8_t code)`
  - `class IcsParser : public ByteSink`: `IcsParser(time_t dayStartLocal, const char* deviceTzIana)`; `void finish(CalSnap&)`
- **Fixtures:** recorded real Open-Meteo and Algolia responses; hand-made ICS files covering folded lines, a 10 KB description, all-day, multi-day, weekly BYDAY, yearly, EXDATE, a moved instance, a cancelled instance, a foreign TZID, and a DST-change day. No real calendar data in the repo.
- **Sub-teams:** text + feed (1 Sonnet), weather + HN + rotation (1 Sonnet), ICS (1 Sonnet, last to land), fixtures (Haiku).

### Team C: Network and storage

- **Owns:** `lib/net/src/{Fetcher.*,FetchPolicy.h,Clock.*,FeedFile.*,Api.*,ApiScreen.cpp,StaticFiles.*}`, `scripts/embed_ui.py`, `scripts/tests/test_embed_ui.py`, `test/test_fetch_policy/`, the Improv URL change in `lib/net/src/ImprovSerial.cpp`
- **Consumes:** `FeedConfig`, `feed::fromJson/toJson`, snapshots, parsers, `Canvas::packRows`.
- **Produces:**
  - `net::beginClock(const char* posixTz)`, `bool net::timeSynced()`
  - `net::beginFetcher()`, `net::requestRefresh()`, `net::copyWeather(WeatherSnap&)`, `copyHn`, `copyCalendar`, `net::onFeedChanged(const FeedConfig&)`. Intervals: weather 30 min, HN 15 min, calendar 60 min; 10 s timeouts; backoff 1, 2, 5, 15 min in pure `FetchPolicy.h`.
  - `bool feedfile::load(FeedConfig&)` (defaults on missing/corrupt, corrupt kept as `/feed.bad`), `bool feedfile::save(const FeedConfig&)` (temp file + rename)
  - `net::beginApi(ApiHooks)` with `struct ApiHooks { FeedConfig* feed; void (*onFeedSaved)(); void (*renderPage)(PageId, Canvas&); const Canvas* (*liveCanvas)(); void (*showPage)(PageId); const char* (*currentPage)(); }`; `net::updateApi()` called from `loop()`
  - `embed_ui.py`: gzips each file under `device-ui/` except `mock/` and `tests/` (fonts stored ungzipped) into `$BUILD_DIR/generated/ui_files.h` as `struct UiFile { const char* path; const char* mime; const uint8_t* data; size_t len; bool gzip; }`; skipped on native; writes only when content changed.
- **Feature flag:** `FEATURE_DASHBOARD` (WiFi required), added by Team E's config section.

### Team D: Web app

- **Owns:** `device-ui/**` except `mock/fixtures/` (frozen), `THIRD_PARTY_NOTICES.md` font entry
- **Consumes:** `docs/api.md`, fixtures, the palette/type/layout/copy rules in the spec's "Web app" section. Load the `frontend-design` skill.
- **Produces:**
  - `index.html`, `styles/{tokens,base,app}.css`, `js/{api,screen,queue,editors,tz,geocode,main}.js`, each under 300 lines
  - `js/screen.js`: `drawScreen(canvas, bytes)` painting the 3904-byte format at an integer scale with paper `#E3E5DC` / ink `#17191B`
  - `js/tz.js`: bundled IANA-to-POSIX table (generated once from the system tz database by a committed script) and `posixFor(iana)`
  - `mock/server.mjs`: zero-dependency Node server implementing every route from `docs/api.md` against the fixtures, serving `device-ui/` statically; `/api/screen` returns a canned bitmap until Phase 2 wires in real renders
  - `tests/*.test.mjs` (`node:test`): screen byte decoding, feed edit reducers (reorder, toggle, add/remove message, limits from `/api/state`), tz lookup, API error mapping to copy
- **Design critique:** screenshot at 390 px and 1280 px with Playwright against the mock, review against the spec, iterate once before handing over.

### Team E: Config, docs, site

- **Owns:** `scripts/config/sections/dashboard.py` (registration in `__init__.py` goes through the orchestrator), `scripts/tests/test_dashboard_section.py`, `config.yaml` `dashboard:` block, `assets/starter-messages.txt`, `README.md`, `docs/*.md` except `docs/api.md` and `docs/superpowers/`, `web/src/content/site.ts`, `THIRD_PARTY_NOTICES.md` (GxEPD2, U8g2, fonts)
- **Produces:** `dashboard` section emitting `FEATURE_DASHBOARD`, `AppConfig::DEFAULT_DWELL_S`, `AppConfig::STARTER_MESSAGES[]` (`{text, by}`, about 30, each within `MESSAGE_TEXT_MAX`), validated against the same numbers as `Limits.h` (the test parses `Limits.h` so the two cannot drift). README at or under 250 lines states: no login, GPL-3.0 binaries, V1.1 panel only tested.

---

## Phase 2: Integration (orchestrator)

### Task 2.1: Page views
- **Files:** `src/pages/{Pages.h,ClockPage,WeatherPage,HnPage,MessagePage,CalendarPage,NotesPage,SetupPage}.cpp`, `test/test_pages/` (golden 3904-byte images under `test/fixtures/golden/`)
- **Interface:** `void pages::render(PageId, const PageContext&, Canvas&)` with `struct PageContext { const FeedConfig& feed; const WeatherSnap& weather; const HnSnap& hn; const CalSnap& cal; time_t now; uint8_t visit; }`; `pages::renderSetup(Canvas&, const char* line)`.
- Golden tests for each page's normal, empty and stale state. Commit per two pages.

### Task 2.2: Main wiring
- **Files:** `src/main.cpp`, `src/App.{h,cpp}` if `main.cpp` would pass 300 lines.
- Boot: board, panel, feed file, WiFi + Improv, clock, fetcher, API. Loop: button gesture (click `Rotation::next`, hold `requestRefresh`), `Rotation::tick`, re-render when the page, its snapshot or the feed changed, minute tick on the clock page with `Refresh::Fast`, hourly full refresh. Mock server switched to real renders through a small native tool `tools/render_page` built from the same sources.

### Task 2.3: On-device verification with Alec
- Re-identify and flash; provision WiFi; walk the spec's hardware checklist; fix layout from what Alec sees; record every observation and every still-pending check in `TODO.md`.

## Phase 3: Review and finish
- Independent `feature-dev:code-reviewer` agents per area (render, logic, network, web), each given the diff and the spec, not conclusions. Fix confirmed findings.
- One DRY and file-size pass (`code-simplifier`), then `bash scripts/check.sh`, `pio run -e wireless-paper`, `node --test device-ui/tests`, web checks, and the 1 h soak.
- Final commit: `Record verification results and pending hardware checks`.

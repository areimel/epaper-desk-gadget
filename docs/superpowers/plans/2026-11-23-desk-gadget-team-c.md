# Team C (network and storage) task plan

Charter: `2026-11-23-desk-gadget.md`, "Team C". Contract: `docs/api.md`. Everything in
`lib/net` that is new is gated by `#if FEATURE_DASHBOARD` (which implies `FEATURE_WIFI`).
Pure decisions live in header-only files that native tests include by relative path
(`#include "../../lib/net/src/X.h"`), because the native env ignores `lib/net`.

## People and files

| Who | Files |
|---|---|
| Builder (Sonnet) | `scripts/embed_ui.py`, `scripts/tests/test_embed_ui.py` |
| Lead (Opus) | `lib/net/src/{FetchPolicy.h, Clock.*, Fetcher.*, HttpGet.*, FeedFile.*, Api.*, ApiState.cpp, ApiScreen.cpp, StateJson.h, StaticFiles.*}`, `ImprovSerial.cpp` (redirect URL), `test/test_fetch_policy/`, `test/test_net_state_json/`, `docs/net-notes.md` |

The builder runs only Python tests, so no `pio` builds collide. The lead serialises every
`pio` run and reviews the builder's code before committing it.

## Tasks (each one a commit)

1. **Team plan** (this file).
2. **FetchPolicy + tests**: pure `SourcePolicy` (interval, backoff ladder 1/2/5/15 min,
   `requestNow`, wrap-safe `due`) and `nextState`. Native suite `test_fetch_policy`.
3. **Clock + Fetcher**: `beginClock/setTimezone/timeSynced`; one low-priority task, a source
   table (weather, HN, calendar placeholder), `HttpGet` doing HTTPS with the built-in CA
   bundle into a bounded sink via `HTTPClient::writeToStream` (de-chunks), mutex-guarded
   snapshot copies with "changed" flags plus read-only status accessors.
4. **FeedFile**: LittleFS on the `spiffs` partition; load (missing -> false, corrupt ->
   `/feed.bad`), save (tmp + rename, length checked).
5. **Api + state JSON**: `WebServer` routes exactly as `docs/api.md`, raw-body PUT with a
   413 check before buffering, static scratch `FeedConfig` and `Canvas`, mDNS. `/api/state`
   JSON built by a pure header (`StateJson.h`) tested against the mock fixture's keys and types.
6. **Static files + embed script**: `embed_ui.py` (builder) and `StaticFiles` serving the
   generated table with gzip, MIME, ETag from the firmware version; built-in fallback page.
7. **Improv URL**: provisioned replies carry `http://<ip>/`.

## Verification

- `pio test -e native`, `python -m unittest discover -s scripts/tests -v`.
- Temporary `src/main.cpp` wiring calling every public function, `pio run -e wireless-paper`,
  sizes recorded in `docs/net-notes.md`, then `src/main.cpp` reverted before committing.
- Nothing is flashed. Everything that touches the network, LittleFS or the HTTP server is
  compiled but unverified on hardware, and is listed as such in `docs/net-notes.md`.

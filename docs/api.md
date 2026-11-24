# Device HTTP API

The device serves this API and the web app on port 80, at its IP address and at
`http://desk-gadget.local`. There is no login: anyone on the same network can
read and change the feed. All bodies are JSON (`application/json`, UTF-8) unless
stated. Text limits are bytes of UTF-8 and come from `lib/feed/src/Limits.h`;
clients read them from `GET /api/state` and must not hard-code them.

| Route | Purpose |
|---|---|
| `GET /` and static files | the web app |
| `GET /api/state` | device status, source health, limits |
| `GET /api/feed` | the feed, without the calendar link |
| `PUT /api/feed` | validate and save a whole feed |
| `GET /api/screen` | the bitmap on the panel now |
| `GET /api/screen?page=<id>` | a page rendered off-panel with current data |
| `POST /api/show` | jump the panel to a page |
| `POST /api/refresh` | refetch every source now |

Page ids: `clock`, `weather`, `hn`, `messages`, `calendar`, `notes`.

## Errors

Every error is `{"error": "<plain sentence>", "path": "<field path or empty>"}`.

| Status | When |
|---|---|
| 400 | body is not JSON, a field is missing, has the wrong type or breaks a limit; `path` names it, e.g. `messages[3].text` |
| 404 | unknown route or page id |
| 413 | body larger than 16384 bytes |
| 500 | the feed could not be written to storage; the previous feed is still in use |

## GET /api/state

```json
{
  "name": "Desk Gadget", "version": "0.1.0", "board": "Heltec Wireless Paper",
  "ip": "192.168.1.50", "host": "desk-gadget.local",
  "time": 1795452060, "timeSynced": true,
  "page": "weather",
  "screen": {"w": 250, "h": 122, "rowBytes": 32},
  "limits": {"dwellMin": 180, "dwellMax": 3600, "messagesMax": 40,
             "messageTextMax": 140, "messageByMax": 32, "noteLinesMax": 5,
             "noteTextMax": 40, "noteTitleMax": 24, "urlMax": 256,
             "placeMax": 32, "tzMax": 48},
  "sources": {
    "weather":  {"state": "ok",    "updated": 1795451400, "error": ""},
    "hn":       {"state": "stale", "updated": 1795448000, "error": "no network"},
    "calendar": {"state": "never", "updated": 0,          "error": ""}
  },
  "calendarSkipped": 0, "heapFree": 182000, "refreshCount": 12
}
```

- `time` and `updated` are Unix seconds (UTC); `updated` is 0 if never fetched.
- `page` is the page on the panel, or `""` when none is enabled or the setup
  screen is showing.
- `state` is `never`, `ok`, `stale` (showing older data after a failed fetch) or
  `error` (no data at all). `error` is a short reason for `stale`/`error`.
- `calendarSkipped` counts events left out of today's calendar because they use
  another time zone or an unsupported repeat rule.

## GET /api/feed

```json
{
  "v": 1,
  "dwellSec": 300,
  "tz": {"iana": "America/New_York", "posix": "EST5EDT,M3.2.0,M11.1.0"},
  "pages": [{"id": "clock", "on": true}, {"id": "weather", "on": true},
            {"id": "hn", "on": true}, {"id": "messages", "on": true},
            {"id": "calendar", "on": false}, {"id": "notes", "on": true}],
  "weather": {"place": "Philadelphia", "lat": 39.95, "lon": -75.16, "units": "f"},
  "calendar": {"set": false},
  "messages": [{"text": "You are doing better than you think.", "by": "your desk"}],
  "notes": {"title": "Today",
            "lines": [{"text": "Ship the gadget", "todo": true, "done": false}]}
}
```

- `pages` is the rotation order and lists each page id exactly once.
- `weather` is `null` until a place is chosen. `units` is `"c"` or `"f"`.
- `calendar.set` says whether a link is stored. The link itself is never returned.
- `by` may be `""`. `done` is ignored unless `todo` is true.

## PUT /api/feed

Send the whole feed in the shape above. Differences from the `GET` shape:

- `calendar`: `{"url": "https://..."}` stores a link, `{"url": ""}` removes it.
  If `url` is absent (including sending back `{"set": ...}` unchanged) the stored
  link is kept. The link must start with `https://` or `webcal://`.
- `v` must be `1`. Unknown top-level keys are an error.
- `dwellSec` must be within `limits.dwellMin`..`limits.dwellMax`.

Success is `200` with the saved feed in the `GET /api/feed` shape. The panel
re-renders and affected sources refetch. On any error nothing is changed.

## GET /api/screen

`200`, `application/octet-stream`, exactly 3904 bytes: 122 rows of 32 bytes,
top row first. Within a row the most significant bit of the first byte is the
leftmost pixel; a set bit is black. Only the first 250 bits of each row are
pixels; the last 6 are zero. `Cache-Control: no-store`.

Without `page`, it is what the panel shows now. With `?page=<id>` the device
renders that page with current data without changing the panel (also for pages
that are switched off); an unknown id is `404`.

## POST /api/show

Body `{"page": "<id>"}`. Shows that page now (a full refresh, 2-4 s) and
restarts the dwell timer. `200 {"page": "<id>"}`; unknown id `404`; a page that
is switched off is still shown once.

## POST /api/refresh

No body. Queues a refetch of every configured source. `202 {}`. Watch
`/api/state` for the result.

## Timing

The device handles one request at a time and does not answer while the panel is
refreshing (up to about 4 s). Clients should send requests one after another
and allow a 10 s timeout.

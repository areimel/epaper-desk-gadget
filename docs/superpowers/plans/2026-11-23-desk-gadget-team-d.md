# Team D: Web app (device UI) plan

Charter: master plan "Team D: Web app". Contract: `docs/api.md` and
`device-ui/mock/fixtures/*.json` (frozen). Design brief: spec "Web app (device
UI)". No build step, no framework, no npm dependencies; files ship gzipped as-is.

## Team and file ownership (disjoint)

| Who | Files |
|---|---|
| Lead (Opus) | this plan, `device-ui/index.html`, `styles/*`, `js/{api,screen,feed,errors,geocode,dom,queue,editors*,main}.js`, `fonts/`, `THIRD_PARTY.md`, `tests/{screen,feed,errors,geocode,api}.test.mjs` |
| Helper 1 (Haiku) | `device-ui/tools/make_tz.mjs`, generated `device-ui/js/tz.js`, `tests/tz.test.mjs` |
| Helper 2 (Sonnet) | `device-ui/mock/{server,validate,bitmap}.mjs`, `tests/mock-*.test.mjs` |

## Module interfaces

Pure modules (no DOM, importable from Node):

- `js/screen.js`: `decodeScreen(bytes, {w,h,rowBytes}) -> Uint8Array(w*h)` of
  0/1 (1 = ink); `drawScreen(canvas, bytes, scale, geom)` paints paper
  `#E3E5DC` / ink `#17191B` (DOM side, guarded).
- `js/feed.js`: `utf8Bytes(s)`, `clone(feed)`, `isDirty(a, b)`, and immutable
  edits returning a new feed: `movePage(feed, from, to)`, `togglePage(feed, id,
  on)`, `setDwell`, `setTz(feed, iana, posix)`, `setPlace(feed, {place, lat,
  lon})`, `setUnits`, `setCalendarUrl(feed, url)` (`""` = remove),
  `addMessage/editMessage/removeMessage/moveMessage`, `setNotesTitle`,
  `addNoteLine/editNoteLine/removeNoteLine/moveNoteLine`; `checkFeed(feed,
  limits) -> [{path, message}]`; `canAdd(feed, limits, kind)`; `toPutBody(feed)`
  (calendar sent as `{url}` only when the user changed it, else `{}`).
- `js/errors.js`: `apiErrorFor(status, body) -> {path, message}` with plain copy
  for 400/404/413/500/network/timeout; `fieldId(path)` maps `messages[3].text`
  to a DOM id; `sourceLine(name, source, now)` and `calendarSkippedLine(n)` for
  source health; `ageText(now, then)`.
- `js/tz.js` (generated): `TZ` object `{iana: posix}`, `posixFor(iana)` (null if
  unknown), `zoneNames()` sorted, `browserZone()`.
- `js/geocode.js`: `geocodeUrl(name)`, `parsePlaces(json, placeMax) ->
  [{place, label, lat, lon}]` (place cut to `placeMax` bytes),
  `searchPlaces(name, placeMax, fetchImpl)`.
- `js/api.js`: `createApi({fetch, base, timeoutMs=10000})` returning
  `state()`, `feed()`, `saveFeed(body)`, `screen(page?)` (ArrayBuffer),
  `show(id)`, `refresh()`. Every call goes through one serial queue (the device
  serves one request at a time). Errors throw `ApiError {status, body}`.

DOM modules: `js/dom.js` (`h()` element helper), `js/queue.js` (page strip:
mini-screens, drag + move buttons + keyboard, on/off switch, selection),
`js/editors.js` (per-page settings panels; lists split into
`editors-lists.js`), `js/main.js` (state, load sequence, save, show, refresh,
source health).

## Mock server

`node device-ui/mock/server.mjs` binds 127.0.0.1 (`PORT`, default 8080), serves
`device-ui/` statically, implements every route in `docs/api.md` from the
fixtures with in-memory state. `SLOW=1` env or `?slow=1` on any request delays
~1.5 s. `STALE=1` is not needed: the fixture already has a stale source.
Exports `createMockServer(opts)` for tests. `validate.mjs` exports
`validateFeed(body, limits) -> {ok, path, error}`; `bitmap.mjs` exports
`screenFor(pageId) -> Uint8Array(3904)` (border + per-page pattern).

## Tests (`node --test device-ui/tests`, written first)

- screen: 3904-byte decode, MSB-left, last 6 bits ignored, wrong length throws.
- feed: reorder, toggle, add/edit/remove/move message, notes lines, byte
  limits (emoji, accents), dwell bounds, calendar put-body rules, dirty check.
- errors: 400 path -> field id + server copy, 413, 500, network, source lines
  (ok, stale, error, never), calendarSkipped copy.
- geocode: URL encoding, result parsing, place byte cut.
- api: requests are serialised, timeout raises, error body parsed.
- tz: known zones, unknown zone null, every value under 48 bytes.
- mock: every route, PUT validation (limits, unknown id, duplicate id, bad JSON,
  413, calendar keep/clear), screens 3904 bytes and differ per page.

## Order

1. Plan (this file). 2. Mock server + tests (helper 2) and tz (helper 1) in
parallel with pure modules + tests (lead). 3. App shell + preview. 4. Editors.
5. Playwright critique at 390x844 and 1280x800, iterate, commit.

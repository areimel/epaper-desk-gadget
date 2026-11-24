# The dashboard

What the gadget shows, where the data comes from, how often the panel
refreshes, and how you edit it. This describes the design in
[the spec](superpowers/specs/2026-11-23-desk-gadget-design.md); parts are still
being built, and nothing here has been observed on hardware beyond what
[../TODO.md](../TODO.md) lists under "Observed". Items marked (planned) are not
in the tree yet.

## Pages

The panel is 250x122 pixels, black on white. Six pages take turns:

| Page id | Shows |
|---|---|
| `clock` | Weekday and date over large numerals. The only page that updates every minute. |
| `weather` | Current conditions, feels-like, high and low, rain chance, and a short forecast. |
| `hn` | Three Hacker News front-page stories per visit, stepping through the top stories. |
| `messages` | One of your messages or quotes, in the largest type that fits. |
| `calendar` | Today's events from a Google Calendar, all-day first. |
| `notes` | A short title and a few lines, with `[ ]` and `[x]` drawn as boxes. |

Every page shows how old its data is. If a source fails, the page says so in
plain words instead of showing old data as if it were fresh. With no WiFi
configured the panel shows a setup page.

You can switch pages off and reorder them in the web app. The page ids are the
ones used by the [HTTP API](api.md).

## Refresh behaviour

E-paper holds its image without power but changes slowly, and a full refresh
flashes the whole panel. The design keeps that calm:

- Each page stays up for the dwell time: a default set by `dashboard.dwell_s`
  in `config.yaml`, then changeable in the web app. The allowed range is in
  `lib/feed/src/Limits.h`; the minimum exists to limit panel wear.
- The panel does a full refresh whenever the page changes and at least hourly.
- A fast (partial) refresh is used only for the clock's minute tick, with a
  full refresh after five fast ones, to clear ghosting.
- The button: click shows the next page now; hold refetches every source.
  (Button behaviour is pending a hardware check.)

So a page can take a few minutes to appear. That is intended, not a fault.

## Data sources

The device fetches everything itself over HTTPS, using the core's built-in
certificate bundle. No API keys or accounts are needed, and no fetch runs
before the clock has synced over NTP. One request runs at a time.

| Source | What is used | How often |
|---|---|---|
| Open-Meteo | Forecast for the place you chose in the web app; the browser does the city search against Open-Meteo's geocoding | every 30 minutes |
| Hacker News via Algolia | One `front_page` request: title, points and comment count for the top stories | every 15 minutes |
| Google Calendar | A private iCal address you paste into the web app | hourly |
| NTP | Time, in the timezone you pick in the web app | at boot and periodically |

The calendar link is a secret: anyone who has it can read your calendar. It is
stored only in `feed.json` on the device and is never returned by the API (the
web app only learns that one is set). It is not in `config.yaml`, the firmware
image or the repository. Events in another timezone and recurrence rules the
parser does not support are skipped and counted, with the count shown in the
web app, rather than shown at the wrong time. The calendar page is the last to
land and the riskiest.

Service terms and attribution: [../THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## The web app

When the device is on your WiFi it serves a web app on port 80, at its IP
address and at `http://desk-gadget.local` (the `.local` name depends on your
network; the IP always works). The setup reply over Improv includes the IP URL.

**There is no login.** Anyone on the same network can view and change what the
screen shows, including your messages, notes and calendar link settings. Do not
put anything private in a message or note, and do not expose the device to the
internet.

The app shows a pixel-exact preview of the panel and the page queue, where you
reorder and toggle pages and edit each page's settings. Saving writes the feed
to `feed.json` in the device's LittleFS partition, through a temp file and
rename. A missing or corrupt file falls back to defaults compiled from
`config.yaml`; a corrupt one is kept as `feed.bad`. The routes are in
[api.md](api.md). The web app source will live in `device-ui/` (planned) and is
embedded into the firmware as gzipped files.

## Defaults from config.yaml

The `dashboard:` section sets compile-time defaults: whether the dashboard is
on, the default dwell, and the starter messages used on first boot. See
[configuration.md](configuration.md). The dashboard needs `wifi.enabled: true`;
without it the feature is switched off with a build warning.

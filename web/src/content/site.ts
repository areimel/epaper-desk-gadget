const otaNote =
  "Release builds have OTA disabled. OTA is for local development builds, enabled by an ota_password in your own secrets.yaml.";

/** Installer-site copy for the desk gadget. Project identity from firmware/index.json overrides `project`. */
export const site = {
  project: {
    name: "Desk Gadget",
    tagline: "An e-paper desk dashboard.",
    description:
      "Clock, weather, Hacker News, messages, calendar and notes on a Heltec Wireless Paper e-paper display, edited from a web app the device serves on your WiFi.",
  },
  /** Source link for forks: REPO_URL, else GitHub Actions' repository, else none. */
  repository:
    process.env.REPO_URL ||
    (process.env.GITHUB_REPOSITORY
      ? `https://github.com/${process.env.GITHUB_REPOSITORY}`
      : undefined),
  noRelease:
    "No release published yet. Build locally with pio run, or publish a release to enable browser install.",
  otaNote,
  /** Update this when more behavior has been observed on hardware (see TODO.md). */
  hardwareNote:
    "Hardware status: on one Heltec Wireless Paper the panel test card has been seen upright and readable. The six dashboard pages, WiFi setup, the web app and the button have not been observed on hardware yet.",
  settingsNote:
    "The installer never writes the NVS settings partition, so the layout is meant to keep stored WiFi credentials across a reflash unless you choose to erase. This has not been observed.",
  features: [
    {
      title: "Six pages, slowly turning",
      body: "Clock, weather, Hacker News, messages, calendar and notes take turns on the e-paper screen. Pages turn every few minutes on purpose, to keep the panel calm.",
    },
    {
      title: "Edit it from your phone",
      body: "The device serves a small web app on your WiFi. Reorder pages, set your city, write messages and notes, and preview exactly what the panel shows.",
    },
    {
      title: "No account, no cloud",
      body: "The device fetches weather and headlines itself. Your calendar link stays on the device. The web app has no login, so anyone on your WiFi can edit it.",
    },
    {
      title: "Safe browser flashing",
      body: "The installer writes four images: bootloader, partition table, boot_app0 and firmware. It never writes the settings partition.",
    },
  ],
  steps: [
    "Connect the Heltec Wireless Paper over USB with a data cable.",
    "Press Install, pick the serial port in the browser prompt and confirm.",
    "When the flash finishes, enter your WiFi name and password in the setup dialog.",
    "Open the address the installer shows (or http://desk-gadget.local) to set your city, messages and notes.",
  ],
  faq: [
    {
      q: "Which browsers work?",
      a: "Chromium-based browsers that provide Web Serial, such as Chrome, Edge and Opera. Safari and Firefox cannot flash.",
    },
    {
      q: "The port does not appear.",
      a: "Use a USB data cable and close any serial monitor. If it still does not show, unplug the board and plug it in again.",
    },
    {
      q: "The screen did not change right away.",
      a: "E-paper turns pages slowly on purpose, every few minutes. Press the button to move to the next page now, or hold it to refetch everything.",
    },
    {
      q: "The web app does not open at desk-gadget.local.",
      a: "The .local name depends on your network. Use the device's IP address instead; the installer shows it after WiFi setup.",
    },
    {
      q: "Who can change what the screen shows?",
      a: "Anyone on the same WiFi network. The web app has no login. Do not put anything private in a message or note.",
    },
    {
      q: "Does the released firmware support OTA updates?",
      a: `No. ${otaNote}`,
    },
    {
      q: "How do I recover a bad flash?",
      a: "Reflash from this page and choose to erase the device (the erase option has not been tried on hardware), or run pio run -t erase followed by pio run -t upload.",
    },
    {
      q: "What licence is the firmware under?",
      a: "The project source is MIT. Firmware binaries link the GxEPD2 display driver and are distributed under GPL-3.0. See THIRD_PARTY_NOTICES.md in the repository.",
    },
  ],
};

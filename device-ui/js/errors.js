// Plain-language copy for API failures and source health. Pure.
const FALLBACK = {
  0: "Couldn't reach the gadget. Check it is powered and on the same WiFi, then try again.",
  timeout: "The gadget didn't answer within 10 seconds. It may be redrawing the screen; try again.",
  400: "The gadget didn't accept these changes. Check the highlighted fields and try again.",
  404: "The gadget doesn't know that page. Reload this page and try again.",
  413: 'The feed is too large to save. Remove some messages or notes and try again.',
  500: "The gadget couldn't save to its storage, so the previous feed is still in use. Try again.",
};

export function apiErrorFor(status, body) {
  const path = typeof body?.path === 'string' ? body.path : '';
  if (status === 413 || status === 500 || status === 0 || status === 'timeout') {
    return { path, message: FALLBACK[status] };
  }
  const server = typeof body?.error === 'string' && body.error.trim() ? body.error : '';
  return { path, message: server || FALLBACK[status] || `The gadget answered with an error (${status}). Try again.` };
}

// "messages[3].text" -> "f-messages-3-text": the id of the matching input.
export function fieldId(path) {
  if (!path) return '';
  return 'f-' + path.replace(/\[(\d+)\]/g, '-$1').replace(/\./g, '-');
}

export function ageText(now, then) {
  const s = Math.max(0, now - then);
  if (s < 60) return 'just now';
  if (s < 3600) return `${Math.floor(s / 60)} min ago`;
  if (s < 86400) return `${Math.floor(s / 3600)} h ago`;
  if (s < 2 * 86400) return 'yesterday';
  return `${Math.floor(s / 86400)} days ago`;
}

const SOURCES = {
  weather: { name: 'Weather', older: 'an older forecast', fix: 'Check the place in Weather settings.' },
  hn: { name: 'Hacker News', older: 'older headlines', fix: 'It will try again on its own.' },
  calendar: { name: 'Calendar', older: "an older copy of today's events", fix: 'Check the calendar link.' },
};

export function sourceName(id) {
  return SOURCES[id]?.name || id;
}

export function sourceLine(id, src, now) {
  const s = SOURCES[id] || { name: id, older: 'older data', fix: '' };
  const why = src.error ? `, ${src.error}` : '';
  switch (src.state) {
    case 'ok':
      return { ok: true, text: `${s.name}: updated ${ageText(now, src.updated)}` };
    case 'stale':
      return { ok: false, text: `${s.name}: showing ${s.older} from ${ageText(now, src.updated)}${why}` };
    case 'error':
      return { ok: false, text: `${s.name}: didn't load${why}. ${s.fix}`.trim() };
    default:
      return { ok: true, text: `${s.name}: not loaded yet` };
  }
}

export function calendarSkippedLine(n) {
  if (!n) return '';
  const one = n === 1;
  return `${n} ${one ? 'event' : 'events'} today ${one ? 'is' : 'are'} left out because ${one ? 'it uses' : 'they use'} another time zone or a repeat rule the gadget can't follow.`;
}

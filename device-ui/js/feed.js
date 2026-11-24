// Pure feed edits. Every function returns a new feed and leaves its input alone.
const enc = new TextEncoder();
export const utf8Bytes = (s) => enc.encode(s || '').length;
export const clone = (o) => (o == null ? o : JSON.parse(JSON.stringify(o)));
export const isDirty = (a, b) => JSON.stringify(a) !== JSON.stringify(b);

function moved(list, from, to) {
  if (from < 0 || from >= list.length || to < 0 || to >= list.length || from === to) return list.slice();
  const out = list.slice();
  const [item] = out.splice(from, 1);
  out.splice(to, 0, item);
  return out;
}
const patchAt = (list, i, patch) => list.map((x, j) => (j === i ? { ...x, ...patch } : x));

export const movePage = (f, from, to) => ({ ...clone(f), pages: moved(f.pages, from, to) });
export function movePageBy(f, id, delta) {
  const i = f.pages.findIndex((p) => p.id === id);
  return movePage(f, i, i + delta);
}
export const togglePage = (f, id, on) =>
  ({ ...clone(f), pages: f.pages.map((p) => (p.id === id ? { ...p, on } : { ...p })) });
export const pageOn = (f, id) => !!f.pages.find((p) => p.id === id)?.on;

export const setDwell = (f, sec) => ({ ...clone(f), dwellSec: Math.round(Number(sec)) });
export const setTz = (f, iana, posix) => ({ ...clone(f), tz: { iana, posix } });
export function setPlace(f, { place, lat, lon }) {
  const units = f.weather?.units || 'c';
  return { ...clone(f), weather: { place, lat, lon, units } };
}
export const setUnits = (f, units) =>
  ({ ...clone(f), weather: f.weather ? { ...f.weather, units } : null });

// calendar: GET gives {set}; a pending change adds `url` ("" = remove).
export const setCalendarUrl = (f, url) => ({ ...clone(f), calendar: { ...f.calendar, url } });
export const undoCalendar = (f) => ({ ...clone(f), calendar: { set: !!f.calendar?.set } });
export function calendarState(f) {
  const c = f.calendar || {};
  if (typeof c.url === 'string') return c.url === '' ? (c.set ? 'removing' : 'none') : 'new';
  return c.set ? 'saved' : 'none';
}

export const addMessage = (f) => ({ ...clone(f), messages: [...f.messages, { text: '', by: '' }] });
export const editMessage = (f, i, patch) => ({ ...clone(f), messages: patchAt(f.messages, i, patch) });
export const removeMessage = (f, i) => ({ ...clone(f), messages: f.messages.filter((_, j) => j !== i) });
export const moveMessage = (f, from, to) => ({ ...clone(f), messages: moved(f.messages, from, to) });

const withLines = (f, lines) => ({ ...clone(f), notes: { ...f.notes, lines } });
export const setNotesTitle = (f, title) => ({ ...clone(f), notes: { ...f.notes, title } });
export const addNoteLine = (f) => withLines(f, [...f.notes.lines, { text: '', todo: true, done: false }]);
export function editNoteLine(f, i, patch) {
  const p = patch.todo === false ? { ...patch, done: false } : patch;
  return withLines(f, patchAt(f.notes.lines, i, p));
}
export const removeNoteLine = (f, i) => withLines(f, f.notes.lines.filter((_, j) => j !== i));
export const moveNoteLine = (f, from, to) => withLines(f, moved(f.notes.lines, from, to));

export function canAdd(f, limits, kind) {
  if (kind === 'message') return f.messages.length < limits.messagesMax;
  if (kind === 'note') return f.notes.lines.length < limits.noteLinesMax;
  return false;
}

// The body for PUT /api/feed: calendar link only when the person changed it.
export function toPutBody(f) {
  const body = clone(f);
  body.calendar = typeof f.calendar?.url === 'string' ? { url: f.calendar.url } : {};
  return body;
}

function tooLong(issues, path, s, max, what) {
  const n = utf8Bytes(s);
  if (n > max) issues.push({ path, message: `${what} is ${n} bytes; the limit is ${max}. Shorten it.` });
}

// Checks the browser can make before saving; the device re-checks everything.
export function checkFeed(f, limits) {
  const issues = [];
  const L = limits;
  if (!(f.dwellSec >= L.dwellMin && f.dwellSec <= L.dwellMax)) {
    issues.push({ path: 'dwellSec', message: `Pick a time between ${L.dwellMin / 60} and ${L.dwellMax / 60} minutes.` });
  }
  f.messages.forEach((m, i) => {
    if (!m.text.trim()) issues.push({ path: `messages[${i}].text`, message: 'Write a message or remove this one.' });
    tooLong(issues, `messages[${i}].text`, m.text, L.messageTextMax, 'This message');
    tooLong(issues, `messages[${i}].by`, m.by, L.messageByMax, 'This name');
  });
  if (f.messages.length > L.messagesMax) {
    issues.push({ path: 'messages', message: `Keep at most ${L.messagesMax} messages.` });
  }
  tooLong(issues, 'notes.title', f.notes.title, L.noteTitleMax, 'The title');
  f.notes.lines.forEach((l, i) => {
    if (!l.text.trim()) issues.push({ path: `notes.lines[${i}].text`, message: 'Write something on this line or remove it.' });
    tooLong(issues, `notes.lines[${i}].text`, l.text, L.noteTextMax, 'This line');
  });
  if (f.notes.lines.length > L.noteLinesMax) {
    issues.push({ path: 'notes.lines', message: `Keep at most ${L.noteLinesMax} lines.` });
  }
  const url = f.calendar?.url;
  if (typeof url === 'string' && url !== '') {
    if (!/^(https|webcal):\/\//i.test(url)) {
      issues.push({ path: 'calendar.url', message: 'Use the secret address in iCal format; it starts with https:// or webcal://.' });
    } else tooLong(issues, 'calendar.url', url, L.urlMax, 'This link');
  }
  if (f.weather) tooLong(issues, 'weather.place', f.weather.place, L.placeMax, 'The place name');
  tooLong(issues, 'tz.posix', f.tz?.posix, L.tzMax, 'The time zone rule');
  return issues;
}

// Feed validation for the mock server. Mirrors docs/api.md; lengths are UTF-8 bytes.
export const PAGE_IDS = ['clock', 'weather', 'hn', 'messages', 'calendar', 'notes'];
const KEYS = ['v', 'dwellSec', 'tz', 'pages', 'weather', 'calendar', 'messages', 'notes'];
const bytes = (s) => Buffer.byteLength(s, 'utf8');
const isObj = (v) => v !== null && typeof v === 'object' && !Array.isArray(v);
const fail = (path, error) => ({ ok: false, path, error });

function text(v, path, label, min, max) {
  if (typeof v !== 'string') return fail(path, `${label} must be text.`);
  const n = bytes(v);
  if (n < min) return fail(path, `${label} cannot be empty.`);
  if (n > max) return fail(path, `${label} is ${n} bytes; the limit is ${max}.`);
  return null;
}

function checkTz(tz, l) {
  if (!isObj(tz)) return fail('tz', 'The time zone is missing.');
  return text(tz.iana, 'tz.iana', 'The time zone name', 1, l.tzMax)
    || text(tz.posix, 'tz.posix', 'The time zone rule', 1, l.tzMax);
}

function checkPages(pages) {
  if (!Array.isArray(pages)) return fail('pages', 'The page list is missing.');
  const seen = new Set();
  for (let i = 0; i < pages.length; i++) {
    const p = pages[i];
    if (!isObj(p) || !PAGE_IDS.includes(p.id)) {
      return fail(`pages[${i}].id`, `There is no page called "${isObj(p) ? p.id : p}".`);
    }
    if (seen.has(p.id)) return fail(`pages[${i}].id`, `The page "${p.id}" is listed twice.`);
    seen.add(p.id);
    if (typeof p.on !== 'boolean') return fail(`pages[${i}].on`, `Page "${p.id}" must be on or off.`);
  }
  const missing = PAGE_IDS.find((id) => !seen.has(id));
  return missing ? fail('pages', `The page "${missing}" is missing from the list.`) : null;
}

function checkWeather(w, l) {
  if (w === null) return null;
  if (!isObj(w)) return fail('weather', 'Weather must be a place or null.');
  const bad = text(w.place, 'weather.place', 'The place name', 1, l.placeMax);
  if (bad) return bad;
  const num = (v, lo, hi) => typeof v === 'number' && Number.isFinite(v) && v >= lo && v <= hi;
  if (!num(w.lat, -90, 90)) return fail('weather.lat', 'Latitude must be a number from -90 to 90.');
  if (!num(w.lon, -180, 180)) return fail('weather.lon', 'Longitude must be a number from -180 to 180.');
  if (w.units !== 'c' && w.units !== 'f') return fail('weather.units', 'Units must be "c" or "f".');
  return null;
}

function checkCalendar(c, l) {
  if (!isObj(c)) return fail('calendar', 'The calendar section is missing.');
  if (!('url' in c)) return null;
  if (typeof c.url !== 'string') return fail('calendar.url', 'The calendar link must be text.');
  if (c.url === '') return null;
  if (!/^(https:\/\/|webcal:\/\/)/.test(c.url)) {
    return fail('calendar.url', 'The calendar link must start with https:// or webcal://.');
  }
  const n = bytes(c.url);
  return n > l.urlMax ? fail('calendar.url', `The calendar link is ${n} bytes; the limit is ${l.urlMax}.`) : null;
}

function checkMessages(m, l) {
  if (!Array.isArray(m)) return fail('messages', 'The message list is missing.');
  if (m.length > l.messagesMax) {
    return fail('messages', `There are ${m.length} messages; the limit is ${l.messagesMax}.`);
  }
  for (let i = 0; i < m.length; i++) {
    if (!isObj(m[i])) return fail(`messages[${i}]`, `Message ${i + 1} is not valid.`);
    const bad = text(m[i].text, `messages[${i}].text`, `Message ${i + 1}`, 1, l.messageTextMax)
      || text(m[i].by, `messages[${i}].by`, `The author of message ${i + 1}`, 0, l.messageByMax);
    if (bad) return bad;
  }
  return null;
}

function checkNotes(n, l) {
  if (!isObj(n)) return fail('notes', 'The notes section is missing.');
  const bad = text(n.title, 'notes.title', 'The notes title', 0, l.noteTitleMax);
  if (bad) return bad;
  if (!Array.isArray(n.lines)) return fail('notes.lines', 'The note lines are missing.');
  if (n.lines.length > l.noteLinesMax) {
    return fail('notes.lines', `There are ${n.lines.length} note lines; the limit is ${l.noteLinesMax}.`);
  }
  for (let i = 0; i < n.lines.length; i++) {
    const ln = n.lines[i];
    if (!isObj(ln)) return fail(`notes.lines[${i}]`, `Note line ${i + 1} is not valid.`);
    const b = text(ln.text, `notes.lines[${i}].text`, `Note line ${i + 1}`, 1, l.noteTextMax);
    if (b) return b;
    for (const k of ['todo', 'done']) {
      if (typeof ln[k] !== 'boolean') {
        return fail(`notes.lines[${i}].${k}`, `Note line ${i + 1} needs ${k} to be true or false.`);
      }
    }
  }
  return null;
}

export function validateFeed(body, limits) {
  if (!isObj(body)) return fail('', 'The feed must be a JSON object.');
  for (const k of Object.keys(body)) {
    if (!KEYS.includes(k)) return fail(k, `The feed has an unknown field "${k}".`);
  }
  for (const k of KEYS) if (!(k in body)) return fail(k, `The feed is missing "${k}".`);
  if (body.v !== 1) return fail('v', 'The feed version must be 1.');
  const d = body.dwellSec;
  if (!Number.isInteger(d) || d < limits.dwellMin || d > limits.dwellMax) {
    return fail('dwellSec', `Seconds per page must be a whole number from ${limits.dwellMin} to ${limits.dwellMax}.`);
  }
  return checkTz(body.tz, limits) || checkPages(body.pages) || checkWeather(body.weather, limits)
    || checkCalendar(body.calendar, limits) || checkMessages(body.messages, limits)
    || checkNotes(body.notes, limits) || { ok: true };
}

// Settings for the selected page, shown next to that page's own preview.
import { h, field } from './dom.js';
import { fieldId, sourceLine, calendarSkippedLine } from './errors.js';
import * as F from './feed.js';
import { zoneNames, posixFor, browserZone } from './tz.js';
import { searchPlaces } from './geocode.js';
import { PAGE_NAMES, paintScreen } from './queue.js';
import { messagesEditor, notesEditor } from './editors-lists.js';

const TITLES = {
  clock: 'Clock and date', weather: 'Weather', hn: 'Hacker News',
  messages: 'Messages and quotes', calendar: "Today's calendar", notes: 'Notes and to-dos',
};

function health(app, id) {
  const src = app.state?.sources?.[id];
  if (!src) return null;
  const line = sourceLine(id, src, app.state.time);
  return h('p', { class: `source${line.ok ? '' : ' stale'}` }, line.text);
}

function clockEditor(app) {
  const f = app.draft;
  const names = zoneNames();
  const select = h('select', {
    onchange: (e) => {
      const iana = e.target.value;
      app.update((d) => F.setTz(d, iana, posixFor(iana)), { rerender: 'editor', clear: 'tz' });
    },
  });
  const known = names.includes(f.tz.iana);
  if (!known) select.append(h('option', { value: f.tz.iana }, `${f.tz.iana} (saved)`));
  for (const n of names) select.append(h('option', { value: n }, n.replace(/_/g, ' ')));
  select.value = f.tz.iana;
  const { wrap } = field({
    id: fieldId('tz.iana'), label: 'Time zone', input: select,
    hint: 'The clock, the calendar and every "updated" time use this zone.',
  });
  const here = browserZone();
  const use = here && here !== f.tz.iana
    ? h('button', {
        type: 'button', class: 'link',
        onclick: () => app.update((d) => F.setTz(d, here, posixFor(here)), { rerender: 'editor', clear: 'tz' }),
      }, `Use this browser's zone, ${here.replace(/_/g, ' ')}`)
    : null;
  return [wrap, use];
}

function weatherEditor(app) {
  const f = app.draft;
  const L = app.state.limits;
  const results = h('ul', { class: 'places', 'aria-live': 'polite' });
  const input = h('input', { type: 'search', autocomplete: 'off', enterkeyhint: 'search', placeholder: 'City name' });
  const status = h('p', { class: 'hint', role: 'status' });
  async function search(e) {
    e.preventDefault();
    status.textContent = 'Searching...';
    results.replaceChildren();
    try {
      const places = await searchPlaces(input.value, L.placeMax);
      status.textContent = places.length ? 'Pick your place:' : `No places called "${input.value.trim()}". Check the spelling or try a nearby city.`;
      results.replaceChildren(...places.map((p) => h('li', {}, h('button', {
        type: 'button', class: 'place',
        onclick: () => app.update((d) => F.setPlace(d, p), { rerender: 'editor', clear: 'weather' }),
      }, p.label))));
    } catch (err) {
      status.textContent = err.message;
    }
  }
  const { wrap } = field({ id: fieldId('weather.place'), label: f.weather ? 'Change the place' : 'Find your city', input });
  const form = h('form', { class: 'search', onsubmit: search }, wrap, h('button', { type: 'submit' }, 'Search'));
  const pending = f.weather && f.weather.place !== app.saved.weather?.place;
  const current = f.weather
    ? h('p', { class: 'current' }, pending ? 'After you save, the gadget shows the weather for ' : 'Showing the weather for ',
      h('strong', {}, f.weather.place), '.')
    : h('p', { class: 'empty' }, 'No place chosen yet. Search for your city to show its weather.');
  const units = f.weather ? h('fieldset', { class: 'units' }, h('legend', {}, 'Temperatures in'),
    ...[['c', 'Celsius'], ['f', 'Fahrenheit']].map(([v, label]) => h('label', {},
      h('input', {
        type: 'radio', name: 'units', value: v, checked: f.weather.units === v,
        onchange: () => app.update((d) => F.setUnits(d, v)),
      }), label))) : null;
  return [current, form, status, results, units];
}

function calendarEditor(app, local) {
  const f = app.draft;
  const state = F.calendarState(f);
  const out = [];
  const rerender = { rerender: 'editor', clear: 'calendar' };
  if (state === 'saved' && !local.replacing) {
    out.push(h('p', { class: 'current' }, 'A calendar link is saved.'),
      h('div', { class: 'actions' },
        h('button', { type: 'button', onclick: () => { local.replacing = true; app.rerender('editor'); } }, 'Replace link'),
        h('button', { type: 'button', onclick: () => app.update((d) => F.setCalendarUrl(d, ''), rerender) }, 'Remove link')));
  } else if (state === 'removing') {
    out.push(h('p', { class: 'current' }, 'The saved link will be removed when you save changes.'),
      h('button', { type: 'button', onclick: () => app.update((d) => F.undoCalendar(d), rerender) }, 'Keep the saved link'));
  } else {
    const input = h('input', {
      type: 'url', inputmode: 'url', autocomplete: 'off', spellcheck: false,
      placeholder: 'https://calendar.google.com/calendar/ical/...', value: f.calendar.url || '',
      maxlength: app.state.limits.urlMax * 4,
      oninput: (e) => {
        const v = e.target.value.trim();
        app.update((d) => (v ? F.setCalendarUrl(d, v) : F.undoCalendar(d)), { clear: 'calendar' });
      },
    });
    const { wrap } = field({
      id: fieldId('calendar.url'), label: f.calendar.set ? 'New calendar link' : 'Calendar link', input,
      hint: 'In Google Calendar open Settings, pick your calendar, and copy "Secret address in iCal format". The gadget keeps it private and never shows it again.',
    });
    out.push(wrap);
    if (f.calendar.set) {
      out.push(h('button', {
        type: 'button', class: 'link',
        onclick: () => { local.replacing = false; app.update((d) => F.undoCalendar(d), rerender); },
      }, 'Keep the saved link'));
    }
  }
  const skipped = calendarSkippedLine(app.state.calendarSkipped);
  if (skipped) out.push(h('p', { class: 'source stale' }, skipped));
  return out;
}

const hnEditor = () => [h('p', {}, 'Nothing to set up. The gadget shows the top front-page stories, a few at a time, and steps through them on each visit.')];

const BUILDERS = {
  clock: clockEditor, weather: weatherEditor, hn: hnEditor,
  messages: messagesEditor, calendar: calendarEditor, notes: notesEditor,
};
export const DIRTY_NOTE = 'The preview updates after you save changes.';
const local = {}; // per-page view state (e.g. calendar "replacing"); never saved

export function renderEditor(root, app) {
  const id = app.selected;
  if (!id) {
    root.hidden = true;
    root.replaceChildren();
    return;
  }
  local[id] ||= {};
  if (id === 'calendar' && F.calendarState(app.draft) === 'none') local[id].replacing = false;
  const on = F.pageOn(app.draft, id);
  const canvas = h('canvas', { class: 'screen loading', width: 250, height: 122, role: 'img', 'aria-label': `Preview of the ${PAGE_NAMES[id]} page` });
  paintScreen(canvas, app.screens.get(id), app.geom, Math.min(250, root.clientWidth || 358));
  const clean = on ? 'In the rotation.' : 'Switched off, so it is not in the rotation.';
  root.hidden = false;
  root.replaceChildren(
    h('div', { class: 'editor-head' },
      h('h2', { id: 'editor-title', tabindex: '-1' }, TITLES[id]),
      h('button', { type: 'button', class: 'link', onclick: () => app.select(null) }, 'Close')),
    h('div', { class: 'editor-body' },
      h('figure', { class: 'editor-preview' }, canvas,
        h('figcaption', { id: 'preview-note', 'data-clean': clean }, app.dirty ? DIRTY_NOTE : clean),
        h('button', { type: 'button', onclick: () => app.showPage(id) }, 'Show on gadget'),
        h('p', { id: 'show-status', class: 'hint', role: 'status' })),
      h('div', { class: 'editor-form' }, health(app, id), ...BUILDERS[id](app, local[id]))));
  applyErrors(root, app);
}

// Put each server or browser error next to the field its path names.
export function applyErrors(root, app) {
  for (const el of root.querySelectorAll('.field-error')) { el.hidden = true; el.textContent = ''; }
  for (const el of root.querySelectorAll('[aria-invalid]')) el.removeAttribute('aria-invalid');
  for (const [path, message] of app.errors) {
    const input = document.getElementById(fieldId(path));
    const slot = document.getElementById(`${fieldId(path)}-error`);
    if (!input || !slot || !root.contains(input)) continue;
    slot.textContent = message;
    slot.hidden = false;
    input.setAttribute('aria-invalid', 'true');
  }
}

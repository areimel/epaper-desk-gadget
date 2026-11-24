// Wiring: load state, feed and screens one request at a time, then render.
import { h, $ } from './dom.js';
import { createApi } from './api.js';
import { GEOM, drawScreen, fitCanvas } from './screen.js';
import * as F from './feed.js';
import { apiErrorFor, fieldId, pageForPath, sourceLine, calendarSkippedLine } from './errors.js';
import { renderQueue, paintMini, PAGE_NAMES } from './queue.js';
import { renderEditor, applyErrors, DIRTY_NOTE } from './editors.js';

const api = createApi();
const app = {
  state: null, saved: null, draft: null, selected: null, geom: GEOM,
  screens: new Map(), live: null, errors: new Map(), dirty: false,
  save: { phase: 'idle', message: '' }, // idle | saving | saved | error
};

// ---- rendering -------------------------------------------------------------

function paintLive() {
  const canvas = $('#live');
  const wrap = $('.hero');
  fitCanvas(canvas, wrap.clientWidth - 24, app.geom, 3, window.devicePixelRatio || 1);
  if (app.live) { drawScreen(canvas, app.live, app.geom); canvas.classList.remove('loading'); }
}

function renderHero() {
  const s = app.state;
  const name = s?.page ? PAGE_NAMES[s.page] : '';
  const mins = app.draft ? Math.round(app.saved.dwellSec / 60) : 0;
  $('#live-caption').textContent = !s ? 'Loading the screen from the gadget...'
    : name ? `On the gadget now: ${name}. Pages change every ${mins} minutes.`
      : 'The gadget has no page to show. Switch on a page below.';
  paintLive();
}

function renderHealth() {
  const s = app.state;
  if (!s) return;
  const items = Object.entries(s.sources).map(([id, src]) => {
    const line = sourceLine(id, src, s.time);
    return h('li', { class: line.ok ? 'ok' : 'stale' }, line.text);
  });
  const skipped = calendarSkippedLine(s.calendarSkipped);
  if (skipped) items.push(h('li', { class: 'stale' }, skipped));
  if (!s.timeSynced) items.unshift(h('li', { class: 'stale' }, "The gadget hasn't set its clock yet, so it can't fetch anything. Check that the WiFi has internet access."));
  $('#sources').replaceChildren(...items);
  $('#device').textContent = `Version ${s.version} on a ${s.board}, at ${s.host} (${s.ip}).`;
  $('h1').textContent = s.name;
}

function renderDwell() {
  const L = app.state.limits;
  const input = $('#f-dwellSec');
  input.min = Math.ceil(L.dwellMin / 60);
  input.max = Math.floor(L.dwellMax / 60);
  if (document.activeElement !== input) input.value = Math.round(app.draft.dwellSec / 60);
  $('#f-dwellSec-hint').textContent = `Between ${input.min} and ${input.max} minutes.`;
  const err = app.errors.get('dwellSec');
  $('#f-dwellSec-error').hidden = !err;
  $('#f-dwellSec-error').textContent = err || '';
  input.toggleAttribute('aria-invalid', !!err);
}

function renderSaveBar() {
  const bar = $('#savebar');
  const { phase, message } = app.save;
  const visible = app.dirty || phase !== 'idle';
  bar.hidden = !visible;
  document.body.classList.toggle('has-savebar', visible);
  const text = phase === 'saving' ? 'Saving...' : phase === 'saved' && !app.dirty ? 'Saved'
    : phase === 'error' ? message : 'You have unsaved changes.';
  $('#save-status').textContent = text;
  bar.classList.toggle('error', phase === 'error');
  bar.classList.toggle('saved', phase === 'saved' && !app.dirty);
  $('#save').disabled = phase === 'saving' || !app.dirty;
  const note = $('#preview-note');
  if (note) note.textContent = app.dirty ? DIRTY_NOTE : note.dataset.clean;
  $('#discard').hidden = !app.dirty;
  $('#save').hidden = !app.dirty;
}

function renderAll() {
  renderHero();
  renderHealth();
  renderDwell();
  renderQueue($('#queue'), app);
  renderEditor($('#editor'), app);
  renderSaveBar();
}

app.rerender = (what) => {
  if (what === 'queue') { renderQueue($('#queue'), app); renderEditor($('#editor'), app); }
  if (what === 'editor') renderEditor($('#editor'), app);
};

// ---- actions ---------------------------------------------------------------

app.update = (fn, opts = {}) => {
  app.draft = fn(app.draft);
  app.dirty = F.isDirty(app.draft, app.saved);
  if (opts.clearAll) app.errors.clear();
  if (opts.clear) for (const p of [...app.errors.keys()]) if (p.startsWith(opts.clear)) app.errors.delete(p);
  if (app.save.phase !== 'saving') app.save = { phase: 'idle', message: '' };
  if (opts.rerender) app.rerender(opts.rerender);
  else if (opts.clear) applyErrors($('#editor'), app);
  renderDwell();
  renderSaveBar();
};

app.select = (id) => {
  app.selected = id;
  renderQueue($('#queue'), app);
  renderEditor($('#editor'), app);
  if (id) {
    const title = $('#editor-title');
    const reduce = matchMedia('(prefers-reduced-motion: reduce)').matches;
    title.scrollIntoView({ behavior: reduce ? 'auto' : 'smooth', block: 'start' });
    title.focus({ preventScroll: true });
  }
};

function notice(text, bad = false) {
  const el = $('#notice');
  el.textContent = text;
  el.classList.toggle('stale', bad);
}

async function loadScreens() {
  try {
    app.live = await api.screen();
    paintLive();
  } catch { /* the caption already explains a missing connection */ }
  for (const { id } of app.draft.pages) {
    try {
      app.screens.set(id, await api.screen(id));
      paintMini(app, id);
      if (app.selected === id) renderEditor($('#editor'), app);
    } catch { /* leave the blank paper; the next save or reload retries */ }
  }
}

function showFieldError(path, message) {
  app.errors.set(path, message);
  const page = pageForPath(path);
  if (page && app.selected !== page) app.select(page);
  else app.rerender('editor');
  renderDwell();
  requestAnimationFrame(() => {
    const el = document.getElementById(fieldId(path));
    if (!el) return;
    el.focus({ preventScroll: true });
    el.scrollIntoView({ block: 'nearest' });
  });
}

async function save() {
  const issues = F.checkFeed(app.draft, app.state.limits);
  if (issues.length) {
    app.errors = new Map(issues.map((i) => [i.path, i.message]));
    const n = issues.length;
    app.save = { phase: 'error', message: `Fix ${n === 1 ? 'the highlighted field' : `the ${n} highlighted fields`} before saving.` };
    showFieldError(issues[0].path, issues[0].message);
    renderSaveBar();
    return;
  }
  app.save = { phase: 'saving', message: '' };
  renderSaveBar();
  try {
    const saved = await api.saveFeed(F.toPutBody(app.draft));
    app.saved = saved;
    app.draft = F.clone(saved);
    app.dirty = false;
    app.errors.clear();
    app.save = { phase: 'saved', message: '' };
    renderAll();
    setTimeout(() => { if (app.save.phase === 'saved') { app.save = { phase: 'idle', message: '' }; renderSaveBar(); } }, 3000);
    app.state = await api.state();
    renderHero();
    renderHealth();
    await loadScreens();
  } catch (e) {
    const err = apiErrorFor(e.status, e.body);
    app.save = { phase: 'error', message: err.path ? "Couldn't save. Fix the highlighted field and try again." : err.message };
    if (err.path) showFieldError(err.path, err.message);
    renderSaveBar();
  }
}

app.showPage = async (id) => {
  notice(`Showing ${PAGE_NAMES[id]} on the gadget. The screen takes a few seconds to redraw.`);
  try {
    await api.show(id);
    app.state = await api.state();
    app.live = await api.screen();
    renderHero();
    notice(`${PAGE_NAMES[id]} is on the gadget now.`);
  } catch (e) {
    notice(apiErrorFor(e.status, e.body).message, true);
  }
};

async function refreshData() {
  notice('Asking the gadget to fetch everything again. This takes a minute or so.');
  try {
    await api.refresh();
    setTimeout(async () => {
      try { app.state = await api.state(); renderHealth(); notice('Data refreshed. The lines above show what came back.'); } catch { /* keep notice */ }
    }, 4000);
  } catch (e) {
    notice(apiErrorFor(e.status, e.body).message, true);
  }
}

async function start() {
  $('#retry').hidden = true;
  try {
    app.state = await api.state();
    if (app.state.screen) app.geom = app.state.screen;
    app.saved = await api.feed();
    app.draft = F.clone(app.saved);
  } catch (e) {
    $('#live-caption').textContent = apiErrorFor(e.status, e.body).message;
    $('#retry').hidden = false;
    return;
  }
  document.body.classList.remove('booting');
  renderAll();
  await loadScreens();
}

// ---- events ----------------------------------------------------------------

$('#save').addEventListener('click', save);
$('#discard').addEventListener('click', () => {
  app.draft = F.clone(app.saved);
  app.dirty = false;
  app.errors.clear();
  app.save = { phase: 'idle', message: '' };
  renderAll();
});
$('#refresh').addEventListener('click', refreshData);
$('#retry').addEventListener('click', start);
$('#f-dwellSec').addEventListener('input', (e) => {
  const min = Number(e.target.value);
  if (Number.isFinite(min) && e.target.value !== '') app.update((f) => F.setDwell(f, min * 60), { clear: 'dwellSec' });
});
window.addEventListener('beforeunload', (e) => { if (app.dirty) e.preventDefault(); });
let resizeTimer = 0;
window.addEventListener('resize', () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(() => { if (app.draft) { paintLive(); app.rerender('queue'); } }, 150);
});

start();

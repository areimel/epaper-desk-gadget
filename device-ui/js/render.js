// Painting the page from `app`. No state changes here, except the notice line.
import { $ } from './dom.js';
import { drawScreen, fitCanvas } from './screen.js';
import { sourceLine, calendarSkippedLine } from './errors.js';
import { heroCaption } from './caption.js';
import { renderQueue } from './queue.js';
import { renderEditor, DIRTY_NOTE } from './editors.js';

export function paintLive(app) {
  const canvas = $('#live');
  const wrap = $('.hero');
  fitCanvas(canvas, wrap.clientWidth - 24, app.geom, 3, window.devicePixelRatio || 1);
  if (app.live) { drawScreen(canvas, app.live, app.geom); canvas.classList.remove('loading'); }
}

export function renderHero(app) {
  $('#live-caption').textContent = heroCaption(app.state, app.saved);
  paintLive(app);
}

export function renderHealth(app) {
  const s = app.state;
  if (!s) return;
  const items = Object.entries(s.sources).map(([id, src]) => {
    const line = sourceLine(id, src, s.time);
    const li = document.createElement('li');
    li.className = line.ok ? 'ok' : 'stale';
    li.textContent = line.text;
    return li;
  });
  const skipped = calendarSkippedLine(s.calendarSkipped);
  const extra = (text) => {
    const li = document.createElement('li');
    li.className = 'stale';
    li.textContent = text;
    return li;
  };
  if (skipped) items.push(extra(skipped));
  if (!s.timeSynced) items.unshift(extra("The gadget hasn't set its clock yet, so it can't fetch anything. Check that the WiFi has internet access."));
  $('#sources').replaceChildren(...items);
  $('#device').textContent = `Version ${s.version} on a ${s.board}, at ${s.host} (${s.ip}).`;
  $('h1').textContent = s.name;
}

export function renderDwell(app) {
  const L = app.state.limits;
  const input = $('#f-dwellSec');
  input.min = Math.ceil(L.dwellMin / 60);
  input.max = Math.floor(L.dwellMax / 60);
  if (document.activeElement !== input && !app.dwellError) input.value = Math.round(app.draft.dwellSec / 60);
  $('#f-dwellSec-hint').textContent = `Between ${input.min} and ${input.max} minutes.`;
  const err = app.errors.get('dwellSec');
  $('#f-dwellSec-error').hidden = !err;
  $('#f-dwellSec-error').textContent = err || '';
  input.toggleAttribute('aria-invalid', !!err);
}

export function renderSaveBar(app) {
  const bar = $('#savebar');
  const { phase, message } = app.save;
  const visible = app.dirty || phase !== 'idle';
  bar.hidden = !visible;
  document.body.classList.toggle('has-savebar', visible);
  const text = phase === 'saving' ? 'Saving...'
    : phase === 'saved' ? (app.dirty ? 'Saved. Your newer changes are not saved yet.' : 'Saved')
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

export function renderAll(app) {
  renderHero(app);
  renderHealth(app);
  renderDwell(app);
  renderQueue($('#queue'), app);
  renderEditor($('#editor'), app);
  renderSaveBar(app);
}

export function notice(text, bad = false) {
  const el = $('#notice');
  el.textContent = text;
  el.classList.toggle('stale', bad);
}

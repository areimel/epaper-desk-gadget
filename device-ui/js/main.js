// Wiring: load state, feed and screens one request at a time, then render.
import { $ } from './dom.js';
import { createApi } from './api.js';
import { GEOM } from './screen.js';
import * as F from './feed.js';
import { apiErrorFor, fieldId, pageForPath, hasInput, fixMessage } from './errors.js';
import { renderQueue, paintMini, PAGE_NAMES } from './queue.js';
import { renderEditor, applyErrors, resetEditorState, paintEditorPreview } from './editors.js';
import { mergeAfterSave, discardTarget } from './saveflow.js';
import { keepFocus } from './focus.js';
import { configuredSources, waitForRefresh, refreshMessage } from './refresh.js';
import * as R from './render.js';

const api = createApi();
const app = {
  state: null, saved: null, draft: null, selected: null, geom: GEOM,
  screens: new Map(), live: null, errors: new Map(), dirty: false,
  dwellError: '', // text in the rotation field that is not a number
  sent: null, // the feed a save in flight is carrying
  save: { phase: 'idle', message: '' }, // idle | saving | saved | error
};
const idle = () => ({ phase: 'idle', message: '' });
const syncDirty = () => { app.dirty = F.isDirty(app.draft, app.saved) || !!app.dwellError; };

// ---- actions ---------------------------------------------------------------

// The one way to re-render: whatever had focus keeps it, or `focusId` gets it.
app.rerender = (what, focusId) => keepFocus(() => {
  if (what === 'all') return R.renderAll(app);
  if (what === 'queue') renderQueue($('#queue'), app);
  renderEditor($('#editor'), app);
}, { focusId });

app.update = (fn, opts = {}) => {
  app.draft = fn(app.draft);
  syncDirty();
  if (opts.clearAll) app.errors.clear();
  if (opts.clear) for (const p of [...app.errors.keys()]) if (p.startsWith(opts.clear)) app.errors.delete(p);
  if (app.save.phase !== 'saving') app.save = idle();
  if (opts.rerender) app.rerender(opts.rerender, opts.focus);
  else if (opts.clear) applyErrors($('#editor'), app);
  R.renderDwell(app);
  R.renderSaveBar(app);
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

async function loadScreens() {
  try {
    app.live = await api.screen();
    R.paintLive(app);
  } catch { /* the caption already explains a missing connection */ }
  for (const { id } of app.draft.pages) {
    try {
      app.screens.set(id, await api.screen(id));
      paintMini(app, id);
      paintEditorPreview(app, id); // the preview canvas only; never the form
    } catch { /* leave the blank paper; the next save or reload retries */ }
  }
}

// Shows a message next to the field a path names. Returns false when the
// screen has no such field, so the caller can say it in the save bar instead.
function showFieldError(path, message) {
  app.errors.set(path, message);
  const page = pageForPath(path);
  if (page && app.selected !== page) app.select(page);
  else app.rerender('editor');
  R.renderDwell(app);
  const el = document.getElementById(fieldId(path));
  if (el) {
    requestAnimationFrame(() => {
      el.focus({ preventScroll: true });
      el.scrollIntoView({ block: 'nearest' });
    });
  }
  return !!el;
}

async function save() {
  if (app.save.phase === 'saving') return;
  const issues = F.checkFeed(app.draft, app.state.limits);
  if (app.dwellError) issues.unshift({ path: 'dwellSec', message: app.dwellError });
  if (issues.length) {
    app.errors = new Map(issues.map((i) => [i.path, i.message]));
    const found = showFieldError(issues[0].path, issues[0].message);
    const has = (p) => hasInput(p) && (p !== issues[0].path || found);
    app.save = { phase: 'error', message: fixMessage(issues, has) };
    R.renderSaveBar(app);
    return;
  }
  const sent = F.clone(app.draft); // typing can go on while the device redraws
  app.sent = sent;
  app.save = { phase: 'saving', message: '' };
  R.renderSaveBar(app);
  let saved;
  try {
    saved = await api.saveFeed(F.toPutBody(sent));
  } catch (e) {
    app.sent = null;
    const err = apiErrorFor(e.status, e.body);
    app.save = { phase: 'error', message: err.message };
    if (err.path && showFieldError(err.path, err.message)) {
      app.save.message = "Couldn't save. Fix the highlighted field and try again.";
    }
    R.renderSaveBar(app);
    return;
  }
  app.sent = null;
  const merged = mergeAfterSave({ sent, saved, draft: app.draft });
  app.saved = saved;
  app.draft = merged.draft;
  syncDirty();
  app.errors.clear();
  if (app.dwellError) app.errors.set('dwellSec', app.dwellError);
  resetEditorState();
  app.save = { phase: 'saved', message: '' };
  app.rerender('all');
  setTimeout(() => { if (app.save.phase === 'saved') { app.save = idle(); R.renderSaveBar(app); } }, 3000);
  await refreshAfterSave(); // its own result; the save is already reported
}

async function refreshAfterSave() {
  try {
    app.state = await api.state();
    R.renderHero(app);
    R.renderHealth(app);
  } catch (e) {
    R.notice(`Saved, but the gadget's status didn't update. ${apiErrorFor(e.status, e.body).message}`, true);
  }
  await loadScreens();
}

function discard() {
  const target = discardTarget({ saving: app.save.phase === 'saving', sent: app.sent, saved: app.saved });
  app.draft = F.clone(target);
  app.dwellError = '';
  app.errors.clear();
  syncDirty();
  if (app.save.phase !== 'saving') app.save = idle();
  resetEditorState();
  app.rerender('all');
}

app.showPage = async (id) => {
  const say = (text, bad = false) => {
    const el = $('#show-status');
    if (el) { el.textContent = text; el.classList.toggle('stale', bad); }
  };
  say('Showing it now. The gadget takes a few seconds to redraw.');
  try {
    await api.show(id);
    app.state = await api.state();
    app.live = await api.screen();
    R.renderHero(app);
    say(`${PAGE_NAMES[id]} is on the gadget now.`);
  } catch (e) {
    say(apiErrorFor(e.status, e.body).message, true);
  }
};

async function refreshData() {
  const button = $('#refresh');
  button.disabled = true;
  R.notice('Asking the gadget to fetch everything again. This can take up to half a minute.');
  try {
    const before = (await api.state()).sources;
    await api.refresh();
    const result = await waitForRefresh({
      fetchState: () => api.state(), sleep: (ms) => new Promise((r) => setTimeout(r, ms)),
      before, ids: configuredSources(app.saved),
    });
    if (result.state) { app.state = result.state; R.renderHealth(app); }
    const m = refreshMessage(result);
    R.notice(m.text, m.bad);
  } catch (e) {
    R.notice(apiErrorFor(e.status, e.body).message, true);
  } finally {
    button.disabled = false;
  }
}

async function start() {
  $('#retry').hidden = true;
  R.paintLive(app); // size the empty screen now so the page doesn't jump later
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
  R.renderAll(app);
  await loadScreens();
}

// ---- events ----------------------------------------------------------------

$('#save').addEventListener('click', save);
$('#discard').addEventListener('click', discard);
$('#refresh').addEventListener('click', refreshData);
$('#retry').addEventListener('click', start);
$('#f-dwellSec').addEventListener('input', (e) => {
  const r = F.parseDwellMinutes(e.target.value);
  app.dwellError = r.error || '';
  if (r.error) {
    app.errors.set('dwellSec', r.error);
    app.update((f) => f); // shows the error and keeps Save available to explain it
  } else {
    app.update((f) => F.setDwell(f, r.sec), { clear: 'dwellSec' });
  }
});
window.addEventListener('beforeunload', (e) => { if (app.dirty) e.preventDefault(); });
let resizeTimer = 0;
window.addEventListener('resize', () => {
  clearTimeout(resizeTimer);
  resizeTimer = setTimeout(() => { if (app.draft) { R.paintLive(app); app.rerender('queue'); } }, 150);
});

start();

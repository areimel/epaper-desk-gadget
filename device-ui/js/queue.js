// The page queue: real mini-screens in rotation order, reorderable by drag,
// by the move buttons, or with Alt+Arrow keys on a focused screen.
import { h, iconButton } from './dom.js';
import { drawScreen, fitCanvas } from './screen.js';
import { movePage, movePageBy, togglePage } from './feed.js';

export const PAGE_NAMES = {
  clock: 'Clock', weather: 'Weather', hn: 'Hacker News',
  messages: 'Messages', calendar: 'Calendar', notes: 'Notes',
};

let dragFrom = -1;

export function paintScreen(canvas, buf, geom, availCss) {
  fitCanvas(canvas, availCss, geom, 1, window.devicePixelRatio || 1);
  if (buf) {
    drawScreen(canvas, buf, geom);
    canvas.classList.remove('loading');
  }
}

// Width a mini-screen may use: half the queue minus the gap, the highlight
// padding and the outline; at most the panel's own size.
function miniWidth(root) {
  const w = root.clientWidth || 358;
  return Math.min(250, (w - 16) / 2 - 16);
}

function move(app, id, delta) {
  app.update((f) => movePageBy(f, id, delta), { rerender: 'queue' });
  requestAnimationFrame(() => {
    const btn = document.querySelector(`[data-page="${id}"] .mini`);
    if (btn) btn.focus();
  });
}

function item(app, page, index, count, avail) {
  const { id, on } = page;
  const name = PAGE_NAMES[id] || id;
  const selected = app.selected === id;
  const canvas = h('canvas', { class: 'screen loading', 'aria-hidden': 'true', width: 250, height: 122 });
  paintScreen(canvas, app.screens.get(id), app.geom, avail);

  const mini = h('button', {
    type: 'button', class: 'mini', 'aria-pressed': selected ? 'true' : 'false',
    'aria-label': `${index + 1}. ${name}, ${on ? 'on' : 'off'}. Open settings`,
    onclick: () => app.select(selected ? null : id),
    onkeydown: (e) => {
      if (!e.altKey) return;
      if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') { e.preventDefault(); move(app, id, -1); }
      if (e.key === 'ArrowRight' || e.key === 'ArrowDown') { e.preventDefault(); move(app, id, 1); }
    },
  }, canvas);

  const toggle = h('input', {
    type: 'checkbox', class: 'switch', id: `on-${id}`, checked: on, role: 'switch',
    onchange: (e) => app.update((f) => togglePage(f, id, e.target.checked), { rerender: 'queue' }),
  });

  const li = h('li', {
    class: `page${selected ? ' selected' : ''}${on ? '' : ' off'}`, 'data-page': id, draggable: 'true',
    ondragstart: (e) => {
      dragFrom = index;
      e.dataTransfer.effectAllowed = 'move';
      e.dataTransfer.setData('text/plain', id);
      li.classList.add('dragging');
    },
    ondragend: () => { li.classList.remove('dragging'); dragFrom = -1; },
    ondragover: (e) => { if (dragFrom >= 0) { e.preventDefault(); li.classList.add('drop'); } },
    ondragleave: () => li.classList.remove('drop'),
    ondrop: (e) => {
      e.preventDefault();
      li.classList.remove('drop');
      if (dragFrom >= 0 && dragFrom !== index) {
        const from = dragFrom;
        app.update((f) => movePage(f, from, index), { rerender: 'queue' });
      }
    },
  },
  mini,
  h('div', { class: 'page-row' },
    h('label', { class: 'page-name', for: `on-${id}` }, toggle,
      h('span', {}, h('span', { class: 'order' }, `${index + 1} `), name)),
    h('span', { class: 'moves' },
      iconButton(`Move ${name} earlier`, '←', () => move(app, id, -1), { disabled: index === 0 }),
      iconButton(`Move ${name} later`, '→', () => move(app, id, 1), { disabled: index === count - 1 }))));
  return li;
}

export function renderQueue(root, app) {
  const pages = app.draft.pages;
  const avail = miniWidth(root);
  const list = h('ol', { class: 'queue', 'aria-describedby': 'queue-help' },
    pages.map((p, i) => item(app, p, i, pages.length, avail)));
  const onCount = pages.filter((p) => p.on).length;
  const empty = onCount === 0
    ? h('p', { class: 'warn' }, 'Every page is off, so the gadget has nothing to show. Switch on at least one page.')
    : null;
  root.replaceChildren(list, empty || '');
}

// Repaint one mini-screen in place once its bitmap arrives.
export function paintMini(app, id) {
  const canvas = document.querySelector(`[data-page="${id}"] canvas`);
  const root = document.getElementById('queue');
  if (canvas && root) paintScreen(canvas, app.screens.get(id), app.geom, miniWidth(root));
}

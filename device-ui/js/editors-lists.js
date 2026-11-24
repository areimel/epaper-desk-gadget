// List editors: messages and quotes, notes and to-dos. Limits come from
// /api/state and are counted in UTF-8 bytes.
import { h, field, counter, iconButton } from './dom.js';
import { fieldId } from './errors.js';
import * as F from './feed.js';

function focusSoon(id) {
  requestAnimationFrame(() => document.getElementById(id)?.focus());
}

// Move up, move down and remove buttons for row i of n.
function rowTools(app, what, i, n, { move, remove, focusId }) {
  const go = (fn, focus) => { app.update(fn, { rerender: 'editor', clearAll: true }); focusSoon(focus); };
  return h('div', { class: 'row-tools' },
    iconButton(`Move ${what} ${i + 1} up`, '↑', () => go((f) => move(f, i, i - 1), focusId(i - 1)), { disabled: i === 0 }),
    iconButton(`Move ${what} ${i + 1} down`, '↓', () => go((f) => move(f, i, i + 1), focusId(i + 1)), { disabled: i === n - 1 }),
    h('button', { type: 'button', class: 'remove', onclick: () => go((f) => remove(f, i), 'add-row') }, 'Remove'));
}

function addButton(app, label, fullText, ok, fn, focusId) {
  return h('div', { class: 'add' },
    h('button', {
      type: 'button', id: 'add-row', disabled: !ok,
      onclick: () => { app.update(fn, { rerender: 'editor' }); focusSoon(focusId); },
    }, label),
    ok ? null : h('p', { class: 'hint' }, fullText));
}

export function messagesEditor(app) {
  const f = app.draft;
  const L = app.state.limits;
  const n = f.messages.length;
  const tools = { move: F.moveMessage, remove: F.removeMessage, focusId: (i) => fieldId(`messages[${i}].text`) };
  const rows = f.messages.map((m, i) => {
    const textPath = `messages[${i}].text`, byPath = `messages[${i}].by`;
    const text = h('textarea', {
      rows: 2, value: m.text,
      oninput: (e) => app.update((d) => F.editMessage(d, i, { text: e.target.value }), { clear: textPath }),
    });
    const by = h('input', {
      type: 'text', value: m.by, autocomplete: 'off',
      oninput: (e) => app.update((d) => F.editMessage(d, i, { by: e.target.value }), { clear: byPath }),
    });
    const t = field({ id: fieldId(textPath), label: `Message ${i + 1}`, input: text });
    t.wrap.querySelector('label').append(counter(text, L.messageTextMax, F.utf8Bytes));
    const b = field({ id: fieldId(byPath), label: 'By (optional)', input: by });
    return h('li', { class: 'item' }, t.wrap, h('div', { class: 'item-foot' }, b.wrap, rowTools(app, 'message', i, n, tools)));
  });
  return [
    h('p', { class: 'hint' }, 'The gadget shows one message per visit, in this order. It draws Latin letters only, so emoji show as question marks and use more of the space.'),
    n ? h('ol', { class: 'items' }, rows)
      : h('p', { class: 'empty' }, 'No messages yet. Add a kind word or a favourite quote and the gadget will show it.'),
    addButton(app, 'Add a message', `The gadget holds up to ${L.messagesMax} messages. Remove one to add another.`,
      F.canAdd(f, L, 'message'), F.addMessage, fieldId(`messages[${n}].text`)),
  ];
}

export function notesEditor(app) {
  const f = app.draft;
  const L = app.state.limits;
  const lines = f.notes.lines;
  const n = lines.length;
  const title = h('input', {
    type: 'text', value: f.notes.title, autocomplete: 'off', placeholder: 'Today',
    oninput: (e) => app.update((d) => F.setNotesTitle(d, e.target.value), { clear: 'notes.title' }),
  });
  const t = field({ id: fieldId('notes.title'), label: 'Title', input: title });
  t.wrap.querySelector('label').append(counter(title, L.noteTitleMax, F.utf8Bytes));
  const tools = { move: F.moveNoteLine, remove: F.removeNoteLine, focusId: (i) => fieldId(`notes.lines[${i}].text`) };

  const rows = lines.map((l, i) => {
    const path = `notes.lines[${i}].text`;
    const text = h('input', {
      type: 'text', value: l.text, autocomplete: 'off',
      oninput: (e) => app.update((d) => F.editNoteLine(d, i, { text: e.target.value }), { clear: path }),
    });
    const fl = field({ id: fieldId(path), label: `Line ${i + 1}`, input: text });
    fl.wrap.querySelector('label').append(counter(text, L.noteTextMax, F.utf8Bytes));
    const check = (key, label, checked, disabled) => h('label', { class: 'check' },
      h('input', {
        type: 'checkbox', checked, disabled,
        onchange: (e) => {
          const on = e.target.checked;
          app.update((d) => F.editNoteLine(d, i, { [key]: on }), { rerender: key === 'todo' ? 'editor' : '' });
          if (key === 'done') e.target.closest('li').classList.toggle('done', on);
        },
      }), label);
    return h('li', { class: `item${l.todo && l.done ? ' done' : ''}` }, fl.wrap,
      h('div', { class: 'item-foot' },
        h('div', { class: 'checks' },
          check('todo', 'To-do box', l.todo, false),
          l.todo ? check('done', 'Done', l.done, false) : null),
        rowTools(app, 'line', i, n, tools)));
  });
  return [
    t.wrap,
    n ? h('ol', { class: 'items' }, rows)
      : h('p', { class: 'empty' }, 'No lines yet. Add a reminder or a to-do for the day.'),
    addButton(app, 'Add a line', `The notes page fits ${L.noteLinesMax} lines. Remove one to add another.`,
      F.canAdd(f, L, 'note'), F.addNoteLine, fieldId(`notes.lines[${n}].text`)),
  ];
}

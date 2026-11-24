import { test } from 'node:test';
import assert from 'node:assert/strict';
import { keepFocus } from '../js/focus.js';

function fakeDoc(initial) {
  const els = new Map();
  const doc = {
    activeElement: null,
    getElementById: (id) => els.get(id) || null,
    mount(ids) {
      els.clear();
      for (const id of ids) {
        const el = { id, focused: 0, focus() { this.focused++; doc.activeElement = this; } };
        els.set(id, el);
      }
    },
  };
  doc.mount(initial);
  return doc;
}

test('focus returns to the element with the same id after a re-render', () => {
  const doc = fakeDoc(['on-clock', 'other']);
  doc.activeElement = doc.getElementById('on-clock');
  keepFocus(() => doc.mount(['on-clock', 'other']), { doc });
  assert.equal(doc.activeElement.id, 'on-clock');
  assert.equal(doc.activeElement.focused, 1);
});

test('nothing focused, nothing restored', () => {
  const doc = fakeDoc(['a']);
  keepFocus(() => doc.mount(['a']), { doc });
  assert.equal(doc.getElementById('a').focused, 0);
});

test('an explicit id wins; a missing id falls back to the old one', () => {
  const doc = fakeDoc(['old']);
  doc.activeElement = doc.getElementById('old');
  keepFocus(() => doc.mount(['old', 'new']), { doc, focusId: 'new' });
  assert.equal(doc.activeElement.id, 'new');
  doc.mount(['old']);
  doc.activeElement = doc.getElementById('old');
  keepFocus(() => doc.mount(['old']), { doc, focusId: 'gone' });
  assert.equal(doc.activeElement.id, 'old');
  assert.equal(doc.activeElement.focused, 1);
});

test('elements without an id are ignored, and a vanished id is not an error', () => {
  const doc = fakeDoc(['a']);
  doc.activeElement = { id: '' };
  assert.doesNotThrow(() => keepFocus(() => doc.mount([]), { doc }));
  doc.activeElement = { id: 'a' };
  assert.doesNotThrow(() => keepFocus(() => doc.mount([]), { doc }));
});

test('the render result is returned', () => {
  assert.equal(keepFocus(() => 7, { doc: fakeDoc([]) }), 7);
});

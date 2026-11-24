import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mergeAfterSave, discardTarget } from '../js/saveflow.js';

const feed = (over = {}) => ({
  v: 1, dwellSec: 300, tz: { iana: 'UTC', posix: 'UTC0' }, pages: [], weather: null,
  calendar: { set: false }, messages: [{ text: 'a', by: '' }], notes: { title: '', lines: [] }, ...over,
});

test('untouched draft is replaced by what the device stored', () => {
  const sent = feed();
  const saved = feed({ dwellSec: 360 });
  const r = mergeAfterSave({ sent, saved, draft: structuredClone(sent) });
  assert.deepEqual(r.draft, saved);
  assert.equal(r.dirty, false);
  assert.equal(r.edited, false);
});

test('edits made during the save are kept and stay unsaved', () => {
  const sent = feed();
  const saved = feed();
  const draft = feed({ messages: [{ text: 'a, more', by: '' }] });
  const r = mergeAfterSave({ sent, saved, draft });
  assert.equal(r.draft.messages[0].text, 'a, more');
  assert.equal(r.dirty, true);
  assert.equal(r.edited, true);
});

test('dirty is worked out against the new saved copy', () => {
  // The device stored what was sent, and the draft matches that copy again.
  const sent = feed({ dwellSec: 600 });
  const saved = feed({ dwellSec: 600 });
  const r = mergeAfterSave({ sent, saved, draft: feed({ dwellSec: 900 }) });
  assert.equal(r.dirty, true);
  // The device normalised the value to what the draft already holds.
  const r2 = mergeAfterSave({ sent, saved: feed({ dwellSec: 900 }), draft: feed({ dwellSec: 900 }) });
  assert.equal(r2.dirty, false);
  assert.equal(r2.edited, true);
});

test('a calendar link that was sent is not kept as a pending change', () => {
  const sent = feed({ calendar: { set: false, url: 'https://x.test/a.ics' } });
  const saved = feed({ calendar: { set: true } });
  const draft = feed({ calendar: { set: false, url: 'https://x.test/a.ics' }, dwellSec: 900 });
  const r = mergeAfterSave({ sent, saved, draft });
  assert.deepEqual(r.draft.calendar, { set: true });
  assert.equal(r.draft.dwellSec, 900);
  assert.equal(r.dirty, true);
});

test('a calendar link edited during the save stays pending', () => {
  const sent = feed({ calendar: { set: true, url: 'https://x.test/a.ics' } });
  const saved = feed({ calendar: { set: true } });
  const draft = feed({ calendar: { set: true, url: 'https://x.test/b.ics' } });
  assert.equal(mergeAfterSave({ sent, saved, draft }).draft.calendar.url, 'https://x.test/b.ics');
});

test('does not mutate its inputs', () => {
  const sent = feed(), saved = feed(), draft = feed({ dwellSec: 900 });
  const before = JSON.stringify([sent, saved, draft]);
  mergeAfterSave({ sent, saved, draft });
  assert.equal(JSON.stringify([sent, saved, draft]), before);
});

test('discard goes back to the saved copy, or to what was sent while saving', () => {
  const sent = feed({ dwellSec: 600 }), saved = feed();
  assert.equal(discardTarget({ saving: false, sent: null, saved }), saved);
  assert.equal(discardTarget({ saving: true, sent, saved }), sent);
});

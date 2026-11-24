import { test } from 'node:test';
import assert from 'node:assert/strict';
import { apiErrorFor, fieldId, sourceLine, calendarSkippedLine, ageText, pageForPath } from '../js/errors.js';

test('pageForPath finds the page whose settings hold a field', () => {
  assert.equal(pageForPath('messages[3].text'), 'messages');
  assert.equal(pageForPath('notes.lines[0].done'), 'notes');
  assert.equal(pageForPath('tz.posix'), 'clock');
  assert.equal(pageForPath('weather'), 'weather');
  assert.equal(pageForPath('dwellSec'), '');
  assert.equal(pageForPath(''), '');
});

test('400 keeps the server copy and its path', () => {
  const e = apiErrorFor(400, { error: 'Message 4 is 152 bytes; the limit is 140.', path: 'messages[3].text' });
  assert.deepEqual(e, { path: 'messages[3].text', message: 'Message 4 is 152 bytes; the limit is 140.' });
});

test('400 without a body still says what to do', () => {
  const e = apiErrorFor(400, null);
  assert.equal(e.path, '');
  assert.match(e.message, /didn't accept/);
});

test('413, 500, 404, network and timeout map to plain copy', () => {
  assert.match(apiErrorFor(413, {}).message, /too large/i);
  assert.match(apiErrorFor(500, {}).message, /previous feed/i);
  assert.match(apiErrorFor(404, { error: 'There is no page called "x".', path: 'page' }).message, /no page/);
  assert.match(apiErrorFor(0, null).message, /reach the gadget/i);
  assert.match(apiErrorFor('timeout', null).message, /didn't answer/i);
});

test('fieldId maps paths to DOM ids', () => {
  assert.equal(fieldId('messages[3].text'), 'f-messages-3-text');
  assert.equal(fieldId('notes.lines[0].text'), 'f-notes-lines-0-text');
  assert.equal(fieldId('dwellSec'), 'f-dwellSec');
  assert.equal(fieldId(''), '');
});

const NOW = 1795452060;

test('ageText is short and plain', () => {
  assert.equal(ageText(NOW, NOW - 20), 'just now');
  assert.equal(ageText(NOW, NOW - 11 * 60), '11 min ago');
  assert.equal(ageText(NOW, NOW - 3 * 3600 - 5), '3 h ago');
  assert.equal(ageText(NOW, NOW - 30 * 3600), 'yesterday');
  assert.equal(ageText(NOW, NOW - 4 * 86400), '4 days ago');
});

test('sourceLine says how each source is doing', () => {
  const ok = sourceLine('weather', { state: 'ok', updated: NOW - 660, error: '' }, NOW);
  assert.deepEqual(ok, { ok: true, text: 'Weather: updated 11 min ago' });
  const stale = sourceLine('hn', { state: 'stale', updated: NOW - 4060, error: 'no network' }, NOW);
  assert.equal(stale.ok, false);
  assert.equal(stale.text, 'Hacker News: showing older headlines from 1 h ago, no network');
  const never = sourceLine('calendar', { state: 'never', updated: 0, error: '' }, NOW);
  assert.equal(never.text, 'Calendar: not loaded yet');
  const err = sourceLine('calendar', { state: 'error', updated: 0, error: 'bad address' }, NOW);
  assert.equal(err.ok, false);
  assert.equal(err.text, "Calendar: didn't load, bad address. Check the calendar link.");
});

test('calendarSkippedLine is empty for zero and plural-aware otherwise', () => {
  assert.equal(calendarSkippedLine(0), '');
  assert.match(calendarSkippedLine(1), /^1 event today is left out/);
  assert.match(calendarSkippedLine(3), /^3 events today are left out/);
});

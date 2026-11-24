import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { validateFeed } from '../mock/validate.mjs';

const dir = new URL('../mock/fixtures/', import.meta.url);
const feed = () => JSON.parse(readFileSync(new URL('feed.json', dir), 'utf8'));
const limits = JSON.parse(readFileSync(new URL('state.json', dir), 'utf8')).limits;
const check = (mut) => {
  const f = feed();
  mut(f);
  return validateFeed(f, limits);
};

test('fixture feed is valid', () => assert.deepEqual(validateFeed(feed(), limits), { ok: true }));
test('non-object rejected', () => assert.equal(validateFeed([], limits).ok, false));
test('v 2 invalid', () => assert.equal(check((f) => { f.v = 2; }).path, 'v'));
test('unknown key', () => {
  const r = check((f) => { f.extra = 1; });
  assert.equal(r.ok, false);
  assert.equal(r.path, 'extra');
});
test('dwell bounds', () => {
  assert.equal(check((f) => { f.dwellSec = 179; }).path, 'dwellSec');
  assert.equal(check((f) => { f.dwellSec = 3601; }).path, 'dwellSec');
  assert.equal(check((f) => { f.dwellSec = 300.5; }).path, 'dwellSec');
  assert.equal(check((f) => { f.dwellSec = 180; }).ok, true);
});
test('missing field names its path', () => assert.equal(check((f) => { delete f.notes; }).path, 'notes'));
test('tz limits', () => {
  assert.equal(check((f) => { f.tz.posix = ''; }).path, 'tz.posix');
  assert.equal(check((f) => { f.tz.iana = 'x'.repeat(49); }).path, 'tz.iana');
});
test('unknown page id', () => assert.equal(check((f) => { f.pages[2].id = 'bogus'; }).path, 'pages[2].id'));
test('duplicate page id', () => assert.equal(check((f) => { f.pages[3].id = 'clock'; }).ok, false));
test('missing page id', () => assert.equal(check((f) => { f.pages.pop(); }).ok, false));
test('page on must be boolean', () => assert.equal(check((f) => { f.pages[0].on = 1; }).path, 'pages[0].on'));
test('weather rules', () => {
  assert.equal(check((f) => { f.weather = null; }).ok, true);
  assert.equal(check((f) => { f.weather.lat = 91; }).path, 'weather.lat');
  assert.equal(check((f) => { f.weather.units = 'k'; }).path, 'weather.units');
  assert.equal(check((f) => { f.weather.place = ''; }).path, 'weather.place');
});
test('message length counts bytes, not characters', () => {
  const ok = 'é😀'.repeat(20); // 120 bytes
  assert.equal(check((f) => { f.messages[0].text = ok; }).ok, true);
  const long = 'é😀'.repeat(24); // 48 UTF-16 units, 144 bytes
  assert.ok(long.length < 140);
  const r = check((f) => { f.messages[0].text = long; });
  assert.equal(r.ok, false);
  assert.equal(r.path, 'messages[0].text');
  assert.match(r.error, /144 bytes/);
  assert.match(r.error, /140/);
});
test('too many messages', () => {
  const r = check((f) => { f.messages = Array.from({ length: 41 }, () => ({ text: 'a', by: '' })); });
  assert.equal(r.path, 'messages');
});
test('note lines over limit', () => {
  const r = check((f) => { f.notes.lines = Array.from({ length: 6 }, () => ({ text: 'a', todo: false, done: false })); });
  assert.equal(r.path, 'notes.lines');
});
test('calendar url rules', () => {
  assert.equal(check((f) => { f.calendar = { url: 'http://x.org/a.ics' }; }).path, 'calendar.url');
  assert.equal(check((f) => { f.calendar = { url: 'webcal://x.org/a.ics' }; }).ok, true);
  assert.equal(check((f) => { f.calendar = { url: '' }; }).ok, true);
  assert.equal(check((f) => { f.calendar = { url: 5 }; }).path, 'calendar.url');
});

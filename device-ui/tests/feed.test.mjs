import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import * as F from '../js/feed.js';

const fixture = JSON.parse(readFileSync(new URL('../mock/fixtures/feed.json', import.meta.url)));
const limits = JSON.parse(readFileSync(new URL('../mock/fixtures/state.json', import.meta.url))).limits;
const ids = (feed) => feed.pages.map((p) => p.id);

test('utf8Bytes counts bytes, not characters', () => {
  assert.equal(F.utf8Bytes('abc'), 3);
  assert.equal(F.utf8Bytes('é'), 2);
  assert.equal(F.utf8Bytes('😀'), 4);
  assert.equal(F.utf8Bytes(''), 0);
});

test('edits never mutate the input', () => {
  const before = JSON.stringify(fixture);
  F.movePage(fixture, 0, 3);
  F.togglePage(fixture, 'clock', false);
  F.addMessage(fixture);
  F.editNoteLine(fixture, 0, { done: true });
  assert.equal(JSON.stringify(fixture), before);
});

test('movePage reorders and clamps', () => {
  assert.deepEqual(ids(F.movePage(fixture, 0, 2)),
    ['weather', 'hn', 'clock', 'messages', 'calendar', 'notes']);
  assert.deepEqual(ids(F.movePage(fixture, 5, 0)),
    ['notes', 'clock', 'weather', 'hn', 'messages', 'calendar']);
  assert.deepEqual(ids(F.movePage(fixture, 0, -1)), ids(fixture));
  assert.deepEqual(ids(F.movePage(fixture, 5, 9)), ids(fixture));
});

test('movePageBy moves one step by id', () => {
  assert.deepEqual(ids(F.movePageBy(fixture, 'hn', -1)).slice(0, 3), ['clock', 'hn', 'weather']);
  assert.deepEqual(ids(F.movePageBy(fixture, 'clock', -1)), ids(fixture));
});

test('togglePage switches one page', () => {
  const f = F.togglePage(fixture, 'calendar', true);
  assert.equal(f.pages.find((p) => p.id === 'calendar').on, true);
  assert.equal(F.togglePage(f, 'calendar', false).pages[4].on, false);
});

test('messages: add, edit, remove, move', () => {
  let f = F.addMessage(fixture);
  assert.equal(f.messages.length, 4);
  assert.deepEqual(f.messages[3], { text: '', by: '' });
  f = F.editMessage(f, 3, { text: 'Hi', by: 'me' });
  assert.deepEqual(f.messages[3], { text: 'Hi', by: 'me' });
  f = F.moveMessage(f, 3, 0);
  assert.equal(f.messages[0].text, 'Hi');
  f = F.removeMessage(f, 0);
  assert.deepEqual(f.messages, fixture.messages);
});

test('notes: title and lines', () => {
  let f = F.setNotesTitle(fixture, 'Week');
  assert.equal(f.notes.title, 'Week');
  f = F.addNoteLine(f);
  assert.deepEqual(f.notes.lines[3], { text: '', todo: true, done: false });
  f = F.editNoteLine(f, 3, { text: 'Water plants' });
  f = F.moveNoteLine(f, 3, 0);
  assert.equal(f.notes.lines[0].text, 'Water plants');
  f = F.editNoteLine(f, 1, { todo: false });
  assert.equal(f.notes.lines[1].done, false, 'turning off todo clears done');
  f = F.removeNoteLine(f, 0);
  assert.equal(f.notes.lines.length, 3);
});

test('canAdd respects limits from /api/state', () => {
  assert.equal(F.canAdd(fixture, limits, 'message'), true);
  const tight = { ...limits, messagesMax: 3, noteLinesMax: 3 };
  assert.equal(F.canAdd(fixture, tight, 'message'), false);
  assert.equal(F.canAdd(fixture, tight, 'note'), false);
});

test('checkFeed passes the fixture', () => {
  assert.deepEqual(F.checkFeed(fixture, limits), []);
});

test('checkFeed counts message text in bytes', () => {
  const text = 'é'.repeat(71); // 71 characters, 142 bytes
  const issues = F.checkFeed(F.editMessage(fixture, 1, { text }), limits);
  assert.equal(issues.length, 1);
  assert.equal(issues[0].path, 'messages[1].text');
  assert.match(issues[0].message, /142.*140|140.*142/);
});

test('checkFeed flags empty message, long by, long note, dwell range, title', () => {
  let f = F.editMessage(fixture, 0, { text: '' });
  f = F.editMessage(f, 2, { by: 'x'.repeat(limits.messageByMax + 1) });
  f = F.editNoteLine(f, 0, { text: 'y'.repeat(limits.noteTextMax + 1) });
  f = F.setNotesTitle(f, 'z'.repeat(limits.noteTitleMax + 1));
  f = F.setDwell(f, limits.dwellMin - 1);
  const paths = F.checkFeed(f, limits).map((i) => i.path).sort();
  assert.deepEqual(paths, ['dwellSec', 'messages[0].text', 'messages[2].by',
    'notes.lines[0].text', 'notes.title'].sort());
});

test('checkFeed checks calendar link only when changed', () => {
  assert.deepEqual(F.checkFeed(F.setCalendarUrl(fixture, ''), limits), []);
  const bad = F.checkFeed(F.setCalendarUrl(fixture, 'http://x'), limits);
  assert.equal(bad[0].path, 'calendar.url');
  assert.deepEqual(F.checkFeed(F.setCalendarUrl(fixture, 'webcal://x/y.ics'), limits), []);
});

test('setDwell rounds to whole seconds', () => {
  assert.equal(F.setDwell(fixture, 300.6).dwellSec, 301);
});

test('setTz, setPlace, setUnits', () => {
  let f = F.setTz(fixture, 'Europe/London', 'GMT0BST,M3.5.0/1,M10.5.0');
  assert.deepEqual(f.tz, { iana: 'Europe/London', posix: 'GMT0BST,M3.5.0/1,M10.5.0' });
  f = F.setPlace({ ...f, weather: null }, { place: 'Leeds', lat: 53.8, lon: -1.55 });
  assert.deepEqual(f.weather, { place: 'Leeds', lat: 53.8, lon: -1.55, units: 'c' });
  f = F.setUnits(f, 'f');
  assert.equal(f.weather.units, 'f');
  assert.equal(F.setUnits({ ...f, weather: null }, 'c').weather, null);
});

test('toPutBody keeps the stored calendar link unless changed', () => {
  assert.deepEqual(F.toPutBody(fixture).calendar, {});
  assert.deepEqual(F.toPutBody(F.setCalendarUrl(fixture, 'https://a/b.ics')).calendar,
    { url: 'https://a/b.ics' });
  assert.deepEqual(F.toPutBody(F.setCalendarUrl(fixture, '')).calendar, { url: '' });
  const body = F.toPutBody(fixture);
  assert.equal(body.v, 1);
  assert.deepEqual(body.messages, fixture.messages);
});

test('isDirty compares content', () => {
  assert.equal(F.isDirty(fixture, F.clone(fixture)), false);
  assert.equal(F.isDirty(fixture, F.togglePage(fixture, 'hn', false)), true);
});

test('pageOn and calendarState helpers', () => {
  assert.equal(F.pageOn(fixture, 'calendar'), false);
  assert.equal(F.calendarState(fixture), 'none');
  assert.equal(F.calendarState({ ...fixture, calendar: { set: true } }), 'saved');
  assert.equal(F.calendarState(F.setCalendarUrl(fixture, 'https://x')), 'new');
  assert.equal(F.calendarState(F.setCalendarUrl({ ...fixture, calendar: { set: true } }, '')), 'removing');
});

test('toPutBody lower-cases the calendar scheme only', () => {
  const put = (url) => F.toPutBody({ ...fixture, calendar: { set: false, url } }).calendar.url;
  assert.equal(put('HTTPS://Example.com/A.ics?X=1'), 'https://Example.com/A.ics?X=1');
  assert.equal(put('WebCal://example.com/a'), 'webcal://example.com/a');
  assert.equal(put('https://ok'), 'https://ok');
  assert.equal(put(''), '');
});

test('checkFeed rejects a missing time zone rule at the time zone field', () => {
  for (const posix of [null, '', undefined]) {
    const issues = F.checkFeed({ ...fixture, tz: { iana: 'Etc/UTC', posix } }, limits);
    const i = issues.find((x) => x.path === 'tz.posix');
    assert.ok(i, `posix ${posix}`);
    assert.match(i.message, /time zone/i);
  }
  assert.equal(F.checkFeed(fixture, limits).some((x) => x.path === 'tz.posix'), false);
});

test('parseDwellMinutes accepts numbers and flags empty or non-numeric text', () => {
  assert.deepEqual(F.parseDwellMinutes('5'), { sec: 300 });
  assert.deepEqual(F.parseDwellMinutes(' 2.5 '), { sec: 150 });
  for (const bad of ['', '   ', 'abc', '1e999', 'NaN']) {
    const r = F.parseDwellMinutes(bad);
    assert.equal(r.sec, undefined, bad);
    assert.match(r.error, /minutes/);
  }
});

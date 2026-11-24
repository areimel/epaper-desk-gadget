import { test } from 'node:test';
import assert from 'node:assert/strict';
import { configuredSources, sourcesSettled, waitForRefresh, refreshMessage } from '../js/refresh.js';

const before = {
  weather: { state: 'ok', updated: 100, error: '' },
  hn: { state: 'stale', updated: 50, error: 'no network' },
  calendar: { state: 'never', updated: 0, error: '' },
};
const withChange = (id, patch) => ({ ...structuredClone(before), [id]: { ...before[id], ...patch } });

test('configuredSources: hn always, weather and calendar when set', () => {
  assert.deepEqual(configuredSources({ weather: null, calendar: { set: false } }), ['hn']);
  assert.deepEqual(configuredSources({ weather: { place: 'x' }, calendar: { set: true } }), ['hn', 'weather', 'calendar']);
});

test('sourcesSettled needs every listed source to change', () => {
  assert.equal(sourcesSettled(before, before, ['weather', 'hn']).settled, false);
  const half = withChange('weather', { updated: 200 });
  assert.equal(sourcesSettled(before, half, ['weather', 'hn']).settled, false);
  const all = { ...half, hn: { state: 'ok', updated: 200, error: '' } };
  const r = sourcesSettled(before, all, ['weather', 'hn']);
  assert.equal(r.settled, true);
  assert.deepEqual(r.failed, []);
});

test('a changed error text counts, and failures are listed', () => {
  const now = { ...structuredClone(before), hn: { state: 'stale', updated: 50, error: 'timeout' } };
  const r = sourcesSettled(before, now, ['hn']);
  assert.equal(r.settled, true);
  assert.deepEqual(r.failed, ['hn']);
});

test('sources missing from the new state are not settled', () => {
  assert.equal(sourcesSettled(before, {}, ['hn']).settled, false);
});

function fake(states) {
  const calls = { sleeps: [], fetches: 0 };
  return {
    calls,
    sleep: async (ms) => { calls.sleeps.push(ms); },
    fetchState: async () => {
      const s = states[Math.min(calls.fetches++, states.length - 1)];
      if (s instanceof Error) throw s;
      return { sources: s };
    },
  };
}

test('waitForRefresh polls until the sources change', async () => {
  const done = withChange('hn', { state: 'ok', updated: 300 });
  const f = fake([before, before, done]);
  const r = await waitForRefresh({ ...f, before, ids: ['hn'] });
  assert.equal(r.settled, true);
  assert.equal(f.calls.fetches, 3);
  assert.deepEqual(f.calls.sleeps, [2000, 2000, 2000]);
});

test('waitForRefresh gives up after 30 s', async () => {
  const f = fake([before]);
  const r = await waitForRefresh({ ...f, before, ids: ['hn'] });
  assert.equal(r.settled, false);
  assert.equal(r.reached, true);
  assert.equal(f.calls.fetches, 15);
});

test('waitForRefresh keeps going after a failed poll', async () => {
  const done = withChange('hn', { state: 'ok', updated: 300 });
  const r = await waitForRefresh({ ...fake([new Error('x'), done]), before, ids: ['hn'] });
  assert.equal(r.settled, true);
});

test('waitForRefresh reports when the gadget never answered', async () => {
  const r = await waitForRefresh({ ...fake([new Error('x')]), before, ids: ['hn'], maxMs: 4000 });
  assert.equal(r.settled, false);
  assert.equal(r.reached, false);
});

test('refreshMessage', () => {
  assert.match(refreshMessage({ settled: true, failed: [] }).text, /^Data refreshed/);
  const some = refreshMessage({ settled: true, failed: ['hn'] });
  assert.equal(some.bad, true);
  assert.match(some.text, /didn't load/);
  assert.equal(refreshMessage({ settled: false, reached: true }).text, 'Still fetching. Check back in a moment.');
  assert.match(refreshMessage({ settled: false, reached: false }).text, /Couldn't reach the gadget/);
});

import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import { readFileSync } from 'node:fs';
import { startMockServer } from '../mock/server.mjs';

const feedFixture = () => JSON.parse(readFileSync(new URL('../mock/fixtures/feed.json', import.meta.url), 'utf8'));
let server;
let base;
before(async () => {
  server = await startMockServer({ port: 0 });
  base = `http://127.0.0.1:${server.address().port}`;
});
after(() => { server.closeAllConnections(); server.close(); });

const put = (body, raw) => fetch(`${base}/api/feed`, { method: 'PUT', body: raw ?? JSON.stringify(body) });
const getFeed = async () => (await fetch(`${base}/api/feed`)).text();
const rawGet = (p) => new Promise((resolve, reject) => {
  http.get({ host: '127.0.0.1', port: server.address().port, path: p }, (res) => {
    res.resume();
    res.on('end', () => resolve(res.statusCode));
  }).on('error', reject);
});

test('binds loopback only', () => assert.equal(server.address().address, '127.0.0.1'));

test('GET /api/state has shape, limits and live time', async () => {
  const r = await fetch(`${base}/api/state`);
  assert.equal(r.status, 200);
  assert.match(r.headers.get('content-type'), /^application\/json; charset=utf-8/);
  assert.equal(r.headers.get('cache-control'), 'no-store');
  const s = await r.json();
  assert.equal(s.limits.messageTextMax, 140);
  assert.equal(s.screen.rowBytes, 32);
  assert.ok(Math.abs(s.time - Date.now() / 1000) < 5);
});

test('GET /api/feed hides the calendar link', async () => {
  const f = await (await fetch(`${base}/api/feed`)).json();
  assert.deepEqual(f.calendar, { set: false });
});

test('PUT valid feed returns 200 in GET shape', async () => {
  const r = await put(feedFixture());
  assert.equal(r.status, 200);
  assert.deepEqual((await r.json()).calendar, { set: false });
});

test('calendar url: store, keep, clear; never echoed', async () => {
  const url = 'https://calendar.example.com/secret-token.ics';
  const f = feedFixture();
  f.calendar = { url };
  let r = await put(f);
  assert.equal(r.status, 200);
  assert.ok(!(await r.text()).includes(url));
  let text = await getFeed();
  assert.ok(!text.includes('secret-token'));
  assert.deepEqual(JSON.parse(text).calendar, { set: true });
  f.calendar = { set: true };
  assert.equal((await put(f)).status, 200);
  assert.deepEqual(JSON.parse(await getFeed()).calendar, { set: true });
  f.calendar = { url: '' };
  assert.equal((await put(f)).status, 200);
  text = await getFeed();
  assert.deepEqual(JSON.parse(text).calendar, { set: false });
});

test('PUT bad JSON is 400 with empty path', async () => {
  const r = await put(null, '{nope');
  assert.equal(r.status, 400);
  const b = await r.json();
  assert.equal(b.path, '');
  assert.equal(b.error, 'The feed is not valid JSON.');
});

test('PUT over-limit is 400 with a path and changes nothing', async () => {
  const before = await getFeed();
  const f = feedFixture();
  f.messages[1].text = 'x'.repeat(141);
  const r = await put(f);
  assert.equal(r.status, 400);
  const b = await r.json();
  assert.equal(b.path, 'messages[1].text');
  assert.match(b.error, /141 bytes/);
  assert.equal(await getFeed(), before);
});

test('PUT 17000 bytes is 413', async () => {
  const r = await put(null, JSON.stringify({ pad: 'x'.repeat(17000) }));
  assert.equal(r.status, 413);
  assert.equal((await r.json()).path, '');
});

test('GET /api/screen is 3904 bytes, no-store, differs per page', async () => {
  const seen = new Set();
  for (const id of ['clock', 'weather', 'hn', 'messages', 'calendar', 'notes']) {
    const r = await fetch(`${base}/api/screen?page=${id}`);
    assert.equal(r.status, 200);
    assert.equal(r.headers.get('content-type'), 'application/octet-stream');
    assert.equal(r.headers.get('cache-control'), 'no-store');
    const buf = Buffer.from(await r.arrayBuffer());
    assert.equal(buf.length, 3904);
    seen.add(buf.toString('hex'));
  }
  assert.equal(seen.size, 6);
  assert.equal((await (await fetch(`${base}/api/screen`)).arrayBuffer()).byteLength, 3904);
  assert.equal((await fetch(`${base}/api/screen?page=nope`)).status, 404);
});

test('POST /api/show updates the page and the live screen', async () => {
  const r = await fetch(`${base}/api/show`, { method: 'POST', body: '{"page":"notes"}' });
  assert.equal(r.status, 200);
  assert.deepEqual(await r.json(), { page: 'notes' });
  assert.equal((await (await fetch(`${base}/api/state`)).json()).page, 'notes');
  const live = Buffer.from(await (await fetch(`${base}/api/screen`)).arrayBuffer());
  const notes = Buffer.from(await (await fetch(`${base}/api/screen?page=notes`)).arrayBuffer());
  assert.ok(live.equals(notes));
  const bad = await fetch(`${base}/api/show`, { method: 'POST', body: '{"page":"x"}' });
  assert.equal(bad.status, 404);
  assert.equal((await bad.json()).error, 'There is no page called "x".');
  assert.equal((await fetch(`${base}/api/show`, { method: 'POST', body: 'nope' })).status, 400);
});

test('POST /api/refresh is 202', async () => {
  const r = await fetch(`${base}/api/refresh`, { method: 'POST' });
  assert.equal(r.status, 202);
  assert.deepEqual(await r.json(), {});
});

test('unknown api route 404, wrong method 405', async () => {
  const r = await fetch(`${base}/api/nope`);
  assert.equal(r.status, 404);
  assert.equal((await r.json()).path, '');
  assert.equal((await fetch(`${base}/api/state`, { method: 'POST' })).status, 405);
});

test('static: index, blocked dirs and traversal', async () => {
  const r = await fetch(`${base}/`);
  assert.ok(r.status === 200 || r.status === 404);
  if (r.status === 200) assert.match(r.headers.get('content-type'), /^text\/html/);
  assert.equal((await fetch(`${base}/mock/server.mjs`)).status, 404);
  assert.equal((await fetch(`${base}/tests/mock-server.test.mjs`)).status, 404);
  assert.equal(await rawGet('/../AGENTS.md'), 404);
  assert.equal(await rawGet('/%2e%2e/AGENTS.md'), 404);
  assert.equal(await rawGet('/..%2fAGENTS.md'), 404);
});

test('slow query delays the answer', async () => {
  const t = Date.now();
  await fetch(`${base}/api/state?slow=1`);
  assert.ok(Date.now() - t >= 1400);
});

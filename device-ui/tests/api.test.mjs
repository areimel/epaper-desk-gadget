import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createApi, ApiError } from '../js/api.js';

function json(status, body) {
  return { ok: status < 400, status, json: async () => body, arrayBuffer: async () => new ArrayBuffer(3904) };
}

test('requests run one at a time, in order', async () => {
  let active = 0, peak = 0;
  const seen = [];
  const fake = async (url, init) => {
    active++; peak = Math.max(peak, active);
    seen.push(`${init?.method || 'GET'} ${url}`);
    await new Promise((r) => setTimeout(r, 10));
    active--;
    return json(200, {});
  };
  const api = createApi({ fetch: fake });
  await Promise.all([api.state(), api.feed(), api.screen('clock'), api.show('hn'), api.refresh()]);
  assert.equal(peak, 1);
  assert.deepEqual(seen, ['GET /api/state', 'GET /api/feed', 'GET /api/screen?page=clock',
    'POST /api/show', 'POST /api/refresh']);
});

test('saveFeed PUTs JSON', async () => {
  let got;
  const api = createApi({ fetch: async (url, init) => { got = { url, init }; return json(200, { v: 1 }); } });
  assert.deepEqual(await api.saveFeed({ v: 1 }), { v: 1 });
  assert.equal(got.init.method, 'PUT');
  assert.equal(got.init.body, '{"v":1}');
  assert.equal(got.init.headers['Content-Type'], 'application/json');
});

test('screen without page fetches the live panel', async () => {
  let got;
  const api = createApi({ fetch: async (url) => { got = url; return json(200, null); } });
  const buf = await api.screen();
  assert.equal(got, '/api/screen');
  assert.equal(buf.byteLength, 3904);
});

test('error responses throw ApiError with the parsed body', async () => {
  const api = createApi({ fetch: async () => json(400, { error: 'Bad', path: 'dwellSec' }) });
  await assert.rejects(api.saveFeed({}), (e) => e instanceof ApiError && e.status === 400 && e.body.path === 'dwellSec');
});

test('a failure does not block later requests', async () => {
  let n = 0;
  const api = createApi({ fetch: async () => { if (n++ === 0) throw new TypeError('offline'); return json(200, { ok: 1 }); } });
  await assert.rejects(api.state(), (e) => e instanceof ApiError && e.status === 0);
  assert.deepEqual(await api.state(), { ok: 1 });
});

test('a slow device times out as status "timeout"', async () => {
  const fake = (url, init) => new Promise((_, reject) => {
    init.signal.addEventListener('abort', () => reject(new DOMException('aborted', 'AbortError')));
  });
  const api = createApi({ fetch: fake, timeoutMs: 20 });
  await assert.rejects(api.state(), (e) => e instanceof ApiError && e.status === 'timeout');
});

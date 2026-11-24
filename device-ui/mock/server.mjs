// Zero-dependency mock of the Desk Gadget device API (docs/api.md).
import http from 'node:http';
import { readFileSync } from 'node:fs';
import { readFile, stat } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { screenFor, setLivePage } from './bitmap.mjs';
import { PAGE_IDS, validateFeed } from './validate.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const MAX_BODY = 16384;
const SLOW_DELAY = 1500;
const BLOCKED = new Set(['mock', 'tests', 'tools']);
const MIME = {
  '.html': 'text/html; charset=utf-8', '.css': 'text/css', '.js': 'text/javascript',
  '.mjs': 'text/javascript', '.json': 'application/json', '.woff2': 'font/woff2',
  '.svg': 'image/svg+xml', '.txt': 'text/plain', '.png': 'image/png', '.ico': 'image/x-icon',
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const clone = (v) => JSON.parse(JSON.stringify(v));

function sendJson(res, status, body) {
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(body));
}
const sendError = (res, status, error, p = '') => sendJson(res, status, { error, path: p });

function readBody(req) {
  return new Promise((resolve) => {
    const chunks = [];
    let size = 0;
    let tooBig = false;
    req.on('data', (c) => {
      size += c.length;
      if (size > MAX_BODY) tooBig = true;
      else chunks.push(c);
    });
    req.on('end', () => resolve({ tooBig, text: Buffer.concat(chunks).toString('utf8') }));
    req.on('error', () => resolve({ tooBig, text: '' }));
  });
}

async function readJson(req, res) {
  const { tooBig, text } = await readBody(req);
  if (tooBig) {
    res.setHeader('Connection', 'close');
    sendError(res, 413, 'The feed is too large to save. Remove some messages or notes and try again.');
    return undefined;
  }
  try {
    return JSON.parse(text);
  } catch {
    sendError(res, 400, 'The feed is not valid JSON.');
    return undefined;
  }
}

async function serveStatic(res, rootDir, pathname) {
  let rel;
  try { rel = decodeURIComponent(pathname); } catch { rel = null; }
  if (rel === '/') rel = '/index.html';
  const notFound = () => sendError(res, 404, 'There is no such file.');
  if (rel === null || rel.includes('\0')) return notFound();
  const file = path.resolve(rootDir, '.' + rel);
  const sub = path.relative(rootDir, file);
  if (!sub || sub.startsWith('..') || path.isAbsolute(sub)) return notFound();
  if (BLOCKED.has(sub.split(path.sep)[0])) return notFound();
  try {
    if (!(await stat(file)).isFile()) return notFound();
    const data = await readFile(file);
    res.writeHead(200, { 'Content-Type': MIME[path.extname(file).toLowerCase()] || 'application/octet-stream', 'Cache-Control': 'no-store' });
    res.end(data);
  } catch {
    notFound();
  }
}

export function createMockServer({ slowMs = 0, fixturesDir = path.join(HERE, 'fixtures'), rootDir = path.dirname(HERE), log = false } = {}) {
  const load = (n) => JSON.parse(readFileSync(path.join(fixturesDir, n), 'utf8'));
  const state = load('state.json');
  let feed = load('feed.json');
  let calendarUrl = null;
  setLivePage(state.page);
  const publicFeed = () => ({ ...clone(feed), calendar: { set: calendarUrl !== null } });

  function applyFeed(body) {
    const next = clone(body);
    if ('url' in next.calendar) calendarUrl = next.calendar.url === '' ? null : next.calendar.url;
    next.calendar = { set: calendarUrl !== null };
    feed = next;
  }

  const api = {
    'GET /api/state': (req, res) => sendJson(res, 200, { ...clone(state), time: Math.floor(Date.now() / 1000), page: state.page }),
    'GET /api/feed': (req, res) => sendJson(res, 200, publicFeed()),
    async 'PUT /api/feed'(req, res) {
      const body = await readJson(req, res);
      if (body === undefined) return;
      // Loading the app as /?reject=1 makes saves fail, to exercise the error UI.
      if (/reject=1/.test(req.headers.referer || '') && body.messages?.length) {
        return sendError(res, 400, 'The gadget can only show Latin letters in message 1. Remove emoji or other symbols.', 'messages[0].text');
      }
      const v = validateFeed(body, state.limits);
      if (!v.ok) return sendError(res, 400, v.error, v.path);
      applyFeed(body);
      sendJson(res, 200, publicFeed());
    },
    'GET /api/screen'(req, res, url) {
      const id = url.searchParams.get('page');
      if (id !== null && !PAGE_IDS.includes(id)) return sendError(res, 404, `There is no page called "${id}".`, 'page');
      res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'Cache-Control': 'no-store' });
      res.end(Buffer.from(screenFor(id ?? state.page)));
    },
    async 'POST /api/show'(req, res) {
      const body = await readJson(req, res);
      if (body === undefined) return;
      const id = body && typeof body === 'object' ? body.page : undefined;
      if (!PAGE_IDS.includes(id)) return sendError(res, 404, `There is no page called "${id}".`, 'page');
      state.page = id;
      setLivePage(id);
      sendJson(res, 200, { page: id });
    },
    'POST /api/refresh'(req, res) {
      setTimeout(() => {
        const now = Math.floor(Date.now() / 1000);
        for (const [id, s] of Object.entries(state.sources)) {
          // The device refetches every source it has; the mock has them all succeed.
          if (s.state !== 'never' || (id === 'calendar' && calendarUrl !== null)) Object.assign(s, { state: 'ok', updated: now, error: '' });
        }
        state.refreshCount += 1;
      }, 2000).unref();
      sendJson(res, 202, {});
    },
  };
  const apiPaths = new Set(Object.keys(api).map((k) => k.split(' ')[1]));

  const server = http.createServer(async (req, res) => {
    const url = new URL(req.url, 'http://localhost');
    if (log) console.log(`${req.method} ${req.url}`);
    try {
      if (url.pathname.startsWith('/api/')) {
        const slow = slowMs > 0 || url.searchParams.get('slow') === '1' || /slow=1/.test(req.headers.referer || '');
        if (slow) await sleep(slowMs > 0 ? slowMs : SLOW_DELAY);
        const handler = api[`${req.method} ${url.pathname}`];
        if (handler) return await handler(req, res, url);
        if (apiPaths.has(url.pathname)) return sendError(res, 405, `${req.method} is not allowed here.`);
        return sendError(res, 404, 'There is no such API route.');
      }
      if (req.method !== 'GET' && req.method !== 'HEAD') return sendError(res, 405, `${req.method} is not allowed here.`);
      await serveStatic(res, rootDir, url.pathname);
    } catch (e) {
      if (!res.headersSent) sendError(res, 500, 'The mock server hit an error: ' + e.message);
    }
  });
  return server;
}

export async function startMockServer({ port = 0, ...opts } = {}) {
  const server = createMockServer(opts);
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(port, '127.0.0.1', resolve);
  });
  return server;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const port = Number(process.env.PORT) || 8080;
  const server = await startMockServer({ port, slowMs: process.env.SLOW === '1' ? SLOW_DELAY : 0, log: true });
  console.log(`Desk Gadget mock running at http://127.0.0.1:${server.address().port}/`);
}

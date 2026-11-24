// Device API client. The device serves one request at a time, so every call
// waits for the previous one (see "Timing" in docs/api.md).
export class ApiError extends Error {
  constructor(status, body) {
    super(body?.error || `HTTP ${status}`);
    this.status = status; // number, 0 for no connection, or 'timeout'
    this.body = body;
  }
}

export function createApi({ fetch: fetchImpl = globalThis.fetch.bind(globalThis), base = '', timeoutMs = 10000 } = {}) {
  let tail = Promise.resolve();

  async function run(path, init, read) {
    const ctl = new AbortController();
    const timer = setTimeout(() => ctl.abort(), timeoutMs);
    try {
      let res;
      try {
        res = await fetchImpl(base + path, { ...init, signal: ctl.signal });
      } catch {
        throw new ApiError(ctl.signal.aborted ? 'timeout' : 0, null);
      }
      if (!res.ok) {
        let body = null;
        try { body = await res.json(); } catch { /* not JSON */ }
        throw new ApiError(res.status, body);
      }
      return await read(res);
    } finally {
      clearTimeout(timer);
    }
  }

  function enqueue(path, init, read) {
    const p = tail.then(() => run(path, init, read));
    tail = p.catch(() => {});
    return p;
  }

  const json = (r) => r.json();
  const post = (path, body) => enqueue(path, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: body === undefined ? undefined : JSON.stringify(body),
  }, json);

  return {
    state: () => enqueue('/api/state', undefined, json),
    feed: () => enqueue('/api/feed', undefined, json),
    saveFeed: (body) => enqueue('/api/feed', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    }, json),
    screen: (page) => enqueue(page ? `/api/screen?page=${encodeURIComponent(page)}` : '/api/screen',
      undefined, (r) => r.arrayBuffer()),
    show: (page) => post('/api/show', { page }),
    refresh: () => post('/api/refresh'),
  };
}

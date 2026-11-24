// "Refresh data now": ask, then poll /api/state until the sources report back.
export const POLL_MS = 2000;
export const GIVE_UP_MS = 30000;

// The sources the feed asks the device to fetch.
export function configuredSources(feed) {
  const ids = ['hn'];
  if (feed?.weather) ids.push('weather');
  if (feed?.calendar?.set) ids.push('calendar');
  return ids;
}

const stamp = (s) => (s ? `${s.state}|${s.updated}|${s.error}` : '');

// Settled once every listed source differs from how it looked before the request.
export function sourcesSettled(before, now, ids) {
  const settled = ids.every((id) => now?.[id] && stamp(now[id]) !== stamp(before?.[id]));
  const failed = settled ? ids.filter((id) => now[id].state === 'error' || now[id].state === 'stale') : [];
  return { settled, failed };
}

export async function waitForRefresh({ fetchState, sleep, before, ids, intervalMs = POLL_MS, maxMs = GIVE_UP_MS }) {
  let reached = false;
  let last = null;
  for (let waited = 0; waited < maxMs; waited += intervalMs) {
    await sleep(intervalMs);
    try {
      const state = await fetchState();
      reached = true;
      last = state;
      const r = sourcesSettled(before, state.sources, ids);
      if (r.settled) return { ...r, reached, state };
    } catch { /* try again on the next poll */ }
  }
  return { settled: false, failed: [], reached, state: last };
}

export function refreshMessage({ settled, failed, reached }) {
  if (settled && !failed?.length) return { text: 'Data refreshed. The lines above show what came back.', bad: false };
  if (settled) return { text: "The gadget tried, but some data didn't load. The lines above say why.", bad: true };
  if (reached) return { text: 'Still fetching. Check back in a moment.', bad: false };
  return { text: "Couldn't reach the gadget to see how the refresh went. Check it is powered and try again.", bad: true };
}

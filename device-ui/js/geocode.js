// City search, done by the browser against Open-Meteo (CORS open, no key).
const BASE = 'https://geocoding-api.open-meteo.com/v1/search';

export const geocodeUrl = (name) => `${BASE}?name=${encodeURIComponent(name)}&count=5`;

const enc = new TextEncoder();
export function cutBytes(s, max) {
  let out = '';
  let n = 0;
  for (const ch of s) {
    n += enc.encode(ch).length;
    if (n > max) break;
    out += ch;
  }
  return out;
}

const round4 = (x) => Math.round(x * 1e4) / 1e4;

export function parsePlaces(json, placeMax) {
  const list = Array.isArray(json?.results) ? json.results : [];
  return list
    .filter((r) => typeof r.name === 'string' && Number.isFinite(r.latitude) && Number.isFinite(r.longitude))
    .map((r) => ({
      place: cutBytes(r.name, placeMax),
      label: [r.name, r.admin1, r.country].filter(Boolean).join(', '),
      lat: round4(r.latitude),
      lon: round4(r.longitude),
    }));
}

export async function searchPlaces(name, placeMax, fetchImpl = globalThis.fetch) {
  const q = name.trim();
  if (!q) return [];
  let res;
  try {
    res = await fetchImpl(geocodeUrl(q));
  } catch {
    res = null;
  }
  if (!res || !res.ok) {
    throw new Error("City search didn't work. This browser needs internet access for it; try again.");
  }
  return parsePlaces(await res.json(), placeMax);
}

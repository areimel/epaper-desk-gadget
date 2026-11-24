import { test } from 'node:test';
import assert from 'node:assert/strict';
import { geocodeUrl, parsePlaces, searchPlaces, cutBytes, latestOnly } from '../js/geocode.js';

const sample = {
  results: [
    { name: 'Philadelphia', latitude: 39.95238, longitude: -75.16362, admin1: 'Pennsylvania', country: 'United States' },
    { name: 'Philadelphia', latitude: 32.77152, longitude: -89.11673, admin1: 'Mississippi', country: 'United States' },
    { name: 'São Paulo', latitude: -23.5475, longitude: -46.63611, country: 'Brazil' },
  ],
};

test('geocodeUrl encodes the name and asks for 5 results', () => {
  assert.equal(geocodeUrl('São Paulo'),
    'https://geocoding-api.open-meteo.com/v1/search?name=S%C3%A3o%20Paulo&count=5');
});

test('parsePlaces builds labels and rounds coordinates', () => {
  const p = parsePlaces(sample, 32);
  assert.equal(p.length, 3);
  assert.deepEqual(p[0], { place: 'Philadelphia', label: 'Philadelphia, Pennsylvania, United States', lat: 39.9524, lon: -75.1636 });
  assert.equal(p[2].label, 'São Paulo, Brazil');
});

test('parsePlaces handles no results', () => {
  assert.deepEqual(parsePlaces({}, 32), []);
  assert.deepEqual(parsePlaces(null, 32), []);
});

test('cutBytes trims to a UTF-8 byte budget without splitting characters', () => {
  assert.equal(cutBytes('abcdef', 4), 'abcd');
  assert.equal(cutBytes('ééé', 5), 'éé');
  assert.equal(cutBytes('a😀', 4), 'a');
  assert.equal(parsePlaces({ results: [{ name: 'Llanfairpwllgwyngyllgogerychwyrndrobwll', latitude: 1, longitude: 2 }] }, 32)[0].place.length, 32);
});

test('searchPlaces uses the injected fetch', async () => {
  let asked = '';
  const fake = async (url) => { asked = url; return { ok: true, json: async () => sample }; };
  const p = await searchPlaces('Philadelphia', 32, fake);
  assert.match(asked, /name=Philadelphia/);
  assert.equal(p.length, 3);
});

test('searchPlaces rejects with plain copy when the service fails', async () => {
  const fake = async () => ({ ok: false, status: 500, json: async () => ({}) });
  await assert.rejects(searchPlaces('x', 32, fake), /City search didn't work/);
  assert.deepEqual(await searchPlaces('  ', 32, fake), []);
});

test('parsePlaces skips null and malformed entries', () => {
  const p = parsePlaces({ results: [null, 7, 'x', { name: 'Ok', latitude: 1, longitude: 2 }, {}, { name: 5, latitude: 1, longitude: 2 }] }, 32);
  assert.deepEqual(p.map((x) => x.place), ['Ok']);
});

test('searchPlaces gives plain copy for a reply that is not JSON', async () => {
  const fake = async () => ({ ok: true, json: async () => { throw new SyntaxError('Unexpected token < in JSON'); } });
  await assert.rejects(searchPlaces('x', 32, fake), (e) => /City search/.test(e.message) && !/SyntaxError|token/.test(e.message));
});

test('latestOnly: only the newest search is current', () => {
  const next = latestOnly();
  const first = next();
  assert.equal(first(), true);
  const second = next();
  assert.equal(first(), false);
  assert.equal(second(), true);
});

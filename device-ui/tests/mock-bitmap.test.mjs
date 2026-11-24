import test from 'node:test';
import assert from 'node:assert/strict';
import { SCREEN, screenFor, setPixel } from '../mock/bitmap.mjs';
import { readFileSync, mkdtempSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { PAGE_IDS } from '../mock/validate.mjs';

const px = (b, x, y) => (b[y * 32 + (x >> 3)] >> (7 - (x & 7))) & 1;

test('screen constants', () => {
  assert.deepEqual(SCREEN, { w: 250, h: 122, rowBytes: 32, bytes: 3904 });
});

test('every page is 3904 bytes with zero padding bits', () => {
  for (const id of PAGE_IDS) {
    const b = screenFor(id);
    assert.equal(b.length, 3904, id);
    for (let y = 0; y < 122; y++) assert.equal(b[y * 32 + 31] & 0x3f, 0, `${id} row ${y}`);
  }
});

test('placeholder border, title bar and top-left pixel are black', () => {
  for (const id of PAGE_IDS) {
    const b = screenFor(id, null);
    assert.equal(px(b, 0, 0), 1);
    for (let x = 0; x < 250; x++) {
      assert.equal(px(b, x, 0), 1);
      assert.equal(px(b, x, 121), 1);
    }
    for (let y = 0; y < 122; y++) {
      assert.equal(px(b, 0, y), 1);
      assert.equal(px(b, 249, y), 1);
    }
    for (let y = 0; y < 14; y++) assert.equal(px(b, 245, y), 1);
  }
});

test('pages differ pairwise', () => {
  for (let i = 0; i < PAGE_IDS.length; i++) {
    for (let j = i + 1; j < PAGE_IDS.length; j++) {
      assert.notDeepEqual(screenFor(PAGE_IDS[i], null), screenFor(PAGE_IDS[j], null), `${PAGE_IDS[i]} vs ${PAGE_IDS[j]}`);
    }
  }
});

test('setPixel ignores out-of-range coordinates', () => {
  const b = new Uint8Array(3904);
  setPixel(b, 250, 0);
  setPixel(b, -1, 0);
  setPixel(b, 0, 122);
  assert.ok(b.every((v) => v === 0));
  setPixel(b, 0, 0);
  assert.equal(b[0], 0x80);
});

test('live screen has the right size', () => {
  assert.equal(screenFor(undefined).length, 3904);
});

test('a golden page rendering is served when it exists', () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'golden-'));
  const golden = new Uint8Array(3904).fill(0xaa);
  writeFileSync(path.join(dir, 'weather.bin'), golden);
  writeFileSync(path.join(dir, 'hn.bin'), new Uint8Array(10)); // wrong size: ignored
  assert.deepEqual(screenFor('weather', dir), golden);
  assert.deepEqual(screenFor('hn', dir), screenFor('hn', null));
  assert.deepEqual(screenFor('notes', dir), screenFor('notes', null));
});

test('repository goldens are what the server serves', () => {
  for (const id of PAGE_IDS) {
    const file = new URL(`../../test/fixtures/golden/${id}.bin`, import.meta.url);
    assert.deepEqual(screenFor(id), new Uint8Array(readFileSync(file)), id);
  }
});

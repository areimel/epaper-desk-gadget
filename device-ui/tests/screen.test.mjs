import { test } from 'node:test';
import assert from 'node:assert/strict';
import { decodeScreen, GEOM, bestScale } from '../js/screen.js';

test('decodes 3904 bytes into 250x122 pixels, MSB is leftmost', () => {
  const bytes = new Uint8Array(3904);
  bytes[0] = 0b1000_0001;          // x=0 and x=7 on row 0
  bytes[32 * 121 + 31] = 0b0100_0000; // row 121, x=249 (bit 1 of byte 31 = x 249)
  const px = decodeScreen(bytes);
  assert.equal(px.length, 250 * 122);
  assert.equal(px[0], 1);
  assert.equal(px[1], 0);
  assert.equal(px[7], 1);
  assert.equal(px[121 * 250 + 249], 1);
  assert.equal(px.reduce((a, b) => a + b, 0), 3);
});

test('ignores the 6 padding bits at the end of each row', () => {
  const bytes = new Uint8Array(3904);
  for (let y = 0; y < 122; y++) bytes[y * 32 + 31] = 0b0011_1111;
  const px = decodeScreen(bytes);
  assert.equal(px.reduce((a, b) => a + b, 0), 0);
});

test('accepts an ArrayBuffer', () => {
  const buf = new ArrayBuffer(3904);
  new Uint8Array(buf)[32] = 0x80;
  assert.equal(decodeScreen(buf)[250], 1);
});

test('rejects a wrong length', () => {
  assert.throws(() => decodeScreen(new Uint8Array(100)), /3904/);
});

test('custom geometry from /api/state screen', () => {
  const px = decodeScreen(new Uint8Array([0xff, 0x00]), { w: 4, h: 2, rowBytes: 1 });
  assert.deepEqual([...px], [1, 1, 1, 1, 0, 0, 0, 0]);
});

test('default geometry matches the spec', () => {
  assert.deepEqual(GEOM, { w: 250, h: 122, rowBytes: 32 });
});

test('bestScale picks the largest integer scale that fits, at least 1', () => {
  assert.equal(bestScale(358, 250), 1);
  assert.equal(bestScale(704, 250), 2);
  assert.equal(bestScale(760, 250, 3), 3);
  assert.equal(bestScale(2000, 250, 3), 3);
  assert.equal(bestScale(100, 250), 1);
});

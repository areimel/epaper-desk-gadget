import { test } from 'node:test';
import assert from 'node:assert/strict';
import { TZ, posixFor, zoneNames, browserZone } from '../js/tz.js';

test('known zones map to POSIX strings', () => {
  assert.equal(posixFor('America/New_York'), 'EST5EDT,M3.2.0,M11.1.0');
  assert.equal(posixFor('Europe/London'), 'GMT0BST,M3.5.0/1,M10.5.0');
  assert.equal(posixFor('UTC'), 'UTC0');
});

test('unknown or empty zones return null', () => {
  assert.equal(posixFor('Nowhere/Fake'), null);
  assert.equal(posixFor(''), null);
  assert.equal(posixFor('constructor'), null);
});

test('zoneNames is sorted and complete', () => {
  const names = zoneNames();
  assert.ok(names.length > 300, `expected > 300 zones, got ${names.length}`);
  assert.deepEqual(names, [...names].sort());
  assert.deepEqual(names, Object.keys(TZ));
  assert.notEqual(names, zoneNames(), 'returns a fresh array each call');
});

test('TZ is frozen', () => {
  assert.ok(Object.isFrozen(TZ));
});

test('every POSIX value is a short single-line non-empty string', () => {
  for (const [name, value] of Object.entries(TZ)) {
    assert.equal(typeof value, 'string', name);
    assert.ok(value.length > 0, `${name} is empty`);
    assert.ok(new TextEncoder().encode(value).length < 48, `${name} is too long`);
    assert.ok(!value.includes('\n'), `${name} contains a newline`);
  }
});

test('browserZone returns null or a known zone', () => {
  const zone = browserZone();
  assert.ok(zone === null || Object.hasOwn(TZ, zone), `unexpected ${zone}`);
});

test('Etc/UTC, the factory zone, is in the table', () => {
  assert.equal(posixFor('Etc/UTC'), 'UTC0');
  assert.ok(zoneNames().includes('Etc/UTC'));
});

test('the saved zone keeps its saved rule when the table lacks it', () => {
  const saved = { iana: 'Mars/Olympus', posix: '<+04>-4' };
  assert.equal(posixFor('Mars/Olympus', saved), '<+04>-4');
  assert.equal(posixFor('Mars/Phobos', saved), null);
  assert.equal(posixFor('Mars/Olympus', { iana: 'Mars/Olympus', posix: '' }), null);
  assert.equal(posixFor('America/New_York', saved), 'EST5EDT,M3.2.0,M11.1.0');
});

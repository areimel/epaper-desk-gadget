import { test } from 'node:test';
import assert from 'node:assert/strict';
import { heroCaption } from '../js/caption.js';

const saved = (on) => ({ dwellSec: 300, pages: [{ id: 'clock', on }] });

test('loading', () => {
  assert.match(heroCaption(null, null), /Loading/);
});

test('a page on the panel is named, with the rotation time', () => {
  assert.equal(heroCaption({ page: 'weather' }, saved(true)),
    'On the gadget now: Weather. Pages change every 5 minutes.');
});

test('an empty page with pages switched on means the setup screen', () => {
  assert.equal(heroCaption({ page: '' }, saved(true)), 'The gadget is showing its setup screen.');
});

test('an empty page with every page off asks for a page to be switched on', () => {
  assert.match(heroCaption({ page: '' }, saved(false)), /no page to show\. Switch on a page/);
});

test('an unknown page id is shown as is rather than as a blank', () => {
  assert.match(heroCaption({ page: 'sudoku' }, saved(true)), /sudoku/);
});

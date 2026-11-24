// The sentence under the live screen. Pure.
import { PAGE_NAMES } from './queue.js';

export function heroCaption(state, saved) {
  if (!state) return 'Loading the screen from the gadget...';
  if (!state.page) {
    // The device reports "" for its setup screen and for "no page is on".
    return saved?.pages?.some((p) => p.on)
      ? 'The gadget is showing its setup screen.'
      : 'The gadget has no page to show. Switch on a page below.';
  }
  const name = PAGE_NAMES[state.page] || state.page;
  const mins = saved ? ` Pages change every ${Math.round(saved.dwellSec / 60)} minutes.` : '';
  return `On the gadget now: ${name}.${mins}`;
}

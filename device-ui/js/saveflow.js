// Pure decisions for what happens to the draft when a save or discard lands.
import { clone, isDirty } from './feed.js';

// A save sent `sent`, the device answered with `saved`, and the person may have
// kept typing: `draft` is what is on screen now. Keep their newer edits.
export function mergeAfterSave({ sent, saved, draft }) {
  if (!isDirty(draft, sent)) return { draft: clone(saved), dirty: false, edited: false };
  const next = clone(draft);
  // A link that was just stored is no longer a pending change.
  if (typeof sent.calendar?.url === 'string' && next.calendar?.url === sent.calendar.url) {
    next.calendar = { set: !!saved.calendar?.set };
  }
  return { draft: next, dirty: isDirty(next, saved), edited: true };
}

// While a save is in flight the device is about to store `sent`, so a discard
// can only go back to that, not to the older saved copy.
export const discardTarget = ({ saving, sent, saved }) => (saving && sent ? sent : saved);

// The one helper for "re-render and keep keyboard focus": a re-render replaces
// elements, so focus goes back to the element that has the same id.
export function keepFocus(render, { doc = globalThis.document, focusId = '' } = {}) {
  const before = doc.activeElement;
  const oldId = before?.id || '';
  const range = before && oldId && 'selectionStart' in before
    ? [before.selectionStart, before.selectionEnd] : null;
  const result = render();
  const next = (focusId && doc.getElementById(focusId)) || (oldId && doc.getElementById(oldId));
  if (next && next !== doc.activeElement) {
    next.focus({ preventScroll: true });
    if (range && next.id === oldId) {
      try { next.setSelectionRange(range[0], range[1]); } catch { /* not a text field */ }
    }
  }
  return result;
}

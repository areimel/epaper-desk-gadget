// Tiny element builder: h('button', {class: 'x', onclick}, 'Label', child...).
export function h(tag, props = {}, ...children) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(props || {})) {
    if (v == null || v === false) continue;
    if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
    else if (k === 'class') el.className = v;
    else if (k === 'text') el.textContent = v;
    else if (k === 'value') el.value = v;
    else if (k in el && typeof v !== 'string') el[k] = v;
    else el.setAttribute(k, v === true ? '' : v);
  }
  for (const c of children.flat()) {
    if (c == null || c === false) continue;
    el.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return el;
}

export const $ = (sel, root = document) => root.querySelector(sel);

// A labelled field with an error slot. Returns {wrap, input, error}.
export function field({ id, label, hint, input }) {
  input.id = id;
  const err = h('p', { class: 'field-error', id: `${id}-error`, hidden: true });
  const hintEl = hint ? h('p', { class: 'hint', id: `${id}-hint` }, hint) : null;
  input.setAttribute('aria-describedby', [hintEl && hintEl.id, err.id].filter(Boolean).join(' '));
  const wrap = h('div', { class: 'field' }, h('label', { for: id }, label), hintEl, input, err);
  return { wrap, input, error: err };
}

// Live "34 of 140 bytes" counter that turns red past the limit.
export function counter(input, max, bytes) {
  const out = h('span', { class: 'count', 'aria-live': 'polite' });
  const update = () => {
    const n = bytes(input.value);
    out.textContent = n > max ? `${n - max} over the limit` : `${max - n} left`;
    out.classList.toggle('over', n > max);
  };
  input.addEventListener('input', update);
  update();
  return out;
}

export function iconButton(label, glyph, onclick, extra = {}) {
  return h('button', { type: 'button', class: 'icon', 'aria-label': label, title: label, onclick, ...extra },
    h('span', { 'aria-hidden': 'true' }, glyph));
}

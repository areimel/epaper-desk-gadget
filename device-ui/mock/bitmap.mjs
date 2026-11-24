// 1-bit 250x122 mock screens: MSB-left, set bit = black, 6 zero padding bits per row.
export const SCREEN = { w: 250, h: 122, rowBytes: 32, bytes: 3904 };

let livePage = 'clock';
export function setLivePage(id) { livePage = id; }

export function setPixel(buf, x, y, on = true) {
  if (x < 0 || y < 0 || x >= SCREEN.w || y >= SCREEN.h) return;
  const i = y * SCREEN.rowBytes + (x >> 3);
  const m = 0x80 >> (x & 7);
  buf[i] = on ? buf[i] | m : buf[i] & ~m;
}

function rect(b, x, y, w, h, on = true) {
  for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) setPixel(b, x + i, y + j, on);
}
function box(b, x, y, w, h) {
  rect(b, x, y, w, 1); rect(b, x, y + h - 1, w, 1); rect(b, x, y, 1, h); rect(b, x + w - 1, y, 1, h);
}
function circle(b, cx, cy, r, fill) {
  for (let y = -r; y <= r; y++) {
    for (let x = -r; x <= r; x++) {
      const d = x * x + y * y;
      if (fill ? d <= r * r : d <= r * r && d > (r - 2) * (r - 2)) setPixel(b, cx + x, cy + y);
    }
  }
}
function line(b, x0, y0, x1, y1) {
  const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0));
  for (let i = 0; i <= n; i++) {
    const x = Math.round(x0 + ((x1 - x0) * i) / n);
    const y = Math.round(y0 + ((y1 - y0) * i) / n);
    rect(b, x, y, 2, 2);
  }
}

// 3x5 glyphs, five octal digits per glyph (one per row, 3 bits each).
const GLYPHS = {
  A: '25755', B: '65656', C: '34443', D: '65556', E: '74647', F: '74644', G: '34553',
  H: '55755', I: '72227', J: '11152', K: '55655', L: '44447', M: '57755', N: '65555',
  O: '25552', P: '65644', Q: '25573', R: '65655', S: '34216', T: '72222', U: '55557',
  V: '55552', W: '55775', X: '55255', Y: '55222', Z: '71247', 0: '25552', 1: '26227',
  2: '61247', 3: '61216', 4: '55711', 5: '74616', 6: '34652', 7: '71244', 8: '25252',
  9: '25316', ' ': '00000',
};
function knockout(b, str, x0, y0) {
  [...str.toUpperCase()].forEach((ch, n) => {
    const g = GLYPHS[ch] || GLYPHS[' '];
    for (let r = 0; r < 5; r++) {
      for (let c = 0; c < 3; c++) {
        if ((Number(g[r]) >> (2 - c)) & 1) rect(b, x0 + n * 8 + c * 2, y0 + r * 2, 2, 2, false);
      }
    }
  });
}

const PAGES = {
  clock(b) {
    circle(b, 125, 68, 40, false);
    line(b, 125, 68, 125, 38);
    line(b, 125, 68, 148, 80);
  },
  weather(b) {
    circle(b, 75, 68, 26, true);
    for (let i = 0; i < 4; i++) rect(b, 125, 40 + i * 18, 110, 3);
  },
  hn(b) {
    for (let i = 0; i < 3; i++) {
      rect(b, 10, 22 + i * 32, 225, 8);
      rect(b, 10, 34 + i * 32, 150, 8);
    }
  },
  messages(b) {
    rect(b, 40, 62, 170, 14);
    rect(b, 20, 24, 10, 10); rect(b, 34, 24, 10, 10);
    rect(b, 206, 96, 10, 10); rect(b, 220, 96, 10, 10);
  },
  calendar(b) {
    for (let i = 0; i < 4; i++) {
      rect(b, 12, 22 + i * 24, 12, 12);
      rect(b, 32, 25 + i * 24, 90 + i * 30, 6);
    }
  },
  notes(b) {
    for (let i = 0; i < 5; i++) {
      box(b, 12, 20 + i * 19, 11, 11);
      rect(b, 32, 23 + i * 19, 150 - i * 10, 5);
    }
  },
};

export function screenFor(pageId = livePage) {
  const b = new Uint8Array(SCREEN.bytes);
  rect(b, 0, 0, SCREEN.w, 14);
  box(b, 0, 0, SCREEN.w, SCREEN.h);
  (PAGES[pageId] || PAGES.clock)(b);
  knockout(b, pageId, 6, 2);
  return b;
}

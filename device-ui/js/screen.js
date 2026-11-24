// The /api/screen bitmap: rows of `rowBytes`, MSB first, set bit = ink.
export const GEOM = Object.freeze({ w: 250, h: 122, rowBytes: 32 });

export function decodeScreen(bytes, geom = GEOM) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  const { w, h, rowBytes } = geom;
  if (b.length !== h * rowBytes) {
    throw new Error(`Screen must be ${h * rowBytes} bytes, got ${b.length}`);
  }
  const px = new Uint8Array(w * h);
  for (let y = 0; y < h; y++) {
    const row = y * rowBytes;
    for (let x = 0; x < w; x++) {
      px[y * w + x] = (b[row + (x >> 3)] >> (7 - (x & 7))) & 1;
    }
  }
  return px;
}

// Largest integer scale whose width fits `avail` CSS px, between 1 and `max`.
export function bestScale(avail, w, max = 4) {
  return Math.max(1, Math.min(max, Math.floor(avail / w)));
}

// Paint at native size; CSS scales it up with image-rendering: pixelated.
export function drawScreen(canvas, bytes, geom = GEOM) {
  const px = decodeScreen(bytes, geom);
  canvas.width = geom.w;
  canvas.height = geom.h;
  const ctx = canvas.getContext('2d');
  const img = ctx.createImageData(geom.w, geom.h);
  const ink = [0x17, 0x19, 0x1b], paper = [0xe3, 0xe5, 0xdc];
  for (let i = 0; i < px.length; i++) {
    const c = px[i] ? ink : paper;
    img.data[i * 4] = c[0];
    img.data[i * 4 + 1] = c[1];
    img.data[i * 4 + 2] = c[2];
    img.data[i * 4 + 3] = 255;
  }
  ctx.putImageData(img, 0, 0);
}

// Size a canvas to a whole number of DEVICE pixels per panel pixel, so every
// panel pixel stays square and sharp on any screen density.
// On a low-density screen narrower than the panel there is no whole scale, so
// the canvas shrinks smoothly instead (class "shrunk") and returns 0.
export function fitCanvas(canvas, availCss, geom = GEOM, maxCss = 3, dpr = 1) {
  const shrink = availCss * dpr < geom.w;
  const k = shrink ? 0 : bestScale(availCss * dpr, geom.w, Math.max(1, Math.floor(maxCss * dpr)));
  const cssW = shrink ? Math.floor(availCss) : (geom.w * k) / dpr;
  canvas.style.width = `${cssW}px`;
  canvas.style.height = `${(cssW * geom.h) / geom.w}px`;
  canvas.classList.toggle('shrunk', shrink);
  return k;
}

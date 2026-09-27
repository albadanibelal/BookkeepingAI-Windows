// Builds the UI atlas used by the native renderer:
//   ui_atlas.png  – 2048x2048 grayscale coverage (white on black)
//   ui_atlas.txt  – metrics:
//     F <font> <px> <ascent> <descent>
//     G <font> <codepoint> <x> <y> <w> <h> <xoff> <yoff> <advance>
//     I <name> <x> <y> <w> <h>            (icons, Arabic labels, title art)
// Fonts: Inter (Latin UI), Amiri / Amiri Quran (Arabic). All OFL-1.1.
import { chromium } from 'playwright';
import fs from 'fs';
import path from 'path';
import { createRequire } from 'module';

const require = createRequire(import.meta.url);
const fdir = (p) => path.join(path.dirname(require.resolve(`@fontsource/${p}/package.json`)), 'files');
const QMH = path.dirname(require.resolve('quran-madina-html/package.json'));
const outDir = process.argv[2] || '../assets';
const db = JSON.parse(fs.readFileSync(path.join(QMH, 'assets/db/Madina05-Hafs-24px.json'), 'utf8'));

const b64 = (f) => fs.readFileSync(f).toString('base64');
const fonts = [
  ['Inter', 400, `${fdir('inter')}/inter-latin-400-normal.woff2`],
  ['Inter', 600, `${fdir('inter')}/inter-latin-600-normal.woff2`],
  ['Amiri', 700, `${fdir('amiri')}/amiri-arabic-700-normal.woff2`],
  ['AmiriQuran', 400, `${fdir('amiri-quran')}/amiri-quran-arabic-400-normal.woff2`],
];

// 24x24 stroke icons (drawn at 96px).
const S = (d, extra = '') => `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">${d}${extra}</svg>`;
const gear = (() => {
  let d = '';
  for (let i = 0; i < 8; i++) {
    const a0 = (i / 8) * Math.PI * 2, a1 = a0 + 0.28, a2 = a0 + 0.5, a3 = a0 + 0.78;
    const p = (a, r) => `${(12 + r * Math.cos(a)).toFixed(2)} ${(12 + r * Math.sin(a)).toFixed(2)}`;
    d += `${i ? 'L' : 'M'}${p(a0, 7.2)} L${p(a1, 9.6)} L${p(a2, 9.6)} L${p(a3, 7.2)} `;
  }
  return `<path d="${d}Z"/><circle cx="12" cy="12" r="3"/>`;
})();
const rosette = (() => {
  let pts = [];
  for (let i = 0; i < 16; i++) { const a = (i * Math.PI) / 8, r = i % 2 ? 5.2 : 10.5; pts.push(`${(12 + r * Math.cos(a)).toFixed(2)},${(12 + r * Math.sin(a)).toFixed(2)}`); }
  return `<polygon points="${pts.join(' ')}"/><circle cx="12" cy="12" r="3.2"/><circle cx="12" cy="12" r="1" fill="#fff"/>`;
})();
const icons = {
  book: S('<path d="M2.5 5.5c3-1.3 6.2-1.2 9.5.8v13c-3.3-2-6.5-2.1-9.5-.8z"/><path d="M21.5 5.5c-3-1.3-6.2-1.2-9.5.8v13c3.3-2 6.5-2.1 9.5-.8z"/>'),
  search: S('<circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5 21 21"/>'),
  bookmark: S('<path d="M6 3.5h12v17l-6-4.2-6 4.2z"/>'),
  bookmark_fill: S('<path d="M6 3.5h12v17l-6-4.2-6 4.2z" fill="#fff"/>'),
  settings: S(gear),
  chev_left: S('<path d="M15 5l-7 7 7 7"/>'),
  chev_right: S('<path d="M9 5l7 7-7 7"/>'),
  chev_up: S('<path d="M5 15l7-7 7 7"/>'),
  chev_down: S('<path d="M5 9l7 7 7-7"/>'),
  globe: S('<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c2.8 2.6 4 5.6 4 9s-1.2 6.4-4 9c-2.8-2.6-4-5.6-4-9s1.2-6.4 4-9z"/>'),
  pin: S('<path d="M12 21s-7-6.2-7-11.5A7 7 0 0 1 19 9.5C19 14.8 12 21 12 21z"/><circle cx="12" cy="9.5" r="2.5"/>'),
  wave: S('<path d="M4 10v4M8 7v10M12 4v16M16 8v8M20 11v2"/>'),
  close: S('<path d="M6 6l12 12M18 6 6 18"/>'),
  check: S('<path d="M4.5 12.5l5 5 10-11"/>'),
  plus: S('<path d="M12 5v14M5 12h14"/>'),
  minus: S('<path d="M5 12h14"/>'),
  trash: S('<path d="M4 7h16M9 7V4.5h6V7M6.5 7l1 13h9l1-13"/>'),
  backspace: S('<path d="M8.5 5H21v14H8.5L2.5 12z"/><path d="M11.5 9.5l5 5M16.5 9.5l-5 5"/>'),
  sun: S('<circle cx="12" cy="12" r="4"/><path d="M12 2v2.5M12 19.5V22M2 12h2.5M19.5 12H22M4.9 4.9l1.8 1.8M17.3 17.3l1.8 1.8M4.9 19.1l1.8-1.8M17.3 6.7l1.8-1.8"/>'),
  moon: S('<path d="M20 14.5A8.5 8.5 0 0 1 9.5 4a8.5 8.5 0 1 0 10.5 10.5z"/>'),
  recenter: S('<circle cx="12" cy="12" r="3"/><path d="M12 2v4M12 18v4M2 12h4M18 12h4"/><circle cx="12" cy="12" r="8"/>'),
  eye: S('<path d="M2 12s3.6-6.5 10-6.5S22 12 22 12s-3.6 6.5-10 6.5S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>'),
  list: S('<path d="M8 6h13M8 12h13M8 18h13M3.5 6h.01M3.5 12h.01M3.5 18h.01"/>'),
  info: S('<circle cx="12" cy="12" r="9"/><path d="M12 11v6M12 7.5h.01"/>'),
  rosette: S(rosette),
  kaaba: S('<path d="M5 8l7-3 7 3v9l-7 3-7-3z"/><path d="M5 8l7 3 7-3M12 11v9"/><path d="M5 11.5l7 3 7-3" stroke-width="2.4"/>'),
  dot: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9" fill="#fff"/></svg>`,
  ring: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><circle cx="12" cy="12" r="9" fill="none" stroke="#fff" stroke-width="2.5"/></svg>`,
  glow: `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><defs><radialGradient id="g"><stop offset="0" stop-color="#fff"/><stop offset=".35" stop-color="#fff" stop-opacity=".55"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></radialGradient></defs><circle cx="12" cy="12" r="12" fill="url(#g)"/></svg>`,
};

const html = `<html><head><meta charset="utf-8"><style>
${fonts.map(([f, w, p]) => `@font-face{font-family:${f};font-weight:${w};src:url(data:font/woff2;base64,${b64(p)})}`).join('\n')}
body{margin:0;background:#000}</style></head><body>
${fonts.map(([f, w]) => `<span style="font-family:${f};font-weight:${w}">a ا</span>`).join('')}
<canvas id="c" width="2048" height="2048"></canvas></body></html>`;

const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || undefined });
const page = await browser.newPage();
await page.setContent(html);
await page.evaluate(async () => { await document.fonts.ready; });

const surahNames = db.suras.map((s) => s.name.replace(/^سورة /, ''));
const result = await page.evaluate(async ({ icons, surahNames }) => {
  const c = document.getElementById('c');
  const g = c.getContext('2d');
  g.fillStyle = '#000'; g.fillRect(0, 0, 2048, 2048);
  g.fillStyle = '#fff';
  const out = [];
  // Shelf packer
  let sx = 2, sy = 2, sh = 0;
  const place = (w, h) => {
    if (sx + w + 2 > 2048) { sx = 2; sy += sh + 3; sh = 0; }
    const r = [sx, sy]; sx += w + 3; sh = Math.max(sh, h);
    if (sy + h > 2048) throw new Error('atlas full');
    return r;
  };
  // --- Latin glyphs ---
  const chars = [];
  for (let i = 32; i < 127; i++) chars.push(i);
  for (const ch of '–—·°’…•×→←') chars.push(ch.codePointAt(0));
  const PX = 56;
  for (const [name, css] of [['regular', `400 ${PX}px Inter`], ['semibold', `600 ${PX}px Inter`]]) {
    g.font = css;
    const m = g.measureText('Hg');
    const asc = Math.ceil(m.fontBoundingBoxAscent), desc = Math.ceil(m.fontBoundingBoxDescent);
    out.push(`F ${name} ${PX} ${asc} ${desc}`);
    for (const cp of chars) {
      const s = String.fromCodePoint(cp);
      const mm = g.measureText(s);
      const l = Math.ceil(mm.actualBoundingBoxLeft) + 2, r = Math.ceil(mm.actualBoundingBoxRight) + 2;
      const a = Math.ceil(mm.actualBoundingBoxAscent) + 2, d = Math.ceil(mm.actualBoundingBoxDescent) + 2;
      const w = Math.max(1, l + r), h = Math.max(1, a + d);
      if (cp === 32) { out.push(`G ${name} 32 0 0 0 0 0 0 ${mm.width.toFixed(2)}`); continue; }
      const [x, y] = place(w, h);
      g.fillText(s, x + l, y + a);
      out.push(`G ${name} ${cp} ${x} ${y} ${w} ${h} ${-l} ${-a} ${mm.width.toFixed(2)}`);
    }
  }
  // --- Images: icons ---
  sx = 2; sy += sh + 6; sh = 0;
  const loadImg = (svg) => new Promise((res) => { const im = new Image(); im.onload = () => res(im); im.src = 'data:image/svg+xml;base64,' + btoa(svg); });
  for (const [name, svg] of Object.entries(icons)) {
    const im = await loadImg(svg);
    const [x, y] = place(96, 96);
    g.drawImage(im, x, y, 96, 96);
    out.push(`I ${name} ${x} ${y} 96 96`);
  }
  // --- Arabic surah names ---
  sx = 2; sy += sh + 6; sh = 0;
  const arab = (name, text, css, pad) => {
    g.font = css; g.textAlign = 'left'; g.direction = 'ltr';
    const mm = g.measureText(text);
    const l = Math.ceil(mm.actualBoundingBoxLeft), r = Math.ceil(mm.actualBoundingBoxRight);
    const w = l + r + pad * 2;
    const a = Math.ceil(mm.actualBoundingBoxAscent) + pad, d = Math.ceil(mm.actualBoundingBoxDescent) + pad;
    const [x, y] = place(w, a + d);
    g.fillText(text, x + pad + l, y + a);
    out.push(`I ${name} ${x} ${y} ${w} ${a + d}`);
  };
  surahNames.forEach((t, i) => arab(`sura${i + 1}`, t, '700 46px Amiri', 3));
  // --- Title calligraphy ---
  arab('title', 'القرآن الكريم', '400 150px AmiriQuran', 8);
  arab('bismillah', 'بسم الله الرحمن الرحيم', '700 60px Amiri', 4);
  const img = g.getImageData(0, 0, 2048, 2048).data;
  let maxY = 0;
  for (let y = 2047; y >= 0 && !maxY; y--) for (let x = 0; x < 2048; x++) if (img[(y * 2048 + x) * 4]) { maxY = y; break; }
  return { lines: out, png: c.toDataURL('image/png'), usedY: maxY };
}, { icons, surahNames });

fs.writeFileSync(path.join(outDir, 'ui_atlas.txt'), result.lines.join('\n') + '\n');
fs.writeFileSync(path.join(outDir, 'ui_atlas_rgba.png'), Buffer.from(result.png.split(',')[1], 'base64'));
await browser.close();
console.log('atlas entries', result.lines.length, 'used height', result.usedY);

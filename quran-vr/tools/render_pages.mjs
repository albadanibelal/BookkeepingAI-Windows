// Renders the 604 Madina Mushaf pages into channel-packed PNG textures.
//
// Text comes from quran-madina-html (Madina 1405H edition, line-by-line layout,
// KFGQPC Hafs font). Each output PNG is RGB on black:
//   R = ink (Quran text, headers, page number)
//   G = gold ornament (surah title cartouche, page-number medallion)
//   B = ayah-end markers
// The headset shader colors each channel, so one small texture per page gives
// crisp, themeable pages (classic cream, night mode...).
//
// Usage: node render_pages.mjs <outDir> [firstPage] [lastPage]
import { chromium } from 'playwright';
import fs from 'fs';
import path from 'path';
import { createRequire } from 'module';
import * as qm from 'quran-meta';

const require = createRequire(import.meta.url);
const QMH = path.dirname(require.resolve('quran-madina-html/package.json'));
const FS = path.dirname(require.resolve('@fontsource/amiri/package.json'));

const outDir = process.argv[2] || '../assets/pages';
const first = +(process.argv[3] || 1);
const last = +(process.argv[4] || 604);
fs.mkdirSync(outDir, { recursive: true });

const db = JSON.parse(fs.readFileSync(path.join(QMH, 'assets/db/Madina05-Hafs-24px.json'), 'utf8'));
const hafs = qm.createHafs();

// ---- page geometry (px) ----
export const W = 1000, H = 1540;
const TEXT_W = 780;                  // justified line width
const FONT = 24 * TEXT_W / db.line_width;  // keep DB proportions
const TOP = 182, PITCH = 82;         // 15 lines: 182 .. 182+14*82

// Collect lines per page: {page: {line: [{sura, aya, t, s}]}}
const pages = {};
db.suras.forEach((sura, si) => {
  sura.ayas.forEach((a, ai) => {
    for (const r of a.r) {
      const pg = (pages[a.p] ||= {});
      (pg[r.l] ||= []).push({ sura: si, aya: ai, t: r.t, s: r.s });
    }
  });
});

const arabicDigits = (n) => String(n).replace(/\d/g, (d) => '٠١٢٣٤٥٦٧٨٩'[d]);
const JUZ_NAMES = ['الأول','الثاني','الثالث','الرابع','الخامس','السادس','السابع','الثامن','التاسع','العاشر',
  'الحادي عشر','الثاني عشر','الثالث عشر','الرابع عشر','الخامس عشر','السادس عشر','السابع عشر','الثامن عشر','التاسع عشر','العشرون',
  'الحادي والعشرون','الثاني والعشرون','الثالث والعشرون','الرابع والعشرون','الخامس والعشرون','السادس والعشرون','السابع والعشرون','الثامن والعشرون','التاسع والعشرون','الثلاثون'];

const svgData = (f) => 'data:image/svg+xml;base64,' + fs.readFileSync(f).toString('base64');
const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;');
// Wrap ayah-end markers ﴿N﴾ so they land in the blue channel.
const markText = (t) => esc(t).replace(/(﴿[^﴾]*﴾)/g, '<span class="m">$1</span>');

function pageModel(p) {
  const pg = pages[p] || {};
  const lines = [];
  for (let l = 1; l <= 15; l++) {
    const runs = (pg[l] || []).filter((r) => r.t !== '');
    if (!runs.length) continue;
    const text = runs.map((r) => r.t).join('');
    const isHeader = runs.length === 1 && runs[0].aya <= 1 && runs[0].t === db.suras[runs[0].sura].name;
    const isBasmala = runs.length === 1 && runs[0].aya === 1 && runs[0].sura !== 0 && runs[0].sura !== 8;
    lines.push({ l, text, header: isHeader, basmala: isBasmala, s: runs[0].s });
  }
  const pm = hafs.getPageMeta(p);
  const juz = hafs.findJuz(pm.first[0], pm.first[1]);
  const suraName = db.suras[pm.first[0] - 1].name;
  return { p, lines, juz, suraName };
}

function pageHtml(model) {
  const opening = model.p <= 2;
  let body = '';
  // Header outside the frame
  body += `<div class="hdr r">${esc('الجزء ' + JUZ_NAMES[model.juz - 1])}</div>`;
  body += `<div class="hdr l">${esc(model.suraName)}</div>`;
  const n = model.lines.length;
  // Opening pages: vertically centered block inside the cartouche
  const top0 = opening ? 800 - ((n - 1) * 92) / 2 : TOP;
  const pitch = opening ? 92 : PITCH;
  for (const ln of model.lines) {
    const idx = opening ? model.lines.indexOf(ln) : ln.l - 1;
    const y = top0 + idx * pitch;
    if (ln.header) {
      body += `<div class="sh" style="top:${y - 36}px"><div class="shb"></div><div class="sht">${esc(ln.text)}</div></div>`;
    } else {
      const center = ln.s < 0 || ln.basmala || opening;
      body += `<div class="ln ${center ? 'c' : 'j'}" style="top:${y - 44}px"><span>${markText(ln.text)}</span></div>`;
    }
  }
  body += `<div class="pn"><div class="pnb"></div><div class="pnt">${arabicDigits(model.p)}</div></div>`;
  return body;
}

const css = `
@font-face{font-family:Hafs;src:url(file://${QMH}/assets/fonts/Hafs.woff2)}
@font-face{font-family:Amiri;font-weight:400;src:url(file://${FS}/files/amiri-arabic-400-normal.woff2)}
@font-face{font-family:Amiri;font-weight:700;src:url(file://${FS}/files/amiri-arabic-700-normal.woff2)}
html,body{margin:0;background:#000}
#pg{position:relative;width:${W}px;height:${H}px;overflow:hidden;background:#000;direction:rtl}
.ln{position:absolute;right:${(W - TEXT_W) / 2}px;width:${TEXT_W}px;height:88px;line-height:88px;white-space:nowrap;
    font-family:Hafs;font-size:${FONT}px;color:#f00}
.ln span{display:inline-block;transform-origin:top right}
.ln.c{text-align:center}.ln.c span{transform-origin:top center}
.ln.j{text-align:right}
.m{color:#00f}
.hdr{position:absolute;top:22px;font-family:Amiri;font-size:30px;color:#f00}
.hdr.r{right:${(W - TEXT_W) / 2 + 6}px}.hdr.l{left:${(W - TEXT_W) / 2 + 6}px}
.sh{position:absolute;left:${(W - TEXT_W) / 2 - 8}px;width:${TEXT_W + 16}px;height:72px}
.shb{position:absolute;inset:0;background:#0f0;-webkit-mask:url(${svgData(path.join(QMH, 'assets/img/sura_border_sym4.svg'))}) center/100% 100% no-repeat}
.sht{position:absolute;inset:0;text-align:center;line-height:74px;font-family:Amiri;font-weight:700;font-size:34px;color:#f00}
.pn{position:absolute;left:50%;bottom:14px;width:84px;height:54px;margin-left:-42px}
.pnb{position:absolute;inset:0;background:#0f0;-webkit-mask:url(__PNMASK__) center/100% 100% no-repeat}
.pnt{position:absolute;inset:0;text-align:center;line-height:56px;font-family:Amiri;font-weight:700;font-size:28px;color:#f00}
`;

// Page-number medallion: an eight-lobed rosette outline.
const pnMask = path.resolve(outDir, '../../assets_src/pn_mask.svg');
fs.mkdirSync(path.dirname(pnMask), { recursive: true });
fs.writeFileSync(pnMask, `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 84 54">
<path fill-rule="evenodd" d="M42 2 C54 2 58 8 66 10 C76 12 82 20 82 27 C82 34 76 42 66 44 C58 46 54 52 42 52 C30 52 26 46 18 44 C8 42 2 34 2 27 C2 20 8 12 18 10 C26 8 30 2 42 2 Z
M42 6 C52 6 56 11 64 13 C73 15 78 21 78 27 C78 33 73 39 64 41 C56 43 52 48 42 48 C32 48 28 43 20 41 C11 39 6 33 6 27 C6 21 11 15 20 13 C28 11 32 6 42 6 Z"/></svg>`);

const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || undefined, args: ['--font-render-hinting=none'] });
const page = await browser.newPage({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
const htmlFile = path.resolve(outDir, '../../assets_src/_page.html');
fs.writeFileSync(htmlFile, `<html><head><meta charset="utf-8"><style>${css.replace('__PNMASK__', svgData(pnMask))}</style></head><body><div id="pg"></div></body></html>`);
await page.goto('file://' + htmlFile);
// Warm fonts
await page.evaluate(async () => {
  const d = document.getElementById('pg');
  d.innerHTML = '<div class="ln j" style="top:0"><span>بِسْمِ ٱللَّهِ ﴿١﴾</span></div><div class="hdr r">ا</div><div class="sht" style="position:static;font-weight:700">ا</div>';
  await document.fonts.ready;
  await Promise.all([...document.fonts].map((f) => f.load()));
});

for (let p = first; p <= last; p++) {
  const model = pageModel(p);
  await page.evaluate((html) => {
    const d = document.getElementById('pg');
    d.innerHTML = html;
    // Justify: scale each line's natural width to exactly the text width.
    for (const ln of d.querySelectorAll('.ln.j')) {
      const sp = ln.firstChild;
      const w = sp.getBoundingClientRect().width;
      if (w > 0) sp.style.transform = `scaleX(${ln.clientWidth / w})`;
    }
    for (const ln of d.querySelectorAll('.ln.c')) {
      const sp = ln.firstChild;
      const w = sp.getBoundingClientRect().width;
      if (w > ln.clientWidth) sp.style.transform = `scaleX(${ln.clientWidth / w})`;
    }
  }, pageHtml(model));
  await page.locator('#pg').screenshot({ path: path.join(outDir, `p${String(p).padStart(3, '0')}.png`) });
  if (p % 50 === 0) console.log('page', p);
}
await browser.close();
console.log('done', first, last);

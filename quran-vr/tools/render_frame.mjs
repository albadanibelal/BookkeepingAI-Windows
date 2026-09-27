// Renders the shared Mushaf page frame textures (full color, opaque):
//   frame.png       – regular page: cream paper + green/gold geometric border
//   frame_open.png  – pages 1-2: border + illuminated cartouche around the text
import { chromium } from 'playwright';
import fs from 'fs';
import path from 'path';

const outDir = process.argv[2] || '../assets';
const W = 1000, H = 1540;
const GOLD = '#b8913f', GOLD_L = '#e2c47a', GREEN = '#1f5e46', GREEN_D = '#14402f', RED = '#8c2f2a';
const PAPER = '#f6eedb';

// Band geometry
const O = { x0: 36, y0: 84, x1: 964, y1: 1464 };   // outer edge
const B = 42;                                     // band width

function starTile(s) {
  // One band tile: eight-pointed star (two rotated squares) with a rosette core.
  const c = s / 2, r = s * 0.40, q = r * 0.72;
  const sq = (rot) => `<rect x="${c - q}" y="${c - q}" width="${2 * q}" height="${2 * q}" transform="rotate(${rot} ${c} ${c})"/>`;
  return `
  <rect width="${s}" height="${s}" fill="${GREEN}"/>
  <g fill="${GOLD}" stroke="${GOLD_L}" stroke-width="1">${sq(0)}${sq(45)}</g>
  <g fill="${GREEN_D}">${sq(0).replace(/q/g, '')}</g>
  <circle cx="${c}" cy="${c}" r="${r * 0.46}" fill="${GREEN_D}" stroke="${GOLD_L}" stroke-width="1.5"/>
  <circle cx="${c}" cy="${c}" r="${r * 0.18}" fill="${GOLD_L}"/>
  <circle cx="0" cy="0" r="${s * 0.13}" fill="${GOLD}"/><circle cx="${s}" cy="0" r="${s * 0.13}" fill="${GOLD}"/>
  <circle cx="0" cy="${s}" r="${s * 0.13}" fill="${GOLD}"/><circle cx="${s}" cy="${s}" r="${s * 0.13}" fill="${GOLD}"/>`;
}

function border() {
  const { x0, y0, x1, y1 } = O;
  const i = { x0: x0 + B, y0: y0 + B, x1: x1 - B, y1: y1 - B };
  // Band = outer rect minus inner rect, filled with the star pattern.
  return `
  <defs>
    <pattern id="band" width="${B}" height="${B}" patternUnits="userSpaceOnUse" x="${x0}" y="${y0}">${starTile(B)}</pattern>
    <radialGradient id="rose" cx="50%" cy="50%" r="50%">
      <stop offset="0" stop-color="${GOLD_L}"/><stop offset="1" stop-color="${GOLD}"/>
    </radialGradient>
  </defs>
  <path fill-rule="evenodd" fill="url(#band)" d="M${x0} ${y0}H${x1}V${y1}H${x0}Z M${i.x0} ${i.y0}V${i.y1}H${i.x1}V${i.y0}Z"/>
  <rect x="${x0 - 5}" y="${y0 - 5}" width="${x1 - x0 + 10}" height="${y1 - y0 + 10}" fill="none" stroke="${GOLD}" stroke-width="3"/>
  <rect x="${x0 - 11}" y="${y0 - 11}" width="${x1 - x0 + 22}" height="${y1 - y0 + 22}" fill="none" stroke="${GREEN}" stroke-width="1.5"/>
  <rect x="${x0}" y="${y0}" width="${x1 - x0}" height="${y1 - y0}" fill="none" stroke="${GOLD_L}" stroke-width="2"/>
  <rect x="${i.x0}" y="${i.y0}" width="${i.x1 - i.x0}" height="${i.y1 - i.y0}" fill="none" stroke="${GOLD}" stroke-width="3"/>
  <rect x="${i.x0 + 7}" y="${i.y0 + 7}" width="${i.x1 - i.x0 - 14}" height="${i.y1 - i.y0 - 14}" fill="none" stroke="${RED}" stroke-width="1.2" opacity=".7"/>
  ${[[x0, y0], [x1, y0], [x0, y1], [x1, y1]].map(([x, y]) => corner(x + (x === x0 ? B / 2 : -B / 2), y + (y === y0 ? B / 2 : -B / 2))).join('')}`;
}

function corner(cx, cy) {
  const pts = [];
  for (let k = 0; k < 16; k++) {
    const a = (k * Math.PI) / 8, r = k % 2 ? 17 : 30;
    pts.push(`${(cx + r * Math.cos(a)).toFixed(1)},${(cy + r * Math.sin(a)).toFixed(1)}`);
  }
  return `<polygon points="${pts.join(' ')}" fill="url(#rose)" stroke="${GREEN_D}" stroke-width="1.5"/>
    <circle cx="${cx}" cy="${cy}" r="10" fill="${GREEN}" stroke="${GOLD_L}" stroke-width="2"/>`;
}

function cartouche() {
  // Illuminated panel for the two opening pages: arabesque field with an
  // ogee-arched window of paper where the text sits.
  const x0 = 78, x1 = 922, y0 = 126, y1 = 1422;       // inner frame
  const wx0 = 96, wx1 = 904, wy0 = 330, wy1 = 1270;   // text window
  const mid = (wx0 + wx1) / 2;
  const win = `M${wx0} ${wy1} C${wx0} ${wy1 + 90} ${mid - 180} ${wy1 + 40} ${mid} ${wy1 + 130} C${mid + 180} ${wy1 + 40} ${wx1} ${wy1 + 90} ${wx1} ${wy1}` +
    ` L${wx1} ${wy0} C${wx1} ${wy0 - 90} ${mid + 180} ${wy0 - 40} ${mid} ${wy0 - 130} C${mid - 180} ${wy0 - 40} ${wx0} ${wy0 - 90} ${wx0} ${wy0}Z`;
  let field = '';
  // Arabesque field: tiled interlocking circles.
  for (let y = y0 + 20; y < y1; y += 56)
    for (let x = x0 + 20 + (((y - y0) / 56) % 2) * 28; x < x1; x += 56)
      field += `<circle cx="${x}" cy="${y}" r="30"/>`;
  return `
  <clipPath id="fld"><rect x="${x0}" y="${y0}" width="${x1 - x0}" height="${y1 - y0}"/></clipPath>
  <g clip-path="url(#fld)">
    <rect x="${x0}" y="${y0}" width="${x1 - x0}" height="${y1 - y0}" fill="#e3ecd9"/>
    <g fill="none" stroke="${GOLD}" stroke-width="2" opacity=".75">${field}</g>
    <g fill="${GREEN}" opacity=".18">${field.replace(/r="30"/g, 'r="9"')}</g>
  </g>
  <path d="${win}" fill="${PAPER}" stroke="${GOLD}" stroke-width="10"/>
  <path d="${win}" fill="none" stroke="${GREEN}" stroke-width="3"/>
  <path d="${win}" fill="none" stroke="${GOLD_L}" stroke-width="1.5" transform="translate(${mid} 800) scale(.975) translate(${-mid} -800)"/>`;
}

function svg(open) {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">
  <defs>
    <filter id="grain"><feTurbulence type="fractalNoise" baseFrequency=".9" numOctaves="2" seed="7"/>
      <feColorMatrix values="0 0 0 0 .45  0 0 0 0 .38  0 0 0 0 .25  0 0 0 .07 0"/></filter>
    <radialGradient id="vig" cx="50%" cy="50%" r="75%"><stop offset=".6" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#6b5530" stop-opacity=".18"/></radialGradient>
  </defs>
  <rect width="${W}" height="${H}" fill="${PAPER}"/>
  <rect width="${W}" height="${H}" filter="url(#grain)"/>
  <rect width="${W}" height="${H}" fill="url(#vig)"/>
  ${open ? cartouche() : ''}
  ${border()}
  </svg>`;
}

const browser = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || undefined });
const page = await browser.newPage({ viewport: { width: W, height: H } });
for (const [name, open] of [['frame.png', false], ['frame_open.png', true]]) {
  await page.setContent(`<html><body style="margin:0">${svg(open)}</body></html>`);
  await page.locator('svg').screenshot({ path: path.join(outDir, name) });
}
await browser.close();
console.log('frames done');

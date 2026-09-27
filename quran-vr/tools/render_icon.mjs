// App icon: gold eight-pointed star medallion with "القرآن" on deep green.
import { chromium } from 'playwright';
import fs from 'fs';
import path from 'path';
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
const f = path.join(path.dirname(require.resolve('@fontsource/amiri-quran/package.json')), 'files/amiri-quran-arabic-400-normal.woff2');
const out = process.argv[2], size = +(process.argv[3] || 512);
const star = (r1, r2, n = 16) => Array.from({ length: n }, (_, i) => { const a = (i * Math.PI * 2) / n - Math.PI / 2, r = i % 2 ? r2 : r1; return `${256 + r * Math.cos(a)},${256 + r * Math.sin(a)}`; }).join(' ');
const html = `<html><head><style>@font-face{font-family:AQ;src:url(data:font/woff2;base64,${fs.readFileSync(f).toString('base64')})}
body{margin:0;background:transparent}</style></head><body>
<svg id="s" xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 512 512">
<defs><radialGradient id="bg" cx="50%" cy="40%" r="75%"><stop offset="0" stop-color="#1f6b52"/><stop offset="1" stop-color="#0a2a22"/></radialGradient>
<linearGradient id="gold" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f7e3a3"/><stop offset="1" stop-color="#c39437"/></linearGradient></defs>
<rect width="512" height="512" rx="112" fill="url(#bg)"/>
<polygon points="${star(228, 196)}" fill="none" stroke="url(#gold)" stroke-width="10" stroke-linejoin="round"/>
<circle cx="256" cy="256" r="160" fill="#0c3328" stroke="url(#gold)" stroke-width="6"/>
<circle cx="256" cy="256" r="146" fill="none" stroke="#c39437" stroke-width="2" opacity=".7"/>
<text x="256" y="300" text-anchor="middle" font-family="AQ" font-size="118" fill="url(#gold)">القرآن</text>
</svg></body></html>`;
const b = await chromium.launch({ executablePath: process.env.CHROMIUM_PATH || undefined });
const p = await b.newPage();
await p.setContent(html);
await p.evaluate(async () => { await document.fonts.ready; await document.fonts.load('118px AQ', 'القرآن'); });
await p.locator('#s').screenshot({ path: out, omitBackground: true });
await b.close();
console.log('icon', out);

// Exports compact Quran metadata (surahs, pages, juz) for the native app.
// Source: quran-meta (Hafs, Madina Mushaf 604-page layout).
import * as m from 'quran-meta';
import fs from 'fs';

const out = process.argv[2] || '../assets/meta.txt';
const h = m.createHafs();
const lines = [];

for (let s = 1; s <= 114; s++) {
  const meta = h.getSurahMeta(s);
  const [translit, meaning] = m.getSurahName(s);
  const startPage = h.findPage(s, 1);
  // S|num|translit|meaning|ayahs|startPage|M(eccan)/D(medinan)
  lines.push(['S', s, translit, meaning, meta.ayahCount, startPage, meta.isMeccan ? 'M' : 'D'].join('|'));
}
for (let p = 1; p <= 604; p++) {
  const pm = h.getPageMeta(p);
  const [s, a] = pm.first;
  const juz = h.findJuz(s, a);
  // P|page|juz|firstSurah|firstAyah|lastSurah|lastAyah
  lines.push(['P', p, juz, s, a, pm.last[0], pm.last[1]].join('|'));
}
for (let j = 1; j <= 30; j++) {
  const jm = h.getJuzMeta(j);
  const [s, a] = jm.first;
  lines.push(['J', j, s, a, h.findPage(s, a)].join('|'));
}
fs.writeFileSync(out, lines.join('\n') + '\n');
console.log('wrote', out, lines.length, 'records');

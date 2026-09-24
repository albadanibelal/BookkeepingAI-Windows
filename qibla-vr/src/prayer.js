import { Coordinates, CalculationMethod, PrayerTimes, HighLatitudeRule, Madhab } from 'adhan';

export const METHODS = {
  auto: 'Automatic',
  MuslimWorldLeague: 'Muslim World League',
  NorthAmerica: 'ISNA (North America)',
  UmmAlQura: 'Umm al-Qura, Makkah',
  Egyptian: 'Egyptian General Authority',
  Karachi: 'University of Islamic Sciences, Karachi',
  Dubai: 'Dubai',
  Qatar: 'Qatar',
  Kuwait: 'Kuwait',
  Singapore: 'Singapore, Malaysia, Indonesia',
  Turkey: 'Diyanet, Türkiye',
  Tehran: 'Institute of Geophysics, Tehran',
  MoonsightingCommittee: 'Moonsighting Committee',
};

const box = (loc, s, n, w, e) => loc.lat >= s && loc.lat <= n && loc.lon >= w && loc.lon <= e;

/** A sensible regional default, chosen from coordinates alone. */
export function autoMethod(loc) {
  if (box(loc, 15, 72, -170, -50)) return 'NorthAmerica';
  if (box(loc, 16, 32.5, 34.5, 55.7)) return 'UmmAlQura';
  if (box(loc, 22, 26.5, 51.5, 56.5)) return 'Dubai';
  if (box(loc, 24.4, 26.3, 50.7, 51.7)) return 'Qatar';
  if (box(loc, 28.5, 30.2, 46.5, 48.6)) return 'Kuwait';
  if (box(loc, 22, 31.8, 24.5, 36.5)) return 'Egyptian';
  if (box(loc, 36, 42.2, 26, 45)) return 'Turkey';
  if (box(loc, 25, 39.8, 44, 63.3)) return 'Tehran';
  if (box(loc, 5, 37, 60.5, 97.5)) return 'Karachi';
  if (box(loc, -11, 7.5, 95, 141)) return 'Singapore';
  return 'MuslimWorldLeague';
}

const ORDER = [
  ['fajr', 'Fajr'], ['sunrise', 'Sunrise'], ['dhuhr', 'Dhuhr'],
  ['asr', 'Asr'], ['maghrib', 'Maghrib'], ['isha', 'Isha'],
];

// Adhan reads the calendar day from a Date's local fields, so build a Date whose
// local Y/M/D equal the calendar day at the location.
function dayAt(now, tz, offsetDays = 0) {
  const [y, m, d] = new Intl.DateTimeFormat('en-CA', { timeZone: tz, year: 'numeric', month: '2-digit', day: '2-digit' })
    .format(now).split('-').map(Number);
  return new Date(y, m - 1, d + offsetDays);
}

export function prayerInfo(loc, method = 'auto', hanafi = false, tz = undefined, now = new Date()) {
  const key = method === 'auto' ? autoMethod(loc) : method;
  const params = CalculationMethod[key]();
  params.madhab = hanafi ? Madhab.Hanafi : Madhab.Shafi;
  params.highLatitudeRule = HighLatitudeRule.recommended(new Coordinates(loc.lat, loc.lon));
  const coords = new Coordinates(loc.lat, loc.lon);
  const today = new PrayerTimes(coords, tz ? dayAt(now, tz) : now, params);
  const times = ORDER.map(([id, name]) => ({ id, name, time: today[id] }));

  let nextId = today.nextPrayer(now);
  let next;
  if (nextId === 'none') {
    const tomorrow = new PrayerTimes(coords, tz ? dayAt(now, tz, 1) : new Date(now.getTime() + 86400000), params);
    next = { id: 'fajr', name: 'Fajr', time: tomorrow.fajr };
  } else {
    next = times.find((t) => t.id === nextId);
  }
  const current = today.currentPrayer(now);
  return { times, next, current, method: key, tz };
}

export function formatTime(d, timeZone = undefined) {
  return d.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit', timeZone });
}

export function formatIn(ms) {
  const m = Math.max(0, Math.round(ms / 60000));
  const h = Math.floor(m / 60);
  if (h === 0) return `in ${m} min`;
  return `in ${h} hr ${m % 60} min`;
}

// Geographic and astronomical helpers. Pure functions, no Three.js.

import { CITIES } from './cities.js';

export const KAABA = { lat: 21.422487, lon: 39.826206 };
const EARTH_RADIUS_KM = 6371.0088;
const DEG = Math.PI / 180;

/** Initial great-circle bearing from `from` to `to`, degrees clockwise from true north. */
export function bearing(from, to) {
  const φ1 = from.lat * DEG, φ2 = to.lat * DEG, Δλ = (to.lon - from.lon) * DEG;
  const y = Math.sin(Δλ) * Math.cos(φ2);
  const x = Math.cos(φ1) * Math.sin(φ2) - Math.sin(φ1) * Math.cos(φ2) * Math.cos(Δλ);
  return (Math.atan2(y, x) / DEG + 360) % 360;
}

export function qiblaBearing(loc) {
  return bearing(loc, KAABA);
}

/** Great-circle distance in km (haversine). */
export function distanceKm(a, b) {
  const φ1 = a.lat * DEG, φ2 = b.lat * DEG;
  const dφ = φ2 - φ1, dλ = (b.lon - a.lon) * DEG;
  const h = Math.sin(dφ / 2) ** 2 + Math.cos(φ1) * Math.cos(φ2) * Math.sin(dλ / 2) ** 2;
  return 2 * EARTH_RADIUS_KM * Math.asin(Math.min(1, Math.sqrt(h)));
}

/**
 * Subsolar point (where the sun is directly overhead) for a given instant.
 * NOAA low-precision solar position; good to ~0.1°, plenty for lighting.
 */
export function subsolarPoint(date = new Date()) {
  const jd = date.getTime() / 86400000 + 2440587.5;
  const n = jd - 2451545.0;
  const L = (280.46 + 0.9856474 * n) % 360;
  const g = ((357.528 + 0.9856003 * n) % 360) * DEG;
  const λ = (L + 1.915 * Math.sin(g) + 0.02 * Math.sin(2 * g)) * DEG;
  const ε = (23.439 - 0.0000004 * n) * DEG;
  const decl = Math.asin(Math.sin(ε) * Math.sin(λ));
  const ra = Math.atan2(Math.cos(ε) * Math.sin(λ), Math.cos(λ));
  // Greenwich mean sidereal time, degrees.
  const gmst = (280.46061837 + 360.98564736629 * n) % 360;
  let lon = ra / DEG - gmst;
  lon = ((lon + 540) % 360) - 180;
  return { lat: decl / DEG, lon };
}

/** Nearest known city within `maxKm`, or null. */
export function nearestCity(loc, maxKm = 80) {
  let best = null, bestD = Infinity;
  for (const c of CITIES) {
    const d = distanceKm(loc, c);
    if (d < bestD) { bestD = d; best = c; }
  }
  return bestD <= maxKm ? best : null;
}

export function formatCoords({ lat, lon }) {
  const ns = lat >= 0 ? 'N' : 'S';
  const ew = lon >= 0 ? 'E' : 'W';
  return `${Math.abs(lat).toFixed(2)}° ${ns}, ${Math.abs(lon).toFixed(2)}° ${ew}`;
}

export function placeName(loc) {
  if (loc.name) return loc.name;
  const c = nearestCity(loc);
  return c ? c.name : formatCoords(loc);
}

const COMPASS_POINTS = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
export function compassPoint(deg) {
  return COMPASS_POINTS[Math.round(((deg % 360) + 360) % 360 / 45) % 8];
}

export function formatDistance(km) {
  const imperial = (navigator.language || '').toLowerCase() === 'en-us';
  const v = imperial ? km * 0.621371 : km;
  const unit = imperial ? 'mi' : 'km';
  return `${Math.round(v).toLocaleString()} ${unit}`;
}

/** Signed smallest angle a → b in degrees, in (-180, 180]. Positive = turn right. */
export function angleDelta(a, b) {
  let d = ((b - a) % 360 + 540) % 360 - 180;
  return d === -180 ? 180 : d;
}

export function deviceTimeZone() {
  return Intl.DateTimeFormat().resolvedOptions().timeZone;
}

/**
 * Best time zone for a location without a network lookup: its own, else the
 * nearest listed city's (within ~500 km), else a whole-hour zone from longitude.
 */
export function timeZoneFor(loc) {
  if (loc.tz) return loc.tz;
  const c = nearestCity(loc, 500);
  if (c) return c.tz;
  const h = Math.round(loc.lon / 15);
  return h === 0 ? 'Etc/GMT' : `Etc/GMT${h > 0 ? '-' : '+'}${Math.abs(h)}`;
}

import '@fontsource-variable/inter/opsz.css';
import './styles.css';
import * as THREE from 'three';
import { Globe } from './earth.js';
import { createStars, createSun, createPlatform, CompassRing, createHorizonBeacon } from './space.js';
import { Hud, InfoCard, Button, MakkahLabel } from './panels.js';
import { ICON, C } from './glass.js';
import {
  KAABA, qiblaBearing, distanceKm, placeName, formatCoords, compassPoint, formatDistance, angleDelta,
  timeZoneFor, deviceTimeZone,
} from './geo.js';
import { CITIES } from './cities.js';
import { prayerInfo, METHODS, formatTime, formatIn } from './prayer.js';
import { chime, unlockAudio } from './audio.js';

const DEG = Math.PI / 180;
const Y_AXIS = new THREE.Vector3(0, 1, 0);

// ------------------------------------------------------------------ storage

const store = {
  get(k, fallback = null) {
    try { const v = localStorage.getItem(`qibla.${k}`); return v === null ? fallback : JSON.parse(v); } catch { return fallback; }
  },
  set(k, v) {
    try { localStorage.setItem(`qibla.${k}`, JSON.stringify(v)); } catch { /* storage unavailable */ }
  },
};

// -------------------------------------------------------------------- state

const state = {
  location: store.get('location'),
  method: store.get('method', 'auto'),
  hanafi: store.get('hanafi', false),
  sound: store.get('sound', true),
  view: 'orbit',          // 'orbit' — standing above your home; 'table' — a globe in front of you
  mode: 'normal',         // 'normal' | 'calibrate' | 'pick'
  northYaw: 0,            // world yaw (radians) that points to true north
  calibrated: false,      // true once north is known in the current space
  headingSource: 'virtual', // 'virtual' | 'xr' | 'compass'
  bearing: 0,
  distance: 0,
  aligned: false,
  alignAmount: 0,
  ar: false,
};

// ---------------------------------------------------------------- renderer

const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.setSize(window.innerWidth, window.innerHeight);
renderer.xr.enabled = true;
renderer.xr.setReferenceSpaceType('local-floor');
renderer.xr.setFoveation?.(0.6);
document.getElementById('stage').appendChild(renderer.domElement);

const scene = new THREE.Scene();
const SPACE = new THREE.Color(0x000105);
scene.background = SPACE;

const camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.02, 1200);
camera.position.set(0, 1.6, 0);
const look = { yaw: 0.35, pitch: -0.28 };

const stars = createStars();
stars.material.uniforms.pixelRatio.value = renderer.getPixelRatio();
scene.add(stars);
const sun = createSun();
scene.add(sun);
const platform = createPlatform();
scene.add(platform);
const ring = new CompassRing();
scene.add(ring.group);
const beacon = createHorizonBeacon();
scene.add(beacon);
const globe = new Globe();
scene.add(globe.group);
const hud = new Hud();
scene.add(hud.mesh);
const label = new MakkahLabel();
scene.add(label.mesh);
const card = new InfoCard();
scene.add(card.group);

const buttons = {
  north: new Button('Set North', ICON.compass, () => setMode(state.mode === 'calibrate' ? 'normal' : 'calibrate')),
  location: new Button('Location', ICON.location, () => setMode(state.mode === 'pick' ? 'normal' : 'pick')),
  view: new Button('Globe', ICON.globe, () => setView(state.view === 'orbit' ? 'table' : 'orbit')),
  sound: new Button('Sound', (g, x, y, s, c) => ICON.speaker(g, x, y, s, c, state.sound), () => {
    state.sound = !state.sound;
    store.set('sound', state.sound);
    buttons.sound.set(state.sound ? 'Sound' : 'Muted', { active: false });
    buttons.sound.draw();
  }),
};
card.addButtons(Object.values(buttons));
if (!state.sound) buttons.sound.set('Muted');

// ------------------------------------------------------------------ layout

const head = { pos: new THREE.Vector3(0, 1.6, 0), quat: new THREE.Quaternion() };

function headYaw() {
  return new THREE.Euler().setFromQuaternion(head.quat, 'YXZ').y;
}

function applyLayout(instant = false) {
  if (state.view === 'orbit') {
    globe.setLayout(new THREE.Vector3(0, -5 - 1.25, 0), 5, { instant, dotScale: 1, beam: 1.0 });
  } else {
    const yaw = headYaw();
    const p = head.pos.clone().add(new THREE.Vector3(-Math.sin(yaw), 0, -Math.cos(yaw)).multiplyScalar(0.72));
    p.y = Math.max(0.75, head.pos.y - 0.5);
    globe.setLayout(p, 0.26, { instant, dotScale: 1.8, beam: 0.35 });
  }
  platform.visible = !state.ar && state.view === 'orbit';
}

function setView(view) {
  state.view = view;
  buttons.view.set(view === 'orbit' ? 'Globe' : 'Orbit', { active: false });
  applyLayout();
  // The tabletop globe appears where you're looking; slide the card aside to make room.
  if (view === 'table' && renderer.xr.isPresenting) card.summon(head.pos, head.quat, 0.8);
  syncDom();
}

function setMode(mode) {
  state.mode = mode;
  buttons.north.set('Set North', { active: mode === 'calibrate' });
  buttons.location.set('Location', { active: mode === 'pick' });
  // Picking is easiest on a globe in front of you.
  if (mode === 'pick' && state.view !== 'table') setView('table');
}

// ------------------------------------------------------------ location/north

function applyLocation(loc, { persist = true } = {}) {
  state.location = { lat: loc.lat, lon: loc.lon, ...(loc.name ? { name: loc.name } : {}) };
  state.location.tz = timeZoneFor(loc);
  if (persist) store.set('location', state.location);
  state.bearing = qiblaBearing(state.location);
  state.distance = distanceKm(state.location, KAABA);
  globe.setLocation(state.location);
  ring.set(state.bearing, state.northYaw);
  syncDom();
}

function applyNorth(yaw) {
  state.northYaw = yaw;
  globe.setNorthYaw(yaw);
  ring.set(state.bearing, yaw);
}

function locate({ quiet = false } = {}) {
  if (!navigator.geolocation) return;
  dom.locate.disabled = true;
  navigator.geolocation.getCurrentPosition(
    (p) => {
      dom.locate.disabled = false;
      applyLocation({ lat: p.coords.latitude, lon: p.coords.longitude, tz: deviceTimeZone() });
      if (state.mode === 'pick') setMode('normal');
    },
    () => {
      dom.locate.disabled = false;
      if (!quiet) toast('Couldn’t get your location. Choose a city instead.');
      if (!state.location && renderer.xr.isPresenting) setMode('pick');
    },
    { enableHighAccuracy: false, timeout: 12000, maximumAge: 600000 },
  );
}

// North in the headset. Quest has no magnetometer, so the user shows us north
// once; we remember it with a persistent spatial anchor when the runtime allows.
let northAnchor = null;
let pendingNorthYaw = null;

async function createNorthAnchor(frame, yaw) {
  const session = renderer.xr.getSession();
  const ref = renderer.xr.getReferenceSpace();
  if (!frame.createAnchor || !session) return;
  try {
    const q = new THREE.Quaternion().setFromAxisAngle(Y_AXIS, yaw);
    const pose = new XRRigidTransform({ x: head.pos.x, y: 0, z: head.pos.z }, { x: q.x, y: q.y, z: q.z, w: q.w });
    const anchor = await frame.createAnchor(pose, ref);
    northAnchor = anchor;
    if (anchor.requestPersistentHandle) {
      const old = store.get('northAnchor');
      if (old && session.deletePersistentAnchor) session.deletePersistentAnchor(old).catch(() => {});
      store.set('northAnchor', await anchor.requestPersistentHandle());
    }
  } catch (err) {
    console.info('Anchors unavailable; north will be asked for each session.', err);
  }
}

async function restoreNorthAnchor(session) {
  const uuid = store.get('northAnchor');
  if (!uuid || !session.restorePersistentAnchor) return;
  try {
    northAnchor = await session.restorePersistentAnchor(uuid);
  } catch {
    store.set('northAnchor', null);
  }
}

function trackNorthAnchor(frame) {
  if (!northAnchor) return;
  const pose = frame.getPose(northAnchor.anchorSpace, renderer.xr.getReferenceSpace());
  if (!pose) return;
  const o = pose.transform.orientation;
  const yaw = new THREE.Euler().setFromQuaternion(new THREE.Quaternion(o.x, o.y, o.z, o.w), 'YXZ').y;
  if (!state.calibrated || Math.abs(angleDelta(yaw / DEG, state.northYaw / DEG)) > 0.2) applyNorth(yaw);
  state.calibrated = true;
}

// ------------------------------------------------------------------- input

const raycaster = new THREE.Raycaster();
const pointers = [];
const tmpMat = new THREE.Matrix4();

function makeRayVisual() {
  const g = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(0, 0, 0), new THREE.Vector3(0, 0, -1)]);
  g.setAttribute('color', new THREE.Float32BufferAttribute([1, 1, 1, 0, 0, 0], 3));
  const line = new THREE.Line(g, new THREE.LineBasicMaterial({ vertexColors: true, transparent: true, opacity: 0.8, blending: THREE.AdditiveBlending }));
  line.scale.z = 1.5;
  const cursor = new THREE.Mesh(
    new THREE.RingGeometry(0.006, 0.009, 32),
    new THREE.MeshBasicMaterial({ color: 0xffffff, transparent: true, depthTest: false }),
  );
  cursor.renderOrder = 30;
  cursor.visible = false;
  scene.add(cursor);
  return { line, cursor };
}

for (let i = 0; i < 2; i++) {
  const controller = renderer.xr.getController(i);
  const { line, cursor } = makeRayVisual();
  controller.add(line);
  const p = { controller, line, cursor, hover: null, globeHit: null, inputSource: null };
  controller.addEventListener('connected', (e) => { p.inputSource = e.data; });
  controller.addEventListener('disconnected', () => { p.inputSource = null; p.cursor.visible = false; });
  controller.addEventListener('select', () => onSelect(p));
  controller.addEventListener('squeeze', () => { card.summon(head.pos, head.quat); });
  scene.add(controller);
  pointers.push(p);
}

const mousePointer = { ray: new THREE.Ray(), hover: null, globeHit: null, cursor: makeRayVisual().cursor, isMouse: true };

function buttonMeshes() {
  return Object.values(buttons).map((b) => b.mesh);
}

function castPointer(p) {
  if (!p.isMouse) {
    tmpMat.identity().extractRotation(p.controller.matrixWorld);
    raycaster.ray.origin.setFromMatrixPosition(p.controller.matrixWorld);
    raycaster.ray.direction.set(0, 0, -1).applyMatrix4(tmpMat);
  } else {
    raycaster.ray.copy(p.ray);
  }
  const hits = raycaster.intersectObjects(buttonMeshes(), false);
  const hover = hits[0]?.object.userData.button ?? null;
  if (hover !== p.hover) {
    p.hover?.setHover(false);
    hover?.setHover(true);
    if (hover && p.inputSource) pulse(p.inputSource, 0.15, 12);
    p.hover = hover;
  }
  let point = hits[0]?.point ?? null;
  p.globeHit = null;
  if (!hover && state.mode === 'pick') {
    const hit = raycaster.intersectObject(globe.earth, false)[0];
    if (hit) { p.globeHit = globe.pick(raycaster); point = hit.point; }
  }
  if (p.line) p.line.scale.z = point ? raycaster.ray.origin.distanceTo(point) : 1.5;
  p.cursor.visible = !!point;
  if (point) {
    p.cursor.position.copy(point);
    p.cursor.lookAt(head.pos);
    p.cursor.scale.setScalar(Math.max(0.6, head.pos.distanceTo(point)));
  }
}

function onSelect(p) {
  unlockAudio();
  if (p.hover) { p.hover.press(); if (p.inputSource) pulse(p.inputSource, 0.4, 30); return; }
  if (state.mode === 'calibrate') {
    // The pointing direction becomes north.
    const dir = raycaster.ray.direction.clone();
    if (!p.isMouse) {
      tmpMat.identity().extractRotation(p.controller.matrixWorld);
      dir.set(0, 0, -1).applyMatrix4(tmpMat);
    }
    const yaw = Math.atan2(-dir.x, -dir.z);
    applyNorth(yaw);
    state.calibrated = true;
    pendingNorthYaw = yaw;
    setMode('normal');
    if (state.sound) chime();
    return;
  }
  if (state.mode === 'pick' && p.globeHit) {
    applyLocation(p.globeHit);
    setMode('normal');
    if (state.sound) chime();
  }
}

function pulse(inputSource, intensity, ms) {
  inputSource?.gamepad?.hapticActuators?.[0]?.pulse?.(intensity, ms);
}

// Desktop / phone: drag to look, click to press.
{
  const el = renderer.domElement;
  let down = null;
  const toNdc = (e) => new THREE.Vector2((e.clientX / window.innerWidth) * 2 - 1, -(e.clientY / window.innerHeight) * 2 + 1);
  el.addEventListener('pointerdown', (e) => { down = { x: e.clientX, y: e.clientY, yaw: look.yaw, pitch: look.pitch, moved: false }; el.setPointerCapture(e.pointerId); });
  el.addEventListener('pointermove', (e) => {
    raycaster.setFromCamera(toNdc(e), camera);
    mousePointer.ray.copy(raycaster.ray);
    if (!down) return;
    const dx = e.clientX - down.x, dy = e.clientY - down.y;
    if (Math.hypot(dx, dy) > 4) down.moved = true;
    if (down.moved && state.headingSource !== 'compass') {
      look.yaw = down.yaw + dx * 0.004;
      look.pitch = THREE.MathUtils.clamp(down.pitch + dy * 0.004, -1.45, 1.45);
    }
  });
  el.addEventListener('pointerup', (e) => {
    if (down && !down.moved) {
      raycaster.setFromCamera(toNdc(e), camera);
      mousePointer.ray.copy(raycaster.ray);
      castPointer(mousePointer);
      onSelect(mousePointer);
    }
    down = null;
  });
  el.addEventListener('wheel', (e) => {
    camera.fov = THREE.MathUtils.clamp(camera.fov + e.deltaY * 0.03, 35, 90);
    camera.updateProjectionMatrix();
  }, { passive: true });
}

// Phones: use the real compass, like Google's Qibla Finder on mobile.
const compass = { q: new THREE.Quaternion(), active: false, iosOffset: null };
function onDeviceOrientation(e) {
  if (e.alpha === null) return;
  const alpha = e.alpha * DEG, beta = e.beta * DEG, gamma = e.gamma * DEG;
  const orient = (screen.orientation?.angle ?? window.orientation ?? 0) * DEG;
  const euler = new THREE.Euler(beta, alpha, -gamma, 'YXZ');
  compass.q.setFromEuler(euler);
  compass.q.multiply(new THREE.Quaternion(-Math.sqrt(0.5), 0, 0, Math.sqrt(0.5)));
  compass.q.multiply(new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 0, 1), -orient));
  let north = 0;
  if (typeof e.webkitCompassHeading === 'number') {
    // iOS: alpha is relative; webkitCompassHeading is clockwise from north.
    north = alpha + e.webkitCompassHeading * DEG;
  } else if (!e.absolute) {
    return; // relative-only sensor can't find north
  }
  compass.active = true;
  if (Math.abs(angleDelta(north / DEG, state.northYaw / DEG)) > 0.5) applyNorth(north);
  state.calibrated = true;
}

async function enableCompass() {
  try {
    if (typeof DeviceOrientationEvent !== 'undefined' && DeviceOrientationEvent.requestPermission) {
      if ((await DeviceOrientationEvent.requestPermission()) !== 'granted') return;
    }
    state.headingSource = 'compass';
    window.addEventListener('deviceorientationabsolute', onDeviceOrientation);
    if (!('ondeviceorientationabsolute' in window)) window.addEventListener('deviceorientation', onDeviceOrientation);
    dom.compass.textContent = 'Compass on';
    dom.compass.disabled = true;
  } catch {
    toast('Compass isn’t available on this device.');
  }
}

// ------------------------------------------------------------------ XR flow

let needsSummon = true;

async function startSession(mode) {
  if (!navigator.xr || renderer.xr.isPresenting) return;
  unlockAudio();
  const init = { optionalFeatures: ['local-floor', 'bounded-floor', 'hand-tracking', 'anchors'] };
  try {
    const session = await navigator.xr.requestSession(mode, init);
    state.ar = mode === 'immersive-ar';
    await renderer.xr.setSession(session);
    onSessionStart(session);
  } catch (err) {
    toast(`Couldn’t start ${mode === 'immersive-ar' ? 'mixed reality' : 'VR'}: ${err.message}`);
  }
}

function onSessionStart(session) {
  document.body.classList.add('in-xr');
  state.headingSource = 'xr';
  state.calibrated = false;
  northAnchor = null;
  needsSummon = true;
  scene.background = state.ar ? null : SPACE;
  stars.visible = sun.visible = !state.ar;
  state.view = state.ar ? 'table' : 'orbit';
  setView(state.view);
  restoreNorthAnchor(session);
  if (!state.location) { setMode('pick'); locate({ quiet: true }); }
  session.addEventListener('end', onSessionEnd);
}

function onSessionEnd() {
  document.body.classList.remove('in-xr');
  state.headingSource = compass.active ? 'compass' : 'virtual';
  state.ar = false;
  scene.background = SPACE;
  stars.visible = sun.visible = true;
  northAnchor = null;
  if (!compass.active) applyNorth(0);
  state.calibrated = compass.active;
  setMode('normal');
  setView('orbit');
  hud.yaw = null;
  placeCardForDesktop();
}

// Installed as an immersive PWA on Quest, the OS grants a session at launch.
navigator.xr?.addEventListener?.('sessiongranted', () => startSession('immersive-vr'));

// ---------------------------------------------------------------- per frame

const timer = new THREE.Timer();
timer.connect?.(document);
const tmpV = new THREE.Vector3();
let lastTickBucket = null;

function update(time, dt, frame) {
  const xr = renderer.xr.isPresenting;
  const cam = xr ? renderer.xr.getCamera() : camera;

  if (!xr) {
    if (state.headingSource === 'compass' && compass.active) camera.quaternion.copy(compass.q);
    else camera.quaternion.setFromEuler(new THREE.Euler(look.pitch, look.yaw, 0, 'YXZ'));
    camera.updateMatrixWorld();
  }
  cam.getWorldPosition(head.pos);
  cam.getWorldQuaternion(head.quat);

  if (frame) {
    if (pendingNorthYaw !== null) { createNorthAnchor(frame, pendingNorthYaw); pendingNorthYaw = null; }
    trackNorthAnchor(frame);
  }

  if (xr && needsSummon && head.pos.lengthSq() > 0.01) {
    needsSummon = false;
    hud.yaw = null;
    card.summon(head.pos, head.quat);
    applyLayout(true);
  }

  // Heading and alignment.
  const yaw = headYaw();
  const heading = ((state.northYaw - yaw) / DEG + 360) % 360;
  const delta = Math.round(angleDelta(heading, state.bearing));
  const known = !!state.location && (state.calibrated || state.headingSource === 'virtual');
  const wasAligned = state.aligned;
  state.aligned = known && Math.abs(delta) <= (wasAligned ? 8 : 5);
  if (state.aligned && !wasAligned) {
    if (state.sound) chime();
    for (const p of pointers) pulse(p.inputSource, 0.6, 90);
  }
  // A light haptic detent every 15° while turning.
  const bucket = Math.round(delta / 15);
  if (xr && known && lastTickBucket !== null && bucket !== lastTickBucket) for (const p of pointers) pulse(p.inputSource, 0.12, 8);
  lastTickBucket = bucket;
  state.alignAmount = THREE.MathUtils.lerp(state.alignAmount, state.aligned ? 1 : 0, 1 - Math.exp(-dt * 4));

  // World.
  globe.update(time, dt);
  globe.setAligned(state.alignAmount);
  ring.update(time, state.alignAmount);
  ring.group.position.set(head.pos.x, 0, head.pos.z);
  if (!xr) ring.group.position.set(0, 0, 0);
  stars.material.uniforms.time.value = time;
  stars.position.copy(head.pos);
  sun.position.copy(head.pos).addScaledVector(tmpV.copy(globe.sunWorld).normalize(), 400);

  const qYaw = state.northYaw - state.bearing * DEG;
  beacon.position.set(-Math.sin(qYaw), 0, -Math.cos(qYaw)).multiplyScalar(80).add(head.pos);
  beacon.position.y = head.pos.y;
  beacon.visible = !!state.location && state.distance > 1;
  beacon.material.opacity = 0.75 + 0.25 * Math.sin(time * 1.7) + 0.4 * state.alignAmount;
  beacon.scale.setScalar(4 + 2 * state.alignAmount);

  label.visible = beacon.visible;
  if (state.location) {
    label.show(formatDistance(state.distance));
    label.place(globe.makkahLabelAnchor(tmpV), head.pos);
  }

  // Pointers.
  if (xr) for (const p of pointers) { if (p.inputSource) castPointer(p); else p.cursor.visible = false; }
  else castPointer(mousePointer);

  // HUD.
  hud.follow(head.pos, head.quat, dt);
  hud.show(hudState(delta));

  // Card.
  if (!state.location) card.show({ empty: true });
  else {
    card.show({
      place: placeName(state.location),
      coords: formatCoords(state.location),
      bearing: state.bearing,
      point: compassPoint(state.bearing),
      distance: formatDistance(state.distance),
      prayer: prayerInfo(state.location, state.method, state.hanafi, state.location.tz),
      method: state.method + state.hanafi,
    });
  }
  card.update(dt);
  for (const p of pointers) p.line.visible = !!p.inputSource;
}

function hudState(delta) {
  if (state.mode === 'calibrate') {
    return { kind: 'info', icon: 'compass', iconColor: '#FF453A', title: 'Point toward North', sub: 'Aim with your controller or hand, then select' };
  }
  if (state.mode === 'pick' || !state.location) {
    return { kind: 'info', icon: 'globe', title: 'Where are you?', sub: 'Point at the globe and select your city' };
  }
  if (!state.calibrated && state.headingSource === 'xr') {
    return { kind: 'info', icon: 'compass', iconColor: C.gold, title: 'Show me North', sub: 'Headsets have no compass · tap Set North' };
  }
  if (state.aligned) {
    return { kind: 'aligned', sub: `Makkah · ${formatDistance(state.distance)}` };
  }
  return { kind: 'turn', delta };
}

renderer.setAnimationLoop((t, frame) => {
  timer.update(t);
  const dt = Math.min(timer.getDelta(), 0.1);
  update(timer.getElapsed(), dt, frame);
  renderer.render(scene, camera);
});

function placeCardForDesktop() {
  card.group.position.set(-0.62, 1.38, -0.95);
  card.group.lookAt(0, 1.5, 0);
}

window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});

// --------------------------------------------------------------------- DOM

const $ = (id) => document.getElementById(id);
const dom = {
  place: $('place'), summary: $('summary'), prayer: $('prayer'),
  vr: $('enter-vr'), mr: $('enter-mr'), compass: $('use-compass'), xrNote: $('xr-note'),
  locate: $('locate'), search: $('city'), cities: $('cities'), method: $('method'), hanafi: $('hanafi'),
  view: $('view'), toast: $('toast'), sheet: $('sheet'), collapse: $('collapse'),
};

function syncDom() {
  if (!state.location) {
    dom.place.textContent = 'Where are you?';
    dom.summary.textContent = 'Share your location or choose a city to find the Qibla.';
    dom.prayer.hidden = true;
  } else {
    dom.place.textContent = placeName(state.location);
    dom.summary.innerHTML = `Qibla <b>${Math.round(state.bearing)}° ${compassPoint(state.bearing)}</b> · Makkah is <b>${formatDistance(state.distance)}</b> away`;
    const p = prayerInfo(state.location, state.method, state.hanafi, state.location.tz);
    dom.prayer.hidden = false;
    dom.prayer.innerHTML = `<span>Next prayer</span><strong>${p.next.name}</strong><em>${formatTime(p.next.time, p.tz)} · ${formatIn(p.next.time - Date.now())}</em>`;
  }
  dom.view.textContent = state.view === 'orbit' ? 'Show globe' : 'Stand above home';
}

let toastTimer;
function toast(msg) {
  dom.toast.textContent = msg;
  dom.toast.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => dom.toast.classList.remove('show'), 4000);
}

for (const c of CITIES) {
  const o = document.createElement('option');
  o.value = c.name;
  dom.cities.appendChild(o);
}
dom.search.addEventListener('change', () => {
  const q = dom.search.value.trim().toLowerCase();
  const c = CITIES.find((x) => x.name.toLowerCase() === q) || CITIES.find((x) => x.name.toLowerCase().startsWith(q));
  const coords = q.match(/^(-?\d+(?:\.\d+)?)\s*[, ]\s*(-?\d+(?:\.\d+)?)$/);
  if (c) applyLocation(c);
  else if (coords) applyLocation({ lat: +coords[1], lon: +coords[2] });
  else { toast('Try a city name, or coordinates like 40.71, -74.00'); return; }
  dom.search.value = '';
  dom.search.blur();
});
dom.locate.addEventListener('click', () => locate());

for (const [k, v] of Object.entries(METHODS)) {
  const o = document.createElement('option');
  o.value = k; o.textContent = v;
  dom.method.appendChild(o);
}
dom.method.value = state.method;
dom.method.addEventListener('change', () => { state.method = dom.method.value; store.set('method', state.method); syncDom(); });
dom.hanafi.checked = state.hanafi;
dom.hanafi.addEventListener('change', () => { state.hanafi = dom.hanafi.checked; store.set('hanafi', state.hanafi); syncDom(); });
dom.view.addEventListener('click', () => setView(state.view === 'orbit' ? 'table' : 'orbit'));
dom.vr.addEventListener('click', () => startSession('immersive-vr'));
dom.mr.addEventListener('click', () => startSession('immersive-ar'));
dom.compass.addEventListener('click', enableCompass);
dom.collapse.addEventListener('click', () => dom.sheet.classList.toggle('collapsed'));

(async () => {
  const [vr, ar] = await Promise.all([
    navigator.xr?.isSessionSupported('immersive-vr').catch(() => false),
    navigator.xr?.isSessionSupported('immersive-ar').catch(() => false),
  ]);
  dom.vr.hidden = !vr;
  dom.mr.hidden = !ar;
  const touch = matchMedia('(pointer: coarse)').matches && 'DeviceOrientationEvent' in window;
  dom.compass.hidden = !touch || vr;
  dom.xrNote.hidden = vr || ar;
})();

setInterval(syncDom, 30000);

// ------------------------------------------------------------------ start

await document.fonts.ready.catch(() => {});
await Promise.all(['500', '600', '650', '700'].map((w) => document.fonts.load(`${w} 32px "Inter Variable"`).catch(() => {})));

if (state.location) applyLocation(state.location, { persist: false });
else locate({ quiet: true });
applyNorth(0);
setView('orbit');
applyLayout(true);
placeCardForDesktop();
syncDom();
Object.values(buttons).forEach((b) => b.draw());

if ('serviceWorker' in navigator && import.meta.env.PROD) {
  navigator.serviceWorker.register(`${import.meta.env.BASE_URL}sw.js`).catch(() => {});
}

if (import.meta.env.DEV) window.__qibla = { state, buttons, card, globe, head, THREE };

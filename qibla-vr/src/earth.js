// The living Earth: procedurally painted globe with a real-time day/night
// terminator, drifting clouds, atmosphere, and the great-circle path to Makkah.

import * as THREE from 'three';
import { feature, mesh } from 'topojson-client';
import countries from 'world-atlas/countries-50m.json';
import { KAABA, subsolarPoint } from './geo.js';

const DEG = Math.PI / 180;

/** Unit vector for a lat/lon, matching THREE.SphereGeometry's UV layout. */
export function latLonToVec(lat, lon, out = new THREE.Vector3()) {
  const φ = lat * DEG, λ = lon * DEG;
  return out.set(Math.cos(φ) * Math.cos(λ), Math.sin(φ), -Math.cos(φ) * Math.sin(λ));
}

export function vecToLatLon(v) {
  const n = v.clone().normalize();
  return { lat: Math.asin(THREE.MathUtils.clamp(n.y, -1, 1)) / DEG, lon: Math.atan2(-n.z, n.x) / DEG };
}

// ---------------------------------------------------------------- textures

function projectRings(ctx, geom, W, H) {
  const polys = geom.type === 'Polygon' ? [geom.coordinates] : geom.type === 'MultiPolygon' ? geom.coordinates : [];
  for (const poly of polys) {
    for (const ring of poly) {
      ring.forEach(([lon, lat], i) => {
        const x = ((lon + 180) / 360) * W, y = ((90 - lat) / 180) * H;
        i ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
      });
      ctx.closePath();
    }
  }
}

function strokeLines(ctx, geom, W, H) {
  const lines = geom.type === 'MultiLineString' ? geom.coordinates : [geom.coordinates];
  for (const line of lines) {
    let prevLon = null;
    line.forEach(([lon, lat], i) => {
      const x = ((lon + 180) / 360) * W, y = ((90 - lat) / 180) * H;
      // Don't draw a segment across the antimeridian.
      if (i === 0 || Math.abs(lon - prevLon) > 180) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      prevLon = lon;
    });
  }
}

// Small seeded 3D value noise, sampled on the sphere so textures wrap seamlessly.
function makeNoise(seed = 1) {
  const perm = new Uint8Array(512);
  let s = seed;
  const rand = () => ((s = (s * 16807) % 2147483647) / 2147483647);
  const p = Array.from({ length: 256 }, (_, i) => i);
  for (let i = 255; i > 0; i--) { const j = Math.floor(rand() * (i + 1)); [p[i], p[j]] = [p[j], p[i]]; }
  for (let i = 0; i < 512; i++) perm[i] = p[i & 255];
  const vals = Float32Array.from({ length: 256 }, rand);
  const h = (x, y, z) => vals[perm[perm[perm[x & 255] + (y & 255)] + (z & 255)]];
  const sm = (t) => t * t * (3 - 2 * t);
  const lerp = (a, b, t) => a + (b - a) * t;
  const noise = (x, y, z) => {
    const xi = Math.floor(x), yi = Math.floor(y), zi = Math.floor(z);
    const xf = sm(x - xi), yf = sm(y - yi), zf = sm(z - zi);
    return lerp(
      lerp(lerp(h(xi, yi, zi), h(xi + 1, yi, zi), xf), lerp(h(xi, yi + 1, zi), h(xi + 1, yi + 1, zi), xf), yf),
      lerp(lerp(h(xi, yi, zi + 1), h(xi + 1, yi, zi + 1), xf), lerp(h(xi, yi + 1, zi + 1), h(xi + 1, yi + 1, zi + 1), xf), yf),
      zf);
  };
  return (x, y, z, octaves = 5) => {
    let a = 0.5, f = 1, sum = 0, norm = 0;
    for (let o = 0; o < octaves; o++) { sum += a * noise(x * f, y * f, z * f); norm += a; a *= 0.5; f *= 2.03; }
    return sum / norm;
  };
}

function fillSphereNoise(W, H, scale, seed, fn) {
  const fbm = makeNoise(seed);
  const img = new ImageData(W, H);
  for (let y = 0; y < H; y++) {
    const lat = 90 - ((y + 0.5) / H) * 180;
    for (let x = 0; x < W; x++) {
      const lon = ((x + 0.5) / W) * 360 - 180;
      const φ = lat * DEG, λ = lon * DEG;
      const n = fbm(Math.cos(φ) * Math.cos(λ) * scale + 10, Math.sin(φ) * scale + 10, Math.cos(φ) * Math.sin(λ) * scale + 10);
      fn(img.data, (y * W + x) * 4, n, lat);
    }
  }
  return img;
}

function canvas(W, H) {
  const c = document.createElement('canvas');
  c.width = W; c.height = H;
  return c;
}

/** Stylized day map, and a "night" map holding a land dot-matrix (R) and ocean mask (G). */
export function buildEarthTextures() {
  const land = feature(countries, countries.objects.countries);
  const borders = mesh(countries, countries.objects.countries, (a, b) => a !== b);

  const W = 4096, H = 2048;
  const day = canvas(W, H);
  const ctx = day.getContext('2d');

  // Ocean: deep navy at the poles, richer blue toward the tropics.
  const ocean = ctx.createLinearGradient(0, 0, 0, H);
  [[0, '#0b1d33'], [0.25, '#0d2f52'], [0.5, '#0f4474'], [0.75, '#0d2f52'], [1, '#0b1d33']]
    .forEach(([o, c]) => ocean.addColorStop(o, c));
  ctx.fillStyle = ocean;
  ctx.fillRect(0, 0, W, H);

  // Land colored by climate band, broken up with noise, then masked to coastlines.
  const landC = canvas(W, H);
  const lctx = landC.getContext('2d');
  const bands = lctx.createLinearGradient(0, 0, 0, H);
  const band = (lat, color) => bands.addColorStop((90 - lat) / 180, color);
  band(90, '#f1f5f8'); band(70, '#dfe6ea'); band(64, '#8c9b86'); band(55, '#4d6e45');
  band(42, '#6a8a4c'); band(33, '#b99e67'); band(22, '#d2b27a'); band(14, '#7f8f4e');
  band(6, '#2f6431'); band(-6, '#2d6230'); band(-16, '#6f8a48'); band(-26, '#c4a473');
  band(-36, '#7c8d55'); band(-50, '#5e7550'); band(-60, '#c9d3d8'); band(-66, '#eef3f6'); band(-90, '#f7fafc');
  lctx.fillStyle = bands;
  lctx.fillRect(0, 0, W, H);

  const tex = fillSphereNoise(1024, 512, 6, 7, (d, i, n) => {
    const v = Math.round(THREE.MathUtils.clamp((n - 0.5) * 2.2 + 0.5, 0, 1) * 255);
    d[i] = d[i + 1] = d[i + 2] = v; d[i + 3] = 255;
  });
  const texC = canvas(1024, 512);
  texC.getContext('2d').putImageData(tex, 0, 0);
  lctx.globalCompositeOperation = 'soft-light';
  lctx.drawImage(texC, 0, 0, W, H);

  lctx.globalCompositeOperation = 'destination-in';
  lctx.beginPath();
  for (const f of land.features) projectRings(lctx, f.geometry, W, H);
  lctx.fillStyle = '#fff';
  lctx.fill('evenodd');
  lctx.globalCompositeOperation = 'source-over';

  ctx.drawImage(landC, 0, 0);

  // Coastlines and borders: hairlines, like a map rendered by Apple.
  ctx.lineJoin = 'round';
  ctx.beginPath();
  for (const f of land.features) projectRings(ctx, f.geometry, W, H);
  ctx.strokeStyle = 'rgba(210,235,255,0.35)';
  ctx.lineWidth = 2;
  ctx.stroke();
  ctx.beginPath();
  strokeLines(ctx, borders, W, H);
  ctx.strokeStyle = 'rgba(255,255,255,0.22)';
  ctx.lineWidth = 1.4;
  ctx.stroke();

  // Night map.
  const NW = 2048, NH = 1024;
  const night = canvas(NW, NH);
  const nctx = night.getContext('2d');
  nctx.fillStyle = '#00ff00';
  nctx.fillRect(0, 0, NW, NH);
  nctx.beginPath();
  for (const f of land.features) projectRings(nctx, f.geometry, NW, NH);
  nctx.fillStyle = '#000000';
  nctx.fill('evenodd');
  const mask = nctx.getImageData(0, 0, NW, NH).data;
  nctx.fillStyle = '#ff0000';
  const step = 9;
  for (let y = step / 2; y < NH; y += step) {
    const lat = 90 - (y / NH) * 180;
    if (lat < -60) continue; // no dots over Antarctica
    // Keep dots roughly evenly spaced on the sphere.
    const xStep = step / Math.max(0.25, Math.cos(lat * DEG));
    for (let x = xStep / 2; x < NW; x += xStep) {
      const i = (Math.floor(y) * NW + Math.floor(x)) * 4;
      if (mask[i + 1] < 128) {
        nctx.beginPath();
        nctx.arc(x, y, 1.9, 0, Math.PI * 2);
        nctx.fill();
      }
    }
  }

  const dayTex = new THREE.CanvasTexture(day);
  const nightTex = new THREE.CanvasTexture(night);
  for (const t of [dayTex, nightTex]) {
    t.anisotropy = 8;
    t.generateMipmaps = true;
    t.minFilter = THREE.LinearMipmapLinearFilter;
  }
  return { dayTex, nightTex };
}

function buildCloudTexture() {
  const W = 1024, H = 512;
  const img = fillSphereNoise(W, H, 3.2, 42, (d, i, n, lat) => {
    // Fewer clouds in the subtropical high-pressure belts, more at the ITCZ and mid-latitudes.
    const belt = 0.85 + 0.15 * Math.cos(lat * DEG * 4);
    const a = THREE.MathUtils.smoothstep(n * belt, 0.5, 0.78);
    d[i] = d[i + 1] = d[i + 2] = 255;
    d[i + 3] = Math.round(a * 235);
  });
  const c = canvas(W, H);
  c.getContext('2d').putImageData(img, 0, 0);
  const t = new THREE.CanvasTexture(c);
  t.wrapS = THREE.RepeatWrapping;
  return t;
}

// ----------------------------------------------------------------- shaders

const earthVert = /* glsl */`
  varying vec2 vUv;
  varying vec3 vObjN;
  varying vec3 vWorldN;
  varying vec3 vWorldPos;
  void main() {
    vUv = uv;
    vObjN = normal;
    vWorldN = normalize(mat3(modelMatrix) * normal);
    vec4 wp = modelMatrix * vec4(position, 1.0);
    vWorldPos = wp.xyz;
    gl_Position = projectionMatrix * viewMatrix * wp;
  }`;

const earthFrag = /* glsl */`
  uniform sampler2D dayMap;
  uniform sampler2D nightMap;
  uniform sampler2D cloudMap;
  uniform vec3 sunObj;
  uniform vec3 sunWorld;
  uniform float cloudShift;
  varying vec2 vUv;
  varying vec3 vObjN;
  varying vec3 vWorldN;
  varying vec3 vWorldPos;
  void main() {
    vec3 n = normalize(vObjN);
    float d = dot(n, sunObj);
    float dayMix = smoothstep(-0.10, 0.22, d);
    vec3 dayCol = texture2D(dayMap, vUv).rgb;
    vec4 nm = texture2D(nightMap, vUv);
    float ocean = nm.g;

    vec3 lit = dayCol * (0.18 + 0.95 * max(d, 0.0));

    // Cloud shadows on the ground.
    float cloud = texture2D(cloudMap, vUv + vec2(cloudShift - 0.004, 0.002)).a;
    lit *= 1.0 - cloud * 0.35;

    vec3 V = normalize(cameraPosition - vWorldPos);
    vec3 N = normalize(vWorldN);
    vec3 H = normalize(normalize(sunWorld) + V);
    float spec = pow(max(dot(N, H), 0.0), 70.0) * ocean * dayMix;
    lit += vec3(1.0, 0.93, 0.8) * spec * 0.55;

    // Night: a golden dot-matrix over land, like a city-lights map drawn by a designer.
    vec3 night = dayCol * 0.035 + vec3(1.0, 0.76, 0.42) * nm.r * 0.85;

    vec3 col = mix(night, lit, dayMix);

    // Warm band along the terminator (sunrise / sunset right now).
    col += vec3(1.0, 0.42, 0.12) * 0.10 * exp(-pow(d * 7.0, 2.0));

    // Atmospheric rim.
    float fres = pow(1.0 - max(dot(N, V), 0.0), 2.6);
    col += vec3(0.32, 0.62, 1.0) * fres * (0.12 + 0.7 * dayMix);

    gl_FragColor = vec4(col, 1.0);
  }`;

const cloudFrag = /* glsl */`
  uniform sampler2D cloudMap;
  uniform vec3 sunObj;
  uniform float cloudShift;
  varying vec2 vUv;
  varying vec3 vObjN;
  void main() {
    float a = texture2D(cloudMap, vUv + vec2(cloudShift, 0.0)).a;
    float d = dot(normalize(vObjN), sunObj);
    float light = smoothstep(-0.15, 0.3, d);
    vec3 col = mix(vec3(0.05, 0.07, 0.12), vec3(1.0), light);
    col = mix(col, vec3(1.0, 0.6, 0.35), 0.35 * exp(-pow(d * 6.0, 2.0)));
    gl_FragColor = vec4(col, a * (0.25 + 0.6 * light));
  }`;

const atmoFrag = /* glsl */`
  uniform vec3 sunObj;
  varying vec3 vObjN;
  varying vec3 vWorldN;
  varying vec3 vWorldPos;
  void main() {
    vec3 V = normalize(cameraPosition - vWorldPos);
    float rim = pow(clamp(0.78 + dot(normalize(vWorldN), V), 0.0, 1.0), 4.0);
    float day = smoothstep(-0.35, 0.4, dot(normalize(vObjN), sunObj));
    vec3 col = mix(vec3(0.15, 0.25, 0.6), vec3(0.35, 0.65, 1.0), day);
    gl_FragColor = vec4(col * rim * (0.25 + 0.9 * day), 1.0);
  }`;

const arcVert = /* glsl */`
  varying vec2 vUv;
  void main() { vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0); }`;

const arcFrag = /* glsl */`
  uniform float time;
  uniform float boost;
  varying vec2 vUv;
  void main() {
    float t = vUv.x;
    vec3 col = mix(vec3(0.04, 0.52, 1.0), vec3(1.0, 0.8, 0.3), smoothstep(0.1, 0.95, t));
    // Pulses of light travelling toward Makkah.
    float pulse = pow(fract(t * 5.0 - time * 0.35), 10.0);
    float edge = smoothstep(0.0, 0.03, t) * smoothstep(1.0, 0.97, t);
    float a = (0.45 + 1.6 * pulse) * edge * (0.8 + boost);
    gl_FragColor = vec4(col * a, 1.0);
  }`;

const beamFrag = /* glsl */`
  uniform float time;
  uniform float boost;
  varying vec2 vUv;
  void main() {
    float h = vUv.y;
    float fade = pow(1.0 - h, 1.6);
    float shimmer = 0.85 + 0.15 * sin(time * 2.0 - h * 18.0);
    vec3 col = vec3(1.0, 0.82, 0.38) * fade * shimmer * (0.9 + boost);
    gl_FragColor = vec4(col, 1.0);
  }`;

function glowTexture(inner = 'rgba(255,220,140,1)', outer = 'rgba(255,190,80,0)') {
  const c = canvas(128, 128);
  const g = c.getContext('2d');
  const grad = g.createRadialGradient(64, 64, 0, 64, 64, 64);
  grad.addColorStop(0, inner);
  grad.addColorStop(0.25, inner.replace(/[\d.]+\)$/, '0.45)'));
  grad.addColorStop(1, outer);
  g.fillStyle = grad;
  g.fillRect(0, 0, 128, 128);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

function locationDotTexture() {
  const c = canvas(256, 256);
  const g = c.getContext('2d');
  g.fillStyle = 'rgba(10,132,255,0.22)';
  g.beginPath(); g.arc(128, 128, 120, 0, Math.PI * 2); g.fill();
  g.shadowColor = 'rgba(0,0,0,0.5)'; g.shadowBlur = 12;
  g.fillStyle = '#fff';
  g.beginPath(); g.arc(128, 128, 54, 0, Math.PI * 2); g.fill();
  g.shadowBlur = 0;
  g.fillStyle = '#0A84FF';
  g.beginPath(); g.arc(128, 128, 40, 0, Math.PI * 2); g.fill();
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  return t;
}

// ------------------------------------------------------------------- Globe

export class Globe {
  constructor() {
    this.group = new THREE.Group();          // positioned + scaled in the world
    this.oriented = new THREE.Group();       // rotated so "you" are on top, north aligned
    this.group.add(this.oriented);

    const { dayTex, nightTex } = buildEarthTextures();
    this.cloudTex = buildCloudTexture();
    this.sunObj = new THREE.Vector3(1, 0, 0);
    this.sunWorld = new THREE.Vector3(1, 0, 0);

    const shared = {
      sunObj: { value: this.sunObj },
      sunWorld: { value: this.sunWorld },
      cloudShift: { value: 0 },
      cloudMap: { value: this.cloudTex },
    };
    this.uniforms = shared;

    this.earth = new THREE.Mesh(
      new THREE.SphereGeometry(1, 160, 96),
      new THREE.ShaderMaterial({
        uniforms: { ...shared, dayMap: { value: dayTex }, nightMap: { value: nightTex } },
        vertexShader: earthVert,
        fragmentShader: earthFrag,
      }),
    );
    this.oriented.add(this.earth);

    this.clouds = new THREE.Mesh(
      new THREE.SphereGeometry(1.008, 128, 72),
      new THREE.ShaderMaterial({
        uniforms: shared,
        vertexShader: earthVert,
        fragmentShader: cloudFrag,
        transparent: true,
        depthWrite: false,
      }),
    );
    this.oriented.add(this.clouds);

    this.atmosphere = new THREE.Mesh(
      new THREE.SphereGeometry(1.07, 96, 64),
      new THREE.ShaderMaterial({
        uniforms: shared,
        vertexShader: earthVert,
        fragmentShader: atmoFrag,
        side: THREE.BackSide,
        blending: THREE.AdditiveBlending,
        transparent: true,
        depthWrite: false,
      }),
    );
    this.oriented.add(this.atmosphere);

    this.fxUniforms = { time: { value: 0 }, boost: { value: 0 } };
    this.arcMat = new THREE.ShaderMaterial({
      uniforms: this.fxUniforms, vertexShader: arcVert, fragmentShader: arcFrag,
      blending: THREE.AdditiveBlending, transparent: true, depthWrite: false,
    });
    this.arc = null;

    // Makkah: a pillar of light rising from the Kaaba.
    this.makkah = new THREE.Group();
    const beamGeo = new THREE.CylinderGeometry(0.004, 0.012, 1, 24, 1, true);
    beamGeo.translate(0, 0.5, 0);
    this.beam = new THREE.Mesh(beamGeo, new THREE.ShaderMaterial({
      uniforms: this.fxUniforms, vertexShader: arcVert, fragmentShader: beamFrag,
      blending: THREE.AdditiveBlending, transparent: true, depthWrite: false, side: THREE.DoubleSide,
    }));
    this.beam.scale.set(1, 0.45, 1);
    this.makkah.add(this.beam);
    this.makkahGlow = new THREE.Sprite(new THREE.SpriteMaterial({
      map: glowTexture(), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true,
    }));
    this.makkahGlow.scale.setScalar(0.07);
    this.makkahGlow.position.y = 0.004;
    this.makkah.add(this.makkahGlow);
    const kaaba = new THREE.Mesh(
      new THREE.BoxGeometry(0.006, 0.007, 0.006),
      new THREE.MeshBasicMaterial({ color: 0x0b0b0b }),
    );
    kaaba.position.y = 0.0035;
    this.makkah.add(kaaba);
    const band = new THREE.Mesh(
      new THREE.BoxGeometry(0.0062, 0.0012, 0.0062),
      new THREE.MeshBasicMaterial({ color: 0xf5c451 }),
    );
    band.position.y = 0.0052;
    this.makkah.add(band);
    this.oriented.add(this.makkah);
    this.makkahTop = new THREE.Object3D();
    this.makkah.add(this.makkahTop);
    const kaabaUp = latLonToVec(KAABA.lat, KAABA.lon);
    this.makkah.position.copy(kaabaUp);
    this.makkah.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), kaabaUp);

    // You: an iOS-style location dot with a breathing halo.
    this.you = new THREE.Group();
    const dotTex = locationDotTexture();
    this.youDot = new THREE.Mesh(
      new THREE.PlaneGeometry(0.03, 0.03),
      new THREE.MeshBasicMaterial({ map: dotTex, transparent: true, depthWrite: false }),
    );
    this.youDot.rotation.x = -Math.PI / 2;
    this.youDot.position.y = 0.0015;
    this.you.add(this.youDot);
    this.youHalo = new THREE.Mesh(
      new THREE.RingGeometry(0.8, 1, 64),
      new THREE.MeshBasicMaterial({ color: 0x0a84ff, transparent: true, depthWrite: false, side: THREE.DoubleSide }),
    );
    this.youHalo.rotation.x = -Math.PI / 2;
    this.youHalo.position.y = 0.001;
    this.you.add(this.youHalo);
    this.oriented.add(this.you);

    this.location = null;
    this.northYaw = 0;
    this.target = { position: new THREE.Vector3(), scale: 1 };
    this.dotScale = 1;
  }

  setLocation(loc) {
    this.location = loc;
    const up = latLonToVec(Math.max(-89.5, Math.min(89.5, loc.lat)), loc.lon);
    this.you.position.copy(up);
    this.you.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), up);
    this._buildArc(up);
    this._orient();
  }

  setNorthYaw(yaw) {
    this.northYaw = yaw;
    this._orient();
  }

  _orient() {
    if (!this.location) return;
    const lat = Math.max(-89.5, Math.min(89.5, this.location.lat));
    const up = latLonToVec(lat, this.location.lon);
    const east = new THREE.Vector3(0, 1, 0).cross(up).normalize();
    const north = up.clone().cross(east);
    const basis = new THREE.Matrix4().makeBasis(east, up, north.negate());
    const toLocal = new THREE.Quaternion().setFromRotationMatrix(basis).invert();
    const yaw = new THREE.Quaternion().setFromAxisAngle(new THREE.Vector3(0, 1, 0), this.northYaw);
    const first = !this.targetQuat;
    this.targetQuat = yaw.multiply(toLocal);
    if (first) this.oriented.quaternion.copy(this.targetQuat);
  }

  _buildArc(from) {
    const to = latLonToVec(KAABA.lat, KAABA.lon);
    const angle = from.angleTo(to);
    if (this.arc) { this.oriented.remove(this.arc); this.arc.geometry.dispose(); this.arc = null; }
    if (angle < 0.002) return; // you're in Makkah
    const lift = 0.02 + 0.22 * (angle / Math.PI);
    const pts = [];
    const N = 96;
    const q = new THREE.Quaternion();
    const axis = from.clone().cross(to).normalize();
    for (let i = 0; i <= N; i++) {
      const t = i / N;
      q.setFromAxisAngle(axis, angle * t);
      pts.push(from.clone().applyQuaternion(q).multiplyScalar(1.003 + lift * Math.sin(Math.PI * t)));
    }
    const curve = new THREE.CatmullRomCurve3(pts);
    this.arc = new THREE.Mesh(new THREE.TubeGeometry(curve, 160, 0.0035, 8, false), this.arcMat);
    this.oriented.add(this.arc);
  }

  /** Animate toward a layout: world position of the globe's center, and its radius in meters. */
  setLayout(position, radius, { instant = false, dotScale = 1, beam = 0.45 } = {}) {
    this.target.position.copy(position);
    this.target.scale = radius;
    this.target.dotScale = dotScale;
    this.target.beam = beam;
    if (instant) {
      this.group.position.copy(position);
      this.group.scale.setScalar(radius);
      this.dotScale = dotScale;
      this.beam.scale.y = beam;
    }
  }

  update(time, dt) {
    const k = 1 - Math.exp(-dt * 3.2);
    this.group.position.lerp(this.target.position, k);
    const s = THREE.MathUtils.lerp(this.group.scale.x, this.target.scale, k);
    this.group.scale.setScalar(s);
    this.dotScale = THREE.MathUtils.lerp(this.dotScale, this.target.dotScale ?? 1, k);
    this.beam.scale.y = THREE.MathUtils.lerp(this.beam.scale.y, this.target.beam ?? 0.45, k);
    // Glide (rather than snap) to a new location or a new north.
    if (this.targetQuat) this.oriented.quaternion.slerp(this.targetQuat, 1 - Math.exp(-dt * 2.5));

    // The sun, right now.
    const ss = subsolarPoint(new Date());
    latLonToVec(ss.lat, ss.lon, this.sunObj);
    this.oriented.updateMatrixWorld();
    this.sunWorld.copy(this.sunObj).applyQuaternion(this.oriented.getWorldQuaternion(new THREE.Quaternion()));

    this.uniforms.cloudShift.value = (time * 0.0006) % 1;
    this.fxUniforms.time.value = time;

    const breathe = (time * 0.6) % 1;
    this.youHalo.scale.setScalar(0.012 + 0.03 * breathe).multiplyScalar(this.dotScale);
    this.youHalo.material.opacity = 0.6 * (1 - breathe);
    this.youDot.scale.setScalar(this.dotScale);
    this.makkahGlow.scale.setScalar(0.05 * (1 + 0.1 * Math.sin(time * 2)) * Math.sqrt(this.dotScale));
  }

  setAligned(amount) {
    this.fxUniforms.boost.value = amount;
  }

  /** World position of the top of the Makkah light pillar. */
  makkahLabelAnchor(out = new THREE.Vector3()) {
    this.makkahTop.position.set(0, this.beam.scale.y * 0.55, 0);
    return this.makkahTop.getWorldPosition(out);
  }

  /** Intersect a ray with the globe; returns {lat, lon} or null. */
  pick(raycaster) {
    const hit = raycaster.intersectObject(this.earth, false)[0];
    if (!hit) return null;
    return vecToLatLon(this.oriented.worldToLocal(hit.point.clone()));
  }
}

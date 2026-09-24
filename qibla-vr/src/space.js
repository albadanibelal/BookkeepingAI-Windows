// Everything around you: stars, the sun, the glass platform you stand on,
// the compass ring at your feet, and the beacon on the horizon.

import * as THREE from 'three';
import { C, FONT, ICON } from './glass.js';

function canvas(W, H) {
  const c = document.createElement('canvas');
  c.width = W; c.height = H;
  return c;
}

export function createStars(count = 6000) {
  const pos = new Float32Array(count * 3);
  const size = new Float32Array(count);
  const phase = new Float32Array(count);
  const color = new Float32Array(count * 3);
  const v = new THREE.Vector3();
  let s = 12345;
  const rand = () => ((s = (s * 16807) % 2147483647) / 2147483647);
  for (let i = 0; i < count; i++) {
    // Denser toward a band, suggesting the Milky Way.
    v.set(rand() * 2 - 1, (rand() * 2 - 1) * (i % 3 === 0 ? 0.18 : 1), rand() * 2 - 1).normalize();
    v.applyAxisAngle(new THREE.Vector3(1, 0, 0.3).normalize(), 1.05);
    v.multiplyScalar(300).toArray(pos, i * 3);
    size[i] = Math.pow(rand(), 6) * 4 + 0.8;
    phase[i] = rand() * Math.PI * 2;
    const t = rand();
    const c = t < 0.15 ? [0.75, 0.85, 1] : t > 0.9 ? [1, 0.85, 0.7] : [1, 1, 1];
    color.set(c, i * 3);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('size', new THREE.BufferAttribute(size, 1));
  geo.setAttribute('phase', new THREE.BufferAttribute(phase, 1));
  geo.setAttribute('color', new THREE.BufferAttribute(color, 3));
  const mat = new THREE.ShaderMaterial({
    uniforms: { time: { value: 0 }, pixelRatio: { value: 1 } },
    vertexShader: /* glsl */`
      attribute float size; attribute float phase; attribute vec3 color;
      uniform float time; uniform float pixelRatio;
      varying float vA; varying vec3 vC;
      void main() {
        vC = color;
        vA = 0.55 + 0.45 * sin(time * (0.6 + fract(phase) * 1.8) + phase);
        vec4 mv = modelViewMatrix * vec4(position, 1.0);
        gl_PointSize = size * pixelRatio * 1.6;
        gl_Position = projectionMatrix * mv;
      }`,
    fragmentShader: /* glsl */`
      varying float vA; varying vec3 vC;
      void main() {
        float d = length(gl_PointCoord - 0.5);
        float a = smoothstep(0.5, 0.0, d);
        gl_FragColor = vec4(vC * a * vA, 1.0);
      }`,
    blending: THREE.AdditiveBlending,
    depthWrite: false,
    transparent: true,
  });
  const pts = new THREE.Points(geo, mat);
  pts.frustumCulled = false;
  pts.renderOrder = -10;
  return pts;
}

export function createSun() {
  const c = canvas(256, 256);
  const g = c.getContext('2d');
  const grad = g.createRadialGradient(128, 128, 0, 128, 128, 128);
  grad.addColorStop(0, 'rgba(255,255,255,1)');
  grad.addColorStop(0.08, 'rgba(255,250,235,1)');
  grad.addColorStop(0.2, 'rgba(255,220,160,0.45)');
  grad.addColorStop(0.5, 'rgba(255,170,90,0.10)');
  grad.addColorStop(1, 'rgba(255,150,60,0)');
  g.fillStyle = grad;
  g.fillRect(0, 0, 256, 256);
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  const sun = new THREE.Sprite(new THREE.SpriteMaterial({ map: t, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
  sun.scale.setScalar(60);
  sun.renderOrder = -9;
  return sun;
}

/** The frosted disc you stand on. */
export function createPlatform() {
  const group = new THREE.Group();
  const c = canvas(1024, 1024);
  const g = c.getContext('2d');
  const grad = g.createRadialGradient(512, 512, 0, 512, 512, 512);
  grad.addColorStop(0, 'rgba(255,255,255,0.10)');
  grad.addColorStop(0.9, 'rgba(255,255,255,0.05)');
  grad.addColorStop(0.97, 'rgba(255,255,255,0.35)');
  grad.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grad;
  g.beginPath(); g.arc(512, 512, 512, 0, Math.PI * 2); g.fill();
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  const disc = new THREE.Mesh(
    new THREE.CircleGeometry(0.75, 96),
    new THREE.MeshBasicMaterial({ map: t, transparent: true, depthWrite: false }),
  );
  disc.rotation.x = -Math.PI / 2;
  disc.position.y = 0.002;
  group.add(disc);
  return group;
}

/**
 * Compass ring lying on the floor around you. Rotates with north; carries a
 * golden Kaaba marker at the Qibla bearing and a glowing path toward it.
 */
export class CompassRing {
  constructor() {
    this.group = new THREE.Group();
    this.rotor = new THREE.Group(); // rotated so its "N" points at north
    this.group.add(this.rotor);

    this.canvas = canvas(2048, 2048);
    this.tex = new THREE.CanvasTexture(this.canvas);
    this.tex.colorSpace = THREE.SRGBColorSpace;
    this.tex.anisotropy = 8;
    const ring = new THREE.Mesh(
      new THREE.PlaneGeometry(2.6, 2.6),
      new THREE.MeshBasicMaterial({ map: this.tex, transparent: true, depthWrite: false }),
    );
    ring.rotation.x = -Math.PI / 2;
    ring.position.y = 0.004;
    this.rotor.add(ring);

    // Qibla path on the floor: a soft golden runway from your feet toward Makkah.
    const pc = canvas(64, 512);
    const pg = pc.getContext('2d');
    const grad = pg.createLinearGradient(0, 512, 0, 0);
    grad.addColorStop(0, 'rgba(255,214,120,0)');
    grad.addColorStop(0.25, 'rgba(255,214,120,0.55)');
    grad.addColorStop(1, 'rgba(255,214,120,0.0)');
    pg.fillStyle = grad;
    pg.fillRect(20, 0, 24, 512);
    const side = pg.createLinearGradient(0, 0, 64, 0);
    side.addColorStop(0, 'rgba(0,0,0,1)'); side.addColorStop(0.5, 'rgba(0,0,0,0)'); side.addColorStop(1, 'rgba(0,0,0,1)');
    pg.globalCompositeOperation = 'destination-out';
    pg.fillStyle = side; pg.fillRect(0, 0, 64, 512);
    const pt = new THREE.CanvasTexture(pc);
    pt.colorSpace = THREE.SRGBColorSpace;
    this.pathMat = new THREE.MeshBasicMaterial({ map: pt, transparent: true, depthWrite: false, blending: THREE.AdditiveBlending });
    this.path = new THREE.Mesh(new THREE.PlaneGeometry(0.22, 1.25), this.pathMat);
    this.path.rotation.x = -Math.PI / 2;
    this.path.position.set(0, 0.006, -0.62);
    this.qiblaPivot = new THREE.Group();
    this.qiblaPivot.add(this.path);
    this.group.add(this.qiblaPivot);

    this.bearing = 0;
    this.aligned = 0;
    this.draw();
  }

  draw() {
    const g = this.canvas.getContext('2d');
    const S = 2048, cx = S / 2, R = S * 0.46;
    g.clearRect(0, 0, S, S);
    // Glass track.
    g.lineWidth = 70;
    g.strokeStyle = 'rgba(255,255,255,0.06)';
    g.beginPath(); g.arc(cx, cx, R - 40, 0, Math.PI * 2); g.stroke();
    g.lineWidth = 2;
    g.strokeStyle = 'rgba(255,255,255,0.35)';
    g.beginPath(); g.arc(cx, cx, R, 0, Math.PI * 2); g.stroke();
    g.strokeStyle = 'rgba(255,255,255,0.12)';
    g.beginPath(); g.arc(cx, cx, R - 80, 0, Math.PI * 2); g.stroke();

    // Canvas y grows downward; in the ring's local frame "up" on the canvas is -Z (north).
    for (let d = 0; d < 360; d += 2) {
      const a = (d - 90) * Math.PI / 180;
      const major = d % 30 === 0;
      const len = major ? 42 : d % 10 === 0 ? 28 : 16;
      g.strokeStyle = major ? 'rgba(255,255,255,0.9)' : 'rgba(255,255,255,0.4)';
      g.lineWidth = major ? 5 : 3;
      g.beginPath();
      g.moveTo(cx + Math.cos(a) * (R - 6), cx + Math.sin(a) * (R - 6));
      g.lineTo(cx + Math.cos(a) * (R - 6 - len), cx + Math.sin(a) * (R - 6 - len));
      g.stroke();
    }
    const label = (str, deg, size, color, weight = 600) => {
      const a = (deg - 90) * Math.PI / 180;
      g.save();
      g.translate(cx + Math.cos(a) * (R - 120), cx + Math.sin(a) * (R - 120));
      g.rotate(a + Math.PI / 2);
      g.font = `${weight} ${size}px ${FONT}`;
      g.fillStyle = color;
      g.textAlign = 'center'; g.textBaseline = 'middle';
      g.fillText(str, 0, 0);
      g.restore();
    };
    label('N', 0, 84, '#FF453A', 700);
    label('E', 90, 64, C.label);
    label('S', 180, 64, C.label);
    label('W', 270, 64, C.label);
    for (const d of [30, 60, 120, 150, 210, 240, 300, 330]) label(String(d), d, 38, C.secondary, 500);

    // Kaaba marker at the qibla bearing.
    const a = (this.bearing - 90) * Math.PI / 180;
    const mx = cx + Math.cos(a) * (R + 0), my = cx + Math.sin(a) * (R + 0);
    const glow = g.createRadialGradient(mx, my, 0, mx, my, 150);
    glow.addColorStop(0, `rgba(255,214,120,${0.5 + 0.4 * this.aligned})`);
    glow.addColorStop(1, 'rgba(255,214,120,0)');
    g.fillStyle = glow;
    g.beginPath(); g.arc(mx, my, 150, 0, Math.PI * 2); g.fill();
    g.save();
    g.translate(mx, my);
    g.rotate(a + Math.PI / 2);
    ICON.kaaba(g, 0, 0, 96);
    g.restore();
    this.tex.needsUpdate = true;
  }

  set(bearing, northYaw) {
    this.rotor.rotation.y = northYaw;
    this.qiblaPivot.rotation.y = northYaw - bearing * Math.PI / 180;
    if (Math.abs(bearing - this.bearing) > 0.01) { this.bearing = bearing; this.draw(); }
  }

  update(time, aligned) {
    this.aligned = aligned;
    this.pathMat.opacity = 0.55 + 0.45 * aligned + 0.1 * Math.sin(time * 2.2);
  }
}

/** A far-away golden star sitting exactly on the Qibla direction at eye level. */
export function createHorizonBeacon() {
  const c = canvas(256, 256);
  const g = c.getContext('2d');
  const grad = g.createRadialGradient(128, 128, 0, 128, 128, 128);
  grad.addColorStop(0, 'rgba(255,248,225,1)');
  grad.addColorStop(0.12, 'rgba(255,214,120,0.9)');
  grad.addColorStop(0.4, 'rgba(255,190,80,0.18)');
  grad.addColorStop(1, 'rgba(255,170,60,0)');
  g.fillStyle = grad; g.fillRect(0, 0, 256, 256);
  // Four-point sparkle.
  g.globalCompositeOperation = 'lighter';
  const flare = g.createLinearGradient(0, 128, 256, 128);
  flare.addColorStop(0, 'rgba(255,220,150,0)'); flare.addColorStop(0.5, 'rgba(255,240,200,0.8)'); flare.addColorStop(1, 'rgba(255,220,150,0)');
  g.fillStyle = flare; g.fillRect(0, 126, 256, 4);
  g.save(); g.translate(128, 128); g.rotate(Math.PI / 2); g.translate(-128, -128); g.fillRect(0, 126, 256, 4); g.restore();
  const t = new THREE.CanvasTexture(c);
  t.colorSpace = THREE.SRGBColorSpace;
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: t, blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }));
  s.scale.setScalar(4);
  s.renderOrder = -5;
  return s;
}

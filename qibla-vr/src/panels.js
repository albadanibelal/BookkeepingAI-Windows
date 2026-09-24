// In-headset UI: a Dynamic Island–style guidance pill, the glass info card,
// buttons, and the floating Makkah label.

import * as THREE from 'three';
import { C, ICON, drawGlass, text, measure, roundRect } from './glass.js';
import { formatTime, formatIn } from './prayer.js';

const PX_PER_M = 2000;

class CanvasPlane {
  constructor(wPx, hPx, { pxPerM = PX_PER_M } = {}) {
    this.canvas = document.createElement('canvas');
    this.canvas.width = wPx;
    this.canvas.height = hPx;
    this.ctx = this.canvas.getContext('2d');
    this.tex = new THREE.CanvasTexture(this.canvas);
    this.tex.colorSpace = THREE.SRGBColorSpace;
    this.tex.anisotropy = 4;
    this.mesh = new THREE.Mesh(
      new THREE.PlaneGeometry(wPx / pxPerM, hPx / pxPerM),
      new THREE.MeshBasicMaterial({ map: this.tex, transparent: true, depthWrite: false }),
    );
    this.mesh.renderOrder = 10;
  }
  commit() { this.tex.needsUpdate = true; }
}

export class Button extends CanvasPlane {
  constructor(label, icon, onPress) {
    super(256, 112);
    this.label = label;
    this.icon = icon;
    this.onPress = onPress;
    this.hover = false;
    this.active = false;
    this.mesh.userData.button = this;
    this.mesh.renderOrder = 11;
    this.pressT = 0;
    this.draw();
  }
  set(label, { icon = this.icon, active = this.active } = {}) {
    if (label === this.label && icon === this.icon && active === this.active) return;
    this.label = label; this.icon = icon; this.active = active;
    this.draw();
  }
  setHover(h) {
    if (h !== this.hover) { this.hover = h; this.draw(); }
  }
  draw() {
    const g = this.ctx, W = this.canvas.width, H = this.canvas.height;
    g.clearRect(0, 0, W, H);
    const tint = this.active ? C.blue : null;
    drawGlass(g, 4, 4, W - 8, H - 8, (H - 8) / 2, { tint, alpha: this.hover ? 0.62 : 0.4 });
    if (this.hover) {
      roundRect(g, 4, 4, W - 8, H - 8, (H - 8) / 2);
      g.fillStyle = 'rgba(255,255,255,0.10)';
      g.fill();
    }
    const color = this.active ? '#9CCBFF' : C.label;
    const tw = measure(g, this.label, 34, 600);
    const total = 40 + 14 + tw;
    const x0 = (W - total) / 2;
    this.icon(g, x0 + 20, H / 2, 40, color);
    text(g, this.label, x0 + 54, H / 2 + 1, { size: 34, weight: 600, color, baseline: 'middle' });
    this.commit();
  }
  press() {
    this.pressT = 1;
    this.onPress?.();
  }
  update(dt) {
    this.pressT = Math.max(0, this.pressT - dt * 5);
    const s = 1 - 0.06 * Math.sin(this.pressT * Math.PI);
    this.mesh.scale.setScalar(s);
  }
}

/** The guidance pill that floats below your gaze. */
export class Hud extends CanvasPlane {
  constructor() {
    super(900, 220);
    this.key = '';
    this.mesh.renderOrder = 20;
    this.mesh.material.depthTest = false;
    this.yaw = null;
  }

  show(state) {
    const key = JSON.stringify(state);
    if (key === this.key) return;
    this.key = key;
    const g = this.ctx, W = this.canvas.width, H = this.canvas.height;
    g.clearRect(0, 0, W, H);

    const pillW = state.kind === 'aligned' ? 820 : 700;
    const x = (W - pillW) / 2, y = 20, h = 180;
    // Near-black island with a glass rim.
    roundRect(g, x, y, pillW, h, h / 2);
    g.fillStyle = 'rgba(8,8,10,0.86)';
    g.fill();
    drawGlass(g, x, y, pillW, h, h / 2, { alpha: 0.0, tint: state.kind === 'aligned' ? C.green : null });

    const cy = y + h / 2;
    if (state.kind === 'aligned') {
      g.fillStyle = C.green;
      g.beginPath(); g.arc(x + 92, cy, 50, 0, Math.PI * 2); g.fill();
      ICON.check(g, x + 92, cy + 2, 56, '#fff');
      text(g, 'Facing the Qibla', x + 170, cy - 8, { size: 52, weight: 650 });
      text(g, state.sub, x + 170, cy + 44, { size: 32, weight: 500, color: C.secondary });
    } else if (state.kind === 'turn') {
      const right = state.delta > 0;
      const ax = right ? x + pillW - 90 : x + 90;
      g.fillStyle = 'rgba(255,214,10,0.16)';
      g.beginPath(); g.arc(ax, cy, 50, 0, Math.PI * 2); g.fill();
      ICON.chevron(g, ax + (right ? 3 : -3), cy, 50, C.gold, right ? 1 : -1);
      const tx = right ? x + 70 : x + 170;
      text(g, `${Math.abs(state.delta)}°`, tx, cy + 26, { size: 84, weight: 700 });
      const nw = measure(g, `${Math.abs(state.delta)}°`, 84, 700);
      text(g, right ? 'Turn right' : 'Turn left', tx + nw + 26, cy - 8, { size: 40, weight: 600 });
      text(g, 'toward Makkah', tx + nw + 26, cy + 38, { size: 32, weight: 500, color: C.secondary });
    } else {
      ICON[state.icon || 'compass'](g, x + 92, cy, 64, state.iconColor || C.blue);
      text(g, state.title, x + 160, cy - 8, { size: 46, weight: 650 });
      text(g, state.sub, x + 160, cy + 42, { size: 30, weight: 500, color: C.secondary });
    }
    this.commit();
  }

  /** Follow the head lazily, like a visionOS window tethered to your gaze. */
  follow(camPos, camQuat, dt) {
    const e = new THREE.Euler().setFromQuaternion(camQuat, 'YXZ');
    if (this.yaw === null) this.yaw = e.y;
    const k = 1 - Math.exp(-dt * 4);
    let d = e.y - this.yaw;
    d = Math.atan2(Math.sin(d), Math.cos(d));
    this.yaw += d * k;
    const pitch = THREE.MathUtils.clamp(e.x, -0.9, 0.5) - 0.32;
    const dir = new THREE.Vector3(0, 0, -1).applyEuler(new THREE.Euler(pitch, this.yaw, 0, 'YXZ'));
    this.mesh.position.copy(camPos).addScaledVector(dir, 1.25);
    this.mesh.lookAt(camPos);
  }
}

/** The main glass card: where you are, how far Makkah is, and what prayer is next. */
export class InfoCard extends CanvasPlane {
  constructor() {
    super(1200, 980);
    this.group = new THREE.Group();
    this.group.add(this.mesh);
    this.buttons = [];
    this.key = '';
  }

  addButtons(buttons) {
    this.buttons = buttons;
    const W = this.canvas.width / PX_PER_M, H = this.canvas.height / PX_PER_M;
    const bw = 256 / PX_PER_M, gap = 0.008;
    const total = buttons.length * bw + (buttons.length - 1) * gap;
    buttons.forEach((b, i) => {
      b.mesh.position.set(-total / 2 + bw / 2 + i * (bw + gap), -H / 2 + 0.05, 0.004);
      this.group.add(b.mesh);
    });
    void W;
  }

  show(s) {
    if (s.empty) return this.showEmpty();
    const minuteKey = Math.floor(Date.now() / 30000);
    const key = JSON.stringify([s.place, s.coords, s.bearing, s.distance, s.method, minuteKey]);
    if (key === this.key) return;
    this.key = key;
    const g = this.ctx, W = this.canvas.width, H = this.canvas.height;
    g.clearRect(0, 0, W, H);
    drawGlass(g, 6, 6, W - 12, H - 12, 72);

    const P = 64;
    text(g, 'QIBLA', P, 104, { size: 28, weight: 650, color: C.secondary, tracking: 3 });
    text(g, formatTime(new Date(), s.prayer.tz), W - P, 104, { size: 28, weight: 600, color: C.secondary, align: 'right' });
    ICON.location(g, P + 18, 172, 34, C.blue);
    text(g, s.place, P + 52, 188, { size: 64, weight: 700 });
    text(g, s.coords, P, 244, { size: 30, weight: 500, color: C.secondary });

    // Stat tiles.
    const tile = (x, label, value, unit, color) => {
      roundRect(g, x, 284, 520, 190, 40);
      g.fillStyle = 'rgba(255,255,255,0.07)';
      g.fill();
      text(g, label, x + 36, 336, { size: 28, weight: 600, color: C.secondary });
      text(g, value, x + 36, 440, { size: 96, weight: 700, color });
      const vw = measure(g, value, 96, 700);
      text(g, unit, x + 36 + vw + 14, 440, { size: 40, weight: 600, color: C.secondary });
    };
    tile(P, 'Qibla direction', `${Math.round(s.bearing)}°`, s.point, C.kaaba);
    const [dv, du] = s.distance.split(' ');
    tile(P + 552, 'Distance to Makkah', dv, du, C.label);

    // Prayer widget.
    const p = s.prayer;
    const top = 506;
    roundRect(g, P, top, W - 2 * P, 300, 40);
    g.fillStyle = 'rgba(255,255,255,0.07)';
    g.fill();
    text(g, 'Next prayer', P + 36, top + 56, { size: 28, weight: 600, color: C.secondary });
    text(g, p.next.name, P + 36, top + 138, { size: 72, weight: 700 });
    const nw = measure(g, p.next.name, 72, 700);
    text(g, `${formatTime(p.next.time, p.tz)} · ${formatIn(p.next.time - Date.now())}`, P + 36 + nw + 24, top + 134, { size: 34, weight: 600, color: C.blue });

    const list = p.times.filter((t) => t.id !== 'sunrise');
    const colW = (W - 2 * P - 72) / list.length;
    list.forEach((t, i) => {
      const cx = P + 36 + colW * i + colW / 2;
      const isNext = t.id === p.next.id;
      if (isNext) {
        roundRect(g, cx - colW / 2 + 6, top + 172, colW - 12, 104, 26);
        g.fillStyle = 'rgba(10,132,255,0.28)';
        g.fill();
      }
      text(g, t.name, cx, top + 214, { size: 26, weight: 600, color: isNext ? C.label : C.secondary, align: 'center' });
      text(g, formatTime(t.time, p.tz), cx, top + 256, { size: 30, weight: 600, color: isNext ? C.label : 'rgba(235,235,245,0.85)', align: 'center' });
    });
    this.commit();
  }

  showEmpty() {
    if (this.key === 'empty') return;
    this.key = 'empty';
    const g = this.ctx, W = this.canvas.width, H = this.canvas.height;
    g.clearRect(0, 0, W, H);
    drawGlass(g, 6, 6, W - 12, H - 12, 72);
    const P = 64;
    text(g, 'QIBLA', P, 104, { size: 28, weight: 650, color: C.secondary, tracking: 3 });
    ICON.globe(g, W / 2, 330, 150, C.blue);
    text(g, 'Where are you?', W / 2, 520, { size: 72, weight: 700, align: 'center' });
    text(g, 'Point at the globe and select your city.', W / 2, 590, { size: 36, weight: 500, color: C.secondary, align: 'center' });
    text(g, 'Your location never leaves this headset.', W / 2, 640, { size: 30, weight: 500, color: C.tertiary, align: 'center' });
    this.commit();
  }

  /** Place the card in front of (and slightly left of) a head pose, facing it. */
  summon(camPos, camQuat, sideAngle = 0.5) {
    const e = new THREE.Euler().setFromQuaternion(camQuat, 'YXZ');
    const yaw = e.y + sideAngle;
    const dir = new THREE.Vector3(-Math.sin(yaw), 0, -Math.cos(yaw));
    this.group.position.copy(camPos).addScaledVector(dir, 0.85);
    this.group.position.y = camPos.y - 0.22;
    this.group.lookAt(camPos.x, camPos.y - 0.1, camPos.z);
  }

  update(dt) {
    for (const b of this.buttons) b.update(dt);
  }
}

/** A glass tag hovering over the pillar of light at Makkah. Constant angular size. */
export class MakkahLabel extends CanvasPlane {
  constructor() {
    super(640, 200, { pxPerM: 640 });
    this.key = '';
    this.mesh.material.depthTest = false;
    this.mesh.renderOrder = 15;
  }
  show(distance) {
    if (distance === this.key) return;
    this.key = distance;
    const g = this.ctx, W = this.canvas.width, H = this.canvas.height;
    g.clearRect(0, 0, W, H);
    drawGlass(g, 4, 4, W - 8, H - 8, (H - 8) / 2, { alpha: 0.55 });
    ICON.kaaba(g, 100, H / 2, 72);
    text(g, 'Makkah', 160, H / 2 - 6, { size: 60, weight: 700 });
    text(g, distance, 160, H / 2 + 50, { size: 38, weight: 500, color: C.secondary });
    this.commit();
  }
  place(anchor, camPos) {
    const d = anchor.distanceTo(camPos);
    this.mesh.position.copy(anchor);
    this.mesh.scale.setScalar(Math.max(0.08, d * 0.16));
    this.mesh.position.y += this.mesh.scale.y * 0.2;
    this.mesh.lookAt(camPos);
  }
}

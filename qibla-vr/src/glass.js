// Canvas drawing helpers for the in-headset UI: Apple-style "Liquid Glass"
// surfaces and system typography, rendered to textures.

export const FONT = '"SF Pro Display", "SF Pro Text", -apple-system, "Inter Variable", Inter, system-ui, sans-serif';

// iOS dark-mode system palette.
export const C = {
  label: '#FFFFFF',
  secondary: 'rgba(235,235,245,0.62)',
  tertiary: 'rgba(235,235,245,0.32)',
  separator: 'rgba(255,255,255,0.14)',
  blue: '#0A84FF',
  green: '#30D158',
  gold: '#FFD60A',
  kaaba: '#F5C451',
  orange: '#FF9F0A',
};

export function font(size, weight = 500) {
  return `${weight} ${size}px ${FONT}`;
}

export function roundRect(ctx, x, y, w, h, r) {
  r = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

/**
 * A dark Liquid Glass slab: translucent body, soft inner glow, and a bright
 * specular rim that is strongest top-left, as if lit from above.
 */
export function drawGlass(ctx, x, y, w, h, r, { tint = null, alpha = 0.42 } = {}) {
  ctx.save();
  roundRect(ctx, x, y, w, h, r);
  const body = ctx.createLinearGradient(0, y, 0, y + h);
  body.addColorStop(0, `rgba(58,58,66,${alpha})`);
  body.addColorStop(1, `rgba(28,28,34,${alpha + 0.12})`);
  ctx.fillStyle = body;
  ctx.fill();

  if (tint) {
    ctx.globalAlpha = 0.22;
    ctx.fillStyle = tint;
    ctx.fill();
    ctx.globalAlpha = 1;
  }

  // Top sheen.
  ctx.clip();
  const sheen = ctx.createLinearGradient(0, y, 0, y + h * 0.55);
  sheen.addColorStop(0, 'rgba(255,255,255,0.16)');
  sheen.addColorStop(1, 'rgba(255,255,255,0)');
  ctx.fillStyle = sheen;
  ctx.fillRect(x, y, w, h * 0.55);
  ctx.restore();

  // Specular rim.
  ctx.save();
  roundRect(ctx, x + 1.5, y + 1.5, w - 3, h - 3, r - 1.5);
  const rim = ctx.createLinearGradient(x, y, x + w, y + h);
  rim.addColorStop(0, 'rgba(255,255,255,0.75)');
  rim.addColorStop(0.35, 'rgba(255,255,255,0.12)');
  rim.addColorStop(0.7, 'rgba(255,255,255,0.06)');
  rim.addColorStop(1, 'rgba(255,255,255,0.35)');
  ctx.strokeStyle = rim;
  ctx.lineWidth = 3;
  ctx.stroke();
  ctx.restore();
}

export function text(ctx, str, x, y, { size = 32, weight = 500, color = C.label, align = 'left', baseline = 'alphabetic', tracking = null } = {}) {
  ctx.font = font(size, weight);
  ctx.fillStyle = color;
  ctx.textAlign = align;
  ctx.textBaseline = baseline;
  // SF-style tracking: tighter at display sizes, looser for small text.
  if ('letterSpacing' in ctx) ctx.letterSpacing = `${tracking ?? (size >= 40 ? -0.02 * size : 0)}px`;
  ctx.fillText(str, x, y);
  if ('letterSpacing' in ctx) ctx.letterSpacing = '0px';
}

export function measure(ctx, str, size, weight = 500) {
  ctx.font = font(size, weight);
  if ('letterSpacing' in ctx) ctx.letterSpacing = `${size >= 40 ? -0.02 * size : 0}px`;
  const w = ctx.measureText(str).width;
  if ('letterSpacing' in ctx) ctx.letterSpacing = '0px';
  return w;
}

// Minimal SF Symbols–style glyphs drawn as paths, centered at (x, y).
export const ICON = {
  location(ctx, x, y, s, color) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(s / 24, s / 24);
    ctx.beginPath();
    ctx.moveTo(-9, -1); ctx.lineTo(10, -9); ctx.lineTo(2, 10); ctx.lineTo(0, 1); ctx.closePath();
    ctx.fillStyle = color;
    ctx.fill();
    ctx.restore();
  },
  compass(ctx, x, y, s, color) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(s / 24, s / 24);
    ctx.strokeStyle = color; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(0, 0, 10, 0, Math.PI * 2); ctx.stroke();
    ctx.fillStyle = color;
    ctx.beginPath(); ctx.moveTo(0, -7); ctx.lineTo(3, 0); ctx.lineTo(-3, 0); ctx.closePath(); ctx.fill();
    ctx.globalAlpha = 0.45;
    ctx.beginPath(); ctx.moveTo(0, 7); ctx.lineTo(3, 0); ctx.lineTo(-3, 0); ctx.closePath(); ctx.fill();
    ctx.restore();
  },
  globe(ctx, x, y, s, color) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(s / 24, s / 24);
    ctx.strokeStyle = color; ctx.lineWidth = 1.8;
    ctx.beginPath(); ctx.arc(0, 0, 10, 0, Math.PI * 2); ctx.stroke();
    ctx.beginPath(); ctx.ellipse(0, 0, 4.5, 10, 0, 0, Math.PI * 2); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(-10, 0); ctx.lineTo(10, 0); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(-8.5, -5); ctx.lineTo(8.5, -5); ctx.moveTo(-8.5, 5); ctx.lineTo(8.5, 5); ctx.stroke();
    ctx.restore();
  },
  speaker(ctx, x, y, s, color, on = true) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(s / 24, s / 24);
    ctx.fillStyle = color; ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(-10, -4); ctx.lineTo(-5, -4); ctx.lineTo(1, -9); ctx.lineTo(1, 9); ctx.lineTo(-5, 4); ctx.lineTo(-10, 4);
    ctx.closePath(); ctx.fill();
    if (on) {
      ctx.beginPath(); ctx.arc(2, 0, 5, -0.8, 0.8); ctx.stroke();
      ctx.beginPath(); ctx.arc(2, 0, 9, -0.8, 0.8); ctx.stroke();
    } else {
      ctx.beginPath(); ctx.moveTo(5, -4); ctx.lineTo(11, 4); ctx.moveTo(11, -4); ctx.lineTo(5, 4); ctx.stroke();
    }
    ctx.restore();
  },
  check(ctx, x, y, s, color) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(s / 24, s / 24);
    ctx.strokeStyle = color; ctx.lineWidth = 3; ctx.lineCap = 'round'; ctx.lineJoin = 'round';
    ctx.beginPath(); ctx.moveTo(-8, 0); ctx.lineTo(-2, 6); ctx.lineTo(9, -7); ctx.stroke();
    ctx.restore();
  },
  chevron(ctx, x, y, s, color, dir = 1) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale((s / 24) * dir, s / 24);
    ctx.strokeStyle = color; ctx.lineWidth = 3.2; ctx.lineCap = 'round'; ctx.lineJoin = 'round';
    ctx.beginPath(); ctx.moveTo(-4, -9); ctx.lineTo(5, 0); ctx.lineTo(-4, 9); ctx.stroke();
    ctx.restore();
  },
  kaaba(ctx, x, y, s) {
    ctx.save();
    ctx.translate(x, y);
    ctx.scale(s / 24, s / 24);
    ctx.fillStyle = '#111';
    roundRect(ctx, -9, -9, 18, 18, 2.5); ctx.fill();
    ctx.fillStyle = C.kaaba;
    ctx.fillRect(-9, -4.5, 18, 2.6);
    ctx.strokeStyle = 'rgba(255,255,255,0.35)'; ctx.lineWidth = 1;
    roundRect(ctx, -9, -9, 18, 18, 2.5); ctx.stroke();
    ctx.restore();
  },
};

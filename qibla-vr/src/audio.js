// Soft, synthesized sounds so the app ships with no audio assets.

let ctx = null;

function ac() {
  if (!ctx) ctx = new (window.AudioContext || window.webkitAudioContext)();
  if (ctx.state === 'suspended') ctx.resume();
  return ctx;
}

/** A gentle two-partial bell, like an iOS notification heard from far away. */
export function chime() {
  const a = ac();
  const t = a.currentTime;
  const out = a.createGain();
  out.gain.value = 0.18;
  out.connect(a.destination);
  for (const [freq, gain, decay] of [[880, 0.6, 1.6], [1318.5, 0.35, 1.1], [1760, 0.12, 0.6]]) {
    const o = a.createOscillator();
    const g = a.createGain();
    o.type = 'sine';
    o.frequency.value = freq;
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(gain, t + 0.015);
    g.gain.exponentialRampToValueAtTime(0.0001, t + decay);
    o.connect(g).connect(out);
    o.start(t);
    o.stop(t + decay + 0.05);
  }
}

/** A short tick, like a detent on a dial. */
export function tick() {
  const a = ac();
  const t = a.currentTime;
  const o = a.createOscillator();
  const g = a.createGain();
  o.type = 'triangle';
  o.frequency.value = 2200;
  g.gain.setValueAtTime(0.05, t);
  g.gain.exponentialRampToValueAtTime(0.0001, t + 0.04);
  o.connect(g).connect(a.destination);
  o.start(t);
  o.stop(t + 0.05);
}

export function unlockAudio() {
  try { ac(); } catch { /* audio unavailable */ }
}

## Shader sources used by the environment (kept in code so the whole scene is procedural).
extends RefCounted

const SKY := """
shader_type sky;
uniform vec3 sun_dir = vec3(0.35, 0.05, -1.0);

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
	float v = 0.0; float a = 0.5;
	for (int i = 0; i < 5; i++) { v += a * noise(p); p *= 2.03; a *= 0.5; }
	return v;
}

void sky() {
	vec3 d = normalize(EYEDIR);
	vec3 s = normalize(sun_dir);
	float h = d.y;
	float sd = max(dot(d, s), 0.0);
	vec3 zenith = vec3(0.10, 0.12, 0.30);
	vec3 upper = vec3(0.36, 0.30, 0.55);
	vec3 mid = vec3(0.86, 0.46, 0.52);
	vec3 horizon = vec3(1.0, 0.62, 0.34);
	vec3 col = mix(horizon, mid, smoothstep(0.0, 0.12, h));
	col = mix(col, upper, smoothstep(0.1, 0.35, h));
	col = mix(col, zenith, smoothstep(0.3, 0.85, h));
	col += vec3(1.0, 0.55, 0.25) * pow(sd, 6.0) * 0.55;
	col += vec3(1.0, 0.85, 0.55) * pow(sd, 90.0) * 1.2;
	col += vec3(1.0, 0.95, 0.8) * smoothstep(0.9993, 0.9997, sd) * 4.0;
	if (h > 0.0) {
		vec2 uv = d.xz / (h + 0.12) * 0.9;
		float n = fbm(uv * 1.3 + vec2(TIME * 0.004, 0.0));
		float n2 = fbm(uv * 3.1 - vec2(TIME * 0.006, 0.0));
		float band = smoothstep(0.015, 0.12, h) * (1.0 - smoothstep(0.35, 0.75, h));
		float c = smoothstep(0.48, 0.78, n * 0.75 + n2 * 0.35) * band;
		vec3 lit = mix(vec3(0.95, 0.52, 0.58), vec3(1.0, 0.78, 0.52), pow(sd, 2.0));
		vec3 shade = vec3(0.45, 0.32, 0.5);
		vec3 cc = mix(shade, lit, smoothstep(0.3, 0.9, n2));
		col = mix(col, cc, c * 0.85);
	} else {
		col = mix(horizon * 0.7, vec3(0.12, 0.1, 0.2), smoothstep(0.0, -0.2, h));
	}
	COLOR = col;
}
"""

const WATER := """
shader_type spatial;
render_mode unshaded, cull_disabled;
uniform vec3 sun_dir = vec3(0.35, 0.05, -1.0);
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
varying vec3 wpos;
void vertex() { wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz; }
void fragment() {
	vec3 view = normalize(wpos - CAMERA_POSITION_WORLD);
	float dist = length(wpos.xz - CAMERA_POSITION_WORLD.xz);
	float ripple = noise(wpos.xz * vec2(0.08, 0.5) + vec2(0.0, TIME * 0.2)) * 0.6 + noise(wpos.xz * vec2(0.3, 1.6) - vec2(TIME * 0.1, 0.0)) * 0.4;
	vec3 deep = vec3(0.16, 0.14, 0.3);
	vec3 refl = mix(vec3(0.95, 0.55, 0.45), vec3(0.4, 0.3, 0.55), smoothstep(40.0, 200.0, dist) * 0.3 + ripple * 0.35);
	float fres = pow(1.0 - abs(view.y), 3.0);
	vec3 col = mix(deep, refl, 0.35 + fres * 0.55);
	vec3 s = normalize(vec3(sun_dir.x, 0.0, sun_dir.z));
	vec2 toward = normalize(view.xz);
	float glint = pow(max(dot(toward, s.xz), 0.0), 180.0) * smoothstep(0.55, 0.9, ripple);
	col += vec3(1.0, 0.75, 0.45) * glint * 1.6;
	ALBEDO = col;
}
"""

const BUILDING := """
shader_type spatial;
render_mode unshaded;
uniform vec3 base_color : source_color = vec3(0.2, 0.18, 0.32);
uniform float seed = 1.0;
varying vec3 wpos;
varying vec3 wnorm;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + seed) * 43758.5453); }
void vertex() {
	wpos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	wnorm = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}
void fragment() {
	vec2 g = vec2((abs(wnorm.x) > 0.5 ? wpos.z : wpos.x) / 1.6, wpos.y / 2.2);
	vec2 cell = floor(g);
	vec2 f = fract(g);
	float win = step(0.25, f.x) * step(f.x, 0.75) * step(0.3, f.y) * step(f.y, 0.75);
	float lit = step(0.62, hash(cell)) * win;
	float shade = wnorm.x > 0.5 ? 1.25 : (wnorm.z > 0.5 ? 0.9 : 0.8);
	vec3 col = base_color * shade * (0.8 + 0.4 * clamp(wpos.y / 60.0, 0.0, 1.0));
	col = mix(col, vec3(1.0, 0.78, 0.45), lit * 0.9);
	ALBEDO = col;
}
"""

const ARCH_NICHE := """
shader_type spatial;
render_mode unshaded;
uniform vec3 inner : source_color = vec3(0.1, 0.06, 0.04);
uniform vec3 glow : source_color = vec3(0.85, 0.5, 0.22);
void fragment() {
	vec2 uv = UV;
	// pointed arch: rectangle bottom, ogive top
	float x = abs(uv.x - 0.5) * 2.0;
	float top = 1.0 - uv.y;
	float body = step(top, 0.62);
	float arc = step(length(vec2(x + 0.55, (top - 0.62) * 2.4)), 1.55);
	float inside = max(body, arc * step(0.62, top));
	if (inside < 0.5) discard;
	float g = smoothstep(1.0, 0.0, top) * (1.0 - x * 0.6);
	ALBEDO = mix(inner, glow, g * 0.55);
}
"""

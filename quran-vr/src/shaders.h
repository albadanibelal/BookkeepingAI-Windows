// GLSL ES 3.00 shader sources. A "#version 300 es" + precision header is
// prepended by gl_program().
#pragma once

// ---------------------------------------------------------------- common
#define GLSL(...) #__VA_ARGS__ "\n"

// Standard mesh vertex shader (pos, normal, uv).
static const char* VS_MESH __attribute__((unused)) = GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aUV;
uniform mat4 uModel;
uniform mat4 uViewProj;
out vec3 vWPos;
out vec3 vN;
out vec2 vUV;
out vec3 vLPos;
void main() {
  vec4 w = uModel * vec4(aPos, 1.0);
  vWPos = w.xyz;
  vN = mat3(uModel) * aNrm;
  vUV = aUV;
  vLPos = aPos;
  gl_Position = uViewProj * w;
});

// ---------------------------------------------------------------- lit materials
// uMode: 0 standard, 1 floor, 2 page-block edge, 3 leather cover,
//        4 emissive strip, 5 glossy dark, 6 dome rib (white w/ light seam)
static const char* FS_LIT __attribute__((unused)) = GLSL(
in vec3 vWPos; in vec3 vN; in vec2 vUV; in vec3 vLPos;
uniform vec3 uAlbedo;
uniform vec3 uEmissive;
uniform float uSpec;
uniform float uGloss;
uniform int uMode;
uniform vec4 uParam;
uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform vec3 uLightCol;
uniform vec3 uSkyCol;
uniform vec3 uGroundCol;
uniform float uTime;
out vec4 fragColor;

float hash21(vec2 p) { p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vnoise(vec2 p) {
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}

void main() {
  vec3 N = normalize(vN);
  if (!gl_FrontFacing) N = -N;
  vec3 V = normalize(uCamPos - vWPos);
  vec3 albedo = uAlbedo;
  vec3 emissive = uEmissive;
  float spec = uSpec, gloss = uGloss;

  if (uMode == 1) {
    // Floor: satin white with concentric light seams, fading into the dark at the rim.
    float r = length(vLPos.xz);
    float seams = 0.0;
    seams += smoothstep(0.012, 0.0, abs(r - 1.55)) * 0.9;
    seams += smoothstep(0.010, 0.0, abs(r - 3.2)) * 0.6;
    seams += smoothstep(0.010, 0.0, abs(r - 5.1)) * 0.45;
    float tiles = smoothstep(0.006, 0.0, abs(fract(atan(vLPos.z, vLPos.x) * 24.0 / 6.2831853) - 0.5) - 0.497) * step(3.2, r) * step(r, 5.1);
    albedo *= mix(1.0, 0.55, smoothstep(3.5, 9.0, r));
    albedo *= 0.94 + 0.06 * vnoise(vLPos.xz * 3.0);
    emissive += vec3(0.45, 0.78, 1.0) * (seams + tiles * 0.35) * uParam.x;
  } else if (uMode == 2) {
    // Book block edge: fine page lines
    float lines = 0.5 + 0.5 * sin(vUV.y * 900.0);
    albedo *= mix(0.86, 1.0, lines);
  } else if (uMode == 3) {
    // Leather cover with a tooled gold border
    float n = vnoise(vUV * 90.0) * 0.5 + vnoise(vUV * 230.0) * 0.5;
    albedo *= 0.85 + 0.3 * n;
    vec2 e = min(vUV, 1.0 - vUV) * uParam.xy;
    float border = smoothstep(0.004, 0.0, abs(e.x - 0.012)) + smoothstep(0.004, 0.0, abs(e.y - 0.012));
    border *= step(0.008, min(e.x, e.y) + 0.004);
    float b2 = smoothstep(0.0015, 0.0, abs(min(e.x, e.y) - 0.006));
    vec3 gold = vec3(0.83, 0.62, 0.26);
    albedo = mix(albedo, gold, clamp(border + b2, 0.0, 1.0));
    spec = mix(spec, 0.9, clamp(border + b2, 0.0, 1.0));
  } else if (uMode == 4) {
    float pulse = 0.85 + 0.15 * sin(uTime * 1.2 + vUV.x * 12.0);
    fragColor = vec4(uEmissive * pulse, 1.0);
    return;
  } else if (uMode == 6) {
    float seam = smoothstep(0.08, 0.0, abs(vUV.y - 0.5)) * step(0.5, abs(N.z));
    emissive += vec3(0.55, 0.85, 1.0) * seam * 0.9;
  }

  float ndl = max(dot(N, uLightDir), 0.0);
  float wrap = max((dot(N, uLightDir) + 0.35) / 1.35, 0.0);
  vec3 amb = mix(uGroundCol, uSkyCol, N.y * 0.5 + 0.5);
  vec3 H = normalize(uLightDir + V);
  float sp = pow(max(dot(N, H), 0.0), gloss) * spec;
  float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);
  vec3 col = albedo * (amb + uLightCol * mix(ndl, wrap, 0.5)) + uLightCol * sp + uSkyCol * fres * spec * 0.6 + emissive;
  fragColor = vec4(col, 1.0);
});

// ---------------------------------------------------------------- Mushaf page
static const char* FS_PAGE __attribute__((unused)) = GLSL(
in vec3 vWPos; in vec3 vN; in vec2 vUV; in vec3 vLPos;
uniform sampler2D uFrame;
uniform sampler2D uInk;
uniform vec3 uInkCol;
uniform vec3 uGoldCol;
uniform vec3 uMarkCol;
uniform vec3 uPaperTint;
uniform float uNight;
uniform float uLoaded;
uniform float uSpine;      // 0: spine at u=0, 1: spine at u=1
uniform float uBack;       // 1 when drawing the back face of a flipping leaf
uniform vec3 uCamPos;
uniform vec3 uLightDir;
uniform float uHighlight;
out vec4 fragColor;
void main() {
  float u = mix(vUV.x, 1.0 - vUV.x, uBack);
  vec2 uv = vec2(u, 1.0 - vUV.y);
  vec3 paper = texture(uFrame, uv).rgb;
  float lum = dot(paper, vec3(0.3, 0.55, 0.15));
  vec3 nightPaper = paper * vec3(0.045, 0.05, 0.058) + max(paper - vec3(lum), 0.0) * 0.55;
  paper = mix(paper * uPaperTint, nightPaper, uNight);
  vec3 k = texture(uInk, uv).rgb * uLoaded;
  // Coverage was rasterized in sRGB space; convert for linear blending.
  vec3 kd = 1.0 - pow(1.0 - k, vec3(2.2));
  vec3 kl = pow(k, vec3(1.25));
  k = mix(kd, kl, uNight);
  vec3 c = paper;
  c = mix(c, uGoldCol, k.g);
  c = mix(c, uMarkCol, k.b);
  c = mix(c, uInkCol, k.r);
  // Gutter shading near the spine, soft light falloff on the page curve.
  float d = mix(u, 1.0 - u, uSpine);
  c *= mix(0.62, 1.0, smoothstep(0.0, 0.16, d));
  vec3 N = normalize(vN);
  if (!gl_FrontFacing) N = -N;
  float ndl = dot(N, uLightDir) * 0.5 + 0.5;
  c *= 0.86 + 0.22 * ndl;
  c += vec3(0.9, 0.8, 0.5) * uHighlight * 0.06;
  fragColor = vec4(c, 1.0);
});

// ---------------------------------------------------------------- sky (cubemap)
static const char* VS_SKY __attribute__((unused)) = GLSL(
layout(location = 0) in vec3 aPos;
uniform mat4 uViewRotProj;
out vec3 vDir;
void main() {
  vDir = aPos;
  vec4 p = uViewRotProj * vec4(aPos, 1.0);
  gl_Position = p.xyww;
});

static const char* FS_SKY __attribute__((unused)) = GLSL(
in vec3 vDir;
uniform samplerCube uStars;
uniform vec3 uSunDir;
uniform float uSunVis;
out vec4 fragColor;
void main() {
  vec3 d = normalize(vDir);
  vec3 c = texture(uStars, d).rgb;
  float s = max(dot(d, uSunDir), 0.0);
  c += uSunVis * (vec3(1.0, 0.9, 0.75) * pow(s, 900.0) * 30.0 + vec3(1.0, 0.8, 0.55) * pow(s, 60.0) * 0.6 + vec3(0.5, 0.6, 1.0) * pow(s, 8.0) * 0.06);
  fragColor = vec4(c, 1.0);
});

// Procedural star field rendered once into a cubemap.
static const char* VS_FULLSCREEN __attribute__((unused)) = GLSL(
out vec2 vUV;
void main() {
  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  vUV = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
});

static const char* FS_STARGEN __attribute__((unused)) = GLSL(
in vec2 vUV;
uniform int uFace;
out vec4 fragColor;
vec3 hash33(vec3 p) {
  p = fract(p * vec3(443.897, 441.423, 437.195));
  p += dot(p, p.yxz + 19.19);
  return fract((p.xxy + p.yxx) * p.zyx);
}
float hash31(vec3 p) { return hash33(p).x; }
float noise3(vec3 p) {
  vec3 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float n000 = hash31(i), n100 = hash31(i + vec3(1, 0, 0)), n010 = hash31(i + vec3(0, 1, 0)), n110 = hash31(i + vec3(1, 1, 0));
  float n001 = hash31(i + vec3(0, 0, 1)), n101 = hash31(i + vec3(1, 0, 1)), n011 = hash31(i + vec3(0, 1, 1)), n111 = hash31(i + vec3(1, 1, 1));
  return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y), mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}
float fbm(vec3 p) {
  float a = 0.5, s = 0.0;
  for (int i = 0; i < 5; i++) { s += a * noise3(p); p *= 2.03; a *= 0.5; }
  return s;
}
vec3 faceDir(int f, vec2 uv) {
  vec2 p = uv * 2.0 - 1.0;
  if (f == 0) return vec3(1, -p.y, -p.x);
  if (f == 1) return vec3(-1, -p.y, p.x);
  if (f == 2) return vec3(p.x, 1, p.y);
  if (f == 3) return vec3(p.x, -1, -p.y);
  if (f == 4) return vec3(p.x, -p.y, 1);
  return vec3(-p.x, -p.y, -1);
}
float starLayer(vec3 d, float scale, float size, float density) {
  vec3 p = d * scale;
  vec3 cell = floor(p);
  vec3 h = hash33(cell);
  if (h.z > density) return 0.0;
  vec3 sp = cell + 0.2 + 0.6 * hash33(cell + 7.1);
  float dist = length(p - sp);
  float b = pow(hash31(cell + 3.3), 6.0) * 3.0 + 0.25;
  return b * exp(-dist * dist / (size * size));
}
void main() {
  vec3 d = normalize(faceDir(uFace, vUV));
  // Milky way band
  vec3 gN = normalize(vec3(0.35, 0.82, -0.45));
  float band = exp(-pow(dot(d, gN) / 0.22, 2.0));
  float neb = fbm(d * 3.5) * fbm(d * 7.0 + 3.0);
  vec3 col = vec3(0.004, 0.006, 0.014);
  col += band * (vec3(0.05, 0.045, 0.07) * neb * 2.2 + vec3(0.012, 0.014, 0.024));
  col += vec3(0.02, 0.01, 0.035) * pow(fbm(d * 2.0 + 11.0), 3.0);
  float st = 0.0;
  st += starLayer(d, 90.0, 0.06, 0.55);
  st += starLayer(d, 170.0, 0.08, 0.5 + band * 0.4) * 0.6;
  st += starLayer(d, 320.0, 0.10, 0.3 + band * 0.6) * 0.35;
  vec3 tint = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.9, 0.75), hash31(floor(d * 90.0)));
  col += tint * st;
  fragColor = vec4(col, 1.0);
});

// ---------------------------------------------------------------- Earth
static const char* FS_EARTH __attribute__((unused)) = GLSL(
in vec3 vWPos; in vec3 vN; in vec2 vUV; in vec3 vLPos;
uniform sampler2D uDay;
uniform sampler2D uNightTex;
uniform sampler2D uWater;
uniform sampler2D uClouds;
uniform vec3 uSunDir;
uniform vec3 uCamPos;
uniform vec3 uMarker;     // world-space unit normal of Makkah
uniform float uTime;
out vec4 fragColor;
void main() {
  vec3 N = normalize(vN);
  vec3 V = normalize(uCamPos - vWPos);
  float ndl = dot(N, uSunDir);
  vec3 day = texture(uDay, vUV).rgb;
  vec3 night = texture(uNightTex, vUV).rgb;
  float water = texture(uWater, vUV).r;
  float cloud = texture(uClouds, vUV + vec2(uTime * 0.0004, 0.0)).r;
  cloud = smoothstep(0.15, 0.95, cloud) * 0.8;
  float dayMix = smoothstep(-0.10, 0.20, ndl);
  vec3 H = normalize(uSunDir + V);
  float spec = pow(max(dot(N, H), 0.0), 70.0) * water * 0.9 * (1.0 - cloud);
  vec3 lit = day * (0.06 + 1.55 * max(ndl, 0.0));
  lit = mix(lit, vec3(1.0) * (0.08 + 1.7 * max(ndl, 0.0)), cloud * 0.85);
  vec3 col = lit + spec * vec3(1.0, 0.92, 0.8) * dayMix;
  vec3 lights = night * night * vec3(1.6, 1.15, 0.7) * 1.6 * (1.0 - cloud * 0.7);
  col += lights * (1.0 - dayMix);
  // Terminator glow + atmosphere rim
  float mu = max(dot(N, V), 0.0);
  float rim = pow(1.0 - mu, 2.2);
  float sunside = smoothstep(-0.35, 0.5, ndl);
  col = mix(col, vec3(0.35, 0.6, 1.0) * (0.15 + 0.9 * sunside), rim * 0.75);
  col += vec3(1.0, 0.45, 0.15) * smoothstep(0.18, 0.0, abs(ndl)) * 0.10;
  // Makkah marker: soft pulsing golden light
  float md = distance(N, uMarker);
  float pulse = 0.6 + 0.4 * sin(uTime * 2.6);
  col += vec3(1.0, 0.78, 0.35) * (smoothstep(0.012, 0.004, md) * 3.0 + smoothstep(0.06, 0.0, md) * 0.35 * pulse);
  col += vec3(1.0, 0.8, 0.4) * smoothstep(0.003, 0.0, abs(md - 0.015 - 0.03 * fract(uTime * 0.5))) * (1.0 - fract(uTime * 0.5)) * 0.8;
  fragColor = vec4(col, 1.0);
});

static const char* FS_ATMO __attribute__((unused)) = GLSL(
in vec3 vWPos; in vec3 vN; in vec2 vUV; in vec3 vLPos;
uniform vec3 uSunDir;
uniform vec3 uCamPos;
out vec4 fragColor;
void main() {
  vec3 N = normalize(vN);
  vec3 V = normalize(uCamPos - vWPos);
  float mu = max(dot(N, V), 0.0);
  float halo = smoothstep(0.0, 0.32, mu) * pow(1.0 - mu, 4.0) * 3.2;
  float sunside = smoothstep(-0.45, 0.45, dot(N, uSunDir));
  vec3 c = vec3(0.30, 0.55, 1.0) * halo * (0.12 + 0.95 * sunside);
  fragColor = vec4(c, 1.0);
});

// ---------------------------------------------------------------- additive billboard (sun glow, laser)
static const char* VS_BILL __attribute__((unused)) = GLSL(
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aCol;
uniform mat4 uViewProj;
out vec2 vUV;
out vec4 vCol;
void main() {
  vUV = aUV;
  vCol = aCol;
  gl_Position = uViewProj * vec4(aPos, 1.0);
});

// uKind: 0 radial glow, 1 beam (fade along u, soft across v), 2 sun with streaks
static const char* FS_BILL __attribute__((unused)) = GLSL(
in vec2 vUV;
in vec4 vCol;
uniform int uKind;
out vec4 fragColor;
void main() {
  vec2 p = vUV * 2.0 - 1.0;
  float a;
  if (uKind == 0) {
    float r = length(p);
    a = exp(-r * r * 6.0) + smoothstep(0.35, 0.2, r) * 0.6;
  } else if (uKind == 1) {
    float across = exp(-p.y * p.y * 7.0);
    float core = exp(-p.y * p.y * 60.0);
    a = (across * 0.45 + core) * smoothstep(1.0, 0.0, vUV.x) * smoothstep(0.0, 0.04, vUV.x);
  } else {
    float r = length(p);
    a = exp(-r * r * 30.0) * 2.0 + exp(-r * 5.0) * 0.5 + exp(-r * r * 2.2) * 0.12;
    a += exp(-abs(p.y) * 90.0) * exp(-abs(p.x) * 2.5) * 0.5;
    a += exp(-abs(p.x) * 120.0) * exp(-abs(p.y) * 4.0) * 0.2;
  }
  fragColor = vec4(vCol.rgb * vCol.a * a, 0.0);
});

// ---------------------------------------------------------------- UI (panels, text, icons)
static const char* VS_UI __attribute__((unused)) = GLSL(
layout(location = 0) in vec2 aPx;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aCol;
layout(location = 3) in vec4 aShape;
layout(location = 4) in vec4 aClip;
layout(location = 5) in float aMode;
uniform mat4 uMVP;
uniform float uAlpha;
out vec2 vPx;
out vec2 vUV;
out vec4 vCol;
out vec4 vShape;
out vec4 vClip;
out float vMode;
void main() {
  vPx = aPx;
  vUV = aUV;
  vCol = vec4(pow(aCol.rgb, vec3(2.2)), aCol.a * uAlpha);
  vShape = aShape;
  vClip = aClip;
  vMode = aMode;
  gl_Position = uMVP * vec4(aPx, 0.0, 1.0);
});

static const char* FS_UI __attribute__((unused)) = GLSL(
in vec2 vPx;
in vec2 vUV;
in vec4 vCol;
in vec4 vShape;
in vec4 vClip;
in float vMode;
uniform sampler2D uAtlas;
out vec4 fragColor;
float sdRoundBox(vec2 p, vec2 b, float r) {
  vec2 q = abs(p) - b + r;
  return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
void main() {
  if (vPx.x < vClip.x || vPx.y < vClip.y || vPx.x > vClip.z || vPx.y > vClip.w) discard;
  float a;
  int mode = int(vMode + 0.5);
  if (mode == 2) {
    a = pow(texture(uAtlas, vUV).r, 1.2);
  } else {
    float d = sdRoundBox(vUV, vShape.xy, vShape.z);
    float aa = max(fwidth(d), 0.001) * 0.85;
    if (mode == 0) a = 1.0 - smoothstep(-aa, aa, d);
    else if (mode == 1) a = 1.0 - smoothstep(vShape.w * 0.5 - aa, vShape.w * 0.5 + aa, abs(d + vShape.w * 0.5));
    else a = 1.0 - smoothstep(-vShape.w, vShape.w, d);  // soft glow / shadow
  }
  fragColor = vec4(vCol.rgb, vCol.a * a);
});

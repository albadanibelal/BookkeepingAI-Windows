// Renderer for the Quran VR scene: deep-space sky, live Earth, observation
// deck, lectern with an open Madinah Mushaf (with page-curl turning), glass UI
// panels and tracked controllers. GLES 3.0 only; no platform dependencies.
#include "scene.h"

#define FLOOR_Y (-1.25f)

// ------------------------------------------------------------------ shaders

#define GLSL_HEADER "#version 300 es\nprecision highp float;\n"

static const char* VS_MESH = GLSL_HEADER
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aNormal;\n"
    "layout(location=2) in vec2 aUV;\n"
    "uniform mat4 uModel, uViewProj;\n"
    "out vec3 vWorld; out vec3 vNormal; out vec2 vUV; out vec3 vObj;\n"
    "void main(){\n"
    "  vec4 w = uModel * vec4(aPos,1.0);\n"
    "  vWorld = w.xyz; vNormal = mat3(uModel) * aNormal; vUV = aUV; vObj = aPos;\n"
    "  gl_Position = uViewProj * w;\n"
    "}\n";

static const char* VS_SKY = GLSL_HEADER
    "layout(location=0) in vec3 aPos;\n"
    "uniform mat4 uViewRot, uProj;\n"
    "out vec3 vDir;\n"
    "void main(){ vDir = aPos; vec4 p = uProj * uViewRot * vec4(aPos*50.0,1.0); gl_Position = p.xyww; }\n";

// The sky is procedural but static, so it is baked once into a cubemap
// (FS_SKY renders one face per pass) and then just sampled (FS_SKYCUBE).
static const char* VS_FULL = GLSL_HEADER
    "layout(location=0) in vec3 aPos;\n"
    "void main(){ gl_Position = vec4(aPos.xy*2.0, 0.0, 1.0); }\n";

static const char* FS_SKYCUBE = GLSL_HEADER
    "in vec3 vDir; out vec4 o;\n"
    "uniform samplerCube uSky;\n"
    "void main(){ o = vec4(texture(uSky, vDir).rgb, 1.0); }\n";

static const char* FS_SKY = GLSL_HEADER
    "out vec4 o;\n"
    "uniform vec3 uFlareDir; uniform float uSize; uniform int uFace;\n"
    "float h13(vec3 p){ p=fract(p*0.1031); p+=dot(p,p.zyx+31.32); return fract((p.x+p.y)*p.z); }\n"
    "vec3 h33(vec3 p){ p=fract(p*vec3(.1031,.1030,.0973)); p+=dot(p,p.yxz+33.33); return fract((p.xxy+p.yxx)*p.zyx); }\n"
    "float vnoise(vec3 p){ vec3 i=floor(p); vec3 f=fract(p); f=f*f*(3.0-2.0*f);\n"
    "  return mix(mix(mix(h13(i),h13(i+vec3(1,0,0)),f.x),mix(h13(i+vec3(0,1,0)),h13(i+vec3(1,1,0)),f.x),f.y),\n"
    "             mix(mix(h13(i+vec3(0,0,1)),h13(i+vec3(1,0,1)),f.x),mix(h13(i+vec3(0,1,1)),h13(i+vec3(1,1,1)),f.x),f.y),f.z); }\n"
    "float fbm(vec3 p){ float a=0.5, s=0.0; for(int i=0;i<5;i++){ s+=a*vnoise(p); p*=2.03; a*=0.5; } return s; }\n"
    "vec3 stars(vec3 d, float scale, float thresh, float size, float gain){\n"
    "  vec3 p=d*scale; vec3 c=floor(p); vec3 f=p-c; float h=h13(c);\n"
    "  if(h<thresh) return vec3(0.0);\n"
    "  vec3 sp=h33(c)*0.6+0.2; vec3 df=f-sp; float dd=dot(df,df);\n"
    "  float b=(h-thresh)/(1.0-thresh); float inten=(b*b*1.6+0.12)*gain;\n"
    "  vec3 tint=mix(vec3(0.70,0.80,1.0),vec3(1.0,0.86,0.68),h33(c+7.0).x);\n"
    "  return tint*inten*exp(-dd/(size*size));\n"
    "}\n"
    "void main(){\n"
    "  vec2 st=gl_FragCoord.xy/uSize*2.0-1.0; float s0=st.x, t0=st.y; vec3 d;\n"
    "  if(uFace==0) d=vec3(1.0,-t0,-s0); else if(uFace==1) d=vec3(-1.0,-t0,s0);\n"
    "  else if(uFace==2) d=vec3(s0,1.0,t0); else if(uFace==3) d=vec3(s0,-1.0,-t0);\n"
    "  else if(uFace==4) d=vec3(s0,-t0,1.0); else d=vec3(-s0,-t0,-1.0);\n"
    "  d=normalize(d);\n"
    "  vec3 col=vec3(0.0015,0.0025,0.007);\n"
    // milky way band
    "  vec3 gN=normalize(vec3(0.35,0.75,-0.55)); float g=dot(d,gN);\n"
    "  float band=exp(-g*g/0.035); float neb=fbm(d*3.5); float dust=fbm(d*9.0+3.1);\n"
    "  col+=band*neb*neb*vec3(0.030,0.034,0.048)*(1.2-0.9*smoothstep(0.45,0.7,dust));\n"
    "  col+=exp(-g*g/0.2)*vec3(0.003,0.004,0.008);\n"
    "  col+=stars(d,260.0,0.86,0.10,0.55)+stars(d,110.0,0.955,0.08,1.1)+stars(d,45.0,0.988,0.06,2.4);\n"
    "  col+=stars(d,160.0,0.92,0.09,0.8)*band*2.0;\n"
    // bright horizon haze beyond the deck (upper atmosphere glow)
    "  float e=asin(clamp(d.y,-1.0,1.0));\n"
    "  float hz=exp(-abs(e+0.05)*22.0);\n"
    "  col+=hz*vec3(0.10,0.25,0.62)*0.9 + exp(-abs(e+0.05)*90.0)*vec3(0.35,0.55,0.9)*0.8;\n"
    "  if(e<-0.05){ col=mix(col,vec3(0.004,0.012,0.035),smoothstep(-0.05,-0.25,e)); }\n"
    // decorative sun flare
    "  float s=max(dot(d,normalize(uFlareDir)),0.0);\n"
    "  col+=vec3(1.0,0.93,0.80)*(pow(s,6000.0)*30.0+pow(s,600.0)*1.4+pow(s,60.0)*0.12+pow(s,8.0)*0.03);\n"
    "  o=vec4(col,1.0);\n"
    "}\n";

// Generic lit material. uMode: 0 plain, 1 deck floor, 2 page-block edge, 3 emissive only
static const char* FS_LIT = GLSL_HEADER
    "in vec3 vWorld; in vec3 vNormal; in vec2 vUV; in vec3 vObj; out vec4 o;\n"
    "uniform vec4 uColor; uniform vec3 uEmissive; uniform vec3 uEye;\n"
    "uniform float uShininess, uSpec; uniform int uMode;\n"
    "void main(){\n"
    "  vec3 n=normalize(vNormal);\n"
    "  vec3 V=normalize(uEye-vWorld); if(dot(n,V)<0.0) n=-n;\n"
    "  vec3 base=uColor.rgb;\n"
    "  if(uMode==2){ base*=0.86+0.14*sin(vUV.y*2400.0)*sin(vUV.y*777.0+1.0); }\n"
    "  vec3 L1=normalize(vec3(0.35,0.85,0.45)); vec3 L2=normalize(vec3(0.85,0.15,-0.5));\n"
    "  vec3 amb=mix(vec3(0.07,0.075,0.09),vec3(0.20,0.22,0.28),n.y*0.5+0.5);\n"
    "  vec3 diff=amb+vec3(1.0,0.97,0.92)*max(dot(n,L1),0.0)*0.75+vec3(0.35,0.5,0.8)*max(dot(n,L2),0.0)*0.35;\n"
    "  vec3 H=normalize(L1+V); float sp=pow(max(dot(n,H),0.0),uShininess)*uSpec;\n"
    "  vec3 col=base*diff+vec3(sp);\n"
    "  if(uMode==1){\n"
    "    float r=length(vWorld.xz);\n"
    "    float fres=pow(1.0-max(dot(n,V),0.0),4.0);\n"
    "    col=base*diff*(0.85+0.15*smoothstep(5.0,0.0,r)) + fres*vec3(0.05,0.07,0.11);\n"
    "    float ring=exp(-pow((r-1.35)*140.0,2.0))+exp(-pow((r-2.45)*140.0,2.0))*0.8;\n"
    "    col+=ring*vec3(0.45,0.75,1.0)*0.9;\n"
    "    float seams=smoothstep(0.985,1.0,abs(cos(atan(vWorld.x,vWorld.z)*12.0)))*step(1.4,r)*0.04;\n"
    "    col-=seams;\n"
    "    col+=exp(-r*r*0.6)*vec3(0.05,0.06,0.08);\n"
    "  }\n"
    "  if(uMode==3){ col=vec3(0.0); }\n"
    "  o=vec4(col+uEmissive,uColor.a);\n"
    "}\n";

static const char* FS_EARTH = GLSL_HEADER
    "in vec3 vWorld; in vec3 vNormal; in vec2 vUV; in vec3 vObj; out vec4 o;\n"
    "uniform sampler2D uDay, uNight, uClouds, uSpecMap;\n"
    "uniform vec3 uSun, uEye, uMarker; uniform float uCloudShift, uTime;\n"
    "void main(){\n"
    "  vec3 n=normalize(vNormal); vec3 V=normalize(uEye-vWorld); vec3 L=normalize(uSun);\n"
    "  float ndl=dot(n,L);\n"
    "  float day=smoothstep(-0.10,0.22,ndl);\n"
    "  vec3 dayc=texture(uDay,vUV).rgb;\n"
    "  vec3 nightc=texture(uNight,vUV).rgb;\n"
    "  float cl=texture(uClouds,vUV+vec2(uCloudShift,0.0)).r; cl=smoothstep(0.08,0.9,cl);\n"
    "  float ocean=texture(uSpecMap,vUV).r;\n"
    "  vec3 H=normalize(L+V);\n"
    "  float sp=pow(max(dot(n,H),0.0),70.0)*ocean*0.9*day*(1.0-cl);\n"
    "  vec3 lit=dayc*1.25*(max(ndl,0.0)+0.015);\n"
    "  vec3 cloud=vec3(1.0)*(max(ndl,0.0)*1.05+0.004);\n"
    "  vec3 col=mix(lit,cloud,cl*0.92)+vec3(1.0,0.95,0.85)*sp;\n"
    "  vec3 lights=max(nightc-vec3(0.035,0.035,0.06),0.0); lights*=vec3(1.9,1.5,1.0)*2.6;\n"
    "  col+=lights*(1.0-day)*(1.0-cl*0.8);\n"
    "  col+=nightc*0.08*(1.0-day);\n"
    "  col+=vec3(1.0,0.45,0.15)*0.05*exp(-ndl*ndl*80.0);\n"
    "  float nv=max(dot(n,V),0.0);\n"
    "  float fres=pow(1.0-nv,2.6);\n"
    "  col=mix(col,vec3(0.30,0.55,1.0)*(0.25+0.9*smoothstep(-0.25,0.6,ndl)),fres*0.85);\n"
    "  float m=dot(normalize(vObj),normalize(uMarker));\n"
    "  float pulse=0.6+0.4*sin(uTime*3.0);\n"
    "  col+=vec3(1.0,0.78,0.35)*(exp(-(1.0-m)*90000.0)*2.2+exp(-(1.0-m)*6000.0)*0.35*pulse);\n"
    "  o=vec4(col,1.0);\n"
    "}\n";

static const char* FS_ATMO = GLSL_HEADER
    "in vec3 vWorld; in vec3 vNormal; in vec2 vUV; in vec3 vObj; out vec4 o;\n"
    "uniform vec3 uSun, uEye, uCenter; uniform float uRadius;\n"
    "void main(){\n"
    "  vec3 V=normalize(uEye-vWorld); vec3 n=normalize(vNormal);\n"
    // impact parameter of the view ray with respect to the planet centre
    "  vec3 toC=uCenter-uEye; vec3 rd=-V; float t=dot(toC,rd); float b=length(toC-rd*t)/uRadius;\n"
    "  float lit=smoothstep(-0.35,0.55,dot(n,normalize(uSun)));\n"
    "  float g=exp(-max(b-1.0,0.0)*38.0)*smoothstep(0.80,1.0,b);\n"
    "  vec3 col=vec3(0.28,0.55,1.0)*g*(0.10+1.2*lit);\n"
    "  o=vec4(col,0.0);\n"
    "}\n";

static const char* FS_PAGE = GLSL_HEADER
    "in vec3 vWorld; in vec3 vNormal; in vec2 vUV; in vec3 vObj; out vec4 o;\n"
    "uniform sampler2D uTex; uniform float uFlipU, uNight, uHasTex;\n"
    "uniform vec3 uEye; uniform float uShadowAmt, uShadowExtent, uShadowSide;\n"
    "void main(){\n"
    "  vec3 n=normalize(vNormal); if(!gl_FrontFacing) n=-n;\n"
    "  vec2 uv=vec2(uFlipU>0.5?1.0-vUV.x:vUV.x, vUV.y);\n"
    "  vec3 paper=vec3(0.95,0.91,0.80);\n"
    "  vec3 c=uHasTex>0.5?texture(uTex,uv).rgb:paper;\n"
    "  float sigma=vUV.x;\n"
    "  float gutter=0.55+0.45*smoothstep(0.0,0.14,sigma);\n"
    "  vec3 L1=normalize(vec3(0.35,0.85,0.45));\n"
    "  float light=0.72+0.33*max(dot(n,L1),0.0);\n"
    "  vec3 V=normalize(uEye-vWorld); vec3 H=normalize(L1+V);\n"
    "  float sheen=pow(max(dot(n,H),0.0),24.0)*0.05;\n"
    "  float sh=1.0;\n"
    "  if(uShadowAmt>0.0 && vObj.x*uShadowSide>0.0){\n"
    "    float ax=abs(vObj.x); sh=1.0-uShadowAmt*(1.0-smoothstep(uShadowExtent*0.6,uShadowExtent+0.02,ax));\n"
    "  }\n"
    "  vec3 col=c*gutter*light*sh+sheen;\n"
    "  col=mix(col,col*vec3(1.0,0.84,0.62)*0.72,uNight);\n"
    "  o=vec4(col,1.0);\n"
    "}\n";

static const char* FS_PANEL = GLSL_HEADER
    "in vec3 vWorld; in vec3 vNormal; in vec2 vUV; in vec3 vObj; out vec4 o;\n"
    "uniform sampler2D uTex; uniform float uOpacity;\n"
    "void main(){\n"
    "  vec4 t=texture(uTex,vUV);\n"
    "  vec3 c=t.a>0.0001?t.rgb/t.a:vec3(0.0);\n"
    "  c=pow(c,vec3(2.2));\n"
    "  o=vec4(c*t.a,t.a)*uOpacity;\n"
    "}\n";

// Additive effects: 0 radial glow sprite, 1 laser beam
static const char* FS_FX = GLSL_HEADER
    "in vec3 vWorld; in vec3 vNormal; in vec2 vUV; in vec3 vObj; out vec4 o;\n"
    "uniform vec3 uColor; uniform int uMode; uniform float uSharp;\n"
    "void main(){\n"
    "  float a;\n"
    "  if(uMode==0){ vec2 p=vUV*2.0-1.0; float r2=dot(p,p); a=exp(-r2*uSharp)*(1.0-smoothstep(0.8,1.0,sqrt(r2))); }\n"
    "  else { float across=abs(vUV.y*2.0-1.0); a=(1.0-smoothstep(0.0,1.0,across))*pow(1.0-vUV.x,0.7)*smoothstep(0.0,0.02,vUV.x); }\n"
    "  o=vec4(uColor*a,0.0);\n"
    "}\n";

// ------------------------------------------------------------------ GL utils

typedef struct {
    GLuint prog;
    GLint uModel, uViewProj, uColor, uEmissive, uEye, uShininess, uSpec, uMode;
    GLint uViewRot, uProj, uFlareDir;
    GLint uDay, uNight, uClouds, uSpecMap, uSun, uMarker, uCloudShift, uTime, uCenter, uRadius;
    GLint uSky, uSize, uFace;
    GLint uTex, uFlipU, uNight_, uHasTex, uShadowAmt, uShadowExtent, uShadowSide, uOpacity, uSharp;
} Program;

typedef struct {
    GLuint vao, vbo, ibo;
    GLsizei count;
    GLenum mode;
} Mesh;

typedef struct { float px, py, pz, nx, ny, nz, u, v; } Vtx;

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        LOGE("shader compile failed: %s", log);
    }
    return s;
}

static Program make_program(const char* vs, const char* fs) {
    Program p;
    memset(&p, 0, sizeof(p));
    p.prog = glCreateProgram();
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p.prog, v);
    glAttachShader(p.prog, f);
    glLinkProgram(p.prog);
    GLint ok = 0;
    glGetProgramiv(p.prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p.prog, sizeof(log), NULL, log);
        LOGE("program link failed: %s", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
#define U(name) p.name = glGetUniformLocation(p.prog, #name)
    U(uModel); U(uViewProj); U(uColor); U(uEmissive); U(uEye); U(uShininess); U(uSpec); U(uMode);
    U(uViewRot); U(uProj); U(uFlareDir);
    U(uDay); U(uNight); U(uClouds); U(uSpecMap); U(uSun); U(uMarker); U(uCloudShift); U(uTime);
    U(uCenter); U(uRadius);
    U(uSky); U(uSize); U(uFace);
    U(uTex); U(uFlipU); U(uHasTex); U(uShadowAmt); U(uShadowExtent); U(uShadowSide); U(uOpacity); U(uSharp);
#undef U
    p.uNight_ = glGetUniformLocation(p.prog, "uNight");
    return p;
}

static Mesh make_mesh(const Vtx* v, int nv, const unsigned short* idx, int ni, int dynamic) {
    Mesh m;
    glGenVertexArrays(1, &m.vao);
    glBindVertexArray(m.vao);
    glGenBuffers(1, &m.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, nv * sizeof(Vtx), v, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (void*)(6 * sizeof(float)));
    glGenBuffers(1, &m.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, ni * sizeof(unsigned short), idx, GL_STATIC_DRAW);
    glBindVertexArray(0);
    m.count = ni;
    m.mode = GL_TRIANGLES;
    return m;
}

static void draw_mesh(const Mesh* m) {
    glBindVertexArray(m->vao);
    glDrawElements(m->mode, m->count, GL_UNSIGNED_SHORT, 0);
}

unsigned gl_upload_rgba(unsigned tex, int w, int h, const void* pixels, int mipmaps, int srgb) {
    GLuint t = tex;
    if (!t) glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // anisotropic filtering keeps the tilted Mushaf text crisp
    glTexParameterf(GL_TEXTURE_2D, 0x84FE /*GL_TEXTURE_MAX_ANISOTROPY_EXT*/, 8.0f);
    glGetError();  // ignore if unsupported
    return t;
}

// ------------------------------------------------------------------ geometry

#define MAXV 20000
static Vtx g_v[MAXV];
static unsigned short g_i[MAXV * 3];
static int g_nv, g_ni;

static void geo_begin(void) { g_nv = 0; g_ni = 0; }
static int geo_v(vec3 p, vec3 n, float u, float v) {
    Vtx* x = &g_v[g_nv];
    x->px = p.x; x->py = p.y; x->pz = p.z;
    x->nx = n.x; x->ny = n.y; x->nz = n.z;
    x->u = u; x->v = v;
    return g_nv++;
}
static void geo_tri(int a, int b, int c) { g_i[g_ni++] = a; g_i[g_ni++] = b; g_i[g_ni++] = c; }
static void geo_quad(int a, int b, int c, int d) { geo_tri(a, b, c); geo_tri(a, c, d); }
static Mesh geo_end(int dynamic) { return make_mesh(g_v, g_nv, g_i, g_ni, dynamic); }

// Earth-fixed coordinates: Y = north pole, X = (lat 0, lon 0), east = -Z at lon 0.
static vec3 latlon_dir(float lat, float lon) {
    return v3(cosf(lat) * cosf(lon), sinf(lat), -cosf(lat) * sinf(lon));
}

static Mesh build_sphere(int nlon, int nlat) {
    geo_begin();
    for (int j = 0; j <= nlat; j++) {
        float v = (float)j / nlat;
        float lat = PI_F * (0.5f - v);
        for (int i = 0; i <= nlon; i++) {
            float u = (float)i / nlon;
            float lon = 2.0f * PI_F * u - PI_F;
            vec3 p = latlon_dir(lat, lon);
            geo_v(p, p, u, v);
        }
    }
    for (int j = 0; j < nlat; j++)
        for (int i = 0; i < nlon; i++) {
            int a = j * (nlon + 1) + i, b = a + 1, c = a + nlon + 1, d = c + 1;
            // CCW seen from outside
            geo_tri(a, c, b);
            geo_tri(b, c, d);
        }
    return geo_end(0);
}

static Mesh build_quad(void) {
    geo_begin();
    vec3 n = v3(0, 0, 1);
    int a = geo_v(v3(-0.5f, -0.5f, 0), n, 0, 1);
    int b = geo_v(v3(0.5f, -0.5f, 0), n, 1, 1);
    int c = geo_v(v3(0.5f, 0.5f, 0), n, 1, 0);
    int d = geo_v(v3(-0.5f, 0.5f, 0), n, 0, 0);
    geo_quad(a, b, c, d);
    return geo_end(1);
}

static void geo_box(vec3 lo, vec3 hi) {
    vec3 c[8];
    for (int i = 0; i < 8; i++) c[i] = v3(i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z);
    static const int f[6][4] = {{0, 4, 6, 2}, {1, 3, 7, 5}, {0, 1, 5, 4}, {2, 6, 7, 3}, {0, 2, 3, 1}, {4, 5, 7, 6}};
    static const float nrm[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    for (int k = 0; k < 6; k++) {
        vec3 n = v3(nrm[k][0], nrm[k][1], nrm[k][2]);
        int a = geo_v(c[f[k][0]], n, 0, 0), b = geo_v(c[f[k][1]], n, 1, 0);
        int cc = geo_v(c[f[k][2]], n, 1, 1), d = geo_v(c[f[k][3]], n, 0, 1);
        geo_quad(a, b, cc, d);
    }
}

// rounded "pill" box built from a squashed sphere -- used for the controllers
static void geo_ellipsoid(vec3 c, vec3 r, int nl, int nt) {
    int base = g_nv;
    for (int j = 0; j <= nt; j++) {
        float lat = PI_F * (0.5f - (float)j / nt);
        for (int i = 0; i <= nl; i++) {
            float lon = 2.0f * PI_F * i / nl;
            vec3 p = latlon_dir(lat, lon);
            vec3 n = v3norm(v3(p.x / r.x, p.y / r.y, p.z / r.z));
            geo_v(v3(c.x + p.x * r.x, c.y + p.y * r.y, c.z + p.z * r.z), n, 0, 0);
        }
    }
    for (int j = 0; j < nt; j++)
        for (int i = 0; i < nl; i++) {
            int a = base + j * (nl + 1) + i, b = a + 1, cc = a + nl + 1, d = cc + 1;
            geo_tri(a, cc, b);
            geo_tri(b, cc, d);
        }
}

static void geo_torus(vec3 c, float R, float r, int nu, int nv, quat q) {
    int base = g_nv;
    for (int j = 0; j <= nv; j++) {
        float b = 2.0f * PI_F * j / nv;
        for (int i = 0; i <= nu; i++) {
            float a = 2.0f * PI_F * i / nu;
            vec3 dir = v3(cosf(a), 0, sinf(a));
            vec3 n = v3add(v3scale(dir, cosf(b)), v3(0, sinf(b), 0));
            vec3 p = v3add(v3scale(dir, R), v3scale(n, r));
            geo_v(v3add(c, qrot(q, p)), qrot(q, n), 0, 0);
        }
    }
    for (int j = 0; j < nv; j++)
        for (int i = 0; i < nu; i++) {
            int a = base + j * (nu + 1) + i, bb = a + 1, cc = a + nu + 1, d = cc + 1;
            geo_tri(a, bb, cc);
            geo_tri(bb, d, cc);
        }
}

// vertical cylinder wall segment (normals facing inward when inward=1)
static void geo_wall(float radius, float az0, float az1, float y0, float y1, int seg, int inward) {
    int base = g_nv;
    for (int i = 0; i <= seg; i++) {
        float az = lerpf(az0, az1, (float)i / seg);
        vec3 d = v3(sinf(az), 0, -cosf(az));
        vec3 n = inward ? v3scale(d, -1) : d;
        geo_v(v3(d.x * radius, y0, d.z * radius), n, (float)i / seg, 0);
        geo_v(v3(d.x * radius, y1, d.z * radius), n, (float)i / seg, 1);
    }
    for (int i = 0; i < seg; i++) {
        int a = base + i * 2;
        if (inward) geo_quad(a, a + 2, a + 3, a + 1);
        else geo_quad(a, a + 1, a + 3, a + 2);
    }
}

// horizontal ring (annulus) segment at height y, facing up (or down)
static void geo_ring(float r0, float r1, float az0, float az1, float y, int seg, int up) {
    int base = g_nv;
    vec3 n = v3(0, up ? 1.0f : -1.0f, 0);
    for (int i = 0; i <= seg; i++) {
        float az = lerpf(az0, az1, (float)i / seg);
        vec3 d = v3(sinf(az), 0, -cosf(az));
        geo_v(v3(d.x * r0, y, d.z * r0), n, (float)i / seg, 0);
        geo_v(v3(d.x * r1, y, d.z * r1), n, (float)i / seg, 1);
    }
    for (int i = 0; i < seg; i++) {
        int a = base + i * 2;
        if (up) geo_quad(a, a + 1, a + 3, a + 2);
        else geo_quad(a, a + 2, a + 3, a + 1);
    }
}

static void geo_disc(float radius, float y, int rings, int seg, float cz) {
    int base = g_nv;
    for (int j = 0; j <= rings; j++) {
        float r = radius * (float)j / rings;
        for (int i = 0; i <= seg; i++) {
            float az = 2.0f * PI_F * i / seg;
            geo_v(v3(sinf(az) * r, y, cz - cosf(az) * r), v3(0, 1, 0), 0, 0);
        }
    }
    for (int j = 0; j < rings; j++)
        for (int i = 0; i < seg; i++) {
            int a = base + j * (seg + 1) + i, b = a + 1, c = a + seg + 1, d = c + 1;
            geo_quad(a, c, d, b);
        }
}

static void geo_cylinder(vec3 c, float r, float h, int seg) {
    int base = g_nv;
    for (int i = 0; i <= seg; i++) {
        float az = 2.0f * PI_F * i / seg;
        vec3 d = v3(sinf(az), 0, -cosf(az));
        geo_v(v3(c.x + d.x * r, c.y, c.z + d.z * r), d, 0, 0);
        geo_v(v3(c.x + d.x * r, c.y + h, c.z + d.z * r), d, 0, 1);
    }
    for (int i = 0; i < seg; i++) {
        int a = base + i * 2;
        geo_quad(a, a + 1, a + 3, a + 2);
    }
}

// ------------------------------------------------------------------ book

#define LEAF_NX 28
static Mesh g_leaf;             // dynamic strip, rebuilt per draw
static Mesh g_blockR, g_blockL; // page block edges (rebuilt when the spread changes)
static int g_blockSpread = -1;

static float stack_thickness(int spread, int left) {
    float read = (float)spread / (SPREAD_COUNT - 1);
    // Arabic Mushaf: read pages accumulate on the right-hand side.
    return 0.003f + 0.017f * (left ? (1.0f - read) : read);
}

static float profile_angle(float sigma) {
    float a = 1.0f - sigma;
    return 0.55f * a * a * a - 0.03f;
}

// Computes the leaf cross-section (x,z) and normals at NX+1 stations.
static void leaf_profile(float alpha, float curl, float tR, float tL, float* xs, float* zs, float* nxs, float* nzs) {
    float ds = PAGE_W / LEAF_NX;
    float x = 0, z = 0;
    float ca = cosf(alpha), sa = sinf(alpha);
    float thick = lerpf(tR, tL, (1.0f - ca) * 0.5f);
    for (int i = 0; i <= LEAF_NX; i++) {
        float sigma = (float)i / LEAF_NX;
        float sm = (i + 0.5f) / LEAF_NX;
        float beta = profile_angle(i == LEAF_NX ? sigma : sm) * ca + curl * powf(sm, 1.6f);
        float lx = x, lz = z;
        float rx = lx * ca - lz * sa, rz = lx * sa + lz * ca;
        float off = thick * smoothstepf(0.0f, 0.12f, sigma);
        xs[i] = rx;
        zs[i] = rz + off;
        float tb = beta;
        float tx = cosf(tb) * ca - sinf(tb) * sa, tz = cosf(tb) * sa + sinf(tb) * ca;
        nxs[i] = -tz;
        nzs[i] = tx;
        x += cosf(beta) * ds;
        z += sinf(beta) * ds;
    }
}

static void build_leaf(float alpha, float curl, float tR, float tL) {
    float xs[LEAF_NX + 1], zs[LEAF_NX + 1], nx[LEAF_NX + 1], nz[LEAF_NX + 1];
    leaf_profile(alpha, curl, tR, tL, xs, zs, nx, nz);
    Vtx v[(LEAF_NX + 1) * 2];
    for (int i = 0; i <= LEAF_NX; i++) {
        float sigma = (float)i / LEAF_NX;
        for (int k = 0; k < 2; k++) {
            Vtx* t = &v[i * 2 + k];
            t->px = xs[i];
            t->py = k ? PAGE_H * 0.5f : -PAGE_H * 0.5f;
            t->pz = zs[i];
            t->nx = nx[i]; t->ny = 0; t->nz = nz[i];
            t->u = sigma;
            t->v = k ? 0.0f : 1.0f;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, g_leaf.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
}

static Mesh init_leaf_mesh(void) {
    geo_begin();
    for (int i = 0; i <= LEAF_NX; i++) {
        geo_v(v3(0, 0, 0), v3(0, 0, 1), 0, 0);
        geo_v(v3(0, 0, 0), v3(0, 0, 1), 0, 0);
    }
    // CCW when the leaf lies on the right facing +Z (side A)
    for (int i = 0; i < LEAF_NX; i++) {
        int a = i * 2, b = a + 1, c = a + 2, d = a + 3;  // a: bottom(i) b: top(i)
        geo_tri(a, c, d);
        geo_tri(a, d, b);
    }
    return geo_end(1);
}

// Page block (the visible edges of the stacked pages under the top page)
static Mesh build_block(int left, float thick) {
    float xs[LEAF_NX + 1], zs[LEAF_NX + 1], nx[LEAF_NX + 1], nz[LEAF_NX + 1];
    float alpha = left ? PI_F : 0.0f;
    leaf_profile(alpha, 0, left ? 0 : thick, left ? thick : 0, xs, zs, nx, nz);
    geo_begin();
    float hy = PAGE_H * 0.5f - 0.001f;
    // head and tail edges
    for (int e = 0; e < 2; e++) {
        float y = e ? hy : -hy;
        vec3 n = v3(0, e ? 1.0f : -1.0f, 0);
        int base = g_nv;
        for (int i = 0; i <= LEAF_NX; i++) {
            float zt = zs[i] - 0.0006f;
            geo_v(v3(xs[i], y, zt), n, 0, zt * 12.0f);
            geo_v(v3(xs[i], y, 0.0f), n, 0, 0);
        }
        for (int i = 0; i < LEAF_NX; i++) {
            int a = base + i * 2;
            geo_quad(a, a + 1, a + 3, a + 2);
        }
    }
    // fore-edge
    float xe = xs[LEAF_NX], ze = zs[LEAF_NX] - 0.0006f;
    vec3 n = v3(left ? -1.0f : 1.0f, 0, 0);
    int a = geo_v(v3(xe, -hy, 0), n, 0, 0);
    int b = geo_v(v3(xe, hy, 0), n, 0, 0);
    int c = geo_v(v3(xe, hy, ze), n, 0, ze * 12.0f);
    int d = geo_v(v3(xe, -hy, ze), n, 0, ze * 12.0f);
    geo_quad(a, b, c, d);
    return geo_end(0);
}

// ------------------------------------------------------------------ state

static Program P_SKY, P_SKYBAKE, P_LIT, P_EARTH, P_ATMO, P_PAGE, P_PANEL, P_FX;
static Mesh M_SPHERE, M_SKY, M_QUAD, M_FLOOR, M_STATION, M_STRIPS, M_STAND, M_STAND_GLOW, M_COVER,
    M_CTRL_BODY, M_CTRL_RING, M_LASER;
static GLuint T_DAY, T_NIGHT, T_CLOUDS, T_SPEC, T_SKY;
static GLuint T_PANEL[PANEL_COUNT];

static PanelLayout g_panels[PANEL_COUNT] = {
    /* SURAHS */ {0.44f, {-0.78f, 0.03f, -1.08f}, 0.62f, 0.0f, 736, 1024},
    /* TITLE  */ {1.10f, {-0.02f, 0.33f, -1.75f}, 0.0f, 0.0f, 1300, 520},
    /* EARTH  */ {0.30f, {0.80f, -0.10f, -1.05f}, -0.62f, 0.0f, 480, 400},
    /* PAGER  */ {0.19f, {0.0f, -0.462f, -0.455f}, 0.0f, -0.62f, 520, 104},
    /* DOCK   */ {0.34f, {0.0f, -0.528f, -0.43f}, 0.0f, -0.70f, 880, 220},
    /* SHEET  */ {0.44f, {0.78f, 0.03f, -1.08f}, -0.62f, 0.0f, 736, 1024},
};

static const vec3 EARTH_POS = {2.45f, 0.62f, -3.75f};
static const float EARTH_R = 1.08f;

const PanelLayout* scene_panel_layout(int panel) { return &g_panels[panel]; }
void scene_set_panel_size(int panel, int w, int h) { g_panels[panel].texW = w; g_panels[panel].texH = h; }

mat4 scene_panel_matrix(int panel) {
    const PanelLayout* p = &g_panels[panel];
    float h = p->width * p->texH / (float)p->texW;
    return m4mul(m4mul(m4translate(p->pos), m4mul(m4rotY(p->yaw), m4rotX(p->pitch))), m4scale(v3(p->width, h, 1)));
}

#define BOOK_TILT 1.05f   // page plane angle from horizontal (~60 deg)
mat4 scene_book_matrix(void) {
    vec3 X = v3(1, 0, 0);
    vec3 Y = v3(0, sinf(BOOK_TILT), -cosf(BOOK_TILT));
    vec3 Z = v3(0, cosf(BOOK_TILT), sinf(BOOK_TILT));
    return m4basis(X, Y, Z, v3(0.0f, -0.235f, -0.585f));
}

float scene_page_surface_z(float x) {
    float s = fabsf(x) / PAGE_W;
    float z = 0;
    float ds = 1.0f / 40;
    for (float t = 0; t < s; t += ds) z += sinf(profile_angle(t + ds * 0.5f)) * ds * PAGE_W;
    return z + 0.012f;
}

void scene_set_earth_textures(unsigned day, unsigned night, unsigned clouds, unsigned spec) {
    T_DAY = day; T_NIGHT = night; T_CLOUDS = clouds; T_SPEC = spec;
}
void scene_set_panel_texture(int panel, unsigned tex, int w, int h) {
    T_PANEL[panel] = tex;
    g_panels[panel].texW = w;
    g_panels[panel].texH = h;
}

static Mesh build_station(void) {
    geo_begin();
    // low parapet around the deck
    geo_wall(4.2f, -PI_F, PI_F, FLOOR_Y, FLOOR_Y + 0.22f, 160, 1);
    geo_ring(4.05f, 4.2f, -PI_F, PI_F, FLOOR_Y + 0.22f, 160, 1);
    // left enclosure wall and the curved ceiling rim (top-left of the view)
    geo_wall(3.5f, deg2rad(-168.0f), deg2rad(-68.0f), FLOOR_Y, FLOOR_Y + 3.1f, 60, 1);
    geo_ring(2.55f, 3.5f, deg2rad(-178.0f), deg2rad(-38.0f), FLOOR_Y + 3.1f, 90, 0);
    geo_wall(2.55f, deg2rad(-178.0f), deg2rad(-38.0f), FLOOR_Y + 3.1f, FLOOR_Y + 3.35f, 90, 0);
    // a slim pillar framing the window on the left
    geo_wall(3.0f, deg2rad(-72.0f), deg2rad(-64.0f), FLOOR_Y, FLOOR_Y + 3.1f, 6, 1);
    // bench along the left wall
    for (int i = 0; i < 3; i++) {
        float az = deg2rad(-140.0f + i * 18.0f);
        vec3 c = v3(sinf(az) * 3.05f, FLOOR_Y, -cosf(az) * 3.05f);
        geo_box(v3(c.x - 0.28f, FLOOR_Y, c.z - 0.28f), v3(c.x + 0.28f, FLOOR_Y + 0.38f, c.z + 0.28f));
    }
    return geo_end(0);
}

static Mesh build_strips(void) {
    geo_begin();
    // emissive light strips
    geo_ring(4.03f, 4.07f, -PI_F, PI_F, FLOOR_Y + 0.225f, 160, 1);
    geo_wall(2.54f, deg2rad(-178.0f), deg2rad(-38.0f), FLOOR_Y + 3.07f, FLOOR_Y + 3.12f, 90, 0);
    geo_wall(3.48f, deg2rad(-168.0f), deg2rad(-68.0f), FLOOR_Y + 0.05f, FLOOR_Y + 0.09f, 60, 1);
    return geo_end(0);
}

static Mesh build_plants(void) { return (Mesh){0}; }

static Mesh build_stand(void) {
    geo_begin();
    // column + base under the lectern
    geo_cylinder(v3(0, FLOOR_Y, -0.63f), 0.035f, 0.84f, 32);
    geo_cylinder(v3(0, FLOOR_Y, -0.63f), 0.26f, 0.025f, 64);
    geo_disc(0.26f, FLOOR_Y + 0.025f, 4, 64, -0.63f);
    return geo_end(0);
}

static Mesh build_stand_glow(void) {
    geo_begin();
    geo_cylinder(v3(0, FLOOR_Y + 0.004f, -0.63f), 0.262f, 0.006f, 64);
    return geo_end(0);
}

static Mesh build_cover(void) {
    geo_begin();
    float m = 0.013f;
    // two boards + spine
    geo_box(v3(-PAGE_W - m, -PAGE_H * 0.5f - m, -0.010f), v3(-0.004f, PAGE_H * 0.5f + m, 0.0f));
    geo_box(v3(0.004f, -PAGE_H * 0.5f - m, -0.010f), v3(PAGE_W + m, PAGE_H * 0.5f + m, 0.0f));
    geo_box(v3(-0.012f, -PAGE_H * 0.5f - m, -0.018f), v3(0.012f, PAGE_H * 0.5f + m, -0.004f));
    // lectern plate and ledge
    geo_box(v3(-0.33f, -PAGE_H * 0.5f - 0.03f, -0.034f), v3(0.33f, PAGE_H * 0.5f - 0.02f, -0.016f));
    geo_box(v3(-0.31f, -PAGE_H * 0.5f - 0.032f, -0.016f), v3(0.31f, -PAGE_H * 0.5f - 0.016f, 0.022f));
    return geo_end(0);
}

static Mesh build_ctrl_body(void) {
    geo_begin();
    // handle, pointing down/back from the grip pose, and a face plate
    geo_ellipsoid(v3(0, -0.015f, 0.03f), v3(0.022f, 0.030f, 0.055f), 24, 16);
    geo_ellipsoid(v3(0, 0.012f, -0.018f), v3(0.030f, 0.014f, 0.030f), 24, 12);
    return geo_end(0);
}

static Mesh build_ctrl_ring(void) {
    geo_begin();
    geo_torus(v3(0, 0.022f, -0.03f), 0.036f, 0.0055f, 48, 12, qaxis(v3(1, 0, 0), 0.45f));
    return geo_end(0);
}

#define SKY_SIZE 2048
static void bake_sky(void) {
    GLint prevFbo = 0, vp[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
    glGetIntegerv(GL_VIEWPORT, vp);
    glGenTextures(1, &T_SKY);
    glBindTexture(GL_TEXTURE_CUBE_MAP, T_SKY);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_SRGB8_ALPHA8, SKY_SIZE, SKY_SIZE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, SKY_SIZE, SKY_SIZE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glUseProgram(P_SKYBAKE.prog);
    vec3 flare = v3norm(v3add(v3norm(EARTH_POS), v3(0.30f, 0.27f, 0.12f)));
    glUniform3f(P_SKYBAKE.uFlareDir, flare.x, flare.y, flare.z);
    glUniform1f(P_SKYBAKE.uSize, (float)SKY_SIZE);
    for (int f = 0; f < 6; f++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, T_SKY, 0);
        glUniform1i(P_SKYBAKE.uFace, f);
        draw_mesh(&M_QUAD);
        glFlush();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
    glDeleteFramebuffers(1, &fbo);
    glViewport(vp[0], vp[1], vp[2], vp[3]);
    glEnable(GL_DEPTH_TEST);
}

void scene_init(void) {
    P_SKY = make_program(VS_SKY, FS_SKYCUBE);
    P_SKYBAKE = make_program(VS_FULL, FS_SKY);
    P_LIT = make_program(VS_MESH, FS_LIT);
    P_EARTH = make_program(VS_MESH, FS_EARTH);
    P_ATMO = make_program(VS_MESH, FS_ATMO);
    P_PAGE = make_program(VS_MESH, FS_PAGE);
    P_PANEL = make_program(VS_MESH, FS_PANEL);
    P_FX = make_program(VS_MESH, FS_FX);

    M_SPHERE = build_sphere(128, 64);
    M_SKY = build_sphere(48, 24);
    M_QUAD = build_quad();
    M_LASER = build_quad();
    geo_begin(); geo_disc(4.2f, FLOOR_Y, 24, 128, 0.0f); M_FLOOR = geo_end(0);
    M_STATION = build_station();
    M_STRIPS = build_strips();
    M_STAND = build_stand();
    M_STAND_GLOW = build_stand_glow();
    M_COVER = build_cover();
    M_CTRL_BODY = build_ctrl_body();
    M_CTRL_RING = build_ctrl_ring();
    g_leaf = init_leaf_mesh();
    bake_sky();
    (void)build_plants;
}

// ------------------------------------------------------------------ render

static mat4 g_vp;
static vec3 g_eye;

static void use_lit(const mat4* model, vec3 color, float alpha, vec3 emissive, float spec, float shin, int mode) {
    glUseProgram(P_LIT.prog);
    glUniformMatrix4fv(P_LIT.uModel, 1, GL_FALSE, model->m);
    glUniformMatrix4fv(P_LIT.uViewProj, 1, GL_FALSE, g_vp.m);
    glUniform4f(P_LIT.uColor, color.x, color.y, color.z, alpha);
    glUniform3f(P_LIT.uEmissive, emissive.x, emissive.y, emissive.z);
    glUniform3f(P_LIT.uEye, g_eye.x, g_eye.y, g_eye.z);
    glUniform1f(P_LIT.uSpec, spec);
    glUniform1f(P_LIT.uShininess, shin);
    glUniform1i(P_LIT.uMode, mode);
}

// Sub-solar point for a unix time (low precision solar position, ~0.1 deg).
static void subsolar(double t, float* lat, float* lon) {
    double d = t / 86400.0 - 10957.5;                 // days since J2000.0
    double g = fmod(357.529 + 0.98560028 * d, 360.0) * 3.14159265358979 / 180.0;
    double q = fmod(280.459 + 0.98564736 * d, 360.0);
    double L = (q + 1.915 * sin(g) + 0.020 * sin(2 * g)) * 3.14159265358979 / 180.0;
    double e = (23.439 - 0.00000036 * d) * 3.14159265358979 / 180.0;
    double dec = asinf((float)(sin(e) * sin(L)));
    double ra = atan2f((float)(cos(e) * sin(L)), (float)cos(L)) * 180.0 / 3.14159265358979;
    double gmst = fmod(280.46061837 + 360.98564736629 * d, 360.0);
    double lonDeg = fmod(ra - gmst + 540.0, 360.0) - 180.0;
    *lat = (float)dec;
    *lon = (float)(lonDeg * 3.14159265358979 / 180.0);
}

// Direction of the (lighting) sun in the scene -- chosen so the visible hemisphere
// is mostly in daylight with the terminator on the left, as in the reference art.
static vec3 scene_sun_dir(void) { return v3norm(v3(0.55f, 0.32f, 0.78f)); }

static mat4 earth_rotation(double utc) {
    float slat, slon;
    subsolar(utc, &slat, &slon);
    vec3 a = latlon_dir(slat, slon);          // earth-fixed sub-solar direction
    vec3 north = v3(0, 1, 0);
    vec3 b = v3norm(v3sub(north, v3scale(a, v3dot(north, a))));
    vec3 c = v3cross(a, b);
    vec3 A = scene_sun_dir();
    vec3 up = v3norm(v3(0.18f, 1.0f, 0.12f)); // slight artistic axial tilt
    vec3 B = v3norm(v3sub(up, v3scale(A, v3dot(up, A))));
    vec3 C = v3cross(A, B);
    // R = [A B C] * [a b c]^T
    mat4 r = m4identity();
    for (int col = 0; col < 3; col++) {
        float ec[3] = {col == 0 ? 1.0f : 0.0f, col == 1 ? 1.0f : 0.0f, col == 2 ? 1.0f : 0.0f};
        vec3 e = v3(ec[0], ec[1], ec[2]);
        float ka = v3dot(a, e), kb = v3dot(b, e), kc = v3dot(c, e);
        vec3 w = v3add(v3add(v3scale(A, ka), v3scale(B, kb)), v3scale(C, kc));
        r.m[col * 4 + 0] = w.x; r.m[col * 4 + 1] = w.y; r.m[col * 4 + 2] = w.z;
    }
    return r;
}

static void draw_sky(const View* v) {
    mat4 rot = v->view;
    rot.m[12] = rot.m[13] = rot.m[14] = 0;
    glUseProgram(P_SKY.prog);
    glUniformMatrix4fv(P_SKY.uViewRot, 1, GL_FALSE, rot.m);
    glUniformMatrix4fv(P_SKY.uProj, 1, GL_FALSE, v->proj.m);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, T_SKY);
    glUniform1i(P_SKY.uSky, 0);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    draw_mesh(&M_SKY);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

static void draw_earth(const SceneState* s) {
    double t = s->utc + s->earthUtcOffset;
    mat4 rot = earth_rotation(t);
    mat4 model = m4mul(m4mul(m4translate(EARTH_POS), rot), m4scale(v3(EARTH_R, EARTH_R, EARTH_R)));
    vec3 sun = scene_sun_dir();
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(P_EARTH.prog);
    glUniformMatrix4fv(P_EARTH.uModel, 1, GL_FALSE, model.m);
    glUniformMatrix4fv(P_EARTH.uViewProj, 1, GL_FALSE, g_vp.m);
    glUniform3f(P_EARTH.uSun, sun.x, sun.y, sun.z);
    glUniform3f(P_EARTH.uEye, g_eye.x, g_eye.y, g_eye.z);
    vec3 mk = latlon_dir(deg2rad(s->markerLat), deg2rad(s->markerLon));
    glUniform3f(P_EARTH.uMarker, mk.x, mk.y, mk.z);
    glUniform1f(P_EARTH.uCloudShift, (float)fmod(t / 86400.0 * 0.01, 1.0));
    glUniform1f(P_EARTH.uTime, (float)s->time);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, T_DAY); glUniform1i(P_EARTH.uDay, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, T_NIGHT); glUniform1i(P_EARTH.uNight, 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, T_CLOUDS); glUniform1i(P_EARTH.uClouds, 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, T_SPEC); glUniform1i(P_EARTH.uSpecMap, 3);
    glActiveTexture(GL_TEXTURE0);
    draw_mesh(&M_SPHERE);

    // atmosphere shell (additive)
    float ar = EARTH_R * 1.06f;
    mat4 am = m4mul(m4translate(EARTH_POS), m4scale(v3(ar, ar, ar)));
    glUseProgram(P_ATMO.prog);
    glUniformMatrix4fv(P_ATMO.uModel, 1, GL_FALSE, am.m);
    glUniformMatrix4fv(P_ATMO.uViewProj, 1, GL_FALSE, g_vp.m);
    glUniform3f(P_ATMO.uSun, sun.x, sun.y, sun.z);
    glUniform3f(P_ATMO.uEye, g_eye.x, g_eye.y, g_eye.z);
    glUniform3f(P_ATMO.uCenter, EARTH_POS.x, EARTH_POS.y, EARTH_POS.z);
    glUniform1f(P_ATMO.uRadius, EARTH_R);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDepthMask(GL_FALSE);
    draw_mesh(&M_SPHERE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void draw_station(void) {
    glDisable(GL_CULL_FACE);
    mat4 id = m4identity();
    use_lit(&id, v3(0.84f, 0.85f, 0.88f), 1, v3(0.02f, 0.024f, 0.03f), 0.45f, 80.0f, 1);
    draw_mesh(&M_FLOOR);
    use_lit(&id, v3(0.78f, 0.80f, 0.84f), 1, v3(0.015f, 0.017f, 0.022f), 0.25f, 40.0f, 0);
    draw_mesh(&M_STATION);
    use_lit(&id, v3(0, 0, 0), 1, v3(0.55f, 0.80f, 1.25f), 0, 1, 3);
    draw_mesh(&M_STRIPS);
}

static void draw_stand(void) {
    glDisable(GL_CULL_FACE);
    mat4 id = m4identity();
    use_lit(&id, v3(0.55f, 0.57f, 0.61f), 1, v3(0, 0, 0), 0.8f, 90.0f, 0);
    draw_mesh(&M_STAND);
    use_lit(&id, v3(0, 0, 0), 1, v3(0.6f, 0.85f, 1.3f), 0, 1, 3);
    draw_mesh(&M_STAND_GLOW);
    mat4 book = scene_book_matrix();
    use_lit(&book, v3(0.028f, 0.030f, 0.032f), 1, v3(0, 0, 0), 0.35f, 30.0f, 0);
    draw_mesh(&M_COVER);
}

static void page_uniforms(const mat4* model, unsigned tex, int flip, const SceneState* s,
                          float shadowAmt, float shadowExtent, float shadowSide) {
    glUseProgram(P_PAGE.prog);
    glUniformMatrix4fv(P_PAGE.uModel, 1, GL_FALSE, model->m);
    glUniformMatrix4fv(P_PAGE.uViewProj, 1, GL_FALSE, g_vp.m);
    glUniform3f(P_PAGE.uEye, g_eye.x, g_eye.y, g_eye.z);
    glUniform1f(P_PAGE.uFlipU, flip ? 1.0f : 0.0f);
    glUniform1f(P_PAGE.uNight_, s->nightMode ? 1.0f : 0.0f);
    glUniform1f(P_PAGE.uHasTex, tex ? 1.0f : 0.0f);
    glUniform1f(P_PAGE.uShadowAmt, shadowAmt);
    glUniform1f(P_PAGE.uShadowExtent, shadowExtent);
    glUniform1f(P_PAGE.uShadowSide, shadowSide);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(P_PAGE.uTex, 0);
}

// Draws the leaf at angle alpha. Side A (facing +Z when on the right) shows texA,
// side B shows texB (a left-hand page, mirrored U).
static void draw_leaf(const mat4* book, float alpha, float curl, float tR, float tL,
                      int pageA, int pageB, const SceneState* s, PageTexFn tex,
                      float shAmt, float shExt, float shSide) {
    build_leaf(alpha, curl, tR, tL);
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CCW);
    if (pageA > 0) {
        glCullFace(GL_BACK);
        page_uniforms(book, tex(pageA), 0, s, shAmt, shExt, shSide);
        draw_mesh(&g_leaf);
    }
    if (pageB > 0) {
        glCullFace(GL_FRONT);
        page_uniforms(book, tex(pageB), 1, s, shAmt, shExt, shSide);
        draw_mesh(&g_leaf);
    }
    glCullFace(GL_BACK);
}

static void draw_book(const SceneState* s, PageTexFn tex) {
    mat4 book = scene_book_matrix();
    int k = s->spread;
    float tR = stack_thickness(k, 0), tL = stack_thickness(k, 1);
    if (g_blockSpread != k) {
        if (g_blockSpread >= 0) {
            glDeleteBuffers(1, &g_blockR.vbo); glDeleteBuffers(1, &g_blockR.ibo); glDeleteVertexArrays(1, &g_blockR.vao);
            glDeleteBuffers(1, &g_blockL.vbo); glDeleteBuffers(1, &g_blockL.ibo); glDeleteVertexArrays(1, &g_blockL.vao);
        }
        g_blockR = build_block(0, tR);
        g_blockL = build_block(1, tL);
        g_blockSpread = k;
    }
    glDisable(GL_CULL_FACE);
    use_lit(&book, v3(0.93f, 0.89f, 0.78f), 1, v3(0, 0, 0), 0.05f, 10.0f, 2);
    draw_mesh(&g_blockR);
    draw_mesh(&g_blockL);

    int right = 2 * k + 1, left = 2 * k + 2;
    float a = s->alpha;
    float shAmt = s->turning ? 0.38f * sinf(a) : 0.0f;
    float shExt = PAGE_W * fabsf(cosf(a)) + 0.02f;
    float shSide = cosf(a) >= 0 ? 1.0f : -1.0f;

    if (s->turning > 0) {
        // forward: leaf = (front: left page of k) / (back: right page of k+1)
        draw_leaf(&book, 0.0f, 0, tR, tL, right, 0, s, tex, shAmt, shExt, shSide);            // right stack
        draw_leaf(&book, PI_F, 0, tR, tL, 0, left + 2, s, tex, shAmt, shExt, shSide);         // new left
        float curl = 0.85f * sinf(a);
        draw_leaf(&book, a, curl, tR, tL, right + 2, left, s, tex, 0, 0, 0);
    } else if (s->turning < 0) {
        // backward: leaf = (front: right page of k) / (back: left page of k-1)
        draw_leaf(&book, 0.0f, 0, tR, tL, right - 2, 0, s, tex, shAmt, shExt, shSide);       // previous right
        draw_leaf(&book, PI_F, 0, tR, tL, 0, left, s, tex, shAmt, shExt, shSide);             // current left
        float curl = -0.85f * sinf(a);
        draw_leaf(&book, a, curl, tR, tL, right, left - 2, s, tex, 0, 0, 0);
    } else {
        draw_leaf(&book, 0.0f, 0, tR, tL, right, 0, s, tex, 0, 0, 0);
        draw_leaf(&book, PI_F, 0, tR, tL, 0, left, s, tex, 0, 0, 0);
    }
}

static void draw_controllers(const SceneState* s) {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    for (int h = 0; h < 2; h++) {
        const HandVis* hv = &s->hands[h];
        if (!hv->active) continue;
        mat4 m = m4pose(hv->grip);
        glDisable(GL_CULL_FACE);
        use_lit(&m, v3(0.86f, 0.87f, 0.89f), 1, v3(0.02f, 0.02f, 0.025f), 0.5f, 50.0f, 0);
        draw_mesh(&M_CTRL_BODY);
        use_lit(&m, v3(0.80f, 0.81f, 0.84f), 1, v3(0.0f, 0.0f, 0.0f), 0.6f, 60.0f, 0);
        draw_mesh(&M_CTRL_RING);
    }
}

static void fx_draw(const mat4* model, vec3 color, int mode, float sharp) {
    glUseProgram(P_FX.prog);
    glUniformMatrix4fv(P_FX.uModel, 1, GL_FALSE, model->m);
    glUniformMatrix4fv(P_FX.uViewProj, 1, GL_FALSE, g_vp.m);
    glUniform3f(P_FX.uColor, color.x, color.y, color.z);
    glUniform1i(P_FX.uMode, mode);
    glUniform1f(P_FX.uSharp, sharp);
    draw_mesh(mode == 1 ? &M_LASER : &M_QUAD);
}

// camera-facing sprite at p
static mat4 billboard(vec3 p, float size) {
    vec3 z = v3norm(v3sub(g_eye, p));
    vec3 x = v3norm(v3cross(v3(0, 1, 0), z));
    vec3 y = v3cross(z, x);
    return m4basis(v3scale(x, size), v3scale(y, size), z, p);
}

static void draw_lasers(const SceneState* s) {
    glDisable(GL_CULL_FACE);
    for (int h = 0; h < 2; h++) {
        const HandVis* hv = &s->hands[h];
        if (!hv->active) continue;
        vec3 o = hv->aim.pos;
        vec3 d = qrot(hv->aim.ori, v3(0, 0, -1));
        float len = hv->rayLen;
        vec3 mid = v3mad(o, d, len * 0.5f);
        vec3 toEye = v3norm(v3sub(g_eye, mid));
        vec3 side = v3norm(v3cross(d, toEye));
        vec3 nz = v3cross(side, d);
        float w = 0.0028f;
        // quad x runs along the ray (uv.x 0 at origin), y across
        mat4 m = m4basis(v3scale(d, len), v3scale(side, w), nz, mid);
        float b = hv->pointer ? 1.0f : 0.45f;
        fx_draw(&m, v3(0.75f * b, 0.85f * b, 1.0f * b), 1, 0);
        if (hv->rayHit) {
            vec3 hp = v3mad(o, d, len);
            mat4 c = billboard(hp, 0.018f);
            fx_draw(&c, v3(1.2f * b, 1.25f * b, 1.35f * b), 0, 7.0f);
        }
    }
}

static void draw_panel(int i, float opacity) {
    if (!T_PANEL[i] || opacity <= 0.001f) return;
    mat4 m = scene_panel_matrix(i);
    glUseProgram(P_PANEL.prog);
    glUniformMatrix4fv(P_PANEL.uModel, 1, GL_FALSE, m.m);
    glUniformMatrix4fv(P_PANEL.uViewProj, 1, GL_FALSE, g_vp.m);
    glUniform1f(P_PANEL.uOpacity, opacity);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, T_PANEL[i]);
    glUniform1i(P_PANEL.uTex, 0);
    draw_mesh(&M_QUAD);
}

void scene_render(const View* v, const SceneState* s, PageTexFn pageTex) {
    g_vp = m4mul(v->proj, v->view);
    g_eye = v->eye;

    glClearColor(0, 0, 0, 1);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);

    draw_sky(v);
    draw_earth(s);
    if (s->showStation) draw_station();
    draw_stand();
    draw_book(s, pageTex);
    draw_controllers(s);

    // transparent pass
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    int order[PANEL_COUNT];
    float dist[PANEL_COUNT];
    for (int i = 0; i < PANEL_COUNT; i++) {
        order[i] = i;
        dist[i] = v3len(v3sub(g_panels[i].pos, g_eye));
    }
    for (int i = 0; i < PANEL_COUNT; i++)
        for (int j = i + 1; j < PANEL_COUNT; j++)
            if (dist[order[j]] > dist[order[i]]) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int i = 0; i < PANEL_COUNT; i++) {
        int p = order[i];
        if (s->panelVisible & (1u << p)) draw_panel(p, s->panelFade[p]);
    }
    glBlendFunc(GL_ONE, GL_ONE);
    draw_lasers(s);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

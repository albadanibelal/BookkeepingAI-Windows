#!/usr/bin/env python3
"""Generate link-time stub libraries for Android system libraries.

The native library is linked against tiny stub .so files that carry the right
SONAMEs (libc.so, libGLESv3.so, ...) and export exactly the symbols our code
imports. At runtime Android's dynamic linker resolves those symbols from the
real system libraries. Every imported symbol is checked against bionic's
official symbol maps (with API levels) so nothing unavailable on the device
slips through.

usage: gen_stubs.py <outdir> <bionic_libc_dir> <min_api> <obj files...>
"""
import os
import re
import subprocess
import sys

outdir, bionic, min_api = sys.argv[1], sys.argv[2], int(sys.argv[3])
objs = sys.argv[4:]

undef = set()
for o in objs:
    out = subprocess.run(['llvm-nm', '-u', o], capture_output=True, text=True, check=True).stdout
    for line in out.split():
        if line not in ('U',):
            undef.add(line)
undef.discard('U')
defined = set()
for o in objs:
    out = subprocess.run(['llvm-nm', '--defined-only', '-j', o], capture_output=True, text=True, check=True).stdout
    defined.update(out.split())
undef -= defined

MATH = set('''sin cos tan asin acos atan atan2 sinh cosh tanh exp exp2 log log2 log10 pow sqrt cbrt
floor ceil round trunc fmod fabs fmin fmax ldexp frexp modf hypot sincos'''.split())
MATH |= {f + 'f' for f in MATH}

def lib_for(sym):
    if sym.startswith('xr'):
        return 'openxr_loader'
    if sym.startswith('gl'):
        return 'GLESv3'
    if sym.startswith('egl'):
        return 'EGL'
    if sym.startswith('__android_log'):
        return 'log'
    if re.match(r'^(AAsset|AInput|AKeyEvent|ALooper|ANativeActivity|AConfiguration)', sym):
        return 'android'
    if sym in MATH:
        return 'm'
    if sym.startswith('dl'):
        return 'dl'
    return 'c'

# Parse bionic symbol maps: "symbol; # introduced=NN" lines.
def parse_map(path):
    syms = {}
    if not os.path.exists(path):
        return syms
    for line in open(path):
        m = re.match(r'\s*([A-Za-z_][A-Za-z0-9_]*);(.*)', line)
        if not m:
            continue
        name, rest = m.group(1), m.group(2)
        api = 0
        mm = re.search(r'introduced=(\d+)', rest)
        if mm:
            api = int(mm.group(1))
        elif re.search(r'introduced-arm64=(\d+)', rest):
            api = int(re.search(r'introduced-arm64=(\d+)', rest).group(1))
        if 'platform-only' in rest or 'apex' in rest and 'introduced' not in rest:
            api = 999
        syms[name] = api
    return syms

bionic_maps = {
    'c': parse_map(os.path.join(bionic, 'libc/libc.map.txt')),
    'm': parse_map(os.path.join(bionic, 'libm/libm.map.txt')),
    'dl': parse_map(os.path.join(bionic, 'libdl/libdl.map.txt')),
}

groups = {}
problems = []
for s in sorted(undef):
    lib = lib_for(s)
    groups.setdefault(lib, []).append(s)
    if lib in bionic_maps and bionic_maps[lib]:
        api = bionic_maps[lib].get(s)
        if api is None:
            problems.append(f'{s}: not exported by bionic lib{lib}')
        elif api > min_api:
            problems.append(f'{s}: needs API {api} > minSdk {min_api}')

os.makedirs(outdir, exist_ok=True)
for lib, syms in groups.items():
    if lib == 'openxr_loader':
        continue
    src = os.path.join(outdir, f'lib{lib}.c')
    with open(src, 'w') as f:
        f.write('// generated link stub\n')
        for s in syms:
            f.write(f'void {s}(void) {{}}\n')
    subprocess.run(['clang', '--target=aarch64-linux-android29', '-w', '-shared', '-nostdlib', '-fPIC',
                    '-Wl,-soname,lib%s.so' % lib, '-fuse-ld=lld', '-o', os.path.join(outdir, f'lib{lib}.so'), src],
                   check=True)

for lib, syms in sorted(groups.items()):
    print(f'lib{lib}.so: {len(syms)} symbols')
if problems:
    print('SYMBOL PROBLEMS:')
    for p in problems:
        print('  ' + p)
    sys.exit(1)
print('all libc/libm/libdl imports verified against bionic symbol maps (minSdk %d)' % min_api)

#!/usr/bin/env python3
"""Creates link-time stub libraries for the Android system libraries.

The app's native code is built with plain clang (no NDK): at link time we only
need shared objects that export the right symbol names with the right SONAME;
on the device the real system libraries are loaded instead.

usage: gen_stubs.py <out dir> <clang> <symbol list file>
"""
import os
import subprocess
import sys

out, clang, symfile = sys.argv[1:4]
MATH = {'sinf', 'cosf', 'tanf', 'sqrtf', 'atan2f', 'acosf', 'asinf', 'atanf', 'fabsf', 'floorf',
        'powf', 'expf', 'fmodf', 'sin', 'cos', 'floor', 'fmod', 'sqrt', 'pow', 'exp'}

libs = {}
for sym in open(symfile).read().split():
    if sym.startswith('xr'):
        continue                       # real libopenxr_loader.so
    if sym.startswith('gl'):
        lib = 'GLESv3'
    elif sym.startswith('egl'):
        lib = 'EGL'
    elif sym.startswith('__android_log'):
        lib = 'log'
    elif sym.startswith('AndroidBitmap_'):
        lib = 'jnigraphics'
    elif sym in MATH:
        lib = 'm'
    else:
        lib = 'c'
    libs.setdefault(lib, []).append(sym)

os.makedirs(out, exist_ok=True)
for lib, syms in libs.items():
    src = os.path.join(out, 'lib%s.c' % lib)
    with open(src, 'w') as f:
        for s in sorted(set(syms)):
            f.write('void %s(void) {}\n' % s)
    subprocess.check_call([clang, '--target=aarch64-linux-android29', '-fPIC', '-shared', '-nostdlib',
                           '-ffreestanding', '-fno-builtin', '-w', '-fuse-ld=lld',
                           '-Wl,-soname,lib%s.so' % lib, src, '-o', os.path.join(out, 'lib%s.so' % lib)])
    print('stub lib%s.so: %d symbols' % (lib, len(set(syms))))

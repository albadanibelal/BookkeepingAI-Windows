#!/usr/bin/env bash
# Builds QuranVR.apk for Meta Quest without Android Studio, Gradle or the NDK.
#
# Requirements (Ubuntu 24.04):
#   apt install clang lld openjdk-17-jdk-headless aapt dalvik-exchange zipalign apksigner \
#               android-sdk-platform-23 python3-pip curl
#   pip install pillow fonttools brotli
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
DEPS="$BUILD/deps"
OUT="$ROOT/dist"
ANDROID_JAR="${ANDROID_JAR:-/usr/lib/android-sdk/platforms/android-23/android.jar}"
JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(which javac)")")")}"
CLANG="${CLANG:-clang}"
LLVM_NM="${LLVM_NM:-$(ls /usr/lib/llvm-*/bin/llvm-nm 2>/dev/null | tail -1)}"

mkdir -p "$BUILD" "$DEPS" "$OUT"

# ------------------------------------------------------------------ dependencies
npm_fetch() {  # name version dir
    local dir="$DEPS/$3"
    [ -d "$dir/package" ] && return
    mkdir -p "$dir"
    local base="${1##*/}"
    curl -sSfL --retry 6 --retry-all-errors --retry-delay 5 "https://registry.npmjs.org/$1/-/$base-$2.tgz" | tar xz -C "$dir"
}
npm_fetch quran-qcf4 1.1.0 qcf4                        # King Fahd Complex QCF4 glyph data + fonts
npm_fetch inter-ui 4.1.1 inter                         # Inter (SF-style UI font), OFL
npm_fetch @fontsource/amiri 5.3.0 amiri                # Amiri (Arabic UI font), OFL
npm_fetch globe-threejs 3.0.1 globe                   # 4K day / night-lights Earth textures (MIT)
npm_fetch earth-visualization 1.0.1 earthvis          # clouds + ocean mask (MIT)
if [ ! -f "$DEPS/openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so" ]; then
    curl -sSfL --retry 6 --retry-all-errors --retry-delay 5 -o "$DEPS/openxr.aar" \
        https://repo1.maven.org/maven2/org/khronos/openxr/openxr_loader_for_android/1.1.63/openxr_loader_for_android-1.1.63.aar
    mkdir -p "$DEPS/openxr" && (cd "$DEPS/openxr" && unzip -oq ../openxr.aar)
fi

# ------------------------------------------------------------------ assets
STAGE="$BUILD/apk"
rm -rf "$STAGE" && mkdir -p "$STAGE/assets" "$STAGE/lib/arm64-v8a" "$BUILD/res/drawable"
if [ ! -f "$BUILD/pages/604.webp" ]; then
    mkdir -p "$BUILD/ttf"
    python3 - "$DEPS/qcf4/package/fonts-woff2" "$BUILD/ttf" <<'EOF'
import glob, os, sys
from fontTools.ttLib import TTFont
for f in glob.glob(os.path.join(sys.argv[1], '*.woff2')):
    t = TTFont(f); t.flavor = None
    t.save(os.path.join(sys.argv[2], os.path.basename(f)[:-6] + '.ttf'))
EOF
fi
python3 "$ROOT/tools/prepare_assets.py" "$DEPS/qcf4/package" "$DEPS/inter/package" "$DEPS/amiri/package" \
    "$DEPS/globe/package" "$DEPS/earthvis/package" "$STAGE/assets" "$ROOT/android/java/com/quranvr/mushaf/ui"
if [ ! -f "$BUILD/pages/604.webp" ]; then
    AMIRI_TTF="$STAGE/assets/fonts/Amiri-Bold.ttf" python3 "$ROOT/tools/render_pages.py" \
        "$DEPS/qcf4/package" "$BUILD/ttf" "$BUILD/pages"
fi
cp -r "$BUILD/pages" "$STAGE/assets/pages"
python3 "$ROOT/tools/make_icon.py" "$BUILD/res/drawable/icon.png"

# ------------------------------------------------------------------ native
NATIVE="$BUILD/native" && mkdir -p "$NATIVE/stubinc"
: > "$NATIVE/stubinc/stdio.h"   # jni.h includes <stdio.h>; nothing from it is used
CFLAGS=(--target=aarch64-linux-android29 -fPIC -O2 -ffreestanding -nostdlibinc -fno-stack-protector
        -fvisibility=hidden -Wall -Wno-unused-function
        -I"$ROOT/native" -I"$ROOT/native/third_party/include" -I"$NATIVE/stubinc"
        -I"$JAVA_HOME/include" -I"$JAVA_HOME/include/linux")
OBJS=()
for src in scene app xr_main; do
    "$CLANG" "${CFLAGS[@]}" -c "$ROOT/native/$src.c" -o "$NATIVE/$src.o"
    OBJS+=("$NATIVE/$src.o")
done
"$LLVM_NM" -u "${OBJS[@]}" | awk 'NF==2 && $1=="U"{print $2} NF==1{print $1}' | grep -v ':$' | sort -u > "$NATIVE/undef.txt"
python3 "$ROOT/tools/gen_stubs.py" "$NATIVE/stubs" "$CLANG" "$NATIVE/undef.txt"
LOADER="$DEPS/openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so"
cp "$LOADER" "$NATIVE/stubs/"
"$CLANG" --target=aarch64-linux-android29 -shared -nostdlib -fuse-ld=lld "${OBJS[@]}" \
    -L"$NATIVE/stubs" -lopenxr_loader -lGLESv3 -lEGL -ljnigraphics -llog -lm -lc \
    -Wl,-soname,libquranvr.so -Wl,--no-undefined -Wl,-z,max-page-size=16384 -Wl,--hash-style=both \
    -Wl,--build-id=sha1 -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -s \
    -o "$STAGE/lib/arm64-v8a/libquranvr.so"
cp "$LOADER" "$STAGE/lib/arm64-v8a/"

# ------------------------------------------------------------------ java
CLS="$BUILD/classes" && rm -rf "$CLS" && mkdir -p "$CLS"
javac -source 8 -target 8 -Xlint:-options -nowarn -encoding UTF-8 -bootclasspath "$ANDROID_JAR" -d "$CLS" \
    $(find "$ROOT/android/java" -name '*.java')
dalvik-exchange --dex --min-sdk-version=26 --output="$STAGE/classes.dex" "$CLS"

# ------------------------------------------------------------------ package + sign
UNSIGNED="$BUILD/QuranVR-unsigned.apk"
rm -f "$UNSIGNED"
aapt package -f -M "$ROOT/android/AndroidManifest.xml" -S "$BUILD/res" -I "$ANDROID_JAR" \
    -A "$STAGE/assets" -0 webp -0 jpg -F "$UNSIGNED"
(cd "$STAGE" && aapt add "$UNSIGNED" classes.dex lib/arm64-v8a/libquranvr.so lib/arm64-v8a/libopenxr_loader.so >/dev/null)
zipalign -f -p 4 "$UNSIGNED" "$BUILD/QuranVR-aligned.apk"

KEYSTORE="${KEYSTORE:-$ROOT/keystore/quranvr-debug.jks}"
if [ ! -f "$KEYSTORE" ]; then
    mkdir -p "$(dirname "$KEYSTORE")"
    keytool -genkeypair -keystore "$KEYSTORE" -storepass android -keypass android -alias quranvr \
        -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Quran VR, O=Quran VR, C=SA" 2>/dev/null
fi
apksigner sign --ks "$KEYSTORE" --ks-pass pass:android --key-pass pass:android --ks-key-alias quranvr \
    --min-sdk-version 29 --v2-signing-enabled true --v3-signing-enabled true --out "$OUT/QuranVR.apk" "$BUILD/QuranVR-aligned.apk"
apksigner verify "$OUT/QuranVR.apk"
ls -lh "$OUT/QuranVR.apk"

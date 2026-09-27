#!/bin/bash
# Builds the Meta Quest APK (arm64-v8a, pure native NativeActivity + OpenXR).
#
# Toolchain: clang/lld (aarch64-linux-android29 target), bionic headers,
# aapt2 + zipalign + apksigner, Khronos OpenXR loader from Maven Central.
# See README.md for how to set up the sysroot and signing key.
#
# Environment:
#   SYSROOT      bionic + GLES/EGL/OpenXR/JNI headers   (default /opt/build/sysroot)
#   BIONIC_SRC   bionic checkout for symbol maps        (default /opt/build/deps/bionic)
#   XR_LOADER    arm64 libopenxr_loader.so              (default from the Khronos AAR)
#   ANDROID_JAR  framework resources for linking      (default: Debian's Android 14 framework-res.apk)
#   KEYSTORE / KEYSTORE_PASS / KEY_ALIAS                (default: release key in ./signing)
set -euo pipefail
cd "$(dirname "$0")"

SYSROOT=${SYSROOT:-/opt/build/sysroot}
BIONIC_SRC=${BIONIC_SRC:-/opt/build/deps/bionic}
XR_LOADER=${XR_LOADER:-/opt/build/deps/xrl/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so}
ANDROID_JAR=${ANDROID_JAR:-/usr/share/android-framework-res/framework-res.apk}  # Android 14 resource table
KEYSTORE=${KEYSTORE:-signing/holyquranvr-release.jks}
KEY_ALIAS=${KEY_ALIAS:-holyquranvr}
MIN_API=29
OUT=build/android
APK=build/HolyQuranVR.apk

rm -rf "$OUT"
mkdir -p "$OUT/obj" "$OUT/stubs" "$OUT/apk/lib/arm64-v8a" "$OUT/apk/assets"

# ---------------------------------------------------------------- compile
CFLAGS=(--target=aarch64-linux-android$MIN_API -nostdinc
        -isystem "$(clang -print-resource-dir)/include" -isystem "$SYSROOT/usr/include"
        -Isrc/ndk -Isrc -std=c11 -O2 -fPIC -ffunction-sections -fdata-sections
        -fvisibility=hidden -fno-stack-protector -mno-outline-atomics -fno-math-errno
        -DQVR_ANDROID -D__ANDROID_API__=$MIN_API -Wall -Wno-unused-parameter -Wno-missing-field-initializers)
SRCS=(main_android app scene ui gfx pages quran stb_impl)
for s in "${SRCS[@]}"; do
  clang "${CFLAGS[@]}" -c "src/$s.c" -o "$OUT/obj/$s.o"
done

# ---------------------------------------------------------------- link
python3 tools/gen_stubs.py "$OUT/stubs" "$BIONIC_SRC" $MIN_API "$OUT"/obj/*.o
cp "$XR_LOADER" "$OUT/apk/lib/arm64-v8a/libopenxr_loader.so"
STUB_LIBS=()
for f in "$OUT"/stubs/lib*.so; do n=$(basename "$f" .so); STUB_LIBS+=("-l${n#lib}"); done
ld.lld -shared -soname libquranvr.so --no-undefined --gc-sections --build-id=sha1 \
  -z noexecstack -z relro -z now -z max-page-size=16384 --hash-style=both \
  -o "$OUT/apk/lib/arm64-v8a/libquranvr.so" "$OUT"/obj/*.o \
  -L"$OUT/stubs" -L"$OUT/apk/lib/arm64-v8a" -lopenxr_loader "${STUB_LIBS[@]}"
llvm-strip --strip-unneeded "$OUT/apk/lib/arm64-v8a/libquranvr.so"

# ---------------------------------------------------------------- resources + manifest
aapt2 compile --dir android/res -o "$OUT/res.zip"
aapt2 link -o "$OUT/base.apk" -I "$ANDROID_JAR" --manifest android/AndroidManifest.xml \
  --min-sdk-version $MIN_API --target-sdk-version 32 "$OUT/res.zip"

# ---------------------------------------------------------------- package
cp -r assets/* "$OUT/apk/assets/"
cp "$OUT/base.apk" "$OUT/unaligned.apk"
( cd "$OUT/apk"
  # Assets are already-compressed PNG/JPEG: store them so AAssetManager can map them.
  zip -q -0 -r ../unaligned.apk assets
  zip -q -9 -r ../unaligned.apk lib )
zipalign -f -p 4 "$OUT/unaligned.apk" "$OUT/aligned.apk"

# ---------------------------------------------------------------- sign
if [ ! -f "$KEYSTORE" ]; then
  echo "No keystore at $KEYSTORE; creating a new release key (keep it safe: store updates must use the same key)."
  mkdir -p "$(dirname "$KEYSTORE")"
  KEYSTORE_PASS=${KEYSTORE_PASS:-$(head -c 18 /dev/urandom | base64 | tr -d '/+=')}
  keytool -genkeypair -v -keystore "$KEYSTORE" -alias "$KEY_ALIAS" -keyalg RSA -keysize 4096 -validity 36500 \
    -storepass "$KEYSTORE_PASS" -keypass "$KEYSTORE_PASS" -dname "CN=Holy Quran VR, O=Albadani, C=SA" 2>/dev/null
  echo "$KEYSTORE_PASS" > "$(dirname "$KEYSTORE")/keystore-password.txt"
fi
KEYSTORE_PASS=${KEYSTORE_PASS:-$(cat "$(dirname "$KEYSTORE")/keystore-password.txt")}
apksigner sign --ks "$KEYSTORE" --ks-key-alias "$KEY_ALIAS" --ks-pass "pass:$KEYSTORE_PASS" \
  --min-sdk-version $MIN_API --out "$APK" "$OUT/aligned.apk"
apksigner verify --min-sdk-version $MIN_API "$APK"
ls -la "$APK"
echo "Built $APK"

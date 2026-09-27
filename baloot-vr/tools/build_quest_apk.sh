#!/usr/bin/env bash
# Build a signed Meta Quest APK from the Godot project without Gradle / Android SDK.
# Requires: godot 4.4.1 + export templates, apktool.jar, uber-apk-signer.jar,
# the Khronos OpenXR loader (libopenxr_loader.so, arm64-v8a) and a release keystore.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="${VR_TOOLS:-/opt/vr}"
GODOT="${GODOT:-$TOOLS/godot}"
PKG="com.balootvr.majlis"
OUT="$HERE/build"
mkdir -p "$OUT"
rm -rf "$OUT/raw.apk" "$OUT/dec" "$OUT/unsigned.apk" "$OUT"/signed

"$GODOT" --headless --path "$HERE" --import >/dev/null 2>&1 || true
"$GODOT" --headless --path "$HERE" --export-release "Quest" "$OUT/raw.apk"
java -jar "$TOOLS/apktool.jar" d -f -o "$OUT/dec" "$OUT/raw.apk"
python3 "$HERE/tools/patch_quest_apk.py" "$OUT/dec" "$TOOLS/aar/meta/jni/arm64-v8a/libopenxr_loader.so" "$PKG"
java -jar "$TOOLS/apktool.jar" b -o "$OUT/unsigned.apk" "$OUT/dec"
java -jar "$TOOLS/uber-apk-signer.jar" -a "$OUT/unsigned.apk" -o "$OUT/signed" \
  --ks "${KEYSTORE:-$TOOLS/baloot-release.keystore}" --ksAlias "${KS_ALIAS:-baloot}" \
  --ksPass "${KS_PASS:-balootvr}" --ksKeyPass "${KS_PASS:-balootvr}"
cp "$OUT"/signed/*.apk "$OUT/BalootVR-quest.apk"
echo "Built $OUT/BalootVR-quest.apk"

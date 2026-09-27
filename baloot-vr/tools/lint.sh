#!/usr/bin/env bash
# Parse-check every GDScript file headlessly.
cd "$(dirname "$0")/.."
for f in $(find scripts -name "*.gd"); do
  out=$(timeout 60 "${GODOT:-/opt/vr/godot}" --headless --path . --check-only -s "$f" 2>&1 < /dev/null | grep -E "SCRIPT ERROR|Parse Error|ERROR: " | grep -v "OpenXR" | head -5)
  [ -n "$out" ] && echo "== $f" && echo "$out"
done
echo lint done

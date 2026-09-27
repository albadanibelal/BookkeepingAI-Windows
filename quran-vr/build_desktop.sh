#!/bin/sh
# Builds the headless desktop preview renderer (same app/renderer code as the Quest build).
set -e
cd "$(dirname "$0")"
mkdir -p build
gcc -O2 -g -std=c11 -D_GNU_SOURCE -DQVR_DESKTOP -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
  -Isrc src/main_desktop.c src/app.c src/scene.c src/ui.c src/gfx.c src/pages.c src/quran.c src/stb_impl.c \
  -o build/qvr_preview -lEGL -lGLESv2 -lm -lpthread
echo built build/qvr_preview

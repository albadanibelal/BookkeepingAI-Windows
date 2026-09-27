#!/bin/bash
# Regenerates every runtime asset in ./assets from its sources:
#   pages/p001..p604.png  Madina Mushaf pages (Chromium + KFGQPC Hafs font)
#   frame*.png            ornamental page borders
#   ui_atlas.*            UI font/icon/Arabic-label atlas
#   meta.txt              surah/page/juz metadata
#   earth_*.{jpg,png}     NASA Blue/Black Marble textures + procedural clouds
# Requires: node 18+, python3 (pillow, numpy), Chromium (PLAYWRIGHT executable).
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p assets assets_src
(cd tools && npm install --silent)
# Set CHROMIUM_PATH to use a specific Chromium build (otherwise Playwright's default).
pip install --quiet pillow numpy
cd tools
node export_meta.mjs ../assets/meta.txt
node render_frame.mjs ../assets
node render_pages.mjs ../assets/pages 1 604
python3 quantize_pages.py ../assets/pages
node render_ui_atlas.mjs ../assets
python3 -c "from PIL import Image; Image.open('../assets/ui_atlas_rgba.png').convert('L').save('../assets/ui_atlas.png', optimize=True)"
rm -f ../assets/ui_atlas_rgba.png
python3 gen_clouds.py ../assets/earth_clouds.png
node render_icon.mjs ../android/res/drawable/ic_launcher.png 192
node render_icon.mjs ../assets_src/icon_512.png 512
# NASA imagery (public domain) as packaged in the three-globe examples
TG=$(mktemp -d)
git clone --depth 1 --filter=blob:none --sparse https://github.com/vasturiano/three-globe.git "$TG" -q
(cd "$TG" && git sparse-checkout set example/img)
python3 - "$TG/example/img" <<'EOF'
import sys
from PIL import Image
src = sys.argv[1]
Image.open(f'{src}/earth-blue-marble.jpg').convert('RGB').save('../assets/earth_day.jpg', quality=90)
Image.open(f'{src}/earth-night.jpg').convert('RGB').save('../assets/earth_night.jpg', quality=90)
Image.open(f'{src}/earth-water.png').convert('L').resize((2048, 1024)).save('../assets/earth_water.png', optimize=True)
EOF
rm -rf "$TG"
echo "assets ready"

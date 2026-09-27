# Quantize page textures to 4 bits/channel (16 anti-aliasing levels) and
# re-compress. Cuts the APK size by ~35% with no visible difference in VR.
import sys, glob, numpy as np
from PIL import Image
for f in sorted(glob.glob(sys.argv[1] + '/p*.png')):
    im = np.asarray(Image.open(f).convert('RGB'))
    q = ((im.astype(np.uint16) + 8) // 17 * 17).clip(0, 255).astype(np.uint8)
    Image.fromarray(q).save(f, 'PNG', optimize=True)
print('quantized')

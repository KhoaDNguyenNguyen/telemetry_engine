import sys
from PIL import Image

def gen_c_array(path, w, h, name):
    try:
        img = Image.open(path).resize((w, h)).convert('RGB')
    except Exception:
        img = Image.new('RGB', (w, h), color=(255, 0, 255))
    res = f'const uint16_t {name}[{w * h}] = {{\n'
    for y in range(h):
        for x in range(w):
            r, g, b = img.getpixel((x, y))
            c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            res += f'0x{c:04X},'
    return res + '};\n'

with open('core/inc/assets.h', 'w') as f:
    f.write('#ifndef ASSETS_H\n#define ASSETS_H\n#include <stdint.h>\n')
    f.write(gen_c_array(sys.argv[1] if len(sys.argv) > 1 else 'face.jpg', 48, 48, 'FACE_SPRITE'))
    f.write(gen_c_array(sys.argv[2] if len(sys.argv) > 2 else 'cactus.jpg', 32, 48, 'CACTUS_SPRITE'))
    f.write(gen_c_array(sys.argv[3] if len(sys.argv) > 3 else 'plane.jpg', 40, 24, 'PLANE_SPRITE'))
    f.write('#endif\n')

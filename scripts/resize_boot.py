import sys
from PIL import Image
try:
    img = Image.open(sys.argv[1]).resize((240, 240)).convert('RGB')
except Exception:
    img = Image.new('RGB', (240, 240), color=(15, 20, 30))
with open('core/inc/boot_logo.h', 'w') as f:
    f.write('#ifndef BOOT_LOGO_H\n#define BOOT_LOGO_H\n#include <stdint.h>\nconst uint16_t BOOT_LOGO[240 * 240] = {\n')
    for y in range(240):
        for x in range(240):
            r, g, b = img.getpixel((x, y))
            c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            f.write(f'0x{c:04X},')
    f.write('};\n#endif\n')

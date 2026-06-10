from PIL import Image
import sys

def convert_to_rgb565(img_path, output_path):
    img = Image.open(img_path).convert('RGB')
    img = img.resize((240, 240))
    pixels = img.load()

    with open(output_path, 'w') as f:
        f.write("#ifndef BOOT_LOGO_H\n#define BOOT_LOGO_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write("const uint16_t BOOT_LOGO[240 * 240] = {\n")
        
        for y in range(240):
            for x in range(240):
                r, g, b = pixels[x, y]
                # Convert to RGB565 format
                val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                f.write(f"0x{val:04X}, ")
            f.write("\n")
            
        f.write("};\n\n#endif // BOOT_LOGO_H\n")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python img_to_c.py <input.png> <output.h>")
        sys.exit(1)
    convert_to_rgb565(sys.argv[1], sys.argv[2])

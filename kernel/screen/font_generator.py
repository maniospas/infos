# python3 kernel/screen/font_generator.py kernel/screen/font.ttf 

#!/usr/bin/env python3
from PIL import Image, ImageFont, ImageDraw
import sys, os

FIRST, LAST = 32, 126
SIZES = [
    (32, 64, 62),
]

# -------------- parameters you can tweak --------------
THRESHOLD = 250      # 0–255: higher = thinner strokes, lower = bolder
BASELINE_SCALE = 0.85
# -------------------------------------------------------

def generate_font_array(font_path):
    font_dir = os.path.dirname(os.path.abspath(font_path))
    font_name = os.path.splitext(os.path.basename(font_path))[0]

    for CHAR_WIDTH, CHAR_HEIGHT, FONT_SIZE in SIZES:
        font = ImageFont.truetype(font_path, FONT_SIZE)
        baseline_y = int(CHAR_HEIGHT * BASELINE_SCALE)
        out = []

        for code in range(FIRST, LAST + 1):
            c = chr(code)
            img = Image.new("L", (CHAR_WIDTH, CHAR_HEIGHT), 0)
            draw = ImageDraw.Draw(img)

            bbox = font.getbbox(c)
            if bbox is None:
                out.append([[0] * CHAR_HEIGHT for _ in range(CHAR_WIDTH)])
                continue

            glyph_w = bbox[2] - bbox[0]
            x_offset = (CHAR_WIDTH - glyph_w) // 2 - bbox[0]

            # Draw as grayscale
            draw.text((x_offset, baseline_y), c, font=font, fill=255, anchor="ls")

            # Convert grayscale → binary (remove antialias)
            arr = img.load()
            glyph = [[1 if arr[x, y] >= THRESHOLD else 0 for y in range(CHAR_HEIGHT)]
                     for x in range(CHAR_WIDTH)]
            out.append(glyph)

        output_header = os.path.join(font_dir, f"{font_name}_font{CHAR_WIDTH}x{CHAR_HEIGHT}.h")
        macro = f"FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_H"

        with open(output_header, "w") as f:
            f.write(f"/* Auto-generated {CHAR_WIDTH}x{CHAR_HEIGHT} font from {os.path.basename(font_path)} */\n")
            f.write(f"#ifndef {macro}\n#define {macro}\n\n#include <stdint.h>\n\n")
            f.write(f"#define FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_WIDTH {CHAR_WIDTH}\n")
            f.write(f"#define FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_HEIGHT {CHAR_HEIGHT}\n")
            f.write(f"#define FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_FIRST {FIRST}\n")
            f.write(f"#define FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_LAST {LAST}\n")
            f.write(f"#define FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_COUNT (FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_LAST - FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_FIRST + 1)\n\n")
            f.write(f"static const uint8_t font{CHAR_WIDTH}x{CHAR_HEIGHT}[FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_COUNT][{CHAR_WIDTH}][{CHAR_HEIGHT}] = {{\n")
            for i, g in enumerate(out):
                c = chr(i + FIRST)
                safe_char = c if c not in ["\\", "'"] else f"\\{c}"
                f.write(f"  /* '{safe_char}' */ {{\n")
                for x in range(CHAR_WIDTH):
                    row = ",".join("1" if v else "0" for v in g[x])
                    f.write(f"    {{{row}}},\n")
                f.write("  },\n")
            f.write("};\n\n#endif\n")

        print(f"✅ Generated {output_header} ({len(out)} glyphs)")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 font_generator.py <fontfile.ttf>")
        sys.exit(1)
    generate_font_array(sys.argv[1])

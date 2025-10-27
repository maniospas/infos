# python3 kernel/screen/font_generator.py kernel/screen/spleen-8x16.bdf kernel/screen/spleen-16x32.bdf kernel/screen/spleen-32x64.bdf
#!/usr/bin/env python3
import sys, os, re

FIRST, LAST = 32, 126

def parse_bdf_metadata(path):
    """Extract FONTBOUNDINGBOX width and height from the BDF header"""
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith("FONTBOUNDINGBOX"):
                parts = line.split()
                if len(parts) >= 3:
                    return int(parts[1]), int(parts[2])
    raise ValueError("FONTBOUNDINGBOX not found in BDF file")

def parse_bdf(path):
    """Parse BDF file and return a dict {codepoint: [rows of hex strings]}"""
    glyphs = {}
    current_code = None
    collecting = False
    bitmap_data = []

    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if line.startswith("ENCODING "):
                current_code = int(line.split()[1])
            elif line.startswith("BITMAP"):
                collecting = True
                bitmap_data = []
            elif line.startswith("ENDCHAR"):
                if current_code is not None and bitmap_data:
                    glyphs[current_code] = bitmap_data
                collecting = False
                bitmap_data = []
                current_code = None
            elif collecting:
                bitmap_data.append(line)
    return glyphs

def hex_to_bits(hex_str, width):
    """Convert hex bitmap string to binary list of given width"""
    bits = bin(int(hex_str, 16))[2:].zfill(len(hex_str) * 4)
    return [int(b) for b in bits[:width]]

def generate_font_array(bdf_path):
    font_dir = os.path.dirname(os.path.abspath(bdf_path))
    font_name = os.path.splitext(os.path.basename(bdf_path))[0]

    # Auto-detect glyph size
    CHAR_WIDTH, CHAR_HEIGHT = parse_bdf_metadata(bdf_path)
    print(f"🧩 Detected font {font_name}: {CHAR_WIDTH}x{CHAR_HEIGHT}")

    glyphs = parse_bdf(bdf_path)
    out = []

    for code in range(FIRST, LAST + 1):
        data = glyphs.get(code)
        if not data:
            out.append([[0] * CHAR_HEIGHT for _ in range(CHAR_WIDTH)])
            continue

        rows = [hex_to_bits(line, CHAR_WIDTH) for line in data]
        # BDF bitmaps are top-down; pad to height
        while len(rows) < CHAR_HEIGHT:
            rows.insert(0, [0] * CHAR_WIDTH)
        rows = rows[-CHAR_HEIGHT:]

        # Transpose (x-major)
        glyph = [[rows[y][x] for y in range(CHAR_HEIGHT)] for x in range(CHAR_WIDTH)]
        out.append(glyph)

    output_header = os.path.join(font_dir, f"{font_name}_font{CHAR_WIDTH}x{CHAR_HEIGHT}.h")
    macro = f"FONT{CHAR_WIDTH}X{CHAR_HEIGHT}_H"

    with open(output_header, "w") as f:
        f.write(f"/* Auto-generated {CHAR_WIDTH}x{CHAR_HEIGHT} font from {os.path.basename(bdf_path)} */\n")
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

    print(f"✅ Generated {output_header} ({len(out)} glyphs)\n")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 font_generator_bdf.py <fontfile1.bdf> [fontfile2.bdf ...]")
        sys.exit(1)

    for font_path in sys.argv[1:]:
        generate_font_array(font_path)

#!/usr/bin/env python3
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "source" / "font_generated.h"

FONT_FILE = Path("/usr/share/fonts/TTF/DejaVuSans.ttf")
if not FONT_FILE.exists():
    matches = list(Path("/usr/share/fonts").rglob("DejaVuSans.ttf"))
    if not matches:
        raise SystemExit("DejaVuSans.ttf not found. Install dejavu-fonts-ttf.")
    FONT_FILE = matches[0]

font = ImageFont.truetype(str(FONT_FILE), 11)
GLYPH_H = 15
GLYPH_W = 12

with OUT.open("w") as f:
    f.write("/* Generated from DejaVu Sans using Pillow. */\n")
    f.write("#ifndef DSIMUSIC_FONT_GENERATED_H\n")
    f.write("#define DSIMUSIC_FONT_GENERATED_H\n")
    f.write("#include <stdint.h>\n")
    f.write("#define FONT_HEIGHT 15\n")
    f.write("#define FONT_FIRST 32\n")
    f.write("#define FONT_COUNT 95\n\n")

    widths = []
    glyphs = []

    for code in range(32, 127):
        ch = chr(code)
        img = Image.new("L", (GLYPH_W, GLYPH_H), 0)
        draw = ImageDraw.Draw(img)
        draw.text((0, 11), ch, font=font, fill=255, anchor="ls")

        width = max(1, min(GLYPH_W, round(font.getlength(ch)) + 1))
        widths.append(width)

        rows = []
        for y in range(GLYPH_H):
            bits = 0
            for x in range(GLYPH_W):
                if img.getpixel((x, y)) >= 110:
                    bits |= 1 << x
            rows.append(bits)

        glyphs.append(rows)

    f.write("static const uint8_t fontWidth[FONT_COUNT] = {\n")
    f.write("    " + ", ".join(map(str, widths)) + "\n};\n\n")

    f.write(
        "static const uint16_t "
        "fontBitmap[FONT_COUNT][FONT_HEIGHT] = {\n"
    )

    for rows in glyphs:
        f.write("    {" + ", ".join(f"0x{x:03X}" for x in rows) + "},\n")

    f.write("};\n#endif\n")

print(f"Generated {OUT}")

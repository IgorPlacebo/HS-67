#!/usr/bin/env python3
"""Bake the fonts for the HS-02 modern colour screens.

Output: source/Core/Threads/UI/drawing/color_160x80/Hs02ModernFont.hpp

Four faces, each stored as 2 bits per pixel (0 = empty, 1 = anti-alias edge, 3 = solid):
  * hero  - Oswald SemiBold, digits for the big temperature readout
  * value - Oswald Medium, numbers + units (watts, target temperature)
  * label - Oswald Medium, every character used in Hs02UiStrings.hpp (status pill, hints)
  * menu  - Oswald Medium, menu rows
  * title - Oswald SemiBold, the device name on the heating screen

Solid pixels are those the anti-aliased render covers strongly (SOLID_THRESHOLD), edge
pixels the ones it covers noticeably (EDGE_THRESHOLD). The drawing code paints edges in a
dimmer colour of the same band palette, which gives a cheap two-level anti-aliasing on a
2bpp framebuffer.

Requires Pillow. Usage:  python3 tools/hs02-ui-fonts/gen_modern_font.py
"""
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
FONT_DIR = Path(__file__).resolve().parent / "fonts"
UI_DIR = ROOT / "source/Core/Threads/UI/drawing/color_160x80"
STRINGS = UI_DIR / "Hs02UiStrings.hpp"
OUT = UI_DIR / "Hs02ModernFont.hpp"

EDGE_THRESHOLD = 90  # anti-aliased coverage (0..255) that still earns an edge pixel
# Oswald's hinted monochrome rendering breaks stems at these sizes, so solid pixels are taken
# from the anti-aliased coverage instead of FreeType's mono rasteriser.
SOLID_THRESHOLD = 165


def title_charset() -> str:
    """Characters of the device name (kBootPill, both languages), for the Title face."""
    m = re.search(r'HS02_TEXT\(kBootPill,\s*"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)"\)', STRINGS.read_text(encoding="utf-8"))
    return "".join(sorted(set((m.group(1) + m.group(2)) if m else "")))


def ui_strings_charset() -> str:
    text = STRINGS.read_text(encoding="utf-8")
    chars = set()
    for literal in re.findall(r'"((?:[^"\\]|\\.)*)"', text):
        chars.update(literal)
    chars.update("0123456789.:% v°/+-")
    return "".join(sorted(chars))


FONT_FILE = "Oswald-Variable.ttf"  # SIL OFL 1.1, see OFL-Oswald.txt
FONT_NAME = "Oswald (c) The Oswald Project Authors, SIL Open Font License 1.1 (tools/hs02-ui-fonts/OFL-Oswald.txt)."

FACES = [
    # name, font file, pixel size, weight (variable-font wght axis), characters
    ("Hero", FONT_FILE, 36, 600, "0123456789-HS"),  # digits + splash name "HS-67"
    ("Value", FONT_FILE, 16, 500, "0123456789.-:%°ВтWV "),
    ("Label", FONT_FILE, 12, 500, None),  # filled from the UI strings
    ("Menu", FONT_FILE, 13, 500, None),  # menu rows, filled from the UI strings
    ("Title", FONT_FILE, 20, 600, "TITLE"),  # the device name on the heating screen (kBootPill)
]


def load_font(font_file: str, size: int, weight: int):
    font = ImageFont.truetype(str(FONT_DIR / font_file), size)
    try:
        axes = font.get_variation_axes()
    except OSError:  # static font: no axes
        return font
    values = []
    for axis in axes:
        name = axis.get("name")
        value = weight if name in (b"Weight", "Weight") else axis["default"]
        values.append(min(max(value, axis["minimum"]), axis["maximum"]))
    font.set_variation_by_axes(values)
    return font


def render_face(font_file: str, size: int, weight: int, charset: str):
    font = load_font(font_file, size, weight)
    ascent, descent = font.getmetrics()
    canvas_h = ascent + descent + 4
    glyphs = []
    top, bottom = canvas_h, 0
    for ch in charset:
        advance = round(font.getlength(ch))
        bbox = font.getbbox(ch)
        canvas_w = max(advance, bbox[2]) + 4
        aa = Image.new("L", (canvas_w, canvas_h), 0)
        ImageDraw.Draw(aa).text((2, 2), ch, font=font, fill=255)
        px = []
        for y in range(canvas_h):
            row = []
            for x in range(canvas_w):
                if aa.getpixel((x, y)) >= SOLID_THRESHOLD:
                    row.append(3)
                elif aa.getpixel((x, y)) >= EDGE_THRESHOLD:
                    row.append(1)
                else:
                    row.append(0)
            px.append(row)
        cols = [x for x in range(canvas_w) if any(px[y][x] for y in range(canvas_h))]
        rows = [y for y in range(canvas_h) if any(px[y])]
        if rows:
            top, bottom = min(top, rows[0]), max(bottom, rows[-1])
        glyphs.append((ch, advance, px, cols))
    if bottom < top:
        top, bottom = 0, 0
    height = bottom - top + 1
    out = []
    for ch, advance, px, cols in glyphs:
        if cols:
            x0, x1 = cols[0], cols[-1]
        else:
            x0, x1 = 2, 1  # zero-width (space)
        width = x1 - x0 + 1
        bitmap = [row[x0 : x1 + 1] for row in px[top : bottom + 1]]
        out.append((ch, advance, width, x0 - 2, bitmap))
    return height, ascent - (top - 2), out


def pack(bitmap, width):
    data = []
    for row in bitmap:
        for i in range(0, width, 4):
            b = 0
            for k in range(4):
                if i + k < width:
                    b |= (row[i + k] & 3) << (k * 2)
            data.append(b)
    return data


def main():
    label_chars = ui_strings_charset()
    lines = [
        "#pragma once",
        "// GENERATED by tools/hs02-ui-fonts/gen_modern_font.py -- do not edit by hand.",
        "// " + FONT_NAME,
        "// 2bpp glyphs, row-major, 4 px per byte (LSB first): 0 empty, 1 edge, 3 solid.",
        "#include <stdint.h>",
        "",
        "namespace Hs02ModernFont {",
        "struct Glyph {",
        "  uint16_t code;    // Unicode code point",
        "  uint8_t  advance; // pen advance in px",
        "  uint8_t  width;   // bitmap width in px",
        "  int8_t   xOffset; // bitmap x relative to the pen",
        "  uint16_t offset;  // byte offset into the face's data",
        "};",
        "struct Face {",
        "  uint8_t      height;   // bitmap rows (shared by every glyph)",
        "  uint8_t      baseline; // rows from the top of the bitmap to the baseline",
        "  uint8_t      count;",
        "  const Glyph *glyphs; // sorted by code",
        "  const uint8_t *data;",
        "};",
        "",
    ]
    total = 0
    for name, font_file, size, weight, charset in FACES:
        chars = title_charset() if charset == "TITLE" else (charset if charset is not None else label_chars)
        chars = "".join(sorted(set(chars), key=ord))
        height, baseline, glyphs = render_face(font_file, size, weight, chars)
        data, table = [], []
        for ch, advance, width, xoff, bitmap in glyphs:
            table.append((ord(ch), advance, width, xoff, len(data)))
            data += pack(bitmap, width)
        total += len(data) + len(table) * 8
        lines.append(f"// {name}: {font_file} wght {weight} @ {size}px, {len(glyphs)} glyphs, {len(data)} bytes")
        lines.append(f"constexpr uint8_t k{name}Data[] = {{")
        for i in range(0, len(data), 24):
            lines.append("  " + ",".join(f"0x{b:02X}" for b in data[i : i + 24]) + ",")
        lines.append("};")
        lines.append(f"constexpr Glyph k{name}Glyphs[] = {{")
        for code, advance, width, xoff, off in table:
            shown = chr(code) if code > 32 else " "
            lines.append(f"  {{0x{code:04X}, {advance}, {width}, {xoff}, {off}}}, // {shown}")
        lines.append("};")
        lines.append(f"constexpr Face k{name} = {{{height}, {baseline}, {len(table)}, k{name}Glyphs, k{name}Data}};")
        lines.append("")
    lines.append("} // namespace Hs02ModernFont")
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)} (~{total} bytes of flash), label charset: {label_chars!r}")


if __name__ == "__main__":
    sys.exit(main())

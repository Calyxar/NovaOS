#!/usr/bin/env python3

"""
NovaOS Font Generator

Converts a TrueType/OpenType font into anti-aliased
C++ glyph data that can be compiled directly into NovaOS.

Usage:
    python3 tools/fonts/generate_font.py \
        data/fonts/NovaSans.ttf \
        kernel/drivers/video/generated_font.h
"""

import sys
from pathlib import Path

from PIL import ImageFont


# ============================================================
# Configuration
# ============================================================

FONT_SIZES = [14, 16, 20, 24, 32]

FIRST_CHAR = 32
LAST_CHAR = 126


# ============================================================
# Helpers
# ============================================================

def sanitize_name(path: Path) -> str:
    name = path.stem

    result = []

    for ch in name:
        if ch.isalnum():
            result.append(ch)
        else:
            result.append("_")

    return "".join(result)


def get_glyph(font, character):
    """
    Rasterize one glyph as an 8-bit grayscale coverage bitmap.
    """

    # Pillow's mask contains anti-aliased coverage values:
    # 0   = transparent
    # 255 = fully covered
    mask, offset = font.getmask2(
        character,
        mode="L"
    )

    width, height = mask.size

    bitmap = list(mask)

    # Advance width can be fractional.
    advance = round(font.getlength(character))

    offset_x, offset_y = offset

    return {
        "width": width,
        "height": height,
        "offset_x": offset_x,
        "offset_y": offset_y,
        "advance": advance,
        "bitmap": bitmap,
    }


def write_bitmap(
    output,
    name,
    bitmap
):
    output.write(
        f"static const uint8_t {name}[] = {{\n"
    )

    if not bitmap:
        output.write("    0\n")
    else:
        for i in range(0, len(bitmap), 16):

            row = bitmap[i:i + 16]

            values = ", ".join(
                f"{value:3d}"
                for value in row
            )

            output.write(
                f"    {values}"
            )

            if i + 16 < len(bitmap):
                output.write(",")

            output.write("\n")

    output.write("};\n\n")


# ============================================================
# Generator
# ============================================================

def generate(
    font_path: Path,
    output_path: Path
):

    font_name = sanitize_name(font_path)

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True
    )

    with output_path.open(
        "w",
        encoding="utf-8"
    ) as output:

        # ----------------------------------------------------
        # Header
        # ----------------------------------------------------

        output.write(
            "/**\n"
            " * NovaOS — Generated Font Data\n"
            " *\n"
            " * AUTO-GENERATED FILE.\n"
            " * DO NOT EDIT MANUALLY.\n"
            " */\n\n"
        )

        output.write("#pragma once\n\n")
        output.write("#include <stdint.h>\n\n")

        output.write(
            "namespace NovaFontData {\n\n"
        )

        # ----------------------------------------------------
        # Structures
        # ----------------------------------------------------

        output.write(
            "struct GlyphData {\n"
            "    uint16_t width;\n"
            "    uint16_t height;\n"
            "\n"
            "    int16_t offset_x;\n"
            "    int16_t offset_y;\n"
            "\n"
            "    uint16_t advance;\n"
            "\n"
            "    const uint8_t* bitmap;\n"
            "};\n\n"
        )

        output.write(
            "struct FontSizeData {\n"
            "    uint16_t pixel_size;\n"
            "    uint16_t ascent;\n"
            "    uint16_t descent;\n"
            "    uint16_t line_height;\n"
            "\n"
            "    const GlyphData* glyphs;\n"
            "};\n\n"
        )

        # ----------------------------------------------------
        # Generate every configured size
        # ----------------------------------------------------

        font_size_names = []

        for pixel_size in FONT_SIZES:

            print(
                f"Generating {pixel_size}px..."
            )

            font = ImageFont.truetype(
                str(font_path),
                pixel_size
            )

            ascent, descent = font.getmetrics()

            line_height = ascent + descent

            glyphs = []

            # ------------------------------------------------
            # Glyph bitmaps
            # ------------------------------------------------

            for codepoint in range(
                FIRST_CHAR,
                LAST_CHAR + 1
            ):

                character = chr(codepoint)

                glyph = get_glyph(
                    font,
                    character
                )

                bitmap_name = (
                    f"glyph_{pixel_size}_{codepoint}"
                )

                write_bitmap(
                    output,
                    bitmap_name,
                    glyph["bitmap"]
                )

                glyph["bitmap_name"] = bitmap_name

                glyphs.append(glyph)

            # ------------------------------------------------
            # Glyph table
            # ------------------------------------------------

            table_name = (
                f"glyphs_{pixel_size}"
            )

            output.write(
                f"static const GlyphData "
                f"{table_name}[] = {{\n"
            )

            for glyph in glyphs:

                output.write(
                    "    { "
                    f"{glyph['width']}, "
                    f"{glyph['height']}, "
                    f"{glyph['offset_x']}, "
                    f"{glyph['offset_y']}, "
                    f"{glyph['advance']}, "
                    f"{glyph['bitmap_name']} "
                    "},\n"
                )

            output.write("};\n\n")

            # ------------------------------------------------
            # Font size descriptor
            # ------------------------------------------------

            descriptor_name = (
                f"font_{pixel_size}"
            )

            output.write(
                f"static const FontSizeData "
                f"{descriptor_name} = {{\n"
                f"    {pixel_size},\n"
                f"    {ascent},\n"
                f"    {descent},\n"
                f"    {line_height},\n"
                f"    {table_name}\n"
                f"}};\n\n"
            )

            font_size_names.append(
                descriptor_name
            )

        # ----------------------------------------------------
        # Available font sizes
        # ----------------------------------------------------

        output.write(
            "static const FontSizeData* "
            "font_sizes[] = {\n"
        )

        for name in font_size_names:
            output.write(
                f"    &{name},\n"
            )

        output.write("};\n\n")

        output.write(
            "static constexpr uint32_t "
            "font_size_count = "
            "sizeof(font_sizes) / "
            "sizeof(font_sizes[0]);\n\n"
        )

        output.write(
            f"static constexpr uint32_t "
            f"first_character = {FIRST_CHAR};\n"
        )

        output.write(
            f"static constexpr uint32_t "
            f"last_character = {LAST_CHAR};\n\n"
        )

        output.write(
            "} // namespace NovaFontData\n"
        )

    print()
    print("NovaOS font generated successfully.")
    print(f"Input : {font_path}")
    print(f"Output: {output_path}")


# ============================================================
# Main
# ============================================================

def main():

    if len(sys.argv) != 3:

        print(
            "Usage:\n"
            "  generate_font.py "
            "<font.ttf> "
            "<output.h>"
        )

        return 1

    font_path = Path(
        sys.argv[1]
    )

    output_path = Path(
        sys.argv[2]
    )

    if not font_path.exists():

        print(
            f"ERROR: Font not found: "
            f"{font_path}"
        )

        return 1

    try:

        generate(
            font_path,
            output_path
        )

    except Exception as error:

        print(
            f"ERROR: {error}"
        )

        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""
Generate 12x12 pixel bitmap data for Chinese characters for ESP32 LCD display.
Simpler version with better error handling.
"""

import os
import sys
import subprocess

# First, ensure Pillow is installed
try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Installing Pillow...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow", "-q"])
    from PIL import Image, ImageDraw, ImageFont

# Characters to generate
CHARACTERS = [
    ('气', 0x6C14),
    ('温', 0x6E29),
    ('湿', 0x6E7F),
    ('度', 0x5EA6),
    ('土', 0x571F),
    ('光', 0x5149),
    ('照', 0x7167),
    ('压', 0x538B),
    ('告', 0x544A),
    ('警', 0x8B66),
    ('等', 0x7B49),
    ('待', 0x5F85),
    ('无', 0x65E0),
    ('数', 0x6570),
    ('据', 0x636E),
    ('正', 0x6B63),
    ('常', 0x5E38),
]

BITMAP_SIZE = 12

def find_font():
    """Find an available CJK font."""
    font_paths = [
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/NSimSun.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",
        "C:\\Windows\\Fonts\\NSimSun.ttf",
    ]
    
    for font_path in font_paths:
        if os.path.exists(font_path):
            print(f"✓ Found font: {font_path}")
            return font_path
    
    print("✗ No system fonts found, will attempt with default...")
    return None

def char_to_bitmap(char, font, size=12):
    """Convert a single character to a 12x12 bitmap."""
    img = Image.new('1', (BITMAP_SIZE, BITMAP_SIZE), color=1)
    draw = ImageDraw.Draw(img)
    
    try:
        # Draw character centered
        bbox = draw.textbbox((0, 0), char, font=font)
        if bbox:
            char_width = bbox[2] - bbox[0]
            char_height = bbox[3] - bbox[1]
        else:
            char_width, char_height = size, size
        
        x = max(0, (BITMAP_SIZE - char_width) // 2)
        y = max(0, (BITMAP_SIZE - char_height) // 2)
        
        draw.text((x, y), char, font=font, fill=0)
    except Exception as e:
        print(f"  Warning rendering {char}: {e}")
    
    return img

def bitmap_to_hex_row(img, row_idx):
    """Convert one row of bitmap to hex uint16_t."""
    pixels = list(img.getdata())
    row_pixels = pixels[row_idx * BITMAP_SIZE:(row_idx + 1) * BITMAP_SIZE]
    
    uint16_val = 0
    for col in range(BITMAP_SIZE):
        pixel = row_pixels[col]
        bit_val = 1 if pixel == 0 else 0  # Invert: 0=black (filled)
        uint16_val |= (bit_val << (15 - col))
    
    return f"0x{uint16_val:04X}"

def main():
    print("=" * 70)
    print("Chinese Character Bitmap Generator for ESP32 LCD")
    print("=" * 70)
    print()
    
    # Find font
    font_path = find_font()
    
    # Create font
    try:
        if font_path:
            font = ImageFont.truetype(font_path, 11)
            print(f"✓ Loaded font successfully")
        else:
            font = ImageFont.truetype("arial.ttf", 11)
            print("✓ Using Arial font")
    except Exception as e:
        print(f"✗ Font error: {e}")
        font = ImageFont.load_default()
    
    print()
    print(f"Generating bitmaps for {len(CHARACTERS)} characters...")
    print()
    
    # Generate C header
    c_lines = []
    c_lines.append("#ifndef CHAR_BITMAP_H")
    c_lines.append("#define CHAR_BITMAP_H")
    c_lines.append("")
    c_lines.append("#include <stdint.h>")
    c_lines.append("")
    c_lines.append("// Chinese character bitmaps (12x12 pixels)")
    c_lines.append("// Format: uint16_t array with pixels in bits [15:4]")
    c_lines.append("// MSB (bit 15) = leftmost pixel, bit 4 = rightmost pixel")
    c_lines.append("// Bits [3:0] are always 0")
    c_lines.append("")
    
    for char, code_point in CHARACTERS:
        try:
            print(f"  {char} (U+{code_point:04X})... ", end="", flush=True)
            
            img = char_to_bitmap(char, font)
            
            c_lines.append(f"// {char} (U+{code_point:04X})")
            c_lines.append("const uint16_t bitmap_char_{}[] = {{".format(code_point))
            
            for row in range(BITMAP_SIZE):
                hex_val = bitmap_to_hex_row(img, row)
                if row < BITMAP_SIZE - 1:
                    c_lines.append(f"    {hex_val},")
                else:
                    c_lines.append(f"    {hex_val}")
            
            c_lines.append("};")
            c_lines.append("")
            print("OK")
            
        except Exception as e:
            print(f"ERROR: {e}")
    
    # Add footer
    c_lines.append("// Bitmap array indices for quick lookup")
    c_lines.append("typedef struct {")
    c_lines.append("    uint32_t code_point;")
    c_lines.append("    const uint16_t *bitmap;")
    c_lines.append("} CharBitmap;")
    c_lines.append("")
    c_lines.append("const CharBitmap char_bitmaps[] = {")
    for char, code_point in CHARACTERS:
        c_lines.append(f"    {{0x{code_point:04X}, bitmap_char_{code_point}}},")
    c_lines.append("    {0, NULL}")
    c_lines.append("};")
    c_lines.append("")
    c_lines.append("#endif // CHAR_BITMAP_H")
    
    # Write file
    output_content = "\n".join(c_lines)
    output_file = "char_bitmap.h"
    
    with open(output_file, "w", encoding="utf-8") as f:
        f.write(output_content)
    
    print()
    print("=" * 70)
    print(f"✓ Output saved to: {output_file}")
    print("=" * 70)
    print()
    
    # Print preview
    print("Preview of generated code:")
    print("-" * 70)
    lines_to_show = output_content.split('\n')[:50]
    for line in lines_to_show:
        print(line)
    print("...")
    print("-" * 70)

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Minimal bitmap generator - creates sample data when fonts aren't available.
"""

# Sample bitmap data for each character (12x12, as uint16_t arrays)
# Generated assuming typical stroked character appearance
SAMPLE_BITMAPS = {
    '气': [  # U+6C14
        0x1C00, 0x3E00, 0x3600, 0x0600, 0x0C00, 0x1800, 0x3000, 0x3E00, 0x3600, 0x0600, 0x0C00, 0x1800
    ],
    '温': [  # U+6E29
        0x3E00, 0x3600, 0x3000, 0x3E00, 0x3600, 0x0600, 0x0C00, 0x1800, 0x3000, 0x3E00, 0x3600, 0x0000
    ],
    '湿': [  # U+6E7F
        0x1E00, 0x3F00, 0x3300, 0x0300, 0x0600, 0x0C00, 0x1800, 0x3000, 0x2000, 0x3C00, 0x3C00, 0x0000
    ],
    '度': [  # U+5EA6
        0x3E00, 0x2200, 0x2200, 0x3E00, 0x2200, 0x2200, 0x3E00, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000
    ],
    '土': [  # U+571F - simple square
        0x7E00, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x7E00
    ],
    '光': [  # U+5149
        0x0C00, 0x1E00, 0x3300, 0x3B00, 0x3900, 0x3800, 0x1C00, 0x0E00, 0x0700, 0x0380, 0x03C0, 0x0000
    ],
    '照': [  # U+7167
        0x3E00, 0x2200, 0x2200, 0x3E00, 0x2000, 0x4000, 0x4000, 0x7E00, 0x4200, 0x4200, 0x7E00, 0x0000
    ],
    '压': [  # U+538B
        0x0400, 0x0800, 0x1000, 0x3E00, 0x2200, 0x2200, 0x3E00, 0x2000, 0x4000, 0x4000, 0x7E00, 0x0000
    ],
    '告': [  # U+544A
        0x7E00, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x7E00, 0x0200, 0x0400, 0x0800, 0x1000
    ],
    '警': [  # U+8B66
        0x2200, 0x5400, 0x5400, 0x5400, 0x5400, 0x3800, 0x1000, 0x2200, 0x4400, 0x4400, 0x7E00, 0x0000
    ],
    '等': [  # U+7B49
        0x3E00, 0x2200, 0x2200, 0x3E00, 0x2200, 0x2200, 0x3E00, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000
    ],
    '待': [  # U+5F85
        0x0400, 0x0800, 0x1000, 0x3E00, 0x2200, 0x2200, 0x3E00, 0x2000, 0x4000, 0x4000, 0x7E00, 0x0000
    ],
    '无': [  # U+65E0
        0x3E00, 0x2200, 0x2200, 0x2200, 0x2200, 0x3E00, 0x2200, 0x2200, 0x2200, 0x2200, 0x7E00, 0x0000
    ],
    '数': [  # U+6570
        0x2000, 0x5000, 0x5000, 0x5000, 0x2800, 0x1400, 0x0A00, 0x0500, 0x0200, 0x0100, 0x0080, 0x0040
    ],
    '据': [  # U+636E
        0x4200, 0x4200, 0x4200, 0x7E00, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x4200, 0x7E00, 0x0000
    ],
    '正': [  # U+6B63
        0x7E00, 0x0400, 0x0800, 0x1000, 0x2000, 0x2000, 0x2000, 0x1000, 0x0800, 0x0400, 0x7E00, 0x0000
    ],
    '常': [  # U+5E38
        0x2200, 0x5400, 0x5400, 0x5400, 0x5400, 0x2200, 0x2200, 0x2200, 0x2200, 0x2200, 0x7E00, 0x0000
    ],
}

def generate_header():
    """Generate C header file content."""
    lines = []
    lines.append("#ifndef CHAR_BITMAP_H")
    lines.append("#define CHAR_BITMAP_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("// Chinese character bitmaps (12x12 pixels)")
    lines.append("// Format: uint16_t array with pixels in bits [15:4]")
    lines.append("// MSB (bit 15) = leftmost pixel, bit 4 = rightmost pixel")
    lines.append("// Bits [3:0] are always 0")
    lines.append("")
    
    char_list = [
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
    
    for char, code_point in char_list:
        lines.append(f"// {char} (U+{code_point:04X})")
        lines.append(f"const uint16_t bitmap_char_{code_point:04X}[] = {{")
        
        if char in SAMPLE_BITMAPS:
            bitmap = SAMPLE_BITMAPS[char]
            for i, val in enumerate(bitmap):
                if i < len(bitmap) - 1:
                    lines.append(f"    0x{val:04X},")
                else:
                    lines.append(f"    0x{val:04X}")
        
        lines.append("};")
        lines.append("")
    
    # Add lookup table
    lines.append("// Bitmap array indices for quick lookup")
    lines.append("typedef struct {")
    lines.append("    uint32_t code_point;")
    lines.append("    const uint16_t *bitmap;")
    lines.append("} CharBitmap;")
    lines.append("")
    lines.append("const CharBitmap char_bitmaps[] = {")
    
    for char, code_point in char_list:
        lines.append(f"    {{0x{code_point:04X}, bitmap_char_{code_point:04X}}},")
    
    lines.append("    {0, NULL}")
    lines.append("};")
    lines.append("")
    lines.append("#endif // CHAR_BITMAP_H")
    
    return "\n".join(lines)

if __name__ == "__main__":
    output = generate_header()
    
    with open("char_bitmap.h", "w", encoding="utf-8") as f:
        f.write(output)
    
    print("✓ Generated char_bitmap.h")
    print()
    print("=" * 70)
    print("Preview of char_bitmap.h:")
    print("=" * 70)
    print(output[:2000])
    print("...")
    print("=" * 70)

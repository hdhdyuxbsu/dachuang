#!/usr/bin/env python3
"""
Generate 12x12 pixel bitmap data for Chinese characters for ESP32 LCD display.
Output format: C arrays with uint16_t values (pixels in bits [15:4], MSB first).
"""

import os
import subprocess
import sys
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
FONT_PATHS = [
    "C:/Windows/Fonts/msyh.ttc",      # Microsoft YaHei
    "C:/Windows/Fonts/simhei.ttf",    # SimHei
    "C:/Windows/Fonts/simsun.ttc",    # SimSun
    "C:/Windows/Fonts/NSimSun.ttf",   # NSimSun
]

def find_font():
    """Find an available CJK font."""
    for font_path in FONT_PATHS:
        if os.path.exists(font_path):
            print(f"Found font: {font_path}")
            return font_path
    
    # Try system fonts
    print("No standard font paths found, attempting to use default...")
    return None

def create_font(font_path, size=12):
    """Create a font object."""
    try:
        if font_path:
            font = ImageFont.truetype(font_path, size)
        else:
            # Try to get a default font
            try:
                font = ImageFont.truetype("arial.ttf", size)
            except:
                print("Warning: Using default font, characters may not render correctly")
                font = ImageFont.load_default()
        return font
    except Exception as e:
        print(f"Error loading font: {e}")
        return None

def char_to_bitmap(char, font):
    """Convert a single character to a 12x12 bitmap."""
    # Create an image with white background
    img = Image.new('1', (BITMAP_SIZE, BITMAP_SIZE), color=1)
    draw = ImageDraw.Draw(img)
    
    # Draw character centered
    try:
        # Get bounding box to center the character
        bbox = draw.textbbox((0, 0), char, font=font)
        if bbox:
            char_width = bbox[2] - bbox[0]
            char_height = bbox[3] - bbox[1]
        else:
            char_width, char_height = BITMAP_SIZE, BITMAP_SIZE
        
        x = (BITMAP_SIZE - char_width) // 2
        y = (BITMAP_SIZE - char_height) // 2
        
        draw.text((x, y), char, font=font, fill=0)  # 0 = black, 1 = white
    except Exception as e:
        print(f"Warning: Could not render character {char}: {e}")
    
    return img

def bitmap_to_uint16_array(img):
    """Convert PIL image to array of uint16_t values (pixels in bits [15:4])."""
    pixels = list(img.getdata())
    array = []
    
    # Process 12 rows
    for row in range(BITMAP_SIZE):
        row_pixels = pixels[row * BITMAP_SIZE:(row + 1) * BITMAP_SIZE]
        
        # Pack 12 pixels into uint16_t
        # Bits [15:4] contain the pixels, bits [3:0] are 0
        # MSB (bit 15) = leftmost pixel
        uint16_val = 0
        for col in range(BITMAP_SIZE):
            pixel = row_pixels[col]
            # Invert: PIL uses 0=black, 1=white; we want 1=filled, 0=empty
            bit_val = 1 if pixel == 0 else 0
            # Shift into position: bit 15 = col 0, bit 4 = col 11
            uint16_val |= (bit_val << (15 - col))
        
        array.append(uint16_val)
    
    return array

def generate_c_array(characters, font):
    """Generate C array data for all characters."""
    c_code = []
    c_code.append("// Chinese character bitmaps for 12x12 display")
    c_code.append("// Format: uint16_t with pixels in bits [15:4], MSB = leftmost pixel")
    c_code.append("")
    
    for char, code_point in characters:
        c_code.append(f"// {char} (U+{code_point:04X})")
        c_code.append("const uint16_t char_bitmap[] = {")
        
        try:
            img = char_to_bitmap(char, font)
            array = bitmap_to_uint16_array(img)
            
            for i, val in enumerate(array):
                # Format as hex
                hex_str = f"0x{val:04X}"
                if i < len(array) - 1:
                    c_code.append(f"    {hex_str},")
                else:
                    c_code.append(f"    {hex_str}")
            
            c_code.append("};")
            c_code.append("")
        except Exception as e:
            print(f"Error generating bitmap for {char}: {e}")
            c_code.append(f"    // Error: {e}")
            c_code.append("};")
            c_code.append("")
    
    return "\n".join(c_code)

def main():
    """Main function."""
    print("Checking Pillow installation...")
    try:
        from PIL import Image
        print("Pillow is installed")
    except ImportError:
        print("Installing Pillow...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", "Pillow"])
        from PIL import Image
        print("Pillow installed successfully")
    
    print(f"\nGenerating bitmaps for {len(CHARACTERS)} characters...")
    print(f"Bitmap size: {BITMAP_SIZE}x{BITMAP_SIZE}")
    
    # Find font
    font_path = find_font()
    if font_path:
        print(f"Using font: {font_path}")
    
    # Create font
    font = create_font(font_path)
    if not font:
        print("Error: Could not create font")
        sys.exit(1)
    
    # Generate C array
    c_code = generate_c_array(CHARACTERS, font)
    
    # Output
    print("\n" + "="*70)
    print("GENERATED C ARRAY DATA:")
    print("="*70 + "\n")
    print(c_code)
    
    # Save to file
    output_file = "char_bitmap.h"
    with open(output_file, "w", encoding="utf-8") as f:
        f.write("#ifndef CHAR_BITMAP_H\n")
        f.write("#define CHAR_BITMAP_H\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(c_code)
        f.write("\n\n#endif // CHAR_BITMAP_H\n")
    
    print(f"\n{'='*70}")
    print(f"Output saved to: {output_file}")
    print("="*70)

if __name__ == "__main__":
    main()

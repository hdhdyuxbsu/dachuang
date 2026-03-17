# Chinese Character Bitmap Generator for ESP32 LCD Display

This project provides 12×12 pixel bitmap data for 17 Chinese characters commonly used in sensor display projects (temperature, humidity, soil, light, etc.).

## Files

### Generated Files
- **`char_bitmap.h`** - C header file with bitmap data for all 17 characters
  - Ready to include directly in your ESP32 project
  - Contains precomputed uint16_t arrays for each character
  - Includes lookup table and helper functions

### Scripts
- **`generate_bitmap.py`** - Main Python script to generate optimized bitmaps
  - Uses PIL/Pillow for rendering characters from system fonts
  - Searches for Chinese fonts (Microsoft YaHei, SimHei, SimSun, etc.)
  - Generates C header file with optimized bitmap data
  - **Usage**: `python generate_bitmap.py`

- **`generate_bitmap_simple.py`** - Simplified version using predefined data
  - No external font dependencies
  - Fast generation
  - Use if PIL/font installation is problematic

- **`generate_bitmap_v2.py`** - Enhanced version with better error handling

### Examples
- **`char_bitmap_example.c`** - Example code showing how to use the header file
  - Function prototypes for displaying characters
  - Integration examples with LCD libraries
  - Lookup and iteration examples

## Character Table

| Char | Unicode | Pinyin | English | Use Case |
|------|---------|--------|---------|----------|
| 气   | U+6C14  | qì     | Air     | Air quality sensor |
| 温   | U+6E29  | wēn    | Warm    | Temperature sensor |
| 湿   | U+6E7F  | shī    | Wet     | Humidity sensor |
| 度   | U+5EA6  | dù     | Degree  | Measurement unit |
| 土   | U+571F  | tǔ     | Soil    | Soil sensor |
| 光   | U+5149  | guāng  | Light   | Light sensor |
| 照   | U+7167  | zhào   | Shine   | Illumination |
| 压   | U+538B  | yā     | Press   | Pressure sensor |
| 告   | U+544A  | gào    | Tell    | Alert |
| 警   | U+8B66  | jǐng   | Warn    | Warning |
| 等   | U+7B49  | děng   | Etc     | "And more" |
| 待   | U+5F85  | dài    | Wait    | Waiting status |
| 无   | U+65E0  | wú     | None    | No data |
| 数   | U+6570  | shù    | Number  | Number/Count |
| 据   | U+636E  | jù     | Data    | Data indicator |
| 正   | U+6B63  | zhèng  | Right   | Normal/Correct |
| 常   | U+5E38  | cháng  | Normal  | Normal status |

## Bitmap Format

Each character bitmap is stored as an array of 12 `uint16_t` values (one per row):

```c
const uint16_t bitmap_char_6E29[] = {
    0x3E00,  // Row 0: bits [15:4] = pixel data, bits [3:0] = 0
    0x3600,  // Row 1
    // ... (10 more rows)
};
```

### Bit Layout
For each uint16_t:
- **Bits [15:4]** - 12 pixels (MSB = leftmost pixel)
- **Bits [3:0]** - Always 0 (reserved)

Example for a row: `0x3E00` binary = `0011 1110 0000 0000`
- Bit 15 = 0 (pixel 0)
- Bit 14 = 0 (pixel 1)
- Bit 13 = 1 (pixel 2)
- ...
- Bit 4 = 0 (pixel 11)
- Bits 3-0 = 0000

## API Reference

### Lookup Function
```c
const uint16_t* get_char_bitmap(uint32_t code_point);
```
Returns pointer to 12-element uint16_t array for the given Unicode code point, or NULL if not found.

### Lookup Table
```c
typedef struct {
    uint32_t code_point;
    const uint16_t *bitmap;
    const char *name;           // Pinyin name
} CharBitmap;

extern const CharBitmap char_bitmaps[];
```

## Using in Your ESP32 Project

### 1. Include the Header
```c
#include "char_bitmap.h"
```

### 2. Display a Character
```c
const uint16_t *bitmap = get_char_bitmap(0x6E29);  // 温

if (bitmap != NULL) {
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 12; col++) {
            // Extract pixel
            int pixel = (bitmap[row] >> (15 - col)) & 1;
            
            if (pixel) {
                // Draw pixel using your LCD driver
                lcd_draw_pixel(x + col, y + row, color);
            }
        }
    }
}
```

### 3. Display Multiple Characters
```c
// Display "温度" (Temperature)
const uint32_t text[] = {0x6E29, 0x5EA6};  // 温, 度

for (int i = 0; i < 2; i++) {
    const uint16_t *bitmap = get_char_bitmap(text[i]);
    if (bitmap) {
        // Draw at position (i * 14, 0)
        for (int row = 0; row < 12; row++) {
            for (int col = 0; col < 12; col++) {
                int pixel = (bitmap[row] >> (15 - col)) & 1;
                if (pixel) {
                    lcd_draw_pixel(i * 14 + col, row, WHITE);
                }
            }
        }
    }
}
```

## Generating Custom Bitmaps

### Option 1: Using System Fonts (Recommended)

```bash
python generate_bitmap.py
```

Requirements:
- Python 3.x
- PIL/Pillow: `pip install Pillow`
- Windows system fonts (automatically found):
  - `C:\Windows\Fonts\msyh.ttc` (Microsoft YaHei)
  - `C:\Windows\Fonts\simhei.ttf` (SimHei)
  - `C:\Windows\Fonts\simsun.ttc` (SimSun)
  - `C:\Windows\Fonts\NSimSun.ttf` (NSimSun)

The script will:
1. Auto-install Pillow if needed
2. Search for available Chinese fonts
3. Render each character at 11pt size
4. Convert to 12×12 bitmap
5. Output as `char_bitmap.h`

### Option 2: Using Precomputed Data

```bash
python generate_bitmap_simple.py
```

No dependencies needed, uses preset bitmap patterns.

## Technical Details

### Rendering Process
1. Load TrueType font from Windows system directory
2. Render character at 11pt to ensure it fits in 12×12 canvas
3. Center character on canvas (white background, black text)
4. Extract bitmap rows
5. Pack each row into uint16_t with pixels in bits [15:4]

### Memory Footprint
- Per character: 12 × uint16_t = 24 bytes
- All 17 characters: ~408 bytes
- Lookup table: ~200 bytes
- **Total: ~600 bytes** (fits comfortably in ESP32 flash)

### Display Integration
The bitmap format is designed for easy integration with:
- SPI/I2C LCD controllers
- GPIO-based bit-banging
- Custom LCD libraries
- ILI9341, ST7735, SSD1306, and similar displays

## Troubleshooting

### Script doesn't find fonts
- Windows: Copy fonts from `C:\Windows\Fonts\` to project directory
- Linux: Install fonts: `sudo apt install fonts-wqy-zenhei`
- macOS: Install Chinese fonts or use fallback Arial

### Characters look pixelated
- This is normal for 12×12 display
- Increase resolution or use larger font sizes
- Consider using 14×14 or 16×16 for better appearance

### Bitmap rendering seems wrong
- Verify font supports CJK characters
- Try different fonts in `generate_bitmap.py`
- Check that PIL/Pillow is correctly installed

## License

These bitmap files and example code are provided as-is for use in embedded systems projects.

## References

### Unicode Code Points
- U+6C14: 气 (qì) - air
- U+6E29: 温 (wēn) - warm/temperature
- U+6E7F: 湿 (shī) - wet/humid
- U+5EA6: 度 (dù) - degree/measure
- U+571F: 土 (tǔ) - soil/earth
- U+5149: 光 (guāng) - light
- U+7167: 照 (zhào) - shine/illuminate
- U+538B: 压 (yā) - press/pressure
- U+544A: 告 (gào) - inform/tell
- U+8B66: 警 (jǐng) - warn/alert
- U+7B49: 等 (děng) - equal/etc
- U+5F85: 待 (dài) - wait/treat
- U+65E0: 无 (wú) - none/without
- U+6570: 数 (shù) - number/count
- U+636E: 据 (jù) - according to/data
- U+6B63: 正 (zhèng) - correct/right
- U+5E38: 常 (cháng) - normal/usual

### Further Reading
- [Unicode Standard](https://unicode.org/)
- [PIL/Pillow Documentation](https://pillow.readthedocs.io/)
- [ESP32 Documentation](https://docs.espressif.com/)
- [CJK Unified Ideographs](https://en.wikipedia.org/wiki/CJK_Unified_Ideographs)

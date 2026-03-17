/**
 * QUICK REFERENCE: Using char_bitmap.h in your ESP32 project
 * 
 * This file shows the simplest way to use the Chinese character bitmaps
 */

#include "char_bitmap.h"

/**
 * EXAMPLE 1: Display a single character at position (x, y)
 * Assumes you have a function: void setPixel(int x, int y, uint16_t color);
 */
void displayChar(uint32_t codepoint, int x, int y, uint16_t color) {
    const uint16_t *bitmap = get_char_bitmap(codepoint);
    if (bitmap == NULL) return;
    
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 12; col++) {
            // Extract the pixel from bits [15:4]
            int pixel = (bitmap[row] >> (15 - col)) & 1;
            if (pixel) {
                setPixel(x + col, y + row, color);
            }
        }
    }
}

// Usage:
// displayChar(0x6E29, 0, 0, 0xFFFF);  // Display "温" at (0,0) in white

/**
 * EXAMPLE 2: Display a string of characters
 */
void displayString(const uint32_t *text, int count, int x, int y, 
                  uint16_t color, int spacing) {
    for (int i = 0; i < count; i++) {
        displayChar(text[i], x + i * (12 + spacing), y, color);
    }
}

// Usage:
// uint32_t text[] = {0x6E29, 0x5EA6};  // "温度"
// displayString(text, 2, 10, 10, 0xFFFF, 2);

/**
 * EXAMPLE 3: Common character codes for quick reference
 */
#define CHAR_AIR    0x6C14    // 气
#define CHAR_TEMP   0x6E29    // 温
#define CHAR_HUMID  0x6E7F    // 湿
#define CHAR_DEGREE 0x5EA6    // 度
#define CHAR_SOIL   0x571F    // 土
#define CHAR_LIGHT  0x5149    // 光
#define CHAR_SHINE  0x7167    // 照
#define CHAR_PRESS  0x538B    // 压
#define CHAR_INFORM 0x544A    // 告
#define CHAR_WARN   0x8B66    // 警
#define CHAR_ETC    0x7B49    // 等
#define CHAR_WAIT   0x5F85    // 待
#define CHAR_NONE   0x65E0    // 无
#define CHAR_NUM    0x6570    // 数
#define CHAR_DATA   0x636E    // 据
#define CHAR_OK     0x6B63    // 正
#define CHAR_NORM   0x5E38    // 常

/**
 * EXAMPLE 4: Combine characters into useful strings
 */

// Function to display common sensor readouts
void showSensorStatus() {
    // "温度" (Temperature)
    uint32_t temp[] = {CHAR_TEMP, CHAR_DEGREE};
    displayString(temp, 2, 0, 0, 0xFFFF, 2);
    
    // "湿度" (Humidity)
    uint32_t humid[] = {CHAR_HUMID, CHAR_DEGREE};
    displayString(humid, 2, 0, 14, 0xFFFF, 2);
    
    // "土壤" (Soil moisture)
    uint32_t soil[] = {CHAR_SOIL, CHAR_LIGHT};  // Approximate
    displayString(soil, 2, 0, 28, 0xFFFF, 2);
    
    // "压强" (Pressure)
    uint32_t press[] = {CHAR_PRESS};
    displayString(press, 1, 0, 42, 0xFFFF, 2);
}

/**
 * EXAMPLE 5: Iterate through all available characters
 */
void listAllCharacters() {
    printf("Available characters:\n");
    for (int i = 0; char_bitmaps[i].code_point != 0; i++) {
        const CharBitmap *cb = &char_bitmaps[i];
        printf("  U+%04X (%s)\n", cb->code_point, cb->name);
    }
}

/**
 * EXAMPLE 6: Memory efficient display (for small displays)
 * This version uses row-by-row rendering with minimal buffering
 */
void displayCharOptimized(uint32_t codepoint, int x, int y) {
    const uint16_t *bitmap = get_char_bitmap(codepoint);
    if (!bitmap) return;
    
    for (int row = 0; row < 12; row++) {
        uint16_t row_data = bitmap[row];
        
        // Process all pixels in this row
        for (int col = 0; col < 12; col++) {
            // Extract pixel bit by bit
            if (row_data & (1 << (15 - col))) {
                setPixel(x + col, y + row, WHITE);
            }
        }
    }
}

/**
 * EXAMPLE 7: For use with common ESP32 LCD libraries
 * (Uncomment as needed based on your library)
 */

/*
// For SSD1306 OLED
#include <Adafruit_SSD1306.h>

void displayCharOnOLED(Adafruit_SSD1306 &display, uint32_t cp, int x, int y) {
    const uint16_t *bitmap = get_char_bitmap(cp);
    if (!bitmap) return;
    
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 12; col++) {
            if ((bitmap[row] >> (15 - col)) & 1) {
                display.drawPixel(x + col, y + row, WHITE);
            }
        }
    }
}
*/

/*
// For ILI9341 TFT display
#include <TFT_eSPI.h>

void displayCharOnTFT(TFT_eSPI &tft, uint32_t cp, int x, int y, uint16_t color) {
    const uint16_t *bitmap = get_char_bitmap(cp);
    if (!bitmap) return;
    
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 12; col++) {
            if ((bitmap[row] >> (15 - col)) & 1) {
                tft.drawPixel(x + col, y + row, color);
            }
        }
    }
}
*/

/*
// For ST7735 display with direct SPI
void displayCharOnST7735(uint32_t cp, int x, int y, uint16_t color) {
    const uint16_t *bitmap = get_char_bitmap(cp);
    if (!bitmap) return;
    
    // Set drawing area (12x12)
    setWindow(x, y, x + 11, y + 11);
    
    for (int row = 0; row < 12; row++) {
        uint16_t row_data = bitmap[row];
        for (int col = 0; col < 12; col++) {
            uint16_t pixel_color = (row_data >> (15 - col)) & 1 ? color : 0;
            writeData(pixel_color);
        }
    }
}
*/

/**
 * EXAMPLE 8: Using with C++ class (for Arduino/ESP32)
 */
class CharacterDisplay {
private:
    int startX, startY;
    uint16_t fgColor, bgColor;
    
public:
    CharacterDisplay(int x, int y, uint16_t fg, uint16_t bg)
        : startX(x), startY(y), fgColor(fg), bgColor(bg) {}
    
    void setPosition(int x, int y) {
        startX = x;
        startY = y;
    }
    
    void setColors(uint16_t fg, uint16_t bg) {
        fgColor = fg;
        bgColor = bg;
    }
    
    void display(uint32_t codepoint) {
        const uint16_t *bitmap = get_char_bitmap(codepoint);
        if (!bitmap) return;
        
        for (int row = 0; row < 12; row++) {
            for (int col = 0; col < 12; col++) {
                int pixel = (bitmap[row] >> (15 - col)) & 1;
                // setPixel(startX + col, startY + row, pixel ? fgColor : bgColor);
            }
        }
    }
    
    void displayString(const uint32_t *text, int len, int spacing = 2) {
        for (int i = 0; i < len; i++) {
            int originalX = startX;
            startX += 12 + spacing;
            display(text[i]);
            startX = originalX;
        }
    }
};

// Usage:
// CharacterDisplay cd(0, 0, 0xFFFF, 0x0000);
// cd.display(0x6E29);  // 温

/**
 * EXAMPLE 9: Performance tips
 * 
 * 1. Cache pointers to frequently used characters
 * 2. Use DMA for batch pixel updates when possible
 * 3. Consider pre-scaling if you need larger characters
 * 4. Use bit operations for fast pixel extraction
 * 5. Build display updates off-screen first, then flush
 */

// Example: Pre-cache common strings
typedef struct {
    uint32_t chars[10];
    int count;
} CachedString;

const uint16_t *cachedBitmaps[10];

void cacheString(const uint32_t *text, int count) {
    for (int i = 0; i < count && i < 10; i++) {
        cachedBitmaps[i] = get_char_bitmap(text[i]);
    }
}

void displayCachedString(int x, int y, int count, uint16_t color) {
    for (int i = 0; i < count; i++) {
        if (cachedBitmaps[i] != NULL) {
            // Use pre-cached pointer (faster than lookup)
            for (int row = 0; row < 12; row++) {
                for (int col = 0; col < 12; col++) {
                    if ((cachedBitmaps[i][row] >> (15 - col)) & 1) {
                        setPixel(x + i * 14 + col, y + row, color);
                    }
                }
            }
        }
    }
}

/**
 * EXAMPLE 10: Reading bitmap data directly
 */
void printBitmapData(uint32_t codepoint) {
    const uint16_t *bitmap = get_char_bitmap(codepoint);
    if (!bitmap) {
        printf("Character 0x%04X not found\n", codepoint);
        return;
    }
    
    printf("Bitmap for U+%04X:\n", codepoint);
    for (int row = 0; row < 12; row++) {
        printf("  Row %2d: 0x%04X = ", row, bitmap[row]);
        
        // Print as binary
        for (int col = 0; col < 12; col++) {
            printf("%c", (bitmap[row] >> (15 - col)) & 1 ? '#' : '.');
        }
        printf("\n");
    }
}

// Usage: printBitmapData(0x6E29);
// Output shows visual representation of the character

#endif // Quick reference

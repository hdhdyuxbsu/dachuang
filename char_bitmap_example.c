/**
 * Example usage of Chinese character bitmaps for ESP32 LCD display
 * 
 * This example demonstrates how to use the char_bitmap.h header file
 * to display Chinese characters on an LCD display.
 */

#include <stdint.h>
#include "char_bitmap.h"

/**
 * Display a single Chinese character on LCD
 * 
 * @param cp: Unicode code point of the character
 * @param x: X coordinate (column) on display
 * @param y: Y coordinate (row) on display
 * @param color: Color to draw (format depends on your LCD driver)
 */
void display_char(uint32_t cp, int x, int y, uint16_t color) {
    const uint16_t *bitmap = get_char_bitmap(cp);
    
    if (bitmap == NULL) {
        // Character not found in bitmap table
        return;
    }
    
    // Draw each row of the 12x12 bitmap
    for (int row = 0; row < 12; row++) {
        uint16_t row_data = bitmap[row];
        
        // Process each pixel in the row (bits 15 down to 4)
        for (int col = 0; col < 12; col++) {
            // Extract pixel from bits [15:4]
            int bit_position = 15 - col;
            int pixel = (row_data >> bit_position) & 1;
            
            if (pixel) {
                // Draw pixel at (x + col, y + row)
                // This is where you'd call your LCD driver's pixel drawing function
                // Example: lcd_draw_pixel(x + col, y + row, color);
            }
        }
    }
}

/**
 * Display a string of Chinese characters
 * 
 * @param text: Array of Unicode code points
 * @param length: Number of characters in the string
 * @param x: Starting X coordinate
 * @param y: Starting Y coordinate
 * @param color: Color to draw
 * @param char_spacing: Space between characters
 */
void display_string(const uint32_t *text, int length, int x, int y, 
                   uint16_t color, int char_spacing) {
    for (int i = 0; i < length; i++) {
        display_char(text[i], x + i * (12 + char_spacing), y, color);
    }
}

// Example usage:
void example_display_sensors() {
    // Display "温度" (Temperature)
    uint32_t text1[] = {0x6E29, 0x5EA6};  // 温度
    display_string(text1, 2, 0, 0, 0xFFFF, 2);
    
    // Display "湿度" (Humidity)
    uint32_t text2[] = {0x6E7F, 0x5EA6};  // 湿度
    display_string(text2, 2, 0, 14, 0xFFFF, 2);
    
    // Display "土壤光照" (Soil Light)
    uint32_t text3[] = {0x571F, 0x5149};  // 土光
    display_string(text3, 2, 0, 28, 0xFFFF, 2);
    
    // Display "空气质量" (Air Quality)
    uint32_t text4[] = {0x6C14, 0x538B};  // 气压
    display_string(text4, 2, 0, 42, 0xFFFF, 2);
}

// Character lookup example
void find_character_example() {
    // Look up the bitmap for "正常" (Normal)
    const uint16_t *normal_bitmap = get_char_bitmap(0x6B63);  // 正
    const uint16_t *common_bitmap = get_char_bitmap(0x5E38);  // 常
    
    if (normal_bitmap && common_bitmap) {
        // Both characters found
        display_char(0x6B63, 0, 0, 0xFFFF);
        display_char(0x5E38, 12, 0, 0xFFFF);
    }
}

// Iterate through all available characters
void list_all_characters() {
    for (int i = 0; char_bitmaps[i].code_point != 0; i++) {
        const CharBitmap *cb = &char_bitmaps[i];
        // Process each character:
        // cb->code_point - Unicode code point
        // cb->bitmap - Pointer to 12x16_t bitmap data
        // cb->name - Pinyin/Name (e.g., "wen", "du", etc.)
    }
}

/**
 * Example for Arduino/ESP32 using common LCD libraries
 * Adjust this based on your specific LCD library
 */
void example_with_lcd_library() {
    // This is a pseudo-code example
    // Replace with your actual LCD library functions
    
    /*
    // Initialize LCD
    LCD.init();
    LCD.setColor(WHITE);
    
    // Display a Chinese character
    const uint16_t *bitmap = get_char_bitmap(0x6E29);  // 温
    
    for (int row = 0; row < 12; row++) {
        for (int col = 0; col < 12; col++) {
            int pixel = (bitmap[row] >> (15 - col)) & 1;
            if (pixel) {
                LCD.setPixel(100 + col, 50 + row, WHITE);
            }
        }
    }
    */
}

/**
 * Data structure for character constants
 */
typedef struct {
    uint32_t code_point;
    const char *description;
} CharInfo;

const CharInfo char_info[] = {
    {0x6C14, "气 - Air/Gas"},
    {0x6E29, "温 - Temperature"},
    {0x6E7F, "湿 - Humidity"},
    {0x5EA6, "度 - Degree"},
    {0x571F, "土 - Soil"},
    {0x5149, "光 - Light"},
    {0x7167, "照 - Shine"},
    {0x538B, "压 - Pressure"},
    {0x544A, "告 - Inform"},
    {0x8B66, "警 - Alert/Warn"},
    {0x7B49, "等 - Etc"},
    {0x5F85, "待 - Wait"},
    {0x65E0, "无 - None"},
    {0x6570, "数 - Number"},
    {0x636E, "据 - According/Data"},
    {0x6B63, "正 - Correct"},
    {0x5E38, "常 - Normal"},
    {0, NULL}
};

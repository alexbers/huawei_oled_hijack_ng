#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "oled.h"
#include "oled_font.h"

uint16_t secret_screen_buf[LCD_WIDTH*LCD_HEIGHT] = {};

struct lcd_screen secret_screen = {1, 128, 1, 128, LCD_WIDTH*LCD_HEIGHT*sizeof(uint16_t), secret_screen_buf};

void put_pixel(uint8_t x, uint8_t y, uint8_t red, uint8_t green, uint8_t blue) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }
    // truncate
    red = red >> 3;
    green = green >> 2;
    blue = blue >> 3;

    // color is: 5 bits blue, 6 bits green and 5 bits red
    uint16_t color = 0;
    color = (color << 0) | red;
    color = (color << 6) | green;
    color = (color << 5) | blue;

    // swap for little endian hosts
    color = (color >> 8) | (color << 8);
    // secret_screen_buf[x*128 + y] = color;
    secret_screen_buf[y*128 + x] = color;
}

void put_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t red, uint8_t green, uint8_t blue) {
    for (int p_y = y; p_y < y + h; p_y += 1) {
        for (int p_x = x; p_x < x + w; p_x += 1) {
            put_pixel(p_x, p_y, red, green, blue);
        }
    }
}

void put_text(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
              uint8_t red, uint8_t green, uint8_t blue, uint8_t *text,
              uint8_t *font, uint8_t font_bytes_per_char, uint8_t font_y_size, uint8_t* font_widths) {

    if (!text) {
        return;
    }

    int max_x = x + w;
    int max_y = y + h;

    int char_x = x;
    int char_y = y;

    while(*text) {
        uint8_t char_width = font_widths[*text];

        if(*text == '\n') {
            char_x = x;
            char_y += font_y_size;
            text += 1;
            continue;
        }

        for(int letter_y = 0; letter_y < font_y_size; letter_y += 1) {
            for (int letter_x = 0; letter_x < char_width; letter_x += 1) {
                int cur_x = char_x + letter_x;
                int cur_y = char_y + letter_y;

                if(cur_x >= max_x || cur_y >= max_y) {
                    continue;
                }

                int letter_idx = letter_y * font_bytes_per_char + (letter_x / 8);

                int bit_idx = 7 - (letter_x % 8);
                if(font[*text * font_bytes_per_char*font_y_size + letter_idx] & (0x1 << bit_idx)) {
                    put_pixel(cur_x, cur_y, red, green, blue);
                }
            }
        }

        char_x += char_width;

        text += 1;
    }
}

void put_small_text(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                    uint8_t red, uint8_t green, uint8_t blue, uint8_t *text) {

    put_text(x, y, w, h, red, green, blue, text, (uint8_t*) SMALL_FONT, SMALL_FONT_BYTES_PER_CHAR, SMALL_FONT_SIZE, SMALL_FONT_WIDTHS);
}

void put_large_text(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                    uint8_t red, uint8_t green, uint8_t blue, uint8_t *text) {

    put_text(x, y, w, h, red, green, blue, text, (uint8_t*) LARGE_FONT, LARGE_FONT_BYTES_PER_CHAR, LARGE_FONT_SIZE, LARGE_FONT_WIDTHS);
}

void put_raw_buffer(uint8_t* from, uint32_t len) {
    if (len > LCD_HEIGHT * LCD_WIDTH * sizeof(uint16_t)) {
        len = LCD_HEIGHT * LCD_WIDTH * sizeof(uint16_t);
    }

    memcpy(secret_screen_buf, from, len);
}
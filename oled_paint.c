#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>

#include "oled.h"
#include "oled_font.h"

uint8_t is_small_screen = 0;
uint8_t lcd_width = LCD_MAX_WIDTH;
uint8_t lcd_height = LCD_MAX_HEIGHT;

uint16_t secret_screen_buf[LCD_MAX_WIDTH*LCD_MAX_HEIGHT] = {};

struct lcd_screen secret_screen = {1, 128, 1, 128, LCD_MAX_WIDTH*LCD_MAX_HEIGHT*sizeof(uint16_t), secret_screen_buf};

void switch_to_small_screen_mode() {
    if (is_small_screen == 1) {
        return;
    }
    is_small_screen = 1;
    lcd_width = 128;
    lcd_height = 64;

    secret_screen.sx = 0;
    secret_screen.height = lcd_height;
    secret_screen.sy = 0;
    secret_screen.width = lcd_width;
    const int BITS_IN_BYTE = 8;
    secret_screen.buf_len = (lcd_width * lcd_height) / BITS_IN_BYTE;
}

void put_small_screen_pixel(uint8_t x, uint8_t y, uint8_t iswhite) {
    if (x >= lcd_width || y >= lcd_height) {
        return;
    }

    const int BITS_IN_BYTE = 8;
    uint16_t offset = (y * lcd_width + x) / (BITS_IN_BYTE * sizeof(uint16_t));

    uint8_t bit_num = 0;
    uint8_t remainder = x % (BITS_IN_BYTE * sizeof(uint16_t));

    if (remainder < 8) {
        bit_num = 7 - remainder;
    } else {
        bit_num = 15 - (remainder-8);
    }

    if (iswhite) {
        secret_screen_buf[offset] |= 1<<bit_num;
    } else {
        secret_screen_buf[offset] &= ~(1<<bit_num);
    }
}

void put_pixel(uint8_t x, uint8_t y, uint8_t red, uint8_t green, uint8_t blue) {
    if (x >= lcd_width || y >= lcd_height) {
        return;
    }
    if (is_small_screen) {
        if (red || green || blue) {
            put_small_screen_pixel(x, y, 1);
        } else {
            put_small_screen_pixel(x, y, 0);
        }
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
        uint8_t char_idx = get_char_idx_and_go_next(&text);
        uint8_t char_width = font_widths[char_idx];

        if(char_idx == '\n') {
            char_x = x;
            char_y += font_y_size;
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
                if(font[char_idx * font_bytes_per_char*font_y_size + letter_idx] & (0x1 << bit_idx)) {
                    put_pixel(cur_x, cur_y, red, green, blue);
                }
            }
        }

        char_x += char_width;
    }
}

uint32_t get_bytes_num_fit_by_width(uint8_t x, uint8_t w, uint8_t *text, uint8_t* font_widths) {
    uint8_t *start_text = text;
    uint8_t *last_good_text = start_text;
    while(x < w) {
        last_good_text = text;
        if(!*text) {
            break;
        }
        uint8_t char_idx = get_char_idx_and_go_next(&text);
        x += font_widths[char_idx];
    }
    return last_good_text - start_text;
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
    if (len > lcd_height * lcd_width * sizeof(uint16_t)) {
        len = lcd_height * lcd_width * sizeof(uint16_t);
    }

    memcpy(secret_screen_buf, from, len);
}

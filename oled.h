#ifndef OLED_H
#define OLED_H

#define SUBSYSTEM_GPIO 21002
#define UI_MENU_EXIT 1006

#define BUTTON_POWER 8
#define BUTTON_LONGPOWER 22
#define BUTTON_LONGLONGPOWER 4
#define BUTTON_MENU 9
#define BUTTON_LONGMENU 15

#define LCD_WIDTH 128
#define LCD_HEIGHT 128

#define LED_ON 100
#define LED_DIM 101
#define LED_SLEEP 102

struct lcd_screen {
    uint32_t sx;
    uint32_t height;
    uint32_t sy;
    uint32_t width;
    uint32_t buf_len;
    uint16_t *buf;
};

struct led_widget {
    char name[256];
    int32_t lcd_sleep_ms;
    void (*init)();
    void (*deinit)();
    void (*paint)();
    void (*power_key_handler)();
    void (*menu_key_handler)();
    uint32_t parent_idx;
};


#endif

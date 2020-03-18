#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>

#include "oled.h"
#include "oled_pictures.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

extern void lcd_refresh_screen(struct lcd_screen*);
extern int lcd_control_operate(int);
extern int notify_handler_async(int subsystemid, int action, int subaction);

extern void put_pixel(uint8_t x, uint8_t y, uint8_t red, uint8_t green, uint8_t blue);
extern void put_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t red, uint8_t green, uint8_t blue);
extern void put_small_text(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t red, uint8_t green, uint8_t blue, char *text);
extern void put_large_text(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t red, uint8_t green, uint8_t blue, char *text);

extern uint32_t (*timer_create_ex)(uint32_t, uint32_t, void (*)(), uint32_t);
extern uint32_t (*timer_delete_ex)(uint32_t);

int create_process(char* command, void (*finish_callback)(int, char *));
void destroy_process();
void destroy_process_pooler();

extern struct lcd_screen secret_screen;

uint32_t active_widget = 0;

// these are decared in the ends
struct led_widget widgets[];
const uint32_t WIDGETS_SIZE;

uint32_t lcd_timer = 0;
uint32_t lcd_state = LED_ON;

void lcd_turn_on() {
    if (lcd_state != LED_ON) {
        lcd_state = LED_ON;
        lcd_control_operate(lcd_state);
    }
}

void lcd_turn_off() {
    if (lcd_state != LED_SLEEP) {
        lcd_state = LED_SLEEP;
        lcd_control_operate(lcd_state);
    }
}

void reschedule_lcd_timer() {
    if (lcd_timer) {
        timer_delete_ex(lcd_timer);
        lcd_timer = 0;
    }

    lcd_timer = timer_create_ex(widgets[active_widget].lcd_sleep_ms, 0, lcd_turn_off, 0);
}

void clear_screen() {
    put_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0, 0);
}

void repaint() {
    clear_screen();
    if(widgets[active_widget].paint) {
        widgets[active_widget].paint();
    }
    lcd_refresh_screen(&secret_screen);
}

void enter_widget(uint32_t num) {
    if (num >= WIDGETS_SIZE) {
        fprintf(stderr, "Attempted to switch to non-existing widget %d\n", num);
        return;
    }

    // do not deinit active widget for better user experience
    active_widget = num;
    widgets[active_widget].init();
    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}

void leave_widget() {
    if (widgets[active_widget].deinit) {
        widgets[active_widget].deinit();
    }

    destroy_process_pooler();
    destroy_process();

    active_widget = widgets[active_widget].parent_idx;
    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}

void reset_widgets() {
    for(uint32_t widget = 0; widget < WIDGETS_SIZE; widget += 1) {
        if(widgets[widget].deinit) {
            widgets[widget].deinit();
        }
    }
    active_widget = 0;
    enter_widget(active_widget);
}

void dispatch_power_key() {
    if(widgets[active_widget].power_key_handler) {
        widgets[active_widget].power_key_handler();
    }

    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}

void dispatch_menu_key() {
    if(widgets[active_widget].menu_key_handler) {
        widgets[active_widget].menu_key_handler();
    }

    reschedule_lcd_timer();
    lcd_turn_on();
    repaint();
}


// ---------------------------------- THE MAIN WIDGET ---------------------

uint8_t main_current_item;

char *main_lines[] = {
    "<Back>",
    "Signal info",
    "Radio mode",
    "Matrix",
    "Photo",
    "Snake",
};

char main_lines_num = 6;

void main_init() {
    main_current_item = 0;
}

void main_paint() {
    const int lines_per_page = 7;

    const int first_page = main_current_item / lines_per_page * lines_per_page;

    for (int i = 0; i < lines_per_page && first_page + i < main_lines_num; i += 1) {
        char* cur_line = main_lines[first_page + i];

        if (first_page + i == main_current_item) {
            put_small_text(5, 10 + i * 15, LCD_WIDTH, LCD_HEIGHT, 255,0,255, "#");
        }
        put_small_text(20, 10 + i * 15, LCD_WIDTH, LCD_HEIGHT, 255,255,255, cur_line);
    }

}

void main_power_key_pressed() {
    switch(main_current_item) {
        case 0: 
            notify_handler_async(SUBSYSTEM_GPIO, BUTTON_LONGMENU, 0);
            break;
        case 1:
            enter_widget(1);
            break;
        case 2:
            enter_widget(2);
            break;
        case 3:
            enter_widget(3);
            break;
        case 4:
            enter_widget(4);
            break;
        case 5:
            enter_widget(5);
            break;
    }
}

void main_menu_key_pressed() {
    main_current_item += 1;
    if (main_current_item >= main_lines_num) {
        main_current_item = 0;
    }
}

// ---------------------------------- MOBILE SIGNAL --------------------------

uint32_t mobile_timer = 0;

uint8_t mobile_graph_mode = 1;

int32_t mobile_rssi = 0;
int32_t mobile_rsrq = 0;
int32_t mobile_rsrp = 0;
int32_t mobile_sinr = 0;
int32_t mobile_rscp = 0;
int32_t mobile_ecio = 0;
int32_t mobile_ul_bw = 0;
int32_t mobile_dl_bw = 0;

const int32_t MAX_LAST_RSSI = 128;
int32_t last_rssi[MAX_LAST_RSSI] = {};

void mobile_process_callback(int good, char *buf) {
    if (!good) {
        return;
    }

    mobile_rssi = mobile_rsrq = mobile_rsrp = mobile_sinr = mobile_rscp = mobile_ecio = 0;
    mobile_ul_bw = mobile_dl_bw = 0;

    int offset = 0;

    while (buf[offset]) {
        int32_t val;

        if (sscanf(&buf[offset], "<rssi>&gt;=%ddBm</rssi>", &val) == 1) {
            mobile_rssi = val;
        } else if (sscanf(&buf[offset], "<rssi>%ddBm</rssi>", &val) == 1) {
            mobile_rssi = val;
        } else if (sscanf(&buf[offset], "<rsrq>%ddB</rsrq>", &val) == 1) {
            mobile_rsrq = val;
        } else if (sscanf(&buf[offset], "<rsrp>%ddBm</rsrp>", &val) == 1) {
            mobile_rsrp = val;
        } else if (sscanf(&buf[offset], "<sinr>%ddB</sinr>", &val) == 1) {
            mobile_sinr = val;
        } else if (sscanf(&buf[offset], "<rscp>%ddBm</rscp>", &val) == 1) {
            mobile_rscp = val;
        } else if (sscanf(&buf[offset], "<ecio>%ddB</ecio>", &val) == 1) {
            mobile_ecio = val;
        } else if (sscanf(&buf[offset], "<ulbandwidth>%dMHz</ulbandwidth>", &val) == 1) {
            mobile_ul_bw = val;
        } else if (sscanf(&buf[offset], "<dlbandwidth>%dMHz</dlbandwidth>", &val) == 1) {
            mobile_dl_bw = val;
        }

        while(buf[offset] != 0 && buf[offset] != '\n') {
            offset += 1;
        }

        if(buf[offset] == '\n') {
            offset += 1;
        }
    }

    if (mobile_rssi) {
        for(int i = MAX_LAST_RSSI - 1; i > 0; i -= 1) {
            last_rssi[i] = last_rssi[i - 1];
        }
        last_rssi[0] = mobile_rssi;
    }

    repaint();
}

void update_measurements() {
    create_process("/online/web_hook_client device signal 1 1", mobile_process_callback);
}

void mobile_signal_init() {
    mobile_rssi = mobile_rsrq = mobile_rsrp = mobile_sinr = mobile_rscp = mobile_ecio = 0;
    mobile_ul_bw = mobile_dl_bw = 0;

    mobile_graph_mode = 1;

    for (int i = 0; i < MAX_LAST_RSSI; i += 1) {
        last_rssi[i] = 0;
    }

    update_measurements();
    mobile_timer = timer_create_ex(1000, 1, update_measurements, 0);
}

void mobile_signal_deinit() {
    if (mobile_timer) {
        timer_delete_ex(mobile_timer);
        mobile_timer = 0;
    }
}

void mobile_print_val_colorized(int x, int y, int thresh1, int thresh2, int thresh3, int val, char* addition) {
    char buf[256];
    uint8_t r, g, b;

    snprintf(buf, 256, "%d", val);
    strcat(buf, addition);

    if (val > thresh1) {
        r = 0; g = 255; b = 0;
    } else if (val > thresh2) {
        r = 188; g = 255; b = 0;
    } else if (val > thresh3) {
        r = 255; g = 188; b = 0;
    } else {
        r = 255; g = 0; b = 0;
    }

    put_large_text(x, y, LCD_WIDTH, LCD_HEIGHT, r, g, b, buf);
}

void mobile_put_pixel_colorized(int x, int y, int thresh1, int thresh2, int thresh3, int val) {
    uint8_t r, g, b;

    if (val > thresh1) {
        r = 0; g = 255; b = 0;
    } else if (val > thresh2) {
        r = 188; g = 255; b = 0;
    } else if (val > thresh3) {
        r = 255; g = 188; b = 0;
    } else {
        r = 255; g = 0; b = 0;
    }

    put_pixel(x, y, r, g, b);
}

void mobile_signal_text_paint() {
    put_small_text(8, 8, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "RSSI");
    mobile_print_val_colorized(53, 4, -65, -75, -85, mobile_rssi, "dBm");

    if (mobile_rsrp != 0) {
        put_small_text(8, 28, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "RSRP");
        mobile_print_val_colorized(53, 24, -84, -102, -111, mobile_rsrp, "dBm");
    } else if (mobile_rscp != 0) {
        put_small_text(8, 28, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "RSCP");
        mobile_print_val_colorized(53, 24, -65, -75, -85, mobile_rscp, "dBm");
    }

    if (mobile_rsrq != 0) {
        put_small_text(8, 48, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "RSRQ");
        mobile_print_val_colorized(53, 44, -5, -9, -12, mobile_rsrq, "dB");
    } else if (mobile_ecio != 0) {
        put_small_text(8, 48, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "EC/IO");
        mobile_print_val_colorized(53, 44, -6, -9, -12, mobile_ecio, "dB");
    }

    if (mobile_sinr != 0) {
        put_small_text(8, 68, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "SINR");
        mobile_print_val_colorized(53, 64, 12, 10, 7, mobile_sinr, "dB");
    }

    if (mobile_ul_bw != 0) {
        put_small_text(8, 88, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "UL BW");
        mobile_print_val_colorized(53, 84, 12, 10, 7, mobile_ul_bw, "Mhz");
    }

    if (mobile_dl_bw != 0) {
        put_small_text(8, 108, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "DL BW");
        mobile_print_val_colorized(53, 104, 12, 10, 7, mobile_dl_bw, "Mhz");
    }
}

uint8_t mobile_val_to_y(int32_t val) {
        if (val > -51) {
            val = -51;
        }

        if (val < -105) {
            val = -105;
        }

        uint8_t y = -(val + 51) * 2;
        return y;
}

uint32_t mobile_y_to_val(uint8_t y) {
    return -y/2 - 51;
}


void mobile_signal_graph_paint() {
    put_small_text(8, 113, LCD_WIDTH, LCD_HEIGHT, 255, 255, 255, "RSSI");
    mobile_print_val_colorized(53, 109, -65, -75, -85, mobile_rssi, "dBm");

    uint32_t prev_val = 0;

    for (int i = 0; i < MAX_LAST_RSSI-1; i += 1) {
        uint8_t x = LCD_WIDTH - i - 1;

        if (last_rssi[i] == 0 || last_rssi[i + 1] == 0) {
            continue;
        }

        uint8_t y_from = MIN(mobile_val_to_y(last_rssi[i]), mobile_val_to_y(last_rssi[i+1]));
        uint8_t y_to = MAX(mobile_val_to_y(last_rssi[i]), mobile_val_to_y(last_rssi[i+1]));

        if ((y_to - y_from) > 1) {
            // more smooth lines
            y_from += 1;
        }
        if ((y_to - y_from) > 2) {
            // more smooth lines
            y_to -= 1;
        }

        for (int y = y_from; y <= y_to; y += 1) {
            mobile_put_pixel_colorized(x, y, -65, -75, -85, mobile_y_to_val(y));
        }
    }
}


void mobile_signal_paint() {
    if (mobile_graph_mode) {
        mobile_signal_graph_paint();
    } else {
        mobile_signal_text_paint();
    }
}

void mobile_switch_mode() {
    mobile_graph_mode = !mobile_graph_mode;
    repaint();
}

// -------------------------------------- RADIO MODE -------------------------

uint8_t radio_cur_item = 0;

const int MAXMENUITEMS = 20;
char menu_items[MAXMENUITEMS][256] = {};

void radio_mode_process_callback(int isgood, char* buf) {

}

void radio_mode_init() {
    radio_cur_item = 0;
    create_process("/online/radio_mode.sh", mobile_process_callback);
    for (int i = 0; i < MAXMENUITEMS; i += 1) {
        menu_items[i][0] = 0;
    }
    strcpy(menu_items[0], "item:<back>");
}

void radio_mode_paint() {

}

void radio_mode_menu_key_pressed() {

}

void radio_mode_power_key_pressed() {

}


// -------------------------------------- MATRIX -----------------------------

uint32_t matrix_timer = 0;

struct {
    int x;
    int y;
    char str[16+1];
    uint8_t green;
    int speed; // pixels per tick
} matrix_seq[20];

void matrix_paint() {
    for (int i = 0; i < 20; i += 1) {
        put_small_text(matrix_seq[i].x, matrix_seq[i].y, LCD_WIDTH, 255, 0, matrix_seq[i].green, 0, matrix_seq[i].str);
    }
}

void matrix_tick() {
    // fprintf(stderr, "tick\n");
    for (int i = 0; i < 20; i += 1) {
        matrix_seq[i].y = (matrix_seq[i].y + matrix_seq[i].speed) % 256;
        if(matrix_seq[i].y > LCD_HEIGHT && matrix_seq[i].y < LCD_HEIGHT + 15) {
            matrix_seq[i].x = rand() % LCD_WIDTH;
        }
    }
    repaint();
}

void matrix_init() {
    matrix_timer = timer_create_ex(50, 1, matrix_tick, 0);

    for (int i = 0; i < 20; i += 1) {
        matrix_seq[i].x = rand() % 128;
        matrix_seq[i].y = rand() % 128;
        int len = 8 + rand() % 8;
        for (int j = 0; j < 16; j += 1) {
            if (j%2 == 1) {
                matrix_seq[i].str[j] = '\n';
            } else {
                if (j < len) {
                    matrix_seq[i].str[j] = ' ' + rand() % 15;
                } else {
                    matrix_seq[i].str[j] = ' ';
                }
            }
        }
        matrix_seq[i].green = rand() % 256;
        matrix_seq[i].speed = 2 + rand() % 8;

    }
}

void matrix_deinit() {
    if(matrix_timer) {
        timer_delete_ex(matrix_timer);
        matrix_timer = 0;
    }
}

// ---------------------------- PHOTO -------------------------------

uint8_t photo_index = 0;

void photo_init() {
    photo_index = 0;
}

void photo_next() {
    photo_index = (photo_index + 1) % PICTURES_NUM;
    repaint();
}

void photo_paint() {
    for(int y = 0; y < LCD_WIDTH; y += 1) {
        for (int x = 0; x < LCD_HEIGHT; x += 1) {
            uint8_t *offset = pictures[photo_index] + (y * LCD_WIDTH + x) * 3;
            put_pixel(x, y, offset[0], offset[1], offset[2]);
        }
    }
}

// ---------------------------- SNAKE -------------------------------

// 0 - right, 1 - up, 2 - left, 3 - down
uint8_t snake_direction = 0;
int snake_score = 0;

uint32_t snake_len = 0;
const uint32_t SNAKE_FIELD_WIDTH = 16;
const uint32_t SNAKE_FIELD_HEIGHT = 14;
const uint32_t SNAKE_MAX_LEN = 16 * 16;
const uint32_t SNAKE_SQUARE_SIZE = 8;
const uint32_t SNAKE_SCORE_SPACE = 16;
int snake_dead = 0;

struct snake_point {
    uint8_t x;
    uint8_t y;
} snake[SNAKE_MAX_LEN], goal_pos;

uint32_t snake_timer = 0;

void snake_place_goal() {
    const int max_tries = SNAKE_MAX_LEN * 10;
    for (int i = 0; i < max_tries; i += 1) {
        int8_t pos_good = 1;
        goal_pos.x = rand() % SNAKE_FIELD_WIDTH;
        goal_pos.y = rand() % SNAKE_FIELD_HEIGHT;

        for (uint32_t j = 0; j < snake_len; j += 1) {
            if(goal_pos.x == snake[j].x && goal_pos.y == snake[j].y) {
                pos_good = 0;
                break;
            }
        }

        if(pos_good) {
            return;
        }
    }
}

void snake_tick() {
    struct snake_point next_head;

    if(snake_direction == 0) {
        next_head.x = snake[0].x + 1;
        next_head.y = snake[0].y;
    } else if (snake_direction == 1) {
        next_head.x = snake[0].x;
        next_head.y = snake[0].y - 1;
    } else if (snake_direction == 2) {
        next_head.x = snake[0].x - 1;
        next_head.y = snake[0].y;
    } else if (snake_direction == 3) {
        next_head.x = snake[0].x;
        next_head.y = snake[0].y + 1;
    }

    for (uint32_t i = 0; i < snake_len; i += 1) {
        if (next_head.x == snake[i].x && next_head.y == snake[i].y) {
            snake_dead = 1;
        }
    }

    if (next_head.x < 0 || next_head.x >= SNAKE_FIELD_WIDTH || 
        next_head.y < 0 || next_head.y >= SNAKE_FIELD_HEIGHT) {
        snake_dead = 1;
    }

    if (snake_dead) {
        repaint();
        return;
    }

    for (int i = snake_len; i > 0; i -= 1) {
        snake[i].x = snake[i-1].x;
        snake[i].y = snake[i-1].y;
    }

    snake[0].x = next_head.x;
    snake[0].y = next_head.y;


    uint8_t goal_taken = (next_head.x == goal_pos.x) && (next_head.y == goal_pos.y);
    if (goal_taken && snake_len < SNAKE_MAX_LEN - 1) {
        snake_len += 1;
        snake_score += 1;
        snake_place_goal();
    }

    repaint();
}

void snake_init() {
    snake_dead = 0;
    snake_direction = 0;
    snake_score = 0;
    snake_len = 3;
    snake[0].x = 5; snake[0].y = 8;
    snake[1].x = 4; snake[1].y = 8;
    snake[2].x = 3; snake[2].y = 8;
    snake_place_goal();

    snake_timer = timer_create_ex(200, 1, snake_tick, 0);
}

void snake_deinit() {
    if(snake_timer) {
        timer_delete_ex(snake_timer);
        snake_timer = 0;
    }
}

void snake_paint() {
    char buf[64] = {0};
    if (snake_dead) {
        snprintf(buf, 32, "Score: %d, game over", snake_score);
    } else {
        snprintf(buf, 32, "Score: %d", snake_score);
    }

    put_small_text(5, 1, LCD_WIDTH, SNAKE_SCORE_SPACE, 255, 255, 255, buf);
    put_rect(0, SNAKE_SCORE_SPACE - 1, LCD_WIDTH, 1, 255, 255, 255);
    
    for (uint32_t i = 0; i < snake_len; i += 1) {
        put_rect(snake[i].x * SNAKE_SQUARE_SIZE, SNAKE_SCORE_SPACE + snake[i].y * SNAKE_SQUARE_SIZE,
                 SNAKE_SQUARE_SIZE, SNAKE_SQUARE_SIZE, 255, 255, 255);
    }
    put_rect(goal_pos.x * SNAKE_SQUARE_SIZE, SNAKE_SCORE_SPACE + goal_pos.y * SNAKE_SQUARE_SIZE,
             SNAKE_SQUARE_SIZE, SNAKE_SQUARE_SIZE, 255, 0, 0);
}

void snake_turn_left() {
    if (snake_dead) {
        leave_widget();
    }
    snake_direction = (snake_direction + 1) % 4;
}

void snake_turn_right() {
    if (snake_dead) {
        leave_widget();
    }
    snake_direction = (snake_direction + 4 - 1) % 4;
}

struct led_widget widgets[] = {
    {
        .name = "main",
        .lcd_sleep_ms = 20000,
        .init = main_init,
        .deinit = 0,
        .paint = main_paint,
        .menu_key_handler = main_menu_key_pressed,
        .power_key_handler = main_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "mobile signal",
        .lcd_sleep_ms = 600000,
        .init = mobile_signal_init,
        .deinit = mobile_signal_deinit,
        .paint = mobile_signal_paint,
        .menu_key_handler = mobile_switch_mode,
        .power_key_handler = leave_widget,
        .parent_idx = 0
    },    
    {
        .name = "radio mode",
        .lcd_sleep_ms = 15000,
        .init = radio_mode_init,
        .deinit = 0,
        .paint = radio_mode_paint,
        .menu_key_handler = radio_mode_menu_key_pressed,
        .power_key_handler = radio_mode_power_key_pressed,
        .parent_idx = 0
    },
    {
        .name = "matrix",
        .lcd_sleep_ms = 300000,
        .init = matrix_init,
        .deinit = matrix_deinit,
        .paint = matrix_paint,
        .menu_key_handler = leave_widget,
        .power_key_handler = leave_widget,
        .parent_idx = 0
    },    
    {
        .name = "photo",
        .lcd_sleep_ms = 15000,
        .init = photo_init,
        .deinit = 0,
        .paint = photo_paint,
        .menu_key_handler = photo_next,
        .power_key_handler = leave_widget,
        .parent_idx = 0
    },    
    {
        .name = "snake",
        .lcd_sleep_ms = 20000,
        .init = snake_init,
        .deinit = snake_deinit,
        .paint = snake_paint,
        .menu_key_handler = snake_turn_left,
        .power_key_handler = snake_turn_right,
        .parent_idx = 0
    },    
};

const uint32_t WIDGETS_SIZE = 6;
CC = armv7a-linux-androideabi16-clang

all: oled_hijack.so

oled_hijack.so: oled_hijack.c oled_paint.c oled_widgets.c oled.h oled_font.h oled_pictures.h
	$(CC) -shared -ldl -fPIC -O2 -s -o oled_hijack.so oled_hijack.c oled_paint.c oled_widgets.c

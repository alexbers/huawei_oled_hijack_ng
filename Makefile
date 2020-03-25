CC = armv7a-linux-androideabi16-clang

all: oled_hijack.so web_hook.so web_hook_client

oled_hijack.so: oled_hijack.c oled_paint.c oled_widgets.c oled_process.c oled.h oled_font.h
	$(CC) -W -shared -ldl -fPIC -O2 -s -o oled_hijack.so oled_hijack.c oled_paint.c oled_process.c oled_widgets.c

web_hook.so: web_hook.c
	$(CC) -shared -ldl -fPIC -O2 -s -pthread -DHOOK -DSOCK_NAME='"/var/webhook"' -o web_hook.so web_hook.c

web_hook_client: web_hook.c
	$(CC) -fPIC -O2 -DCLIENT -DSOCK_NAME='"/var/webhook"' -s -o web_hook_client web_hook.c

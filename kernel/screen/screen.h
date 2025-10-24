#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t DEFAULT_FG;
    uint32_t DEFAULT_BG;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t scale_nominator;
    uint32_t scale_denominator;
} Window;


void fb_init(void* mb_info_addr);
void fb_clear(Window *win);
void fb_write(Window *win, const char *s);
void fb_write_ansi(Window *win, const char *s);
void fb_put_char(Window *win, char c);
void fb_removechar(Window *win);
void fb_set_scale(Window *win, uint32_t nom, uint32_t denom);
void fb_write_dec(Window *win, uint64_t num);
void fb_write_hex(Window *win, uint64_t num);
void init_fullscreen(Window* win);
void fb_window_border(Window *win, char* title, uint32_t color);
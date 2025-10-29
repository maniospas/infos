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
    uint32_t no_bg_mode;
    uint32_t scroll_limit; // 0 for infinite scrolling
    uint32_t accumulated_scroll_limit;
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
void fb_window_border(Window *win, char* title, uint32_t color, int appid);
void fb_bar(Window* win, long int value, long int max_value, size_t width);
void fb_image(Window *win, const void *data, size_t size, size_t target_width, size_t target_height);
void fb_putpixel(int x, int y, uint64_t color);
void fb_clearline(Window *win, size_t line_start_cursor_x);
void fb_scrollbar(Window *win, long pos, long size); // pos and size are number 0-10000 corresponding to [0,1]
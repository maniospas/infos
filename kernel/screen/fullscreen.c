#include "screen.h"
#include "../multiboot2.h"
#include <stdint.h>
#include <stddef.h>
#include "vga.h"

#pragma GCC target("no-mmx,no-sse,no-sse2")
extern uint32_t fb_width, fb_height, margin;

void init_fullscreen(Window *fullscreen) {
    fullscreen->x = 0;
    fullscreen->y = 0;
    fullscreen->width = fb_width;
    fullscreen->height = fb_height;
    fullscreen->DEFAULT_FG = 0xEEEEEE;
    fullscreen->DEFAULT_BG = 0x1F1F1F;
    fullscreen->cursor_x = margin;
    fullscreen->cursor_y = margin;
    fullscreen->fg_color = fullscreen->DEFAULT_FG;
    fullscreen->bg_color = fullscreen->DEFAULT_BG;
    fullscreen->scale_nominator = 1;
    fullscreen->scale_denominator = 1;
}
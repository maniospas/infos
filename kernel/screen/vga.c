#include "vga.h"
#include "../string.h" // for simple atoi if available (or write your own)
#include <stddef.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDRESS 0xB8000

static volatile uint16_t *vga_buffer = (uint16_t*) VGA_ADDRESS;
static size_t cursor_row = 0;
static size_t cursor_col = 0;
static uint8_t color = 0x07; // light gray on black

// === Internal helpers ===
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

// === Color ===
void vga_set_color(uint8_t fg, uint8_t bg) { color = (bg << 4) | (fg & 0x0F); }
uint8_t vga_get_color(void) { return color; }
void vga_set_raw_color(uint8_t c) { color = c; }

// === Cursor & screen ops ===
void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', color);
    cursor_row = cursor_col = 0;
}

void vga_move_cursor(size_t row, size_t col) {
    if (row < VGA_HEIGHT) cursor_row = row;
    if (col < VGA_WIDTH) cursor_col = col;
}

void vga_removechar(void) {
    if (cursor_col > 0) cursor_col--;
    else if (cursor_row > 0) { cursor_row--; cursor_col = VGA_WIDTH - 1; }
    else return;
    vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(' ', color);
}

// === Character output ===
void vga_put_char(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    }
    else if (c == '\r') {
        cursor_col = 0;
    }
    else {
        if (cursor_row < VGA_HEIGHT && cursor_col < VGA_WIDTH)
            vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = vga_entry(c, color);
        cursor_col++;
    }
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }
    if (cursor_row >= VGA_HEIGHT) {
        for (size_t y = 1; y < VGA_HEIGHT; y++)
            for (size_t x = 0; x < VGA_WIDTH; x++)
                vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
        cursor_row = VGA_HEIGHT - 1;
    }
}

void vga_write(const char *s) { while (*s) vga_put_char(*s++); }

// === ANSI Parser ===
static uint8_t ansi_fg = VGA_LIGHT_GRAY;
static uint8_t ansi_bg = VGA_BLACK;

static int ansi_to_color(int code) {
    switch (code) {
        case 30: return VGA_BLACK;
        case 31: return VGA_RED;
        case 32: return VGA_GREEN;
        case 33: return VGA_BROWN;
        case 34: return VGA_BLUE;
        case 35: return VGA_MAGENTA;
        case 36: return VGA_CYAN;
        case 37: return VGA_LIGHT_GRAY;
        default: return VGA_LIGHT_GRAY;
    }
}

void vga_write_ansi(const char *s) {
    while (*s) {
        if (*s == '\x1b' && *(s+1)=='[') {
            s += 2; // skip "\x1b["
            int params[3] = {0};
            int p = 0, val = 0;
            int has_val = 0;
            while (*s && *s != 'm' && *s != 'H' && *s != 'f' && *s != 'J' && *s != 'K') {
                if (*s >= '0' && *s <= '9') {
                    val = val * 10 + (*s - '0');
                    has_val = 1;
                }
                else if (*s == ';') {
                    if (has_val && p < 3) params[p++] = val;
                    val = 0; has_val = 0;
                }
                else break;
                s++;
            }
            if (has_val && p < 3) params[p++] = val;

            char cmd = *s++;
            switch (cmd) {
                case 'm':
                    if (p == 0 || params[0] == 0) { ansi_fg = VGA_LIGHT_GRAY; ansi_bg = VGA_BLACK; }
                    for (int i = 0; i < p; i++) {
                        if (params[i] >= 30 && params[i] <= 37) ansi_fg = ansi_to_color(params[i]);
                        else if (params[i] >= 40 && params[i] <= 47) ansi_bg = ansi_to_color(params[i] - 10);
                    }
                    vga_set_color(ansi_fg, ansi_bg);
                    break;

                case 'H':
                    vga_move_cursor(params[0] ? params[0] - 1 : 0, params[1] ? params[1] - 1 : 0);
                    break;
                case 'f':
                    vga_move_cursor(params[0] ? params[0] - 1 : 0, params[1] ? params[1] - 1 : 0);
                    break;

                case 'J':
                    vga_clear();
                    break;

                case 'K': // clear line from cursor to end
                    for (size_t x = cursor_col; x < VGA_WIDTH; x++)
                        vga_buffer[cursor_row * VGA_WIDTH + x] = vga_entry(' ', color);
                break;

                default:
                    break;
            }
        }
        else {
            vga_put_char(*s++);
        }
    }
}

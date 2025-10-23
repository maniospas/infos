#pragma once
#include <stddef.h>
#include <stdint.h>

enum VGA_COLOR {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN, VGA_RED, VGA_MAGENTA,
    VGA_BROWN, VGA_LIGHT_GRAY, VGA_DARK_GRAY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN, VGA_LIGHT_RED, VGA_LIGHT_MAGENTA, VGA_YELLOW, VGA_WHITE
};

void vga_clear(void);
void vga_put_char(char c);
void vga_write(const char *s);
void vga_write_ansi(const char *s);
void vga_removechar(void);
void vga_move_cursor(size_t row, size_t col);

void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_raw_color(uint8_t col);
uint8_t vga_get_color(void);

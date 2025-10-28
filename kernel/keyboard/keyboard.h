#pragma once
#include <stdbool.h>
void keyboard_init(void);
void keyboard_clear(void);
unsigned char keyboard_read(void);

// Internal (called by ISR)
void keyboard_handler_c(unsigned char scancode);

// === Event type helpers ===
bool key_released(unsigned char scancode);
bool key_extended(unsigned char scancode);

// === Printable character conversion ===
char key_printable(unsigned char scancode);

// === Common key codes (set 1 scancodes) ===
#define KEY_ENTER     0x1C
#define KEY_BACKSPACE 0x0E
#define KEY_TAB       0x0F
#define KEY_SPACE     0x39
#define KEY_LSHIFT    0x2A
#define KEY_RSHIFT    0x36
#define KEY_CTRL      0x1D
#define KEY_ALT       0x38
#define KEY_ESC       0x01
#define KEY_CAPSLOCK  0x3A
#define KEY_DELETE    0x53  

// Extended (0xE0 prefix usually)
#define KEY_ARROW_UP    0x48
#define KEY_ARROW_DOWN  0x50
#define KEY_ARROW_LEFT  0x4B
#define KEY_ARROW_RIGHT 0x4D

#include "../console.h"

int console_readline(Window* win, char *buffer, size_t size) {
    if (*buffer) return 1;
    size_t pos = 0;
    int shift = 0; // track if shift is active

    for (;;) {
        unsigned char key = keyboard_read();
        if (!key) {
            __asm__("hlt");
            continue;
        }

        // Handle Shift press/release (PS/2 scan codes)
        if (key == 0x2A || key == 0x36) { // Left or Right Shift pressed
            shift = 1;
            continue;
        }
        if (key == 0xAA || key == 0xB6) { // Left or Right Shift released
            shift = 0;
            continue;
        }

        // Handle backspace
        if (key == KEY_BACKSPACE) {
            if (pos > 0) {
                pos--;
                fb_removechar(win);
            }
            continue;
        }

        char c = key_printable(key);

        if (shift) {
            // Uppercase letters
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            } else {
                // Shifted symbols
                switch (c) {
                    case '1': c = '!'; break;
                    case '2': c = '@'; break;
                    case '3': c = '#'; break;
                    case '4': c = '$'; break;
                    case '5': c = '%'; break;
                    case '6': c = '^'; break;
                    case '7': c = '&'; break;
                    case '8': c = '*'; break;
                    case '9': c = '('; break;
                    case '0': c = ')'; break;
                    case '-': c = '_'; break;
                    case '=': c = '+'; break;
                    case '[': c = '{'; break;
                    case ']': c = '}'; break;
                    case ';': c = ':'; break;
                    case '\'': c = '"'; break;
                    case ',': c = '<'; break;
                    case '.': c = '>'; break;
                    case '/': c = '?'; break;
                    case '\\': c = '|'; break;
                    case '`': c = '~'; break;
                }
            }
        }

        // Enter pressed
        if (c == '\n' || c == '\r') {
            buffer[pos] = '\0';
            fb_write(win, "\n");
            return 0;
        }

        // Normal printable characters
        if (c && pos < size - 1) {
            buffer[pos++] = c;
            char s[2] = {c, 0};
            fb_write(win, s);
        }
    }
    return 0;
}
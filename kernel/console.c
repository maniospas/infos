#include "console.h"
#include "screen/screen.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "string.h"
#include "io.h"

// === Local helper to power off system ===
static void poweroff(void) {
    fb_write_ansi("\x1b[1;32mShutting down...\x1b[0m\n");
    // Modern QEMU/ACPI method
    outw(0x604, 0x2000);
    // Bochs/QEMU legacy
    outw(0xB004, 0x2000);
    // If that didn’t work, hang forever
    for (;;) __asm__("hlt");
}

void console_prompt(void) {
    fb_write("> ");
}

void console_readline(char *buffer, size_t size) {
    size_t pos = 0;
    for (;;) {
        unsigned char key = keyboard_read();
        if (!key) {
            __asm__("hlt");
            continue;
        }

        if (key == KEY_BACKSPACE) {
            if (pos > 0) {
                pos--;
                fb_removechar();
            }
            continue;
        }

        char c = key_printable(key);
        if (c == '\n' || c == '\r') {
            buffer[pos] = '\0';
            fb_write("\n");
            return;
        }
        else if (c && pos < size - 1) {
            buffer[pos++] = c;
            char s[2] = {c, 0};
            fb_write(s);
        }
    }
}

void console_execute(const char *cmd) {
    if (cmd[0] == '\0') return;

    if (!strcmp(cmd, "help")) {
        fb_write("Available commands:\n");
        fb_write("  help   - Show this help\n");
        fb_write("  ls     - List files in root\n");
        fb_write("  cat X  - Print file contents\n");
        fb_write("  clear  - Clear screen\n");
        fb_write("  exit   - Shut down\n");
    }
    else if (!strcmp(cmd, "ls")) {
        fat32_ls_root();
    }
    else if (!strncmp(cmd, "cat ", 4)) {
        fat32_cat(cmd + 4);
    }
    else if (!strcmp(cmd, "clear")) {
        fb_clear();
    }
    else if (!strcmp(cmd, "exit")) {
        poweroff();
    }
    else {
        fb_write("Unknown command: ");
        fb_write(cmd);
        fb_write("\n");
    }
}

#include "console.h"
#include "screen/screen.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "string.h"
#include "io.h"
#include "memory/memory.h" 
#include "memory/dynamic.h" 

// === Local helper to power off system ===
static void poweroff(void) {
    fb_write_ansi("\x1b[32mShutting down...\x1b[0m\n");
    // Modern QEMU/ACPI method
    outw(0x604, 0x2000);
    // Bochs/QEMU legacy
    outw(0xB004, 0x2000);
    // If that didn’t work, hang forever
    for (;;) __asm__("hlt");
}

void console_prompt(void) {
    fb_write_ansi("\x1b[35m");
    fb_write(fat32_get_current_path());
    fb_write(": ");
    fb_write_ansi("\x1b[0m");
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
        } else if (c && pos < size - 1) {
            buffer[pos++] = c;
            char s[2] = {c, 0};
            fb_write(s);
        }
    }
}

static void fb_draw_bar(uint32_t used, uint32_t total, uint8_t width) {
    if (total == 0) total = 1; // avoid div by zero
    uint32_t filled = (used * width) / total;

    fb_write(" [");
    for (uint8_t i = 0; i < width; i++) {
        if (i < filled)
            fb_write("#");
        else
            fb_write("-");
    }
    fb_write("] ");
}

void console_execute(const char *cmd) {
    if (!strcmp(cmd, "help")) {
        fb_write_ansi("\n  \033[36mhelp\033[0m   - Show this help\n");
        fb_write_ansi("  \033[36mls\033[0m     - List files in current directory\n");
        fb_write_ansi("  \033[36mcd\033[0m X   - Change directory (use /home for root)\n");
        fb_write_ansi("  \033[36mcat\033[0m X  - Print file contents\n");
        fb_write_ansi("  \033[36mps\033[0m     - Show memory & disk usage\n");
        fb_write_ansi("  \033[36mclear\033[0m  - Clear screen\n");
        fb_write_ansi("  \033[36mexit\033[0m   - Shut down\n");
        fb_write("\n");
    }

    else if (!strcmp(cmd, "ps")) {
        uint64_t memory_size = memory_total_with_regions();
        uint64_t memory_cons = memory_used() + (memory_total_with_regions() - memory_total());
        struct FAT32_Usage usage = fat32_get_usage();

        fb_write_ansi("\n\033[35m  Memory\033[0m ");
        fb_draw_bar((uint32_t)(memory_cons / (1024 * 1024)),
                    (uint32_t)(memory_size / (1024 * 1024)), 10);
        fb_write_dec((memory_cons) / (1024 * 1024));
        fb_write("MB / ");
        fb_write_dec((memory_size) / (1024 * 1024));
        fb_write("MB\n");

        fb_write_ansi("  \033[35mDisk\033[0m   ");
        fb_draw_bar(usage.used_mb, usage.total_mb, 10);
        fb_write_dec(usage.used_mb);
        fb_write("MB / ");
        fb_write_dec(usage.total_mb);
        fb_write("MB\n\n");
    }

    else if (!strcmp(cmd, "ls")) {
        fat32_ls(fat32_get_current_dir());
    }

    else if (!strncmp(cmd, "cd ", 3)) {
        fat32_cd(cmd + 3);
    }

    else if (!strncmp(cmd, "cat ", 4)) {
        fat32_cat(cmd + 4);
    }

    else if (!strcmp(cmd, "clear")) {
        margin = 10;
        fb_clear();
        fb_set_scale(6, 1);
        fb_write_ansi("\x1b[33minfOS\n\x1b[0m");
        margin = 20;
        fb_set_scale(2, 1);
        fb_write("\n");
    }

    else if (!strcmp(cmd, "exit")) {
        poweroff();
    }

    else {
        fb_write_ansi("\n  \033[31mERROR\033[0m Unknown command. Type \033[36mhelp\033[0m for help.\n\n");
    }
}

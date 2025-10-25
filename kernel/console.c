#include "console.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "string.h"
#include "io.h"
#include "memory/memory.h" 
#include "memory/dynamic.h"
#include "application.h"

extern Application apps[5];
extern int text_size;

static void poweroff(Window *win) {
    fb_write_ansi(win, "\x1b[32mShutting down...\x1b[0m\n");
    // Modern QEMU/ACPI method
    outw(0x604, 0x2000);
    // Bochs/QEMU legacy
    outw(0xB004, 0x2000);
    // If that didn’t work, hang forever
    for (;;) __asm__("hlt");
}

void console_prompt(Window* win) {
    fb_write_ansi(win, "\n\x1b[33m");
    fb_write(win, fat32_get_current_path());
    fb_write(win, ": ");
    fb_write_ansi(win, "\x1b[0m");
}

void console_readline(Window* win, char *buffer, size_t size) {
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
                fb_removechar(win);
            }
            continue;
        }
        char c = key_printable(key);
        if (c == '\n' || c == '\r') {
            buffer[pos] = '\0';
            fb_write(win, "\n");
            return;
        } 
        else if (c && pos < size - 1) {
            buffer[pos++] = c;
            char s[2] = {c, 0};
            fb_write(win, s);
        }
    }
}

static void fb_draw_bar(Window* win, uint32_t used, uint32_t total, uint8_t width) {
    if (total == 0) total = 1; // avoid div by zero
    uint32_t filled = (used * width) / total;
    fb_write(win, " [");
    for (uint8_t i = 0; i < width; i++) {
        if (i < filled)
            fb_write(win, "#");
        else
            fb_write(win, "-");
    }
    fb_write(win, "] ");
}

void widget_run(Application* app) {
    fb_clear(app->window);
    fb_set_scale(app->window, 2, 1);
    fb_window_border(app->window, app->data, 0x000000);
    fb_set_scale(app->window, 3, 2);
    console_execute(app->window, app->data, app->vars, app->MAX_VARS);
}

uint64_t hash_str(const char* s) {
    uint64_t hash = 1469598103934665603ULL;
    while (*s) {
        hash ^= (unsigned char)*s++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

#define MAX_HASH_VARS 1024
typedef struct {
    char* name;
    char* value;
} VarEntry;
static VarEntry var_table[MAX_HASH_VARS];
static size_t var_count = 0;

// Finds or inserts a variable by name.
// Returns index in var_table or -1 if full.
static int find_or_insert_var(const char* name) {
    uint64_t hash = hash_str(name);
    size_t idx = hash % MAX_HASH_VARS;
    size_t start = idx;

    do {
        VarEntry* e = &var_table[idx];
        if (!e->name) {
            // empty slot → insert
            size_t len = strlen(name) + 1;
            e->name = malloc(len);
            memcpy(e->name, name, len);
            e->value = NULL;
            var_count++;
            return (int)idx;
        }
        else if (!strcmp(e->name, name)) {
            // found existing
            return (int)idx;
        }

        idx = (idx + 1) % MAX_HASH_VARS;
    } while (idx != start);

    return -1; // table full
}

static int find_var(const char* name) {
    uint64_t hash = hash_str(name);
    size_t idx = hash % MAX_HASH_VARS;
    size_t start = idx;
    do {
        VarEntry* e = &var_table[idx];
        if (!e->name)
            return -1; // not found
        if (!strcmp(e->name, name))
            return (int)idx;
        idx = (idx + 1) % MAX_HASH_VARS;
    } while (idx != start);

    return -1;
}


void console_execute(Window *win, const char *cmd, char** vars, size_t MAX_VARS) {
    if (!strncmp(cmd, "let ", 4)) {
        const char* args = cmd + 4;
        while (*args == ' ') args++;

        // Extract variable name
        const char* name_start = args;
        while (*args && *args != ' ') args++;
        size_t name_len = args - name_start;
        if (name_len == 0) {
            fb_write_ansi(win, "\n\x1b[31mERROR\x1b[0m Missing variable name.\n");
            return;
        }

        // Copy variable name
        char varname[64];
        size_t i = 0;
        while (i < name_len && i < sizeof(varname) - 1) {
            varname[i] = name_start[i];
            i++;
        }
        varname[i] = 0;

        while (*args == ' ') args++;
        if (!*args) {
            fb_write_ansi(win, "\n\x1b[31mERROR\x1b[0m Missing value.\n");
            return;
        }

        // Find or insert variable
        int idx = find_or_insert_var(varname);
        if (idx < 0) {
            fb_write_ansi(win, "\n\x1b[31mERROR\x1b[0m Variable table full.\n");
            return;
        }

        // Copy value string
        size_t val_len = strlen(args) + 1;
        char* new_val = malloc(val_len);
        memcpy(new_val, args, val_len);

        // Free old value if present
        if (var_table[idx].value)
            free(var_table[idx].value);

        var_table[idx].value = new_val;
        fb_write_ansi(win, "\x1b[32mOK\x1b[0m");
    }
    else if (!strcmp(cmd, "help")) {
        fb_write_ansi(win, "\n  \033[32mhelp\033[0m   - Show this help\n");
        fb_write_ansi(win, "  \033[32mls\033[0m     - List files in current directory\n");
        fb_write_ansi(win, "  \033[32mcd\033[0m X   - Change directory (use /home for root)\n");
        fb_write_ansi(win, "  \033[32mcat\033[0m X  - Print file contents\n");
        fb_write_ansi(win, "  \033[32mps\033[0m     - Show memory & disk usage\n");
        fb_write_ansi(win, "  \033[32mclear\033[0m  - Clear screen\n");
        fb_write_ansi(win, "  \033[32mtext\033[0m X - Sets font X among big, small, default\n");
        fb_write_ansi(win, "  \033[32mapp\033[0m X  - Keep running command X\n");
        fb_write_ansi(win, "  \033[32mexit\033[0m   - Shut down\n");
    }
    else if (!strcmp(cmd, "ps")) {
        uint64_t memory_size = memory_total_with_regions();
        uint64_t memory_cons = memory_used() + (memory_total_with_regions() - memory_total());
        struct FAT32_Usage usage = fat32_get_usage();

        fb_write_ansi(win, "\n\033[35m  Memory\033[0m ");
        fb_draw_bar(win, (uint32_t)(memory_cons / (1024 * 1024)),
                    (uint32_t)(memory_size / (1024 * 1024)), 10);
        fb_write_dec(win, (memory_cons) / (1024 * 1024));
        fb_write(win, "MB / ");
        fb_write_dec(win, (memory_size) / (1024 * 1024));
        fb_write(win, "MB\n");

        fb_write_ansi(win, "  \033[35mDisk\033[0m   ");
        fb_draw_bar(win, usage.used_mb, usage.total_mb, 10);
        fb_write_dec(win, usage.used_mb);
        fb_write(win, "MB / ");
        fb_write_dec(win, usage.total_mb);
        fb_write(win, "MB\n\n");
    }
    else if (!strcmp(cmd, "ls")) 
        fat32_ls(win, fat32_get_current_dir());
    else if (!strncmp(cmd, "cd ", 3)) 
        fat32_cd(win, cmd + 3);
    else if (!strncmp(cmd, "cat ", 4)) 
        fat32_cat(win, cmd + 4);
    else if (!strncmp(cmd, "text ", 5)) {
        const char *arg = cmd + 5;
        if (!strcmp(arg, "small")) 
            text_size = 2;
        else if (!strcmp(arg, "big")) 
            text_size = 0;
        else if (!strcmp(arg, "default")) 
            text_size = 1;
        else {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Valid usage: text big | text small | text default\n");
            return;
        }
        fb_clear(win);
        fb_set_scale(win, 6 + text_size, 1 + text_size);
        fb_write_ansi(win, 
            "\x1b[32m"
            "LettuOS\n"
            "\x1b[0m"
        );
        fb_clear(win);
        fb_set_scale(win, 2+(text_size>=2?(text_size-1):text_size),1+text_size);
        fb_window_border(win, NULL, 0x000000);
    }
    else if (!strcmp(cmd, "clear")) {
        fb_clear(win);
        fb_set_scale(win, 2+text_size,1+text_size);
        fb_window_border(win, NULL, 0x000000);
    }
    else if (!strcmp(cmd, "exit")) 
        poweroff(win);
    else if (!strncmp(cmd, "app app", 7)) 
        fb_write_ansi(win, "\n\033[31mERROR\033[0m The app would unconditionally copy itself.\n\n");
    else if (!strncmp(cmd, "app ", 4)) {
        const char *arg = cmd + 4;
        // Find first free widget slot
        for (int i = 1; i < 5; i++) {
            if (!apps[i].run) {
                apps[i].run = widget_run;
                size_t pos = 0;
                while(*arg && pos<apps[i].data_size) {
                    ((char*)apps[i].data)[pos] = *arg;
                    arg++;
                    pos++;
                }
                ((char*)apps[i].data)[pos] = 0;
                return;
            }
        }
        fb_write_ansi(win, "\n\033[31mERROR\033[0m No free widget slots available.\n");
    }
    // else 
    //     fb_write_ansi(win, "\n  \033[31mERROR\033[0m Unknown command. Type \033[32mhelp\033[0m for help.\n");
    else {
        const char* name = cmd;
        while (*name == ' ') name++;
        if (!*name) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing variable name.");
            return;
        }
        char varname[64];
        size_t i = 0;
        while (*name && *name != ' ' && i < sizeof(varname) - 1) 
            varname[i++] = *name++;
        varname[i] = 0;
        int idx = find_var(varname);
        if (idx < 0 || !var_table[idx].value) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Not yet set: ");
            fb_write(win, varname);
            return;
        }
        fb_write(win, var_table[idx].value);
    }
}

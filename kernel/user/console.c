#include "console.h"
#include "../string.h"
#include "../file/fat32.h"
#include "../keyboard/keyboard.h"
#include "../memory/memory.h"
#include "../user/application.h"
#include "../io.h"

static void poweroff(Window *win) {
    fb_write_ansi(win, "\x1b[32mShutting down...\x1b[0m\n");
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    for (;;) __asm__("hlt");
}

void console_prompt(Window* win) {
    fb_write_ansi(win, "\x1b[33m");
    fb_write(win, fat32_get_current_path());
    fb_write(win, ": ");
    fb_write_ansi(win, "\x1b[0m");
}

int console_readline(Window* win, char *buffer, size_t size) {
    if(*buffer) return 1;
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
            return 0;
        } 
        else if (c && pos < size - 1) {
            buffer[pos++] = c;
            char s[2] = {c, 0};
            fb_write(win, s);
        }
    }
    return 0;
}

void widget_run(Application* app, int appid) {
    if(!app->window || !app->window->width || !app->window->height)
        return;
    fb_clear(app->window);
    fb_set_scale(app->window, 2, 1);
    fb_window_border(app->window, app->data, 0x000000, appid);
    fb_set_scale(app->window, 3, 2);
    console_execute(app);
    fb_put_char(app->window, '\n');
}

void widget_terminate(Application* app, int appid) {
    if(!app->window || !app->window->width || !app->window->height)
        return;
    uint32_t prev = app->window->bg_color;
    app->window->bg_color = 0;
    app->window->y -= 40;
    app->window->height += 80;
    app->window->width += 40;
    fb_clear(app->window);
    app->window->y += 40;
    app->window->height -= 80;
    app->window->width -= 40;
    app->window->bg_color = prev;
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

void console_execute(Application *app) {
    Window* win = app->window;
    char** vars = app->vars;
    size_t MAX_VARS = app->MAX_VARS;
    const char *cmd = app->data;
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
        fb_write_ansi(win, "  \033[32mhelp\033[0m     - Show this help\n");
        fb_write_ansi(win, "  \033[32mls\033[0m       - List files in current directory\n");
        fb_write_ansi(win, "  \033[32mcd\033[0m X     - Change directory (use /home for root)\n");
        fb_write_ansi(win, "  \033[32mcat\033[0m X    - Print file contents\n");
        fb_write_ansi(win, "  \033[32mps\033[0m       - Show memory & disk usage\n");
        fb_write_ansi(win, "  \033[32mclear\033[0m    - Clear screen\n");
        fb_write_ansi(win, "  \033[32mtext\033[0m X   - Sets font X among big, small, default\n");
        fb_write_ansi(win, "  \033[32mapp\033[0m X    - Keep running command X\n");
        fb_write_ansi(win, "  \033[32mkill\033[0m X   - Kills app #X (look at top right)\n");
        fb_write_ansi(win, "  \033[32msend X M\033[0m - Sends message M to app #X' input\n");
        fb_write_ansi(win, "  \033[32mlogger\033[0m   - Accumulates inputs to an output buffer.\n");
        fb_write_ansi(win, "  \033[32mexit\033[0m     - Shut down\n");
    }
    else if (!strcmp(cmd, "ps")) {
        uint64_t memory_size = memory_total_with_regions();
        uint64_t memory_cons = memory_used() + (memory_total_with_regions() - memory_total());
        struct FAT32_Usage usage = fat32_get_usage();

        fb_write_ansi(win, "\033[35m  Memory\033[0m ");
        fb_bar(win, (uint32_t)(memory_cons), (uint32_t)(memory_size), 50);
        fb_write_dec(win, (memory_cons) / (1024 * 1024));
        fb_write(win, "MB / ");
        fb_write_dec(win, (memory_size) / (1024 * 1024));
        fb_write(win, "MB\n");
        fb_write_ansi(win, "  \033[35mDisk\033[0m   ");
        fb_bar(win, (long int )usage.used_mb, (long int )usage.total_mb, 50);
        fb_write_dec(win, usage.used_mb);
        fb_write(win, "MB / ");
        fb_write_dec(win, usage.total_mb);
        fb_write(win, "MB\n");
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
        fb_window_border(win, NULL, 0x000000, 0);
    }
    else if (!strcmp(cmd, "clear")) {
        fb_clear(win);
        fb_set_scale(win, 2+text_size,1+text_size);
        fb_window_border(win, NULL, 0x000000, 0);
    }
    else if (!strcmp(cmd, "exit")) {
        int saved = 1;
        for (uint32_t i = 0; i < MAX_APPLICATIONS; i++) 
            if(apps[i].save && !apps[i].save(&apps[i], i)) {
                fb_write_ansi(win, "\033[35mWARN\033[0m App is busy: ");
                fb_write(win, apps[i].data);
                fb_write(win, "\n");
            }
        if(!saved) {
            fb_write_ansi(win, "\033[35mWARN\033[0m What to do with busy apps?\n\033[35mkill\033[0m|\033[35mcancel:\033[0m ");
            char termination_buffer[8];
            console_readline(win, termination_buffer, 8);
            termination_buffer[7] = 0;
            if(!strcmp(termination_buffer, "kill"))
                saved = 1;
            else
                fb_write_ansi(win, "\033[31mERROR\033[0m Cancelled by user");
        }
        if(saved) {
            for (uint32_t i = 0; i < MAX_APPLICATIONS; i++) 
                if(apps[i].terminate)
                    apps[i].terminate(&apps[i], i);
            poweroff(win);
        }
    }
    else if (!strncmp(cmd, "kill ", 5)) {
        const char *arg = cmd + 5;
        while (*arg == ' ') arg++;
        if (!*arg) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing app ID. Example: kill 1");
            return;
        }
        uint32_t id = 0;
        const char* p = arg;
        while (*p >= '0' && *p <= '9') {
            id = id * 10 + (*p - '0');
            p++;
        }
        if(*p) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m App #id should be a number. Example: kill 1");
            return;
        }
        if (id >= MAX_APPLICATIONS) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m App does not exist with #id:");
            fb_write_dec(win, id);
            return;
        }
        if (id == 0) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m App #0 is the system console. Use \x1b[32mexit\x1b[0m to turn off the computer.");
            return;
        }
        if (!apps[id].run) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m App does not exist with #id: ");
            fb_write_dec(win, id);
            return;
        }
        int saved = 1;
        if (apps[id].save) {
            fb_write_ansi(win, "\033[35mWARN\033[0m App is busy. What to do?\n\033[35mkill\033[0m|\033[35mcancel:\033[0m ");
            char termination_buffer[8];
            console_readline(win, termination_buffer, 8);
            termination_buffer[7] = 0;
            if(strcmp(termination_buffer, "kill")) 
                saved = 0;
        }
        if(saved) {
            if (apps[id].terminate) {
                apps[id].terminate(&apps[id], id);
                if(apps[id].window) {
                    free(apps[id].window);
                    apps[id].window = NULL;
                }
            }
            apps[id].window = NULL;  
            apps[id].run = NULL;
            apps[id].save = NULL;
            apps[id].terminate = NULL;
            fb_write_ansi(win, "\x1b[32mOK\x1b[0m App killed, freeing up #id: ");
            fb_write_dec(win, id);
            fb_write(win, "\n");
        }
        else 
            fb_write_ansi(win, "\033[31mERROR\033[0m Cancelled by user.\n");
    }
    else if (!strncmp(cmd, "app ", 4)) {
        const char *arg = cmd + 4;
        // Find first free widget slot
        for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
            if (!apps[i].run) {
                apps[i].window = malloc(sizeof(Window));
                init_fullscreen(apps[i].window);
                apps[i].window->width = 0;
                apps[i].window->height = 0;
                apps[i].vars = vars;
                apps[i].MAX_VARS = MAX_VARS;
                if(!apps[i].window) {
                    fb_write_ansi(win, "\033[31mERROR\033[0m Not enough memory to start application.\n");
                    return;
                }
                apps[i].run = widget_run;
                apps[i].save = NULL;
                apps[i].terminate = widget_terminate;
                apps[i].input[0] = '\0';
                apps[i].output[0] = '\0';
                apps[i].output_state = 0;
                size_t pos = 0;
                while(*arg && pos<APPLICATION_MESSAGE_SIZE-1) {
                    ((char*)apps[i].data)[pos] = *arg;
                    arg++;
                    pos++;
                }
                ((char*)apps[i].data)[pos] = 0;
                fb_write_ansi(win, "\033[32mOK\033[0m App created with #id: ");
                fb_write_dec(win, i);
                fb_write(win, "\n");
                return;
            }
        }
        fb_write_ansi(win, "\033[31mERROR\033[0m No free app slots available.\n");
    }
    else if (!strncmp(cmd, "send ", 5)) {
        char *arg = cmd + 5;
        while (*arg == ' ') arg++;
        if (!*arg) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing app #id. Type \033[32mhelp\033[0m for help.\n");
            return;
        }
        uint32_t id = 0;
        char* p = arg;
        while (*p >= '0' && *p <= '9') {
            id = id * 10 + (*p - '0');
            p++;
        }
        if (p == arg || (*p && *p != ' ')) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid app #id. It must be a number. Example: send 1 hello\n");
            return;
        }

        while (*p == ' ') p++;
        if (id && (id >= MAX_APPLICATIONS || !apps[id].run)) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m No active app with #id: ");
            fb_write_dec(win, id);
            fb_write(win, "\n");
            return;
        }
        if (!*p) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing message to send. Type \033[32mhelp\033[0m for help.\n");
            return;
        }
        size_t len = strlen(p);
        if (len >= APPLICATION_MESSAGE_SIZE)
            len = APPLICATION_MESSAGE_SIZE - 1;
        memcpy(apps[id].input, p, len);
        apps[id].input[len] = '\0';
        fb_write_ansi(win, "\x1b[32mOK\x1b[0m Message sent to app #");
        fb_write_dec(win, id);
        fb_write(win, "\n");
    }
    else if (!strcmp(cmd, "logger")) {
        char *out = app->output;
        size_t pos = app->output_state;
        const char *in = app->input;
        size_t in_len = strlen(in);
        if(in_len) {
            const size_t limit = APPLICATION_MESSAGE_SIZE;
            if (pos == 0 && out[0] == '\0')
                pos = 0;
            else if (out[pos] != '\0')
                out[pos] = '\0';
            size_t needed = in_len + 2;
            if (pos + needed >= limit) {
                size_t shift = 0;
                size_t new_start = 0;
                while (new_start < pos && (pos - new_start + needed >= limit)) {
                    if (out[new_start] == '\n')
                        shift = new_start + 1; // cut after this newline
                    new_start++;
                }
                if (shift > 0 && shift < pos) {
                    size_t remain = pos - shift;
                    for (size_t i = 0; i < remain; i++)
                        out[i] = out[i + shift];
                    pos = remain;
                    out[pos] = '\0';
                } else {
                    pos = 0;
                    out[0] = '\0';
                }
            }
            for (size_t i = 0; i < in_len && pos < limit - 2; i++)
                out[pos++] = in[i];
            if (pos < limit - 1)
                out[pos++] = '\n';
            out[pos] = '\0';
            app->output_state = pos;
            app->input[0] = '\0';
            fb_write(win, out);
            fb_write_ansi(win, "\x1b[32mOK\x1b[0m Received new input!\n");
        }
        else {
            fb_write(win, out);
            fb_write_ansi(win, "\x1b[33mWAITS:\x1b[0m For input from \x1b[32msend\x1b[0m.\n");
        }
    }
    // else 
    //     fb_write_ansi(win, "\n  \033[31mERROR\033[0m Unknown command. Type \033[32mhelp\033[0m for help.\n");
    else {
        const char* name = cmd;
        while (*name == ' ') name++;
        if (!*name) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing variable name. Type \033[32mhelp\033[0m for help.\n");
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
            fb_write_ansi(win, ". Type \033[32mhelp\033[0m for help.\n");
            return;
        }
        fb_write(win, var_table[idx].value);
    }
}

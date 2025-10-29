#include "../console.h"

int console_command(Application *app) {
    Window* win = app->window;
    char** vars = app->vars;
    // fb_write(win, app->data);
    // fb_write(win, "\n");
    size_t MAX_VARS = app->MAX_VARS;
    const char *cmd = app->data;
    while (*cmd == ' ') cmd++;
    if (!strncmp(cmd, "let ", 4)) {
        const char* args = cmd + 4;
        while (*args == ' ') args++;

        // Extract variable name
        const char* name_start = args;
        while (*args && *args != ' ') args++;
        size_t name_len = args - name_start;
        if (name_len == 0) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing variable name. Type \033[32mhelp\033[0m for help.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
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
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing value. Type \033[32mhelp\033[0m for help.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // Find or insert variable
        int idx = find_or_insert_var(varname);
        if (idx < 0) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Variable table full.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // Copy value string
        size_t val_len = strlen(args) + 1;
        char* new_val = malloc(val_len);
        memcpy(new_val, args, val_len);

        // Free old value if present
        if (var_table[idx].value)
            free(var_table[idx].value);

        var_table[idx].value = new_val;
        fb_write_ansi(win, "\x1b[32mOK\x1b[0m Set value ");
        fb_write(win, varname);
        fb_write(win, ": ");
        fb_write(win, new_val);
        fb_write(win, "\n");
    }
    else if (!strcmp(cmd, "help")) {
        fb_write_ansi(win, "Enter: run typed command\n(X) evaluates X\n{X} yields unevaluated X\n");
        fb_write_ansi(win, "Left and right arrows move cursor\nCtrl+arrows skip words\n");
        fb_write_ansi(win, "\033[32mhelp\033[0m     - Show this help\n");
        fb_write_ansi(win, "\033[32mread\033[0m     - Read file or directory, up to 4KB\n");
        fb_write_ansi(win, "\033[32mfile X\033[0m   - Open file from path X\n");
        fb_write_ansi(win, "\033[32mcd\033[0m X     - Change dir, can start from \033[33m/home\033[0m\n");
        //fb_write_ansi(win, "\033[32mcat\033[0m X    - Print file contents\n");
        fb_write_ansi(win, "\033[32mps\033[0m       - Show memory & disk usage\n");
        fb_write_ansi(win, "\033[32mclear\033[0m    - Clear screen\n");
        //fb_write_ansi(win, "\033[32mtext\033[0m X   - Set font X among big, small, default\n");
        fb_write_ansi(win, "\033[32mapp\033[0m X    - Keep running command X\n");
        fb_write_ansi(win, "\033[32mkill\033[0m X   - Kills app or file handle X\n");
        fb_write_ansi(win, "\033[32mto\033[0m X V   - Sends message V to app X's input\n");
        fb_write_ansi(win, "\033[32margs\033[0m     - Retrieves last message from \033[32mto\033[0m\n");
        fb_write_ansi(win, "\033[32mlet\033[0m  X V - Sets value V to variable X\n");
        //fb_write_ansi(win, "\033[32mlog\033[0m      - Accumulates and prints messages\n");
        fb_write_ansi(win, "\033[32mexit\033[0m     - Shut down\n");
    }
    else if (!strcmp(cmd, "ps")) {
        uint64_t memory_size = memory_total_with_regions();
        uint64_t memory_cons = memory_used() + (memory_total_with_regions() - memory_total());
        struct FAT32_Usage usage = fat32_get_usage();

        fb_write_ansi(win, "\033[35mMemory\033[0m ");
        fb_bar(win, (uint32_t)(memory_cons), (uint32_t)(memory_size), 50);
        fb_write_dec(win, (memory_cons) / (1024 * 1024));
        fb_write(win, "MB / ");
        fb_write_dec(win, (memory_size) / (1024 * 1024));
        fb_write(win, "MB\n");
        fb_write_ansi(win, "\033[35mDisk\033[0m   ");
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
    // else if (!strncmp(cmd, "cat ", 4)) 
    //    fat32_cat(win, cmd + 4);
    /*else if (!strncmp(cmd, "text ", 5)) {
        const char *arg = cmd + 5;
        if (!strcmp(arg, "small")) 
            text_size = 2;
        else if (!strcmp(arg, "big")) 
            text_size = 0;
        else if (!strcmp(arg, "default")) 
            text_size = 1;
        else {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Valid usage: text big | text small | text default\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        fb_clear(win);
        fb_set_scale(win, 6 + text_size, 1 + text_size);
        fb_write_ansi(win, 
            "\x1b[32m"
            "letOS\n"
            "\x1b[0m"
        );
        fb_clear(win);
        fb_set_scale(win, 2+(text_size>=2?(text_size-1):text_size),1+text_size);
        fb_window_border(win, NULL, 0x000000, 0);
    }*/
    else if (!strcmp(cmd, "clear")) {
        fb_clear(win);
        fb_set_scale(win, 2+text_size,1+text_size);
        fb_window_border(win, NULL, 0x000000, 0);
    }
    else if (!strncmp(cmd, "file ", 5)) {
        const char *path = cmd + 5;
        while (*path == ' ') 
            path++;
        if (!*path) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing path. Example: file logo.bmp\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        int handle = fat32_open_file(path);
        if (handle == -2) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Too many open files. Try to \x1b[32mkill\x1b[0m some.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        if (handle < 0) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Failed to open file: ");
            fb_write(win, path);
            fb_write(win, "\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // --- Print feedback to screen ---
        fb_write_ansi(win, "\x1b[32mOK\x1b[0m Opened: file");
        fb_write_dec(win, handle);
        fb_write(win, "\n");

        // --- Write handle string into app->output (e.g. "file3") ---
        char *dst = app->output;
        dst[0] = 'f';
        dst[1] = 'i';
        dst[2] = 'l';
        dst[3] = 'e';
        int pos = 4;

        // Convert handle number to string (manual itoa)
        int h = handle;
        char digits[4]; // enough to store all digits
        int dpos = 0;
        if (h == 0) {
            digits[dpos++] = '0';
        } else {
            while (h > 0 && dpos < 15) {
                digits[dpos++] = '0' + (h % 10);
                h /= 10;
            }
        }
        // Reverse digits into output buffer
        for (int i = dpos - 1; i >= 0 && pos < APPLICATION_MESSAGE_SIZE - 1; i--)
            dst[pos++] = digits[i];

        dst[pos] = '\0';
        app->output_state = pos;
    }
    else if (!strncmp(cmd, "image ", 6)) {
        const char *arg = cmd + 6;
        while (*arg == ' ') arg++;
        if (!*arg) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Usage: image file<ID> [width height]\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        if (strncmp(arg, "file", 4)) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Expected a file handle like file1.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        arg += 4;
        int file_id = 0;
        while (*arg >= '0' && *arg <= '9') {
            file_id = file_id * 10 + (*arg - '0');
            arg++;
        }

        if (file_id < 0 || file_id >= 16) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid file handle.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        size_t width = 0, height = 0;
        while (*arg == ' ') arg++;
        while (*arg >= '0' && *arg <= '9') {
            width = width * 10 + (*arg - '0');
            arg++;
        }
        while (*arg == ' ') arg++;
        while (*arg >= '0' && *arg <= '9') {
            height = height * 10 + (*arg - '0');
            arg++;
        }
        if (!width) width = 200;
        if (!height) height = 200;
        //fb_write(win, "\n\n\n\n\n\n\n");
        //win->cursor_y -= 200;
        fb_image_from_file(win, file_id, width, height);
        fb_write(win, "\n");
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
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid ID. Example: kill app1 or kill file3\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        int is_file = 0;
        if (!strncmp(arg, "file", 4)) {
            is_file = 1;
            arg += 4;
        } else if (!strncmp(arg, "app", 3)) {
            arg += 3;
        } else {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid ID. Example: kill app1 or kill file3\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // Parse numeric ID
        uint32_t id = 0;
        const char *p = arg;
        while (*p >= '0' && *p <= '9') {
            id = id * 10 + (*p - '0');
            p++;
        }
        if (*p) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid ID. Example: kill app1 or kill file3\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        if (is_file) {
            // --- Kill a file handle ---
            if (id >= 16) { // assuming max 16 file handles
                fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Not open: file");
                fb_write_dec(win, id);
                fb_write(win, "\n");
                return CONSOLE_EXECUTE_RUNTIME_ERROR;
            }
            fat32_close_file(id);
            fb_write_ansi(win, "\x1b[32mOK\x1b[0m Not open: ");
            fb_write_dec(win, id);
            fb_write(win, "\n");
            return CONSOLE_EXECUTE_OK;
        }

        // --- Kill an app ---
        if (id >= MAX_APPLICATIONS) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Not running: app");
            fb_write_dec(win, id);
            fb_write(win, "\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        if (id == 0) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Use \x1b[32mexit\x1b[0m to turn off the computer instead.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        if (!apps[id].run) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Not running: app");
            fb_write_dec(win, id);
            fb_write(win, "\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        int saved = 1;
        if (apps[id].save) {
            fb_write_ansi(win, "\033[35mWARN\033[0m App is busy. What to do?\n\033[35mkill\033[0m|\033[35mcancel:\033[0m ");
            char termination_buffer[8];
            console_readline(win, termination_buffer, 8);
            termination_buffer[7] = 0;
            if (strcmp(termination_buffer, "kill"))
                saved = 0;
        }
        if (saved) {
            if (apps[id].terminate) {
                apps[id].terminate(&apps[id], id);
                if (apps[id].window) {
                    free(apps[id].window);
                    apps[id].window = NULL;
                }
            }
            apps[id].window = NULL;
            apps[id].run = NULL;
            apps[id].save = NULL;
            apps[id].terminate = NULL;
            fb_write_ansi(win, "\x1b[32mOK\x1b[0m Killed: app");
            fb_write_dec(win, id);
            fb_write(win, "\n");
        } else {
            fb_write_ansi(win, "\033[31mERROR\033[0m Cancelled by user.\n");
        }
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
                    fb_write_ansi(win, "\033[31mERROR\033[0m Not enough memory to start app.\n");
                    return CONSOLE_EXECUTE_RUNTIME_ERROR;
                }
                apps[i].run = widget_run;
                apps[i].save = NULL;
                apps[i].terminate = widget_terminate;
                apps[i].input[0] = '\0';
                apps[i].output[0] = '\0';
                apps[i].output_state = 0;
                apps[i].input_state = 0;
                size_t pos = 0;
                while(*arg && pos<APPLICATION_MESSAGE_SIZE-1) {
                    ((char*)apps[i].data)[pos] = *arg;
                    arg++;
                    pos++;
                }
                ((char*)apps[i].data)[pos] = 0;

                // Write app id directly into app->output (no temp buffer)
                uint32_t id_val = i;
                uint32_t div = 1;
                for(;id_val>div;div*=10);
                size_t k = 3;
                app->output[0] = 'a';
                app->output[1] = 'p';
                app->output[2] = 'p'; 
                while (div > 0 && k < APPLICATION_MESSAGE_SIZE - 1) {
                    app->output[k++] = '0' + (id_val / div);
                    id_val %= div;
                    div /= 10;
                }
                app->output[k] = 0;
                app->output_state = k;

                fb_write_ansi(win, "\033[32mOK\033[0m Created: app");
                fb_write_dec(win, i);
                fb_write(win, "\n");
                return CONSOLE_EXECUTE_OK;
            }
        }
        fb_write_ansi(win, "\033[31mERROR\033[0m No free app slots available.\n");
        return CONSOLE_EXECUTE_RUNTIME_ERROR;
    }
    else if (!strncmp(cmd, "to ", 3)) {
        const char *arg = cmd + 3;
        while (*arg == ' ') arg++;
        if (!*arg) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing app target. Example: to app1 hello\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // --- Require "app" prefix ---
        if (strncmp(arg, "app", 3) != 0) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid target. Example: to app1 hello'\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        arg += 3;

        // --- Parse numeric ID ---
        uint32_t id = 0;
        const char *p = arg;
        while (*p >= '0' && *p <= '9') {
            id = id * 10 + (*p - '0');
            p++;
        }
        if (p == arg || (*p && *p != ' ')) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid app ID. Example: to app1 hello\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // --- Skip spaces to get message ---
        while (*p == ' ') p++;
        if (!*p) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing message. Example: to app1 hello\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // --- Validate app ID ---
        if (id == 0) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Cannot send to app0 (system console).\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        if (id >= MAX_APPLICATIONS || !apps[id].run) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Does not exist: app");
            fb_write_dec(win, id);
            fb_write(win, "\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        // --- Copy message ---
        size_t len = strlen(p);
        if (len >= APPLICATION_MESSAGE_SIZE)
            len = APPLICATION_MESSAGE_SIZE - 1;
        memcpy(apps[id].input, p, len);
        apps[id].input[len] = '\0';
        apps[id].input_state = len;

        fb_write_ansi(win, "\x1b[32mOK\x1b[0m Message sent to: app");
        fb_write_dec(win, id);
        fb_write(win, "\n");
    }

    else if (!strncmp(cmd, "read ", 5)) {
        const char *path = cmd + 5;
        while (*path == ' ') path++;
        if (!*path) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Missing path. Example: read /home\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        app->output_state = fat32_read(app->output, APPLICATION_MESSAGE_SIZE, 0, path);
        if (app->output_state) {
            fb_write_ansi(win, "\x1b[32mOK\x1b[0m Read path contents: ");
            fb_write(win, path);
            fb_write(win, " ");
            fb_write_dec(win, app->output_state/1024);
            fb_write(win, " KB\n");
        } 
        else {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Failed to read: ");
            fb_write(win, path);
            fb_write(win, "\n");
        }
    }
    else if (!strncmp(cmd, "print ", 6)) {
        const char *val = cmd + 6;
        while (*val == ' ') val++;
        if (*val) 
            fb_write_ansi(win, val);
        fb_write(win, " \n");
    }
    else if (!strcmp(cmd, "args")) {
        const char *in = app->input;
        if (*in && app->input_state && *in!='\n') {
            size_t in_len = app->input_state;
            if (in_len >= APPLICATION_MESSAGE_SIZE)
                in_len = APPLICATION_MESSAGE_SIZE - 1;
            memcpy(app->output, in, in_len);
            app->output[in_len] = '\0';
            app->output_state = in_len;
            app->input[0] = '\0';
            app->input_state = 0;
            fb_write_ansi(win, "\x1b[32mOK\x1b[0m New args:");
            fb_write(win, app->output);
            fb_write_ansi(win, "\n-----\n");
        } else {
            if(*app->output) {
                fb_write_ansi(win, "\x1b[33mWAITING\x1b[0m Last args:");
                fb_write(win, app->output);
            }
            else {
                fb_write_ansi(win, "\x1b[33mWAITING\x1b[0m No args yet.");
            }
            fb_write_ansi(win, "\n-----\n");
        }
    }
    else if (!strcmp(cmd, "log")) {
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
            fb_write_ansi(win, "\x1b[32mOK\x1b[0m Received new input!\n-----\n");
            fb_write(win, out);
        }
        else {
            fb_write_ansi(win, "\x1b[33mWAITING\x1b[0m for message.\n-----\n");
            fb_write(win, out);
        }
    }
    // else 
    //     fb_write_ansi(win, "\n  \033[31mERROR\033[0m Unknown command. Type \033[32mhelp\033[0m for help.\n");
    else {
        const char* name = cmd;
        while (*name == ' ') name++;
        if (!*name) {
            fb_write_ansi(app->window, "Type \x1b[32mhelp\x1b[0m for a list of commands.\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }
        char varname[64];
        size_t i = 0;
        while (*name && *name != ' ' && i < sizeof(varname) - 1)
            varname[i++] = *name++;
        varname[i] = 0;
        int idx = find_var(varname);
        if (idx < 0 || !var_table[idx].value) {
            fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Failed to evaluate: ");
            fb_write(win, cmd);
            fb_write(win, "\n");
            return CONSOLE_EXECUTE_RUNTIME_ERROR;
        }

        const char *val = var_table[idx].value;

        // --- Write value to output buffer ---
        size_t pos = 0;
        while (val[pos] && pos < APPLICATION_MESSAGE_SIZE - 1) {
            app->output[pos] = val[pos];
            pos++;
        }
        app->output[pos] = 0;
        app->output_state = pos;

        // --- Also print it to screen ---
        fb_write_ansi(win, "\x1b[32mOK\x1b[0m Got value ");
        fb_write(win, varname);
        fb_write(win, ": ");
        fb_write(win, val);
        fb_write(win, "\n");
    }
    return CONSOLE_EXECUTE_OK;
}

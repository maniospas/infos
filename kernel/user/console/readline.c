#include "../console.h"

/* ANSI Colors */
#define RESET  "\033[0m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define RED    "\033[31m"
#define BLUE   "\033[34m"

extern uint32_t font_size;
extern uint32_t margin;

/* Keywords for syntax highlighting */
static const char* keywords[] = {
    "help","ls","cd","ps","clear","text",
    "app","kill","to","log","exit","let","read","print",
    "image","file","args"
};
#define NUM_KEYWORDS (sizeof(keywords)/sizeof(keywords[0]))

#define MAX_HISTORY 16
#define MAX_CMD_LEN 256

/* Shifted key mapping */
static const char shift_map[128] = {
    ['1']='!', ['2']='@', ['3']='#', ['4']='$',
    ['5']='%', ['6']='^', ['7']='&', ['8']='*',
    ['9']='(', ['0']=')', ['-']='_', ['=']='+',
    ['[']='{', [']']='}', [';']=':', ['\'']='"',
    [',']='<', ['.']='>', ['/']='?', ['\\']='|', ['`']='~'
};

/* Helpers */
static inline char apply_shift(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    if ((unsigned char)c < 128 && shift_map[(int)c]) return shift_map[(int)c];
    return c;
}

static inline int is_delim(char c) {
    return (c==' '||c=='('||c==')'||c=='['||c==']'||c=='{'||c=='}'||c=='|');
}

static inline const char* color_for_depth(int depth) {
    switch (depth % 3) {
        case 0: return YELLOW;
        case 1: return RED;
        default: return BLUE;
    }
}

static inline int char_px_width(Window* win) {
    float scale = (float)win->scale_nominator / (float)win->scale_denominator;
    return (int)((font_size / 2.0f) * scale);
}

static int line_capacity_chars(Window* win, size_t line_start_x) {
    int cw = char_px_width(win);
    int right = (int)(win->x + win->width - margin);
    int avail = right - (int)line_start_x;
    if (cw <= 0 || avail <= 0) return 0;
    return avail / cw;
}

static void last_word_bounds(const char* buf, int pos, int* start, int* len) {
    int i = pos - 1;
    while (i >= 0 && !is_delim(buf[i])) i--;
    *start = i + 1;
    *len = pos - *start;
}

static int matches_keyword(const char* buf, int start, int len) {
    for (int k = 0; k < (int)NUM_KEYWORDS; ++k) {
        const char* kw = keywords[k];
        int i = 0;
        while (i < len && kw[i]) {
            if (buf[start + i] != kw[i]) goto next_kw;
            ++i;
        }
        if (i == len && kw[i] == '\0') return 1;
    next_kw:;
    }
    return 0;
}

/* Utility to draw a colored word */
static void draw_colored_word(Window* win, const char* word, int len, const char* color) {
    fb_write_ansi(win, color);
    for (int i = 0; i < len; i++)
        fb_put_char(win, word[i]);
    fb_write_ansi(win, RESET);
}

/* full-line render with syntax highlighting + caret overlay by reprinting same char */
static void render_line(Window* win, const char* buf, size_t len, size_t cursor_pos, size_t line_start_x) {
    const int cw = (int)((font_size / 2.0f) * ((float)win->scale_nominator / (float)win->scale_denominator));
    const int right = (int)(win->x + win->width - margin);
    const int avail = right - (int)line_start_x;
    const int capacity = (cw > 0 && avail > 0) ? (avail / cw) : 0;

    size_t draw_len = len;
    if ((int)draw_len > capacity) draw_len = (size_t)capacity;

    size_t xpos[MAX_CMD_LEN + 1];
    uint32_t fg_used[MAX_CMD_LEN];
    char depth_stack[128]; // track what opened each depth level

    fb_clearline(win, line_start_x);
    win->cursor_x = line_start_x;

    int depth = 0;
    size_t i = 0;

    while (i < draw_len) {
        char c = buf[i];
        xpos[i] = win->cursor_x;

        /* Opening tokens: (, {, or | */
        if (c == '(' || c == '{' || c == '|') {
            if (depth < (int)(sizeof(depth_stack))) {
                depth_stack[depth] = c;
            }

            fb_write_ansi(win, color_for_depth(depth));
            fg_used[i] = win->fg_color;
            fb_put_char(win, c);
            fb_write_ansi(win, RESET);
            depth++;
            i++;
            continue;
        }

        /* Closing tokens: ) or } */
        if (c == ')' || c == '}' || c == '|') {
            int close_depth = depth;
            if (depth > 0) {
                char opener = depth_stack[depth - 1];

                if (c == '|') {
                    // normal behavior — just close one pipe
                    if (opener == '|') depth--;
                    close_depth = depth;
                } else {
                    // find matching non-pipe opener
                    int match_depth = depth - 1;
                    while (match_depth > 0 && depth_stack[match_depth] == '|')
                        match_depth--;

                    // color = color of that non-pipe opener
                    close_depth = (match_depth >= 0) ? match_depth : 0;

                    // pop everything until and including the matching opener
                    depth = match_depth;
                    if (depth > 0) depth--; // consume that opener
                }
            }

            fb_write_ansi(win, color_for_depth(close_depth));
            fg_used[i] = win->fg_color;
            fb_put_char(win, c);
            fb_write_ansi(win, RESET);
            i++;
            continue;
        }


        /* Alphanumeric sequences — keyword highlighting */
        if (is_alnum(c)) {
            size_t start = i;
            while (i < draw_len && is_alnum(buf[i])) i++;
            int wlen = (int)(i - start);
            int is_kw = matches_keyword(buf, (int)start, wlen);
            if (is_kw) fb_write_ansi(win, GREEN);
            for (int k = 0; k < wlen; ++k) {
                size_t idx = start + (size_t)k;
                xpos[idx] = win->cursor_x;
                fg_used[idx] = win->fg_color;
                fb_put_char(win, buf[idx]);
            }
            if (is_kw) fb_write_ansi(win, RESET);
            continue;
        }

        /* Everything else — spaces, punctuation, etc. */
        fg_used[i] = win->fg_color;
        fb_put_char(win, c);
        i++;
    }

    xpos[draw_len] = win->cursor_x;

    /* caret overlay AFTER drawing */
    if (cursor_pos != (size_t)-1 && cursor_pos <= draw_len) {
        int save_x = win->cursor_x, save_y = win->cursor_y;
        uint32_t save_fg = win->fg_color, save_bg = win->bg_color;

        size_t caret_idx = cursor_pos;
        if ((int)caret_idx > capacity) caret_idx = (size_t)capacity;
        if (caret_idx > draw_len) caret_idx = draw_len;

        win->cursor_x = xpos[caret_idx];
        win->bg_color = 0xDDDDDD; /* caret background */

        if (caret_idx < draw_len) {
            /* temporarily set fg to 0 for the caret */
            uint32_t original_fg = win->fg_color;
            win->fg_color = 0; /* <- make caret black/hidden color */
            fb_put_char(win, buf[caret_idx]);
            win->fg_color = original_fg; /* restore immediately */
        } else {
            win->fg_color = 0; /* caret empty space also black */
            fb_put_char(win, ' ');
        }

        win->fg_color = save_fg;
        win->bg_color = save_bg;
        win->cursor_x = save_x;
        win->cursor_y = save_y;
    }

}


/* History + clipboard */
static char history[MAX_HISTORY][MAX_CMD_LEN];
static int history_size = 0;
static int history_index = -1;
static char clipboard[MAX_CMD_LEN];

static void add_to_history(const char* cmd) {
    if (!*cmd) return;

    // Check for duplicates
    for (int i = 0; i < history_size; i++) {
        if (strcmp(history[i], cmd) == 0)
            return; // already exists, skip
    }

    if (history_size < MAX_HISTORY) {
        strncpy(history[history_size++], cmd, MAX_CMD_LEN - 1);
        history[history_size - 1][MAX_CMD_LEN - 1] = '\0';
    } else {
        for (int i = 1; i < MAX_HISTORY; i++)
            strcpy(history[i - 1], history[i]);
        strncpy(history[MAX_HISTORY - 1], cmd, MAX_CMD_LEN - 1);
        history[MAX_HISTORY - 1][MAX_CMD_LEN - 1] = '\0';
    }
}


/* Main readline */
int console_readline(Window* win, char* buffer, size_t size) {
    if (*buffer) return 1;

    memset(buffer, 0, size);
    size_t pos = 0, len = 0;
    int shift = 0, ctrl = 0;

    size_t line_start_x = win->cursor_x;
    int capacity = line_capacity_chars(win, line_start_x);

    render_line(win, buffer, len, pos, line_start_x);

    for (;;) {
        unsigned char key = keyboard_read();
        if (!key) { __asm__("hlt"); continue; }

        if (key == 0x2A || key == 0x36) { shift = 1; continue; } // Shift down
        if (key == 0xAA || key == 0xB6) { shift = 0; continue; } // Shift up
        if (key == 0x1D) { ctrl = 1; continue; }  // Ctrl down
        if (key == 0x9D) { ctrl = 0; continue; }  // Ctrl up

        /* Arrow keys */
        if (key == KEY_ARROW_LEFT) {
            if (ctrl) {
                /* move to previous word */
                if (pos > 0) {
                    pos--;
                    /* skip any spaces before the word */
                    while (pos > 0 && buffer[pos] == ' ')
                        pos--;
                    /* skip the word itself */
                    while (pos > 0 && !is_delim(buffer[pos - 1]))
                        pos--;
                }
            } else if (pos > 0) {
                pos--;
            }
            render_line(win, buffer, len, pos, line_start_x);
            continue;
        }

        if (key == KEY_ARROW_RIGHT) {
            if (ctrl) {
                /* move to next word */
                if (pos < len) {
                    /* skip current word if in one */
                    while (pos < len && !is_delim(buffer[pos]))
                        pos++;
                    /* skip following spaces */
                    while (pos < len && buffer[pos] == ' ')
                        pos++;
                }
            } else if (pos < len) {
                pos++;
            }
            render_line(win, buffer, len, pos, line_start_x);
            continue;
        }


        /* History navigation */
        if (key == KEY_ARROW_UP && history_size > 0) {
            if (history_index < history_size - 1) history_index++;
            strncpy(buffer, history[history_size - 1 - history_index], size - 1);
            buffer[size - 1] = '\0';
            len = strlen(buffer);
            if ((int)len > capacity) len = (size_t)capacity;
            pos = len;
            render_line(win, buffer, len, pos, line_start_x);
            continue;
        }
        if (key == KEY_ARROW_DOWN) {
            if (history_index > 0) {
                history_index--;
                strncpy(buffer, history[history_size - 1 - history_index], size - 1);
            } else {
                history_index = -1;
                buffer[0] = '\0';
            }
            len = strlen(buffer);
            if ((int)len > capacity) len = (size_t)capacity;
            pos = len;
            render_line(win, buffer, len, pos, line_start_x);
            continue;
        }

        /* Backspace / Delete */
        if (key == KEY_BACKSPACE && pos > 0) {
            memmove(&buffer[pos - 1], &buffer[pos], len - pos);
            pos--; len--;
            buffer[len] = '\0';
            render_line(win, buffer, len, pos, line_start_x);
            continue;
        }
        if (key == KEY_DELETE && pos < len) {
            memmove(&buffer[pos], &buffer[pos + 1], len - pos - 1);
            len--;
            buffer[len] = '\0';
            render_line(win, buffer, len, pos, line_start_x);
            continue;
        }

        /* Copy (Ctrl+C) */
        if (ctrl && key == 0x2E) {
            strncpy(clipboard, buffer, MAX_CMD_LEN - 1);
            clipboard[MAX_CMD_LEN - 1] = '\0';
            continue;
        }

        /* Paste (Ctrl+V) */
        if (ctrl && key == 0x2F) {
            size_t cliplen = strlen(clipboard);
            if ((int)cliplen > capacity - (int)len)
                cliplen = capacity - (int)len;
            if (cliplen > 0 && len + cliplen < size - 1) {
                memmove(&buffer[pos + cliplen], &buffer[pos], len - pos);
                memcpy(&buffer[pos], clipboard, cliplen);
                pos += cliplen; len += cliplen;
                buffer[len] = '\0';
                render_line(win, buffer, len, pos, line_start_x);
            }
            continue;
        }

        /* Printable input */
        char c = key_printable(key);
        if (shift) c = apply_shift(c);
        if (!c) continue;

        /* Enter */
        if (c == '\n' || c == '\r') {
            buffer[len] = '\0';

            /* redraw once without caret to clear visual highlight */
            render_line(win, buffer, len, (size_t)-1, line_start_x);  // pass invalid caret to disable

            fb_write(win, "\n");
            add_to_history(buffer);
            history_index = -1;
            return 0;
        }


        /* Insert */
        if ((int)len < capacity && len < size - 1) {
            memmove(&buffer[pos + 1], &buffer[pos], len - pos);
            buffer[pos] = c;
            pos++; len++;
            buffer[len] = '\0';
            render_line(win, buffer, len, pos, line_start_x);
        } else {
            render_line(win, buffer, len, pos, line_start_x);
        }
    }
}

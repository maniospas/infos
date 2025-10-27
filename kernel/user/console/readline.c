#include "../console.h"

/* ANSI */
#define RESET  "\033[0m"
#define GREEN  "\033[32m"
#define YELLOW "\033[33m"
#define RED    "\033[31m"
#define BLUE   "\033[34m"

/* Keywords to highlight (green) */
static const char* keywords[] = {
    "help","ls","cd","cat","ps","clear","text",
    "app","kill","to","log","exit", "let", "read", "print"
};
#define NUM_KEYWORDS (sizeof(keywords)/sizeof(keywords[0]))

/* Shift map for symbols */
static const char shift_map[128] = {
    ['1'] = '!', ['2'] = '@', ['3'] = '#', ['4'] = '$',
    ['5'] = '%', ['6'] = '^', ['7'] = '&', ['8'] = '*',
    ['9'] = '(', ['0'] = ')',
    ['-'] = '_', ['='] = '+',
    ['['] = '{', [']'] = '}',
    [';'] = ':', ['\''] = '"',
    [','] = '<', ['.'] = '>',
    ['/'] = '?', ['\\'] = '|',
    ['`'] = '~'
};

static inline char apply_shift(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    if ((unsigned char)c < 128 && shift_map[(int)c]) return shift_map[(int)c];
    return c;
}

static inline int is_delim(char c) {
    return (c == ' ' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '|');
}


static inline const char* color_for_depth(int depth) {
    switch (depth % 3) {
        case 0: return YELLOW;
        case 1: return RED;
        default: return BLUE;
    }
}

/* word == contiguous run of chars not in {space, any parenthesis} */
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
    next_kw: ;
    }
    return 0;
}

/* Repaint the last word (from buffer) in green if keyword, plain otherwise */
static void repaint_last_word(Window* win, const char* buf, int pos) {
    int start, len;
    last_word_bounds(buf, pos, &start, &len);
    if (len <= 0) return;

    /* erase the currently printed last word */
    for (int i = 0; i < len; ++i) fb_removechar(win);

    /* copy it and print colored/plain */
    char tmp[64];
    if (len > 63) len = 63;
    for (int i = 0; i < len; ++i) tmp[i] = buf[start + i];
    tmp[len] = '\0';

    if (matches_keyword(buf, start, len)) {
        fb_write_ansi(win, GREEN);
        fb_write(win, tmp);
        fb_write_ansi(win, RESET);
    } else {
        fb_write(win, tmp);
    }
}

int console_readline(Window* win, char *buffer, size_t size) {
    if (*buffer) return 1;

    size_t pos = 0;
    int shift = 0;
    int depth = 0;

    for (;;) {
        unsigned char key = keyboard_read();
        if (!key) { __asm__("hlt"); continue; }

        /* Shift make/break */
        if (key == 0x2A || key == 0x36) { shift = 1; continue; }
        if (key == 0xAA || key == 0xB6) { shift = 0; continue; }

        /* Backspace */
        if (key == KEY_BACKSPACE) {
            if (pos > 0) {
                char removed = buffer[pos - 1];
                pos--;
                fb_removechar(win);

                /* keep paren depth consistent with what we did on insert */
                if (removed == '(' || removed == '[' || removed == '{') {
                    if (depth > 0) depth--;        /* we had incremented after printing open */
                } else if (removed == ')' || removed == ']' || removed == '}') {
                    depth++;                       /* we had decremented before printing close */
                }

                /* After deletion, always re-evaluate and repaint last word */
                repaint_last_word(win, buffer, (int)pos);
            }
            continue;
        }

        char c = key_printable(key);
        if (shift) c = apply_shift(c);
        if (!c) continue;

        /* Enter: line done. Last word already painted; just terminate. */
        if (c == '\n' || c == '\r') {
            buffer[pos] = '\0';
            fb_write(win, "\n");
            return 0;
        }

        if (pos >= size - 1) continue;

        /* Handle parentheses with alternating colors */
        if (c == '(' || c == '[' || c == '{') {
            fb_write_ansi(win, color_for_depth(depth));
            char s[2] = { c, 0 };
            fb_write(win, s);
            fb_write_ansi(win, RESET);
            buffer[pos++] = c;
            depth++;                /* increase after printing open */
            /* new word starts after delimiter; nothing else to repaint */
            continue;
        }
        if (c == ')' || c == ']' || c == '}') {
            if (depth > 0) depth--; /* choose color for closing level */
            fb_write_ansi(win, color_for_depth(depth));
            char s[2] = { c, 0 };
            fb_write(win, s);
            fb_write_ansi(win, RESET);
            buffer[pos++] = c;
            /* new word starts after delimiter; nothing else to repaint */
            continue;
        }
        if (c == '|') {
            fb_write_ansi(win, color_for_depth(depth+1));
            char s[2] = { c, 0 };
            fb_write(win, s);
            fb_write_ansi(win, RESET);
            buffer[pos++] = c;
            continue;
        }

        /* Space: delimiter for a new word */
        if (c == ' ') {
            char s[2] = { c, 0 };
            fb_write(win, s);
            buffer[pos++] = c;
            /* new word starts now; nothing to repaint */
            continue;
        }

        /* Normal printable char: print, store, then repaint last word */
        {
            char s[2] = { c, 0 };
            fb_write(win, s);
            buffer[pos++] = c;

            /* ALWAYS repaint the last word so it’s green iff it’s a keyword */
            repaint_last_word(win, buffer, (int)pos);
        }
    }
    /* not reached */
    return 0;
}

#ifndef LETOS_H
#define LETOS_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t DEFAULT_FG;
    uint32_t DEFAULT_BG;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t scale_nominator;
    uint32_t scale_denominator;
    uint32_t no_bg_mode;
    uint32_t scroll_limit; // 0 for infinite scrolling
    uint32_t accumulated_scroll_limit;
} Window;

extern void fb_write_ansi(void* window, const char* message);
extern void fb_write_dec(void* window, uint64_t num);
extern void* malloc(size_t size);
extern void* realloc(void* ptr, size_t size);
extern void free(void* ptr);

void main(Window*, const char*);
#define prints(text) fb_write_ansi(window, text)
#define printu(num)  fb_write_dec(window, num)

#ifdef __cplusplus
}
#endif
#endif // LETOS_H

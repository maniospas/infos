#include "screen.h"
#include "../multiboot2.h"
#include <stdint.h>
#include <stddef.h>
#include "spleen-32x64_font32x64.h"
#include "spleen-16x32_font16x32.h"
#include "spleen-8x16_font8x16.h"
#include "vga.h"

// Multiboot2 framebuffer info
uint32_t *fb_addr = NULL;
uint32_t fb_width = 0;
uint32_t fb_height = 0;
uint32_t fb_pitch = 0;
uint8_t  fb_bpp = 0;
uint32_t margin = 20;
#define scale win->scale_nominator/win->scale_denominator
#define invscale win->scale_denominator/win->scale_nominator
#define CHAR_W (font_size/2*scale)
#define CHAR_H (font_size*scale)
uint32_t font_size = 64;

typedef struct {
    const void *data;
    uint32_t width;
    uint32_t height;
    uint32_t first;
    uint32_t last;
} FontInfo;

// Declare each Spleen variant
static const FontInfo FONTS[] = {
    { (const void *)font8x16,  8, 16,  FONT8X16_FIRST,  FONT8X16_LAST  },
    { (const void *)font16x32, 16, 32, FONT16X32_FIRST, FONT16X32_LAST },
    { (const void *)font32x64, 32, 64, FONT32X64_FIRST, FONT32X64_LAST },
};
static const size_t FONT_COUNT = sizeof(FONTS) / sizeof(FONTS[0]);

static const FontInfo* get_best_font(uint32_t desired_size) {
    const FontInfo* best = &FONTS[0];
    uint32_t best_diff = (desired_size > FONTS[0].height)
        ? desired_size - FONTS[0].height
        : FONTS[0].height - desired_size;

    for (size_t i = 1; i < FONT_COUNT; i++) {
        uint32_t diff = (desired_size > FONTS[i].height)
            ? desired_size - FONTS[i].height
            : FONTS[i].height - desired_size;
        if (diff < best_diff)
            best = &FONTS[i], best_diff = diff;
    }
    return best;
}


void fb_set_scale(Window* win, uint32_t nom, uint32_t denom) {
    win->scale_nominator = nom;
    win->scale_denominator = denom*3;
}

static void set_color(Window* win, uint32_t color) {
    win->fg_color = color;
}

/**
 * Map the framebuffer region into identity-mapped space.
 * This function assumes paging uses 2 MiB pages and an already identity-mapped kernel.
 */
extern uint64_t pml4_table[];
extern uint64_t pdpt_table[];
extern uint64_t pd_table[];

extern uint64_t pml4_table[];
extern uint64_t pdpt_table[];


void map_framebuffer(uint64_t phys_addr, uint64_t size_bytes) {
    uint64_t base = phys_addr & ~0x1FFFFFULL;
    uint64_t end  = (phys_addr + size_bytes + 0x1FFFFFULL) & ~0x1FFFFFULL;
    uint64_t pdpt_index = base >> 30;
    uint64_t *pd;
    if (!(pdpt_table[pdpt_index] & 1)) {
        // allocate one statically or from a pool; here we reuse our single PD
        pd = (uint64_t *)((uintptr_t)&pd_table);
        pdpt_table[pdpt_index] = (uint64_t)pd | 0x03; // Present | RW
    } 
    else 
        pd = (uint64_t *)(pdpt_table[pdpt_index] & ~0xFFFULL);

    // Now fill PD entries for that region
    for (uint64_t addr = base; addr < end; addr += 0x200000) {
        uint64_t pd_index = (addr >> 21) & 0x1FFULL;
        pd[pd_index] = addr | 0x83ULL; // Present | RW | 2 MiB
    }
}

void fb_init(void *mb_info_addr) {
    struct multiboot_tag *tag;
    for (tag = (void *)((uint8_t *)mb_info_addr + 8);
         tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (void *)((uint8_t *)tag + ((tag->size + 7) & ~7))) {

        if (tag->type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER) {
            struct multiboot_tag_framebuffer *fb = (struct multiboot_tag_framebuffer *)tag;

            fb_addr   = (uint32_t *)(uintptr_t)fb->framebuffer_addr;
            fb_width  = fb->framebuffer_width;
            fb_height = fb->framebuffer_height;
            fb_pitch  = fb->framebuffer_pitch;
            fb_bpp    = fb->framebuffer_bpp;

            if (fb_bpp != 32 && fb_bpp != 24) {
                //vga_write("Unsupported framebuffer format\n");
                for (;;) __asm__("hlt");
            }
            
            map_framebuffer((uint64_t)fb_addr, (uint64_t)fb_pitch * fb_height);
            return;
        }
    }
    vga_write("Framebuffer not found!\n");
    for (;;) __asm__("hlt");
}

void fb_putpixel(int x, int y, uint64_t color) {
    fb_addr[y*fb_width + x] = color;
}

void fb_window_border(Window *win, char* title, uint32_t color, int appid) {
    uint32_t x0 = win->x;
    uint32_t y0 = win->y;
    uint32_t has_title = title && *title;
    if (has_title) 
        y0 -= CHAR_H;
    uint32_t x1 = win->x + win->width - 1;
    uint32_t y1 = win->y + win->height - 1;
    for (uint32_t x = x0; x <= x1+8; x++) {
        for(int i=1;i<12;i++)
            fb_putpixel(x, y1+i, 0x444444);
    }
    for (uint32_t x = x0; x <= x1; x++) {
        fb_putpixel(x, y0-1, win->DEFAULT_FG);
        fb_putpixel(x, y1+1, win->DEFAULT_FG);
        if(has_title)
            for(uint32_t y=0;y<CHAR_H;++y) 
                fb_putpixel(x, y0+y, win->DEFAULT_FG);
    }
    for (uint32_t y = y0; y <= y1; y++) {
        fb_putpixel(x0, y, win->DEFAULT_FG);
        fb_putpixel(x1, y, win->DEFAULT_FG);
    }
    for (uint32_t y = y0; y <= y1; y++) {
        for(uint32_t i=1;i<8;i++)
            fb_putpixel(x1+i, y, 0x444444);
    }
    win->cursor_x = win->x + margin;
    win->cursor_y = win->y - CHAR_H;
    if (has_title) {
        win->fg_color = color;
        win->bg_color = win->DEFAULT_FG;
        fb_write(win, title);
        win->fg_color = 0x006600;
        win->cursor_x = win->x+win->width-120;
        if(appid>=0) {
            fb_write(win, " app");
            fb_write_dec(win, appid);
        }
        fb_write(win, "\n");
        win->fg_color = win->DEFAULT_FG;
        win->bg_color = win->DEFAULT_BG;
    }
    win->cursor_y = win->y + margin;
}

void fb_clear(Window *win) {
    win->cursor_x = win->x + margin;
    win->cursor_y = win->y + margin;

    int sx = win->x;
    int sy = win->y;
    int w  = win->width;
    int h  = win->height;
    uint32_t color = win->bg_color;

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b = (color >> 0)  & 0xFF;

    for (int y = 0; y < h; y++) {
        uint8_t *row = (uint8_t *)fb_addr + (sy + y) * fb_pitch + sx * (fb_bpp / 8);
        for (int x = 0; x < w; x++) {
            size_t off = x * (fb_bpp / 8);
            row[off + 0] = b;
            row[off + 1] = g;
            row[off + 2] = r;
            if (fb_bpp == 32)
                row[off + 3] = 0; // alpha ignored
        }
    }
}
void fb_clearline(Window *win, size_t line_start_cursor_x) {
    if (!fb_addr) return;

    int sx = (int)line_start_cursor_x;
    if (sx < win->x + margin)
        sx = win->x + margin;

    int sy = win->cursor_y;
    int ex = win->x + win->width - margin;
    int ey = sy + CHAR_H;
    uint32_t color = win->bg_color;

    // Clamp drawing to window bounds
    if (sy < win->y)
        sy = win->y;
    if (ey > win->y + win->height)
        ey = win->y + win->height;
    if (sx >= ex || sy >= ey)
        return; // nothing visible

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b = (color >> 0)  & 0xFF;

    for (int y = sy; y < ey; y++) {
        if (y < 0 || y >= (int)fb_height) continue;
        uint8_t *row = (uint8_t *)fb_addr + y * fb_pitch;
        for (int x = sx; x < ex && x < (int)fb_width; x++) {
            size_t off = x * (fb_bpp / 8);
            row[off + 0] = b;
            row[off + 1] = g;
            row[off + 2] = r;
            if (fb_bpp == 32)
                row[off + 3] = 0;
        }
    }

    // Reset cursor safely to start of the cleared line within bounds
    win->cursor_x = win->x + margin;
}


void fb_put_char(Window* win, char c) {
    if (!fb_addr) return;

    if (c == '\n') {
        win->cursor_x = win->x + margin;
        win->cursor_y += CHAR_H;
        return;
    }

    const FontInfo* fontinfo = get_best_font(font_size);
    uint32_t fw = fontinfo->width;
    uint32_t fh = fontinfo->height;

    if ((uint8_t)c < fontinfo->first || (uint8_t)c > fontinfo->last)
        return; // Ignore unsupported characters
    if(win->height<2*CHAR_H+margin*2)
        return;
    // Handle scrolling
    while (win->cursor_y + CHAR_H + margin > win->y + win->height) {
        // Amount by which we need to scroll up so that bottom aligns at y + height - margin
        uint32_t scroll_amount = (win->cursor_y + CHAR_H + margin) - (win->y + win->height);

        // Clamp scroll amount to not exceed CHAR_H (usually 1 line)
        if (scroll_amount > CHAR_H)
            scroll_amount = CHAR_H;

        // Scroll the framebuffer up by scroll_amount
        for (uint32_t y = win->y; y < win->y + win->height - scroll_amount; y++) {
            for (uint32_t x = win->x; x < win->x + win->width; x++) {
                fb_addr[y * fb_width + x] = fb_addr[(y + scroll_amount) * fb_width + x];
            }
        }

        // Clear the newly exposed area at the bottom
        for (uint32_t y = win->y + win->height - scroll_amount; y < win->y + win->height; y++) {
            for (uint32_t x = win->x+1; x < win->x + win->width-1; x++) {
                fb_addr[y * fb_width + x] = win->bg_color;
            }
        }

        // Move the cursor up by the same scroll amount
        win->cursor_y -= scroll_amount;
    }


    // Render character
    // Render character safely
    for (uint32_t y = 0; y < CHAR_H; y++) {
        int draw_y = win->cursor_y + y;
        if (draw_y < 0 || draw_y >= fb_height) continue;

        for (uint32_t x = 0; x < CHAR_W; x++) {
            int draw_x = win->cursor_x + x;
            if (draw_x < 0 || draw_x >= fb_width) continue;

            uint32_t src_x = (uint32_t)(x * invscale);
            uint32_t src_y = (uint32_t)(y * invscale);
            if (src_x >= fw || src_y >= fh)
                continue;

            uint8_t bit = 0;
            if (fw == FONT8X16_WIDTH) {
                const uint8_t (*font)[FONT8X16_WIDTH][FONT8X16_HEIGHT] =
                    (const void *)fontinfo->data;
                bit = font[(uint8_t)c - fontinfo->first][src_x][src_y];
            } else if (fw == FONT16X32_WIDTH) {
                const uint8_t (*font)[FONT16X32_WIDTH][FONT16X32_HEIGHT] =
                    (const void *)fontinfo->data;
                bit = font[(uint8_t)c - fontinfo->first][src_x][src_y];
            } else {
                const uint8_t (*font)[FONT32X64_WIDTH][FONT32X64_HEIGHT] =
                    (const void *)fontinfo->data;
                bit = font[(uint8_t)c - fontinfo->first][src_x][src_y];
            }

            uint32_t color = bit ? win->fg_color : win->bg_color;
            if (!win->no_bg_mode && color == win->bg_color)
                continue;

            fb_putpixel(draw_x, draw_y, color);
        }
    }


    // Advance cursor
    win->cursor_x += CHAR_W;
    if (win->cursor_x + CHAR_W >= fb_width - margin) {
        win->cursor_x = win->x + margin;
        win->cursor_y += CHAR_H;
    }
}



void fb_removechar(Window* win) {
    if (win->cursor_x == win->x+margin) {
        if (win->cursor_y == win->x+margin) return;
        win->cursor_y -= CHAR_H;
        win->cursor_x = win->width - (win->width % CHAR_W);
        if (win->cursor_x >= fb_width-margin) win->cursor_x = fb_width - CHAR_W;
    } 
    else 
        win->cursor_x -= CHAR_W;
    
    for (uint32_t y = 0; y < CHAR_H && win->y+y<win->height; y++)
        for (uint32_t x = 0; x < CHAR_W && win->x+x<win->width; x++)
            fb_putpixel(win->cursor_x + x, win->cursor_y + y, win->bg_color);
}

void fb_write(Window* win, const char *s) {
    while (*s) fb_put_char(win, *s++);
}

static uint32_t ansi_to_rgb(int code) {
    switch (code) {
        case 30: return 0x1F1F1F;
        case 31: return 0xEE7A7A;
        case 32: return 0x66DD88;
        case 33: return 0xEEEE88;
        case 34: return 0x5599EE;
        case 35: return 0xEE88EE;
        case 36: return 0x66EEEE;
        case 37: return 0xEEEEEE;
        case 90: return 0x7A7A7A;
        default: return 0xEEEEEE;
    }
}

// Write unsigned integer in decimal
void fb_write_dec(Window* win, uint64_t num) {
    char buf[32];
    int i = 0;

    if (num == 0) {
        fb_put_char(win, '0');
        return;
    }

    while (num > 0 && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--) {
        fb_put_char(win, buf[i]);
    }
}

// Write 64-bit value in hexadecimal (with no 0x prefix)
void fb_write_hex(Window* win, uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    int shift = 60;
    int leading = 1;
    for (; shift >= 0; shift -= 4) {
        uint8_t nibble = (val >> shift) & 0xF;
        if (nibble == 0 && leading && shift)
            continue;
        leading = 0;
        fb_put_char(win, hex[nibble]);
    }
    if (leading)
        fb_put_char(win, '0');
}

void fb_write_ansi(Window* win, const char *s) {
    while (*s) {
        if (*s == '\x1b' && *(s + 1) == '[') {
            s += 2;
            int val = 0;
            while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
            if (*s == 'm') {
                if (val == 0) { win->fg_color = win->DEFAULT_FG; win->bg_color = win->DEFAULT_BG; }
                else win->fg_color = ansi_to_rgb(val);
            }
            s++;
        } 
        else 
            fb_put_char(win, *s++);
    }
}

void fb_bar(Window* win, long int value, long int max_value, size_t width) {
    if (!fb_addr || max_value <= 0) return;

    // Clamp value range
    if (value < 0) value = 0;
    if (value > max_value) value = max_value;

    // Compute dimensions
    int bar_x = win->cursor_x;
    int bar_y = win->cursor_y + CHAR_H/2-CHAR_H/12;
    int bar_w = width;
    int bar_h = CHAR_H / 3;  // Half the character height looks nice

    // Compute filled width proportionally
    int filled_w = (int)(((long double)value / (long double)max_value) * bar_w);

    // Draw bar background
    for (int y = 0; y < bar_h; y++) {
        for (int x = 0; x < bar_w; x++) {
            uint32_t color = (x < filled_w) ? win->fg_color : win->bg_color;
            fb_putpixel(bar_x + x, bar_y + y, color);
        }
    }

    // Optional border around bar
    uint32_t border_color = win->DEFAULT_FG;
    for (int x = -1; x <= bar_w; x++) {
        fb_putpixel(bar_x + x, bar_y - 1, border_color);
        fb_putpixel(bar_x + x, bar_y + bar_h, border_color);
    }
    for (int y = -1; y <= bar_h; y++) {
        fb_putpixel(bar_x - 1, bar_y + y, border_color);
        fb_putpixel(bar_x + bar_w, bar_y + y, border_color);
    }

    // Move cursor down for next elements
    win->cursor_x += bar_w+10;
}

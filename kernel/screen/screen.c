#include "screen.h"
#include "../multiboot2.h"
#include <stdint.h>
#include <stddef.h>
#include "font8x16.h"
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
#define CHAR_W (8*scale)
#define CHAR_H (16*scale)

void fb_set_scale(Window* win, uint32_t nom, uint32_t denom) {
    win->scale_nominator = nom;
    win->scale_denominator = denom;
}

static void set_color(Window* win, uint32_t color) {
    win->fg_color = color;
}

static inline uint64_t align_down(uint64_t addr, uint64_t align) {
    return addr & ~(align - 1);
}

static inline uint64_t align_up(uint64_t addr, uint64_t align) {
    return (addr + align - 1) & ~(align - 1);
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
//extern uint64_t pd_tables[512][512]; // optional, but shows concept


void map_framebuffer(uint64_t phys_addr, uint64_t size_bytes) {
    uint64_t base = phys_addr & ~0x1FFFFFULL;
    uint64_t end  = (phys_addr + size_bytes + 0x1FFFFFULL) & ~0x1FFFFFULL;

    // Calculate PDPT index (each PD covers 1 GiB)
    uint64_t pdpt_index = base >> 30; // 1 GiB per PDPT entry

    // Find or create a PD for that 1 GiB region
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

    // Fallback: framebuffer not found
    //vga_write("Framebuffer not found!\n");
    for (;;) __asm__("hlt");
}

// === Pixel & char drawing ===
static inline void fb_putpixel(int x, int y, uint64_t color) {
    fb_addr[y*fb_width + x] = color;
}

void fb_window_border(Window *win, char* title, uint32_t color) {
    int x0 = win->x;
    int y0 = win->y;
    int has_title = title && *title;
    if (has_title) 
        y0 -= CHAR_H;
    int x1 = win->x + win->width - 1;
    int y1 = win->y + win->height - 1;
    for (int x = x0; x <= x1+8; x++) {
        for(int i=1;i<12;i++)
            fb_putpixel(x, y1+i, 0x444444);
    }
    for (int x = x0; x <= x1; x++) {
        fb_putpixel(x, y0-1, win->DEFAULT_FG);
        fb_putpixel(x, y1+1, win->DEFAULT_FG);
        if(has_title)
            for(int y=0;y<CHAR_H;++y) 
                fb_putpixel(x, y0+y, win->DEFAULT_FG);
    }
    for (int y = y0; y <= y1; y++) {
        fb_putpixel(x0, y, win->DEFAULT_FG);
        fb_putpixel(x1, y, win->DEFAULT_FG);
    }
    for (int y = y0; y <= y1; y++) {
        for(int i=1;i<8;i++)
            fb_putpixel(x1+i, y, 0x444444);
    }
    win->cursor_x = win->x + margin;
    win->cursor_y = win->y - CHAR_H;
    if (has_title) {
        win->fg_color = color;
        win->bg_color = win->DEFAULT_FG;
        fb_write(win, title);
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


uint8_t * font8x16 = ATIx550_8x16; //ATIx550_8x16;IBM_VGA_8x16;

void fb_put_char(Window* win, char c) {
    if (!fb_addr) return;

    if (c == '\n') {
        win->cursor_x = win->x+margin;
        win->cursor_y += CHAR_H;
    } else {
        const uint8_t *glyph = &font8x16[16 * (uint8_t)c];
        for (uint32_t y = 0; y < CHAR_H && y<win->y+win->height; y++) {
            for (uint32_t x = 0; x < CHAR_W && x<win->width+win->x; x++) {
                if (glyph[y * invscale] & (1 << (7 - x * invscale)))
                    fb_putpixel(win->cursor_x + x, win->cursor_y + y, win->fg_color);
                else
                    fb_putpixel(win->cursor_x + x, win->cursor_y + y, win->bg_color);
            }
        }
        win->cursor_x += CHAR_W;
        if (win->cursor_x + CHAR_W >= fb_width - margin) {
            win->cursor_x = win->x+margin;
            win->cursor_y += CHAR_H;
        }
    }

    // === handle scrolling ===
    if (win->cursor_x!=win->x+margin && win->cursor_y + CHAR_H >= win->y+win->height - margin && win->cursor_y>=CHAR_H) {
        for (uint32_t y = win->y; y < win->y+win->height - CHAR_H; y++) 
            for (uint32_t x = win->x; x < win->x+win->width; x++) 
                fb_addr[y*fb_width + x] = fb_addr[(y+CHAR_H)*fb_width + x];
        for (uint32_t y = fb_height - CHAR_H; y < fb_height; y++) {
            for (uint32_t x = win->x; x < win->x+win->width; x++) 
                fb_addr[y*fb_width + x] = win->bg_color;
        }
        win->cursor_y -= CHAR_H;
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

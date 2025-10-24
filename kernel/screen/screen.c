#include "screen.h"
#include "../multiboot2.h"
#include <stdint.h>
#include <stddef.h>
#include "font8x16.h"
#include "vga.h"

// Multiboot2 framebuffer info
static uint32_t *fb_addr = NULL;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint8_t  fb_bpp = 0;
static uint32_t scale_nominator = 1;
static uint32_t scale_denominator = 1;
uint32_t margin = 0;
#define scale scale_nominator/scale_denominator
#define invscale scale_denominator/scale_nominator

static size_t cursor_x, cursor_y;
static uint32_t fg_color = 0xFFFFFF; // white
static uint32_t bg_color = 0x000000; // black

#define CHAR_W (8*scale)
#define CHAR_H (16*scale)

void fb_set_scale(uint32_t nom, uint32_t denom) {
    scale_nominator = nom;
    scale_denominator = denom;
}

static void set_color(uint32_t color) {
    fg_color = color;
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
                vga_write("Unsupported framebuffer format\n");
                for (;;) __asm__("hlt");
            }
            
            map_framebuffer((uint64_t)fb_addr, (uint64_t)fb_pitch * fb_height);

            return;
        }
    }

    // Fallback: framebuffer not found
    vga_write("Framebuffer not found!\n");
    for (;;) __asm__("hlt");
}

// === Pixel & char drawing ===

static inline void fb_putpixel(int x, int y, uint64_t color) {
    fb_addr[y*fb_width + x] = color;



    /*for (int i = 0; i < fb_width * fb_height; i++) {
        fb_addr[i] = 0xFFFFFFFF; // Fill screen with white
    }
    size_t offset = y * fb_pitch + x * (fb_bpp / 8);
    fb_addr[offset] = color;
    if (!fb_addr) return;
    if (x < 0 || y < 0 || x >= (int)fb_width || y >= (int)fb_height) return;

    uint8_t *fb8 = (uint8_t *)fb_addr;
    size_t offset = y * fb_pitch + x * (fb_bpp / 8);

    // Framebuffer is usually BGRA (little endian)
    fb8[offset + 0] = (color >> 0)  & 0xFF; // Blue
    fb8[offset + 1] = (color >> 8)  & 0xFF; // Green
    fb8[offset + 2] = (color >> 16) & 0xFF; // Red
    if (fb_bpp == 32)
        fb8[offset + 3] = 0xFF; // Alpha ignored*/
}

void fb_clear(void) {
    cursor_x = margin;
    cursor_y = margin;
    for (size_t y = 0; y < fb_height; y++) {
        uint8_t *row = (uint8_t *)fb_addr + y * fb_pitch;
        for (size_t x = 0; x < fb_width; x++) {
            size_t off = x * (fb_bpp / 8);
            row[off + 0] = 0; // blue
            row[off + 1] = 0; // green
            row[off + 2] = 0; // red
            if (fb_bpp == 32)
                row[off + 3] = 0;
        }
    }
    cursor_x = cursor_y = margin;
}

uint8_t * font8x16 = ATIx550_8x16;

void fb_put_char(char c) {
    if (c == '\n') {
        cursor_x = margin;
        cursor_y += CHAR_H;
        if (cursor_y + CHAR_H >= fb_height-margin) cursor_y = 0;
        return;
    }
    const uint8_t *glyph = &font8x16[16 * (uint8_t)c];
    for (uint32_t y = 0; y < CHAR_H; y++) {
        for (uint32_t x = 0; x < CHAR_W; x++) {
            if (glyph[y*invscale] & (1 << (7 - x*invscale)))
                fb_putpixel(cursor_x+x, cursor_y+y, fg_color);
            else
                fb_putpixel(cursor_x+x, cursor_y+y, bg_color);
        }
    }
    cursor_x += CHAR_W;
    if (cursor_x + CHAR_W >= fb_width-margin) {
        cursor_x = margin;
        cursor_y += CHAR_H;
    }
}

void fb_removechar(void) {
    if (cursor_x == margin) {
        if (cursor_y == margin) return;
        cursor_y -= CHAR_H;
        cursor_x = fb_width - (fb_width % CHAR_W);
        if (cursor_x >= fb_width-margin) cursor_x = fb_width - CHAR_W;
    } 
    else 
        cursor_x -= CHAR_W;
    
    for (uint32_t y = 0; y < CHAR_H; y++)
        for (uint32_t x = 0; x < CHAR_W; x++)
            fb_putpixel(cursor_x + x, cursor_y + y, bg_color);
}

void fb_write(const char *s) {
    while (*s) fb_put_char(*s++);
}

static uint32_t ansi_to_rgb(int code) {
    switch (code) {
        case 30: return 0x000000;
        case 31: return 0xAA0000;
        case 32: return 0x00AA00;
        case 33: return 0xAAAA00;
        case 34: return 0x0000AA;
        case 35: return 0xAA00AA;
        case 36: return 0x00AAAA;
        case 37: return 0xFFFFFF;
        default: return 0xFFFFFF;
    }
}

// Write unsigned integer in decimal
void fb_write_dec(uint64_t num) {
    char buf[32];
    int i = 0;

    if (num == 0) {
        fb_put_char('0');
        return;
    }

    while (num > 0 && i < (int)sizeof(buf)) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--) {
        fb_put_char(buf[i]);
    }
}

// Write 64-bit value in hexadecimal (with no 0x prefix)
void fb_write_hex(uint64_t val) {
    const char *hex = "0123456789ABCDEF";
    int shift = 60;
    int leading = 1;

    for (; shift >= 0; shift -= 4) {
        uint8_t nibble = (val >> shift) & 0xF;
        if (nibble == 0 && leading && shift)
            continue;
        leading = 0;
        fb_put_char(hex[nibble]);
    }
    if (leading)
        fb_put_char('0');
}

void fb_write_ansi(const char *s) {
    while (*s) {
        if (*s == '\x1b' && *(s + 1) == '[') {
            s += 2;
            int val = 0;
            while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
            if (*s == 'm') {
                if (val == 0) { fg_color = 0xFFFFFF; bg_color = 0x000000; }
                else fg_color = ansi_to_rgb(val);
            }
            s++;
        } 
        else 
            fb_put_char(*s++);
    }
}

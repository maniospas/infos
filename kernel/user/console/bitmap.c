#include "../console.h"
#include "../../screen/screen.h"
#include "../../file/fat32.h"

// -----------------------------------------------------------------------------
// Fast BMP draw: reads the entire image into memory once
// -----------------------------------------------------------------------------
void fb_image_from_file(Window *win, int file_id, size_t target_width, size_t target_height) {
    if (file_id < 0) {
        fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid file handle.\n");
        return;
    }

    uint8_t header[54];
    size_t read = fat32_read_chunk(file_id, header, 54, 0);
    if (read < 54) {
        fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Failed to read BMP header.\n");
        return;
    }

    // --- Verify BMP signature ---
    if (header[0] != 'B' || header[1] != 'M') {
        fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Unsupported image format.\n");
        return;
    }

    // --- Parse BMP header ---
    uint32_t data_offset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
    uint32_t width       = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    uint32_t height      = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    uint16_t bpp         = header[28] | (header[29] << 8);

    if (width == 0 || height == 0) return;
    if (!target_width)  target_width  = width;
    if (!target_height) target_height = height;

    if (bpp != 24 && bpp != 32) {
        fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Unsupported BMP bit depth: ");
        fb_write_dec(win, bpp);
        fb_write(win, "\n");
        return;
    }

    uint32_t bytes_per_pixel = bpp / 8;
    uint32_t row_size = ((width * bytes_per_pixel + 3) & ~3); // 4-byte aligned
    size_t image_size = (size_t)row_size * height;

    // --- Allocate full image buffer ---
    uint8_t *img = malloc(image_size);
    if (!img) {
        fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Not enough memory to load image.\n");
        return;
    }

    // --- Read the entire image into RAM ---
    size_t r = fat32_read_chunk(file_id, img, image_size, data_offset);
    if (r < image_size) {
        fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Incomplete BMP read (file too short).\n");
        free(img);
        return;
    }

    // --- Render to framebuffer ---
    size_t x0 = win->cursor_x;
    size_t y0 = win->cursor_y;

    for (uint32_t ty = 0; ty < target_height; ty++) {
        uint32_t src_y = (uint64_t)ty * height / target_height;
        if (src_y >= height) src_y = height - 1;

        const uint8_t *row = img + (size_t)(height - 1 - src_y) * row_size;
        uint32_t draw_y = y0 + ty;
        if (draw_y >= win->y + win->height) break;

        for (uint32_t tx = 0; tx < target_width; tx++) {
            uint32_t src_x = (uint64_t)tx * width / target_width;
            if (src_x >= width) src_x = width - 1;

            const uint8_t *px = row + src_x * bytes_per_pixel;
            uint8_t b = px[0];
            uint8_t g = px[1];
            uint8_t r8 = px[2];
            uint32_t color = (r8 << 16) | (g << 8) | b;

            uint32_t draw_x = x0 + tx;
            if (draw_x >= win->x + win->width) break;

            fb_putpixel(draw_x, draw_y, color);
        }
    }

    free(img);
    win->cursor_y += target_height;
}






// BACKUP: the following implementation is buggy


// #include "../console.h"
// #include "../../screen/screen.h"
// #include "../../file/fat32.h"

// #define BMP_ROW_CACHE 16  // number of rows to read at once

// // -----------------------------------------------------------------------------
// // Draw BMP image efficiently from a FAT32 file handle (buffered streaming)
// // -----------------------------------------------------------------------------
// void fb_image_from_file(Window *win, int file_id, size_t target_width, size_t target_height) {
//     if (file_id < 0) {
//         fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Invalid file handle.\n");
//         return;
//     }

//     uint8_t header[54];
//     size_t read = fat32_read_chunk(file_id, header, 54, 0);
//     if (read < 54) {
//         fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Failed to read BMP header.\n");
//         return;
//     }

//     // --- Verify BMP header ---
//     if (header[0] != 'B' || header[1] != 'M') {
//         fb_write(win, "\x1b[31mERROR\x1b[0m Image format not supported\n");
//         return;
//     }

//     // --- Parse header ---
//     uint32_t data_offset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
//     uint32_t width       = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
//     uint32_t height      = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
//     uint16_t bpp         = header[28] | (header[29] << 8);

//     if (width == 0 || height == 0) return;
//     if (!target_width)  target_width  = width;
//     if (!target_height) target_height = height;

//     if (bpp != 24 && bpp != 32) {
//         fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Unsupported BMP bit depth: ");
//         fb_write_dec(win, bpp);
//         return;
//     }

//     uint32_t bytes_per_pixel = bpp / 8;
//     uint32_t row_size = ((width * bytes_per_pixel + 3) & ~3); // 4-byte aligned rows

//     // --- Allocate cache for multiple rows ---
//     size_t cache_bytes = row_size * BMP_ROW_CACHE;
//     uint8_t *cache = malloc(cache_bytes);
//     if (!cache) {
//         fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Out of memory for BMP cache.\n");
//         return;
//     }

//     uint32_t cached_start_row = 0;
//     uint32_t cached_rows = 0;

//     size_t x0 = win->cursor_x;
//     size_t y0 = win->cursor_y;

//     // --- Draw loop (top-to-bottom rendering, BMP is bottom-to-top) ---
//     for (uint32_t ty = 0; ty < target_height; ty++) {
//         uint32_t src_y = (uint64_t)ty * height / target_height;
//         if (src_y >= height) src_y = height - 1;
//         uint32_t fetch_row = height - 1 - src_y;  // BMP rows stored bottom-up

//         // --- Load cache block if necessary ---
//         if (fetch_row < cached_start_row ||
//             fetch_row >= cached_start_row + cached_rows) {
//             // Compute which block of rows to fetch
//             uint32_t start_row = (fetch_row >= BMP_ROW_CACHE)
//                                    ? fetch_row - (BMP_ROW_CACHE - 1)
//                                    : 0;
//             size_t offset = data_offset + (size_t)(height - 1 - start_row) * row_size;
//             size_t rows_to_read = (height - start_row >= BMP_ROW_CACHE)
//                                   ? BMP_ROW_CACHE
//                                   : (height - start_row);
//             size_t bytes_to_read = rows_to_read * row_size;

//             size_t r = fat32_read_chunk(file_id, cache, bytes_to_read, offset);
//             if (r < bytes_to_read) {
//                 fb_write_ansi(win, "\x1b[31mERROR\x1b[0m Incomplete BMP read.\n");
//                 break;
//             }

//             cached_start_row = start_row;
//             cached_rows = rows_to_read;
//         }

//         const uint8_t *row = cache + (fetch_row - cached_start_row) * row_size;
//         uint32_t draw_y = y0 + ty;
//         if (draw_y >= win->y + win->height) break;

//         for (uint32_t tx = 0; tx < target_width; tx++) {
//             uint32_t src_x = (uint64_t)tx * width / target_width;
//             if (src_x >= width) src_x = width - 1;

//             const uint8_t *px = row + src_x * bytes_per_pixel;
//             uint8_t b = px[0];
//             uint8_t g = px[1];
//             uint8_t r8 = px[2];
//             uint32_t color = (r8 << 16) | (g << 8) | b;

//             uint32_t draw_x = x0 + tx;
//             if (draw_x >= win->x + win->width) break;

//             fb_putpixel(draw_x, draw_y, color);
//         }
//     }

//     free(cache);
//     win->cursor_y += target_height;
// }

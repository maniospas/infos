#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "user/console.h"
#include "user/application.h"

extern uint32_t focus_id;
void update_layout(Window* fullscreen, Application* apps, uint32_t MAX_APPLICATIONS, size_t total_height) {
    // Left-side fullscreen console
    fullscreen->x = 20;
    fullscreen->y = 60;
    fullscreen->height = total_height;

    // Right-hand grid
    uint32_t spacing_x = 20;
    uint32_t spacing_y = 20;
    uint32_t right_x = fullscreen->x + fullscreen->width + spacing_x;
    uint32_t right_y = fullscreen->y;
    uint32_t right_width = fullscreen->width;
    uint32_t right_height = total_height;

    // Grid constants
    const uint32_t cols = 2;
    const uint32_t row_height = 180;
    const uint32_t subcol_width = (right_width - spacing_x) / cols;

    // For each app slot (constant grid position)
    for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
        Window* w = apps[i].window;
        if (!w) continue;

        // Determine slot position by app index
        uint32_t slot_index = i - 1;
        uint32_t col = 1-slot_index % cols;
        uint32_t row = slot_index / cols;

        w->x = right_x + col * (subcol_width + spacing_x);
        w->y = right_y + row * (row_height + spacing_y);
        w->width = subcol_width;
        w->height = row_height;

        // Hide inactive apps
        // if (apps[i].run == NULL || w == fullscreen)
        //     w->height = 0; // collapsed
    }
}

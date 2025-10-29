#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "user/console.h"
#include "user/application.h"

extern uint32_t focus_id;

void update_layout(Window* fullscreen, Application* apps, uint32_t MAX_APPLICATIONS, size_t total_height) {
    uint32_t active_count = 0;
    for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
        if (apps[i].run != NULL && apps[i].window && apps[i].window != fullscreen)
            active_count++;
    }

    if (active_count == 0)
        return;

    // Console covers full left-hand side
    fullscreen->x = 20;
    fullscreen->y = 80; // unchanged
    fullscreen->width = fullscreen->width; // left column width
    fullscreen->height = total_height;

    // Right-hand side positioning
    uint32_t spacing_x = 20;
    uint32_t right_x = fullscreen->x + fullscreen->width + spacing_x;
    uint32_t right_y = fullscreen->y;
    uint32_t right_width = fullscreen->width;
    uint32_t right_height = total_height;

    // Find how many windows will be visible (excluding fullscreen console)
    uint32_t placed = 0;

    // Compute total height distribution
    uint32_t available_height = right_height;
    uint32_t focused_h = fullscreen->height;//(active_count == 1) ? available_height : available_height / 2;
    uint32_t other_h = 180;

    uint32_t y_cursor = right_y;

    for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
        if (apps[i].run == NULL || apps[i].window == NULL || apps[i].window == fullscreen) {
            y_cursor += other_h; 
            y_cursor += 20;
            continue;
        }

        Window* w = apps[i].window;

        w->x = right_x;
        w->width = right_width;

        /*if (i == focus_id) {
            // Focused window (or default first)
            w->y = y_cursor;
            w->height = focused_h;
            y_cursor += focused_h;
        } else if(focus_id) {
            w->height = 0;
        } else*/ {
            // Other active windows
            w->y = y_cursor;
            w->height = other_h;
            // no margin
        }
        y_cursor += other_h; 
        y_cursor += 20;

        placed++;
    }
}

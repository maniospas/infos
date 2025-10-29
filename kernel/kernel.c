#include "interrupts.h"
#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "user/console.h"
#include "user/application.h"
#include "memory/memory.h" 

extern void* multiboot_info_ptr;
uint32_t text_size = 1;
extern uint32_t margin;
int focus_id = 0;

__attribute__((noreturn))
void kernel_main(void) {
    fb_init(multiboot_info_ptr);
    memory_init(multiboot_info_ptr);
    paging_map_heap();
    memory_buddy_init();

    Window* fullscreen = malloc(sizeof(Window));
    init_fullscreen(fullscreen);
    fullscreen->bg_color = 0;
    fb_clear(fullscreen);
    fb_set_scale(fullscreen, 6 + text_size, 1 + text_size);
    fb_write_ansi(fullscreen, "\x1b[32mletOS\n\x1b[0m");
    fb_set_scale(fullscreen, 2 + text_size, 1 + text_size);
    fullscreen->bg_color = fullscreen->DEFAULT_BG;

    uint32_t toolbar_size = 10;
    fullscreen->x = 20;
    fullscreen->y = 180;
    fullscreen->width  = 50 * 16;
    fullscreen->height -= 40 + fullscreen->y + toolbar_size;
    size_t total_height = fullscreen->height;
    fullscreen->height = 800;
    fb_clear(fullscreen);
    fb_set_scale(fullscreen, 2, 1);
    fb_window_border(fullscreen, "Console", 0x000000, 0);
    fb_set_scale(fullscreen, 3, 2);
    //fullscreen->cursor_y -= 20;

    // Initialize variables
    size_t MAX_VARS = MAX_HASH_VARS;
    char** vars = malloc(MAX_VARS*sizeof(char*));
    if(!vars) {
        fb_write_ansi(fullscreen, "\033[31mERROR\033[0m Cannot initialize lettuce scripting.\n");
        for (;;) __asm__("hlt");
    }
    else
        for (uint32_t i = 0; i < MAX_VARS; i++)
            vars[i] = NULL; 

    // Initialize apps
    MAX_APPLICATIONS = 6;
    apps = malloc(sizeof(Application)*MAX_APPLICATIONS);
    if(!apps) {
        fb_write_ansi(fullscreen, "\033[31mERROR\033[0m Cannot initialize apps.\n");
        for (;;) __asm__("hlt");
    }
    for (uint32_t i = 0; i < MAX_APPLICATIONS; i++) {
        apps[i].MAX_VARS = MAX_VARS;
        apps[i].vars = vars;
        app_init(&apps[i], NULL, NULL);
    }
    apps[0].window = fullscreen;

    // Initialize disk
    uint32_t partition_lba_start = find_fat32_partition();
    if (fat32_init(partition_lba_start))
        fb_write_ansi(fullscreen, "\033[31mERROR\033[0m Cannot mount FAT32 volume.\n");


    // char* ch = malloc(1024*1024*2);
    // char* ch2 = malloc(1024*1024*1);
    // if(ch)
    //     for(int i=0;i<1024*1024;i++)
    //         ch[i] = '\0';


    // Initialize events
    keyboard_init();
    interrupts_init();
    // Event loop

    for (;;) {
        // Adjust layout 
        {
            uint32_t active_count = 0;
            for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
                if (apps[i].run != NULL && apps[i].window && apps[i].window != fullscreen)
                    active_count++;
            }

            if (active_count > 0) {
                uint32_t spacing_x = 40;        // horizontal gap between columns
                uint32_t spacing_y = 55;        // original vertical spacing
                uint32_t col_width = fullscreen->width;  // both columns same width

                uint32_t left_x  = fullscreen->x;
                uint32_t right_x = fullscreen->x + col_width + spacing_x;

                uint32_t top_y       = fullscreen->y;
                uint32_t console_h   = fullscreen->height;
                uint32_t below_y     = top_y + console_h + spacing_y;
                uint32_t below_h     = (total_height > console_h + spacing_y)
                                        ? (total_height - console_h - spacing_y)
                                        : 0;

                uint32_t placed = 0;

                // 1) First app occupies the right side next to the console
                for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
                    if (apps[i].run == NULL || apps[i].window == NULL || apps[i].window == fullscreen)
                        continue;

                    Window* w = apps[i].window;

                    if (placed == 0) {
                        // First app: top-right beside console
                        w->x = right_x;
                        w->y = top_y;
                        w->width  = col_width;
                        w->height = console_h;
                        placed++;
                        continue;
                    }

                    // 2) Remaining apps go below console in 2 columns
                    uint32_t row_index = (placed - 1) / 2;
                    uint32_t col_index = (placed - 1) % 2; // 0 = left, 1 = right

                    uint32_t row_count = (active_count > 1) ? ((active_count - 1 + 1) / 2) : 1;
                    uint32_t available_height = below_h - ((row_count - 1) * spacing_y);
                    uint32_t cell_h = (row_count > 0) ? (available_height / row_count) : 0;

                    w->x = (col_index == 0) ? left_x : right_x;
                    w->y = below_y + row_index * (cell_h + spacing_y);
                    w->width  = col_width;
                    w->height = cell_h;

                    placed++;
                }
            }
        }




        // Run apps
        uint32_t prev_margin = margin;
        margin = 20;
        for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) 
            app_run(&apps[i], i);
        margin = prev_margin;
        apps[0].data[0] = '\0';// always read data
        // console_prompt(fullscreen);
        // console_readline(fullscreen, apps[0].data, APPLICATION_MESSAGE_SIZE);
        Window* target = apps[0].window;//apps[focus_id].window ? apps[focus_id].window : fullscreen;
        console_prompt(target);
        int read_code = console_readline(target, apps[0].data, APPLICATION_MESSAGE_SIZE);
        if(read_code==42) {
            target->cursor_x = target->x+margin;
            continue;
        }
        margin += 40;
        fullscreen->cursor_x = fullscreen->x + margin;
        if (!strcmp(apps[0].data, "clear")) 
            margin -= 40;
        else
            apps[0].window->cursor_y += 10;
        console_execute(&apps[0]);
        if(margin>40) {
            margin -= 40;
            apps[0].window->cursor_x -= 40;
            apps[0].window->cursor_y += 10;
        }
    }

}

__attribute__((noreturn))
void long_mode_start(void) {
    kernel_main();
}

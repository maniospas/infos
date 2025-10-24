#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "console.h"
#include "interrupts.h"
#include "memory/memory.h" 
#include "memory/paging.h" 
#include "memory/dynamic.h" 
#include "application.h"

extern void* multiboot_info_ptr;
int text_size = 1;
Application apps[5];

__attribute__((noreturn))
void kernel_main(void) {
    fb_init(multiboot_info_ptr);
    memory_init(multiboot_info_ptr);
    paging_map_heap();
    memory_buddy_init();

    Window fullscreen = {0};
    init_fullscreen(&fullscreen);
    fullscreen.bg_color = 0;
    fb_clear(&fullscreen);
    fb_set_scale(&fullscreen, 6 + text_size, 1 + text_size);
    fb_write_ansi(&fullscreen, "\x1b[32mLettuOS\n\x1b[0m");
    fb_set_scale(&fullscreen, 2 + text_size, 1 + text_size);
    fullscreen.bg_color = fullscreen.DEFAULT_BG;

    uint32_t toolbar_size = 100;

    static Window windows[5];
    for (int i = 0; i < 5; i++)
        init_fullscreen(&windows[i]);

    // Console window
    windows[0].x = 20;
    windows[0].y = 140;
    windows[0].width  = 60 * 16;
    windows[0].height -= 40 + windows[0].y + toolbar_size;
    fb_clear(&windows[0]);
    fb_set_scale(&windows[0], 2, 1);
    fb_window_border(&windows[0], "Console", 0x000000);
    fb_set_scale(&windows[0], 3, 2);

    // Initialize apps
    int right_x = 60 * 16 + 60;
    int right_y = 140;
    int spacing = 50;
    int widget_height = (windows[0].height - (4 * spacing)) / 4;
    for (int i = 1; i < 5; i++) {
        windows[i].x = right_x;
        windows[i].y = right_y + (i - 1) * (widget_height + spacing);
        windows[i].width  = 50 * 16;
        windows[i].height = widget_height;
    }
    for (int i = 0; i < 5; i++) 
        app_init(&apps[i], NULL, &windows[i], 4096);

    // Initialize events
    keyboard_init();
    interrupts_init();

    // Initialize disk
    uint32_t partition_lba_start = find_fat32_partition();
    if (fat32_init(partition_lba_start))
        fb_write_ansi(&fullscreen, "\033[31mERROR\033[0m Cannot mount FAT32 volume.\n");

    // Event loop
    char buffer[4096];
    for (;;) {
        for (int i = 1; i < 5; i++) {
            app_run(&apps[i]);  
        }
        console_prompt(&windows[0]);
        console_readline(&windows[0], buffer, sizeof(buffer));
        console_execute(&windows[0], buffer);
    }

}

__attribute__((noreturn))
void long_mode_start(void) {
    kernel_main();
}

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
    fullscreen->width  = 60 * 16;
    fullscreen->height -= 40 + fullscreen->y + toolbar_size;
    fb_clear(fullscreen);
    fb_set_scale(fullscreen, 2, 1);
    fb_window_border(fullscreen, "Console", 0x000000, 0);
    fb_set_scale(fullscreen, 3, 2);
    //fullscreen->cursor_y -= 20;

    // Initialize variables
    size_t MAX_VARS = 128; 
    char* vars[MAX_VARS];
    for (uint32_t i = 0; i < MAX_VARS; i++)
        vars[i] = NULL; 

    // Initialize apps
    MAX_APPLICATIONS = 16;
    apps = malloc(sizeof(Application)*MAX_APPLICATIONS);
    for (uint32_t i = 0; i < MAX_APPLICATIONS; i++) {
        apps[i].MAX_VARS = MAX_VARS;
        apps[i].vars = vars;
        app_init(&apps[i], NULL, NULL);
    }
    apps[0].window = fullscreen;

    // Initialize events
    keyboard_init();
    interrupts_init();

    // Initialize disk
    uint32_t partition_lba_start = find_fat32_partition();
    if (fat32_init(partition_lba_start))
        fb_write_ansi(fullscreen, "\033[31mERROR\033[0m Cannot mount FAT32 volume.\n");


    // char* ch = malloc(1024*1024*2);
    // char* ch2 = malloc(1024*1024*1);
    // if(ch)
    //     for(int i=0;i<1024*1024;i++)
    //         ch[i] = '\0';

    // Event loop

    for (;;) {
        // Adjust layout 
        {
            uint32_t active_count = 0;
            for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
                if (apps[i].run != NULL && apps[i].window && apps[i].window != fullscreen)
                    active_count++;
            }
            uint32_t right_x = 60 * 16 + 60;
            uint32_t right_y = 180;
            uint32_t spacing = 55;
            uint32_t total_height = fullscreen->height;  // same region as before
            uint32_t available_height = total_height - (active_count > 1 ? (active_count - 1) * spacing : 0);
            uint32_t widget_height = active_count ? available_height / active_count : 0;
            uint32_t visible_index = 0;
            for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
                if (apps[i].window==NULL)
                    apps[i].window = fullscreen;
                if (apps[i].run == NULL || apps[i].window==fullscreen || apps[i].window==NULL)
                    continue;
                apps[i].window->x = right_x;
                apps[i].window->y = right_y + visible_index * (widget_height + spacing);
                apps[i].window->width  = 50 * 16;
                apps[i].window->height = widget_height;
                visible_index++;
            }
        }

        // Run apps
        for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) 
            app_run(&apps[i], i);
        apps[0].data[0] = '\0';// always read data
        console_prompt(fullscreen);
        margin += 40;
        console_readline(fullscreen, apps[0].data, APPLICATION_MESSAGE_SIZE);
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

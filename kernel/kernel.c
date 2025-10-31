#include "interrupts.h"
#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "user/console.h"
#include "user/application.h"
#include "memory/memory.h" 
#include "smp/smp.h"

extern void* multiboot_info_ptr;
uint32_t text_size = 1;
extern uint32_t margin;
uint32_t focus_id = 0;

// Forward declaration
void update_layout(Window* fullscreen, Application* apps, uint32_t MAX_APPLICATIONS, size_t total_height);

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
    // fb_set_scale(fullscreen, 6 + text_size, 1 + text_size);
    // fb_write_ansi(fullscreen, "\x1b[32mletOS\n\x1b[0m");
    fb_set_scale(fullscreen, 2 + text_size, 1 + text_size);
    fullscreen->bg_color = fullscreen->DEFAULT_BG;

    uint32_t toolbar_size = 10;
    fullscreen->x = 20;
    fullscreen->y = 40;
    fullscreen->width  = 55 * 16;
    fullscreen->height -= 40 + fullscreen->y + toolbar_size;
    size_t total_height = fullscreen->height;
    fullscreen->height = 900;
    fullscreen->bg_color = fullscreen->DEFAULT_BG = 0;
    fb_clear(fullscreen);
    fb_set_scale(fullscreen, 2, 1);
    //fb_window_border(fullscreen, "Console", 0x000000, 0);
    fb_set_scale(fullscreen, 3, 2);

    // Initialize variables
    size_t MAX_VARS = MAX_HASH_VARS;
    char** vars = malloc(MAX_VARS * sizeof(char*));
    if (!vars) {
        fb_write_ansi(fullscreen, "\033[31mERROR\033[0m Cannot initialize lettuce scripting.\n");
        for (;;) __asm__("hlt");
    } else {
        for (uint32_t i = 0; i < MAX_VARS; i++)
            vars[i] = NULL;
    }

    // Initialize apps
    MAX_APPLICATIONS = 11;
    apps = malloc(sizeof(Application) * MAX_APPLICATIONS);
    if (!apps) {
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

    // Initialize events
    keyboard_init();
    interrupts_init();

    // Event loop
    for (;;) {
        update_layout(fullscreen, apps, MAX_APPLICATIONS, total_height);

        // Run apps
        uint32_t prev_focus_id = focus_id;
        uint32_t prev_margin = margin;
        margin = 10;
        for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) 
           app_run(&apps[i], i);
        margin = prev_margin;
        if(!focus_id && prev_focus_id)
            continue;

        apps[0].data[0] = '\0'; // always read data
        Window* target = apps[0].window;
        if (focus_id)
            continue;

        console_prompt(target);
        int read_code = console_readline(target, apps[0].data, APPLICATION_MESSAGE_SIZE);
        margin += 40;
        fullscreen->cursor_x = fullscreen->x + margin;

        if (!strcmp(apps[0].data, "clear")) 
            margin -= 40;
        else
            apps[0].window->cursor_y += 10;

        console_execute(&apps[0]);

        if (margin > 40) {
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

#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "console.h"
#include "interrupts.h"
#include "memory/memory.h" 
#include "memory/paging.h" 
#include "memory/dynamic.h" 

extern void* multiboot_info_ptr;

__attribute__((noreturn))
void kernel_main(void) {
    fb_init(multiboot_info_ptr);

    margin = 10;
    fb_clear();
    fb_set_scale(6, 1);
    fb_write_ansi(
        "\x1b[32m"
        "LettuOS\n"
        "\x1b[0m"
    );
    margin = 20;

    fb_set_scale(2,1);
    fb_write("A healthy operating system.\n");
    memory_init(multiboot_info_ptr);
    paging_map_heap();
    memory_buddy_init();
    // volatile int* x = malloc(1024*1024*255);
    // x[0] = 1;
    // x[1] = 2;
    // fb_write_dec(x[0]);
    // fb_write_dec(x[1]);

    fb_write("\n");

    // === Initialize other subsystems ===
    keyboard_init();
    interrupts_init();

    uint32_t partition_lba_start = find_fat32_partition();
    if (fat32_init(partition_lba_start)) 
        fb_write_ansi("\033[31mERROR\033[0m Cannot mount FAT32 volume.\n");

    // === Main console loop ===
    char buffer[1024];
    for (;;) {
        console_prompt();
        console_readline(buffer, sizeof(buffer));
        console_execute(buffer);
    }
}

__attribute__((noreturn))
void long_mode_start(void) {
    kernel_main();
}

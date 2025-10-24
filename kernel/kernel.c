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
extern uint32_t margin;

__attribute__((noreturn))
void kernel_main(void) {
    fb_init(multiboot_info_ptr);

    margin = 10;
    fb_clear();
    fb_set_scale(4, 1);
    fb_write_ansi(
        "\x1b[33m"
        "infOS\n"
        "\x1b[0m"
    );
    margin = 20;

    fb_set_scale(3,2);
    memory_init(multiboot_info_ptr);
    paging_map_heap();
    memory_buddy_init();
    // volatile int* x = malloc(1024*1024*255);
    // x[0] = 1;
    // x[1] = 2;
    // fb_write_dec(x[0]);
    // fb_write_dec(x[1]);

    uint64_t memory_size = memory_total_with_regions();
    uint64_t memory_cons = memory_used()+(memory_total_with_regions()-memory_total());
    fb_write("\nv0.1 (64bit)\n");
    if(memory_size<1024*1024) {
        fb_write_dec((memory_cons)/1024);
        fb_write("kB / ");
        fb_write_dec((memory_size)/1024);
        fb_write("kB");
    }
    else {
        fb_write_dec((memory_cons)/(1024*1024));
        fb_write("MB / ");
        fb_write_dec((memory_size)/(1024*1024));
        fb_write("MB");
    }
    fb_write("\n");

    // === Initialize other subsystems ===
    keyboard_init();
    interrupts_init();

    uint32_t partition_lba_start = find_fat32_partition();
    if (partition_lba_start == 0) 
        fb_write("Cannot mount FAT32 volume.\n");
    else 
        fat32_init(partition_lba_start);

    // === Main console loop ===
    fb_write("\n");
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

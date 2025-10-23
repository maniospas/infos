#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "console.h"
#include "interrupts.h"
#include "memory/memory.h" 
#include "memory/dynamic.h" 

extern void* multiboot_info_ptr;

__attribute__((noreturn))
void kernel_main(void) {
    fb_init(multiboot_info_ptr);

    fb_clear();
    fb_set_scale(4, 1);
    fb_write_ansi(
        "\x1b[33m"
        "infOS 64-bit\n"
        "\x1b[0m\n"
    );

    fb_set_scale(2, 1);

    memory_init(multiboot_info_ptr);
    memory_buddy_init();

    void* x = malloc(1024*1024*2025);//5MB
    if(x)
        fb_write_hex((uint64_t)x);

    uint64_t memory_size = memory_total();
    uint64_t memory_cons = memory_used();
    fb_write("User memory: ");
    if(memory_size<1024*1024) {
        fb_write_dec((memory_cons)/1024);
        fb_write(" / ");
        fb_write_dec((memory_size)/1024);
        fb_write(" kB");
    }
    else {
        fb_write_dec((memory_cons)/(1024*1024));
        fb_write(" / ");
        fb_write_dec((memory_size)/(1024*1024));
        fb_write(" MB");
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

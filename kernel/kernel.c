#include "screen/screen.h"
#include "screen/vga.h"
#include "keyboard/keyboard.h"
#include "file/fat32.h"
#include "console.h"
#include "interrupts.h"
extern void* multiboot_info_ptr;

__attribute__((noreturn))
void kernel_main(void) {
    fb_init(multiboot_info_ptr);
    fb_clear();

    // === Initialize subsystems ===
    keyboard_init();
    interrupts_init();
    /*fat32_init();*/
    fb_set_scale(2,1);
    fb_write_ansi(
        "\x1b[33m"
        "infOS 64-bit\n"
        "\x1b[0m\n"
    );

    fb_set_scale(3,2);

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

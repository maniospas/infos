// gcc -ffreestanding -fPIC -fno-plt -nostdlib -shared -T disk/shared.ld -o disk/test.so disk/test.c

#include "letos.h"

__attribute__((used))
__attribute__((visibility("default"))) 
void main(Window* window, const char* args) {
    prints("\n\033[32mHello from dynamically loaded ELF!\033[0m\n");
    prints("Echo:");
    prints(args);
    int i = 0;
    while(*(args++)) i++;
    prints("\nInput length:");
    printu(i);
    prints("\n");
}
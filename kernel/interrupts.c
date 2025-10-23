#include "interrupts.h"

#define IDT_SIZE 256
static struct IDTEntry idt[IDT_SIZE];

extern void keyboard_isr(); // from ASM stub

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void idt_set_gate(int n, uint64_t handler) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = 0x08;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;   // Present, DPL=0, interrupt gate
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

void idt_load() {
    struct IDTPointer idtp;
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint64_t)&idt;

    __asm__ volatile("lidt %0" : : "m"(idtp));
}
void interrupts_init(void) {
    // === Set keyboard interrupt gate (IRQ1 = vector 0x21) ===
    idt_set_gate(0x21, (uint64_t)keyboard_isr);
    idt_load();

    // === Remap the PIC ===
    outb(0x20, 0x11); // start initialization (master)
    outb(0xA0, 0x11); // start initialization (slave)
    outb(0x21, 0x20); // master PIC vector offset = 0x20 (IRQs 0–7)
    outb(0xA1, 0x28); // slave PIC vector offset = 0x28 (IRQs 8–15)
    outb(0x21, 0x04); // tell master PIC there is a slave at IRQ2 (0000 0100)
    outb(0xA1, 0x02); // tell slave PIC its cascade identity (0000 0010)
    outb(0x21, 0x01); // 8086/88 mode
    outb(0xA1, 0x01); // 8086/88 mode

    // === Mask all IRQs initially ===
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    // === Unmask only keyboard (IRQ1) ===
    // IRQ1 corresponds to bit 1 (0xFD = 1111 1101)
    outb(0x21, 0xFD);

    // === Enable interrupts globally ===
    __asm__ volatile ("sti");
}

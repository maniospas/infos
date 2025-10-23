#pragma once
#include <stdint.h>

// === IDT Entry Structure (64-bit) ===
struct IDTEntry {
    uint16_t offset_low;     // bits 0–15 of handler address
    uint16_t selector;       // kernel code segment selector
    uint8_t  ist;            // interrupt stack table (0 if unused)
    uint8_t  type_attr;      // type and attributes (e.g. 0x8E)
    uint16_t offset_mid;     // bits 16–31 of handler address
    uint32_t offset_high;    // bits 32–63 of handler address
    uint32_t zero;           // reserved
} __attribute__((packed));

// === IDT Pointer Structure ===
struct IDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// === Interface ===
void idt_set_gate(int n, uint64_t handler);
void idt_load(void);
void interrupts_init(void);

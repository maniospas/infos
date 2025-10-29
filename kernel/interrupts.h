#pragma once
#include <stdint.h>

// === IDT Entry Structure (64-bit) ===
struct IDTEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

// === IDT Pointer Structure ===
struct IDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// === GDT Pointer Structure (used by lgdt) ===
struct GDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

// === Interface ===
void idt_set_gate(int n, uint64_t handler);
void idt_load(void);
void interrupts_init(void);

// GDT setup
void gdt_init(void);

// Exported global descriptor pointer for SMP trampoline
extern uint64_t gdt_descriptor;

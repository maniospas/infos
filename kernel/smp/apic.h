// apic.h
#pragma once
#include <stdint.h>

#define LAPIC_BASE        0xFEE00000
#define LAPIC_ID_REG      0x20
#define LAPIC_VER_REG     0x30
#define LAPIC_TPR_REG     0x80
#define LAPIC_EOI_REG     0xB0
#define LAPIC_SVR_REG     0xF0
#define LAPIC_ICR_LOW     0x300
#define LAPIC_ICR_HIGH    0x310
#define LAPIC_LVT_TIMER   0x320
#define LAPIC_TIMER_INIT  0x380

#define APIC_ENABLE       0x100

static inline void lapic_write(uint32_t reg, uint32_t value) {
    volatile uint32_t *addr = (volatile uint32_t*)(LAPIC_BASE + reg);
    *addr = value;
}

static inline uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t*)(LAPIC_BASE + reg);
}

static inline void lapic_eoi(void) {
    lapic_write(LAPIC_EOI_REG, 0);
}

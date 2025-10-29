// kernel/smp/smp_init.c
#include "apic.h"
#include "smp.h"
#include "../memory/memory.h"
#include "../interrupts.h"
#include "../string.h"

// --- Embed trampoline binary ---
__asm__(
    ".section .rodata\n"
    ".global ap_trampoline\n"
    "ap_trampoline:\n"
    ".incbin \"kernel/smp/ap_trampoline.bin\"\n"
    ".global ap_trampoline_end\n"
    "ap_trampoline_end:\n"
);

extern uint8_t ap_trampoline[];
extern uint8_t ap_trampoline_end[];
extern void ap_main(void);

// From paging/GDT code
extern uint64_t kernel_pml4;
extern uint64_t gdt_descriptor;

// From boot paging tables (defined in boot/boot.s)
extern uint64_t pdpt_table[512];

// === Constants ===
#define TRAMPOLINE_ADDR 0x7000
#define MAX_CPUS 16
#define LAPIC_BASE 0xFEE00000ULL

// Page flags (local)
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_RW       (1ULL << 1)
#define PAGE_PS       (1ULL << 7)

// Offsets in ap_trampoline.asm (must match placeholders)
#define TRAMP_GDT_DESC_OFFSET   0x60
#define TRAMP_PML4_PTR_OFFSET   0x68
#define TRAMP_APMAIN_PTR_OFFSET 0x70

// === Internal state ===
static uint8_t cpu_apic_ids[MAX_CPUS];
static int cpu_count = 1; // BSP = 1

// --- Fake detection until MADT parser is added ---
static void detect_cpus_fake(void) {
    cpu_count = 1;
    for (int i = 0; i < cpu_count; i++)
        cpu_apic_ids[i] = i;
}

// Identity-map LAPIC MMIO (0xFEE00000) so lapic_write() is safe.
static void map_lapic_identity(void) {
    extern uint64_t pdpt_table[512];
    static uint64_t pd_table_apic[512] __attribute__((aligned(4096)));

    // Connect a new PD for LAPIC region if not already used
    pdpt_table[0x1FE] = (uint64_t)pd_table_apic | PAGE_PRESENT | PAGE_RW;
    pd_table_apic[0]  = (LAPIC_BASE & ~(0x1FFFFF)) | PAGE_PRESENT | PAGE_RW | PAGE_PS;
}

// --- Helpers ---
static inline uint32_t get_bsp_apic_id(void) {
    // Avoid MMIO reads; use CPUID leaf 1 EBX[31:24]
    uint32_t ebx;
    asm volatile ("cpuid" : "=b"(ebx) : "a"(1) : "ecx","edx");
    return ebx >> 24;
}

static inline void small_delay(void) {
    for (volatile int i = 0; i < 100000; i++)
        __asm__ volatile("pause");
}

static void lapic_send_ipi(uint8_t apic_id, uint32_t icr_low) {
    // Write destination first (ICR high), then command (ICR low)
    lapic_write(LAPIC_ICR_HIGH, ((uint32_t)apic_id) << 24);
    lapic_write(LAPIC_ICR_LOW, icr_low);
    // Don't poll delivery status (would require a LAPIC read); just wait briefly
    small_delay();
}

// --- Main entry ---
void smp_init(void) {
    detect_cpus_fake();

    // Make LAPIC MMIO accessible for writes
    map_lapic_identity();

    uint32_t bsp_id = get_bsp_apic_id();

    // Copy trampoline binary to low memory
    size_t tramp_size = (size_t)(ap_trampoline_end - ap_trampoline);
    memcpy((void*)TRAMPOLINE_ADDR, ap_trampoline, tramp_size);

    uint8_t* tramp = (uint8_t*)TRAMPOLINE_ADDR;

    // Patch trampoline placeholders
    *(uint64_t*)(tramp + TRAMP_GDT_DESC_OFFSET) = (uint64_t)&gdt_descriptor;
    *(uint64_t*)(tramp + TRAMP_PML4_PTR_OFFSET) = (uint64_t)&kernel_pml4;
    *(uint64_t*)(tramp + TRAMP_APMAIN_PTR_OFFSET) = (uint64_t)&ap_main;

    // INIT + SIPI sequence (classic)
    for (int i = 0; i < cpu_count; i++) {
        uint8_t id = cpu_apic_ids[i];
        if (id == bsp_id) continue;

        // INIT IPI: delivery mode=INIT (101b), level=1, trigger=level
        // Common shorthand value used in OSDev: 0x4500
        lapic_send_ipi(id, 0x4500);
        // 10ms-ish wait; small loop is fine in QEMU
        for (volatile int d=0; d<10; ++d) small_delay();

        // SIPI: delivery mode=Startup (110b) | vector (trampoline >> 12)
        uint8_t vector = (uint8_t)(TRAMPOLINE_ADDR >> 12);
        lapic_send_ipi(id, 0x4600 | vector);
        small_delay();

        // Second SIPI for reliability
        lapic_send_ipi(id, 0x4600 | vector);
        small_delay();
    }
}

uint32_t get_cpu_id(void) {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid"
                 : "=b"(ebx)
                 : "a"(1)
                 : "ecx", "edx");
    return (ebx >> 24);
}



#include "../user/application.h"
volatile int cpu_ready[MAX_CPUS] = {0};
extern uint32_t MAX_APPLICATIONS;
extern Application *apps;

void ap_main(void) {
    uint32_t id = get_cpu_id();
    interrupts_init();

    cpu_ready[id] = 1;

    fb_write_ansi(apps[0].window, "\033[36m[AP] CPU ready.\033[0m\n");

    while (1) {
        for (uint32_t i = 1; i < MAX_APPLICATIONS; i++) {
            if (apps[i].run != NULL) 
                app_run(&apps[i], i);
        }

        // Halt until next timer interrupt or IPI
        __asm__ volatile("hlt");
    }
}

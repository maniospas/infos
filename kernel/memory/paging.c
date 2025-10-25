#include <stdint.h>
#include "../screen/screen.h"
#include "region.h"
#include "paging.h"

extern uint64_t pd_table_heap[];
extern uint8_t* heap_base;
extern uint64_t heap_size;
extern uint64_t heap_used;

#define HEAP_VIRT_BASE 0x40000000ULL  // map heap here

void paging_map_heap(void) {
    if (!heap_base || heap_size == 0)
        return;

    uint64_t phys_start = (uint64_t)heap_base;
    uint64_t phys_end   = phys_start + heap_size;
    phys_start &= ~(PAGE_SIZE_2M - 1);
    phys_end   = (phys_end + PAGE_SIZE_2M - 1) & ~(PAGE_SIZE_2M - 1);

    uint64_t virt_start = HEAP_VIRT_BASE;
    uint64_t virt_end   = virt_start + (phys_end - phys_start);

    for (uint64_t p = phys_start, v = virt_start;
         p < phys_end;
         p += PAGE_SIZE_2M, v += PAGE_SIZE_2M)
    {
        size_t index = (v - HEAP_VIRT_BASE) / PAGE_SIZE_2M;
        pd_table_heap[index] = (p & ~(PAGE_SIZE_2M - 1))
                             | PAGE_PRESENT | PAGE_RW | PAGE_PS;
    }

    heap_base = (uint8_t*)HEAP_VIRT_BASE;   // use virtual address going forward
    heap_size = virt_end - virt_start;
}

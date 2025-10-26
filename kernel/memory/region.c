#include "region.h"
#include <stdint.h>
#include "../multiboot2.h"
#include "../screen/screen.h"

#define MULTIBOOT_MEMORY_AVAILABLE 1

#pragma GCC target("no-mmx,no-sse,no-sse2")

MemoryRegion memory_regions[MAX_MEMORY_REGIONS];
size_t memory_region_count = 0;

/* heap info */
uint8_t* heap_base = NULL;
uint64_t heap_size = 0;
uint64_t heap_used = 0;

extern char _bss_end; // defined in linker.ld

enum {
    MEM_OK = 0,
    MEM_ERR_NULL_PTR,
    MEM_ERR_ALIGN,
    MEM_ERR_TAGSIZE,
    MEM_ERR_NO_REGIONS
};

int memory_init(void *mb_info_ptr) {
    if (!mb_info_ptr)
        return MEM_ERR_NULL_PTR;
    uint8_t *base = (uint8_t *)mb_info_ptr;
    uint32_t total_size = *(uint32_t *)base;
    uint8_t *end = base + total_size;
    for (struct multiboot_tag *tag = (void *)(base + 8);
         (uint8_t *)tag < end && tag->type != MULTIBOOT_TAG_TYPE_END;
         tag = (void *)((uintptr_t)tag + ((tag->size + 7) & ~7))) {
        if ((uintptr_t)tag & 7)
            return MEM_ERR_ALIGN;
        if (tag->size < 8)
            return MEM_ERR_TAGSIZE;
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
            struct multiboot_tag_mmap *mmap_tag = (void *)tag;
            size_t entry_count = (mmap_tag->size - sizeof(*mmap_tag)) / mmap_tag->entry_size;
            for (size_t i = 0; i < entry_count && memory_region_count < MAX_MEMORY_REGIONS; ++i) {
                struct multiboot_mmap_entry *entry =
                    (struct multiboot_mmap_entry *)((uintptr_t)mmap_tag->entries + i * mmap_tag->entry_size);

                if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && entry->len > 0) {
                    memory_regions[memory_region_count].addr = entry->addr;
                    memory_regions[memory_region_count].len  = entry->len;
                    memory_region_count++;
                }
            }
        }
    }
    if (!memory_region_count)
        return MEM_ERR_NO_REGIONS;
    /* ----------------------------------------------------------
     * Automatically set heap region after kernel in usable range
     * ---------------------------------------------------------- */
    heap_used = 0;
    heap_base = 0;
    uint64_t kernel_end = (uint64_t)&_bss_end;
    for (size_t i = 0; i < memory_region_count; ++i) {
        uint64_t region_start = memory_regions[i].addr;
        uint64_t region_end   = memory_regions[i].addr + memory_regions[i].len;
        if (kernel_end >= region_start && kernel_end <= region_end) { 
            // let kernel end exactly at region start or end
            // align heap start up to next 4 KB boundary
            // while ensuring that the aligned start doesn't push us past this region
            uint64_t aligned_heap_start = (kernel_end + 0xFFF) & ~0xFFFULL;
            if (aligned_heap_start < region_end) {
                heap_base = (uint8_t *)aligned_heap_start;
                heap_size = region_end - aligned_heap_start;
            }
            break;
        }
    }
    if (!heap_base)
        return MEM_ERR_NO_REGIONS;
    return MEM_OK;
}

uint64_t memory_total_with_regions(void) {
    uint64_t total = 0;
    for (size_t i = 0; i < memory_region_count; ++i)
        total += memory_regions[i].len;
    return total;
}

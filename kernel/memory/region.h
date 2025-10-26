#pragma once
#include <stddef.h>
#include <stdint.h>
#define MAX_MEMORY_REGIONS 16

typedef struct {
    uint64_t addr;
    uint64_t len;
} MemoryRegion;

int memory_init(void *mb_info_ptr);
uint64_t memory_total_with_regions(void);

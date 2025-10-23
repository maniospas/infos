#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t addr;
    uint64_t len;
} MemoryRegion;

int memory_init(void *mb_info_ptr);
#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIN_ORDER 12  // 4 KiB
#define MAX_ORDER 20  // 1 MiB
#define ORDER_COUNT (MAX_ORDER - MIN_ORDER + 1)

typedef struct Block {
    struct Block* next;
} Block;

void memory_buddy_init();
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t new_size);

uint64_t memory_used(void);
uint64_t memory_total(void);

#ifdef __cplusplus
}
#endif

#endif

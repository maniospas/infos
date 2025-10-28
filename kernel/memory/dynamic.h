#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIN_ORDER 12  // 4 KB (move to lower amounts for embedded maybe)
#define MAX_ORDER 42  // 1TB theoretical max, dynamically limited
#define ORDER_COUNT (MAX_ORDER - MIN_ORDER + 1)

typedef struct Block {
    struct Block* next;
} Block;

/* Core API */
void memory_buddy_init(void);
void* malloc(size_t size);
void free(void* ptr);
void* realloc(void* ptr, size_t new_size);

/* Info helpers */
uint64_t memory_used(void);
uint64_t memory_total(void);

#ifdef __cplusplus
}
#endif

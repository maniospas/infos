#include "dynamic.h"
#include "../screen/screen.h"  // for fb_write / fb_write_hex if desired

// ==== Globals ====
extern uint8_t* heap_base;
extern uint64_t heap_size;
extern uint64_t heap_used;

typedef struct {
    Block* free_list[ORDER_COUNT];
} BuddyAllocator;

static BuddyAllocator buddy;

// Helper macros
#define ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))

// ==== Internal helpers ====
static inline uintptr_t buddy_of(uintptr_t addr, int order) {
    size_t size = 1UL << order;
    return addr ^ size;
}

// ==== Initialization ====
void memory_buddy_init() {
    void* base = heap_base;
    size_t size = heap_size;
    uintptr_t addr = (uintptr_t)base;
    addr = ALIGN_UP(addr, 1UL << MIN_ORDER);
    size &= ~((1UL << MIN_ORDER) - 1);

    heap_base = (uint8_t*)addr;
    heap_size = size;
    heap_used = 0;

    for (int i = 0; i < ORDER_COUNT; i++)
        buddy.free_list[i] = NULL;

    // find max order that fits in heap
    int max_order = MAX_ORDER;
    while ((1UL << max_order) > size && max_order > MIN_ORDER)
        max_order--;

    Block* first = (Block*)heap_base;
    first->next = NULL;
    buddy.free_list[max_order - MIN_ORDER] = first;
}

// ==== Allocation ====
static int order_for_size(size_t size) {
    int order = MIN_ORDER;
    size_t block_size = 1UL << order;
    while (block_size < size && order <= MAX_ORDER) {
        order++;
        block_size <<= 1;
    }
    return order;
}

void* malloc(size_t size) {
    if (size == 0) return NULL;
    int order = order_for_size(size + sizeof(uint64_t)); // small header
    if (order > MAX_ORDER) return NULL;
    int i = order - MIN_ORDER;
    while (i < ORDER_COUNT && buddy.free_list[i] == NULL)
        i++;
    if (i == ORDER_COUNT) return NULL;
    for (; i > order - MIN_ORDER; i--) {
        Block* block = buddy.free_list[i];
        buddy.free_list[i] = block->next;
        uintptr_t addr = (uintptr_t)block;
        size_t half = 1UL << (i + MIN_ORDER - 1);
        Block* b1 = (Block*)addr;
        Block* b2 = (Block*)(addr + half);
        b1->next = b2;
        b2->next = buddy.free_list[i - 1];
        buddy.free_list[i - 1] = b1;
    }
    Block* alloc = buddy.free_list[order - MIN_ORDER];
    buddy.free_list[order - MIN_ORDER] = alloc->next;
    heap_used += (1UL << order);
    uint64_t* header = (uint64_t*)alloc;
    *header = order;
    return (void*)(header + 1);
}

void free(void* ptr) {
    if (!ptr) return;
    uint64_t* header = (uint64_t*)ptr - 1;
    int order = (int)*header;
    uintptr_t addr = (uintptr_t)header;
    heap_used -= (1UL << order);
    while (order <= MAX_ORDER) {
        uintptr_t buddy_addr = buddy_of(addr, order);
        Block** list = &buddy.free_list[order - MIN_ORDER];
        Block* prev = NULL;
        Block* curr = *list;
        while (curr) {
            if ((uintptr_t)curr == buddy_addr) {
                if (prev) prev->next = curr->next;
                else *list = curr->next;
                addr = (addr < buddy_addr) ? addr : buddy_addr;
                order++;
                goto merge_again;
            }
            prev = curr;
            curr = curr->next;
        }

        // no merge
        Block* block = (Block*)addr;
        block->next = *list;
        *list = block;
        return;

    merge_again: ;
    }
}
void* realloc(void* ptr, size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) { free(ptr); return NULL; }
    uint64_t* header = (uint64_t*)ptr - 1;
    int old_order = (int)*header;
    size_t old_size = (1UL << old_order) - sizeof(uint64_t);
    if (new_size <= old_size) return ptr;
    void* new_ptr = malloc(new_size);
    if (!new_ptr) return NULL;
    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (size_t i = 0; i < old_size; i++)
        dst[i] = src[i];
    free(ptr);
    return new_ptr;
}

uint64_t memory_used(void) {
    return heap_used;
}

uint64_t memory_total(void) {
    return heap_size;
}

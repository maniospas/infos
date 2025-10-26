#include "dynamic.h"
#include "../screen/screen.h"  // optional debug prints

// ==== External heap globals (from memory.c) ====
extern uint8_t* heap_base;
extern uint64_t heap_size;
extern uint64_t heap_used;

// ==== Buddy allocator ====
typedef struct {
    Block* free_list[ORDER_COUNT];
    int    max_order;  // dynamically determined
} BuddyAllocator;

static BuddyAllocator buddy;

#define ALIGN_UP(x, a) (((x) + ((a)-1)) & ~((a)-1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
static inline uintptr_t buddy_of(uintptr_t addr, int order) {
    return addr ^ (1UL << order);
}


// ==== Allocation ====
static inline int order_for_size(size_t size) {
    return 64 - __builtin_clzll(size-1);
}

void memory_buddy_init(void) {
    uintptr_t addr = (uintptr_t)heap_base;
    size_t size = heap_size;
    size_t align = (1UL << MIN_ORDER);
    addr = ALIGN_DOWN(addr, align);
    size -= (addr - (uintptr_t)heap_base);
    size &= ~((align - 1));
    heap_base = (uint8_t*)addr;
    heap_size = size;
    heap_used = 0;
    for (int i = 0; i < ORDER_COUNT; i++)
        buddy.free_list[i] = NULL;
    buddy.max_order = order_for_size(heap_size);
    Block* first = (Block*)heap_base;
    first->next = NULL;
    buddy.free_list[buddy.max_order - MIN_ORDER] = first;
}

void* malloc(size_t size) {
    if (size == 0) return NULL;
    int order = order_for_size(size);
    if (order > buddy.max_order) return NULL;
    int i = order - MIN_ORDER;
    while(i < (buddy.max_order - MIN_ORDER + 1) && buddy.free_list[i] == NULL)
        i++;
    if(i >= (buddy.max_order - MIN_ORDER + 1)) 
        return NULL;
    // Split larger blocks until correct size
    for(; i > order - MIN_ORDER; i--) {
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
    // Allocate from the correct order list
    Block* alloc = buddy.free_list[order - MIN_ORDER];
    buddy.free_list[order - MIN_ORDER] = alloc->next;
    heap_used += (1UL << order);
    uint64_t* header = (uint64_t*)alloc;
    *header = order;
    return (void*)(header + 1);
}

// ==== Free ====
void free(void* ptr) {
    if (!ptr) 
        return;
    uint64_t* header = (uint64_t*)ptr - 1;
    int order = (int)*header;
    uintptr_t addr = (uintptr_t)header;
    heap_used -= (1UL << order);
    while(order <= buddy.max_order) {
        uintptr_t buddy_addr = buddy_of(addr, order);
        Block** list = &buddy.free_list[order - MIN_ORDER];
        Block* prev = NULL;
        Block* curr = *list;
        while (curr) {
            if ((uintptr_t)curr == buddy_addr) {
                // Found buddy — remove it and merge
                if (prev) prev->next = curr->next;
                else *list = curr->next;
                addr = (addr < buddy_addr) ? addr : buddy_addr;
                order++;
                goto merge_again;
            }
            prev = curr;
            curr = curr->next;
        }
        // No buddy found — add this block
        Block* block = (Block*)addr;
        block->next = *list;
        *list = block;
        return;
    merge_again: ;
    }
}

// ==== Reallocation ====
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

// ==== Info ====
uint64_t memory_used(void) {
    return heap_used;
}

uint64_t memory_total(void) {
    return heap_size;
}

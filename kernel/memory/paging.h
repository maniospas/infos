#ifndef PAGING_H
#define PAGING_H

#include <stddef.h>
#include <stdint.h>

/*
 * Basic paging flags (x86_64)
 *  ----------------------------
 *  Bit 0  (P)  - Present
 *  Bit 1  (RW) - Read/Write
 *  Bit 2  (US) - User/Supervisor
 *  Bit 7  (PS) - Page Size (set for 2 MiB / 1 GiB pages)
 */
#define PAGE_PRESENT  0x001ULL
#define PAGE_RW       0x002ULL
#define PAGE_USER     0x004ULL
#define PAGE_PWT      0x008ULL
#define PAGE_PCD      0x010ULL
#define PAGE_ACCESSED 0x020ULL
#define PAGE_DIRTY    0x040ULL
#define PAGE_PS       0x080ULL
#define PAGE_GLOBAL   0x100ULL

#define PAGE_SIZE_4K  0x1000ULL
#define PAGE_SIZE_2M  0x200000ULL
#define PAGE_SIZE_1G  0x40000000ULL

#define ENTRIES_PER_TABLE 512

#ifdef __cplusplus
extern "C" {
#endif

void paging_map_heap(void);

#ifdef __cplusplus
}
#endif

#endif // PAGING_H

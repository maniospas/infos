#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include <stddef.h>
#include "../../screen/screen.h"

// =======================================
// ELF-64 header structures (no libc)
// =======================================
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

typedef struct {
    uint32_t st_name;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} Elf64_Sym;

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} Elf64_Rela;

// =======================================
// ELF loader interface
// =======================================

// Loads an ELF module (.so or .elf) into memory
// - elf_image: pointer to raw ELF bytes (already read from disk)
// - out_load_base: receives the allocated memory base (optional)
// Returns: pointer to the module's `main(void*, const char*)` function, or NULL if missing
void* load_elf_module(Window* window, void* elf_image, uint8_t** out_load_base);

// Performs relocation on a loaded ELF
void relocate_elf_with_base(Window* window, void* elf_image, uint8_t* load_base, size_t alloc_size);

#endif

#include "../../screen/screen.h"
#include "../../memory/memory.h"
#include "../../string.h"
#include "elf.h"
#include <stdint.h>
#include <stddef.h>

// ============================
// Symbol table for kernel imports
// ============================
typedef struct {
    const char* name;
    void* addr;
} Symbol;

static Symbol kernel_symbols[] = {
    { "memcpy",  memcpy  },
    { "memset",  memset  },
    { "malloc",  malloc  },
    { "realloc", realloc    },
    { "free",    free    },
    { "strcmp",  strcmp  },
    { "fb_write_ansi", fb_write_ansi },
    { "fb_write_dec", fb_write_dec },
    { NULL, NULL }
};

static void* find_symbol(const char* name) {
    for (int i = 0; kernel_symbols[i].name; i++) {
        if (strcmp(kernel_symbols[i].name, name) == 0)
            return kernel_symbols[i].addr;
    }
    return NULL;
}

// ============================
// ELF constants
// ============================
#define PT_LOAD 1
#define SHT_SYMTAB 2
#define SHT_RELA   4
#define SHT_DYNSYM 11

#define R_X86_64_RELATIVE 8
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_64 1

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((uint32_t)(i))

// ============================
// Manual 4K-aligned allocator
// ============================
static void* alloc_aligned(size_t size, size_t align) {
    size_t total = size + align;
    uint8_t* raw = malloc(total);
    if (!raw) return NULL;
    uintptr_t addr = (uintptr_t)raw;
    uintptr_t aligned = (addr + (align - 1)) & ~(align - 1);
    uint8_t* ptr = (uint8_t*)aligned;
    // Store original pointer just before aligned address
    ((void**)ptr)[-1] = raw;
    return ptr;
}

static void free_aligned(void* ptr) {
    if (!ptr) return;
    void* raw = ((void**)ptr)[-1];
    free(raw);
}

// ============================
// ELF relocation
// ============================
void relocate_elf_with_base(Window* window, void* elf_image, uint8_t* load_base, size_t alloc_size) {
    Elf64_Ehdr* eh = (Elf64_Ehdr*)elf_image;
    Elf64_Shdr* sh = (Elf64_Shdr*)((uint8_t*)elf_image + eh->e_shoff);

    Elf64_Sym* symtab = NULL;
    const char* strtab = NULL;
    uint64_t sym_count = 0;

    //fb_write_ansi(window, "[ELF] Relocation phase started\n");

    // --- Locate symbol and string tables ---
    for (int i = 0; i < eh->e_shnum; i++) {
        if (sh[i].sh_type == SHT_SYMTAB || sh[i].sh_type == SHT_DYNSYM) {
            symtab = (Elf64_Sym*)((uint8_t*)elf_image + sh[i].sh_offset);
            strtab = (const char*)((uint8_t*)elf_image + sh[sh[i].sh_link].sh_offset);
            sym_count = sh[i].sh_size / sizeof(Elf64_Sym);
            break;
        }
    }

    if (!symtab || !strtab) {
        //fb_write_ansi(window, "[ELF] No symbol table found — skipping relocations.\n");
        return;
    }

    // --- Apply relocations ---
    for (int i = 0; i < eh->e_shnum; i++) {
        if (sh[i].sh_type != SHT_RELA)
            continue;

        Elf64_Rela* rela = (Elf64_Rela*)((uint8_t*)elf_image + sh[i].sh_offset);
        size_t count = sh[i].sh_size / sizeof(Elf64_Rela);

        //fb_write_ansi(window, "[ELF] Found RELA section with ");
        //fb_write_dec(window, count);
        //fb_write_ansi(window, " entries\n");

        for (size_t j = 0; j < count; j++) {
            uint32_t type = ELF64_R_TYPE(rela[j].r_info);
            uint32_t sym_idx = ELF64_R_SYM(rela[j].r_info);
            uint8_t* target_addr = load_base + rela[j].r_offset;

            if ((uintptr_t)target_addr < (uintptr_t)load_base ||
                (uintptr_t)target_addr >= (uintptr_t)(load_base + alloc_size)) {
                //fb_write_ansi(window, "[ELF] ERROR: Relocation target out of allocated range!\n");
                continue;
            }

            uint64_t value = 0;
            const char* sym_name = "(none)";

            if (sym_idx < sym_count) {
                Elf64_Sym* sym = &symtab[sym_idx];
                if (sym->st_name)
                    sym_name = &strtab[sym->st_name];

                void* addr = find_symbol(sym_name);
                if (addr)
                    value = (uint64_t)(uintptr_t)addr;
                else
                    value = (uint64_t)(uintptr_t)(load_base + sym->st_value);

                // fb_write_ansi(window, "[ELF] Linking ");
                // fb_write_ansi(window, sym_name);
                // fb_write_ansi(window, " @ ");
                // fb_write_hex(window, (uint64_t)target_addr);
                // fb_write_ansi(window, " -> ");
                // fb_write_hex(window, value);
                // fb_write_ansi(window, "\n");
            }

            uint64_t* target = (uint64_t*)target_addr;
            switch (type) {
                case R_X86_64_RELATIVE:
                    *target = (uint64_t)(load_base + rela[j].r_addend);
                    break;
                case R_X86_64_64:
                case R_X86_64_GLOB_DAT:
                case R_X86_64_JUMP_SLOT:
                    *target = value + rela[j].r_addend;
                    break;
                default:
                    // fb_write_ansi(window, "[ELF] WARN: Unknown relocation type ");
                    // fb_write_dec(window, type);
                    // fb_write_ansi(window, "\n");
                    break;
            }
        }
    }

    //fb_write_ansi(window, "[ELF] Relocation phase completed.\n");
}

// ============================
// ELF loader
// ============================
void* load_elf_module(Window* window, void* elf_image, uint8_t** out_load_base) {
    Elf64_Ehdr* eh = (Elf64_Ehdr*)elf_image;
    Elf64_Phdr* ph = (Elf64_Phdr*)((uint8_t*)elf_image + eh->e_phoff);

    //fb_write_ansi(window, "[ELF] Loading ELF file...\n");

    uint64_t base_addr = UINT64_MAX;
    uint64_t max_addr  = 0;

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_LOAD) {
            if (ph[i].p_vaddr < base_addr) base_addr = ph[i].p_vaddr;
            if (ph[i].p_vaddr + ph[i].p_memsz > max_addr)
                max_addr = ph[i].p_vaddr + ph[i].p_memsz;
        }
    }

    size_t alloc_size = (max_addr - base_addr + 0x1000) & ~0xFFF;
    uint8_t* load_base = alloc_aligned(alloc_size, 0x1000);
    if (!load_base) {
        //fb_write_ansi(window, "[ELF] ERROR: Out of memory.\n");
        return NULL;
    }
    memset(load_base, 0, alloc_size);

    // fb_write_ansi(window, "[ELF] Allocated ");
    // fb_write_dec(window, alloc_size);
    // fb_write_ansi(window, " bytes (aligned 4K)\n");

    // Copy LOAD segments
    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD)
            continue;

        uint8_t* src  = (uint8_t*)elf_image + ph[i].p_offset;
        uint64_t vaddr_offset = ph[i].p_vaddr - base_addr;
        uint8_t* dest = load_base + vaddr_offset;

        memcpy(dest, src, ph[i].p_filesz);
        if (ph[i].p_memsz > ph[i].p_filesz)
            memset(dest + ph[i].p_filesz, 0, ph[i].p_memsz - ph[i].p_filesz);
    }

    //fb_write_ansi(window, "[ELF] Segments copied.\n");

    // Relocate
    relocate_elf_with_base(window, elf_image, load_base, alloc_size);

    // Find "main" symbol
    void* main_entry = NULL;
    Elf64_Shdr* sh = (Elf64_Shdr*)((uint8_t*)elf_image + eh->e_shoff);
    for (int i = 0; i < eh->e_shnum; i++) {
        if (sh[i].sh_type != SHT_SYMTAB && sh[i].sh_type != SHT_DYNSYM)
            continue;

        Elf64_Sym* symtab = (Elf64_Sym*)((uint8_t*)elf_image + sh[i].sh_offset);
        const char* strtab = (const char*)((uint8_t*)elf_image + sh[sh[i].sh_link].sh_offset);
        size_t sym_count = sh[i].sh_size / sizeof(Elf64_Sym);

        for (size_t j = 0; j < sym_count; j++) {
            if (!symtab[j].st_name)
                continue;

            const char* sym_name = &strtab[symtab[j].st_name];
            if (strcmp(sym_name, "main") == 0) {
                main_entry = (void*)(uintptr_t)(load_base + symtab[j].st_value);
                // fb_write_ansi(window, "[ELF] Found main @ ");
                // fb_write_hex(window, (uint64_t)main_entry);
                // fb_write_ansi(window, "\n");
                break;
            }
        }
        if (main_entry)
            break;
    }

    //if (!main_entry)
    //    fb_write_ansi(window, "[ELF] WARNING: No 'main' symbol found.\n");

    if (out_load_base)
        *out_load_base = load_base;

    return main_entry;
}

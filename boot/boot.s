; === boot/boot.s ===
[BITS 32]

; ---------------------------------------------------------------------------
; Multiboot2 Header
; ---------------------------------------------------------------------------
section .multiboot_header
align 8

MB2_MAGIC      equ 0xe85250d6
MB2_ARCH       equ 0
MB2_LENGTH     equ header_end - header_start
MB2_CHECKSUM   equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

header_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LENGTH
    dd MB2_CHECKSUM

; Framebuffer request tag (type 5)
align 8
framebuffer_tag_start:
    dw 5
    dw 0
    dd framebuffer_tag_end - framebuffer_tag_start
    dd 1920
    dd 1080
    dd 32
framebuffer_tag_end:

; End tag (type = 0)
align 8
    dw 0
    dw 0
    dd 8

header_end:

; ---------------------------------------------------------------------------
section .bss
align 16
global safe_mb_copy
safe_mb_copy:
    resb 8192            ; 8 KiB buffer for Multiboot info

stack_bottom:
    resb 8192
stack_top:

; ---------------------------------------------------------------------------
section .text
global _start
extern long_mode_start
global multiboot_info_ptr

_start:
    cli
    mov [multiboot_info_ptr], ebx   ; EBX = Multiboot2 info pointer
    xor eax, eax
    mov [multiboot_info_ptr+4], eax
    mov esp, stack_top

    ; === Copy Multiboot info to a safe low buffer ===
    mov esi, ebx             ; source = physical addr
    mov ecx, [esi]           ; total_size field
    cmp ecx, 16384
    jbe .copy_ok
    mov ecx, 16384           ; clamp to buffer size
.copy_ok:
    lea edi, [safe_mb_copy]
    rep movsb

    ; Store full pointer (64-bit)
    mov eax, safe_mb_copy
    mov dword [multiboot_info_ptr], eax
    xor eax, eax
    mov dword [multiboot_info_ptr + 4], eax

    ; === Load GDT ===
    lgdt [gdt_descriptor]

    ; === Enable PAE ===
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; === Load PML4 ===
    mov eax, pml4_table
    mov cr3, eax

    ; === Enable Long Mode ===
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; === Enable Paging ===
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; === Jump to 64-bit mode ===
    jmp 0x08:long_mode_entry

; ---------------------------------------------------------------------------
section .data
align 8
global multiboot_info_ptr
multiboot_info_ptr:
    dq 0

; ---------------------------------------------------------------------------
[BITS 64]
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    extern long_mode_start
    call long_mode_start

.hang:
    hlt
    jmp .hang

; ---------------------------------------------------------------------------
align 8
gdt64:
    dq 0x0000000000000000
    dq 0x00af9a000000ffff
    dq 0x00af92000000ffff

gdt_descriptor:
    dw gdt64_end - gdt64 - 1
    dd gdt64
gdt64_end:

; ---------------------------------------------------------------------------
section .data
align 4096
global pml4_table
global pdpt_table
global pd_table
global pd_table_heap

; PML4 → PDPT
pml4_table:
    dq pdpt_table + 0x03        ; Present | RW
align 4096

; PDPT → PDs
pdpt_table:
    dq pd_table + 0x03          ; First PD: low memory (kernel)
    dq pd_table_heap + 0x03     ; Second PD: heap region
    times 510 dq 0              ; rest unused
align 4096

; PD #0 → first 4 MiB of memory
pd_table:
    %assign i 0
    %rep 2
        dq (i * 0x200000) | 0x83
        %assign i i + 1
    %endrep
    times (512 - 2) dq 0       ; fill rest with zeros
align 4096

; PD #1 → heap region (empty, mapped dynamically)
pd_table_heap:
    times 512 dq 0

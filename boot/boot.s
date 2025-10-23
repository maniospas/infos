; === boot/boot.s ===
[BITS 32]

section .multiboot_header
align 8

; === Multiboot2 Header Constants ===
MB2_MAGIC      equ 0xe85250d6
MB2_ARCH       equ 0
MB2_LENGTH     equ header_end - header_start
MB2_CHECKSUM   equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

; === Header Start ===
header_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LENGTH
    dd MB2_CHECKSUM

; ---------------------------------------------------------------------------
; Framebuffer request tag (type 5)
; See: https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Framebuffer-request
; ---------------------------------------------------------------------------
align 8
framebuffer_tag_start:
    dw 5                      ; type = framebuffer request
    dw 0                      ; flags = required
    dd framebuffer_tag_end - framebuffer_tag_start
    dd 1920                   ; preferred width
    dd 1080                   ; preferred height
    dd 32                     ; preferred bpp
framebuffer_tag_end:

; ---------------------------------------------------------------------------
; End tag (type = 0)
; ---------------------------------------------------------------------------
align 8
    dw 0
    dw 0
    dd 8

header_end:

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
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

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
global pml4_table
global pdpt_table
global pd_table
align 4096
pml4_table:
    dq pdpt_table + 0x03
align 4096
pdpt_table:
    dq pd_table + 0x03
align 4096
pd_table:
    dq 0x0000000000000000 + 0x83

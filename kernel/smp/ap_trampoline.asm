; kernel/smp/ap_trampoline.asm
; Builds as flat binary for low-memory AP startup (no external references)
; Assembled with: nasm -f bin ap_trampoline.asm -o ap_trampoline.bin

[BITS 16]
[ORG 0x7000]

global ap_trampoline
global ap_trampoline_end

ap_trampoline:

cli
xor ax, ax
mov ds, ax
mov ss, ax
mov sp, 0x7000

; --- Load GDT ---
; The real address of the GDT descriptor will be patched by the kernel.
; We place a placeholder here (6 bytes: 2-byte limit, 4-byte base)
lgdt [gdt_descriptor]

; Enter protected mode
mov eax, cr0
or eax, 1
mov cr0, eax
jmp 0x08:protected_mode

[BITS 32]
protected_mode:
mov ax, 0x10
mov ds, ax
mov es, ax
mov fs, ax
mov gs, ax
mov ss, ax

; Enable PAE
mov eax, cr4
or eax, (1 << 5)
mov cr4, eax

; Load PML4 (filled by kernel)
mov eax, [pml4_pointer]
mov cr3, eax

; Enable long mode
mov ecx, 0xC0000080
rdmsr
or eax, (1 << 8)
wrmsr

; Enable paging
mov eax, cr0
or eax, (1 << 31)
mov cr0, eax

jmp 0x08:long_mode

[BITS 64]
long_mode:
; The kernel will patch this address with ap_main().
mov rax, [ap_main_pointer]
mov rsp, 0x80000
call rax
hlt

; === Data placeholders to be patched later ===
align 8
gdt_descriptor:   dq 0
pml4_pointer:     dq 0
ap_main_pointer:  dq 0

ap_trampoline_end:

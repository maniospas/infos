[BITS 64]
global keyboard_isr
extern keyboard_handler_c

keyboard_isr:
    ; === Standard interrupt prologue ===
    cli
    push rbp
    mov rbp, rsp
    sub rsp, 8                ; align stack to 16 bytes (SysV ABI requires it)

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; === Read scancode from PS/2 data port ===
    in al, 0x60
    movzx rdi, al             ; put scancode in RDI (1st argument)
    call keyboard_handler_c   ; call C handler

    ; === Send End-of-Interrupt to PIC ===
    mov al, 0x20
    out 0x20, al

    ; === Restore registers ===
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    mov rsp, rbp
    pop rbp
    sti
    iretq

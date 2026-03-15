MBOOT_MAGIC    equ 0x1BADB002
MBOOT_FLAGS    equ (1<<0)|(1<<1)
MBOOT_CHECKSUM equ -(MBOOT_MAGIC + MBOOT_FLAGS)

section .multiboot
align 4
    dd MBOOT_MAGIC
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

section .bss
align 16
stack_bottom:
    resb 65536
stack_top:

section .text
global _start
global isr_timer
global isr_keyboard
global isr_noerr
global isr_err
global isr_irq
extern kernel_main
extern timer_handler
extern keyboard_handler

_start:
    cli
    mov esp, stack_top
    push ebx
    push eax
    call kernel_main
    cli
.hang: hlt
    jmp .hang

isr_noerr: pusha; popa; iret
isr_err:   add esp, 4; iret
isr_irq:   pusha; mov al,0x20; out 0xA0,al; out 0x20,al; popa; iret

isr_timer:
    pusha
    call timer_handler
    popa
    iret

isr_keyboard:
    pusha
    call keyboard_handler
    popa
    iret

/* linker.ld - Kernel linker script */

ENTRY(_start)

SECTIONS {
    . = 1M;                     /* Load at 1 MB (standard for kernels) */

    .text BLOCK(4K) : ALIGN(4K) {
        *(.multiboot)           /* Multiboot header must come first */
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K) {
        *(COMMON)
        *(.bss)
    }
}

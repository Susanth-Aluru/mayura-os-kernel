# SimpleKernel v0.1

A minimal x86 bootable kernel. Boots via GRUB (Multiboot spec),
sets up a stack, and prints coloured text using VGA text mode.

---

## Files

```
kernel/
├── boot.asm     - Multiboot header + ASM entry point
├── kernel.c     - Main kernel (VGA output, boot info)
├── linker.ld    - Linker script (loads at 1 MB)
├── Makefile     - Build, ISO, and QEMU targets
└── grub.cfg     - GRUB menu config (for ISO)
```

---

## Requirements

| Tool | Purpose |
|------|---------|
| `i686-elf-gcc` | Cross-compiler (32-bit ELF) |
| `nasm` | Assembler |
| `qemu-system-i386` | Emulator for testing |
| `grub-mkrescue` + `xorriso` | ISO creation (optional) |

### Installing the cross-compiler (Ubuntu/Debian)

The easiest way is to use a pre-built toolchain:

```bash
# Option A: OSDev pre-built binaries
# Download from: https://github.com/lordmilko/i686-elf-tools/releases

# Option B: Build it yourself (takes ~30 min)
# Follow: https://wiki.osdev.org/GCC_Cross-Compiler

# QEMU + GRUB tools
sudo apt install qemu-system-x86 grub-pc-bin xorriso
```

---

## Build & Run

```bash
# Build the kernel ELF
make

# Run directly in QEMU (fastest)
make run

# Build a bootable ISO then run
make iso
make run-iso

# Clean build artifacts
make clean
```

---

## What it does

- Sets up a 16 KB stack in BSS
- Passes Multiboot magic + info pointer to `kernel_main`
- Initialises VGA text mode (80×25, writes to 0xB8000)
- Prints coloured status messages and a welcome banner
- Detects if booted by GRUB
- Halts with `hlt`

---

## What to add next

- **GDT** – Set up your own Global Descriptor Table
- **IDT + ISRs** – Handle CPU exceptions and hardware interrupts
- **Keyboard driver** – Read from IRQ1 / PS/2 controller
- **Physical memory manager** – Parse the Multiboot memory map
- **Paging** – Enable virtual memory (CR0/CR3)
- **Shell** – Simple command loop over keyboard input

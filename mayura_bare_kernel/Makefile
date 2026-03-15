# Makefile for SimpleKernel

TARGET   = kernel.elf
ISO      = simplekernel.iso
CC       = gcc
AS       = nasm
LD       = gcc

CFLAGS   = -m32 -ffreestanding -O2 -Wall -Wextra -std=gnu99 \
           -fno-stack-protector -fno-builtin -nostdlib
ASFLAGS  = -f elf32
LDFLAGS  = -m32 -ffreestanding -nostdlib -lgcc -T linker.ld

OBJS     = boot.o kernel.o

# ── Build ────────────────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

boot.o: boot.asm
	$(AS) $(ASFLAGS) -o $@ $<

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Create bootable ISO with GRUB ───────────────────────────
iso: $(TARGET)
	mkdir -p iso/boot/grub
	cp $(TARGET) iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o $(ISO) iso
	@echo "ISO ready: $(ISO)"

# ── Run in QEMU ─────────────────────────────────────────────
run: $(TARGET)
	qemu-system-i386 -kernel $(TARGET)

run-iso: iso
	qemu-system-i386 -cdrom $(ISO)

# ── Clean ────────────────────────────────────────────────────
clean:
	rm -f *.o $(TARGET) $(ISO)
	rm -rf iso/

.PHONY: all iso run run-iso clean

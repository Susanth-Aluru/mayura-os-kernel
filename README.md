# 🦚 Mayura OS

> **⚠️ Vibe Coded:** This project was architected and designed by me, coded by AI. It works, but the internals may not follow textbook OS design patterns. Read the source with that in mind.

A bare-metal x86 operating system built from scratch in pure C and Assembly. No Linux. No libc. No external dependencies. Boots directly on real hardware.

---

## What is this?

Mayura OS is a hobby OS that boots via GRUB, runs in 32-bit protected mode, and includes everything you'd expect from a usable system:

- Unix-like shell with 100+ commands
- RAM filesystem with full directory tree
- **SimpleC** — a built-in scripting language with 4 standard libraries
- Music player (7 genres including Indian Classical) via PC speaker
- AI chatbot (pattern matching, no network)
- GUI desktop with tab-based window manager
- 5 games: Snake, Tetris, Pong, 2048, Minesweeper
- Animated boot logo + boot mode chooser (GUI or CLI)
- In-OS documentation browser (`docs` command)
- Screensaver, world clock, system dashboard, and more

The entire OS is a single `kernel.c` file (~3000 lines) + a small `boot.asm` stub.

---

## Builds

| Build | File | Description |
|-------|------|-------------|
| Full OS | `kernel.c` | Everything — GUI, CLI, SimpleC, games, music |
| CLI only | `bare_cli_kernel.c` | No GUI. Same shell and all apps, just terminal |
| Bare kernel | `bare_kernel.c` | 243 lines. VGA + keyboard + timer. Build your own. |

---

## Getting Started

### Requirements

```bash
sudo apt install gcc-multilib nasm qemu-system-x86 grub-pc-bin xorriso
```

### Build and run

```bash
make          # build kernel.elf
make run      # run in QEMU
make iso      # build bootable ISO
make run-iso  # run ISO in QEMU
make clean
```

### With audio (music player)

```bash
qemu-system-i386 -kernel kernel.elf -soundhw pcspk
# or newer QEMU:
qemu-system-i386 -kernel kernel.elf -device pcspk
```

---

## Running on VirtualBox

> ⚠️ **Important:** Set Graphics Controller to **VBoxVGA**. Using VMSVGA causes a black screen.

| Setting | Value |
|---------|-------|
| Guest OS Type | Other/Unknown (32-bit) |
| Graphics Controller | **VBoxVGA** (not VMSVGA) |
| Chipset | PIIX3 |
| EFI | Disabled |
| Audio | PulseAudio + ICH AC97 |

Enable PC speaker for music:

```bash
VBoxManage setextradata "YourVMName" \
  "VBoxInternal/Devices/pcspk/0/Config/Enabled" 1
```

---

## Running on Real Hardware

```bash
# Check your USB device first
lsblk

# Flash the ISO (replace sdX with your USB drive)
sudo dd if=mayura-os.iso of=/dev/sdX bs=4M status=progress
sync
```

Reboot, set BIOS to boot from USB. If using UEFI, enable CSM / Legacy Boot.

---

## SimpleC

Type `sc` in the shell to open the REPL. Type `run file.sc` to execute a script.

```c
// FizzBuzz
for (int i = 1; i <= 20; i++) {
    if (mod(i, 15) == 0)     { println("FizzBuzz"); }
    else if (mod(i, 3) == 0) { println("Fizz"); }
    else if (mod(i, 5) == 0) { println("Buzz"); }
    else                     { println(i); }
}

// Play a note
beep(440, 300);

// Write to filesystem
writefile("out.txt", "hello from SimpleC");
```

**4 standard libraries:** Math (`abs min max pow sqrt mod rand`), String (`len upper lower concat substr strcmp contains`), File I/O (`readfile writefile appendfile`), System (`sleep beep gettime getticks exit`)

---

## File Structure

```
boot.asm        Multiboot header, 64KB stack, ISR stubs
kernel.c        Entire OS (~3000 lines)
linker.ld       Loads kernel at 1MB
Makefile        Build system
grub.cfg        GRUB menu config
bare_cli_kernel.c   CLI-only build (no GUI)
bare_kernel.c       Minimal 243-line foundation
```

---

## Default Filesystem

Files live in RAM and are lost on reboot.

```
/
├── home/susanth/
│   ├── readme.txt
│   ├── hello.sc        SimpleC example
│   └── .bashrc
├── etc/
│   ├── hostname
│   ├── motd
│   └── os-release
├── tmp/
├── usr/share/fortune/quotes
├── var/log/kern.log
└── bin/README
```

---

## Internals

| Component | Details |
|-----------|---------|
| Architecture | i686 x86-32 |
| Entry point | `0x00100000` (1 MB) |
| Stack | 64 KB (BSS in boot.asm) |
| VGA | Text mode 80×25 @ `0xB8000` |
| Timer | PIT at 100 Hz (each tick = 10 ms) |
| Keyboard | PS/2 scancode set 1, 256-entry ring buffer |
| PC Speaker | PIT channel 2, ports `0x42/0x43/0x61` |
| RTC | Ports `0x70/0x71`, BCD-decoded, IST (UTC+5:30) |
| IDT | 256 entries: CPU exceptions + IRQs + timer + keyboard |

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Black screen in VirtualBox | Set Graphics to **VBoxVGA**, disable EFI |
| No sound / music silent | Enable PC speaker via VBoxManage (see above) |
| Triple fault on boot | `make clean && make iso`, ensure VBoxVGA not VMSVGA |
| Prompt glitching | Run `cd ~` to reset path |
| Won't boot on real hardware | Enable CSM / Legacy Boot in UEFI |

---

## Easter Eggs

There are a few hidden commands not listed in `help`. Type `om` for one of them.

---

## Disclaimer

> This OS was **architected and designed by Susanth**, and **coded by AI** (Claude by Anthropic).  
> It is a learning/hobby project. The code works, boots on real hardware, and is functional — but it is not a production OS and was not written by hand line by line.  
> If you're studying OS development, treat this as a reference for concepts and structure, not necessarily best practices.

---

## License

Do whatever you want with it. Attribution appreciated but not required.

---

*Created by Susanth*

# NovaOS

> Next-Generation Operating System for Future Computers

![NovaOS](https://img.shields.io/badge/version-0.1.0-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20C%20%7C%20ASM-green)
![Platform](https://img.shields.io/badge/platform-x86-orange)
![Status](https://img.shields.io/badge/status-active-brightgreen)

NovaOS is a custom operating system built entirely from scratch in C++, C, and x86 Assembly. macOS-level polish meets gaming-first GPU architecture — built for the future.

## Features

- Custom x86 bootloader (Assembly)
- 32-bit protected mode kernel (C/C++)
- GDT + IDT (CPU descriptor tables)
- Real PIT timer @ 1000Hz (IRQ0)
- PS/2 keyboard driver with Shift, Caps Lock, symbols (IRQ1)
- Physical memory manager (bitmap allocator)
- Virtual memory manager
- RAM filesystem (NovaFS) — create, read, write, delete files
- Cooperative process scheduler — spawn and run processes
- Nova Shell — interactive command shell with 17 commands
- Kernel panic handler — styled crash screen
- Splash screen with animated loading bar
- Colored VGA text output

## Shell Commands

| Command | Description |
|---------|-------------|
| `help` | Show all commands |
| `version` | OS version |
| `about` | About NovaOS |
| `clear` | Clear screen |
| `cpu` | CPU info |
| `mem` | Memory info |
| `uptime` | Live uptime (h/m/s) |
| `echo <text>` | Print text |
| `ls` | List files |
| `cat <file>` | Read file |
| `touch <file>` | Create file |
| `write <file> <text>` | Write to file |
| `rm <file>` | Delete file |
| `ps` | List processes |
| `run <program>` | Run: hello, counter, sysinfo |
| `panic` | Trigger kernel panic (test) |
| `color` | Color test |

## Tech Stack

| Layer | Language |
|-------|----------|
| Bootloader | x86 ASM |
| Kernel core | C + C++ |
| Drivers | C + C++ |
| Shell | C++ |
| Build system | GNU Make |

## Project Structure
## Build & Run

### Requirements
- `i686-elf-gcc` / `i686-elf-g++` cross-compiler
- `nasm` assembler
- `grub-mkrescue` + `xorriso`
- `qemu-system-i386`

### Commands
```bash
make          # Build kernel + ISO
make run      # Boot in QEMU
make clean    # Clean build output
```

## Roadmap

- [ ] x86-64 migration
- [ ] Nova Compositor (Wayland)
- [ ] NVIDIA GPU driver integration
- [ ] Persistent filesystem (NovaFS on disk)
- [ ] Preemptive multitasking
- [ ] Nova SDK for app development
- [ ] Nova Store

## Author

Built by **Alyssa Baker** ([@A-Bak-tech](https://github.com/A-Bak-tech))

---

*NovaOS — Built from scratch. Built for the future.*

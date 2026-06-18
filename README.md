# NovaOS

> The Operating System for Everyone — Productivity, Gaming, and Everything In Between

![NovaOS](https://img.shields.io/badge/version-0.1.0-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20C%20%7C%20ASM-green)
![Platform](https://img.shields.io/badge/platform-x86-orange)
![License](https://img.shields.io/badge/license-MIT-brightgreen)
![Status](https://img.shields.io/badge/status-active-brightgreen)

NovaOS is a next-generation operating system built entirely from scratch in C++, C, and x86 Assembly. It combines the polish and simplicity of macOS, the hardware freedom and gaming power of Windows, and the productivity of a modern office OS — all in one.

**NovaOS is for everyone:**
- 👩‍💼 Office workers who want a clean, fast, distraction-free OS
- 🎮 Gamers who need NVIDIA GPU support and high performance
- 👩‍💻 Developers who want a powerful, open platform
- 🎬 Content creators who need GPU-accelerated tools
- 🌍 Everyday users who just want something that works

---

## Why NovaOS?

| | Windows | macOS | NovaOS |
|---|---|---|---|
| Open hardware | ✅ | ❌ | ✅ |
| Gaming support | ✅ | ❌ | ✅ |
| Clean UI | ❌ | ✅ | ✅ |
| Office productivity | ✅ | ✅ | ✅ |
| NVIDIA GPU | ✅ | ❌ | ✅ |
| Privacy-first | ❌ | ⚠️ | ✅ |
| Open source | ❌ | ❌ | ✅ |
| Free forever | ❌ | ⚠️ | ✅ |

---

## Current Features (v0.1.0)

- ⚙️ Custom x86 bootloader (Assembly)
- 🧠 32-bit protected mode kernel (C/C++)
- 🔧 GDT + IDT (CPU descriptor tables)
- ⏱️ Real PIT timer @ 1000Hz (IRQ0)
- ⌨️ PS/2 keyboard — Shift, Caps Lock, symbols (IRQ1)
- 🧮 Physical memory manager (bitmap allocator)
- 📁 NovaFS — RAM filesystem (create, read, write, delete)
- ⚡ Cooperative process scheduler
- 💻 Nova Shell — 17 interactive commands
- 💥 Kernel panic handler — styled crash screen
- 🎨 Splash screen with animated loading bar
- 🌈 Colored VGA text output

---

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

---

## Tech Stack

| Layer | Language |
|-------|----------|
| Bootloader | x86 ASM |
| Kernel core | C + C++ |
| Drivers | C + C++ |
| Shell | C++ |
| Build system | GNU Make |

---

## Project Structure
---

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

---

## Roadmap

### Phase 1 — Foundation (Now)
- [x] Bootloader + kernel
- [x] Memory manager
- [x] Keyboard + timer drivers
- [x] Nova Shell
- [x] RAM filesystem
- [x] Process scheduler
- [x] Kernel panic handler

### Phase 2 — Core OS
- [ ] x86-64 migration
- [ ] Real persistent filesystem (NovaFS on disk)
- [ ] Preemptive multitasking
- [ ] Virtual memory (paging)
- [ ] Syscall interface

### Phase 3 — Desktop
- [ ] Nova Compositor (Wayland)
- [ ] GUI window manager
- [ ] Nova Browser
- [ ] Office suite integration (docs, spreadsheets)
- [ ] Nova Files (file manager)
- [ ] Nova Terminal (GPU-accelerated)

### Phase 4 — Gaming & Creative
- [ ] NVIDIA GPU driver (open kernel module)
- [ ] Vulkan render pipeline
- [ ] Game Mode (performance profiles)
- [ ] DLSS / FSR support
- [ ] Nova Screen (streaming/recording)
- [ ] OBS integration

### Phase 5 — Ecosystem
- [ ] Nova SDK for app developers
- [ ] Nova Store (app marketplace)
- [ ] Steam + Heroic launcher integration
- [ ] Hardware certification program
- [ ] novaos.space website

---

## Contributing

NovaOS is open source and welcomes contributors! Whether you want to work on the kernel, drivers, shell, or documentation — all help is appreciated.

---

## Author

Built by **Alyssa Baker** under [Calyxar](https://github.com/Calyxar)

- GitHub: [@A-Bak-tech](https://github.com/A-Bak-tech)
- Org: [@Calyxar](https://github.com/Calyxar)

---

*NovaOS — Built from scratch. Built for everyone. Built for the future.*

# letOS

![lettuce logo](disk/logo.bmp)

LetOS (pronounced *lettuce*) is a lightweight operating system featuring a keyboard-driven windowed interface and persistent processes. It uses minimal resources, though it uses GRUB as a bootloader that requires 6MB of RAM and consumes 12 MB of disk space.

## 💿 Running the ISO

Run *letOS.iso* by one of these options:
- unpacking the letos.iso image into a bootable USB device (VirtualBox or VMware can be alternatives to this)
- running virtual environment like QEMU per the following command

```
qemu-system-x86_64 -cdrom "letOS.iso" -boot d -m 512M -vga virtio -display sdl,gl=on -full-screen \
    -drive file="fat32.img",format=raw,media=disk 
```

Use *-accel hvf* instead of *-enable-kvm* in windows.


## 🖥️ Interface and Environment

letOS boots directly into a minimalist windowed environment.
Every interaction is keyboard-driven — windows can be opened, moved, and switched without a mouse.
The OS supports persistent processes, so programs can remain active even when you switch between windows or close their interface.

At the core of the environment is the integrated Lettuce Console, which acts as both a shell and a programming environment.
From this console, you can start processes, send commands, and write or execute Lettuce scripts directly.

## 🥬 The Lettuce Language

Lettuce is a small, interpreted programming language built directly into letOS.
It enables interactive programming and system control from within the console.
Programs can manipulate data, draw to the screen, and interact with running processes.

Lettuce serves as both the user shell and the scripting layer for automating system behavior.

## 🪛 Toolchain

letOS is built from scratch in Assembly and C, booting through GRUB2 into 64-bit long mode and initializing a framebuffer-based graphical environment with multitasking and interactive scripting capabilities.

To build the system from source, you need a cross-compilation toolchain targeting x86_64-elf and a few utilities for assembling and packaging the image. All setup instructions are provided in bootstrapping.txt, including:

- Installing nasm, xorriso, and grub-mkrescue
- Building or installing x86_64-elf-gcc
- Setting up qemu-system-x86_64 for testing

After completing these steps, you can build and run letOS directly on any Linux distribution.

## ⚙️ Project Structure

- boot/ — 32-bit bootstrap code and transition to long mode
- kernel/ — core 64-bit kernel and window manager
- lettuce/ — the Lettuce programming language runtime and interpreter
- drivers/ — framebuffer, keyboard, and process management subsystems
- Makefile — build and run automation
- bootstrapping.txt — toolchain setup instructions

**Notes:**
- *malloc* or *realloc* are expected to return NULL if they try to allocate more than half (or even lower numbers) 
of remainder memory. This is due to the buddy memory management system.
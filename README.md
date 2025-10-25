# 🥬 LettuOS

LettuOS is a lightweight operating system featuring a keyboard-driven windowed interface, persistent processes, and an integrated programming language called Lettuce.

## 💿 Running the ISO

If you prefer to run LettuOS manually or in another virtualization environment, first download the image *LettuOS.iso* from this repository. Or you can mount it directly in Linux with:

```
sudo mount -o loop build/LettuOS.iso /mnt
```

or run it in a virtual environment such as QEMU per the following command. The ISO can also be booted inside VirtualBox or VMware by attaching it as a virtual optical drive.

```
qemu-system-x86_64 -cdrom build/LettuOS.iso -vga virtio -display sdl,gl=on
```

## 🖥️ Interface and Environment

LettuOS boots directly into a minimalist windowed environment.
Every interaction is keyboard-driven — windows can be opened, moved, and switched without a mouse.
The OS supports persistent processes, so programs can remain active even when you switch between windows or close their interface.

At the core of the environment is the integrated Lettuce Console, which acts as both a shell and a programming environment.
From this console, you can start processes, send commands, and write or execute Lettuce scripts directly.

## 🥬 The Lettuce Language

Lettuce is a small, interpreted programming language built directly into LettuOS.
It enables interactive programming and system control from within the console.
Programs can manipulate data, draw to the screen, and interact with running processes.

Lettuce serves as both the user shell and the scripting layer for automating system behavior.

## 🪛 Toolchain

LettuOS is built from scratch in Assembly and C, booting through GRUB2 into 64-bit long mode and initializing a framebuffer-based graphical environment with multitasking and interactive scripting capabilities.

To build the system from source, you need a cross-compilation toolchain targeting x86_64-elf and a few utilities for assembling and packaging the image. All setup instructions are provided in bootstrapping.txt, including:

- Installing nasm, xorriso, and grub-mkrescue
- Building or installing x86_64-elf-gcc
- Setting up qemu-system-x86_64 for testing

After completing these steps, you can build and run LettuOS directly on any Linux distribution.

## ⚙️ Project Structure

- boot/ — 32-bit bootstrap code and transition to long mode
- kernel/ — core 64-bit kernel and window manager
- lettuce/ — the Lettuce programming language runtime and interpreter
- drivers/ — framebuffer, keyboard, and process management subsystems
- Makefile — build and run automation
- bootstrapping.txt — toolchain setup instructions
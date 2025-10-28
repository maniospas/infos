# letOS

![lettuce logo](disk/logo.bmp)

LetOS (pronounced *lettuce*) is a lightweight operating system featuring a keyboard-driven windowed interface and persistent processes. It uses minimal resources, though it uses GRUB as a bootloader that requires 6MB of RAM and consumes 12 MB of disk space.

**License:** Apache 2.0 - see [LICENSE.txt](LICENSE.txt)<br>
**Maintainer:** Emmanouil Krasanakis (maniospas@hotmail.com)

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

## 🥬 The lettuce language

Lettuce is a small, interpreted programming language built directly into letOS. In a sense, the operating system *is* the language and conversely.

It enables interactive programming and system control from within the console. Programs can manipulate data, draw to the screen, and interact with running processes.

Type *help* and enter in the main terminal to see the language's basic constructs and usage rules. Mainly, command take the form of a name followed by space-separated arguments like so: `let a 1` to set the text *"1"* to a variable *a*. Commands output values, so enclose them in parentheses to make such evaluation. For example, `print (a)` evaluates *a* and transforms this to `print 1`, eventually printing the value to the console. You can use the `|` operator as basically an open parenthesis that ends at end of line. Brackets are used to prevent one evaluation.

Below is an example, where *app* is used to create an application handle and *args* listens to the input stream of the application to re-run it based on inputs. App and file commands create a handle string that can be used. The second command opens a file and sends the file handle to the application handle.

```rust
let myapp|app {image|args}
to (myapp) (file logo.bmp)
```


## 🪛 Toolchain

letOS is built from scratch in Assembly and C, booting through GRUB2 into 64-bit long mode and initializing a framebuffer-based graphical environment with multitasking and interactive scripting capabilities.

To build the system from source, you need a cross-compilation toolchain targeting x86_64-elf and a few utilities for assembling and packaging the image. All setup instructions are provided in bootstrapping.txt, including:

- Installing nasm, xorriso, and grub-mkrescue
- Building or installing x86_64-elf-gcc
- Setting up qemu-system-x86_64 for testing

After completing these steps, you can build and run letOS directly on any Linux distribution. The project is structured as follows:

- boot/ — 32-bit bootstrap code and transition to long mode
- kernel/ — core 64-bit kernel and window manager
- lettuce/ — the Lettuce programming language runtime and interpreter
- drivers/ — framebuffer, keyboard, and process management subsystems
- Makefile — build and run automation
- bootstrapping.txt — toolchain setup instructions

⚠️ **Development note:** While developing, note that *malloc* or *realloc* are expected to return NULL if they try to allocate more than half (or even lower numbers) of remainder memory. This is due to the buddy memory management system. Furthermore, allocations may consume a whole power-of-2 block. In general, do NOT check available memory beforehand but only the outcome of allocators. Furthermore, prefer preallocating buffers.

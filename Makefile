# === Toolchain ===
PREFIX := $(HOME)/opt/cross/bin/x86_64-elf-
CC := $(PREFIX)gcc
AS := nasm
LD := $(PREFIX)ld

KERNEL := kernel.elf
ISO := infos.iso

# === Compiler & Linker Flags ===
CFLAGS64 := -ffreestanding -O2 -Wall -Wextra -fno-pic -m64 -fno-exceptions -fno-asynchronous-unwind-tables
CFLAGS32 := -ffreestanding -O2 -Wall -Wextra -fno-pic -m32 -fno-exceptions -fno-asynchronous-unwind-tables
LDFLAGS  := -T linker.ld -nostdlib -z max-page-size=0x1000 -z noexecstack -m elf_x86_64

# === Auto-detect all source files ===
BOOT_SRC := $(wildcard boot/*.s)
KERNEL_CSRC := $(wildcard kernel/**/*.c kernel/*.c)
KERNEL_SSRC := $(wildcard kernel/**/*.s kernel/*.s)
OBJ := $(BOOT_SRC:.s=.o) $(KERNEL_CSRC:.c=.o) $(KERNEL_SSRC:.s=.o)

# === Build Rules ===
all: $(KERNEL)

# Boot (always 32-bit)
boot/%.o: boot/%.s
	@echo "  AS32    $<"
	$(AS) -w+zext-reloc=0 -f elf32 $< -o $@

# Kernel assembly (always 64-bit)
kernel/%.o: kernel/%.s
	@echo "  AS64    $<"
	$(AS) -f elf64 $< -o $@

# Kernel C sources (64-bit)
kernel/%.o: kernel/%.c
	@echo "  CC64    $<"
	$(CC) $(CFLAGS64) -c $< -o $@

# Link kernel
$(KERNEL): $(OBJ)
	@echo "  LD      $@"
	$(LD) $(LDFLAGS) --no-warn-mismatch -o $@ $^

# === ISO Creation ===
$(ISO): $(KERNEL) grub.cfg
	@echo "  MKISO   $@"
	mkdir -p iso/boot/grub
	cp $(KERNEL) iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o $(ISO) iso

# === Run Targets ===
run: $(ISO)
	@echo "  QEMU (BIOS)"
	qemu-system-x86_64 -cdrom $(ISO) -boot d -m 512M -vga virtio -display sdl

# === Clean ===
clean:
	@echo "  CLEAN"
	rm -rf $(OBJ) $(KERNEL) iso *.iso

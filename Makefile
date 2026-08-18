CC := gcc
LD := ld
QEMU := qemu-system-i386

BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/kernel.elf
ISO := $(BUILD_DIR)/tinyshell.iso
QEMU_LOG := $(BUILD_DIR)/qemu.log

CFLAGS := -m32 -std=c11 -ffreestanding -fno-pie -fno-stack-protector \
	-fno-asynchronous-unwind-tables -Wall -Wextra -Werror -O2 -g -Iinclude
ASFLAGS := -m32 -ffreestanding -fno-pie -c
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

C_SOURCES := $(shell find kernel -type f -name '*.c' | sort)
ASM_SOURCES := $(shell find boot -type f -name '*.S' | sort)
OBJECTS := \
	$(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES)) \
	$(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SOURCES))

.PHONY: all check clean run debug test

all: check $(ISO)

check:
	@bash tools/check-env.sh

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -Map $(BUILD_DIR)/kernel.map -o $@ $(OBJECTS)
	grub-file --is-x86-multiboot $@

$(ISO): $(KERNEL) config/grub.cfg
	@mkdir -p $(ISO_ROOT)/boot/grub
	cp $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
	cp config/grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_ROOT) >/dev/null 2>&1

run: check $(ISO)
	$(QEMU) -cdrom $(ISO) -m 64M -display none -serial mon:stdio -no-reboot

debug: check $(ISO)
	$(QEMU) -cdrom $(ISO) -m 64M -display none -serial mon:stdio -no-reboot -S -s

test: check $(ISO)
	@rm -f $(QEMU_LOG)
	@timeout 5s $(QEMU) -cdrom $(ISO) -m 64M -display none \
		-serial file:$(QEMU_LOG) -monitor none -no-reboot -no-shutdown \
		>/dev/null 2>&1 || test $$? -eq 124
	@grep -q "TinyShell OS booting" $(QEMU_LOG)
	@grep -q "BOOT_OK" $(QEMU_LOG)
	@echo "QEMU boot test: PASS"

clean:
	rm -rf $(BUILD_DIR)

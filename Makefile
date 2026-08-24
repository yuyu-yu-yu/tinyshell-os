CC := gcc
LD := ld
QEMU := qemu-system-i386
QEMU_MEMORY ?= 64M
QEMU_MEMORY_MATRIX ?= 16M 64M 128M

BUILD_DIR := build
ISO_ROOT := $(BUILD_DIR)/iso
KERNEL := $(BUILD_DIR)/kernel.elf
ISO := $(BUILD_DIR)/tinyshell.iso

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
	$(QEMU) -cdrom $(ISO) -m $(QEMU_MEMORY) -display none -serial mon:stdio -no-reboot

debug: check $(ISO)
	$(QEMU) -cdrom $(ISO) -m $(QEMU_MEMORY) -display none -serial mon:stdio -no-reboot -S -s

test: check $(ISO)
	@set -eu; \
	markers='CONSOLE_OK GDT_OK IDT_OK MULTIBOOT_OK MEMORY_MAP_OK INT3_TEST_OK PMM_OK PMM_ALLOC_FREE_OK PIC_OK IRQ_OK PIT_OK TIMER_IRQ_OK KEYBOARD_DECODE_OK KEYBOARD_READY BOOT_OK'; \
	for memory in $(QEMU_MEMORY_MATRIX); do \
		log="$(BUILD_DIR)/qemu-$$memory.log"; \
		rm -f "$$log"; \
		status=0; \
		timeout 5s $(QEMU) -cdrom $(ISO) -m "$$memory" -display none \
			-serial "file:$$log" -monitor none -no-reboot -no-shutdown \
			>/dev/null 2>&1 || status=$$?; \
		test "$$status" -eq 0 -o "$$status" -eq 124; \
		grep -q 'TinyShell OS booting' "$$log"; \
		grep -q 'EXCEPTION vector=3' "$$log"; \
		for marker in $$markers; do grep -q "$$marker" "$$log"; done; \
		! grep -q 'BOOT_FAIL:' "$$log"; \
		test "$$(grep -c '^PMM_FREE_PAGES=[0-9][0-9]*' "$$log")" -eq 1; \
		echo "QEMU $$memory boot test: PASS"; \
	done; \
	p16=$$(awk -F= '/^PMM_FREE_PAGES=/{gsub(/\r/, "", $$2); print $$2}' $(BUILD_DIR)/qemu-16M.log); \
	p64=$$(awk -F= '/^PMM_FREE_PAGES=/{gsub(/\r/, "", $$2); print $$2}' $(BUILD_DIR)/qemu-64M.log); \
	p128=$$(awk -F= '/^PMM_FREE_PAGES=/{gsub(/\r/, "", $$2); print $$2}' $(BUILD_DIR)/qemu-128M.log); \
	test "$$p16" -lt "$$p64"; \
	test "$$p64" -lt "$$p128"; \
	echo "PMM memory scaling: $$p16 < $$p64 < $$p128"; \
	echo "QEMU boot matrix: PASS"

clean:
	rm -rf $(BUILD_DIR)

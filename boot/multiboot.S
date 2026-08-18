.set MULTIBOOT_ALIGN,    1 << 0
.set MULTIBOOT_MEMINFO,  1 << 1
.set MULTIBOOT_FLAGS,    MULTIBOOT_ALIGN | MULTIBOOT_MEMINFO
.set MULTIBOOT_MAGIC,    0x1BADB002
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.section .multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
.extern kernel_main

_start:
    mov $stack_top, %esp
    xor %ebp, %ebp

    push %ebx
    push %eax
    call kernel_main

1:
    cli
    hlt
    jmp 1b

.size _start, . - _start

.section .note.GNU-stack,"",@progbits

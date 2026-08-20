.set GDT_KERNEL_CODE_SELECTOR, 0x08
.set GDT_KERNEL_DATA_SELECTOR, 0x10

.section .text
.global gdt_load
.type gdt_load, @function

gdt_load:
    mov 4(%esp), %eax
    lgdt (%eax)

    mov $GDT_KERNEL_DATA_SELECTOR, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    ljmp $GDT_KERNEL_CODE_SELECTOR, $.Lgdt_loaded

.Lgdt_loaded:
    ret

.size gdt_load, . - gdt_load

.section .note.GNU-stack,"",@progbits

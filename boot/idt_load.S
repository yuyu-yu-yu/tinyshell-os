.section .text
.global idt_load
.type idt_load, @function

idt_load:
    movl 4(%esp), %eax
    lidt (%eax)
    ret

.size idt_load, . - idt_load

.section .note.GNU-stack,"",@progbits

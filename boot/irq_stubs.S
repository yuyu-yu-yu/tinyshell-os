.section .text

.macro IRQ_STUB number
.global irq\number
.type irq\number, @function
irq\number:
    pushl $0
    pushl $(32 + \number)
    jmp interrupt_common_entry
.size irq\number, . - irq\number
.endm

IRQ_STUB 0
IRQ_STUB 1
IRQ_STUB 2
IRQ_STUB 3
IRQ_STUB 4
IRQ_STUB 5
IRQ_STUB 6
IRQ_STUB 7
IRQ_STUB 8
IRQ_STUB 9
IRQ_STUB 10
IRQ_STUB 11
IRQ_STUB 12
IRQ_STUB 13
IRQ_STUB 14
IRQ_STUB 15

.section .rodata
.align 4
.global irq_stub_table
irq_stub_table:
    .long irq0
    .long irq1
    .long irq2
    .long irq3
    .long irq4
    .long irq5
    .long irq6
    .long irq7
    .long irq8
    .long irq9
    .long irq10
    .long irq11
    .long irq12
    .long irq13
    .long irq14
    .long irq15

.section .note.GNU-stack,"",@progbits

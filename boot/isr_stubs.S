.section .text

.extern interrupt_dispatch

.macro ISR_NO_ERROR number
.global isr\number
.type isr\number, @function
isr\number:
    pushl $0
    pushl $\number
    jmp interrupt_common_entry
.size isr\number, . - isr\number
.endm

.macro ISR_WITH_ERROR number
.global isr\number
.type isr\number, @function
isr\number:
    pushl $\number
    jmp interrupt_common_entry
.size isr\number, . - isr\number
.endm

ISR_NO_ERROR 0
ISR_NO_ERROR 1
ISR_NO_ERROR 2
ISR_NO_ERROR 3
ISR_NO_ERROR 4
ISR_NO_ERROR 5
ISR_NO_ERROR 6
ISR_NO_ERROR 7
ISR_WITH_ERROR 8
ISR_NO_ERROR 9
ISR_WITH_ERROR 10
ISR_WITH_ERROR 11
ISR_WITH_ERROR 12
ISR_WITH_ERROR 13
ISR_WITH_ERROR 14
ISR_NO_ERROR 15
ISR_NO_ERROR 16
ISR_WITH_ERROR 17
ISR_NO_ERROR 18
ISR_NO_ERROR 19
ISR_NO_ERROR 20
ISR_WITH_ERROR 21
ISR_NO_ERROR 22
ISR_NO_ERROR 23
ISR_NO_ERROR 24
ISR_NO_ERROR 25
ISR_NO_ERROR 26
ISR_NO_ERROR 27
ISR_NO_ERROR 28
ISR_WITH_ERROR 29
ISR_WITH_ERROR 30
ISR_NO_ERROR 31

.section .text
.global interrupt_common_entry
.type interrupt_common_entry, @function

interrupt_common_entry:
    pushal
    movl %esp, %eax
    pushl %eax
    call interrupt_dispatch
    addl $4, %esp
    popal
    addl $8, %esp
    iret

.size interrupt_common_entry, . - interrupt_common_entry

.section .rodata
.align 4
.global isr_stub_table
isr_stub_table:
    .long isr0
    .long isr1
    .long isr2
    .long isr3
    .long isr4
    .long isr5
    .long isr6
    .long isr7
    .long isr8
    .long isr9
    .long isr10
    .long isr11
    .long isr12
    .long isr13
    .long isr14
    .long isr15
    .long isr16
    .long isr17
    .long isr18
    .long isr19
    .long isr20
    .long isr21
    .long isr22
    .long isr23
    .long isr24
    .long isr25
    .long isr26
    .long isr27
    .long isr28
    .long isr29
    .long isr30
    .long isr31

.section .note.GNU-stack,"",@progbits

.section .text

.global task_context_switch
.type task_context_switch, @function

/*
 * void task_context_switch(uintptr_t *old_sp, uintptr_t new_sp,
 *                          uint32_t old_eflags);
 *
 * The C scheduler captures EFLAGS before disabling interrupts and passes that
 * value here. This frame therefore preserves the caller's original IF state,
 * while every task-table mutation and the actual stack swap run with IF=0.
 */
task_context_switch:
    movl 12(%esp), %eax
    pushl %eax
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl 24(%esp), %eax
    movl %esp, (%eax)
    movl 28(%esp), %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    popfl
    ret

.size task_context_switch, . - task_context_switch

.section .note.GNU-stack,"",@progbits

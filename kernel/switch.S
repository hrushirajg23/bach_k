.global context_switch

/* Thread struct offsets (must match struct thread_struct in task.h) */
.equ THREAD_ESP0,  0
.equ THREAD_ESP,   4
.equ THREAD_SS0,   8
.equ THREAD_EIP,  12
.equ THREAD_FS,   16
.equ THREAD_GS,   20
.equ THREAD_IOPL, 24
.equ THREAD_FPU,  28

/*
 * Stack layout of switch_to (cdecl):
 *                 HIGH ADDRESS
 * +----------------------------+
 * | (&last)                    | 16(%ebp)
 * +----------------------------+
 * | (next)                     | 12(%ebp)
 * +----------------------------+
 * | (prev)                     | 8(%ebp)
 * +----------------------------+
 * | return address             | 4(%ebp)
 * +----------------------------+
 * | old EBP                    | 0(%ebp)
 * +----------------------------+
 *                 LOW ADDRESS
 */
.global switch_to
switch_to:
    pushl %ebp
    movl  %esp, %ebp

    movl  8(%ebp), %ecx      # ecx = prev
    movl 12(%ebp), %edx      # edx = next
    movl 16(%ebp), %ebx      # ebx = &last

    /* Save callee-saved registers per cdecl */
    pushfl
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    /* Save current stack pointer into prev->thread.esp */
    movl %esp, THREAD_ESP(%ecx)

    /* Save resume address: when prev is rescheduled it continues at 1f */
    movl $1f, THREAD_EIP(%ecx)

    /* Load next's stack */
    movl THREAD_ESP(%edx), %esp

    /* Push next->thread.eip as a fake return address, then jmp __switch_to.
     * __switch_to does TSS housekeeping and rets into next's eip. */
    pushl THREAD_EIP(%edx)

    jmp __switch_to

1:
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp   /* reverse of pushl %ebp: was pushed 2nd, pop 4th */
    popfl       /* reverse of pushfl:    was pushed 1st, pop 5th (last) */

    /* Store returned prev into *last */
    movl 16(%ebp), %ebx
    movl %eax, (%ebx)

    /* Standard function epilogue:
     * leave = movl %ebp,%esp (esp←frame base) + popl %ebp (pop saved old_ebp)
     * This skips the old_ebp saved by the prologue so ret pops the
     * correct return address to context_switch. */
    leave
    ret

.global __switch_to
__switch_to:

    /* ecx = prev, edx = next (preserved by cdecl across the jmp) */

    /* Update TSS.esp0 so that on the next ring-3->ring-0 transition
     * the CPU uses next's kernel stack top.
     * cpu_tss is a struct (not a pointer), so we use its address. */
    leal cpu_tss, %eax
    movl THREAD_ESP0(%edx), %ebx
    movl %ebx, 4(%eax)          # cpu_tss.esp0 = next->thread.esp0

    /* Return prev in eax — it will be stored into *last by the caller */
    movl %ecx, %eax

    ret

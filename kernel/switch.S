.global context_switch

THREAD_ESP0 0
THREAD_ESP  4
THREAD_SS0  8
THREAD_EIP  12
THREAD_FS   16
THREAD_GS   20
THREAD_IOPL 24
THREAD_FPU  28
    
                ; HIGH ADDRESS
; +----------------------------+
; | (&last)                    | 16(%ebp)
; +----------------------------+
; | (next)                     | 12(%ebp)
; +----------------------------+
; | (prev)                     | 8(%ebp)
; +----------------------------+
; | return address             | 4(%ebp)
; +----------------------------+
; | old EBP                    | 0(%ebp)
; +----------------------------+
                ; LOW ADDRESS
.global switch_to
switch_to:
    pushl %ebp
    movl  %esp, %ebp

    movl  8(%ebp), %ecx      # ecx = prev
    movl 12(%ebp), %edx      # edx = next
    movl 16(%ebp), %ebx      # ebx = &last

    # Save callee-saved registers acc to cdecl 
    pushfl
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    # Save current stack pointer into prev->thread.esp
    movl %esp, THREAD_ESP(%ecx)

    # Save resume address into prev->thread.eip
    # So any process that was preempted earlier and is now scheduled 
    # will start executing from $1f 
    # llok at the explanation in docs
    
    movl $1f, THREAD_EIP(%ecx)

    # Load next's stack
    movl THREAD_ESP(%edx), %esp

    # Push next->thread.eip as return address
    # fake  a call instruction, magic :)
    pushl THREAD_EIP(%edx)

    jmp __switch_to

1:
    popl %edi
    popl %esi
    popl %ebx
    popfl


     # Store returned prev into *last

    movl 16(%ebp), %ebx # ebx = &last
    movl %eax, (%ebx)   # *(&last) = prev

    movl %ebp, %esp
    popl %ebp
    ret

.global __switch_to
__switch_to:

    # ecx is prev
    # edx is  next

    # Update TSS.esp0 = current's esp0
    movl cpu_tss, %eax
    movl THREAD_ESP0(%edx), 4(%eax)

    # Return prev in eax, that will be vallue to be filled in last , 
    # every process should be aware from which process 
    # was it preempted
    movl %ecx, %eax

    ret
    
    

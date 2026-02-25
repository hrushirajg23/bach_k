.global context_switch

context_switch:
    movl 4(%esp), %eax  # eax = prev
    movl 8(%esp), %edx  # edx = next

    # save callee-saved registers
    pushfl
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl %esp, (%eax)   # save current esp into prev esp
    movl (%edx), %esp   # load next's esp
    

    # restore callee-saved registers
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    ret

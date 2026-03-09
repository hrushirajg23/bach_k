.code32

# Low-level exception handlers (Linux 0.11 style)


/* ================= OFFSETS ================= */

.globl divide_error, debug, nmi, int3, overflow, bounds, invalid_op
.globl device_not_available, double_fault, coprocessor_segment_overrun
.globl invalid_TSS, segment_not_present, stack_segment
.globl general_protection, coprocessor_error, reserved
.globl timer_intr

.extern do_divide_error
.extern do_int3
.extern do_nmi
.extern do_overflow
.extern do_bounds
.extern do_invalid_op
.extern do_device_not_available
.extern do_coprocessor_segment_overrun
.extern do_reserved
.extern do_coprocessor_error
.extern do_double_fault
.extern do_invalid_TSS
.extern do_segment_not_present
.extern do_stack_segment
.extern do_general_protection

.extern current
.extern last_task_used_math
.extern math_state_restore
.extern jiffies
.extern operate_timer
.extern send_EOI

/* ================= NO ERROR CODE ================= */

divide_error:
    pushl $do_divide_error

no_error_code:
    xchgl %eax, (%esp)
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    pushl %ds
    pushl %es
    pushl %fs
    pushl $0
    leal 44(%esp), %edx
    pushl %edx
    movw $0x10, %dx
    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    call *%eax
    addl $8, %esp
    popl %fs
    popl %es
    popl %ds
    popl %ebp
    popl %esi
    popl %edi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    iret

debug:      pushl $do_int3;      jmp no_error_code
nmi:        pushl $do_nmi;       jmp no_error_code
int3:       pushl $do_int3;      jmp no_error_code
overflow:   pushl $do_overflow;  jmp no_error_code
bounds:     pushl $do_bounds;    jmp no_error_code
invalid_op: pushl $do_invalid_op;jmp no_error_code

/* ================= DEVICE NOT AVAILABLE ================= */

math_emulate:
    popl %eax
    pushl $do_device_not_available
    jmp no_error_code

device_not_available:
    pushl %eax
    movl %cr0, %eax
    bt $2, %eax
    jc math_emulate

    clts

    movl current, %eax
    cmpl last_task_used_math, %eax
    je 1f

    pushl %ecx
    pushl %edx
    pushl %ds

    movw $0x10, %ax
    movw %ax, %ds

    call math_state_restore

    popl %ds
    popl %edx
    popl %ecx
1:
    popl %eax
    iret

coprocessor_segment_overrun:
    pushl $do_coprocessor_segment_overrun
    jmp no_error_code

reserved:
    pushl $do_reserved
    jmp no_error_code

coprocessor_error:
    pushl $do_coprocessor_error
    jmp no_error_code

/* ================= WITH ERROR CODE ================= */

double_fault:
    pushl $do_double_fault

error_code:
    xchgl %eax, 4(%esp)
    xchgl %ebx, (%esp)
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    pushl %ds
    pushl %es
    pushl %fs
    pushl %eax
    leal 44(%esp), %eax
    pushl %eax
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    call *%ebx
    addl $8, %esp
    popl %fs
    popl %es
    popl %ds
    popl %ebp
    popl %esi
    popl %edi
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    iret

invalid_TSS:        pushl $do_invalid_TSS;        jmp error_code
segment_not_present:pushl $do_segment_not_present;jmp error_code
stack_segment:      pushl $do_stack_segment;      jmp error_code
general_protection: pushl $do_general_protection; jmp error_code


timer_intr:
    pushal
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    incl jiffies

    pushl $1
    call send_EOI
    addl $4, %esp

    movl 52(%esp), %eax
    andl $3, %eax
    pushl %eax
    call operate_timer
    addl $4, %esp

    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal
    iret

/*
SAVE_ALL saves all the CPU registers that may be used by the interrupt handler on the
stack, except for eflags, cs, eip, ss, and esp, which are already saved automatically by
the control unit */
SAVE_ALL:
	cld
	push %gs
	push %fs
	push %es
	push %ds
	pushl %eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %edx
	pushl %ecx
	pushl %ebx
	movl $0x10, %edx  /* kernel data segment (KERNEL_DATA = 0x10) */
	movl %edx, %ds
	movl %edx, %es

RESTORE_ALL: 
	add $4, %esp        # skip orig_eax
	popl %ebx
	popl %ecx
	popl %edx
	popl %esi
	popl %edi
	popl %ebp
	popl %eax
	pop %ds
	pop %es
	pop %fs
	pop %gs

irq_return:
	iret //return to previous mode

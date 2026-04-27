.code32

/* pt_regs offsets (must match struct pt_regs) */

.equ PT_EBX,     0x00
.equ PT_ECX,     0x04
.equ PT_EDX,     0x08
.equ PT_ESI,     0x0C
.equ PT_EDI,     0x10
.equ PT_EBP,     0x14
.equ PT_EAX,     0x18
.equ PT_DS,      0x1C
.equ PT_ES,      0x20
.equ PT_FS,      0x24
.equ PT_GS,      0x28
.equ PT_ORIG_EAX,0x2C
.equ PT_EIP,     0x30
.equ PT_CS,      0x34
.equ PT_EFLAGS,  0x38
.equ PT_OLDESP,  0x3C
.equ PT_OLDSS,   0x40


/* task_struct signal field offsets */

.equ SIGHANDLE_OFF, 20
.equ RESTORER_OFF,  24
.equ SIG_FN_OFF,    28

.globl ret_from_sys_call
.globl handle_sig
.globl ret_from_fork
.globl system_call

.extern current
.extern process_table
.extern psig
.extern find_empty_process
.extern sys_call_table

/* -----------------------------------------------------------------------
 * SAVE_ALL  - push all pt_regs fields (except orig_eax, eip, cs, eflags,
 *             oldesp, oldss which are pushed by caller / CPU).
 *             After this macro, %esp points at the base of a full pt_regs.
 * ----------------------------------------------------------------------- */
.macro SAVE_ALL
    cld
    pushl %gs
    pushl %fs
    pushl %es
    pushl %ds
    pushl %eax
    pushl %ebp
    pushl %edi
    pushl %esi
    pushl %edx
    pushl %ecx
    pushl %ebx
    movl $0x10, %edx        /* KERNEL_DATA selector */
    movl %edx, %ds
    movl %edx, %es
.endm

/* -----------------------------------------------------------------------
 * RESTORE_ALL - reverse of SAVE_ALL, then iret.
 * Skips orig_eax (not of any use while returing[), then pops regs and returns.
 * ----------------------------------------------------------------------- */
.macro RESTORE_ALL
    popl %ebx
    popl %ecx
    popl %edx
    popl %esi
    popl %edi
    popl %ebp
    popl %eax
    popl %ds
    popl %es
    popl %fs
    popl %gs
    addl $4, %esp           /* skip orig_eax */
    iret
.endm

.macro GET_THREAD_INFO reg
    movl %esp, \reg
    andl $0xFFFFE000, \reg  /* STACK_SIZE = 8192 = 2^13 */
.endm

/* check if you are returning back to kernel mode
	. iF you're returning to user mode, then only call psig
	remember the state diagram, signals are checked and handled while
	returning to user mode only
	We do this by comparing the current priviliege level with previous stack segment
	pushed
	if result is not zero , you're returning back to user mode , hence call psig
*/
ret_from_sys_call:
    movl current, %eax
    cmpl $process_table, %eax
    je 1f

    movl PT_CS(%esp), %ebx
    testl $3, %ebx
    je 1f

    cmpw $0x23, PT_OLDSS(%esp)
    jne 1f

    call psig
1:
    ret

/* -----------------------------------------------------------------------
 * handle_sig – set up user stack to call signal handler on iret.
 * user stack's eip will be set to sigcatchter function address and the 
 * sigcatcher function's return address will be set to instruction address 
 * where originally process was interrupted.
 * ----------------------------------------------------------------------- */
handle_sig:
    movl current, %eax
    movl SIGHANDLE_OFF(%eax), %edx
    movl SIG_FN_OFF(%eax,%edx,4), %ebx
    xchgl PT_EIP(%esp), %ebx

    subl $28, PT_OLDESP(%esp)
    movl PT_OLDESP(%esp), %edx

    movl current, %eax
    movl RESTORER_OFF(%eax), %eax
    movl %eax, %fs:(%edx)

    movl current, %eax
    movl SIGHANDLE_OFF(%eax), %ecx
    movl %ecx, %fs:4(%edx)

    movl PT_EAX(%esp), %eax
    movl %eax, %fs:8(%edx)

    movl PT_ECX(%esp), %eax
    movl %eax, %fs:12(%edx)

    movl PT_EDX(%esp), %eax
    movl %eax, %fs:16(%edx)

    movl PT_EFLAGS(%esp), %eax
    movl %eax, %fs:20(%edx)

    movl %ebx, %fs:24(%edx)

    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    popl %fs
    popl %es
    popl %ds
    iret

/* -----------------------------------------------------------------------
 * ret_from_fork – entry point for a newly forked kernel thread.
 *
 * When the scheduler first runs a new task created by copy_thread(),
 * switch_to restores its esp and then rets into this label (because
 * copy_thread set new->thread.eip = ret_from_fork().
 *
 * At this point the stack looks like what copy_thread set up:
 *   esp childregs (pt_regs copy) same as parents, but below vleus are just changed
 *         which has eip = fn, eax = arg (for kernel_thread callers)
 *
 * We re-enable interrupts (they are off across switch_to) and then
 * iret into the thread function so it starts running with interrupts on.
 * ----------------------------------------------------------------------- */
/*
 * ret_from_fork - entry point for a newly created kernel thread.
 *
 * When switch_to first runs the child, its stack (next->thread.esp) points
 * at the pt_regs frame copy_thread built:
 *
 *   esp+0x00  ebx
 *   esp+0x04  ecx
 *   esp+0x08  edx
 *   esp+0x0C  esi
 *   esp+0x10  edi
 *   esp+0x14  ebp
 *   esp+0x18  eax   (= 0 for fork child; = arg for kernel_thread)
 *   esp+0x1C  ds
 *   esp+0x20  es
 *   esp+0x24  fs
 *   esp+0x28  gs
 *   esp+0x2C  orig_eax  (skip)
 *   esp+0x30  eip   (= fn for kernel_thread)
 *   esp+0x34  cs
 *   esp+0x38  eflags  (= 0x0202, IF enabled)
 *   esp+0x3C  oldesp  (only consumed by iret for ring-3; ignored for ring-0)
 *   esp+0x40  oldss
 *
 * We restore all general-purpose and segment registers then iret, which
 * picks up eip/cs/eflags and jumps into the thread function.
 */
ret_from_fork:
	popl %ebx
	popl %ecx
	popl %edx
	popl %esi
	popl %edi
	popl %ebp
	popl %eax
	popl %ds
	popl %es
	popl %fs
	popl %gs
	addl $4, %esp		/* skip orig_eax */
	iret		//go back to prev mode

/*
 * system_call - int 0x80 entry point.
 *
 * On entry the CPU has pushed (ring-3 to ring-0):  oldss, oldesp, eflags, cs, eip (these are of user)
 * We push orig_eax (= syscall number) then SAVE_ALL builds the full pt_regs.
 *
 * Stack after SAVE_ALL (esp points to pt_regs ):
 *   ebx ecx edx esi edi ebp eax ds es fs gs  orig_eax  eip cs eflags oldesp oldss
 *   0   4   8   C   10  14  18  1C 20 24 28  2C        30  34 38     3C     40
 */
system_call:
	pushl %eax              /* orig_eax = syscall number            */
	SAVE_ALL                /* build full pt_regs on kernel stack   */
	GET_THREAD_INFO %ebp    /* ebp = &thread_info (base of stack)   */

	movl PT_ORIG_EAX(%esp), %eax   /* reload syscall number into %eax */
	pushl %esp                     /* arg0 = ptr to pt_regs frame     */
	call *sys_call_table(,%eax,4)  /* dispatch: sys_call_table[nr]()  */
	addl $4, %esp                  /* pop the pt_regs arg             */
	movl %eax, PT_EAX(%esp)        /* store return value in eax, so for fork, when parent is returning,, it will have 
									here child's pid*/

syscall_exit:
	cli
	movl 4(%ebp), %ecx      /* thread_info->flags                   */
	testw $0xffff, %cx /* haha we don't have any flags nowwwwwwwwwwwwwwwwwwwwww */
	; jne  .check_signals    /* check for signals and handle singals    Later :), tomorrow, fuck bsp*/
	RESTORE_ALL             

.check_signals:
	sti
	call psig               /* deliver any pending signal           */
	jmp  syscall_exit       /* loop until clean                     */

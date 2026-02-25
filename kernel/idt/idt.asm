; idt_asm.asm
; Low-level exception handlers (Linux 0.11 style)

; Stack layout for saving registers
;	 0(%esp) - %eax
;	 4(%esp) - %ebx
;	 8(%esp) - %ecx
;	 C(%esp) - %edx
;	10(%esp) - %fs
;	14(%esp) - %es
;	18(%esp) - %ds
;	1C(%esp) - %eip
;	20(%esp) - %cs
;	24(%esp) - %eflags
;	28(%esp) - %oldesp
;	2C(%esp) - %oldss

EAX_OFF		equ 0x00
EBX_OFF		equ 0x04
ECX_OFF		equ 0x08
EDX_OFF		equ 0x0C
FS_OFF		equ 0x10
ES_OFF		equ 0x14
DS_OFF		equ 0x18
EIP_OFF		equ 0x1C
CS_OFF		equ 0x20
EFLAGS_OFF	equ 0x24
OLDESP_OFF	equ 0x28
OLDSS_OFF	equ 0x2C

BITS 32

global divide_error, debug, nmi, int3, overflow, bounds, invalid_op
global device_not_available, double_fault, coprocessor_segment_overrun
global invalid_TSS, segment_not_present, stack_segment
global general_protection, coprocessor_error, reserved
global save_registers

extern do_divide_error
extern do_int3
extern do_nmi
extern do_overflow
extern do_bounds
extern do_invalid_op
extern do_device_not_available
extern do_coprocessor_segment_overrun
extern do_reserved
extern do_coprocessor_error
extern do_double_fault
extern do_invalid_TSS
extern do_segment_not_present
extern do_stack_segment
extern do_general_protection
extern jiffies

; -------------------------
; Exceptions without error code
; -------------------------

divide_error:
    push dword do_divide_error

no_error_code:
    xchg eax, [esp]
    push ebx
    push ecx
    push edx
    push edi
    push esi
    push ebp
    push ds
    push es
    push fs
    push dword 0              ; fake error code
    lea edx, [esp + 44]
    push edx
    mov edx, 0x10             ; kernel data selector
    mov ds, dx
    mov es, dx
    mov fs, dx
    call eax
    add esp, 8
    pop fs
    pop es
    pop ds
    pop ebp
    pop esi
    pop edi
    pop edx
    pop ecx
    pop ebx
    pop eax
    iret

debug:
    push dword do_int3
    jmp no_error_code

nmi:
    push dword do_nmi
    jmp no_error_code

int3:
    push dword do_int3
    jmp no_error_code

overflow:
    push dword do_overflow
    jmp no_error_code

bounds:
    push dword do_bounds
    jmp no_error_code

invalid_op:
    push dword do_invalid_op
    jmp no_error_code

; -------------------------
; Math / coprocessor
; -------------------------

global device_not_available

extern current
extern last_task_used_math
extern math_state_restore


math_emulate:
    pop eax
    push dword do_device_not_available
    jmp no_error_code
; Device Not Available (#NM, vector 7)
; Lazy FPU handling (Linux 0.11 style)
device_not_available:
    push eax

    mov eax, cr0
    bt eax, 2                  ; test EM (math emulation) bit
    jc math_emulate

    clts                       ; clear TS so FPU can be used

    mov eax, [current]
    cmp eax, [last_task_used_math]
    je .done                   ; shouldn't happen really

    push ecx
    push edx
    push ds

    mov eax, 0x10              ; kernel data selector
    mov ds, ax

    call math_state_restore

    pop ds
    pop edx
    pop ecx

.done:
    pop eax
    iret

coprocessor_segment_overrun:
    push dword do_coprocessor_segment_overrun
    jmp no_error_code

reserved:
    push dword do_reserved
    jmp no_error_code

coprocessor_error:
    push dword do_coprocessor_error
    jmp no_error_code

; -------------------------
; Exceptions WITH error code
; -------------------------

double_fault:
    push dword do_double_fault

error_code:
    xchg eax, [esp + 4]        ; error code <-> eax
    xchg ebx, [esp]            ; handler <-> ebx
    push ecx
    push edx
    push edi
    push esi
    push ebp
    push ds
    push es
    push fs
    push eax                  ; error code
    lea eax, [esp + 44]
    push eax
    mov eax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    call ebx
    add esp, 8
    pop fs
    pop es
    pop ds
    pop ebp
    pop esi
    pop edi
    pop edx
    pop ecx
    pop ebx
    pop eax
    iret

invalid_TSS:
    push dword do_invalid_TSS
    jmp error_code

segment_not_present:
    push dword do_segment_not_present
    jmp error_code

stack_segment:
    push dword do_stack_segment
    jmp error_code

general_protection:
    push dword do_general_protection
    jmp error_code

extern process_table
extern psig

ret_from_sys_call:
    mov eax, [current]          ; check that the current process is not task 0
    cmp eax, process_table
    je .done
    mov ebx, [esp + CS_OFF]
    test ebx, 3
    je .done                    ; jump out if we came from kernel mode itself
    cmp word [esp + OLDSS_OFF], 0x23 ; check that the stack segment pushed in kernel stack is equal to user 
                                ; stack segment i.e USER_DATA index in gdt (0x20) + 3 (cpl)
    jne .done                   ; jump out if you're not returning back to user mode
    call psig
.done:

; -----------------------------------------------------------------------------
; 80386 INTERRUPT STACK FRAME LAYOUT
;
; Reference:
;   Intel 80386 Programmer's Reference Manual
;   path: file:///home/roseate/Documents/books/machines/i386_inteL_manual.pdf
;   Chapter 9 — Exceptions and Interrupts
;   Section: Interrupt Stack Frame
;
; The CPU pushes different stack frames depending on:
;   1) Whether privilege level changes (CPL change)
;   2) Whether the exception provides an error code
;
; -----------------------------------------------------------------------------
; CASE 1: NO PRIVILEGE TRANSITION (same CPL)
; -----------------------------------------------------------------------------
;
; WITHOUT ERROR CODE
;
;   Higher Address
;   ---------------
;        OLD EFLAGS
;        OLD CS
;        OLD EIP
;   ---------------
;   Lower Address   <-- ESP after interrupt
;
;
; WITH ERROR CODE
;
;   Higher Address
;   ---------------
;        OLD EFLAGS
;        OLD CS
;        OLD EIP
;        ERROR CODE
;   ---------------
;   Lower Address   <-- ESP after interrupt
;
;
; -----------------------------------------------------------------------------
; CASE 2: WITH PRIVILEGE TRANSITION (e.g., Ring 3 → Ring 0)
; -----------------------------------------------------------------------------
;
; WITHOUT ERROR CODE
;
;   Higher Address
;   ---------------
;        OLD SS
;        OLD ESP
;        OLD EFLAGS
;        OLD CS
;        OLD EIP
;   ---------------
;   Lower Address   <-- New ESP (from TSS)
;
;
; WITH ERROR CODE
;
;   Higher Address
;   ---------------
;        OLD SS
;        OLD ESP
;        OLD EFLAGS
;        OLD CS
;        OLD EIP
;        ERROR CODE
;   ---------------
;   Lower Address   <-- New ESP (from TSS)
;
;
; NOTES:
; - On privilege change, CPU loads new SS:ESP from TSS.
; - OLD SS and OLD ESP are pushed only if CPL changes.
; - IRET restores the frame in reverse order.
; - timer interrupt chya case madhe error code store hoat nahi.
; - ERROR CODE is pushed only for specific exceptions.
;
; -----------------------------------------------------------------------------

; kernel stack frame when we save registers, so called hardware level context
; as said by bach
;ESP →   +0   GS
;        +4   FS
;        +8   ES
;        +12  DS
;        +16  EDI
;        +20  ESI
;        +24  EBP
;        +28  OLD ESP (pushad)
;        +32  EBX
;        +36  EDX
;        +40  ECX
;        +44  EAX
;        --------------------------------
;        +48  EIP
;        +52  CS
;        +56  EFLAGS
;        --------------------------------- ;old esp and ss jar apan mode 3->0 karat ahot
;        +60  OLD ESP (ring3)
;        +64  OLD SS
;

global timer_intr
extern jiffies
extern operate_timer
extern send_EOI

timer_intr: 
    pushad          ; save all generaln registers
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10    ;we switch to kernel DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    inc dword [jiffies] ;not useful now, nantar vapru
    
    push dword 1
    call send_EOI   
    add esp, 4

    ;determing the cpl
    mov eax, [esp + 52] ; previous cs register saved
    and eax, 3
    push eax
    call operate_timer
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popad

    iret

    

SIGHANDLE_OFF   equ 20
RESTORER_OFF    equ 24
SIG_FN_OFF      equ 28

global handle_sig

handle_sig:
    mov eax, [current]
    movzx edx, byte [eax + SIGHANDLE_OFF]
    mov ebx, [eax + SIG_FN_OFF + edx*4] ; get the sighandler address
    xchg [esp + EIP_OFF], ebx
    sub dword [esp + OLDESP_OFF], 28
    mov edx, [esp + OLDESP_OFF]

; call verify area to check if sighandler is not out of range

    mov eax, [current]
    mov eax, [eax + RESTORER_OFF]
    mov [fs:edx], eax        ; flag/reg restorer
    
    mov eax, [current]
    movzx ecx, byte [eax + SIGHANDLE_OFF]
    mov [fs:edx + 4], ecx         ; signal nr
    
    mov eax, [esp + EAX_OFF]
    mov [fs:edx + 8], eax    ; old eax
    mov eax, [esp + ECX_OFF]
    mov [fs:edx + 12], eax   ; old ecx
    mov eax, [esp + EDX_OFF]
    mov [fs:edx + 16], eax   ; old edx
    mov eax, [esp + EFLAGS_OFF]
    mov [fs:edx + 20], eax   ; old eflags
    mov [fs:edx + 24], ebx   ; old return addr
    pop eax
    pop ebx
    pop ecx
    pop edx
    pop fs
    pop es
    pop ds
    iret

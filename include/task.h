#ifndef _TASK_H
#define _TASK_H

#include "list.h"
#include "vfs.h"

/* Process state transitions, chapter 6 ,
 * This describes a human's life
 * starting with forked 8
 * the child is created in mother's womb
 * whatever te mother eats goes into child (address space)
 * mother and child are tied with the umbilical knot
 * when user(child) calls exec, the tied knot is cut between
 * mother and child, signifying that child was born for his own karm.
 */
//1 and 2 are of no use, just modes
#define USER_RUNNING 1
#define KERNEL_RUNNING 2
#define READY_TO_RUN_M 3 //ready to run in memory
#define ASLEEP_M 4//asleep in memory
#define READ_TO_RUN_S 5// read to run but swapped
#define ASLEEP_S 6//asleep and swapped
#define PREEMPTED 7 //preempted while returning to user mode
#define FORKED 8 //just created
#define ZOMBIE 9 //zombie bro

#define NR_TASKS      32
#define TIME_QUANTUM  20    /* timer ticks before preemption */
typedef int (*fn_ptr)();

struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

struct tss_struct {
    uint32_t back_link; //16bits long 
    uint32_t esp0;  //kernel stack pointer and only used when mode 3 to 0 takes place,
                    //hence it doesn;t have the current track of stack pointer in kernel
                    //mode
    uint32_t ss0;  //16bits , kernel stack segment
    uint32_t esp1; 
    uint32_t ss1; //16bits
    uint32_t esp2;
    uint32_t ss2; //16bits
    uint32_t cr3; //its va_space i.e pg_dir
    uint32_t eip;
    uint32_t eflags;

/* general purpose registers */
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
/* segment registers */
    uint32_t es; /* 16 high bits are zero */
    uint32_t cs; /* 16 high bits are zero */
    uint32_t ss; /* 16 high bits are zero */
    uint32_t ds; /* 16 high bits are zero */
    uint32_t fs; /* 16 high bits are zero */
    uint32_t gs; /* 16 high bits are zero */
    uint32_t ldtr; /* 16 high bits are zero */

    uint32_t iopb; // trace bitmap 0-15: reserved, 16-31: iopb
    struct i387_struct i387;
}__attribute__((packed));

struct region {
    // struct m_inode *ex_inode; //pointer to inode of executable file
    char reg_type; 
    unsigned long reg_size;
    unsigned long reg_phy_addr; //physical address of region
    unsigned char status; /*
                             locked,
                             in demand,
                             being loaded in memory,
                             valid(loaded in memory)
                             */

    unsigned int ref_cnt; //no of proc using this region
};
/* per process region table */
/*
 * pprt contains the starting virtual addresses of the regions
 * pprt allows giving permissions for a process to access
 * its regions r/w, r, w, etc much similar like file table */
/* bach ch6 page 166 */
struct pprt {
    unsigned long va_text;
    unsigned char p_text; //permission for text
    struct region *ptr_text;
    unsigned long va_data;
    unsigned char p_data;
    struct region *ptr_data;
    unsigned long va_stack;
    unsigned char p_stack;
    struct region *ptr_stack;
 };

struct thread_struct {
    unsigned long esp0; //similar to TSS.esp0 , points to top of stack
    unsigned long esp; //saved on switch 
    uint32_t ss0;
    unsigned long eip;
    unsigned long fs;
    unsigned long gs;
    unsigned long iopl;
    struct i387_struct i387;
};
/*
 * kernel stack process
 * Each process will have :
 * a kernel stack pointer , 
    When switching:
    mov esp, next->kernel_stack_top
    And before returning to user mode:

    Update TSS.esp0 to:
    current->esp

    TSS madhe jo esp ahe , tyachakade current esp cha count nasto
    so context switch karaycha adhi, update it to current to current esp
    so apan jevha ya process madhe parat yeu tevha tss cha esp previously save kelela milel.

    Eg.
    Process A
    current->tss.esp = 8000
    current->esp = 8000
    .....
    ... blah blah operations
    now we need to switch, so save the esp values
    noew esp is = 8192
    current->esp = esp;
    current->tss.esp = current->esp;

    Process B.... Process C...

    Context switched back to process A
    current->tss.esp = 8192
    current->esp = 8192
*/

struct task_struct {
    struct thread_struct thread;
    unsigned long *stack; //points to the stack  of this task
    long state; 
    long priority;
    unsigned int counter; //the time unit for process scheduling
    long signal; //signals sent to the process but not yet handled
    char sighandle; //signal marked by issig to be handled by psic
    fn_ptr sig_restorer;
    fn_ptr sig_fn[32];
    
    int exit_code;
    struct pprt pprt;
    long pid,father,pgrp,session,leader;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	long alarm;
	long utime,stime,cutime,cstime,start_time;
	unsigned short used_math;

    /* commenting now since file system is not ready */
	// int tty;		/* -1 if no tty, so it must be signed */
	// unsigned short umask;
	struct inode * pwd;
	struct inode * root;
	// unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
    int first_free_filp; //file free filp entry to save time

};

struct thread_info {
    struct task_struct *task;
    unsigned int flags;
};

/*
 * instrad of thread_info we'll be using 
 * task_struct directly
 */
#define STACK_SIZE 8192
#define THREAD_SIZE STACK_SIZE
#define current_task() \
    ((struct thread_info*)(esp & ~(STACK_SIZE - 1)))
/*
 * Kernel stack layout (8KB per thread)
 *
 * Stack size = 8192 bytes (8KB)
 *
 * Example memory layout:
 *
 *   0xC0400000  <- High address (top of kernel stack)
 *        |
 *        |  Stack grows downward
 *        V
 *   0xC03FE000  <= Base of stack page (thread_info lives here)
 *
 * So:
 *   thead_info = 0xC03FE000
 *   stack_top   = 0xC0400000
 *
 * If at some point:
 *
 *   %esp = 0xC03FF800
 *
 * We can compute task_struct using masking:
 *
 *   thread_info = esp & ~(8192 - 1)
 *               = esp & ~0x1FFF
 *               = esp & 0xFFFFE000
 *
 * Why this works:
 *
 *   - 8192 bytes = 2^13
 *   - So lower 13 bits represent offset inside the stack
 *   - Masking them out gives the page-aligned base address
 *   - That base address is exactly where thread_info resides
 *
 * This allows O(1) retrieval of current thread_info
 * directly from the stack pointer.
 */

// #define INIT_TASK { \
//     .state = 0, \
//     .priority = 15, \
//     .counter = 0,\
//     .signal = 0, \
//     .sighandle = 0, \
//     .sig_restorer = NULL, \
//     .sig_fn = { [0 ... 31] = NULL }, \
//     .exit_code = 0, \
//     .pprt = { 0 }, \
//     .pid = 0, \
//     .father = -1, \
//     .pgrp = 0, \
//     .session = 0, \
//     .leader = 0, \
//     .uid = 0, .euid = 0, .suid = 0, \
//     .gid = 0, .egid = 0, .sgid = 0, \
//     .alarm = 0, \
//     .utime = 0, .stime = 0, .cutime = 0, .cstime = 0, .start_time = 0, \
//     .used_math = 0,\
//     .pwd = NULL, \
//     .root = NULL, \
//     .filp = 0, \
//     .first_free_filp = 0, \
// }


/* this struct defines the way the registers are stored on the
   stack during a system call. */

/*
 pt_regs structure on the Kernel Mode (exception) stack contain-
ing the registers saved right after the interrupt occurred. The pt_regs structure
consists of 15 fields:
• The first nine fields are the register values pushed by SAVE_ALL
• The tenth field, referenced through a field called orig_eax, encodes the IRQ
number
• The remaining fields correspond to the register values pushed on automati-
cally by the control unit
*/
struct pt_regs {
    unsigned long ebx;      /* 0x00 */
    unsigned long ecx;      /* 0x04 */
    unsigned long edx;      /* 0x08 */
    unsigned long esi;      /* 0x0C */
    unsigned long edi;      /* 0x10 */
    unsigned long ebp;      /* 0x14 */
    unsigned long eax;      /* 0x18 */

    unsigned long ds;       /* 0x1C */
    unsigned long es;       /* 0x20 */
    unsigned long fs;       /* 0x24 */
    unsigned long gs;       /* 0x28 */

    unsigned long orig_eax; /* 0x2C */

    unsigned long eip;      /* 0x30 */
    unsigned long cs;       /* 0x34 */
    unsigned long eflags;   /* 0x38 */
    unsigned long oldesp;   /* 0x3C */
    unsigned long oldss;    /* 0x40 */
};

#define PRIORITIES 5
struct prio_array_t {
    int nr_active;
    unsigned long bitmap[1]; 
    /*
     * A priority bitmap: each flag is set if and only if the corre-
     * sponding priority list is not empty
    */
    struct list_head queue[PRIORITIES];
};

//gives you kernel stack top
#define KSTK_TOP(info) \
({ \
    (unsigned long)((unsigned long)info + THREAD_SIZE); \
})
/*
 *
 *
 *  High address
    +-------------------------+
    | reserved 8 bytes        |
    +-------------------------+
    | struct pt_regs          | ← task_pt_regs()
    +-------------------------+
    | kernel stack            |
    | function frames         |
    | interrupts              |
    | syscalls                |
    +-------------------------+
    | struct thread_info      |
    +-------------------------+
    Low address
    In the below the macro, 
    it first points to top of stack (high address), and then 
    subtracts 8 because you may have entered from user mode,
    hence it saves ss and esp, if its switching from kernel 
    to kernel mode, nothing is saved.
    But to keep standard we've done this.
    __regs__ - 1 is pointer arithmetic 


*/
#define task_pt_regs(task)                                             \
({                                                                     \
       struct pt_regs *__regs__;                                       \
       __regs__ = (struct pt_regs *)(KSTK_TOP(task_stack_page(task)) - 8); \
       __regs__ - 1;                                                   \
})

    
void tss_load(int offset);
void sched_init(void);
void do_exit(int signal);
void handle_sig(void);
void schedule(void);
int kernel_thread(int (*fn)(void *), void *arg);
void move_to_user_mode(void);
int do_fork(unsigned long clone_flags, unsigned long stack_start,
            struct pt_regs *regs, unsigned long stack_size);
int find_empty_process(void);

/* Allocation helpers defined in task.c */
struct task_struct *alloc_task_struct(void);
void free_task_struct(struct task_struct *task);
struct thread_info *alloc_thread_info(struct task_struct *task);
void *alloc_kernel_stack(void);

/* Global scheduler state */
extern struct task_struct *current;
extern struct task_struct *process_table[NR_TASKS];
extern unsigned long jiffies;
extern struct tss_struct cpu_tss;


#endif

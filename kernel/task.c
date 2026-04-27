#include "serial.h"
#include "task.h"
#include "mm.h"
#include "zone.h"
#include "slab.h"
#include "time.h"
#include "signal.h"
#include "timer.h"
#include "string.h"

#define KERNEL_CS   0x08
#define KERNEL_DATA 0x10
#define KSTACK_ORDER 1   
#define TSS_ENTRY    5

struct tss_struct cpu_tss;
struct task_struct *current = NULL;
struct task_struct *process_table[NR_TASKS];

kmem_cache_t *task_cache = NULL;
struct task_struct *last_task_used_math = NULL;
unsigned long jiffies = 0;

/* forward decl from switch.S */
extern void switch_to(struct task_struct *prev,
                      struct task_struct *next,
                      struct task_struct **last);

/*
 * alloc_kernel_stack - allocates a 2-page (8KB) kernel stack.
 * Returns a pointer to the TOP of the stack (high address).
 */
void *alloc_kernel_stack(void)
{
    struct page *p = alloc_pages(0, KSTACK_ORDER);
    if (!p)
        return NULL;

    void *base = page_address(p);
    void *top  = base + STACK_SIZE;
    printk("[task] stack: base=%p top=%p\n", base, top);
    return top;   /* callers treat this as the high-address stack top */
}

struct task_struct *alloc_task_struct(void)
{
    struct task_struct *task;
    task = kmem_cache_zalloc(task_cache, 0);
    if (!task)
        printk("alloc_task_struct: kmem_cache_zalloc failed\n");
    return task;
}

void free_task_struct(struct task_struct *task)
{
    kmem_cache_free(task_cache, task);
}

/*
 * alloc_thread_info - "allocates" a thread_info at the base of a new
 * kernel stack.  We reuse alloc_kernel_stack: it returns the TOP, so
 * we subtract STACK_SIZE to get the base where thread_info lives.
 */
struct thread_info *alloc_thread_info(struct task_struct *task)
{
    void *top = alloc_kernel_stack();
    if (!top) {
        printk("alloc_thread_info: failed to allocate kernel stack\n");
        return NULL;
    }
    /* thread_info sits at the very base (low address) of the stack */
    return (struct thread_info *)((unsigned long)top - STACK_SIZE);
}

void math_state_restore(void)
{
    if (last_task_used_math)
        __asm__("fnsave %0" :: "m"(last_task_used_math->thread.i387));
    if (current->used_math)
        __asm__("frstor %0" :: "m"(current->thread.i387));
    else {
        __asm__("fninit" ::);
        current->used_math = 1;
    }
    last_task_used_math = current;
}

bool issig(void)
{
    int iCnt = 0, siglen = sizeof(long);
    while (iCnt < siglen) {
        current->sighandle = iCnt;
        if (IS_FLAG(current->signal, 1 << iCnt)) {
            if (iCnt == SIGCLD) {
                if (current->sig_fn[iCnt] == (void *)1) {
                    /* ignore: free zombie children */
                } else if (!current->sig_fn[iCnt]) {
                    return true;
                }
            } else if (!current->sig_fn[iCnt]) {
                return true;
            }
            CLEAR_FLAG(current->signal, 1 << iCnt);
        }
        iCnt++;
    }
    return false;
}

void psig(void)
{
    if (!issig()) {
        do_exit(current->sighandle);
        return;
    }
    CLEAR_FLAG(current->signal, 1 << current->sighandle);
    if (!current->sig_fn[current->sighandle]) {
        handle_sig();
        return;
    }
    do_exit(current->sighandle);
}

static void context_switch(struct task_struct *prev, struct task_struct *next)
{
    struct task_struct *last = NULL;
    printk("[sched] pid %d -> pid %d\n", prev->pid, next->pid);
    current = next;
    switch_to(prev, next, &last);
    /* 'last' is the task that was running before *this* task was switched
     * back to — useful for cleaning up (e.g., on exit), ignored for now. */
}

void schedule(void)
{
    int iCnt, next = -1;
    struct task_struct *tnext = NULL;

    for (iCnt = 1; iCnt < NR_TASKS; iCnt++) {
        int idx = (current->pid + iCnt) % NR_TASKS;
        if (process_table[idx] && process_table[idx]->state == READY_TO_RUN_M) {
            next = idx;
            break;
        }
    }

    if (next == -1) {
        /* no other runnable task */
        return;
    }

    if (next == (int)current->pid)
        return;

    tnext = process_table[next];
    tnext->counter = TIME_QUANTUM;
    context_switch(current, tnext);
}

void do_exit(int signal)
{
    /* TODO: clean up task resources */
    current->state = ZOMBIE;
    schedule();
}

int find_empty_process(void)
{
    int iCnt;
    for (iCnt = 1; iCnt < NR_TASKS; iCnt++) {
        if (!process_table[iCnt])
            return iCnt;
    }
    return -1;
}

void sched_init(void)
{
    int iCnt;
    printk("sched_init: initialising scheduler\n");

    memset(&cpu_tss, 0, sizeof(struct tss_struct));
    cpu_tss.ss0 = KERNEL_DATA;  /* iwthout this kernel stack won't work */

    for (iCnt = 0; iCnt < NR_TASKS; iCnt++)
        process_table[iCnt] = NULL;

    task_cache = kmem_cache_create("task_struct", sizeof(struct task_struct),
                                    KMALLOC_MINALIGN, SLAB_HWCACHE_ALIGN, NULL);
    if (!task_cache) {
        printk("sched_init: failed to create task_struct cache\n");
        return;
    }

    struct task_struct *init = alloc_task_struct();
    if (!init)
        return;

    /*
     * The idle (init) task gets a fresh kernel stack.
     * alloc_kernel_stack() returns the TOP (high address).
     */
    void *stack_top = alloc_kernel_stack();
    if (!stack_top) {
        printk("sched_init: failed to allocate init stack\n");
        free_task_struct(init);
        return;
    }

    /* thread_info lives at base of the stack */
    init->stack = (unsigned long *)((unsigned long)stack_top - STACK_SIZE);

    init->thread.esp0 = (unsigned long)stack_top;
    init->thread.esp  = (unsigned long)stack_top;
    init->thread.ss0  = KERNEL_DATA;

    init->state    = READY_TO_RUN_M;
    init->priority = 1;
    init->counter  = TIME_QUANTUM;
    init->pid      = 0;

    process_table[0] = init;
    current = init;

    printk("sched_init: idle task esp0=%x\n", init->thread.esp0);


}

/* -----------------------------------------------------------------------
 * kernel_thread - create a kernel-mode thread that runs fn(arg).
 * ----------------------------------------------------------------------- */
int kernel_thread(int (*fn)(void *), void *arg)
{
    struct pt_regs regs;
    memset(&regs, 0, sizeof(regs));

    /*
     * Arrange for ret_from_fork to iret into fn with arg in eax.
     * copy_thread will copy this regs frame onto the child's kernel stack.
     */
    regs.eip    = (unsigned long)fn;
    regs.eax    = (unsigned long)arg;
    regs.ds     = KERNEL_DATA;
    regs.es     = KERNEL_DATA;
    regs.fs     = KERNEL_DATA;
    regs.cs     = KERNEL_CS;
    regs.eflags = 0x0202;       /* IF=1 (enable interrupts) + reserved bit 1 */

    return do_fork(0, 0, &regs, 0);
}

int fork(void)
{
    struct pt_regs regs;
    memset(&regs, 0, sizeof(regs));
    /* For a proper fork we'd capture the current register state here;
     * use do_fork like Linux does instead. */
    return do_fork(0, 0, &regs, 0);
}

/* -----------------------------------------------------------------------
 * move_to_user_mode - transition the calling kernel thread to ring 3.
 *
 * Builds a fake iret frame on the kernel stack so faking that user called it and we're returning back
 *   ss     = 0x23  (user data segment, RPL=3, GDT index 4)
 *   esp    = current %esp (reuse same stack(linus did the same); fine for early testing)
 *   eflags = IF=1
 *   cs     = 0x1B  (user code segment, RPL=3, GDT index 3)
 *   eip    = address of next instruction (the label '1:' below)
 *
 * After iret the CPU switches to ring 3.  ds/es/fs/gs are reloaded
 * to the user data selector so user-mode code can access data.
 * ----------------------------------------------------------------------- */
void move_to_user_mode(void)
{
    __asm__ volatile (
        "movl %%esp, %%eax      \n\t"   /* save current esp              */
        "pushl $0x23            \n\t"   /* SS  = user data seg (RPL=3)   */
        "pushl %%eax            \n\t"   /* ESP                           */
        "pushfl                 \n\t"   /* EFLAGS                        */
        "orl   $0x200, (%%esp)  \n\t"   /* set IF in saved eflags        */
        "pushl $0x1B            \n\t"   /* CS  = user code seg (RPL=3)   */
        "pushl $1f              \n\t"   /* EIP = label after iret        */
        "iret                   \n\t"   /* got back to ring 3                     */
        "1:                     \n\t"
        /* now in ring 3; reload data segments */
        "movl $0x23, %%eax      \n\t"
        "movw %%ax, %%ds        \n\t"
        "movw %%ax, %%es        \n\t"
        "movw %%ax, %%fs        \n\t"
        "movw %%ax, %%gs        \n\t"
        ::: "eax", "memory"
    );
}

int sys_kill(int pid, int sig)
{
    //return do_kill(pid, sig);
}

int do_kill(int pid, int sig)
{
    int iCnt = 0;
    for (iCnt = 0; iCnt < NR_TASKS; iCnt++) {
        if (process_table[iCnt]->pid == pid) {
            if (IS_FLAG(process_table[iCnt]->signal, sig)) {
                printk("signal is already sent\n");
                return 0;
            }
            SET_FLAG(process_table[iCnt]->signal, sig);
            return 0;
        }
    }
    return -1;
}

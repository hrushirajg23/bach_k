#include "serial.h"
#include "task.h"
#include "mm.h"
#include "zone.h"
#include "slab.h"
#include "time.h"
#include "signal.h"
#include "timer.h"

#define KERNEL_DATA 0x10
#define KSTACK_ORDER 1   // 8KB stack
#define STACK_SIZE ( PAGE_SIZE << KSTACK_ORDER )
#define TSS_ENTRY 5

struct tss_struct cpu_tss;
struct task_struct *current = NULL;
struct task_struct *process_table[NR_TASKS];

kmem_cache_t *task_cache = NULL;
struct task_struct *last_task_used_math = NULL;
unsigned long jiffies = 0;

void *alloc_kernel_stack(void)
{
    struct page *p = alloc_pages(0, KSTACK_ORDER);
    if (!p)
        return NULL;

    void *addr = page_address(p);
    printk("stack address will be %p and esp is %p\n", addr, addr + (PAGE_SIZE << KSTACK_ORDER));
    return page_address(p) + (PAGE_SIZE << KSTACK_ORDER);
}

extern void context_switch(struct task_struct *prev, struct task_struct *next);

static inline void context_switch(struct task_struct *prev,
                                   struct task_struct *next)
{
    printk("from current : %d, switching to %d\n", current->pid, next->pid);
    if (next == current)
        return;
    current = next;
    switch_to(prev, next, prev); 
}

/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math)
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	if (current->used_math)
		__asm__("frstor %0"::"m" (current->tss.i387));
	else {
		__asm__("fninit"::);
		current->used_math=1;
	}
	last_task_used_math=current;
}
int fork(void)
{
    int iCnt = 0;
    for (iCnt = 0; iCnt < NR_TASKS; iCnt++) {
        if (process_table[iCnt] == NULL) 
            break;
    }
    if (iCnt == NR_TASKS) {
        printk("process table full\n");
        return -1;
    }

    struct task_struct *c = kmem_cache_zalloc(task_cache, 0);
    if (!c) {
        printk("failed to allocate cache object for task_struct \n");
        return -1;
    }
    /*
     * allocate a stack and then
     * copy the current process's stack into child stack
     */

    void *stack_top = alloc_kernel_stack();
    memcpy(stack_top, (void *)((unsigned long)current->esp - 
            ((unsigned long)current->esp % STACK_SIZE)), 
            (unsigned long)current->esp % STACK_SIZE); 
    stack_top += current->esp % STACK_SIZE;

    c->esp = (unsigned long)stack_top;
    c->pid = iCnt;
    c->state = READY_TO_RUN_M;
    c->counter = TIME_QUANTUM;

    process_table[iCnt] = c;

    return iCnt;
    
}

void sched_init(void)
{

    int iCnt = 0;
    printk("initializing scheduler \n");

    memset(&cpu_tss, 0, sizeof(struct tss_struct));

    for (iCnt = 0; iCnt < NR_TASKS; iCnt++){
        process_table[iCnt] = NULL;
    }

    task_cache = kmem_cache_create("task_struct", sizeof(struct task_struct),
            KMALLOC_MINALIGN, SLAB_HWCACHE_ALIGN, NULL);
    if (!task_cache) {
        printk("failed to create task_struct cache\n");
        return;
    }

    struct task_struct *init = kmem_cache_zalloc(task_cache, 0);
    if (!init) {
        printk("failed to alloc task_struct cache entry\n");
        return;
    }

    void *stack_top = alloc_kernel_stack();

    init->thread.esp = (unsigned long)stack_top;
    init->state = READY_TO_RUN_M;
    init->priority = 1;
    init->counter = TIME_QUANTUM;
    init->pid = 0; //first handmade process 

    process_table[0] = init;
    current = init;

    init->thread.esp0 = (unsigned long)stack_top;
    init->thread.ss0  = KERNEL_DATA;

    printk("kernel stack points to %x\n", init->thread.esp0);
   

}

/* test for receipt of signals 
 * input: none
 * output: true, if process received signals that it does not
 *          ignore
 *          false, otherwise
 */
bool issig(void)
{
    int iCnt = 0, siglen = sizeof(long);
    while (iCnt < siglen) {
        current->sighandle = iCnt;
        if(IS_FLAG(current->signal, 1 << iCnt)) {
            /*
             * if signal is death of child
             * special case
             */
            if (iCnt == SIGCLD) {

                /* if ignoring death of child signals */
                if (current->sig_fn[iCnt] == (void*)1) {
                    /*
                     * free process table entries of zombie children
                     */
                }
                else if (!(current->sig_fn[iCnt])) {
                    return true;
                }
            }
            else if (!(current->sig_fn[iCnt])) {
                return true;
            }
            CLEAR_FLAG(current->signal, 1 << iCnt);
        }
    }
    return false;
}

/* handle signals after recognizing their 
 * existence 
 */
void psig(void)
{
    if (!issig()) {
        do_exit(current->sighandle); //paramter as the first 8 bytes, which describe the signal due to which process exited
    }
    CLEAR_FLAG(current->signal, 1 << current->sighandle); //if we've return true from issig() 
                                                          //we have not cleared the set signal bit

    /*
     * user has specified handler
     */
    if (!(current->sig_fn[current->sighandle])) {
        handle_sig();
    }
    do_exit(current->sighandle);
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
        printk("[sched] no runnable task found (current pid=%d)\n", current->pid);
        return;
    }
    if (next == (int)current->pid) {
        /* no other task available, keep running */
        return;
    }
    tnext = process_table[next];
    tnext->counter = TIME_QUANTUM;
    printk("[sched] switching pid %d -> pid %d\n", current->pid, next);
    switch_to(tnext);
}

void do_exit(int signal)
{
}



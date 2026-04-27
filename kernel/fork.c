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

#define task_thread_info(task)  ((struct thread_info *)(task->stack))
#define task_stack_page(task)   ((task)->stack)

extern void ret_from_fork(void);

static inline void setup_thread_stack(struct task_struct *new,
                                      struct task_struct *org)
{
    *task_thread_info(new) = *task_thread_info(org);
    task_thread_info(new)->task = new;
}

static inline unsigned long *end_of_stack(struct task_struct *task)
{
    return (unsigned long *)(task_thread_info(task) + 1);
}

/*
 * dup_task_struct - allocate a new task_struct + kernel stack
 *                   and copy from @from.
 */
static struct task_struct *dup_task_struct(struct task_struct *from)
{
    struct thread_info *ti;
    struct task_struct *new;

    new = alloc_task_struct();
    if (!new) {
        printk("dup_task_struct: failed to allocate task_struct\n");
        return NULL;
    }

    ti = alloc_thread_info(new);
    if (!ti) {
        printk("dup_task_struct: failed to allocate thread_info\n");
        free_task_struct(new);
        return NULL;
    }

    /* copy parent's task_struct */
    *new = *from;

    /* give child its own stack */
    new->stack = (unsigned long *)ti;

    setup_thread_stack(new, from);

    return new;
}

/*
 * copy_thread - set up the child's kernel stack so that when the
 *               scheduler first runs it, it lands at ret_from_fork.
 *
 * For kernel threads (stack_start == 0): regs->eip is the thread fn,
 *                                         regs->eax is the arg.
 */
int copy_thread(unsigned long clone_flags, unsigned long sp,
                struct task_struct *new, struct pt_regs *regs)
{
    struct pt_regs *childregs;

    /* Place a pt_regs frame at the top of the new child's kernel stack */
    childregs = task_pt_regs(new);

    /* Copy the parent's saved register state */
    *childregs = *regs;

    /* Child returns 0 from fork (or lands in its thread fn via iret) */
    childregs->eax = 0;

    /* For kernel threads the caller passes the desired esp;
     * for user-space forks it is the parent's user-space esp. */
    if (sp)
        childregs->oldesp = sp;

    /* thread.esp points just below the pt_regs frame.
     * switch_to restores esp here, then rets into thread.eip. */
    new->thread.esp  = (unsigned long)childregs;
    new->thread.esp0 = (unsigned long)(childregs + 1); /* kernel stack top */

    /* When the scheduler first runs this task it will ret into ret_from_fork,
     * which restores the pt_regs and irets into the thread function. */
    new->thread.eip  = (unsigned long)ret_from_fork;

    return 0;
}

/*
 * copy_process - duplicate the current process/thread.
 */
struct task_struct *copy_process(unsigned long clone_flags,
                                 unsigned long stack_start,
                                 struct pt_regs *regs,
                                 unsigned long stack_size)
{
    int ret;
    struct task_struct *new;

    new = dup_task_struct(current);
    if (!new) {
        printk("copy_process: dup_task_struct failed\n");
        return NULL;
    }

    ret = copy_thread(clone_flags, stack_start, new, regs);
    if (ret < 0) {
        printk("copy_process: copy_thread failed (%d)\n", ret);
        free_task_struct(new);
        return NULL;
    }

    return new;
}

/*
 * do_fork - the heart of fork()/kernel_thread().
 *
 * Returns the new PID on success, negative on failure.
 */
int do_fork(unsigned long clone_flags, unsigned long stack_start,
            struct pt_regs *regs, unsigned long stack_size)
{
    struct task_struct *new;
    int nr;

    nr = find_empty_process();
    if (nr < 0) {
        printk("do_fork: process table full\n");
        return -1;
    }

    new = copy_process(clone_flags, stack_start, regs, stack_size);
    if (!new) {
        printk("do_fork: copy_process failed\n");
        return -1;
    }

    new->pid      = nr;
    new->father   = current->pid;
    new->state    = READY_TO_RUN_M;
    new->counter  = TIME_QUANTUM;

    process_table[nr] = new;

    printk("[fork] created pid %d (parent pid %d)\n", nr, current->pid);

    return nr;
}

int sys_fork(struct pt_regs *regs)
{
    return do_fork(0, regs->oldesp, regs, 0);
}

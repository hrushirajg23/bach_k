#include "serial.h"
#include "task.h"
#include "mm.h"
#include "zone.h"
#include "slab.h"

#define KERNEL_DATA 0x10
#define STACK_SIZE PAGE_SIZE
#define KSTACK_ORDER 1   // 8KB stack
#define TSS_ENTRY 5

#define MAX_TASKS 32 //hence we'll use a single integer in prio_array_t data 
                     //structure as a bitmap to denote all tasks

int last_pid = 0;
kmem_cache_t *cache_task_struct = NULL;

struct tss_struct tss;

union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task), *last_task_used_math = NULL;

struct task_struct * process_table[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;

struct {
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

void init_process_subsystem(void)
{
    int iCnt = 0;
    for (iCnt = 0; iCnt < NR_TASKS; iCnt++){
        process_table[iCnt] = NULL;
    }
    //set the tss descriptor


}

void fork(void)
{
    
}

void *alloc_kernel_stack(void)
{
    struct page *p = alloc_pages(0, KSTACK_ORDER);
    if (!p)
        return NULL;

    void *addr = page_address(p);
    printk("stack address will be %p and esp is %p\n", addr, addr + (PAGE_SIZE << KSTACK_ORDER));
    return page_address(p) + (PAGE_SIZE << KSTACK_ORDER);
}


int find_empty_process(void)
{
    int iCnt = last_pid;
    for (; iCnt < NR_TASKS; iCnt++){
        if (process_table[iCnt] == NULL){
            return iCnt;
        }
    }
    return -1;
}

void math_state_restore()
{
	/* if (last_task_used_math) */
	/* 	__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387)); */
	/* if (current->used_math) */
	/* 	__asm__("frstor %0"::"m" (current->tss.i387)); */
	/* else { */
	/* 	__asm__("fninit"::); */
	/* 	current->used_math=1; */
	/* } */
	last_task_used_math=current;
}

void sched_init(void)
{
    printk("initializing scheduler \n");
    memset(&tss, 0, sizeof(tss));
    tss.esp0 = (uint32_t)alloc_kernel_stack();
    tss.ss0  = KERNEL_DATA;
    tss.iopb = 0;

    printk("kernel stack points to %x\n", tss.esp0);
   

}



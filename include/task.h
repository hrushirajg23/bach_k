#ifndef _TASK_H
#define _TASK_H

#include "list.h"
#include "vfs.h"
// #include "vfs.h"

#define NR_TASKS 10
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
    uint32_t esp0;  //kernel stack pointer
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

struct task_struct {
    long state; 
    long priority;
    long signal; //signals sent to the process but not yet handled
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
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */

	// struct desc_struct ldt[3];
/* tss for this task */
	// struct tss_struct tss;
};
#define INIT_TASK { \
    .state = 0, \
    .priority = 15, \
    .signal = 0, \
    .sig_restorer = NULL, \
    .sig_fn = { [0 ... 31] = NULL }, \
    .exit_code = 0, \
    .pprt = { 0 }, \
    .pid = 0, \
    .father = -1, \
    .pgrp = 0, \
    .session = 0, \
    .leader = 0, \
    .uid = 0, .euid = 0, .suid = 0, \
    .gid = 0, .egid = 0, .sgid = 0, \
    .alarm = 0, \
    .utime = 0, .stime = 0, .cutime = 0, .cstime = 0, .start_time = 0, \
    .used_math = 0,\
    .pwd = NULL, \
    .root = NULL, \
    .filp = 0, \
    .first_free_filp = 0 \
}

// /* fs info */	-1,0133,NULL,NULL,0, \
// /* filp */	{NULL,}, \
// 	{ \
// 		{0,0}, \

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
    
void tss_load(int offset);
void sched_init(void);

#endif

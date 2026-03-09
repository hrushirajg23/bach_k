/**
 *
 * All unimplemented syscalls point to sys_null which returns -1 (ENOSYS).
 * Wire real implementations in as they are written.
 * generated this file via ai
 */

#include "task.h"   /* fn_ptr typedef */

int sys_null(void) { return -1; }

/* Implemented syscalls -------------------------------------------------- */
extern int sys_fork(struct pt_regs *regs);  

#define SYS(fn)  ((fn_ptr)(fn))
#define NUL      ((fn_ptr)sys_null)

fn_ptr sys_call_table[] = {
    /* 0  */ NUL,           /* sys_setup   (not needed in this kernel) */
    /* 1  */ NUL,           /* sys_exit    */
    /* 2  */ SYS(sys_fork), /* sys_fork    ← implemented */
    /* 3  */ NUL,           /* sys_read    */
    /* 4  */ NUL,           /* sys_write   */
    /* 5  */ NUL,           /* sys_open    */
    /* 6  */ NUL,           /* sys_close   */
    /* 7  */ NUL, NUL, NUL, /* 7-9         */
    /* 10 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 10-19 */
    /* 20 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 20-29 */
    /* 30 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 30-39 */
    /* 40 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 40-49 */
    /* 50 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 50-59 */
    /* 60 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 60-69 */
    /* 70 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 70-79 */
    /* 80 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 80-89 */
    /* 90 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, /* 90-99 */
    /* 100-109 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,
    /* 110-119 */ NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL, NUL,
    /* 120-319 (fill remainder) */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 120-129 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 130-139 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 140-149 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 150-159 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 160-169 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 170-179 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 180-189 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 190-199 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 200-209 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 210-219 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 220-229 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 230-239 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 240-249 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 250-259 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 260-269 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 270-279 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 280-289 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 290-299 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 300-309 */
    NUL,NUL,NUL,NUL,NUL, NUL,NUL,NUL,NUL,NUL, /* 310-319 */
};

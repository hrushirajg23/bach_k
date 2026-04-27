#ifndef _BUFFER_H
#define _BUFFER_H

#include "list.h"
#include "vfs.h"
#include "fs.h"

#define hash_fn(bno, devno) (bno % devno)


/* BUFFER_CACHE
 * ======================================================================================
 */

#define BH_lock 1 << 0
#define BH_dirty 1 << 1
#define BH_uptodate 1 << 2
#define BH_delay 1 << 3
#define BH_old 1 << 5


struct buffer_head{
    unsigned int flags;
    unsigned int b_count; //buffer ref count
    char* b_data; //ptr to data //1024 bytes
    unsigned short b_dev; //if ==0 means free
    unsigned long b_blocknr; //block number
    struct list_head b_hash;
    struct list_head b_free;
};

struct buffer_cache {
    struct list_head b_hash[DEV_NO]; //the buffer hashmap
    struct list_head b_free; //the freelist
};


/* buffer cache operations */
void create_buffer_cache(void);
struct buffer_head *bread(unsigned short dev_no, unsigned long blocknr);
void bwrite(struct buffer_head *bh);
void brelse(struct buffer_head *bh);
struct buffer_head *getblk(unsigned short dev_no, unsigned long blocknr);
void create_buffer_cache(void);

#endif

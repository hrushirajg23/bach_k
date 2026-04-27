#include "slab.h"
#include "mm.h"
#include "fs.h"
#include "serial.h"
#include "disk.h"
#include "buffer.h"

#define BUFFERS 100
#define BUFFER_SIZE 1024
kmem_cache_t *bcache;
struct buffer_cache buffer_cache;

struct buffer_head *search_hash(unsigned short dev_no, unsigned long blocknr);

void test_bcache(void);

void binit(struct buffer_head *bhead, unsigned short b_dev, unsigned long blocknr)
{
    bhead->flags = 0;
    bhead->b_data = kmalloc(BUFFER_SIZE, 0);
    if (!bhead->b_data) {
        printk("failed buffer_head kmalloc\n");
        return;
    }
    bhead->b_dev = b_dev;
    bhead->b_blocknr = blocknr;
    bhead->b_count = 0;
    INIT_LIST_NULL(&bhead->b_hash);
    INIT_LIST_NULL(&bhead->b_free);
}

void display_buffer_cache(void) 
{
    int iCnt = 0;
    struct list_head *run;
    struct buffer_head *bh;

    for (; iCnt < DEV_NO; iCnt++) {
        printk("================ b_hash %d=================\n", iCnt);
        list_for_each(run, &buffer_cache.b_hash[iCnt]) {
            bh = list_entry(run, struct buffer_head, b_hash);
            printk("<= %d => ", bh->b_blocknr);
        }
        printk("\n");
    }

    printk("================ b_free %d=================\n", iCnt);
    list_for_each(run, &buffer_cache.b_free) {
        bh = list_entry(run, struct buffer_head, b_free);
        
        printk("<= %u => ", bh->b_blocknr);
    }
    printk("\n");

}

void create_buffer_cache(void) 
{
    bcache = kmem_cache_create("buffer_head", sizeof(struct buffer_head), KMALLOC_MINALIGN, SLAB_HWCACHE_ALIGN, NULL);
    if (!bcache) {
        printk("failed kmem_cache_create for buffer_head\n");
        return;
    }


    int iCnt = 0;
    for (; iCnt < DEV_NO; iCnt++) {
        INIT_LIST_HEAD(&buffer_cache.b_hash[iCnt]);
    }
    INIT_LIST_HEAD(&buffer_cache.b_free);

    struct buffer_head *tmp = NULL;
    for (iCnt = 0; iCnt < BUFFERS; iCnt++) {
        tmp = kmem_cache_alloc(bcache, 0); 
        if (!tmp) {
            printk("failed allocating buffer_head for %d\n", iCnt);
            break;
        }
       
        binit(tmp, DEV_NO, iCnt);
        
        /*
         * add to correct hash list and also to freelist */
        list_add(&buffer_cache.b_hash[hash_fn(iCnt, DEV_NO)], &tmp->b_hash);
        list_add(&buffer_cache.b_free, &tmp->b_free);
    }
    
    printk("displaying buffer cache................\n");
    display_buffer_cache();
    /* test_bcache(); */

}

struct buffer_head *search_hash(unsigned short dev_no, unsigned long blocknr)
{
    struct buffer_head *bh = NULL;
    struct list_head *run;
    int index = hash_fn(blocknr, dev_no);

    list_for_each(run, &buffer_cache.b_hash[index]) {
        bh = list_entry(run, struct buffer_head, b_hash);
        if (bh->b_blocknr == blocknr && bh->b_dev == dev_no) {
            return bh;
        }
    }
    return NULL;
}

static inline struct buffer_head *locked_buffer(struct buffer_head *bh)
{
    SET_FLAG(bh->flags, BH_lock);
    return bh;
}

static inline struct buffer_head *unlocked_buffer(struct buffer_head *bh)
{
    CLEAR_FLAG(bh->flags, BH_lock);
    return bh;
}
struct buffer_head *getblk(unsigned short dev_no, unsigned long blocknr)
{
    struct buffer_head *bh = NULL;
    /* printk("inside getblk \n"); */

    while (1) {
        bh = search_hash(dev_no, blocknr);
        if (bh) {
            /* printk("getblk hash found \n"); */
            if (IS_FLAG(bh->flags, BH_lock)) { //scenario 5
                //sleep
                printk("getblk scenario 5\n");
                continue;
            }
            /* printk("getblk scenario 1\n"); */
            list_del(&bh->b_free); //remove from the free list
            return locked_buffer(bh);
        }
        else { //block is not on hash queue

            /* printk("block not on hash queue\n"); */
            if (list_is_empty(&buffer_cache.b_free)) { //scenario 4
                //sleep till any buffer doesn't become free
                printk("BUFFER LIST IS EMPTY ===========================\n");
                /* printk("getblk scenario 4\n"); */
                continue; //to avoid race conditions 
            }
            /*
             * remove the first free buffer from free list */

            bh = list_first_entry(&buffer_cache.b_free, struct buffer_head, b_free);
            list_del(&bh->b_free);

            if (IS_FLAG(bh->flags, BH_delay)) { //scenario 3 marked for delayed write
                //async write to disk
                //For async write ig we should use interrupt driven i/o
                //put the write block in queue, it gets scheduled accordingly
                //and raises an interrupt when completed
                /* printk("getblk scenario 3\n"); */
                disk_write_blk(bh->b_blocknr, bh->b_data);
                CLEAR_FLAG(bh->flags, BH_delay);
                list_add(&buffer_cache.b_free, &bh->b_free);
                continue; 
            }

            /* printk("getblk scenario 2\n"); */

            //scenario 2 -- found a free buffer, use it 
            //remove the buffer from the old hash queue

            list_del(&bh->b_hash);

            bh->b_dev = dev_no;
            bh->b_blocknr = blocknr;
            bh->flags = 0;

            list_add(&buffer_cache.b_hash[hash_fn(blocknr, dev_no)], &bh->b_hash);
            
            return locked_buffer(bh);
        }
    }
    return NULL;
}

void brelse(struct buffer_head *bh)
{
    /*
     * 1 wakeup all procs: event, waiting for any buffer to become free
     * 2 wakeup all procs: event, waiting for this buffer to become free
     *
     * raise processor execution level to block interrupts
     *
     */
    asm volatile ("cli");

    if ( !IS_FLAG(bh->flags, BH_dirty) && !IS_FLAG(bh->flags, BH_old)) {
        /* buffer is valid and not old */
        list_add_tail(&buffer_cache.b_free, &bh->b_free);
    }
    else {
        /* buffer is dirty */
        list_add(&buffer_cache.b_free, &bh->b_free);
    }

    asm volatile ("sti");
    unlocked_buffer(bh);
}

struct buffer_head *bread(unsigned short dev_no, unsigned long blocknr)
{
    struct buffer_head *bh = getblk(dev_no, blocknr);
    
    if (IS_FLAG(bh->flags, BH_uptodate)) {
        unlocked_buffer(bh);
        return bh;
    }

    if (disk_read_blk(bh->b_blocknr, bh->b_data) != 0) {
        /* printk("bread: disk_read_blk failed for block %u\n", blocknr); */
        brelse(bh);
        return NULL;
    }

    SET_FLAG(bh->flags, BH_uptodate);
    CLEAR_FLAG(bh->flags, BH_dirty);
    unlocked_buffer(bh);

    return bh;
}


void bwrite(struct buffer_head *bh)
{
    /* printk("invoed bwrite \n"); */
    locked_buffer(bh);
    disk_write_blk(bh->b_blocknr, (uint8_t *)bh->b_data);
    CLEAR_FLAG(bh->flags, BH_dirty);
    unlocked_buffer(bh);
    /*
     * if I/O is synchronous 
     *      sleep(event I/O completes)
     *      brelse
     * else if buffer is marked for delayed write
     *      mark buffer to put at head of list 
     *      
     *      I guess this is where its marked old
     *
     */
    brelse(bh);
}

void test_disk_block(void)
{
    uint8_t buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    disk_read(16, buffer);
    printk("disk content %s\n", buffer);

}
void test_bcache(void)
{
    printk("testing buffer cache .............\n");
    struct buffer_head *bh = getblk(DEV_NO, 16);
    if (!bh) {
        printk(" getblk failed \n");
        return ;
    }

    
    for (int iCnt = 0; iCnt < BUFFER_SIZE; iCnt++ )
    {
        bh->b_data[iCnt] = iCnt / 256 + 'a';
    }
    printk("invoking bwrite\n");

    bwrite(bh);
    
    printk("bwrite completed\n");

    memset(bh->b_data, 0, BUFFER_SIZE);

    struct buffer_head *tmp = NULL;

    printk("invoking bread\n");
    tmp = bread(DEV_NO, 16); 

    printk("bread completed\n");
    printk("buffer content %s\n", tmp->b_data);

    brelse(tmp);

    /* test_disk_block(); */
}


#include "fs.h"
#include "slab.h"
#include "mm.h"
#include "disk.h"
#include "serial.h"
#include "vfs.h"
#include "list.h"
#include "kernel.h"
#include "buffer.h"
#include <stdint.h>

#define INODES 30

kmem_cache_t *inode_cache;
kmem_cache_t *file_cache; //for file table entries

struct inode_cache {
    struct list_head i_hash[DEV_NO];
    struct list_head i_free;
}i_cache;

void display_inode_cache(void) 
{
    int iCnt = 0;
    for (; iCnt < DEV_NO; iCnt++) {
        struct list_head *run;
        struct inode *inode;
        printk("================ i_hash %d=================\n", iCnt);
        list_for_each(run, &i_cache.i_hash[iCnt]) {
            inode = list_entry(run, struct inode, i_hash);
            printk("<= %d => ", inode->i_no);
        }
        printk("\n");
    }
}
static struct inode *search_hash(unsigned short dev_no, unsigned long inum)
{
    struct inode *inode = NULL;
    struct list_head *run;
    int index = hash_fn(inum, dev_no);

    list_for_each(run, &i_cache.i_hash[index]) {
        inode = list_entry(run, struct inode, i_hash);
        if (inode->i_no == inum && inode->i_dev == dev_no) {
            printk("found inode in hash\n");
            return inode;
        }
    }
    printk("search_hash failed\n");
    return NULL;
}


void inode_init(struct inode *inode, unsigned short dev_no, unsigned long inum)
{
    INIT_LIST_NULL(&inode->i_hash);
    INIT_LIST_NULL(&inode->i_sb_list);
    INIT_LIST_NULL(&inode->i_dentry);
    inode->i_no = inum;
    inode->i_dev = dev_no;
    inode->i_count = 0;
    inode->i_mode = 0;
    inode->i_nlinks = 0;
    inode->i_uid = 0;
    inode->i_gid = 0;
    inode->i_flags = 0;
    inode->i_size = 0;
    inode->i_atime = 0;
    inode->i_mtime = 0;
    inode->i_ctime = 0;
    inode->i_blocks = 0;
    inode->i_op = NULL;
    inode->f_op = NULL;
    inode->i_sb = NULL;
}

void test_icache(void)
{
    printk("testing inode cache .............\n");
    struct inode *inode = iget(DEV_NO, 16);
    if (!inode) {
        printk(" iget failed \n");
        return ;
    }
    printk("iget passed inode->i_no : %d\n", inode->i_no);
   
}

void create_file_cache(void)
{
    file_cache = kmem_cache_create("file", sizeof(struct file), KMALLOC_MINALIGN, SLAB_HWCACHE_ALIGN, NULL);
    if (!file_cache) {
        printk("failed kmem_cache_create for file_cache \n");
        return;
    }
}
void create_inode_cache(void)
{

    int iCnt = 0;
    struct inode *tmp = NULL;

    inode_cache = kmem_cache_create("inode", sizeof(struct inode), KMALLOC_MINALIGN, SLAB_HWCACHE_ALIGN, NULL);
    if (!inode_cache) {
        printk("failed kmem_cache_create for inode\n");
        return;
    }

    create_file_cache();

    for (iCnt = 0; iCnt < DEV_NO; iCnt++) {
        INIT_LIST_HEAD(&i_cache.i_hash[iCnt]);
    }
    INIT_LIST_HEAD(&i_cache.i_free);

    for (iCnt = 0; iCnt < INODES; iCnt++) {
        tmp = kmem_cache_alloc(inode_cache, 0); 
        if (!tmp) {
            printk("failed allocating inode for %d\n", iCnt);
            break;
        }
       
        inode_init(tmp, DEV_NO, iCnt);
        /*
         * add to correct hash list and also to freelist */
        list_add(&i_cache.i_hash[hash_fn(iCnt, DEV_NO)], &tmp->i_hash);
        list_add(&i_cache.i_free, &tmp->i_free);
    }

    printk("size of inode is %d\n", sizeof(struct inode));
    printk("displaying inode cache................\n");
    display_inode_cache();
    test_icache();
}

struct inode *iget(unsigned short dev_no, unsigned int inum)
{
    struct inode *inode = NULL;
    while (1) {
        inode = search_hash(dev_no, inum); 
        if (inode) {
            if (IS_FLAG(inode->i_flags, I_lock)) {
                /*
                 * sleep (event this inode becomes free )
                 */
                continue;
            }
            if (inode->i_count == 0) { //inode is on free list
                list_del(&inode->i_free);  
            }
            inode->i_count++;
            return locked_inode(inode);
        }

        if (list_is_empty(&i_cache.i_free)) {
            return NULL;
        }

        list_del(&inode->i_free);
        inode->i_no = inum;
        /* ino->dev_no = dev_no; */
        list_del(&inode->i_hash);
        list_add(&i_cache.i_hash[hash_fn(dev_no, inode->i_no)], &inode->i_hash); 
        //read inode from disk via bread at core 
        /* ext2_read_inode(inode); */ 
        inode->i_sb->s_op->read_inode(inode);
        inode->i_count++; 

        return locked_inode(inode);
    }
    return NULL;
}

void iput(struct inode *inode)
{
    inode = locked_inode(inode);
    inode->i_count--;

    if (inode->i_count == 0) {
        /* if (inode->i_nlinks == 0) { */
        /*     //free disk blocks for file ( bach ch4 Disk Blocks ) */
        /*     //set file type to 0 */
        /*     //free inode (ifree) */
        /* } */
        if (IS_FLAG(inode->i_state, I_DIRTY)) {
           inode->i_sb->s_op->write_inode(inode); //update the disk inode 
        }
    }
    list_add_tail(&i_cache.i_free, &inode->i_free);

    unlocked_inode(inode);
}


/* VFS Operation Wrappers */

int vfs_create(struct inode *dir, struct dentry *dentry, int mode)
{
    if (!dir || !dentry) {
        return -1;
    }
    
    if (!dir->i_op || !dir->i_op->create) {
        printk("vfs_create: no create operation\n");
        return -1;
    }
    
    return dir->i_op->create(dir, dentry, mode);
}

int vfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
    if (!dir || !dentry) {
        return -1;
    }
    
    if (!dir->i_op || !dir->i_op->mkdir) {
        printk("vfs_mkdir: no mkdir operation\n");
        return -1;
    }
    
    return dir->i_op->mkdir(dir, dentry, mode);
}

int vfs_rmdir(struct inode *dir, struct dentry *dentry)
{
    if (!dir || !dentry) {
        return -1;
    }
    
    if (!dir->i_op || !dir->i_op->rmdir) {
        printk("vfs_rmdir: no rmdir operation\n");
        return -1;
    }
    
    return dir->i_op->rmdir(dir, dentry);
}

int vfs_unlink(struct inode *dir, struct dentry *dentry)
{
    if (!dir || !dentry) {
        return -1;
    }
    
    /* For now, use rmdir - we'll add proper unlink later */
    if (!dir->i_op || !dir->i_op->rmdir) {
        printk("vfs_unlink: no unlink operation\n");
        return -1;
    }
    
    return dir->i_op->rmdir(dir, dentry);
}

int vfs_rename(struct inode *old_dir, struct dentry *old_dentry,
               struct inode *new_dir, struct dentry *new_dentry)
{
    if (!old_dir || !old_dentry || !new_dir || !new_dentry) {
        return -1;
    }
    
    if (!old_dir->i_op || !old_dir->i_op->rename) {
        printk("vfs_rename: no rename operation\n");
        return -1;
    }
    
    return old_dir->i_op->rename(old_dir, old_dentry, new_dir, new_dentry);
}

ssize_t vfs_read(struct file *file, char *buf, size_t count, loff_t *pos)
{
    if (!file || !buf) {
        return -1;
    }
    
    if (!file->f_op || !file->f_op->read) {
        printk("vfs_read: no read operation\n");
        return -1;
    }
    
    return file->f_op->read(file, buf, count, pos);
}

ssize_t vfs_write(struct file *file, const char *buf, size_t count, loff_t *pos)
{
    if (!file || !buf) {
        return -1;
    }
    
    if (!file->f_op || !file->f_op->write) {
        printk("vfs_write: no write operation\n");
        return -1;
    }
    
    return file->f_op->write(file, buf, count, pos);
}

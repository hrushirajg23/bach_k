#include "fs.h"
#include "vfs.h"
#include "dcache.h"
#include "serial.h"
#include "string.h"
#include "slab.h"
#include "namei.h"
#include "task.h"
#include "buffer.h"

/* Forward declarations */
static ssize_t ext2_file_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos);
static ssize_t ext2_file_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos);
static int ext2_file_open(struct inode *inode, struct file *filp);
static int ext2_file_release(struct inode *inode, struct file *filp);
static loff_t ext2_llseek(struct file *file, loff_t offset, int whence);

extern struct task_struct *current;

/* ext2 file operations */
struct file_operations ext2_file_operations = {
    .owner = NULL,
    .llseek = ext2_llseek,
    .read = ext2_file_read,
    .write = ext2_file_write,
    .open = ext2_file_open,
    .release = ext2_file_release,
    .fsync = NULL,
};

extern kmem_cache_t *file_cache;

/* File open operation */
static int ext2_file_open(struct inode *inode, struct file *filp)
{
    if (!inode || !filp) {
        return -1;
    }
    
    /* Set file operations */
    filp->f_op = &ext2_file_operations;
    
    return 0;
}

/* File release operation */
static int ext2_file_release(struct inode *inode, struct file *filp)
{
    /* Nothing special to do */
    return 0;
}

/* File seek operation */
static loff_t ext2_llseek(struct file *file, loff_t offset, int whence)
{
    loff_t new_pos;
    
    if (!file || !file->f_dentry || !file->f_dentry->d_inode) {
        return -1;
    }
    
    struct inode *inode = file->f_dentry->d_inode;
    
    switch (whence) {
        case 0: /* SEEK_SET */
            new_pos = offset;
            break;
        case 1: /* SEEK_CUR */
            new_pos = file->f_pos + offset;
            break;
        case 2: /* SEEK_END */
            new_pos = inode->i_size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_pos < 0) {
        return -1;
    }
    
    file->f_pos = new_pos;
    return new_pos;
}


/* Read from file */
static ssize_t ext2_file_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode;
    loff_t pos;
    size_t read_bytes = 0;
    uint32_t block_size = EXT2_BLK_SIZE;
    
    if (!filp || !buf || !ppos) {
        return -1;
    }
    
    inode = filp->f_dentry->d_inode;
    if (!inode) {
        return -1;
    }
    
    pos = *ppos;
    
    /* Don't read past end of file */
    if (pos >= inode->i_size) {
        return 0;
    }
    
    if (pos + count > inode->i_size) {
        count = inode->i_size - pos;
    }
    
    /* Read data block by block */
    while (read_bytes < count) {
        uint32_t block_idx = pos / block_size;
        uint32_t block_offset = pos % block_size;
        uint32_t bytes_to_read = block_size - block_offset;
        
        if (bytes_to_read > count - read_bytes) {
            bytes_to_read = count - read_bytes;
        }
        
        /* Get block number for this file offset */
        uint32_t block_num = indirect_block(*(inode_t*)inode, block_idx);
        if (block_num == 0) {
            break;  /* Sparse file or error */
        }
        
        /* Read block using buffer cache */
        struct buffer_head *bh = bread(inode->i_dev, block_num);
        if (!bh) {
            break;
        }
        
        /* Copy data to user buffer */
        memcpy(buf + read_bytes, bh->b_data + block_offset, bytes_to_read);
        
        brelse(bh);
        
        read_bytes += bytes_to_read;
        pos += bytes_to_read;
    }
    
    *ppos = pos;
    return read_bytes;
}

/* Write to file */
static ssize_t ext2_file_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    struct inode *inode;
    loff_t pos;
    size_t written_bytes = 0;
    uint32_t block_size = EXT2_BLK_SIZE;
    
    if (!filp || !buf || !ppos) {
        return -1;
    }
    
    inode = filp->f_dentry->d_inode;
    if (!inode) {
        return -1;
    }
    
    pos = *ppos;
    
    /* Write data block by block */
    while (written_bytes < count) {
        uint32_t block_idx = pos / block_size;
        uint32_t block_offset = pos % block_size;
        uint32_t bytes_to_write = block_size - block_offset;
        
        if (bytes_to_write > count - written_bytes) {
            bytes_to_write = count - written_bytes;
        }
        
        /* Get or allocate block for this file offset */
        uint32_t block_num = indirect_block(*(inode_t*)inode, block_idx);
        
        if (block_num == 0) {
            /* Need to allocate a new block */
            if (reserve_free_block(&block_num) != 0) {
                break;  /* No free blocks */
            }
            
            /* Update inode with new block */
            /* This is simplified - in reality we'd need to update the inode properly */
        }
        
        /* Read block using buffer cache */
        struct buffer_head *bh = bread(inode->i_dev, block_num);
        if (!bh) {
            break;
        }
        
        /* Copy data from user buffer */
        memcpy(bh->b_data + block_offset, buf + written_bytes, bytes_to_write);
        
        /* Mark buffer as dirty and write back */
        SET_FLAG(bh->flags, BH_dirty);
        bwrite(bh);
        
        written_bytes += bytes_to_write;
        pos += bytes_to_write;
    }
    
    /* Update file size if we wrote past the end */
    if (pos > inode->i_size) {
        inode->i_size = pos;
        /* Mark inode as dirty - would need to write back to disk */
    }
    
    *ppos = pos;
    return written_bytes;
}

void init_file_entry(struct file *file)
{
    file->f_count++;
    file->f_pos = 0;
}

int open(const char *pathname, int flags, int mode)
{
    /*
     * call namei 
     */
    int iRet = 0;
    struct nameidata nd;
    struct file *file;
    iRet = path_lookup(pathname, 0, &nd);
    if (iRet == -1) {
        printk("open call failed\n");
        return iRet;
    }

    file = kmem_cache_zalloc(file_cache, 0);
    if (!file) {
        printk("failed to allocate file table entry \n");
        return -1;
    }
    init_file_entry(file);

    /* ext2_file_open(*inode, file) */

    current->filp[current->first_free_filp++] = file;



}


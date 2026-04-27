#ifndef _UFS_H
#define _UFS_H

#include "hardware.h"
#include "disk.h"
#include "slab.h"
#include "mm.h"
#include "system.h"
#include "kernel.h"
#include "list.h"
#include "string.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_BITMAP_BLOCKS 10
#define U_BLK_SIZE 1024
#define MAX_DISK_BLOCKS 10

//BLOCK size must be completely divisible by DIR_ENTRY_BYTES 
#define DIR_ENTRY_BYTES 32
#define DIR_ENTRY_INODE_SIZE 4
#define DIR_ENTRY_NAME_SIZE (DIR_ENTRY_BYTES - DIR_ENTRY_INODE_SIZE)

#define FREE_INODE_LIST 50

#define DILB_START 2 //block from where the dilb starts
#define DILB_END 11 //last block of dilb

#define LOCKED(var) ((var == 1 ? 1 : 0))
#define LOCK(var) (*var = 1)
#define UNLOCK(var) (*var = 0)

#define DEV_NO 4

#define INODE_MEMBER_BLOCKS 15
#define NR_OPEN 30

#define SUPER_DIRTY 0 << 1
#define SUPER_MODIFIED 0 << 2

#define FREE_BLOCK_LIST 128

struct ufs_super_block {
    short s_fs_size;
    short s_flags; //super block flags, like modified, synced, dirty , etc 

    /* inode part */
    uint32_t s_inode_list_size;
    unsigned long s_total_inodes;
    unsigned long s_n_free_inodes; //number of free inodes in the file sys
    uint32_t s_free_inodes[FREE_INODE_LIST]; //the free inode list (though it is an array)
    int s_next_free_inode; //index of the next free inode in the list
                           //this should be reverse, when it becomes -1
                           //means list has become empty

    unsigned long s_remembered_inode;
    char s_inode_list_lock; //lock for free inode list

    int s_blk_dilb_start; //dilb_start block
    int s_blk_dilb_end; //dilb_end block

    /* block part */ 
    char s_block_list_lock; //lock for free block list
    uint32_t s_n_free_blocks; //total number of free blocks on fs
    uint32_t s_free_blocks[FREE_BLOCK_LIST];                            
    int s_next_free_block; //index of the next free block in the list

};

typedef struct ufs_super_block s_ufs;

// 128 bytes
struct d_inode {
    uint16_t i_mode; //file type and access rights
    uint16_t i_uid; //owner identifier
    loff_t i_size; //file length in bytes
    uint32_t i_atime; //time of last file access
    uint32_t i_ctime; //time when inode was last changed
    uint32_t i_mtime; //time when file content was changed
    uint32_t i_dtime; //time of file deletion
    uint16_t i_gid;     //user group identifier
    uint16_t i_nlinks; //hard links counter
    uint32_t i_n_blocks; //number of data blocks of file
    uint32_t i_flags;   //file flags
    uint32_t i_dev;        // os specific info
    uint32_t i_blocks[INODE_MEMBER_BLOCKS];   //pointer to data blocks, 13 member array of bach
    uint32_t i_no;  
    uint32_t i_blkno;    //the disk block in which this inode is stored
    uint32_t i_dir_acl;     //directory access control list
    uint32_t i_pino;       //parent inode number
    uint8_t i_osd2[12];     //os specific info
};


enum inode_state {
    I_LOCKED        = 1 << 0,  /* inode is locked */
    I_WAITING       = 1 << 1,  /* processes waiting on inode lock */
    I_DIRTY_INODE   = 1 << 2,  /* inode metadata differs from disk */
    I_DIRTY_DATA    = 1 << 3,  /* file data differs from disk */
    I_MOUNT         = 1 << 4,  /* inode is a mount point */
};

struct inode {
    /* similar to d_inode 
     * do not touch */
    uint16_t i_mode; //file type and access rights
    uint16_t i_uid; //owner identifier
    loff_t i_size; //file length in bytes
    uint32_t i_atime; //time of last file access
    uint32_t i_ctime; //time when inode was last changed
    uint32_t i_mtime; //time when file content was changed
    uint32_t i_dtime; //time of file deletion
    uint16_t i_gid;     //user group identifier
    uint16_t i_nlinks; //hard links counter
    uint32_t i_n_blocks; //number of data blocks of file
    uint32_t i_flags;   //file flags
    uint32_t i_dev;        // os specific info
    uint32_t i_blocks[INODE_MEMBER_BLOCKS];   //pointer to data blocks, 13 member array of bach
    uint32_t i_no; //inode number  
    uint32_t i_file_acl;    //file access control list
    uint32_t i_dir_acl;     //directory access control list
    uint32_t i_pino;       //parent inode number
    uint8_t i_osd2[12];     //os specific info

    /* in-core inode starts */
    struct list_head i_hash;
    struct list_head i_free;
    uint16_t i_state; //one of the contents from inode_state enum
    uint32_t i_count; //reference count
    struct inode *i_parent;
};

enum open_flags {
   O_CREAT = 1 << 1,
   O_RDONLY = 1 << 2,
   O_WRONLY = 1 << 3,
   O_TRUNC = 1 << 4,
   O_DIR = 1 << 5,
};

enum namei_modes {
    N_PARENT = 1 << 1,
};

struct file {
    struct inode *f_inode;
    struct list_head f_list; //pointers for generic file object list
    unsigned int f_count; //file object's reference counter
    unsigned int f_flags; //flags specified when opening the file 
    unsigned short f_mode; //process access mode
    loff_t f_pos; //current file offset
};

struct dir_entry {
    unsigned long dir_inode;
    char dir_name[DIR_ENTRY_NAME_SIZE];
};


#define INCORE_TABLE 50 //incore inode table size
struct incore_table {
    struct inode **table;
    int size;
};

static inline struct inode *locked_inode(struct inode *inode) {
    SET_FLAG(inode->i_flags, I_LOCKED);
    return inode;
}
 
static inline struct inode *unlocked_inode(struct inode *inode) {
    CLEAR_FLAG(inode->i_flags, I_LOCKED);
    return inode;
}

void create_inode_cache(void);
struct inode *iget(unsigned short dev_no, unsigned int inum);
void iput(struct inode *inode);
void mkufs(int mb);
void init_fs(void);
void test_fs();
int mount_rootfs(void);
int sync();



#endif

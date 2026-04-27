#ifndef _VFS_H
#define _VFS_H

#include "system.h"
#include "kernel.h"
#include "list.h"
#include "mm.h"
#include "string.h"
#include <stddef.h>
#include <stdbool.h>

#ifndef __user
#define __user
#endif

#define NR_OPEN 30 //process max file open count
/*
 * vfs data structures-----------------------------------------------------------------
 */

/* Filesystem type registration */
struct file_system_type {
    const char *name;
    int fs_flags;
    struct super_block *(*read_super)(struct super_block *, void *, int);
    struct list_head fs_supers;  /* list of superblocks */
    struct list_head fs_list;    /* list in filesystem registry */
};

/* Mount point */
struct vfsmount {
    struct dentry *mnt_root;      /* root dentry of this mount */
    struct super_block *mnt_sb;   /* superblock */
    int mnt_flags;
    struct vfsmount *mnt_parent;  /* parent mount */
};

/* Forward declarations */
struct file;

struct dentry {
    unsigned int d_count; //refernce counter
    unsigned int d_flags;
    struct inode *d_inode; //inode associated with the filename
    struct dentry *d_parent; //parent object    
    char d_name[30]; //directory name
    
    struct list_head d_lru; //pointer to unused dentries in the cache
    struct list_head d_child; //for directories , pointers for the list of 
                              //directories dentries in the same parent directory
    
    struct list_head d_subdirs; //for directories, head of list of subdirs
    struct dentry_operations *d_op;
    struct super_block *sb;
    
    struct list_head d_hash; //node for entry in hash of dentyr cache
};

struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    // ssize_t (*aio_read) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    // ssize_t (*aio_write) (struct kiocb *, const struct iovec *, unsigned long, loff_t);
    // int (*readdir) (struct file *, void *, filldir_t);
    // int (*mmap) (struct file *, struct vm_area_struct *);
    int (*open) (struct inode *, struct file *);
    // int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, int datasync);
};

struct dentry_operations {
    // int (*d_revalidate)(struct dentry *, struct nameidata *);
    int (*d_hash)(const struct dentry *, const struct inode *,
            const char *);
    int (*d_compare)(const struct dentry *, const struct inode *,
            const struct dentry *, const struct inode *,
            unsigned int, const char *, const char *str);
    int (*d_delete)(const struct dentry *);
    void (*d_release)(struct dentry *);
    void (*d_iput)(struct dentry *, struct inode *);
    char *(*d_dname)(struct dentry *, char *, int);
    // struct vfsmount *(*d_automount)(struct path *);
    int (*d_manage)(struct dentry *, bool);
} ____cacheline_aligned;


struct inode_operations {
    struct dentry * (*lookup) (struct inode *,struct dentry *);
    void * (*follow_link) (struct dentry *);
    int (*permission) (struct inode *, int, unsigned int);
    // int (*check_acl)(struct inode *, int, unsigned int);

    // int (*readlink) (struct dentry *, char __user *,int);
    void (*put_link) (struct dentry *, void *);

    int (*create) (struct inode *,struct dentry *,int);
    int (*link) (struct dentry *,struct inode *,struct dentry *);
    // int (*unlink) (struct inode *,struct dentry *);
    // int (*symlink) (struct inode *,struct dentry *,const char *);
    int (*mkdir) (struct inode *,struct dentry *, int);
    int (*rmdir) (struct inode *,struct dentry *);
    // int (*mknod) (struct inode *,struct dentry *,int,dev_t);
    int (*rename) (struct inode *, struct dentry *,
            struct inode *, struct dentry *);
    void (*truncate) (struct inode *);
    // int (*setattr) (struct dentry *, struct iattr *);
    // int (*getattr) (struct vfsmount *mnt, struct dentry *, struct kstat *);
    // int (*setxattr) (struct dentry *, const char *,const void *,size_t,int);
    // ssize_t (*getxattr) (struct dentry *, const char *, void *, size_t);
    // ssize_t (*listxattr) (struct dentry *, char *, size_t);
    // int (*removexattr) (struct dentry *, const char *);
    void (*truncate_range)(struct inode *, loff_t, loff_t);
    // int (*fiemap)(struct inode *, struct fiemap_extent_info *, u64 start,
              // u64 len);
} ____cacheline_aligned;


struct super_block {
    struct list_head s_list; //super block list
    unsigned char s_dev;
    unsigned long s_blocksize; //block size in bytes
    unsigned char s_dirt;      //has been modified
    struct super_operations *s_op; //superblock methods that will point to fs specific  
    unsigned int s_flags; //mount , etc flags
    unsigned long s_magic; //filesystem magic number
    struct dentry *s_root; //dentry object of the root directory
    unsigned int s_count;  //ref count
    
    struct list_head s_inodes; //list of all inodes
    struct list_head s_dirty; //list of all dirty inodes
    struct list_head s_files; //list of file objects
    void  *s_fs_info; //pointer to the filesystem's superblock
};

struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    int (*write_inode)(struct inode *); //write the inode to disk to the appropriate fs's disk inode
    void (*read_inode)(struct inode *); //read the disk inode into the given inode structure
    void (*put_inode)(struct inode *);  
    int (*drop_inode)(struct inode *); /*
 Invoked when the inode is about to be destroyed—that is, when the last user
releases the inode;
*/
    void (*delete_inode)(struct inode *);    
    void (*put_super) (struct super_block *);
    void (*write_super) (struct super_block *);
    int (*sync_fs)(struct super_block *sb, int wait);
};

#define I_DIRTY 1

struct inode {
    struct list_head i_hash;
    struct list_head i_free;
    struct list_head i_sb_list; //pointer of list of inodes on superblock
    struct list_head i_dentry; //the head of list of dentry objects referencing this inode
    unsigned short i_dev;
    unsigned long i_no; //inode number
    unsigned int i_count; //reference count
    unsigned short i_mode; //file type and access rights
    unsigned int i_nlinks; //number of hard links
    unsigned short i_uid;
    unsigned short i_gid; 
    unsigned int i_flags;
    unsigned long i_size; //file length in bytes
    unsigned int i_atime; //last accessed time of file
    unsigned int i_mtime; //last modified time of file
    unsigned int i_ctime; //time of last inode change 
    int i_state; //if dirty or not
    unsigned long i_blocks; //number of blocks of file
    struct inode_operations *i_op; //inode operations 
    struct file_operations *f_op; //def file operations
    struct super_block *i_sb; //pointer to superblock object

};

/*
 * below is the file table entry
 */
struct file {
    struct list_head f_list; //pointers for generic file object list
    struct dentry *f_dentry; //dentry object associated with file
    
    struct file_operations *f_op; //pointer to file operation table
    unsigned int f_count; //file object's reference counter
    unsigned int f_flags; //flags specified when opening the file 
    unsigned short f_mode; //process access mode
    loff_t f_pos; //current file offset
    unsigned int f_uid; //user uid
    unsigned int f_gid; 
};


#define I_lock 1 << 0

// #define locked_inode(inode) (SET_FLAG(inode->i_flags, I_lock));
// #define unlocked_inode(inode) (CLEAR_FLAG(inode->i_flags, I_lock));

static inline struct inode *locked_inode(struct inode *inode) {
    SET_FLAG(inode->i_flags, I_lock);
    return inode;
}
 
static inline struct inode *unlocked_inode(struct inode *inode) {
    CLEAR_FLAG(inode->i_flags, I_lock);
    return inode;
}

void create_inode_cache(void);
struct inode *iget(unsigned short dev_no, unsigned int inum);

/* VFS operation wrappers */
int vfs_create(struct inode *dir, struct dentry *dentry, int mode);
int vfs_mkdir(struct inode *dir, struct dentry *dentry, int mode);
int vfs_rmdir(struct inode *dir, struct dentry *dentry);
int vfs_unlink(struct inode *dir, struct dentry *dentry);
int vfs_rename(struct inode *old_dir, struct dentry *old_dentry,
               struct inode *new_dir, struct dentry *new_dentry);
ssize_t vfs_read(struct file *file, char *buf, size_t count, loff_t *pos);
ssize_t vfs_write(struct file *file, const char *buf, size_t count, loff_t *pos);

/* Filesystem registration */
int register_filesystem(struct file_system_type *fs);
int unregister_filesystem(struct file_system_type *fs);
struct file_system_type *get_fs_type(const char *name);

/* Superblock management */
struct super_block *alloc_super(void);
void destroy_super(struct super_block *sb);
struct super_block *get_super(unsigned char dev_no);

#endif

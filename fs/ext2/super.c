#include "vfs.h"
#include "list.h"
#include "slab.h"
#include "serial.h"
#include "string.h"

/* List of registered filesystems */
static struct list_head file_systems;
static int fs_list_initialized = 0;

/* Superblock cache */
static kmem_cache_t *super_cache;

/* Initialize filesystem registry */
static void init_fs_registry(void)
{
    if (!fs_list_initialized) {
        INIT_LIST_HEAD(&file_systems);
        fs_list_initialized = 1;
        
        /* Create superblock cache */
        super_cache = kmem_cache_create("super_block", sizeof(struct super_block),
                                       KMALLOC_MINALIGN, SLAB_HWCACHE_ALIGN, NULL);
        if (!super_cache) {
            printk("Failed to create superblock cache\n");
        }
    }
}

/* Register a filesystem type */
int register_filesystem(struct file_system_type *fs)
{
    struct list_head *tmp;
    struct file_system_type *existing;
    
    if (!fs || !fs->name) {
        return -1;
    }
    
    init_fs_registry();
    
    /* Check if already registered */
    list_for_each(tmp, &file_systems) {
        existing = list_entry(tmp, struct file_system_type, fs_list);
        if (!strcmp(existing->name, fs->name)) {
            printk("Filesystem %s already registered\n", fs->name);
            return -1;
        }
    }
    
    /* Add to list */
    INIT_LIST_HEAD(&fs->fs_supers);
    list_add(&file_systems, &fs->fs_list);
    
    printk("Registered filesystem: %s\n", fs->name);
    return 0;
}

/* Unregister a filesystem type */
int unregister_filesystem(struct file_system_type *fs)
{
    if (!fs) {
        return -1;
    }
    
    /* Remove from list */
    list_del(&fs->fs_list);
    
    printk("Unregistered filesystem: %s\n", fs->name);
    return 0;
}

/* Get filesystem type by name */
struct file_system_type *get_fs_type(const char *name)
{
    struct list_head *tmp;
    struct file_system_type *fs;
    
    if (!name) {
        return NULL;
    }
    
    init_fs_registry();
    
    list_for_each(tmp, &file_systems) {
        fs = list_entry(tmp, struct file_system_type, fs_list);
        if (!strcmp(fs->name, name)) {
            return fs;
        }
    }
    
    return NULL;
}

/* Allocate a new superblock */
struct super_block *alloc_super(void)
{
    struct super_block *sb;
    
    if (!super_cache) {
        init_fs_registry();
    }
    
    sb = (struct super_block *)kmem_cache_alloc(super_cache, 0);
    if (!sb) {
        return NULL;
    }
    
    /* Initialize superblock */
    INIT_LIST_HEAD(&sb->s_list);
    sb->s_dev = 0;
    sb->s_blocksize = 0;
    sb->s_dirt = 0;
    sb->s_op = NULL;
    sb->s_flags = 0;
    sb->s_magic = 0;
    sb->s_root = NULL;
    sb->s_count = 1;
    
    INIT_LIST_HEAD(&sb->s_inodes);
    INIT_LIST_HEAD(&sb->s_dirty);
    INIT_LIST_HEAD(&sb->s_files);
    sb->s_fs_info = NULL;
    
    return sb;
}

/* Destroy a superblock */
void destroy_super(struct super_block *sb)
{
    if (!sb) {
        return;
    }
    
    sb->s_count--;
    
    if (sb->s_count == 0) {
        /* Free filesystem-specific data */
        if (sb->s_fs_info) {
            kfree(sb->s_fs_info);
        }
        
        /* Free superblock */
        kmem_cache_free(super_cache, sb);
    }
}

/* Get superblock for device */
struct super_block *get_super(unsigned char dev)
{
    struct list_head *p, *q;
    struct file_system_type *fs;
    struct super_block *sb;
    
    if (!fs_list_initialized) {
        return NULL;
    }
    
    list_for_each(p, &file_systems) {
        fs = list_entry(p, struct file_system_type, fs_list);
        list_for_each(q, &fs->fs_supers) {
            sb = list_entry(q, struct super_block, s_list);
            if (sb->s_dev == dev) {
                return sb;
            }
        }
    }
    
    return NULL;
}

/* this file should have no knowledge of ext2 constants. */
/* it should derive all its information from the superblock. */

#include <stdint.h>
#include "fs.h"
#include "hardware.h"
#include "disk.h"
#include "slab.h"
#include "mm.h"
#include "serial.h"
#include "string.h"
#include "vfs.h"
#include "dcache.h"
#include "ext2_balloc.h"
#include "task.h"
#include "buffer.h"

#define KLOG(fmt, ...) printk("[ext2] " fmt "\n", ##__VA_ARGS__)

#define S_BLOCK_SIZE    (1024 << super.s_log_block_size)

#define SECTORS_PER_BLOCK   (S_BLOCK_SIZE / DISK_SECTOR_SIZE)

#define SUPERBLOCK_OFFSET   SECTORS_PER_BLOCK

/* External references */
extern struct task_struct *current;

// TODO: maybe...move this into the "disk" section?
// in general, how to handle disk partitions.
uint32_t filesys_start; // used for correct linear base addressing.
uint8_t fs_start_set = 0;
uint16_t n_block_groups;

// TODO: initialize this with a calloc
bgdesc_t bgdt[1024];

ext2_super_block super;


int disk_read_blk(uint32_t block_num, uint8_t *buf) {
    if (!fs_start_set) {
        printk("disk_read_blk: fs_start_set is 0! (block %u)\n", block_num);
        return 1;
    }

    uint32_t lba = filesys_start + block_num * SECTORS_PER_BLOCK;
    
    /* printk("R-BLK %u @ LBA %u (s_log_bs=%u)\n", 
           block_num, lba, super.s_log_block_size); */

    for (int i = 0; i < SECTORS_PER_BLOCK; i++) {
        disk_read(lba + i, buf + DISK_SECTOR_SIZE * i);
    }
    return 0;
}

int disk_write_blk(uint32_t block_num, uint8_t *buf) {
    if (!fs_start_set) return -1;

    uint32_t lba = filesys_start + block_num * SECTORS_PER_BLOCK;
    for (int i = 0; i < SECTORS_PER_BLOCK; i++) {
        disk_write(lba + i, buf + DISK_SECTOR_SIZE * i, DISK_SECTOR_SIZE);
    }
    return 0;
}

// TODO: replace disk_write_blk with disk_write_bn
int disk_write_bn(uint32_t block_num, uint8_t *buf, uint16_t len) {
    if (!fs_start_set) return -1;
    if (len > S_BLOCK_SIZE) return -1;

    uint32_t lba = filesys_start + block_num * SECTORS_PER_BLOCK;

    disk_write(lba, buf, len);
    return 0;
}


/* Block allocation functions moved to ext2_balloc.c */
/* Inode table operations moved to ext2_inode.c */

inode_t new_dir_inode() {
    inode_t new_inode;
    new_inode.i_mode = EXT2_S_IFDIR;
    new_inode.i_links_count = 1;
    new_inode.i_blocks = 0; 
    new_inode.i_links_count = 1;
    new_inode.i_file_acl = 0;
    new_inode.i_dir_acl = 0;
    new_inode.i_faddr = 0;
    return new_inode;
}


// used for getting the filesystem superblock location
// from a given filesystem start point (in Mb)
uint32_t fs_start_to_lba_superblk(uint32_t mb_start) {
    // adding a block's worth of sectors (SECTORS_PER_BLOCK)
    // to the initial write address, since the first block 
    // is reserved for boot records on ext2 (which we don't use).
    //
    // the write_addr is lba, so it's measured in disk sectors
    // (512 byte units)
    return mb_to_lba(mb_start) + SUPERBLOCK_OFFSET;
}


void set_superblock() {
    // set superblock 
    ext2_super_block b;
    /* Superblock is at offset 1024. If block size is 1024, it's block 1. */
    if (disk_read_blk(1, (uint8_t *) &b)) {
        printk("set_superblock: Failed to read superblock LBA 2.\n");
        return;
    }

    if (b.s_magic != EXT2_SUPER_MAGIC) {
        printk("set_superblock: invalid magic 0x%x at Block 1 (LBA 2)\n", b.s_magic);
        return;
    }

    super = b;
    printk("set_superblock: loaded magic 0x%x, blocks_per_group=%u\n", 
           super.s_magic, super.s_blocks_per_group);
}

void set_bgdt() {
    uint32_t bc = super.s_blocks_count;
    uint32_t bpg = super.s_blocks_per_group;
    
    if (bpg == 0) {
        printk("set_bgdt: s_blocks_per_group is 0! (bc=%u, magic=0x%x)\n", 
               bc, super.s_magic);
        return;
    }
    
    n_block_groups = bc / bpg;
    if (bc % bpg != 0) n_block_groups++;


    /* BGDT follows the superblock. If block size is 1024, it's block 2. */
    uint32_t bgdt_start_blkno = 2;
    uint8_t buf[S_BLOCK_SIZE];

    disk_read_blk(bgdt_start_blkno, buf);

    // copy into bgdt
    uint8_t *bgdt_buf = (uint8_t *) bgdt;
    for (int i = 0; i < n_block_groups * sizeof(bgdesc_t); i++) {
        bgdt_buf[i] = buf[i];
    }
}

void read_fs(uint32_t fs_start_in_mb) {
    // file-global
    filesys_start = mb_to_lba(fs_start_in_mb);
    fs_start_set = 1;

    // once filesys is set, we can use disk_read_blk.
    set_superblock();

    // now super is set, and we can use that.    
    // next, we want to set the bgdt entries.
    set_bgdt();

}

inode_t get_inode(uint32_t inode_n) {
    // inode numbers start from 1
    uint32_t index = inode_n - 1;

    uint16_t bgdt_n = index / super.s_inodes_per_group;
    uint32_t inode_offset = index % super.s_inodes_per_group;

    uint32_t inode_table_blkn = bgdt[bgdt_n].bg_inode_table;

    // each 1024-byte block evenly fits 8 inodes (128 bytes each)
    uint16_t inodes_per_blk = S_BLOCK_SIZE / sizeof(inode_t);

    uint16_t inode_blk_n = inode_table_blkn + inode_offset / inodes_per_blk;
    uint16_t inode_blk_offset = inode_offset % inodes_per_blk;

    struct buffer_head *bh;

    inode_t blk_nodes[S_BLOCK_SIZE / sizeof(inode_t)];

    bh = bread(DEV_NO, inode_blk_n);

    if (!bh) {
        printk("failed to read buffer %d\n", inode_blk_n);
        /* return 0; */
    }

    /* disk_read_blk(inode_blk_n, (uint8_t *) blk_nodes); */
    
    inode_t res = ((inode_t *)bh->b_data)[inode_blk_offset];
    brelse(bh);

    return res;
}


// get the block number of the ith data block for a file. 
// hides all the "indirect block" stuff
// at the cost of a bit of efficiency.
//
uint32_t indirect_block(inode_t file, uint32_t i) {
    struct buffer_head *bh;
    uint32_t dir_blk_len = 12;
    if (i < dir_blk_len) {
        return file.i_block[i];
    }

    uint32_t ind_blk_len = S_BLOCK_SIZE / sizeof(uint32_t);

    if (i < dir_blk_len + ind_blk_len) {
        /* uint32_t blocks[ind_blk_len]; */
        /* disk_read_blk(file.i_block[12], (uint8_t *) blocks); */

        bh = bread(DEV_NO, file.i_block[12]);
        uint16_t j = i - dir_blk_len;
        /* return blocks[j]; */
        return *(uint32_t*)(bh->b_data + j);
    }

    uint32_t dbl_ind_blk_len = S_BLOCK_SIZE * ind_blk_len;
    if (i < dir_blk_len + ind_blk_len + dbl_ind_blk_len) {
        /* uint32_t blocks[ind_blk_len]; */
        /* disk_read_blk(file.i_block[13], (uint8_t *) blocks); */
        bh = bread(DEV_NO, file.i_block[13]);

        uint32_t j = i - dir_blk_len - ind_blk_len;

        uint32_t ind_blk_index = j / ind_blk_len;
        uint32_t blk_index = j % ind_blk_len;
        struct buffer_head *bh2;

        /* disk_read_blk(blocks[ind_blk_index], (uint8_t *) blocks); */
        bh2 = bread(DEV_NO, bh->b_data[ind_blk_index]);

        /* return blocks[blk_index]; */

        return *(uint32_t*)(bh->b_data + blk_index);
    }

    // TODO: triply-independent.
    // i.e. file needs more than 65536 1kb blocks, 
    // so the file is greater than 65 Mb in size. 
    // our OS is not at that point yet. 
    return 0;
}

/* Get the ith block for a file. */
uint8_t *get_file_block(ext2_file_handle file, uint32_t i) {
    struct buffer_head *bh;
    uint32_t block_n = indirect_block(file.inode, i);

    uint8_t *buf = (uint8_t *)kmalloc(S_BLOCK_SIZE, 0);
    if (!buf) {
        printk("failed kmalloc \n");
        return NULL;
    }

    if (!(bh = bread(DEV_NO, block_n))) return NULL;

    return (uint8_t*)bh->b_data;
}


/* Finds a free block and updates all relevant metadata. */
/* Splitting into separate functions "reserve free block" 
 * and "reserve free inode" will often double the number of 
 * disk syncs of the superblock and the BGDT. 
 * To get around this, a future optimization could defer 
 * actually writing out to disk for a while. */

int set_i_block(inode_t *file, uint32_t inode_n, uint32_t i, uint32_t blockn) {
    /* Direct blocks */

    struct buffer_head *bh1, *bh2;
    uint32_t dir_blk_len = 12;
    if (i < dir_blk_len) {
        file->i_block[i] = blockn;
        write_inode_table(inode_n, *file);
        return 0;
    }

    /* Indirect blocks. */

    uint32_t ind_blk_len = S_BLOCK_SIZE / sizeof(uint32_t);

    if (i < dir_blk_len + ind_blk_len) {
        /* uint32_t blocks[ind_blk_len]; */
        /* disk_read_blk(file->i_block[12], (uint8_t *) blocks); */
        bh1 = bread(DEV_NO, file->i_block[12]);
        uint16_t j = i - dir_blk_len;
        /* blocks[j] = blockn; */
        *(uint32_t*)(bh1->b_data + j) = blockn;
        /* disk_write_blk(file->i_block[12], (uint8_t *) blocks); */
        bwrite(bh1);

        return 0;
    }

    /* Doubly-indirect blocks. 
     * TODO: use buffer algos instead of direct disk reads
     */

    uint32_t dbl_ind_blk_len = S_BLOCK_SIZE * ind_blk_len;
    if (i < dir_blk_len + ind_blk_len + dbl_ind_blk_len) {
        uint32_t blocks[ind_blk_len];
        disk_read_blk(file->i_block[13], (uint8_t *) blocks);

        uint32_t j = i - dir_blk_len - ind_blk_len;

        uint32_t ind_blk_index = j / ind_blk_len;
        uint32_t blk_index = j % ind_blk_len;

        uint32_t update_location = blocks[ind_blk_index];

        disk_read_blk(update_location, (uint8_t *) blocks);

        blocks[blk_index] = blockn;

        disk_write_blk(update_location, (uint8_t *) blocks);

        return 0;
    }

    /* Triply-indirect blocks. TODO */

    return 0;
}

uint16_t i_block_len(inode_t i) {
    return i.i_blocks / (2 << super.s_log_block_size);
}


int chdir(ext2_file_handle current_dir, const char *name, ext2_file_handle *ret_dir) {
    uint16_t nblocks = i_block_len(current_dir.inode);

    // NOTE: "dentries_per_blk" only makes sense because we've regularized the size of dentries.
    // in the future, we'll relax this restriction. 
    uint16_t dentries_per_blk = S_BLOCK_SIZE / sizeof(dentry_t);
    dentry_t dentries[dentries_per_blk];

    for (int i = 0; i < nblocks; i++) {
        dentry_t *dentries = (dentry_t *) get_file_block(current_dir, i);
        if (dentries == NULL) {
            printk("null\n");
            return -1;
        }

        for (int i = 0; i < dentries_per_blk; i++) {
            dentry_t d = dentries[i];
            if (!strcmp(d.name, name)) {
                // this is the match. 
                ext2_file_handle f = {
                    .inode = get_inode(d.inode),
                    .inode_n = d.inode
                };

                *ret_dir = f;
                return 0;
            }
        }
        kfree(dentries);
    }

    // we didn't find name. 
    return -1; 
}

int add_block_to_file(ext2_file_handle *file, uint32_t new_block) {
    // need to: update inode with new i_blocks
    // and i_block (and make sure inode table is updated...)
    file->inode.i_blocks += 2;

    // update i_block array
    uint16_t block_len = i_block_len(file->inode);

    // writes inode table
    set_i_block(&(file->inode), file->inode_n, block_len - 1, new_block);

    return 0; 
}

int write_block_at(uint32_t block, uint16_t offset, uint8_t *chunk, uint16_t len) {
    if (offset + len > S_BLOCK_SIZE) {
        return -1;
    }
    // read the existing buffer
    /* uint8_t buf[S_BLOCK_SIZE]; */
    /* disk_read_blk(block, buf); */
    uint8_t *buf;
    unsigned short i = 0;
    struct buffer_head *bh;
    bh = bread(DEV_NO, block);
    if (!bh) {
        printk("bread failed for block %u\n", block);
        return -1; // Return error if bread fails
    }

    buf = (uint8_t*)bh->b_data;

    // update the buffer at location we want
    for (i = 0; i < len; i++) {
        buf[i + offset] = chunk[i];
    }

    // write it back
    /* disk_write_blk(block, buf); */
    bwrite(bh);
    brelse(bh); // Release the buffer head

    return 0;
}

int add_dentry_to_new_block(ext2_file_handle dir, dentry_t d) {
    uint32_t new_block;
    reserve_free_block(&new_block);

    add_block_to_file(&dir, new_block);

    // set rec_len to the end of the block
    d.rec_len = S_BLOCK_SIZE;

    return write_block_at(new_block, 0, (uint8_t *) &d, sizeof(dentry_t));
}

int add_dentry(ext2_file_handle dir, dentry_t d) {
    // find the insert point
    inode_t inode = dir.inode;
    
    uint16_t block_len = i_block_len(inode);

    if (block_len == 0) {
        return add_dentry_to_new_block(dir, d);
    }

    // at least one block in use. see if the last block is full. 
    // TODO: if entries are deleted, directory might have 
    // "holes" in it. 
    uint8_t *block = get_file_block(dir, block_len - 1); 
    if (block == NULL) return -1; 

    dentry_t *curr;
    uint8_t dentries_read = 0;
    uint16_t pos = 0;
    uint16_t lastpos;

    while (pos < S_BLOCK_SIZE) {
        curr = (dentry_t *) (block + pos);
        lastpos = pos;
        pos += curr->rec_len;

        dentries_read++;
    }

    if (dentries_read >= S_BLOCK_SIZE / sizeof(dentry_t)) {
        // we can't fit another dentry in this block.
        return add_dentry_to_new_block(dir, d);
    }

    // we can fit another dentry in this block.

    // modify the last dentry, if it exists.
    // it will still be pointing to the end of the block, 
    // and we don't want that. 
    if (dentries_read > 0) {
        curr->rec_len = sizeof(dentry_t); 
    }

    dentry_t *new_dentry = curr + 1;
    *new_dentry = d;

    // update rec_len to appropriately point to the end of the block
    // from the start of this dentry.
    new_dentry->rec_len = S_BLOCK_SIZE - dentries_read * sizeof(dentry_t);

    // write this disk block back.

    uint32_t fblock_n = indirect_block(dir.inode, block_len - 1);
   
    disk_write_blk(fblock_n, block);

    return 0;
}

// TODO: just cache this, like we do with superblock.
ext2_file_handle get_root_dir() {
    ext2_file_handle f = {
        .inode = get_inode(EXT2_ROOT_INO),
        .inode_n = EXT2_ROOT_INO     
    };
    return f;
}

// deprecate! use a generic mkfile thing. 
// "/test.txt"
int create_test_file() {
    char *fn = "test.txt";
    uint8_t flen = 9; 
    uint8_t dlen = 4;

    char data[dlen];
    data[0] = 'H';
    data[1] = 'i';
    data[2] = '\n';
    data[3] = '\0';

    // reserve a free block and free inode
    uint32_t free_block_n, free_inode_n;
    if (first_free_block_num(&free_block_n)) {
        printk("No free blocks available.\n");
        return -1;
    }

    if (first_free_inode_num(&free_inode_n)) {
        printk("No free inodes available.\n");
        return -1;
    }

    // create file inode, etc.
    inode_t n = { 0 };
    n.i_mode = EXT2_S_IFREG;
    n.i_size = 4;
    n.i_blocks = 2; // ~wasteful~
    n.i_block[0] = free_block_n;

    write_inode_table(free_inode_n, n);

    set_inode_bitmap(free_inode_n);

    set_block_bitmap(free_block_n);

    // add a "directory entry" in the root directory. 
    dentry_t d = {
        .inode = free_inode_n,
        .rec_len = S_BLOCK_SIZE, // rec_len needs to point to end of block
        // since it's the last entry. 
        .name_len = flen,
        .file_type = EXT2_FT_DIR };

    strcpy(d.name, fn);

    // put the dentry d in the directory file!
    ext2_file_handle root = get_root_dir();

    // for our test file, we can write to the first block~

    // write to the directory file.
    uint32_t data_block_n = root.inode.i_block[0];

    // TODO: fix disk_write_blk
    // so we don't have to read the whole thing
    disk_write_bn(data_block_n, (uint8_t *) &d, sizeof(d));

    // write the data (finally) to the actual file!
    disk_write_bn(free_block_n, (uint8_t *) data, dlen);

    // add to inode table in free place
    update_inode_bg_desc(free_inode_n);
    update_block_bg_desc(free_block_n);
    disk_sync_bgdt();

    // update the block group descriptor that the block belongs to.
    // update the superblock. 
    super.s_free_blocks_count -= 1;
    super.s_free_inodes_count -= 1;
    disk_sync_super();
}

// print file in root directory. 
// this is just a test.
void print_file(const char *fn) {
    ext2_file_handle root = get_root_dir();
    // read from directory
    uint32_t data_block_n = root.inode.i_block[0];

    uint8_t buf[S_BLOCK_SIZE];
    disk_read_blk(data_block_n, (uint8_t *) buf);

    dentry_t *d = (dentry_t *) buf;

    if(!strcmp((const char *) d->name, fn)) {
        // this is the entry we want!

        // get the inode
        uint32_t inode_n = d->inode;
        inode_t ind = get_inode(inode_n);

        data_block_n = ind.i_block[0];
        uint8_t bufb[S_BLOCK_SIZE];

        // should be...
        disk_read_blk(data_block_n, (uint8_t *) bufb);
    
        uint8_t fdata[ind.i_size];
        for (int i = 0; i < ind.i_size; i++) {
            fdata[i] = bufb[i];
        }
        printk((char *) fdata);
    }
}

int create_root_directory() {
    // the root directory is always at the same location
    // in the inode table.
    uint32_t root_inode_n = EXT2_ROOT_INO;

    // Create a new inode, and update the inode table with it.
    inode_t new_inode = new_dir_inode();

    reserve_inode(root_inode_n);

    // write to inode table
    write_inode_table(root_inode_n, new_inode);
}


void set_initial_used_blocks() {
    // how many blocks for inode table, in each block group?
    uint16_t inodes_per_block = S_BLOCK_SIZE / sizeof(inode_t);
    uint32_t inode_table_blocks = super.s_inodes_per_group / inodes_per_block;

    uint32_t bpg = super.s_blocks_per_group;

    // set for block group 1 + 2
    uint16_t boot_offset = 1;
    uint16_t super_bgdt_offset = 2; // superblock, bgdt
    uint16_t bitmap_offset = 2;

    // boot
    set_block_bitmap(0);

    // superblock + backup
    set_block_bitmap(1);
    set_block_bitmap(1 + bpg);

    // bgdt + backup
    set_block_bitmap(2);
    set_block_bitmap(2 + bpg);
   
    // mark the bitmaps + inode table blocks, for each block group. 
    for (int i = 0; i < bitmap_offset + inode_table_blocks; i++) {
        // block group 1
        set_block_bitmap(boot_offset + super_bgdt_offset + i);
        // block group 2
        set_block_bitmap(boot_offset + super_bgdt_offset + bpg + i);
        // block group 3
        set_block_bitmap(boot_offset + 2*bpg + i);
    }
}


void finish_fs_init(uint32_t mb_start) {
    // read the metadata into our "local" variables.
    /* read_fs(mb_start); */

    show_fs(mb_start);

    /* set_initial_used_blocks(); */

    /* create_root_directory(); */

}


/* int open(const char *name, int flags, int perm) */
/* { */

/* } */

int split_path(const char *usr_path, ext2_file_handle *parent, char *leaf) {
    // copy the path right away, so strtok doesn't corrupt it.
    char path[strlen(usr_path)];
    strcpy(path, usr_path);

    char *next_dirname = strtok(path, "/");
    // first character should be "/"
    if (strlen(next_dirname) != 0) return -1;

    next_dirname = strtok(NULL, "/");

    char *following_dirname = strtok(NULL, "/");

    ext2_file_handle curr_dir = get_root_dir();

    ext2_file_handle next_dir;
    while (following_dirname != NULL) {
        // failed to change directory
        if (chdir(curr_dir, next_dirname, &next_dir)) {
            printk("failed to change directory\n");
            return -1;
        }
        curr_dir = next_dir;
        next_dirname = following_dirname;
        following_dirname = strtok(NULL, "/");
    }

    strcpy(leaf, next_dirname);
    *parent = curr_dir;

    return 0;
}

int ls(char *path) {
    ext2_file_handle parent_dir;
    char leaf[64];
    split_path(path, &parent_dir, leaf);
    ext2_file_handle leaf_dir;
    chdir(parent_dir, leaf, &leaf_dir);

    // list all files in leaf_directory
    uint32_t block_len = i_block_len(leaf_dir.inode);
    for (uint32_t i = 0; i < block_len; i++) {
        // read the block
        uint8_t *block = get_file_block(leaf_dir, i);

        kfree(block); 
    }
}

int mkdir(char *path) {
    ext2_file_handle parent_dir;
    char new_dirname[64];
    if (split_path(path, &parent_dir, new_dirname)) return -1;

    // looks good here!!
    printk("mkdir: ");
    printk(new_dirname);
    printk("\n");

    uint32_t new_inode_n, new_block_n;
    reserve_free_inode(&new_inode_n);
    reserve_free_block(&new_block_n);

    // Create a new inode, and update the inode table with it.
    inode_t new_inode = new_dir_inode();

    // write to inode table
    write_inode_table(new_inode_n, new_inode);

    // add a dentry for this new directory
    dentry_t d = {
        .inode = new_inode_n,
        .rec_len = sizeof(dentry_t),
        .name_len = strlen(new_dirname),
        .file_type = EXT2_FT_DIR,
    };

    // copy the name
    strcpy(d.name, new_dirname);

    // add directory entry to parent directory
    add_dentry(parent_dir, d);
}

int rmdir(char *path) {
    ext2_file_handle parent_dir;
    char target_dirname[64];
    printk("\nin rmdir\n");
    printk("received path: "); printk(path); printk("\n");
    if (split_path(path, &parent_dir, target_dirname)) return -1;

    printk("rmdir: ");
    printk(target_dirname);
    printk("\n");
}

void read_inode(unsigned long inode_num)
{
    uint32_t index = inode_num - 1;
}




void copy_ext2_sb_to_mem(struct ext2_sb_info *ext2_sb, ext2_super_block *s_es)
{
    ext2_sb->s_inodes_per_block = EXT2_BLK_SIZE / sizeof(inode_t); 
    ext2_sb->s_blocks_per_group =  s_es->s_blocks_per_group;
    ext2_sb->s_inodes_per_group =  s_es->s_inodes_per_group;
    ext2_sb->s_itb_per_group = 1;
    ext2_sb->s_gdb_count = (g_n_block_groups * sizeof(bgdesc_t)) /
        EXT2_BLK_SIZE; //blocks needed to store block group descriptors
    ext2_sb->s_desc_per_block = EXT2_BLK_SIZE / sizeof(bgdesc_t);
    ext2_sb->s_groups_count = g_n_block_groups; //total block groups;
    ext2_sb->s_sbh = NULL;  //buffer containing super block
    ext2_sb->s_es = s_es;
    ext2_sb->s_group_desc = NULL; //buffer containing group desc
    ext2_sb->s_inode_size = s_es->s_inode_size;
    ext2_sb->s_first_ino = s_es->s_first_ino;
    ext2_sb->s_freeblocks_counter = s_es->s_free_blocks_count;
    ext2_sb->s_freeinodes_counter = s_es->s_free_inodes_count;
}


void display_ext2_super_block(ext2_super_block *sb)
{
    int i;

    KLOG("EXT2 Superblock dump:");

    KLOG("Inodes count            : %u", sb->s_inodes_count);
    KLOG("Blocks count            : %u", sb->s_blocks_count);
    KLOG("Reserved blocks count   : %u", sb->s_r_blocks_count);
    KLOG("Free blocks count       : %u", sb->s_free_blocks_count);
    KLOG("Free inodes count       : %u", sb->s_free_inodes_count);
    KLOG("First data block        : %u", sb->s_first_data_block);
    KLOG("Log block size          : %u (block size = %u)",
         sb->s_log_block_size, 1024U << sb->s_log_block_size);
    KLOG("Log fragment size       : %u", sb->s_log_frag_size);
    KLOG("Blocks per group        : %u", sb->s_blocks_per_group);
    KLOG("Fragments per group     : %u", sb->s_frags_per_group);
    KLOG("Inodes per group        : %u", sb->s_inodes_per_group);
    /* KLOG("Mount time              : %u", sb->s_mtime); */
    /* KLOG("Write time              : %u", sb->s_wtime); */

    /* KLOG("Mount count             : %u", sb->s_mnt_count); */
    /* KLOG("Max mount count         : %u", sb->s_max_mnt_count); */
    /* KLOG("Magic                   : 0x%X", sb->s_magic); */
    /* KLOG("Filesystem state        : %u", sb->s_state); */
    /* KLOG("Errors behavior         : %u", sb->s_errors); */
    /* KLOG("Minor revision level    : %u", sb->s_minor_rev_level); */

    /* KLOG("Last check              : %u", sb->s_lastcheck); */
    /* KLOG("Check interval          : %u", sb->s_checkinterval); */
    /* KLOG("Creator OS              : %u", sb->s_creator_os); */
    /* KLOG("Revision level          : %u", sb->s_rev_level); */
    /* KLOG("Default res UID         : %u", sb->s_def_resuid); */
    /* KLOG("Default res GID         : %u", sb->s_def_resgid); */

    /* KLOG("First non-reserved inode: %u", sb->s_first_ino); */
    KLOG("Inode size              : %u", sb->s_inode_size);
    KLOG("Block group number      : %u", sb->s_block_group_nr);
    /* KLOG("Feature compat          : 0x%X", sb->s_feature_compat); */
    /* KLOG("Feature incompat        : 0x%X", sb->s_feature_incompat); */
    /* KLOG("Feature ro_compat       : 0x%X", sb->s_feature_ro_compat); */

    /* printk("[ext2] UUID                    : "); */
    /* for (i = 0; i < 16; i++) */
    /*     printk("%02X", (uint8_t)sb->s_uuid[i]); */
    /* printk("\n"); */

    /* printk("[ext2] Volume name             : "); */
    /* for (i = 0; i < 16 && sb->s_volume_name[i]; i++) */
    /*     printk("%c", sb->s_volume_name[i]); */
    /* printk("\n"); */

    /* printk("[ext2] Last mounted at         : "); */
    /* for (i = 0; i < 64 && sb->s_last_mounted[i]; i++) */
    /*     printk("%c", sb->s_last_mounted[i]); */
    /* printk("\n"); */

    /* KLOG("Algorithm bitmap        : %u", sb->s_algo_bitmap); */
    /* KLOG("Prealloc blocks         : %u", sb->s_prealloc_blocks); */
    /* KLOG("Prealloc dir blocks     : %u", sb->s_prealloc_dir_blocks); */

    /* printk("[ext2] Journal UUID            : "); */
    /* for (i = 0; i < 16; i++) */
    /*     printk("%02X", (uint8_t)sb->s_journal_uuid[i]); */
    /* printk("\n"); */

    /* KLOG("Journal inode           : %u", sb->s_journal_inum); */
    /* KLOG("Journal device          : %u", sb->s_journal_dev); */
    /* KLOG("Last orphan             : %u", sb->s_last_orphan); */

    /* KLOG("Default hash version    : %u", sb->s_def_hash_version); */
    /* KLOG("Default mount options   : 0x%X", sb->s_default_mount_options); */
    /* KLOG("First meta block group  : %u", sb->s_first_meta_bg); */

    KLOG("Superblock dump complete.");
}

void copy_ext2_sb_from_mem(struct ext2_sb_info *ext2_sb, ext2_super_block *s_es)
{
    s_es->s_free_inodes_count = ext2_sb->s_freeinodes_counter;
    s_es->s_free_blocks_count = ext2_sb->s_freeblocks_counter;
}
void read_sb(struct ext2_sb_info *ext2_sb)
{
    ext2_super_block *s_es;
    struct buffer_head *bh;

    bh = bread(DEV_NO, 2);
    if (!bh) {
        printk("failed reading buffer 1 for sb\n");
    }
    s_es = (ext2_super_block*)bh->b_data;
    copy_ext2_sb_to_mem(ext2_sb, s_es);

    display_ext2_super_block(s_es);

    brelse(bh);
}
static inline struct ext2_sb_info *EXT2_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

/* Byte order conversion macros (simplified - assumes little endian) */
#define cpu_to_le32(x) (x)
#define le32_to_cpu(x) (x)

/* Stub functions for counting - these would need proper implementation */
static uint32_t ext2_count_free_blocks(struct super_block *sb)
{
    struct ext2_sb_info *sbi = EXT2_SB(sb);
    return sbi->s_freeblocks_counter;
}

static uint32_t ext2_count_free_inodes(struct super_block *sb)
{
    struct ext2_sb_info *sbi = EXT2_SB(sb);
    return sbi->s_freeinodes_counter;
}

/* Mark buffer as dirty */
static void mark_buffer_dirty(struct buffer_head *bh)
{
    if (bh) {
        SET_FLAG(bh->flags, BH_dirty);
    }
}

static void ext2_sync_super(struct super_block *sb, ext2_super_block *es,
			    int wait)
{
	/* ext2_clear_super_error(sb); */
	es->s_free_blocks_count = cpu_to_le32(ext2_count_free_blocks(sb));
	es->s_free_inodes_count = cpu_to_le32(ext2_count_free_inodes(sb));
	/* es->s_wtime = cpu_to_le32(get_seconds()); */
	/* unlock before we do IO */
	mark_buffer_dirty(EXT2_SB(sb)->s_sbh);
	/* if (wait) */
	/* 	sync_dirty_buffer(EXT2_SB(sb)->s_sbh); */
	sb->s_dirt = 0;
}


int ext2_sync_fs(struct super_block *sb, int wait)
{
	struct ext2_sb_info *sbi = EXT2_SB(sb);
	ext2_super_block *es = EXT2_SB(sb)->s_es;

	/* if (es->s_state & cpu_to_le16(EXT2_VALID_FS)) { */
	/* 	es->s_state &= cpu_to_le16(~EXT2_VALID_FS); */
	/* } */
	
    ext2_sync_super(sb, es, wait);
	return 0;
}
void ext2_write_super(struct super_block *sb)
{
    ext2_sync_fs(sb, 1);
}

void write_sb(struct ext2_sb_info *ext2_sb)
{
    ext2_super_block *s_es;
    struct buffer_head *bh;

    bh = bread(DEV_NO, 2);
    if (!bh) {
        printk("failed reading buffer 1 for sb\n");
    }

    s_es = (ext2_super_block*)bh->b_data;
    copy_ext2_sb_from_mem(ext2_sb, s_es);

    bwrite(bh);

    brelse(bh);
}

void test_fs(void)
{
    printk("testing fs\n");
    struct buffer_head *bh;
    bh = bread(DEV_NO, 1);
    if (!bh) {
        printk("failed reading buffer 1 for sb\n");
    }
    ext2_super_block *sb = (ext2_super_block*)bh->b_data;
    /* ext2_super_block sb; */
    /* uint8_t sb_buffer[EXT2_BLK_SIZE]; */
    /* disk_read(2, sb_buffer); */
    /* memcpy(&sb, sb_buffer, sizeof(ext2_super_block)); */
    display_ext2_super_block(sb);

    brelse(bh);

    printk("Creating test file...\n");
    create_test_file();
    
    printk("Reading back test file...\n");
    print_file("test.txt");
}

void ext2_fs_init(void)
{
    
    struct ext2_sb_info *ext2_sb; //in memory super-block data
    ext2_sb = kmalloc(sizeof(struct ext2_sb_info), 0);
    if (!ext2_sb) {
        printk("failed kmalloc for ext2_sb");
        return;
    }
    read_sb(ext2_sb);

    printk("read data =================== from sb \n");
    printk("free inodes %d\n", ext2_sb->s_freeinodes_counter);

    /* ext2_sb->s_freeinodes_counter--; */

    write_sb(ext2_sb);

    test_fs();
}

/* ext2 superblock operations table */
struct super_operations ext2_sops = {
    .alloc_inode = NULL,
    .destroy_inode = NULL,
    .write_inode = ext2_write_inode,
    .read_inode = ext2_read_inode,
    .put_inode = NULL,
    .drop_inode = NULL,
    .delete_inode = ext2_delete_inode,
    .put_super = ext2_put_super,
    .write_super = ext2_write_super,
    .sync_fs = ext2_sync_fs,
};

/* Mount ext2 filesystem */
struct super_block *ext2_read_super(struct super_block *sb, void *data, int silent)
{
    struct ext2_sb_info *sbi;
    ext2_super_block *es;
    struct buffer_head *bh;
    struct inode *root_inode;
    struct dentry *root_dentry;
    
    if (!sb) {
        return NULL;
    }
    
    printk("ext2_read_super: mounting ext2 filesystem\n");
    
    /* Allocate ext2-specific superblock info */
    sbi = (struct ext2_sb_info *)kmalloc(sizeof(struct ext2_sb_info), 0);
    if (!sbi) {
        printk("ext2_read_super: failed to allocate sbi\n");
        return NULL;
    }
    
    /* Read superblock from disk */
    bh = bread(DEV_NO, 1);  /* Superblock is at block 1 (offset 1024) */
    if (!bh) {
        printk("ext2_read_super: failed to read superblock\n");
        kfree(sbi);
        return NULL;
    }
    
    es = (ext2_super_block *)bh->b_data;
    
    /* Verify magic number */
    if (es->s_magic != EXT2_SUPER_MAGIC) {
        printk("ext2_read_super: invalid magic number 0x%x\n", es->s_magic);
        brelse(bh);
        kfree(sbi);
        return NULL;
    }
    
    /* Register superblock with filesystem type */
    struct file_system_type *fst = get_fs_type("ext2");
    if (fst) {
        list_add(&sb->s_list, &fst->fs_supers);
    }
    
    /* Copy superblock info to memory */
    copy_ext2_sb_to_mem(sbi, es);
    
    /* Initialize VFS superblock */
    sb->s_blocksize = EXT2_BLK_SIZE;
    sb->s_magic = EXT2_SUPER_MAGIC;
    sb->s_op = &ext2_sops;
    sb->s_fs_info = sbi;
    sb->s_dev = DEV_NO;
    
    /* Keep superblock buffer */
    sbi->s_sbh = bh;
    sbi->s_es = es;
    
    /* Get root inode */
    root_inode = iget(DEV_NO, EXT2_ROOT_INO);
    if (!root_inode) {
        printk("ext2_read_super: failed to get root inode\n");
        brelse(bh);
        kfree(sbi);
        return NULL;
    }
    
    /* Read root inode from disk */
    ext2_read_inode(root_inode);
    
    /* Create root dentry */
    root_dentry = d_alloc_root(root_inode);
    if (!root_dentry) {
        printk("ext2_read_super: failed to allocate root dentry\n");
        brelse(bh);
        kfree(sbi);
        return NULL;
    }
    
    sb->s_root = root_dentry;
    
    printk("ext2_read_super: mount successful\n");
    display_ext2_super_block(es);
    
    return sb;
}

/* Unmount ext2 filesystem */
void ext2_put_super(struct super_block *sb)
{
    struct ext2_sb_info *sbi;
    
    if (!sb) {
        return;
    }
    
    printk("ext2_put_super: unmounting ext2 filesystem\n");
    
    sbi = EXT2_SB(sb);
    if (sbi) {
        /* Write superblock if dirty */
        if (sb->s_dirt) {
            ext2_write_super(sb);
        }
        
        /* Release superblock buffer */
        if (sbi->s_sbh) {
            brelse(sbi->s_sbh);
        }
        
        /* Free ext2-specific info */
        kfree(sbi);
        sb->s_fs_info = NULL;
    }
}

/* ext2 filesystem type */
struct file_system_type ext2_fs_type = {
    .name = "ext2",
    .fs_flags = 0,
    .read_super = ext2_read_super,
};

/* Register ext2 filesystem */
int ext2_register_filesystem(void)
{
    return register_filesystem(&ext2_fs_type);
}

/* Initialize ext2 filesystem structures */
int ext2_init_fs(void)
{
    printk("ext2_init_fs: initializing ext2 filesystem\\n");
    
    /* Register ext2 filesystem type */
    if (ext2_register_filesystem() != 0) {
        printk("ext2_init_fs: failed to register ext2 filesystem\\n");
        return -1;
    }
    
    /* Initialize dentry cache */
    dcache_init();
    
    printk("ext2_init_fs: ext2 filesystem registered\\n");
    return 0;
}

/* Mount root filesystem */
int ext2_mount_root(void)
{
    struct super_block *sb;
    struct inode *root_inode;
    
    printk("ext2_mount_root: mounting ext2 root filesystem\\n");
    
    /* Read filesystem metadata */
    read_fs(0);  /* 0 MB offset */
    
    /* Allocate superblock */
    sb = alloc_super();
    if (!sb) {
        printk("ext2_mount_root: failed to allocate superblock\\n");
        return -1;
    }
    
    /* Fill superblock by reading from disk */
    if (!ext2_read_super(sb, NULL, 0)) {
        printk("ext2_mount_root: failed to read superblock\\n");
        destroy_super(sb);
        return -1;
    }
    
    /* Get root inode */
    root_inode = sb->s_root->d_inode;
    if (!root_inode) {
        printk("ext2_mount_root: failed to get root inode\\n");
        destroy_super(sb);
        return -1;
    }
    
    /* Set current process root and pwd */
    if (current) {
        current->root = root_inode;
        current->pwd = root_inode;
        root_inode->i_count += 2;  /* Two references */
    }
    
    printk("ext2_mount_root: root filesystem mounted successfully\\n");
    printk("ext2_mount_root: root inode = %u, size = %u\\n", 
           root_inode->i_no, root_inode->i_size);
    
    /* Display root directory contents */
    if (root_inode->i_size > 0) {
        printk("ext2_mount_root: root directory contents:\\n");
        
        /* Read first block of root directory */
        inode_t disk_inode = get_inode(EXT2_ROOT_INO);
        if (disk_inode.i_blocks > 0) {
            uint32_t dir_block = disk_inode.i_block[0];
            struct buffer_head *bh = bread(DEV_NO, dir_block);
            if (bh) {
                dentry_t *entries = (dentry_t *)bh->b_data;
                uint32_t num_entries = disk_inode.i_size / sizeof(dentry_t);
                
                for (uint32_t i = 0; i < num_entries && i < 10; i++) {
                    if (entries[i].inode != 0) {
                        printk("  entry %u: inode=%u, name='%s', type=%u\\n",
                               i, entries[i].inode, entries[i].name, entries[i].file_type);
                    }
                }
                brelse(bh);
            }
        }
    }
    
    return 0;
}

/* Sync filesystem - flush dirty buffers and metadata to disk */
void sync(void)
{
    printk("sync: flushing filesystem to disk\n");
    
    /* Write superblock */
    disk_sync_super();
    
    /* Write block group descriptor table */
    disk_sync_bgdt();
    
    /* TODO: Flush all dirty inodes and buffer cache */
    printk("sync: filesystem synced\n");
}

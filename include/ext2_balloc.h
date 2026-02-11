#ifndef _EXT2_BALLOC_H
#define _EXT2_BALLOC_H

#include <stdint.h>
#include "fs.h"

/* Block allocation and bitmap management */
uint32_t first_free_bm(uint8_t *bitmap, uint8_t allowed_start);
uint32_t first_free_block(bgdesc_t *d);
uint32_t first_free_inode(bgdesc_t *d);
int first_free_block_num(uint32_t *res);
int first_free_inode_num(uint32_t *res);

/* Bitmap operations */
void set_bit(uint8_t *bitmap, uint16_t i);
void unset_bit(uint8_t *bitmap, uint16_t i);
void mark_block(uint32_t block_num, uint8_t val);
void set_block_bitmap(uint32_t block_num);
void unset_block_bitmap(uint32_t block_num);
void mark_inode(uint32_t inode_num, uint8_t val);
void set_inode_bitmap(uint32_t inode_num);
void unset_inode_bitmap(uint32_t inode_num);

/* Block/inode reservation */
int reserve_free_block(uint32_t *block_n);
int reserve_free_inode(uint32_t *inode_n);
int reserve_inode(uint32_t inode_n);

/* Block group descriptor operations */
void update_inode_bg_desc(uint32_t free_inode_n);
void update_block_bg_desc(uint32_t free_block_n);
void disk_sync_bgdt(void);

/* Superblock sync */
void disk_sync_super(void);

/* Access to global data */
extern bgdesc_t *get_bgdt(void);
extern ext2_super_block *get_ext2_super(void);
extern uint16_t get_n_block_groups(void);

#endif /* _EXT2_BALLOC_H */

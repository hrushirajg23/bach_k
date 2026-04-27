#ifndef _DISK_H
#define _DISK_H

#include <stdint.h>

#define SECTOR_CHUNK        0xff
#define DISK_SECTOR_SIZE    512 // matches ATA_SECTOR_SIZE in disk.c

// disk commands
void disk_write(uint32_t lba, uint8_t *buf, uint32_t nchar);
void disk_read(uint32_t lba, uint8_t *buf);
void disk_read_bootloader(uint32_t lba, uint8_t *buf, uint8_t chunk);

void test_disk(void);
int disk_read_blk(uint32_t block_num, uint8_t *buf);
int disk_write_blk(uint32_t block_num, uint8_t *buf);
#endif

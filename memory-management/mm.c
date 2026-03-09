#include <stddef.h>
#include <stdint.h>

#include "list.h"
#include "serial.h"
#include "utils.h"
#include "zone.h"
#include "multiboot.h"
#include "mm.h"
#include "manager.h"
#include "allocator.h"
#include "slab.h"
#include "string.h"

struct page *mem_map = NULL;
unsigned long swapper_pg_dir[PG_DIR_ENTRIES] __attribute__((aligned(4096)));

zone_t zone;
struct phy_layout phy_layout;
void print_mem_map(void);
extern unsigned long confirm_paging(void);

/*
 * I've mapped 0xC0000000 similar to 0x00000000 entries
 * Linux did the same thing, referred: understanding the linux kernel chapter 2
 */
/* struct page *virt_to_page(void *addr) */
/* { */
/*     unsigned int index; */
/*     if ((unsigned long)addr < PAGE_OFFSET) */
/*         index = ((unsigned long)addr) >> PAGE_SHIFT; */
/*     else */
/*         index = ((unsigned long)addr - PAGE_OFFSET) >> PAGE_SHIFT; */
/*      printk("index of va to page is %x\n", index); */
/*     return &zone.zone_mem_map[index]; */
/* } */


unsigned long page_to_phys(struct page *page)
{
    unsigned long phy_addr = (page - page->zone->zone_mem_map) * PAGE_SIZE;
    return phy_addr;
}

void *memcpy(void *dest, const void *src, unsigned int size)
{
    char *psrc = (char *)src;
    char *pdest = (char *)dest;
    unsigned int iCnt = 0;
    for (iCnt = 0; iCnt < size; iCnt++) {
        pdest[iCnt] = psrc[iCnt];
    }
    return dest;
}

void copy_mem_map(void)
{
    int iCnt = 0;
    unsigned long phy_addr = 0;
    unsigned long phy_start = 0;
    unsigned int page_req = 0;
    page_req = ((sizeof(struct page) * phy_layout.totalram_pages) / PAGE_SIZE) + 1;
    printk("\npages req by mem_map are: %x", page_req);
    for (iCnt = 0; iCnt < page_req; iCnt++) {
        phy_addr = get_free_page();
        if (iCnt == 0) {
            phy_start = phy_addr;
        }
    }
    memcpy((void *)phy_start, mem_map, (sizeof(struct page)) * phy_layout.totalram_pages);

    /* print_mem_map(); */

    kbfree(mem_map);

    mem_map = (struct page *)phy_start;

    /* free the conventional_memory */

    page_req = phy_layout.max_low_pfn;

    for (iCnt = 0; iCnt < page_req; iCnt++) {
        CLEAR_FLAG(mem_map[iCnt].flags, PG_FLAG_TAKEN);
        CLEAR_FLAG(mem_map[iCnt].flags, PG_FLAG_RESERVED);
    }

    /* print_mem_map(); */
}

/* void memset(void *addr, char val, unsigned int size) */
/* { */
/*     char *ptr = (char *)addr; */
/*     register int iCnt = 0; */
/*     while (iCnt < size) { */
/*         ptr[iCnt++] = val; */
/*     } */
/* } */


void print_machine_map(void)
{
    printk("\n highest_usable_pfn: 0x%x", phy_layout.highest_usable_pfn);
    printk("\n total_ram_pages: 0x%x", phy_layout.totalram_pages);
    printk("\n max_pfn: 0x%x", phy_layout.max_pfn);
    printk("\n max_low_pfn: 0x%x", phy_layout.max_low_pfn);
    printk("\n high_start_pfn: 0x%x", phy_layout.highstart_pfn);
    printk("\n high_end_pfn: 0x%x", phy_layout.highend_pfn);
    printk("\n total_high_pages: 0x%x", phy_layout.totalhigh_pages);
}

void machine_specific_memory_setup(multiboot_info_t *mbi, memory_blocks_t *blocks, int num_blocks)
{
    unsigned long total_ram_bytes = (mbi->mem_upper * 1024 + (1024 * 1024));  // mem_upper in KB
    unsigned long total_ram_pfn   = total_ram_bytes / PAGE_SIZE;
    unsigned long lowmem_limit_pfn = ((mbi->mem_lower * 1024) / PAGE_SIZE);

    phy_layout.highest_usable_pfn = (mbi->mem_upper * 1024) / PAGE_SIZE;
    phy_layout.totalram_pages = total_ram_pfn;
    phy_layout.max_pfn = phy_layout.highest_usable_pfn;
    // pick the smaller one, karan ram 896mb peksha kami asel tar
    if (total_ram_pfn < lowmem_limit_pfn)
        phy_layout.max_low_pfn = total_ram_pfn;
    else
        phy_layout.max_low_pfn = lowmem_limit_pfn;

    phy_layout.highstart_pfn = phy_layout.max_low_pfn;
    phy_layout.highend_pfn = phy_layout.max_pfn;
    phy_layout.totalhigh_pages = phy_layout.highend_pfn - phy_layout.highstart_pfn;

    print_machine_map();
}

void init_page(struct page *page)
{
    memset(page, 0, sizeof(struct page));
}

void create_zone(zone_t *zone)
{
    register int iCnt = 0;
    free_area_t *free_area = zone->free_area;
    zone->present_pages = phy_layout.totalram_pages;
    for (iCnt = 0; iCnt < zone->present_pages; iCnt++) {
        zone->free_pages++;
    }
    zone->zone_mem_map = mem_map;
    zone->pages_min = ZONE_WATERMARK;
    zone->pages_low = ZONE_WATERMARK;
    zone->zone_start_pfn = 0; //0 since I have only single zone for now

    for (iCnt = 0; iCnt < BUDDY_GROUPS; iCnt++) {
        INIT_LIST_HEAD(&free_area[iCnt].free_list);
    }
}
/*
 * this function calculates the number of 
 * page tables needed to map the whole memory
 */
unsigned int count_pgdir_entries(zone_t *zone)
{
    unsigned long zone_mem = zone->present_pages * PAGE_SIZE;
    unsigned int pgtbl_size = PAGE_SIZE * PG_TABLE_ENTRIES;

    return zone_mem / pgtbl_size;
}

unsigned long get_free_page(void)
{
    register int iCnt = 0;
    for (iCnt = 0; iCnt < phy_layout.totalram_pages; iCnt++) {
        if (!(mem_map[iCnt].flags & PG_FLAG_TAKEN)) {
            mem_map[iCnt].flags |= PG_FLAG_TAKEN;
            return (unsigned long)(iCnt * PAGE_SIZE);
        }
    }
    return 0;
}

void release_page(unsigned long phy_addr)
{
    struct page *page = &zone.zone_mem_map[phy_addr / PAGE_SIZE];
    CLEAR_FLAG(page->flags, PG_FLAG_TAKEN);
}

void print_mem_map(void)
{
    register int iCnt = 0;
    for (iCnt = 0; iCnt < phy_layout.totalram_pages; iCnt++) {
        printk("\n iCnt : %x\tpage->page_no: %d, page->private: %d, status: ", iCnt, mem_map[iCnt].page_no, mem_map[iCnt].private);
        if ((mem_map[iCnt].flags & PG_FLAG_RESERVED) == PG_FLAG_RESERVED) {
            printk("reserved, ");
        }
        if ((mem_map[iCnt].flags & PG_FLAG_TAKEN) == PG_FLAG_TAKEN) {
            printk("taken, ");
        }
        else {
            printk("free, ");
        }
    }
}

int create_mem_map(multiboot_info_t *mbi)
{
    register unsigned int iCnt = 0;
    unsigned long phy_start = 0, phy_end = 0;
    unsigned int page_start = 0, page_end = 0;
    mem_map = (struct page *)kalloc(phy_layout.totalram_pages * sizeof(struct page));
    if (!mem_map) {
        printk("fail mem_map allocation\n");
        return 0;
    }

    memset(mem_map, 0, phy_layout.totalram_pages * sizeof(struct page));
    printk("\nmem_map starts at addr :%p", mem_map);

    for (iCnt = 0; iCnt < phy_layout.totalram_pages; iCnt++) {
        mem_map[iCnt].flags |= (PG_FLAG_TAKEN | PG_FLAG_RESERVED);
        mem_map[iCnt].page_no = iCnt;
    }

    /*
     * mem_map's data is present in the lower 640kb . Hence we don't want to wipe it off.
     * Mark the whole 640kb of pages as PG_TAKEN , since you don't want to replace the mem_map data 
     * structure. Once initialzed copy the mem_map to some_where up via page_allocator and free the existing pages 
     * in lower 640kb memory
     */
    for (iCnt = 0; iCnt < MAX_MEMORY_BLOCKS; iCnt++) {
        if (free_memory_blocks[iCnt].start == 0 || free_memory_blocks[iCnt].end == 0)
            continue;

        phy_start = free_memory_blocks[iCnt].start;
        printk("\ncurr aligned phy_start: \t%x", phy_start);

        if (!IS_PAGE_ALIGNED(phy_start)) {
            phy_start = ALIGN_PAGE(phy_start) + PAGE_SIZE;
        }
        printk("\nnew aligned phy_start: \t%x", phy_start);

        page_start = phy_start / PAGE_SIZE;
        printk("\nnew aligned page_start: \t%x", page_start);

        /*
         * For example free_block end is 4120 
         * so 0 - 4095 is free but , for next page 4096 - 8191
         * only till 4120 is free, hence avoid that page, 
         * page_end will the the last free_page */

        phy_end = free_memory_blocks[iCnt].end;
        printk("\ncurr aligned phy_end: \t%x", phy_end);

        if (!IS_PAGE_ALIGNED(phy_end)) {
            phy_end = ALIGN_PAGE(phy_end) + PAGE_SIZE - 1;
        }
        printk("\nnew aligned phy_end: \t%x", phy_end);

        page_end = (phy_end - 1) / PAGE_SIZE;
        printk("\nnew aligned page_end: \t%x", page_end);

        for (iCnt = page_start; iCnt <= page_end; iCnt++) {
            mem_map[iCnt].flags &= ~PG_FLAG_TAKEN;
            mem_map[iCnt].flags &= ~PG_FLAG_RESERVED;
        }
    }

    /* print_mem_map(); */
    return 1;
}

unsigned int pgd_index(unsigned int phy_addr)
{
    return (phy_addr / (PG_DIR_ENTRIES * PAGE_SIZE));
}

unsigned long test_mmu(void)
{
    unsigned long iRet = 0;
    struct page *page = NULL;

    printk("\ntesting mmu \n");
    unsigned long phy_addr = get_free_page();

    printk("%x", phy_addr);

    *(unsigned long *)phy_addr = 0x5555;

    printk("\n page no is  : %x", phy_addr / PAGE_SIZE);

    page = &zone.zone_mem_map[phy_addr / PAGE_SIZE];
    printk("\npage->flags : ");
    if ((page->flags & PG_FLAG_TAKEN)) {
        printk("page is taken");
        iRet = 1;
    }
    else {
        printk("page is free\n");
    }


    /*
     * Below code gives the proof that mmu is working correctly 
     * i.e translation HIGH addresses from 0xC0000000 correctly 
     */
    void *ptr = ((void *)(PAGE_OFFSET + phy_addr));
    printk("\nva value is %x\n", *(unsigned int *)ptr);

    ptr = ((void *)phy_addr);
    printk("\npa value is %x\n", *(unsigned int *)ptr);

    release_page(phy_addr);

    printk("after release page->flags : ");
    if ((page->flags & PG_FLAG_TAKEN)) {
        printk("page is taken");
    }
    else {
        iRet = 1;
        printk("page is free\n");
    }
    /* below two functions should produce the same 
     * result i.e page number 
     */
    /*        ------------------------ */
    page = virt_to_page((void *)0xC0002001);
    printk("va to page is %x\n", page->page_no);

    page = phys_to_page((unsigned long)0x00002001);
    printk("pa to page is %x\n", page->page_no);
    /*        ------------------------ */
    return iRet;
}
/*
 * The pages containing page tables start at 1mb */
void setup_paging(unsigned int pgdir_entries)
{
    register int iCnt = 0, jCnt = 0, kCnt = 0;
    unsigned long phy_addr = 0x00000000;
    unsigned int pgd = pgd_index((unsigned int)PAGE_OFFSET);
    unsigned long temp_addr = 0;
    unsigned long *pg_ptr = NULL;
    unsigned long total_ram = phy_layout.totalram_pages * PAGE_SIZE;

    printk("\n pgd is : %x", pgd);

    for (iCnt = 0; iCnt < pgdir_entries; iCnt++) {
        temp_addr = get_free_page();
        swapper_pg_dir[pgd + iCnt] = (temp_addr & 0xFFFFF000) | PG_PRESENT | PG_RW | PG_USER;
        swapper_pg_dir[iCnt] = (temp_addr & 0xFFFFF000) | PG_PRESENT | PG_RW | PG_USER;
        pg_ptr = (unsigned long *)temp_addr;

        /* printk("\npg_dir entry : %d", iCnt); */
        /* printk("\npage_table address : %p", pg_ptr); */
        jCnt = 0;
        while (phy_addr < total_ram && jCnt < PG_TABLE_ENTRIES) {
            pg_ptr[jCnt] = (phy_addr & 0xFFFFF000) | PG_PRESENT | PG_RW | PG_USER;

            /* printk("\npage_table_entry : %d\t val: %x", jCnt, pg_ptr[jCnt]); */
            jCnt++;
            phy_addr += PAGE_SIZE;
        }
    }
    printk("\nswapper_pg_dir addr is : %p", (void *)swapper_pg_dir);
    enable_paging((unsigned long)swapper_pg_dir);

    temp_addr = confirm_paging();
    if ((temp_addr & 0x80000000) == 0x80000000) {
        printk("\npaging enabled, cr0:  %x", temp_addr);
    }
    else {
        printk("\n couldn't enable paging %x", temp_addr);
    }

    temp_addr = test_mmu();
    if (temp_addr) {
        printk("\n mmu working \n");
    }
    else {
        printk("\n mmu not working correctly \n");
    }
}


void init_mem(multiboot_info_t *mbi)
{
    register int iCnt = 0, jCnt = 0;
    unsigned int pgdir_entries = 0;
    int iRet = 0;

    iRet = create_mem_map(mbi);
    if (!iRet) {
        printk("create mem_map fail\n");
        return;
    }

    copy_mem_map();

    create_zone(&zone);

    pgdir_entries = count_pgdir_entries(&zone);
    printk("\npgdir_entires are : %x", pgdir_entries);

    setup_paging(pgdir_entries);

    init_zone(&zone);

    init_slab();
    
    test_slab();

    show_buddy(&zone);

    /* struct page *p1 = &zone.zone_mem_map[2]; */
    /* void *addr1 = page_address(&zone.zone_mem_map[5]); */
    /* *(int*)addr1 = 100; */
    /* printk("page_address test addr is %p, val: %d\n", addr1, *(int*)addr1); */

    /* unsigned long addr2 = __pa(addr1); */
    /* printk("page_address test addr is %x, val: %d\n", addr2, *(int*)addr2); */
   

    /* print_mem_map(); */
}

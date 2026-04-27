#ifndef _MM_H
#define _MM_H

#include "zone.h"
#include "page.h"

#define ALIGN_PAGE(address) (address - (address % PAGE_SIZE))
#define IS_PAGE_ALIGNED(address) (((address) % PAGE_SIZE) == 0)

#define CLEAR_FLAG(var, flag) ((var &= ~(flag)))
#define SET_FLAG(var, flag) ((var |= (flag)))
#define IS_FLAG(var, flag)      (((var) & (flag)) != 0)


void memset(void *addr, char val, unsigned int size);
void *memcpy(void *dest, const void *src, unsigned int size);
void init_mem(multiboot_info_t *);


static inline struct page *compound_head(struct page *page)
{
	// if (unlikely(PageTail(page)))
		return page->first_page;
	// return page;
}

static inline struct page *virt_to_head_page(const void *x)
{
	struct page *page = virt_to_page(x);
	return compound_head(page);
}

// #define page_address(page) (void*)((page - page->zone->zone_mem_map)* (PAGE_SIZE))

// #define page_address(page) \
//     ((void *)((((struct page*)page - page->zone->zone_mem_map) + page->zone->zone_start_pfn) \
//               << PAGE_SHIFT))
#define page_to_pfn(page) \
    ((page) - mem_map)

#define page_address(page) \
    __va(page_to_pfn(page) << PAGE_SHIFT)

// void *page_address(struct page *page)
// {
//     return (void *)((page - page->zone->zone_mem_map) * PAGE_SIZE);
// }

#endif

#include "slab.h"
#include "serial.h"
#include "kernel.h"
#include "page.h"

#define BYTES_PER_WORD      sizeof(void *)
#define MAX_MALLOC_SIZE     131072UL
/* #define KMALLOC_FLAGS       SLAB_HWCACHE_ALIGN */
/* #define KMALLOC_MINALIGN    __alignof__(unsigned long long) */

extern zone_t zone;
kmem_cache_t cache_cache;
static struct list_head cache_chain; //list of kmem_cache structures
void run_slab_tests(void);

struct cache_names {
    char *name;
    /* char *name_dma; */
};
typedef struct cache_sizes {
    size_t cs_size;
    kmem_cache_t *cs_cachep;
    /* kmem_cache_t *cs_dmacachep; */
} cache_sizes_t;

struct cache_sizes malloc_sizes[] = {
#define CACHE(x) { .cs_size = (x) },
    CACHE(32)
    CACHE(64)
    CACHE(96)
    CACHE(128)
    CACHE(192)
    CACHE(256)
    CACHE(512)
    CACHE(1024)
    CACHE(2048)
    CACHE(4096)
    CACHE(8192)
    CACHE(16384)
    CACHE(32768)
    CACHE(65536)
    CACHE(131072)
    /* CACHE(ULONG_MAX) */
    { .cs_size = 0 } //sentinel value to stop loop
#undef CACHE
};

#define MALLOC_SIZES_COUNT  (sizeof(malloc_sizes) / sizeof(malloc_sizes[0]))

static struct cache_names cache_names[] = {
#define CACHE(x) { .name = "size-" #x},
    CACHE(32)
    CACHE(64)
    CACHE(96)
    CACHE(128)
    CACHE(192)
    CACHE(256)
    CACHE(512)
    CACHE(1024)
    CACHE(2048)
    CACHE(4096)
    CACHE(8192)
    CACHE(16384)
    CACHE(32768)
    CACHE(65536)
    CACHE(131072)
    {NULL,}
#undef CACHE
};

/* void *page_address(struct page *page) */
/* { */
/*     return (void *)((page - page->zone->zone_mem_map) * PAGE_SIZE); */
/* } */

static inline void page_set_cache(struct page *page, struct kmem_cache *cachep)
{
    page->lru.next = (struct list_head *)cachep;
}

static inline struct kmem_cache *page_get_cache(struct page *page)
{
    page = compound_head(page);
    return (struct kmem_cache *)page->lru.next;
}

static inline void page_set_slab(struct page *page, struct slab_s *slabp)
{
    page->lru.prev = (struct list_head *)slabp;
}

static inline struct slab_s *page_get_slab(struct page *page)
{
    return (struct slab_s *)page->lru.prev;
}

static inline struct kmem_cache *virt_to_cache(const void *obj)
{
    struct page *page = virt_to_head_page(obj);
    return page_get_cache(page);
}

static inline struct slab_s *virt_to_slab(const void *obj)
{
    struct page *page = virt_to_head_page(obj);
    return page_get_slab(page);
}

/*
 * This function interfaces the slab allocator with Zoned
 * Page Frame Allocator
 * When slab allocator creates a new slab , it gets pages 
 * from the zoned page frame allocator by obtaining a group of
 * contiguous page frames;
 */
void *kmem_getpages(kmem_cache_t *cachep, int flags)
{
    struct page *page = NULL;
    struct page *temp = NULL;
    int i = 0;

    /* printk("kmem_getpages called for : %x\n", cachep->gfporder); */

    flags |= cachep->gfpflags;
    page = alloc_pages(flags, cachep->gfporder);
    if (!page)
        return NULL;
    i = (1 << cachep->gfporder);
    /* printk("page no is : %x\n", page->page_no); */
    /*
     * SLAB_RECLAIM account code
     */
    temp = page;
    while (i--)
        SetPageSlab(temp++);

    return page_address(page);
}

void kmem_freepages(kmem_cache_t *cachep, void *addr)
{
    unsigned long i = (1 << cachep->gfporder);
    struct page *page = virt_to_page(addr);

    /*
     * reclaim code
     */

    /* printk("freeing address : %p\n", addr); */
    /* printk("freeing page no: %x\n", page->page_no); */
    struct page *temp = page;
    while (i--)
        ClearPageSlab(temp++);
    free_pages(page, cachep->gfporder);

    /*
     * reclaim code
     */
}

static void kmem_list3_init(struct kmem_list3 *parent)
{
    INIT_LIST_HEAD(&parent->slabs_full);
    INIT_LIST_HEAD(&parent->slabs_partial);
    INIT_LIST_HEAD(&parent->slabs_free);
    parent->free_objects = 0;
}


/*
 * Remember there are three main kmem_caches
 * 1. The cache containing objects of type kmem_cache 
 * 2. The cache containing objects of type struct slab_s (i.e for external slab descriptors)
 * 3. The cache containing objects of type struct kmem_bufctl ( i.e external object descriptors ) 
 * So the object descriptors must be immediately after the slab descriptor
 *
 * |--------|----------|
 * | slab   |obj descs |
 * | desc   |          | 
 * |        |          |
 * |--------|----------| 
*/

/*
 * This function initializes the first general purpose cache
 * who objects are cache_descriptors themselves, since they're freq
 * needed. 
 */
void kmem_cache_init(kmem_cache_t *cachep, const char *name, size_t objsize,
                     void (*ctor)(void *, kmem_cache_t *, unsigned long),
                     void (*dtor)(void *, kmem_cache_t *, unsigned long))
{
    unsigned int left_over;
    int i, order;
    cache_sizes_t *sizes;
    struct cache_names *names;

    INIT_LIST_HEAD(&cache_chain);
    INIT_LIST_HEAD(&cache_cache.next);
    list_add(&cache_chain, &cache_cache.next);
    cachep->colour_off = cache_line_size();
    /* printk("\n initializing slab\n"); */
    /*
     * The offset is in units of BYTES PER WORD unless 
     * SLAB HWCACHE ALIGN is set, in which case it is aligned to blocks of 
     * L1 CACHE BYTES for alignment to the L1 hardware cache
     * The objects are aligned int BYTES_PER_WORD (4) normally
     * but if SLAB_HWCACHE_ALIGN is set, they're aligned to 
     * L1_CACHE_BYTES (32 bytes in our case) , for faster performance.
     */
    cachep->objsize = ALIGN(objsize, cache_line_size());
    cachep->name = name;
    kmem_list3_init(&cachep->lists);
    cachep->ctor = ctor;
    cachep->dtor = dtor;


    for (order = 0; order < 11; order++) {
        cache_estimate(order, cachep->objsize,
                       cache_line_size(), 0, &left_over, &cachep->num);
        if (cachep->num)
            break;
    }

    /* printk("cache line size is %x", cache_line_size()); */

    cachep->gfporder = order;
    /* printk("left over is :%d", left_over); */

    /*
     * left_over is the free_space availabe in the memory allocated 
     * for objects.
     * The number of available colors is
     * free⁄aln (this value is stored in the colour field of the cache 
     * descriptor). Thus, the first color is denoted as 0 and the last one 
     * is denoted as (free⁄aln)−1. (As a particular case, if free is lower 
     * than aln, colour is set to 0, nevertheless all slabs use color 0, thus
     * really the number of colors is one.)
     */

    /*
     * In the case of the kmem_cache i.e cache_cache
     * each objects is of type kmem_cache i.e ( 100 bytes )
     * so aligning it with L1_CACHE_BYTES(32) it becomes 128
     * Hence size of each objects is now 128 bytes.
     */
    /*
     * colour : total of the number of colours that can fit in the slab
     * colour_next : current colour to be used ( 0 <= colour )
     * colour_off : (aln) basic alignment offset in slabs
     */
    /*
     * The cache_grow() function assigns the color specified 
     * by colour_next to a new slab and then increases the value 
     * of this field. After reaching colour, it wraps around again to 0.
     * In this way, each slab is created with a different color from the 
     * previous one, up to the maximum available colors.
     */

    cachep->colour = left_over / cachep->colour_off;
    cachep->colour_next = 0;
    cachep->slab_size = ALIGN(sizeof(slab_t), cache_line_size());
    cachep->free_limit = cachep->num;

   
    // Create a cache of slab descriptors,
    cachep->slabp_cache = kmem_cache_create("slab_cache", sizeof(struct slab_s), KMALLOC_MINALIGN, SLAB_HWCACHE_ALIGN, NULL);
    if (!cachep->slabp_cache)
        printk("creation of slab cache failed\n");
    /*
    * Now below we create the general geometric size caches
    * 32 to 131,072 
    */
    sizes = malloc_sizes;
    names = cache_names;

    for (i = 0; sizes[i].cs_size != 0; i++) {
        sizes[i].cs_cachep = kmem_cache_create(names[i].name, sizes[i].cs_size, KMALLOC_MINALIGN, CFLGS_OFF_SLAB | KMALLOC_FLAGS, NULL);
        if (!sizes[i].cs_cachep) {
            printk("\nkmem_cache_create failed for i %d", i);
            break;
        }
    }
}

static size_t slab_mgmt_size(size_t align)
{
    return ALIGN(sizeof(struct slab_s), align);
}

void cache_estimate(int gfporder, unsigned int objsize, unsigned int align, int flags, unsigned int *left_over, unsigned int *num)
{
    int nr_objs;
    unsigned int mgmt_size;
    unsigned int slab_size = PAGE_SIZE << gfporder;

    /* printk("\ncache estimation gfp order: \n%x", gfporder); */
    /* printk("\nobjsize: \n%x", objsize); */

    if (flags & CFLGS_OFF_SLAB) {
        mgmt_size = 0;
        nr_objs = slab_size / objsize;

        /* if (nr_objs > SLAB_LIMIT) */
        /*     nr_objs = SLAB_LIMIT; */
    }
    else {
        nr_objs = (slab_size - sizeof(slab_t)) / objsize;

        if ((slab_mgmt_size(align) + nr_objs * objsize) > slab_size)
            nr_objs--;
        /* if (nr_objs > SLAB_LIMIT) */
        /*     nr_objs = SLAB_LIMIT; */

        mgmt_size = slab_mgmt_size(align);
    }
    /* printk("\n nr_objs: \n%x", nr_objs); */

    *num = nr_objs;
    *left_over = slab_size - nr_objs * objsize - mgmt_size;
}

void cache_init_objs(kmem_cache_t *cachep, slab_t *slabp)
{
    slabp->free_list = NULL;

    char *ptr = (char *)slabp->s_mem;
    /* printk("slab s_mem is\n%x\n", slabp->s_mem); */
    /* printk("number of objects are \n%d\n", cachep->num); */
    /* printk("object size: %d\n", cachep->objsize); */
    /* printk("cachep->name: %s\n", cachep->name); */

    for (int i = cachep->num - 1; i >= 0; i--) {
        void *objp = ptr + i * cachep->objsize;
        /* now we form a free list by inserting 
         * value of next free node into prev node
         */
        /* obj0 -> obj1 -> .. -> NULL */

        /* printk("object no: %d\n", i); */
        /* printk("objp : %p\n", objp); */

        *(void **)objp = slabp->free_list;
        /* printk("*objp is : %p\n", *(void **)objp); */

        slabp->free_list = objp;
        if (cachep->ctor) {
            (cachep->ctor)(objp, cachep, 0);
        }
    }

    /* printk("after init objs free list points to : %p\n", slabp->free_list); */
}

/*
 *
 * The lru field of the page desc is now used for storing the 
 * kmem_cache and slab descriptor.
 * The lru.prev is set to slab and
 * lru.next is set to kmem_cache desc to which the page is associated.
 */
static void slab_map_pages(kmem_cache_t *cachep, struct slab_s *slabp,
                           void *objp)
{
    int nr_pages;
    struct page *page;

    page = virt_to_page(objp);
    nr_pages = 1;
    nr_pages <<= cachep->gfporder;

    struct page *first = page;
    /*
     * first page to get its head */
    do {
        page->first_page = first;
        page_set_cache(page, cachep);
        page_set_slab(page, slabp);
        page++;
    } while (--nr_pages);
}

/*
 * cachep: pointer to cache
 * objp: pointer to allocated object memory via kmem_getpages
 * colour_off: offset from where the object/(slab if internal slab) will start
 * flags: 
 */
static struct slab_s *alloc_slabmgmt(kmem_cache_t *cachep, void *objp,
                                     int colour_off, int flags)
{
    struct slab_s *slabp;

    if (OFF_SLAB(cachep)) {
        /*
         * blah blah allocate memory 
         * from generic caches
         */
        /* printk(" off slab mgmt for cache: %s\n", cachep->name); */ 
        slabp = kmem_cache_alloc(cachep->slabp_cache, flags);
        if (!slabp)
            return NULL;
        /* printk(" slab desc address is %p\n", slabp); */
        
    }
    else {
        slabp = objp + colour_off;
        colour_off += cachep->slab_size;
    }
    INIT_LIST_HEAD(&slabp->list);
    slabp->inuse = 0;
    slabp->colouroff = colour_off;
    slabp->s_mem = objp + colour_off;
    slabp->free = 0;
    slabp->free_list = NULL;

    return slabp;
}
/*
 * allocates a slab for a cache
 * This is called by kmem_cache_alloc() when
 * there are no objects left in the cache 
 */
/*
 * This function calls kmem_getpages() to obtain from the 
 * zoned page frame allocator the group of page frames needed to store a 
 * single slab; it then calls alloc_slabmgmt( ) to get a new slab descriptor. 
 * If the CFLGS_OFF_SLAB flag of the cache descriptor is set, the slab 
 * descriptor is allocated from the general cache pointed to by the slabp_cache
 * field of the cache descriptor; otherwise, the slab descriptor is allocated 
 * in the first page frame of the slab.
*/
static int cache_grow(kmem_cache_t *cachep, int flags, void *objp)
{
    size_t offset;
    slab_t *slabp;

    offset = cachep->colour_next;
    cachep->colour_next++;
    if (cachep->colour_next >= cachep->colour)
        cachep->colour_next = 0;

    offset *= cachep->colour_off;

    /* printk("inside cache_grow  : \n"); */

    /*
     * This gets storage for objects */
    if (!objp) {
        objp = kmem_getpages(cachep, flags);
        /* printk("address of the first slab memory is : \n%p\n", objp); */
    }
    if (!objp)
        goto failed;

    /* this allocates a slab desc */
    slabp = alloc_slabmgmt(cachep, objp, offset, flags);
    /* printk("address of the first slab desc is : \n%p\n", slabp); */

    if (!slabp)
        goto oops;


    slab_map_pages(cachep, slabp, objp);

    cache_init_objs(cachep, slabp);
    list_add_tail(&cachep->lists.slabs_free, &slabp->list);
    cachep->lists.free_objects += cachep->num;

    return 1;
oops:
    kmem_freepages(cachep, objp);

failed:
    return 0;
}

void *slab_get_obj(kmem_cache_t *cachep, struct slab_s *slabp)
{
    void *objp;
    objp = slabp->free_list;
    /* printk("in slab_get_obj free_list points to :%p\n", slabp->free_list); */
    /* printk("in slab_get_obj objp points to :%p\n", objp); */
    slabp->free_list = *(void **)objp;

    slabp->inuse++;
    cachep->lists.free_objects--;
    return objp;
}

void slab_put_obj(kmem_cache_t *cachep, struct slab_s *slabp,
                  void *objp)
{
    struct kmem_list3 *l3 = &cachep->lists;
    *(void **)objp = slabp->free_list;
    slabp->free_list = objp;
    slabp->inuse--;
}

void slab_destroy(kmem_cache_t *cachep, slab_t *slabp)
{
    if (cachep->dtor) {
        int i;
        for (i = 0; i < cachep->num; i++) {
            void *objp = slabp->s_mem + cachep->objsize * i;
            (cachep->dtor)(objp, cachep, 0);
        }
    }
    if (OFF_SLAB(cachep)) {
        kmem_freepages(cachep, slabp->s_mem);
        kmem_cache_free(cachep->slabp_cache, slabp);
    }
    else {
        kmem_freepages(cachep, slabp->s_mem - slabp->colouroff);
    }
}

void display_slab(struct slab_s *slabp)
{
    /* printk("displaying slab info\n"); */
    /* printk("colour off is : %x\n", slabp->colouroff); */
    /* printk("s_mem is : %p\n", slabp->s_mem); */
    /* printk("in_use : %x\n", slabp->inuse); */
    /* printk("free_list is : %p\n", slabp->free_list); */
}

void display_kmemlist(kmem_cache_t *cachep)
{
    struct kmem_list3 *l3 = &cachep->lists;
    struct list_head *run;
    struct slab_s *slabp;

    /* printk("displaying lists for cache : %s\n", cachep->name); */

    /* printk("=== Partial Slabs ===\n"); */
    list_for_each(run, &l3->slabs_partial) {
        slabp = list_entry(run, struct slab_s, list);
        display_slab(slabp);
    }

    /* printk("=== Full Slabs ===\n"); */
    list_for_each(run, &l3->slabs_full) {
        slabp = list_entry(run, struct slab_s, list);
        display_slab(slabp);
    }

    /* printk("=== Free Slabs ===\n"); */
    list_for_each(run, &l3->slabs_free) {
        slabp = list_entry(run, struct slab_s, list);
        display_slab(slabp);
    }
}

void *cache_alloc_refill(kmem_cache_t *cachep, unsigned int flags)
{
    struct kmem_list3 *l3;
    void *objp = NULL;
    int x;

    l3 = &cachep->lists;
    struct list_head *entry;
    struct slab_s *slabp;

    /* printk("\n*****************START cache-alloc-refill for cache: %s *****************\n", cachep->name); */

    display_kmemlist(cachep);
    entry = l3->slabs_partial.next;
    if (entry == &l3->slabs_partial) {
        entry = l3->slabs_free.next;
        if (entry == &l3->slabs_free) {
            /* goto must_grow; */
            /* printk("slab partial is empty  and slab_free too \n"); */
            /* printk("must grow \n"); */
            x = cache_grow(cachep, flags, objp);
            if (!x)
                return NULL;
        }
        /*
         * cache's slab free list would have been populated with 
         * a slab desc by now due to cache_grow
         * Hence now point it to the available entry in the 
         * slabs_free list and move it to partial list,
         * since we are obtaining some objects from it.
         * kalla na ? :)
         */
        entry = l3->slabs_free.next;
        slabp = list_entry(entry, struct slab_s, list);
        list_del(&slabp->list);
        list_add(&l3->slabs_partial, &slabp->list);
    }
    else {
        /* printk("slab partial is not empty \n"); */
    }


    slabp = list_entry(entry, struct slab_s, list);

    /*
     * take an object from the partial list 
     */


    objp = slab_get_obj(cachep, slabp);
    /* partial slab list may have some slabs
     * check if they have some free objects
     * If partial doesn't have any checky for 
     * slabs_free and move it to partial slab.
     * If the slabs_free list is also empty
     * invoke cache_grow().
     */

    /*
     * now if slab's all objects are allocated
     * move it to full list.
     * its a human's responsibility to clean his shit
     */
    if (slabp->free_list == NULL) {
        list_del(&slabp->list);
        list_add(&cachep->lists.slabs_full, &slabp->list);
    }

    display_kmemlist(cachep);

    /* slabp = list_entry(entry, struct slab, list); */
    /* list_del(&slabp->list); */

    /*
     * The slab was in either:
     * 1. slabs_free
     * OR 
     * 2. slabs_partial
     * Now we remove it because its state might change after 
     * allocating from it 
     */
    /* printk("\n*****************END cache-alloc-refill for cache: %s *****************\n", cachep->name); */
    return objp;
}

/*
 * The function kmem cache alloc() is responsible for allocating one 
 * object to the caller.
 */
void *kmem_cache_alloc(kmem_cache_t *cachep, unsigned int flags)
{
    void *objp;
    /* check is already present in cpu cache , 
     * will implement this later 
     */

    objp = cache_alloc_refill(cachep, flags);
    if (!objp) {
        printk("kmem_cache_alloc failed\n");
        return NULL;
    }

    return objp;
}

void *kmem_cache_zalloc(kmem_cache_t *cachep, unsigned int flags)
{
    void *objp;

    objp = kmem_cache_alloc(cachep, flags);
    if (!objp) {
        printk("kmem_cache_zalloc failed\n");
        return NULL;
    }
    memset(objp, 0, cachep->objsize);

    return objp;
}


/*
 * kmem_cache_free -  Deallocate an object
 * @cachep: The cache the object was allocated from
 * @objp: The previously allocated object
 */
void kmem_cache_free(kmem_cache_t *cachep, void *objp)
{
    struct kmem_list3 *l3;
    struct slab_s *slabp;
    int was_full = 0;

    slabp = virt_to_slab(objp);
    l3 = &cachep->lists;

    if (slabp->inuse == cachep->num)
        was_full = 1;

    slab_put_obj(cachep, slabp, objp);
    l3->free_objects++;



    if (slabp->inuse == 0) {
        /*
         * try setting the free_limit to number of objects, cause 
         * we don't have the local cache support yet 
         * The cachep->free_limit is usually equal to 
         * cachep->num + (1 + N) x cachep->batchcount. where N = no of CPUs.
         * But since we don't have local cache currently we only use
         * free_limit = cachep->num
         *
         */

        list_del(&slabp->list);
        if (l3->free_objects > cachep->free_limit) {
            l3->free_objects -= cachep->num;
            printk("destroying slab\n");
            slab_destroy(cachep, slabp);
        }
        else {
            list_add(&l3->slabs_free, &slabp->list);
        }
    }
    else if (was_full) {
        /* Unconditionally move a slab to the end of the
         * partial list on free - maximum time for the
         * other objects to be freed, too.
         */
        /*
         * slab might be on the slab_full list, hence move 
         * it to partial list
         */

        list_del(&slabp->list);
        list_add_tail(&l3->slabs_partial, &slabp->list);
    }
    /*
     * else {
     * already in partial , we should not move it 
     */
}


/**
 * calculate_slab_order - calculate size (page order) of slabs
 * anyway this calls cache_estimate internally
 *
 * @cachep: pointer to the cache that is being created
 * @size: size of objects to be created in this cache.
 * @align: required alignment for the objects.
 * @flags: slab allocation flags
 *
 * Also calculates the number of objects per slab.
 */
static size_t calculate_slab_order(kmem_cache_t *cachep, size_t size,
                                   size_t align, unsigned int flags)
{
    unsigned long offslab_limit;
    unsigned int left_over = 0;
    int gfporder;
    unsigned int num;
    unsigned int remainder;

    for (gfporder = 0; gfporder <= KMALLOC_MAX_ORDER; gfporder++) {

        cache_estimate(gfporder, size, align, flags, &remainder, &num);
        /* printk("cache_estimate result is : %x\n", num); */
        if (!num) {
            continue;
        }

        if (flags & CFLGS_OFF_SLAB) {
            offslab_limit = size - sizeof(struct slab_s);

            if (num > offslab_limit)
                break;
        }
        break;
    }
    cachep->num = num;
    cachep->gfporder = gfporder;
    left_over = remainder;
    /* printk("calculating slab order, no of objects are : %x\n", cachep->num); */
    /* printk("order is : %x\n", cachep->gfporder); */
    /* printk("left over is : %x", left_over); */

    return left_over;
}

kmem_cache_t *kmem_cache_create(const char *name, size_t size, size_t align,
                                unsigned int flags, void (*ctor)(void *, kmem_cache_t *, unsigned long))
{
    printk("\ninside kmem_cache_create for \n%s\n", name);

    size_t left_over, slab_size;
    size_t ralign; //required alignment
    kmem_cache_t *cachep = NULL, *pc = NULL;
    int gfp =0;

    if (size & (BYTES_PER_WORD - 1)) {
        size += (BYTES_PER_WORD - 1);
        size &= ~(BYTES_PER_WORD - 1);
    }

    if (flags & SLAB_HWCACHE_ALIGN) {
        ralign = cache_line_size();
        /*
         * but is object size if even less than cache line's half, then the 
         * cache line will be wasted, instead it can be used to allocated 
         * multiple objects 
         *
         */
        while (size <= ralign / 2)
            ralign /= 2;
        /* printk("i'm aligned to %d\n", ralign); */
    }
    else {
        /* printk("i'm not aligned \n"); */
        ralign = BYTES_PER_WORD;
    }

    /*
     * alignment estimated should not be less than
     * requested by user
     */
    if (ralign < align)
        ralign = align;
    /* printk("size is %d\n", ralign); */
    cachep = kmem_cache_alloc(&cache_cache, gfp);
    if (!cachep)
        goto oops;
    /* printk("\ncachep as an object its address is : %p\n", cachep); */

    kmem_list3_init(&cachep->lists);
    cachep->objsize = size;
    size = ALIGN(size, align);

    left_over = calculate_slab_order(cachep, size, align, flags);
    /* printk("left over is : %x\n", left_over); */

    if (!cachep->num) {
        printk("\nkmem_cache_create: couldn't create cache \n");
        kmem_cache_free(&cache_cache, cachep);
        cachep = NULL;
        goto oops;
    }
    cachep->free_limit = cachep->num;

    if (flags & CFLGS_OFF_SLAB)
        slab_size = sizeof(struct slab_s);
    else
        slab_size = ALIGN(sizeof(struct slab_s), align);

    cachep->colour_off = cache_line_size();
    if (cachep->colour_off < align)
        cachep->colour_off = align;
    /*
     * regarding colour read the explanation
     * for utlk and somewhere in this file too
     * explained very well, I won't do that again here
     * Probably inside kmem_cache_init. dk dk 
     */
    cachep->colour = left_over / cachep->colour_off; //free / aln 
    cachep->slab_size = slab_size;
    cachep->flags = flags;
    /*
     * changed object size after alignment */
    cachep->objsize = size;

    cachep->ctor = ctor;
    cachep->name = name;
    cachep->slabp_cache = cache_cache.slabp_cache;

    INIT_LIST_HEAD(&cachep->next);

    list_add(&cache_chain, &cachep->next);

    return cachep;

oops:
    printk("cache creation failed \n");
    return NULL;
}

void *kzalloc(size_t size, int flags)
{
    struct cache_sizes *csizep = malloc_sizes;
    kmem_cache_t *cachep;

    for (; csizep->cs_size; csizep++) {
        /*
         * malloc_sizes table to locate the nearest power-of-2 size 
         * to the requested size
         */
        if (size > csizep->cs_size)
            continue;

        /* printk("close matching size is : %x\n", csizep->cs_size); */
        cachep = csizep->cs_cachep;
        return kmem_cache_zalloc(cachep, flags);
    }
    return NULL;
}

void *kmalloc(size_t size, int flags)
{
    struct cache_sizes *csizep = malloc_sizes;
    kmem_cache_t *cachep;

    for (; csizep->cs_size; csizep++) {
        /*
         * malloc_sizes table to locate the nearest power-of-2 size 
         * to the requested size
         */
        if (size > csizep->cs_size)
            continue;

        /* printk("close matching size is : %x\n", csizep->cs_size); */
        cachep = csizep->cs_cachep;
        return kmem_cache_alloc(cachep, flags);
    }

    return NULL;
}

void kfree(const void *objp)
{
    kmem_cache_t *cachep;
    unsigned int flags;

    if (!objp)
        return;

    cachep = (kmem_cache_t *)(virt_to_page(objp)->lru.next);
    kmem_cache_free(cachep, (void *)objp);
}


void init_slab(void)
{
    kmem_cache_init(&cache_cache, "kmem_cache", sizeof(struct kmem_cache), NULL, NULL);
}
void test_slab(void)
{
    struct page *page = virt_to_page((void *)0x00001001);
    /* printk("my page is : %x\n", page->page_no); */

   /* run_slab_tests(); 
    * run tests from mem-test.c
    *
    */
}



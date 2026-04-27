#ifndef _SLAB_H
#define _SLAB_H

#include "zone.h"
#include "mm.h"

#define FREE_OBJ_LIMIT 0x1000
#define KMALLOC_MAX_ORDER 10

/* Below flags are stored inside kmem_caches's flags attribute */
#define CFLGS_OFF_SLAB		(0x80000000U)
#define SLAB_HWCACHE_ALIGN 0x00002000  /* HWALIGN flags is paseed during kmalloc */

#define KMALLOC_FLAGS       SLAB_HWCACHE_ALIGN
#define KMALLOC_MINALIGN    __alignof__(unsigned long long)


struct kmem_list3 {
    struct list_head slabs_partial; //list of slab descs with free and non-free object
    struct list_head slabs_full; //list of slab descs with no free object
    struct list_head slabs_free; //list of slab descs with free object only
    unsigned int free_objects; //no of free objects in cache
    int free_limit;
    /*
    *
    * int free_touced;
    * unsigned int next_reap;
    */ 

};

typedef struct kmem_cache {
    unsigned int limit; //max number of free objects in local caches.
    struct kmem_list3 lists;
    unsigned int objsize; //size of objects included in cache 
    // unsigned int buffer_size; //aligned object size
    unsigned int flags;
    unsigned int num; //no of objects packed in a single slab. (consistent)
    unsigned int free_limit; // upper limit of free objects in the whole slab cachae
    
    unsigned int gfporder; //logarithm of number of page frames allocated to a single slab
    unsigned int gfpflags; //flags passed to buddy system when allocating page frames
    size_t colour; //number of colors ( used for slab coloring)
    unsigned int colour_off; //basic alignment offset in slabs
    unsigned int colour_next; //colour to use for next allocated slab
    
    struct kmem_cache *slabp_cache; //Pointer to general slab cache containing 
                                    //the slab descriptors if external slab
                                    //descriptors are used


    unsigned int slab_size; //size of single slab
    unsigned int dflags; //dynamic properties of cache

    void (*ctor)(void *, struct kmem_cache*, unsigned long); //pointer to constructor method associated with the cache
    void (*dtor)(void *, struct kmem_cache*, unsigned long); //pointer to desctructor method associated with the cache

    const char *name; //name of slab
    struct list_head next; //pointer to doubly linked list of cache descriptors
} kmem_cache_t;

typedef struct slab_s {
    struct list_head list; //ptr to one of three linked lists of slab state
    unsigned long colouroff; //offset of the first object in the slab
    void *s_mem; //address of the first object (either allocated or free in the slab)
    unsigned int inuse; //number of objects that are currently used (not free)
    unsigned int free; //index of next free object in slab or BUFCTL_END if there are no objects left
    void *free_list; //pointer to the first free object
}slab_t;

#define	OFF_SLAB(x)	((x)->flags & CFLGS_OFF_SLAB)

void test_slab(void);
void kmem_cache_init(kmem_cache_t *cachep, const char *name, size_t objsize,
                     void (*ctor)(void *, kmem_cache_t *, unsigned long),
                     void (*dtor)(void *, kmem_cache_t *, unsigned long));

void cache_estimate(int gfporder, unsigned int objsize, unsigned int align, int flags, unsigned int * left_over, unsigned int *num);

kmem_cache_t *kmem_cache_create(const char *name, size_t size, size_t align, 
        unsigned int flags,void (*ctor)(void *, kmem_cache_t *, unsigned long));
void kmem_cache_free(kmem_cache_t *cachep, void *objp);
void *kmem_cache_alloc(kmem_cache_t *cachep, unsigned int flags);
void *kmem_cache_zalloc(kmem_cache_t *cachep, unsigned int flags);
void *kmalloc(size_t size, int flags);
void kfree(const void *objp);
void *kzalloc(size_t size, int flags);
void init_slab();



#endif

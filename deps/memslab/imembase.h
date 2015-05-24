/**********************************************************************
 *
 * imembase.h - basic interface of memory operation
 *
 * - application layer slab allocator implementation
 * - unit interval time cost: almost speed up 500% - 1200% vs malloc
 * - optional page supplier: with the "GFP-Tree" algorithm
 * - memory recycle: automatic give memory back to os to avoid wasting
 * - platform independence
 *
 * for the basic information about slab algorithm, please see:
 *   The Slab Allocator: An Object Caching Kernel 
 *   Memory Allocator (Jeff Bonwick, Sun Microsystems, 1994)
 * with the URL below:
 *   http://citeseer.ist.psu.edu/bonwick94slab.html
 *
 **********************************************************************/

#ifndef __IMEMBASE_H__
#define __IMEMBASE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <stdlib.h>
#include <string.h>


/*====================================================================*/
/* IULONG/ILONG (ensure sizeof(iulong) == sizeof(void*))              */
/*====================================================================*/
#ifndef __IULONG_DEFINED
#define __IULONG_DEFINED
typedef ptrdiff_t ilong;
typedef size_t iulong;
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*====================================================================*/
/* IALLOCATOR                                                         */
/*====================================================================*/
struct IALLOCATOR
{
    void *(*alloc)(struct IALLOCATOR *, size_t);
    void (*free)(struct IALLOCATOR *, void *);
    void *udata;
    ilong reserved;
};

void* internal_malloc(struct IALLOCATOR *allocator, size_t size);
void internal_free(struct IALLOCATOR *allocator, void *ptr);


/*====================================================================*/
/* IVECTOR                                                            */
/*====================================================================*/
struct IVECTOR
{
	unsigned char *data;       
	size_t size;      
	size_t block;       
	struct IALLOCATOR *allocator;
};

void iv_init(struct IVECTOR *v, struct IALLOCATOR *allocator);
void iv_destroy(struct IVECTOR *v);
int iv_resize(struct IVECTOR *v, size_t newsize);
int iv_reserve(struct IVECTOR *v, size_t newsize);

size_t iv_pop(struct IVECTOR *v, void *data, size_t size);
int iv_push(struct IVECTOR *v, const void *data, size_t size);
int iv_insert(struct IVECTOR *v, size_t pos, const void *data, size_t size);
int iv_erase(struct IVECTOR *v, size_t pos, size_t size);

#define iv_size(v) ((v)->size)
#define iv_data(v) ((v)->data)

#define iv_index(v, type, index) (((type*)iv_data(v))[index])

#define IMROUNDSHIFT	3
#define IMROUNDSIZE		(((size_t)1) << IMROUNDSHIFT)
#define IMROUNDUP(s)	(((s) + IMROUNDSIZE - 1) & ~(IMROUNDSIZE - 1))


/*====================================================================*/
/* IMEMNODE                                                           */
/*====================================================================*/
struct IMEMNODE
{
	struct IALLOCATOR *allocator;   /* memory allocator        */

	struct IVECTOR vprev;           /* prev node link vector   */
	struct IVECTOR vnext;           /* next node link vector   */
	struct IVECTOR vnode;           /* node information data   */
	struct IVECTOR vdata;           /* node data buffer vector */
	struct IVECTOR vmode;           /* mode of allocation      */
	ilong *mprev;                   /* prev node array         */
	ilong *mnext;                   /* next node array         */
	ilong *mnode;                   /* node info array         */
	void **mdata;                   /* node data array         */
	ilong *mmode;                   /* node mode array         */
	ilong *extra;                   /* extra user data         */
	ilong node_free;                /* number of free nodes    */
	ilong node_used;                /* number of allocated     */
	ilong node_max;                 /* number of all nodes     */
	ilong grow_limit;               /* limit of growing        */

	ilong node_size;                /* node data fixed size    */
	ilong node_shift;               /* node data size shift    */

	struct IVECTOR vmem;            /* mem-pages in the pool   */
	char **mmem;                    /* mem-pages array         */
	ilong mem_max;                  /* max num of memory pages */
	ilong mem_count;                /* number of mem-pages     */

	ilong list_open;                /* the entry of open-list  */
	ilong list_close;               /* the entry of close-list */
	ilong total_mem;                /* total memory size       */
};


void imnode_init(struct IMEMNODE *mn, ilong nodesize, struct IALLOCATOR *ac);
void imnode_destroy(struct IMEMNODE *mnode);
ilong imnode_new(struct IMEMNODE *mnode);
void imnode_del(struct IMEMNODE *mnode, ilong index);
ilong imnode_head(const struct IMEMNODE *mnode);
ilong imnode_next(const struct IMEMNODE *mnode, ilong index);
ilong imnode_prev(const struct IMEMNODE *mnode, ilong index);
void*imnode_data(struct IMEMNODE *mnode, ilong index);
const void* imnode_data_const(const struct IMEMNODE *mnode, ilong index);

#define IMNODE_NODE(mnodeptr, i) ((mnodeptr)->mnode[i])
#define IMNODE_PREV(mnodeptr, i) ((mnodeptr)->mprev[i])
#define IMNODE_NEXT(mnodeptr, i) ((mnodeptr)->mnext[i])
#define IMNODE_DATA(mnodeptr, i) ((mnodeptr)->mdata[i])
#define IMNODE_MODE(mnodeptr, i) ((mnodeptr)->mmode[i])


/*====================================================================*/
/* QUEUE DEFINITION                                                   */
/*====================================================================*/
#ifndef __IQUEUE_DEF__
#define __IQUEUE_DEF__

struct IQUEUEHEAD {
	struct IQUEUEHEAD *next, *prev;
};

typedef struct IQUEUEHEAD iqueue_head;


/*--------------------------------------------------------------------*/
/* queue init                                                         */
/*--------------------------------------------------------------------*/
#define IQUEUE_HEAD_INIT(name) { &(name), &(name) }
#define IQUEUE_HEAD(name) \
	struct IQUEUEHEAD name = IQUEUE_HEAD_INIT(name)

#define IQUEUE_INIT(ptr) ( \
	(ptr)->next = (ptr), (ptr)->prev = (ptr))

#define IOFFSETOF(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define ICONTAINEROF(ptr, type, member) ( \
		(type*)( ((char*)((type*)ptr)) - IOFFSETOF(type, member)) )

#define IQUEUE_ENTRY(ptr, type, member) ICONTAINEROF(ptr, type, member)


/*--------------------------------------------------------------------*/
/* queue operation                                                    */
/*--------------------------------------------------------------------*/
#define IQUEUE_ADD(node, head) ( \
	(node)->prev = (head), (node)->next = (head)->next, \
	(head)->next->prev = (node), (head)->next = (node))

#define IQUEUE_ADD_TAIL(node, head) ( \
	(node)->prev = (head)->prev, (node)->next = (head), \
	(head)->prev->next = (node), (head)->prev = (node))

#define IQUEUE_DEL_BETWEEN(p, n) ((n)->prev = (p), (p)->next = (n))

#define IQUEUE_DEL(entry) (\
	(entry)->next->prev = (entry)->prev, \
	(entry)->prev->next = (entry)->next, \
	(entry)->next = 0, (entry)->prev = 0)

#define IQUEUE_DEL_INIT(entry) do { \
	IQUEUE_DEL(entry); IQUEUE_INIT(entry); } while (0)

#define IQUEUE_IS_EMPTY(entry) ((entry) == (entry)->next)

#define iqueue_init		IQUEUE_INIT
#define iqueue_entry	IQUEUE_ENTRY
#define iqueue_add		IQUEUE_ADD
#define iqueue_add_tail	IQUEUE_ADD_TAIL
#define iqueue_del		IQUEUE_DEL
#define iqueue_del_init	IQUEUE_DEL_INIT
#define iqueue_is_empty IQUEUE_IS_EMPTY

#define IQUEUE_FOREACH(iterator, head, TYPE, MEMBER) \
	for ((iterator) = iqueue_entry((head)->next, TYPE, MEMBER); \
		&((iterator)->MEMBER) != (head); \
		(iterator) = iqueue_entry((iterator)->MEMBER.next, TYPE, MEMBER))

#define iqueue_foreach(iterator, head, TYPE, MEMBER) \
	IQUEUE_FOREACH(iterator, head, TYPE, MEMBER)

#define iqueue_foreach_entry(pos, head) \
	for( (pos) = (head)->next; (pos) != (head) ; (pos) = (pos)->next )
	

#define __iqueue_splice(list, head) do {	\
		iqueue_head *first = (list)->next, *last = (list)->prev; \
		iqueue_head *at = (head)->next; \
		(first)->prev = (head), (head)->next = (first);		\
		(last)->next = (at), (at)->prev = (last); }	while (0)

#define iqueue_splice(list, head) do { \
	if (!iqueue_is_empty(list)) __iqueue_splice(list, head); } while (0)

#define iqueue_splice_init(list, head) do {	\
	iqueue_splice(list, head);	iqueue_init(list); } while (0)


#ifdef _MSC_VER
#pragma warning(disable:4311)
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif

#endif


/*====================================================================*/
/* IMEMSLAB                                                           */
/*====================================================================*/
struct IMEMSLAB
{
	struct IQUEUEHEAD queue;
	size_t coloroff;
	void*membase;
	ilong memsize;
	ilong inuse;
	void*bufctl;
	void*extra;
};

typedef struct IMEMSLAB imemslab_t;

#define IMEMSLAB_ISFULL(s) ((s)->bufctl == 0)
#define IMEMSLAB_ISEMPTY(s) ((s)->inuse == 0)

#ifdef __cplusplus
}
#endif

/*====================================================================*/
/* IMUTEX - mutex interfaces                                          */
/*====================================================================*/
#ifndef IMUTEX_TYPE

#ifndef IMUTEX_DISABLE
#if (defined(WIN32) || defined(_WIN32))
#if ((!defined(_M_PPC)) && (!defined(_M_PPC_BE)) && (!defined(_XBOX)))
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN  
#endif
#include <windows.h>
#else
#ifndef _XBOX
#define _XBOX
#endif
#include <xtl.h>
#endif

#define IMUTEX_TYPE         CRITICAL_SECTION
#define IMUTEX_INIT(m)      InitializeCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_DESTROY(m)   DeleteCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_LOCK(m)      EnterCriticalSection((CRITICAL_SECTION*)(m))
#define IMUTEX_UNLOCK(m)    LeaveCriticalSection((CRITICAL_SECTION*)(m))

#elif defined(__unix) || defined(unix) || defined(__MACH__)
#include <unistd.h>
#include <pthread.h>
#define IMUTEX_TYPE         pthread_mutex_t
#define IMUTEX_INIT(m)      pthread_mutex_init((pthread_mutex_t*)(m), 0)
#define IMUTEX_DESTROY(m)   pthread_mutex_destroy((pthread_mutex_t*)(m))
#define IMUTEX_LOCK(m)      pthread_mutex_lock((pthread_mutex_t*)(m))
#define IMUTEX_UNLOCK(m)    pthread_mutex_unlock((pthread_mutex_t*)(m))
#endif
#endif

#ifndef IMUTEX_TYPE
#define IMUTEX_TYPE         int
#define IMUTEX_INIT(m)      { (m) = (m); }
#define IMUTEX_DESTROY(m)   { (m) = (m); }
#define IMUTEX_LOCK(m)      { (m) = (m); }
#define IMUTEX_UNLOCK(m)    { (m) = (m); }
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef IMUTEX_TYPE imutex_t;
extern int imutex_disable;

void imutex_init(imutex_t *mutex);
void imutex_destroy(imutex_t *mutex);
void imutex_lock(imutex_t *mutex);
void imutex_unlock(imutex_t *mutex);


/*====================================================================*/
/* IMEMGFP (mem_get_free_pages) - a page-supplyer class               */
/*====================================================================*/
struct IMEMGFP
{
	size_t page_size;
	ilong refcnt;
	void*(*alloc_page)(struct IMEMGFP *gfp);
	void (*free_page)(struct IMEMGFP *gfp, void *ptr);
	void *extra;
	size_t pages_inuse;
	size_t pages_new;
	size_t pages_del;
};

#define IDEFAULT_PAGE_SHIFT 16

typedef struct IMEMGFP imemgfp_t;


/*====================================================================*/
/* IMEMLRU                                                            */
/*====================================================================*/
#ifndef IMCACHE_ARRAYLIMIT
#define IMCACHE_ARRAYLIMIT 64
#endif

#ifndef IMCACHE_NODECOUNT_SHIFT
#define IMCACHE_NODECOUNT_SHIFT 0
#endif

#define IMCACHE_NODECOUNT (1 << (IMCACHE_NODECOUNT_SHIFT))

#ifndef IMCACHE_NAMESIZE
#define IMCACHE_NAMESIZE 32
#endif

#ifndef IMCACHE_LRU_SHIFT
#define IMCACHE_LRU_SHIFT	2
#endif

#define IMCACHE_LRU_COUNT	(1 << IMCACHE_LRU_SHIFT)

struct IMEMLRU
{
	int avial;
	int limit;
	int batchcount;
	imutex_t lock;
	void *entry[IMCACHE_ARRAYLIMIT];
};

typedef struct IMEMLRU imemlru_t;


/*====================================================================*/
/* IMEMCACHE                                                          */
/*====================================================================*/
struct IMEMCACHE
{
	size_t obj_size;
	size_t unit_size;
	size_t page_size;
	size_t count_partial;
	size_t count_full;
	size_t count_free;
	size_t free_objects;
	size_t free_limit;
	size_t color_next;
	size_t color_limit;

	iqueue_head queue;
	imutex_t list_lock;

	iqueue_head slabs_partial;
	iqueue_head slabs_full;
	iqueue_head slabs_free;

	imemlru_t array[IMCACHE_LRU_COUNT];
	imemgfp_t *gfp;		
	imemgfp_t page_supply;

	size_t batchcount;
	size_t limit;
	size_t num;	
	ilong flags;
	ilong user;
	void*extra;

	char name[IMCACHE_NAMESIZE + 1];
	size_t pages_hiwater;
	size_t pages_inuse;
	size_t pages_new;
	size_t pages_del;
};

typedef struct IMEMCACHE imemcache_t;


/*====================================================================*/
/* IKMEMHOOK                                                          */
/*====================================================================*/
struct IKMEMHOOK
{
	void* (*kmem_malloc_fn)(size_t size);
	void (*kmem_free_fn)(void *ptr);
	void* (*kmem_realloc_fn)(void *ptr, size_t size);
	size_t (*kmem_ptr_size_fn)(const void *ptr);
	void (*kmem_shrink_fn)(void);
};

typedef struct IKMEMHOOK ikmemhook_t;


/*====================================================================*/
/* IKMEM INTERFACE                                                    */
/*====================================================================*/
void ikmem_init(int page_shift, int pg_malloc, size_t *sz);
void ikmem_destroy(void);

void* ikmem_malloc(size_t size);
void* ikmem_realloc(void *ptr, size_t size);
void ikmem_free(void *ptr);
void ikmem_shrink(void);

imemcache_t *ikmem_create(const char *name, size_t size);
void ikmem_delete(imemcache_t *cache);
void *ikmem_cache_alloc(imemcache_t *cache);
void ikmem_cache_free(imemcache_t *cache, void *ptr);

size_t ikmem_ptr_size(const void *ptr);
void ikmem_option(size_t watermark);
imemcache_t *ikmem_get(const char *name);

ilong ikmem_page_info(ilong *pg_inuse, ilong *pg_new, ilong *pg_del);
ilong ikmem_cache_info(int id, int *inuse, int *cnew, int *cdel, int *cfree);
ilong ikmem_waste_info(ilong *kmem_inuse, ilong *total_mem);

int ikmem_hook_install(const ikmemhook_t *hook);
const ikmemhook_t *ikmem_hook_get(int id);


/*====================================================================*/
/* IVECTOR / IMEMNODE MANAGEMENT                                      */
/*====================================================================*/
extern struct IALLOCATOR ikmem_allocator;

typedef struct IVECTOR ivector_t;
typedef struct IMEMNODE imemnode_t;

ivector_t *iv_create(void);
void iv_delete(ivector_t *vec);

imemnode_t *imnode_create(ilong nodesize, int grow_limit);
void imnode_delete(imemnode_t *);


#ifdef __cplusplus
}
#endif

#endif



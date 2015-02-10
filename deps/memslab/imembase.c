/**********************************************************************
 *
 * imembase.c - basic interface of memory operation
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

#include "imembase.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#if (defined(__BORLANDC__) || defined(__WATCOMC__))
#if defined(_WIN32) || defined(WIN32)
#pragma warn -8002  
#pragma warn -8004  
#pragma warn -8008  
#pragma warn -8012
#pragma warn -8027
#pragma warn -8057  
#pragma warn -8066 
#endif
#endif


/*====================================================================*/
/* IALLOCATOR                                                         */
/*====================================================================*/
void *(*__ihook_malloc)(size_t size) = NULL;
void (*__ihook_free)(void *) = NULL;


void* internal_malloc(struct IALLOCATOR *allocator, size_t size)
{
	if (allocator != NULL) {
		return allocator->alloc(allocator, size);
	}
	if (__ihook_malloc != NULL) {
		return __ihook_malloc(size);
	}
	return malloc(size);
}

void internal_free(struct IALLOCATOR *allocator, void *ptr)
{
	if (allocator != NULL) {
		allocator->free(allocator, ptr);
		return;
	}
	if (__ihook_free != NULL) {
		__ihook_free(ptr);
		return;
	}
	free(ptr);
}



/*====================================================================*/
/* IVECTOR                                                            */
/*====================================================================*/
void iv_init(struct IVECTOR *v, struct IALLOCATOR *allocator)
{
	if (v == 0) return;
	v->data = 0;
	v->size = 0;
	v->block = 0;
	v->allocator = allocator;
}

void iv_destroy(struct IVECTOR *v)
{
	if (v == NULL) return;
	if (v->data) {
		internal_free(v->allocator, v->data);
	}
	v->data = NULL;
	v->size = 0;
	v->block = 0;
}

int iv_resize(struct IVECTOR *v, size_t newsize)
{
	unsigned char*lptr;
	size_t block, min;
	size_t nblock;

	if (v == NULL) return -1;
	if (newsize >= v->size && newsize <= v->block) { 
		v->size = newsize; 
		return 0; 
	}

	if (newsize == 0) {
		if (v->block > 0) {
			internal_free(v->allocator, v->data);
			v->block = 0;
			v->size = 0;
			v->data = NULL;
		}
		return 0;
	}

	for (nblock = sizeof(char*); nblock < newsize; ) nblock <<= 1;
	block = nblock;

	if (block == v->block) { 
		v->size = newsize; 
		return 0; 
	}

	if (v->block == 0 || v->data == NULL) {
		v->data = (unsigned char*)internal_malloc(v->allocator, block);
		if (v->data == NULL) return -1;
		v->size = newsize;
		v->block = block;
	}    else {
		lptr = (unsigned char*)internal_malloc(v->allocator, block);
		if (lptr == NULL) return -1;

		min = (v->size <= newsize)? v->size : newsize;
		memcpy(lptr, v->data, (size_t)min);
		internal_free(v->allocator, v->data);

		v->data = lptr;
		v->size = newsize;
		v->block = block;
	}

	return 0;
}

int iv_push(struct IVECTOR *v, const void *data, size_t size)
{
	size_t current = v->size;
	if (iv_resize(v, current + size) != 0)
		return -1;
	if (data != NULL) 
		memcpy(v->data + current, data, size);
	return 0;
}

size_t iv_pop(struct IVECTOR *v, void *data, size_t size)
{
	size_t current = v->size;
	if (size >= current) size = current;
	if (data != NULL) 
		memcpy(data, v->data + current - size, size);
	iv_resize(v, current - size);
	return size;
}

int iv_insert(struct IVECTOR *v, size_t pos, const void *data, size_t size)
{
	size_t current = v->size;
	if (iv_resize(v, current + size) != 0)
		return -1;
	memmove(v->data + pos + size, v->data + pos, size);
	if (data != NULL) 
		memcpy(v->data + pos, data, size);
	return 0;
}

int iv_erase(struct IVECTOR *v, size_t pos, size_t size)
{
	size_t current = v->size;
	if (pos >= current) return 0;
	if (pos + size >= current) size = current - pos;
	if (size == 0) return 0;
	memmove(v->data + pos, v->data + pos + size, current - pos - size);
	if (iv_resize(v, current - size) != 0) 
		return -1;
	return 0;
}


/*====================================================================*/
/* IMEMNODE                                                           */
/*====================================================================*/
void imnode_init(struct IMEMNODE *mn, ilong nodesize, struct IALLOCATOR *ac)
{
	struct IMEMNODE *mnode = mn;
	size_t newsize, shift;

	assert(mnode != NULL);
	mnode->allocator = ac;

	iv_init(&mnode->vprev, ac);
	iv_init(&mnode->vnext, ac);
	iv_init(&mnode->vnode, ac);
	iv_init(&mnode->vdata, ac);
	iv_init(&mnode->vmem, ac);
	iv_init(&mnode->vmode, ac);

	for (shift = 1; (((size_t)1) << shift) < (size_t)nodesize; ) shift++;

	newsize = (nodesize < (ilong)IMROUNDSIZE)? (ilong)IMROUNDSIZE : nodesize;
	newsize = IMROUNDUP(newsize);

	mnode->node_size = newsize;
	mnode->node_shift = (ilong)shift;
	mnode->node_free = 0;
	mnode->node_used = 0;
	mnode->node_max = 0;
	mnode->mem_max = 0;
	mnode->mem_count = 0;
	mnode->list_open = -1;
	mnode->list_close = -1;
	mnode->total_mem = 0;
	mnode->grow_limit = 0;
	mnode->extra = NULL;
}

void imnode_destroy(struct IMEMNODE *mnode)
{
    ilong i;

	assert(mnode != NULL);
	if (mnode->mem_count > 0) {
		for (i = 0; i < mnode->mem_count && mnode->mmem; i++) {
			if (mnode->mmem[i]) {
				internal_free(mnode->allocator, mnode->mmem[i]);
			}
			mnode->mmem[i] = NULL;
		}
		mnode->mem_count = 0;
		mnode->mem_max = 0;
		iv_destroy(&mnode->vmem);
		mnode->mmem = NULL;
	}

	iv_destroy(&mnode->vprev);
	iv_destroy(&mnode->vnext);
	iv_destroy(&mnode->vnode);
	iv_destroy(&mnode->vdata);
	iv_destroy(&mnode->vmode);

	mnode->mprev = NULL;
	mnode->mnext = NULL;
	mnode->mnode = NULL;
	mnode->mdata = NULL;
	mnode->mmode = NULL;

	mnode->node_free = 0;
	mnode->node_used = 0;
	mnode->node_max = 0;
	mnode->list_open = -1;
	mnode->list_close= -1;
	mnode->total_mem = 0;
}

static int imnode_node_resize(struct IMEMNODE *mnode, ilong size)
{
	size_t size1, size2;

	size1 = (size_t)(size * (ilong)sizeof(ilong));
	size2 = (size_t)(size * (ilong)sizeof(void*));

	if (iv_resize(&mnode->vprev, size1)) return -1;
	if (iv_resize(&mnode->vnext, size1)) return -2;
	if (iv_resize(&mnode->vnode, size1)) return -3;
	if (iv_resize(&mnode->vdata, size2)) return -5;
	if (iv_resize(&mnode->vmode, size1)) return -6;

	mnode->mprev = (ilong*)((void*)mnode->vprev.data);
	mnode->mnext = (ilong*)((void*)mnode->vnext.data);
	mnode->mnode = (ilong*)((void*)mnode->vnode.data);
	mnode->mdata =(void**)((void*)mnode->vdata.data);
	mnode->mmode = (ilong*)((void*)mnode->vmode.data);
	mnode->node_max = size;

	return 0;
}

static int imnode_mem_add(struct IMEMNODE*mnode, ilong node_count, void**mem)
{
	size_t newsize;
	char *mptr;

	if (mnode->mem_count >= mnode->mem_max) {
		newsize = (mnode->mem_max <= 0)? IMROUNDSIZE : mnode->mem_max * 2;
		if (iv_resize(&mnode->vmem, newsize * sizeof(void*))) 
			return -1;
		mnode->mem_max = newsize;
		mnode->mmem = (char**)((void*)mnode->vmem.data);
	}
	newsize = node_count * mnode->node_size + IMROUNDSIZE;
	mptr = (char*)internal_malloc(mnode->allocator, newsize);
	if (mptr == NULL) return -2;

	mnode->mmem[mnode->mem_count++] = mptr;
	mnode->total_mem += newsize;
	mptr = (char*)IMROUNDUP(((size_t)mptr));

	if (mem) *mem = mptr;

	return 0;
}


static long imnode_grow(struct IMEMNODE *mnode)
{
	ilong size_start = mnode->node_max;
	ilong size_endup;
	ilong retval, count, i, j;
	void *mptr;
	char *p;

	count = (mnode->node_max <= 0)? IMROUNDSIZE : mnode->node_max;
	if (mnode->grow_limit > 0) {
		if (count > mnode->grow_limit) count = mnode->grow_limit;
	}
	size_endup = size_start + count;

	retval = imnode_node_resize(mnode, size_endup);
	if (retval) return -10 + (long)retval;

	retval = imnode_mem_add(mnode, count, &mptr);

	if (retval) {
		imnode_node_resize(mnode, size_start);
		mnode->node_max = size_start;
		return -20 + (long)retval;
	}

	p = (char*)mptr;
	for (i = mnode->node_max - 1, j = 0; j < count; i--, j++) {
		IMNODE_NODE(mnode, i) = 0;
		IMNODE_MODE(mnode, i) = 0;
		IMNODE_DATA(mnode, i) = p;
		IMNODE_PREV(mnode, i) = -1;
		IMNODE_NEXT(mnode, i) = mnode->list_open;
		if (mnode->list_open >= 0) IMNODE_PREV(mnode, mnode->list_open) = i;
		mnode->list_open = i;
		mnode->node_free++;
		p += mnode->node_size;
	}

	return 0;
}


ilong imnode_new(struct IMEMNODE *mnode)
{
	ilong node, next;

	assert(mnode);
	if (mnode->list_open < 0) {
		if (imnode_grow(mnode)) return -2;
	}

	if (mnode->list_open < 0 || mnode->node_free <= 0) return -3;

	node = mnode->list_open;
	next = IMNODE_NEXT(mnode, node);
	if (next >= 0) IMNODE_PREV(mnode, next) = -1;
	mnode->list_open = next;

	IMNODE_PREV(mnode, node) = -1;
	IMNODE_NEXT(mnode, node) = mnode->list_close;

	if (mnode->list_close >= 0) IMNODE_PREV(mnode, mnode->list_close) = node;
	mnode->list_close = node;
	IMNODE_MODE(mnode, node) = 1;

	mnode->node_free--;
	mnode->node_used++;

	return node;
}

void imnode_del(struct IMEMNODE *mnode, ilong index)
{
	ilong prev, next;

	assert(mnode);
	assert((index >= 0) && (index < mnode->node_max));
	assert(IMNODE_MODE(mnode, index));

	next = IMNODE_NEXT(mnode, index);
	prev = IMNODE_PREV(mnode, index);

	if (next >= 0) IMNODE_PREV(mnode, next) = prev;
	if (prev >= 0) IMNODE_NEXT(mnode, prev) = next;
	else mnode->list_close = next;

	IMNODE_PREV(mnode, index) = -1;
	IMNODE_NEXT(mnode, index) = mnode->list_open;

	if (mnode->list_open >= 0) IMNODE_PREV(mnode, mnode->list_open) = index;
	mnode->list_open = index;

	IMNODE_MODE(mnode, index) = 0;
	mnode->node_free++;
	mnode->node_used--;
}

ilong imnode_head(const struct IMEMNODE *mnode)
{
	return (mnode)? mnode->list_close : -1;
}

ilong imnode_next(const struct IMEMNODE *mnode, ilong index)
{
	return (mnode)? IMNODE_NEXT(mnode, index) : -1;
}

ilong imnode_prev(const struct IMEMNODE *mnode, ilong index)
{
	return (mnode)? IMNODE_PREV(mnode, index) : -1;
}

void *imnode_data(struct IMEMNODE *mnode, ilong index)
{
	return (char*)IMNODE_DATA(mnode, index);
}

const void* imnode_data_const(const struct IMEMNODE *mnode, ilong index)
{
	return (const char*)IMNODE_DATA(mnode, index);
}



/*====================================================================*/
/* IMEMSLAB                                                           */
/*====================================================================*/
#define IMEM_NEXT_PTR(p)  (((void**)(p))[0])

/* init slab structure with given memory block and coloroff */
static ilong imslab_init(imemslab_t *slab, void *membase, 
	size_t memsize, ilong obj_size, size_t coloroff)
{
	char *start = ((char*)membase) + coloroff;
	char *endup = ((char*)membase) + memsize - obj_size;
	ilong retval = 0;
	char *tail;

	assert(slab && membase);
	assert((size_t)obj_size >= sizeof(void*));

	iqueue_init(&slab->queue);
	slab->membase = membase;
	slab->memsize = memsize;
	slab->coloroff = coloroff;
	slab->inuse = 0;
	slab->extra = NULL;
	slab->bufctl = NULL;

	for (tail = NULL; start <= endup; start += obj_size) {
		IMEM_NEXT_PTR(start) = NULL;
		if (tail == NULL) slab->bufctl = start;
		else IMEM_NEXT_PTR(tail) = start;
		tail = start;
		retval++;
	}

	return retval;
}


/* alloc data from slab */ 
static void *imslab_alloc(imemslab_t *slab)
{
	void *ptr;

	if (slab->bufctl == 0) return 0;
	ptr = slab->bufctl;
	slab->bufctl = IMEM_NEXT_PTR(slab->bufctl);
	slab->inuse++;

	return ptr;
}


/* free data into slab */
static void imslab_free(imemslab_t *slab, void *ptr)
{
	char *start = ((char*)slab->membase) + slab->coloroff;
	char *endup = ((char*)slab->membase) + slab->memsize;
	char *p = (char*)ptr;

	assert(slab->inuse > 0);
	assert(p >= start && p < endup);

	if (p >= start && p < endup) {
		IMEM_NEXT_PTR(p) = slab->bufctl;
		slab->bufctl = p;
	}

	slab->inuse--;
}



/*====================================================================*/
/* IMUTEX - mutex interfaces                                          */
/*====================================================================*/
int imutex_disable = 0;

void imutex_init(imutex_t *mutex)
{
	#ifdef IMUTEX_INIT
	IMUTEX_INIT(mutex);
	#endif
}

void imutex_destroy(imutex_t *mutex)
{
	#ifdef IMUTEX_DESTROY
	IMUTEX_DESTROY(mutex);
	#endif
}

void imutex_lock(imutex_t *mutex)
{
	#ifdef IMUTEX_LOCK
	if (imutex_disable == 0)
		IMUTEX_LOCK(mutex);
	#endif
}

void imutex_unlock(imutex_t *mutex)
{
	#ifdef IMUTEX_UNLOCK
	if (imutex_disable == 0)
		IMUTEX_UNLOCK(mutex);
	#endif
}


/*====================================================================*/
/* IMEMGFP (mem_get_free_pages) - a page-supplyer class               */
/*====================================================================*/
static struct IMEMNODE imem_page_cache;
static imemgfp_t imem_gfp_default;
static imutex_t imem_gfp_lock;
static size_t imem_page_size;
static size_t imem_page_shift;
static int imem_gfp_malloc = 0;
static int imem_gfp_inited = 0;


static void* imem_gfp_alloc(imemgfp_t *gfp)
{
	ilong index;
	char *lptr;

	if (gfp != NULL && gfp != &imem_gfp_default) 
		return gfp->alloc_page(gfp);

	if (imem_gfp_malloc) {
		lptr = (char*)internal_malloc(0, imem_page_size);
		if (lptr == NULL) {
			return NULL;
		}
	}	else {
		assert(imem_gfp_inited);
		
		imutex_lock(&imem_gfp_lock);
		index = imnode_new(&imem_page_cache);
		assert(index >= 0);

		if (index < 0) {
			imutex_unlock(&imem_gfp_lock);
			return NULL;
		}

		lptr = (char*)IMNODE_DATA(&imem_page_cache, index);
		imutex_unlock(&imem_gfp_lock);

		*(ilong*)lptr = index;
		lptr += IMROUNDUP(sizeof(ilong));
	}

	imem_gfp_default.pages_new++;
	imem_gfp_default.pages_inuse++;

	return lptr;
}

static void imem_gfp_free(imemgfp_t *gfp, void *ptr)
{
	ilong index;
	char *lptr;
	int invalidptr;
	
	if (gfp != NULL && gfp != &imem_gfp_default) {
		gfp->free_page(gfp, ptr);
		return;
	}

	if (imem_gfp_malloc) {
		internal_free(0, ptr);

	}	else {
		lptr = (char*)ptr - IMROUNDUP(sizeof(ilong));
		index = *(ilong*)lptr;

		invalidptr = (index < 0 || index >= imem_page_cache.node_max);
		assert( !invalidptr );

		if (invalidptr) return;

		imutex_lock(&imem_gfp_lock);

		if ((char*)IMNODE_DATA(&imem_page_cache, index) != lptr ||
			IMNODE_MODE(&imem_page_cache, index) == 0)
			invalidptr = 1;

		assert( !invalidptr );
		if (invalidptr) {
			imutex_unlock(&imem_gfp_lock);
			return;
		}
		
		imnode_del(&imem_page_cache, index);
		imutex_unlock(&imem_gfp_lock);
	}

	imem_gfp_default.pages_del++;
	imem_gfp_default.pages_inuse--;
}

static void imem_gfp_init(int page_shift, int use_malloc)
{
	size_t require_size;

	if (imem_gfp_inited != 0) 
		return;

	if (page_shift <= 0) 
		page_shift = IDEFAULT_PAGE_SHIFT;

	imem_page_shift = (size_t)page_shift;
	imem_page_size = (((size_t)1) << page_shift) + 
		IMROUNDUP(sizeof(void*)) * 16;

	require_size = imem_page_size + IMROUNDUP(sizeof(void*));
	imnode_init(&imem_page_cache, require_size, 0);

	imem_page_cache.grow_limit = 8;

	imutex_init(&imem_gfp_lock);

	imem_gfp_default.page_size = imem_page_size;
	imem_gfp_default.alloc_page = imem_gfp_alloc;
	imem_gfp_default.free_page = imem_gfp_free;
	imem_gfp_default.refcnt = 0;
	imem_gfp_default.extra = NULL;
	imem_gfp_default.pages_new = 0;
	imem_gfp_default.pages_del = 0;
	imem_gfp_default.pages_inuse = 0;

	imem_gfp_malloc = use_malloc;

	imem_gfp_inited = 1;
}

static void imem_gfp_destroy(void)
{
	if (imem_gfp_inited == 0)
		return;

	imutex_lock(&imem_gfp_lock);
	imnode_destroy(&imem_page_cache);
	imutex_unlock(&imem_gfp_lock);

	imutex_destroy(&imem_gfp_lock);
	imem_gfp_inited = 0;
}



/*====================================================================*/
/* SLAB SET                                                           */
/*====================================================================*/
static struct IMEMNODE imslab_cache;
static imutex_t imslab_lock;
static int imslab_inited = 0;

static void imslab_set_init(void)
{
	size_t size;
	if (imslab_inited != 0) 
		return;
	imutex_init(&imslab_lock);
	size = sizeof(imemslab_t) + IMROUNDUP(sizeof(void*));
	imnode_init(&imslab_cache, size, 0);
	imslab_inited = 1;
}

static void imslab_set_destroy(void)
{
	if (imslab_inited == 0)
		return;

	imutex_lock(&imslab_lock);
	imnode_destroy(&imslab_cache);
	imutex_unlock(&imslab_lock);

	imutex_destroy(&imslab_lock);
	imslab_inited = 0;
}

static imemslab_t *imslab_set_new(void)
{
	ilong index;
	char *lptr;

	assert(imslab_inited != 0);

	imutex_lock(&imslab_lock);
	index = imnode_new(&imslab_cache);
	
	assert(index >= 0);
	if (index < 0) {
		imutex_unlock(&imslab_lock);
		return NULL;
	}

	lptr = (char*)IMNODE_DATA(&imslab_cache, index);
	imutex_unlock(&imslab_lock);

	*(ilong*)lptr = index;
	lptr += IMROUNDUP(sizeof(ilong));

	return (imemslab_t*)lptr;
}

static void imslab_set_delete(imemslab_t *slab)
{
	char *lptr = (char*)slab;
	ilong index;
	int invalidptr;

	lptr -= IMROUNDUP(sizeof(ilong));
	index = *(ilong*)lptr;

	invalidptr = (index < 0 || index >= imem_page_cache.node_max);
	assert( !invalidptr );

	if (invalidptr) return;

	imutex_lock(&imslab_lock);
	
	if ((char*)IMNODE_DATA(&imslab_cache, index) != lptr ||
		IMNODE_MODE(&imslab_cache, index) == 0)
		invalidptr = 1;

	assert( !invalidptr );
	if (invalidptr) {
		imutex_unlock(&imslab_lock);
		return;
	}

	imnode_del(&imslab_cache, index);
	imutex_unlock(&imslab_lock);
}


/*====================================================================*/
/* IMEMCACHE                                                          */
/*====================================================================*/
#define IMCACHE_FLAG_OFFSLAB	1
#define IMCACHE_FLAG_NODRAIN	2
#define IMCACHE_FLAG_NOLOCK		4
#define IMCACHE_FLAG_SYSTEM		8
#define IMCACHE_FLAG_ONQUEUE	16

#define IMCACHE_OFFSLAB(mlist) ((mlist)->flags & IMCACHE_FLAG_OFFSLAB)
#define IMCACHE_NODRAIN(mlist) ((mlist)->flags & IMCACHE_FLAG_NODRAIN)
#define IMCACHE_NOLOCK(mlist)  ((mlist)->flags & IMCACHE_FLAG_NOLOCK)
#define IMCACHE_SYSTEM(mlist)  ((mlist)->flags & IMCACHE_FLAG_SYSTEM)
#define IMCACHE_ONQUEUE(mlist) ((mlist)->flags & IMCACHE_FLAG_ONQUEUE)


static void imemcache_calculate(imemcache_t *cache)
{
	size_t obj_num;
	size_t unit_size;
	size_t size;
	size_t color;
	int mustonslab = 0;

	unit_size = cache->unit_size;

	if (unit_size > IMROUNDUP(sizeof(imemslab_t))) {
		obj_num = cache->page_size / unit_size;
		size = cache->page_size - obj_num * unit_size;
		if (size >= IMROUNDUP(sizeof(imemslab_t) + IMROUNDSIZE))
			mustonslab = 1;
	}

	if (unit_size >= (cache->page_size >> 3) && mustonslab == 0) 
		cache->flags |= IMCACHE_FLAG_OFFSLAB;

	if (unit_size >= 1024) cache->limit = 32;
	else if (unit_size >= 256) cache->limit = 48;
	else if (unit_size >= 128) cache->limit = 64;
	else if (unit_size >= 64) cache->limit = 96;
	else cache->limit = 128;

	cache->batchcount = (cache->limit + 1) >> 1;

	if (IMCACHE_OFFSLAB(cache)) {
		obj_num = cache->page_size / unit_size;
		color = cache->page_size - obj_num * unit_size;
	}	else {
		obj_num = (cache->page_size - IMROUNDUP(sizeof(imemslab_t)) -
			IMROUNDSIZE) / unit_size;
		color = (cache->page_size - obj_num * unit_size) - 
			IMROUNDUP(sizeof(imemslab_t)) - IMROUNDSIZE;
	}
	color = color > unit_size ? unit_size : color;
	cache->num = obj_num;
	cache->color_limit = color;
}

static void imemcache_init_list(imemcache_t *cache, imemgfp_t *gfp,
	size_t obj_size)
{
	size_t limit;
	int i;

	assert(imslab_inited && imem_gfp_inited && cache);
	assert(obj_size >= sizeof(void*));

	cache->gfp = gfp;

	if (gfp == NULL) gfp = &imem_gfp_default;
	iqueue_init(&cache->slabs_free);
	iqueue_init(&cache->slabs_partial);
	iqueue_init(&cache->slabs_full);

	cache->page_size = gfp->page_size;
	cache->obj_size = obj_size;
	cache->unit_size = IMROUNDUP(cache->obj_size + sizeof(void*));
	cache->flags = 0;
	cache->num = 0;
	cache->count_free = 0;
	cache->count_partial = 0;
	cache->count_full = 0;
	cache->free_objects = 0;
	cache->free_limit = 0;
	cache->color_limit = 0;
	cache->color_next = 0;
	cache->extra = NULL;

	imutex_init(&cache->list_lock);

	imemcache_calculate(cache);

	limit = cache->batchcount + cache->num;
	cache->free_limit = limit;
	limit = (limit >= IMCACHE_ARRAYLIMIT)? IMCACHE_ARRAYLIMIT : limit;

	for (i = 0; i < (int)IMCACHE_LRU_COUNT; i++) {
		cache->array[i].avial = 0;
		cache->array[i].batchcount = (int)(limit >> 1);
		cache->array[i].limit = (int)limit;
		imutex_init(&cache->array[i].lock);
	}

	iqueue_init(&cache->queue);

	cache->pages_new = 0;
	cache->pages_del = 0;
	cache->pages_hiwater = 0;
	cache->pages_inuse = 0;
}

static imemslab_t *imemcache_slab_create(imemcache_t *cache, ilong *num)
{
	imemslab_t *slab;
	size_t coloroff;
	size_t obj_size;
	ilong count;
	char *page;

	obj_size = cache->unit_size;
	page = (char*)imem_gfp_alloc(cache->gfp);

	if (page == NULL) {
		return NULL;
	}

	coloroff = cache->color_next;
	if (IMCACHE_OFFSLAB(cache)) {
		slab = imslab_set_new();
		if (slab == NULL) {
			imem_gfp_free(cache->gfp, page);
			return NULL;
		}
	}	else {
		coloroff = IMROUNDUP((size_t)(page + coloroff));
		coloroff -= (size_t)page;
		slab = (imemslab_t*)(page + coloroff);
		coloroff += sizeof(imemslab_t);
	}

	assert(IMROUNDUP((size_t)slab) == (size_t)slab);

	count = imslab_init(slab, page, cache->page_size, obj_size, coloroff);

	cache->color_next += IMROUNDSIZE;

	if (cache->color_next >= cache->color_limit)
		cache->color_next = 0;

	slab->extra = cache;
	cache->pages_new++;
	cache->pages_inuse++;

	if (num) *num = count;

	return slab;
}

static void imemcache_slab_delete(imemcache_t *cache, imemslab_t *slab)
{
	imem_gfp_free(cache->gfp, slab->membase);
	cache->pages_del++;
	cache->pages_inuse--;
	if (IMCACHE_OFFSLAB(cache)) {
		imslab_set_delete(slab);
	}
}

static ilong imemcache_drain_list(imemcache_t *cache, int id, ilong tofree)
{
	imemslab_t *slab;
	iqueue_head *head, *p;
	ilong free_count = 0;

	if (id == 0) head = &cache->slabs_free;
	else if (id == 1) head = &cache->slabs_full;
	else if (id == 2) head = &cache->slabs_partial;
	else return -1;

	while (!iqueue_is_empty(head)) {
		if (tofree >= 0 && free_count >= tofree) break;
		imutex_lock(&cache->list_lock);
		p = head->prev;
		if (p == head) {
			imutex_unlock(&cache->list_lock);
			break;
		}
		slab = iqueue_entry(p, imemslab_t, queue);
		iqueue_del(p);
		imutex_unlock(&cache->list_lock);
		if (id == 0) {
			while (!IMEMSLAB_ISFULL(slab)) imslab_alloc(slab);
			cache->free_objects -= slab->inuse;
		}
		imemcache_slab_delete(cache, slab);
		free_count++;
	}
	if (id == 0) cache->count_free -= free_count;
	else if (id == 1) cache->count_full -= free_count;
	else cache->count_partial -= free_count;
	return free_count;
}

static void imemcache_destroy_list(imemcache_t *cache)
{
	int i;

	for (i = 0; i < (int)IMCACHE_LRU_COUNT; i++) 
		imutex_destroy(&cache->array[i].lock);
	
	imemcache_drain_list(cache, 0, -1);
	imemcache_drain_list(cache, 1, -1);
	imemcache_drain_list(cache, 2, -1);

	imutex_lock(&cache->list_lock);
	cache->count_free = 0;
	cache->count_full = 0;
	cache->count_partial = 0;
	cache->free_objects = 0;
	imutex_unlock(&cache->list_lock);

	imutex_destroy(&cache->list_lock);
}

#define IMCACHE_CHECK_MAGIC		0x05

static void* imemcache_list_alloc(imemcache_t *cache)
{
	imemslab_t *slab;
	iqueue_head *p;
	ilong slab_obj_num;
	char *lptr = NULL;

	if (IMCACHE_NOLOCK(cache) == 0)
		imutex_lock(&cache->list_lock);

	p = cache->slabs_partial.next;
	if (p == &cache->slabs_partial) {
		p = cache->slabs_free.next;
		if (p == &cache->slabs_free) {
			slab = imemcache_slab_create(cache, &slab_obj_num);
			if (slab == NULL) {
				if (IMCACHE_NOLOCK(cache) == 0)
					imutex_unlock(&cache->list_lock);
				return NULL;
			}
			p = &slab->queue;
			cache->free_objects += slab_obj_num;
			slab = iqueue_entry(p, imemslab_t, queue);
		}	else {
			iqueue_del(p);
			iqueue_init(p);
			cache->count_free--;
			slab = iqueue_entry(p, imemslab_t, queue);
		}
		iqueue_add(p, &cache->slabs_partial);
		cache->count_partial++;
	}
	slab = iqueue_entry(p, imemslab_t, queue);
	assert(IMEMSLAB_ISFULL(slab) == 0);
	lptr = (char*)imslab_alloc(slab);
	assert(lptr);
	if (cache->free_objects) cache->free_objects--;
	if (IMEMSLAB_ISFULL(slab)) {
		iqueue_del(p);
		iqueue_init(p);
		iqueue_add(p, &cache->slabs_full);
		cache->count_partial--;
		cache->count_full++;
	}
	if (IMCACHE_NOLOCK(cache) == 0)
		imutex_unlock(&cache->list_lock);
	*(imemslab_t**)lptr = slab;
	lptr += sizeof(imemslab_t*);
	return lptr;
}

static void imemcache_list_free(imemcache_t *cache, void *ptr)
{
	imemslab_t *slab;
	iqueue_head *p;
	char *lptr = (char*)ptr;
	char *membase;
	int invalidptr;
	ilong tofree;

	lptr -= sizeof(imemslab_t*);
	slab = *(imemslab_t**)lptr;
	assert(slab);

	membase = (char*)slab->membase;
	invalidptr = !(lptr >= membase && lptr < membase + slab->memsize);

	assert( !invalidptr );
	if (invalidptr) return;
	if (cache != NULL) {
		invalidptr = ((void*)cache != slab->extra);
		assert( !invalidptr );
		if (invalidptr) return;
	}

	cache = (imemcache_t*)slab->extra;
	p = &slab->queue;

	if (IMCACHE_NOLOCK(cache) == 0)
		imutex_lock(&cache->list_lock);

	if (IMEMSLAB_ISFULL(slab)) {
		assert(cache->count_full);
		iqueue_del(p);
		iqueue_init(p);
		iqueue_add_tail(p, &cache->slabs_partial);
		cache->count_full--;
		cache->count_partial++;
	}
	imslab_free(slab, lptr);
	cache->free_objects++;

	if (IMEMSLAB_ISEMPTY(slab)) {
		iqueue_del(p);
		iqueue_init(p);
		iqueue_add(p, &cache->slabs_free);
		cache->count_partial--;
		cache->count_free++;
	}

	if (IMCACHE_NOLOCK(cache) == 0)
		imutex_unlock(&cache->list_lock);

	if (IMCACHE_NODRAIN(cache) == 0) {
		if (cache->free_objects >= cache->free_limit) {
			tofree = cache->count_free >> 1;
			if (tofree > 0)
				tofree = imemcache_drain_list(cache, 0, tofree);
		}
	}
}


/*====================================================================*/
/* IMEMECACHE INTERFACE                                               */
/*====================================================================*/
static int imemcache_fill_batch(imemcache_t *cache, int array_index)
{
	imemlru_t *array = &cache->array[array_index];
	int count = 0;
	void *ptr;

	imutex_lock(&cache->list_lock);

	for (count = 0; array->avial < array->batchcount; count++) {
		ptr = imemcache_list_alloc(cache);
		if (ptr == NULL) break;
		array->entry[array->avial++] = ptr;
	}

	imutex_unlock(&cache->list_lock);

	if (cache->pages_inuse > cache->pages_hiwater)
		cache->pages_hiwater = cache->pages_inuse;

	return count;
}


static void *imemcache_alloc(imemcache_t *cache)
{
	imemlru_t *array;
	int array_index = 0;
	void *ptr = NULL;
	void **head;

	array_index = 0;
	array_index &= (IMCACHE_LRU_COUNT - 1);

	array = &cache->array[array_index];

	imutex_lock(&array->lock);
	if (array->avial == 0)
		imemcache_fill_batch(cache, array_index);
	if (array->avial != 0) {
		ptr = array->entry[--array->avial];
	}
	imutex_unlock(&array->lock);

	if (ptr == 0) {
		return NULL;
	}

	assert(ptr);
	head = (void**)((char*)ptr - sizeof(void*));
	head[0] = (void*)((size_t)head[0] | IMCACHE_CHECK_MAGIC);

	return ptr;
}

static void *imemcache_free(imemcache_t *cache, void *ptr)
{
	imemslab_t *slab;
	imemlru_t *array;
	size_t linear;
	char *lptr = (char*)ptr;
	void **head;
	int array_index = 0;
	int invalidptr, count;

	array_index = 0;
	array_index &= (IMCACHE_LRU_COUNT - 1);
	
	head = (void**)(lptr - sizeof(void*));
	linear = (size_t)head[0];
	invalidptr = ((linear & IMCACHE_CHECK_MAGIC) != IMCACHE_CHECK_MAGIC);
	head[0] = (void*)(linear & ~(IMROUNDSIZE - 1));

	assert( !invalidptr );
	if (invalidptr) return NULL;

	lptr -= sizeof(imemslab_t*);
	slab = *(imemslab_t**)lptr;

	if (cache) {
		assert(cache == (imemcache_t*)slab->extra);
		if (cache != slab->extra) {
			return NULL;
		}
	}

	cache = (imemcache_t*)slab->extra;
	array = &cache->array[array_index];

	imutex_lock(&array->lock);
	
	if (array->avial >= array->limit) {

		imutex_lock(&cache->list_lock);

		for (count = 0; array->avial > array->batchcount; count++)
			imemcache_list_free(cache, array->entry[--array->avial]);

		imemcache_list_free(cache, ptr);

		imutex_unlock(&cache->list_lock);

		if (cache->free_objects >= cache->free_limit) {
			if (cache->count_free > 1) 
				imemcache_drain_list(cache, 0, cache->count_free >> 1);
		}

	}	else {

		array->entry[array->avial++] = ptr;
	}

	imutex_unlock(&array->lock);

	return cache;
}


static void imemcache_shrink(imemcache_t *cache)
{
	imemlru_t *array;
	int array_index = 0;

	array_index = 0;
	array_index &= (IMCACHE_LRU_COUNT - 1);

	array = &cache->array[array_index];

	imutex_lock(&array->lock);
	imutex_lock(&cache->list_lock);

	for (; array->avial > 0; ) 
		imemcache_list_free(cache, array->entry[--array->avial]);

	imemcache_drain_list(cache, 0, -1);

	imutex_unlock(&cache->list_lock);
	imutex_unlock(&array->lock);
}


static void *imemcache_gfp_alloc(struct IMEMGFP *gfp)
{
	imemcache_t *cache = (imemcache_t*)gfp->extra;
	char *lptr;
	lptr = (char*)imemcache_alloc(cache);
	assert(lptr);
	return lptr;
}

static void imemcache_gfp_free(struct IMEMGFP *gfp, void *ptr)
{
	imemcache_t *cache = (imemcache_t*)gfp->extra;
	char *lptr = (char*)ptr;;
	imemcache_free(cache, lptr);
}

static imemcache_t *imemcache_create(const char *name, 
		size_t obj_size, struct IMEMGFP *gfp)
{
	imemcache_t *cache;
	size_t page_size;

	assert(imslab_inited && imem_gfp_inited);
	assert(obj_size >= sizeof(void*));

	page_size = (!gfp)? imem_gfp_default.page_size : gfp->page_size;
	assert(obj_size > 0 && obj_size <= page_size - IMROUNDSIZE);

	cache = (imemcache_t*)internal_malloc(0, sizeof(imemcache_t));
	assert(cache);

	imemcache_init_list(cache, gfp, obj_size);

	name = name? name : "NONAME";
	strncpy(cache->name, name, IMCACHE_NAMESIZE);

	if (cache->gfp) cache->gfp->refcnt++;
	cache->flags |= IMCACHE_FLAG_NODRAIN | IMCACHE_FLAG_NOLOCK;

	iqueue_init(&cache->queue);

	gfp = &cache->page_supply;
	gfp->page_size = cache->obj_size;
	gfp->refcnt = 0;
	gfp->alloc_page = imemcache_gfp_alloc;
	gfp->free_page = imemcache_gfp_free;
	gfp->extra = cache;
	gfp->pages_inuse = 0;
	gfp->pages_new = 0;
	gfp->pages_del = 0;

	return cache;
}

static void imemcache_release(imemcache_t *cache)
{
	imemlru_t *array;
	void *entry[IMCACHE_ARRAYLIMIT];
	ilong n, i, j;

	for (i = 0; i < IMCACHE_LRU_COUNT; i++) {
		array = &cache->array[i];

		imutex_lock(&array->lock);
		for (j = 0, n = 0; j < array->avial; j++, n++) 
			entry[j] = array->entry[j];
		array->avial = 0;
		imutex_unlock(&array->lock);

		imutex_lock(&cache->list_lock);

		for (j = 0; j < n; j++) 
			imemcache_list_free(NULL, entry[j]);

		imutex_unlock(&cache->list_lock);
	}

	imemcache_destroy_list(cache);

	if (cache->gfp) 
		cache->gfp->refcnt--;
	
	internal_free(0, cache);
}



/*====================================================================*/
/* IKMEM INTERFACE                                                    */
/*====================================================================*/
static imemcache_t **ikmem_array = NULL;
static imemcache_t **ikmem_lookup = NULL;

static imutex_t ikmem_lock;
static int ikmem_count = 0;
static int ikmem_inited = 0;

static imemcache_t *ikmem_size_lookup1[257];
static imemcache_t *ikmem_size_lookup2[257];

static size_t ikmem_inuse = 0;
static iqueue_head ikmem_head;
static iqueue_head ikmem_large_ptr;

static size_t ikmem_water_mark = 0;

static size_t ikmem_range_high = 0;
static size_t ikmem_range_low = 0;

static const ikmemhook_t *ikmem_hook = NULL;


static int ikmem_append(size_t size, struct IMEMGFP *gfp)
{
	imemcache_t *cache, **p1, **p2;
	static int sizelimit = 0;
	char name[64];
	char nums[32];
	int index, k;
	size_t num;

	strncpy(name, "kmem_", 20);

	for (num = size, index = 0; ; ) {
		nums[index++] = (char)((num % 10) + '0');
		num /= 10;
		if (num == 0) break;
	}

	for (k = 0; k < index; k++) 
		name[5 + k] = nums[index - 1 - k];

	name[5 + index] = 0;

	cache = imemcache_create(name, size, gfp);
	
	#ifdef IKMEM_MINWASTE
	cache->array[0].limit = 2;
	cache->array[0].batchcount = 1;
	cache->free_limit = 1;
	#endif

	assert(cache);
	
	cache->flags |= IMCACHE_FLAG_SYSTEM;

	if (ikmem_count == 0) sizelimit = 0;
	if (ikmem_count >= sizelimit || ikmem_array == 0) {
		sizelimit = (sizelimit <= 0)? 8 : sizelimit * 2;
		sizelimit = (sizelimit < ikmem_count)? ikmem_count + 2 : sizelimit;
	
		p1 = (imemcache_t**)internal_malloc(0, 
								sizeof(imemcache_t) * sizelimit * 2);
		p2 = p1 + sizelimit;

		assert(p1);

		if (ikmem_array != NULL) {
			memcpy(p1, ikmem_array, sizeof(imemcache_t) * ikmem_count);
			memcpy(p2, ikmem_lookup, sizeof(imemcache_t) * ikmem_count);
			internal_free(0, ikmem_array);
		}
		ikmem_array = p1;
		ikmem_lookup = p2;
	}

	ikmem_array[ikmem_count] = cache;
	ikmem_lookup[ikmem_count++] = cache;

	for (index = ikmem_count - 1; index > 1; index = k) {
		k = index - 1;
		if (ikmem_lookup[index]->obj_size < ikmem_lookup[k]->obj_size)
			break;
		cache = ikmem_lookup[index];
		ikmem_lookup[index] = ikmem_lookup[k];
		ikmem_lookup[k] = cache;
	}

	return ikmem_count - 1;
}


static size_t ikmem_page_waste(size_t obj_size, 
		size_t page_size)
{
	imemcache_t cache;
	size_t size, k;
	cache.obj_size = (size_t)obj_size;
	cache.page_size = (size_t)page_size;
	cache.unit_size = IMROUNDUP(obj_size + sizeof(void*));
	cache.flags = 0;
	imemcache_calculate(&cache);
	size = IMROUNDUP(cache.obj_size + sizeof(void*)) * cache.num;
	size = page_size - size;
	k = IMROUNDUP(sizeof(imemslab_t));
	if (IMCACHE_OFFSLAB(&cache)) size += k;
	return size;
}

static size_t ikmem_gfp_waste(size_t obj_size, imemgfp_t *gfp)
{
	imemcache_t *cache;
	size_t waste;
	if (gfp == NULL) {
		return ikmem_page_waste(obj_size, imem_gfp_default.page_size);
	}
	cache = (imemcache_t*)gfp->extra;
	waste = ikmem_page_waste(obj_size, cache->obj_size) * cache->num;
	return waste + ikmem_gfp_waste(cache->obj_size, cache->gfp);
}

static imemgfp_t *ikmem_choose_gfp(size_t obj_size, ilong *w)
{
	size_t hiwater = IMROUNDUP(obj_size + sizeof(void*)) * 64;
	size_t lowater = IMROUNDUP(obj_size + sizeof(void*)) * 8;
	imemcache_t *cache;
	size_t min, waste;
	int index, i = -1;

	min = imem_gfp_default.page_size;
	for (index = 0; index < ikmem_count; index++) {
		cache = ikmem_array[index];
		if (cache->obj_size < lowater || cache->obj_size > hiwater)
			continue;
		waste = ikmem_gfp_waste(obj_size, &ikmem_array[index]->page_supply);
		if (waste < min) min = waste, i = index;
	}
	if (i < 0 || i >= ikmem_count) {
		if (w) w[0] = (ilong)ikmem_gfp_waste(obj_size, NULL);
		return NULL;
	}
	if (w) w[0] = (ilong)min;
	return &ikmem_array[i]->page_supply;
}

static void ikmem_insert(size_t objsize, int approxy)
{
	imemgfp_t *gfp;
	size_t optimize;
	ilong index, waste;

	for (index = 0; index < ikmem_count; index++) {
		optimize = ikmem_array[index]->obj_size;
		if (optimize < objsize) continue;
		if (optimize == objsize) break;
		if (optimize - objsize <= (objsize >> 3) && approxy) break;
	}

	if (index < ikmem_count) 
		return;

	gfp = ikmem_choose_gfp(objsize, &waste);
	ikmem_append(objsize, gfp);
}

static imemcache_t *ikmem_choose_size(size_t size)
{
	int index;
	if (size >= imem_page_size) return NULL;
	if (ikmem_count > 0) if (size > ikmem_lookup[0]->obj_size) return NULL;
	for (index = ikmem_count - 1; index >= 0; index--) {
		if (ikmem_lookup[index]->obj_size >= size) 
			return ikmem_lookup[index];
	}
	return NULL;
}

static void ikmem_setup_caches(size_t *sizelist)
{
	size_t fib1 = 8, fib2 = 16, f;
	size_t *sizevec, *p;
	size_t k = 0;
	ilong limit, shift, count, i, j;
	imemcache_t *cache;

	limit = 32;
	sizevec = (size_t*)internal_malloc(0, sizeof(ilong) * limit);
	assert(sizevec);

	#define ikmem_sizevec_append(size) do { \
		if (count >= limit) {	\
			limit = limit * 2;	\
			p = (size_t*)internal_malloc(0, sizeof(ilong) * limit);	\
			assert(p);	\
			memcpy(p, sizevec, sizeof(ilong) * count);	\
			free(sizevec);	\
			sizevec = p;	\
		}	\
		sizevec[count++] = (size); \
	}	while (0)

	shift = imem_page_shift;
	for (count = 0; shift >= 3; shift--) {
		ikmem_sizevec_append(((size_t)1) << shift);
	}

	for (; fib2 < (imem_gfp_default.page_size >> 2); ) {
		f = fib1 + fib2;
		fib1 = fib2;
		fib2 = f;
		#ifdef IKMEM_USEFIB
		ikmem_sizevec_append(f);
		#endif
	}

	for (i = 0; i < count - 1; i++) {
		for (j = i + 1; j < count; j++) {
			if (sizevec[i] < sizevec[j]) 
				k = sizevec[i], 
				sizevec[i] = sizevec[j], 
				sizevec[j] = k;
		}
	}

	for (i = 0; i < count; i++) ikmem_insert(sizevec[i], 1);
	internal_free(0, sizevec);

	if (sizelist) {
		for (; sizelist[0]; sizelist++) 
			ikmem_insert(*sizelist, 0);
	}

	for (f = 4; f <= 1024; f += 4) 
		ikmem_size_lookup1[f >> 2] = ikmem_choose_size(f);

	for (f = 1024; f <= (256 << 10); f += 1024) 
		ikmem_size_lookup2[f >> 10] = ikmem_choose_size(f);

	for (i = 0; i < ikmem_count; i++) {
		cache = ikmem_array[i];
		cache->extra = (ilong*)internal_malloc(0, sizeof(ilong) * 8);
		assert(cache->extra);
		memset(cache->extra, 0, sizeof(ilong) * 8);
	}

	ikmem_size_lookup1[0] = NULL;
	ikmem_size_lookup2[0] = NULL;
}


void ikmem_init(int page_shift, int pg_malloc, size_t *sz)
{
	size_t psize;
	ilong limit;

	assert(ikmem_inited == 0 && imslab_inited == 0);
	assert(imem_gfp_inited == 0);

	imem_gfp_init(page_shift, pg_malloc);
	imslab_set_init();
	
	ikmem_lookup = NULL;
	ikmem_array = NULL;
	ikmem_count = 0;
	ikmem_inuse = 0;

	psize = imem_gfp_default.page_size - IMROUNDUP(sizeof(void*));
	ikmem_append(psize, 0);

	limit = imem_page_shift - 4;
	limit = limit < 10? 10 : limit;
	
	ikmem_setup_caches(sz);

	imutex_init(&ikmem_lock);
	iqueue_init(&ikmem_head);
	iqueue_init(&ikmem_large_ptr);

	ikmem_range_high = (size_t)0;
	ikmem_range_low = ~((size_t)0);

	ikmem_inited = 1;
}

void ikmem_destroy(void)
{
	imemcache_t *cache;
	iqueue_head *p, *next;
	ilong index;

	if (ikmem_inited == 0) {
		return;
	}

	imutex_lock(&ikmem_lock);
	for (p = ikmem_head.next; p != &ikmem_head; ) {
		cache = IQUEUE_ENTRY(p, imemcache_t, queue);
		p = p->next;
		iqueue_del(&cache->queue);
		imemcache_release(cache);
	}

	imutex_unlock(&ikmem_lock);

	for (index = ikmem_count - 1; index >= 0; index--) {
		cache = ikmem_array[index];
		if (cache->extra) {
			internal_free(0, cache->extra);
			cache->extra = NULL;
		}
		imemcache_release(cache);
	}

	internal_free(0, ikmem_array);
	ikmem_lookup = NULL;
	ikmem_array = NULL;
	ikmem_count = 0;

	imutex_lock(&ikmem_lock);

	for (p = ikmem_large_ptr.next; p != &ikmem_large_ptr; ) {
		next = p->next;
		iqueue_del(p);
		internal_free(0, p);
		p = next;
	}

	imutex_unlock(&ikmem_lock);
	imutex_destroy(&ikmem_lock);

	imslab_set_destroy();
	imem_gfp_destroy();

	ikmem_inited = 0;
}

#define IKMEM_LARGE_HEAD	\
	IMROUNDUP(sizeof(iqueue_head) + sizeof(void*) + sizeof(ilong))

#define IKMEM_STAT(cache, id) (((ilong*)((cache)->extra))[id])

void* ikmem_malloc(size_t size)
{
	imemcache_t *cache = NULL;
	size_t round;
	iqueue_head *p;
	char *lptr;

	if (ikmem_hook) {
		return ikmem_hook->kmem_malloc_fn(size);
	}

	if (ikmem_inited == 0) ikmem_init(0, 0, NULL);

	assert(size > 0 && size <= (((size_t)1) << 30));
	round = (size + 3) & ~((size_t)3);

	if (round <= 1024) {
		cache = ikmem_size_lookup1[round >> 2];
	}	else {
		round = (size + 1023) & ~((size_t)1023);
		if (round < (256 << 10)) 
			cache = ikmem_size_lookup2[round >> 10];
	}

	if (cache == NULL)
		cache = ikmem_choose_size(size);

	if (ikmem_water_mark > 0 && size > ikmem_water_mark)
		cache = NULL;

	if (cache == NULL) {
		lptr = (char*)internal_malloc(0, IKMEM_LARGE_HEAD + size);
		if (lptr == NULL) return NULL;
		
		p = (iqueue_head*)lptr;
		lptr += IKMEM_LARGE_HEAD;
		*(void**)(lptr - sizeof(void*)) = NULL;
		*(ilong*)(lptr - sizeof(void*) - sizeof(ilong)) = size;

		imutex_lock(&ikmem_lock);
		iqueue_add(p, &ikmem_large_ptr);
		imutex_unlock(&ikmem_lock);

	}	else {
		lptr = (char*)imemcache_alloc(cache);
		if (lptr == NULL) {
			ikmem_shrink();
			lptr = (char*)imemcache_alloc(cache);
			if (lptr == NULL) {
				return NULL;
			}
		}
		if (cache->extra) {
			IKMEM_STAT(cache, 0) += 1;
			IKMEM_STAT(cache, 1) += 1;
		}
		ikmem_inuse += cache->obj_size;
	}

	if (ikmem_range_high < (size_t)lptr)
		ikmem_range_high = (size_t)lptr;
	if (ikmem_range_low > (size_t)lptr)
		ikmem_range_low = (size_t)lptr;

	return lptr;
}

void ikmem_free(void *ptr)
{
	imemcache_t *cache = NULL;
	iqueue_head *p;
	char *lptr = (char*)ptr;

	if (ikmem_hook) {
		ikmem_hook->kmem_free_fn(ptr);
		return;
	}

	if (*(void**)(lptr - sizeof(void*)) == NULL) {
		lptr -= IKMEM_LARGE_HEAD;
		p = (iqueue_head*)lptr;
		imutex_lock(&ikmem_lock);
		iqueue_del(p);
		imutex_unlock(&ikmem_lock);
		internal_free(0, lptr);
	}	else {
		cache = (imemcache_t*)imemcache_free(NULL, ptr);
		if (cache == NULL) return;
		if (cache->extra) {
			IKMEM_STAT(cache, 0) -= 1;
			IKMEM_STAT(cache, 2) += 1;
		}
		ikmem_inuse -= cache->obj_size;
	}
}

size_t ikmem_ptr_size(const void *ptr)
{
	size_t size, linear;
	imemcache_t *cache;
	imemslab_t *slab;
	const char *lptr = (const char*)ptr;
	int invalidptr;

	if (ikmem_hook) {
		return ikmem_hook->kmem_ptr_size_fn(ptr);
	}

	if ((size_t)lptr < ikmem_range_low ||
		(size_t)lptr > ikmem_range_high) 
		return 0;

	if (*(const void**)(lptr - sizeof(void*)) == NULL) {
		size = *(const size_t*)(lptr - sizeof(void*) - sizeof(ilong));
	}	else {

		linear = (size_t)(*(const void**)(lptr - sizeof(void*)));
		invalidptr = ((linear & IMCACHE_CHECK_MAGIC) != IMCACHE_CHECK_MAGIC);

		if ( invalidptr ) return 0;

		slab = (imemslab_t*)(linear & ~(IMROUNDSIZE - 1));
		lptr -= sizeof(void*);

		if (lptr >= (const char*)slab->membase + slab->memsize ||
			lptr < (const char*)slab->membase) 
			return 0;

		cache = (imemcache_t*)slab->extra;
		size = cache->obj_size;
	}

	return size;
}

void* ikmem_realloc(void *ptr, size_t size)
{
	size_t oldsize;
	void *newptr;

	if (ikmem_hook) {
		return ikmem_hook->kmem_realloc_fn(ptr, size);
	}

	if (ptr == NULL) return ikmem_malloc(size);
	oldsize = ikmem_ptr_size(ptr);

	if (size == 0) {
		ikmem_free(ptr);
		return NULL;
	}

	assert(oldsize > 0);
	
	if (oldsize >= size) {
		if (oldsize * 3 < size * 4) 
			return ptr;
	}

	newptr = ikmem_malloc(size);

	if (newptr == NULL) {
		ikmem_free(ptr);
		return NULL;
	}

	memcpy(newptr, ptr, oldsize < size? oldsize : size);
	ikmem_free(ptr);

	return newptr;
}

void ikmem_shrink(void)
{
	imemcache_t *cache;
	int index;
	if (ikmem_hook) {
		if (ikmem_hook->kmem_shrink_fn)
			ikmem_hook->kmem_shrink_fn();
		return;
	}
	for (index = ikmem_count - 1; index >= 0; index--) {
		cache = ikmem_lookup[index];
		imemcache_shrink(cache);
	}
}

static imemcache_t* ikmem_search(const char *name, int needlock)
{
	imemcache_t *cache, *result;
	iqueue_head *head;
	ilong index;

	for (index = 0; index < ikmem_count; index++) {
		cache = ikmem_array[index];
		if (strcmp(cache->name, name) == 0) return cache;
	}
	result = NULL;
	if (needlock) imutex_lock(&ikmem_lock);
	for (head = ikmem_head.next; head != &ikmem_head; head = head->next) {
		cache = iqueue_entry(head, imemcache_t, queue);
		if (strcmp(cache->name, name) == 0) {
			result = cache;
			break;
		}
	}
	if (needlock) imutex_unlock(&ikmem_lock);
	return result;
}


void ikmem_option(size_t watermark)
{
	ikmem_water_mark = watermark;
}


imemcache_t *ikmem_get(const char *name)
{
	return ikmem_search(name, 1);
}


ilong ikmem_page_info(ilong *pg_inuse, ilong *pg_new, ilong *pg_del)
{
	if (pg_inuse) pg_inuse[0] = imem_gfp_default.pages_inuse;
	if (pg_new) pg_new[0] = imem_gfp_default.pages_new;
	if (pg_del) pg_del[0] = imem_gfp_default.pages_del;
	return imem_page_size;
}

ilong ikmem_cache_info(int id, int *inuse, int *cnew, int *cdel, int *cfree)
{
	imemcache_t *cache;
	ilong nfree, i;
	if (id < 0 || id >= ikmem_count) return -1;
	cache = ikmem_array[id];
	nfree = cache->free_objects;
	for (i = 0; i < IMCACHE_LRU_COUNT; i++) 
		nfree += cache->array[i].avial;
	if (cache->extra) {
		if (inuse) inuse[0] = (int)IKMEM_STAT(cache, 0);
		if (cnew) cnew[0] = (int)IKMEM_STAT(cache, 1);
		if (cdel) cdel[0] = (int)IKMEM_STAT(cache, 2);
	}
	if (cfree) cfree[0] = (int)nfree;
	return cache->obj_size;
}

ilong ikmem_waste_info(ilong *kmem_inuse, ilong *total_mem)
{
	size_t totalmem;
	totalmem = imem_page_size * imem_gfp_default.pages_inuse;
	if (kmem_inuse) kmem_inuse[0] = ikmem_inuse;
	if (total_mem) total_mem[0] = totalmem;
	return ikmem_inuse;
}



#ifndef IKMEM_CACHE_TYPE
#define IKMEM_CACHE_TYPE
typedef void* ikmem_cache_t;
#endif

imemcache_t *ikmem_create(const char *name, size_t size)
{
	imemcache_t *cache;
	imemgfp_t *gfp;
	
	if (ikmem_inited == 0) ikmem_init(0, 0, 0);
	if (size >= imem_page_size) return NULL;

	gfp = ikmem_choose_gfp(size, NULL);
	imutex_lock(&ikmem_lock);
	cache = ikmem_search(name, 0);
	if (cache != NULL) {
		imutex_unlock(&ikmem_lock);
		return NULL;
	}
	cache = imemcache_create(name, size, gfp);
	if (cache == NULL) {
		imutex_unlock(&ikmem_lock);
		return NULL;
	}
	cache->flags |= IMCACHE_FLAG_ONQUEUE;
	cache->user = (ilong)gfp;
	iqueue_add_tail(&ikmem_head, &cache->queue);
	imutex_unlock(&ikmem_lock);

	return cache;
}

void ikmem_delete(imemcache_t *cache)
{
	assert(IMCACHE_SYSTEM(cache) == 0);
	assert(IMCACHE_ONQUEUE(cache));
	if (IMCACHE_SYSTEM(cache)) return;
	if (IMCACHE_ONQUEUE(cache) == 0) return;
	imutex_lock(&ikmem_lock);
	iqueue_del(&cache->queue);
	imutex_unlock(&ikmem_lock);
	imemcache_release(cache); 
}

void *ikmem_cache_alloc(imemcache_t *cache)
{
	char *ptr;
	assert(cache);
	ptr = (char*)imemcache_alloc(cache);
	return ptr;
}

void ikmem_cache_free(imemcache_t *cache, void *ptr)
{
	imemcache_free(cache, ptr);
}


/*====================================================================*/
/* IKMEM HOOKING                                                      */
/*====================================================================*/
static void* ikmem_std_malloc(size_t size)
{
	size_t round = (size + 3) & ~((size_t)3);
	char *lptr;
	assert(size > 0);
	lptr = (char*)internal_malloc(0, round + sizeof(void*));
	if (lptr == NULL) return NULL;
	*((size_t*)lptr) = (round | 1);
	lptr += sizeof(void*);
	return lptr;
}

static void ikmem_std_free(void *ptr)
{
	char *lptr = ((char*)ptr) - sizeof(void*);
	size_t size = *((size_t*)lptr);
	int invalidptr;
	invalidptr = (size & 1) != 1;
	if (invalidptr) {
		assert(!invalidptr);
		return;
	}
	*((size_t*)ptr) = 0;
	internal_free(0, lptr);
}

static size_t ikmem_std_ptr_size(const void *ptr)
{
	const char *lptr = ((const char*)ptr) - sizeof(void*);
	size_t size = *((const size_t*)lptr);
	if ((size & 1) != 1) {
		assert((size & 1) == 1);
		return 0;
	}
	return size & (~((size_t)3));
}

static void* ikmem_std_realloc(void *ptr, size_t size)
{
	size_t oldsize;
	void *newptr;

	if (ptr == NULL) return ikmem_std_malloc(size);
	oldsize = ikmem_std_ptr_size(ptr);

	if (size == 0) {
		ikmem_std_free(ptr);
		return NULL;
	}

	assert(oldsize > 0);
	if (oldsize >= size) {
		if (oldsize * 3 < size * 4) 
			return ptr;
	}

	newptr = ikmem_std_malloc(size);
	if (newptr == NULL) {
		ikmem_std_free(ptr);
		return NULL;
	}

	memcpy(newptr, ptr, oldsize < size? oldsize : size);
	ikmem_std_free(ptr);

	return newptr;
}

static const ikmemhook_t ikmem_std_hook = 
{
	ikmem_std_malloc,
	ikmem_std_free,
	ikmem_std_realloc,
	ikmem_std_ptr_size,
	NULL,
};

static const ikmemhook_t ikmem_origin =
{
	ikmem_malloc,
	ikmem_free,
	ikmem_realloc,
	ikmem_ptr_size,
	ikmem_shrink,
};

int ikmem_hook_install(const ikmemhook_t *hook)
{
	if (ikmem_inited) return -1;
	if (hook == NULL) {
		ikmem_hook = NULL;
		return 0;
	}
	if (hook == (const ikmemhook_t*)(~((size_t)0))) {
		ikmem_hook = &ikmem_std_hook;
		return 0;
	}
	if (hook->kmem_malloc_fn == ikmem_malloc)
		return -1;
	if (hook->kmem_free_fn == ikmem_free)
		return -1;
	if (hook->kmem_realloc_fn == ikmem_realloc)
		return -1;
	if (hook->kmem_ptr_size_fn == ikmem_ptr_size)
		return -1;
	if (hook->kmem_shrink_fn == ikmem_shrink)
		return -1;
	ikmem_hook = hook;
	return 0;
}

const ikmemhook_t *ikmem_hook_get(int id)
{
	if (id == 0) return &ikmem_origin;
	return &ikmem_std_hook;
}


/*====================================================================*/
/* IVECTOR / IMEMNODE MANAGEMENT                                      */
/*====================================================================*/
static void* ikmem_allocator_malloc(struct IALLOCATOR *, size_t);
static void ikmem_allocator_free(struct IALLOCATOR *, void *);

struct IALLOCATOR ikmem_allocator = 
{
	ikmem_allocator_malloc,
	ikmem_allocator_free,
	0, 0
};


static void* ikmem_allocator_malloc(struct IALLOCATOR *a, size_t len)
{
	return ikmem_malloc(len);
}

static void ikmem_allocator_free(struct IALLOCATOR *a, void *ptr)
{
	assert(ptr);
	ikmem_free(ptr);
}


ivector_t *iv_create(void)
{
	ivector_t *vec;
	vec = (ivector_t*)ikmem_malloc(sizeof(ivector_t));
	if (vec == NULL) return NULL;
	iv_init(vec, &ikmem_allocator);
	return vec;
}

void iv_delete(ivector_t *vec)
{
	assert(vec);
	iv_destroy(vec);
	ikmem_free(vec);
}

imemnode_t *imnode_create(ilong nodesize, int grow_limit)
{
	imemnode_t *mnode;
	mnode = (imemnode_t*)ikmem_malloc(sizeof(imemnode_t));
	if (mnode == NULL) return NULL;
	imnode_init(mnode, nodesize, &ikmem_allocator);
	mnode->grow_limit = grow_limit;
	return mnode;
}

void imnode_delete(imemnode_t *mnode)
{
	assert(mnode);
	imnode_destroy(mnode);
	ikmem_free(mnode);
}




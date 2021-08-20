/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2020 nghttp2 contributors
 * Copyright (c) 2018 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGHTTP2_KSL_H
#define NGHTTP2_KSL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <nghttp2/nghttp2.h>

/*
 * Skip List using single key instead of range.
 */

#define NGHTTP2_KSL_DEGR 16
/* NGHTTP2_KSL_MAX_NBLK is the maximum number of nodes which a single
   block can contain. */
#define NGHTTP2_KSL_MAX_NBLK (2 * NGHTTP2_KSL_DEGR - 1)
/* NGHTTP2_KSL_MIN_NBLK is the minimum number of nodes which a single
   block other than root must contains. */
#define NGHTTP2_KSL_MIN_NBLK (NGHTTP2_KSL_DEGR - 1)

/*
 * nghttp2_ksl_key represents key in nghttp2_ksl.
 */
typedef void nghttp2_ksl_key;

struct nghttp2_ksl_node;
typedef struct nghttp2_ksl_node nghttp2_ksl_node;

struct nghttp2_ksl_blk;
typedef struct nghttp2_ksl_blk nghttp2_ksl_blk;

/*
 * nghttp2_ksl_node is a node which contains either nghttp2_ksl_blk or
 * opaque data.  If a node is an internal node, it contains
 * nghttp2_ksl_blk.  Otherwise, it has data.  The key is stored at the
 * location starting at key.
 */
struct nghttp2_ksl_node {
  union {
    nghttp2_ksl_blk *blk;
    void *data;
  };
  union {
    uint64_t align;
    /* key is a buffer to include key associated to this node.
       Because the length of key is unknown until nghttp2_ksl_init is
       called, the actual buffer will be allocated after this
       field. */
    uint8_t key[1];
  };
};

/*
 * nghttp2_ksl_blk contains nghttp2_ksl_node objects.
 */
struct nghttp2_ksl_blk {
  /* next points to the next block if leaf field is nonzero. */
  nghttp2_ksl_blk *next;
  /* prev points to the previous block if leaf field is nonzero. */
  nghttp2_ksl_blk *prev;
  /* n is the number of nodes this object contains in nodes. */
  size_t n;
  /* leaf is nonzero if this block contains leaf nodes. */
  int leaf;
  union {
    uint64_t align;
    /* nodes is a buffer to contain NGHTTP2_KSL_MAX_NBLK
       nghttp2_ksl_node objects.  Because nghttp2_ksl_node object is
       allocated along with the additional variable length key
       storage, the size of buffer is unknown until nghttp2_ksl_init is
       called. */
    uint8_t nodes[1];
  };
};

/*
 * nghttp2_ksl_compar is a function type which returns nonzero if key
 * |lhs| should be placed before |rhs|.  It returns 0 otherwise.
 */
typedef int (*nghttp2_ksl_compar)(const nghttp2_ksl_key *lhs,
                                  const nghttp2_ksl_key *rhs);

struct nghttp2_ksl;
typedef struct nghttp2_ksl nghttp2_ksl;

struct nghttp2_ksl_it;
typedef struct nghttp2_ksl_it nghttp2_ksl_it;

/*
 * nghttp2_ksl_it is a forward iterator to iterate nodes.
 */
struct nghttp2_ksl_it {
  const nghttp2_ksl *ksl;
  nghttp2_ksl_blk *blk;
  size_t i;
};

/*
 * nghttp2_ksl is a deterministic paged skip list.
 */
struct nghttp2_ksl {
  /* head points to the root block. */
  nghttp2_ksl_blk *head;
  /* front points to the first leaf block. */
  nghttp2_ksl_blk *front;
  /* back points to the last leaf block. */
  nghttp2_ksl_blk *back;
  nghttp2_ksl_compar compar;
  size_t n;
  /* keylen is the size of key */
  size_t keylen;
  /* nodelen is the actual size of nghttp2_ksl_node including key
     storage. */
  size_t nodelen;
  nghttp2_mem *mem;
};

/*
 * nghttp2_ksl_init initializes |ksl|.  |compar| specifies compare
 * function.  |keylen| is the length of key.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGHTTP2_ERR_NOMEM
 *   Out of memory.
 */
int nghttp2_ksl_init(nghttp2_ksl *ksl, nghttp2_ksl_compar compar, size_t keylen,
                     nghttp2_mem *mem);

/*
 * nghttp2_ksl_free frees resources allocated for |ksl|.  If |ksl| is
 * NULL, this function does nothing.  It does not free the memory
 * region pointed by |ksl| itself.
 */
void nghttp2_ksl_free(nghttp2_ksl *ksl);

/*
 * nghttp2_ksl_insert inserts |key| with its associated |data|.  On
 * successful insertion, the iterator points to the inserted node is
 * stored in |*it|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_NOMEM
 *   Out of memory.
 * NGHTTP2_ERR_INVALID_ARGUMENT
 *   |key| already exists.
 */
int nghttp2_ksl_insert(nghttp2_ksl *ksl, nghttp2_ksl_it *it,
                       const nghttp2_ksl_key *key, void *data);

/*
 * nghttp2_ksl_remove removes the |key| from |ksl|.
 *
 * This function assigns the iterator to |*it|, which points to the
 * node which is located at the right next of the removed node if |it|
 * is not NULL.  If |key| is not found, no deletion takes place and
 * the return value of nghttp2_ksl_end(ksl) is assigned to |*it|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP2_ERR_INVALID_ARGUMENT
 *   |key| does not exist.
 */
int nghttp2_ksl_remove(nghttp2_ksl *ksl, nghttp2_ksl_it *it,
                       const nghttp2_ksl_key *key);

/*
 * nghttp2_ksl_lower_bound returns the iterator which points to the
 * first node which has the key which is equal to |key| or the last
 * node which satisfies !compar(&node->key, key).  If there is no such
 * node, it returns the iterator which satisfies nghttp2_ksl_it_end(it)
 * != 0.
 */
nghttp2_ksl_it nghttp2_ksl_lower_bound(nghttp2_ksl *ksl,
                                       const nghttp2_ksl_key *key);

/*
 * nghttp2_ksl_lower_bound_compar works like nghttp2_ksl_lower_bound,
 * but it takes custom function |compar| to do lower bound search.
 */
nghttp2_ksl_it nghttp2_ksl_lower_bound_compar(nghttp2_ksl *ksl,
                                              const nghttp2_ksl_key *key,
                                              nghttp2_ksl_compar compar);

/*
 * nghttp2_ksl_update_key replaces the key of nodes which has |old_key|
 * with |new_key|.  |new_key| must be strictly greater than the
 * previous node and strictly smaller than the next node.
 */
void nghttp2_ksl_update_key(nghttp2_ksl *ksl, const nghttp2_ksl_key *old_key,
                            const nghttp2_ksl_key *new_key);

/*
 * nghttp2_ksl_begin returns the iterator which points to the first
 * node.  If there is no node in |ksl|, it returns the iterator which
 * satisfies nghttp2_ksl_it_end(it) != 0.
 */
nghttp2_ksl_it nghttp2_ksl_begin(const nghttp2_ksl *ksl);

/*
 * nghttp2_ksl_end returns the iterator which points to the node
 * following the last node.  The returned object satisfies
 * nghttp2_ksl_it_end().  If there is no node in |ksl|, it returns the
 * iterator which satisfies nghttp2_ksl_it_begin(it) != 0.
 */
nghttp2_ksl_it nghttp2_ksl_end(const nghttp2_ksl *ksl);

/*
 * nghttp2_ksl_len returns the number of elements stored in |ksl|.
 */
size_t nghttp2_ksl_len(nghttp2_ksl *ksl);

/*
 * nghttp2_ksl_clear removes all elements stored in |ksl|.
 */
void nghttp2_ksl_clear(nghttp2_ksl *ksl);

/*
 * nghttp2_ksl_nth_node returns the |n|th node under |blk|.
 */
#define nghttp2_ksl_nth_node(KSL, BLK, N)                                      \
  ((nghttp2_ksl_node *)(void *)((BLK)->nodes + (KSL)->nodelen * (N)))

/*
 * nghttp2_ksl_print prints its internal state in stderr.  It assumes
 * that the key is of type int64_t.  This function should be used for
 * the debugging purpose only.
 */
void nghttp2_ksl_print(nghttp2_ksl *ksl);

/*
 * nghttp2_ksl_it_init initializes |it|.
 */
void nghttp2_ksl_it_init(nghttp2_ksl_it *it, const nghttp2_ksl *ksl,
                         nghttp2_ksl_blk *blk, size_t i);

/*
 * nghttp2_ksl_it_get returns the data associated to the node which
 * |it| points to.  It is undefined to call this function when
 * nghttp2_ksl_it_end(it) returns nonzero.
 */
void *nghttp2_ksl_it_get(const nghttp2_ksl_it *it);

/*
 * nghttp2_ksl_it_next advances the iterator by one.  It is undefined
 * if this function is called when nghttp2_ksl_it_end(it) returns
 * nonzero.
 */
#define nghttp2_ksl_it_next(IT)                                                \
  (++(IT)->i == (IT)->blk->n && (IT)->blk->next                                \
       ? ((IT)->blk = (IT)->blk->next, (IT)->i = 0)                            \
       : 0)

/*
 * nghttp2_ksl_it_prev moves backward the iterator by one.  It is
 * undefined if this function is called when nghttp2_ksl_it_begin(it)
 * returns nonzero.
 */
void nghttp2_ksl_it_prev(nghttp2_ksl_it *it);

/*
 * nghttp2_ksl_it_end returns nonzero if |it| points to the beyond the
 * last node.
 */
#define nghttp2_ksl_it_end(IT)                                                 \
  ((IT)->blk->n == (IT)->i && (IT)->blk->next == NULL)

/*
 * nghttp2_ksl_it_begin returns nonzero if |it| points to the first
 * node.  |it| might satisfy both nghttp2_ksl_it_begin(&it) and
 * nghttp2_ksl_it_end(&it) if the skip list has no node.
 */
int nghttp2_ksl_it_begin(const nghttp2_ksl_it *it);

/*
 * nghttp2_ksl_key returns the key of the node which |it| points to.
 * It is undefined to call this function when nghttp2_ksl_it_end(it)
 * returns nonzero.
 */
#define nghttp2_ksl_it_key(IT)                                                 \
  ((nghttp2_ksl_key *)nghttp2_ksl_nth_node((IT)->ksl, (IT)->blk, (IT)->i)->key)

#endif /* NGHTTP2_KSL_H */

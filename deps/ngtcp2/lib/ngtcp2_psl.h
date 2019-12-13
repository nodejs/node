/*
 * ngtcp2
 *
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
#ifndef NGTCP2_PSL_H
#define NGTCP2_PSL_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>

#include <ngtcp2/ngtcp2.h>

#include "ngtcp2_range.h"

/*
 * Skip List implementation inspired by
 * https://github.com/jabr/olio/blob/master/skiplist.c
 */

#define NGTCP2_PSL_DEGR 8
/* NGTCP2_PSL_MAX_NBLK is the maximum number of nodes which a single
   block can contain. */
#define NGTCP2_PSL_MAX_NBLK (2 * NGTCP2_PSL_DEGR - 1)
/* NGTCP2_PSL_MIN_NBLK is the minimum number of nodes which a single
   block other than root must contains. */
#define NGTCP2_PSL_MIN_NBLK (NGTCP2_PSL_DEGR - 1)

struct ngtcp2_psl_node;
typedef struct ngtcp2_psl_node ngtcp2_psl_node;

struct ngtcp2_psl_blk;
typedef struct ngtcp2_psl_blk ngtcp2_psl_blk;

/*
 * ngtcp2_psl_node is a node which contains either ngtcp2_psl_blk or
 * opaque data.  If a node is an internal node, it contains
 * ngtcp2_psl_blk.  Otherwise, it has data.  The invariant is that the
 * range of internal node dictates the maximum range in its
 * descendants, and the corresponding leaf node must exist.
 */
struct ngtcp2_psl_node {
  ngtcp2_range range;
  union {
    ngtcp2_psl_blk *blk;
    void *data;
  };
};

/*
 * ngtcp2_psl_blk contains ngtcp2_psl_node objects.
 */
struct ngtcp2_psl_blk {
  /* next points to the next block if leaf field is nonzero. */
  ngtcp2_psl_blk *next;
  /* n is the number of nodes this object contains in nodes. */
  size_t n;
  /* leaf is nonzero if this block contains leaf nodes. */
  int leaf;
  ngtcp2_psl_node nodes[NGTCP2_PSL_MAX_NBLK];
};

struct ngtcp2_psl_it;
typedef struct ngtcp2_psl_it ngtcp2_psl_it;

/*
 * ngtcp2_psl_it is a forward iterator to iterate nodes.
 */
struct ngtcp2_psl_it {
  const ngtcp2_psl_blk *blk;
  size_t i;
};

struct ngtcp2_psl;
typedef struct ngtcp2_psl ngtcp2_psl;

/*
 * ngtcp2_psl is a deterministic paged skip list.
 */
struct ngtcp2_psl {
  /* head points to the root block. */
  ngtcp2_psl_blk *head;
  /* front points to the first leaf block. */
  ngtcp2_psl_blk *front;
  size_t n;
  const ngtcp2_mem *mem;
};

/*
 * ngtcp2_psl_init initializes |psl|.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGTCP2_ERR_NOMEM
 *   Out of memory.
 */
int ngtcp2_psl_init(ngtcp2_psl *psl, const ngtcp2_mem *mem);

/*
 * ngtcp2_psl_free frees resources allocated for |psl|.  If |psl| is
 * NULL, this function does nothing.  It does not free the memory
 * region pointed by |psl| itself.
 */
void ngtcp2_psl_free(ngtcp2_psl *psl);

/*
 * ngtcp2_psl_insert inserts |range| with its associated |data|.  On
 * successful insertion, the iterator points to the inserted node is
 * stored in |*it|.
 *
 * This function assumes that the existing ranges do not intersect
 * with |range|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *   Out of memory.
 */
int ngtcp2_psl_insert(ngtcp2_psl *psl, ngtcp2_psl_it *it,
                      const ngtcp2_range *range, void *data);

/*
 * ngtcp2_psl_remove removes the |range| from |psl|.  It assumes such
 * the range is included in |psl|.
 *
 * This function assigns the iterator to |*it|, which points to the
 * node which is located at the right next of the removed node if |it|
 * is not NULL.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *   Out of memory.
 */
int ngtcp2_psl_remove(ngtcp2_psl *psl, ngtcp2_psl_it *it,
                      const ngtcp2_range *range);

/*
 * ngtcp2_psl_update_range replaces the range of nodes which has
 * |old_range| with |new_range|.  |old_range| must include
 * |new_range|.
 */
void ngtcp2_psl_update_range(ngtcp2_psl *psl, const ngtcp2_range *old_range,
                             const ngtcp2_range *new_range);

/*
 * ngtcp2_psl_lower_bound returns the iterator which points to the
 * first node whose range intersects with |range|.  If there is no
 * such node, it returns the iterator which satisfies
 * ngtcp2_psl_it_end(it) != 0.
 */
ngtcp2_psl_it ngtcp2_psl_lower_bound(ngtcp2_psl *psl,
                                     const ngtcp2_range *range);

/*
 * ngtcp2_psl_begin returns the iterator which points to the first
 * node.  If there is no node in |psl|, it returns the iterator which
 * satisfies ngtcp2_psl_it_end(it) != 0.
 */
ngtcp2_psl_it ngtcp2_psl_begin(const ngtcp2_psl *psl);

/*
 * ngtcp2_psl_len returns the number of elements stored in |ksl|.
 */
size_t ngtcp2_psl_len(ngtcp2_psl *psl);

/*
 * ngtcp2_psl_print prints its internal state in stderr.  This
 * function should be used for the debugging purpose only.
 */
void ngtcp2_psl_print(ngtcp2_psl *psl);

/*
 * ngtcp2_psl_it_init initializes |it|.
 */
void ngtcp2_psl_it_init(ngtcp2_psl_it *it, const ngtcp2_psl_blk *blk, size_t i);

/*
 * ngtcp2_psl_it_get returns the data associated to the node which
 * |it| points to.  If this function is called when
 * ngtcp2_psl_it_end(it) returns nonzero, it returns NULL.
 */
void *ngtcp2_psl_it_get(const ngtcp2_psl_it *it);

/*
 * ngtcp2_psl_it_next advances the iterator by one.  It is undefined
 * if this function is called when ngtcp2_psl_it_end(it) returns
 * nonzero.
 */
void ngtcp2_psl_it_next(ngtcp2_psl_it *it);

/*
 * ngtcp2_psl_it_end returns nonzero if |it| points to the beyond the
 * last node.
 */
int ngtcp2_psl_it_end(const ngtcp2_psl_it *it);

/*
 * ngtcp2_psl_range returns the range of the node which |it| points
 * to.  It is OK to call this function when ngtcp2_psl_it_end(it)
 * returns nonzero.  In this case, this function returns {UINT64_MAX,
 * UINT64_MAX}.
 */
ngtcp2_range ngtcp2_psl_it_range(const ngtcp2_psl_it *it);

#endif /* NGTCP2_PSL_H */

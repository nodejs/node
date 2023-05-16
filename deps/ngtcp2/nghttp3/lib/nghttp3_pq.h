/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
 * Copyright (c) 2017 ngtcp2 contributors
 * Copyright (c) 2012 nghttp2 contributors
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
#ifndef NGHTTP3_PQ_H
#define NGHTTP3_PQ_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp3/nghttp3.h>

#include "nghttp3_mem.h"

/* Implementation of priority queue */

/* NGHTTP3_PQ_BAD_INDEX is the priority queue index which indicates
   that an entry is not queued.  Assigning this value to
   nghttp3_pq_entry.index can check that the entry is queued or not. */
#define NGHTTP3_PQ_BAD_INDEX SIZE_MAX

typedef struct nghttp3_pq_entry {
  size_t index;
} nghttp3_pq_entry;

/* "less" function, return nonzero if |lhs| is less than |rhs|. */
typedef int (*nghttp3_less)(const nghttp3_pq_entry *lhs,
                            const nghttp3_pq_entry *rhs);

typedef struct nghttp3_pq {
  /* The pointer to the pointer to the item stored */
  nghttp3_pq_entry **q;
  /* Memory allocator */
  const nghttp3_mem *mem;
  /* The number of items stored */
  size_t length;
  /* The maximum number of items this pq can store. This is
     automatically extended when length is reached to this value. */
  size_t capacity;
  /* The less function between items */
  nghttp3_less less;
} nghttp3_pq;

/*
 * Initializes priority queue |pq| with compare function |cmp|.
 */
void nghttp3_pq_init(nghttp3_pq *pq, nghttp3_less less, const nghttp3_mem *mem);

/*
 * Deallocates any resources allocated for |pq|.  The stored items are
 * not freed by this function.
 */
void nghttp3_pq_free(nghttp3_pq *pq);

/*
 * Adds |item| to the priority queue |pq|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGHTTP3_ERR_NOMEM
 *     Out of memory.
 */
int nghttp3_pq_push(nghttp3_pq *pq, nghttp3_pq_entry *item);

/*
 * Returns item at the top of the queue |pq|.  It is undefined if the
 * queue is empty.
 */
nghttp3_pq_entry *nghttp3_pq_top(const nghttp3_pq *pq);

/*
 * Pops item at the top of the queue |pq|. The popped item is not
 * freed by this function.
 */
void nghttp3_pq_pop(nghttp3_pq *pq);

/*
 * Returns nonzero if the queue |pq| is empty.
 */
int nghttp3_pq_empty(const nghttp3_pq *pq);

/*
 * Returns the number of items in the queue |pq|.
 */
size_t nghttp3_pq_size(const nghttp3_pq *pq);

typedef int (*nghttp3_pq_item_cb)(nghttp3_pq_entry *item, void *arg);

/*
 * Applies |fun| to each item in |pq|.  The |arg| is passed as arg
 * parameter to callback function.  This function must not change the
 * ordering key.  If the return value from callback is nonzero, this
 * function returns 1 immediately without iterating remaining items.
 * Otherwise this function returns 0.
 */
int nghttp3_pq_each(const nghttp3_pq *pq, nghttp3_pq_item_cb fun, void *arg);

/*
 * Removes |item| from priority queue.
 */
void nghttp3_pq_remove(nghttp3_pq *pq, nghttp3_pq_entry *item);

void nghttp3_pq_clear(nghttp3_pq *pq);

#endif /* NGHTTP3_PQ_H */

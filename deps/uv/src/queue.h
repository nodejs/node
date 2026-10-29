/* Copyright (c) 2013, Ben Noordhuis <info@bnoordhuis.nl>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef QUEUE_H_
#define QUEUE_H_

#include <stddef.h>

#define uv__queue_data(pointer, type, field)                                  \
  ((type*) ((char*) (pointer) - offsetof(type, field)))

#define uv__queue_foreach(q, h)                                               \
  for ((q) = (h)->next; (q) != (h); (q) = (q)->next)

static inline void uv__queue_init(struct uv__queue* q) {
  q->next = q;
  q->prev = q;
}

static inline int uv__queue_empty(const struct uv__queue* q) {
  return q == q->next;
}

static inline struct uv__queue* uv__queue_head(const struct uv__queue* q) {
  return q->next;
}

static inline struct uv__queue* uv__queue_next(const struct uv__queue* q) {
  return q->next;
}

static inline void uv__queue_add(struct uv__queue* h, struct uv__queue* n) {
  h->prev->next = n->next;
  n->next->prev = h->prev;
  h->prev = n->prev;
  h->prev->next = h;
}

static inline void uv__queue_split(struct uv__queue* h,
                                   struct uv__queue* q,
                                   struct uv__queue* n) {
  n->prev = h->prev;
  n->prev->next = n;
  n->next = q;
  h->prev = q->prev;
  h->prev->next = h;
  q->prev = n;
}

static inline void uv__queue_move(struct uv__queue* h, struct uv__queue* n) {
  if (uv__queue_empty(h))
    uv__queue_init(n);
  else
    uv__queue_split(h, h->next, n);
}

static inline void uv__queue_insert_head(struct uv__queue* h,
                                         struct uv__queue* q) {
  q->next = h->next;
  q->prev = h;
  q->next->prev = q;
  h->next = q;
}

static inline void uv__queue_insert_tail(struct uv__queue* h,
                                         struct uv__queue* q) {
  q->next = h;
  q->prev = h->prev;
  q->prev->next = q;
  h->prev = q;
}

static inline void uv__queue_remove(struct uv__queue* q) {
  q->prev->next = q->next;
  q->next->prev = q->prev;
}

#endif /* QUEUE_H_ */

/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
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
#include "nghttp3_tnode.h"

#include <assert.h>

#include "nghttp3_macro.h"
#include "nghttp3_stream.h"
#include "nghttp3_conn.h"

nghttp3_node_id *nghttp3_node_id_init(nghttp3_node_id *nid,
                                      nghttp3_node_id_type type, int64_t id) {
  nid->type = type;
  nid->id = id;
  return nid;
}

int nghttp3_node_id_eq(const nghttp3_node_id *a, const nghttp3_node_id *b) {
  return a->type == b->type && a->id == b->id;
}

static int cycle_less(const nghttp3_pq_entry *lhsx,
                      const nghttp3_pq_entry *rhsx) {
  const nghttp3_tnode *lhs = nghttp3_struct_of(lhsx, nghttp3_tnode, pe);
  const nghttp3_tnode *rhs = nghttp3_struct_of(rhsx, nghttp3_tnode, pe);

  if (lhs->cycle == rhs->cycle) {
    return lhs->seq < rhs->seq;
  }

  return rhs->cycle - lhs->cycle <= NGHTTP3_TNODE_MAX_CYCLE_GAP;
}

void nghttp3_tnode_init(nghttp3_tnode *tnode, const nghttp3_node_id *nid,
                        uint64_t seq, uint32_t weight, nghttp3_tnode *parent,
                        const nghttp3_mem *mem) {
  nghttp3_pq_init(&tnode->pq, cycle_less, mem);

  tnode->pe.index = NGHTTP3_PQ_BAD_INDEX;
  tnode->nid = *nid;
  tnode->seq = seq;
  tnode->cycle = 0;
  tnode->pending_penalty = 0;
  tnode->weight = weight;
  tnode->parent = parent;
  tnode->first_child = NULL;
  tnode->num_children = 0;
  tnode->active = 0;

  if (parent) {
    tnode->next_sibling = parent->first_child;
    parent->first_child = tnode;
    ++parent->num_children;
  } else {
    tnode->next_sibling = NULL;
  }
}

void nghttp3_tnode_free(nghttp3_tnode *tnode) { nghttp3_pq_free(&tnode->pq); }

int nghttp3_tnode_is_active(nghttp3_tnode *tnode) {
  nghttp3_push_promise *pp;

  switch (tnode->nid.type) {
  case NGHTTP3_NODE_ID_TYPE_STREAM:
    return nghttp3_stream_is_active(
        nghttp3_struct_of(tnode, nghttp3_stream, node));
  case NGHTTP3_NODE_ID_TYPE_PUSH:
    pp = nghttp3_struct_of(tnode, nghttp3_push_promise, node);
    return pp->stream && nghttp3_stream_is_active(pp->stream);
  case NGHTTP3_NODE_ID_TYPE_UT:
    /* For unit test */
    return tnode->active;
  default:
    return 0;
  }
}

static void tnode_unschedule(nghttp3_tnode *tnode) {
  nghttp3_tnode *parent = tnode->parent;

  for (parent = tnode->parent; parent; tnode = parent, parent = tnode->parent) {
    assert(tnode->pe.index != NGHTTP3_PQ_BAD_INDEX);

    nghttp3_pq_remove(&parent->pq, &tnode->pe);
    tnode->pe.index = NGHTTP3_PQ_BAD_INDEX;

    if (nghttp3_tnode_is_active(parent) || !nghttp3_pq_empty(&parent->pq)) {
      return;
    }
  }
}

void nghttp3_tnode_unschedule(nghttp3_tnode *tnode) {
  if (tnode->pe.index == NGHTTP3_PQ_BAD_INDEX ||
      !nghttp3_pq_empty(&tnode->pq)) {
    return;
  }

  tnode_unschedule(tnode);
}

void nghttp3_tnode_unschedule_detach(nghttp3_tnode *tnode) {
  if (tnode->pe.index == NGHTTP3_PQ_BAD_INDEX) {
    return;
  }

  tnode_unschedule(tnode);
}

static int tnode_schedule(nghttp3_tnode *tnode, nghttp3_tnode *parent,
                          uint64_t base_cycle, size_t nwrite) {
  uint64_t penalty =
      (uint64_t)nwrite * NGHTTP3_MAX_WEIGHT + tnode->pending_penalty;

  tnode->cycle = base_cycle + penalty / tnode->weight;
  tnode->pending_penalty = (uint32_t)(penalty % tnode->weight);

  return nghttp3_pq_push(&parent->pq, &tnode->pe);
}

static uint64_t tnode_get_first_cycle(nghttp3_tnode *tnode) {
  nghttp3_tnode *top;

  if (nghttp3_pq_empty(&tnode->pq)) {
    return 0;
  }

  top = nghttp3_struct_of(nghttp3_pq_top(&tnode->pq), nghttp3_tnode, pe);
  return top->cycle;
}

int nghttp3_tnode_schedule(nghttp3_tnode *tnode, size_t nwrite) {
  nghttp3_tnode *parent;
  uint64_t cycle;
  int rv;

  for (parent = tnode->parent; parent; tnode = parent, parent = tnode->parent) {
    if (tnode->pe.index == NGHTTP3_PQ_BAD_INDEX) {
      cycle = tnode_get_first_cycle(parent);
    } else if (nwrite != 0) {
      cycle = tnode->cycle;
      nghttp3_pq_remove(&parent->pq, &tnode->pe);
      tnode->pe.index = NGHTTP3_PQ_BAD_INDEX;
    } else {
      return 0;
    }

    rv = tnode_schedule(tnode, parent, cycle, nwrite);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

int nghttp3_tnode_is_scheduled(nghttp3_tnode *tnode) {
  return tnode->pe.index != NGHTTP3_PQ_BAD_INDEX;
}

nghttp3_tnode *nghttp3_tnode_get_next(nghttp3_tnode *tnode) {
  if (nghttp3_pq_empty(&tnode->pq)) {
    return NULL;
  }

  tnode = nghttp3_struct_of(nghttp3_pq_top(&tnode->pq), nghttp3_tnode, pe);

  for (;;) {
    if (nghttp3_tnode_is_active(tnode)) {
      return tnode;
    }
    assert(!nghttp3_pq_empty(&tnode->pq));
    tnode = nghttp3_struct_of(nghttp3_pq_top(&tnode->pq), nghttp3_tnode, pe);
    assert(tnode);
  }
}

void nghttp3_tnode_insert(nghttp3_tnode *tnode, nghttp3_tnode *parent) {
  assert(tnode->parent == NULL);
  assert(tnode->next_sibling == NULL);
  assert(tnode->pe.index == NGHTTP3_PQ_BAD_INDEX);

  tnode->next_sibling = parent->first_child;
  parent->first_child = tnode;
  tnode->parent = parent;
  ++parent->num_children;
}

int nghttp3_tnode_insert_exclusive(nghttp3_tnode *tnode,
                                   nghttp3_tnode *parent) {
  nghttp3_tnode **p, *node;
  int rv;

  for (node = parent->first_child; node; node = node->next_sibling) {
    node->parent = tnode;

    if (node->pe.index == NGHTTP3_PQ_BAD_INDEX) {
      continue;
    }

    nghttp3_pq_remove(&parent->pq, &node->pe);
    node->pe.index = NGHTTP3_PQ_BAD_INDEX;
  }

  for (p = &tnode->first_child; *p; p = &(*p)->next_sibling)
    ;

  *p = parent->first_child;
  parent->first_child = NULL;
  tnode->num_children += parent->num_children;
  parent->num_children = 0;

  nghttp3_tnode_insert(tnode, parent);

  for (node = *p; node; node = node->next_sibling) {
    if (nghttp3_tnode_is_active(node) ||
        nghttp3_tnode_has_active_descendant(node)) {
      rv = nghttp3_tnode_schedule(node, 0);
      if (rv != 0) {
        return rv;
      }
    }
  }

  return 0;
}

void nghttp3_tnode_remove(nghttp3_tnode *tnode) {
  nghttp3_tnode *parent = tnode->parent, **p;

  assert(parent);

  if (tnode->pe.index != NGHTTP3_PQ_BAD_INDEX) {
    nghttp3_tnode_unschedule_detach(tnode);
  }

  for (p = &parent->first_child; *p != tnode; p = &(*p)->next_sibling)
    ;

  *p = tnode->next_sibling;
  --parent->num_children;
  tnode->parent = tnode->next_sibling = NULL;
}

int nghttp3_tnode_squash(nghttp3_tnode *tnode) {
  nghttp3_tnode *parent = tnode->parent, *node, **p, *first, *end;
  int rv;

  assert(parent);

  for (p = &parent->first_child; *p != tnode; p = &(*p)->next_sibling)
    ;

  *p = tnode->first_child;

  for (; *p; p = &(*p)->next_sibling) {
    node = *p;
    node->parent = parent;
    node->weight =
        (uint32_t)(node->weight * tnode->weight / tnode->num_children);
    if (node->weight == 0) {
      node->weight = 1;
    }

    if (node->pe.index == NGHTTP3_PQ_BAD_INDEX) {
      continue;
    }

    nghttp3_pq_remove(&tnode->pq, &node->pe);
    node->pe.index = NGHTTP3_PQ_BAD_INDEX;
  }

  *p = tnode->next_sibling;

  first = tnode->first_child;
  end = tnode->next_sibling;

  if (tnode->pe.index != NGHTTP3_PQ_BAD_INDEX) {
    nghttp3_tnode_unschedule(tnode);
    assert(tnode->pe.index == NGHTTP3_PQ_BAD_INDEX);
  }

  parent->num_children += tnode->num_children;
  tnode->num_children = 0;
  tnode->parent = tnode->next_sibling = tnode->first_child = NULL;

  for (node = first; node && node != end; node = node->next_sibling) {
    if (nghttp3_tnode_is_active(node) ||
        nghttp3_tnode_has_active_descendant(node)) {
      rv = nghttp3_tnode_schedule(node, 0);
      if (rv != 0) {
        return rv;
      }
    }
  }

  return 0;
}

nghttp3_tnode *nghttp3_tnode_find_ascendant(nghttp3_tnode *tnode,
                                            const nghttp3_node_id *nid) {
  for (tnode = tnode->parent; tnode && !nghttp3_node_id_eq(nid, &tnode->nid);
       tnode = tnode->parent)
    ;

  return tnode;
}

int nghttp3_tnode_has_active_descendant(nghttp3_tnode *tnode) {
  return !nghttp3_pq_empty(&tnode->pq);
}

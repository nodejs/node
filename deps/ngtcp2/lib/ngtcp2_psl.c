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
#include "ngtcp2_psl.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "ngtcp2_macro.h"
#include "ngtcp2_mem.h"

int ngtcp2_psl_init(ngtcp2_psl *psl, const ngtcp2_mem *mem) {
  ngtcp2_psl_blk *head;

  psl->mem = mem;
  psl->head = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_psl_blk));
  if (!psl->head) {
    return NGTCP2_ERR_NOMEM;
  }
  psl->front = psl->head;
  psl->n = 0;

  head = psl->head;

  head->next = NULL;
  head->n = 1;
  head->leaf = 1;
  head->nodes[0].range.begin = UINT64_MAX;
  head->nodes[0].range.end = UINT64_MAX;
  head->nodes[0].data = NULL;

  return 0;
}

/*
 * free_blk frees |blk| recursively.
 */
static void free_blk(ngtcp2_psl_blk *blk, const ngtcp2_mem *mem) {
  size_t i;

  if (!blk->leaf) {
    for (i = 0; i < blk->n; ++i) {
      free_blk(blk->nodes[i].blk, mem);
    }
  }

  ngtcp2_mem_free(mem, blk);
}

void ngtcp2_psl_free(ngtcp2_psl *psl) {
  if (!psl) {
    return;
  }

  free_blk(psl->head, psl->mem);
}

/*
 * psl_split_blk splits |blk| into 2 ngtcp2_psl_blk objects.  The new
 * ngtcp2_psl_blk is always the "right" block.
 *
 * It returns the pointer to the ngtcp2_psl_blk created which is the
 * located at the right of |blk|, or NULL which indicates out of
 * memory error.
 */
static ngtcp2_psl_blk *psl_split_blk(ngtcp2_psl *psl, ngtcp2_psl_blk *blk) {
  ngtcp2_psl_blk *rblk;

  rblk = ngtcp2_mem_malloc(psl->mem, sizeof(ngtcp2_psl_blk));
  if (rblk == NULL) {
    return NULL;
  }

  rblk->next = blk->next;
  blk->next = rblk;
  rblk->leaf = blk->leaf;

  rblk->n = blk->n / 2;

  memcpy(rblk->nodes, &blk->nodes[blk->n - rblk->n],
         sizeof(ngtcp2_psl_node) * rblk->n);

  blk->n -= rblk->n;

  assert(blk->n >= NGTCP2_PSL_MIN_NBLK);
  assert(rblk->n >= NGTCP2_PSL_MIN_NBLK);

  return rblk;
}

/*
 * psl_split_node splits a node included in |blk| at the position |i|
 * into 2 adjacent nodes.  The new node is always inserted at the
 * position |i+1|.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGTCP2_ERR_NOMEM
 *   Out of memory.
 */
static int psl_split_node(ngtcp2_psl *psl, ngtcp2_psl_blk *blk, size_t i) {
  ngtcp2_psl_blk *lblk = blk->nodes[i].blk, *rblk;

  rblk = psl_split_blk(psl, lblk);
  if (rblk == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  memmove(&blk->nodes[i + 2], &blk->nodes[i + 1],
          sizeof(ngtcp2_psl_node) * (blk->n - (i + 1)));

  blk->nodes[i + 1].blk = rblk;

  ++blk->n;

  blk->nodes[i].range = lblk->nodes[lblk->n - 1].range;
  blk->nodes[i + 1].range = rblk->nodes[rblk->n - 1].range;

  return 0;
}

/*
 * psl_split_head splits a head (root) block.  It increases the height
 * of skip list by 1.
 *
 * It returns 0 if it succeeds, or one of the following negative error
 * codes:
 *
 * NGTCP2_ERR_NOMEM
 *   Out of memory.
 */
static int psl_split_head(ngtcp2_psl *psl) {
  ngtcp2_psl_blk *rblk = NULL, *lblk, *nhead = NULL;

  rblk = psl_split_blk(psl, psl->head);
  if (rblk == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  lblk = psl->head;

  nhead = ngtcp2_mem_malloc(psl->mem, sizeof(ngtcp2_psl_blk));
  if (nhead == NULL) {
    ngtcp2_mem_free(psl->mem, rblk);
    return NGTCP2_ERR_NOMEM;
  }
  nhead->next = NULL;
  nhead->n = 2;
  nhead->leaf = 0;

  nhead->nodes[0].range = lblk->nodes[lblk->n - 1].range;
  nhead->nodes[0].blk = lblk;
  nhead->nodes[1].range = rblk->nodes[rblk->n - 1].range;
  nhead->nodes[1].blk = rblk;

  psl->head = nhead;

  return 0;
}

/*
 * insert_node inserts a node whose range is |range| with the
 * associated |data| at the index of |i|.  This function assumes that
 * the number of nodes contained by |blk| is strictly less than
 * NGTCP2_PSL_MAX_NBLK.
 */
static void insert_node(ngtcp2_psl_blk *blk, size_t i,
                        const ngtcp2_range *range, void *data) {
  ngtcp2_psl_node *node;

  assert(blk->n < NGTCP2_PSL_MAX_NBLK);

  memmove(&blk->nodes[i + 1], &blk->nodes[i],
          sizeof(ngtcp2_psl_node) * (blk->n - i));

  node = &blk->nodes[i];
  node->range = *range;
  node->data = data;

  ++blk->n;
}

static int range_intersect(const ngtcp2_range *a, const ngtcp2_range *b) {
  return ngtcp2_max(a->begin, b->begin) < ngtcp2_min(a->end, b->end);
}

int ngtcp2_psl_insert(ngtcp2_psl *psl, ngtcp2_psl_it *it,
                      const ngtcp2_range *range, void *data) {
  ngtcp2_psl_blk *blk = psl->head;
  ngtcp2_psl_node *node;
  size_t i;
  int rv;

  if (blk->n == NGTCP2_PSL_MAX_NBLK) {
    rv = psl_split_head(psl);
    if (rv != 0) {
      return rv;
    }
    blk = psl->head;
  }

  for (;;) {
    for (i = 0, node = &blk->nodes[i]; node->range.begin < range->begin;
         ++i, ++node)
      ;

    assert(!range_intersect(&node->range, range));

    if (blk->leaf) {
      insert_node(blk, i, range, data);
      ++psl->n;
      if (it) {
        ngtcp2_psl_it_init(it, blk, i);
      }
      return 0;
    }

    if (node->blk->n == NGTCP2_PSL_MAX_NBLK) {
      rv = psl_split_node(psl, blk, i);
      if (rv != 0) {
        return rv;
      }
      if (node->range.begin < range->begin) {
        node = &blk->nodes[i + 1];
      }
    }

    blk = node->blk;
  }
}

/*
 * remove_node removes the node included in |blk| at the index of |i|.
 */
static void remove_node(ngtcp2_psl_blk *blk, size_t i) {
  memmove(&blk->nodes[i], &blk->nodes[i + 1],
          sizeof(ngtcp2_psl_node) * (blk->n - (i + 1)));

  --blk->n;
}

/*
 * psl_merge_node merges 2 nodes which are the nodes at the index of
 * |i| and |i + 1|.
 *
 * If |blk| is the direct descendant of head (root) block and the head
 * block contains just 2 nodes, the merged block becomes head block,
 * which decreases the height of |psl| by 1.
 *
 * This function returns the pointer to the merged block.
 */
static ngtcp2_psl_blk *psl_merge_node(ngtcp2_psl *psl, ngtcp2_psl_blk *blk,
                                      size_t i) {
  ngtcp2_psl_blk *lblk, *rblk;

  assert(i + 1 < blk->n);

  lblk = blk->nodes[i].blk;
  rblk = blk->nodes[i + 1].blk;

  assert(lblk->n + rblk->n < NGTCP2_PSL_MAX_NBLK);

  memcpy(&lblk->nodes[lblk->n], &rblk->nodes[0],
         sizeof(ngtcp2_psl_node) * rblk->n);

  lblk->n += rblk->n;
  lblk->next = rblk->next;

  ngtcp2_mem_free(psl->mem, rblk);

  if (psl->head == blk && blk->n == 2) {
    ngtcp2_mem_free(psl->mem, psl->head);
    psl->head = lblk;
  } else {
    remove_node(blk, i + 1);
    blk->nodes[i].range = lblk->nodes[lblk->n - 1].range;
  }

  return lblk;
}

/*
 * psl_relocate_node replaces the key at the index |*pi| in
 * *pblk->nodes with something other without violating contract.  It
 * might involve merging 2 nodes or moving a node to left or right.
 *
 * It assigns the index of the block in |*pblk| where the node is
 * moved to |*pi|.  If merging 2 nodes occurs and it becomes new head,
 * the new head is assigned to |*pblk| and it still contains the key.
 * The caller should handle this situation.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int psl_relocate_node(ngtcp2_psl *psl, ngtcp2_psl_blk **pblk,
                             size_t *pi) {
  ngtcp2_psl_blk *blk = *pblk;
  size_t i = *pi;
  ngtcp2_psl_node *node = &blk->nodes[i];
  ngtcp2_psl_node *rnode = &blk->nodes[i + 1];
  size_t j;
  int rv;

  assert(i + 1 < blk->n);

  if (node->blk->n == NGTCP2_PSL_MIN_NBLK &&
      node->blk->n + rnode->blk->n < NGTCP2_PSL_MAX_NBLK) {
    j = node->blk->n - 1;
    blk = psl_merge_node(psl, blk, i);
    if (blk != psl->head) {
      return 0;
    }
    *pblk = blk;
    i = j;
    if (blk->leaf) {
      *pi = i;
      return 0;
    }
    node = &blk->nodes[i];
    rnode = &blk->nodes[i + 1];

    if (node->blk->n == NGTCP2_PSL_MIN_NBLK &&
        node->blk->n + rnode->blk->n < NGTCP2_PSL_MAX_NBLK) {
      j = node->blk->n - 1;
      blk = psl_merge_node(psl, blk, i);
      assert(blk != psl->head);
      *pi = j;
      return 0;
    }
  }

  if (node->blk->n < rnode->blk->n) {
    node->blk->nodes[node->blk->n] = rnode->blk->nodes[0];
    memmove(&rnode->blk->nodes[0], &rnode->blk->nodes[1],
            sizeof(ngtcp2_psl_node) * (rnode->blk->n - 1));
    --rnode->blk->n;
    ++node->blk->n;
    node->range = node->blk->nodes[node->blk->n - 1].range;
    *pi = i;
    return 0;
  }

  if (rnode->blk->n == NGTCP2_PSL_MAX_NBLK) {
    rv = psl_split_node(psl, blk, i + 1);
    if (rv != 0) {
      return rv;
    }
  }

  memmove(&rnode->blk->nodes[1], &rnode->blk->nodes[0],
          sizeof(ngtcp2_psl_node) * rnode->blk->n);

  rnode->blk->nodes[0] = node->blk->nodes[node->blk->n - 1];
  ++rnode->blk->n;

  --node->blk->n;

  node->range = node->blk->nodes[node->blk->n - 1].range;

  *pi = i + 1;
  return 0;
}

/*
 * shift_left moves the first node in blk->nodes[i]->blk->nodes to
 * blk->nodes[i - 1]->blk->nodes.
 */
static void shift_left(ngtcp2_psl_blk *blk, size_t i) {
  ngtcp2_psl_node *lnode, *rnode;

  assert(i > 0);

  lnode = &blk->nodes[i - 1];
  rnode = &blk->nodes[i];

  assert(lnode->blk->n < NGTCP2_PSL_MAX_NBLK);
  assert(rnode->blk->n > NGTCP2_PSL_MIN_NBLK);

  lnode->blk->nodes[lnode->blk->n] = rnode->blk->nodes[0];
  lnode->range = lnode->blk->nodes[lnode->blk->n].range;
  ++lnode->blk->n;

  --rnode->blk->n;
  memmove(&rnode->blk->nodes[0], &rnode->blk->nodes[1],
          sizeof(ngtcp2_psl_node) * rnode->blk->n);
}

/*
 * shift_right moves the last node in blk->nodes[i]->blk->nodes to
 * blk->nodes[i + 1]->blk->nodes.
 */
static void shift_right(ngtcp2_psl_blk *blk, size_t i) {
  ngtcp2_psl_node *lnode, *rnode;

  assert(i < blk->n - 1);

  lnode = &blk->nodes[i];
  rnode = &blk->nodes[i + 1];

  assert(lnode->blk->n > NGTCP2_PSL_MIN_NBLK);
  assert(rnode->blk->n < NGTCP2_PSL_MAX_NBLK);

  memmove(&rnode->blk->nodes[1], &rnode->blk->nodes[0],
          sizeof(ngtcp2_psl_node) * rnode->blk->n);
  ++rnode->blk->n;
  rnode->blk->nodes[0] = lnode->blk->nodes[lnode->blk->n - 1];

  --lnode->blk->n;
  lnode->range = lnode->blk->nodes[lnode->blk->n - 1].range;
}

int ngtcp2_psl_remove(ngtcp2_psl *psl, ngtcp2_psl_it *it,
                      const ngtcp2_range *range) {
  ngtcp2_psl_blk *blk = psl->head, *lblk, *rblk;
  ngtcp2_psl_node *node;
  size_t i, j;
  int rv;

  if (!blk->leaf && blk->n == NGTCP2_PSL_MAX_NBLK) {
    rv = psl_split_head(psl);
    if (rv != 0) {
      return rv;
    }
    blk = psl->head;
  }

  for (;;) {
    for (i = 0, node = &blk->nodes[i]; node->range.begin < range->begin;
         ++i, ++node)
      ;

    if (blk->leaf) {
      assert(i < blk->n);
      remove_node(blk, i);
      --psl->n;
      if (it) {
        if (blk->n == i) {
          ngtcp2_psl_it_init(it, blk->next, 0);
        } else {
          ngtcp2_psl_it_init(it, blk, i);
        }
      }
      return 0;
    }

    if (node->blk->n == NGTCP2_PSL_MAX_NBLK) {
      rv = psl_split_node(psl, blk, i);
      if (rv != 0) {
        return rv;
      }
      if (node->range.begin < range->begin) {
        ++i;
        node = &blk->nodes[i];
      }
    }

    if (ngtcp2_range_eq(&node->range, range)) {
      rv = psl_relocate_node(psl, &blk, &i);
      if (rv != 0) {
        return rv;
      }
      if (!blk->leaf) {
        node = &blk->nodes[i];
        blk = node->blk;
      }
    } else if (node->blk->n == NGTCP2_PSL_MIN_NBLK) {
      j = i == 0 ? 0 : i - 1;

      lblk = blk->nodes[j].blk;
      rblk = blk->nodes[j + 1].blk;

      if (lblk->n + rblk->n < NGTCP2_PSL_MAX_NBLK) {
        blk = psl_merge_node(psl, blk, j);
      } else {
        if (i == j) {
          shift_left(blk, j + 1);
        } else {
          shift_right(blk, j);
        }
        blk = node->blk;
      }
    } else {
      blk = node->blk;
    }
  }
}

ngtcp2_psl_it ngtcp2_psl_lower_bound(ngtcp2_psl *psl,
                                     const ngtcp2_range *range) {
  ngtcp2_psl_blk *blk = psl->head;
  ngtcp2_psl_node *node;
  size_t i;

  for (;;) {
    for (i = 0, node = &blk->nodes[i]; node->range.begin < range->begin &&
                                       !range_intersect(&node->range, range);
         ++i, node = &blk->nodes[i])
      ;

    if (blk->leaf) {
      ngtcp2_psl_it it = {blk, i};
      return it;
    }

    blk = node->blk;
  }
}

void ngtcp2_psl_update_range(ngtcp2_psl *psl, const ngtcp2_range *old_range,
                             const ngtcp2_range *new_range) {
  ngtcp2_psl_blk *blk = psl->head;
  ngtcp2_psl_node *node;
  size_t i;

  assert(old_range->begin <= new_range->begin);
  assert(new_range->end <= old_range->end);

  for (;;) {
    for (i = 0, node = &blk->nodes[i]; node->range.begin < old_range->begin;
         ++i, node = &blk->nodes[i])
      ;

    if (blk->leaf) {
      assert(ngtcp2_range_eq(&node->range, old_range));
      node->range = *new_range;
      return;
    }

    if (ngtcp2_range_eq(&node->range, old_range)) {
      node->range = *new_range;
    } else {
      assert(!range_intersect(&node->range, old_range));
    }

    blk = node->blk;
  }
}

static void psl_print(ngtcp2_psl *psl, const ngtcp2_psl_blk *blk,
                      size_t level) {
  size_t i;

  fprintf(stderr, "LV=%zu n=%zu\n", level, blk->n);

  if (blk->leaf) {
    for (i = 0; i < blk->n; ++i) {
      fprintf(stderr, " [%" PRIu64 ", %" PRIu64 ")", blk->nodes[i].range.begin,
              blk->nodes[i].range.end);
    }
    fprintf(stderr, "\n");
    return;
  }

  for (i = 0; i < blk->n; ++i) {
    psl_print(psl, blk->nodes[i].blk, level + 1);
  }
}

void ngtcp2_psl_print(ngtcp2_psl *psl) { psl_print(psl, psl->head, 0); }

ngtcp2_psl_it ngtcp2_psl_begin(const ngtcp2_psl *psl) {
  ngtcp2_psl_it it = {psl->front, 0};
  return it;
}

size_t ngtcp2_psl_len(ngtcp2_psl *psl) { return psl->n; }

void ngtcp2_psl_it_init(ngtcp2_psl_it *it, const ngtcp2_psl_blk *blk,
                        size_t i) {
  it->blk = blk;
  it->i = i;
}

void *ngtcp2_psl_it_get(const ngtcp2_psl_it *it) {
  return it->blk->nodes[it->i].data;
}

void ngtcp2_psl_it_next(ngtcp2_psl_it *it) {
  assert(!ngtcp2_psl_it_end(it));

  if (++it->i == it->blk->n) {
    it->blk = it->blk->next;
    it->i = 0;
  }
}

int ngtcp2_psl_it_end(const ngtcp2_psl_it *it) {
  ngtcp2_range end = {UINT64_MAX, UINT64_MAX};
  return ngtcp2_range_eq(&end, &it->blk->nodes[it->i].range);
}

ngtcp2_range ngtcp2_psl_it_range(const ngtcp2_psl_it *it) {
  return it->blk->nodes[it->i].range;
}

/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
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
#include "ngtcp2_rob.h"

#include <string.h>
#include <assert.h>

#include "ngtcp2_macro.h"

static int rob_data_new(uint8_t **pd, size_t chunk, const ngtcp2_mem *mem) {
  *pd = ngtcp2_mem_malloc(mem, chunk);
  if (*pd == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  return 0;
}

static void rob_data_del(uint8_t *d, const ngtcp2_mem *mem) {
  ngtcp2_mem_free(mem, d);
}

int ngtcp2_rob_init(ngtcp2_rob *rob, size_t chunk, const ngtcp2_mem *mem) {
  int rv;
  static const ngtcp2_range g = {
    .end = UINT64_MAX,
  };

  ngtcp2_ksl_init(&rob->gapksl, ngtcp2_ksl_range_compar,
                  ngtcp2_ksl_range_search, sizeof(ngtcp2_range), mem);

  rv = ngtcp2_ksl_insert(&rob->gapksl, NULL, &g, NULL);
  if (rv != 0) {
    ngtcp2_ksl_free(&rob->gapksl);
    return rv;
  }

  ngtcp2_ksl_init(&rob->dataksl, ngtcp2_ksl_range_compar,
                  ngtcp2_ksl_range_search, sizeof(ngtcp2_range), mem);

  rob->chunk = chunk;
  rob->mem = mem;
  rob->discard_data = 0;

  return 0;
}

void ngtcp2_rob_free(ngtcp2_rob *rob) {
  ngtcp2_ksl_it it;

  if (rob == NULL) {
    return;
  }

  for (it = ngtcp2_ksl_begin(&rob->dataksl); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    rob_data_del(ngtcp2_ksl_it_get(&it), rob->mem);
  }

  ngtcp2_ksl_free(&rob->dataksl);
  ngtcp2_ksl_free(&rob->gapksl);
}

static int rob_write_data(ngtcp2_rob *rob, uint64_t offset, const uint8_t *data,
                          size_t len) {
  size_t n;
  int rv;
  uint8_t *d;
  ngtcp2_range range = {
    .begin = offset,
    .end = offset + len,
  };
  ngtcp2_ksl_it it;
  const ngtcp2_range *r;
  uint64_t chunk_offset;

  if (rob->discard_data) {
    return 0;
  }

  for (it = ngtcp2_ksl_lower_bound_search(&rob->dataksl, &range,
                                          ngtcp2_ksl_range_exclusive_search);
       len; ngtcp2_ksl_it_next(&it)) {
    if (ngtcp2_ksl_it_end(&it)) {
      d = NULL;
    } else {
      r = ngtcp2_ksl_it_key(&it);
      d = ngtcp2_ksl_it_get(&it);
    }

    if (d == NULL || offset < r->begin) {
      rv = rob_data_new(&d, rob->chunk, rob->mem);
      if (rv != 0) {
        return rv;
      }

      chunk_offset = (offset / rob->chunk) * rob->chunk;

      rv = ngtcp2_ksl_insert(&rob->dataksl, &it,
                             &(ngtcp2_range){
                               .begin = chunk_offset,
                               .end = chunk_offset + rob->chunk,
                             },
                             d);
      if (rv != 0) {
        rob_data_del(d, rob->mem);
        return rv;
      }

      r = ngtcp2_ksl_it_key(&it);
    }

    n = (size_t)ngtcp2_min((uint64_t)len, r->end - offset);
    memcpy(d + (offset - r->begin), data, n);
    offset += n;
    data += n;
    len -= n;
  }

  return 0;
}

ngtcp2_ssize ngtcp2_rob_push(ngtcp2_rob *rob, uint64_t offset,
                             const uint8_t *data, size_t datalen) {
  int rv;
  ngtcp2_range g;
  ngtcp2_range m, l, r;
  ngtcp2_range q = {
    .begin = offset,
    .end = offset + datalen,
  };
  ngtcp2_ksl_it it;
  ngtcp2_ssize nwrite = 0;
  size_t mlen;

  it = ngtcp2_ksl_lower_bound_search(&rob->gapksl, &q,
                                     ngtcp2_ksl_range_exclusive_search);

  for (; !ngtcp2_ksl_it_end(&it);) {
    g = *(const ngtcp2_range *)ngtcp2_ksl_it_key(&it);
    m = ngtcp2_range_intersect(&q, &g);

    mlen = (size_t)ngtcp2_range_len(&m);
    if (mlen == 0) {
      break;
    }

    if (ngtcp2_range_eq(&g, &m)) {
      ngtcp2_ksl_remove_hint(&rob->gapksl, &it, &it, &g);

      rv = rob_write_data(rob, m.begin, data + (m.begin - offset), mlen);
      if (rv != 0) {
        return rv;
      }

      nwrite += (ngtcp2_ssize)mlen;

      continue;
    }

    ngtcp2_range_cut(&l, &r, &g, &m);

    if (ngtcp2_range_len(&l)) {
      ngtcp2_ksl_update_key(&rob->gapksl, &g, &l);

      if (ngtcp2_range_len(&r)) {
        rv = ngtcp2_ksl_insert(&rob->gapksl, &it, &r, NULL);
        if (rv != 0) {
          return rv;
        }
      }
    } else if (ngtcp2_range_len(&r)) {
      ngtcp2_ksl_update_key(&rob->gapksl, &g, &r);
    }

    rv = rob_write_data(rob, m.begin, data + (m.begin - offset), mlen);
    if (rv != 0) {
      return rv;
    }

    nwrite += (ngtcp2_ssize)mlen;

    ngtcp2_ksl_it_next(&it);
  }

  return nwrite;
}

void ngtcp2_rob_remove_prefix(ngtcp2_rob *rob, uint64_t offset) {
  ngtcp2_range g;
  ngtcp2_range r;
  uint8_t *d;
  ngtcp2_ksl_it it;

  it = ngtcp2_ksl_begin(&rob->gapksl);

  for (; !ngtcp2_ksl_it_end(&it);) {
    g = *(const ngtcp2_range *)ngtcp2_ksl_it_key(&it);
    if (offset <= g.begin) {
      break;
    }

    if (offset < g.end) {
      ngtcp2_ksl_update_key(&rob->gapksl, &g,
                            &(ngtcp2_range){
                              .begin = offset,
                              .end = g.end,
                            });

      break;
    }

    ngtcp2_ksl_remove_hint(&rob->gapksl, &it, &it, &g);
  }

  if (rob->discard_data) {
    return;
  }

  it = ngtcp2_ksl_begin(&rob->dataksl);

  for (; !ngtcp2_ksl_it_end(&it);) {
    r = *(const ngtcp2_range *)ngtcp2_ksl_it_key(&it);
    if (offset < r.end) {
      return;
    }

    d = ngtcp2_ksl_it_get(&it);

    ngtcp2_ksl_remove_hint(&rob->dataksl, &it, &it, &r);
    rob_data_del(d, rob->mem);
  }
}

uint64_t ngtcp2_rob_data_at(const ngtcp2_rob *rob, const uint8_t **pdest,
                            uint64_t offset) {
  const ngtcp2_range *g;
  const ngtcp2_range *r;
  uint8_t *d;
  ngtcp2_ksl_it it;

  it = ngtcp2_ksl_begin(&rob->gapksl);
  if (ngtcp2_ksl_it_end(&it)) {
    return 0;
  }

  g = ngtcp2_ksl_it_key(&it);

  if (g->begin <= offset) {
    return 0;
  }

  if (rob->discard_data) {
    *pdest = NULL;

    return g->begin - offset;
  }

  it = ngtcp2_ksl_begin(&rob->dataksl);
  r = ngtcp2_ksl_it_key(&it);
  d = ngtcp2_ksl_it_get(&it);

  assert(d);
  assert(r->begin <= offset);
  assert(offset < r->end);

  *pdest = d + (offset - r->begin);

  return ngtcp2_min(g->begin, r->end) - offset;
}

void ngtcp2_rob_pop(ngtcp2_rob *rob, uint64_t offset, uint64_t len) {
  ngtcp2_ksl_it it;
  ngtcp2_range r;
  uint8_t *d;

  if (rob->discard_data) {
    return;
  }

  it = ngtcp2_ksl_begin(&rob->dataksl);
  r = *(const ngtcp2_range *)ngtcp2_ksl_it_key(&it);
  d = ngtcp2_ksl_it_get(&it);

  assert(d);

  if (offset + len < r.end) {
    return;
  }

  ngtcp2_ksl_remove_hint(&rob->dataksl, NULL, &it, &r);
  rob_data_del(d, rob->mem);
}

uint64_t ngtcp2_rob_first_gap_offset(const ngtcp2_rob *rob) {
  ngtcp2_ksl_it it = ngtcp2_ksl_begin(&rob->gapksl);
  const ngtcp2_range *g;

  if (ngtcp2_ksl_it_end(&it)) {
    return UINT64_MAX;
  }

  g = ngtcp2_ksl_it_key(&it);

  return g->begin;
}

int ngtcp2_rob_data_buffered(const ngtcp2_rob *rob) {
  return ngtcp2_ksl_len(&rob->dataksl) != 0;
}

void ngtcp2_rob_discard_data(ngtcp2_rob *rob) {
  ngtcp2_ksl_it it;

  rob->discard_data = 1;

  for (it = ngtcp2_ksl_begin(&rob->dataksl); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    rob_data_del(ngtcp2_ksl_it_get(&it), rob->mem);
  }

  ngtcp2_ksl_clear(&rob->dataksl);
}

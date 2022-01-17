/*
 * nghttp3
 *
 * Copyright (c) 2019 nghttp3 contributors
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
#include "nghttp3_gaptr.h"
#include "nghttp3_range.h"

#include <string.h>
#include <assert.h>

int nghttp3_gaptr_init(nghttp3_gaptr *gaptr, const nghttp3_mem *mem) {
  int rv;
  nghttp3_range range = {0, UINT64_MAX};

  rv = nghttp3_ksl_init(&gaptr->gap, nghttp3_ksl_range_compar,
                        sizeof(nghttp3_range), mem);
  if (rv != 0) {
    return rv;
  }

  rv = nghttp3_ksl_insert(&gaptr->gap, NULL, &range, NULL);
  if (rv != 0) {
    nghttp3_ksl_free(&gaptr->gap);
    return rv;
  }

  gaptr->mem = mem;

  return 0;
}

void nghttp3_gaptr_free(nghttp3_gaptr *gaptr) {
  if (gaptr == NULL) {
    return;
  }

  nghttp3_ksl_free(&gaptr->gap);
}

int nghttp3_gaptr_push(nghttp3_gaptr *gaptr, uint64_t offset, size_t datalen) {
  int rv;
  nghttp3_range k, m, l, r, q = {offset, offset + datalen};
  nghttp3_ksl_it it;

  it = nghttp3_ksl_lower_bound_compar(&gaptr->gap, &q,
                                      nghttp3_ksl_range_exclusive_compar);

  for (; !nghttp3_ksl_it_end(&it);) {
    k = *(nghttp3_range *)nghttp3_ksl_it_key(&it);
    m = nghttp3_range_intersect(&q, &k);
    if (!nghttp3_range_len(&m)) {
      break;
    }

    if (nghttp3_range_eq(&k, &m)) {
      nghttp3_ksl_remove(&gaptr->gap, &it, &k);
      continue;
    }
    nghttp3_range_cut(&l, &r, &k, &m);
    if (nghttp3_range_len(&l)) {
      nghttp3_ksl_update_key(&gaptr->gap, &k, &l);

      if (nghttp3_range_len(&r)) {
        rv = nghttp3_ksl_insert(&gaptr->gap, &it, &r, NULL);
        if (rv != 0) {
          return rv;
        }
      }
    } else if (nghttp3_range_len(&r)) {
      nghttp3_ksl_update_key(&gaptr->gap, &k, &r);
    }
    nghttp3_ksl_it_next(&it);
  }
  return 0;
}

uint64_t nghttp3_gaptr_first_gap_offset(nghttp3_gaptr *gaptr) {
  nghttp3_ksl_it it = nghttp3_ksl_begin(&gaptr->gap);
  nghttp3_range r = *(nghttp3_range *)nghttp3_ksl_it_key(&it);
  return r.begin;
}

nghttp3_ksl_it nghttp3_gaptr_get_first_gap_after(nghttp3_gaptr *gaptr,
                                                 uint64_t offset) {
  nghttp3_range q = {offset, offset + 1};
  return nghttp3_ksl_lower_bound_compar(&gaptr->gap, &q,
                                        nghttp3_ksl_range_exclusive_compar);
}

int nghttp3_gaptr_is_pushed(nghttp3_gaptr *gaptr, uint64_t offset,
                            size_t datalen) {
  nghttp3_range q = {offset, offset + datalen};
  nghttp3_ksl_it it = nghttp3_ksl_lower_bound_compar(
      &gaptr->gap, &q, nghttp3_ksl_range_exclusive_compar);
  nghttp3_range k = *(nghttp3_range *)nghttp3_ksl_it_key(&it);
  nghttp3_range m = nghttp3_range_intersect(&q, &k);
  return nghttp3_range_len(&m) == 0;
}

void nghttp3_gaptr_drop_first_gap(nghttp3_gaptr *gaptr) {
  nghttp3_ksl_it it = nghttp3_ksl_begin(&gaptr->gap);
  nghttp3_range r;

  assert(!nghttp3_ksl_it_end(&it));

  r = *(nghttp3_range *)nghttp3_ksl_it_key(&it);

  nghttp3_ksl_remove(&gaptr->gap, NULL, &r);
}

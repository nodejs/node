/*
 * ngtcp2
 *
 * Copyright (c) 2025 ngtcp2 contributors
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
#include "ngtcp2_dcidtr.h"

#include <assert.h>

#include "ngtcp2_tstamp.h"
#include "ngtcp2_macro.h"

void ngtcp2_dcidtr_init(ngtcp2_dcidtr *dtr) {
  ngtcp2_static_ringbuf_dcid_unused_init(&dtr->unused);
  ngtcp2_static_ringbuf_dcid_bound_init(&dtr->bound);
  ngtcp2_static_ringbuf_dcid_retired_init(&dtr->retired);

  dtr->retire_unacked.len = 0;
}

int ngtcp2_dcidtr_track_retired_seq(ngtcp2_dcidtr *dtr, uint64_t seq) {
  if (dtr->retire_unacked.len >= ngtcp2_arraylen(dtr->retire_unacked.seqs)) {
    return NGTCP2_ERR_CONNECTION_ID_LIMIT;
  }

  dtr->retire_unacked.seqs[dtr->retire_unacked.len++] = seq;

  return 0;
}

void ngtcp2_dcidtr_untrack_retired_seq(ngtcp2_dcidtr *dtr, uint64_t seq) {
  size_t i;

  for (i = 0; i < dtr->retire_unacked.len; ++i) {
    if (dtr->retire_unacked.seqs[i] != seq) {
      continue;
    }

    if (i != dtr->retire_unacked.len - 1) {
      dtr->retire_unacked.seqs[i] =
        dtr->retire_unacked.seqs[dtr->retire_unacked.len - 1];
    }

    --dtr->retire_unacked.len;

    return;
  }
}

int ngtcp2_dcidtr_check_retired_seq_tracked(const ngtcp2_dcidtr *dtr,
                                            uint64_t seq) {
  size_t i;

  for (i = 0; i < dtr->retire_unacked.len; ++i) {
    if (dtr->retire_unacked.seqs[i] == seq) {
      return 1;
    }
  }

  return 0;
}

static int dcidtr_on_retire(ngtcp2_dcidtr *dtr, const ngtcp2_dcid *dcid,
                            ngtcp2_dcidtr_cb on_retire, void *user_data) {
  int rv;

  if (ngtcp2_dcidtr_check_retired_seq_tracked(dtr, dcid->seq)) {
    return 0;
  }

  rv = ngtcp2_dcidtr_track_retired_seq(dtr, dcid->seq);
  if (rv != 0) {
    return rv;
  }

  if (!on_retire) {
    return 0;
  }

  return on_retire(dcid, user_data);
}

ngtcp2_dcid *ngtcp2_dcidtr_find_bound_dcid(const ngtcp2_dcidtr *dtr,
                                           const ngtcp2_path *path) {
  ngtcp2_dcid *dcid;
  const ngtcp2_ringbuf *rb = &dtr->bound.rb;
  size_t i, len = ngtcp2_ringbuf_len(rb);

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(rb, i);

    if (ngtcp2_path_eq(&dcid->ps.path, path)) {
      return dcid;
    }
  }

  return NULL;
}

ngtcp2_dcid *ngtcp2_dcidtr_bind_zerolen_dcid(ngtcp2_dcidtr *dtr,
                                             const ngtcp2_path *path) {
  ngtcp2_dcid *dcid = ngtcp2_ringbuf_push_back(&dtr->bound.rb);
  ngtcp2_cid cid;

  ngtcp2_cid_zero(&cid);
  ngtcp2_dcid_init(dcid, 0, &cid, NULL);
  ngtcp2_dcid_set_path(dcid, path);

  return dcid;
}

int ngtcp2_dcidtr_bind_dcid(ngtcp2_dcidtr *dtr, ngtcp2_dcid **pdest,
                            const ngtcp2_path *path, ngtcp2_tstamp ts,
                            ngtcp2_dcidtr_cb on_retire, void *user_data) {
  const ngtcp2_dcid *src;
  ngtcp2_dcid *dest;
  int rv;

  if (ngtcp2_ringbuf_full(&dtr->bound.rb)) {
    rv = dcidtr_on_retire(dtr, ngtcp2_ringbuf_get(&dtr->bound.rb, 0), on_retire,
                          user_data);
    if (rv != 0) {
      return rv;
    }
  }

  src = ngtcp2_ringbuf_get(&dtr->unused.rb, 0);
  dest = ngtcp2_ringbuf_push_back(&dtr->bound.rb);

  ngtcp2_dcid_copy(dest, src);
  dest->bound_ts = ts;
  ngtcp2_dcid_set_path(dest, path);

  ngtcp2_ringbuf_pop_front(&dtr->unused.rb);

  *pdest = dest;

  return 0;
}

int ngtcp2_dcidtr_verify_stateless_reset(const ngtcp2_dcidtr *dtr,
                                         const ngtcp2_path *path,
                                         const uint8_t *token) {
  const ngtcp2_dcid *dcid;
  const ngtcp2_ringbuf *rb = &dtr->bound.rb;
  size_t i, len = ngtcp2_ringbuf_len(rb);

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(rb, i);
    if (ngtcp2_dcid_verify_stateless_reset_token(dcid, path, token) == 0) {
      return 0;
    }
  }

  return NGTCP2_ERR_INVALID_ARGUMENT;
}

static int verify_token_uniqueness(const ngtcp2_ringbuf *rb, int *pfound,
                                   uint64_t seq, const ngtcp2_cid *cid,
                                   const uint8_t *token) {
  const ngtcp2_dcid *dcid;
  size_t i, len = ngtcp2_ringbuf_len(rb);
  int rv;

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(rb, i);
    rv = ngtcp2_dcid_verify_uniqueness(dcid, seq, cid, token);
    if (rv != 0) {
      return NGTCP2_ERR_PROTO;
    }

    if (ngtcp2_cid_eq(&dcid->cid, cid)) {
      *pfound = 1;
    }
  }

  return 0;
}

int ngtcp2_dcidtr_verify_token_uniqueness(const ngtcp2_dcidtr *dtr, int *pfound,
                                          uint64_t seq, const ngtcp2_cid *cid,
                                          const uint8_t *token) {
  int rv;

  rv = verify_token_uniqueness(&dtr->bound.rb, pfound, seq, cid, token);
  if (rv != 0) {
    return rv;
  }

  return verify_token_uniqueness(&dtr->unused.rb, pfound, seq, cid, token);
}

static void remove_dcid_at(ngtcp2_ringbuf *rb, size_t at) {
  const ngtcp2_dcid *src;
  ngtcp2_dcid *dest;

  if (at == 0) {
    ngtcp2_ringbuf_pop_front(rb);
    return;
  }

  if (at == ngtcp2_ringbuf_len(rb) - 1) {
    ngtcp2_ringbuf_pop_back(rb);
    return;
  }

  src = ngtcp2_ringbuf_get(rb, ngtcp2_ringbuf_len(rb) - 1);
  dest = ngtcp2_ringbuf_get(rb, at);

  ngtcp2_dcid_copy(dest, src);
  ngtcp2_ringbuf_pop_back(rb);
}

static int dcidtr_retire_dcid_prior_to(ngtcp2_dcidtr *dtr, ngtcp2_ringbuf *rb,
                                       uint64_t seq, ngtcp2_dcidtr_cb on_retire,
                                       void *user_data) {
  size_t i;
  const ngtcp2_dcid *dcid;
  int rv;

  for (i = 0; i < ngtcp2_ringbuf_len(rb);) {
    dcid = ngtcp2_ringbuf_get(rb, i);
    if (dcid->seq >= seq) {
      ++i;
      continue;
    }

    rv = dcidtr_on_retire(dtr, dcid, on_retire, user_data);
    if (rv != 0) {
      return rv;
    }

    remove_dcid_at(rb, i);
  }

  return 0;
}

int ngtcp2_dcidtr_retire_inactive_dcid_prior_to(ngtcp2_dcidtr *dtr,
                                                uint64_t seq,
                                                ngtcp2_dcidtr_cb on_retire,
                                                void *user_data) {
  int rv;

  rv =
    dcidtr_retire_dcid_prior_to(dtr, &dtr->bound.rb, seq, on_retire, user_data);
  if (rv != 0) {
    return rv;
  }

  return dcidtr_retire_dcid_prior_to(dtr, &dtr->unused.rb, seq, on_retire,
                                     user_data);
}

int ngtcp2_dcidtr_retire_active_dcid(ngtcp2_dcidtr *dtr,
                                     const ngtcp2_dcid *dcid, ngtcp2_tstamp ts,
                                     ngtcp2_dcidtr_cb on_deactivate,
                                     void *user_data) {
  ngtcp2_ringbuf *rb = &dtr->retired.rb;
  const ngtcp2_dcid *stale_dcid;
  ngtcp2_dcid *dest;
  int rv;

  assert(dcid->cid.datalen);

  if (ngtcp2_ringbuf_full(rb)) {
    stale_dcid = ngtcp2_ringbuf_get(rb, 0);
    rv = on_deactivate(stale_dcid, user_data);
    if (rv != 0) {
      return rv;
    }
  }

  dest = ngtcp2_ringbuf_push_back(rb);
  ngtcp2_dcid_copy(dest, dcid);
  dest->retired_ts = ts;

  return dcidtr_on_retire(dtr, dest, NULL, NULL);
}

int ngtcp2_dcidtr_remove_stale_retired_dcid(ngtcp2_dcidtr *dtr,
                                            ngtcp2_duration timeout,
                                            ngtcp2_tstamp ts,
                                            ngtcp2_dcidtr_cb on_deactivate,
                                            void *user_data) {
  ngtcp2_ringbuf *rb = &dtr->retired.rb;
  const ngtcp2_dcid *dcid;
  int rv;

  for (; ngtcp2_ringbuf_len(rb);) {
    dcid = ngtcp2_ringbuf_get(rb, 0);
    if (ngtcp2_tstamp_not_elapsed(dcid->retired_ts, timeout, ts)) {
      break;
    }

    rv = on_deactivate(dcid, user_data);
    if (rv != 0) {
      return rv;
    }

    ngtcp2_ringbuf_pop_front(rb);
  }

  return 0;
}

int ngtcp2_dcidtr_pop_bound_dcid(ngtcp2_dcidtr *dtr, ngtcp2_dcid *dest,
                                 const ngtcp2_path *path) {
  const ngtcp2_dcid *src;
  ngtcp2_ringbuf *rb = &dtr->bound.rb;
  size_t len = ngtcp2_ringbuf_len(rb);
  size_t i;

  for (i = 0; i < len; ++i) {
    src = ngtcp2_ringbuf_get(rb, i);
    if (ngtcp2_path_eq(&src->ps.path, path)) {
      ngtcp2_dcid_copy(dest, src);
      remove_dcid_at(rb, i);

      return 0;
    }
  }

  return NGTCP2_ERR_INVALID_ARGUMENT;
}

int ngtcp2_dcidtr_retire_stale_bound_dcid(ngtcp2_dcidtr *dtr,
                                          ngtcp2_duration timeout,
                                          ngtcp2_tstamp ts,
                                          ngtcp2_dcidtr_cb on_retire,
                                          void *user_data) {
  ngtcp2_ringbuf *rb = &dtr->bound.rb;
  size_t i;
  const ngtcp2_dcid *dcid;
  int rv;

  for (i = 0; i < ngtcp2_ringbuf_len(rb);) {
    dcid = ngtcp2_ringbuf_get(rb, i);

    assert(dcid->cid.datalen);

    if (ngtcp2_tstamp_not_elapsed(dcid->bound_ts, timeout, ts)) {
      ++i;
      continue;
    }

    rv = dcidtr_on_retire(dtr, dcid, on_retire, user_data);
    if (rv != 0) {
      return rv;
    }

    remove_dcid_at(rb, i);
  }

  return 0;
}

ngtcp2_tstamp ngtcp2_dcidtr_earliest_bound_ts(const ngtcp2_dcidtr *dtr) {
  const ngtcp2_ringbuf *rb = &dtr->bound.rb;
  size_t i, len = ngtcp2_ringbuf_len(rb);
  ngtcp2_tstamp res = UINT64_MAX;
  const ngtcp2_dcid *dcid;

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(rb, i);

    assert(dcid->cid.datalen);
    assert(dcid->bound_ts != UINT64_MAX);

    res = ngtcp2_min_uint64(res, dcid->bound_ts);
  }

  return res;
}

ngtcp2_tstamp ngtcp2_dcidtr_earliest_retired_ts(const ngtcp2_dcidtr *dtr) {
  const ngtcp2_ringbuf *rb = &dtr->retired.rb;
  const ngtcp2_dcid *dcid;

  if (ngtcp2_ringbuf_len(rb) == 0) {
    return UINT64_MAX;
  }

  dcid = ngtcp2_ringbuf_get(rb, 0);

  return dcid->retired_ts;
}

void ngtcp2_dcidtr_push_unused(ngtcp2_dcidtr *dtr, uint64_t seq,
                               const ngtcp2_cid *cid, const uint8_t *token) {
  ngtcp2_dcid *dcid = ngtcp2_ringbuf_push_back(&dtr->unused.rb);

  ngtcp2_dcid_init(dcid, seq, cid, token);
}

void ngtcp2_dcidtr_pop_unused_cid_token(ngtcp2_dcidtr *dtr, ngtcp2_dcid *dest) {
  ngtcp2_ringbuf *rb = &dtr->unused.rb;
  const ngtcp2_dcid *src;

  assert(ngtcp2_ringbuf_len(rb));

  src = ngtcp2_ringbuf_get(rb, 0);

  dest->flags = NGTCP2_DCID_FLAG_NONE;
  ngtcp2_dcid_copy_cid_token(dest, src);

  ngtcp2_ringbuf_pop_front(rb);
}

void ngtcp2_dcidtr_pop_unused(ngtcp2_dcidtr *dtr, ngtcp2_dcid *dest) {
  ngtcp2_ringbuf *rb = &dtr->unused.rb;
  const ngtcp2_dcid *src;

  assert(ngtcp2_ringbuf_len(rb));

  src = ngtcp2_ringbuf_get(rb, 0);

  ngtcp2_dcid_copy(dest, src);

  ngtcp2_ringbuf_pop_front(rb);
}

int ngtcp2_dcidtr_check_path_retired(const ngtcp2_dcidtr *dtr,
                                     const ngtcp2_path *path) {
  const ngtcp2_ringbuf *rb = &dtr->retired.rb;
  size_t i, len = ngtcp2_ringbuf_len(rb);
  const ngtcp2_dcid *dcid;

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(rb, i);
    if (ngtcp2_path_eq(&dcid->ps.path, path)) {
      return 1;
    }
  }

  return 0;
}

size_t ngtcp2_dcidtr_unused_len(const ngtcp2_dcidtr *dtr) {
  return ngtcp2_ringbuf_len(&dtr->unused.rb);
}

size_t ngtcp2_dcidtr_bound_len(const ngtcp2_dcidtr *dtr) {
  return ngtcp2_ringbuf_len(&dtr->bound.rb);
}

size_t ngtcp2_dcidtr_retired_len(const ngtcp2_dcidtr *dtr) {
  return ngtcp2_ringbuf_len(&dtr->retired.rb);
}

size_t ngtcp2_dcidtr_inactive_len(const ngtcp2_dcidtr *dtr) {
  return ngtcp2_ringbuf_len(&dtr->unused.rb) +
         ngtcp2_ringbuf_len(&dtr->bound.rb);
}

int ngtcp2_dcidtr_unused_full(const ngtcp2_dcidtr *dtr) {
  return ngtcp2_ringbuf_full(&dtr->unused.rb);
}

int ngtcp2_dcidtr_unused_empty(const ngtcp2_dcidtr *dtr) {
  return ngtcp2_ringbuf_len(&dtr->unused.rb) == 0;
}

int ngtcp2_dcidtr_bound_full(const ngtcp2_dcidtr *dtr) {
  return ngtcp2_ringbuf_full(&dtr->bound.rb);
}

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
#include "ngtcp2_strm.h"

#include <string.h>
#include <assert.h>

#include "ngtcp2_rtb.h"
#include "ngtcp2_pkt.h"
#include "ngtcp2_vec.h"

static int offset_less(const ngtcp2_ksl_key *lhs, const ngtcp2_ksl_key *rhs) {
  return *(int64_t *)lhs < *(int64_t *)rhs;
}

int ngtcp2_strm_init(ngtcp2_strm *strm, int64_t stream_id, uint32_t flags,
                     uint64_t max_rx_offset, uint64_t max_tx_offset,
                     void *stream_user_data, const ngtcp2_mem *mem) {
  int rv;

  strm->cycle = 0;
  strm->tx.offset = 0;
  strm->tx.max_offset = max_tx_offset;
  strm->rx.last_offset = 0;
  strm->stream_id = stream_id;
  strm->flags = flags;
  strm->stream_user_data = stream_user_data;
  strm->rx.max_offset = strm->rx.unsent_max_offset = max_rx_offset;
  strm->me.key = (uint64_t)stream_id;
  strm->me.next = NULL;
  strm->pe.index = NGTCP2_PQ_BAD_INDEX;
  strm->mem = mem;
  strm->app_error_code = 0;

  rv = ngtcp2_gaptr_init(&strm->tx.acked_offset, mem);
  if (rv != 0) {
    goto fail_gaptr_init;
  }

  rv = ngtcp2_rob_init(&strm->rx.rob, 8 * 1024, mem);
  if (rv != 0) {
    goto fail_rob_init;
  }

  rv = ngtcp2_ksl_init(&strm->tx.streamfrq, offset_less, sizeof(uint64_t), mem);
  if (rv != 0) {
    goto fail_tx_streamfrq_init;
  }

  return 0;

fail_tx_streamfrq_init:
  ngtcp2_rob_free(&strm->rx.rob);
fail_rob_init:
  ngtcp2_gaptr_free(&strm->tx.acked_offset);
fail_gaptr_init:
  return rv;
}

void ngtcp2_strm_free(ngtcp2_strm *strm) {
  ngtcp2_ksl_it it;

  if (strm == NULL) {
    return;
  }

  for (it = ngtcp2_ksl_begin(&strm->tx.streamfrq); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    ngtcp2_frame_chain_del(ngtcp2_ksl_it_get(&it), strm->mem);
  }

  ngtcp2_ksl_free(&strm->tx.streamfrq);
  ngtcp2_rob_free(&strm->rx.rob);
  ngtcp2_gaptr_free(&strm->tx.acked_offset);
}

uint64_t ngtcp2_strm_rx_offset(ngtcp2_strm *strm) {
  return ngtcp2_rob_first_gap_offset(&strm->rx.rob);
}

int ngtcp2_strm_recv_reordering(ngtcp2_strm *strm, const uint8_t *data,
                                size_t datalen, uint64_t offset) {
  return ngtcp2_rob_push(&strm->rx.rob, offset, data, datalen);
}

void ngtcp2_strm_shutdown(ngtcp2_strm *strm, uint32_t flags) {
  strm->flags |= flags & NGTCP2_STRM_FLAG_SHUT_RDWR;
}

int ngtcp2_strm_streamfrq_push(ngtcp2_strm *strm, ngtcp2_frame_chain *frc) {
  assert(frc->fr.type == NGTCP2_FRAME_STREAM);
  assert(frc->next == NULL);

  return ngtcp2_ksl_insert(&strm->tx.streamfrq, NULL, &frc->fr.stream.offset,
                           frc);
}

int ngtcp2_strm_streamfrq_pop(ngtcp2_strm *strm, ngtcp2_frame_chain **pfrc,
                              size_t left) {
  ngtcp2_stream *fr, *nfr;
  ngtcp2_frame_chain *frc, *nfrc;
  int rv;
  size_t nmerged;
  size_t datalen;
  ngtcp2_vec a[NGTCP2_MAX_STREAM_DATACNT];
  ngtcp2_vec b[NGTCP2_MAX_STREAM_DATACNT];
  size_t acnt, bcnt;
  ngtcp2_ksl_it it;
  uint64_t old_offset;

  if (ngtcp2_ksl_len(&strm->tx.streamfrq) == 0) {
    *pfrc = NULL;
    return 0;
  }

  it = ngtcp2_ksl_begin(&strm->tx.streamfrq);
  frc = ngtcp2_ksl_it_get(&it);
  fr = &frc->fr.stream;

  datalen = ngtcp2_vec_len(fr->data, fr->datacnt);

  if (left == 0) {
    /* datalen could be zero if 0 length STREAM has been sent */
    if (datalen || ngtcp2_ksl_len(&strm->tx.streamfrq) > 1) {
      *pfrc = NULL;
      return 0;
    }
  }

  ngtcp2_ksl_remove(&strm->tx.streamfrq, NULL, &fr->offset);

  if (datalen > left) {
    ngtcp2_vec_copy(a, fr->data, fr->datacnt);
    acnt = fr->datacnt;

    bcnt = 0;
    ngtcp2_vec_split(a, &acnt, b, &bcnt, left, NGTCP2_MAX_STREAM_DATACNT);

    assert(acnt > 0);
    assert(bcnt > 0);

    rv = ngtcp2_frame_chain_stream_datacnt_new(&nfrc, bcnt, strm->mem);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(frc, strm->mem);
      return rv;
    }

    nfr = &nfrc->fr.stream;
    nfr->type = NGTCP2_FRAME_STREAM;
    nfr->flags = 0;
    nfr->fin = fr->fin;
    nfr->stream_id = fr->stream_id;
    nfr->offset = fr->offset + left;
    nfr->datacnt = bcnt;
    ngtcp2_vec_copy(nfr->data, b, bcnt);

    rv = ngtcp2_ksl_insert(&strm->tx.streamfrq, NULL, &nfr->offset, nfrc);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(nfrc, strm->mem);
      ngtcp2_frame_chain_del(frc, strm->mem);
      return rv;
    }

    rv = ngtcp2_frame_chain_stream_datacnt_new(&nfrc, acnt, strm->mem);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(frc, strm->mem);
      return rv;
    }

    nfr = &nfrc->fr.stream;
    *nfr = *fr;
    nfr->fin = 0;
    nfr->datacnt = acnt;
    ngtcp2_vec_copy(nfr->data, a, acnt);

    ngtcp2_frame_chain_del(frc, strm->mem);

    *pfrc = nfrc;

    return 0;
  }

  left -= datalen;

  ngtcp2_vec_copy(a, fr->data, fr->datacnt);
  acnt = fr->datacnt;

  for (; left && ngtcp2_ksl_len(&strm->tx.streamfrq);) {
    it = ngtcp2_ksl_begin(&strm->tx.streamfrq);
    nfrc = ngtcp2_ksl_it_get(&it);
    nfr = &nfrc->fr.stream;

    if (nfr->offset != fr->offset + datalen) {
      assert(fr->offset + datalen < nfr->offset);
      break;
    }

    if (nfr->fin && nfr->datacnt == 0) {
      fr->fin = 1;
      ngtcp2_ksl_remove(&strm->tx.streamfrq, NULL, &nfr->offset);
      ngtcp2_frame_chain_del(nfrc, strm->mem);
      break;
    }

    nmerged = ngtcp2_vec_merge(a, &acnt, nfr->data, &nfr->datacnt, left,
                               NGTCP2_MAX_STREAM_DATACNT);
    if (nmerged == 0) {
      break;
    }

    datalen += nmerged;
    left -= nmerged;

    if (nfr->datacnt == 0) {
      fr->fin = nfr->fin;
      ngtcp2_ksl_remove(&strm->tx.streamfrq, NULL, &nfr->offset);
      ngtcp2_frame_chain_del(nfrc, strm->mem);
      continue;
    }

    old_offset = nfr->offset;
    nfr->offset += nmerged;

    ngtcp2_ksl_update_key(&strm->tx.streamfrq, &old_offset, &nfr->offset);

    break;
  }

  if (acnt == fr->datacnt) {
    if (acnt > 0) {
      fr->data[acnt - 1] = a[acnt - 1];
    }

    *pfrc = frc;
    return 0;
  }

  assert(acnt > fr->datacnt);

  rv = ngtcp2_frame_chain_stream_datacnt_new(&nfrc, acnt, strm->mem);
  if (rv != 0) {
    ngtcp2_frame_chain_del(frc, strm->mem);
    return rv;
  }

  nfr = &nfrc->fr.stream;
  *nfr = *fr;
  nfr->datacnt = acnt;
  ngtcp2_vec_copy(nfr->data, a, acnt);

  ngtcp2_frame_chain_del(frc, strm->mem);

  *pfrc = nfrc;

  return 0;
}

ngtcp2_frame_chain *ngtcp2_strm_streamfrq_top(ngtcp2_strm *strm) {
  ngtcp2_ksl_it it;

  assert(ngtcp2_ksl_len(&strm->tx.streamfrq));

  it = ngtcp2_ksl_begin(&strm->tx.streamfrq);
  return ngtcp2_ksl_it_get(&it);
}

int ngtcp2_strm_streamfrq_empty(ngtcp2_strm *strm) {
  return ngtcp2_ksl_len(&strm->tx.streamfrq) == 0;
}

void ngtcp2_strm_streamfrq_clear(ngtcp2_strm *strm) {
  ngtcp2_frame_chain *frc;
  ngtcp2_ksl_it it;

  for (it = ngtcp2_ksl_begin(&strm->tx.streamfrq); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    frc = ngtcp2_ksl_it_get(&it);
    ngtcp2_frame_chain_del(frc, strm->mem);
  }
  ngtcp2_ksl_clear(&strm->tx.streamfrq);
}

int ngtcp2_strm_is_tx_queued(ngtcp2_strm *strm) {
  return strm->pe.index != NGTCP2_PQ_BAD_INDEX;
}

int ngtcp2_strm_is_all_tx_data_acked(ngtcp2_strm *strm) {
  return ngtcp2_gaptr_first_gap_offset(&strm->tx.acked_offset) ==
         strm->tx.offset;
}

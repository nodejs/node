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
#include "ngtcp2_acktr.h"

#include <assert.h>
#include <string.h>

#include "ngtcp2_macro.h"
#include "ngtcp2_tstamp.h"

static int pkt_range_greater(const ngtcp2_ksl_key *lhs,
                             const ngtcp2_ksl_key *rhs) {
  const ngtcp2_pkt_range *a = lhs;
  const ngtcp2_pkt_range *b = rhs;

  return a->pkt_num > b->pkt_num;
}

ngtcp2_ksl_search_def(pkt_range_greater, pkt_range_greater)

void ngtcp2_acktr_init(ngtcp2_acktr *acktr, ngtcp2_log *log,
                       const ngtcp2_mem *mem) {
  ngtcp2_static_ringbuf_acks_init(&acktr->acks);

  ngtcp2_ksl_init(&acktr->ents, pkt_range_greater, ksl_pkt_range_greater_search,
                  sizeof(ngtcp2_pkt_range), mem);

  acktr->log = log;
  acktr->flags = NGTCP2_ACKTR_FLAG_NONE;
  acktr->first_unacked_ts = UINT64_MAX;
  acktr->rx_npkt = 0;
  acktr->max_pkt_num = -1;
  acktr->max_pkt_ts = UINT64_MAX;
  memset(&acktr->ecn, 0, sizeof(acktr->ecn));
}

void ngtcp2_acktr_free(ngtcp2_acktr *acktr) {
  if (acktr == NULL) {
    return;
  }

  ngtcp2_ksl_free(&acktr->ents);
}

int ngtcp2_acktr_add(ngtcp2_acktr *acktr, int64_t pkt_num, int active_ack,
                     ngtcp2_tstamp ts) {
  ngtcp2_ksl_it it, prev_it;
  ngtcp2_pkt_range *ent, *prev_ent;
  ngtcp2_pkt_range key = {
    .pkt_num = pkt_num,
    .len = 1,
  };
  ngtcp2_pkt_range old_key;
  int rv;
  int added = 0;

  if (ngtcp2_ksl_len(&acktr->ents)) {
    it = ngtcp2_ksl_lower_bound(&acktr->ents, &key);
    if (ngtcp2_ksl_it_end(&it)) {
      ngtcp2_ksl_it_prev(&it);
      ent = (ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&it);

      assert(ent->pkt_num >= pkt_num + (int64_t)ent->len);

      if (ent->pkt_num == pkt_num + (int64_t)ent->len) {
        ++ent->len;
        added = 1;
      }
    } else {
      ent = (ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&it);

      assert(ent->pkt_num != pkt_num);

      if (ngtcp2_ksl_it_begin(&it)) {
        if (ent->pkt_num + 1 == pkt_num) {
          old_key = *ent;
          key.len = ent->len + 1;

          ngtcp2_ksl_update_key(&acktr->ents, &old_key, &key);
          added = 1;
        }
      } else {
        prev_it = it;
        ngtcp2_ksl_it_prev(&prev_it);
        prev_ent = (ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&prev_it);

        assert(prev_ent->pkt_num >= pkt_num + (int64_t)prev_ent->len);

        if (ent->pkt_num + 1 == pkt_num) {
          if (prev_ent->pkt_num == pkt_num + (int64_t)prev_ent->len) {
            prev_ent->len += ent->len + 1;
            old_key = *ent;

            ngtcp2_ksl_remove_hint(&acktr->ents, NULL, &it, &old_key);
            added = 1;
          } else {
            old_key = *ent;
            key.len = ent->len + 1;

            ngtcp2_ksl_update_key(&acktr->ents, &old_key, &key);
            added = 1;
          }
        } else if (prev_ent->pkt_num == pkt_num + (int64_t)prev_ent->len) {
          ++prev_ent->len;
          added = 1;
        }
      }
    }
  }

  if (!added) {
    rv = ngtcp2_ksl_insert(&acktr->ents, NULL, &key, NULL);
    if (rv != 0) {
      return rv;
    }
  }

  if (active_ack) {
    acktr->flags |= NGTCP2_ACKTR_FLAG_ACTIVE_ACK;
    if (acktr->first_unacked_ts == UINT64_MAX) {
      acktr->first_unacked_ts = ts;
    }
  }

  if (ngtcp2_ksl_len(&acktr->ents) > NGTCP2_ACKTR_MAX_ENT) {
    it = ngtcp2_ksl_end(&acktr->ents);
    ngtcp2_ksl_it_prev(&it);
    old_key = *(const ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&it);
    ngtcp2_ksl_remove_hint(&acktr->ents, NULL, &it, &old_key);
  }

  if (acktr->max_pkt_num < pkt_num) {
    acktr->max_pkt_num = pkt_num;
    acktr->max_pkt_ts = ts;
  }

  return 0;
}

void ngtcp2_acktr_forget(ngtcp2_acktr *acktr, int64_t pkt_num) {
  ngtcp2_ksl_it it;
  ngtcp2_pkt_range key = {
    .pkt_num = pkt_num,
  };

  it = ngtcp2_ksl_lower_bound(&acktr->ents, &key);
  assert(pkt_num ==
         ((const ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&it))->pkt_num);

  for (; !ngtcp2_ksl_it_end(&it);) {
    key = *(const ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&it);
    ngtcp2_ksl_remove_hint(&acktr->ents, &it, &it, &key);
  }
}

ngtcp2_ksl_it ngtcp2_acktr_get(const ngtcp2_acktr *acktr) {
  return ngtcp2_ksl_begin(&acktr->ents);
}

int ngtcp2_acktr_empty(const ngtcp2_acktr *acktr) {
  ngtcp2_ksl_it it = ngtcp2_ksl_begin(&acktr->ents);
  return ngtcp2_ksl_it_end(&it);
}

ngtcp2_acktr_ack_entry *ngtcp2_acktr_add_ack(ngtcp2_acktr *acktr,
                                             int64_t pkt_num,
                                             int64_t largest_ack) {
  ngtcp2_acktr_ack_entry *ent = ngtcp2_ringbuf_push_front(&acktr->acks.rb);

  *ent = (ngtcp2_acktr_ack_entry){
    .largest_ack = largest_ack,
    .pkt_num = pkt_num,
  };

  return ent;
}

static void acktr_on_ack(ngtcp2_acktr *acktr, ngtcp2_ringbuf *rb,
                         size_t ack_ent_offset) {
  ngtcp2_acktr_ack_entry *ack_ent;
  ngtcp2_pkt_range *ent;
  ngtcp2_ksl_it it;
  ngtcp2_pkt_range key;

  assert(ngtcp2_ringbuf_len(rb));

  ack_ent = ngtcp2_ringbuf_get(rb, ack_ent_offset);

  key = (ngtcp2_pkt_range){
    .pkt_num = ack_ent->largest_ack,
  };

  /* Assume that ngtcp2_pkt_validate_ack(fr) returns 0 */
  it = ngtcp2_ksl_lower_bound(&acktr->ents, &key);
  for (; !ngtcp2_ksl_it_end(&it);) {
    key = *(const ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&it);
    ngtcp2_ksl_remove_hint(&acktr->ents, &it, &it, &key);
  }

  if (ngtcp2_ksl_len(&acktr->ents)) {
    assert(ngtcp2_ksl_it_end(&it));

    ngtcp2_ksl_it_prev(&it);
    ent = (ngtcp2_pkt_range *)ngtcp2_ksl_it_key(&it);

    assert(ent->pkt_num > ack_ent->largest_ack);

    if (ack_ent->largest_ack + (int64_t)ent->len > ent->pkt_num) {
      ent->len = (size_t)(ent->pkt_num - ack_ent->largest_ack);
    }
  }

  ngtcp2_ringbuf_resize(rb, ack_ent_offset);
}

void ngtcp2_acktr_recv_ack(ngtcp2_acktr *acktr, const ngtcp2_ack *fr) {
  ngtcp2_acktr_ack_entry *ent;
  int64_t largest_ack = fr->largest_ack, min_ack;
  size_t i, j;
  ngtcp2_ringbuf *rb = &acktr->acks.rb;
  size_t nacks = ngtcp2_ringbuf_len(rb);

  /* Assume that ngtcp2_pkt_validate_ack(fr) returns 0 */
  for (j = 0; j < nacks; ++j) {
    ent = ngtcp2_ringbuf_get(rb, j);
    if (largest_ack >= ent->pkt_num) {
      break;
    }
  }
  if (j == nacks) {
    return;
  }

  min_ack = largest_ack - (int64_t)fr->first_ack_range;

  if (min_ack <= ent->pkt_num) {
    acktr_on_ack(acktr, rb, j);
    return;
  }

  for (i = 0; i < fr->rangecnt && j < nacks; ++i) {
    largest_ack = min_ack - (int64_t)fr->ranges[i].gap - 2;
    min_ack = largest_ack - (int64_t)fr->ranges[i].len;

    for (;;) {
      if (ent->pkt_num > largest_ack) {
        if (++j == nacks) {
          return;
        }
        ent = ngtcp2_ringbuf_get(rb, j);
        continue;
      }
      if (ent->pkt_num < min_ack) {
        break;
      }
      acktr_on_ack(acktr, rb, j);
      return;
    }
  }
}

void ngtcp2_acktr_commit_ack(ngtcp2_acktr *acktr) {
  acktr->flags &=
    (uint16_t)~(NGTCP2_ACKTR_FLAG_ACTIVE_ACK | NGTCP2_ACKTR_FLAG_IMMEDIATE_ACK |
                NGTCP2_ACKTR_FLAG_CANCEL_TIMER);
  acktr->first_unacked_ts = UINT64_MAX;
  acktr->rx_npkt = 0;
}

int ngtcp2_acktr_require_active_ack(const ngtcp2_acktr *acktr,
                                    ngtcp2_duration max_ack_delay,
                                    ngtcp2_tstamp ts) {
  return ngtcp2_tstamp_elapsed(acktr->first_unacked_ts, max_ack_delay, ts);
}

void ngtcp2_acktr_immediate_ack(ngtcp2_acktr *acktr) {
  acktr->flags |= NGTCP2_ACKTR_FLAG_IMMEDIATE_ACK;
}

int ngtcp2_acktr_create_ack_frame(ngtcp2_acktr *acktr, ngtcp2_ack *ack,
                                  uint8_t type, ngtcp2_tstamp ts,
                                  ngtcp2_duration ack_delay,
                                  uint64_t ack_delay_exponent) {
  int64_t last_pkt_num;
  ngtcp2_ack_range *range;
  ngtcp2_ksl_it it;
  const ngtcp2_pkt_range *rpkt;
  size_t num_acks;

  if (acktr->flags & NGTCP2_ACKTR_FLAG_IMMEDIATE_ACK) {
    ack_delay = 0;
  }

  if (!ngtcp2_acktr_require_active_ack(acktr, ack_delay, ts)) {
    return -1;
  }

  it = ngtcp2_acktr_get(acktr);
  if (ngtcp2_ksl_it_end(&it)) {
    ngtcp2_acktr_commit_ack(acktr);
    return -1;
  }

  num_acks = ngtcp2_ksl_len(&acktr->ents);

  if (acktr->ecn.ect0 || acktr->ecn.ect1 || acktr->ecn.ce) {
    ack->type = NGTCP2_FRAME_ACK_ECN;
    ack->ecn.ect0 = acktr->ecn.ect0;
    ack->ecn.ect1 = acktr->ecn.ect1;
    ack->ecn.ce = acktr->ecn.ce;
  } else {
    ack->type = NGTCP2_FRAME_ACK;
  }
  ack->rangecnt = 0;

  rpkt = ngtcp2_ksl_it_key(&it);

  if (rpkt->pkt_num == acktr->max_pkt_num) {
    last_pkt_num = rpkt->pkt_num - (int64_t)(rpkt->len - 1);
    ack->largest_ack = rpkt->pkt_num;
    ack->first_ack_range = rpkt->len - 1;

    ngtcp2_ksl_it_next(&it);
    --num_acks;
  } else if (rpkt->pkt_num + 1 == acktr->max_pkt_num) {
    last_pkt_num = rpkt->pkt_num - (int64_t)(rpkt->len - 1);
    ack->largest_ack = acktr->max_pkt_num;
    ack->first_ack_range = rpkt->len;

    ngtcp2_ksl_it_next(&it);
    --num_acks;
  } else {
    assert(rpkt->pkt_num < acktr->max_pkt_num);

    last_pkt_num = acktr->max_pkt_num;
    ack->largest_ack = acktr->max_pkt_num;
    ack->first_ack_range = 0;
  }

  if (type == NGTCP2_PKT_1RTT) {
    ack->ack_delay_unscaled = ts - acktr->max_pkt_ts;
    ack->ack_delay = ack->ack_delay_unscaled / NGTCP2_MICROSECONDS /
                     (1ULL << ack_delay_exponent);
  } else {
    ack->ack_delay_unscaled = 0;
    ack->ack_delay = 0;
  }

  num_acks = ngtcp2_min(num_acks, NGTCP2_MAX_ACK_RANGES);

  for (; ack->rangecnt < num_acks; ngtcp2_ksl_it_next(&it)) {
    rpkt = ngtcp2_ksl_it_key(&it);

    range = &ack->ranges[ack->rangecnt++];
    range->gap = (uint64_t)(last_pkt_num - rpkt->pkt_num - 2);
    range->len = rpkt->len - 1;

    last_pkt_num = rpkt->pkt_num - (int64_t)(rpkt->len - 1);
  }

  return 0;
}

void ngtcp2_acktr_increase_ecn_counts(ngtcp2_acktr *acktr,
                                      const ngtcp2_pkt_info *pi) {
  switch (pi->ecn & NGTCP2_ECN_MASK) {
  case NGTCP2_ECN_ECT_0:
    ++acktr->ecn.ect0;
    break;
  case NGTCP2_ECN_ECT_1:
    ++acktr->ecn.ect1;
    break;
  case NGTCP2_ECN_CE:
    ++acktr->ecn.ce;
    break;
  }
}

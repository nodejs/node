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
#include "ngtcp2_rtb.h"

#include <assert.h>
#include <string.h>

#include "ngtcp2_macro.h"
#include "ngtcp2_conn.h"
#include "ngtcp2_log.h"
#include "ngtcp2_vec.h"
#include "ngtcp2_cc.h"
#include "ngtcp2_rcvry.h"

int ngtcp2_frame_chain_new(ngtcp2_frame_chain **pfrc, const ngtcp2_mem *mem) {
  *pfrc = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_frame_chain));
  if (*pfrc == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  ngtcp2_frame_chain_init(*pfrc);

  return 0;
}

int ngtcp2_frame_chain_extralen_new(ngtcp2_frame_chain **pfrc, size_t extralen,
                                    const ngtcp2_mem *mem) {
  *pfrc = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_frame_chain) + extralen);
  if (*pfrc == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  ngtcp2_frame_chain_init(*pfrc);

  return 0;
}

int ngtcp2_frame_chain_stream_datacnt_new(ngtcp2_frame_chain **pfrc,
                                          size_t datacnt,
                                          const ngtcp2_mem *mem) {
  size_t need = sizeof(ngtcp2_vec) * (datacnt - 1);
  size_t avail = sizeof(ngtcp2_frame) - sizeof(ngtcp2_stream);

  if (datacnt > 0 && need > avail) {
    return ngtcp2_frame_chain_extralen_new(pfrc, need - avail, mem);
  }

  return ngtcp2_frame_chain_new(pfrc, mem);
}

int ngtcp2_frame_chain_crypto_datacnt_new(ngtcp2_frame_chain **pfrc,
                                          size_t datacnt,
                                          const ngtcp2_mem *mem) {
  size_t need = sizeof(ngtcp2_vec) * (datacnt - 1);
  size_t avail = sizeof(ngtcp2_frame) - sizeof(ngtcp2_crypto);

  if (datacnt > 0 && need > avail) {
    return ngtcp2_frame_chain_extralen_new(pfrc, need - avail, mem);
  }

  return ngtcp2_frame_chain_new(pfrc, mem);
}

void ngtcp2_frame_chain_del(ngtcp2_frame_chain *frc, const ngtcp2_mem *mem) {
  ngtcp2_mem_free(mem, frc);
}

void ngtcp2_frame_chain_init(ngtcp2_frame_chain *frc) { frc->next = NULL; }

void ngtcp2_frame_chain_list_del(ngtcp2_frame_chain *frc,
                                 const ngtcp2_mem *mem) {
  ngtcp2_frame_chain *next;

  for (; frc;) {
    next = frc->next;
    ngtcp2_mem_free(mem, frc);
    frc = next;
  }
}

static void frame_chain_insert(ngtcp2_frame_chain **pfrc,
                               ngtcp2_frame_chain *frc) {
  ngtcp2_frame_chain **plast;

  assert(frc);

  for (plast = &frc; *plast; plast = &(*plast)->next)
    ;

  *plast = *pfrc;
  *pfrc = frc;
}

int ngtcp2_rtb_entry_new(ngtcp2_rtb_entry **pent, const ngtcp2_pkt_hd *hd,
                         ngtcp2_frame_chain *frc, ngtcp2_tstamp ts,
                         size_t pktlen, uint8_t flags, const ngtcp2_mem *mem) {
  (*pent) = ngtcp2_mem_calloc(mem, 1, sizeof(ngtcp2_rtb_entry));
  if (*pent == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  (*pent)->hd.pkt_num = hd->pkt_num;
  (*pent)->hd.type = hd->type;
  (*pent)->hd.flags = hd->flags;
  (*pent)->frc = frc;
  (*pent)->ts = ts;
  (*pent)->pktlen = pktlen;
  (*pent)->flags = flags;

  return 0;
}

void ngtcp2_rtb_entry_del(ngtcp2_rtb_entry *ent, const ngtcp2_mem *mem) {
  if (ent == NULL) {
    return;
  }

  ngtcp2_frame_chain_list_del(ent->frc, mem);

  ngtcp2_mem_free(mem, ent);
}

static int greater(const ngtcp2_ksl_key *lhs, const ngtcp2_ksl_key *rhs) {
  return *lhs->i > *rhs->i;
}

void ngtcp2_rtb_init(ngtcp2_rtb *rtb, ngtcp2_crypto_level crypto_level,
                     ngtcp2_strm *crypto, ngtcp2_default_cc *cc,
                     ngtcp2_log *log, ngtcp2_qlog *qlog,
                     const ngtcp2_mem *mem) {
  ngtcp2_ksl_init(&rtb->ents, greater, sizeof(int64_t), mem);
  rtb->crypto = crypto;
  rtb->cc = cc;
  rtb->log = log;
  rtb->qlog = qlog;
  rtb->mem = mem;
  rtb->largest_acked_tx_pkt_num = -1;
  rtb->num_ack_eliciting = 0;
  rtb->loss_time = 0;
  rtb->crypto_level = crypto_level;
}

void ngtcp2_rtb_free(ngtcp2_rtb *rtb) {
  ngtcp2_ksl_it it;

  if (rtb == NULL) {
    return;
  }

  it = ngtcp2_ksl_begin(&rtb->ents);

  for (; !ngtcp2_ksl_it_end(&it); ngtcp2_ksl_it_next(&it)) {
    ngtcp2_rtb_entry_del(ngtcp2_ksl_it_get(&it), rtb->mem);
  }

  ngtcp2_ksl_free(&rtb->ents);
}

static void rtb_on_add(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent) {
  rtb->cc->ccs->bytes_in_flight += ent->pktlen;

  if (ent->flags & NGTCP2_RTB_FLAG_ACK_ELICITING) {
    ++rtb->num_ack_eliciting;
  }
}

static void rtb_on_remove(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent) {
  if (ent->flags & NGTCP2_RTB_FLAG_ACK_ELICITING) {
    assert(rtb->num_ack_eliciting);
    --rtb->num_ack_eliciting;
  }

  assert(rtb->cc->ccs->bytes_in_flight >= ent->pktlen);
  rtb->cc->ccs->bytes_in_flight -= ent->pktlen;
}

static void rtb_on_pkt_lost(ngtcp2_rtb *rtb, ngtcp2_frame_chain **pfrc,
                            ngtcp2_rtb_entry *ent) {
  ngtcp2_log_pkt_lost(rtb->log, ent->hd.pkt_num, ent->hd.type, ent->hd.flags,
                      ent->ts);

  if (rtb->qlog) {
    ngtcp2_qlog_pkt_lost(rtb->qlog, ent);
  }

  if (!(ent->flags & NGTCP2_RTB_FLAG_PROBE)) {
    if (ent->flags & NGTCP2_RTB_FLAG_CRYPTO_TIMEOUT_RETRANSMITTED) {
      ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                      "pkn=%" PRId64 " CRYPTO has already been retransmitted",
                      ent->hd.pkt_num);
    } else if (ent->frc) {
      /* PADDING only (or PADDING + ACK ) packets will have NULL
         ent->frc. */
      /* TODO Reconsider the order of pfrc */
      frame_chain_insert(pfrc, ent->frc);
      ent->frc = NULL;
    }
  }

  ngtcp2_rtb_entry_del(ent, rtb->mem);
}

int ngtcp2_rtb_add(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent) {
  int rv;
  ngtcp2_ksl_key key;

  ent->next = NULL;

  rv = ngtcp2_ksl_insert(&rtb->ents, NULL,
                         ngtcp2_ksl_key_ptr(&key, &ent->hd.pkt_num), ent);
  if (rv != 0) {
    return rv;
  }

  rtb_on_add(rtb, ent);

  return 0;
}

ngtcp2_ksl_it ngtcp2_rtb_head(ngtcp2_rtb *rtb) {
  return ngtcp2_ksl_begin(&rtb->ents);
}

static void rtb_remove(ngtcp2_rtb *rtb, ngtcp2_ksl_it *it,
                       ngtcp2_rtb_entry *ent) {
  ngtcp2_ksl_key key;

  ngtcp2_ksl_remove(&rtb->ents, it, ngtcp2_ksl_key_ptr(&key, &ent->hd.pkt_num));
  rtb_on_remove(rtb, ent);
  ngtcp2_rtb_entry_del(ent, rtb->mem);
}

static int rtb_call_acked_stream_offset(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                                        ngtcp2_conn *conn) {
  ngtcp2_frame_chain *frc;
  uint64_t prev_stream_offset, stream_offset;
  ngtcp2_strm *strm;
  int rv;
  size_t datalen;
  ngtcp2_strm *crypto = rtb->crypto;

  for (frc = ent->frc; frc; frc = frc->next) {
    switch (frc->fr.type) {
    case NGTCP2_FRAME_STREAM:
      strm = ngtcp2_conn_find_stream(conn, frc->fr.stream.stream_id);
      if (strm == NULL) {
        break;
      }
      prev_stream_offset =
          ngtcp2_gaptr_first_gap_offset(&strm->tx.acked_offset);
      rv = ngtcp2_gaptr_push(
          &strm->tx.acked_offset, frc->fr.stream.offset,
          ngtcp2_vec_len(frc->fr.stream.data, frc->fr.stream.datacnt));
      if (rv != 0) {
        return rv;
      }

      if (conn->callbacks.acked_stream_data_offset) {
        stream_offset = ngtcp2_gaptr_first_gap_offset(&strm->tx.acked_offset);
        datalen = stream_offset - prev_stream_offset;
        if (datalen == 0 && !frc->fr.stream.fin) {
          break;
        }

        rv = conn->callbacks.acked_stream_data_offset(
            conn, strm->stream_id, prev_stream_offset, datalen, conn->user_data,
            strm->stream_user_data);
        if (rv != 0) {
          return NGTCP2_ERR_CALLBACK_FAILURE;
        }
      }

      rv = ngtcp2_conn_close_stream_if_shut_rdwr(conn, strm, NGTCP2_NO_ERROR);
      if (rv != 0) {
        return rv;
      }
      break;
    case NGTCP2_FRAME_CRYPTO:
      prev_stream_offset =
          ngtcp2_gaptr_first_gap_offset(&crypto->tx.acked_offset);
      rv = ngtcp2_gaptr_push(
          &crypto->tx.acked_offset, frc->fr.crypto.offset,
          ngtcp2_vec_len(frc->fr.crypto.data, frc->fr.crypto.datacnt));
      if (rv != 0) {
        return rv;
      }

      if (conn->callbacks.acked_crypto_offset) {
        stream_offset = ngtcp2_gaptr_first_gap_offset(&crypto->tx.acked_offset);
        datalen = stream_offset - prev_stream_offset;
        if (datalen == 0) {
          break;
        }

        rv = conn->callbacks.acked_crypto_offset(conn, rtb->crypto_level,
                                                 prev_stream_offset, datalen,
                                                 conn->user_data);
        if (rv != 0) {
          return NGTCP2_ERR_CALLBACK_FAILURE;
        }
      }
      break;
    case NGTCP2_FRAME_RESET_STREAM:
      strm = ngtcp2_conn_find_stream(conn, frc->fr.reset_stream.stream_id);
      if (strm == NULL) {
        break;
      }
      strm->flags |= NGTCP2_STRM_FLAG_RST_ACKED;
      rv = ngtcp2_conn_close_stream_if_shut_rdwr(conn, strm, NGTCP2_NO_ERROR);
      if (rv != 0) {
        return rv;
      }
      break;
    }
  }
  return 0;
}

static void rtb_on_pkt_acked(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                             const ngtcp2_rcvry_stat *rcs, ngtcp2_tstamp ts) {
  ngtcp2_cc_pkt pkt;

  ngtcp2_default_cc_on_pkt_acked(
      rtb->cc, ngtcp2_cc_pkt_init(&pkt, ent->hd.pkt_num, ent->pktlen, ent->ts),
      rcs, ts);
}

ngtcp2_ssize ngtcp2_rtb_recv_ack(ngtcp2_rtb *rtb, const ngtcp2_ack *fr,
                                 ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  ngtcp2_rtb_entry *ent;
  int64_t largest_ack = fr->largest_ack, min_ack;
  size_t i;
  int rv;
  ngtcp2_ksl_it it;
  ngtcp2_ksl_key key;
  ngtcp2_ssize num_acked = 0;
  int largest_pkt_acked = 0;
  int rtt_updated = 0;
  ngtcp2_tstamp largest_pkt_sent_ts = 0;

  if (conn && (conn->flags & NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED) &&
      largest_ack >= conn->pktns.crypto.tx.ckm->pkt_num) {
    conn->flags &= (uint16_t)~NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED;
    conn->crypto.key_update.confirmed_ts = ts;

    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_CRY, "key update confirmed");
  }

  rtb->largest_acked_tx_pkt_num =
      ngtcp2_max(rtb->largest_acked_tx_pkt_num, largest_ack);

  /* Assume that ngtcp2_pkt_validate_ack(fr) returns 0 */
  it = ngtcp2_ksl_lower_bound(&rtb->ents,
                              ngtcp2_ksl_key_ptr(&key, &largest_ack));
  if (ngtcp2_ksl_it_end(&it)) {
    return 0;
  }

  min_ack = largest_ack - (int64_t)fr->first_ack_blklen;

  for (; !ngtcp2_ksl_it_end(&it);) {
    key = ngtcp2_ksl_it_key(&it);
    if (min_ack <= *key.i && *key.i <= largest_ack) {
      ent = ngtcp2_ksl_it_get(&it);
      if (conn) {
        rv = rtb_call_acked_stream_offset(rtb, ent, conn);
        if (rv != 0) {
          return rv;
        }
        if (largest_ack == *key.i) {
          largest_pkt_sent_ts = ent->ts;
          largest_pkt_acked = 1;
        }
        if (!rtt_updated && largest_pkt_acked &&
            (ent->flags & NGTCP2_RTB_FLAG_ACK_ELICITING)) {
          rtt_updated = 1;
          ngtcp2_conn_update_rtt(conn, ts - largest_pkt_sent_ts,
                                 fr->ack_delay_unscaled);
        }
        rtb_on_pkt_acked(rtb, ent, &conn->rcs, ts);
        /* At this point, it is invalided because rtb->ents might be
           modified. */
      }
      rtb_remove(rtb, &it, ent);
      ++num_acked;
      continue;
    }
    break;
  }

  for (i = 0; i < fr->num_blks;) {
    largest_ack = min_ack - (int64_t)fr->blks[i].gap - 2;
    min_ack = largest_ack - (int64_t)fr->blks[i].blklen;

    it = ngtcp2_ksl_lower_bound(&rtb->ents,
                                ngtcp2_ksl_key_ptr(&key, &largest_ack));
    if (ngtcp2_ksl_it_end(&it)) {
      break;
    }

    for (; !ngtcp2_ksl_it_end(&it);) {
      key = ngtcp2_ksl_it_key(&it);
      if (*key.i < min_ack) {
        break;
      }
      ent = ngtcp2_ksl_it_get(&it);
      if (conn) {
        rv = rtb_call_acked_stream_offset(rtb, ent, conn);
        if (rv != 0) {
          return rv;
        }
        if (!rtt_updated && largest_pkt_acked &&
            (ent->flags & NGTCP2_RTB_FLAG_ACK_ELICITING)) {
          rtt_updated = 1;
          ngtcp2_conn_update_rtt(conn, ts - largest_pkt_sent_ts,
                                 fr->ack_delay_unscaled);
        }
        rtb_on_pkt_acked(rtb, ent, &conn->rcs, ts);
      }
      rtb_remove(rtb, &it, ent);
      ++num_acked;
    }

    ++i;
  }

  return num_acked;
}

static int rtb_pkt_lost(ngtcp2_rtb *rtb, const ngtcp2_rtb_entry *ent,
                        uint64_t loss_delay, ngtcp2_tstamp lost_send_time) {
  if (ent->ts <= lost_send_time ||
      rtb->largest_acked_tx_pkt_num >= ent->hd.pkt_num + NGTCP2_PKT_THRESHOLD) {
    return 1;
  }

  if (rtb->loss_time == 0) {
    rtb->loss_time = ent->ts + loss_delay;
  } else {
    rtb->loss_time = ngtcp2_min(rtb->loss_time, ent->ts + loss_delay);
  }

  return 0;
}

/*
 * rtb_compute_pkt_loss_delay computes delay until packet is
 * considered lost in NGTCP2_MICROSECONDS resolution.
 */
static ngtcp2_duration compute_pkt_loss_delay(const ngtcp2_rcvry_stat *rcs) {
  /* 9/8 is kTimeThreshold */
  ngtcp2_duration loss_delay =
      ngtcp2_max(rcs->latest_rtt, rcs->smoothed_rtt) * 9 / 8;
  return ngtcp2_max(loss_delay, NGTCP2_GRANULARITY);
}

void ngtcp2_rtb_detect_lost_pkt(ngtcp2_rtb *rtb, ngtcp2_frame_chain **pfrc,
                                ngtcp2_rcvry_stat *rcs, ngtcp2_duration pto,
                                ngtcp2_tstamp ts) {
  ngtcp2_rtb_entry *ent;
  ngtcp2_duration loss_delay;
  ngtcp2_tstamp lost_send_time;
  ngtcp2_ksl_it it;
  ngtcp2_tstamp latest_ts, oldest_ts;
  int64_t last_lost_pkt_num;
  ngtcp2_ksl_key key;

  rtb->loss_time = 0;
  loss_delay = compute_pkt_loss_delay(rcs);
  lost_send_time = ts - loss_delay;

  it = ngtcp2_ksl_lower_bound(
      &rtb->ents, ngtcp2_ksl_key_ptr(&key, &rtb->largest_acked_tx_pkt_num));
  for (; !ngtcp2_ksl_it_end(&it); ngtcp2_ksl_it_next(&it)) {
    ent = ngtcp2_ksl_it_get(&it);

    if (rtb_pkt_lost(rtb, ent, loss_delay, lost_send_time)) {
      /* All entries from ent are considered to be lost. */
      latest_ts = oldest_ts = ent->ts;
      last_lost_pkt_num = ent->hd.pkt_num;

      for (; !ngtcp2_ksl_it_end(&it);) {
        ent = ngtcp2_ksl_it_get(&it);
        ngtcp2_ksl_remove(&rtb->ents, &it,
                          ngtcp2_ksl_key_ptr(&key, &ent->hd.pkt_num));

        if (last_lost_pkt_num == ent->hd.pkt_num + 1) {
          last_lost_pkt_num = ent->hd.pkt_num;
        } else {
          last_lost_pkt_num = -1;
        }

        oldest_ts = ent->ts;
        rtb_on_remove(rtb, ent);
        rtb_on_pkt_lost(rtb, pfrc, ent);
      }

      ngtcp2_default_cc_congestion_event(rtb->cc, latest_ts, ts);

      if (last_lost_pkt_num != -1) {
        ngtcp2_default_cc_handle_persistent_congestion(
            rtb->cc, latest_ts - oldest_ts, pto);
      }

      return;
    }
  }
}

void ngtcp2_rtb_remove_all(ngtcp2_rtb *rtb, ngtcp2_frame_chain **pfrc) {
  ngtcp2_rtb_entry *ent;
  ngtcp2_ksl_it it;
  ngtcp2_ksl_key key;

  it = ngtcp2_ksl_begin(&rtb->ents);

  for (; !ngtcp2_ksl_it_end(&it);) {
    ent = ngtcp2_ksl_it_get(&it);

    rtb_on_remove(rtb, ent);
    ngtcp2_ksl_remove(&rtb->ents, &it,
                      ngtcp2_ksl_key_ptr(&key, &ent->hd.pkt_num));

    rtb_on_pkt_lost(rtb, pfrc, ent);
  }
}

int ngtcp2_rtb_on_crypto_timeout(ngtcp2_rtb *rtb, ngtcp2_frame_chain **pfrc) {
  ngtcp2_rtb_entry *ent;
  ngtcp2_ksl_it it;
  ngtcp2_frame_chain *nfrc;
  ngtcp2_frame_chain *frc;
  ngtcp2_ksl_it gapit;
  ngtcp2_range gap, range;
  ngtcp2_crypto *fr;
  int all_acked;
  int rv;
  ngtcp2_ksl_key key;

  it = ngtcp2_ksl_begin(&rtb->ents);

  for (; !ngtcp2_ksl_it_end(&it);) {
    ent = ngtcp2_ksl_it_get(&it);

    if ((ent->flags & NGTCP2_RTB_FLAG_PROBE) ||
        !(ent->flags & NGTCP2_RTB_FLAG_CRYPTO_PKT)) {
      ngtcp2_ksl_it_next(&it);
      continue;
    }

    all_acked = 1;

    for (frc = ent->frc; frc; frc = frc->next) {
      assert(frc->fr.type == NGTCP2_FRAME_CRYPTO);

      fr = &frc->fr.crypto;

      /* Don't resend CRYPTO frame if the whole region it contains has
         been acknowledged */
      gapit = ngtcp2_gaptr_get_first_gap_after(&rtb->crypto->tx.acked_offset,
                                               fr->offset);
      gap = *(ngtcp2_range *)ngtcp2_ksl_it_key(&gapit).ptr;

      range.begin = fr->offset;
      range.end = fr->offset + ngtcp2_vec_len(fr->data, fr->datacnt);
      range = ngtcp2_range_intersect(&range, &gap);
      if (ngtcp2_range_len(&range) == 0) {
        continue;
      }

      all_acked = 0;

      if (!(ent->flags & NGTCP2_RTB_FLAG_CRYPTO_TIMEOUT_RETRANSMITTED)) {
        rv = ngtcp2_frame_chain_crypto_datacnt_new(
            &nfrc, frc->fr.crypto.datacnt, rtb->mem);
        if (rv != 0) {
          return rv;
        }

        nfrc->fr = frc->fr;
        ngtcp2_vec_copy(nfrc->fr.crypto.data, frc->fr.crypto.data,
                        frc->fr.crypto.datacnt);

        frame_chain_insert(pfrc, nfrc);
      }
    }

    if (all_acked) {
      /* If the frames that ent contains have been acknowledged,
         remove it from rtb.  Otherwise crypto timer keeps firing. */
      rtb_on_remove(rtb, ent);
      ngtcp2_ksl_remove(&rtb->ents, &it,
                        ngtcp2_ksl_key_ptr(&key, &ent->hd.pkt_num));
      ngtcp2_rtb_entry_del(ent, rtb->mem);
      continue;
    }

    ent->flags |= NGTCP2_RTB_FLAG_CRYPTO_TIMEOUT_RETRANSMITTED;

    ngtcp2_ksl_it_next(&it);
  }

  return 0;
}

int ngtcp2_rtb_empty(ngtcp2_rtb *rtb) {
  return ngtcp2_ksl_len(&rtb->ents) == 0;
}

void ngtcp2_rtb_clear(ngtcp2_rtb *rtb) {
  ngtcp2_ksl_it it;
  ngtcp2_rtb_entry *ent;

  it = ngtcp2_ksl_begin(&rtb->ents);

  for (; !ngtcp2_ksl_it_end(&it); ngtcp2_ksl_it_next(&it)) {
    ent = ngtcp2_ksl_it_get(&it);
    rtb->cc->ccs->bytes_in_flight -= ent->pktlen;
    ngtcp2_rtb_entry_del(ent, rtb->mem);
  }
  ngtcp2_ksl_clear(&rtb->ents);

  rtb->largest_acked_tx_pkt_num = -1;
  rtb->num_ack_eliciting = 0;
}

size_t ngtcp2_rtb_num_ack_eliciting(ngtcp2_rtb *rtb) {
  return rtb->num_ack_eliciting;
}

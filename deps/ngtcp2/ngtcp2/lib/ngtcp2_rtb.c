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
#include "ngtcp2_rst.h"

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

int ngtcp2_frame_chain_new_token_new(ngtcp2_frame_chain **pfrc,
                                     const ngtcp2_vec *token,
                                     const ngtcp2_mem *mem) {
  size_t avail = sizeof(ngtcp2_frame) - sizeof(ngtcp2_new_token);
  int rv;
  uint8_t *p;
  ngtcp2_frame *fr;

  if (token->len > avail) {
    rv = ngtcp2_frame_chain_extralen_new(pfrc, token->len - avail, mem);
  } else {
    rv = ngtcp2_frame_chain_new(pfrc, mem);
  }
  if (rv != 0) {
    return rv;
  }

  fr = &(*pfrc)->fr;
  fr->type = NGTCP2_FRAME_NEW_TOKEN;

  p = (uint8_t *)(*pfrc) + sizeof(ngtcp2_new_token);
  memcpy(p, token->base, token->len);

  ngtcp2_vec_init(&fr->new_token.token, p, token->len);

  return 0;
}

void ngtcp2_frame_chain_del(ngtcp2_frame_chain *frc, const ngtcp2_mem *mem) {
  ngtcp2_frame_chain_binder *binder;

  if (frc == NULL) {
    return;
  }

  binder = frc->binder;
  if (binder && --binder->refcount == 0) {
    ngtcp2_mem_free(mem, binder);
  }

  ngtcp2_mem_free(mem, frc);
}

void ngtcp2_frame_chain_init(ngtcp2_frame_chain *frc) {
  frc->next = NULL;
  frc->binder = NULL;
}

void ngtcp2_frame_chain_list_del(ngtcp2_frame_chain *frc,
                                 const ngtcp2_mem *mem) {
  ngtcp2_frame_chain *next;

  for (; frc;) {
    next = frc->next;
    ngtcp2_frame_chain_del(frc, mem);
    frc = next;
  }
}

int ngtcp2_frame_chain_binder_new(ngtcp2_frame_chain_binder **pbinder,
                                  const ngtcp2_mem *mem) {
  *pbinder = ngtcp2_mem_calloc(mem, 1, sizeof(ngtcp2_frame_chain_binder));
  if (*pbinder == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  return 0;
}

int ngtcp2_bind_frame_chains(ngtcp2_frame_chain *a, ngtcp2_frame_chain *b,
                             const ngtcp2_mem *mem) {
  ngtcp2_frame_chain_binder *binder;
  int rv;

  assert(b->binder == NULL);

  if (a->binder == NULL) {
    rv = ngtcp2_frame_chain_binder_new(&binder, mem);
    if (rv != 0) {
      return rv;
    }

    a->binder = binder;
    ++a->binder->refcount;
  }

  b->binder = a->binder;
  ++b->binder->refcount;

  return 0;
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
  (*pent)->lost_ts = UINT64_MAX;
  (*pent)->pktlen = pktlen;
  (*pent)->flags = flags;
  (*pent)->next = NULL;

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
  return *(int64_t *)lhs > *(int64_t *)rhs;
}

void ngtcp2_rtb_init(ngtcp2_rtb *rtb, ngtcp2_pktns_id pktns_id,
                     ngtcp2_strm *crypto, ngtcp2_rst *rst, ngtcp2_cc *cc,
                     ngtcp2_log *log, ngtcp2_qlog *qlog,
                     const ngtcp2_mem *mem) {
  ngtcp2_ksl_init(&rtb->ents, greater, sizeof(int64_t), mem);
  rtb->crypto = crypto;
  rtb->rst = rst;
  rtb->cc = cc;
  rtb->log = log;
  rtb->qlog = qlog;
  rtb->mem = mem;
  rtb->largest_acked_tx_pkt_num = -1;
  rtb->num_ack_eliciting = 0;
  rtb->num_retransmittable = 0;
  rtb->probe_pkt_left = 0;
  rtb->pktns_id = pktns_id;
  rtb->cc_pkt_num = 0;
  rtb->cc_bytes_in_flight = 0;
  rtb->persistent_congestion_start_ts = UINT64_MAX;
  rtb->num_lost_pkts = 0;
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

static void rtb_on_add(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                       ngtcp2_conn_stat *cstat) {
  ngtcp2_rst_on_pkt_sent(rtb->rst, ent, cstat);

  assert(rtb->cc_pkt_num <= ent->hd.pkt_num);

  cstat->bytes_in_flight += ent->pktlen;
  rtb->cc_bytes_in_flight += ent->pktlen;

  ngtcp2_rst_update_app_limited(rtb->rst, cstat);

  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) {
    ++rtb->num_ack_eliciting;
  }
  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE) {
    ++rtb->num_retransmittable;
  }
}

static void rtb_on_remove(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                          ngtcp2_conn_stat *cstat) {
  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED) {
    assert(rtb->num_lost_pkts);
    --rtb->num_lost_pkts;
    return;
  }

  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) {
    assert(rtb->num_ack_eliciting);
    --rtb->num_ack_eliciting;
  }

  if ((ent->flags & NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE) &&
      !(ent->flags & NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED)) {
    assert(rtb->num_retransmittable);
    --rtb->num_retransmittable;
  }

  if (rtb->cc_pkt_num <= ent->hd.pkt_num) {
    assert(cstat->bytes_in_flight >= ent->pktlen);
    cstat->bytes_in_flight -= ent->pktlen;

    assert(rtb->cc_bytes_in_flight >= ent->pktlen);
    rtb->cc_bytes_in_flight -= ent->pktlen;
  }
}

/*
 * rtb_reclaim_frame queues unacknowledged frames included in |ent|
 * for retransmission.  The re-queued frames are not deleted from
 * |ent|.  It returns the number of frames queued.
 */
static ngtcp2_ssize rtb_reclaim_frame(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                                      ngtcp2_pktns *pktns,
                                      ngtcp2_rtb_entry *ent) {
  ngtcp2_frame_chain *frc, *nfrc, **pfrc = &pktns->tx.frq;
  ngtcp2_frame *fr;
  ngtcp2_strm *strm;
  ngtcp2_range gap, range;
  size_t num_reclaimed = 0;
  int rv;

  /* PADDING only (or PADDING + ACK ) packets will have NULL
     ent->frc. */
  /* TODO Reconsider the order of pfrc */
  for (frc = ent->frc; frc; frc = frc->next) {
    fr = &frc->fr;
    /* Check that a late ACK acknowledged this frame. */
    if (frc->binder &&
        (frc->binder->flags & NGTCP2_FRAME_CHAIN_BINDER_FLAG_ACK)) {
      continue;
    }
    switch (frc->fr.type) {
    case NGTCP2_FRAME_STREAM:
      strm = ngtcp2_conn_find_stream(conn, fr->stream.stream_id);
      if (strm == NULL) {
        continue;
      }

      gap = ngtcp2_strm_get_unacked_range_after(strm, fr->stream.offset);

      range.begin = fr->stream.offset;
      range.end = fr->stream.offset +
                  ngtcp2_vec_len(fr->stream.data, fr->stream.datacnt);
      range = ngtcp2_range_intersect(&range, &gap);
      if (ngtcp2_range_len(&range) == 0 &&
          (!fr->stream.fin || (strm->flags & NGTCP2_STRM_FLAG_FIN_ACKED))) {
        continue;
      }

      rv = ngtcp2_frame_chain_stream_datacnt_new(&nfrc, fr->stream.datacnt,
                                                 rtb->mem);
      if (rv != 0) {
        return rv;
      }

      nfrc->fr = *fr;
      ngtcp2_vec_copy(nfrc->fr.stream.data, fr->stream.data,
                      fr->stream.datacnt);

      rv = ngtcp2_strm_streamfrq_push(strm, nfrc);
      if (rv != 0) {
        ngtcp2_frame_chain_del(nfrc, conn->mem);
        return rv;
      }
      if (!ngtcp2_strm_is_tx_queued(strm)) {
        strm->cycle = ngtcp2_conn_tx_strmq_first_cycle(conn);
        rv = ngtcp2_conn_tx_strmq_push(conn, strm);
        if (rv != 0) {
          return rv;
        }
      }

      ++num_reclaimed;

      continue;
    case NGTCP2_FRAME_CRYPTO:
      /* Don't resend CRYPTO frame if the whole region it contains has
         been acknowledged */
      gap = ngtcp2_strm_get_unacked_range_after(rtb->crypto, fr->crypto.offset);

      range.begin = fr->crypto.offset;
      range.end = fr->crypto.offset +
                  ngtcp2_vec_len(fr->crypto.data, fr->crypto.datacnt);
      range = ngtcp2_range_intersect(&range, &gap);
      if (ngtcp2_range_len(&range) == 0) {
        continue;
      }

      rv = ngtcp2_frame_chain_crypto_datacnt_new(&nfrc, fr->crypto.datacnt,
                                                 rtb->mem);
      if (rv != 0) {
        return rv;
      }

      nfrc->fr = *fr;
      ngtcp2_vec_copy(nfrc->fr.crypto.data, fr->crypto.data,
                      fr->crypto.datacnt);

      rv = ngtcp2_ksl_insert(&pktns->crypto.tx.frq, NULL,
                             &nfrc->fr.crypto.offset, nfrc);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        ngtcp2_frame_chain_del(nfrc, conn->mem);
        return rv;
      }

      ++num_reclaimed;

      continue;
    case NGTCP2_FRAME_NEW_TOKEN:
      rv = ngtcp2_frame_chain_new_token_new(&nfrc, &fr->new_token.token,
                                            rtb->mem);
      if (rv != 0) {
        return rv;
      }

      rv = ngtcp2_bind_frame_chains(frc, nfrc, rtb->mem);
      if (rv != 0) {
        return rv;
      }

      break;
    default:
      rv = ngtcp2_frame_chain_new(&nfrc, rtb->mem);
      if (rv != 0) {
        return rv;
      }

      nfrc->fr = *fr;

      rv = ngtcp2_bind_frame_chains(frc, nfrc, rtb->mem);
      if (rv != 0) {
        return rv;
      }

      break;
    }

    ++num_reclaimed;

    nfrc->next = *pfrc;
    *pfrc = nfrc;
    pfrc = &nfrc->next;
  }

  return (ngtcp2_ssize)num_reclaimed;
}

static int rtb_on_pkt_lost(ngtcp2_rtb *rtb, ngtcp2_ksl_it *it,
                           ngtcp2_rtb_entry *ent, ngtcp2_conn *conn,
                           ngtcp2_pktns *pktns, ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_ssize reclaimed;

  ngtcp2_log_pkt_lost(rtb->log, ent->hd.pkt_num, ent->hd.type, ent->hd.flags,
                      ent->ts);

  if (rtb->qlog) {
    ngtcp2_qlog_pkt_lost(rtb->qlog, ent);
  }

  if (!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_PROBE)) {
    if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED) {
      ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                      "pkn=%" PRId64 " has already been reclaimed on PTO",
                      ent->hd.pkt_num);
      assert(!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED));
      assert(UINT64_MAX == ent->lost_ts);

      ent->flags |= NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED;
      ent->lost_ts = ts;

      ++rtb->num_lost_pkts;

      ngtcp2_ksl_it_next(it);

      return 0;
    }

    if (ent->frc) {
      assert(!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED));
      assert(UINT64_MAX == ent->lost_ts);

      reclaimed = rtb_reclaim_frame(rtb, conn, pktns, ent);
      if (reclaimed < 0) {
        return (int)reclaimed;
      }

      if (reclaimed) {
        ent->flags |= NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED;
        ent->lost_ts = ts;

        ++rtb->num_lost_pkts;

        ngtcp2_ksl_it_next(it);

        return 0;
      }
    }
  } else {
    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                    "pkn=%" PRId64
                    " is a probe packet, no retransmission is necessary",
                    ent->hd.pkt_num);
  }

  rv = ngtcp2_ksl_remove(&rtb->ents, it, &ent->hd.pkt_num);
  assert(0 == rv);

  ngtcp2_rtb_entry_del(ent, rtb->mem);

  return 0;
}

int ngtcp2_rtb_add(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                   ngtcp2_conn_stat *cstat) {
  int rv;

  rv = ngtcp2_ksl_insert(&rtb->ents, NULL, &ent->hd.pkt_num, ent);
  if (rv != 0) {
    return rv;
  }

  rtb_on_add(rtb, ent, cstat);

  return 0;
}

ngtcp2_ksl_it ngtcp2_rtb_head(ngtcp2_rtb *rtb) {
  return ngtcp2_ksl_begin(&rtb->ents);
}

static void rtb_remove(ngtcp2_rtb *rtb, ngtcp2_ksl_it *it,
                       ngtcp2_rtb_entry **pent, ngtcp2_rtb_entry *ent,
                       ngtcp2_conn_stat *cstat) {
  int rv;
  rv = ngtcp2_ksl_remove(&rtb->ents, it, &ent->hd.pkt_num);
  assert(0 == rv);
  rtb_on_remove(rtb, ent, cstat);

  assert(ent->next == NULL);

  ngtcp2_list_insert(ent, pent);
}

static int rtb_process_acked_pkt(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                                 ngtcp2_conn *conn) {
  ngtcp2_frame_chain *frc;
  uint64_t prev_stream_offset, stream_offset;
  ngtcp2_strm *strm;
  int rv;
  uint64_t datalen;
  ngtcp2_strm *crypto = rtb->crypto;
  ngtcp2_crypto_level crypto_level;

  for (frc = ent->frc; frc; frc = frc->next) {
    if (frc->binder) {
      frc->binder->flags |= NGTCP2_FRAME_CHAIN_BINDER_FLAG_ACK;
    }

    switch (frc->fr.type) {
    case NGTCP2_FRAME_STREAM:
      strm = ngtcp2_conn_find_stream(conn, frc->fr.stream.stream_id);
      if (strm == NULL) {
        break;
      }

      if (frc->fr.stream.fin) {
        strm->flags |= NGTCP2_STRM_FLAG_FIN_ACKED;
      }

      prev_stream_offset = ngtcp2_strm_get_acked_offset(strm);
      rv = ngtcp2_strm_ack_data(
          strm, frc->fr.stream.offset,
          ngtcp2_vec_len(frc->fr.stream.data, frc->fr.stream.datacnt));
      if (rv != 0) {
        return rv;
      }

      if (conn->callbacks.acked_stream_data_offset) {
        stream_offset = ngtcp2_strm_get_acked_offset(strm);
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
      prev_stream_offset = ngtcp2_strm_get_acked_offset(crypto);
      rv = ngtcp2_strm_ack_data(
          crypto, frc->fr.crypto.offset,
          ngtcp2_vec_len(frc->fr.crypto.data, frc->fr.crypto.datacnt));
      if (rv != 0) {
        return rv;
      }

      if (conn->callbacks.acked_crypto_offset) {
        stream_offset = ngtcp2_strm_get_acked_offset(crypto);
        datalen = stream_offset - prev_stream_offset;
        if (datalen == 0) {
          break;
        }

        switch (rtb->pktns_id) {
        case NGTCP2_PKTNS_ID_INITIAL:
          crypto_level = NGTCP2_CRYPTO_LEVEL_INITIAL;
          break;
        case NGTCP2_PKTNS_ID_HANDSHAKE:
          crypto_level = NGTCP2_CRYPTO_LEVEL_HANDSHAKE;
          break;
        case NGTCP2_PKTNS_ID_APPLICATION:
          crypto_level = NGTCP2_CRYPTO_LEVEL_APPLICATION;
          break;
        default:
          assert(0);
        }

        rv = conn->callbacks.acked_crypto_offset(
            conn, crypto_level, prev_stream_offset, datalen, conn->user_data);
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
    case NGTCP2_FRAME_RETIRE_CONNECTION_ID:
      assert(conn->dcid.num_retire_queued);
      --conn->dcid.num_retire_queued;
      break;
    }
  }
  return 0;
}

static void rtb_on_pkt_acked(ngtcp2_rtb *rtb, ngtcp2_rtb_entry *ent,
                             ngtcp2_conn_stat *cstat, ngtcp2_tstamp ts) {
  ngtcp2_cc *cc = rtb->cc;
  ngtcp2_cc_pkt pkt;

  ngtcp2_rst_update_rate_sample(rtb->rst, ent, ts);

  cc->on_pkt_acked(cc, cstat,
                   ngtcp2_cc_pkt_init(&pkt, ent->hd.pkt_num, ent->pktlen,
                                      rtb->pktns_id, ent->ts),
                   ts);

  if (!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_PROBE) &&
      (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING)) {
    cstat->pto_count = 0;
  }
}

static void conn_verify_ecn(ngtcp2_conn *conn, ngtcp2_pktns *pktns,
                            ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                            const ngtcp2_ack *fr, size_t ecn_acked,
                            ngtcp2_tstamp largest_acked_sent_ts,
                            ngtcp2_tstamp ts) {
  if (conn->tx.ecn.state == NGTCP2_ECN_STATE_FAILED) {
    return;
  }

  if ((ecn_acked && fr->type == NGTCP2_FRAME_ACK) ||
      (fr->type == NGTCP2_FRAME_ACK_ECN &&
       (pktns->rx.ecn.ack.ect0 > fr->ecn.ect0 ||
        pktns->rx.ecn.ack.ect1 > fr->ecn.ect1 ||
        pktns->rx.ecn.ack.ce > fr->ecn.ce ||
        (fr->ecn.ect0 - pktns->rx.ecn.ack.ect0) +
                (fr->ecn.ce - pktns->rx.ecn.ack.ce) <
            ecn_acked ||
        fr->ecn.ect0 > pktns->tx.ecn.ect0 || fr->ecn.ect1))) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                    "path is not ECN capable");
    conn->tx.ecn.state = NGTCP2_ECN_STATE_FAILED;
    return;
  }

  if (conn->tx.ecn.state != NGTCP2_ECN_STATE_CAPABLE && ecn_acked) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON, "path is ECN capable");
    conn->tx.ecn.state = NGTCP2_ECN_STATE_CAPABLE;
  }

  if (fr->type == NGTCP2_FRAME_ACK_ECN) {
    if (largest_acked_sent_ts != UINT64_MAX &&
        fr->ecn.ce > pktns->rx.ecn.ack.ce) {
      cc->congestion_event(cc, cstat, largest_acked_sent_ts, ts);
    }

    pktns->rx.ecn.ack.ect0 = fr->ecn.ect0;
    pktns->rx.ecn.ack.ect1 = fr->ecn.ect1;
    pktns->rx.ecn.ack.ce = fr->ecn.ce;
  }
}

ngtcp2_ssize ngtcp2_rtb_recv_ack(ngtcp2_rtb *rtb, const ngtcp2_ack *fr,
                                 ngtcp2_conn_stat *cstat, ngtcp2_conn *conn,
                                 ngtcp2_pktns *pktns, ngtcp2_tstamp pkt_ts,
                                 ngtcp2_tstamp ts) {
  ngtcp2_rtb_entry *ent;
  int64_t largest_ack = fr->largest_ack, min_ack;
  size_t i;
  int rv;
  ngtcp2_ksl_it it;
  ngtcp2_ssize num_acked = 0;
  ngtcp2_tstamp largest_pkt_sent_ts = UINT64_MAX;
  ngtcp2_tstamp largest_acked_sent_ts = UINT64_MAX;
  int64_t pkt_num;
  ngtcp2_cc *cc = rtb->cc;
  ngtcp2_rtb_entry *acked_ent = NULL;
  int ack_eliciting_pkt_acked = 0;
  size_t ecn_acked = 0;
  int verify_ecn = 0;

  if (conn && (conn->flags & NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED) &&
      largest_ack >= conn->pktns.crypto.tx.ckm->pkt_num) {
    conn->flags &= (uint16_t)~NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED;
    conn->crypto.key_update.confirmed_ts = ts;

    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_CRY, "key update confirmed");
  }

  if (rtb->largest_acked_tx_pkt_num < largest_ack) {
    rtb->largest_acked_tx_pkt_num = largest_ack;
    verify_ecn = 1;
  }

  /* Assume that ngtcp2_pkt_validate_ack(fr) returns 0 */
  it = ngtcp2_ksl_lower_bound(&rtb->ents, &largest_ack);
  if (ngtcp2_ksl_it_end(&it)) {
    if (verify_ecn) {
      conn_verify_ecn(conn, pktns, rtb->cc, cstat, fr, ecn_acked,
                      largest_acked_sent_ts, ts);
    }
    return 0;
  }

  min_ack = largest_ack - (int64_t)fr->first_ack_blklen;

  for (; !ngtcp2_ksl_it_end(&it);) {
    pkt_num = *(int64_t *)ngtcp2_ksl_it_key(&it);

    assert(pkt_num <= largest_ack);

    if (pkt_num < min_ack) {
      break;
    }

    ent = ngtcp2_ksl_it_get(&it);

    if (largest_ack == pkt_num) {
      largest_pkt_sent_ts = ent->ts;
    }

    if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) {
      ack_eliciting_pkt_acked = 1;
    }

    rtb_remove(rtb, &it, &acked_ent, ent, cstat);
    ++num_acked;
  }

  for (i = 0; i < fr->num_blks;) {
    largest_ack = min_ack - (int64_t)fr->blks[i].gap - 2;
    min_ack = largest_ack - (int64_t)fr->blks[i].blklen;

    it = ngtcp2_ksl_lower_bound(&rtb->ents, &largest_ack);
    if (ngtcp2_ksl_it_end(&it)) {
      break;
    }

    for (; !ngtcp2_ksl_it_end(&it);) {
      pkt_num = *(int64_t *)ngtcp2_ksl_it_key(&it);
      if (pkt_num < min_ack) {
        break;
      }
      ent = ngtcp2_ksl_it_get(&it);

      if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) {
        ack_eliciting_pkt_acked = 1;
      }

      rtb_remove(rtb, &it, &acked_ent, ent, cstat);
      ++num_acked;
    }

    ++i;
  }

  if (largest_pkt_sent_ts != UINT64_MAX && ack_eliciting_pkt_acked) {
    ngtcp2_conn_update_rtt(conn, pkt_ts - largest_pkt_sent_ts,
                           fr->ack_delay_unscaled, ts);
    if (cc->new_rtt_sample) {
      cc->new_rtt_sample(cc, cstat, ts);
    }
  }

  ngtcp2_rst_on_ack_recv(rtb->rst, cstat);

  if (conn) {
    for (ent = acked_ent; ent; ent = acked_ent) {
      if (ent->hd.pkt_num >= pktns->tx.ecn.start_pkt_num &&
          (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ECN)) {
        ++ecn_acked;
      }

      assert(largest_acked_sent_ts == UINT64_MAX ||
             largest_acked_sent_ts <= ent->ts);

      largest_acked_sent_ts = ent->ts;

      rv = rtb_process_acked_pkt(rtb, ent, conn);
      if (rv != 0) {
        goto fail;
      }

      rtb_on_pkt_acked(rtb, ent, cstat, ts);
      acked_ent = ent->next;
      ngtcp2_rtb_entry_del(ent, rtb->mem);
    }

    if (verify_ecn) {
      conn_verify_ecn(conn, pktns, rtb->cc, cstat, fr, ecn_acked,
                      largest_acked_sent_ts, ts);
    }
  } else {
    /* For unit tests */
    for (ent = acked_ent; ent; ent = acked_ent) {
      rtb_on_pkt_acked(rtb, ent, cstat, ts);
      acked_ent = ent->next;
      ngtcp2_rtb_entry_del(ent, rtb->mem);
    }
  }

  cc->on_ack_recv(cc, cstat, ts);

  return num_acked;

fail:
  for (ent = acked_ent; ent; ent = acked_ent) {
    acked_ent = ent->next;
    ngtcp2_rtb_entry_del(ent, rtb->mem);
  }

  return rv;
}

static int rtb_pkt_lost(ngtcp2_rtb *rtb, ngtcp2_conn_stat *cstat,
                        const ngtcp2_rtb_entry *ent, uint64_t loss_delay,
                        ngtcp2_tstamp lost_send_time, uint64_t pkt_thres) {
  ngtcp2_tstamp loss_time;

  if (ent->ts <= lost_send_time ||
      rtb->largest_acked_tx_pkt_num >= ent->hd.pkt_num + (int64_t)pkt_thres) {
    return 1;
  }

  loss_time = cstat->loss_time[rtb->pktns_id];

  if (loss_time == UINT64_MAX) {
    loss_time = ent->ts + loss_delay;
  } else {
    loss_time = ngtcp2_min(loss_time, ent->ts + loss_delay);
  }

  cstat->loss_time[rtb->pktns_id] = loss_time;

  return 0;
}

/*
 * rtb_compute_pkt_loss_delay computes loss delay.
 */
static ngtcp2_duration compute_pkt_loss_delay(const ngtcp2_conn_stat *cstat) {
  /* 9/8 is kTimeThreshold */
  ngtcp2_duration loss_delay =
      ngtcp2_max(cstat->latest_rtt, cstat->smoothed_rtt) * 9 / 8;
  return ngtcp2_max(loss_delay, NGTCP2_GRANULARITY);
}

/*
 * conn_all_ecn_pkt_lost returns nonzero if all ECN QUIC packets are
 * lost during validation period.
 */
static int conn_all_ecn_pkt_lost(ngtcp2_conn *conn) {
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;
  ngtcp2_pktns *pktns = &conn->pktns;

  return (!in_pktns || in_pktns->tx.ecn.validation_pkt_sent ==
                           in_pktns->tx.ecn.validation_pkt_lost) &&
         (!hs_pktns || hs_pktns->tx.ecn.validation_pkt_sent ==
                           hs_pktns->tx.ecn.validation_pkt_lost) &&
         pktns->tx.ecn.validation_pkt_sent == pktns->tx.ecn.validation_pkt_lost;
}

int ngtcp2_rtb_detect_lost_pkt(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                               ngtcp2_pktns *pktns, ngtcp2_conn_stat *cstat,
                               ngtcp2_duration pto, ngtcp2_tstamp ts) {
  ngtcp2_rtb_entry *ent;
  ngtcp2_duration loss_delay;
  ngtcp2_tstamp lost_send_time;
  ngtcp2_ksl_it it;
  ngtcp2_tstamp latest_ts, oldest_ts;
  int64_t last_lost_pkt_num;
  ngtcp2_duration loss_window, congestion_period;
  ngtcp2_cc *cc = rtb->cc;
  int rv;
  uint64_t pkt_thres =
      rtb->cc_bytes_in_flight / cstat->max_udp_payload_size / 2;
  size_t ecn_pkt_lost = 0;
  ngtcp2_tstamp start_ts;

  pkt_thres = ngtcp2_max(pkt_thres, NGTCP2_PKT_THRESHOLD);
  cstat->loss_time[rtb->pktns_id] = UINT64_MAX;
  loss_delay = compute_pkt_loss_delay(cstat);
  lost_send_time = ts - loss_delay;

  it = ngtcp2_ksl_lower_bound(&rtb->ents, &rtb->largest_acked_tx_pkt_num);
  for (; !ngtcp2_ksl_it_end(&it); ngtcp2_ksl_it_next(&it)) {
    ent = ngtcp2_ksl_it_get(&it);

    if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED) {
      break;
    }

    if (rtb_pkt_lost(rtb, cstat, ent, loss_delay, lost_send_time, pkt_thres)) {
      /* All entries from ent are considered to be lost. */
      latest_ts = oldest_ts = ent->ts;
      last_lost_pkt_num = ent->hd.pkt_num;

      congestion_period = (cstat->smoothed_rtt +
                           ngtcp2_max(4 * cstat->rttvar, NGTCP2_GRANULARITY) +
                           conn->remote.transport_params.max_ack_delay) *
                          NGTCP2_PERSISTENT_CONGESTION_THRESHOLD;

      start_ts = ngtcp2_max(rtb->persistent_congestion_start_ts,
                            cstat->first_rtt_sample_ts);

      for (; !ngtcp2_ksl_it_end(&it);) {
        ent = ngtcp2_ksl_it_get(&it);

        if (last_lost_pkt_num == ent->hd.pkt_num + 1 && ent->ts >= start_ts) {
          last_lost_pkt_num = ent->hd.pkt_num;
          oldest_ts = ent->ts;
        } else {
          last_lost_pkt_num = -1;
        }

        if ((ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED)) {
          if (rtb->pktns_id != NGTCP2_PKTNS_ID_APPLICATION ||
              last_lost_pkt_num == -1 ||
              latest_ts - oldest_ts >= congestion_period) {
            break;
          }
          ngtcp2_ksl_it_next(&it);
          continue;
        }

        if (ent->hd.pkt_num >= pktns->tx.ecn.start_pkt_num &&
            (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ECN)) {
          ++ecn_pkt_lost;
        }

        rtb_on_remove(rtb, ent, cstat);
        rv = rtb_on_pkt_lost(rtb, &it, ent, conn, pktns, ts);
        if (rv != 0) {
          return rv;
        }
      }

      switch (conn->tx.ecn.state) {
      case NGTCP2_ECN_STATE_TESTING:
        if (conn->tx.ecn.validation_start_ts == UINT64_MAX) {
          break;
        }
        if (ts - conn->tx.ecn.validation_start_ts < 3 * pto) {
          pktns->tx.ecn.validation_pkt_lost += ecn_pkt_lost;
          assert(pktns->tx.ecn.validation_pkt_sent >=
                 pktns->tx.ecn.validation_pkt_lost);
          break;
        }
        conn->tx.ecn.state = NGTCP2_ECN_STATE_UNKNOWN;
        /* fall through */
      case NGTCP2_ECN_STATE_UNKNOWN:
        pktns->tx.ecn.validation_pkt_lost += ecn_pkt_lost;
        assert(pktns->tx.ecn.validation_pkt_sent >=
               pktns->tx.ecn.validation_pkt_lost);
        if (conn_all_ecn_pkt_lost(conn)) {
          conn->tx.ecn.state = NGTCP2_ECN_STATE_FAILED;
        }
        break;
      default:
        break;
      }

      cc->congestion_event(cc, cstat, latest_ts, ts);

      loss_window = latest_ts - oldest_ts;
      /* Persistent congestion situation is only evaluated for app
       * packet number space and for the packets sent after handshake
       * is confirmed.  During handshake, there is not much packets
       * sent and also people seem to do lots of effort not to trigger
       * persistent congestion there, then it is a lot easier to just
       * not enable it during handshake.
       */
      if (rtb->pktns_id == NGTCP2_PKTNS_ID_APPLICATION && loss_window > 0) {
        if (loss_window >= congestion_period) {
          ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                          "persistent congestion loss_window=%" PRIu64
                          " congestion_period=%" PRIu64,
                          loss_window, congestion_period);

          /* Reset min_rtt, srtt, and rttvar here.  Next new RTT
             sample will be used to recalculate these values. */
          cstat->min_rtt = UINT64_MAX;
          cstat->smoothed_rtt = conn->local.settings.initial_rtt;
          cstat->rttvar = conn->local.settings.initial_rtt / 2;
          cstat->first_rtt_sample_ts = UINT64_MAX;

          cc->on_persistent_congestion(cc, cstat, ts);
        }
      }

      break;
    }
  }

  ngtcp2_rtb_remove_excessive_lost_pkt(rtb, pkt_thres);

  return 0;
}

void ngtcp2_rtb_remove_excessive_lost_pkt(ngtcp2_rtb *rtb, size_t n) {
  ngtcp2_ksl_it it = ngtcp2_ksl_end(&rtb->ents);
  ngtcp2_rtb_entry *ent;
  int rv;

  for (; rtb->num_lost_pkts > n;) {
    assert(ngtcp2_ksl_it_end(&it));
    ngtcp2_ksl_it_prev(&it);
    ent = ngtcp2_ksl_it_get(&it);

    assert(ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED);

    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                    "removing stale lost pkn=%" PRId64, ent->hd.pkt_num);

    --rtb->num_lost_pkts;
    rv = ngtcp2_ksl_remove(&rtb->ents, &it, &ent->hd.pkt_num);
    assert(0 == rv);
    ngtcp2_rtb_entry_del(ent, rtb->mem);
  }
}

void ngtcp2_rtb_remove_expired_lost_pkt(ngtcp2_rtb *rtb, ngtcp2_duration pto,
                                        ngtcp2_tstamp ts) {
  ngtcp2_ksl_it it;
  ngtcp2_rtb_entry *ent;
  int rv;

  if (ngtcp2_ksl_len(&rtb->ents) == 0) {
    return;
  }

  it = ngtcp2_ksl_end(&rtb->ents);

  for (;;) {
    assert(ngtcp2_ksl_it_end(&it));

    ngtcp2_ksl_it_prev(&it);
    ent = ngtcp2_ksl_it_get(&it);

    if (!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED) ||
        ts - ent->lost_ts < pto) {
      return;
    }

    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                    "removing stale lost pkn=%" PRId64, ent->hd.pkt_num);

    --rtb->num_lost_pkts;
    rv = ngtcp2_ksl_remove(&rtb->ents, &it, &ent->hd.pkt_num);
    assert(0 == rv);
    ngtcp2_rtb_entry_del(ent, rtb->mem);

    if (ngtcp2_ksl_len(&rtb->ents) == 0) {
      return;
    }
  }
}

ngtcp2_tstamp ngtcp2_rtb_lost_pkt_ts(ngtcp2_rtb *rtb) {
  ngtcp2_ksl_it it;
  ngtcp2_rtb_entry *ent;

  if (ngtcp2_ksl_len(&rtb->ents) == 0) {
    return UINT64_MAX;
  }

  it = ngtcp2_ksl_end(&rtb->ents);
  ngtcp2_ksl_it_prev(&it);
  ent = ngtcp2_ksl_it_get(&it);

  if (!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED)) {
    return UINT64_MAX;
  }

  return ent->lost_ts;
}

static int rtb_on_pkt_lost_resched_move(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                                        ngtcp2_pktns *pktns,
                                        ngtcp2_rtb_entry *ent) {
  ngtcp2_frame_chain **pfrc, *frc;
  ngtcp2_stream *sfr;
  ngtcp2_strm *strm;
  int rv;

  ngtcp2_log_pkt_lost(rtb->log, ent->hd.pkt_num, ent->hd.type, ent->hd.flags,
                      ent->ts);

  if (rtb->qlog) {
    ngtcp2_qlog_pkt_lost(rtb->qlog, ent);
  }

  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_PROBE) {
    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                    "pkn=%" PRId64
                    " is a probe packet, no retransmission is necessary",
                    ent->hd.pkt_num);
    return 0;
  }

  if (!ent->frc) {
    /* PADDING only (or PADDING + ACK ) packets will have NULL
       ent->frc. */
    assert(!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED));
    assert(!(ent->flags & NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED));
    return 0;
  }

  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED) {
    --rtb->num_lost_pkts;
  }

  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED) {
    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                    "pkn=%" PRId64
                    " was declared lost and has already been retransmitted",
                    ent->hd.pkt_num);
    return 0;
  }

  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED) {
    ngtcp2_log_info(rtb->log, NGTCP2_LOG_EVENT_RCV,
                    "pkn=%" PRId64 " has already been reclaimed on PTO",
                    ent->hd.pkt_num);
    return 0;
  }

  pfrc = &ent->frc;

  for (; *pfrc;) {
    switch ((*pfrc)->fr.type) {
    case NGTCP2_FRAME_STREAM:
      frc = *pfrc;

      *pfrc = frc->next;
      frc->next = NULL;
      sfr = &frc->fr.stream;

      strm = ngtcp2_conn_find_stream(conn, sfr->stream_id);
      if (!strm) {
        ngtcp2_frame_chain_del(frc, conn->mem);
        break;
      }
      rv = ngtcp2_strm_streamfrq_push(strm, frc);
      if (rv != 0) {
        ngtcp2_frame_chain_del(frc, conn->mem);
        return rv;
      }
      if (!ngtcp2_strm_is_tx_queued(strm)) {
        strm->cycle = ngtcp2_conn_tx_strmq_first_cycle(conn);
        rv = ngtcp2_conn_tx_strmq_push(conn, strm);
        if (rv != 0) {
          return rv;
        }
      }
      break;
    case NGTCP2_FRAME_CRYPTO:
      frc = *pfrc;

      *pfrc = frc->next;
      frc->next = NULL;

      rv = ngtcp2_ksl_insert(&pktns->crypto.tx.frq, NULL,
                             &frc->fr.crypto.offset, frc);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        ngtcp2_frame_chain_del(frc, conn->mem);
        return rv;
      }
      break;
    default:
      pfrc = &(*pfrc)->next;
    }
  }

  *pfrc = pktns->tx.frq;
  pktns->tx.frq = ent->frc;
  ent->frc = NULL;

  return 0;
}

int ngtcp2_rtb_remove_all(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                          ngtcp2_pktns *pktns, ngtcp2_conn_stat *cstat) {
  ngtcp2_rtb_entry *ent;
  ngtcp2_ksl_it it;
  int rv;

  it = ngtcp2_ksl_begin(&rtb->ents);

  for (; !ngtcp2_ksl_it_end(&it);) {
    ent = ngtcp2_ksl_it_get(&it);

    rtb_on_remove(rtb, ent, cstat);
    rv = ngtcp2_ksl_remove(&rtb->ents, &it, &ent->hd.pkt_num);
    assert(0 == rv);

    rv = rtb_on_pkt_lost_resched_move(rtb, conn, pktns, ent);
    ngtcp2_rtb_entry_del(ent, rtb->mem);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

int ngtcp2_rtb_empty(ngtcp2_rtb *rtb) {
  return ngtcp2_ksl_len(&rtb->ents) == 0;
}

void ngtcp2_rtb_reset_cc_state(ngtcp2_rtb *rtb, int64_t cc_pkt_num) {
  rtb->cc_pkt_num = cc_pkt_num;
  rtb->cc_bytes_in_flight = 0;
}

ngtcp2_ssize ngtcp2_rtb_reclaim_on_pto(ngtcp2_rtb *rtb, ngtcp2_conn *conn,
                                       ngtcp2_pktns *pktns, size_t num_pkts) {
  ngtcp2_ksl_it it;
  ngtcp2_rtb_entry *ent;
  ngtcp2_ssize reclaimed;
  size_t atmost = num_pkts;

  it = ngtcp2_ksl_end(&rtb->ents);
  for (; !ngtcp2_ksl_it_begin(&it) && num_pkts >= 1;) {
    ngtcp2_ksl_it_prev(&it);
    ent = ngtcp2_ksl_it_get(&it);

    if ((ent->flags & (NGTCP2_RTB_ENTRY_FLAG_LOST_RETRANSMITTED |
                       NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED)) ||
        !(ent->flags & NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE)) {
      continue;
    }

    assert(ent->frc);

    reclaimed = rtb_reclaim_frame(rtb, conn, pktns, ent);
    if (reclaimed < 0) {
      return reclaimed;
    }

    /* Mark reclaimed even if reclaimed == 0 so that we can skip it in
       the next run. */
    ent->flags |= NGTCP2_RTB_ENTRY_FLAG_PTO_RECLAIMED;

    assert(rtb->num_retransmittable);
    --rtb->num_retransmittable;

    if (reclaimed) {
      --num_pkts;
    }
  }

  return (ngtcp2_ssize)(atmost - num_pkts);
}

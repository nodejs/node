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
#include "ngtcp2_conn.h"

#include <string.h>
#include <assert.h>
#include <math.h>

#include "ngtcp2_macro.h"
#include "ngtcp2_log.h"
#include "ngtcp2_cid.h"
#include "ngtcp2_conv.h"
#include "ngtcp2_vec.h"
#include "ngtcp2_addr.h"
#include "ngtcp2_path.h"
#include "ngtcp2_rcvry.h"

/* NGTCP2_FLOW_WINDOW_RTT_FACTOR is the factor of RTT when flow
   control window auto-tuning is triggered. */
#define NGTCP2_FLOW_WINDOW_RTT_FACTOR 2
/* NGTCP2_FLOW_WINDOW_SCALING_FACTOR is the growth factor of flow
   control window. */
#define NGTCP2_FLOW_WINDOW_SCALING_FACTOR 2

/*
 * conn_local_stream returns nonzero if |stream_id| indicates that it
 * is the stream initiated by local endpoint.
 */
static int conn_local_stream(ngtcp2_conn *conn, int64_t stream_id) {
  return (uint8_t)(stream_id & 1) == conn->server;
}

/*
 * bidi_stream returns nonzero if |stream_id| is a bidirectional
 * stream ID.
 */
static int bidi_stream(int64_t stream_id) { return (stream_id & 0x2) == 0; }

static int conn_call_recv_client_initial(ngtcp2_conn *conn,
                                         const ngtcp2_cid *dcid) {
  int rv;

  assert(conn->callbacks.recv_client_initial);

  rv = conn->callbacks.recv_client_initial(conn, dcid, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_handshake_completed(ngtcp2_conn *conn) {
  int rv;

  if (!conn->callbacks.handshake_completed) {
    return 0;
  }

  rv = conn->callbacks.handshake_completed(conn, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_recv_stream_data(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                      uint32_t flags, uint64_t offset,
                                      const uint8_t *data, size_t datalen) {
  int rv;

  if (!conn->callbacks.recv_stream_data) {
    return 0;
  }

  rv = conn->callbacks.recv_stream_data(conn, flags, strm->stream_id, offset,
                                        data, datalen, conn->user_data,
                                        strm->stream_user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_recv_crypto_data(ngtcp2_conn *conn,
                                      ngtcp2_crypto_level crypto_level,
                                      uint64_t offset, const uint8_t *data,
                                      size_t datalen) {
  int rv;

  assert(conn->callbacks.recv_crypto_data);

  rv = conn->callbacks.recv_crypto_data(conn, crypto_level, offset, data,
                                        datalen, conn->user_data);
  switch (rv) {
  case 0:
  case NGTCP2_ERR_CRYPTO:
  case NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM:
  case NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM:
  case NGTCP2_ERR_TRANSPORT_PARAM:
  case NGTCP2_ERR_PROTO:
  case NGTCP2_ERR_CALLBACK_FAILURE:
    return rv;
  default:
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }
}

static int conn_call_stream_open(ngtcp2_conn *conn, ngtcp2_strm *strm) {
  int rv;

  if (!conn->callbacks.stream_open) {
    return 0;
  }

  rv = conn->callbacks.stream_open(conn, strm->stream_id, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_stream_close(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                  uint64_t app_error_code) {
  int rv;

  if (!conn->callbacks.stream_close) {
    return 0;
  }

  rv = conn->callbacks.stream_close(conn, strm->stream_id, app_error_code,
                                    conn->user_data, strm->stream_user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_stream_reset(ngtcp2_conn *conn, int64_t stream_id,
                                  uint64_t final_size, uint64_t app_error_code,
                                  void *stream_user_data) {
  int rv;

  if (!conn->callbacks.stream_reset) {
    return 0;
  }

  rv = conn->callbacks.stream_reset(conn, stream_id, final_size, app_error_code,
                                    conn->user_data, stream_user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_extend_max_local_streams_bidi(ngtcp2_conn *conn,
                                                   uint64_t max_streams) {
  int rv;

  if (!conn->callbacks.extend_max_local_streams_bidi) {
    return 0;
  }

  rv = conn->callbacks.extend_max_local_streams_bidi(conn, max_streams,
                                                     conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_extend_max_local_streams_uni(ngtcp2_conn *conn,
                                                  uint64_t max_streams) {
  int rv;

  if (!conn->callbacks.extend_max_local_streams_uni) {
    return 0;
  }

  rv = conn->callbacks.extend_max_local_streams_uni(conn, max_streams,
                                                    conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_get_new_connection_id(ngtcp2_conn *conn, ngtcp2_cid *cid,
                                           uint8_t *token, size_t cidlen) {
  int rv;

  assert(conn->callbacks.get_new_connection_id);

  rv = conn->callbacks.get_new_connection_id(conn, cid, token, cidlen,
                                             conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_remove_connection_id(ngtcp2_conn *conn,
                                          const ngtcp2_cid *cid) {
  int rv;

  if (!conn->callbacks.remove_connection_id) {
    return 0;
  }

  rv = conn->callbacks.remove_connection_id(conn, cid, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_path_validation(ngtcp2_conn *conn, const ngtcp2_path *path,
                                     ngtcp2_path_validation_result res) {
  int rv;

  if (!conn->callbacks.path_validation) {
    return 0;
  }

  rv = conn->callbacks.path_validation(conn, path, res, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_select_preferred_addr(ngtcp2_conn *conn,
                                           ngtcp2_addr *dest) {
  int rv;

  if (!conn->callbacks.select_preferred_addr) {
    return 0;
  }

  assert(conn->remote.transport_params.preferred_address_present);

  rv = conn->callbacks.select_preferred_addr(
      conn, dest, &conn->remote.transport_params.preferred_address,
      conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_extend_max_remote_streams_bidi(ngtcp2_conn *conn,
                                                    uint64_t max_streams) {
  int rv;

  if (!conn->callbacks.extend_max_remote_streams_bidi) {
    return 0;
  }

  rv = conn->callbacks.extend_max_remote_streams_bidi(conn, max_streams,
                                                      conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_extend_max_remote_streams_uni(ngtcp2_conn *conn,
                                                   uint64_t max_streams) {
  int rv;

  if (!conn->callbacks.extend_max_remote_streams_uni) {
    return 0;
  }

  rv = conn->callbacks.extend_max_remote_streams_uni(conn, max_streams,
                                                     conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_extend_max_stream_data(ngtcp2_conn *conn,
                                            ngtcp2_strm *strm,
                                            int64_t stream_id,
                                            uint64_t datalen) {
  int rv;

  if (!conn->callbacks.extend_max_stream_data) {
    return 0;
  }

  rv = conn->callbacks.extend_max_stream_data(
      conn, stream_id, datalen, conn->user_data, strm->stream_user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_dcid_status(ngtcp2_conn *conn,
                                 ngtcp2_connection_id_status_type type,
                                 const ngtcp2_dcid *dcid) {
  int rv;

  if (!conn->callbacks.dcid_status) {
    return 0;
  }

  rv = conn->callbacks.dcid_status(
      conn, (int)type, dcid->seq, &dcid->cid,
      ngtcp2_check_invalid_stateless_reset_token(dcid->token) ? NULL
                                                              : dcid->token,
      conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

static int conn_call_activate_dcid(ngtcp2_conn *conn, const ngtcp2_dcid *dcid) {
  return conn_call_dcid_status(conn, NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE,
                               dcid);
}

static int conn_call_deactivate_dcid(ngtcp2_conn *conn,
                                     const ngtcp2_dcid *dcid) {
  return conn_call_dcid_status(
      conn, NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE, dcid);
}

static void conn_call_delete_crypto_aead_ctx(ngtcp2_conn *conn,
                                             ngtcp2_crypto_aead_ctx *aead_ctx) {
  if (!aead_ctx->native_handle) {
    return;
  }

  assert(conn->callbacks.delete_crypto_aead_ctx);

  conn->callbacks.delete_crypto_aead_ctx(conn, aead_ctx, conn->user_data);
}

static void
conn_call_delete_crypto_cipher_ctx(ngtcp2_conn *conn,
                                   ngtcp2_crypto_cipher_ctx *cipher_ctx) {
  if (!cipher_ctx->native_handle) {
    return;
  }

  assert(conn->callbacks.delete_crypto_cipher_ctx);

  conn->callbacks.delete_crypto_cipher_ctx(conn, cipher_ctx, conn->user_data);
}

static int crypto_offset_less(const ngtcp2_ksl_key *lhs,
                              const ngtcp2_ksl_key *rhs) {
  return *(int64_t *)lhs < *(int64_t *)rhs;
}

static int pktns_init(ngtcp2_pktns *pktns, ngtcp2_pktns_id pktns_id,
                      ngtcp2_rst *rst, ngtcp2_cc *cc, ngtcp2_log *log,
                      ngtcp2_qlog *qlog, const ngtcp2_mem *mem) {
  int rv;

  memset(pktns, 0, sizeof(*pktns));

  rv = ngtcp2_gaptr_init(&pktns->rx.pngap, mem);
  if (rv != 0) {
    return rv;
  }

  pktns->tx.last_pkt_num = -1;
  pktns->rx.max_pkt_num = -1;
  pktns->rx.max_ack_eliciting_pkt_num = -1;

  rv = ngtcp2_acktr_init(&pktns->acktr, log, mem);
  if (rv != 0) {
    goto fail_acktr_init;
  }

  rv = ngtcp2_strm_init(&pktns->crypto.strm, 0, NGTCP2_STRM_FLAG_NONE, 0, 0,
                        NULL, mem);
  if (rv != 0) {
    goto fail_crypto_init;
  }

  rv = ngtcp2_ksl_init(&pktns->crypto.tx.frq, crypto_offset_less,
                       sizeof(uint64_t), mem);
  if (rv != 0) {
    goto fail_tx_frq_init;
  }

  ngtcp2_rtb_init(&pktns->rtb, pktns_id, &pktns->crypto.strm, rst, cc, log,
                  qlog, mem);

  return 0;

fail_tx_frq_init:
  ngtcp2_strm_free(&pktns->crypto.strm);
fail_crypto_init:
  ngtcp2_acktr_free(&pktns->acktr);
fail_acktr_init:
  ngtcp2_gaptr_free(&pktns->rx.pngap);

  return rv;
}

static int pktns_new(ngtcp2_pktns **ppktns, ngtcp2_pktns_id pktns_id,
                     ngtcp2_rst *rst, ngtcp2_cc *cc, ngtcp2_log *log,
                     ngtcp2_qlog *qlog, const ngtcp2_mem *mem) {
  int rv;

  *ppktns = ngtcp2_mem_malloc(mem, sizeof(ngtcp2_pktns));
  if (*ppktns == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  rv = pktns_init(*ppktns, pktns_id, rst, cc, log, qlog, mem);
  if (rv != 0) {
    ngtcp2_mem_free(mem, *ppktns);
  }

  return rv;
}

static int cycle_less(const ngtcp2_pq_entry *lhs, const ngtcp2_pq_entry *rhs) {
  ngtcp2_strm *ls = ngtcp2_struct_of(lhs, ngtcp2_strm, pe);
  ngtcp2_strm *rs = ngtcp2_struct_of(rhs, ngtcp2_strm, pe);

  if (ls->cycle == rs->cycle) {
    return ls->stream_id < rs->stream_id;
  }

  return rs->cycle - ls->cycle <= 1;
}

static void delete_buffed_pkts(ngtcp2_pkt_chain *pc, const ngtcp2_mem *mem) {
  ngtcp2_pkt_chain *next;

  for (; pc;) {
    next = pc->next;
    ngtcp2_pkt_chain_del(pc, mem);
    pc = next;
  }
}

static void pktns_free(ngtcp2_pktns *pktns, const ngtcp2_mem *mem) {
  ngtcp2_frame_chain *frc;
  ngtcp2_ksl_it it;

  delete_buffed_pkts(pktns->rx.buffed_pkts, mem);

  ngtcp2_frame_chain_list_del(pktns->tx.frq, mem);

  ngtcp2_crypto_km_del(pktns->crypto.rx.ckm, mem);
  ngtcp2_crypto_km_del(pktns->crypto.tx.ckm, mem);

  for (it = ngtcp2_ksl_begin(&pktns->crypto.tx.frq); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    frc = ngtcp2_ksl_it_get(&it);
    ngtcp2_frame_chain_del(frc, mem);
  }

  ngtcp2_ksl_free(&pktns->crypto.tx.frq);
  ngtcp2_rtb_free(&pktns->rtb);
  ngtcp2_strm_free(&pktns->crypto.strm);
  ngtcp2_acktr_free(&pktns->acktr);
  ngtcp2_gaptr_free(&pktns->rx.pngap);
}

static void pktns_del(ngtcp2_pktns *pktns, const ngtcp2_mem *mem) {
  if (pktns == NULL) {
    return;
  }

  pktns_free(pktns, mem);

  ngtcp2_mem_free(mem, pktns);
}

static void cc_del(ngtcp2_cc *cc, ngtcp2_cc_algo cc_algo,
                   const ngtcp2_mem *mem) {
  switch (cc_algo) {
  case NGTCP2_CC_ALGO_RENO:
    ngtcp2_cc_reno_cc_free(cc, mem);
    break;
  case NGTCP2_CC_ALGO_CUBIC:
    ngtcp2_cc_cubic_cc_free(cc, mem);
    break;
  default:
    break;
  }
}

static int cid_less(const ngtcp2_ksl_key *lhs, const ngtcp2_ksl_key *rhs) {
  return ngtcp2_cid_less(lhs, rhs);
}

static int ts_retired_less(const ngtcp2_pq_entry *lhs,
                           const ngtcp2_pq_entry *rhs) {
  const ngtcp2_scid *a = ngtcp2_struct_of(lhs, ngtcp2_scid, pe);
  const ngtcp2_scid *b = ngtcp2_struct_of(rhs, ngtcp2_scid, pe);

  return a->ts_retired < b->ts_retired;
}

/*
 * conn_reset_conn_stat_cc resets congestion state in |cstat|.
 */
static void conn_reset_conn_stat_cc(ngtcp2_conn *conn,
                                    ngtcp2_conn_stat *cstat) {
  cstat->latest_rtt = 0;
  cstat->min_rtt = UINT64_MAX;
  cstat->smoothed_rtt = conn->local.settings.initial_rtt;
  cstat->rttvar = conn->local.settings.initial_rtt / 2;
  cstat->first_rtt_sample_ts = UINT64_MAX;
  cstat->pto_count = 0;
  cstat->loss_detection_timer = UINT64_MAX;
  cstat->cwnd =
      ngtcp2_cc_compute_initcwnd(conn->local.settings.max_udp_payload_size);
  cstat->ssthresh = UINT64_MAX;
  cstat->congestion_recovery_start_ts = UINT64_MAX;
  cstat->bytes_in_flight = 0;
  cstat->delivery_rate_sec = 0;
}

/*
 * reset_conn_stat_recovery resets the fields related to the recovery
 * function
 */
static void reset_conn_stat_recovery(ngtcp2_conn_stat *cstat) {
  // Initializes them with UINT64_MAX.
  memset(cstat->loss_time, 0xff, sizeof(cstat->loss_time));
  memset(cstat->last_tx_pkt_ts, 0xff, sizeof(cstat->last_tx_pkt_ts));
}

/*
 * conn_reset_conn_stat resets |cstat|.  The following fields are not
 * reset: initial_rtt and max_udp_payload_size.
 */
static void conn_reset_conn_stat(ngtcp2_conn *conn, ngtcp2_conn_stat *cstat) {
  conn_reset_conn_stat_cc(conn, cstat);
  reset_conn_stat_recovery(cstat);
}

static void delete_scid(ngtcp2_ksl *scids, const ngtcp2_mem *mem) {
  ngtcp2_ksl_it it;

  for (it = ngtcp2_ksl_begin(scids); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    ngtcp2_mem_free(mem, ngtcp2_ksl_it_get(&it));
  }
}

/*
 * compute_pto computes PTO.
 */
static ngtcp2_duration compute_pto(ngtcp2_duration smoothed_rtt,
                                   ngtcp2_duration rttvar,
                                   ngtcp2_duration max_ack_delay) {
  ngtcp2_duration var = ngtcp2_max(4 * rttvar, NGTCP2_GRANULARITY);
  return smoothed_rtt + var + max_ack_delay;
}

/*
 * conn_compute_initial_pto computes PTO using the initial RTT.
 */
static ngtcp2_duration conn_compute_initial_pto(ngtcp2_conn *conn,
                                                ngtcp2_pktns *pktns) {
  ngtcp2_duration initial_rtt = conn->local.settings.initial_rtt;
  ngtcp2_duration max_ack_delay =
      pktns->rtb.pktns_id == NGTCP2_PKTNS_ID_APPLICATION
          ? conn->remote.transport_params.max_ack_delay
          : 0;
  return compute_pto(initial_rtt, initial_rtt / 2, max_ack_delay);
}

/*
 * conn_compute_pto computes the current PTO.
 */
static ngtcp2_duration conn_compute_pto(ngtcp2_conn *conn,
                                        ngtcp2_pktns *pktns) {
  ngtcp2_conn_stat *cstat = &conn->cstat;
  ngtcp2_duration max_ack_delay =
      pktns->rtb.pktns_id == NGTCP2_PKTNS_ID_APPLICATION
          ? conn->remote.transport_params.max_ack_delay
          : 0;
  return compute_pto(cstat->smoothed_rtt, cstat->rttvar, max_ack_delay);
}

static void conn_handle_tx_ecn(ngtcp2_conn *conn, ngtcp2_pkt_info *pi,
                               uint8_t *prtb_entry_flags, ngtcp2_pktns *pktns,
                               const ngtcp2_pkt_hd *hd, ngtcp2_tstamp ts) {
  assert(pi);

  if (pi->ecn != NGTCP2_ECN_NOT_ECT) {
    /* We have already made a transition of validation state and
      deceided to send UDP datagram with ECN bit set.  Coalesced QUIC
      packets also bear ECN bits set. */
    if (pktns->tx.ecn.start_pkt_num == INT64_MAX) {
      pktns->tx.ecn.start_pkt_num = hd->pkt_num;
    }

    ++pktns->tx.ecn.validation_pkt_sent;

    if (prtb_entry_flags) {
      *prtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ECN;
    }

    ++pktns->tx.ecn.ect0;

    return;
  }

  switch (conn->tx.ecn.state) {
  case NGTCP2_ECN_STATE_TESTING:
    if (conn->tx.ecn.validation_start_ts == UINT64_MAX) {
      assert(0 == pktns->tx.ecn.validation_pkt_sent);
      assert(0 == pktns->tx.ecn.validation_pkt_lost);

      conn->tx.ecn.validation_start_ts = ts;
    } else if (ts - conn->tx.ecn.validation_start_ts >=
               3 * conn_compute_pto(conn, pktns)) {
      conn->tx.ecn.state = NGTCP2_ECN_STATE_UNKNOWN;
      break;
    }

    if (pktns->tx.ecn.start_pkt_num == INT64_MAX) {
      pktns->tx.ecn.start_pkt_num = hd->pkt_num;
    }

    ++pktns->tx.ecn.validation_pkt_sent;

    if (++conn->tx.ecn.dgram_sent == NGTCP2_ECN_MAX_NUM_VALIDATION_PKTS) {
      conn->tx.ecn.state = NGTCP2_ECN_STATE_UNKNOWN;
    }
    /* fall through */
  case NGTCP2_ECN_STATE_CAPABLE:
    /* pi is provided per UDP datagram. */
    assert(NGTCP2_ECN_NOT_ECT == pi->ecn);

    pi->ecn = NGTCP2_ECN_ECT_0;

    if (prtb_entry_flags) {
      *prtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ECN;
    }

    ++pktns->tx.ecn.ect0;
    break;
  case NGTCP2_ECN_STATE_UNKNOWN:
  case NGTCP2_ECN_STATE_FAILED:
    break;
  default:
    assert(0);
  }
}

static void conn_reset_ecn_validation_state(ngtcp2_conn *conn) {
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;
  ngtcp2_pktns *pktns = &conn->pktns;

  conn->tx.ecn.state = NGTCP2_ECN_STATE_TESTING;
  conn->tx.ecn.validation_start_ts = UINT64_MAX;
  conn->tx.ecn.dgram_sent = 0;

  if (in_pktns) {
    in_pktns->tx.ecn.start_pkt_num = INT64_MAX;
    in_pktns->tx.ecn.validation_pkt_sent = 0;
    in_pktns->tx.ecn.validation_pkt_lost = 0;
  }

  if (hs_pktns) {
    hs_pktns->tx.ecn.start_pkt_num = INT64_MAX;
    hs_pktns->tx.ecn.validation_pkt_sent = 0;
    hs_pktns->tx.ecn.validation_pkt_lost = 0;
  }

  pktns->tx.ecn.start_pkt_num = INT64_MAX;
  pktns->tx.ecn.validation_pkt_sent = 0;
  pktns->tx.ecn.validation_pkt_lost = 0;
}

static int conn_new(ngtcp2_conn **pconn, const ngtcp2_cid *dcid,
                    const ngtcp2_cid *scid, const ngtcp2_path *path,
                    uint32_t version, const ngtcp2_callbacks *callbacks,
                    const ngtcp2_settings *settings,
                    const ngtcp2_transport_params *params,
                    const ngtcp2_mem *mem, void *user_data, int server) {
  int rv;
  ngtcp2_scid *scident;
  uint8_t *buf;

  assert(settings->max_window <= NGTCP2_MAX_VARINT);
  assert(settings->max_stream_window <= NGTCP2_MAX_VARINT);
  assert(params->active_connection_id_limit <= NGTCP2_MAX_DCID_POOL_SIZE);
  assert(params->initial_max_data <= NGTCP2_MAX_VARINT);
  assert(params->initial_max_stream_data_bidi_local <= NGTCP2_MAX_VARINT);
  assert(params->initial_max_stream_data_bidi_remote <= NGTCP2_MAX_VARINT);
  assert(params->initial_max_stream_data_uni <= NGTCP2_MAX_VARINT);

  if (mem == NULL) {
    mem = ngtcp2_mem_default();
  }

  *pconn = ngtcp2_mem_calloc(mem, 1, sizeof(ngtcp2_conn));
  if (*pconn == NULL) {
    rv = NGTCP2_ERR_NOMEM;
    goto fail_conn;
  }

  rv = ngtcp2_ringbuf_init(&(*pconn)->dcid.bound,
                           NGTCP2_MAX_BOUND_DCID_POOL_SIZE, sizeof(ngtcp2_dcid),
                           mem);
  if (rv != 0) {
    goto fail_dcid_bound_init;
  }

  rv = ngtcp2_ringbuf_init(&(*pconn)->dcid.unused, NGTCP2_MAX_DCID_POOL_SIZE,
                           sizeof(ngtcp2_dcid), mem);
  if (rv != 0) {
    goto fail_dcid_unused_init;
  }

  rv =
      ngtcp2_ringbuf_init(&(*pconn)->dcid.retired, NGTCP2_MAX_DCID_RETIRED_SIZE,
                          sizeof(ngtcp2_dcid), mem);
  if (rv != 0) {
    goto fail_dcid_retired_init;
  }

  rv = ngtcp2_gaptr_init(&(*pconn)->dcid.seqgap, mem);
  if (rv != 0) {
    goto fail_seqgap_init;
  }

  rv = ngtcp2_ksl_init(&(*pconn)->scid.set, cid_less, sizeof(ngtcp2_cid), mem);
  if (rv != 0) {
    goto fail_scid_set_init;
  }

  ngtcp2_pq_init(&(*pconn)->scid.used, ts_retired_less, mem);

  rv = ngtcp2_map_init(&(*pconn)->strms, mem);
  if (rv != 0) {
    goto fail_strms_init;
  }

  ngtcp2_pq_init(&(*pconn)->tx.strmq, cycle_less, mem);

  rv = ngtcp2_idtr_init(&(*pconn)->remote.bidi.idtr, !server, mem);
  if (rv != 0) {
    goto fail_remote_bidi_idtr_init;
  }

  rv = ngtcp2_idtr_init(&(*pconn)->remote.uni.idtr, !server, mem);
  if (rv != 0) {
    goto fail_remote_uni_idtr_init;
  }

  rv = ngtcp2_ringbuf_init(&(*pconn)->rx.path_challenge, 4,
                           sizeof(ngtcp2_path_challenge_entry), mem);
  if (rv != 0) {
    goto fail_rx_path_challenge_init;
  }

  ngtcp2_log_init(&(*pconn)->log, scid, settings->log_printf,
                  settings->initial_ts, user_data);
  ngtcp2_qlog_init(&(*pconn)->qlog, settings->qlog.write, settings->initial_ts,
                   user_data);
  if ((*pconn)->qlog.write) {
    buf = ngtcp2_mem_malloc(mem, NGTCP2_QLOG_BUFLEN);
    if (buf == NULL) {
      goto fail_qlog_buf;
    }
    ngtcp2_buf_init(&(*pconn)->qlog.buf, buf, NGTCP2_QLOG_BUFLEN);
  }

  (*pconn)->local.settings = *settings;
  (*pconn)->local.transport_params = *params;

  if (settings->token.len) {
    buf = ngtcp2_mem_malloc(mem, settings->token.len);
    if (buf == NULL) {
      goto fail_token;
    }
    memcpy(buf, settings->token.base, settings->token.len);
    (*pconn)->local.settings.token.base = buf;
  } else {
    (*pconn)->local.settings.token.base = NULL;
    (*pconn)->local.settings.token.len = 0;
  }

  if (settings->max_udp_payload_size == 0) {
    (*pconn)->local.settings.max_udp_payload_size = NGTCP2_DEFAULT_MAX_PKTLEN;
  }

  conn_reset_conn_stat(*pconn, &(*pconn)->cstat);
  (*pconn)->cstat.initial_rtt = settings->initial_rtt;
  (*pconn)->cstat.max_udp_payload_size =
      (*pconn)->local.settings.max_udp_payload_size;

  ngtcp2_rst_init(&(*pconn)->rst);

  (*pconn)->cc_algo = settings->cc_algo;

  switch (settings->cc_algo) {
  case NGTCP2_CC_ALGO_RENO:
    rv = ngtcp2_cc_reno_cc_init(&(*pconn)->cc, &(*pconn)->log, mem);
    if (rv != 0) {
      goto fail_cc_init;
    }
    break;
  case NGTCP2_CC_ALGO_CUBIC:
    rv = ngtcp2_cc_cubic_cc_init(&(*pconn)->cc, &(*pconn)->log, mem);
    if (rv != 0) {
      goto fail_cc_init;
    }
    break;
  case NGTCP2_CC_ALGO_CUSTOM:
    assert(settings->cc);
    (*pconn)->cc = *settings->cc;
    (*pconn)->cc.ccb->log = &(*pconn)->log;
    break;
  default:
    assert(0);
  }

  rv = pktns_new(&(*pconn)->in_pktns, NGTCP2_PKTNS_ID_INITIAL, &(*pconn)->rst,
                 &(*pconn)->cc, &(*pconn)->log, &(*pconn)->qlog, mem);
  if (rv != 0) {
    goto fail_in_pktns_init;
  }

  rv = pktns_new(&(*pconn)->hs_pktns, NGTCP2_PKTNS_ID_HANDSHAKE, &(*pconn)->rst,
                 &(*pconn)->cc, &(*pconn)->log, &(*pconn)->qlog, mem);
  if (rv != 0) {
    goto fail_hs_pktns_init;
  }

  rv = pktns_init(&(*pconn)->pktns, NGTCP2_PKTNS_ID_APPLICATION, &(*pconn)->rst,
                  &(*pconn)->cc, &(*pconn)->log, &(*pconn)->qlog, mem);
  if (rv != 0) {
    goto fail_pktns_init;
  }

  scident = ngtcp2_mem_malloc(mem, sizeof(*scident));
  if (scident == NULL) {
    rv = NGTCP2_ERR_NOMEM;
    goto fail_scident;
  }

  /* Set stateless reset token later if it is available in the local
     transport parameters */
  ngtcp2_scid_init(scident, 0, scid, NULL);

  rv = ngtcp2_ksl_insert(&(*pconn)->scid.set, NULL, &scident->cid, scident);
  if (rv != 0) {
    goto fail_scid_set_insert;
  }

  scident = NULL;

  ngtcp2_dcid_init(&(*pconn)->dcid.current, 0, dcid, NULL);
  ngtcp2_path_copy(&(*pconn)->dcid.current.ps.path, path);

  rv = ngtcp2_gaptr_push(&(*pconn)->dcid.seqgap, 0, 1);
  if (rv != 0) {
    goto fail_seqgap_push;
  }

  (*pconn)->server = server;
  (*pconn)->oscid = *scid;
  (*pconn)->callbacks = *callbacks;
  (*pconn)->version = version;
  (*pconn)->mem = mem;
  (*pconn)->user_data = user_data;
  (*pconn)->idle_ts = settings->initial_ts;
  (*pconn)->crypto.key_update.confirmed_ts = UINT64_MAX;
  (*pconn)->tx.last_max_data_ts = UINT64_MAX;
  (*pconn)->early.discard_started_ts = UINT64_MAX;

  conn_reset_ecn_validation_state(*pconn);

  ngtcp2_qlog_start(&(*pconn)->qlog, server ? &settings->qlog.odcid : dcid,
                    server);

  return 0;

fail_seqgap_push:
fail_scid_set_insert:
  ngtcp2_mem_free(mem, scident);
fail_scident:
  ngtcp2_mem_free(mem, (*pconn)->local.settings.token.base);
fail_token:
  pktns_free(&(*pconn)->pktns, mem);
fail_pktns_init:
  pktns_del((*pconn)->hs_pktns, mem);
fail_hs_pktns_init:
  pktns_del((*pconn)->in_pktns, mem);
fail_in_pktns_init:
  cc_del(&(*pconn)->cc, settings->cc_algo, mem);
fail_cc_init:
  ngtcp2_mem_free(mem, (*pconn)->qlog.buf.begin);
fail_qlog_buf:
  ngtcp2_ringbuf_free(&(*pconn)->rx.path_challenge);
fail_rx_path_challenge_init:
  ngtcp2_idtr_free(&(*pconn)->remote.uni.idtr);
fail_remote_uni_idtr_init:
  ngtcp2_idtr_free(&(*pconn)->remote.bidi.idtr);
fail_remote_bidi_idtr_init:
  ngtcp2_map_free(&(*pconn)->strms);
fail_strms_init:
  delete_scid(&(*pconn)->scid.set, mem);
  ngtcp2_ksl_free(&(*pconn)->scid.set);
fail_scid_set_init:
  ngtcp2_gaptr_free(&(*pconn)->dcid.seqgap);
fail_seqgap_init:
  ngtcp2_ringbuf_free(&(*pconn)->dcid.retired);
fail_dcid_retired_init:
  ngtcp2_ringbuf_free(&(*pconn)->dcid.unused);
fail_dcid_unused_init:
  ngtcp2_ringbuf_free(&(*pconn)->dcid.bound);
fail_dcid_bound_init:
  ngtcp2_mem_free(mem, *pconn);
fail_conn:
  return rv;
}

int ngtcp2_conn_client_new(ngtcp2_conn **pconn, const ngtcp2_cid *dcid,
                           const ngtcp2_cid *scid, const ngtcp2_path *path,
                           uint32_t version, const ngtcp2_callbacks *callbacks,
                           const ngtcp2_settings *settings,
                           const ngtcp2_transport_params *params,
                           const ngtcp2_mem *mem, void *user_data) {
  int rv;
  rv = conn_new(pconn, dcid, scid, path, version, callbacks, settings, params,
                mem, user_data, 0);
  if (rv != 0) {
    return rv;
  }
  (*pconn)->rcid = *dcid;
  (*pconn)->state = NGTCP2_CS_CLIENT_INITIAL;
  (*pconn)->local.bidi.next_stream_id = 0;
  (*pconn)->local.uni.next_stream_id = 2;

  rv = ngtcp2_conn_commit_local_transport_params(*pconn);
  if (rv != 0) {
    ngtcp2_conn_del(*pconn);
    return rv;
  }

  return 0;
}

int ngtcp2_conn_server_new(ngtcp2_conn **pconn, const ngtcp2_cid *dcid,
                           const ngtcp2_cid *scid, const ngtcp2_path *path,
                           uint32_t version, const ngtcp2_callbacks *callbacks,
                           const ngtcp2_settings *settings,
                           const ngtcp2_transport_params *params,
                           const ngtcp2_mem *mem, void *user_data) {
  int rv;
  rv = conn_new(pconn, dcid, scid, path, version, callbacks, settings, params,
                mem, user_data, 1);
  if (rv != 0) {
    return rv;
  }
  (*pconn)->state = NGTCP2_CS_SERVER_INITIAL;
  (*pconn)->local.bidi.next_stream_id = 1;
  (*pconn)->local.uni.next_stream_id = 3;

  if ((*pconn)->local.settings.token.len) {
    /* Usage of token lifts amplification limit */
    (*pconn)->dcid.current.flags |= NGTCP2_DCID_FLAG_PATH_VALIDATED;
  }

  return 0;
}

/*
 * conn_fc_credits returns the number of bytes allowed to be sent to
 * the given stream.  Both connection and stream level flow control
 * credits are considered.
 */
static uint64_t conn_fc_credits(ngtcp2_conn *conn, ngtcp2_strm *strm) {
  return ngtcp2_min(strm->tx.max_offset - strm->tx.offset,
                    conn->tx.max_offset - conn->tx.offset);
}

/*
 * conn_enforce_flow_control returns the number of bytes allowed to be
 * sent to the given stream.  |len| might be shorted because of
 * available flow control credits.
 */
static size_t conn_enforce_flow_control(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                        size_t len) {
  uint64_t fc_credits = conn_fc_credits(conn, strm);
  return (size_t)ngtcp2_min((uint64_t)len, fc_credits);
}

static int delete_strms_each(ngtcp2_map_entry *ent, void *ptr) {
  const ngtcp2_mem *mem = ptr;
  ngtcp2_strm *s = ngtcp2_struct_of(ent, ngtcp2_strm, me);

  ngtcp2_strm_free(s);
  ngtcp2_mem_free(mem, s);

  return 0;
}

void ngtcp2_conn_del(ngtcp2_conn *conn) {
  if (conn == NULL) {
    return;
  }

  ngtcp2_qlog_end(&conn->qlog);

  if (conn->early.ckm) {
    conn_call_delete_crypto_aead_ctx(conn, &conn->early.ckm->aead_ctx);
  }
  conn_call_delete_crypto_cipher_ctx(conn, &conn->early.hp_ctx);

  if (conn->crypto.key_update.old_rx_ckm) {
    conn_call_delete_crypto_aead_ctx(
        conn, &conn->crypto.key_update.old_rx_ckm->aead_ctx);
  }
  if (conn->crypto.key_update.new_rx_ckm) {
    conn_call_delete_crypto_aead_ctx(
        conn, &conn->crypto.key_update.new_rx_ckm->aead_ctx);
  }
  if (conn->crypto.key_update.new_tx_ckm) {
    conn_call_delete_crypto_aead_ctx(
        conn, &conn->crypto.key_update.new_tx_ckm->aead_ctx);
  }

  if (conn->pktns.crypto.rx.ckm) {
    conn_call_delete_crypto_aead_ctx(conn,
                                     &conn->pktns.crypto.rx.ckm->aead_ctx);
  }
  conn_call_delete_crypto_cipher_ctx(conn, &conn->pktns.crypto.rx.hp_ctx);

  if (conn->pktns.crypto.tx.ckm) {
    conn_call_delete_crypto_aead_ctx(conn,
                                     &conn->pktns.crypto.tx.ckm->aead_ctx);
  }
  conn_call_delete_crypto_cipher_ctx(conn, &conn->pktns.crypto.tx.hp_ctx);

  if (conn->hs_pktns) {
    if (conn->hs_pktns->crypto.rx.ckm) {
      conn_call_delete_crypto_aead_ctx(
          conn, &conn->hs_pktns->crypto.rx.ckm->aead_ctx);
    }
    conn_call_delete_crypto_cipher_ctx(conn, &conn->hs_pktns->crypto.rx.hp_ctx);

    if (conn->hs_pktns->crypto.tx.ckm) {
      conn_call_delete_crypto_aead_ctx(
          conn, &conn->hs_pktns->crypto.tx.ckm->aead_ctx);
    }
    conn_call_delete_crypto_cipher_ctx(conn, &conn->hs_pktns->crypto.tx.hp_ctx);
  }
  if (conn->in_pktns) {
    if (conn->in_pktns->crypto.rx.ckm) {
      conn_call_delete_crypto_aead_ctx(
          conn, &conn->in_pktns->crypto.rx.ckm->aead_ctx);
    }
    conn_call_delete_crypto_cipher_ctx(conn, &conn->in_pktns->crypto.rx.hp_ctx);

    if (conn->in_pktns->crypto.tx.ckm) {
      conn_call_delete_crypto_aead_ctx(
          conn, &conn->in_pktns->crypto.tx.ckm->aead_ctx);
    }
    conn_call_delete_crypto_cipher_ctx(conn, &conn->in_pktns->crypto.tx.hp_ctx);
  }

  conn_call_delete_crypto_aead_ctx(conn, &conn->crypto.retry_aead_ctx);

  ngtcp2_mem_free(conn->mem, conn->crypto.decrypt_buf.base);
  ngtcp2_mem_free(conn->mem, conn->crypto.decrypt_hp_buf.base);
  ngtcp2_mem_free(conn->mem, conn->local.settings.token.base);

  ngtcp2_crypto_km_del(conn->crypto.key_update.old_rx_ckm, conn->mem);
  ngtcp2_crypto_km_del(conn->crypto.key_update.new_rx_ckm, conn->mem);
  ngtcp2_crypto_km_del(conn->crypto.key_update.new_tx_ckm, conn->mem);
  ngtcp2_crypto_km_del(conn->early.ckm, conn->mem);

  pktns_free(&conn->pktns, conn->mem);
  pktns_del(conn->hs_pktns, conn->mem);
  pktns_del(conn->in_pktns, conn->mem);

  cc_del(&conn->cc, conn->cc_algo, conn->mem);

  ngtcp2_mem_free(conn->mem, conn->qlog.buf.begin);

  ngtcp2_ringbuf_free(&conn->rx.path_challenge);

  ngtcp2_pv_del(conn->pv);

  ngtcp2_idtr_free(&conn->remote.uni.idtr);
  ngtcp2_idtr_free(&conn->remote.bidi.idtr);
  ngtcp2_mem_free(conn->mem, conn->tx.ack);
  ngtcp2_pq_free(&conn->tx.strmq);
  ngtcp2_map_each_free(&conn->strms, delete_strms_each, (void *)conn->mem);
  ngtcp2_map_free(&conn->strms);

  ngtcp2_pq_free(&conn->scid.used);
  delete_scid(&conn->scid.set, conn->mem);
  ngtcp2_ksl_free(&conn->scid.set);
  ngtcp2_gaptr_free(&conn->dcid.seqgap);
  ngtcp2_ringbuf_free(&conn->dcid.retired);
  ngtcp2_ringbuf_free(&conn->dcid.unused);
  ngtcp2_ringbuf_free(&conn->dcid.bound);

  ngtcp2_mem_free(conn->mem, conn);
}

/*
 * conn_ensure_ack_blks makes sure that conn->tx.ack->ack.blks can
 * contain at least |n| additional ngtcp2_ack_blk.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_ensure_ack_blks(ngtcp2_conn *conn, size_t n) {
  ngtcp2_frame *fr;
  size_t max = conn->tx.max_ack_blks;

  if (n <= max) {
    return 0;
  }

  max *= 2;

  assert(max >= n);

  fr = ngtcp2_mem_realloc(conn->mem, conn->tx.ack,
                          sizeof(ngtcp2_ack) + sizeof(ngtcp2_ack_blk) * max);
  if (fr == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  conn->tx.ack = fr;
  conn->tx.max_ack_blks = max;

  return 0;
}

/*
 * conn_compute_ack_delay computes ACK delay for outgoing protected
 * ACK.
 */
static ngtcp2_duration conn_compute_ack_delay(ngtcp2_conn *conn) {
  return ngtcp2_min(conn->local.transport_params.max_ack_delay,
                    conn->cstat.smoothed_rtt / 8);
}

/*
 * conn_create_ack_frame creates ACK frame, and assigns its pointer to
 * |*pfr| if there are any received packets to acknowledge.  If there
 * are no packets to acknowledge, this function returns 0, and |*pfr|
 * is untouched.  The caller is advised to set |*pfr| to NULL before
 * calling this function, and check it after this function returns.
 * If |nodelay| is nonzero, delayed ACK timer is ignored.
 *
 * The memory for ACK frame is dynamically allocated by this function.
 * A caller is responsible to free it.
 *
 * Call ngtcp2_acktr_commit_ack after a created ACK frame is
 * successfully serialized into a packet.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_create_ack_frame(ngtcp2_conn *conn, ngtcp2_frame **pfr,
                                 ngtcp2_pktns *pktns, uint8_t type,
                                 ngtcp2_tstamp ts, ngtcp2_duration ack_delay,
                                 uint64_t ack_delay_exponent) {
  /* TODO Measure an actual size of ACK blocks to find the best
     default value. */
  const size_t initial_max_ack_blks = 8;
  int64_t last_pkt_num;
  ngtcp2_acktr *acktr = &pktns->acktr;
  ngtcp2_ack_blk *blk;
  ngtcp2_ksl_it it;
  ngtcp2_acktr_entry *rpkt;
  ngtcp2_ack *ack;
  size_t blk_idx;
  ngtcp2_tstamp largest_ack_ts;
  int rv;

  if (acktr->flags & NGTCP2_ACKTR_FLAG_IMMEDIATE_ACK) {
    ack_delay = 0;
  }

  if (!ngtcp2_acktr_require_active_ack(acktr, ack_delay, ts)) {
    return 0;
  }

  it = ngtcp2_acktr_get(acktr);
  if (ngtcp2_ksl_it_end(&it)) {
    ngtcp2_acktr_commit_ack(acktr);
    return 0;
  }

  if (conn->tx.ack == NULL) {
    conn->tx.ack = ngtcp2_mem_malloc(
        conn->mem,
        sizeof(ngtcp2_ack) + sizeof(ngtcp2_ack_blk) * initial_max_ack_blks);
    if (conn->tx.ack == NULL) {
      return NGTCP2_ERR_NOMEM;
    }
    conn->tx.max_ack_blks = initial_max_ack_blks;
  }

  ack = &conn->tx.ack->ack;

  if (pktns->rx.ecn.ect0 || pktns->rx.ecn.ect1 || pktns->rx.ecn.ce) {
    ack->type = NGTCP2_FRAME_ACK_ECN;
    ack->ecn.ect0 = pktns->rx.ecn.ect0;
    ack->ecn.ect1 = pktns->rx.ecn.ect1;
    ack->ecn.ce = pktns->rx.ecn.ce;
  } else {
    ack->type = NGTCP2_FRAME_ACK;
  }
  ack->num_blks = 0;

  rpkt = ngtcp2_ksl_it_get(&it);

  if (rpkt->pkt_num == pktns->rx.max_pkt_num) {
    last_pkt_num = rpkt->pkt_num - (int64_t)(rpkt->len - 1);
    largest_ack_ts = rpkt->tstamp;
    ack->largest_ack = rpkt->pkt_num;
    ack->first_ack_blklen = rpkt->len - 1;

    ngtcp2_ksl_it_next(&it);
  } else {
    assert(rpkt->pkt_num < pktns->rx.max_pkt_num);

    last_pkt_num = pktns->rx.max_pkt_num;
    largest_ack_ts = pktns->rx.max_pkt_ts;
    ack->largest_ack = pktns->rx.max_pkt_num;
    ack->first_ack_blklen = 0;
  }

  if (type == NGTCP2_PKT_SHORT) {
    ack->ack_delay_unscaled = ts - largest_ack_ts;
    ack->ack_delay = ack->ack_delay_unscaled / NGTCP2_MICROSECONDS /
                     (1ULL << ack_delay_exponent);
  } else {
    ack->ack_delay_unscaled = 0;
    ack->ack_delay = 0;
  }

  for (; !ngtcp2_ksl_it_end(&it); ngtcp2_ksl_it_next(&it)) {
    if (ack->num_blks == NGTCP2_MAX_ACK_BLKS) {
      break;
    }

    rpkt = ngtcp2_ksl_it_get(&it);

    blk_idx = ack->num_blks++;
    rv = conn_ensure_ack_blks(conn, ack->num_blks);
    if (rv != 0) {
      return rv;
    }
    ack = &conn->tx.ack->ack;
    blk = &ack->blks[blk_idx];
    blk->gap = (uint64_t)(last_pkt_num - rpkt->pkt_num - 2);
    blk->blklen = rpkt->len - 1;

    last_pkt_num = rpkt->pkt_num - (int64_t)(rpkt->len - 1);
  }

  /* TODO Just remove entries which cannot fit into a single ACK frame
     for now. */
  if (!ngtcp2_ksl_it_end(&it)) {
    ngtcp2_acktr_forget(acktr, ngtcp2_ksl_it_get(&it));
  }

  *pfr = conn->tx.ack;

  return 0;
}

/*
 * conn_ppe_write_frame writes |fr| to |ppe|.  If |hd_logged| is not
 * NULL and |*hd_logged| is zero, packet header is logged, and 1 is
 * assigned to |*hd_logged|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer is too small.
 */
static int conn_ppe_write_frame_hd_log(ngtcp2_conn *conn, ngtcp2_ppe *ppe,
                                       int *hd_logged, const ngtcp2_pkt_hd *hd,
                                       ngtcp2_frame *fr) {
  int rv;

  rv = ngtcp2_ppe_encode_frame(ppe, fr);
  if (rv != 0) {
    assert(NGTCP2_ERR_NOBUF == rv);
    return rv;
  }

  if (hd_logged && !*hd_logged) {
    *hd_logged = 1;
    ngtcp2_log_tx_pkt_hd(&conn->log, hd);
    ngtcp2_qlog_pkt_sent_start(&conn->qlog);
  }

  ngtcp2_log_tx_fr(&conn->log, hd, fr);
  ngtcp2_qlog_write_frame(&conn->qlog, fr);

  return 0;
}

/*
 * conn_ppe_write_frame writes |fr| to |ppe|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOBUF
 *     Buffer is too small.
 */
static int conn_ppe_write_frame(ngtcp2_conn *conn, ngtcp2_ppe *ppe,
                                const ngtcp2_pkt_hd *hd, ngtcp2_frame *fr) {
  return conn_ppe_write_frame_hd_log(conn, ppe, NULL, hd, fr);
}

/*
 * conn_on_pkt_sent is called when new non-ACK-only packet is sent.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
static int conn_on_pkt_sent(ngtcp2_conn *conn, ngtcp2_rtb *rtb,
                            ngtcp2_rtb_entry *ent) {
  int rv;

  /* This function implements OnPacketSent, but it handles only
     non-ACK-only packet. */
  rv = ngtcp2_rtb_add(rtb, ent, &conn->cstat);
  if (rv != 0) {
    return rv;
  }

  if (ent->flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) {
    conn->cstat.last_tx_pkt_ts[rtb->pktns_id] = ent->ts;
  }

  ngtcp2_conn_set_loss_detection_timer(conn, ent->ts);

  return 0;
}

/*
 * pktns_select_pkt_numlen selects shortest packet number encoding for
 * the next packet number based on the largest acknowledged packet
 * number.  It returns the number of bytes to encode the packet
 * number.
 */
static size_t pktns_select_pkt_numlen(ngtcp2_pktns *pktns) {
  int64_t pkt_num = pktns->tx.last_pkt_num + 1;
  ngtcp2_rtb *rtb = &pktns->rtb;
  int64_t n = pkt_num - rtb->largest_acked_tx_pkt_num;

  if (NGTCP2_MAX_PKT_NUM / 2 <= pkt_num) {
    return 4;
  }

  n = n * 2 + 1;

  if (n > 0xffffff) {
    return 4;
  }
  if (n > 0xffff) {
    return 3;
  }
  if (n > 0xff) {
    return 2;
  }
  return 1;
}

/*
 * conn_cwnd_is_zero returns nonzero if the number of bytes the local
 * endpoint can sent at this time is zero.
 */
static uint64_t conn_cwnd_is_zero(ngtcp2_conn *conn) {
  uint64_t bytes_in_flight = conn->cstat.bytes_in_flight;
  uint64_t cwnd =
      conn->pv && (conn->pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE)
          ? ngtcp2_cc_compute_initcwnd(conn->cstat.max_udp_payload_size)
          : conn->cstat.cwnd;

  return bytes_in_flight >= cwnd;
}

/*
 * conn_retry_early_payloadlen returns the estimated wire length of
 * the first STREAM frame of 0-RTT packet which should be
 * retransmitted due to Retry frame
 */
static size_t conn_retry_early_payloadlen(ngtcp2_conn *conn) {
  ngtcp2_frame_chain *frc;
  ngtcp2_strm *strm;

  if (conn->flags & NGTCP2_CONN_FLAG_EARLY_DATA_REJECTED) {
    return 0;
  }

  for (; !ngtcp2_pq_empty(&conn->tx.strmq);) {
    strm = ngtcp2_conn_tx_strmq_top(conn);
    if (ngtcp2_strm_streamfrq_empty(strm)) {
      ngtcp2_conn_tx_strmq_pop(conn);
      continue;
    }

    frc = ngtcp2_strm_streamfrq_top(strm);
    return ngtcp2_vec_len(frc->fr.stream.data, frc->fr.stream.datacnt) +
           NGTCP2_STREAM_OVERHEAD;
  }

  return 0;
}

static void conn_cryptofrq_clear(ngtcp2_conn *conn, ngtcp2_pktns *pktns) {
  ngtcp2_frame_chain *frc;
  ngtcp2_ksl_it it;

  for (it = ngtcp2_ksl_begin(&pktns->crypto.tx.frq); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    frc = ngtcp2_ksl_it_get(&it);
    ngtcp2_frame_chain_del(frc, conn->mem);
  }
  ngtcp2_ksl_clear(&pktns->crypto.tx.frq);
}

/*
 * conn_cryptofrq_unacked_offset returns the CRYPTO frame offset by
 * taking into account acknowledged offset.  If there is no data to
 * send, this function returns (uint64_t)-1.
 */
static uint64_t conn_cryptofrq_unacked_offset(ngtcp2_conn *conn,
                                              ngtcp2_pktns *pktns) {
  ngtcp2_frame_chain *frc;
  ngtcp2_crypto *fr;
  ngtcp2_range gap;
  ngtcp2_rtb *rtb = &pktns->rtb;
  ngtcp2_ksl_it it;
  size_t datalen;

  (void)conn;

  for (it = ngtcp2_ksl_begin(&pktns->crypto.tx.frq); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    frc = ngtcp2_ksl_it_get(&it);
    fr = &frc->fr.crypto;

    gap = ngtcp2_strm_get_unacked_range_after(rtb->crypto, fr->offset);

    datalen = ngtcp2_vec_len(fr->data, fr->datacnt);

    if (gap.begin <= fr->offset) {
      return fr->offset;
    }
    if (gap.begin < fr->offset + datalen) {
      return gap.begin;
    }
  }

  return (uint64_t)-1;
}

static int conn_cryptofrq_unacked_pop(ngtcp2_conn *conn, ngtcp2_pktns *pktns,
                                      ngtcp2_frame_chain **pfrc) {
  ngtcp2_frame_chain *frc, *nfrc;
  ngtcp2_crypto *fr, *nfr;
  uint64_t offset, end_offset;
  size_t idx, end_idx;
  uint64_t base_offset, end_base_offset;
  ngtcp2_range gap;
  ngtcp2_rtb *rtb = &pktns->rtb;
  ngtcp2_vec *v;
  int rv;
  ngtcp2_ksl_it it;

  *pfrc = NULL;

  for (it = ngtcp2_ksl_begin(&pktns->crypto.tx.frq); !ngtcp2_ksl_it_end(&it);) {
    frc = ngtcp2_ksl_it_get(&it);
    fr = &frc->fr.crypto;

    ngtcp2_ksl_remove(&pktns->crypto.tx.frq, &it, &fr->offset);

    idx = 0;
    offset = fr->offset;
    base_offset = 0;

    gap = ngtcp2_strm_get_unacked_range_after(rtb->crypto, offset);
    if (gap.begin < offset) {
      gap.begin = offset;
    }

    for (; idx < fr->datacnt && offset < gap.begin; ++idx) {
      v = &fr->data[idx];
      if (offset + v->len > gap.begin) {
        base_offset = gap.begin - offset;
        break;
      }

      offset += v->len;
    }

    if (idx == fr->datacnt) {
      ngtcp2_frame_chain_del(frc, conn->mem);
      continue;
    }

    assert(gap.begin == offset + base_offset);

    end_idx = idx;
    end_offset = offset;
    end_base_offset = 0;

    for (; end_idx < fr->datacnt; ++end_idx) {
      v = &fr->data[end_idx];
      if (end_offset + v->len > gap.end) {
        end_base_offset = gap.end - end_offset;
        break;
      }

      end_offset += v->len;
    }

    if (fr->offset == offset && base_offset == 0 && fr->datacnt == end_idx) {
      *pfrc = frc;
      return 0;
    }

    if (fr->datacnt == end_idx) {
      memmove(fr->data, fr->data + idx, sizeof(fr->data[0]) * (end_idx - idx));

      assert(fr->data[0].len > base_offset);

      fr->offset = offset + base_offset;
      fr->datacnt = end_idx - idx;
      fr->data[0].base += base_offset;
      fr->data[0].len -= (size_t)base_offset;

      *pfrc = frc;
      return 0;
    }

    rv = ngtcp2_frame_chain_crypto_datacnt_new(&nfrc, fr->datacnt - end_idx,
                                               conn->mem);
    if (rv != 0) {
      ngtcp2_frame_chain_del(frc, conn->mem);
      return rv;
    }

    nfr = &nfrc->fr.crypto;
    nfr->type = NGTCP2_FRAME_CRYPTO;
    memcpy(nfr->data, fr->data + end_idx,
           sizeof(nfr->data[0]) * (fr->datacnt - end_idx));

    assert(nfr->data[0].len > end_base_offset);

    nfr->offset = end_offset + end_base_offset;
    nfr->datacnt = fr->datacnt - end_idx;
    nfr->data[0].base += end_base_offset;
    nfr->data[0].len -= (size_t)end_base_offset;

    rv = ngtcp2_ksl_insert(&pktns->crypto.tx.frq, NULL, &nfr->offset, nfrc);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(nfrc, conn->mem);
      ngtcp2_frame_chain_del(frc, conn->mem);
      return rv;
    }

    if (end_base_offset) {
      ++end_idx;
    }

    memmove(fr->data, fr->data + idx, sizeof(fr->data[0]) * (end_idx - idx));

    assert(fr->data[0].len > base_offset);

    fr->offset = offset + base_offset;
    fr->datacnt = end_idx - idx;
    if (end_base_offset) {
      assert(fr->data[fr->datacnt - 1].len > end_base_offset);
      fr->data[fr->datacnt - 1].len = (size_t)end_base_offset;
    }
    fr->data[0].base += base_offset;
    fr->data[0].len -= (size_t)base_offset;

    *pfrc = frc;
    return 0;
  }

  return 0;
}
static int conn_cryptofrq_pop(ngtcp2_conn *conn, ngtcp2_frame_chain **pfrc,
                              ngtcp2_pktns *pktns, size_t left) {
  ngtcp2_crypto *fr, *nfr;
  ngtcp2_frame_chain *frc, *nfrc;
  int rv;
  size_t nmerged;
  size_t datalen;
  ngtcp2_vec a[NGTCP2_MAX_CRYPTO_DATACNT];
  ngtcp2_vec b[NGTCP2_MAX_CRYPTO_DATACNT];
  size_t acnt, bcnt;
  ngtcp2_ksl_it it;

  rv = conn_cryptofrq_unacked_pop(conn, pktns, &frc);
  if (rv != 0) {
    return rv;
  }
  if (frc == NULL) {
    *pfrc = NULL;
    return 0;
  }

  fr = &frc->fr.crypto;
  datalen = ngtcp2_vec_len(fr->data, fr->datacnt);

  if (datalen > left) {
    ngtcp2_vec_copy(a, fr->data, fr->datacnt);
    acnt = fr->datacnt;

    bcnt = 0;
    ngtcp2_vec_split(a, &acnt, b, &bcnt, left, NGTCP2_MAX_CRYPTO_DATACNT);

    assert(acnt > 0);
    assert(bcnt > 0);

    rv = ngtcp2_frame_chain_crypto_datacnt_new(&nfrc, bcnt, conn->mem);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(frc, conn->mem);
      return rv;
    }

    nfr = &nfrc->fr.crypto;
    nfr->type = NGTCP2_FRAME_CRYPTO;
    nfr->offset = fr->offset + left;
    nfr->datacnt = bcnt;
    ngtcp2_vec_copy(nfr->data, b, bcnt);

    rv = ngtcp2_ksl_insert(&pktns->crypto.tx.frq, NULL, &nfr->offset, nfrc);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(nfrc, conn->mem);
      ngtcp2_frame_chain_del(frc, conn->mem);
      return rv;
    }

    rv = ngtcp2_frame_chain_crypto_datacnt_new(&nfrc, acnt, conn->mem);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(frc, conn->mem);
      return rv;
    }

    nfr = &nfrc->fr.crypto;
    *nfr = *fr;
    nfr->datacnt = acnt;
    ngtcp2_vec_copy(nfr->data, a, acnt);

    ngtcp2_frame_chain_del(frc, conn->mem);

    *pfrc = nfrc;

    return 0;
  }

  left -= datalen;

  ngtcp2_vec_copy(a, fr->data, fr->datacnt);
  acnt = fr->datacnt;

  for (; left && ngtcp2_ksl_len(&pktns->crypto.tx.frq);) {
    it = ngtcp2_ksl_begin(&pktns->crypto.tx.frq);
    nfrc = ngtcp2_ksl_it_get(&it);
    nfr = &nfrc->fr.crypto;

    if (nfr->offset != fr->offset + datalen) {
      assert(fr->offset + datalen < nfr->offset);
      break;
    }

    rv = conn_cryptofrq_unacked_pop(conn, pktns, &nfrc);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_del(frc, conn->mem);
      return rv;
    }
    if (nfrc == NULL) {
      break;
    }

    nfr = &nfrc->fr.crypto;

    nmerged = ngtcp2_vec_merge(a, &acnt, nfr->data, &nfr->datacnt, left,
                               NGTCP2_MAX_CRYPTO_DATACNT);
    if (nmerged == 0) {
      rv = ngtcp2_ksl_insert(&pktns->crypto.tx.frq, NULL, &nfr->offset, nfrc);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        ngtcp2_frame_chain_del(nfrc, conn->mem);
        ngtcp2_frame_chain_del(frc, conn->mem);
        return rv;
      }
      break;
    }

    datalen += nmerged;
    left -= nmerged;

    if (nfr->datacnt == 0) {
      ngtcp2_frame_chain_del(nfrc, conn->mem);
      continue;
    }

    nfr->offset += nmerged;

    rv = ngtcp2_ksl_insert(&pktns->crypto.tx.frq, NULL, &nfr->offset, nfrc);
    if (rv != 0) {
      ngtcp2_frame_chain_del(nfrc, conn->mem);
      ngtcp2_frame_chain_del(frc, conn->mem);
      return rv;
    }

    break;
  }

  if (acnt == fr->datacnt) {
    assert(acnt > 0);
    fr->data[acnt - 1] = a[acnt - 1];

    *pfrc = frc;
    return 0;
  }

  assert(acnt > fr->datacnt);

  rv = ngtcp2_frame_chain_crypto_datacnt_new(&nfrc, acnt, conn->mem);
  if (rv != 0) {
    ngtcp2_frame_chain_del(frc, conn->mem);
    return rv;
  }

  nfr = &nfrc->fr.crypto;
  *nfr = *fr;
  nfr->datacnt = acnt;
  ngtcp2_vec_copy(nfr->data, a, acnt);

  ngtcp2_frame_chain_del(frc, conn->mem);

  *pfrc = nfrc;

  return 0;
}

/*
 * conn_verify_dcid verifies that destination connection ID in |hd| is
 * valid for the connection.  If it is successfully verified and the
 * remote endpoint uses new DCID in the packet, nonzero value is
 * assigned to |*pnew_cid_used| if it is not NULL.  Otherwise 0 is
 * assigned to it.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     |dcid| is not known to the local endpoint.
 */
static int conn_verify_dcid(ngtcp2_conn *conn, int *pnew_cid_used,
                            const ngtcp2_pkt_hd *hd) {
  ngtcp2_ksl_it it;
  ngtcp2_scid *scid;
  int rv;

  it = ngtcp2_ksl_lower_bound(&conn->scid.set, &hd->dcid);
  if (ngtcp2_ksl_it_end(&it)) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  scid = ngtcp2_ksl_it_get(&it);
  if (!ngtcp2_cid_eq(&scid->cid, &hd->dcid)) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (!(scid->flags & NGTCP2_SCID_FLAG_USED)) {
    scid->flags |= NGTCP2_SCID_FLAG_USED;

    if (scid->pe.index == NGTCP2_PQ_BAD_INDEX) {
      rv = ngtcp2_pq_push(&conn->scid.used, &scid->pe);
      if (rv != 0) {
        return rv;
      }
    }

    if (pnew_cid_used) {
      *pnew_cid_used = 1;
    }
  } else if (pnew_cid_used) {
    *pnew_cid_used = 0;
  }

  return 0;
}

/*
 * conn_should_pad_pkt returns nonzero if the packet should be padded.
 * |type| is the type of packet.  |left| is the space left in packet
 * buffer.  |early_datalen| is the number of bytes which will be sent
 * in the next, coalesced 0-RTT packet.
 */
static int conn_should_pad_pkt(ngtcp2_conn *conn, uint8_t type, size_t left,
                               size_t early_datalen, int ack_eliciting) {
  size_t min_payloadlen;

  if (conn->server) {
    if (type != NGTCP2_PKT_INITIAL || !ack_eliciting) {
      return 0;
    }

    if (conn->hs_pktns->crypto.tx.ckm &&
        (conn->hs_pktns->rtb.probe_pkt_left ||
         ngtcp2_ksl_len(&conn->hs_pktns->crypto.tx.frq) ||
         !ngtcp2_acktr_empty(&conn->hs_pktns->acktr))) {
      /* If we have something to send in Handshake packet, then add
         PADDING in Handshake packet. */
      min_payloadlen = 128;
    } else {
      return 1;
    }
  } else {
    if (type == NGTCP2_PKT_HANDSHAKE) {
      return conn->in_pktns != NULL;
    }

    if (conn->hs_pktns->crypto.tx.ckm &&
        (conn->hs_pktns->rtb.probe_pkt_left ||
         ngtcp2_ksl_len(&conn->hs_pktns->crypto.tx.frq) ||
         !ngtcp2_acktr_empty(&conn->hs_pktns->acktr))) {
      /* If we have something to send in Handshake packet, then add
         PADDING in Handshake packet. */
      min_payloadlen = 128;
    } else if (!conn->early.ckm || early_datalen == 0) {
      return 1;
    } else {
      /* If we have something to send in 0RTT packet, then add PADDING
         in 0RTT packet. */
      min_payloadlen = ngtcp2_min(early_datalen, 128);
    }
  }

  return left <
         /* TODO Assuming that pkt_num is encoded in 1 byte. */
         NGTCP2_MIN_LONG_HEADERLEN + conn->dcid.current.cid.datalen +
             conn->oscid.datalen + 1 /* payloadlen bytes - 1 */ +
             min_payloadlen + NGTCP2_MAX_AEAD_OVERHEAD;
}

static void conn_restart_timer_on_write(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  conn->idle_ts = ts;
  conn->flags &= (uint16_t)~NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE;
}

static void conn_restart_timer_on_read(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  conn->idle_ts = ts;
  conn->flags |= NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE;
}

/* NGTCP2_WRITE_PKT_FLAG_NONE indicates that no flag is set. */
#define NGTCP2_WRITE_PKT_FLAG_NONE 0x00
/* NGTCP2_WRITE_PKT_FLAG_REQUIRE_PADDING indicates that packet should
   be padded */
#define NGTCP2_WRITE_PKT_FLAG_REQUIRE_PADDING 0x01
/* NGTCP2_WRITE_PKT_FLAG_MORE indicates that more frames might come
   and it should be encoded into the current packet. */
#define NGTCP2_WRITE_PKT_FLAG_MORE 0x02

/*
 * conn_write_handshake_pkt writes handshake packet in the buffer
 * pointed by |dest| whose length is |destlen|.  |type| specifies long
 * packet type.  It should be either NGTCP2_PKT_INITIAL or
 * NGTCP2_PKT_HANDSHAKE_PKT.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static ngtcp2_ssize
conn_write_handshake_pkt(ngtcp2_conn *conn, ngtcp2_pkt_info *pi, uint8_t *dest,
                         size_t destlen, uint8_t type, uint8_t flags,
                         size_t early_datalen, ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_ppe ppe;
  ngtcp2_pkt_hd hd;
  ngtcp2_frame_chain *frq = NULL, **pfrc = &frq;
  ngtcp2_frame_chain *nfrc;
  ngtcp2_frame *ackfr = NULL, lfr;
  ngtcp2_ssize spktlen;
  ngtcp2_crypto_cc cc;
  ngtcp2_rtb_entry *rtbent;
  ngtcp2_pktns *pktns;
  size_t left;
  uint8_t rtb_entry_flags = NGTCP2_RTB_ENTRY_FLAG_NONE;
  int require_padding = (flags & NGTCP2_WRITE_PKT_FLAG_REQUIRE_PADDING) != 0;
  int pkt_empty = 1;
  int padded = 0;
  int hd_logged = 0;
  uint64_t crypto_offset;
  ngtcp2_ssize num_reclaimed;

  switch (type) {
  case NGTCP2_PKT_INITIAL:
    if (!conn->in_pktns) {
      return 0;
    }
    assert(conn->in_pktns->crypto.tx.ckm);
    pktns = conn->in_pktns;
    break;
  case NGTCP2_PKT_HANDSHAKE:
    if (!conn->hs_pktns || !conn->hs_pktns->crypto.tx.ckm) {
      return 0;
    }
    pktns = conn->hs_pktns;
    break;
  default:
    assert(0);
  }

  cc.aead = pktns->crypto.ctx.aead;
  cc.hp = pktns->crypto.ctx.hp;
  cc.ckm = pktns->crypto.tx.ckm;
  cc.hp_ctx = pktns->crypto.tx.hp_ctx;
  cc.encrypt = conn->callbacks.encrypt;
  cc.hp_mask = conn->callbacks.hp_mask;

  ngtcp2_pkt_hd_init(&hd, NGTCP2_PKT_FLAG_LONG_FORM, type,
                     &conn->dcid.current.cid, &conn->oscid,
                     pktns->tx.last_pkt_num + 1, pktns_select_pkt_numlen(pktns),
                     conn->version, 0);

  if (!conn->server && type == NGTCP2_PKT_INITIAL &&
      conn->local.settings.token.len) {
    hd.token = conn->local.settings.token;
  }

  ngtcp2_ppe_init(&ppe, dest, destlen, &cc);

  rv = ngtcp2_ppe_encode_hd(&ppe, &hd);
  if (rv != 0) {
    assert(NGTCP2_ERR_NOBUF == rv);
    return 0;
  }

  if (!ngtcp2_ppe_ensure_hp_sample(&ppe)) {
    return 0;
  }

  rv = conn_create_ack_frame(conn, &ackfr, pktns, type, ts,
                             /* ack_delay = */ 0,
                             NGTCP2_DEFAULT_ACK_DELAY_EXPONENT);
  if (rv != 0) {
    ngtcp2_frame_chain_list_del(frq, conn->mem);
    return rv;
  }

  if (ackfr) {
    rv = conn_ppe_write_frame_hd_log(conn, &ppe, &hd_logged, &hd, ackfr);
    if (rv != 0) {
      assert(NGTCP2_ERR_NOBUF == rv);
    } else {
      ngtcp2_acktr_commit_ack(&pktns->acktr);
      ngtcp2_acktr_add_ack(&pktns->acktr, hd.pkt_num, ackfr->ack.largest_ack);
      pkt_empty = 0;
    }
  }

  /* Server requires at least NGTCP2_DEFAULT_MAX_PKTLEN bytes in order
     to send ack-eliciting Initial packet. */
  if (!conn->server || type != NGTCP2_PKT_INITIAL ||
      destlen >= NGTCP2_DEFAULT_MAX_PKTLEN) {
  build_pkt:
    for (; ngtcp2_ksl_len(&pktns->crypto.tx.frq);) {
      left = ngtcp2_ppe_left(&ppe);

      crypto_offset = conn_cryptofrq_unacked_offset(conn, pktns);
      if (crypto_offset == (size_t)-1) {
        conn_cryptofrq_clear(conn, pktns);
        break;
      }

      left = ngtcp2_pkt_crypto_max_datalen(crypto_offset, left, left);
      if (left == (size_t)-1) {
        break;
      }

      rv = conn_cryptofrq_pop(conn, &nfrc, pktns, left);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        ngtcp2_frame_chain_list_del(frq, conn->mem);
        return rv;
      }

      if (nfrc == NULL) {
        break;
      }

      rv = conn_ppe_write_frame_hd_log(conn, &ppe, &hd_logged, &hd, &nfrc->fr);
      if (rv != 0) {
        assert(0);
      }

      *pfrc = nfrc;
      pfrc = &(*pfrc)->next;

      pkt_empty = 0;
      rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                         NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;
    }

    if (!(rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) &&
        pktns->rtb.num_retransmittable && pktns->rtb.probe_pkt_left) {
      num_reclaimed = ngtcp2_rtb_reclaim_on_pto(&pktns->rtb, conn, pktns,
                                                pktns->rtb.probe_pkt_left + 1);
      if (num_reclaimed < 0) {
        ngtcp2_frame_chain_list_del(frq, conn->mem);
        return rv;
      }
      if (num_reclaimed) {
        goto build_pkt;
      }
      /* We had pktns->rtb.num_retransmittable > 0 but the contents of
         those packets have been acknowledged (i.e., retransmission in
         another packet).  For server, in this case, we don't have to
         send any probe packet.  Client needs to send probe packets
         until it knows that server has completed address validation or
         handshake has been confirmed. */
      if (pktns->rtb.num_retransmittable == 0 &&
          (conn->server ||
           (conn->flags & (NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED |
                           NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED)))) {
        pktns->rtb.probe_pkt_left = 0;
        ngtcp2_conn_set_loss_detection_timer(conn, ts);
      }
    }

    /* Don't send any PING frame if client Initial has not been
       acknowledged yet. */
    if (!(rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) &&
        pktns->rtb.probe_pkt_left &&
        (type != NGTCP2_PKT_INITIAL ||
         ngtcp2_strm_is_all_tx_data_acked(&pktns->crypto.strm))) {
      lfr.type = NGTCP2_FRAME_PING;

      rv = conn_ppe_write_frame_hd_log(conn, &ppe, &hd_logged, &hd, &lfr);
      if (rv != 0) {
        assert(rv == NGTCP2_ERR_NOBUF);
      } else {
        rtb_entry_flags |=
            NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING | NGTCP2_RTB_ENTRY_FLAG_PROBE;
        pkt_empty = 0;
      }
    }

    if (!pkt_empty) {
      if (!(rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING)) {
        /* The intention of smaller limit is get more chance to measure
           RTT samples in early phase. */
        if (pktns->rtb.probe_pkt_left || pktns->tx.num_non_ack_pkt >= 1) {
          lfr.type = NGTCP2_FRAME_PING;

          rv = conn_ppe_write_frame_hd_log(conn, &ppe, &hd_logged, &hd, &lfr);
          if (rv != 0) {
            assert(rv == NGTCP2_ERR_NOBUF);
          } else {
            rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING;
            pktns->tx.num_non_ack_pkt = 0;
          }
        } else {
          ++pktns->tx.num_non_ack_pkt;
        }
      } else {
        pktns->tx.num_non_ack_pkt = 0;
      }
    }
  }

  if (pkt_empty) {
    return 0;
  }

  /* If we cannot write another packet, then we need to add padding to
     Initial here. */
  if (require_padding ||
      conn_should_pad_pkt(
          conn, type, ngtcp2_ppe_left(&ppe), early_datalen,
          (rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) != 0)) {
    lfr.type = NGTCP2_FRAME_PADDING;
    lfr.padding.len = ngtcp2_ppe_padding(&ppe);
  } else {
    lfr.type = NGTCP2_FRAME_PADDING;
    lfr.padding.len = ngtcp2_ppe_padding_hp_sample(&ppe);
  }

  if (lfr.padding.len) {
    padded = 1;
    ngtcp2_log_tx_fr(&conn->log, &hd, &lfr);
    ngtcp2_qlog_write_frame(&conn->qlog, &lfr);
  }

  spktlen = ngtcp2_ppe_final(&ppe, NULL);
  if (spktlen < 0) {
    assert(ngtcp2_err_is_fatal((int)spktlen));
    ngtcp2_frame_chain_list_del(frq, conn->mem);
    return spktlen;
  }

  ngtcp2_qlog_pkt_sent_end(&conn->qlog, &hd, (size_t)spktlen);

  if ((rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) || padded) {
    if (pi) {
      conn_handle_tx_ecn(conn, pi, &rtb_entry_flags, pktns, &hd, ts);
    }

    rv = ngtcp2_rtb_entry_new(&rtbent, &hd, frq, ts, (size_t)spktlen,
                              rtb_entry_flags, conn->mem);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_frame_chain_list_del(frq, conn->mem);
      return rv;
    }

    rv = conn_on_pkt_sent(conn, &pktns->rtb, rtbent);
    if (rv != 0) {
      ngtcp2_rtb_entry_del(rtbent, conn->mem);
      return rv;
    }

    if ((rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) &&
        (conn->flags & NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE)) {
      conn_restart_timer_on_write(conn, ts);
    }
  } else if (pi && conn->tx.ecn.state == NGTCP2_ECN_STATE_CAPABLE) {
    conn_handle_tx_ecn(conn, pi, NULL, pktns, &hd, ts);
  }

  if (pktns->rtb.probe_pkt_left &&
      (rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING)) {
    --pktns->rtb.probe_pkt_left;
  }

  conn->dcid.current.bytes_sent += (uint64_t)spktlen;

  ngtcp2_qlog_metrics_updated(&conn->qlog, &conn->cstat);

  ++pktns->tx.last_pkt_num;

  return spktlen;
}

/*
 * conn_write_ack_pkt writes QUIC packet for type |type| which only
 * includes ACK frame in the buffer pointed by |dest| whose length is
 * |destlen|.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static ngtcp2_ssize conn_write_ack_pkt(ngtcp2_conn *conn, ngtcp2_pkt_info *pi,
                                       uint8_t *dest, size_t destlen,
                                       uint8_t type, ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_frame *ackfr;
  ngtcp2_pktns *pktns;
  ngtcp2_duration ack_delay;
  uint64_t ack_delay_exponent;
  ngtcp2_ssize spktlen;

  assert(!(conn->flags & NGTCP2_CONN_FLAG_PPE_PENDING));

  switch (type) {
  case NGTCP2_PKT_INITIAL:
    assert(conn->server);
    pktns = conn->in_pktns;
    ack_delay = 0;
    ack_delay_exponent = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT;
    break;
  case NGTCP2_PKT_HANDSHAKE:
    pktns = conn->hs_pktns;
    ack_delay = 0;
    ack_delay_exponent = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT;
    break;
  case NGTCP2_PKT_SHORT:
    pktns = &conn->pktns;
    ack_delay = conn_compute_ack_delay(conn);
    ack_delay_exponent = conn->local.transport_params.ack_delay_exponent;
    break;
  default:
    assert(0);
  }

  if (!pktns->crypto.tx.ckm) {
    return 0;
  }

  ackfr = NULL;
  rv = conn_create_ack_frame(conn, &ackfr, pktns, type, ts, ack_delay,
                             ack_delay_exponent);
  if (rv != 0) {
    return rv;
  }

  if (!ackfr) {
    return 0;
  }

  spktlen = ngtcp2_conn_write_single_frame_pkt(
      conn, pi, dest, destlen, type, &conn->dcid.current.cid, ackfr,
      NGTCP2_RTB_ENTRY_FLAG_NONE, NULL, ts);

  if (spktlen <= 0) {
    return spktlen;
  }

  conn->dcid.current.bytes_sent += (uint64_t)spktlen;

  return spktlen;
}

static void conn_discard_pktns(ngtcp2_conn *conn, ngtcp2_pktns **ppktns,
                               ngtcp2_tstamp ts) {
  ngtcp2_pktns *pktns = *ppktns;
  uint64_t bytes_in_flight;

  bytes_in_flight = pktns->rtb.cc_bytes_in_flight;

  assert(conn->cstat.bytes_in_flight >= bytes_in_flight);

  conn->cstat.bytes_in_flight -= bytes_in_flight;
  conn->cstat.pto_count = 0;
  conn->cstat.last_tx_pkt_ts[pktns->rtb.pktns_id] = UINT64_MAX;
  conn->cstat.loss_time[pktns->rtb.pktns_id] = UINT64_MAX;

  conn_call_delete_crypto_aead_ctx(conn, &pktns->crypto.rx.ckm->aead_ctx);
  conn_call_delete_crypto_cipher_ctx(conn, &pktns->crypto.rx.hp_ctx);
  conn_call_delete_crypto_aead_ctx(conn, &pktns->crypto.tx.ckm->aead_ctx);
  conn_call_delete_crypto_cipher_ctx(conn, &pktns->crypto.tx.hp_ctx);

  pktns_del(pktns, conn->mem);
  *ppktns = NULL;

  ngtcp2_conn_set_loss_detection_timer(conn, ts);
}

/*
 * conn_discard_initial_state discards state for Initial packet number
 * space.
 */
static void conn_discard_initial_state(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  if (!conn->in_pktns) {
    return;
  }

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                  "discarding Initial packet number space");

  conn_discard_pktns(conn, &conn->in_pktns, ts);
}

/*
 * conn_discard_handshake_state discards state for Handshake packet
 * number space.
 */
static void conn_discard_handshake_state(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  if (!conn->hs_pktns) {
    return;
  }

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                  "discarding Handshake packet number space");

  conn_discard_pktns(conn, &conn->hs_pktns, ts);
}

/*
 * conn_discard_early_key discards early key.
 */
static void conn_discard_early_key(ngtcp2_conn *conn) {
  assert(conn->early.ckm);

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON, "discarding early key");

  conn_call_delete_crypto_aead_ctx(conn, &conn->early.ckm->aead_ctx);
  conn_call_delete_crypto_cipher_ctx(conn, &conn->early.hp_ctx);
  memset(&conn->early.hp_ctx, 0, sizeof(conn->early.hp_ctx));

  ngtcp2_crypto_km_del(conn->early.ckm, conn->mem);
  conn->early.ckm = NULL;
}

/*
 * conn_write_handshake_ack_pkts writes packets which contain ACK
 * frame only.  This function writes at most 2 packets for each
 * Initial and Handshake packet.
 */
static ngtcp2_ssize conn_write_handshake_ack_pkts(ngtcp2_conn *conn,
                                                  ngtcp2_pkt_info *pi,
                                                  uint8_t *dest, size_t destlen,
                                                  ngtcp2_tstamp ts) {
  ngtcp2_ssize res = 0, nwrite = 0;

  /* In the most cases, client sends ACK in conn_write_handshake_pkt.
     This function is only called when it is CWND limited.  It is not
     required for client to send ACK for server Initial.  This is
     because once it gets server Initial, it gets Handshake tx key and
     discards Initial key.  The only good reason to send ACK is give
     server RTT measurement early. */
  if (conn->server && conn->in_pktns) {
    nwrite =
        conn_write_ack_pkt(conn, pi, dest, destlen, NGTCP2_PKT_INITIAL, ts);
    if (nwrite < 0) {
      assert(nwrite != NGTCP2_ERR_NOBUF);
      return nwrite;
    }

    res += nwrite;
    dest += nwrite;
    destlen -= (size_t)nwrite;
  }

  if (conn->hs_pktns->crypto.tx.ckm) {
    nwrite =
        conn_write_ack_pkt(conn, pi, dest, destlen, NGTCP2_PKT_HANDSHAKE, ts);
    if (nwrite < 0) {
      assert(nwrite != NGTCP2_ERR_NOBUF);
      return nwrite;
    }

    res += nwrite;

    if (!conn->server && nwrite) {
      conn_discard_initial_state(conn, ts);
    }
  }

  return res;
}

/*
 * conn_write_client_initial writes Initial packet in the buffer
 * pointed by |dest| whose length is |destlen|.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static ngtcp2_ssize conn_write_client_initial(ngtcp2_conn *conn,
                                              ngtcp2_pkt_info *pi,
                                              uint8_t *dest, size_t destlen,
                                              size_t early_datalen,
                                              ngtcp2_tstamp ts) {
  int rv;

  assert(conn->callbacks.client_initial);

  rv = conn->callbacks.client_initial(conn, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return conn_write_handshake_pkt(conn, pi, dest, destlen, NGTCP2_PKT_INITIAL,
                                  NGTCP2_WRITE_PKT_FLAG_NONE, early_datalen,
                                  ts);
}

/*
 * conn_write_handshake_pkts writes Initial and Handshake packets in
 * the buffer pointed by |dest| whose length is |destlen|.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static ngtcp2_ssize conn_write_handshake_pkts(ngtcp2_conn *conn,
                                              ngtcp2_pkt_info *pi,
                                              uint8_t *dest, size_t destlen,
                                              size_t early_datalen,
                                              ngtcp2_tstamp ts) {
  ngtcp2_ssize nwrite;
  ngtcp2_ssize res = 0;
  int64_t prev_pkt_num = -1;
  ngtcp2_rtb_entry *rtbent;
  uint8_t wflags = NGTCP2_WRITE_PKT_FLAG_NONE;
  ngtcp2_ksl_it it;

  /* As a client, we would like to discard Initial packet number space
     when sending the first Handshake packet.  When sending Handshake
     packet, it should be one of 1) sending ACK, 2) sending PTO probe
     packet, or 3) sending CRYPTO.  If we have pending acknowledgement
     for Initial, then do not discard Initial packet number space.
     Otherwise, if either 1) or 2) is satisfied, discard Initial
     packet number space.  When sending Handshake CRYPTO, it indicates
     that client has received Handshake CRYPTO from server.  Initial
     packet number space is discarded because 1) is met.  If there is
     pending Initial ACK, Initial packet number space is discarded
     after writing the first Handshake packet.
   */
  if (!conn->server && conn->hs_pktns->crypto.tx.ckm && conn->in_pktns &&
      !ngtcp2_acktr_require_active_ack(&conn->in_pktns->acktr,
                                       /* max_ack_delay = */ 0, ts) &&
      (ngtcp2_acktr_require_active_ack(&conn->hs_pktns->acktr,
                                       /* max_ack_delay = */ 0, ts) ||
       conn->hs_pktns->rtb.probe_pkt_left)) {
    /* Discard Initial state here so that Handshake packet is not
       padded. */
    conn_discard_initial_state(conn, ts);
  } else if (conn->in_pktns) {
    if (conn->server) {
      it = ngtcp2_rtb_head(&conn->in_pktns->rtb);
      if (!ngtcp2_ksl_it_end(&it)) {
        rtbent = ngtcp2_ksl_it_get(&it);
        prev_pkt_num = rtbent->hd.pkt_num;
      }
    }

    nwrite =
        conn_write_handshake_pkt(conn, pi, dest, destlen, NGTCP2_PKT_INITIAL,
                                 NGTCP2_WRITE_PKT_FLAG_NONE, early_datalen, ts);
    if (nwrite < 0) {
      assert(nwrite != NGTCP2_ERR_NOBUF);
      return nwrite;
    }

    if (nwrite == 0) {
      if (conn->server && (conn->in_pktns->rtb.probe_pkt_left ||
                           ngtcp2_ksl_len(&conn->in_pktns->crypto.tx.frq))) {
        return 0;
      }
    } else {
      res += nwrite;
      dest += nwrite;
      destlen -= (size_t)nwrite;

      if (conn->server && destlen) {
        it = ngtcp2_rtb_head(&conn->in_pktns->rtb);
        if (!ngtcp2_ksl_it_end(&it)) {
          rtbent = ngtcp2_ksl_it_get(&it);
          if (rtbent->hd.pkt_num != prev_pkt_num &&
              (rtbent->flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING)) {
            /* We might have already added padding to Initial, but in
               that case, we should have destlen == 0 and no Handshake
               packet will be written. */
            wflags |= NGTCP2_WRITE_PKT_FLAG_REQUIRE_PADDING;
          }
        }
      }
    }
  }

  nwrite = conn_write_handshake_pkt(conn, pi, dest, destlen,
                                    NGTCP2_PKT_HANDSHAKE, wflags, 0, ts);
  if (nwrite < 0) {
    assert(nwrite != NGTCP2_ERR_NOBUF);
    return nwrite;
  }

  res += nwrite;

  if (!conn->server && conn->hs_pktns->crypto.tx.ckm && nwrite) {
    /* We don't need to send further Initial packet if we have
       Handshake key and sent something with it.  So discard initial
       state here. */
    conn_discard_initial_state(conn, ts);
  }

  return res;
}

/*
 * conn_initial_stream_rx_offset returns the initial maximum offset of
 * data for a stream denoted by |stream_id|.
 */
static uint64_t conn_initial_stream_rx_offset(ngtcp2_conn *conn,
                                              int64_t stream_id) {
  int local_stream = conn_local_stream(conn, stream_id);

  if (bidi_stream(stream_id)) {
    if (local_stream) {
      return conn->local.transport_params.initial_max_stream_data_bidi_local;
    }
    return conn->local.transport_params.initial_max_stream_data_bidi_remote;
  }

  if (local_stream) {
    return 0;
  }
  return conn->local.transport_params.initial_max_stream_data_uni;
}

/*
 * conn_should_send_max_stream_data returns nonzero if MAX_STREAM_DATA
 * frame should be send for |strm|.
 */
static int conn_should_send_max_stream_data(ngtcp2_conn *conn,
                                            ngtcp2_strm *strm) {
  uint64_t inc = strm->rx.unsent_max_offset - strm->rx.max_offset;
  (void)conn;

  return strm->rx.window < 2 * inc;
}

/*
 * conn_should_send_max_data returns nonzero if MAX_DATA frame should
 * be sent.
 */
static int conn_should_send_max_data(ngtcp2_conn *conn) {
  uint64_t inc = conn->rx.unsent_max_offset - conn->rx.max_offset;

  return conn->rx.window < 2 * inc;
}

/*
 * conn_required_num_new_connection_id returns the number of
 * additional connection ID the local endpoint has to provide to the
 * remote endpoint.
 */
static size_t conn_required_num_new_connection_id(ngtcp2_conn *conn) {
  uint64_t n;
  size_t len = ngtcp2_ksl_len(&conn->scid.set);

  if (len >= NGTCP2_MAX_SCID_POOL_SIZE) {
    return 0;
  }

  assert(conn->remote.transport_params.active_connection_id_limit);

  /* len includes retired CID.  We don't provide extra CID if doing so
     exceeds NGTCP2_MAX_SCID_POOL_SIZE. */

  n = conn->remote.transport_params.active_connection_id_limit +
      conn->scid.num_retired;

  return (size_t)ngtcp2_min(NGTCP2_MAX_SCID_POOL_SIZE, n) - len;
}

/*
 * conn_enqueue_new_connection_id generates additional connection IDs
 * and prepares to send them to the remote endpoint.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_enqueue_new_connection_id(ngtcp2_conn *conn) {
  size_t i, need = conn_required_num_new_connection_id(conn);
  size_t cidlen = conn->oscid.datalen;
  ngtcp2_cid cid;
  uint64_t seq;
  int rv;
  uint8_t token[NGTCP2_STATELESS_RESET_TOKENLEN];
  ngtcp2_frame_chain *nfrc;
  ngtcp2_pktns *pktns = &conn->pktns;
  ngtcp2_scid *scid;
  ngtcp2_ksl_it it;

  for (i = 0; i < need; ++i) {
    rv = conn_call_get_new_connection_id(conn, &cid, token, cidlen);
    if (rv != 0) {
      return rv;
    }

    if (cid.datalen != cidlen) {
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    /* Assert uniqueness */
    it = ngtcp2_ksl_lower_bound(&conn->scid.set, &cid);
    if (!ngtcp2_ksl_it_end(&it) &&
        ngtcp2_cid_eq(ngtcp2_ksl_it_key(&it), &cid)) {
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    seq = ++conn->scid.last_seq;

    scid = ngtcp2_mem_malloc(conn->mem, sizeof(*scid));
    if (scid == NULL) {
      return NGTCP2_ERR_NOMEM;
    }

    ngtcp2_scid_init(scid, seq, &cid, token);

    rv = ngtcp2_ksl_insert(&conn->scid.set, NULL, &scid->cid, scid);
    if (rv != 0) {
      ngtcp2_mem_free(conn->mem, scid);
      return rv;
    }

    rv = ngtcp2_frame_chain_new(&nfrc, conn->mem);
    if (rv != 0) {
      return rv;
    }

    nfrc->fr.type = NGTCP2_FRAME_NEW_CONNECTION_ID;
    nfrc->fr.new_connection_id.seq = seq;
    nfrc->fr.new_connection_id.retire_prior_to = 0;
    nfrc->fr.new_connection_id.cid = cid;
    memcpy(nfrc->fr.new_connection_id.stateless_reset_token, token,
           sizeof(token));
    nfrc->next = pktns->tx.frq;
    pktns->tx.frq = nfrc;
  }

  return 0;
}

/*
 * conn_remove_retired_connection_id removes the already retired
 * connection ID.  It waits PTO before actually removing a connection
 * ID after it receives RETIRE_CONNECTION_ID from peer to catch
 * reordered packets.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_remove_retired_connection_id(ngtcp2_conn *conn,
                                             ngtcp2_duration pto,
                                             ngtcp2_tstamp ts) {
  ngtcp2_duration timeout = pto;
  ngtcp2_scid *scid;
  ngtcp2_dcid *dcid;
  int rv;

  for (; !ngtcp2_pq_empty(&conn->scid.used);) {
    scid = ngtcp2_struct_of(ngtcp2_pq_top(&conn->scid.used), ngtcp2_scid, pe);

    if (scid->ts_retired == UINT64_MAX || scid->ts_retired + timeout >= ts) {
      break;
    }

    assert(scid->flags & NGTCP2_SCID_FLAG_RETIRED);

    rv = conn_call_remove_connection_id(conn, &scid->cid);
    if (rv != 0) {
      return rv;
    }

    ngtcp2_ksl_remove(&conn->scid.set, NULL, &scid->cid);
    ngtcp2_pq_pop(&conn->scid.used);
    ngtcp2_mem_free(conn->mem, scid);

    assert(conn->scid.num_retired);
    --conn->scid.num_retired;
  }

  for (; ngtcp2_ringbuf_len(&conn->dcid.retired);) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.retired, 0);
    if (dcid->ts_retired + timeout >= ts) {
      break;
    }

    rv = conn_call_deactivate_dcid(conn, dcid);
    if (rv != 0) {
      return rv;
    }

    ngtcp2_ringbuf_pop_front(&conn->dcid.retired);
  }

  return 0;
}

/*
 * conn_min_short_pktlen returns the minimum length of Short packet
 * this endpoint sends.
 */
static size_t conn_min_short_pktlen(ngtcp2_conn *conn) {
  return conn->dcid.current.cid.datalen + NGTCP2_MIN_PKT_EXPANDLEN;
}

/*
 * conn_write_pkt writes a protected packet in the buffer pointed by
 * |dest| whose length if |destlen|.  |type| specifies the type of
 * packet.  It can be NGTCP2_PKT_SHORT or NGTCP2_PKT_0RTT.
 *
 * This function can send new stream data.  In order to send stream
 * data, specify the underlying stream and parameters to
 * |vmsg|->stream.  If |vmsg|->stream.fin is set to nonzero, it
 * signals that the given data is the final portion of the stream.
 * |vmsg|->stream.data vector of length |vmsg|->stream.datacnt
 * specifies stream data to send.  The number of bytes sent to the
 * stream is assigned to *|vmsg|->stream.pdatalen.  If 0 length STREAM
 * data is sent, 0 is assigned to it.  The caller should initialize
 * *|vmsg|->stream.pdatalen to -1.
 *
 * If |require_padding| is nonzero, padding bytes are added to occupy
 * the remaining packet payload.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_STREAM_DATA_BLOCKED
 *     Stream data could not be written because of flow control.
 */
static ngtcp2_ssize conn_write_pkt(ngtcp2_conn *conn, ngtcp2_pkt_info *pi,
                                   uint8_t *dest, size_t destlen,
                                   ngtcp2_vmsg *vmsg, uint8_t type,
                                   uint8_t flags, ngtcp2_tstamp ts) {
  int rv = 0;
  ngtcp2_crypto_cc *cc = &conn->pkt.cc;
  ngtcp2_ppe *ppe = &conn->pkt.ppe;
  ngtcp2_pkt_hd *hd = &conn->pkt.hd;
  ngtcp2_frame *ackfr = NULL, lfr;
  ngtcp2_ssize nwrite;
  ngtcp2_frame_chain **pfrc, *nfrc, *frc;
  ngtcp2_rtb_entry *ent;
  ngtcp2_strm *strm;
  int pkt_empty = 1;
  size_t ndatalen = 0;
  int send_stream = 0;
  int stream_blocked = 0;
  int send_datagram = 0;
  ngtcp2_pktns *pktns = &conn->pktns;
  size_t left;
  size_t datalen = 0;
  ngtcp2_vec data[NGTCP2_MAX_STREAM_DATACNT];
  size_t datacnt;
  uint8_t rtb_entry_flags = NGTCP2_RTB_ENTRY_FLAG_NONE;
  int hd_logged = 0;
  ngtcp2_path_challenge_entry *pcent;
  uint8_t hd_flags;
  int require_padding = (flags & NGTCP2_WRITE_PKT_FLAG_REQUIRE_PADDING) != 0;
  int write_more = (flags & NGTCP2_WRITE_PKT_FLAG_MORE) != 0;
  int ppe_pending = (conn->flags & NGTCP2_CONN_FLAG_PPE_PENDING) != 0;
  size_t min_pktlen = conn_min_short_pktlen(conn);
  int padded = 0;
  int credit_expanded = 0;
  ngtcp2_cc_pkt cc_pkt;
  uint64_t crypto_offset;
  uint64_t stream_offset;
  ngtcp2_ssize num_reclaimed;
  int fin;
  uint64_t target_max_data;
  ngtcp2_conn_stat *cstat = &conn->cstat;
  uint64_t delta;

  /* Return 0 if destlen is less than minimum packet length which can
     trigger Stateless Reset */
  if (destlen < min_pktlen) {
    return 0;
  }

  if (vmsg) {
    switch (vmsg->type) {
    case NGTCP2_VMSG_TYPE_STREAM:
      datalen = ngtcp2_vec_len(vmsg->stream.data, vmsg->stream.datacnt);
      ndatalen = conn_enforce_flow_control(conn, vmsg->stream.strm, datalen);
      /* 0 length STREAM frame is allowed */
      if (ndatalen || datalen == 0) {
        send_stream = 1;
      } else {
        stream_blocked = 1;
      }
      break;
    case NGTCP2_VMSG_TYPE_DATAGRAM:
      datalen = ngtcp2_vec_len(vmsg->datagram.data, vmsg->datagram.datacnt);
      send_datagram = 1;
      break;
    default:
      break;
    }
  }

  if (!ppe_pending) {
    switch (type) {
    case NGTCP2_PKT_SHORT:
      hd_flags =
          (pktns->crypto.tx.ckm->flags & NGTCP2_CRYPTO_KM_FLAG_KEY_PHASE_ONE)
              ? NGTCP2_PKT_FLAG_KEY_PHASE
              : NGTCP2_PKT_FLAG_NONE;
      cc->aead = pktns->crypto.ctx.aead;
      cc->hp = pktns->crypto.ctx.hp;
      cc->ckm = pktns->crypto.tx.ckm;
      cc->hp_ctx = pktns->crypto.tx.hp_ctx;

      /* transport parameter is only valid after handshake completion
         which means we don't know how many connection ID that remote
         peer can accept before handshake completion. */
      if (conn->oscid.datalen &&
          (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
        rv = conn_enqueue_new_connection_id(conn);
        if (rv != 0) {
          return rv;
        }
      }

      break;
    case NGTCP2_PKT_0RTT:
      assert(!conn->server);
      if (!conn->early.ckm) {
        return 0;
      }
      hd_flags = NGTCP2_PKT_FLAG_LONG_FORM;
      cc->aead = conn->early.ctx.aead;
      cc->hp = conn->early.ctx.hp;
      cc->ckm = conn->early.ckm;
      cc->hp_ctx = conn->early.hp_ctx;
      break;
    default:
      /* Unreachable */
      assert(0);
    }

    cc->encrypt = conn->callbacks.encrypt;
    cc->hp_mask = conn->callbacks.hp_mask;

    if (conn_should_send_max_data(conn)) {
      rv = ngtcp2_frame_chain_new(&nfrc, conn->mem);
      if (rv != 0) {
        return rv;
      }

      if (conn->local.settings.max_window &&
          conn->tx.last_max_data_ts != UINT64_MAX &&
          ts - conn->tx.last_max_data_ts <
              NGTCP2_FLOW_WINDOW_RTT_FACTOR * cstat->smoothed_rtt &&
          conn->local.settings.max_window > conn->rx.window) {
        target_max_data = NGTCP2_FLOW_WINDOW_SCALING_FACTOR * conn->rx.window;
        if (target_max_data > conn->local.settings.max_window) {
          target_max_data = conn->local.settings.max_window;
        }

        delta = target_max_data - conn->rx.window;
        if (conn->rx.unsent_max_offset + delta > NGTCP2_MAX_VARINT) {
          delta = NGTCP2_MAX_VARINT - conn->rx.unsent_max_offset;
        }

        conn->rx.window = target_max_data;
      } else {
        delta = 0;
      }

      conn->tx.last_max_data_ts = ts;

      nfrc->fr.type = NGTCP2_FRAME_MAX_DATA;
      nfrc->fr.max_data.max_data = conn->rx.unsent_max_offset + delta;
      nfrc->next = pktns->tx.frq;
      pktns->tx.frq = nfrc;

      conn->rx.max_offset = conn->rx.unsent_max_offset =
          nfrc->fr.max_data.max_data;
      credit_expanded = 1;
    }

    ngtcp2_pkt_hd_init(hd, hd_flags, type, &conn->dcid.current.cid,
                       &conn->oscid, pktns->tx.last_pkt_num + 1,
                       pktns_select_pkt_numlen(pktns), conn->version, 0);

    ngtcp2_ppe_init(ppe, dest, destlen, cc);

    rv = ngtcp2_ppe_encode_hd(ppe, hd);
    if (rv != 0) {
      assert(NGTCP2_ERR_NOBUF == rv);
      return 0;
    }

    if (!ngtcp2_ppe_ensure_hp_sample(ppe)) {
      return 0;
    }

    if (ngtcp2_ringbuf_len(&conn->rx.path_challenge)) {
      pcent = ngtcp2_ringbuf_get(&conn->rx.path_challenge, 0);

      /* PATH_RESPONSE is bound to the path that the corresponding
         PATH_CHALLENGE is received. */
      if (ngtcp2_path_eq(&conn->dcid.current.ps.path, &pcent->ps.path)) {
        lfr.type = NGTCP2_FRAME_PATH_RESPONSE;
        memcpy(lfr.path_response.data, pcent->data,
               sizeof(lfr.path_response.data));

        rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &lfr);
        if (rv != 0) {
          assert(NGTCP2_ERR_NOBUF == rv);
        } else {
          ngtcp2_ringbuf_pop_front(&conn->rx.path_challenge);

          pkt_empty = 0;
          rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING;
          require_padding = !conn->server || destlen >= 1200;
          /* We don't retransmit PATH_RESPONSE. */
        }
      }
    }

    rv = conn_create_ack_frame(conn, &ackfr, pktns, type, ts,
                               conn_compute_ack_delay(conn),
                               conn->local.transport_params.ack_delay_exponent);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      return rv;
    }

    if (ackfr) {
      rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, ackfr);
      if (rv != 0) {
        assert(NGTCP2_ERR_NOBUF == rv);
      } else {
        ngtcp2_acktr_commit_ack(&pktns->acktr);
        ngtcp2_acktr_add_ack(&pktns->acktr, hd->pkt_num,
                             ackfr->ack.largest_ack);
        pkt_empty = 0;
      }
    }

  build_pkt:
    for (pfrc = &pktns->tx.frq; *pfrc;) {
      if ((*pfrc)->binder &&
          ((*pfrc)->binder->flags & NGTCP2_FRAME_CHAIN_BINDER_FLAG_ACK)) {
        frc = *pfrc;
        *pfrc = (*pfrc)->next;
        ngtcp2_frame_chain_del(frc, conn->mem);
        continue;
      }

      switch ((*pfrc)->fr.type) {
      case NGTCP2_FRAME_STOP_SENDING:
        strm =
            ngtcp2_conn_find_stream(conn, (*pfrc)->fr.stop_sending.stream_id);
        if (strm == NULL || (strm->flags & NGTCP2_STRM_FLAG_SHUT_RD)) {
          frc = *pfrc;
          *pfrc = (*pfrc)->next;
          ngtcp2_frame_chain_del(frc, conn->mem);
          continue;
        }
        break;
      case NGTCP2_FRAME_STREAM:
        assert(0);
        break;
      case NGTCP2_FRAME_MAX_STREAMS_BIDI:
        if ((*pfrc)->fr.max_streams.max_streams <
            conn->remote.bidi.max_streams) {
          frc = *pfrc;
          *pfrc = (*pfrc)->next;
          ngtcp2_frame_chain_del(frc, conn->mem);
          continue;
        }
        break;
      case NGTCP2_FRAME_MAX_STREAMS_UNI:
        if ((*pfrc)->fr.max_streams.max_streams <
            conn->remote.uni.max_streams) {
          frc = *pfrc;
          *pfrc = (*pfrc)->next;
          ngtcp2_frame_chain_del(frc, conn->mem);
          continue;
        }
        break;
      case NGTCP2_FRAME_MAX_STREAM_DATA:
        strm = ngtcp2_conn_find_stream(conn,
                                       (*pfrc)->fr.max_stream_data.stream_id);
        if (strm == NULL || (strm->flags & NGTCP2_STRM_FLAG_SHUT_RD) ||
            (*pfrc)->fr.max_stream_data.max_stream_data < strm->rx.max_offset) {
          frc = *pfrc;
          *pfrc = (*pfrc)->next;
          ngtcp2_frame_chain_del(frc, conn->mem);
          continue;
        }
        break;
      case NGTCP2_FRAME_MAX_DATA:
        if ((*pfrc)->fr.max_data.max_data < conn->rx.max_offset) {
          frc = *pfrc;
          *pfrc = (*pfrc)->next;
          ngtcp2_frame_chain_del(frc, conn->mem);
          continue;
        }
        break;
      case NGTCP2_FRAME_RETIRE_CONNECTION_ID:
        ++conn->dcid.num_retire_queued;
        break;
      case NGTCP2_FRAME_CRYPTO:
        assert(0);
        break;
      }

      rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &(*pfrc)->fr);
      if (rv != 0) {
        assert(NGTCP2_ERR_NOBUF == rv);
        break;
      }

      pkt_empty = 0;
      rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                         NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;
      pfrc = &(*pfrc)->next;
    }

    if (rv != NGTCP2_ERR_NOBUF) {
      for (; ngtcp2_ksl_len(&pktns->crypto.tx.frq);) {
        left = ngtcp2_ppe_left(ppe);

        crypto_offset = conn_cryptofrq_unacked_offset(conn, pktns);
        if (crypto_offset == (size_t)-1) {
          conn_cryptofrq_clear(conn, pktns);
          break;
        }

        left = ngtcp2_pkt_crypto_max_datalen(crypto_offset, left, left);

        if (left == (size_t)-1) {
          break;
        }

        rv = conn_cryptofrq_pop(conn, &nfrc, pktns, left);
        if (rv != 0) {
          assert(ngtcp2_err_is_fatal(rv));
          return rv;
        }

        if (nfrc == NULL) {
          break;
        }

        rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &nfrc->fr);
        if (rv != 0) {
          assert(0);
        }

        *pfrc = nfrc;
        pfrc = &(*pfrc)->next;

        pkt_empty = 0;
        rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                           NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;
      }
    }

    /* Write MAX_STREAM_ID after RESET_STREAM so that we can extend stream
       ID space in one packet. */
    if (rv != NGTCP2_ERR_NOBUF && *pfrc == NULL &&
        conn->remote.bidi.unsent_max_streams > conn->remote.bidi.max_streams) {
      rv = conn_call_extend_max_remote_streams_bidi(
          conn, conn->remote.bidi.unsent_max_streams);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        return rv;
      }

      rv = ngtcp2_frame_chain_new(&nfrc, conn->mem);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        return rv;
      }
      nfrc->fr.type = NGTCP2_FRAME_MAX_STREAMS_BIDI;
      nfrc->fr.max_streams.max_streams = conn->remote.bidi.unsent_max_streams;
      *pfrc = nfrc;

      conn->remote.bidi.max_streams = conn->remote.bidi.unsent_max_streams;

      rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &(*pfrc)->fr);
      if (rv != 0) {
        assert(NGTCP2_ERR_NOBUF == rv);
      } else {
        pkt_empty = 0;
        rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                           NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;
        pfrc = &(*pfrc)->next;
      }
    }

    if (rv != NGTCP2_ERR_NOBUF && *pfrc == NULL) {
      if (conn->remote.uni.unsent_max_streams > conn->remote.uni.max_streams) {
        rv = conn_call_extend_max_remote_streams_uni(
            conn, conn->remote.uni.unsent_max_streams);
        if (rv != 0) {
          assert(ngtcp2_err_is_fatal(rv));
          return rv;
        }

        rv = ngtcp2_frame_chain_new(&nfrc, conn->mem);
        if (rv != 0) {
          assert(ngtcp2_err_is_fatal(rv));
          return rv;
        }
        nfrc->fr.type = NGTCP2_FRAME_MAX_STREAMS_UNI;
        nfrc->fr.max_streams.max_streams = conn->remote.uni.unsent_max_streams;
        *pfrc = nfrc;

        conn->remote.uni.max_streams = conn->remote.uni.unsent_max_streams;

        rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd,
                                         &(*pfrc)->fr);
        if (rv != 0) {
          assert(NGTCP2_ERR_NOBUF == rv);
        } else {
          pkt_empty = 0;
          rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                             NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;
          pfrc = &(*pfrc)->next;
        }
      }
    }

    if (rv != NGTCP2_ERR_NOBUF) {
      for (; !ngtcp2_pq_empty(&conn->tx.strmq);) {
        strm = ngtcp2_conn_tx_strmq_top(conn);

        if (!(strm->flags & NGTCP2_STRM_FLAG_SHUT_RD) &&
            conn_should_send_max_stream_data(conn, strm)) {
          rv = ngtcp2_frame_chain_new(&nfrc, conn->mem);
          if (rv != 0) {
            assert(ngtcp2_err_is_fatal(rv));
            return rv;
          }

          if (conn->local.settings.max_stream_window &&
              strm->tx.last_max_stream_data_ts != UINT64_MAX &&
              ts - strm->tx.last_max_stream_data_ts <
                  NGTCP2_FLOW_WINDOW_RTT_FACTOR * cstat->smoothed_rtt &&
              conn->local.settings.max_stream_window > strm->rx.window) {
            target_max_data =
                NGTCP2_FLOW_WINDOW_SCALING_FACTOR * strm->rx.window;
            if (target_max_data > conn->local.settings.max_stream_window) {
              target_max_data = conn->local.settings.max_stream_window;
            }

            delta = target_max_data - strm->rx.window;
            if (strm->rx.unsent_max_offset + delta > NGTCP2_MAX_VARINT) {
              delta = NGTCP2_MAX_VARINT - strm->rx.unsent_max_offset;
            }

            strm->rx.window = target_max_data;
          } else {
            delta = 0;
          }

          strm->tx.last_max_stream_data_ts = ts;

          nfrc->fr.type = NGTCP2_FRAME_MAX_STREAM_DATA;
          nfrc->fr.max_stream_data.stream_id = strm->stream_id;
          nfrc->fr.max_stream_data.max_stream_data =
              strm->rx.unsent_max_offset + delta;
          ngtcp2_list_insert(nfrc, pfrc);

          rv =
              conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &nfrc->fr);
          if (rv != 0) {
            assert(NGTCP2_ERR_NOBUF == rv);
            break;
          }

          pkt_empty = 0;
          credit_expanded = 1;
          rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                             NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;
          pfrc = &(*pfrc)->next;
          strm->rx.max_offset = strm->rx.unsent_max_offset =
              nfrc->fr.max_stream_data.max_stream_data;
        }

        if (ngtcp2_strm_streamfrq_empty(strm)) {
          ngtcp2_conn_tx_strmq_pop(conn);
          continue;
        }

        stream_offset = ngtcp2_strm_streamfrq_unacked_offset(strm);
        if (stream_offset == (uint64_t)-1) {
          ngtcp2_strm_streamfrq_clear(strm);
          ngtcp2_conn_tx_strmq_pop(conn);
          continue;
        }

        left = ngtcp2_ppe_left(ppe);

        left = ngtcp2_pkt_stream_max_datalen(strm->stream_id, stream_offset,
                                             left, left);

        if (left == (size_t)-1) {
          break;
        }

        rv = ngtcp2_strm_streamfrq_pop(strm, &nfrc, left);
        if (rv != 0) {
          assert(ngtcp2_err_is_fatal(rv));
          return rv;
        }

        if (nfrc == NULL) {
          /* TODO Why? */
          break;
        }

        rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &nfrc->fr);
        if (rv != 0) {
          assert(0);
        }

        *pfrc = nfrc;
        pfrc = &(*pfrc)->next;

        pkt_empty = 0;
        rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                           NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;

        if (ngtcp2_strm_streamfrq_empty(strm)) {
          ngtcp2_conn_tx_strmq_pop(conn);
          continue;
        }

        ngtcp2_conn_tx_strmq_pop(conn);
        ++strm->cycle;
        rv = ngtcp2_conn_tx_strmq_push(conn, strm);
        if (rv != 0) {
          assert(ngtcp2_err_is_fatal(rv));
          return rv;
        }
      }
    }

    /* Add ACK if MAX_DATA or MAX_STREAM_DATA frame is encoded to
       decrease packet count. */
    if (ackfr == NULL && credit_expanded) {
      rv = conn_create_ack_frame(
          conn, &ackfr, pktns, type, ts, /* ack_delay = */ 0,
          conn->local.transport_params.ack_delay_exponent);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        return rv;
      }

      if (ackfr) {
        rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, ackfr);
        if (rv != 0) {
          assert(NGTCP2_ERR_NOBUF == rv);
        } else {
          ngtcp2_acktr_commit_ack(&pktns->acktr);
          ngtcp2_acktr_add_ack(&pktns->acktr, hd->pkt_num,
                               ackfr->ack.largest_ack);
        }
      }
    }

    if (rv != NGTCP2_ERR_NOBUF && !send_stream && !send_datagram &&
        !(rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) &&
        pktns->rtb.num_retransmittable && pktns->tx.frq == NULL &&
        pktns->rtb.probe_pkt_left) {
      num_reclaimed = ngtcp2_rtb_reclaim_on_pto(&pktns->rtb, conn, pktns,
                                                pktns->rtb.probe_pkt_left + 1);
      if (num_reclaimed < 0) {
        return rv;
      }
      if (num_reclaimed) {
        goto build_pkt;
      }

      /* We had pktns->rtb.num_retransmittable > 0 but the contents of
         those packets have been acknowledged (i.e., retransmission in
         another packet).  In this case, we don't have to send any
         probe packet. */
      if (pktns->rtb.num_retransmittable == 0) {
        pktns->rtb.probe_pkt_left = 0;
        ngtcp2_conn_set_loss_detection_timer(conn, ts);
      }
    }
  } else {
    pfrc = conn->pkt.pfrc;
    rtb_entry_flags |= conn->pkt.rtb_entry_flags;
    pkt_empty = conn->pkt.pkt_empty;
    hd_logged = conn->pkt.hd_logged;
  }

  left = ngtcp2_ppe_left(ppe);

  if (rv != NGTCP2_ERR_NOBUF && send_stream && *pfrc == NULL &&
      (ndatalen = ngtcp2_pkt_stream_max_datalen(
           vmsg->stream.strm->stream_id, vmsg->stream.strm->tx.offset, ndatalen,
           left)) != (size_t)-1 &&
      (ndatalen || datalen == 0)) {
    datacnt = ngtcp2_vec_copy_at_most(
        data, &ndatalen, NGTCP2_MAX_STREAM_DATACNT, vmsg->stream.data,
        vmsg->stream.datacnt, ndatalen);

    rv = ngtcp2_frame_chain_stream_datacnt_new(&nfrc, datacnt, conn->mem);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      return rv;
    }

    nfrc->fr.stream.type = NGTCP2_FRAME_STREAM;
    nfrc->fr.stream.flags = 0;
    nfrc->fr.stream.stream_id = vmsg->stream.strm->stream_id;
    nfrc->fr.stream.offset = vmsg->stream.strm->tx.offset;
    nfrc->fr.stream.datacnt = datacnt;
    ngtcp2_vec_copy(nfrc->fr.stream.data, data, datacnt);

    fin = (vmsg->stream.flags & NGTCP2_WRITE_STREAM_FLAG_FIN) &&
          ndatalen == datalen;
    nfrc->fr.stream.fin = (uint8_t)fin;

    rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &nfrc->fr);
    if (rv != 0) {
      assert(0);
    }

    *pfrc = nfrc;
    pfrc = &(*pfrc)->next;

    pkt_empty = 0;
    rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING |
                       NGTCP2_RTB_ENTRY_FLAG_RETRANSMITTABLE;

    vmsg->stream.strm->tx.offset += ndatalen;
    conn->tx.offset += ndatalen;

    if (fin) {
      ngtcp2_strm_shutdown(vmsg->stream.strm, NGTCP2_STRM_FLAG_SHUT_WR);
    }

    if (vmsg->stream.pdatalen) {
      *vmsg->stream.pdatalen = (ngtcp2_ssize)ndatalen;
    }
  } else {
    send_stream = 0;
  }

  if (rv != NGTCP2_ERR_NOBUF && send_datagram &&
      left >= ngtcp2_pkt_datagram_framelen(datalen)) {
    lfr.datagram.type = NGTCP2_FRAME_DATAGRAM_LEN;
    lfr.datagram.datacnt = vmsg->datagram.datacnt;
    lfr.datagram.data = (ngtcp2_vec *)vmsg->datagram.data;

    rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &lfr);
    assert(rv == 0);

    pkt_empty = 0;
    rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING;

    if (vmsg->datagram.paccepted) {
      *vmsg->datagram.paccepted = 1;
    }
  } else {
    send_datagram = 0;
  }

  if (pkt_empty) {
    assert(rv == 0 || NGTCP2_ERR_NOBUF == rv);
    if (rv == 0 && stream_blocked && ngtcp2_conn_get_max_data_left(conn)) {
      return NGTCP2_ERR_STREAM_DATA_BLOCKED;
    }

    if (conn->pktns.rtb.probe_pkt_left == 0) {
      return 0;
    }
  } else if (write_more) {
    conn->pkt.pfrc = pfrc;
    conn->pkt.pkt_empty = pkt_empty;
    conn->pkt.rtb_entry_flags = rtb_entry_flags;
    conn->pkt.hd_logged = hd_logged;
    conn->flags |= NGTCP2_CONN_FLAG_PPE_PENDING;

    assert(vmsg);

    switch (vmsg->type) {
    case NGTCP2_VMSG_TYPE_STREAM:
      if (send_stream) {
        if (ngtcp2_ppe_left(ppe)) {
          return NGTCP2_ERR_WRITE_MORE;
        }
      } else if (ngtcp2_conn_get_max_data_left(conn) && stream_blocked) {
        return NGTCP2_ERR_STREAM_DATA_BLOCKED;
      }
      break;
    case NGTCP2_VMSG_TYPE_DATAGRAM:
      if (send_datagram && ngtcp2_ppe_left(ppe)) {
        return NGTCP2_ERR_WRITE_MORE;
      }
      /* If DATAGRAM cannot be written due to insufficient space,
         continue to create a packet with the hope that application
         calls ngtcp2_conn_writev_datagram again. */
      break;
    default:
      assert(0);
    }
  }

  if (!(rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING)) {
    if (pktns->tx.num_non_ack_pkt >= NGTCP2_MAX_NON_ACK_TX_PKT) {
      lfr.type = NGTCP2_FRAME_PING;

      rv = conn_ppe_write_frame_hd_log(conn, ppe, &hd_logged, hd, &lfr);
      if (rv != 0) {
        assert(rv == NGTCP2_ERR_NOBUF);
        /* TODO If buffer is too small, PING cannot be written if
           packet is still empty. */
      } else {
        rtb_entry_flags |= NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING;
        pktns->tx.num_non_ack_pkt = 0;
      }
    } else {
      ++pktns->tx.num_non_ack_pkt;
    }
  } else {
    pktns->tx.num_non_ack_pkt = 0;
  }

  /* TODO Push STREAM frame back to ngtcp2_strm if there is an error
     before ngtcp2_rtb_entry is safely created and added. */
  lfr.type = NGTCP2_FRAME_PADDING;
  if ((require_padding ||
       /* Making full sized packet will help GSO a bit */
       ngtcp2_ppe_left(ppe) < 10 ||
       (type == NGTCP2_PKT_0RTT && conn->state == NGTCP2_CS_CLIENT_INITIAL)) &&
      ngtcp2_ppe_left(ppe)) {
    lfr.padding.len = ngtcp2_ppe_padding(ppe);
  } else {
    lfr.padding.len = ngtcp2_ppe_padding_size(ppe, min_pktlen);
  }

  if (lfr.padding.len) {
    padded = 1;
    ngtcp2_log_tx_fr(&conn->log, hd, &lfr);
    ngtcp2_qlog_write_frame(&conn->qlog, &lfr);
  }

  nwrite = ngtcp2_ppe_final(ppe, NULL);
  if (nwrite < 0) {
    assert(ngtcp2_err_is_fatal((int)nwrite));
    return nwrite;
  }

  ++cc->ckm->use_count;

  ngtcp2_qlog_pkt_sent_end(&conn->qlog, hd, (size_t)nwrite);

  /* TODO ack-eliciting vs needs-tracking */
  /* probe packet needs tracking but it does not need ACK, could be lost. */
  if ((rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) || padded) {
    if (pi) {
      conn_handle_tx_ecn(conn, pi, &rtb_entry_flags, pktns, hd, ts);
    }

    rv = ngtcp2_rtb_entry_new(&ent, hd, NULL, ts, (size_t)nwrite,
                              rtb_entry_flags, conn->mem);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal((int)nwrite));
      return rv;
    }

    if (*pfrc != pktns->tx.frq) {
      ent->frc = pktns->tx.frq;
      pktns->tx.frq = *pfrc;
      *pfrc = NULL;
    }

    if ((rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) &&
        pktns->rtb.num_ack_eliciting == 0 && conn->cc.event) {
      conn->cc.event(&conn->cc, &conn->cstat, NGTCP2_CC_EVENT_TYPE_TX_START,
                     ts);
    }

    rv = conn_on_pkt_sent(conn, &pktns->rtb, ent);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      ngtcp2_rtb_entry_del(ent, conn->mem);
      return rv;
    }

    if (rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) {
      if (conn->cc.on_pkt_sent) {
        conn->cc.on_pkt_sent(
            &conn->cc, &conn->cstat,
            ngtcp2_cc_pkt_init(&cc_pkt, hd->pkt_num, (size_t)nwrite,
                               NGTCP2_PKTNS_ID_APPLICATION, ts));
      }

      if (conn->flags & NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE) {
        conn_restart_timer_on_write(conn, ts);
      }
    }
  } else if (pi && conn->tx.ecn.state == NGTCP2_ECN_STATE_CAPABLE) {
    conn_handle_tx_ecn(conn, pi, NULL, pktns, hd, ts);
  }

  conn->flags &= (uint16_t)~NGTCP2_CONN_FLAG_PPE_PENDING;

  if (pktns->rtb.probe_pkt_left &&
      (rtb_entry_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING)) {
    --pktns->rtb.probe_pkt_left;

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON, "probe pkt size=%td",
                    nwrite);
  }

  conn->dcid.current.bytes_sent += (uint64_t)nwrite;

  ngtcp2_qlog_metrics_updated(&conn->qlog, &conn->cstat);

  ++pktns->tx.last_pkt_num;

  return nwrite;
}

ngtcp2_ssize ngtcp2_conn_write_single_frame_pkt(
    ngtcp2_conn *conn, ngtcp2_pkt_info *pi, uint8_t *dest, size_t destlen,
    uint8_t type, const ngtcp2_cid *dcid, ngtcp2_frame *fr, uint8_t rtb_flags,
    const ngtcp2_path *path, ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_ppe ppe;
  ngtcp2_pkt_hd hd;
  ngtcp2_frame lfr;
  ngtcp2_ssize nwrite;
  ngtcp2_crypto_cc cc;
  ngtcp2_pktns *pktns;
  uint8_t flags;
  ngtcp2_rtb_entry *rtbent;
  int padded = 0;

  switch (type) {
  case NGTCP2_PKT_INITIAL:
    pktns = conn->in_pktns;
    flags = NGTCP2_PKT_FLAG_LONG_FORM;
    break;
  case NGTCP2_PKT_HANDSHAKE:
    pktns = conn->hs_pktns;
    flags = NGTCP2_PKT_FLAG_LONG_FORM;
    break;
  case NGTCP2_PKT_SHORT:
    /* 0 means Short packet. */
    pktns = &conn->pktns;
    flags = (pktns->crypto.tx.ckm->flags & NGTCP2_CRYPTO_KM_FLAG_KEY_PHASE_ONE)
                ? NGTCP2_PKT_FLAG_KEY_PHASE
                : NGTCP2_PKT_FLAG_NONE;
    break;
  default:
    /* We don't support 0-RTT packet in this function. */
    assert(0);
  }

  cc.aead = pktns->crypto.ctx.aead;
  cc.hp = pktns->crypto.ctx.hp;
  cc.ckm = pktns->crypto.tx.ckm;
  cc.hp_ctx = pktns->crypto.tx.hp_ctx;
  cc.encrypt = conn->callbacks.encrypt;
  cc.hp_mask = conn->callbacks.hp_mask;

  ngtcp2_pkt_hd_init(&hd, flags, type, dcid, &conn->oscid,
                     pktns->tx.last_pkt_num + 1, pktns_select_pkt_numlen(pktns),
                     conn->version, 0);

  ngtcp2_ppe_init(&ppe, dest, destlen, &cc);

  rv = ngtcp2_ppe_encode_hd(&ppe, &hd);
  if (rv != 0) {
    assert(NGTCP2_ERR_NOBUF == rv);
    return 0;
  }

  if (!ngtcp2_ppe_ensure_hp_sample(&ppe)) {
    return 0;
  }

  ngtcp2_log_tx_pkt_hd(&conn->log, &hd);
  ngtcp2_qlog_pkt_sent_start(&conn->qlog);

  rv = conn_ppe_write_frame(conn, &ppe, &hd, fr);
  if (rv != 0) {
    assert(NGTCP2_ERR_NOBUF == rv);
    return 0;
  }

  lfr.type = NGTCP2_FRAME_PADDING;
  switch (fr->type) {
  case NGTCP2_FRAME_PATH_CHALLENGE:
  case NGTCP2_FRAME_PATH_RESPONSE:
    if (!conn->server || destlen >= 1200) {
      lfr.padding.len = ngtcp2_ppe_padding(&ppe);
    } else {
      lfr.padding.len = 0;
    }
    break;
  default:
    if (type == NGTCP2_PKT_SHORT) {
      lfr.padding.len =
          ngtcp2_ppe_padding_size(&ppe, conn_min_short_pktlen(conn));
    } else {
      lfr.padding.len = ngtcp2_ppe_padding_hp_sample(&ppe);
    }
  }
  if (lfr.padding.len) {
    padded = 1;
    ngtcp2_log_tx_fr(&conn->log, &hd, &lfr);
    ngtcp2_qlog_write_frame(&conn->qlog, &lfr);
  }

  nwrite = ngtcp2_ppe_final(&ppe, NULL);
  if (nwrite < 0) {
    return nwrite;
  }

  if (type == NGTCP2_PKT_SHORT) {
    ++cc.ckm->use_count;
  }

  ngtcp2_qlog_pkt_sent_end(&conn->qlog, &hd, (size_t)nwrite);

  /* Do this when we are sure that there is no error. */
  switch (fr->type) {
  case NGTCP2_FRAME_ACK:
  case NGTCP2_FRAME_ACK_ECN:
    ngtcp2_acktr_commit_ack(&pktns->acktr);
    ngtcp2_acktr_add_ack(&pktns->acktr, hd.pkt_num, fr->ack.largest_ack);
    break;
  }

  if (((rtb_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) || padded) &&
      (!path || ngtcp2_path_eq(&conn->dcid.current.ps.path, path))) {
    if (pi) {
      conn_handle_tx_ecn(conn, pi, &rtb_flags, pktns, &hd, ts);
    }

    rv = ngtcp2_rtb_entry_new(&rtbent, &hd, NULL, ts, (size_t)nwrite, rtb_flags,
                              conn->mem);
    if (rv != 0) {
      return rv;
    }

    rv = conn_on_pkt_sent(conn, &pktns->rtb, rtbent);
    if (rv != 0) {
      ngtcp2_rtb_entry_del(rtbent, conn->mem);
      return rv;
    }

    if ((rtb_flags & NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING) &&
        (conn->flags & NGTCP2_CONN_FLAG_RESTART_IDLE_TIMER_ON_WRITE)) {
      conn_restart_timer_on_write(conn, ts);
    }
  } else if (pi && conn->tx.ecn.state == NGTCP2_ECN_STATE_CAPABLE) {
    conn_handle_tx_ecn(conn, pi, NULL, pktns, &hd, ts);
  }

  ngtcp2_qlog_metrics_updated(&conn->qlog, &conn->cstat);

  ++pktns->tx.last_pkt_num;

  return nwrite;
}

/*
 * conn_process_early_rtb makes any pending 0RTT packet Short packet.
 */
static void conn_process_early_rtb(ngtcp2_conn *conn) {
  ngtcp2_rtb_entry *ent;
  ngtcp2_rtb *rtb = &conn->pktns.rtb;
  ngtcp2_ksl_it it;

  for (it = ngtcp2_rtb_head(rtb); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    ent = ngtcp2_ksl_it_get(&it);

    if ((ent->hd.flags & NGTCP2_PKT_FLAG_LONG_FORM) == 0 ||
        ent->hd.type != NGTCP2_PKT_0RTT) {
      continue;
    }

    /*  0-RTT packet is retransmitted as a Short packet. */
    ent->hd.flags &= (uint8_t)~NGTCP2_PKT_FLAG_LONG_FORM;
    ent->hd.type = NGTCP2_PKT_SHORT;
  }
}

/*
 * conn_handshake_remnants_left returns nonzero if there may be
 * handshake packets the local endpoint has to send, including new
 * packets and lost ones.
 */
static int conn_handshake_remnants_left(ngtcp2_conn *conn) {
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;

  return !(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED) ||
         (in_pktns && (in_pktns->rtb.num_retransmittable ||
                       ngtcp2_ksl_len(&in_pktns->crypto.tx.frq))) ||
         (hs_pktns && (hs_pktns->rtb.num_retransmittable ||
                       ngtcp2_ksl_len(&hs_pktns->crypto.tx.frq)));
}

/*
 * conn_retire_dcid_seq retires destination connection ID denoted by
 * |seq|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_retire_dcid_seq(ngtcp2_conn *conn, uint64_t seq) {
  ngtcp2_pktns *pktns = &conn->pktns;
  ngtcp2_frame_chain *nfrc;
  int rv;

  rv = ngtcp2_frame_chain_new(&nfrc, conn->mem);
  if (rv != 0) {
    return rv;
  }

  nfrc->fr.type = NGTCP2_FRAME_RETIRE_CONNECTION_ID;
  nfrc->fr.retire_connection_id.seq = seq;
  nfrc->next = pktns->tx.frq;
  pktns->tx.frq = nfrc;

  return 0;
}

/*
 * conn_retire_dcid retires |dcid|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
static int conn_retire_dcid(ngtcp2_conn *conn, const ngtcp2_dcid *dcid,
                            ngtcp2_tstamp ts) {
  ngtcp2_ringbuf *rb = &conn->dcid.retired;
  ngtcp2_dcid *dest, *stale_dcid;
  int rv;

  assert(dcid->cid.datalen);

  if (ngtcp2_ringbuf_full(rb)) {
    stale_dcid = ngtcp2_ringbuf_get(rb, 0);
    rv = conn_call_deactivate_dcid(conn, stale_dcid);
    if (rv != 0) {
      return rv;
    }

    ngtcp2_ringbuf_pop_front(rb);
  }

  dest = ngtcp2_ringbuf_push_back(rb);
  ngtcp2_dcid_copy(dest, dcid);
  dest->ts_retired = ts;

  return conn_retire_dcid_seq(conn, dcid->seq);
}

/*
 * conn_bind_dcid stores the DCID to |*pdcid| bound to |path|.  If
 * such DCID is not found, bind the new DCID to |path| and stores it
 * to |*pdcid|.  If a remote endpoint uses zero-length connection ID,
 * the pointer to conn->dcid.current is assigned to |*pdcid|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CONN_ID_BLOCKED
 *     No unused DCID is available
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
static int conn_bind_dcid(ngtcp2_conn *conn, ngtcp2_dcid **pdcid,
                          const ngtcp2_path *path, ngtcp2_tstamp ts) {
  ngtcp2_pv *pv = conn->pv;
  ngtcp2_dcid *dcid, *ndcid;
  ngtcp2_cid cid;
  size_t i, len;
  int rv;

  assert(!ngtcp2_path_eq(&conn->dcid.current.ps.path, path));
  assert(!pv || !ngtcp2_path_eq(&pv->dcid.ps.path, path));
  assert(!pv || !(pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) ||
         !ngtcp2_path_eq(&pv->fallback_dcid.ps.path, path));

  len = ngtcp2_ringbuf_len(&conn->dcid.bound);
  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.bound, i);

    if (ngtcp2_path_eq(&dcid->ps.path, path)) {
      *pdcid = dcid;
      return 0;
    }
  }

  if (conn->dcid.current.cid.datalen == 0) {
    ndcid = ngtcp2_ringbuf_push_back(&conn->dcid.bound);
    ngtcp2_cid_zero(&cid);
    ngtcp2_dcid_init(ndcid, ++conn->dcid.zerolen_seq, &cid, NULL);
    ngtcp2_path_copy(&ndcid->ps.path, path);

    *pdcid = ndcid;

    return 0;
  }

  if (ngtcp2_ringbuf_len(&conn->dcid.unused) == 0) {
    return NGTCP2_ERR_CONN_ID_BLOCKED;
  }

  if (ngtcp2_ringbuf_full(&conn->dcid.bound)) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.bound, 0);
    rv = conn_retire_dcid(conn, dcid, ts);
    if (rv != 0) {
      return rv;
    }
  }

  dcid = ngtcp2_ringbuf_get(&conn->dcid.unused, 0);
  ndcid = ngtcp2_ringbuf_push_back(&conn->dcid.bound);

  ngtcp2_dcid_copy(ndcid, dcid);
  ngtcp2_path_copy(&ndcid->ps.path, path);

  ngtcp2_ringbuf_pop_front(&conn->dcid.unused);

  *pdcid = ndcid;

  return 0;
}

/*
 * conn_stop_pv stops the path validation which is currently running.
 * This function does nothing if no path validation is currently being
 * performed.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
static int conn_stop_pv(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  int rv = 0;
  ngtcp2_pv *pv = conn->pv;

  if (pv == NULL) {
    return 0;
  }

  if (pv->dcid.cid.datalen && pv->dcid.seq != conn->dcid.current.seq) {
    rv = conn_retire_dcid(conn, &pv->dcid, ts);
    if (rv != 0) {
      goto fin;
    }
  }

  if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
      pv->fallback_dcid.cid.datalen &&
      pv->fallback_dcid.seq != conn->dcid.current.seq &&
      pv->fallback_dcid.seq != pv->dcid.seq) {
    rv = conn_retire_dcid(conn, &pv->fallback_dcid, ts);
    if (rv != 0) {
      goto fin;
    }
  }

fin:
  ngtcp2_pv_del(pv);
  conn->pv = NULL;

  return rv;
}

static void conn_reset_congestion_state(ngtcp2_conn *conn);

/*
 * conn_on_path_validation_failed is called when path validation
 * fails.  This function may delete |pv|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_on_path_validation_failed(ngtcp2_conn *conn, ngtcp2_pv *pv,
                                          ngtcp2_tstamp ts) {
  int rv;

  rv = conn_call_path_validation(conn, &pv->dcid.ps.path,
                                 NGTCP2_PATH_VALIDATION_RESULT_FAILURE);
  if (rv != 0) {
    return rv;
  }

  if (pv->flags & NGTCP2_PV_FLAG_MTU_PROBE) {
    return NGTCP2_ERR_NO_VIABLE_PATH;
  }

  if (pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) {
    ngtcp2_dcid_copy(&conn->dcid.current, &pv->fallback_dcid);
    conn_reset_congestion_state(conn);
  }

  return conn_stop_pv(conn, ts);
}

/*
 * dcid_tx_left returns the maximum number of bytes that server is
 * allowed to send to an unvalidated path associated to |dcid|.
 */
static size_t dcid_tx_left(ngtcp2_dcid *dcid) {
  if (dcid->flags & NGTCP2_DCID_FLAG_PATH_VALIDATED) {
    return SIZE_MAX;
  }
  /* From QUIC spec: Prior to validating the client address, servers
     MUST NOT send more than three times as many bytes as the number
     of bytes they have received. */
  assert(dcid->bytes_recv * 3 >= dcid->bytes_sent);

  return dcid->bytes_recv * 3 - dcid->bytes_sent;
}

/*
 * conn_server_tx_left returns the maximum number of bytes that server
 * is allowed to send to an unvalidated path.
 */
static size_t conn_server_tx_left(ngtcp2_conn *conn, ngtcp2_dcid *dcid) {
  assert(conn->server);

  /* If pv->dcid has the current path, use conn->dcid.current.  This
     is because conn->dcid.current gets update for bytes_recv and
     bytes_sent. */
  if (ngtcp2_path_eq(&dcid->ps.path, &conn->dcid.current.ps.path)) {
    return dcid_tx_left(&conn->dcid.current);
  }

  return dcid_tx_left(dcid);
}

/*
 * conn_write_path_challenge writes a packet which includes
 * PATH_CHALLENGE frame into |dest| of length |destlen|.
 *
 * This function returns the number of bytes written to |dest|, or one
 * of the following negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static ngtcp2_ssize conn_write_path_challenge(ngtcp2_conn *conn,
                                              ngtcp2_path *path,
                                              ngtcp2_pkt_info *pi,
                                              uint8_t *dest, size_t destlen,
                                              ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_ssize nwrite;
  ngtcp2_tstamp expiry;
  ngtcp2_pv *pv = conn->pv;
  ngtcp2_frame lfr;
  ngtcp2_duration timeout;
  uint8_t flags;
  size_t tx_left;

  if (ngtcp2_pv_validation_timed_out(pv, ts)) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PTV,
                    "path validation was timed out");
    return conn_on_path_validation_failed(conn, pv, ts);
  }

  ngtcp2_pv_handle_entry_expiry(pv, ts);

  if (!ngtcp2_pv_should_send_probe(pv)) {
    return 0;
  }

  assert(conn->callbacks.rand);
  rv = conn->callbacks.rand(
      lfr.path_challenge.data, sizeof(lfr.path_challenge.data),
      &conn->local.settings.rand_ctx, NGTCP2_RAND_USAGE_PATH_CHALLENGE);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  lfr.type = NGTCP2_FRAME_PATH_CHALLENGE;

  timeout = conn_compute_pto(conn, &conn->pktns);
  timeout = ngtcp2_max(timeout, 3 * conn->cstat.initial_rtt);
  expiry = ts + timeout * (1ULL << pv->round);

  if (conn->server) {
    if (!(pv->dcid.flags & NGTCP2_DCID_FLAG_PATH_VALIDATED)) {
      tx_left = conn_server_tx_left(conn, &pv->dcid);
      destlen = ngtcp2_min(destlen, tx_left);
      if (destlen == 0) {
        return 0;
      }
    }

    if (destlen < 1200) {
      flags = NGTCP2_PV_ENTRY_FLAG_UNDERSIZED;
    } else {
      flags = NGTCP2_PV_ENTRY_FLAG_NONE;
    }
  } else {
    flags = NGTCP2_PV_ENTRY_FLAG_NONE;
  }

  ngtcp2_pv_add_entry(pv, lfr.path_challenge.data, expiry, flags, ts);

  nwrite = ngtcp2_conn_write_single_frame_pkt(
      conn, pi, dest, destlen, NGTCP2_PKT_SHORT, &pv->dcid.cid, &lfr,
      NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING, &pv->dcid.ps.path, ts);
  if (nwrite <= 0) {
    return nwrite;
  }

  if (path) {
    ngtcp2_path_copy(path, &pv->dcid.ps.path);
  }

  if (ngtcp2_path_eq(&pv->dcid.ps.path, &conn->dcid.current.ps.path)) {
    conn->dcid.current.bytes_sent += (uint64_t)nwrite;
  } else {
    pv->dcid.bytes_sent += (uint64_t)nwrite;
  }

  return nwrite;
}

/*
 * conn_write_path_response writes a packet which includes
 * PATH_RESPONSE frame into |dest| of length |destlen|.
 *
 * This function returns the number of bytes written to |dest|, or one
 * of the following negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static ngtcp2_ssize conn_write_path_response(ngtcp2_conn *conn,
                                             ngtcp2_path *path,
                                             ngtcp2_pkt_info *pi, uint8_t *dest,
                                             size_t destlen, ngtcp2_tstamp ts) {
  ngtcp2_pv *pv = conn->pv;
  ngtcp2_path_challenge_entry *pcent = NULL;
  ngtcp2_dcid *dcid = NULL;
  ngtcp2_frame lfr;
  ngtcp2_ssize nwrite;
  int rv;
  size_t tx_left;

  for (; ngtcp2_ringbuf_len(&conn->rx.path_challenge);) {
    pcent = ngtcp2_ringbuf_get(&conn->rx.path_challenge, 0);

    if (ngtcp2_path_eq(&conn->dcid.current.ps.path, &pcent->ps.path)) {
      /* Send PATH_RESPONSE from conn_write_pkt. */
      return 0;
    }

    if (pv) {
      if (ngtcp2_path_eq(&pv->dcid.ps.path, &pcent->ps.path)) {
        dcid = &pv->dcid;
        break;
      }
      if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
          ngtcp2_path_eq(&pv->fallback_dcid.ps.path, &pcent->ps.path)) {
        dcid = &pv->fallback_dcid;
        break;
      }
    }

    if (conn->server) {
      break;
    }

    /* Client does not expect to respond to path validation against
       unknown path */
    ngtcp2_ringbuf_pop_front(&conn->rx.path_challenge);
    pcent = NULL;
  }

  if (pcent == NULL) {
    return 0;
  }

  if (dcid == NULL) {
    /* client is expected to have |path| in conn->dcid.current or
       conn->pv. */
    assert(conn->server);

    rv = conn_bind_dcid(conn, &dcid, &pcent->ps.path, ts);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      return 0;
    }
  }

  if (conn->server && !(dcid->flags & NGTCP2_DCID_FLAG_PATH_VALIDATED)) {
    tx_left = conn_server_tx_left(conn, dcid);
    destlen = ngtcp2_min(destlen, tx_left);
    if (destlen == 0) {
      return 0;
    }
  }

  lfr.type = NGTCP2_FRAME_PATH_RESPONSE;
  memcpy(lfr.path_response.data, pcent->data, sizeof(lfr.path_response.data));

  nwrite = ngtcp2_conn_write_single_frame_pkt(
      conn, pi, dest, destlen, NGTCP2_PKT_SHORT, &dcid->cid, &lfr,
      NGTCP2_RTB_ENTRY_FLAG_ACK_ELICITING, &pcent->ps.path, ts);
  if (nwrite <= 0) {
    return nwrite;
  }

  if (path) {
    ngtcp2_path_copy(path, &pcent->ps.path);
  }

  ngtcp2_ringbuf_pop_front(&conn->rx.path_challenge);

  dcid->bytes_sent += (uint64_t)nwrite;

  return nwrite;
}

ngtcp2_ssize ngtcp2_conn_write_pkt(ngtcp2_conn *conn, ngtcp2_path *path,
                                   ngtcp2_pkt_info *pi, uint8_t *dest,
                                   size_t destlen, ngtcp2_tstamp ts) {
  return ngtcp2_conn_writev_stream(conn, path, pi, dest, destlen,
                                   /* pdatalen = */ NULL,
                                   NGTCP2_WRITE_STREAM_FLAG_NONE,
                                   /* stream_id = */ -1,
                                   /* datav = */ NULL, /* datavcnt = */ 0, ts);
}

/*
 * conn_on_version_negotiation is called when Version Negotiation
 * packet is received.  The function decodes the data in the buffer
 * pointed by |payload| whose length is |payloadlen| as Version
 * Negotiation packet payload.  The packet header is given in |hd|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     Packet payload is badly formatted.
 */
static int conn_on_version_negotiation(ngtcp2_conn *conn,
                                       const ngtcp2_pkt_hd *hd,
                                       const uint8_t *payload,
                                       size_t payloadlen) {
  uint32_t sv[16];
  uint32_t *p;
  int rv = 0;
  size_t nsv;
  size_t i;

  if (payloadlen % sizeof(uint32_t)) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (payloadlen > sizeof(sv)) {
    p = ngtcp2_mem_malloc(conn->mem, payloadlen);
    if (p == NULL) {
      return NGTCP2_ERR_NOMEM;
    }
  } else {
    p = sv;
  }

  nsv = ngtcp2_pkt_decode_version_negotiation(p, payload, payloadlen);

  ngtcp2_log_rx_vn(&conn->log, hd, p, nsv);

  for (i = 0; i < nsv; ++i) {
    if (p[i] == conn->version) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "ignore Version Negotiation because it contains version "
                      "selected by client");

      rv = NGTCP2_ERR_INVALID_ARGUMENT;
      goto fin;
    }
  }

  if (conn->callbacks.recv_version_negotiation) {
    rv = conn->callbacks.recv_version_negotiation(conn, hd, p, nsv,
                                                  conn->user_data);
    if (rv != 0) {
      rv = NGTCP2_ERR_CALLBACK_FAILURE;
    }
  }

  if (rv == 0) {
    /* TODO Just move to the terminal state for now in order not to
       send CONNECTION_CLOSE frame. */
    conn->state = NGTCP2_CS_DRAINING;
  }

fin:
  if (p != sv) {
    ngtcp2_mem_free(conn->mem, p);
  }

  return rv;
}

static uint64_t conn_tx_strmq_first_cycle(ngtcp2_conn *conn) {
  ngtcp2_strm *strm;

  if (ngtcp2_pq_empty(&conn->tx.strmq)) {
    return 0;
  }

  strm = ngtcp2_struct_of(ngtcp2_pq_top(&conn->tx.strmq), ngtcp2_strm, pe);
  return strm->cycle;
}

uint64_t ngtcp2_conn_tx_strmq_first_cycle(ngtcp2_conn *conn) {
  ngtcp2_strm *strm;

  if (ngtcp2_pq_empty(&conn->tx.strmq)) {
    return 0;
  }

  strm = ngtcp2_struct_of(ngtcp2_pq_top(&conn->tx.strmq), ngtcp2_strm, pe);
  return strm->cycle;
}

int ngtcp2_conn_resched_frames(ngtcp2_conn *conn, ngtcp2_pktns *pktns,
                               ngtcp2_frame_chain **pfrc) {
  ngtcp2_frame_chain **first = pfrc;
  ngtcp2_frame_chain *frc;
  ngtcp2_stream *sfr;
  ngtcp2_strm *strm;
  int rv;

  if (*pfrc == NULL) {
    return 0;
  }

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
        strm->cycle = conn_tx_strmq_first_cycle(conn);
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
  pktns->tx.frq = *first;

  return 0;
}

/*
 * conn_on_retry is called when Retry packet is received.  The
 * function decodes the data in the buffer pointed by |pkt| whose
 * length is |pktlen| as Retry packet.  The length of long packet
 * header is given in |hdpktlen|.  |pkt| includes packet header.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     Packet payload is badly formatted.
 * NGTCP2_ERR_PROTO
 *     ODCID does not match; or Token is empty.
 */
static int conn_on_retry(ngtcp2_conn *conn, const ngtcp2_pkt_hd *hd,
                         size_t hdpktlen, const uint8_t *pkt, size_t pktlen) {
  int rv;
  ngtcp2_pkt_retry retry;
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_rtb *rtb = &conn->pktns.rtb;
  ngtcp2_rtb *in_rtb;
  uint8_t cidbuf[sizeof(retry.odcid.data) * 2 + 1];
  ngtcp2_vec *token;

  if (!in_pktns || conn->flags & NGTCP2_CONN_FLAG_RECV_RETRY) {
    return 0;
  }

  in_rtb = &in_pktns->rtb;

  rv = ngtcp2_pkt_decode_retry(&retry, pkt + hdpktlen, pktlen - hdpktlen);
  if (rv != 0) {
    return rv;
  }

  retry.odcid = conn->dcid.current.cid;

  rv = ngtcp2_pkt_verify_retry_tag(
      conn->version, &retry, pkt, pktlen, conn->callbacks.encrypt,
      &conn->crypto.retry_aead, &conn->crypto.retry_aead_ctx);
  if (rv != 0) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "unable to verify Retry packet integrity");
    return rv;
  }

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT, "odcid=0x%s",
                  (const char *)ngtcp2_encode_hex(cidbuf, retry.odcid.data,
                                                  retry.odcid.datalen));

  if (retry.token.len == 0) {
    return NGTCP2_ERR_PROTO;
  }

  if (ngtcp2_cid_eq(&conn->dcid.current.cid, &hd->scid)) {
    return 0;
  }

  ngtcp2_qlog_retry_pkt_received(&conn->qlog, hd);

  /* DCID must be updated before invoking callback because client
     generates new initial keys there. */
  conn->dcid.current.cid = hd->scid;
  conn->retry_scid = hd->scid;

  conn->flags |= NGTCP2_CONN_FLAG_RECV_RETRY;

  assert(conn->callbacks.recv_retry);

  rv = conn->callbacks.recv_retry(conn, hd, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  conn->state = NGTCP2_CS_CLIENT_INITIAL;

  /* Just freeing memory is dangerous because we might free twice. */

  rv = ngtcp2_rtb_remove_all(rtb, conn, &conn->pktns, &conn->cstat);
  if (rv != 0) {
    return rv;
  }

  rv = ngtcp2_rtb_remove_all(in_rtb, conn, in_pktns, &conn->cstat);
  if (rv != 0) {
    return rv;
  }

  token = &conn->local.settings.token;

  ngtcp2_mem_free(conn->mem, token->base);
  token->base = NULL;
  token->len = 0;

  token->base = ngtcp2_mem_malloc(conn->mem, retry.token.len);
  if (token->base == NULL) {
    return NGTCP2_ERR_NOMEM;
  }
  token->len = retry.token.len;

  ngtcp2_cpymem(token->base, retry.token.base, retry.token.len);

  reset_conn_stat_recovery(&conn->cstat);
  conn_reset_congestion_state(conn);
  conn_reset_ecn_validation_state(conn);

  return 0;
}

int ngtcp2_conn_detect_lost_pkt(ngtcp2_conn *conn, ngtcp2_pktns *pktns,
                                ngtcp2_conn_stat *cstat, ngtcp2_tstamp ts) {
  ngtcp2_duration pto = conn_compute_pto(conn, pktns);
  int rv = ngtcp2_rtb_detect_lost_pkt(&pktns->rtb, conn, pktns, cstat, pto, ts);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

/*
 * conn_recv_ack processes received ACK frame |fr|.  |pkt_ts| is the
 * timestamp when packet is received.  |ts| should be the current
 * time.  Usually they are the same, but for buffered packets,
 * |pkt_ts| would be earlier than |ts|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_ACK_FRAME
 *     ACK frame is malformed.
 * NGTCP2_ERR_PROTO
 *     |fr| acknowledges a packet this endpoint has not sent.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed.
 */
static int conn_recv_ack(ngtcp2_conn *conn, ngtcp2_pktns *pktns, ngtcp2_ack *fr,
                         ngtcp2_tstamp pkt_ts, ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_frame_chain *frc = NULL;
  ngtcp2_ssize num_acked;
  ngtcp2_conn_stat *cstat = &conn->cstat;

  if (pktns->tx.last_pkt_num < fr->largest_ack) {
    return NGTCP2_ERR_PROTO;
  }

  rv = ngtcp2_pkt_validate_ack(fr);
  if (rv != 0) {
    return rv;
  }

  ngtcp2_acktr_recv_ack(&pktns->acktr, fr);

  num_acked = ngtcp2_rtb_recv_ack(&pktns->rtb, fr, &conn->cstat, conn, pktns,
                                  pkt_ts, ts);
  if (num_acked < 0) {
    /* TODO assert this */
    assert(ngtcp2_err_is_fatal((int)num_acked));
    ngtcp2_frame_chain_list_del(frc, conn->mem);
    return (int)num_acked;
  }

  if (num_acked == 0) {
    return 0;
  }

  rv = ngtcp2_conn_detect_lost_pkt(conn, pktns, &conn->cstat, ts);
  if (rv != 0) {
    return rv;
  }

  pktns->rtb.probe_pkt_left = 0;

  if (cstat->pto_count &&
      (conn->server || (conn->flags & NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED))) {
    /* Reset PTO count but no less than 2 to avoid frequent probe
       packet transmission. */
    cstat->pto_count = ngtcp2_min(cstat->pto_count, 2);
  }

  ngtcp2_conn_set_loss_detection_timer(conn, ts);

  return 0;
}

/*
 * conn_assign_recved_ack_delay_unscaled assigns
 * fr->ack_delay_unscaled.
 */
static void assign_recved_ack_delay_unscaled(ngtcp2_ack *fr,
                                             uint64_t ack_delay_exponent) {
  fr->ack_delay_unscaled =
      fr->ack_delay * (1ULL << ack_delay_exponent) * NGTCP2_MICROSECONDS;
}

/*
 * conn_recv_max_stream_data processes received MAX_STREAM_DATA frame
 * |fr|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_STREAM_STATE
 *     Stream ID indicates that it is a local stream, and the local
 *     endpoint has not initiated it; or stream is peer initiated
 *     unidirectional stream.
 * NGTCP2_ERR_STREAM_LIMIT
 *     Stream ID exceeds allowed limit.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_recv_max_stream_data(ngtcp2_conn *conn,
                                     const ngtcp2_max_stream_data *fr) {
  ngtcp2_strm *strm;
  ngtcp2_idtr *idtr;
  int local_stream = conn_local_stream(conn, fr->stream_id);
  int bidi = bidi_stream(fr->stream_id);
  int rv;

  if (bidi) {
    if (local_stream) {
      if (conn->local.bidi.next_stream_id <= fr->stream_id) {
        return NGTCP2_ERR_STREAM_STATE;
      }
    } else if (conn->remote.bidi.max_streams <
               ngtcp2_ord_stream_id(fr->stream_id)) {
      return NGTCP2_ERR_STREAM_LIMIT;
    }

    idtr = &conn->remote.bidi.idtr;
  } else {
    if (!local_stream || conn->local.uni.next_stream_id <= fr->stream_id) {
      return NGTCP2_ERR_STREAM_STATE;
    }

    idtr = &conn->remote.uni.idtr;
  }

  strm = ngtcp2_conn_find_stream(conn, fr->stream_id);
  if (strm == NULL) {
    if (local_stream) {
      /* Stream has been closed. */
      return 0;
    }

    rv = ngtcp2_idtr_open(idtr, fr->stream_id);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      assert(rv == NGTCP2_ERR_STREAM_IN_USE);
      /* Stream has been closed. */
      return 0;
    }

    strm = ngtcp2_mem_malloc(conn->mem, sizeof(ngtcp2_strm));
    if (strm == NULL) {
      return NGTCP2_ERR_NOMEM;
    }
    rv = ngtcp2_conn_init_stream(conn, strm, fr->stream_id, NULL);
    if (rv != 0) {
      ngtcp2_mem_free(conn->mem, strm);
      return rv;
    }
  }

  if (strm->tx.max_offset < fr->max_stream_data) {
    strm->tx.max_offset = fr->max_stream_data;

    /* Don't call callback if stream is half-closed local */
    if (strm->flags & NGTCP2_STRM_FLAG_SHUT_WR) {
      return 0;
    }

    rv = conn_call_extend_max_stream_data(conn, strm, fr->stream_id,
                                          fr->max_stream_data);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

/*
 * conn_recv_max_data processes received MAX_DATA frame |fr|.
 */
static void conn_recv_max_data(ngtcp2_conn *conn, const ngtcp2_max_data *fr) {
  conn->tx.max_offset = ngtcp2_max(conn->tx.max_offset, fr->max_data);
}

/*
 * conn_buffer_pkt buffers |pkt| of length |pktlen|, chaining it from
 * |*ppc|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_buffer_pkt(ngtcp2_conn *conn, ngtcp2_pktns *pktns,
                           const ngtcp2_path *path, const ngtcp2_pkt_info *pi,
                           const uint8_t *pkt, size_t pktlen, size_t dgramlen,
                           ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_pkt_chain **ppc = &pktns->rx.buffed_pkts, *pc;
  size_t i;
  for (i = 0; *ppc && i < NGTCP2_MAX_NUM_BUFFED_RX_PKTS;
       ppc = &(*ppc)->next, ++i)
    ;

  if (i == NGTCP2_MAX_NUM_BUFFED_RX_PKTS) {
    return 0;
  }

  rv =
      ngtcp2_pkt_chain_new(&pc, path, pi, pkt, pktlen, dgramlen, ts, conn->mem);
  if (rv != 0) {
    return rv;
  }

  *ppc = pc;

  return 0;
}

static int ensure_decrypt_buffer(ngtcp2_vec *vec, size_t n, size_t initial,
                                 const ngtcp2_mem *mem) {
  uint8_t *nbuf;
  size_t len;

  if (vec->len >= n) {
    return 0;
  }

  len = vec->len == 0 ? initial : vec->len * 2;
  for (; len < n; len *= 2)
    ;
  nbuf = ngtcp2_mem_realloc(mem, vec->base, len);
  if (nbuf == NULL) {
    return NGTCP2_ERR_NOMEM;
  }
  vec->base = nbuf;
  vec->len = len;

  return 0;
}

/*
 * conn_ensure_decrypt_hp_buffer ensures that
 * conn->crypto.decrypt_hp_buf has at least |n| bytes space.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_ensure_decrypt_hp_buffer(ngtcp2_conn *conn, size_t n) {
  return ensure_decrypt_buffer(&conn->crypto.decrypt_hp_buf, n, 256, conn->mem);
}

/*
 * conn_ensure_decrypt_buffer ensures that conn->crypto.decrypt_buf
 * has at least |n| bytes space.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_ensure_decrypt_buffer(ngtcp2_conn *conn, size_t n) {
  return ensure_decrypt_buffer(&conn->crypto.decrypt_buf, n, 2048, conn->mem);
}

/*
 * decrypt_pkt decrypts the data pointed by |payload| whose length is
 * |payloadlen|, and writes plaintext data to the buffer pointed by
 * |dest|.  The buffer pointed by |ad| is the Additional Data, and its
 * length is |adlen|.  |pkt_num| is used to create a nonce.  |ckm| is
 * the cryptographic key, and iv to use.  |decrypt| is a callback
 * function which actually decrypts a packet.
 *
 * This function returns the number of bytes written in |dest| if it
 * succeeds, or one of the following negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed.
 * NGTCP2_ERR_TLS_DECRYPT
 *     TLS backend failed to decrypt data.
 */
static ngtcp2_ssize decrypt_pkt(uint8_t *dest, const ngtcp2_crypto_aead *aead,
                                const uint8_t *payload, size_t payloadlen,
                                const uint8_t *ad, size_t adlen,
                                int64_t pkt_num, ngtcp2_crypto_km *ckm,
                                ngtcp2_decrypt decrypt) {
  /* TODO nonce is limited to 64 bytes. */
  uint8_t nonce[64];
  int rv;

  assert(sizeof(nonce) >= ckm->iv.len);

  ngtcp2_crypto_create_nonce(nonce, ckm->iv.base, ckm->iv.len, pkt_num);

  rv = decrypt(dest, aead, &ckm->aead_ctx, payload, payloadlen, nonce,
               ckm->iv.len, ad, adlen);

  if (rv != 0) {
    if (rv == NGTCP2_ERR_TLS_DECRYPT) {
      return rv;
    }
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  assert(payloadlen >= aead->max_overhead);

  return (ngtcp2_ssize)(payloadlen - aead->max_overhead);
}

/*
 * decrypt_hp decryptes packet header.  The packet number starts at
 * |pkt| + |pkt_num_offset|.  The entire plaintext QUIC packet header
 * will be written to the buffer pointed by |dest| whose capacity is
 * |destlen|.
 *
 * This function returns the number of bytes written to |dest|, or one
 * of the following negative error codes:
 *
 * NGTCP2_ERR_PROTO
 *     Packet is badly formatted
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed; or it does not return
 *     expected result.
 */
static ngtcp2_ssize decrypt_hp(ngtcp2_pkt_hd *hd, uint8_t *dest,
                               const ngtcp2_crypto_cipher *hp,
                               const uint8_t *pkt, size_t pktlen,
                               size_t pkt_num_offset, ngtcp2_crypto_km *ckm,
                               const ngtcp2_crypto_cipher_ctx *hp_ctx,
                               ngtcp2_hp_mask hp_mask) {
  size_t sample_offset;
  uint8_t *p = dest;
  uint8_t mask[NGTCP2_HP_MASKLEN];
  size_t i;
  int rv;

  assert(hp_mask);
  assert(ckm);

  if (pkt_num_offset + 4 + NGTCP2_HP_SAMPLELEN > pktlen) {
    return NGTCP2_ERR_PROTO;
  }

  p = ngtcp2_cpymem(p, pkt, pkt_num_offset);

  sample_offset = pkt_num_offset + 4;

  rv = hp_mask(mask, hp, hp_ctx, pkt + sample_offset);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  if (hd->flags & NGTCP2_PKT_FLAG_LONG_FORM) {
    dest[0] = (uint8_t)(dest[0] ^ (mask[0] & 0x0f));
  } else {
    dest[0] = (uint8_t)(dest[0] ^ (mask[0] & 0x1f));
    if (dest[0] & NGTCP2_SHORT_KEY_PHASE_BIT) {
      hd->flags |= NGTCP2_PKT_FLAG_KEY_PHASE;
    }
  }

  hd->pkt_numlen = (size_t)((dest[0] & NGTCP2_PKT_NUMLEN_MASK) + 1);

  for (i = 0; i < hd->pkt_numlen; ++i) {
    *p++ = *(pkt + pkt_num_offset + i) ^ mask[i + 1];
  }

  hd->pkt_num = ngtcp2_get_pkt_num(p - hd->pkt_numlen, hd->pkt_numlen);

  return p - dest;
}

/*
 * conn_emit_pending_crypto_data delivers pending stream data to the
 * application due to packet reordering.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed
 * NGTCP2_ERR_CRYPTO
 *     TLS backend reported error
 */
static int conn_emit_pending_crypto_data(ngtcp2_conn *conn,
                                         ngtcp2_crypto_level crypto_level,
                                         ngtcp2_strm *strm,
                                         uint64_t rx_offset) {
  size_t datalen;
  const uint8_t *data;
  int rv;
  uint64_t offset;

  if (!strm->rx.rob) {
    return 0;
  }

  for (;;) {
    datalen = ngtcp2_rob_data_at(strm->rx.rob, &data, rx_offset);
    if (datalen == 0) {
      assert(rx_offset == ngtcp2_strm_rx_offset(strm));
      return 0;
    }

    offset = rx_offset;
    rx_offset += datalen;

    rv = conn_call_recv_crypto_data(conn, crypto_level, offset, data, datalen);
    if (rv != 0) {
      return rv;
    }

    ngtcp2_rob_pop(strm->rx.rob, rx_offset - datalen, datalen);
  }
}

/*
 * conn_recv_connection_close is called when CONNECTION_CLOSE or
 * APPLICATION_CLOSE frame is received.
 */
static void conn_recv_connection_close(ngtcp2_conn *conn,
                                       ngtcp2_connection_close *fr) {
  conn->state = NGTCP2_CS_DRAINING;
  if (fr->type == NGTCP2_FRAME_CONNECTION_CLOSE) {
    conn->rx.ccec.type = NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_TRANSPORT;
  } else {
    conn->rx.ccec.type = NGTCP2_CONNECTION_CLOSE_ERROR_CODE_TYPE_APPLICATION;
  }
  conn->rx.ccec.error_code = fr->error_code;
}

static void conn_recv_path_challenge(ngtcp2_conn *conn, const ngtcp2_path *path,
                                     ngtcp2_path_challenge *fr) {
  ngtcp2_path_challenge_entry *ent;

  /* client only responds to PATH_CHALLENGE from the current path. */
  if (!conn->server && !ngtcp2_path_eq(&conn->dcid.current.ps.path, path)) {
    ngtcp2_log_info(
        &conn->log, NGTCP2_LOG_EVENT_CON,
        "discard PATH_CHALLENGE from the path which is not current");
    return;
  }

  ent = ngtcp2_ringbuf_push_front(&conn->rx.path_challenge);
  ngtcp2_path_challenge_entry_init(ent, path, fr->data);
}

/*
 * conn_reset_congestion_state resets congestion state.
 */
static void conn_reset_congestion_state(ngtcp2_conn *conn) {
  conn_reset_conn_stat_cc(conn, &conn->cstat);

  conn->cc.reset(&conn->cc);

  if (conn->hs_pktns) {
    ngtcp2_rtb_reset_cc_state(&conn->hs_pktns->rtb,
                              conn->hs_pktns->tx.last_pkt_num + 1);
  }
  ngtcp2_rtb_reset_cc_state(&conn->pktns.rtb, conn->pktns.tx.last_pkt_num + 1);
  ngtcp2_rst_init(&conn->rst);
}

static int conn_recv_path_response(ngtcp2_conn *conn, ngtcp2_path_response *fr,
                                   ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_duration pto, timeout;
  ngtcp2_pv *pv = conn->pv, *npv;
  uint8_t ent_flags;

  if (!pv) {
    return 0;
  }

  rv = ngtcp2_pv_validate(pv, &ent_flags, fr->data);
  if (rv != 0) {
    if (rv == NGTCP2_ERR_PATH_VALIDATION_FAILED) {
      return conn_on_path_validation_failed(conn, pv, ts);
    }
    return 0;
  }

  if (!(pv->flags & NGTCP2_PV_FLAG_DONT_CARE)) {
    if (!(pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE)) {
      if (pv->dcid.seq != conn->dcid.current.seq) {
        assert(conn->dcid.current.cid.datalen);

        rv = conn_retire_dcid(conn, &conn->dcid.current, ts);
        if (rv != 0) {
          goto fail;
        }
        ngtcp2_dcid_copy(&conn->dcid.current, &pv->dcid);
      }
      conn_reset_congestion_state(conn);
      conn_reset_ecn_validation_state(conn);
    }

    if (ngtcp2_path_eq(&pv->dcid.ps.path, &conn->dcid.current.ps.path)) {
      conn->dcid.current.flags |= NGTCP2_DCID_FLAG_PATH_VALIDATED;
    }

    rv = conn_call_path_validation(conn, &pv->dcid.ps.path,
                                   NGTCP2_PATH_VALIDATION_RESULT_SUCCESS);
    if (rv != 0) {
      goto fail;
    }
  }

  if (pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) {
    pto = conn_compute_pto(conn, &conn->pktns);
    timeout = 3 * ngtcp2_max(pto, pv->fallback_pto);

    if (ent_flags & NGTCP2_PV_ENTRY_FLAG_UNDERSIZED) {
      assert(conn->server);

      /* Validate path again */
      rv = ngtcp2_pv_new(&npv, &pv->dcid, timeout,
                         NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE |
                             NGTCP2_PV_FLAG_MTU_PROBE,
                         &conn->log, conn->mem);
      if (rv != 0) {
        goto fail;
      }

      npv->dcid.flags |= NGTCP2_DCID_FLAG_PATH_VALIDATED;
      ngtcp2_dcid_copy(&npv->fallback_dcid, &pv->fallback_dcid);
      npv->fallback_pto = pv->fallback_pto;
    } else {
      rv = ngtcp2_pv_new(&npv, &pv->fallback_dcid, timeout,
                         NGTCP2_PV_FLAG_DONT_CARE, &conn->log, conn->mem);
      if (rv != 0) {
        goto fail;
      }
    }

    /* Unset the flag bit so that conn_stop_pv does not retire
       DCID. */
    pv->flags &= (uint8_t)~NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE;

    rv = conn_stop_pv(conn, ts);
    if (rv != 0) {
      ngtcp2_pv_del(npv);
      return rv;
    }

    conn->pv = npv;

    return 0;
  }

fail:
  return conn_stop_pv(conn, ts);
}

/*
 * pkt_num_bits returns the number of bits available when packet
 * number is encoded in |pkt_numlen| bytes.
 */
static size_t pkt_num_bits(size_t pkt_numlen) {
  switch (pkt_numlen) {
  case 1:
    return 8;
  case 2:
    return 16;
  case 3:
    return 24;
  case 4:
    return 32;
  default:
    assert(0);
    abort();
  }
}

/*
 * pktns_pkt_num_is_duplicate returns nonzero if |pkt_num| is
 * duplicated packet number.
 */
static int pktns_pkt_num_is_duplicate(ngtcp2_pktns *pktns, int64_t pkt_num) {
  return ngtcp2_gaptr_is_pushed(&pktns->rx.pngap, (uint64_t)pkt_num, 1);
}

/*
 * pktns_commit_recv_pkt_num marks packet number |pkt_num| as
 * received.
 */
static int pktns_commit_recv_pkt_num(ngtcp2_pktns *pktns, int64_t pkt_num,
                                     int ack_eliciting, ngtcp2_tstamp ts) {
  int rv;

  if (ack_eliciting && pktns->rx.max_ack_eliciting_pkt_num + 1 != pkt_num) {
    ngtcp2_acktr_immediate_ack(&pktns->acktr);
  }
  if (pktns->rx.max_pkt_num < pkt_num) {
    pktns->rx.max_pkt_num = pkt_num;
    pktns->rx.max_pkt_ts = ts;
  }
  if (ack_eliciting && pktns->rx.max_ack_eliciting_pkt_num < pkt_num) {
    pktns->rx.max_ack_eliciting_pkt_num = pkt_num;
  }

  rv = ngtcp2_gaptr_push(&pktns->rx.pngap, (uint64_t)pkt_num, 1);
  if (rv != 0) {
    return rv;
  }

  if (ngtcp2_ksl_len(&pktns->rx.pngap.gap) > 256) {
    ngtcp2_gaptr_drop_first_gap(&pktns->rx.pngap);
  }

  return 0;
}

/*
 * verify_token verifies |hd| contains |token| in its token field.  It
 * returns 0 if it succeeds, or NGTCP2_ERR_PROTO.
 */
static int verify_token(const ngtcp2_vec *token, const ngtcp2_pkt_hd *hd) {
  if (token->len == hd->token.len &&
      ngtcp2_cmemeq(token->base, hd->token.base, token->len)) {
    return 0;
  }
  return NGTCP2_ERR_PROTO;
}

static void pktns_increase_ecn_counts(ngtcp2_pktns *pktns,
                                      const ngtcp2_pkt_info *pi) {
  switch (pi->ecn & NGTCP2_ECN_MASK) {
  case NGTCP2_ECN_ECT_0:
    ++pktns->rx.ecn.ect0;
    break;
  case NGTCP2_ECN_ECT_1:
    ++pktns->rx.ecn.ect1;
    break;
  case NGTCP2_ECN_CE:
    ++pktns->rx.ecn.ce;
    break;
  }
}

static int conn_recv_crypto(ngtcp2_conn *conn, ngtcp2_crypto_level crypto_level,
                            ngtcp2_strm *strm, const ngtcp2_crypto *fr);

static ngtcp2_ssize conn_recv_pkt(ngtcp2_conn *conn, const ngtcp2_path *path,
                                  const ngtcp2_pkt_info *pi, const uint8_t *pkt,
                                  size_t pktlen, size_t dgramlen,
                                  ngtcp2_tstamp pkt_ts, ngtcp2_tstamp ts);

static int conn_process_buffered_protected_pkt(ngtcp2_conn *conn,
                                               ngtcp2_pktns *pktns,
                                               ngtcp2_tstamp ts);

/*
 * conn_recv_handshake_pkt processes received packet |pkt| whose
 * length is |pktlen| during handshake period.  The buffer pointed by
 * |pkt| might contain multiple packets.  This function only processes
 * one packet.  |pkt_ts| is the timestamp when packet is received.
 * |ts| should be the current time.  Usually they are the same, but
 * for buffered packets, |pkt_ts| would be earlier than |ts|.
 *
 * This function returns the number of bytes it reads if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_RECV_VERSION_NEGOTIATION
 *     Version Negotiation packet is received.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_DISCARD_PKT
 *     Packet was discarded because plain text header was malformed;
 *     or its payload could not be decrypted.
 * NGTCP2_ERR_FRAME_FORMAT
 *     Frame is badly formatted
 * NGTCP2_ERR_ACK_FRAME
 *     ACK frame is malformed.
 * NGTCP2_ERR_CRYPTO
 *     TLS stack reported error.
 * NGTCP2_ERR_PROTO
 *     Generic QUIC protocol error.
 *
 * In addition to the above error codes, error codes returned from
 * conn_recv_pkt are also returned.
 */
static ngtcp2_ssize
conn_recv_handshake_pkt(ngtcp2_conn *conn, const ngtcp2_path *path,
                        const ngtcp2_pkt_info *pi, const uint8_t *pkt,
                        size_t pktlen, size_t dgramlen, ngtcp2_tstamp pkt_ts,
                        ngtcp2_tstamp ts) {
  ngtcp2_ssize nread;
  ngtcp2_pkt_hd hd;
  ngtcp2_max_frame mfr;
  ngtcp2_frame *fr = &mfr.fr;
  int rv;
  int require_ack = 0;
  size_t hdpktlen;
  const uint8_t *payload;
  size_t payloadlen;
  ngtcp2_ssize nwrite;
  ngtcp2_crypto_aead *aead;
  ngtcp2_crypto_cipher *hp;
  ngtcp2_crypto_km *ckm;
  ngtcp2_crypto_cipher_ctx *hp_ctx;
  ngtcp2_hp_mask hp_mask;
  ngtcp2_decrypt decrypt;
  ngtcp2_pktns *pktns;
  ngtcp2_strm *crypto;
  ngtcp2_crypto_level crypto_level;
  int invalid_reserved_bits = 0;

  if (pktlen == 0) {
    return 0;
  }

  if (!(pkt[0] & NGTCP2_HEADER_FORM_BIT)) {
    if (conn->state == NGTCP2_CS_SERVER_INITIAL) {
      /* Ignore Short packet unless server's first Handshake packet
         has been transmitted. */
      return (ngtcp2_ssize)pktlen;
    }

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                    "buffering Short packet len=%zu", pktlen);

    rv = conn_buffer_pkt(conn, &conn->pktns, path, pi, pkt, pktlen, dgramlen,
                         ts);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      return rv;
    }
    return (ngtcp2_ssize)pktlen;
  }

  nread = ngtcp2_pkt_decode_hd_long(&hd, pkt, pktlen);
  if (nread < 0) {
    return NGTCP2_ERR_DISCARD_PKT;
  }

  switch (hd.type) {
  case NGTCP2_PKT_VERSION_NEGOTIATION:
    hdpktlen = (size_t)nread;

    ngtcp2_log_rx_pkt_hd(&conn->log, &hd);

    if (conn->server) {
      return NGTCP2_ERR_DISCARD_PKT;
    }

    /* Receiving Version Negotiation packet after getting Handshake
       packet from server is invalid. */
    if (conn->flags & NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED) {
      return NGTCP2_ERR_DISCARD_PKT;
    }

    if (!ngtcp2_cid_eq(&conn->oscid, &hd.dcid)) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "packet was ignored because of mismatched DCID");
      return NGTCP2_ERR_DISCARD_PKT;
    }

    if (!ngtcp2_cid_eq(&conn->dcid.current.cid, &hd.scid)) {
      /* Just discard invalid Version Negotiation packet */
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "packet was ignored because of mismatched SCID");
      return NGTCP2_ERR_DISCARD_PKT;
    }
    rv = conn_on_version_negotiation(conn, &hd, pkt + hdpktlen,
                                     pktlen - hdpktlen);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      return NGTCP2_ERR_DISCARD_PKT;
    }
    return NGTCP2_ERR_RECV_VERSION_NEGOTIATION;
  case NGTCP2_PKT_RETRY:
    hdpktlen = (size_t)nread;

    ngtcp2_log_rx_pkt_hd(&conn->log, &hd);

    if (conn->server) {
      return NGTCP2_ERR_DISCARD_PKT;
    }

    /* Receiving Retry packet after getting Initial packet from server
       is invalid. */
    if (conn->flags & NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED) {
      return NGTCP2_ERR_DISCARD_PKT;
    }

    rv = conn_on_retry(conn, &hd, hdpktlen, pkt, pktlen);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      return NGTCP2_ERR_DISCARD_PKT;
    }
    return (ngtcp2_ssize)pktlen;
  }

  if (pktlen < (size_t)nread + hd.len) {
    return NGTCP2_ERR_DISCARD_PKT;
  }

  pktlen = (size_t)nread + hd.len;

  if (conn->version != hd.version) {
    return NGTCP2_ERR_DISCARD_PKT;
  }

  /* Quoted from spec: if subsequent packets of those types include a
     different Source Connection ID, they MUST be discarded. */
  if ((conn->flags & NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED) &&
      !ngtcp2_cid_eq(&conn->dcid.current.cid, &hd.scid)) {
    ngtcp2_log_rx_pkt_hd(&conn->log, &hd);
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "packet was ignored because of mismatched SCID");
    return NGTCP2_ERR_DISCARD_PKT;
  }

  switch (hd.type) {
  case NGTCP2_PKT_0RTT:
    if (!conn->server) {
      return NGTCP2_ERR_DISCARD_PKT;
    }
    if (conn->flags & NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED) {
      if (conn->early.ckm) {
        ngtcp2_ssize nread2;
        /* TODO Avoid to parse header twice. */
        nread2 =
            conn_recv_pkt(conn, path, pi, pkt, pktlen, dgramlen, pkt_ts, ts);
        if (nread2 < 0) {
          return nread2;
        }
      }

      /* Discard 0-RTT packet if we don't have a key to decrypt it. */
      return (ngtcp2_ssize)pktlen;
    }

    /* Buffer re-ordered 0-RTT packet. */
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                    "buffering 0-RTT packet len=%zu", pktlen);

    rv = conn_buffer_pkt(conn, conn->in_pktns, path, pi, pkt, pktlen, dgramlen,
                         ts);
    if (rv != 0) {
      assert(ngtcp2_err_is_fatal(rv));
      return rv;
    }

    return (ngtcp2_ssize)pktlen;
  case NGTCP2_PKT_INITIAL:
    if (!conn->in_pktns) {
      ngtcp2_log_info(
          &conn->log, NGTCP2_LOG_EVENT_PKT,
          "Initial packet is discarded because keys have been discarded");
      return (ngtcp2_ssize)pktlen;
    }

    assert(conn->in_pktns);

    if (conn->server) {
      if (conn->local.settings.token.len) {
        rv = verify_token(&conn->local.settings.token, &hd);
        if (rv != 0) {
          ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                          "packet was ignored because token is invalid");
          return NGTCP2_ERR_DISCARD_PKT;
        }
      }
      if ((conn->flags & NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED) == 0) {
        /* Set rcid here so that it is available to callback.  If this
           packet is discarded later in this function and no packet is
           processed in this connection attempt so far, connection
           will be dropped. */
        conn->rcid = hd.dcid;

        rv = conn_call_recv_client_initial(conn, &hd.dcid);
        if (rv != 0) {
          return rv;
        }
      }
    } else if (hd.token.len != 0) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "packet was ignored because token is not empty");
      return NGTCP2_ERR_DISCARD_PKT;
    }

    pktns = conn->in_pktns;
    crypto = &pktns->crypto.strm;
    crypto_level = NGTCP2_CRYPTO_LEVEL_INITIAL;

    break;
  case NGTCP2_PKT_HANDSHAKE:
    if (!conn->hs_pktns->crypto.rx.ckm) {
      if (conn->server) {
        ngtcp2_log_info(
            &conn->log, NGTCP2_LOG_EVENT_PKT,
            "Handshake packet at this point is unexpected and discarded");
        return (ngtcp2_ssize)pktlen;
      }
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                      "buffering Handshake packet len=%zu", pktlen);

      rv = conn_buffer_pkt(conn, conn->hs_pktns, path, pi, pkt, pktlen,
                           dgramlen, ts);
      if (rv != 0) {
        assert(ngtcp2_err_is_fatal(rv));
        return rv;
      }
      return (ngtcp2_ssize)pktlen;
    }

    pktns = conn->hs_pktns;
    crypto = &pktns->crypto.strm;
    crypto_level = NGTCP2_CRYPTO_LEVEL_HANDSHAKE;

    break;
  default:
    /* unknown packet type */
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "packet was ignored because of unknown packet type");
    return (ngtcp2_ssize)pktlen;
  }

  hp_mask = conn->callbacks.hp_mask;
  decrypt = conn->callbacks.decrypt;
  aead = &pktns->crypto.ctx.aead;
  hp = &pktns->crypto.ctx.hp;
  ckm = pktns->crypto.rx.ckm;
  hp_ctx = &pktns->crypto.rx.hp_ctx;

  assert(ckm);
  assert(hp_mask);
  assert(decrypt);

  rv = conn_ensure_decrypt_hp_buffer(conn, (size_t)nread + 4);
  if (rv != 0) {
    return rv;
  }

  nwrite = decrypt_hp(&hd, conn->crypto.decrypt_hp_buf.base, hp, pkt, pktlen,
                      (size_t)nread, ckm, hp_ctx, hp_mask);
  if (nwrite < 0) {
    if (ngtcp2_err_is_fatal((int)nwrite)) {
      return nwrite;
    }
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "could not decrypt packet number");
    return NGTCP2_ERR_DISCARD_PKT;
  }

  hdpktlen = (size_t)nwrite;
  payload = pkt + hdpktlen;
  payloadlen = hd.len - hd.pkt_numlen;

  hd.pkt_num = ngtcp2_pkt_adjust_pkt_num(pktns->rx.max_pkt_num, hd.pkt_num,
                                         pkt_num_bits(hd.pkt_numlen));
  if (hd.pkt_num > NGTCP2_MAX_PKT_NUM) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "pkn=%" PRId64 " is greater than maximum pkn", hd.pkt_num);
    return NGTCP2_ERR_DISCARD_PKT;
  }

  ngtcp2_log_rx_pkt_hd(&conn->log, &hd);

  rv = ngtcp2_pkt_verify_reserved_bits(conn->crypto.decrypt_hp_buf.base[0]);
  if (rv != 0) {
    invalid_reserved_bits = 1;

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "packet has incorrect reserved bits");

    /* Will return error after decrypting payload */
  }

  if (pktns_pkt_num_is_duplicate(pktns, hd.pkt_num)) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "packet was discarded because of duplicated packet number");
    return NGTCP2_ERR_DISCARD_PKT;
  }

  rv = conn_ensure_decrypt_buffer(conn, payloadlen);
  if (rv != 0) {
    return rv;
  }

  nwrite = decrypt_pkt(conn->crypto.decrypt_buf.base, aead, payload, payloadlen,
                       conn->crypto.decrypt_hp_buf.base, hdpktlen, hd.pkt_num,
                       ckm, decrypt);
  if (nwrite < 0) {
    if (ngtcp2_err_is_fatal((int)nwrite)) {
      return nwrite;
    }
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "could not decrypt packet payload");
    return NGTCP2_ERR_DISCARD_PKT;
  }

  if (invalid_reserved_bits) {
    return NGTCP2_ERR_PROTO;
  }

  payload = conn->crypto.decrypt_buf.base;
  payloadlen = (size_t)nwrite;

  switch (hd.type) {
  case NGTCP2_PKT_INITIAL:
    if (!conn->server || ((conn->flags & NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED) &&
                          !ngtcp2_cid_eq(&conn->rcid, &hd.dcid))) {
      rv = conn_verify_dcid(conn, NULL, &hd);
      if (rv != 0) {
        if (ngtcp2_err_is_fatal(rv)) {
          return rv;
        }
        ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                        "packet was ignored because of mismatched DCID");
        return NGTCP2_ERR_DISCARD_PKT;
      }
    }
    break;
  case NGTCP2_PKT_HANDSHAKE:
    rv = conn_verify_dcid(conn, NULL, &hd);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "packet was ignored because of mismatched DCID");
      return NGTCP2_ERR_DISCARD_PKT;
    }
    break;
  default:
    assert(0);
  }

  if (payloadlen == 0) {
    /* QUIC packet must contain at least one frame */
    if (hd.type == NGTCP2_PKT_INITIAL) {
      return NGTCP2_ERR_DISCARD_PKT;
    }
    return NGTCP2_ERR_PROTO;
  }

  if (hd.type == NGTCP2_PKT_INITIAL &&
      !(conn->flags & NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED)) {
    conn->flags |= NGTCP2_CONN_FLAG_CONN_ID_NEGOTIATED;
    if (!conn->server) {
      conn->dcid.current.cid = hd.scid;
    }
  }

  ngtcp2_qlog_pkt_received_start(&conn->qlog);

  for (; payloadlen;) {
    nread = ngtcp2_pkt_decode_frame(fr, payload, payloadlen);
    if (nread < 0) {
      return nread;
    }

    payload += nread;
    payloadlen -= (size_t)nread;

    switch (fr->type) {
    case NGTCP2_FRAME_ACK:
    case NGTCP2_FRAME_ACK_ECN:
      fr->ack.ack_delay = 0;
      fr->ack.ack_delay_unscaled = 0;
      break;
    }

    ngtcp2_log_rx_fr(&conn->log, &hd, fr);

    switch (fr->type) {
    case NGTCP2_FRAME_ACK:
    case NGTCP2_FRAME_ACK_ECN:
      if (!conn->server && hd.type == NGTCP2_PKT_HANDSHAKE) {
        conn->flags |= NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED;
      }
      rv = conn_recv_ack(conn, pktns, &fr->ack, pkt_ts, ts);
      if (rv != 0) {
        return rv;
      }
      break;
    case NGTCP2_FRAME_PADDING:
      break;
    case NGTCP2_FRAME_CRYPTO:
      rv = conn_recv_crypto(conn, crypto_level, crypto, &fr->crypto);
      if (rv != 0) {
        return rv;
      }
      require_ack = 1;
      break;
    case NGTCP2_FRAME_CONNECTION_CLOSE:
      conn_recv_connection_close(conn, &fr->connection_close);
      break;
    case NGTCP2_FRAME_PING:
      require_ack = 1;
      break;
    default:
      return NGTCP2_ERR_PROTO;
    }

    ngtcp2_qlog_write_frame(&conn->qlog, fr);
  }

  if (conn->server && hd.type == NGTCP2_PKT_HANDSHAKE) {
    /* Successful processing of Handshake packet from client verifies
       source address. */
    conn->dcid.current.flags |= NGTCP2_DCID_FLAG_PATH_VALIDATED;
  }

  ngtcp2_qlog_pkt_received_end(&conn->qlog, &hd, pktlen);

  rv = pktns_commit_recv_pkt_num(pktns, hd.pkt_num, require_ack, pkt_ts);
  if (rv != 0) {
    return rv;
  }

  pktns_increase_ecn_counts(pktns, pi);

  /* TODO Initial and Handshake are always acknowledged without
     delay. */
  if (require_ack &&
      (++pktns->acktr.rx_npkt >= conn->local.settings.ack_thresh ||
       (pi->ecn & NGTCP2_ECN_MASK) == NGTCP2_ECN_CE)) {
    ngtcp2_acktr_immediate_ack(&pktns->acktr);
  }

  rv = ngtcp2_conn_sched_ack(conn, &pktns->acktr, hd.pkt_num, require_ack,
                             pkt_ts);
  if (rv != 0) {
    return rv;
  }

  conn_restart_timer_on_read(conn, ts);

  ngtcp2_qlog_metrics_updated(&conn->qlog, &conn->cstat);

  return conn->state == NGTCP2_CS_DRAINING ? NGTCP2_ERR_DRAINING
                                           : (ngtcp2_ssize)pktlen;
}

/*
 * conn_recv_handshake_cpkt processes compound packet during
 * handshake.  The buffer pointed by |pkt| might contain multiple
 * packets.  The Short packet must be the last one because it does not
 * have payload length field.
 *
 * This function returns the same error code returned by
 * conn_recv_handshake_pkt.
 */
static int conn_recv_handshake_cpkt(ngtcp2_conn *conn, const ngtcp2_path *path,
                                    const ngtcp2_pkt_info *pi,
                                    const uint8_t *pkt, size_t pktlen,
                                    ngtcp2_tstamp ts) {
  ngtcp2_ssize nread;
  size_t dgramlen = pktlen;

  if (ngtcp2_path_eq(&conn->dcid.current.ps.path, path)) {
    conn->dcid.current.bytes_recv += dgramlen;
  }

  while (pktlen) {
    nread =
        conn_recv_handshake_pkt(conn, path, pi, pkt, pktlen, dgramlen, ts, ts);
    if (nread < 0) {
      if (ngtcp2_err_is_fatal((int)nread)) {
        return (int)nread;
      }

      if (nread == NGTCP2_ERR_DRAINING) {
        return NGTCP2_ERR_DRAINING;
      }

      if ((pkt[0] & NGTCP2_HEADER_FORM_BIT) &&
          /* Not a Version Negotiation packet */
          pktlen > 4 && ngtcp2_get_uint32(&pkt[1]) > 0 &&
          ngtcp2_pkt_get_type_long(pkt[0]) == NGTCP2_PKT_INITIAL) {
        if (conn->server) {
          /* If server discards first Initial, then drop connection
             state.  This is because SCID in packet might be corrupted
             and the current connection state might wrongly discard
             valid packet and prevent the handshake from
             completing. */
          if (conn->in_pktns && conn->in_pktns->rx.max_pkt_num == -1) {
            /* If this is crypto related error, then return normally
               in order to send CONNECTION_CLOSE with TLS alert (e.g.,
               no_application_protocol). */
            switch (nread) {
            case NGTCP2_ERR_CRYPTO:
            case NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM:
            case NGTCP2_ERR_MALFORMED_TRANSPORT_PARAM:
            case NGTCP2_ERR_TRANSPORT_PARAM:
              return (int)nread;
            }

            return NGTCP2_ERR_DROP_CONN;
          }
          return 0;
        }
        /* client */
        if (nread == NGTCP2_ERR_CRYPTO) {
          /* If client gets crypto error from TLS stack, it is
             unrecoverable, therefore drop connection. */
          return (int)nread;
        }
        return 0;
      }

      if (nread == NGTCP2_ERR_DISCARD_PKT) {
        return 0;
      }

      return (int)nread;
    }

    assert(pktlen >= (size_t)nread);
    pkt += nread;
    pktlen -= (size_t)nread;

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "read packet %td left %zu", nread, pktlen);
  }

  return 0;
}

int ngtcp2_conn_init_stream(ngtcp2_conn *conn, ngtcp2_strm *strm,
                            int64_t stream_id, void *stream_user_data) {
  int rv;
  uint64_t max_rx_offset;
  uint64_t max_tx_offset;
  int local_stream = conn_local_stream(conn, stream_id);

  if (bidi_stream(stream_id)) {
    if (local_stream) {
      max_rx_offset =
          conn->local.transport_params.initial_max_stream_data_bidi_local;
      max_tx_offset =
          conn->remote.transport_params.initial_max_stream_data_bidi_remote;
    } else {
      max_rx_offset =
          conn->local.transport_params.initial_max_stream_data_bidi_remote;
      max_tx_offset =
          conn->remote.transport_params.initial_max_stream_data_bidi_local;
    }
  } else if (local_stream) {
    max_rx_offset = 0;
    max_tx_offset = conn->remote.transport_params.initial_max_stream_data_uni;
  } else {
    max_rx_offset = conn->local.transport_params.initial_max_stream_data_uni;
    max_tx_offset = 0;
  }

  rv = ngtcp2_strm_init(strm, stream_id, NGTCP2_STRM_FLAG_NONE, max_rx_offset,
                        max_tx_offset, stream_user_data, conn->mem);
  if (rv != 0) {
    return rv;
  }

  rv = ngtcp2_map_insert(&conn->strms, &strm->me);
  if (rv != 0) {
    assert(rv != NGTCP2_ERR_INVALID_ARGUMENT);
    goto fail;
  }

  if (!conn_local_stream(conn, stream_id)) {
    rv = conn_call_stream_open(conn, strm);
    if (rv != 0) {
      goto fail;
    }
  }

  return 0;

fail:
  ngtcp2_strm_free(strm);
  return rv;
}

/*
 * conn_emit_pending_stream_data passes buffered ordered stream data
 * to the application.  |rx_offset| is the first offset to deliver to
 * the application.  This function assumes that the data up to
 * |rx_offset| has been delivered already.  This function only passes
 * the ordered data without any gap.  If there is a gap, it stops
 * providing the data to the application, and returns.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_emit_pending_stream_data(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                         uint64_t rx_offset) {
  size_t datalen;
  const uint8_t *data;
  int rv;
  uint64_t offset;
  uint32_t sdflags;
  int handshake_completed = conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED;

  if (!strm->rx.rob) {
    return 0;
  }

  for (;;) {
    /* Stop calling callback if application has called
       ngtcp2_conn_shutdown_stream_read() inside the callback.
       Because it doubly counts connection window. */
    if (strm->flags & (NGTCP2_STRM_FLAG_STOP_SENDING)) {
      return 0;
    }

    datalen = ngtcp2_rob_data_at(strm->rx.rob, &data, rx_offset);
    if (datalen == 0) {
      assert(rx_offset == ngtcp2_strm_rx_offset(strm));
      return 0;
    }

    offset = rx_offset;
    rx_offset += datalen;

    sdflags = NGTCP2_STREAM_DATA_FLAG_NONE;
    if ((strm->flags & NGTCP2_STRM_FLAG_SHUT_RD) &&
        rx_offset == strm->rx.last_offset) {
      sdflags |= NGTCP2_STREAM_DATA_FLAG_FIN;
    }
    if (!handshake_completed) {
      sdflags |= NGTCP2_STREAM_DATA_FLAG_EARLY;
    }

    rv = conn_call_recv_stream_data(conn, strm, sdflags, offset, data, datalen);
    if (rv != 0) {
      return rv;
    }

    ngtcp2_rob_pop(strm->rx.rob, rx_offset - datalen, datalen);
  }
}

/*
 * conn_recv_crypto is called when CRYPTO frame |fr| is received.
 * |rx_offset_base| is the offset in the entire TLS handshake stream.
 * fr->offset specifies the offset in each encryption level.
 * |max_rx_offset| is, if it is nonzero, the maximum offset in the
 * entire TLS handshake stream that |fr| can carry.  |crypto_level| is
 * the encryption level where this data is received.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_PROTO
 *     CRYPTO frame has invalid offset.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CRYPTO
 *     TLS stack reported error.
 * NGTCP2_ERR_FRAME_ENCODING
 *     The end offset exceeds the maximum value.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_recv_crypto(ngtcp2_conn *conn, ngtcp2_crypto_level crypto_level,
                            ngtcp2_strm *crypto, const ngtcp2_crypto *fr) {
  uint64_t fr_end_offset;
  uint64_t rx_offset;
  int rv;

  if (fr->datacnt == 0) {
    return 0;
  }

  fr_end_offset = fr->offset + fr->data[0].len;

  if (NGTCP2_MAX_VARINT < fr_end_offset) {
    return NGTCP2_ERR_FRAME_ENCODING;
  }

  rx_offset = ngtcp2_strm_rx_offset(crypto);

  if (fr_end_offset <= rx_offset) {
    if (conn->server &&
        !(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_EARLY_RETRANSMIT) &&
        crypto_level == NGTCP2_CRYPTO_LEVEL_INITIAL) {
      /* recovery draft: Speeding Up Handshake Completion

         When a server receives an Initial packet containing duplicate
         CRYPTO data, it can assume the client did not receive all of
         the server's CRYPTO data sent in Initial packets, or the
         client's estimated RTT is too small.  ...  To speed up
         handshake completion under these conditions, an endpoint MAY
         send a packet containing unacknowledged CRYPTO data earlier
         than the PTO expiry, subject to address validation limits;
         ... */
      conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_EARLY_RETRANSMIT;
      conn->in_pktns->rtb.probe_pkt_left = 1;
      conn->hs_pktns->rtb.probe_pkt_left = 1;
    }
    return 0;
  }

  crypto->rx.last_offset = ngtcp2_max(crypto->rx.last_offset, fr_end_offset);

  /* TODO Before dispatching incoming data to TLS stack, make sure
     that previous data in previous encryption level has been
     completely sent to TLS stack.  Usually, if data is left, it is an
     error because key is generated after consuming all data in the
     previous encryption level. */
  if (fr->offset <= rx_offset) {
    size_t ncut = (size_t)(rx_offset - fr->offset);
    const uint8_t *data = fr->data[0].base + ncut;
    size_t datalen = fr->data[0].len - ncut;
    uint64_t offset = rx_offset;

    rx_offset += datalen;
    rv = ngtcp2_strm_update_rx_offset(crypto, rx_offset);
    if (rv != 0) {
      return rv;
    }

    rv = conn_call_recv_crypto_data(conn, crypto_level, offset, data, datalen);
    if (rv != 0) {
      return rv;
    }

    rv = conn_emit_pending_crypto_data(conn, crypto_level, crypto, rx_offset);
    if (rv != 0) {
      return rv;
    }

    return 0;
  }

  if (fr_end_offset - rx_offset > NGTCP2_MAX_REORDERED_CRYPTO_DATA) {
    return NGTCP2_ERR_CRYPTO_BUFFER_EXCEEDED;
  }

  return ngtcp2_strm_recv_reordering(crypto, fr->data[0].base, fr->data[0].len,
                                     fr->offset);
}

/*
 * conn_max_data_violated returns nonzero if receiving |datalen|
 * violates connection flow control on local endpoint.
 */
static int conn_max_data_violated(ngtcp2_conn *conn, uint64_t datalen) {
  return conn->rx.max_offset - conn->rx.offset < datalen;
}

/*
 * conn_recv_stream is called when STREAM frame |fr| is received.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_STREAM_STATE
 *     STREAM frame is received for a local stream which is not
 *     initiated; or STREAM frame is received for a local
 *     unidirectional stream
 * NGTCP2_ERR_STREAM_LIMIT
 *     STREAM frame has remote stream ID which is strictly greater
 *     than the allowed limit.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_FLOW_CONTROL
 *     Flow control limit is violated; or the end offset of stream
 *     data is beyond the NGTCP2_MAX_VARINT.
 * NGTCP2_ERR_FINAL_SIZE
 *     STREAM frame has strictly larger end offset than it is
 *     permitted.
 */
static int conn_recv_stream(ngtcp2_conn *conn, const ngtcp2_stream *fr) {
  int rv;
  ngtcp2_strm *strm;
  ngtcp2_idtr *idtr;
  uint64_t rx_offset, fr_end_offset;
  int local_stream;
  int bidi;
  size_t datalen = ngtcp2_vec_len(fr->data, fr->datacnt);
  uint32_t sdflags = NGTCP2_STREAM_DATA_FLAG_NONE;

  local_stream = conn_local_stream(conn, fr->stream_id);
  bidi = bidi_stream(fr->stream_id);

  if (bidi) {
    if (local_stream) {
      if (conn->local.bidi.next_stream_id <= fr->stream_id) {
        return NGTCP2_ERR_STREAM_STATE;
      }
    } else if (conn->remote.bidi.max_streams <
               ngtcp2_ord_stream_id(fr->stream_id)) {
      return NGTCP2_ERR_STREAM_LIMIT;
    }

    idtr = &conn->remote.bidi.idtr;
  } else {
    if (local_stream) {
      return NGTCP2_ERR_STREAM_STATE;
    }
    if (conn->remote.uni.max_streams < ngtcp2_ord_stream_id(fr->stream_id)) {
      return NGTCP2_ERR_STREAM_LIMIT;
    }

    idtr = &conn->remote.uni.idtr;
  }

  if (NGTCP2_MAX_VARINT - datalen < fr->offset) {
    return NGTCP2_ERR_FLOW_CONTROL;
  }

  strm = ngtcp2_conn_find_stream(conn, fr->stream_id);
  if (strm == NULL) {
    if (local_stream) {
      /* TODO The stream has been closed.  This should be responded
         with RESET_STREAM, or simply ignored. */
      return 0;
    }

    rv = ngtcp2_idtr_open(idtr, fr->stream_id);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      assert(rv == NGTCP2_ERR_STREAM_IN_USE);
      /* TODO The stream has been closed.  This should be responded
         with RESET_STREAM, or simply ignored. */
      return 0;
    }

    strm = ngtcp2_mem_malloc(conn->mem, sizeof(ngtcp2_strm));
    if (strm == NULL) {
      return NGTCP2_ERR_NOMEM;
    }
    /* TODO Perhaps, call new_stream callback? */
    rv = ngtcp2_conn_init_stream(conn, strm, fr->stream_id, NULL);
    if (rv != 0) {
      ngtcp2_mem_free(conn->mem, strm);
      return rv;
    }
    if (!bidi) {
      ngtcp2_strm_shutdown(strm, NGTCP2_STRM_FLAG_SHUT_WR);
    }
  }

  fr_end_offset = fr->offset + datalen;

  if (strm->rx.max_offset < fr_end_offset) {
    return NGTCP2_ERR_FLOW_CONTROL;
  }

  if (strm->rx.last_offset < fr_end_offset) {
    uint64_t len = fr_end_offset - strm->rx.last_offset;

    if (conn_max_data_violated(conn, len)) {
      return NGTCP2_ERR_FLOW_CONTROL;
    }

    conn->rx.offset += len;

    if (strm->flags & NGTCP2_STRM_FLAG_STOP_SENDING) {
      ngtcp2_conn_extend_max_offset(conn, len);
    }
  }

  rx_offset = ngtcp2_strm_rx_offset(strm);

  if (fr->fin) {
    if (strm->flags & NGTCP2_STRM_FLAG_SHUT_RD) {
      if (strm->rx.last_offset != fr_end_offset) {
        return NGTCP2_ERR_FINAL_SIZE;
      }

      if (strm->flags &
          (NGTCP2_STRM_FLAG_STOP_SENDING | NGTCP2_STRM_FLAG_RECV_RST)) {
        return 0;
      }

      if (rx_offset == fr_end_offset) {
        return 0;
      }
    } else if (strm->rx.last_offset > fr_end_offset) {
      return NGTCP2_ERR_FINAL_SIZE;
    } else {
      strm->rx.last_offset = fr_end_offset;

      ngtcp2_strm_shutdown(strm, NGTCP2_STRM_FLAG_SHUT_RD);

      if (strm->flags & NGTCP2_STRM_FLAG_STOP_SENDING) {
        return ngtcp2_conn_close_stream_if_shut_rdwr(conn, strm,
                                                     strm->app_error_code);
      }
    }
  } else {
    if ((strm->flags & NGTCP2_STRM_FLAG_SHUT_RD) &&
        strm->rx.last_offset < fr_end_offset) {
      return NGTCP2_ERR_FINAL_SIZE;
    }

    strm->rx.last_offset = ngtcp2_max(strm->rx.last_offset, fr_end_offset);

    if (fr_end_offset <= rx_offset) {
      return 0;
    }

    if (strm->flags &
        (NGTCP2_STRM_FLAG_STOP_SENDING | NGTCP2_STRM_FLAG_RECV_RST)) {
      return 0;
    }
  }

  if (fr->offset <= rx_offset) {
    size_t ncut = (size_t)(rx_offset - fr->offset);
    uint64_t offset = rx_offset;
    const uint8_t *data;
    int fin;

    if (fr->datacnt) {
      data = fr->data[0].base + ncut;
      datalen -= ncut;

      rx_offset += datalen;
      rv = ngtcp2_strm_update_rx_offset(strm, rx_offset);
      if (rv != 0) {
        return rv;
      }
    } else {
      data = NULL;
      datalen = 0;
    }

    fin = (strm->flags & NGTCP2_STRM_FLAG_SHUT_RD) &&
          rx_offset == strm->rx.last_offset;

    if (fin || datalen) {
      if (fin) {
        sdflags |= NGTCP2_STREAM_DATA_FLAG_FIN;
      }
      if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
        sdflags |= NGTCP2_STREAM_DATA_FLAG_EARLY;
      }
      rv = conn_call_recv_stream_data(conn, strm, sdflags, offset, data,
                                      datalen);
      if (rv != 0) {
        return rv;
      }

      rv = conn_emit_pending_stream_data(conn, strm, rx_offset);
      if (rv != 0) {
        return rv;
      }
    }
  } else if (fr->datacnt) {
    rv = ngtcp2_strm_recv_reordering(strm, fr->data[0].base, fr->data[0].len,
                                     fr->offset);
    if (rv != 0) {
      return rv;
    }
  }
  return ngtcp2_conn_close_stream_if_shut_rdwr(conn, strm, NGTCP2_NO_ERROR);
}

/*
 * conn_reset_stream adds RESET_STREAM frame to the transmission
 * queue.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_reset_stream(ngtcp2_conn *conn, ngtcp2_strm *strm,
                             uint64_t app_error_code) {
  int rv;
  ngtcp2_frame_chain *frc;
  ngtcp2_pktns *pktns = &conn->pktns;

  rv = ngtcp2_frame_chain_new(&frc, conn->mem);
  if (rv != 0) {
    return rv;
  }

  frc->fr.type = NGTCP2_FRAME_RESET_STREAM;
  frc->fr.reset_stream.stream_id = strm->stream_id;
  frc->fr.reset_stream.app_error_code = app_error_code;
  frc->fr.reset_stream.final_size = strm->tx.offset;

  /* TODO This prepends RESET_STREAM to pktns->tx.frq. */
  frc->next = pktns->tx.frq;
  pktns->tx.frq = frc;

  return 0;
}

/*
 * conn_stop_sending adds STOP_SENDING frame to the transmission
 * queue.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_stop_sending(ngtcp2_conn *conn, ngtcp2_strm *strm,
                             uint64_t app_error_code) {
  int rv;
  ngtcp2_frame_chain *frc;
  ngtcp2_pktns *pktns = &conn->pktns;

  rv = ngtcp2_frame_chain_new(&frc, conn->mem);
  if (rv != 0) {
    return rv;
  }

  frc->fr.type = NGTCP2_FRAME_STOP_SENDING;
  frc->fr.stop_sending.stream_id = strm->stream_id;
  frc->fr.stop_sending.app_error_code = app_error_code;

  /* TODO This prepends STOP_SENDING to pktns->tx.frq. */
  frc->next = pktns->tx.frq;
  pktns->tx.frq = frc;

  return 0;
}

/*
 * handle_max_remote_streams_extension extends
 * |*punsent_max_remote_streams| by |n| if a condition allows it.
 */
static void
handle_max_remote_streams_extension(uint64_t *punsent_max_remote_streams,
                                    size_t n) {
  if (
#if SIZE_MAX > UINT32_MAX
      NGTCP2_MAX_STREAMS < n ||
#endif /* SIZE_MAX > UINT32_MAX */
      *punsent_max_remote_streams > (uint64_t)(NGTCP2_MAX_STREAMS - n)) {
    *punsent_max_remote_streams = NGTCP2_MAX_STREAMS;
  } else {
    *punsent_max_remote_streams += n;
  }
}

/*
 * conn_recv_reset_stream is called when RESET_STREAM |fr| is
 * received.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_STREAM_STATE
 *     RESET_STREAM frame is received to the local stream which is not
 *     initiated.
 * NGTCP2_ERR_STREAM_LIMIT
 *     RESET_STREAM frame has remote stream ID which is strictly
 *     greater than the allowed limit.
 * NGTCP2_ERR_PROTO
 *     RESET_STREAM frame is received to the local unidirectional
 *     stream
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_FLOW_CONTROL
 *     Flow control limit is violated; or the final size is beyond the
 *     NGTCP2_MAX_VARINT.
 * NGTCP2_ERR_FINAL_SIZE
 *     The final offset is strictly larger than it is permitted.
 */
static int conn_recv_reset_stream(ngtcp2_conn *conn,
                                  const ngtcp2_reset_stream *fr) {
  ngtcp2_strm *strm;
  int local_stream = conn_local_stream(conn, fr->stream_id);
  int bidi = bidi_stream(fr->stream_id);
  uint64_t datalen;
  ngtcp2_idtr *idtr;
  int rv;

  /* TODO share this piece of code */
  if (bidi) {
    if (local_stream) {
      if (conn->local.bidi.next_stream_id <= fr->stream_id) {
        return NGTCP2_ERR_STREAM_STATE;
      }
    } else if (conn->remote.bidi.max_streams <
               ngtcp2_ord_stream_id(fr->stream_id)) {
      return NGTCP2_ERR_STREAM_LIMIT;
    }

    idtr = &conn->remote.bidi.idtr;
  } else {
    if (local_stream) {
      return NGTCP2_ERR_PROTO;
    }
    if (conn->remote.uni.max_streams < ngtcp2_ord_stream_id(fr->stream_id)) {
      return NGTCP2_ERR_STREAM_LIMIT;
    }

    idtr = &conn->remote.uni.idtr;
  }

  if (NGTCP2_MAX_VARINT < fr->final_size) {
    return NGTCP2_ERR_FLOW_CONTROL;
  }

  strm = ngtcp2_conn_find_stream(conn, fr->stream_id);
  if (strm == NULL) {
    if (local_stream) {
      return 0;
    }

    rv = ngtcp2_idtr_open(idtr, fr->stream_id);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      assert(rv == NGTCP2_ERR_STREAM_IN_USE);
      return 0;
    }

    if (conn_initial_stream_rx_offset(conn, fr->stream_id) < fr->final_size ||
        conn_max_data_violated(conn, fr->final_size)) {
      return NGTCP2_ERR_FLOW_CONTROL;
    }

    /* Stream is reset before we create ngtcp2_strm object. */
    conn->rx.offset += fr->final_size;
    ngtcp2_conn_extend_max_offset(conn, fr->final_size);

    rv = conn_call_stream_reset(conn, fr->stream_id, fr->final_size,
                                fr->app_error_code, NULL);
    if (rv != 0) {
      return rv;
    }

    /* There will be no activity in this stream because we got
       RESET_STREAM and don't write stream data any further.  This
       effectively allows another new stream for peer. */
    if (bidi) {
      handle_max_remote_streams_extension(&conn->remote.bidi.unsent_max_streams,
                                          1);
    } else {
      handle_max_remote_streams_extension(&conn->remote.uni.unsent_max_streams,
                                          1);
    }

    return 0;
  }

  if ((strm->flags & NGTCP2_STRM_FLAG_SHUT_RD)) {
    if (strm->rx.last_offset != fr->final_size) {
      return NGTCP2_ERR_FINAL_SIZE;
    }
  } else if (strm->rx.last_offset > fr->final_size) {
    return NGTCP2_ERR_FINAL_SIZE;
  }

  datalen = fr->final_size - strm->rx.last_offset;

  if (strm->rx.max_offset < fr->final_size ||
      conn_max_data_violated(conn, datalen)) {
    return NGTCP2_ERR_FLOW_CONTROL;
  }

  if (!(strm->flags & NGTCP2_STRM_FLAG_RECV_RST)) {
    rv = conn_call_stream_reset(conn, fr->stream_id, fr->final_size,
                                fr->app_error_code, strm->stream_user_data);
    if (rv != 0) {
      return rv;
    }

    /* Extend connection flow control window for the amount of data
       which are not passed to application. */
    if (!(strm->flags & NGTCP2_STRM_FLAG_STOP_SENDING)) {
      ngtcp2_conn_extend_max_offset(conn, strm->rx.last_offset -
                                              ngtcp2_strm_rx_offset(strm));
    }
  }

  conn->rx.offset += datalen;
  ngtcp2_conn_extend_max_offset(conn, datalen);

  strm->rx.last_offset = fr->final_size;
  strm->flags |= NGTCP2_STRM_FLAG_SHUT_RD | NGTCP2_STRM_FLAG_RECV_RST;

  return ngtcp2_conn_close_stream_if_shut_rdwr(conn, strm, fr->app_error_code);
}

/*
 * conn_recv_stop_sending is called when STOP_SENDING |fr| is received.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_STREAM_STATE
 *     STOP_SENDING frame is received for a local stream which is not
 *     initiated; or STOP_SENDING frame is received for a local
 *     unidirectional stream.
 * NGTCP2_ERR_STREAM_LIMIT
 *     STOP_SENDING frame has remote stream ID which is strictly
 *     greater than the allowed limit.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_recv_stop_sending(ngtcp2_conn *conn,
                                  const ngtcp2_stop_sending *fr) {
  int rv;
  ngtcp2_strm *strm;
  ngtcp2_idtr *idtr;
  int local_stream = conn_local_stream(conn, fr->stream_id);
  int bidi = bidi_stream(fr->stream_id);

  if (bidi) {
    if (local_stream) {
      if (conn->local.bidi.next_stream_id <= fr->stream_id) {
        return NGTCP2_ERR_STREAM_STATE;
      }
    } else if (conn->remote.bidi.max_streams <
               ngtcp2_ord_stream_id(fr->stream_id)) {
      return NGTCP2_ERR_STREAM_LIMIT;
    }

    idtr = &conn->remote.bidi.idtr;
  } else {
    if (!local_stream || conn->local.uni.next_stream_id <= fr->stream_id) {
      return NGTCP2_ERR_STREAM_STATE;
    }

    idtr = &conn->remote.uni.idtr;
  }

  strm = ngtcp2_conn_find_stream(conn, fr->stream_id);
  if (strm == NULL) {
    if (local_stream) {
      return 0;
    }
    rv = ngtcp2_idtr_open(idtr, fr->stream_id);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      assert(rv == NGTCP2_ERR_STREAM_IN_USE);
      return 0;
    }

    /* Frame is received reset before we create ngtcp2_strm
       object. */
    strm = ngtcp2_mem_malloc(conn->mem, sizeof(ngtcp2_strm));
    if (strm == NULL) {
      return NGTCP2_ERR_NOMEM;
    }
    rv = ngtcp2_conn_init_stream(conn, strm, fr->stream_id, NULL);
    if (rv != 0) {
      ngtcp2_mem_free(conn->mem, strm);
      return rv;
    }
  }

  /* No RESET_STREAM is required if we have sent FIN and all data have
     been acknowledged. */
  if ((strm->flags & NGTCP2_STRM_FLAG_SHUT_WR) &&
      ngtcp2_strm_is_all_tx_data_acked(strm)) {
    return 0;
  }

  rv = conn_reset_stream(conn, strm, fr->app_error_code);
  if (rv != 0) {
    return rv;
  }

  strm->flags |= NGTCP2_STRM_FLAG_SHUT_WR | NGTCP2_STRM_FLAG_SENT_RST;

  ngtcp2_strm_streamfrq_clear(strm);

  return ngtcp2_conn_close_stream_if_shut_rdwr(conn, strm, fr->app_error_code);
}

/*
 * check_stateless_reset returns nonzero if Stateless Reset |sr|
 * coming via |path| is valid against |dcid|.
 */
static int check_stateless_reset(const ngtcp2_dcid *dcid,
                                 const ngtcp2_path *path,
                                 const ngtcp2_pkt_stateless_reset *sr) {
  return ngtcp2_path_eq(&dcid->ps.path, path) &&
         ngtcp2_verify_stateless_reset_token(dcid->token,
                                             sr->stateless_reset_token) == 0;
}

/*
 * conn_on_stateless_reset decodes Stateless Reset from the buffer
 * pointed by |payload| whose length is |payloadlen|.  |payload|
 * should start after first byte of packet.
 *
 * If Stateless Reset is decoded, and the Stateless Reset Token is
 * validated, the connection is closed.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_INVALID_ARGUMENT
 *     Could not decode Stateless Reset; or Stateless Reset Token does
 *     not match; or No stateless reset token is available.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed.
 */
static int conn_on_stateless_reset(ngtcp2_conn *conn, const ngtcp2_path *path,
                                   const uint8_t *payload, size_t payloadlen) {
  int rv = 1;
  ngtcp2_pv *pv = conn->pv;
  ngtcp2_dcid *dcid;
  ngtcp2_pkt_stateless_reset sr;
  size_t len, i;

  rv = ngtcp2_pkt_decode_stateless_reset(&sr, payload, payloadlen);
  if (rv != 0) {
    return rv;
  }

  if (!check_stateless_reset(&conn->dcid.current, path, &sr) &&
      (!pv || (!check_stateless_reset(&pv->dcid, path, &sr) &&
               (!(pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) ||
                !check_stateless_reset(&pv->fallback_dcid, path, &sr))))) {
    len = ngtcp2_ringbuf_len(&conn->dcid.retired);
    for (i = 0; i < len; ++i) {
      dcid = ngtcp2_ringbuf_get(&conn->dcid.retired, i);
      if (check_stateless_reset(dcid, path, &sr)) {
        break;
      }
    }

    if (i == len) {
      len = ngtcp2_ringbuf_len(&conn->dcid.bound);
      for (i = 0; i < len; ++i) {
        dcid = ngtcp2_ringbuf_get(&conn->dcid.bound, i);
        if (check_stateless_reset(dcid, path, &sr)) {
          break;
        }
      }

      if (i == len) {
        return NGTCP2_ERR_INVALID_ARGUMENT;
      }
    }
  }

  conn->state = NGTCP2_CS_DRAINING;

  ngtcp2_log_rx_sr(&conn->log, &sr);

  if (!conn->callbacks.recv_stateless_reset) {
    return 0;
  }

  rv = conn->callbacks.recv_stateless_reset(conn, &sr, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

/*
 * conn_recv_max_streams processes the incoming MAX_STREAMS frame
 * |fr|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed.
 * NGTCP2_ERR_FRAME_ENCODING
 *     The maximum streams field exceeds the maximum value.
 */
static int conn_recv_max_streams(ngtcp2_conn *conn,
                                 const ngtcp2_max_streams *fr) {
  uint64_t n;

  if (fr->max_streams > NGTCP2_MAX_STREAMS) {
    return NGTCP2_ERR_FRAME_ENCODING;
  }

  n = ngtcp2_min(fr->max_streams, NGTCP2_MAX_STREAMS);

  if (fr->type == NGTCP2_FRAME_MAX_STREAMS_BIDI) {
    if (conn->local.bidi.max_streams < n) {
      conn->local.bidi.max_streams = n;
      return conn_call_extend_max_local_streams_bidi(conn, n);
    }
    return 0;
  }

  if (conn->local.uni.max_streams < n) {
    conn->local.uni.max_streams = n;
    return conn_call_extend_max_local_streams_uni(conn, n);
  }
  return 0;
}

static int conn_retire_dcid_prior_to(ngtcp2_conn *conn, ngtcp2_ringbuf *rb,
                                     uint64_t retire_prior_to) {
  size_t i;
  ngtcp2_dcid *dcid, *last;
  int rv;

  for (i = 0; i < ngtcp2_ringbuf_len(rb);) {
    dcid = ngtcp2_ringbuf_get(rb, i);
    if (dcid->seq >= retire_prior_to) {
      ++i;
      continue;
    }

    rv = conn_retire_dcid_seq(conn, dcid->seq);
    if (rv != 0) {
      return rv;
    }
    if (i == 0) {
      ngtcp2_ringbuf_pop_front(rb);
    } else if (i == ngtcp2_ringbuf_len(rb) - 1) {
      ngtcp2_ringbuf_pop_back(rb);
      break;
    } else {
      last = ngtcp2_ringbuf_get(rb, ngtcp2_ringbuf_len(rb) - 1);
      ngtcp2_dcid_copy(dcid, last);
      ngtcp2_ringbuf_pop_back(rb);
    }
  }

  return 0;
}

/*
 * conn_recv_new_connection_id processes the incoming
 * NEW_CONNECTION_ID frame |fr|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_PROTO
 *     |fr| has the duplicated sequence number with different CID or
 *     token; or DCID is zero-length.
 */
static int conn_recv_new_connection_id(ngtcp2_conn *conn,
                                       const ngtcp2_new_connection_id *fr) {
  size_t i, len;
  ngtcp2_dcid *dcid;
  ngtcp2_pv *pv = conn->pv;
  int rv;
  int found = 0;
  size_t extra_dcid = 0;

  if (conn->dcid.current.cid.datalen == 0) {
    return NGTCP2_ERR_PROTO;
  }

  if (fr->retire_prior_to > fr->seq) {
    return NGTCP2_ERR_FRAME_ENCODING;
  }

  rv = ngtcp2_dcid_verify_uniqueness(&conn->dcid.current, fr->seq, &fr->cid,
                                     fr->stateless_reset_token);
  if (rv != 0) {
    return rv;
  }
  if (ngtcp2_cid_eq(&conn->dcid.current.cid, &fr->cid)) {
    found = 1;
  }

  if (pv) {
    rv = ngtcp2_dcid_verify_uniqueness(&pv->dcid, fr->seq, &fr->cid,
                                       fr->stateless_reset_token);
    if (rv != 0) {
      return rv;
    }
    if (ngtcp2_cid_eq(&pv->dcid.cid, &fr->cid)) {
      found = 1;
    }
  }

  len = ngtcp2_ringbuf_len(&conn->dcid.bound);

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.bound, i);
    rv = ngtcp2_dcid_verify_uniqueness(dcid, fr->seq, &fr->cid,
                                       fr->stateless_reset_token);
    if (rv != 0) {
      return NGTCP2_ERR_PROTO;
    }
    if (ngtcp2_cid_eq(&dcid->cid, &fr->cid)) {
      found = 1;
    }
  }

  len = ngtcp2_ringbuf_len(&conn->dcid.unused);

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.unused, i);
    rv = ngtcp2_dcid_verify_uniqueness(dcid, fr->seq, &fr->cid,
                                       fr->stateless_reset_token);
    if (rv != 0) {
      return NGTCP2_ERR_PROTO;
    }
    if (ngtcp2_cid_eq(&dcid->cid, &fr->cid)) {
      found = 1;
    }
  }

  if (conn->dcid.retire_prior_to < fr->retire_prior_to) {
    conn->dcid.retire_prior_to = fr->retire_prior_to;

    rv =
        conn_retire_dcid_prior_to(conn, &conn->dcid.bound, fr->retire_prior_to);
    if (rv != 0) {
      return rv;
    }

    rv = conn_retire_dcid_prior_to(conn, &conn->dcid.unused,
                                   conn->dcid.retire_prior_to);
    if (rv != 0) {
      return rv;
    }
  } else if (fr->seq < conn->dcid.retire_prior_to) {
    /* If packets are reordered, we might have retire_prior_to which
       is larger than fr->seq.

       A malicious peer might send crafted NEW_CONNECTION_ID to force
       local endpoint to create lots of RETIRE_CONNECTION_ID frames.
       For example, a peer might send seq = 50000 and retire_prior_to
       = 50000.  Then send NEW_CONNECTION_ID frames with seq <
       50000. */
    /* TODO we might queue lots of RETIRE_CONNECTION_ID frame here
       because conn->dcid.num_retire_queued is incremented when the
       frame is serialized. */
    if (conn->dcid.num_retire_queued < NGTCP2_MAX_DCID_POOL_SIZE * 2) {
      return conn_retire_dcid_seq(conn, fr->seq);
    }
    return 0;
  }

  if (found) {
    return 0;
  }

  if (ngtcp2_gaptr_is_pushed(&conn->dcid.seqgap, fr->seq, 1)) {
    return 0;
  }

  rv = ngtcp2_gaptr_push(&conn->dcid.seqgap, fr->seq, 1);
  if (rv != 0) {
    return rv;
  }

  if (ngtcp2_ksl_len(&conn->dcid.seqgap.gap) > 32) {
    ngtcp2_gaptr_drop_first_gap(&conn->dcid.seqgap);
  }

  len = ngtcp2_ringbuf_len(&conn->dcid.unused);

  if (conn->dcid.current.seq >= conn->dcid.retire_prior_to) {
    ++extra_dcid;
  }
  if (pv) {
    if (pv->dcid.seq != conn->dcid.current.seq &&
        pv->dcid.seq >= conn->dcid.retire_prior_to) {
      ++extra_dcid;
    }
    if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
        pv->fallback_dcid.seq != conn->dcid.current.seq &&
        pv->fallback_dcid.seq >= conn->dcid.retire_prior_to) {
      ++extra_dcid;
    }
  }

  if (conn->local.transport_params.active_connection_id_limit <=
      len + extra_dcid) {
    return NGTCP2_ERR_CONNECTION_ID_LIMIT;
  }

  if (len >= NGTCP2_MAX_DCID_POOL_SIZE) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON, "too many connection ID");
    return 0;
  }

  dcid = ngtcp2_ringbuf_push_back(&conn->dcid.unused);
  ngtcp2_dcid_init(dcid, fr->seq, &fr->cid, fr->stateless_reset_token);

  return 0;
}

/*
 * conn_post_process_recv_new_connection_id handles retirement request
 * of active DCIDs.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_post_process_recv_new_connection_id(ngtcp2_conn *conn,
                                                    ngtcp2_tstamp ts) {
  ngtcp2_pv *pv = conn->pv;
  ngtcp2_dcid *dcid;
  int rv;

  if (conn->dcid.current.seq < conn->dcid.retire_prior_to) {
    if (ngtcp2_ringbuf_len(&conn->dcid.unused) == 0) {
      return 0;
    }

    rv = conn_retire_dcid(conn, &conn->dcid.current, ts);
    if (rv != 0) {
      return rv;
    }

    dcid = ngtcp2_ringbuf_get(&conn->dcid.unused, 0);
    if (pv) {
      if (conn->dcid.current.seq == pv->dcid.seq) {
        ngtcp2_dcid_copy_cid_token(&pv->dcid, dcid);
      }
      if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
          conn->dcid.current.seq == pv->fallback_dcid.seq) {
        ngtcp2_dcid_copy_cid_token(&pv->fallback_dcid, dcid);
      }
    }

    ngtcp2_dcid_copy_cid_token(&conn->dcid.current, dcid);
    ngtcp2_ringbuf_pop_front(&conn->dcid.unused);

    rv = conn_call_activate_dcid(conn, &conn->dcid.current);
    if (rv != 0) {
      return rv;
    }
  }

  if (pv) {
    if (pv->dcid.seq < conn->dcid.retire_prior_to) {
      if (ngtcp2_ringbuf_len(&conn->dcid.unused)) {
        rv = conn_retire_dcid(conn, &pv->dcid, ts);
        if (rv != 0) {
          return rv;
        }

        dcid = ngtcp2_ringbuf_get(&conn->dcid.unused, 0);

        if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
            pv->dcid.seq == pv->fallback_dcid.seq) {
          ngtcp2_dcid_copy_cid_token(&pv->fallback_dcid, dcid);
        }

        ngtcp2_dcid_copy_cid_token(&pv->dcid, dcid);
        ngtcp2_ringbuf_pop_front(&conn->dcid.unused);

        rv = conn_call_activate_dcid(conn, &pv->dcid);
        if (rv != 0) {
          return rv;
        }
      } else {
        ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PTV,
                        "path migration is aborted because connection ID is"
                        "retired and no unused connection ID is available");

        return conn_stop_pv(conn, ts);
      }
    }
    if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
        pv->fallback_dcid.seq < conn->dcid.retire_prior_to) {
      if (ngtcp2_ringbuf_len(&conn->dcid.unused)) {
        rv = conn_retire_dcid(conn, &pv->fallback_dcid, ts);
        if (rv != 0) {
          return rv;
        }

        dcid = ngtcp2_ringbuf_get(&conn->dcid.unused, 0);
        ngtcp2_dcid_copy_cid_token(&pv->fallback_dcid, dcid);
        ngtcp2_ringbuf_pop_front(&conn->dcid.unused);

        rv = conn_call_activate_dcid(conn, &pv->fallback_dcid);
        if (rv != 0) {
          return rv;
        }
      } else {
        /* Now we have no fallback dcid. */
        return conn_stop_pv(conn, ts);
      }
    }
  }

  return 0;
}

/*
 * conn_recv_retire_connection_id processes the incoming
 * RETIRE_CONNECTION_ID frame |fr|.  |hd| is a packet header which
 * |fr| is included.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_PROTO
 *     SCID is zero-length.
 * NGTCP2_ERR_FRAME_ENCODING
 *     Attempt to retire CID which is used as DCID to send this frame.
 */
static int conn_recv_retire_connection_id(ngtcp2_conn *conn,
                                          const ngtcp2_pkt_hd *hd,
                                          const ngtcp2_retire_connection_id *fr,
                                          ngtcp2_tstamp ts) {
  ngtcp2_ksl_it it;
  ngtcp2_scid *scid;

  if (conn->oscid.datalen == 0 || conn->scid.last_seq < fr->seq) {
    return NGTCP2_ERR_PROTO;
  }

  for (it = ngtcp2_ksl_begin(&conn->scid.set); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    scid = ngtcp2_ksl_it_get(&it);
    if (scid->seq == fr->seq) {
      if (ngtcp2_cid_eq(&scid->cid, &hd->dcid)) {
        return NGTCP2_ERR_PROTO;
      }

      if (!(scid->flags & NGTCP2_SCID_FLAG_RETIRED)) {
        scid->flags |= NGTCP2_SCID_FLAG_RETIRED;
        ++conn->scid.num_retired;
      }

      if (scid->pe.index != NGTCP2_PQ_BAD_INDEX) {
        ngtcp2_pq_remove(&conn->scid.used, &scid->pe);
        scid->pe.index = NGTCP2_PQ_BAD_INDEX;
      }

      scid->ts_retired = ts;

      return ngtcp2_pq_push(&conn->scid.used, &scid->pe);
    }
  }

  return 0;
}

/*
 * conn_recv_new_token processes the incoming NEW_TOKEN frame |fr|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Token is empty
 * NGTCP2_ERR_PROTO:
 *     Server received NEW_TOKEN.
 */
static int conn_recv_new_token(ngtcp2_conn *conn, const ngtcp2_new_token *fr) {
  int rv;

  if (conn->server) {
    return NGTCP2_ERR_PROTO;
  }

  if (fr->token.len == 0) {
    return NGTCP2_ERR_FRAME_ENCODING;
  }

  if (conn->callbacks.recv_new_token) {
    rv = conn->callbacks.recv_new_token(conn, &fr->token, conn->user_data);
    if (rv != 0) {
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }
  }

  return 0;
}

/*
 * conn_recv_streams_blocked_bidi processes the incoming
 * STREAMS_BLOCKED (0x16).
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Maximum Streams is larger than advertised value.
 */
static int conn_recv_streams_blocked_bidi(ngtcp2_conn *conn,
                                          ngtcp2_streams_blocked *fr) {
  if (fr->max_streams > conn->remote.bidi.max_streams) {
    return NGTCP2_ERR_FRAME_ENCODING;
  }

  return 0;
}

/*
 * conn_recv_streams_blocked_uni processes the incoming
 * STREAMS_BLOCKED (0x17).
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Maximum Streams is larger than advertised value.
 */
static int conn_recv_streams_blocked_uni(ngtcp2_conn *conn,
                                         ngtcp2_streams_blocked *fr) {
  if (fr->max_streams > conn->remote.uni.max_streams) {
    return NGTCP2_ERR_FRAME_ENCODING;
  }

  return 0;
}

/*
 * conn_select_preferred_addr asks a client application to select a
 * server address from preferred addresses received from server.  If a
 * client chooses the address, path validation will start.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_select_preferred_addr(ngtcp2_conn *conn) {
  struct sockaddr_storage buf;
  ngtcp2_addr addr;
  int rv;
  ngtcp2_duration pto, initial_pto, timeout;
  ngtcp2_pv *pv;
  ngtcp2_dcid *dcid;

  ngtcp2_addr_init(&addr, (struct sockaddr *)&buf, 0, NULL);

  if (ngtcp2_ringbuf_len(&conn->dcid.unused) == 0) {
    return 0;
  }

  rv = conn_call_select_preferred_addr(conn, &addr);
  if (rv != 0) {
    return rv;
  }

  if (addr.addrlen == 0 ||
      ngtcp2_addr_eq(&conn->dcid.current.ps.path.remote, &addr)) {
    return 0;
  }

  assert(conn->pv == NULL);

  dcid = ngtcp2_ringbuf_get(&conn->dcid.unused, 0);
  pto = conn_compute_pto(conn, &conn->pktns);
  initial_pto = conn_compute_initial_pto(conn, &conn->pktns);
  timeout = 3 * ngtcp2_max(pto, initial_pto);

  rv = ngtcp2_pv_new(&pv, dcid, timeout, NGTCP2_PV_FLAG_NONE, &conn->log,
                     conn->mem);
  if (rv != 0) {
    /* TODO Call ngtcp2_dcid_free here if it is introduced */
    return rv;
  }

  ngtcp2_ringbuf_pop_front(&conn->dcid.unused);
  conn->pv = pv;

  ngtcp2_addr_copy(&pv->dcid.ps.path.local, &conn->dcid.current.ps.path.local);
  ngtcp2_addr_copy(&pv->dcid.ps.path.remote, &addr);

  return conn_call_activate_dcid(conn, &pv->dcid);
}

/*
 * conn_recv_handshake_done processes the incoming HANDSHAKE_DONE
 * frame |fr|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_PROTO
 *     Server received HANDSHAKE_DONE frame.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_recv_handshake_done(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  int rv;

  if (conn->server) {
    return NGTCP2_ERR_PROTO;
  }

  if (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED) {
    return 0;
  }

  conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED |
                 NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED;

  conn->pktns.rtb.persistent_congestion_start_ts = ts;

  conn_discard_handshake_state(conn, ts);

  if (conn->remote.transport_params.preferred_address_present) {
    rv = conn_select_preferred_addr(conn);
    if (rv != 0) {
      return rv;
    }
  }

  if (conn->callbacks.handshake_confirmed) {
    rv = conn->callbacks.handshake_confirmed(conn, conn->user_data);
    if (rv != 0) {
      return NGTCP2_ERR_CALLBACK_FAILURE;
    }
  }

  /* Re-arm loss detection timer after handshake has been
     confirmed. */
  ngtcp2_conn_set_loss_detection_timer(conn, ts);

  return 0;
}

/*
 * conn_recv_datagram processes the incoming DATAGRAM frame |fr|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 */
static int conn_recv_datagram(ngtcp2_conn *conn, ngtcp2_datagram *fr) {
  const uint8_t *data;
  size_t datalen;
  int rv;
  uint32_t flags = NGTCP2_DATAGRAM_FLAG_NONE;

  assert(conn->local.transport_params.max_datagram_frame_size);

  if (!conn->callbacks.recv_datagram) {
    return 0;
  }

  if (fr->datacnt) {
    assert(fr->datacnt == 1);

    data = fr->data->base;
    datalen = fr->data->len;
  } else {
    data = NULL;
    datalen = 0;
  }

  if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
    flags |= NGTCP2_DATAGRAM_FLAG_EARLY;
  }

  rv = conn->callbacks.recv_datagram(conn, flags, data, datalen,
                                     conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  return 0;
}

/*
 * conn_key_phase_changed returns nonzero if |hd| indicates that the
 * key phase has unexpected value.
 */
static int conn_key_phase_changed(ngtcp2_conn *conn, const ngtcp2_pkt_hd *hd) {
  ngtcp2_pktns *pktns = &conn->pktns;

  return !(pktns->crypto.rx.ckm->flags & NGTCP2_CRYPTO_KM_FLAG_KEY_PHASE_ONE) ^
         !(hd->flags & NGTCP2_PKT_FLAG_KEY_PHASE);
}

/*
 * conn_prepare_key_update installs new updated keys.
 */
static int conn_prepare_key_update(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_tstamp confirmed_ts = conn->crypto.key_update.confirmed_ts;
  ngtcp2_duration pto = conn_compute_pto(conn, &conn->pktns);
  ngtcp2_pktns *pktns = &conn->pktns;
  ngtcp2_crypto_km *rx_ckm = pktns->crypto.rx.ckm;
  ngtcp2_crypto_km *tx_ckm = pktns->crypto.tx.ckm;
  ngtcp2_crypto_km *new_rx_ckm, *new_tx_ckm;
  ngtcp2_crypto_aead_ctx rx_aead_ctx = {0}, tx_aead_ctx = {0};
  size_t secretlen, ivlen;

  if ((conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED) &&
      tx_ckm->use_count >= pktns->crypto.ctx.max_encryption &&
      ngtcp2_conn_initiate_key_update(conn, ts) != 0) {
    return NGTCP2_ERR_AEAD_LIMIT_REACHED;
  }

  if ((conn->flags & NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED) ||
      (confirmed_ts != UINT64_MAX && confirmed_ts + pto > ts)) {
    return 0;
  }

  if (conn->crypto.key_update.new_rx_ckm ||
      conn->crypto.key_update.new_tx_ckm) {
    assert(conn->crypto.key_update.new_rx_ckm);
    assert(conn->crypto.key_update.new_tx_ckm);
    return 0;
  }

  secretlen = rx_ckm->secret.len;
  ivlen = rx_ckm->iv.len;

  rv = ngtcp2_crypto_km_nocopy_new(&conn->crypto.key_update.new_rx_ckm,
                                   secretlen, ivlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  rv = ngtcp2_crypto_km_nocopy_new(&conn->crypto.key_update.new_tx_ckm,
                                   secretlen, ivlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  new_rx_ckm = conn->crypto.key_update.new_rx_ckm;
  new_tx_ckm = conn->crypto.key_update.new_tx_ckm;

  assert(conn->callbacks.update_key);

  rv = conn->callbacks.update_key(
      conn, new_rx_ckm->secret.base, new_tx_ckm->secret.base, &rx_aead_ctx,
      new_rx_ckm->iv.base, &tx_aead_ctx, new_tx_ckm->iv.base,
      rx_ckm->secret.base, tx_ckm->secret.base, secretlen, conn->user_data);
  if (rv != 0) {
    return NGTCP2_ERR_CALLBACK_FAILURE;
  }

  new_rx_ckm->aead_ctx = rx_aead_ctx;
  new_tx_ckm->aead_ctx = tx_aead_ctx;

  if (!(rx_ckm->flags & NGTCP2_CRYPTO_KM_FLAG_KEY_PHASE_ONE)) {
    new_rx_ckm->flags |= NGTCP2_CRYPTO_KM_FLAG_KEY_PHASE_ONE;
    new_tx_ckm->flags |= NGTCP2_CRYPTO_KM_FLAG_KEY_PHASE_ONE;
  }

  if (conn->crypto.key_update.old_rx_ckm) {
    conn_call_delete_crypto_aead_ctx(
        conn, &conn->crypto.key_update.old_rx_ckm->aead_ctx);
    ngtcp2_crypto_km_del(conn->crypto.key_update.old_rx_ckm, conn->mem);
    conn->crypto.key_update.old_rx_ckm = NULL;
  }

  return 0;
}

/*
 * conn_rotate_keys rotates keys.  The current key moves to old key,
 * and new key moves to the current key.
 */
static void conn_rotate_keys(ngtcp2_conn *conn, int64_t pkt_num) {
  ngtcp2_pktns *pktns = &conn->pktns;

  assert(conn->crypto.key_update.new_rx_ckm);
  assert(conn->crypto.key_update.new_tx_ckm);
  assert(!conn->crypto.key_update.old_rx_ckm);
  assert(!(conn->flags & NGTCP2_CONN_FLAG_PPE_PENDING));

  conn->crypto.key_update.old_rx_ckm = pktns->crypto.rx.ckm;

  pktns->crypto.rx.ckm = conn->crypto.key_update.new_rx_ckm;
  conn->crypto.key_update.new_rx_ckm = NULL;
  pktns->crypto.rx.ckm->pkt_num = pkt_num;

  assert(pktns->crypto.tx.ckm);

  conn_call_delete_crypto_aead_ctx(conn, &pktns->crypto.tx.ckm->aead_ctx);
  ngtcp2_crypto_km_del(pktns->crypto.tx.ckm, conn->mem);

  pktns->crypto.tx.ckm = conn->crypto.key_update.new_tx_ckm;
  conn->crypto.key_update.new_tx_ckm = NULL;
  pktns->crypto.tx.ckm->pkt_num = pktns->tx.last_pkt_num + 1;

  conn->flags |= NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED;
}

/*
 * conn_path_validation_in_progress returns nonzero if path validation
 * against |path| is underway.
 */
static int conn_path_validation_in_progress(ngtcp2_conn *conn,
                                            const ngtcp2_path *path) {
  ngtcp2_pv *pv = conn->pv;

  return pv && ngtcp2_path_eq(&pv->dcid.ps.path, path);
}

/*
 * conn_recv_non_probing_pkt_on_new_path is called when non-probing
 * packet is received via new path.  It starts path validation against
 * the new path.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CONN_ID_BLOCKED
 *     No DCID is available
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
static int conn_recv_non_probing_pkt_on_new_path(ngtcp2_conn *conn,
                                                 const ngtcp2_path *path,
                                                 size_t dgramlen,
                                                 int new_cid_used,
                                                 ngtcp2_tstamp ts) {

  ngtcp2_dcid dcid, *bound_dcid, *last;
  ngtcp2_pv *pv;
  int rv;
  ngtcp2_duration pto, initial_pto, timeout;
  int require_new_cid;
  int local_addr_eq;
  uint32_t remote_addr_cmp;
  size_t len, i;

  assert(conn->server);

  if (conn->pv && (conn->pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
      ngtcp2_path_eq(&conn->pv->fallback_dcid.ps.path, path)) {
    /* If new path equals fallback path, that means connection
       migrated back to the original path.  Fallback path is
       considered to be validated. */
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PTV,
                    "path is migrated back to the original path");
    ngtcp2_dcid_copy(&conn->dcid.current, &conn->pv->fallback_dcid);
    conn_reset_congestion_state(conn);
    conn->dcid.current.bytes_recv += dgramlen;
    conn_reset_ecn_validation_state(conn);
    rv = conn_stop_pv(conn, ts);
    if (rv != 0) {
      return rv;
    }
    return 0;
  }

  remote_addr_cmp =
      ngtcp2_addr_compare(&conn->dcid.current.ps.path.remote, &path->remote);
  local_addr_eq =
      ngtcp2_addr_eq(&conn->dcid.current.ps.path.local, &path->local);

  /* The transport specification draft-27 says:
   *
   * An endpoint MUST use a new connection ID if it initiates
   * connection migration as described in Section 9.2 or probes a new
   * network path as described in Section 9.1. An endpoint MUST use a
   * new connection ID in response to a change in the address of a
   * peer if the packet with the new peer address uses an active
   * connection ID that has not been previously used by the peer.
   */
  require_new_cid = conn->dcid.current.cid.datalen &&
                    ((new_cid_used && remote_addr_cmp) || !local_addr_eq);

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                  "non-probing packet was received from new remote address");

  pto = conn_compute_pto(conn, &conn->pktns);
  initial_pto = conn_compute_initial_pto(conn, &conn->pktns);
  timeout = 3 * ngtcp2_max(pto, initial_pto);

  len = ngtcp2_ringbuf_len(&conn->dcid.bound);

  for (i = 0; i < len; ++i) {
    bound_dcid = ngtcp2_ringbuf_get(&conn->dcid.bound, i);
    if (ngtcp2_path_eq(&bound_dcid->ps.path, path)) {
      ngtcp2_log_info(
          &conn->log, NGTCP2_LOG_EVENT_CON,
          "Found DCID which has already been bound to the new path");

      ngtcp2_dcid_copy(&dcid, bound_dcid);
      if (i == 0) {
        ngtcp2_ringbuf_pop_front(&conn->dcid.bound);
      } else if (i == ngtcp2_ringbuf_len(&conn->dcid.bound) - 1) {
        ngtcp2_ringbuf_pop_back(&conn->dcid.bound);
      } else {
        last = ngtcp2_ringbuf_get(&conn->dcid.bound, len - 1);
        ngtcp2_dcid_copy(bound_dcid, last);
        ngtcp2_ringbuf_pop_back(&conn->dcid.bound);
      }
      require_new_cid = 0;

      if (dcid.cid.datalen) {
        rv = conn_call_activate_dcid(conn, &dcid);
        if (rv != 0) {
          return rv;
        }
      }
      break;
    }
  }

  if (i == len) {
    if (require_new_cid) {
      if (ngtcp2_ringbuf_len(&conn->dcid.unused) == 0) {
        return NGTCP2_ERR_CONN_ID_BLOCKED;
      }
      ngtcp2_dcid_copy(&dcid, ngtcp2_ringbuf_get(&conn->dcid.unused, 0));
      ngtcp2_ringbuf_pop_front(&conn->dcid.unused);

      rv = conn_call_activate_dcid(conn, &dcid);
      if (rv != 0) {
        return rv;
      }
    } else {
      /* Use the current DCID if a remote endpoint does not change
         DCID. */
      ngtcp2_dcid_copy(&dcid, &conn->dcid.current);
      dcid.bytes_sent = 0;
      dcid.bytes_recv = 0;
      dcid.flags &= (uint8_t)~NGTCP2_DCID_FLAG_PATH_VALIDATED;
    }
  }

  ngtcp2_path_copy(&dcid.ps.path, path);
  dcid.bytes_recv += dgramlen;

  rv = ngtcp2_pv_new(&pv, &dcid, timeout, NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE,
                     &conn->log, conn->mem);
  if (rv != 0) {
    return rv;
  }

  if (conn->pv && (conn->pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE)) {
    ngtcp2_dcid_copy(&pv->fallback_dcid, &conn->pv->fallback_dcid);
    pv->fallback_pto = conn->pv->fallback_pto;
  } else {
    ngtcp2_dcid_copy(&pv->fallback_dcid, &conn->dcid.current);
    pv->fallback_pto = pto;
  }

  ngtcp2_dcid_copy(&conn->dcid.current, &dcid);

  if (!local_addr_eq || (remote_addr_cmp & (NGTCP2_ADDR_COMPARE_FLAG_ADDR |
                                            NGTCP2_ADDR_COMPARE_FLAG_FAMILY))) {
    conn_reset_congestion_state(conn);
  }
  conn_reset_ecn_validation_state(conn);

  if (conn->pv) {
    ngtcp2_log_info(
        &conn->log, NGTCP2_LOG_EVENT_PTV,
        "path migration is aborted because new migration has started");
    rv = conn_stop_pv(conn, ts);
    if (rv != 0) {
      return rv;
    }
  }

  conn->pv = pv;

  return 0;
}

/*
 * conn_recv_pkt_from_new_path is called when a Short packet is
 * received from new path (not current path).  This packet would be a
 * packet which only contains probing frame, or reordered packet, or a
 * path is being validated.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CONN_ID_BLOCKED
 *     No unused DCID is available
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 */
static int conn_recv_pkt_from_new_path(ngtcp2_conn *conn,
                                       const ngtcp2_path *path, size_t dgramlen,
                                       int path_challenge_recved,
                                       ngtcp2_tstamp ts) {
  ngtcp2_pv *pv = conn->pv;
  ngtcp2_dcid *bound_dcid;
  int rv;

  if (pv) {
    if (ngtcp2_path_eq(&pv->dcid.ps.path, path)) {
      pv->dcid.bytes_recv += dgramlen;
      return 0;
    }

    if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
        ngtcp2_path_eq(&pv->fallback_dcid.ps.path, path)) {
      pv->fallback_dcid.bytes_recv += dgramlen;
      return 0;
    }
  }

  if (!path_challenge_recved) {
    return 0;
  }

  rv = conn_bind_dcid(conn, &bound_dcid, path, ts);
  if (rv != 0) {
    return rv;
  }

  ngtcp2_path_copy(&bound_dcid->ps.path, path);
  bound_dcid->bytes_recv += dgramlen;

  return 0;
}

/*
 * conn_recv_delayed_handshake_pkt processes the received Handshake
 * packet which is received after handshake completed.  This function
 * does the minimal job, and its purpose is send acknowledgement of
 * this packet to the peer.  We assume that hd->type ==
 * NGTCP2_PKT_HANDSHAKE.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_FRAME_ENCODING
 *     Frame is badly formatted; or frame type is unknown.
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_DISCARD_PKT
 *     Packet was discarded.
 * NGTCP2_ERR_ACK_FRAME
 *     ACK frame is malformed.
 * NGTCP2_ERR_PROTO
 *     Frame that is not allowed in Handshake packet is received.
 */
static int
conn_recv_delayed_handshake_pkt(ngtcp2_conn *conn, const ngtcp2_pkt_info *pi,
                                const ngtcp2_pkt_hd *hd, size_t pktlen,
                                const uint8_t *payload, size_t payloadlen,
                                ngtcp2_tstamp pkt_ts, ngtcp2_tstamp ts) {
  ngtcp2_ssize nread;
  ngtcp2_max_frame mfr;
  ngtcp2_frame *fr = &mfr.fr;
  int rv;
  int require_ack = 0;
  ngtcp2_pktns *pktns;

  assert(hd->type == NGTCP2_PKT_HANDSHAKE);

  pktns = conn->hs_pktns;

  if (payloadlen == 0) {
    /* QUIC packet must contain at least one frame */
    return NGTCP2_ERR_PROTO;
  }

  ngtcp2_qlog_pkt_received_start(&conn->qlog);

  for (; payloadlen;) {
    nread = ngtcp2_pkt_decode_frame(fr, payload, payloadlen);
    if (nread < 0) {
      return (int)nread;
    }

    payload += nread;
    payloadlen -= (size_t)nread;

    switch (fr->type) {
    case NGTCP2_FRAME_ACK:
    case NGTCP2_FRAME_ACK_ECN:
      fr->ack.ack_delay = 0;
      fr->ack.ack_delay_unscaled = 0;
      break;
    }

    ngtcp2_log_rx_fr(&conn->log, hd, fr);

    switch (fr->type) {
    case NGTCP2_FRAME_ACK:
    case NGTCP2_FRAME_ACK_ECN:
      if (!conn->server) {
        conn->flags |= NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED;
      }
      rv = conn_recv_ack(conn, pktns, &fr->ack, pkt_ts, ts);
      if (rv != 0) {
        return rv;
      }
      break;
    case NGTCP2_FRAME_PADDING:
      break;
    case NGTCP2_FRAME_CONNECTION_CLOSE:
      conn_recv_connection_close(conn, &fr->connection_close);
      break;
    case NGTCP2_FRAME_CRYPTO:
    case NGTCP2_FRAME_PING:
      require_ack = 1;
      break;
    default:
      return NGTCP2_ERR_PROTO;
    }

    ngtcp2_qlog_write_frame(&conn->qlog, fr);
  }

  ngtcp2_qlog_pkt_received_end(&conn->qlog, hd, pktlen);

  rv = pktns_commit_recv_pkt_num(pktns, hd->pkt_num, require_ack, pkt_ts);
  if (rv != 0) {
    return rv;
  }

  pktns_increase_ecn_counts(pktns, pi);

  if (require_ack &&
      (++pktns->acktr.rx_npkt >= conn->local.settings.ack_thresh ||
       (pi->ecn & NGTCP2_ECN_MASK) == NGTCP2_ECN_CE)) {
    ngtcp2_acktr_immediate_ack(&pktns->acktr);
  }

  rv = ngtcp2_conn_sched_ack(conn, &pktns->acktr, hd->pkt_num, require_ack,
                             pkt_ts);
  if (rv != 0) {
    return rv;
  }

  conn_restart_timer_on_read(conn, ts);

  ngtcp2_qlog_metrics_updated(&conn->qlog, &conn->cstat);

  return 0;
}

/*
 * conn_recv_pkt processes a packet contained in the buffer pointed by
 * |pkt| of length |pktlen|.  |pkt| may contain multiple QUIC packets.
 * This function only processes the first packet.  |pkt_ts| is the
 * timestamp when packet is received.  |ts| should be the current
 * time.  Usually they are the same, but for buffered packets,
 * |pkt_ts| would be earlier than |ts|.
 *
 * This function returns the number of bytes processed if it succeeds,
 * or one of the following negative error codes:
 *
 * NGTCP2_ERR_DISCARD_PKT
 *     Packet was discarded because plain text header was malformed;
 *     or its payload could not be decrypted.
 * NGTCP2_ERR_PROTO
 *     Packet is badly formatted; or 0RTT packet contains other than
 *     PADDING or STREAM frames; or other QUIC protocol violation is
 *     found.
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User-defined callback function failed.
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 * NGTCP2_ERR_FRAME_ENCODING
 *     Frame is badly formatted; or frame type is unknown.
 * NGTCP2_ERR_ACK_FRAME
 *     ACK frame is malformed.
 * NGTCP2_ERR_STREAM_STATE
 *     Frame is received to the local stream which is not initiated.
 * NGTCP2_ERR_STREAM_LIMIT
 *     Frame has remote stream ID which is strictly greater than the
 *     allowed limit.
 * NGTCP2_ERR_FLOW_CONTROL
 *     Flow control limit is violated.
 * NGTCP2_ERR_FINAL_SIZE
 *     Frame has strictly larger end offset than it is permitted.
 */
static ngtcp2_ssize conn_recv_pkt(ngtcp2_conn *conn, const ngtcp2_path *path,
                                  const ngtcp2_pkt_info *pi, const uint8_t *pkt,
                                  size_t pktlen, size_t dgramlen,
                                  ngtcp2_tstamp pkt_ts, ngtcp2_tstamp ts) {
  ngtcp2_pkt_hd hd;
  int rv = 0;
  size_t hdpktlen;
  const uint8_t *payload;
  size_t payloadlen;
  ngtcp2_ssize nread, nwrite;
  ngtcp2_max_frame mfr;
  ngtcp2_frame *fr = &mfr.fr;
  int require_ack = 0;
  ngtcp2_crypto_aead *aead;
  ngtcp2_crypto_cipher *hp;
  ngtcp2_crypto_km *ckm;
  ngtcp2_crypto_cipher_ctx *hp_ctx;
  ngtcp2_hp_mask hp_mask;
  ngtcp2_decrypt decrypt;
  ngtcp2_pktns *pktns;
  int non_probing_pkt = 0;
  int key_phase_bit_changed = 0;
  int force_decrypt_failure = 0;
  int recv_ncid = 0;
  int new_cid_used = 0;
  int path_challenge_recved = 0;

  if (pkt[0] & NGTCP2_HEADER_FORM_BIT) {
    nread = ngtcp2_pkt_decode_hd_long(&hd, pkt, pktlen);
    if (nread < 0) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "could not decode long header");
      return NGTCP2_ERR_DISCARD_PKT;
    }

    if (pktlen < (size_t)nread + hd.len || conn->version != hd.version) {
      return NGTCP2_ERR_DISCARD_PKT;
    }

    pktlen = (size_t)nread + hd.len;

    /* Quoted from spec: if subsequent packets of those types include
       a different Source Connection ID, they MUST be discarded. */
    if (!ngtcp2_cid_eq(&conn->dcid.current.cid, &hd.scid)) {
      ngtcp2_log_rx_pkt_hd(&conn->log, &hd);
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "packet was ignored because of mismatched SCID");
      return NGTCP2_ERR_DISCARD_PKT;
    }

    switch (hd.type) {
    case NGTCP2_PKT_INITIAL:
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "delayed Initial packet was discarded");
      return (ngtcp2_ssize)pktlen;
    case NGTCP2_PKT_HANDSHAKE:
      if (!conn->hs_pktns) {
        ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                        "delayed Handshake packet was discarded");
        return (ngtcp2_ssize)pktlen;
      }

      pktns = conn->hs_pktns;
      aead = &pktns->crypto.ctx.aead;
      hp = &pktns->crypto.ctx.hp;
      ckm = pktns->crypto.rx.ckm;
      hp_ctx = &pktns->crypto.rx.hp_ctx;
      hp_mask = conn->callbacks.hp_mask;
      decrypt = conn->callbacks.decrypt;
      break;
    case NGTCP2_PKT_0RTT:
      if (!conn->server) {
        return NGTCP2_ERR_DISCARD_PKT;
      }

      if (!conn->early.ckm) {
        return (ngtcp2_ssize)pktlen;
      }

      pktns = &conn->pktns;
      aead = &conn->early.ctx.aead;
      hp = &conn->early.ctx.hp;
      ckm = conn->early.ckm;
      hp_ctx = &conn->early.hp_ctx;
      hp_mask = conn->callbacks.hp_mask;
      decrypt = conn->callbacks.decrypt;
      break;
    default:
      ngtcp2_log_rx_pkt_hd(&conn->log, &hd);
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "packet type 0x%02x was ignored", hd.type);
      return (ngtcp2_ssize)pktlen;
    }
  } else {
    nread = ngtcp2_pkt_decode_hd_short(&hd, pkt, pktlen, conn->oscid.datalen);
    if (nread < 0) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "could not decode short header");
      return NGTCP2_ERR_DISCARD_PKT;
    }

    pktns = &conn->pktns;
    aead = &pktns->crypto.ctx.aead;
    hp = &pktns->crypto.ctx.hp;
    ckm = pktns->crypto.rx.ckm;
    hp_ctx = &pktns->crypto.rx.hp_ctx;
    hp_mask = conn->callbacks.hp_mask;
    decrypt = conn->callbacks.decrypt;
  }

  rv = conn_ensure_decrypt_hp_buffer(conn, (size_t)nread + 4);
  if (rv != 0) {
    return rv;
  }

  nwrite = decrypt_hp(&hd, conn->crypto.decrypt_hp_buf.base, hp, pkt, pktlen,
                      (size_t)nread, ckm, hp_ctx, hp_mask);
  if (nwrite < 0) {
    if (ngtcp2_err_is_fatal((int)nwrite)) {
      return nwrite;
    }
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "could not decrypt packet number");
    return NGTCP2_ERR_DISCARD_PKT;
  }

  hdpktlen = (size_t)nwrite;
  payload = pkt + hdpktlen;
  payloadlen = pktlen - hdpktlen;

  hd.pkt_num = ngtcp2_pkt_adjust_pkt_num(pktns->rx.max_pkt_num, hd.pkt_num,
                                         pkt_num_bits(hd.pkt_numlen));
  if (hd.pkt_num > NGTCP2_MAX_PKT_NUM) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "pkn=%" PRId64 " is greater than maximum pkn", hd.pkt_num);
    return NGTCP2_ERR_DISCARD_PKT;
  }

  ngtcp2_log_rx_pkt_hd(&conn->log, &hd);

  if (hd.type == NGTCP2_PKT_SHORT) {
    key_phase_bit_changed = conn_key_phase_changed(conn, &hd);
  }

  rv = conn_ensure_decrypt_buffer(conn, payloadlen);
  if (rv != 0) {
    return rv;
  }

  if (key_phase_bit_changed) {
    assert(hd.type == NGTCP2_PKT_SHORT);

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT, "unexpected KEY_PHASE");

    if (ckm->pkt_num > hd.pkt_num) {
      if (conn->crypto.key_update.old_rx_ckm) {
        ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                        "decrypting with old key");
        ckm = conn->crypto.key_update.old_rx_ckm;
      } else {
        force_decrypt_failure = 1;
      }
    } else if (pktns->rx.max_pkt_num < hd.pkt_num) {
      assert(ckm->pkt_num < hd.pkt_num);
      if (!conn->crypto.key_update.new_rx_ckm) {
        ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                        "new key is not available");
        force_decrypt_failure = 1;
      } else {
        ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                        "decrypting with new key");
        ckm = conn->crypto.key_update.new_rx_ckm;
      }
    } else {
      force_decrypt_failure = 1;
    }
  }

  nwrite = decrypt_pkt(conn->crypto.decrypt_buf.base, aead, payload, payloadlen,
                       conn->crypto.decrypt_hp_buf.base, hdpktlen, hd.pkt_num,
                       ckm, decrypt);

  if (force_decrypt_failure) {
    nwrite = NGTCP2_ERR_TLS_DECRYPT;
  }

  if (nwrite < 0) {
    if (ngtcp2_err_is_fatal((int)nwrite)) {
      return nwrite;
    }

    assert(NGTCP2_ERR_TLS_DECRYPT == nwrite);

    if (hd.type == NGTCP2_PKT_SHORT &&
        ++conn->crypto.decryption_failure_count >=
            pktns->crypto.ctx.max_decryption_failure) {
      return NGTCP2_ERR_AEAD_LIMIT_REACHED;
    }

    if (hd.flags & NGTCP2_PKT_FLAG_LONG_FORM) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "could not decrypt packet payload");
      return NGTCP2_ERR_DISCARD_PKT;
    }

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "could not decrypt packet payload");
    return NGTCP2_ERR_DISCARD_PKT;
  }

  rv = ngtcp2_pkt_verify_reserved_bits(conn->crypto.decrypt_hp_buf.base[0]);
  if (rv != 0) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "packet has incorrect reserved bits");

    return NGTCP2_ERR_PROTO;
  }

  if (pktns_pkt_num_is_duplicate(pktns, hd.pkt_num)) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "packet was discarded because of duplicated packet number");
    return NGTCP2_ERR_DISCARD_PKT;
  }

  payload = conn->crypto.decrypt_buf.base;
  payloadlen = (size_t)nwrite;

  if (payloadlen == 0) {
    /* QUIC packet must contain at least one frame */
    return NGTCP2_ERR_PROTO;
  }

  if (hd.flags & NGTCP2_PKT_FLAG_LONG_FORM) {
    switch (hd.type) {
    case NGTCP2_PKT_HANDSHAKE:
      rv = conn_verify_dcid(conn, NULL, &hd);
      if (rv != 0) {
        if (ngtcp2_err_is_fatal(rv)) {
          return rv;
        }
        ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                        "packet was ignored because of mismatched DCID");
        return NGTCP2_ERR_DISCARD_PKT;
      }

      rv = conn_recv_delayed_handshake_pkt(conn, pi, &hd, pktlen, payload,
                                           payloadlen, pkt_ts, ts);
      if (rv < 0) {
        return (ngtcp2_ssize)rv;
      }

      return (ngtcp2_ssize)pktlen;
    case NGTCP2_PKT_0RTT:
      if (!ngtcp2_cid_eq(&conn->rcid, &hd.dcid)) {
        rv = conn_verify_dcid(conn, NULL, &hd);
        if (rv != 0) {
          if (ngtcp2_err_is_fatal(rv)) {
            return rv;
          }
          ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                          "packet was ignored because of mismatched DCID");
          return NGTCP2_ERR_DISCARD_PKT;
        }
      }
      break;
    default:
      /* Unreachable */
      assert(0);
    }
  } else {
    rv = conn_verify_dcid(conn, &new_cid_used, &hd);
    if (rv != 0) {
      if (ngtcp2_err_is_fatal(rv)) {
        return rv;
      }
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                      "packet was ignored because of mismatched DCID");
      return NGTCP2_ERR_DISCARD_PKT;
    }

    conn->flags |= NGTCP2_CONN_FLAG_RECV_PROTECTED_PKT;
  }

  ngtcp2_qlog_pkt_received_start(&conn->qlog);

  for (; payloadlen;) {
    nread = ngtcp2_pkt_decode_frame(fr, payload, payloadlen);
    if (nread < 0) {
      return nread;
    }

    payload += nread;
    payloadlen -= (size_t)nread;

    switch (fr->type) {
    case NGTCP2_FRAME_ACK:
    case NGTCP2_FRAME_ACK_ECN:
      if ((hd.flags & NGTCP2_PKT_FLAG_LONG_FORM) &&
          hd.type == NGTCP2_PKT_0RTT) {
        return NGTCP2_ERR_PROTO;
      }
      assign_recved_ack_delay_unscaled(
          &fr->ack, conn->remote.transport_params.ack_delay_exponent);
      break;
    }

    ngtcp2_log_rx_fr(&conn->log, &hd, fr);

    if (hd.type == NGTCP2_PKT_0RTT) {
      switch (fr->type) {
      case NGTCP2_FRAME_PADDING:
      case NGTCP2_FRAME_PING:
      case NGTCP2_FRAME_RESET_STREAM:
      case NGTCP2_FRAME_STOP_SENDING:
      case NGTCP2_FRAME_STREAM:
      case NGTCP2_FRAME_MAX_DATA:
      case NGTCP2_FRAME_MAX_STREAM_DATA:
      case NGTCP2_FRAME_MAX_STREAMS_BIDI:
      case NGTCP2_FRAME_MAX_STREAMS_UNI:
      case NGTCP2_FRAME_DATA_BLOCKED:
      case NGTCP2_FRAME_STREAM_DATA_BLOCKED:
      case NGTCP2_FRAME_STREAMS_BLOCKED_BIDI:
      case NGTCP2_FRAME_STREAMS_BLOCKED_UNI:
      case NGTCP2_FRAME_NEW_CONNECTION_ID:
      case NGTCP2_FRAME_PATH_CHALLENGE:
      case NGTCP2_FRAME_CONNECTION_CLOSE:
      case NGTCP2_FRAME_CONNECTION_CLOSE_APP:
      case NGTCP2_FRAME_DATAGRAM:
      case NGTCP2_FRAME_DATAGRAM_LEN:
        break;
      default:
        return NGTCP2_ERR_PROTO;
      }
    }

    switch (fr->type) {
    case NGTCP2_FRAME_ACK:
    case NGTCP2_FRAME_ACK_ECN:
    case NGTCP2_FRAME_PADDING:
    case NGTCP2_FRAME_CONNECTION_CLOSE:
    case NGTCP2_FRAME_CONNECTION_CLOSE_APP:
      break;
    default:
      require_ack = 1;
    }

    switch (fr->type) {
    case NGTCP2_FRAME_ACK:
    case NGTCP2_FRAME_ACK_ECN:
      if (!conn->server) {
        conn->flags |= NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED;
      }
      rv = conn_recv_ack(conn, pktns, &fr->ack, pkt_ts, ts);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_STREAM:
      rv = conn_recv_stream(conn, &fr->stream);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_CRYPTO:
      rv = conn_recv_crypto(conn, NGTCP2_CRYPTO_LEVEL_APPLICATION,
                            &pktns->crypto.strm, &fr->crypto);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_RESET_STREAM:
      rv = conn_recv_reset_stream(conn, &fr->reset_stream);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_STOP_SENDING:
      rv = conn_recv_stop_sending(conn, &fr->stop_sending);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_MAX_STREAM_DATA:
      rv = conn_recv_max_stream_data(conn, &fr->max_stream_data);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_MAX_DATA:
      conn_recv_max_data(conn, &fr->max_data);
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_MAX_STREAMS_BIDI:
    case NGTCP2_FRAME_MAX_STREAMS_UNI:
      rv = conn_recv_max_streams(conn, &fr->max_streams);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_CONNECTION_CLOSE:
    case NGTCP2_FRAME_CONNECTION_CLOSE_APP:
      conn_recv_connection_close(conn, &fr->connection_close);
      break;
    case NGTCP2_FRAME_PING:
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_PATH_CHALLENGE:
      conn_recv_path_challenge(conn, path, &fr->path_challenge);
      path_challenge_recved = 1;
      break;
    case NGTCP2_FRAME_PATH_RESPONSE:
      rv = conn_recv_path_response(conn, &fr->path_response, ts);
      if (rv != 0) {
        return rv;
      }
      break;
    case NGTCP2_FRAME_NEW_CONNECTION_ID:
      rv = conn_recv_new_connection_id(conn, &fr->new_connection_id);
      if (rv != 0) {
        return rv;
      }
      recv_ncid = 1;
      break;
    case NGTCP2_FRAME_RETIRE_CONNECTION_ID:
      rv = conn_recv_retire_connection_id(conn, &hd, &fr->retire_connection_id,
                                          ts);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_NEW_TOKEN:
      rv = conn_recv_new_token(conn, &fr->new_token);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_HANDSHAKE_DONE:
      rv = conn_recv_handshake_done(conn, ts);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_STREAMS_BLOCKED_BIDI:
      rv = conn_recv_streams_blocked_bidi(conn, &fr->streams_blocked);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_STREAMS_BLOCKED_UNI:
      rv = conn_recv_streams_blocked_uni(conn, &fr->streams_blocked);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_DATA_BLOCKED:
      /* TODO Not implemented yet */
      non_probing_pkt = 1;
      break;
    case NGTCP2_FRAME_DATAGRAM:
    case NGTCP2_FRAME_DATAGRAM_LEN:
      if ((uint64_t)nread >
          conn->local.transport_params.max_datagram_frame_size) {
        return NGTCP2_ERR_PROTO;
      }
      rv = conn_recv_datagram(conn, &fr->datagram);
      if (rv != 0) {
        return rv;
      }
      non_probing_pkt = 1;
      break;
    }

    ngtcp2_qlog_write_frame(&conn->qlog, fr);
  }

  ngtcp2_qlog_pkt_received_end(&conn->qlog, &hd, pktlen);

  if (recv_ncid) {
    rv = conn_post_process_recv_new_connection_id(conn, ts);
    if (rv != 0) {
      return rv;
    }
  }

  if (conn->server && hd.type == NGTCP2_PKT_SHORT &&
      !ngtcp2_path_eq(&conn->dcid.current.ps.path, path)) {
    if (non_probing_pkt && pktns->rx.max_pkt_num < hd.pkt_num &&
        !conn_path_validation_in_progress(conn, path)) {
      rv = conn_recv_non_probing_pkt_on_new_path(conn, path, dgramlen,
                                                 new_cid_used, ts);
      if (rv != 0) {
        if (ngtcp2_err_is_fatal(rv)) {
          return rv;
        }

        /* DCID is not available.  Just continue. */
        assert(NGTCP2_ERR_CONN_ID_BLOCKED == rv);
      }
    } else {
      rv = conn_recv_pkt_from_new_path(conn, path, dgramlen,
                                       path_challenge_recved, ts);
      if (rv != 0) {
        if (ngtcp2_err_is_fatal(rv)) {
          return rv;
        }

        /* DCID is not available.  Just continue. */
        assert(NGTCP2_ERR_CONN_ID_BLOCKED == rv);
      }
    }
  }

  if (hd.type == NGTCP2_PKT_SHORT) {
    if (ckm == conn->crypto.key_update.new_rx_ckm) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON, "rotate keys");
      conn_rotate_keys(conn, hd.pkt_num);
    } else if (ckm->pkt_num > hd.pkt_num) {
      ckm->pkt_num = hd.pkt_num;
    }

    if (conn->server && conn->early.ckm &&
        conn->early.discard_started_ts == UINT64_MAX) {
      conn->early.discard_started_ts = ts;
    }
  }

  rv = pktns_commit_recv_pkt_num(pktns, hd.pkt_num, require_ack, pkt_ts);
  if (rv != 0) {
    return rv;
  }

  pktns_increase_ecn_counts(pktns, pi);

  if (require_ack &&
      (++pktns->acktr.rx_npkt >= conn->local.settings.ack_thresh ||
       (pi->ecn & NGTCP2_ECN_MASK) == NGTCP2_ECN_CE)) {
    ngtcp2_acktr_immediate_ack(&pktns->acktr);
  }

  rv = ngtcp2_conn_sched_ack(conn, &pktns->acktr, hd.pkt_num, require_ack,
                             pkt_ts);
  if (rv != 0) {
    return rv;
  }

  conn_restart_timer_on_read(conn, ts);

  ngtcp2_qlog_metrics_updated(&conn->qlog, &conn->cstat);

  return conn->state == NGTCP2_CS_DRAINING ? NGTCP2_ERR_DRAINING
                                           : (ngtcp2_ssize)pktlen;
}

/*
 * conn_process_buffered_protected_pkt processes buffered 0RTT or
 * Short packets.
 *
 * This function returns 0 if it succeeds, or the same negative error
 * codes from conn_recv_pkt.
 */
static int conn_process_buffered_protected_pkt(ngtcp2_conn *conn,
                                               ngtcp2_pktns *pktns,
                                               ngtcp2_tstamp ts) {
  ngtcp2_ssize nread;
  ngtcp2_pkt_chain **ppc, *next;
  int rv;

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                  "processing buffered protected packet");

  for (ppc = &pktns->rx.buffed_pkts; *ppc;) {
    next = (*ppc)->next;
    nread = conn_recv_pkt(conn, &(*ppc)->path.path, &(*ppc)->pi, (*ppc)->pkt,
                          (*ppc)->pktlen, (*ppc)->dgramlen, (*ppc)->ts, ts);
    if (nread < 0 && !ngtcp2_err_is_fatal((int)nread) &&
        nread != NGTCP2_ERR_DRAINING) {
      /* TODO We don't know this is the first QUIC packet in a
         datagram. */
      rv = conn_on_stateless_reset(conn, &(*ppc)->path.path, (*ppc)->pkt,
                                   (*ppc)->pktlen);
      if (rv == 0) {
        ngtcp2_pkt_chain_del(*ppc, conn->mem);
        *ppc = next;
        return NGTCP2_ERR_DRAINING;
      }
    }

    ngtcp2_pkt_chain_del(*ppc, conn->mem);
    *ppc = next;
    if (nread < 0) {
      if (nread == NGTCP2_ERR_DISCARD_PKT) {
        continue;
      }
      return (int)nread;
    }
  }

  return 0;
}

/*
 * conn_process_buffered_handshake_pkt processes buffered Handshake
 * packets.
 *
 * This function returns 0 if it succeeds, or the same negative error
 * codes from conn_recv_handshake_pkt.
 */
static int conn_process_buffered_handshake_pkt(ngtcp2_conn *conn,
                                               ngtcp2_tstamp ts) {
  ngtcp2_pktns *pktns = conn->hs_pktns;
  ngtcp2_ssize nread;
  ngtcp2_pkt_chain **ppc, *next;

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                  "processing buffered handshake packet");

  for (ppc = &pktns->rx.buffed_pkts; *ppc;) {
    next = (*ppc)->next;
    nread = conn_recv_handshake_pkt(conn, &(*ppc)->path.path, &(*ppc)->pi,
                                    (*ppc)->pkt, (*ppc)->pktlen,
                                    (*ppc)->dgramlen, (*ppc)->ts, ts);
    ngtcp2_pkt_chain_del(*ppc, conn->mem);
    *ppc = next;
    if (nread < 0) {
      if (nread == NGTCP2_ERR_DISCARD_PKT) {
        continue;
      }
      return (int)nread;
    }
  }

  return 0;
}

static void conn_sync_stream_id_limit(ngtcp2_conn *conn) {
  ngtcp2_transport_params *params = &conn->remote.transport_params;

  conn->local.bidi.max_streams = params->initial_max_streams_bidi;
  conn->local.uni.max_streams = params->initial_max_streams_uni;
}

/*
 * conn_handshake_completed is called once cryptographic handshake has
 * completed.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_CALLBACK_FAILURE
 *     User callback failed.
 */
static int conn_handshake_completed(ngtcp2_conn *conn) {
  int rv;

  conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED;

  rv = conn_call_handshake_completed(conn);
  if (rv != 0) {
    return rv;
  }

  if (conn->local.bidi.max_streams > 0) {
    rv = conn_call_extend_max_local_streams_bidi(conn,
                                                 conn->local.bidi.max_streams);
    if (rv != 0) {
      return rv;
    }
  }
  if (conn->local.uni.max_streams > 0) {
    rv = conn_call_extend_max_local_streams_uni(conn,
                                                conn->local.uni.max_streams);
    if (rv != 0) {
      return rv;
    }
  }

  return 0;
}

/*
 * conn_recv_cpkt processes compound packet after handshake.  The
 * buffer pointed by |pkt| might contain multiple packets.  The Short
 * packet must be the last one because it does not have payload length
 * field.
 *
 * This function returns 0 if it succeeds, or the same negative error
 * codes from conn_recv_pkt except for NGTCP2_ERR_DISCARD_PKT.
 */
static int conn_recv_cpkt(ngtcp2_conn *conn, const ngtcp2_path *path,
                          const ngtcp2_pkt_info *pi, const uint8_t *pkt,
                          size_t pktlen, ngtcp2_tstamp ts) {
  ngtcp2_ssize nread;
  int rv;
  const uint8_t *origpkt = pkt;
  size_t dgramlen = pktlen;

  if (ngtcp2_path_eq(&conn->dcid.current.ps.path, path)) {
    conn->dcid.current.bytes_recv += dgramlen;
  }

  while (pktlen) {
    nread = conn_recv_pkt(conn, path, pi, pkt, pktlen, dgramlen, ts, ts);
    if (nread < 0) {
      if (ngtcp2_err_is_fatal((int)nread)) {
        return (int)nread;
      }

      if (nread == NGTCP2_ERR_DRAINING) {
        return NGTCP2_ERR_DRAINING;
      }

      if (origpkt == pkt) {
        rv = conn_on_stateless_reset(conn, path, origpkt, dgramlen);
        if (rv == 0) {
          return NGTCP2_ERR_DRAINING;
        }
      }
      if (nread == NGTCP2_ERR_DISCARD_PKT) {
        return 0;
      }
      return (int)nread;
    }

    assert(pktlen >= (size_t)nread);
    pkt += nread;
    pktlen -= (size_t)nread;

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_PKT,
                    "read packet %td left %zu", nread, pktlen);
  }

  return 0;
}

/*
 * conn_is_retired_path returns nonzero if |path| is included in
 * retired path list.
 */
static int conn_is_retired_path(ngtcp2_conn *conn, const ngtcp2_path *path) {
  size_t i, len = ngtcp2_ringbuf_len(&conn->dcid.retired);
  ngtcp2_dcid *dcid;

  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.retired, i);
    if (ngtcp2_path_eq(&dcid->ps.path, path)) {
      return 1;
    }
  }

  return 0;
}

/*
 * conn_enqueue_handshake_done enqueues HANDSHAKE_DONE frame for
 * transmission.
 */
static int conn_enqueue_handshake_done(ngtcp2_conn *conn) {
  ngtcp2_pktns *pktns = &conn->pktns;
  ngtcp2_frame_chain *nfrc;
  int rv;

  assert(conn->server);

  rv = ngtcp2_frame_chain_new(&nfrc, conn->mem);
  if (rv != 0) {
    return rv;
  }

  nfrc->fr.type = NGTCP2_FRAME_HANDSHAKE_DONE;
  nfrc->next = pktns->tx.frq;
  pktns->tx.frq = nfrc;

  return 0;
}

/**
 * @function
 *
 * `conn_read_handshake` performs QUIC cryptographic handshake by
 * reading given data.  |pkt| points to the buffer to read and
 * |pktlen| is the length of the buffer.  |path| is the network path.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes: (TBD).
 */
static int conn_read_handshake(ngtcp2_conn *conn, const ngtcp2_path *path,
                               const ngtcp2_pkt_info *pi, const uint8_t *pkt,
                               size_t pktlen, ngtcp2_tstamp ts) {
  int rv;

  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
    /* TODO Better to log something when we ignore input */
    return 0;
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    rv = conn_recv_handshake_cpkt(conn, path, pi, pkt, pktlen, ts);
    if (rv < 0) {
      return rv;
    }

    if (conn->state == NGTCP2_CS_CLIENT_INITIAL) {
      /* Retry packet was received */
      return 0;
    }

    assert(conn->hs_pktns);

    if (conn->hs_pktns->crypto.rx.ckm && conn->in_pktns) {
      rv = conn_process_buffered_handshake_pkt(conn, ts);
      if (rv != 0) {
        return rv;
      }
    }

    if ((conn->flags & (NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED |
                        NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED)) ==
        NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED) {
      rv = conn_handshake_completed(conn);
      if (rv != 0) {
        return rv;
      }
    }

    return 0;
  case NGTCP2_CS_SERVER_INITIAL:
    rv = conn_recv_handshake_cpkt(conn, path, pi, pkt, pktlen, ts);
    if (rv < 0) {
      return rv;
    }

    /*
     * Client ServerHello might not fit into single Initial packet
     * (e.g., resuming session with client authentication).  If we get
     * Client Initial which does not increase offset or it is 0RTT
     * packet buffered, perform address validation in order to buffer
     * validated data only.
     */
    if (ngtcp2_strm_rx_offset(&conn->in_pktns->crypto.strm) == 0) {
      if (conn->in_pktns->crypto.strm.rx.rob &&
          ngtcp2_rob_data_buffered(conn->in_pktns->crypto.strm.rx.rob)) {
        /* Address has been validated with token */
        if (conn->local.settings.token.len) {
          return 0;
        }
        return NGTCP2_ERR_RETRY;
      }
      if (conn->in_pktns->rx.buffed_pkts) {
        /* 0RTT is buffered, force retry */
        return NGTCP2_ERR_RETRY;
      }
      /* If neither CRYPTO frame nor 0RTT packet is processed, just
         drop connection. */
      return NGTCP2_ERR_DROP_CONN;
    }

    /* Process re-ordered 0-RTT packets which arrived before Initial
       packet. */
    if (conn->early.ckm) {
      assert(conn->in_pktns);

      rv = conn_process_buffered_protected_pkt(conn, conn->in_pktns, ts);
      if (rv != 0) {
        return rv;
      }
    }

    return 0;
  case NGTCP2_CS_SERVER_WAIT_HANDSHAKE:
    rv = conn_recv_handshake_cpkt(conn, path, pi, pkt, pktlen, ts);
    if (rv < 0) {
      return rv;
    }

    if (conn->hs_pktns->crypto.rx.ckm) {
      rv = conn_process_buffered_handshake_pkt(conn, ts);
      if (rv != 0) {
        return rv;
      }
    }

    if (conn->hs_pktns->rx.max_pkt_num != -1) {
      conn_discard_initial_state(conn, ts);
    }

    if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
      /* If server hits amplification limit, it cancels loss detection
         timer.  If server receives a packet from client, the limit is
         increased and server can send more.  If server has
         ack-eliciting Initial or Handshake packets, it should resend
         it if timer fired but timer is not armed in this case.  So
         instead of resending Initial/Handshake packets, if server has
         1RTT data to send, it might send them and then might hit
         amplification limit again until it hits stream data limit.
         Initial/Handshake data is not resent.  In order to avoid this
         situation, try to arm loss detection and check the expiry
         here so that on next write call, we can resend
         Initial/Handshake first. */
      if (conn->cstat.loss_detection_timer == UINT64_MAX) {
        ngtcp2_conn_set_loss_detection_timer(conn, ts);
        if (ngtcp2_conn_loss_detection_expiry(conn) <= ts) {
          rv = ngtcp2_conn_on_loss_detection_timer(conn, ts);
          if (rv != 0) {
            return rv;
          }
        }
      }

      return 0;
    }

    if (!(conn->flags & NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED)) {
      return NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM;
    }

    rv = conn_handshake_completed(conn);
    if (rv != 0) {
      return rv;
    }
    conn->state = NGTCP2_CS_POST_HANDSHAKE;

    rv = conn_call_activate_dcid(conn, &conn->dcid.current);
    if (rv != 0) {
      return rv;
    }

    rv = conn_process_buffered_protected_pkt(conn, &conn->pktns, ts);
    if (rv != 0) {
      return rv;
    }

    conn_discard_handshake_state(conn, ts);

    rv = conn_enqueue_handshake_done(conn);
    if (rv != 0) {
      return rv;
    }

    conn->pktns.rtb.persistent_congestion_start_ts = ts;

    /* Re-arm loss detection timer here after handshake has been
       confirmed. */
    ngtcp2_conn_set_loss_detection_timer(conn, ts);

    return 0;
  case NGTCP2_CS_CLOSING:
    return NGTCP2_ERR_CLOSING;
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_DRAINING;
  default:
    return 0;
  }
}

int ngtcp2_conn_read_pkt(ngtcp2_conn *conn, const ngtcp2_path *path,
                         const ngtcp2_pkt_info *pi, const uint8_t *pkt,
                         size_t pktlen, ngtcp2_tstamp ts) {
  int rv = 0;

  conn->log.last_ts = ts;
  conn->qlog.last_ts = ts;

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON, "recv packet len=%zu",
                  pktlen);

  if (pktlen == 0) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  /* client does not expect a packet from unknown path. */
  if (!conn->server && !ngtcp2_path_eq(&conn->dcid.current.ps.path, path) &&
      (!conn->pv || !ngtcp2_path_eq(&conn->pv->dcid.ps.path, path)) &&
      !conn_is_retired_path(conn, path)) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                    "ignore packet from unknown path");
    return 0;
  }

  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
  case NGTCP2_CS_CLIENT_TLS_HANDSHAKE_FAILED:
    return conn_read_handshake(conn, path, pi, pkt, pktlen, ts);
  case NGTCP2_CS_SERVER_INITIAL:
  case NGTCP2_CS_SERVER_WAIT_HANDSHAKE:
  case NGTCP2_CS_SERVER_TLS_HANDSHAKE_FAILED:
    if (!ngtcp2_path_eq(&conn->dcid.current.ps.path, path)) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                      "ignore packet from unknown path during handshake");

      if (conn->state == NGTCP2_CS_SERVER_INITIAL &&
          ngtcp2_strm_rx_offset(&conn->in_pktns->crypto.strm) == 0 &&
          (!conn->in_pktns->crypto.strm.rx.rob ||
           !ngtcp2_rob_data_buffered(conn->in_pktns->crypto.strm.rx.rob))) {
        return NGTCP2_ERR_DROP_CONN;
      }

      return 0;
    }
    return conn_read_handshake(conn, path, pi, pkt, pktlen, ts);
  case NGTCP2_CS_CLOSING:
    return NGTCP2_ERR_CLOSING;
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_DRAINING;
  case NGTCP2_CS_POST_HANDSHAKE:
    rv = conn_prepare_key_update(conn, ts);
    if (rv != 0) {
      return rv;
    }
    return conn_recv_cpkt(conn, path, pi, pkt, pktlen, ts);
  default:
    assert(0);
    abort();
  }
}

/*
 * conn_check_pkt_num_exhausted returns nonzero if packet number is
 * exhausted in at least one of packet number space.
 */
static int conn_check_pkt_num_exhausted(ngtcp2_conn *conn) {
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;

  return (in_pktns && in_pktns->tx.last_pkt_num == NGTCP2_MAX_PKT_NUM) ||
         (hs_pktns && hs_pktns->tx.last_pkt_num == NGTCP2_MAX_PKT_NUM) ||
         conn->pktns.tx.last_pkt_num == NGTCP2_MAX_PKT_NUM;
}

/*
 * conn_retransmit_retry_early retransmits 0RTT packet after Retry is
 * received from server.
 */
static ngtcp2_ssize conn_retransmit_retry_early(ngtcp2_conn *conn,
                                                ngtcp2_pkt_info *pi,
                                                uint8_t *dest, size_t destlen,
                                                ngtcp2_tstamp ts) {
  return conn_write_pkt(conn, pi, dest, destlen, NULL, NGTCP2_PKT_0RTT,
                        NGTCP2_WRITE_PKT_FLAG_NONE, ts);
}

/*
 * conn_handshake_probe_left returns nonzero if there are probe
 * packets to be sent for Initial or Handshake packet number space
 * left.
 */
static int conn_handshake_probe_left(ngtcp2_conn *conn) {
  return (conn->in_pktns && conn->in_pktns->rtb.probe_pkt_left) ||
         conn->hs_pktns->rtb.probe_pkt_left;
}

/*
 * conn_write_handshake writes QUIC handshake packets to the buffer
 * pointed by |dest| of length |destlen|.  |early_datalen| specifies
 * the expected length of early data to send.  Specify 0 to
 * |early_datalen| if there is no early data.
 *
 * This function returns the number of bytes written to the buffer, or
 * one of the following negative error codes:
 *
 * NGTCP2_ERR_PKT_NUM_EXHAUSTED
 *     Packet number is exhausted.
 * NGTCP2_ERR_NOMEM
 *     Out of memory
 * NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM
 *     Required transport parameter is missing.
 * NGTCP2_CS_CLOSING
 *     Connection is in closing state.
 * NGTCP2_CS_DRAINING
 *     Connection is in draining state.
 *
 * In addition to the above negative error codes, the same error codes
 * from conn_recv_pkt may also be returned.
 */
static ngtcp2_ssize conn_write_handshake(ngtcp2_conn *conn, ngtcp2_pkt_info *pi,
                                         uint8_t *dest, size_t destlen,
                                         size_t early_datalen,
                                         ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_ssize res = 0, nwrite = 0, early_spktlen = 0;
  size_t origlen = destlen;
  size_t server_tx_left;
  ngtcp2_conn_stat *cstat = &conn->cstat;
  size_t pending_early_datalen;
  ngtcp2_dcid *dcid;
  ngtcp2_preferred_addr *paddr;

  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
    pending_early_datalen = conn_retry_early_payloadlen(conn);
    if (pending_early_datalen) {
      early_datalen = pending_early_datalen;
    }

    if (!(conn->flags & NGTCP2_CONN_FLAG_RECV_RETRY)) {
      nwrite =
          conn_write_client_initial(conn, pi, dest, destlen, early_datalen, ts);
      if (nwrite <= 0) {
        return nwrite;
      }
    } else {
      nwrite = conn_write_handshake_pkt(
          conn, pi, dest, destlen, NGTCP2_PKT_INITIAL,
          NGTCP2_WRITE_PKT_FLAG_NONE, early_datalen, ts);
      if (nwrite < 0) {
        return nwrite;
      }
    }

    if (pending_early_datalen) {
      early_spktlen = conn_retransmit_retry_early(conn, pi, dest + nwrite,
                                                  destlen - (size_t)nwrite, ts);

      if (early_spktlen < 0) {
        assert(ngtcp2_err_is_fatal((int)early_spktlen));
        return early_spktlen;
      }
    }

    conn->state = NGTCP2_CS_CLIENT_WAIT_HANDSHAKE;

    res = nwrite + early_spktlen;

    return res;
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
    if (!conn_handshake_probe_left(conn) && conn_cwnd_is_zero(conn)) {
      destlen = 0;
    } else {
      if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED)) {
        pending_early_datalen = conn_retry_early_payloadlen(conn);
        if (pending_early_datalen) {
          early_datalen = pending_early_datalen;
        }
      }

      nwrite =
          conn_write_handshake_pkts(conn, pi, dest, destlen, early_datalen, ts);
      if (nwrite < 0) {
        return nwrite;
      }

      res += nwrite;
      dest += nwrite;
      destlen -= (size_t)nwrite;
    }

    if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
      if (!(conn->flags & NGTCP2_CONN_FLAG_EARLY_DATA_REJECTED)) {
        nwrite = conn_retransmit_retry_early(conn, pi, dest, destlen, ts);
        if (nwrite < 0) {
          return nwrite;
        }

        res += nwrite;
      }

      if (res == 0) {
        nwrite = conn_write_handshake_ack_pkts(conn, pi, dest, origlen, ts);
        if (nwrite < 0) {
          return nwrite;
        }
        res = nwrite;
      }

      return res;
    }

    if (!(conn->flags & NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED)) {
      return NGTCP2_ERR_REQUIRED_TRANSPORT_PARAM;
    }

    conn->state = NGTCP2_CS_POST_HANDSHAKE;

    if (conn->remote.transport_params.preferred_address_present) {
      assert(!ngtcp2_ringbuf_full(&conn->dcid.unused));

      paddr = &conn->remote.transport_params.preferred_address;
      dcid = ngtcp2_ringbuf_push_back(&conn->dcid.unused);
      ngtcp2_dcid_init(dcid, 1, &paddr->cid, paddr->stateless_reset_token);

      rv = ngtcp2_gaptr_push(&conn->dcid.seqgap, 1, 1);
      if (rv != 0) {
        return (ngtcp2_ssize)rv;
      }
    }

    if (conn->remote.transport_params.stateless_reset_token_present) {
      assert(conn->dcid.current.seq == 0);
      memcpy(conn->dcid.current.token,
             conn->remote.transport_params.stateless_reset_token,
             sizeof(conn->dcid.current.token));
    }

    rv = conn_call_activate_dcid(conn, &conn->dcid.current);
    if (rv != 0) {
      return rv;
    }

    conn_process_early_rtb(conn);

    rv = conn_process_buffered_protected_pkt(conn, &conn->pktns, ts);
    if (rv != 0) {
      return (ngtcp2_ssize)rv;
    }

    return res;
  case NGTCP2_CS_SERVER_INITIAL:
    nwrite = conn_write_handshake_pkts(conn, pi, dest, destlen,
                                       /* early_datalen = */ 0, ts);
    if (nwrite < 0) {
      return nwrite;
    }

    if (nwrite) {
      conn->state = NGTCP2_CS_SERVER_WAIT_HANDSHAKE;
    }

    return nwrite;
  case NGTCP2_CS_SERVER_WAIT_HANDSHAKE:
    if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
      if (!(conn->dcid.current.flags & NGTCP2_DCID_FLAG_PATH_VALIDATED)) {
        server_tx_left = conn_server_tx_left(conn, &conn->dcid.current);
        if (server_tx_left == 0) {
          if (cstat->loss_detection_timer != UINT64_MAX) {
            ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV,
                            "loss detection timer canceled");
            cstat->loss_detection_timer = UINT64_MAX;
            cstat->pto_count = 0;
          }
          return 0;
        }
      }

      if (conn_handshake_probe_left(conn) || !conn_cwnd_is_zero(conn)) {
        nwrite = conn_write_handshake_pkts(conn, pi, dest, destlen,
                                           /* early_datalen = */ 0, ts);
        if (nwrite < 0) {
          return nwrite;
        }

        res += nwrite;
        dest += nwrite;
        destlen -= (size_t)nwrite;
      }

      if (res == 0) {
        nwrite = conn_write_handshake_ack_pkts(conn, pi, dest, origlen, ts);
        if (nwrite < 0) {
          return nwrite;
        }

        res += nwrite;
        dest += nwrite;
        origlen -= (size_t)nwrite;
      }

      return res;
    }

    return 0;
  case NGTCP2_CS_CLOSING:
    return NGTCP2_ERR_CLOSING;
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_DRAINING;
  default:
    return 0;
  }
}

/**
 * @function
 *
 * `conn_client_write_handshake` writes client side handshake data and
 * 0RTT packet.
 *
 * In order to send STREAM data in 0RTT packet, specify
 * |vmsg|->stream.  |vmsg|->stream.strm, |vmsg|->stream.fin,
 * |vmsg|->stream.data, and |vmsg|->stream.datacnt are stream to which
 * 0-RTT data is sent, whether it is a last data chunk in this stream,
 * a vector of 0-RTT data, and its number of elements respectively.
 * The amount of 0RTT data sent is assigned to
 * *|vmsg|->stream.pdatalen.  If no data is sent, -1 is assigned.
 * Note that 0 length STREAM frame is allowed in QUIC, so 0 might be
 * assigned to *|vmsg|->stream.pdatalen.
 *
 * This function returns 0 if it cannot write any frame because buffer
 * is too small, or packet is congestion limited.  Application should
 * keep reading and wait for congestion window to grow.
 *
 * This function returns the number of bytes written to the buffer
 * pointed by |dest| if it succeeds, or one of the following negative
 * error codes: (TBD).
 */
static ngtcp2_ssize conn_client_write_handshake(ngtcp2_conn *conn,
                                                ngtcp2_pkt_info *pi,
                                                uint8_t *dest, size_t destlen,
                                                ngtcp2_vmsg *vmsg,
                                                ngtcp2_tstamp ts) {
  int send_stream = 0;
  int send_datagram = 0;
  ngtcp2_ssize spktlen, early_spktlen;
  int was_client_initial;
  size_t datalen;
  size_t early_datalen = 0;
  uint8_t wflags = NGTCP2_WRITE_PKT_FLAG_NONE;
  int ppe_pending = (conn->flags & NGTCP2_CONN_FLAG_PPE_PENDING) != 0;

  assert(!conn->server);

  /* conn->early.ckm might be created in the first call of
     conn_handshake().  Check it later. */
  if (vmsg && !(conn->flags & NGTCP2_CONN_FLAG_EARLY_DATA_REJECTED)) {
    switch (vmsg->type) {
    case NGTCP2_VMSG_TYPE_STREAM:
      datalen = ngtcp2_vec_len(vmsg->stream.data, vmsg->stream.datacnt);
      send_stream =
          conn_retry_early_payloadlen(conn) == 0 &&
          /* 0 length STREAM frame is allowed */
          (datalen == 0 ||
           (datalen > 0 &&
            (vmsg->stream.strm->tx.max_offset - vmsg->stream.strm->tx.offset) &&
            (conn->tx.max_offset - conn->tx.offset)));
      if (send_stream) {
        early_datalen =
            conn_enforce_flow_control(conn, vmsg->stream.strm, datalen) +
            NGTCP2_STREAM_OVERHEAD;

        if (vmsg->stream.flags & NGTCP2_WRITE_STREAM_FLAG_MORE) {
          wflags |= NGTCP2_WRITE_PKT_FLAG_MORE;
        }
      } else {
        vmsg = NULL;
      }
      break;
    case NGTCP2_VMSG_TYPE_DATAGRAM:
      datalen = ngtcp2_vec_len(vmsg->datagram.data, vmsg->datagram.datacnt);
      /* TODO Do we need this?  DATAGRAM is independent from STREAM
         data and no retransmission */
      send_datagram = conn_retry_early_payloadlen(conn) == 0;
      if (send_datagram) {
        early_datalen = datalen + NGTCP2_DATAGRAM_OVERHEAD;

        if (vmsg->datagram.flags & NGTCP2_WRITE_DATAGRAM_FLAG_MORE) {
          wflags |= NGTCP2_WRITE_PKT_FLAG_MORE;
        }
      } else {
        vmsg = NULL;
      }
      break;
    }
  }

  if (!ppe_pending) {
    was_client_initial = conn->state == NGTCP2_CS_CLIENT_INITIAL;
    spktlen = conn_write_handshake(conn, pi, dest, destlen, early_datalen, ts);

    if (spktlen < 0) {
      return spktlen;
    }

    if (conn->pktns.crypto.tx.ckm || !conn->early.ckm ||
        (!send_stream && !send_datagram)) {
      return spktlen;
    }
  } else {
    assert(!conn->pktns.crypto.tx.ckm);
    assert(conn->early.ckm);

    was_client_initial = conn->pkt.was_client_initial;
    spktlen = conn->pkt.hs_spktlen;
  }

  /* If spktlen > 0, we are making a compound packet.  If Initial
     packet is written, we have to pad bytes to 0-RTT packet. */

  if (spktlen && was_client_initial) {
    wflags |= NGTCP2_WRITE_PKT_FLAG_REQUIRE_PADDING;
  }

  dest += spktlen;
  destlen -= (size_t)spktlen;

  if (conn_cwnd_is_zero(conn)) {
    return spktlen;
  }

  early_spktlen = conn_write_pkt(conn, pi, dest, destlen, vmsg, NGTCP2_PKT_0RTT,
                                 wflags, ts);

  if (early_spktlen < 0) {
    switch (early_spktlen) {
    case NGTCP2_ERR_STREAM_DATA_BLOCKED:
      return spktlen;
    case NGTCP2_ERR_WRITE_MORE:
      conn->pkt.was_client_initial = was_client_initial;
      conn->pkt.hs_spktlen = spktlen;
      break;
    }
    return early_spktlen;
  }

  return spktlen + early_spktlen;
}

void ngtcp2_conn_handshake_completed(ngtcp2_conn *conn) {
  conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED;
  if (conn->server) {
    conn->flags |= NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED;
  }
}

int ngtcp2_conn_get_handshake_completed(ngtcp2_conn *conn) {
  return (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED) &&
         (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED);
}

int ngtcp2_conn_sched_ack(ngtcp2_conn *conn, ngtcp2_acktr *acktr,
                          int64_t pkt_num, int active_ack, ngtcp2_tstamp ts) {
  int rv;
  (void)conn;

  rv = ngtcp2_acktr_add(acktr, pkt_num, active_ack, ts);
  if (rv != 0) {
    assert(rv != NGTCP2_ERR_INVALID_ARGUMENT);
    return rv;
  }

  return 0;
}

int ngtcp2_accept(ngtcp2_pkt_hd *dest, const uint8_t *pkt, size_t pktlen) {
  ngtcp2_ssize nread;
  ngtcp2_pkt_hd hd, *p;

  if (dest) {
    p = dest;
  } else {
    p = &hd;
  }

  if (pktlen == 0 || (pkt[0] & NGTCP2_HEADER_FORM_BIT) == 0) {
    return -1;
  }

  nread = ngtcp2_pkt_decode_hd_long(p, pkt, pktlen);
  if (nread < 0) {
    return -1;
  }

  switch (p->type) {
  case NGTCP2_PKT_INITIAL:
    if (pktlen < NGTCP2_MIN_INITIAL_PKTLEN) {
      return -1;
    }
    if (p->token.len == 0 && p->dcid.datalen < NGTCP2_MIN_INITIAL_DCIDLEN) {
      return -1;
    }
    break;
  case NGTCP2_PKT_0RTT:
    /* 0-RTT packet may arrive before Initial packet due to
       re-ordering. */
    break;
  default:
    return -1;
  }

  if (p->version != NGTCP2_PROTO_VER_V1 &&
      (p->version < NGTCP2_PROTO_VER_DRAFT_MIN ||
       NGTCP2_PROTO_VER_DRAFT_MAX < p->version)) {
    return 1;
  }

  return 0;
}

int ngtcp2_conn_install_initial_key(
    ngtcp2_conn *conn, const ngtcp2_crypto_aead_ctx *rx_aead_ctx,
    const uint8_t *rx_iv, const ngtcp2_crypto_cipher_ctx *rx_hp_ctx,
    const ngtcp2_crypto_aead_ctx *tx_aead_ctx, const uint8_t *tx_iv,
    const ngtcp2_crypto_cipher_ctx *tx_hp_ctx, size_t ivlen) {
  ngtcp2_pktns *pktns = conn->in_pktns;
  int rv;

  assert(pktns);

  conn_call_delete_crypto_cipher_ctx(conn, &pktns->crypto.rx.hp_ctx);
  pktns->crypto.rx.hp_ctx.native_handle = NULL;

  if (pktns->crypto.rx.ckm) {
    conn_call_delete_crypto_aead_ctx(conn, &pktns->crypto.rx.ckm->aead_ctx);
    ngtcp2_crypto_km_del(pktns->crypto.rx.ckm, conn->mem);
    pktns->crypto.rx.ckm = NULL;
  }

  conn_call_delete_crypto_cipher_ctx(conn, &pktns->crypto.tx.hp_ctx);
  pktns->crypto.tx.hp_ctx.native_handle = NULL;

  if (pktns->crypto.tx.ckm) {
    conn_call_delete_crypto_aead_ctx(conn, &pktns->crypto.tx.ckm->aead_ctx);
    ngtcp2_crypto_km_del(pktns->crypto.tx.ckm, conn->mem);
    pktns->crypto.tx.ckm = NULL;
  }

  rv = ngtcp2_crypto_km_new(&pktns->crypto.rx.ckm, NULL, 0, NULL, rx_iv, ivlen,
                            conn->mem);
  if (rv != 0) {
    return rv;
  }

  rv = ngtcp2_crypto_km_new(&pktns->crypto.tx.ckm, NULL, 0, NULL, tx_iv, ivlen,
                            conn->mem);
  if (rv != 0) {
    return rv;
  }

  /* Take owner ship after we are sure that no failure occurs, so that
     caller can delete these contexts on failure. */
  pktns->crypto.rx.ckm->aead_ctx = *rx_aead_ctx;
  pktns->crypto.rx.hp_ctx = *rx_hp_ctx;
  pktns->crypto.tx.ckm->aead_ctx = *tx_aead_ctx;
  pktns->crypto.tx.hp_ctx = *tx_hp_ctx;

  return 0;
}

int ngtcp2_conn_install_rx_handshake_key(
    ngtcp2_conn *conn, const ngtcp2_crypto_aead_ctx *aead_ctx,
    const uint8_t *iv, size_t ivlen, const ngtcp2_crypto_cipher_ctx *hp_ctx) {
  ngtcp2_pktns *pktns = conn->hs_pktns;
  int rv;

  assert(pktns);
  assert(!pktns->crypto.rx.hp_ctx.native_handle);
  assert(!pktns->crypto.rx.ckm);

  rv = ngtcp2_crypto_km_new(&pktns->crypto.rx.ckm, NULL, 0, aead_ctx, iv, ivlen,
                            conn->mem);
  if (rv != 0) {
    return rv;
  }

  pktns->crypto.rx.hp_ctx = *hp_ctx;

  return 0;
}

int ngtcp2_conn_install_tx_handshake_key(
    ngtcp2_conn *conn, const ngtcp2_crypto_aead_ctx *aead_ctx,
    const uint8_t *iv, size_t ivlen, const ngtcp2_crypto_cipher_ctx *hp_ctx) {
  ngtcp2_pktns *pktns = conn->hs_pktns;
  int rv;

  assert(pktns);
  assert(!pktns->crypto.tx.hp_ctx.native_handle);
  assert(!pktns->crypto.tx.ckm);

  rv = ngtcp2_crypto_km_new(&pktns->crypto.tx.ckm, NULL, 0, aead_ctx, iv, ivlen,
                            conn->mem);
  if (rv != 0) {
    return rv;
  }

  pktns->crypto.tx.hp_ctx = *hp_ctx;

  if (conn->server) {
    return ngtcp2_conn_commit_local_transport_params(conn);
  }

  return 0;
}

int ngtcp2_conn_install_early_key(ngtcp2_conn *conn,
                                  const ngtcp2_crypto_aead_ctx *aead_ctx,
                                  const uint8_t *iv, size_t ivlen,
                                  const ngtcp2_crypto_cipher_ctx *hp_ctx) {
  int rv;

  assert(!conn->early.hp_ctx.native_handle);
  assert(!conn->early.ckm);

  rv = ngtcp2_crypto_km_new(&conn->early.ckm, NULL, 0, aead_ctx, iv, ivlen,
                            conn->mem);
  if (rv != 0) {
    return rv;
  }

  conn->early.hp_ctx = *hp_ctx;

  return 0;
}

int ngtcp2_conn_install_rx_key(ngtcp2_conn *conn, const uint8_t *secret,
                               size_t secretlen,
                               const ngtcp2_crypto_aead_ctx *aead_ctx,
                               const uint8_t *iv, size_t ivlen,
                               const ngtcp2_crypto_cipher_ctx *hp_ctx) {
  ngtcp2_pktns *pktns = &conn->pktns;
  int rv;

  assert(!pktns->crypto.rx.hp_ctx.native_handle);
  assert(!pktns->crypto.rx.ckm);

  rv = ngtcp2_crypto_km_new(&pktns->crypto.rx.ckm, secret, secretlen, aead_ctx,
                            iv, ivlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  pktns->crypto.rx.hp_ctx = *hp_ctx;

  if (!conn->server) {
    conn->remote.transport_params = conn->remote.pending_transport_params;
    conn_sync_stream_id_limit(conn);
    conn->tx.max_offset = conn->remote.transport_params.initial_max_data;
  }

  return 0;
}

int ngtcp2_conn_install_tx_key(ngtcp2_conn *conn, const uint8_t *secret,
                               size_t secretlen,
                               const ngtcp2_crypto_aead_ctx *aead_ctx,
                               const uint8_t *iv, size_t ivlen,
                               const ngtcp2_crypto_cipher_ctx *hp_ctx) {
  ngtcp2_pktns *pktns = &conn->pktns;
  int rv;

  assert(!pktns->crypto.tx.hp_ctx.native_handle);
  assert(!pktns->crypto.tx.ckm);

  rv = ngtcp2_crypto_km_new(&pktns->crypto.tx.ckm, secret, secretlen, aead_ctx,
                            iv, ivlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  pktns->crypto.tx.hp_ctx = *hp_ctx;

  if (conn->server) {
    conn->remote.transport_params = conn->remote.pending_transport_params;
    conn_sync_stream_id_limit(conn);
    conn->tx.max_offset = conn->remote.transport_params.initial_max_data;
  } else if (conn->early.ckm) {
    conn_discard_early_key(conn);
  }

  return 0;
}

int ngtcp2_conn_initiate_key_update(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  ngtcp2_tstamp confirmed_ts = conn->crypto.key_update.confirmed_ts;
  ngtcp2_duration pto = conn_compute_pto(conn, &conn->pktns);

  assert(conn->state == NGTCP2_CS_POST_HANDSHAKE);

  if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED) ||
      (conn->flags & NGTCP2_CONN_FLAG_KEY_UPDATE_NOT_CONFIRMED) ||
      !conn->crypto.key_update.new_tx_ckm ||
      !conn->crypto.key_update.new_rx_ckm ||
      (confirmed_ts != UINT64_MAX && confirmed_ts + 3 * pto > ts)) {
    return NGTCP2_ERR_INVALID_STATE;
  }

  conn_rotate_keys(conn, NGTCP2_MAX_PKT_NUM);

  return 0;
}

ngtcp2_tstamp ngtcp2_conn_loss_detection_expiry(ngtcp2_conn *conn) {
  return conn->cstat.loss_detection_timer;
}

ngtcp2_tstamp ngtcp2_conn_internal_expiry(ngtcp2_conn *conn) {
  ngtcp2_tstamp res = UINT64_MAX, t;
  ngtcp2_duration pto = conn_compute_pto(conn, &conn->pktns);
  ngtcp2_scid *scid;
  ngtcp2_dcid *dcid;

  if (conn->pv) {
    res = ngtcp2_pv_next_expiry(conn->pv);
  }

  if (!ngtcp2_pq_empty(&conn->scid.used)) {
    scid = ngtcp2_struct_of(ngtcp2_pq_top(&conn->scid.used), ngtcp2_scid, pe);
    if (scid->ts_retired != UINT64_MAX) {
      res = ngtcp2_min(res, scid->ts_retired + pto);
    }
  }

  if (ngtcp2_ringbuf_len(&conn->dcid.retired)) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.retired, 0);
    res = ngtcp2_min(res, dcid->ts_retired + pto);
  }

  if (conn->server && conn->early.ckm &&
      conn->early.discard_started_ts != UINT64_MAX) {
    t = conn->early.discard_started_ts + 3 * pto;
    res = ngtcp2_min(res, t);
  }

  return res;
}

ngtcp2_tstamp ngtcp2_conn_ack_delay_expiry(ngtcp2_conn *conn) {
  ngtcp2_acktr *acktr = &conn->pktns.acktr;

  if (!(acktr->flags & NGTCP2_ACKTR_FLAG_CANCEL_TIMER) &&
      acktr->first_unacked_ts != UINT64_MAX) {
    return acktr->first_unacked_ts + conn_compute_ack_delay(conn);
  }
  return UINT64_MAX;
}

ngtcp2_tstamp ngtcp2_conn_get_expiry(ngtcp2_conn *conn) {
  ngtcp2_tstamp t1 = ngtcp2_conn_loss_detection_expiry(conn);
  ngtcp2_tstamp t2 = ngtcp2_conn_ack_delay_expiry(conn);
  ngtcp2_tstamp t3 = ngtcp2_conn_internal_expiry(conn);
  ngtcp2_tstamp t4 = ngtcp2_conn_lost_pkt_expiry(conn);
  ngtcp2_tstamp res = ngtcp2_min(t1, t2);
  res = ngtcp2_min(res, t3);
  return ngtcp2_min(res, t4);
}

int ngtcp2_conn_handle_expiry(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_duration pto = conn_compute_pto(conn, &conn->pktns);

  ngtcp2_conn_cancel_expired_ack_delay_timer(conn, ts);

  ngtcp2_conn_remove_lost_pkt(conn, ts);

  if (conn->pv) {
    ngtcp2_pv_cancel_expired_timer(conn->pv, ts);
  }

  if (ngtcp2_conn_loss_detection_expiry(conn) <= ts) {
    rv = ngtcp2_conn_on_loss_detection_timer(conn, ts);
    if (rv != 0) {
      return rv;
    }
  }

  rv = conn_remove_retired_connection_id(conn, pto, ts);
  if (rv != 0) {
    return rv;
  }

  if (conn->server && conn->early.ckm &&
      conn->early.discard_started_ts != UINT64_MAX) {
    if (conn->early.discard_started_ts + 3 * pto <= ts) {
      conn_discard_early_key(conn);
    }
  }

  return 0;
}

static void acktr_cancel_expired_ack_delay_timer(ngtcp2_acktr *acktr,
                                                 ngtcp2_tstamp ts) {
  if (!(acktr->flags & NGTCP2_ACKTR_FLAG_CANCEL_TIMER) &&
      acktr->first_unacked_ts <= ts) {
    acktr->flags |= NGTCP2_ACKTR_FLAG_CANCEL_TIMER;
  }
}

void ngtcp2_conn_cancel_expired_ack_delay_timer(ngtcp2_conn *conn,
                                                ngtcp2_tstamp ts) {
  if (conn->in_pktns) {
    acktr_cancel_expired_ack_delay_timer(&conn->in_pktns->acktr, ts);
  }
  if (conn->hs_pktns) {
    acktr_cancel_expired_ack_delay_timer(&conn->hs_pktns->acktr, ts);
  }
  acktr_cancel_expired_ack_delay_timer(&conn->pktns.acktr, ts);
}

ngtcp2_tstamp ngtcp2_conn_lost_pkt_expiry(ngtcp2_conn *conn) {
  ngtcp2_tstamp res = UINT64_MAX, ts;

  if (conn->in_pktns) {
    ts = ngtcp2_rtb_lost_pkt_ts(&conn->in_pktns->rtb);
    if (ts != UINT64_MAX) {
      ts += conn_compute_pto(conn, conn->in_pktns);
      res = ngtcp2_min(res, ts);
    }
  }

  if (conn->hs_pktns) {
    ts = ngtcp2_rtb_lost_pkt_ts(&conn->hs_pktns->rtb);
    if (ts != UINT64_MAX) {
      ts += conn_compute_pto(conn, conn->hs_pktns);
      res = ngtcp2_min(res, ts);
    }
  }

  ts = ngtcp2_rtb_lost_pkt_ts(&conn->pktns.rtb);
  if (ts != UINT64_MAX) {
    ts += conn_compute_pto(conn, &conn->pktns);
    res = ngtcp2_min(res, ts);
  }

  return res;
}

void ngtcp2_conn_remove_lost_pkt(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  ngtcp2_duration pto;

  if (conn->in_pktns) {
    pto = conn_compute_pto(conn, conn->in_pktns);
    ngtcp2_rtb_remove_expired_lost_pkt(&conn->in_pktns->rtb, pto, ts);
  }
  if (conn->hs_pktns) {
    pto = conn_compute_pto(conn, conn->hs_pktns);
    ngtcp2_rtb_remove_expired_lost_pkt(&conn->hs_pktns->rtb, pto, ts);
  }
  pto = conn_compute_pto(conn, &conn->pktns);
  ngtcp2_rtb_remove_expired_lost_pkt(&conn->pktns.rtb, pto, ts);
}

/*
 * conn_client_validate_transport_params validates |params| as client.
 * |params| must be sent with Encrypted Extensions.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_PROTO
 *     Validation against either of original_dcid and retry_scid is
 *     failed.
 * NGTCP2_ERR_TRANSPORT_PARAM
 *     params contains preferred address but server chose zero-length
 *     connection ID.
 */
static int
conn_client_validate_transport_params(ngtcp2_conn *conn,
                                      const ngtcp2_transport_params *params) {
  if (!ngtcp2_cid_eq(&conn->rcid, &params->original_dcid)) {
    return NGTCP2_ERR_TRANSPORT_PARAM;
  }

  if (conn->flags & NGTCP2_CONN_FLAG_RECV_RETRY) {
    if (!params->retry_scid_present) {
      return NGTCP2_ERR_TRANSPORT_PARAM;
    }
    if (!ngtcp2_cid_eq(&conn->retry_scid, &params->retry_scid)) {
      return NGTCP2_ERR_TRANSPORT_PARAM;
    }
  } else if (params->retry_scid_present) {
    return NGTCP2_ERR_TRANSPORT_PARAM;
  }

  if (params->preferred_address_present &&
      conn->dcid.current.cid.datalen == 0) {
    return NGTCP2_ERR_TRANSPORT_PARAM;
  }

  return 0;
}

int ngtcp2_conn_set_remote_transport_params(
    ngtcp2_conn *conn, const ngtcp2_transport_params *params) {
  int rv;

  assert(!(conn->flags & NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED));

  /* Assume that ngtcp2_decode_transport_params sets default value if
     active_connection_id_limit is omitted. */
  if (params->active_connection_id_limit <
      NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT) {
    return NGTCP2_ERR_TRANSPORT_PARAM;
  }

  /* We assume that conn->dcid.current.cid is still the initial one.
     This requires that transport parameter must be fed into
     ngtcp2_conn as early as possible. */
  if (!ngtcp2_cid_eq(&conn->dcid.current.cid, &params->initial_scid)) {
    return NGTCP2_ERR_TRANSPORT_PARAM;
  }

  if (params->max_udp_payload_size < 1200) {
    return NGTCP2_ERR_TRANSPORT_PARAM;
  }

  if (!conn->server) {
    rv = conn_client_validate_transport_params(conn, params);
    if (rv != 0) {
      return rv;
    }
  }

  ngtcp2_log_remote_tp(&conn->log,
                       conn->server
                           ? NGTCP2_TRANSPORT_PARAMS_TYPE_CLIENT_HELLO
                           : NGTCP2_TRANSPORT_PARAMS_TYPE_ENCRYPTED_EXTENSIONS,
                       params);

  ngtcp2_qlog_parameters_set_transport_params(&conn->qlog, params, conn->server,
                                              NGTCP2_QLOG_SIDE_REMOTE);

  if ((conn->server && conn->pktns.crypto.tx.ckm) ||
      (!conn->server && conn->pktns.crypto.rx.ckm)) {
    conn->remote.transport_params = *params;
    conn_sync_stream_id_limit(conn);
    conn->tx.max_offset = conn->remote.transport_params.initial_max_data;
  } else {
    conn->remote.pending_transport_params = *params;
  }

  conn->flags |= NGTCP2_CONN_FLAG_TRANSPORT_PARAM_RECVED;

  return 0;
}

void ngtcp2_conn_get_remote_transport_params(ngtcp2_conn *conn,
                                             ngtcp2_transport_params *params) {
  if (conn->pktns.crypto.rx.ckm) {
    *params = conn->remote.transport_params;
  } else {
    *params = conn->remote.pending_transport_params;
  }
}

void ngtcp2_conn_set_early_remote_transport_params(
    ngtcp2_conn *conn, const ngtcp2_transport_params *params) {
  ngtcp2_transport_params *p = &conn->remote.transport_params;

  assert(!conn->server);

  memset(p, 0, sizeof(*p));

  p->initial_max_streams_bidi = params->initial_max_streams_bidi;
  p->initial_max_streams_uni = params->initial_max_streams_uni;
  p->initial_max_stream_data_bidi_local =
      params->initial_max_stream_data_bidi_local;
  p->initial_max_stream_data_bidi_remote =
      params->initial_max_stream_data_bidi_remote;
  p->initial_max_stream_data_uni = params->initial_max_stream_data_uni;
  p->initial_max_data = params->initial_max_data;
  p->max_datagram_frame_size = params->max_datagram_frame_size;

  conn_sync_stream_id_limit(conn);

  conn->tx.max_offset = p->initial_max_data;

  ngtcp2_qlog_parameters_set_transport_params(&conn->qlog, p, conn->server,
                                              NGTCP2_QLOG_SIDE_REMOTE);
}

int ngtcp2_conn_set_local_transport_params(
    ngtcp2_conn *conn, const ngtcp2_transport_params *params) {
  assert(conn->server);
  assert(params->active_connection_id_limit <= NGTCP2_MAX_DCID_POOL_SIZE);

  if (conn->hs_pktns == NULL || conn->hs_pktns->crypto.tx.ckm) {
    return NGTCP2_ERR_INVALID_STATE;
  }

  conn->local.transport_params = *params;

  return 0;
}

int ngtcp2_conn_commit_local_transport_params(ngtcp2_conn *conn) {
  const ngtcp2_mem *mem = conn->mem;
  ngtcp2_transport_params *params = &conn->local.transport_params;
  ngtcp2_scid *scident;
  ngtcp2_ksl_it it;
  int rv;

  assert(1 == ngtcp2_ksl_len(&conn->scid.set));

  if (params->active_connection_id_limit == 0) {
    params->active_connection_id_limit =
        NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;
  }

  params->initial_scid = conn->oscid;

  if (conn->oscid.datalen == 0) {
    params->preferred_address_present = 0;
  }

  if (conn->server) {
    if (params->stateless_reset_token_present) {
      it = ngtcp2_ksl_begin(&conn->scid.set);
      scident = ngtcp2_ksl_it_get(&it);

      memcpy(scident->token, params->stateless_reset_token,
             NGTCP2_STATELESS_RESET_TOKENLEN);
    }

    if (params->preferred_address_present) {
      scident = ngtcp2_mem_malloc(mem, sizeof(*scident));
      if (scident == NULL) {
        return NGTCP2_ERR_NOMEM;
      }

      ngtcp2_scid_init(scident, 1, &params->preferred_address.cid,
                       params->preferred_address.stateless_reset_token);

      rv = ngtcp2_ksl_insert(&conn->scid.set, NULL, &scident->cid, scident);
      if (rv != 0) {
        ngtcp2_mem_free(mem, scident);
        return rv;
      }

      conn->scid.last_seq = 1;
    }
  }

  conn->rx.window = conn->rx.unsent_max_offset = conn->rx.max_offset =
      params->initial_max_data;
  conn->remote.bidi.unsent_max_streams = params->initial_max_streams_bidi;
  conn->remote.bidi.max_streams = params->initial_max_streams_bidi;
  conn->remote.uni.unsent_max_streams = params->initial_max_streams_uni;
  conn->remote.uni.max_streams = params->initial_max_streams_uni;

  ngtcp2_qlog_parameters_set_transport_params(&conn->qlog, params, conn->server,
                                              NGTCP2_QLOG_SIDE_LOCAL);

  return 0;
}

void ngtcp2_conn_get_local_transport_params(ngtcp2_conn *conn,
                                            ngtcp2_transport_params *params) {
  *params = conn->local.transport_params;
}

int ngtcp2_conn_open_bidi_stream(ngtcp2_conn *conn, int64_t *pstream_id,
                                 void *stream_user_data) {
  int rv;
  ngtcp2_strm *strm;

  if (ngtcp2_conn_get_streams_bidi_left(conn) == 0) {
    return NGTCP2_ERR_STREAM_ID_BLOCKED;
  }

  strm = ngtcp2_mem_malloc(conn->mem, sizeof(ngtcp2_strm));
  if (strm == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  rv = ngtcp2_conn_init_stream(conn, strm, conn->local.bidi.next_stream_id,
                               stream_user_data);
  if (rv != 0) {
    ngtcp2_mem_free(conn->mem, strm);
    return rv;
  }

  *pstream_id = conn->local.bidi.next_stream_id;
  conn->local.bidi.next_stream_id += 4;

  return 0;
}

int ngtcp2_conn_open_uni_stream(ngtcp2_conn *conn, int64_t *pstream_id,
                                void *stream_user_data) {
  int rv;
  ngtcp2_strm *strm;

  if (ngtcp2_conn_get_streams_uni_left(conn) == 0) {
    return NGTCP2_ERR_STREAM_ID_BLOCKED;
  }

  strm = ngtcp2_mem_malloc(conn->mem, sizeof(ngtcp2_strm));
  if (strm == NULL) {
    return NGTCP2_ERR_NOMEM;
  }

  rv = ngtcp2_conn_init_stream(conn, strm, conn->local.uni.next_stream_id,
                               stream_user_data);
  if (rv != 0) {
    ngtcp2_mem_free(conn->mem, strm);
    return rv;
  }
  ngtcp2_strm_shutdown(strm, NGTCP2_STRM_FLAG_SHUT_RD);

  *pstream_id = conn->local.uni.next_stream_id;
  conn->local.uni.next_stream_id += 4;

  return 0;
}

ngtcp2_strm *ngtcp2_conn_find_stream(ngtcp2_conn *conn, int64_t stream_id) {
  ngtcp2_map_entry *me;

  me = ngtcp2_map_find(&conn->strms, (uint64_t)stream_id);
  if (me == NULL) {
    return NULL;
  }

  return ngtcp2_struct_of(me, ngtcp2_strm, me);
}

ngtcp2_ssize ngtcp2_conn_write_stream(ngtcp2_conn *conn, ngtcp2_path *path,
                                      ngtcp2_pkt_info *pi, uint8_t *dest,
                                      size_t destlen, ngtcp2_ssize *pdatalen,
                                      uint32_t flags, int64_t stream_id,
                                      const uint8_t *data, size_t datalen,
                                      ngtcp2_tstamp ts) {
  ngtcp2_vec datav;

  datav.len = datalen;
  datav.base = (uint8_t *)data;

  return ngtcp2_conn_writev_stream(conn, path, pi, dest, destlen, pdatalen,
                                   flags, stream_id, &datav, 1, ts);
}

ngtcp2_ssize ngtcp2_conn_writev_stream(ngtcp2_conn *conn, ngtcp2_path *path,
                                       ngtcp2_pkt_info *pi, uint8_t *dest,
                                       size_t destlen, ngtcp2_ssize *pdatalen,
                                       uint32_t flags, int64_t stream_id,
                                       const ngtcp2_vec *datav, size_t datavcnt,
                                       ngtcp2_tstamp ts) {
  ngtcp2_vmsg vmsg, *pvmsg;
  ngtcp2_strm *strm;

  if (pdatalen) {
    *pdatalen = -1;
  }

  if (stream_id != -1) {
    strm = ngtcp2_conn_find_stream(conn, stream_id);
    if (strm == NULL) {
      return NGTCP2_ERR_STREAM_NOT_FOUND;
    }

    if (strm->flags & NGTCP2_STRM_FLAG_SHUT_WR) {
      return NGTCP2_ERR_STREAM_SHUT_WR;
    }

    vmsg.type = NGTCP2_VMSG_TYPE_STREAM;
    vmsg.stream.strm = strm;
    vmsg.stream.flags = flags;
    vmsg.stream.data = datav;
    vmsg.stream.datacnt = datavcnt;
    vmsg.stream.pdatalen = pdatalen;

    pvmsg = &vmsg;
  } else {
    pvmsg = NULL;
  }

  return ngtcp2_conn_write_vmsg(conn, path, pi, dest, destlen, pvmsg, ts);
}

ngtcp2_ssize ngtcp2_conn_writev_datagram(ngtcp2_conn *conn, ngtcp2_path *path,
                                         ngtcp2_pkt_info *pi, uint8_t *dest,
                                         size_t destlen, int *paccepted,
                                         uint32_t flags,
                                         const ngtcp2_vec *datav,
                                         size_t datavcnt, ngtcp2_tstamp ts) {
  ngtcp2_vmsg vmsg;

  if (paccepted) {
    *paccepted = 0;
  }

  if (conn->remote.transport_params.max_datagram_frame_size == 0) {
    return NGTCP2_ERR_INVALID_STATE;
  }
  if (conn->remote.transport_params.max_datagram_frame_size <
      ngtcp2_pkt_datagram_framelen(ngtcp2_vec_len(datav, datavcnt))) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  vmsg.type = NGTCP2_VMSG_TYPE_DATAGRAM;
  vmsg.datagram.flags = flags;
  vmsg.datagram.data = datav;
  vmsg.datagram.datacnt = datavcnt;
  vmsg.datagram.paccepted = paccepted;

  return ngtcp2_conn_write_vmsg(conn, path, pi, dest, destlen, &vmsg, ts);
}

ngtcp2_ssize ngtcp2_conn_write_vmsg(ngtcp2_conn *conn, ngtcp2_path *path,
                                    ngtcp2_pkt_info *pi, uint8_t *dest,
                                    size_t destlen, ngtcp2_vmsg *vmsg,
                                    ngtcp2_tstamp ts) {
  ngtcp2_ssize nwrite;
  ngtcp2_pktns *pktns = &conn->pktns;
  size_t origlen = destlen;
  int rv;
  uint8_t wflags = NGTCP2_WRITE_PKT_FLAG_NONE;
  int ppe_pending = (conn->flags & NGTCP2_CONN_FLAG_PPE_PENDING) != 0;
  ngtcp2_ssize res = 0;
  size_t server_tx_left;

  conn->log.last_ts = ts;
  conn->qlog.last_ts = ts;

  if (path) {
    ngtcp2_path_copy(path, &conn->dcid.current.ps.path);
  }

  if (!ppe_pending && pi) {
    pi->ecn = NGTCP2_ECN_NOT_ECT;
  }

  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLIENT_WAIT_HANDSHAKE:
  case NGTCP2_CS_CLIENT_TLS_HANDSHAKE_FAILED:
    nwrite = conn_client_write_handshake(conn, pi, dest, destlen, vmsg, ts);
    if (nwrite < 0) {
      return nwrite;
    }
    if (conn->state != NGTCP2_CS_POST_HANDSHAKE) {
      return nwrite;
    }
    res = nwrite;
    dest += nwrite;
    destlen -= (size_t)nwrite;
    /* Break here so that we can coalesces Short packets. */
    break;
  case NGTCP2_CS_SERVER_INITIAL:
  case NGTCP2_CS_SERVER_WAIT_HANDSHAKE:
  case NGTCP2_CS_SERVER_TLS_HANDSHAKE_FAILED:
    if (!ppe_pending) {
      if (!(conn->dcid.current.flags & NGTCP2_DCID_FLAG_PATH_VALIDATED)) {
        server_tx_left = conn_server_tx_left(conn, &conn->dcid.current);
        destlen = ngtcp2_min(destlen, server_tx_left);
      }

      nwrite = conn_write_handshake(conn, pi, dest, destlen, 0, ts);
      if (nwrite < 0) {
        return nwrite;
      }

      if (conn->dcid.current.flags & NGTCP2_DCID_FLAG_PATH_VALIDATED) {
        destlen = origlen;
      } else {
        origlen = destlen;
      }

      res = nwrite;
      dest += nwrite;
      destlen -= (size_t)nwrite;
    }
    if (conn->state != NGTCP2_CS_POST_HANDSHAKE &&
        conn->pktns.crypto.tx.ckm == NULL) {
      return res;
    }
    break;
  case NGTCP2_CS_POST_HANDSHAKE:
    break;
  case NGTCP2_CS_CLOSING:
    return NGTCP2_ERR_CLOSING;
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_DRAINING;
  default:
    return 0;
  }

  assert(pktns->crypto.tx.ckm);

  if (conn_check_pkt_num_exhausted(conn)) {
    return NGTCP2_ERR_PKT_NUM_EXHAUSTED;
  }

  if (vmsg) {
    switch (vmsg->type) {
    case NGTCP2_VMSG_TYPE_STREAM:
      if (vmsg->stream.flags & NGTCP2_WRITE_STREAM_FLAG_MORE) {
        wflags |= NGTCP2_WRITE_PKT_FLAG_MORE;
      }
      break;
    case NGTCP2_VMSG_TYPE_DATAGRAM:
      if (vmsg->datagram.flags & NGTCP2_WRITE_DATAGRAM_FLAG_MORE) {
        wflags |= NGTCP2_WRITE_PKT_FLAG_MORE;
      }
      break;
    default:
      break;
    }
  }

  if (ppe_pending) {
    res = conn->pkt.hs_spktlen;
    conn->pkt.hs_spktlen = 0;
    /* dest and destlen have already been adjusted in ppe in the first
       run.  They are adjusted for probe packet later. */
    nwrite = conn_write_pkt(conn, pi, dest, destlen, vmsg, NGTCP2_PKT_SHORT,
                            wflags, ts);
    goto fin;
  } else {
    if (conn->state == NGTCP2_CS_POST_HANDSHAKE) {
      rv = conn_prepare_key_update(conn, ts);
      if (rv != 0) {
        return rv;
      }
    }

    if (!conn->pktns.rtb.probe_pkt_left && conn_cwnd_is_zero(conn)) {
      destlen = 0;
    } else {
      nwrite = conn_write_path_response(conn, path, pi, dest, destlen, ts);
      if (nwrite) {
        goto fin;
      }

      if (conn->pv) {
        nwrite = conn_write_path_challenge(conn, path, pi, dest, destlen, ts);
        if (nwrite) {
          goto fin;
        }
      }

      if (conn->server &&
          !(conn->dcid.current.flags & NGTCP2_DCID_FLAG_PATH_VALIDATED)) {
        server_tx_left = conn_server_tx_left(conn, &conn->dcid.current);
        origlen = ngtcp2_min(origlen, server_tx_left);
        destlen = ngtcp2_min(destlen, server_tx_left);

        if (destlen == 0 && conn->cstat.loss_detection_timer != UINT64_MAX) {
          ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV,
                          "loss detection timer canceled");
          conn->cstat.loss_detection_timer = UINT64_MAX;
          conn->cstat.pto_count = 0;
        }
      }
    }
  }

  if (res == 0) {
    if (conn_handshake_remnants_left(conn)) {
      if (conn_handshake_probe_left(conn)) {
        destlen = origlen;
      }
      nwrite = conn_write_handshake_pkts(conn, pi, dest, destlen, 0, ts);
      if (nwrite < 0) {
        return nwrite;
      }
      if (nwrite > 0) {
        res = nwrite;
        dest += nwrite;
        destlen -= (size_t)nwrite;
      }
    }
  }

  if (conn->pktns.rtb.probe_pkt_left) {
    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_CON,
                    "transmit probe pkt left=%zu",
                    conn->pktns.rtb.probe_pkt_left);

    nwrite = conn_write_pkt(conn, pi, dest, destlen, vmsg, NGTCP2_PKT_SHORT,
                            wflags, ts);

    goto fin;
  }

  nwrite = conn_write_pkt(conn, pi, dest, destlen, vmsg, NGTCP2_PKT_SHORT,
                          wflags, ts);
  if (nwrite) {
    assert(nwrite != NGTCP2_ERR_NOBUF);
    goto fin;
  }

  if (res == 0) {
    nwrite = conn_write_ack_pkt(conn, pi, dest, origlen, NGTCP2_PKT_SHORT, ts);
  }

fin:
  conn->pkt.hs_spktlen = 0;

  if (nwrite >= 0) {
    res += nwrite;
    return res;
  }
  /* NGTCP2_CONN_FLAG_PPE_PENDING is set in conn_write_pkt above.
     ppe_pending cannot be used here. */
  if (conn->flags & NGTCP2_CONN_FLAG_PPE_PENDING) {
    conn->pkt.hs_spktlen = res;
  }

  return nwrite;
}

static ngtcp2_ssize
conn_write_connection_close(ngtcp2_conn *conn, ngtcp2_pkt_info *pi,
                            uint8_t *dest, size_t destlen, uint8_t pkt_type,
                            uint64_t error_code, ngtcp2_tstamp ts) {
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;
  ngtcp2_ssize res = 0, nwrite;
  ngtcp2_frame fr;

  fr.type = NGTCP2_FRAME_CONNECTION_CLOSE;
  fr.connection_close.error_code = error_code;
  fr.connection_close.frame_type = 0;
  fr.connection_close.reasonlen = 0;
  fr.connection_close.reason = NULL;

  if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED) &&
      pkt_type != NGTCP2_PKT_INITIAL) {
    if (in_pktns && conn->server) {
      nwrite = ngtcp2_conn_write_single_frame_pkt(
          conn, pi, dest, destlen, NGTCP2_PKT_INITIAL, &conn->dcid.current.cid,
          &fr, NGTCP2_RTB_ENTRY_FLAG_NONE, NULL, ts);
      if (nwrite < 0) {
        return nwrite;
      }

      dest += nwrite;
      destlen -= (size_t)nwrite;
      res += nwrite;
    }

    if (pkt_type != NGTCP2_PKT_HANDSHAKE && hs_pktns &&
        hs_pktns->crypto.tx.ckm) {
      nwrite = ngtcp2_conn_write_single_frame_pkt(
          conn, pi, dest, destlen, NGTCP2_PKT_HANDSHAKE,
          &conn->dcid.current.cid, &fr, NGTCP2_RTB_ENTRY_FLAG_NONE, NULL, ts);
      if (nwrite < 0) {
        return nwrite;
      }

      dest += nwrite;
      destlen -= (size_t)nwrite;
      res += nwrite;
    }
  }

  nwrite = ngtcp2_conn_write_single_frame_pkt(
      conn, pi, dest, destlen, pkt_type, &conn->dcid.current.cid, &fr,
      NGTCP2_RTB_ENTRY_FLAG_NONE, NULL, ts);

  if (nwrite < 0) {
    return nwrite;
  }

  res += nwrite;

  if (res == 0) {
    return NGTCP2_ERR_NOBUF;
  }

  return res;
}

ngtcp2_ssize ngtcp2_conn_write_connection_close(
    ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
    size_t destlen, uint64_t error_code, ngtcp2_tstamp ts) {
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;
  uint8_t pkt_type;
  ngtcp2_ssize nwrite;

  conn->log.last_ts = ts;
  conn->qlog.last_ts = ts;

  if (conn_check_pkt_num_exhausted(conn)) {
    return NGTCP2_ERR_PKT_NUM_EXHAUSTED;
  }

  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLOSING:
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_INVALID_STATE;
  default:
    break;
  }

  if (path) {
    ngtcp2_path_copy(path, &conn->dcid.current.ps.path);
  }

  if (pi) {
    pi->ecn = NGTCP2_ECN_NOT_ECT;
  }

  if (conn->state == NGTCP2_CS_POST_HANDSHAKE ||
      (conn->server && conn->pktns.crypto.tx.ckm)) {
    pkt_type = NGTCP2_PKT_SHORT;
  } else if (hs_pktns && hs_pktns->crypto.tx.ckm) {
    pkt_type = NGTCP2_PKT_HANDSHAKE;
  } else if (in_pktns && in_pktns->crypto.tx.ckm) {
    pkt_type = NGTCP2_PKT_INITIAL;
  } else {
    /* This branch is taken if server has not read any Initial packet
       from client. */
    return NGTCP2_ERR_INVALID_STATE;
  }

  nwrite = conn_write_connection_close(conn, pi, dest, destlen, pkt_type,
                                       error_code, ts);
  if (nwrite < 0) {
    return nwrite;
  }

  conn->state = NGTCP2_CS_CLOSING;

  return nwrite;
}

ngtcp2_ssize ngtcp2_conn_write_application_close(
    ngtcp2_conn *conn, ngtcp2_path *path, ngtcp2_pkt_info *pi, uint8_t *dest,
    size_t destlen, uint64_t app_error_code, ngtcp2_tstamp ts) {
  ngtcp2_ssize nwrite;
  ngtcp2_ssize res = 0;
  ngtcp2_frame fr;

  conn->log.last_ts = ts;
  conn->qlog.last_ts = ts;

  if (conn_check_pkt_num_exhausted(conn)) {
    return NGTCP2_ERR_PKT_NUM_EXHAUSTED;
  }

  switch (conn->state) {
  case NGTCP2_CS_CLIENT_INITIAL:
  case NGTCP2_CS_CLOSING:
  case NGTCP2_CS_DRAINING:
    return NGTCP2_ERR_INVALID_STATE;
  default:
    break;
  }

  if (path) {
    ngtcp2_path_copy(path, &conn->dcid.current.ps.path);
  }

  if (pi) {
    pi->ecn = NGTCP2_ECN_NOT_ECT;
  }

  if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED)) {
    nwrite = conn_write_connection_close(conn, pi, dest, destlen,
                                         conn->hs_pktns->crypto.tx.ckm
                                             ? NGTCP2_PKT_HANDSHAKE
                                             : NGTCP2_PKT_INITIAL,
                                         NGTCP2_APPLICATION_ERROR, ts);
    if (nwrite < 0) {
      return nwrite;
    }
    res = nwrite;
    dest += nwrite;
    destlen -= (size_t)nwrite;
  }

  if (conn->state != NGTCP2_CS_POST_HANDSHAKE) {
    assert(res);

    if (!conn->server || !conn->pktns.crypto.tx.ckm) {
      return res;
    }
  }

  assert(conn->pktns.crypto.tx.ckm);

  fr.type = NGTCP2_FRAME_CONNECTION_CLOSE_APP;
  fr.connection_close.error_code = app_error_code;
  fr.connection_close.frame_type = 0;
  fr.connection_close.reasonlen = 0;
  fr.connection_close.reason = NULL;

  nwrite = ngtcp2_conn_write_single_frame_pkt(
      conn, pi, dest, destlen, NGTCP2_PKT_SHORT, &conn->dcid.current.cid, &fr,
      NGTCP2_RTB_ENTRY_FLAG_NONE, NULL, ts);

  if (nwrite < 0) {
    return nwrite;
  }

  res += nwrite;

  if (res == 0) {
    return NGTCP2_ERR_NOBUF;
  }

  conn->state = NGTCP2_CS_CLOSING;

  return res;
}

int ngtcp2_conn_is_in_closing_period(ngtcp2_conn *conn) {
  return conn->state == NGTCP2_CS_CLOSING;
}

int ngtcp2_conn_is_in_draining_period(ngtcp2_conn *conn) {
  return conn->state == NGTCP2_CS_DRAINING;
}

int ngtcp2_conn_close_stream(ngtcp2_conn *conn, ngtcp2_strm *strm,
                             uint64_t app_error_code) {
  int rv;

  if (!strm->app_error_code) {
    app_error_code = strm->app_error_code;
  }

  rv = ngtcp2_map_remove(&conn->strms, strm->me.key);
  if (rv != 0) {
    assert(rv != NGTCP2_ERR_INVALID_ARGUMENT);
    return rv;
  }

  rv = conn_call_stream_close(conn, strm, app_error_code);
  if (rv != 0) {
    goto fin;
  }

  if (ngtcp2_strm_is_tx_queued(strm)) {
    ngtcp2_pq_remove(&conn->tx.strmq, &strm->pe);
  }

fin:
  ngtcp2_strm_free(strm);
  ngtcp2_mem_free(conn->mem, strm);

  return rv;
}

int ngtcp2_conn_close_stream_if_shut_rdwr(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                          uint64_t app_error_code) {
  if ((strm->flags & NGTCP2_STRM_FLAG_SHUT_RDWR) ==
          NGTCP2_STRM_FLAG_SHUT_RDWR &&
      ((strm->flags & NGTCP2_STRM_FLAG_RECV_RST) ||
       ngtcp2_strm_rx_offset(strm) == strm->rx.last_offset) &&
      (((strm->flags & NGTCP2_STRM_FLAG_SENT_RST) &&
        (strm->flags & NGTCP2_STRM_FLAG_RST_ACKED)) ||
       (!(strm->flags & NGTCP2_STRM_FLAG_SENT_RST) &&
        ngtcp2_strm_is_all_tx_data_acked(strm)))) {
    return ngtcp2_conn_close_stream(conn, strm, app_error_code);
  }
  return 0;
}

/*
 * conn_shutdown_stream_write closes send stream with error code
 * |app_error_code|.  RESET_STREAM frame is scheduled.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_shutdown_stream_write(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                      uint64_t app_error_code) {
  if (strm->flags & NGTCP2_STRM_FLAG_SENT_RST) {
    return 0;
  }

  /* Set this flag so that we don't accidentally send DATA to this
     stream. */
  strm->flags |= NGTCP2_STRM_FLAG_SHUT_WR | NGTCP2_STRM_FLAG_SENT_RST;
  strm->app_error_code = app_error_code;

  ngtcp2_strm_streamfrq_clear(strm);

  return conn_reset_stream(conn, strm, app_error_code);
}

/*
 * conn_shutdown_stream_read closes read stream with error code
 * |app_error_code|.  STOP_SENDING frame is scheduled.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_shutdown_stream_read(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                     uint64_t app_error_code) {
  if (strm->flags & NGTCP2_STRM_FLAG_STOP_SENDING) {
    return 0;
  }
  if ((strm->flags & NGTCP2_STRM_FLAG_SHUT_RD) &&
      ngtcp2_strm_rx_offset(strm) == strm->rx.last_offset) {
    return 0;
  }

  /* Extend connection flow control window for the amount of data
     which are not passed to application. */
  if (!(strm->flags &
        (NGTCP2_STRM_FLAG_STOP_SENDING | NGTCP2_STRM_FLAG_RECV_RST))) {
    ngtcp2_conn_extend_max_offset(conn, strm->rx.last_offset -
                                            ngtcp2_strm_rx_offset(strm));
  }

  strm->flags |= NGTCP2_STRM_FLAG_STOP_SENDING;
  strm->app_error_code = app_error_code;

  return conn_stop_sending(conn, strm, app_error_code);
}

int ngtcp2_conn_shutdown_stream(ngtcp2_conn *conn, int64_t stream_id,
                                uint64_t app_error_code) {
  int rv;
  ngtcp2_strm *strm;

  strm = ngtcp2_conn_find_stream(conn, stream_id);
  if (strm == NULL) {
    return NGTCP2_ERR_STREAM_NOT_FOUND;
  }

  rv = conn_shutdown_stream_read(conn, strm, app_error_code);
  if (rv != 0) {
    return rv;
  }

  rv = conn_shutdown_stream_write(conn, strm, app_error_code);
  if (rv != 0) {
    return rv;
  }

  return 0;
}

int ngtcp2_conn_shutdown_stream_write(ngtcp2_conn *conn, int64_t stream_id,
                                      uint64_t app_error_code) {
  ngtcp2_strm *strm;

  strm = ngtcp2_conn_find_stream(conn, stream_id);
  if (strm == NULL) {
    return NGTCP2_ERR_STREAM_NOT_FOUND;
  }

  return conn_shutdown_stream_write(conn, strm, app_error_code);
}

int ngtcp2_conn_shutdown_stream_read(ngtcp2_conn *conn, int64_t stream_id,
                                     uint64_t app_error_code) {
  ngtcp2_strm *strm;

  strm = ngtcp2_conn_find_stream(conn, stream_id);
  if (strm == NULL) {
    return NGTCP2_ERR_STREAM_NOT_FOUND;
  }

  return conn_shutdown_stream_read(conn, strm, app_error_code);
}

/*
 * conn_extend_max_stream_offset extends stream level flow control
 * window by |datalen| of the stream denoted by |strm|.
 *
 * This function returns 0 if it succeeds, or one of the following
 * negative error codes:
 *
 * NGTCP2_ERR_NOMEM
 *     Out of memory.
 */
static int conn_extend_max_stream_offset(ngtcp2_conn *conn, ngtcp2_strm *strm,
                                         uint64_t datalen) {
  ngtcp2_strm *top;

  if (datalen > NGTCP2_MAX_VARINT ||
      strm->rx.unsent_max_offset > NGTCP2_MAX_VARINT - datalen) {
    strm->rx.unsent_max_offset = NGTCP2_MAX_VARINT;
  } else {
    strm->rx.unsent_max_offset += datalen;
  }

  if (!(strm->flags &
        (NGTCP2_STRM_FLAG_SHUT_RD | NGTCP2_STRM_FLAG_STOP_SENDING)) &&
      !ngtcp2_strm_is_tx_queued(strm) &&
      conn_should_send_max_stream_data(conn, strm)) {
    if (!ngtcp2_pq_empty(&conn->tx.strmq)) {
      top = ngtcp2_conn_tx_strmq_top(conn);
      strm->cycle = top->cycle;
    }
    strm->cycle = conn_tx_strmq_first_cycle(conn);
    return ngtcp2_conn_tx_strmq_push(conn, strm);
  }

  return 0;
}

int ngtcp2_conn_extend_max_stream_offset(ngtcp2_conn *conn, int64_t stream_id,
                                         uint64_t datalen) {
  ngtcp2_strm *strm;

  strm = ngtcp2_conn_find_stream(conn, stream_id);
  if (strm == NULL) {
    return NGTCP2_ERR_STREAM_NOT_FOUND;
  }

  return conn_extend_max_stream_offset(conn, strm, datalen);
}

void ngtcp2_conn_extend_max_offset(ngtcp2_conn *conn, uint64_t datalen) {
  if (NGTCP2_MAX_VARINT < datalen ||
      conn->rx.unsent_max_offset > NGTCP2_MAX_VARINT - datalen) {
    conn->rx.unsent_max_offset = NGTCP2_MAX_VARINT;
    return;
  }

  conn->rx.unsent_max_offset += datalen;
}

void ngtcp2_conn_extend_max_streams_bidi(ngtcp2_conn *conn, size_t n) {
  handle_max_remote_streams_extension(&conn->remote.bidi.unsent_max_streams, n);
}

void ngtcp2_conn_extend_max_streams_uni(ngtcp2_conn *conn, size_t n) {
  handle_max_remote_streams_extension(&conn->remote.uni.unsent_max_streams, n);
}

const ngtcp2_cid *ngtcp2_conn_get_dcid(ngtcp2_conn *conn) {
  return &conn->dcid.current.cid;
}

uint32_t ngtcp2_conn_get_negotiated_version(ngtcp2_conn *conn) {
  return conn->version;
}

int ngtcp2_conn_early_data_rejected(ngtcp2_conn *conn) {
  ngtcp2_pktns *pktns = &conn->pktns;
  ngtcp2_rtb *rtb = &conn->pktns.rtb;
  int rv;

  conn->flags |= NGTCP2_CONN_FLAG_EARLY_DATA_REJECTED;

  rv = ngtcp2_rtb_remove_all(rtb, conn, pktns, &conn->cstat);
  if (rv != 0) {
    return rv;
  }

  return rv;
}

void ngtcp2_conn_update_rtt(ngtcp2_conn *conn, ngtcp2_duration rtt,
                            ngtcp2_duration ack_delay, ngtcp2_tstamp ts) {
  ngtcp2_conn_stat *cstat = &conn->cstat;
  ngtcp2_duration min_rtt;

  rtt = ngtcp2_max(rtt, NGTCP2_GRANULARITY);

  cstat->latest_rtt = rtt;

  if (cstat->min_rtt == UINT64_MAX) {
    cstat->min_rtt = rtt;
    cstat->smoothed_rtt = rtt;
    cstat->rttvar = rtt / 2;
    cstat->first_rtt_sample_ts = ts;
  } else {
    min_rtt = ngtcp2_min(cstat->min_rtt, rtt);
    if (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED) {
      ack_delay =
          ngtcp2_min(ack_delay, conn->remote.transport_params.max_ack_delay);
    } else if (ack_delay > 0 && rtt < cstat->min_rtt + ack_delay) {
      /* Ignore RTT sample if adjusting ack_delay causes the sample
         less than min_rtt before handshake confirmation. */
      ngtcp2_log_info(
          &conn->log, NGTCP2_LOG_EVENT_RCV,
          "ignore rtt sample because ack_delay is too large latest_rtt=%" PRIu64
          " min_rtt=%" PRIu64 " ack_delay=%" PRIu64,
          (uint64_t)(rtt / NGTCP2_MILLISECONDS),
          (uint64_t)(cstat->min_rtt / NGTCP2_MILLISECONDS),
          (uint64_t)(ack_delay / NGTCP2_MILLISECONDS));
      return;
    }

    if (rtt > min_rtt + ack_delay) {
      rtt -= ack_delay;
    }

    cstat->min_rtt = min_rtt;
    cstat->rttvar = (cstat->rttvar * 3 + (cstat->smoothed_rtt < rtt
                                              ? rtt - cstat->smoothed_rtt
                                              : cstat->smoothed_rtt - rtt)) /
                    4;
    cstat->smoothed_rtt = (cstat->smoothed_rtt * 7 + rtt) / 8;
  }

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV,
                  "latest_rtt=%" PRIu64 " min_rtt=%" PRIu64
                  " smoothed_rtt=%" PRIu64 " rttvar=%" PRIu64
                  " ack_delay=%" PRIu64,
                  (uint64_t)(cstat->latest_rtt / NGTCP2_MILLISECONDS),
                  (uint64_t)(cstat->min_rtt / NGTCP2_MILLISECONDS),
                  cstat->smoothed_rtt / NGTCP2_MILLISECONDS,
                  cstat->rttvar / NGTCP2_MILLISECONDS,
                  (uint64_t)(ack_delay / NGTCP2_MILLISECONDS));
}

void ngtcp2_conn_get_conn_stat(ngtcp2_conn *conn, ngtcp2_conn_stat *cstat) {
  *cstat = conn->cstat;
}

static ngtcp2_pktns *conn_get_earliest_pktns(ngtcp2_conn *conn,
                                             ngtcp2_tstamp *pts,
                                             const ngtcp2_tstamp *times) {
  ngtcp2_pktns *ns[] = {conn->in_pktns, conn->hs_pktns, &conn->pktns};
  ngtcp2_pktns *res = ns[0];
  size_t i;
  ngtcp2_tstamp earliest_ts = times[NGTCP2_PKTNS_ID_INITIAL];

  for (i = NGTCP2_PKTNS_ID_HANDSHAKE; i < NGTCP2_PKTNS_ID_MAX; ++i) {
    if (ns[i] == NULL || ns[i]->rtb.num_retransmittable == 0 ||
        (times[i] == UINT64_MAX ||
         (earliest_ts != UINT64_MAX && times[i] >= earliest_ts) ||
         (i == NGTCP2_PKTNS_ID_APPLICATION &&
          !(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED)))) {
      continue;
    }

    earliest_ts = times[i];
    res = ns[i];
  }

  if (res == NULL && !conn->server) {
    earliest_ts = UINT64_MAX;

    if (conn->hs_pktns && conn->hs_pktns->crypto.tx.ckm) {
      res = conn->hs_pktns;
    } else {
      res = conn->in_pktns;
    }
  }

  if (pts) {
    *pts = earliest_ts;
  }
  return res;
}

void ngtcp2_conn_set_loss_detection_timer(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  ngtcp2_conn_stat *cstat = &conn->cstat;
  ngtcp2_duration timeout;
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;
  ngtcp2_pktns *pktns = &conn->pktns;
  ngtcp2_pktns *earliest_pktns;
  ngtcp2_tstamp earliest_loss_time;
  ngtcp2_tstamp last_tx_pkt_ts;

  conn_get_earliest_pktns(conn, &earliest_loss_time, cstat->loss_time);

  if (earliest_loss_time != UINT64_MAX) {
    cstat->loss_detection_timer = earliest_loss_time;

    ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV,
                    "loss_detection_timer=%" PRIu64 " nonzero crypto loss time",
                    cstat->loss_detection_timer);
    return;
  }

  if ((!in_pktns || in_pktns->rtb.num_retransmittable == 0) &&
      (!hs_pktns || hs_pktns->rtb.num_retransmittable == 0) &&
      (pktns->rtb.num_retransmittable == 0 ||
       !(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED)) &&
      (conn->server ||
       (conn->flags & (NGTCP2_CONN_FLAG_SERVER_ADDR_VERIFIED |
                       NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED)))) {
    if (cstat->loss_detection_timer != UINT64_MAX) {
      ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV,
                      "loss detection timer canceled");
      cstat->loss_detection_timer = UINT64_MAX;
      cstat->pto_count = 0;
    }
    return;
  }

  earliest_pktns =
      conn_get_earliest_pktns(conn, &last_tx_pkt_ts, cstat->last_tx_pkt_ts);

  assert(earliest_pktns);

  if (last_tx_pkt_ts == UINT64_MAX) {
    last_tx_pkt_ts = ts;
  }

  timeout = conn_compute_pto(conn, earliest_pktns) * (1ULL << cstat->pto_count);

  cstat->loss_detection_timer = last_tx_pkt_ts + timeout;

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV,
                  "loss_detection_timer=%" PRIu64 " last_tx_pkt_ts=%" PRIu64
                  " timeout=%" PRIu64,
                  cstat->loss_detection_timer, last_tx_pkt_ts,
                  (uint64_t)(timeout / NGTCP2_MILLISECONDS));
}

int ngtcp2_conn_on_loss_detection_timer(ngtcp2_conn *conn, ngtcp2_tstamp ts) {
  ngtcp2_conn_stat *cstat = &conn->cstat;
  int rv;
  ngtcp2_pktns *in_pktns = conn->in_pktns;
  ngtcp2_pktns *hs_pktns = conn->hs_pktns;
  ngtcp2_tstamp earliest_loss_time;
  ngtcp2_pktns *loss_pktns =
      conn_get_earliest_pktns(conn, &earliest_loss_time, cstat->loss_time);
  ngtcp2_pktns *earliest_pktns;

  conn->log.last_ts = ts;
  conn->qlog.last_ts = ts;

  switch (conn->state) {
  case NGTCP2_CS_CLOSING:
  case NGTCP2_CS_DRAINING:
    cstat->loss_detection_timer = UINT64_MAX;
    cstat->pto_count = 0;
    return 0;
  default:
    break;
  }

  if (cstat->loss_detection_timer == UINT64_MAX) {
    return 0;
  }

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV,
                  "loss detection timer fired");

  if (earliest_loss_time != UINT64_MAX) {
    rv = ngtcp2_conn_detect_lost_pkt(conn, loss_pktns, cstat, ts);
    if (rv != 0) {
      return rv;
    }
    ngtcp2_conn_set_loss_detection_timer(conn, ts);
    return 0;
  }

  if (!conn->server && !(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)) {
    if (hs_pktns->crypto.tx.ckm) {
      hs_pktns->rtb.probe_pkt_left = 1;
    } else {
      in_pktns->rtb.probe_pkt_left = 1;
    }
  } else {
    earliest_pktns = conn_get_earliest_pktns(conn, NULL, cstat->last_tx_pkt_ts);

    assert(earliest_pktns);

    switch (earliest_pktns->rtb.pktns_id) {
    case NGTCP2_PKTNS_ID_INITIAL:
      assert(in_pktns);
      in_pktns->rtb.probe_pkt_left = 1;
      if (!conn->server) {
        break;
      }
      /* fall through for server so that it can coalesce packets. */
      /* fall through */
    case NGTCP2_PKTNS_ID_HANDSHAKE:
      assert(hs_pktns);
      hs_pktns->rtb.probe_pkt_left = 1;
      break;
    case NGTCP2_PKTNS_ID_APPLICATION:
      conn->pktns.rtb.probe_pkt_left = 2;
      break;
    default:
      assert(0);
    }
  }

  ++cstat->pto_count;

  ngtcp2_log_info(&conn->log, NGTCP2_LOG_EVENT_RCV, "pto_count=%zu",
                  cstat->pto_count);

  ngtcp2_conn_set_loss_detection_timer(conn, ts);

  return 0;
}

int ngtcp2_conn_submit_crypto_data(ngtcp2_conn *conn,
                                   ngtcp2_crypto_level crypto_level,
                                   const uint8_t *data, const size_t datalen) {
  ngtcp2_pktns *pktns;
  ngtcp2_frame_chain *frc;
  ngtcp2_crypto *fr;
  int rv;

  if (datalen == 0) {
    return 0;
  }

  switch (crypto_level) {
  case NGTCP2_CRYPTO_LEVEL_INITIAL:
    assert(conn->in_pktns);
    pktns = conn->in_pktns;
    break;
  case NGTCP2_CRYPTO_LEVEL_HANDSHAKE:
    assert(conn->hs_pktns);
    pktns = conn->hs_pktns;
    break;
  case NGTCP2_CRYPTO_LEVEL_APPLICATION:
    pktns = &conn->pktns;
    break;
  default:
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  rv = ngtcp2_frame_chain_new(&frc, conn->mem);
  if (rv != 0) {
    return rv;
  }

  fr = &frc->fr.crypto;

  fr->type = NGTCP2_FRAME_CRYPTO;
  fr->offset = pktns->crypto.tx.offset;
  fr->datacnt = 1;
  fr->data[0].len = datalen;
  fr->data[0].base = (uint8_t *)data;

  rv = ngtcp2_ksl_insert(&pktns->crypto.tx.frq, NULL, &fr->offset, frc);
  if (rv != 0) {
    ngtcp2_frame_chain_del(frc, conn->mem);
    return rv;
  }

  pktns->crypto.strm.tx.offset += datalen;
  pktns->crypto.tx.offset += datalen;

  return 0;
}

int ngtcp2_conn_submit_new_token(ngtcp2_conn *conn, const uint8_t *token,
                                 size_t tokenlen) {
  int rv;
  ngtcp2_frame_chain *nfrc;
  uint8_t *p;

  assert(conn->server);
  assert(token);
  assert(tokenlen);

  rv = ngtcp2_frame_chain_extralen_new(&nfrc, tokenlen, conn->mem);
  if (rv != 0) {
    return rv;
  }

  nfrc->fr.type = NGTCP2_FRAME_NEW_TOKEN;

  p = (uint8_t *)nfrc + sizeof(*nfrc);
  memcpy(p, token, tokenlen);

  ngtcp2_vec_init(&nfrc->fr.new_token.token, p, tokenlen);

  nfrc->next = conn->pktns.tx.frq;
  conn->pktns.tx.frq = nfrc;

  return 0;
}

ngtcp2_strm *ngtcp2_conn_tx_strmq_top(ngtcp2_conn *conn) {
  assert(!ngtcp2_pq_empty(&conn->tx.strmq));
  return ngtcp2_struct_of(ngtcp2_pq_top(&conn->tx.strmq), ngtcp2_strm, pe);
}

void ngtcp2_conn_tx_strmq_pop(ngtcp2_conn *conn) {
  ngtcp2_strm *strm = ngtcp2_conn_tx_strmq_top(conn);
  assert(strm);
  ngtcp2_pq_pop(&conn->tx.strmq);
  strm->pe.index = NGTCP2_PQ_BAD_INDEX;
}

int ngtcp2_conn_tx_strmq_push(ngtcp2_conn *conn, ngtcp2_strm *strm) {
  return ngtcp2_pq_push(&conn->tx.strmq, &strm->pe);
}

size_t ngtcp2_conn_get_num_scid(ngtcp2_conn *conn) {
  return ngtcp2_ksl_len(&conn->scid.set);
}

size_t ngtcp2_conn_get_scid(ngtcp2_conn *conn, ngtcp2_cid *dest) {
  ngtcp2_ksl_it it;
  ngtcp2_scid *scid;

  for (it = ngtcp2_ksl_begin(&conn->scid.set); !ngtcp2_ksl_it_end(&it);
       ngtcp2_ksl_it_next(&it)) {
    scid = ngtcp2_ksl_it_get(&it);
    *dest++ = scid->cid;
  }

  return ngtcp2_ksl_len(&conn->scid.set);
}

size_t ngtcp2_conn_get_num_active_dcid(ngtcp2_conn *conn) {
  size_t n = 1; /* for conn->dcid.current */
  ngtcp2_pv *pv = conn->pv;

  if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED)) {
    return 0;
  }

  if (pv) {
    if (pv->dcid.seq != conn->dcid.current.seq) {
      ++n;
    }
    if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
        pv->fallback_dcid.seq != conn->dcid.current.seq &&
        pv->fallback_dcid.seq != pv->dcid.seq) {
      ++n;
    }
  }

  n += ngtcp2_ringbuf_len(&conn->dcid.retired);

  return n;
}

static void copy_dcid_to_cid_token(ngtcp2_cid_token *dest,
                                   const ngtcp2_dcid *src) {
  dest->seq = src->seq;
  dest->cid = src->cid;
  ngtcp2_path_storage_init2(&dest->ps, &src->ps.path);
  dest->token_present =
      (uint8_t)!ngtcp2_check_invalid_stateless_reset_token(src->token);
  memcpy(dest->token, src->token, NGTCP2_STATELESS_RESET_TOKENLEN);
}

size_t ngtcp2_conn_get_active_dcid(ngtcp2_conn *conn, ngtcp2_cid_token *dest) {
  ngtcp2_pv *pv = conn->pv;
  ngtcp2_cid_token *orig = dest;
  ngtcp2_dcid *dcid;
  size_t len, i;

  if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED_HANDLED)) {
    return 0;
  }

  copy_dcid_to_cid_token(dest, &conn->dcid.current);
  ++dest;

  if (pv) {
    if (pv->dcid.seq != conn->dcid.current.seq) {
      copy_dcid_to_cid_token(dest, &pv->dcid);
      ++dest;
    }
    if ((pv->flags & NGTCP2_PV_FLAG_FALLBACK_ON_FAILURE) &&
        pv->fallback_dcid.seq != conn->dcid.current.seq &&
        pv->fallback_dcid.seq != pv->dcid.seq) {
      copy_dcid_to_cid_token(dest, &pv->fallback_dcid);
      ++dest;
    }
  }

  len = ngtcp2_ringbuf_len(&conn->dcid.retired);
  for (i = 0; i < len; ++i) {
    dcid = ngtcp2_ringbuf_get(&conn->dcid.retired, i);
    copy_dcid_to_cid_token(dest, dcid);
    ++dest;
  }

  return (size_t)(dest - orig);
}

void ngtcp2_conn_set_local_addr(ngtcp2_conn *conn, const ngtcp2_addr *addr) {
  ngtcp2_addr *dest = &conn->dcid.current.ps.path.local;

  assert(addr->addrlen <= sizeof(conn->dcid.current.ps.local_addrbuf));
  ngtcp2_addr_copy(dest, addr);
}

void ngtcp2_conn_set_remote_addr(ngtcp2_conn *conn, const ngtcp2_addr *addr) {
  ngtcp2_addr *dest = &conn->dcid.current.ps.path.remote;

  assert(addr->addrlen <= sizeof(conn->dcid.current.ps.remote_addrbuf));
  ngtcp2_addr_copy(dest, addr);
}

const ngtcp2_path *ngtcp2_conn_get_path(ngtcp2_conn *conn) {
  return &conn->dcid.current.ps.path;
}

int ngtcp2_conn_initiate_migration(ngtcp2_conn *conn, const ngtcp2_path *path,
                                   ngtcp2_tstamp ts) {
  int rv;
  ngtcp2_dcid *dcid;

  assert(!conn->server);

  conn->log.last_ts = ts;
  conn->qlog.last_ts = ts;

  if (conn->remote.transport_params.disable_active_migration ||
      conn->dcid.current.cid.datalen == 0 ||
      !(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_CONFIRMED)) {
    return NGTCP2_ERR_INVALID_STATE;
  }
  if (ngtcp2_ringbuf_len(&conn->dcid.unused) == 0) {
    return NGTCP2_ERR_CONN_ID_BLOCKED;
  }

  if (ngtcp2_path_eq(&conn->dcid.current.ps.path, path)) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  dcid = ngtcp2_ringbuf_get(&conn->dcid.unused, 0);

  rv = conn_stop_pv(conn, ts);
  if (rv != 0) {
    return rv;
  }

  rv = conn_retire_dcid(conn, &conn->dcid.current, ts);
  if (rv != 0) {
    return rv;
  }

  ngtcp2_dcid_copy(&conn->dcid.current, dcid);
  ngtcp2_path_copy(&conn->dcid.current.ps.path, path);
  ngtcp2_ringbuf_pop_front(&conn->dcid.unused);

  rv = conn_call_activate_dcid(conn, &conn->dcid.current);
  if (rv != 0) {
    return rv;
  }

  conn_reset_congestion_state(conn);
  conn_reset_ecn_validation_state(conn);

  return 0;
}

uint64_t ngtcp2_conn_get_max_local_streams_uni(ngtcp2_conn *conn) {
  return conn->local.uni.max_streams;
}

uint64_t ngtcp2_conn_get_max_data_left(ngtcp2_conn *conn) {
  return conn->tx.max_offset - conn->tx.offset;
}

uint64_t ngtcp2_conn_get_streams_bidi_left(ngtcp2_conn *conn) {
  uint64_t n = ngtcp2_ord_stream_id(conn->local.bidi.next_stream_id);

  return n > conn->local.bidi.max_streams
             ? 0
             : conn->local.bidi.max_streams - n + 1;
}

uint64_t ngtcp2_conn_get_streams_uni_left(ngtcp2_conn *conn) {
  uint64_t n = ngtcp2_ord_stream_id(conn->local.uni.next_stream_id);

  return n > conn->local.uni.max_streams ? 0
                                         : conn->local.uni.max_streams - n + 1;
}

ngtcp2_tstamp ngtcp2_conn_get_idle_expiry(ngtcp2_conn *conn) {
  ngtcp2_duration trpto;
  ngtcp2_duration idle_timeout;

  /* TODO Remote max_idle_timeout becomes effective after handshake
     completion. */

  if (!(conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED) ||
      conn->remote.transport_params.max_idle_timeout == 0 ||
      (conn->local.transport_params.max_idle_timeout &&
       conn->local.transport_params.max_idle_timeout <
           conn->remote.transport_params.max_idle_timeout)) {
    idle_timeout = conn->local.transport_params.max_idle_timeout;
  } else {
    idle_timeout = conn->remote.transport_params.max_idle_timeout;
  }

  if (idle_timeout == 0) {
    return UINT64_MAX;
  }

  trpto = 3 * conn_compute_pto(
                  conn, (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)
                            ? &conn->pktns
                            : conn->hs_pktns);

  return conn->idle_ts + ngtcp2_max(idle_timeout, trpto);
}

ngtcp2_duration ngtcp2_conn_get_pto(ngtcp2_conn *conn) {
  return conn_compute_pto(conn,
                          (conn->flags & NGTCP2_CONN_FLAG_HANDSHAKE_COMPLETED)
                              ? &conn->pktns
                              : conn->hs_pktns);
}

void ngtcp2_conn_set_initial_crypto_ctx(ngtcp2_conn *conn,
                                        const ngtcp2_crypto_ctx *ctx) {
  assert(conn->in_pktns);
  conn->in_pktns->crypto.ctx = *ctx;
}

const ngtcp2_crypto_ctx *ngtcp2_conn_get_initial_crypto_ctx(ngtcp2_conn *conn) {
  assert(conn->in_pktns);
  return &conn->in_pktns->crypto.ctx;
}

void ngtcp2_conn_set_retry_aead(ngtcp2_conn *conn,
                                const ngtcp2_crypto_aead *aead,
                                const ngtcp2_crypto_aead_ctx *aead_ctx) {
  assert(!conn->crypto.retry_aead_ctx.native_handle);

  conn->crypto.retry_aead = *aead;
  conn->crypto.retry_aead_ctx = *aead_ctx;
}

void ngtcp2_conn_set_crypto_ctx(ngtcp2_conn *conn,
                                const ngtcp2_crypto_ctx *ctx) {
  assert(conn->hs_pktns);
  conn->hs_pktns->crypto.ctx = *ctx;
  conn->pktns.crypto.ctx = *ctx;
}

const ngtcp2_crypto_ctx *ngtcp2_conn_get_crypto_ctx(ngtcp2_conn *conn) {
  return &conn->pktns.crypto.ctx;
}

void ngtcp2_conn_set_early_crypto_ctx(ngtcp2_conn *conn,
                                      const ngtcp2_crypto_ctx *ctx) {
  conn->early.ctx = *ctx;
}

const ngtcp2_crypto_ctx *ngtcp2_conn_get_early_crypto_ctx(ngtcp2_conn *conn) {
  return &conn->early.ctx;
}

void *ngtcp2_conn_get_tls_native_handle(ngtcp2_conn *conn) {
  return conn->crypto.tls_native_handle;
}

void ngtcp2_conn_set_tls_native_handle(ngtcp2_conn *conn,
                                       void *tls_native_handle) {
  conn->crypto.tls_native_handle = tls_native_handle;
}

void ngtcp2_conn_get_connection_close_error_code(
    ngtcp2_conn *conn, ngtcp2_connection_close_error_code *ccec) {
  *ccec = conn->rx.ccec;
}

void ngtcp2_conn_set_tls_error(ngtcp2_conn *conn, int liberr) {
  conn->crypto.tls_error = liberr;
}

int ngtcp2_conn_get_tls_error(ngtcp2_conn *conn) {
  return conn->crypto.tls_error;
}

int ngtcp2_conn_is_local_stream(ngtcp2_conn *conn, int64_t stream_id) {
  return conn_local_stream(conn, stream_id);
}

int ngtcp2_conn_is_server(ngtcp2_conn *conn) { return conn->server; }

int ngtcp2_conn_after_retry(ngtcp2_conn *conn) {
  return (conn->flags & NGTCP2_CONN_FLAG_RECV_RETRY) != 0;
}

int ngtcp2_conn_set_stream_user_data(ngtcp2_conn *conn, int64_t stream_id,
                                     void *stream_user_data) {
  ngtcp2_strm *strm = ngtcp2_conn_find_stream(conn, stream_id);

  if (strm == NULL) {
    return NGTCP2_ERR_STREAM_NOT_FOUND;
  }

  strm->stream_user_data = stream_user_data;

  return 0;
}

void ngtcp2_path_challenge_entry_init(ngtcp2_path_challenge_entry *pcent,
                                      const ngtcp2_path *path,
                                      const uint8_t *data) {
  ngtcp2_path_storage_init2(&pcent->ps, path);
  memcpy(pcent->data, data, sizeof(pcent->data));
}

void ngtcp2_settings_default(ngtcp2_settings *settings) {
  memset(settings, 0, sizeof(*settings));
  settings->cc_algo = NGTCP2_CC_ALGO_CUBIC;
  settings->initial_rtt = NGTCP2_DEFAULT_INITIAL_RTT;
  settings->ack_thresh = 2;
}

void ngtcp2_transport_params_default(ngtcp2_transport_params *params) {
  memset(params, 0, sizeof(*params));
  params->max_udp_payload_size = NGTCP2_DEFAULT_MAX_UDP_PAYLOAD_SIZE;
  params->ack_delay_exponent = NGTCP2_DEFAULT_ACK_DELAY_EXPONENT;
  params->max_ack_delay = NGTCP2_DEFAULT_MAX_ACK_DELAY;
  params->active_connection_id_limit =
      NGTCP2_DEFAULT_ACTIVE_CONNECTION_ID_LIMIT;
}

/* The functions prefixed with ngtcp2_pkt_ are usually put inside
   ngtcp2_pkt.c.  This function uses encryption construct and uses
   test data defined only in ngtcp2_conn_test.c, so it is written
   here. */
ngtcp2_ssize ngtcp2_pkt_write_connection_close(
    uint8_t *dest, size_t destlen, uint32_t version, const ngtcp2_cid *dcid,
    const ngtcp2_cid *scid, uint64_t error_code, ngtcp2_encrypt encrypt,
    const ngtcp2_crypto_aead *aead, const ngtcp2_crypto_aead_ctx *aead_ctx,
    const uint8_t *iv, ngtcp2_hp_mask hp_mask, const ngtcp2_crypto_cipher *hp,
    const ngtcp2_crypto_cipher_ctx *hp_ctx) {
  ngtcp2_pkt_hd hd;
  ngtcp2_crypto_km ckm;
  ngtcp2_crypto_cc cc;
  ngtcp2_ppe ppe;
  ngtcp2_frame fr = {0};
  int rv;

  ngtcp2_pkt_hd_init(&hd, NGTCP2_PKT_FLAG_LONG_FORM, NGTCP2_PKT_INITIAL, dcid,
                     scid, /* pkt_num = */ 0, /* pkt_numlen = */ 1, version,
                     /* len = */ 0);

  ngtcp2_vec_init(&ckm.secret, NULL, 0);
  ngtcp2_vec_init(&ckm.iv, iv, 12);
  ckm.aead_ctx = *aead_ctx;
  ckm.pkt_num = 0;
  ckm.flags = NGTCP2_CRYPTO_KM_FLAG_NONE;

  cc.aead = *aead;
  cc.hp = *hp;
  cc.ckm = &ckm;
  cc.hp_ctx = *hp_ctx;
  cc.encrypt = encrypt;
  cc.hp_mask = hp_mask;

  ngtcp2_ppe_init(&ppe, dest, destlen, &cc);

  rv = ngtcp2_ppe_encode_hd(&ppe, &hd);
  if (rv != 0) {
    assert(NGTCP2_ERR_NOBUF == rv);
    return rv;
  }

  if (!ngtcp2_ppe_ensure_hp_sample(&ppe)) {
    return NGTCP2_ERR_NOBUF;
  }

  fr.type = NGTCP2_FRAME_CONNECTION_CLOSE;
  fr.connection_close.error_code = error_code;

  rv = ngtcp2_ppe_encode_frame(&ppe, &fr);
  if (rv != 0) {
    assert(NGTCP2_ERR_NOBUF == rv);
    return rv;
  }

  return ngtcp2_ppe_final(&ppe, NULL);
}

int ngtcp2_is_bidi_stream(int64_t stream_id) { return bidi_stream(stream_id); }

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
#include "ngtcp2_cc.h"
#include "ngtcp2_log.h"
#include "ngtcp2_macro.h"
#include "ngtcp2_rst.h"

ngtcp2_cc_pkt *ngtcp2_cc_pkt_init(ngtcp2_cc_pkt *pkt, int64_t pkt_num,
                                  size_t pktlen, ngtcp2_tstamp ts_sent) {
  pkt->pkt_num = pkt_num;
  pkt->pktlen = pktlen;
  pkt->ts_sent = ts_sent;

  return pkt;
}

void ngtcp2_default_cc_init(ngtcp2_default_cc *cc, ngtcp2_cc_stat *ccs,
                            ngtcp2_rst *rst, ngtcp2_log *log) {
  cc->log = log;
  cc->ccs = ccs;
  cc->rst = rst;
  cc->max_delivery_rate = 0.;
  cc->min_rtt = 0;
  cc->min_rtt_ts = 0;
  cc->target_cwnd = 0;
}

void ngtcp2_default_cc_free(ngtcp2_default_cc *cc) { (void)cc; }

static int default_cc_in_congestion_recovery(ngtcp2_default_cc *cc,
                                             ngtcp2_tstamp sent_time) {
  return sent_time <= cc->ccs->congestion_recovery_start_ts;
}

void ngtcp2_default_cc_on_pkt_acked(ngtcp2_default_cc *cc,
                                    const ngtcp2_cc_pkt *pkt) {
  ngtcp2_cc_stat *ccs = cc->ccs;

  if (default_cc_in_congestion_recovery(cc, pkt->ts_sent)) {
    return;
  }

  if (cc->target_cwnd && ccs->cwnd >= cc->target_cwnd) {
    return;
  }

  if (ccs->cwnd < ccs->ssthresh) {
    ccs->cwnd += pkt->pktlen;
    ngtcp2_log_info(cc->log, NGTCP2_LOG_EVENT_RCV,
                    "pkn=%" PRId64 " acked, slow start cwnd=%lu", pkt->pkt_num,
                    ccs->cwnd);
    return;
  }

  ccs->cwnd += NGTCP2_MAX_DGRAM_SIZE * pkt->pktlen / ccs->cwnd;
}

void ngtcp2_default_cc_congestion_event(ngtcp2_default_cc *cc,
                                        ngtcp2_tstamp ts_sent,
                                        ngtcp2_tstamp ts) {
  ngtcp2_cc_stat *ccs = cc->ccs;

  if (default_cc_in_congestion_recovery(cc, ts_sent)) {
    return;
  }
  ccs->congestion_recovery_start_ts = ts;
  ccs->cwnd >>= NGTCP2_LOSS_REDUCTION_FACTOR_BITS;
  ccs->cwnd = ngtcp2_max(ccs->cwnd, NGTCP2_MIN_CWND);
  ccs->ssthresh = ccs->cwnd;

  ngtcp2_log_info(cc->log, NGTCP2_LOG_EVENT_RCV,
                  "reduce cwnd because of packet loss cwnd=%lu", ccs->cwnd);
}

void ngtcp2_default_cc_handle_persistent_congestion(ngtcp2_default_cc *cc,
                                                    ngtcp2_duration loss_window,
                                                    ngtcp2_duration pto) {
  ngtcp2_cc_stat *ccs = cc->ccs;
  ngtcp2_duration congestion_period =
      pto * NGTCP2_PERSISTENT_CONGESTION_THRESHOLD;

  if (loss_window >= congestion_period) {
    ngtcp2_log_info(cc->log, NGTCP2_LOG_EVENT_RCV,
                    "persistent congestion loss_window=%" PRIu64
                    " congestion_period=%" PRIu64,
                    loss_window, congestion_period);

    ccs->cwnd = NGTCP2_MIN_CWND;
  }
}

void ngtcp2_default_cc_on_ack_recv(ngtcp2_default_cc *cc,
                                   ngtcp2_duration latest_rtt,
                                   ngtcp2_tstamp ts) {
  /* TODO Use sliding window */
  if (latest_rtt && (cc->min_rtt == 0 || cc->min_rtt > latest_rtt)) {
    cc->min_rtt = latest_rtt;
    cc->min_rtt_ts = ts;
  }

  /* TODO Use sliding window */
  cc->max_delivery_rate =
      ngtcp2_max(cc->max_delivery_rate, cc->rst->rs.delivery_rate);

  if (cc->min_rtt && cc->max_delivery_rate > 1e-9) {
    uint64_t target_cwnd =
        (uint64_t)(2.89 * cc->max_delivery_rate * (double)cc->min_rtt);
    cc->target_cwnd = ngtcp2_max(NGTCP2_MIN_CWND, target_cwnd);

    ngtcp2_log_info(cc->log, NGTCP2_LOG_EVENT_RCV,
                    "target_cwnd=%lu max_delivery_rate=%.02f min_rtt=%lu",
                    cc->target_cwnd, cc->max_delivery_rate * 1000000000,
                    cc->min_rtt);
  }
}

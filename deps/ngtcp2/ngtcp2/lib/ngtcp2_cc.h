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
#ifndef NGTCP2_CC_H
#define NGTCP2_CC_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2.h>

#define NGTCP2_LOSS_REDUCTION_FACTOR_BITS 1
#define NGTCP2_PERSISTENT_CONGESTION_THRESHOLD 3

typedef struct ngtcp2_log ngtcp2_log;

/*
 * ngtcp2_cc_compute_initcwnd computes initial cwnd.
 */
uint64_t ngtcp2_cc_compute_initcwnd(size_t max_packet_size);

ngtcp2_cc_pkt *ngtcp2_cc_pkt_init(ngtcp2_cc_pkt *pkt, int64_t pkt_num,
                                  size_t pktlen, ngtcp2_pktns_id pktns_id,
                                  ngtcp2_tstamp ts_sent);

/* ngtcp2_reno_cc is the RENO congestion controller. */
typedef struct ngtcp2_reno_cc {
  ngtcp2_cc_base ccb;
  uint64_t max_delivery_rate_sec;
  uint64_t target_cwnd;
  uint64_t pending_add;
} ngtcp2_reno_cc;

int ngtcp2_cc_reno_cc_init(ngtcp2_cc *cc, ngtcp2_log *log,
                           const ngtcp2_mem *mem);

void ngtcp2_cc_reno_cc_free(ngtcp2_cc *cc, const ngtcp2_mem *mem);

void ngtcp2_reno_cc_init(ngtcp2_reno_cc *cc, ngtcp2_log *log);

void ngtcp2_reno_cc_free(ngtcp2_reno_cc *cc);

void ngtcp2_cc_reno_cc_on_pkt_acked(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                    const ngtcp2_cc_pkt *pkt, ngtcp2_tstamp ts);

void ngtcp2_cc_reno_cc_congestion_event(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                        ngtcp2_tstamp ts_sent,
                                        ngtcp2_tstamp ts);

void ngtcp2_cc_reno_cc_on_persistent_congestion(ngtcp2_cc *cc,
                                                ngtcp2_conn_stat *cstat,
                                                ngtcp2_tstamp ts);

void ngtcp2_cc_reno_cc_on_ack_recv(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                   ngtcp2_tstamp ts);

void ngtcp2_cc_reno_cc_reset(ngtcp2_cc *cc);

/* ngtcp2_cubic_cc is CUBIC congestion controller. */
typedef struct ngtcp2_cubic_cc {
  ngtcp2_cc_base ccb;
  uint64_t max_delivery_rate_sec;
  uint64_t target_cwnd;
  uint64_t w_last_max;
  uint64_t w_tcp;
  uint64_t origin_point;
  ngtcp2_tstamp epoch_start;
  uint64_t k;
  /* HyStart++ variables */
  size_t rtt_sample_count;
  uint64_t current_round_min_rtt;
  uint64_t last_round_min_rtt;
  int64_t window_end;
  uint64_t pending_add;
  uint64_t pending_w_add;
} ngtcp2_cubic_cc;

int ngtcp2_cc_cubic_cc_init(ngtcp2_cc *cc, ngtcp2_log *log,
                            const ngtcp2_mem *mem);

void ngtcp2_cc_cubic_cc_free(ngtcp2_cc *cc, const ngtcp2_mem *mem);

void ngtcp2_cubic_cc_init(ngtcp2_cubic_cc *cc, ngtcp2_log *log);

void ngtcp2_cubic_cc_free(ngtcp2_cubic_cc *cc);

void ngtcp2_cc_cubic_cc_on_pkt_acked(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                     const ngtcp2_cc_pkt *pkt,
                                     ngtcp2_tstamp ts);

void ngtcp2_cc_cubic_cc_congestion_event(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                         ngtcp2_tstamp ts_sent,
                                         ngtcp2_tstamp ts);

void ngtcp2_cc_cubic_cc_on_persistent_congestion(ngtcp2_cc *cc,
                                                 ngtcp2_conn_stat *cstat,
                                                 ngtcp2_tstamp ts);

void ngtcp2_cc_cubic_cc_on_ack_recv(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                    ngtcp2_tstamp ts);

void ngtcp2_cc_cubic_cc_on_pkt_sent(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                    const ngtcp2_cc_pkt *pkt);

void ngtcp2_cc_cubic_cc_new_rtt_sample(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                       ngtcp2_tstamp ts);

void ngtcp2_cc_cubic_cc_reset(ngtcp2_cc *cc);

void ngtcp2_cc_cubic_cc_event(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                              ngtcp2_cc_event_type event, ngtcp2_tstamp ts);

#endif /* NGTCP2_CC_H */

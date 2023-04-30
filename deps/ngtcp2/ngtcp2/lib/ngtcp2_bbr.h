/*
 * ngtcp2
 *
 * Copyright (c) 2021 ngtcp2 contributors
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
#ifndef NGTCP2_BBR_H
#define NGTCP2_BBR_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2.h>

#include "ngtcp2_cc.h"
#include "ngtcp2_window_filter.h"

typedef struct ngtcp2_rst ngtcp2_rst;
typedef struct ngtcp2_conn_stat ngtcp2_conn_stat;

typedef enum ngtcp2_bbr_state {
  NGTCP2_BBR_STATE_STARTUP,
  NGTCP2_BBR_STATE_DRAIN,
  NGTCP2_BBR_STATE_PROBE_BW,
  NGTCP2_BBR_STATE_PROBE_RTT,
} ngtcp2_bbr_state;

/*
 * ngtcp2_cc_bbr is BBR congestion controller, described in
 * https://tools.ietf.org/html/draft-cardwell-iccrg-bbr-congestion-control-00
 */
typedef struct ngtcp2_cc_bbr {
  ngtcp2_cc cc;

  /* The max filter used to estimate BBR.BtlBw. */
  ngtcp2_window_filter btl_bw_filter;
  uint64_t initial_cwnd;
  ngtcp2_rst *rst;
  ngtcp2_rand rand;
  ngtcp2_rand_ctx rand_ctx;

  /* BBR variables */

  /* The dynamic gain factor used to scale BBR.BtlBw to
         produce BBR.pacing_rate. */
  double pacing_gain;
  /* The dynamic gain factor used to scale the estimated BDP to produce a
     congestion window (cwnd). */
  double cwnd_gain;
  uint64_t full_bw;
  /* packet.delivered value denoting the end of a packet-timed round trip. */
  uint64_t next_round_delivered;
  /* Count of packet-timed round trips. */
  uint64_t round_count;
  uint64_t prior_cwnd;
  /* target_cwnd is the upper bound on the volume of data BBR
     allows in flight. */
  uint64_t target_cwnd;
  /* BBR's estimated bottleneck bandwidth available to the
     transport flow, estimated from the maximum delivery rate sample in a
     sliding window. */
  uint64_t btl_bw;
  /* BBR's estimated two-way round-trip propagation delay of
     the path, estimated from the windowed minimum recent round-trip delay
     sample. */
  ngtcp2_duration rt_prop;
  /* The wall clock time at which the current BBR.RTProp
     sample was obtained. */
  ngtcp2_tstamp rtprop_stamp;
  ngtcp2_tstamp cycle_stamp;
  ngtcp2_tstamp probe_rtt_done_stamp;
  /* congestion_recovery_start_ts is the time when congestion recovery
     period started.*/
  ngtcp2_tstamp congestion_recovery_start_ts;
  uint64_t congestion_recovery_next_round_delivered;
  size_t full_bw_count;
  size_t cycle_index;
  ngtcp2_bbr_state state;
  /* A boolean that records whether BBR estimates that it has ever fully
     utilized its available bandwidth ("filled the pipe"). */
  int filled_pipe;
  /* A boolean that BBR sets to true once per packet-timed round trip,
     on ACKs that advance BBR.round_count. */
  int round_start;
  int rtprop_expired;
  int idle_restart;
  int packet_conservation;
  int probe_rtt_round_done;
  /* in_loss_recovery becomes nonzero when BBR enters loss recovery
     period. */
  int in_loss_recovery;
} ngtcp2_cc_bbr;

void ngtcp2_cc_bbr_init(ngtcp2_cc_bbr *cc, ngtcp2_log *log,
                        ngtcp2_conn_stat *cstat, ngtcp2_rst *rst,
                        ngtcp2_tstamp initial_ts, ngtcp2_rand rand,
                        const ngtcp2_rand_ctx *rand_ctx);

void ngtcp2_cc_bbr_cc_congestion_event(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                       ngtcp2_tstamp sent_ts, ngtcp2_tstamp ts);

void ngtcp2_cc_bbr_cc_on_spurious_congestion(ngtcp2_cc *ccx,
                                             ngtcp2_conn_stat *cstat,
                                             ngtcp2_tstamp ts);

void ngtcp2_cc_bbr_cc_on_persistent_congestion(ngtcp2_cc *cc,
                                               ngtcp2_conn_stat *cstat,
                                               ngtcp2_tstamp ts);

void ngtcp2_cc_bbr_cc_on_ack_recv(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_ack *ack, ngtcp2_tstamp ts);

void ngtcp2_cc_bbr_cc_on_pkt_sent(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                                  const ngtcp2_cc_pkt *pkt);

void ngtcp2_cc_bbr_cc_reset(ngtcp2_cc *cc, ngtcp2_conn_stat *cstat,
                            ngtcp2_tstamp ts);

#endif /* NGTCP2_BBR_H */

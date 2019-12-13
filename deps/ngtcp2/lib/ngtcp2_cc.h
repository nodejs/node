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

#define NGTCP2_MAX_DGRAM_SIZE 1200
#define NGTCP2_MIN_CWND (2 * NGTCP2_MAX_DGRAM_SIZE)
#define NGTCP2_LOSS_REDUCTION_FACTOR_BITS 1
#define NGTCP2_PERSISTENT_CONGESTION_THRESHOLD 3

struct ngtcp2_log;
typedef struct ngtcp2_log ngtcp2_log;

struct ngtcp2_rst;
typedef struct ngtcp2_rst ngtcp2_rst;

/* ngtcp2_cc_pkt is a convenient structure to include acked/lost/sent
   packet. */
typedef struct {
  /* pkt_num is the packet number */
  int64_t pkt_num;
  /* pktlen is the length of packet. */
  size_t pktlen;
  /* ts_sent is the timestamp when packet is sent. */
  ngtcp2_tstamp ts_sent;
} ngtcp2_cc_pkt;

ngtcp2_cc_pkt *ngtcp2_cc_pkt_init(ngtcp2_cc_pkt *pkt, int64_t pkt_num,
                                  size_t pktlen, ngtcp2_tstamp ts_sent);

/* ngtcp2_default_cc is the default congestion controller. */
struct ngtcp2_default_cc {
  ngtcp2_log *log;
  ngtcp2_cc_stat *ccs;
  ngtcp2_rst *rst;
  double max_delivery_rate;
  ngtcp2_duration min_rtt;
  ngtcp2_tstamp min_rtt_ts;
  uint64_t target_cwnd;
};

typedef struct ngtcp2_default_cc ngtcp2_default_cc;

void ngtcp2_default_cc_init(ngtcp2_default_cc *cc, ngtcp2_cc_stat *ccs,
                            ngtcp2_rst *rst, ngtcp2_log *log);

void ngtcp2_default_cc_free(ngtcp2_default_cc *cc);

void ngtcp2_default_cc_on_pkt_acked(ngtcp2_default_cc *cc,
                                    const ngtcp2_cc_pkt *pkt);

void ngtcp2_default_cc_congestion_event(ngtcp2_default_cc *cc,
                                        ngtcp2_tstamp ts_sent,
                                        ngtcp2_tstamp ts);

void ngtcp2_default_cc_handle_persistent_congestion(ngtcp2_default_cc *cc,
                                                    ngtcp2_duration loss_window,
                                                    ngtcp2_duration pto);

void ngtcp2_default_cc_on_ack_recv(ngtcp2_default_cc *cc,
                                   ngtcp2_duration latest_rtt,
                                   ngtcp2_tstamp ts);

#endif /* NGTCP2_CC_H */

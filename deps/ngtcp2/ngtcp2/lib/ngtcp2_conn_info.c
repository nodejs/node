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
#include "ngtcp2_conn_info.h"
#include "ngtcp2_conn_stat.h"

void ngtcp2_conn_info_init_versioned(int conn_info_version,
                                     ngtcp2_conn_info *cinfo,
                                     const ngtcp2_conn_stat *cstat) {
  cinfo->latest_rtt = cstat->latest_rtt;
  cinfo->min_rtt = cstat->min_rtt;
  cinfo->smoothed_rtt = cstat->smoothed_rtt;
  cinfo->rttvar = cstat->rttvar;
  cinfo->cwnd = cstat->cwnd;
  cinfo->ssthresh = cstat->ssthresh;
  cinfo->bytes_in_flight = cstat->bytes_in_flight;

  switch (conn_info_version) {
  case NGTCP2_CONN_INFO_V2:
    cinfo->pkt_sent = cstat->pkt_sent;
    cinfo->bytes_sent = cstat->bytes_sent;
    cinfo->pkt_recv = cstat->pkt_recv;
    cinfo->bytes_recv = cstat->bytes_recv;
    cinfo->pkt_lost = cstat->pkt_lost;
    cinfo->bytes_lost = cstat->bytes_lost;
    cinfo->ping_recv = cstat->ping_recv;
    cinfo->pkt_discarded = cstat->pkt_discarded;
  }
}

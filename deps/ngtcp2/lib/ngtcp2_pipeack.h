/*
 * ngtcp2
 *
 * Copyright (c) 2019 ngtcp2 contributors
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
#ifndef NGTCP2_PIPEACK_H
#define NGTCP2_PIPEACK_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <ngtcp2/ngtcp2.h>

typedef struct ngtcp2_pipeack_sample {
  /*
   * value is the maximum cumulative acknowledged bytes per RTT in
   * this sample.
   */
  uint64_t value;
  /* ts is the time when this sample started. */
  ngtcp2_tstamp ts;
} ngtcp2_pipeack_sample;

#define NGTCP2_PIPEACK_NUM_SAMPLES 4

/*
 * ngtcp2_pipeack implements pipeACK measurement described in RFC
 * 7661.
 */
typedef struct ngtcp2_pipeack {
  /* value is the pre-computed maximum pipeACK value in the effective
     sample period. */
  uint64_t value;
  /* acked_bytes is the accumulated acked_bytes per RTT. */
  uint64_t acked_bytes;
  /* ts is the timestamp when the accumulation to acked_bytes
     started. */
  ngtcp2_tstamp ts;
  /* pos points to the latest sample in samples. */
  size_t pos;
  /* len is the number of effective from pos.  samples is treated as
     ring buffer and its index will wrap when pos + len goes beyond
     NGTCP2_PIPEACK_NUM_SAMPLES - 1. */
  size_t len;
  ngtcp2_pipeack_sample samples[NGTCP2_PIPEACK_NUM_SAMPLES];
} ngtcp2_pipeack;

/*
 * ngtcp2_pipeack_init initializes the object pointed by |pipeack|.
 */
void ngtcp2_pipeack_init(ngtcp2_pipeack *pipeack, ngtcp2_tstamp ts);

/*
 * ngtcp2_pipeack_update notifies the acknowledged bytes |acked_bytes|
 * to |pipeack|.
 */
void ngtcp2_pipeack_update(ngtcp2_pipeack *pipeack, uint64_t acked_bytes,
                           const ngtcp2_rcvry_stat *rcs, ngtcp2_tstamp ts);

/*
 * ngtcp2_pipeack_update_value computes maximum pipeACK value.
 * Samples which have timestamp <= |ts| - pipeACK sampling period are
 * discarded.
 */
void ngtcp2_pipeack_update_value(ngtcp2_pipeack *pipeack,
                                 const ngtcp2_rcvry_stat *rcs,
                                 ngtcp2_tstamp ts);

#endif /* NGTCP2_PIPEACK_H */

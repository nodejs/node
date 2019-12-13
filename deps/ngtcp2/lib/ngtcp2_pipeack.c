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
#include "ngtcp2_pipeack.h"

#include <string.h>

#include "ngtcp2_macro.h"
#include "ngtcp2_rcvry.h"

void ngtcp2_pipeack_init(ngtcp2_pipeack *pipeack, ngtcp2_tstamp ts) {
  ngtcp2_pipeack_sample *sample;

  memset(pipeack, 0, sizeof(*pipeack));

  /* Initialize value to UINT64_MAX so that it does not restrict CWND
     growth.  It serves as "undefined" value. */
  pipeack->value = UINT64_MAX;
  pipeack->ts = ts;
  pipeack->len = 1;

  sample = &pipeack->samples[pipeack->pos];
  sample->value = 0;
  sample->ts = ts;
}

static ngtcp2_duration get_rtt(const ngtcp2_rcvry_stat *rcs) {
  if (rcs->smoothed_rtt == 0) {
    return NGTCP2_DEFAULT_INITIAL_RTT;
  }
  return rcs->smoothed_rtt;
}

/*
 * compute_sampling_period returns pipeACK sampling period.
 */
static ngtcp2_duration compute_sampling_period(ngtcp2_duration rtt) {
  return ngtcp2_max(rtt * 3, NGTCP2_SECONDS);
}

/*
 * compute_measurement_period returns the measurement period using
 * pipeACK sampling period |speriod|.
 */
static ngtcp2_duration compute_measurement_period(ngtcp2_duration speriod) {
  return speriod / NGTCP2_PIPEACK_NUM_SAMPLES;
}

static void pipeack_update_sample(ngtcp2_pipeack *pipeack, uint64_t acked_bytes,
                                  const ngtcp2_rcvry_stat *rcs,
                                  ngtcp2_tstamp ts) {
  ngtcp2_pipeack_sample *sample;
  ngtcp2_duration rtt = get_rtt(rcs);
  ngtcp2_duration speriod = compute_sampling_period(rtt);
  ngtcp2_duration mperiod = compute_measurement_period(speriod);

  if (pipeack->len == 0) {
    pipeack->len = 1;

    sample = &pipeack->samples[pipeack->pos];
    sample->ts = ts;
    sample->value = acked_bytes;

    return;
  }

  sample = &pipeack->samples[pipeack->pos];
  if (sample->ts + mperiod <= ts) {
    ts = sample->ts + (ts - sample->ts) / mperiod * mperiod;
    pipeack->pos = (pipeack->pos - 1) & (NGTCP2_PIPEACK_NUM_SAMPLES - 1);

    if (pipeack->len < NGTCP2_PIPEACK_NUM_SAMPLES) {
      ++pipeack->len;
    }

    sample = &pipeack->samples[pipeack->pos];
    sample->ts = ts;
    sample->value = acked_bytes;

    return;
  }

  sample->value = ngtcp2_max(sample->value, acked_bytes);
}

void ngtcp2_pipeack_update(ngtcp2_pipeack *pipeack, uint64_t acked_bytes,
                           const ngtcp2_rcvry_stat *rcs, ngtcp2_tstamp ts) {
  uint64_t value;
  ngtcp2_duration d = ts - pipeack->ts;
  ngtcp2_duration rtt = get_rtt(rcs);

  pipeack->acked_bytes += acked_bytes;

  if (d < rtt) {
    return;
  }

  value = pipeack->acked_bytes * rtt / d;
  pipeack->acked_bytes = 0;
  pipeack->ts = ts;

  pipeack_update_sample(pipeack, value, rcs, ts);
}

void ngtcp2_pipeack_update_value(ngtcp2_pipeack *pipeack,
                                 const ngtcp2_rcvry_stat *rcs,
                                 ngtcp2_tstamp ts) {
  ngtcp2_pipeack_sample *sample;
  ngtcp2_duration rtt = get_rtt(rcs);
  ngtcp2_duration speriod = compute_sampling_period(rtt);
  ngtcp2_duration mperiod = compute_measurement_period(speriod);
  size_t i;
  uint64_t res = 0;

  for (i = pipeack->len; i > 0; --i) {
    sample = &pipeack->samples[(pipeack->pos + i - 1) &
                               (NGTCP2_PIPEACK_NUM_SAMPLES - 1)];
    if (sample->ts + speriod <= ts) {
      --pipeack->len;
      continue;
    }
    if (sample->ts + mperiod > ts) {
      break;
    }

    res = ngtcp2_max(res, sample->value);
  }

  if (res == 0) {
    pipeack->value = UINT64_MAX;
  } else {
    pipeack->value = res;
  }
}

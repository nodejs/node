/*
 * ngtcp2
 *
 * Copyright (c) 2025 ngtcp2 contributors
 * Copyright (c) 2023 nghttp2 contributors
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
#include "ngtcp2_ratelim.h"

#include <assert.h>

#include "ngtcp2_macro.h"

void ngtcp2_ratelim_init(ngtcp2_ratelim *rlim, uint64_t burst, uint64_t rate,
                         ngtcp2_tstamp ts) {
  *rlim = (ngtcp2_ratelim){
    .burst = burst,
    .rate = rate,
    .tokens = burst,
    .ts = ts,
  };
}

/* ratelim_update updates rlim->tokens with the current |ts|. */
static void ratelim_update(ngtcp2_ratelim *rlim, ngtcp2_tstamp ts) {
  uint64_t d, gain, gps;

  assert(ts >= rlim->ts);

  if (ts == rlim->ts) {
    return;
  }

  d = ts - rlim->ts;
  rlim->ts = ts;

  if (rlim->rate > (UINT64_MAX - rlim->carry) / d) {
    gain = UINT64_MAX;
  } else {
    gain = rlim->rate * d + rlim->carry;
  }

  gps = gain / NGTCP2_SECONDS;

  if (gps < rlim->burst && rlim->tokens < rlim->burst - gps) {
    rlim->tokens += gps;
    rlim->carry = gain % NGTCP2_SECONDS;

    return;
  }

  rlim->tokens = rlim->burst;
  rlim->carry = 0;
}

int ngtcp2_ratelim_drain(ngtcp2_ratelim *rlim, uint64_t n, ngtcp2_tstamp ts) {
  ratelim_update(rlim, ts);

  if (rlim->tokens < n) {
    return -1;
  }

  rlim->tokens -= n;

  return 0;
}

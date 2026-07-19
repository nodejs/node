/*
 * nghttp3
 *
 * Copyright (c) 2025 nghttp3 contributors
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
#include "nghttp3_ratelim.h"

#include <assert.h>

#include "nghttp3_macro.h"

void nghttp3_ratelim_init(nghttp3_ratelim *rlim, uint64_t burst, uint64_t rate,
                          nghttp3_tstamp ts) {
  *rlim = (nghttp3_ratelim){
    .burst = burst,
    .rate = rate,
    .tokens = burst,
    .ts = ts,
  };
}

/* ratelim_update updates rlim->tokens with the current |ts|. */
static void ratelim_update(nghttp3_ratelim *rlim, nghttp3_tstamp ts) {
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

  gps = gain / NGHTTP3_SECONDS;

  if (gps < rlim->burst && rlim->tokens < rlim->burst - gps) {
    rlim->tokens += gps;
    rlim->carry = gain % NGHTTP3_SECONDS;

    return;
  }

  rlim->tokens = rlim->burst;
  rlim->carry = 0;
}

int nghttp3_ratelim_drain(nghttp3_ratelim *rlim, uint64_t n,
                          nghttp3_tstamp ts) {
  ratelim_update(rlim, ts);

  if (rlim->tokens < n) {
    return -1;
  }

  rlim->tokens -= n;

  return 0;
}

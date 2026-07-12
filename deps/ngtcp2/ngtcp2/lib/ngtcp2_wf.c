/*
 * ngtcp2
 *
 * Copyright (c) 2026 ngtcp2 contributors
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
#include "ngtcp2_wf.h"

void ngtcp2_wf_init(ngtcp2_wf *wf, uint64_t win) {
  *wf = (ngtcp2_wf){
    .win = win,
  };
}

void ngtcp2_wf_update(ngtcp2_wf *wf, uint64_t value, uint64_t ts) {
  ngtcp2_wf_sample s = {
    .value = value,
    .ts = ts,
  };

  if (wf->samples[0].value <= value || ts - wf->samples[2].ts > wf->win) {
    wf->samples[0] = wf->samples[1] = wf->samples[2] = s;

    return;
  }

  if (wf->samples[1].value <= value) {
    wf->samples[1] = wf->samples[2] = s;
  } else if (wf->samples[2].value <= value) {
    wf->samples[2] = s;
  }

  if (ts - wf->samples[1].ts > wf->win) {
    wf->samples[0] = wf->samples[2];
    wf->samples[1] = wf->samples[2] = s;

    return;
  }

  if (ts - wf->samples[0].ts > wf->win) {
    wf->samples[0] = wf->samples[1];
    wf->samples[1] = wf->samples[2];
    wf->samples[2] = s;

    return;
  }

  if (wf->samples[0].value == wf->samples[1].value &&
      ts - wf->samples[0].ts > wf->win / 4) {
    wf->samples[1] = wf->samples[2] = s;

    return;
  }

  if (wf->samples[1].value == wf->samples[2].value &&
      ts - wf->samples[0].ts > wf->win / 2) {
    wf->samples[2] = s;
  }
}

uint64_t ngtcp2_wf_get_best(const ngtcp2_wf *wf) {
  return wf->samples[0].value;
}

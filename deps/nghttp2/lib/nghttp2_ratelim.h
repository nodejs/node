/*
 * nghttp2 - HTTP/2 C Library
 *
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
#ifndef NGHTTP2_RATELIM_H
#define NGHTTP2_RATELIM_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <nghttp2/nghttp2.h>

typedef struct nghttp2_ratelim {
  /* burst is the maximum value of val. */
  uint64_t burst;
  /* rate is the amount of value that is regenerated per 1 tstamp. */
  uint64_t rate;
  /* val is the amount of value available to drain. */
  uint64_t val;
  /* tstamp is the last timestamp in second resolution that is known
     to this object. */
  uint64_t tstamp;
} nghttp2_ratelim;

/* nghttp2_ratelim_init initializes |rl| with the given parameters. */
void nghttp2_ratelim_init(nghttp2_ratelim *rl, uint64_t burst, uint64_t rate);

/* nghttp2_ratelim_update updates rl->val with the current |tstamp|
   given in second resolution. */
void nghttp2_ratelim_update(nghttp2_ratelim *rl, uint64_t tstamp);

/* nghttp2_ratelim_drain drains |n| from rl->val.  It returns 0 if it
   succeeds, or -1. */
int nghttp2_ratelim_drain(nghttp2_ratelim *rl, uint64_t n);

#endif /* NGHTTP2_RATELIM_H */

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
#ifndef NGTCP2_PCG_H
#define NGTCP2_PCG_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <ngtcp2/ngtcp2.h>

typedef struct ngtcp2_pcg32 {
  uint64_t state;
} ngtcp2_pcg32;

/*
 * ngtcp2_pcg32_init initializes |pcg| with |seed|.
 */
void ngtcp2_pcg32_init(ngtcp2_pcg32 *pcg, uint64_t seed);

/*
 * ngtcp2_pcg32_rand returns a random value in [0, UINT32_MAX].
 */
uint32_t ngtcp2_pcg32_rand(ngtcp2_pcg32 *pcg);

/*
 * ngtcp2_pcg32_rand_n returns a random value in [0, n).  |n| must not
 * be zero.
 */
uint32_t ngtcp2_pcg32_rand_n(ngtcp2_pcg32 *pcg, uint32_t n);

#endif /* !defined(NGTCP2_PCG_H) */

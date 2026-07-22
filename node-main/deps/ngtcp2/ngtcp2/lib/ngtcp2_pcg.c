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
#include "ngtcp2_pcg.h"

#include <assert.h>

/*
 * PCG implementation from
 * https://github.com/imneme/pcg-c/blob/83252d9c23df9c82ecb42210afed61a7b42402d7/include/pcg_variants.h
 *
 * PCG Random Number Generation for C.
 *
 * Copyright 2014-2019 Melissa O'Neill <oneill@pcg-random.org>,
 *                     and the PCG Project contributors.
 *
 * SPDX-License-Identifier: (Apache-2.0 OR MIT)
 *
 * Licensed under the Apache License, Version 2.0 (provided in
 * LICENSE-APACHE.txt and at http://www.apache.org/licenses/LICENSE-2.0)
 * or under the MIT license (provided in LICENSE-MIT.txt and at
 * http://opensource.org/licenses/MIT), at your option. This file may not
 * be copied, modified, or distributed except according to those terms.
 *
 * Distributed on an "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied.  See your chosen license for details.
 *
 * For additional information about the PCG random number generation scheme,
 * visit http://www.pcg-random.org/.
 */

#define NGTCP2_PCG_DEFAULT_MULTIPLIER_64 6364136223846793005ULL
#define NGTCP2_PCG_DEFAULT_INCREMENT_64 1442695040888963407ULL

static void pcg_oneseq_64_step_r(ngtcp2_pcg32 *pcg) {
  pcg->state = pcg->state * NGTCP2_PCG_DEFAULT_MULTIPLIER_64 +
               NGTCP2_PCG_DEFAULT_INCREMENT_64;
}

void ngtcp2_pcg32_init(ngtcp2_pcg32 *pcg, uint64_t seed) {
  pcg->state = 0;
  pcg_oneseq_64_step_r(pcg);
  pcg->state += seed;
  pcg_oneseq_64_step_r(pcg);
}

static uint32_t pcg_rotr_32(uint32_t value, unsigned int rot) {
  return (value >> rot) | (value << ((-rot) & 31));
}

static uint32_t pcg_output_xsh_rr_64_32(uint64_t state) {
  return pcg_rotr_32((uint32_t)(((state >> 18u) ^ state) >> 27u),
                     (unsigned int)(state >> 59u));
}

uint32_t ngtcp2_pcg32_rand(ngtcp2_pcg32 *pcg) {
  uint64_t oldstate = pcg->state;

  pcg_oneseq_64_step_r(pcg);

  return pcg_output_xsh_rr_64_32(oldstate);
}

uint32_t ngtcp2_pcg32_rand_n(ngtcp2_pcg32 *pcg, uint32_t n) {
  assert(n);
  return (uint32_t)(((uint64_t)ngtcp2_pcg32_rand(pcg) * n) >> 32);
}

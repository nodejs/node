/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Functions to estimate the bit cost of Huffman trees. */

#ifndef BROTLI_ENC_BIT_COST_H_
#define BROTLI_ENC_BIT_COST_H_

#include "../common/platform.h"
#include "histogram.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

BROTLI_INTERNAL double BrotliBitsEntropy(
    const uint32_t* population, size_t size);
BROTLI_INTERNAL double BrotliPopulationCostLiteral(
    const HistogramLiteral* histogram);
BROTLI_INTERNAL double BrotliPopulationCostCommand(
    const HistogramCommand* histogram);
BROTLI_INTERNAL double BrotliPopulationCostDistance(
    const HistogramDistance* histogram);

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_BIT_COST_H_ */

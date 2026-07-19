/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Functions to estimate the bit cost of Huffman trees. */

#include "bit_cost.h"

#include "../common/platform.h"
#include "fast_log.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

double BrotliBitsEntropy(const uint32_t* population, size_t size) {
  size_t sum = 0;
  double retval = 0;
  const uint32_t* population_end = population + size;
  size_t p;
  if (size & 1) {
    goto odd_number_of_elements_left;
  }
  while (population < population_end) {
    p = *population++;
    sum += p;
    retval -= (double)p * FastLog2(p);
 odd_number_of_elements_left:
    p = *population++;
    sum += p;
    retval -= (double)p * FastLog2(p);
  }
  if (sum) retval += (double)sum * FastLog2(sum);

  if (retval < (double)sum) {
    /* TODO(eustas): consider doing that per-symbol? */
    /* At least one bit per literal is needed. */
    retval = (double)sum;
  }

  return retval;
}

#define FN(X) X ## Literal
#include "bit_cost_inc.h"  /* NOLINT(build/include) */
#undef FN

#define FN(X) X ## Command
#include "bit_cost_inc.h"  /* NOLINT(build/include) */
#undef FN

#define FN(X) X ## Distance
#include "bit_cost_inc.h"  /* NOLINT(build/include) */
#undef FN

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

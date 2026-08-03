/* NOLINT(build/header_guard) */
/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* template parameters: FN */

#define HistogramType FN(Histogram)

double FN(BrotliPopulationCost)(const HistogramType* histogram) {
  static const double kOneSymbolHistogramCost = 12;
  static const double kTwoSymbolHistogramCost = 20;
  static const double kThreeSymbolHistogramCost = 28;
  static const double kFourSymbolHistogramCost = 37;
  const size_t data_size = FN(HistogramDataSize)();
  int count = 0;
  size_t s[5];
  double bits = 0.0;
  size_t i;
  if (histogram->total_count_ == 0) {
    return kOneSymbolHistogramCost;
  }
  for (i = 0; i < data_size; ++i) {
    if (histogram->data_[i] > 0) {
      s[count] = i;
      ++count;
      if (count > 4) break;
    }
  }
  if (count == 1) {
    return kOneSymbolHistogramCost;
  }
  if (count == 2) {
    return (kTwoSymbolHistogramCost + (double)histogram->total_count_);
  }
  if (count == 3) {
    const uint32_t histo0 = histogram->data_[s[0]];
    const uint32_t histo1 = histogram->data_[s[1]];
    const uint32_t histo2 = histogram->data_[s[2]];
    const uint32_t histomax =
        BROTLI_MAX(uint32_t, histo0, BROTLI_MAX(uint32_t, histo1, histo2));
    return (kThreeSymbolHistogramCost +
            2 * (histo0 + histo1 + histo2) - histomax);
  }
  if (count == 4) {
    uint32_t histo[4];
    uint32_t h23;
    uint32_t histomax;
    for (i = 0; i < 4; ++i) {
      histo[i] = histogram->data_[s[i]];
    }
    /* Sort */
    for (i = 0; i < 4; ++i) {
      size_t j;
      for (j = i + 1; j < 4; ++j) {
        if (histo[j] > histo[i]) {
          BROTLI_SWAP(uint32_t, histo, j, i);
        }
      }
    }
    h23 = histo[2] + histo[3];
    histomax = BROTLI_MAX(uint32_t, h23, histo[0]);
    return (kFourSymbolHistogramCost +
            3 * h23 + 2 * (histo[0] + histo[1]) - histomax);
  }

  {
    /* In this loop we compute the entropy of the histogram and simultaneously
       build a simplified histogram of the code length codes where we use the
       zero repeat code 17, but we don't use the non-zero repeat code 16. */
    size_t max_depth = 1;
    uint32_t depth_histo[BROTLI_CODE_LENGTH_CODES] = { 0 };
    const double log2total = FastLog2(histogram->total_count_);
    for (i = 0; i < data_size;) {
      if (histogram->data_[i] > 0) {
        /* Compute -log2(P(symbol)) = -log2(count(symbol)/total_count) =
                                    = log2(total_count) - log2(count(symbol)) */
        double log2p = log2total - FastLog2(histogram->data_[i]);
        /* Approximate the bit depth by round(-log2(P(symbol))) */
        size_t depth = (size_t)(log2p + 0.5);
        bits += histogram->data_[i] * log2p;
        if (depth > 15) {
          depth = 15;
        }
        if (depth > max_depth) {
          max_depth = depth;
        }
        ++depth_histo[depth];
        ++i;
      } else {
        /* Compute the run length of zeros and add the appropriate number of 0
           and 17 code length codes to the code length code histogram. */
        uint32_t reps = 1;
        size_t k;
        for (k = i + 1; k < data_size && histogram->data_[k] == 0; ++k) {
          ++reps;
        }
        i += reps;
        if (i == data_size) {
          /* Don't add any cost for the last zero run, since these are encoded
             only implicitly. */
          break;
        }
        if (reps < 3) {
          depth_histo[0] += reps;
        } else {
          reps -= 2;
          while (reps > 0) {
            ++depth_histo[BROTLI_REPEAT_ZERO_CODE_LENGTH];
            /* Add the 3 extra bits for the 17 code length code. */
            bits += 3;
            reps >>= 3;
          }
        }
      }
    }
    /* Add the estimated encoding cost of the code length code histogram. */
    bits += (double)(18 + 2 * max_depth);
    /* Add the entropy of the code length code histogram. */
    bits += BitsEntropy(depth_histo, BROTLI_CODE_LENGTH_CODES);
  }
  return bits;
}

#undef HistogramType

/* NOLINT(build/header_guard) */
/* Copyright 2014 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* template parameters: FN */

#define HistogramType FN(Histogram)

/* Creates entropy codes for all block types and stores them to the bit
   stream. */
static void FN(BuildAndStoreEntropyCodes)(MemoryManager* m, BlockEncoder* self,
    const HistogramType* histograms, const size_t histograms_size,
    const size_t alphabet_size, HuffmanTree* tree,
    size_t* storage_ix, uint8_t* storage) {
  const size_t table_size = histograms_size * self->histogram_length_;
  self->depths_ = BROTLI_ALLOC(m, uint8_t, table_size);
  self->bits_ = BROTLI_ALLOC(m, uint16_t, table_size);
  if (BROTLI_IS_OOM(m)) return;

  {
    size_t i;
    for (i = 0; i < histograms_size; ++i) {
      size_t ix = i * self->histogram_length_;
      BuildAndStoreHuffmanTree(&histograms[i].data_[0], self->histogram_length_,
          alphabet_size, tree, &self->depths_[ix], &self->bits_[ix],
          storage_ix, storage);
    }
  }
}

#undef HistogramType

/* NOLINT(build/header_guard) */
/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* template parameters: Histogram, DATA_SIZE, DataType */

/* A simple container for histograms of data in blocks. */

typedef struct FN(Histogram) {
  uint32_t data_[DATA_SIZE];
  size_t total_count_;
  double bit_cost_;
} FN(Histogram);

static BROTLI_INLINE void FN(HistogramClear)(FN(Histogram)* self) {
  memset(self->data_, 0, sizeof(self->data_));
  self->total_count_ = 0;
  self->bit_cost_ = HUGE_VAL;
}

static BROTLI_INLINE void FN(ClearHistograms)(
    FN(Histogram)* array, size_t length) {
  size_t i;
  for (i = 0; i < length; ++i) FN(HistogramClear)(array + i);
}

static BROTLI_INLINE void FN(HistogramAdd)(FN(Histogram)* self, size_t val) {
  ++self->data_[val];
  ++self->total_count_;
}

static BROTLI_INLINE void FN(HistogramAddVector)(FN(Histogram)* self,
    const DataType* p, size_t n) {
  self->total_count_ += n;
  n += 1;
  while (--n) ++self->data_[*p++];
}

static BROTLI_INLINE void FN(HistogramAddHistogram)(FN(Histogram)* self,
    const FN(Histogram)* v) {
  size_t i;
  self->total_count_ += v->total_count_;
  for (i = 0; i < DATA_SIZE; ++i) {
    self->data_[i] += v->data_[i];
  }
}

static BROTLI_INLINE size_t FN(HistogramDataSize)(void) { return DATA_SIZE; }

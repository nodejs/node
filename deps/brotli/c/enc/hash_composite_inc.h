/* NOLINT(build/header_guard) */
/* Copyright 2018 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* template parameters: FN, HASHER_A, HASHER_B */

/* Composite hasher: This hasher allows to combine two other hashers, HASHER_A
   and HASHER_B. */

#define HashComposite HASHER()

#define FN_A(X) EXPAND_CAT(X, HASHER_A)
#define FN_B(X) EXPAND_CAT(X, HASHER_B)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) {
  size_t a =  FN_A(HashTypeLength)();
  size_t b =  FN_B(HashTypeLength)();
  return a > b ? a : b;
}

static BROTLI_INLINE size_t FN(StoreLookahead)(void) {
  size_t a =  FN_A(StoreLookahead)();
  size_t b =  FN_B(StoreLookahead)();
  return a > b ? a : b;
}

typedef struct HashComposite {
  HasherHandle ha;
  HasherHandle hb;
  const BrotliEncoderParams* params;
} HashComposite;

static BROTLI_INLINE HashComposite* FN(Self)(HasherHandle handle) {
  return (HashComposite*)&(GetHasherCommon(handle)[1]);
}

static void FN(Initialize)(
    HasherHandle handle, const BrotliEncoderParams* params) {
  HashComposite* self = FN(Self)(handle);
  self->ha = 0;
  self->hb = 0;
  self->params = params;
  /* TODO: Initialize of the hashers is defered to Prepare (and params
     remembered here) because we don't get the one_shot and input_size params
     here that are needed to know the memory size of them. Instead provide
     those params to all hashers FN(Initialize) */
}

static void FN(Prepare)(HasherHandle handle, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* data) {
  HashComposite* self = FN(Self)(handle);
  if (!self->ha) {
    HasherCommon* common_a;
    HasherCommon* common_b;

    self->ha = handle + sizeof(HasherCommon) + sizeof(HashComposite);
    common_a = (HasherCommon*)self->ha;
    common_a->params = self->params->hasher;
    common_a->is_prepared_ = BROTLI_FALSE;
    common_a->dict_num_lookups = 0;
    common_a->dict_num_matches = 0;
    FN_A(Initialize)(self->ha, self->params);

    self->hb = self->ha + sizeof(HasherCommon) + FN_A(HashMemAllocInBytes)(
        self->params, one_shot, input_size);
    common_b = (HasherCommon*)self->hb;
    common_b->params = self->params->hasher;
    common_b->is_prepared_ = BROTLI_FALSE;
    common_b->dict_num_lookups = 0;
    common_b->dict_num_matches = 0;
    FN_B(Initialize)(self->hb, self->params);
  }
  FN_A(Prepare)(self->ha, one_shot, input_size, data);
  FN_B(Prepare)(self->hb, one_shot, input_size, data);
}

static BROTLI_INLINE size_t FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size) {
  return sizeof(HashComposite) + 2 * sizeof(HasherCommon) +
      FN_A(HashMemAllocInBytes)(params, one_shot, input_size) +
      FN_B(HashMemAllocInBytes)(params, one_shot, input_size);
}

static BROTLI_INLINE void FN(Store)(HasherHandle BROTLI_RESTRICT handle,
    const uint8_t* BROTLI_RESTRICT data, const size_t mask, const size_t ix) {
  HashComposite* self = FN(Self)(handle);
  FN_A(Store)(self->ha, data, mask, ix);
  FN_B(Store)(self->hb, data, mask, ix);
}

static BROTLI_INLINE void FN(StoreRange)(HasherHandle handle,
    const uint8_t* data, const size_t mask, const size_t ix_start,
    const size_t ix_end) {
  HashComposite* self = FN(Self)(handle);
  FN_A(StoreRange)(self->ha, data, mask, ix_start, ix_end);
  FN_B(StoreRange)(self->hb, data, mask, ix_start, ix_end);
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(HasherHandle handle,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ring_buffer_mask) {
  HashComposite* self = FN(Self)(handle);
  FN_A(StitchToPreviousBlock)(self->ha, num_bytes, position, ringbuffer,
      ring_buffer_mask);
  FN_B(StitchToPreviousBlock)(self->hb, num_bytes, position, ringbuffer,
      ring_buffer_mask);
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HasherHandle handle, int* BROTLI_RESTRICT distance_cache) {
  HashComposite* self = FN(Self)(handle);
  FN_A(PrepareDistanceCache)(self->ha, distance_cache);
  FN_B(PrepareDistanceCache)(self->hb, distance_cache);
}

static BROTLI_INLINE void FN(FindLongestMatch)(HasherHandle handle,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward,
    const size_t gap, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  HashComposite* self = FN(Self)(handle);
  FN_A(FindLongestMatch)(self->ha, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, gap,
      max_distance, out);
  FN_B(FindLongestMatch)(self->hb, dictionary, data, ring_buffer_mask,
      distance_cache, cur_ix, max_length, max_backward, gap,
      max_distance, out);
}

#undef HashComposite

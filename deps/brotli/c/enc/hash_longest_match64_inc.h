/* NOLINT(build/header_guard) */
/* Copyright 2010 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* template parameters: FN */

/* A (forgetful) hash table to the data seen by the compressor, to
   help create backward references to previous data.

   This is a hash map of fixed size (bucket_size_) to a ring buffer of
   fixed size (block_size_). The ring buffer contains the last block_size_
   index positions of the given hash key in the compressed data. */

#define HashLongestMatch HASHER()

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

/* HashBytes is the function that chooses the bucket to place the address in. */
static BROTLI_INLINE uint32_t FN(HashBytes)(const uint8_t* data,
                                            const uint64_t mask,
                                            const int shift) {
  const uint64_t h = (BROTLI_UNALIGNED_LOAD64LE(data) & mask) * kHashMul64Long;
  /* The higher bits contain more mixture from the multiplication,
     so we take our results from there. */
  return (uint32_t)(h >> shift);
}

typedef struct HashLongestMatch {
  /* Number of hash buckets. */
  size_t bucket_size_;
  /* Only block_size_ newest backward references are kept,
     and the older are forgotten. */
  size_t block_size_;
  /* Left-shift for computing hash bucket index from hash value. */
  int hash_shift_;
  /* Mask for selecting the next 4-8 bytes of input */
  uint64_t hash_mask_;
  /* Mask for accessing entries in a block (in a ring-buffer manner). */
  uint32_t block_mask_;

  /* --- Dynamic size members --- */

  /* Number of entries in a particular bucket. */
  /* uint16_t num[bucket_size]; */

  /* Buckets containing block_size_ of backward references. */
  /* uint32_t* buckets[bucket_size * block_size]; */
} HashLongestMatch;

static BROTLI_INLINE HashLongestMatch* FN(Self)(HasherHandle handle) {
  return (HashLongestMatch*)&(GetHasherCommon(handle)[1]);
}

static BROTLI_INLINE uint16_t* FN(Num)(HashLongestMatch* self) {
  return (uint16_t*)(&self[1]);
}

static BROTLI_INLINE uint32_t* FN(Buckets)(HashLongestMatch* self) {
  return (uint32_t*)(&FN(Num)(self)[self->bucket_size_]);
}

static void FN(Initialize)(
    HasherHandle handle, const BrotliEncoderParams* params) {
  HasherCommon* common = GetHasherCommon(handle);
  HashLongestMatch* self = FN(Self)(handle);
  BROTLI_UNUSED(params);
  self->hash_shift_ = 64 - common->params.bucket_bits;
  self->hash_mask_ = (~((uint64_t)0U)) >> (64 - 8 * common->params.hash_len);
  self->bucket_size_ = (size_t)1 << common->params.bucket_bits;
  self->block_size_ = (size_t)1 << common->params.block_bits;
  self->block_mask_ = (uint32_t)(self->block_size_ - 1);
}

static void FN(Prepare)(HasherHandle handle, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* data) {
  HashLongestMatch* self = FN(Self)(handle);
  uint16_t* num = FN(Num)(self);
  /* Partial preparation is 100 times slower (per socket). */
  size_t partial_prepare_threshold = self->bucket_size_ >> 6;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t key = FN(HashBytes)(&data[i], self->hash_mask_,
                                         self->hash_shift_);
      num[key] = 0;
    }
  } else {
    memset(num, 0, self->bucket_size_ * sizeof(num[0]));
  }
}

static BROTLI_INLINE size_t FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size) {
  size_t bucket_size = (size_t)1 << params->hasher.bucket_bits;
  size_t block_size = (size_t)1 << params->hasher.block_bits;
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  return sizeof(HashLongestMatch) + bucket_size * (2 + 4 * block_size);
}

/* Look at 4 bytes at &data[ix & mask].
   Compute a hash from these, and store the value of ix at that position. */
static BROTLI_INLINE void FN(Store)(HasherHandle handle, const uint8_t* data,
    const size_t mask, const size_t ix) {
  HashLongestMatch* self = FN(Self)(handle);
  uint16_t* num = FN(Num)(self);
  const uint32_t key = FN(HashBytes)(&data[ix & mask], self->hash_mask_,
                                     self->hash_shift_);
  const size_t minor_ix = num[key] & self->block_mask_;
  const size_t offset =
      minor_ix + (key << GetHasherCommon(handle)->params.block_bits);
  FN(Buckets)(self)[offset] = (uint32_t)ix;
  ++num[key];
}

static BROTLI_INLINE void FN(StoreRange)(HasherHandle handle,
    const uint8_t* data, const size_t mask, const size_t ix_start,
    const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(handle, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(HasherHandle handle,
    size_t num_bytes, size_t position, const uint8_t* ringbuffer,
    size_t ringbuffer_mask) {
  if (num_bytes >= FN(HashTypeLength)() - 1 && position >= 3) {
    /* Prepare the hashes for three last bytes of the last write.
       These could not be calculated before, since they require knowledge
       of both the previous and the current block. */
    FN(Store)(handle, ringbuffer, ringbuffer_mask, position - 3);
    FN(Store)(handle, ringbuffer, ringbuffer_mask, position - 2);
    FN(Store)(handle, ringbuffer, ringbuffer_mask, position - 1);
  }
}

static BROTLI_INLINE void FN(PrepareDistanceCache)(
    HasherHandle handle, int* BROTLI_RESTRICT distance_cache) {
  PrepareDistanceCache(distance_cache,
      GetHasherCommon(handle)->params.num_last_distances_to_check);
}

/* Find a longest backward match of &data[cur_ix] up to the length of
   max_length and stores the position cur_ix in the hash table.

   REQUIRES: FN(PrepareDistanceCache) must be invoked for current distance cache
             values; if this method is invoked repeatedly with the same distance
             cache values, it is enough to invoke FN(PrepareDistanceCache) once.

   Does not look for matches longer than max_length.
   Does not look for matches further away than max_backward.
   Writes the best match into |out|.
   |out|->score is updated only if a better match is found. */
static BROTLI_INLINE void FN(FindLongestMatch)(HasherHandle handle,
    const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data, const size_t ring_buffer_mask,
    const int* BROTLI_RESTRICT distance_cache, const size_t cur_ix,
    const size_t max_length, const size_t max_backward, const size_t gap,
    const size_t max_distance, HasherSearchResult* BROTLI_RESTRICT out) {
  HasherCommon* common = GetHasherCommon(handle);
  HashLongestMatch* self = FN(Self)(handle);
  uint16_t* num = FN(Num)(self);
  uint32_t* buckets = FN(Buckets)(self);
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  /* Don't accept a short copy from far away. */
  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = out->len;
  size_t i;
  out->len = 0;
  out->len_code_delta = 0;
  /* Try last distance first. */
  for (i = 0; i < (size_t)common->params.num_last_distances_to_check; ++i) {
    const size_t backward = (size_t)distance_cache[i];
    size_t prev_ix = (size_t)(cur_ix - backward);
    if (prev_ix >= cur_ix) {
      continue;
    }
    if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
      continue;
    }
    prev_ix &= ring_buffer_mask;

    if (cur_ix_masked + best_len > ring_buffer_mask ||
        prev_ix + best_len > ring_buffer_mask ||
        data[cur_ix_masked + best_len] != data[prev_ix + best_len]) {
      continue;
    }
    {
      const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                  &data[cur_ix_masked],
                                                  max_length);
      if (len >= 3 || (len == 2 && i < 2)) {
        /* Comparing for >= 2 does not change the semantics, but just saves for
           a few unnecessary binary logarithms in backward reference score,
           since we are not interested in such short matches. */
        score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          if (i != 0) score -= BackwardReferencePenaltyUsingLastDistance(i);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
  }
  {
    const uint32_t key = FN(HashBytes)(
        &data[cur_ix_masked], self->hash_mask_, self->hash_shift_);
    uint32_t* BROTLI_RESTRICT bucket =
        &buckets[key << common->params.block_bits];
    const size_t down =
        (num[key] > self->block_size_) ?
        (num[key] - self->block_size_) : 0u;
    for (i = num[key]; i > down;) {
      size_t prev_ix = bucket[--i & self->block_mask_];
      const size_t backward = cur_ix - prev_ix;
      if (BROTLI_PREDICT_FALSE(backward > max_backward)) {
        break;
      }
      prev_ix &= ring_buffer_mask;
      if (cur_ix_masked + best_len > ring_buffer_mask ||
          prev_ix + best_len > ring_buffer_mask ||
          data[cur_ix_masked + best_len] != data[prev_ix + best_len]) {
        continue;
      }
      {
        const size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                                    &data[cur_ix_masked],
                                                    max_length);
        if (len >= 4) {
          /* Comparing for >= 3 does not change the semantics, but just saves
             for a few unnecessary binary logarithms in backward reference
             score, since we are not interested in such short matches. */
          score_t score = BackwardReferenceScore(len, backward);
          if (best_score < score) {
            best_score = score;
            best_len = len;
            out->len = best_len;
            out->distance = backward;
            out->score = best_score;
          }
        }
      }
    }
    bucket[num[key] & self->block_mask_] = (uint32_t)cur_ix;
    ++num[key];
  }
  if (min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        handle, &data[cur_ix_masked], max_length, max_backward + gap,
        max_distance, out, BROTLI_FALSE);
  }
}

#undef HashLongestMatch

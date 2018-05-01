/* NOLINT(build/header_guard) */
/* Copyright 2010 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* template parameters: FN, BUCKET_BITS, BUCKET_SWEEP, HASH_LEN,
                        USE_DICTIONARY
 */

#define HashLongestMatchQuickly HASHER()

#define BUCKET_SIZE (1 << BUCKET_BITS)

#define HASH_MAP_SIZE (4 << BUCKET_BITS)

static BROTLI_INLINE size_t FN(HashTypeLength)(void) { return 8; }
static BROTLI_INLINE size_t FN(StoreLookahead)(void) { return 8; }

/* HashBytes is the function that chooses the bucket to place
   the address in. The HashLongestMatch and HashLongestMatchQuickly
   classes have separate, different implementations of hashing. */
static uint32_t FN(HashBytes)(const uint8_t* data) {
  const uint64_t h = ((BROTLI_UNALIGNED_LOAD64LE(data) << (64 - 8 * HASH_LEN)) *
                      kHashMul64);
  /* The higher bits contain more mixture from the multiplication,
     so we take our results from there. */
  return (uint32_t)(h >> (64 - BUCKET_BITS));
}

/* A (forgetful) hash table to the data seen by the compressor, to
   help create backward references to previous data.

   This is a hash map of fixed size (BUCKET_SIZE). Starting from the
   given index, BUCKET_SWEEP buckets are used to store values of a key. */
typedef struct HashLongestMatchQuickly {
  uint32_t buckets_[BUCKET_SIZE + BUCKET_SWEEP];
} HashLongestMatchQuickly;

static BROTLI_INLINE HashLongestMatchQuickly* FN(Self)(HasherHandle handle) {
  return (HashLongestMatchQuickly*)&(GetHasherCommon(handle)[1]);
}

static void FN(Initialize)(
    HasherHandle handle, const BrotliEncoderParams* params) {
  BROTLI_UNUSED(handle);
  BROTLI_UNUSED(params);
}

static void FN(Prepare)(HasherHandle handle, BROTLI_BOOL one_shot,
    size_t input_size, const uint8_t* data) {
  HashLongestMatchQuickly* self = FN(Self)(handle);
  /* Partial preparation is 100 times slower (per socket). */
  size_t partial_prepare_threshold = HASH_MAP_SIZE >> 7;
  if (one_shot && input_size <= partial_prepare_threshold) {
    size_t i;
    for (i = 0; i < input_size; ++i) {
      const uint32_t key = FN(HashBytes)(&data[i]);
      memset(&self->buckets_[key], 0, BUCKET_SWEEP * sizeof(self->buckets_[0]));
    }
  } else {
    /* It is not strictly necessary to fill this buffer here, but
       not filling will make the results of the compression stochastic
       (but correct). This is because random data would cause the
       system to find accidentally good backward references here and there. */
    memset(&self->buckets_[0], 0, sizeof(self->buckets_));
  }
}

static BROTLI_INLINE size_t FN(HashMemAllocInBytes)(
    const BrotliEncoderParams* params, BROTLI_BOOL one_shot,
    size_t input_size) {
  BROTLI_UNUSED(params);
  BROTLI_UNUSED(one_shot);
  BROTLI_UNUSED(input_size);
  return sizeof(HashLongestMatchQuickly);
}

/* Look at 5 bytes at &data[ix & mask].
   Compute a hash from these, and store the value somewhere within
   [ix .. ix+3]. */
static BROTLI_INLINE void FN(Store)(HasherHandle handle,
    const uint8_t* data, const size_t mask, const size_t ix) {
  const uint32_t key = FN(HashBytes)(&data[ix & mask]);
  /* Wiggle the value with the bucket sweep range. */
  const uint32_t off = (ix >> 3) % BUCKET_SWEEP;
  FN(Self)(handle)->buckets_[key + off] = (uint32_t)ix;
}

static BROTLI_INLINE void FN(StoreRange)(HasherHandle handle,
    const uint8_t* data, const size_t mask, const size_t ix_start,
    const size_t ix_end) {
  size_t i;
  for (i = ix_start; i < ix_end; ++i) {
    FN(Store)(handle, data, mask, i);
  }
}

static BROTLI_INLINE void FN(StitchToPreviousBlock)(
    HasherHandle handle, size_t num_bytes, size_t position,
    const uint8_t* ringbuffer, size_t ringbuffer_mask) {
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
  BROTLI_UNUSED(handle);
  BROTLI_UNUSED(distance_cache);
}

/* Find a longest backward match of &data[cur_ix & ring_buffer_mask]
   up to the length of max_length and stores the position cur_ix in the
   hash table.

   Does not look for matches longer than max_length.
   Does not look for matches further away than max_backward.
   Writes the best match into |out|.
   |out|->score is updated only if a better match is found. */
static BROTLI_INLINE void FN(FindLongestMatch)(
    HasherHandle handle, const BrotliEncoderDictionary* dictionary,
    const uint8_t* BROTLI_RESTRICT data,
    const size_t ring_buffer_mask, const int* BROTLI_RESTRICT distance_cache,
    const size_t cur_ix, const size_t max_length, const size_t max_backward,
    const size_t gap, const size_t max_distance,
    HasherSearchResult* BROTLI_RESTRICT out) {
  HashLongestMatchQuickly* self = FN(Self)(handle);
  const size_t best_len_in = out->len;
  const size_t cur_ix_masked = cur_ix & ring_buffer_mask;
  const uint32_t key = FN(HashBytes)(&data[cur_ix_masked]);
  int compare_char = data[cur_ix_masked + best_len_in];
  score_t min_score = out->score;
  score_t best_score = out->score;
  size_t best_len = best_len_in;
  size_t cached_backward = (size_t)distance_cache[0];
  size_t prev_ix = cur_ix - cached_backward;
  out->len_code_delta = 0;
  if (prev_ix < cur_ix) {
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char == data[prev_ix + best_len]) {
      size_t len = FindMatchLengthWithLimit(&data[prev_ix],
                                            &data[cur_ix_masked],
                                            max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScoreUsingLastDistance(len);
        if (best_score < score) {
          best_score = score;
          best_len = len;
          out->len = len;
          out->distance = cached_backward;
          out->score = best_score;
          compare_char = data[cur_ix_masked + best_len];
          if (BUCKET_SWEEP == 1) {
            self->buckets_[key] = (uint32_t)cur_ix;
            return;
          }
        }
      }
    }
  }
  if (BUCKET_SWEEP == 1) {
    size_t backward;
    size_t len;
    /* Only one to look for, don't bother to prepare for a loop. */
    prev_ix = self->buckets_[key];
    self->buckets_[key] = (uint32_t)cur_ix;
    backward = cur_ix - prev_ix;
    prev_ix &= (uint32_t)ring_buffer_mask;
    if (compare_char != data[prev_ix + best_len_in]) {
      return;
    }
    if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
      return;
    }
    len = FindMatchLengthWithLimit(&data[prev_ix],
                                   &data[cur_ix_masked],
                                   max_length);
    if (len >= 4) {
      const score_t score = BackwardReferenceScore(len, backward);
      if (best_score < score) {
        out->len = len;
        out->distance = backward;
        out->score = score;
        return;
      }
    }
  } else {
    uint32_t* bucket = self->buckets_ + key;
    int i;
    prev_ix = *bucket++;
    for (i = 0; i < BUCKET_SWEEP; ++i, prev_ix = *bucket++) {
      const size_t backward = cur_ix - prev_ix;
      size_t len;
      prev_ix &= (uint32_t)ring_buffer_mask;
      if (compare_char != data[prev_ix + best_len]) {
        continue;
      }
      if (BROTLI_PREDICT_FALSE(backward == 0 || backward > max_backward)) {
        continue;
      }
      len = FindMatchLengthWithLimit(&data[prev_ix],
                                     &data[cur_ix_masked],
                                     max_length);
      if (len >= 4) {
        const score_t score = BackwardReferenceScore(len, backward);
        if (best_score < score) {
          best_score = score;
          best_len = len;
          out->len = best_len;
          out->distance = backward;
          out->score = score;
          compare_char = data[cur_ix_masked + best_len];
        }
      }
    }
  }
  if (USE_DICTIONARY && min_score == out->score) {
    SearchInStaticDictionary(dictionary,
        handle, &data[cur_ix_masked], max_length, max_backward + gap,
        max_distance, out, BROTLI_TRUE);
  }
  self->buckets_[key + ((cur_ix >> 3) % BUCKET_SWEEP)] = (uint32_t)cur_ix;
}

#undef HASH_MAP_SIZE
#undef BUCKET_SIZE

#undef HashLongestMatchQuickly

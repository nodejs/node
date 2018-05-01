/* Copyright 2010 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* A (forgetful) hash table to the data seen by the compressor, to
   help create backward references to previous data. */

#ifndef BROTLI_ENC_HASH_H_
#define BROTLI_ENC_HASH_H_

#include <string.h>  /* memcmp, memset */

#include "../common/constants.h"
#include "../common/dictionary.h"
#include "../common/platform.h"
#include <brotli/types.h>
#include "./encoder_dict.h"
#include "./fast_log.h"
#include "./find_match_length.h"
#include "./memory.h"
#include "./quality.h"
#include "./static_dict.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Pointer to hasher data.
 *
 * Excluding initialization and destruction, hasher can be passed as
 * HasherHandle by value.
 *
 * Typically hasher data consists of 3 sections:
 * * HasherCommon structure
 * * private structured hasher data, depending on hasher type
 * * private dynamic hasher data, depending on hasher type and parameters
 */
typedef uint8_t* HasherHandle;

typedef struct {
  BrotliHasherParams params;

  /* False if hasher needs to be "prepared" before use. */
  BROTLI_BOOL is_prepared_;

  size_t dict_num_lookups;
  size_t dict_num_matches;
} HasherCommon;

static BROTLI_INLINE HasherCommon* GetHasherCommon(HasherHandle handle) {
  return (HasherCommon*)handle;
}

#define score_t size_t

static const uint32_t kCutoffTransformsCount = 10;
/*   0,  12,   27,    23,    42,    63,    56,    48,    59,    64 */
/* 0+0, 4+8, 8+19, 12+11, 16+26, 20+43, 24+32, 28+20, 32+27, 36+28 */
static const uint64_t kCutoffTransforms =
    BROTLI_MAKE_UINT64_T(0x071B520A, 0xDA2D3200);

typedef struct HasherSearchResult {
  size_t len;
  size_t distance;
  score_t score;
  int len_code_delta; /* == len_code - len */
} HasherSearchResult;

/* kHashMul32 multiplier has these properties:
   * The multiplier must be odd. Otherwise we may lose the highest bit.
   * No long streaks of ones or zeros.
   * There is no effort to ensure that it is a prime, the oddity is enough
     for this use.
   * The number has been tuned heuristically against compression benchmarks. */
static const uint32_t kHashMul32 = 0x1E35A7BD;
static const uint64_t kHashMul64 = BROTLI_MAKE_UINT64_T(0x1E35A7BD, 0x1E35A7BD);
static const uint64_t kHashMul64Long =
    BROTLI_MAKE_UINT64_T(0x1FE35A7Bu, 0xD3579BD3u);

static BROTLI_INLINE uint32_t Hash14(const uint8_t* data) {
  uint32_t h = BROTLI_UNALIGNED_LOAD32LE(data) * kHashMul32;
  /* The higher bits contain more mixture from the multiplication,
     so we take our results from there. */
  return h >> (32 - 14);
}

static BROTLI_INLINE void PrepareDistanceCache(
    int* BROTLI_RESTRICT distance_cache, const int num_distances) {
  if (num_distances > 4) {
    int last_distance = distance_cache[0];
    distance_cache[4] = last_distance - 1;
    distance_cache[5] = last_distance + 1;
    distance_cache[6] = last_distance - 2;
    distance_cache[7] = last_distance + 2;
    distance_cache[8] = last_distance - 3;
    distance_cache[9] = last_distance + 3;
    if (num_distances > 10) {
      int next_last_distance = distance_cache[1];
      distance_cache[10] = next_last_distance - 1;
      distance_cache[11] = next_last_distance + 1;
      distance_cache[12] = next_last_distance - 2;
      distance_cache[13] = next_last_distance + 2;
      distance_cache[14] = next_last_distance - 3;
      distance_cache[15] = next_last_distance + 3;
    }
  }
}

#define BROTLI_LITERAL_BYTE_SCORE 135
#define BROTLI_DISTANCE_BIT_PENALTY 30
/* Score must be positive after applying maximal penalty. */
#define BROTLI_SCORE_BASE (BROTLI_DISTANCE_BIT_PENALTY * 8 * sizeof(size_t))

/* Usually, we always choose the longest backward reference. This function
   allows for the exception of that rule.

   If we choose a backward reference that is further away, it will
   usually be coded with more bits. We approximate this by assuming
   log2(distance). If the distance can be expressed in terms of the
   last four distances, we use some heuristic constants to estimate
   the bits cost. For the first up to four literals we use the bit
   cost of the literals from the literal cost model, after that we
   use the average bit cost of the cost model.

   This function is used to sometimes discard a longer backward reference
   when it is not much longer and the bit cost for encoding it is more
   than the saved literals.

   backward_reference_offset MUST be positive. */
static BROTLI_INLINE score_t BackwardReferenceScore(
    size_t copy_length, size_t backward_reference_offset) {
  return BROTLI_SCORE_BASE + BROTLI_LITERAL_BYTE_SCORE * (score_t)copy_length -
      BROTLI_DISTANCE_BIT_PENALTY * Log2FloorNonZero(backward_reference_offset);
}

static BROTLI_INLINE score_t BackwardReferenceScoreUsingLastDistance(
    size_t copy_length) {
  return BROTLI_LITERAL_BYTE_SCORE * (score_t)copy_length +
      BROTLI_SCORE_BASE + 15;
}

static BROTLI_INLINE score_t BackwardReferencePenaltyUsingLastDistance(
    size_t distance_short_code) {
  return (score_t)39 + ((0x1CA10 >> (distance_short_code & 0xE)) & 0xE);
}

static BROTLI_INLINE BROTLI_BOOL TestStaticDictionaryItem(
    const BrotliEncoderDictionary* dictionary, size_t item, const uint8_t* data,
    size_t max_length, size_t max_backward, size_t max_distance,
    HasherSearchResult* out) {
  size_t len;
  size_t dist;
  size_t offset;
  size_t matchlen;
  size_t backward;
  score_t score;
  len = item & 0x1F;
  dist = item >> 5;
  offset = dictionary->words->offsets_by_length[len] + len * dist;
  if (len > max_length) {
    return BROTLI_FALSE;
  }

  matchlen =
      FindMatchLengthWithLimit(data, &dictionary->words->data[offset], len);
  if (matchlen + dictionary->cutoffTransformsCount <= len || matchlen == 0) {
    return BROTLI_FALSE;
  }
  {
    size_t cut = len - matchlen;
    size_t transform_id = (cut << 2) +
        (size_t)((dictionary->cutoffTransforms >> (cut * 6)) & 0x3F);
    backward = max_backward + dist + 1 +
        (transform_id << dictionary->words->size_bits_by_length[len]);
  }
  if (backward > max_distance) {
    return BROTLI_FALSE;
  }
  score = BackwardReferenceScore(matchlen, backward);
  if (score < out->score) {
    return BROTLI_FALSE;
  }
  out->len = matchlen;
  out->len_code_delta = (int)len - (int)matchlen;
  out->distance = backward;
  out->score = score;
  return BROTLI_TRUE;
}

static BROTLI_INLINE void SearchInStaticDictionary(
    const BrotliEncoderDictionary* dictionary,
    HasherHandle handle, const uint8_t* data, size_t max_length,
    size_t max_backward, size_t max_distance,
    HasherSearchResult* out, BROTLI_BOOL shallow) {
  size_t key;
  size_t i;
  HasherCommon* self = GetHasherCommon(handle);
  if (self->dict_num_matches < (self->dict_num_lookups >> 7)) {
    return;
  }
  key = Hash14(data) << 1;
  for (i = 0; i < (shallow ? 1u : 2u); ++i, ++key) {
    size_t item = dictionary->hash_table[key];
    self->dict_num_lookups++;
    if (item != 0) {
      BROTLI_BOOL item_matches = TestStaticDictionaryItem(
          dictionary, item, data, max_length, max_backward, max_distance, out);
      if (item_matches) {
        self->dict_num_matches++;
      }
    }
  }
}

typedef struct BackwardMatch {
  uint32_t distance;
  uint32_t length_and_code;
} BackwardMatch;

static BROTLI_INLINE void InitBackwardMatch(BackwardMatch* self,
    size_t dist, size_t len) {
  self->distance = (uint32_t)dist;
  self->length_and_code = (uint32_t)(len << 5);
}

static BROTLI_INLINE void InitDictionaryBackwardMatch(BackwardMatch* self,
    size_t dist, size_t len, size_t len_code) {
  self->distance = (uint32_t)dist;
  self->length_and_code =
      (uint32_t)((len << 5) | (len == len_code ? 0 : len_code));
}

static BROTLI_INLINE size_t BackwardMatchLength(const BackwardMatch* self) {
  return self->length_and_code >> 5;
}

static BROTLI_INLINE size_t BackwardMatchLengthCode(const BackwardMatch* self) {
  size_t code = self->length_and_code & 31;
  return code ? code : BackwardMatchLength(self);
}

#define EXPAND_CAT(a, b) CAT(a, b)
#define CAT(a, b) a ## b
#define FN(X) EXPAND_CAT(X, HASHER())

#define HASHER() H10
#define BUCKET_BITS 17
#define MAX_TREE_SEARCH_DEPTH 64
#define MAX_TREE_COMP_LENGTH 128
#include "./hash_to_binary_tree_inc.h"  /* NOLINT(build/include) */
#undef MAX_TREE_SEARCH_DEPTH
#undef MAX_TREE_COMP_LENGTH
#undef BUCKET_BITS
#undef HASHER
/* MAX_NUM_MATCHES == 64 + MAX_TREE_SEARCH_DEPTH */
#define MAX_NUM_MATCHES_H10 128

/* For BUCKET_SWEEP == 1, enabling the dictionary lookup makes compression
   a little faster (0.5% - 1%) and it compresses 0.15% better on small text
   and HTML inputs. */

#define HASHER() H2
#define BUCKET_BITS 16
#define BUCKET_SWEEP 1
#define HASH_LEN 5
#define USE_DICTIONARY 1
#include "./hash_longest_match_quickly_inc.h"  /* NOLINT(build/include) */
#undef BUCKET_SWEEP
#undef USE_DICTIONARY
#undef HASHER

#define HASHER() H3
#define BUCKET_SWEEP 2
#define USE_DICTIONARY 0
#include "./hash_longest_match_quickly_inc.h"  /* NOLINT(build/include) */
#undef USE_DICTIONARY
#undef BUCKET_SWEEP
#undef BUCKET_BITS
#undef HASHER

#define HASHER() H4
#define BUCKET_BITS 17
#define BUCKET_SWEEP 4
#define USE_DICTIONARY 1
#include "./hash_longest_match_quickly_inc.h"  /* NOLINT(build/include) */
#undef USE_DICTIONARY
#undef HASH_LEN
#undef BUCKET_SWEEP
#undef BUCKET_BITS
#undef HASHER

#define HASHER() H5
#include "./hash_longest_match_inc.h"  /* NOLINT(build/include) */
#undef HASHER

#define HASHER() H6
#include "./hash_longest_match64_inc.h"  /* NOLINT(build/include) */
#undef HASHER

#define BUCKET_BITS 15

#define NUM_LAST_DISTANCES_TO_CHECK 4
#define NUM_BANKS 1
#define BANK_BITS 16
#define HASHER() H40
#include "./hash_forgetful_chain_inc.h"  /* NOLINT(build/include) */
#undef HASHER
#undef NUM_LAST_DISTANCES_TO_CHECK

#define NUM_LAST_DISTANCES_TO_CHECK 10
#define HASHER() H41
#include "./hash_forgetful_chain_inc.h"  /* NOLINT(build/include) */
#undef HASHER
#undef NUM_LAST_DISTANCES_TO_CHECK
#undef NUM_BANKS
#undef BANK_BITS

#define NUM_LAST_DISTANCES_TO_CHECK 16
#define NUM_BANKS 512
#define BANK_BITS 9
#define HASHER() H42
#include "./hash_forgetful_chain_inc.h"  /* NOLINT(build/include) */
#undef HASHER
#undef NUM_LAST_DISTANCES_TO_CHECK
#undef NUM_BANKS
#undef BANK_BITS

#undef BUCKET_BITS

#define HASHER() H54
#define BUCKET_BITS 20
#define BUCKET_SWEEP 4
#define HASH_LEN 7
#define USE_DICTIONARY 0
#include "./hash_longest_match_quickly_inc.h"  /* NOLINT(build/include) */
#undef USE_DICTIONARY
#undef HASH_LEN
#undef BUCKET_SWEEP
#undef BUCKET_BITS
#undef HASHER

#undef FN
#undef CAT
#undef EXPAND_CAT

#define FOR_GENERIC_HASHERS(H) H(2) H(3) H(4) H(5) H(6) H(40) H(41) H(42) H(54)
#define FOR_ALL_HASHERS(H) FOR_GENERIC_HASHERS(H) H(10)

static BROTLI_INLINE void DestroyHasher(
    MemoryManager* m, HasherHandle* handle) {
  if (*handle == NULL) return;
  BROTLI_FREE(m, *handle);
}

static BROTLI_INLINE void HasherReset(HasherHandle handle) {
  if (handle == NULL) return;
  GetHasherCommon(handle)->is_prepared_ = BROTLI_FALSE;
}

static BROTLI_INLINE size_t HasherSize(const BrotliEncoderParams* params,
    BROTLI_BOOL one_shot, const size_t input_size) {
  size_t result = sizeof(HasherCommon);
  switch (params->hasher.type) {
#define SIZE_(N)                                                         \
    case N:                                                              \
      result += HashMemAllocInBytesH ## N(params, one_shot, input_size); \
      break;
    FOR_ALL_HASHERS(SIZE_)
#undef SIZE_
    default:
      break;
  }
  return result;
}

static BROTLI_INLINE void HasherSetup(MemoryManager* m, HasherHandle* handle,
    BrotliEncoderParams* params, const uint8_t* data, size_t position,
    size_t input_size, BROTLI_BOOL is_last) {
  HasherHandle self = NULL;
  HasherCommon* common = NULL;
  BROTLI_BOOL one_shot = (position == 0 && is_last);
  if (*handle == NULL) {
    size_t alloc_size;
    ChooseHasher(params, &params->hasher);
    alloc_size = HasherSize(params, one_shot, input_size);
    self = BROTLI_ALLOC(m, uint8_t, alloc_size);
    if (BROTLI_IS_OOM(m)) return;
    *handle = self;
    common = GetHasherCommon(self);
    common->params = params->hasher;
    switch (common->params.type) {
#define INITIALIZE_(N)                     \
      case N:                              \
        InitializeH ## N(*handle, params); \
        break;
      FOR_ALL_HASHERS(INITIALIZE_);
#undef INITIALIZE_
      default:
        break;
    }
    HasherReset(*handle);
  }

  self = *handle;
  common = GetHasherCommon(self);
  if (!common->is_prepared_) {
    switch (common->params.type) {
#define PREPARE_(N)                                      \
      case N:                                            \
        PrepareH ## N(self, one_shot, input_size, data); \
        break;
      FOR_ALL_HASHERS(PREPARE_)
#undef PREPARE_
      default: break;
    }
    if (position == 0) {
        common->dict_num_lookups = 0;
        common->dict_num_matches = 0;
    }
    common->is_prepared_ = BROTLI_TRUE;
  }
}

static BROTLI_INLINE void InitOrStitchToPreviousBlock(
    MemoryManager* m, HasherHandle* handle, const uint8_t* data, size_t mask,
    BrotliEncoderParams* params, size_t position, size_t input_size,
    BROTLI_BOOL is_last) {
  HasherHandle self;
  HasherSetup(m, handle, params, data, position, input_size, is_last);
  if (BROTLI_IS_OOM(m)) return;
  self = *handle;
  switch (GetHasherCommon(self)->params.type) {
#define INIT_(N)                                                           \
    case N:                                                                \
      StitchToPreviousBlockH ## N(self, input_size, position, data, mask); \
    break;
    FOR_ALL_HASHERS(INIT_)
#undef INIT_
    default: break;
  }
}

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_HASH_H_ */

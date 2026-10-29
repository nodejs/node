/* Copyright 2025 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Lookup table for static dictionary and transforms. */

#include "static_dict_lut.h"

#include "../common/platform.h"  /* IWYU pragma: keep */
#include "../common/static_init.h"

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
#include "../common/dictionary.h"
#include "../common/transform.h"
#include "hash_base.h"
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)

/* TODO(eustas): deal with largest bucket(s). Not it contains 163 items. */
static BROTLI_BOOL BROTLI_COLD DoBrotliEncoderInitStaticDictionaryLut(
    const BrotliDictionary* dict, uint16_t* buckets, DictWord* words,
    void* arena) {
  DictWord* slots = (DictWord*)arena;
  uint16_t* heads = (uint16_t*)(slots + BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS);
  uint16_t* counts = heads + BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS;
  uint16_t* prev = counts + BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS;
  size_t next_slot = 0;
  uint8_t transformed_word[24];
  uint8_t transformed_other_word[24];
  size_t l;
  size_t pos;
  size_t i;

  memset(counts, 0, BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS * sizeof(uint16_t));
  memset(heads, 0, BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS * sizeof(uint16_t));
  memset(prev, 0, BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS * sizeof(uint16_t));

  for (l = 4; l <= 24; ++l) {
    size_t n = 1u << dict->size_bits_by_length[l];
    const uint8_t* dict_words = dict->data + dict->offsets_by_length[l];
    for (i = 0; i < n; ++i) {
      const uint8_t* dict_word = dict_words + l * i;
      uint32_t key = Hash15(dict_word);
      slots[next_slot].len = (uint8_t)l;
      slots[next_slot].transform = BROTLI_TRANSFORM_IDENTITY;
      slots[next_slot].idx = (uint16_t)i;
      prev[next_slot] = heads[key];
      heads[key] = (uint16_t)next_slot;
      counts[key]++;
      ++next_slot;
    }
    for (i = 0; i < n; ++i) {
      uint32_t key;
      uint32_t prefix;
      BROTLI_BOOL found;
      size_t curr;
      const uint8_t* dict_word = dict_words + l * i;
      if (dict_word[0] < 'a' || dict_word[0] > 'z') continue;
      memcpy(transformed_word, dict_word, l);
      transformed_word[0] = transformed_word[0] - 32;
      key = Hash15(transformed_word);
      prefix = BROTLI_UNALIGNED_LOAD32LE(transformed_word) & ~0x20202020u;
      found = BROTLI_FALSE;
      curr = heads[key];
      while (curr != 0) {
        const uint8_t* other_word;
        uint32_t other_prefix;
        if (slots[curr].len != l) break;
        other_word = dict_words + l * slots[curr].idx;
        other_prefix = BROTLI_UNALIGNED_LOAD32LE(other_word) & ~0x20202020u;
        if (prefix == other_prefix) {
          if (memcmp(transformed_word, other_word, l) == 0) {
            found = BROTLI_TRUE;
            break;
          }
        }
        curr = prev[curr];
      }
      if (found) continue;
      slots[next_slot].len = (uint8_t)l;
      slots[next_slot].transform = BROTLI_TRANSFORM_UPPERCASE_FIRST;
      slots[next_slot].idx = (uint16_t)i;
      prev[next_slot] = heads[key];
      heads[key] = (uint16_t)next_slot;
      counts[key]++;
      ++next_slot;
    }
    for (i = 0; i < n; ++i) {
      const uint8_t* dict_word = dict_words + l * i;
      BROTLI_BOOL is_ascii = BROTLI_TRUE;
      BROTLI_BOOL has_lower = BROTLI_FALSE;
      size_t k;
      uint32_t prefix;
      uint32_t key;
      size_t curr;
      BROTLI_BOOL found;
      for (k = 0; k < l; ++k) {
        if (dict_word[k] >= 128) is_ascii = BROTLI_FALSE;
        if (k > 0 && dict_word[k] >= 'a' && dict_word[k] <= 'z')
          has_lower = BROTLI_TRUE;
      }
      if (!is_ascii || !has_lower) continue;
      memcpy(transformed_word, dict_word, l);
      prefix = BROTLI_UNALIGNED_LOAD32LE(transformed_word) & ~0x20202020u;
      for (k = 0; k < l; ++k) {
        if (transformed_word[k] >= 'a' && transformed_word[k] <= 'z') {
          transformed_word[k] = transformed_word[k] - 32;
        }
      }
      key = Hash15(transformed_word);
      found = BROTLI_FALSE;
      curr = heads[key];
      while (curr != 0) {
        const uint8_t* other_word;
        uint32_t other_prefix;
        if (slots[curr].len != l) break;
        other_word = dict_words + l * slots[curr].idx;
        other_prefix = BROTLI_UNALIGNED_LOAD32LE(other_word) & ~0x20202020u;
        if (prefix == other_prefix) {
          if (slots[curr].transform == BROTLI_TRANSFORM_IDENTITY) {
            if (memcmp(transformed_word, other_word, l) == 0) {
              found = BROTLI_TRUE;
              break;
            }
          } else if (slots[curr].transform ==
                     BROTLI_TRANSFORM_UPPERCASE_FIRST) {
            if ((transformed_word[0] == (other_word[0] - 32)) &&
                memcmp(transformed_word + 1, other_word + 1, l - 1) == 0) {
              found = BROTLI_TRUE;
              break;
            }
          } else {
            for (k = 0; k < l; ++k) {
              if (other_word[k] >= 'a' && other_word[k] <= 'z') {
                transformed_other_word[k] = other_word[k] - 32;
              } else {
                transformed_other_word[k] = other_word[k];
              }
            }
            if (memcmp(transformed_word, transformed_other_word, l) == 0) {
              found = BROTLI_TRUE;
              break;
            }
          }
        }
        curr = prev[curr];
      }
      if (found) {
        continue;
      }
      slots[next_slot].len = (uint8_t)l;
      slots[next_slot].transform = BROTLI_TRANSFORM_UPPERCASE_ALL;
      slots[next_slot].idx = (uint16_t)i;
      prev[next_slot] = heads[key];
      heads[key] = (uint16_t)next_slot;
      counts[key]++;
      ++next_slot;
    }
  }

  if (next_slot != 31704) return BROTLI_FALSE;
  pos = 0;
  /* Unused; makes offsets start from 1. */
  words[pos].len = 0;
  words[pos].transform = 0;
  words[pos].idx = 0;
  pos++;
  for (i = 0; i < BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS; ++i) {
    size_t num_words = counts[i];
    size_t curr;
    if (num_words == 0) {
      buckets[i] = 0;
      continue;
    }
    buckets[i] = (uint16_t)pos;
    curr = heads[i];
    pos += num_words;
    for (size_t k = 0; k < num_words; ++k) {
      words[pos - 1 - k] = slots[curr];
      curr = prev[curr];
    }
    words[pos - 1].len |= 0x80;
  }
  return BROTLI_TRUE;
}

BROTLI_BOOL BrotliEncoderInitStaticDictionaryLut(
    const BrotliDictionary* dict, uint16_t* buckets, DictWord* words) {
  size_t arena_size =
      BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS *
          (sizeof(uint16_t) + sizeof(DictWord)) +
      BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS * 2 * sizeof(uint16_t);
  void* arena = malloc(arena_size);
  BROTLI_BOOL ok;
  if (arena == NULL) {
    return BROTLI_FALSE;
  }
  ok = DoBrotliEncoderInitStaticDictionaryLut(dict, buckets, words, arena);
  free(arena);
  return ok;
}

BROTLI_MODEL("small")
uint16_t kStaticDictionaryBuckets[BROTLI_ENC_STATIC_DICT_LUT_NUM_BUCKETS];
BROTLI_MODEL("small")
DictWord kStaticDictionaryWords[BROTLI_ENC_STATIC_DICT_LUT_NUM_ITEMS];

#else  /* BROTLI_STATIC_INIT */

/* Embed kStaticDictionaryBuckets and kStaticDictionaryWords. */
#include "static_dict_lut_inc.h"

#endif  /* BROTLI_STATIC_INIT */

#if defined(__cplusplus) || defined(c_plusplus)
} /* extern "C" */
#endif

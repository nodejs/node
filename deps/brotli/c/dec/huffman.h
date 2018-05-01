/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Utilities for building Huffman decoding tables. */

#ifndef BROTLI_DEC_HUFFMAN_H_
#define BROTLI_DEC_HUFFMAN_H_

#include "../common/platform.h"
#include <brotli/types.h>

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#define BROTLI_HUFFMAN_MAX_CODE_LENGTH 15

/* Maximum possible Huffman table size for an alphabet size of (index * 32),
   max code length 15 and root table bits 8. */
static const uint16_t kMaxHuffmanTableSize[] = {
  256, 402, 436, 468, 500, 534, 566, 598, 630, 662, 694, 726, 758, 790, 822,
  854, 886, 920, 952, 984, 1016, 1048, 1080, 1112, 1144, 1176, 1208, 1240, 1272,
  1304, 1336, 1368, 1400, 1432, 1464, 1496, 1528};
/* BROTLI_NUM_BLOCK_LEN_SYMBOLS == 26 */
#define BROTLI_HUFFMAN_MAX_SIZE_26 396
/* BROTLI_MAX_BLOCK_TYPE_SYMBOLS == 258 */
#define BROTLI_HUFFMAN_MAX_SIZE_258 632
/* BROTLI_MAX_CONTEXT_MAP_SYMBOLS == 272 */
#define BROTLI_HUFFMAN_MAX_SIZE_272 646

#define BROTLI_HUFFMAN_MAX_CODE_LENGTH_CODE_LENGTH 5

#if ((defined(BROTLI_TARGET_ARMV7) || defined(BROTLI_TARGET_ARMV8_32)) && \
  BROTLI_GNUC_HAS_ATTRIBUTE(aligned, 2, 7, 0))
#define BROTLI_HUFFMAN_CODE_FAST_LOAD
#endif

#if !defined(BROTLI_HUFFMAN_CODE_FAST_LOAD)
/* Do not create this struct directly - use the ConstructHuffmanCode
 * constructor below! */
typedef struct {
  uint8_t bits;    /* number of bits used for this symbol */
  uint16_t value;  /* symbol value or table offset */
} HuffmanCode;

static BROTLI_INLINE HuffmanCode ConstructHuffmanCode(const uint8_t bits,
    const uint16_t value) {
  HuffmanCode h;
  h.bits = bits;
  h.value = value;
  return h;
}

/* Please use the following macros to optimize HuffmanCode accesses in hot
 * paths.
 *
 * For example, assuming |table| contains a HuffmanCode pointer:
 *
 *   BROTLI_HC_MARK_TABLE_FOR_FAST_LOAD(table);
 *   BROTLI_HC_ADJUST_TABLE_INDEX(table, index_into_table);
 *   *bits = BROTLI_HC_GET_BITS(table);
 *   *value = BROTLI_HC_GET_VALUE(table);
 *   BROTLI_HC_ADJUST_TABLE_INDEX(table, offset);
 *   *bits2 = BROTLI_HC_GET_BITS(table);
 *   *value2 = BROTLI_HC_GET_VALUE(table);
 *
 */

#define BROTLI_HC_MARK_TABLE_FOR_FAST_LOAD(H)
#define BROTLI_HC_ADJUST_TABLE_INDEX(H, V) H += (V)

/* These must be given a HuffmanCode pointer! */
#define BROTLI_HC_FAST_LOAD_BITS(H) (H->bits)
#define BROTLI_HC_FAST_LOAD_VALUE(H) (H->value)

#else /* BROTLI_HUFFMAN_CODE_FAST_LOAD */

typedef BROTLI_ALIGNED(4) uint32_t HuffmanCode;

static BROTLI_INLINE HuffmanCode ConstructHuffmanCode(const uint8_t bits,
    const uint16_t value) {
  return ((value & 0xFFFF) << 16) | (bits & 0xFF);
}

#define BROTLI_HC_MARK_TABLE_FOR_FAST_LOAD(H) uint32_t __fastload_##H = (*H)
#define BROTLI_HC_ADJUST_TABLE_INDEX(H, V) H += (V); __fastload_##H = (*H)

/* These must be given a HuffmanCode pointer! */
#define BROTLI_HC_FAST_LOAD_BITS(H) ((__fastload_##H) & 0xFF)
#define BROTLI_HC_FAST_LOAD_VALUE(H) ((__fastload_##H) >> 16)
#endif /* BROTLI_HUFFMAN_CODE_FAST_LOAD */

/* Builds Huffman lookup table assuming code lengths are in symbol order. */
BROTLI_INTERNAL void BrotliBuildCodeLengthsHuffmanTable(HuffmanCode* root_table,
    const uint8_t* const code_lengths, uint16_t* count);

/* Builds Huffman lookup table assuming code lengths are in symbol order.
   Returns size of resulting table. */
BROTLI_INTERNAL uint32_t BrotliBuildHuffmanTable(HuffmanCode* root_table,
    int root_bits, const uint16_t* const symbol_lists, uint16_t* count_arg);

/* Builds a simple Huffman table. The |num_symbols| parameter is to be
   interpreted as follows: 0 means 1 symbol, 1 means 2 symbols,
   2 means 3 symbols, 3 means 4 symbols with lengths [2, 2, 2, 2],
   4 means 4 symbols with lengths [1, 2, 3, 3]. */
BROTLI_INTERNAL uint32_t BrotliBuildSimpleHuffmanTable(HuffmanCode* table,
    int root_bits, uint16_t* symbols, uint32_t num_symbols);

/* Contains a collection of Huffman trees with the same alphabet size. */
/* max_symbol is needed due to simple codes since log2(alphabet_size) could be
   greater than log2(max_symbol). */
typedef struct {
  HuffmanCode** htrees;
  HuffmanCode* codes;
  uint16_t alphabet_size;
  uint16_t max_symbol;
  uint16_t num_htrees;
} HuffmanTreeGroup;

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_DEC_HUFFMAN_H_ */

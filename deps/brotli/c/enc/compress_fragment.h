/* Copyright 2015 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Function for fast encoding of an input fragment, independently from the input
   history. This function uses one-pass processing: when we find a backward
   match, we immediately emit the corresponding command and literal codes to
   the bit stream. */

#ifndef BROTLI_ENC_COMPRESS_FRAGMENT_H_
#define BROTLI_ENC_COMPRESS_FRAGMENT_H_

#include "../common/platform.h"
#include <brotli/types.h>
#include "./memory.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Compresses "input" string to the "*storage" buffer as one or more complete
   meta-blocks, and updates the "*storage_ix" bit position.

   If "is_last" is 1, emits an additional empty last meta-block.

   "cmd_depth" and "cmd_bits" contain the command and distance prefix codes
   (see comment in encode.h) used for the encoding of this input fragment.
   If "is_last" is 0, they are updated to reflect the statistics
   of this input fragment, to be used for the encoding of the next fragment.

   "*cmd_code_numbits" is the number of bits of the compressed representation
   of the command and distance prefix codes, and "cmd_code" is an array of
   at least "(*cmd_code_numbits + 7) >> 3" size that contains the compressed
   command and distance prefix codes. If "is_last" is 0, these are also
   updated to represent the updated "cmd_depth" and "cmd_bits".

   REQUIRES: "input_size" is greater than zero, or "is_last" is 1.
   REQUIRES: "input_size" is less or equal to maximal metablock size (1 << 24).
   REQUIRES: All elements in "table[0..table_size-1]" are initialized to zero.
   REQUIRES: "table_size" is an odd (9, 11, 13, 15) power of two
   OUTPUT: maximal copy distance <= |input_size|
   OUTPUT: maximal copy distance <= BROTLI_MAX_BACKWARD_LIMIT(18) */
BROTLI_INTERNAL void BrotliCompressFragmentFast(MemoryManager* m,
                                                const uint8_t* input,
                                                size_t input_size,
                                                BROTLI_BOOL is_last,
                                                int* table, size_t table_size,
                                                uint8_t cmd_depth[128],
                                                uint16_t cmd_bits[128],
                                                size_t* cmd_code_numbits,
                                                uint8_t* cmd_code,
                                                size_t* storage_ix,
                                                uint8_t* storage);

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_ENC_COMPRESS_FRAGMENT_H_ */

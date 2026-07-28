/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Lookup tables to map prefix codes to value ranges. This is used during
   decoding of the block lengths, literal insertion lengths and copy lengths. */

#ifndef BROTLI_DEC_PREFIX_H_
#define BROTLI_DEC_PREFIX_H_

#include "../common/constants.h"
#include "../common/platform.h"  /* IWYU pragma: keep */
#include "../common/static_init.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

typedef struct CmdLutElement {
  uint8_t insert_len_extra_bits;
  uint8_t copy_len_extra_bits;
  int8_t distance_code;
  uint8_t context;
  uint16_t insert_len_offset;
  uint16_t copy_len_offset;
} CmdLutElement;

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_NONE)
BROTLI_INTERNAL extern const BROTLI_MODEL("small")
    CmdLutElement kCmdLut[BROTLI_NUM_COMMAND_SYMBOLS];
#else
BROTLI_INTERNAL BROTLI_BOOL BrotliDecoderInitCmdLut(CmdLutElement* items);
BROTLI_INTERNAL extern BROTLI_MODEL("small")
    CmdLutElement kCmdLut[BROTLI_NUM_COMMAND_SYMBOLS];
#endif

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

#endif  /* BROTLI_DEC_PREFIX_H_ */

/* Copyright 2025 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#include "prefix.h"

#include "../common/platform.h"  /* IWYU pragma: keep */
#include "../common/static_init.h"

#if (BROTLI_STATIC_INIT != BROTLI_STATIC_INIT_NONE)
#include "../common/constants.h"
#endif

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#if (BROTLI_STATIC_INIT == BROTLI_STATIC_INIT_NONE)
/* Embed kCmdLut. */
#include "prefix_inc.h"
#else
BROTLI_COLD BROTLI_BOOL BrotliDecoderInitCmdLut(CmdLutElement* items) {
  static const uint8_t kInsertLengthExtraBits[24] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03,
      0x04, 0x04, 0x05, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0C, 0x0E, 0x18};
  static const uint8_t kCopyLengthExtraBits[24] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x02,
      0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x18};
  static const uint8_t kCellPos[11] = {0, 1, 0, 1, 8, 9, 2, 16, 10, 17, 18};

  uint16_t insert_length_offsets[24];
  uint16_t copy_length_offsets[24];
  insert_length_offsets[0] = 0;
  copy_length_offsets[0] = 2;
  for (size_t i = 0; i < 23; ++i) {
    insert_length_offsets[i + 1] =
        insert_length_offsets[i] + (uint16_t)(1u << kInsertLengthExtraBits[i]);
    copy_length_offsets[i + 1] =
        copy_length_offsets[i] + (uint16_t)(1u << kCopyLengthExtraBits[i]);
  }

  for (size_t symbol = 0; symbol < BROTLI_NUM_COMMAND_SYMBOLS; ++symbol) {
    CmdLutElement* item = items + symbol;
    const size_t cell_idx = symbol >> 6;
    const size_t cell_pos = kCellPos[cell_idx];
    const size_t copy_code = ((cell_pos << 3) & 0x18) + (symbol & 0x7);
    const uint16_t copy_len_offset = copy_length_offsets[copy_code];
    const size_t insert_code = (cell_pos & 0x18) + ((symbol >> 3) & 0x7);
    item->copy_len_extra_bits = kCopyLengthExtraBits[copy_code];
    item->context = (copy_len_offset > 4) ? 3 : ((uint8_t)copy_len_offset - 2);
    item->copy_len_offset = copy_len_offset;
    item->distance_code = (cell_idx >= 2) ? -1 : 0;
    item->insert_len_extra_bits = kInsertLengthExtraBits[insert_code];
    item->insert_len_offset = insert_length_offsets[insert_code];
  }
  return BROTLI_TRUE;
}

BROTLI_MODEL("small")
CmdLutElement kCmdLut[BROTLI_NUM_COMMAND_SYMBOLS];
#endif  /* BROTLI_STATIC_INIT */

#if defined(__cplusplus) || defined(c_plusplus)
}  /* extern "C" */
#endif

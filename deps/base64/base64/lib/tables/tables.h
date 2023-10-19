#ifndef BASE64_TABLES_H
#define BASE64_TABLES_H

#include <stdint.h>

#include "../env.h"

// These tables are used by all codecs for fallback plain encoding/decoding:
extern const uint8_t base64_table_enc_6bit[];
extern const uint8_t base64_table_dec_8bit[];

// These tables are used for the 32-bit and 64-bit generic decoders:
#if BASE64_WORDSIZE >= 32
extern const uint32_t base64_table_dec_32bit_d0[];
extern const uint32_t base64_table_dec_32bit_d1[];
extern const uint32_t base64_table_dec_32bit_d2[];
extern const uint32_t base64_table_dec_32bit_d3[];

// This table is used by the 32 and 64-bit generic encoders:
extern const uint16_t base64_table_enc_12bit[];
#endif

#endif	// BASE64_TABLES_H

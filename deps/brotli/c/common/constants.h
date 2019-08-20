/* Copyright 2016 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

#ifndef BROTLI_COMMON_CONSTANTS_H_
#define BROTLI_COMMON_CONSTANTS_H_

/* Specification: 7.3. Encoding of the context map */
#define BROTLI_CONTEXT_MAP_MAX_RLE 16

/* Specification: 2. Compressed representation overview */
#define BROTLI_MAX_NUMBER_OF_BLOCK_TYPES 256

/* Specification: 3.3. Alphabet sizes: insert-and-copy length */
#define BROTLI_NUM_LITERAL_SYMBOLS 256
#define BROTLI_NUM_COMMAND_SYMBOLS 704
#define BROTLI_NUM_BLOCK_LEN_SYMBOLS 26
#define BROTLI_MAX_CONTEXT_MAP_SYMBOLS (BROTLI_MAX_NUMBER_OF_BLOCK_TYPES + \
                                        BROTLI_CONTEXT_MAP_MAX_RLE)
#define BROTLI_MAX_BLOCK_TYPE_SYMBOLS (BROTLI_MAX_NUMBER_OF_BLOCK_TYPES + 2)

/* Specification: 3.5. Complex prefix codes */
#define BROTLI_REPEAT_PREVIOUS_CODE_LENGTH 16
#define BROTLI_REPEAT_ZERO_CODE_LENGTH 17
#define BROTLI_CODE_LENGTH_CODES (BROTLI_REPEAT_ZERO_CODE_LENGTH + 1)
/* "code length of 8 is repeated" */
#define BROTLI_INITIAL_REPEATED_CODE_LENGTH 8

/* "Large Window Brotli" */
#define BROTLI_LARGE_MAX_DISTANCE_BITS 62U
#define BROTLI_LARGE_MIN_WBITS 10
#define BROTLI_LARGE_MAX_WBITS 30

/* Specification: 4. Encoding of distances */
#define BROTLI_NUM_DISTANCE_SHORT_CODES 16
#define BROTLI_MAX_NPOSTFIX 3
#define BROTLI_MAX_NDIRECT 120
#define BROTLI_MAX_DISTANCE_BITS 24U
#define BROTLI_DISTANCE_ALPHABET_SIZE(NPOSTFIX, NDIRECT, MAXNBITS) ( \
    BROTLI_NUM_DISTANCE_SHORT_CODES + (NDIRECT) +                    \
    ((MAXNBITS) << ((NPOSTFIX) + 1)))
/* BROTLI_NUM_DISTANCE_SYMBOLS == 1128 */
#define BROTLI_NUM_DISTANCE_SYMBOLS \
    BROTLI_DISTANCE_ALPHABET_SIZE(  \
        BROTLI_MAX_NDIRECT, BROTLI_MAX_NPOSTFIX, BROTLI_LARGE_MAX_DISTANCE_BITS)
#define BROTLI_MAX_DISTANCE 0x3FFFFFC
#define BROTLI_MAX_ALLOWED_DISTANCE 0x7FFFFFFC

/* 7.1. Context modes and context ID lookup for literals */
/* "context IDs for literals are in the range of 0..63" */
#define BROTLI_LITERAL_CONTEXT_BITS 6

/* 7.2. Context ID for distances */
#define BROTLI_DISTANCE_CONTEXT_BITS 2

/* 9.1. Format of the Stream Header */
/* Number of slack bytes for window size. Don't confuse
   with BROTLI_NUM_DISTANCE_SHORT_CODES. */
#define BROTLI_WINDOW_GAP 16
#define BROTLI_MAX_BACKWARD_LIMIT(W) (((size_t)1 << (W)) - BROTLI_WINDOW_GAP)

#endif  /* BROTLI_COMMON_CONSTANTS_H_ */

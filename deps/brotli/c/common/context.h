/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* Lookup table to map the previous two bytes to a context id.

  There are four different context modeling modes defined here:
    CONTEXT_LSB6: context id is the least significant 6 bits of the last byte,
    CONTEXT_MSB6: context id is the most significant 6 bits of the last byte,
    CONTEXT_UTF8: second-order context model tuned for UTF8-encoded text,
    CONTEXT_SIGNED: second-order context model tuned for signed integers.

  If |p1| and |p2| are the previous two bytes, and |mode| is current context
  mode, we calculate the context as:

    context = ContextLut(mode)[p1] | ContextLut(mode)[p2 + 256].

  For CONTEXT_UTF8 mode, if the previous two bytes are ASCII characters
  (i.e. < 128), this will be equivalent to

    context = 4 * context1(p1) + context2(p2),

  where context1 is based on the previous byte in the following way:

    0  : non-ASCII control
    1  : \t, \n, \r
    2  : space
    3  : other punctuation
    4  : " '
    5  : %
    6  : ( < [ {
    7  : ) > ] }
    8  : , ; :
    9  : .
    10 : =
    11 : number
    12 : upper-case vowel
    13 : upper-case consonant
    14 : lower-case vowel
    15 : lower-case consonant

  and context2 is based on the second last byte:

    0 : control, space
    1 : punctuation
    2 : upper-case letter, number
    3 : lower-case letter

  If the last byte is ASCII, and the second last byte is not (in a valid UTF8
  stream it will be a continuation byte, value between 128 and 191), the
  context is the same as if the second last byte was an ASCII control or space.

  If the last byte is a UTF8 lead byte (value >= 192), then the next byte will
  be a continuation byte and the context id is 2 or 3 depending on the LSB of
  the last byte and to a lesser extent on the second last byte if it is ASCII.

  If the last byte is a UTF8 continuation byte, the second last byte can be:
    - continuation byte: the next byte is probably ASCII or lead byte (assuming
      4-byte UTF8 characters are rare) and the context id is 0 or 1.
    - lead byte (192 - 207): next byte is ASCII or lead byte, context is 0 or 1
    - lead byte (208 - 255): next byte is continuation byte, context is 2 or 3

  The possible value combinations of the previous two bytes, the range of
  context ids and the type of the next byte is summarized in the table below:

  |--------\-----------------------------------------------------------------|
  |         \                         Last byte                              |
  | Second   \---------------------------------------------------------------|
  | last byte \    ASCII            |   cont. byte        |   lead byte      |
  |            \   (0-127)          |   (128-191)         |   (192-)         |
  |=============|===================|=====================|==================|
  |  ASCII      | next: ASCII/lead  |  not valid          |  next: cont.     |
  |  (0-127)    | context: 4 - 63   |                     |  context: 2 - 3  |
  |-------------|-------------------|---------------------|------------------|
  |  cont. byte | next: ASCII/lead  |  next: ASCII/lead   |  next: cont.     |
  |  (128-191)  | context: 4 - 63   |  context: 0 - 1     |  context: 2 - 3  |
  |-------------|-------------------|---------------------|------------------|
  |  lead byte  | not valid         |  next: ASCII/lead   |  not valid       |
  |  (192-207)  |                   |  context: 0 - 1     |                  |
  |-------------|-------------------|---------------------|------------------|
  |  lead byte  | not valid         |  next: cont.        |  not valid       |
  |  (208-)     |                   |  context: 2 - 3     |                  |
  |-------------|-------------------|---------------------|------------------|
*/

#ifndef BROTLI_COMMON_CONTEXT_H_
#define BROTLI_COMMON_CONTEXT_H_

#include "platform.h"

typedef enum ContextType {
  CONTEXT_LSB6 = 0,
  CONTEXT_MSB6 = 1,
  CONTEXT_UTF8 = 2,
  CONTEXT_SIGNED = 3
} ContextType;

/* "Soft-private", it is exported, but not "advertised" as API. */
/* Common context lookup table for all context modes. */
BROTLI_COMMON_API extern const uint8_t _kBrotliContextLookupTable[2048];

typedef const uint8_t* ContextLut;

/* typeof(MODE) == ContextType; returns ContextLut */
#define BROTLI_CONTEXT_LUT(MODE) (&_kBrotliContextLookupTable[(MODE) << 9])

/* typeof(LUT) == ContextLut */
#define BROTLI_CONTEXT(P1, P2, LUT) ((LUT)[P1] | ((LUT) + 256)[P2])

#endif  /* BROTLI_COMMON_CONTEXT_H_ */

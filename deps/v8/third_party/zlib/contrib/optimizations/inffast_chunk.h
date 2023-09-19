/* inffast_chunk.h -- header to use inffast_chunk.c
 * Copyright (C) 1995-2003, 2010 Mark Adler
 * Copyright (C) 2017 ARM, Inc.
 * Copyright 2023 The Chromium Authors
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* WARNING: this file should *not* be used by applications. It is
   part of the implementation of the compression library and is
   subject to change. Applications should only use zlib.h.
 */

#include "inffast.h"

/* INFLATE_FAST_MIN_INPUT:
   The minimum number of input bytes needed so that we can safely call
   inflate_fast() with only one up-front bounds check. One
   length/distance code pair (15 bits for the length code, 5 bits for length
   extra, 15 bits for the distance code, 13 bits for distance extra) requires
   reading up to 48 input bits. Additionally, in the same iteraction, we may
   decode two literals from the root-table (requiring MIN_OUTPUT = 258 + 2).

   Each root-table entry is up to 10 bits, for a total of 68 input bits each
   iteraction.

   The refill variant reads 8 bytes from the buffer at a time, and advances
   the input pointer by up to 7 bytes, ensuring there are at least 56-bits
   available in the bit-buffer. The technique was documented by Fabian Giesen
   on his blog as variant 4 in the article 'Reading bits in far too many ways':
   https://fgiesen.wordpress.com/2018/02/20/

   In the worst case, we may refill twice in the same iteraction, requiring
   MIN_INPUT = 8 + 7.
*/
#ifdef INFLATE_CHUNK_READ_64LE
#undef INFLATE_FAST_MIN_INPUT
#define INFLATE_FAST_MIN_INPUT 15
#undef INFLATE_FAST_MIN_OUTPUT
#define INFLATE_FAST_MIN_OUTPUT 260
#endif

void ZLIB_INTERNAL inflate_fast_chunk_ OF((z_streamp strm, unsigned start));

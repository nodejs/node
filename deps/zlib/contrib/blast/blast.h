/* blast.h -- interface for blast.c
  Copyright (C) 2003, 2012, 2013 Mark Adler
  version 1.3, 24 Aug 2013

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler    madler@alumni.caltech.edu
 */


/*
 * blast() decompresses the PKWare Data Compression Library (DCL) compressed
 * format.  It provides the same functionality as the explode() function in
 * that library.  (Note: PKWare overused the "implode" verb, and the format
 * used by their library implode() function is completely different and
 * incompatible with the implode compression method supported by PKZIP.)
 *
 * The binary mode for stdio functions should be used to assure that the
 * compressed data is not corrupted when read or written.  For example:
 * fopen(..., "rb") and fopen(..., "wb").
 */


typedef unsigned (*blast_in)(void *how, unsigned char **buf);
typedef int (*blast_out)(void *how, unsigned char *buf, unsigned len);
/* Definitions for input/output functions passed to blast().  See below for
 * what the provided functions need to do.
 */


int blast(blast_in infun, void *inhow, blast_out outfun, void *outhow,
          unsigned *left, unsigned char **in);
/* Decompress input to output using the provided infun() and outfun() calls.
 * On success, the return value of blast() is zero.  If there is an error in
 * the source data, i.e. it is not in the proper format, then a negative value
 * is returned.  If there is not enough input available or there is not enough
 * output space, then a positive error is returned.
 *
 * The input function is invoked: len = infun(how, &buf), where buf is set by
 * infun() to point to the input buffer, and infun() returns the number of
 * available bytes there.  If infun() returns zero, then blast() returns with
 * an input error.  (blast() only asks for input if it needs it.)  inhow is for
 * use by the application to pass an input descriptor to infun(), if desired.
 *
 * If left and in are not NULL and *left is not zero when blast() is called,
 * then the *left bytes are *in are consumed for input before infun() is used.
 *
 * The output function is invoked: err = outfun(how, buf, len), where the bytes
 * to be written are buf[0..len-1].  If err is not zero, then blast() returns
 * with an output error.  outfun() is always called with len <= 4096.  outhow
 * is for use by the application to pass an output descriptor to outfun(), if
 * desired.
 *
 * If there is any unused input, *left is set to the number of bytes that were
 * read and *in points to them.  Otherwise *left is set to zero and *in is set
 * to NULL.  If left or in are NULL, then they are not set.
 *
 * The return codes are:
 *
 *   2:  ran out of input before completing decompression
 *   1:  output error before completing decompression
 *   0:  successful decompression
 *  -1:  literal flag not zero or one
 *  -2:  dictionary size not in 4..6
 *  -3:  distance is too far back
 *
 * At the bottom of blast.c is an example program that uses blast() that can be
 * compiled to produce a command-line decompression filter by defining TEST.
 */

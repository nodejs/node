/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef NGTCP2_MACRO_H
#define NGTCP2_MACRO_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <stddef.h>

#include <ngtcp2/ngtcp2.h>

#define ngtcp2_struct_of(ptr, type, member)                                    \
  ((type *)(void *)((char *)(ptr) - offsetof(type, member)))

/* ngtcp2_list_insert inserts |T| before |*PD|.  The contract is that
   this is singly linked list, and the next element is pointed by next
   field of the previous element.  |PD| must be a pointer to the
   pointer to the next field of the previous element of |*PD|: if C is
   the previous element of |PD|, PD = &C->next. */
#define ngtcp2_list_insert(T, PD)                                              \
  do {                                                                         \
    (T)->next = *(PD);                                                         \
    *(PD) = (T);                                                               \
  } while (0)

/*
 * ngtcp2_arraylen returns the number of elements in array |A|.
 */
#define ngtcp2_arraylen(A) (sizeof(A) / sizeof(A[0]))

/*
 * ngtcp2_strlen_lit returns the length of string literal |S|.  This
 * macro assumes |S| is NULL-terminated string literal.  It must not
 * be used with pointers.
 */
#define ngtcp2_strlen_lit(S) (sizeof(S) - 1)

#define ngtcp2_max_def(SUFFIX, T)                                              \
  static inline T ngtcp2_max_##SUFFIX(T a, T b) { return a < b ? b : a; }

ngtcp2_max_def(long_long_int, long long int)
ngtcp2_max_def(long_int, long int)
ngtcp2_max_def(int, int)
ngtcp2_max_def(short_int, short int)
ngtcp2_max_def(signed_char, signed char)
ngtcp2_max_def(char, char)
ngtcp2_max_def(unsigned_long_long_int, unsigned long long int)
ngtcp2_max_def(unsigned_long_int, unsigned long int)
ngtcp2_max_def(unsigned_int, unsigned int)
ngtcp2_max_def(unsigned_short_int, unsigned short int)
ngtcp2_max_def(unsigned_char, unsigned char)

#define ngtcp2_max(A, B)                                                       \
  _Generic((A),                                                                \
    long long int: ngtcp2_max_long_long_int,                                   \
    long int: ngtcp2_max_long_int,                                             \
    int: _Generic((B),                                                         \
      long long int: ngtcp2_max_long_long_int,                                 \
      long int: ngtcp2_max_long_int,                                           \
      int: ngtcp2_max_int,                                                     \
      short int: ngtcp2_max_short_int,                                         \
      signed char: ngtcp2_max_signed_char,                                     \
      char: ngtcp2_max_char,                                                   \
      unsigned long long int: ngtcp2_max_unsigned_long_long_int,               \
      unsigned long int: ngtcp2_max_unsigned_long_int,                         \
      unsigned int: ngtcp2_max_unsigned_int,                                   \
      unsigned short int: ngtcp2_max_unsigned_short_int,                       \
      unsigned char: ngtcp2_max_unsigned_char),                                \
    short int: ngtcp2_max_short_int,                                           \
    signed char: ngtcp2_max_signed_char,                                       \
    char: ngtcp2_max_char,                                                     \
    unsigned long long int: ngtcp2_max_unsigned_long_long_int,                 \
    unsigned long int: ngtcp2_max_unsigned_long_int,                           \
    unsigned int: ngtcp2_max_unsigned_int,                                     \
    unsigned short int: ngtcp2_max_unsigned_short_int,                         \
    unsigned char: ngtcp2_max_unsigned_char)((A), (B))

#define ngtcp2_min_def(SUFFIX, T)                                              \
  static inline T ngtcp2_min_##SUFFIX(T a, T b) { return a < b ? a : b; }

ngtcp2_min_def(long_long_int, long long int)
ngtcp2_min_def(long_int, long int)
ngtcp2_min_def(int, int)
ngtcp2_min_def(short_int, short int)
ngtcp2_min_def(signed_char, signed char)
ngtcp2_min_def(char, char)
ngtcp2_min_def(unsigned_long_long_int, unsigned long long int)
ngtcp2_min_def(unsigned_long_int, unsigned long int)
ngtcp2_min_def(unsigned_int, unsigned int)
ngtcp2_min_def(unsigned_short_int, unsigned short int)
ngtcp2_min_def(unsigned_char, unsigned char)

#define ngtcp2_min(A, B)                                                       \
  _Generic((A),                                                                \
    long long int: ngtcp2_min_long_long_int,                                   \
    long int: ngtcp2_min_long_int,                                             \
    int: _Generic((B),                                                         \
      long long int: ngtcp2_min_long_long_int,                                 \
      long int: ngtcp2_min_long_int,                                           \
      int: ngtcp2_min_int,                                                     \
      short int: ngtcp2_min_short_int,                                         \
      signed char: ngtcp2_min_signed_char,                                     \
      char: ngtcp2_min_char,                                                   \
      unsigned long long int: ngtcp2_min_unsigned_long_long_int,               \
      unsigned long int: ngtcp2_min_unsigned_long_int,                         \
      unsigned int: ngtcp2_min_unsigned_int,                                   \
      unsigned short int: ngtcp2_min_unsigned_short_int,                       \
      unsigned char: ngtcp2_min_unsigned_char),                                \
    short int: ngtcp2_min_short_int,                                           \
    signed char: ngtcp2_min_signed_char,                                       \
    char: ngtcp2_min_char,                                                     \
    unsigned long long int: ngtcp2_min_unsigned_long_long_int,                 \
    unsigned long int: ngtcp2_min_unsigned_long_int,                           \
    unsigned int: ngtcp2_min_unsigned_int,                                     \
    unsigned short int: ngtcp2_min_unsigned_short_int,                         \
    unsigned char: ngtcp2_min_unsigned_char)((A), (B))

#endif /* !defined(NGTCP2_MACRO_H) */

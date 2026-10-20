/*
 * ngtcp2
 *
 * Copyright (c) 2026 ngtcp2 contributors
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
#ifndef NGTCP2_FMT_H
#define NGTCP2_FMT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif /* defined(HAVE_CONFIG_H) */

#include <ngtcp2/ngtcp2.h>

typedef struct ngtcp2_fmt_hex {
  uint64_t n;
} ngtcp2_fmt_hex;

static inline ngtcp2_fmt_hex ngtcp2_fmt_hex_init(uint64_t n) {
  return (ngtcp2_fmt_hex){
    .n = n,
  };
}

static inline ngtcp2_fmt_hex ngtcp2_fmt_hex_signed_init(int64_t n) {
  return ngtcp2_fmt_hex_init((uint64_t)n);
}

static inline ngtcp2_fmt_hex ngtcp2_fmt_hex_long_int_init(long int n) {
  return ngtcp2_fmt_hex_init((unsigned long int)n);
}

static inline ngtcp2_fmt_hex ngtcp2_fmt_hex_int_init(int n) {
  return ngtcp2_fmt_hex_init((unsigned int)n);
}

static inline ngtcp2_fmt_hex ngtcp2_fmt_hex_short_int_init(short int n) {
  return ngtcp2_fmt_hex_init((unsigned short int)n);
}

static inline ngtcp2_fmt_hex ngtcp2_fmt_hex_signed_char_init(signed char n) {
  return ngtcp2_fmt_hex_init((unsigned char)n);
}

static inline ngtcp2_fmt_hex ngtcp2_fmt_hex_char_init(char n) {
  return ngtcp2_fmt_hex_init((unsigned char)n);
}

/* hex formats integral |T| in unsigned hexadecimal notation.  It
   drops the leading zeros. */
#define hex(T)                                                                 \
  _Generic((T),                                                                \
    long long int: ngtcp2_fmt_hex_signed_init,                                 \
    long int: ngtcp2_fmt_hex_long_int_init,                                    \
    int: ngtcp2_fmt_hex_int_init,                                              \
    short int: ngtcp2_fmt_hex_short_int_init,                                  \
    signed char: ngtcp2_fmt_hex_signed_char_init,                              \
    char: ngtcp2_fmt_hex_char_init,                                            \
    unsigned long long int: ngtcp2_fmt_hex_init,                               \
    unsigned long int: ngtcp2_fmt_hex_init,                                    \
    unsigned int: ngtcp2_fmt_hex_init,                                         \
    unsigned short int: ngtcp2_fmt_hex_init,                                   \
    unsigned char: ngtcp2_fmt_hex_init)((T))

typedef struct ngtcp2_fmt_hexw {
  uint64_t n;
  size_t width;
} ngtcp2_fmt_hexw;

static inline ngtcp2_fmt_hexw ngtcp2_fmt_hexw_init(uint64_t n, size_t width) {
  return (ngtcp2_fmt_hexw){
    .n = n,
    .width = width,
  };
}

static inline ngtcp2_fmt_hexw ngtcp2_fmt_hexw_signed_init(int64_t n,
                                                          size_t width) {
  return ngtcp2_fmt_hexw_init((uint64_t)n, width);
}

static inline ngtcp2_fmt_hexw ngtcp2_fmt_hexw_long_int_init(long int n,
                                                            size_t width) {
  return ngtcp2_fmt_hexw_init((unsigned long int)n, width);
}

static inline ngtcp2_fmt_hexw ngtcp2_fmt_hexw_int_init(int n, size_t width) {
  return ngtcp2_fmt_hexw_init((unsigned int)n, width);
}

static inline ngtcp2_fmt_hexw ngtcp2_fmt_hexw_short_int_init(short int n,
                                                             size_t width) {
  return ngtcp2_fmt_hexw_init((unsigned short int)n, width);
}

static inline ngtcp2_fmt_hexw ngtcp2_fmt_hexw_signed_char_init(signed char n,
                                                               size_t width) {
  return ngtcp2_fmt_hexw_init((unsigned char)n, width);
}

static inline ngtcp2_fmt_hexw ngtcp2_fmt_hexw_char_init(char n, size_t width) {
  return ngtcp2_fmt_hexw_init((unsigned char)n, width);
}

/* hex formats integral |T| in unsigned hexadecimal notation.  If the
   produced value has fewer characters than |WIDTH|, it will be padded
   with 0 on the left. */
#define hexw(T, WIDTH)                                                         \
  _Generic((T),                                                                \
    long long int: ngtcp2_fmt_hexw_signed_init,                                \
    long int: ngtcp2_fmt_hexw_long_int_init,                                   \
    int: ngtcp2_fmt_hexw_int_init,                                             \
    short int: ngtcp2_fmt_hexw_short_int_init,                                 \
    signed char: ngtcp2_fmt_hexw_signed_char_init,                             \
    char: ngtcp2_fmt_hexw_char_init,                                           \
    unsigned long long int: ngtcp2_fmt_hexw_init,                              \
    unsigned long int: ngtcp2_fmt_hexw_init,                                   \
    unsigned int: ngtcp2_fmt_hexw_init,                                        \
    unsigned short int: ngtcp2_fmt_hexw_init,                                  \
    unsigned char: ngtcp2_fmt_hexw_init)((T), (WIDTH))

typedef struct ngtcp2_fmt_uint64w {
  uint64_t n;
  size_t width;
} ngtcp2_fmt_uint64w;

/* uintw formats integral |n|.  If the produced value has fewer
   characters than |width|, it will be padded with 0 on the left. */
static inline ngtcp2_fmt_uint64w uintw(uint64_t n, size_t width) {
  return (ngtcp2_fmt_uint64w){
    .n = n,
    .width = width,
  };
}

typedef struct ngtcp2_fmt_bhex {
  const uint8_t *data;
  size_t len;
} ngtcp2_fmt_bhex;

/* bhex formats the binary data [|data|, |data| + |len|) in
   hexadecimal notation. */
static inline ngtcp2_fmt_bhex bhex(const uint8_t *data, size_t len) {
  return (ngtcp2_fmt_bhex){
    .data = data,
    .len = len,
  };
}

/* lbhex formats the binary data [|B|, |B| + sizeof(|B|)) in
   hexadecimal notation.  To make it work, |B| must be an array that
   is not decayed to the pointer. */
#define lbhex(B) bhex((B), sizeof(B))

typedef struct ngtcp2_fmt_printable_ascii {
  const uint8_t *data;
  size_t len;
} ngtcp2_fmt_printable_ascii;

/* ascii formats the binary data [|data|, |data| + |len|) in such a
   way that the printable ASCII characters are copied as is, and the
   other characters are converted to '.'. */
static inline ngtcp2_fmt_printable_ascii ascii(const uint8_t *data,
                                               size_t len) {
  return (ngtcp2_fmt_printable_ascii){
    .data = data,
    .len = len,
  };
}

#define ngtcp2_fmt_stringify(M) #M

/* stringify converts macro |M| to string literal.*/
#define stringify(M) ngtcp2_fmt_stringify(M)

char *ngtcp2_fmt_write_int64(char *dest, int64_t n);
char *ngtcp2_fmt_write_uint64(char *dest, uint64_t n);
char *ngtcp2_fmt_write_char(char *dest, char c);
char *ngtcp2_fmt_write_str(char *dest, const char *s);
char *ngtcp2_fmt_write_uint64w(char *dest, ngtcp2_fmt_uint64w f);
char *ngtcp2_fmt_write_hex(char *dest, ngtcp2_fmt_hex f);
char *ngtcp2_fmt_write_hexw(char *dest, ngtcp2_fmt_hexw f);
char *ngtcp2_fmt_write_cid(char *dest, const ngtcp2_cid *cid);
char *ngtcp2_fmt_write_stateless_reset_token(
  char *dest, const ngtcp2_stateless_reset_token *token);
char *
ngtcp2_fmt_write_path_challenge_data(char *dest,
                                     const ngtcp2_path_challenge_data *data);
char *ngtcp2_fmt_write_bhex(char *dest, ngtcp2_fmt_bhex f);
char *ngtcp2_fmt_write_in_addr(char *dest, const ngtcp2_in_addr *addr);
char *ngtcp2_fmt_write_in6_addr(char *dest, const ngtcp2_in6_addr *addr);
char *ngtcp2_fmt_write_printable_ascii(char *dest,
                                       const ngtcp2_fmt_printable_ascii f);

#define NGTCP2_FMT_WRITE_TYPE(DEST, T)                                         \
  _Generic((T),                                                                \
    long long int: ngtcp2_fmt_write_int64,                                     \
    long int: ngtcp2_fmt_write_int64,                                          \
    int: ngtcp2_fmt_write_int64,                                               \
    short int: ngtcp2_fmt_write_int64,                                         \
    signed char: ngtcp2_fmt_write_int64,                                       \
    char: ngtcp2_fmt_write_char,                                               \
    unsigned long long int: ngtcp2_fmt_write_uint64,                           \
    unsigned long int: ngtcp2_fmt_write_uint64,                                \
    unsigned int: ngtcp2_fmt_write_uint64,                                     \
    unsigned short int: ngtcp2_fmt_write_uint64,                               \
    unsigned char: ngtcp2_fmt_write_uint64,                                    \
    char *: ngtcp2_fmt_write_str,                                              \
    const char *: ngtcp2_fmt_write_str,                                        \
    ngtcp2_cid *: ngtcp2_fmt_write_cid,                                        \
    const ngtcp2_cid *: ngtcp2_fmt_write_cid,                                  \
    ngtcp2_stateless_reset_token *: ngtcp2_fmt_write_stateless_reset_token,    \
    const ngtcp2_stateless_reset_token                                         \
      *: ngtcp2_fmt_write_stateless_reset_token,                               \
    ngtcp2_path_challenge_data *: ngtcp2_fmt_write_path_challenge_data,        \
    const ngtcp2_path_challenge_data *: ngtcp2_fmt_write_path_challenge_data,  \
    ngtcp2_in_addr *: ngtcp2_fmt_write_in_addr,                                \
    const ngtcp2_in_addr *: ngtcp2_fmt_write_in_addr,                          \
    ngtcp2_in6_addr *: ngtcp2_fmt_write_in6_addr,                              \
    const ngtcp2_in6_addr *: ngtcp2_fmt_write_in6_addr,                        \
    ngtcp2_fmt_uint64w: ngtcp2_fmt_write_uint64w,                              \
    ngtcp2_fmt_hex: ngtcp2_fmt_write_hex,                                      \
    ngtcp2_fmt_hexw: ngtcp2_fmt_write_hexw,                                    \
    ngtcp2_fmt_bhex: ngtcp2_fmt_write_bhex,                                    \
    ngtcp2_fmt_printable_ascii: ngtcp2_fmt_write_printable_ascii)((DEST), (T))

/* ngtcp2_fmt_format formats arguments and writes them into the buffer
   pointed by |BUF|.  It also writes the terminal NUL byte.  The
   function assumes that the buffer is the large enough.  It assigns
   the number of bytes written, excluding the terminal NUL, to
   |*PNWRITE|. */

/* Generated by fmtgen.py */
#define NGTCP2_FMT_SELECT_WRITE_PACK(                                          \
  _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17,  \
  _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32,   \
  _33, _34, _35, _36, _37, _38, _39, _40, PACK, ...)                           \
  PACK

#define ngtcp2_fmt_format(BUF, PNWRITE, ...)                                   \
  do {                                                                         \
    char *fmt_destp = (char *)(BUF);                                           \
    NGTCP2_FMT_SELECT_WRITE_PACK(                                              \
      __VA_ARGS__, NGTCP2_FMT_WRITE_PACK_40, NGTCP2_FMT_WRITE_PACK_39,         \
      NGTCP2_FMT_WRITE_PACK_38, NGTCP2_FMT_WRITE_PACK_37,                      \
      NGTCP2_FMT_WRITE_PACK_36, NGTCP2_FMT_WRITE_PACK_35,                      \
      NGTCP2_FMT_WRITE_PACK_34, NGTCP2_FMT_WRITE_PACK_33,                      \
      NGTCP2_FMT_WRITE_PACK_32, NGTCP2_FMT_WRITE_PACK_31,                      \
      NGTCP2_FMT_WRITE_PACK_30, NGTCP2_FMT_WRITE_PACK_29,                      \
      NGTCP2_FMT_WRITE_PACK_28, NGTCP2_FMT_WRITE_PACK_27,                      \
      NGTCP2_FMT_WRITE_PACK_26, NGTCP2_FMT_WRITE_PACK_25,                      \
      NGTCP2_FMT_WRITE_PACK_24, NGTCP2_FMT_WRITE_PACK_23,                      \
      NGTCP2_FMT_WRITE_PACK_22, NGTCP2_FMT_WRITE_PACK_21,                      \
      NGTCP2_FMT_WRITE_PACK_20, NGTCP2_FMT_WRITE_PACK_19,                      \
      NGTCP2_FMT_WRITE_PACK_18, NGTCP2_FMT_WRITE_PACK_17,                      \
      NGTCP2_FMT_WRITE_PACK_16, NGTCP2_FMT_WRITE_PACK_15,                      \
      NGTCP2_FMT_WRITE_PACK_14, NGTCP2_FMT_WRITE_PACK_13,                      \
      NGTCP2_FMT_WRITE_PACK_12, NGTCP2_FMT_WRITE_PACK_11,                      \
      NGTCP2_FMT_WRITE_PACK_10, NGTCP2_FMT_WRITE_PACK_9,                       \
      NGTCP2_FMT_WRITE_PACK_8, NGTCP2_FMT_WRITE_PACK_7,                        \
      NGTCP2_FMT_WRITE_PACK_6, NGTCP2_FMT_WRITE_PACK_5,                        \
      NGTCP2_FMT_WRITE_PACK_4, NGTCP2_FMT_WRITE_PACK_3,                        \
      NGTCP2_FMT_WRITE_PACK_2,                                                 \
      NGTCP2_FMT_WRITE_PACK_1)(fmt_destp, __VA_ARGS__);                        \
    *fmt_destp = '\0';                                                         \
    *(PNWRITE) = (size_t)(fmt_destp - (char *)(BUF));                          \
  } while (0)

#define NGTCP2_FMT_WRITE_PACK_1(DEST, _1)                                      \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1))
#define NGTCP2_FMT_WRITE_PACK_2(DEST, _1, _2)                                  \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2))
#define NGTCP2_FMT_WRITE_PACK_3(DEST, _1, _2, _3)                              \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3))
#define NGTCP2_FMT_WRITE_PACK_4(DEST, _1, _2, _3, _4)                          \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4))
#define NGTCP2_FMT_WRITE_PACK_5(DEST, _1, _2, _3, _4, _5)                      \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5))
#define NGTCP2_FMT_WRITE_PACK_6(DEST, _1, _2, _3, _4, _5, _6)                  \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6))
#define NGTCP2_FMT_WRITE_PACK_7(DEST, _1, _2, _3, _4, _5, _6, _7)              \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7))
#define NGTCP2_FMT_WRITE_PACK_8(DEST, _1, _2, _3, _4, _5, _6, _7, _8)          \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8))
#define NGTCP2_FMT_WRITE_PACK_9(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9)      \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9))
#define NGTCP2_FMT_WRITE_PACK_10(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10)                                          \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10))
#define NGTCP2_FMT_WRITE_PACK_11(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11)                                     \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11))
#define NGTCP2_FMT_WRITE_PACK_12(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12)                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12))
#define NGTCP2_FMT_WRITE_PACK_13(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13)                           \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13))
#define NGTCP2_FMT_WRITE_PACK_14(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14)                      \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14))
#define NGTCP2_FMT_WRITE_PACK_15(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15)                 \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15))
#define NGTCP2_FMT_WRITE_PACK_16(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16)            \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16))
#define NGTCP2_FMT_WRITE_PACK_17(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17)       \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17))
#define NGTCP2_FMT_WRITE_PACK_18(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18)  \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18))
#define NGTCP2_FMT_WRITE_PACK_19(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19)                                          \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19))
#define NGTCP2_FMT_WRITE_PACK_20(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20)                                     \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20))
#define NGTCP2_FMT_WRITE_PACK_21(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21)                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21))
#define NGTCP2_FMT_WRITE_PACK_22(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22)                           \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22))
#define NGTCP2_FMT_WRITE_PACK_23(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23)                      \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23))
#define NGTCP2_FMT_WRITE_PACK_24(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24)                 \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24))
#define NGTCP2_FMT_WRITE_PACK_25(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25)            \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25))
#define NGTCP2_FMT_WRITE_PACK_26(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25, _26)       \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26))
#define NGTCP2_FMT_WRITE_PACK_27(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25, _26, _27)  \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27))
#define NGTCP2_FMT_WRITE_PACK_28(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28)                  \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28))
#define NGTCP2_FMT_WRITE_PACK_29(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29)             \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29))
#define NGTCP2_FMT_WRITE_PACK_30(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30)        \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30))
#define NGTCP2_FMT_WRITE_PACK_31(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31)   \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31))
#define NGTCP2_FMT_WRITE_PACK_32(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25, _26, _27,  \
                                 _28, _29, _30, _31, _32)                      \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32))
#define NGTCP2_FMT_WRITE_PACK_33(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25, _26, _27,  \
                                 _28, _29, _30, _31, _32, _33)                 \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33))
#define NGTCP2_FMT_WRITE_PACK_34(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25, _26, _27,  \
                                 _28, _29, _30, _31, _32, _33, _34)            \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_34))
#define NGTCP2_FMT_WRITE_PACK_35(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25, _26, _27,  \
                                 _28, _29, _30, _31, _32, _33, _34, _35)       \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_34));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_35))
#define NGTCP2_FMT_WRITE_PACK_36(DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9,     \
                                 _10, _11, _12, _13, _14, _15, _16, _17, _18,  \
                                 _19, _20, _21, _22, _23, _24, _25, _26, _27,  \
                                 _28, _29, _30, _31, _32, _33, _34, _35, _36)  \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_34));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_35));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_36))
#define NGTCP2_FMT_WRITE_PACK_37(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31,   \
  _32, _33, _34, _35, _36, _37)                                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_34));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_35));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_36));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_37))
#define NGTCP2_FMT_WRITE_PACK_38(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31,   \
  _32, _33, _34, _35, _36, _37, _38)                                           \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_34));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_35));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_36));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_37));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_38))
#define NGTCP2_FMT_WRITE_PACK_39(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31,   \
  _32, _33, _34, _35, _36, _37, _38, _39)                                      \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_34));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_35));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_36));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_37));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_38));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_39))
#define NGTCP2_FMT_WRITE_PACK_40(                                              \
  DEST, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
  _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31,   \
  _32, _33, _34, _35, _36, _37, _38, _39, _40)                                 \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_1));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_2));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_3));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_4));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_5));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_6));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_7));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_8));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_9));                                \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_10));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_11));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_12));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_13));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_14));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_15));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_16));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_17));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_18));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_19));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_20));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_21));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_22));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_23));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_24));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_25));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_26));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_27));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_28));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_29));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_30));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_31));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_32));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_33));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_34));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_35));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_36));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_37));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_38));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_39));                               \
  (DEST) = NGTCP2_FMT_WRITE_TYPE((DEST), (_40))

#endif /* !defined(NGTCP2_FMT_H) */

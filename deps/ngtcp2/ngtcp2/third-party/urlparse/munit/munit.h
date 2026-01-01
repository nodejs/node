/* µnit Testing Framework
 * Copyright (c) 2013-2017 Evan Nemerson <evan@nemerson.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MUNIT_H
#define MUNIT_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define MUNIT_VERSION(major, minor, revision)                                  \
  (((major) << 16) | ((minor) << 8) | (revision))

#define MUNIT_CURRENT_VERSION MUNIT_VERSION(0, 4, 1)

#if defined(_MSC_VER) && (_MSC_VER < 1600)
#  define munit_int8_t __int8
#  define munit_uint8_t unsigned __int8
#  define munit_int16_t __int16
#  define munit_uint16_t unsigned __int16
#  define munit_int32_t __int32
#  define munit_uint32_t unsigned __int32
#  define munit_int64_t __int64
#  define munit_uint64_t unsigned __int64
#else
#  include <stdint.h>
#  define munit_int8_t int8_t
#  define munit_uint8_t uint8_t
#  define munit_int16_t int16_t
#  define munit_uint16_t uint16_t
#  define munit_int32_t int32_t
#  define munit_uint32_t uint32_t
#  define munit_int64_t int64_t
#  define munit_uint64_t uint64_t
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1800)
#  if !defined(PRIi8)
#    define PRIi8 "i"
#  endif
#  if !defined(PRIi16)
#    define PRIi16 "i"
#  endif
#  if !defined(PRIi32)
#    define PRIi32 "i"
#  endif
#  if !defined(PRIi64)
#    define PRIi64 "I64i"
#  endif
#  if !defined(PRId8)
#    define PRId8 "d"
#  endif
#  if !defined(PRId16)
#    define PRId16 "d"
#  endif
#  if !defined(PRId32)
#    define PRId32 "d"
#  endif
#  if !defined(PRId64)
#    define PRId64 "I64d"
#  endif
#  if !defined(PRIx8)
#    define PRIx8 "x"
#  endif
#  if !defined(PRIx16)
#    define PRIx16 "x"
#  endif
#  if !defined(PRIx32)
#    define PRIx32 "x"
#  endif
#  if !defined(PRIx64)
#    define PRIx64 "I64x"
#  endif
#  if !defined(PRIu8)
#    define PRIu8 "u"
#  endif
#  if !defined(PRIu16)
#    define PRIu16 "u"
#  endif
#  if !defined(PRIu32)
#    define PRIu32 "u"
#  endif
#  if !defined(PRIu64)
#    define PRIu64 "I64u"
#  endif
#else
#  include <inttypes.h>
#endif

#if !defined(munit_bool)
#  if defined(bool)
#    define munit_bool bool
#  elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#    define munit_bool _Bool
#  else
#    define munit_bool int
#  endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__GNUC__)
#  define MUNIT_LIKELY(expr) (__builtin_expect((expr), 1))
#  define MUNIT_UNLIKELY(expr) (__builtin_expect((expr), 0))
#  define MUNIT_UNUSED __attribute__((__unused__))
#else
#  define MUNIT_LIKELY(expr) (expr)
#  define MUNIT_UNLIKELY(expr) (expr)
#  define MUNIT_UNUSED
#endif

#if !defined(_WIN32)
#  define MUNIT_SIZE_MODIFIER "z"
#  define MUNIT_CHAR_MODIFIER "hh"
#  define MUNIT_SHORT_MODIFIER "h"
#else
#  if defined(_M_X64) || defined(__amd64__)
#    define MUNIT_SIZE_MODIFIER "I64"
#  else
#    define MUNIT_SIZE_MODIFIER ""
#  endif
#  define MUNIT_CHAR_MODIFIER ""
#  define MUNIT_SHORT_MODIFIER ""
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define MUNIT_NO_RETURN _Noreturn
#elif defined(__GNUC__)
#  define MUNIT_NO_RETURN __attribute__((__noreturn__))
#elif defined(_MSC_VER)
#  define MUNIT_NO_RETURN __declspec(noreturn)
#else
#  define MUNIT_NO_RETURN
#endif

#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#  define MUNIT_PUSH_DISABLE_MSVC_C4127_                                       \
    __pragma(warning(push)) __pragma(warning(disable : 4127))
#  define MUNIT_POP_DISABLE_MSVC_C4127_ __pragma(warning(pop))
#else
#  define MUNIT_PUSH_DISABLE_MSVC_C4127_
#  define MUNIT_POP_DISABLE_MSVC_C4127_
#endif

typedef enum {
  MUNIT_LOG_DEBUG,
  MUNIT_LOG_INFO,
  MUNIT_LOG_WARNING,
  MUNIT_LOG_ERROR
} MunitLogLevel;

#if defined(__GNUC__) && !defined(__MINGW32__)
#  define MUNIT_PRINTF(string_index, first_to_check)                           \
    __attribute__((format(printf, string_index, first_to_check)))
#else
#  define MUNIT_PRINTF(string_index, first_to_check)
#endif

MUNIT_PRINTF(4, 5)
void munit_logf_ex(MunitLogLevel level, const char *filename, int line,
                   const char *format, ...);

#define munit_logf(level, format, ...)                                         \
  munit_logf_ex(level, __FILE__, __LINE__, format, __VA_ARGS__)

#define munit_log(level, msg) munit_logf(level, "%s", msg)

MUNIT_NO_RETURN
MUNIT_PRINTF(3, 4)
void munit_errorf_ex(const char *filename, int line, const char *format, ...);

#define munit_errorf(format, ...)                                              \
  munit_errorf_ex(__FILE__, __LINE__, format, __VA_ARGS__)

#define munit_error(msg) munit_errorf("%s", msg)

#define munit_assert(expr)                                                     \
  do {                                                                         \
    if (!MUNIT_LIKELY(expr)) {                                                 \
      munit_error("assertion failed: " #expr);                                 \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_true(expr)                                                \
  do {                                                                         \
    if (!MUNIT_LIKELY(expr)) {                                                 \
      munit_error("assertion failed: " #expr " is not true");                  \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_false(expr)                                               \
  do {                                                                         \
    if (!MUNIT_LIKELY(!(expr))) {                                              \
      munit_error("assertion failed: " #expr " is not false");                 \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_type_full(prefix, suffix, T, fmt, a, op, b)               \
  do {                                                                         \
    T munit_tmp_a_ = (a);                                                      \
    T munit_tmp_b_ = (b);                                                      \
    if (!(munit_tmp_a_ op munit_tmp_b_)) {                                     \
      munit_errorf("assertion failed: %s %s %s (" prefix "%" fmt suffix        \
                   " %s " prefix "%" fmt suffix ")",                           \
                   #a, #op, #b, munit_tmp_a_, #op, munit_tmp_b_);              \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_type(T, fmt, a, op, b)                                    \
  munit_assert_type_full("", "", T, fmt, a, op, b)

#define munit_assert_char(a, op, b)                                            \
  munit_assert_type_full("'\\x", "'", char, "02" MUNIT_CHAR_MODIFIER "x", a,   \
                         op, b)
#define munit_assert_uchar(a, op, b)                                           \
  munit_assert_type_full("'\\x", "'", unsigned char,                           \
                         "02" MUNIT_CHAR_MODIFIER "x", a, op, b)
#define munit_assert_short(a, op, b)                                           \
  munit_assert_type(short, MUNIT_SHORT_MODIFIER "d", a, op, b)
#define munit_assert_ushort(a, op, b)                                          \
  munit_assert_type(unsigned short, MUNIT_SHORT_MODIFIER "u", a, op, b)
#define munit_assert_int(a, op, b) munit_assert_type(int, "d", a, op, b)
#define munit_assert_uint(a, op, b)                                            \
  munit_assert_type(unsigned int, "u", a, op, b)
#define munit_assert_long(a, op, b) munit_assert_type(long int, "ld", a, op, b)
#define munit_assert_ulong(a, op, b)                                           \
  munit_assert_type(unsigned long int, "lu", a, op, b)
#define munit_assert_llong(a, op, b)                                           \
  munit_assert_type(long long int, "lld", a, op, b)
#define munit_assert_ullong(a, op, b)                                          \
  munit_assert_type(unsigned long long int, "llu", a, op, b)

#define munit_assert_size(a, op, b)                                            \
  munit_assert_type(size_t, MUNIT_SIZE_MODIFIER "u", a, op, b)
#define munit_assert_ssize(a, op, b)                                           \
  munit_assert_type(ssize_t, MUNIT_SIZE_MODIFIER "d", a, op, b)

#define munit_assert_float(a, op, b) munit_assert_type(float, "f", a, op, b)
#define munit_assert_double(a, op, b) munit_assert_type(double, "g", a, op, b)
#define munit_assert_ptr(a, op, b)                                             \
  munit_assert_type(const void *, "p", a, op, b)

#define munit_assert_int8(a, op, b)                                            \
  munit_assert_type(munit_int8_t, PRIi8, a, op, b)
#define munit_assert_uint8(a, op, b)                                           \
  munit_assert_type(munit_uint8_t, PRIu8, a, op, b)
#define munit_assert_int16(a, op, b)                                           \
  munit_assert_type(munit_int16_t, PRIi16, a, op, b)
#define munit_assert_uint16(a, op, b)                                          \
  munit_assert_type(munit_uint16_t, PRIu16, a, op, b)
#define munit_assert_int32(a, op, b)                                           \
  munit_assert_type(munit_int32_t, PRIi32, a, op, b)
#define munit_assert_uint32(a, op, b)                                          \
  munit_assert_type(munit_uint32_t, PRIu32, a, op, b)
#define munit_assert_int64(a, op, b)                                           \
  munit_assert_type(munit_int64_t, PRIi64, a, op, b)
#define munit_assert_uint64(a, op, b)                                          \
  munit_assert_type(munit_uint64_t, PRIu64, a, op, b)

#define munit_assert_ptrdiff(a, op, b)                                         \
  munit_assert_type(ptrdiff_t, "td", a, op, b)

#define munit_assert_enum(T, a, op, b) munit_assert_type(T, "d", a, op, b)

#define munit_assert_double_equal(a, b, precision)                             \
  do {                                                                         \
    const double munit_tmp_a_ = (a);                                           \
    const double munit_tmp_b_ = (b);                                           \
    const double munit_tmp_diff_ = ((munit_tmp_a_ - munit_tmp_b_) < 0)         \
                                     ? -(munit_tmp_a_ - munit_tmp_b_)          \
                                     : (munit_tmp_a_ - munit_tmp_b_);          \
    if (MUNIT_UNLIKELY(munit_tmp_diff_ > 1e-##precision)) {                    \
      munit_errorf("assertion failed: %s == %s (%0." #precision                \
                   "g == %0." #precision "g)",                                 \
                   #a, #b, munit_tmp_a_, munit_tmp_b_);                        \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#include <string.h>
#define munit_assert_string_equal(a, b)                                        \
  do {                                                                         \
    const char *munit_tmp_a_ = (a);                                            \
    const char *munit_tmp_b_ = (b);                                            \
    if (MUNIT_UNLIKELY(strcmp(munit_tmp_a_, munit_tmp_b_) != 0)) {             \
      munit_hexdump_diff(stderr, munit_tmp_a_, strlen(munit_tmp_a_),           \
                         munit_tmp_b_, strlen(munit_tmp_b_));                  \
      munit_errorf("assertion failed: string %s == %s (\"%s\" == \"%s\")", #a, \
                   #b, munit_tmp_a_, munit_tmp_b_);                            \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_string_not_equal(a, b)                                    \
  do {                                                                         \
    const char *munit_tmp_a_ = (a);                                            \
    const char *munit_tmp_b_ = (b);                                            \
    if (MUNIT_UNLIKELY(strcmp(munit_tmp_a_, munit_tmp_b_) == 0)) {             \
      munit_errorf("assertion failed: string %s != %s (\"%s\" == \"%s\")", #a, \
                   #b, munit_tmp_a_, munit_tmp_b_);                            \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_memory_equal(size, a, b)                                  \
  do {                                                                         \
    const unsigned char *munit_tmp_a_ = (const unsigned char *)(a);            \
    const unsigned char *munit_tmp_b_ = (const unsigned char *)(b);            \
    const size_t munit_tmp_size_ = (size);                                     \
    if (MUNIT_UNLIKELY(memcmp(munit_tmp_a_, munit_tmp_b_, munit_tmp_size_)) != \
        0) {                                                                   \
      size_t munit_tmp_pos_;                                                   \
      for (munit_tmp_pos_ = 0; munit_tmp_pos_ < munit_tmp_size_;               \
           munit_tmp_pos_++) {                                                 \
        if (munit_tmp_a_[munit_tmp_pos_] != munit_tmp_b_[munit_tmp_pos_]) {    \
          munit_hexdump_diff(stderr, munit_tmp_a_, size, munit_tmp_b_, size);  \
          munit_errorf("assertion failed: memory %s == %s, at offset "         \
                       "%" MUNIT_SIZE_MODIFIER "u",                            \
                       #a, #b, munit_tmp_pos_);                                \
          break;                                                               \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_memn_equal(a, a_size, b, b_size)                          \
  do {                                                                         \
    const unsigned char *munit_tmp_a_ = (const unsigned char *)(a);            \
    const unsigned char *munit_tmp_b_ = (const unsigned char *)(b);            \
    const size_t munit_tmp_a_size_ = (a_size);                                 \
    const size_t munit_tmp_b_size_ = (b_size);                                 \
    if (MUNIT_UNLIKELY(munit_tmp_a_size_ != munit_tmp_b_size_) ||              \
        MUNIT_UNLIKELY(munit_tmp_a_size_ && memcmp(munit_tmp_a_, munit_tmp_b_, \
                                                   munit_tmp_a_size_)) != 0) { \
      munit_hexdump_diff(stderr, munit_tmp_a_, munit_tmp_a_size_,              \
                         munit_tmp_b_, munit_tmp_b_size_);                     \
      munit_errorf("assertion failed: memory %s == %s", #a, #b);               \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_memory_not_equal(size, a, b)                              \
  do {                                                                         \
    const unsigned char *munit_tmp_a_ = (const unsigned char *)(a);            \
    const unsigned char *munit_tmp_b_ = (const unsigned char *)(b);            \
    const size_t munit_tmp_size_ = (size);                                     \
    if (MUNIT_UNLIKELY(memcmp(munit_tmp_a_, munit_tmp_b_, munit_tmp_size_)) == \
        0) {                                                                   \
      munit_errorf("assertion failed: memory %s != %s (%zu bytes)", #a, #b,    \
                   munit_tmp_size_);                                           \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#define munit_assert_ptr_equal(a, b) munit_assert_ptr(a, ==, b)
#define munit_assert_ptr_not_equal(a, b) munit_assert_ptr(a, !=, b)
#define munit_assert_null(ptr) munit_assert_ptr(ptr, ==, NULL)
#define munit_assert_not_null(ptr) munit_assert_ptr(ptr, !=, NULL)
#define munit_assert_ptr_null(ptr) munit_assert_ptr(ptr, ==, NULL)
#define munit_assert_ptr_not_null(ptr) munit_assert_ptr(ptr, !=, NULL)

/*** Memory allocation ***/

void *munit_malloc_ex(const char *filename, int line, size_t size);

#define munit_malloc(size) munit_malloc_ex(__FILE__, __LINE__, (size))

#define munit_new(type) ((type *)munit_malloc(sizeof(type)))

#define munit_calloc(nmemb, size) munit_malloc((nmemb) * (size))

#define munit_newa(type, nmemb) ((type *)munit_calloc((nmemb), sizeof(type)))

/*** Random number generation ***/

void munit_rand_seed(munit_uint32_t seed);
munit_uint32_t munit_rand_uint32(void);
int munit_rand_int_range(int min, int max);
double munit_rand_double(void);
void munit_rand_memory(size_t size, munit_uint8_t *buffer);

/*** Tests and Suites ***/

typedef enum {
  /* Test successful */
  MUNIT_OK,
  /* Test failed */
  MUNIT_FAIL,
  /* Test was skipped */
  MUNIT_SKIP,
  /* Test failed due to circumstances not intended to be tested
   * (things like network errors, invalid parameter value, failure to
   * allocate memory in the test harness, etc.). */
  MUNIT_ERROR
} MunitResult;

typedef struct {
  char *name;
  char **values;
} MunitParameterEnum;

typedef struct {
  char *name;
  char *value;
} MunitParameter;

const char *munit_parameters_get(const MunitParameter params[],
                                 const char *key);

typedef enum {
  MUNIT_TEST_OPTION_NONE = 0,
  MUNIT_TEST_OPTION_SINGLE_ITERATION = 1 << 0,
  MUNIT_TEST_OPTION_TODO = 1 << 1
} MunitTestOptions;

typedef MunitResult (*MunitTestFunc)(const MunitParameter params[],
                                     void *user_data_or_fixture);
typedef void *(*MunitTestSetup)(const MunitParameter params[], void *user_data);
typedef void (*MunitTestTearDown)(void *fixture);

typedef struct {
  const char *name;
  MunitTestFunc test;
  MunitTestSetup setup;
  MunitTestTearDown tear_down;
  MunitTestOptions options;
  MunitParameterEnum *parameters;
} MunitTest;

typedef enum { MUNIT_SUITE_OPTION_NONE = 0 } MunitSuiteOptions;

typedef struct MunitSuite_ MunitSuite;

struct MunitSuite_ {
  const char *prefix;
  const MunitTest *tests;
  const MunitSuite *suites;
  unsigned int iterations;
  MunitSuiteOptions options;
};

int munit_suite_main(const MunitSuite *suite, void *user_data, int argc,
                     char *const *argv);

/* Note: I'm not very happy with this API; it's likely to change if I
 * figure out something better.  Suggestions welcome. */

typedef struct MunitArgument_ MunitArgument;

struct MunitArgument_ {
  char *name;
  munit_bool (*parse_argument)(const MunitSuite *suite, void *user_data,
                               int *arg, int argc, char *const *argv);
  void (*write_help)(const MunitArgument *argument, void *user_data);
};

int munit_suite_main_custom(const MunitSuite *suite, void *user_data, int argc,
                            char *const *argv, const MunitArgument arguments[]);

#if defined(MUNIT_ENABLE_ASSERT_ALIASES)

#  define assert_true(expr) munit_assert_true(expr)
#  define assert_false(expr) munit_assert_false(expr)
#  define assert_char(a, op, b) munit_assert_char(a, op, b)
#  define assert_uchar(a, op, b) munit_assert_uchar(a, op, b)
#  define assert_short(a, op, b) munit_assert_short(a, op, b)
#  define assert_ushort(a, op, b) munit_assert_ushort(a, op, b)
#  define assert_int(a, op, b) munit_assert_int(a, op, b)
#  define assert_uint(a, op, b) munit_assert_uint(a, op, b)
#  define assert_long(a, op, b) munit_assert_long(a, op, b)
#  define assert_ulong(a, op, b) munit_assert_ulong(a, op, b)
#  define assert_llong(a, op, b) munit_assert_llong(a, op, b)
#  define assert_ullong(a, op, b) munit_assert_ullong(a, op, b)
#  define assert_size(a, op, b) munit_assert_size(a, op, b)
#  define assert_ssize(a, op, b) munit_assert_ssize(a, op, b)
#  define assert_float(a, op, b) munit_assert_float(a, op, b)
#  define assert_double(a, op, b) munit_assert_double(a, op, b)
#  define assert_ptr(a, op, b) munit_assert_ptr(a, op, b)

#  define assert_int8(a, op, b) munit_assert_int8(a, op, b)
#  define assert_uint8(a, op, b) munit_assert_uint8(a, op, b)
#  define assert_int16(a, op, b) munit_assert_int16(a, op, b)
#  define assert_uint16(a, op, b) munit_assert_uint16(a, op, b)
#  define assert_int32(a, op, b) munit_assert_int32(a, op, b)
#  define assert_uint32(a, op, b) munit_assert_uint32(a, op, b)
#  define assert_int64(a, op, b) munit_assert_int64(a, op, b)
#  define assert_uint64(a, op, b) munit_assert_uint64(a, op, b)

#  define assert_ptrdiff(a, op, b) munit_assert_ptrdiff(a, op, b)

#  define assert_enum(T, a, op, b) munit_assert_enum(T, a, op, b)

#  define assert_double_equal(a, b, precision)                                 \
    munit_assert_double_equal(a, b, precision)
#  define assert_string_equal(a, b) munit_assert_string_equal(a, b)
#  define assert_string_not_equal(a, b) munit_assert_string_not_equal(a, b)
#  define assert_memory_equal(size, a, b) munit_assert_memory_equal(size, a, b)
#  define assert_memn_equal(a, a_size, b, b_size)                              \
    munit_assert_memn_equal(a, a_size, b, b_size)
#  define assert_memory_not_equal(size, a, b)                                  \
    munit_assert_memory_not_equal(size, a, b)
#  define assert_ptr_equal(a, b) munit_assert_ptr_equal(a, b)
#  define assert_ptr_not_equal(a, b) munit_assert_ptr_not_equal(a, b)
#  define assert_ptr_null(ptr) munit_assert_null_equal(ptr)
#  define assert_ptr_not_null(ptr) munit_assert_not_null(ptr)

#  define assert_null(ptr) munit_assert_null(ptr)
#  define assert_not_null(ptr) munit_assert_not_null(ptr)

#endif /* defined(MUNIT_ENABLE_ASSERT_ALIASES) */

#define munit_void_test_decl(func)                                             \
  void func(void);                                                             \
                                                                               \
  static inline MunitResult wrap_##func(const MunitParameter params[],         \
                                        void *fixture) {                       \
    (void)params;                                                              \
    (void)fixture;                                                             \
                                                                               \
    func();                                                                    \
    return MUNIT_OK;                                                           \
  }

#define munit_void_test(func)                                                  \
  {"/" #func, wrap_##func, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}

#define munit_test_end() {NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL}

int munit_hexdump(FILE *fp, const void *data, size_t datalen);

int munit_hexdump_diff(FILE *fp, const void *a, size_t alen, const void *b,
                       size_t blen);

#if defined(__cplusplus)
}
#endif

#endif /* !defined(MUNIT_H) */

#if defined(MUNIT_ENABLE_ASSERT_ALIASES)
#if defined(assert)
#  undef assert
#endif
#define assert(expr) munit_assert(expr)
#endif

/* µnit Testing Framework
 * Copyright (c) 2013-2017 Evan Nemerson <evan@nemerson.com>
 * Copyright (c) 2023 munit contributors
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

#ifndef MUNITXX_H
#define MUNITXX_H

#include <munit.h>

#include <type_traits>
#include <string>
#if __cplusplus >= 201703L
#  include <string_view>
#endif // __cplusplus >= 201703L

#define munit_assert_stdstring_equal(a, b)                                     \
  do {                                                                         \
    const std::string munit_tmp_a_ = (a);                                      \
    const std::string munit_tmp_b_ = (b);                                      \
    if (MUNIT_UNLIKELY(munit_tmp_a_ != munit_tmp_b_)) {                        \
      munit_hexdump_diff(stderr, munit_tmp_a_.c_str(), munit_tmp_a_.size(),    \
                         munit_tmp_b_.c_str(), munit_tmp_b_.size());           \
      munit_errorf("assertion failed: string %s == %s (\"%s\" == \"%s\")", #a, \
                   #b, munit_tmp_a_.c_str(), munit_tmp_b_.c_str());            \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#if __cplusplus >= 201703L
#  define munit_assert_stdsv_equal(a, b)                                       \
    do {                                                                       \
      const std::string_view munit_tmp_a_ = (a);                               \
      const std::string_view munit_tmp_b_ = (b);                               \
      if (MUNIT_UNLIKELY(munit_tmp_a_ != munit_tmp_b_)) {                      \
        munit_hexdump_diff(stderr, munit_tmp_a_.data(), munit_tmp_a_.size(),   \
                           munit_tmp_b_.data(), munit_tmp_b_.size());          \
        munit_errorf(                                                          \
          "assertion failed: string %s == %s (\"%.*s\" == \"%.*s\")", #a, #b,  \
          (int)munit_tmp_a_.size(), munit_tmp_a_.data(),                       \
          (int)munit_tmp_b_.size(), munit_tmp_b_.data());                      \
      }                                                                        \
      MUNIT_PUSH_DISABLE_MSVC_C4127_                                           \
    } while (0) MUNIT_POP_DISABLE_MSVC_C4127_
#endif // __cplusplus >= 201703L

#define munit_assert_enum_class(a, op, b)                                      \
  do {                                                                         \
    auto munit_tmp_a_ = (a);                                                   \
    auto munit_tmp_b_ = (b);                                                   \
    if (!(munit_tmp_a_ op munit_tmp_b_)) {                                     \
      auto munit_tmp_a_str_ = std::to_string(                                  \
        static_cast<std::underlying_type_t<decltype(munit_tmp_a_)>>(           \
          munit_tmp_a_));                                                      \
      auto munit_tmp_b_str_ = std::to_string(                                  \
        static_cast<std::underlying_type_t<decltype(munit_tmp_b_)>>(           \
          munit_tmp_b_));                                                      \
      munit_errorf("assertion failed: %s %s %s (%s %s %s)", #a, #op, #b,       \
                   munit_tmp_a_str_.c_str(), #op, munit_tmp_b_str_.c_str());   \
    }                                                                          \
    MUNIT_PUSH_DISABLE_MSVC_C4127_                                             \
  } while (0) MUNIT_POP_DISABLE_MSVC_C4127_

#if defined(MUNIT_ENABLE_ASSERT_ALIASES)

#  define assert_stdstring_equal(a, b) munit_assert_stdstring_equal(a, b)
#  if __cplusplus >= 201703L
#    define assert_stdsv_equal(a, b) munit_assert_stdsv_equal(a, b)
#  endif // __cplusplus >= 201703L
#  define assert_enum_class(a, op, b) munit_assert_enum_class(a, op, b)

#endif /* defined(MUNIT_ENABLE_ASSERT_ALIASES) */

#endif // MUNITXX_H

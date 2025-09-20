/*
 * ngtcp2
 *
 * Copyright (c) 2018 ngtcp2 contributors
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
#ifndef UTIL_TEST_H
#define UTIL_TEST_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif // defined(HAVE_CONFIG_H)

#define MUNIT_ENABLE_ASSERT_ALIASES

#include "munitxx.h"

namespace ngtcp2 {

extern const MunitSuite util_suite;

munit_void_test_decl(test_util_format_durationf)
munit_void_test_decl(test_util_format_uint)
munit_void_test_decl(test_util_format_uint_iec)
munit_void_test_decl(test_util_format_duration)
munit_void_test_decl(test_util_parse_uint)
munit_void_test_decl(test_util_parse_uint_iec)
munit_void_test_decl(test_util_parse_duration)
munit_void_test_decl(test_util_normalize_path)
munit_void_test_decl(test_util_hexdump)
munit_void_test_decl(test_util_format_hex)
munit_void_test_decl(test_util_decode_hex)
munit_void_test_decl(test_util_is_hex_string)

} // namespace ngtcp2

#endif // !defined(UTIL_TEST_H)

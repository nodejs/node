/*
 * ngtcp2
 *
 * Copyright (c) 2018 ngtcp2 contributors
 * Copyright (c) 2013 nghttp2 contributors
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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif // defined(HAVE_CONFIG_H)

#include "munit.h"

// include test cases' include files here
#include "util_test.h"
#include "siphash_test.h"

int main(int argc, char *argv[]) {
  const MunitSuite suites[] = {
    ngtcp2::util_suite,
    ngtcp2::siphash_suite,
    {},
  };
  const MunitSuite suite = {
    .prefix = "",
    .suites = suites,
    .iterations = 1,
  };

  return munit_suite_main(&suite, nullptr, argc, argv);
}

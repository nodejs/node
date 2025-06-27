// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/base/numbers/dtoa.h"

#include <stdlib.h>

#include "src/base/numbers/double.h"
#include "test/unittests/gay-fixed.h"
#include "test/unittests/gay-precision.h"
#include "test/unittests/gay-shortest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {

using DtoaTest = ::testing::Test;

namespace base {
namespace test_dtoa {

// Removes trailing '0' digits (modifies {representation}). Can create an empty
// string if all digits are 0.
static void TrimRepresentation(char* representation) {
  size_t len = strlen(representation);
  while (len > 0 && representation[len - 1] == '0') --len;
  representation[len] = '\0';
}

static const int kBufferSize = 100;

TEST_F(DtoaTest, DtoaVariousDoubles) {
  char buffer_container[kBufferSize];
  base::Vector<char> buffer(buffer_container, kBufferSize);
  int length;
  int point;
  int sign;

  DoubleToAscii(0.0, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("0", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(0.0, DTOA_FIXED, 2, buffer, &sign, &length, &point);
  CHECK_EQ(1, length);
  CHECK_EQ(0, strcmp("0", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(0.0, DTOA_PRECISION, 3, buffer, &sign, &length, &point);
  CHECK_EQ(1, length);
  CHECK_EQ(0, strcmp("0", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(1.0, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("1", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(1.0, DTOA_FIXED, 3, buffer, &sign, &length, &point);
  CHECK_GE(3, length - point);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("1", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(1.0, DTOA_PRECISION, 3, buffer, &sign, &length, &point);
  CHECK_GE(3, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("1", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(1.5, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("15", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(1.5, DTOA_FIXED, 10, buffer, &sign, &length, &point);
  CHECK_GE(10, length - point);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("15", buffer.begin()));
  CHECK_EQ(1, point);

  DoubleToAscii(1.5, DTOA_PRECISION, 10, buffer, &sign, &length, &point);
  CHECK_GE(10, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("15", buffer.begin()));
  CHECK_EQ(1, point);

  double min_double = 5e-324;
  DoubleToAscii(min_double, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("5", buffer.begin()));
  CHECK_EQ(-323, point);

  DoubleToAscii(min_double, DTOA_FIXED, 5, buffer, &sign, &length, &point);
  CHECK_GE(5, length - point);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("", buffer.begin()));
  CHECK_GE(-5, point);

  DoubleToAscii(min_double, DTOA_PRECISION, 5, buffer, &sign, &length, &point);
  CHECK_GE(5, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("49407", buffer.begin()));
  CHECK_EQ(-323, point);

  double max_double = 1.7976931348623157e308;
  DoubleToAscii(max_double, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("17976931348623157", buffer.begin()));
  CHECK_EQ(309, point);

  DoubleToAscii(max_double, DTOA_PRECISION, 7, buffer, &sign, &length, &point);
  CHECK_GE(7, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("1797693", buffer.begin()));
  CHECK_EQ(309, point);

  DoubleToAscii(4294967272.0, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("4294967272", buffer.begin()));
  CHECK_EQ(10, point);

  DoubleToAscii(4294967272.0, DTOA_FIXED, 5, buffer, &sign, &length, &point);
  CHECK_GE(5, length - point);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("4294967272", buffer.begin()));
  CHECK_EQ(10, point);

  DoubleToAscii(4294967272.0, DTOA_PRECISION, 14, buffer, &sign, &length,
                &point);
  CHECK_GE(14, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("4294967272", buffer.begin()));
  CHECK_EQ(10, point);

  DoubleToAscii(4.1855804968213567e298, DTOA_SHORTEST, 0, buffer, &sign,
                &length, &point);
  CHECK_EQ(0, strcmp("4185580496821357", buffer.begin()));
  CHECK_EQ(299, point);

  DoubleToAscii(4.1855804968213567e298, DTOA_PRECISION, 20, buffer, &sign,
                &length, &point);
  CHECK_GE(20, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("41855804968213567225", buffer.begin()));
  CHECK_EQ(299, point);

  DoubleToAscii(5.5626846462680035e-309, DTOA_SHORTEST, 0, buffer, &sign,
                &length, &point);
  CHECK_EQ(0, strcmp("5562684646268003", buffer.begin()));
  CHECK_EQ(-308, point);

  DoubleToAscii(5.5626846462680035e-309, DTOA_PRECISION, 1, buffer, &sign,
                &length, &point);
  CHECK_GE(1, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("6", buffer.begin()));
  CHECK_EQ(-308, point);

  DoubleToAscii(-2147483648.0, DTOA_SHORTEST, 0, buffer, &sign, &length,
                &point);
  CHECK_EQ(1, sign);
  CHECK_EQ(0, strcmp("2147483648", buffer.begin()));
  CHECK_EQ(10, point);

  DoubleToAscii(-2147483648.0, DTOA_FIXED, 2, buffer, &sign, &length, &point);
  CHECK_GE(2, length - point);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(1, sign);
  CHECK_EQ(0, strcmp("2147483648", buffer.begin()));
  CHECK_EQ(10, point);

  DoubleToAscii(-2147483648.0, DTOA_PRECISION, 5, buffer, &sign, &length,
                &point);
  CHECK_GE(5, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(1, sign);
  CHECK_EQ(0, strcmp("21475", buffer.begin()));
  CHECK_EQ(10, point);

  DoubleToAscii(-3.5844466002796428e+298, DTOA_SHORTEST, 0, buffer, &sign,
                &length, &point);
  CHECK_EQ(1, sign);
  CHECK_EQ(0, strcmp("35844466002796428", buffer.begin()));
  CHECK_EQ(299, point);

  DoubleToAscii(-3.5844466002796428e+298, DTOA_PRECISION, 10, buffer, &sign,
                &length, &point);
  CHECK_EQ(1, sign);
  CHECK_GE(10, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("35844466", buffer.begin()));
  CHECK_EQ(299, point);

  uint64_t smallest_normal64 = 0x0010'0000'0000'0000;
  double v = Double(smallest_normal64).value();
  DoubleToAscii(v, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("22250738585072014", buffer.begin()));
  CHECK_EQ(-307, point);

  DoubleToAscii(v, DTOA_PRECISION, 20, buffer, &sign, &length, &point);
  CHECK_GE(20, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("22250738585072013831", buffer.begin()));
  CHECK_EQ(-307, point);

  uint64_t largest_denormal64 = 0x000F'FFFF'FFFF'FFFF;
  v = Double(largest_denormal64).value();
  DoubleToAscii(v, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("2225073858507201", buffer.begin()));
  CHECK_EQ(-307, point);

  DoubleToAscii(v, DTOA_PRECISION, 20, buffer, &sign, &length, &point);
  CHECK_GE(20, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("2225073858507200889", buffer.begin()));
  CHECK_EQ(-307, point);

  DoubleToAscii(4128420500802942e-24, DTOA_SHORTEST, 0, buffer, &sign, &length,
                &point);
  CHECK_EQ(0, sign);
  CHECK_EQ(0, strcmp("4128420500802942", buffer.begin()));
  CHECK_EQ(-8, point);

  v = -3.9292015898194142585311918e-10;
  DoubleToAscii(v, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
  CHECK_EQ(0, strcmp("39292015898194143", buffer.begin()));

  v = 4194304.0;
  DoubleToAscii(v, DTOA_FIXED, 5, buffer, &sign, &length, &point);
  CHECK_GE(5, length - point);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("4194304", buffer.begin()));

  v = 3.3161339052167390562200598e-237;
  DoubleToAscii(v, DTOA_PRECISION, 19, buffer, &sign, &length, &point);
  CHECK_GE(19, length);
  TrimRepresentation(buffer.begin());
  CHECK_EQ(0, strcmp("3316133905216739056", buffer.begin()));
  CHECK_EQ(-236, point);
}

TEST_F(DtoaTest, DtoaGayShortest) {
  char buffer_container[kBufferSize];
  base::Vector<char> buffer(buffer_container, kBufferSize);
  int sign;
  int length;
  int point;

  base::Vector<const PrecomputedShortest> precomputed =
      PrecomputedShortestRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedShortest current_test = precomputed[i];
    double v = current_test.v;
    DoubleToAscii(v, DTOA_SHORTEST, 0, buffer, &sign, &length, &point);
    CHECK_EQ(0, sign);  // All precomputed numbers are positive.
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_EQ(0, strcmp(current_test.representation, buffer.begin()));
  }
}

TEST_F(DtoaTest, DtoaGayFixed) {
  char buffer_container[kBufferSize];
  base::Vector<char> buffer(buffer_container, kBufferSize);
  int sign;
  int length;
  int point;

  base::Vector<const PrecomputedFixed> precomputed =
      PrecomputedFixedRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedFixed current_test = precomputed[i];
    double v = current_test.v;
    int number_digits = current_test.number_digits;
    DoubleToAscii(v, DTOA_FIXED, number_digits, buffer, &sign, &length, &point);
    CHECK_EQ(0, sign);  // All precomputed numbers are positive.
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_GE(number_digits, length - point);
    TrimRepresentation(buffer.begin());
    CHECK_EQ(0, strcmp(current_test.representation, buffer.begin()));
  }
}

TEST_F(DtoaTest, DtoaGayPrecision) {
  char buffer_container[kBufferSize];
  base::Vector<char> buffer(buffer_container, kBufferSize);
  int sign;
  int length;
  int point;

  base::Vector<const PrecomputedPrecision> precomputed =
      PrecomputedPrecisionRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedPrecision current_test = precomputed[i];
    double v = current_test.v;
    int number_digits = current_test.number_digits;
    DoubleToAscii(v, DTOA_PRECISION, number_digits, buffer, &sign, &length,
                  &point);
    CHECK_EQ(0, sign);  // All precomputed numbers are positive.
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_GE(number_digits, length);
    TrimRepresentation(buffer.begin());
    CHECK_EQ(0, strcmp(current_test.representation, buffer.begin()));
  }
}

}  // namespace test_dtoa
}  // namespace base
}  // namespace v8

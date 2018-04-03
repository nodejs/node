// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include <stdlib.h>

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/diy-fp.h"
#include "src/double.h"
#include "src/fast-dtoa.h"
#include "test/cctest/cctest.h"
#include "test/cctest/gay-precision.h"
#include "test/cctest/gay-shortest.h"

namespace v8 {
namespace internal {
namespace test_fast_dtoa {

static const int kBufferSize = 100;


// Removes trailing '0' digits.
static void TrimRepresentation(Vector<char> representation) {
  int len = StrLength(representation.start());
  int i;
  for (i = len - 1; i >= 0; --i) {
    if (representation[i] != '0') break;
  }
  representation[i + 1] = '\0';
}


TEST(FastDtoaShortestVariousDoubles) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  int length;
  int point;
  int status;

  double min_double = 5e-324;
  status = FastDtoa(min_double, FAST_DTOA_SHORTEST, 0,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("5", buffer.start()));
  CHECK_EQ(-323, point);

  double max_double = 1.7976931348623157e308;
  status = FastDtoa(max_double, FAST_DTOA_SHORTEST, 0,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("17976931348623157", buffer.start()));
  CHECK_EQ(309, point);

  status = FastDtoa(4294967272.0, FAST_DTOA_SHORTEST, 0,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("4294967272", buffer.start()));
  CHECK_EQ(10, point);

  status = FastDtoa(4.1855804968213567e298, FAST_DTOA_SHORTEST, 0,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("4185580496821357", buffer.start()));
  CHECK_EQ(299, point);

  status = FastDtoa(5.5626846462680035e-309, FAST_DTOA_SHORTEST, 0,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("5562684646268003", buffer.start()));
  CHECK_EQ(-308, point);

  status = FastDtoa(2147483648.0, FAST_DTOA_SHORTEST, 0,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("2147483648", buffer.start()));
  CHECK_EQ(10, point);

  status = FastDtoa(3.5844466002796428e+298, FAST_DTOA_SHORTEST, 0,
                    buffer, &length, &point);
  if (status) {  // Not all FastDtoa variants manage to compute this number.
    CHECK_EQ(0, strcmp("35844466002796428", buffer.start()));
    CHECK_EQ(299, point);
  }

  uint64_t smallest_normal64 = V8_2PART_UINT64_C(0x00100000, 00000000);
  double v = Double(smallest_normal64).value();
  status = FastDtoa(v, FAST_DTOA_SHORTEST, 0, buffer, &length, &point);
  if (status) {
    CHECK_EQ(0, strcmp("22250738585072014", buffer.start()));
    CHECK_EQ(-307, point);
  }

  uint64_t largest_denormal64 = V8_2PART_UINT64_C(0x000FFFFF, FFFFFFFF);
  v = Double(largest_denormal64).value();
  status = FastDtoa(v, FAST_DTOA_SHORTEST, 0, buffer, &length, &point);
  if (status) {
    CHECK_EQ(0, strcmp("2225073858507201", buffer.start()));
    CHECK_EQ(-307, point);
  }
}


TEST(FastDtoaPrecisionVariousDoubles) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  int length;
  int point;
  int status;

  status = FastDtoa(1.0, FAST_DTOA_PRECISION, 3, buffer, &length, &point);
  CHECK(status);
  CHECK_GE(3, length);
  TrimRepresentation(buffer);
  CHECK_EQ(0, strcmp("1", buffer.start()));
  CHECK_EQ(1, point);

  status = FastDtoa(1.5, FAST_DTOA_PRECISION, 10, buffer, &length, &point);
  if (status) {
    CHECK_GE(10, length);
    TrimRepresentation(buffer);
    CHECK_EQ(0, strcmp("15", buffer.start()));
    CHECK_EQ(1, point);
  }

  double min_double = 5e-324;
  status = FastDtoa(min_double, FAST_DTOA_PRECISION, 5,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("49407", buffer.start()));
  CHECK_EQ(-323, point);

  double max_double = 1.7976931348623157e308;
  status = FastDtoa(max_double, FAST_DTOA_PRECISION, 7,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("1797693", buffer.start()));
  CHECK_EQ(309, point);

  status = FastDtoa(4294967272.0, FAST_DTOA_PRECISION, 14,
                    buffer, &length, &point);
  if (status) {
    CHECK_GE(14, length);
    TrimRepresentation(buffer);
    CHECK_EQ(0, strcmp("4294967272", buffer.start()));
    CHECK_EQ(10, point);
  }

  status = FastDtoa(4.1855804968213567e298, FAST_DTOA_PRECISION, 17,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("41855804968213567", buffer.start()));
  CHECK_EQ(299, point);

  status = FastDtoa(5.5626846462680035e-309, FAST_DTOA_PRECISION, 1,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("6", buffer.start()));
  CHECK_EQ(-308, point);

  status = FastDtoa(2147483648.0, FAST_DTOA_PRECISION, 5,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("21475", buffer.start()));
  CHECK_EQ(10, point);

  status = FastDtoa(3.5844466002796428e+298, FAST_DTOA_PRECISION, 10,
                    buffer, &length, &point);
  CHECK(status);
  CHECK_GE(10, length);
  TrimRepresentation(buffer);
  CHECK_EQ(0, strcmp("35844466", buffer.start()));
  CHECK_EQ(299, point);

  uint64_t smallest_normal64 = V8_2PART_UINT64_C(0x00100000, 00000000);
  double v = Double(smallest_normal64).value();
  status = FastDtoa(v, FAST_DTOA_PRECISION, 17, buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("22250738585072014", buffer.start()));
  CHECK_EQ(-307, point);

  uint64_t largest_denormal64 = V8_2PART_UINT64_C(0x000FFFFF, FFFFFFFF);
  v = Double(largest_denormal64).value();
  status = FastDtoa(v, FAST_DTOA_PRECISION, 17, buffer, &length, &point);
  CHECK(status);
  CHECK_GE(20, length);
  TrimRepresentation(buffer);
  CHECK_EQ(0, strcmp("22250738585072009", buffer.start()));
  CHECK_EQ(-307, point);

  v = 3.3161339052167390562200598e-237;
  status = FastDtoa(v, FAST_DTOA_PRECISION, 18, buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("331613390521673906", buffer.start()));
  CHECK_EQ(-236, point);

  v = 7.9885183916008099497815232e+191;
  status = FastDtoa(v, FAST_DTOA_PRECISION, 4, buffer, &length, &point);
  CHECK(status);
  CHECK_EQ(0, strcmp("7989", buffer.start()));
  CHECK_EQ(192, point);
}


TEST(FastDtoaGayShortest) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  bool status;
  int length;
  int point;
  int succeeded = 0;
  int total = 0;
  bool needed_max_length = false;

  Vector<const PrecomputedShortest> precomputed =
      PrecomputedShortestRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedShortest current_test = precomputed[i];
    total++;
    double v = current_test.v;
    status = FastDtoa(v, FAST_DTOA_SHORTEST, 0, buffer, &length, &point);
    CHECK_GE(kFastDtoaMaximalLength, length);
    if (!status) continue;
    if (length == kFastDtoaMaximalLength) needed_max_length = true;
    succeeded++;
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_EQ(0, strcmp(current_test.representation, buffer.start()));
  }
  CHECK_GT(succeeded*1.0/total, 0.99);
  CHECK(needed_max_length);
}


TEST(FastDtoaGayPrecision) {
  char buffer_container[kBufferSize];
  Vector<char> buffer(buffer_container, kBufferSize);
  bool status;
  int length;
  int point;
  int succeeded = 0;
  int total = 0;
  // Count separately for entries with less than 15 requested digits.
  int succeeded_15 = 0;
  int total_15 = 0;

  Vector<const PrecomputedPrecision> precomputed =
      PrecomputedPrecisionRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const PrecomputedPrecision current_test = precomputed[i];
    double v = current_test.v;
    int number_digits = current_test.number_digits;
    total++;
    if (number_digits <= 15) total_15++;
    status = FastDtoa(v, FAST_DTOA_PRECISION, number_digits,
                      buffer, &length, &point);
    CHECK_GE(number_digits, length);
    if (!status) continue;
    succeeded++;
    if (number_digits <= 15) succeeded_15++;
    TrimRepresentation(buffer);
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_EQ(0, strcmp(current_test.representation, buffer.start()));
  }
  // The precomputed numbers contain many entries with many requested
  // digits. These have a high failure rate and we therefore expect a lower
  // success rate than for the shortest representation.
  CHECK_GT(succeeded*1.0/total, 0.85);
  // However with less than 15 digits almost the algorithm should almost always
  // succeed.
  CHECK_GT(succeeded_15*1.0/total_15, 0.9999);
}

}  // namespace test_fast_dtoa
}  // namespace internal
}  // namespace v8

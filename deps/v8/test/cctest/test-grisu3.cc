// Copyright 2006-2008 the V8 project authors. All rights reserved.

#include <stdlib.h>

#include "v8.h"

#include "platform.h"
#include "cctest.h"
#include "diy_fp.h"
#include "double.h"
#include "gay_shortest.h"
#include "grisu3.h"

using namespace v8::internal;

static const int kBufferSize = 100;

TEST(GrisuVariousDoubles) {
  char buffer[kBufferSize];
  int sign;
  int length;
  int point;
  int status;

  double min_double = 5e-324;
  status = grisu3(min_double, buffer, &sign, &length, &point);
  CHECK(status);
  CHECK_EQ(0, sign);
  CHECK_EQ("5", buffer);
  CHECK_EQ(-323, point);

  double max_double = 1.7976931348623157e308;
  status = grisu3(max_double, buffer, &sign, &length, &point);
  CHECK(status);
  CHECK_EQ(0, sign);
  CHECK_EQ("17976931348623157", buffer);
  CHECK_EQ(309, point);

  status = grisu3(4294967272.0, buffer, &sign, &length, &point);
  CHECK(status);
  CHECK_EQ(0, sign);
  CHECK_EQ("4294967272", buffer);
  CHECK_EQ(10, point);

  status = grisu3(4.1855804968213567e298, buffer, &sign, &length, &point);
  CHECK(status);
  CHECK_EQ(0, sign);
  CHECK_EQ("4185580496821357", buffer);
  CHECK_EQ(299, point);

  status = grisu3(5.5626846462680035e-309, buffer, &sign, &length, &point);
  CHECK(status);
  CHECK_EQ(0, sign);
  CHECK_EQ("5562684646268003", buffer);
  CHECK_EQ(-308, point);

  status = grisu3(2147483648.0, buffer, &sign, &length, &point);
  CHECK(status);
  CHECK_EQ(0, sign);
  CHECK_EQ("2147483648", buffer);
  CHECK_EQ(10, point);

  status = grisu3(3.5844466002796428e+298, buffer, &sign, &length, &point);
  if (status) {  // Not all grisu3 variants manage to compute this number.
    CHECK_EQ("35844466002796428", buffer);
    CHECK_EQ(0, sign);
    CHECK_EQ(299, point);
  }

  uint64_t smallest_normal64 = V8_2PART_UINT64_C(0x00100000, 00000000);
  double v = Double(smallest_normal64).value();
  status = grisu3(v, buffer, &sign, &length, &point);
  if (status) {
    CHECK_EQ(0, sign);
    CHECK_EQ("22250738585072014", buffer);
    CHECK_EQ(-307, point);
  }

  uint64_t largest_denormal64 = V8_2PART_UINT64_C(0x000FFFFF, FFFFFFFF);
  v = Double(largest_denormal64).value();
  status = grisu3(v, buffer, &sign, &length, &point);
  if (status) {
    CHECK_EQ(0, sign);
    CHECK_EQ("2225073858507201", buffer);
    CHECK_EQ(-307, point);
  }
}


TEST(GrisuGayShortest) {
  char buffer[kBufferSize];
  bool status;
  int sign;
  int length;
  int point;
  int succeeded = 0;
  int total = 0;
  bool needed_max_length = false;

  Vector<const GayShortest> precomputed = PrecomputedShortestRepresentations();
  for (int i = 0; i < precomputed.length(); ++i) {
    const GayShortest current_test = precomputed[i];
    total++;
    double v = current_test.v;
    status = grisu3(v, buffer, &sign, &length, &point);
    CHECK_GE(kGrisu3MaximalLength, length);
    if (!status) continue;
    if (length == kGrisu3MaximalLength) needed_max_length = true;
    succeeded++;
    CHECK_EQ(0, sign);  // All precomputed numbers are positive.
    CHECK_EQ(current_test.decimal_point, point);
    CHECK_EQ(current_test.representation, buffer);
  }
  CHECK_GT(succeeded*1.0/total, 0.99);
  CHECK(needed_max_length);
}

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/checks.h"

#include "test/cctest/cctest.h"


TEST(CheckEqualsZeroAndMinusZero) {
  CHECK_EQ(0.0, 0.0);
  CHECK_NE(0.0, -0.0);
  CHECK_NE(-0.0, 0.0);
  CHECK_EQ(-0.0, -0.0);
}


TEST(CheckEqualsReflexivity) {
  double inf = V8_INFINITY;
  double nan = v8::base::OS::nan_value();
  double constants[] = {-nan, -inf, -3.1415, -1.0,   -0.1, -0.0,
                        0.0,  0.1,  1.0,     3.1415, inf,  nan};
  for (size_t i = 0; i < arraysize(constants); ++i) {
    CHECK_EQ(constants[i], constants[i]);
  }
}

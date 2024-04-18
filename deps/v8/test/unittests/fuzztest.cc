// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Demo fuzz tests used to probe-test the FuzzTest and Centipede integration
// with Clusterfuzz.

#include "test/unittests/fuzztest.h"

#include <iostream>
#include <string>
#include <vector>

namespace v8::internal {

#ifdef V8_ENABLE_FUZZTEST

static void ManyConditions(std::vector<int> input) {
  int i = 0;

  if (input.size() > 4) {
    if (input[0] >= 1500 && input[0] <= 2000) {
      i++;
    }
    if (input[1] >= 250 && input[1] <= 500) {
      i++;
    }
    if (input[2] >= 200 && input[2] <= 250) {
      i++;
    }
    if (input[3] >= 150 && input[3] <= 200) {
      i++;
    }
    if (input[4] >= 3 && input[4] <= 20) {
      i++;
    }
  }

  // Fake Heap-buffer-overflow.
  if (i >= 5) {
    int* adr = new int(3);
    std::cout << *(adr + 3);
  }

  // Fake assert.
  ASSERT_LT(i, 4);
}

V8_FUZZ_TEST(SmokeTest, ManyConditions)
    .WithDomains(fuzztest::VectorOf(fuzztest::InRange(1, 2000)));

static void SingleString(std::string input) { ASSERT_NE(input, "V8"); }

V8_FUZZ_TEST(SmokeTest, SingleString);

#endif  // V8_ENABLE_FUZZTEST
}  // namespace v8::internal

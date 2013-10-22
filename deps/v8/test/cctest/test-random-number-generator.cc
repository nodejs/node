// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "cctest.h"
#include "utils/random-number-generator.h"

using namespace v8::internal;


static const int kMaxRuns = 12345;
static const int kRandomSeeds[] = {
  -1, 1, 42, 100, 1234567890, 987654321
};


TEST(NextIntWithMaxValue) {
  for (unsigned n = 0; n < ARRAY_SIZE(kRandomSeeds); ++n) {
    RandomNumberGenerator rng(kRandomSeeds[n]);
    for (int max = 1; max <= kMaxRuns; ++max) {
      int n = rng.NextInt(max);
      CHECK_LE(0, n);
      CHECK_LT(n, max);
    }
  }
}


TEST(NextBoolReturnsBooleanValue) {
  for (unsigned n = 0; n < ARRAY_SIZE(kRandomSeeds); ++n) {
    RandomNumberGenerator rng(kRandomSeeds[n]);
    for (int k = 0; k < kMaxRuns; ++k) {
      bool b = rng.NextBool();
      CHECK(b == false || b == true);
    }
  }
}


TEST(NextDoubleRange) {
  for (unsigned n = 0; n < ARRAY_SIZE(kRandomSeeds); ++n) {
    RandomNumberGenerator rng(kRandomSeeds[n]);
    for (int k = 0; k < kMaxRuns; ++k) {
      double d = rng.NextDouble();
      CHECK_LE(0.0, d);
      CHECK_LT(d, 1.0);
    }
  }
}


TEST(RandomSeedFlagIsUsed) {
  for (unsigned n = 0; n < ARRAY_SIZE(kRandomSeeds); ++n) {
    FLAG_random_seed = kRandomSeeds[n];
    RandomNumberGenerator rng1;
    RandomNumberGenerator rng2(kRandomSeeds[n]);
    for (int k = 1; k <= kMaxRuns; ++k) {
      int64_t i1, i2;
      rng1.NextBytes(&i1, sizeof(i1));
      rng2.NextBytes(&i2, sizeof(i2));
      CHECK_EQ(i2, i1);
      CHECK_EQ(rng2.NextInt(), rng1.NextInt());
      CHECK_EQ(rng2.NextInt(k), rng1.NextInt(k));
      CHECK_EQ(rng2.NextDouble(), rng1.NextDouble());
    }
  }
}

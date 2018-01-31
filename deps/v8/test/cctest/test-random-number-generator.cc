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

#include "src/flags.h"
#include "src/isolate.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/utils/random-number-generator.h"

namespace v8 {
namespace internal {

static const int64_t kRandomSeeds[] = {-1, 1, 42, 100, 1234567890, 987654321};


TEST(RandomSeedFlagIsUsed) {
  for (unsigned n = 0; n < arraysize(kRandomSeeds); ++n) {
    FLAG_random_seed = static_cast<int>(kRandomSeeds[n]);
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
    v8::Isolate* i = v8::Isolate::New(create_params);
    v8::base::RandomNumberGenerator& rng =
        *reinterpret_cast<Isolate*>(i)->random_number_generator();
    CHECK_EQ(kRandomSeeds[n], rng.initial_seed());
    i->Dispose();
  }
}


// Chi squared for getting m 0s out of n bits.
double ChiSquared(int m, int n) {
  double ys_minus_np1 = (m - n / 2.0);
  double chi_squared_1 = ys_minus_np1 * ys_minus_np1 * 2.0 / n;
  double ys_minus_np2 = ((n - m) - n / 2.0);
  double chi_squared_2 = ys_minus_np2 * ys_minus_np2 * 2.0 / n;
  return chi_squared_1 + chi_squared_2;
}


// Test for correlations between recent bits from the PRNG, or bits that are
// biased.
void RandomBitCorrelation(int random_bit) {
  FLAG_random_seed = 31415926;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  v8::base::RandomNumberGenerator* rng = i_isolate->random_number_generator();
#ifdef DEBUG
  const int kHistory = 2;
  const int kRepeats = 1000;
#else
  const int kHistory = 8;
  const int kRepeats = 10000;
#endif
  uint32_t history[kHistory];
  // The predictor bit is either constant 0 or 1, or one of the bits from the
  // history.
  for (int predictor_bit = -2; predictor_bit < 32; predictor_bit++) {
    // The predicted bit is one of the bits from the PRNG.
    for (int ago = 0; ago < kHistory; ago++) {
      // We don't want to check whether each bit predicts itself.
      if (ago == 0 && predictor_bit == random_bit) continue;

      // Enter the new random value into the history
      for (int i = ago; i >= 0; i--) {
        history[i] = bit_cast<uint32_t>(rng->NextInt());
      }

      // Find out how many of the bits are the same as the prediction bit.
      int m = 0;
      for (int i = 0; i < kRepeats; i++) {
        v8::HandleScope scope(isolate);
        uint32_t random = bit_cast<uint32_t>(rng->NextInt());
        for (int j = ago - 1; j >= 0; j--) history[j + 1] = history[j];
        history[0] = random;

        int predicted;
        if (predictor_bit >= 0) {
          predicted = (history[ago] >> predictor_bit) & 1;
        } else {
          predicted = predictor_bit == -2 ? 0 : 1;
        }
        int bit = (random >> random_bit) & 1;
        if (bit == predicted) m++;
      }

      // Chi squared analysis for k = 2 (2, states: same/not-same) and one
      // degree of freedom (k - 1).
      double chi_squared = ChiSquared(m, kRepeats);
      if (chi_squared > 24) {
        int percent = static_cast<int>(m * 100.0 / kRepeats);
        if (predictor_bit < 0) {
          PrintF("Bit %d is %d %d%% of the time\n", random_bit,
                 predictor_bit == -2 ? 0 : 1, percent);
        } else {
          PrintF("Bit %d is the same as bit %d %d ago %d%% of the time\n",
                 random_bit, predictor_bit, ago, percent);
        }
      }

      // For 1 degree of freedom this corresponds to 1 in a million.  We are
      // running ~8000 tests, so that would be surprising.
      CHECK_LE(chi_squared, 24);

      // If the predictor bit is a fixed 0 or 1 then it makes no sense to
      // repeat the test with a different age.
      if (predictor_bit < 0) break;
    }
  }
  isolate->Dispose();
}


#define TEST_RANDOM_BIT(BIT) \
  TEST(RandomBitCorrelations##BIT) { RandomBitCorrelation(BIT); }

TEST_RANDOM_BIT(0)
TEST_RANDOM_BIT(1)
TEST_RANDOM_BIT(2)
TEST_RANDOM_BIT(3)
TEST_RANDOM_BIT(4)
TEST_RANDOM_BIT(5)
TEST_RANDOM_BIT(6)
TEST_RANDOM_BIT(7)
TEST_RANDOM_BIT(8)
TEST_RANDOM_BIT(9)
TEST_RANDOM_BIT(10)
TEST_RANDOM_BIT(11)
TEST_RANDOM_BIT(12)
TEST_RANDOM_BIT(13)
TEST_RANDOM_BIT(14)
TEST_RANDOM_BIT(15)
TEST_RANDOM_BIT(16)
TEST_RANDOM_BIT(17)
TEST_RANDOM_BIT(18)
TEST_RANDOM_BIT(19)
TEST_RANDOM_BIT(20)
TEST_RANDOM_BIT(21)
TEST_RANDOM_BIT(22)
TEST_RANDOM_BIT(23)
TEST_RANDOM_BIT(24)
TEST_RANDOM_BIT(25)
TEST_RANDOM_BIT(26)
TEST_RANDOM_BIT(27)
TEST_RANDOM_BIT(28)
TEST_RANDOM_BIT(29)
TEST_RANDOM_BIT(30)
TEST_RANDOM_BIT(31)

#undef TEST_RANDOM_BIT

}  // namespace internal
}  // namespace v8

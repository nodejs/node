// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "test/common/wasm/deopt-fuzzer-utils.h"
#include "test/common/wasm/fuzzer-common.h"
#include "test/unittests/fuzztest.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm::fuzzing {

namespace {

inline auto wire_bytes_domain() {
  return fuzztest::VectorOf(fuzztest::Arbitrary<uint8_t>())
      .WithMinSize(1)
      .WithMaxSize(kMaxFuzzerInputSize);
}

}  // namespace

class DeoptFuzzTest
    : public fuzztest::PerFuzzTestFixtureAdapter<TestWithContext> {
 public:
  DeoptFuzzTest() {
    // Centipede has a default stack limit of 128kB. Thus also lower V8's stack
    // limit to something even smaller, so we catch stack overflows before
    // Centipede terminates the process.
    static_assert(V8_DEFAULT_STACK_SIZE_KB > 100);
    isolate()->SetStackLimit(base::Stack::GetStackStart() - 100 * KB);
  }

  ~DeoptFuzzTest() override = default;

  void Test(const std::vector<uint8_t>& input);
};

void DeoptFuzzTest::Test(const std::vector<uint8_t>& input) {
  v8::HandleScope scope(isolate());

  if (FuzzWasmDeopt(base::VectorOf(input), isolate()) < 0) {
    fuzztest::SkipTestsOrCurrentInput();
  }
}

V8_FUZZ_TEST_F(DeoptFuzzTest, Test).WithDomains(wire_bytes_domain());

}  // namespace v8::internal::wasm::fuzzing

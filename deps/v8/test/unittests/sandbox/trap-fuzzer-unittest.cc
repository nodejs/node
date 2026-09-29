// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/trap-fuzzer.h"

#include "src/sandbox/hardware-support.h"
#include "src/sandbox/sandbox-malloc.h"
#include "src/sandbox/sandbox.h"
#include "src/sandbox/testing.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"

#ifdef V8_SANDBOX_TRAP_FUZZER_AVAILABLE

namespace v8 {
namespace internal {

class SandboxTrapFuzzerTest : public TestWithContext {
 public:
  SandboxTrapFuzzerTest()
      : TestWithContext(),
        // The trap fuzzer is not thread-safe and requires --single-threaded.
        // See crbug.com/506943333.
        single_threaded_(&v8_flags.single_threaded, true) {}

 private:
  FlagScope<bool> single_threaded_;
};

TEST_F(SandboxTrapFuzzerTest, Basic) {
  if (!SandboxHardwareSupport::IsActive()) GTEST_SKIP();

  int* in_sandbox_memory = SandboxAlloc<int>();
  volatile int* in_sandbox_ptr = in_sandbox_memory;
  *in_sandbox_ptr = 0x12345678;

  SandboxTrapFuzzer::Enable();

  double start = base::OS::TimeCurrentMillis();
  while (*in_sandbox_ptr == 0x12345678) {
    // This loop should eventually see a different value because the fuzzer
    // will trap the read access to in-sandbox memory and mutate the value.
    if (base::OS::TimeCurrentMillis() - start > 1000) {
      FAIL() << "Trap fuzzer did not mutate memory within one second";
    }
  }

  SandboxTrapFuzzer::Disable();
  SandboxFree(in_sandbox_memory);
}

TEST_F(SandboxTrapFuzzerTest, SingleStepping) {
  if (!SandboxHardwareSupport::IsActive()) GTEST_SKIP();

  int* in_sandbox_memory = SandboxAlloc<int>();
  volatile int* in_sandbox_ptr = in_sandbox_memory;
  const int kOriginalValue = 0x12345678;
  *in_sandbox_ptr = kOriginalValue;

  SandboxTrapFuzzer::Enable();

  // We want to mutate the n-th read.
  base::RandomNumberGenerator rng(v8_flags.random_seed);
  int n = rng.NextInt(100) + 1;
  SandboxTrapFuzzer::RegisterMutationRequest(n);

  // Perform n - 1 reads, they should all see the original value.
  for (int i = 0; i < n - 1; i++) {
    ASSERT_EQ(*in_sandbox_ptr, kOriginalValue);
  }

  // The n-th read should be mutated.
  ASSERT_NE(*in_sandbox_ptr, kOriginalValue);

  SandboxTrapFuzzer::Disable();
  SandboxFree(in_sandbox_memory);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_TRAP_FUZZER_AVAILABLE

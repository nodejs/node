// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/fuzzing/random-module-generation.h"
#include "test/common/flag-utils.h"
#include "test/common/wasm/fuzzer-common.h"
#include "test/unittests/fuzztest.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::wasm::fuzzing {

class ModuleGenerationTest
    : public fuzztest::PerFuzzTestFixtureAdapter<TestWithContext> {
 public:
  ModuleGenerationTest() : zone_(&allocator_, "ModuleGenerationTest") {
    // Centipede has a default stack limit of 128kB. Thus also lower V8's stack
    // limit so something even smaller, so we catch stack overflows before
    // Centipede terminates the process.
    static_assert(V8_DEFAULT_STACK_SIZE_KB > 100);
    isolate()->SetStackLimit(base::Stack::GetStackStart() - 100 * KB);

    // Enable GC, required by `ResetTypeCanonicalizer`.
    v8_flags.expose_gc = true;

    // Random module generation mixed the old and new EH proposal; allow that
    // generally.
    // Note that for libfuzzer fuzzers this is implied by `--fuzzing`, but for
    // now we are more selective here and only enable this one flag.
    v8_flags.wasm_allow_mixed_eh_for_testing = true;
    EnableExperimentalWasmFeatures(isolate());
  }

  ~ModuleGenerationTest() override = default;

  // The different fuzztest targets.
  void TestMVP(int tier_mask, int debug_mask, const std::vector<uint8_t>&);
  void TestSimd(int tier_mask, int debug_mask, const std::vector<uint8_t>&);
  void TestGC(int tier_mask, int debug_mask, const std::vector<uint8_t>&);
  void TestAll(int tier_mask, int debug_mask, const std::vector<uint8_t>&);

 private:
  void Test(WasmModuleGenerationOptions options, int tier_mask, int debug_mask,
            const std::vector<uint8_t>&);

  AccountingAllocator allocator_;
  Zone zone_;
};

void ModuleGenerationTest::TestMVP(int tier_mask, int debug_mask,
                                   const std::vector<uint8_t>& input) {
  Test(WasmModuleGenerationOptions::MVP(), tier_mask, debug_mask, input);
}

void ModuleGenerationTest::TestSimd(int tier_mask, int debug_mask,
                                    const std::vector<uint8_t>& input) {
  Test(WasmModuleGenerationOptions::Simd(), tier_mask, debug_mask, input);
}

void ModuleGenerationTest::TestGC(int tier_mask, int debug_mask,
                                  const std::vector<uint8_t>& input) {
  Test(WasmModuleGenerationOptions::WasmGC(), tier_mask, debug_mask, input);
}

void ModuleGenerationTest::TestAll(int tier_mask, int debug_mask,
                                   const std::vector<uint8_t>& input) {
  Test(WasmModuleGenerationOptions::All(), tier_mask, debug_mask, input);
}

void ModuleGenerationTest::Test(WasmModuleGenerationOptions options,
                                int tier_mask, int debug_mask,
                                const std::vector<uint8_t>& input) {
  // Set the tier mask to deterministically test a combination of Liftoff and
  // Turbofan.
  FlagScope<int> tier_mask_scope(&v8_flags.wasm_tier_mask_for_testing,
                                 tier_mask);
  // Generate debug code for some Liftoff functions.
  FlagScope<int> debug_mask_scope(&v8_flags.wasm_debug_mask_for_testing,
                                  debug_mask);

  // Do not generate SIMD if the CPU does not support SIMD.
  if (options.generate_simd() && !CpuFeatures::SupportsWasmSimd128()) {
    options = options - WasmModuleGenerationOption::kGenerateSIMD;
  }

  zone_.Reset();
  base::Vector<const uint8_t> wire_bytes =
      GenerateRandomWasmModule(&zone_, options, base::VectorOf(input));
  constexpr bool kRequireValid = true;
  SyncCompileAndExecuteAgainstReference(isolate(), wire_bytes, kRequireValid);
}

inline auto tier_mask_domain() { return fuzztest::Arbitrary<int>(); }
inline auto debug_mask_domain() { return fuzztest::Arbitrary<int>(); }
inline auto wire_bytes_domain() {
  return fuzztest::VectorOf(fuzztest::Arbitrary<uint8_t>())
      .WithMinSize(1)
      .WithMaxSize(512);
}

V8_FUZZ_TEST_F(ModuleGenerationTest, TestAll)
    .WithDomains(tier_mask_domain(), debug_mask_domain(), wire_bytes_domain());

V8_FUZZ_TEST_F(ModuleGenerationTest, TestSimd)
    .WithDomains(tier_mask_domain(), debug_mask_domain(), wire_bytes_domain());

V8_FUZZ_TEST_F(ModuleGenerationTest, TestGC)
    .WithDomains(tier_mask_domain(), debug_mask_domain(), wire_bytes_domain());

V8_FUZZ_TEST_F(ModuleGenerationTest, TestMVP)
    .WithDomains(tier_mask_domain(), debug_mask_domain(), wire_bytes_domain());

}  // namespace v8::internal::wasm::fuzzing

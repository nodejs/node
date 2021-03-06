// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/objects-inl.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-limits.h"
#include "test/common/wasm/wasm-macro-gen.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace module_decoder_unittest {

#define EXPECT_OK(result)                                        \
  do {                                                           \
    if (!result.ok()) {                                          \
      GTEST_NONFATAL_FAILURE_(result.error().message().c_str()); \
      return;                                                    \
    }                                                            \
  } while (false)

class Memory64DecodingTest : public TestWithIsolateAndZone {
 public:
  ModuleResult DecodeModule(std::initializer_list<uint8_t> module_body_bytes) {
    // Add the wasm magic and version number automatically.
    std::vector<uint8_t> module_bytes{WASM_MODULE_HEADER};
    module_bytes.insert(module_bytes.end(), module_body_bytes);
    static constexpr WasmFeatures kEnabledFeatures{
        WasmFeature::kFeature_memory64};
    return DecodeWasmModule(
        kEnabledFeatures, module_bytes.data(),
        module_bytes.data() + module_bytes.size(), false, kWasmOrigin,
        isolate()->counters(), isolate()->metrics_recorder(),
        v8::metrics::Recorder::ContextId::Empty(), DecodingMethod::kSync,
        isolate()->wasm_engine()->allocator());
  }
};

TEST_F(Memory64DecodingTest, MemoryLimitLEB64) {
  // 2 bytes LEB (32-bit range), no maximum.
  ModuleResult result = DecodeModule(
      {SECTION(Memory, ENTRY_COUNT(1), kMemory64NoMaximum, U32V_2(5))});
  EXPECT_OK(result);
  EXPECT_EQ(5u, result.value()->initial_pages);
  EXPECT_EQ(false, result.value()->has_maximum_pages);

  // 2 bytes LEB (32-bit range), with maximum.
  result = DecodeModule({SECTION(Memory, ENTRY_COUNT(1), kMemory64WithMaximum,
                                 U32V_2(7), U32V_2(47))});
  EXPECT_OK(result);
  EXPECT_EQ(7u, result.value()->initial_pages);
  EXPECT_EQ(true, result.value()->has_maximum_pages);
  EXPECT_EQ(47u, result.value()->maximum_pages);

  // 10 bytes LEB, 32-bit range, no maximum.
  result = DecodeModule(
      {SECTION(Memory, ENTRY_COUNT(1), kMemory64NoMaximum, U64V_10(2))});
  EXPECT_OK(result);
  EXPECT_EQ(2u, result.value()->initial_pages);
  EXPECT_EQ(false, result.value()->has_maximum_pages);

  // 10 bytes LEB, 32-bit range, with maximum.
  result = DecodeModule({SECTION(Memory, ENTRY_COUNT(1), kMemory64WithMaximum,
                                 U64V_10(2), U64V_10(6))});
  EXPECT_OK(result);
  EXPECT_EQ(2u, result.value()->initial_pages);
  EXPECT_EQ(true, result.value()->has_maximum_pages);
  EXPECT_EQ(6u, result.value()->maximum_pages);

  // TODO(clemensb): Test numbers outside the 32-bit range once that's
  // supported.
}

}  // namespace module_decoder_unittest
}  // namespace wasm
}  // namespace internal
}  // namespace v8

// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-engine.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-runner.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8::internal::wasm {

TEST(FuzzerStepsPrefixedOpcode) {
  WasmRunner<uint32_t> r(TestExecutionTier::kLiftoffForFuzzing);
  // Entry: 16. MemoryCopy: 1000. Constant: 1. Total: 1017.
  r.builder().AddMemory(kWasmPageSize);
  r.Build(
      {WASM_MEMORY_COPY(0, 0, WASM_I32V_1(0), WASM_I32V_1(0), WASM_I32V_1(0)),
       WASM_I32V_1(42)});

  // 1. Run which does not trap.
  r.SetMaxSteps(2000);
  CHECK_EQ(42, r.Call());

  // 2. Run which should trap.
  r.SetMaxSteps(500);
  r.CheckCallViaJSTraps();
}

TEST(FuzzerStepsNonPrefixedOpcode) {
  WasmRunner<uint32_t> r(TestExecutionTier::kLiftoffForFuzzing);
  // Entry: 16. MemoryGrow: 1000. Constant: 1. Total: 1017.
  r.builder().AddMemory(kWasmPageSize);
  r.Build({WASM_MEMORY_GROW(WASM_I32V_1(1)), WASM_DROP, WASM_I32V_1(42)});

  // 1. Run which does not trap.
  r.SetMaxSteps(2000);
  CHECK_EQ(42, r.Call());

  // 2. Run which should trap.
  r.SetMaxSteps(500);
  r.CheckCallViaJSTraps();
}

}  // namespace v8::internal::wasm

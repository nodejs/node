// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEST_CCTEST_WASM_WASM_RUNNER_H
#define TEST_CCTEST_WASM_WASM_RUNNER_H

#include "test/cctest/cctest.h"
#include "test/common/wasm/wasm-run-utils.h"

namespace v8::internal::wasm {

template <typename ReturnType, typename... ParamTypes>
class WasmRunner : public CommonWasmRunner<ReturnType, ParamTypes...> {
 public:
  // If the caller does not provide an isolate, use CcTest's main isolate.
  template <typename... Args>
    requires(... && !std::is_same_v<Args, Isolate*>)
  explicit WasmRunner(TestExecutionTier execution_tier, Args&&... args)
      : CommonWasmRunner<ReturnType, ParamTypes...>(
            CcTest::InitIsolateOnce(), execution_tier,
            std::forward<Args&&>(args)...) {}

  // If an isolate is provided, use that.
  template <typename... Args>
    requires(... && !std::is_same_v<Args, Isolate*>)
  explicit WasmRunner(Isolate* isolate, Args&&... args)
      : CommonWasmRunner<ReturnType, ParamTypes...>(
            isolate, std::forward<Args&&>(args)...) {}
};

}  // namespace v8::internal::wasm

#endif  //  TEST_CCTEST_WASM_WASM_RUNNER_H

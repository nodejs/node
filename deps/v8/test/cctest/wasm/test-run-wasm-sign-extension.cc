// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {

WASM_EXEC_TEST(I32SExtendI8) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  r.Build({WASM_I32_SIGN_EXT_I8(WASM_LOCAL_GET(0))});
  CHECK_EQ(0, r.Call(0));
  CHECK_EQ(1, r.Call(1));
  CHECK_EQ(-1, r.Call(-1));
  CHECK_EQ(0x7a, r.Call(0x7a));
  CHECK_EQ(-0x80, r.Call(0x80));
}

WASM_EXEC_TEST(I32SExtendI16) {
  WasmRunner<int32_t, int32_t> r(execution_tier);
  r.Build({WASM_I32_SIGN_EXT_I16(WASM_LOCAL_GET(0))});
  CHECK_EQ(0, r.Call(0));
  CHECK_EQ(1, r.Call(1));
  CHECK_EQ(-1, r.Call(-1));
  CHECK_EQ(0x7afa, r.Call(0x7afa));
  CHECK_EQ(-0x8000, r.Call(0x8000));
}

WASM_EXEC_TEST(I64SExtendI8) {
  WasmRunner<int64_t, int64_t> r(execution_tier);
  r.Build({WASM_I64_SIGN_EXT_I8(WASM_LOCAL_GET(0))});
  CHECK_EQ(0, r.Call(0));
  CHECK_EQ(1, r.Call(1));
  CHECK_EQ(-1, r.Call(-1));
  CHECK_EQ(0x7a, r.Call(0x7a));
  CHECK_EQ(-0x80, r.Call(0x80));
}

WASM_EXEC_TEST(I64SExtendI16) {
  WasmRunner<int64_t, int64_t> r(execution_tier);
  r.Build({WASM_I64_SIGN_EXT_I16(WASM_LOCAL_GET(0))});
  CHECK_EQ(0, r.Call(0));
  CHECK_EQ(1, r.Call(1));
  CHECK_EQ(-1, r.Call(-1));
  CHECK_EQ(0x7afa, r.Call(0x7afa));
  CHECK_EQ(-0x8000, r.Call(0x8000));
}

WASM_EXEC_TEST(I64SExtendI32) {
  WasmRunner<int64_t, int64_t> r(execution_tier);
  r.Build({WASM_I64_SIGN_EXT_I32(WASM_LOCAL_GET(0))});
  CHECK_EQ(0, r.Call(0));
  CHECK_EQ(1, r.Call(1));
  CHECK_EQ(-1, r.Call(-1));
  CHECK_EQ(0x7fffffff, r.Call(0x7fffffff));
  CHECK_EQ(-0x80000000LL, r.Call(0x80000000));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

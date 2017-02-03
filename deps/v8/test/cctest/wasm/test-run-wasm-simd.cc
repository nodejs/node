// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-macro-gen.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

WASM_EXEC_TEST(Splat) {
  FLAG_wasm_simd_prototype = true;

  // Store SIMD value in a local variable, use extract lane to check lane values
  // This test is not a test for ExtractLane as Splat does not create
  // interesting SIMD values.
  //
  // SetLocal(1, I32x4Splat(Local(0)));
  // For each lane index
  // if(Local(0) != I32x4ExtractLane(Local(1), index)
  //   return 0
  //
  // return 1
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  r.AllocateLocal(kAstS128);
  BUILD(r,
        WASM_BLOCK(
            WASM_SET_LOCAL(1, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0))),
            WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(0), WASM_SIMD_I32x4_EXTRACT_LANE(
                                                       0, WASM_GET_LOCAL(1))),
                    WASM_RETURN1(WASM_ZERO)),
            WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(0), WASM_SIMD_I32x4_EXTRACT_LANE(
                                                       1, WASM_GET_LOCAL(1))),
                    WASM_RETURN1(WASM_ZERO)),
            WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(0), WASM_SIMD_I32x4_EXTRACT_LANE(
                                                       2, WASM_GET_LOCAL(1))),
                    WASM_RETURN1(WASM_ZERO)),
            WASM_IF(WASM_I32_NE(WASM_GET_LOCAL(0), WASM_SIMD_I32x4_EXTRACT_LANE(
                                                       3, WASM_GET_LOCAL(1))),
                    WASM_RETURN1(WASM_ZERO)),
            WASM_RETURN1(WASM_ONE)));

  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(*i)); }
}

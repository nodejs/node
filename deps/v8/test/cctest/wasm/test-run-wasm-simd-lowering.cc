// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::base;
using namespace v8::internal;
using namespace v8::internal::compiler;
using namespace v8::internal::wasm;

WASM_EXEC_TEST(Simd_I32x4_Splat) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  BUILD(r,
        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(5))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(5, r.Call()); }
}

WASM_EXEC_TEST(Simd_I32x4_Add) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  BUILD(r, WASM_SIMD_I32x4_EXTRACT_LANE(
               0, WASM_SIMD_I32x4_ADD(WASM_SIMD_I32x4_SPLAT(WASM_I32V(5)),
                                      WASM_SIMD_I32x4_SPLAT(WASM_I32V(6)))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(11, r.Call()); }
}

WASM_EXEC_TEST(Simd_F32x4_Splat) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  BUILD(r,
        WASM_IF_ELSE(WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                                     0, WASM_SIMD_F32x4_SPLAT(WASM_F32(9.5))),
                                 WASM_F32(9.5)),
                     WASM_RETURN1(WASM_I32V(1)), WASM_RETURN1(WASM_I32V(0))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_TEST(Simd_I32x4_Extract_With_F32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  BUILD(r,
        WASM_IF_ELSE(WASM_I32_EQ(WASM_SIMD_I32x4_EXTRACT_LANE(
                                     0, WASM_SIMD_F32x4_SPLAT(WASM_F32(30.5))),
                                 WASM_I32_REINTERPRET_F32(WASM_F32(30.5))),
                     WASM_RETURN1(WASM_I32V(1)), WASM_RETURN1(WASM_I32V(0))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_TEST(Simd_F32x4_Extract_With_I32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  BUILD(r,
        WASM_IF_ELSE(WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                                     0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(15))),
                                 WASM_F32_REINTERPRET_I32(WASM_I32V(15))),
                     WASM_RETURN1(WASM_I32V(1)), WASM_RETURN1(WASM_I32V(0))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_TEST(Simd_F32x4_Add_With_I32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  BUILD(r,
        WASM_IF_ELSE(
            WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                            0, WASM_SIMD_F32x4_ADD(
                                   WASM_SIMD_I32x4_SPLAT(WASM_I32V(32)),
                                   WASM_SIMD_I32x4_SPLAT(WASM_I32V(19)))),
                        WASM_F32_ADD(WASM_F32_REINTERPRET_I32(WASM_I32V(32)),
                                     WASM_F32_REINTERPRET_I32(WASM_I32V(19)))),
            WASM_RETURN1(WASM_I32V(1)), WASM_RETURN1(WASM_I32V(0))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_TEST(Simd_I32x4_Add_With_F32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled, MachineType::Int32());
  BUILD(r,
        WASM_IF_ELSE(
            WASM_I32_EQ(WASM_SIMD_I32x4_EXTRACT_LANE(
                            0, WASM_SIMD_I32x4_ADD(
                                   WASM_SIMD_F32x4_SPLAT(WASM_F32(21.25)),
                                   WASM_SIMD_F32x4_SPLAT(WASM_F32(31.5)))),
                        WASM_I32_ADD(WASM_I32_REINTERPRET_F32(WASM_F32(21.25)),
                                     WASM_I32_REINTERPRET_F32(WASM_F32(31.5)))),
            WASM_RETURN1(WASM_I32V(1)), WASM_RETURN1(WASM_I32V(0))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

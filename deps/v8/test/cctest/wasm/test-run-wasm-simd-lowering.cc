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

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Splat) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  BUILD(r,
        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(5))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(5, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Add) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  BUILD(r, WASM_SIMD_I32x4_EXTRACT_LANE(
               0, WASM_SIMD_I32x4_ADD(WASM_SIMD_I32x4_SPLAT(WASM_I32V(5)),
                                      WASM_SIMD_I32x4_SPLAT(WASM_I32V(6)))));
  FOR_INT32_INPUTS(i) { CHECK_EQ(11, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_F32x4_Splat) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  BUILD(r,
        WASM_IF_ELSE_I(WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                                       0, WASM_SIMD_F32x4_SPLAT(WASM_F32(9.5))),
                                   WASM_F32(9.5)),
                       WASM_I32V(1), WASM_I32V(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Extract_With_F32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  BUILD(r, WASM_IF_ELSE_I(
               WASM_I32_EQ(WASM_SIMD_I32x4_EXTRACT_LANE(
                               0, WASM_SIMD_F32x4_SPLAT(WASM_F32(30.5))),
                           WASM_I32_REINTERPRET_F32(WASM_F32(30.5))),
               WASM_I32V(1), WASM_I32V(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_F32x4_Extract_With_I32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  BUILD(r,
        WASM_IF_ELSE_I(WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                                       0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(15))),
                                   WASM_F32_REINTERPRET_I32(WASM_I32V(15))),
                       WASM_I32V(1), WASM_I32V(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_F32x4_Add_With_I32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  BUILD(r,
        WASM_IF_ELSE_I(
            WASM_F32_EQ(WASM_SIMD_F32x4_EXTRACT_LANE(
                            0, WASM_SIMD_F32x4_ADD(
                                   WASM_SIMD_I32x4_SPLAT(WASM_I32V(32)),
                                   WASM_SIMD_I32x4_SPLAT(WASM_I32V(19)))),
                        WASM_F32_ADD(WASM_F32_REINTERPRET_I32(WASM_I32V(32)),
                                     WASM_F32_REINTERPRET_I32(WASM_I32V(19)))),
            WASM_I32V(1), WASM_I32V(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Add_With_F32x4) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  BUILD(r,
        WASM_IF_ELSE_I(
            WASM_I32_EQ(WASM_SIMD_I32x4_EXTRACT_LANE(
                            0, WASM_SIMD_I32x4_ADD(
                                   WASM_SIMD_F32x4_SPLAT(WASM_F32(21.25)),
                                   WASM_SIMD_F32x4_SPLAT(WASM_F32(31.5)))),
                        WASM_I32_ADD(WASM_I32_REINTERPRET_F32(WASM_F32(21.25)),
                                     WASM_I32_REINTERPRET_F32(WASM_F32(31.5)))),
            WASM_I32V(1), WASM_I32V(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Local) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),

        WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(31, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Replace_Lane) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),
        WASM_SET_LOCAL(0, WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_GET_LOCAL(0),
                                                       WASM_I32V(53))),
        WASM_SIMD_I32x4_EXTRACT_LANE(2, WASM_GET_LOCAL(0)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(53, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_F32x4_Replace_Lane) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  r.AllocateLocal(kWasmF32);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(1, WASM_SIMD_F32x4_SPLAT(WASM_F32(23.5))),
        WASM_SET_LOCAL(1, WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GET_LOCAL(1),
                                                       WASM_F32(65.25))),
        WASM_SET_LOCAL(0, WASM_SIMD_F32x4_EXTRACT_LANE(3, WASM_GET_LOCAL(1))),
        WASM_IF(WASM_F32_EQ(WASM_GET_LOCAL(0), WASM_F32(65.25)),
                WASM_RETURN1(WASM_I32V(1))),
        WASM_I32V(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Splat_From_Extract) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(0, WASM_SIMD_I32x4_EXTRACT_LANE(
                                 0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(76)))),
        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0))),
        WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GET_LOCAL(1)));
  FOR_INT32_INPUTS(i) { CHECK_EQ(76, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Get_Global) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  int32_t* global = r.module().AddGlobal<int32_t>(kWasmS128);
  *(global) = 0;
  *(global + 1) = 1;
  *(global + 2) = 2;
  *(global + 3) = 3;
  r.AllocateLocal(kWasmI32);
  BUILD(
      r, WASM_SET_LOCAL(1, WASM_I32V(1)),
      WASM_IF(WASM_I32_NE(WASM_I32V(0),
                          WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(1),
                          WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(2),
                          WASM_SIMD_I32x4_EXTRACT_LANE(2, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_I32_NE(WASM_I32V(3),
                          WASM_SIMD_I32x4_EXTRACT_LANE(3, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_GET_LOCAL(1));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(0)); }
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_Set_Global) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  int32_t* global = r.module().AddGlobal<int32_t>(kWasmS128);
  BUILD(r, WASM_SET_GLOBAL(0, WASM_SIMD_I32x4_SPLAT(WASM_I32V(23))),
        WASM_SET_GLOBAL(0, WASM_SIMD_I32x4_REPLACE_LANE(1, WASM_GET_GLOBAL(0),
                                                        WASM_I32V(34))),
        WASM_SET_GLOBAL(0, WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_GET_GLOBAL(0),
                                                        WASM_I32V(45))),
        WASM_SET_GLOBAL(0, WASM_SIMD_I32x4_REPLACE_LANE(3, WASM_GET_GLOBAL(0),
                                                        WASM_I32V(56))),
        WASM_I32V(1));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(0)); }
  CHECK_EQ(*global, 23);
  CHECK_EQ(*(global + 1), 34);
  CHECK_EQ(*(global + 2), 45);
  CHECK_EQ(*(global + 3), 56);
}

WASM_EXEC_COMPILED_TEST(Simd_F32x4_Get_Global) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  float* global = r.module().AddGlobal<float>(kWasmS128);
  *(global) = 0.0;
  *(global + 1) = 1.5;
  *(global + 2) = 2.25;
  *(global + 3) = 3.5;
  r.AllocateLocal(kWasmI32);
  BUILD(
      r, WASM_SET_LOCAL(1, WASM_I32V(1)),
      WASM_IF(WASM_F32_NE(WASM_F32(0.0),
                          WASM_SIMD_F32x4_EXTRACT_LANE(0, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(1.5),
                          WASM_SIMD_F32x4_EXTRACT_LANE(1, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(2.25),
                          WASM_SIMD_F32x4_EXTRACT_LANE(2, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_IF(WASM_F32_NE(WASM_F32(3.5),
                          WASM_SIMD_F32x4_EXTRACT_LANE(3, WASM_GET_GLOBAL(0))),
              WASM_SET_LOCAL(1, WASM_I32V(0))),
      WASM_GET_LOCAL(1));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(0)); }
}

WASM_EXEC_COMPILED_TEST(Simd_F32x4_Set_Global) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t, int32_t> r(kExecuteCompiled);
  float* global = r.module().AddGlobal<float>(kWasmS128);
  BUILD(r, WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_SPLAT(WASM_F32(13.5))),
        WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_REPLACE_LANE(1, WASM_GET_GLOBAL(0),
                                                        WASM_F32(45.5))),
        WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_REPLACE_LANE(2, WASM_GET_GLOBAL(0),
                                                        WASM_F32(32.25))),
        WASM_SET_GLOBAL(0, WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GET_GLOBAL(0),
                                                        WASM_F32(65.0))),
        WASM_I32V(1));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call(0)); }
  CHECK_EQ(*global, 13.5);
  CHECK_EQ(*(global + 1), 45.5);
  CHECK_EQ(*(global + 2), 32.25);
  CHECK_EQ(*(global + 3), 65.0);
}

WASM_EXEC_COMPILED_TEST(Simd_I32x4_For) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r,

        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_SPLAT(WASM_I32V(31))),
        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_REPLACE_LANE(1, WASM_GET_LOCAL(1),
                                                       WASM_I32V(53))),
        WASM_SET_LOCAL(1, WASM_SIMD_I32x4_REPLACE_LANE(2, WASM_GET_LOCAL(1),
                                                       WASM_I32V(23))),
        WASM_SET_LOCAL(0, WASM_I32V(0)),
        WASM_LOOP(
            WASM_SET_LOCAL(
                1, WASM_SIMD_I32x4_ADD(WASM_GET_LOCAL(1),
                                       WASM_SIMD_I32x4_SPLAT(WASM_I32V(1)))),
            WASM_IF(WASM_I32_NE(WASM_INC_LOCAL(0), WASM_I32V(5)), WASM_BR(1))),
        WASM_SET_LOCAL(0, WASM_I32V(1)),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(0, WASM_GET_LOCAL(1)),
                            WASM_I32V(36)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(1, WASM_GET_LOCAL(1)),
                            WASM_I32V(58)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(2, WASM_GET_LOCAL(1)),
                            WASM_I32V(28)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_I32_NE(WASM_SIMD_I32x4_EXTRACT_LANE(3, WASM_GET_LOCAL(1)),
                            WASM_I32V(36)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

WASM_EXEC_COMPILED_TEST(Simd_F32x4_For) {
  FLAG_wasm_simd_prototype = true;
  WasmRunner<int32_t> r(kExecuteCompiled);
  r.AllocateLocal(kWasmI32);
  r.AllocateLocal(kWasmS128);
  BUILD(r, WASM_SET_LOCAL(1, WASM_SIMD_F32x4_SPLAT(WASM_F32(21.25))),
        WASM_SET_LOCAL(1, WASM_SIMD_F32x4_REPLACE_LANE(3, WASM_GET_LOCAL(1),
                                                       WASM_F32(19.5))),
        WASM_SET_LOCAL(0, WASM_I32V(0)),
        WASM_LOOP(
            WASM_SET_LOCAL(
                1, WASM_SIMD_F32x4_ADD(WASM_GET_LOCAL(1),
                                       WASM_SIMD_F32x4_SPLAT(WASM_F32(2.0)))),
            WASM_IF(WASM_I32_NE(WASM_INC_LOCAL(0), WASM_I32V(3)), WASM_BR(1))),
        WASM_SET_LOCAL(0, WASM_I32V(1)),
        WASM_IF(WASM_F32_NE(WASM_SIMD_F32x4_EXTRACT_LANE(0, WASM_GET_LOCAL(1)),
                            WASM_F32(27.25)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_IF(WASM_F32_NE(WASM_SIMD_F32x4_EXTRACT_LANE(3, WASM_GET_LOCAL(1)),
                            WASM_F32(25.5)),
                WASM_SET_LOCAL(0, WASM_I32V(0))),
        WASM_GET_LOCAL(0));
  FOR_INT32_INPUTS(i) { CHECK_EQ(1, r.Call()); }
}

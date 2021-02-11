// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-tier.h"
#include "test/cctest/cctest.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/flag-utils.h"
#include "test/common/wasm/test-signatures.h"
#include "test/common/wasm/wasm-macro-gen.h"

namespace v8 {
namespace internal {
namespace wasm {
namespace test_run_wasm_simd {

#define WASM_SIMD_TEST(name)                                         \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                   \
                             TestExecutionTier execution_tier);      \
  TEST(RunWasm_##name##_simd_lowered) {                              \
    EXPERIMENTAL_FLAG_SCOPE(simd);                                   \
    RunWasm_##name##_Impl(kLowerSimd, TestExecutionTier::kTurbofan); \
  }                                                                  \
  void RunWasm_##name##_Impl(LowerSimd lower_simd,                   \
                             TestExecutionTier execution_tier)

WASM_SIMD_TEST(I8x16ToF32x4) {
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
  float* g = r.builder().AddGlobal<float>(kWasmS128);
  byte param1 = 0;
  BUILD(r,
        WASM_SET_GLOBAL(
            0, WASM_SIMD_UNOP(kExprF32x4Sqrt,
                              WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(param1)))),
        WASM_ONE);

  // Arbitrary pattern that doesn't end up creating a NaN.
  r.Call(0x5b);
  float f = bit_cast<float>(0x5b5b5b5b);
  float actual = ReadLittleEndianValue<float>(&g[0]);
  float expected = std::sqrt(f);
  CHECK_EQ(expected, actual);
}

WASM_SIMD_TEST(F64x2_Call_Return) {
  // Check that calling a function with i16x8 arguments, and returns i16x8, is
  // correctly lowered. The signature of the functions are always lowered to 4
  // Word32, so each i16x8 needs to be correctly converted.
  TestSignatures sigs;
  WasmRunner<double, double, double> r(execution_tier, lower_simd);

  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_ss());
  BUILD(fn,
        WASM_SIMD_BINOP(kExprF64x2Min, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  byte c1[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  byte c2[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0x7f,
                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0x7f};

  BUILD(r,
        WASM_SIMD_F64x2_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(), WASM_SIMD_CONSTANT(c1),
                                  WASM_SIMD_CONSTANT(c2))));
  CHECK_EQ(0, r.Call(double{0}, bit_cast<double>(0x7fefffffffffffff)));
}

WASM_SIMD_TEST(F32x4_Call_Return) {
  // Check that functions that return F32x4 are correctly lowered into 4 int32
  // nodes. The signature of such functions are always lowered to 4 Word32, and
  // if the last operation before the return was a f32x4, it will need to be
  // bitcasted from float to int.
  TestSignatures sigs;
  WasmRunner<float, float> r(execution_tier, lower_simd);

  // A simple function that just calls f32x4.neg on the param.
  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_s());
  BUILD(fn, WASM_SIMD_UNOP(kExprF32x4Neg, WASM_GET_LOCAL(0)));

  // TODO(v8:10507)
  // Use i32x4 splat since scalar lowering has a problem with f32x4 as a param
  // to a function call, the lowering is not correct yet.
  BUILD(r,
        WASM_SIMD_F32x4_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(),
                                  WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(0)))));
  CHECK_EQ(-1.0, r.Call(1));
}

WASM_SIMD_TEST(I8x16_Call_Return) {
  // Check that calling a function with i8x16 arguments, and returns i8x16, is
  // correctly lowered. The signature of the functions are always lowered to 4
  // Word32, so each i8x16 needs to be correctly converted.
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t> r(execution_tier, lower_simd);

  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_ss());
  BUILD(fn,
        WASM_SIMD_BINOP(kExprI8x16Add, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  BUILD(r,
        WASM_SIMD_I8x16_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(),
                                  WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(0)),
                                  WASM_SIMD_I8x16_SPLAT(WASM_GET_LOCAL(0)))));
  CHECK_EQ(2, r.Call(1));
}

WASM_SIMD_TEST(I16x8_Call_Return) {
  // Check that calling a function with i16x8 arguments, and returns i16x8, is
  // correctly lowered. The signature of the functions are always lowered to 4
  // Word32, so each i16x8 needs to be correctly converted.
  TestSignatures sigs;
  WasmRunner<uint32_t, uint32_t> r(execution_tier, lower_simd);

  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_ss());
  BUILD(fn,
        WASM_SIMD_BINOP(kExprI16x8Add, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  BUILD(r,
        WASM_SIMD_I16x8_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(),
                                  WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(0)),
                                  WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(0)))));
  CHECK_EQ(2, r.Call(1));
}

WASM_SIMD_TEST(I64x2_Call_Return) {
  // Check that calling a function with i64x2 arguments, and returns i64x2, is
  // correctly lowered. The signature of the functions are always lowered to 4
  // Word32, so each i64x2 needs to be correctly converted.
  TestSignatures sigs;
  WasmRunner<uint64_t, uint64_t> r(execution_tier, lower_simd);

  WasmFunctionCompiler& fn = r.NewFunction(sigs.s_ss());
  BUILD(fn,
        WASM_SIMD_BINOP(kExprI64x2Add, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  BUILD(r,
        WASM_SIMD_I64x2_EXTRACT_LANE(
            0, WASM_CALL_FUNCTION(fn.function_index(),
                                  WASM_SIMD_I64x2_SPLAT(WASM_GET_LOCAL(0)),
                                  WASM_SIMD_I64x2_SPLAT(WASM_GET_LOCAL(0)))));
  CHECK_EQ(2, r.Call(1));
}

WASM_SIMD_TEST(I8x16Eq_ToTest_S128Const) {
  // Test implementation of S128Const in scalar lowering, this test case was
  // causing a crash.
  TestSignatures sigs;
  WasmRunner<uint32_t> r(execution_tier, lower_simd);

  byte c1[16] = {0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0x00,
                 0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40};
  byte c2[16] = {0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
                 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02};
  byte c3[16] = {0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  BUILD(r,
        WASM_SIMD_BINOP(kExprI8x16Eq, WASM_SIMD_CONSTANT(c1),
                        WASM_SIMD_CONSTANT(c2)),
        WASM_SIMD_CONSTANT(c3), WASM_SIMD_OP(kExprI8x16Eq),
        WASM_SIMD_OP(kExprI8x16ExtractLaneS), TO_BYTE(4));
  CHECK_EQ(0xffffffff, r.Call());
}

WASM_SIMD_TEST(F32x4_S128Const) {
  // Test that S128Const lowering is done correctly when it is used as an input
  // into a f32x4 operation. This was triggering a CHECK failure in the
  // register-allocator-verifier.
  TestSignatures sigs;
  WasmRunner<float> r(execution_tier, lower_simd);

  // f32x4(1.0, 2.0, 3.0, 4.0)
  byte c1[16] = {0x00, 0x00, 0x80, 0x3f, 0x00, 0x00, 0x00, 0x40,
                 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x80, 0x40};
  // f32x4(5.0, 6.0, 7.0, 8.0)
  byte c2[16] = {0x00, 0x00, 0xa0, 0x40, 0x00, 0x00, 0xc0, 0x40,
                 0x00, 0x00, 0xe0, 0x40, 0x00, 0x00, 0x00, 0x41};

  BUILD(r,
        WASM_SIMD_BINOP(kExprF32x4Min, WASM_SIMD_CONSTANT(c1),
                        WASM_SIMD_CONSTANT(c2)),
        WASM_SIMD_OP(kExprF32x4ExtractLane), TO_BYTE(0));
  CHECK_EQ(1.0, r.Call());
}

WASM_SIMD_TEST(AllTrue_DifferentShapes) {
  // Test all_true lowring with splats of different shapes.
  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV8x16AllTrue));

    CHECK_EQ(0, r.Call(0x00FF00FF));
  }

  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV16x8AllTrue));

    CHECK_EQ(0, r.Call(0x000000FF));
  }

  // Check float input to all_true.
  {
    WasmRunner<int32_t, float> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV16x8AllTrue));

    CHECK_EQ(1, r.Call(0x000F000F));
  }
}

WASM_SIMD_TEST(AnyTrue_DifferentShapes) {
  // Test any_true lowring with splats of different shapes.
  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV8x16AnyTrue));

    CHECK_EQ(0, r.Call(0x00000000));
  }

  {
    WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV16x8AnyTrue));

    CHECK_EQ(1, r.Call(0x000000FF));
  }

  // Check float input to any_true.
  {
    WasmRunner<int32_t, float> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_F32x4_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprV8x16AnyTrue));

    CHECK_EQ(0, r.Call(0x00000000));
  }
}

WASM_SIMD_TEST(V128_I64_PARAMS) {
  // This test exercises interaction between simd and int64 lowering. The
  // parameter indices were not correctly lowered because simd lowered a v128 in
  // the function signature into 4 word32, and int64 was still treating it as 1
  // parameter.
  WasmRunner<uint64_t, uint64_t> r(execution_tier, lower_simd);

  FunctionSig::Builder builder(r.zone(), 1, 2);
  builder.AddParam(kWasmS128);
  builder.AddParam(kWasmI64);
  builder.AddReturn(kWasmS128);
  FunctionSig* sig = builder.Build();
  WasmFunctionCompiler& fn = r.NewFunction(sig);

  // Build a function that has both V128 and I64 arguments.
  BUILD(fn,
        WASM_SIMD_I64x2_REPLACE_LANE(0, WASM_GET_LOCAL(0), WASM_GET_LOCAL(1)));

  BUILD(r, WASM_SIMD_I64x2_EXTRACT_LANE(
               0, WASM_SIMD_I64x2_SPLAT(WASM_GET_LOCAL(0))));

  CHECK_EQ(0, r.Call(0));
}

WASM_SIMD_TEST(I8x16WidenS_I16x8NarrowU) {
  // Test any_true lowring with splats of different shapes.
  {
    WasmRunner<int32_t, int16_t> r(execution_tier, lower_simd);

    BUILD(r, WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_I16x8_SPLAT(WASM_GET_LOCAL(0)),
          WASM_SIMD_OP(kExprI8x16UConvertI16x8),
          WASM_SIMD_OP(kExprI16x8SConvertI8x16Low),
          WASM_SIMD_OP(kExprI32x4ExtractLane), TO_BYTE(0));

    CHECK_EQ(bit_cast<int32_t>(0xffffffff), r.Call(0x7fff));
  }
}

WASM_SIMD_TEST(S128SelectWithF32x4) {
  WasmRunner<float, int32_t, float, int32_t> r(execution_tier, lower_simd);
  BUILD(r, WASM_GET_LOCAL(0), WASM_SIMD_OP(kExprI32x4Splat), WASM_GET_LOCAL(1),
        WASM_SIMD_OP(kExprF32x4Splat), WASM_GET_LOCAL(2),
        WASM_SIMD_OP(kExprI32x4Splat), WASM_SIMD_OP(kExprS128Select),
        WASM_SIMD_OP(kExprF32x4ExtractLane), 0);
  // Selection mask is all 0, so always select 2.0.
  CHECK_EQ(2.0, r.Call(1, 2.0, 0));
}

WASM_SIMD_TEST(S128AndNotWithF32x4) {
  WasmRunner<float, int32_t, float> r(execution_tier, lower_simd);
  BUILD(r, WASM_GET_LOCAL(0), WASM_SIMD_OP(kExprI32x4Splat), WASM_GET_LOCAL(1),
        WASM_SIMD_OP(kExprF32x4Splat), WASM_SIMD_OP(kExprS128AndNot),
        WASM_SIMD_OP(kExprF32x4ExtractLane), 0);
  // 0x00700000 & !0x40800000 = 0x00700000
  CHECK_EQ(bit_cast<float>(0x700000),
           r.Call(0x00700000, bit_cast<float>(0x40800000)));
}

WASM_SIMD_TEST(FunctionCallWithExtractLaneOutputAsArgument) {
  // This uses the result of an extract lane as an argument to a function call
  // to exercise lowering for kCall and make sure the the extract lane is
  // correctly replaced with a scalar.
  TestSignatures sigs;
  WasmRunner<int32_t, int32_t> r(execution_tier, lower_simd);
  WasmFunctionCompiler& fn = r.NewFunction(sigs.f_f());

  BUILD(fn, WASM_GET_LOCAL(0), WASM_GET_LOCAL(0), kExprF32Add);

  BUILD(r, WASM_GET_LOCAL(0), WASM_SIMD_OP(kExprI32x4Splat),
        WASM_SIMD_OP(kExprF32x4ExtractLane), 0, kExprCallFunction,
        fn.function_index(), WASM_SIMD_OP(kExprF32x4Splat), WASM_GET_LOCAL(0),
        WASM_SIMD_OP(kExprI32x4Splat), WASM_SIMD_OP(kExprI32x4Add),
        WASM_SIMD_OP(kExprI32x4ExtractLane), 0);
  CHECK_EQ(15, r.Call(5));
}

}  // namespace test_run_wasm_simd
}  // namespace wasm
}  // namespace internal
}  // namespace v8

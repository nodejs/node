// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_WASM_OPCODES_INL_H_
#define V8_WASM_WASM_OPCODES_INL_H_

#include <array>

#include "src/base/template-utils.h"
#include "src/codegen/signature.h"
#include "src/execution/messages.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-opcodes.h"

namespace v8 {
namespace internal {
namespace wasm {

#define CASE_OP(name, str) \
  case kExpr##name:        \
    return str;
#define CASE_I32_OP(name, str) CASE_OP(I32##name, "i32." str)
#define CASE_I64_OP(name, str) CASE_OP(I64##name, "i64." str)
#define CASE_F32_OP(name, str) CASE_OP(F32##name, "f32." str)
#define CASE_F64_OP(name, str) CASE_OP(F64##name, "f64." str)
#define CASE_REF_OP(name, str) CASE_OP(Ref##name, "ref." str)
#define CASE_F64x2_OP(name, str) CASE_OP(F64x2##name, "f64x2." str)
#define CASE_F32x4_OP(name, str) CASE_OP(F32x4##name, "f32x4." str)
#define CASE_I64x2_OP(name, str) CASE_OP(I64x2##name, "i64x2." str)
#define CASE_I32x4_OP(name, str) CASE_OP(I32x4##name, "i32x4." str)
#define CASE_I16x8_OP(name, str) CASE_OP(I16x8##name, "i16x8." str)
#define CASE_I8x16_OP(name, str) CASE_OP(I8x16##name, "i8x16." str)
#define CASE_S128_OP(name, str) CASE_OP(S128##name, "s128." str)
#define CASE_V128_OP(name, str) CASE_OP(V128##name, "v128." str)
#define CASE_S64x2_OP(name, str) CASE_OP(S64x2##name, "s64x2." str)
#define CASE_S32x4_OP(name, str) CASE_OP(S32x4##name, "s32x4." str)
#define CASE_S16x8_OP(name, str) CASE_OP(S16x8##name, "s16x8." str)
#define CASE_V64x2_OP(name, str) CASE_OP(V64x2##name, "v64x2." str)
#define CASE_V32x4_OP(name, str) CASE_OP(V32x4##name, "v32x4." str)
#define CASE_V16x8_OP(name, str) CASE_OP(V16x8##name, "v16x8." str)
#define CASE_V8x16_OP(name, str) CASE_OP(V8x16##name, "v8x16." str)
#define CASE_INT_OP(name, str) CASE_I32_OP(name, str) CASE_I64_OP(name, str)
#define CASE_FLOAT_OP(name, str) CASE_F32_OP(name, str) CASE_F64_OP(name, str)
#define CASE_ALL_OP(name, str) CASE_FLOAT_OP(name, str) CASE_INT_OP(name, str)
#define CASE_SIMD_OP(name, str)                                              \
  CASE_F64x2_OP(name, str) CASE_I64x2_OP(name, str) CASE_F32x4_OP(name, str) \
      CASE_I32x4_OP(name, str) CASE_I16x8_OP(name, str)                      \
          CASE_I8x16_OP(name, str)
#define CASE_SIMDF_OP(name, str) \
  CASE_F32x4_OP(name, str) CASE_F64x2_OP(name, str)
#define CASE_SIMDI_OP(name, str)                                             \
  CASE_I64x2_OP(name, str) CASE_I32x4_OP(name, str) CASE_I16x8_OP(name, str) \
      CASE_I8x16_OP(name, str)
#define CASE_SIMDI_NO64X2_OP(name, str) \
  CASE_I32x4_OP(name, str) CASE_I16x8_OP(name, str) CASE_I8x16_OP(name, str)
#define CASE_SIGN_OP(TYPE, name, str) \
  CASE_##TYPE##_OP(name##S, str "_s") CASE_##TYPE##_OP(name##U, str "_u")
#define CASE_UNSIGNED_OP(TYPE, name, str) CASE_##TYPE##_OP(name##U, str "_u")
#define CASE_ALL_SIGN_OP(name, str) \
  CASE_FLOAT_OP(name, str) CASE_SIGN_OP(INT, name, str)
#define CASE_CONVERT_OP(name, RES, SRC, src_suffix, str) \
  CASE_##RES##_OP(U##name##SRC, str "_" src_suffix "_u") \
      CASE_##RES##_OP(S##name##SRC, str "_" src_suffix "_s")
#define CASE_CONVERT_SAT_OP(name, RES, SRC, src_suffix, str)      \
  CASE_##RES##_OP(U##name##Sat##SRC, str "_sat_" src_suffix "_u") \
      CASE_##RES##_OP(S##name##Sat##SRC, str "_sat_" src_suffix "_s")
#define CASE_L32_OP(name, str)          \
  CASE_SIGN_OP(I32, name##8, str "8")   \
  CASE_SIGN_OP(I32, name##16, str "16") \
  CASE_I32_OP(name, str "32")
#define CASE_U32_OP(name, str)            \
  CASE_I32_OP(name, str "32")             \
  CASE_UNSIGNED_OP(I32, name##8, str "8") \
  CASE_UNSIGNED_OP(I32, name##16, str "16")
#define CASE_UNSIGNED_ALL_OP(name, str)     \
  CASE_U32_OP(name, str)                    \
  CASE_I64_OP(name, str "64")               \
  CASE_UNSIGNED_OP(I64, name##8, str "8")   \
  CASE_UNSIGNED_OP(I64, name##16, str "16") \
  CASE_UNSIGNED_OP(I64, name##32, str "32")

// static
constexpr const char* WasmOpcodes::OpcodeName(WasmOpcode opcode) {
  switch (opcode) {
    // clang-format off

    // Standard opcodes
    CASE_INT_OP(Eqz, "eqz")
    CASE_ALL_OP(Eq, "eq")
    CASE_ALL_OP(Ne, "ne")
    CASE_ALL_OP(Add, "add")
    CASE_ALL_OP(Sub, "sub")
    CASE_ALL_OP(Mul, "mul")
    CASE_ALL_SIGN_OP(Lt, "lt")
    CASE_ALL_SIGN_OP(Gt, "gt")
    CASE_ALL_SIGN_OP(Le, "le")
    CASE_ALL_SIGN_OP(Ge, "ge")
    CASE_INT_OP(Clz, "clz")
    CASE_INT_OP(Ctz, "ctz")
    CASE_INT_OP(Popcnt, "popcnt")
    CASE_ALL_SIGN_OP(Div, "div")
    CASE_SIGN_OP(INT, Rem, "rem")
    CASE_INT_OP(And, "and")
    CASE_INT_OP(Ior, "or")
    CASE_INT_OP(Xor, "xor")
    CASE_INT_OP(Shl, "shl")
    CASE_SIGN_OP(INT, Shr, "shr")
    CASE_INT_OP(Rol, "rol")
    CASE_INT_OP(Ror, "ror")
    CASE_FLOAT_OP(Abs, "abs")
    CASE_FLOAT_OP(Neg, "neg")
    CASE_FLOAT_OP(Ceil, "ceil")
    CASE_FLOAT_OP(Floor, "floor")
    CASE_FLOAT_OP(Trunc, "trunc")
    CASE_FLOAT_OP(NearestInt, "nearest")
    CASE_FLOAT_OP(Sqrt, "sqrt")
    CASE_FLOAT_OP(Min, "min")
    CASE_FLOAT_OP(Max, "max")
    CASE_FLOAT_OP(CopySign, "copysign")
    CASE_REF_OP(Null, "null")
    CASE_REF_OP(IsNull, "is_null")
    CASE_REF_OP(Func, "func")
    CASE_REF_OP(AsNonNull, "as_non_null")
    CASE_I32_OP(ConvertI64, "wrap_i64")
    CASE_CONVERT_OP(Convert, INT, F32, "f32", "trunc")
    CASE_CONVERT_OP(Convert, INT, F64, "f64", "trunc")
    CASE_CONVERT_OP(Convert, I64, I32, "i32", "extend")
    CASE_CONVERT_OP(Convert, F32, I32, "i32", "convert")
    CASE_CONVERT_OP(Convert, F32, I64, "i64", "convert")
    CASE_F32_OP(ConvertF64, "demote_f64")
    CASE_CONVERT_OP(Convert, F64, I32, "i32", "convert")
    CASE_CONVERT_OP(Convert, F64, I64, "i64", "convert")
    CASE_F64_OP(ConvertF32, "promote_f32")
    CASE_I32_OP(ReinterpretF32, "reinterpret_f32")
    CASE_I64_OP(ReinterpretF64, "reinterpret_f64")
    CASE_F32_OP(ReinterpretI32, "reinterpret_i32")
    CASE_F64_OP(ReinterpretI64, "reinterpret_i64")
    CASE_INT_OP(SExtendI8, "extend8_s")
    CASE_INT_OP(SExtendI16, "extend16_s")
    CASE_I64_OP(SExtendI32, "extend32_s")
    CASE_OP(Unreachable, "unreachable")
    CASE_OP(Nop, "nop")
    CASE_OP(NopForTestingUnsupportedInLiftoff, "nop_for_testing")
    CASE_OP(Block, "block")
    CASE_OP(Loop, "loop")
    CASE_OP(If, "if")
    CASE_OP(Else, "else")
    CASE_OP(End, "end")
    CASE_OP(Br, "br")
    CASE_OP(BrIf, "br_if")
    CASE_OP(BrTable, "br_table")
    CASE_OP(Return, "return")
    CASE_OP(CallFunction, "call")
    CASE_OP(CallIndirect, "call_indirect")
    CASE_OP(ReturnCall, "return_call")
    CASE_OP(ReturnCallIndirect, "return_call_indirect")
    CASE_OP(CallRef, "call_ref")
    CASE_OP(ReturnCallRef, "return_call_ref")
    CASE_OP(BrOnNull, "br_on_null")
    CASE_OP(BrOnNonNull, "br_on_non_null")
    CASE_OP(Drop, "drop")
    CASE_OP(Select, "select")
    CASE_OP(SelectWithType, "select")
    CASE_OP(LocalGet, "local.get")
    CASE_OP(LocalSet, "local.set")
    CASE_OP(LocalTee, "local.tee")
    CASE_OP(GlobalGet, "global.get")
    CASE_OP(GlobalSet, "global.set")
    CASE_OP(TableGet, "table.get")
    CASE_OP(TableSet, "table.set")
    CASE_ALL_OP(Const, "const")
    CASE_OP(MemorySize, "memory.size")
    CASE_OP(MemoryGrow, "memory.grow")
    CASE_ALL_OP(LoadMem, "load")
    CASE_SIGN_OP(INT, LoadMem8, "load8")
    CASE_SIGN_OP(INT, LoadMem16, "load16")
    CASE_SIGN_OP(I64, LoadMem32, "load32")
    CASE_S128_OP(LoadMem, "load128")
    CASE_S128_OP(Const, "const")
    CASE_ALL_OP(StoreMem, "store")
    CASE_INT_OP(StoreMem8, "store8")
    CASE_INT_OP(StoreMem16, "store16")
    CASE_I64_OP(StoreMem32, "store32")
    CASE_S128_OP(StoreMem, "store128")
    CASE_OP(RefEq, "ref.eq")
    CASE_OP(Let, "let")

    // Exception handling opcodes.
    CASE_OP(Try, "try")
    CASE_OP(Catch, "catch")
    CASE_OP(Delegate, "delegate")
    CASE_OP(Throw, "throw")
    CASE_OP(Rethrow, "rethrow")
    CASE_OP(CatchAll, "catch-all")

    // asm.js-only opcodes.
    CASE_F64_OP(Acos, "acos")
    CASE_F64_OP(Asin, "asin")
    CASE_F64_OP(Atan, "atan")
    CASE_F64_OP(Cos, "cos")
    CASE_F64_OP(Sin, "sin")
    CASE_F64_OP(Tan, "tan")
    CASE_F64_OP(Exp, "exp")
    CASE_F64_OP(Log, "log")
    CASE_F64_OP(Atan2, "atan2")
    CASE_F64_OP(Pow, "pow")
    CASE_F64_OP(Mod, "mod")
    CASE_F32_OP(AsmjsLoadMem, "asmjs_load")
    CASE_F64_OP(AsmjsLoadMem, "asmjs_load")
    CASE_L32_OP(AsmjsLoadMem, "asmjs_load")
    CASE_I32_OP(AsmjsStoreMem, "asmjs_store")
    CASE_F32_OP(AsmjsStoreMem, "asmjs_store")
    CASE_F64_OP(AsmjsStoreMem, "asmjs_store")
    CASE_I32_OP(AsmjsStoreMem8, "asmjs_store8")
    CASE_I32_OP(AsmjsStoreMem16, "asmjs_store16")
    CASE_SIGN_OP(I32, AsmjsDiv, "asmjs_div")
    CASE_SIGN_OP(I32, AsmjsRem, "asmjs_rem")
    CASE_I32_OP(AsmjsSConvertF32, "asmjs_convert_f32_s")
    CASE_I32_OP(AsmjsUConvertF32, "asmjs_convert_f32_u")
    CASE_I32_OP(AsmjsSConvertF64, "asmjs_convert_f64_s")
    CASE_I32_OP(AsmjsUConvertF64, "asmjs_convert_f64_u")

    // Numeric Opcodes.
    CASE_CONVERT_SAT_OP(Convert, I32, F32, "f32", "trunc")
    CASE_CONVERT_SAT_OP(Convert, I32, F64, "f64", "trunc")
    CASE_CONVERT_SAT_OP(Convert, I64, F32, "f32", "trunc")
    CASE_CONVERT_SAT_OP(Convert, I64, F64, "f64", "trunc")
    CASE_OP(MemoryInit, "memory.init")
    CASE_OP(DataDrop, "data.drop")
    CASE_OP(MemoryCopy, "memory.copy")
    CASE_OP(MemoryFill, "memory.fill")
    CASE_OP(TableInit, "table.init")
    CASE_OP(ElemDrop, "elem.drop")
    CASE_OP(TableCopy, "table.copy")
    CASE_OP(TableGrow, "table.grow")
    CASE_OP(TableSize, "table.size")
    CASE_OP(TableFill, "table.fill")

    // SIMD opcodes.
    CASE_SIMD_OP(Splat, "splat")
    CASE_SIMD_OP(Neg, "neg")
    CASE_SIMDF_OP(Sqrt, "sqrt")
    CASE_SIMD_OP(Eq, "eq")
    CASE_SIMD_OP(Ne, "ne")
    CASE_SIMD_OP(Add, "add")
    CASE_SIMD_OP(Sub, "sub")
    CASE_I16x8_OP(Mul, "mul")
    CASE_I32x4_OP(Mul, "mul")
    CASE_I64x2_OP(Mul, "mul")
    CASE_SIMDF_OP(Mul, "mul")
    CASE_SIMDF_OP(Div, "div")
    CASE_SIMDF_OP(Lt, "lt")
    CASE_SIMDF_OP(Le, "le")
    CASE_SIMDF_OP(Gt, "gt")
    CASE_SIMDF_OP(Ge, "ge")
    CASE_SIMDF_OP(Abs, "abs")
    CASE_SIMDF_OP(Min, "min")
    CASE_SIMDF_OP(Max, "max")
    CASE_CONVERT_OP(Convert, F32x4, I32x4, "i32x4", "convert")
    CASE_CONVERT_OP(Convert, I32x4, F32x4, "f32x4", "convert")
    CASE_CONVERT_OP(Convert, I32x4, I16x8Low, "i16x8_low", "convert")
    CASE_CONVERT_OP(Convert, I32x4, I16x8High, "i16x8_high", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I32x4, "i32x4", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I8x16Low, "i8x16_low", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I8x16High, "i8x16_high", "convert")
    CASE_CONVERT_OP(Convert, I8x16, I16x8, "i16x8", "convert")
    CASE_SIMDF_OP(ExtractLane, "extract_lane")
    CASE_SIMDF_OP(ReplaceLane, "replace_lane")
    CASE_I64x2_OP(ExtractLane, "extract_lane")
    CASE_I32x4_OP(ExtractLane, "extract_lane")
    CASE_SIGN_OP(I16x8, ExtractLane, "extract_lane")
    CASE_SIGN_OP(I8x16, ExtractLane, "extract_lane")
    CASE_SIMDI_OP(ReplaceLane, "replace_lane")
    CASE_SIGN_OP(SIMDI_NO64X2, Min, "min")
    CASE_SIGN_OP(SIMDI_NO64X2, Max, "max")
    CASE_SIGN_OP(SIMDI_NO64X2, Lt, "lt")
    CASE_I64x2_OP(LtS, "lt_s")
    CASE_I64x2_OP(GtS, "gt_s")
    CASE_I64x2_OP(LeS, "le_s")
    CASE_I64x2_OP(GeS, "ge_s")
    CASE_SIGN_OP(SIMDI_NO64X2, Le, "le")
    CASE_SIGN_OP(SIMDI_NO64X2, Gt, "gt")
    CASE_SIGN_OP(SIMDI_NO64X2, Ge, "ge")
    CASE_CONVERT_OP(Convert, I64x2, I32x4Low, "i32x4_low", "convert")
    CASE_CONVERT_OP(Convert, I64x2, I32x4High, "i32x4_high", "convert")
    CASE_SIGN_OP(SIMDI, Shr, "shr")
    CASE_SIMDI_OP(Shl, "shl")
    CASE_SIGN_OP(I16x8, AddSat, "add_sat")
    CASE_SIGN_OP(I8x16, AddSat, "add_sat")
    CASE_SIGN_OP(I16x8, SubSat, "sub_sat")
    CASE_SIGN_OP(I8x16, SubSat, "sub_sat")
    CASE_S128_OP(And, "and")
    CASE_S128_OP(Or, "or")
    CASE_S128_OP(Xor, "xor")
    CASE_S128_OP(Not, "not")
    CASE_S128_OP(Select, "select")
    CASE_S128_OP(AndNot, "andnot")
    CASE_I8x16_OP(Swizzle, "swizzle")
    CASE_I8x16_OP(Shuffle, "shuffle")
    CASE_V128_OP(AnyTrue, "any_true")
    CASE_SIMDI_OP(AllTrue, "all_true")

    CASE_S128_OP(Load32Zero, "load32_zero")
    CASE_S128_OP(Load64Zero, "load64_zero")
    CASE_S128_OP(Load8Splat, "load8_splat")
    CASE_S128_OP(Load16Splat, "load16_splat")
    CASE_S128_OP(Load32Splat, "load32_splat")
    CASE_S128_OP(Load64Splat, "load64_splat")
    CASE_S128_OP(Load8x8S, "load8x8_s")
    CASE_S128_OP(Load8x8U, "load8x8_u")
    CASE_S128_OP(Load16x4S, "load16x4_s")
    CASE_S128_OP(Load16x4U, "load16x4_u")
    CASE_S128_OP(Load32x2S, "load32x2_s")
    CASE_S128_OP(Load32x2U, "load32x2_u")
    CASE_S128_OP(Load8Lane, "load8_lane")
    CASE_S128_OP(Load16Lane, "load16_lane")
    CASE_S128_OP(Load32Lane, "load32_lane")
    CASE_S128_OP(Load64Lane, "load64_lane")
    CASE_S128_OP(Store8Lane, "store8_lane")
    CASE_S128_OP(Store16Lane, "store16_lane")
    CASE_S128_OP(Store32Lane, "store32_lane")
    CASE_S128_OP(Store64Lane, "store64_lane")

    CASE_I8x16_OP(RoundingAverageU, "avgr_u")
    CASE_I16x8_OP(RoundingAverageU, "avgr_u")
    CASE_I16x8_OP(Q15MulRSatS, "q15mulr_sat_s")

    CASE_SIMDI_OP(Abs, "abs")
    CASE_SIMDI_OP(BitMask, "bitmask")
    CASE_I8x16_OP(Popcnt, "popcnt")


    CASE_SIMDF_OP(Pmin, "pmin")
    CASE_SIMDF_OP(Pmax, "pmax")

    CASE_SIMDF_OP(Ceil, "ceil")
    CASE_SIMDF_OP(Floor, "floor")
    CASE_SIMDF_OP(Trunc, "trunc")
    CASE_SIMDF_OP(NearestInt, "nearest")

    CASE_I32x4_OP(DotI16x8S, "dot_i16x8_s")

    CASE_SIGN_OP(I16x8, ExtMulLowI8x16, "extmul_low_i8x16")
    CASE_SIGN_OP(I16x8, ExtMulHighI8x16, "extmul_high_i8x16")
    CASE_SIGN_OP(I32x4, ExtMulLowI16x8, "extmul_low_i16x8")
    CASE_SIGN_OP(I32x4, ExtMulHighI16x8, "extmul_high_i16x8")
    CASE_SIGN_OP(I64x2, ExtMulLowI32x4, "extmul_low_i32x4")
    CASE_SIGN_OP(I64x2, ExtMulHighI32x4, "extmul_high_i32x4")

    CASE_SIGN_OP(I32x4, ExtAddPairwiseI16x8, "extadd_pairwise_i16x8")
    CASE_SIGN_OP(I16x8, ExtAddPairwiseI8x16, "extadd_pairwise_i8x6")

    CASE_F64x2_OP(ConvertLowI32x4S, "convert_low_i32x4_s")
    CASE_F64x2_OP(ConvertLowI32x4U, "convert_low_i32x4_u")
    CASE_I32x4_OP(TruncSatF64x2SZero, "trunc_sat_f64x2_s_zero")
    CASE_I32x4_OP(TruncSatF64x2UZero, "trunc_sat_f64x2_u_zero")
    CASE_F32x4_OP(DemoteF64x2Zero, "demote_f64x2_zero")
    CASE_F64x2_OP(PromoteLowF32x4, "promote_low_f32x4")

    // Relaxed SIMD opcodes.
    CASE_F32x4_OP(RecipApprox, "recip_approx")
    CASE_F32x4_OP(RecipSqrtApprox, "recip_sqrt_approx")
    CASE_SIMDF_OP(Qfma, "qfma")
    CASE_SIMDF_OP(Qfms, "qfms")
    CASE_I8x16_OP(RelaxedSwizzle, "relaxed_swizzle");
    CASE_I8x16_OP(RelaxedLaneSelect, "relaxed_laneselect");
    CASE_I16x8_OP(RelaxedLaneSelect, "relaxed_laneselect");
    CASE_I32x4_OP(RelaxedLaneSelect, "relaxed_laneselect");
    CASE_I64x2_OP(RelaxedLaneSelect, "relaxed_laneselect");
    CASE_SIMDF_OP(RelaxedMin, "relaxed_min");
    CASE_SIMDF_OP(RelaxedMax, "relaxed_max");
    CASE_I32x4_OP(RelaxedTruncF32x4S, "relaxed_trunc_f32x4_s");
    CASE_I32x4_OP(RelaxedTruncF32x4U, "relaxed_trunc_f32x4_u");
    CASE_I32x4_OP(RelaxedTruncF64x2SZero, "relaxed_trunc_f64x2_s_zero");
    CASE_I32x4_OP(RelaxedTruncF64x2UZero, "relaxed_trunc_f64x2_u_zero");

    // Atomic operations.
    CASE_OP(AtomicNotify, "atomic.notify")
    CASE_INT_OP(AtomicWait, "atomic.wait")
    CASE_OP(AtomicFence, "atomic.fence")
    CASE_UNSIGNED_ALL_OP(AtomicLoad, "atomic.load")
    CASE_UNSIGNED_ALL_OP(AtomicStore, "atomic.store")
    CASE_UNSIGNED_ALL_OP(AtomicAdd, "atomic.add")
    CASE_UNSIGNED_ALL_OP(AtomicSub, "atomic.sub")
    CASE_UNSIGNED_ALL_OP(AtomicAnd, "atomic.and")
    CASE_UNSIGNED_ALL_OP(AtomicOr, "atomic.or")
    CASE_UNSIGNED_ALL_OP(AtomicXor, "atomic.xor")
    CASE_UNSIGNED_ALL_OP(AtomicExchange, "atomic.xchng")
    CASE_UNSIGNED_ALL_OP(AtomicCompareExchange, "atomic.cmpxchng")

    // GC operations.
    CASE_OP(StructNewWithRtt, "struct.new_with_rtt")
    CASE_OP(StructNewDefaultWithRtt, "struct.new_default_with_rtt")
    CASE_OP(StructNew, "struct.new")
    CASE_OP(StructNewDefault, "struct.new_default")
    CASE_OP(StructGet, "struct.get")
    CASE_OP(StructGetS, "struct.get_s")
    CASE_OP(StructGetU, "struct.get_u")
    CASE_OP(StructSet, "struct.set")
    CASE_OP(ArrayNewWithRtt, "array.new_with_rtt")
    CASE_OP(ArrayNewDefaultWithRtt, "array.new_default_with_rtt")
    CASE_OP(ArrayNew, "array.new")
    CASE_OP(ArrayNewDefault, "array.new_default")
    CASE_OP(ArrayGet, "array.get")
    CASE_OP(ArrayGetS, "array.get_s")
    CASE_OP(ArrayGetU, "array.get_u")
    CASE_OP(ArraySet, "array.set")
    CASE_OP(ArrayLen, "array.len")
    CASE_OP(ArrayCopy, "array.copy")
    CASE_OP(ArrayInit, "array.init")
    CASE_OP(ArrayInitStatic, "array.init_static")
    CASE_OP(ArrayInitFromData, "array.init_from_data")
    CASE_OP(ArrayInitFromDataStatic, "array.init_from_data_static")
    CASE_OP(I31New, "i31.new")
    CASE_OP(I31GetS, "i31.get_s")
    CASE_OP(I31GetU, "i31.get_u")
    CASE_OP(RttCanon, "rtt.canon")
    CASE_OP(RefTest, "ref.test")
    CASE_OP(RefTestStatic, "ref.test_static")
    CASE_OP(RefCast, "ref.cast")
    CASE_OP(RefCastStatic, "ref.cast_static")
    CASE_OP(BrOnCast, "br_on_cast")
    CASE_OP(BrOnCastStatic, "br_on_cast_static")
    CASE_OP(BrOnCastFail, "br_on_cast_fail")
    CASE_OP(BrOnCastStaticFail, "br_on_cast_static_fail")
    CASE_OP(RefIsFunc, "ref.is_func")
    CASE_OP(RefIsData, "ref.is_data")
    CASE_OP(RefIsI31, "ref.is_i31")
    CASE_OP(RefIsArray, "ref.is_array")
    CASE_OP(RefAsFunc, "ref.as_func")
    CASE_OP(RefAsData, "ref.as_data")
    CASE_OP(RefAsI31, "ref.as_i31")
    CASE_OP(RefAsArray, "ref.as_array")
    CASE_OP(BrOnFunc, "br_on_func")
    CASE_OP(BrOnData, "br_on_data")
    CASE_OP(BrOnI31, "br_on_i31")
    CASE_OP(BrOnArray, "br_on_array")
    CASE_OP(BrOnNonFunc, "br_on_non_func")
    CASE_OP(BrOnNonData, "br_on_non_data")
    CASE_OP(BrOnNonI31, "br_on_non_i31")
    CASE_OP(BrOnNonArray, "br_on_non_array")

    case kNumericPrefix:
    case kSimdPrefix:
    case kAtomicPrefix:
    case kGCPrefix:
      return "unknown";
    // clang-format on
  }
  // Even though the switch above handles all well-defined enum values,
  // random modules (e.g. fuzzer generated) can call this function with
  // random (invalid) opcodes. Handle those here:
  return "invalid opcode";
}

#undef CASE_OP
#undef CASE_I32_OP
#undef CASE_I64_OP
#undef CASE_F32_OP
#undef CASE_F64_OP
#undef CASE_REF_OP
#undef CASE_F64x2_OP
#undef CASE_F32x4_OP
#undef CASE_I64x2_OP
#undef CASE_I32x4_OP
#undef CASE_I16x8_OP
#undef CASE_I8x16_OP
#undef CASE_S128_OP
#undef CASE_S64x2_OP
#undef CASE_S32x4_OP
#undef CASE_S16x8_OP
#undef CASE_INT_OP
#undef CASE_FLOAT_OP
#undef CASE_ALL_OP
#undef CASE_SIMD_OP
#undef CASE_SIMDI_OP
#undef CASE_SIMDI_NO64X2_OP
#undef CASE_SIGN_OP
#undef CASE_UNSIGNED_OP
#undef CASE_UNSIGNED_ALL_OP
#undef CASE_ALL_SIGN_OP
#undef CASE_CONVERT_OP
#undef CASE_CONVERT_SAT_OP
#undef CASE_L32_OP
#undef CASE_U32_OP

// static
constexpr bool WasmOpcodes::IsPrefixOpcode(WasmOpcode opcode) {
  switch (opcode) {
#define CHECK_PREFIX(name, opcode) case k##name##Prefix:
    FOREACH_PREFIX(CHECK_PREFIX)
#undef CHECK_PREFIX
    return true;
    default:
      return false;
  }
}

// static
constexpr bool WasmOpcodes::IsControlOpcode(WasmOpcode opcode) {
  switch (opcode) {
#define CHECK_OPCODE(name, opcode, _) case kExpr##name:
    FOREACH_CONTROL_OPCODE(CHECK_OPCODE)
#undef CHECK_OPCODE
    return true;
    default:
      return false;
  }
}

// static
constexpr bool WasmOpcodes::IsUnconditionalJump(WasmOpcode opcode) {
  switch (opcode) {
    case kExprUnreachable:
    case kExprBr:
    case kExprBrTable:
    case kExprReturn:
    case kExprReturnCall:
    case kExprReturnCallIndirect:
    case kExprThrow:
    case kExprRethrow:
      return true;
    default:
      return false;
  }
}

// static
constexpr bool WasmOpcodes::IsBreakable(WasmOpcode opcode) {
  switch (opcode) {
    case kExprBlock:
    case kExprTry:
    case kExprCatch:
    case kExprLoop:
    case kExprElse:
      return false;
    default:
      return true;
  }
}

// static
constexpr bool WasmOpcodes::IsExternRefOpcode(WasmOpcode opcode) {
  switch (opcode) {
    case kExprRefNull:
    case kExprRefIsNull:
    case kExprRefFunc:
    case kExprRefAsNonNull:
      return true;
    default:
      return false;
  }
}

// static
constexpr bool WasmOpcodes::IsThrowingOpcode(WasmOpcode opcode) {
  // TODO(8729): Trapping opcodes are not yet considered to be throwing.
  switch (opcode) {
    case kExprThrow:
    case kExprRethrow:
    case kExprCallFunction:
    case kExprCallIndirect:
      return true;
    default:
      return false;
  }
}

// static
constexpr bool WasmOpcodes::IsRelaxedSimdOpcode(WasmOpcode opcode) {
  switch (opcode) {
#define CHECK_OPCODE(name, opcode, _) case kExpr##name:
    FOREACH_RELAXED_SIMD_OPCODE(CHECK_OPCODE)
#undef CHECK_OPCODE
    return true;
    default:
      return false;
  }
}

namespace impl {

#define DECLARE_SIG_ENUM(name, ...) kSigEnum_##name,
enum WasmOpcodeSig : byte {
  kSigEnum_None,
  FOREACH_SIGNATURE(DECLARE_SIG_ENUM)
};
#undef DECLARE_SIG_ENUM
#define DECLARE_SIG(name, ...)                                                \
  constexpr ValueType kTypes_##name[] = {__VA_ARGS__};                        \
  constexpr int kReturnsCount_##name = kTypes_##name[0] == kWasmVoid ? 0 : 1; \
  constexpr FunctionSig kSig_##name(                                          \
      kReturnsCount_##name, static_cast<int>(arraysize(kTypes_##name)) - 1,   \
      kTypes_##name + (1 - kReturnsCount_##name));
FOREACH_SIGNATURE(DECLARE_SIG)
#undef DECLARE_SIG

#define DECLARE_SIG_ENTRY(name, ...) &kSig_##name,
constexpr const FunctionSig* kCachedSigs[] = {
    nullptr, FOREACH_SIGNATURE(DECLARE_SIG_ENTRY)};
#undef DECLARE_SIG_ENTRY

constexpr WasmOpcodeSig GetShortOpcodeSigIndex(byte opcode) {
#define CASE(name, opc, sig) opcode == opc ? kSigEnum_##sig:
  return FOREACH_SIMPLE_OPCODE(CASE) FOREACH_SIMPLE_PROTOTYPE_OPCODE(CASE)
      kSigEnum_None;
#undef CASE
}

constexpr WasmOpcodeSig GetAsmJsOpcodeSigIndex(byte opcode) {
#define CASE(name, opc, sig) opcode == opc ? kSigEnum_##sig:
  return FOREACH_ASMJS_COMPAT_OPCODE(CASE) kSigEnum_None;
#undef CASE
}

constexpr WasmOpcodeSig GetSimdOpcodeSigIndex(byte opcode) {
#define CASE(name, opc, sig) opcode == (opc & 0xFF) ? kSigEnum_##sig:
  return FOREACH_SIMD_0_OPERAND_OPCODE(CASE) FOREACH_SIMD_MEM_OPCODE(CASE)
      FOREACH_SIMD_MEM_1_OPERAND_OPCODE(CASE) kSigEnum_None;
#undef CASE
}

constexpr WasmOpcodeSig GetAtomicOpcodeSigIndex(byte opcode) {
#define CASE(name, opc, sig) opcode == (opc & 0xFF) ? kSigEnum_##sig:
  return FOREACH_ATOMIC_OPCODE(CASE) FOREACH_ATOMIC_0_OPERAND_OPCODE(CASE)
      kSigEnum_None;
#undef CASE
}

constexpr WasmOpcodeSig GetNumericOpcodeSigIndex(byte opcode) {
#define CASE_SIG(name, opc, sig) opcode == (opc & 0xFF) ? kSigEnum_##sig:
#define CASE_VARIADIC(name, opc)
  return FOREACH_NUMERIC_OPCODE(CASE_SIG, CASE_VARIADIC) kSigEnum_None;
#undef CASE_SIG
#undef CASE_VARIADIC
}

constexpr std::array<WasmOpcodeSig, 256> kShortSigTable =
    base::make_array<256>(GetShortOpcodeSigIndex);
constexpr std::array<WasmOpcodeSig, 256> kSimpleAsmjsExprSigTable =
    base::make_array<256>(GetAsmJsOpcodeSigIndex);
constexpr std::array<WasmOpcodeSig, 256> kSimdExprSigTable =
    base::make_array<256>(GetSimdOpcodeSigIndex);
constexpr std::array<WasmOpcodeSig, 256> kAtomicExprSigTable =
    base::make_array<256>(GetAtomicOpcodeSigIndex);
constexpr std::array<WasmOpcodeSig, 256> kNumericExprSigTable =
    base::make_array<256>(GetNumericOpcodeSigIndex);

}  // namespace impl

constexpr const FunctionSig* WasmOpcodes::Signature(WasmOpcode opcode) {
  switch (opcode >> 8) {
    case 0:
      return impl::kCachedSigs[impl::kShortSigTable[opcode]];
    case kSimdPrefix:
      return impl::kCachedSigs[impl::kSimdExprSigTable[opcode & 0xFF]];
    case kAtomicPrefix:
      return impl::kCachedSigs[impl::kAtomicExprSigTable[opcode & 0xFF]];
    case kNumericPrefix:
      return impl::kCachedSigs[impl::kNumericExprSigTable[opcode & 0xFF]];
    default:
      UNREACHABLE();  // invalid prefix.
  }
}

constexpr const FunctionSig* WasmOpcodes::AsmjsSignature(WasmOpcode opcode) {
  DCHECK_GT(impl::kSimpleAsmjsExprSigTable.size(), opcode);
  return impl::kCachedSigs[impl::kSimpleAsmjsExprSigTable[opcode]];
}

constexpr MessageTemplate WasmOpcodes::TrapReasonToMessageId(
    TrapReason reason) {
  switch (reason) {
#define TRAPREASON_TO_MESSAGE(name) \
  case k##name:                     \
    return MessageTemplate::kWasm##name;
    FOREACH_WASM_TRAPREASON(TRAPREASON_TO_MESSAGE)
#undef TRAPREASON_TO_MESSAGE
    default:
      return MessageTemplate::kNone;
  }
}

const char* WasmOpcodes::TrapReasonMessage(TrapReason reason) {
  return MessageFormatter::TemplateString(TrapReasonToMessageId(reason));
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_OPCODES_INL_H_

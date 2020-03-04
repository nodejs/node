// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-opcodes.h"

#include <array>

#include "src/base/template-utils.h"
#include "src/codegen/signature.h"
#include "src/execution/messages.h"
#include "src/runtime/runtime.h"
#include "src/wasm/wasm-features.h"

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
#define CASE_S64x2_OP(name, str) CASE_OP(S64x2##name, "s64x2." str)
#define CASE_S32x4_OP(name, str) CASE_OP(S32x4##name, "s32x4." str)
#define CASE_S16x8_OP(name, str) CASE_OP(S16x8##name, "s16x8." str)
#define CASE_S8x16_OP(name, str) CASE_OP(S8x16##name, "s8x16." str)
#define CASE_S1x2_OP(name, str) CASE_OP(S1x2##name, "s1x2." str)
#define CASE_S1x4_OP(name, str) CASE_OP(S1x4##name, "s1x4." str)
#define CASE_S1x8_OP(name, str) CASE_OP(S1x8##name, "s1x8." str)
#define CASE_S1x16_OP(name, str) CASE_OP(S1x16##name, "s1x16." str)
#define CASE_INT_OP(name, str) CASE_I32_OP(name, str) CASE_I64_OP(name, str)
#define CASE_FLOAT_OP(name, str) CASE_F32_OP(name, str) CASE_F64_OP(name, str)
#define CASE_ALL_OP(name, str) CASE_FLOAT_OP(name, str) CASE_INT_OP(name, str)
#define CASE_SIMD_OP(name, str)                                              \
  CASE_F32x4_OP(name, str) CASE_I32x4_OP(name, str) CASE_I16x8_OP(name, str) \
      CASE_I8x16_OP(name, str)
#define CASE_SIMDI_OP(name, str) \
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

const char* WasmOpcodes::OpcodeName(WasmOpcode opcode) {
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
    CASE_ALL_OP(StoreMem, "store")
    CASE_INT_OP(StoreMem8, "store8")
    CASE_INT_OP(StoreMem16, "store16")
    CASE_I64_OP(StoreMem32, "store32")
    CASE_S128_OP(StoreMem, "store128")

    // Exception handling opcodes.
    CASE_OP(Try, "try")
    CASE_OP(Catch, "catch")
    CASE_OP(Throw, "throw")
    CASE_OP(Rethrow, "rethrow")
    CASE_OP(BrOnExn, "br_on_exn")

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
    CASE_F64x2_OP(Neg, "neg")
    CASE_F64x2_OP(Sqrt, "sqrt")
    CASE_F32x4_OP(Sqrt, "sqrt")
    CASE_I64x2_OP(Neg, "neg")
    CASE_SIMD_OP(Eq, "eq")
    CASE_F64x2_OP(Eq, "eq")
    CASE_I64x2_OP(Eq, "eq")
    CASE_SIMD_OP(Ne, "ne")
    CASE_F64x2_OP(Ne, "ne")
    CASE_I64x2_OP(Ne, "ne")
    CASE_SIMD_OP(Add, "add")
    CASE_F64x2_OP(Add, "add")
    CASE_I64x2_OP(Add, "add")
    CASE_SIMD_OP(Sub, "sub")
    CASE_F64x2_OP(Sub, "sub")
    CASE_I64x2_OP(Sub, "sub")
    CASE_SIMD_OP(Mul, "mul")
    CASE_F64x2_OP(Mul, "mul")
    CASE_I64x2_OP(Mul, "mul")
    CASE_F64x2_OP(Div, "div")
    CASE_F32x4_OP(Div, "div")
    CASE_F64x2_OP(Splat, "splat")
    CASE_F64x2_OP(Lt, "lt")
    CASE_F64x2_OP(Le, "le")
    CASE_F64x2_OP(Gt, "gt")
    CASE_F64x2_OP(Ge, "ge")
    CASE_F64x2_OP(Abs, "abs")
    CASE_F32x4_OP(Abs, "abs")
    CASE_F32x4_OP(AddHoriz, "add_horizontal")
    CASE_F32x4_OP(RecipApprox, "recip_approx")
    CASE_F32x4_OP(RecipSqrtApprox, "recip_sqrt_approx")
    CASE_F64x2_OP(Min, "min")
    CASE_F32x4_OP(Min, "min")
    CASE_F64x2_OP(Max, "max")
    CASE_F32x4_OP(Max, "max")
    CASE_F32x4_OP(Lt, "lt")
    CASE_F32x4_OP(Le, "le")
    CASE_F32x4_OP(Gt, "gt")
    CASE_F32x4_OP(Ge, "ge")
    CASE_CONVERT_OP(Convert, F64x2, I64x2, "i64", "convert")
    CASE_CONVERT_OP(Convert, F32x4, I32x4, "i32", "convert")
    CASE_CONVERT_OP(Convert, I32x4, F32x4, "f32", "convert")
    CASE_CONVERT_OP(Convert, I32x4, I16x8Low, "i32", "convert")
    CASE_CONVERT_OP(Convert, I32x4, I16x8High, "i32", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I32x4, "i32", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I8x16Low, "i32", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I8x16High, "i32", "convert")
    CASE_CONVERT_OP(Convert, I8x16, I16x8, "i32", "convert")
    CASE_F64x2_OP(ExtractLane, "extract_lane")
    CASE_F64x2_OP(ReplaceLane, "replace_lane")
    CASE_F32x4_OP(ExtractLane, "extract_lane")
    CASE_F32x4_OP(ReplaceLane, "replace_lane")
    CASE_I64x2_OP(ExtractLane, "extract_lane")
    CASE_I64x2_OP(ReplaceLane, "replace_lane")
    CASE_I32x4_OP(ExtractLane, "extract_lane")
    CASE_SIGN_OP(I16x8, ExtractLane, "extract_lane")
    CASE_SIGN_OP(I8x16, ExtractLane, "extract_lane")
    CASE_SIMDI_OP(ReplaceLane, "replace_lane")
    CASE_SIGN_OP(SIMDI, Min, "min")
    CASE_SIGN_OP(I64x2, Min, "min")
    CASE_SIGN_OP(SIMDI, Max, "max")
    CASE_SIGN_OP(I64x2, Max, "max")
    CASE_SIGN_OP(SIMDI, Lt, "lt")
    CASE_SIGN_OP(I64x2, Lt, "lt")
    CASE_SIGN_OP(SIMDI, Le, "le")
    CASE_SIGN_OP(I64x2, Le, "le")
    CASE_SIGN_OP(SIMDI, Gt, "gt")
    CASE_SIGN_OP(I64x2, Gt, "gt")
    CASE_SIGN_OP(SIMDI, Ge, "ge")
    CASE_SIGN_OP(I64x2, Ge, "ge")
    CASE_SIGN_OP(SIMDI, Shr, "shr")
    CASE_SIGN_OP(I64x2, Shr, "shr")
    CASE_SIMDI_OP(Shl, "shl")
    CASE_I64x2_OP(Shl, "shl")
    CASE_I64x2_OP(Splat, "splat")
    CASE_I32x4_OP(AddHoriz, "add_horizontal")
    CASE_I16x8_OP(AddHoriz, "add_horizontal")
    CASE_SIGN_OP(I16x8, AddSaturate, "add_saturate")
    CASE_SIGN_OP(I8x16, AddSaturate, "add_saturate")
    CASE_SIGN_OP(I16x8, SubSaturate, "sub_saturate")
    CASE_SIGN_OP(I8x16, SubSaturate, "sub_saturate")
    CASE_S128_OP(And, "and")
    CASE_S128_OP(Or, "or")
    CASE_S128_OP(Xor, "xor")
    CASE_S128_OP(Not, "not")
    CASE_S128_OP(Select, "select")
    CASE_S8x16_OP(Swizzle, "swizzle")
    CASE_S8x16_OP(Shuffle, "shuffle")
    CASE_S1x2_OP(AnyTrue, "any_true")
    CASE_S1x2_OP(AllTrue, "all_true")
    CASE_S1x4_OP(AnyTrue, "any_true")
    CASE_S1x4_OP(AllTrue, "all_true")
    CASE_S1x8_OP(AnyTrue, "any_true")
    CASE_S1x8_OP(AllTrue, "all_true")
    CASE_S1x16_OP(AnyTrue, "any_true")
    CASE_S1x16_OP(AllTrue, "all_true")
    CASE_F64x2_OP(Qfma, "qfma")
    CASE_F64x2_OP(Qfms, "qfms")
    CASE_F32x4_OP(Qfma, "qfma")
    CASE_F32x4_OP(Qfms, "qfms")

    CASE_S8x16_OP(LoadSplat, "load_splat")
    CASE_S16x8_OP(LoadSplat, "load_splat")
    CASE_S32x4_OP(LoadSplat, "load_splat")
    CASE_S64x2_OP(LoadSplat, "load_splat")
    CASE_I16x8_OP(Load8x8S, "load8x8_s")
    CASE_I16x8_OP(Load8x8U, "load8x8_u")
    CASE_I32x4_OP(Load16x4S, "load16x4_s")
    CASE_I32x4_OP(Load16x4U, "load16x4_u")
    CASE_I64x2_OP(Load32x2S, "load32x2_s")
    CASE_I64x2_OP(Load32x2U, "load32x2_u")

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

    default : return "unknown";
    // clang-format on
  }
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
#undef CASE_S8x16_OP
#undef CASE_S1x2_OP
#undef CASE_S1x4_OP
#undef CASE_S1x8_OP
#undef CASE_S1x16_OP
#undef CASE_INT_OP
#undef CASE_FLOAT_OP
#undef CASE_ALL_OP
#undef CASE_SIMD_OP
#undef CASE_SIMDI_OP
#undef CASE_SIGN_OP
#undef CASE_UNSIGNED_OP
#undef CASE_UNSIGNED_ALL_OP
#undef CASE_ALL_SIGN_OP
#undef CASE_CONVERT_OP
#undef CASE_CONVERT_SAT_OP
#undef CASE_L32_OP
#undef CASE_U32_OP

bool WasmOpcodes::IsPrefixOpcode(WasmOpcode opcode) {
  switch (opcode) {
#define CHECK_PREFIX(name, opcode) case k##name##Prefix:
    FOREACH_PREFIX(CHECK_PREFIX)
#undef CHECK_PREFIX
    return true;
    default:
      return false;
  }
}

bool WasmOpcodes::IsControlOpcode(WasmOpcode opcode) {
  switch (opcode) {
#define CHECK_OPCODE(name, opcode, _) case kExpr##name:
    FOREACH_CONTROL_OPCODE(CHECK_OPCODE)
#undef CHECK_OPCODE
    return true;
    default:
      return false;
  }
}

bool WasmOpcodes::IsUnconditionalJump(WasmOpcode opcode) {
  switch (opcode) {
    case kExprUnreachable:
    case kExprBr:
    case kExprBrTable:
    case kExprReturn:
      return true;
    default:
      return false;
  }
}

bool WasmOpcodes::IsSignExtensionOpcode(WasmOpcode opcode) {
  switch (opcode) {
    case kExprI32SExtendI8:
    case kExprI32SExtendI16:
    case kExprI64SExtendI8:
    case kExprI64SExtendI16:
    case kExprI64SExtendI32:
      return true;
    default:
      return false;
  }
}

bool WasmOpcodes::IsAnyRefOpcode(WasmOpcode opcode) {
  switch (opcode) {
    case kExprRefNull:
    case kExprRefIsNull:
    case kExprRefFunc:
      return true;
    default:
      return false;
  }
}

bool WasmOpcodes::IsThrowingOpcode(WasmOpcode opcode) {
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

std::ostream& operator<<(std::ostream& os, const FunctionSig& sig) {
  if (sig.return_count() == 0) os << "v";
  for (auto ret : sig.returns()) {
    os << ValueTypes::ShortNameOf(ret);
  }
  os << "_";
  if (sig.parameter_count() == 0) os << "v";
  for (auto param : sig.parameters()) {
    os << ValueTypes::ShortNameOf(param);
  }
  return os;
}

bool IsJSCompatibleSignature(const FunctionSig* sig,
                             const WasmFeatures& enabled_features) {
  if (!enabled_features.has_mv() && sig->return_count() > 1) {
    return false;
  }
  for (auto type : sig->all()) {
    if (!enabled_features.has_bigint() && type == kWasmI64) {
      return false;
    }

    if (type == kWasmS128) return false;
  }
  return true;
}

namespace {

#define DECLARE_SIG_ENUM(name, ...) kSigEnum_##name,
enum WasmOpcodeSig : byte {
  kSigEnum_None,
  FOREACH_SIGNATURE(DECLARE_SIG_ENUM)
};
#undef DECLARE_SIG_ENUM
#define DECLARE_SIG(name, ...)                                                \
  constexpr ValueType kTypes_##name[] = {__VA_ARGS__};                        \
  constexpr int kReturnsCount_##name = kTypes_##name[0] == kWasmStmt ? 0 : 1; \
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
        kSigEnum_None;
#undef CASE
}

constexpr WasmOpcodeSig GetAtomicOpcodeSigIndex(byte opcode) {
#define CASE(name, opc, sig) opcode == (opc & 0xFF) ? kSigEnum_##sig:
    return FOREACH_ATOMIC_OPCODE(CASE) FOREACH_ATOMIC_0_OPERAND_OPCODE(CASE)
        kSigEnum_None;
#undef CASE
}

constexpr WasmOpcodeSig GetNumericOpcodeSigIndex(byte opcode) {
#define CASE(name, opc, sig) opcode == (opc & 0xFF) ? kSigEnum_##sig:
    return FOREACH_NUMERIC_OPCODE(CASE) kSigEnum_None;
#undef CASE
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

}  // namespace

FunctionSig* WasmOpcodes::Signature(WasmOpcode opcode) {
  switch (opcode >> 8) {
    case 0:
      return const_cast<FunctionSig*>(kCachedSigs[kShortSigTable[opcode]]);
    case kSimdPrefix:
      return const_cast<FunctionSig*>(
          kCachedSigs[kSimdExprSigTable[opcode & 0xFF]]);
    case kAtomicPrefix:
      return const_cast<FunctionSig*>(
          kCachedSigs[kAtomicExprSigTable[opcode & 0xFF]]);
    case kNumericPrefix:
      return const_cast<FunctionSig*>(
          kCachedSigs[kNumericExprSigTable[opcode & 0xFF]]);
    default:
      UNREACHABLE();  // invalid prefix.
      return nullptr;
  }
}

FunctionSig* WasmOpcodes::AsmjsSignature(WasmOpcode opcode) {
  DCHECK_GT(kSimpleAsmjsExprSigTable.size(), opcode);
  return const_cast<FunctionSig*>(
      kCachedSigs[kSimpleAsmjsExprSigTable[opcode]]);
}

// Define constexpr arrays.
constexpr uint8_t LoadType::kLoadSizeLog2[];
constexpr ValueType LoadType::kValueType[];
constexpr MachineType LoadType::kMemType[];
constexpr uint8_t StoreType::kStoreSizeLog2[];
constexpr ValueType StoreType::kValueType[];
constexpr MachineRepresentation StoreType::kMemRep[];

MessageTemplate WasmOpcodes::TrapReasonToMessageId(TrapReason reason) {
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

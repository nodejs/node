// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-opcodes.h"
#include "src/messages.h"
#include "src/runtime/runtime.h"
#include "src/signature.h"

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
#define CASE_F32x4_OP(name, str) CASE_OP(F32x4##name, "f32x4." str)
#define CASE_I32x4_OP(name, str) CASE_OP(I32x4##name, "i32x4." str)
#define CASE_I16x8_OP(name, str) CASE_OP(I16x8##name, "i16x8." str)
#define CASE_I8x16_OP(name, str) CASE_OP(I8x16##name, "i8x16." str)
#define CASE_S128_OP(name, str) CASE_OP(S128##name, "s128." str)
#define CASE_S32x4_OP(name, str) CASE_OP(S32x4##name, "s32x4." str)
#define CASE_S16x8_OP(name, str) CASE_OP(S16x8##name, "s16x8." str)
#define CASE_S8x16_OP(name, str) CASE_OP(S8x16##name, "s8x16." str)
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
#define CASE_ALL_SIGN_OP(name, str) \
  CASE_FLOAT_OP(name, str) CASE_SIGN_OP(INT, name, str)
#define CASE_CONVERT_OP(name, RES, SRC, src_suffix, str) \
  CASE_##RES##_OP(U##name##SRC, str "_u/" src_suffix)    \
      CASE_##RES##_OP(S##name##SRC, str "_s/" src_suffix)
#define CASE_L32_OP(name, str)          \
  CASE_SIGN_OP(I32, name##8, str "8")   \
  CASE_SIGN_OP(I32, name##16, str "16") \
  CASE_I32_OP(name, str "32")

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
    CASE_I32_OP(ConvertI64, "wrap/i64")
    CASE_CONVERT_OP(Convert, INT, F32, "f32", "trunc")
    CASE_CONVERT_OP(Convert, INT, F64, "f64", "trunc")
    CASE_CONVERT_OP(Convert, I64, I32, "i32", "extend")
    CASE_CONVERT_OP(Convert, F32, I32, "i32", "convert")
    CASE_CONVERT_OP(Convert, F32, I64, "i64", "convert")
    CASE_F32_OP(ConvertF64, "demote/f64")
    CASE_CONVERT_OP(Convert, F64, I32, "i32", "convert")
    CASE_CONVERT_OP(Convert, F64, I64, "i64", "convert")
    CASE_F64_OP(ConvertF32, "promote/f32")
    CASE_I32_OP(ReinterpretF32, "reinterpret/f32")
    CASE_I64_OP(ReinterpretF64, "reinterpret/f64")
    CASE_F32_OP(ReinterpretI32, "reinterpret/i32")
    CASE_F64_OP(ReinterpretI64, "reinterpret/i64")
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
    CASE_OP(Drop, "drop")
    CASE_OP(Select, "select")
    CASE_OP(GetLocal, "get_local")
    CASE_OP(SetLocal, "set_local")
    CASE_OP(TeeLocal, "tee_local")
    CASE_OP(GetGlobal, "get_global")
    CASE_OP(SetGlobal, "set_global")
    CASE_ALL_OP(Const, "const")
    CASE_OP(MemorySize, "current_memory")
    CASE_OP(GrowMemory, "grow_memory")
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

    // Non-standard opcodes.
    CASE_OP(Try, "try")
    CASE_OP(Throw, "throw")
    CASE_OP(Catch, "catch")

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
    CASE_I32_OP(AsmjsSConvertF32, "asmjs_convert_s/f32")
    CASE_I32_OP(AsmjsUConvertF32, "asmjs_convert_u/f32")
    CASE_I32_OP(AsmjsSConvertF64, "asmjs_convert_s/f64")
    CASE_I32_OP(AsmjsUConvertF64, "asmjs_convert_u/f64")

    // SIMD opcodes.
    CASE_SIMD_OP(Splat, "splat")
    CASE_SIMD_OP(Neg, "neg")
    CASE_SIMD_OP(Eq, "eq")
    CASE_SIMD_OP(Ne, "ne")
    CASE_SIMD_OP(Add, "add")
    CASE_SIMD_OP(Sub, "sub")
    CASE_SIMD_OP(Mul, "mul")
    CASE_F32x4_OP(Abs, "abs")
    CASE_F32x4_OP(AddHoriz, "add_horizontal")
    CASE_F32x4_OP(RecipApprox, "recip_approx")
    CASE_F32x4_OP(RecipSqrtApprox, "recip_sqrt_approx")
    CASE_F32x4_OP(Min, "min")
    CASE_F32x4_OP(Max, "max")
    CASE_F32x4_OP(Lt, "lt")
    CASE_F32x4_OP(Le, "le")
    CASE_F32x4_OP(Gt, "gt")
    CASE_F32x4_OP(Ge, "ge")
    CASE_CONVERT_OP(Convert, F32x4, I32x4, "i32", "convert")
    CASE_CONVERT_OP(Convert, I32x4, F32x4, "f32", "convert")
    CASE_CONVERT_OP(Convert, I32x4, I16x8Low, "i32", "convert")
    CASE_CONVERT_OP(Convert, I32x4, I16x8High, "i32", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I32x4, "i32", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I8x16Low, "i32", "convert")
    CASE_CONVERT_OP(Convert, I16x8, I8x16High, "i32", "convert")
    CASE_CONVERT_OP(Convert, I8x16, I16x8, "i32", "convert")
    CASE_F32x4_OP(ExtractLane, "extract_lane")
    CASE_F32x4_OP(ReplaceLane, "replace_lane")
    CASE_SIMDI_OP(ExtractLane, "extract_lane")
    CASE_SIMDI_OP(ReplaceLane, "replace_lane")
    CASE_SIGN_OP(SIMDI, Min, "min")
    CASE_SIGN_OP(SIMDI, Max, "max")
    CASE_SIGN_OP(SIMDI, Lt, "lt")
    CASE_SIGN_OP(SIMDI, Le, "le")
    CASE_SIGN_OP(SIMDI, Gt, "gt")
    CASE_SIGN_OP(SIMDI, Ge, "ge")
    CASE_SIGN_OP(SIMDI, Shr, "shr")
    CASE_SIMDI_OP(Shl, "shl")
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
    CASE_S32x4_OP(Shuffle, "shuffle")
    CASE_S32x4_OP(Select, "select")
    CASE_S16x8_OP(Shuffle, "shuffle")
    CASE_S16x8_OP(Select, "select")
    CASE_S8x16_OP(Shuffle, "shuffle")
    CASE_S8x16_OP(Select, "select")
    CASE_S1x4_OP(And, "and")
    CASE_S1x4_OP(Or, "or")
    CASE_S1x4_OP(Xor, "xor")
    CASE_S1x4_OP(Not, "not")
    CASE_S1x4_OP(AnyTrue, "any_true")
    CASE_S1x4_OP(AllTrue, "all_true")
    CASE_S1x8_OP(And, "and")
    CASE_S1x8_OP(Or, "or")
    CASE_S1x8_OP(Xor, "xor")
    CASE_S1x8_OP(Not, "not")
    CASE_S1x8_OP(AnyTrue, "any_true")
    CASE_S1x8_OP(AllTrue, "all_true")
    CASE_S1x16_OP(And, "and")
    CASE_S1x16_OP(Or, "or")
    CASE_S1x16_OP(Xor, "xor")
    CASE_S1x16_OP(Not, "not")
    CASE_S1x16_OP(AnyTrue, "any_true")
    CASE_S1x16_OP(AllTrue, "all_true")

    // Atomic operations.
    CASE_L32_OP(AtomicAdd, "atomic_add")
    CASE_L32_OP(AtomicAnd, "atomic_and")
    CASE_L32_OP(AtomicCompareExchange, "atomic_cmpxchng")
    CASE_L32_OP(AtomicExchange, "atomic_xchng")
    CASE_L32_OP(AtomicOr, "atomic_or")
    CASE_L32_OP(AtomicSub, "atomic_sub")
    CASE_L32_OP(AtomicXor, "atomic_xor")

    default : return "unknown";
    // clang-format on
  }
}

bool WasmOpcodes::IsPrefixOpcode(WasmOpcode opcode) {
  switch (opcode) {
#define CHECK_PREFIX(name, opcode) \
  case k##name##Prefix:            \
    return true;
    FOREACH_PREFIX(CHECK_PREFIX)
#undef CHECK_PREFIX
    default:
      return false;
  }
}

bool WasmOpcodes::IsControlOpcode(WasmOpcode opcode) {
  switch (opcode) {
#define CHECK_OPCODE(name, opcode, _) \
  case kExpr##name:                   \
    return true;
    FOREACH_CONTROL_OPCODE(CHECK_OPCODE)
#undef CHECK_OPCODE
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

std::ostream& operator<<(std::ostream& os, const FunctionSig& sig) {
  if (sig.return_count() == 0) os << "v";
  for (auto ret : sig.returns()) {
    os << WasmOpcodes::ShortNameOf(ret);
  }
  os << "_";
  if (sig.parameter_count() == 0) os << "v";
  for (auto param : sig.parameters()) {
    os << WasmOpcodes::ShortNameOf(param);
  }
  return os;
}

bool IsJSCompatibleSignature(const FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmI64 || type == wasm::kWasmS128) return false;
  }
  return true;
}

#define DECLARE_SIG_ENUM(name, ...) kSigEnum_##name,

enum WasmOpcodeSig { FOREACH_SIGNATURE(DECLARE_SIG_ENUM) };

// TODO(titzer): not static-initializer safe. Wrap in LazyInstance.
#define DECLARE_SIG(name, ...)                      \
  static ValueType kTypes_##name[] = {__VA_ARGS__}; \
  static const FunctionSig kSig_##name(             \
      1, static_cast<int>(arraysize(kTypes_##name)) - 1, kTypes_##name);

FOREACH_SIGNATURE(DECLARE_SIG)

#define DECLARE_SIG_ENTRY(name, ...) &kSig_##name,

static const FunctionSig* kSimpleExprSigs[] = {
    nullptr, FOREACH_SIGNATURE(DECLARE_SIG_ENTRY)};

#define DECLARE_SIMD_SIG_ENTRY(name, ...) &kSig_##name,

static const FunctionSig* kSimdExprSigs[] = {
    nullptr, FOREACH_SIMD_SIGNATURE(DECLARE_SIMD_SIG_ENTRY)};

static byte kSimpleExprSigTable[256];
static byte kSimpleAsmjsExprSigTable[256];
static byte kSimdExprSigTable[256];
static byte kAtomicExprSigTable[256];

// Initialize the signature table.
static void InitSigTables() {
#define SET_SIG_TABLE(name, opcode, sig) \
  kSimpleExprSigTable[opcode] = static_cast<int>(kSigEnum_##sig) + 1;
  FOREACH_SIMPLE_OPCODE(SET_SIG_TABLE);
#undef SET_SIG_TABLE
#define SET_ASMJS_SIG_TABLE(name, opcode, sig) \
  kSimpleAsmjsExprSigTable[opcode] = static_cast<int>(kSigEnum_##sig) + 1;
  FOREACH_ASMJS_COMPAT_OPCODE(SET_ASMJS_SIG_TABLE);
#undef SET_ASMJS_SIG_TABLE
  byte simd_index;
#define SET_SIG_TABLE(name, opcode, sig) \
  simd_index = opcode & 0xff;            \
  kSimdExprSigTable[simd_index] = static_cast<int>(kSigEnum_##sig) + 1;
  FOREACH_SIMD_0_OPERAND_OPCODE(SET_SIG_TABLE)
#undef SET_SIG_TABLE
  byte atomic_index;
#define SET_ATOMIC_SIG_TABLE(name, opcode, sig) \
  atomic_index = opcode & 0xff;                 \
  kAtomicExprSigTable[atomic_index] = static_cast<int>(kSigEnum_##sig) + 1;
  FOREACH_ATOMIC_OPCODE(SET_ATOMIC_SIG_TABLE)
#undef SET_ATOMIC_SIG_TABLE
}

class SigTable {
 public:
  SigTable() {
    // TODO(ahaas): Move {InitSigTable} into the class.
    InitSigTables();
  }
  FunctionSig* Signature(WasmOpcode opcode) const {
    return const_cast<FunctionSig*>(
        kSimpleExprSigs[kSimpleExprSigTable[static_cast<byte>(opcode)]]);
  }
  FunctionSig* AsmjsSignature(WasmOpcode opcode) const {
    return const_cast<FunctionSig*>(
        kSimpleExprSigs[kSimpleAsmjsExprSigTable[static_cast<byte>(opcode)]]);
  }
  FunctionSig* SimdSignature(WasmOpcode opcode) const {
    return const_cast<FunctionSig*>(
        kSimdExprSigs[kSimdExprSigTable[static_cast<byte>(opcode & 0xff)]]);
  }
  FunctionSig* AtomicSignature(WasmOpcode opcode) const {
    return const_cast<FunctionSig*>(
        kSimpleExprSigs[kAtomicExprSigTable[static_cast<byte>(opcode & 0xff)]]);
  }
};

static base::LazyInstance<SigTable>::type sig_table = LAZY_INSTANCE_INITIALIZER;

FunctionSig* WasmOpcodes::Signature(WasmOpcode opcode) {
  if (opcode >> 8 == kSimdPrefix) {
    return sig_table.Get().SimdSignature(opcode);
  } else {
    return sig_table.Get().Signature(opcode);
  }
}

FunctionSig* WasmOpcodes::AsmjsSignature(WasmOpcode opcode) {
  return sig_table.Get().AsmjsSignature(opcode);
}

FunctionSig* WasmOpcodes::AtomicSignature(WasmOpcode opcode) {
  return sig_table.Get().AtomicSignature(opcode);
}

// TODO(titzer): pull WASM_64 up to a common header.
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
#define WASM_64 1
#else
#define WASM_64 0
#endif

int WasmOpcodes::TrapReasonToMessageId(TrapReason reason) {
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
  return MessageTemplate::TemplateString(TrapReasonToMessageId(reason));
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8

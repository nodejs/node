// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-opcodes.h"
#include "src/signature.h"

namespace v8 {
namespace internal {
namespace wasm {

typedef Signature<LocalType> FunctionSig;

const char* WasmOpcodes::OpcodeName(WasmOpcode opcode) {
  switch (opcode) {
#define DECLARE_NAME_CASE(name, opcode, sig) \
  case kExpr##name:                          \
    return "Expr" #name;
    FOREACH_OPCODE(DECLARE_NAME_CASE)
#undef DECLARE_NAME_CASE
    default:
      break;
  }
  return "Unknown";
}


std::ostream& operator<<(std::ostream& os, const FunctionSig& sig) {
  if (sig.return_count() == 0) os << "v";
  for (size_t i = 0; i < sig.return_count(); i++) {
    os << WasmOpcodes::ShortNameOf(sig.GetReturn(i));
  }
  os << "_";
  if (sig.parameter_count() == 0) os << "v";
  for (size_t i = 0; i < sig.parameter_count(); i++) {
    os << WasmOpcodes::ShortNameOf(sig.GetParam(i));
  }
  return os;
}


#define DECLARE_SIG_ENUM(name, ...) kSigEnum_##name,


enum WasmOpcodeSig { FOREACH_SIGNATURE(DECLARE_SIG_ENUM) };


// TODO(titzer): not static-initializer safe. Wrap in LazyInstance.
#define DECLARE_SIG(name, ...)                      \
  static LocalType kTypes_##name[] = {__VA_ARGS__}; \
  static const FunctionSig kSig_##name(             \
      1, static_cast<int>(arraysize(kTypes_##name)) - 1, kTypes_##name);

FOREACH_SIGNATURE(DECLARE_SIG)

#define DECLARE_SIG_ENTRY(name, ...) &kSig_##name,

static const FunctionSig* kSimpleExprSigs[] = {
    nullptr, FOREACH_SIGNATURE(DECLARE_SIG_ENTRY)};

static byte kSimpleExprSigTable[256];


// Initialize the signature table.
static void InitSigTable() {
#define SET_SIG_TABLE(name, opcode, sig) \
  kSimpleExprSigTable[opcode] = static_cast<int>(kSigEnum_##sig) + 1;
  FOREACH_SIMPLE_OPCODE(SET_SIG_TABLE);
#undef SET_SIG_TABLE
}


FunctionSig* WasmOpcodes::Signature(WasmOpcode opcode) {
  // TODO(titzer): use LazyInstance to make this thread safe.
  if (kSimpleExprSigTable[kExprI32Add] == 0) InitSigTable();
  return const_cast<FunctionSig*>(
      kSimpleExprSigs[kSimpleExprSigTable[static_cast<byte>(opcode)]]);
}


// TODO(titzer): pull WASM_64 up to a common header.
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
#define WASM_64 1
#else
#define WASM_64 0
#endif


bool WasmOpcodes::IsSupported(WasmOpcode opcode) {
#if !WASM_64
  switch (opcode) {
    // Opcodes not supported on 32-bit platforms.
    case kExprI64Add:
    case kExprI64Sub:
    case kExprI64Mul:
    case kExprI64DivS:
    case kExprI64DivU:
    case kExprI64RemS:
    case kExprI64RemU:
    case kExprI64And:
    case kExprI64Ior:
    case kExprI64Xor:
    case kExprI64Shl:
    case kExprI64ShrU:
    case kExprI64ShrS:
    case kExprI64Eq:
    case kExprI64Ne:
    case kExprI64LtS:
    case kExprI64LeS:
    case kExprI64LtU:
    case kExprI64LeU:
    case kExprI64GtS:
    case kExprI64GeS:
    case kExprI64GtU:
    case kExprI64GeU:

    case kExprI32ConvertI64:
    case kExprI64SConvertI32:
    case kExprI64UConvertI32:

    case kExprF64ReinterpretI64:
    case kExprI64ReinterpretF64:

    case kExprI64Clz:
    case kExprI64Ctz:
    case kExprI64Popcnt:

    case kExprF32SConvertI64:
    case kExprF32UConvertI64:
    case kExprF64SConvertI64:
    case kExprF64UConvertI64:
    case kExprI64SConvertF32:
    case kExprI64SConvertF64:
    case kExprI64UConvertF32:
    case kExprI64UConvertF64:

      return false;
    default:
      return true;
  }
#else
  return true;
#endif
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8

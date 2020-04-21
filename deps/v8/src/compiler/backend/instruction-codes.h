// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_INSTRUCTION_CODES_H_
#define V8_COMPILER_BACKEND_INSTRUCTION_CODES_H_

#include <iosfwd>

#if V8_TARGET_ARCH_ARM
#include "src/compiler/backend/arm/instruction-codes-arm.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/compiler/backend/arm64/instruction-codes-arm64.h"
#elif V8_TARGET_ARCH_IA32
#include "src/compiler/backend/ia32/instruction-codes-ia32.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/compiler/backend/mips/instruction-codes-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/compiler/backend/mips64/instruction-codes-mips64.h"
#elif V8_TARGET_ARCH_X64
#include "src/compiler/backend/x64/instruction-codes-x64.h"
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#include "src/compiler/backend/ppc/instruction-codes-ppc.h"
#elif V8_TARGET_ARCH_S390
#include "src/compiler/backend/s390/instruction-codes-s390.h"
#else
#define TARGET_ARCH_OPCODE_LIST(V)
#define TARGET_ADDRESSING_MODE_LIST(V)
#endif
#include "src/base/bit-field.h"
#include "src/compiler/write-barrier-kind.h"

namespace v8 {
namespace internal {
namespace compiler {

// Modes for ArchStoreWithWriteBarrier below.
enum class RecordWriteMode {
  kValueIsMap,
  kValueIsPointer,
  kValueIsEphemeronKey,
  kValueIsAny,
};

inline RecordWriteMode WriteBarrierKindToRecordWriteMode(
    WriteBarrierKind write_barrier_kind) {
  switch (write_barrier_kind) {
    case kMapWriteBarrier:
      return RecordWriteMode::kValueIsMap;
    case kPointerWriteBarrier:
      return RecordWriteMode::kValueIsPointer;
    case kEphemeronKeyWriteBarrier:
      return RecordWriteMode::kValueIsEphemeronKey;
    case kFullWriteBarrier:
      return RecordWriteMode::kValueIsAny;
    case kNoWriteBarrier:
    // Should not be passed as argument.
    default:
      break;
  }
  UNREACHABLE();
}

// Target-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define COMMON_ARCH_OPCODE_LIST(V)                                     \
  /* Tail call opcodes are grouped together to make IsTailCall fast */ \
  V(ArchTailCallCodeObjectFromJSFunction)                              \
  V(ArchTailCallCodeObject)                                            \
  V(ArchTailCallAddress)                                               \
  V(ArchTailCallWasm)                                                  \
  /* Update IsTailCall if further TailCall opcodes are added */        \
                                                                       \
  V(ArchCallCodeObject)                                                \
  V(ArchCallJSFunction)                                                \
  V(ArchPrepareCallCFunction)                                          \
  V(ArchSaveCallerRegisters)                                           \
  V(ArchRestoreCallerRegisters)                                        \
  V(ArchCallCFunction)                                                 \
  V(ArchPrepareTailCall)                                               \
  V(ArchCallWasmFunction)                                              \
  V(ArchCallBuiltinPointer)                                            \
  V(ArchJmp)                                                           \
  V(ArchBinarySearchSwitch)                                            \
  V(ArchTableSwitch)                                                   \
  V(ArchNop)                                                           \
  V(ArchAbortCSAAssert)                                                \
  V(ArchDebugBreak)                                                    \
  V(ArchComment)                                                       \
  V(ArchThrowTerminator)                                               \
  V(ArchDeoptimize)                                                    \
  V(ArchRet)                                                           \
  V(ArchFramePointer)                                                  \
  V(ArchParentFramePointer)                                            \
  V(ArchTruncateDoubleToI)                                             \
  V(ArchStoreWithWriteBarrier)                                         \
  V(ArchStackSlot)                                                     \
  V(ArchWordPoisonOnSpeculation)                                       \
  V(ArchStackPointerGreaterThan)                                       \
  V(ArchStackCheckOffset)                                              \
  V(Word32AtomicLoadInt8)                                              \
  V(Word32AtomicLoadUint8)                                             \
  V(Word32AtomicLoadInt16)                                             \
  V(Word32AtomicLoadUint16)                                            \
  V(Word32AtomicLoadWord32)                                            \
  V(Word32AtomicStoreWord8)                                            \
  V(Word32AtomicStoreWord16)                                           \
  V(Word32AtomicStoreWord32)                                           \
  V(Word32AtomicExchangeInt8)                                          \
  V(Word32AtomicExchangeUint8)                                         \
  V(Word32AtomicExchangeInt16)                                         \
  V(Word32AtomicExchangeUint16)                                        \
  V(Word32AtomicExchangeWord32)                                        \
  V(Word32AtomicCompareExchangeInt8)                                   \
  V(Word32AtomicCompareExchangeUint8)                                  \
  V(Word32AtomicCompareExchangeInt16)                                  \
  V(Word32AtomicCompareExchangeUint16)                                 \
  V(Word32AtomicCompareExchangeWord32)                                 \
  V(Word32AtomicAddInt8)                                               \
  V(Word32AtomicAddUint8)                                              \
  V(Word32AtomicAddInt16)                                              \
  V(Word32AtomicAddUint16)                                             \
  V(Word32AtomicAddWord32)                                             \
  V(Word32AtomicSubInt8)                                               \
  V(Word32AtomicSubUint8)                                              \
  V(Word32AtomicSubInt16)                                              \
  V(Word32AtomicSubUint16)                                             \
  V(Word32AtomicSubWord32)                                             \
  V(Word32AtomicAndInt8)                                               \
  V(Word32AtomicAndUint8)                                              \
  V(Word32AtomicAndInt16)                                              \
  V(Word32AtomicAndUint16)                                             \
  V(Word32AtomicAndWord32)                                             \
  V(Word32AtomicOrInt8)                                                \
  V(Word32AtomicOrUint8)                                               \
  V(Word32AtomicOrInt16)                                               \
  V(Word32AtomicOrUint16)                                              \
  V(Word32AtomicOrWord32)                                              \
  V(Word32AtomicXorInt8)                                               \
  V(Word32AtomicXorUint8)                                              \
  V(Word32AtomicXorInt16)                                              \
  V(Word32AtomicXorUint16)                                             \
  V(Word32AtomicXorWord32)                                             \
  V(Ieee754Float64Acos)                                                \
  V(Ieee754Float64Acosh)                                               \
  V(Ieee754Float64Asin)                                                \
  V(Ieee754Float64Asinh)                                               \
  V(Ieee754Float64Atan)                                                \
  V(Ieee754Float64Atanh)                                               \
  V(Ieee754Float64Atan2)                                               \
  V(Ieee754Float64Cbrt)                                                \
  V(Ieee754Float64Cos)                                                 \
  V(Ieee754Float64Cosh)                                                \
  V(Ieee754Float64Exp)                                                 \
  V(Ieee754Float64Expm1)                                               \
  V(Ieee754Float64Log)                                                 \
  V(Ieee754Float64Log1p)                                               \
  V(Ieee754Float64Log10)                                               \
  V(Ieee754Float64Log2)                                                \
  V(Ieee754Float64Pow)                                                 \
  V(Ieee754Float64Sin)                                                 \
  V(Ieee754Float64Sinh)                                                \
  V(Ieee754Float64Tan)                                                 \
  V(Ieee754Float64Tanh)

#define ARCH_OPCODE_LIST(V)  \
  COMMON_ARCH_OPCODE_LIST(V) \
  TARGET_ARCH_OPCODE_LIST(V)

enum ArchOpcode {
#define DECLARE_ARCH_OPCODE(Name) k##Name,
  ARCH_OPCODE_LIST(DECLARE_ARCH_OPCODE)
#undef DECLARE_ARCH_OPCODE
#define COUNT_ARCH_OPCODE(Name) +1
      kLastArchOpcode = -1 ARCH_OPCODE_LIST(COUNT_ARCH_OPCODE)
#undef COUNT_ARCH_OPCODE
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const ArchOpcode& ao);

// Addressing modes represent the "shape" of inputs to an instruction.
// Many instructions support multiple addressing modes. Addressing modes
// are encoded into the InstructionCode of the instruction and tell the
// code generator after register allocation which assembler method to call.
#define ADDRESSING_MODE_LIST(V) \
  V(None)                       \
  TARGET_ADDRESSING_MODE_LIST(V)

enum AddressingMode {
#define DECLARE_ADDRESSING_MODE(Name) kMode_##Name,
  ADDRESSING_MODE_LIST(DECLARE_ADDRESSING_MODE)
#undef DECLARE_ADDRESSING_MODE
#define COUNT_ADDRESSING_MODE(Name) +1
      kLastAddressingMode = -1 ADDRESSING_MODE_LIST(COUNT_ADDRESSING_MODE)
#undef COUNT_ADDRESSING_MODE
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const AddressingMode& am);

// The mode of the flags continuation (see below).
enum FlagsMode {
  kFlags_none = 0,
  kFlags_branch = 1,
  kFlags_branch_and_poison = 2,
  kFlags_deoptimize = 3,
  kFlags_deoptimize_and_poison = 4,
  kFlags_set = 5,
  kFlags_trap = 6
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const FlagsMode& fm);

// The condition of flags continuation (see below).
enum FlagsCondition {
  kEqual,
  kNotEqual,
  kSignedLessThan,
  kSignedGreaterThanOrEqual,
  kSignedLessThanOrEqual,
  kSignedGreaterThan,
  kUnsignedLessThan,
  kUnsignedGreaterThanOrEqual,
  kUnsignedLessThanOrEqual,
  kUnsignedGreaterThan,
  kFloatLessThanOrUnordered,
  kFloatGreaterThanOrEqual,
  kFloatLessThanOrEqual,
  kFloatGreaterThanOrUnordered,
  kFloatLessThan,
  kFloatGreaterThanOrEqualOrUnordered,
  kFloatLessThanOrEqualOrUnordered,
  kFloatGreaterThan,
  kUnorderedEqual,
  kUnorderedNotEqual,
  kOverflow,
  kNotOverflow,
  kPositiveOrZero,
  kNegative
};

static constexpr FlagsCondition kStackPointerGreaterThanCondition =
    kUnsignedGreaterThan;

inline FlagsCondition NegateFlagsCondition(FlagsCondition condition) {
  return static_cast<FlagsCondition>(condition ^ 1);
}

FlagsCondition CommuteFlagsCondition(FlagsCondition condition);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           const FlagsCondition& fc);

enum MemoryAccessMode {
  kMemoryAccessDirect = 0,
  kMemoryAccessProtected = 1,
  kMemoryAccessPoisoned = 2
};

// The InstructionCode is an opaque, target-specific integer that encodes
// what code to emit for an instruction in the code generator. It is not
// interesting to the register allocator, as the inputs and flags on the
// instructions specify everything of interest.
using InstructionCode = uint32_t;

// Helpers for encoding / decoding InstructionCode into the fields needed
// for code generation. We encode the instruction, addressing mode, and flags
// continuation into a single InstructionCode which is stored as part of
// the instruction.
using ArchOpcodeField = base::BitField<ArchOpcode, 0, 9>;
using AddressingModeField = base::BitField<AddressingMode, 9, 5>;
using FlagsModeField = base::BitField<FlagsMode, 14, 3>;
using FlagsConditionField = base::BitField<FlagsCondition, 17, 5>;
using MiscField = base::BitField<int, 22, 10>;

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_CODES_H_

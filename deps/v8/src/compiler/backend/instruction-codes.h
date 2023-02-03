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
#elif V8_TARGET_ARCH_MIPS64
#include "src/compiler/backend/mips64/instruction-codes-mips64.h"
#elif V8_TARGET_ARCH_LOONG64
#include "src/compiler/backend/loong64/instruction-codes-loong64.h"
#elif V8_TARGET_ARCH_X64
#include "src/compiler/backend/x64/instruction-codes-x64.h"
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#include "src/compiler/backend/ppc/instruction-codes-ppc.h"
#elif V8_TARGET_ARCH_S390
#include "src/compiler/backend/s390/instruction-codes-s390.h"
#elif V8_TARGET_ARCH_RISCV32 || V8_TARGET_ARCH_RISCV64
#include "src/compiler/backend/riscv/instruction-codes-riscv.h"
#else
#define TARGET_ARCH_OPCODE_LIST(V)
#define TARGET_ADDRESSING_MODE_LIST(V)
#endif
#include "src/base/bit-field.h"
#include "src/codegen/atomic-memory-order.h"
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

#define COMMON_ARCH_OPCODE_WITH_MEMORY_ACCESS_MODE_LIST(V) \
  V(AtomicExchangeInt8)                                    \
  V(AtomicExchangeUint8)                                   \
  V(AtomicExchangeInt16)                                   \
  V(AtomicExchangeUint16)                                  \
  V(AtomicExchangeWord32)                                  \
  V(AtomicCompareExchangeInt8)                             \
  V(AtomicCompareExchangeUint8)                            \
  V(AtomicCompareExchangeInt16)                            \
  V(AtomicCompareExchangeUint16)                           \
  V(AtomicCompareExchangeWord32)                           \
  V(AtomicAddInt8)                                         \
  V(AtomicAddUint8)                                        \
  V(AtomicAddInt16)                                        \
  V(AtomicAddUint16)                                       \
  V(AtomicAddWord32)                                       \
  V(AtomicSubInt8)                                         \
  V(AtomicSubUint8)                                        \
  V(AtomicSubInt16)                                        \
  V(AtomicSubUint16)                                       \
  V(AtomicSubWord32)                                       \
  V(AtomicAndInt8)                                         \
  V(AtomicAndUint8)                                        \
  V(AtomicAndInt16)                                        \
  V(AtomicAndUint16)                                       \
  V(AtomicAndWord32)                                       \
  V(AtomicOrInt8)                                          \
  V(AtomicOrUint8)                                         \
  V(AtomicOrInt16)                                         \
  V(AtomicOrUint16)                                        \
  V(AtomicOrWord32)                                        \
  V(AtomicXorInt8)                                         \
  V(AtomicXorUint8)                                        \
  V(AtomicXorInt16)                                        \
  V(AtomicXorUint16)                                       \
  V(AtomicXorWord32)                                       \
  V(ArchStoreWithWriteBarrier)                             \
  V(ArchAtomicStoreWithWriteBarrier)                       \
  V(AtomicLoadInt8)                                        \
  V(AtomicLoadUint8)                                       \
  V(AtomicLoadInt16)                                       \
  V(AtomicLoadUint16)                                      \
  V(AtomicLoadWord32)                                      \
  V(AtomicStoreWord8)                                      \
  V(AtomicStoreWord16)                                     \
  V(AtomicStoreWord32)

// Target-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define COMMON_ARCH_OPCODE_LIST(V)                                         \
  /* Tail call opcodes are grouped together to make IsTailCall fast */     \
  /* and Arch call opcodes are grouped together to make */                 \
  /* IsCallWithDescriptorFlags fast */                                     \
  V(ArchTailCallCodeObject)                                                \
  V(ArchTailCallAddress)                                                   \
  IF_WASM(V, ArchTailCallWasm)                                             \
  /* Update IsTailCall if further TailCall opcodes are added */            \
                                                                           \
  V(ArchCallCodeObject)                                                    \
  V(ArchCallJSFunction)                                                    \
  IF_WASM(V, ArchCallWasmFunction)                                         \
  V(ArchCallBuiltinPointer)                                                \
  /* Update IsCallWithDescriptorFlags if further Call opcodes are added */ \
                                                                           \
  V(ArchPrepareCallCFunction)                                              \
  V(ArchSaveCallerRegisters)                                               \
  V(ArchRestoreCallerRegisters)                                            \
  V(ArchCallCFunction)                                                     \
  V(ArchPrepareTailCall)                                                   \
  V(ArchJmp)                                                               \
  V(ArchBinarySearchSwitch)                                                \
  V(ArchTableSwitch)                                                       \
  V(ArchNop)                                                               \
  V(ArchAbortCSADcheck)                                                    \
  V(ArchDebugBreak)                                                        \
  V(ArchComment)                                                           \
  V(ArchThrowTerminator)                                                   \
  V(ArchDeoptimize)                                                        \
  V(ArchRet)                                                               \
  V(ArchFramePointer)                                                      \
  V(ArchParentFramePointer)                                                \
  V(ArchTruncateDoubleToI)                                                 \
  V(ArchStackSlot)                                                         \
  V(ArchStackPointerGreaterThan)                                           \
  V(ArchStackCheckOffset)                                                  \
  V(Ieee754Float64Acos)                                                    \
  V(Ieee754Float64Acosh)                                                   \
  V(Ieee754Float64Asin)                                                    \
  V(Ieee754Float64Asinh)                                                   \
  V(Ieee754Float64Atan)                                                    \
  V(Ieee754Float64Atanh)                                                   \
  V(Ieee754Float64Atan2)                                                   \
  V(Ieee754Float64Cbrt)                                                    \
  V(Ieee754Float64Cos)                                                     \
  V(Ieee754Float64Cosh)                                                    \
  V(Ieee754Float64Exp)                                                     \
  V(Ieee754Float64Expm1)                                                   \
  V(Ieee754Float64Log)                                                     \
  V(Ieee754Float64Log1p)                                                   \
  V(Ieee754Float64Log10)                                                   \
  V(Ieee754Float64Log2)                                                    \
  V(Ieee754Float64Pow)                                                     \
  V(Ieee754Float64Sin)                                                     \
  V(Ieee754Float64Sinh)                                                    \
  V(Ieee754Float64Tan)                                                     \
  V(Ieee754Float64Tanh)                                                    \
  COMMON_ARCH_OPCODE_WITH_MEMORY_ACCESS_MODE_LIST(V)

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

enum AddressingMode : uint8_t {
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
  kFlags_deoptimize = 2,
  kFlags_set = 3,
  kFlags_trap = 4,
  kFlags_select = 5,
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
};

enum class AtomicWidth { kWord32, kWord64 };

inline size_t AtomicWidthSize(AtomicWidth width) {
  switch (width) {
    case AtomicWidth::kWord32:
      return 4;
    case AtomicWidth::kWord64:
      return 8;
  }
  UNREACHABLE();
}

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
static_assert(ArchOpcodeField::is_valid(kLastArchOpcode),
              "All opcodes must fit in the 9-bit ArchOpcodeField.");
using AddressingModeField = base::BitField<AddressingMode, 9, 5>;
static_assert(
    AddressingModeField::is_valid(kLastAddressingMode),
    "All addressing modes must fit in the 5-bit AddressingModeField.");
using FlagsModeField = base::BitField<FlagsMode, 14, 3>;
using FlagsConditionField = base::BitField<FlagsCondition, 17, 5>;
using MiscField = base::BitField<int, 22, 10>;

// {MiscField} is used for a variety of things, depending on the opcode.
// TODO(turbofan): There should be an abstraction that ensures safe encoding and
// decoding. {HasMemoryAccessMode} and its uses are a small step in that
// direction.

// LaneSizeField and AccessModeField are helper types to encode/decode a lane
// size, an access mode, or both inside the overlapping MiscField.
using LaneSizeField = base::BitField<int, 22, 8>;
using AccessModeField = base::BitField<MemoryAccessMode, 30, 1>;
// TODO(turbofan): {HasMemoryAccessMode} is currently only used to guard
// decoding (in CodeGenerator and InstructionScheduler). Encoding (in
// InstructionSelector) is not yet guarded. There are in fact instructions for
// which InstructionSelector does set a MemoryAccessMode but CodeGenerator
// doesn't care to consume it (e.g. kArm64LdrDecompressTaggedSigned). This is
// scary. {HasMemoryAccessMode} does not include these instructions, so they can
// be easily found by guarding encoding.
inline bool HasMemoryAccessMode(ArchOpcode opcode) {
#if defined(TARGET_ARCH_OPCODE_WITH_MEMORY_ACCESS_MODE_LIST)
  switch (opcode) {
#define CASE(Name) \
  case k##Name:    \
    return true;
    COMMON_ARCH_OPCODE_WITH_MEMORY_ACCESS_MODE_LIST(CASE)
    TARGET_ARCH_OPCODE_WITH_MEMORY_ACCESS_MODE_LIST(CASE)
#undef CASE
    default:
      return false;
  }
#else
  return false;
#endif
}

using DeoptImmedArgsCountField = base::BitField<int, 22, 2>;
using DeoptFrameStateOffsetField = base::BitField<int, 24, 8>;

// AtomicWidthField overlaps with MiscField and is used for the various Atomic
// opcodes. Only used on 64bit architectures. All atomic instructions on 32bit
// architectures are assumed to be 32bit wide.
using AtomicWidthField = base::BitField<AtomicWidth, 22, 2>;

// AtomicMemoryOrderField overlaps with MiscField and is used for the various
// Atomic opcodes. This field is not used on all architectures. It is used on
// architectures where the codegen for kSeqCst and kAcqRel differ only by
// emitting fences.
using AtomicMemoryOrderField = base::BitField<AtomicMemoryOrder, 24, 2>;
using AtomicStoreRecordWriteModeField = base::BitField<RecordWriteMode, 26, 4>;

// ParamField and FPParamField overlap with MiscField, as the latter is never
// used for Call instructions. These 2 fields represent the general purpose
// and floating point parameter counts of a direct call into C and are given 5
// bits each, which allow storing a number up to the current maximum parameter
// count, which is 20 (see kMaxCParameters defined in macro-assembler.h).
using ParamField = base::BitField<int, 22, 5>;
using FPParamField = base::BitField<int, 27, 5>;

// This static assertion serves as an early warning if we are about to exhaust
// the available opcode space. If we are about to exhaust it, we should start
// looking into options to compress some opcodes (see
// https://crbug.com/v8/12093) before we fully run out of available opcodes.
// Otherwise we risk being unable to land an important security fix or merge
// back fixes that add new opcodes.
// It is OK to temporarily reduce the required slack if we have a tracking bug
// to reduce the number of used opcodes again.
static_assert(ArchOpcodeField::kMax - kLastArchOpcode >= 16,
              "We are running close to the number of available opcodes.");

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_INSTRUCTION_CODES_H_

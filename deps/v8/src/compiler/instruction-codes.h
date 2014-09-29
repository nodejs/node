// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INSTRUCTION_CODES_H_
#define V8_COMPILER_INSTRUCTION_CODES_H_

#if V8_TARGET_ARCH_ARM
#include "src/compiler/arm/instruction-codes-arm.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/compiler/arm64/instruction-codes-arm64.h"
#elif V8_TARGET_ARCH_IA32
#include "src/compiler/ia32/instruction-codes-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/compiler/x64/instruction-codes-x64.h"
#else
#define TARGET_ARCH_OPCODE_LIST(V)
#define TARGET_ADDRESSING_MODE_LIST(V)
#endif
#include "src/utils.h"

namespace v8 {
namespace internal {

class OStream;

namespace compiler {

// Target-specific opcodes that specify which assembly sequence to emit.
// Most opcodes specify a single instruction.
#define ARCH_OPCODE_LIST(V) \
  V(ArchDeoptimize)         \
  V(ArchJmp)                \
  V(ArchNop)                \
  V(ArchRet)                \
  TARGET_ARCH_OPCODE_LIST(V)

enum ArchOpcode {
#define DECLARE_ARCH_OPCODE(Name) k##Name,
  ARCH_OPCODE_LIST(DECLARE_ARCH_OPCODE)
#undef DECLARE_ARCH_OPCODE
#define COUNT_ARCH_OPCODE(Name) +1
  kLastArchOpcode = -1 ARCH_OPCODE_LIST(COUNT_ARCH_OPCODE)
#undef COUNT_ARCH_OPCODE
};

OStream& operator<<(OStream& os, const ArchOpcode& ao);

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

OStream& operator<<(OStream& os, const AddressingMode& am);

// The mode of the flags continuation (see below).
enum FlagsMode { kFlags_none = 0, kFlags_branch = 1, kFlags_set = 2 };

OStream& operator<<(OStream& os, const FlagsMode& fm);

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
  kUnorderedEqual,
  kUnorderedNotEqual,
  kUnorderedLessThan,
  kUnorderedGreaterThanOrEqual,
  kUnorderedLessThanOrEqual,
  kUnorderedGreaterThan,
  kOverflow,
  kNotOverflow
};

OStream& operator<<(OStream& os, const FlagsCondition& fc);

// The InstructionCode is an opaque, target-specific integer that encodes
// what code to emit for an instruction in the code generator. It is not
// interesting to the register allocator, as the inputs and flags on the
// instructions specify everything of interest.
typedef int32_t InstructionCode;

// Helpers for encoding / decoding InstructionCode into the fields needed
// for code generation. We encode the instruction, addressing mode, and flags
// continuation into a single InstructionCode which is stored as part of
// the instruction.
typedef BitField<ArchOpcode, 0, 7> ArchOpcodeField;
typedef BitField<AddressingMode, 7, 4> AddressingModeField;
typedef BitField<FlagsMode, 11, 2> FlagsModeField;
typedef BitField<FlagsCondition, 13, 5> FlagsConditionField;
typedef BitField<int, 13, 19> MiscField;

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INSTRUCTION_CODES_H_

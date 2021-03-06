// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/torque-code-generator.h"

namespace v8 {
namespace internal {
namespace torque {

bool TorqueCodeGenerator::IsEmptyInstruction(const Instruction& instruction) {
  switch (instruction.kind()) {
    case InstructionKind::kPeekInstruction:
    case InstructionKind::kPokeInstruction:
    case InstructionKind::kDeleteRangeInstruction:
    case InstructionKind::kPushUninitializedInstruction:
    case InstructionKind::kPushBuiltinPointerInstruction:
    case InstructionKind::kUnsafeCastInstruction:
      return true;
    default:
      return false;
  }
}

void TorqueCodeGenerator::EmitInstruction(const Instruction& instruction,
                                          Stack<std::string>* stack) {
#ifdef DEBUG
  if (!IsEmptyInstruction(instruction)) {
    EmitSourcePosition(instruction->pos);
  }
#endif

  switch (instruction.kind()) {
#define ENUM_ITEM(T)          \
  case InstructionKind::k##T: \
    return EmitInstruction(instruction.Cast<T>(), stack);
    TORQUE_INSTRUCTION_LIST(ENUM_ITEM)
#undef ENUM_ITEM
  }
}

void TorqueCodeGenerator::EmitInstruction(const PeekInstruction& instruction,
                                          Stack<std::string>* stack) {
  stack->Push(stack->Peek(instruction.slot));
}

void TorqueCodeGenerator::EmitInstruction(const PokeInstruction& instruction,
                                          Stack<std::string>* stack) {
  stack->Poke(instruction.slot, stack->Top());
  stack->Pop();
}

void TorqueCodeGenerator::EmitInstruction(
    const DeleteRangeInstruction& instruction, Stack<std::string>* stack) {
  stack->DeleteRange(instruction.range);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

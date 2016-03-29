// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/register-translator.h"

#include "src/interpreter/bytecode-array-builder.h"

namespace v8 {
namespace internal {
namespace interpreter {

RegisterTranslator::RegisterTranslator(RegisterMover* mover)
    : mover_(mover),
      emitting_moves_(false),
      window_registers_count_(0),
      output_moves_count_(0) {}

void RegisterTranslator::TranslateInputRegisters(Bytecode bytecode,
                                                 uint32_t* raw_operands,
                                                 int raw_operand_count) {
  DCHECK_EQ(Bytecodes::NumberOfOperands(bytecode), raw_operand_count);
  if (!emitting_moves_) {
    emitting_moves_ = true;
    DCHECK_EQ(window_registers_count_, 0);
    int register_bitmap = Bytecodes::GetRegisterOperandBitmap(bytecode);
    for (int i = 0; i < raw_operand_count; i++) {
      if ((register_bitmap & (1 << i)) == 0) {
        continue;
      }
      Register in_reg = Register::FromRawOperand(raw_operands[i]);
      Register out_reg = TranslateAndMove(bytecode, i, in_reg);
      raw_operands[i] = out_reg.ToRawOperand();
    }
    window_registers_count_ = 0;
    emitting_moves_ = false;
  } else {
    // When the register translator is translating registers, it will
    // cause the bytecode generator to emit moves on it's behalf. This
    // path is reached by these moves.
    DCHECK(bytecode == Bytecode::kMovWide && raw_operand_count == 2 &&
           Register::FromRawOperand(raw_operands[0]).is_valid() &&
           Register::FromRawOperand(raw_operands[1]).is_valid());
  }
}

Register RegisterTranslator::TranslateAndMove(Bytecode bytecode,
                                              int operand_index, Register reg) {
  if (FitsInReg8Operand(reg)) {
    return reg;
  }

  OperandType operand_type = Bytecodes::GetOperandType(bytecode, operand_index);
  OperandSize operand_size = Bytecodes::SizeOfOperand(operand_type);
  if (operand_size == OperandSize::kShort) {
    CHECK(FitsInReg16Operand(reg));
    return Translate(reg);
  }

  CHECK((operand_type == OperandType::kReg8 ||
         operand_type == OperandType::kRegOut8) &&
        RegisterIsMovableToWindow(bytecode, operand_index));
  Register translated_reg = Translate(reg);
  Register window_reg(kTranslationWindowStart + window_registers_count_);
  window_registers_count_ += 1;
  if (Bytecodes::IsRegisterInputOperandType(operand_type)) {
    DCHECK(!Bytecodes::IsRegisterOutputOperandType(operand_type));
    mover()->MoveRegisterUntranslated(translated_reg, window_reg);
  } else if (Bytecodes::IsRegisterOutputOperandType(operand_type)) {
    DCHECK_LT(output_moves_count_, kTranslationWindowLength);
    output_moves_[output_moves_count_] =
        std::make_pair(window_reg, translated_reg);
    output_moves_count_ += 1;
  } else {
    UNREACHABLE();
  }
  return window_reg;
}

// static
bool RegisterTranslator::RegisterIsMovableToWindow(Bytecode bytecode,
                                                   int operand_index) {
  // By design, we only support moving individual registers. There
  // should be wide variants of such bytecodes instead to avoid the
  // need for a large translation window.
  OperandType operand_type = Bytecodes::GetOperandType(bytecode, operand_index);
  if (operand_type != OperandType::kReg8 &&
      operand_type != OperandType::kRegOut8) {
    return false;
  } else if (operand_index + 1 == Bytecodes::NumberOfOperands(bytecode)) {
    return true;
  } else {
    OperandType next_operand_type =
        Bytecodes::GetOperandType(bytecode, operand_index + 1);
    return (next_operand_type != OperandType::kRegCount8 &&
            next_operand_type != OperandType::kRegCount16);
  }
}

void RegisterTranslator::TranslateOutputRegisters() {
  if (!emitting_moves_) {
    emitting_moves_ = true;
    while (output_moves_count_ > 0) {
      output_moves_count_ -= 1;
      mover()->MoveRegisterUntranslated(
          output_moves_[output_moves_count_].first,
          output_moves_[output_moves_count_].second);
    }
    emitting_moves_ = false;
  }
}

// static
Register RegisterTranslator::Translate(Register reg) {
  if (reg.index() >= kTranslationWindowStart) {
    return Register(reg.index() + kTranslationWindowLength);
  } else {
    return reg;
  }
}

// static
bool RegisterTranslator::InTranslationWindow(Register reg) {
  return (reg.index() >= kTranslationWindowStart &&
          reg.index() <= kTranslationWindowLimit);
}

// static
Register RegisterTranslator::UntranslateRegister(Register reg) {
  if (reg.index() >= kTranslationWindowStart) {
    return Register(reg.index() - kTranslationWindowLength);
  } else {
    return reg;
  }
}

// static
int RegisterTranslator::DistanceToTranslationWindow(Register reg) {
  return kTranslationWindowStart - reg.index();
}

// static
bool RegisterTranslator::FitsInReg8Operand(Register reg) {
  return reg.is_byte_operand() && reg.index() < kTranslationWindowStart;
}

// static
bool RegisterTranslator::FitsInReg16Operand(Register reg) {
  int max_index = Register::MaxRegisterIndex() - kTranslationWindowLength + 1;
  return reg.is_short_operand() && reg.index() < max_index;
}

// static
int RegisterTranslator::RegisterCountAdjustment(int register_count,
                                                int parameter_count) {
  if (register_count > kTranslationWindowStart) {
    return kTranslationWindowLength;
  } else if (parameter_count > 0) {
    Register param0 = Register::FromParameterIndex(0, parameter_count);
    if (!param0.is_byte_operand()) {
      // TODO(oth): Number of parameters means translation is
      // required, but the translation window location is such that
      // some space is wasted. Hopefully a rare corner case, but could
      // relocate window to limit waste.
      return kTranslationWindowLimit + 1 - register_count;
    }
  }
  return 0;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

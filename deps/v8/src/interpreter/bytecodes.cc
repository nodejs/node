// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecodes.h"

#include "src/frames.h"
#include "src/interpreter/bytecode-traits.h"

namespace v8 {
namespace internal {
namespace interpreter {


// static
const char* Bytecodes::ToString(Bytecode bytecode) {
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return #Name;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return "";
}


// static
const char* Bytecodes::OperandTypeToString(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return #Name;
    OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return "";
}


// static
const char* Bytecodes::OperandSizeToString(OperandSize operand_size) {
  switch (operand_size) {
    case OperandSize::kNone:
      return "None";
    case OperandSize::kByte:
      return "Byte";
    case OperandSize::kShort:
      return "Short";
  }
  UNREACHABLE();
  return "";
}


// static
uint8_t Bytecodes::ToByte(Bytecode bytecode) {
  return static_cast<uint8_t>(bytecode);
}


// static
Bytecode Bytecodes::FromByte(uint8_t value) {
  Bytecode bytecode = static_cast<Bytecode>(value);
  DCHECK(bytecode <= Bytecode::kLast);
  return bytecode;
}


// static
int Bytecodes::Size(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__, OPERAND_TERM>::kSize;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return 0;
}


// static
int Bytecodes::NumberOfOperands(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__, OPERAND_TERM>::kOperandCount;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return 0;
}


// static
OperandType Bytecodes::GetOperandType(Bytecode bytecode, int i) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__, OPERAND_TERM>::GetOperandType(i);
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return OperandType::kNone;
}


// static
OperandSize Bytecodes::GetOperandSize(Bytecode bytecode, int i) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__, OPERAND_TERM>::GetOperandSize(i);
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return OperandSize::kNone;
}


// static
int Bytecodes::GetOperandOffset(Bytecode bytecode, int i) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__, OPERAND_TERM>::GetOperandOffset(i);
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return 0;
}


// static
OperandSize Bytecodes::SizeOfOperand(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, Size)     \
  case OperandType::k##Name: \
    return Size;
    OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return OperandSize::kNone;
}


// static
bool Bytecodes::IsConditionalJumpImmediate(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpIfTrue ||
         bytecode == Bytecode::kJumpIfFalse ||
         bytecode == Bytecode::kJumpIfToBooleanTrue ||
         bytecode == Bytecode::kJumpIfToBooleanFalse ||
         bytecode == Bytecode::kJumpIfNull ||
         bytecode == Bytecode::kJumpIfUndefined;
}


// static
bool Bytecodes::IsConditionalJumpConstant(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpIfTrueConstant ||
         bytecode == Bytecode::kJumpIfFalseConstant ||
         bytecode == Bytecode::kJumpIfToBooleanTrueConstant ||
         bytecode == Bytecode::kJumpIfToBooleanFalseConstant ||
         bytecode == Bytecode::kJumpIfNullConstant ||
         bytecode == Bytecode::kJumpIfUndefinedConstant;
}


// static
bool Bytecodes::IsConditionalJumpConstantWide(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpIfTrueConstantWide ||
         bytecode == Bytecode::kJumpIfFalseConstantWide ||
         bytecode == Bytecode::kJumpIfToBooleanTrueConstantWide ||
         bytecode == Bytecode::kJumpIfToBooleanFalseConstantWide ||
         bytecode == Bytecode::kJumpIfNullConstantWide ||
         bytecode == Bytecode::kJumpIfUndefinedConstantWide;
}


// static
bool Bytecodes::IsConditionalJump(Bytecode bytecode) {
  return IsConditionalJumpImmediate(bytecode) ||
         IsConditionalJumpConstant(bytecode) ||
         IsConditionalJumpConstantWide(bytecode);
}


// static
bool Bytecodes::IsJumpImmediate(Bytecode bytecode) {
  return bytecode == Bytecode::kJump || IsConditionalJumpImmediate(bytecode);
}


// static
bool Bytecodes::IsJumpConstant(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpConstant ||
         IsConditionalJumpConstant(bytecode);
}


// static
bool Bytecodes::IsJumpConstantWide(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpConstantWide ||
         IsConditionalJumpConstantWide(bytecode);
}


// static
bool Bytecodes::IsJump(Bytecode bytecode) {
  return IsJumpImmediate(bytecode) || IsJumpConstant(bytecode) ||
         IsJumpConstantWide(bytecode);
}


// static
bool Bytecodes::IsJumpOrReturn(Bytecode bytecode) {
  return bytecode == Bytecode::kReturn || IsJump(bytecode);
}


// static
std::ostream& Bytecodes::Decode(std::ostream& os, const uint8_t* bytecode_start,
                                int parameter_count) {
  Vector<char> buf = Vector<char>::New(50);

  Bytecode bytecode = Bytecodes::FromByte(bytecode_start[0]);
  int bytecode_size = Bytecodes::Size(bytecode);

  for (int i = 0; i < bytecode_size; i++) {
    SNPrintF(buf, "%02x ", bytecode_start[i]);
    os << buf.start();
  }
  const int kBytecodeColumnSize = 6;
  for (int i = bytecode_size; i < kBytecodeColumnSize; i++) {
    os << "   ";
  }

  os << bytecode << " ";

  int number_of_operands = NumberOfOperands(bytecode);
  for (int i = 0; i < number_of_operands; i++) {
    OperandType op_type = GetOperandType(bytecode, i);
    const uint8_t* operand_start =
        &bytecode_start[GetOperandOffset(bytecode, i)];
    switch (op_type) {
      case interpreter::OperandType::kCount8:
        os << "#" << static_cast<unsigned int>(*operand_start);
        break;
      case interpreter::OperandType::kCount16:
        os << '#' << ReadUnalignedUInt16(operand_start);
        break;
      case interpreter::OperandType::kIdx8:
        os << "[" << static_cast<unsigned int>(*operand_start) << "]";
        break;
      case interpreter::OperandType::kIdx16:
        os << "[" << ReadUnalignedUInt16(operand_start) << "]";
        break;
      case interpreter::OperandType::kImm8:
        os << "#" << static_cast<int>(static_cast<int8_t>(*operand_start));
        break;
      case interpreter::OperandType::kReg8:
      case interpreter::OperandType::kMaybeReg8: {
        Register reg = Register::FromOperand(*operand_start);
        if (reg.is_function_context()) {
          os << "<context>";
        } else if (reg.is_function_closure()) {
          os << "<closure>";
        } else if (reg.is_new_target()) {
          os << "<new.target>";
        } else if (reg.is_parameter()) {
          int parameter_index = reg.ToParameterIndex(parameter_count);
          if (parameter_index == 0) {
            os << "<this>";
          } else {
            os << "a" << parameter_index - 1;
          }
        } else {
          os << "r" << reg.index();
        }
        break;
      }
      case interpreter::OperandType::kRegPair8: {
        Register reg = Register::FromOperand(*operand_start);
        if (reg.is_parameter()) {
          int parameter_index = reg.ToParameterIndex(parameter_count);
          DCHECK_NE(parameter_index, 0);
          os << "a" << parameter_index - 1 << "-" << parameter_index;
        } else {
          os << "r" << reg.index() << "-" << reg.index() + 1;
        }
        break;
      }
      case interpreter::OperandType::kReg16: {
        Register reg =
            Register::FromWideOperand(ReadUnalignedUInt16(operand_start));
        if (reg.is_parameter()) {
          int parameter_index = reg.ToParameterIndex(parameter_count);
          DCHECK_NE(parameter_index, 0);
          os << "a" << parameter_index - 1;
        } else {
          os << "r" << reg.index();
        }
        break;
      }
      case interpreter::OperandType::kNone:
        UNREACHABLE();
        break;
    }
    if (i != number_of_operands - 1) {
      os << ", ";
    }
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, const Bytecode& bytecode) {
  return os << Bytecodes::ToString(bytecode);
}


std::ostream& operator<<(std::ostream& os, const OperandType& operand_type) {
  return os << Bytecodes::OperandTypeToString(operand_type);
}


std::ostream& operator<<(std::ostream& os, const OperandSize& operand_size) {
  return os << Bytecodes::OperandSizeToString(operand_size);
}


static const int kLastParamRegisterIndex =
    -InterpreterFrameConstants::kLastParamFromRegisterPointer / kPointerSize;
static const int kFunctionClosureRegisterIndex =
    -InterpreterFrameConstants::kFunctionFromRegisterPointer / kPointerSize;
static const int kFunctionContextRegisterIndex =
    -InterpreterFrameConstants::kContextFromRegisterPointer / kPointerSize;
static const int kNewTargetRegisterIndex =
    -InterpreterFrameConstants::kNewTargetFromRegisterPointer / kPointerSize;


// Registers occupy range 0-127 in 8-bit value leaving 128 unused values.
// Parameter indices are biased with the negative value kLastParamRegisterIndex
// for ease of access in the interpreter.
static const int kMaxParameterIndex = 128 + kLastParamRegisterIndex;


Register Register::FromParameterIndex(int index, int parameter_count) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, parameter_count);
  DCHECK_LE(parameter_count, kMaxParameterIndex + 1);
  int register_index = kLastParamRegisterIndex - parameter_count + index + 1;
  DCHECK_LT(register_index, 0);
  DCHECK_GE(register_index, kMinInt8);
  return Register(register_index);
}


int Register::ToParameterIndex(int parameter_count) const {
  DCHECK(is_parameter());
  return index() - kLastParamRegisterIndex + parameter_count - 1;
}


Register Register::function_closure() {
  return Register(kFunctionClosureRegisterIndex);
}


bool Register::is_function_closure() const {
  return index() == kFunctionClosureRegisterIndex;
}


Register Register::function_context() {
  return Register(kFunctionContextRegisterIndex);
}


bool Register::is_function_context() const {
  return index() == kFunctionContextRegisterIndex;
}


Register Register::new_target() { return Register(kNewTargetRegisterIndex); }


bool Register::is_new_target() const {
  return index() == kNewTargetRegisterIndex;
}


int Register::MaxParameterIndex() { return kMaxParameterIndex; }


uint8_t Register::ToOperand() const {
  DCHECK_GE(index_, kMinInt8);
  DCHECK_LE(index_, kMaxInt8);
  return static_cast<uint8_t>(-index_);
}


Register Register::FromOperand(uint8_t operand) {
  return Register(-static_cast<int8_t>(operand));
}


uint16_t Register::ToWideOperand() const {
  DCHECK_GE(index_, kMinInt16);
  DCHECK_LE(index_, kMaxInt16);
  return static_cast<uint16_t>(-index_);
}


Register Register::FromWideOperand(uint16_t operand) {
  return Register(-static_cast<int16_t>(operand));
}


bool Register::AreContiguous(Register reg1, Register reg2, Register reg3,
                             Register reg4, Register reg5) {
  if (reg1.index() + 1 != reg2.index()) {
    return false;
  }
  if (reg3.is_valid() && reg2.index() + 1 != reg3.index()) {
    return false;
  }
  if (reg4.is_valid() && reg3.index() + 1 != reg4.index()) {
    return false;
  }
  if (reg5.is_valid() && reg4.index() + 1 != reg5.index()) {
    return false;
  }
  return true;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

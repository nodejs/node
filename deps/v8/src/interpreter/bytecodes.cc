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
  DCHECK(bytecode <= Bytecode::kLast);
  return static_cast<uint8_t>(bytecode);
}


// static
Bytecode Bytecodes::FromByte(uint8_t value) {
  Bytecode bytecode = static_cast<Bytecode>(value);
  DCHECK(bytecode <= Bytecode::kLast);
  return bytecode;
}


// static
Bytecode Bytecodes::GetDebugBreak(Bytecode bytecode) {
  switch (Size(bytecode)) {
#define CASE(Name, ...)                                  \
  case BytecodeTraits<__VA_ARGS__, OPERAND_TERM>::kSize: \
    return Bytecode::k##Name;
    DEBUG_BREAK_BYTECODE_LIST(CASE)
#undef CASE
    default:
      break;
  }
  UNREACHABLE();
  return static_cast<Bytecode>(-1);
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
int Bytecodes::NumberOfRegisterOperands(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)                                            \
  case Bytecode::k##Name:                                          \
    typedef BytecodeTraits<__VA_ARGS__, OPERAND_TERM> Name##Trait; \
    return Name##Trait::kRegisterOperandCount;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
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
int Bytecodes::GetRegisterOperandBitmap(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)                                            \
  case Bytecode::k##Name:                                          \
    typedef BytecodeTraits<__VA_ARGS__, OPERAND_TERM> Name##Trait; \
    return Name##Trait::kRegisterOperandBitmap;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
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
         bytecode == Bytecode::kJumpIfNotHole ||
         bytecode == Bytecode::kJumpIfNull ||
         bytecode == Bytecode::kJumpIfUndefined;
}


// static
bool Bytecodes::IsConditionalJumpConstant(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpIfTrueConstant ||
         bytecode == Bytecode::kJumpIfFalseConstant ||
         bytecode == Bytecode::kJumpIfToBooleanTrueConstant ||
         bytecode == Bytecode::kJumpIfToBooleanFalseConstant ||
         bytecode == Bytecode::kJumpIfNotHoleConstant ||
         bytecode == Bytecode::kJumpIfNullConstant ||
         bytecode == Bytecode::kJumpIfUndefinedConstant;
}


// static
bool Bytecodes::IsConditionalJumpConstantWide(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpIfTrueConstantWide ||
         bytecode == Bytecode::kJumpIfFalseConstantWide ||
         bytecode == Bytecode::kJumpIfToBooleanTrueConstantWide ||
         bytecode == Bytecode::kJumpIfToBooleanFalseConstantWide ||
         bytecode == Bytecode::kJumpIfNotHoleConstantWide ||
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
bool Bytecodes::IsCallOrNew(Bytecode bytecode) {
  return bytecode == Bytecode::kCall || bytecode == Bytecode::kTailCall ||
         bytecode == Bytecode::kNew || bytecode == Bytecode::kCallWide ||
         bytecode == Bytecode::kTailCallWide || bytecode == Bytecode::kNewWide;
}

// static
bool Bytecodes::IsDebugBreak(Bytecode bytecode) {
  switch (bytecode) {
#define CASE(Name, ...) case Bytecode::k##Name:
    DEBUG_BREAK_BYTECODE_LIST(CASE);
#undef CASE
    return true;
    default:
      break;
  }
  return false;
}

// static
bool Bytecodes::IsJumpOrReturn(Bytecode bytecode) {
  return bytecode == Bytecode::kReturn || IsJump(bytecode);
}

// static
bool Bytecodes::IsIndexOperandType(OperandType operand_type) {
  return operand_type == OperandType::kIdx8 ||
         operand_type == OperandType::kIdx16;
}

// static
bool Bytecodes::IsImmediateOperandType(OperandType operand_type) {
  return operand_type == OperandType::kImm8;
}

// static
bool Bytecodes::IsRegisterCountOperandType(OperandType operand_type) {
  return (operand_type == OperandType::kRegCount8 ||
          operand_type == OperandType::kRegCount16);
}

// static
bool Bytecodes::IsMaybeRegisterOperandType(OperandType operand_type) {
  return (operand_type == OperandType::kMaybeReg8 ||
          operand_type == OperandType::kMaybeReg16);
}

// static
bool Bytecodes::IsRegisterOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return true;
    REGISTER_OPERAND_TYPE_LIST(CASE)
#undef CASE
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    break;
    NON_REGISTER_OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  return false;
}

// static
bool Bytecodes::IsRegisterInputOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return true;
    REGISTER_INPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    break;
    NON_REGISTER_OPERAND_TYPE_LIST(CASE)
    REGISTER_OUTPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  return false;
}

// static
bool Bytecodes::IsRegisterOutputOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return true;
    REGISTER_OUTPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    break;
    NON_REGISTER_OPERAND_TYPE_LIST(CASE)
    REGISTER_INPUT_OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  return false;
}

namespace {
static Register DecodeRegister(const uint8_t* operand_start,
                               OperandType operand_type) {
  switch (Bytecodes::SizeOfOperand(operand_type)) {
    case OperandSize::kByte:
      return Register::FromOperand(*operand_start);
    case OperandSize::kShort:
      return Register::FromWideOperand(ReadUnalignedUInt16(operand_start));
    case OperandSize::kNone: {
      UNREACHABLE();
    }
  }
  return Register();
}
}  // namespace


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

  // Operands for the debug break are from the original instruction.
  if (IsDebugBreak(bytecode)) return os;

  int number_of_operands = NumberOfOperands(bytecode);
  int range = 0;
  for (int i = 0; i < number_of_operands; i++) {
    OperandType op_type = GetOperandType(bytecode, i);
    const uint8_t* operand_start =
        &bytecode_start[GetOperandOffset(bytecode, i)];
    switch (op_type) {
      case interpreter::OperandType::kRegCount8:
        os << "#" << static_cast<unsigned int>(*operand_start);
        break;
      case interpreter::OperandType::kRegCount16:
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
      case interpreter::OperandType::kMaybeReg8:
      case interpreter::OperandType::kMaybeReg16:
      case interpreter::OperandType::kReg8:
      case interpreter::OperandType::kReg16:
      case interpreter::OperandType::kRegOut8:
      case interpreter::OperandType::kRegOut16: {
        Register reg = DecodeRegister(operand_start, op_type);
        os << reg.ToString(parameter_count);
        break;
      }
      case interpreter::OperandType::kRegOutTriple8:
      case interpreter::OperandType::kRegOutTriple16:
        range += 1;
      case interpreter::OperandType::kRegOutPair8:
      case interpreter::OperandType::kRegOutPair16:
      case interpreter::OperandType::kRegPair8:
      case interpreter::OperandType::kRegPair16: {
        range += 1;
        Register first_reg = DecodeRegister(operand_start, op_type);
        Register last_reg = Register(first_reg.index() + range);
        os << first_reg.ToString(parameter_count) << "-"
           << last_reg.ToString(parameter_count);
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
static const int kCurrentContextRegisterIndex =
    -InterpreterFrameConstants::kContextFromRegisterPointer / kPointerSize;
static const int kNewTargetRegisterIndex =
    -InterpreterFrameConstants::kNewTargetFromRegisterPointer / kPointerSize;

// The register space is a signed 16-bit space. Register operands
// occupy range above 0. Parameter indices are biased with the
// negative value kLastParamRegisterIndex for ease of access in the
// interpreter.
static const int kMaxParameterIndex = kMaxInt16 + kLastParamRegisterIndex;
static const int kMaxRegisterIndex = -kMinInt16;
static const int kMaxReg8Index = -kMinInt8;
static const int kMinReg8Index = -kMaxInt8;
static const int kMaxReg16Index = -kMinInt16;
static const int kMinReg16Index = -kMaxInt16;

bool Register::is_byte_operand() const {
  return index_ >= kMinReg8Index && index_ <= kMaxReg8Index;
}

bool Register::is_short_operand() const {
  return index_ >= kMinReg16Index && index_ <= kMaxReg16Index;
}

Register Register::FromParameterIndex(int index, int parameter_count) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, parameter_count);
  DCHECK_LE(parameter_count, kMaxParameterIndex + 1);
  int register_index = kLastParamRegisterIndex - parameter_count + index + 1;
  DCHECK_LT(register_index, 0);
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


Register Register::current_context() {
  return Register(kCurrentContextRegisterIndex);
}


bool Register::is_current_context() const {
  return index() == kCurrentContextRegisterIndex;
}


Register Register::new_target() { return Register(kNewTargetRegisterIndex); }


bool Register::is_new_target() const {
  return index() == kNewTargetRegisterIndex;
}

int Register::MaxParameterIndex() { return kMaxParameterIndex; }

int Register::MaxRegisterIndex() { return kMaxRegisterIndex; }

int Register::MaxRegisterIndexForByteOperand() { return kMaxReg8Index; }

uint8_t Register::ToOperand() const {
  DCHECK(is_byte_operand());
  return static_cast<uint8_t>(-index_);
}


Register Register::FromOperand(uint8_t operand) {
  return Register(-static_cast<int8_t>(operand));
}


uint16_t Register::ToWideOperand() const {
  DCHECK(is_short_operand());
  return static_cast<uint16_t>(-index_);
}


Register Register::FromWideOperand(uint16_t operand) {
  return Register(-static_cast<int16_t>(operand));
}


uint32_t Register::ToRawOperand() const {
  return static_cast<uint32_t>(-index_);
}


Register Register::FromRawOperand(uint32_t operand) {
  return Register(-static_cast<int32_t>(operand));
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

std::string Register::ToString(int parameter_count) {
  if (is_current_context()) {
    return std::string("<context>");
  } else if (is_function_closure()) {
    return std::string("<closure>");
  } else if (is_new_target()) {
    return std::string("<new.target>");
  } else if (is_parameter()) {
    int parameter_index = ToParameterIndex(parameter_count);
    if (parameter_index == 0) {
      return std::string("<this>");
    } else {
      std::ostringstream s;
      s << "a" << parameter_index - 1;
      return s.str();
    }
  } else {
    std::ostringstream s;
    s << "r" << index();
    return s.str();
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

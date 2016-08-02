// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecodes.h"

#include <iomanip>

#include "src/frames.h"
#include "src/interpreter/bytecode-traits.h"
#include "src/interpreter/interpreter.h"

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
std::string Bytecodes::ToString(Bytecode bytecode, OperandScale operand_scale) {
  static const char kSeparator = '.';

  std::string value(ToString(bytecode));
  if (operand_scale > OperandScale::kSingle) {
    Bytecode prefix_bytecode = OperandScaleToPrefixBytecode(operand_scale);
    std::string suffix = ToString(prefix_bytecode);
    return value.append(1, kSeparator).append(suffix);
  } else {
    return value;
  }
}

// static
const char* Bytecodes::AccumulatorUseToString(AccumulatorUse accumulator_use) {
  switch (accumulator_use) {
    case AccumulatorUse::kNone:
      return "None";
    case AccumulatorUse::kRead:
      return "Read";
    case AccumulatorUse::kWrite:
      return "Write";
    case AccumulatorUse::kReadWrite:
      return "ReadWrite";
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
const char* Bytecodes::OperandScaleToString(OperandScale operand_scale) {
  switch (operand_scale) {
#define CASE(Name, _)         \
  case OperandScale::k##Name: \
    return #Name;
    OPERAND_SCALE_LIST(CASE)
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
    case OperandSize::kQuad:
      return "Quad";
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
  DCHECK(!IsDebugBreak(bytecode));
  if (bytecode == Bytecode::kWide) {
    return Bytecode::kDebugBreakWide;
  }
  if (bytecode == Bytecode::kExtraWide) {
    return Bytecode::kDebugBreakExtraWide;
  }
  int bytecode_size = Size(bytecode, OperandScale::kSingle);
#define RETURN_IF_DEBUG_BREAK_SIZE_MATCHES(Name, ...)                    \
  if (bytecode_size == Size(Bytecode::k##Name, OperandScale::kSingle)) { \
    return Bytecode::k##Name;                                            \
  }
  DEBUG_BREAK_PLAIN_BYTECODE_LIST(RETURN_IF_DEBUG_BREAK_SIZE_MATCHES)
#undef RETURN_IF_DEBUG_BREAK_SIZE_MATCHES
  UNREACHABLE();
  return Bytecode::kIllegal;
}

// static
int Bytecodes::Size(Bytecode bytecode, OperandScale operand_scale) {
  int size = 1;
  for (int i = 0; i < NumberOfOperands(bytecode); i++) {
    OperandSize operand_size = GetOperandSize(bytecode, i, operand_scale);
    int delta = static_cast<int>(operand_size);
    DCHECK(base::bits::IsPowerOfTwo32(static_cast<uint32_t>(delta)));
    size += delta;
  }
  return size;
}


// static
size_t Bytecodes::ReturnCount(Bytecode bytecode) {
  return bytecode == Bytecode::kReturn ? 1 : 0;
}

// static
int Bytecodes::NumberOfOperands(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__>::kOperandCount;
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
#define CASE(Name, ...)                              \
  case Bytecode::k##Name:                            \
    typedef BytecodeTraits<__VA_ARGS__> Name##Trait; \
    return Name##Trait::kRegisterOperandCount;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
}

// static
Bytecode Bytecodes::OperandScaleToPrefixBytecode(OperandScale operand_scale) {
  switch (operand_scale) {
    case OperandScale::kQuadruple:
      return Bytecode::kExtraWide;
    case OperandScale::kDouble:
      return Bytecode::kWide;
    default:
      UNREACHABLE();
      return Bytecode::kIllegal;
  }
}

// static
bool Bytecodes::OperandScaleRequiresPrefixBytecode(OperandScale operand_scale) {
  return operand_scale != OperandScale::kSingle;
}

// static
OperandScale Bytecodes::PrefixBytecodeToOperandScale(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kExtraWide:
    case Bytecode::kDebugBreakExtraWide:
      return OperandScale::kQuadruple;
    case Bytecode::kWide:
    case Bytecode::kDebugBreakWide:
      return OperandScale::kDouble;
    default:
      UNREACHABLE();
      return OperandScale::kSingle;
  }
}

// static
AccumulatorUse Bytecodes::GetAccumulatorUse(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__>::kAccumulatorUse;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return AccumulatorUse::kNone;
}

// static
bool Bytecodes::ReadsAccumulator(Bytecode bytecode) {
  return (GetAccumulatorUse(bytecode) & AccumulatorUse::kRead) ==
         AccumulatorUse::kRead;
}

// static
bool Bytecodes::WritesAccumulator(Bytecode bytecode) {
  return (GetAccumulatorUse(bytecode) & AccumulatorUse::kWrite) ==
         AccumulatorUse::kWrite;
}

// static
bool Bytecodes::WritesBooleanToAccumulator(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kLdaTrue:
    case Bytecode::kLdaFalse:
    case Bytecode::kToBooleanLogicalNot:
    case Bytecode::kLogicalNot:
    case Bytecode::kTestEqual:
    case Bytecode::kTestNotEqual:
    case Bytecode::kTestEqualStrict:
    case Bytecode::kTestLessThan:
    case Bytecode::kTestLessThanOrEqual:
    case Bytecode::kTestGreaterThan:
    case Bytecode::kTestGreaterThanOrEqual:
    case Bytecode::kTestInstanceOf:
    case Bytecode::kTestIn:
    case Bytecode::kForInDone:
      return true;
    default:
      return false;
  }
}

// static
bool Bytecodes::IsAccumulatorLoadWithoutEffects(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kLdaZero:
    case Bytecode::kLdaSmi:
    case Bytecode::kLdaUndefined:
    case Bytecode::kLdaNull:
    case Bytecode::kLdaTheHole:
    case Bytecode::kLdaTrue:
    case Bytecode::kLdaFalse:
    case Bytecode::kLdaConstant:
    case Bytecode::kLdar:
      return true;
    default:
      return false;
  }
}

// static
OperandType Bytecodes::GetOperandType(Bytecode bytecode, int i) {
  DCHECK_LE(bytecode, Bytecode::kLast);
  DCHECK_LT(i, NumberOfOperands(bytecode));
  DCHECK_GE(i, 0);
  return GetOperandTypes(bytecode)[i];
}

// static
const OperandType* Bytecodes::GetOperandTypes(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__>::GetOperandTypes();
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return nullptr;
}

// static
OperandSize Bytecodes::GetOperandSize(Bytecode bytecode, int i,
                                      OperandScale operand_scale) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__>::GetOperandSize(i, operand_scale);
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
#define CASE(Name, ...)                              \
  case Bytecode::k##Name:                            \
    typedef BytecodeTraits<__VA_ARGS__> Name##Trait; \
    return Name##Trait::kRegisterOperandBitmap;
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
}

// static
int Bytecodes::GetOperandOffset(Bytecode bytecode, int i,
                                OperandScale operand_scale) {
  DCHECK_LT(i, Bytecodes::NumberOfOperands(bytecode));
  // TODO(oth): restore this to a statically determined constant.
  int offset = 1;
  for (int operand_index = 0; operand_index < i; ++operand_index) {
    OperandSize operand_size =
        GetOperandSize(bytecode, operand_index, operand_scale);
    offset += static_cast<int>(operand_size);
  }
  return offset;
}

// static
OperandSize Bytecodes::SizeOfOperand(OperandType operand_type,
                                     OperandScale operand_scale) {
  return static_cast<OperandSize>(
      ScaledOperandSize(operand_type, operand_scale));
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
bool Bytecodes::IsConditionalJump(Bytecode bytecode) {
  return IsConditionalJumpImmediate(bytecode) ||
         IsConditionalJumpConstant(bytecode);
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
bool Bytecodes::IsJump(Bytecode bytecode) {
  return IsJumpImmediate(bytecode) || IsJumpConstant(bytecode);
}

// static
bool Bytecodes::IsJumpIfToBoolean(Bytecode bytecode) {
  return bytecode == Bytecode::kJumpIfToBooleanTrue ||
         bytecode == Bytecode::kJumpIfToBooleanFalse ||
         bytecode == Bytecode::kJumpIfToBooleanTrueConstant ||
         bytecode == Bytecode::kJumpIfToBooleanFalseConstant;
}

// static
Bytecode Bytecodes::GetJumpWithoutToBoolean(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kJumpIfToBooleanTrue:
      return Bytecode::kJumpIfTrue;
    case Bytecode::kJumpIfToBooleanFalse:
      return Bytecode::kJumpIfFalse;
    case Bytecode::kJumpIfToBooleanTrueConstant:
      return Bytecode::kJumpIfTrueConstant;
    case Bytecode::kJumpIfToBooleanFalseConstant:
      return Bytecode::kJumpIfFalseConstant;
    default:
      break;
  }
  UNREACHABLE();
  return Bytecode::kIllegal;
}

// static
bool Bytecodes::IsCallOrNew(Bytecode bytecode) {
  return bytecode == Bytecode::kCall || bytecode == Bytecode::kTailCall ||
         bytecode == Bytecode::kNew;
}

// static
bool Bytecodes::IsCallRuntime(Bytecode bytecode) {
  return bytecode == Bytecode::kCallRuntime ||
         bytecode == Bytecode::kCallRuntimeForPair ||
         bytecode == Bytecode::kInvokeIntrinsic;
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
bool Bytecodes::IsLdarOrStar(Bytecode bytecode) {
  return bytecode == Bytecode::kLdar || bytecode == Bytecode::kStar;
}

// static
bool Bytecodes::IsBytecodeWithScalableOperands(Bytecode bytecode) {
  switch (bytecode) {
#define CASE(Name, ...)                              \
  case Bytecode::k##Name:                            \
    typedef BytecodeTraits<__VA_ARGS__> Name##Trait; \
    return Name##Trait::IsScalable();
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
}

// static
bool Bytecodes::IsPrefixScalingBytecode(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kExtraWide:
    case Bytecode::kDebugBreakExtraWide:
    case Bytecode::kWide:
    case Bytecode::kDebugBreakWide:
      return true;
    default:
      return false;
  }
}

// static
bool Bytecodes::IsJumpOrReturn(Bytecode bytecode) {
  return bytecode == Bytecode::kReturn || IsJump(bytecode);
}

// static
bool Bytecodes::IsMaybeRegisterOperandType(OperandType operand_type) {
  return operand_type == OperandType::kMaybeReg;
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

// static
int Bytecodes::GetNumberOfRegistersRepresentedBy(OperandType operand_type) {
  switch (operand_type) {
    case OperandType::kMaybeReg:
    case OperandType::kReg:
    case OperandType::kRegOut:
      return 1;
    case OperandType::kRegPair:
    case OperandType::kRegOutPair:
      return 2;
    case OperandType::kRegOutTriple:
      return 3;
    default:
      UNREACHABLE();
  }
  return 0;
}

// static
bool Bytecodes::IsUnsignedOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return OperandTraits<OperandType::k##Name>::TypeInfo::kIsUnsigned;
    OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
}

// static
OperandSize Bytecodes::SizeForSignedOperand(int value) {
  if (kMinInt8 <= value && value <= kMaxInt8) {
    return OperandSize::kByte;
  } else if (kMinInt16 <= value && value <= kMaxInt16) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
}

// static
OperandSize Bytecodes::SizeForUnsignedOperand(int value) {
  DCHECK_GE(value, 0);
  if (value <= kMaxUInt8) {
    return OperandSize::kByte;
  } else if (value <= kMaxUInt16) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
}

OperandSize Bytecodes::SizeForUnsignedOperand(size_t value) {
  if (value <= static_cast<size_t>(kMaxUInt8)) {
    return OperandSize::kByte;
  } else if (value <= static_cast<size_t>(kMaxUInt16)) {
    return OperandSize::kShort;
  } else if (value <= kMaxUInt32) {
    return OperandSize::kQuad;
  } else {
    UNREACHABLE();
    return OperandSize::kQuad;
  }
}

OperandScale Bytecodes::OperandSizesToScale(OperandSize size0,
                                            OperandSize size1,
                                            OperandSize size2,
                                            OperandSize size3) {
  OperandSize upper = std::max(size0, size1);
  OperandSize lower = std::max(size2, size3);
  OperandSize result = std::max(upper, lower);
  // Operand sizes have been scaled before calling this function.
  // Currently all scalable operands are byte sized at
  // OperandScale::kSingle.
  STATIC_ASSERT(static_cast<int>(OperandSize::kByte) ==
                    static_cast<int>(OperandScale::kSingle) &&
                static_cast<int>(OperandSize::kShort) ==
                    static_cast<int>(OperandScale::kDouble) &&
                static_cast<int>(OperandSize::kQuad) ==
                    static_cast<int>(OperandScale::kQuadruple));
  OperandScale operand_scale = static_cast<OperandScale>(result);
  DCHECK(operand_scale == OperandScale::kSingle ||
         operand_scale == OperandScale::kDouble ||
         operand_scale == OperandScale::kQuadruple);
  return operand_scale;
}

// static
Register Bytecodes::DecodeRegisterOperand(const uint8_t* operand_start,
                                          OperandType operand_type,
                                          OperandScale operand_scale) {
  DCHECK(Bytecodes::IsRegisterOperandType(operand_type));
  int32_t operand =
      DecodeSignedOperand(operand_start, operand_type, operand_scale);
  return Register::FromOperand(operand);
}

// static
int32_t Bytecodes::DecodeSignedOperand(const uint8_t* operand_start,
                                       OperandType operand_type,
                                       OperandScale operand_scale) {
  DCHECK(!Bytecodes::IsUnsignedOperandType(operand_type));
  switch (Bytecodes::SizeOfOperand(operand_type, operand_scale)) {
    case OperandSize::kByte:
      return static_cast<int8_t>(*operand_start);
    case OperandSize::kShort:
      return static_cast<int16_t>(ReadUnalignedUInt16(operand_start));
    case OperandSize::kQuad:
      return static_cast<int32_t>(ReadUnalignedUInt32(operand_start));
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return 0;
}

// static
uint32_t Bytecodes::DecodeUnsignedOperand(const uint8_t* operand_start,
                                          OperandType operand_type,
                                          OperandScale operand_scale) {
  DCHECK(Bytecodes::IsUnsignedOperandType(operand_type));
  switch (Bytecodes::SizeOfOperand(operand_type, operand_scale)) {
    case OperandSize::kByte:
      return *operand_start;
    case OperandSize::kShort:
      return ReadUnalignedUInt16(operand_start);
    case OperandSize::kQuad:
      return ReadUnalignedUInt32(operand_start);
    case OperandSize::kNone:
      UNREACHABLE();
  }
  return 0;
}

// static
std::ostream& Bytecodes::Decode(std::ostream& os, const uint8_t* bytecode_start,
                                int parameter_count) {
  Bytecode bytecode = Bytecodes::FromByte(bytecode_start[0]);
  int prefix_offset = 0;
  OperandScale operand_scale = OperandScale::kSingle;
  if (IsPrefixScalingBytecode(bytecode)) {
    prefix_offset = 1;
    operand_scale = Bytecodes::PrefixBytecodeToOperandScale(bytecode);
    bytecode = Bytecodes::FromByte(bytecode_start[1]);
  }

  // Prepare to print bytecode and operands as hex digits.
  std::ios saved_format(nullptr);
  saved_format.copyfmt(saved_format);
  os.fill('0');
  os.flags(std::ios::hex);

  int bytecode_size = Bytecodes::Size(bytecode, operand_scale);
  for (int i = 0; i < prefix_offset + bytecode_size; i++) {
    os << std::setw(2) << static_cast<uint32_t>(bytecode_start[i]) << ' ';
  }
  os.copyfmt(saved_format);

  const int kBytecodeColumnSize = 6;
  for (int i = prefix_offset + bytecode_size; i < kBytecodeColumnSize; i++) {
    os << "   ";
  }

  os << Bytecodes::ToString(bytecode, operand_scale) << " ";

  // Operands for the debug break are from the original instruction.
  if (IsDebugBreak(bytecode)) return os;

  int number_of_operands = NumberOfOperands(bytecode);
  int range = 0;
  for (int i = 0; i < number_of_operands; i++) {
    OperandType op_type = GetOperandType(bytecode, i);
    const uint8_t* operand_start =
        &bytecode_start[prefix_offset +
                        GetOperandOffset(bytecode, i, operand_scale)];
    switch (op_type) {
      case interpreter::OperandType::kRegCount:
        os << "#"
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale);
        break;
      case interpreter::OperandType::kIdx:
      case interpreter::OperandType::kRuntimeId:
        os << "["
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale)
           << "]";
        break;
      case interpreter::OperandType::kImm:
        os << "[" << DecodeSignedOperand(operand_start, op_type, operand_scale)
           << "]";
        break;
      case interpreter::OperandType::kFlag8:
        os << "#"
           << DecodeUnsignedOperand(operand_start, op_type, operand_scale);
        break;
      case interpreter::OperandType::kMaybeReg:
      case interpreter::OperandType::kReg:
      case interpreter::OperandType::kRegOut: {
        Register reg =
            DecodeRegisterOperand(operand_start, op_type, operand_scale);
        os << reg.ToString(parameter_count);
        break;
      }
      case interpreter::OperandType::kRegOutTriple:
        range += 1;
      case interpreter::OperandType::kRegOutPair:
      case interpreter::OperandType::kRegPair: {
        range += 1;
        Register first_reg =
            DecodeRegisterOperand(operand_start, op_type, operand_scale);
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

// static
bool Bytecodes::BytecodeHasHandler(Bytecode bytecode,
                                   OperandScale operand_scale) {
  return operand_scale == OperandScale::kSingle ||
         Bytecodes::IsBytecodeWithScalableOperands(bytecode);
}

std::ostream& operator<<(std::ostream& os, const Bytecode& bytecode) {
  return os << Bytecodes::ToString(bytecode);
}

std::ostream& operator<<(std::ostream& os, const AccumulatorUse& use) {
  return os << Bytecodes::AccumulatorUseToString(use);
}

std::ostream& operator<<(std::ostream& os, const OperandSize& operand_size) {
  return os << Bytecodes::OperandSizeToString(operand_size);
}

std::ostream& operator<<(std::ostream& os, const OperandScale& operand_scale) {
  return os << Bytecodes::OperandScaleToString(operand_scale);
}

std::ostream& operator<<(std::ostream& os, const OperandType& operand_type) {
  return os << Bytecodes::OperandTypeToString(operand_type);
}

static const int kLastParamRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kLastParamFromFp) /
    kPointerSize;
static const int kFunctionClosureRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     StandardFrameConstants::kFunctionOffset) /
    kPointerSize;
static const int kCurrentContextRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     StandardFrameConstants::kContextOffset) /
    kPointerSize;
static const int kNewTargetRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kNewTargetFromFp) /
    kPointerSize;
static const int kBytecodeArrayRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kBytecodeArrayFromFp) /
    kPointerSize;
static const int kBytecodeOffsetRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kBytecodeOffsetFromFp) /
    kPointerSize;

Register Register::FromParameterIndex(int index, int parameter_count) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, parameter_count);
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

Register Register::bytecode_array() {
  return Register(kBytecodeArrayRegisterIndex);
}

bool Register::is_bytecode_array() const {
  return index() == kBytecodeArrayRegisterIndex;
}

Register Register::bytecode_offset() {
  return Register(kBytecodeOffsetRegisterIndex);
}

bool Register::is_bytecode_offset() const {
  return index() == kBytecodeOffsetRegisterIndex;
}

OperandSize Register::SizeOfOperand() const {
  int32_t operand = ToOperand();
  if (operand >= kMinInt8 && operand <= kMaxInt8) {
    return OperandSize::kByte;
  } else if (operand >= kMinInt16 && operand <= kMaxInt16) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
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

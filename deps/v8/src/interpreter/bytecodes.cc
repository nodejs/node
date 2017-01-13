// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecodes.h"

#include <iomanip>

#include "src/base/bits.h"
#include "src/globals.h"
#include "src/interpreter/bytecode-traits.h"

namespace v8 {
namespace internal {
namespace interpreter {

STATIC_CONST_MEMBER_DEFINITION const int Bytecodes::kMaxOperands;

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
  DCHECK_LE(bytecode, Bytecode::kLast);
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
bool Bytecodes::IsJumpWithoutEffects(Bytecode bytecode) {
  return IsJump(bytecode) && !IsJumpIfToBoolean(bytecode);
}

// static
bool Bytecodes::IsRegisterLoadWithoutEffects(Bytecode bytecode) {
  switch (bytecode) {
    case Bytecode::kMov:
    case Bytecode::kPopContext:
    case Bytecode::kPushContext:
    case Bytecode::kStar:
    case Bytecode::kLdrUndefined:
      return true;
    default:
      return false;
  }
}

// static
bool Bytecodes::IsWithoutExternalSideEffects(Bytecode bytecode) {
  // These bytecodes only manipulate interpreter frame state and will
  // never throw.
  return (IsAccumulatorLoadWithoutEffects(bytecode) ||
          IsRegisterLoadWithoutEffects(bytecode) ||
          bytecode == Bytecode::kNop || IsJumpWithoutEffects(bytecode));
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
const OperandTypeInfo* Bytecodes::GetOperandTypeInfos(Bytecode bytecode) {
  DCHECK(bytecode <= Bytecode::kLast);
  switch (bytecode) {
#define CASE(Name, ...)   \
  case Bytecode::k##Name: \
    return BytecodeTraits<__VA_ARGS__>::GetOperandTypeInfos();
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return nullptr;
}

// static
OperandSize Bytecodes::GetOperandSize(Bytecode bytecode, int i,
                                      OperandScale operand_scale) {
  DCHECK_LT(i, NumberOfOperands(bytecode));
  OperandType operand_type = GetOperandType(bytecode, i);
  return SizeOfOperand(operand_type, operand_scale);
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
bool Bytecodes::PutsNameInAccumulator(Bytecode bytecode) {
  return bytecode == Bytecode::kTypeOf;
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
bool Bytecodes::IsStarLookahead(Bytecode bytecode, OperandScale operand_scale) {
  if (operand_scale == OperandScale::kSingle) {
    switch (bytecode) {
      case Bytecode::kLdaZero:
      case Bytecode::kLdaSmi:
      case Bytecode::kLdaNull:
      case Bytecode::kLdaTheHole:
      case Bytecode::kLdaConstant:
      case Bytecode::kAdd:
      case Bytecode::kSub:
      case Bytecode::kMul:
      case Bytecode::kAddSmi:
      case Bytecode::kSubSmi:
      case Bytecode::kInc:
      case Bytecode::kDec:
      case Bytecode::kTypeOf:
      case Bytecode::kCall:
      case Bytecode::kNew:
        return true;
      default:
        return false;
    }
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
      return 0;
  }
  return 0;
}

// static
bool Bytecodes::IsUnsignedOperandType(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return OperandTraits<OperandType::k##Name>::TypeInfoTraits::kIsUnsigned;
    OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
  return false;
}

// static
OperandSize Bytecodes::SizeForSignedOperand(int value) {
  if (value >= kMinInt8 && value <= kMaxInt8) {
    return OperandSize::kByte;
  } else if (value >= kMinInt16 && value <= kMaxInt16) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
}

// static
OperandSize Bytecodes::SizeForUnsignedOperand(uint32_t value) {
  if (value <= kMaxUInt8) {
    return OperandSize::kByte;
  } else if (value <= kMaxUInt16) {
    return OperandSize::kShort;
  } else {
    return OperandSize::kQuad;
  }
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

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

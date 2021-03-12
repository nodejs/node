// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-operands.h"

#include <iomanip>

namespace v8 {
namespace internal {
namespace interpreter {

namespace {

const char* ImplicitRegisterUseToString(
    ImplicitRegisterUse implicit_register_use) {
  switch (implicit_register_use) {
    case ImplicitRegisterUse::kNone:
      return "None";
    case ImplicitRegisterUse::kReadAccumulator:
      return "ReadAccumulator";
    case ImplicitRegisterUse::kWriteAccumulator:
      return "WriteAccumulator";
    case ImplicitRegisterUse::kWriteShortStar:
      return "WriteShortStar";
    case ImplicitRegisterUse::kReadWriteAccumulator:
      return "ReadWriteAccumulator";
    case ImplicitRegisterUse::kReadAccumulatorWriteShortStar:
      return "ReadAccumulatorWriteShortStar";
  }
  UNREACHABLE();
}

const char* OperandTypeToString(OperandType operand_type) {
  switch (operand_type) {
#define CASE(Name, _)        \
  case OperandType::k##Name: \
    return #Name;
    OPERAND_TYPE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
}

const char* OperandScaleToString(OperandScale operand_scale) {
  switch (operand_scale) {
#define CASE(Name, _)         \
  case OperandScale::k##Name: \
    return #Name;
    OPERAND_SCALE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
}

const char* OperandSizeToString(OperandSize operand_size) {
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
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const ImplicitRegisterUse& use) {
  return os << ImplicitRegisterUseToString(use);
}

std::ostream& operator<<(std::ostream& os, const OperandSize& operand_size) {
  return os << OperandSizeToString(operand_size);
}

std::ostream& operator<<(std::ostream& os, const OperandScale& operand_scale) {
  return os << OperandScaleToString(operand_scale);
}

std::ostream& operator<<(std::ostream& os, const OperandType& operand_type) {
  return os << OperandTypeToString(operand_type);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

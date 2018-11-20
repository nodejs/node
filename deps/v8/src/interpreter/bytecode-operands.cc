// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-operands.h"

#include <iomanip>

namespace v8 {
namespace internal {
namespace interpreter {

namespace {

const char* AccumulatorUseToString(AccumulatorUse accumulator_use) {
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

std::ostream& operator<<(std::ostream& os, const AccumulatorUse& use) {
  return os << AccumulatorUseToString(use);
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

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-register.h"

namespace v8 {
namespace internal {
namespace interpreter {

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
static const int kCallerPCOffsetRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kCallerPCOffsetFromFp) /
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

// static
Register Register::virtual_accumulator() {
  return Register(kCallerPCOffsetRegisterIndex);
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

std::string Register::ToString(int parameter_count) const {
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

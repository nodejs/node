// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-register.h"

namespace v8 {
namespace internal {
namespace interpreter {

static const int kFirstParamRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kFirstParamFromFp) /
    kSystemPointerSize;
static const int kFunctionClosureRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     StandardFrameConstants::kFunctionOffset) /
    kSystemPointerSize;
static const int kCurrentContextRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     StandardFrameConstants::kContextOffset) /
    kSystemPointerSize;
static const int kBytecodeArrayRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kBytecodeArrayFromFp) /
    kSystemPointerSize;
static const int kBytecodeOffsetRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kBytecodeOffsetFromFp) /
    kSystemPointerSize;
static const int kCallerPCOffsetRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kCallerPCOffset) /
    kSystemPointerSize;
static const int kArgumentCountRegisterIndex =
    (InterpreterFrameConstants::kRegisterFileFromFp -
     InterpreterFrameConstants::kArgCOffset) /
    kSystemPointerSize;

Register Register::FromParameterIndex(int index) {
  DCHECK_GE(index, 0);
  int register_index = kFirstParamRegisterIndex - index;
  DCHECK_LT(register_index, 0);
  return Register(register_index);
}

int Register::ToParameterIndex() const {
  DCHECK(is_parameter());
  return kFirstParamRegisterIndex - index();
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

// static
Register Register::argument_count() {
  return Register(kArgumentCountRegisterIndex);
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

std::string Register::ToString() const {
  if (is_current_context()) {
    return std::string("<context>");
  } else if (is_function_closure()) {
    return std::string("<closure>");
  } else if (*this == virtual_accumulator()) {
    return std::string("<accumulator>");
  } else if (is_parameter()) {
    int parameter_index = ToParameterIndex();
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

// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_REGISTER_H_
#define V8_INTERPRETER_BYTECODE_REGISTER_H_

#include "src/interpreter/bytecodes.h"

#include "src/frames.h"

namespace v8 {
namespace internal {
namespace interpreter {

// An interpreter Register which is located in the function's Register file
// in its stack-frame. Register hold parameters, this, and expression values.
class Register final {
 public:
  explicit Register(int index = kInvalidIndex) : index_(index) {}

  int index() const { return index_; }
  bool is_parameter() const { return index() < 0; }
  bool is_valid() const { return index_ != kInvalidIndex; }

  static Register FromParameterIndex(int index, int parameter_count);
  int ToParameterIndex(int parameter_count) const;

  // Returns an invalid register.
  static Register invalid_value() { return Register(); }

  // Returns the register for the function's closure object.
  static Register function_closure();
  bool is_function_closure() const;

  // Returns the register which holds the current context object.
  static Register current_context();
  bool is_current_context() const;

  // Returns the register for the incoming new target value.
  static Register new_target();
  bool is_new_target() const;

  // Returns the register for the bytecode array.
  static Register bytecode_array();
  bool is_bytecode_array() const;

  // Returns the register for the saved bytecode offset.
  static Register bytecode_offset();
  bool is_bytecode_offset() const;

  // Returns a register that can be used to represent the accumulator
  // within code in the interpreter, but should never be emitted in
  // bytecode.
  static Register virtual_accumulator();

  OperandSize SizeOfOperand() const;

  int32_t ToOperand() const { return kRegisterFileStartOffset - index_; }
  static Register FromOperand(int32_t operand) {
    return Register(kRegisterFileStartOffset - operand);
  }

  static bool AreContiguous(Register reg1, Register reg2,
                            Register reg3 = Register(),
                            Register reg4 = Register(),
                            Register reg5 = Register());

  std::string ToString(int parameter_count);

  bool operator==(const Register& other) const {
    return index() == other.index();
  }
  bool operator!=(const Register& other) const {
    return index() != other.index();
  }
  bool operator<(const Register& other) const {
    return index() < other.index();
  }
  bool operator<=(const Register& other) const {
    return index() <= other.index();
  }
  bool operator>(const Register& other) const {
    return index() > other.index();
  }
  bool operator>=(const Register& other) const {
    return index() >= other.index();
  }

 private:
  static const int kInvalidIndex = kMaxInt;
  static const int kRegisterFileStartOffset =
      InterpreterFrameConstants::kRegisterFileFromFp / kPointerSize;

  void* operator new(size_t size) = delete;
  void operator delete(void* p) = delete;

  int index_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_REGISTER_H_

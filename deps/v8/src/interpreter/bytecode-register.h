// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_REGISTER_H_
#define V8_INTERPRETER_BYTECODE_REGISTER_H_

#include "src/interpreter/bytecodes.h"

#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/common/globals.h"
#include "src/execution/frame-constants.h"

namespace v8 {
namespace internal {
namespace interpreter {

// An interpreter Register which is located in the function's Register file
// in its stack-frame. Register hold parameters, this, and expression values.
class V8_EXPORT_PRIVATE Register final {
 public:
  constexpr explicit Register(int index = kInvalidIndex) : index_(index) {}

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

  // Returns the register for the bytecode array.
  static Register bytecode_array();
  bool is_bytecode_array() const;

  // Returns the register for the saved bytecode offset.
  static Register bytecode_offset();
  bool is_bytecode_offset() const;

  // Returns the register for the argument count.
  static Register argument_count();

  // Returns a register that can be used to represent the accumulator
  // within code in the interpreter, but should never be emitted in
  // bytecode.
  static Register virtual_accumulator();

  OperandSize SizeOfOperand() const;

  constexpr int32_t ToOperand() const {
    return kRegisterFileStartOffset - index_;
  }
  static Register FromOperand(int32_t operand) {
    return Register(kRegisterFileStartOffset - operand);
  }

  static Register FromShortStar(Bytecode bytecode) {
    DCHECK(Bytecodes::IsShortStar(bytecode));
    return Register(static_cast<int>(Bytecode::kStar0) -
                    static_cast<int>(bytecode));
  }

  const base::Optional<Bytecode> TryToShortStar() const {
    if (index() >= 0 && index() < Bytecodes::kShortStarCount) {
      Bytecode bytecode =
          static_cast<Bytecode>(static_cast<int>(Bytecode::kStar0) - index());
      DCHECK_GE(bytecode, Bytecode::kFirstShortStar);
      DCHECK_LE(bytecode, Bytecode::kLastShortStar);
      return bytecode;
    }
    return {};
  }

  static bool AreContiguous(Register reg1, Register reg2,
                            Register reg3 = invalid_value(),
                            Register reg4 = invalid_value(),
                            Register reg5 = invalid_value());

  std::string ToString(int parameter_count) const;

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
  DISALLOW_NEW_AND_DELETE()

  static const int kInvalidIndex = kMaxInt;
  static const int kRegisterFileStartOffset =
      InterpreterFrameConstants::kRegisterFileFromFp / kSystemPointerSize;

  int index_;
};

class RegisterList {
 public:
  RegisterList()
      : first_reg_index_(Register::invalid_value().index()),
        register_count_(0) {}
  explicit RegisterList(Register r) : RegisterList(r.index(), 1) {}

  // Returns a new RegisterList which is a truncated version of this list, with
  // |count| registers.
  const RegisterList Truncate(int new_count) {
    DCHECK_GE(new_count, 0);
    DCHECK_LT(new_count, register_count_);
    return RegisterList(first_reg_index_, new_count);
  }
  const RegisterList PopLeft() {
    DCHECK_GE(register_count_, 0);
    return RegisterList(first_reg_index_ + 1, register_count_ - 1);
  }

  const Register operator[](size_t i) const {
    DCHECK_LT(static_cast<int>(i), register_count_);
    return Register(first_reg_index_ + static_cast<int>(i));
  }

  const Register first_register() const {
    return (register_count() == 0) ? Register(0) : (*this)[0];
  }

  const Register last_register() const {
    return (register_count() == 0) ? Register(0) : (*this)[register_count_ - 1];
  }

  int register_count() const { return register_count_; }

 private:
  friend class BytecodeRegisterAllocator;
  friend class BytecodeDecoder;
  friend class InterpreterTester;
  friend class BytecodeUtils;
  friend class BytecodeArrayIterator;

  RegisterList(int first_reg_index, int register_count)
      : first_reg_index_(first_reg_index), register_count_(register_count) {}

  // Increases the size of the register list by one.
  void IncrementRegisterCount() { register_count_++; }

  int first_reg_index_;
  int register_count_;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_REGISTER_H_

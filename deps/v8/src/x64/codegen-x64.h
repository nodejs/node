// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_X64_CODEGEN_X64_H_
#define V8_X64_CODEGEN_X64_H_

#include "ast.h"
#include "ic-inl.h"

namespace v8 {
namespace internal {


enum TypeofState { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };


class StringCharLoadGenerator : public AllStatic {
 public:
  // Generates the code for handling different string types and loading the
  // indexed character into |result|.  We expect |index| as untagged input and
  // |result| as untagged output.
  static void Generate(MacroAssembler* masm,
                       Register string,
                       Register index,
                       Register result,
                       Label* call_runtime);

 private:
  DISALLOW_COPY_AND_ASSIGN(StringCharLoadGenerator);
};


class MathExpGenerator : public AllStatic {
 public:
  static void EmitMathExp(MacroAssembler* masm,
                          XMMRegister input,
                          XMMRegister result,
                          XMMRegister double_scratch,
                          Register temp1,
                          Register temp2);

 private:
  DISALLOW_COPY_AND_ASSIGN(MathExpGenerator);
};


enum StackArgumentsAccessorReceiverMode {
  ARGUMENTS_CONTAIN_RECEIVER,
  ARGUMENTS_DONT_CONTAIN_RECEIVER
};


class StackArgumentsAccessor BASE_EMBEDDED {
 public:
  StackArgumentsAccessor(
      Register base_reg,
      int argument_count_immediate,
      StackArgumentsAccessorReceiverMode receiver_mode =
          ARGUMENTS_CONTAIN_RECEIVER,
      int extra_displacement_to_last_argument = 0)
      : base_reg_(base_reg),
        argument_count_reg_(no_reg),
        argument_count_immediate_(argument_count_immediate),
        receiver_mode_(receiver_mode),
        extra_displacement_to_last_argument_(
            extra_displacement_to_last_argument) { }

  StackArgumentsAccessor(
      Register base_reg,
      Register argument_count_reg,
      StackArgumentsAccessorReceiverMode receiver_mode =
          ARGUMENTS_CONTAIN_RECEIVER,
      int extra_displacement_to_last_argument = 0)
      : base_reg_(base_reg),
        argument_count_reg_(argument_count_reg),
        argument_count_immediate_(0),
        receiver_mode_(receiver_mode),
        extra_displacement_to_last_argument_(
            extra_displacement_to_last_argument) { }

  StackArgumentsAccessor(
      Register base_reg,
      const ParameterCount& parameter_count,
      StackArgumentsAccessorReceiverMode receiver_mode =
          ARGUMENTS_CONTAIN_RECEIVER,
      int extra_displacement_to_last_argument = 0)
      : base_reg_(base_reg),
        argument_count_reg_(parameter_count.is_reg() ?
                            parameter_count.reg() : no_reg),
        argument_count_immediate_(parameter_count.is_immediate() ?
                                  parameter_count.immediate() : 0),
        receiver_mode_(receiver_mode),
        extra_displacement_to_last_argument_(
            extra_displacement_to_last_argument) { }

  Operand GetArgumentOperand(int index);
  Operand GetReceiverOperand() {
    ASSERT(receiver_mode_ == ARGUMENTS_CONTAIN_RECEIVER);
    return GetArgumentOperand(0);
  }

 private:
  const Register base_reg_;
  const Register argument_count_reg_;
  const int argument_count_immediate_;
  const StackArgumentsAccessorReceiverMode receiver_mode_;
  const int extra_displacement_to_last_argument_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StackArgumentsAccessor);
};


} }  // namespace v8::internal

#endif  // V8_X64_CODEGEN_X64_H_

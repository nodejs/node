// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_X64_CODEGEN_X64_H_
#define V8_X64_CODEGEN_X64_H_

#include "ast.h"
#include "ic-inl.h"

namespace v8 {
namespace internal {

// Forward declarations
class CompilationInfo;

enum TypeofState { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };

// -------------------------------------------------------------------------
// CodeGenerator

class CodeGenerator: public AstVisitor {
 public:
  explicit CodeGenerator(Isolate* isolate) {
    InitializeAstVisitor(isolate);
  }

  static bool MakeCode(CompilationInfo* info);

  // Printing of AST, etc. as requested by flags.
  static void MakeCodePrologue(CompilationInfo* info, const char* kind);

  // Allocate and install the code.
  static Handle<Code> MakeCodeEpilogue(MacroAssembler* masm,
                                       Code::Flags flags,
                                       CompilationInfo* info);

  // Print the code after compiling it.
  static void PrintCode(Handle<Code> code, CompilationInfo* info);

  static bool ShouldGenerateLog(Isolate* isolate, Expression* type);

  static bool RecordPositions(MacroAssembler* masm,
                              int pos,
                              bool right_here = false);

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};


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

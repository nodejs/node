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

#ifndef V8_IA32_CODEGEN_IA32_H_
#define V8_IA32_CODEGEN_IA32_H_

#include "ast.h"
#include "ic-inl.h"

namespace v8 {
namespace internal {

// Forward declarations
class CompilationInfo;

// -------------------------------------------------------------------------
// CodeGenerator

class CodeGenerator {
 public:
  // Printing of AST, etc. as requested by flags.
  static void MakeCodePrologue(CompilationInfo* info, const char* kind);

  // Allocate and install the code.
  static Handle<Code> MakeCodeEpilogue(MacroAssembler* masm,
                                       Code::Flags flags,
                                       CompilationInfo* info);

  // Print the code after compiling it.
  static void PrintCode(Handle<Code> code, CompilationInfo* info);

  static bool ShouldGenerateLog(Expression* type);

  static bool RecordPositions(MacroAssembler* masm,
                              int pos,
                              bool right_here = false);


  static Operand FixedArrayElementOperand(Register array,
                                          Register index_as_smi,
                                          int additional_offset = 0) {
    int offset = FixedArray::kHeaderSize + additional_offset * kPointerSize;
    return FieldOperand(array, index_as_smi, times_half_pointer_size, offset);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};


class StringCharLoadGenerator : public AllStatic {
 public:
  // Generates the code for handling different string types and loading the
  // indexed character into |result|.  We expect |index| as untagged input and
  // |result| as untagged output.
  static void Generate(MacroAssembler* masm,
                       Factory* factory,
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

} }  // namespace v8::internal

#endif  // V8_IA32_CODEGEN_IA32_H_

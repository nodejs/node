// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_CODEGEN_H_
#define V8_CODEGEN_H_

#include "code-stubs.h"
#include "runtime.h"

// Include the declaration of the architecture defined class CodeGenerator.
// The contract  to the shared code is that the the CodeGenerator is a subclass
// of Visitor and that the following methods are available publicly:
//   MakeCode
//   MakeCodePrologue
//   MakeCodeEpilogue
//   masm
//   frame
//   script
//   has_valid_frame
//   SetFrame
//   DeleteFrame
//   allocator
//   AddDeferred
//   in_spilled_code
//   set_in_spilled_code
//   RecordPositions
//
// These methods are either used privately by the shared code or implemented as
// shared code:
//   CodeGenerator
//   ~CodeGenerator
//   Generate
//   ComputeLazyCompile
//   BuildFunctionInfo
//   ProcessDeclarations
//   DeclareGlobals
//   CheckForInlineRuntimeCall
//   AnalyzeCondition
//   CodeForFunctionPosition
//   CodeForReturnPosition
//   CodeForStatementPosition
//   CodeForDoWhileConditionPosition
//   CodeForSourcePosition

enum TypeofState { INSIDE_TYPEOF, NOT_INSIDE_TYPEOF };

#if V8_TARGET_ARCH_IA32
#include "ia32/codegen-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/codegen-x64.h"
#elif V8_TARGET_ARCH_A64
#include "a64/codegen-a64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/codegen-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/codegen-mips.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {


class CompilationInfo;


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

  static bool ShouldGenerateLog(Isolate* isolate, Expression* type);

  static bool RecordPositions(MacroAssembler* masm,
                              int pos,
                              bool right_here = false);

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};


// Results of the library implementation of transcendental functions may differ
// from the one we use in our generated code.  Therefore we use the same
// generated code both in runtime and compiled code.
typedef double (*UnaryMathFunction)(double x);

UnaryMathFunction CreateExpFunction();
UnaryMathFunction CreateSqrtFunction();


class ElementsTransitionGenerator : public AllStatic {
 public:
  // If |mode| is set to DONT_TRACK_ALLOCATION_SITE,
  // |allocation_memento_found| may be NULL.
  static void GenerateMapChangeElementsTransition(MacroAssembler* masm,
      AllocationSiteMode mode,
      Label* allocation_memento_found);
  static void GenerateSmiToDouble(MacroAssembler* masm,
                                  AllocationSiteMode mode,
                                  Label* fail);
  static void GenerateDoubleToObject(MacroAssembler* masm,
                                     AllocationSiteMode mode,
                                     Label* fail);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsTransitionGenerator);
};

static const int kNumberDictionaryProbes = 4;


} }  // namespace v8::internal

#endif  // V8_CODEGEN_H_

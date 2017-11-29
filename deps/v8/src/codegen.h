// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_H_
#define V8_CODEGEN_H_

#include "src/code-stubs.h"
#include "src/globals.h"
#include "src/runtime/runtime.h"

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
//   ProcessDeclarations
//   DeclareGlobals
//   CheckForInlineRuntimeCall
//   AnalyzeCondition
//   CodeForFunctionPosition
//   CodeForReturnPosition
//   CodeForStatementPosition
//   CodeForDoWhileConditionPosition
//   CodeForSourcePosition

#if V8_TARGET_ARCH_IA32
#include "src/ia32/codegen-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/x64/codegen-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/codegen-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/arm/codegen-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC
#include "src/ppc/codegen-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/codegen-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/codegen-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/s390/codegen-s390.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

class CompilationInfo;
class EhFrameWriter;
class ParseInfo;

class CodeGenerator {
 public:
  // Printing of AST, etc. as requested by flags.
  static void MakeCodePrologue(ParseInfo* parse_info, CompilationInfo* info,
                               const char* kind);

  // Allocate and install the code.
  static Handle<Code> MakeCodeEpilogue(TurboAssembler* tasm,
                                       EhFrameWriter* unwinding,
                                       CompilationInfo* info,
                                       Handle<Object> self_reference);

  // Print the code after compiling it.
  static void PrintCode(Handle<Code> code, CompilationInfo* info);

 private:
  DISALLOW_COPY_AND_ASSIGN(CodeGenerator);
};

// Results of the library implementation of transcendental functions may differ
// from the one we use in our generated code.  Therefore we use the same
// generated code both in runtime and compiled code.
typedef double (*UnaryMathFunctionWithIsolate)(double x, Isolate* isolate);

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate);

V8_EXPORT_PRIVATE double modulo(double x, double y);

// Custom implementation of math functions.
double fast_sqrt(double input, Isolate* isolate);
void lazily_initialize_fast_sqrt(Isolate* isolate);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_H_

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_H_
#define V8_CODEGEN_H_

#include "src/code-stubs.h"
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
#elif V8_TARGET_ARCH_X87
#include "src/x87/codegen-x87.h"  // NOLINT
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {


class CompilationInfo;
class EhFrameWriter;

class CodeGenerator {
 public:
  // Printing of AST, etc. as requested by flags.
  static void MakeCodePrologue(CompilationInfo* info, const char* kind);

  // Allocate and install the code.
  static Handle<Code> MakeCodeEpilogue(MacroAssembler* masm,
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


double modulo(double x, double y);

// Custom implementation of math functions.
double fast_sqrt(double input, Isolate* isolate);
void lazily_initialize_fast_sqrt(Isolate* isolate);


class ElementsTransitionGenerator : public AllStatic {
 public:
  // If |mode| is set to DONT_TRACK_ALLOCATION_SITE,
  // |allocation_memento_found| may be NULL.
  static void GenerateMapChangeElementsTransition(
      MacroAssembler* masm,
      Register receiver,
      Register key,
      Register value,
      Register target_map,
      AllocationSiteMode mode,
      Label* allocation_memento_found);
  static void GenerateSmiToDouble(
      MacroAssembler* masm,
      Register receiver,
      Register key,
      Register value,
      Register target_map,
      AllocationSiteMode mode,
      Label* fail);
  static void GenerateDoubleToObject(
      MacroAssembler* masm,
      Register receiver,
      Register key,
      Register value,
      Register target_map,
      AllocationSiteMode mode,
      Label* fail);

 private:
  DISALLOW_COPY_AND_ASSIGN(ElementsTransitionGenerator);
};

static const int kNumberDictionaryProbes = 4;


class CodeAgingHelper {
 public:
  explicit CodeAgingHelper(Isolate* isolate);

  uint32_t young_sequence_length() const { return young_sequence_.length(); }
  bool IsYoung(byte* candidate) const {
    return memcmp(candidate,
                  young_sequence_.start(),
                  young_sequence_.length()) == 0;
  }
  void CopyYoungSequenceTo(byte* new_buffer) const {
    CopyBytes(new_buffer, young_sequence_.start(), young_sequence_.length());
  }

#ifdef DEBUG
  bool IsOld(byte* candidate) const;
#endif

 protected:
  const EmbeddedVector<byte, kNoCodeAgeSequenceLength> young_sequence_;
#ifdef DEBUG
#ifdef V8_TARGET_ARCH_ARM64
  const EmbeddedVector<byte, kNoCodeAgeSequenceLength> old_sequence_;
#endif
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_H_

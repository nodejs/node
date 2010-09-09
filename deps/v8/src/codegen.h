// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
#include "type-info.h"

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
//   ProcessDeferred
//   Generate
//   ComputeLazyCompile
//   BuildFunctionInfo
//   ComputeCallInitialize
//   ComputeCallInitializeInLoop
//   ProcessDeclarations
//   DeclareGlobals
//   FindInlineRuntimeLUT
//   CheckForInlineRuntimeCall
//   AnalyzeCondition
//   CodeForFunctionPosition
//   CodeForReturnPosition
//   CodeForStatementPosition
//   CodeForDoWhileConditionPosition
//   CodeForSourcePosition


#define INLINE_RUNTIME_FUNCTION_LIST(F) \
  F(IsSmi, 1, 1)                                                             \
  F(IsNonNegativeSmi, 1, 1)                                                  \
  F(IsArray, 1, 1)                                                           \
  F(IsRegExp, 1, 1)                                                          \
  F(CallFunction, -1 /* receiver + n args + function */, 1)                  \
  F(IsConstructCall, 0, 1)                                                   \
  F(ArgumentsLength, 0, 1)                                                   \
  F(Arguments, 1, 1)                                                         \
  F(ClassOf, 1, 1)                                                           \
  F(ValueOf, 1, 1)                                                           \
  F(SetValueOf, 2, 1)                                                        \
  F(StringCharCodeAt, 2, 1)                                                  \
  F(StringCharFromCode, 1, 1)                                                \
  F(StringCharAt, 2, 1)                                                      \
  F(ObjectEquals, 2, 1)                                                      \
  F(Log, 3, 1)                                                               \
  F(RandomHeapNumber, 0, 1)                                                  \
  F(IsObject, 1, 1)                                                          \
  F(IsFunction, 1, 1)                                                        \
  F(IsUndetectableObject, 1, 1)                                              \
  F(IsSpecObject, 1, 1)                                                      \
  F(IsStringWrapperSafeForDefaultValueOf, 1, 1)                              \
  F(StringAdd, 2, 1)                                                         \
  F(SubString, 3, 1)                                                         \
  F(StringCompare, 2, 1)                                                     \
  F(RegExpExec, 4, 1)                                                        \
  F(RegExpConstructResult, 3, 1)                                             \
  F(RegExpCloneResult, 1, 1)                                                 \
  F(GetFromCache, 2, 1)                                                      \
  F(NumberToString, 1, 1)                                                    \
  F(SwapElements, 3, 1)                                                      \
  F(MathPow, 2, 1)                                                           \
  F(MathSin, 1, 1)                                                           \
  F(MathCos, 1, 1)                                                           \
  F(MathSqrt, 1, 1)                                                          \
  F(IsRegExpEquivalent, 2, 1)                                                \
  F(HasCachedArrayIndex, 1, 1)                                               \
  F(GetCachedArrayIndex, 1, 1)


#if V8_TARGET_ARCH_IA32
#include "ia32/codegen-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/codegen-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/codegen-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/codegen-mips.h"
#else
#error Unsupported target architecture.
#endif

#include "register-allocator.h"

namespace v8 {
namespace internal {

// Code generation can be nested.  Code generation scopes form a stack
// of active code generators.
class CodeGeneratorScope BASE_EMBEDDED {
 public:
  explicit CodeGeneratorScope(CodeGenerator* cgen) {
    previous_ = top_;
    top_ = cgen;
  }

  ~CodeGeneratorScope() {
    top_ = previous_;
  }

  static CodeGenerator* Current() {
    ASSERT(top_ != NULL);
    return top_;
  }

 private:
  static CodeGenerator* top_;
  CodeGenerator* previous_;
};


#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64

// State of used registers in a virtual frame.
class FrameRegisterState {
 public:
  // Captures the current state of the given frame.
  explicit FrameRegisterState(VirtualFrame* frame);

  // Saves the state in the stack.
  void Save(MacroAssembler* masm) const;

  // Restores the state from the stack.
  void Restore(MacroAssembler* masm) const;

 private:
  // Constants indicating special actions.  They should not be multiples
  // of kPointerSize so they will not collide with valid offsets from
  // the frame pointer.
  static const int kIgnore = -1;
  static const int kPush = 1;

  // This flag is ored with a valid offset from the frame pointer, so
  // it should fit in the low zero bits of a valid offset.
  static const int kSyncedFlag = 2;

  int registers_[RegisterAllocator::kNumRegisters];
};

#elif V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_MIPS


class FrameRegisterState {
 public:
  inline FrameRegisterState(VirtualFrame frame) : frame_(frame) { }

  inline const VirtualFrame* frame() const { return &frame_; }

 private:
  VirtualFrame frame_;
};

#else

#error Unsupported target architecture.

#endif


// RuntimeCallHelper implementation that saves/restores state of a
// virtual frame.
class VirtualFrameRuntimeCallHelper : public RuntimeCallHelper {
 public:
  // Does not take ownership of |frame_state|.
  explicit VirtualFrameRuntimeCallHelper(const FrameRegisterState* frame_state)
      : frame_state_(frame_state) {}

  virtual void BeforeCall(MacroAssembler* masm) const;

  virtual void AfterCall(MacroAssembler* masm) const;

 private:
  const FrameRegisterState* frame_state_;
};


// Deferred code objects are small pieces of code that are compiled
// out of line. They are used to defer the compilation of uncommon
// paths thereby avoiding expensive jumps around uncommon code parts.
class DeferredCode: public ZoneObject {
 public:
  DeferredCode();
  virtual ~DeferredCode() { }

  virtual void Generate() = 0;

  MacroAssembler* masm() { return masm_; }

  int statement_position() const { return statement_position_; }
  int position() const { return position_; }

  Label* entry_label() { return &entry_label_; }
  Label* exit_label() { return &exit_label_; }

#ifdef DEBUG
  void set_comment(const char* comment) { comment_ = comment; }
  const char* comment() const { return comment_; }
#else
  void set_comment(const char* comment) { }
  const char* comment() const { return ""; }
#endif

  inline void Jump();
  inline void Branch(Condition cc);
  void BindExit() { masm_->bind(&exit_label_); }

  const FrameRegisterState* frame_state() const { return &frame_state_; }

  void SaveRegisters();
  void RestoreRegisters();
  void Exit();

  // If this returns true then all registers will be saved for the duration
  // of the Generate() call.  Otherwise the registers are not saved and the
  // Generate() call must bracket runtime any runtime calls with calls to
  // SaveRegisters() and RestoreRegisters().  In this case the Generate
  // method must also call Exit() in order to return to the non-deferred
  // code.
  virtual bool AutoSaveAndRestore() { return true; }

 protected:
  MacroAssembler* masm_;

 private:
  int statement_position_;
  int position_;

  Label entry_label_;
  Label exit_label_;

  FrameRegisterState frame_state_;

#ifdef DEBUG
  const char* comment_;
#endif
  DISALLOW_COPY_AND_ASSIGN(DeferredCode);
};


} }  // namespace v8::internal

#endif  // V8_CODEGEN_H_

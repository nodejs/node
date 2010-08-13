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
//   PatchInlineRuntimeEntry
//   AnalyzeCondition
//   CodeForFunctionPosition
//   CodeForReturnPosition
//   CodeForStatementPosition
//   CodeForDoWhileConditionPosition
//   CodeForSourcePosition


// Mode to overwrite BinaryExpression values.
enum OverwriteMode { NO_OVERWRITE, OVERWRITE_LEFT, OVERWRITE_RIGHT };
enum UnaryOverwriteMode { UNARY_OVERWRITE, UNARY_NO_OVERWRITE };

// Types of uncatchable exceptions.
enum UncatchableExceptionType { OUT_OF_MEMORY, TERMINATION };

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
  F(IsSpecObject, 1, 1)                                            \
  F(StringAdd, 2, 1)                                                         \
  F(SubString, 3, 1)                                                         \
  F(StringCompare, 2, 1)                                                     \
  F(RegExpExec, 4, 1)                                                        \
  F(RegExpConstructResult, 3, 1)                                             \
  F(GetFromCache, 2, 1)                                                      \
  F(NumberToString, 1, 1)                                                    \
  F(SwapElements, 3, 1)                                                      \
  F(MathPow, 2, 1)                                                           \
  F(MathSin, 1, 1)                                                           \
  F(MathCos, 1, 1)                                                           \
  F(MathSqrt, 1, 1)                                                          \
  F(IsRegExpEquivalent, 2, 1)


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

// Support for "structured" code comments.
#ifdef DEBUG

class Comment BASE_EMBEDDED {
 public:
  Comment(MacroAssembler* masm, const char* msg);
  ~Comment();

 private:
  MacroAssembler* masm_;
  const char* msg_;
};

#else

class Comment BASE_EMBEDDED {
 public:
  Comment(MacroAssembler*, const char*)  {}
};

#endif  // DEBUG


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


// Helper interface to prepare to/restore after making runtime calls.
class RuntimeCallHelper {
 public:
  virtual ~RuntimeCallHelper() {}

  virtual void BeforeCall(MacroAssembler* masm) const = 0;

  virtual void AfterCall(MacroAssembler* masm) const = 0;

 protected:
  RuntimeCallHelper() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RuntimeCallHelper);
};


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


// RuntimeCallHelper implementation used in IC stubs: enters/leaves a
// newly created internal frame before/after the runtime call.
class ICRuntimeCallHelper : public RuntimeCallHelper {
 public:
  ICRuntimeCallHelper() {}

  virtual void BeforeCall(MacroAssembler* masm) const;

  virtual void AfterCall(MacroAssembler* masm) const;
};


// Trivial RuntimeCallHelper implementation.
class NopRuntimeCallHelper : public RuntimeCallHelper {
 public:
  NopRuntimeCallHelper() {}

  virtual void BeforeCall(MacroAssembler* masm) const {}

  virtual void AfterCall(MacroAssembler* masm) const {}
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

class StackCheckStub : public CodeStub {
 public:
  StackCheckStub() { }

  void Generate(MacroAssembler* masm);

 private:

  const char* GetName() { return "StackCheckStub"; }

  Major MajorKey() { return StackCheck; }
  int MinorKey() { return 0; }
};


class FastNewClosureStub : public CodeStub {
 public:
  void Generate(MacroAssembler* masm);

 private:
  const char* GetName() { return "FastNewClosureStub"; }
  Major MajorKey() { return FastNewClosure; }
  int MinorKey() { return 0; }
};


class FastNewContextStub : public CodeStub {
 public:
  static const int kMaximumSlots = 64;

  explicit FastNewContextStub(int slots) : slots_(slots) {
    ASSERT(slots_ > 0 && slots <= kMaximumSlots);
  }

  void Generate(MacroAssembler* masm);

 private:
  int slots_;

  const char* GetName() { return "FastNewContextStub"; }
  Major MajorKey() { return FastNewContext; }
  int MinorKey() { return slots_; }
};


class FastCloneShallowArrayStub : public CodeStub {
 public:
  static const int kMaximumLength = 8;

  explicit FastCloneShallowArrayStub(int length) : length_(length) {
    ASSERT(length >= 0 && length <= kMaximumLength);
  }

  void Generate(MacroAssembler* masm);

 private:
  int length_;

  const char* GetName() { return "FastCloneShallowArrayStub"; }
  Major MajorKey() { return FastCloneShallowArray; }
  int MinorKey() { return length_; }
};


class InstanceofStub: public CodeStub {
 public:
  InstanceofStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return Instanceof; }
  int MinorKey() { return 0; }
};


enum NegativeZeroHandling {
  kStrictNegativeZero,
  kIgnoreNegativeZero
};


class GenericUnaryOpStub : public CodeStub {
 public:
  GenericUnaryOpStub(Token::Value op,
                     UnaryOverwriteMode overwrite,
                     NegativeZeroHandling negative_zero = kStrictNegativeZero)
      : op_(op), overwrite_(overwrite), negative_zero_(negative_zero) { }

 private:
  Token::Value op_;
  UnaryOverwriteMode overwrite_;
  NegativeZeroHandling negative_zero_;

  class OverwriteField: public BitField<UnaryOverwriteMode, 0, 1> {};
  class NegativeZeroField: public BitField<NegativeZeroHandling, 1, 1> {};
  class OpField: public BitField<Token::Value, 2, kMinorBits - 2> {};

  Major MajorKey() { return GenericUnaryOp; }
  int MinorKey() {
    return OpField::encode(op_) |
           OverwriteField::encode(overwrite_) |
           NegativeZeroField::encode(negative_zero_);
  }

  void Generate(MacroAssembler* masm);

  const char* GetName();
};


enum NaNInformation {
  kBothCouldBeNaN,
  kCantBothBeNaN
};


class CompareStub: public CodeStub {
 public:
  CompareStub(Condition cc,
              bool strict,
              NaNInformation nan_info = kBothCouldBeNaN,
              bool include_number_compare = true,
              Register lhs = no_reg,
              Register rhs = no_reg) :
      cc_(cc),
      strict_(strict),
      never_nan_nan_(nan_info == kCantBothBeNaN),
      include_number_compare_(include_number_compare),
      lhs_(lhs),
      rhs_(rhs),
      name_(NULL) { }

  void Generate(MacroAssembler* masm);

 private:
  Condition cc_;
  bool strict_;
  // Only used for 'equal' comparisons.  Tells the stub that we already know
  // that at least one side of the comparison is not NaN.  This allows the
  // stub to use object identity in the positive case.  We ignore it when
  // generating the minor key for other comparisons to avoid creating more
  // stubs.
  bool never_nan_nan_;
  // Do generate the number comparison code in the stub. Stubs without number
  // comparison code is used when the number comparison has been inlined, and
  // the stub will be called if one of the operands is not a number.
  bool include_number_compare_;
  // Register holding the left hand side of the comparison if the stub gives
  // a choice, no_reg otherwise.
  Register lhs_;
  // Register holding the right hand side of the comparison if the stub gives
  // a choice, no_reg otherwise.
  Register rhs_;

  // Encoding of the minor key CCCCCCCCCCCCRCNS.
  class StrictField: public BitField<bool, 0, 1> {};
  class NeverNanNanField: public BitField<bool, 1, 1> {};
  class IncludeNumberCompareField: public BitField<bool, 2, 1> {};
  class RegisterField: public BitField<bool, 3, 1> {};
  class ConditionField: public BitField<int, 4, 12> {};

  Major MajorKey() { return Compare; }

  int MinorKey();

  // Branch to the label if the given object isn't a symbol.
  void BranchIfNonSymbol(MacroAssembler* masm,
                         Label* label,
                         Register object,
                         Register scratch);

  // Unfortunately you have to run without snapshots to see most of these
  // names in the profile since most compare stubs end up in the snapshot.
  char* name_;
  const char* GetName();
#ifdef DEBUG
  void Print() {
    PrintF("CompareStub (cc %d), (strict %s), "
           "(never_nan_nan %s), (number_compare %s) ",
           static_cast<int>(cc_),
           strict_ ? "true" : "false",
           never_nan_nan_ ? "true" : "false",
           include_number_compare_ ? "included" : "not included");

    if (!lhs_.is(no_reg) && !rhs_.is(no_reg)) {
      PrintF("(lhs r%d), (rhs r%d)\n", lhs_.code(), rhs_.code());
    } else {
      PrintF("\n");
    }
  }
#endif
};


class CEntryStub : public CodeStub {
 public:
  explicit CEntryStub(int result_size,
                      ExitFrame::Mode mode = ExitFrame::MODE_NORMAL)
      : result_size_(result_size), mode_(mode) { }

  void Generate(MacroAssembler* masm);

 private:
  void GenerateCore(MacroAssembler* masm,
                    Label* throw_normal_exception,
                    Label* throw_termination_exception,
                    Label* throw_out_of_memory_exception,
                    bool do_gc,
                    bool always_allocate_scope,
                    int alignment_skew = 0);
  void GenerateThrowTOS(MacroAssembler* masm);
  void GenerateThrowUncatchable(MacroAssembler* masm,
                                UncatchableExceptionType type);

  // Number of pointers/values returned.
  const int result_size_;
  const ExitFrame::Mode mode_;

  // Minor key encoding
  class ExitFrameModeBits: public BitField<ExitFrame::Mode, 0, 1> {};
  class IndirectResultBits: public BitField<bool, 1, 1> {};

  Major MajorKey() { return CEntry; }
  // Minor key must differ if different result_size_ values means different
  // code is generated.
  int MinorKey();

  const char* GetName() { return "CEntryStub"; }
};


class ApiGetterEntryStub : public CodeStub {
 public:
  ApiGetterEntryStub(Handle<AccessorInfo> info,
                     ApiFunction* fun)
      : info_(info),
        fun_(fun) { }
  void Generate(MacroAssembler* masm);
  virtual bool has_custom_cache() { return true; }
  virtual bool GetCustomCache(Code** code_out);
  virtual void SetCustomCache(Code* value);

  static const int kStackSpace = 5;
  static const int kArgc = 4;
 private:
  Handle<AccessorInfo> info() { return info_; }
  ApiFunction* fun() { return fun_; }
  Major MajorKey() { return NoCache; }
  int MinorKey() { return 0; }
  const char* GetName() { return "ApiEntryStub"; }
  // The accessor info associated with the function.
  Handle<AccessorInfo> info_;
  // The function to be called.
  ApiFunction* fun_;
};


class JSEntryStub : public CodeStub {
 public:
  JSEntryStub() { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, false); }

 protected:
  void GenerateBody(MacroAssembler* masm, bool is_construct);

 private:
  Major MajorKey() { return JSEntry; }
  int MinorKey() { return 0; }

  const char* GetName() { return "JSEntryStub"; }
};


class JSConstructEntryStub : public JSEntryStub {
 public:
  JSConstructEntryStub() { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, true); }

 private:
  int MinorKey() { return 1; }

  const char* GetName() { return "JSConstructEntryStub"; }
};


class ArgumentsAccessStub: public CodeStub {
 public:
  enum Type {
    READ_ELEMENT,
    NEW_OBJECT
  };

  explicit ArgumentsAccessStub(Type type) : type_(type) { }

 private:
  Type type_;

  Major MajorKey() { return ArgumentsAccess; }
  int MinorKey() { return type_; }

  void Generate(MacroAssembler* masm);
  void GenerateReadElement(MacroAssembler* masm);
  void GenerateNewObject(MacroAssembler* masm);

  const char* GetName() { return "ArgumentsAccessStub"; }

#ifdef DEBUG
  void Print() {
    PrintF("ArgumentsAccessStub (type %d)\n", type_);
  }
#endif
};


class RegExpExecStub: public CodeStub {
 public:
  RegExpExecStub() { }

 private:
  Major MajorKey() { return RegExpExec; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "RegExpExecStub"; }

#ifdef DEBUG
  void Print() {
    PrintF("RegExpExecStub\n");
  }
#endif
};


class CallFunctionStub: public CodeStub {
 public:
  CallFunctionStub(int argc, InLoopFlag in_loop, CallFunctionFlags flags)
      : argc_(argc), in_loop_(in_loop), flags_(flags) { }

  void Generate(MacroAssembler* masm);

 private:
  int argc_;
  InLoopFlag in_loop_;
  CallFunctionFlags flags_;

#ifdef DEBUG
  void Print() {
    PrintF("CallFunctionStub (args %d, in_loop %d, flags %d)\n",
           argc_,
           static_cast<int>(in_loop_),
           static_cast<int>(flags_));
  }
#endif

  // Minor key encoding in 32 bits with Bitfield <Type, shift, size>.
  class InLoopBits: public BitField<InLoopFlag, 0, 1> {};
  class FlagBits: public BitField<CallFunctionFlags, 1, 1> {};
  class ArgcBits: public BitField<int, 2, 32 - 2> {};

  Major MajorKey() { return CallFunction; }
  int MinorKey() {
    // Encode the parameters in a unique 32 bit value.
    return InLoopBits::encode(in_loop_)
           | FlagBits::encode(flags_)
           | ArgcBits::encode(argc_);
  }

  InLoopFlag InLoop() { return in_loop_; }
  bool ReceiverMightBeValue() {
    return (flags_ & RECEIVER_MIGHT_BE_VALUE) != 0;
  }

 public:
  static int ExtractArgcFromMinorKey(int minor_key) {
    return ArgcBits::decode(minor_key);
  }
};


class ToBooleanStub: public CodeStub {
 public:
  ToBooleanStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return ToBoolean; }
  int MinorKey() { return 0; }
};


enum StringIndexFlags {
  // Accepts smis or heap numbers.
  STRING_INDEX_IS_NUMBER,

  // Accepts smis or heap numbers that are valid array indices
  // (ECMA-262 15.4). Invalid indices are reported as being out of
  // range.
  STRING_INDEX_IS_ARRAY_INDEX
};


// Generates code implementing String.prototype.charCodeAt.
//
// Only supports the case when the receiver is a string and the index
// is a number (smi or heap number) that is a valid index into the
// string. Additional index constraints are specified by the
// flags. Otherwise, bails out to the provided labels.
//
// Register usage: |object| may be changed to another string in a way
// that doesn't affect charCodeAt/charAt semantics, |index| is
// preserved, |scratch| and |result| are clobbered.
class StringCharCodeAtGenerator {
 public:
  StringCharCodeAtGenerator(Register object,
                            Register index,
                            Register scratch,
                            Register result,
                            Label* receiver_not_string,
                            Label* index_not_number,
                            Label* index_out_of_range,
                            StringIndexFlags index_flags)
      : object_(object),
        index_(index),
        scratch_(scratch),
        result_(result),
        receiver_not_string_(receiver_not_string),
        index_not_number_(index_not_number),
        index_out_of_range_(index_out_of_range),
        index_flags_(index_flags) {
    ASSERT(!scratch_.is(object_));
    ASSERT(!scratch_.is(index_));
    ASSERT(!scratch_.is(result_));
    ASSERT(!result_.is(object_));
    ASSERT(!result_.is(index_));
  }

  // Generates the fast case code. On the fallthrough path |result|
  // register contains the result.
  void GenerateFast(MacroAssembler* masm);

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm,
                    const RuntimeCallHelper& call_helper);

 private:
  Register object_;
  Register index_;
  Register scratch_;
  Register result_;

  Label* receiver_not_string_;
  Label* index_not_number_;
  Label* index_out_of_range_;

  StringIndexFlags index_flags_;

  Label call_runtime_;
  Label index_not_smi_;
  Label got_smi_index_;
  Label exit_;

  DISALLOW_COPY_AND_ASSIGN(StringCharCodeAtGenerator);
};


// Generates code for creating a one-char string from a char code.
class StringCharFromCodeGenerator {
 public:
  StringCharFromCodeGenerator(Register code,
                              Register result)
      : code_(code),
        result_(result) {
    ASSERT(!code_.is(result_));
  }

  // Generates the fast case code. On the fallthrough path |result|
  // register contains the result.
  void GenerateFast(MacroAssembler* masm);

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm,
                    const RuntimeCallHelper& call_helper);

 private:
  Register code_;
  Register result_;

  Label slow_case_;
  Label exit_;

  DISALLOW_COPY_AND_ASSIGN(StringCharFromCodeGenerator);
};


// Generates code implementing String.prototype.charAt.
//
// Only supports the case when the receiver is a string and the index
// is a number (smi or heap number) that is a valid index into the
// string. Additional index constraints are specified by the
// flags. Otherwise, bails out to the provided labels.
//
// Register usage: |object| may be changed to another string in a way
// that doesn't affect charCodeAt/charAt semantics, |index| is
// preserved, |scratch1|, |scratch2|, and |result| are clobbered.
class StringCharAtGenerator {
 public:
  StringCharAtGenerator(Register object,
                        Register index,
                        Register scratch1,
                        Register scratch2,
                        Register result,
                        Label* receiver_not_string,
                        Label* index_not_number,
                        Label* index_out_of_range,
                        StringIndexFlags index_flags)
      : char_code_at_generator_(object,
                                index,
                                scratch1,
                                scratch2,
                                receiver_not_string,
                                index_not_number,
                                index_out_of_range,
                                index_flags),
        char_from_code_generator_(scratch2, result) {}

  // Generates the fast case code. On the fallthrough path |result|
  // register contains the result.
  void GenerateFast(MacroAssembler* masm);

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm,
                    const RuntimeCallHelper& call_helper);

 private:
  StringCharCodeAtGenerator char_code_at_generator_;
  StringCharFromCodeGenerator char_from_code_generator_;

  DISALLOW_COPY_AND_ASSIGN(StringCharAtGenerator);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_H_

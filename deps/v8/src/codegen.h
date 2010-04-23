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

#include "ast.h"
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

// Types of uncatchable exceptions.
enum UncatchableExceptionType { OUT_OF_MEMORY, TERMINATION };


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
  F(FastCharCodeAt, 2, 1)                                                    \
  F(CharFromCode, 1, 1)                                                      \
  F(ObjectEquals, 2, 1)                                                      \
  F(Log, 3, 1)                                                               \
  F(RandomHeapNumber, 0, 1)                                          \
  F(IsObject, 1, 1)                                                          \
  F(IsFunction, 1, 1)                                                        \
  F(IsUndetectableObject, 1, 1)                                              \
  F(StringAdd, 2, 1)                                                         \
  F(SubString, 3, 1)                                                         \
  F(StringCompare, 2, 1)                                                     \
  F(RegExpExec, 4, 1)                                                        \
  F(RegExpConstructResult, 3, 1)                                             \
  F(GetFromCache, 2, 1)                                                      \
  F(NumberToString, 1, 1)                                                    \
  F(MathPow, 2, 1)                                                           \
  F(MathSin, 1, 1)                                                           \
  F(MathCos, 1, 1)                                                           \
  F(MathSqrt, 1, 1)


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

  void SaveRegisters();
  void RestoreRegisters();

 protected:
  MacroAssembler* masm_;

 private:
  // Constants indicating special actions.  They should not be multiples
  // of kPointerSize so they will not collide with valid offsets from
  // the frame pointer.
  static const int kIgnore = -1;
  static const int kPush = 1;

  // This flag is ored with a valid offset from the frame pointer, so
  // it should fit in the low zero bits of a valid offset.
  static const int kSyncedFlag = 2;

  int statement_position_;
  int position_;

  Label entry_label_;
  Label exit_label_;

  // C++ doesn't allow zero length arrays, so we make the array length 1 even
  // if we don't need it.
  static const int kRegistersArrayLength =
      (RegisterAllocator::kNumRegisters == 0) ?
          1 : RegisterAllocator::kNumRegisters;
  int registers_[kRegistersArrayLength];

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


class GenericUnaryOpStub : public CodeStub {
 public:
  GenericUnaryOpStub(Token::Value op, bool overwrite)
      : op_(op), overwrite_(overwrite) { }

 private:
  Token::Value op_;
  bool overwrite_;

  class OverwriteField: public BitField<int, 0, 1> {};
  class OpField: public BitField<Token::Value, 1, kMinorBits - 1> {};

  Major MajorKey() { return GenericUnaryOp; }
  int MinorKey() {
    return OpField::encode(op_) | OverwriteField::encode(overwrite_);
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
              bool include_number_compare = true) :
      cc_(cc),
      strict_(strict),
      never_nan_nan_(nan_info == kCantBothBeNaN),
      include_number_compare_(include_number_compare),
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

  // Encoding of the minor key CCCCCCCCCCCCCCNS.
  class StrictField: public BitField<bool, 0, 1> {};
  class NeverNanNanField: public BitField<bool, 1, 1> {};
  class IncludeNumberCompareField: public BitField<bool, 2, 1> {};
  class ConditionField: public BitField<int, 3, 13> {};

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
           "(never_nan_nan %s), (number_compare %s)\n",
           static_cast<int>(cc_),
           strict_ ? "true" : "false",
           never_nan_nan_ ? "true" : "false",
           include_number_compare_ ? "included" : "not included");
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


}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_H_

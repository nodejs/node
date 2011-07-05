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

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "globals.h"

namespace v8 {
namespace internal {

// List of code stubs used on all platforms. The order in this list is important
// as only the stubs up to and including Instanceof allows nested stub calls.
#define CODE_STUB_LIST_ALL_PLATFORMS(V)  \
  V(CallFunction)                        \
  V(GenericBinaryOp)                     \
  V(TypeRecordingBinaryOp)               \
  V(StringAdd)                           \
  V(StringCharAt)                        \
  V(SubString)                           \
  V(StringCompare)                       \
  V(SmiOp)                               \
  V(Compare)                             \
  V(CompareIC)                           \
  V(MathPow)                             \
  V(TranscendentalCache)                 \
  V(Instanceof)                          \
  V(ConvertToDouble)                     \
  V(WriteInt32ToHeapNumber)              \
  V(IntegerMod)                          \
  V(StackCheck)                          \
  V(FastNewClosure)                      \
  V(FastNewContext)                      \
  V(FastCloneShallowArray)               \
  V(GenericUnaryOp)                      \
  V(RevertToNumber)                      \
  V(ToBoolean)                           \
  V(ToNumber)                            \
  V(CounterOp)                           \
  V(ArgumentsAccess)                     \
  V(RegExpExec)                          \
  V(RegExpConstructResult)               \
  V(NumberToString)                      \
  V(CEntry)                              \
  V(JSEntry)                             \
  V(DebuggerStatement)

// List of code stubs only used on ARM platforms.
#ifdef V8_TARGET_ARCH_ARM
#define CODE_STUB_LIST_ARM(V)  \
  V(GetProperty)               \
  V(SetProperty)               \
  V(InvokeBuiltin)             \
  V(RegExpCEntry)              \
  V(DirectCEntry)
#else
#define CODE_STUB_LIST_ARM(V)
#endif

// Combined list of code stubs.
#define CODE_STUB_LIST(V)            \
  CODE_STUB_LIST_ALL_PLATFORMS(V)    \
  CODE_STUB_LIST_ARM(V)

// Mode to overwrite BinaryExpression values.
enum OverwriteMode { NO_OVERWRITE, OVERWRITE_LEFT, OVERWRITE_RIGHT };
enum UnaryOverwriteMode { UNARY_OVERWRITE, UNARY_NO_OVERWRITE };


// Stub is base classes of all stubs.
class CodeStub BASE_EMBEDDED {
 public:
  enum Major {
#define DEF_ENUM(name) name,
    CODE_STUB_LIST(DEF_ENUM)
#undef DEF_ENUM
    NoCache,  // marker for stubs that do custom caching
    NUMBER_OF_IDS
  };

  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GetCode();

  // Retrieve the code for the stub if already generated.  Do not
  // generate the code if not already generated and instead return a
  // retry after GC Failure object.
  MUST_USE_RESULT MaybeObject* TryGetCode();

  static Major MajorKeyFromKey(uint32_t key) {
    return static_cast<Major>(MajorKeyBits::decode(key));
  }
  static int MinorKeyFromKey(uint32_t key) {
    return MinorKeyBits::decode(key);
  }

  // Gets the major key from a code object that is a code stub or binary op IC.
  static Major GetMajorKey(Code* code_stub) {
    return static_cast<Major>(code_stub->major_key());
  }

  static const char* MajorName(Major major_key, bool allow_unknown_keys);

  virtual ~CodeStub() {}

 protected:
  static const int kMajorBits = 6;
  static const int kMinorBits = kBitsPerInt - kSmiTagSize - kMajorBits;

 private:
  // Lookup the code in the (possibly custom) cache.
  bool FindCodeInCache(Code** code_out);

  // Nonvirtual wrapper around the stub-specific Generate function.  Call
  // this function to set up the macro assembler and generate the code.
  void GenerateCode(MacroAssembler* masm);

  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;

  // Perform bookkeeping required after code generation when stub code is
  // initially generated.
  void RecordCodeGeneration(Code* code, MacroAssembler* masm);

  // Finish the code object after it has been generated.
  virtual void FinishCode(Code* code) { }

  // Returns information for computing the number key.
  virtual Major MajorKey() = 0;
  virtual int MinorKey() = 0;

  // The CallFunctionStub needs to override this so it can encode whether a
  // lazily generated function should be fully optimized or not.
  virtual InLoopFlag InLoop() { return NOT_IN_LOOP; }

  // GenericBinaryOpStub needs to override this.
  virtual int GetCodeKind();

  // GenericBinaryOpStub needs to override this.
  virtual InlineCacheState GetICState() {
    return UNINITIALIZED;
  }

  // Returns a name for logging/debugging purposes.
  virtual const char* GetName() { return MajorName(MajorKey(), false); }

#ifdef DEBUG
  virtual void Print() { PrintF("%s\n", GetName()); }
#endif

  // Computes the key based on major and minor.
  uint32_t GetKey() {
    ASSERT(static_cast<int>(MajorKey()) < NUMBER_OF_IDS);
    return MinorKeyBits::encode(MinorKey()) |
           MajorKeyBits::encode(MajorKey());
  }

  bool AllowsStubCalls() { return MajorKey() <= Instanceof; }

  class MajorKeyBits: public BitField<uint32_t, 0, kMajorBits> {};
  class MinorKeyBits: public BitField<uint32_t, kMajorBits, kMinorBits> {};

  friend class BreakPointIterator;
};


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

} }  // namespace v8::internal

#if V8_TARGET_ARCH_IA32
#include "ia32/code-stubs-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/code-stubs-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/code-stubs-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "mips/code-stubs-mips.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {


// RuntimeCallHelper implementation used in stubs: enters/leaves a
// newly created internal frame before/after the runtime call.
class StubRuntimeCallHelper : public RuntimeCallHelper {
 public:
  StubRuntimeCallHelper() {}

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


class StackCheckStub : public CodeStub {
 public:
  StackCheckStub() { }

  void Generate(MacroAssembler* masm);

 private:

  const char* GetName() { return "StackCheckStub"; }

  Major MajorKey() { return StackCheck; }
  int MinorKey() { return 0; }
};


class ToNumberStub: public CodeStub {
 public:
  ToNumberStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return ToNumber; }
  int MinorKey() { return 0; }
  const char* GetName() { return "ToNumberStub"; }
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
  // Maximum length of copied elements array.
  static const int kMaximumClonedLength = 8;

  enum Mode {
    CLONE_ELEMENTS,
    COPY_ON_WRITE_ELEMENTS
  };

  FastCloneShallowArrayStub(Mode mode, int length)
      : mode_(mode),
        length_((mode == COPY_ON_WRITE_ELEMENTS) ? 0 : length) {
    ASSERT(length_ >= 0);
    ASSERT(length_ <= kMaximumClonedLength);
  }

  void Generate(MacroAssembler* masm);

 private:
  Mode mode_;
  int length_;

  const char* GetName() { return "FastCloneShallowArrayStub"; }
  Major MajorKey() { return FastCloneShallowArray; }
  int MinorKey() {
    ASSERT(mode_ == 0 || mode_ == 1);
    return (length_ << 1) | mode_;
  }
};


class InstanceofStub: public CodeStub {
 public:
  enum Flags {
    kNoFlags = 0,
    kArgsInRegisters = 1 << 0,
    kCallSiteInlineCheck = 1 << 1,
    kReturnTrueFalseObject = 1 << 2
  };

  explicit InstanceofStub(Flags flags) : flags_(flags), name_(NULL) { }

  static Register left();
  static Register right();

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return Instanceof; }
  int MinorKey() { return static_cast<int>(flags_); }

  bool HasArgsInRegisters() const {
    return (flags_ & kArgsInRegisters) != 0;
  }

  bool HasCallSiteInlineCheck() const {
    return (flags_ & kCallSiteInlineCheck) != 0;
  }

  bool ReturnTrueFalseObject() const {
    return (flags_ & kReturnTrueFalseObject) != 0;
  }

  const char* GetName();

  Flags flags_;
  char* name_;
};


enum NegativeZeroHandling {
  kStrictNegativeZero,
  kIgnoreNegativeZero
};


enum UnaryOpFlags {
  NO_UNARY_FLAGS = 0,
  NO_UNARY_SMI_CODE_IN_STUB = 1 << 0
};


class GenericUnaryOpStub : public CodeStub {
 public:
  GenericUnaryOpStub(Token::Value op,
                     UnaryOverwriteMode overwrite,
                     UnaryOpFlags flags,
                     NegativeZeroHandling negative_zero = kStrictNegativeZero)
      : op_(op),
        overwrite_(overwrite),
        include_smi_code_((flags & NO_UNARY_SMI_CODE_IN_STUB) == 0),
        negative_zero_(negative_zero) { }

 private:
  Token::Value op_;
  UnaryOverwriteMode overwrite_;
  bool include_smi_code_;
  NegativeZeroHandling negative_zero_;

  class OverwriteField: public BitField<UnaryOverwriteMode, 0, 1> {};
  class IncludeSmiCodeField: public BitField<bool, 1, 1> {};
  class NegativeZeroField: public BitField<NegativeZeroHandling, 2, 1> {};
  class OpField: public BitField<Token::Value, 3, kMinorBits - 3> {};

  Major MajorKey() { return GenericUnaryOp; }
  int MinorKey() {
    return OpField::encode(op_) |
        OverwriteField::encode(overwrite_) |
        IncludeSmiCodeField::encode(include_smi_code_) |
        NegativeZeroField::encode(negative_zero_);
  }

  void Generate(MacroAssembler* masm);

  const char* GetName();
};


class MathPowStub: public CodeStub {
 public:
  MathPowStub() {}
  virtual void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return MathPow; }
  virtual int MinorKey() { return 0; }

  const char* GetName() { return "MathPowStub"; }
};


class StringCharAtStub: public CodeStub {
 public:
  StringCharAtStub() {}

 private:
  Major MajorKey() { return StringCharAt; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class ICCompareStub: public CodeStub {
 public:
  ICCompareStub(Token::Value op, CompareIC::State state)
      : op_(op), state_(state) {
    ASSERT(Token::IsCompareOp(op));
  }

  virtual void Generate(MacroAssembler* masm);

 private:
  class OpField: public BitField<int, 0, 3> { };
  class StateField: public BitField<int, 3, 5> { };

  virtual void FinishCode(Code* code) { code->set_compare_state(state_); }

  virtual CodeStub::Major MajorKey() { return CompareIC; }
  virtual int MinorKey();

  virtual int GetCodeKind() { return Code::COMPARE_IC; }

  void GenerateSmis(MacroAssembler* masm);
  void GenerateHeapNumbers(MacroAssembler* masm);
  void GenerateObjects(MacroAssembler* masm);
  void GenerateMiss(MacroAssembler* masm);

  bool strict() const { return op_ == Token::EQ_STRICT; }
  Condition GetCondition() const { return CompareIC::ComputeCondition(op_); }

  Token::Value op_;
  CompareIC::State state_;
};


// Flags that control the compare stub code generation.
enum CompareFlags {
  NO_COMPARE_FLAGS = 0,
  NO_SMI_COMPARE_IN_STUB = 1 << 0,
  NO_NUMBER_COMPARE_IN_STUB = 1 << 1,
  CANT_BOTH_BE_NAN = 1 << 2
};


enum NaNInformation {
  kBothCouldBeNaN,
  kCantBothBeNaN
};


class CompareStub: public CodeStub {
 public:
  CompareStub(Condition cc,
              bool strict,
              CompareFlags flags,
              Register lhs,
              Register rhs) :
     cc_(cc),
      strict_(strict),
      never_nan_nan_((flags & CANT_BOTH_BE_NAN) != 0),
      include_number_compare_((flags & NO_NUMBER_COMPARE_IN_STUB) == 0),
      include_smi_compare_((flags & NO_SMI_COMPARE_IN_STUB) == 0),
      lhs_(lhs),
      rhs_(rhs),
      name_(NULL) { }

  CompareStub(Condition cc,
              bool strict,
              CompareFlags flags) :
      cc_(cc),
      strict_(strict),
      never_nan_nan_((flags & CANT_BOTH_BE_NAN) != 0),
      include_number_compare_((flags & NO_NUMBER_COMPARE_IN_STUB) == 0),
      include_smi_compare_((flags & NO_SMI_COMPARE_IN_STUB) == 0),
      lhs_(no_reg),
      rhs_(no_reg),
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

  // Generate the comparison code for two smi operands in the stub.
  bool include_smi_compare_;

  // Register holding the left hand side of the comparison if the stub gives
  // a choice, no_reg otherwise.

  Register lhs_;
  // Register holding the right hand side of the comparison if the stub gives
  // a choice, no_reg otherwise.
  Register rhs_;

  // Encoding of the minor key in 16 bits.
  class StrictField: public BitField<bool, 0, 1> {};
  class NeverNanNanField: public BitField<bool, 1, 1> {};
  class IncludeNumberCompareField: public BitField<bool, 2, 1> {};
  class IncludeSmiCompareField: public  BitField<bool, 3, 1> {};
  class RegisterField: public BitField<bool, 4, 1> {};
  class ConditionField: public BitField<int, 5, 11> {};

  Major MajorKey() { return Compare; }

  int MinorKey();

  virtual int GetCodeKind() { return Code::COMPARE_IC; }
  virtual void FinishCode(Code* code) {
    code->set_compare_state(CompareIC::GENERIC);
  }

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
    PrintF("CompareStub (minor %d) (cc %d), (strict %s), "
           "(never_nan_nan %s), (smi_compare %s) (number_compare %s) ",
           MinorKey(),
           static_cast<int>(cc_),
           strict_ ? "true" : "false",
           never_nan_nan_ ? "true" : "false",
           include_smi_compare_ ? "inluded" : "not included",
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
  explicit CEntryStub(int result_size)
      : result_size_(result_size), save_doubles_(false) { }

  void Generate(MacroAssembler* masm);
  void SaveDoubles() { save_doubles_ = true; }

 private:
  void GenerateCore(MacroAssembler* masm,
                    Label* throw_normal_exception,
                    Label* throw_termination_exception,
                    Label* throw_out_of_memory_exception,
                    bool do_gc,
                    bool always_allocate_scope);
  void GenerateThrowTOS(MacroAssembler* masm);
  void GenerateThrowUncatchable(MacroAssembler* masm,
                                UncatchableExceptionType type);

  // Number of pointers/values returned.
  const int result_size_;
  bool save_doubles_;

  Major MajorKey() { return CEntry; }
  int MinorKey();

  const char* GetName() { return "CEntryStub"; }
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


class RegExpConstructResultStub: public CodeStub {
 public:
  RegExpConstructResultStub() { }

 private:
  Major MajorKey() { return RegExpConstructResult; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  const char* GetName() { return "RegExpConstructResultStub"; }

#ifdef DEBUG
  void Print() {
    PrintF("RegExpConstructResultStub\n");
  }
#endif
};


class CallFunctionStub: public CodeStub {
 public:
  CallFunctionStub(int argc, InLoopFlag in_loop, CallFunctionFlags flags)
      : argc_(argc), in_loop_(in_loop), flags_(flags) { }

  void Generate(MacroAssembler* masm);

  static int ExtractArgcFromMinorKey(int minor_key) {
    return ArgcBits::decode(minor_key);
  }

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


class AllowStubCallsScope {
 public:
  AllowStubCallsScope(MacroAssembler* masm, bool allow)
       : masm_(masm), previous_allow_(masm->allow_stub_calls()) {
    masm_->set_allow_stub_calls(allow);
  }
  ~AllowStubCallsScope() {
    masm_->set_allow_stub_calls(previous_allow_);
  }

 private:
  MacroAssembler* masm_;
  bool previous_allow_;

  DISALLOW_COPY_AND_ASSIGN(AllowStubCallsScope);
};

} }  // namespace v8::internal

#endif  // V8_CODE_STUBS_H_

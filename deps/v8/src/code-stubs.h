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

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "allocation.h"
#include "globals.h"
#include "codegen.h"

namespace v8 {
namespace internal {

// List of code stubs used on all platforms.
#define CODE_STUB_LIST_ALL_PLATFORMS(V)  \
  V(CallFunction)                        \
  V(CallConstruct)                       \
  V(UnaryOp)                             \
  V(BinaryOp)                            \
  V(StringAdd)                           \
  V(SubString)                           \
  V(StringCompare)                       \
  V(Compare)                             \
  V(CompareIC)                           \
  V(MathPow)                             \
  V(RecordWrite)                         \
  V(StoreBufferOverflow)                 \
  V(RegExpExec)                          \
  V(TranscendentalCache)                 \
  V(Instanceof)                          \
  V(ConvertToDouble)                     \
  V(WriteInt32ToHeapNumber)              \
  V(StackCheck)                          \
  V(Interrupt)                           \
  V(FastNewClosure)                      \
  V(FastNewContext)                      \
  V(FastNewBlockContext)                 \
  V(FastCloneShallowArray)               \
  V(FastCloneShallowObject)              \
  V(ToBoolean)                           \
  V(ToNumber)                            \
  V(ArgumentsAccess)                     \
  V(RegExpConstructResult)               \
  V(NumberToString)                      \
  V(CEntry)                              \
  V(JSEntry)                             \
  V(KeyedLoadElement)                    \
  V(KeyedStoreElement)                   \
  V(DebuggerStatement)                   \
  V(StringDictionaryLookup)              \
  V(ElementsTransitionAndStore)          \
  V(StoreArrayLiteralElement)            \
  V(ProfileEntryHook)

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

// List of code stubs only used on MIPS platforms.
#ifdef V8_TARGET_ARCH_MIPS
#define CODE_STUB_LIST_MIPS(V)  \
  V(RegExpCEntry)               \
  V(DirectCEntry)
#else
#define CODE_STUB_LIST_MIPS(V)
#endif

// Combined list of code stubs.
#define CODE_STUB_LIST(V)            \
  CODE_STUB_LIST_ALL_PLATFORMS(V)    \
  CODE_STUB_LIST_ARM(V)              \
  CODE_STUB_LIST_MIPS(V)

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

  bool CompilingCallsToThisStubIsGCSafe() {
    bool is_pregenerated = IsPregenerated();
    Code* code = NULL;
    CHECK(!is_pregenerated || FindCodeInCache(&code));
    return is_pregenerated;
  }

  // See comment above, where Instanceof is defined.
  virtual bool IsPregenerated() { return false; }

  static void GenerateStubsAheadOfTime();
  static void GenerateFPStubs();

  // Some stubs put untagged junk on the stack that cannot be scanned by the
  // GC.  This means that we must be statically sure that no GC can occur while
  // they are running.  If that is the case they should override this to return
  // true, which will cause an assertion if we try to call something that can
  // GC or if we try to put a stack frame on top of the junk, which would not
  // result in a traversable stack.
  virtual bool SometimesSetsUpAFrame() { return true; }

  // Lookup the code in the (possibly custom) cache.
  bool FindCodeInCache(Code** code_out);

 private:
  // Nonvirtual wrapper around the stub-specific Generate function.  Call
  // this function to set up the macro assembler and generate the code.
  void GenerateCode(MacroAssembler* masm);

  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;

  // Perform bookkeeping required after code generation when stub code is
  // initially generated.
  void RecordCodeGeneration(Code* code, MacroAssembler* masm);

  // Finish the code object after it has been generated.
  virtual void FinishCode(Handle<Code> code) { }

  // Activate newly generated stub. Is called after
  // registering stub in the stub cache.
  virtual void Activate(Code* code) { }

  // Returns information for computing the number key.
  virtual Major MajorKey() = 0;
  virtual int MinorKey() = 0;

  // BinaryOpStub needs to override this.
  virtual int GetCodeKind();

  // BinaryOpStub needs to override this.
  virtual InlineCacheState GetICState() {
    return UNINITIALIZED;
  }

  // Add the code to a specialized cache, specific to an individual
  // stub type. Please note, this method must add the code object to a
  // roots object, otherwise we will remove the code during GC.
  virtual void AddToSpecialCache(Handle<Code> new_object) { }

  // Find code in a specialized cache, work is delegated to the specific stub.
  virtual bool FindCodeInSpecialCache(Code** code_out) { return false; }

  // If a stub uses a special cache override this.
  virtual bool UseSpecialCache() { return false; }

  // Returns a name for logging/debugging purposes.
  SmartArrayPointer<const char> GetName();
  virtual void PrintName(StringStream* stream);

  // Returns whether the code generated for this stub needs to be allocated as
  // a fixed (non-moveable) code object.
  virtual bool NeedsImmovableCode() { return false; }

  // Computes the key based on major and minor.
  uint32_t GetKey() {
    ASSERT(static_cast<int>(MajorKey()) < NUMBER_OF_IDS);
    return MinorKeyBits::encode(MinorKey()) |
           MajorKeyBits::encode(MajorKey());
  }

  class MajorKeyBits: public BitField<uint32_t, 0, kStubMajorKeyBits> {};
  class MinorKeyBits: public BitField<uint32_t,
      kStubMajorKeyBits, kStubMinorKeyBits> {};  // NOLINT

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
  Major MajorKey() { return StackCheck; }
  int MinorKey() { return 0; }
};


class InterruptStub : public CodeStub {
 public:
  InterruptStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return Interrupt; }
  int MinorKey() { return 0; }
};


class ToNumberStub: public CodeStub {
 public:
  ToNumberStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return ToNumber; }
  int MinorKey() { return 0; }
};


class FastNewClosureStub : public CodeStub {
 public:
  explicit FastNewClosureStub(LanguageMode language_mode)
    : language_mode_(language_mode) { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return FastNewClosure; }
  int MinorKey() { return language_mode_ == CLASSIC_MODE
        ? kNonStrictMode : kStrictMode; }

  LanguageMode language_mode_;
};


class FastNewContextStub : public CodeStub {
 public:
  static const int kMaximumSlots = 64;

  explicit FastNewContextStub(int slots) : slots_(slots) {
    ASSERT(slots_ > 0 && slots_ <= kMaximumSlots);
  }

  void Generate(MacroAssembler* masm);

 private:
  int slots_;

  Major MajorKey() { return FastNewContext; }
  int MinorKey() { return slots_; }
};


class FastNewBlockContextStub : public CodeStub {
 public:
  static const int kMaximumSlots = 64;

  explicit FastNewBlockContextStub(int slots) : slots_(slots) {
    ASSERT(slots_ > 0 && slots_ <= kMaximumSlots);
  }

  void Generate(MacroAssembler* masm);

 private:
  int slots_;

  Major MajorKey() { return FastNewBlockContext; }
  int MinorKey() { return slots_; }
};


class FastCloneShallowArrayStub : public CodeStub {
 public:
  // Maximum length of copied elements array.
  static const int kMaximumClonedLength = 8;

  enum Mode {
    CLONE_ELEMENTS,
    CLONE_DOUBLE_ELEMENTS,
    COPY_ON_WRITE_ELEMENTS,
    CLONE_ANY_ELEMENTS
  };

  FastCloneShallowArrayStub(Mode mode, int length)
      : mode_(mode),
        length_((mode == COPY_ON_WRITE_ELEMENTS) ? 0 : length) {
    ASSERT_GE(length_, 0);
    ASSERT_LE(length_, kMaximumClonedLength);
  }

  void Generate(MacroAssembler* masm);

 private:
  Mode mode_;
  int length_;

  Major MajorKey() { return FastCloneShallowArray; }
  int MinorKey() {
    ASSERT(mode_ == 0 || mode_ == 1 || mode_ == 2 || mode_ == 3);
    return length_ * 4 +  mode_;
  }
};


class FastCloneShallowObjectStub : public CodeStub {
 public:
  // Maximum number of properties in copied object.
  static const int kMaximumClonedProperties = 6;

  explicit FastCloneShallowObjectStub(int length) : length_(length) {
    ASSERT_GE(length_, 0);
    ASSERT_LE(length_, kMaximumClonedProperties);
  }

  void Generate(MacroAssembler* masm);

 private:
  int length_;

  Major MajorKey() { return FastCloneShallowObject; }
  int MinorKey() { return length_; }
};


class InstanceofStub: public CodeStub {
 public:
  enum Flags {
    kNoFlags = 0,
    kArgsInRegisters = 1 << 0,
    kCallSiteInlineCheck = 1 << 1,
    kReturnTrueFalseObject = 1 << 2
  };

  explicit InstanceofStub(Flags flags) : flags_(flags) { }

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

  virtual void PrintName(StringStream* stream);

  Flags flags_;
};


class MathPowStub: public CodeStub {
 public:
  enum ExponentType { INTEGER, DOUBLE, TAGGED, ON_STACK};

  explicit MathPowStub(ExponentType exponent_type)
      : exponent_type_(exponent_type) { }
  virtual void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return MathPow; }
  virtual int MinorKey() { return exponent_type_; }

  ExponentType exponent_type_;
};


class ICCompareStub: public CodeStub {
 public:
  ICCompareStub(Token::Value op, CompareIC::State state)
      : op_(op), state_(state) {
    ASSERT(Token::IsCompareOp(op));
  }

  virtual void Generate(MacroAssembler* masm);

  void set_known_map(Handle<Map> map) { known_map_ = map; }

 private:
  class OpField: public BitField<int, 0, 3> { };
  class StateField: public BitField<int, 3, 5> { };

  virtual void FinishCode(Handle<Code> code) {
    code->set_compare_state(state_);
    code->set_compare_operation(op_ - Token::EQ);
  }

  virtual CodeStub::Major MajorKey() { return CompareIC; }
  virtual int MinorKey();

  virtual int GetCodeKind() { return Code::COMPARE_IC; }

  void GenerateSmis(MacroAssembler* masm);
  void GenerateHeapNumbers(MacroAssembler* masm);
  void GenerateSymbols(MacroAssembler* masm);
  void GenerateStrings(MacroAssembler* masm);
  void GenerateObjects(MacroAssembler* masm);
  void GenerateMiss(MacroAssembler* masm);
  void GenerateKnownObjects(MacroAssembler* masm);

  bool strict() const { return op_ == Token::EQ_STRICT; }
  Condition GetCondition() const { return CompareIC::ComputeCondition(op_); }

  virtual void AddToSpecialCache(Handle<Code> new_object);
  virtual bool FindCodeInSpecialCache(Code** code_out);
  virtual bool UseSpecialCache() { return state_ == CompareIC::KNOWN_OBJECTS; }

  Token::Value op_;
  CompareIC::State state_;
  Handle<Map> known_map_;
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
      rhs_(rhs) { }

  CompareStub(Condition cc,
              bool strict,
              CompareFlags flags) :
      cc_(cc),
      strict_(strict),
      never_nan_nan_((flags & CANT_BOTH_BE_NAN) != 0),
      include_number_compare_((flags & NO_NUMBER_COMPARE_IN_STUB) == 0),
      include_smi_compare_((flags & NO_SMI_COMPARE_IN_STUB) == 0),
      lhs_(no_reg),
      rhs_(no_reg) { }

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
  virtual void FinishCode(Handle<Code> code) {
    code->set_compare_state(CompareIC::GENERIC);
  }

  // Branch to the label if the given object isn't a symbol.
  void BranchIfNonSymbol(MacroAssembler* masm,
                         Label* label,
                         Register object,
                         Register scratch);

  // Unfortunately you have to run without snapshots to see most of these
  // names in the profile since most compare stubs end up in the snapshot.
  virtual void PrintName(StringStream* stream);
};


class CEntryStub : public CodeStub {
 public:
  explicit CEntryStub(int result_size,
                      SaveFPRegsMode save_doubles = kDontSaveFPRegs)
      : result_size_(result_size), save_doubles_(save_doubles) { }

  void Generate(MacroAssembler* masm);

  // The version of this stub that doesn't save doubles is generated ahead of
  // time, so it's OK to call it from other stubs that can't cope with GC during
  // their code generation.  On machines that always have gp registers (x64) we
  // can generate both variants ahead of time.
  virtual bool IsPregenerated();
  static void GenerateAheadOfTime();

 private:
  void GenerateCore(MacroAssembler* masm,
                    Label* throw_normal_exception,
                    Label* throw_termination_exception,
                    Label* throw_out_of_memory_exception,
                    bool do_gc,
                    bool always_allocate_scope);

  // Number of pointers/values returned.
  const int result_size_;
  SaveFPRegsMode save_doubles_;

  Major MajorKey() { return CEntry; }
  int MinorKey();

  bool NeedsImmovableCode();
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

  virtual void FinishCode(Handle<Code> code);

  int handler_offset_;
};


class JSConstructEntryStub : public JSEntryStub {
 public:
  JSConstructEntryStub() { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, true); }

 private:
  int MinorKey() { return 1; }

  virtual void PrintName(StringStream* stream) {
    stream->Add("JSConstructEntryStub");
  }
};


class ArgumentsAccessStub: public CodeStub {
 public:
  enum Type {
    READ_ELEMENT,
    NEW_NON_STRICT_FAST,
    NEW_NON_STRICT_SLOW,
    NEW_STRICT
  };

  explicit ArgumentsAccessStub(Type type) : type_(type) { }

 private:
  Type type_;

  Major MajorKey() { return ArgumentsAccess; }
  int MinorKey() { return type_; }

  void Generate(MacroAssembler* masm);
  void GenerateReadElement(MacroAssembler* masm);
  void GenerateNewStrict(MacroAssembler* masm);
  void GenerateNewNonStrictFast(MacroAssembler* masm);
  void GenerateNewNonStrictSlow(MacroAssembler* masm);

  virtual void PrintName(StringStream* stream);
};


class RegExpExecStub: public CodeStub {
 public:
  RegExpExecStub() { }

 private:
  Major MajorKey() { return RegExpExec; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class RegExpConstructResultStub: public CodeStub {
 public:
  RegExpConstructResultStub() { }

 private:
  Major MajorKey() { return RegExpConstructResult; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class CallFunctionStub: public CodeStub {
 public:
  CallFunctionStub(int argc, CallFunctionFlags flags)
      : argc_(argc), flags_(flags) { }

  void Generate(MacroAssembler* masm);

  virtual void FinishCode(Handle<Code> code) {
    code->set_has_function_cache(RecordCallTarget());
  }

  static int ExtractArgcFromMinorKey(int minor_key) {
    return ArgcBits::decode(minor_key);
  }

 private:
  int argc_;
  CallFunctionFlags flags_;

  virtual void PrintName(StringStream* stream);

  // Minor key encoding in 32 bits with Bitfield <Type, shift, size>.
  class FlagBits: public BitField<CallFunctionFlags, 0, 2> {};
  class ArgcBits: public BitField<unsigned, 2, 32 - 2> {};

  Major MajorKey() { return CallFunction; }
  int MinorKey() {
    // Encode the parameters in a unique 32 bit value.
    return FlagBits::encode(flags_) | ArgcBits::encode(argc_);
  }

  bool ReceiverMightBeImplicit() {
    return (flags_ & RECEIVER_MIGHT_BE_IMPLICIT) != 0;
  }

  bool RecordCallTarget() {
    return (flags_ & RECORD_CALL_TARGET) != 0;
  }
};


class CallConstructStub: public CodeStub {
 public:
  explicit CallConstructStub(CallFunctionFlags flags) : flags_(flags) {}

  void Generate(MacroAssembler* masm);

  virtual void FinishCode(Handle<Code> code) {
    code->set_has_function_cache(RecordCallTarget());
  }

 private:
  CallFunctionFlags flags_;

  virtual void PrintName(StringStream* stream);

  Major MajorKey() { return CallConstruct; }
  int MinorKey() { return flags_; }

  bool RecordCallTarget() {
    return (flags_ & RECORD_CALL_TARGET) != 0;
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
                            Register result,
                            Label* receiver_not_string,
                            Label* index_not_number,
                            Label* index_out_of_range,
                            StringIndexFlags index_flags)
      : object_(object),
        index_(index),
        result_(result),
        receiver_not_string_(receiver_not_string),
        index_not_number_(index_not_number),
        index_out_of_range_(index_out_of_range),
        index_flags_(index_flags) {
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
                        Register scratch,
                        Register result,
                        Label* receiver_not_string,
                        Label* index_not_number,
                        Label* index_out_of_range,
                        StringIndexFlags index_flags)
      : char_code_at_generator_(object,
                                index,
                                scratch,
                                receiver_not_string,
                                index_not_number,
                                index_out_of_range,
                                index_flags),
        char_from_code_generator_(scratch, result) {}

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


class KeyedLoadElementStub : public CodeStub {
 public:
  explicit KeyedLoadElementStub(ElementsKind elements_kind)
      : elements_kind_(elements_kind)
  { }

  Major MajorKey() { return KeyedLoadElement; }
  int MinorKey() { return elements_kind_; }

  void Generate(MacroAssembler* masm);

 private:
  ElementsKind elements_kind_;

  DISALLOW_COPY_AND_ASSIGN(KeyedLoadElementStub);
};


class KeyedStoreElementStub : public CodeStub {
 public:
  KeyedStoreElementStub(bool is_js_array,
                        ElementsKind elements_kind,
                        KeyedAccessGrowMode grow_mode)
      : is_js_array_(is_js_array),
        elements_kind_(elements_kind),
        grow_mode_(grow_mode) { }

  Major MajorKey() { return KeyedStoreElement; }
  int MinorKey() {
    return ElementsKindBits::encode(elements_kind_) |
        IsJSArrayBits::encode(is_js_array_) |
        GrowModeBits::encode(grow_mode_);
  }

  void Generate(MacroAssembler* masm);

 private:
  class ElementsKindBits: public BitField<ElementsKind,    0, 8> {};
  class GrowModeBits: public BitField<KeyedAccessGrowMode, 8, 1> {};
  class IsJSArrayBits: public BitField<bool,               9, 1> {};

  bool is_js_array_;
  ElementsKind elements_kind_;
  KeyedAccessGrowMode grow_mode_;

  DISALLOW_COPY_AND_ASSIGN(KeyedStoreElementStub);
};


class ToBooleanStub: public CodeStub {
 public:
  enum Type {
    UNDEFINED,
    BOOLEAN,
    NULL_TYPE,
    SMI,
    SPEC_OBJECT,
    STRING,
    HEAP_NUMBER,
    NUMBER_OF_TYPES
  };

  // At most 8 different types can be distinguished, because the Code object
  // only has room for a single byte to hold a set of these types. :-P
  STATIC_ASSERT(NUMBER_OF_TYPES <= 8);

  class Types {
   public:
    Types() {}
    explicit Types(byte bits) : set_(bits) {}

    bool IsEmpty() const { return set_.IsEmpty(); }
    bool Contains(Type type) const { return set_.Contains(type); }
    void Add(Type type) { set_.Add(type); }
    byte ToByte() const { return set_.ToIntegral(); }
    void Print(StringStream* stream) const;
    void TraceTransition(Types to) const;
    bool Record(Handle<Object> object);
    bool NeedsMap() const;
    bool CanBeUndetectable() const;

   private:
    EnumSet<Type, byte> set_;
  };

  static Types no_types() { return Types(); }
  static Types all_types() { return Types((1 << NUMBER_OF_TYPES) - 1); }

  explicit ToBooleanStub(Register tos, Types types = Types())
      : tos_(tos), types_(types) { }

  void Generate(MacroAssembler* masm);
  virtual int GetCodeKind() { return Code::TO_BOOLEAN_IC; }
  virtual void PrintName(StringStream* stream);

  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  Major MajorKey() { return ToBoolean; }
  int MinorKey() { return (tos_.code() << NUMBER_OF_TYPES) | types_.ToByte(); }

  virtual void FinishCode(Handle<Code> code) {
    code->set_to_boolean_state(types_.ToByte());
  }

  void CheckOddball(MacroAssembler* masm,
                    Type type,
                    Heap::RootListIndex value,
                    bool result);
  void GenerateTypeTransition(MacroAssembler* masm);

  Register tos_;
  Types types_;
};


class ElementsTransitionAndStoreStub : public CodeStub {
 public:
  ElementsTransitionAndStoreStub(ElementsKind from,
                                 ElementsKind to,
                                 bool is_jsarray,
                                 StrictModeFlag strict_mode,
                                 KeyedAccessGrowMode grow_mode)
      : from_(from),
        to_(to),
        is_jsarray_(is_jsarray),
        strict_mode_(strict_mode),
        grow_mode_(grow_mode) {}

 private:
  class FromBits:       public BitField<ElementsKind,      0, 8> {};
  class ToBits:         public BitField<ElementsKind,      8, 8> {};
  class IsJSArrayBits:  public BitField<bool,              16, 1> {};
  class StrictModeBits: public BitField<StrictModeFlag,    17, 1> {};
  class GrowModeBits: public BitField<KeyedAccessGrowMode, 18, 1> {};

  Major MajorKey() { return ElementsTransitionAndStore; }
  int MinorKey() {
    return FromBits::encode(from_) |
        ToBits::encode(to_) |
        IsJSArrayBits::encode(is_jsarray_) |
        StrictModeBits::encode(strict_mode_) |
        GrowModeBits::encode(grow_mode_);
  }

  void Generate(MacroAssembler* masm);

  ElementsKind from_;
  ElementsKind to_;
  bool is_jsarray_;
  StrictModeFlag strict_mode_;
  KeyedAccessGrowMode grow_mode_;

  DISALLOW_COPY_AND_ASSIGN(ElementsTransitionAndStoreStub);
};


class StoreArrayLiteralElementStub : public CodeStub {
 public:
  explicit StoreArrayLiteralElementStub() {}

 private:
  Major MajorKey() { return StoreArrayLiteralElement; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(StoreArrayLiteralElementStub);
};


class ProfileEntryHookStub : public CodeStub {
 public:
  explicit ProfileEntryHookStub() {}

  // The profile entry hook function is not allowed to cause a GC.
  virtual bool SometimesSetsUpAFrame() { return false; }

  // Generates a call to the entry hook if it's enabled.
  static void MaybeCallEntryHook(MacroAssembler* masm);

  // Sets or unsets the entry hook function. Returns true on success,
  // false on an attempt to replace a non-NULL entry hook with another
  // non-NULL hook.
  static bool SetFunctionEntryHook(FunctionEntryHook entry_hook);

 private:
  static void EntryHookTrampoline(intptr_t function,
                                  intptr_t stack_pointer);

  Major MajorKey() { return ProfileEntryHook; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  // The current function entry hook.
  static FunctionEntryHook entry_hook_;

  DISALLOW_COPY_AND_ASSIGN(ProfileEntryHookStub);
};

} }  // namespace v8::internal

#endif  // V8_CODE_STUBS_H_

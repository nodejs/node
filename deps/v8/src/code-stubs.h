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
#include "assembler.h"
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
  V(CompareNilIC)                        \
  V(MathPow)                             \
  V(StringLength)                        \
  V(FunctionPrototype)                   \
  V(StoreArrayLength)                    \
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
  V(ArrayNoArgumentConstructor)          \
  V(ArraySingleArgumentConstructor)      \
  V(ArrayNArgumentsConstructor)          \
  V(KeyedStoreElement)                   \
  V(DebuggerStatement)                   \
  V(NameDictionaryLookup)                \
  V(ElementsTransitionAndStore)          \
  V(TransitionElementsKind)              \
  V(StoreArrayLiteralElement)            \
  V(StubFailureTrampoline)               \
  V(ArrayConstructor)                    \
  V(ProfileEntryHook)                    \
  /* IC Handler stubs */                 \
  V(LoadField)

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
  Handle<Code> GetCode(Isolate* isolate);

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

  bool CompilingCallsToThisStubIsGCSafe(Isolate* isolate) {
    bool is_pregenerated = IsPregenerated();
    Code* code = NULL;
    CHECK(!is_pregenerated || FindCodeInCache(&code, isolate));
    return is_pregenerated;
  }

  // See comment above, where Instanceof is defined.
  virtual bool IsPregenerated() { return false; }

  static void GenerateStubsAheadOfTime(Isolate* isolate);
  static void GenerateFPStubs(Isolate* isolate);

  // Some stubs put untagged junk on the stack that cannot be scanned by the
  // GC.  This means that we must be statically sure that no GC can occur while
  // they are running.  If that is the case they should override this to return
  // true, which will cause an assertion if we try to call something that can
  // GC or if we try to put a stack frame on top of the junk, which would not
  // result in a traversable stack.
  virtual bool SometimesSetsUpAFrame() { return true; }

  // Lookup the code in the (possibly custom) cache.
  bool FindCodeInCache(Code** code_out, Isolate* isolate);

  // Returns information for computing the number key.
  virtual Major MajorKey() = 0;
  virtual int MinorKey() = 0;

  virtual InlineCacheState GetICState() {
    return UNINITIALIZED;
  }
  virtual Code::ExtraICState GetExtraICState() {
    return Code::kNoExtraICState;
  }

 protected:
  static bool CanUseFPRegisters();

  // Generates the assembler code for the stub.
  virtual Handle<Code> GenerateCode() = 0;

  virtual Code::StubType GetStubType() {
    return Code::NORMAL;
  }

  // Returns whether the code generated for this stub needs to be allocated as
  // a fixed (non-moveable) code object.
  virtual bool NeedsImmovableCode() { return false; }

 private:
  // Perform bookkeeping required after code generation when stub code is
  // initially generated.
  void RecordCodeGeneration(Code* code, Isolate* isolate);

  // Finish the code object after it has been generated.
  virtual void FinishCode(Handle<Code> code) { }

  // Activate newly generated stub. Is called after
  // registering stub in the stub cache.
  virtual void Activate(Code* code) { }

  // BinaryOpStub needs to override this.
  virtual Code::Kind GetCodeKind() const;

  // Add the code to a specialized cache, specific to an individual
  // stub type. Please note, this method must add the code object to a
  // roots object, otherwise we will remove the code during GC.
  virtual void AddToSpecialCache(Handle<Code> new_object) { }

  // Find code in a specialized cache, work is delegated to the specific stub.
  virtual bool FindCodeInSpecialCache(Code** code_out, Isolate* isolate) {
    return false;
  }

  // If a stub uses a special cache override this.
  virtual bool UseSpecialCache() { return false; }

  // Returns a name for logging/debugging purposes.
  SmartArrayPointer<const char> GetName();
  virtual void PrintName(StringStream* stream);

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


class PlatformCodeStub : public CodeStub {
 public:
  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode();

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }
  virtual int GetStubFlags() { return -1; }

 protected:
  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;
};


enum StubFunctionMode { NOT_JS_FUNCTION_STUB_MODE, JS_FUNCTION_STUB_MODE };


struct CodeStubInterfaceDescriptor {
  CodeStubInterfaceDescriptor();
  int register_param_count_;
  const Register* stack_parameter_count_;
  // if hint_stack_parameter_count_ > 0, the code stub can optimize the
  // return sequence. Default value is -1, which means it is ignored.
  int hint_stack_parameter_count_;
  StubFunctionMode function_mode_;
  Register* register_params_;
  Address deoptimization_handler_;
  ExternalReference miss_handler_;

  int environment_length() const {
    if (stack_parameter_count_ != NULL) {
      return register_param_count_ + 1;
    }
    return register_param_count_;
  }

  bool initialized() const { return register_param_count_ >= 0; }
};

// A helper to make up for the fact that type Register is not fully
// defined outside of the platform directories
#define DESCRIPTOR_GET_PARAMETER_REGISTER(descriptor, index) \
  ((index) == (descriptor)->register_param_count_)           \
      ? *((descriptor)->stack_parameter_count_)              \
      : (descriptor)->register_params_[(index)]


class HydrogenCodeStub : public CodeStub {
 public:
  enum InitializationState {
    CODE_STUB_IS_NOT_MISS,
    CODE_STUB_IS_MISS
  };

  explicit HydrogenCodeStub(InitializationState state) {
    is_miss_ = (state == CODE_STUB_IS_MISS);
  }

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }

  CodeStubInterfaceDescriptor* GetInterfaceDescriptor(Isolate* isolate) {
    return isolate->code_stub_interface_descriptor(MajorKey());
  }

  bool IsMiss() { return is_miss_; }

  template<class SubClass>
  static Handle<Code> GetUninitialized(Isolate* isolate) {
    SubClass::GenerateAheadOfTime(isolate);
    return SubClass().GetCode(isolate);
  }

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor) = 0;

  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode() = 0;

  virtual int NotMissMinorKey() = 0;

  Handle<Code> GenerateLightweightMissCode(Isolate* isolate);

 private:
  class MinorKeyBits: public BitField<int, 0, kStubMinorKeyBits - 1> {};
  class IsMissBits: public BitField<bool, kStubMinorKeyBits - 1, 1> {};

  void GenerateLightweightMiss(MacroAssembler* masm);
  virtual int MinorKey() {
    return IsMissBits::encode(is_miss_) |
        MinorKeyBits::encode(NotMissMinorKey());
  }

  bool is_miss_;
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


class StackCheckStub : public PlatformCodeStub {
 public:
  StackCheckStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return StackCheck; }
  int MinorKey() { return 0; }
};


class InterruptStub : public PlatformCodeStub {
 public:
  InterruptStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return Interrupt; }
  int MinorKey() { return 0; }
};


class ToNumberStub: public PlatformCodeStub {
 public:
  ToNumberStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return ToNumber; }
  int MinorKey() { return 0; }
};


class FastNewClosureStub : public PlatformCodeStub {
 public:
  explicit FastNewClosureStub(LanguageMode language_mode, bool is_generator)
    : language_mode_(language_mode),
      is_generator_(is_generator) { }

  void Generate(MacroAssembler* masm);

 private:
  class StrictModeBits: public BitField<bool, 0, 1> {};
  class IsGeneratorBits: public BitField<bool, 1, 1> {};

  Major MajorKey() { return FastNewClosure; }
  int MinorKey() {
    return StrictModeBits::encode(language_mode_ != CLASSIC_MODE) |
      IsGeneratorBits::encode(is_generator_);
  }

  LanguageMode language_mode_;
  bool is_generator_;
};


class FastNewContextStub : public PlatformCodeStub {
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


class FastNewBlockContextStub : public PlatformCodeStub {
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


class FastCloneShallowArrayStub : public HydrogenCodeStub {
 public:
  // Maximum length of copied elements array.
  static const int kMaximumClonedLength = 8;
  enum Mode {
    CLONE_ELEMENTS,
    CLONE_DOUBLE_ELEMENTS,
    COPY_ON_WRITE_ELEMENTS,
    CLONE_ANY_ELEMENTS,
    LAST_CLONE_MODE = CLONE_ANY_ELEMENTS
  };

  static const int kFastCloneModeCount = LAST_CLONE_MODE + 1;

  FastCloneShallowArrayStub(Mode mode,
                            AllocationSiteMode allocation_site_mode,
                            int length)
      : HydrogenCodeStub(CODE_STUB_IS_NOT_MISS),
        mode_(mode),
        allocation_site_mode_(allocation_site_mode),
        length_((mode == COPY_ON_WRITE_ELEMENTS) ? 0 : length) {
    ASSERT_GE(length_, 0);
    ASSERT_LE(length_, kMaximumClonedLength);
  }

  Mode mode() const { return mode_; }
  int length() const { return length_; }
  AllocationSiteMode allocation_site_mode() const {
    return allocation_site_mode_;
  }

  ElementsKind ComputeElementsKind() const {
    switch (mode()) {
      case CLONE_ELEMENTS:
      case COPY_ON_WRITE_ELEMENTS:
        return FAST_ELEMENTS;
      case CLONE_DOUBLE_ELEMENTS:
        return FAST_DOUBLE_ELEMENTS;
      case CLONE_ANY_ELEMENTS:
        /*fall-through*/;
    }
    UNREACHABLE();
    return LAST_ELEMENTS_KIND;
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  Mode mode_;
  AllocationSiteMode allocation_site_mode_;
  int length_;

  class AllocationSiteModeBits: public BitField<AllocationSiteMode, 0, 1> {};
  class ModeBits: public BitField<Mode, 1, 4> {};
  class LengthBits: public BitField<int, 5, 4> {};
  // Ensure data fits within available bits.
  STATIC_ASSERT(LAST_ALLOCATION_SITE_MODE == 1);
  STATIC_ASSERT(kFastCloneModeCount < 16);
  STATIC_ASSERT(kMaximumClonedLength < 16);
  Major MajorKey() { return FastCloneShallowArray; }
  int NotMissMinorKey() {
    return AllocationSiteModeBits::encode(allocation_site_mode_)
        | ModeBits::encode(mode_)
        | LengthBits::encode(length_);
  }
};


class FastCloneShallowObjectStub : public HydrogenCodeStub {
 public:
  // Maximum number of properties in copied object.
  static const int kMaximumClonedProperties = 6;

  explicit FastCloneShallowObjectStub(int length)
      : HydrogenCodeStub(CODE_STUB_IS_NOT_MISS),
        length_(length) {
    ASSERT_GE(length_, 0);
    ASSERT_LE(length_, kMaximumClonedProperties);
  }

  int length() const { return length_; }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  int length_;

  Major MajorKey() { return FastCloneShallowObject; }
  int NotMissMinorKey() { return length_; }

  DISALLOW_COPY_AND_ASSIGN(FastCloneShallowObjectStub);
};


class InstanceofStub: public PlatformCodeStub {
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


class ArrayConstructorStub: public PlatformCodeStub {
 public:
  enum ArgumentCountKey { ANY, NONE, ONE, MORE_THAN_ONE };
  ArrayConstructorStub(Isolate* isolate, int argument_count);
  explicit ArrayConstructorStub(Isolate* isolate);

  void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return ArrayConstructor; }
  virtual int MinorKey() { return argument_count_; }

  ArgumentCountKey argument_count_;
};


class MathPowStub: public PlatformCodeStub {
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


class ICStub: public PlatformCodeStub {
 public:
  explicit ICStub(Code::Kind kind) : kind_(kind) { }
  virtual Code::Kind GetCodeKind() const { return kind_; }
  virtual InlineCacheState GetICState() { return MONOMORPHIC; }

  bool Describes(Code* code) {
    return GetMajorKey(code) == MajorKey() && code->stub_info() == MinorKey();
  }

 protected:
  class KindBits: public BitField<Code::Kind, 0, 4> {};
  virtual void FinishCode(Handle<Code> code) {
    code->set_stub_info(MinorKey());
  }
  Code::Kind kind() { return kind_; }

  virtual int MinorKey() {
    return KindBits::encode(kind_);
  }

 private:
  Code::Kind kind_;
};


class FunctionPrototypeStub: public ICStub {
 public:
  explicit FunctionPrototypeStub(Code::Kind kind) : ICStub(kind) { }
  virtual void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return FunctionPrototype; }
};


class StringLengthStub: public ICStub {
 public:
  StringLengthStub(Code::Kind kind, bool support_wrapper)
      : ICStub(kind), support_wrapper_(support_wrapper) { }
  virtual void Generate(MacroAssembler* masm);

 private:
  STATIC_ASSERT(KindBits::kSize == 4);
  class WrapperModeBits: public BitField<bool, 4, 1> {};
  virtual CodeStub::Major MajorKey() { return StringLength; }
  virtual int MinorKey() {
    return KindBits::encode(kind()) | WrapperModeBits::encode(support_wrapper_);
  }

  bool support_wrapper_;
};


class StoreICStub: public ICStub {
 public:
  StoreICStub(Code::Kind kind, StrictModeFlag strict_mode)
      : ICStub(kind), strict_mode_(strict_mode) { }

 protected:
  virtual Code::ExtraICState GetExtraICState() {
    return strict_mode_;
  }

 private:
  STATIC_ASSERT(KindBits::kSize == 4);
  class StrictModeBits: public BitField<bool, 4, 1> {};
  virtual int MinorKey() {
    return KindBits::encode(kind()) | StrictModeBits::encode(strict_mode_);
  }

  StrictModeFlag strict_mode_;
};


class StoreArrayLengthStub: public StoreICStub {
 public:
  explicit StoreArrayLengthStub(Code::Kind kind, StrictModeFlag strict_mode)
      : StoreICStub(kind, strict_mode) { }
  virtual void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return StoreArrayLength; }
};


class HandlerStub: public ICStub {
 public:
  explicit HandlerStub(Code::Kind kind) : ICStub(kind) { }
  virtual Code::Kind GetCodeKind() const { return Code::STUB; }
  virtual int GetStubFlags() { return kind(); }
};


class LoadFieldStub: public HandlerStub {
 public:
  LoadFieldStub(Register reg, bool inobject, int index)
      : HandlerStub(Code::LOAD_IC),
        reg_(reg),
        inobject_(inobject),
        index_(index) { }
  virtual void Generate(MacroAssembler* masm);

 protected:
  virtual Code::StubType GetStubType() { return Code::FIELD; }

 private:
  STATIC_ASSERT(KindBits::kSize == 4);
  class RegisterBits: public BitField<int, 4, 6> {};
  class InobjectBits: public BitField<bool, 10, 1> {};
  class IndexBits: public BitField<int, 11, 11> {};
  virtual CodeStub::Major MajorKey() { return LoadField; }
  virtual int MinorKey() {
    return KindBits::encode(kind())
        | RegisterBits::encode(reg_.code())
        | InobjectBits::encode(inobject_)
        | IndexBits::encode(index_);
  }

  Register reg_;
  bool inobject_;
  int index_;
};


class BinaryOpStub: public PlatformCodeStub {
 public:
  BinaryOpStub(Token::Value op, OverwriteMode mode)
      : op_(op),
        mode_(mode),
        platform_specific_bit_(false),
        left_type_(BinaryOpIC::UNINITIALIZED),
        right_type_(BinaryOpIC::UNINITIALIZED),
        result_type_(BinaryOpIC::UNINITIALIZED) {
    Initialize();
    ASSERT(OpBits::is_valid(Token::NUM_TOKENS));
  }

  BinaryOpStub(
      int key,
      BinaryOpIC::TypeInfo left_type,
      BinaryOpIC::TypeInfo right_type,
      BinaryOpIC::TypeInfo result_type = BinaryOpIC::UNINITIALIZED)
      : op_(OpBits::decode(key)),
        mode_(ModeBits::decode(key)),
        platform_specific_bit_(PlatformSpecificBits::decode(key)),
        left_type_(left_type),
        right_type_(right_type),
        result_type_(result_type) { }

  static void decode_types_from_minor_key(int minor_key,
                                          BinaryOpIC::TypeInfo* left_type,
                                          BinaryOpIC::TypeInfo* right_type,
                                          BinaryOpIC::TypeInfo* result_type) {
    *left_type =
        static_cast<BinaryOpIC::TypeInfo>(LeftTypeBits::decode(minor_key));
    *right_type =
        static_cast<BinaryOpIC::TypeInfo>(RightTypeBits::decode(minor_key));
    *result_type =
        static_cast<BinaryOpIC::TypeInfo>(ResultTypeBits::decode(minor_key));
  }

  static Token::Value decode_op_from_minor_key(int minor_key) {
    return static_cast<Token::Value>(OpBits::decode(minor_key));
  }

  enum SmiCodeGenerateHeapNumberResults {
    ALLOW_HEAPNUMBER_RESULTS,
    NO_HEAPNUMBER_RESULTS
  };

 private:
  Token::Value op_;
  OverwriteMode mode_;
  bool platform_specific_bit_;  // Indicates SSE3 on IA32.

  // Operand type information determined at runtime.
  BinaryOpIC::TypeInfo left_type_;
  BinaryOpIC::TypeInfo right_type_;
  BinaryOpIC::TypeInfo result_type_;

  virtual void PrintName(StringStream* stream);

  // Minor key encoding in 19 bits TTTRRRLLLSOOOOOOOMM.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 7> {};
  class PlatformSpecificBits: public BitField<bool, 9, 1> {};
  class LeftTypeBits: public BitField<BinaryOpIC::TypeInfo, 10, 3> {};
  class RightTypeBits: public BitField<BinaryOpIC::TypeInfo, 13, 3> {};
  class ResultTypeBits: public BitField<BinaryOpIC::TypeInfo, 16, 3> {};

  Major MajorKey() { return BinaryOp; }
  int MinorKey() {
    return OpBits::encode(op_)
           | ModeBits::encode(mode_)
           | PlatformSpecificBits::encode(platform_specific_bit_)
           | LeftTypeBits::encode(left_type_)
           | RightTypeBits::encode(right_type_)
           | ResultTypeBits::encode(result_type_);
  }


  // Platform-independent implementation.
  void Generate(MacroAssembler* masm);
  void GenerateCallRuntime(MacroAssembler* masm);

  // Platform-independent signature, platform-specific implementation.
  void Initialize();
  void GenerateAddStrings(MacroAssembler* masm);
  void GenerateBothStringStub(MacroAssembler* masm);
  void GenerateGeneric(MacroAssembler* masm);
  void GenerateGenericStub(MacroAssembler* masm);
  void GenerateNumberStub(MacroAssembler* masm);
  void GenerateInt32Stub(MacroAssembler* masm);
  void GenerateLoadArguments(MacroAssembler* masm);
  void GenerateOddballStub(MacroAssembler* masm);
  void GenerateRegisterArgsPush(MacroAssembler* masm);
  void GenerateReturn(MacroAssembler* masm);
  void GenerateSmiStub(MacroAssembler* masm);
  void GenerateStringStub(MacroAssembler* masm);
  void GenerateTypeTransition(MacroAssembler* masm);
  void GenerateTypeTransitionWithSavedArgs(MacroAssembler* masm);
  void GenerateUninitializedStub(MacroAssembler* masm);

  // Entirely platform-specific methods are defined as static helper
  // functions in the <arch>/code-stubs-<arch>.cc files.

  virtual Code::Kind GetCodeKind() const { return Code::BINARY_OP_IC; }

  virtual InlineCacheState GetICState() {
    return BinaryOpIC::ToState(Max(left_type_, right_type_));
  }

  virtual void FinishCode(Handle<Code> code) {
    code->set_stub_info(MinorKey());
  }

  friend class CodeGenerator;
};


class ICCompareStub: public PlatformCodeStub {
 public:
  ICCompareStub(Token::Value op,
                CompareIC::State left,
                CompareIC::State right,
                CompareIC::State handler)
      : op_(op),
        left_(left),
        right_(right),
        state_(handler) {
    ASSERT(Token::IsCompareOp(op));
  }

  virtual void Generate(MacroAssembler* masm);

  void set_known_map(Handle<Map> map) { known_map_ = map; }

  static void DecodeMinorKey(int minor_key,
                             CompareIC::State* left_state,
                             CompareIC::State* right_state,
                             CompareIC::State* handler_state,
                             Token::Value* op);

  static CompareIC::State CompareState(int minor_key) {
    return static_cast<CompareIC::State>(HandlerStateField::decode(minor_key));
  }

 private:
  class OpField: public BitField<int, 0, 3> { };
  class LeftStateField: public BitField<int, 3, 4> { };
  class RightStateField: public BitField<int, 7, 4> { };
  class HandlerStateField: public BitField<int, 11, 4> { };

  virtual void FinishCode(Handle<Code> code) {
    code->set_stub_info(MinorKey());
  }

  virtual CodeStub::Major MajorKey() { return CompareIC; }
  virtual int MinorKey();

  virtual Code::Kind GetCodeKind() const { return Code::COMPARE_IC; }

  void GenerateSmis(MacroAssembler* masm);
  void GenerateNumbers(MacroAssembler* masm);
  void GenerateInternalizedStrings(MacroAssembler* masm);
  void GenerateStrings(MacroAssembler* masm);
  void GenerateUniqueNames(MacroAssembler* masm);
  void GenerateObjects(MacroAssembler* masm);
  void GenerateMiss(MacroAssembler* masm);
  void GenerateKnownObjects(MacroAssembler* masm);
  void GenerateGeneric(MacroAssembler* masm);

  bool strict() const { return op_ == Token::EQ_STRICT; }
  Condition GetCondition() const { return CompareIC::ComputeCondition(op_); }

  virtual void AddToSpecialCache(Handle<Code> new_object);
  virtual bool FindCodeInSpecialCache(Code** code_out, Isolate* isolate);
  virtual bool UseSpecialCache() { return state_ == CompareIC::KNOWN_OBJECT; }

  Token::Value op_;
  CompareIC::State left_;
  CompareIC::State right_;
  CompareIC::State state_;
  Handle<Map> known_map_;
};


class CompareNilICStub : public HydrogenCodeStub  {
 public:
  enum Types {
    kCompareAgainstNull = 1 << 0,
    kCompareAgainstUndefined = 1 << 1,
    kCompareAgainstMonomorphicMap = 1 << 2,
    kCompareAgainstUndetectable = 1 << 3,
    kFullCompare = kCompareAgainstNull | kCompareAgainstUndefined |
        kCompareAgainstUndetectable
  };

  CompareNilICStub(EqualityKind kind, NilValue nil, Types types)
      : HydrogenCodeStub(CODE_STUB_IS_NOT_MISS), bit_field_(0) {
    bit_field_ = EqualityKindField::encode(kind) |
        NilValueField::encode(nil) |
        TypesField::encode(types);
  }

  virtual InlineCacheState GetICState() {
    Types types = GetTypes();
    if (types == kFullCompare) {
      return MEGAMORPHIC;
    } else if ((types & kCompareAgainstMonomorphicMap) != 0) {
      return MONOMORPHIC;
    } else {
      return PREMONOMORPHIC;
    }
  }

  virtual Code::Kind GetCodeKind() const { return Code::COMPARE_NIL_IC; }

  Handle<Code> GenerateCode();

  static Handle<Code> GetUninitialized(Isolate* isolate,
                                       EqualityKind kind,
                                       NilValue nil) {
    return CompareNilICStub(kind, nil).GetCode(isolate);
  }

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

  static void InitializeForIsolate(Isolate* isolate) {
    CompareNilICStub compare_stub(kStrictEquality, kNullValue);
    compare_stub.InitializeInterfaceDescriptor(
        isolate,
        isolate->code_stub_interface_descriptor(CodeStub::CompareNilIC));
  }

  virtual Code::ExtraICState GetExtraICState() {
    return bit_field_;
  }

  EqualityKind GetKind() { return EqualityKindField::decode(bit_field_); }
  NilValue GetNilValue() { return NilValueField::decode(bit_field_); }
  Types GetTypes() { return TypesField::decode(bit_field_); }

  static Types TypesFromExtraICState(
      Code::ExtraICState state) {
    return TypesField::decode(state);
  }
  static EqualityKind EqualityKindFromExtraICState(
      Code::ExtraICState state) {
    return EqualityKindField::decode(state);
  }
  static NilValue NilValueFromExtraICState(Code::ExtraICState state) {
    return NilValueField::decode(state);
  }

  static Types GetPatchedICFlags(Code::ExtraICState extra_ic_state,
                                 Handle<Object> object,
                                 bool* already_monomorphic);

 private:
  friend class CompareNilIC;

  class EqualityKindField : public BitField<EqualityKind, 0, 1> {};
  class NilValueField : public BitField<NilValue, 1, 1> {};
  class TypesField : public BitField<Types, 3, 4> {};

  CompareNilICStub(EqualityKind kind, NilValue nil)
      : HydrogenCodeStub(CODE_STUB_IS_MISS), bit_field_(0) {
    bit_field_ = EqualityKindField::encode(kind) |
        NilValueField::encode(nil);
  }

  virtual CodeStub::Major MajorKey() { return CompareNilIC; }
  virtual int NotMissMinorKey() { return bit_field_; }

  int bit_field_;

  DISALLOW_COPY_AND_ASSIGN(CompareNilICStub);
};


class CEntryStub : public PlatformCodeStub {
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
  static void GenerateAheadOfTime(Isolate* isolate);

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


class JSEntryStub : public PlatformCodeStub {
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


class ArgumentsAccessStub: public PlatformCodeStub {
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


class RegExpExecStub: public PlatformCodeStub {
 public:
  RegExpExecStub() { }

 private:
  Major MajorKey() { return RegExpExec; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class RegExpConstructResultStub: public PlatformCodeStub {
 public:
  RegExpConstructResultStub() { }

 private:
  Major MajorKey() { return RegExpConstructResult; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class CallFunctionStub: public PlatformCodeStub {
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


class CallConstructStub: public PlatformCodeStub {
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

  // Skip handling slow case and directly jump to bailout.
  void SkipSlow(MacroAssembler* masm, Label* bailout) {
    masm->bind(&index_not_smi_);
    masm->bind(&call_runtime_);
    masm->jmp(bailout);
  }

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

  // Skip handling slow case and directly jump to bailout.
  void SkipSlow(MacroAssembler* masm, Label* bailout) {
    masm->bind(&slow_case_);
    masm->jmp(bailout);
  }

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
  void GenerateFast(MacroAssembler* masm) {
    char_code_at_generator_.GenerateFast(masm);
    char_from_code_generator_.GenerateFast(masm);
  }

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm,
                    const RuntimeCallHelper& call_helper) {
    char_code_at_generator_.GenerateSlow(masm, call_helper);
    char_from_code_generator_.GenerateSlow(masm, call_helper);
  }

  // Skip handling slow case and directly jump to bailout.
  void SkipSlow(MacroAssembler* masm, Label* bailout) {
    char_code_at_generator_.SkipSlow(masm, bailout);
    char_from_code_generator_.SkipSlow(masm, bailout);
  }

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


class KeyedLoadDictionaryElementStub : public PlatformCodeStub {
 public:
  KeyedLoadDictionaryElementStub() {}

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return KeyedLoadElement; }
  int MinorKey() { return DICTIONARY_ELEMENTS; }

  DISALLOW_COPY_AND_ASSIGN(KeyedLoadDictionaryElementStub);
};


class KeyedLoadFastElementStub : public HydrogenCodeStub {
 public:
  KeyedLoadFastElementStub(bool is_js_array, ElementsKind elements_kind)
      : HydrogenCodeStub(CODE_STUB_IS_NOT_MISS) {
    bit_field_ = ElementsKindBits::encode(elements_kind) |
        IsJSArrayBits::encode(is_js_array);
  }

  bool is_js_array() const {
    return IsJSArrayBits::decode(bit_field_);
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(bit_field_);
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class IsJSArrayBits: public BitField<bool, 8, 1> {};
  uint32_t bit_field_;

  Major MajorKey() { return KeyedLoadElement; }
  int NotMissMinorKey() { return bit_field_; }

  DISALLOW_COPY_AND_ASSIGN(KeyedLoadFastElementStub);
};


class KeyedStoreFastElementStub : public HydrogenCodeStub {
 public:
  KeyedStoreFastElementStub(bool is_js_array,
                            ElementsKind elements_kind,
                            KeyedAccessStoreMode mode)
      : HydrogenCodeStub(CODE_STUB_IS_NOT_MISS) {
    bit_field_ = ElementsKindBits::encode(elements_kind) |
        IsJSArrayBits::encode(is_js_array) |
        StoreModeBits::encode(mode);
  }

  bool is_js_array() const {
    return IsJSArrayBits::decode(bit_field_);
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(bit_field_);
  }

  KeyedAccessStoreMode store_mode() const {
    return StoreModeBits::decode(bit_field_);
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  class ElementsKindBits: public BitField<ElementsKind,      0, 8> {};
  class StoreModeBits: public BitField<KeyedAccessStoreMode, 8, 4> {};
  class IsJSArrayBits: public BitField<bool,                12, 1> {};
  uint32_t bit_field_;

  Major MajorKey() { return KeyedStoreElement; }
  int NotMissMinorKey() { return bit_field_; }

  DISALLOW_COPY_AND_ASSIGN(KeyedStoreFastElementStub);
};


class TransitionElementsKindStub : public HydrogenCodeStub {
 public:
  TransitionElementsKindStub(ElementsKind from_kind,
                             ElementsKind to_kind)
      : HydrogenCodeStub(CODE_STUB_IS_NOT_MISS) {
    bit_field_ = FromKindBits::encode(from_kind) |
        ToKindBits::encode(to_kind);
  }

  ElementsKind from_kind() const {
    return FromKindBits::decode(bit_field_);
  }

  ElementsKind to_kind() const {
    return ToKindBits::decode(bit_field_);
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  class FromKindBits: public BitField<ElementsKind, 8, 8> {};
  class ToKindBits: public BitField<ElementsKind, 0, 8> {};
  uint32_t bit_field_;

  Major MajorKey() { return TransitionElementsKind; }
  int NotMissMinorKey() { return bit_field_; }

  DISALLOW_COPY_AND_ASSIGN(TransitionElementsKindStub);
};


class ArrayConstructorStubBase : public HydrogenCodeStub {
 public:
  ArrayConstructorStubBase(ElementsKind kind, AllocationSiteMode mode)
      : HydrogenCodeStub(CODE_STUB_IS_NOT_MISS) {
    bit_field_ = ElementsKindBits::encode(kind) |
        AllocationSiteModeBits::encode(mode == TRACK_ALLOCATION_SITE);
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(bit_field_);
  }

  AllocationSiteMode mode() const {
    return AllocationSiteModeBits::decode(bit_field_)
        ? TRACK_ALLOCATION_SITE
        : DONT_TRACK_ALLOCATION_SITE;
  }

  virtual bool IsPregenerated() { return true; }
  static void GenerateStubsAheadOfTime(Isolate* isolate);
  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kPropertyCell = 0;

 private:
  int NotMissMinorKey() { return bit_field_; }

  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class AllocationSiteModeBits: public BitField<bool, 8, 1> {};
  uint32_t bit_field_;

  DISALLOW_COPY_AND_ASSIGN(ArrayConstructorStubBase);
};


class ArrayNoArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNoArgumentConstructorStub(
      ElementsKind kind,
      AllocationSiteMode mode = TRACK_ALLOCATION_SITE)
      : ArrayConstructorStubBase(kind, mode) {
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  Major MajorKey() { return ArrayNoArgumentConstructor; }

  DISALLOW_COPY_AND_ASSIGN(ArrayNoArgumentConstructorStub);
};


class ArraySingleArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArraySingleArgumentConstructorStub(
      ElementsKind kind,
      AllocationSiteMode mode = TRACK_ALLOCATION_SITE)
      : ArrayConstructorStubBase(kind, mode) {
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  Major MajorKey() { return ArraySingleArgumentConstructor; }

  DISALLOW_COPY_AND_ASSIGN(ArraySingleArgumentConstructorStub);
};


class ArrayNArgumentsConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNArgumentsConstructorStub(
      ElementsKind kind,
      AllocationSiteMode mode = TRACK_ALLOCATION_SITE) :
    ArrayConstructorStubBase(kind, mode) {
  }

  virtual Handle<Code> GenerateCode();

  virtual void InitializeInterfaceDescriptor(
      Isolate* isolate,
      CodeStubInterfaceDescriptor* descriptor);

 private:
  Major MajorKey() { return ArrayNArgumentsConstructor; }

  DISALLOW_COPY_AND_ASSIGN(ArrayNArgumentsConstructorStub);
};


class KeyedStoreElementStub : public PlatformCodeStub {
 public:
  KeyedStoreElementStub(bool is_js_array,
                        ElementsKind elements_kind,
                        KeyedAccessStoreMode store_mode)
      : is_js_array_(is_js_array),
        elements_kind_(elements_kind),
        store_mode_(store_mode),
        fp_registers_(CanUseFPRegisters()) { }

  Major MajorKey() { return KeyedStoreElement; }
  int MinorKey() {
    return ElementsKindBits::encode(elements_kind_) |
        IsJSArrayBits::encode(is_js_array_) |
        StoreModeBits::encode(store_mode_) |
        FPRegisters::encode(fp_registers_);
  }

  void Generate(MacroAssembler* masm);

 private:
  class ElementsKindBits: public BitField<ElementsKind,      0, 8> {};
  class StoreModeBits: public BitField<KeyedAccessStoreMode, 8, 4> {};
  class IsJSArrayBits: public BitField<bool,                12, 1> {};
  class FPRegisters: public BitField<bool,                  13, 1> {};

  bool is_js_array_;
  ElementsKind elements_kind_;
  KeyedAccessStoreMode store_mode_;
  bool fp_registers_;

  DISALLOW_COPY_AND_ASSIGN(KeyedStoreElementStub);
};


class ToBooleanStub: public PlatformCodeStub {
 public:
  enum Type {
    UNDEFINED,
    BOOLEAN,
    NULL_TYPE,
    SMI,
    SPEC_OBJECT,
    STRING,
    SYMBOL,
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
    bool ContainsAnyOf(Types types) const {
      return set_.ContainsAnyOf(types.set_);
    }
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
  virtual Code::Kind GetCodeKind() const { return Code::TO_BOOLEAN_IC; }
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


class ElementsTransitionAndStoreStub : public PlatformCodeStub {
 public:
  ElementsTransitionAndStoreStub(ElementsKind from,
                                 ElementsKind to,
                                 bool is_jsarray,
                                 StrictModeFlag strict_mode,
                                 KeyedAccessStoreMode store_mode)
      : from_(from),
        to_(to),
        is_jsarray_(is_jsarray),
        strict_mode_(strict_mode),
        store_mode_(store_mode) {}

 private:
  class FromBits:       public BitField<ElementsKind,        0, 8> {};
  class ToBits:         public BitField<ElementsKind,        8, 8> {};
  class IsJSArrayBits:  public BitField<bool,                16, 1> {};
  class StrictModeBits: public BitField<StrictModeFlag,      17, 1> {};
  class StoreModeBits: public BitField<KeyedAccessStoreMode, 18, 4> {};

  Major MajorKey() { return ElementsTransitionAndStore; }
  int MinorKey() {
    return FromBits::encode(from_) |
        ToBits::encode(to_) |
        IsJSArrayBits::encode(is_jsarray_) |
        StrictModeBits::encode(strict_mode_) |
        StoreModeBits::encode(store_mode_);
  }

  void Generate(MacroAssembler* masm);

  ElementsKind from_;
  ElementsKind to_;
  bool is_jsarray_;
  StrictModeFlag strict_mode_;
  KeyedAccessStoreMode store_mode_;

  DISALLOW_COPY_AND_ASSIGN(ElementsTransitionAndStoreStub);
};


class StoreArrayLiteralElementStub : public PlatformCodeStub {
 public:
  StoreArrayLiteralElementStub()
        : fp_registers_(CanUseFPRegisters()) { }

 private:
  class FPRegisters: public BitField<bool,                0, 1> {};

  Major MajorKey() { return StoreArrayLiteralElement; }
  int MinorKey() { return FPRegisters::encode(fp_registers_); }

  void Generate(MacroAssembler* masm);

  bool fp_registers_;

  DISALLOW_COPY_AND_ASSIGN(StoreArrayLiteralElementStub);
};


class StubFailureTrampolineStub : public PlatformCodeStub {
 public:
  explicit StubFailureTrampolineStub(StubFunctionMode function_mode)
      : fp_registers_(CanUseFPRegisters()), function_mode_(function_mode) {}

  virtual bool IsPregenerated() { return true; }

  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  class FPRegisters:       public BitField<bool,                0, 1> {};
  class FunctionModeField: public BitField<StubFunctionMode,    1, 1> {};

  Major MajorKey() { return StubFailureTrampoline; }
  int MinorKey() {
    return FPRegisters::encode(fp_registers_) |
        FunctionModeField::encode(function_mode_);
  }

  void Generate(MacroAssembler* masm);

  bool fp_registers_;
  StubFunctionMode function_mode_;

  DISALLOW_COPY_AND_ASSIGN(StubFailureTrampolineStub);
};


class ProfileEntryHookStub : public PlatformCodeStub {
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

  static bool HasEntryHook() { return entry_hook_ != NULL; }

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

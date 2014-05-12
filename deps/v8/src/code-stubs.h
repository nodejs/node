// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "allocation.h"
#include "assembler.h"
#include "codegen.h"
#include "globals.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

// List of code stubs used on all platforms.
#define CODE_STUB_LIST_ALL_PLATFORMS(V)  \
  V(CallFunction)                        \
  V(CallConstruct)                       \
  V(BinaryOpIC)                          \
  V(BinaryOpICWithAllocationSite)        \
  V(BinaryOpWithAllocationSite)          \
  V(StringAdd)                           \
  V(SubString)                           \
  V(StringCompare)                       \
  V(Compare)                             \
  V(CompareIC)                           \
  V(CompareNilIC)                        \
  V(MathPow)                             \
  V(CallIC)                              \
  V(FunctionPrototype)                   \
  V(RecordWrite)                         \
  V(StoreBufferOverflow)                 \
  V(RegExpExec)                          \
  V(Instanceof)                          \
  V(ConvertToDouble)                     \
  V(WriteInt32ToHeapNumber)              \
  V(StackCheck)                          \
  V(Interrupt)                           \
  V(FastNewClosure)                      \
  V(FastNewContext)                      \
  V(FastCloneShallowArray)               \
  V(FastCloneShallowObject)              \
  V(CreateAllocationSite)                \
  V(ToBoolean)                           \
  V(ToNumber)                            \
  V(ArgumentsAccess)                     \
  V(RegExpConstructResult)               \
  V(NumberToString)                      \
  V(DoubleToI)                           \
  V(CEntry)                              \
  V(JSEntry)                             \
  V(KeyedLoadElement)                    \
  V(ArrayNoArgumentConstructor)          \
  V(ArraySingleArgumentConstructor)      \
  V(ArrayNArgumentsConstructor)          \
  V(InternalArrayNoArgumentConstructor)  \
  V(InternalArraySingleArgumentConstructor)      \
  V(InternalArrayNArgumentsConstructor)  \
  V(KeyedStoreElement)                   \
  V(DebuggerStatement)                   \
  V(NameDictionaryLookup)                \
  V(ElementsTransitionAndStore)          \
  V(TransitionElementsKind)              \
  V(StoreArrayLiteralElement)            \
  V(StubFailureTrampoline)               \
  V(ArrayConstructor)                    \
  V(InternalArrayConstructor)            \
  V(ProfileEntryHook)                    \
  V(StoreGlobal)                         \
  V(CallApiFunction)                     \
  V(CallApiGetter)                       \
  /* IC Handler stubs */                 \
  V(LoadField)                           \
  V(KeyedLoadField)                      \
  V(StringLength)                        \
  V(KeyedStringLength)

// List of code stubs only used on ARM 32 bits platforms.
#if V8_TARGET_ARCH_ARM
#define CODE_STUB_LIST_ARM(V)  \
  V(GetProperty)               \
  V(SetProperty)               \
  V(InvokeBuiltin)             \
  V(DirectCEntry)
#else
#define CODE_STUB_LIST_ARM(V)
#endif

// List of code stubs only used on ARM 64 bits platforms.
#if V8_TARGET_ARCH_ARM64
#define CODE_STUB_LIST_ARM64(V)  \
  V(GetProperty)               \
  V(SetProperty)               \
  V(InvokeBuiltin)             \
  V(DirectCEntry)              \
  V(StoreRegistersState)       \
  V(RestoreRegistersState)
#else
#define CODE_STUB_LIST_ARM64(V)
#endif

// List of code stubs only used on MIPS platforms.
#if V8_TARGET_ARCH_MIPS
#define CODE_STUB_LIST_MIPS(V)  \
  V(RegExpCEntry)               \
  V(DirectCEntry)               \
  V(StoreRegistersState)        \
  V(RestoreRegistersState)
#else
#define CODE_STUB_LIST_MIPS(V)
#endif

// Combined list of code stubs.
#define CODE_STUB_LIST(V)            \
  CODE_STUB_LIST_ALL_PLATFORMS(V)    \
  CODE_STUB_LIST_ARM(V)              \
  CODE_STUB_LIST_ARM64(V)           \
  CODE_STUB_LIST_MIPS(V)

// Stub is base classes of all stubs.
class CodeStub BASE_EMBEDDED {
 public:
  enum Major {
    UninitializedMajorKey = 0,
#define DEF_ENUM(name) name,
    CODE_STUB_LIST(DEF_ENUM)
#undef DEF_ENUM
    NoCache,  // marker for stubs that do custom caching
    NUMBER_OF_IDS
  };

  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GetCode();

  // Retrieve the code for the stub, make and return a copy of the code.
  Handle<Code> GetCodeCopy(const Code::FindAndReplacePattern& pattern);

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

  explicit CodeStub(Isolate* isolate) : isolate_(isolate) { }
  virtual ~CodeStub() {}

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
  bool FindCodeInCache(Code** code_out);

  // Returns information for computing the number key.
  virtual Major MajorKey() = 0;
  virtual int MinorKey() = 0;

  virtual InlineCacheState GetICState() {
    return UNINITIALIZED;
  }
  virtual ExtraICState GetExtraICState() {
    return kNoExtraICState;
  }
  virtual Code::StubType GetStubType() {
    return Code::NORMAL;
  }

  virtual void PrintName(StringStream* stream);

  // Returns a name for logging/debugging purposes.
  SmartArrayPointer<const char> GetName();

  Isolate* isolate() const { return isolate_; }

 protected:
  static bool CanUseFPRegisters();

  // Generates the assembler code for the stub.
  virtual Handle<Code> GenerateCode() = 0;

  virtual void VerifyPlatformFeatures();

  // Returns whether the code generated for this stub needs to be allocated as
  // a fixed (non-moveable) code object.
  virtual bool NeedsImmovableCode() { return false; }

  virtual void PrintBaseName(StringStream* stream);
  virtual void PrintState(StringStream* stream) { }

 private:
  // Perform bookkeeping required after code generation when stub code is
  // initially generated.
  void RecordCodeGeneration(Handle<Code> code);

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
  virtual bool FindCodeInSpecialCache(Code** code_out) {
    return false;
  }

  // If a stub uses a special cache override this.
  virtual bool UseSpecialCache() { return false; }

  // Computes the key based on major and minor.
  uint32_t GetKey() {
    ASSERT(static_cast<int>(MajorKey()) < NUMBER_OF_IDS);
    return MinorKeyBits::encode(MinorKey()) |
           MajorKeyBits::encode(MajorKey());
  }

  STATIC_ASSERT(NUMBER_OF_IDS < (1 << kStubMajorKeyBits));
  class MajorKeyBits: public BitField<uint32_t, 0, kStubMajorKeyBits> {};
  class MinorKeyBits: public BitField<uint32_t,
      kStubMajorKeyBits, kStubMinorKeyBits> {};  // NOLINT

  friend class BreakPointIterator;

  Isolate* isolate_;
};


class PlatformCodeStub : public CodeStub {
 public:
  explicit PlatformCodeStub(Isolate* isolate) : CodeStub(isolate) { }

  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }

 protected:
  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;
};


enum StubFunctionMode { NOT_JS_FUNCTION_STUB_MODE, JS_FUNCTION_STUB_MODE };
enum HandlerArgumentsMode { DONT_PASS_ARGUMENTS, PASS_ARGUMENTS };

struct CodeStubInterfaceDescriptor {
  CodeStubInterfaceDescriptor();
  int register_param_count_;

  Register stack_parameter_count_;
  // if hint_stack_parameter_count_ > 0, the code stub can optimize the
  // return sequence. Default value is -1, which means it is ignored.
  int hint_stack_parameter_count_;
  StubFunctionMode function_mode_;
  Register* register_params_;

  Address deoptimization_handler_;
  HandlerArgumentsMode handler_arguments_mode_;

  bool initialized() const { return register_param_count_ >= 0; }

  int environment_length() const {
    return register_param_count_;
  }

  void SetMissHandler(ExternalReference handler) {
    miss_handler_ = handler;
    has_miss_handler_ = true;
    // Our miss handler infrastructure doesn't currently support
    // variable stack parameter counts.
    ASSERT(!stack_parameter_count_.is_valid());
  }

  ExternalReference miss_handler() {
    ASSERT(has_miss_handler_);
    return miss_handler_;
  }

  bool has_miss_handler() {
    return has_miss_handler_;
  }

  Register GetParameterRegister(int index) const {
    return register_params_[index];
  }

  bool IsParameterCountRegister(int index) {
    return GetParameterRegister(index).is(stack_parameter_count_);
  }

  int GetHandlerParameterCount() {
    int params = environment_length();
    if (handler_arguments_mode_ == PASS_ARGUMENTS) {
      params += 1;
    }
    return params;
  }

 private:
  ExternalReference miss_handler_;
  bool has_miss_handler_;
};


struct PlatformCallInterfaceDescriptor;


struct CallInterfaceDescriptor {
  CallInterfaceDescriptor()
      : register_param_count_(-1),
        register_params_(NULL),
        param_representations_(NULL),
        platform_specific_descriptor_(NULL) { }

  bool initialized() const { return register_param_count_ >= 0; }

  int environment_length() const {
    return register_param_count_;
  }

  Representation GetParameterRepresentation(int index) const {
    return param_representations_[index];
  }

  Register GetParameterRegister(int index) const {
    return register_params_[index];
  }

  PlatformCallInterfaceDescriptor* platform_specific_descriptor() const {
    return platform_specific_descriptor_;
  }

  int register_param_count_;
  Register* register_params_;
  Representation* param_representations_;
  PlatformCallInterfaceDescriptor* platform_specific_descriptor_;
};


class HydrogenCodeStub : public CodeStub {
 public:
  enum InitializationState {
    UNINITIALIZED,
    INITIALIZED
  };

  HydrogenCodeStub(Isolate* isolate, InitializationState state = INITIALIZED)
      : CodeStub(isolate) {
    is_uninitialized_ = (state == UNINITIALIZED);
  }

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }

  CodeStubInterfaceDescriptor* GetInterfaceDescriptor() {
    return isolate()->code_stub_interface_descriptor(MajorKey());
  }

  bool IsUninitialized() { return is_uninitialized_; }

  template<class SubClass>
  static Handle<Code> GetUninitialized(Isolate* isolate) {
    SubClass::GenerateAheadOfTime(isolate);
    return SubClass().GetCode(isolate);
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) = 0;

  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode() = 0;

  virtual int NotMissMinorKey() = 0;

  Handle<Code> GenerateLightweightMissCode();

  template<class StateType>
  void TraceTransition(StateType from, StateType to);

 private:
  class MinorKeyBits: public BitField<int, 0, kStubMinorKeyBits - 1> {};
  class IsMissBits: public BitField<bool, kStubMinorKeyBits - 1, 1> {};

  void GenerateLightweightMiss(MacroAssembler* masm);
  virtual int MinorKey() {
    return IsMissBits::encode(is_uninitialized_) |
        MinorKeyBits::encode(NotMissMinorKey());
  }

  bool is_uninitialized_;
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
#elif V8_TARGET_ARCH_ARM64
#include "arm64/code-stubs-arm64.h"
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


class ToNumberStub: public HydrogenCodeStub {
 public:
  explicit ToNumberStub(Isolate* isolate) : HydrogenCodeStub(isolate) { }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate) {
    ToNumberStub stub(isolate);
    stub.InitializeInterfaceDescriptor(
        isolate->code_stub_interface_descriptor(CodeStub::ToNumber));
  }

 private:
  Major MajorKey() { return ToNumber; }
  int NotMissMinorKey() { return 0; }
};


class NumberToStringStub V8_FINAL : public HydrogenCodeStub {
 public:
  explicit NumberToStringStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kNumber = 0;

 private:
  virtual Major MajorKey() V8_OVERRIDE { return NumberToString; }
  virtual int NotMissMinorKey() V8_OVERRIDE { return 0; }
};


class FastNewClosureStub : public HydrogenCodeStub {
 public:
  FastNewClosureStub(Isolate* isolate,
                     StrictMode strict_mode,
                     bool is_generator)
      : HydrogenCodeStub(isolate),
        strict_mode_(strict_mode),
        is_generator_(is_generator) { }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  StrictMode strict_mode() const { return strict_mode_; }
  bool is_generator() const { return is_generator_; }

 private:
  class StrictModeBits: public BitField<bool, 0, 1> {};
  class IsGeneratorBits: public BitField<bool, 1, 1> {};

  Major MajorKey() { return FastNewClosure; }
  int NotMissMinorKey() {
    return StrictModeBits::encode(strict_mode_ == STRICT) |
      IsGeneratorBits::encode(is_generator_);
  }

  StrictMode strict_mode_;
  bool is_generator_;
};


class FastNewContextStub V8_FINAL : public HydrogenCodeStub {
 public:
  static const int kMaximumSlots = 64;

  FastNewContextStub(Isolate* isolate, int slots)
      : HydrogenCodeStub(isolate), slots_(slots) {
    ASSERT(slots_ > 0 && slots_ <= kMaximumSlots);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  int slots() const { return slots_; }

  virtual Major MajorKey() V8_OVERRIDE { return FastNewContext; }
  virtual int NotMissMinorKey() V8_OVERRIDE { return slots_; }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kFunction = 0;

 private:
  int slots_;
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

  FastCloneShallowArrayStub(Isolate* isolate,
                            Mode mode,
                            AllocationSiteMode allocation_site_mode,
                            int length)
      : HydrogenCodeStub(isolate),
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

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

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

  FastCloneShallowObjectStub(Isolate* isolate, int length)
      : HydrogenCodeStub(isolate), length_(length) {
    ASSERT_GE(length_, 0);
    ASSERT_LE(length_, kMaximumClonedProperties);
  }

  int length() const { return length_; }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  int length_;

  Major MajorKey() { return FastCloneShallowObject; }
  int NotMissMinorKey() { return length_; }

  DISALLOW_COPY_AND_ASSIGN(FastCloneShallowObjectStub);
};


class CreateAllocationSiteStub : public HydrogenCodeStub {
 public:
  explicit CreateAllocationSiteStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) { }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  static void GenerateAheadOfTime(Isolate* isolate);

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return CreateAllocationSite; }
  int NotMissMinorKey() { return 0; }

  DISALLOW_COPY_AND_ASSIGN(CreateAllocationSiteStub);
};


class InstanceofStub: public PlatformCodeStub {
 public:
  enum Flags {
    kNoFlags = 0,
    kArgsInRegisters = 1 << 0,
    kCallSiteInlineCheck = 1 << 1,
    kReturnTrueFalseObject = 1 << 2
  };

  InstanceofStub(Isolate* isolate, Flags flags)
      : PlatformCodeStub(isolate), flags_(flags) { }

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


enum AllocationSiteOverrideMode {
  DONT_OVERRIDE,
  DISABLE_ALLOCATION_SITES,
  LAST_ALLOCATION_SITE_OVERRIDE_MODE = DISABLE_ALLOCATION_SITES
};


class ArrayConstructorStub: public PlatformCodeStub {
 public:
  enum ArgumentCountKey { ANY, NONE, ONE, MORE_THAN_ONE };
  ArrayConstructorStub(Isolate* isolate, int argument_count);
  explicit ArrayConstructorStub(Isolate* isolate);

  void Generate(MacroAssembler* masm);

 private:
  void GenerateDispatchToArrayStub(MacroAssembler* masm,
                                   AllocationSiteOverrideMode mode);
  virtual void PrintName(StringStream* stream);

  virtual CodeStub::Major MajorKey() { return ArrayConstructor; }
  virtual int MinorKey() { return argument_count_; }

  ArgumentCountKey argument_count_;
};


class InternalArrayConstructorStub: public PlatformCodeStub {
 public:
  explicit InternalArrayConstructorStub(Isolate* isolate);

  void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return InternalArrayConstructor; }
  virtual int MinorKey() { return 0; }

  void GenerateCase(MacroAssembler* masm, ElementsKind kind);
};


class MathPowStub: public PlatformCodeStub {
 public:
  enum ExponentType { INTEGER, DOUBLE, TAGGED, ON_STACK };

  MathPowStub(Isolate* isolate, ExponentType exponent_type)
      : PlatformCodeStub(isolate), exponent_type_(exponent_type) { }
  virtual void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return MathPow; }
  virtual int MinorKey() { return exponent_type_; }

  ExponentType exponent_type_;
};


class ICStub: public PlatformCodeStub {
 public:
  ICStub(Isolate* isolate, Code::Kind kind)
      : PlatformCodeStub(isolate), kind_(kind) { }
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


class CallICStub: public PlatformCodeStub {
 public:
  CallICStub(Isolate* isolate, const CallIC::State& state)
      : PlatformCodeStub(isolate), state_(state) {}

  bool CallAsMethod() const { return state_.CallAsMethod(); }

  int arg_count() const { return state_.arg_count(); }

  static int ExtractArgcFromMinorKey(int minor_key) {
    CallIC::State state((ExtraICState) minor_key);
    return state.arg_count();
  }

  virtual void Generate(MacroAssembler* masm);

  virtual Code::Kind GetCodeKind() const V8_OVERRIDE {
    return Code::CALL_IC;
  }

  virtual InlineCacheState GetICState() V8_FINAL V8_OVERRIDE {
    return state_.GetICState();
  }

  virtual ExtraICState GetExtraICState() V8_FINAL V8_OVERRIDE {
    return state_.GetExtraICState();
  }

 protected:
  virtual int MinorKey() { return GetExtraICState(); }
  virtual void PrintState(StringStream* stream) V8_FINAL V8_OVERRIDE;

 private:
  virtual CodeStub::Major MajorKey() { return CallIC; }

  // Code generation helpers.
  void GenerateMiss(MacroAssembler* masm);

  CallIC::State state_;
};


class FunctionPrototypeStub: public ICStub {
 public:
  FunctionPrototypeStub(Isolate* isolate, Code::Kind kind)
      : ICStub(isolate, kind) { }
  virtual void Generate(MacroAssembler* masm);

 private:
  virtual CodeStub::Major MajorKey() { return FunctionPrototype; }
};


class StoreICStub: public ICStub {
 public:
  StoreICStub(Isolate* isolate, Code::Kind kind, StrictMode strict_mode)
      : ICStub(isolate, kind), strict_mode_(strict_mode) { }

 protected:
  virtual ExtraICState GetExtraICState() {
    return StoreIC::ComputeExtraICState(strict_mode_);
  }

 private:
  STATIC_ASSERT(KindBits::kSize == 4);
  class StrictModeBits: public BitField<bool, 4, 1> {};
  virtual int MinorKey() {
    return KindBits::encode(kind()) | StrictModeBits::encode(strict_mode_);
  }

  StrictMode strict_mode_;
};


class HICStub: public HydrogenCodeStub {
 public:
  explicit HICStub(Isolate* isolate) : HydrogenCodeStub(isolate) { }
  virtual Code::Kind GetCodeKind() const { return kind(); }
  virtual InlineCacheState GetICState() { return MONOMORPHIC; }

 protected:
  class KindBits: public BitField<Code::Kind, 0, 4> {};
  virtual Code::Kind kind() const = 0;
};


class HandlerStub: public HICStub {
 public:
  virtual Code::Kind GetCodeKind() const { return Code::HANDLER; }
  virtual ExtraICState GetExtraICState() { return kind(); }

 protected:
  explicit HandlerStub(Isolate* isolate) : HICStub(isolate) { }
  virtual int NotMissMinorKey() { return bit_field_; }
  int bit_field_;
};


class LoadFieldStub: public HandlerStub {
 public:
  LoadFieldStub(Isolate* isolate,
                bool inobject,
                int index, Representation representation)
      : HandlerStub(isolate) {
    Initialize(Code::LOAD_IC, inobject, index, representation);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  Representation representation() {
    if (unboxed_double()) return Representation::Double();
    return Representation::Tagged();
  }

  virtual Code::Kind kind() const {
    return KindBits::decode(bit_field_);
  }

  bool is_inobject() {
    return InobjectBits::decode(bit_field_);
  }

  int offset() {
    int index = IndexBits::decode(bit_field_);
    int offset = index * kPointerSize;
    if (is_inobject()) return offset;
    return FixedArray::kHeaderSize + offset;
  }

  bool unboxed_double() {
    return UnboxedDoubleBits::decode(bit_field_);
  }

  virtual Code::StubType GetStubType() { return Code::FAST; }

 protected:
  explicit LoadFieldStub(Isolate* isolate) : HandlerStub(isolate) { }

  void Initialize(Code::Kind kind,
                  bool inobject,
                  int index,
                  Representation representation) {
    bit_field_ = KindBits::encode(kind)
        | InobjectBits::encode(inobject)
        | IndexBits::encode(index)
        | UnboxedDoubleBits::encode(representation.IsDouble());
  }

 private:
  STATIC_ASSERT(KindBits::kSize == 4);
  class InobjectBits: public BitField<bool, 4, 1> {};
  class IndexBits: public BitField<int, 5, 11> {};
  class UnboxedDoubleBits: public BitField<bool, 16, 1> {};
  virtual CodeStub::Major MajorKey() { return LoadField; }
};


class StringLengthStub: public HandlerStub {
 public:
  explicit StringLengthStub(Isolate* isolate) : HandlerStub(isolate) {
    Initialize(Code::LOAD_IC);
  }
  virtual Handle<Code> GenerateCode() V8_OVERRIDE;
  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 protected:
  virtual Code::Kind kind() const {
    return KindBits::decode(bit_field_);
  }

  void Initialize(Code::Kind kind) {
    bit_field_ = KindBits::encode(kind);
  }

 private:
  virtual CodeStub::Major MajorKey() { return StringLength; }
};


class KeyedStringLengthStub: public StringLengthStub {
 public:
  explicit KeyedStringLengthStub(Isolate* isolate) : StringLengthStub(isolate) {
    Initialize(Code::KEYED_LOAD_IC);
  }
  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  virtual CodeStub::Major MajorKey() { return KeyedStringLength; }
};


class StoreGlobalStub : public HandlerStub {
 public:
  StoreGlobalStub(Isolate* isolate, bool is_constant, bool check_global)
      : HandlerStub(isolate) {
    bit_field_ = IsConstantBits::encode(is_constant) |
        CheckGlobalBits::encode(check_global);
  }

  static Handle<HeapObject> global_placeholder(Isolate* isolate) {
    return isolate->factory()->uninitialized_value();
  }

  Handle<Code> GetCodeCopyFromTemplate(Handle<GlobalObject> global,
                                       Handle<PropertyCell> cell) {
    if (check_global()) {
      Code::FindAndReplacePattern pattern;
      pattern.Add(Handle<Map>(global_placeholder(isolate())->map()), global);
      pattern.Add(isolate()->factory()->meta_map(), Handle<Map>(global->map()));
      pattern.Add(isolate()->factory()->global_property_cell_map(), cell);
      return CodeStub::GetCodeCopy(pattern);
    } else {
      Code::FindAndReplacePattern pattern;
      pattern.Add(isolate()->factory()->global_property_cell_map(), cell);
      return CodeStub::GetCodeCopy(pattern);
    }
  }

  virtual Code::Kind kind() const { return Code::STORE_IC; }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  bool is_constant() const {
    return IsConstantBits::decode(bit_field_);
  }
  bool check_global() const {
    return CheckGlobalBits::decode(bit_field_);
  }
  void set_is_constant(bool value) {
    bit_field_ = IsConstantBits::update(bit_field_, value);
  }

  Representation representation() {
    return Representation::FromKind(RepresentationBits::decode(bit_field_));
  }
  void set_representation(Representation r) {
    bit_field_ = RepresentationBits::update(bit_field_, r.kind());
  }

 private:
  Major MajorKey() { return StoreGlobal; }

  class IsConstantBits: public BitField<bool, 0, 1> {};
  class RepresentationBits: public BitField<Representation::Kind, 1, 8> {};
  class CheckGlobalBits: public BitField<bool, 9, 1> {};

  DISALLOW_COPY_AND_ASSIGN(StoreGlobalStub);
};


class CallApiFunctionStub : public PlatformCodeStub {
 public:
  CallApiFunctionStub(Isolate* isolate,
                      bool is_store,
                      bool call_data_undefined,
                      int argc) : PlatformCodeStub(isolate) {
    bit_field_ =
        IsStoreBits::encode(is_store) |
        CallDataUndefinedBits::encode(call_data_undefined) |
        ArgumentBits::encode(argc);
    ASSERT(!is_store || argc == 1);
  }

 private:
  virtual void Generate(MacroAssembler* masm) V8_OVERRIDE;
  virtual Major MajorKey() V8_OVERRIDE { return CallApiFunction; }
  virtual int MinorKey() V8_OVERRIDE { return bit_field_; }

  class IsStoreBits: public BitField<bool, 0, 1> {};
  class CallDataUndefinedBits: public BitField<bool, 1, 1> {};
  class ArgumentBits: public BitField<int, 2, Code::kArgumentsBits> {};

  int bit_field_;

  DISALLOW_COPY_AND_ASSIGN(CallApiFunctionStub);
};


class CallApiGetterStub : public PlatformCodeStub {
 public:
  explicit CallApiGetterStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

 private:
  virtual void Generate(MacroAssembler* masm) V8_OVERRIDE;
  virtual Major MajorKey() V8_OVERRIDE { return CallApiGetter; }
  virtual int MinorKey() V8_OVERRIDE { return 0; }

  DISALLOW_COPY_AND_ASSIGN(CallApiGetterStub);
};


class KeyedLoadFieldStub: public LoadFieldStub {
 public:
  KeyedLoadFieldStub(Isolate* isolate,
                     bool inobject,
                     int index, Representation representation)
      : LoadFieldStub(isolate) {
    Initialize(Code::KEYED_LOAD_IC, inobject, index, representation);
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  virtual CodeStub::Major MajorKey() { return KeyedLoadField; }
};


class BinaryOpICStub : public HydrogenCodeStub {
 public:
  BinaryOpICStub(Isolate* isolate, Token::Value op, OverwriteMode mode)
      : HydrogenCodeStub(isolate, UNINITIALIZED), state_(isolate, op, mode) {}

  BinaryOpICStub(Isolate* isolate, const BinaryOpIC::State& state)
      : HydrogenCodeStub(isolate), state_(state) {}

  static void GenerateAheadOfTime(Isolate* isolate);

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const V8_OVERRIDE {
    return Code::BINARY_OP_IC;
  }

  virtual InlineCacheState GetICState() V8_FINAL V8_OVERRIDE {
    return state_.GetICState();
  }

  virtual ExtraICState GetExtraICState() V8_FINAL V8_OVERRIDE {
    return state_.GetExtraICState();
  }

  virtual void VerifyPlatformFeatures() V8_FINAL V8_OVERRIDE {
    ASSERT(CpuFeatures::VerifyCrossCompiling(SSE2));
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  const BinaryOpIC::State& state() const { return state_; }

  virtual void PrintState(StringStream* stream) V8_FINAL V8_OVERRIDE;

  virtual Major MajorKey() V8_OVERRIDE { return BinaryOpIC; }
  virtual int NotMissMinorKey() V8_FINAL V8_OVERRIDE {
    return GetExtraICState();
  }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLeft = 0;
  static const int kRight = 1;

 private:
  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpIC::State& state);

  BinaryOpIC::State state_;

  DISALLOW_COPY_AND_ASSIGN(BinaryOpICStub);
};


// TODO(bmeurer): Merge this into the BinaryOpICStub once we have proper tail
// call support for stubs in Hydrogen.
class BinaryOpICWithAllocationSiteStub V8_FINAL : public PlatformCodeStub {
 public:
  BinaryOpICWithAllocationSiteStub(Isolate* isolate,
                                   const BinaryOpIC::State& state)
      : PlatformCodeStub(isolate), state_(state) {}

  static void GenerateAheadOfTime(Isolate* isolate);

  Handle<Code> GetCodeCopyFromTemplate(Handle<AllocationSite> allocation_site) {
    Code::FindAndReplacePattern pattern;
    pattern.Add(isolate()->factory()->undefined_map(), allocation_site);
    return CodeStub::GetCodeCopy(pattern);
  }

  virtual Code::Kind GetCodeKind() const V8_OVERRIDE {
    return Code::BINARY_OP_IC;
  }

  virtual InlineCacheState GetICState() V8_OVERRIDE {
    return state_.GetICState();
  }

  virtual ExtraICState GetExtraICState() V8_OVERRIDE {
    return state_.GetExtraICState();
  }

  virtual void VerifyPlatformFeatures() V8_OVERRIDE {
    ASSERT(CpuFeatures::VerifyCrossCompiling(SSE2));
  }

  virtual void Generate(MacroAssembler* masm) V8_OVERRIDE;

  virtual void PrintState(StringStream* stream) V8_OVERRIDE;

  virtual Major MajorKey() V8_OVERRIDE { return BinaryOpICWithAllocationSite; }
  virtual int MinorKey() V8_OVERRIDE { return GetExtraICState(); }

 private:
  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpIC::State& state);

  BinaryOpIC::State state_;

  DISALLOW_COPY_AND_ASSIGN(BinaryOpICWithAllocationSiteStub);
};


class BinaryOpWithAllocationSiteStub V8_FINAL : public BinaryOpICStub {
 public:
  BinaryOpWithAllocationSiteStub(Isolate* isolate,
                                 Token::Value op,
                                 OverwriteMode mode)
      : BinaryOpICStub(isolate, op, mode) {}

  BinaryOpWithAllocationSiteStub(Isolate* isolate,
                                 const BinaryOpIC::State& state)
      : BinaryOpICStub(isolate, state) {}

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const V8_FINAL V8_OVERRIDE {
    return Code::STUB;
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual Major MajorKey() V8_OVERRIDE {
    return BinaryOpWithAllocationSite;
  }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kAllocationSite = 0;
  static const int kLeft = 1;
  static const int kRight = 2;
};


enum StringAddFlags {
  // Omit both parameter checks.
  STRING_ADD_CHECK_NONE = 0,
  // Check left parameter.
  STRING_ADD_CHECK_LEFT = 1 << 0,
  // Check right parameter.
  STRING_ADD_CHECK_RIGHT = 1 << 1,
  // Check both parameters.
  STRING_ADD_CHECK_BOTH = STRING_ADD_CHECK_LEFT | STRING_ADD_CHECK_RIGHT
};


class StringAddStub V8_FINAL : public HydrogenCodeStub {
 public:
  StringAddStub(Isolate* isolate,
                StringAddFlags flags,
                PretenureFlag pretenure_flag)
      : HydrogenCodeStub(isolate),
        bit_field_(StringAddFlagsBits::encode(flags) |
                   PretenureFlagBits::encode(pretenure_flag)) {}

  StringAddFlags flags() const {
    return StringAddFlagsBits::decode(bit_field_);
  }

  PretenureFlag pretenure_flag() const {
    return PretenureFlagBits::decode(bit_field_);
  }

  virtual void VerifyPlatformFeatures() V8_OVERRIDE {
    ASSERT(CpuFeatures::VerifyCrossCompiling(SSE2));
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLeft = 0;
  static const int kRight = 1;

 private:
  class StringAddFlagsBits: public BitField<StringAddFlags, 0, 2> {};
  class PretenureFlagBits: public BitField<PretenureFlag, 2, 1> {};
  uint32_t bit_field_;

  virtual Major MajorKey() V8_OVERRIDE { return StringAdd; }
  virtual int NotMissMinorKey() V8_OVERRIDE { return bit_field_; }

  virtual void PrintBaseName(StringStream* stream) V8_OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(StringAddStub);
};


class ICCompareStub: public PlatformCodeStub {
 public:
  ICCompareStub(Isolate* isolate,
                Token::Value op,
                CompareIC::State left,
                CompareIC::State right,
                CompareIC::State handler)
      : PlatformCodeStub(isolate),
        op_(op),
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

  virtual InlineCacheState GetICState();

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
  virtual bool FindCodeInSpecialCache(Code** code_out);
  virtual bool UseSpecialCache() { return state_ == CompareIC::KNOWN_OBJECT; }

  Token::Value op_;
  CompareIC::State left_;
  CompareIC::State right_;
  CompareIC::State state_;
  Handle<Map> known_map_;
};


class CompareNilICStub : public HydrogenCodeStub  {
 public:
  Type* GetType(Zone* zone, Handle<Map> map = Handle<Map>());
  Type* GetInputType(Zone* zone, Handle<Map> map);

  CompareNilICStub(Isolate* isolate, NilValue nil)
      : HydrogenCodeStub(isolate), nil_value_(nil) { }

  CompareNilICStub(Isolate* isolate,
                   ExtraICState ic_state,
                   InitializationState init_state = INITIALIZED)
      : HydrogenCodeStub(isolate, init_state),
        nil_value_(NilValueField::decode(ic_state)),
        state_(State(TypesField::decode(ic_state))) {
      }

  static Handle<Code> GetUninitialized(Isolate* isolate,
                                       NilValue nil) {
    return CompareNilICStub(isolate, nil, UNINITIALIZED).GetCode();
  }

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  static void InstallDescriptors(Isolate* isolate) {
    CompareNilICStub compare_stub(isolate, kNullValue, UNINITIALIZED);
    compare_stub.InitializeInterfaceDescriptor(
        isolate->code_stub_interface_descriptor(CodeStub::CompareNilIC));
  }

  virtual InlineCacheState GetICState() {
    if (state_.Contains(GENERIC)) {
      return MEGAMORPHIC;
    } else if (state_.Contains(MONOMORPHIC_MAP)) {
      return MONOMORPHIC;
    } else {
      return PREMONOMORPHIC;
    }
  }

  virtual Code::Kind GetCodeKind() const { return Code::COMPARE_NIL_IC; }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual ExtraICState GetExtraICState() {
    return NilValueField::encode(nil_value_) |
           TypesField::encode(state_.ToIntegral());
  }

  void UpdateStatus(Handle<Object> object);

  bool IsMonomorphic() const { return state_.Contains(MONOMORPHIC_MAP); }
  NilValue GetNilValue() const { return nil_value_; }
  void ClearState() { state_.RemoveAll(); }

  virtual void PrintState(StringStream* stream);
  virtual void PrintBaseName(StringStream* stream);

 private:
  friend class CompareNilIC;

  enum CompareNilType {
    UNDEFINED,
    NULL_TYPE,
    MONOMORPHIC_MAP,
    GENERIC,
    NUMBER_OF_TYPES
  };

  // At most 6 different types can be distinguished, because the Code object
  // only has room for a single byte to hold a set and there are two more
  // boolean flags we need to store. :-P
  STATIC_ASSERT(NUMBER_OF_TYPES <= 6);

  class State : public EnumSet<CompareNilType, byte> {
   public:
    State() : EnumSet<CompareNilType, byte>(0) { }
    explicit State(byte bits) : EnumSet<CompareNilType, byte>(bits) { }

    void Print(StringStream* stream) const;
  };

  CompareNilICStub(Isolate* isolate,
                   NilValue nil,
                   InitializationState init_state)
      : HydrogenCodeStub(isolate, init_state), nil_value_(nil) { }

  class NilValueField : public BitField<NilValue, 0, 1> {};
  class TypesField    : public BitField<byte,     1, NUMBER_OF_TYPES> {};

  virtual CodeStub::Major MajorKey() { return CompareNilIC; }
  virtual int NotMissMinorKey() { return GetExtraICState(); }

  NilValue nil_value_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(CompareNilICStub);
};


class CEntryStub : public PlatformCodeStub {
 public:
  CEntryStub(Isolate* isolate,
             int result_size,
             SaveFPRegsMode save_doubles = kDontSaveFPRegs)
      : PlatformCodeStub(isolate),
        result_size_(result_size),
        save_doubles_(save_doubles) { }

  void Generate(MacroAssembler* masm);

  // The version of this stub that doesn't save doubles is generated ahead of
  // time, so it's OK to call it from other stubs that can't cope with GC during
  // their code generation.  On machines that always have gp registers (x64) we
  // can generate both variants ahead of time.
  static void GenerateAheadOfTime(Isolate* isolate);

 protected:
  virtual void VerifyPlatformFeatures() V8_OVERRIDE {
    ASSERT(CpuFeatures::VerifyCrossCompiling(SSE2));
  };

 private:
  // Number of pointers/values returned.
  const int result_size_;
  SaveFPRegsMode save_doubles_;

  Major MajorKey() { return CEntry; }
  int MinorKey();

  bool NeedsImmovableCode();
};


class JSEntryStub : public PlatformCodeStub {
 public:
  explicit JSEntryStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

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
  explicit JSConstructEntryStub(Isolate* isolate) : JSEntryStub(isolate) { }

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
    NEW_SLOPPY_FAST,
    NEW_SLOPPY_SLOW,
    NEW_STRICT
  };

  ArgumentsAccessStub(Isolate* isolate, Type type)
      : PlatformCodeStub(isolate), type_(type) { }

 private:
  Type type_;

  Major MajorKey() { return ArgumentsAccess; }
  int MinorKey() { return type_; }

  void Generate(MacroAssembler* masm);
  void GenerateReadElement(MacroAssembler* masm);
  void GenerateNewStrict(MacroAssembler* masm);
  void GenerateNewSloppyFast(MacroAssembler* masm);
  void GenerateNewSloppySlow(MacroAssembler* masm);

  virtual void PrintName(StringStream* stream);
};


class RegExpExecStub: public PlatformCodeStub {
 public:
  explicit RegExpExecStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

 private:
  Major MajorKey() { return RegExpExec; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);
};


class RegExpConstructResultStub V8_FINAL : public HydrogenCodeStub {
 public:
  explicit RegExpConstructResultStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) { }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  virtual Major MajorKey() V8_OVERRIDE { return RegExpConstructResult; }
  virtual int NotMissMinorKey() V8_OVERRIDE { return 0; }

  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLength = 0;
  static const int kIndex = 1;
  static const int kInput = 2;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegExpConstructResultStub);
};


class CallFunctionStub: public PlatformCodeStub {
 public:
  CallFunctionStub(Isolate* isolate, int argc, CallFunctionFlags flags)
      : PlatformCodeStub(isolate), argc_(argc), flags_(flags) { }

  void Generate(MacroAssembler* masm);

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

  bool CallAsMethod() {
    return flags_ == CALL_AS_METHOD || flags_ == WRAP_AND_CALL;
  }

  bool NeedsChecks() {
    return flags_ != WRAP_AND_CALL;
  }
};


class CallConstructStub: public PlatformCodeStub {
 public:
  CallConstructStub(Isolate* isolate, CallConstructorFlags flags)
      : PlatformCodeStub(isolate), flags_(flags) {}

  void Generate(MacroAssembler* masm);

  virtual void FinishCode(Handle<Code> code) {
    code->set_has_function_cache(RecordCallTarget());
  }

 private:
  CallConstructorFlags flags_;

  virtual void PrintName(StringStream* stream);

  Major MajorKey() { return CallConstruct; }
  int MinorKey() { return flags_; }

  bool RecordCallTarget() {
    return (flags_ & RECORD_CONSTRUCTOR_TARGET) != 0;
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


class KeyedLoadDictionaryElementStub : public HydrogenCodeStub {
 public:
  explicit KeyedLoadDictionaryElementStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) {}

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return KeyedLoadElement; }
  int NotMissMinorKey() { return DICTIONARY_ELEMENTS; }

  DISALLOW_COPY_AND_ASSIGN(KeyedLoadDictionaryElementStub);
};


class KeyedLoadDictionaryElementPlatformStub : public PlatformCodeStub {
 public:
  explicit KeyedLoadDictionaryElementPlatformStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return KeyedLoadElement; }
  int MinorKey() { return DICTIONARY_ELEMENTS; }

  DISALLOW_COPY_AND_ASSIGN(KeyedLoadDictionaryElementPlatformStub);
};


class DoubleToIStub : public PlatformCodeStub {
 public:
  DoubleToIStub(Isolate* isolate,
                Register source,
                Register destination,
                int offset,
                bool is_truncating,
                bool skip_fastpath = false)
      : PlatformCodeStub(isolate), bit_field_(0) {
    bit_field_ = SourceRegisterBits::encode(source.code()) |
      DestinationRegisterBits::encode(destination.code()) |
      OffsetBits::encode(offset) |
      IsTruncatingBits::encode(is_truncating) |
      SkipFastPathBits::encode(skip_fastpath) |
      SSEBits::encode(
          CpuFeatures::IsSafeForSnapshot(isolate, SSE2) ?
          CpuFeatures::IsSafeForSnapshot(isolate, SSE3) ? 2 : 1 : 0);
  }

  Register source() {
    return Register::from_code(SourceRegisterBits::decode(bit_field_));
  }

  Register destination() {
    return Register::from_code(DestinationRegisterBits::decode(bit_field_));
  }

  bool is_truncating() {
    return IsTruncatingBits::decode(bit_field_);
  }

  bool skip_fastpath() {
    return SkipFastPathBits::decode(bit_field_);
  }

  int offset() {
    return OffsetBits::decode(bit_field_);
  }

  void Generate(MacroAssembler* masm);

  virtual bool SometimesSetsUpAFrame() { return false; }

 protected:
  virtual void VerifyPlatformFeatures() V8_OVERRIDE {
    ASSERT(CpuFeatures::VerifyCrossCompiling(SSE2));
  }

 private:
  static const int kBitsPerRegisterNumber = 6;
  STATIC_ASSERT((1L << kBitsPerRegisterNumber) >= Register::kNumRegisters);
  class SourceRegisterBits:
      public BitField<int, 0, kBitsPerRegisterNumber> {};  // NOLINT
  class DestinationRegisterBits:
      public BitField<int, kBitsPerRegisterNumber,
        kBitsPerRegisterNumber> {};  // NOLINT
  class IsTruncatingBits:
      public BitField<bool, 2 * kBitsPerRegisterNumber, 1> {};  // NOLINT
  class OffsetBits:
      public BitField<int, 2 * kBitsPerRegisterNumber + 1, 3> {};  // NOLINT
  class SkipFastPathBits:
      public BitField<int, 2 * kBitsPerRegisterNumber + 4, 1> {};  // NOLINT
  class SSEBits:
      public BitField<int, 2 * kBitsPerRegisterNumber + 5, 2> {};  // NOLINT

  Major MajorKey() { return DoubleToI; }
  int MinorKey() { return bit_field_; }

  int bit_field_;

  DISALLOW_COPY_AND_ASSIGN(DoubleToIStub);
};


class KeyedLoadFastElementStub : public HydrogenCodeStub {
 public:
  KeyedLoadFastElementStub(Isolate* isolate,
                           bool is_js_array,
                           ElementsKind elements_kind)
      : HydrogenCodeStub(isolate) {
    bit_field_ = ElementsKindBits::encode(elements_kind) |
        IsJSArrayBits::encode(is_js_array);
  }

  bool is_js_array() const {
    return IsJSArrayBits::decode(bit_field_);
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(bit_field_);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

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
  KeyedStoreFastElementStub(Isolate* isolate,
                            bool is_js_array,
                            ElementsKind elements_kind,
                            KeyedAccessStoreMode mode)
      : HydrogenCodeStub(isolate) {
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

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

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
  TransitionElementsKindStub(Isolate* isolate,
                             ElementsKind from_kind,
                             ElementsKind to_kind,
                             bool is_js_array) : HydrogenCodeStub(isolate) {
    bit_field_ = FromKindBits::encode(from_kind) |
                 ToKindBits::encode(to_kind) |
                 IsJSArrayBits::encode(is_js_array);
  }

  ElementsKind from_kind() const {
    return FromKindBits::decode(bit_field_);
  }

  ElementsKind to_kind() const {
    return ToKindBits::decode(bit_field_);
  }

  bool is_js_array() const {
    return IsJSArrayBits::decode(bit_field_);
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  class FromKindBits: public BitField<ElementsKind, 8, 8> {};
  class ToKindBits: public BitField<ElementsKind, 0, 8> {};
  class IsJSArrayBits: public BitField<bool, 16, 1> {};
  uint32_t bit_field_;

  Major MajorKey() { return TransitionElementsKind; }
  int NotMissMinorKey() { return bit_field_; }

  DISALLOW_COPY_AND_ASSIGN(TransitionElementsKindStub);
};


class ArrayConstructorStubBase : public HydrogenCodeStub {
 public:
  ArrayConstructorStubBase(Isolate* isolate,
                           ElementsKind kind,
                           AllocationSiteOverrideMode override_mode)
      : HydrogenCodeStub(isolate) {
    // It only makes sense to override local allocation site behavior
    // if there is a difference between the global allocation site policy
    // for an ElementsKind and the desired usage of the stub.
    ASSERT(override_mode != DISABLE_ALLOCATION_SITES ||
           AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE);
    bit_field_ = ElementsKindBits::encode(kind) |
        AllocationSiteOverrideModeBits::encode(override_mode);
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(bit_field_);
  }

  AllocationSiteOverrideMode override_mode() const {
    return AllocationSiteOverrideModeBits::decode(bit_field_);
  }

  static void GenerateStubsAheadOfTime(Isolate* isolate);
  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kConstructor = 0;
  static const int kAllocationSite = 1;

 protected:
  void BasePrintName(const char* name, StringStream* stream);

 private:
  int NotMissMinorKey() { return bit_field_; }

  // Ensure data fits within available bits.
  STATIC_ASSERT(LAST_ALLOCATION_SITE_OVERRIDE_MODE == 1);

  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class AllocationSiteOverrideModeBits: public
      BitField<AllocationSiteOverrideMode, 8, 1> {};  // NOLINT
  uint32_t bit_field_;

  DISALLOW_COPY_AND_ASSIGN(ArrayConstructorStubBase);
};


class ArrayNoArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNoArgumentConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return ArrayNoArgumentConstructor; }

  virtual void PrintName(StringStream* stream) {
    BasePrintName("ArrayNoArgumentConstructorStub", stream);
  }

  DISALLOW_COPY_AND_ASSIGN(ArrayNoArgumentConstructorStub);
};


class ArraySingleArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArraySingleArgumentConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return ArraySingleArgumentConstructor; }

  virtual void PrintName(StringStream* stream) {
    BasePrintName("ArraySingleArgumentConstructorStub", stream);
  }

  DISALLOW_COPY_AND_ASSIGN(ArraySingleArgumentConstructorStub);
};


class ArrayNArgumentsConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNArgumentsConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return ArrayNArgumentsConstructor; }

  virtual void PrintName(StringStream* stream) {
    BasePrintName("ArrayNArgumentsConstructorStub", stream);
  }

  DISALLOW_COPY_AND_ASSIGN(ArrayNArgumentsConstructorStub);
};


class InternalArrayConstructorStubBase : public HydrogenCodeStub {
 public:
  InternalArrayConstructorStubBase(Isolate* isolate, ElementsKind kind)
      : HydrogenCodeStub(isolate) {
    kind_ = kind;
  }

  static void GenerateStubsAheadOfTime(Isolate* isolate);
  static void InstallDescriptors(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kConstructor = 0;

  ElementsKind elements_kind() const { return kind_; }

 private:
  int NotMissMinorKey() { return kind_; }

  ElementsKind kind_;

  DISALLOW_COPY_AND_ASSIGN(InternalArrayConstructorStubBase);
};


class InternalArrayNoArgumentConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArrayNoArgumentConstructorStub(Isolate* isolate,
                                         ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return InternalArrayNoArgumentConstructor; }

  DISALLOW_COPY_AND_ASSIGN(InternalArrayNoArgumentConstructorStub);
};


class InternalArraySingleArgumentConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArraySingleArgumentConstructorStub(Isolate* isolate,
                                             ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return InternalArraySingleArgumentConstructor; }

  DISALLOW_COPY_AND_ASSIGN(InternalArraySingleArgumentConstructorStub);
};


class InternalArrayNArgumentsConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArrayNArgumentsConstructorStub(Isolate* isolate, ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  Major MajorKey() { return InternalArrayNArgumentsConstructor; }

  DISALLOW_COPY_AND_ASSIGN(InternalArrayNArgumentsConstructorStub);
};


class KeyedStoreElementStub : public PlatformCodeStub {
 public:
  KeyedStoreElementStub(Isolate* isolate,
                        bool is_js_array,
                        ElementsKind elements_kind,
                        KeyedAccessStoreMode store_mode)
      : PlatformCodeStub(isolate),
        is_js_array_(is_js_array),
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


class ToBooleanStub: public HydrogenCodeStub {
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

  class Types : public EnumSet<Type, byte> {
   public:
    Types() : EnumSet<Type, byte>(0) {}
    explicit Types(byte bits) : EnumSet<Type, byte>(bits) {}

    byte ToByte() const { return ToIntegral(); }
    void Print(StringStream* stream) const;
    bool UpdateStatus(Handle<Object> object);
    bool NeedsMap() const;
    bool CanBeUndetectable() const;
    bool IsGeneric() const { return ToIntegral() == Generic().ToIntegral(); }

    static Types Generic() { return Types((1 << NUMBER_OF_TYPES) - 1); }
  };

  ToBooleanStub(Isolate* isolate, Types types = Types())
      : HydrogenCodeStub(isolate), types_(types) { }
  ToBooleanStub(Isolate* isolate, ExtraICState state)
      : HydrogenCodeStub(isolate), types_(static_cast<byte>(state)) { }

  bool UpdateStatus(Handle<Object> object);
  Types GetTypes() { return types_; }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;
  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

  virtual Code::Kind GetCodeKind() const { return Code::TO_BOOLEAN_IC; }
  virtual void PrintState(StringStream* stream);

  virtual bool SometimesSetsUpAFrame() { return false; }

  static void InstallDescriptors(Isolate* isolate) {
    ToBooleanStub stub(isolate);
    stub.InitializeInterfaceDescriptor(
        isolate->code_stub_interface_descriptor(CodeStub::ToBoolean));
  }

  static Handle<Code> GetUninitialized(Isolate* isolate) {
    return ToBooleanStub(isolate, UNINITIALIZED).GetCode();
  }

  virtual ExtraICState GetExtraICState() {
    return types_.ToIntegral();
  }

  virtual InlineCacheState GetICState() {
    if (types_.IsEmpty()) {
      return ::v8::internal::UNINITIALIZED;
    } else {
      return MONOMORPHIC;
    }
  }

 private:
  Major MajorKey() { return ToBoolean; }
  int NotMissMinorKey() { return GetExtraICState(); }

  ToBooleanStub(Isolate* isolate, InitializationState init_state) :
      HydrogenCodeStub(isolate, init_state) {}

  Types types_;
};


class ElementsTransitionAndStoreStub : public HydrogenCodeStub {
 public:
  ElementsTransitionAndStoreStub(Isolate* isolate,
                                 ElementsKind from_kind,
                                 ElementsKind to_kind,
                                 bool is_jsarray,
                                 KeyedAccessStoreMode store_mode)
      : HydrogenCodeStub(isolate),
        from_kind_(from_kind),
        to_kind_(to_kind),
        is_jsarray_(is_jsarray),
        store_mode_(store_mode) {}

  ElementsKind from_kind() const { return from_kind_; }
  ElementsKind to_kind() const { return to_kind_; }
  bool is_jsarray() const { return is_jsarray_; }
  KeyedAccessStoreMode store_mode() const { return store_mode_; }

  virtual Handle<Code> GenerateCode() V8_OVERRIDE;

  virtual void InitializeInterfaceDescriptor(
      CodeStubInterfaceDescriptor* descriptor) V8_OVERRIDE;

 private:
  class FromBits:      public BitField<ElementsKind,          0, 8> {};
  class ToBits:        public BitField<ElementsKind,          8, 8> {};
  class IsJSArrayBits: public BitField<bool,                 16, 1> {};
  class StoreModeBits: public BitField<KeyedAccessStoreMode, 17, 4> {};

  Major MajorKey() { return ElementsTransitionAndStore; }
  int NotMissMinorKey() {
    return FromBits::encode(from_kind_) |
        ToBits::encode(to_kind_) |
        IsJSArrayBits::encode(is_jsarray_) |
        StoreModeBits::encode(store_mode_);
  }

  ElementsKind from_kind_;
  ElementsKind to_kind_;
  bool is_jsarray_;
  KeyedAccessStoreMode store_mode_;

  DISALLOW_COPY_AND_ASSIGN(ElementsTransitionAndStoreStub);
};


class StoreArrayLiteralElementStub : public PlatformCodeStub {
 public:
  explicit StoreArrayLiteralElementStub(Isolate* isolate)
      : PlatformCodeStub(isolate), fp_registers_(CanUseFPRegisters()) { }

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
  StubFailureTrampolineStub(Isolate* isolate, StubFunctionMode function_mode)
      : PlatformCodeStub(isolate),
        fp_registers_(CanUseFPRegisters()),
        function_mode_(function_mode) {}

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
  explicit ProfileEntryHookStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  // The profile entry hook function is not allowed to cause a GC.
  virtual bool SometimesSetsUpAFrame() { return false; }

  // Generates a call to the entry hook if it's enabled.
  static void MaybeCallEntryHook(MacroAssembler* masm);

 private:
  static void EntryHookTrampoline(intptr_t function,
                                  intptr_t stack_pointer,
                                  Isolate* isolate);

  Major MajorKey() { return ProfileEntryHook; }
  int MinorKey() { return 0; }

  void Generate(MacroAssembler* masm);

  DISALLOW_COPY_AND_ASSIGN(ProfileEntryHookStub);
};


class CallDescriptors {
 public:
  static void InitializeForIsolate(Isolate* isolate);
};

} }  // namespace v8::internal

#endif  // V8_CODE_STUBS_H_

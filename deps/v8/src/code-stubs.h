// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/codegen.h"
#include "src/globals.h"
#include "src/ic/ic-state.h"
#include "src/interface-descriptors.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

// List of code stubs used on all platforms.
#define CODE_STUB_LIST_ALL_PLATFORMS(V)     \
  /* PlatformCodeStubs */                   \
  V(ArgumentsAccess)                        \
  V(ArrayConstructor)                       \
  V(BinaryOpICWithAllocationSite)           \
  V(CallApiFunction)                        \
  V(CallApiGetter)                          \
  V(CallConstruct)                          \
  V(CallFunction)                           \
  V(CallIC)                                 \
  V(CallIC_Array)                           \
  V(CEntry)                                 \
  V(CompareIC)                              \
  V(DoubleToI)                              \
  V(FunctionPrototype)                      \
  V(Instanceof)                             \
  V(InternalArrayConstructor)               \
  V(JSEntry)                                \
  V(KeyedLoadICTrampoline)                  \
  V(LoadICTrampoline)                       \
  V(LoadIndexedInterceptor)                 \
  V(MathPow)                                \
  V(ProfileEntryHook)                       \
  V(RecordWrite)                            \
  V(RegExpExec)                             \
  V(StoreArrayLiteralElement)               \
  V(StoreBufferOverflow)                    \
  V(StoreElement)                           \
  V(StringCompare)                          \
  V(StubFailureTrampoline)                  \
  V(SubString)                              \
  /* HydrogenCodeStubs */                   \
  V(ArrayNArgumentsConstructor)             \
  V(ArrayNoArgumentConstructor)             \
  V(ArraySingleArgumentConstructor)         \
  V(BinaryOpIC)                             \
  V(BinaryOpWithAllocationSite)             \
  V(CompareNilIC)                           \
  V(CreateAllocationSite)                   \
  V(ElementsTransitionAndStore)             \
  V(FastCloneShallowArray)                  \
  V(FastCloneShallowObject)                 \
  V(FastNewClosure)                         \
  V(FastNewContext)                         \
  V(InternalArrayNArgumentsConstructor)     \
  V(InternalArrayNoArgumentConstructor)     \
  V(InternalArraySingleArgumentConstructor) \
  V(KeyedLoadGeneric)                       \
  V(LoadDictionaryElement)                  \
  V(LoadFastElement)                        \
  V(MegamorphicLoad)                        \
  V(NameDictionaryLookup)                   \
  V(NumberToString)                         \
  V(RegExpConstructResult)                  \
  V(StoreFastElement)                       \
  V(StringAdd)                              \
  V(ToBoolean)                              \
  V(ToNumber)                               \
  V(TransitionElementsKind)                 \
  V(VectorKeyedLoad)                        \
  V(VectorLoad)                             \
  /* IC Handler stubs */                    \
  V(LoadConstant)                           \
  V(LoadField)                              \
  V(KeyedLoadSloppyArguments)               \
  V(StoreField)                             \
  V(StoreGlobal)                            \
  V(StringLength)

// List of code stubs only used on ARM 32 bits platforms.
#if V8_TARGET_ARCH_ARM
#define CODE_STUB_LIST_ARM(V) \
  V(DirectCEntry)             \
  V(WriteInt32ToHeapNumber)

#else
#define CODE_STUB_LIST_ARM(V)
#endif

// List of code stubs only used on ARM 64 bits platforms.
#if V8_TARGET_ARCH_ARM64
#define CODE_STUB_LIST_ARM64(V) \
  V(DirectCEntry)               \
  V(RestoreRegistersState)      \
  V(StoreRegistersState)

#else
#define CODE_STUB_LIST_ARM64(V)
#endif

// List of code stubs only used on MIPS platforms.
#if V8_TARGET_ARCH_MIPS
#define CODE_STUB_LIST_MIPS(V)  \
  V(DirectCEntry)               \
  V(RestoreRegistersState)      \
  V(StoreRegistersState)        \
  V(WriteInt32ToHeapNumber)
#elif V8_TARGET_ARCH_MIPS64
#define CODE_STUB_LIST_MIPS(V)  \
  V(DirectCEntry)               \
  V(RestoreRegistersState)      \
  V(StoreRegistersState)        \
  V(WriteInt32ToHeapNumber)
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
    // TODO(mvstanton): eliminate the NoCache key by getting rid
    //                  of the non-monomorphic-cache.
    NoCache = 0,  // marker for stubs that do custom caching]
#define DEF_ENUM(name) name,
    CODE_STUB_LIST(DEF_ENUM)
#undef DEF_ENUM
    NUMBER_OF_IDS
  };

  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GetCode();

  // Retrieve the code for the stub, make and return a copy of the code.
  Handle<Code> GetCodeCopy(const Code::FindAndReplacePattern& pattern);

  static Major MajorKeyFromKey(uint32_t key) {
    return static_cast<Major>(MajorKeyBits::decode(key));
  }
  static uint32_t MinorKeyFromKey(uint32_t key) {
    return MinorKeyBits::decode(key);
  }

  // Gets the major key from a code object that is a code stub or binary op IC.
  static Major GetMajorKey(Code* code_stub) {
    return MajorKeyFromKey(code_stub->stub_key());
  }

  static uint32_t NoCacheKey() { return MajorKeyBits::encode(NoCache); }

  static const char* MajorName(Major major_key, bool allow_unknown_keys);

  explicit CodeStub(Isolate* isolate) : minor_key_(0), isolate_(isolate) {}
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

  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() = 0;

  virtual void InitializeDescriptor(CodeStubDescriptor* descriptor) {}

  static void InitializeDescriptor(Isolate* isolate, uint32_t key,
                                   CodeStubDescriptor* desc);

  static MaybeHandle<Code> GetCode(Isolate* isolate, uint32_t key);

  // Returns information for computing the number key.
  virtual Major MajorKey() const = 0;
  uint32_t MinorKey() const { return minor_key_; }

  virtual InlineCacheState GetICState() const { return UNINITIALIZED; }
  virtual ExtraICState GetExtraICState() const { return kNoExtraICState; }
  virtual Code::StubType GetStubType() {
    return Code::NORMAL;
  }

  friend OStream& operator<<(OStream& os, const CodeStub& s) {
    s.PrintName(os);
    return os;
  }

  Isolate* isolate() const { return isolate_; }

 protected:
  CodeStub(uint32_t key, Isolate* isolate)
      : minor_key_(MinorKeyFromKey(key)), isolate_(isolate) {}

  // Generates the assembler code for the stub.
  virtual Handle<Code> GenerateCode() = 0;

  // Returns whether the code generated for this stub needs to be allocated as
  // a fixed (non-moveable) code object.
  virtual bool NeedsImmovableCode() { return false; }

  virtual void PrintName(OStream& os) const;        // NOLINT
  virtual void PrintBaseName(OStream& os) const;    // NOLINT
  virtual void PrintState(OStream& os) const { ; }  // NOLINT

  // Computes the key based on major and minor.
  uint32_t GetKey() {
    DCHECK(static_cast<int>(MajorKey()) < NUMBER_OF_IDS);
    return MinorKeyBits::encode(MinorKey()) | MajorKeyBits::encode(MajorKey());
  }

  uint32_t minor_key_;

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

  // We use this dispatch to statically instantiate the correct code stub for
  // the given stub key and call the passed function with that code stub.
  typedef void (*DispatchedCall)(CodeStub* stub, void** value_out);
  static void Dispatch(Isolate* isolate, uint32_t key, void** value_out,
                       DispatchedCall call);

  static void GetCodeDispatchCall(CodeStub* stub, void** value_out);

  STATIC_ASSERT(NUMBER_OF_IDS < (1 << kStubMajorKeyBits));
  class MajorKeyBits: public BitField<uint32_t, 0, kStubMajorKeyBits> {};
  class MinorKeyBits: public BitField<uint32_t,
      kStubMajorKeyBits, kStubMinorKeyBits> {};  // NOLINT

  friend class BreakPointIterator;

  Isolate* isolate_;
};


#define DEFINE_CODE_STUB_BASE(NAME, SUPER)                      \
 public:                                                        \
  NAME(uint32_t key, Isolate* isolate) : SUPER(key, isolate) {} \
                                                                \
 private:                                                       \
  DISALLOW_COPY_AND_ASSIGN(NAME)


#define DEFINE_CODE_STUB(NAME, SUPER)              \
 protected:                                        \
  virtual inline Major MajorKey() const OVERRIDE { \
    return NAME;                                   \
  };                                               \
  DEFINE_CODE_STUB_BASE(NAME##Stub, SUPER)


#define DEFINE_PLATFORM_CODE_STUB(NAME, SUPER)          \
 private:                                               \
  virtual void Generate(MacroAssembler* masm) OVERRIDE; \
  DEFINE_CODE_STUB(NAME, SUPER)


#define DEFINE_HYDROGEN_CODE_STUB(NAME, SUPER)                                \
 public:                                                                      \
  virtual void InitializeDescriptor(CodeStubDescriptor* descriptor) OVERRIDE; \
  virtual Handle<Code> GenerateCode() OVERRIDE;                               \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_HANDLER_CODE_STUB(NAME, SUPER)   \
 public:                                        \
  virtual Handle<Code> GenerateCode() OVERRIDE; \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_CALL_INTERFACE_DESCRIPTOR(NAME)                            \
 public:                                                                  \
  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() OVERRIDE { \
    return NAME##Descriptor(isolate());                                   \
  }

// There are some code stubs we just can't describe right now with a
// CallInterfaceDescriptor. Isolate behavior for those cases with this macro.
// An attempt to retrieve a descriptor will fail.
#define DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR()                           \
 public:                                                                  \
  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() OVERRIDE { \
    UNREACHABLE();                                                        \
    return CallInterfaceDescriptor();                                     \
  }


class PlatformCodeStub : public CodeStub {
 public:
  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode() OVERRIDE;

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }

 protected:
  explicit PlatformCodeStub(Isolate* isolate) : CodeStub(isolate) {}

  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;

  DEFINE_CODE_STUB_BASE(PlatformCodeStub, CodeStub);
};


enum StubFunctionMode { NOT_JS_FUNCTION_STUB_MODE, JS_FUNCTION_STUB_MODE };
enum HandlerArgumentsMode { DONT_PASS_ARGUMENTS, PASS_ARGUMENTS };


class CodeStubDescriptor {
 public:
  explicit CodeStubDescriptor(CodeStub* stub);

  CodeStubDescriptor(Isolate* isolate, uint32_t stub_key);

  void Initialize(Address deoptimization_handler = NULL,
                  int hint_stack_parameter_count = -1,
                  StubFunctionMode function_mode = NOT_JS_FUNCTION_STUB_MODE);
  void Initialize(Register stack_parameter_count,
                  Address deoptimization_handler = NULL,
                  int hint_stack_parameter_count = -1,
                  StubFunctionMode function_mode = NOT_JS_FUNCTION_STUB_MODE,
                  HandlerArgumentsMode handler_mode = DONT_PASS_ARGUMENTS);

  void SetMissHandler(ExternalReference handler) {
    miss_handler_ = handler;
    has_miss_handler_ = true;
    // Our miss handler infrastructure doesn't currently support
    // variable stack parameter counts.
    DCHECK(!stack_parameter_count_.is_valid());
  }

  void set_call_descriptor(CallInterfaceDescriptor d) { call_descriptor_ = d; }
  CallInterfaceDescriptor call_descriptor() const { return call_descriptor_; }

  int GetEnvironmentParameterCount() const {
    return call_descriptor().GetEnvironmentParameterCount();
  }

  Representation GetEnvironmentParameterRepresentation(int index) const {
    return call_descriptor().GetEnvironmentParameterRepresentation(index);
  }

  ExternalReference miss_handler() const {
    DCHECK(has_miss_handler_);
    return miss_handler_;
  }

  bool has_miss_handler() const {
    return has_miss_handler_;
  }

  bool IsEnvironmentParameterCountRegister(int index) const {
    return call_descriptor().GetEnvironmentParameterRegister(index).is(
        stack_parameter_count_);
  }

  int GetHandlerParameterCount() const {
    int params = call_descriptor().GetEnvironmentParameterCount();
    if (handler_arguments_mode_ == PASS_ARGUMENTS) {
      params += 1;
    }
    return params;
  }

  int hint_stack_parameter_count() const { return hint_stack_parameter_count_; }
  Register stack_parameter_count() const { return stack_parameter_count_; }
  StubFunctionMode function_mode() const { return function_mode_; }
  Address deoptimization_handler() const { return deoptimization_handler_; }

 private:
  CallInterfaceDescriptor call_descriptor_;
  Register stack_parameter_count_;
  // If hint_stack_parameter_count_ > 0, the code stub can optimize the
  // return sequence. Default value is -1, which means it is ignored.
  int hint_stack_parameter_count_;
  StubFunctionMode function_mode_;

  Address deoptimization_handler_;
  HandlerArgumentsMode handler_arguments_mode_;

  ExternalReference miss_handler_;
  bool has_miss_handler_;
};


class HydrogenCodeStub : public CodeStub {
 public:
  enum InitializationState {
    UNINITIALIZED,
    INITIALIZED
  };

  virtual Code::Kind GetCodeKind() const { return Code::STUB; }

  template<class SubClass>
  static Handle<Code> GetUninitialized(Isolate* isolate) {
    SubClass::GenerateAheadOfTime(isolate);
    return SubClass().GetCode(isolate);
  }

  // Retrieve the code for the stub. Generate the code if needed.
  virtual Handle<Code> GenerateCode() = 0;

  bool IsUninitialized() const { return IsMissBits::decode(minor_key_); }

  Handle<Code> GenerateLightweightMissCode(ExternalReference miss);

  template<class StateType>
  void TraceTransition(StateType from, StateType to);

 protected:
  explicit HydrogenCodeStub(Isolate* isolate,
                            InitializationState state = INITIALIZED)
      : CodeStub(isolate) {
    minor_key_ = IsMissBits::encode(state == UNINITIALIZED);
  }

  void set_sub_minor_key(uint32_t key) {
    minor_key_ = SubMinorKeyBits::update(minor_key_, key);
  }

  uint32_t sub_minor_key() const { return SubMinorKeyBits::decode(minor_key_); }

  static const int kSubMinorKeyBits = kStubMinorKeyBits - 1;

 private:
  class IsMissBits : public BitField<bool, kSubMinorKeyBits, 1> {};
  class SubMinorKeyBits : public BitField<int, 0, kSubMinorKeyBits> {};

  void GenerateLightweightMiss(MacroAssembler* masm, ExternalReference miss);

  DEFINE_CODE_STUB_BASE(HydrogenCodeStub, CodeStub);
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
#include "src/ia32/code-stubs-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/x64/code-stubs-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/code-stubs-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/code-stubs-arm.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/code-stubs-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/code-stubs-mips64.h"
#elif V8_TARGET_ARCH_X87
#include "src/x87/code-stubs-x87.h"
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

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ToNumber);
  DEFINE_HYDROGEN_CODE_STUB(ToNumber, HydrogenCodeStub);
};


class NumberToStringStub FINAL : public HydrogenCodeStub {
 public:
  explicit NumberToStringStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kNumber = 0;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(NumberToString);
  DEFINE_HYDROGEN_CODE_STUB(NumberToString, HydrogenCodeStub);
};


class FastNewClosureStub : public HydrogenCodeStub {
 public:
  FastNewClosureStub(Isolate* isolate, StrictMode strict_mode,
                     FunctionKind kind)
      : HydrogenCodeStub(isolate) {
    DCHECK(IsValidFunctionKind(kind));
    set_sub_minor_key(StrictModeBits::encode(strict_mode) |
                      FunctionKindBits::encode(kind));
  }

  StrictMode strict_mode() const {
    return StrictModeBits::decode(sub_minor_key());
  }

  FunctionKind kind() const {
    return FunctionKindBits::decode(sub_minor_key());
  }
  bool is_arrow() const { return IsArrowFunction(kind()); }
  bool is_generator() const { return IsGeneratorFunction(kind()); }
  bool is_concise_method() const { return IsConciseMethod(kind()); }

 private:
  class StrictModeBits : public BitField<StrictMode, 0, 1> {};
  class FunctionKindBits : public BitField<FunctionKind, 1, 3> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewClosure);
  DEFINE_HYDROGEN_CODE_STUB(FastNewClosure, HydrogenCodeStub);
};


class FastNewContextStub FINAL : public HydrogenCodeStub {
 public:
  static const int kMaximumSlots = 64;

  FastNewContextStub(Isolate* isolate, int slots) : HydrogenCodeStub(isolate) {
    DCHECK(slots > 0 && slots <= kMaximumSlots);
    set_sub_minor_key(SlotsBits::encode(slots));
  }

  int slots() const { return SlotsBits::decode(sub_minor_key()); }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kFunction = 0;

 private:
  class SlotsBits : public BitField<int, 0, 8> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewContext);
  DEFINE_HYDROGEN_CODE_STUB(FastNewContext, HydrogenCodeStub);
};


class FastCloneShallowArrayStub : public HydrogenCodeStub {
 public:
  FastCloneShallowArrayStub(Isolate* isolate,
                            AllocationSiteMode allocation_site_mode)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(AllocationSiteModeBits::encode(allocation_site_mode));
  }

  AllocationSiteMode allocation_site_mode() const {
    return AllocationSiteModeBits::decode(sub_minor_key());
  }

 private:
  class AllocationSiteModeBits: public BitField<AllocationSiteMode, 0, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastCloneShallowArray);
  DEFINE_HYDROGEN_CODE_STUB(FastCloneShallowArray, HydrogenCodeStub);
};


class FastCloneShallowObjectStub : public HydrogenCodeStub {
 public:
  // Maximum number of properties in copied object.
  static const int kMaximumClonedProperties = 6;

  FastCloneShallowObjectStub(Isolate* isolate, int length)
      : HydrogenCodeStub(isolate) {
    DCHECK_GE(length, 0);
    DCHECK_LE(length, kMaximumClonedProperties);
    set_sub_minor_key(LengthBits::encode(length));
  }

  int length() const { return LengthBits::decode(sub_minor_key()); }

 private:
  class LengthBits : public BitField<int, 0, 4> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastCloneShallowObject);
  DEFINE_HYDROGEN_CODE_STUB(FastCloneShallowObject, HydrogenCodeStub);
};


class CreateAllocationSiteStub : public HydrogenCodeStub {
 public:
  explicit CreateAllocationSiteStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) { }

  static void GenerateAheadOfTime(Isolate* isolate);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CreateAllocationSite);
  DEFINE_HYDROGEN_CODE_STUB(CreateAllocationSite, HydrogenCodeStub);
};


class InstanceofStub: public PlatformCodeStub {
 public:
  enum Flags {
    kNoFlags = 0,
    kArgsInRegisters = 1 << 0,
    kCallSiteInlineCheck = 1 << 1,
    kReturnTrueFalseObject = 1 << 2
  };

  InstanceofStub(Isolate* isolate, Flags flags) : PlatformCodeStub(isolate) {
    minor_key_ = FlagBits::encode(flags);
  }

  static Register left() { return InstanceofDescriptor::left(); }
  static Register right() { return InstanceofDescriptor::right(); }

  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() OVERRIDE {
    if (HasArgsInRegisters()) {
      return InstanceofDescriptor(isolate());
    }
    return ContextOnlyDescriptor(isolate());
  }

 private:
  Flags flags() const { return FlagBits::decode(minor_key_); }

  bool HasArgsInRegisters() const { return (flags() & kArgsInRegisters) != 0; }

  bool HasCallSiteInlineCheck() const {
    return (flags() & kCallSiteInlineCheck) != 0;
  }

  bool ReturnTrueFalseObject() const {
    return (flags() & kReturnTrueFalseObject) != 0;
  }

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class FlagBits : public BitField<Flags, 0, 3> {};

  DEFINE_PLATFORM_CODE_STUB(Instanceof, PlatformCodeStub);
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

 private:
  ArgumentCountKey argument_count() const {
    return ArgumentCountBits::decode(minor_key_);
  }

  void GenerateDispatchToArrayStub(MacroAssembler* masm,
                                   AllocationSiteOverrideMode mode);

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class ArgumentCountBits : public BitField<ArgumentCountKey, 0, 2> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayConstructor);
  DEFINE_PLATFORM_CODE_STUB(ArrayConstructor, PlatformCodeStub);
};


class InternalArrayConstructorStub: public PlatformCodeStub {
 public:
  explicit InternalArrayConstructorStub(Isolate* isolate);

 private:
  void GenerateCase(MacroAssembler* masm, ElementsKind kind);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(InternalArrayConstructor);
  DEFINE_PLATFORM_CODE_STUB(InternalArrayConstructor, PlatformCodeStub);
};


class MathPowStub: public PlatformCodeStub {
 public:
  enum ExponentType { INTEGER, DOUBLE, TAGGED, ON_STACK };

  MathPowStub(Isolate* isolate, ExponentType exponent_type)
      : PlatformCodeStub(isolate) {
    minor_key_ = ExponentTypeBits::encode(exponent_type);
  }

  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() OVERRIDE {
    if (exponent_type() == TAGGED) {
      return MathPowTaggedDescriptor(isolate());
    } else if (exponent_type() == INTEGER) {
      return MathPowIntegerDescriptor(isolate());
    }
    // A CallInterfaceDescriptor doesn't specify double registers (yet).
    return ContextOnlyDescriptor(isolate());
  }

 private:
  ExponentType exponent_type() const {
    return ExponentTypeBits::decode(minor_key_);
  }

  class ExponentTypeBits : public BitField<ExponentType, 0, 2> {};

  DEFINE_PLATFORM_CODE_STUB(MathPow, PlatformCodeStub);
};


class CallICStub: public PlatformCodeStub {
 public:
  CallICStub(Isolate* isolate, const CallICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  static int ExtractArgcFromMinorKey(int minor_key) {
    CallICState state(static_cast<ExtraICState>(minor_key));
    return state.arg_count();
  }

  virtual Code::Kind GetCodeKind() const OVERRIDE { return Code::CALL_IC; }

  virtual InlineCacheState GetICState() const OVERRIDE { return DEFAULT; }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(minor_key_);
  }

 protected:
  bool CallAsMethod() const {
    return state().call_type() == CallICState::METHOD;
  }

  int arg_count() const { return state().arg_count(); }

  CallICState state() const {
    return CallICState(static_cast<ExtraICState>(minor_key_));
  }

  // Code generation helpers.
  void GenerateMiss(MacroAssembler* masm);

 private:
  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallFunctionWithFeedback);
  DEFINE_PLATFORM_CODE_STUB(CallIC, PlatformCodeStub);
};


class CallIC_ArrayStub: public CallICStub {
 public:
  CallIC_ArrayStub(Isolate* isolate, const CallICState& state_in)
      : CallICStub(isolate, state_in) {}

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return MONOMORPHIC;
  }

 private:
  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

  DEFINE_PLATFORM_CODE_STUB(CallIC_Array, CallICStub);
};


// TODO(verwaest): Translate to hydrogen code stub.
class FunctionPrototypeStub : public PlatformCodeStub {
 public:
  explicit FunctionPrototypeStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  virtual Code::Kind GetCodeKind() const { return Code::HANDLER; }

  // TODO(mvstanton): only the receiver register is accessed. When this is
  // translated to a hydrogen code stub, a new CallInterfaceDescriptor
  // should be created that just uses that register for more efficient code.
  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_PLATFORM_CODE_STUB(FunctionPrototype, PlatformCodeStub);
};


// TODO(mvstanton): Translate to hydrogen code stub.
class LoadIndexedInterceptorStub : public PlatformCodeStub {
 public:
  explicit LoadIndexedInterceptorStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  virtual Code::Kind GetCodeKind() const { return Code::HANDLER; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_PLATFORM_CODE_STUB(LoadIndexedInterceptor, PlatformCodeStub);
};


class HandlerStub : public HydrogenCodeStub {
 public:
  virtual Code::Kind GetCodeKind() const { return Code::HANDLER; }
  virtual ExtraICState GetExtraICState() const { return kind(); }
  virtual InlineCacheState GetICState() const { return MONOMORPHIC; }

  virtual void InitializeDescriptor(CodeStubDescriptor* descriptor) OVERRIDE;

  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() OVERRIDE;

 protected:
  explicit HandlerStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  virtual Code::Kind kind() const = 0;

  DEFINE_CODE_STUB_BASE(HandlerStub, HydrogenCodeStub);
};


class LoadFieldStub: public HandlerStub {
 public:
  LoadFieldStub(Isolate* isolate, FieldIndex index) : HandlerStub(isolate) {
    int property_index_key = index.GetFieldAccessStubKey();
    set_sub_minor_key(LoadFieldByIndexBits::encode(property_index_key));
  }

  FieldIndex index() const {
    int property_index_key = LoadFieldByIndexBits::decode(sub_minor_key());
    return FieldIndex::FromFieldAccessStubKey(property_index_key);
  }

 protected:
  virtual Code::Kind kind() const { return Code::LOAD_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  class LoadFieldByIndexBits : public BitField<int, 0, 13> {};

  DEFINE_HANDLER_CODE_STUB(LoadField, HandlerStub);
};


class KeyedLoadSloppyArgumentsStub : public HandlerStub {
 public:
  explicit KeyedLoadSloppyArgumentsStub(Isolate* isolate)
      : HandlerStub(isolate) {}

 protected:
  virtual Code::Kind kind() const { return Code::KEYED_LOAD_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  DEFINE_HANDLER_CODE_STUB(KeyedLoadSloppyArguments, HandlerStub);
};


class LoadConstantStub : public HandlerStub {
 public:
  LoadConstantStub(Isolate* isolate, int constant_index)
      : HandlerStub(isolate) {
    set_sub_minor_key(ConstantIndexBits::encode(constant_index));
  }

  int constant_index() const {
    return ConstantIndexBits::decode(sub_minor_key());
  }

 protected:
  virtual Code::Kind kind() const { return Code::LOAD_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  class ConstantIndexBits : public BitField<int, 0, kSubMinorKeyBits> {};

  DEFINE_HANDLER_CODE_STUB(LoadConstant, HandlerStub);
};


class StringLengthStub: public HandlerStub {
 public:
  explicit StringLengthStub(Isolate* isolate) : HandlerStub(isolate) {}

 protected:
  virtual Code::Kind kind() const { return Code::LOAD_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

  DEFINE_HANDLER_CODE_STUB(StringLength, HandlerStub);
};


class StoreFieldStub : public HandlerStub {
 public:
  StoreFieldStub(Isolate* isolate, FieldIndex index,
                 Representation representation)
      : HandlerStub(isolate) {
    int property_index_key = index.GetFieldAccessStubKey();
    uint8_t repr = PropertyDetails::EncodeRepresentation(representation);
    set_sub_minor_key(StoreFieldByIndexBits::encode(property_index_key) |
                      RepresentationBits::encode(repr));
  }

  FieldIndex index() const {
    int property_index_key = StoreFieldByIndexBits::decode(sub_minor_key());
    return FieldIndex::FromFieldAccessStubKey(property_index_key);
  }

  Representation representation() {
    uint8_t repr = RepresentationBits::decode(sub_minor_key());
    return PropertyDetails::DecodeRepresentation(repr);
  }

 protected:
  virtual Code::Kind kind() const { return Code::STORE_IC; }
  virtual Code::StubType GetStubType() { return Code::FAST; }

 private:
  class StoreFieldByIndexBits : public BitField<int, 0, 13> {};
  class RepresentationBits : public BitField<uint8_t, 13, 4> {};

  DEFINE_HANDLER_CODE_STUB(StoreField, HandlerStub);
};


class StoreGlobalStub : public HandlerStub {
 public:
  StoreGlobalStub(Isolate* isolate, bool is_constant, bool check_global)
      : HandlerStub(isolate) {
    set_sub_minor_key(IsConstantBits::encode(is_constant) |
                      CheckGlobalBits::encode(check_global));
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

  bool is_constant() const { return IsConstantBits::decode(sub_minor_key()); }

  bool check_global() const { return CheckGlobalBits::decode(sub_minor_key()); }

  void set_is_constant(bool value) {
    set_sub_minor_key(IsConstantBits::update(sub_minor_key(), value));
  }

  Representation representation() {
    return Representation::FromKind(
        RepresentationBits::decode(sub_minor_key()));
  }

  void set_representation(Representation r) {
    set_sub_minor_key(RepresentationBits::update(sub_minor_key(), r.kind()));
  }

 private:
  class IsConstantBits: public BitField<bool, 0, 1> {};
  class RepresentationBits: public BitField<Representation::Kind, 1, 8> {};
  class CheckGlobalBits: public BitField<bool, 9, 1> {};

  DEFINE_HANDLER_CODE_STUB(StoreGlobal, HandlerStub);
};


class CallApiFunctionStub : public PlatformCodeStub {
 public:
  CallApiFunctionStub(Isolate* isolate,
                      bool is_store,
                      bool call_data_undefined,
                      int argc) : PlatformCodeStub(isolate) {
    minor_key_ = IsStoreBits::encode(is_store) |
                 CallDataUndefinedBits::encode(call_data_undefined) |
                 ArgumentBits::encode(argc);
    DCHECK(!is_store || argc == 1);
  }

 private:
  bool is_store() const { return IsStoreBits::decode(minor_key_); }
  bool call_data_undefined() const {
    return CallDataUndefinedBits::decode(minor_key_);
  }
  int argc() const { return ArgumentBits::decode(minor_key_); }

  class IsStoreBits: public BitField<bool, 0, 1> {};
  class CallDataUndefinedBits: public BitField<bool, 1, 1> {};
  class ArgumentBits: public BitField<int, 2, Code::kArgumentsBits> {};
  STATIC_ASSERT(Code::kArgumentsBits + 2 <= kStubMinorKeyBits);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ApiFunction);
  DEFINE_PLATFORM_CODE_STUB(CallApiFunction, PlatformCodeStub);
};


class CallApiGetterStub : public PlatformCodeStub {
 public:
  explicit CallApiGetterStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ApiGetter);
  DEFINE_PLATFORM_CODE_STUB(CallApiGetter, PlatformCodeStub);
};


class BinaryOpICStub : public HydrogenCodeStub {
 public:
  BinaryOpICStub(Isolate* isolate, Token::Value op,
                 OverwriteMode mode = NO_OVERWRITE)
      : HydrogenCodeStub(isolate, UNINITIALIZED) {
    BinaryOpICState state(isolate, op, mode);
    set_sub_minor_key(state.GetExtraICState());
  }

  BinaryOpICStub(Isolate* isolate, const BinaryOpICState& state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(state.GetExtraICState());
  }

  static void GenerateAheadOfTime(Isolate* isolate);

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::BINARY_OP_IC;
  }

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return state().GetICState();
  }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(sub_minor_key());
  }

  BinaryOpICState state() const {
    return BinaryOpICState(isolate(), GetExtraICState());
  }

  virtual void PrintState(OStream& os) const FINAL OVERRIDE;  // NOLINT

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLeft = 0;
  static const int kRight = 1;

 private:
  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpICState& state);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_HYDROGEN_CODE_STUB(BinaryOpIC, HydrogenCodeStub);
};


// TODO(bmeurer): Merge this into the BinaryOpICStub once we have proper tail
// call support for stubs in Hydrogen.
class BinaryOpICWithAllocationSiteStub FINAL : public PlatformCodeStub {
 public:
  BinaryOpICWithAllocationSiteStub(Isolate* isolate,
                                   const BinaryOpICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  static void GenerateAheadOfTime(Isolate* isolate);

  Handle<Code> GetCodeCopyFromTemplate(Handle<AllocationSite> allocation_site) {
    Code::FindAndReplacePattern pattern;
    pattern.Add(isolate()->factory()->undefined_map(), allocation_site);
    return CodeStub::GetCodeCopy(pattern);
  }

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::BINARY_OP_IC;
  }

  virtual InlineCacheState GetICState() const OVERRIDE {
    return state().GetICState();
  }

  virtual ExtraICState GetExtraICState() const OVERRIDE {
    return static_cast<ExtraICState>(minor_key_);
  }

  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

 private:
  BinaryOpICState state() const {
    return BinaryOpICState(isolate(), static_cast<ExtraICState>(minor_key_));
  }

  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpICState& state);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithAllocationSite);
  DEFINE_PLATFORM_CODE_STUB(BinaryOpICWithAllocationSite, PlatformCodeStub);
};


class BinaryOpWithAllocationSiteStub FINAL : public BinaryOpICStub {
 public:
  BinaryOpWithAllocationSiteStub(Isolate* isolate,
                                 Token::Value op,
                                 OverwriteMode mode)
      : BinaryOpICStub(isolate, op, mode) {}

  BinaryOpWithAllocationSiteStub(Isolate* isolate, const BinaryOpICState& state)
      : BinaryOpICStub(isolate, state) {}

  virtual Code::Kind GetCodeKind() const FINAL OVERRIDE {
    return Code::STUB;
  }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kAllocationSite = 0;
  static const int kLeft = 1;
  static const int kRight = 2;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithAllocationSite);
  DEFINE_HYDROGEN_CODE_STUB(BinaryOpWithAllocationSite, BinaryOpICStub);
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


class StringAddStub FINAL : public HydrogenCodeStub {
 public:
  StringAddStub(Isolate* isolate, StringAddFlags flags,
                PretenureFlag pretenure_flag)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(StringAddFlagsBits::encode(flags) |
                      PretenureFlagBits::encode(pretenure_flag));
  }

  StringAddFlags flags() const {
    return StringAddFlagsBits::decode(sub_minor_key());
  }

  PretenureFlag pretenure_flag() const {
    return PretenureFlagBits::decode(sub_minor_key());
  }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLeft = 0;
  static const int kRight = 1;

 private:
  class StringAddFlagsBits: public BitField<StringAddFlags, 0, 2> {};
  class PretenureFlagBits: public BitField<PretenureFlag, 2, 1> {};

  virtual void PrintBaseName(OStream& os) const OVERRIDE;  // NOLINT

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StringAdd);
  DEFINE_HYDROGEN_CODE_STUB(StringAdd, HydrogenCodeStub);
};


class CompareICStub : public PlatformCodeStub {
 public:
  CompareICStub(Isolate* isolate, Token::Value op, CompareICState::State left,
                CompareICState::State right, CompareICState::State state)
      : PlatformCodeStub(isolate) {
    DCHECK(Token::IsCompareOp(op));
    minor_key_ = OpBits::encode(op - Token::EQ) | LeftStateBits::encode(left) |
                 RightStateBits::encode(right) | StateBits::encode(state);
  }

  void set_known_map(Handle<Map> map) { known_map_ = map; }

  virtual InlineCacheState GetICState() const;

  Token::Value op() const {
    return static_cast<Token::Value>(Token::EQ + OpBits::decode(minor_key_));
  }

  CompareICState::State left() const {
    return LeftStateBits::decode(minor_key_);
  }
  CompareICState::State right() const {
    return RightStateBits::decode(minor_key_);
  }
  CompareICState::State state() const { return StateBits::decode(minor_key_); }

 private:
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

  bool strict() const { return op() == Token::EQ_STRICT; }
  Condition GetCondition() const;

  virtual void AddToSpecialCache(Handle<Code> new_object);
  virtual bool FindCodeInSpecialCache(Code** code_out);
  virtual bool UseSpecialCache() {
    return state() == CompareICState::KNOWN_OBJECT;
  }

  class OpBits : public BitField<int, 0, 3> {};
  class LeftStateBits : public BitField<CompareICState::State, 3, 4> {};
  class RightStateBits : public BitField<CompareICState::State, 7, 4> {};
  class StateBits : public BitField<CompareICState::State, 11, 4> {};

  Handle<Map> known_map_;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_PLATFORM_CODE_STUB(CompareIC, PlatformCodeStub);
};


class CompareNilICStub : public HydrogenCodeStub  {
 public:
  Type* GetType(Zone* zone, Handle<Map> map = Handle<Map>());
  Type* GetInputType(Zone* zone, Handle<Map> map);

  CompareNilICStub(Isolate* isolate, NilValue nil) : HydrogenCodeStub(isolate) {
    set_sub_minor_key(NilValueBits::encode(nil));
  }

  CompareNilICStub(Isolate* isolate, ExtraICState ic_state,
                   InitializationState init_state = INITIALIZED)
      : HydrogenCodeStub(isolate, init_state) {
    set_sub_minor_key(ic_state);
  }

  static Handle<Code> GetUninitialized(Isolate* isolate,
                                       NilValue nil) {
    return CompareNilICStub(isolate, nil, UNINITIALIZED).GetCode();
  }

  virtual InlineCacheState GetICState() const {
    State state = this->state();
    if (state.Contains(GENERIC)) {
      return MEGAMORPHIC;
    } else if (state.Contains(MONOMORPHIC_MAP)) {
      return MONOMORPHIC;
    } else {
      return PREMONOMORPHIC;
    }
  }

  virtual Code::Kind GetCodeKind() const { return Code::COMPARE_NIL_IC; }

  virtual ExtraICState GetExtraICState() const { return sub_minor_key(); }

  void UpdateStatus(Handle<Object> object);

  bool IsMonomorphic() const { return state().Contains(MONOMORPHIC_MAP); }

  NilValue nil_value() const { return NilValueBits::decode(sub_minor_key()); }

  void ClearState() {
    set_sub_minor_key(TypesBits::update(sub_minor_key(), 0));
  }

  virtual void PrintState(OStream& os) const OVERRIDE;     // NOLINT
  virtual void PrintBaseName(OStream& os) const OVERRIDE;  // NOLINT

 private:
  CompareNilICStub(Isolate* isolate, NilValue nil,
                   InitializationState init_state)
      : HydrogenCodeStub(isolate, init_state) {
    set_sub_minor_key(NilValueBits::encode(nil));
  }

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
  };
  friend OStream& operator<<(OStream& os, const State& s);

  State state() const { return State(TypesBits::decode(sub_minor_key())); }

  class NilValueBits : public BitField<NilValue, 0, 1> {};
  class TypesBits : public BitField<byte, 1, NUMBER_OF_TYPES> {};

  friend class CompareNilIC;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CompareNil);
  DEFINE_HYDROGEN_CODE_STUB(CompareNilIC, HydrogenCodeStub);
};


OStream& operator<<(OStream& os, const CompareNilICStub::State& s);


class CEntryStub : public PlatformCodeStub {
 public:
  CEntryStub(Isolate* isolate, int result_size,
             SaveFPRegsMode save_doubles = kDontSaveFPRegs)
      : PlatformCodeStub(isolate) {
    minor_key_ = SaveDoublesBits::encode(save_doubles == kSaveFPRegs);
    DCHECK(result_size == 1 || result_size == 2);
#ifdef _WIN64
    minor_key_ = ResultSizeBits::update(minor_key_, result_size);
#endif  // _WIN64
  }

  // The version of this stub that doesn't save doubles is generated ahead of
  // time, so it's OK to call it from other stubs that can't cope with GC during
  // their code generation.  On machines that always have gp registers (x64) we
  // can generate both variants ahead of time.
  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  bool save_doubles() const { return SaveDoublesBits::decode(minor_key_); }
#ifdef _WIN64
  int result_size() const { return ResultSizeBits::decode(minor_key_); }
#endif  // _WIN64

  bool NeedsImmovableCode();

  class SaveDoublesBits : public BitField<bool, 0, 1> {};
  class ResultSizeBits : public BitField<int, 1, 3> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(CEntry, PlatformCodeStub);
};


class JSEntryStub : public PlatformCodeStub {
 public:
  JSEntryStub(Isolate* isolate, StackFrame::Type type)
      : PlatformCodeStub(isolate) {
    DCHECK(type == StackFrame::ENTRY || type == StackFrame::ENTRY_CONSTRUCT);
    minor_key_ = StackFrameTypeBits::encode(type);
  }

 private:
  virtual void FinishCode(Handle<Code> code);

  virtual void PrintName(OStream& os) const OVERRIDE {  // NOLINT
    os << (type() == StackFrame::ENTRY ? "JSEntryStub"
                                       : "JSConstructEntryStub");
  }

  StackFrame::Type type() const {
    return StackFrameTypeBits::decode(minor_key_);
  }

  class StackFrameTypeBits : public BitField<StackFrame::Type, 0, 5> {};

  int handler_offset_;

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(JSEntry, PlatformCodeStub);
};


class ArgumentsAccessStub: public PlatformCodeStub {
 public:
  enum Type {
    READ_ELEMENT,
    NEW_SLOPPY_FAST,
    NEW_SLOPPY_SLOW,
    NEW_STRICT
  };

  ArgumentsAccessStub(Isolate* isolate, Type type) : PlatformCodeStub(isolate) {
    minor_key_ = TypeBits::encode(type);
  }

  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() OVERRIDE {
    if (type() == READ_ELEMENT) {
      return ArgumentsAccessReadDescriptor(isolate());
    }
    return ContextOnlyDescriptor(isolate());
  }

 private:
  Type type() const { return TypeBits::decode(minor_key_); }

  void GenerateReadElement(MacroAssembler* masm);
  void GenerateNewStrict(MacroAssembler* masm);
  void GenerateNewSloppyFast(MacroAssembler* masm);
  void GenerateNewSloppySlow(MacroAssembler* masm);

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class TypeBits : public BitField<Type, 0, 2> {};

  DEFINE_PLATFORM_CODE_STUB(ArgumentsAccess, PlatformCodeStub);
};


class RegExpExecStub: public PlatformCodeStub {
 public:
  explicit RegExpExecStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ContextOnly);
  DEFINE_PLATFORM_CODE_STUB(RegExpExec, PlatformCodeStub);
};


class RegExpConstructResultStub FINAL : public HydrogenCodeStub {
 public:
  explicit RegExpConstructResultStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) { }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kLength = 0;
  static const int kIndex = 1;
  static const int kInput = 2;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(RegExpConstructResult);
  DEFINE_HYDROGEN_CODE_STUB(RegExpConstructResult, HydrogenCodeStub);
};


class CallFunctionStub: public PlatformCodeStub {
 public:
  CallFunctionStub(Isolate* isolate, int argc, CallFunctionFlags flags)
      : PlatformCodeStub(isolate) {
    DCHECK(argc >= 0 && argc <= Code::kMaxArguments);
    minor_key_ = ArgcBits::encode(argc) | FlagBits::encode(flags);
  }

  static int ExtractArgcFromMinorKey(int minor_key) {
    return ArgcBits::decode(minor_key);
  }

 private:
  int argc() const { return ArgcBits::decode(minor_key_); }
  int flags() const { return FlagBits::decode(minor_key_); }

  bool CallAsMethod() const {
    return flags() == CALL_AS_METHOD || flags() == WRAP_AND_CALL;
  }

  bool NeedsChecks() const { return flags() != WRAP_AND_CALL; }

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  // Minor key encoding in 32 bits with Bitfield <Type, shift, size>.
  class FlagBits : public BitField<CallFunctionFlags, 0, 2> {};
  class ArgcBits : public BitField<unsigned, 2, Code::kArgumentsBits> {};
  STATIC_ASSERT(Code::kArgumentsBits + 2 <= kStubMinorKeyBits);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallFunction);
  DEFINE_PLATFORM_CODE_STUB(CallFunction, PlatformCodeStub);
};


class CallConstructStub: public PlatformCodeStub {
 public:
  CallConstructStub(Isolate* isolate, CallConstructorFlags flags)
      : PlatformCodeStub(isolate) {
    minor_key_ = FlagBits::encode(flags);
  }

  virtual void FinishCode(Handle<Code> code) {
    code->set_has_function_cache(RecordCallTarget());
  }

 private:
  CallConstructorFlags flags() const { return FlagBits::decode(minor_key_); }

  bool RecordCallTarget() const {
    return (flags() & RECORD_CONSTRUCTOR_TARGET) != 0;
  }

  virtual void PrintName(OStream& os) const OVERRIDE;  // NOLINT

  class FlagBits : public BitField<CallConstructorFlags, 0, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallConstruct);
  DEFINE_PLATFORM_CODE_STUB(CallConstruct, PlatformCodeStub);
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
    DCHECK(!result_.is(object_));
    DCHECK(!result_.is(index_));
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
    DCHECK(!code_.is(result_));
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


class LoadDictionaryElementStub : public HydrogenCodeStub {
 public:
  explicit LoadDictionaryElementStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_HYDROGEN_CODE_STUB(LoadDictionaryElement, HydrogenCodeStub);
};


class KeyedLoadGenericStub : public HydrogenCodeStub {
 public:
  explicit KeyedLoadGenericStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  virtual Code::Kind GetCodeKind() const { return Code::KEYED_LOAD_IC; }
  virtual InlineCacheState GetICState() const { return GENERIC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_HYDROGEN_CODE_STUB(KeyedLoadGeneric, HydrogenCodeStub);
};


class LoadICTrampolineStub : public PlatformCodeStub {
 public:
  LoadICTrampolineStub(Isolate* isolate, const LoadICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  virtual Code::Kind GetCodeKind() const OVERRIDE { return Code::LOAD_IC; }

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return GENERIC;
  }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(minor_key_);
  }

 private:
  LoadICState state() const {
    return LoadICState(static_cast<ExtraICState>(minor_key_));
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(VectorLoadICTrampoline);
  DEFINE_PLATFORM_CODE_STUB(LoadICTrampoline, PlatformCodeStub);
};


class KeyedLoadICTrampolineStub : public LoadICTrampolineStub {
 public:
  explicit KeyedLoadICTrampolineStub(Isolate* isolate)
      : LoadICTrampolineStub(isolate, LoadICState(0)) {}

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::KEYED_LOAD_IC;
  }

  DEFINE_PLATFORM_CODE_STUB(KeyedLoadICTrampoline, LoadICTrampolineStub);
};


class MegamorphicLoadStub : public HydrogenCodeStub {
 public:
  MegamorphicLoadStub(Isolate* isolate, const LoadICState& state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(state.GetExtraICState());
  }

  virtual Code::Kind GetCodeKind() const OVERRIDE { return Code::LOAD_IC; }

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return MEGAMORPHIC;
  }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(sub_minor_key());
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_HYDROGEN_CODE_STUB(MegamorphicLoad, HydrogenCodeStub);
};


class VectorLoadStub : public HydrogenCodeStub {
 public:
  explicit VectorLoadStub(Isolate* isolate, const LoadICState& state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(state.GetExtraICState());
  }

  virtual Code::Kind GetCodeKind() const OVERRIDE { return Code::LOAD_IC; }

  virtual InlineCacheState GetICState() const FINAL OVERRIDE {
    return GENERIC;
  }

  virtual ExtraICState GetExtraICState() const FINAL OVERRIDE {
    return static_cast<ExtraICState>(sub_minor_key());
  }

 private:
  LoadICState state() const { return LoadICState(GetExtraICState()); }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(VectorLoadIC);
  DEFINE_HYDROGEN_CODE_STUB(VectorLoad, HydrogenCodeStub);
};


class VectorKeyedLoadStub : public VectorLoadStub {
 public:
  explicit VectorKeyedLoadStub(Isolate* isolate)
      : VectorLoadStub(isolate, LoadICState(0)) {}

  virtual Code::Kind GetCodeKind() const OVERRIDE {
    return Code::KEYED_LOAD_IC;
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(VectorLoadIC);
  DEFINE_HYDROGEN_CODE_STUB(VectorKeyedLoad, VectorLoadStub);
};


class DoubleToIStub : public PlatformCodeStub {
 public:
  DoubleToIStub(Isolate* isolate, Register source, Register destination,
                int offset, bool is_truncating, bool skip_fastpath = false)
      : PlatformCodeStub(isolate) {
    minor_key_ = SourceRegisterBits::encode(source.code()) |
                 DestinationRegisterBits::encode(destination.code()) |
                 OffsetBits::encode(offset) |
                 IsTruncatingBits::encode(is_truncating) |
                 SkipFastPathBits::encode(skip_fastpath) |
                 SSE3Bits::encode(CpuFeatures::IsSupported(SSE3) ? 1 : 0);
  }

  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  Register source() const {
    return Register::from_code(SourceRegisterBits::decode(minor_key_));
  }
  Register destination() const {
    return Register::from_code(DestinationRegisterBits::decode(minor_key_));
  }
  bool is_truncating() const { return IsTruncatingBits::decode(minor_key_); }
  bool skip_fastpath() const { return SkipFastPathBits::decode(minor_key_); }
  int offset() const { return OffsetBits::decode(minor_key_); }

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
  class SSE3Bits:
      public BitField<int, 2 * kBitsPerRegisterNumber + 5, 1> {};  // NOLINT

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(DoubleToI, PlatformCodeStub);
};


class LoadFastElementStub : public HydrogenCodeStub {
 public:
  LoadFastElementStub(Isolate* isolate, bool is_js_array,
                      ElementsKind elements_kind)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(ElementsKindBits::encode(elements_kind) |
                      IsJSArrayBits::encode(is_js_array));
  }

  bool is_js_array() const { return IsJSArrayBits::decode(sub_minor_key()); }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

 private:
  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class IsJSArrayBits: public BitField<bool, 8, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_HYDROGEN_CODE_STUB(LoadFastElement, HydrogenCodeStub);
};


class StoreFastElementStub : public HydrogenCodeStub {
 public:
  StoreFastElementStub(Isolate* isolate, bool is_js_array,
                       ElementsKind elements_kind, KeyedAccessStoreMode mode)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(ElementsKindBits::encode(elements_kind) |
                      IsJSArrayBits::encode(is_js_array) |
                      StoreModeBits::encode(mode));
  }

  bool is_js_array() const { return IsJSArrayBits::decode(sub_minor_key()); }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

  KeyedAccessStoreMode store_mode() const {
    return StoreModeBits::decode(sub_minor_key());
  }

 private:
  class ElementsKindBits: public BitField<ElementsKind,      0, 8> {};
  class StoreModeBits: public BitField<KeyedAccessStoreMode, 8, 4> {};
  class IsJSArrayBits: public BitField<bool,                12, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Store);
  DEFINE_HYDROGEN_CODE_STUB(StoreFastElement, HydrogenCodeStub);
};


class TransitionElementsKindStub : public HydrogenCodeStub {
 public:
  TransitionElementsKindStub(Isolate* isolate,
                             ElementsKind from_kind,
                             ElementsKind to_kind,
                             bool is_js_array) : HydrogenCodeStub(isolate) {
    set_sub_minor_key(FromKindBits::encode(from_kind) |
                      ToKindBits::encode(to_kind) |
                      IsJSArrayBits::encode(is_js_array));
  }

  ElementsKind from_kind() const {
    return FromKindBits::decode(sub_minor_key());
  }

  ElementsKind to_kind() const { return ToKindBits::decode(sub_minor_key()); }

  bool is_js_array() const { return IsJSArrayBits::decode(sub_minor_key()); }

 private:
  class FromKindBits: public BitField<ElementsKind, 8, 8> {};
  class ToKindBits: public BitField<ElementsKind, 0, 8> {};
  class IsJSArrayBits: public BitField<bool, 16, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TransitionElementsKind);
  DEFINE_HYDROGEN_CODE_STUB(TransitionElementsKind, HydrogenCodeStub);
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
    DCHECK(override_mode != DISABLE_ALLOCATION_SITES ||
           AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE);
    set_sub_minor_key(ElementsKindBits::encode(kind) |
                      AllocationSiteOverrideModeBits::encode(override_mode));
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

  AllocationSiteOverrideMode override_mode() const {
    return AllocationSiteOverrideModeBits::decode(sub_minor_key());
  }

  static void GenerateStubsAheadOfTime(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kConstructor = 0;
  static const int kAllocationSite = 1;

 protected:
  OStream& BasePrintName(OStream& os, const char* name) const;  // NOLINT

 private:
  // Ensure data fits within available bits.
  STATIC_ASSERT(LAST_ALLOCATION_SITE_OVERRIDE_MODE == 1);

  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class AllocationSiteOverrideModeBits: public
      BitField<AllocationSiteOverrideMode, 8, 1> {};  // NOLINT

  DEFINE_CODE_STUB_BASE(ArrayConstructorStubBase, HydrogenCodeStub);
};


class ArrayNoArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNoArgumentConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

 private:
  virtual void PrintName(OStream& os) const OVERRIDE {  // NOLINT
    BasePrintName(os, "ArrayNoArgumentConstructorStub");
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayConstructorConstantArgCount);
  DEFINE_HYDROGEN_CODE_STUB(ArrayNoArgumentConstructor,
                            ArrayConstructorStubBase);
};


class ArraySingleArgumentConstructorStub : public ArrayConstructorStubBase {
 public:
  ArraySingleArgumentConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

 private:
  virtual void PrintName(OStream& os) const {  // NOLINT
    BasePrintName(os, "ArraySingleArgumentConstructorStub");
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayConstructor);
  DEFINE_HYDROGEN_CODE_STUB(ArraySingleArgumentConstructor,
                            ArrayConstructorStubBase);
};


class ArrayNArgumentsConstructorStub : public ArrayConstructorStubBase {
 public:
  ArrayNArgumentsConstructorStub(
      Isolate* isolate,
      ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : ArrayConstructorStubBase(isolate, kind, override_mode) {
  }

 private:
  virtual void PrintName(OStream& os) const {  // NOLINT
    BasePrintName(os, "ArrayNArgumentsConstructorStub");
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayConstructor);
  DEFINE_HYDROGEN_CODE_STUB(ArrayNArgumentsConstructor,
                            ArrayConstructorStubBase);
};


class InternalArrayConstructorStubBase : public HydrogenCodeStub {
 public:
  InternalArrayConstructorStubBase(Isolate* isolate, ElementsKind kind)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(ElementsKindBits::encode(kind));
  }

  static void GenerateStubsAheadOfTime(Isolate* isolate);

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  static const int kConstructor = 0;

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

 private:
  class ElementsKindBits : public BitField<ElementsKind, 0, 8> {};

  DEFINE_CODE_STUB_BASE(InternalArrayConstructorStubBase, HydrogenCodeStub);
};


class InternalArrayNoArgumentConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArrayNoArgumentConstructorStub(Isolate* isolate,
                                         ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(InternalArrayConstructorConstantArgCount);
  DEFINE_HYDROGEN_CODE_STUB(InternalArrayNoArgumentConstructor,
                            InternalArrayConstructorStubBase);
};


class InternalArraySingleArgumentConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArraySingleArgumentConstructorStub(Isolate* isolate,
                                             ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(InternalArrayConstructor);
  DEFINE_HYDROGEN_CODE_STUB(InternalArraySingleArgumentConstructor,
                            InternalArrayConstructorStubBase);
};


class InternalArrayNArgumentsConstructorStub : public
    InternalArrayConstructorStubBase {
 public:
  InternalArrayNArgumentsConstructorStub(Isolate* isolate, ElementsKind kind)
      : InternalArrayConstructorStubBase(isolate, kind) { }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(InternalArrayConstructor);
  DEFINE_HYDROGEN_CODE_STUB(InternalArrayNArgumentsConstructor,
                            InternalArrayConstructorStubBase);
};


class StoreElementStub : public PlatformCodeStub {
 public:
  StoreElementStub(Isolate* isolate, ElementsKind elements_kind)
      : PlatformCodeStub(isolate) {
    minor_key_ = ElementsKindBits::encode(elements_kind);
  }

 private:
  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(minor_key_);
  }

  class ElementsKindBits : public BitField<ElementsKind, 0, 8> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Store);
  DEFINE_PLATFORM_CODE_STUB(StoreElement, PlatformCodeStub);
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

  enum ResultMode {
    RESULT_AS_SMI,             // For Smi(1) on truthy value, Smi(0) otherwise.
    RESULT_AS_ODDBALL,         // For {true} on truthy value, {false} otherwise.
    RESULT_AS_INVERSE_ODDBALL  // For {false} on truthy value, {true} otherwise.
  };

  // At most 8 different types can be distinguished, because the Code object
  // only has room for a single byte to hold a set of these types. :-P
  STATIC_ASSERT(NUMBER_OF_TYPES <= 8);

  class Types : public EnumSet<Type, byte> {
   public:
    Types() : EnumSet<Type, byte>(0) {}
    explicit Types(byte bits) : EnumSet<Type, byte>(bits) {}

    byte ToByte() const { return ToIntegral(); }
    bool UpdateStatus(Handle<Object> object);
    bool NeedsMap() const;
    bool CanBeUndetectable() const;
    bool IsGeneric() const { return ToIntegral() == Generic().ToIntegral(); }

    static Types Generic() { return Types((1 << NUMBER_OF_TYPES) - 1); }
  };

  ToBooleanStub(Isolate* isolate, ResultMode mode, Types types = Types())
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(TypesBits::encode(types.ToByte()) |
                      ResultModeBits::encode(mode));
  }

  ToBooleanStub(Isolate* isolate, ExtraICState state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(TypesBits::encode(static_cast<byte>(state)) |
                      ResultModeBits::encode(RESULT_AS_SMI));
  }

  bool UpdateStatus(Handle<Object> object);
  Types types() const { return Types(TypesBits::decode(sub_minor_key())); }
  ResultMode mode() const { return ResultModeBits::decode(sub_minor_key()); }

  virtual Code::Kind GetCodeKind() const { return Code::TO_BOOLEAN_IC; }
  virtual void PrintState(OStream& os) const OVERRIDE;  // NOLINT

  virtual bool SometimesSetsUpAFrame() { return false; }

  static Handle<Code> GetUninitialized(Isolate* isolate) {
    return ToBooleanStub(isolate, UNINITIALIZED).GetCode();
  }

  virtual ExtraICState GetExtraICState() const { return types().ToIntegral(); }

  virtual InlineCacheState GetICState() const {
    if (types().IsEmpty()) {
      return ::v8::internal::UNINITIALIZED;
    } else {
      return MONOMORPHIC;
    }
  }

 private:
  ToBooleanStub(Isolate* isolate, InitializationState init_state)
      : HydrogenCodeStub(isolate, init_state) {
    set_sub_minor_key(ResultModeBits::encode(RESULT_AS_SMI));
  }

  class TypesBits : public BitField<byte, 0, NUMBER_OF_TYPES> {};
  class ResultModeBits : public BitField<ResultMode, NUMBER_OF_TYPES, 2> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ToBoolean);
  DEFINE_HYDROGEN_CODE_STUB(ToBoolean, HydrogenCodeStub);
};


OStream& operator<<(OStream& os, const ToBooleanStub::Types& t);


class ElementsTransitionAndStoreStub : public HydrogenCodeStub {
 public:
  ElementsTransitionAndStoreStub(Isolate* isolate, ElementsKind from_kind,
                                 ElementsKind to_kind, bool is_jsarray,
                                 KeyedAccessStoreMode store_mode)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(FromBits::encode(from_kind) | ToBits::encode(to_kind) |
                      IsJSArrayBits::encode(is_jsarray) |
                      StoreModeBits::encode(store_mode));
  }

  ElementsKind from_kind() const { return FromBits::decode(sub_minor_key()); }
  ElementsKind to_kind() const { return ToBits::decode(sub_minor_key()); }
  bool is_jsarray() const { return IsJSArrayBits::decode(sub_minor_key()); }
  KeyedAccessStoreMode store_mode() const {
    return StoreModeBits::decode(sub_minor_key());
  }

  // Parameters accessed via CodeStubGraphBuilder::GetParameter()
  enum ParameterIndices {
    kValueIndex,
    kMapIndex,
    kKeyIndex,
    kObjectIndex,
    kParameterCount
  };

  static const Register ValueRegister() {
    return ElementTransitionAndStoreDescriptor::ValueRegister();
  }
  static const Register MapRegister() {
    return ElementTransitionAndStoreDescriptor::MapRegister();
  }
  static const Register KeyRegister() {
    return ElementTransitionAndStoreDescriptor::NameRegister();
  }
  static const Register ObjectRegister() {
    return ElementTransitionAndStoreDescriptor::ReceiverRegister();
  }

 private:
  class FromBits : public BitField<ElementsKind, 0, 8> {};
  class ToBits : public BitField<ElementsKind, 8, 8> {};
  class IsJSArrayBits : public BitField<bool, 16, 1> {};
  class StoreModeBits : public BitField<KeyedAccessStoreMode, 17, 4> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ElementTransitionAndStore);
  DEFINE_HYDROGEN_CODE_STUB(ElementsTransitionAndStore, HydrogenCodeStub);
};


class StoreArrayLiteralElementStub : public PlatformCodeStub {
 public:
  explicit StoreArrayLiteralElementStub(Isolate* isolate)
      : PlatformCodeStub(isolate) { }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreArrayLiteralElement);
  DEFINE_PLATFORM_CODE_STUB(StoreArrayLiteralElement, PlatformCodeStub);
};


class StubFailureTrampolineStub : public PlatformCodeStub {
 public:
  StubFailureTrampolineStub(Isolate* isolate, StubFunctionMode function_mode)
      : PlatformCodeStub(isolate) {
    minor_key_ = FunctionModeField::encode(function_mode);
  }

  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  StubFunctionMode function_mode() const {
    return FunctionModeField::decode(minor_key_);
  }

  class FunctionModeField : public BitField<StubFunctionMode, 0, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(StubFailureTrampoline, PlatformCodeStub);
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

  // ProfileEntryHookStub is called at the start of a function, so it has the
  // same register set.
  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallFunction)
  DEFINE_PLATFORM_CODE_STUB(ProfileEntryHook, PlatformCodeStub);
};


class StoreBufferOverflowStub : public PlatformCodeStub {
 public:
  StoreBufferOverflowStub(Isolate* isolate, SaveFPRegsMode save_fp)
      : PlatformCodeStub(isolate) {
    minor_key_ = SaveDoublesBits::encode(save_fp == kSaveFPRegs);
  }

  static void GenerateFixedRegStubsAheadOfTime(Isolate* isolate);
  virtual bool SometimesSetsUpAFrame() { return false; }

 private:
  bool save_doubles() const { return SaveDoublesBits::decode(minor_key_); }

  class SaveDoublesBits : public BitField<bool, 0, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(StoreBufferOverflow, PlatformCodeStub);
};


class SubStringStub : public PlatformCodeStub {
 public:
  explicit SubStringStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ContextOnly);
  DEFINE_PLATFORM_CODE_STUB(SubString, PlatformCodeStub);
};


class StringCompareStub : public PlatformCodeStub {
 public:
  explicit StringCompareStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ContextOnly);
  DEFINE_PLATFORM_CODE_STUB(StringCompare, PlatformCodeStub);
};


#undef DEFINE_CALL_INTERFACE_DESCRIPTOR
#undef DEFINE_PLATFORM_CODE_STUB
#undef DEFINE_HANDLER_CODE_STUB
#undef DEFINE_HYDROGEN_CODE_STUB
#undef DEFINE_CODE_STUB
#undef DEFINE_CODE_STUB_BASE
} }  // namespace v8::internal

#endif  // V8_CODE_STUBS_H_

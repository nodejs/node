// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/codegen.h"
#include "src/factory.h"
#include "src/find-and-replace-pattern.h"
#include "src/globals.h"
#include "src/ic/ic-state.h"
#include "src/interface-descriptors.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"
#include "src/type-hints.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CodeStubAssembler;
namespace compiler {
class CodeAssemblerLabel;
class CodeAssemblerState;
class Node;
}

// List of code stubs used on all platforms.
#define CODE_STUB_LIST_ALL_PLATFORMS(V)       \
  /* --- PlatformCodeStubs --- */             \
  V(ArrayConstructor)                         \
  V(BinaryOpICWithAllocationSite)             \
  V(CallApiCallback)                          \
  V(CallApiGetter)                            \
  V(CallConstruct)                            \
  V(CallIC)                                   \
  V(CEntry)                                   \
  V(CompareIC)                                \
  V(DoubleToI)                                \
  V(InternalArrayConstructor)                 \
  V(JSEntry)                                  \
  V(MathPow)                                  \
  V(ProfileEntryHook)                         \
  V(RecordWrite)                              \
  V(StoreBufferOverflow)                      \
  V(StoreSlowElement)                         \
  V(SubString)                                \
  V(NameDictionaryLookup)                     \
  /* This can be removed once there are no */ \
  /* more deopting Hydrogen stubs. */         \
  V(StubFailureTrampoline)                    \
  /* These are only called from FCG */        \
  /* They can be removed when only the TF  */ \
  /* version of the corresponding stub is  */ \
  /* used universally */                      \
  V(CallICTrampoline)                         \
  /* --- HydrogenCodeStubs --- */             \
  /* These should never be ported to TF */    \
  /* because they are either used only by */  \
  /* FCG/Crankshaft or are deprecated */      \
  V(BinaryOpIC)                               \
  V(BinaryOpWithAllocationSite)               \
  V(ToBooleanIC)                              \
  V(TransitionElementsKind)                   \
  /* --- TurboFanCodeStubs --- */             \
  V(AllocateHeapNumber)                       \
  V(ArrayNoArgumentConstructor)               \
  V(ArraySingleArgumentConstructor)           \
  V(ArrayNArgumentsConstructor)               \
  V(CreateAllocationSite)                     \
  V(CreateWeakCell)                           \
  V(StringLength)                             \
  V(InternalArrayNoArgumentConstructor)       \
  V(InternalArraySingleArgumentConstructor)   \
  V(ElementsTransitionAndStore)               \
  V(KeyedLoadSloppyArguments)                 \
  V(KeyedStoreSloppyArguments)                \
  V(LoadScriptContextField)                   \
  V(StoreScriptContextField)                  \
  V(NumberToString)                           \
  V(StringAdd)                                \
  V(GetProperty)                              \
  V(StoreFastElement)                         \
  V(StoreInterceptor)                         \
  V(LoadIndexedInterceptor)                   \
  V(GrowArrayElements)

// List of code stubs only used on ARM 32 bits platforms.
#if V8_TARGET_ARCH_ARM
#define CODE_STUB_LIST_ARM(V) V(DirectCEntry)

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

// List of code stubs only used on PPC platforms.
#ifdef V8_TARGET_ARCH_PPC
#define CODE_STUB_LIST_PPC(V) \
  V(DirectCEntry)             \
  V(StoreRegistersState)      \
  V(RestoreRegistersState)
#else
#define CODE_STUB_LIST_PPC(V)
#endif

// List of code stubs only used on MIPS platforms.
#if V8_TARGET_ARCH_MIPS
#define CODE_STUB_LIST_MIPS(V) \
  V(DirectCEntry)              \
  V(RestoreRegistersState)     \
  V(StoreRegistersState)
#elif V8_TARGET_ARCH_MIPS64
#define CODE_STUB_LIST_MIPS(V) \
  V(DirectCEntry)              \
  V(RestoreRegistersState)     \
  V(StoreRegistersState)
#else
#define CODE_STUB_LIST_MIPS(V)
#endif

// List of code stubs only used on S390 platforms.
#ifdef V8_TARGET_ARCH_S390
#define CODE_STUB_LIST_S390(V) \
  V(DirectCEntry)              \
  V(StoreRegistersState)       \
  V(RestoreRegistersState)
#else
#define CODE_STUB_LIST_S390(V)
#endif

// Combined list of code stubs.
#define CODE_STUB_LIST(V)         \
  CODE_STUB_LIST_ALL_PLATFORMS(V) \
  CODE_STUB_LIST_ARM(V)           \
  CODE_STUB_LIST_ARM64(V)         \
  CODE_STUB_LIST_PPC(V)           \
  CODE_STUB_LIST_MIPS(V)          \
  CODE_STUB_LIST_S390(V)

static const int kHasReturnedMinusZeroSentinel = 1;

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
  Handle<Code> GetCodeCopy(const FindAndReplacePattern& pattern);

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

  static const char* MajorName(Major major_key);

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

  virtual CallInterfaceDescriptor GetCallInterfaceDescriptor() const = 0;

  virtual int GetStackParameterCount() const {
    return GetCallInterfaceDescriptor().GetStackParameterCount();
  }

  virtual void InitializeDescriptor(CodeStubDescriptor* descriptor) {}

  static void InitializeDescriptor(Isolate* isolate, uint32_t key,
                                   CodeStubDescriptor* desc);

  static MaybeHandle<Code> GetCode(Isolate* isolate, uint32_t key);

  // Returns information for computing the number key.
  virtual Major MajorKey() const = 0;
  uint32_t MinorKey() const { return minor_key_; }

  // BinaryOpStub needs to override this.
  virtual Code::Kind GetCodeKind() const;

  virtual ExtraICState GetExtraICState() const { return kNoExtraICState; }

  Code::Flags GetCodeFlags() const;

  friend std::ostream& operator<<(std::ostream& os, const CodeStub& s) {
    s.PrintName(os);
    return os;
  }

  Isolate* isolate() const { return isolate_; }

  void DeleteStubFromCacheForTesting();

 protected:
  CodeStub(uint32_t key, Isolate* isolate)
      : minor_key_(MinorKeyFromKey(key)), isolate_(isolate) {}

  // Generates the assembler code for the stub.
  virtual Handle<Code> GenerateCode() = 0;

  // Returns whether the code generated for this stub needs to be allocated as
  // a fixed (non-moveable) code object.
  virtual bool NeedsImmovableCode() { return false; }

  virtual void PrintName(std::ostream& os) const;        // NOLINT
  virtual void PrintBaseName(std::ostream& os) const;    // NOLINT
  virtual void PrintState(std::ostream& os) const { ; }  // NOLINT

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


#define DEFINE_CODE_STUB(NAME, SUPER)                      \
 public:                                                   \
  inline Major MajorKey() const override { return NAME; }; \
                                                           \
  DEFINE_CODE_STUB_BASE(NAME##Stub, SUPER)


#define DEFINE_PLATFORM_CODE_STUB(NAME, SUPER)  \
 private:                                       \
  void Generate(MacroAssembler* masm) override; \
  DEFINE_CODE_STUB(NAME, SUPER)


#define DEFINE_HYDROGEN_CODE_STUB(NAME, SUPER)                        \
 public:                                                              \
  void InitializeDescriptor(CodeStubDescriptor* descriptor) override; \
  Handle<Code> GenerateCode() override;                               \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_TURBOFAN_CODE_STUB(NAME, SUPER)                               \
 public:                                                                     \
  void GenerateAssembly(compiler::CodeAssemblerState* state) const override; \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_HANDLER_CODE_STUB(NAME, SUPER) \
 public:                                      \
  Handle<Code> GenerateCode() override;       \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_CALL_INTERFACE_DESCRIPTOR(NAME)                          \
 public:                                                                \
  typedef NAME##Descriptor Descriptor;                                  \
  CallInterfaceDescriptor GetCallInterfaceDescriptor() const override { \
    return Descriptor(isolate());                                       \
  }

// There are some code stubs we just can't describe right now with a
// CallInterfaceDescriptor. Isolate behavior for those cases with this macro.
// An attempt to retrieve a descriptor will fail.
#define DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR()                         \
 public:                                                                \
  CallInterfaceDescriptor GetCallInterfaceDescriptor() const override { \
    UNREACHABLE();                                                      \
    return CallInterfaceDescriptor();                                   \
  }


class PlatformCodeStub : public CodeStub {
 public:
  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GenerateCode() override;

 protected:
  explicit PlatformCodeStub(Isolate* isolate) : CodeStub(isolate) {}

  // Generates the assembler code for the stub.
  virtual void Generate(MacroAssembler* masm) = 0;

  DEFINE_CODE_STUB_BASE(PlatformCodeStub, CodeStub);
};


enum StubFunctionMode { NOT_JS_FUNCTION_STUB_MODE, JS_FUNCTION_STUB_MODE };


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
                  StubFunctionMode function_mode = NOT_JS_FUNCTION_STUB_MODE);

  void SetMissHandler(Runtime::FunctionId id) {
    miss_handler_id_ = id;
    miss_handler_ = ExternalReference(Runtime::FunctionForId(id), isolate_);
    has_miss_handler_ = true;
    // Our miss handler infrastructure doesn't currently support
    // variable stack parameter counts.
    DCHECK(!stack_parameter_count_.is_valid());
  }

  void set_call_descriptor(CallInterfaceDescriptor d) { call_descriptor_ = d; }
  CallInterfaceDescriptor call_descriptor() const { return call_descriptor_; }

  int GetRegisterParameterCount() const {
    return call_descriptor().GetRegisterParameterCount();
  }

  int GetStackParameterCount() const {
    return call_descriptor().GetStackParameterCount();
  }

  int GetParameterCount() const {
    return call_descriptor().GetParameterCount();
  }

  Register GetRegisterParameter(int index) const {
    return call_descriptor().GetRegisterParameter(index);
  }

  MachineType GetParameterType(int index) const {
    return call_descriptor().GetParameterType(index);
  }

  ExternalReference miss_handler() const {
    DCHECK(has_miss_handler_);
    return miss_handler_;
  }

  Runtime::FunctionId miss_handler_id() const {
    DCHECK(has_miss_handler_);
    return miss_handler_id_;
  }

  bool has_miss_handler() const {
    return has_miss_handler_;
  }

  int GetHandlerParameterCount() const {
    int params = GetParameterCount();
    if (PassesArgumentsToDeoptimizationHandler()) {
      params += 1;
    }
    return params;
  }

  int hint_stack_parameter_count() const { return hint_stack_parameter_count_; }
  Register stack_parameter_count() const { return stack_parameter_count_; }
  StubFunctionMode function_mode() const { return function_mode_; }
  Address deoptimization_handler() const { return deoptimization_handler_; }

 private:
  bool PassesArgumentsToDeoptimizationHandler() const {
    return stack_parameter_count_.is_valid();
  }

  Isolate* isolate_;
  CallInterfaceDescriptor call_descriptor_;
  Register stack_parameter_count_;
  // If hint_stack_parameter_count_ > 0, the code stub can optimize the
  // return sequence. Default value is -1, which means it is ignored.
  int hint_stack_parameter_count_;
  StubFunctionMode function_mode_;

  Address deoptimization_handler_;

  ExternalReference miss_handler_;
  Runtime::FunctionId miss_handler_id_;
  bool has_miss_handler_;
};


class HydrogenCodeStub : public CodeStub {
 public:
  enum InitializationState {
    UNINITIALIZED,
    INITIALIZED
  };

  template<class SubClass>
  static Handle<Code> GetUninitialized(Isolate* isolate) {
    SubClass::GenerateAheadOfTime(isolate);
    return SubClass().GetCode(isolate);
  }

  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GenerateCode() override = 0;

  bool IsUninitialized() const { return IsMissBits::decode(minor_key_); }

  Handle<Code> GenerateLightweightMissCode(ExternalReference miss);

  Handle<Code> GenerateRuntimeTailCall(CodeStubDescriptor* descriptor);

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


class TurboFanCodeStub : public CodeStub {
 public:
  // Retrieve the code for the stub. Generate the code if needed.
  Handle<Code> GenerateCode() override;

  int GetStackParameterCount() const override {
    return GetCallInterfaceDescriptor().GetStackParameterCount();
  }

 protected:
  explicit TurboFanCodeStub(Isolate* isolate) : CodeStub(isolate) {}

  virtual void GenerateAssembly(compiler::CodeAssemblerState* state) const = 0;

 private:
  DEFINE_CODE_STUB_BASE(TurboFanCodeStub, CodeStub);
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


}  // namespace internal
}  // namespace v8

#if V8_TARGET_ARCH_IA32
#include "src/ia32/code-stubs-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/x64/code-stubs-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/arm64/code-stubs-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/code-stubs-arm.h"
#elif V8_TARGET_ARCH_PPC
#include "src/ppc/code-stubs-ppc.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/mips/code-stubs-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/mips64/code-stubs-mips64.h"
#elif V8_TARGET_ARCH_S390
#include "src/s390/code-stubs-s390.h"
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

  void BeforeCall(MacroAssembler* masm) const override;

  void AfterCall(MacroAssembler* masm) const override;
};


// Trivial RuntimeCallHelper implementation.
class NopRuntimeCallHelper : public RuntimeCallHelper {
 public:
  NopRuntimeCallHelper() {}

  void BeforeCall(MacroAssembler* masm) const override {}

  void AfterCall(MacroAssembler* masm) const override {}
};


class StringLengthStub : public TurboFanCodeStub {
 public:
  explicit StringLengthStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StringLength, TurboFanCodeStub);
};

class StoreInterceptorStub : public TurboFanCodeStub {
 public:
  explicit StoreInterceptorStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreInterceptor, TurboFanCodeStub);
};

class LoadIndexedInterceptorStub : public TurboFanCodeStub {
 public:
  explicit LoadIndexedInterceptorStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::KEYED_LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_TURBOFAN_CODE_STUB(LoadIndexedInterceptor, TurboFanCodeStub);
};

// ES6 [[Get]] operation.
class GetPropertyStub : public TurboFanCodeStub {
 public:
  explicit GetPropertyStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(GetProperty);
  DEFINE_TURBOFAN_CODE_STUB(GetProperty, TurboFanCodeStub);
};

class NumberToStringStub final : public TurboFanCodeStub {
 public:
  explicit NumberToStringStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TypeConversion);
  DEFINE_TURBOFAN_CODE_STUB(NumberToString, TurboFanCodeStub);
};

class CreateAllocationSiteStub : public TurboFanCodeStub {
 public:
  explicit CreateAllocationSiteStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}
  static void GenerateAheadOfTime(Isolate* isolate);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CreateAllocationSite);
  DEFINE_TURBOFAN_CODE_STUB(CreateAllocationSite, TurboFanCodeStub);
};

class CreateWeakCellStub : public TurboFanCodeStub {
 public:
  explicit CreateWeakCellStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  static void GenerateAheadOfTime(Isolate* isolate);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CreateWeakCell);
  DEFINE_TURBOFAN_CODE_STUB(CreateWeakCell, TurboFanCodeStub);
};

class GrowArrayElementsStub : public TurboFanCodeStub {
 public:
  GrowArrayElementsStub(Isolate* isolate, ElementsKind kind)
      : TurboFanCodeStub(isolate) {
    minor_key_ = ElementsKindBits::encode(GetHoleyElementsKind(kind));
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(minor_key_);
  }

 private:
  class ElementsKindBits : public BitField<ElementsKind, 0, 8> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(GrowArrayElements);
  DEFINE_TURBOFAN_CODE_STUB(GrowArrayElements, TurboFanCodeStub);
};

enum AllocationSiteOverrideMode {
  DONT_OVERRIDE,
  DISABLE_ALLOCATION_SITES,
  LAST_ALLOCATION_SITE_OVERRIDE_MODE = DISABLE_ALLOCATION_SITES
};


class ArrayConstructorStub: public PlatformCodeStub {
 public:
  explicit ArrayConstructorStub(Isolate* isolate);

 private:
  void GenerateDispatchToArrayStub(MacroAssembler* masm,
                                   AllocationSiteOverrideMode mode);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayConstructor);
  DEFINE_PLATFORM_CODE_STUB(ArrayConstructor, PlatformCodeStub);
};


class InternalArrayConstructorStub: public PlatformCodeStub {
 public:
  explicit InternalArrayConstructorStub(Isolate* isolate);

 private:
  void GenerateCase(MacroAssembler* masm, ElementsKind kind);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayNArgumentsConstructor);
  DEFINE_PLATFORM_CODE_STUB(InternalArrayConstructor, PlatformCodeStub);
};


class MathPowStub: public PlatformCodeStub {
 public:
  enum ExponentType { INTEGER, DOUBLE, TAGGED };

  MathPowStub(Isolate* isolate, ExponentType exponent_type)
      : PlatformCodeStub(isolate) {
    minor_key_ = ExponentTypeBits::encode(exponent_type);
  }

  CallInterfaceDescriptor GetCallInterfaceDescriptor() const override {
    if (exponent_type() == TAGGED) {
      return MathPowTaggedDescriptor(isolate());
    } else if (exponent_type() == INTEGER) {
      return MathPowIntegerDescriptor(isolate());
    } else {
      // A CallInterfaceDescriptor doesn't specify double registers (yet).
      DCHECK_EQ(DOUBLE, exponent_type());
      return ContextOnlyDescriptor(isolate());
    }
  }

 private:
  ExponentType exponent_type() const {
    return ExponentTypeBits::decode(minor_key_);
  }

  class ExponentTypeBits : public BitField<ExponentType, 0, 2> {};

  DEFINE_PLATFORM_CODE_STUB(MathPow, PlatformCodeStub);
};

class CallICStub : public TurboFanCodeStub {
 public:
  CallICStub(Isolate* isolate, ConvertReceiverMode convert_mode,
             TailCallMode tail_call_mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = ConvertModeBits::encode(convert_mode) |
                 TailCallModeBits::encode(tail_call_mode);
  }

  ConvertReceiverMode convert_mode() const {
    return ConvertModeBits::decode(minor_key_);
  }
  TailCallMode tail_call_mode() const {
    return TailCallModeBits::decode(minor_key_);
  }

 protected:
  typedef BitField<ConvertReceiverMode, 0, 2> ConvertModeBits;
  typedef BitField<TailCallMode, ConvertModeBits::kNext, 1> TailCallModeBits;

 private:
  void PrintState(std::ostream& os) const final;  // NOLINT

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallIC);
  DEFINE_TURBOFAN_CODE_STUB(CallIC, TurboFanCodeStub);
};

class KeyedLoadSloppyArgumentsStub : public TurboFanCodeStub {
 public:
  explicit KeyedLoadSloppyArgumentsStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::LOAD_IC; }

 protected:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_TURBOFAN_CODE_STUB(KeyedLoadSloppyArguments, TurboFanCodeStub);
};


class CommonStoreModeBits : public BitField<KeyedAccessStoreMode, 0, 3> {};

class KeyedStoreSloppyArgumentsStub : public TurboFanCodeStub {
 public:
  explicit KeyedStoreSloppyArgumentsStub(Isolate* isolate,
                                         KeyedAccessStoreMode mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = CommonStoreModeBits::encode(mode);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

 protected:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(KeyedStoreSloppyArguments, TurboFanCodeStub);
};

class CallApiCallbackStub : public PlatformCodeStub {
 public:
  static const int kArgBits = 3;
  static const int kArgMax = (1 << kArgBits) - 1;

  // CallApiCallbackStub for regular setters and getters.
  CallApiCallbackStub(Isolate* isolate, bool is_store, bool is_lazy)
      : CallApiCallbackStub(isolate, is_store ? 1 : 0, is_store, is_lazy) {}

  // CallApiCallbackStub for callback functions.
  CallApiCallbackStub(Isolate* isolate, int argc, bool is_lazy)
      : CallApiCallbackStub(isolate, argc, false, is_lazy) {}

 private:
  CallApiCallbackStub(Isolate* isolate, int argc, bool is_store, bool is_lazy)
      : PlatformCodeStub(isolate) {
    CHECK(0 <= argc && argc <= kArgMax);
    minor_key_ = IsStoreBits::encode(is_store) |
                 ArgumentBits::encode(argc) |
                 IsLazyAccessorBits::encode(is_lazy);
  }

  bool is_store() const { return IsStoreBits::decode(minor_key_); }
  bool is_lazy() const { return IsLazyAccessorBits::decode(minor_key_); }
  int argc() const { return ArgumentBits::decode(minor_key_); }

  class IsStoreBits: public BitField<bool, 0, 1> {};
  class IsLazyAccessorBits : public BitField<bool, 1, 1> {};
  class ArgumentBits : public BitField<int, 2, kArgBits> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ApiCallback);
  DEFINE_PLATFORM_CODE_STUB(CallApiCallback, PlatformCodeStub);
};


class CallApiGetterStub : public PlatformCodeStub {
 public:
  explicit CallApiGetterStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ApiGetter);
  DEFINE_PLATFORM_CODE_STUB(CallApiGetter, PlatformCodeStub);
};


class BinaryOpICStub : public HydrogenCodeStub {
 public:
  BinaryOpICStub(Isolate* isolate, Token::Value op)
      : HydrogenCodeStub(isolate, UNINITIALIZED) {
    BinaryOpICState state(isolate, op);
    set_sub_minor_key(state.GetExtraICState());
  }

  BinaryOpICStub(Isolate* isolate, const BinaryOpICState& state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(state.GetExtraICState());
  }

  static void GenerateAheadOfTime(Isolate* isolate);

  Code::Kind GetCodeKind() const override { return Code::BINARY_OP_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(sub_minor_key());
  }

  BinaryOpICState state() const {
    return BinaryOpICState(isolate(), GetExtraICState());
  }

  void PrintState(std::ostream& os) const final;  // NOLINT

 private:
  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpICState& state);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_HYDROGEN_CODE_STUB(BinaryOpIC, HydrogenCodeStub);
};


// TODO(bmeurer): Merge this into the BinaryOpICStub once we have proper tail
// call support for stubs in Hydrogen.
class BinaryOpICWithAllocationSiteStub final : public PlatformCodeStub {
 public:
  BinaryOpICWithAllocationSiteStub(Isolate* isolate,
                                   const BinaryOpICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  static void GenerateAheadOfTime(Isolate* isolate);

  Handle<Code> GetCodeCopyFromTemplate(Handle<AllocationSite> allocation_site) {
    FindAndReplacePattern pattern;
    pattern.Add(isolate()->factory()->undefined_map(), allocation_site);
    return CodeStub::GetCodeCopy(pattern);
  }

  Code::Kind GetCodeKind() const override { return Code::BINARY_OP_IC; }

  ExtraICState GetExtraICState() const override {
    return static_cast<ExtraICState>(minor_key_);
  }

  void PrintState(std::ostream& os) const override;  // NOLINT

 private:
  BinaryOpICState state() const {
    return BinaryOpICState(isolate(), GetExtraICState());
  }

  static void GenerateAheadOfTime(Isolate* isolate,
                                  const BinaryOpICState& state);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithAllocationSite);
  DEFINE_PLATFORM_CODE_STUB(BinaryOpICWithAllocationSite, PlatformCodeStub);
};


class BinaryOpWithAllocationSiteStub final : public BinaryOpICStub {
 public:
  BinaryOpWithAllocationSiteStub(Isolate* isolate, Token::Value op)
      : BinaryOpICStub(isolate, op) {}

  BinaryOpWithAllocationSiteStub(Isolate* isolate, const BinaryOpICState& state)
      : BinaryOpICStub(isolate, state) {}

  Code::Kind GetCodeKind() const final { return Code::STUB; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithAllocationSite);
  DEFINE_HYDROGEN_CODE_STUB(BinaryOpWithAllocationSite, BinaryOpICStub);
};

class StringAddStub final : public TurboFanCodeStub {
 public:
  StringAddStub(Isolate* isolate, StringAddFlags flags,
                PretenureFlag pretenure_flag)
      : TurboFanCodeStub(isolate) {
    minor_key_ = (StringAddFlagsBits::encode(flags) |
                  PretenureFlagBits::encode(pretenure_flag));
  }

  StringAddFlags flags() const {
    return StringAddFlagsBits::decode(minor_key_);
  }

  PretenureFlag pretenure_flag() const {
    return PretenureFlagBits::decode(minor_key_);
  }

 private:
  class StringAddFlagsBits : public BitField<StringAddFlags, 0, 3> {};
  class PretenureFlagBits : public BitField<PretenureFlag, 3, 1> {};

  void PrintBaseName(std::ostream& os) const override;  // NOLINT

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StringAdd);
  DEFINE_TURBOFAN_CODE_STUB(StringAdd, TurboFanCodeStub);
};


class CompareICStub : public PlatformCodeStub {
 public:
  CompareICStub(Isolate* isolate, Token::Value op, CompareICState::State left,
                CompareICState::State right, CompareICState::State state)
      : PlatformCodeStub(isolate) {
    DCHECK(Token::IsCompareOp(op));
    DCHECK(OpBits::is_valid(op - Token::EQ));
    minor_key_ = OpBits::encode(op - Token::EQ) |
                 LeftStateBits::encode(left) | RightStateBits::encode(right) |
                 StateBits::encode(state);
  }
  // Creates uninitialized compare stub.
  CompareICStub(Isolate* isolate, Token::Value op)
      : CompareICStub(isolate, op, CompareICState::UNINITIALIZED,
                      CompareICState::UNINITIALIZED,
                      CompareICState::UNINITIALIZED) {}

  CompareICStub(Isolate* isolate, ExtraICState extra_ic_state)
      : PlatformCodeStub(isolate) {
    minor_key_ = extra_ic_state;
  }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

  void set_known_map(Handle<Map> map) { known_map_ = map; }

  InlineCacheState GetICState() const;

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
  Code::Kind GetCodeKind() const override { return Code::COMPARE_IC; }

  void GenerateBooleans(MacroAssembler* masm);
  void GenerateSmis(MacroAssembler* masm);
  void GenerateNumbers(MacroAssembler* masm);
  void GenerateInternalizedStrings(MacroAssembler* masm);
  void GenerateStrings(MacroAssembler* masm);
  void GenerateUniqueNames(MacroAssembler* masm);
  void GenerateReceivers(MacroAssembler* masm);
  void GenerateMiss(MacroAssembler* masm);
  void GenerateKnownReceivers(MacroAssembler* masm);
  void GenerateGeneric(MacroAssembler* masm);

  bool strict() const { return op() == Token::EQ_STRICT; }
  Condition GetCondition() const;

  // Although we don't cache anything in the special cache we have to define
  // this predicate to avoid appearance of code stubs with embedded maps in
  // the global stub cache.
  bool UseSpecialCache() override {
    return state() == CompareICState::KNOWN_RECEIVER;
  }

  class OpBits : public BitField<int, 0, 3> {};
  class LeftStateBits : public BitField<CompareICState::State, 3, 4> {};
  class RightStateBits : public BitField<CompareICState::State, 7, 4> {};
  class StateBits : public BitField<CompareICState::State, 11, 4> {};

  Handle<Map> known_map_;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_PLATFORM_CODE_STUB(CompareIC, PlatformCodeStub);
};


class CEntryStub : public PlatformCodeStub {
 public:
  CEntryStub(Isolate* isolate, int result_size,
             SaveFPRegsMode save_doubles = kDontSaveFPRegs,
             ArgvMode argv_mode = kArgvOnStack, bool builtin_exit_frame = false)
      : PlatformCodeStub(isolate) {
    minor_key_ = SaveDoublesBits::encode(save_doubles == kSaveFPRegs) |
                 FrameTypeBits::encode(builtin_exit_frame) |
                 ArgvMode::encode(argv_mode == kArgvInRegister);
    DCHECK(result_size == 1 || result_size == 2 || result_size == 3);
    minor_key_ = ResultSizeBits::update(minor_key_, result_size);
  }

  // The version of this stub that doesn't save doubles is generated ahead of
  // time, so it's OK to call it from other stubs that can't cope with GC during
  // their code generation.  On machines that always have gp registers (x64) we
  // can generate both variants ahead of time.
  static void GenerateAheadOfTime(Isolate* isolate);

 private:
  bool save_doubles() const { return SaveDoublesBits::decode(minor_key_); }
  bool argv_in_register() const { return ArgvMode::decode(minor_key_); }
  bool is_builtin_exit() const { return FrameTypeBits::decode(minor_key_); }
  int result_size() const { return ResultSizeBits::decode(minor_key_); }

  bool NeedsImmovableCode() override;

  class SaveDoublesBits : public BitField<bool, 0, 1> {};
  class ArgvMode : public BitField<bool, 1, 1> {};
  class FrameTypeBits : public BitField<bool, 2, 1> {};
  class ResultSizeBits : public BitField<int, 3, 3> {};

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
  void FinishCode(Handle<Code> code) override;

  void PrintName(std::ostream& os) const override {  // NOLINT
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

// TODO(bmeurer/mvstanton): Turn CallConstructStub into ConstructICStub.
class CallConstructStub final : public PlatformCodeStub {
 public:
  explicit CallConstructStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallConstruct);
  DEFINE_PLATFORM_CODE_STUB(CallConstruct, PlatformCodeStub);
};


enum ReceiverCheckMode {
  // We don't know anything about the receiver.
  RECEIVER_IS_UNKNOWN,

  // We know the receiver is a string.
  RECEIVER_IS_STRING
};


enum EmbedMode {
  // The code being generated is part of an IC handler, which may MISS
  // to an IC in failure cases.
  PART_OF_IC_HANDLER,

  NOT_PART_OF_IC_HANDLER
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
  StringCharCodeAtGenerator(Register object, Register index, Register result,
                            Label* receiver_not_string, Label* index_not_number,
                            Label* index_out_of_range,
                            ReceiverCheckMode check_mode = RECEIVER_IS_UNKNOWN)
      : object_(object),
        index_(index),
        result_(result),
        receiver_not_string_(receiver_not_string),
        index_not_number_(index_not_number),
        index_out_of_range_(index_out_of_range),
        check_mode_(check_mode) {
    DCHECK(!result_.is(object_));
    DCHECK(!result_.is(index_));
  }

  // Generates the fast case code. On the fallthrough path |result|
  // register contains the result.
  void GenerateFast(MacroAssembler* masm);

  // Generates the slow case code. Must not be naturally
  // reachable. Expected to be put after a ret instruction (e.g., in
  // deferred code). Always jumps back to the fast case.
  void GenerateSlow(MacroAssembler* masm, EmbedMode embed_mode,
                    const RuntimeCallHelper& call_helper);

 private:
  Register object_;
  Register index_;
  Register result_;

  Label* receiver_not_string_;
  Label* index_not_number_;
  Label* index_out_of_range_;

  ReceiverCheckMode check_mode_;

  Label call_runtime_;
  Label index_not_smi_;
  Label got_smi_index_;
  Label exit_;

  DISALLOW_COPY_AND_ASSIGN(StringCharCodeAtGenerator);
};

class CallICTrampolineStub : public CallICStub {
 public:
  CallICTrampolineStub(Isolate* isolate, ConvertReceiverMode convert_mode,
                       TailCallMode tail_call_mode)
      : CallICStub(isolate, convert_mode, tail_call_mode) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallICTrampoline);
  DEFINE_TURBOFAN_CODE_STUB(CallICTrampoline, CallICStub);
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

  bool SometimesSetsUpAFrame() override { return false; }

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

class ScriptContextFieldStub : public TurboFanCodeStub {
 public:
  ScriptContextFieldStub(Isolate* isolate,
                         const ScriptContextTable::LookupResult* lookup_result)
      : TurboFanCodeStub(isolate) {
    DCHECK(Accepted(lookup_result));
    minor_key_ = ContextIndexBits::encode(lookup_result->context_index) |
                 SlotIndexBits::encode(lookup_result->slot_index);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }

  int context_index() const { return ContextIndexBits::decode(minor_key_); }

  int slot_index() const { return SlotIndexBits::decode(minor_key_); }

  static bool Accepted(const ScriptContextTable::LookupResult* lookup_result) {
    return ContextIndexBits::is_valid(lookup_result->context_index) &&
           SlotIndexBits::is_valid(lookup_result->slot_index);
  }

 private:
  static const int kContextIndexBits = 9;
  static const int kSlotIndexBits = 12;
  class ContextIndexBits : public BitField<int, 0, kContextIndexBits> {};
  class SlotIndexBits
      : public BitField<int, kContextIndexBits, kSlotIndexBits> {};

  DEFINE_CODE_STUB_BASE(ScriptContextFieldStub, TurboFanCodeStub);
};


class LoadScriptContextFieldStub : public ScriptContextFieldStub {
 public:
  LoadScriptContextFieldStub(
      Isolate* isolate, const ScriptContextTable::LookupResult* lookup_result)
      : ScriptContextFieldStub(isolate, lookup_result) {}

  ExtraICState GetExtraICState() const override { return Code::LOAD_IC; }

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_TURBOFAN_CODE_STUB(LoadScriptContextField, ScriptContextFieldStub);
};


class StoreScriptContextFieldStub : public ScriptContextFieldStub {
 public:
  StoreScriptContextFieldStub(
      Isolate* isolate, const ScriptContextTable::LookupResult* lookup_result)
      : ScriptContextFieldStub(isolate, lookup_result) {}

  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreScriptContextField, ScriptContextFieldStub);
};

class StoreFastElementStub : public TurboFanCodeStub {
 public:
  StoreFastElementStub(Isolate* isolate, bool is_js_array,
                       ElementsKind elements_kind, KeyedAccessStoreMode mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = CommonStoreModeBits::encode(mode) |
                 ElementsKindBits::encode(elements_kind) |
                 IsJSArrayBits::encode(is_js_array);
  }

  static void GenerateAheadOfTime(Isolate* isolate);

  bool is_js_array() const { return IsJSArrayBits::decode(minor_key_); }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(minor_key_);
  }

  KeyedAccessStoreMode store_mode() const {
    return CommonStoreModeBits::decode(minor_key_);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::KEYED_STORE_IC; }

 private:
  class ElementsKindBits
      : public BitField<ElementsKind, CommonStoreModeBits::kNext, 8> {};
  class IsJSArrayBits : public BitField<bool, ElementsKindBits::kNext, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreFastElement, TurboFanCodeStub);
};


class TransitionElementsKindStub : public HydrogenCodeStub {
 public:
  TransitionElementsKindStub(Isolate* isolate, ElementsKind from_kind,
                             ElementsKind to_kind)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(FromKindBits::encode(from_kind) |
                      ToKindBits::encode(to_kind));
  }

  ElementsKind from_kind() const {
    return FromKindBits::decode(sub_minor_key());
  }

  ElementsKind to_kind() const { return ToKindBits::decode(sub_minor_key()); }

 private:
  class FromKindBits: public BitField<ElementsKind, 8, 8> {};
  class ToKindBits: public BitField<ElementsKind, 0, 8> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TransitionElementsKind);
  DEFINE_HYDROGEN_CODE_STUB(TransitionElementsKind, HydrogenCodeStub);
};

class AllocateHeapNumberStub : public TurboFanCodeStub {
 public:
  explicit AllocateHeapNumberStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  void InitializeDescriptor(CodeStubDescriptor* descriptor) override;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(AllocateHeapNumber);
  DEFINE_TURBOFAN_CODE_STUB(AllocateHeapNumber, TurboFanCodeStub);
};

class CommonArrayConstructorStub : public TurboFanCodeStub {
 protected:
  CommonArrayConstructorStub(Isolate* isolate, ElementsKind kind,
                             AllocationSiteOverrideMode override_mode)
      : TurboFanCodeStub(isolate) {
    // It only makes sense to override local allocation site behavior
    // if there is a difference between the global allocation site policy
    // for an ElementsKind and the desired usage of the stub.
    DCHECK(override_mode != DISABLE_ALLOCATION_SITES ||
           AllocationSite::GetMode(kind) == TRACK_ALLOCATION_SITE);
    set_sub_minor_key(ElementsKindBits::encode(kind) |
                      AllocationSiteOverrideModeBits::encode(override_mode));
  }

  void set_sub_minor_key(uint32_t key) { minor_key_ = key; }

  uint32_t sub_minor_key() const { return minor_key_; }

  CommonArrayConstructorStub(uint32_t key, Isolate* isolate)
      : TurboFanCodeStub(key, isolate) {}

 public:
  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

  AllocationSiteOverrideMode override_mode() const {
    return AllocationSiteOverrideModeBits::decode(sub_minor_key());
  }

  static void GenerateStubsAheadOfTime(Isolate* isolate);

 private:
  // Ensure data fits within available bits.
  STATIC_ASSERT(LAST_ALLOCATION_SITE_OVERRIDE_MODE == 1);

  class ElementsKindBits : public BitField<ElementsKind, 0, 8> {};
  class AllocationSiteOverrideModeBits
      : public BitField<AllocationSiteOverrideMode, 8, 1> {};  // NOLINT
};

class ArrayNoArgumentConstructorStub : public CommonArrayConstructorStub {
 public:
  ArrayNoArgumentConstructorStub(
      Isolate* isolate, ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : CommonArrayConstructorStub(isolate, kind, override_mode) {}

 private:
  void PrintName(std::ostream& os) const override {  // NOLINT
    os << "ArrayNoArgumentConstructorStub";
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayNoArgumentConstructor);
  DEFINE_TURBOFAN_CODE_STUB(ArrayNoArgumentConstructor,
                            CommonArrayConstructorStub);
};

class InternalArrayNoArgumentConstructorStub
    : public CommonArrayConstructorStub {
 public:
  InternalArrayNoArgumentConstructorStub(Isolate* isolate, ElementsKind kind)
      : CommonArrayConstructorStub(isolate, kind, DONT_OVERRIDE) {}

 private:
  void PrintName(std::ostream& os) const override {  // NOLINT
    os << "InternalArrayNoArgumentConstructorStub";
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayNoArgumentConstructor);
  DEFINE_TURBOFAN_CODE_STUB(InternalArrayNoArgumentConstructor,
                            CommonArrayConstructorStub);
};

class ArraySingleArgumentConstructorStub : public CommonArrayConstructorStub {
 public:
  ArraySingleArgumentConstructorStub(
      Isolate* isolate, ElementsKind kind,
      AllocationSiteOverrideMode override_mode = DONT_OVERRIDE)
      : CommonArrayConstructorStub(isolate, kind, override_mode) {}

 private:
  void PrintName(std::ostream& os) const override {  // NOLINT
    os << "ArraySingleArgumentConstructorStub";
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArraySingleArgumentConstructor);
  DEFINE_TURBOFAN_CODE_STUB(ArraySingleArgumentConstructor,
                            CommonArrayConstructorStub);
};

class InternalArraySingleArgumentConstructorStub
    : public CommonArrayConstructorStub {
 public:
  InternalArraySingleArgumentConstructorStub(Isolate* isolate,
                                             ElementsKind kind)
      : CommonArrayConstructorStub(isolate, kind, DONT_OVERRIDE) {}

 private:
  void PrintName(std::ostream& os) const override {  // NOLINT
    os << "InternalArraySingleArgumentConstructorStub";
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArraySingleArgumentConstructor);
  DEFINE_TURBOFAN_CODE_STUB(InternalArraySingleArgumentConstructor,
                            CommonArrayConstructorStub);
};

class ArrayNArgumentsConstructorStub : public PlatformCodeStub {
 public:
  explicit ArrayNArgumentsConstructorStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  CallInterfaceDescriptor GetCallInterfaceDescriptor() const override {
    return ArrayNArgumentsConstructorDescriptor(isolate());
  }

 private:
  DEFINE_PLATFORM_CODE_STUB(ArrayNArgumentsConstructor, PlatformCodeStub);
};

class StoreSlowElementStub : public TurboFanCodeStub {
 public:
  StoreSlowElementStub(Isolate* isolate, KeyedAccessStoreMode mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = CommonStoreModeBits::encode(mode);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::KEYED_STORE_IC; }

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreSlowElement, TurboFanCodeStub);
};

class ToBooleanICStub : public HydrogenCodeStub {
 public:
  ToBooleanICStub(Isolate* isolate, ExtraICState state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(HintsBits::encode(static_cast<uint16_t>(state)));
  }

  bool UpdateStatus(Handle<Object> object);
  ToBooleanHints hints() const {
    return ToBooleanHints(HintsBits::decode(sub_minor_key()));
  }

  Code::Kind GetCodeKind() const override { return Code::TO_BOOLEAN_IC; }
  void PrintState(std::ostream& os) const override;  // NOLINT

  bool SometimesSetsUpAFrame() override { return false; }

  static Handle<Code> GetUninitialized(Isolate* isolate) {
    return ToBooleanICStub(isolate, UNINITIALIZED).GetCode();
  }

  ExtraICState GetExtraICState() const override { return hints(); }

  InlineCacheState GetICState() const {
    if (hints() == ToBooleanHint::kNone) {
      return ::v8::internal::UNINITIALIZED;
    } else {
      return MONOMORPHIC;
    }
  }

 private:
  ToBooleanICStub(Isolate* isolate, InitializationState init_state)
      : HydrogenCodeStub(isolate, init_state) {}

  static const int kNumHints = 8;
  STATIC_ASSERT(static_cast<int>(ToBooleanHint::kAny) ==
                ((1 << kNumHints) - 1));
  class HintsBits : public BitField<uint16_t, 0, kNumHints> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TypeConversion);
  DEFINE_HYDROGEN_CODE_STUB(ToBooleanIC, HydrogenCodeStub);
};

class ElementsTransitionAndStoreStub : public TurboFanCodeStub {
 public:
  ElementsTransitionAndStoreStub(Isolate* isolate, ElementsKind from_kind,
                                 ElementsKind to_kind, bool is_jsarray,
                                 KeyedAccessStoreMode store_mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = CommonStoreModeBits::encode(store_mode) |
                 FromBits::encode(from_kind) | ToBits::encode(to_kind) |
                 IsJSArrayBits::encode(is_jsarray);
  }

  ElementsKind from_kind() const { return FromBits::decode(minor_key_); }
  ElementsKind to_kind() const { return ToBits::decode(minor_key_); }
  bool is_jsarray() const { return IsJSArrayBits::decode(minor_key_); }
  KeyedAccessStoreMode store_mode() const {
    return CommonStoreModeBits::decode(minor_key_);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::KEYED_STORE_IC; }

 private:
  class FromBits
      : public BitField<ElementsKind, CommonStoreModeBits::kNext, 8> {};
  class ToBits : public BitField<ElementsKind, 11, 8> {};
  class IsJSArrayBits : public BitField<bool, 19, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreTransition);
  DEFINE_TURBOFAN_CODE_STUB(ElementsTransitionAndStore, TurboFanCodeStub);
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
  bool SometimesSetsUpAFrame() override { return false; }

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
  bool SometimesSetsUpAFrame() override { return false; }

 private:
  bool save_doubles() const { return SaveDoublesBits::decode(minor_key_); }

  class SaveDoublesBits : public BitField<bool, 0, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(StoreBufferOverflow, PlatformCodeStub);
};

class SubStringStub : public TurboFanCodeStub {
 public:
  explicit SubStringStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(SubString);
  DEFINE_TURBOFAN_CODE_STUB(SubString, TurboFanCodeStub);
};


#undef DEFINE_CALL_INTERFACE_DESCRIPTOR
#undef DEFINE_PLATFORM_CODE_STUB
#undef DEFINE_HANDLER_CODE_STUB
#undef DEFINE_HYDROGEN_CODE_STUB
#undef DEFINE_CODE_STUB
#undef DEFINE_CODE_STUB_BASE

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_STUBS_H_

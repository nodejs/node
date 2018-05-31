// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/globals.h"
#include "src/heap/factory.h"
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
#define CODE_STUB_LIST_ALL_PLATFORMS(V)     \
  /* --- PlatformCodeStubs --- */           \
  V(ArrayConstructor)                       \
  V(CallApiCallback)                        \
  V(CallApiGetter)                          \
  V(CEntry)                                 \
  V(DoubleToI)                              \
  V(InternalArrayConstructor)               \
  V(JSEntry)                                \
  V(MathPow)                                \
  V(ProfileEntryHook)                       \
  /* --- TurboFanCodeStubs --- */           \
  V(StoreSlowElement)                       \
  V(StoreInArrayLiteralSlow)                \
  V(ArrayNoArgumentConstructor)             \
  V(ArraySingleArgumentConstructor)         \
  V(ArrayNArgumentsConstructor)             \
  V(InternalArrayNoArgumentConstructor)     \
  V(InternalArraySingleArgumentConstructor) \
  V(ElementsTransitionAndStore)             \
  V(KeyedLoadSloppyArguments)               \
  V(KeyedStoreSloppyArguments)              \
  V(StringAdd)                              \
  V(GetProperty)                            \
  V(StoreFastElement)                       \
  V(StoreInterceptor)                       \
  V(TransitionElementsKind)                 \
  V(LoadIndexedInterceptor)

// List of code stubs only used on ARM 32 bits platforms.
#if V8_TARGET_ARCH_ARM
#define CODE_STUB_LIST_ARM(V) V(DirectCEntry)

#else
#define CODE_STUB_LIST_ARM(V)
#endif

// List of code stubs only used on ARM 64 bits platforms.
#if V8_TARGET_ARCH_ARM64
#define CODE_STUB_LIST_ARM64(V) V(DirectCEntry)

#else
#define CODE_STUB_LIST_ARM64(V)
#endif

// List of code stubs only used on PPC platforms.
#ifdef V8_TARGET_ARCH_PPC
#define CODE_STUB_LIST_PPC(V) V(DirectCEntry)
#else
#define CODE_STUB_LIST_PPC(V)
#endif

// List of code stubs only used on MIPS platforms.
#if V8_TARGET_ARCH_MIPS
#define CODE_STUB_LIST_MIPS(V) V(DirectCEntry)
#elif V8_TARGET_ARCH_MIPS64
#define CODE_STUB_LIST_MIPS(V) V(DirectCEntry)
#else
#define CODE_STUB_LIST_MIPS(V)
#endif

// List of code stubs only used on S390 platforms.
#ifdef V8_TARGET_ARCH_S390
#define CODE_STUB_LIST_S390(V) V(DirectCEntry)
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

class CodeStub : public ZoneObject {
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

  static Major MajorKeyFromKey(uint32_t key) {
    return static_cast<Major>(MajorKeyBits::decode(key));
  }
  static uint32_t MinorKeyFromKey(uint32_t key) {
    return MinorKeyBits::decode(key);
  }

  // Gets the major key from a code object that is a code stub or binary op IC.
  static Major GetMajorKey(Code* code_stub);

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

  friend std::ostream& operator<<(std::ostream& os, const CodeStub& s) {
    s.PrintName(os);
    return os;
  }

  Isolate* isolate() const { return isolate_; }
  void set_isolate(Isolate* isolate) {
    DCHECK_NOT_NULL(isolate);
    DCHECK(isolate_ == nullptr || isolate_ == isolate);
    isolate_ = isolate;
  }

  void DeleteStubFromCacheForTesting();

 protected:
  CodeStub(uint32_t key, Isolate* isolate)
      : minor_key_(MinorKeyFromKey(key)), isolate_(isolate) {}

  // Generates the assembler code for the stub.
  virtual Handle<Code> GenerateCode() = 0;

  // Returns whether the code generated for this stub needs to be allocated as
  // a fixed (non-moveable) code object.
  virtual Movability NeedsImmovableCode() { return kMovable; }

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

  // Activate newly generated stub. Is called after
  // registering stub in the stub cache.
  virtual void Activate(Code* code) { }

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


#define DEFINE_TURBOFAN_CODE_STUB(NAME, SUPER)                               \
 public:                                                                     \
  void GenerateAssembly(compiler::CodeAssemblerState* state) const override; \
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

  // Generates the exception handler table for the stub.
  virtual int GenerateHandlerTable(MacroAssembler* masm);

  DEFINE_CODE_STUB_BASE(PlatformCodeStub, CodeStub);
};


enum StubFunctionMode { NOT_JS_FUNCTION_STUB_MODE, JS_FUNCTION_STUB_MODE };


class CodeStubDescriptor {
 public:
  explicit CodeStubDescriptor(CodeStub* stub);

  CodeStubDescriptor(Isolate* isolate, uint32_t stub_key);

  void Initialize(Address deoptimization_handler = nullptr,
                  int hint_stack_parameter_count = -1,
                  StubFunctionMode function_mode = NOT_JS_FUNCTION_STUB_MODE);
  void Initialize(Register stack_parameter_count,
                  Address deoptimization_handler = nullptr,
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

}  // namespace internal
}  // namespace v8

#if V8_TARGET_ARCH_IA32
#elif V8_TARGET_ARCH_X64
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
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

class StoreInterceptorStub : public TurboFanCodeStub {
 public:
  explicit StoreInterceptorStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreInterceptor, TurboFanCodeStub);
};

class TransitionElementsKindStub : public TurboFanCodeStub {
 public:
  TransitionElementsKindStub(Isolate* isolate, ElementsKind from_kind,
                             ElementsKind to_kind, bool is_jsarray)
      : TurboFanCodeStub(isolate) {
    set_sub_minor_key(FromKindBits::encode(from_kind) |
                      ToKindBits::encode(to_kind) |
                      IsJSArrayBits::encode(is_jsarray));
  }

  void set_sub_minor_key(uint32_t key) { minor_key_ = key; }

  uint32_t sub_minor_key() const { return minor_key_; }

  ElementsKind from_kind() const {
    return FromKindBits::decode(sub_minor_key());
  }

  ElementsKind to_kind() const { return ToKindBits::decode(sub_minor_key()); }

  bool is_jsarray() const { return IsJSArrayBits::decode(sub_minor_key()); }

 private:
  class ToKindBits : public BitField<ElementsKind, 0, 8> {};
  class FromKindBits : public BitField<ElementsKind, ToKindBits::kNext, 8> {};
  class IsJSArrayBits : public BitField<bool, FromKindBits::kNext, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TransitionElementsKind);
  DEFINE_TURBOFAN_CODE_STUB(TransitionElementsKind, TurboFanCodeStub);
};

class LoadIndexedInterceptorStub : public TurboFanCodeStub {
 public:
  explicit LoadIndexedInterceptorStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

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
  MathPowStub() : PlatformCodeStub(nullptr) {}

  CallInterfaceDescriptor GetCallInterfaceDescriptor() const override {
    // A CallInterfaceDescriptor doesn't specify double registers (yet).
    return ContextOnlyDescriptor(isolate());
  }

 private:
  DEFINE_PLATFORM_CODE_STUB(MathPow, PlatformCodeStub);
};

class KeyedLoadSloppyArgumentsStub : public TurboFanCodeStub {
 public:
  explicit KeyedLoadSloppyArgumentsStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

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

 protected:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(KeyedStoreSloppyArguments, TurboFanCodeStub);
};

class CallApiCallbackStub : public PlatformCodeStub {
 public:
  static const int kArgBits = 7;
  static const int kArgMax = (1 << kArgBits) - 1;

  CallApiCallbackStub(Isolate* isolate, int argc)
      : PlatformCodeStub(isolate) {
    CHECK_LE(0, argc);
    CHECK_LE(argc, kArgMax);
    minor_key_ = ArgumentBits::encode(argc);
  }

 private:
  int argc() const { return ArgumentBits::decode(minor_key_); }

  class ArgumentBits : public BitField<int, 0, kArgBits> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ApiCallback);
  DEFINE_PLATFORM_CODE_STUB(CallApiCallback, PlatformCodeStub);
};


class CallApiGetterStub : public PlatformCodeStub {
 public:
  explicit CallApiGetterStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ApiGetter);
  DEFINE_PLATFORM_CODE_STUB(CallApiGetter, PlatformCodeStub);
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


class CEntryStub : public PlatformCodeStub {
 public:
  CEntryStub(Isolate* isolate, int result_size,
             SaveFPRegsMode save_doubles = kDontSaveFPRegs,
             ArgvMode argv_mode = kArgvOnStack, bool builtin_exit_frame = false)
      : PlatformCodeStub(isolate) {
    minor_key_ = SaveDoublesBits::encode(save_doubles == kSaveFPRegs) |
                 FrameTypeBits::encode(builtin_exit_frame) |
                 ArgvMode::encode(argv_mode == kArgvInRegister);
    DCHECK(result_size == 1 || result_size == 2);
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

  Movability NeedsImmovableCode() override;

  class SaveDoublesBits : public BitField<bool, 0, 1> {};
  class ArgvMode : public BitField<bool, 1, 1> {};
  class FrameTypeBits : public BitField<bool, 2, 1> {};
  class ResultSizeBits : public BitField<int, 3, 3> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(CEntry, PlatformCodeStub);
};

class JSEntryStub : public PlatformCodeStub {
 public:
  enum class SpecialTarget { kNone, kRunMicrotasks };
  JSEntryStub(Isolate* isolate, StackFrame::Type type)
      : PlatformCodeStub(isolate) {
    DCHECK(type == StackFrame::ENTRY || type == StackFrame::CONSTRUCT_ENTRY);
    minor_key_ = StackFrameTypeBits::encode(type) |
                 SpecialTargetBits::encode(SpecialTarget::kNone);
  }

  JSEntryStub(Isolate* isolate, SpecialTarget target)
      : PlatformCodeStub(isolate) {
    minor_key_ = StackFrameTypeBits::encode(StackFrame::ENTRY) |
                 SpecialTargetBits::encode(target);
  }

 private:
  int GenerateHandlerTable(MacroAssembler* masm) override;

  void PrintName(std::ostream& os) const override {  // NOLINT
    os << (type() == StackFrame::ENTRY ? "JSEntryStub"
                                       : "JSConstructEntryStub");
  }

  StackFrame::Type type() const {
    return StackFrameTypeBits::decode(minor_key_);
  }

  SpecialTarget special_target() const {
    return SpecialTargetBits::decode(minor_key_);
  }

  Handle<Code> EntryTrampoline() {
    switch (special_target()) {
      case SpecialTarget::kNone:
        return (type() == StackFrame::CONSTRUCT_ENTRY)
                   ? BUILTIN_CODE(isolate(), JSConstructEntryTrampoline)
                   : BUILTIN_CODE(isolate(), JSEntryTrampoline);
      case SpecialTarget::kRunMicrotasks:
        return BUILTIN_CODE(isolate(), RunMicrotasks);
    }
    UNREACHABLE();
    return Handle<Code>();
  }

  class StackFrameTypeBits : public BitField<StackFrame::Type, 0, 5> {};
  class SpecialTargetBits
      : public BitField<SpecialTarget, StackFrameTypeBits::kNext, 1> {};

  int handler_offset_;

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(JSEntry, PlatformCodeStub);
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

class DoubleToIStub : public PlatformCodeStub {
 public:
  DoubleToIStub(Isolate* isolate, Register destination)
      : PlatformCodeStub(isolate) {
    minor_key_ = DestinationRegisterBits::encode(destination.code()) |
                 SSE3Bits::encode(CpuFeatures::IsSupported(SSE3) ? 1 : 0);
  }

  bool SometimesSetsUpAFrame() override { return false; }

 private:
  Register destination() const {
    return Register::from_code(DestinationRegisterBits::decode(minor_key_));
  }

  static const int kBitsPerRegisterNumber = 6;
  STATIC_ASSERT((1L << kBitsPerRegisterNumber) >= Register::kNumRegisters);
  class DestinationRegisterBits
      : public BitField<int, 0, kBitsPerRegisterNumber> {};
  class SSE3Bits : public BitField<int, kBitsPerRegisterNumber, 1> {};

  DEFINE_NULL_CALL_INTERFACE_DESCRIPTOR();
  DEFINE_PLATFORM_CODE_STUB(DoubleToI, PlatformCodeStub);
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

 private:
  class ElementsKindBits
      : public BitField<ElementsKind, CommonStoreModeBits::kNext, 8> {};
  class IsJSArrayBits : public BitField<bool, ElementsKindBits::kNext, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreFastElement, TurboFanCodeStub);
};


class CommonArrayConstructorStub : public TurboFanCodeStub {
 protected:
  CommonArrayConstructorStub(Isolate* isolate, ElementsKind kind,
                             AllocationSiteOverrideMode override_mode);

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

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreSlowElement, TurboFanCodeStub);
};

class StoreInArrayLiteralSlowStub : public TurboFanCodeStub {
 public:
  StoreInArrayLiteralSlowStub(Isolate* isolate, KeyedAccessStoreMode mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = CommonStoreModeBits::encode(mode);
  }

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreInArrayLiteralSlow, TurboFanCodeStub);
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

 private:
  class FromBits
      : public BitField<ElementsKind, CommonStoreModeBits::kNext, 8> {};
  class ToBits : public BitField<ElementsKind, 11, 8> {};
  class IsJSArrayBits : public BitField<bool, 19, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreTransition);
  DEFINE_TURBOFAN_CODE_STUB(ElementsTransitionAndStore, TurboFanCodeStub);
};


class ProfileEntryHookStub : public PlatformCodeStub {
 public:
  explicit ProfileEntryHookStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  // The profile entry hook function is not allowed to cause a GC.
  bool SometimesSetsUpAFrame() override { return false; }

  // Generates a call to the entry hook if it's enabled.
  static void MaybeCallEntryHook(MacroAssembler* masm);
  static void MaybeCallEntryHookDelayed(TurboAssembler* tasm, Zone* zone);

 private:
  static void EntryHookTrampoline(intptr_t function,
                                  intptr_t stack_pointer,
                                  Isolate* isolate);

  // ProfileEntryHookStub is called at the start of a function, so it has the
  // same register set.
  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallFunction)
  DEFINE_PLATFORM_CODE_STUB(ProfileEntryHook, PlatformCodeStub);
};


#undef DEFINE_CALL_INTERFACE_DESCRIPTOR
#undef DEFINE_PLATFORM_CODE_STUB
#undef DEFINE_CODE_STUB
#undef DEFINE_CODE_STUB_BASE

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_STUBS_H_

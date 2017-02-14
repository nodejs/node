// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUBS_H_
#define V8_CODE_STUBS_H_

#include "src/allocation.h"
#include "src/assembler.h"
#include "src/code-stub-assembler.h"
#include "src/codegen.h"
#include "src/globals.h"
#include "src/ic/ic-state.h"
#include "src/interface-descriptors.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

class ObjectLiteral;

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
  V(FunctionPrototype)                        \
  V(InternalArrayConstructor)                 \
  V(JSEntry)                                  \
  V(LoadIndexedString)                        \
  V(MathPow)                                  \
  V(ProfileEntryHook)                         \
  V(RecordWrite)                              \
  V(RegExpExec)                               \
  V(StoreBufferOverflow)                      \
  V(StoreElement)                             \
  V(SubString)                                \
  V(StoreIC)                                  \
  V(KeyedStoreIC)                             \
  V(KeyedLoadIC)                              \
  V(LoadIC)                                   \
  V(LoadGlobalIC)                             \
  V(FastNewObject)                            \
  V(FastNewRestParameter)                     \
  V(FastNewSloppyArguments)                   \
  V(FastNewStrictArguments)                   \
  V(NameDictionaryLookup)                     \
  /* This can be removed once there are no */ \
  /* more deopting Hydrogen stubs. */         \
  V(StubFailureTrampoline)                    \
  /* These are only called from FCG */        \
  /* They can be removed when only the TF  */ \
  /* version of the corresponding stub is  */ \
  /* used universally */                      \
  V(CallICTrampoline)                         \
  V(LoadICTrampoline)                         \
  V(KeyedLoadICTrampoline)                    \
  V(KeyedStoreICTrampoline)                   \
  V(StoreICTrampoline)                        \
  /* --- HydrogenCodeStubs --- */             \
  V(NumberToString)                           \
  V(StringAdd)                                \
  /* These builtins w/ JS linkage are */      \
  /* just fast-cases of C++ builtins. They */ \
  /* require varg support from TF */          \
  V(FastArrayPush)                            \
  V(FastFunctionBind)                         \
  /* These will be ported/eliminated */       \
  /* as part of the new IC system, ask */     \
  /* ishell before doing anything  */         \
  V(KeyedLoadGeneric)                         \
  V(LoadConstant)                             \
  V(LoadDictionaryElement)                    \
  V(LoadFastElement)                          \
  V(LoadField)                                \
  /* These should never be ported to TF */    \
  /* because they are either used only by */  \
  /* FCG/Crankshaft or are deprecated */      \
  V(BinaryOpIC)                               \
  V(BinaryOpWithAllocationSite)               \
  V(ToBooleanIC)                              \
  V(RegExpConstructResult)                    \
  V(TransitionElementsKind)                   \
  V(StoreGlobalViaContext)                    \
  /* --- TurboFanCodeStubs --- */             \
  V(AllocateHeapNumber)                       \
  V(AllocateFloat32x4)                        \
  V(AllocateInt32x4)                          \
  V(AllocateUint32x4)                         \
  V(AllocateBool32x4)                         \
  V(AllocateInt16x8)                          \
  V(AllocateUint16x8)                         \
  V(AllocateBool16x8)                         \
  V(AllocateInt8x16)                          \
  V(AllocateUint8x16)                         \
  V(AllocateBool8x16)                         \
  V(ArrayNoArgumentConstructor)               \
  V(ArraySingleArgumentConstructor)           \
  V(ArrayNArgumentsConstructor)               \
  V(CreateAllocationSite)                     \
  V(CreateWeakCell)                           \
  V(StringLength)                             \
  V(Add)                                      \
  V(AddWithFeedback)                          \
  V(Subtract)                                 \
  V(SubtractWithFeedback)                     \
  V(Multiply)                                 \
  V(MultiplyWithFeedback)                     \
  V(Divide)                                   \
  V(DivideWithFeedback)                       \
  V(Modulus)                                  \
  V(ModulusWithFeedback)                      \
  V(ShiftRight)                               \
  V(ShiftRightLogical)                        \
  V(ShiftLeft)                                \
  V(BitwiseAnd)                               \
  V(BitwiseOr)                                \
  V(BitwiseXor)                               \
  V(Inc)                                      \
  V(InternalArrayNoArgumentConstructor)       \
  V(InternalArraySingleArgumentConstructor)   \
  V(Dec)                                      \
  V(ElementsTransitionAndStore)               \
  V(FastCloneRegExp)                          \
  V(FastCloneShallowArray)                    \
  V(FastCloneShallowObject)                   \
  V(FastNewClosure)                           \
  V(FastNewFunctionContext)                   \
  V(InstanceOf)                               \
  V(LessThan)                                 \
  V(LessThanOrEqual)                          \
  V(GreaterThan)                              \
  V(GreaterThanOrEqual)                       \
  V(Equal)                                    \
  V(NotEqual)                                 \
  V(KeyedLoadSloppyArguments)                 \
  V(KeyedStoreSloppyArguments)                \
  V(LoadScriptContextField)                   \
  V(StoreScriptContextField)                  \
  V(StrictEqual)                              \
  V(StrictNotEqual)                           \
  V(ToInteger)                                \
  V(ToLength)                                 \
  V(HasProperty)                              \
  V(ForInFilter)                              \
  V(GetProperty)                              \
  V(LoadICTF)                                 \
  V(KeyedLoadICTF)                            \
  V(StoreFastElement)                         \
  V(StoreField)                               \
  V(StoreGlobal)                              \
  V(StoreICTF)                                \
  V(StoreInterceptor)                         \
  V(StoreMap)                                 \
  V(StoreTransition)                          \
  V(LoadApiGetter)                            \
  V(LoadIndexedInterceptor)                   \
  V(GrowArrayElements)                        \
  V(ToObject)                                 \
  V(Typeof)                                   \
  /* These are only called from FGC and */    \
  /* can be removed when we use ignition */   \
  /* only */                                  \
  V(LoadICTrampolineTF)                       \
  V(LoadGlobalICTrampoline)                   \
  V(KeyedLoadICTrampolineTF)                  \
  V(StoreICTrampolineTF)

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
 protected:                                                \
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

#define DEFINE_TURBOFAN_CODE_STUB(NAME, SUPER)                        \
 public:                                                              \
  void GenerateAssembly(CodeStubAssembler* assembler) const override; \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(NAME, SUPER)                       \
 public:                                                                       \
  static compiler::Node* Generate(CodeStubAssembler* assembler,                \
                                  compiler::Node* left, compiler::Node* right, \
                                  compiler::Node* context);                    \
  void GenerateAssembly(CodeStubAssembler* assembler) const override {         \
    assembler->Return(Generate(assembler, assembler->Parameter(0),             \
                               assembler->Parameter(1),                        \
                               assembler->Parameter(2)));                      \
  }                                                                            \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_TURBOFAN_BINARY_OP_CODE_STUB_WITH_FEEDBACK(NAME, SUPER)        \
 public:                                                                      \
  static compiler::Node* Generate(                                            \
      CodeStubAssembler* assembler, compiler::Node* left,                     \
      compiler::Node* right, compiler::Node* slot_id,                         \
      compiler::Node* type_feedback_vector, compiler::Node* context);         \
  void GenerateAssembly(CodeStubAssembler* assembler) const override {        \
    assembler->Return(                                                        \
        Generate(assembler, assembler->Parameter(0), assembler->Parameter(1), \
                 assembler->Parameter(2), assembler->Parameter(3),            \
                 assembler->Parameter(4)));                                   \
  }                                                                           \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_TURBOFAN_UNARY_OP_CODE_STUB(NAME, SUPER)                \
 public:                                                               \
  static compiler::Node* Generate(CodeStubAssembler* assembler,        \
                                  compiler::Node* value,               \
                                  compiler::Node* context);            \
  void GenerateAssembly(CodeStubAssembler* assembler) const override { \
    assembler->Return(Generate(assembler, assembler->Parameter(0),     \
                               assembler->Parameter(1)));              \
  }                                                                    \
  DEFINE_CODE_STUB(NAME, SUPER)

#define DEFINE_TURBOFAN_UNARY_OP_CODE_STUB_WITH_FEEDBACK(NAME, SUPER)         \
 public:                                                                      \
  static compiler::Node* Generate(                                            \
      CodeStubAssembler* assembler, compiler::Node* value,                    \
      compiler::Node* context, compiler::Node* type_feedback_vector,          \
      compiler::Node* slot_id);                                               \
  void GenerateAssembly(CodeStubAssembler* assembler) const override {        \
    assembler->Return(                                                        \
        Generate(assembler, assembler->Parameter(0), assembler->Parameter(1), \
                 assembler->Parameter(2), assembler->Parameter(3)));          \
  }                                                                           \
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

  virtual void GenerateAssembly(CodeStubAssembler* assembler) const = 0;

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

class AddStub final : public TurboFanCodeStub {
 public:
  explicit AddStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(Add, TurboFanCodeStub);
};

class AddWithFeedbackStub final : public TurboFanCodeStub {
 public:
  explicit AddWithFeedbackStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithVector);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB_WITH_FEEDBACK(AddWithFeedback,
                                                    TurboFanCodeStub);
};

class SubtractStub final : public TurboFanCodeStub {
 public:
  explicit SubtractStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(Subtract, TurboFanCodeStub);
};

class SubtractWithFeedbackStub final : public TurboFanCodeStub {
 public:
  explicit SubtractWithFeedbackStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithVector);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB_WITH_FEEDBACK(SubtractWithFeedback,
                                                    TurboFanCodeStub);
};

class MultiplyStub final : public TurboFanCodeStub {
 public:
  explicit MultiplyStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(Multiply, TurboFanCodeStub);
};

class MultiplyWithFeedbackStub final : public TurboFanCodeStub {
 public:
  explicit MultiplyWithFeedbackStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithVector);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB_WITH_FEEDBACK(MultiplyWithFeedback,
                                                    TurboFanCodeStub);
};

class DivideStub final : public TurboFanCodeStub {
 public:
  explicit DivideStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(Divide, TurboFanCodeStub);
};

class DivideWithFeedbackStub final : public TurboFanCodeStub {
 public:
  explicit DivideWithFeedbackStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithVector);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB_WITH_FEEDBACK(DivideWithFeedback,
                                                    TurboFanCodeStub);
};

class ModulusStub final : public TurboFanCodeStub {
 public:
  explicit ModulusStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(Modulus, TurboFanCodeStub);
};

class ModulusWithFeedbackStub final : public TurboFanCodeStub {
 public:
  explicit ModulusWithFeedbackStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOpWithVector);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB_WITH_FEEDBACK(ModulusWithFeedback,
                                                    TurboFanCodeStub);
};

class ShiftRightStub final : public TurboFanCodeStub {
 public:
  explicit ShiftRightStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(ShiftRight, TurboFanCodeStub);
};

class ShiftRightLogicalStub final : public TurboFanCodeStub {
 public:
  explicit ShiftRightLogicalStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(ShiftRightLogical, TurboFanCodeStub);
};

class ShiftLeftStub final : public TurboFanCodeStub {
 public:
  explicit ShiftLeftStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(ShiftLeft, TurboFanCodeStub);
};

class BitwiseAndStub final : public TurboFanCodeStub {
 public:
  explicit BitwiseAndStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(BitwiseAnd, TurboFanCodeStub);
};

class BitwiseOrStub final : public TurboFanCodeStub {
 public:
  explicit BitwiseOrStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(BitwiseOr, TurboFanCodeStub);
};

class BitwiseXorStub final : public TurboFanCodeStub {
 public:
  explicit BitwiseXorStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(BinaryOp);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(BitwiseXor, TurboFanCodeStub);
};

class IncStub final : public TurboFanCodeStub {
 public:
  explicit IncStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CountOp);
  DEFINE_TURBOFAN_UNARY_OP_CODE_STUB_WITH_FEEDBACK(Inc, TurboFanCodeStub);
};

class DecStub final : public TurboFanCodeStub {
 public:
  explicit DecStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CountOp);
  DEFINE_TURBOFAN_UNARY_OP_CODE_STUB_WITH_FEEDBACK(Dec, TurboFanCodeStub);
};

class InstanceOfStub final : public TurboFanCodeStub {
 public:
  explicit InstanceOfStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(InstanceOf, TurboFanCodeStub);
};

class LessThanStub final : public TurboFanCodeStub {
 public:
  explicit LessThanStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(LessThan, TurboFanCodeStub);
};

class LessThanOrEqualStub final : public TurboFanCodeStub {
 public:
  explicit LessThanOrEqualStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(LessThanOrEqual, TurboFanCodeStub);
};

class GreaterThanStub final : public TurboFanCodeStub {
 public:
  explicit GreaterThanStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(GreaterThan, TurboFanCodeStub);
};

class GreaterThanOrEqualStub final : public TurboFanCodeStub {
 public:
  explicit GreaterThanOrEqualStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(GreaterThanOrEqual, TurboFanCodeStub);
};

class EqualStub final : public TurboFanCodeStub {
 public:
  explicit EqualStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(Equal, TurboFanCodeStub);
};

class NotEqualStub final : public TurboFanCodeStub {
 public:
  explicit NotEqualStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(NotEqual, TurboFanCodeStub);
};

class StrictEqualStub final : public TurboFanCodeStub {
 public:
  explicit StrictEqualStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(StrictEqual, TurboFanCodeStub);
};

class StrictNotEqualStub final : public TurboFanCodeStub {
 public:
  explicit StrictNotEqualStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Compare);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(StrictNotEqual, TurboFanCodeStub);
};

class ToIntegerStub final : public TurboFanCodeStub {
 public:
  explicit ToIntegerStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TypeConversion);
  DEFINE_TURBOFAN_CODE_STUB(ToInteger, TurboFanCodeStub);
};

class ToLengthStub final : public TurboFanCodeStub {
 public:
  explicit ToLengthStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TypeConversion);
  DEFINE_TURBOFAN_CODE_STUB(ToLength, TurboFanCodeStub);
};

class StoreInterceptorStub : public TurboFanCodeStub {
 public:
  explicit StoreInterceptorStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  void GenerateAssembly(CodeStubAssembler* assember) const override;

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_CODE_STUB(StoreInterceptor, TurboFanCodeStub);
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

// ES6 section 12.10.3 "in" operator evaluation.
class HasPropertyStub : public TurboFanCodeStub {
 public:
  explicit HasPropertyStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(HasProperty);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(HasProperty, TurboFanCodeStub);
};

class ForInFilterStub : public TurboFanCodeStub {
 public:
  explicit ForInFilterStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ForInFilter);
  DEFINE_TURBOFAN_BINARY_OP_CODE_STUB(ForInFilter, TurboFanCodeStub);
};

// ES6 [[Get]] operation.
class GetPropertyStub : public TurboFanCodeStub {
 public:
  explicit GetPropertyStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(GetProperty);
  DEFINE_TURBOFAN_CODE_STUB(GetProperty, TurboFanCodeStub);
};

enum StringAddFlags {
  // Omit both parameter checks.
  STRING_ADD_CHECK_NONE = 0,
  // Check left parameter.
  STRING_ADD_CHECK_LEFT = 1 << 0,
  // Check right parameter.
  STRING_ADD_CHECK_RIGHT = 1 << 1,
  // Check both parameters.
  STRING_ADD_CHECK_BOTH = STRING_ADD_CHECK_LEFT | STRING_ADD_CHECK_RIGHT,
  // Convert parameters when check fails (instead of throwing an exception).
  STRING_ADD_CONVERT = 1 << 2,
  STRING_ADD_CONVERT_LEFT = STRING_ADD_CHECK_LEFT | STRING_ADD_CONVERT,
  STRING_ADD_CONVERT_RIGHT = STRING_ADD_CHECK_RIGHT | STRING_ADD_CONVERT
};


std::ostream& operator<<(std::ostream& os, const StringAddFlags& flags);


class NumberToStringStub final : public HydrogenCodeStub {
 public:
  explicit NumberToStringStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TypeConversion);
  DEFINE_HYDROGEN_CODE_STUB(NumberToString, HydrogenCodeStub);
};

class TypeofStub final : public TurboFanCodeStub {
 public:
  explicit TypeofStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Typeof);
  DEFINE_TURBOFAN_UNARY_OP_CODE_STUB(Typeof, TurboFanCodeStub);
};

class FastNewClosureStub : public TurboFanCodeStub {
 public:
  explicit FastNewClosureStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  static compiler::Node* Generate(CodeStubAssembler* assembler,
                                  compiler::Node* shared_info,
                                  compiler::Node* context);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewClosure);
  DEFINE_TURBOFAN_CODE_STUB(FastNewClosure, TurboFanCodeStub);
};

class FastNewFunctionContextStub final : public TurboFanCodeStub {
 public:
  static const int kMaximumSlots = 0x8000;

  explicit FastNewFunctionContextStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  static compiler::Node* Generate(CodeStubAssembler* assembler,
                                  compiler::Node* function,
                                  compiler::Node* slots,
                                  compiler::Node* context);

 private:
  // FastNewFunctionContextStub can only allocate closures which fit in the
  // new space.
  STATIC_ASSERT(((kMaximumSlots + Context::MIN_CONTEXT_SLOTS) * kPointerSize +
                 FixedArray::kHeaderSize) < kMaxRegularHeapObjectSize);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewFunctionContext);
  DEFINE_TURBOFAN_CODE_STUB(FastNewFunctionContext, TurboFanCodeStub);
};


class FastNewObjectStub final : public PlatformCodeStub {
 public:
  explicit FastNewObjectStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewObject);
  DEFINE_PLATFORM_CODE_STUB(FastNewObject, PlatformCodeStub);
};


// TODO(turbofan): This stub should be possible to write in TurboFan
// using the CodeStubAssembler very soon in a way that is as efficient
// and easy as the current handwritten version, which is partly a copy
// of the strict arguments object materialization code.
class FastNewRestParameterStub final : public PlatformCodeStub {
 public:
  explicit FastNewRestParameterStub(Isolate* isolate,
                                    bool skip_stub_frame = false)
      : PlatformCodeStub(isolate) {
    minor_key_ = SkipStubFrameBits::encode(skip_stub_frame);
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewRestParameter);
  DEFINE_PLATFORM_CODE_STUB(FastNewRestParameter, PlatformCodeStub);

  int skip_stub_frame() const { return SkipStubFrameBits::decode(minor_key_); }

 private:
  class SkipStubFrameBits : public BitField<bool, 0, 1> {};
};


// TODO(turbofan): This stub should be possible to write in TurboFan
// using the CodeStubAssembler very soon in a way that is as efficient
// and easy as the current handwritten version.
class FastNewSloppyArgumentsStub final : public PlatformCodeStub {
 public:
  explicit FastNewSloppyArgumentsStub(Isolate* isolate,
                                      bool skip_stub_frame = false)
      : PlatformCodeStub(isolate) {
    minor_key_ = SkipStubFrameBits::encode(skip_stub_frame);
  }

  int skip_stub_frame() const { return SkipStubFrameBits::decode(minor_key_); }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewSloppyArguments);
  DEFINE_PLATFORM_CODE_STUB(FastNewSloppyArguments, PlatformCodeStub);

 private:
  class SkipStubFrameBits : public BitField<bool, 0, 1> {};
};


// TODO(turbofan): This stub should be possible to write in TurboFan
// using the CodeStubAssembler very soon in a way that is as efficient
// and easy as the current handwritten version.
class FastNewStrictArgumentsStub final : public PlatformCodeStub {
 public:
  explicit FastNewStrictArgumentsStub(Isolate* isolate,
                                      bool skip_stub_frame = false)
      : PlatformCodeStub(isolate) {
    minor_key_ = SkipStubFrameBits::encode(skip_stub_frame);
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastNewStrictArguments);
  DEFINE_PLATFORM_CODE_STUB(FastNewStrictArguments, PlatformCodeStub);

  int skip_stub_frame() const { return SkipStubFrameBits::decode(minor_key_); }

 private:
  class SkipStubFrameBits : public BitField<bool, 0, 1> {};
};

class FastCloneRegExpStub final : public TurboFanCodeStub {
 public:
  explicit FastCloneRegExpStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  static compiler::Node* Generate(CodeStubAssembler* assembler,
                                  compiler::Node* closure,
                                  compiler::Node* literal_index,
                                  compiler::Node* pattern,
                                  compiler::Node* flags,
                                  compiler::Node* context);

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastCloneRegExp);
  DEFINE_TURBOFAN_CODE_STUB(FastCloneRegExp, TurboFanCodeStub);
};

class FastCloneShallowArrayStub : public TurboFanCodeStub {
 public:
  FastCloneShallowArrayStub(Isolate* isolate,
                            AllocationSiteMode allocation_site_mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = AllocationSiteModeBits::encode(allocation_site_mode);
  }

  static compiler::Node* Generate(CodeStubAssembler* assembler,
                                  compiler::Node* closure,
                                  compiler::Node* literal_index,
                                  compiler::Node* context,
                                  CodeStubAssembler::Label* call_runtime,
                                  AllocationSiteMode allocation_site_mode);

  AllocationSiteMode allocation_site_mode() const {
    return AllocationSiteModeBits::decode(minor_key_);
  }

 private:
  class AllocationSiteModeBits: public BitField<AllocationSiteMode, 0, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastCloneShallowArray);
  DEFINE_TURBOFAN_CODE_STUB(FastCloneShallowArray, TurboFanCodeStub);
};

class FastCloneShallowObjectStub : public TurboFanCodeStub {
 public:
  // Maximum number of properties in copied object.
  static const int kMaximumClonedProperties = 6;

  FastCloneShallowObjectStub(Isolate* isolate, int length)
      : TurboFanCodeStub(isolate) {
    DCHECK_GE(length, 0);
    DCHECK_LE(length, kMaximumClonedProperties);
    minor_key_ = LengthBits::encode(LengthBits::encode(length));
  }

  static compiler::Node* GenerateFastPath(
      CodeStubAssembler* assembler,
      compiler::CodeAssembler::Label* call_runtime, compiler::Node* closure,
      compiler::Node* literals_index, compiler::Node* properties_count);

  static bool IsSupported(ObjectLiteral* expr);
  static int PropertiesCount(int literal_length);

  int length() const { return LengthBits::decode(minor_key_); }

 private:
  class LengthBits : public BitField<int, 0, 4> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(FastCloneShallowObject);
  DEFINE_TURBOFAN_CODE_STUB(FastCloneShallowObject, TurboFanCodeStub);
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

class FastArrayPushStub : public HydrogenCodeStub {
 public:
  explicit FastArrayPushStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(VarArgFunction);
  DEFINE_HYDROGEN_CODE_STUB(FastArrayPush, HydrogenCodeStub);
};

class FastFunctionBindStub : public HydrogenCodeStub {
 public:
  explicit FastFunctionBindStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(VarArgFunction);
  DEFINE_HYDROGEN_CODE_STUB(FastFunctionBind, HydrogenCodeStub);
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

  void PrintName(std::ostream& os) const override;  // NOLINT

  class ArgumentCountBits : public BitField<ArgumentCountKey, 0, 2> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(ArrayNArgumentsConstructor);
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


class CallICStub: public PlatformCodeStub {
 public:
  CallICStub(Isolate* isolate, const CallICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  Code::Kind GetCodeKind() const override { return Code::CALL_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

 protected:
  int arg_count() const { return state().argc(); }
  ConvertReceiverMode convert_mode() const { return state().convert_mode(); }
  TailCallMode tail_call_mode() const { return state().tail_call_mode(); }

  CallICState state() const { return CallICState(GetExtraICState()); }

  // Code generation helpers.
  void GenerateMiss(MacroAssembler* masm);
  void HandleArrayCase(MacroAssembler* masm, Label* miss);

 private:
  void PrintState(std::ostream& os) const override;  // NOLINT

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallFunctionWithFeedbackAndVector);
  DEFINE_PLATFORM_CODE_STUB(CallIC, PlatformCodeStub);
};


// TODO(verwaest): Translate to hydrogen code stub.
class FunctionPrototypeStub : public PlatformCodeStub {
 public:
  explicit FunctionPrototypeStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::LOAD_IC; }

  // TODO(mvstanton): only the receiver register is accessed. When this is
  // translated to a hydrogen code stub, a new CallInterfaceDescriptor
  // should be created that just uses that register for more efficient code.
  CallInterfaceDescriptor GetCallInterfaceDescriptor() const override {
    return LoadWithVectorDescriptor(isolate());
  }

  DEFINE_PLATFORM_CODE_STUB(FunctionPrototype, PlatformCodeStub);
};


class LoadIndexedStringStub : public PlatformCodeStub {
 public:
  explicit LoadIndexedStringStub(Isolate* isolate)
      : PlatformCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::KEYED_LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_PLATFORM_CODE_STUB(LoadIndexedString, PlatformCodeStub);
};


class HandlerStub : public HydrogenCodeStub {
 public:
  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return kind(); }

  void InitializeDescriptor(CodeStubDescriptor* descriptor) override;

  CallInterfaceDescriptor GetCallInterfaceDescriptor() const override;

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
  Code::Kind kind() const override { return Code::LOAD_IC; }

 private:
  class LoadFieldByIndexBits : public BitField<int, 0, 13> {};

  // TODO(ishell): The stub uses only kReceiver parameter.
  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_HANDLER_CODE_STUB(LoadField, HandlerStub);
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
  Code::Kind kind() const override { return Code::LOAD_IC; }

 private:
  class ConstantIndexBits : public BitField<int, 0, kSubMinorKeyBits> {};

  // TODO(ishell): The stub uses only kReceiver parameter.
  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_HANDLER_CODE_STUB(LoadConstant, HandlerStub);
};

class LoadApiGetterStub : public TurboFanCodeStub {
 public:
  LoadApiGetterStub(Isolate* isolate, bool receiver_is_holder, int index)
      : TurboFanCodeStub(isolate) {
    // If that's not true, we need to ensure that the receiver is actually a
    // JSReceiver. http://crbug.com/609134
    DCHECK(receiver_is_holder);
    minor_key_ = IndexBits::encode(index) |
                 ReceiverIsHolderBits::encode(receiver_is_holder);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::LOAD_IC; }

  int index() const { return IndexBits::decode(minor_key_); }
  bool receiver_is_holder() const {
    return ReceiverIsHolderBits::decode(minor_key_);
  }

 private:
  class ReceiverIsHolderBits : public BitField<bool, 0, 1> {};
  class IndexBits : public BitField<int, 1, kDescriptorIndexBitCount> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_TURBOFAN_CODE_STUB(LoadApiGetter, TurboFanCodeStub);
};

class StoreFieldStub : public TurboFanCodeStub {
 public:
  StoreFieldStub(Isolate* isolate, FieldIndex index,
                 Representation representation)
      : TurboFanCodeStub(isolate) {
    int property_index_key = index.GetFieldAccessStubKey();
    minor_key_ = StoreFieldByIndexBits::encode(property_index_key) |
                 RepresentationBits::encode(representation.kind());
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

  FieldIndex index() const {
    int property_index_key = StoreFieldByIndexBits::decode(minor_key_);
    return FieldIndex::FromFieldAccessStubKey(property_index_key);
  }

  Representation representation() const {
    return Representation::FromKind(RepresentationBits::decode(minor_key_));
  }

 private:
  class StoreFieldByIndexBits : public BitField<int, 0, 13> {};
  class RepresentationBits
      : public BitField<Representation::Kind, StoreFieldByIndexBits::kNext, 4> {
  };
  STATIC_ASSERT(Representation::kNumRepresentations - 1 <
                RepresentationBits::kMax);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreField, TurboFanCodeStub);
};

class StoreMapStub : public TurboFanCodeStub {
 public:
  explicit StoreMapStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreTransition);
  DEFINE_TURBOFAN_CODE_STUB(StoreMap, TurboFanCodeStub);
};

class StoreTransitionStub : public TurboFanCodeStub {
 public:
  enum StoreMode {
    StoreMapAndValue,
    ExtendStorageAndStoreMapAndValue
  };

  StoreTransitionStub(Isolate* isolate, bool is_inobject,
                      Representation representation, StoreMode store_mode)
      : TurboFanCodeStub(isolate) {
    minor_key_ = IsInobjectBits::encode(is_inobject) |
                 RepresentationBits::encode(representation.kind()) |
                 StoreModeBits::encode(store_mode);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

  bool is_inobject() const { return IsInobjectBits::decode(minor_key_); }

  Representation representation() const {
    return Representation::FromKind(RepresentationBits::decode(minor_key_));
  }

  StoreMode store_mode() const { return StoreModeBits::decode(minor_key_); }

 private:
  class IsInobjectBits : public BitField<bool, 0, 1> {};
  class RepresentationBits
      : public BitField<Representation::Kind, IsInobjectBits::kNext, 4> {};
  STATIC_ASSERT(Representation::kNumRepresentations - 1 <
                RepresentationBits::kMax);
  class StoreModeBits
      : public BitField<StoreMode, RepresentationBits::kNext, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreNamedTransition);
  DEFINE_TURBOFAN_CODE_STUB(StoreTransition, TurboFanCodeStub);
};

class StoreGlobalStub : public TurboFanCodeStub {
 public:
  StoreGlobalStub(Isolate* isolate, PropertyCellType type,
                  Maybe<PropertyCellConstantType> constant_type,
                  bool check_global)
      : TurboFanCodeStub(isolate) {
    PropertyCellConstantType encoded_constant_type =
        constant_type.FromMaybe(PropertyCellConstantType::kSmi);
    minor_key_ = CellTypeBits::encode(type) |
                 ConstantTypeBits::encode(encoded_constant_type) |
                 CheckGlobalBits::encode(check_global);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::STORE_IC; }

  static Handle<HeapObject> property_cell_placeholder(Isolate* isolate) {
    return isolate->factory()->uninitialized_value();
  }

  static Handle<HeapObject> global_map_placeholder(Isolate* isolate) {
    return isolate->factory()->termination_exception();
  }

  Handle<Code> GetCodeCopyFromTemplate(Handle<JSGlobalObject> global,
                                       Handle<PropertyCell> cell) {
    Code::FindAndReplacePattern pattern;
    if (check_global()) {
      pattern.Add(handle(global_map_placeholder(isolate())->map()),
                  Map::WeakCellForMap(Handle<Map>(global->map())));
    }
    pattern.Add(handle(property_cell_placeholder(isolate())->map()),
                isolate()->factory()->NewWeakCell(cell));
    return CodeStub::GetCodeCopy(pattern);
  }

  PropertyCellType cell_type() const {
    return CellTypeBits::decode(minor_key_);
  }

  PropertyCellConstantType constant_type() const {
    DCHECK(PropertyCellType::kConstantType == cell_type());
    return ConstantTypeBits::decode(minor_key_);
  }

  bool check_global() const { return CheckGlobalBits::decode(minor_key_); }

 private:
  class CellTypeBits : public BitField<PropertyCellType, 0, 2> {};
  class ConstantTypeBits
      : public BitField<PropertyCellConstantType, CellTypeBits::kNext, 2> {};
  class CheckGlobalBits : public BitField<bool, ConstantTypeBits::kNext, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_TURBOFAN_CODE_STUB(StoreGlobal, TurboFanCodeStub);
};

// TODO(ishell): remove, once StoreGlobalIC is implemented.
class StoreGlobalViaContextStub final : public PlatformCodeStub {
 public:
  static const int kMaximumDepth = 15;

  StoreGlobalViaContextStub(Isolate* isolate, int depth,
                            LanguageMode language_mode)
      : PlatformCodeStub(isolate) {
    minor_key_ =
        DepthBits::encode(depth) | LanguageModeBits::encode(language_mode);
  }

  int depth() const { return DepthBits::decode(minor_key_); }
  LanguageMode language_mode() const {
    return LanguageModeBits::decode(minor_key_);
  }

 private:
  class DepthBits : public BitField<int, 0, 4> {};
  STATIC_ASSERT(DepthBits::kMax == kMaximumDepth);
  class LanguageModeBits : public BitField<LanguageMode, 4, 1> {};
  STATIC_ASSERT(LANGUAGE_END == 2);

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreGlobalViaContext);
  DEFINE_PLATFORM_CODE_STUB(StoreGlobalViaContext, PlatformCodeStub);
};

class CallApiCallbackStub : public PlatformCodeStub {
 public:
  static const int kArgBits = 3;
  static const int kArgMax = (1 << kArgBits) - 1;

  // CallApiCallbackStub for regular setters and getters.
  CallApiCallbackStub(Isolate* isolate, bool is_store, bool call_data_undefined,
                      bool is_lazy)
      : CallApiCallbackStub(isolate, is_store ? 1 : 0, is_store,
                            call_data_undefined, is_lazy) {}

  // CallApiCallbackStub for callback functions.
  CallApiCallbackStub(Isolate* isolate, int argc, bool call_data_undefined,
                      bool is_lazy)
      : CallApiCallbackStub(isolate, argc, false, call_data_undefined,
                            is_lazy) {}

 private:
  CallApiCallbackStub(Isolate* isolate, int argc, bool is_store,
                      bool call_data_undefined, bool is_lazy)
      : PlatformCodeStub(isolate) {
    CHECK(0 <= argc && argc <= kArgMax);
    minor_key_ = IsStoreBits::encode(is_store) |
                 CallDataUndefinedBits::encode(call_data_undefined) |
                 ArgumentBits::encode(argc) |
                 IsLazyAccessorBits::encode(is_lazy);
  }

  bool is_store() const { return IsStoreBits::decode(minor_key_); }
  bool is_lazy() const { return IsLazyAccessorBits::decode(minor_key_); }
  bool call_data_undefined() const {
    return CallDataUndefinedBits::decode(minor_key_);
  }
  int argc() const { return ArgumentBits::decode(minor_key_); }

  class IsStoreBits: public BitField<bool, 0, 1> {};
  class CallDataUndefinedBits: public BitField<bool, 1, 1> {};
  class ArgumentBits : public BitField<int, 2, kArgBits> {};
  class IsLazyAccessorBits : public BitField<bool, 3 + kArgBits, 1> {};

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
    Code::FindAndReplacePattern pattern;
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


class StringAddStub final : public HydrogenCodeStub {
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

 private:
  class StringAddFlagsBits : public BitField<StringAddFlags, 0, 3> {};
  class PretenureFlagBits : public BitField<PretenureFlag, 3, 1> {};

  void PrintBaseName(std::ostream& os) const override;  // NOLINT

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StringAdd);
  DEFINE_HYDROGEN_CODE_STUB(StringAdd, HydrogenCodeStub);
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


class RegExpExecStub: public PlatformCodeStub {
 public:
  explicit RegExpExecStub(Isolate* isolate) : PlatformCodeStub(isolate) { }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(RegExpExec);
  DEFINE_PLATFORM_CODE_STUB(RegExpExec, PlatformCodeStub);
};

// TODO(jgruber): Remove this once all uses in regexp.js have been removed.
class RegExpConstructResultStub final : public HydrogenCodeStub {
 public:
  explicit RegExpConstructResultStub(Isolate* isolate)
      : HydrogenCodeStub(isolate) { }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(RegExpConstructResult);
  DEFINE_HYDROGEN_CODE_STUB(RegExpConstructResult, HydrogenCodeStub);
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

  ReceiverCheckMode check_mode_;

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
  StringCharAtGenerator(Register object, Register index, Register scratch,
                        Register result, Label* receiver_not_string,
                        Label* index_not_number, Label* index_out_of_range,
                        ReceiverCheckMode check_mode = RECEIVER_IS_UNKNOWN)
      : char_code_at_generator_(object, index, scratch, receiver_not_string,
                                index_not_number, index_out_of_range,
                                check_mode),
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
  void GenerateSlow(MacroAssembler* masm, EmbedMode embed_mode,
                    const RuntimeCallHelper& call_helper) {
    char_code_at_generator_.GenerateSlow(masm, embed_mode, call_helper);
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

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_HYDROGEN_CODE_STUB(LoadDictionaryElement, HydrogenCodeStub);
};


class KeyedLoadGenericStub : public HydrogenCodeStub {
 public:
  explicit KeyedLoadGenericStub(Isolate* isolate) : HydrogenCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::KEYED_LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_HYDROGEN_CODE_STUB(KeyedLoadGeneric, HydrogenCodeStub);
};


class LoadICTrampolineStub : public PlatformCodeStub {
 public:
  explicit LoadICTrampolineStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_PLATFORM_CODE_STUB(LoadICTrampoline, PlatformCodeStub);
};

class LoadICTrampolineTFStub : public TurboFanCodeStub {
 public:
  explicit LoadICTrampolineTFStub(Isolate* isolate)
      : TurboFanCodeStub(isolate) {}

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Load);
  DEFINE_CODE_STUB(LoadICTrampolineTF, TurboFanCodeStub);
};

class LoadGlobalICTrampolineStub : public TurboFanCodeStub {
 public:
  explicit LoadGlobalICTrampolineStub(Isolate* isolate,
                                      const LoadGlobalICState& state)
      : TurboFanCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::LOAD_GLOBAL_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadGlobal);
  DEFINE_CODE_STUB(LoadGlobalICTrampoline, TurboFanCodeStub);
};

class KeyedLoadICTrampolineStub : public LoadICTrampolineStub {
 public:
  explicit KeyedLoadICTrampolineStub(Isolate* isolate)
      : LoadICTrampolineStub(isolate) {}

  Code::Kind GetCodeKind() const override { return Code::KEYED_LOAD_IC; }

  DEFINE_PLATFORM_CODE_STUB(KeyedLoadICTrampoline, LoadICTrampolineStub);
};

class KeyedLoadICTrampolineTFStub : public LoadICTrampolineTFStub {
 public:
  explicit KeyedLoadICTrampolineTFStub(Isolate* isolate)
      : LoadICTrampolineTFStub(isolate) {}

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::KEYED_LOAD_IC; }

  DEFINE_CODE_STUB(KeyedLoadICTrampolineTF, LoadICTrampolineTFStub);
};

class StoreICTrampolineStub : public PlatformCodeStub {
 public:
  StoreICTrampolineStub(Isolate* isolate, const StoreICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  Code::Kind GetCodeKind() const override { return Code::STORE_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

 protected:
  StoreICState state() const { return StoreICState(GetExtraICState()); }

 private:
  DEFINE_CALL_INTERFACE_DESCRIPTOR(Store);
  DEFINE_PLATFORM_CODE_STUB(StoreICTrampoline, PlatformCodeStub);
};

class StoreICTrampolineTFStub : public TurboFanCodeStub {
 public:
  StoreICTrampolineTFStub(Isolate* isolate, const StoreICState& state)
      : TurboFanCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::STORE_IC; }
  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

 protected:
  StoreICState state() const { return StoreICState(GetExtraICState()); }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(Store);
  DEFINE_CODE_STUB(StoreICTrampolineTF, TurboFanCodeStub);
};

class KeyedStoreICTrampolineStub : public StoreICTrampolineStub {
 public:
  KeyedStoreICTrampolineStub(Isolate* isolate, const StoreICState& state)
      : StoreICTrampolineStub(isolate, state) {}

  Code::Kind GetCodeKind() const override { return Code::KEYED_STORE_IC; }

  DEFINE_PLATFORM_CODE_STUB(KeyedStoreICTrampoline, StoreICTrampolineStub);
};


class CallICTrampolineStub : public PlatformCodeStub {
 public:
  CallICTrampolineStub(Isolate* isolate, const CallICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  Code::Kind GetCodeKind() const override { return Code::CALL_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

 protected:
  CallICState state() const {
    return CallICState(static_cast<ExtraICState>(minor_key_));
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(CallFunctionWithFeedback);
  DEFINE_PLATFORM_CODE_STUB(CallICTrampoline, PlatformCodeStub);
};


class LoadICStub : public PlatformCodeStub {
 public:
  explicit LoadICStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  void GenerateForTrampoline(MacroAssembler* masm);

  Code::Kind GetCodeKind() const override { return Code::LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_PLATFORM_CODE_STUB(LoadIC, PlatformCodeStub);

 protected:
  void GenerateImpl(MacroAssembler* masm, bool in_frame);
};

class LoadICTFStub : public TurboFanCodeStub {
 public:
  explicit LoadICTFStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_CODE_STUB(LoadICTF, TurboFanCodeStub);
};

class LoadGlobalICStub : public TurboFanCodeStub {
 public:
  explicit LoadGlobalICStub(Isolate* isolate, const LoadGlobalICState& state)
      : TurboFanCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::LOAD_GLOBAL_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadGlobalWithVector);
  DEFINE_CODE_STUB(LoadGlobalIC, TurboFanCodeStub);
};

class KeyedLoadICStub : public PlatformCodeStub {
 public:
  explicit KeyedLoadICStub(Isolate* isolate) : PlatformCodeStub(isolate) {}

  void GenerateForTrampoline(MacroAssembler* masm);

  Code::Kind GetCodeKind() const override { return Code::KEYED_LOAD_IC; }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_PLATFORM_CODE_STUB(KeyedLoadIC, PlatformCodeStub);

 protected:
  void GenerateImpl(MacroAssembler* masm, bool in_frame);
};

class KeyedLoadICTFStub : public LoadICTFStub {
 public:
  explicit KeyedLoadICTFStub(Isolate* isolate) : LoadICTFStub(isolate) {}

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::KEYED_LOAD_IC; }

  DEFINE_CODE_STUB(KeyedLoadICTF, LoadICTFStub);
};

class StoreICStub : public PlatformCodeStub {
 public:
  StoreICStub(Isolate* isolate, const StoreICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  void GenerateForTrampoline(MacroAssembler* masm);

  Code::Kind GetCodeKind() const final { return Code::STORE_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_PLATFORM_CODE_STUB(StoreIC, PlatformCodeStub);

 protected:
  void GenerateImpl(MacroAssembler* masm, bool in_frame);
};

class StoreICTFStub : public TurboFanCodeStub {
 public:
  StoreICTFStub(Isolate* isolate, const StoreICState& state)
      : TurboFanCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  Code::Kind GetCodeKind() const override { return Code::STORE_IC; }
  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_CODE_STUB(StoreICTF, TurboFanCodeStub);
};

class KeyedStoreICStub : public PlatformCodeStub {
 public:
  KeyedStoreICStub(Isolate* isolate, const StoreICState& state)
      : PlatformCodeStub(isolate) {
    minor_key_ = state.GetExtraICState();
  }

  void GenerateForTrampoline(MacroAssembler* masm);

  Code::Kind GetCodeKind() const final { return Code::KEYED_STORE_IC; }

  ExtraICState GetExtraICState() const final {
    return static_cast<ExtraICState>(minor_key_);
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_PLATFORM_CODE_STUB(KeyedStoreIC, PlatformCodeStub);

 protected:
  void GenerateImpl(MacroAssembler* masm, bool in_frame);
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


class LoadFastElementStub : public HandlerStub {
 public:
  LoadFastElementStub(Isolate* isolate, bool is_js_array,
                      ElementsKind elements_kind,
                      bool convert_hole_to_undefined = false)
      : HandlerStub(isolate) {
    set_sub_minor_key(
        ElementsKindBits::encode(elements_kind) |
        IsJSArrayBits::encode(is_js_array) |
        CanConvertHoleToUndefined::encode(convert_hole_to_undefined));
  }

  Code::Kind kind() const override { return Code::KEYED_LOAD_IC; }

  bool is_js_array() const { return IsJSArrayBits::decode(sub_minor_key()); }
  bool convert_hole_to_undefined() const {
    return CanConvertHoleToUndefined::decode(sub_minor_key());
  }

  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(sub_minor_key());
  }

 private:
  class ElementsKindBits: public BitField<ElementsKind, 0, 8> {};
  class IsJSArrayBits: public BitField<bool, 8, 1> {};
  class CanConvertHoleToUndefined : public BitField<bool, 9, 1> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(LoadWithVector);
  DEFINE_HANDLER_CODE_STUB(LoadFastElement, HandlerStub);
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
  void GenerateAssembly(CodeStubAssembler* assembler) const override;

  DEFINE_CALL_INTERFACE_DESCRIPTOR(AllocateHeapNumber);
  DEFINE_CODE_STUB(AllocateHeapNumber, TurboFanCodeStub);
};

#define SIMD128_ALLOC_STUB(TYPE, Type, type, lane_count, lane_type)     \
  class Allocate##Type##Stub : public TurboFanCodeStub {                \
   public:                                                              \
    explicit Allocate##Type##Stub(Isolate* isolate)                     \
        : TurboFanCodeStub(isolate) {}                                  \
                                                                        \
    void InitializeDescriptor(CodeStubDescriptor* descriptor) override; \
    void GenerateAssembly(CodeStubAssembler* assembler) const override; \
                                                                        \
    DEFINE_CALL_INTERFACE_DESCRIPTOR(Allocate##Type);                   \
    DEFINE_CODE_STUB(Allocate##Type, TurboFanCodeStub);                 \
  };
SIMD128_TYPES(SIMD128_ALLOC_STUB)
#undef SIMD128_ALLOC_STUB

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

class StoreElementStub : public PlatformCodeStub {
 public:
  StoreElementStub(Isolate* isolate, ElementsKind elements_kind,
                   KeyedAccessStoreMode mode)
      : PlatformCodeStub(isolate) {
    // TODO(jkummerow): Rename this stub to StoreSlowElementStub,
    // drop elements_kind parameter.
    DCHECK_EQ(DICTIONARY_ELEMENTS, elements_kind);
    minor_key_ = ElementsKindBits::encode(elements_kind) |
                 CommonStoreModeBits::encode(mode);
  }

  Code::Kind GetCodeKind() const override { return Code::HANDLER; }
  ExtraICState GetExtraICState() const override { return Code::KEYED_STORE_IC; }

 private:
  ElementsKind elements_kind() const {
    return ElementsKindBits::decode(minor_key_);
  }

  class ElementsKindBits
      : public BitField<ElementsKind, CommonStoreModeBits::kNext, 8> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(StoreWithVector);
  DEFINE_PLATFORM_CODE_STUB(StoreElement, PlatformCodeStub);
};

class ToBooleanICStub : public HydrogenCodeStub {
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
    SIMD_VALUE,
    NUMBER_OF_TYPES
  };

  // At most 16 different types can be distinguished, because the Code object
  // only has room for two bytes to hold a set of these types. :-P
  STATIC_ASSERT(NUMBER_OF_TYPES <= 16);

  class Types : public EnumSet<Type, uint16_t> {
   public:
    Types() : EnumSet<Type, uint16_t>(0) {}
    explicit Types(uint16_t bits) : EnumSet<Type, uint16_t>(bits) {}

    bool UpdateStatus(Isolate* isolate, Handle<Object> object);
    bool NeedsMap() const;
    bool CanBeUndetectable() const {
      return Contains(ToBooleanICStub::SPEC_OBJECT);
    }
    bool IsGeneric() const { return ToIntegral() == Generic().ToIntegral(); }

    static Types Generic() { return Types((1 << NUMBER_OF_TYPES) - 1); }
  };

  ToBooleanICStub(Isolate* isolate, ExtraICState state)
      : HydrogenCodeStub(isolate) {
    set_sub_minor_key(TypesBits::encode(static_cast<uint16_t>(state)));
  }

  bool UpdateStatus(Handle<Object> object);
  Types types() const { return Types(TypesBits::decode(sub_minor_key())); }

  Code::Kind GetCodeKind() const override { return Code::TO_BOOLEAN_IC; }
  void PrintState(std::ostream& os) const override;  // NOLINT

  bool SometimesSetsUpAFrame() override { return false; }

  static Handle<Code> GetUninitialized(Isolate* isolate) {
    return ToBooleanICStub(isolate, UNINITIALIZED).GetCode();
  }

  ExtraICState GetExtraICState() const override { return types().ToIntegral(); }

  InlineCacheState GetICState() const {
    if (types().IsEmpty()) {
      return ::v8::internal::UNINITIALIZED;
    } else {
      return MONOMORPHIC;
    }
  }

 private:
  ToBooleanICStub(Isolate* isolate, InitializationState init_state)
      : HydrogenCodeStub(isolate, init_state) {}

  class TypesBits : public BitField<uint16_t, 0, NUMBER_OF_TYPES> {};

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TypeConversion);
  DEFINE_HYDROGEN_CODE_STUB(ToBooleanIC, HydrogenCodeStub);
};

std::ostream& operator<<(std::ostream& os, const ToBooleanICStub::Types& t);

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

  static compiler::Node* Generate(CodeStubAssembler* assembler,
                                  compiler::Node* string, compiler::Node* from,
                                  compiler::Node* to, compiler::Node* context);

  void GenerateAssembly(CodeStubAssembler* assembler) const override {
    assembler->Return(Generate(assembler,
                               assembler->Parameter(Descriptor::kString),
                               assembler->Parameter(Descriptor::kFrom),
                               assembler->Parameter(Descriptor::kTo),
                               assembler->Parameter(Descriptor::kContext)));
  }

  DEFINE_CALL_INTERFACE_DESCRIPTOR(SubString);
  DEFINE_CODE_STUB(SubString, TurboFanCodeStub);
};

class ToObjectStub final : public TurboFanCodeStub {
 public:
  explicit ToObjectStub(Isolate* isolate) : TurboFanCodeStub(isolate) {}

  DEFINE_CALL_INTERFACE_DESCRIPTOR(TypeConversion);
  DEFINE_TURBOFAN_CODE_STUB(ToObject, TurboFanCodeStub);
};

#undef DEFINE_CALL_INTERFACE_DESCRIPTOR
#undef DEFINE_PLATFORM_CODE_STUB
#undef DEFINE_HANDLER_CODE_STUB
#undef DEFINE_HYDROGEN_CODE_STUB
#undef DEFINE_CODE_STUB
#undef DEFINE_CODE_STUB_BASE

extern Representation RepresentationFromMachineType(MachineType type);

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_STUBS_H_

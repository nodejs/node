// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CALL_INTERFACE_DESCRIPTOR_H_
#define V8_CALL_INTERFACE_DESCRIPTOR_H_

#include <memory>

#include "src/assembler.h"
#include "src/globals.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

class PlatformInterfaceDescriptor;

#define INTERFACE_DESCRIPTOR_LIST(V)       \
  V(Void)                                  \
  V(ContextOnly)                           \
  V(Load)                                  \
  V(LoadWithVector)                        \
  V(LoadField)                             \
  V(LoadICProtoArray)                      \
  V(LoadGlobal)                            \
  V(LoadGlobalWithVector)                  \
  V(Store)                                 \
  V(StoreWithVector)                       \
  V(StoreNamedTransition)                  \
  V(StoreTransition)                       \
  V(VarArgFunction)                        \
  V(FastNewClosure)                        \
  V(FastNewFunctionContext)                \
  V(FastNewObject)                         \
  V(FastNewArguments)                      \
  V(TypeConversion)                        \
  V(Typeof)                                \
  V(FastCloneRegExp)                       \
  V(FastCloneShallowArray)                 \
  V(FastCloneShallowObject)                \
  V(CreateAllocationSite)                  \
  V(CreateWeakCell)                        \
  V(CallFunction)                          \
  V(CallIC)                                \
  V(CallICTrampoline)                      \
  V(CallForwardVarargs)                    \
  V(CallConstruct)                         \
  V(CallTrampoline)                        \
  V(ConstructStub)                         \
  V(ConstructForwardVarargs)               \
  V(ConstructTrampoline)                   \
  V(TransitionElementsKind)                \
  V(AllocateHeapNumber)                    \
  V(Builtin)                               \
  V(ArrayConstructor)                      \
  V(IteratingArrayBuiltin)                 \
  V(ArrayNoArgumentConstructor)            \
  V(ArraySingleArgumentConstructor)        \
  V(ArrayNArgumentsConstructor)            \
  V(Compare)                               \
  V(BinaryOp)                              \
  V(BinaryOpWithAllocationSite)            \
  V(BinaryOpWithVector)                    \
  V(CountOp)                               \
  V(StringAdd)                             \
  V(StringCharAt)                          \
  V(StringCharCodeAt)                      \
  V(StringCompare)                         \
  V(SubString)                             \
  V(ForInPrepare)                          \
  V(GetProperty)                           \
  V(ArgumentAdaptor)                       \
  V(ApiCallback)                           \
  V(ApiGetter)                             \
  V(MathPowTagged)                         \
  V(MathPowInteger)                        \
  V(GrowArrayElements)                     \
  V(NewArgumentsElements)                  \
  V(InterpreterDispatch)                   \
  V(InterpreterPushArgsThenCall)           \
  V(InterpreterPushArgsThenConstruct)      \
  V(InterpreterPushArgsThenConstructArray) \
  V(InterpreterCEntry)                     \
  V(ResumeGenerator)                       \
  V(FrameDropperTrampoline)                \
  V(WasmRuntimeCall)                       \
  BUILTIN_LIST_TFS(V)

class V8_EXPORT_PRIVATE CallInterfaceDescriptorData {
 public:
  CallInterfaceDescriptorData() : register_param_count_(-1), param_count_(-1) {}

  // A copy of the passed in registers and param_representations is made
  // and owned by the CallInterfaceDescriptorData.

  void InitializePlatformSpecific(
      int register_parameter_count, const Register* registers,
      PlatformInterfaceDescriptor* platform_descriptor = NULL);

  // if machine_types is null, then an array of size
  // (register_parameter_count + extra_parameter_count) will be created
  // with MachineType::AnyTagged() for each member.
  //
  // if machine_types is not null, then it should be of the size
  // register_parameter_count. Those members of the parameter array
  // will be initialized from {machine_types}, and the rest initialized
  // to MachineType::AnyTagged().
  void InitializePlatformIndependent(int parameter_count,
                                     int extra_parameter_count,
                                     const MachineType* machine_types);

  bool IsInitialized() const {
    return register_param_count_ >= 0 && param_count_ >= 0;
  }

  int param_count() const { return param_count_; }
  int register_param_count() const { return register_param_count_; }
  Register register_param(int index) const { return register_params_[index]; }
  Register* register_params() const { return register_params_.get(); }
  MachineType param_type(int index) const { return machine_types_[index]; }
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return platform_specific_descriptor_;
  }

 private:
  int register_param_count_;
  int param_count_;

  // The Register params are allocated dynamically by the
  // InterfaceDescriptor, and freed on destruction. This is because static
  // arrays of Registers cause creation of runtime static initializers
  // which we don't want.
  std::unique_ptr<Register[]> register_params_;
  std::unique_ptr<MachineType[]> machine_types_;

  PlatformInterfaceDescriptor* platform_specific_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(CallInterfaceDescriptorData);
};


class CallDescriptors {
 public:
  enum Key {
#define DEF_ENUM(name, ...) name,
    INTERFACE_DESCRIPTOR_LIST(DEF_ENUM)
#undef DEF_ENUM
        NUMBER_OF_DESCRIPTORS
  };
};

class V8_EXPORT_PRIVATE CallInterfaceDescriptor {
 public:
  CallInterfaceDescriptor() : data_(NULL) {}
  virtual ~CallInterfaceDescriptor() {}

  CallInterfaceDescriptor(Isolate* isolate, CallDescriptors::Key key)
      : data_(isolate->call_descriptor_data(key)) {}

  int GetParameterCount() const { return data()->param_count(); }

  int GetRegisterParameterCount() const {
    return data()->register_param_count();
  }

  int GetStackParameterCount() const {
    return data()->param_count() - data()->register_param_count();
  }

  Register GetRegisterParameter(int index) const {
    return data()->register_param(index);
  }

  MachineType GetParameterType(int index) const {
    DCHECK(index < data()->param_count());
    return data()->param_type(index);
  }

  // Some platforms have extra information to associate with the descriptor.
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return data()->platform_specific_descriptor();
  }

  static const Register ContextRegister();

  const char* DebugName(Isolate* isolate) const;

 protected:
  const CallInterfaceDescriptorData* data() const { return data_; }

  virtual void InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
    UNREACHABLE();
  }

  virtual void InitializePlatformIndependent(
      CallInterfaceDescriptorData* data) {
    data->InitializePlatformIndependent(data->register_param_count(), 0, NULL);
  }

  void Initialize(Isolate* isolate, CallDescriptors::Key key) {
    if (!data()->IsInitialized()) {
      // We should only initialize descriptors on the isolate's main thread.
      DCHECK(ThreadId::Current().Equals(isolate->thread_id()));
      CallInterfaceDescriptorData* d = isolate->call_descriptor_data(key);
      DCHECK(d == data());  // d should be a modifiable pointer to data().
      InitializePlatformSpecific(d);
      InitializePlatformIndependent(d);
    }
  }

  // Initializes |data| using the platform dependent default set of registers.
  // It is intended to be used for TurboFan stubs when particular set of
  // registers does not matter.
  static void DefaultInitializePlatformSpecific(
      CallInterfaceDescriptorData* data, int register_parameter_count);

 private:
  const CallInterfaceDescriptorData* data_;
};

#define DECLARE_DESCRIPTOR_WITH_BASE(name, base)           \
 public:                                                   \
  explicit name(Isolate* isolate) : base(isolate, key()) { \
    Initialize(isolate, key());                            \
  }                                                        \
  static inline CallDescriptors::Key key();

static const int kMaxBuiltinRegisterParams = 5;

#define DECLARE_DEFAULT_DESCRIPTOR(name, base, parameter_count)               \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                    \
 protected:                                                                   \
  static const int kRegisterParams =                                          \
      parameter_count > kMaxBuiltinRegisterParams ? kMaxBuiltinRegisterParams \
                                                  : parameter_count;          \
  static const int kStackParams = parameter_count - kRegisterParams;          \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data)          \
      override {                                                              \
    DefaultInitializePlatformSpecific(data, kRegisterParams);                 \
  }                                                                           \
  void InitializePlatformIndependent(CallInterfaceDescriptorData* data)       \
      override {                                                              \
    data->InitializePlatformIndependent(kRegisterParams, kStackParams, NULL); \
  }                                                                           \
  name(Isolate* isolate, CallDescriptors::Key key) : base(isolate, key) {}    \
                                                                              \
 public:

#define DECLARE_DESCRIPTOR(name, base)                                         \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                     \
 protected:                                                                    \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override; \
  name(Isolate* isolate, CallDescriptors::Key key) : base(isolate, key) {}     \
                                                                               \
 public:

#define DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(name, base)        \
  DECLARE_DESCRIPTOR(name, base)                                        \
 protected:                                                             \
  void InitializePlatformIndependent(CallInterfaceDescriptorData* data) \
      override;                                                         \
                                                                        \
 public:

#define DECLARE_DESCRIPTOR_WITH_STACK_ARGS(name, base)                  \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                              \
 protected:                                                             \
  void InitializePlatformIndependent(CallInterfaceDescriptorData* data) \
      override {                                                        \
    data->InitializePlatformIndependent(0, kParameterCount, NULL);      \
  }                                                                     \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data)    \
      override {                                                        \
    data->InitializePlatformSpecific(0, nullptr);                       \
  }                                                                     \
                                                                        \
 public:

#define DEFINE_EMPTY_PARAMETERS()                       \
  enum ParameterIndices {                               \
    kParameterCount,                                    \
    kContext = kParameterCount /* implicit parameter */ \
  };

#define DEFINE_PARAMETERS(...)                          \
  enum ParameterIndices {                               \
    __VA_ARGS__,                                        \
                                                        \
    kParameterCount,                                    \
    kContext = kParameterCount /* implicit parameter */ \
  };

#define DECLARE_BUILTIN_DESCRIPTOR(name)                                      \
  DECLARE_DESCRIPTOR_WITH_BASE(name, BuiltinDescriptor)                       \
 protected:                                                                   \
  void InitializePlatformIndependent(CallInterfaceDescriptorData* data)       \
      override {                                                              \
    MachineType machine_types[] = {MachineType::AnyTagged(),                  \
                                   MachineType::AnyTagged(),                  \
                                   MachineType::Int32()};                     \
    data->InitializePlatformIndependent(arraysize(machine_types),             \
                                        kStackParameterCount, machine_types); \
  }                                                                           \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data)          \
      override {                                                              \
    Register registers[] = {TargetRegister(), NewTargetRegister(),            \
                            ArgumentsCountRegister()};                        \
    data->InitializePlatformSpecific(arraysize(registers), registers);        \
  }                                                                           \
                                                                              \
 public:

#define DEFINE_BUILTIN_PARAMETERS(...)                                  \
  enum ParameterIndices {                                               \
    kReceiver,                                                          \
    kBeforeFirstStackParameter = kReceiver,                             \
    __VA_ARGS__,                                                        \
    kAfterLastStackParameter,                                           \
    kNewTarget = kAfterLastStackParameter,                              \
    kArgumentsCount,                                                    \
    kContext, /* implicit parameter */                                  \
    kParameterCount = kContext,                                         \
    kArity = kAfterLastStackParameter - kBeforeFirstStackParameter - 1, \
    kStackParameterCount = kArity + 1                                   \
  };

class VoidDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(VoidDescriptor, CallInterfaceDescriptor)
};

class ContextOnlyDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ContextOnlyDescriptor, CallInterfaceDescriptor)
};

// LoadDescriptor is used by all stubs that implement Load/KeyedLoad ICs.
class LoadDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadDescriptor,
                                               CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register SlotRegister();
};

// LoadFieldDescriptor is used by the shared handler that loads a field from an
// object based on the smi-encoded field description.
class LoadFieldDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kSmiHandler)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadFieldDescriptor,
                                               CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register SmiHandlerRegister();
};

class LoadGlobalDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kName, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadGlobalDescriptor,
                                               CallInterfaceDescriptor)

  static const Register NameRegister() {
    return LoadDescriptor::NameRegister();
  }

  static const Register SlotRegister() {
    return LoadDescriptor::SlotRegister();
  }
};

class StoreDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreDescriptor,
                                               CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register ValueRegister();
  static const Register SlotRegister();

#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X87
  static const bool kPassLastArgsOnStack = true;
#else
  static const bool kPassLastArgsOnStack = false;
#endif

  // Pass value and slot through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 2 : 0;
};

class StoreTransitionDescriptor : public StoreDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kMap, kValue, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreTransitionDescriptor,
                                               StoreDescriptor)

  static const Register MapRegister();
  static const Register SlotRegister();
  static const Register VectorRegister();

  // Pass value, slot and vector through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 3 : 0;
};

class StoreNamedTransitionDescriptor : public StoreTransitionDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kFieldOffset, kMap, kValue, kSlot, kVector,
                    kName)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreNamedTransitionDescriptor,
                                               StoreTransitionDescriptor)

  // Always pass name on the stack.
  static const bool kPassLastArgsOnStack = true;
  static const int kStackArgumentsCount =
      StoreTransitionDescriptor::kStackArgumentsCount + 1;

  static const Register NameRegister() { return no_reg; }
  static const Register FieldOffsetRegister() {
    return StoreTransitionDescriptor::NameRegister();
  }
};

class StoreWithVectorDescriptor : public StoreDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreWithVectorDescriptor,
                                               StoreDescriptor)

  static const Register VectorRegister();

  // Pass value, slot and vector through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 3 : 0;
};

class LoadWithVectorDescriptor : public LoadDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadWithVectorDescriptor,
                                               LoadDescriptor)

  static const Register VectorRegister();
};

class LoadICProtoArrayDescriptor : public LoadWithVectorDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot, kVector, kHandler)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadICProtoArrayDescriptor,
                                               LoadWithVectorDescriptor)

  static const Register HandlerRegister();
};

class LoadGlobalWithVectorDescriptor : public LoadGlobalDescriptor {
 public:
  DEFINE_PARAMETERS(kName, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadGlobalWithVectorDescriptor,
                                               LoadGlobalDescriptor)

  static const Register VectorRegister() {
    return LoadWithVectorDescriptor::VectorRegister();
  }
};

class FastNewClosureDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kSharedFunctionInfo, kVector, kSlot)
  DECLARE_DESCRIPTOR(FastNewClosureDescriptor, CallInterfaceDescriptor)
};

class FastNewFunctionContextDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kSlots)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastNewFunctionContextDescriptor,
                                               CallInterfaceDescriptor)

  static const Register FunctionRegister();
  static const Register SlotsRegister();
};

class FastNewObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget)
  DECLARE_DESCRIPTOR(FastNewObjectDescriptor, CallInterfaceDescriptor)
  static const Register TargetRegister();
  static const Register NewTargetRegister();
};

class FastNewArgumentsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction)
  DECLARE_DESCRIPTOR(FastNewArgumentsDescriptor, CallInterfaceDescriptor)
  static const Register TargetRegister();
};

class TypeConversionDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kArgument)
  DECLARE_DESCRIPTOR(TypeConversionDescriptor, CallInterfaceDescriptor)

  static const Register ArgumentRegister();
};

class ForInPrepareDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject)
  DECLARE_DEFAULT_DESCRIPTOR(ForInPrepareDescriptor, CallInterfaceDescriptor,
                             kParameterCount)
};

class GetPropertyDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DECLARE_DEFAULT_DESCRIPTOR(GetPropertyDescriptor, CallInterfaceDescriptor,
                             kParameterCount)
};

class TypeofDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject)
  DECLARE_DESCRIPTOR(TypeofDescriptor, CallInterfaceDescriptor)
};


class FastCloneRegExpDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kClosure, kLiteralIndex, kPattern, kFlags)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastCloneRegExpDescriptor,
                                               CallInterfaceDescriptor)
};


class FastCloneShallowArrayDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kClosure, kLiteralIndex, kConstantElements)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastCloneShallowArrayDescriptor,
                                               CallInterfaceDescriptor)
};


class FastCloneShallowObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kClosure, kLiteralIndex, kBoilerplateDescription, kFlags)
  DECLARE_DESCRIPTOR(FastCloneShallowObjectDescriptor, CallInterfaceDescriptor)
};


class CreateAllocationSiteDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kVector, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CreateAllocationSiteDescriptor,
                                               CallInterfaceDescriptor)
};


class CreateWeakCellDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kVector, kSlot, kValue)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CreateWeakCellDescriptor,
                                               CallInterfaceDescriptor)
};


class CallTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CallTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};

class CallForwardVarargsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kActualArgumentsCount, kStartIndex)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CallForwardVarargsDescriptor,
                                               CallInterfaceDescriptor)
};

class ConstructForwardVarargsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget, kActualArgumentsCount, kStartIndex)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ConstructForwardVarargsDescriptor, CallInterfaceDescriptor)
};

class ConstructStubDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kNewTarget, kActualArgumentsCount,
                    kAllocationSite)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ConstructStubDescriptor,
                                               CallInterfaceDescriptor)
};


class ConstructTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kNewTarget, kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ConstructTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};


class CallFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallFunctionDescriptor, CallInterfaceDescriptor)
};

class CallICDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kActualArgumentsCount, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CallICDescriptor,
                                               CallInterfaceDescriptor)
};

class CallICTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kActualArgumentsCount, kSlot)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CallICTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};

class CallConstructDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallConstructDescriptor, CallInterfaceDescriptor)
};

class TransitionElementsKindDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kMap)
  DECLARE_DESCRIPTOR(TransitionElementsKindDescriptor, CallInterfaceDescriptor)
};


class AllocateHeapNumberDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_EMPTY_PARAMETERS()
  DECLARE_DESCRIPTOR(AllocateHeapNumberDescriptor, CallInterfaceDescriptor)
};

class BuiltinDescriptor : public CallInterfaceDescriptor {
 public:
  // TODO(ishell): Where is kFunction??
  DEFINE_PARAMETERS(kNewTarget, kArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(BuiltinDescriptor,
                                               CallInterfaceDescriptor)
  static const Register ArgumentsCountRegister();
  static const Register NewTargetRegister();
  static const Register TargetRegister();
};

class IteratingArrayBuiltinDescriptor : public BuiltinDescriptor {
 public:
  DEFINE_BUILTIN_PARAMETERS(kCallback, kThisArg)
  DECLARE_BUILTIN_DESCRIPTOR(IteratingArrayBuiltinDescriptor)
};

class ArrayConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget, kActualArgumentsCount, kAllocationSite)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ArrayConstructorDescriptor,
                                               CallInterfaceDescriptor)
};

class ArrayNoArgumentConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount,
                    kFunctionParameter)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ArrayNoArgumentConstructorDescriptor, CallInterfaceDescriptor)
};

class ArraySingleArgumentConstructorDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount,
                    kFunctionParameter, kArraySizeSmiParameter)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ArraySingleArgumentConstructorDescriptor, CallInterfaceDescriptor)
};

class ArrayNArgumentsConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ArrayNArgumentsConstructorDescriptor, CallInterfaceDescriptor)
};


class CompareDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(CompareDescriptor, CallInterfaceDescriptor)
};


class BinaryOpDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(BinaryOpDescriptor, CallInterfaceDescriptor)
};


class BinaryOpWithAllocationSiteDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kAllocationSite, kLeft, kRight)
  DECLARE_DESCRIPTOR(BinaryOpWithAllocationSiteDescriptor,
                     CallInterfaceDescriptor)
};

class BinaryOpWithVectorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight, kSlot, kVector)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(BinaryOpWithVectorDescriptor,
                                               CallInterfaceDescriptor)
};

class CountOpDescriptor final : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CountOpDescriptor, CallInterfaceDescriptor)
};

class StringAddDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(StringAddDescriptor, CallInterfaceDescriptor)
};

class StringCharAtDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kPosition)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StringCharAtDescriptor,
                                               CallInterfaceDescriptor)
};

class StringCharCodeAtDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kPosition)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StringCharCodeAtDescriptor,
                                               CallInterfaceDescriptor)
};

class StringCompareDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(StringCompareDescriptor, CallInterfaceDescriptor)

  static const Register LeftRegister();
  static const Register RightRegister();
};

class SubStringDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kString, kFrom, kTo)
  DECLARE_DESCRIPTOR_WITH_STACK_ARGS(SubStringDescriptor,
                                     CallInterfaceDescriptor)
};

class ArgumentAdaptorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kNewTarget, kActualArgumentsCount,
                    kExpectedArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ArgumentAdaptorDescriptor,
                                               CallInterfaceDescriptor)
};

class ApiCallbackDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kCallData, kHolder, kApiFunctionAddress)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ApiCallbackDescriptor,
                                               CallInterfaceDescriptor)
};

class ApiGetterDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kHolder, kCallback)
  DECLARE_DESCRIPTOR(ApiGetterDescriptor, CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register HolderRegister();
  static const Register CallbackRegister();
};

class MathPowTaggedDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kExponent)
  DECLARE_DESCRIPTOR(MathPowTaggedDescriptor, CallInterfaceDescriptor)

  static const Register exponent();
};

class MathPowIntegerDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kExponent)
  DECLARE_DESCRIPTOR(MathPowIntegerDescriptor, CallInterfaceDescriptor)

  static const Register exponent();
};

class VarArgFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kActualArgumentsCount)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(VarArgFunctionDescriptor,
                                               CallInterfaceDescriptor)
};

// TODO(turbofan): We should probably rename this to GrowFastElementsDescriptor.
class GrowArrayElementsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DECLARE_DESCRIPTOR(GrowArrayElementsDescriptor, CallInterfaceDescriptor)

  static const Register ObjectRegister();
  static const Register KeyRegister();
};

class NewArgumentsElementsDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFrame, kLength)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(NewArgumentsElementsDescriptor,
                                               CallInterfaceDescriptor)
};

class V8_EXPORT_PRIVATE InterpreterDispatchDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kAccumulator, kBytecodeOffset, kBytecodeArray,
                    kDispatchTable)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(InterpreterDispatchDescriptor,
                                               CallInterfaceDescriptor)
};

class InterpreterPushArgsThenCallDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kFirstArgument, kFunction)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      InterpreterPushArgsThenCallDescriptor, CallInterfaceDescriptor)
};

class InterpreterPushArgsThenConstructDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kNewTarget, kConstructor,
                    kFeedbackElement, kFirstArgument)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      InterpreterPushArgsThenConstructDescriptor, CallInterfaceDescriptor)
};

class InterpreterPushArgsThenConstructArrayDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kFunction, kFeedbackElement,
                    kFirstArgument)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      InterpreterPushArgsThenConstructArrayDescriptor, CallInterfaceDescriptor)
};

class InterpreterCEntryDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kFirstArgument, kFunctionEntry)
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(InterpreterCEntryDescriptor,
                                               CallInterfaceDescriptor)
};

class ResumeGeneratorDescriptor final : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ResumeGeneratorDescriptor, CallInterfaceDescriptor)
};

class FrameDropperTrampolineDescriptor final : public CallInterfaceDescriptor {
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FrameDropperTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};

class WasmRuntimeCallDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_EMPTY_PARAMETERS()
  DECLARE_DEFAULT_DESCRIPTOR(WasmRuntimeCallDescriptor, CallInterfaceDescriptor,
                             0)
};

#define DEFINE_TFS_BUILTIN_DESCRIPTOR(Name, ...)                          \
  class Name##Descriptor : public CallInterfaceDescriptor {               \
   public:                                                                \
    DEFINE_PARAMETERS(__VA_ARGS__)                                        \
    DECLARE_DEFAULT_DESCRIPTOR(Name##Descriptor, CallInterfaceDescriptor, \
                               kParameterCount)                           \
  };
BUILTIN_LIST_TFS(DEFINE_TFS_BUILTIN_DESCRIPTOR)
#undef DEFINE_TFS_BUILTIN_DESCRIPTOR

#undef DECLARE_DEFAULT_DESCRIPTOR
#undef DECLARE_DESCRIPTOR_WITH_BASE
#undef DECLARE_DESCRIPTOR
#undef DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE
#undef DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG
#undef DEFINE_PARAMETERS

// We define the association between CallDescriptors::Key and the specialized
// descriptor here to reduce boilerplate and mistakes.
#define DEF_KEY(name, ...) \
  CallDescriptors::Key name##Descriptor::key() { return CallDescriptors::name; }
INTERFACE_DESCRIPTOR_LIST(DEF_KEY)
#undef DEF_KEY
}  // namespace internal
}  // namespace v8


#if V8_TARGET_ARCH_ARM64
#include "src/arm64/interface-descriptors-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/interface-descriptors-arm.h"
#endif

#endif  // V8_CALL_INTERFACE_DESCRIPTOR_H_

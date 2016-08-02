// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CALL_INTERFACE_DESCRIPTOR_H_
#define V8_CALL_INTERFACE_DESCRIPTOR_H_

#include "src/assembler.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

class PlatformInterfaceDescriptor;

#define INTERFACE_DESCRIPTOR_LIST(V)          \
  V(Void)                                     \
  V(Load)                                     \
  V(Store)                                    \
  V(StoreTransition)                          \
  V(VectorStoreTransition)                    \
  V(VectorStoreICTrampoline)                  \
  V(VectorStoreIC)                            \
  V(LoadWithVector)                           \
  V(FastArrayPush)                            \
  V(FastNewClosure)                           \
  V(FastNewContext)                           \
  V(FastNewObject)                            \
  V(FastNewRestParameter)                     \
  V(FastNewSloppyArguments)                   \
  V(FastNewStrictArguments)                   \
  V(TypeConversion)                           \
  V(Typeof)                                   \
  V(FastCloneRegExp)                          \
  V(FastCloneShallowArray)                    \
  V(FastCloneShallowObject)                   \
  V(CreateAllocationSite)                     \
  V(CreateWeakCell)                           \
  V(CallFunction)                             \
  V(CallFunctionWithFeedback)                 \
  V(CallFunctionWithFeedbackAndVector)        \
  V(CallConstruct)                            \
  V(CallTrampoline)                           \
  V(ConstructStub)                            \
  V(ConstructTrampoline)                      \
  V(RegExpConstructResult)                    \
  V(TransitionElementsKind)                   \
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
  V(ArrayConstructorConstantArgCount)         \
  V(ArrayConstructor)                         \
  V(InternalArrayConstructorConstantArgCount) \
  V(InternalArrayConstructor)                 \
  V(Compare)                                  \
  V(BinaryOp)                                 \
  V(BinaryOpWithAllocationSite)               \
  V(CountOp)                                  \
  V(StringAdd)                                \
  V(StringCompare)                            \
  V(Keyed)                                    \
  V(Named)                                    \
  V(HasProperty)                              \
  V(CallHandler)                              \
  V(ArgumentAdaptor)                          \
  V(ApiCallbackWith0Args)                     \
  V(ApiCallbackWith1Args)                     \
  V(ApiCallbackWith2Args)                     \
  V(ApiCallbackWith3Args)                     \
  V(ApiCallbackWith4Args)                     \
  V(ApiCallbackWith5Args)                     \
  V(ApiCallbackWith6Args)                     \
  V(ApiCallbackWith7Args)                     \
  V(ApiGetter)                                \
  V(LoadGlobalViaContext)                     \
  V(StoreGlobalViaContext)                    \
  V(MathPowTagged)                            \
  V(MathPowInteger)                           \
  V(ContextOnly)                              \
  V(GrowArrayElements)                        \
  V(InterpreterDispatch)                      \
  V(InterpreterPushArgsAndCall)               \
  V(InterpreterPushArgsAndConstruct)          \
  V(InterpreterCEntry)                        \
  V(ResumeGenerator)

class CallInterfaceDescriptorData {
 public:
  CallInterfaceDescriptorData()
      : register_param_count_(-1), function_type_(nullptr) {}

  // A copy of the passed in registers and param_representations is made
  // and owned by the CallInterfaceDescriptorData.

  void InitializePlatformIndependent(FunctionType* function_type) {
    function_type_ = function_type;
  }

  // TODO(mvstanton): Instead of taking parallel arrays register and
  // param_representations, how about a struct that puts the representation
  // and register side by side (eg, RegRep(r1, Representation::Tagged()).
  // The same should go for the CodeStubDescriptor class.
  void InitializePlatformSpecific(
      int register_parameter_count, Register* registers,
      PlatformInterfaceDescriptor* platform_descriptor = NULL);

  bool IsInitialized() const { return register_param_count_ >= 0; }

  int param_count() const { return function_type_->Arity(); }
  int register_param_count() const { return register_param_count_; }
  Register register_param(int index) const { return register_params_[index]; }
  Register* register_params() const { return register_params_.get(); }
  Type* param_type(int index) const { return function_type_->Parameter(index); }
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return platform_specific_descriptor_;
  }

  FunctionType* function_type() const { return function_type_; }

 private:
  int register_param_count_;

  // The Register params are allocated dynamically by the
  // InterfaceDescriptor, and freed on destruction. This is because static
  // arrays of Registers cause creation of runtime static initializers
  // which we don't want.
  base::SmartArrayPointer<Register> register_params_;

  // Specifies types for parameters and return
  FunctionType* function_type_;

  PlatformInterfaceDescriptor* platform_specific_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(CallInterfaceDescriptorData);
};


class CallDescriptors {
 public:
  enum Key {
#define DEF_ENUM(name) name,
    INTERFACE_DESCRIPTOR_LIST(DEF_ENUM)
#undef DEF_ENUM
    NUMBER_OF_DESCRIPTORS
  };
};


class CallInterfaceDescriptor {
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
    return data()->function_type()->Arity() - data()->register_param_count();
  }

  Register GetRegisterParameter(int index) const {
    return data()->register_param(index);
  }

  Type* GetParameterType(int index) const {
    DCHECK(index < data()->param_count());
    return data()->param_type(index);
  }

  // Some platforms have extra information to associate with the descriptor.
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return data()->platform_specific_descriptor();
  }

  FunctionType* GetFunctionType() const { return data()->function_type(); }

  static const Register ContextRegister();

  const char* DebugName(Isolate* isolate) const;

  static FunctionType* BuildDefaultFunctionType(Isolate* isolate,
                                                int paramater_count);

 protected:
  const CallInterfaceDescriptorData* data() const { return data_; }

  virtual FunctionType* BuildCallInterfaceDescriptorFunctionType(
      Isolate* isolate, int register_param_count) {
    return BuildDefaultFunctionType(isolate, register_param_count);
  }

  virtual void InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
    UNREACHABLE();
  }

  void Initialize(Isolate* isolate, CallDescriptors::Key key) {
    if (!data()->IsInitialized()) {
      CallInterfaceDescriptorData* d = isolate->call_descriptor_data(key);
      DCHECK(d == data());  // d should be a modifiable pointer to data().
      InitializePlatformSpecific(d);
      FunctionType* function_type = BuildCallInterfaceDescriptorFunctionType(
          isolate, d->register_param_count());
      d->InitializePlatformIndependent(function_type);
    }
  }

 private:
  const CallInterfaceDescriptorData* data_;
};

#define DECLARE_DESCRIPTOR_WITH_BASE(name, base)           \
 public:                                                   \
  explicit name(Isolate* isolate) : base(isolate, key()) { \
    Initialize(isolate, key());                            \
  }                                                        \
  static inline CallDescriptors::Key key();

#define DECLARE_DESCRIPTOR(name, base)                                         \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                     \
 protected:                                                                    \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override; \
  name(Isolate* isolate, CallDescriptors::Key key) : base(isolate, key) {}     \
                                                                               \
 public:

#define DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(name, base) \
  DECLARE_DESCRIPTOR(name, base)                                 \
 protected:                                                      \
  FunctionType* BuildCallInterfaceDescriptorFunctionType(        \
      Isolate* isolate, int register_param_count) override;      \
                                                                 \
 public:

#define DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(name, base, arg) \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                  \
 protected:                                                                 \
  FunctionType* BuildCallInterfaceDescriptorFunctionType(                   \
      Isolate* isolate, int register_param_count) override {                \
    return BuildCallInterfaceDescriptorFunctionTypeWithArg(                 \
        isolate, register_param_count, arg);                                \
  }                                                                         \
                                                                            \
 public:

class VoidDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(VoidDescriptor, CallInterfaceDescriptor)
};


// LoadDescriptor is used by all stubs that implement Load/KeyedLoad ICs.
class LoadDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadDescriptor,
                                               CallInterfaceDescriptor)

  enum ParameterIndices { kReceiverIndex, kNameIndex, kSlotIndex };
  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register SlotRegister();
};


class StoreDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(StoreDescriptor, CallInterfaceDescriptor)

  enum ParameterIndices {
    kReceiverIndex,
    kNameIndex,
    kValueIndex,
    kParameterCount
  };
  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register ValueRegister();
};


class StoreTransitionDescriptor : public StoreDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreTransitionDescriptor,
                                               StoreDescriptor)

  // Extends StoreDescriptor with Map parameter.
  enum ParameterIndices {
    kReceiverIndex,
    kNameIndex,
    kValueIndex,
    kMapIndex,
    kParameterCount
  };

  static const Register MapRegister();
};


class VectorStoreTransitionDescriptor : public StoreDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(VectorStoreTransitionDescriptor,
                                               StoreDescriptor)

  // Extends StoreDescriptor with Map parameter.
  enum ParameterIndices {
    kReceiverIndex = 0,
    kNameIndex = 1,
    kValueIndex = 2,

    kMapIndex = 3,

    kSlotIndex = 4,  // not present on ia32.
    kVirtualSlotVectorIndex = 4,

    kVectorIndex = 5
  };

  static const Register MapRegister();
  static const Register SlotRegister();
  static const Register VectorRegister();
};


class VectorStoreICTrampolineDescriptor : public StoreDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      VectorStoreICTrampolineDescriptor, StoreDescriptor)

  enum ParameterIndices { kReceiverIndex, kNameIndex, kValueIndex, kSlotIndex };

  static const Register SlotRegister();
};


class VectorStoreICDescriptor : public VectorStoreICTrampolineDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      VectorStoreICDescriptor, VectorStoreICTrampolineDescriptor)

  enum ParameterIndices {
    kReceiverIndex,
    kNameIndex,
    kValueIndex,
    kSlotIndex,
    kVectorIndex
  };

  static const Register VectorRegister();
};


class LoadWithVectorDescriptor : public LoadDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadWithVectorDescriptor,
                                               LoadDescriptor)

  enum ParameterIndices {
    kReceiverIndex,
    kNameIndex,
    kSlotIndex,
    kVectorIndex
  };

  static const Register VectorRegister();
};


class FastNewClosureDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewClosureDescriptor, CallInterfaceDescriptor)
};


class FastNewContextDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewContextDescriptor, CallInterfaceDescriptor)
};

class FastNewObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewObjectDescriptor, CallInterfaceDescriptor)
};

class FastNewRestParameterDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewRestParameterDescriptor, CallInterfaceDescriptor)
};

class FastNewSloppyArgumentsDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewSloppyArgumentsDescriptor,
                     CallInterfaceDescriptor)
};

class FastNewStrictArgumentsDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastNewStrictArgumentsDescriptor,
                     CallInterfaceDescriptor)
};

class TypeConversionDescriptor final : public CallInterfaceDescriptor {
 public:
  enum ParameterIndices { kArgumentIndex };

  DECLARE_DESCRIPTOR(TypeConversionDescriptor, CallInterfaceDescriptor)

  static const Register ArgumentRegister();
};

class HasPropertyDescriptor final : public CallInterfaceDescriptor {
 public:
  enum ParameterIndices { kKeyIndex, kObjectIndex };

  DECLARE_DESCRIPTOR(HasPropertyDescriptor, CallInterfaceDescriptor)

  static const Register KeyRegister();
  static const Register ObjectRegister();
};

class TypeofDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(TypeofDescriptor, CallInterfaceDescriptor)
};


class FastCloneRegExpDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastCloneRegExpDescriptor,
                                               CallInterfaceDescriptor)
};


class FastCloneShallowArrayDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastCloneShallowArrayDescriptor,
                                               CallInterfaceDescriptor)
};


class FastCloneShallowObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastCloneShallowObjectDescriptor, CallInterfaceDescriptor)
};


class CreateAllocationSiteDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CreateAllocationSiteDescriptor,
                                               CallInterfaceDescriptor)
};


class CreateWeakCellDescriptor : public CallInterfaceDescriptor {
 public:
  enum ParameterIndices {
    kVectorIndex,
    kSlotIndex,
    kValueIndex,
    kParameterCount
  };

  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CreateWeakCellDescriptor,
                                               CallInterfaceDescriptor)
};


class CallTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(CallTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};


class ConstructStubDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ConstructStubDescriptor,
                                               CallInterfaceDescriptor)
};


class ConstructTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ConstructTrampolineDescriptor,
                                               CallInterfaceDescriptor)
};


class CallFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallFunctionDescriptor, CallInterfaceDescriptor)
};


class CallFunctionWithFeedbackDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      CallFunctionWithFeedbackDescriptor, CallInterfaceDescriptor)
};


class CallFunctionWithFeedbackAndVectorDescriptor
    : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      CallFunctionWithFeedbackAndVectorDescriptor, CallInterfaceDescriptor)
};


class CallConstructDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallConstructDescriptor, CallInterfaceDescriptor)
};


class RegExpConstructResultDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(RegExpConstructResultDescriptor, CallInterfaceDescriptor)
};


class LoadGlobalViaContextDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(LoadGlobalViaContextDescriptor,
                                               CallInterfaceDescriptor)

  static const Register SlotRegister();
};


class StoreGlobalViaContextDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(StoreGlobalViaContextDescriptor,
                                               CallInterfaceDescriptor)

  static const Register SlotRegister();
  static const Register ValueRegister();
};


class TransitionElementsKindDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(TransitionElementsKindDescriptor, CallInterfaceDescriptor)
};


class AllocateHeapNumberDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(AllocateHeapNumberDescriptor, CallInterfaceDescriptor)
};

#define SIMD128_ALLOC_DESC(TYPE, Type, type, lane_count, lane_type)         \
  class Allocate##Type##Descriptor : public CallInterfaceDescriptor {       \
   public:                                                                  \
    DECLARE_DESCRIPTOR(Allocate##Type##Descriptor, CallInterfaceDescriptor) \
  };
SIMD128_TYPES(SIMD128_ALLOC_DESC)
#undef SIMD128_ALLOC_DESC

class ArrayNoArgumentConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      ArrayNoArgumentConstructorDescriptor, CallInterfaceDescriptor)
  enum ParameterIndices {
    kFunctionIndex,
    kAllocationSiteIndex,
    kArgumentCountIndex,
    kContextIndex
  };
};

class ArrayConstructorConstantArgCountDescriptor
    : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ArrayConstructorConstantArgCountDescriptor,
                     CallInterfaceDescriptor)
};


class ArrayConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ArrayConstructorDescriptor,
                                               CallInterfaceDescriptor)
};


class InternalArrayConstructorConstantArgCountDescriptor
    : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InternalArrayConstructorConstantArgCountDescriptor,
                     CallInterfaceDescriptor)
};


class InternalArrayConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(
      InternalArrayConstructorDescriptor, CallInterfaceDescriptor)
};


class CompareDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CompareDescriptor, CallInterfaceDescriptor)
};


class BinaryOpDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(BinaryOpDescriptor, CallInterfaceDescriptor)
};


class BinaryOpWithAllocationSiteDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(BinaryOpWithAllocationSiteDescriptor,
                     CallInterfaceDescriptor)
};

class CountOpDescriptor final : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CountOpDescriptor, CallInterfaceDescriptor)
};

class StringAddDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(StringAddDescriptor, CallInterfaceDescriptor)
};


class StringCompareDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(StringCompareDescriptor, CallInterfaceDescriptor)

  enum ParameterIndices { kLeftIndex, kRightIndex, kParameterCount };
  static const Register LeftRegister();
  static const Register RightRegister();
};


class KeyedDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(KeyedDescriptor, CallInterfaceDescriptor)
};


class NamedDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(NamedDescriptor, CallInterfaceDescriptor)
};


class CallHandlerDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallHandlerDescriptor, CallInterfaceDescriptor)
};


class ArgumentAdaptorDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ArgumentAdaptorDescriptor,
                                               CallInterfaceDescriptor)
};

// The ApiCallback*Descriptors have a lot of boilerplate. The superclass
// ApiCallbackDescriptorBase contains all the logic, and the
// ApiCallbackWith*ArgsDescriptor merely instantiate these with a
// parameter for the number of args.
//
// The base class is not meant to be instantiated directly and has no
// public constructors to ensure this is so.
//
// The simplest usage for all the ApiCallback*Descriptors is probably
//   ApiCallbackDescriptorBase::ForArgs(isolate, argc)
//
class ApiCallbackDescriptorBase : public CallInterfaceDescriptor {
 public:
  static CallInterfaceDescriptor ForArgs(Isolate* isolate, int argc);

 protected:
  ApiCallbackDescriptorBase(Isolate* isolate, CallDescriptors::Key key)
      : CallInterfaceDescriptor(isolate, key) {}
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override;
  FunctionType* BuildCallInterfaceDescriptorFunctionTypeWithArg(
      Isolate* isolate, int parameter_count, int argc);
};

class ApiCallbackWith0ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith0ArgsDescriptor, ApiCallbackDescriptorBase, 0)
};

class ApiCallbackWith1ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith1ArgsDescriptor, ApiCallbackDescriptorBase, 1)
};

class ApiCallbackWith2ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith2ArgsDescriptor, ApiCallbackDescriptorBase, 2)
};

class ApiCallbackWith3ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith3ArgsDescriptor, ApiCallbackDescriptorBase, 3)
};

class ApiCallbackWith4ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith4ArgsDescriptor, ApiCallbackDescriptorBase, 4)
};

class ApiCallbackWith5ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith5ArgsDescriptor, ApiCallbackDescriptorBase, 5)
};

class ApiCallbackWith6ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith6ArgsDescriptor, ApiCallbackDescriptorBase, 6)
};

class ApiCallbackWith7ArgsDescriptor : public ApiCallbackDescriptorBase {
 public:
  DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG(
      ApiCallbackWith7ArgsDescriptor, ApiCallbackDescriptorBase, 7)
};


class ApiGetterDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ApiGetterDescriptor, CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register HolderRegister();
  static const Register CallbackRegister();
};


class MathPowTaggedDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(MathPowTaggedDescriptor, CallInterfaceDescriptor)

  static const Register exponent();
};


class MathPowIntegerDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(MathPowIntegerDescriptor, CallInterfaceDescriptor)

  static const Register exponent();
};


class ContextOnlyDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ContextOnlyDescriptor, CallInterfaceDescriptor)
};

class FastArrayPushDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(FastArrayPushDescriptor,
                                               CallInterfaceDescriptor)
};

class GrowArrayElementsDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(GrowArrayElementsDescriptor, CallInterfaceDescriptor)

  enum RegisterInfo { kObjectIndex, kKeyIndex };
  static const Register ObjectRegister();
  static const Register KeyRegister();
};

class InterpreterDispatchDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(InterpreterDispatchDescriptor,
                                               CallInterfaceDescriptor)

  static const int kAccumulatorParameter = 0;
  static const int kBytecodeOffsetParameter = 1;
  static const int kBytecodeArrayParameter = 2;
  static const int kDispatchTableParameter = 3;
};

class InterpreterPushArgsAndCallDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InterpreterPushArgsAndCallDescriptor,
                     CallInterfaceDescriptor)
};


class InterpreterPushArgsAndConstructDescriptor
    : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InterpreterPushArgsAndConstructDescriptor,
                     CallInterfaceDescriptor)
};


class InterpreterCEntryDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InterpreterCEntryDescriptor, CallInterfaceDescriptor)
};

class ResumeGeneratorDescriptor final : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ResumeGeneratorDescriptor, CallInterfaceDescriptor)
};

#undef DECLARE_DESCRIPTOR_WITH_BASE
#undef DECLARE_DESCRIPTOR
#undef DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE
#undef DECLARE_DESCRIPTOR_WITH_BASE_AND_FUNCTION_TYPE_ARG

// We define the association between CallDescriptors::Key and the specialized
// descriptor here to reduce boilerplate and mistakes.
#define DEF_KEY(name) \
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

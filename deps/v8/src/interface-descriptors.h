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
  V(Load)                                     \
  V(Store)                                    \
  V(StoreTransition)                          \
  V(ElementTransitionAndStore)                \
  V(VectorStoreICTrampoline)                  \
  V(VectorStoreIC)                            \
  V(Instanceof)                               \
  V(LoadWithVector)                           \
  V(FastNewClosure)                           \
  V(FastNewContext)                           \
  V(ToNumber)                                 \
  V(NumberToString)                           \
  V(Typeof)                                   \
  V(FastCloneShallowArray)                    \
  V(FastCloneShallowObject)                   \
  V(CreateAllocationSite)                     \
  V(CreateWeakCell)                           \
  V(CallFunction)                             \
  V(CallFunctionWithFeedback)                 \
  V(CallFunctionWithFeedbackAndVector)        \
  V(CallConstruct)                            \
  V(RegExpConstructResult)                    \
  V(TransitionElementsKind)                   \
  V(AllocateHeapNumber)                       \
  V(ArrayConstructorConstantArgCount)         \
  V(ArrayConstructor)                         \
  V(InternalArrayConstructorConstantArgCount) \
  V(InternalArrayConstructor)                 \
  V(Compare)                                  \
  V(CompareNil)                               \
  V(ToBoolean)                                \
  V(BinaryOp)                                 \
  V(BinaryOpWithAllocationSite)               \
  V(StringAdd)                                \
  V(Keyed)                                    \
  V(Named)                                    \
  V(CallHandler)                              \
  V(ArgumentAdaptor)                          \
  V(ApiFunction)                              \
  V(ApiAccessor)                              \
  V(ApiGetter)                                \
  V(ArgumentsAccessRead)                      \
  V(StoreArrayLiteralElement)                 \
  V(MathPowTagged)                            \
  V(MathPowInteger)                           \
  V(ContextOnly)                              \
  V(GrowArrayElements)                        \
  V(MathRoundVariant)


class CallInterfaceDescriptorData {
 public:
  CallInterfaceDescriptorData()
      : stack_paramater_count_(-1),
        register_param_count_(-1),
        function_type_(nullptr) {}

  // A copy of the passed in registers and param_representations is made
  // and owned by the CallInterfaceDescriptorData.

  void InitializePlatformIndependent(int stack_paramater_count,
                                     Type::FunctionType* function_type) {
    function_type_ = function_type;
    stack_paramater_count_ = stack_paramater_count;
  }

  // TODO(mvstanton): Instead of taking parallel arrays register and
  // param_representations, how about a struct that puts the representation
  // and register side by side (eg, RegRep(r1, Representation::Tagged()).
  // The same should go for the CodeStubDescriptor class.
  void InitializePlatformSpecific(
      int register_parameter_count, Register* registers,
      PlatformInterfaceDescriptor* platform_descriptor = NULL);

  bool IsInitialized() const { return register_param_count_ >= 0; }

  int register_param_count() const { return register_param_count_; }
  Register register_param(int index) const { return register_params_[index]; }
  Register* register_params() const { return register_params_.get(); }
  Type* register_param_type(int index) const {
    return function_type_->Parameter(index);
  }
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return platform_specific_descriptor_;
  }

  Type::FunctionType* function_type() const { return function_type_; }

 private:
  int stack_paramater_count_;
  int register_param_count_;

  // The Register params are allocated dynamically by the
  // InterfaceDescriptor, and freed on destruction. This is because static
  // arrays of Registers cause creation of runtime static initializers
  // which we don't want.
  SmartArrayPointer<Register> register_params_;

  // Specifies types for parameters and return
  Type::FunctionType* function_type_;

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
    DCHECK(index < data()->register_param_count());
    return data()->register_param_type(index);
  }

  // Some platforms have extra information to associate with the descriptor.
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return data()->platform_specific_descriptor();
  }

  Type::FunctionType* GetFunctionType() const {
    return data()->function_type();
  }

  static const Register ContextRegister();

  const char* DebugName(Isolate* isolate) const;

  static Type::FunctionType* BuildDefaultFunctionType(Isolate* isolate,
                                                      int paramater_count);

 protected:
  const CallInterfaceDescriptorData* data() const { return data_; }

  virtual Type::FunctionType* BuildCallInterfaceDescriptorFunctionType(
      Isolate* isolate, int register_param_count) {
    return BuildDefaultFunctionType(isolate, register_param_count);
  }

  virtual void InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
    UNREACHABLE();
  }

  void Initialize(Isolate* isolate, CallDescriptors::Key key) {
    if (!data()->IsInitialized()) {
      CallInterfaceDescriptorData* d = isolate->call_descriptor_data(key);
      InitializePlatformSpecific(d);
      Type::FunctionType* function_type =
          BuildCallInterfaceDescriptorFunctionType(isolate,
                                                   d->register_param_count());
      d->InitializePlatformIndependent(0, function_type);
    }
  }

 private:
  const CallInterfaceDescriptorData* data_;
};


#define DECLARE_DESCRIPTOR(name, base)                                         \
  explicit name(Isolate* isolate) : base(isolate, key()) {                     \
    Initialize(isolate, key());                                                \
  }                                                                            \
                                                                               \
 protected:                                                                    \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override; \
  name(Isolate* isolate, CallDescriptors::Key key) : base(isolate, key) {}     \
                                                                               \
 public:                                                                       \
  static inline CallDescriptors::Key key();


#define DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(name, base)        \
  DECLARE_DESCRIPTOR(name, base)                                        \
 protected:                                                             \
  virtual Type::FunctionType* BuildCallInterfaceDescriptorFunctionType( \
      Isolate* isolate, int register_param_count) override;             \
                                                                        \
 public:
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
  DECLARE_DESCRIPTOR(StoreTransitionDescriptor, StoreDescriptor)

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


class ElementTransitionAndStoreDescriptor : public StoreDescriptor {
 public:
  DECLARE_DESCRIPTOR(ElementTransitionAndStoreDescriptor, StoreDescriptor)

  static const Register MapRegister();
};


class InstanceofDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InstanceofDescriptor, CallInterfaceDescriptor)

  enum ParameterIndices { kLeftIndex, kRightIndex, kParameterCount };
  static const Register left();
  static const Register right();
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


class ToNumberDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ToNumberDescriptor, CallInterfaceDescriptor)
};


class NumberToStringDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(NumberToStringDescriptor, CallInterfaceDescriptor)
};


class TypeofDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(TypeofDescriptor, CallInterfaceDescriptor)
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


class TransitionElementsKindDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(TransitionElementsKindDescriptor, CallInterfaceDescriptor)
};


class AllocateHeapNumberDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(AllocateHeapNumberDescriptor, CallInterfaceDescriptor)
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


class CompareNilDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CompareNilDescriptor, CallInterfaceDescriptor)
};


class ToBooleanDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ToBooleanDescriptor, CallInterfaceDescriptor)
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


class StringAddDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(StringAddDescriptor, CallInterfaceDescriptor)
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


class ApiFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ApiFunctionDescriptor,
                                               CallInterfaceDescriptor)
};


class ApiAccessorDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ApiAccessorDescriptor,
                                               CallInterfaceDescriptor)
};


class ApiGetterDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(ApiGetterDescriptor,
                                               CallInterfaceDescriptor)

  static const Register function_address();
};


class ArgumentsAccessReadDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ArgumentsAccessReadDescriptor, CallInterfaceDescriptor)

  static const Register index();
  static const Register parameter_count();
};


class StoreArrayLiteralElementDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(StoreArrayLiteralElementDescriptor,
                     CallInterfaceDescriptor)
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


class MathRoundVariantDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR_WITH_CUSTOM_FUNCTION_TYPE(MathRoundVariantDescriptor,
                                               CallInterfaceDescriptor)
};


class ContextOnlyDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ContextOnlyDescriptor, CallInterfaceDescriptor)
};


class GrowArrayElementsDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(GrowArrayElementsDescriptor, CallInterfaceDescriptor)

  enum RegisterInfo { kObjectIndex, kKeyIndex };
  static const Register ObjectRegister();
  static const Register KeyRegister();
};

#undef DECLARE_DESCRIPTOR


// We define the association between CallDescriptors::Key and the specialized
// descriptor here to reduce boilerplate and mistakes.
#define DEF_KEY(name) \
  CallDescriptors::Key name##Descriptor::key() { return CallDescriptors::name; }
INTERFACE_DESCRIPTOR_LIST(DEF_KEY)
#undef DEF_KEY
}
}  // namespace v8::internal


#if V8_TARGET_ARCH_ARM64
#include "src/arm64/interface-descriptors-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/arm/interface-descriptors-arm.h"
#endif

#endif  // V8_CALL_INTERFACE_DESCRIPTOR_H_

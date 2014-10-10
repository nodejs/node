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
  V(ElementTransitionAndStore)                \
  V(Instanceof)                               \
  V(VectorLoadICTrampoline)                   \
  V(VectorLoadIC)                             \
  V(FastNewClosure)                           \
  V(FastNewContext)                           \
  V(ToNumber)                                 \
  V(NumberToString)                           \
  V(FastCloneShallowArray)                    \
  V(FastCloneShallowObject)                   \
  V(CreateAllocationSite)                     \
  V(CallFunction)                             \
  V(CallFunctionWithFeedback)                 \
  V(CallConstruct)                            \
  V(RegExpConstructResult)                    \
  V(TransitionElementsKind)                   \
  V(ArrayConstructorConstantArgCount)         \
  V(ArrayConstructor)                         \
  V(InternalArrayConstructorConstantArgCount) \
  V(InternalArrayConstructor)                 \
  V(CompareNil)                               \
  V(ToBoolean)                                \
  V(BinaryOp)                                 \
  V(BinaryOpWithAllocationSite)               \
  V(StringAdd)                                \
  V(Keyed)                                    \
  V(Named)                                    \
  V(CallHandler)                              \
  V(ArgumentAdaptor)                          \
  V(ApiGetter)                                \
  V(ApiFunction)                              \
  V(ArgumentsAccessRead)                      \
  V(StoreArrayLiteralElement)                 \
  V(MathPowTagged)                            \
  V(MathPowInteger)                           \
  V(ContextOnly)


class CallInterfaceDescriptorData {
 public:
  CallInterfaceDescriptorData() : register_param_count_(-1) {}

  // A copy of the passed in registers and param_representations is made
  // and owned by the CallInterfaceDescriptorData.

  // TODO(mvstanton): Instead of taking parallel arrays register and
  // param_representations, how about a struct that puts the representation
  // and register side by side (eg, RegRep(r1, Representation::Tagged()).
  // The same should go for the CodeStubDescriptor class.
  void Initialize(int register_parameter_count, Register* registers,
                  Representation* param_representations,
                  PlatformInterfaceDescriptor* platform_descriptor = NULL);

  bool IsInitialized() const { return register_param_count_ >= 0; }

  int register_param_count() const { return register_param_count_; }
  Register register_param(int index) const { return register_params_[index]; }
  Register* register_params() const { return register_params_.get(); }
  Representation register_param_representation(int index) const {
    return register_param_representations_[index];
  }
  Representation* register_param_representations() const {
    return register_param_representations_.get();
  }
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return platform_specific_descriptor_;
  }

 private:
  int register_param_count_;

  // The Register params are allocated dynamically by the
  // InterfaceDescriptor, and freed on destruction. This is because static
  // arrays of Registers cause creation of runtime static initializers
  // which we don't want.
  SmartArrayPointer<Register> register_params_;
  // Specifies Representations for the stub's parameter. Points to an array of
  // Representations of the same length of the numbers of parameters to the
  // stub, or if NULL (the default value), Representation of each parameter
  // assumed to be Tagged().
  SmartArrayPointer<Representation> register_param_representations_;

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

  CallInterfaceDescriptor(Isolate* isolate, CallDescriptors::Key key)
      : data_(isolate->call_descriptor_data(key)) {}

  int GetEnvironmentLength() const { return data()->register_param_count(); }

  int GetRegisterParameterCount() const {
    return data()->register_param_count();
  }

  Register GetParameterRegister(int index) const {
    return data()->register_param(index);
  }

  Representation GetParameterRepresentation(int index) const {
    DCHECK(index < data()->register_param_count());
    if (data()->register_param_representations() == NULL) {
      return Representation::Tagged();
    }

    return data()->register_param_representation(index);
  }

  // "Environment" versions of parameter functions. The first register
  // parameter (context) is not included.
  int GetEnvironmentParameterCount() const {
    return GetEnvironmentLength() - 1;
  }

  Register GetEnvironmentParameterRegister(int index) const {
    return GetParameterRegister(index + 1);
  }

  Representation GetEnvironmentParameterRepresentation(int index) const {
    return GetParameterRepresentation(index + 1);
  }

  // Some platforms have extra information to associate with the descriptor.
  PlatformInterfaceDescriptor* platform_specific_descriptor() const {
    return data()->platform_specific_descriptor();
  }

  static const Register ContextRegister();

  const char* DebugName(Isolate* isolate);

 protected:
  const CallInterfaceDescriptorData* data() const { return data_; }

 private:
  const CallInterfaceDescriptorData* data_;
};


#define DECLARE_DESCRIPTOR(name, base)                                     \
  explicit name(Isolate* isolate) : base(isolate, key()) {                 \
    if (!data()->IsInitialized())                                          \
      Initialize(isolate->call_descriptor_data(key()));                    \
  }                                                                        \
                                                                           \
 protected:                                                                \
  void Initialize(CallInterfaceDescriptorData* data);                      \
  name(Isolate* isolate, CallDescriptors::Key key) : base(isolate, key) {} \
                                                                           \
 public:                                                                   \
  static inline CallDescriptors::Key key();


// LoadDescriptor is used by all stubs that implement Load/KeyedLoad ICs.
class LoadDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(LoadDescriptor, CallInterfaceDescriptor)

  enum ParameterIndices { kReceiverIndex, kNameIndex };
  static const Register ReceiverRegister();
  static const Register NameRegister();
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


class VectorLoadICTrampolineDescriptor : public LoadDescriptor {
 public:
  DECLARE_DESCRIPTOR(VectorLoadICTrampolineDescriptor, LoadDescriptor)

  enum ParameterIndices { kReceiverIndex, kNameIndex, kSlotIndex };

  static const Register SlotRegister();
};


class VectorLoadICDescriptor : public VectorLoadICTrampolineDescriptor {
 public:
  DECLARE_DESCRIPTOR(VectorLoadICDescriptor, VectorLoadICTrampolineDescriptor)

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


class FastCloneShallowArrayDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastCloneShallowArrayDescriptor, CallInterfaceDescriptor)
};


class FastCloneShallowObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(FastCloneShallowObjectDescriptor, CallInterfaceDescriptor)
};


class CreateAllocationSiteDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CreateAllocationSiteDescriptor, CallInterfaceDescriptor)
};


class CallFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallFunctionDescriptor, CallInterfaceDescriptor)
};


class CallFunctionWithFeedbackDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(CallFunctionWithFeedbackDescriptor,
                     CallInterfaceDescriptor)
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


class ArrayConstructorConstantArgCountDescriptor
    : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ArrayConstructorConstantArgCountDescriptor,
                     CallInterfaceDescriptor)
};


class ArrayConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ArrayConstructorDescriptor, CallInterfaceDescriptor)
};


class InternalArrayConstructorConstantArgCountDescriptor
    : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InternalArrayConstructorConstantArgCountDescriptor,
                     CallInterfaceDescriptor)
};


class InternalArrayConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(InternalArrayConstructorDescriptor,
                     CallInterfaceDescriptor)
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
  DECLARE_DESCRIPTOR(ArgumentAdaptorDescriptor, CallInterfaceDescriptor)
};


class ApiFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ApiFunctionDescriptor, CallInterfaceDescriptor)
};


class ApiGetterDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ApiGetterDescriptor, CallInterfaceDescriptor)

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


class ContextOnlyDescriptor : public CallInterfaceDescriptor {
 public:
  DECLARE_DESCRIPTOR(ContextOnlyDescriptor, CallInterfaceDescriptor)
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

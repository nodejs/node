// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

namespace {
// Constructors for common combined semantic and representation types.
Type* SmiType(Zone* zone) {
  return Type::Intersect(Type::SignedSmall(), Type::TaggedSigned(), zone);
}


Type* UntaggedSigned32(Zone* zone) {
  return Type::Intersect(Type::Signed32(), Type::UntaggedSigned32(), zone);
}


Type* AnyTagged(Zone* zone) {
  return Type::Intersect(
      Type::Any(),
      Type::Union(Type::TaggedPointer(), Type::TaggedSigned(), zone), zone);
}


Type* ExternalPointer(Zone* zone) {
  return Type::Intersect(Type::Internal(), Type::UntaggedPointer(), zone);
}
}


Type::FunctionType* CallInterfaceDescriptor::BuildDefaultFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(zone), Type::Undefined(), parameter_count, zone);
  while (parameter_count-- != 0) {
    function->InitParameter(parameter_count, AnyTagged(zone));
  }
  return function;
}


void CallInterfaceDescriptorData::InitializePlatformSpecific(
    int register_parameter_count, Register* registers,
    PlatformInterfaceDescriptor* platform_descriptor) {
  platform_specific_descriptor_ = platform_descriptor;
  register_param_count_ = register_parameter_count;

  // InterfaceDescriptor owns a copy of the registers array.
  register_params_.Reset(NewArray<Register>(register_parameter_count));
  for (int i = 0; i < register_parameter_count; i++) {
    register_params_[i] = registers[i];
  }
}

const char* CallInterfaceDescriptor::DebugName(Isolate* isolate) const {
  CallInterfaceDescriptorData* start = isolate->call_descriptor_data(0);
  size_t index = data_ - start;
  DCHECK(index < CallDescriptors::NUMBER_OF_DESCRIPTORS);
  CallDescriptors::Key key = static_cast<CallDescriptors::Key>(index);
  switch (key) {
#define DEF_CASE(NAME)        \
  case CallDescriptors::NAME: \
    return #NAME " Descriptor";
    INTERFACE_DESCRIPTOR_LIST(DEF_CASE)
#undef DEF_CASE
    case CallDescriptors::NUMBER_OF_DESCRIPTORS:
      break;
  }
  return "";
}


Type::FunctionType* LoadDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 3, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, AnyTagged(zone));
  function->InitParameter(2, SmiType(zone));
  return function;
}

void LoadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StoreDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StoreTransitionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          MapRegister()};

  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
StoreTransitionDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 4, zone);
  function->InitParameter(0, AnyTagged(zone));  // Receiver
  function->InitParameter(1, AnyTagged(zone));  // Name
  function->InitParameter(2, AnyTagged(zone));  // Value
  function->InitParameter(3, AnyTagged(zone));  // Map
  return function;
}


Type::FunctionType*
LoadGlobalViaContextDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 1, zone);
  function->InitParameter(0, UntaggedSigned32(zone));
  return function;
}


void LoadGlobalViaContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
StoreGlobalViaContextDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 2, zone);
  function->InitParameter(0, UntaggedSigned32(zone));
  function->InitParameter(1, AnyTagged(zone));
  return function;
}


void StoreGlobalViaContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {SlotRegister(), ValueRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InstanceOfDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {LeftRegister(), RightRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StringCompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {LeftRegister(), RightRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToStringDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ToObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void MathPowTaggedDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {exponent()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void MathPowIntegerDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {exponent()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
LoadWithVectorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 4, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, AnyTagged(zone));
  function->InitParameter(2, SmiType(zone));
  function->InitParameter(3, AnyTagged(zone));
  return function;
}


void LoadWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister(),
                          VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
VectorStoreTransitionDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 6, zone);
  function->InitParameter(0, AnyTagged(zone));  // receiver
  function->InitParameter(1, AnyTagged(zone));  // name
  function->InitParameter(2, AnyTagged(zone));  // value
  function->InitParameter(3, SmiType(zone));    // slot
  function->InitParameter(4, AnyTagged(zone));  // vector
  function->InitParameter(5, AnyTagged(zone));  // map
  return function;
}


Type::FunctionType*
VectorStoreICDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 5, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, AnyTagged(zone));
  function->InitParameter(2, AnyTagged(zone));
  function->InitParameter(3, SmiType(zone));
  function->InitParameter(4, AnyTagged(zone));
  return function;
}


void VectorStoreICDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister(), VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
VectorStoreICTrampolineDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 4, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, AnyTagged(zone));
  function->InitParameter(2, AnyTagged(zone));
  function->InitParameter(3, SmiType(zone));
  return function;
}


void VectorStoreICTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
ApiGetterDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 1, zone);
  function->InitParameter(0, ExternalPointer(zone));
  return function;
}


void ApiGetterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {function_address()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ArgumentsAccessReadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {index(), parameter_count()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
ArgumentsAccessNewDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 3, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, SmiType(zone));
  function->InitParameter(2, ExternalPointer(zone));
  return function;
}


void ArgumentsAccessNewDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {function(), parameter_count(), parameter_pointer()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void ContextOnlyDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}


void GrowArrayElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ObjectRegister(), KeyRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
FastCloneShallowArrayDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 3, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, SmiType(zone));
  function->InitParameter(2, AnyTagged(zone));
  return function;
}


Type::FunctionType*
CreateAllocationSiteDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 2, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, SmiType(zone));
  return function;
}


Type::FunctionType*
CreateWeakCellDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 3, zone);
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, SmiType(zone));
  function->InitParameter(2, AnyTagged(zone));
  return function;
}


Type::FunctionType*
CallTrampolineDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 2, zone);
  function->InitParameter(0, AnyTagged(zone));  // target
  function->InitParameter(
      1, UntaggedSigned32(zone));  // actual number of arguments
  return function;
}


Type::FunctionType*
CallFunctionWithFeedbackDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 2, zone);
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, SmiType(zone));
  return function;
}


Type::FunctionType* CallFunctionWithFeedbackAndVectorDescriptor::
    BuildCallInterfaceDescriptorFunctionType(Isolate* isolate,
                                             int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 3, zone);
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, SmiType(zone));
  function->InitParameter(2, AnyTagged(zone));
  return function;
}


Type::FunctionType*
ArrayConstructorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 3, zone);
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, AnyTagged(zone));
  function->InitParameter(2, UntaggedSigned32(zone));
  return function;
}


Type::FunctionType*
InternalArrayConstructorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 2, zone);
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, UntaggedSigned32(zone));
  return function;
}


Type::FunctionType*
ArgumentAdaptorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 3, zone);
  function->InitParameter(0, Type::Receiver());    // JSFunction
  function->InitParameter(
      1, UntaggedSigned32(zone));  // actual number of arguments
  function->InitParameter(
      2,
      UntaggedSigned32(zone));  // expected number of arguments
  return function;
}


Type::FunctionType*
ApiFunctionDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 5, zone);
  function->InitParameter(0, AnyTagged(zone));        // callee
  function->InitParameter(1, AnyTagged(zone));        // call_data
  function->InitParameter(2, AnyTagged(zone));        // holder
  function->InitParameter(3, ExternalPointer(zone));  // api_function_address
  function->InitParameter(
      4, UntaggedSigned32(zone));  // actual number of arguments
  return function;
}


Type::FunctionType*
ApiAccessorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 4, zone);
  function->InitParameter(0, AnyTagged(zone));        // callee
  function->InitParameter(1, AnyTagged(zone));        // call_data
  function->InitParameter(2, AnyTagged(zone));        // holder
  function->InitParameter(3, ExternalPointer(zone));  // api_function_address
  return function;
}


Type::FunctionType* MathRoundVariantCallFromUnoptimizedCodeDescriptor::
    BuildCallInterfaceDescriptorFunctionType(Isolate* isolate,
                                             int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 4, zone);
  function->InitParameter(0, Type::Receiver());
  function->InitParameter(1, SmiType(zone));
  function->InitParameter(2, AnyTagged(zone));
  function->InitParameter(3, AnyTagged(zone));
  return function;
}


Type::FunctionType* MathRoundVariantCallFromOptimizedCodeDescriptor::
    BuildCallInterfaceDescriptorFunctionType(Isolate* isolate,
                                             int paramater_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(zone), Type::Undefined(), 5, zone);
  function->InitParameter(0, Type::Receiver());
  function->InitParameter(1, SmiType(zone));
  function->InitParameter(2, AnyTagged(zone));
  function->InitParameter(3, AnyTagged(zone));
  function->InitParameter(4, AnyTagged(zone));
  return function;
}
}  // namespace internal
}  // namespace v8

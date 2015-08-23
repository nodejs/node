// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

namespace {
// Constructors for common combined semantic and representation types.
Type* SmiType() {
  return Type::Intersect(Type::SignedSmall(), Type::TaggedSigned());
}


Type* UntaggedSigned32() {
  return Type::Intersect(Type::Signed32(), Type::UntaggedSigned32());
}


Type* AnyTagged() {
  return Type::Intersect(
      Type::Any(), Type::Union(Type::TaggedPointer(), Type::TaggedSigned()));
}


Type* ExternalPointer() {
  return Type::Intersect(Type::Internal(), Type::UntaggedPointer());
}
}


Type::FunctionType* CallInterfaceDescriptor::BuildDefaultFunctionType(
    Isolate* isolate, int parameter_count) {
  Type::FunctionType* function =
      Type::FunctionType::New(AnyTagged(), Type::Undefined(), parameter_count,
                              isolate->interface_descriptor_zone());
  while (parameter_count-- != 0) {
    function->InitParameter(parameter_count, AnyTagged());
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
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 3, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());
  function->InitParameter(1, AnyTagged());
  function->InitParameter(2, SmiType());
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


void ElementTransitionAndStoreDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ValueRegister(), MapRegister(), NameRegister(),
                          ReceiverRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void InstanceofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {left(), right()};
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
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 4, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());
  function->InitParameter(1, AnyTagged());
  function->InitParameter(2, SmiType());
  function->InitParameter(3, AnyTagged());
  return function;
}


void LoadWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister(),
                          VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


Type::FunctionType*
VectorStoreICDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 5, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());
  function->InitParameter(1, AnyTagged());
  function->InitParameter(2, AnyTagged());
  function->InitParameter(3, SmiType());
  function->InitParameter(4, AnyTagged());
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
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 4, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());
  function->InitParameter(1, AnyTagged());
  function->InitParameter(2, AnyTagged());
  function->InitParameter(3, SmiType());
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
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 1, isolate->interface_descriptor_zone());
  function->InitParameter(0, ExternalPointer());
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
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 3, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());
  function->InitParameter(1, SmiType());
  function->InitParameter(2, AnyTagged());
  return function;
}


Type::FunctionType*
CreateAllocationSiteDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 2, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());
  function->InitParameter(1, SmiType());
  return function;
}


Type::FunctionType*
CreateWeakCellDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 3, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());
  function->InitParameter(1, SmiType());
  function->InitParameter(2, AnyTagged());
  return function;
}


Type::FunctionType*
CallFunctionWithFeedbackDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 2, isolate->interface_descriptor_zone());
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, SmiType());
  return function;
}


Type::FunctionType* CallFunctionWithFeedbackAndVectorDescriptor::
    BuildCallInterfaceDescriptorFunctionType(Isolate* isolate,
                                             int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 3, isolate->interface_descriptor_zone());
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, SmiType());
  function->InitParameter(2, AnyTagged());
  return function;
}


Type::FunctionType*
ArrayConstructorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 3, isolate->interface_descriptor_zone());
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, AnyTagged());
  function->InitParameter(2, UntaggedSigned32());
  return function;
}


Type::FunctionType*
InternalArrayConstructorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 2, isolate->interface_descriptor_zone());
  function->InitParameter(0, Type::Receiver());  // JSFunction
  function->InitParameter(1, UntaggedSigned32());
  return function;
}


Type::FunctionType*
ArgumentAdaptorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 3, isolate->interface_descriptor_zone());
  function->InitParameter(0, Type::Receiver());    // JSFunction
  function->InitParameter(1, UntaggedSigned32());  // actual number of arguments
  function->InitParameter(2,
                          UntaggedSigned32());  // expected number of arguments
  return function;
}


Type::FunctionType*
ApiFunctionDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 5, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());         // callee
  function->InitParameter(1, AnyTagged());         // call_data
  function->InitParameter(2, AnyTagged());         // holder
  function->InitParameter(3, ExternalPointer());   // api_function_address
  function->InitParameter(4, UntaggedSigned32());  // actual number of arguments
  return function;
}


Type::FunctionType*
ApiAccessorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 4, isolate->interface_descriptor_zone());
  function->InitParameter(0, AnyTagged());        // callee
  function->InitParameter(1, AnyTagged());        // call_data
  function->InitParameter(2, AnyTagged());        // holder
  function->InitParameter(3, ExternalPointer());  // api_function_address
  return function;
}


Type::FunctionType*
MathRoundVariantDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int paramater_count) {
  Type::FunctionType* function = Type::FunctionType::New(
      AnyTagged(), Type::Undefined(), 2, isolate->interface_descriptor_zone());
  function->InitParameter(0, SmiType());
  function->InitParameter(1, AnyTagged());
  return function;
}


}  // namespace internal
}  // namespace v8

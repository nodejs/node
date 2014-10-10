// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

void CallInterfaceDescriptorData::Initialize(
    int register_parameter_count, Register* registers,
    Representation* register_param_representations,
    PlatformInterfaceDescriptor* platform_descriptor) {
  platform_specific_descriptor_ = platform_descriptor;
  register_param_count_ = register_parameter_count;

  // An interface descriptor must have a context register.
  DCHECK(register_parameter_count > 0 &&
         registers[0].is(CallInterfaceDescriptor::ContextRegister()));

  // InterfaceDescriptor owns a copy of the registers array.
  register_params_.Reset(NewArray<Register>(register_parameter_count));
  for (int i = 0; i < register_parameter_count; i++) {
    register_params_[i] = registers[i];
  }

  // If a representations array is specified, then the descriptor owns that as
  // well.
  if (register_param_representations != NULL) {
    register_param_representations_.Reset(
        NewArray<Representation>(register_parameter_count));
    for (int i = 0; i < register_parameter_count; i++) {
      // If there is a context register, the representation must be tagged.
      DCHECK(
          i != 0 ||
          register_param_representations[i].Equals(Representation::Tagged()));
      register_param_representations_[i] = register_param_representations[i];
    }
  }
}


const char* CallInterfaceDescriptor::DebugName(Isolate* isolate) {
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


void LoadDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), ReceiverRegister(),
                          NameRegister()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void StoreDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), ReceiverRegister(), NameRegister(),
                          ValueRegister()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ElementTransitionAndStoreDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), ValueRegister(), MapRegister(),
                          NameRegister(), ReceiverRegister()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void InstanceofDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), left(), right()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void MathPowTaggedDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), exponent()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void MathPowIntegerDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), exponent()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void VectorLoadICTrampolineDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), ReceiverRegister(), NameRegister(),
                          SlotRegister()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void VectorLoadICDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), ReceiverRegister(), NameRegister(),
                          SlotRegister(), VectorRegister()};
  Representation representations[] = {
      Representation::Tagged(), Representation::Tagged(),
      Representation::Tagged(), Representation::Smi(),
      Representation::Tagged()};
  data->Initialize(arraysize(registers), registers, representations);
}


void ApiGetterDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), function_address()};
  Representation representations[] = {Representation::Tagged(),
                                      Representation::External()};
  data->Initialize(arraysize(registers), registers, representations);
}


void ArgumentsAccessReadDescriptor::Initialize(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister(), index(), parameter_count()};
  data->Initialize(arraysize(registers), registers, NULL);
}


void ContextOnlyDescriptor::Initialize(CallInterfaceDescriptorData* data) {
  Register registers[] = {ContextRegister()};
  data->Initialize(arraysize(registers), registers, NULL);
}

}
}  // namespace v8::internal

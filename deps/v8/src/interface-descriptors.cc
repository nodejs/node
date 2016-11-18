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


Type* UntaggedIntegral32(Zone* zone) {
  return Type::Intersect(Type::Signed32(), Type::UntaggedIntegral32(), zone);
}


Type* AnyTagged(Zone* zone) {
  return Type::Intersect(
      Type::Any(),
      Type::Union(Type::TaggedPointer(), Type::TaggedSigned(), zone), zone);
}


Type* ExternalPointer(Zone* zone) {
  return Type::Intersect(Type::Internal(), Type::UntaggedPointer(), zone);
}
}  // namespace

FunctionType* CallInterfaceDescriptor::BuildDefaultFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), parameter_count, zone)
          ->AsFunction();
  while (parameter_count-- != 0) {
    function->InitParameter(parameter_count, AnyTagged(zone));
  }
  return function;
}

void CallInterfaceDescriptorData::InitializePlatformSpecific(
    int register_parameter_count, const Register* registers,
    PlatformInterfaceDescriptor* platform_descriptor) {
  platform_specific_descriptor_ = platform_descriptor;
  register_param_count_ = register_parameter_count;

  // InterfaceDescriptor owns a copy of the registers array.
  register_params_.reset(NewArray<Register>(register_parameter_count));
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


void VoidDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

FunctionType*
FastNewFunctionContextDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), 2, zone)->AsFunction();
  function->InitParameter(0, AnyTagged(zone));
  function->InitParameter(1, UntaggedIntegral32(zone));
  return function;
}

void FastNewFunctionContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {FunctionRegister(), SlotsRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

FunctionType* LoadDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kReceiver, AnyTagged(zone));
  function->InitParameter(kName, AnyTagged(zone));
  function->InitParameter(kSlot, SmiType(zone));
  return function;
}


void LoadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

FunctionType* LoadGlobalDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kSlot, SmiType(zone));
  return function;
}

void LoadGlobalDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {LoadWithVectorDescriptor::SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

FunctionType*
LoadGlobalWithVectorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kSlot, SmiType(zone));
  function->InitParameter(kVector, AnyTagged(zone));
  return function;
}

void LoadGlobalWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {LoadWithVectorDescriptor::SlotRegister(),
                          LoadWithVectorDescriptor::VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

FunctionType* StoreDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kReceiver, AnyTagged(zone));
  function->InitParameter(kName, AnyTagged(zone));
  function->InitParameter(kValue, AnyTagged(zone));
  function->InitParameter(kSlot, SmiType(zone));
  return function;
}

void StoreDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StoreTransitionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          MapRegister()};

  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void VectorStoreTransitionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  if (SlotRegister().is(no_reg)) {
    Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                            MapRegister(), VectorRegister()};
    data->InitializePlatformSpecific(arraysize(registers), registers);
  } else {
    Register registers[] = {ReceiverRegister(), NameRegister(),
                            ValueRegister(),    MapRegister(),
                            SlotRegister(),     VectorRegister()};
    data->InitializePlatformSpecific(arraysize(registers), registers);
  }
}

FunctionType*
StoreTransitionDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kReceiver, AnyTagged(zone));
  function->InitParameter(kName, AnyTagged(zone));
  function->InitParameter(kValue, AnyTagged(zone));
  function->InitParameter(kMap, AnyTagged(zone));
  return function;
}

FunctionType*
StoreGlobalViaContextDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kSlot, UntaggedIntegral32(zone));
  function->InitParameter(kValue, AnyTagged(zone));
  return function;
}


void StoreGlobalViaContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {SlotRegister(), ValueRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


void StringCompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {LeftRegister(), RightRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void TypeConversionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ArgumentRegister()};
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

FunctionType*
LoadWithVectorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kReceiver, AnyTagged(zone));
  function->InitParameter(kName, AnyTagged(zone));
  function->InitParameter(kSlot, SmiType(zone));
  function->InitParameter(kVector, AnyTagged(zone));
  return function;
}


void LoadWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister(),
                          VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

FunctionType*
VectorStoreTransitionDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  bool has_slot = !VectorStoreTransitionDescriptor::SlotRegister().is(no_reg);
  int arg_count = has_slot ? 6 : 5;
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), arg_count, zone)
          ->AsFunction();
  int index = 0;
  // TODO(ishell): use ParameterIndices here
  function->InitParameter(index++, AnyTagged(zone));  // receiver
  function->InitParameter(index++, AnyTagged(zone));  // name
  function->InitParameter(index++, AnyTagged(zone));  // value
  function->InitParameter(index++, AnyTagged(zone));  // map
  if (has_slot) {
    function->InitParameter(index++, SmiType(zone));  // slot
  }
  function->InitParameter(index++, AnyTagged(zone));  // vector
  return function;
}

FunctionType*
StoreWithVectorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kReceiver, AnyTagged(zone));
  function->InitParameter(kName, AnyTagged(zone));
  function->InitParameter(kValue, AnyTagged(zone));
  function->InitParameter(kSlot, SmiType(zone));
  function->InitParameter(kVector, AnyTagged(zone));
  return function;
}

void StoreWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister(), VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

FunctionType*
BinaryOpWithVectorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  DCHECK_EQ(parameter_count, kParameterCount);
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kLeft, AnyTagged(zone));
  function->InitParameter(kRight, AnyTagged(zone));
  function->InitParameter(kSlot, UntaggedIntegral32(zone));
  function->InitParameter(kVector, AnyTagged(zone));
  return function;
}

const Register ApiGetterDescriptor::ReceiverRegister() {
  return LoadDescriptor::ReceiverRegister();
}

void ApiGetterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), HolderRegister(),
                          CallbackRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ContextOnlyDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

CallInterfaceDescriptor OnStackArgsDescriptorBase::ForArgs(
    Isolate* isolate, int parameter_count) {
  switch (parameter_count) {
    case 1:
      return OnStackWith1ArgsDescriptor(isolate);
    case 2:
      return OnStackWith2ArgsDescriptor(isolate);
    case 3:
      return OnStackWith3ArgsDescriptor(isolate);
    case 4:
      return OnStackWith4ArgsDescriptor(isolate);
    case 5:
      return OnStackWith5ArgsDescriptor(isolate);
    case 6:
      return OnStackWith6ArgsDescriptor(isolate);
    case 7:
      return OnStackWith7ArgsDescriptor(isolate);
    default:
      UNREACHABLE();
      return VoidDescriptor(isolate);
  }
}

FunctionType*
OnStackArgsDescriptorBase::BuildCallInterfaceDescriptorFunctionTypeWithArg(
    Isolate* isolate, int register_parameter_count, int parameter_count) {
  DCHECK_EQ(0, register_parameter_count);
  DCHECK_GT(parameter_count, 0);
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), AnyTagged(zone), parameter_count, zone)
          ->AsFunction();
  for (int i = 0; i < parameter_count; i++) {
    function->InitParameter(i, AnyTagged(zone));
  }
  return function;
}

void OnStackArgsDescriptorBase::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void GrowArrayElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ObjectRegister(), KeyRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

FunctionType*
VarArgFunctionDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), AnyTagged(zone), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  return function;
}

FunctionType*
FastCloneRegExpDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kClosure, AnyTagged(zone));
  function->InitParameter(kLiteralIndex, SmiType(zone));
  function->InitParameter(kPattern, AnyTagged(zone));
  function->InitParameter(kFlags, AnyTagged(zone));
  return function;
}

FunctionType*
FastCloneShallowArrayDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kClosure, AnyTagged(zone));
  function->InitParameter(kLiteralIndex, SmiType(zone));
  function->InitParameter(kConstantElements, AnyTagged(zone));
  return function;
}

FunctionType*
CreateAllocationSiteDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kVector, AnyTagged(zone));
  function->InitParameter(kSlot, SmiType(zone));
  return function;
}

FunctionType*
CreateWeakCellDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kVector, AnyTagged(zone));
  function->InitParameter(kSlot, SmiType(zone));
  function->InitParameter(kValue, AnyTagged(zone));
  return function;
}

FunctionType*
CallTrampolineDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, AnyTagged(zone));
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  return function;
}

FunctionType* ConstructStubDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, AnyTagged(zone));
  function->InitParameter(kNewTarget, AnyTagged(zone));
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  function->InitParameter(kAllocationSite, AnyTagged(zone));
  return function;
}

FunctionType*
ConstructTrampolineDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, AnyTagged(zone));
  function->InitParameter(kNewTarget, AnyTagged(zone));
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  return function;
}

FunctionType*
CallFunctionWithFeedbackDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, Type::Receiver());
  function->InitParameter(kSlot, SmiType(zone));
  return function;
}

FunctionType* CallFunctionWithFeedbackAndVectorDescriptor::
    BuildCallInterfaceDescriptorFunctionType(Isolate* isolate,
                                             int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, Type::Receiver());
  function->InitParameter(kSlot, SmiType(zone));
  function->InitParameter(kVector, AnyTagged(zone));
  return function;
}

FunctionType*
ArrayNoArgumentConstructorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, Type::Receiver());
  function->InitParameter(kAllocationSite, AnyTagged(zone));
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  function->InitParameter(kFunctionParameter, AnyTagged(zone));
  return function;
}

FunctionType* ArraySingleArgumentConstructorDescriptor::
    BuildCallInterfaceDescriptorFunctionType(Isolate* isolate,
                                             int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, Type::Receiver());
  function->InitParameter(kAllocationSite, AnyTagged(zone));
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  function->InitParameter(kFunctionParameter, AnyTagged(zone));
  function->InitParameter(kArraySizeSmiParameter, AnyTagged(zone));
  return function;
}

FunctionType*
ArrayNArgumentsConstructorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, Type::Receiver());
  function->InitParameter(kAllocationSite, AnyTagged(zone));
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  return function;
}

FunctionType*
ArgumentAdaptorDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kFunction, Type::Receiver());
  function->InitParameter(kNewTarget, AnyTagged(zone));
  function->InitParameter(kActualArgumentsCount, UntaggedIntegral32(zone));
  function->InitParameter(kExpectedArgumentsCount, UntaggedIntegral32(zone));
  return function;
}

CallInterfaceDescriptor ApiCallbackDescriptorBase::ForArgs(Isolate* isolate,
                                                           int argc) {
  switch (argc) {
    case 0:
      return ApiCallbackWith0ArgsDescriptor(isolate);
    case 1:
      return ApiCallbackWith1ArgsDescriptor(isolate);
    case 2:
      return ApiCallbackWith2ArgsDescriptor(isolate);
    case 3:
      return ApiCallbackWith3ArgsDescriptor(isolate);
    case 4:
      return ApiCallbackWith4ArgsDescriptor(isolate);
    case 5:
      return ApiCallbackWith5ArgsDescriptor(isolate);
    case 6:
      return ApiCallbackWith6ArgsDescriptor(isolate);
    case 7:
      return ApiCallbackWith7ArgsDescriptor(isolate);
    default:
      UNREACHABLE();
      return VoidDescriptor(isolate);
  }
}

FunctionType*
ApiCallbackDescriptorBase::BuildCallInterfaceDescriptorFunctionTypeWithArg(
    Isolate* isolate, int parameter_count, int argc) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function = Type::Function(AnyTagged(zone), Type::Undefined(),
                                          kParameterCount + argc, zone)
                               ->AsFunction();
  function->InitParameter(kFunction, AnyTagged(zone));
  function->InitParameter(kCallData, AnyTagged(zone));
  function->InitParameter(kHolder, AnyTagged(zone));
  function->InitParameter(kApiFunctionAddress, ExternalPointer(zone));
  for (int i = 0; i < argc; i++) {
    function->InitParameter(i, AnyTagged(zone));
  }
  return function;
}

FunctionType*
InterpreterDispatchDescriptor::BuildCallInterfaceDescriptorFunctionType(
    Isolate* isolate, int parameter_count) {
  Zone* zone = isolate->interface_descriptor_zone();
  FunctionType* function =
      Type::Function(AnyTagged(zone), Type::Undefined(), kParameterCount, zone)
          ->AsFunction();
  function->InitParameter(kAccumulator, AnyTagged(zone));
  function->InitParameter(kBytecodeOffset, UntaggedIntegral32(zone));
  function->InitParameter(kBytecodeArray, AnyTagged(zone));
  function->InitParameter(kDispatchTable, AnyTagged(zone));
  return function;
}

}  // namespace internal
}  // namespace v8

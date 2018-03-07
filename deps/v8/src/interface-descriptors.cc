// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {


void CallInterfaceDescriptorData::InitializePlatformSpecific(
    int register_parameter_count, const Register* registers,
    PlatformInterfaceDescriptor* platform_descriptor) {
  platform_specific_descriptor_ = platform_descriptor;
  register_param_count_ = register_parameter_count;

  // InterfaceDescriptor owns a copy of the registers array.
  register_params_.reset(NewArray<Register>(register_parameter_count, no_reg));
  for (int i = 0; i < register_parameter_count; i++) {
    register_params_[i] = registers[i];
  }
}

void CallInterfaceDescriptorData::InitializePlatformIndependent(
    int parameter_count, int extra_parameter_count,
    const MachineType* machine_types) {
  // InterfaceDescriptor owns a copy of the MachineType array.
  // We only care about parameters, not receiver and result.
  param_count_ = parameter_count + extra_parameter_count;
  machine_types_.reset(NewArray<MachineType>(param_count_));
  for (int i = 0; i < param_count_; i++) {
    if (machine_types == nullptr || i >= parameter_count) {
      machine_types_[i] = MachineType::AnyTagged();
    } else {
      machine_types_[i] = machine_types[i];
    }
  }
}

const char* CallInterfaceDescriptor::DebugName(Isolate* isolate) const {
  CallInterfaceDescriptorData* start = isolate->call_descriptor_data(0);
  size_t index = data_ - start;
  DCHECK(index < CallDescriptors::NUMBER_OF_DESCRIPTORS);
  CallDescriptors::Key key = static_cast<CallDescriptors::Key>(index);
  switch (key) {
#define DEF_CASE(NAME, ...)   \
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

void FastNewFunctionContextDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void FastNewFunctionContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {FunctionRegister(), SlotsRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FastNewObjectDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {TargetRegister(), NewTargetRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

const Register FastNewObjectDescriptor::TargetRegister() {
  return kJSFunctionRegister;
}

const Register FastNewObjectDescriptor::NewTargetRegister() {
  return kJavaScriptCallNewTargetRegister;
}

void FastNewArgumentsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {TargetRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

const Register FastNewArgumentsDescriptor::TargetRegister() {
  return kJSFunctionRegister;
}

void RecordWriteDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  MachineType machine_types[] = {MachineType::TaggedPointer(),
                                 MachineType::Pointer(), MachineType::Pointer(),
                                 MachineType::TaggedSigned(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void LoadFieldDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kSmiHandler
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadFieldDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), SmiHandlerRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void LoadGlobalDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadGlobalDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void LoadGlobalWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kSlot, kVector
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadGlobalWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), SlotRegister(), VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StoreGlobalDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kValue, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreGlobalDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), ValueRegister(), SlotRegister()};

  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreGlobalWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kName, kValue, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreGlobalWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), ValueRegister(), SlotRegister(),
                          VectorRegister()};
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kValue, kSlot
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister()};

  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreTransitionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      ReceiverRegister(), NameRegister(), MapRegister(),
      ValueRegister(),    SlotRegister(), VectorRegister(),
  };
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreTransitionDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kMap, kValue, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(),    MachineType::AnyTagged(),
      MachineType::AnyTagged(),    MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreNamedTransitionDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kFieldOffset, kMap, kValue, kSlot, kVector, kName
  MachineType machine_types[] = {
      MachineType::AnyTagged(),    MachineType::TaggedSigned(),
      MachineType::AnyTagged(),    MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged(),
      MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreNamedTransitionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      ReceiverRegister(), FieldOffsetRegister(), MapRegister(),
      ValueRegister(),    SlotRegister(),        VectorRegister(),
      NameRegister(),
  };
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StringAtDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kPosition
  // TODO(turbofan): Allow builtins to return untagged values.
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StringAtDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void TypeConversionDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ArgumentRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void TypeConversionStackParameterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void TypeConversionStackParameterDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformIndependent(data->register_param_count(), 1, nullptr);
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

const Register LoadFieldDescriptor::ReceiverRegister() {
  // Reuse the register from the LoadDescriptor, since given the
  // LoadFieldDescriptor's usage, it doesn't matter exactly which registers are
  // used to pass parameters in.
  return LoadDescriptor::ReceiverRegister();
}
const Register LoadFieldDescriptor::SmiHandlerRegister() {
  // Reuse the register from the LoadDescriptor, since given the
  // LoadFieldDescriptor's usage, it doesn't matter exactly which registers are
  // used to pass parameters in.
  return LoadDescriptor::NameRegister();
}

void LoadWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister(),
                          VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StoreWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kValue, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::TaggedSigned(),
      MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StoreWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister(), VectorRegister()};
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
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

void GrowArrayElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ObjectRegister(), KeyRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void NewArgumentsElementsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFrame, kLength, kMappedCount
  MachineType const kMachineTypes[] = {MachineType::Pointer(),
                                       MachineType::TaggedSigned(),
                                       MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(kMachineTypes), 0,
                                      kMachineTypes);
}

void NewArgumentsElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, 3);
}

void CallTrampolineDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kActualArgumentsCount
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kActualArgumentsCount, kArgumentsList, kArgumentsLength
  MachineType machine_types[] = {MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallForwardVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kActualArgumentsCount, kStartIndex
  MachineType machine_types[] = {MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallWithSpreadDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kArgumentsCount, kArgumentsList
  MachineType machine_types[] = {MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallWithArrayLikeDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kArgumentsList
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kArgumentsList,
  // kArgumentsLength
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(), MachineType::Int32(),
      MachineType::AnyTagged(), MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructForwardVarargsDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kStartIndex
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructWithSpreadDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kArgumentsCount, kSpread
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kArgumentsList
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructStubDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kNewTarget, kActualArgumentsCount, kAllocationSite
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ConstructTrampolineDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kNewTarget, kActualArgumentsCount
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(), MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void BuiltinDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kArgumentsCount
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(), MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void BuiltinDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {TargetRegister(), NewTargetRegister(),
                          ArgumentsCountRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

const Register BuiltinDescriptor::ArgumentsCountRegister() {
  return kJavaScriptCallArgCountRegister;
}
const Register BuiltinDescriptor::NewTargetRegister() {
  return kJavaScriptCallNewTargetRegister;
}

const Register BuiltinDescriptor::TargetRegister() {
  return kJSFunctionRegister;
}

void ArrayConstructorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTarget, kNewTarget, kActualArgumentsCount, kAllocationSite
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kAllocationSite, kActualArgumentsCount, kFunctionParameter
  MachineType machine_types[] = {MachineType::TaggedPointer(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kAllocationSite, kActualArgumentsCount, kFunctionParameter,
  // kArraySizeSmiParameter
  MachineType machine_types[] = {
      MachineType::TaggedPointer(), MachineType::AnyTagged(),
      MachineType::Int32(), MachineType::AnyTagged(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kAllocationSite, kActualArgumentsCount
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(), MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ArgumentAdaptorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kNewTarget, kActualArgumentsCount, kExpectedArgumentsCount
  MachineType machine_types[] = {MachineType::TaggedPointer(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void ApiCallbackDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kTargetContext, kCallData, kHolder, kApiFunctionAddress
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterDispatchDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kAccumulator, kBytecodeOffset, kBytecodeArray, kDispatchTable
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::IntPtr(), MachineType::AnyTagged(),
      MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterPushArgsThenCallDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kFirstArgument, kFunction
  MachineType machine_types[] = {MachineType::Int32(), MachineType::Pointer(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kNewTarget, kConstructor, kFeedbackElement,
  // kFirstArgument
  MachineType machine_types[] = {
      MachineType::Int32(), MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterCEntryDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kFirstArgument, kFunctionEntry
  MachineType machine_types[] = {MachineType::Int32(), MachineType::Pointer(),
                                 MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void FrameDropperTrampolineDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // New FP value.
  MachineType machine_types[] = {MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

}  // namespace internal
}  // namespace v8

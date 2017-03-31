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
  register_params_.reset(NewArray<Register>(register_parameter_count));
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
    if (machine_types == NULL || i >= parameter_count) {
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

void StringCharAtDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kPosition
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StringCharAtDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void StringCharCodeAtDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kPosition
  // TODO(turbofan): Allow builtins to return untagged values.
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void StringCharCodeAtDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
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

void LoadICProtoArrayDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kReceiver, kName, kSlot, kVector, kHandler
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::TaggedSigned(), MachineType::AnyTagged(),
      MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void LoadICProtoArrayDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister(),
                          VectorRegister(), HandlerRegister()};
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

void BinaryOpWithVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kLeft, kRight, kSlot, kVector
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::AnyTagged(), MachineType::Int32(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
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
  MachineType const kMachineTypes[] = {MachineType::IntPtr()};
  data->InitializePlatformIndependent(arraysize(kMachineTypes), 0,
                                      kMachineTypes);
}

void NewArgumentsElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, 1);
}

void VarArgFunctionDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kActualArgumentsCount
  MachineType machine_types[] = {MachineType::Int32()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void FastCloneRegExpDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kClosure, kLiteralIndex, kPattern, kFlags
  MachineType machine_types[] = {
      MachineType::AnyTagged(), MachineType::TaggedSigned(),
      MachineType::AnyTagged(), MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void FastCloneShallowArrayDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kClosure, kLiteralIndex, kConstantElements
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CreateAllocationSiteDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kVector, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CreateWeakCellDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kVector, kSlot, kValue
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallTrampolineDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kActualArgumentsCount
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::Int32()};
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

void CallFunctionWithFeedbackDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kSlot
  MachineType machine_types[] = {MachineType::AnyTagged(),
                                 MachineType::TaggedSigned()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void CallFunctionWithFeedbackAndVectorDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kFunction, kActualArgumentsCount, kSlot, kVector
  MachineType machine_types[] = {
      MachineType::TaggedPointer(), MachineType::Int32(),
      MachineType::TaggedSigned(), MachineType::AnyTagged()};
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
  MachineType machine_types[] = {MachineType::TaggedPointer(),
                                 MachineType::AnyTagged(),
                                 MachineType::Int32()};
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
  // kFunction, kCallData, kHolder, kApiFunctionAddress
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
      MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterPushArgsAndCallDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kFirstArgument, kFunction
  MachineType machine_types[] = {MachineType::Int32(), MachineType::Pointer(),
                                 MachineType::AnyTagged()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterPushArgsAndConstructDescriptor::InitializePlatformIndependent(
    CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kNewTarget, kConstructor, kFeedbackElement,
  // kFirstArgument
  MachineType machine_types[] = {
      MachineType::Int32(), MachineType::AnyTagged(), MachineType::AnyTagged(),
      MachineType::AnyTagged(), MachineType::Pointer()};
  data->InitializePlatformIndependent(arraysize(machine_types), 0,
                                      machine_types);
}

void InterpreterPushArgsAndConstructArrayDescriptor::
    InitializePlatformIndependent(CallInterfaceDescriptorData* data) {
  // kNumberOfArguments, kFunction, kFeedbackElement, kFirstArgument
  MachineType machine_types[] = {MachineType::Int32(), MachineType::AnyTagged(),
                                 MachineType::AnyTagged(),
                                 MachineType::Pointer()};
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

}  // namespace internal
}  // namespace v8

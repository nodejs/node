// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/interface-descriptors.h"

#include "src/codegen/macro-assembler.h"

namespace v8 {
namespace internal {

void CallInterfaceDescriptorData::InitializePlatformSpecific(
    int register_parameter_count, const Register* registers) {
  DCHECK(!IsInitializedPlatformIndependent());

  register_param_count_ = register_parameter_count;

  // UBSan doesn't like creating zero-length arrays.
  if (register_parameter_count == 0) return;

  // InterfaceDescriptor owns a copy of the registers array.
  register_params_ = NewArray<Register>(register_parameter_count, no_reg);
  for (int i = 0; i < register_parameter_count; i++) {
    // The value of the root register must be reserved, thus any uses
    // within the calling convention are disallowed.
    DCHECK_NE(registers[i], kRootRegister);
    register_params_[i] = registers[i];
  }
}

void CallInterfaceDescriptorData::InitializePlatformIndependent(
    Flags flags, int return_count, int parameter_count,
    const MachineType* machine_types, int machine_types_length) {
  DCHECK(IsInitializedPlatformSpecific());

  flags_ = flags;
  return_count_ = return_count;
  param_count_ = parameter_count;
  const int types_length = return_count_ + param_count_;

  // Machine types are either fully initialized or null.
  if (machine_types == nullptr) {
    machine_types_ =
        NewArray<MachineType>(types_length, MachineType::AnyTagged());
  } else {
    DCHECK_EQ(machine_types_length, types_length);
    machine_types_ = NewArray<MachineType>(types_length);
    for (int i = 0; i < types_length; i++) machine_types_[i] = machine_types[i];
  }

  if (!(flags_ & kNoStackScan)) DCHECK(AllStackParametersAreTagged());
}

#ifdef DEBUG
bool CallInterfaceDescriptorData::AllStackParametersAreTagged() const {
  DCHECK(IsInitialized());
  const int types_length = return_count_ + param_count_;
  const int first_stack_param = return_count_ + register_param_count_;
  for (int i = first_stack_param; i < types_length; i++) {
    if (!machine_types_[i].IsTagged()) return false;
  }
  return true;
}
#endif  // DEBUG

void CallInterfaceDescriptorData::Reset() {
  delete[] machine_types_;
  machine_types_ = nullptr;
  delete[] register_params_;
  register_params_ = nullptr;
}

// static
CallInterfaceDescriptorData
    CallDescriptors::call_descriptor_data_[NUMBER_OF_DESCRIPTORS];

void CallDescriptors::InitializeOncePerProcess() {
#define INTERFACE_DESCRIPTOR(name, ...) \
  name##Descriptor().Initialize(&call_descriptor_data_[CallDescriptors::name]);
  INTERFACE_DESCRIPTOR_LIST(INTERFACE_DESCRIPTOR)
#undef INTERFACE_DESCRIPTOR

  DCHECK(ContextOnlyDescriptor{}.HasContextParameter());
  DCHECK(!NoContextDescriptor{}.HasContextParameter());
  DCHECK(!AllocateDescriptor{}.HasContextParameter());
  DCHECK(!AllocateHeapNumberDescriptor{}.HasContextParameter());
  DCHECK(!AbortDescriptor{}.HasContextParameter());
}

void CallDescriptors::TearDown() {
  for (CallInterfaceDescriptorData& data : call_descriptor_data_) {
    data.Reset();
  }
}

void CallInterfaceDescriptor::JSDefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int non_js_register_parameter_count) {
  DCHECK_LE(static_cast<unsigned>(non_js_register_parameter_count), 1);

  // 3 is for kTarget, kNewTarget and kActualArgumentsCount
  int register_parameter_count = 3 + non_js_register_parameter_count;

  DCHECK(!AreAliased(
      kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
      kJavaScriptCallArgCountRegister, kJavaScriptCallExtraArg1Register));

  const Register default_js_stub_registers[] = {
      kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
      kJavaScriptCallArgCountRegister, kJavaScriptCallExtraArg1Register};

  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_js_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_js_stub_registers);
}

const char* CallInterfaceDescriptor::DebugName() const {
  CallDescriptors::Key key = CallDescriptors::GetKey(data_);
  switch (key) {
#define DEF_CASE(name, ...)   \
  case CallDescriptors::name: \
    return #name " Descriptor";
    INTERFACE_DESCRIPTOR_LIST(DEF_CASE)
#undef DEF_CASE
    case CallDescriptors::NUMBER_OF_DESCRIPTORS:
      break;
  }
  return "";
}

#if !defined(V8_TARGET_ARCH_MIPS) && !defined(V8_TARGET_ARCH_MIPS64)
bool CallInterfaceDescriptor::IsValidFloatParameterRegister(Register reg) {
  return true;
}
#endif

void VoidDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void AllocateDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {kAllocateSizeRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CEntry1ArgvOnStackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {kRuntimeCallArgCountRegister,
                          kRuntimeCallFunctionRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

namespace {

void InterpreterCEntryDescriptor_InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {kRuntimeCallArgCountRegister,
                          kRuntimeCallArgvRegister,
                          kRuntimeCallFunctionRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace

void InterpreterCEntry1Descriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  InterpreterCEntryDescriptor_InitializePlatformSpecific(data);
}

void InterpreterCEntry2Descriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  InterpreterCEntryDescriptor_InitializePlatformSpecific(data);
}

void FastNewFunctionContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ScopeInfoRegister(), SlotsRegister()};
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

void LoadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void LoadGlobalDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), SlotRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void LoadGlobalWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), SlotRegister(), VectorRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void StoreGlobalDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), ValueRegister(), SlotRegister()};

  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreGlobalWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {NameRegister(), ValueRegister(), SlotRegister(),
                          VectorRegister()};
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
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

void StringAtDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void StringSubstringDescriptor::InitializePlatformSpecific(
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

void AsyncFunctionStackParameterDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void LoadWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), SlotRegister(),
                          VectorRegister()};
  // TODO(jgruber): This DCHECK could be enabled if RegisterBase::ListOf were
  // to allow no_reg entries.
  // DCHECK(!AreAliased(ReceiverRegister(), NameRegister(), SlotRegister(),
  //                    VectorRegister(), kRootRegister));
  int len = arraysize(registers) - kStackArgumentsCount;
  data->InitializePlatformSpecific(len, registers);
}

void StoreWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ReceiverRegister(), NameRegister(), ValueRegister(),
                          SlotRegister(), VectorRegister()};
  // TODO(jgruber): This DCHECK could be enabled if RegisterBase::ListOf were
  // to allow no_reg entries.
  // DCHECK(!AreAliased(ReceiverRegister(), NameRegister(), kRootRegister));
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

void NoContextDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void GrowArrayElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {ObjectRegister(), KeyRegister()};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void NewArgumentsElementsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void ArrayNoArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // This descriptor must use the same set of registers as the
  // ArrayNArgumentsConstructorDescriptor.
  ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(data);
}

void ArraySingleArgumentConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // This descriptor must use the same set of registers as the
  // ArrayNArgumentsConstructorDescriptor.
  ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(data);
}

void ArrayNArgumentsConstructorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // Keep the arguments on the same registers as they were in
  // ArrayConstructorDescriptor to avoid unnecessary register moves.
  // kFunction, kAllocationSite, kActualArgumentsCount
  Register registers[] = {kJavaScriptCallTargetRegister,
                          kJavaScriptCallExtraArg1Register,
                          kJavaScriptCallArgCountRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void WasmMemoryGrowDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void WasmTableGetDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void WasmTableSetDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void WasmThrowDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void WasmAtomicNotifyDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

#if !defined(V8_TARGET_ARCH_MIPS) && !defined(V8_TARGET_ARCH_MIPS64)
void WasmI32AtomicWaitDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void WasmI64AtomicWaitDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}
#endif

void CloneObjectWithVectorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

// static
Register RunMicrotasksDescriptor::MicrotaskQueueRegister() {
  return CallDescriptors::call_descriptor_data(CallDescriptors::RunMicrotasks)
      ->register_param(0);
}

void RunMicrotasksDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void I64ToBigIntDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

void BigIntToI64Descriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  DefaultInitializePlatformSpecific(data, kParameterCount);
}

}  // namespace internal
}  // namespace v8

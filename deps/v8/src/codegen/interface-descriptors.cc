// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/interface-descriptors.h"

#include "src/codegen/interface-descriptors-inl.h"
#include "src/codegen/macro-assembler.h"

namespace v8 {
namespace internal {

void CallInterfaceDescriptorData::InitializeRegisters(
    Flags flags, int return_count, int parameter_count,
    StackArgumentOrder stack_order, int register_parameter_count,
    const Register* registers) {
  DCHECK(!IsInitializedTypes());

#ifdef DEBUG
  {
    // Make sure that the registers are all valid, and don't alias each other.
    RegList reglist = 0;
    for (int i = 0; i < register_parameter_count; ++i) {
      Register reg = registers[i];
      DCHECK(reg.is_valid());
      DCHECK_EQ(reglist & reg.bit(), 0);
      DCHECK_NE(reg, kRootRegister);
#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
      DCHECK_NE(reg, kPtrComprCageBaseRegister);
#endif
      reglist = CombineRegLists(reglist, reg.bit());
    }
  }
#endif

  flags_ = flags;
  stack_order_ = stack_order;
  return_count_ = return_count;
  param_count_ = parameter_count;
  register_param_count_ = register_parameter_count;

  // The caller owns the the registers array, so we just set the pointer.
  register_params_ = registers;
}

void CallInterfaceDescriptorData::InitializeTypes(
    const MachineType* machine_types, int machine_types_length) {
  DCHECK(IsInitializedRegisters());
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
  DCHECK(!AbortDescriptor{}.HasContextParameter());
  DCHECK(!WasmFloat32ToNumberDescriptor{}.HasContextParameter());
  DCHECK(!WasmFloat64ToNumberDescriptor{}.HasContextParameter());
}

void CallDescriptors::TearDown() {
  for (CallInterfaceDescriptorData& data : call_descriptor_data_) {
    data.Reset();
  }
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

bool CallInterfaceDescriptor::IsValidFloatParameterRegister(Register reg) {
#if defined(V8_TARGET_ARCH_MIPS) || defined(V8_TARGET_ARCH_MIPS64)
  return reg.code() % 2 == 0;
#else
  return true;
#endif
}

#if DEBUG
template <typename DerivedDescriptor>
void StaticCallInterfaceDescriptor<DerivedDescriptor>::Verify(
    CallInterfaceDescriptorData* data) {}
// static
void WriteBarrierDescriptor::Verify(CallInterfaceDescriptorData* data) {
  DCHECK(!AreAliased(ObjectRegister(), SlotAddressRegister(), ValueRegister()));
  // The default parameters should not clobber vital registers in order to
  // reduce code size:
  DCHECK(!AreAliased(ObjectRegister(), kContextRegister,
                     kInterpreterAccumulatorRegister));
  DCHECK(!AreAliased(SlotAddressRegister(), kContextRegister,
                     kInterpreterAccumulatorRegister));
  DCHECK(!AreAliased(ValueRegister(), kContextRegister,
                     kInterpreterAccumulatorRegister));
  DCHECK(!AreAliased(SlotAddressRegister(), kJavaScriptCallNewTargetRegister));
  // Coincidental: to make calling from various builtins easier.
  DCHECK_EQ(ObjectRegister(), kJSFunctionRegister);
  // We need a certain set of registers by default:
  RegList allocatable_regs = data->allocatable_registers();
  DCHECK(allocatable_regs | kContextRegister.bit());
  DCHECK(allocatable_regs | kReturnRegister0.bit());
  VerifyArgumentRegisterCount(data, 4);
}
#endif  // DEBUG

}  // namespace internal
}  // namespace v8

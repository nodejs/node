// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/interface-descriptors.h"

#include "src/frames.h"

namespace v8 {
namespace internal {

const Register CallInterfaceDescriptor::ContextRegister() { return rsi; }

void CallInterfaceDescriptor::DefaultInitializePlatformSpecific(
    CallInterfaceDescriptorData* data, int register_parameter_count) {
  const Register default_stub_registers[] = {rax, rbx, rcx, rdx, rdi};
  CHECK_LE(static_cast<size_t>(register_parameter_count),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(register_parameter_count,
                                   default_stub_registers);
}

void RecordWriteDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {arg_reg_1, arg_reg_2, arg_reg_3,
                                             arg_reg_4, kReturnRegister0};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

void EphemeronKeyBarrierDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  const Register default_stub_registers[] = {arg_reg_1, arg_reg_2, arg_reg_3,
                                             arg_reg_4, kReturnRegister0};

  data->RestrictAllocatableRegisters(default_stub_registers,
                                     arraysize(default_stub_registers));

  CHECK_LE(static_cast<size_t>(kParameterCount),
           arraysize(default_stub_registers));
  data->InitializePlatformSpecific(kParameterCount, default_stub_registers);
}

const Register FastNewFunctionContextDescriptor::ScopeInfoRegister() {
  return rdi;
}
const Register FastNewFunctionContextDescriptor::SlotsRegister() { return rax; }

const Register LoadDescriptor::ReceiverRegister() { return rdx; }
const Register LoadDescriptor::NameRegister() { return rcx; }
const Register LoadDescriptor::SlotRegister() { return rax; }

const Register LoadWithVectorDescriptor::VectorRegister() { return rbx; }

const Register StoreDescriptor::ReceiverRegister() { return rdx; }
const Register StoreDescriptor::NameRegister() { return rcx; }
const Register StoreDescriptor::ValueRegister() { return rax; }
const Register StoreDescriptor::SlotRegister() { return rdi; }

const Register StoreWithVectorDescriptor::VectorRegister() { return rbx; }

const Register StoreTransitionDescriptor::SlotRegister() { return rdi; }
const Register StoreTransitionDescriptor::VectorRegister() { return rbx; }
const Register StoreTransitionDescriptor::MapRegister() { return r11; }

const Register ApiGetterDescriptor::HolderRegister() { return rcx; }
const Register ApiGetterDescriptor::CallbackRegister() { return rbx; }

const Register GrowArrayElementsDescriptor::ObjectRegister() { return rax; }
const Register GrowArrayElementsDescriptor::KeyRegister() { return rbx; }


void TypeofDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}


// static
const Register TypeConversionDescriptor::ArgumentRegister() { return rax; }

void CallTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rdi : the target to call
  Register registers[] = {rdi, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments (on the stack, not including receiver)
  // rdi : the target to call
  // rcx : arguments list length (untagged)
  // rbx : arguments list (FixedArray)
  Register registers[] = {rdi, rax, rcx, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rcx : start index (to support rest parameters)
  // rdi : the target to call
  Register registers[] = {rdi, rax, rcx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallFunctionTemplateDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rdx: the function template info
  // rcx: number of arguments (on the stack, not including receiver)
  Register registers[] = {rdx, rcx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments (on the stack, not including receiver)
  // rdi : the target to call
  // rbx : the object to spread
  Register registers[] = {rdi, rax, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void CallWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rdi : the target to call
  // rbx : the arguments list
  Register registers[] = {rdi, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments (on the stack, not including receiver)
  // rdi : the target to call
  // rdx : the new target
  // rcx : arguments list length (untagged)
  // rbx : arguments list (FixedArray)
  Register registers[] = {rdi, rdx, rax, rcx, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructForwardVarargsDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rdx : the new target
  // rcx : start index (to support rest parameters)
  // rdi : the target to call
  Register registers[] = {rdi, rdx, rax, rcx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithSpreadDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments (on the stack, not including receiver)
  // rdi : the target to call
  // rdx : the new target
  // rbx : the object to spread
  Register registers[] = {rdi, rdx, rax, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructWithArrayLikeDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rdi : the target to call
  // rdx : the new target
  // rbx : the arguments list
  Register registers[] = {rdi, rdx, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ConstructStubDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  // rax : number of arguments
  // rdx : the new target
  // rdi : the target to call
  // rbx : allocation site or undefined
  Register registers[] = {rdi, rdx, rax, rbx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AbortDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdx};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void AllocateHeapNumberDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  data->InitializePlatformSpecific(0, nullptr);
}

void CompareDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void BinaryOpDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {rdx, rax};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ArgumentsAdaptorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rdi,  // JSFunction
      rdx,  // the new target
      rax,  // actual number of arguments
      rbx,  // expected number of arguments
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ApiCallbackDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rdx,  // api function address
      rcx,  // argument count (not including receiver)
      rbx,  // call data
      rdi,  // holder
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterDispatchDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      kInterpreterAccumulatorRegister, kInterpreterBytecodeOffsetRegister,
      kInterpreterBytecodeArrayRegister, kInterpreterDispatchTableRegister};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenCallDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rax,  // argument count (not including receiver)
      rbx,  // address of first argument
      rdi   // the target callable to be call
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void InterpreterPushArgsThenConstructDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rax,  // argument count (not including receiver)
      rcx,  // address of first argument
      rdi,  // constructor to call
      rdx,  // new target
      rbx,  // allocation site feedback if available, undefined otherwise
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void ResumeGeneratorDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rax,  // the value to pass to the generator
      rdx   // the JSGeneratorObject / JSAsyncGeneratorObject to resume
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void FrameDropperTrampolineDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {
      rbx,  // loaded new FP
  };
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

void RunMicrotasksEntryDescriptor::InitializePlatformSpecific(
    CallInterfaceDescriptorData* data) {
  Register registers[] = {arg_reg_1, arg_reg_2};
  data->InitializePlatformSpecific(arraysize(registers), registers);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

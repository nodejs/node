// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_INTERFACE_DESCRIPTORS_INL_H_
#define V8_CODEGEN_INTERFACE_DESCRIPTORS_INL_H_

#include <utility>

#include "src/base/logging.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/register.h"

#if V8_TARGET_ARCH_X64
#include "src/codegen/x64/interface-descriptors-x64-inl.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/codegen/arm64/interface-descriptors-arm64-inl.h"
#elif V8_TARGET_ARCH_IA32
#include "src/codegen/ia32/interface-descriptors-ia32-inl.h"
#elif V8_TARGET_ARCH_ARM
#include "src/codegen/arm/interface-descriptors-arm-inl.h"
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#include "src/codegen/ppc/interface-descriptors-ppc-inl.h"
#elif V8_TARGET_ARCH_S390
#include "src/codegen/s390/interface-descriptors-s390-inl.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/codegen/mips64/interface-descriptors-mips64-inl.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/codegen/mips/interface-descriptors-mips-inl.h"
#elif V8_TARGET_ARCH_LOONG64
#include "src/codegen/loong64/interface-descriptors-loong64-inl.h"
#elif V8_TARGET_ARCH_RISCV64
#include "src/codegen/riscv64/interface-descriptors-riscv64-inl.h"
#else
#error Unsupported target architecture.
#endif

namespace v8 {
namespace internal {

// static
constexpr std::array<Register, kJSBuiltinRegisterParams>
CallInterfaceDescriptor::DefaultJSRegisterArray() {
  return RegisterArray(
      kJavaScriptCallTargetRegister, kJavaScriptCallNewTargetRegister,
      kJavaScriptCallArgCountRegister, kJavaScriptCallExtraArg1Register);
}

// static
template <typename DerivedDescriptor>
constexpr auto StaticCallInterfaceDescriptor<DerivedDescriptor>::registers() {
  return CallInterfaceDescriptor::DefaultRegisterArray();
}

// static
template <typename DerivedDescriptor>
constexpr auto StaticJSCallInterfaceDescriptor<DerivedDescriptor>::registers() {
  return CallInterfaceDescriptor::DefaultJSRegisterArray();
}

template <typename DerivedDescriptor>
void StaticCallInterfaceDescriptor<DerivedDescriptor>::Initialize(
    CallInterfaceDescriptorData* data) {
  // Static local copy of the Registers array, for platform-specific
  // initialization
  static auto registers = DerivedDescriptor::registers();

  // The passed pointer should be a modifiable pointer to our own data.
  DCHECK_EQ(data, this->data());
  DCHECK(!data->IsInitialized());

  if (DerivedDescriptor::kRestrictAllocatableRegisters) {
    data->RestrictAllocatableRegisters(registers.data(), registers.size());
  } else {
    DCHECK(!DerivedDescriptor::kCalleeSaveRegisters);
  }

  data->InitializeRegisters(
      DerivedDescriptor::flags(), DerivedDescriptor::kReturnCount,
      DerivedDescriptor::GetParameterCount(),
      DerivedDescriptor::kStackArgumentOrder,
      DerivedDescriptor::GetRegisterParameterCount(), registers.data());

  // InitializeTypes is customizable by the DerivedDescriptor subclass.
  DerivedDescriptor::InitializeTypes(data);

  DCHECK(data->IsInitialized());
  DCHECK(this->CheckFloatingPointParameters(data));
#if DEBUG
  DerivedDescriptor::Verify(data);
#endif
}
// static
template <typename DerivedDescriptor>
constexpr int
StaticCallInterfaceDescriptor<DerivedDescriptor>::GetReturnCount() {
  static_assert(
      DerivedDescriptor::kReturnCount >= 0,
      "DerivedDescriptor subclass should override return count with a value "
      "that is greater than 0");

  return DerivedDescriptor::kReturnCount;
}

// static
template <typename DerivedDescriptor>
constexpr int
StaticCallInterfaceDescriptor<DerivedDescriptor>::GetParameterCount() {
  static_assert(
      DerivedDescriptor::kParameterCount >= 0,
      "DerivedDescriptor subclass should override parameter count with a "
      "value that is greater than 0");

  return DerivedDescriptor::kParameterCount;
}

namespace detail {

// Helper trait for statically checking if a type is a std::array<Register,N>.
template <typename T>
struct IsRegisterArray : public std::false_type {};
template <size_t N>
struct IsRegisterArray<std::array<Register, N>> : public std::true_type {};
template <>
struct IsRegisterArray<EmptyRegisterArray> : public std::true_type {};

// Helper for finding the index of the first invalid register in a register
// array.
template <size_t N, size_t Index>
struct FirstInvalidRegisterHelper {
  static constexpr int Call(std::array<Register, N> regs) {
    if (!std::get<Index>(regs).is_valid()) {
      // All registers after the first invalid one have to also be invalid (this
      // DCHECK will be checked recursively).
      DCHECK_EQ((FirstInvalidRegisterHelper<N, Index + 1>::Call(regs)),
                Index + 1);
      return Index;
    }
    return FirstInvalidRegisterHelper<N, Index + 1>::Call(regs);
  }
};
template <size_t N>
struct FirstInvalidRegisterHelper<N, N> {
  static constexpr int Call(std::array<Register, N> regs) { return N; }
};
template <size_t N, size_t Index = 0>
constexpr size_t FirstInvalidRegister(std::array<Register, N> regs) {
  return FirstInvalidRegisterHelper<N, 0>::Call(regs);
}
constexpr size_t FirstInvalidRegister(EmptyRegisterArray regs) { return 0; }

}  // namespace detail

// static
template <typename DerivedDescriptor>
constexpr int
StaticCallInterfaceDescriptor<DerivedDescriptor>::GetRegisterParameterCount() {
  static_assert(
      detail::IsRegisterArray<decltype(DerivedDescriptor::registers())>::value,
      "DerivedDescriptor subclass should define a registers() function "
      "returning a std::array<Register>");

  // The register parameter count is the minimum of:
  //   1. The number of named parameters in the descriptor, and
  //   2. The number of valid registers the descriptor provides with its
  //      registers() function, e.g. for {rax, rbx, no_reg} this number is 2.
  //   3. The maximum number of register parameters allowed (
  //      kMaxBuiltinRegisterParams for most builtins,
  //      kMaxTFSBuiltinRegisterParams for TFS builtins, customizable by the
  //      subclass otherwise).
  return std::min<int>({DerivedDescriptor::GetParameterCount(),
                        static_cast<int>(detail::FirstInvalidRegister(
                            DerivedDescriptor::registers())),
                        DerivedDescriptor::kMaxRegisterParams});
}

// static
template <typename DerivedDescriptor>
constexpr int
StaticCallInterfaceDescriptor<DerivedDescriptor>::GetStackParameterCount() {
  return DerivedDescriptor::GetParameterCount() -
         DerivedDescriptor::GetRegisterParameterCount();
}

// static
template <typename DerivedDescriptor>
constexpr Register
StaticCallInterfaceDescriptor<DerivedDescriptor>::GetRegisterParameter(int i) {
  return DerivedDescriptor::registers()[i];
}

// static
constexpr Register FastNewObjectDescriptor::TargetRegister() {
  return kJSFunctionRegister;
}

// static
constexpr Register FastNewObjectDescriptor::NewTargetRegister() {
  return kJavaScriptCallNewTargetRegister;
}

// static
constexpr Register WriteBarrierDescriptor::ObjectRegister() {
  return std::get<kObject>(registers());
}
// static
constexpr Register WriteBarrierDescriptor::SlotAddressRegister() {
  return std::get<kSlotAddress>(registers());
}

// static
constexpr Register WriteBarrierDescriptor::ValueRegister() {
  return std::get<kSlotAddress + 1>(registers());
}

// static
constexpr RegList WriteBarrierDescriptor::ComputeSavedRegisters(
    Register object, Register slot_address) {
  DCHECK(!AreAliased(object, slot_address));
  RegList saved_registers;
#if V8_TARGET_ARCH_X64
  // Only push clobbered registers.
  if (object != ObjectRegister()) saved_registers.set(ObjectRegister());
  if (slot_address != no_reg && slot_address != SlotAddressRegister()) {
    saved_registers.set(SlotAddressRegister());
  }
#elif V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_LOONG64 || \
    V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_MIPS
  if (object != ObjectRegister()) saved_registers.set(ObjectRegister());
  // The slot address is always clobbered.
  saved_registers.set(SlotAddressRegister());
#else
  // TODO(cbruni): Enable callee-saved registers for other platforms.
  // This is a temporary workaround to prepare code for callee-saved registers.
  constexpr auto allocated_registers = registers();
  for (size_t i = 0; i < allocated_registers.size(); ++i) {
    saved_registers.set(allocated_registers[i]);
  }
#endif
  return saved_registers;
}

// static
constexpr Register ApiGetterDescriptor::ReceiverRegister() {
  return LoadDescriptor::ReceiverRegister();
}

// static
constexpr Register LoadGlobalNoFeedbackDescriptor::ICKindRegister() {
  return LoadDescriptor::SlotRegister();
}

// static
constexpr Register LoadNoFeedbackDescriptor::ICKindRegister() {
  return LoadGlobalNoFeedbackDescriptor::ICKindRegister();
}

#if V8_TARGET_ARCH_IA32
// On ia32, LoadWithVectorDescriptor passes vector on the stack and thus we
// need to choose a new register here.
// static
constexpr Register LoadGlobalWithVectorDescriptor::VectorRegister() {
  STATIC_ASSERT(!LoadWithVectorDescriptor::VectorRegister().is_valid());
  return LoadDescriptor::ReceiverRegister();
}
#else
// static
constexpr Register LoadGlobalWithVectorDescriptor::VectorRegister() {
  return LoadWithVectorDescriptor::VectorRegister();
}
#endif

// static
constexpr auto LoadDescriptor::registers() {
  return RegisterArray(ReceiverRegister(), NameRegister(), SlotRegister());
}

// static
constexpr auto LoadBaselineDescriptor::registers() {
  return LoadDescriptor::registers();
}

// static
constexpr auto LoadGlobalDescriptor::registers() {
  return RegisterArray(LoadDescriptor::NameRegister(),
                       LoadDescriptor::SlotRegister());
}

// static
constexpr auto LoadGlobalBaselineDescriptor::registers() {
  return LoadGlobalDescriptor::registers();
}

// static
constexpr auto StoreDescriptor::registers() {
  return RegisterArray(ReceiverRegister(), NameRegister(), ValueRegister(),
                       SlotRegister());
}

// static
constexpr auto StoreBaselineDescriptor::registers() {
  return StoreDescriptor::registers();
}

// static
constexpr auto StoreGlobalDescriptor::registers() {
  return RegisterArray(StoreDescriptor::NameRegister(),
                       StoreDescriptor::ValueRegister(),
                       StoreDescriptor::SlotRegister());
}

// static
constexpr auto StoreGlobalBaselineDescriptor::registers() {
  return StoreGlobalDescriptor::registers();
}

// static
constexpr auto LoadWithReceiverBaselineDescriptor::registers() {
  return RegisterArray(
      LoadDescriptor::ReceiverRegister(),
      LoadWithReceiverAndVectorDescriptor::LookupStartObjectRegister(),
      LoadDescriptor::NameRegister(), LoadDescriptor::SlotRegister());
}

// static
constexpr auto BaselineOutOfLinePrologueDescriptor::registers() {
  // TODO(v8:11421): Implement on other platforms.
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_ARM ||       \
    V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_S390 ||      \
    V8_TARGET_ARCH_RISCV64 || V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_MIPS || \
    V8_TARGET_ARCH_LOONG64
  return RegisterArray(
      kContextRegister, kJSFunctionRegister, kJavaScriptCallArgCountRegister,
      kJavaScriptCallExtraArg1Register, kJavaScriptCallNewTargetRegister,
      kInterpreterBytecodeArrayRegister);
#elif V8_TARGET_ARCH_IA32
  STATIC_ASSERT(kJSFunctionRegister == kInterpreterBytecodeArrayRegister);
  return RegisterArray(
      kContextRegister, kJSFunctionRegister, kJavaScriptCallArgCountRegister,
      kJavaScriptCallExtraArg1Register, kJavaScriptCallNewTargetRegister);
#else
  return DefaultRegisterArray();
#endif
}

// static
constexpr auto BaselineLeaveFrameDescriptor::registers() {
  // TODO(v8:11421): Implement on other platforms.
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_ARM64 ||      \
    V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64 ||       \
    V8_TARGET_ARCH_S390 || V8_TARGET_ARCH_RISCV64 || V8_TARGET_ARCH_MIPS64 || \
    V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_LOONG64
  return RegisterArray(ParamsSizeRegister(), WeightRegister());
#else
  return DefaultRegisterArray();
#endif
}

// static
constexpr auto VoidDescriptor::registers() { return RegisterArray(); }

// static
constexpr auto AllocateDescriptor::registers() {
  return RegisterArray(kAllocateSizeRegister);
}

// static
constexpr auto CEntry1ArgvOnStackDescriptor::registers() {
  return RegisterArray(kRuntimeCallArgCountRegister,
                       kRuntimeCallFunctionRegister);
}

// static
constexpr auto InterpreterCEntry1Descriptor::registers() {
  return RegisterArray(kRuntimeCallArgCountRegister, kRuntimeCallArgvRegister,
                       kRuntimeCallFunctionRegister);
}

// static
constexpr auto InterpreterCEntry2Descriptor::registers() {
  return RegisterArray(kRuntimeCallArgCountRegister, kRuntimeCallArgvRegister,
                       kRuntimeCallFunctionRegister);
}

// static
constexpr auto FastNewObjectDescriptor::registers() {
  return RegisterArray(TargetRegister(), NewTargetRegister());
}

// static
constexpr auto LoadNoFeedbackDescriptor::registers() {
  return RegisterArray(LoadDescriptor::ReceiverRegister(),
                       LoadDescriptor::NameRegister(), ICKindRegister());
}

// static
constexpr auto LoadGlobalNoFeedbackDescriptor::registers() {
  return RegisterArray(LoadDescriptor::NameRegister(), ICKindRegister());
}

// static
constexpr auto LoadGlobalWithVectorDescriptor::registers() {
  return RegisterArray(LoadDescriptor::NameRegister(),
                       LoadDescriptor::SlotRegister(), VectorRegister());
}

// static
constexpr auto LoadWithReceiverAndVectorDescriptor::registers() {
  return RegisterArray(
      LoadDescriptor::ReceiverRegister(), LookupStartObjectRegister(),
      LoadDescriptor::NameRegister(), LoadDescriptor::SlotRegister(),
      LoadWithVectorDescriptor::VectorRegister());
}

// static
constexpr auto StoreGlobalWithVectorDescriptor::registers() {
  return RegisterArray(StoreDescriptor::NameRegister(),
                       StoreDescriptor::ValueRegister(),
                       StoreDescriptor::SlotRegister(),
                       StoreWithVectorDescriptor::VectorRegister());
}

// static
constexpr auto StoreTransitionDescriptor::registers() {
  return RegisterArray(StoreDescriptor::ReceiverRegister(),
                       StoreDescriptor::NameRegister(), MapRegister(),
                       StoreDescriptor::ValueRegister(),
                       StoreDescriptor::SlotRegister(),
                       StoreWithVectorDescriptor::VectorRegister());
}

// static
constexpr auto TypeConversionDescriptor::registers() {
  return RegisterArray(ArgumentRegister());
}

// static
constexpr auto TypeConversionNoContextDescriptor::registers() {
  return RegisterArray(TypeConversionDescriptor::ArgumentRegister());
}

// static
constexpr auto SingleParameterOnStackDescriptor::registers() {
  return RegisterArray();
}

// static
constexpr auto AsyncFunctionStackParameterDescriptor::registers() {
  return RegisterArray();
}

// static
constexpr auto GetIteratorStackParameterDescriptor::registers() {
  return RegisterArray();
}

// static
constexpr auto LoadWithVectorDescriptor::registers() {
  return RegisterArray(LoadDescriptor::ReceiverRegister(),
                       LoadDescriptor::NameRegister(),
                       LoadDescriptor::SlotRegister(), VectorRegister());
}

// static
constexpr auto KeyedLoadBaselineDescriptor::registers() {
  return RegisterArray(ReceiverRegister(), NameRegister(), SlotRegister());
}

// static
constexpr auto KeyedLoadDescriptor::registers() {
  return KeyedLoadBaselineDescriptor::registers();
}

// static
constexpr auto KeyedLoadWithVectorDescriptor::registers() {
  return RegisterArray(KeyedLoadBaselineDescriptor::ReceiverRegister(),
                       KeyedLoadBaselineDescriptor::NameRegister(),
                       KeyedLoadBaselineDescriptor::SlotRegister(),
                       VectorRegister());
}

// static
constexpr auto KeyedHasICBaselineDescriptor::registers() {
  return RegisterArray(ReceiverRegister(), NameRegister(), SlotRegister());
}

// static
constexpr auto KeyedHasICWithVectorDescriptor::registers() {
  return RegisterArray(KeyedHasICBaselineDescriptor::ReceiverRegister(),
                       KeyedHasICBaselineDescriptor::NameRegister(),
                       KeyedHasICBaselineDescriptor::SlotRegister(),
                       VectorRegister());
}

// static
constexpr auto StoreWithVectorDescriptor::registers() {
  return RegisterArray(StoreDescriptor::ReceiverRegister(),
                       StoreDescriptor::NameRegister(),
                       StoreDescriptor::ValueRegister(),
                       StoreDescriptor::SlotRegister(), VectorRegister());
}

// static
constexpr auto ApiGetterDescriptor::registers() {
  return RegisterArray(ReceiverRegister(), HolderRegister(),
                       CallbackRegister());
}

// static
constexpr auto ContextOnlyDescriptor::registers() { return RegisterArray(); }

// static
constexpr auto NoContextDescriptor::registers() { return RegisterArray(); }

// static
constexpr auto GrowArrayElementsDescriptor::registers() {
  return RegisterArray(ObjectRegister(), KeyRegister());
}

// static
constexpr auto ArrayNArgumentsConstructorDescriptor::registers() {
  // Keep the arguments on the same registers as they were in
  // ArrayConstructorDescriptor to avoid unnecessary register moves.
  // kFunction, kAllocationSite, kActualArgumentsCount
  return RegisterArray(kJavaScriptCallTargetRegister,
                       kJavaScriptCallExtraArg1Register,
                       kJavaScriptCallArgCountRegister);
}

// static
constexpr auto ArrayNoArgumentConstructorDescriptor::registers() {
  // This descriptor must use the same set of registers as the
  // ArrayNArgumentsConstructorDescriptor.
  return ArrayNArgumentsConstructorDescriptor::registers();
}

// static
constexpr auto ArraySingleArgumentConstructorDescriptor::registers() {
  // This descriptor must use the same set of registers as the
  // ArrayNArgumentsConstructorDescriptor.
  return ArrayNArgumentsConstructorDescriptor::registers();
}

// static
// static
constexpr Register RunMicrotasksDescriptor::MicrotaskQueueRegister() {
  return GetRegisterParameter(0);
}

#define DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER(Name, DescriptorName) \
  template <>                                                         \
  struct CallInterfaceDescriptorFor<Builtin::k##Name> {               \
    using type = DescriptorName##Descriptor;                          \
  };
BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN,
             /*TFC*/ DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER, IGNORE_BUILTIN,
             /*TFH*/ DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER, IGNORE_BUILTIN,
             /*ASM*/ DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER)
#undef DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER
#define DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER(Name, ...) \
  template <>                                              \
  struct CallInterfaceDescriptorFor<Builtin::k##Name> {    \
    using type = Name##Descriptor;                         \
  };
BUILTIN_LIST_TFS(DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER)
#undef DEFINE_STATIC_BUILTIN_DESCRIPTOR_GETTER

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_INTERFACE_DESCRIPTORS_INL_H_

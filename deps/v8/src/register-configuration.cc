// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/register-configuration.h"
#include "src/globals.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

namespace {

#define REGISTER_COUNT(R) 1 +
static const int kMaxAllocatableGeneralRegisterCount =
    ALLOCATABLE_GENERAL_REGISTERS(REGISTER_COUNT)0;
static const int kMaxAllocatableDoubleRegisterCount =
    ALLOCATABLE_DOUBLE_REGISTERS(REGISTER_COUNT)0;

static const int kAllocatableGeneralCodes[] = {
#define REGISTER_CODE(R) Register::kCode_##R,
    ALLOCATABLE_GENERAL_REGISTERS(REGISTER_CODE)};
#undef REGISTER_CODE

static const int kAllocatableDoubleCodes[] = {
#define REGISTER_CODE(R) DoubleRegister::kCode_##R,
    ALLOCATABLE_DOUBLE_REGISTERS(REGISTER_CODE)};
#undef REGISTER_CODE

static const char* const kGeneralRegisterNames[] = {
#define REGISTER_NAME(R) #R,
    GENERAL_REGISTERS(REGISTER_NAME)
#undef REGISTER_NAME
};

static const char* const kDoubleRegisterNames[] = {
#define REGISTER_NAME(R) #R,
    DOUBLE_REGISTERS(REGISTER_NAME)
#undef REGISTER_NAME
};

STATIC_ASSERT(RegisterConfiguration::kMaxGeneralRegisters >=
              Register::kNumRegisters);
STATIC_ASSERT(RegisterConfiguration::kMaxDoubleRegisters >=
              DoubleRegister::kMaxNumRegisters);

class ArchDefaultRegisterConfiguration : public RegisterConfiguration {
 public:
  explicit ArchDefaultRegisterConfiguration(CompilerSelector compiler)
      : RegisterConfiguration(Register::kNumRegisters,
                              DoubleRegister::kMaxNumRegisters,
#if V8_TARGET_ARCH_IA32
                              kMaxAllocatableGeneralRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
#elif V8_TARGET_ARCH_X87
                              kMaxAllocatableGeneralRegisterCount,
                              compiler == TURBOFAN
                                  ? 1
                                  : kMaxAllocatableDoubleRegisterCount,
                              compiler == TURBOFAN
                                  ? 1
                                  : kMaxAllocatableDoubleRegisterCount,
#elif V8_TARGET_ARCH_X64
                              kMaxAllocatableGeneralRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
#elif V8_TARGET_ARCH_ARM
                              FLAG_enable_embedded_constant_pool
                                  ? (kMaxAllocatableGeneralRegisterCount - 1)
                                  : kMaxAllocatableGeneralRegisterCount,
                              CpuFeatures::IsSupported(VFP32DREGS)
                                  ? kMaxAllocatableDoubleRegisterCount
                                  : (ALLOCATABLE_NO_VFP32_DOUBLE_REGISTERS(
                                        REGISTER_COUNT)0),
                              ALLOCATABLE_NO_VFP32_DOUBLE_REGISTERS(
                                  REGISTER_COUNT)0,
#elif V8_TARGET_ARCH_ARM64
                              kMaxAllocatableGeneralRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
#elif V8_TARGET_ARCH_MIPS
                              kMaxAllocatableGeneralRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
#elif V8_TARGET_ARCH_MIPS64
                              kMaxAllocatableGeneralRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
#elif V8_TARGET_ARCH_PPC
                              kMaxAllocatableGeneralRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
                              kMaxAllocatableDoubleRegisterCount,
#else
#error Unsupported target architecture.
#endif
                              kAllocatableGeneralCodes, kAllocatableDoubleCodes,
                              kGeneralRegisterNames, kDoubleRegisterNames) {
  }
};


template <RegisterConfiguration::CompilerSelector compiler>
struct RegisterConfigurationInitializer {
  static void Construct(ArchDefaultRegisterConfiguration* config) {
    new (config) ArchDefaultRegisterConfiguration(compiler);
  }
};

static base::LazyInstance<
    ArchDefaultRegisterConfiguration,
    RegisterConfigurationInitializer<RegisterConfiguration::CRANKSHAFT>>::type
    kDefaultRegisterConfigurationForCrankshaft = LAZY_INSTANCE_INITIALIZER;


static base::LazyInstance<
    ArchDefaultRegisterConfiguration,
    RegisterConfigurationInitializer<RegisterConfiguration::TURBOFAN>>::type
    kDefaultRegisterConfigurationForTurboFan = LAZY_INSTANCE_INITIALIZER;

}  // namespace


const RegisterConfiguration* RegisterConfiguration::ArchDefault(
    CompilerSelector compiler) {
  return compiler == TURBOFAN
             ? &kDefaultRegisterConfigurationForTurboFan.Get()
             : &kDefaultRegisterConfigurationForCrankshaft.Get();
}


RegisterConfiguration::RegisterConfiguration(
    int num_general_registers, int num_double_registers,
    int num_allocatable_general_registers, int num_allocatable_double_registers,
    int num_allocatable_aliased_double_registers,
    const int* allocatable_general_codes, const int* allocatable_double_codes,
    const char* const* general_register_names,
    const char* const* double_register_names)
    : num_general_registers_(num_general_registers),
      num_double_registers_(num_double_registers),
      num_allocatable_general_registers_(num_allocatable_general_registers),
      num_allocatable_double_registers_(num_allocatable_double_registers),
      num_allocatable_aliased_double_registers_(
          num_allocatable_aliased_double_registers),
      allocatable_general_codes_mask_(0),
      allocatable_double_codes_mask_(0),
      allocatable_general_codes_(allocatable_general_codes),
      allocatable_double_codes_(allocatable_double_codes),
      general_register_names_(general_register_names),
      double_register_names_(double_register_names) {
  for (int i = 0; i < num_allocatable_general_registers_; ++i) {
    allocatable_general_codes_mask_ |= (1 << allocatable_general_codes_[i]);
  }
  for (int i = 0; i < num_allocatable_double_registers_; ++i) {
    allocatable_double_codes_mask_ |= (1 << allocatable_double_codes_[i]);
  }
}

#undef REGISTER_COUNT

}  // namespace internal
}  // namespace v8

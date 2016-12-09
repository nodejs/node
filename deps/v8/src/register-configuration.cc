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

static const char* const kFloatRegisterNames[] = {
#define REGISTER_NAME(R) #R,
    FLOAT_REGISTERS(REGISTER_NAME)
#undef REGISTER_NAME
};

static const char* const kDoubleRegisterNames[] = {
#define REGISTER_NAME(R) #R,
    DOUBLE_REGISTERS(REGISTER_NAME)
#undef REGISTER_NAME
};

static const char* const kSimd128RegisterNames[] = {
#define REGISTER_NAME(R) #R,
    SIMD128_REGISTERS(REGISTER_NAME)
#undef REGISTER_NAME
};

STATIC_ASSERT(RegisterConfiguration::kMaxGeneralRegisters >=
              Register::kNumRegisters);
STATIC_ASSERT(RegisterConfiguration::kMaxFPRegisters >=
              FloatRegister::kMaxNumRegisters);
STATIC_ASSERT(RegisterConfiguration::kMaxFPRegisters >=
              DoubleRegister::kMaxNumRegisters);
STATIC_ASSERT(RegisterConfiguration::kMaxFPRegisters >=
              Simd128Register::kMaxNumRegisters);

enum CompilerSelector { CRANKSHAFT, TURBOFAN };

class ArchDefaultRegisterConfiguration : public RegisterConfiguration {
 public:
  explicit ArchDefaultRegisterConfiguration(CompilerSelector compiler)
      : RegisterConfiguration(
            Register::kNumRegisters, DoubleRegister::kMaxNumRegisters,
#if V8_TARGET_ARCH_IA32
            kMaxAllocatableGeneralRegisterCount,
            kMaxAllocatableDoubleRegisterCount,
            kMaxAllocatableDoubleRegisterCount,
#elif V8_TARGET_ARCH_X87
            kMaxAllocatableGeneralRegisterCount,
            compiler == TURBOFAN ? 1 : kMaxAllocatableDoubleRegisterCount,
            compiler == TURBOFAN ? 1 : kMaxAllocatableDoubleRegisterCount,
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
                : (ALLOCATABLE_NO_VFP32_DOUBLE_REGISTERS(REGISTER_COUNT) 0),
            ALLOCATABLE_NO_VFP32_DOUBLE_REGISTERS(REGISTER_COUNT) 0,
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
#elif V8_TARGET_ARCH_S390
            kMaxAllocatableGeneralRegisterCount,
            kMaxAllocatableDoubleRegisterCount,
            kMaxAllocatableDoubleRegisterCount,
#else
#error Unsupported target architecture.
#endif
            kAllocatableGeneralCodes, kAllocatableDoubleCodes,
            kSimpleFPAliasing ? AliasingKind::OVERLAP : AliasingKind::COMBINE,
            kGeneralRegisterNames, kFloatRegisterNames, kDoubleRegisterNames,
            kSimd128RegisterNames) {
  }
};

template <CompilerSelector compiler>
struct RegisterConfigurationInitializer {
  static void Construct(ArchDefaultRegisterConfiguration* config) {
    new (config) ArchDefaultRegisterConfiguration(compiler);
  }
};

static base::LazyInstance<ArchDefaultRegisterConfiguration,
                          RegisterConfigurationInitializer<CRANKSHAFT>>::type
    kDefaultRegisterConfigurationForCrankshaft = LAZY_INSTANCE_INITIALIZER;

static base::LazyInstance<ArchDefaultRegisterConfiguration,
                          RegisterConfigurationInitializer<TURBOFAN>>::type
    kDefaultRegisterConfigurationForTurboFan = LAZY_INSTANCE_INITIALIZER;

}  // namespace

const RegisterConfiguration* RegisterConfiguration::Crankshaft() {
  return &kDefaultRegisterConfigurationForCrankshaft.Get();
}

const RegisterConfiguration* RegisterConfiguration::Turbofan() {
  return &kDefaultRegisterConfigurationForTurboFan.Get();
}

RegisterConfiguration::RegisterConfiguration(
    int num_general_registers, int num_double_registers,
    int num_allocatable_general_registers, int num_allocatable_double_registers,
    int num_allocatable_aliased_double_registers,
    const int* allocatable_general_codes, const int* allocatable_double_codes,
    AliasingKind fp_aliasing_kind, const char* const* general_register_names,
    const char* const* float_register_names,
    const char* const* double_register_names,
    const char* const* simd128_register_names)
    : num_general_registers_(num_general_registers),
      num_float_registers_(0),
      num_double_registers_(num_double_registers),
      num_simd128_registers_(0),
      num_allocatable_general_registers_(num_allocatable_general_registers),
      num_allocatable_float_registers_(0),
      num_allocatable_double_registers_(num_allocatable_double_registers),
      num_allocatable_aliased_double_registers_(
          num_allocatable_aliased_double_registers),
      num_allocatable_simd128_registers_(0),
      allocatable_general_codes_mask_(0),
      allocatable_float_codes_mask_(0),
      allocatable_double_codes_mask_(0),
      allocatable_simd128_codes_mask_(0),
      allocatable_general_codes_(allocatable_general_codes),
      allocatable_double_codes_(allocatable_double_codes),
      fp_aliasing_kind_(fp_aliasing_kind),
      general_register_names_(general_register_names),
      float_register_names_(float_register_names),
      double_register_names_(double_register_names),
      simd128_register_names_(simd128_register_names) {
  DCHECK(num_general_registers_ <= RegisterConfiguration::kMaxGeneralRegisters);
  DCHECK(num_double_registers_ <= RegisterConfiguration::kMaxFPRegisters);
  for (int i = 0; i < num_allocatable_general_registers_; ++i) {
    allocatable_general_codes_mask_ |= (1 << allocatable_general_codes_[i]);
  }
  for (int i = 0; i < num_allocatable_double_registers_; ++i) {
    allocatable_double_codes_mask_ |= (1 << allocatable_double_codes_[i]);
  }

  if (fp_aliasing_kind_ == COMBINE) {
    num_float_registers_ = num_double_registers_ * 2 <= kMaxFPRegisters
                               ? num_double_registers_ * 2
                               : kMaxFPRegisters;
    num_allocatable_float_registers_ = 0;
    for (int i = 0; i < num_allocatable_double_registers_; i++) {
      int base_code = allocatable_double_codes_[i] * 2;
      if (base_code >= kMaxFPRegisters) continue;
      allocatable_float_codes_[num_allocatable_float_registers_++] = base_code;
      allocatable_float_codes_[num_allocatable_float_registers_++] =
          base_code + 1;
      allocatable_float_codes_mask_ |= (0x3 << base_code);
    }
    num_simd128_registers_ = num_double_registers_ / 2;
    num_allocatable_simd128_registers_ = 0;
    int last_simd128_code = allocatable_double_codes_[0] / 2;
    for (int i = 1; i < num_allocatable_double_registers_; i++) {
      int next_simd128_code = allocatable_double_codes_[i] / 2;
      // This scheme assumes allocatable_double_codes_ are strictly increasing.
      DCHECK_GE(next_simd128_code, last_simd128_code);
      if (last_simd128_code == next_simd128_code) {
        allocatable_simd128_codes_[num_allocatable_simd128_registers_++] =
            next_simd128_code;
        allocatable_simd128_codes_mask_ |= (0x1 << next_simd128_code);
      }
      last_simd128_code = next_simd128_code;
    }
  } else {
    DCHECK(fp_aliasing_kind_ == OVERLAP);
    num_float_registers_ = num_simd128_registers_ = num_double_registers_;
    num_allocatable_float_registers_ = num_allocatable_simd128_registers_ =
        num_allocatable_double_registers_;
    for (int i = 0; i < num_allocatable_float_registers_; ++i) {
      allocatable_float_codes_[i] = allocatable_simd128_codes_[i] =
          allocatable_double_codes_[i];
    }
    allocatable_float_codes_mask_ = allocatable_simd128_codes_mask_ =
        allocatable_double_codes_mask_;
  }
}

// Assert that kFloat32, kFloat64, and kSimd128 are consecutive values.
STATIC_ASSERT(static_cast<int>(MachineRepresentation::kSimd128) ==
              static_cast<int>(MachineRepresentation::kFloat64) + 1);
STATIC_ASSERT(static_cast<int>(MachineRepresentation::kFloat64) ==
              static_cast<int>(MachineRepresentation::kFloat32) + 1);

int RegisterConfiguration::GetAliases(MachineRepresentation rep, int index,
                                      MachineRepresentation other_rep,
                                      int* alias_base_index) const {
  DCHECK(fp_aliasing_kind_ == COMBINE);
  DCHECK(IsFloatingPoint(rep) && IsFloatingPoint(other_rep));
  if (rep == other_rep) {
    *alias_base_index = index;
    return 1;
  }
  int rep_int = static_cast<int>(rep);
  int other_rep_int = static_cast<int>(other_rep);
  if (rep_int > other_rep_int) {
    int shift = rep_int - other_rep_int;
    int base_index = index << shift;
    if (base_index >= kMaxFPRegisters) {
      // Alias indices would be out of FP register range.
      return 0;
    }
    *alias_base_index = base_index;
    return 1 << shift;
  }
  int shift = other_rep_int - rep_int;
  *alias_base_index = index >> shift;
  return 1;
}

bool RegisterConfiguration::AreAliases(MachineRepresentation rep, int index,
                                       MachineRepresentation other_rep,
                                       int other_index) const {
  DCHECK(fp_aliasing_kind_ == COMBINE);
  DCHECK(IsFloatingPoint(rep) && IsFloatingPoint(other_rep));
  if (rep == other_rep) {
    return index == other_index;
  }
  int rep_int = static_cast<int>(rep);
  int other_rep_int = static_cast<int>(other_rep);
  if (rep_int > other_rep_int) {
    int shift = rep_int - other_rep_int;
    return index == other_index >> shift;
  }
  int shift = other_rep_int - rep_int;
  return index >> shift == other_index;
}

#undef REGISTER_COUNT

}  // namespace internal
}  // namespace v8

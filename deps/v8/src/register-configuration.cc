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
#define REGISTER_CODE(R) kRegCode_##R,
    ALLOCATABLE_GENERAL_REGISTERS(REGISTER_CODE)};
#undef REGISTER_CODE

#define REGISTER_CODE(R) kDoubleCode_##R,
static const int kAllocatableDoubleCodes[] = {
    ALLOCATABLE_DOUBLE_REGISTERS(REGISTER_CODE)};
#if V8_TARGET_ARCH_ARM
static const int kAllocatableNoVFP32DoubleCodes[] = {
    ALLOCATABLE_NO_VFP32_DOUBLE_REGISTERS(REGISTER_CODE)};
#endif  // V8_TARGET_ARCH_ARM
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
              FloatRegister::kNumRegisters);
STATIC_ASSERT(RegisterConfiguration::kMaxFPRegisters >=
              DoubleRegister::kNumRegisters);
STATIC_ASSERT(RegisterConfiguration::kMaxFPRegisters >=
              Simd128Register::kNumRegisters);

static int get_num_allocatable_double_registers() {
  return
#if V8_TARGET_ARCH_IA32
      kMaxAllocatableDoubleRegisterCount;
#elif V8_TARGET_ARCH_X64
      kMaxAllocatableDoubleRegisterCount;
#elif V8_TARGET_ARCH_ARM
      CpuFeatures::IsSupported(VFP32DREGS)
          ? kMaxAllocatableDoubleRegisterCount
          : (ALLOCATABLE_NO_VFP32_DOUBLE_REGISTERS(REGISTER_COUNT) 0);
#elif V8_TARGET_ARCH_ARM64
      kMaxAllocatableDoubleRegisterCount;
#elif V8_TARGET_ARCH_MIPS
      kMaxAllocatableDoubleRegisterCount;
#elif V8_TARGET_ARCH_MIPS64
      kMaxAllocatableDoubleRegisterCount;
#elif V8_TARGET_ARCH_PPC
      kMaxAllocatableDoubleRegisterCount;
#elif V8_TARGET_ARCH_S390
      kMaxAllocatableDoubleRegisterCount;
#else
#error Unsupported target architecture.
#endif
}

static const int* get_allocatable_double_codes() {
  return
#if V8_TARGET_ARCH_ARM
      CpuFeatures::IsSupported(VFP32DREGS) ? kAllocatableDoubleCodes
                                           : kAllocatableNoVFP32DoubleCodes;
#else
      kAllocatableDoubleCodes;
#endif
}

class ArchDefaultRegisterConfiguration : public RegisterConfiguration {
 public:
  ArchDefaultRegisterConfiguration()
      : RegisterConfiguration(
            Register::kNumRegisters, DoubleRegister::kNumRegisters,
            kMaxAllocatableGeneralRegisterCount,
            get_num_allocatable_double_registers(), kAllocatableGeneralCodes,
            get_allocatable_double_codes(),
            kSimpleFPAliasing ? AliasingKind::OVERLAP : AliasingKind::COMBINE,
            kGeneralRegisterNames, kFloatRegisterNames, kDoubleRegisterNames,
            kSimd128RegisterNames) {}
};

struct RegisterConfigurationInitializer {
  static void Construct(void* config) {
    new (config) ArchDefaultRegisterConfiguration();
  }
};

static base::LazyInstance<ArchDefaultRegisterConfiguration,
                          RegisterConfigurationInitializer>::type
    kDefaultRegisterConfiguration = LAZY_INSTANCE_INITIALIZER;

// Allocatable registers with the masking register removed.
class ArchDefaultPoisoningRegisterConfiguration : public RegisterConfiguration {
 public:
  ArchDefaultPoisoningRegisterConfiguration()
      : RegisterConfiguration(
            Register::kNumRegisters, DoubleRegister::kNumRegisters,
            kMaxAllocatableGeneralRegisterCount - 1,
            get_num_allocatable_double_registers(),
            InitializeGeneralRegisterCodes(), get_allocatable_double_codes(),
            kSimpleFPAliasing ? AliasingKind::OVERLAP : AliasingKind::COMBINE,
            kGeneralRegisterNames, kFloatRegisterNames, kDoubleRegisterNames,
            kSimd128RegisterNames) {}

 private:
  static const int* InitializeGeneralRegisterCodes() {
    int filtered_index = 0;
    for (int i = 0; i < kMaxAllocatableGeneralRegisterCount; ++i) {
      if (kAllocatableGeneralCodes[i] != kSpeculationPoisonRegister.code()) {
        allocatable_general_codes_[filtered_index] =
            kAllocatableGeneralCodes[i];
        filtered_index++;
      }
    }
    DCHECK_EQ(filtered_index, kMaxAllocatableGeneralRegisterCount - 1);
    return allocatable_general_codes_;
  }

  static int
      allocatable_general_codes_[kMaxAllocatableGeneralRegisterCount - 1];
};

int ArchDefaultPoisoningRegisterConfiguration::allocatable_general_codes_
    [kMaxAllocatableGeneralRegisterCount - 1];

struct PoisoningRegisterConfigurationInitializer {
  static void Construct(void* config) {
    new (config) ArchDefaultPoisoningRegisterConfiguration();
  }
};

static base::LazyInstance<ArchDefaultPoisoningRegisterConfiguration,
                          PoisoningRegisterConfigurationInitializer>::type
    kDefaultPoisoningRegisterConfiguration = LAZY_INSTANCE_INITIALIZER;

#if defined(V8_TARGET_ARCH_IA32) && defined(V8_EMBEDDED_BUILTINS)
// Allocatable registers with the root register removed.
// TODO(v8:6666): Once all builtins have been migrated, we could remove this
// configuration and remove kRootRegister from ALLOCATABLE_GENERAL_REGISTERS
// instead.
class ArchPreserveRootIA32RegisterConfiguration : public RegisterConfiguration {
 public:
  ArchPreserveRootIA32RegisterConfiguration()
      : RegisterConfiguration(
            Register::kNumRegisters, DoubleRegister::kNumRegisters,
            kMaxAllocatableGeneralRegisterCount - 1,
            get_num_allocatable_double_registers(),
            InitializeGeneralRegisterCodes(), get_allocatable_double_codes(),
            kSimpleFPAliasing ? AliasingKind::OVERLAP : AliasingKind::COMBINE,
            kGeneralRegisterNames, kFloatRegisterNames, kDoubleRegisterNames,
            kSimd128RegisterNames) {}

 private:
  static const int* InitializeGeneralRegisterCodes() {
    int filtered_index = 0;
    for (int i = 0; i < kMaxAllocatableGeneralRegisterCount; ++i) {
      if (kAllocatableGeneralCodes[i] != kRootRegister.code()) {
        allocatable_general_codes_[filtered_index] =
            kAllocatableGeneralCodes[i];
        filtered_index++;
      }
    }
    DCHECK_EQ(filtered_index, kMaxAllocatableGeneralRegisterCount - 1);
    return allocatable_general_codes_;
  }

  static int
      allocatable_general_codes_[kMaxAllocatableGeneralRegisterCount - 1];
};

int ArchPreserveRootIA32RegisterConfiguration::allocatable_general_codes_
    [kMaxAllocatableGeneralRegisterCount - 1];

struct PreserveRootIA32RegisterConfigurationInitializer {
  static void Construct(void* config) {
    new (config) ArchPreserveRootIA32RegisterConfiguration();
  }
};

static base::LazyInstance<ArchPreserveRootIA32RegisterConfiguration,
                          PreserveRootIA32RegisterConfigurationInitializer>::
    type kPreserveRootIA32RegisterConfiguration = LAZY_INSTANCE_INITIALIZER;
#endif  // defined(V8_TARGET_ARCH_IA32) && defined(V8_EMBEDDED_BUILTINS)

// RestrictedRegisterConfiguration uses the subset of allocatable general
// registers the architecture support, which results into generating assembly
// to use less registers. Currently, it's only used by RecordWrite code stub.
class RestrictedRegisterConfiguration : public RegisterConfiguration {
 public:
  RestrictedRegisterConfiguration(
      int num_allocatable_general_registers,
      std::unique_ptr<int[]> allocatable_general_register_codes,
      std::unique_ptr<char const* []> allocatable_general_register_names)
      : RegisterConfiguration(
            Register::kNumRegisters, DoubleRegister::kNumRegisters,
            num_allocatable_general_registers,
            get_num_allocatable_double_registers(),
            allocatable_general_register_codes.get(),
            get_allocatable_double_codes(),
            kSimpleFPAliasing ? AliasingKind::OVERLAP : AliasingKind::COMBINE,
            kGeneralRegisterNames, kFloatRegisterNames, kDoubleRegisterNames,
            kSimd128RegisterNames),
        allocatable_general_register_codes_(
            std::move(allocatable_general_register_codes)),
        allocatable_general_register_names_(
            std::move(allocatable_general_register_names)) {
    for (int i = 0; i < num_allocatable_general_registers; ++i) {
      DCHECK(
          IsAllocatableGeneralRegister(allocatable_general_register_codes_[i]));
    }
  }

  bool IsAllocatableGeneralRegister(int code) {
    for (int i = 0; i < kMaxAllocatableGeneralRegisterCount; ++i) {
      if (code == kAllocatableGeneralCodes[i]) {
        return true;
      }
    }
    return false;
  }

 private:
  std::unique_ptr<int[]> allocatable_general_register_codes_;
  std::unique_ptr<char const* []> allocatable_general_register_names_;
};

}  // namespace

const RegisterConfiguration* RegisterConfiguration::Default() {
  return &kDefaultRegisterConfiguration.Get();
}

const RegisterConfiguration* RegisterConfiguration::Poisoning() {
  return &kDefaultPoisoningRegisterConfiguration.Get();
}

#if defined(V8_TARGET_ARCH_IA32) && defined(V8_EMBEDDED_BUILTINS)
const RegisterConfiguration* RegisterConfiguration::PreserveRootIA32() {
  return &kPreserveRootIA32RegisterConfiguration.Get();
}
#endif  // defined(V8_TARGET_ARCH_IA32) && defined(V8_EMBEDDED_BUILTINS)

const RegisterConfiguration* RegisterConfiguration::RestrictGeneralRegisters(
    RegList registers) {
  int num = NumRegs(registers);
  std::unique_ptr<int[]> codes{new int[num]};
  std::unique_ptr<char const* []> names { new char const*[num] };
  int counter = 0;
  for (int i = 0; i < Default()->num_allocatable_general_registers(); ++i) {
    auto reg = Register::from_code(Default()->GetAllocatableGeneralCode(i));
    if (reg.bit() & registers) {
      DCHECK(counter < num);
      codes[counter] = reg.code();
      names[counter] = Default()->GetGeneralRegisterName(i);
      counter++;
    }
  }

  return new RestrictedRegisterConfiguration(num, std::move(codes),
                                             std::move(names));
}

RegisterConfiguration::RegisterConfiguration(
    int num_general_registers, int num_double_registers,
    int num_allocatable_general_registers, int num_allocatable_double_registers,
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
  DCHECK_LE(num_general_registers_,
            RegisterConfiguration::kMaxGeneralRegisters);
  DCHECK_LE(num_double_registers_, RegisterConfiguration::kMaxFPRegisters);
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

const char* RegisterConfiguration::GetGeneralOrSpecialRegisterName(
    int code) const {
  if (code < num_general_registers_) return GetGeneralRegisterName(code);
  return Assembler::GetSpecialRegisterName(code);
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

}  // namespace internal
}  // namespace v8

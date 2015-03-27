// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/register-configuration.h"
#include "src/globals.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

STATIC_ASSERT(RegisterConfiguration::kMaxGeneralRegisters >=
              Register::kNumRegisters);
STATIC_ASSERT(RegisterConfiguration::kMaxDoubleRegisters >=
              DoubleRegister::kMaxNumRegisters);

class ArchDefaultRegisterConfiguration : public RegisterConfiguration {
 public:
  ArchDefaultRegisterConfiguration()
      : RegisterConfiguration(Register::kMaxNumAllocatableRegisters,
                              DoubleRegister::kMaxNumAllocatableRegisters,
                              DoubleRegister::NumAllocatableAliasedRegisters(),
                              general_register_name_table_,
                              double_register_name_table_) {
    DCHECK_EQ(Register::kMaxNumAllocatableRegisters,
              Register::NumAllocatableRegisters());
    for (int i = 0; i < Register::kMaxNumAllocatableRegisters; ++i) {
      general_register_name_table_[i] = Register::AllocationIndexToString(i);
    }
    for (int i = 0; i < DoubleRegister::kMaxNumAllocatableRegisters; ++i) {
      double_register_name_table_[i] =
          DoubleRegister::AllocationIndexToString(i);
    }
  }

  const char*
      general_register_name_table_[Register::kMaxNumAllocatableRegisters];
  const char*
      double_register_name_table_[DoubleRegister::kMaxNumAllocatableRegisters];
};


static base::LazyInstance<ArchDefaultRegisterConfiguration>::type
    kDefaultRegisterConfiguration = LAZY_INSTANCE_INITIALIZER;

}  // namepace


const RegisterConfiguration* RegisterConfiguration::ArchDefault() {
  return &kDefaultRegisterConfiguration.Get();
}

RegisterConfiguration::RegisterConfiguration(
    int num_general_registers, int num_double_registers,
    int num_aliased_double_registers, const char* const* general_register_names,
    const char* const* double_register_names)
    : num_general_registers_(num_general_registers),
      num_double_registers_(num_double_registers),
      num_aliased_double_registers_(num_aliased_double_registers),
      general_register_names_(general_register_names),
      double_register_names_(double_register_names) {}


}  // namespace compiler
}  // namespace internal
}  // namespace v8

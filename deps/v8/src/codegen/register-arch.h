// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_REGISTER_ARCH_H_
#define V8_CODEGEN_REGISTER_ARCH_H_

#include "src/codegen/register.h"
#include "src/codegen/reglist.h"

#if V8_TARGET_ARCH_IA32
#include "src/codegen/ia32/register-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "src/codegen/x64/register-x64.h"
#elif V8_TARGET_ARCH_ARM64
#include "src/codegen/arm64/register-arm64.h"
#elif V8_TARGET_ARCH_ARM
#include "src/codegen/arm/register-arm.h"
#elif V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
#include "src/codegen/ppc/register-ppc.h"
#elif V8_TARGET_ARCH_MIPS
#include "src/codegen/mips/register-mips.h"
#elif V8_TARGET_ARCH_MIPS64
#include "src/codegen/mips64/register-mips64.h"
#elif V8_TARGET_ARCH_S390
#include "src/codegen/s390/register-s390.h"
#elif V8_TARGET_ARCH_RISCV64
#include "src/codegen/riscv64/register-riscv64.h"
#else
#error Unknown architecture.
#endif

namespace v8 {
namespace internal {

constexpr int AddArgumentPaddingSlots(int argument_count) {
  return argument_count + ArgumentPaddingSlots(argument_count);
}

constexpr bool ShouldPadArguments(int argument_count) {
  return ArgumentPaddingSlots(argument_count) != 0;
}

#ifdef DEBUG
struct CountIfValidRegisterFunctor {
  template <typename RegType>
  constexpr int operator()(int count, RegType reg) const {
    return count + (reg.is_valid() ? 1 : 0);
  }
};

template <typename RegType, typename... RegTypes,
          // All arguments must be either Register or DoubleRegister.
          typename = typename std::enable_if<
              base::is_same<Register, RegType, RegTypes...>::value ||
              base::is_same<DoubleRegister, RegType, RegTypes...>::value>::type>
inline constexpr bool AreAliased(RegType first_reg, RegTypes... regs) {
  int num_different_regs = NumRegs(RegType::ListOf(first_reg, regs...));
  int num_given_regs =
      base::fold(CountIfValidRegisterFunctor{}, 0, first_reg, regs...);
  return num_different_regs < num_given_regs;
}
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_REGISTER_ARCH_H_

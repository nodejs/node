// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_REGISTER_H_
#define V8_CODEGEN_REGISTER_H_

#include "src/codegen/register-arch.h"
#include "src/codegen/reglist.h"

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
  int num_different_regs = RegListBase<RegType>{first_reg, regs...}.Count();
  int num_given_regs =
      base::fold(CountIfValidRegisterFunctor{}, 0, first_reg, regs...);
  return num_different_regs < num_given_regs;
}
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_REGISTER_H_

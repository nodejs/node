// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_REGISTER_H_
#define V8_CODEGEN_REGISTER_H_

#include "src/base/bounds.h"
#include "src/codegen/reglist.h"

namespace v8 {

namespace internal {

// Base type for CPU Registers.
//
// 1) We would prefer to use an enum for registers, but enum values are
// assignment-compatible with int, which has caused code-generation bugs.
//
// 2) By not using an enum, we are possibly preventing the compiler from
// doing certain constant folds, which may significantly reduce the
// code generated for some assembly instructions (because they boil down
// to a few constants). If this is a problem, we could change the code
// such that we use an enum in optimized mode, and the class in debug
// mode. This way we get the compile-time error checking in debug mode
// and best performance in optimized code.
template <typename SubType, int kAfterLastRegister>
class RegisterBase {
 public:
  static constexpr int kCode_no_reg = -1;
  static constexpr int kNumRegisters = kAfterLastRegister;

  static constexpr SubType no_reg() { return SubType{kCode_no_reg}; }

  static constexpr SubType from_code(int code) {
    CONSTEXPR_DCHECK(base::IsInRange(code, 0, kNumRegisters - 1));
    return SubType{code};
  }

  template <typename... Register>
  static constexpr RegList ListOf(Register... regs) {
    return CombineRegLists(regs.bit()...);
  }

  constexpr bool is_valid() const { return reg_code_ != kCode_no_reg; }

  constexpr int code() const {
    CONSTEXPR_DCHECK(is_valid());
    return reg_code_;
  }

  constexpr RegList bit() const {
    return is_valid() ? RegList{1} << code() : RegList{};
  }

  inline constexpr bool operator==(SubType other) const {
    return reg_code_ == other.reg_code_;
  }
  inline constexpr bool operator!=(SubType other) const {
    return reg_code_ != other.reg_code_;
  }

  // Used to print the name of some special registers.
  static const char* GetSpecialRegisterName(int code) { return "UNKNOWN"; }

 protected:
  explicit constexpr RegisterBase(int code) : reg_code_(code) {}

 private:
  int reg_code_;
};

template <typename RegType,
          typename = decltype(RegisterName(std::declval<RegType>()))>
inline std::ostream& operator<<(std::ostream& os, RegType reg) {
  return os << RegisterName(reg);
}

// Helper macros to define a {RegisterName} method based on a macro list
// containing all names.
#define DEFINE_REGISTER_NAMES_NAME(name) #name,
#define DEFINE_REGISTER_NAMES(RegType, LIST)                                   \
  inline const char* RegisterName(RegType reg) {                               \
    static constexpr const char* Names[] = {LIST(DEFINE_REGISTER_NAMES_NAME)}; \
    STATIC_ASSERT(arraysize(Names) == RegType::kNumRegisters);                 \
    return reg.is_valid() ? Names[reg.code()] : "invalid";                     \
  }

}  // namespace internal
}  // namespace v8
#endif  // V8_CODEGEN_REGISTER_H_

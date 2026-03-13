// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_REGISTER_BASE_H_
#define V8_CODEGEN_REGISTER_BASE_H_

#include "src/base/bits.h"
#include "src/base/bounds.h"
#include "src/common/globals.h"

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
  static constexpr int8_t kCode_no_reg = -1;
  static constexpr int8_t kNumRegisters = kAfterLastRegister;

  static constexpr SubType no_reg() { return SubType{kCode_no_reg}; }

  static constexpr SubType from_code(int8_t code) {
    V8_ASSUME(code >= 0 && code < kNumRegisters);
    return SubType{code};
  }

  constexpr bool is_valid() const { return reg_code_ != kCode_no_reg; }

  constexpr int8_t code() const {
#if V8_TARGET_ARCH_ARM64
    // Arm64 uses kSPRegInternalCode which is > kNumRegisters.
    V8_ASSUME(reg_code_ >= 0);
#else
    V8_ASSUME(reg_code_ >= 0 && reg_code_ < kNumRegisters);
#endif
    return reg_code_;
  }

  inline constexpr bool operator==(
      const RegisterBase<SubType, kAfterLastRegister>& other) const {
    return reg_code_ == other.reg_code_;
  }
  inline constexpr bool operator!=(
      const RegisterBase<SubType, kAfterLastRegister>& other) const {
    return reg_code_ != other.reg_code_;
  }

  // Used to print the name of some special registers.
  static const char* GetSpecialRegisterName(int code) { return "UNKNOWN"; }

 protected:
  explicit constexpr RegisterBase(int code) : reg_code_(code) {}

 private:
  int8_t reg_code_;
  static_assert(kAfterLastRegister <= kMaxInt8);
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
    static_assert(arraysize(Names) == RegType::kNumRegisters);                 \
    return reg.is_valid() ? Names[reg.code()] : "invalid";                     \
  }

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_REGISTER_BASE_H_

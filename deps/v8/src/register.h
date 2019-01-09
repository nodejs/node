// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGISTER_H_
#define V8_REGISTER_H_

#include "src/reglist.h"

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
  // Internal enum class; used for calling constexpr methods, where we need to
  // pass an integral type as template parameter.
  enum class RegisterCode : int { kFirst = 0, kAfterLast = kAfterLastRegister };

 public:
  static constexpr int kCode_no_reg = -1;
  static constexpr int kNumRegisters = kAfterLastRegister;

  static constexpr SubType no_reg() { return SubType{kCode_no_reg}; }

  template <int code>
  static constexpr SubType from_code() {
    static_assert(code >= 0 && code < kNumRegisters, "must be valid reg code");
    return SubType{code};
  }

  constexpr operator RegisterCode() const {
    return static_cast<RegisterCode>(reg_code_);
  }

  template <RegisterCode reg_code>
  static constexpr int code() {
    static_assert(
        reg_code >= RegisterCode::kFirst && reg_code < RegisterCode::kAfterLast,
        "must be valid reg");
    return static_cast<int>(reg_code);
  }

  template <RegisterCode reg_code>
  static constexpr int is_valid() {
    return static_cast<int>(reg_code) != kCode_no_reg;
  }

  template <RegisterCode reg_code>
  static constexpr RegList bit() {
    return is_valid<reg_code>() ? RegList{1} << code<reg_code>() : RegList{};
  }

  static SubType from_code(int code) {
    DCHECK_LE(0, code);
    DCHECK_GT(kNumRegisters, code);
    return SubType{code};
  }

  // Constexpr version (pass registers as template parameters).
  template <RegisterCode... reg_codes>
  static constexpr RegList ListOf() {
    return CombineRegLists(RegisterBase::bit<reg_codes>()...);
  }

  // Non-constexpr version (pass registers as method parameters).
  template <typename... Register>
  static RegList ListOf(Register... regs) {
    return CombineRegLists(regs.bit()...);
  }

  constexpr bool is_valid() const { return reg_code_ != kCode_no_reg; }

  int code() const {
    DCHECK(is_valid());
    return reg_code_;
  }

  RegList bit() const { return is_valid() ? RegList{1} << code() : RegList{}; }

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
#endif  // V8_REGISTER_H_

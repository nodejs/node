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

// Static register clobber and control-flow analysis annotations for Assembler
// and MacroAssembler operations (checked by experimental/regmole).
#if defined(__clang__) && defined(V8_ENABLE_REGMOLE)
// Parameter register role annotations:
// - V8_REG_IN: read on entry, preserved on exit (default for Register/Operand).
// - V8_REG_OUT: not read on entry, holds valid output on exit.
// - V8_REG_INOUT: read on entry, holds valid output on exit.
// - V8_REG_SCRATCH: not read on entry, clobbered on exit.
// - V8_REG_IN_CLOBBER: read on entry, clobbered on exit.
// - V8_REG_RDONLY: read on entry, must not be modified or clobbered in body.
#define V8_REG_IN __attribute__((annotate("v8_reg_in")))
#define V8_REG_OUT __attribute__((annotate("v8_reg_out")))
#define V8_REG_INOUT __attribute__((annotate("v8_reg_inout")))
#define V8_REG_SCRATCH __attribute__((annotate("v8_reg_scratch")))
#define V8_REG_IN_CLOBBER __attribute__((annotate("v8_reg_in_clobber")))
#define V8_REG_RDONLY __attribute__((annotate("v8_reg_rdonly")))

// Function-level implicit physical register annotations
// (V8_REG_CLOBBERS_IF takes a parameter comparison such as `low != 0`):
#define V8_REG_IMPLICIT_IN(...) \
  __attribute__((annotate("v8_reg_implicit_in", __VA_ARGS__)))
#define V8_REG_IMPLICIT_OUT(...) \
  __attribute__((annotate("v8_reg_implicit_out", __VA_ARGS__)))
#define V8_REG_IMPLICIT_INOUT(...) \
  __attribute__((annotate("v8_reg_implicit_inout", __VA_ARGS__)))
#define V8_REG_CLOBBERS(...) \
  __attribute__((annotate("v8_reg_clobbers", __VA_ARGS__)))
#define V8_REG_CLOBBERS_IF(cond, ...) \
  __attribute__((annotate("v8_reg_clobbers_if", #cond, __VA_ARGS__)))
#define V8_REG_CLOBBERS_CALLER_SAVED \
  __attribute__((annotate("v8_reg_clobbers_caller_saved")))

// Generated control-flow and special instruction annotations:
// - V8_ASM_BIND: marks a Label* parameter that binds a branch target.
// - V8_ASM_BRANCH: marks a Label* parameter that is branched to.
// - V8_ASM_NO_FALLTHROUGH: marks an unconditional control transfer (jmp, ret,
//   TailCall, Abort, Trap) where sequential assembly execution does not
//   fall through to the next instruction.
// - V8_ASM_ZEROING_IF_SAME: marks a 2-operand instruction (xor, sub, pxor)
//   that becomes a pure write (V8_REG_OUT) when dst == src.
// - V8_ASM_PUSH / V8_ASM_POP: marks LIFO stack save/restore operations.
#define V8_ASM_BIND __attribute__((annotate("v8_asm_bind")))
#define V8_ASM_BRANCH __attribute__((annotate("v8_asm_branch")))
#define V8_ASM_NO_FALLTHROUGH __attribute__((annotate("v8_asm_no_fallthrough")))
#define V8_ASM_ZEROING_IF_SAME \
  __attribute__((annotate("v8_asm_zeroing_if_same")))
#define V8_ASM_PUSH __attribute__((annotate("v8_asm_push")))
#define V8_ASM_POP __attribute__((annotate("v8_asm_pop")))
#else
#define V8_REG_IN
#define V8_REG_OUT
#define V8_REG_INOUT
#define V8_REG_SCRATCH
#define V8_REG_IN_CLOBBER
#define V8_REG_RDONLY
#define V8_REG_IMPLICIT_IN(...)
#define V8_REG_IMPLICIT_OUT(...)
#define V8_REG_IMPLICIT_INOUT(...)
#define V8_REG_CLOBBERS(...)
#define V8_REG_CLOBBERS_IF(cond, ...)
#define V8_REG_CLOBBERS_CALLER_SAVED
#define V8_ASM_BIND
#define V8_ASM_BRANCH
#define V8_ASM_NO_FALLTHROUGH
#define V8_ASM_ZEROING_IF_SAME
#define V8_ASM_PUSH
#define V8_ASM_POP
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_REGISTER_BASE_H_

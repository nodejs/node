// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_X64_REGISTER_X64_H_
#define V8_CODEGEN_X64_REGISTER_X64_H_

#include "src/codegen/register.h"
#include "src/codegen/reglist.h"

namespace v8 {
namespace internal {

#define GENERAL_REGISTERS(V) \
  V(rax)                     \
  V(rcx)                     \
  V(rdx)                     \
  V(rbx)                     \
  V(rsp)                     \
  V(rbp)                     \
  V(rsi)                     \
  V(rdi)                     \
  V(r8)                      \
  V(r9)                      \
  V(r10)                     \
  V(r11)                     \
  V(r12)                     \
  V(r13)                     \
  V(r14)                     \
  V(r15)

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(rax)                                 \
  V(rbx)                                 \
  V(rdx)                                 \
  V(rcx)                                 \
  V(rsi)                                 \
  V(rdi)                                 \
  V(r8)                                  \
  V(r9)                                  \
  V(r11)                                 \
  V(r12)                                 \
  V(r14)                                 \
  V(r15)

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
  bool is_byte_register() const { return code() <= 3; }
  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const { return code() >> 3; }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const { return code() & 0x7; }

 private:
  friend class RegisterBase<Register, kRegAfterLast>;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(Register);
static_assert(sizeof(Register) == sizeof(int),
              "Register can efficiently be passed by value");

#define DECLARE_REGISTER(R) \
  constexpr Register R = Register::from_code(kRegCode_##R);
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
constexpr Register no_reg = Register::no_reg();

constexpr int kNumRegs = 16;

constexpr RegList kJSCallerSaved =
    Register::ListOf(rax, rcx, rdx,
                     rbx,  // used as a caller-saved register in JavaScript code
                     rdi);  // callee function

constexpr int kNumJSCallerSaved = 5;

// Number of registers for which space is reserved in safepoints.
constexpr int kNumSafepointRegisters = 16;

#ifdef V8_TARGET_OS_WIN
// Windows calling convention
constexpr Register arg_reg_1 = rcx;
constexpr Register arg_reg_2 = rdx;
constexpr Register arg_reg_3 = r8;
constexpr Register arg_reg_4 = r9;
#else
// AMD64 calling convention
constexpr Register arg_reg_1 = rdi;
constexpr Register arg_reg_2 = rsi;
constexpr Register arg_reg_3 = rdx;
constexpr Register arg_reg_4 = rcx;
#endif  // V8_TARGET_OS_WIN

#define DOUBLE_REGISTERS(V) \
  V(xmm0)                   \
  V(xmm1)                   \
  V(xmm2)                   \
  V(xmm3)                   \
  V(xmm4)                   \
  V(xmm5)                   \
  V(xmm6)                   \
  V(xmm7)                   \
  V(xmm8)                   \
  V(xmm9)                   \
  V(xmm10)                  \
  V(xmm11)                  \
  V(xmm12)                  \
  V(xmm13)                  \
  V(xmm14)                  \
  V(xmm15)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

#define ALLOCATABLE_DOUBLE_REGISTERS(V) \
  V(xmm0)                               \
  V(xmm1)                               \
  V(xmm2)                               \
  V(xmm3)                               \
  V(xmm4)                               \
  V(xmm5)                               \
  V(xmm6)                               \
  V(xmm7)                               \
  V(xmm8)                               \
  V(xmm9)                               \
  V(xmm10)                              \
  V(xmm11)                              \
  V(xmm12)                              \
  V(xmm13)                              \
  V(xmm14)

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

class XMMRegister : public RegisterBase<XMMRegister, kDoubleAfterLast> {
 public:
  // Return the high bit of the register code as a 0 or 1.  Used often
  // when constructing the REX prefix byte.
  int high_bit() const { return code() >> 3; }
  // Return the 3 low bits of the register code.  Used when encoding registers
  // in modR/M, SIB, and opcode bytes.
  int low_bits() const { return code() & 0x7; }

 private:
  friend class RegisterBase<XMMRegister, kDoubleAfterLast>;
  explicit constexpr XMMRegister(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(XMMRegister);
static_assert(sizeof(XMMRegister) == sizeof(int),
              "XMMRegister can efficiently be passed by value");

using FloatRegister = XMMRegister;

using DoubleRegister = XMMRegister;

using Simd128Register = XMMRegister;

#define DECLARE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code(kDoubleCode_##R);
DOUBLE_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER
constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

// Define {RegisterName} methods for the register types.
DEFINE_REGISTER_NAMES(Register, GENERAL_REGISTERS)
DEFINE_REGISTER_NAMES(XMMRegister, DOUBLE_REGISTERS)

// Give alias names to registers for calling conventions.
constexpr Register kReturnRegister0 = rax;
constexpr Register kReturnRegister1 = rdx;
constexpr Register kReturnRegister2 = r8;
constexpr Register kJSFunctionRegister = rdi;
constexpr Register kContextRegister = rsi;
constexpr Register kAllocateSizeRegister = rdx;
constexpr Register kSpeculationPoisonRegister = r12;
constexpr Register kInterpreterAccumulatorRegister = rax;
constexpr Register kInterpreterBytecodeOffsetRegister = r9;
constexpr Register kInterpreterBytecodeArrayRegister = r14;
constexpr Register kInterpreterDispatchTableRegister = r15;

constexpr Register kJavaScriptCallArgCountRegister = rax;
constexpr Register kJavaScriptCallCodeStartRegister = rcx;
constexpr Register kJavaScriptCallTargetRegister = kJSFunctionRegister;
constexpr Register kJavaScriptCallNewTargetRegister = rdx;
constexpr Register kJavaScriptCallExtraArg1Register = rbx;

constexpr Register kRuntimeCallFunctionRegister = rbx;
constexpr Register kRuntimeCallArgCountRegister = rax;
constexpr Register kRuntimeCallArgvRegister = r15;
constexpr Register kWasmInstanceRegister = rsi;

// Default scratch register used by MacroAssembler (and other code that needs
// a spare register). The register isn't callee save, and not used by the
// function calling convention.
constexpr Register kScratchRegister = r10;
constexpr XMMRegister kScratchDoubleReg = xmm15;
constexpr Register kRootRegister = r13;  // callee save

constexpr Register kOffHeapTrampolineRegister = kScratchRegister;

constexpr DoubleRegister kFPReturnRegister0 = xmm0;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_X64_REGISTER_X64_H_

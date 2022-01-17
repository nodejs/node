// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_LOONG64_REGISTER_LOONG64_H_
#define V8_CODEGEN_LOONG64_REGISTER_LOONG64_H_

#include "src/codegen/loong64/constants-loong64.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(zero_reg)   V(ra)  V(tp)  V(sp) \
  V(a0)  V(a1)  V(a2)  V(a3) V(a4)  V(a5)  V(a6)  V(a7)  \
  V(t0)  V(t1)  V(t2)  V(t3) V(t4)  V(t5)  V(t6)  V(t7)  V(t8) \
  V(x_reg)      V(fp)  \
  V(s0)  V(s1)  V(s2)  V(s3)  V(s4)  V(s5)  V(s6)  V(s7)  V(s8) \

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(a0)  V(a1)  V(a2)  V(a3)  V(a4)  V(a5)  V(a6)  V(a7) \
  V(t0)  V(t1)  V(t2)  V(t3)  V(t4)  V(t5)               \
  V(s0)  V(s1)  V(s2)  V(s3)  V(s4)  V(s5)  V(s7)  V(s8)

#define DOUBLE_REGISTERS(V)                               \
  V(f0)  V(f1)  V(f2)  V(f3)  V(f4)  V(f5)  V(f6)  V(f7)  \
  V(f8)  V(f9)  V(f10) V(f11) V(f12) V(f13) V(f14) V(f15) \
  V(f16) V(f17) V(f18) V(f19) V(f20) V(f21) V(f22) V(f23) \
  V(f24) V(f25) V(f26) V(f27) V(f28) V(f29) V(f30) V(f31)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS(V)                              \
  V(w0)  V(w1)  V(w2)  V(w3)  V(w4)  V(w5)  V(w6)  V(w7)  \
  V(w8)  V(w9)  V(w10) V(w11) V(w12) V(w13) V(w14) V(w15) \
  V(w16) V(w17) V(w18) V(w19) V(w20) V(w21) V(w22) V(w23) \
  V(w24) V(w25) V(w26) V(w27) V(w28) V(w29) V(w30) V(w31)

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                   \
  V(f0)  V(f1)  V(f2)  V(f3)  V(f4)  V(f5) V(f6) V(f7) \
  V(f8) V(f9) V(f10) V(f11) V(f12) V(f13) V(f14) V(f15) V(f16) \
  V(f17) V(f18) V(f19) V(f20) V(f21) V(f22) V(f23)
// clang-format on

// Note that the bit values must match those used in actual instruction
// encoding.
const int kNumRegs = 32;

const RegList kJSCallerSaved = 1 << 4 |   // a0
                               1 << 5 |   // a1
                               1 << 6 |   // a2
                               1 << 7 |   // a3
                               1 << 8 |   // a4
                               1 << 9 |   // a5
                               1 << 10 |  // a6
                               1 << 11 |  // a7
                               1 << 12 |  // t0
                               1 << 13 |  // t1
                               1 << 14 |  // t2
                               1 << 15 |  // t3
                               1 << 16 |  // t4
                               1 << 17 |  // t5
                               1 << 20;   // t8

const int kNumJSCallerSaved = 15;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = 1 << 22 |  // fp
                             1 << 23 |  // s0
                             1 << 24 |  // s1
                             1 << 25 |  // s2
                             1 << 26 |  // s3
                             1 << 27 |  // s4
                             1 << 28 |  // s5
                             1 << 29 |  // s6 (roots in Javascript code)
                             1 << 30 |  // s7 (cp in Javascript code)
                             1 << 31;   // s8

const int kNumCalleeSaved = 10;

const RegList kCalleeSavedFPU = 1 << 24 |  // f24
                                1 << 25 |  // f25
                                1 << 26 |  // f26
                                1 << 27 |  // f27
                                1 << 28 |  // f28
                                1 << 29 |  // f29
                                1 << 30 |  // f30
                                1 << 31;   // f31

const int kNumCalleeSavedFPU = 8;

const RegList kCallerSavedFPU = 1 << 0 |   // f0
                                1 << 1 |   // f1
                                1 << 2 |   // f2
                                1 << 3 |   // f3
                                1 << 4 |   // f4
                                1 << 5 |   // f5
                                1 << 6 |   // f6
                                1 << 7 |   // f7
                                1 << 8 |   // f8
                                1 << 9 |   // f9
                                1 << 10 |  // f10
                                1 << 11 |  // f11
                                1 << 12 |  // f12
                                1 << 13 |  // f13
                                1 << 14 |  // f14
                                1 << 15 |  // f15
                                1 << 16 |  // f16
                                1 << 17 |  // f17
                                1 << 18 |  // f18
                                1 << 19 |  // f19
                                1 << 20 |  // f20
                                1 << 21 |  // f21
                                1 << 22 |  // f22
                                1 << 23;   // f23

// CPU Registers.
//
// 1) We would prefer to use an enum, but enum values are assignment-
// compatible with int, which has caused code-generation bugs.
//
// 2) We would prefer to use a class instead of a struct but we don't like
// the register initialization to depend on the particular initialization
// order (which appears to be different on OS X, Linux, and Windows for the
// installed versions of C++ we tried). Using a struct permits C-style
// "initialization". Also, the Register objects cannot be const as this
// forces initialization stubs in MSVC, making us dependent on initialization
// order.
//
// 3) By not using an enum, we are possibly preventing the compiler from
// doing certain constant folds, which may significantly reduce the
// code generated for some assembly instructions (because they boil down
// to a few constants). If this is a problem, we could change the code
// such that we use an enum in optimized mode, and the struct in debug
// mode. This way we get the compile-time error checking in debug mode
// and best performance in optimized code.

// -----------------------------------------------------------------------------
// Implementation of Register and FPURegister.

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
  static constexpr int kMantissaOffset = 0;
  static constexpr int kExponentOffset = 4;

 private:
  friend class RegisterBase;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

// s7: context register
// s3: scratch register
// s4: scratch register 2
#define DECLARE_REGISTER(R) \
  constexpr Register R = Register::from_code(kRegCode_##R);
GENERAL_REGISTERS(DECLARE_REGISTER)
#undef DECLARE_REGISTER

constexpr Register no_reg = Register::no_reg();

int ToNumber(Register reg);

Register ToRegister(int num);

// Returns the number of padding slots needed for stack pointer alignment.
constexpr int ArgumentPaddingSlots(int argument_count) {
  // No argument padding required.
  return 0;
}

constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

// FPURegister register.
class FPURegister : public RegisterBase<FPURegister, kDoubleAfterLast> {
 public:
  FPURegister low() const { return FPURegister::from_code(code()); }

 private:
  friend class RegisterBase;
  explicit constexpr FPURegister(int code) : RegisterBase(code) {}
};

// Condition Flag Register
enum CFRegister { FCC0, FCC1, FCC2, FCC3, FCC4, FCC5, FCC6, FCC7 };

using FloatRegister = FPURegister;

using DoubleRegister = FPURegister;

using Simd128Register = FPURegister;

#define DECLARE_DOUBLE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code(kDoubleCode_##R);
DOUBLE_REGISTERS(DECLARE_DOUBLE_REGISTER)
#undef DECLARE_DOUBLE_REGISTER

constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

// Register aliases.
// cp is assumed to be a callee saved register.
constexpr Register kRootRegister = s6;
constexpr Register cp = s7;
constexpr Register kScratchReg = s3;
constexpr Register kScratchReg2 = s4;
constexpr DoubleRegister kScratchDoubleReg = f30;
constexpr DoubleRegister kScratchDoubleReg1 = f30;
constexpr DoubleRegister kScratchDoubleReg2 = f31;
// FPU zero reg is often used to hold 0.0, but it's not hardwired to 0.0.
constexpr DoubleRegister kDoubleRegZero = f29;

struct FPUControlRegister {
  bool is_valid() const { return (reg_code >> 2) == 0; }
  bool is(FPUControlRegister creg) const { return reg_code == creg.reg_code; }
  int code() const {
    DCHECK(is_valid());
    return reg_code;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << reg_code;
  }
  void setcode(int f) {
    reg_code = f;
    DCHECK(is_valid());
  }
  // Unfortunately we can't make this private in a struct.
  int reg_code;
};

constexpr FPUControlRegister no_fpucreg = {kInvalidFPUControlRegister};
constexpr FPUControlRegister FCSR = {kFCSRRegister};
constexpr FPUControlRegister FCSR0 = {kFCSRRegister};
constexpr FPUControlRegister FCSR1 = {kFCSRRegister + 1};
constexpr FPUControlRegister FCSR2 = {kFCSRRegister + 2};
constexpr FPUControlRegister FCSR3 = {kFCSRRegister + 3};

// Define {RegisterName} methods for the register types.
DEFINE_REGISTER_NAMES(Register, GENERAL_REGISTERS)
DEFINE_REGISTER_NAMES(FPURegister, DOUBLE_REGISTERS)

// Give alias names to registers for calling conventions.
constexpr Register kReturnRegister0 = a0;
constexpr Register kReturnRegister1 = a1;
constexpr Register kReturnRegister2 = a2;
constexpr Register kJSFunctionRegister = a1;
constexpr Register kContextRegister = s7;
constexpr Register kAllocateSizeRegister = a0;
constexpr Register kInterpreterAccumulatorRegister = a0;
constexpr Register kInterpreterBytecodeOffsetRegister = t0;
constexpr Register kInterpreterBytecodeArrayRegister = t1;
constexpr Register kInterpreterDispatchTableRegister = t2;

constexpr Register kJavaScriptCallArgCountRegister = a0;
constexpr Register kJavaScriptCallCodeStartRegister = a2;
constexpr Register kJavaScriptCallTargetRegister = kJSFunctionRegister;
constexpr Register kJavaScriptCallNewTargetRegister = a3;
constexpr Register kJavaScriptCallExtraArg1Register = a2;

constexpr Register kOffHeapTrampolineRegister = t7;
constexpr Register kRuntimeCallFunctionRegister = a1;
constexpr Register kRuntimeCallArgCountRegister = a0;
constexpr Register kRuntimeCallArgvRegister = a2;
constexpr Register kWasmInstanceRegister = a0;
constexpr Register kWasmCompileLazyFuncIndexRegister = t0;

constexpr DoubleRegister kFPReturnRegister0 = f0;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_LOONG64_REGISTER_LOONG64_H_

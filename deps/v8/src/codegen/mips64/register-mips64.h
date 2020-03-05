// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_MIPS64_REGISTER_MIPS64_H_
#define V8_CODEGEN_MIPS64_REGISTER_MIPS64_H_

#include "src/codegen/mips64/constants-mips64.h"
#include "src/codegen/register.h"
#include "src/codegen/reglist.h"

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(zero_reg)  V(at)  V(v0)  V(v1)  V(a0)  V(a1)  V(a2)  V(a3)  \
  V(a4)  V(a5)  V(a6)  V(a7)  V(t0)  V(t1)  V(t2)  V(t3)  \
  V(s0)  V(s1)  V(s2)  V(s3)  V(s4)  V(s5)  V(s6)  V(s7)  V(t8)  V(t9) \
  V(k0)  V(k1)  V(gp)  V(sp)  V(fp)  V(ra)

#define ALLOCATABLE_GENERAL_REGISTERS(V) \
  V(a0)  V(a1)  V(a2)  V(a3) \
  V(a4)  V(a5)  V(a6)  V(a7)  V(t0)  V(t1)  V(t2) V(s7) \
  V(v0)  V(v1)

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
  V(f0)  V(f2)  V(f4)  V(f6)  V(f8)  V(f10) V(f12) V(f14) \
  V(f16) V(f18) V(f20) V(f22) V(f24) V(f26)
// clang-format on

// Note that the bit values must match those used in actual instruction
// encoding.
const int kNumRegs = 32;

const RegList kJSCallerSaved = 1 << 2 |   // v0
                               1 << 3 |   // v1
                               1 << 4 |   // a0
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
                               1 << 15;   // t3

const int kNumJSCallerSaved = 14;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = 1 << 16 |  // s0
                             1 << 17 |  // s1
                             1 << 18 |  // s2
                             1 << 19 |  // s3
                             1 << 20 |  // s4
                             1 << 21 |  // s5
                             1 << 22 |  // s6 (roots in Javascript code)
                             1 << 23 |  // s7 (cp in Javascript code)
                             1 << 30;   // fp/s8

const int kNumCalleeSaved = 9;

const RegList kCalleeSavedFPU = 1 << 20 |  // f20
                                1 << 22 |  // f22
                                1 << 24 |  // f24
                                1 << 26 |  // f26
                                1 << 28 |  // f28
                                1 << 30;   // f30

const int kNumCalleeSavedFPU = 6;

const RegList kCallerSavedFPU = 1 << 0 |   // f0
                                1 << 2 |   // f2
                                1 << 4 |   // f4
                                1 << 6 |   // f6
                                1 << 8 |   // f8
                                1 << 10 |  // f10
                                1 << 12 |  // f12
                                1 << 14 |  // f14
                                1 << 16 |  // f16
                                1 << 18;   // f18

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
const int kNumSafepointRegisters = 24;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
const int kNumSafepointSavedRegisters = kNumJSCallerSaved + kNumCalleeSaved;

const int kUndefIndex = -1;
// Map with indexes on stack that corresponds to codes of saved registers.
const int kSafepointRegisterStackIndexMap[kNumRegs] = {kUndefIndex,  // zero_reg
                                                       kUndefIndex,  // at
                                                       0,            // v0
                                                       1,            // v1
                                                       2,            // a0
                                                       3,            // a1
                                                       4,            // a2
                                                       5,            // a3
                                                       6,            // a4
                                                       7,            // a5
                                                       8,            // a6
                                                       9,            // a7
                                                       10,           // t0
                                                       11,           // t1
                                                       12,           // t2
                                                       13,           // t3
                                                       14,           // s0
                                                       15,           // s1
                                                       16,           // s2
                                                       17,           // s3
                                                       18,           // s4
                                                       19,           // s5
                                                       20,           // s6
                                                       21,           // s7
                                                       kUndefIndex,  // t8
                                                       kUndefIndex,  // t9
                                                       kUndefIndex,  // k0
                                                       kUndefIndex,  // k1
                                                       kUndefIndex,  // gp
                                                       kUndefIndex,  // sp
                                                       22,           // fp
                                                       kUndefIndex};

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
#if defined(V8_TARGET_LITTLE_ENDIAN)
  static constexpr int kMantissaOffset = 0;
  static constexpr int kExponentOffset = 4;
#elif defined(V8_TARGET_BIG_ENDIAN)
  static constexpr int kMantissaOffset = 4;
  static constexpr int kExponentOffset = 0;
#else
#error Unknown endianness
#endif

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

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

// Coprocessor register.
class FPURegister : public RegisterBase<FPURegister, kDoubleAfterLast> {
 public:
  // TODO(plind): Warning, inconsistent numbering here. kNumFPURegisters refers
  // to number of 32-bit FPU regs, but kNumAllocatableRegisters refers to
  // number of Double regs (64-bit regs, or FPU-reg-pairs).

  FPURegister low() const {
    // TODO(plind): Create DCHECK for FR=0 mode. This usage suspect for FR=1.
    // Find low reg of a Double-reg pair, which is the reg itself.
    DCHECK_EQ(code() % 2, 0);  // Specified Double reg must be even.
    return FPURegister::from_code(code());
  }
  FPURegister high() const {
    // TODO(plind): Create DCHECK for FR=0 mode. This usage illegal in FR=1.
    // Find high reg of a Doubel-reg pair, which is reg + 1.
    DCHECK_EQ(code() % 2, 0);  // Specified Double reg must be even.
    return FPURegister::from_code(code() + 1);
  }

 private:
  friend class RegisterBase;
  explicit constexpr FPURegister(int code) : RegisterBase(code) {}
};

enum MSARegisterCode {
#define REGISTER_CODE(R) kMsaCode_##R,
  SIMD128_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kMsaAfterLast
};

// MIPS SIMD (MSA) register
class MSARegister : public RegisterBase<MSARegister, kMsaAfterLast> {
  friend class RegisterBase;
  explicit constexpr MSARegister(int code) : RegisterBase(code) {}
};

// A few double registers are reserved: one as a scratch register and one to
// hold 0.0.
//  f28: 0.0
//  f30: scratch register.

// V8 now supports the O32 ABI, and the FPU Registers are organized as 32
// 32-bit registers, f0 through f31. When used as 'double' they are used
// in pairs, starting with the even numbered register. So a double operation
// on f0 really uses f0 and f1.
// (Modern mips hardware also supports 32 64-bit registers, via setting
// (privileged) Status Register FR bit to 1. This is used by the N32 ABI,
// but it is not in common use. Someday we will want to support this in v8.)

// For O32 ABI, Floats and Doubles refer to same set of 32 32-bit registers.
using FloatRegister = FPURegister;

using DoubleRegister = FPURegister;

#define DECLARE_DOUBLE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code(kDoubleCode_##R);
DOUBLE_REGISTERS(DECLARE_DOUBLE_REGISTER)
#undef DECLARE_DOUBLE_REGISTER

constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

// SIMD registers.
using Simd128Register = MSARegister;

#define DECLARE_SIMD128_REGISTER(R) \
  constexpr Simd128Register R = Simd128Register::from_code(kMsaCode_##R);
SIMD128_REGISTERS(DECLARE_SIMD128_REGISTER)
#undef DECLARE_SIMD128_REGISTER

const Simd128Register no_msareg = Simd128Register::no_reg();

// Register aliases.
// cp is assumed to be a callee saved register.
constexpr Register kRootRegister = s6;
constexpr Register cp = s7;
constexpr Register kScratchReg = s3;
constexpr Register kScratchReg2 = s4;
constexpr DoubleRegister kScratchDoubleReg = f30;
constexpr DoubleRegister kDoubleRegZero = f28;
// Used on mips64r6 for compare operations.
// We use the last non-callee saved odd register for N64 ABI
constexpr DoubleRegister kDoubleCompareReg = f23;
// MSA zero and scratch regs must have the same numbers as FPU zero and scratch
constexpr Simd128Register kSimd128RegZero = w28;
constexpr Simd128Register kSimd128ScratchReg = w30;

// FPU (coprocessor 1) control registers.
// Currently only FCSR (#31) is implemented.
struct FPUControlRegister {
  bool is_valid() const { return reg_code == kFCSRRegister; }
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

// MSA control registers
struct MSAControlRegister {
  bool is_valid() const {
    return (reg_code == kMSAIRRegister) || (reg_code == kMSACSRRegister);
  }
  bool is(MSAControlRegister creg) const { return reg_code == creg.reg_code; }
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

constexpr MSAControlRegister no_msacreg = {kInvalidMSAControlRegister};
constexpr MSAControlRegister MSAIR = {kMSAIRRegister};
constexpr MSAControlRegister MSACSR = {kMSACSRRegister};

// Define {RegisterName} methods for the register types.
DEFINE_REGISTER_NAMES(Register, GENERAL_REGISTERS)
DEFINE_REGISTER_NAMES(FPURegister, DOUBLE_REGISTERS)
DEFINE_REGISTER_NAMES(MSARegister, SIMD128_REGISTERS)

// Give alias names to registers for calling conventions.
constexpr Register kReturnRegister0 = v0;
constexpr Register kReturnRegister1 = v1;
constexpr Register kReturnRegister2 = a0;
constexpr Register kJSFunctionRegister = a1;
constexpr Register kContextRegister = s7;
constexpr Register kAllocateSizeRegister = a0;
constexpr Register kSpeculationPoisonRegister = a7;
constexpr Register kInterpreterAccumulatorRegister = v0;
constexpr Register kInterpreterBytecodeOffsetRegister = t0;
constexpr Register kInterpreterBytecodeArrayRegister = t1;
constexpr Register kInterpreterDispatchTableRegister = t2;

constexpr Register kJavaScriptCallArgCountRegister = a0;
constexpr Register kJavaScriptCallCodeStartRegister = a2;
constexpr Register kJavaScriptCallTargetRegister = kJSFunctionRegister;
constexpr Register kJavaScriptCallNewTargetRegister = a3;
constexpr Register kJavaScriptCallExtraArg1Register = a2;

constexpr Register kOffHeapTrampolineRegister = at;
constexpr Register kRuntimeCallFunctionRegister = a1;
constexpr Register kRuntimeCallArgCountRegister = a0;
constexpr Register kRuntimeCallArgvRegister = a2;
constexpr Register kWasmInstanceRegister = a0;
constexpr Register kWasmCompileLazyFuncIndexRegister = t0;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_MIPS64_REGISTER_MIPS64_H_

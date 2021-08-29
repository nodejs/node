// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV64_REGISTER_RISCV64_H_
#define V8_CODEGEN_RISCV64_REGISTER_RISCV64_H_

#include "src/codegen/register.h"
#include "src/codegen/reglist.h"
#include "src/codegen/riscv64/constants-riscv64.h"

namespace v8 {
namespace internal {

// clang-format off

#define GENERAL_REGISTERS(V)                                            \
  V(zero_reg)  V(ra)  V(sp)  V(gp)  V(tp)  V(t0)  V(t1)  V(t2)          \
  V(fp)  V(s1)  V(a0)  V(a1)  V(a2)  V(a3)  V(a4)  V(a5)                \
  V(a6)  V(a7)  V(s2)  V(s3)  V(s4)  V(s5)  V(s6)  V(s7)  V(s8)  V(s9)  \
  V(s10)  V(s11)  V(t3)  V(t4)  V(t5)  V(t6)

// s3: scratch register s4: scratch register 2  used in code-generator-riscv64
// s6: roots in Javascript code s7: context register
// s11: PtrComprCageBaseRegister
// t3 t5 s10 : scratch register used in scratch_register_list

// t0 t1 t2 t4:caller saved scratch register can be used in macroassembler and
// builtin-riscv64
#define ALWAYS_ALLOCATABLE_GENERAL_REGISTERS(V)  \
             V(a0)  V(a1)  V(a2)  V(a3) \
             V(a4)  V(a5)  V(a6)  V(a7)  V(t0)  \
             V(t1)  V(t2)  V(t4)  V(s7)  V(s8) V(s9)

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
#define MAYBE_ALLOCATABLE_GENERAL_REGISTERS(V)
#else
#define MAYBE_ALLOCATABLE_GENERAL_REGISTERS(V) V(s11)
#endif

#define ALLOCATABLE_GENERAL_REGISTERS(V)  \
  ALWAYS_ALLOCATABLE_GENERAL_REGISTERS(V) \
  MAYBE_ALLOCATABLE_GENERAL_REGISTERS(V)

#define DOUBLE_REGISTERS(V)                                       \
  V(ft0)  V(ft1)  V(ft2)  V(ft3)  V(ft4)  V(ft5)  V(ft6)  V(ft7)  \
  V(fs0)  V(fs1)  V(fa0) V(fa1) V(fa2) V(fa3) V(fa4) V(fa5)       \
  V(fa6) V(fa7) V(fs2) V(fs3) V(fs4) V(fs5) V(fs6) V(fs7)         \
  V(fs8) V(fs9) V(fs10) V(fs11) V(ft8) V(ft9) V(ft10) V(ft11)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS(V)                               \
  V(w0)  V(w1)  V(w2)  V(w3)  V(w4)  V(w5)  V(w6)  V(w7)   \
  V(w8)  V(w9)  V(w10) V(w11) V(w12) V(w13) V(w14) V(w15)  \
  V(w16) V(w17) V(w18) V(w19) V(w20) V(w21) V(w22) V(w23)  \
  V(w24) V(w25) V(w26) V(w27) V(w28) V(w29) V(w30) V(w31)

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                                   \
  V(ft0)  V(ft1)  V(ft2) V(ft3)                                           \
  V(ft4)  V(ft5) V(ft6) V(ft7) V(fa0) V(fa1) V(fa2) V(fa3) V(fa4) V(fa5)  \
  V(fa6) V(fa7)

// Returns the number of padding slots needed for stack pointer alignment.
constexpr int ArgumentPaddingSlots(int argument_count) {
  // No argument padding required.
  return 0;
}

// clang-format on

// Note that the bit values must match those used in actual instruction
// encoding.
const int kNumRegs = 32;

const RegList kJSCallerSaved = 1 << 5 |   // t0
                               1 << 6 |   // t1
                               1 << 7 |   // t2
                               1 << 10 |  // a0
                               1 << 11 |  // a1
                               1 << 12 |  // a2
                               1 << 13 |  // a3
                               1 << 14 |  // a4
                               1 << 15 |  // a5
                               1 << 16 |  // a6
                               1 << 17 |  // a7
                               1 << 29;   // t4

const int kNumJSCallerSaved = 12;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = 1 << 8 |   // fp/s0
                             1 << 9 |   // s1
                             1 << 18 |  // s2
                             1 << 19 |  // s3 scratch register
                             1 << 20 |  // s4 scratch register 2
                             1 << 21 |  // s5
                             1 << 22 |  // s6 (roots in Javascript code)
                             1 << 23 |  // s7 (cp in Javascript code)
                             1 << 24 |  // s8
                             1 << 25 |  // s9
                             1 << 26 |  // s10
                             1 << 27;   // s11

const int kNumCalleeSaved = 12;

const RegList kCalleeSavedFPU = 1 << 8 |   // fs0
                                1 << 9 |   // fs1
                                1 << 18 |  // fs2
                                1 << 19 |  // fs3
                                1 << 20 |  // fs4
                                1 << 21 |  // fs5
                                1 << 22 |  // fs6
                                1 << 23 |  // fs7
                                1 << 24 |  // fs8
                                1 << 25 |  // fs9
                                1 << 26 |  // fs10
                                1 << 27;   // fs11

const int kNumCalleeSavedFPU = 12;

const RegList kCallerSavedFPU = 1 << 0 |   // ft0
                                1 << 1 |   // ft1
                                1 << 2 |   // ft2
                                1 << 3 |   // ft3
                                1 << 4 |   // ft4
                                1 << 5 |   // ft5
                                1 << 6 |   // ft6
                                1 << 7 |   // ft7
                                1 << 10 |  // fa0
                                1 << 11 |  // fa1
                                1 << 12 |  // fa2
                                1 << 13 |  // fa3
                                1 << 14 |  // fa4
                                1 << 15 |  // fa5
                                1 << 16 |  // fa6
                                1 << 17 |  // fa7
                                1 << 28 |  // ft8
                                1 << 29 |  // ft9
                                1 << 30 |  // ft10
                                1 << 31;   // ft11

// Number of registers for which space is reserved in safepoints. Must be a
// multiple of 8.
const int kNumSafepointRegisters = 32;

// Define the list of registers actually saved at safepoints.
// Note that the number of saved registers may be smaller than the reserved
// space, i.e. kNumSafepointSavedRegisters <= kNumSafepointRegisters.
const RegList kSafepointSavedRegisters = kJSCallerSaved | kCalleeSaved;
const int kNumSafepointSavedRegisters = kNumJSCallerSaved + kNumCalleeSaved;

const int kUndefIndex = -1;
// Map with indexes on stack that corresponds to codes of saved registers.
const int kSafepointRegisterStackIndexMap[kNumRegs] = {kUndefIndex,  // zero_reg
                                                       kUndefIndex,  // ra
                                                       kUndefIndex,  // sp
                                                       kUndefIndex,  // gp
                                                       kUndefIndex,  // tp
                                                       0,            // t0
                                                       1,            // t1
                                                       2,            // t2
                                                       3,            // s0/fp
                                                       4,            // s1
                                                       5,            // a0
                                                       6,            // a1
                                                       7,            // a2
                                                       8,            // a3
                                                       9,            // a4
                                                       10,           // a5
                                                       11,           // a6
                                                       12,           // a7
                                                       13,           // s2
                                                       14,           // s3
                                                       15,           // s4
                                                       16,           // s5
                                                       17,           // s6
                                                       18,           // s7
                                                       19,           // s8
                                                       10,           // s9
                                                       21,           // s10
                                                       22,           // s11
                                                       kUndefIndex,  // t3
                                                       23,           // t4
                                                       kUndefIndex,  // t5
                                                       kUndefIndex};  // t6
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
    return FPURegister::from_code(code());
  }
  FPURegister high() const {
    // TODO(plind): Create DCHECK for FR=0 mode. This usage illegal in FR=1.
    // Find high reg of a Doubel-reg pair, which is reg + 1.
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
// TODO(RISCV): Remove MIPS MSA registers.
//              https://github.com/v8-riscv/v8/issues/429
class MSARegister : public RegisterBase<MSARegister, kMsaAfterLast> {
  friend class RegisterBase;
  explicit constexpr MSARegister(int code) : RegisterBase(code) {}
};

// A few double registers are reserved: one as a scratch register and one to
//  hold 0.0.
//  fs9: 0.0
//  fs11: scratch register.

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

constexpr DoubleRegister kScratchDoubleReg = fs11;

constexpr DoubleRegister kDoubleRegZero = fs9;

// Define {RegisterName} methods for the register types.
DEFINE_REGISTER_NAMES(Register, GENERAL_REGISTERS)
DEFINE_REGISTER_NAMES(FPURegister, DOUBLE_REGISTERS)
DEFINE_REGISTER_NAMES(MSARegister, SIMD128_REGISTERS)

// Give alias names to registers for calling conventions.
constexpr Register kReturnRegister0 = a0;
constexpr Register kReturnRegister1 = a1;
constexpr Register kReturnRegister2 = a2;
constexpr Register kJSFunctionRegister = a1;
constexpr Register kContextRegister = s7;
constexpr Register kAllocateSizeRegister = a1;
constexpr Register kSpeculationPoisonRegister = a7;
constexpr Register kInterpreterAccumulatorRegister = a0;
constexpr Register kInterpreterBytecodeOffsetRegister = t0;
constexpr Register kInterpreterBytecodeArrayRegister = t1;
constexpr Register kInterpreterDispatchTableRegister = t2;

constexpr Register kJavaScriptCallArgCountRegister = a0;
constexpr Register kJavaScriptCallCodeStartRegister = a2;
constexpr Register kJavaScriptCallTargetRegister = kJSFunctionRegister;
constexpr Register kJavaScriptCallNewTargetRegister = a3;
constexpr Register kJavaScriptCallExtraArg1Register = a2;

constexpr Register kOffHeapTrampolineRegister = t6;
constexpr Register kRuntimeCallFunctionRegister = a1;
constexpr Register kRuntimeCallArgCountRegister = a0;
constexpr Register kRuntimeCallArgvRegister = a2;
constexpr Register kWasmInstanceRegister = a0;
constexpr Register kWasmCompileLazyFuncIndexRegister = t0;

constexpr DoubleRegister kFPReturnRegister0 = fa0;

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
constexpr Register kPtrComprCageBaseRegister = s11;  // callee save
#else
constexpr Register kPtrComprCageBaseRegister = kRootRegister;
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV64_REGISTER_RISCV64_H_

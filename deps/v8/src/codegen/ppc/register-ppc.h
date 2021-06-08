// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_PPC_REGISTER_PPC_H_
#define V8_CODEGEN_PPC_REGISTER_PPC_H_

#include "src/codegen/register.h"
#include "src/codegen/reglist.h"

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(r0)  V(sp)  V(r2)  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)  \
  V(r8)  V(r9)  V(r10) V(r11) V(ip) V(r13) V(r14) V(r15)  \
  V(r16) V(r17) V(r18) V(r19) V(r20) V(r21) V(r22) V(r23) \
  V(r24) V(r25) V(r26) V(r27) V(r28) V(r29) V(r30) V(fp)

#if V8_EMBEDDED_CONSTANT_POOL
#define ALLOCATABLE_GENERAL_REGISTERS(V)                  \
  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)                       \
  V(r8)  V(r9)  V(r10) V(r14) V(r15)                      \
  V(r16) V(r17) V(r18) V(r19) V(r20) V(r21) V(r22) V(r23) \
  V(r24) V(r25) V(r26) V(r27) V(r30)
#else
#define ALLOCATABLE_GENERAL_REGISTERS(V)                  \
  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)                       \
  V(r8)  V(r9)  V(r10) V(r14) V(r15)                      \
  V(r16) V(r17) V(r18) V(r19) V(r20) V(r21) V(r22) V(r23) \
  V(r24) V(r25) V(r26) V(r27) V(r28) V(r30)
#endif

#define LOW_DOUBLE_REGISTERS(V)                           \
  V(d0)  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)  \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d13) V(d14) V(d15)

#define NON_LOW_DOUBLE_REGISTERS(V)                       \
  V(d16) V(d17) V(d18) V(d19) V(d20) V(d21) V(d22) V(d23) \
  V(d24) V(d25) V(d26) V(d27) V(d28) V(d29) V(d30) V(d31)

#define DOUBLE_REGISTERS(V) \
  LOW_DOUBLE_REGISTERS(V) NON_LOW_DOUBLE_REGISTERS(V)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS(V)                              \
  V(v0)  V(v1)  V(v2)  V(v3)  V(v4)  V(v5)  V(v6)  V(v7)  \
  V(v8)  V(v9)  V(v10) V(v11) V(v12) V(v13) V(v14) V(v15) \
  V(v16) V(v17) V(v18) V(v19) V(v20) V(v21) V(v22) V(v23) \
  V(v24) V(v25) V(v26) V(v27) V(v28) V(v29) V(v30) V(v31)

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                   \
  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)         \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d15)               \
  V(d16) V(d17) V(d18) V(d19) V(d20) V(d21) V(d22) V(d23) \
  V(d24) V(d25) V(d26) V(d27) V(d28) V(d29) V(d30) V(d31)

#define C_REGISTERS(V)                                            \
  V(cr0)  V(cr1)  V(cr2)  V(cr3)  V(cr4)  V(cr5)  V(cr6)  V(cr7)  \
  V(cr8)  V(cr9)  V(cr10) V(cr11) V(cr12) V(cr15)
// clang-format on

// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding

// Caller-saved/arguments registers
const RegList kJSCallerSaved = 1 << 3 |   // r3  a1
                               1 << 4 |   // r4  a2
                               1 << 5 |   // r5  a3
                               1 << 6 |   // r6  a4
                               1 << 7 |   // r7  a5
                               1 << 8 |   // r8  a6
                               1 << 9 |   // r9  a7
                               1 << 10 |  // r10 a8
                               1 << 11;

const int kNumJSCallerSaved = 9;

// Return the code of the n-th caller-saved register available to JavaScript
// e.g. JSCallerSavedReg(0) returns r0.code() == 0
int JSCallerSavedCode(int n);

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved = 1 << 14 |  // r14
                             1 << 15 |  // r15
                             1 << 16 |  // r16
                             1 << 17 |  // r17
                             1 << 18 |  // r18
                             1 << 19 |  // r19
                             1 << 20 |  // r20
                             1 << 21 |  // r21
                             1 << 22 |  // r22
                             1 << 23 |  // r23
                             1 << 24 |  // r24
                             1 << 25 |  // r25
                             1 << 26 |  // r26
                             1 << 27 |  // r27
                             1 << 28 |  // r28
                             1 << 29 |  // r29
                             1 << 30 |  // r20
                             1 << 31;   // r31

const int kNumCalleeSaved = 18;

const RegList kCallerSavedDoubles = 1 << 0 |   // d0
                                    1 << 1 |   // d1
                                    1 << 2 |   // d2
                                    1 << 3 |   // d3
                                    1 << 4 |   // d4
                                    1 << 5 |   // d5
                                    1 << 6 |   // d6
                                    1 << 7 |   // d7
                                    1 << 8 |   // d8
                                    1 << 9 |   // d9
                                    1 << 10 |  // d10
                                    1 << 11 |  // d11
                                    1 << 12 |  // d12
                                    1 << 13;   // d13

const int kNumCallerSavedDoubles = 14;

const RegList kCalleeSavedDoubles = 1 << 14 |  // d14
                                    1 << 15 |  // d15
                                    1 << 16 |  // d16
                                    1 << 17 |  // d17
                                    1 << 18 |  // d18
                                    1 << 19 |  // d19
                                    1 << 20 |  // d20
                                    1 << 21 |  // d21
                                    1 << 22 |  // d22
                                    1 << 23 |  // d23
                                    1 << 24 |  // d24
                                    1 << 25 |  // d25
                                    1 << 26 |  // d26
                                    1 << 27 |  // d27
                                    1 << 28 |  // d28
                                    1 << 29 |  // d29
                                    1 << 30 |  // d30
                                    1 << 31;   // d31

const int kNumCalleeSavedDoubles = 18;

// The following constants describe the stack frame linkage area as
// defined by the ABI.  Note that kNumRequiredStackFrameSlots must
// satisfy alignment requirements (rounding up if required).
#if V8_TARGET_ARCH_PPC64 &&     \
    (V8_TARGET_LITTLE_ENDIAN || \
     (defined(_CALL_ELF) && _CALL_ELF == 2))  // ELFv2 ABI
// [0] back chain
// [1] condition register save area
// [2] link register save area
// [3] TOC save area
// [4] Parameter1 save area
// ...
// [11] Parameter8 save area
// [12] Parameter9 slot (if necessary)
// ...
const int kNumRequiredStackFrameSlots = 12;
const int kStackFrameLRSlot = 2;
const int kStackFrameExtraParamSlot = 12;
#else  // AIX
// [0] back chain
// [1] condition register save area
// [2] link register save area
// [3] reserved for compiler
// [4] reserved by binder
// [5] TOC save area
// [6] Parameter1 save area
// ...
// [13] Parameter8 save area
// [14] Parameter9 slot (if necessary)
// ...
const int kNumRequiredStackFrameSlots = 14;
const int kStackFrameLRSlot = 2;
const int kStackFrameExtraParamSlot = 14;
#endif

enum RegisterCode {
#define REGISTER_CODE(R) kRegCode_##R,
  GENERAL_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kRegAfterLast
};

class Register : public RegisterBase<Register, kRegAfterLast> {
 public:
#if V8_TARGET_LITTLE_ENDIAN
  static constexpr int kMantissaOffset = 0;
  static constexpr int kExponentOffset = 4;
#else
  static constexpr int kMantissaOffset = 4;
  static constexpr int kExponentOffset = 0;
#endif

 private:
  friend class RegisterBase;
  explicit constexpr Register(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(Register);
static_assert(sizeof(Register) == sizeof(int),
              "Register can efficiently be passed by value");

#define DEFINE_REGISTER(R) \
  constexpr Register R = Register::from_code(kRegCode_##R);
GENERAL_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr Register no_reg = Register::no_reg();

// Aliases
constexpr Register kConstantPoolRegister = r28;  // Constant pool.
constexpr Register kRootRegister = r29;          // Roots array pointer.
constexpr Register cp = r30;                     // JavaScript context pointer.

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

// Double word FP register.
class DoubleRegister : public RegisterBase<DoubleRegister, kDoubleAfterLast> {
 public:
  // A few double registers are reserved: one as a scratch register and one to
  // hold 0.0, that does not fit in the immediate field of vmov instructions.
  // d14: 0.0
  // d15: scratch register.
  static constexpr int kSizeInBytes = 8;

  // This function differs from kNumRegisters by returning the number of double
  // registers supported by the current CPU, while kNumRegisters always returns
  // 32.
  inline static int SupportedRegisterCount();

 private:
  friend class RegisterBase;
  explicit constexpr DoubleRegister(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(DoubleRegister);
static_assert(sizeof(DoubleRegister) == sizeof(int),
              "DoubleRegister can efficiently be passed by value");

using FloatRegister = DoubleRegister;

//     |      | 0
//     |      | 1
//     |      | 2
//     |      | ...
//     |      | 31
// VSX |
//     |      | 32
//     |      | 33
//     |  VMX | 34
//     |      | ...
//     |      | 63
//
// VSX registers (0 to 63) can be used by VSX vector instructions, which are
// mainly focused on Floating Point arithmetic. They do have few Integer
// Instructions such as logical operations, merge and select. The main Simd
// integer instructions such as add/sub/mul/ extract_lane/replace_lane,
// comparisons etc. are only available with VMX instructions and can only access
// the VMX set of vector registers (which is a subset of VSX registers). So to
// assure access to all Simd instructions in V8 and avoid moving data between
// registers, we are only using the upper 32 registers (VMX set) for Simd
// operations and only use the lower set for scalar (non simd) floating point
// operations which makes our Simd register set separate from Floating Point
// ones.
enum Simd128RegisterCode {
#define REGISTER_CODE(R) kSimd128Code_##R,
  SIMD128_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kSimd128AfterLast
};

// Simd128 register.
class Simd128Register
    : public RegisterBase<Simd128Register, kSimd128AfterLast> {
 private:
  friend class RegisterBase;
  explicit constexpr Simd128Register(int code) : RegisterBase(code) {}
};
ASSERT_TRIVIALLY_COPYABLE(Simd128Register);
static_assert(sizeof(Simd128Register) == sizeof(int),
              "Simd128Register can efficiently be passed by value");

#define DECLARE_SIMD128_REGISTER(R) \
  constexpr Simd128Register R = Simd128Register::from_code(kSimd128Code_##R);
SIMD128_REGISTERS(DECLARE_SIMD128_REGISTER)
#undef DECLARE_SIMD128_REGISTER
const Simd128Register no_simdreg = Simd128Register::no_reg();

#define DEFINE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code(kDoubleCode_##R);
DOUBLE_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

constexpr DoubleRegister kFirstCalleeSavedDoubleReg = d14;
constexpr DoubleRegister kLastCalleeSavedDoubleReg = d31;
constexpr DoubleRegister kDoubleRegZero = d14;
constexpr DoubleRegister kScratchDoubleReg = d13;
// Simd128 zero and scratch regs must have the same numbers as Double zero and
// scratch
constexpr Simd128Register kSimd128RegZero = v14;
constexpr Simd128Register kScratchSimd128Reg = v13;

Register ToRegister(int num);

enum CRegisterCode {
#define REGISTER_CODE(R) kCCode_##R,
  C_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kCAfterLast
};

// Coprocessor register
class CRegister : public RegisterBase<CRegister, kCAfterLast> {
  friend class RegisterBase;
  explicit constexpr CRegister(int code) : RegisterBase(code) {}
};

constexpr CRegister no_creg = CRegister::no_reg();
#define DECLARE_C_REGISTER(R) \
  constexpr CRegister R = CRegister::from_code(kCCode_##R);
C_REGISTERS(DECLARE_C_REGISTER)
#undef DECLARE_C_REGISTER

// Define {RegisterName} methods for the register types.
DEFINE_REGISTER_NAMES(Register, GENERAL_REGISTERS)
DEFINE_REGISTER_NAMES(DoubleRegister, DOUBLE_REGISTERS)
DEFINE_REGISTER_NAMES(Simd128Register, SIMD128_REGISTERS)

// Give alias names to registers for calling conventions.
constexpr Register kReturnRegister0 = r3;
constexpr Register kReturnRegister1 = r4;
constexpr Register kReturnRegister2 = r5;
constexpr Register kJSFunctionRegister = r4;
constexpr Register kContextRegister = r30;
constexpr Register kAllocateSizeRegister = r4;
constexpr Register kSpeculationPoisonRegister = r14;
constexpr Register kInterpreterAccumulatorRegister = r3;
constexpr Register kInterpreterBytecodeOffsetRegister = r15;
constexpr Register kInterpreterBytecodeArrayRegister = r16;
constexpr Register kInterpreterDispatchTableRegister = r17;

constexpr Register kJavaScriptCallArgCountRegister = r3;
constexpr Register kJavaScriptCallCodeStartRegister = r5;
constexpr Register kJavaScriptCallTargetRegister = kJSFunctionRegister;
constexpr Register kJavaScriptCallNewTargetRegister = r6;
constexpr Register kJavaScriptCallExtraArg1Register = r5;

constexpr Register kOffHeapTrampolineRegister = ip;
constexpr Register kRuntimeCallFunctionRegister = r4;
constexpr Register kRuntimeCallArgCountRegister = r3;
constexpr Register kRuntimeCallArgvRegister = r5;
constexpr Register kWasmInstanceRegister = r10;
constexpr Register kWasmCompileLazyFuncIndexRegister = r15;

constexpr DoubleRegister kFPReturnRegister0 = d1;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_PPC_REGISTER_PPC_H_

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_S390_REGISTER_S390_H_
#define V8_CODEGEN_S390_REGISTER_S390_H_

#include "src/codegen/register.h"
#include "src/codegen/reglist.h"

namespace v8 {
namespace internal {

// clang-format off
#define GENERAL_REGISTERS(V)                              \
  V(r0)  V(r1)  V(r2)  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)  \
  V(r8)  V(r9)  V(r10) V(fp) V(ip) V(r13) V(r14) V(sp)

#define ALLOCATABLE_GENERAL_REGISTERS(V)                  \
  V(r2)  V(r3)  V(r4)  V(r5)  V(r6)  V(r7)                \
  V(r8)  V(r9)  V(r13)

#define DOUBLE_REGISTERS(V)                               \
  V(d0)  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)  \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d13) V(d14) V(d15)

#define FLOAT_REGISTERS DOUBLE_REGISTERS
#define SIMD128_REGISTERS DOUBLE_REGISTERS

#define ALLOCATABLE_DOUBLE_REGISTERS(V)                   \
  V(d1)  V(d2)  V(d3)  V(d4)  V(d5)  V(d6)  V(d7)         \
  V(d8)  V(d9)  V(d10) V(d11) V(d12) V(d15) V(d0)

#define C_REGISTERS(V)                                            \
  V(cr0)  V(cr1)  V(cr2)  V(cr3)  V(cr4)  V(cr5)  V(cr6)  V(cr7)  \
  V(cr8)  V(cr9)  V(cr10) V(cr11) V(cr12) V(cr15)
// clang-format on

// Register list in load/store instructions
// Note that the bit values must match those used in actual instruction encoding

// Caller-saved/arguments registers
const RegList kJSCallerSaved = 1 << 1 | 1 << 2 |  // r2  a1
                               1 << 3 |           // r3  a2
                               1 << 4 |           // r4  a3
                               1 << 5;            // r5  a4

const int kNumJSCallerSaved = 5;

// Callee-saved registers preserved when switching from C to JavaScript
const RegList kCalleeSaved =
    1 << 6 |   // r6 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 7 |   // r7 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 8 |   // r8 (argument passing in CEntryStub)
               //    (HandleScope logic in MacroAssembler)
    1 << 9 |   // r9 (HandleScope logic in MacroAssembler)
    1 << 10 |  // r10 (Roots register in Javascript)
    1 << 11 |  // r11 (fp in Javascript)
    1 << 12 |  // r12 (ip in Javascript)
    1 << 13;   // r13 (cp in Javascript)
// 1 << 15;   // r15 (sp in Javascript)

const int kNumCalleeSaved = 8;

const RegList kCallerSavedDoubles = 1 << 0 |  // d0
                                    1 << 1 |  // d1
                                    1 << 2 |  // d2
                                    1 << 3 |  // d3
                                    1 << 4 |  // d4
                                    1 << 5 |  // d5
                                    1 << 6 |  // d6
                                    1 << 7;   // d7

const int kNumCallerSavedDoubles = 8;

const RegList kCalleeSavedDoubles = 1 << 8 |   // d8
                                    1 << 9 |   // d9
                                    1 << 10 |  // d10
                                    1 << 11 |  // d11
                                    1 << 12 |  // d12
                                    1 << 13 |  // d12
                                    1 << 14 |  // d12
                                    1 << 15;   // d13

const int kNumCalleeSavedDoubles = 8;

// The following constants describe the stack frame linkage area as
// defined by the ABI.

#if V8_TARGET_ARCH_S390X
// [0] Back Chain
// [1] Reserved for compiler use
// [2] GPR 2
// [3] GPR 3
// ...
// [15] GPR 15
// [16] FPR 0
// [17] FPR 2
// [18] FPR 4
// [19] FPR 6
const int kNumRequiredStackFrameSlots = 20;
const int kStackFrameRASlot = 14;
const int kStackFrameSPSlot = 15;
const int kStackFrameExtraParamSlot = 20;
#else
// [0] Back Chain
// [1] Reserved for compiler use
// [2] GPR 2
// [3] GPR 3
// ...
// [15] GPR 15
// [16..17] FPR 0
// [18..19] FPR 2
// [20..21] FPR 4
// [22..23] FPR 6
const int kNumRequiredStackFrameSlots = 24;
const int kStackFrameRASlot = 14;
const int kStackFrameSPSlot = 15;
const int kStackFrameExtraParamSlot = 24;
#endif

// zLinux ABI requires caller frames to include sufficient space for
// callee preserved register save area.
#if V8_TARGET_ARCH_S390X
const int kCalleeRegisterSaveAreaSize = 160;
#elif V8_TARGET_ARCH_S390
const int kCalleeRegisterSaveAreaSize = 96;
#else
const int kCalleeRegisterSaveAreaSize = 0;
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

// Register aliases
constexpr Register kRootRegister = r10;  // Roots array pointer.
constexpr Register cp = r13;             // JavaScript context pointer.

constexpr bool kPadArguments = false;
constexpr bool kSimpleFPAliasing = true;
constexpr bool kSimdMaskRegisters = false;

enum DoubleRegisterCode {
#define REGISTER_CODE(R) kDoubleCode_##R,
  DOUBLE_REGISTERS(REGISTER_CODE)
#undef REGISTER_CODE
      kDoubleAfterLast
};

// Double word VFP register.
class DoubleRegister : public RegisterBase<DoubleRegister, kDoubleAfterLast> {
 public:
  // A few double registers are reserved: one as a scratch register and one to
  // hold 0.0, that does not fit in the immediate field of vmov instructions.
  // d14: 0.0
  // d15: scratch register.
  static constexpr int kSizeInBytes = 8;
  inline static int NumRegisters();

 private:
  friend class RegisterBase;

  explicit constexpr DoubleRegister(int code) : RegisterBase(code) {}
};

ASSERT_TRIVIALLY_COPYABLE(DoubleRegister);
static_assert(sizeof(DoubleRegister) == sizeof(int),
              "DoubleRegister can efficiently be passed by value");

using FloatRegister = DoubleRegister;

// TODO(john.yan) Define SIMD registers.
using Simd128Register = DoubleRegister;

#define DEFINE_REGISTER(R) \
  constexpr DoubleRegister R = DoubleRegister::from_code(kDoubleCode_##R);
DOUBLE_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER
constexpr DoubleRegister no_dreg = DoubleRegister::no_reg();

constexpr DoubleRegister kDoubleRegZero = d14;
constexpr DoubleRegister kScratchDoubleReg = d13;

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

// Give alias names to registers for calling conventions.
constexpr Register kReturnRegister0 = r2;
constexpr Register kReturnRegister1 = r3;
constexpr Register kReturnRegister2 = r4;
constexpr Register kJSFunctionRegister = r3;
constexpr Register kContextRegister = r13;
constexpr Register kAllocateSizeRegister = r3;
constexpr Register kSpeculationPoisonRegister = r9;
constexpr Register kInterpreterAccumulatorRegister = r2;
constexpr Register kInterpreterBytecodeOffsetRegister = r6;
constexpr Register kInterpreterBytecodeArrayRegister = r7;
constexpr Register kInterpreterDispatchTableRegister = r8;

constexpr Register kJavaScriptCallArgCountRegister = r2;
constexpr Register kJavaScriptCallCodeStartRegister = r4;
constexpr Register kJavaScriptCallTargetRegister = kJSFunctionRegister;
constexpr Register kJavaScriptCallNewTargetRegister = r5;
constexpr Register kJavaScriptCallExtraArg1Register = r4;

constexpr Register kOffHeapTrampolineRegister = ip;
constexpr Register kRuntimeCallFunctionRegister = r3;
constexpr Register kRuntimeCallArgCountRegister = r2;
constexpr Register kRuntimeCallArgvRegister = r4;
constexpr Register kWasmInstanceRegister = r6;
constexpr Register kWasmCompileLazyFuncIndexRegister = r7;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_S390_REGISTER_S390_H_

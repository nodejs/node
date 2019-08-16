// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_ARM64_CONSTANTS_ARM64_H_
#define V8_CODEGEN_ARM64_CONSTANTS_ARM64_H_

#include "src/base/macros.h"
#include "src/common/globals.h"

// Assert that this is an LP64 system, or LLP64 on Windows.
STATIC_ASSERT(sizeof(int) == sizeof(int32_t));
#if defined(V8_OS_WIN)
STATIC_ASSERT(sizeof(1L) == sizeof(int32_t));
#else
STATIC_ASSERT(sizeof(long) == sizeof(int64_t));  // NOLINT(runtime/int)
STATIC_ASSERT(sizeof(1L) == sizeof(int64_t));
#endif
STATIC_ASSERT(sizeof(void*) == sizeof(int64_t));
STATIC_ASSERT(sizeof(1) == sizeof(int32_t));

// Get the standard printf format macros for C99 stdint types.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

namespace v8 {
namespace internal {

constexpr size_t kMaxPCRelativeCodeRangeInMB = 128;

constexpr uint8_t kInstrSize = 4;
constexpr uint8_t kInstrSizeLog2 = 2;
constexpr uint8_t kLoadLiteralScaleLog2 = 2;
constexpr int kMaxLoadLiteralRange = 1 * MB;

const int kNumberOfRegisters = 32;
const int kNumberOfVRegisters = 32;
// Callee saved registers are x19-x30(lr).
const int kNumberOfCalleeSavedRegisters = 11;
const int kFirstCalleeSavedRegisterIndex = 19;
// Callee saved FP registers are d8-d15.
const int kNumberOfCalleeSavedVRegisters = 8;
const int kFirstCalleeSavedVRegisterIndex = 8;
// Callee saved registers with no specific purpose in JS are x19-x25.
const size_t kJSCalleeSavedRegList = 0x03f80000;
const int kWRegSizeInBits = 32;
const int kWRegSizeInBitsLog2 = 5;
const int kWRegSize = kWRegSizeInBits >> 3;
const int kWRegSizeLog2 = kWRegSizeInBitsLog2 - 3;
const int kXRegSizeInBits = 64;
const int kXRegSizeInBitsLog2 = 6;
const int kXRegSize = kXRegSizeInBits >> 3;
const int kXRegSizeLog2 = kXRegSizeInBitsLog2 - 3;
const int kSRegSizeInBits = 32;
const int kSRegSizeInBitsLog2 = 5;
const int kSRegSize = kSRegSizeInBits >> 3;
const int kSRegSizeLog2 = kSRegSizeInBitsLog2 - 3;
const int kDRegSizeInBits = 64;
const int kDRegSizeInBitsLog2 = 6;
const int kDRegSize = kDRegSizeInBits >> 3;
const int kDRegSizeLog2 = kDRegSizeInBitsLog2 - 3;
const int kDRegSizeInBytesLog2 = kDRegSizeInBitsLog2 - 3;
const int kBRegSizeInBits = 8;
const int kBRegSize = kBRegSizeInBits >> 3;
const int kHRegSizeInBits = 16;
const int kHRegSize = kHRegSizeInBits >> 3;
const int kQRegSizeInBits = 128;
const int kQRegSizeInBitsLog2 = 7;
const int kQRegSize = kQRegSizeInBits >> 3;
const int kQRegSizeLog2 = kQRegSizeInBitsLog2 - 3;
const int kVRegSizeInBits = kQRegSizeInBits;
const int kVRegSize = kVRegSizeInBits >> 3;
const int64_t kWRegMask = 0x00000000ffffffffL;
const int64_t kXRegMask = 0xffffffffffffffffL;
const int64_t kSRegMask = 0x00000000ffffffffL;
const int64_t kDRegMask = 0xffffffffffffffffL;
// TODO(all) check if the expression below works on all compilers or if it
// triggers an overflow error.
const int64_t kDSignBit = 63;
const int64_t kDSignMask = 0x1LL << kDSignBit;
const int64_t kSSignBit = 31;
const int64_t kSSignMask = 0x1LL << kSSignBit;
const int64_t kXSignBit = 63;
const int64_t kXSignMask = 0x1LL << kXSignBit;
const int64_t kWSignBit = 31;
const int64_t kWSignMask = 0x1LL << kWSignBit;
const int64_t kDQuietNanBit = 51;
const int64_t kDQuietNanMask = 0x1LL << kDQuietNanBit;
const int64_t kSQuietNanBit = 22;
const int64_t kSQuietNanMask = 0x1LL << kSQuietNanBit;
const int64_t kByteMask = 0xffL;
const int64_t kHalfWordMask = 0xffffL;
const int64_t kWordMask = 0xffffffffL;
const uint64_t kXMaxUInt = 0xffffffffffffffffUL;
const uint64_t kWMaxUInt = 0xffffffffUL;
const int64_t kXMaxInt = 0x7fffffffffffffffL;
const int64_t kXMinInt = 0x8000000000000000L;
const int32_t kWMaxInt = 0x7fffffff;
const int32_t kWMinInt = 0x80000000;
const int kIp0Code = 16;
const int kIp1Code = 17;
const int kFramePointerRegCode = 29;
const int kLinkRegCode = 30;
const int kZeroRegCode = 31;
const int kSPRegInternalCode = 63;
const unsigned kRegCodeMask = 0x1f;
const unsigned kShiftAmountWRegMask = 0x1f;
const unsigned kShiftAmountXRegMask = 0x3f;
// Standard machine types defined by AAPCS64.
const unsigned kHalfWordSize = 16;
const unsigned kHalfWordSizeLog2 = 4;
const unsigned kHalfWordSizeInBytes = kHalfWordSize >> 3;
const unsigned kHalfWordSizeInBytesLog2 = kHalfWordSizeLog2 - 3;
const unsigned kWordSize = 32;
const unsigned kWordSizeLog2 = 5;
const unsigned kWordSizeInBytes = kWordSize >> 3;
const unsigned kWordSizeInBytesLog2 = kWordSizeLog2 - 3;
const unsigned kDoubleWordSize = 64;
const unsigned kDoubleWordSizeInBytes = kDoubleWordSize >> 3;
const unsigned kQuadWordSize = 128;
const unsigned kQuadWordSizeInBytes = kQuadWordSize >> 3;
const int kMaxLanesPerVector = 16;

const unsigned kAddressTagOffset = 56;
const unsigned kAddressTagWidth = 8;
const uint64_t kAddressTagMask = ((UINT64_C(1) << kAddressTagWidth) - 1)
                                 << kAddressTagOffset;
static_assert(kAddressTagMask == UINT64_C(0xff00000000000000),
              "AddressTagMask must represent most-significant eight bits.");

// AArch64 floating-point specifics. These match IEEE-754.
const unsigned kDoubleMantissaBits = 52;
const unsigned kDoubleExponentBits = 11;
const unsigned kDoubleExponentBias = 1023;
const unsigned kFloatMantissaBits = 23;
const unsigned kFloatExponentBits = 8;
const unsigned kFloatExponentBias = 127;
const unsigned kFloat16MantissaBits = 10;
const unsigned kFloat16ExponentBits = 5;
const unsigned kFloat16ExponentBias = 15;

// Actual value of root register is offset from the root array's start
// to take advantage of negative displacement values.
// TODO(sigurds): Choose best value.
constexpr int kRootRegisterBias = 256;

using float16 = uint16_t;

#define INSTRUCTION_FIELDS_LIST(V_)                     \
  /* Register fields */                                 \
  V_(Rd, 4, 0, Bits)    /* Destination register.     */ \
  V_(Rn, 9, 5, Bits)    /* First source register.    */ \
  V_(Rm, 20, 16, Bits)  /* Second source register.   */ \
  V_(Ra, 14, 10, Bits)  /* Third source register.    */ \
  V_(Rt, 4, 0, Bits)    /* Load dest / store source. */ \
  V_(Rt2, 14, 10, Bits) /* Load second dest /        */ \
                        /* store second source.      */ \
  V_(Rs, 20, 16, Bits)  /* Store-exclusive status    */ \
  V_(PrefetchMode, 4, 0, Bits)                          \
                                                        \
  /* Common bits */                                     \
  V_(SixtyFourBits, 31, 31, Bits)                       \
  V_(FlagsUpdate, 29, 29, Bits)                         \
                                                        \
  /* PC relative addressing */                          \
  V_(ImmPCRelHi, 23, 5, SignedBits)                     \
  V_(ImmPCRelLo, 30, 29, Bits)                          \
                                                        \
  /* Add/subtract/logical shift register */             \
  V_(ShiftDP, 23, 22, Bits)                             \
  V_(ImmDPShift, 15, 10, Bits)                          \
                                                        \
  /* Add/subtract immediate */                          \
  V_(ImmAddSub, 21, 10, Bits)                           \
  V_(ShiftAddSub, 23, 22, Bits)                         \
                                                        \
  /* Add/subtract extend */                             \
  V_(ImmExtendShift, 12, 10, Bits)                      \
  V_(ExtendMode, 15, 13, Bits)                          \
                                                        \
  /* Move wide */                                       \
  V_(ImmMoveWide, 20, 5, Bits)                          \
  V_(ShiftMoveWide, 22, 21, Bits)                       \
                                                        \
  /* Logical immediate, bitfield and extract */         \
  V_(BitN, 22, 22, Bits)                                \
  V_(ImmRotate, 21, 16, Bits)                           \
  V_(ImmSetBits, 15, 10, Bits)                          \
  V_(ImmR, 21, 16, Bits)                                \
  V_(ImmS, 15, 10, Bits)                                \
                                                        \
  /* Test and branch immediate */                       \
  V_(ImmTestBranch, 18, 5, SignedBits)                  \
  V_(ImmTestBranchBit40, 23, 19, Bits)                  \
  V_(ImmTestBranchBit5, 31, 31, Bits)                   \
                                                        \
  /* Conditionals */                                    \
  V_(Condition, 15, 12, Bits)                           \
  V_(ConditionBranch, 3, 0, Bits)                       \
  V_(Nzcv, 3, 0, Bits)                                  \
  V_(ImmCondCmp, 20, 16, Bits)                          \
  V_(ImmCondBranch, 23, 5, SignedBits)                  \
                                                        \
  /* Floating point */                                  \
  V_(FPType, 23, 22, Bits)                              \
  V_(ImmFP, 20, 13, Bits)                               \
  V_(FPScale, 15, 10, Bits)                             \
                                                        \
  /* Load Store */                                      \
  V_(ImmLS, 20, 12, SignedBits)                         \
  V_(ImmLSUnsigned, 21, 10, Bits)                       \
  V_(ImmLSPair, 21, 15, SignedBits)                     \
  V_(ImmShiftLS, 12, 12, Bits)                          \
  V_(LSOpc, 23, 22, Bits)                               \
  V_(LSVector, 26, 26, Bits)                            \
  V_(LSSize, 31, 30, Bits)                              \
                                                        \
  /* NEON generic fields */                             \
  V_(NEONQ, 30, 30, Bits)                               \
  V_(NEONSize, 23, 22, Bits)                            \
  V_(NEONLSSize, 11, 10, Bits)                          \
  V_(NEONS, 12, 12, Bits)                               \
  V_(NEONL, 21, 21, Bits)                               \
  V_(NEONM, 20, 20, Bits)                               \
  V_(NEONH, 11, 11, Bits)                               \
  V_(ImmNEONExt, 14, 11, Bits)                          \
  V_(ImmNEON5, 20, 16, Bits)                            \
  V_(ImmNEON4, 14, 11, Bits)                            \
                                                        \
  /* Other immediates */                                \
  V_(ImmUncondBranch, 25, 0, SignedBits)                \
  V_(ImmCmpBranch, 23, 5, SignedBits)                   \
  V_(ImmLLiteral, 23, 5, SignedBits)                    \
  V_(ImmException, 20, 5, Bits)                         \
  V_(ImmHint, 11, 5, Bits)                              \
  V_(ImmBarrierDomain, 11, 10, Bits)                    \
  V_(ImmBarrierType, 9, 8, Bits)                        \
                                                        \
  /* System (MRS, MSR) */                               \
  V_(ImmSystemRegister, 19, 5, Bits)                    \
  V_(SysO0, 19, 19, Bits)                               \
  V_(SysOp1, 18, 16, Bits)                              \
  V_(SysOp2, 7, 5, Bits)                                \
  V_(CRn, 15, 12, Bits)                                 \
  V_(CRm, 11, 8, Bits)                                  \
                                                        \
  /* Load-/store-exclusive */                           \
  V_(LoadStoreXLoad, 22, 22, Bits)                      \
  V_(LoadStoreXNotExclusive, 23, 23, Bits)              \
  V_(LoadStoreXAcquireRelease, 15, 15, Bits)            \
  V_(LoadStoreXSizeLog2, 31, 30, Bits)                  \
  V_(LoadStoreXPair, 21, 21, Bits)                      \
                                                        \
  /* NEON load/store */                                 \
  V_(NEONLoad, 22, 22, Bits)                            \
                                                        \
  /* NEON Modified Immediate fields */                  \
  V_(ImmNEONabc, 18, 16, Bits)                          \
  V_(ImmNEONdefgh, 9, 5, Bits)                          \
  V_(NEONModImmOp, 29, 29, Bits)                        \
  V_(NEONCmode, 15, 12, Bits)                           \
                                                        \
  /* NEON Shift Immediate fields */                     \
  V_(ImmNEONImmhImmb, 22, 16, Bits)                     \
  V_(ImmNEONImmh, 22, 19, Bits)                         \
  V_(ImmNEONImmb, 18, 16, Bits)

#define SYSTEM_REGISTER_FIELDS_LIST(V_, M_) \
  /* NZCV */                                \
  V_(Flags, 31, 28, Bits, uint32_t)         \
  V_(N, 31, 31, Bits, bool)                 \
  V_(Z, 30, 30, Bits, bool)                 \
  V_(C, 29, 29, Bits, bool)                 \
  V_(V, 28, 28, Bits, bool)                 \
  M_(NZCV, Flags_mask)                      \
                                            \
  /* FPCR */                                \
  V_(AHP, 26, 26, Bits, bool)               \
  V_(DN, 25, 25, Bits, bool)                \
  V_(FZ, 24, 24, Bits, bool)                \
  V_(RMode, 23, 22, Bits, FPRounding)       \
  M_(FPCR, AHP_mask | DN_mask | FZ_mask | RMode_mask)

// Fields offsets.
#define DECLARE_FIELDS_OFFSETS(Name, HighBit, LowBit, unused_1, unused_2) \
  const int Name##_offset = LowBit;                                       \
  const int Name##_width = HighBit - LowBit + 1;                          \
  const uint32_t Name##_mask = ((1 << Name##_width) - 1) << LowBit;
#define DECLARE_INSTRUCTION_FIELDS_OFFSETS(Name, HighBit, LowBit, unused_1) \
  DECLARE_FIELDS_OFFSETS(Name, HighBit, LowBit, unused_1, unused_2)
INSTRUCTION_FIELDS_LIST(DECLARE_INSTRUCTION_FIELDS_OFFSETS)
SYSTEM_REGISTER_FIELDS_LIST(DECLARE_FIELDS_OFFSETS, NOTHING)
#undef DECLARE_FIELDS_OFFSETS
#undef DECLARE_INSTRUCTION_FIELDS_OFFSETS

// ImmPCRel is a compound field (not present in INSTRUCTION_FIELDS_LIST), formed
// from ImmPCRelLo and ImmPCRelHi.
const int ImmPCRel_mask = ImmPCRelLo_mask | ImmPCRelHi_mask;

// Condition codes.
enum Condition {
  eq = 0,
  ne = 1,
  hs = 2,
  cs = hs,
  lo = 3,
  cc = lo,
  mi = 4,
  pl = 5,
  vs = 6,
  vc = 7,
  hi = 8,
  ls = 9,
  ge = 10,
  lt = 11,
  gt = 12,
  le = 13,
  al = 14,
  nv = 15  // Behaves as always/al.
};

inline Condition NegateCondition(Condition cond) {
  // Conditions al and nv behave identically, as "always true". They can't be
  // inverted, because there is no never condition.
  DCHECK((cond != al) && (cond != nv));
  return static_cast<Condition>(cond ^ 1);
}

enum FlagsUpdate { SetFlags = 1, LeaveFlags = 0 };

enum StatusFlags {
  NoFlag = 0,

  // Derive the flag combinations from the system register bit descriptions.
  NFlag = N_mask,
  ZFlag = Z_mask,
  CFlag = C_mask,
  VFlag = V_mask,
  NZFlag = NFlag | ZFlag,
  NCFlag = NFlag | CFlag,
  NVFlag = NFlag | VFlag,
  ZCFlag = ZFlag | CFlag,
  ZVFlag = ZFlag | VFlag,
  CVFlag = CFlag | VFlag,
  NZCFlag = NFlag | ZFlag | CFlag,
  NZVFlag = NFlag | ZFlag | VFlag,
  NCVFlag = NFlag | CFlag | VFlag,
  ZCVFlag = ZFlag | CFlag | VFlag,
  NZCVFlag = NFlag | ZFlag | CFlag | VFlag,

  // Floating-point comparison results.
  FPEqualFlag = ZCFlag,
  FPLessThanFlag = NFlag,
  FPGreaterThanFlag = CFlag,
  FPUnorderedFlag = CVFlag
};

enum Shift {
  NO_SHIFT = -1,
  LSL = 0x0,
  LSR = 0x1,
  ASR = 0x2,
  ROR = 0x3,
  MSL = 0x4
};

enum Extend {
  NO_EXTEND = -1,
  UXTB = 0,
  UXTH = 1,
  UXTW = 2,
  UXTX = 3,
  SXTB = 4,
  SXTH = 5,
  SXTW = 6,
  SXTX = 7
};

enum SystemHint {
  NOP = 0,
  YIELD = 1,
  WFE = 2,
  WFI = 3,
  SEV = 4,
  SEVL = 5,
  CSDB = 20
};

enum BarrierDomain {
  OuterShareable = 0,
  NonShareable = 1,
  InnerShareable = 2,
  FullSystem = 3
};

enum BarrierType {
  BarrierOther = 0,
  BarrierReads = 1,
  BarrierWrites = 2,
  BarrierAll = 3
};

// System/special register names.
// This information is not encoded as one field but as the concatenation of
// multiple fields (Op0<0>, Op1, Crn, Crm, Op2).
enum SystemRegister {
  NZCV = ((0x1 << SysO0_offset) | (0x3 << SysOp1_offset) | (0x4 << CRn_offset) |
          (0x2 << CRm_offset) | (0x0 << SysOp2_offset)) >>
         ImmSystemRegister_offset,
  FPCR = ((0x1 << SysO0_offset) | (0x3 << SysOp1_offset) | (0x4 << CRn_offset) |
          (0x4 << CRm_offset) | (0x0 << SysOp2_offset)) >>
         ImmSystemRegister_offset
};

// Instruction enumerations.
//
// These are the masks that define a class of instructions, and the list of
// instructions within each class. Each enumeration has a Fixed, FMask and
// Mask value.
//
// Fixed: The fixed bits in this instruction class.
// FMask: The mask used to extract the fixed bits in the class.
// Mask:  The mask used to identify the instructions within a class.
//
// The enumerations can be used like this:
//
// DCHECK(instr->Mask(PCRelAddressingFMask) == PCRelAddressingFixed);
// switch(instr->Mask(PCRelAddressingMask)) {
//   case ADR:  Format("adr 'Xd, 'AddrPCRelByte"); break;
//   case ADRP: Format("adrp 'Xd, 'AddrPCRelPage"); break;
//   default:   printf("Unknown instruction\n");
// }

// Used to corrupt encodings by setting all bits when orred. Although currently
// unallocated in AArch64, this encoding is not guaranteed to be undefined
// indefinitely.
const uint32_t kUnallocatedInstruction = 0xffffffff;

// Generic fields.
enum GenericInstrField : uint32_t {
  SixtyFourBits = 0x80000000,
  ThirtyTwoBits = 0x00000000,
  FP32 = 0x00000000,
  FP64 = 0x00400000
};

enum NEONFormatField : uint32_t {
  NEONFormatFieldMask = 0x40C00000,
  NEON_Q = 0x40000000,
  NEON_8B = 0x00000000,
  NEON_16B = NEON_8B | NEON_Q,
  NEON_4H = 0x00400000,
  NEON_8H = NEON_4H | NEON_Q,
  NEON_2S = 0x00800000,
  NEON_4S = NEON_2S | NEON_Q,
  NEON_1D = 0x00C00000,
  NEON_2D = 0x00C00000 | NEON_Q
};

enum NEONFPFormatField : uint32_t {
  NEONFPFormatFieldMask = 0x40400000,
  NEON_FP_2S = FP32,
  NEON_FP_4S = FP32 | NEON_Q,
  NEON_FP_2D = FP64 | NEON_Q
};

enum NEONLSFormatField : uint32_t {
  NEONLSFormatFieldMask = 0x40000C00,
  LS_NEON_8B = 0x00000000,
  LS_NEON_16B = LS_NEON_8B | NEON_Q,
  LS_NEON_4H = 0x00000400,
  LS_NEON_8H = LS_NEON_4H | NEON_Q,
  LS_NEON_2S = 0x00000800,
  LS_NEON_4S = LS_NEON_2S | NEON_Q,
  LS_NEON_1D = 0x00000C00,
  LS_NEON_2D = LS_NEON_1D | NEON_Q
};

enum NEONScalarFormatField : uint32_t {
  NEONScalarFormatFieldMask = 0x00C00000,
  NEONScalar = 0x10000000,
  NEON_B = 0x00000000,
  NEON_H = 0x00400000,
  NEON_S = 0x00800000,
  NEON_D = 0x00C00000
};

// PC relative addressing.
enum PCRelAddressingOp : uint32_t {
  PCRelAddressingFixed = 0x10000000,
  PCRelAddressingFMask = 0x1F000000,
  PCRelAddressingMask = 0x9F000000,
  ADR = PCRelAddressingFixed | 0x00000000,
  ADRP = PCRelAddressingFixed | 0x80000000
};

// Add/sub (immediate, shifted and extended.)
const int kSFOffset = 31;
enum AddSubOp : uint32_t {
  AddSubOpMask = 0x60000000,
  AddSubSetFlagsBit = 0x20000000,
  ADD = 0x00000000,
  ADDS = ADD | AddSubSetFlagsBit,
  SUB = 0x40000000,
  SUBS = SUB | AddSubSetFlagsBit
};

#define ADD_SUB_OP_LIST(V) V(ADD), V(ADDS), V(SUB), V(SUBS)

enum AddSubImmediateOp : uint32_t {
  AddSubImmediateFixed = 0x11000000,
  AddSubImmediateFMask = 0x1F000000,
  AddSubImmediateMask = 0xFF000000,
#define ADD_SUB_IMMEDIATE(A)            \
  A##_w_imm = AddSubImmediateFixed | A, \
  A##_x_imm = AddSubImmediateFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_IMMEDIATE)
#undef ADD_SUB_IMMEDIATE
};

enum AddSubShiftedOp : uint32_t {
  AddSubShiftedFixed = 0x0B000000,
  AddSubShiftedFMask = 0x1F200000,
  AddSubShiftedMask = 0xFF200000,
#define ADD_SUB_SHIFTED(A)              \
  A##_w_shift = AddSubShiftedFixed | A, \
  A##_x_shift = AddSubShiftedFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_SHIFTED)
#undef ADD_SUB_SHIFTED
};

enum AddSubExtendedOp : uint32_t {
  AddSubExtendedFixed = 0x0B200000,
  AddSubExtendedFMask = 0x1F200000,
  AddSubExtendedMask = 0xFFE00000,
#define ADD_SUB_EXTENDED(A)            \
  A##_w_ext = AddSubExtendedFixed | A, \
  A##_x_ext = AddSubExtendedFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_EXTENDED)
#undef ADD_SUB_EXTENDED
};

// Add/sub with carry.
enum AddSubWithCarryOp : uint32_t {
  AddSubWithCarryFixed = 0x1A000000,
  AddSubWithCarryFMask = 0x1FE00000,
  AddSubWithCarryMask = 0xFFE0FC00,
  ADC_w = AddSubWithCarryFixed | ADD,
  ADC_x = AddSubWithCarryFixed | ADD | SixtyFourBits,
  ADC = ADC_w,
  ADCS_w = AddSubWithCarryFixed | ADDS,
  ADCS_x = AddSubWithCarryFixed | ADDS | SixtyFourBits,
  SBC_w = AddSubWithCarryFixed | SUB,
  SBC_x = AddSubWithCarryFixed | SUB | SixtyFourBits,
  SBC = SBC_w,
  SBCS_w = AddSubWithCarryFixed | SUBS,
  SBCS_x = AddSubWithCarryFixed | SUBS | SixtyFourBits
};

// Logical (immediate and shifted register).
enum LogicalOp : uint32_t {
  LogicalOpMask = 0x60200000,
  NOT = 0x00200000,
  AND = 0x00000000,
  BIC = AND | NOT,
  ORR = 0x20000000,
  ORN = ORR | NOT,
  EOR = 0x40000000,
  EON = EOR | NOT,
  ANDS = 0x60000000,
  BICS = ANDS | NOT
};

// Logical immediate.
enum LogicalImmediateOp : uint32_t {
  LogicalImmediateFixed = 0x12000000,
  LogicalImmediateFMask = 0x1F800000,
  LogicalImmediateMask = 0xFF800000,
  AND_w_imm = LogicalImmediateFixed | AND,
  AND_x_imm = LogicalImmediateFixed | AND | SixtyFourBits,
  ORR_w_imm = LogicalImmediateFixed | ORR,
  ORR_x_imm = LogicalImmediateFixed | ORR | SixtyFourBits,
  EOR_w_imm = LogicalImmediateFixed | EOR,
  EOR_x_imm = LogicalImmediateFixed | EOR | SixtyFourBits,
  ANDS_w_imm = LogicalImmediateFixed | ANDS,
  ANDS_x_imm = LogicalImmediateFixed | ANDS | SixtyFourBits
};

// Logical shifted register.
enum LogicalShiftedOp : uint32_t {
  LogicalShiftedFixed = 0x0A000000,
  LogicalShiftedFMask = 0x1F000000,
  LogicalShiftedMask = 0xFF200000,
  AND_w = LogicalShiftedFixed | AND,
  AND_x = LogicalShiftedFixed | AND | SixtyFourBits,
  AND_shift = AND_w,
  BIC_w = LogicalShiftedFixed | BIC,
  BIC_x = LogicalShiftedFixed | BIC | SixtyFourBits,
  BIC_shift = BIC_w,
  ORR_w = LogicalShiftedFixed | ORR,
  ORR_x = LogicalShiftedFixed | ORR | SixtyFourBits,
  ORR_shift = ORR_w,
  ORN_w = LogicalShiftedFixed | ORN,
  ORN_x = LogicalShiftedFixed | ORN | SixtyFourBits,
  ORN_shift = ORN_w,
  EOR_w = LogicalShiftedFixed | EOR,
  EOR_x = LogicalShiftedFixed | EOR | SixtyFourBits,
  EOR_shift = EOR_w,
  EON_w = LogicalShiftedFixed | EON,
  EON_x = LogicalShiftedFixed | EON | SixtyFourBits,
  EON_shift = EON_w,
  ANDS_w = LogicalShiftedFixed | ANDS,
  ANDS_x = LogicalShiftedFixed | ANDS | SixtyFourBits,
  ANDS_shift = ANDS_w,
  BICS_w = LogicalShiftedFixed | BICS,
  BICS_x = LogicalShiftedFixed | BICS | SixtyFourBits,
  BICS_shift = BICS_w
};

// Move wide immediate.
enum MoveWideImmediateOp : uint32_t {
  MoveWideImmediateFixed = 0x12800000,
  MoveWideImmediateFMask = 0x1F800000,
  MoveWideImmediateMask = 0xFF800000,
  MOVN = 0x00000000,
  MOVZ = 0x40000000,
  MOVK = 0x60000000,
  MOVN_w = MoveWideImmediateFixed | MOVN,
  MOVN_x = MoveWideImmediateFixed | MOVN | SixtyFourBits,
  MOVZ_w = MoveWideImmediateFixed | MOVZ,
  MOVZ_x = MoveWideImmediateFixed | MOVZ | SixtyFourBits,
  MOVK_w = MoveWideImmediateFixed | MOVK,
  MOVK_x = MoveWideImmediateFixed | MOVK | SixtyFourBits
};

// Bitfield.
const int kBitfieldNOffset = 22;
enum BitfieldOp : uint32_t {
  BitfieldFixed = 0x13000000,
  BitfieldFMask = 0x1F800000,
  BitfieldMask = 0xFF800000,
  SBFM_w = BitfieldFixed | 0x00000000,
  SBFM_x = BitfieldFixed | 0x80000000,
  SBFM = SBFM_w,
  BFM_w = BitfieldFixed | 0x20000000,
  BFM_x = BitfieldFixed | 0xA0000000,
  BFM = BFM_w,
  UBFM_w = BitfieldFixed | 0x40000000,
  UBFM_x = BitfieldFixed | 0xC0000000,
  UBFM = UBFM_w
  // Bitfield N field.
};

// Extract.
enum ExtractOp : uint32_t {
  ExtractFixed = 0x13800000,
  ExtractFMask = 0x1F800000,
  ExtractMask = 0xFFA00000,
  EXTR_w = ExtractFixed | 0x00000000,
  EXTR_x = ExtractFixed | 0x80000000,
  EXTR = EXTR_w
};

// Unconditional branch.
enum UnconditionalBranchOp : uint32_t {
  UnconditionalBranchFixed = 0x14000000,
  UnconditionalBranchFMask = 0x7C000000,
  UnconditionalBranchMask = 0xFC000000,
  B = UnconditionalBranchFixed | 0x00000000,
  BL = UnconditionalBranchFixed | 0x80000000
};

// Unconditional branch to register.
enum UnconditionalBranchToRegisterOp : uint32_t {
  UnconditionalBranchToRegisterFixed = 0xD6000000,
  UnconditionalBranchToRegisterFMask = 0xFE000000,
  UnconditionalBranchToRegisterMask = 0xFFFFFC1F,
  BR = UnconditionalBranchToRegisterFixed | 0x001F0000,
  BLR = UnconditionalBranchToRegisterFixed | 0x003F0000,
  RET = UnconditionalBranchToRegisterFixed | 0x005F0000
};

// Compare and branch.
enum CompareBranchOp : uint32_t {
  CompareBranchFixed = 0x34000000,
  CompareBranchFMask = 0x7E000000,
  CompareBranchMask = 0xFF000000,
  CBZ_w = CompareBranchFixed | 0x00000000,
  CBZ_x = CompareBranchFixed | 0x80000000,
  CBZ = CBZ_w,
  CBNZ_w = CompareBranchFixed | 0x01000000,
  CBNZ_x = CompareBranchFixed | 0x81000000,
  CBNZ = CBNZ_w
};

// Test and branch.
enum TestBranchOp : uint32_t {
  TestBranchFixed = 0x36000000,
  TestBranchFMask = 0x7E000000,
  TestBranchMask = 0x7F000000,
  TBZ = TestBranchFixed | 0x00000000,
  TBNZ = TestBranchFixed | 0x01000000
};

// Conditional branch.
enum ConditionalBranchOp : uint32_t {
  ConditionalBranchFixed = 0x54000000,
  ConditionalBranchFMask = 0xFE000000,
  ConditionalBranchMask = 0xFF000010,
  B_cond = ConditionalBranchFixed | 0x00000000
};

// System.
// System instruction encoding is complicated because some instructions use op
// and CR fields to encode parameters. To handle this cleanly, the system
// instructions are split into more than one enum.

enum SystemOp : uint32_t { SystemFixed = 0xD5000000, SystemFMask = 0xFFC00000 };

enum SystemSysRegOp : uint32_t {
  SystemSysRegFixed = 0xD5100000,
  SystemSysRegFMask = 0xFFD00000,
  SystemSysRegMask = 0xFFF00000,
  MRS = SystemSysRegFixed | 0x00200000,
  MSR = SystemSysRegFixed | 0x00000000
};

enum SystemHintOp : uint32_t {
  SystemHintFixed = 0xD503201F,
  SystemHintFMask = 0xFFFFF01F,
  SystemHintMask = 0xFFFFF01F,
  HINT = SystemHintFixed | 0x00000000
};

// Exception.
enum ExceptionOp : uint32_t {
  ExceptionFixed = 0xD4000000,
  ExceptionFMask = 0xFF000000,
  ExceptionMask = 0xFFE0001F,
  HLT = ExceptionFixed | 0x00400000,
  BRK = ExceptionFixed | 0x00200000,
  SVC = ExceptionFixed | 0x00000001,
  HVC = ExceptionFixed | 0x00000002,
  SMC = ExceptionFixed | 0x00000003,
  DCPS1 = ExceptionFixed | 0x00A00001,
  DCPS2 = ExceptionFixed | 0x00A00002,
  DCPS3 = ExceptionFixed | 0x00A00003
};
// Code used to spot hlt instructions that should not be hit.
const int kHltBadCode = 0xbad;

enum MemBarrierOp : uint32_t {
  MemBarrierFixed = 0xD503309F,
  MemBarrierFMask = 0xFFFFF09F,
  MemBarrierMask = 0xFFFFF0FF,
  DSB = MemBarrierFixed | 0x00000000,
  DMB = MemBarrierFixed | 0x00000020,
  ISB = MemBarrierFixed | 0x00000040
};

// Any load or store (including pair).
enum LoadStoreAnyOp : uint32_t {
  LoadStoreAnyFMask = 0x0a000000,
  LoadStoreAnyFixed = 0x08000000
};

// Any load pair or store pair.
enum LoadStorePairAnyOp : uint32_t {
  LoadStorePairAnyFMask = 0x3a000000,
  LoadStorePairAnyFixed = 0x28000000
};

#define LOAD_STORE_PAIR_OP_LIST(V)                                         \
  V(STP, w, 0x00000000)                                                    \
  , V(LDP, w, 0x00400000), V(LDPSW, x, 0x40400000), V(STP, x, 0x80000000), \
      V(LDP, x, 0x80400000), V(STP, s, 0x04000000), V(LDP, s, 0x04400000), \
      V(STP, d, 0x44000000), V(LDP, d, 0x44400000), V(STP, q, 0x84000000), \
      V(LDP, q, 0x84400000)

// Load/store pair (post, pre and offset.)
enum LoadStorePairOp : uint32_t {
  LoadStorePairMask = 0xC4400000,
  LoadStorePairLBit = 1 << 22,
#define LOAD_STORE_PAIR(A, B, C) A##_##B = C
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR)
#undef LOAD_STORE_PAIR
};

enum LoadStorePairPostIndexOp : uint32_t {
  LoadStorePairPostIndexFixed = 0x28800000,
  LoadStorePairPostIndexFMask = 0x3B800000,
  LoadStorePairPostIndexMask = 0xFFC00000,
#define LOAD_STORE_PAIR_POST_INDEX(A, B, C) \
  A##_##B##_post = LoadStorePairPostIndexFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_POST_INDEX)
#undef LOAD_STORE_PAIR_POST_INDEX
};

enum LoadStorePairPreIndexOp : uint32_t {
  LoadStorePairPreIndexFixed = 0x29800000,
  LoadStorePairPreIndexFMask = 0x3B800000,
  LoadStorePairPreIndexMask = 0xFFC00000,
#define LOAD_STORE_PAIR_PRE_INDEX(A, B, C) \
  A##_##B##_pre = LoadStorePairPreIndexFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_PRE_INDEX)
#undef LOAD_STORE_PAIR_PRE_INDEX
};

enum LoadStorePairOffsetOp : uint32_t {
  LoadStorePairOffsetFixed = 0x29000000,
  LoadStorePairOffsetFMask = 0x3B800000,
  LoadStorePairOffsetMask = 0xFFC00000,
#define LOAD_STORE_PAIR_OFFSET(A, B, C) \
  A##_##B##_off = LoadStorePairOffsetFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_OFFSET)
#undef LOAD_STORE_PAIR_OFFSET
};

// Load literal.
enum LoadLiteralOp : uint32_t {
  LoadLiteralFixed = 0x18000000,
  LoadLiteralFMask = 0x3B000000,
  LoadLiteralMask = 0xFF000000,
  LDR_w_lit = LoadLiteralFixed | 0x00000000,
  LDR_x_lit = LoadLiteralFixed | 0x40000000,
  LDRSW_x_lit = LoadLiteralFixed | 0x80000000,
  PRFM_lit = LoadLiteralFixed | 0xC0000000,
  LDR_s_lit = LoadLiteralFixed | 0x04000000,
  LDR_d_lit = LoadLiteralFixed | 0x44000000
};

// clang-format off

#define LOAD_STORE_OP_LIST(V)  \
  V(ST, RB, w,  0x00000000),   \
  V(ST, RH, w,  0x40000000),   \
  V(ST, R, w,   0x80000000),   \
  V(ST, R, x,   0xC0000000),   \
  V(LD, RB, w,  0x00400000),   \
  V(LD, RH, w,  0x40400000),   \
  V(LD, R, w,   0x80400000),   \
  V(LD, R, x,   0xC0400000),   \
  V(LD, RSB, x, 0x00800000),   \
  V(LD, RSH, x, 0x40800000),   \
  V(LD, RSW, x, 0x80800000),   \
  V(LD, RSB, w, 0x00C00000),   \
  V(LD, RSH, w, 0x40C00000),   \
  V(ST, R, b,   0x04000000),   \
  V(ST, R, h,   0x44000000),   \
  V(ST, R, s,   0x84000000),   \
  V(ST, R, d,   0xC4000000),   \
  V(ST, R, q,   0x04800000),   \
  V(LD, R, b,   0x04400000),   \
  V(LD, R, h,   0x44400000),   \
  V(LD, R, s,   0x84400000),   \
  V(LD, R, d,   0xC4400000),   \
  V(LD, R, q,   0x04C00000)

// clang-format on

// Load/store unscaled offset.
enum LoadStoreUnscaledOffsetOp : uint32_t {
  LoadStoreUnscaledOffsetFixed = 0x38000000,
  LoadStoreUnscaledOffsetFMask = 0x3B200C00,
  LoadStoreUnscaledOffsetMask = 0xFFE00C00,
#define LOAD_STORE_UNSCALED(A, B, C, D) \
  A##U##B##_##C = LoadStoreUnscaledOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSCALED)
#undef LOAD_STORE_UNSCALED
};

// Load/store (post, pre, offset and unsigned.)
enum LoadStoreOp : uint32_t {
  LoadStoreMask = 0xC4C00000,
#define LOAD_STORE(A, B, C, D) A##B##_##C = D
  LOAD_STORE_OP_LIST(LOAD_STORE),
#undef LOAD_STORE
  PRFM = 0xC0800000
};

// Load/store post index.
enum LoadStorePostIndex : uint32_t {
  LoadStorePostIndexFixed = 0x38000400,
  LoadStorePostIndexFMask = 0x3B200C00,
  LoadStorePostIndexMask = 0xFFE00C00,
#define LOAD_STORE_POST_INDEX(A, B, C, D) \
  A##B##_##C##_post = LoadStorePostIndexFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_POST_INDEX)
#undef LOAD_STORE_POST_INDEX
};

// Load/store pre index.
enum LoadStorePreIndex : uint32_t {
  LoadStorePreIndexFixed = 0x38000C00,
  LoadStorePreIndexFMask = 0x3B200C00,
  LoadStorePreIndexMask = 0xFFE00C00,
#define LOAD_STORE_PRE_INDEX(A, B, C, D) \
  A##B##_##C##_pre = LoadStorePreIndexFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_PRE_INDEX)
#undef LOAD_STORE_PRE_INDEX
};

// Load/store unsigned offset.
enum LoadStoreUnsignedOffset : uint32_t {
  LoadStoreUnsignedOffsetFixed = 0x39000000,
  LoadStoreUnsignedOffsetFMask = 0x3B000000,
  LoadStoreUnsignedOffsetMask = 0xFFC00000,
  PRFM_unsigned = LoadStoreUnsignedOffsetFixed | PRFM,
#define LOAD_STORE_UNSIGNED_OFFSET(A, B, C, D) \
  A##B##_##C##_unsigned = LoadStoreUnsignedOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSIGNED_OFFSET)
#undef LOAD_STORE_UNSIGNED_OFFSET
};

// Load/store register offset.
enum LoadStoreRegisterOffset : uint32_t {
  LoadStoreRegisterOffsetFixed = 0x38200800,
  LoadStoreRegisterOffsetFMask = 0x3B200C00,
  LoadStoreRegisterOffsetMask = 0xFFE00C00,
  PRFM_reg = LoadStoreRegisterOffsetFixed | PRFM,
#define LOAD_STORE_REGISTER_OFFSET(A, B, C, D) \
  A##B##_##C##_reg = LoadStoreRegisterOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_REGISTER_OFFSET)
#undef LOAD_STORE_REGISTER_OFFSET
};

// Load/store acquire/release.
enum LoadStoreAcquireReleaseOp : uint32_t {
  LoadStoreAcquireReleaseFixed = 0x08000000,
  LoadStoreAcquireReleaseFMask = 0x3F000000,
  LoadStoreAcquireReleaseMask = 0xCFC08000,
  STLXR_b = LoadStoreAcquireReleaseFixed | 0x00008000,
  LDAXR_b = LoadStoreAcquireReleaseFixed | 0x00408000,
  STLR_b = LoadStoreAcquireReleaseFixed | 0x00808000,
  LDAR_b = LoadStoreAcquireReleaseFixed | 0x00C08000,
  STLXR_h = LoadStoreAcquireReleaseFixed | 0x40008000,
  LDAXR_h = LoadStoreAcquireReleaseFixed | 0x40408000,
  STLR_h = LoadStoreAcquireReleaseFixed | 0x40808000,
  LDAR_h = LoadStoreAcquireReleaseFixed | 0x40C08000,
  STLXR_w = LoadStoreAcquireReleaseFixed | 0x80008000,
  LDAXR_w = LoadStoreAcquireReleaseFixed | 0x80408000,
  STLR_w = LoadStoreAcquireReleaseFixed | 0x80808000,
  LDAR_w = LoadStoreAcquireReleaseFixed | 0x80C08000,
  STLXR_x = LoadStoreAcquireReleaseFixed | 0xC0008000,
  LDAXR_x = LoadStoreAcquireReleaseFixed | 0xC0408000,
  STLR_x = LoadStoreAcquireReleaseFixed | 0xC0808000,
  LDAR_x = LoadStoreAcquireReleaseFixed | 0xC0C08000,
};

// Conditional compare.
enum ConditionalCompareOp : uint32_t {
  ConditionalCompareMask = 0x60000000,
  CCMN = 0x20000000,
  CCMP = 0x60000000
};

// Conditional compare register.
enum ConditionalCompareRegisterOp : uint32_t {
  ConditionalCompareRegisterFixed = 0x1A400000,
  ConditionalCompareRegisterFMask = 0x1FE00800,
  ConditionalCompareRegisterMask = 0xFFE00C10,
  CCMN_w = ConditionalCompareRegisterFixed | CCMN,
  CCMN_x = ConditionalCompareRegisterFixed | SixtyFourBits | CCMN,
  CCMP_w = ConditionalCompareRegisterFixed | CCMP,
  CCMP_x = ConditionalCompareRegisterFixed | SixtyFourBits | CCMP
};

// Conditional compare immediate.
enum ConditionalCompareImmediateOp : uint32_t {
  ConditionalCompareImmediateFixed = 0x1A400800,
  ConditionalCompareImmediateFMask = 0x1FE00800,
  ConditionalCompareImmediateMask = 0xFFE00C10,
  CCMN_w_imm = ConditionalCompareImmediateFixed | CCMN,
  CCMN_x_imm = ConditionalCompareImmediateFixed | SixtyFourBits | CCMN,
  CCMP_w_imm = ConditionalCompareImmediateFixed | CCMP,
  CCMP_x_imm = ConditionalCompareImmediateFixed | SixtyFourBits | CCMP
};

// Conditional select.
enum ConditionalSelectOp : uint32_t {
  ConditionalSelectFixed = 0x1A800000,
  ConditionalSelectFMask = 0x1FE00000,
  ConditionalSelectMask = 0xFFE00C00,
  CSEL_w = ConditionalSelectFixed | 0x00000000,
  CSEL_x = ConditionalSelectFixed | 0x80000000,
  CSEL = CSEL_w,
  CSINC_w = ConditionalSelectFixed | 0x00000400,
  CSINC_x = ConditionalSelectFixed | 0x80000400,
  CSINC = CSINC_w,
  CSINV_w = ConditionalSelectFixed | 0x40000000,
  CSINV_x = ConditionalSelectFixed | 0xC0000000,
  CSINV = CSINV_w,
  CSNEG_w = ConditionalSelectFixed | 0x40000400,
  CSNEG_x = ConditionalSelectFixed | 0xC0000400,
  CSNEG = CSNEG_w
};

// Data processing 1 source.
enum DataProcessing1SourceOp : uint32_t {
  DataProcessing1SourceFixed = 0x5AC00000,
  DataProcessing1SourceFMask = 0x5FE00000,
  DataProcessing1SourceMask = 0xFFFFFC00,
  RBIT = DataProcessing1SourceFixed | 0x00000000,
  RBIT_w = RBIT,
  RBIT_x = RBIT | SixtyFourBits,
  REV16 = DataProcessing1SourceFixed | 0x00000400,
  REV16_w = REV16,
  REV16_x = REV16 | SixtyFourBits,
  REV = DataProcessing1SourceFixed | 0x00000800,
  REV_w = REV,
  REV32_x = REV | SixtyFourBits,
  REV_x = DataProcessing1SourceFixed | SixtyFourBits | 0x00000C00,
  CLZ = DataProcessing1SourceFixed | 0x00001000,
  CLZ_w = CLZ,
  CLZ_x = CLZ | SixtyFourBits,
  CLS = DataProcessing1SourceFixed | 0x00001400,
  CLS_w = CLS,
  CLS_x = CLS | SixtyFourBits
};

// Data processing 2 source.
enum DataProcessing2SourceOp : uint32_t {
  DataProcessing2SourceFixed = 0x1AC00000,
  DataProcessing2SourceFMask = 0x5FE00000,
  DataProcessing2SourceMask = 0xFFE0FC00,
  UDIV_w = DataProcessing2SourceFixed | 0x00000800,
  UDIV_x = DataProcessing2SourceFixed | 0x80000800,
  UDIV = UDIV_w,
  SDIV_w = DataProcessing2SourceFixed | 0x00000C00,
  SDIV_x = DataProcessing2SourceFixed | 0x80000C00,
  SDIV = SDIV_w,
  LSLV_w = DataProcessing2SourceFixed | 0x00002000,
  LSLV_x = DataProcessing2SourceFixed | 0x80002000,
  LSLV = LSLV_w,
  LSRV_w = DataProcessing2SourceFixed | 0x00002400,
  LSRV_x = DataProcessing2SourceFixed | 0x80002400,
  LSRV = LSRV_w,
  ASRV_w = DataProcessing2SourceFixed | 0x00002800,
  ASRV_x = DataProcessing2SourceFixed | 0x80002800,
  ASRV = ASRV_w,
  RORV_w = DataProcessing2SourceFixed | 0x00002C00,
  RORV_x = DataProcessing2SourceFixed | 0x80002C00,
  RORV = RORV_w,
  CRC32B = DataProcessing2SourceFixed | 0x00004000,
  CRC32H = DataProcessing2SourceFixed | 0x00004400,
  CRC32W = DataProcessing2SourceFixed | 0x00004800,
  CRC32X = DataProcessing2SourceFixed | SixtyFourBits | 0x00004C00,
  CRC32CB = DataProcessing2SourceFixed | 0x00005000,
  CRC32CH = DataProcessing2SourceFixed | 0x00005400,
  CRC32CW = DataProcessing2SourceFixed | 0x00005800,
  CRC32CX = DataProcessing2SourceFixed | SixtyFourBits | 0x00005C00
};

// Data processing 3 source.
enum DataProcessing3SourceOp : uint32_t {
  DataProcessing3SourceFixed = 0x1B000000,
  DataProcessing3SourceFMask = 0x1F000000,
  DataProcessing3SourceMask = 0xFFE08000,
  MADD_w = DataProcessing3SourceFixed | 0x00000000,
  MADD_x = DataProcessing3SourceFixed | 0x80000000,
  MADD = MADD_w,
  MSUB_w = DataProcessing3SourceFixed | 0x00008000,
  MSUB_x = DataProcessing3SourceFixed | 0x80008000,
  MSUB = MSUB_w,
  SMADDL_x = DataProcessing3SourceFixed | 0x80200000,
  SMSUBL_x = DataProcessing3SourceFixed | 0x80208000,
  SMULH_x = DataProcessing3SourceFixed | 0x80400000,
  UMADDL_x = DataProcessing3SourceFixed | 0x80A00000,
  UMSUBL_x = DataProcessing3SourceFixed | 0x80A08000,
  UMULH_x = DataProcessing3SourceFixed | 0x80C00000
};

// Floating point compare.
enum FPCompareOp : uint32_t {
  FPCompareFixed = 0x1E202000,
  FPCompareFMask = 0x5F203C00,
  FPCompareMask = 0xFFE0FC1F,
  FCMP_s = FPCompareFixed | 0x00000000,
  FCMP_d = FPCompareFixed | FP64 | 0x00000000,
  FCMP = FCMP_s,
  FCMP_s_zero = FPCompareFixed | 0x00000008,
  FCMP_d_zero = FPCompareFixed | FP64 | 0x00000008,
  FCMP_zero = FCMP_s_zero,
  FCMPE_s = FPCompareFixed | 0x00000010,
  FCMPE_d = FPCompareFixed | FP64 | 0x00000010,
  FCMPE_s_zero = FPCompareFixed | 0x00000018,
  FCMPE_d_zero = FPCompareFixed | FP64 | 0x00000018
};

// Floating point conditional compare.
enum FPConditionalCompareOp : uint32_t {
  FPConditionalCompareFixed = 0x1E200400,
  FPConditionalCompareFMask = 0x5F200C00,
  FPConditionalCompareMask = 0xFFE00C10,
  FCCMP_s = FPConditionalCompareFixed | 0x00000000,
  FCCMP_d = FPConditionalCompareFixed | FP64 | 0x00000000,
  FCCMP = FCCMP_s,
  FCCMPE_s = FPConditionalCompareFixed | 0x00000010,
  FCCMPE_d = FPConditionalCompareFixed | FP64 | 0x00000010,
  FCCMPE = FCCMPE_s
};

// Floating point conditional select.
enum FPConditionalSelectOp : uint32_t {
  FPConditionalSelectFixed = 0x1E200C00,
  FPConditionalSelectFMask = 0x5F200C00,
  FPConditionalSelectMask = 0xFFE00C00,
  FCSEL_s = FPConditionalSelectFixed | 0x00000000,
  FCSEL_d = FPConditionalSelectFixed | FP64 | 0x00000000,
  FCSEL = FCSEL_s
};

// Floating point immediate.
enum FPImmediateOp : uint32_t {
  FPImmediateFixed = 0x1E201000,
  FPImmediateFMask = 0x5F201C00,
  FPImmediateMask = 0xFFE01C00,
  FMOV_s_imm = FPImmediateFixed | 0x00000000,
  FMOV_d_imm = FPImmediateFixed | FP64 | 0x00000000
};

// Floating point data processing 1 source.
enum FPDataProcessing1SourceOp : uint32_t {
  FPDataProcessing1SourceFixed = 0x1E204000,
  FPDataProcessing1SourceFMask = 0x5F207C00,
  FPDataProcessing1SourceMask = 0xFFFFFC00,
  FMOV_s = FPDataProcessing1SourceFixed | 0x00000000,
  FMOV_d = FPDataProcessing1SourceFixed | FP64 | 0x00000000,
  FMOV = FMOV_s,
  FABS_s = FPDataProcessing1SourceFixed | 0x00008000,
  FABS_d = FPDataProcessing1SourceFixed | FP64 | 0x00008000,
  FABS = FABS_s,
  FNEG_s = FPDataProcessing1SourceFixed | 0x00010000,
  FNEG_d = FPDataProcessing1SourceFixed | FP64 | 0x00010000,
  FNEG = FNEG_s,
  FSQRT_s = FPDataProcessing1SourceFixed | 0x00018000,
  FSQRT_d = FPDataProcessing1SourceFixed | FP64 | 0x00018000,
  FSQRT = FSQRT_s,
  FCVT_ds = FPDataProcessing1SourceFixed | 0x00028000,
  FCVT_sd = FPDataProcessing1SourceFixed | FP64 | 0x00020000,
  FCVT_hs = FPDataProcessing1SourceFixed | 0x00038000,
  FCVT_hd = FPDataProcessing1SourceFixed | FP64 | 0x00038000,
  FCVT_sh = FPDataProcessing1SourceFixed | 0x00C20000,
  FCVT_dh = FPDataProcessing1SourceFixed | 0x00C28000,
  FRINTN_s = FPDataProcessing1SourceFixed | 0x00040000,
  FRINTN_d = FPDataProcessing1SourceFixed | FP64 | 0x00040000,
  FRINTN = FRINTN_s,
  FRINTP_s = FPDataProcessing1SourceFixed | 0x00048000,
  FRINTP_d = FPDataProcessing1SourceFixed | FP64 | 0x00048000,
  FRINTP = FRINTP_s,
  FRINTM_s = FPDataProcessing1SourceFixed | 0x00050000,
  FRINTM_d = FPDataProcessing1SourceFixed | FP64 | 0x00050000,
  FRINTM = FRINTM_s,
  FRINTZ_s = FPDataProcessing1SourceFixed | 0x00058000,
  FRINTZ_d = FPDataProcessing1SourceFixed | FP64 | 0x00058000,
  FRINTZ = FRINTZ_s,
  FRINTA_s = FPDataProcessing1SourceFixed | 0x00060000,
  FRINTA_d = FPDataProcessing1SourceFixed | FP64 | 0x00060000,
  FRINTA = FRINTA_s,
  FRINTX_s = FPDataProcessing1SourceFixed | 0x00070000,
  FRINTX_d = FPDataProcessing1SourceFixed | FP64 | 0x00070000,
  FRINTX = FRINTX_s,
  FRINTI_s = FPDataProcessing1SourceFixed | 0x00078000,
  FRINTI_d = FPDataProcessing1SourceFixed | FP64 | 0x00078000,
  FRINTI = FRINTI_s
};

// Floating point data processing 2 source.
enum FPDataProcessing2SourceOp : uint32_t {
  FPDataProcessing2SourceFixed = 0x1E200800,
  FPDataProcessing2SourceFMask = 0x5F200C00,
  FPDataProcessing2SourceMask = 0xFFE0FC00,
  FMUL = FPDataProcessing2SourceFixed | 0x00000000,
  FMUL_s = FMUL,
  FMUL_d = FMUL | FP64,
  FDIV = FPDataProcessing2SourceFixed | 0x00001000,
  FDIV_s = FDIV,
  FDIV_d = FDIV | FP64,
  FADD = FPDataProcessing2SourceFixed | 0x00002000,
  FADD_s = FADD,
  FADD_d = FADD | FP64,
  FSUB = FPDataProcessing2SourceFixed | 0x00003000,
  FSUB_s = FSUB,
  FSUB_d = FSUB | FP64,
  FMAX = FPDataProcessing2SourceFixed | 0x00004000,
  FMAX_s = FMAX,
  FMAX_d = FMAX | FP64,
  FMIN = FPDataProcessing2SourceFixed | 0x00005000,
  FMIN_s = FMIN,
  FMIN_d = FMIN | FP64,
  FMAXNM = FPDataProcessing2SourceFixed | 0x00006000,
  FMAXNM_s = FMAXNM,
  FMAXNM_d = FMAXNM | FP64,
  FMINNM = FPDataProcessing2SourceFixed | 0x00007000,
  FMINNM_s = FMINNM,
  FMINNM_d = FMINNM | FP64,
  FNMUL = FPDataProcessing2SourceFixed | 0x00008000,
  FNMUL_s = FNMUL,
  FNMUL_d = FNMUL | FP64
};

// Floating point data processing 3 source.
enum FPDataProcessing3SourceOp : uint32_t {
  FPDataProcessing3SourceFixed = 0x1F000000,
  FPDataProcessing3SourceFMask = 0x5F000000,
  FPDataProcessing3SourceMask = 0xFFE08000,
  FMADD_s = FPDataProcessing3SourceFixed | 0x00000000,
  FMSUB_s = FPDataProcessing3SourceFixed | 0x00008000,
  FNMADD_s = FPDataProcessing3SourceFixed | 0x00200000,
  FNMSUB_s = FPDataProcessing3SourceFixed | 0x00208000,
  FMADD_d = FPDataProcessing3SourceFixed | 0x00400000,
  FMSUB_d = FPDataProcessing3SourceFixed | 0x00408000,
  FNMADD_d = FPDataProcessing3SourceFixed | 0x00600000,
  FNMSUB_d = FPDataProcessing3SourceFixed | 0x00608000
};

// Conversion between floating point and integer.
enum FPIntegerConvertOp : uint32_t {
  FPIntegerConvertFixed = 0x1E200000,
  FPIntegerConvertFMask = 0x5F20FC00,
  FPIntegerConvertMask = 0xFFFFFC00,
  FCVTNS = FPIntegerConvertFixed | 0x00000000,
  FCVTNS_ws = FCVTNS,
  FCVTNS_xs = FCVTNS | SixtyFourBits,
  FCVTNS_wd = FCVTNS | FP64,
  FCVTNS_xd = FCVTNS | SixtyFourBits | FP64,
  FCVTNU = FPIntegerConvertFixed | 0x00010000,
  FCVTNU_ws = FCVTNU,
  FCVTNU_xs = FCVTNU | SixtyFourBits,
  FCVTNU_wd = FCVTNU | FP64,
  FCVTNU_xd = FCVTNU | SixtyFourBits | FP64,
  FCVTPS = FPIntegerConvertFixed | 0x00080000,
  FCVTPS_ws = FCVTPS,
  FCVTPS_xs = FCVTPS | SixtyFourBits,
  FCVTPS_wd = FCVTPS | FP64,
  FCVTPS_xd = FCVTPS | SixtyFourBits | FP64,
  FCVTPU = FPIntegerConvertFixed | 0x00090000,
  FCVTPU_ws = FCVTPU,
  FCVTPU_xs = FCVTPU | SixtyFourBits,
  FCVTPU_wd = FCVTPU | FP64,
  FCVTPU_xd = FCVTPU | SixtyFourBits | FP64,
  FCVTMS = FPIntegerConvertFixed | 0x00100000,
  FCVTMS_ws = FCVTMS,
  FCVTMS_xs = FCVTMS | SixtyFourBits,
  FCVTMS_wd = FCVTMS | FP64,
  FCVTMS_xd = FCVTMS | SixtyFourBits | FP64,
  FCVTMU = FPIntegerConvertFixed | 0x00110000,
  FCVTMU_ws = FCVTMU,
  FCVTMU_xs = FCVTMU | SixtyFourBits,
  FCVTMU_wd = FCVTMU | FP64,
  FCVTMU_xd = FCVTMU | SixtyFourBits | FP64,
  FCVTZS = FPIntegerConvertFixed | 0x00180000,
  FCVTZS_ws = FCVTZS,
  FCVTZS_xs = FCVTZS | SixtyFourBits,
  FCVTZS_wd = FCVTZS | FP64,
  FCVTZS_xd = FCVTZS | SixtyFourBits | FP64,
  FCVTZU = FPIntegerConvertFixed | 0x00190000,
  FCVTZU_ws = FCVTZU,
  FCVTZU_xs = FCVTZU | SixtyFourBits,
  FCVTZU_wd = FCVTZU | FP64,
  FCVTZU_xd = FCVTZU | SixtyFourBits | FP64,
  SCVTF = FPIntegerConvertFixed | 0x00020000,
  SCVTF_sw = SCVTF,
  SCVTF_sx = SCVTF | SixtyFourBits,
  SCVTF_dw = SCVTF | FP64,
  SCVTF_dx = SCVTF | SixtyFourBits | FP64,
  UCVTF = FPIntegerConvertFixed | 0x00030000,
  UCVTF_sw = UCVTF,
  UCVTF_sx = UCVTF | SixtyFourBits,
  UCVTF_dw = UCVTF | FP64,
  UCVTF_dx = UCVTF | SixtyFourBits | FP64,
  FCVTAS = FPIntegerConvertFixed | 0x00040000,
  FCVTAS_ws = FCVTAS,
  FCVTAS_xs = FCVTAS | SixtyFourBits,
  FCVTAS_wd = FCVTAS | FP64,
  FCVTAS_xd = FCVTAS | SixtyFourBits | FP64,
  FCVTAU = FPIntegerConvertFixed | 0x00050000,
  FCVTAU_ws = FCVTAU,
  FCVTAU_xs = FCVTAU | SixtyFourBits,
  FCVTAU_wd = FCVTAU | FP64,
  FCVTAU_xd = FCVTAU | SixtyFourBits | FP64,
  FMOV_ws = FPIntegerConvertFixed | 0x00060000,
  FMOV_sw = FPIntegerConvertFixed | 0x00070000,
  FMOV_xd = FMOV_ws | SixtyFourBits | FP64,
  FMOV_dx = FMOV_sw | SixtyFourBits | FP64,
  FMOV_d1_x = FPIntegerConvertFixed | SixtyFourBits | 0x008F0000,
  FMOV_x_d1 = FPIntegerConvertFixed | SixtyFourBits | 0x008E0000
};

// Conversion between fixed point and floating point.
enum FPFixedPointConvertOp : uint32_t {
  FPFixedPointConvertFixed = 0x1E000000,
  FPFixedPointConvertFMask = 0x5F200000,
  FPFixedPointConvertMask = 0xFFFF0000,
  FCVTZS_fixed = FPFixedPointConvertFixed | 0x00180000,
  FCVTZS_ws_fixed = FCVTZS_fixed,
  FCVTZS_xs_fixed = FCVTZS_fixed | SixtyFourBits,
  FCVTZS_wd_fixed = FCVTZS_fixed | FP64,
  FCVTZS_xd_fixed = FCVTZS_fixed | SixtyFourBits | FP64,
  FCVTZU_fixed = FPFixedPointConvertFixed | 0x00190000,
  FCVTZU_ws_fixed = FCVTZU_fixed,
  FCVTZU_xs_fixed = FCVTZU_fixed | SixtyFourBits,
  FCVTZU_wd_fixed = FCVTZU_fixed | FP64,
  FCVTZU_xd_fixed = FCVTZU_fixed | SixtyFourBits | FP64,
  SCVTF_fixed = FPFixedPointConvertFixed | 0x00020000,
  SCVTF_sw_fixed = SCVTF_fixed,
  SCVTF_sx_fixed = SCVTF_fixed | SixtyFourBits,
  SCVTF_dw_fixed = SCVTF_fixed | FP64,
  SCVTF_dx_fixed = SCVTF_fixed | SixtyFourBits | FP64,
  UCVTF_fixed = FPFixedPointConvertFixed | 0x00030000,
  UCVTF_sw_fixed = UCVTF_fixed,
  UCVTF_sx_fixed = UCVTF_fixed | SixtyFourBits,
  UCVTF_dw_fixed = UCVTF_fixed | FP64,
  UCVTF_dx_fixed = UCVTF_fixed | SixtyFourBits | FP64
};

// NEON instructions with two register operands.
enum NEON2RegMiscOp : uint32_t {
  NEON2RegMiscFixed = 0x0E200800,
  NEON2RegMiscFMask = 0x9F3E0C00,
  NEON2RegMiscMask = 0xBF3FFC00,
  NEON2RegMiscUBit = 0x20000000,
  NEON_REV64 = NEON2RegMiscFixed | 0x00000000,
  NEON_REV32 = NEON2RegMiscFixed | 0x20000000,
  NEON_REV16 = NEON2RegMiscFixed | 0x00001000,
  NEON_SADDLP = NEON2RegMiscFixed | 0x00002000,
  NEON_UADDLP = NEON_SADDLP | NEON2RegMiscUBit,
  NEON_SUQADD = NEON2RegMiscFixed | 0x00003000,
  NEON_USQADD = NEON_SUQADD | NEON2RegMiscUBit,
  NEON_CLS = NEON2RegMiscFixed | 0x00004000,
  NEON_CLZ = NEON2RegMiscFixed | 0x20004000,
  NEON_CNT = NEON2RegMiscFixed | 0x00005000,
  NEON_RBIT_NOT = NEON2RegMiscFixed | 0x20005000,
  NEON_SADALP = NEON2RegMiscFixed | 0x00006000,
  NEON_UADALP = NEON_SADALP | NEON2RegMiscUBit,
  NEON_SQABS = NEON2RegMiscFixed | 0x00007000,
  NEON_SQNEG = NEON2RegMiscFixed | 0x20007000,
  NEON_CMGT_zero = NEON2RegMiscFixed | 0x00008000,
  NEON_CMGE_zero = NEON2RegMiscFixed | 0x20008000,
  NEON_CMEQ_zero = NEON2RegMiscFixed | 0x00009000,
  NEON_CMLE_zero = NEON2RegMiscFixed | 0x20009000,
  NEON_CMLT_zero = NEON2RegMiscFixed | 0x0000A000,
  NEON_ABS = NEON2RegMiscFixed | 0x0000B000,
  NEON_NEG = NEON2RegMiscFixed | 0x2000B000,
  NEON_XTN = NEON2RegMiscFixed | 0x00012000,
  NEON_SQXTUN = NEON2RegMiscFixed | 0x20012000,
  NEON_SHLL = NEON2RegMiscFixed | 0x20013000,
  NEON_SQXTN = NEON2RegMiscFixed | 0x00014000,
  NEON_UQXTN = NEON_SQXTN | NEON2RegMiscUBit,

  NEON2RegMiscOpcode = 0x0001F000,
  NEON_RBIT_NOT_opcode = NEON_RBIT_NOT & NEON2RegMiscOpcode,
  NEON_NEG_opcode = NEON_NEG & NEON2RegMiscOpcode,
  NEON_XTN_opcode = NEON_XTN & NEON2RegMiscOpcode,
  NEON_UQXTN_opcode = NEON_UQXTN & NEON2RegMiscOpcode,

  // These instructions use only one bit of the size field. The other bit is
  // used to distinguish between instructions.
  NEON2RegMiscFPMask = NEON2RegMiscMask | 0x00800000,
  NEON_FABS = NEON2RegMiscFixed | 0x0080F000,
  NEON_FNEG = NEON2RegMiscFixed | 0x2080F000,
  NEON_FCVTN = NEON2RegMiscFixed | 0x00016000,
  NEON_FCVTXN = NEON2RegMiscFixed | 0x20016000,
  NEON_FCVTL = NEON2RegMiscFixed | 0x00017000,
  NEON_FRINTN = NEON2RegMiscFixed | 0x00018000,
  NEON_FRINTA = NEON2RegMiscFixed | 0x20018000,
  NEON_FRINTP = NEON2RegMiscFixed | 0x00818000,
  NEON_FRINTM = NEON2RegMiscFixed | 0x00019000,
  NEON_FRINTX = NEON2RegMiscFixed | 0x20019000,
  NEON_FRINTZ = NEON2RegMiscFixed | 0x00819000,
  NEON_FRINTI = NEON2RegMiscFixed | 0x20819000,
  NEON_FCVTNS = NEON2RegMiscFixed | 0x0001A000,
  NEON_FCVTNU = NEON_FCVTNS | NEON2RegMiscUBit,
  NEON_FCVTPS = NEON2RegMiscFixed | 0x0081A000,
  NEON_FCVTPU = NEON_FCVTPS | NEON2RegMiscUBit,
  NEON_FCVTMS = NEON2RegMiscFixed | 0x0001B000,
  NEON_FCVTMU = NEON_FCVTMS | NEON2RegMiscUBit,
  NEON_FCVTZS = NEON2RegMiscFixed | 0x0081B000,
  NEON_FCVTZU = NEON_FCVTZS | NEON2RegMiscUBit,
  NEON_FCVTAS = NEON2RegMiscFixed | 0x0001C000,
  NEON_FCVTAU = NEON_FCVTAS | NEON2RegMiscUBit,
  NEON_FSQRT = NEON2RegMiscFixed | 0x2081F000,
  NEON_SCVTF = NEON2RegMiscFixed | 0x0001D000,
  NEON_UCVTF = NEON_SCVTF | NEON2RegMiscUBit,
  NEON_URSQRTE = NEON2RegMiscFixed | 0x2081C000,
  NEON_URECPE = NEON2RegMiscFixed | 0x0081C000,
  NEON_FRSQRTE = NEON2RegMiscFixed | 0x2081D000,
  NEON_FRECPE = NEON2RegMiscFixed | 0x0081D000,
  NEON_FCMGT_zero = NEON2RegMiscFixed | 0x0080C000,
  NEON_FCMGE_zero = NEON2RegMiscFixed | 0x2080C000,
  NEON_FCMEQ_zero = NEON2RegMiscFixed | 0x0080D000,
  NEON_FCMLE_zero = NEON2RegMiscFixed | 0x2080D000,
  NEON_FCMLT_zero = NEON2RegMiscFixed | 0x0080E000,

  NEON_FCVTL_opcode = NEON_FCVTL & NEON2RegMiscOpcode,
  NEON_FCVTN_opcode = NEON_FCVTN & NEON2RegMiscOpcode
};

// NEON instructions with three same-type operands.
enum NEON3SameOp : uint32_t {
  NEON3SameFixed = 0x0E200400,
  NEON3SameFMask = 0x9F200400,
  NEON3SameMask = 0xBF20FC00,
  NEON3SameUBit = 0x20000000,
  NEON_ADD = NEON3SameFixed | 0x00008000,
  NEON_ADDP = NEON3SameFixed | 0x0000B800,
  NEON_SHADD = NEON3SameFixed | 0x00000000,
  NEON_SHSUB = NEON3SameFixed | 0x00002000,
  NEON_SRHADD = NEON3SameFixed | 0x00001000,
  NEON_CMEQ = NEON3SameFixed | NEON3SameUBit | 0x00008800,
  NEON_CMGE = NEON3SameFixed | 0x00003800,
  NEON_CMGT = NEON3SameFixed | 0x00003000,
  NEON_CMHI = NEON3SameFixed | NEON3SameUBit | NEON_CMGT,
  NEON_CMHS = NEON3SameFixed | NEON3SameUBit | NEON_CMGE,
  NEON_CMTST = NEON3SameFixed | 0x00008800,
  NEON_MLA = NEON3SameFixed | 0x00009000,
  NEON_MLS = NEON3SameFixed | 0x20009000,
  NEON_MUL = NEON3SameFixed | 0x00009800,
  NEON_PMUL = NEON3SameFixed | 0x20009800,
  NEON_SRSHL = NEON3SameFixed | 0x00005000,
  NEON_SQSHL = NEON3SameFixed | 0x00004800,
  NEON_SQRSHL = NEON3SameFixed | 0x00005800,
  NEON_SSHL = NEON3SameFixed | 0x00004000,
  NEON_SMAX = NEON3SameFixed | 0x00006000,
  NEON_SMAXP = NEON3SameFixed | 0x0000A000,
  NEON_SMIN = NEON3SameFixed | 0x00006800,
  NEON_SMINP = NEON3SameFixed | 0x0000A800,
  NEON_SABD = NEON3SameFixed | 0x00007000,
  NEON_SABA = NEON3SameFixed | 0x00007800,
  NEON_UABD = NEON3SameFixed | NEON3SameUBit | NEON_SABD,
  NEON_UABA = NEON3SameFixed | NEON3SameUBit | NEON_SABA,
  NEON_SQADD = NEON3SameFixed | 0x00000800,
  NEON_SQSUB = NEON3SameFixed | 0x00002800,
  NEON_SUB = NEON3SameFixed | NEON3SameUBit | 0x00008000,
  NEON_UHADD = NEON3SameFixed | NEON3SameUBit | NEON_SHADD,
  NEON_UHSUB = NEON3SameFixed | NEON3SameUBit | NEON_SHSUB,
  NEON_URHADD = NEON3SameFixed | NEON3SameUBit | NEON_SRHADD,
  NEON_UMAX = NEON3SameFixed | NEON3SameUBit | NEON_SMAX,
  NEON_UMAXP = NEON3SameFixed | NEON3SameUBit | NEON_SMAXP,
  NEON_UMIN = NEON3SameFixed | NEON3SameUBit | NEON_SMIN,
  NEON_UMINP = NEON3SameFixed | NEON3SameUBit | NEON_SMINP,
  NEON_URSHL = NEON3SameFixed | NEON3SameUBit | NEON_SRSHL,
  NEON_UQADD = NEON3SameFixed | NEON3SameUBit | NEON_SQADD,
  NEON_UQRSHL = NEON3SameFixed | NEON3SameUBit | NEON_SQRSHL,
  NEON_UQSHL = NEON3SameFixed | NEON3SameUBit | NEON_SQSHL,
  NEON_UQSUB = NEON3SameFixed | NEON3SameUBit | NEON_SQSUB,
  NEON_USHL = NEON3SameFixed | NEON3SameUBit | NEON_SSHL,
  NEON_SQDMULH = NEON3SameFixed | 0x0000B000,
  NEON_SQRDMULH = NEON3SameFixed | 0x2000B000,

  // NEON floating point instructions with three same-type operands.
  NEON3SameFPFixed = NEON3SameFixed | 0x0000C000,
  NEON3SameFPFMask = NEON3SameFMask | 0x0000C000,
  NEON3SameFPMask = NEON3SameMask | 0x00800000,
  NEON_FADD = NEON3SameFixed | 0x0000D000,
  NEON_FSUB = NEON3SameFixed | 0x0080D000,
  NEON_FMUL = NEON3SameFixed | 0x2000D800,
  NEON_FDIV = NEON3SameFixed | 0x2000F800,
  NEON_FMAX = NEON3SameFixed | 0x0000F000,
  NEON_FMAXNM = NEON3SameFixed | 0x0000C000,
  NEON_FMAXP = NEON3SameFixed | 0x2000F000,
  NEON_FMAXNMP = NEON3SameFixed | 0x2000C000,
  NEON_FMIN = NEON3SameFixed | 0x0080F000,
  NEON_FMINNM = NEON3SameFixed | 0x0080C000,
  NEON_FMINP = NEON3SameFixed | 0x2080F000,
  NEON_FMINNMP = NEON3SameFixed | 0x2080C000,
  NEON_FMLA = NEON3SameFixed | 0x0000C800,
  NEON_FMLS = NEON3SameFixed | 0x0080C800,
  NEON_FMULX = NEON3SameFixed | 0x0000D800,
  NEON_FRECPS = NEON3SameFixed | 0x0000F800,
  NEON_FRSQRTS = NEON3SameFixed | 0x0080F800,
  NEON_FABD = NEON3SameFixed | 0x2080D000,
  NEON_FADDP = NEON3SameFixed | 0x2000D000,
  NEON_FCMEQ = NEON3SameFixed | 0x0000E000,
  NEON_FCMGE = NEON3SameFixed | 0x2000E000,
  NEON_FCMGT = NEON3SameFixed | 0x2080E000,
  NEON_FACGE = NEON3SameFixed | 0x2000E800,
  NEON_FACGT = NEON3SameFixed | 0x2080E800,

  // NEON logical instructions with three same-type operands.
  NEON3SameLogicalFixed = NEON3SameFixed | 0x00001800,
  NEON3SameLogicalFMask = NEON3SameFMask | 0x0000F800,
  NEON3SameLogicalMask = 0xBFE0FC00,
  NEON3SameLogicalFormatMask = NEON_Q,
  NEON_AND = NEON3SameLogicalFixed | 0x00000000,
  NEON_ORR = NEON3SameLogicalFixed | 0x00A00000,
  NEON_ORN = NEON3SameLogicalFixed | 0x00C00000,
  NEON_EOR = NEON3SameLogicalFixed | 0x20000000,
  NEON_BIC = NEON3SameLogicalFixed | 0x00400000,
  NEON_BIF = NEON3SameLogicalFixed | 0x20C00000,
  NEON_BIT = NEON3SameLogicalFixed | 0x20800000,
  NEON_BSL = NEON3SameLogicalFixed | 0x20400000
};

// NEON instructions with three different-type operands.
enum NEON3DifferentOp : uint32_t {
  NEON3DifferentFixed = 0x0E200000,
  NEON3DifferentFMask = 0x9F200C00,
  NEON3DifferentMask = 0xFF20FC00,
  NEON_ADDHN = NEON3DifferentFixed | 0x00004000,
  NEON_ADDHN2 = NEON_ADDHN | NEON_Q,
  NEON_PMULL = NEON3DifferentFixed | 0x0000E000,
  NEON_PMULL2 = NEON_PMULL | NEON_Q,
  NEON_RADDHN = NEON3DifferentFixed | 0x20004000,
  NEON_RADDHN2 = NEON_RADDHN | NEON_Q,
  NEON_RSUBHN = NEON3DifferentFixed | 0x20006000,
  NEON_RSUBHN2 = NEON_RSUBHN | NEON_Q,
  NEON_SABAL = NEON3DifferentFixed | 0x00005000,
  NEON_SABAL2 = NEON_SABAL | NEON_Q,
  NEON_SABDL = NEON3DifferentFixed | 0x00007000,
  NEON_SABDL2 = NEON_SABDL | NEON_Q,
  NEON_SADDL = NEON3DifferentFixed | 0x00000000,
  NEON_SADDL2 = NEON_SADDL | NEON_Q,
  NEON_SADDW = NEON3DifferentFixed | 0x00001000,
  NEON_SADDW2 = NEON_SADDW | NEON_Q,
  NEON_SMLAL = NEON3DifferentFixed | 0x00008000,
  NEON_SMLAL2 = NEON_SMLAL | NEON_Q,
  NEON_SMLSL = NEON3DifferentFixed | 0x0000A000,
  NEON_SMLSL2 = NEON_SMLSL | NEON_Q,
  NEON_SMULL = NEON3DifferentFixed | 0x0000C000,
  NEON_SMULL2 = NEON_SMULL | NEON_Q,
  NEON_SSUBL = NEON3DifferentFixed | 0x00002000,
  NEON_SSUBL2 = NEON_SSUBL | NEON_Q,
  NEON_SSUBW = NEON3DifferentFixed | 0x00003000,
  NEON_SSUBW2 = NEON_SSUBW | NEON_Q,
  NEON_SQDMLAL = NEON3DifferentFixed | 0x00009000,
  NEON_SQDMLAL2 = NEON_SQDMLAL | NEON_Q,
  NEON_SQDMLSL = NEON3DifferentFixed | 0x0000B000,
  NEON_SQDMLSL2 = NEON_SQDMLSL | NEON_Q,
  NEON_SQDMULL = NEON3DifferentFixed | 0x0000D000,
  NEON_SQDMULL2 = NEON_SQDMULL | NEON_Q,
  NEON_SUBHN = NEON3DifferentFixed | 0x00006000,
  NEON_SUBHN2 = NEON_SUBHN | NEON_Q,
  NEON_UABAL = NEON_SABAL | NEON3SameUBit,
  NEON_UABAL2 = NEON_UABAL | NEON_Q,
  NEON_UABDL = NEON_SABDL | NEON3SameUBit,
  NEON_UABDL2 = NEON_UABDL | NEON_Q,
  NEON_UADDL = NEON_SADDL | NEON3SameUBit,
  NEON_UADDL2 = NEON_UADDL | NEON_Q,
  NEON_UADDW = NEON_SADDW | NEON3SameUBit,
  NEON_UADDW2 = NEON_UADDW | NEON_Q,
  NEON_UMLAL = NEON_SMLAL | NEON3SameUBit,
  NEON_UMLAL2 = NEON_UMLAL | NEON_Q,
  NEON_UMLSL = NEON_SMLSL | NEON3SameUBit,
  NEON_UMLSL2 = NEON_UMLSL | NEON_Q,
  NEON_UMULL = NEON_SMULL | NEON3SameUBit,
  NEON_UMULL2 = NEON_UMULL | NEON_Q,
  NEON_USUBL = NEON_SSUBL | NEON3SameUBit,
  NEON_USUBL2 = NEON_USUBL | NEON_Q,
  NEON_USUBW = NEON_SSUBW | NEON3SameUBit,
  NEON_USUBW2 = NEON_USUBW | NEON_Q
};

// NEON instructions operating across vectors.
enum NEONAcrossLanesOp : uint32_t {
  NEONAcrossLanesFixed = 0x0E300800,
  NEONAcrossLanesFMask = 0x9F3E0C00,
  NEONAcrossLanesMask = 0xBF3FFC00,
  NEON_ADDV = NEONAcrossLanesFixed | 0x0001B000,
  NEON_SADDLV = NEONAcrossLanesFixed | 0x00003000,
  NEON_UADDLV = NEONAcrossLanesFixed | 0x20003000,
  NEON_SMAXV = NEONAcrossLanesFixed | 0x0000A000,
  NEON_SMINV = NEONAcrossLanesFixed | 0x0001A000,
  NEON_UMAXV = NEONAcrossLanesFixed | 0x2000A000,
  NEON_UMINV = NEONAcrossLanesFixed | 0x2001A000,

  // NEON floating point across instructions.
  NEONAcrossLanesFPFixed = NEONAcrossLanesFixed | 0x0000C000,
  NEONAcrossLanesFPFMask = NEONAcrossLanesFMask | 0x0000C000,
  NEONAcrossLanesFPMask = NEONAcrossLanesMask | 0x00800000,

  NEON_FMAXV = NEONAcrossLanesFPFixed | 0x2000F000,
  NEON_FMINV = NEONAcrossLanesFPFixed | 0x2080F000,
  NEON_FMAXNMV = NEONAcrossLanesFPFixed | 0x2000C000,
  NEON_FMINNMV = NEONAcrossLanesFPFixed | 0x2080C000
};

// NEON instructions with indexed element operand.
enum NEONByIndexedElementOp : uint32_t {
  NEONByIndexedElementFixed = 0x0F000000,
  NEONByIndexedElementFMask = 0x9F000400,
  NEONByIndexedElementMask = 0xBF00F400,
  NEON_MUL_byelement = NEONByIndexedElementFixed | 0x00008000,
  NEON_MLA_byelement = NEONByIndexedElementFixed | 0x20000000,
  NEON_MLS_byelement = NEONByIndexedElementFixed | 0x20004000,
  NEON_SMULL_byelement = NEONByIndexedElementFixed | 0x0000A000,
  NEON_SMLAL_byelement = NEONByIndexedElementFixed | 0x00002000,
  NEON_SMLSL_byelement = NEONByIndexedElementFixed | 0x00006000,
  NEON_UMULL_byelement = NEONByIndexedElementFixed | 0x2000A000,
  NEON_UMLAL_byelement = NEONByIndexedElementFixed | 0x20002000,
  NEON_UMLSL_byelement = NEONByIndexedElementFixed | 0x20006000,
  NEON_SQDMULL_byelement = NEONByIndexedElementFixed | 0x0000B000,
  NEON_SQDMLAL_byelement = NEONByIndexedElementFixed | 0x00003000,
  NEON_SQDMLSL_byelement = NEONByIndexedElementFixed | 0x00007000,
  NEON_SQDMULH_byelement = NEONByIndexedElementFixed | 0x0000C000,
  NEON_SQRDMULH_byelement = NEONByIndexedElementFixed | 0x0000D000,

  // Floating point instructions.
  NEONByIndexedElementFPFixed = NEONByIndexedElementFixed | 0x00800000,
  NEONByIndexedElementFPMask = NEONByIndexedElementMask | 0x00800000,
  NEON_FMLA_byelement = NEONByIndexedElementFPFixed | 0x00001000,
  NEON_FMLS_byelement = NEONByIndexedElementFPFixed | 0x00005000,
  NEON_FMUL_byelement = NEONByIndexedElementFPFixed | 0x00009000,
  NEON_FMULX_byelement = NEONByIndexedElementFPFixed | 0x20009000
};

// NEON modified immediate.
enum NEONModifiedImmediateOp : uint32_t {
  NEONModifiedImmediateFixed = 0x0F000400,
  NEONModifiedImmediateFMask = 0x9FF80400,
  NEONModifiedImmediateOpBit = 0x20000000,
  NEONModifiedImmediate_MOVI = NEONModifiedImmediateFixed | 0x00000000,
  NEONModifiedImmediate_MVNI = NEONModifiedImmediateFixed | 0x20000000,
  NEONModifiedImmediate_ORR = NEONModifiedImmediateFixed | 0x00001000,
  NEONModifiedImmediate_BIC = NEONModifiedImmediateFixed | 0x20001000
};

// NEON extract.
enum NEONExtractOp : uint32_t {
  NEONExtractFixed = 0x2E000000,
  NEONExtractFMask = 0xBF208400,
  NEONExtractMask = 0xBFE08400,
  NEON_EXT = NEONExtractFixed | 0x00000000
};

enum NEONLoadStoreMultiOp : uint32_t {
  NEONLoadStoreMultiL = 0x00400000,
  NEONLoadStoreMulti1_1v = 0x00007000,
  NEONLoadStoreMulti1_2v = 0x0000A000,
  NEONLoadStoreMulti1_3v = 0x00006000,
  NEONLoadStoreMulti1_4v = 0x00002000,
  NEONLoadStoreMulti2 = 0x00008000,
  NEONLoadStoreMulti3 = 0x00004000,
  NEONLoadStoreMulti4 = 0x00000000
};

// NEON load/store multiple structures.
enum NEONLoadStoreMultiStructOp : uint32_t {
  NEONLoadStoreMultiStructFixed = 0x0C000000,
  NEONLoadStoreMultiStructFMask = 0xBFBF0000,
  NEONLoadStoreMultiStructMask = 0xBFFFF000,
  NEONLoadStoreMultiStructStore = NEONLoadStoreMultiStructFixed,
  NEONLoadStoreMultiStructLoad =
      NEONLoadStoreMultiStructFixed | NEONLoadStoreMultiL,
  NEON_LD1_1v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_1v,
  NEON_LD1_2v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_2v,
  NEON_LD1_3v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_3v,
  NEON_LD1_4v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_4v,
  NEON_LD2 = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti2,
  NEON_LD3 = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti3,
  NEON_LD4 = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti4,
  NEON_ST1_1v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_1v,
  NEON_ST1_2v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_2v,
  NEON_ST1_3v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_3v,
  NEON_ST1_4v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_4v,
  NEON_ST2 = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti2,
  NEON_ST3 = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti3,
  NEON_ST4 = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti4
};

// NEON load/store multiple structures with post-index addressing.
enum NEONLoadStoreMultiStructPostIndexOp : uint32_t {
  NEONLoadStoreMultiStructPostIndexFixed = 0x0C800000,
  NEONLoadStoreMultiStructPostIndexFMask = 0xBFA00000,
  NEONLoadStoreMultiStructPostIndexMask = 0xBFE0F000,
  NEONLoadStoreMultiStructPostIndex = 0x00800000,
  NEON_LD1_1v_post = NEON_LD1_1v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD1_2v_post = NEON_LD1_2v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD1_3v_post = NEON_LD1_3v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD1_4v_post = NEON_LD1_4v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD2_post = NEON_LD2 | NEONLoadStoreMultiStructPostIndex,
  NEON_LD3_post = NEON_LD3 | NEONLoadStoreMultiStructPostIndex,
  NEON_LD4_post = NEON_LD4 | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_1v_post = NEON_ST1_1v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_2v_post = NEON_ST1_2v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_3v_post = NEON_ST1_3v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_4v_post = NEON_ST1_4v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST2_post = NEON_ST2 | NEONLoadStoreMultiStructPostIndex,
  NEON_ST3_post = NEON_ST3 | NEONLoadStoreMultiStructPostIndex,
  NEON_ST4_post = NEON_ST4 | NEONLoadStoreMultiStructPostIndex
};

enum NEONLoadStoreSingleOp : uint32_t {
  NEONLoadStoreSingle1 = 0x00000000,
  NEONLoadStoreSingle2 = 0x00200000,
  NEONLoadStoreSingle3 = 0x00002000,
  NEONLoadStoreSingle4 = 0x00202000,
  NEONLoadStoreSingleL = 0x00400000,
  NEONLoadStoreSingle_b = 0x00000000,
  NEONLoadStoreSingle_h = 0x00004000,
  NEONLoadStoreSingle_s = 0x00008000,
  NEONLoadStoreSingle_d = 0x00008400,
  NEONLoadStoreSingleAllLanes = 0x0000C000,
  NEONLoadStoreSingleLenMask = 0x00202000
};

// NEON load/store single structure.
enum NEONLoadStoreSingleStructOp : uint32_t {
  NEONLoadStoreSingleStructFixed = 0x0D000000,
  NEONLoadStoreSingleStructFMask = 0xBF9F0000,
  NEONLoadStoreSingleStructMask = 0xBFFFE000,
  NEONLoadStoreSingleStructStore = NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructLoad =
      NEONLoadStoreSingleStructFixed | NEONLoadStoreSingleL,
  NEONLoadStoreSingleStructLoad1 =
      NEONLoadStoreSingle1 | NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructLoad2 =
      NEONLoadStoreSingle2 | NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructLoad3 =
      NEONLoadStoreSingle3 | NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructLoad4 =
      NEONLoadStoreSingle4 | NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructStore1 =
      NEONLoadStoreSingle1 | NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructStore2 =
      NEONLoadStoreSingle2 | NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructStore3 =
      NEONLoadStoreSingle3 | NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructStore4 =
      NEONLoadStoreSingle4 | NEONLoadStoreSingleStructFixed,
  NEON_LD1_b = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_b,
  NEON_LD1_h = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_h,
  NEON_LD1_s = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_s,
  NEON_LD1_d = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_d,
  NEON_LD1R = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingleAllLanes,
  NEON_ST1_b = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_b,
  NEON_ST1_h = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_h,
  NEON_ST1_s = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_s,
  NEON_ST1_d = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_d,

  NEON_LD2_b = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_b,
  NEON_LD2_h = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_h,
  NEON_LD2_s = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_s,
  NEON_LD2_d = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_d,
  NEON_LD2R = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingleAllLanes,
  NEON_ST2_b = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_b,
  NEON_ST2_h = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_h,
  NEON_ST2_s = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_s,
  NEON_ST2_d = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_d,

  NEON_LD3_b = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_b,
  NEON_LD3_h = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_h,
  NEON_LD3_s = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_s,
  NEON_LD3_d = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_d,
  NEON_LD3R = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingleAllLanes,
  NEON_ST3_b = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_b,
  NEON_ST3_h = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_h,
  NEON_ST3_s = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_s,
  NEON_ST3_d = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_d,

  NEON_LD4_b = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_b,
  NEON_LD4_h = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_h,
  NEON_LD4_s = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_s,
  NEON_LD4_d = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_d,
  NEON_LD4R = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingleAllLanes,
  NEON_ST4_b = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_b,
  NEON_ST4_h = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_h,
  NEON_ST4_s = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_s,
  NEON_ST4_d = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_d
};

// NEON load/store single structure with post-index addressing.
enum NEONLoadStoreSingleStructPostIndexOp : uint32_t {
  NEONLoadStoreSingleStructPostIndexFixed = 0x0D800000,
  NEONLoadStoreSingleStructPostIndexFMask = 0xBF800000,
  NEONLoadStoreSingleStructPostIndexMask = 0xBFE0E000,
  NEONLoadStoreSingleStructPostIndex = 0x00800000,
  NEON_LD1_b_post = NEON_LD1_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1_h_post = NEON_LD1_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1_s_post = NEON_LD1_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1_d_post = NEON_LD1_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1R_post = NEON_LD1R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_b_post = NEON_ST1_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_h_post = NEON_ST1_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_s_post = NEON_ST1_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_d_post = NEON_ST1_d | NEONLoadStoreSingleStructPostIndex,

  NEON_LD2_b_post = NEON_LD2_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2_h_post = NEON_LD2_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2_s_post = NEON_LD2_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2_d_post = NEON_LD2_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2R_post = NEON_LD2R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_b_post = NEON_ST2_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_h_post = NEON_ST2_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_s_post = NEON_ST2_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_d_post = NEON_ST2_d | NEONLoadStoreSingleStructPostIndex,

  NEON_LD3_b_post = NEON_LD3_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3_h_post = NEON_LD3_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3_s_post = NEON_LD3_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3_d_post = NEON_LD3_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3R_post = NEON_LD3R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_b_post = NEON_ST3_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_h_post = NEON_ST3_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_s_post = NEON_ST3_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_d_post = NEON_ST3_d | NEONLoadStoreSingleStructPostIndex,

  NEON_LD4_b_post = NEON_LD4_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4_h_post = NEON_LD4_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4_s_post = NEON_LD4_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4_d_post = NEON_LD4_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4R_post = NEON_LD4R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_b_post = NEON_ST4_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_h_post = NEON_ST4_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_s_post = NEON_ST4_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_d_post = NEON_ST4_d | NEONLoadStoreSingleStructPostIndex
};

// NEON register copy.
enum NEONCopyOp : uint32_t {
  NEONCopyFixed = 0x0E000400,
  NEONCopyFMask = 0x9FE08400,
  NEONCopyMask = 0x3FE08400,
  NEONCopyInsElementMask = NEONCopyMask | 0x40000000,
  NEONCopyInsGeneralMask = NEONCopyMask | 0x40007800,
  NEONCopyDupElementMask = NEONCopyMask | 0x20007800,
  NEONCopyDupGeneralMask = NEONCopyDupElementMask,
  NEONCopyUmovMask = NEONCopyMask | 0x20007800,
  NEONCopySmovMask = NEONCopyMask | 0x20007800,
  NEON_INS_ELEMENT = NEONCopyFixed | 0x60000000,
  NEON_INS_GENERAL = NEONCopyFixed | 0x40001800,
  NEON_DUP_ELEMENT = NEONCopyFixed | 0x00000000,
  NEON_DUP_GENERAL = NEONCopyFixed | 0x00000800,
  NEON_SMOV = NEONCopyFixed | 0x00002800,
  NEON_UMOV = NEONCopyFixed | 0x00003800
};

// NEON scalar instructions with indexed element operand.
enum NEONScalarByIndexedElementOp : uint32_t {
  NEONScalarByIndexedElementFixed = 0x5F000000,
  NEONScalarByIndexedElementFMask = 0xDF000400,
  NEONScalarByIndexedElementMask = 0xFF00F400,
  NEON_SQDMLAL_byelement_scalar = NEON_Q | NEONScalar | NEON_SQDMLAL_byelement,
  NEON_SQDMLSL_byelement_scalar = NEON_Q | NEONScalar | NEON_SQDMLSL_byelement,
  NEON_SQDMULL_byelement_scalar = NEON_Q | NEONScalar | NEON_SQDMULL_byelement,
  NEON_SQDMULH_byelement_scalar = NEON_Q | NEONScalar | NEON_SQDMULH_byelement,
  NEON_SQRDMULH_byelement_scalar =
      NEON_Q | NEONScalar | NEON_SQRDMULH_byelement,

  // Floating point instructions.
  NEONScalarByIndexedElementFPFixed =
      NEONScalarByIndexedElementFixed | 0x00800000,
  NEONScalarByIndexedElementFPMask =
      NEONScalarByIndexedElementMask | 0x00800000,
  NEON_FMLA_byelement_scalar = NEON_Q | NEONScalar | NEON_FMLA_byelement,
  NEON_FMLS_byelement_scalar = NEON_Q | NEONScalar | NEON_FMLS_byelement,
  NEON_FMUL_byelement_scalar = NEON_Q | NEONScalar | NEON_FMUL_byelement,
  NEON_FMULX_byelement_scalar = NEON_Q | NEONScalar | NEON_FMULX_byelement
};

// NEON shift immediate.
enum NEONShiftImmediateOp : uint32_t {
  NEONShiftImmediateFixed = 0x0F000400,
  NEONShiftImmediateFMask = 0x9F800400,
  NEONShiftImmediateMask = 0xBF80FC00,
  NEONShiftImmediateUBit = 0x20000000,
  NEON_SHL = NEONShiftImmediateFixed | 0x00005000,
  NEON_SSHLL = NEONShiftImmediateFixed | 0x0000A000,
  NEON_USHLL = NEONShiftImmediateFixed | 0x2000A000,
  NEON_SLI = NEONShiftImmediateFixed | 0x20005000,
  NEON_SRI = NEONShiftImmediateFixed | 0x20004000,
  NEON_SHRN = NEONShiftImmediateFixed | 0x00008000,
  NEON_RSHRN = NEONShiftImmediateFixed | 0x00008800,
  NEON_UQSHRN = NEONShiftImmediateFixed | 0x20009000,
  NEON_UQRSHRN = NEONShiftImmediateFixed | 0x20009800,
  NEON_SQSHRN = NEONShiftImmediateFixed | 0x00009000,
  NEON_SQRSHRN = NEONShiftImmediateFixed | 0x00009800,
  NEON_SQSHRUN = NEONShiftImmediateFixed | 0x20008000,
  NEON_SQRSHRUN = NEONShiftImmediateFixed | 0x20008800,
  NEON_SSHR = NEONShiftImmediateFixed | 0x00000000,
  NEON_SRSHR = NEONShiftImmediateFixed | 0x00002000,
  NEON_USHR = NEONShiftImmediateFixed | 0x20000000,
  NEON_URSHR = NEONShiftImmediateFixed | 0x20002000,
  NEON_SSRA = NEONShiftImmediateFixed | 0x00001000,
  NEON_SRSRA = NEONShiftImmediateFixed | 0x00003000,
  NEON_USRA = NEONShiftImmediateFixed | 0x20001000,
  NEON_URSRA = NEONShiftImmediateFixed | 0x20003000,
  NEON_SQSHLU = NEONShiftImmediateFixed | 0x20006000,
  NEON_SCVTF_imm = NEONShiftImmediateFixed | 0x0000E000,
  NEON_UCVTF_imm = NEONShiftImmediateFixed | 0x2000E000,
  NEON_FCVTZS_imm = NEONShiftImmediateFixed | 0x0000F800,
  NEON_FCVTZU_imm = NEONShiftImmediateFixed | 0x2000F800,
  NEON_SQSHL_imm = NEONShiftImmediateFixed | 0x00007000,
  NEON_UQSHL_imm = NEONShiftImmediateFixed | 0x20007000
};

// NEON scalar register copy.
enum NEONScalarCopyOp : uint32_t {
  NEONScalarCopyFixed = 0x5E000400,
  NEONScalarCopyFMask = 0xDFE08400,
  NEONScalarCopyMask = 0xFFE0FC00,
  NEON_DUP_ELEMENT_scalar = NEON_Q | NEONScalar | NEON_DUP_ELEMENT
};

// NEON scalar pairwise instructions.
enum NEONScalarPairwiseOp : uint32_t {
  NEONScalarPairwiseFixed = 0x5E300800,
  NEONScalarPairwiseFMask = 0xDF3E0C00,
  NEONScalarPairwiseMask = 0xFFB1F800,
  NEON_ADDP_scalar = NEONScalarPairwiseFixed | 0x0081B000,
  NEON_FMAXNMP_scalar = NEONScalarPairwiseFixed | 0x2000C000,
  NEON_FMINNMP_scalar = NEONScalarPairwiseFixed | 0x2080C000,
  NEON_FADDP_scalar = NEONScalarPairwiseFixed | 0x2000D000,
  NEON_FMAXP_scalar = NEONScalarPairwiseFixed | 0x2000F000,
  NEON_FMINP_scalar = NEONScalarPairwiseFixed | 0x2080F000
};

// NEON scalar shift immediate.
enum NEONScalarShiftImmediateOp : uint32_t {
  NEONScalarShiftImmediateFixed = 0x5F000400,
  NEONScalarShiftImmediateFMask = 0xDF800400,
  NEONScalarShiftImmediateMask = 0xFF80FC00,
  NEON_SHL_scalar = NEON_Q | NEONScalar | NEON_SHL,
  NEON_SLI_scalar = NEON_Q | NEONScalar | NEON_SLI,
  NEON_SRI_scalar = NEON_Q | NEONScalar | NEON_SRI,
  NEON_SSHR_scalar = NEON_Q | NEONScalar | NEON_SSHR,
  NEON_USHR_scalar = NEON_Q | NEONScalar | NEON_USHR,
  NEON_SRSHR_scalar = NEON_Q | NEONScalar | NEON_SRSHR,
  NEON_URSHR_scalar = NEON_Q | NEONScalar | NEON_URSHR,
  NEON_SSRA_scalar = NEON_Q | NEONScalar | NEON_SSRA,
  NEON_USRA_scalar = NEON_Q | NEONScalar | NEON_USRA,
  NEON_SRSRA_scalar = NEON_Q | NEONScalar | NEON_SRSRA,
  NEON_URSRA_scalar = NEON_Q | NEONScalar | NEON_URSRA,
  NEON_UQSHRN_scalar = NEON_Q | NEONScalar | NEON_UQSHRN,
  NEON_UQRSHRN_scalar = NEON_Q | NEONScalar | NEON_UQRSHRN,
  NEON_SQSHRN_scalar = NEON_Q | NEONScalar | NEON_SQSHRN,
  NEON_SQRSHRN_scalar = NEON_Q | NEONScalar | NEON_SQRSHRN,
  NEON_SQSHRUN_scalar = NEON_Q | NEONScalar | NEON_SQSHRUN,
  NEON_SQRSHRUN_scalar = NEON_Q | NEONScalar | NEON_SQRSHRUN,
  NEON_SQSHLU_scalar = NEON_Q | NEONScalar | NEON_SQSHLU,
  NEON_SQSHL_imm_scalar = NEON_Q | NEONScalar | NEON_SQSHL_imm,
  NEON_UQSHL_imm_scalar = NEON_Q | NEONScalar | NEON_UQSHL_imm,
  NEON_SCVTF_imm_scalar = NEON_Q | NEONScalar | NEON_SCVTF_imm,
  NEON_UCVTF_imm_scalar = NEON_Q | NEONScalar | NEON_UCVTF_imm,
  NEON_FCVTZS_imm_scalar = NEON_Q | NEONScalar | NEON_FCVTZS_imm,
  NEON_FCVTZU_imm_scalar = NEON_Q | NEONScalar | NEON_FCVTZU_imm
};

// NEON table.
enum NEONTableOp : uint32_t {
  NEONTableFixed = 0x0E000000,
  NEONTableFMask = 0xBF208C00,
  NEONTableExt = 0x00001000,
  NEONTableMask = 0xBF20FC00,
  NEON_TBL_1v = NEONTableFixed | 0x00000000,
  NEON_TBL_2v = NEONTableFixed | 0x00002000,
  NEON_TBL_3v = NEONTableFixed | 0x00004000,
  NEON_TBL_4v = NEONTableFixed | 0x00006000,
  NEON_TBX_1v = NEON_TBL_1v | NEONTableExt,
  NEON_TBX_2v = NEON_TBL_2v | NEONTableExt,
  NEON_TBX_3v = NEON_TBL_3v | NEONTableExt,
  NEON_TBX_4v = NEON_TBL_4v | NEONTableExt
};

// NEON perm.
enum NEONPermOp : uint32_t {
  NEONPermFixed = 0x0E000800,
  NEONPermFMask = 0xBF208C00,
  NEONPermMask = 0x3F20FC00,
  NEON_UZP1 = NEONPermFixed | 0x00001000,
  NEON_TRN1 = NEONPermFixed | 0x00002000,
  NEON_ZIP1 = NEONPermFixed | 0x00003000,
  NEON_UZP2 = NEONPermFixed | 0x00005000,
  NEON_TRN2 = NEONPermFixed | 0x00006000,
  NEON_ZIP2 = NEONPermFixed | 0x00007000
};

// NEON scalar instructions with two register operands.
enum NEONScalar2RegMiscOp : uint32_t {
  NEONScalar2RegMiscFixed = 0x5E200800,
  NEONScalar2RegMiscFMask = 0xDF3E0C00,
  NEONScalar2RegMiscMask = NEON_Q | NEONScalar | NEON2RegMiscMask,
  NEON_CMGT_zero_scalar = NEON_Q | NEONScalar | NEON_CMGT_zero,
  NEON_CMEQ_zero_scalar = NEON_Q | NEONScalar | NEON_CMEQ_zero,
  NEON_CMLT_zero_scalar = NEON_Q | NEONScalar | NEON_CMLT_zero,
  NEON_CMGE_zero_scalar = NEON_Q | NEONScalar | NEON_CMGE_zero,
  NEON_CMLE_zero_scalar = NEON_Q | NEONScalar | NEON_CMLE_zero,
  NEON_ABS_scalar = NEON_Q | NEONScalar | NEON_ABS,
  NEON_SQABS_scalar = NEON_Q | NEONScalar | NEON_SQABS,
  NEON_NEG_scalar = NEON_Q | NEONScalar | NEON_NEG,
  NEON_SQNEG_scalar = NEON_Q | NEONScalar | NEON_SQNEG,
  NEON_SQXTN_scalar = NEON_Q | NEONScalar | NEON_SQXTN,
  NEON_UQXTN_scalar = NEON_Q | NEONScalar | NEON_UQXTN,
  NEON_SQXTUN_scalar = NEON_Q | NEONScalar | NEON_SQXTUN,
  NEON_SUQADD_scalar = NEON_Q | NEONScalar | NEON_SUQADD,
  NEON_USQADD_scalar = NEON_Q | NEONScalar | NEON_USQADD,

  NEONScalar2RegMiscOpcode = NEON2RegMiscOpcode,
  NEON_NEG_scalar_opcode = NEON_NEG_scalar & NEONScalar2RegMiscOpcode,

  NEONScalar2RegMiscFPMask = NEONScalar2RegMiscMask | 0x00800000,
  NEON_FRSQRTE_scalar = NEON_Q | NEONScalar | NEON_FRSQRTE,
  NEON_FRECPE_scalar = NEON_Q | NEONScalar | NEON_FRECPE,
  NEON_SCVTF_scalar = NEON_Q | NEONScalar | NEON_SCVTF,
  NEON_UCVTF_scalar = NEON_Q | NEONScalar | NEON_UCVTF,
  NEON_FCMGT_zero_scalar = NEON_Q | NEONScalar | NEON_FCMGT_zero,
  NEON_FCMEQ_zero_scalar = NEON_Q | NEONScalar | NEON_FCMEQ_zero,
  NEON_FCMLT_zero_scalar = NEON_Q | NEONScalar | NEON_FCMLT_zero,
  NEON_FCMGE_zero_scalar = NEON_Q | NEONScalar | NEON_FCMGE_zero,
  NEON_FCMLE_zero_scalar = NEON_Q | NEONScalar | NEON_FCMLE_zero,
  NEON_FRECPX_scalar = NEONScalar2RegMiscFixed | 0x0081F000,
  NEON_FCVTNS_scalar = NEON_Q | NEONScalar | NEON_FCVTNS,
  NEON_FCVTNU_scalar = NEON_Q | NEONScalar | NEON_FCVTNU,
  NEON_FCVTPS_scalar = NEON_Q | NEONScalar | NEON_FCVTPS,
  NEON_FCVTPU_scalar = NEON_Q | NEONScalar | NEON_FCVTPU,
  NEON_FCVTMS_scalar = NEON_Q | NEONScalar | NEON_FCVTMS,
  NEON_FCVTMU_scalar = NEON_Q | NEONScalar | NEON_FCVTMU,
  NEON_FCVTZS_scalar = NEON_Q | NEONScalar | NEON_FCVTZS,
  NEON_FCVTZU_scalar = NEON_Q | NEONScalar | NEON_FCVTZU,
  NEON_FCVTAS_scalar = NEON_Q | NEONScalar | NEON_FCVTAS,
  NEON_FCVTAU_scalar = NEON_Q | NEONScalar | NEON_FCVTAU,
  NEON_FCVTXN_scalar = NEON_Q | NEONScalar | NEON_FCVTXN
};

// NEON scalar instructions with three same-type operands.
enum NEONScalar3SameOp : uint32_t {
  NEONScalar3SameFixed = 0x5E200400,
  NEONScalar3SameFMask = 0xDF200400,
  NEONScalar3SameMask = 0xFF20FC00,
  NEON_ADD_scalar = NEON_Q | NEONScalar | NEON_ADD,
  NEON_CMEQ_scalar = NEON_Q | NEONScalar | NEON_CMEQ,
  NEON_CMGE_scalar = NEON_Q | NEONScalar | NEON_CMGE,
  NEON_CMGT_scalar = NEON_Q | NEONScalar | NEON_CMGT,
  NEON_CMHI_scalar = NEON_Q | NEONScalar | NEON_CMHI,
  NEON_CMHS_scalar = NEON_Q | NEONScalar | NEON_CMHS,
  NEON_CMTST_scalar = NEON_Q | NEONScalar | NEON_CMTST,
  NEON_SUB_scalar = NEON_Q | NEONScalar | NEON_SUB,
  NEON_UQADD_scalar = NEON_Q | NEONScalar | NEON_UQADD,
  NEON_SQADD_scalar = NEON_Q | NEONScalar | NEON_SQADD,
  NEON_UQSUB_scalar = NEON_Q | NEONScalar | NEON_UQSUB,
  NEON_SQSUB_scalar = NEON_Q | NEONScalar | NEON_SQSUB,
  NEON_USHL_scalar = NEON_Q | NEONScalar | NEON_USHL,
  NEON_SSHL_scalar = NEON_Q | NEONScalar | NEON_SSHL,
  NEON_UQSHL_scalar = NEON_Q | NEONScalar | NEON_UQSHL,
  NEON_SQSHL_scalar = NEON_Q | NEONScalar | NEON_SQSHL,
  NEON_URSHL_scalar = NEON_Q | NEONScalar | NEON_URSHL,
  NEON_SRSHL_scalar = NEON_Q | NEONScalar | NEON_SRSHL,
  NEON_UQRSHL_scalar = NEON_Q | NEONScalar | NEON_UQRSHL,
  NEON_SQRSHL_scalar = NEON_Q | NEONScalar | NEON_SQRSHL,
  NEON_SQDMULH_scalar = NEON_Q | NEONScalar | NEON_SQDMULH,
  NEON_SQRDMULH_scalar = NEON_Q | NEONScalar | NEON_SQRDMULH,

  // NEON floating point scalar instructions with three same-type operands.
  NEONScalar3SameFPFixed = NEONScalar3SameFixed | 0x0000C000,
  NEONScalar3SameFPFMask = NEONScalar3SameFMask | 0x0000C000,
  NEONScalar3SameFPMask = NEONScalar3SameMask | 0x00800000,
  NEON_FACGE_scalar = NEON_Q | NEONScalar | NEON_FACGE,
  NEON_FACGT_scalar = NEON_Q | NEONScalar | NEON_FACGT,
  NEON_FCMEQ_scalar = NEON_Q | NEONScalar | NEON_FCMEQ,
  NEON_FCMGE_scalar = NEON_Q | NEONScalar | NEON_FCMGE,
  NEON_FCMGT_scalar = NEON_Q | NEONScalar | NEON_FCMGT,
  NEON_FMULX_scalar = NEON_Q | NEONScalar | NEON_FMULX,
  NEON_FRECPS_scalar = NEON_Q | NEONScalar | NEON_FRECPS,
  NEON_FRSQRTS_scalar = NEON_Q | NEONScalar | NEON_FRSQRTS,
  NEON_FABD_scalar = NEON_Q | NEONScalar | NEON_FABD
};

// NEON scalar instructions with three different-type operands.
enum NEONScalar3DiffOp : uint32_t {
  NEONScalar3DiffFixed = 0x5E200000,
  NEONScalar3DiffFMask = 0xDF200C00,
  NEONScalar3DiffMask = NEON_Q | NEONScalar | NEON3DifferentMask,
  NEON_SQDMLAL_scalar = NEON_Q | NEONScalar | NEON_SQDMLAL,
  NEON_SQDMLSL_scalar = NEON_Q | NEONScalar | NEON_SQDMLSL,
  NEON_SQDMULL_scalar = NEON_Q | NEONScalar | NEON_SQDMULL
};

// Unimplemented and unallocated instructions. These are defined to make fixed
// bit assertion easier.
enum UnimplementedOp : uint32_t {
  UnimplementedFixed = 0x00000000,
  UnimplementedFMask = 0x00000000
};

enum UnallocatedOp : uint32_t {
  UnallocatedFixed = 0x00000000,
  UnallocatedFMask = 0x00000000
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_ARM64_CONSTANTS_ARM64_H_

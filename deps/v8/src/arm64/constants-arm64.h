// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_CONSTANTS_ARM64_H_
#define V8_ARM64_CONSTANTS_ARM64_H_

#include "src/base/macros.h"
#include "src/globals.h"

// Assert that this is an LP64 system.
STATIC_ASSERT(sizeof(int) == sizeof(int32_t));
STATIC_ASSERT(sizeof(long) == sizeof(int64_t));    // NOLINT(runtime/int)
STATIC_ASSERT(sizeof(void *) == sizeof(int64_t));
STATIC_ASSERT(sizeof(1) == sizeof(int32_t));
STATIC_ASSERT(sizeof(1L) == sizeof(int64_t));


// Get the standard printf format macros for C99 stdint types.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>


namespace v8 {
namespace internal {


const unsigned kInstructionSize = 4;
const unsigned kInstructionSizeLog2 = 2;
const unsigned kLoadLiteralScaleLog2 = 2;
const unsigned kMaxLoadLiteralRange = 1 * MB;

const int kNumberOfRegisters = 32;
const int kNumberOfFPRegisters = 32;
// Callee saved registers are x19-x30(lr).
const int kNumberOfCalleeSavedRegisters = 11;
const int kFirstCalleeSavedRegisterIndex = 19;
// Callee saved FP registers are d8-d15.
const int kNumberOfCalleeSavedFPRegisters = 8;
const int kFirstCalleeSavedFPRegisterIndex = 8;
// Callee saved registers with no specific purpose in JS are x19-x25.
const unsigned kJSCalleeSavedRegList = 0x03f80000;
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
const int64_t kWRegMask = 0x00000000ffffffffL;
const int64_t kXRegMask = 0xffffffffffffffffL;
const int64_t kSRegMask = 0x00000000ffffffffL;
const int64_t kDRegMask = 0xffffffffffffffffL;
// TODO(all) check if the expression below works on all compilers or if it
// triggers an overflow error.
const int64_t kDSignBit = 63;
const int64_t kDSignMask = 0x1L << kDSignBit;
const int64_t kSSignBit = 31;
const int64_t kSSignMask = 0x1L << kSSignBit;
const int64_t kXSignBit = 63;
const int64_t kXSignMask = 0x1L << kXSignBit;
const int64_t kWSignBit = 31;
const int64_t kWSignMask = 0x1L << kWSignBit;
const int64_t kDQuietNanBit = 51;
const int64_t kDQuietNanMask = 0x1L << kDQuietNanBit;
const int64_t kSQuietNanBit = 22;
const int64_t kSQuietNanMask = 0x1L << kSQuietNanBit;
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
const int kJSSPCode = 28;
const int kSPRegInternalCode = 63;
const unsigned kRegCodeMask = 0x1f;
const unsigned kShiftAmountWRegMask = 0x1f;
const unsigned kShiftAmountXRegMask = 0x3f;
// Standard machine types defined by AAPCS64.
const unsigned kByteSize = 8;
const unsigned kByteSizeInBytes = kByteSize >> 3;
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
// AArch64 floating-point specifics. These match IEEE-754.
const unsigned kDoubleMantissaBits = 52;
const unsigned kDoubleExponentBits = 11;
const unsigned kDoubleExponentBias = 1023;
const unsigned kFloatMantissaBits = 23;
const unsigned kFloatExponentBits = 8;

#define INSTRUCTION_FIELDS_LIST(V_)                                            \
/* Register fields */                                                          \
V_(Rd, 4, 0, Bits)                        /* Destination register.     */      \
V_(Rn, 9, 5, Bits)                        /* First source register.    */      \
V_(Rm, 20, 16, Bits)                      /* Second source register.   */      \
V_(Ra, 14, 10, Bits)                      /* Third source register.    */      \
V_(Rt, 4, 0, Bits)                        /* Load dest / store source. */      \
V_(Rt2, 14, 10, Bits)                     /* Load second dest /        */      \
                                         /* store second source.      */       \
V_(PrefetchMode, 4, 0, Bits)                                                   \
                                                                               \
/* Common bits */                                                              \
V_(SixtyFourBits, 31, 31, Bits)                                                \
V_(FlagsUpdate, 29, 29, Bits)                                                  \
                                                                               \
/* PC relative addressing */                                                   \
V_(ImmPCRelHi, 23, 5, SignedBits)                                              \
V_(ImmPCRelLo, 30, 29, Bits)                                                   \
                                                                               \
/* Add/subtract/logical shift register */                                      \
V_(ShiftDP, 23, 22, Bits)                                                      \
V_(ImmDPShift, 15, 10, Bits)                                                   \
                                                                               \
/* Add/subtract immediate */                                                   \
V_(ImmAddSub, 21, 10, Bits)                                                    \
V_(ShiftAddSub, 23, 22, Bits)                                                  \
                                                                               \
/* Add/substract extend */                                                     \
V_(ImmExtendShift, 12, 10, Bits)                                               \
V_(ExtendMode, 15, 13, Bits)                                                   \
                                                                               \
/* Move wide */                                                                \
V_(ImmMoveWide, 20, 5, Bits)                                                   \
V_(ShiftMoveWide, 22, 21, Bits)                                                \
                                                                               \
/* Logical immediate, bitfield and extract */                                  \
V_(BitN, 22, 22, Bits)                                                         \
V_(ImmRotate, 21, 16, Bits)                                                    \
V_(ImmSetBits, 15, 10, Bits)                                                   \
V_(ImmR, 21, 16, Bits)                                                         \
V_(ImmS, 15, 10, Bits)                                                         \
                                                                               \
/* Test and branch immediate */                                                \
V_(ImmTestBranch, 18, 5, SignedBits)                                           \
V_(ImmTestBranchBit40, 23, 19, Bits)                                           \
V_(ImmTestBranchBit5, 31, 31, Bits)                                            \
                                                                               \
/* Conditionals */                                                             \
V_(Condition, 15, 12, Bits)                                                    \
V_(ConditionBranch, 3, 0, Bits)                                                \
V_(Nzcv, 3, 0, Bits)                                                           \
V_(ImmCondCmp, 20, 16, Bits)                                                   \
V_(ImmCondBranch, 23, 5, SignedBits)                                           \
                                                                               \
/* Floating point */                                                           \
V_(FPType, 23, 22, Bits)                                                       \
V_(ImmFP, 20, 13, Bits)                                                        \
V_(FPScale, 15, 10, Bits)                                                      \
                                                                               \
/* Load Store */                                                               \
V_(ImmLS, 20, 12, SignedBits)                                                  \
V_(ImmLSUnsigned, 21, 10, Bits)                                                \
V_(ImmLSPair, 21, 15, SignedBits)                                              \
V_(SizeLS, 31, 30, Bits)                                                       \
V_(ImmShiftLS, 12, 12, Bits)                                                   \
                                                                               \
/* Other immediates */                                                         \
V_(ImmUncondBranch, 25, 0, SignedBits)                                         \
V_(ImmCmpBranch, 23, 5, SignedBits)                                            \
V_(ImmLLiteral, 23, 5, SignedBits)                                             \
V_(ImmException, 20, 5, Bits)                                                  \
V_(ImmHint, 11, 5, Bits)                                                       \
V_(ImmBarrierDomain, 11, 10, Bits)                                             \
V_(ImmBarrierType, 9, 8, Bits)                                                 \
                                                                               \
/* System (MRS, MSR) */                                                        \
V_(ImmSystemRegister, 19, 5, Bits)                                             \
V_(SysO0, 19, 19, Bits)                                                        \
V_(SysOp1, 18, 16, Bits)                                                       \
V_(SysOp2, 7, 5, Bits)                                                         \
V_(CRn, 15, 12, Bits)                                                          \
V_(CRm, 11, 8, Bits)                                                           \


#define SYSTEM_REGISTER_FIELDS_LIST(V_, M_)                                    \
/* NZCV */                                                                     \
V_(Flags, 31, 28, Bits, uint32_t)                                              \
V_(N,     31, 31, Bits, bool)                                                  \
V_(Z,     30, 30, Bits, bool)                                                  \
V_(C,     29, 29, Bits, bool)                                                  \
V_(V,     28, 28, Bits, uint32_t)                                              \
M_(NZCV, Flags_mask)                                                           \
                                                                               \
/* FPCR */                                                                     \
V_(AHP,   26, 26, Bits, bool)                                                  \
V_(DN,    25, 25, Bits, bool)                                                  \
V_(FZ,    24, 24, Bits, bool)                                                  \
V_(RMode, 23, 22, Bits, FPRounding)                                            \
M_(FPCR, AHP_mask | DN_mask | FZ_mask | RMode_mask)


// Fields offsets.
#define DECLARE_FIELDS_OFFSETS(Name, HighBit, LowBit, unused_1, unused_2)      \
  const int Name##_offset = LowBit;                                            \
  const int Name##_width = HighBit - LowBit + 1;                               \
  const uint32_t Name##_mask = ((1 << Name##_width) - 1) << LowBit;
#define DECLARE_INSTRUCTION_FIELDS_OFFSETS(Name, HighBit, LowBit, unused_1)    \
  DECLARE_FIELDS_OFFSETS(Name, HighBit, LowBit, unused_1, unused_2)
#define NOTHING(A, B)
INSTRUCTION_FIELDS_LIST(DECLARE_INSTRUCTION_FIELDS_OFFSETS)
SYSTEM_REGISTER_FIELDS_LIST(DECLARE_FIELDS_OFFSETS, NOTHING)
#undef NOTHING
#undef DECLARE_FIELDS_OFFSETS
#undef DECLARE_INSTRUCTION_FIELDS_OFFSETS

// ImmPCRel is a compound field (not present in INSTRUCTION_FIELDS_LIST), formed
// from ImmPCRelLo and ImmPCRelHi.
const int ImmPCRel_mask = ImmPCRelLo_mask | ImmPCRelHi_mask;

// Condition codes.
enum Condition {
  eq = 0,
  ne = 1,
  hs = 2, cs = hs,
  lo = 3, cc = lo,
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

// Commute a condition such that {a cond b == b cond' a}.
inline Condition CommuteCondition(Condition cond) {
  switch (cond) {
    case lo:
      return hi;
    case hi:
      return lo;
    case hs:
      return ls;
    case ls:
      return hs;
    case lt:
      return gt;
    case gt:
      return lt;
    case ge:
      return le;
    case le:
      return ge;
    case eq:
      return eq;
    default:
      // In practice this function is only used with a condition coming from
      // TokenToCondition in lithium-codegen-arm64.cc. Any other condition is
      // invalid as it doesn't necessary make sense to reverse it (consider
      // 'mi' for instance).
      UNREACHABLE();
      return nv;
  }
}

enum FlagsUpdate {
  SetFlags   = 1,
  LeaveFlags = 0
};

enum StatusFlags {
  NoFlag    = 0,

  // Derive the flag combinations from the system register bit descriptions.
  NFlag     = N_mask,
  ZFlag     = Z_mask,
  CFlag     = C_mask,
  VFlag     = V_mask,
  NZFlag    = NFlag | ZFlag,
  NCFlag    = NFlag | CFlag,
  NVFlag    = NFlag | VFlag,
  ZCFlag    = ZFlag | CFlag,
  ZVFlag    = ZFlag | VFlag,
  CVFlag    = CFlag | VFlag,
  NZCFlag   = NFlag | ZFlag | CFlag,
  NZVFlag   = NFlag | ZFlag | VFlag,
  NCVFlag   = NFlag | CFlag | VFlag,
  ZCVFlag   = ZFlag | CFlag | VFlag,
  NZCVFlag  = NFlag | ZFlag | CFlag | VFlag,

  // Floating-point comparison results.
  FPEqualFlag       = ZCFlag,
  FPLessThanFlag    = NFlag,
  FPGreaterThanFlag = CFlag,
  FPUnorderedFlag   = CVFlag
};

enum Shift {
  NO_SHIFT = -1,
  LSL = 0x0,
  LSR = 0x1,
  ASR = 0x2,
  ROR = 0x3
};

enum Extend {
  NO_EXTEND = -1,
  UXTB      = 0,
  UXTH      = 1,
  UXTW      = 2,
  UXTX      = 3,
  SXTB      = 4,
  SXTH      = 5,
  SXTW      = 6,
  SXTX      = 7
};

enum SystemHint {
  NOP   = 0,
  YIELD = 1,
  WFE   = 2,
  WFI   = 3,
  SEV   = 4,
  SEVL  = 5
};

enum BarrierDomain {
  OuterShareable = 0,
  NonShareable   = 1,
  InnerShareable = 2,
  FullSystem     = 3
};

enum BarrierType {
  BarrierOther  = 0,
  BarrierReads  = 1,
  BarrierWrites = 2,
  BarrierAll    = 3
};

// System/special register names.
// This information is not encoded as one field but as the concatenation of
// multiple fields (Op0<0>, Op1, Crn, Crm, Op2).
enum SystemRegister {
  NZCV = ((0x1 << SysO0_offset) |
          (0x3 << SysOp1_offset) |
          (0x4 << CRn_offset) |
          (0x2 << CRm_offset) |
          (0x0 << SysOp2_offset)) >> ImmSystemRegister_offset,
  FPCR = ((0x1 << SysO0_offset) |
          (0x3 << SysOp1_offset) |
          (0x4 << CRn_offset) |
          (0x4 << CRm_offset) |
          (0x0 << SysOp2_offset)) >> ImmSystemRegister_offset
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


// Generic fields.
enum GenericInstrField {
  SixtyFourBits        = 0x80000000,
  ThirtyTwoBits        = 0x00000000,
  FP32                 = 0x00000000,
  FP64                 = 0x00400000
};

// PC relative addressing.
enum PCRelAddressingOp {
  PCRelAddressingFixed = 0x10000000,
  PCRelAddressingFMask = 0x1F000000,
  PCRelAddressingMask  = 0x9F000000,
  ADR                  = PCRelAddressingFixed | 0x00000000,
  ADRP                 = PCRelAddressingFixed | 0x80000000
};

// Add/sub (immediate, shifted and extended.)
const int kSFOffset = 31;
enum AddSubOp {
  AddSubOpMask      = 0x60000000,
  AddSubSetFlagsBit = 0x20000000,
  ADD               = 0x00000000,
  ADDS              = ADD | AddSubSetFlagsBit,
  SUB               = 0x40000000,
  SUBS              = SUB | AddSubSetFlagsBit
};

#define ADD_SUB_OP_LIST(V)  \
  V(ADD),                   \
  V(ADDS),                  \
  V(SUB),                   \
  V(SUBS)

enum AddSubImmediateOp {
  AddSubImmediateFixed = 0x11000000,
  AddSubImmediateFMask = 0x1F000000,
  AddSubImmediateMask  = 0xFF000000,
  #define ADD_SUB_IMMEDIATE(A)           \
  A##_w_imm = AddSubImmediateFixed | A,  \
  A##_x_imm = AddSubImmediateFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_IMMEDIATE)
  #undef ADD_SUB_IMMEDIATE
};

enum AddSubShiftedOp {
  AddSubShiftedFixed   = 0x0B000000,
  AddSubShiftedFMask   = 0x1F200000,
  AddSubShiftedMask    = 0xFF200000,
  #define ADD_SUB_SHIFTED(A)             \
  A##_w_shift = AddSubShiftedFixed | A,  \
  A##_x_shift = AddSubShiftedFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_SHIFTED)
  #undef ADD_SUB_SHIFTED
};

enum AddSubExtendedOp {
  AddSubExtendedFixed  = 0x0B200000,
  AddSubExtendedFMask  = 0x1F200000,
  AddSubExtendedMask   = 0xFFE00000,
  #define ADD_SUB_EXTENDED(A)           \
  A##_w_ext = AddSubExtendedFixed | A,  \
  A##_x_ext = AddSubExtendedFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_EXTENDED)
  #undef ADD_SUB_EXTENDED
};

// Add/sub with carry.
enum AddSubWithCarryOp {
  AddSubWithCarryFixed = 0x1A000000,
  AddSubWithCarryFMask = 0x1FE00000,
  AddSubWithCarryMask  = 0xFFE0FC00,
  ADC_w                = AddSubWithCarryFixed | ADD,
  ADC_x                = AddSubWithCarryFixed | ADD | SixtyFourBits,
  ADC                  = ADC_w,
  ADCS_w               = AddSubWithCarryFixed | ADDS,
  ADCS_x               = AddSubWithCarryFixed | ADDS | SixtyFourBits,
  SBC_w                = AddSubWithCarryFixed | SUB,
  SBC_x                = AddSubWithCarryFixed | SUB | SixtyFourBits,
  SBC                  = SBC_w,
  SBCS_w               = AddSubWithCarryFixed | SUBS,
  SBCS_x               = AddSubWithCarryFixed | SUBS | SixtyFourBits
};


// Logical (immediate and shifted register).
enum LogicalOp {
  LogicalOpMask = 0x60200000,
  NOT   = 0x00200000,
  AND   = 0x00000000,
  BIC   = AND | NOT,
  ORR   = 0x20000000,
  ORN   = ORR | NOT,
  EOR   = 0x40000000,
  EON   = EOR | NOT,
  ANDS  = 0x60000000,
  BICS  = ANDS | NOT
};

// Logical immediate.
enum LogicalImmediateOp {
  LogicalImmediateFixed = 0x12000000,
  LogicalImmediateFMask = 0x1F800000,
  LogicalImmediateMask  = 0xFF800000,
  AND_w_imm   = LogicalImmediateFixed | AND,
  AND_x_imm   = LogicalImmediateFixed | AND | SixtyFourBits,
  ORR_w_imm   = LogicalImmediateFixed | ORR,
  ORR_x_imm   = LogicalImmediateFixed | ORR | SixtyFourBits,
  EOR_w_imm   = LogicalImmediateFixed | EOR,
  EOR_x_imm   = LogicalImmediateFixed | EOR | SixtyFourBits,
  ANDS_w_imm  = LogicalImmediateFixed | ANDS,
  ANDS_x_imm  = LogicalImmediateFixed | ANDS | SixtyFourBits
};

// Logical shifted register.
enum LogicalShiftedOp {
  LogicalShiftedFixed = 0x0A000000,
  LogicalShiftedFMask = 0x1F000000,
  LogicalShiftedMask  = 0xFF200000,
  AND_w               = LogicalShiftedFixed | AND,
  AND_x               = LogicalShiftedFixed | AND | SixtyFourBits,
  AND_shift           = AND_w,
  BIC_w               = LogicalShiftedFixed | BIC,
  BIC_x               = LogicalShiftedFixed | BIC | SixtyFourBits,
  BIC_shift           = BIC_w,
  ORR_w               = LogicalShiftedFixed | ORR,
  ORR_x               = LogicalShiftedFixed | ORR | SixtyFourBits,
  ORR_shift           = ORR_w,
  ORN_w               = LogicalShiftedFixed | ORN,
  ORN_x               = LogicalShiftedFixed | ORN | SixtyFourBits,
  ORN_shift           = ORN_w,
  EOR_w               = LogicalShiftedFixed | EOR,
  EOR_x               = LogicalShiftedFixed | EOR | SixtyFourBits,
  EOR_shift           = EOR_w,
  EON_w               = LogicalShiftedFixed | EON,
  EON_x               = LogicalShiftedFixed | EON | SixtyFourBits,
  EON_shift           = EON_w,
  ANDS_w              = LogicalShiftedFixed | ANDS,
  ANDS_x              = LogicalShiftedFixed | ANDS | SixtyFourBits,
  ANDS_shift          = ANDS_w,
  BICS_w              = LogicalShiftedFixed | BICS,
  BICS_x              = LogicalShiftedFixed | BICS | SixtyFourBits,
  BICS_shift          = BICS_w
};

// Move wide immediate.
enum MoveWideImmediateOp {
  MoveWideImmediateFixed = 0x12800000,
  MoveWideImmediateFMask = 0x1F800000,
  MoveWideImmediateMask  = 0xFF800000,
  MOVN                   = 0x00000000,
  MOVZ                   = 0x40000000,
  MOVK                   = 0x60000000,
  MOVN_w                 = MoveWideImmediateFixed | MOVN,
  MOVN_x                 = MoveWideImmediateFixed | MOVN | SixtyFourBits,
  MOVZ_w                 = MoveWideImmediateFixed | MOVZ,
  MOVZ_x                 = MoveWideImmediateFixed | MOVZ | SixtyFourBits,
  MOVK_w                 = MoveWideImmediateFixed | MOVK,
  MOVK_x                 = MoveWideImmediateFixed | MOVK | SixtyFourBits
};

// Bitfield.
const int kBitfieldNOffset = 22;
enum BitfieldOp {
  BitfieldFixed = 0x13000000,
  BitfieldFMask = 0x1F800000,
  BitfieldMask  = 0xFF800000,
  SBFM_w        = BitfieldFixed | 0x00000000,
  SBFM_x        = BitfieldFixed | 0x80000000,
  SBFM          = SBFM_w,
  BFM_w         = BitfieldFixed | 0x20000000,
  BFM_x         = BitfieldFixed | 0xA0000000,
  BFM           = BFM_w,
  UBFM_w        = BitfieldFixed | 0x40000000,
  UBFM_x        = BitfieldFixed | 0xC0000000,
  UBFM          = UBFM_w
  // Bitfield N field.
};

// Extract.
enum ExtractOp {
  ExtractFixed = 0x13800000,
  ExtractFMask = 0x1F800000,
  ExtractMask  = 0xFFA00000,
  EXTR_w       = ExtractFixed | 0x00000000,
  EXTR_x       = ExtractFixed | 0x80000000,
  EXTR         = EXTR_w
};

// Unconditional branch.
enum UnconditionalBranchOp {
  UnconditionalBranchFixed = 0x14000000,
  UnconditionalBranchFMask = 0x7C000000,
  UnconditionalBranchMask  = 0xFC000000,
  B                        = UnconditionalBranchFixed | 0x00000000,
  BL                       = UnconditionalBranchFixed | 0x80000000
};

// Unconditional branch to register.
enum UnconditionalBranchToRegisterOp {
  UnconditionalBranchToRegisterFixed = 0xD6000000,
  UnconditionalBranchToRegisterFMask = 0xFE000000,
  UnconditionalBranchToRegisterMask  = 0xFFFFFC1F,
  BR      = UnconditionalBranchToRegisterFixed | 0x001F0000,
  BLR     = UnconditionalBranchToRegisterFixed | 0x003F0000,
  RET     = UnconditionalBranchToRegisterFixed | 0x005F0000
};

// Compare and branch.
enum CompareBranchOp {
  CompareBranchFixed = 0x34000000,
  CompareBranchFMask = 0x7E000000,
  CompareBranchMask  = 0xFF000000,
  CBZ_w              = CompareBranchFixed | 0x00000000,
  CBZ_x              = CompareBranchFixed | 0x80000000,
  CBZ                = CBZ_w,
  CBNZ_w             = CompareBranchFixed | 0x01000000,
  CBNZ_x             = CompareBranchFixed | 0x81000000,
  CBNZ               = CBNZ_w
};

// Test and branch.
enum TestBranchOp {
  TestBranchFixed = 0x36000000,
  TestBranchFMask = 0x7E000000,
  TestBranchMask  = 0x7F000000,
  TBZ             = TestBranchFixed | 0x00000000,
  TBNZ            = TestBranchFixed | 0x01000000
};

// Conditional branch.
enum ConditionalBranchOp {
  ConditionalBranchFixed = 0x54000000,
  ConditionalBranchFMask = 0xFE000000,
  ConditionalBranchMask  = 0xFF000010,
  B_cond                 = ConditionalBranchFixed | 0x00000000
};

// System.
// System instruction encoding is complicated because some instructions use op
// and CR fields to encode parameters. To handle this cleanly, the system
// instructions are split into more than one enum.

enum SystemOp {
  SystemFixed = 0xD5000000,
  SystemFMask = 0xFFC00000
};

enum SystemSysRegOp {
  SystemSysRegFixed = 0xD5100000,
  SystemSysRegFMask = 0xFFD00000,
  SystemSysRegMask  = 0xFFF00000,
  MRS               = SystemSysRegFixed | 0x00200000,
  MSR               = SystemSysRegFixed | 0x00000000
};

enum SystemHintOp {
  SystemHintFixed = 0xD503201F,
  SystemHintFMask = 0xFFFFF01F,
  SystemHintMask  = 0xFFFFF01F,
  HINT            = SystemHintFixed | 0x00000000
};

// Exception.
enum ExceptionOp {
  ExceptionFixed = 0xD4000000,
  ExceptionFMask = 0xFF000000,
  ExceptionMask  = 0xFFE0001F,
  HLT            = ExceptionFixed | 0x00400000,
  BRK            = ExceptionFixed | 0x00200000,
  SVC            = ExceptionFixed | 0x00000001,
  HVC            = ExceptionFixed | 0x00000002,
  SMC            = ExceptionFixed | 0x00000003,
  DCPS1          = ExceptionFixed | 0x00A00001,
  DCPS2          = ExceptionFixed | 0x00A00002,
  DCPS3          = ExceptionFixed | 0x00A00003
};
// Code used to spot hlt instructions that should not be hit.
const int kHltBadCode = 0xbad;

enum MemBarrierOp {
  MemBarrierFixed = 0xD503309F,
  MemBarrierFMask = 0xFFFFF09F,
  MemBarrierMask  = 0xFFFFF0FF,
  DSB             = MemBarrierFixed | 0x00000000,
  DMB             = MemBarrierFixed | 0x00000020,
  ISB             = MemBarrierFixed | 0x00000040
};

// Any load or store (including pair).
enum LoadStoreAnyOp {
  LoadStoreAnyFMask = 0x0a000000,
  LoadStoreAnyFixed = 0x08000000
};

// Any load pair or store pair.
enum LoadStorePairAnyOp {
  LoadStorePairAnyFMask = 0x3a000000,
  LoadStorePairAnyFixed = 0x28000000
};

#define LOAD_STORE_PAIR_OP_LIST(V)  \
  V(STP, w,   0x00000000),          \
  V(LDP, w,   0x00400000),          \
  V(LDPSW, x, 0x40400000),          \
  V(STP, x,   0x80000000),          \
  V(LDP, x,   0x80400000),          \
  V(STP, s,   0x04000000),          \
  V(LDP, s,   0x04400000),          \
  V(STP, d,   0x44000000),          \
  V(LDP, d,   0x44400000)

// Load/store pair (post, pre and offset.)
enum LoadStorePairOp {
  LoadStorePairMask = 0xC4400000,
  LoadStorePairLBit = 1 << 22,
  #define LOAD_STORE_PAIR(A, B, C) \
  A##_##B = C
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR)
  #undef LOAD_STORE_PAIR
};

enum LoadStorePairPostIndexOp {
  LoadStorePairPostIndexFixed = 0x28800000,
  LoadStorePairPostIndexFMask = 0x3B800000,
  LoadStorePairPostIndexMask  = 0xFFC00000,
  #define LOAD_STORE_PAIR_POST_INDEX(A, B, C)  \
  A##_##B##_post = LoadStorePairPostIndexFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_POST_INDEX)
  #undef LOAD_STORE_PAIR_POST_INDEX
};

enum LoadStorePairPreIndexOp {
  LoadStorePairPreIndexFixed = 0x29800000,
  LoadStorePairPreIndexFMask = 0x3B800000,
  LoadStorePairPreIndexMask  = 0xFFC00000,
  #define LOAD_STORE_PAIR_PRE_INDEX(A, B, C)  \
  A##_##B##_pre = LoadStorePairPreIndexFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_PRE_INDEX)
  #undef LOAD_STORE_PAIR_PRE_INDEX
};

enum LoadStorePairOffsetOp {
  LoadStorePairOffsetFixed = 0x29000000,
  LoadStorePairOffsetFMask = 0x3B800000,
  LoadStorePairOffsetMask  = 0xFFC00000,
  #define LOAD_STORE_PAIR_OFFSET(A, B, C)  \
  A##_##B##_off = LoadStorePairOffsetFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_OFFSET)
  #undef LOAD_STORE_PAIR_OFFSET
};

// Load literal.
enum LoadLiteralOp {
  LoadLiteralFixed = 0x18000000,
  LoadLiteralFMask = 0x3B000000,
  LoadLiteralMask  = 0xFF000000,
  LDR_w_lit        = LoadLiteralFixed | 0x00000000,
  LDR_x_lit        = LoadLiteralFixed | 0x40000000,
  LDRSW_x_lit      = LoadLiteralFixed | 0x80000000,
  PRFM_lit         = LoadLiteralFixed | 0xC0000000,
  LDR_s_lit        = LoadLiteralFixed | 0x04000000,
  LDR_d_lit        = LoadLiteralFixed | 0x44000000
};

#define LOAD_STORE_OP_LIST(V)     \
  V(ST, RB, w,  0x00000000),  \
  V(ST, RH, w,  0x40000000),  \
  V(ST, R, w,   0x80000000),  \
  V(ST, R, x,   0xC0000000),  \
  V(LD, RB, w,  0x00400000),  \
  V(LD, RH, w,  0x40400000),  \
  V(LD, R, w,   0x80400000),  \
  V(LD, R, x,   0xC0400000),  \
  V(LD, RSB, x, 0x00800000),  \
  V(LD, RSH, x, 0x40800000),  \
  V(LD, RSW, x, 0x80800000),  \
  V(LD, RSB, w, 0x00C00000),  \
  V(LD, RSH, w, 0x40C00000),  \
  V(ST, R, s,   0x84000000),  \
  V(ST, R, d,   0xC4000000),  \
  V(LD, R, s,   0x84400000),  \
  V(LD, R, d,   0xC4400000)


// Load/store unscaled offset.
enum LoadStoreUnscaledOffsetOp {
  LoadStoreUnscaledOffsetFixed = 0x38000000,
  LoadStoreUnscaledOffsetFMask = 0x3B200C00,
  LoadStoreUnscaledOffsetMask  = 0xFFE00C00,
  #define LOAD_STORE_UNSCALED(A, B, C, D)  \
  A##U##B##_##C = LoadStoreUnscaledOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSCALED)
  #undef LOAD_STORE_UNSCALED
};

// Load/store (post, pre, offset and unsigned.)
enum LoadStoreOp {
  LoadStoreOpMask   = 0xC4C00000,
  #define LOAD_STORE(A, B, C, D)  \
  A##B##_##C = D
  LOAD_STORE_OP_LIST(LOAD_STORE),
  #undef LOAD_STORE
  PRFM = 0xC0800000
};

// Load/store post index.
enum LoadStorePostIndex {
  LoadStorePostIndexFixed = 0x38000400,
  LoadStorePostIndexFMask = 0x3B200C00,
  LoadStorePostIndexMask  = 0xFFE00C00,
  #define LOAD_STORE_POST_INDEX(A, B, C, D)  \
  A##B##_##C##_post = LoadStorePostIndexFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_POST_INDEX)
  #undef LOAD_STORE_POST_INDEX
};

// Load/store pre index.
enum LoadStorePreIndex {
  LoadStorePreIndexFixed = 0x38000C00,
  LoadStorePreIndexFMask = 0x3B200C00,
  LoadStorePreIndexMask  = 0xFFE00C00,
  #define LOAD_STORE_PRE_INDEX(A, B, C, D)  \
  A##B##_##C##_pre = LoadStorePreIndexFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_PRE_INDEX)
  #undef LOAD_STORE_PRE_INDEX
};

// Load/store unsigned offset.
enum LoadStoreUnsignedOffset {
  LoadStoreUnsignedOffsetFixed = 0x39000000,
  LoadStoreUnsignedOffsetFMask = 0x3B000000,
  LoadStoreUnsignedOffsetMask  = 0xFFC00000,
  PRFM_unsigned                = LoadStoreUnsignedOffsetFixed | PRFM,
  #define LOAD_STORE_UNSIGNED_OFFSET(A, B, C, D) \
  A##B##_##C##_unsigned = LoadStoreUnsignedOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSIGNED_OFFSET)
  #undef LOAD_STORE_UNSIGNED_OFFSET
};

// Load/store register offset.
enum LoadStoreRegisterOffset {
  LoadStoreRegisterOffsetFixed = 0x38200800,
  LoadStoreRegisterOffsetFMask = 0x3B200C00,
  LoadStoreRegisterOffsetMask  = 0xFFE00C00,
  PRFM_reg                     = LoadStoreRegisterOffsetFixed | PRFM,
  #define LOAD_STORE_REGISTER_OFFSET(A, B, C, D) \
  A##B##_##C##_reg = LoadStoreRegisterOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_REGISTER_OFFSET)
  #undef LOAD_STORE_REGISTER_OFFSET
};

// Conditional compare.
enum ConditionalCompareOp {
  ConditionalCompareMask = 0x60000000,
  CCMN                   = 0x20000000,
  CCMP                   = 0x60000000
};

// Conditional compare register.
enum ConditionalCompareRegisterOp {
  ConditionalCompareRegisterFixed = 0x1A400000,
  ConditionalCompareRegisterFMask = 0x1FE00800,
  ConditionalCompareRegisterMask  = 0xFFE00C10,
  CCMN_w = ConditionalCompareRegisterFixed | CCMN,
  CCMN_x = ConditionalCompareRegisterFixed | SixtyFourBits | CCMN,
  CCMP_w = ConditionalCompareRegisterFixed | CCMP,
  CCMP_x = ConditionalCompareRegisterFixed | SixtyFourBits | CCMP
};

// Conditional compare immediate.
enum ConditionalCompareImmediateOp {
  ConditionalCompareImmediateFixed = 0x1A400800,
  ConditionalCompareImmediateFMask = 0x1FE00800,
  ConditionalCompareImmediateMask  = 0xFFE00C10,
  CCMN_w_imm = ConditionalCompareImmediateFixed | CCMN,
  CCMN_x_imm = ConditionalCompareImmediateFixed | SixtyFourBits | CCMN,
  CCMP_w_imm = ConditionalCompareImmediateFixed | CCMP,
  CCMP_x_imm = ConditionalCompareImmediateFixed | SixtyFourBits | CCMP
};

// Conditional select.
enum ConditionalSelectOp {
  ConditionalSelectFixed = 0x1A800000,
  ConditionalSelectFMask = 0x1FE00000,
  ConditionalSelectMask  = 0xFFE00C00,
  CSEL_w                 = ConditionalSelectFixed | 0x00000000,
  CSEL_x                 = ConditionalSelectFixed | 0x80000000,
  CSEL                   = CSEL_w,
  CSINC_w                = ConditionalSelectFixed | 0x00000400,
  CSINC_x                = ConditionalSelectFixed | 0x80000400,
  CSINC                  = CSINC_w,
  CSINV_w                = ConditionalSelectFixed | 0x40000000,
  CSINV_x                = ConditionalSelectFixed | 0xC0000000,
  CSINV                  = CSINV_w,
  CSNEG_w                = ConditionalSelectFixed | 0x40000400,
  CSNEG_x                = ConditionalSelectFixed | 0xC0000400,
  CSNEG                  = CSNEG_w
};

// Data processing 1 source.
enum DataProcessing1SourceOp {
  DataProcessing1SourceFixed = 0x5AC00000,
  DataProcessing1SourceFMask = 0x5FE00000,
  DataProcessing1SourceMask  = 0xFFFFFC00,
  RBIT    = DataProcessing1SourceFixed | 0x00000000,
  RBIT_w  = RBIT,
  RBIT_x  = RBIT | SixtyFourBits,
  REV16   = DataProcessing1SourceFixed | 0x00000400,
  REV16_w = REV16,
  REV16_x = REV16 | SixtyFourBits,
  REV     = DataProcessing1SourceFixed | 0x00000800,
  REV_w   = REV,
  REV32_x = REV | SixtyFourBits,
  REV_x   = DataProcessing1SourceFixed | SixtyFourBits | 0x00000C00,
  CLZ     = DataProcessing1SourceFixed | 0x00001000,
  CLZ_w   = CLZ,
  CLZ_x   = CLZ | SixtyFourBits,
  CLS     = DataProcessing1SourceFixed | 0x00001400,
  CLS_w   = CLS,
  CLS_x   = CLS | SixtyFourBits
};

// Data processing 2 source.
enum DataProcessing2SourceOp {
  DataProcessing2SourceFixed = 0x1AC00000,
  DataProcessing2SourceFMask = 0x5FE00000,
  DataProcessing2SourceMask  = 0xFFE0FC00,
  UDIV_w  = DataProcessing2SourceFixed | 0x00000800,
  UDIV_x  = DataProcessing2SourceFixed | 0x80000800,
  UDIV    = UDIV_w,
  SDIV_w  = DataProcessing2SourceFixed | 0x00000C00,
  SDIV_x  = DataProcessing2SourceFixed | 0x80000C00,
  SDIV    = SDIV_w,
  LSLV_w  = DataProcessing2SourceFixed | 0x00002000,
  LSLV_x  = DataProcessing2SourceFixed | 0x80002000,
  LSLV    = LSLV_w,
  LSRV_w  = DataProcessing2SourceFixed | 0x00002400,
  LSRV_x  = DataProcessing2SourceFixed | 0x80002400,
  LSRV    = LSRV_w,
  ASRV_w  = DataProcessing2SourceFixed | 0x00002800,
  ASRV_x  = DataProcessing2SourceFixed | 0x80002800,
  ASRV    = ASRV_w,
  RORV_w  = DataProcessing2SourceFixed | 0x00002C00,
  RORV_x  = DataProcessing2SourceFixed | 0x80002C00,
  RORV    = RORV_w,
  CRC32B  = DataProcessing2SourceFixed | 0x00004000,
  CRC32H  = DataProcessing2SourceFixed | 0x00004400,
  CRC32W  = DataProcessing2SourceFixed | 0x00004800,
  CRC32X  = DataProcessing2SourceFixed | SixtyFourBits | 0x00004C00,
  CRC32CB = DataProcessing2SourceFixed | 0x00005000,
  CRC32CH = DataProcessing2SourceFixed | 0x00005400,
  CRC32CW = DataProcessing2SourceFixed | 0x00005800,
  CRC32CX = DataProcessing2SourceFixed | SixtyFourBits | 0x00005C00
};

// Data processing 3 source.
enum DataProcessing3SourceOp {
  DataProcessing3SourceFixed = 0x1B000000,
  DataProcessing3SourceFMask = 0x1F000000,
  DataProcessing3SourceMask  = 0xFFE08000,
  MADD_w                     = DataProcessing3SourceFixed | 0x00000000,
  MADD_x                     = DataProcessing3SourceFixed | 0x80000000,
  MADD                       = MADD_w,
  MSUB_w                     = DataProcessing3SourceFixed | 0x00008000,
  MSUB_x                     = DataProcessing3SourceFixed | 0x80008000,
  MSUB                       = MSUB_w,
  SMADDL_x                   = DataProcessing3SourceFixed | 0x80200000,
  SMSUBL_x                   = DataProcessing3SourceFixed | 0x80208000,
  SMULH_x                    = DataProcessing3SourceFixed | 0x80400000,
  UMADDL_x                   = DataProcessing3SourceFixed | 0x80A00000,
  UMSUBL_x                   = DataProcessing3SourceFixed | 0x80A08000,
  UMULH_x                    = DataProcessing3SourceFixed | 0x80C00000
};

// Floating point compare.
enum FPCompareOp {
  FPCompareFixed = 0x1E202000,
  FPCompareFMask = 0x5F203C00,
  FPCompareMask  = 0xFFE0FC1F,
  FCMP_s         = FPCompareFixed | 0x00000000,
  FCMP_d         = FPCompareFixed | FP64 | 0x00000000,
  FCMP           = FCMP_s,
  FCMP_s_zero    = FPCompareFixed | 0x00000008,
  FCMP_d_zero    = FPCompareFixed | FP64 | 0x00000008,
  FCMP_zero      = FCMP_s_zero,
  FCMPE_s        = FPCompareFixed | 0x00000010,
  FCMPE_d        = FPCompareFixed | FP64 | 0x00000010,
  FCMPE_s_zero   = FPCompareFixed | 0x00000018,
  FCMPE_d_zero   = FPCompareFixed | FP64 | 0x00000018
};

// Floating point conditional compare.
enum FPConditionalCompareOp {
  FPConditionalCompareFixed = 0x1E200400,
  FPConditionalCompareFMask = 0x5F200C00,
  FPConditionalCompareMask  = 0xFFE00C10,
  FCCMP_s                   = FPConditionalCompareFixed | 0x00000000,
  FCCMP_d                   = FPConditionalCompareFixed | FP64 | 0x00000000,
  FCCMP                     = FCCMP_s,
  FCCMPE_s                  = FPConditionalCompareFixed | 0x00000010,
  FCCMPE_d                  = FPConditionalCompareFixed | FP64 | 0x00000010,
  FCCMPE                    = FCCMPE_s
};

// Floating point conditional select.
enum FPConditionalSelectOp {
  FPConditionalSelectFixed = 0x1E200C00,
  FPConditionalSelectFMask = 0x5F200C00,
  FPConditionalSelectMask  = 0xFFE00C00,
  FCSEL_s                  = FPConditionalSelectFixed | 0x00000000,
  FCSEL_d                  = FPConditionalSelectFixed | FP64 | 0x00000000,
  FCSEL                    = FCSEL_s
};

// Floating point immediate.
enum FPImmediateOp {
  FPImmediateFixed = 0x1E201000,
  FPImmediateFMask = 0x5F201C00,
  FPImmediateMask  = 0xFFE01C00,
  FMOV_s_imm       = FPImmediateFixed | 0x00000000,
  FMOV_d_imm       = FPImmediateFixed | FP64 | 0x00000000
};

// Floating point data processing 1 source.
enum FPDataProcessing1SourceOp {
  FPDataProcessing1SourceFixed = 0x1E204000,
  FPDataProcessing1SourceFMask = 0x5F207C00,
  FPDataProcessing1SourceMask  = 0xFFFFFC00,
  FMOV_s   = FPDataProcessing1SourceFixed | 0x00000000,
  FMOV_d   = FPDataProcessing1SourceFixed | FP64 | 0x00000000,
  FMOV     = FMOV_s,
  FABS_s   = FPDataProcessing1SourceFixed | 0x00008000,
  FABS_d   = FPDataProcessing1SourceFixed | FP64 | 0x00008000,
  FABS     = FABS_s,
  FNEG_s   = FPDataProcessing1SourceFixed | 0x00010000,
  FNEG_d   = FPDataProcessing1SourceFixed | FP64 | 0x00010000,
  FNEG     = FNEG_s,
  FSQRT_s  = FPDataProcessing1SourceFixed | 0x00018000,
  FSQRT_d  = FPDataProcessing1SourceFixed | FP64 | 0x00018000,
  FSQRT    = FSQRT_s,
  FCVT_ds  = FPDataProcessing1SourceFixed | 0x00028000,
  FCVT_sd  = FPDataProcessing1SourceFixed | FP64 | 0x00020000,
  FRINTN_s = FPDataProcessing1SourceFixed | 0x00040000,
  FRINTN_d = FPDataProcessing1SourceFixed | FP64 | 0x00040000,
  FRINTN   = FRINTN_s,
  FRINTP_s = FPDataProcessing1SourceFixed | 0x00048000,
  FRINTP_d = FPDataProcessing1SourceFixed | FP64 | 0x00048000,
  FRINTP   = FRINTP_s,
  FRINTM_s = FPDataProcessing1SourceFixed | 0x00050000,
  FRINTM_d = FPDataProcessing1SourceFixed | FP64 | 0x00050000,
  FRINTM   = FRINTM_s,
  FRINTZ_s = FPDataProcessing1SourceFixed | 0x00058000,
  FRINTZ_d = FPDataProcessing1SourceFixed | FP64 | 0x00058000,
  FRINTZ   = FRINTZ_s,
  FRINTA_s = FPDataProcessing1SourceFixed | 0x00060000,
  FRINTA_d = FPDataProcessing1SourceFixed | FP64 | 0x00060000,
  FRINTA   = FRINTA_s,
  FRINTX_s = FPDataProcessing1SourceFixed | 0x00070000,
  FRINTX_d = FPDataProcessing1SourceFixed | FP64 | 0x00070000,
  FRINTX   = FRINTX_s,
  FRINTI_s = FPDataProcessing1SourceFixed | 0x00078000,
  FRINTI_d = FPDataProcessing1SourceFixed | FP64 | 0x00078000,
  FRINTI   = FRINTI_s
};

// Floating point data processing 2 source.
enum FPDataProcessing2SourceOp {
  FPDataProcessing2SourceFixed = 0x1E200800,
  FPDataProcessing2SourceFMask = 0x5F200C00,
  FPDataProcessing2SourceMask  = 0xFFE0FC00,
  FMUL     = FPDataProcessing2SourceFixed | 0x00000000,
  FMUL_s   = FMUL,
  FMUL_d   = FMUL | FP64,
  FDIV     = FPDataProcessing2SourceFixed | 0x00001000,
  FDIV_s   = FDIV,
  FDIV_d   = FDIV | FP64,
  FADD     = FPDataProcessing2SourceFixed | 0x00002000,
  FADD_s   = FADD,
  FADD_d   = FADD | FP64,
  FSUB     = FPDataProcessing2SourceFixed | 0x00003000,
  FSUB_s   = FSUB,
  FSUB_d   = FSUB | FP64,
  FMAX     = FPDataProcessing2SourceFixed | 0x00004000,
  FMAX_s   = FMAX,
  FMAX_d   = FMAX | FP64,
  FMIN     = FPDataProcessing2SourceFixed | 0x00005000,
  FMIN_s   = FMIN,
  FMIN_d   = FMIN | FP64,
  FMAXNM   = FPDataProcessing2SourceFixed | 0x00006000,
  FMAXNM_s = FMAXNM,
  FMAXNM_d = FMAXNM | FP64,
  FMINNM   = FPDataProcessing2SourceFixed | 0x00007000,
  FMINNM_s = FMINNM,
  FMINNM_d = FMINNM | FP64,
  FNMUL    = FPDataProcessing2SourceFixed | 0x00008000,
  FNMUL_s  = FNMUL,
  FNMUL_d  = FNMUL | FP64
};

// Floating point data processing 3 source.
enum FPDataProcessing3SourceOp {
  FPDataProcessing3SourceFixed = 0x1F000000,
  FPDataProcessing3SourceFMask = 0x5F000000,
  FPDataProcessing3SourceMask  = 0xFFE08000,
  FMADD_s                      = FPDataProcessing3SourceFixed | 0x00000000,
  FMSUB_s                      = FPDataProcessing3SourceFixed | 0x00008000,
  FNMADD_s                     = FPDataProcessing3SourceFixed | 0x00200000,
  FNMSUB_s                     = FPDataProcessing3SourceFixed | 0x00208000,
  FMADD_d                      = FPDataProcessing3SourceFixed | 0x00400000,
  FMSUB_d                      = FPDataProcessing3SourceFixed | 0x00408000,
  FNMADD_d                     = FPDataProcessing3SourceFixed | 0x00600000,
  FNMSUB_d                     = FPDataProcessing3SourceFixed | 0x00608000
};

// Conversion between floating point and integer.
enum FPIntegerConvertOp {
  FPIntegerConvertFixed = 0x1E200000,
  FPIntegerConvertFMask = 0x5F20FC00,
  FPIntegerConvertMask  = 0xFFFFFC00,
  FCVTNS    = FPIntegerConvertFixed | 0x00000000,
  FCVTNS_ws = FCVTNS,
  FCVTNS_xs = FCVTNS | SixtyFourBits,
  FCVTNS_wd = FCVTNS | FP64,
  FCVTNS_xd = FCVTNS | SixtyFourBits | FP64,
  FCVTNU    = FPIntegerConvertFixed | 0x00010000,
  FCVTNU_ws = FCVTNU,
  FCVTNU_xs = FCVTNU | SixtyFourBits,
  FCVTNU_wd = FCVTNU | FP64,
  FCVTNU_xd = FCVTNU | SixtyFourBits | FP64,
  FCVTPS    = FPIntegerConvertFixed | 0x00080000,
  FCVTPS_ws = FCVTPS,
  FCVTPS_xs = FCVTPS | SixtyFourBits,
  FCVTPS_wd = FCVTPS | FP64,
  FCVTPS_xd = FCVTPS | SixtyFourBits | FP64,
  FCVTPU    = FPIntegerConvertFixed | 0x00090000,
  FCVTPU_ws = FCVTPU,
  FCVTPU_xs = FCVTPU | SixtyFourBits,
  FCVTPU_wd = FCVTPU | FP64,
  FCVTPU_xd = FCVTPU | SixtyFourBits | FP64,
  FCVTMS    = FPIntegerConvertFixed | 0x00100000,
  FCVTMS_ws = FCVTMS,
  FCVTMS_xs = FCVTMS | SixtyFourBits,
  FCVTMS_wd = FCVTMS | FP64,
  FCVTMS_xd = FCVTMS | SixtyFourBits | FP64,
  FCVTMU    = FPIntegerConvertFixed | 0x00110000,
  FCVTMU_ws = FCVTMU,
  FCVTMU_xs = FCVTMU | SixtyFourBits,
  FCVTMU_wd = FCVTMU | FP64,
  FCVTMU_xd = FCVTMU | SixtyFourBits | FP64,
  FCVTZS    = FPIntegerConvertFixed | 0x00180000,
  FCVTZS_ws = FCVTZS,
  FCVTZS_xs = FCVTZS | SixtyFourBits,
  FCVTZS_wd = FCVTZS | FP64,
  FCVTZS_xd = FCVTZS | SixtyFourBits | FP64,
  FCVTZU    = FPIntegerConvertFixed | 0x00190000,
  FCVTZU_ws = FCVTZU,
  FCVTZU_xs = FCVTZU | SixtyFourBits,
  FCVTZU_wd = FCVTZU | FP64,
  FCVTZU_xd = FCVTZU | SixtyFourBits | FP64,
  SCVTF     = FPIntegerConvertFixed | 0x00020000,
  SCVTF_sw  = SCVTF,
  SCVTF_sx  = SCVTF | SixtyFourBits,
  SCVTF_dw  = SCVTF | FP64,
  SCVTF_dx  = SCVTF | SixtyFourBits | FP64,
  UCVTF     = FPIntegerConvertFixed | 0x00030000,
  UCVTF_sw  = UCVTF,
  UCVTF_sx  = UCVTF | SixtyFourBits,
  UCVTF_dw  = UCVTF | FP64,
  UCVTF_dx  = UCVTF | SixtyFourBits | FP64,
  FCVTAS    = FPIntegerConvertFixed | 0x00040000,
  FCVTAS_ws = FCVTAS,
  FCVTAS_xs = FCVTAS | SixtyFourBits,
  FCVTAS_wd = FCVTAS | FP64,
  FCVTAS_xd = FCVTAS | SixtyFourBits | FP64,
  FCVTAU    = FPIntegerConvertFixed | 0x00050000,
  FCVTAU_ws = FCVTAU,
  FCVTAU_xs = FCVTAU | SixtyFourBits,
  FCVTAU_wd = FCVTAU | FP64,
  FCVTAU_xd = FCVTAU | SixtyFourBits | FP64,
  FMOV_ws   = FPIntegerConvertFixed | 0x00060000,
  FMOV_sw   = FPIntegerConvertFixed | 0x00070000,
  FMOV_xd   = FMOV_ws | SixtyFourBits | FP64,
  FMOV_dx   = FMOV_sw | SixtyFourBits | FP64
};

// Conversion between fixed point and floating point.
enum FPFixedPointConvertOp {
  FPFixedPointConvertFixed = 0x1E000000,
  FPFixedPointConvertFMask = 0x5F200000,
  FPFixedPointConvertMask  = 0xFFFF0000,
  FCVTZS_fixed    = FPFixedPointConvertFixed | 0x00180000,
  FCVTZS_ws_fixed = FCVTZS_fixed,
  FCVTZS_xs_fixed = FCVTZS_fixed | SixtyFourBits,
  FCVTZS_wd_fixed = FCVTZS_fixed | FP64,
  FCVTZS_xd_fixed = FCVTZS_fixed | SixtyFourBits | FP64,
  FCVTZU_fixed    = FPFixedPointConvertFixed | 0x00190000,
  FCVTZU_ws_fixed = FCVTZU_fixed,
  FCVTZU_xs_fixed = FCVTZU_fixed | SixtyFourBits,
  FCVTZU_wd_fixed = FCVTZU_fixed | FP64,
  FCVTZU_xd_fixed = FCVTZU_fixed | SixtyFourBits | FP64,
  SCVTF_fixed     = FPFixedPointConvertFixed | 0x00020000,
  SCVTF_sw_fixed  = SCVTF_fixed,
  SCVTF_sx_fixed  = SCVTF_fixed | SixtyFourBits,
  SCVTF_dw_fixed  = SCVTF_fixed | FP64,
  SCVTF_dx_fixed  = SCVTF_fixed | SixtyFourBits | FP64,
  UCVTF_fixed     = FPFixedPointConvertFixed | 0x00030000,
  UCVTF_sw_fixed  = UCVTF_fixed,
  UCVTF_sx_fixed  = UCVTF_fixed | SixtyFourBits,
  UCVTF_dw_fixed  = UCVTF_fixed | FP64,
  UCVTF_dx_fixed  = UCVTF_fixed | SixtyFourBits | FP64
};

// Unimplemented and unallocated instructions. These are defined to make fixed
// bit assertion easier.
enum UnimplementedOp {
  UnimplementedFixed = 0x00000000,
  UnimplementedFMask = 0x00000000
};

enum UnallocatedOp {
  UnallocatedFixed = 0x00000000,
  UnallocatedFMask = 0x00000000
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_CONSTANTS_ARM64_H_

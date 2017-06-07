// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  V8_MIPS_CONSTANTS_H_
#define  V8_MIPS_CONSTANTS_H_
#include "src/globals.h"
// UNIMPLEMENTED_ macro for MIPS.
#ifdef DEBUG
#define UNIMPLEMENTED_MIPS()                                                  \
  v8::internal::PrintF("%s, \tline %d: \tfunction %s not implemented. \n",    \
                       __FILE__, __LINE__, __func__)
#else
#define UNIMPLEMENTED_MIPS()
#endif

#define UNSUPPORTED_MIPS() v8::internal::PrintF("Unsupported instruction.\n")

enum ArchVariants {
  kMips32r1 = v8::internal::MIPSr1,
  kMips32r2 = v8::internal::MIPSr2,
  kMips32r6 = v8::internal::MIPSr6,
  kLoongson
};

#ifdef _MIPS_ARCH_MIPS32R2
  static const ArchVariants kArchVariant = kMips32r2;
#elif _MIPS_ARCH_MIPS32R6
  static const ArchVariants kArchVariant = kMips32r6;
#elif _MIPS_ARCH_LOONGSON
// The loongson flag refers to the LOONGSON architectures based on MIPS-III,
// which predates (and is a subset of) the mips32r2 and r1 architectures.
  static const ArchVariants kArchVariant = kLoongson;
#elif _MIPS_ARCH_MIPS32RX
// This flags referred to compatibility mode that creates universal code that
// can run on any MIPS32 architecture revision. The dynamically generated code
// by v8 is specialized for the MIPS host detected in runtime probing.
  static const ArchVariants kArchVariant = kMips32r1;
#else
  static const ArchVariants kArchVariant = kMips32r1;
#endif

enum Endianness {
  kLittle,
  kBig
};

#if defined(V8_TARGET_LITTLE_ENDIAN)
  static const Endianness kArchEndian = kLittle;
#elif defined(V8_TARGET_BIG_ENDIAN)
  static const Endianness kArchEndian = kBig;
#else
#error Unknown endianness
#endif

enum FpuMode {
  kFP32,
  kFP64,
  kFPXX
};

#if defined(FPU_MODE_FP32)
  static const FpuMode kFpuMode = kFP32;
#elif defined(FPU_MODE_FP64)
  static const FpuMode kFpuMode = kFP64;
#elif defined(FPU_MODE_FPXX)
#if defined(_MIPS_ARCH_MIPS32R2) || defined(_MIPS_ARCH_MIPS32R6)
static const FpuMode kFpuMode = kFPXX;
#else
#error "FPXX is supported only on Mips32R2 and Mips32R6"
#endif
#else
static const FpuMode kFpuMode = kFP32;
#endif

#if(defined(__mips_hard_float) && __mips_hard_float != 0)
// Use floating-point coprocessor instructions. This flag is raised when
// -mhard-float is passed to the compiler.
const bool IsMipsSoftFloatABI = false;
#elif(defined(__mips_soft_float) && __mips_soft_float != 0)
// This flag is raised when -msoft-float is passed to the compiler.
// Although FPU is a base requirement for v8, soft-float ABI is used
// on soft-float systems with FPU kernel emulation.
const bool IsMipsSoftFloatABI = true;
#else
const bool IsMipsSoftFloatABI = true;
#endif

#if defined(V8_TARGET_LITTLE_ENDIAN)
const uint32_t kHoleNanUpper32Offset = 4;
const uint32_t kHoleNanLower32Offset = 0;
#elif defined(V8_TARGET_BIG_ENDIAN)
const uint32_t kHoleNanUpper32Offset = 0;
const uint32_t kHoleNanLower32Offset = 4;
#else
#error Unknown endianness
#endif

#define IsFp64Mode() (kFpuMode == kFP64)
#define IsFp32Mode() (kFpuMode == kFP32)
#define IsFpxxMode() (kFpuMode == kFPXX)

#ifndef _MIPS_ARCH_MIPS32RX
#define IsMipsArchVariant(check) \
  (kArchVariant == check)
#else
#define IsMipsArchVariant(check) \
  (CpuFeatures::IsSupported(static_cast<CpuFeature>(check)))
#endif

#if defined(V8_TARGET_LITTLE_ENDIAN)
const uint32_t kMipsLwrOffset = 0;
const uint32_t kMipsLwlOffset = 3;
const uint32_t kMipsSwrOffset = 0;
const uint32_t kMipsSwlOffset = 3;
#elif defined(V8_TARGET_BIG_ENDIAN)
const uint32_t kMipsLwrOffset = 3;
const uint32_t kMipsLwlOffset = 0;
const uint32_t kMipsSwrOffset = 3;
const uint32_t kMipsSwlOffset = 0;
#else
#error Unknown endianness
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

// Defines constants and accessor classes to assemble, disassemble and
// simulate MIPS32 instructions.
//
// See: MIPS32 Architecture For Programmers
//      Volume II: The MIPS32 Instruction Set
// Try www.cs.cornell.edu/courses/cs3410/2008fa/MIPS_Vol2.pdf.

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Registers and FPURegisters.

// Number of general purpose registers.
const int kNumRegisters = 32;
const int kInvalidRegister = -1;

// Number of registers with HI, LO, and pc.
const int kNumSimuRegisters = 35;

// In the simulator, the PC register is simulated as the 34th register.
const int kPCRegister = 34;

// Number coprocessor registers.
const int kNumFPURegisters = 32;
const int kInvalidFPURegister = -1;

// Number of MSA registers
const int kNumMSARegisters = 32;
const int kInvalidMSARegister = -1;

const int kInvalidMSAControlRegister = -1;
const int kMSAIRRegister = 0;
const int kMSACSRRegister = 1;

// FPU (coprocessor 1) control registers. Currently only FCSR is implemented.
const int kFCSRRegister = 31;
const int kInvalidFPUControlRegister = -1;
const uint32_t kFPUInvalidResult = static_cast<uint32_t>(1 << 31) - 1;
const int32_t kFPUInvalidResultNegative = static_cast<int32_t>(1 << 31);
const uint64_t kFPU64InvalidResult =
    static_cast<uint64_t>(static_cast<uint64_t>(1) << 63) - 1;
const int64_t kFPU64InvalidResultNegative =
    static_cast<int64_t>(static_cast<uint64_t>(1) << 63);

// FCSR constants.
const uint32_t kFCSRInexactFlagBit = 2;
const uint32_t kFCSRUnderflowFlagBit = 3;
const uint32_t kFCSROverflowFlagBit = 4;
const uint32_t kFCSRDivideByZeroFlagBit = 5;
const uint32_t kFCSRInvalidOpFlagBit = 6;
const uint32_t kFCSRNaN2008FlagBit = 18;

const uint32_t kFCSRInexactFlagMask = 1 << kFCSRInexactFlagBit;
const uint32_t kFCSRUnderflowFlagMask = 1 << kFCSRUnderflowFlagBit;
const uint32_t kFCSROverflowFlagMask = 1 << kFCSROverflowFlagBit;
const uint32_t kFCSRDivideByZeroFlagMask = 1 << kFCSRDivideByZeroFlagBit;
const uint32_t kFCSRInvalidOpFlagMask = 1 << kFCSRInvalidOpFlagBit;
const uint32_t kFCSRNaN2008FlagMask = 1 << kFCSRNaN2008FlagBit;

const uint32_t kFCSRFlagMask =
    kFCSRInexactFlagMask |
    kFCSRUnderflowFlagMask |
    kFCSROverflowFlagMask |
    kFCSRDivideByZeroFlagMask |
    kFCSRInvalidOpFlagMask;

const uint32_t kFCSRExceptionFlagMask = kFCSRFlagMask ^ kFCSRInexactFlagMask;

// 'pref' instruction hints
const int32_t kPrefHintLoad = 0;
const int32_t kPrefHintStore = 1;
const int32_t kPrefHintLoadStreamed = 4;
const int32_t kPrefHintStoreStreamed = 5;
const int32_t kPrefHintLoadRetained = 6;
const int32_t kPrefHintStoreRetained = 7;
const int32_t kPrefHintWritebackInvalidate = 25;
const int32_t kPrefHintPrepareForStore = 30;

// Helper functions for converting between register numbers and names.
class Registers {
 public:
  // Return the name of the register.
  static const char* Name(int reg);

  // Lookup the register number for the name provided.
  static int Number(const char* name);

  struct RegisterAlias {
    int reg;
    const char* name;
  };

  static const int32_t kMaxValue = 0x7fffffff;
  static const int32_t kMinValue = 0x80000000;

 private:
  static const char* names_[kNumSimuRegisters];
  static const RegisterAlias aliases_[];
};

// Helper functions for converting between register numbers and names.
class FPURegisters {
 public:
  // Return the name of the register.
  static const char* Name(int reg);

  // Lookup the register number for the name provided.
  static int Number(const char* name);

  struct RegisterAlias {
    int creg;
    const char* name;
  };

 private:
  static const char* names_[kNumFPURegisters];
  static const RegisterAlias aliases_[];
};

// Helper functions for converting between register numbers and names.
class MSARegisters {
 public:
  // Return the name of the register.
  static const char* Name(int reg);

  // Lookup the register number for the name provided.
  static int Number(const char* name);

  struct RegisterAlias {
    int creg;
    const char* name;
  };

 private:
  static const char* names_[kNumMSARegisters];
  static const RegisterAlias aliases_[];
};

// -----------------------------------------------------------------------------
// Instructions encoding constants.

// On MIPS all instructions are 32 bits.
typedef int32_t Instr;

// Special Software Interrupt codes when used in the presence of the MIPS
// simulator.
enum SoftwareInterruptCodes {
  // Transition to C code.
  call_rt_redirected = 0xfffff
};

// On MIPS Simulator breakpoints can have different codes:
// - Breaks between 0 and kMaxWatchpointCode are treated as simple watchpoints,
//   the simulator will run through them and print the registers.
// - Breaks between kMaxWatchpointCode and kMaxStopCode are treated as stop()
//   instructions (see Assembler::stop()).
// - Breaks larger than kMaxStopCode are simple breaks, dropping you into the
//   debugger.
const uint32_t kMaxWatchpointCode = 31;
const uint32_t kMaxStopCode = 127;
STATIC_ASSERT(kMaxWatchpointCode < kMaxStopCode);


// ----- Fields offset and length.
const int kOpcodeShift   = 26;
const int kOpcodeBits    = 6;
const int kRsShift       = 21;
const int kRsBits        = 5;
const int kRtShift       = 16;
const int kRtBits        = 5;
const int kRdShift       = 11;
const int kRdBits        = 5;
const int kSaShift       = 6;
const int kSaBits        = 5;
const int kLsaSaBits = 2;
const int kFunctionShift = 0;
const int kFunctionBits  = 6;
const int kLuiShift      = 16;
const int kBp2Shift = 6;
const int kBp2Bits = 2;

const int kImm16Shift = 0;
const int kImm16Bits  = 16;
const int kImm18Shift = 0;
const int kImm18Bits = 18;
const int kImm19Shift = 0;
const int kImm19Bits = 19;
const int kImm21Shift = 0;
const int kImm21Bits  = 21;
const int kImm26Shift = 0;
const int kImm26Bits  = 26;
const int kImm28Shift = 0;
const int kImm28Bits  = 28;
const int kImm32Shift = 0;
const int kImm32Bits  = 32;
const int kMsaImm8Shift = 16;
const int kMsaImm8Bits = 8;
const int kMsaImm5Shift = 16;
const int kMsaImm5Bits = 5;
const int kMsaImm10Shift = 11;
const int kMsaImm10Bits = 10;
const int kMsaImmMI10Shift = 16;
const int kMsaImmMI10Bits = 10;

// In branches and jumps immediate fields point to words, not bytes,
// and are therefore shifted by 2.
const int kImmFieldShift = 2;

const int kFrBits        = 5;
const int kFrShift       = 21;
const int kFsShift       = 11;
const int kFsBits        = 5;
const int kFtShift       = 16;
const int kFtBits        = 5;
const int kFdShift       = 6;
const int kFdBits        = 5;
const int kFCccShift     = 8;
const int kFCccBits      = 3;
const int kFBccShift     = 18;
const int kFBccBits      = 3;
const int kFBtrueShift   = 16;
const int kFBtrueBits    = 1;
const int kWtBits = 5;
const int kWtShift = 16;
const int kWsBits = 5;
const int kWsShift = 11;
const int kWdBits = 5;
const int kWdShift = 6;

// ----- Miscellaneous useful masks.
// Instruction bit masks.
const int kOpcodeMask = ((1 << kOpcodeBits) - 1) << kOpcodeShift;
const int kImm16Mask = ((1 << kImm16Bits) - 1) << kImm16Shift;
const int kImm18Mask = ((1 << kImm18Bits) - 1) << kImm18Shift;
const int kImm19Mask = ((1 << kImm19Bits) - 1) << kImm19Shift;
const int kImm21Mask = ((1 << kImm21Bits) - 1) << kImm21Shift;
const int kImm26Mask = ((1 << kImm26Bits) - 1) << kImm26Shift;
const int kImm28Mask = ((1 << kImm28Bits) - 1) << kImm28Shift;
const int kImm5Mask = ((1 << 5) - 1);
const int kImm8Mask = ((1 << 8) - 1);
const int kImm10Mask = ((1 << 10) - 1);
const int kMsaI5I10Mask = ((7U << 23) | ((1 << 6) - 1));
const int kRsFieldMask = ((1 << kRsBits) - 1) << kRsShift;
const int kRtFieldMask = ((1 << kRtBits) - 1) << kRtShift;
const int kRdFieldMask = ((1 << kRdBits) - 1) << kRdShift;
const int kSaFieldMask = ((1 << kSaBits) - 1) << kSaShift;
const int kFunctionFieldMask = ((1 << kFunctionBits) - 1) << kFunctionShift;
// Misc masks.
const int kHiMask = 0xffff << 16;
const int kLoMask = 0xffff;
const int kSignMask = 0x80000000;
const int kJumpAddrMask = (1 << (kImm26Bits + kImmFieldShift)) - 1;

// ----- MIPS Opcodes and Function Fields.
// We use this presentation to stay close to the table representation in
// MIPS32 Architecture For Programmers, Volume II: The MIPS32 Instruction Set.
enum Opcode : uint32_t {
  SPECIAL = 0U << kOpcodeShift,
  REGIMM = 1U << kOpcodeShift,

  J = ((0U << 3) + 2) << kOpcodeShift,
  JAL = ((0U << 3) + 3) << kOpcodeShift,
  BEQ = ((0U << 3) + 4) << kOpcodeShift,
  BNE = ((0U << 3) + 5) << kOpcodeShift,
  BLEZ = ((0U << 3) + 6) << kOpcodeShift,
  BGTZ = ((0U << 3) + 7) << kOpcodeShift,

  ADDI = ((1U << 3) + 0) << kOpcodeShift,
  ADDIU = ((1U << 3) + 1) << kOpcodeShift,
  SLTI = ((1U << 3) + 2) << kOpcodeShift,
  SLTIU = ((1U << 3) + 3) << kOpcodeShift,
  ANDI = ((1U << 3) + 4) << kOpcodeShift,
  ORI = ((1U << 3) + 5) << kOpcodeShift,
  XORI = ((1U << 3) + 6) << kOpcodeShift,
  LUI = ((1U << 3) + 7) << kOpcodeShift,  // LUI/AUI family.

  BEQC = ((2U << 3) + 0) << kOpcodeShift,
  COP1 = ((2U << 3) + 1) << kOpcodeShift,  // Coprocessor 1 class.
  BEQL = ((2U << 3) + 4) << kOpcodeShift,
  BNEL = ((2U << 3) + 5) << kOpcodeShift,
  BLEZL = ((2U << 3) + 6) << kOpcodeShift,
  BGTZL = ((2U << 3) + 7) << kOpcodeShift,

  DADDI = ((3U << 3) + 0) << kOpcodeShift,  // This is also BNEC.
  SPECIAL2 = ((3U << 3) + 4) << kOpcodeShift,
  MSA = ((3U << 3) + 6) << kOpcodeShift,
  SPECIAL3 = ((3U << 3) + 7) << kOpcodeShift,

  LB = ((4U << 3) + 0) << kOpcodeShift,
  LH = ((4U << 3) + 1) << kOpcodeShift,
  LWL = ((4U << 3) + 2) << kOpcodeShift,
  LW = ((4U << 3) + 3) << kOpcodeShift,
  LBU = ((4U << 3) + 4) << kOpcodeShift,
  LHU = ((4U << 3) + 5) << kOpcodeShift,
  LWR = ((4U << 3) + 6) << kOpcodeShift,
  SB = ((5U << 3) + 0) << kOpcodeShift,
  SH = ((5U << 3) + 1) << kOpcodeShift,
  SWL = ((5U << 3) + 2) << kOpcodeShift,
  SW = ((5U << 3) + 3) << kOpcodeShift,
  SWR = ((5U << 3) + 6) << kOpcodeShift,

  LWC1 = ((6U << 3) + 1) << kOpcodeShift,
  BC = ((6U << 3) + 2) << kOpcodeShift,
  LDC1 = ((6U << 3) + 5) << kOpcodeShift,
  POP66 = ((6U << 3) + 6) << kOpcodeShift,  // beqzc, jic

  PREF = ((6U << 3) + 3) << kOpcodeShift,

  SWC1 = ((7U << 3) + 1) << kOpcodeShift,
  BALC = ((7U << 3) + 2) << kOpcodeShift,
  PCREL = ((7U << 3) + 3) << kOpcodeShift,
  SDC1 = ((7U << 3) + 5) << kOpcodeShift,
  POP76 = ((7U << 3) + 6) << kOpcodeShift,  // bnezc, jialc

  COP1X = ((1U << 4) + 3) << kOpcodeShift,

  // New r6 instruction.
  POP06 = BLEZ,   // bgeuc/bleuc, blezalc, bgezalc
  POP07 = BGTZ,   // bltuc/bgtuc, bgtzalc, bltzalc
  POP10 = ADDI,   // beqzalc, bovc, beqc
  POP26 = BLEZL,  // bgezc, blezc, bgec/blec
  POP27 = BGTZL,  // bgtzc, bltzc, bltc/bgtc
  POP30 = DADDI,  // bnezalc, bnvc, bnec
};

enum SecondaryField : uint32_t {
  // SPECIAL Encoding of Function Field.
  SLL = ((0U << 3) + 0),
  MOVCI = ((0U << 3) + 1),
  SRL = ((0U << 3) + 2),
  SRA = ((0U << 3) + 3),
  SLLV = ((0U << 3) + 4),
  LSA = ((0U << 3) + 5),
  SRLV = ((0U << 3) + 6),
  SRAV = ((0U << 3) + 7),

  JR = ((1U << 3) + 0),
  JALR = ((1U << 3) + 1),
  MOVZ = ((1U << 3) + 2),
  MOVN = ((1U << 3) + 3),
  BREAK = ((1U << 3) + 5),
  SYNC = ((1U << 3) + 7),

  MFHI = ((2U << 3) + 0),
  CLZ_R6 = ((2U << 3) + 0),
  CLO_R6 = ((2U << 3) + 1),
  MFLO = ((2U << 3) + 2),

  MULT = ((3U << 3) + 0),
  MULTU = ((3U << 3) + 1),
  DIV = ((3U << 3) + 2),
  DIVU = ((3U << 3) + 3),

  ADD = ((4U << 3) + 0),
  ADDU = ((4U << 3) + 1),
  SUB = ((4U << 3) + 2),
  SUBU = ((4U << 3) + 3),
  AND = ((4U << 3) + 4),
  OR = ((4U << 3) + 5),
  XOR = ((4U << 3) + 6),
  NOR = ((4U << 3) + 7),

  SLT = ((5U << 3) + 2),
  SLTU = ((5U << 3) + 3),

  TGE = ((6U << 3) + 0),
  TGEU = ((6U << 3) + 1),
  TLT = ((6U << 3) + 2),
  TLTU = ((6U << 3) + 3),
  TEQ = ((6U << 3) + 4),
  SELEQZ_S = ((6U << 3) + 5),
  TNE = ((6U << 3) + 6),
  SELNEZ_S = ((6U << 3) + 7),

  // Multiply integers in r6.
  MUL_MUH = ((3U << 3) + 0),    // MUL, MUH.
  MUL_MUH_U = ((3U << 3) + 1),  // MUL_U, MUH_U.
  RINT = ((3U << 3) + 2),

  MUL_OP = ((0U << 3) + 2),
  MUH_OP = ((0U << 3) + 3),
  DIV_OP = ((0U << 3) + 2),
  MOD_OP = ((0U << 3) + 3),

  DIV_MOD = ((3U << 3) + 2),
  DIV_MOD_U = ((3U << 3) + 3),

  // SPECIAL2 Encoding of Function Field.
  MUL = ((0U << 3) + 2),
  CLZ = ((4U << 3) + 0),
  CLO = ((4U << 3) + 1),

  // SPECIAL3 Encoding of Function Field.
  EXT = ((0U << 3) + 0),
  INS = ((0U << 3) + 4),
  BSHFL = ((4U << 3) + 0),

  // SPECIAL3 Encoding of sa Field.
  BITSWAP = ((0U << 3) + 0),
  ALIGN = ((0U << 3) + 2),
  WSBH = ((0U << 3) + 2),
  SEB = ((2U << 3) + 0),
  SEH = ((3U << 3) + 0),

  // REGIMM  encoding of rt Field.
  BLTZ = ((0U << 3) + 0) << 16,
  BGEZ = ((0U << 3) + 1) << 16,
  BLTZAL = ((2U << 3) + 0) << 16,
  BGEZAL = ((2U << 3) + 1) << 16,
  BGEZALL = ((2U << 3) + 3) << 16,

  // COP1 Encoding of rs Field.
  MFC1 = ((0U << 3) + 0) << 21,
  CFC1 = ((0U << 3) + 2) << 21,
  MFHC1 = ((0U << 3) + 3) << 21,
  MTC1 = ((0U << 3) + 4) << 21,
  CTC1 = ((0U << 3) + 6) << 21,
  MTHC1 = ((0U << 3) + 7) << 21,
  BC1 = ((1U << 3) + 0) << 21,
  S = ((2U << 3) + 0) << 21,
  D = ((2U << 3) + 1) << 21,
  W = ((2U << 3) + 4) << 21,
  L = ((2U << 3) + 5) << 21,
  PS = ((2U << 3) + 6) << 21,
  // COP1 Encoding of Function Field When rs=S.

  ADD_S = ((0U << 3) + 0),
  SUB_S = ((0U << 3) + 1),
  MUL_S = ((0U << 3) + 2),
  DIV_S = ((0U << 3) + 3),
  ABS_S = ((0U << 3) + 5),
  SQRT_S = ((0U << 3) + 4),
  MOV_S = ((0U << 3) + 6),
  NEG_S = ((0U << 3) + 7),
  ROUND_L_S = ((1U << 3) + 0),
  TRUNC_L_S = ((1U << 3) + 1),
  CEIL_L_S = ((1U << 3) + 2),
  FLOOR_L_S = ((1U << 3) + 3),
  ROUND_W_S = ((1U << 3) + 4),
  TRUNC_W_S = ((1U << 3) + 5),
  CEIL_W_S = ((1U << 3) + 6),
  FLOOR_W_S = ((1U << 3) + 7),
  RECIP_S = ((2U << 3) + 5),
  RSQRT_S = ((2U << 3) + 6),
  MADDF_S = ((3U << 3) + 0),
  MSUBF_S = ((3U << 3) + 1),
  CLASS_S = ((3U << 3) + 3),
  CVT_D_S = ((4U << 3) + 1),
  CVT_W_S = ((4U << 3) + 4),
  CVT_L_S = ((4U << 3) + 5),
  CVT_PS_S = ((4U << 3) + 6),

  // COP1 Encoding of Function Field When rs=D.
  ADD_D = ((0U << 3) + 0),
  SUB_D = ((0U << 3) + 1),
  MUL_D = ((0U << 3) + 2),
  DIV_D = ((0U << 3) + 3),
  SQRT_D = ((0U << 3) + 4),
  ABS_D = ((0U << 3) + 5),
  MOV_D = ((0U << 3) + 6),
  NEG_D = ((0U << 3) + 7),
  ROUND_L_D = ((1U << 3) + 0),
  TRUNC_L_D = ((1U << 3) + 1),
  CEIL_L_D = ((1U << 3) + 2),
  FLOOR_L_D = ((1U << 3) + 3),
  ROUND_W_D = ((1U << 3) + 4),
  TRUNC_W_D = ((1U << 3) + 5),
  CEIL_W_D = ((1U << 3) + 6),
  FLOOR_W_D = ((1U << 3) + 7),
  RECIP_D = ((2U << 3) + 5),
  RSQRT_D = ((2U << 3) + 6),
  MADDF_D = ((3U << 3) + 0),
  MSUBF_D = ((3U << 3) + 1),
  CLASS_D = ((3U << 3) + 3),
  MIN = ((3U << 3) + 4),
  MINA = ((3U << 3) + 5),
  MAX = ((3U << 3) + 6),
  MAXA = ((3U << 3) + 7),
  CVT_S_D = ((4U << 3) + 0),
  CVT_W_D = ((4U << 3) + 4),
  CVT_L_D = ((4U << 3) + 5),
  C_F_D = ((6U << 3) + 0),
  C_UN_D = ((6U << 3) + 1),
  C_EQ_D = ((6U << 3) + 2),
  C_UEQ_D = ((6U << 3) + 3),
  C_OLT_D = ((6U << 3) + 4),
  C_ULT_D = ((6U << 3) + 5),
  C_OLE_D = ((6U << 3) + 6),
  C_ULE_D = ((6U << 3) + 7),

  // COP1 Encoding of Function Field When rs=W or L.
  CVT_S_W = ((4U << 3) + 0),
  CVT_D_W = ((4U << 3) + 1),
  CVT_S_L = ((4U << 3) + 0),
  CVT_D_L = ((4U << 3) + 1),
  BC1EQZ = ((2U << 2) + 1) << 21,
  BC1NEZ = ((3U << 2) + 1) << 21,
  // COP1 CMP positive predicates Bit 5..4 = 00.
  CMP_AF = ((0U << 3) + 0),
  CMP_UN = ((0U << 3) + 1),
  CMP_EQ = ((0U << 3) + 2),
  CMP_UEQ = ((0U << 3) + 3),
  CMP_LT = ((0U << 3) + 4),
  CMP_ULT = ((0U << 3) + 5),
  CMP_LE = ((0U << 3) + 6),
  CMP_ULE = ((0U << 3) + 7),
  CMP_SAF = ((1U << 3) + 0),
  CMP_SUN = ((1U << 3) + 1),
  CMP_SEQ = ((1U << 3) + 2),
  CMP_SUEQ = ((1U << 3) + 3),
  CMP_SSLT = ((1U << 3) + 4),
  CMP_SSULT = ((1U << 3) + 5),
  CMP_SLE = ((1U << 3) + 6),
  CMP_SULE = ((1U << 3) + 7),
  // COP1 CMP negative predicates Bit 5..4 = 01.
  CMP_AT = ((2U << 3) + 0),  // Reserved, not implemented.
  CMP_OR = ((2U << 3) + 1),
  CMP_UNE = ((2U << 3) + 2),
  CMP_NE = ((2U << 3) + 3),
  CMP_UGE = ((2U << 3) + 4),  // Reserved, not implemented.
  CMP_OGE = ((2U << 3) + 5),  // Reserved, not implemented.
  CMP_UGT = ((2U << 3) + 6),  // Reserved, not implemented.
  CMP_OGT = ((2U << 3) + 7),  // Reserved, not implemented.
  CMP_SAT = ((3U << 3) + 0),  // Reserved, not implemented.
  CMP_SOR = ((3U << 3) + 1),
  CMP_SUNE = ((3U << 3) + 2),
  CMP_SNE = ((3U << 3) + 3),
  CMP_SUGE = ((3U << 3) + 4),  // Reserved, not implemented.
  CMP_SOGE = ((3U << 3) + 5),  // Reserved, not implemented.
  CMP_SUGT = ((3U << 3) + 6),  // Reserved, not implemented.
  CMP_SOGT = ((3U << 3) + 7),  // Reserved, not implemented.

  SEL = ((2U << 3) + 0),
  MOVZ_C = ((2U << 3) + 2),
  MOVN_C = ((2U << 3) + 3),
  SELEQZ_C = ((2U << 3) + 4),  // COP1 on FPR registers.
  MOVF = ((2U << 3) + 1),      // Function field for MOVT.fmt and MOVF.fmt
  SELNEZ_C = ((2U << 3) + 7),  // COP1 on FPR registers.
                               // COP1 Encoding of Function Field When rs=PS.

  // COP1X Encoding of Function Field.
  MADD_S = ((4U << 3) + 0),
  MADD_D = ((4U << 3) + 1),
  MSUB_S = ((5U << 3) + 0),
  MSUB_D = ((5U << 3) + 1),

  // PCREL Encoding of rt Field.
  ADDIUPC = ((0U << 2) + 0),
  LWPC = ((0U << 2) + 1),
  AUIPC = ((3U << 3) + 6),
  ALUIPC = ((3U << 3) + 7),

  // POP66 Encoding of rs Field.
  JIC = ((0U << 5) + 0),

  // POP76 Encoding of rs Field.
  JIALC = ((0U << 5) + 0),

  // COP1 Encoding of rs Field for MSA Branch Instructions
  BZ_V = (((1U << 3) + 3) << kRsShift),
  BNZ_V = (((1U << 3) + 7) << kRsShift),
  BZ_B = (((3U << 3) + 0) << kRsShift),
  BZ_H = (((3U << 3) + 1) << kRsShift),
  BZ_W = (((3U << 3) + 2) << kRsShift),
  BZ_D = (((3U << 3) + 3) << kRsShift),
  BNZ_B = (((3U << 3) + 4) << kRsShift),
  BNZ_H = (((3U << 3) + 5) << kRsShift),
  BNZ_W = (((3U << 3) + 6) << kRsShift),
  BNZ_D = (((3U << 3) + 7) << kRsShift),

  // MSA: Operation Field for MI10 Instruction Formats
  MSA_LD = (8U << 2),
  MSA_ST = (9U << 2),
  LD_B = ((8U << 2) + 0),
  LD_H = ((8U << 2) + 1),
  LD_W = ((8U << 2) + 2),
  LD_D = ((8U << 2) + 3),
  ST_B = ((9U << 2) + 0),
  ST_H = ((9U << 2) + 1),
  ST_W = ((9U << 2) + 2),
  ST_D = ((9U << 2) + 3),

  // MSA: Operation Field for I5 Instruction Format
  ADDVI = ((0U << 23) + 6),
  SUBVI = ((1U << 23) + 6),
  MAXI_S = ((2U << 23) + 6),
  MAXI_U = ((3U << 23) + 6),
  MINI_S = ((4U << 23) + 6),
  MINI_U = ((5U << 23) + 6),
  CEQI = ((0U << 23) + 7),
  CLTI_S = ((2U << 23) + 7),
  CLTI_U = ((3U << 23) + 7),
  CLEI_S = ((4U << 23) + 7),
  CLEI_U = ((5U << 23) + 7),
  LDI = ((6U << 23) + 7),  // I10 instruction format
  I5_DF_b = (0U << 21),
  I5_DF_h = (1U << 21),
  I5_DF_w = (2U << 21),
  I5_DF_d = (3U << 21),

  // MSA: Operation Field for I8 Instruction Format
  ANDI_B = ((0U << 24) + 0),
  ORI_B = ((1U << 24) + 0),
  NORI_B = ((2U << 24) + 0),
  XORI_B = ((3U << 24) + 0),
  BMNZI_B = ((0U << 24) + 1),
  BMZI_B = ((1U << 24) + 1),
  BSELI_B = ((2U << 24) + 1),
  SHF_B = ((0U << 24) + 2),
  SHF_H = ((1U << 24) + 2),
  SHF_W = ((2U << 24) + 2),

  MSA_VEC_2R_2RF_MINOR = ((3U << 3) + 6),

  // MSA: Operation Field for VEC Instruction Formats
  AND_V = (((0U << 2) + 0) << 21),
  OR_V = (((0U << 2) + 1) << 21),
  NOR_V = (((0U << 2) + 2) << 21),
  XOR_V = (((0U << 2) + 3) << 21),
  BMNZ_V = (((1U << 2) + 0) << 21),
  BMZ_V = (((1U << 2) + 1) << 21),
  BSEL_V = (((1U << 2) + 2) << 21),

  // MSA: Operation Field for 2R Instruction Formats
  MSA_2R_FORMAT = (((6U << 2) + 0) << 21),
  FILL = (0U << 18),
  PCNT = (1U << 18),
  NLOC = (2U << 18),
  NLZC = (3U << 18),
  MSA_2R_DF_b = (0U << 16),
  MSA_2R_DF_h = (1U << 16),
  MSA_2R_DF_w = (2U << 16),
  MSA_2R_DF_d = (3U << 16),

  // MSA: Operation Field for 2RF Instruction Formats
  MSA_2RF_FORMAT = (((6U << 2) + 1) << 21),
  FCLASS = (0U << 17),
  FTRUNC_S = (1U << 17),
  FTRUNC_U = (2U << 17),
  FSQRT = (3U << 17),
  FRSQRT = (4U << 17),
  FRCP = (5U << 17),
  FRINT = (6U << 17),
  FLOG2 = (7U << 17),
  FEXUPL = (8U << 17),
  FEXUPR = (9U << 17),
  FFQL = (10U << 17),
  FFQR = (11U << 17),
  FTINT_S = (12U << 17),
  FTINT_U = (13U << 17),
  FFINT_S = (14U << 17),
  FFINT_U = (15U << 17),
  MSA_2RF_DF_w = (0U << 16),
  MSA_2RF_DF_d = (1U << 16),

  // MSA: Operation Field for 3R Instruction Format
  SLL_MSA = ((0U << 23) + 13),
  SRA_MSA = ((1U << 23) + 13),
  SRL_MSA = ((2U << 23) + 13),
  BCLR = ((3U << 23) + 13),
  BSET = ((4U << 23) + 13),
  BNEG = ((5U << 23) + 13),
  BINSL = ((6U << 23) + 13),
  BINSR = ((7U << 23) + 13),
  ADDV = ((0U << 23) + 14),
  SUBV = ((1U << 23) + 14),
  MAX_S = ((2U << 23) + 14),
  MAX_U = ((3U << 23) + 14),
  MIN_S = ((4U << 23) + 14),
  MIN_U = ((5U << 23) + 14),
  MAX_A = ((6U << 23) + 14),
  MIN_A = ((7U << 23) + 14),
  CEQ = ((0U << 23) + 15),
  CLT_S = ((2U << 23) + 15),
  CLT_U = ((3U << 23) + 15),
  CLE_S = ((4U << 23) + 15),
  CLE_U = ((5U << 23) + 15),
  ADD_A = ((0U << 23) + 16),
  ADDS_A = ((1U << 23) + 16),
  ADDS_S = ((2U << 23) + 16),
  ADDS_U = ((3U << 23) + 16),
  AVE_S = ((4U << 23) + 16),
  AVE_U = ((5U << 23) + 16),
  AVER_S = ((6U << 23) + 16),
  AVER_U = ((7U << 23) + 16),
  SUBS_S = ((0U << 23) + 17),
  SUBS_U = ((1U << 23) + 17),
  SUBSUS_U = ((2U << 23) + 17),
  SUBSUU_S = ((3U << 23) + 17),
  ASUB_S = ((4U << 23) + 17),
  ASUB_U = ((5U << 23) + 17),
  MULV = ((0U << 23) + 18),
  MADDV = ((1U << 23) + 18),
  MSUBV = ((2U << 23) + 18),
  DIV_S_MSA = ((4U << 23) + 18),
  DIV_U = ((5U << 23) + 18),
  MOD_S = ((6U << 23) + 18),
  MOD_U = ((7U << 23) + 18),
  DOTP_S = ((0U << 23) + 19),
  DOTP_U = ((1U << 23) + 19),
  DPADD_S = ((2U << 23) + 19),
  DPADD_U = ((3U << 23) + 19),
  DPSUB_S = ((4U << 23) + 19),
  DPSUB_U = ((5U << 23) + 19),
  SLD = ((0U << 23) + 20),
  SPLAT = ((1U << 23) + 20),
  PCKEV = ((2U << 23) + 20),
  PCKOD = ((3U << 23) + 20),
  ILVL = ((4U << 23) + 20),
  ILVR = ((5U << 23) + 20),
  ILVEV = ((6U << 23) + 20),
  ILVOD = ((7U << 23) + 20),
  VSHF = ((0U << 23) + 21),
  SRAR = ((1U << 23) + 21),
  SRLR = ((2U << 23) + 21),
  HADD_S = ((4U << 23) + 21),
  HADD_U = ((5U << 23) + 21),
  HSUB_S = ((6U << 23) + 21),
  HSUB_U = ((7U << 23) + 21),
  MSA_3R_DF_b = (0U << 21),
  MSA_3R_DF_h = (1U << 21),
  MSA_3R_DF_w = (2U << 21),
  MSA_3R_DF_d = (3U << 21),

  // MSA: Operation Field for 3RF Instruction Format
  FCAF = ((0U << 22) + 26),
  FCUN = ((1U << 22) + 26),
  FCEQ = ((2U << 22) + 26),
  FCUEQ = ((3U << 22) + 26),
  FCLT = ((4U << 22) + 26),
  FCULT = ((5U << 22) + 26),
  FCLE = ((6U << 22) + 26),
  FCULE = ((7U << 22) + 26),
  FSAF = ((8U << 22) + 26),
  FSUN = ((9U << 22) + 26),
  FSEQ = ((10U << 22) + 26),
  FSUEQ = ((11U << 22) + 26),
  FSLT = ((12U << 22) + 26),
  FSULT = ((13U << 22) + 26),
  FSLE = ((14U << 22) + 26),
  FSULE = ((15U << 22) + 26),
  FADD = ((0U << 22) + 27),
  FSUB = ((1U << 22) + 27),
  FMUL = ((2U << 22) + 27),
  FDIV = ((3U << 22) + 27),
  FMADD = ((4U << 22) + 27),
  FMSUB = ((5U << 22) + 27),
  FEXP2 = ((7U << 22) + 27),
  FEXDO = ((8U << 22) + 27),
  FTQ = ((10U << 22) + 27),
  FMIN = ((12U << 22) + 27),
  FMIN_A = ((13U << 22) + 27),
  FMAX = ((14U << 22) + 27),
  FMAX_A = ((15U << 22) + 27),
  FCOR = ((1U << 22) + 28),
  FCUNE = ((2U << 22) + 28),
  FCNE = ((3U << 22) + 28),
  MUL_Q = ((4U << 22) + 28),
  MADD_Q = ((5U << 22) + 28),
  MSUB_Q = ((6U << 22) + 28),
  FSOR = ((9U << 22) + 28),
  FSUNE = ((10U << 22) + 28),
  FSNE = ((11U << 22) + 28),
  MULR_Q = ((12U << 22) + 28),
  MADDR_Q = ((13U << 22) + 28),
  MSUBR_Q = ((14U << 22) + 28),

  // MSA: Operation Field for ELM Instruction Format
  MSA_ELM_MINOR = ((3U << 3) + 1),
  SLDI = (0U << 22),
  CTCMSA = ((0U << 22) | (62U << 16)),
  SPLATI = (1U << 22),
  CFCMSA = ((1U << 22) | (62U << 16)),
  COPY_S = (2U << 22),
  MOVE_V = ((2U << 22) | (62U << 16)),
  COPY_U = (3U << 22),
  INSERT = (4U << 22),
  INSVE = (5U << 22),
  ELM_DF_B = ((0U << 4) << 16),
  ELM_DF_H = ((4U << 3) << 16),
  ELM_DF_W = ((12U << 2) << 16),
  ELM_DF_D = ((28U << 1) << 16),

  // MSA: Operation Field for BIT Instruction Format
  SLLI = ((0U << 23) + 9),
  SRAI = ((1U << 23) + 9),
  SRLI = ((2U << 23) + 9),
  BCLRI = ((3U << 23) + 9),
  BSETI = ((4U << 23) + 9),
  BNEGI = ((5U << 23) + 9),
  BINSLI = ((6U << 23) + 9),
  BINSRI = ((7U << 23) + 9),
  SAT_S = ((0U << 23) + 10),
  SAT_U = ((1U << 23) + 10),
  SRARI = ((2U << 23) + 10),
  SRLRI = ((3U << 23) + 10),
  BIT_DF_b = ((14U << 3) << 16),
  BIT_DF_h = ((6U << 4) << 16),
  BIT_DF_w = ((2U << 5) << 16),
  BIT_DF_d = ((0U << 6) << 16),

  NULLSF = 0U
};

enum MSAMinorOpcode : uint32_t {
  kMsaMinorUndefined = 0,
  kMsaMinorI8,
  kMsaMinorI5,
  kMsaMinorI10,
  kMsaMinorBIT,
  kMsaMinor3R,
  kMsaMinor3RF,
  kMsaMinorELM,
  kMsaMinorVEC,
  kMsaMinor2R,
  kMsaMinor2RF,
  kMsaMinorMI10
};

// ----- Emulated conditions.
// On MIPS we use this enum to abstract from conditional branch instructions.
// The 'U' prefix is used to specify unsigned comparisons.
// Opposite conditions must be paired as odd/even numbers
// because 'NegateCondition' function flips LSB to negate condition.
enum Condition {
  // Any value < 0 is considered no_condition.
  kNoCondition = -1,
  overflow = 0,
  no_overflow = 1,
  Uless = 2,
  Ugreater_equal = 3,
  Uless_equal = 4,
  Ugreater = 5,
  equal = 6,
  not_equal = 7,  // Unordered or Not Equal.
  negative = 8,
  positive = 9,
  parity_even = 10,
  parity_odd = 11,
  less = 12,
  greater_equal = 13,
  less_equal = 14,
  greater = 15,
  ueq = 16,  // Unordered or Equal.
  ogl = 17,  // Ordered and Not Equal.
  cc_always = 18,

  // Aliases.
  carry = Uless,
  not_carry = Ugreater_equal,
  zero = equal,
  eq = equal,
  not_zero = not_equal,
  ne = not_equal,
  nz = not_equal,
  sign = negative,
  not_sign = positive,
  mi = negative,
  pl = positive,
  hi = Ugreater,
  ls = Uless_equal,
  ge = greater_equal,
  lt = less,
  gt = greater,
  le = less_equal,
  hs = Ugreater_equal,
  lo = Uless,
  al = cc_always,
  ult = Uless,
  uge = Ugreater_equal,
  ule = Uless_equal,
  ugt = Ugreater,
  cc_default = kNoCondition
};


// Returns the equivalent of !cc.
// Negation of the default kNoCondition (-1) results in a non-default
// no_condition value (-2). As long as tests for no_condition check
// for condition < 0, this will work as expected.
inline Condition NegateCondition(Condition cc) {
  DCHECK(cc != cc_always);
  return static_cast<Condition>(cc ^ 1);
}


inline Condition NegateFpuCondition(Condition cc) {
  DCHECK(cc != cc_always);
  switch (cc) {
    case ult:
      return ge;
    case ugt:
      return le;
    case uge:
      return lt;
    case ule:
      return gt;
    case lt:
      return uge;
    case gt:
      return ule;
    case ge:
      return ult;
    case le:
      return ugt;
    case eq:
      return ne;
    case ne:
      return eq;
    case ueq:
      return ogl;
    case ogl:
      return ueq;
    default:
      return cc;
  }
}


// Commute a condition such that {a cond b == b cond' a}.
inline Condition CommuteCondition(Condition cc) {
  switch (cc) {
    case Uless:
      return Ugreater;
    case Ugreater:
      return Uless;
    case Ugreater_equal:
      return Uless_equal;
    case Uless_equal:
      return Ugreater_equal;
    case less:
      return greater;
    case greater:
      return less;
    case greater_equal:
      return less_equal;
    case less_equal:
      return greater_equal;
    default:
      return cc;
  }
}


// ----- Coprocessor conditions.
enum FPUCondition {
  kNoFPUCondition = -1,

  F = 0x00,    // False.
  UN = 0x01,   // Unordered.
  EQ = 0x02,   // Equal.
  UEQ = 0x03,  // Unordered or Equal.
  OLT = 0x04,  // Ordered or Less Than, on Mips release < 6.
  LT = 0x04,   // Ordered or Less Than, on Mips release >= 6.
  ULT = 0x05,  // Unordered or Less Than.
  OLE = 0x06,  // Ordered or Less Than or Equal, on Mips release < 6.
  LE = 0x06,   // Ordered or Less Than or Equal, on Mips release >= 6.
  ULE = 0x07,  // Unordered or Less Than or Equal.

  // Following constants are available on Mips release >= 6 only.
  ORD = 0x11,  // Ordered, on Mips release >= 6.
  UNE = 0x12,  // Not equal, on Mips release >= 6.
  NE = 0x13,   // Ordered Greater Than or Less Than. on Mips >= 6 only.
};


// FPU rounding modes.
enum FPURoundingMode {
  RN = 0 << 0,  // Round to Nearest.
  RZ = 1 << 0,  // Round towards zero.
  RP = 2 << 0,  // Round towards Plus Infinity.
  RM = 3 << 0,  // Round towards Minus Infinity.

  // Aliases.
  kRoundToNearest = RN,
  kRoundToZero = RZ,
  kRoundToPlusInf = RP,
  kRoundToMinusInf = RM,

  mode_round = RN,
  mode_ceil = RP,
  mode_floor = RM,
  mode_trunc = RZ
};

const uint32_t kFPURoundingModeMask = 3 << 0;

enum CheckForInexactConversion {
  kCheckForInexactConversion,
  kDontCheckForInexactConversion
};

enum class MaxMinKind : int { kMin = 0, kMax = 1 };

// -----------------------------------------------------------------------------
// Hints.

// Branch hints are not used on the MIPS.  They are defined so that they can
// appear in shared function signatures, but will be ignored in MIPS
// implementations.
enum Hint {
  no_hint = 0
};


inline Hint NegateHint(Hint hint) {
  return no_hint;
}


// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.
// These constants are declared in assembler-mips.cc, as they use named
// registers and other constants.

// addiu(sp, sp, 4) aka Pop() operation or part of Pop(r)
// operations as post-increment of sp.
extern const Instr kPopInstruction;
// addiu(sp, sp, -4) part of Push(r) operation as pre-decrement of sp.
extern const Instr kPushInstruction;
// sw(r, MemOperand(sp, 0))
extern const Instr kPushRegPattern;
// lw(r, MemOperand(sp, 0))
extern const Instr kPopRegPattern;
extern const Instr kLwRegFpOffsetPattern;
extern const Instr kSwRegFpOffsetPattern;
extern const Instr kLwRegFpNegOffsetPattern;
extern const Instr kSwRegFpNegOffsetPattern;
// A mask for the Rt register for push, pop, lw, sw instructions.
extern const Instr kRtMask;
extern const Instr kLwSwInstrTypeMask;
extern const Instr kLwSwInstrArgumentMask;
extern const Instr kLwSwOffsetMask;

// Break 0xfffff, reserved for redirected real time call.
const Instr rtCallRedirInstr = SPECIAL | BREAK | call_rt_redirected << 6;
// A nop instruction. (Encoding of sll 0 0 0).
const Instr nopInstr = 0;

static constexpr uint64_t OpcodeToBitNumber(Opcode opcode) {
  return 1ULL << (static_cast<uint32_t>(opcode) >> kOpcodeShift);
}

class InstructionBase {
 public:
  enum {
    kInstrSize = 4,
    kInstrSizeLog2 = 2,
    // On MIPS PC cannot actually be directly accessed. We behave as if PC was
    // always the value of the current instruction being executed.
    kPCReadOffset = 0
  };

  // Instruction type.
  enum Type { kRegisterType, kImmediateType, kJumpType, kUnsupported = -1 };

  // Get the raw instruction bits.
  inline Instr InstructionBits() const {
    return *reinterpret_cast<const Instr*>(this);
  }

  // Set the raw instruction bits to value.
  inline void SetInstructionBits(Instr value) {
    *reinterpret_cast<Instr*>(this) = value;
  }

  // Read one particular bit out of the instruction bits.
  inline int Bit(int nr) const {
    return (InstructionBits() >> nr) & 1;
  }

  // Read a bit field out of the instruction bits.
  inline int Bits(int hi, int lo) const {
    return (InstructionBits() >> lo) & ((2U << (hi - lo)) - 1);
  }


  static constexpr uint64_t kOpcodeImmediateTypeMask =
      OpcodeToBitNumber(REGIMM) | OpcodeToBitNumber(BEQ) |
      OpcodeToBitNumber(BNE) | OpcodeToBitNumber(BLEZ) |
      OpcodeToBitNumber(BGTZ) | OpcodeToBitNumber(ADDI) |
      OpcodeToBitNumber(DADDI) | OpcodeToBitNumber(ADDIU) |
      OpcodeToBitNumber(SLTI) | OpcodeToBitNumber(SLTIU) |
      OpcodeToBitNumber(ANDI) | OpcodeToBitNumber(ORI) |
      OpcodeToBitNumber(XORI) | OpcodeToBitNumber(LUI) |
      OpcodeToBitNumber(BEQL) | OpcodeToBitNumber(BNEL) |
      OpcodeToBitNumber(BLEZL) | OpcodeToBitNumber(BGTZL) |
      OpcodeToBitNumber(POP66) | OpcodeToBitNumber(POP76) |
      OpcodeToBitNumber(LB) | OpcodeToBitNumber(LH) | OpcodeToBitNumber(LWL) |
      OpcodeToBitNumber(LW) | OpcodeToBitNumber(LBU) | OpcodeToBitNumber(LHU) |
      OpcodeToBitNumber(LWR) | OpcodeToBitNumber(SB) | OpcodeToBitNumber(SH) |
      OpcodeToBitNumber(SWL) | OpcodeToBitNumber(SW) | OpcodeToBitNumber(SWR) |
      OpcodeToBitNumber(LWC1) | OpcodeToBitNumber(LDC1) |
      OpcodeToBitNumber(SWC1) | OpcodeToBitNumber(SDC1) |
      OpcodeToBitNumber(PCREL) | OpcodeToBitNumber(BC) |
      OpcodeToBitNumber(BALC);

#define FunctionFieldToBitNumber(function) (1ULL << function)

  static const uint64_t kFunctionFieldRegisterTypeMask =
      FunctionFieldToBitNumber(JR) | FunctionFieldToBitNumber(JALR) |
      FunctionFieldToBitNumber(BREAK) | FunctionFieldToBitNumber(SLL) |
      FunctionFieldToBitNumber(SRL) | FunctionFieldToBitNumber(SRA) |
      FunctionFieldToBitNumber(SLLV) | FunctionFieldToBitNumber(SRLV) |
      FunctionFieldToBitNumber(SRAV) | FunctionFieldToBitNumber(LSA) |
      FunctionFieldToBitNumber(MFHI) | FunctionFieldToBitNumber(MFLO) |
      FunctionFieldToBitNumber(MULT) | FunctionFieldToBitNumber(MULTU) |
      FunctionFieldToBitNumber(DIV) | FunctionFieldToBitNumber(DIVU) |
      FunctionFieldToBitNumber(ADD) | FunctionFieldToBitNumber(ADDU) |
      FunctionFieldToBitNumber(SUB) | FunctionFieldToBitNumber(SUBU) |
      FunctionFieldToBitNumber(AND) | FunctionFieldToBitNumber(OR) |
      FunctionFieldToBitNumber(XOR) | FunctionFieldToBitNumber(NOR) |
      FunctionFieldToBitNumber(SLT) | FunctionFieldToBitNumber(SLTU) |
      FunctionFieldToBitNumber(TGE) | FunctionFieldToBitNumber(TGEU) |
      FunctionFieldToBitNumber(TLT) | FunctionFieldToBitNumber(TLTU) |
      FunctionFieldToBitNumber(TEQ) | FunctionFieldToBitNumber(TNE) |
      FunctionFieldToBitNumber(MOVZ) | FunctionFieldToBitNumber(MOVN) |
      FunctionFieldToBitNumber(MOVCI) | FunctionFieldToBitNumber(SELEQZ_S) |
      FunctionFieldToBitNumber(SELNEZ_S) | FunctionFieldToBitNumber(SYNC);

  // Accessors for the different named fields used in the MIPS encoding.
  inline Opcode OpcodeValue() const {
    return static_cast<Opcode>(
        Bits(kOpcodeShift + kOpcodeBits - 1, kOpcodeShift));
  }

  inline int FunctionFieldRaw() const {
    return InstructionBits() & kFunctionFieldMask;
  }

  // Return the fields at their original place in the instruction encoding.
  inline Opcode OpcodeFieldRaw() const {
    return static_cast<Opcode>(InstructionBits() & kOpcodeMask);
  }

  // Safe to call within InstructionType().
  inline int RsFieldRawNoAssert() const {
    return InstructionBits() & kRsFieldMask;
  }

  inline int SaFieldRaw() const { return InstructionBits() & kSaFieldMask; }

  // Get the encoding type of the instruction.
  inline Type InstructionType() const;

  inline MSAMinorOpcode MSAMinorOpcodeField() const {
    int op = this->FunctionFieldRaw();
    switch (op) {
      case 0:
      case 1:
      case 2:
        return kMsaMinorI8;
      case 6:
        return kMsaMinorI5;
      case 7:
        return (((this->InstructionBits() & kMsaI5I10Mask) == LDI)
                    ? kMsaMinorI10
                    : kMsaMinorI5);
      case 9:
      case 10:
        return kMsaMinorBIT;
      case 13:
      case 14:
      case 15:
      case 16:
      case 17:
      case 18:
      case 19:
      case 20:
      case 21:
        return kMsaMinor3R;
      case 25:
        return kMsaMinorELM;
      case 26:
      case 27:
      case 28:
        return kMsaMinor3RF;
      case 30:
        switch (this->RsFieldRawNoAssert()) {
          case MSA_2R_FORMAT:
            return kMsaMinor2R;
          case MSA_2RF_FORMAT:
            return kMsaMinor2RF;
          default:
            return kMsaMinorVEC;
        }
        break;
      case 32:
      case 33:
      case 34:
      case 35:
      case 36:
      case 37:
      case 38:
      case 39:
        return kMsaMinorMI10;
      default:
        return kMsaMinorUndefined;
    }
  }

 protected:
  InstructionBase() {}
};

template <class T>
class InstructionGetters : public T {
 public:
  inline int RsValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType ||
           this->InstructionType() == InstructionBase::kImmediateType);
    return InstructionBase::Bits(kRsShift + kRsBits - 1, kRsShift);
  }

  inline int RtValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType ||
           this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kRtShift + kRtBits - 1, kRtShift);
  }

  inline int RdValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType);
    return this->Bits(kRdShift + kRdBits - 1, kRdShift);
  }

  inline int SaValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType);
    return this->Bits(kSaShift + kSaBits - 1, kSaShift);
  }

  inline int LsaSaValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType);
    return this->Bits(kSaShift + kLsaSaBits - 1, kSaShift);
  }

  inline int FunctionValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType ||
           this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kFunctionShift + kFunctionBits - 1, kFunctionShift);
  }

  inline int FdValue() const {
    return this->Bits(kFdShift + kFdBits - 1, kFdShift);
  }

  inline int FsValue() const {
    return this->Bits(kFsShift + kFsBits - 1, kFsShift);
  }

  inline int FtValue() const {
    return this->Bits(kFtShift + kFtBits - 1, kFtShift);
  }

  inline int FrValue() const {
    return this->Bits(kFrShift + kFrBits - 1, kFrShift);
  }

  inline int WdValue() const {
    return this->Bits(kWdShift + kWdBits - 1, kWdShift);
  }

  inline int WsValue() const {
    return this->Bits(kWsShift + kWsBits - 1, kWsShift);
  }

  inline int WtValue() const {
    return this->Bits(kWtShift + kWtBits - 1, kWtShift);
  }

  inline int Bp2Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType);
    return this->Bits(kBp2Shift + kBp2Bits - 1, kBp2Shift);
  }

  // Float Compare condition code instruction bits.
  inline int FCccValue() const {
    return this->Bits(kFCccShift + kFCccBits - 1, kFCccShift);
  }

  // Float Branch condition code instruction bits.
  inline int FBccValue() const {
    return this->Bits(kFBccShift + kFBccBits - 1, kFBccShift);
  }

  // Float Branch true/false instruction bit.
  inline int FBtrueValue() const {
    return this->Bits(kFBtrueShift + kFBtrueBits - 1, kFBtrueShift);
  }

  // Return the fields at their original place in the instruction encoding.
  inline Opcode OpcodeFieldRaw() const {
    return static_cast<Opcode>(this->InstructionBits() & kOpcodeMask);
  }

  inline int RsFieldRaw() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType ||
           this->InstructionType() == InstructionBase::kImmediateType);
    return this->InstructionBits() & kRsFieldMask;
  }

  inline int RtFieldRaw() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType ||
           this->InstructionType() == InstructionBase::kImmediateType);
    return this->InstructionBits() & kRtFieldMask;
  }

  inline int RdFieldRaw() const {
    DCHECK(this->InstructionType() == InstructionBase::kRegisterType);
    return this->InstructionBits() & kRdFieldMask;
  }

  inline int SaFieldRaw() const {
    return this->InstructionBits() & kSaFieldMask;
  }

  inline int FunctionFieldRaw() const {
    return this->InstructionBits() & kFunctionFieldMask;
  }

  // Get the secondary field according to the opcode.
  inline int SecondaryValue() const {
    Opcode op = this->OpcodeFieldRaw();
    switch (op) {
      case SPECIAL:
      case SPECIAL2:
        return FunctionValue();
      case COP1:
        return RsValue();
      case REGIMM:
        return RtValue();
      default:
        return NULLSF;
    }
  }

  inline int32_t ImmValue(int bits) const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(bits - 1, 0);
  }

  inline int32_t Imm16Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kImm16Shift + kImm16Bits - 1, kImm16Shift);
  }

  inline int32_t Imm18Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kImm18Shift + kImm18Bits - 1, kImm18Shift);
  }

  inline int32_t Imm19Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kImm19Shift + kImm19Bits - 1, kImm19Shift);
  }

  inline int32_t Imm21Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kImm21Shift + kImm21Bits - 1, kImm21Shift);
  }

  inline int32_t Imm26Value() const {
    DCHECK((this->InstructionType() == InstructionBase::kJumpType) ||
           (this->InstructionType() == InstructionBase::kImmediateType));
    return this->Bits(kImm26Shift + kImm26Bits - 1, kImm26Shift);
  }

  inline int32_t MsaImm8Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kMsaImm8Shift + kMsaImm8Bits - 1, kMsaImm8Shift);
  }

  inline int32_t MsaImm5Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kMsaImm5Shift + kMsaImm5Bits - 1, kMsaImm5Shift);
  }

  inline int32_t MsaImm10Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kMsaImm10Shift + kMsaImm10Bits - 1, kMsaImm10Shift);
  }

  inline int32_t MsaImmMI10Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(kMsaImmMI10Shift + kMsaImmMI10Bits - 1, kMsaImmMI10Shift);
  }

  inline int32_t MsaBitDf() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    int32_t df_m = this->Bits(22, 16);
    if (((df_m >> 6) & 1U) == 0) {
      return 3;
    } else if (((df_m >> 5) & 3U) == 2) {
      return 2;
    } else if (((df_m >> 4) & 7U) == 6) {
      return 1;
    } else if (((df_m >> 3) & 15U) == 14) {
      return 0;
    } else {
      return -1;
    }
  }

  inline int32_t MsaBitMValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(16 + this->MsaBitDf() + 3, 16);
  }

  inline int32_t MsaElmDf() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    int32_t df_n = this->Bits(21, 16);
    if (((df_n >> 4) & 3U) == 0) {
      return 0;
    } else if (((df_n >> 3) & 7U) == 4) {
      return 1;
    } else if (((df_n >> 2) & 15U) == 12) {
      return 2;
    } else if (((df_n >> 1) & 31U) == 28) {
      return 3;
    } else {
      return -1;
    }
  }

  inline int32_t MsaElmNValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kImmediateType);
    return this->Bits(16 + 4 - this->MsaElmDf(), 16);
  }

  static bool IsForbiddenAfterBranchInstr(Instr instr);

  // Say if the instruction should not be used in a branch delay slot or
  // immediately after a compact branch.
  inline bool IsForbiddenAfterBranch() const {
    return IsForbiddenAfterBranchInstr(this->InstructionBits());
  }

  inline bool IsForbiddenInBranchDelay() const {
    return IsForbiddenAfterBranch();
  }

  // Say if the instruction 'links'. e.g. jal, bal.
  bool IsLinkingInstruction() const;
  // Say if the instruction is a break or a trap.
  bool IsTrap() const;

  inline bool IsMSABranchInstr() const {
    if (this->OpcodeFieldRaw() == COP1) {
      switch (this->RsFieldRaw()) {
        case BZ_V:
        case BZ_B:
        case BZ_H:
        case BZ_W:
        case BZ_D:
        case BNZ_V:
        case BNZ_B:
        case BNZ_H:
        case BNZ_W:
        case BNZ_D:
          return true;
        default:
          return false;
      }
    }
    return false;
  }

  inline bool IsMSAInstr() const {
    if (this->IsMSABranchInstr() || (this->OpcodeFieldRaw() == MSA))
      return true;
    return false;
  }
};

class Instruction : public InstructionGetters<InstructionBase> {
 public:
  // Instructions are read of out a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instruction.
  // Use the At(pc) function to create references to Instruction.
  static Instruction* At(byte* pc) {
    return reinterpret_cast<Instruction*>(pc);
  }

 private:
  // We need to prevent the creation of instances of class Instruction.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instruction);
};


// -----------------------------------------------------------------------------
// MIPS assembly various constants.

// C/C++ argument slots size.
const int kCArgSlotCount = 4;
const int kCArgsSlotsSize = kCArgSlotCount * Instruction::kInstrSize;
const int kInvalidStackOffset = -1;
// JS argument slots size.
const int kJSArgsSlotsSize = 0 * Instruction::kInstrSize;
// Assembly builtins argument slots size.
const int kBArgsSlotsSize = 0 * Instruction::kInstrSize;

const int kBranchReturnOffset = 2 * Instruction::kInstrSize;

InstructionBase::Type InstructionBase::InstructionType() const {
  switch (OpcodeFieldRaw()) {
    case SPECIAL:
      if (FunctionFieldToBitNumber(FunctionFieldRaw()) &
          kFunctionFieldRegisterTypeMask) {
        return kRegisterType;
      }
      return kUnsupported;
    case SPECIAL2:
      switch (FunctionFieldRaw()) {
        case MUL:
        case CLZ:
          return kRegisterType;
        default:
          return kUnsupported;
      }
      break;
    case SPECIAL3:
      switch (FunctionFieldRaw()) {
        case INS:
        case EXT:
          return kRegisterType;
        case BSHFL: {
          int sa = SaFieldRaw() >> kSaShift;
          switch (sa) {
            case BITSWAP:
            case WSBH:
            case SEB:
            case SEH:
              return kRegisterType;
          }
          sa >>= kBp2Bits;
          switch (sa) {
            case ALIGN:
              return kRegisterType;
            default:
              return kUnsupported;
          }
        }
        default:
          return kUnsupported;
      }
      break;
    case COP1:  // Coprocessor instructions.
      switch (RsFieldRawNoAssert()) {
        case BC1:  // Branch on coprocessor condition.
        case BC1EQZ:
        case BC1NEZ:
          return kImmediateType;
        // MSA Branch instructions
        case BZ_V:
        case BNZ_V:
        case BZ_B:
        case BZ_H:
        case BZ_W:
        case BZ_D:
        case BNZ_B:
        case BNZ_H:
        case BNZ_W:
        case BNZ_D:
          return kImmediateType;
        default:
          return kRegisterType;
      }
      break;
    case COP1X:
      return kRegisterType;

    // 26 bits immediate type instructions. e.g.: j imm26.
    case J:
    case JAL:
      return kJumpType;

    case MSA:
      switch (MSAMinorOpcodeField()) {
        case kMsaMinor3R:
        case kMsaMinor3RF:
        case kMsaMinorVEC:
        case kMsaMinor2R:
        case kMsaMinor2RF:
          return kRegisterType;
        default:
          return kImmediateType;
      }

    default:
        return kImmediateType;
  }
}

#undef OpcodeToBitNumber
#undef FunctionFieldToBitNumber

// -----------------------------------------------------------------------------
// Instructions.

template <class P>
bool InstructionGetters<P>::IsLinkingInstruction() const {
  uint32_t op = this->OpcodeFieldRaw();
  switch (op) {
    case JAL:
      return true;
    case POP76:
      if (this->RsFieldRawNoAssert() == JIALC)
        return true;  // JIALC
      else
        return false;  // BNEZC
    case REGIMM:
      switch (this->RtFieldRaw()) {
        case BGEZAL:
        case BLTZAL:
          return true;
        default:
          return false;
      }
    case SPECIAL:
      switch (this->FunctionFieldRaw()) {
        case JALR:
          return true;
        default:
          return false;
      }
    default:
      return false;
  }
}

template <class P>
bool InstructionGetters<P>::IsTrap() const {
  if (this->OpcodeFieldRaw() != SPECIAL) {
    return false;
  } else {
    switch (this->FunctionFieldRaw()) {
      case BREAK:
      case TGE:
      case TGEU:
      case TLT:
      case TLTU:
      case TEQ:
      case TNE:
        return true;
      default:
        return false;
    }
  }
}

// static
template <class T>
bool InstructionGetters<T>::IsForbiddenAfterBranchInstr(Instr instr) {
  Opcode opcode = static_cast<Opcode>(instr & kOpcodeMask);
  switch (opcode) {
    case J:
    case JAL:
    case BEQ:
    case BNE:
    case BLEZ:  // POP06 bgeuc/bleuc, blezalc, bgezalc
    case BGTZ:  // POP07 bltuc/bgtuc, bgtzalc, bltzalc
    case BEQL:
    case BNEL:
    case BLEZL:  // POP26 bgezc, blezc, bgec/blec
    case BGTZL:  // POP27 bgtzc, bltzc, bltc/bgtc
    case BC:
    case BALC:
    case POP10:  // beqzalc, bovc, beqc
    case POP30:  // bnezalc, bnvc, bnec
    case POP66:  // beqzc, jic
    case POP76:  // bnezc, jialc
      return true;
    case REGIMM:
      switch (instr & kRtFieldMask) {
        case BLTZ:
        case BGEZ:
        case BLTZAL:
        case BGEZAL:
          return true;
        default:
          return false;
      }
      break;
    case SPECIAL:
      switch (instr & kFunctionFieldMask) {
        case JR:
        case JALR:
          return true;
        default:
          return false;
      }
      break;
    case COP1:
      switch (instr & kRsFieldMask) {
        case BC1:
        case BC1EQZ:
        case BC1NEZ:
          return true;
          break;
        default:
          return false;
      }
      break;
    default:
      return false;
  }
}
}  // namespace internal
}  // namespace v8

#endif    // #ifndef V8_MIPS_CONSTANTS_H_

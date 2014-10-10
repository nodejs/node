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
  static const FpuMode kFpuMode = kFPXX;
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

#ifndef FPU_MODE_FPXX
#define IsFp64Mode() \
  (kFpuMode == kFP64)
#else
#define IsFp64Mode() \
  (CpuFeatures::IsSupported(FP64FPU))
#endif

#ifndef _MIPS_ARCH_MIPS32RX
#define IsMipsArchVariant(check) \
  (kArchVariant == check)
#else
#define IsMipsArchVariant(check) \
  (CpuFeatures::IsSupported(check))
#endif


#define __STDC_FORMAT_MACROS
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

// FPU (coprocessor 1) control registers. Currently only FCSR is implemented.
const int kFCSRRegister = 31;
const int kInvalidFPUControlRegister = -1;
const uint32_t kFPUInvalidResult = static_cast<uint32_t>(1 << 31) - 1;
const uint64_t kFPU64InvalidResult =
    static_cast<uint64_t>(static_cast<uint64_t>(1) << 63) - 1;

// FCSR constants.
const uint32_t kFCSRInexactFlagBit = 2;
const uint32_t kFCSRUnderflowFlagBit = 3;
const uint32_t kFCSROverflowFlagBit = 4;
const uint32_t kFCSRDivideByZeroFlagBit = 5;
const uint32_t kFCSRInvalidOpFlagBit = 6;

const uint32_t kFCSRInexactFlagMask = 1 << kFCSRInexactFlagBit;
const uint32_t kFCSRUnderflowFlagMask = 1 << kFCSRUnderflowFlagBit;
const uint32_t kFCSROverflowFlagMask = 1 << kFCSROverflowFlagBit;
const uint32_t kFCSRDivideByZeroFlagMask = 1 << kFCSRDivideByZeroFlagBit;
const uint32_t kFCSRInvalidOpFlagMask = 1 << kFCSRInvalidOpFlagBit;

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
const int kFunctionShift = 0;
const int kFunctionBits  = 6;
const int kLuiShift      = 16;

const int kImm16Shift = 0;
const int kImm16Bits  = 16;
const int kImm21Shift = 0;
const int kImm21Bits  = 21;
const int kImm26Shift = 0;
const int kImm26Bits  = 26;
const int kImm28Shift = 0;
const int kImm28Bits  = 28;
const int kImm32Shift = 0;
const int kImm32Bits  = 32;

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

// ----- Miscellaneous useful masks.
// Instruction bit masks.
const int  kOpcodeMask   = ((1 << kOpcodeBits) - 1) << kOpcodeShift;
const int  kImm16Mask    = ((1 << kImm16Bits) - 1) << kImm16Shift;
const int  kImm26Mask    = ((1 << kImm26Bits) - 1) << kImm26Shift;
const int  kImm28Mask    = ((1 << kImm28Bits) - 1) << kImm28Shift;
const int  kRsFieldMask  = ((1 << kRsBits) - 1) << kRsShift;
const int  kRtFieldMask  = ((1 << kRtBits) - 1) << kRtShift;
const int  kRdFieldMask  = ((1 << kRdBits) - 1) << kRdShift;
const int  kSaFieldMask  = ((1 << kSaBits) - 1) << kSaShift;
const int  kFunctionFieldMask = ((1 << kFunctionBits) - 1) << kFunctionShift;
// Misc masks.
const int  kHiMask       =   0xffff << 16;
const int  kLoMask       =   0xffff;
const int  kSignMask     =   0x80000000;
const int  kJumpAddrMask = (1 << (kImm26Bits + kImmFieldShift)) - 1;

// ----- MIPS Opcodes and Function Fields.
// We use this presentation to stay close to the table representation in
// MIPS32 Architecture For Programmers, Volume II: The MIPS32 Instruction Set.
enum Opcode {
  SPECIAL   =   0 << kOpcodeShift,
  REGIMM    =   1 << kOpcodeShift,

  J         =   ((0 << 3) + 2) << kOpcodeShift,
  JAL       =   ((0 << 3) + 3) << kOpcodeShift,
  BEQ       =   ((0 << 3) + 4) << kOpcodeShift,
  BNE       =   ((0 << 3) + 5) << kOpcodeShift,
  BLEZ      =   ((0 << 3) + 6) << kOpcodeShift,
  BGTZ      =   ((0 << 3) + 7) << kOpcodeShift,

  ADDI      =   ((1 << 3) + 0) << kOpcodeShift,
  ADDIU     =   ((1 << 3) + 1) << kOpcodeShift,
  SLTI      =   ((1 << 3) + 2) << kOpcodeShift,
  SLTIU     =   ((1 << 3) + 3) << kOpcodeShift,
  ANDI      =   ((1 << 3) + 4) << kOpcodeShift,
  ORI       =   ((1 << 3) + 5) << kOpcodeShift,
  XORI      =   ((1 << 3) + 6) << kOpcodeShift,
  LUI       =   ((1 << 3) + 7) << kOpcodeShift,  // LUI/AUI family.

  BEQC      =   ((2 << 3) + 0) << kOpcodeShift,
  COP1      =   ((2 << 3) + 1) << kOpcodeShift,  // Coprocessor 1 class.
  BEQL      =   ((2 << 3) + 4) << kOpcodeShift,
  BNEL      =   ((2 << 3) + 5) << kOpcodeShift,
  BLEZL     =   ((2 << 3) + 6) << kOpcodeShift,
  BGTZL     =   ((2 << 3) + 7) << kOpcodeShift,

  DADDI     =   ((3 << 3) + 0) << kOpcodeShift,  // This is also BNEC.
  SPECIAL2  =   ((3 << 3) + 4) << kOpcodeShift,
  SPECIAL3  =   ((3 << 3) + 7) << kOpcodeShift,

  LB        =   ((4 << 3) + 0) << kOpcodeShift,
  LH        =   ((4 << 3) + 1) << kOpcodeShift,
  LWL       =   ((4 << 3) + 2) << kOpcodeShift,
  LW        =   ((4 << 3) + 3) << kOpcodeShift,
  LBU       =   ((4 << 3) + 4) << kOpcodeShift,
  LHU       =   ((4 << 3) + 5) << kOpcodeShift,
  LWR       =   ((4 << 3) + 6) << kOpcodeShift,
  SB        =   ((5 << 3) + 0) << kOpcodeShift,
  SH        =   ((5 << 3) + 1) << kOpcodeShift,
  SWL       =   ((5 << 3) + 2) << kOpcodeShift,
  SW        =   ((5 << 3) + 3) << kOpcodeShift,
  SWR       =   ((5 << 3) + 6) << kOpcodeShift,

  LWC1      =   ((6 << 3) + 1) << kOpcodeShift,
  LDC1      =   ((6 << 3) + 5) << kOpcodeShift,
  BEQZC     =   ((6 << 3) + 6) << kOpcodeShift,

  PREF      =   ((6 << 3) + 3) << kOpcodeShift,

  SWC1      =   ((7 << 3) + 1) << kOpcodeShift,
  SDC1      =   ((7 << 3) + 5) << kOpcodeShift,
  BNEZC     =   ((7 << 3) + 6) << kOpcodeShift,

  COP1X     =   ((1 << 4) + 3) << kOpcodeShift
};

enum SecondaryField {
  // SPECIAL Encoding of Function Field.
  SLL       =   ((0 << 3) + 0),
  MOVCI     =   ((0 << 3) + 1),
  SRL       =   ((0 << 3) + 2),
  SRA       =   ((0 << 3) + 3),
  SLLV      =   ((0 << 3) + 4),
  SRLV      =   ((0 << 3) + 6),
  SRAV      =   ((0 << 3) + 7),

  JR        =   ((1 << 3) + 0),
  JALR      =   ((1 << 3) + 1),
  MOVZ      =   ((1 << 3) + 2),
  MOVN      =   ((1 << 3) + 3),
  BREAK     =   ((1 << 3) + 5),

  MFHI      =   ((2 << 3) + 0),
  CLZ_R6    =   ((2 << 3) + 0),
  CLO_R6    =   ((2 << 3) + 1),
  MFLO      =   ((2 << 3) + 2),

  MULT      =   ((3 << 3) + 0),
  MULTU     =   ((3 << 3) + 1),
  DIV       =   ((3 << 3) + 2),
  DIVU      =   ((3 << 3) + 3),

  ADD       =   ((4 << 3) + 0),
  ADDU      =   ((4 << 3) + 1),
  SUB       =   ((4 << 3) + 2),
  SUBU      =   ((4 << 3) + 3),
  AND       =   ((4 << 3) + 4),
  OR        =   ((4 << 3) + 5),
  XOR       =   ((4 << 3) + 6),
  NOR       =   ((4 << 3) + 7),

  SLT       =   ((5 << 3) + 2),
  SLTU      =   ((5 << 3) + 3),

  TGE       =   ((6 << 3) + 0),
  TGEU      =   ((6 << 3) + 1),
  TLT       =   ((6 << 3) + 2),
  TLTU      =   ((6 << 3) + 3),
  TEQ       =   ((6 << 3) + 4),
  SELEQZ_S  =   ((6 << 3) + 5),
  TNE       =   ((6 << 3) + 6),
  SELNEZ_S  =   ((6 << 3) + 7),

  // Multiply integers in r6.
  MUL_MUH   =   ((3 << 3) + 0),  // MUL, MUH.
  MUL_MUH_U =   ((3 << 3) + 1),  // MUL_U, MUH_U.

  MUL_OP    =   ((0 << 3) + 2),
  MUH_OP    =   ((0 << 3) + 3),
  DIV_OP    =   ((0 << 3) + 2),
  MOD_OP    =   ((0 << 3) + 3),

  DIV_MOD   =   ((3 << 3) + 2),
  DIV_MOD_U =   ((3 << 3) + 3),

  // SPECIAL2 Encoding of Function Field.
  MUL       =   ((0 << 3) + 2),
  CLZ       =   ((4 << 3) + 0),
  CLO       =   ((4 << 3) + 1),

  // SPECIAL3 Encoding of Function Field.
  EXT       =   ((0 << 3) + 0),
  INS       =   ((0 << 3) + 4),

  // REGIMM  encoding of rt Field.
  BLTZ      =   ((0 << 3) + 0) << 16,
  BGEZ      =   ((0 << 3) + 1) << 16,
  BLTZAL    =   ((2 << 3) + 0) << 16,
  BGEZAL    =   ((2 << 3) + 1) << 16,
  BGEZALL   =   ((2 << 3) + 3) << 16,

  // COP1 Encoding of rs Field.
  MFC1      =   ((0 << 3) + 0) << 21,
  CFC1      =   ((0 << 3) + 2) << 21,
  MFHC1     =   ((0 << 3) + 3) << 21,
  MTC1      =   ((0 << 3) + 4) << 21,
  CTC1      =   ((0 << 3) + 6) << 21,
  MTHC1     =   ((0 << 3) + 7) << 21,
  BC1       =   ((1 << 3) + 0) << 21,
  S         =   ((2 << 3) + 0) << 21,
  D         =   ((2 << 3) + 1) << 21,
  W         =   ((2 << 3) + 4) << 21,
  L         =   ((2 << 3) + 5) << 21,
  PS        =   ((2 << 3) + 6) << 21,
  // COP1 Encoding of Function Field When rs=S.
  ROUND_L_S =   ((1 << 3) + 0),
  TRUNC_L_S =   ((1 << 3) + 1),
  CEIL_L_S  =   ((1 << 3) + 2),
  FLOOR_L_S =   ((1 << 3) + 3),
  ROUND_W_S =   ((1 << 3) + 4),
  TRUNC_W_S =   ((1 << 3) + 5),
  CEIL_W_S  =   ((1 << 3) + 6),
  FLOOR_W_S =   ((1 << 3) + 7),
  CVT_D_S   =   ((4 << 3) + 1),
  CVT_W_S   =   ((4 << 3) + 4),
  CVT_L_S   =   ((4 << 3) + 5),
  CVT_PS_S  =   ((4 << 3) + 6),
  // COP1 Encoding of Function Field When rs=D.
  ADD_D     =   ((0 << 3) + 0),
  SUB_D     =   ((0 << 3) + 1),
  MUL_D     =   ((0 << 3) + 2),
  DIV_D     =   ((0 << 3) + 3),
  SQRT_D    =   ((0 << 3) + 4),
  ABS_D     =   ((0 << 3) + 5),
  MOV_D     =   ((0 << 3) + 6),
  NEG_D     =   ((0 << 3) + 7),
  ROUND_L_D =   ((1 << 3) + 0),
  TRUNC_L_D =   ((1 << 3) + 1),
  CEIL_L_D  =   ((1 << 3) + 2),
  FLOOR_L_D =   ((1 << 3) + 3),
  ROUND_W_D =   ((1 << 3) + 4),
  TRUNC_W_D =   ((1 << 3) + 5),
  CEIL_W_D  =   ((1 << 3) + 6),
  FLOOR_W_D =   ((1 << 3) + 7),
  MIN       =   ((3 << 3) + 4),
  MINA      =   ((3 << 3) + 5),
  MAX       =   ((3 << 3) + 6),
  MAXA      =   ((3 << 3) + 7),
  CVT_S_D   =   ((4 << 3) + 0),
  CVT_W_D   =   ((4 << 3) + 4),
  CVT_L_D   =   ((4 << 3) + 5),
  C_F_D     =   ((6 << 3) + 0),
  C_UN_D    =   ((6 << 3) + 1),
  C_EQ_D    =   ((6 << 3) + 2),
  C_UEQ_D   =   ((6 << 3) + 3),
  C_OLT_D   =   ((6 << 3) + 4),
  C_ULT_D   =   ((6 << 3) + 5),
  C_OLE_D   =   ((6 << 3) + 6),
  C_ULE_D   =   ((6 << 3) + 7),
  // COP1 Encoding of Function Field When rs=W or L.
  CVT_S_W   =   ((4 << 3) + 0),
  CVT_D_W   =   ((4 << 3) + 1),
  CVT_S_L   =   ((4 << 3) + 0),
  CVT_D_L   =   ((4 << 3) + 1),
  BC1EQZ    =   ((2 << 2) + 1) << 21,
  BC1NEZ    =   ((3 << 2) + 1) << 21,
  // COP1 CMP positive predicates Bit 5..4 = 00.
  CMP_AF    =   ((0 << 3) + 0),
  CMP_UN    =   ((0 << 3) + 1),
  CMP_EQ    =   ((0 << 3) + 2),
  CMP_UEQ   =   ((0 << 3) + 3),
  CMP_LT    =   ((0 << 3) + 4),
  CMP_ULT   =   ((0 << 3) + 5),
  CMP_LE    =   ((0 << 3) + 6),
  CMP_ULE   =   ((0 << 3) + 7),
  CMP_SAF   =   ((1 << 3) + 0),
  CMP_SUN   =   ((1 << 3) + 1),
  CMP_SEQ   =   ((1 << 3) + 2),
  CMP_SUEQ  =   ((1 << 3) + 3),
  CMP_SSLT  =   ((1 << 3) + 4),
  CMP_SSULT =   ((1 << 3) + 5),
  CMP_SLE   =   ((1 << 3) + 6),
  CMP_SULE  =   ((1 << 3) + 7),
  // COP1 CMP negative predicates Bit 5..4 = 01.
  CMP_AT    =   ((2 << 3) + 0),  // Reserved, not implemented.
  CMP_OR    =   ((2 << 3) + 1),
  CMP_UNE   =   ((2 << 3) + 2),
  CMP_NE    =   ((2 << 3) + 3),
  CMP_UGE   =   ((2 << 3) + 4),  // Reserved, not implemented.
  CMP_OGE   =   ((2 << 3) + 5),  // Reserved, not implemented.
  CMP_UGT   =   ((2 << 3) + 6),  // Reserved, not implemented.
  CMP_OGT   =   ((2 << 3) + 7),  // Reserved, not implemented.
  CMP_SAT   =   ((3 << 3) + 0),  // Reserved, not implemented.
  CMP_SOR   =   ((3 << 3) + 1),
  CMP_SUNE  =   ((3 << 3) + 2),
  CMP_SNE   =   ((3 << 3) + 3),
  CMP_SUGE  =   ((3 << 3) + 4),  // Reserved, not implemented.
  CMP_SOGE  =   ((3 << 3) + 5),  // Reserved, not implemented.
  CMP_SUGT  =   ((3 << 3) + 6),  // Reserved, not implemented.
  CMP_SOGT  =   ((3 << 3) + 7),  // Reserved, not implemented.

  SEL       =   ((2 << 3) + 0),
  SELEQZ_C  =   ((2 << 3) + 4),  // COP1 on FPR registers.
  SELNEZ_C  =   ((2 << 3) + 7),  // COP1 on FPR registers.
  // COP1 Encoding of Function Field When rs=PS.
  // COP1X Encoding of Function Field.
  MADD_D    =   ((4 << 3) + 1),

  NULLSF    =   0
};


// ----- Emulated conditions.
// On MIPS we use this enum to abstract from conditionnal branch instructions.
// The 'U' prefix is used to specify unsigned comparisons.
// Oppposite conditions must be paired as odd/even numbers
// because 'NegateCondition' function flips LSB to negate condition.
enum Condition {
  // Any value < 0 is considered no_condition.
  kNoCondition  = -1,

  overflow      =  0,
  no_overflow   =  1,
  Uless         =  2,
  Ugreater_equal=  3,
  equal         =  4,
  not_equal     =  5,
  Uless_equal   =  6,
  Ugreater      =  7,
  negative      =  8,
  positive      =  9,
  parity_even   = 10,
  parity_odd    = 11,
  less          = 12,
  greater_equal = 13,
  less_equal    = 14,
  greater       = 15,
  ueq           = 16,  // Unordered or Equal.
  nue           = 17,  // Not (Unordered or Equal).

  cc_always     = 18,

  // Aliases.
  carry         = Uless,
  not_carry     = Ugreater_equal,
  zero          = equal,
  eq            = equal,
  not_zero      = not_equal,
  ne            = not_equal,
  nz            = not_equal,
  sign          = negative,
  not_sign      = positive,
  mi            = negative,
  pl            = positive,
  hi            = Ugreater,
  ls            = Uless_equal,
  ge            = greater_equal,
  lt            = less,
  gt            = greater,
  le            = less_equal,
  hs            = Ugreater_equal,
  lo            = Uless,
  al            = cc_always,

  cc_default    = kNoCondition
};


// Returns the equivalent of !cc.
// Negation of the default kNoCondition (-1) results in a non-default
// no_condition value (-2). As long as tests for no_condition check
// for condition < 0, this will work as expected.
inline Condition NegateCondition(Condition cc) {
  DCHECK(cc != cc_always);
  return static_cast<Condition>(cc ^ 1);
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

  F     = 0,  // False.
  UN    = 1,  // Unordered.
  EQ    = 2,  // Equal.
  UEQ   = 3,  // Unordered or Equal.
  OLT   = 4,  // Ordered or Less Than.
  ULT   = 5,  // Unordered or Less Than.
  OLE   = 6,  // Ordered or Less Than or Equal.
  ULE   = 7   // Unordered or Less Than or Equal.
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
  kRoundToMinusInf = RM
};

const uint32_t kFPURoundingModeMask = 3 << 0;

enum CheckForInexactConversion {
  kCheckForInexactConversion,
  kDontCheckForInexactConversion
};


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

class Instruction {
 public:
  enum {
    kInstrSize = 4,
    kInstrSizeLog2 = 2,
    // On MIPS PC cannot actually be directly accessed. We behave as if PC was
    // always the value of the current instruction being executed.
    kPCReadOffset = 0
  };

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
    return (InstructionBits() >> lo) & ((2 << (hi - lo)) - 1);
  }

  // Instruction type.
  enum Type {
    kRegisterType,
    kImmediateType,
    kJumpType,
    kUnsupported = -1
  };

  // Get the encoding type of the instruction.
  Type InstructionType() const;


  // Accessors for the different named fields used in the MIPS encoding.
  inline Opcode OpcodeValue() const {
    return static_cast<Opcode>(
        Bits(kOpcodeShift + kOpcodeBits - 1, kOpcodeShift));
  }

  inline int RsValue() const {
    DCHECK(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return Bits(kRsShift + kRsBits - 1, kRsShift);
  }

  inline int RtValue() const {
    DCHECK(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return Bits(kRtShift + kRtBits - 1, kRtShift);
  }

  inline int RdValue() const {
    DCHECK(InstructionType() == kRegisterType);
    return Bits(kRdShift + kRdBits - 1, kRdShift);
  }

  inline int SaValue() const {
    DCHECK(InstructionType() == kRegisterType);
    return Bits(kSaShift + kSaBits - 1, kSaShift);
  }

  inline int FunctionValue() const {
    DCHECK(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return Bits(kFunctionShift + kFunctionBits - 1, kFunctionShift);
  }

  inline int FdValue() const {
    return Bits(kFdShift + kFdBits - 1, kFdShift);
  }

  inline int FsValue() const {
    return Bits(kFsShift + kFsBits - 1, kFsShift);
  }

  inline int FtValue() const {
    return Bits(kFtShift + kFtBits - 1, kFtShift);
  }

  inline int FrValue() const {
    return Bits(kFrShift + kFrBits -1, kFrShift);
  }

  // Float Compare condition code instruction bits.
  inline int FCccValue() const {
    return Bits(kFCccShift + kFCccBits - 1, kFCccShift);
  }

  // Float Branch condition code instruction bits.
  inline int FBccValue() const {
    return Bits(kFBccShift + kFBccBits - 1, kFBccShift);
  }

  // Float Branch true/false instruction bit.
  inline int FBtrueValue() const {
    return Bits(kFBtrueShift + kFBtrueBits - 1, kFBtrueShift);
  }

  // Return the fields at their original place in the instruction encoding.
  inline Opcode OpcodeFieldRaw() const {
    return static_cast<Opcode>(InstructionBits() & kOpcodeMask);
  }

  inline int RsFieldRaw() const {
    DCHECK(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return InstructionBits() & kRsFieldMask;
  }

  // Same as above function, but safe to call within InstructionType().
  inline int RsFieldRawNoAssert() const {
    return InstructionBits() & kRsFieldMask;
  }

  inline int RtFieldRaw() const {
    DCHECK(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return InstructionBits() & kRtFieldMask;
  }

  inline int RdFieldRaw() const {
    DCHECK(InstructionType() == kRegisterType);
    return InstructionBits() & kRdFieldMask;
  }

  inline int SaFieldRaw() const {
    DCHECK(InstructionType() == kRegisterType);
    return InstructionBits() & kSaFieldMask;
  }

  inline int FunctionFieldRaw() const {
    return InstructionBits() & kFunctionFieldMask;
  }

  // Get the secondary field according to the opcode.
  inline int SecondaryValue() const {
    Opcode op = OpcodeFieldRaw();
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

  inline int32_t Imm16Value() const {
    DCHECK(InstructionType() == kImmediateType);
    return Bits(kImm16Shift + kImm16Bits - 1, kImm16Shift);
  }

  inline int32_t Imm21Value() const {
    DCHECK(InstructionType() == kImmediateType);
    return Bits(kImm21Shift + kImm21Bits - 1, kImm21Shift);
  }

  inline int32_t Imm26Value() const {
    DCHECK(InstructionType() == kJumpType);
    return Bits(kImm26Shift + kImm26Bits - 1, kImm26Shift);
  }

  // Say if the instruction should not be used in a branch delay slot.
  bool IsForbiddenInBranchDelay() const;
  // Say if the instruction 'links'. e.g. jal, bal.
  bool IsLinkingInstruction() const;
  // Say if the instruction is a break or a trap.
  bool IsTrap() const;

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
// JS argument slots size.
const int kJSArgsSlotsSize = 0 * Instruction::kInstrSize;
// Assembly builtins argument slots size.
const int kBArgsSlotsSize = 0 * Instruction::kInstrSize;

const int kBranchReturnOffset = 2 * Instruction::kInstrSize;

} }   // namespace v8::internal

#endif    // #ifndef V8_MIPS_CONSTANTS_H_

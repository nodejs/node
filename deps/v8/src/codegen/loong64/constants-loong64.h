// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_LOONG64_CONSTANTS_LOONG64_H_
#define V8_CODEGEN_LOONG64_CONSTANTS_LOONG64_H_

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/common/code-memory-access.h"
#include "src/common/globals.h"

// Get the standard printf format macros for C99 stdint types.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

// Defines constants and accessor classes to assemble, disassemble and
// simulate LOONG64 instructions.

namespace v8 {
namespace internal {

constexpr size_t kMaxPCRelativeCodeRangeInMB = 128;

// -----------------------------------------------------------------------------
// Registers and FPURegisters.

// Number of general purpose registers.
const int kNumRegisters = 32;
const int kInvalidRegister = -1;

// Number of registers with pc.
const int kNumSimuRegisters = 33;

// In the simulator, the PC register is simulated as the 33th register.
const int kPCRegister = 32;

// Number of floating point registers.
const int kNumFPURegisters = 32;
const int kInvalidFPURegister = -1;

// Number of LSX registers
const int kNumVRegisters = 32;
const int kInvalidVRegister = -1;

const int kInvalidLSXControlRegister = -1;
const int kVRegSize = 128;
const int kLSXLanesByte = kVRegSize / 8;
const int kLSXLanesHalf = kVRegSize / 16;
const int kLSXLanesWord = kVRegSize / 32;
const int kLSXLanesDword = kVRegSize / 64;

// FPU control registers.
const int kFCSRRegister = 0;
const int kInvalidFPUControlRegister = -1;
const uint32_t kFPUInvalidResult = static_cast<uint32_t>(1u << 31) - 1;
const int32_t kFPUInvalidResultNegative = static_cast<int32_t>(1u << 31);
const uint64_t kFPU64InvalidResult =
    static_cast<uint64_t>(static_cast<uint64_t>(1) << 63) - 1;
const int64_t kFPU64InvalidResultNegative =
    static_cast<int64_t>(static_cast<uint64_t>(1) << 63);

// FCSR constants.
const uint32_t kFCSRInexactCauseBit = 24;
const uint32_t kFCSRUnderflowCauseBit = 25;
const uint32_t kFCSROverflowCauseBit = 26;
const uint32_t kFCSRDivideByZeroCauseBit = 27;
const uint32_t kFCSRInvalidOpCauseBit = 28;

const uint32_t kFCSRInexactCauseMask = 1 << kFCSRInexactCauseBit;
const uint32_t kFCSRUnderflowCauseMask = 1 << kFCSRUnderflowCauseBit;
const uint32_t kFCSROverflowCauseMask = 1 << kFCSROverflowCauseBit;
const uint32_t kFCSRDivideByZeroCauseMask = 1 << kFCSRDivideByZeroCauseBit;
const uint32_t kFCSRInvalidOpCauseMask = 1 << kFCSRInvalidOpCauseBit;

const uint32_t kFCSRCauseMask =
    kFCSRInexactCauseMask | kFCSRUnderflowCauseMask | kFCSROverflowCauseMask |
    kFCSRDivideByZeroCauseMask | kFCSRInvalidOpCauseMask;

const uint32_t kFCSRExceptionCauseMask = kFCSRCauseMask ^ kFCSRInexactCauseMask;

// Actual value of root register is offset from the root array's start
// to take advantage of negative displacement values.
constexpr int kRootRegisterBias = 256;

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

  static const int64_t kMaxValue = 0x7fffffffffffffffl;
  static const int64_t kMinValue = 0x8000000000000000l;

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
class VRegisters {
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
  static const char* names_[kNumVRegisters];
  static const RegisterAlias aliases_[];
};

enum LSXSize { LSX_B = 0x0, LSX_H = 0x1, LSX_W = 0x2, LSX_D = 0x3 };

enum LSXDataType {
  LSXS8 = 0,
  LSXS16 = 1,
  LSXS32 = 2,
  LSXS64 = 3,
  LSXU8 = 4,
  LSXU16 = 5,
  LSXU32 = 6,
  LSXU64 = 7
};

// -----------------------------------------------------------------------------
// Instructions encoding constants.

// On LoongArch all instructions are 32 bits.
using Instr = int32_t;

// Special Software Interrupt codes when used in the presence of the LOONG64
// simulator.
enum SoftwareInterruptCodes {
  // Transition to C code.
  call_rt_redirected = 0x7fff
};

// On LOONG64 Simulator breakpoints can have different codes:
// - Breaks between 0 and kMaxWatchpointCode are treated as simple watchpoints,
//   the simulator will run through them and print the registers.
// - Breaks between kMaxWatchpointCode and kMaxStopCode are treated as stop()
//   instructions (see Assembler::stop()).
// - Breaks larger than kMaxStopCode are simple breaks, dropping you into the
//   debugger.
const uint32_t kMaxWatchpointCode = 31;
const uint32_t kMaxStopCode = 127;
static_assert(kMaxWatchpointCode < kMaxStopCode);

// ----- Fields offset and length.
const int kRjShift = 5;
const int kRjBits = 5;
const int kRkShift = 10;
const int kRkBits = 5;
const int kRdShift = 0;
const int kRdBits = 5;
const int kSaShift = 15;
const int kSa2Bits = 2;
const int kSa3Bits = 3;
const int kCdShift = 0;
const int kCdBits = 3;
const int kCjShift = 5;
const int kCjBits = 3;
const int kCodeShift = 0;
const int kCodeBits = 15;
const int kCondShift = 15;
const int kCondBits = 5;
const int kUi1Shift = 10;
const int kUi1Bits = 1;
const int kUi2Shift = 10;
const int kUi2Bits = 2;
const int kUi3Shift = 10;
const int kUi3Bits = 3;
const int kUi4Shift = 10;
const int kUi4Bits = 4;
const int kUi5Shift = 10;
const int kUi5Bits = 5;
const int kUi6Shift = 10;
const int kUi6Bits = 6;
const int kUi7Shift = 10;
const int kUi7Bits = 7;
const int kUi8Shift = 10;
const int kUi8Bits = 8;
const int kUi12Shift = 10;
const int kUi12Bits = 12;
const int kSi5Shift = 10;
const int kSi5Bits = 5;
const int kSi8Shift = 10;
const int kSi8Bits = 8;
const int kSi9Shift = 10;
const int kSi9Bits = 9;
const int kSi10Shift = 10;
const int kSi10Bits = 10;
const int kSi11Shift = 10;
const int kSi11Bits = 11;
const int kSi12Shift = 10;
const int kSi12Bits = 12;
const int kSi13Shift = 5;
const int kSi13Bits = 13;
const int kSi14Shift = 10;
const int kSi14Bits = 14;
const int kSi16Shift = 10;
const int kSi16Bits = 16;
const int kSi20Shift = 5;
const int kSi20Bits = 20;
const int kMsbwShift = 16;
const int kMsbwBits = 5;
const int kLsbwShift = 10;
const int kLsbwBits = 5;
const int kMsbdShift = 16;
const int kMsbdBits = 6;
const int kLsbdShift = 10;
const int kLsbdBits = 6;
const int kFdShift = 0;
const int kFdBits = 5;
const int kFjShift = 5;
const int kFjBits = 5;
const int kFkShift = 10;
const int kFkBits = 5;
const int kFaShift = 15;
const int kFaBits = 5;
const int kCaShift = 15;
const int kCaBits = 3;
const int kVdShift = 0;
const int kVdBits = 5;
const int kVjShift = 5;
const int kVjBits = 5;
const int kVkShift = 10;
const int kVkBits = 5;
const int kVaShift = 15;
const int kVaBits = 5;
const int kHint15Shift = 0;
const int kHint15Bits = 15;
const int kHint5Shift = 0;
const int kHint5Bits = 5;
const int kOffsLowShift = 10;
const int kOffsLowBits = 16;
const int kOffs26HighShift = 0;
const int kOffs26HighBits = 10;
const int kOffs21HighShift = 0;
const int kOffs21HighBits = 5;
const int kImm1Shift = 0;
const int kImm1Bits = 1;
const int kImm2Shift = 0;
const int kImm2Bits = 2;
const int kImm3Shift = 0;
const int kImm3Bits = 3;
const int kImm4Shift = 0;
const int kImm4Bits = 4;
const int kImm5Shift = 0;
const int kImm5Bits = 5;
const int kImm6Shift = 0;
const int kImm6Bits = 6;
const int kImm7Shift = 0;
const int kImm7Bits = 7;
const int kImm8Shift = 0;
const int kImm8Bits = 8;
const int kImm9Shift = 0;
const int kImm9Bits = 9;
const int kImm10Shift = 0;
const int kImm10Bits = 10;
const int kImm11Shift = 0;
const int kImm11Bits = 11;
const int kImm12Shift = 0;
const int kImm12Bits = 12;
const int kImm13Shift = 0;
const int kImm13Bits = 13;
const int kImm16Shift = 0;
const int kImm16Bits = 16;
const int kImm26Shift = 0;
const int kImm26Bits = 26;
const int kImm28Shift = 0;
const int kImm28Bits = 28;
const int kImm32Shift = 0;
const int kImm32Bits = 32;
const int kIdxShift = 18;
const int kIdx1Bits = 1;
const int kIdx2Bits = 2;
const int kIdx3Bits = 3;
const int kIdx4Bits = 4;

// ----- Miscellaneous useful masks.
// Instruction bit masks.
const int kRjFieldMask = ((1 << kRjBits) - 1) << kRjShift;
const int kRkFieldMask = ((1 << kRkBits) - 1) << kRkShift;
const int kRdFieldMask = ((1 << kRdBits) - 1) << kRdShift;
const int kSa2FieldMask = ((1 << kSa2Bits) - 1) << kSaShift;
const int kSa3FieldMask = ((1 << kSa3Bits) - 1) << kSaShift;
// Misc masks.
const int kHiMaskOf32 = 0xffff << 16;  // Only to be used with 32-bit values
const int kLoMaskOf32 = 0xffff;
const int kSignMaskOf32 = 0x80000000;  // Only to be used with 32-bit values
const int64_t kTop16MaskOf64 = (int64_t)0xffff << 48;
const int64_t kHigher16MaskOf64 = (int64_t)0xffff << 32;
const int64_t kUpper16MaskOf64 = (int64_t)0xffff << 16;

const int kImm1Mask = ((1 << kImm1Bits) - 1) << kImm1Shift;
const int kImm2Mask = ((1 << kImm2Bits) - 1) << kImm2Shift;
const int kImm3Mask = ((1 << kImm3Bits) - 1) << kImm3Shift;
const int kImm4Mask = ((1 << kImm4Bits) - 1) << kImm4Shift;
const int kImm5Mask = ((1 << kImm5Bits) - 1) << kImm5Shift;
const int kImm6Mask = ((1 << kImm6Bits) - 1) << kImm6Shift;
const int kImm7Mask = ((1 << kImm7Bits) - 1) << kImm7Shift;
const int kImm8Mask = ((1 << kImm8Bits) - 1) << kImm8Shift;
const int kImm9Mask = ((1 << kImm9Bits) - 1) << kImm9Shift;
const int kImm10Mask = ((1 << kImm10Bits) - 1) << kImm10Shift;
const int kImm11Mask = ((1 << kImm11Bits) - 1) << kImm11Shift;
const int kImm12Mask = ((1 << kImm12Bits) - 1) << kImm12Shift;
const int kImm13Mask = ((1 << kImm13Bits) - 1) << kImm13Shift;
const int kImm16Mask = ((1 << kImm16Bits) - 1) << kImm16Shift;
const int kImm26Mask = ((1 << kImm26Bits) - 1) << kImm26Shift;
const int kImm28Mask = ((1 << kImm28Bits) - 1) << kImm28Shift;

// ----- LOONG64 Opcodes and Function Fields.
enum Opcode : uint32_t {
  BEQZ = 0x10U << 26,
  BNEZ = 0x11U << 26,
  BCZ = 0x12U << 26,  // BCEQZ & BCNEZ
  JIRL = 0x13U << 26,
  B = 0x14U << 26,
  BL = 0x15U << 26,
  BEQ = 0x16U << 26,
  BNE = 0x17U << 26,
  BLT = 0x18U << 26,
  BGE = 0x19U << 26,
  BLTU = 0x1aU << 26,
  BGEU = 0x1bU << 26,

  ADDU16I_D = 0x4U << 26,

  LU12I_W = 0xaU << 25,
  LU32I_D = 0xbU << 25,
  PCADDI = 0xcU << 25,
  PCALAU12I = 0xdU << 25,
  PCADDU12I = 0xeU << 25,
  PCADDU18I = 0xfU << 25,

  LL_W = 0x20U << 24,
  SC_W = 0x21U << 24,
  LL_D = 0x22U << 24,
  SC_D = 0x23U << 24,
  LDPTR_W = 0x24U << 24,
  STPTR_W = 0x25U << 24,
  LDPTR_D = 0x26U << 24,
  STPTR_D = 0x27U << 24,

  BSTR_W = 0x1U << 22,  // BSTRINS_W & BSTRPICK_W
  BSTRINS_W = BSTR_W,
  BSTRPICK_W = BSTR_W,
  BSTRINS_D = 0x2U << 22,
  BSTRPICK_D = 0x3U << 22,

  SLTI = 0x8U << 22,
  SLTUI = 0x9U << 22,
  ADDI_W = 0xaU << 22,
  ADDI_D = 0xbU << 22,
  LU52I_D = 0xcU << 22,
  ANDI = 0xdU << 22,
  ORI = 0xeU << 22,
  XORI = 0xfU << 22,

  LD_B = 0xa0U << 22,
  LD_H = 0xa1U << 22,
  LD_W = 0xa2U << 22,
  LD_D = 0xa3U << 22,
  ST_B = 0xa4U << 22,
  ST_H = 0xa5U << 22,
  ST_W = 0xa6U << 22,
  ST_D = 0xa7U << 22,
  LD_BU = 0xa8U << 22,
  LD_HU = 0xa9U << 22,
  LD_WU = 0xaaU << 22,
  FLD_S = 0xacU << 22,
  FST_S = 0xadU << 22,
  FLD_D = 0xaeU << 22,
  FST_D = 0xafU << 22,

  FMADD_S = 0x81U << 20,
  FMADD_D = 0x82U << 20,
  FMSUB_S = 0x85U << 20,
  FMSUB_D = 0x86U << 20,
  FNMADD_S = 0x89U << 20,
  FNMADD_D = 0x8aU << 20,
  FNMSUB_S = 0x8dU << 20,
  FNMSUB_D = 0x8eU << 20,
  FCMP_COND_S = 0xc1U << 20,
  FCMP_COND_D = 0xc2U << 20,

  BYTEPICK_D = 0x3U << 18,
  BYTEPICK_W = 0x2U << 18,

  FSEL = 0x340U << 18,

  ALSL = 0x1U << 18,
  ALSL_W = ALSL,
  ALSL_WU = ALSL,

  ALSL_D = 0xbU << 18,

  SLLI_W = 0x40U << 16,
  SRLI_W = 0x44U << 16,
  SRAI_W = 0x48U << 16,
  ROTRI_W = 0x4cU << 16,

  SLLI_D = 0x41U << 16,
  SRLI_D = 0x45U << 16,
  SRAI_D = 0x49U << 16,
  ROTRI_D = 0x4dU << 16,

  SLLI = 0x10U << 18,
  SRLI = 0x11U << 18,
  SRAI = 0x12U << 18,
  ROTRI = 0x13U << 18,

  ADD_W = 0x20U << 15,
  ADD_D = 0x21U << 15,
  SUB_W = 0x22U << 15,
  SUB_D = 0x23U << 15,
  SLT = 0x24U << 15,
  SLTU = 0x25U << 15,
  MASKEQZ = 0x26U << 15,
  MASKNEZ = 0x27U << 15,
  NOR = 0x28U << 15,
  AND = 0x29U << 15,
  OR = 0x2aU << 15,
  XOR = 0x2bU << 15,
  ORN = 0x2cU << 15,
  ANDN = 0x2dU << 15,
  SLL_W = 0x2eU << 15,
  SRL_W = 0x2fU << 15,
  SRA_W = 0x30U << 15,
  SLL_D = 0x31U << 15,
  SRL_D = 0x32U << 15,
  SRA_D = 0x33U << 15,
  ROTR_W = 0x36U << 15,
  ROTR_D = 0x37U << 15,
  MUL_W = 0x38U << 15,
  MULH_W = 0x39U << 15,
  MULH_WU = 0x3aU << 15,
  MUL_D = 0x3bU << 15,
  MULH_D = 0x3cU << 15,
  MULH_DU = 0x3dU << 15,
  MULW_D_W = 0x3eU << 15,
  MULW_D_WU = 0x3fU << 15,

  DIV_W = 0x40U << 15,
  MOD_W = 0x41U << 15,
  DIV_WU = 0x42U << 15,
  MOD_WU = 0x43U << 15,
  DIV_D = 0x44U << 15,
  MOD_D = 0x45U << 15,
  DIV_DU = 0x46U << 15,
  MOD_DU = 0x47U << 15,

  BREAK = 0x54U << 15,

  FADD_S = 0x201U << 15,
  FADD_D = 0x202U << 15,
  FSUB_S = 0x205U << 15,
  FSUB_D = 0x206U << 15,
  FMUL_S = 0x209U << 15,
  FMUL_D = 0x20aU << 15,
  FDIV_S = 0x20dU << 15,
  FDIV_D = 0x20eU << 15,
  FMAX_S = 0x211U << 15,
  FMAX_D = 0x212U << 15,
  FMIN_S = 0x215U << 15,
  FMIN_D = 0x216U << 15,
  FMAXA_S = 0x219U << 15,
  FMAXA_D = 0x21aU << 15,
  FMINA_S = 0x21dU << 15,
  FMINA_D = 0x21eU << 15,
  FSCALEB_S = 0x221U << 15,
  FSCALEB_D = 0x222U << 15,
  FCOPYSIGN_S = 0x225U << 15,
  FCOPYSIGN_D = 0x226U << 15,

  LDX_B = 0x7000U << 15,
  LDX_H = 0x7008U << 15,
  LDX_W = 0x7010U << 15,
  LDX_D = 0x7018U << 15,
  STX_B = 0x7020U << 15,
  STX_H = 0x7028U << 15,
  STX_W = 0x7030U << 15,
  STX_D = 0x7038U << 15,
  LDX_BU = 0x7040U << 15,
  LDX_HU = 0x7048U << 15,
  LDX_WU = 0x7050U << 15,
  FLDX_S = 0x7060U << 15,
  FLDX_D = 0x7068U << 15,
  FSTX_S = 0x7070U << 15,
  FSTX_D = 0x7078U << 15,

  AMSWAP_W = 0x70c0U << 15,
  AMSWAP_D = 0x70c1U << 15,
  AMADD_W = 0x70c2U << 15,
  AMADD_D = 0x70c3U << 15,
  AMAND_W = 0x70c4U << 15,
  AMAND_D = 0x70c5U << 15,
  AMOR_W = 0x70c6U << 15,
  AMOR_D = 0x70c7U << 15,
  AMXOR_W = 0x70c8U << 15,
  AMXOR_D = 0x70c9U << 15,
  AMMAX_W = 0x70caU << 15,
  AMMAX_D = 0x70cbU << 15,
  AMMIN_W = 0x70ccU << 15,
  AMMIN_D = 0x70cdU << 15,
  AMMAX_WU = 0x70ceU << 15,
  AMMAX_DU = 0x70cfU << 15,
  AMMIN_WU = 0x70d0U << 15,
  AMMIN_DU = 0x70d1U << 15,
  AMSWAP_DB_W = 0x70d2U << 15,
  AMSWAP_DB_D = 0x70d3U << 15,
  AMADD_DB_W = 0x70d4U << 15,
  AMADD_DB_D = 0x70d5U << 15,
  AMAND_DB_W = 0x70d6U << 15,
  AMAND_DB_D = 0x70d7U << 15,
  AMOR_DB_W = 0x70d8U << 15,
  AMOR_DB_D = 0x70d9U << 15,
  AMXOR_DB_W = 0x70daU << 15,
  AMXOR_DB_D = 0x70dbU << 15,
  AMMAX_DB_W = 0x70dcU << 15,
  AMMAX_DB_D = 0x70ddU << 15,
  AMMIN_DB_W = 0x70deU << 15,
  AMMIN_DB_D = 0x70dfU << 15,
  AMMAX_DB_WU = 0x70e0U << 15,
  AMMAX_DB_DU = 0x70e1U << 15,
  AMMIN_DB_WU = 0x70e2U << 15,
  AMMIN_DB_DU = 0x70e3U << 15,

  DBAR = 0x70e4U << 15,
  IBAR = 0x70e5U << 15,

  CLO_W = 0X4U << 10,
  CLZ_W = 0X5U << 10,
  CTO_W = 0X6U << 10,
  CTZ_W = 0X7U << 10,
  CLO_D = 0X8U << 10,
  CLZ_D = 0X9U << 10,
  CTO_D = 0XaU << 10,
  CTZ_D = 0XbU << 10,
  REVB_2H = 0XcU << 10,
  REVB_4H = 0XdU << 10,
  REVB_2W = 0XeU << 10,
  REVB_D = 0XfU << 10,
  REVH_2W = 0X10U << 10,
  REVH_D = 0X11U << 10,
  BITREV_4B = 0X12U << 10,
  BITREV_8B = 0X13U << 10,
  BITREV_W = 0X14U << 10,
  BITREV_D = 0X15U << 10,
  EXT_W_H = 0X16U << 10,
  EXT_W_B = 0X17U << 10,

  FABS_S = 0X4501U << 10,
  FABS_D = 0X4502U << 10,
  FNEG_S = 0X4505U << 10,
  FNEG_D = 0X4506U << 10,
  FLOGB_S = 0X4509U << 10,
  FLOGB_D = 0X450aU << 10,
  FCLASS_S = 0X450dU << 10,
  FCLASS_D = 0X450eU << 10,
  FSQRT_S = 0X4511U << 10,
  FSQRT_D = 0X4512U << 10,
  FRECIP_S = 0X4515U << 10,
  FRECIP_D = 0X4516U << 10,
  FRSQRT_S = 0X4519U << 10,
  FRSQRT_D = 0X451aU << 10,
  FMOV_S = 0X4525U << 10,
  FMOV_D = 0X4526U << 10,
  MOVGR2FR_W = 0X4529U << 10,
  MOVGR2FR_D = 0X452aU << 10,
  MOVGR2FRH_W = 0X452bU << 10,
  MOVFR2GR_S = 0X452dU << 10,
  MOVFR2GR_D = 0X452eU << 10,
  MOVFRH2GR_S = 0X452fU << 10,
  MOVGR2FCSR = 0X4530U << 10,
  MOVFCSR2GR = 0X4532U << 10,
  MOVFR2CF = 0X4534U << 10,
  MOVGR2CF = 0X4536U << 10,

  FCVT_S_D = 0x4646U << 10,
  FCVT_D_S = 0x4649U << 10,
  FTINTRM_W_S = 0x4681U << 10,
  FTINTRM_W_D = 0x4682U << 10,
  FTINTRM_L_S = 0x4689U << 10,
  FTINTRM_L_D = 0x468aU << 10,
  FTINTRP_W_S = 0x4691U << 10,
  FTINTRP_W_D = 0x4692U << 10,
  FTINTRP_L_S = 0x4699U << 10,
  FTINTRP_L_D = 0x469aU << 10,
  FTINTRZ_W_S = 0x46a1U << 10,
  FTINTRZ_W_D = 0x46a2U << 10,
  FTINTRZ_L_S = 0x46a9U << 10,
  FTINTRZ_L_D = 0x46aaU << 10,
  FTINTRNE_W_S = 0x46b1U << 10,
  FTINTRNE_W_D = 0x46b2U << 10,
  FTINTRNE_L_S = 0x46b9U << 10,
  FTINTRNE_L_D = 0x46baU << 10,
  FTINT_W_S = 0x46c1U << 10,
  FTINT_W_D = 0x46c2U << 10,
  FTINT_L_S = 0x46c9U << 10,
  FTINT_L_D = 0x46caU << 10,
  FFINT_S_W = 0x4744U << 10,
  FFINT_S_L = 0x4746U << 10,
  FFINT_D_W = 0x4748U << 10,
  FFINT_D_L = 0x474aU << 10,
  FRINT_S = 0x4791U << 10,
  FRINT_D = 0x4792U << 10,

  MOVCF2FR = 0x4535U << 10,
  MOVCF2GR = 0x4537U << 10,

  // SIMD Extension
  VFMADD_S = 0x91U << 20,
  VFMADD_D = 0x92U << 20,
  VFMSUB_S = 0x95U << 20,
  VFMSUB_D = 0x96U << 20,
  VFNMADD_S = 0x99U << 20,
  VFNMADD_D = 0x9AU << 20,
  VFNMSUB_S = 0x9DU << 20,
  VFNMSUB_D = 0x9EU << 20,
  VFCMP_COND_S = 0xC5U << 20,
  VFCMP_COND_D = 0xC6U << 20,
  VBITSEL_V = 0xD1U << 20,
  VSHUF_B = 0xD5U << 20,
  VLD = 0xB0U << 22,
  VST = 0xB1U << 22,
  VLDREPL_D = 0x602U << 19,
  VLDREPL_W = 0x302U << 20,
  VLDREPL_H = 0x182U << 21,
  VLDREPL_B = 0xC2U << 22,
  VSTELM_D = 0x622U << 19,
  VSTELM_W = 0x312U << 20,
  VSTELM_H = 0x18AU << 21,
  VSTELM_B = 0xC6U << 22,
  VLDX = 0x7080U << 15,
  VSTX = 0x7088U << 15,
  VSEQ_B = 0xE000U << 15,
  VSEQ_H = 0xE001U << 15,
  VSEQ_W = 0xE002U << 15,
  VSEQ_D = 0xE003U << 15,
  VSLE_B = 0xE004U << 15,
  VSLE_H = 0xE005U << 15,
  VSLE_W = 0xE006U << 15,
  VSLE_D = 0xE007U << 15,
  VSLE_BU = 0xE008U << 15,
  VSLE_HU = 0xE009U << 15,
  VSLE_WU = 0xE00AU << 15,
  VSLE_DU = 0xE00BU << 15,
  VSLT_B = 0xE00CU << 15,
  VSLT_H = 0xE00DU << 15,
  VSLT_W = 0xE00EU << 15,
  VSLT_D = 0xE00FU << 15,
  VSLT_BU = 0xE010U << 15,
  VSLT_HU = 0xE011U << 15,
  VSLT_WU = 0xE012U << 15,
  VSLT_DU = 0xE013U << 15,
  VADD_B = 0xE014U << 15,
  VADD_H = 0xE015U << 15,
  VADD_W = 0xE016U << 15,
  VADD_D = 0xE017U << 15,
  VSUB_B = 0xE018U << 15,
  VSUB_H = 0xE019U << 15,
  VSUB_W = 0xE01AU << 15,
  VSUB_D = 0xE01BU << 15,
  VADDWEV_H_B = 0xE03CU << 15,
  VADDWEV_W_H = 0xE03DU << 15,
  VADDWEV_D_W = 0xE03EU << 15,
  VADDWEV_Q_D = 0xE03FU << 15,
  VSUBWEV_H_B = 0xE040U << 15,
  VSUBWEV_W_H = 0xE041U << 15,
  VSUBWEV_D_W = 0xE042U << 15,
  VSUBWEV_Q_D = 0xE043U << 15,
  VADDWOD_H_B = 0xE044U << 15,
  VADDWOD_W_H = 0xE045U << 15,
  VADDWOD_D_W = 0xE046U << 15,
  VADDWOD_Q_D = 0xE047U << 15,
  VSUBWOD_H_B = 0xE048U << 15,
  VSUBWOD_W_H = 0xE049U << 15,
  VSUBWOD_D_W = 0xE04AU << 15,
  VSUBWOD_Q_D = 0xE04BU << 15,
  VADDWEV_H_BU = 0xE05CU << 15,
  VADDWEV_W_HU = 0xE05DU << 15,
  VADDWEV_D_WU = 0xE05EU << 15,
  VADDWEV_Q_DU = 0xE05FU << 15,
  VSUBWEV_H_BU = 0xE060U << 15,
  VSUBWEV_W_HU = 0xE061U << 15,
  VSUBWEV_D_WU = 0xE062U << 15,
  VSUBWEV_Q_DU = 0xE063U << 15,
  VADDWOD_H_BU = 0xE064U << 15,
  VADDWOD_W_HU = 0xE065U << 15,
  VADDWOD_D_WU = 0xE066U << 15,
  VADDWOD_Q_DU = 0xE067U << 15,
  VSUBWOD_H_BU = 0xE068U << 15,
  VSUBWOD_W_HU = 0xE069U << 15,
  VSUBWOD_D_WU = 0xE06AU << 15,
  VSUBWOD_Q_DU = 0xE06BU << 15,
  VADDWEV_H_BU_B = 0xE07CU << 15,
  VADDWEV_W_HU_H = 0xE07DU << 15,
  VADDWEV_D_WU_W = 0xE07EU << 15,
  VADDWEV_Q_DU_D = 0xE07FU << 15,
  VADDWOD_H_BU_B = 0xE080U << 15,
  VADDWOD_W_HU_H = 0xE081U << 15,
  VADDWOD_D_WU_W = 0xE082U << 15,
  VADDWOD_Q_DU_D = 0xE083U << 15,
  VSADD_B = 0xE08CU << 15,
  VSADD_H = 0xE08DU << 15,
  VSADD_W = 0xE08EU << 15,
  VSADD_D = 0xE08FU << 15,
  VSSUB_B = 0xE090U << 15,
  VSSUB_H = 0xE091U << 15,
  VSSUB_W = 0xE092U << 15,
  VSSUB_D = 0xE093U << 15,
  VSADD_BU = 0xE094U << 15,
  VSADD_HU = 0xE095U << 15,
  VSADD_WU = 0xE096U << 15,
  VSADD_DU = 0xE097U << 15,
  VSSUB_BU = 0xE098U << 15,
  VSSUB_HU = 0xE099U << 15,
  VSSUB_WU = 0xE09AU << 15,
  VSSUB_DU = 0xE09BU << 15,
  VHADDW_H_B = 0xE0A8U << 15,
  VHADDW_W_H = 0xE0A9U << 15,
  VHADDW_D_W = 0xE0AAU << 15,
  VHADDW_Q_D = 0xE0ABU << 15,
  VHSUBW_H_B = 0xE0ACU << 15,
  VHSUBW_W_H = 0xE0ADU << 15,
  VHSUBW_D_W = 0xE0AEU << 15,
  VHSUBW_Q_D = 0xE0AFU << 15,
  VHADDW_HU_BU = 0xE0B0U << 15,
  VHADDW_WU_HU = 0xE0B1U << 15,
  VHADDW_DU_WU = 0xE0B2U << 15,
  VHADDW_QU_DU = 0xE0B3U << 15,
  VHSUBW_HU_BU = 0xE0B4U << 15,
  VHSUBW_WU_HU = 0xE0B5U << 15,
  VHSUBW_DU_WU = 0xE0B6U << 15,
  VHSUBW_QU_DU = 0xE0B7U << 15,
  VADDA_B = 0xE0B8U << 15,
  VADDA_H = 0xE0B9U << 15,
  VADDA_W = 0xE0BAU << 15,
  VADDA_D = 0xE0BBU << 15,
  VABSD_B = 0xE0C0U << 15,
  VABSD_H = 0xE0C1U << 15,
  VABSD_W = 0xE0C2U << 15,
  VABSD_D = 0xE0C3U << 15,
  VABSD_BU = 0xE0C4U << 15,
  VABSD_HU = 0xE0C5U << 15,
  VABSD_WU = 0xE0C6U << 15,
  VABSD_DU = 0xE0C7U << 15,
  VAVG_B = 0xE0C8U << 15,
  VAVG_H = 0xE0C9U << 15,
  VAVG_W = 0xE0CAU << 15,
  VAVG_D = 0xE0CBU << 15,
  VAVG_BU = 0xE0CCU << 15,
  VAVG_HU = 0xE0CDU << 15,
  VAVG_WU = 0xE0CEU << 15,
  VAVG_DU = 0xE0CFU << 15,
  VAVGR_B = 0xE0D0U << 15,
  VAVGR_H = 0xE0D1U << 15,
  VAVGR_W = 0xE0D2U << 15,
  VAVGR_D = 0xE0D3U << 15,
  VAVGR_BU = 0xE0D4U << 15,
  VAVGR_HU = 0xE0D5U << 15,
  VAVGR_WU = 0xE0D6U << 15,
  VAVGR_DU = 0xE0D7U << 15,
  VMAX_B = 0xE0E0U << 15,
  VMAX_H = 0xE0E1U << 15,
  VMAX_W = 0xE0E2U << 15,
  VMAX_D = 0xE0E3U << 15,
  VMIN_B = 0xE0E4U << 15,
  VMIN_H = 0xE0E5U << 15,
  VMIN_W = 0xE0E6U << 15,
  VMIN_D = 0xE0E7U << 15,
  VMAX_BU = 0xE0E8U << 15,
  VMAX_HU = 0xE0E9U << 15,
  VMAX_WU = 0xE0EAU << 15,
  VMAX_DU = 0xE0EBU << 15,
  VMIN_BU = 0xE0ECU << 15,
  VMIN_HU = 0xE0EDU << 15,
  VMIN_WU = 0xE0EEU << 15,
  VMIN_DU = 0xE0EFU << 15,
  VMUL_B = 0xE108U << 15,
  VMUL_H = 0xE109U << 15,
  VMUL_W = 0xE10AU << 15,
  VMUL_D = 0xE10BU << 15,
  VMUH_B = 0xE10CU << 15,
  VMUH_H = 0xE10DU << 15,
  VMUH_W = 0xE10EU << 15,
  VMUH_D = 0xE10FU << 15,
  VMUH_BU = 0xE110U << 15,
  VMUH_HU = 0xE111U << 15,
  VMUH_WU = 0xE112U << 15,
  VMUH_DU = 0xE113U << 15,
  VMULWEV_H_B = 0xE120U << 15,
  VMULWEV_W_H = 0xE121U << 15,
  VMULWEV_D_W = 0xE122U << 15,
  VMULWEV_Q_D = 0xE123U << 15,
  VMULWOD_H_B = 0xE124U << 15,
  VMULWOD_W_H = 0xE125U << 15,
  VMULWOD_D_W = 0xE126U << 15,
  VMULWOD_Q_D = 0xE127U << 15,
  VMULWEV_H_BU = 0xE130U << 15,
  VMULWEV_W_HU = 0xE131U << 15,
  VMULWEV_D_WU = 0xE132U << 15,
  VMULWEV_Q_DU = 0xE133U << 15,
  VMULWOD_H_BU = 0xE134U << 15,
  VMULWOD_W_HU = 0xE135U << 15,
  VMULWOD_D_WU = 0xE136U << 15,
  VMULWOD_Q_DU = 0xE137U << 15,
  VMULWEV_H_BU_B = 0xE140U << 15,
  VMULWEV_W_HU_H = 0xE141U << 15,
  VMULWEV_D_WU_W = 0xE142U << 15,
  VMULWEV_Q_DU_D = 0xE143U << 15,
  VMULWOD_H_BU_B = 0xE144U << 15,
  VMULWOD_W_HU_H = 0xE145U << 15,
  VMULWOD_D_WU_W = 0xE146U << 15,
  VMULWOD_Q_DU_D = 0xE147U << 15,
  VMADD_B = 0xE150U << 15,
  VMADD_H = 0xE151U << 15,
  VMADD_W = 0xE152U << 15,
  VMADD_D = 0xE153U << 15,
  VMSUB_B = 0xE154U << 15,
  VMSUB_H = 0xE155U << 15,
  VMSUB_W = 0xE156U << 15,
  VMSUB_D = 0xE157U << 15,
  VMADDWEV_H_B = 0xE158U << 15,
  VMADDWEV_W_H = 0xE159U << 15,
  VMADDWEV_D_W = 0xE15AU << 15,
  VMADDWEV_Q_D = 0xE15BU << 15,
  VMADDWOD_H_B = 0xE15CU << 15,
  VMADDWOD_W_H = 0xE15DU << 15,
  VMADDWOD_D_W = 0xE15EU << 15,
  VMADDWOD_Q_D = 0xE15FU << 15,
  VMADDWEV_H_BU = 0xE168U << 15,
  VMADDWEV_W_HU = 0xE169U << 15,
  VMADDWEV_D_WU = 0xE16AU << 15,
  VMADDWEV_Q_DU = 0xE16BU << 15,
  VMADDWOD_H_BU = 0xE16CU << 15,
  VMADDWOD_W_HU = 0xE16DU << 15,
  VMADDWOD_D_WU = 0xE16EU << 15,
  VMADDWOD_Q_DU = 0xE16FU << 15,
  VMADDWEV_H_BU_B = 0xE178U << 15,
  VMADDWEV_W_HU_H = 0xE179U << 15,
  VMADDWEV_D_WU_W = 0xE17AU << 15,
  VMADDWEV_Q_DU_D = 0xE17BU << 15,
  VMADDWOD_H_BU_B = 0xE17CU << 15,
  VMADDWOD_W_HU_H = 0xE17DU << 15,
  VMADDWOD_D_WU_W = 0xE17EU << 15,
  VMADDWOD_Q_DU_D = 0xE17FU << 15,
  VDIV_B = 0xE1C0U << 15,
  VDIV_H = 0xE1C1U << 15,
  VDIV_W = 0xE1C2U << 15,
  VDIV_D = 0xE1C3U << 15,
  VMOD_B = 0xE1C4U << 15,
  VMOD_H = 0xE1C5U << 15,
  VMOD_W = 0xE1C6U << 15,
  VMOD_D = 0xE1C7U << 15,
  VDIV_BU = 0xE1C8U << 15,
  VDIV_HU = 0xE1C9U << 15,
  VDIV_WU = 0xE1CAU << 15,
  VDIV_DU = 0xE1CBU << 15,
  VMOD_BU = 0xE1CCU << 15,
  VMOD_HU = 0xE1CDU << 15,
  VMOD_WU = 0xE1CEU << 15,
  VMOD_DU = 0xE1CFU << 15,
  VSLL_B = 0xE1D0U << 15,
  VSLL_H = 0xE1D1U << 15,
  VSLL_W = 0xE1D2U << 15,
  VSLL_D = 0xE1D3U << 15,
  VSRL_B = 0xE1D4U << 15,
  VSRL_H = 0xE1D5U << 15,
  VSRL_W = 0xE1D6U << 15,
  VSRL_D = 0xE1D7U << 15,
  VSRA_B = 0xE1D8U << 15,
  VSRA_H = 0xE1D9U << 15,
  VSRA_W = 0xE1DAU << 15,
  VSRA_D = 0xE1DBU << 15,
  VROTR_B = 0xE1DCU << 15,
  VROTR_H = 0xE1DDU << 15,
  VROTR_W = 0xE1DEU << 15,
  VROTR_D = 0xE1DFU << 15,
  VSRLR_B = 0xE1E0U << 15,
  VSRLR_H = 0xE1E1U << 15,
  VSRLR_W = 0xE1E2U << 15,
  VSRLR_D = 0xE1E3U << 15,
  VSRAR_B = 0xE1E4U << 15,
  VSRAR_H = 0xE1E5U << 15,
  VSRAR_W = 0xE1E6U << 15,
  VSRAR_D = 0xE1E7U << 15,
  VSRLN_B_H = 0xE1E9U << 15,
  VSRLN_H_W = 0xE1EAU << 15,
  VSRLN_W_D = 0xE1EBU << 15,
  VSRAN_B_H = 0xE1EDU << 15,
  VSRAN_H_W = 0xE1EEU << 15,
  VSRAN_W_D = 0xE1EFU << 15,
  VSRLRN_B_H = 0xE1F1U << 15,
  VSRLRN_H_W = 0xE1F2U << 15,
  VSRLRN_W_D = 0xE1F3U << 15,
  VSRARN_B_H = 0xE1F5U << 15,
  VSRARN_H_W = 0xE1F6U << 15,
  VSRARN_W_D = 0xE1F7U << 15,
  VSSRLN_B_H = 0xE1F9U << 15,
  VSSRLN_H_W = 0xE1FAU << 15,
  VSSRLN_W_D = 0xE1FBU << 15,
  VSSRAN_B_H = 0xE1FDU << 15,
  VSSRAN_H_W = 0xE1FEU << 15,
  VSSRAN_W_D = 0xE1FFU << 15,
  VSSRLRN_B_H = 0xE201U << 15,
  VSSRLRN_H_W = 0xE202U << 15,
  VSSRLRN_W_D = 0xE203U << 15,
  VSSRARN_B_H = 0xE205U << 15,
  VSSRARN_H_W = 0xE206U << 15,
  VSSRARN_W_D = 0xE207U << 15,
  VSSRLN_BU_H = 0xE209U << 15,
  VSSRLN_HU_W = 0xE20AU << 15,
  VSSRLN_WU_D = 0xE20BU << 15,
  VSSRAN_BU_H = 0xE20DU << 15,
  VSSRAN_HU_W = 0xE20EU << 15,
  VSSRAN_WU_D = 0xE20FU << 15,
  VSSRLRN_BU_H = 0xE211U << 15,
  VSSRLRN_HU_W = 0xE212U << 15,
  VSSRLRN_WU_D = 0xE213U << 15,
  VSSRARN_BU_H = 0xE215U << 15,
  VSSRARN_HU_W = 0xE216U << 15,
  VSSRARN_WU_D = 0xE217U << 15,
  VBITCLR_B = 0xE218U << 15,
  VBITCLR_H = 0xE219U << 15,
  VBITCLR_W = 0xE21AU << 15,
  VBITCLR_D = 0xE21BU << 15,
  VBITSET_B = 0xE21CU << 15,
  VBITSET_H = 0xE21DU << 15,
  VBITSET_W = 0xE21EU << 15,
  VBITSET_D = 0xE21FU << 15,
  VBITREV_B = 0xE220U << 15,
  VBITREV_H = 0xE221U << 15,
  VBITREV_W = 0xE222U << 15,
  VBITREV_D = 0xE223U << 15,
  VPACKEV_B = 0xE22CU << 15,
  VPACKEV_H = 0xE22DU << 15,
  VPACKEV_W = 0xE22EU << 15,
  VPACKEV_D = 0xE22FU << 15,
  VPACKOD_B = 0xE230U << 15,
  VPACKOD_H = 0xE231U << 15,
  VPACKOD_W = 0xE232U << 15,
  VPACKOD_D = 0xE233U << 15,
  VILVL_B = 0xE234U << 15,
  VILVL_H = 0xE235U << 15,
  VILVL_W = 0xE236U << 15,
  VILVL_D = 0xE237U << 15,
  VILVH_B = 0xE238U << 15,
  VILVH_H = 0xE239U << 15,
  VILVH_W = 0xE23AU << 15,
  VILVH_D = 0xE23BU << 15,
  VPICKEV_B = 0xE23CU << 15,
  VPICKEV_H = 0xE23DU << 15,
  VPICKEV_W = 0xE23EU << 15,
  VPICKEV_D = 0xE23FU << 15,
  VPICKOD_B = 0xE240U << 15,
  VPICKOD_H = 0xE241U << 15,
  VPICKOD_W = 0xE242U << 15,
  VPICKOD_D = 0xE243U << 15,
  VREPLVE_B = 0xE244U << 15,
  VREPLVE_H = 0xE245U << 15,
  VREPLVE_W = 0xE246U << 15,
  VREPLVE_D = 0xE247U << 15,
  VAND_V = 0xE24CU << 15,
  VOR_V = 0xE24DU << 15,
  VXOR_V = 0xE24EU << 15,
  VNOR_V = 0xE24FU << 15,
  VANDN_V = 0xE250U << 15,
  VORN_V = 0xE251U << 15,
  VFRSTP_B = 0xE256U << 15,
  VFRSTP_H = 0xE257U << 15,
  VADD_Q = 0xE25AU << 15,
  VSUB_Q = 0xE25BU << 15,
  VSIGNCOV_B = 0xE25CU << 15,
  VSIGNCOV_H = 0xE25DU << 15,
  VSIGNCOV_W = 0xE25EU << 15,
  VSIGNCOV_D = 0xE25FU << 15,
  VFADD_S = 0xE261U << 15,
  VFADD_D = 0xE262U << 15,
  VFSUB_S = 0xE265U << 15,
  VFSUB_D = 0xE266U << 15,
  VFMUL_S = 0xE271U << 15,
  VFMUL_D = 0xE272U << 15,
  VFDIV_S = 0xE275U << 15,
  VFDIV_D = 0xE276U << 15,
  VFMAX_S = 0xE279U << 15,
  VFMAX_D = 0xE27AU << 15,
  VFMIN_S = 0xE27DU << 15,
  VFMIN_D = 0xE27EU << 15,
  VFMAXA_S = 0xE281U << 15,
  VFMAXA_D = 0xE282U << 15,
  VFMINA_S = 0xE285U << 15,
  VFMINA_D = 0xE286U << 15,
  VFCVT_H_S = 0xE28CU << 15,
  VFCVT_S_D = 0xE28DU << 15,
  VFFINT_S_L = 0xE290U << 15,
  VFTINT_W_D = 0xE293U << 15,
  VFTINTRM_W_D = 0xE294U << 15,
  VFTINTRP_W_D = 0xE295U << 15,
  VFTINTRZ_W_D = 0xE296U << 15,
  VFTINTRNE_W_D = 0xE297U << 15,
  VSHUF_H = 0xE2F5U << 15,
  VSHUF_W = 0xE2F6U << 15,
  VSHUF_D = 0xE2F7U << 15,
  VSEQI_B = 0xE500U << 15,
  VSEQI_H = 0xE501U << 15,
  VSEQI_W = 0xE502U << 15,
  VSEQI_D = 0xE503U << 15,
  VSLEI_B = 0xE504U << 15,
  VSLEI_H = 0xE505U << 15,
  VSLEI_W = 0xE506U << 15,
  VSLEI_D = 0xE507U << 15,
  VSLEI_BU = 0xE508U << 15,
  VSLEI_HU = 0xE509U << 15,
  VSLEI_WU = 0xE50AU << 15,
  VSLEI_DU = 0xE50BU << 15,
  VSLTI_B = 0xE50CU << 15,
  VSLTI_H = 0xE50DU << 15,
  VSLTI_W = 0xE50EU << 15,
  VSLTI_D = 0xE50FU << 15,
  VSLTI_BU = 0xE510U << 15,
  VSLTI_HU = 0xE511U << 15,
  VSLTI_WU = 0xE512U << 15,
  VSLTI_DU = 0xE513U << 15,
  VADDI_BU = 0xE514U << 15,
  VADDI_HU = 0xE515U << 15,
  VADDI_WU = 0xE516U << 15,
  VADDI_DU = 0xE517U << 15,
  VSUBI_BU = 0xE518U << 15,
  VSUBI_HU = 0xE519U << 15,
  VSUBI_WU = 0xE51AU << 15,
  VSUBI_DU = 0xE51BU << 15,
  VBSLL_V = 0xE51CU << 15,
  VBSRL_V = 0xE51DU << 15,
  VMAXI_B = 0xE520U << 15,
  VMAXI_H = 0xE521U << 15,
  VMAXI_W = 0xE522U << 15,
  VMAXI_D = 0xE523U << 15,
  VMINI_B = 0xE524U << 15,
  VMINI_H = 0xE525U << 15,
  VMINI_W = 0xE526U << 15,
  VMINI_D = 0xE527U << 15,
  VMAXI_BU = 0xE528U << 15,
  VMAXI_HU = 0xE529U << 15,
  VMAXI_WU = 0xE52AU << 15,
  VMAXI_DU = 0xE52BU << 15,
  VMINI_BU = 0xE52CU << 15,
  VMINI_HU = 0xE52DU << 15,
  VMINI_WU = 0xE52EU << 15,
  VMINI_DU = 0xE52FU << 15,
  VFRSTPI_B = 0xE534U << 15,
  VFRSTPI_H = 0xE535U << 15,
  VCLO_B = 0x1CA700U << 10,
  VCLO_H = 0x1CA701U << 10,
  VCLO_W = 0x1CA702U << 10,
  VCLO_D = 0x1CA703U << 10,
  VCLZ_B = 0x1CA704U << 10,
  VCLZ_H = 0x1CA705U << 10,
  VCLZ_W = 0x1CA706U << 10,
  VCLZ_D = 0x1CA707U << 10,
  VPCNT_B = 0x1CA708U << 10,
  VPCNT_H = 0x1CA709U << 10,
  VPCNT_W = 0x1CA70AU << 10,
  VPCNT_D = 0x1CA70BU << 10,
  VNEG_B = 0x1CA70CU << 10,
  VNEG_H = 0x1CA70DU << 10,
  VNEG_W = 0x1CA70EU << 10,
  VNEG_D = 0x1CA70FU << 10,
  VMSKLTZ_B = 0x1CA710U << 10,
  VMSKLTZ_H = 0x1CA711U << 10,
  VMSKLTZ_W = 0x1CA712U << 10,
  VMSKLTZ_D = 0x1CA713U << 10,
  VMSKGEZ_B = 0x1CA714U << 10,
  VMSKNZ_B = 0x1CA718U << 10,
  VSETEQZ_V = 0x1CA726U << 10,
  VSETNEZ_V = 0x1CA727U << 10,
  VSETANYEQZ_B = 0x1CA728U << 10,
  VSETANYEQZ_H = 0x1CA729U << 10,
  VSETANYEQZ_W = 0x1CA72AU << 10,
  VSETANYEQZ_D = 0x1CA72BU << 10,
  VSETALLNEZ_B = 0x1CA72CU << 10,
  VSETALLNEZ_H = 0x1CA72DU << 10,
  VSETALLNEZ_W = 0x1CA72EU << 10,
  VSETALLNEZ_D = 0x1CA72FU << 10,
  VFLOGB_S = 0x1CA731U << 10,
  VFLOGB_D = 0x1CA732U << 10,
  VFCLASS_S = 0x1CA735U << 10,
  VFCLASS_D = 0x1CA736U << 10,
  VFSQRT_S = 0x1CA739U << 10,
  VFSQRT_D = 0x1CA73AU << 10,
  VFRECIP_S = 0x1CA73DU << 10,
  VFRECIP_D = 0x1CA73EU << 10,
  VFRSQRT_S = 0x1CA741U << 10,
  VFRSQRT_D = 0x1CA742U << 10,
  VFRINT_S = 0x1CA74DU << 10,
  VFRINT_D = 0x1CA74EU << 10,
  VFRINTRM_S = 0x1CA751U << 10,
  VFRINTRM_D = 0x1CA752U << 10,
  VFRINTRP_S = 0x1CA755U << 10,
  VFRINTRP_D = 0x1CA756U << 10,
  VFRINTRZ_S = 0x1CA759U << 10,
  VFRINTRZ_D = 0x1CA75AU << 10,
  VFRINTRNE_S = 0x1CA75DU << 10,
  VFRINTRNE_D = 0x1CA75EU << 10,
  VFCVTL_S_H = 0x1CA77AU << 10,
  VFCVTH_S_H = 0x1CA77BU << 10,
  VFCVTL_D_S = 0x1CA77CU << 10,
  VFCVTH_D_S = 0x1CA77DU << 10,
  VFFINT_S_W = 0x1CA780U << 10,
  VFFINT_S_WU = 0x1CA781U << 10,
  VFFINT_D_L = 0x1CA782U << 10,
  VFFINT_D_LU = 0x1CA783U << 10,
  VFFINTL_D_W = 0x1CA784U << 10,
  VFFINTH_D_W = 0x1CA785U << 10,
  VFTINT_W_S = 0x1CA78CU << 10,
  VFTINT_L_D = 0x1CA78DU << 10,
  VFTINTRM_W_S = 0x1CA78EU << 10,
  VFTINTRM_L_D = 0x1CA78FU << 10,
  VFTINTRP_W_S = 0x1CA790U << 10,
  VFTINTRP_L_D = 0x1CA791U << 10,
  VFTINTRZ_W_S = 0x1CA792U << 10,
  VFTINTRZ_L_D = 0x1CA793U << 10,
  VFTINTRNE_W_S = 0x1CA794U << 10,
  VFTINTRNE_L_D = 0x1CA795U << 10,
  VFTINT_WU_S = 0x1CA796U << 10,
  VFTINT_LU_D = 0x1CA797U << 10,
  VFTINTRZ_WU_S = 0x1CA79CU << 10,
  VFTINTRZ_LU_D = 0x1CA79DU << 10,
  VFTINTL_L_S = 0x1CA7A0U << 10,
  VFTINTH_L_S = 0x1CA7A1U << 10,
  VFTINTRML_L_S = 0x1CA7A2U << 10,
  VFTINTRMH_L_S = 0x1CA7A3U << 10,
  VFTINTRPL_L_S = 0x1CA7A4U << 10,
  VFTINTRPH_L_S = 0x1CA7A5U << 10,
  VFTINTRZL_L_S = 0x1CA7A6U << 10,
  VFTINTRZH_L_S = 0x1CA7A7U << 10,
  VFTINTRNEL_L_S = 0x1CA7A8U << 10,
  VFTINTRNEH_L_S = 0x1CA7A9U << 10,
  VEXTH_H_B = 0x1CA7B8U << 10,
  VEXTH_W_H = 0x1CA7B9U << 10,
  VEXTH_D_W = 0x1CA7BAU << 10,
  VEXTH_Q_D = 0x1CA7BBU << 10,
  VEXTH_HU_BU = 0x1CA7BCU << 10,
  VEXTH_WU_HU = 0x1CA7BDU << 10,
  VEXTH_DU_WU = 0x1CA7BEU << 10,
  VEXTH_QU_DU = 0x1CA7BFU << 10,
  VREPLGR2VR_B = 0x1CA7C0U << 10,
  VREPLGR2VR_H = 0x1CA7C1U << 10,
  VREPLGR2VR_W = 0x1CA7C2U << 10,
  VREPLGR2VR_D = 0x1CA7C3U << 10,
  VROTRI_B = 0x39501U << 13,
  VROTRI_H = 0x1CA81U << 14,
  VROTRI_W = 0xE541U << 15,
  VROTRI_D = 0x72A1U << 16,
  VSRLRI_B = 0x39521U << 13,
  VSRLRI_H = 0x1CA91U << 14,
  VSRLRI_W = 0xE549U << 15,
  VSRLRI_D = 0x72A5U << 16,
  VSRARI_B = 0x39541U << 13,
  VSRARI_H = 0x1CAA1U << 14,
  VSRARI_W = 0xE551U << 15,
  VSRARI_D = 0x72A9U << 16,
  VINSGR2VR_B = 0x1CBAEU << 14,
  VINSGR2VR_H = 0x3975EU << 13,
  VINSGR2VR_W = 0x72EBEU << 12,
  VINSGR2VR_D = 0xE5D7EU << 11,
  VPICKVE2GR_B = 0x1CBBEU << 14,
  VPICKVE2GR_H = 0x3977EU << 13,
  VPICKVE2GR_W = 0x72EFEU << 12,
  VPICKVE2GR_D = 0xE5DFEU << 11,
  VPICKVE2GR_BU = 0x1CBCEU << 14,
  VPICKVE2GR_HU = 0x3979EU << 13,
  VPICKVE2GR_WU = 0x72F3EU << 12,
  VPICKVE2GR_DU = 0xE5E7EU << 11,
  VREPLVEI_B = 0x1CBDEU << 14,
  VREPLVEI_H = 0x397BEU << 13,
  VREPLVEI_W = 0x72F7EU << 12,
  VREPLVEI_D = 0xE5EFEU << 11,
  VSLLWIL_H_B = 0x39841U << 13,
  VSLLWIL_W_H = 0x1CC21U << 14,
  VSLLWIL_D_W = 0xE611U << 15,
  VEXTL_Q_D = 0x1CC240U << 10,
  VSLLWIL_HU_BU = 0x39861U << 13,
  VSLLWIL_WU_HU = 0x1CC31U << 14,
  VSLLWIL_DU_WU = 0xE619U << 15,
  VEXTL_QU_DU = 0x1CC340U << 10,
  VBITCLRI_B = 0x39881U << 13,
  VBITCLRI_H = 0x1CC41U << 14,
  VBITCLRI_W = 0xE621U << 15,
  VBITCLRI_D = 0x7311U << 16,
  VBITSETI_B = 0x398A1U << 13,
  VBITSETI_H = 0x1CC51U << 14,
  VBITSETI_W = 0xE629U << 15,
  VBITSETI_D = 0x7315U << 16,
  VBITREVI_B = 0x398C1U << 13,
  VBITREVI_H = 0x1CC61U << 14,
  VBITREVI_W = 0xE631U << 15,
  VBITREVI_D = 0x7319U << 16,
  VSAT_B = 0x39921U << 13,
  VSAT_H = 0x1CC91U << 14,
  VSAT_W = 0xE649U << 15,
  VSAT_D = 0x7325U << 16,
  VSAT_BU = 0x39941U << 13,
  VSAT_HU = 0x1CCA1U << 14,
  VSAT_WU = 0xE651U << 15,
  VSAT_DU = 0x7329U << 16,
  VSLLI_B = 0x39961U << 13,
  VSLLI_H = 0x1CCB1U << 14,
  VSLLI_W = 0xE659U << 15,
  VSLLI_D = 0x732DU << 16,
  VSRLI_B = 0x39981U << 13,
  VSRLI_H = 0x1CCC1U << 14,
  VSRLI_W = 0xE661U << 15,
  VSRLI_D = 0x7331U << 16,
  VSRAI_B = 0x399A1U << 13,
  VSRAI_H = 0x1CCD1U << 14,
  VSRAI_W = 0xE669U << 15,
  VSRAI_D = 0x7335U << 16,
  VSRLNI_B_H = 0x1CD01U << 14,
  VSRLNI_H_W = 0xE681U << 15,
  VSRLNI_W_D = 0x7341U << 16,
  VSRLNI_D_Q = 0x39A1U << 17,
  VSRLRNI_B_H = 0x1CD11U << 14,
  VSRLRNI_H_W = 0xE689U << 15,
  VSRLRNI_W_D = 0x7345U << 16,
  VSRLRNI_D_Q = 0x39A3U << 17,
  VSSRLNI_B_H = 0x1CD21U << 14,
  VSSRLNI_H_W = 0xE691U << 15,
  VSSRLNI_W_D = 0x7349U << 16,
  VSSRLNI_D_Q = 0x39A5U << 17,
  VSSRLNI_BU_H = 0x1CD31U << 14,
  VSSRLNI_HU_W = 0xE699U << 15,
  VSSRLNI_WU_D = 0x734DU << 16,
  VSSRLNI_DU_Q = 0x39A7U << 17,
  VSSRLRNI_B_H = 0x1CD41U << 14,
  VSSRLRNI_H_W = 0xE6A1U << 15,
  VSSRLRNI_W_D = 0x7351U << 16,
  VSSRLRNI_D_Q = 0x39A9U << 17,
  VSSRLRNI_BU_H = 0x1CD51U << 14,
  VSSRLRNI_HU_W = 0xE6A9U << 15,
  VSSRLRNI_WU_D = 0x7355U << 16,
  VSSRLRNI_DU_Q = 0x39ABU << 17,
  VSRANI_B_H = 0x1CD61U << 14,
  VSRANI_H_W = 0xE6B1U << 15,
  VSRANI_W_D = 0x7359U << 16,
  VSRANI_D_Q = 0x39ADU << 17,
  VSRARNI_B_H = 0x1CD71U << 14,
  VSRARNI_H_W = 0xE6B9U << 15,
  VSRARNI_W_D = 0x735DU << 16,
  VSRARNI_D_Q = 0x39AFU << 17,
  VSSRANI_B_H = 0x1CD81U << 14,
  VSSRANI_H_W = 0xE6C1U << 15,
  VSSRANI_W_D = 0x7361U << 16,
  VSSRANI_D_Q = 0x39B1U << 17,
  VSSRANI_BU_H = 0x1CD91U << 14,
  VSSRANI_HU_W = 0xE6C9U << 15,
  VSSRANI_WU_D = 0x7365U << 16,
  VSSRANI_DU_Q = 0x39B3U << 17,
  VSSRARNI_B_H = 0x1CDA1U << 14,
  VSSRARNI_H_W = 0xE6D1U << 15,
  VSSRARNI_W_D = 0x7369U << 16,
  VSSRARNI_D_Q = 0x39B5U << 17,
  VSSRARNI_BU_H = 0x1CDB1U << 14,
  VSSRARNI_HU_W = 0xE6D9U << 15,
  VSSRARNI_WU_D = 0x736DU << 16,
  VSSRARNI_DU_Q = 0x39B7U << 17,
  VEXTRINS_D = 0x1CE0U << 18,
  VEXTRINS_W = 0x1CE1U << 18,
  VEXTRINS_H = 0x1CE2U << 18,
  VEXTRINS_B = 0x1CE3U << 18,
  VSHUF4I_B = 0x1CE4U << 18,
  VSHUF4I_H = 0x1CE5U << 18,
  VSHUF4I_W = 0x1CE6U << 18,
  VSHUF4I_D = 0x1CE7U << 18,
  VBITSELI_B = 0x1CF1U << 18,
  VANDI_B = 0x1CF4U << 18,
  VORI_B = 0x1CF5U << 18,
  VXORI_B = 0x1CF6U << 18,
  VNORI_B = 0x1CF7U << 18,
  VLDI = 0x1CF8U << 18,
  VPERMI_W = 0x1CF9U << 18,
};

// ----- Emulated conditions.
// On LOONG64 we use this enum to abstract from conditional branch instructions.
// The 'U' prefix is used to specify unsigned comparisons.
enum Condition : int {
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

  // Unified cross-platform condition names/aliases.
  kEqual = equal,
  kNotEqual = not_equal,
  kLessThan = less,
  kGreaterThan = greater,
  kLessThanEqual = less_equal,
  kGreaterThanEqual = greater_equal,
  kUnsignedLessThan = Uless,
  kUnsignedGreaterThan = Ugreater,
  kUnsignedLessThanEqual = Uless_equal,
  kUnsignedGreaterThanEqual = Ugreater_equal,
  kOverflow = overflow,
  kNoOverflow = no_overflow,
  kZero = equal,
  kNotZero = not_equal,
};

// Returns the equivalent of !cc.
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

enum LSXBranchCondition {
  all_not_zero = 0,   // Branch If All Elements Are Not Zero
  one_elem_not_zero,  // Branch If At Least One Element of Any Format Is Not
                      // Zero
  one_elem_zero,      // Branch If At Least One Element Is Zero
  all_zero            // Branch If All Elements of Any Format Are Zero
};

inline LSXBranchCondition NegateLSXBranchCondition(LSXBranchCondition cond) {
  switch (cond) {
    case all_not_zero:
      return one_elem_zero;
    case one_elem_not_zero:
      return all_zero;
    case one_elem_zero:
      return all_not_zero;
    case all_zero:
      return one_elem_not_zero;
    default:
      return cond;
  }
}

enum LSXBranchDF {
  LSX_BRANCH_B = 0,
  LSX_BRANCH_H,
  LSX_BRANCH_W,
  LSX_BRANCH_D,
  LSX_BRANCH_V
};

// ----- Coprocessor conditions.
enum FPUCondition {
  kNoFPUCondition = -1,

  CAF = 0x00,   // False.
  SAF = 0x01,   // False.
  CLT = 0x02,   // Less Than quiet
  SLT_ = 0x03,  // Less Than signaling
  CEQ = 0x04,
  SEQ = 0x05,
  CLE = 0x06,
  SLE = 0x07,
  CUN = 0x08,
  SUN = 0x09,
  CULT = 0x0a,
  SULT = 0x0b,
  CUEQ = 0x0c,
  SUEQ = 0x0d,
  CULE = 0x0e,
  SULE = 0x0f,
  CNE = 0x10,
  SNE = 0x11,
  COR = 0x14,
  SOR = 0x15,
  CUNE = 0x18,
  SUNE = 0x19,
};

const uint32_t kFPURoundingModeShift = 8;
const uint32_t kFPURoundingModeMask = 0b11 << kFPURoundingModeShift;

// FPU rounding modes.
enum FPURoundingMode {
  RN = 0b00 << kFPURoundingModeShift,  // Round to Nearest.
  RZ = 0b01 << kFPURoundingModeShift,  // Round towards zero.
  RP = 0b10 << kFPURoundingModeShift,  // Round towards Plus Infinity.
  RM = 0b11 << kFPURoundingModeShift,  // Round towards Minus Infinity.

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

enum CheckForInexactConversion {
  kCheckForInexactConversion,
  kDontCheckForInexactConversion
};

enum class MaxMinKind : int { kMin = 0, kMax = 1 };

// -----------------------------------------------------------------------------
// Hints.

// Branch hints are not used on the LOONG64.  They are defined so that they can
// appear in shared function signatures, but will be ignored in LOONG64
// implementations.
enum Hint { no_hint = 0 };

inline Hint NegateHint(Hint hint) { return no_hint; }

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.
// These constants are declared in assembler-loong64.cc, as they use named
// registers and other constants.

// Break 0xfffff, reserved for redirected real time call.
const Instr rtCallRedirInstr =
    static_cast<uint32_t>(BREAK) | call_rt_redirected;
// A nop instruction. (Encoding of addi_w 0 0 0).
const Instr nopInstr = ADDI_W;

constexpr uint8_t kInstrSize = 4;
constexpr uint8_t kInstrSizeLog2 = 2;

class InstructionBase {
 public:
  enum Type {
    kOp6Type,
    kOp7Type,
    kOp8Type,
    kOp10Type,
    kOp11Type,
    kOp12Type,
    kOp13Type,
    kOp14Type,
    kOp15Type,
    kOp16Type,
    kOp17Type,
    kOp18Type,
    kOp19Type,
    kOp20Type,
    kOp21Type,
    kOp22Type,
    kUnsupported = -1
  };

  // Get the raw instruction bits.
  inline Instr InstructionBits() const {
    return *reinterpret_cast<const Instr*>(this);
  }

  // Set the raw instruction bits to value.
  V8_EXPORT_PRIVATE void SetInstructionBits(
      Instr new_instr, WritableJitAllocation* jit_allocation = nullptr);

  // Read one particular bit out of the instruction bits.
  inline int Bit(int nr) const { return (InstructionBits() >> nr) & 1; }

  // Read a bit field out of the instruction bits.
  inline int Bits(int hi, int lo) const {
    return (InstructionBits() >> lo) & ((2U << (hi - lo)) - 1);
  }

  // Safe to call within InstructionType().
  inline int RjFieldRawNoAssert() const {
    return InstructionBits() & kRjFieldMask;
  }

  // Get the encoding type of the instruction.
  inline Type InstructionType() const;

 protected:
  InstructionBase() {}
};

template <class T>
class InstructionGetters : public T {
 public:
  inline int RjValue() const {
    return this->Bits(kRjShift + kRjBits - 1, kRjShift);
  }

  inline int RkValue() const {
    return this->Bits(kRkShift + kRkBits - 1, kRkShift);
  }

  inline int RdValue() const {
    return this->Bits(kRdShift + kRdBits - 1, kRdShift);
  }

  inline int Sa2Value() const {
    return this->Bits(kSaShift + kSa2Bits - 1, kSaShift);
  }

  inline int Sa3Value() const {
    return this->Bits(kSaShift + kSa3Bits - 1, kSaShift);
  }

  inline int Ui1Value() const {
    return this->Bits(kUi1Shift + kUi1Bits - 1, kUi1Shift);
  }

  inline int Ui2Value() const {
    return this->Bits(kUi2Shift + kUi2Bits - 1, kUi2Shift);
  }

  inline int Ui3Value() const {
    return this->Bits(kUi3Shift + kUi3Bits - 1, kUi3Shift);
  }

  inline int Ui4Value() const {
    return this->Bits(kUi4Shift + kUi4Bits - 1, kUi4Shift);
  }

  inline int Ui5Value() const {
    return this->Bits(kUi5Shift + kUi5Bits - 1, kUi5Shift);
  }

  inline int Ui6Value() const {
    return this->Bits(kUi6Shift + kUi6Bits - 1, kUi6Shift);
  }

  inline int Ui7Value() const {
    return this->Bits(kUi7Shift + kUi7Bits - 1, kUi7Shift);
  }

  inline int Ui8Value() const {
    return this->Bits(kUi8Shift + kUi8Bits - 1, kUi8Shift);
  }

  inline int Ui12Value() const {
    return this->Bits(kUi12Shift + kUi12Bits - 1, kUi12Shift);
  }

  inline int LsbwValue() const {
    return this->Bits(kLsbwShift + kLsbwBits - 1, kLsbwShift);
  }

  inline int MsbwValue() const {
    return this->Bits(kMsbwShift + kMsbwBits - 1, kMsbwShift);
  }

  inline int LsbdValue() const {
    return this->Bits(kLsbdShift + kLsbdBits - 1, kLsbdShift);
  }

  inline int MsbdValue() const {
    return this->Bits(kMsbdShift + kMsbdBits - 1, kMsbdShift);
  }

  inline int CondValue() const {
    return this->Bits(kCondShift + kCondBits - 1, kCondShift);
  }

  inline int Si5Value() const {
    return this->Bits(kSi5Shift + kSi5Bits - 1, kSi5Shift);
  }

  inline int Si8Value() const {
    return this->Bits(kSi8Shift + kSi8Bits - 1, kSi8Shift);
  }

  inline int Si9Value() const {
    return this->Bits(kSi9Shift + kSi9Bits - 1, kSi9Shift);
  }

  inline int Si10Value() const {
    return this->Bits(kSi10Shift + kSi10Bits - 1, kSi10Shift);
  }

  inline int Si11Value() const {
    return this->Bits(kSi11Shift + kSi11Bits - 1, kSi11Shift);
  }

  inline int Si12Value() const {
    return this->Bits(kSi12Shift + kSi12Bits - 1, kSi12Shift);
  }

  inline int Si13Value() const {
    return this->Bits(kSi13Shift + kSi13Bits - 1, kSi13Shift);
  }

  inline int Si14Value() const {
    return this->Bits(kSi14Shift + kSi14Bits - 1, kSi14Shift);
  }

  inline int Si16Value() const {
    return this->Bits(kSi16Shift + kSi16Bits - 1, kSi16Shift);
  }

  inline int Si20Value() const {
    return this->Bits(kSi20Shift + kSi20Bits - 1, kSi20Shift);
  }

  inline int FdValue() const {
    return this->Bits(kFdShift + kFdBits - 1, kFdShift);
  }

  inline int FaValue() const {
    return this->Bits(kFaShift + kFaBits - 1, kFaShift);
  }

  inline int FjValue() const {
    return this->Bits(kFjShift + kFjBits - 1, kFjShift);
  }

  inline int FkValue() const {
    return this->Bits(kFkShift + kFkBits - 1, kFkShift);
  }

  inline int VdValue() const {
    return this->Bits(kVdShift + kVdBits - 1, kVdShift);
  }

  inline int VaValue() const {
    return this->Bits(kVaShift + kVaBits - 1, kVaShift);
  }

  inline int VjValue() const {
    return this->Bits(kVjShift + kVjBits - 1, kVjShift);
  }

  inline int VkValue() const {
    return this->Bits(kVkShift + kVkBits - 1, kVkShift);
  }

  inline int Idx1Value() const {
    return this->Bits(kIdxShift + kIdx1Bits - 1, kIdxShift);
  }

  inline int Idx2Value() const {
    return this->Bits(kIdxShift + kIdx2Bits - 1, kIdxShift);
  }

  inline int Idx3Value() const {
    return this->Bits(kIdxShift + kIdx3Bits - 1, kIdxShift);
  }

  inline int Idx4Value() const {
    return this->Bits(kIdxShift + kIdx4Bits - 1, kIdxShift);
  }

  inline int CjValue() const {
    return this->Bits(kCjShift + kCjBits - 1, kCjShift);
  }

  inline int CdValue() const {
    return this->Bits(kCdShift + kCdBits - 1, kCdShift);
  }

  inline int CaValue() const {
    return this->Bits(kCaShift + kCaBits - 1, kCaShift);
  }

  inline int CodeValue() const {
    return this->Bits(kCodeShift + kCodeBits - 1, kCodeShift);
  }

  inline int Hint5Value() const {
    return this->Bits(kHint5Shift + kHint5Bits - 1, kHint5Shift);
  }

  inline int Hint15Value() const {
    return this->Bits(kHint15Shift + kHint15Bits - 1, kHint15Shift);
  }

  inline int Offs16Value() const {
    return this->Bits(kOffsLowShift + kOffsLowBits - 1, kOffsLowShift);
  }

  inline int Offs21Value() const {
    int low = this->Bits(kOffsLowShift + kOffsLowBits - 1, kOffsLowShift);
    int high =
        this->Bits(kOffs21HighShift + kOffs21HighBits - 1, kOffs21HighShift);
    return ((high << kOffsLowBits) + low);
  }

  inline int Offs26Value() const {
    int low = this->Bits(kOffsLowShift + kOffsLowBits - 1, kOffsLowShift);
    int high =
        this->Bits(kOffs26HighShift + kOffs26HighBits - 1, kOffs26HighShift);
    return ((high << kOffsLowBits) + low);
  }

  inline int RjFieldRaw() const {
    return this->InstructionBits() & kRjFieldMask;
  }

  inline int RkFieldRaw() const {
    return this->InstructionBits() & kRkFieldMask;
  }

  inline int RdFieldRaw() const {
    return this->InstructionBits() & kRdFieldMask;
  }

  inline int32_t ImmValue(int bits) const { return this->Bits(bits - 1, 0); }

  /*TODO*/
  inline int32_t Imm12Value() const { abort(); }

  inline int32_t Imm14Value() const { abort(); }

  inline int32_t Imm16Value() const { abort(); }

  // Say if the instruction is a break.
  bool IsTrap() const;
  // Say if the instruction is a load opcode.
  bool IsLoad() const;
  // Say if the instruction is a store opcode.
  bool IsStore() const;
};

class Instruction : public InstructionGetters<InstructionBase> {
 public:
  // Instructions are read of out a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instruction.
  // Use the At(pc) function to create references to Instruction.
  static Instruction* At(uint8_t* pc) {
    return reinterpret_cast<Instruction*>(pc);
  }

 private:
  // We need to prevent the creation of instances of class Instruction.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instruction);
};

// -----------------------------------------------------------------------------
// LOONG64 assembly various constants.

const int kInvalidStackOffset = -1;

static const int kNegOffset = 0x00008000;

InstructionBase::Type InstructionBase::InstructionType() const {
  InstructionBase::Type kType = kUnsupported;

  // Check for kOp6Type
  switch (Bits(31, 26) << 26) {
    case ADDU16I_D:
    case BEQZ:
    case BNEZ:
    case BCZ:
    case JIRL:
    case B:
    case BL:
    case BEQ:
    case BNE:
    case BLT:
    case BGE:
    case BLTU:
    case BGEU:
      kType = kOp6Type;
      break;
    default:
      kType = kUnsupported;
  }

  if (kType == kUnsupported) {
    // Check for kOp7Type
    switch (Bits(31, 25) << 25) {
      case LU12I_W:
      case LU32I_D:
      case PCADDI:
      case PCALAU12I:
      case PCADDU12I:
      case PCADDU18I:
        kType = kOp7Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp8Type
    switch (Bits(31, 24) << 24) {
      case LDPTR_W:
      case STPTR_W:
      case LDPTR_D:
      case STPTR_D:
      case LL_W:
      case SC_W:
      case LL_D:
      case SC_D:
        kType = kOp8Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp10Type
    switch (Bits(31, 22) << 22) {
      case BSTR_W: {
        // If Bit(21) = 0, then the Opcode is not BSTR_W.
        if (Bit(21) == 0)
          kType = kUnsupported;
        else
          kType = kOp10Type;
        break;
      }
      case BSTRINS_D:
      case BSTRPICK_D:
      case SLTI:
      case SLTUI:
      case ADDI_W:
      case ADDI_D:
      case LU52I_D:
      case ANDI:
      case ORI:
      case XORI:
      case LD_B:
      case LD_H:
      case LD_W:
      case LD_D:
      case ST_B:
      case ST_H:
      case ST_W:
      case ST_D:
      case LD_BU:
      case LD_HU:
      case LD_WU:
      case FLD_S:
      case FST_S:
      case FLD_D:
      case FST_D:
      case VLD:
      case VST:
      case VLDREPL_B:
      case VSTELM_B:
        kType = kOp10Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp11Type
    switch (Bits(31, 21) << 21) {
      case VLDREPL_H:
      case VSTELM_H:
        kType = kOp11Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp12Type
    switch (Bits(31, 20) << 20) {
      case FMADD_S:
      case FMADD_D:
      case FMSUB_S:
      case FMSUB_D:
      case FNMADD_S:
      case FNMADD_D:
      case FNMSUB_S:
      case FNMSUB_D:
      case FCMP_COND_S:
      case FCMP_COND_D:
      case FSEL:
      case VFMADD_S:
      case VFMADD_D:
      case VFMSUB_S:
      case VFMSUB_D:
      case VFNMADD_S:
      case VFNMADD_D:
      case VFNMSUB_S:
      case VFNMSUB_D:
      case VFCMP_COND_S:
      case VFCMP_COND_D:
      case VBITSEL_V:
      case VSHUF_B:
      case VLDREPL_W:
      case VSTELM_W:
        kType = kOp12Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp13Type
    switch (Bits(31, 19) << 19) {
      case VLDREPL_D:
      case VSTELM_D:
        kType = kOp13Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp14Type
    switch (Bits(31, 18) << 18) {
      case ALSL:
      case BYTEPICK_W:
      case BYTEPICK_D:
      case ALSL_D:
      case SLLI:
      case SRLI:
      case SRAI:
      case ROTRI:
      case VEXTRINS_D:
      case VEXTRINS_W:
      case VEXTRINS_H:
      case VEXTRINS_B:
      case VSHUF4I_B:
      case VSHUF4I_H:
      case VSHUF4I_W:
      case VSHUF4I_D:
      case VBITSELI_B:
      case VANDI_B:
      case VORI_B:
      case VXORI_B:
      case VNORI_B:
      case VLDI:
      case VPERMI_W:
        kType = kOp14Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp15Type
    switch (Bits(31, 17) << 17) {
      case VSRLNI_D_Q:
      case VSRLRNI_D_Q:
      case VSSRLNI_D_Q:
      case VSSRLNI_DU_Q:
      case VSSRLRNI_D_Q:
      case VSSRLRNI_DU_Q:
      case VSRANI_D_Q:
      case VSRARNI_D_Q:
      case VSSRANI_D_Q:
      case VSSRANI_DU_Q:
      case VSSRARNI_D_Q:
      case VSSRARNI_DU_Q:
        kType = kOp15Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp16Type
    switch (Bits(31, 16) << 16) {
      case VROTRI_D:
      case VSRLRI_D:
      case VSRARI_D:
      case VBITCLRI_D:
      case VBITSETI_D:
      case VBITREVI_D:
      case VSAT_D:
      case VSAT_DU:
      case VSLLI_D:
      case VSRLI_D:
      case VSRAI_D:
      case VSRLNI_W_D:
      case VSRLRNI_W_D:
      case VSSRLNI_W_D:
      case VSSRLNI_WU_D:
      case VSSRLRNI_W_D:
      case VSSRLRNI_WU_D:
      case VSRANI_W_D:
      case VSRARNI_W_D:
      case VSSRANI_W_D:
      case VSSRANI_WU_D:
      case VSSRARNI_W_D:
      case VSSRARNI_WU_D:
        kType = kOp16Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp17Type
    switch (Bits(31, 15) << 15) {
      case ADD_W:
      case ADD_D:
      case SUB_W:
      case SUB_D:
      case SLT:
      case SLTU:
      case MASKEQZ:
      case MASKNEZ:
      case NOR:
      case AND:
      case OR:
      case XOR:
      case ORN:
      case ANDN:
      case SLL_W:
      case SRL_W:
      case SRA_W:
      case SLL_D:
      case SRL_D:
      case SRA_D:
      case ROTR_D:
      case ROTR_W:
      case MUL_W:
      case MULH_W:
      case MULH_WU:
      case MUL_D:
      case MULH_D:
      case MULH_DU:
      case MULW_D_W:
      case MULW_D_WU:
      case DIV_W:
      case MOD_W:
      case DIV_WU:
      case MOD_WU:
      case DIV_D:
      case MOD_D:
      case DIV_DU:
      case MOD_DU:
      case BREAK:
      case FADD_S:
      case FADD_D:
      case FSUB_S:
      case FSUB_D:
      case FMUL_S:
      case FMUL_D:
      case FDIV_S:
      case FDIV_D:
      case FMAX_S:
      case FMAX_D:
      case FMIN_S:
      case FMIN_D:
      case FMAXA_S:
      case FMAXA_D:
      case FMINA_S:
      case FMINA_D:
      case LDX_B:
      case LDX_H:
      case LDX_W:
      case LDX_D:
      case STX_B:
      case STX_H:
      case STX_W:
      case STX_D:
      case LDX_BU:
      case LDX_HU:
      case LDX_WU:
      case FLDX_S:
      case FLDX_D:
      case FSTX_S:
      case FSTX_D:
      case AMSWAP_W:
      case AMSWAP_D:
      case AMADD_W:
      case AMADD_D:
      case AMAND_W:
      case AMAND_D:
      case AMOR_W:
      case AMOR_D:
      case AMXOR_W:
      case AMXOR_D:
      case AMMAX_W:
      case AMMAX_D:
      case AMMIN_W:
      case AMMIN_D:
      case AMMAX_WU:
      case AMMAX_DU:
      case AMMIN_WU:
      case AMMIN_DU:
      case AMSWAP_DB_W:
      case AMSWAP_DB_D:
      case AMADD_DB_W:
      case AMADD_DB_D:
      case AMAND_DB_W:
      case AMAND_DB_D:
      case AMOR_DB_W:
      case AMOR_DB_D:
      case AMXOR_DB_W:
      case AMXOR_DB_D:
      case AMMAX_DB_W:
      case AMMAX_DB_D:
      case AMMIN_DB_W:
      case AMMIN_DB_D:
      case AMMAX_DB_WU:
      case AMMAX_DB_DU:
      case AMMIN_DB_WU:
      case AMMIN_DB_DU:
      case DBAR:
      case IBAR:
      case FSCALEB_S:
      case FSCALEB_D:
      case FCOPYSIGN_S:
      case FCOPYSIGN_D:
      case VLDX:
      case VSTX:
      case VSEQ_B:
      case VSEQ_H:
      case VSEQ_W:
      case VSEQ_D:
      case VSLE_B:
      case VSLE_H:
      case VSLE_W:
      case VSLE_D:
      case VSLE_BU:
      case VSLE_HU:
      case VSLE_WU:
      case VSLE_DU:
      case VSLT_B:
      case VSLT_H:
      case VSLT_W:
      case VSLT_D:
      case VSLT_BU:
      case VSLT_HU:
      case VSLT_WU:
      case VSLT_DU:
      case VADD_B:
      case VADD_H:
      case VADD_W:
      case VADD_D:
      case VSUB_B:
      case VSUB_H:
      case VSUB_W:
      case VSUB_D:
      case VADDWEV_H_B:
      case VADDWEV_W_H:
      case VADDWEV_D_W:
      case VADDWEV_Q_D:
      case VSUBWEV_H_B:
      case VSUBWEV_W_H:
      case VSUBWEV_D_W:
      case VSUBWEV_Q_D:
      case VADDWOD_H_B:
      case VADDWOD_W_H:
      case VADDWOD_D_W:
      case VADDWOD_Q_D:
      case VSUBWOD_H_B:
      case VSUBWOD_W_H:
      case VSUBWOD_D_W:
      case VSUBWOD_Q_D:
      case VADDWEV_H_BU:
      case VADDWEV_W_HU:
      case VADDWEV_D_WU:
      case VADDWEV_Q_DU:
      case VSUBWEV_H_BU:
      case VSUBWEV_W_HU:
      case VSUBWEV_D_WU:
      case VSUBWEV_Q_DU:
      case VADDWOD_H_BU:
      case VADDWOD_W_HU:
      case VADDWOD_D_WU:
      case VADDWOD_Q_DU:
      case VSUBWOD_H_BU:
      case VSUBWOD_W_HU:
      case VSUBWOD_D_WU:
      case VSUBWOD_Q_DU:
      case VADDWEV_H_BU_B:
      case VADDWEV_W_HU_H:
      case VADDWEV_D_WU_W:
      case VADDWEV_Q_DU_D:
      case VADDWOD_H_BU_B:
      case VADDWOD_W_HU_H:
      case VADDWOD_D_WU_W:
      case VADDWOD_Q_DU_D:
      case VSADD_B:
      case VSADD_H:
      case VSADD_W:
      case VSADD_D:
      case VSSUB_B:
      case VSSUB_H:
      case VSSUB_W:
      case VSSUB_D:
      case VSADD_BU:
      case VSADD_HU:
      case VSADD_WU:
      case VSADD_DU:
      case VSSUB_BU:
      case VSSUB_HU:
      case VSSUB_WU:
      case VSSUB_DU:
      case VHADDW_H_B:
      case VHADDW_W_H:
      case VHADDW_D_W:
      case VHADDW_Q_D:
      case VHSUBW_H_B:
      case VHSUBW_W_H:
      case VHSUBW_D_W:
      case VHSUBW_Q_D:
      case VHADDW_HU_BU:
      case VHADDW_WU_HU:
      case VHADDW_DU_WU:
      case VHADDW_QU_DU:
      case VHSUBW_HU_BU:
      case VHSUBW_WU_HU:
      case VHSUBW_DU_WU:
      case VHSUBW_QU_DU:
      case VADDA_B:
      case VADDA_H:
      case VADDA_W:
      case VADDA_D:
      case VABSD_B:
      case VABSD_H:
      case VABSD_W:
      case VABSD_D:
      case VABSD_BU:
      case VABSD_HU:
      case VABSD_WU:
      case VABSD_DU:
      case VAVG_B:
      case VAVG_H:
      case VAVG_W:
      case VAVG_D:
      case VAVG_BU:
      case VAVG_HU:
      case VAVG_WU:
      case VAVG_DU:
      case VAVGR_B:
      case VAVGR_H:
      case VAVGR_W:
      case VAVGR_D:
      case VAVGR_BU:
      case VAVGR_HU:
      case VAVGR_WU:
      case VAVGR_DU:
      case VMAX_B:
      case VMAX_H:
      case VMAX_W:
      case VMAX_D:
      case VMIN_B:
      case VMIN_H:
      case VMIN_W:
      case VMIN_D:
      case VMAX_BU:
      case VMAX_HU:
      case VMAX_WU:
      case VMAX_DU:
      case VMIN_BU:
      case VMIN_HU:
      case VMIN_WU:
      case VMIN_DU:
      case VMUL_B:
      case VMUL_H:
      case VMUL_W:
      case VMUL_D:
      case VMUH_B:
      case VMUH_H:
      case VMUH_W:
      case VMUH_D:
      case VMUH_BU:
      case VMUH_HU:
      case VMUH_WU:
      case VMUH_DU:
      case VMULWEV_H_B:
      case VMULWEV_W_H:
      case VMULWEV_D_W:
      case VMULWEV_Q_D:
      case VMULWOD_H_B:
      case VMULWOD_W_H:
      case VMULWOD_D_W:
      case VMULWOD_Q_D:
      case VMULWEV_H_BU:
      case VMULWEV_W_HU:
      case VMULWEV_D_WU:
      case VMULWEV_Q_DU:
      case VMULWOD_H_BU:
      case VMULWOD_W_HU:
      case VMULWOD_D_WU:
      case VMULWOD_Q_DU:
      case VMULWEV_H_BU_B:
      case VMULWEV_W_HU_H:
      case VMULWEV_D_WU_W:
      case VMULWEV_Q_DU_D:
      case VMULWOD_H_BU_B:
      case VMULWOD_W_HU_H:
      case VMULWOD_D_WU_W:
      case VMULWOD_Q_DU_D:
      case VMADD_B:
      case VMADD_H:
      case VMADD_W:
      case VMADD_D:
      case VMSUB_B:
      case VMSUB_H:
      case VMSUB_W:
      case VMSUB_D:
      case VMADDWEV_H_B:
      case VMADDWEV_W_H:
      case VMADDWEV_D_W:
      case VMADDWEV_Q_D:
      case VMADDWOD_H_B:
      case VMADDWOD_W_H:
      case VMADDWOD_D_W:
      case VMADDWOD_Q_D:
      case VMADDWEV_H_BU:
      case VMADDWEV_W_HU:
      case VMADDWEV_D_WU:
      case VMADDWEV_Q_DU:
      case VMADDWOD_H_BU:
      case VMADDWOD_W_HU:
      case VMADDWOD_D_WU:
      case VMADDWOD_Q_DU:
      case VMADDWEV_H_BU_B:
      case VMADDWEV_W_HU_H:
      case VMADDWEV_D_WU_W:
      case VMADDWEV_Q_DU_D:
      case VMADDWOD_H_BU_B:
      case VMADDWOD_W_HU_H:
      case VMADDWOD_D_WU_W:
      case VMADDWOD_Q_DU_D:
      case VDIV_B:
      case VDIV_H:
      case VDIV_W:
      case VDIV_D:
      case VMOD_B:
      case VMOD_H:
      case VMOD_W:
      case VMOD_D:
      case VDIV_BU:
      case VDIV_HU:
      case VDIV_WU:
      case VDIV_DU:
      case VMOD_BU:
      case VMOD_HU:
      case VMOD_WU:
      case VMOD_DU:
      case VSLL_B:
      case VSLL_H:
      case VSLL_W:
      case VSLL_D:
      case VSRL_B:
      case VSRL_H:
      case VSRL_W:
      case VSRL_D:
      case VSRA_B:
      case VSRA_H:
      case VSRA_W:
      case VSRA_D:
      case VROTR_B:
      case VROTR_H:
      case VROTR_W:
      case VROTR_D:
      case VSRLR_B:
      case VSRLR_H:
      case VSRLR_W:
      case VSRLR_D:
      case VSRAR_B:
      case VSRAR_H:
      case VSRAR_W:
      case VSRAR_D:
      case VSRLN_B_H:
      case VSRLN_H_W:
      case VSRLN_W_D:
      case VSRAN_B_H:
      case VSRAN_H_W:
      case VSRAN_W_D:
      case VSRLRN_B_H:
      case VSRLRN_H_W:
      case VSRLRN_W_D:
      case VSRARN_B_H:
      case VSRARN_H_W:
      case VSRARN_W_D:
      case VSSRLN_B_H:
      case VSSRLN_H_W:
      case VSSRLN_W_D:
      case VSSRAN_B_H:
      case VSSRAN_H_W:
      case VSSRAN_W_D:
      case VSSRLRN_B_H:
      case VSSRLRN_H_W:
      case VSSRLRN_W_D:
      case VSSRARN_B_H:
      case VSSRARN_H_W:
      case VSSRARN_W_D:
      case VSSRLN_BU_H:
      case VSSRLN_HU_W:
      case VSSRLN_WU_D:
      case VSSRAN_BU_H:
      case VSSRAN_HU_W:
      case VSSRAN_WU_D:
      case VSSRLRN_BU_H:
      case VSSRLRN_HU_W:
      case VSSRLRN_WU_D:
      case VSSRARN_BU_H:
      case VSSRARN_HU_W:
      case VSSRARN_WU_D:
      case VBITCLR_B:
      case VBITCLR_H:
      case VBITCLR_W:
      case VBITCLR_D:
      case VBITSET_B:
      case VBITSET_H:
      case VBITSET_W:
      case VBITSET_D:
      case VBITREV_B:
      case VBITREV_H:
      case VBITREV_W:
      case VBITREV_D:
      case VPACKEV_B:
      case VPACKEV_H:
      case VPACKEV_W:
      case VPACKEV_D:
      case VPACKOD_B:
      case VPACKOD_H:
      case VPACKOD_W:
      case VPACKOD_D:
      case VILVL_B:
      case VILVL_H:
      case VILVL_W:
      case VILVL_D:
      case VILVH_B:
      case VILVH_H:
      case VILVH_W:
      case VILVH_D:
      case VPICKEV_B:
      case VPICKEV_H:
      case VPICKEV_W:
      case VPICKEV_D:
      case VPICKOD_B:
      case VPICKOD_H:
      case VPICKOD_W:
      case VPICKOD_D:
      case VREPLVE_B:
      case VREPLVE_H:
      case VREPLVE_W:
      case VREPLVE_D:
      case VAND_V:
      case VOR_V:
      case VXOR_V:
      case VNOR_V:
      case VANDN_V:
      case VORN_V:
      case VFRSTP_B:
      case VFRSTP_H:
      case VADD_Q:
      case VSUB_Q:
      case VSIGNCOV_B:
      case VSIGNCOV_H:
      case VSIGNCOV_W:
      case VSIGNCOV_D:
      case VFADD_S:
      case VFADD_D:
      case VFSUB_S:
      case VFSUB_D:
      case VFMUL_S:
      case VFMUL_D:
      case VFDIV_S:
      case VFDIV_D:
      case VFMAX_S:
      case VFMAX_D:
      case VFMIN_S:
      case VFMIN_D:
      case VFMAXA_S:
      case VFMAXA_D:
      case VFMINA_S:
      case VFMINA_D:
      case VFCVT_H_S:
      case VFCVT_S_D:
      case VFFINT_S_L:
      case VFTINT_W_D:
      case VFTINTRM_W_D:
      case VFTINTRP_W_D:
      case VFTINTRZ_W_D:
      case VFTINTRNE_W_D:
      case VSHUF_H:
      case VSHUF_W:
      case VSHUF_D:
      case VSEQI_B:
      case VSEQI_H:
      case VSEQI_W:
      case VSEQI_D:
      case VSLEI_B:
      case VSLEI_H:
      case VSLEI_W:
      case VSLEI_D:
      case VSLEI_BU:
      case VSLEI_HU:
      case VSLEI_WU:
      case VSLEI_DU:
      case VSLTI_B:
      case VSLTI_H:
      case VSLTI_W:
      case VSLTI_D:
      case VSLTI_BU:
      case VSLTI_HU:
      case VSLTI_WU:
      case VSLTI_DU:
      case VADDI_BU:
      case VADDI_HU:
      case VADDI_WU:
      case VADDI_DU:
      case VSUBI_BU:
      case VSUBI_HU:
      case VSUBI_WU:
      case VSUBI_DU:
      case VBSLL_V:
      case VBSRL_V:
      case VMAXI_B:
      case VMAXI_H:
      case VMAXI_W:
      case VMAXI_D:
      case VMINI_B:
      case VMINI_H:
      case VMINI_W:
      case VMINI_D:
      case VMAXI_BU:
      case VMAXI_HU:
      case VMAXI_WU:
      case VMAXI_DU:
      case VMINI_BU:
      case VMINI_HU:
      case VMINI_WU:
      case VMINI_DU:
      case VFRSTPI_B:
      case VFRSTPI_H:
      case VROTRI_W:
      case VSRLRI_W:
      case VSRARI_W:
      case VSLLWIL_D_W:
      case VSLLWIL_DU_WU:
      case VBITCLRI_W:
      case VBITSETI_W:
      case VBITREVI_W:
      case VSAT_W:
      case VSAT_WU:
      case VSLLI_W:
      case VSRLI_W:
      case VSRAI_W:
      case VSRLNI_H_W:
      case VSRLRNI_H_W:
      case VSSRLNI_H_W:
      case VSSRLNI_HU_W:
      case VSSRLRNI_H_W:
      case VSSRLRNI_HU_W:
      case VSRANI_H_W:
      case VSRARNI_H_W:
      case VSSRANI_H_W:
      case VSSRANI_HU_W:
      case VSSRARNI_H_W:
      case VSSRARNI_HU_W:
        kType = kOp17Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp18Type
    switch (Bits(31, 14) << 14) {
      case VROTRI_H:
      case VSRLRI_H:
      case VSRARI_H:
      case VINSGR2VR_B:
      case VPICKVE2GR_B:
      case VPICKVE2GR_BU:
      case VREPLVEI_B:
      case VSLLWIL_W_H:
      case VSLLWIL_WU_HU:
      case VBITCLRI_H:
      case VBITSETI_H:
      case VBITREVI_H:
      case VSAT_H:
      case VSAT_HU:
      case VSLLI_H:
      case VSRLI_H:
      case VSRAI_H:
      case VSRLNI_B_H:
      case VSRLRNI_B_H:
      case VSSRLNI_B_H:
      case VSSRLNI_BU_H:
      case VSSRLRNI_B_H:
      case VSSRLRNI_BU_H:
      case VSRANI_B_H:
      case VSRARNI_B_H:
      case VSSRANI_B_H:
      case VSSRANI_BU_H:
      case VSSRARNI_B_H:
      case VSSRARNI_BU_H:
        kType = kOp18Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp19Type
    switch (Bits(31, 13) << 13) {
      case VROTRI_B:
      case VSRLRI_B:
      case VSRARI_B:
      case VINSGR2VR_H:
      case VPICKVE2GR_H:
      case VPICKVE2GR_HU:
      case VREPLVEI_H:
      case VSLLWIL_H_B:
      case VSLLWIL_HU_BU:
      case VBITCLRI_B:
      case VBITSETI_B:
      case VBITREVI_B:
      case VSAT_B:
      case VSAT_BU:
      case VSLLI_B:
      case VSRLI_B:
      case VSRAI_B:
        kType = kOp19Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp20Type
    switch (Bits(31, 12) << 12) {
      case VINSGR2VR_W:
      case VPICKVE2GR_W:
      case VPICKVE2GR_WU:
      case VREPLVEI_W:
        kType = kOp20Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp21Type
    switch (Bits(31, 11) << 11) {
      case VINSGR2VR_D:
      case VPICKVE2GR_D:
      case VPICKVE2GR_DU:
      case VREPLVEI_D:
        kType = kOp21Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  if (kType == kUnsupported) {
    // Check for kOp22Type
    switch (Bits(31, 10) << 10) {
      case CLZ_W:
      case CTZ_W:
      case CLZ_D:
      case CTZ_D:
      case REVB_2H:
      case REVB_4H:
      case REVB_2W:
      case REVB_D:
      case REVH_2W:
      case REVH_D:
      case BITREV_4B:
      case BITREV_8B:
      case BITREV_W:
      case BITREV_D:
      case EXT_W_B:
      case EXT_W_H:
      case FABS_S:
      case FABS_D:
      case FNEG_S:
      case FNEG_D:
      case FSQRT_S:
      case FSQRT_D:
      case FMOV_S:
      case FMOV_D:
      case MOVGR2FR_W:
      case MOVGR2FR_D:
      case MOVGR2FRH_W:
      case MOVFR2GR_S:
      case MOVFR2GR_D:
      case MOVFRH2GR_S:
      case MOVGR2FCSR:
      case MOVFCSR2GR:
      case FCVT_S_D:
      case FCVT_D_S:
      case FTINTRM_W_S:
      case FTINTRM_W_D:
      case FTINTRM_L_S:
      case FTINTRM_L_D:
      case FTINTRP_W_S:
      case FTINTRP_W_D:
      case FTINTRP_L_S:
      case FTINTRP_L_D:
      case FTINTRZ_W_S:
      case FTINTRZ_W_D:
      case FTINTRZ_L_S:
      case FTINTRZ_L_D:
      case FTINTRNE_W_S:
      case FTINTRNE_W_D:
      case FTINTRNE_L_S:
      case FTINTRNE_L_D:
      case FTINT_W_S:
      case FTINT_W_D:
      case FTINT_L_S:
      case FTINT_L_D:
      case FFINT_S_W:
      case FFINT_S_L:
      case FFINT_D_W:
      case FFINT_D_L:
      case FRINT_S:
      case FRINT_D:
      case MOVFR2CF:
      case MOVCF2FR:
      case MOVGR2CF:
      case MOVCF2GR:
      case FRECIP_S:
      case FRECIP_D:
      case FRSQRT_S:
      case FRSQRT_D:
      case FCLASS_S:
      case FCLASS_D:
      case FLOGB_S:
      case FLOGB_D:
      case CLO_W:
      case CTO_W:
      case CLO_D:
      case CTO_D:
      case VCLO_B:
      case VCLO_H:
      case VCLO_W:
      case VCLO_D:
      case VCLZ_B:
      case VCLZ_H:
      case VCLZ_W:
      case VCLZ_D:
      case VPCNT_B:
      case VPCNT_H:
      case VPCNT_W:
      case VPCNT_D:
      case VNEG_B:
      case VNEG_H:
      case VNEG_W:
      case VNEG_D:
      case VMSKLTZ_B:
      case VMSKLTZ_H:
      case VMSKLTZ_W:
      case VMSKLTZ_D:
      case VMSKGEZ_B:
      case VMSKNZ_B:
      case VSETEQZ_V:
      case VSETNEZ_V:
      case VSETANYEQZ_B:
      case VSETANYEQZ_H:
      case VSETANYEQZ_W:
      case VSETANYEQZ_D:
      case VSETALLNEZ_B:
      case VSETALLNEZ_H:
      case VSETALLNEZ_W:
      case VSETALLNEZ_D:
      case VFLOGB_S:
      case VFLOGB_D:
      case VFCLASS_S:
      case VFCLASS_D:
      case VFSQRT_S:
      case VFSQRT_D:
      case VFRECIP_S:
      case VFRECIP_D:
      case VFRSQRT_S:
      case VFRSQRT_D:
      case VFRINT_S:
      case VFRINT_D:
      case VFRINTRM_S:
      case VFRINTRM_D:
      case VFRINTRP_S:
      case VFRINTRP_D:
      case VFRINTRZ_S:
      case VFRINTRZ_D:
      case VFRINTRNE_S:
      case VFRINTRNE_D:
      case VFCVTL_S_H:
      case VFCVTH_S_H:
      case VFCVTL_D_S:
      case VFCVTH_D_S:
      case VFFINT_S_W:
      case VFFINT_S_WU:
      case VFFINT_D_L:
      case VFFINT_D_LU:
      case VFFINTL_D_W:
      case VFFINTH_D_W:
      case VFTINT_W_S:
      case VFTINT_L_D:
      case VFTINTRM_W_S:
      case VFTINTRM_L_D:
      case VFTINTRP_W_S:
      case VFTINTRP_L_D:
      case VFTINTRZ_W_S:
      case VFTINTRZ_L_D:
      case VFTINTRNE_W_S:
      case VFTINTRNE_L_D:
      case VFTINT_WU_S:
      case VFTINT_LU_D:
      case VFTINTRZ_WU_S:
      case VFTINTRZ_LU_D:
      case VFTINTL_L_S:
      case VFTINTH_L_S:
      case VFTINTRML_L_S:
      case VFTINTRMH_L_S:
      case VFTINTRPL_L_S:
      case VFTINTRPH_L_S:
      case VFTINTRZL_L_S:
      case VFTINTRZH_L_S:
      case VFTINTRNEL_L_S:
      case VFTINTRNEH_L_S:
      case VEXTH_H_B:
      case VEXTH_W_H:
      case VEXTH_D_W:
      case VEXTH_Q_D:
      case VEXTH_HU_BU:
      case VEXTH_WU_HU:
      case VEXTH_DU_WU:
      case VEXTH_QU_DU:
      case VREPLGR2VR_B:
      case VREPLGR2VR_H:
      case VREPLGR2VR_W:
      case VREPLGR2VR_D:
      case VEXTL_Q_D:
      case VEXTL_QU_DU:
        kType = kOp22Type;
        break;
      default:
        kType = kUnsupported;
    }
  }

  return kType;
}

// -----------------------------------------------------------------------------
// Instructions.

template <class P>
bool InstructionGetters<P>::IsTrap() const {
  if ((this->Bits(31, 15) << 15) == BREAK) return true;
  return false;
}

template <class P>
bool InstructionGetters<P>::IsLoad() const {
  switch (this->Bits(31, 24) << 24) {
    case LL_W:
    case LL_D:
    case LDPTR_W:
    case LDPTR_D:
      return true;
    default:
      break;
  }
  switch (this->Bits(31, 22) << 22) {
    case LD_B:
    case LD_H:
    case LD_W:
    case LD_D:
    case LD_BU:
    case LD_HU:
    case LD_WU:
    case FLD_S:
    case FLD_D:
    case VLD:
    case VLDREPL_B:
      return true;
    default:
      break;
  }
  if ((this->Bits(31, 21) << 21) == VLDREPL_H) return true;
  if ((this->Bits(31, 20) << 20) == VLDREPL_W) return true;
  if ((this->Bits(31, 19) << 19) == VLDREPL_D) return true;
  switch (this->Bits(31, 15) << 15) {
    case LDX_B:
    case LDX_H:
    case LDX_W:
    case LDX_D:
    case LDX_BU:
    case LDX_HU:
    case LDX_WU:
    case FLDX_S:
    case FLDX_D:
    case VLDX:
    case AMSWAP_W:
    case AMSWAP_D:
    case AMADD_W:
    case AMADD_D:
    case AMAND_W:
    case AMAND_D:
    case AMOR_W:
    case AMOR_D:
    case AMXOR_W:
    case AMXOR_D:
    case AMMAX_W:
    case AMMAX_D:
    case AMMIN_W:
    case AMMIN_D:
    case AMMAX_WU:
    case AMMAX_DU:
    case AMMIN_WU:
    case AMMIN_DU:
    case AMSWAP_DB_W:
    case AMSWAP_DB_D:
    case AMADD_DB_W:
    case AMADD_DB_D:
    case AMAND_DB_W:
    case AMAND_DB_D:
    case AMOR_DB_W:
    case AMOR_DB_D:
    case AMXOR_DB_W:
    case AMXOR_DB_D:
    case AMMAX_DB_W:
    case AMMAX_DB_D:
    case AMMIN_DB_W:
    case AMMIN_DB_D:
    case AMMAX_DB_WU:
    case AMMAX_DB_DU:
    case AMMIN_DB_WU:
    case AMMIN_DB_DU:
      return true;
    default:
      break;
  }
  return false;
}

template <class P>
bool InstructionGetters<P>::IsStore() const {
  switch (this->Bits(31, 24) << 24) {
    case SC_W:
    case SC_D:
    case STPTR_W:
    case STPTR_D:
      return true;
    default:
      break;
  }
  switch (this->Bits(31, 22) << 22) {
    case ST_B:
    case ST_H:
    case ST_W:
    case ST_D:
    case FST_S:
    case FST_D:
    case VST:
    case VSTELM_B:
      return true;
    default:
      break;
  }
  if ((this->Bits(31, 21) << 21) == VSTELM_H) return true;
  if ((this->Bits(31, 20) << 20) == VSTELM_W) return true;
  if ((this->Bits(31, 19) << 19) == VSTELM_D) return true;
  switch (this->Bits(31, 15) << 15) {
    case STX_B:
    case STX_H:
    case STX_W:
    case STX_D:
    case FSTX_S:
    case FSTX_D:
    case VSTX:
      return true;
    default:
      break;
  }
  return false;
}
// The maximum size of the stack restore after a fast API call that pops the
// stack parameters of the call off the stack.
constexpr int kMaxSizeOfMoveAfterFastCall = 4;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_LOONG64_CONSTANTS_LOONG64_H_

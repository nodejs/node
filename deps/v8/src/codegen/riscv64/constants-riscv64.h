// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_RISCV64_CONSTANTS_RISCV64_H_
#define V8_CODEGEN_RISCV64_CONSTANTS_RISCV64_H_

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"

// UNIMPLEMENTED_ macro for RISCV.
#ifdef DEBUG
#define UNIMPLEMENTED_RISCV()                                               \
  v8::internal::PrintF("%s, \tline %d: \tfunction %s  not implemented. \n", \
                       __FILE__, __LINE__, __func__);
#else
#define UNIMPLEMENTED_RISCV()
#endif

#define UNSUPPORTED_RISCV() \
  v8::internal::PrintF("Unsupported instruction %d.\n", __LINE__)

enum Endianness { kLittle, kBig };

#if defined(V8_TARGET_LITTLE_ENDIAN)
static const Endianness kArchEndian = kLittle;
#elif defined(V8_TARGET_BIG_ENDIAN)
static const Endianness kArchEndian = kBig;
#else
#error Unknown endianness
#endif

#if defined(V8_TARGET_LITTLE_ENDIAN)
const uint32_t kLeastSignificantByteInInt32Offset = 0;
const uint32_t kLessSignificantWordInDoublewordOffset = 0;
#elif defined(V8_TARGET_BIG_ENDIAN)
const uint32_t kLeastSignificantByteInInt32Offset = 3;
const uint32_t kLessSignificantWordInDoublewordOffset = 4;
#else
#error Unknown endianness
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>

// Defines constants and accessor classes to assemble, disassemble and
// simulate RISC-V instructions.
//
// See: The RISC-V Instruction Set Manual
//      Volume I: User-Level ISA
// Try https://content.riscv.org/wp-content/uploads/2017/05/riscv-spec-v2.2.pdf.

namespace v8 {
namespace internal {

constexpr size_t kMaxPCRelativeCodeRangeInMB = 4094;

// -----------------------------------------------------------------------------
// Registers and FPURegisters.

// Number of general purpose registers.
const int kNumRegisters = 32;
const int kInvalidRegister = -1;

// Number of registers with pc.
const int kNumSimuRegisters = 33;

// In the simulator, the PC register is simulated as the 34th register.
const int kPCRegister = 34;

// Number coprocessor registers.
const int kNumFPURegisters = 32;
const int kInvalidFPURegister = -1;

// Number vectotr registers
const int kNumVRegisters = 32;
const int kInvalidVRegister = -1;
// 'pref' instruction hints
const int32_t kPrefHintLoad = 0;
const int32_t kPrefHintStore = 1;
const int32_t kPrefHintLoadStreamed = 4;
const int32_t kPrefHintStoreStreamed = 5;
const int32_t kPrefHintLoadRetained = 6;
const int32_t kPrefHintStoreRetained = 7;
const int32_t kPrefHintWritebackInvalidate = 25;
const int32_t kPrefHintPrepareForStore = 30;

// Actual value of root register is offset from the root array's start
// to take advantage of negative displacement values.
// TODO(sigurds): Choose best value.
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

// -----------------------------------------------------------------------------
// Instructions encoding constants.

// On RISCV all instructions are 32 bits, except for RVC.
using Instr = int32_t;
using ShortInstr = int16_t;

// Special Software Interrupt codes when used in the presence of the RISC-V
// simulator.
enum SoftwareInterruptCodes {
  // Transition to C code.
  call_rt_redirected = 0xfffff
};

// On RISC-V Simulator breakpoints can have different codes:
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
// RISCV constants
const int kBaseOpcodeShift = 0;
const int kBaseOpcodeBits = 7;
const int kFunct7Shift = 25;
const int kFunct7Bits = 7;
const int kFunct5Shift = 27;
const int kFunct5Bits = 5;
const int kFunct3Shift = 12;
const int kFunct3Bits = 3;
const int kFunct2Shift = 25;
const int kFunct2Bits = 2;
const int kRs1Shift = 15;
const int kRs1Bits = 5;
const int kVs1Shift = 15;
const int kVs1Bits = 5;
const int kVs2Shift = 20;
const int kVs2Bits = 5;
const int kVdShift = 7;
const int kVdBits = 5;
const int kRs2Shift = 20;
const int kRs2Bits = 5;
const int kRs3Shift = 27;
const int kRs3Bits = 5;
const int kRdShift = 7;
const int kRdBits = 5;
const int kRlShift = 25;
const int kAqShift = 26;
const int kImm12Shift = 20;
const int kImm12Bits = 12;
const int kImm11Shift = 2;
const int kImm11Bits = 11;
const int kShamtShift = 20;
const int kShamtBits = 5;
const int kShamtWShift = 20;
const int kShamtWBits = 6;
const int kArithShiftShift = 30;
const int kImm20Shift = 12;
const int kImm20Bits = 20;
const int kCsrShift = 20;
const int kCsrBits = 12;
const int kMemOrderBits = 4;
const int kPredOrderShift = 24;
const int kSuccOrderShift = 20;
// for C extension
const int kRvcFunct4Shift = 12;
const int kRvcFunct4Bits = 4;
const int kRvcFunct3Shift = 13;
const int kRvcFunct3Bits = 3;
const int kRvcRs1Shift = 7;
const int kRvcRs1Bits = 5;
const int kRvcRs2Shift = 2;
const int kRvcRs2Bits = 5;
const int kRvcRdShift = 7;
const int kRvcRdBits = 5;
const int kRvcRs1sShift = 7;
const int kRvcRs1sBits = 3;
const int kRvcRs2sShift = 2;
const int kRvcRs2sBits = 3;
const int kRvcFunct2Shift = 5;
const int kRvcFunct2BShift = 10;
const int kRvcFunct2Bits = 2;
const int kRvcFunct6Shift = 10;
const int kRvcFunct6Bits = 6;

// for RVV extension
constexpr int kRvvELEN = 64;
constexpr int kRvvVLEN = 128;
constexpr int kRvvSLEN = kRvvVLEN;
const int kRvvFunct6Shift = 26;
const int kRvvFunct6Bits = 6;
const uint32_t kRvvFunct6Mask =
    (((1 << kRvvFunct6Bits) - 1) << kRvvFunct6Shift);

const int kRvvVmBits = 1;
const int kRvvVmShift = 25;
const uint32_t kRvvVmMask = (((1 << kRvvVmBits) - 1) << kRvvVmShift);

const int kRvvVs2Bits = 5;
const int kRvvVs2Shift = 20;
const uint32_t kRvvVs2Mask = (((1 << kRvvVs2Bits) - 1) << kRvvVs2Shift);

const int kRvvVs1Bits = 5;
const int kRvvVs1Shift = 15;
const uint32_t kRvvVs1Mask = (((1 << kRvvVs1Bits) - 1) << kRvvVs1Shift);

const int kRvvRs1Bits = kRvvVs1Bits;
const int kRvvRs1Shift = kRvvVs1Shift;
const uint32_t kRvvRs1Mask = (((1 << kRvvRs1Bits) - 1) << kRvvRs1Shift);

const int kRvvRs2Bits = 5;
const int kRvvRs2Shift = 20;
const uint32_t kRvvRs2Mask = (((1 << kRvvRs2Bits) - 1) << kRvvRs2Shift);

const int kRvvImm5Bits = kRvvVs1Bits;
const int kRvvImm5Shift = kRvvVs1Shift;
const uint32_t kRvvImm5Mask = (((1 << kRvvImm5Bits) - 1) << kRvvImm5Shift);

const int kRvvVdBits = 5;
const int kRvvVdShift = 7;
const uint32_t kRvvVdMask = (((1 << kRvvVdBits) - 1) << kRvvVdShift);

const int kRvvRdBits = kRvvVdBits;
const int kRvvRdShift = kRvvVdShift;
const uint32_t kRvvRdMask = (((1 << kRvvRdBits) - 1) << kRvvRdShift);

const int kRvvZimmBits = 11;
const int kRvvZimmShift = 20;
const uint32_t kRvvZimmMask = (((1 << kRvvZimmBits) - 1) << kRvvZimmShift);

const int kRvvUimmShift = kRvvRs1Shift;
const int kRvvUimmBits = kRvvRs1Bits;
const uint32_t kRvvUimmMask = (((1 << kRvvUimmBits) - 1) << kRvvUimmShift);

const int kRvvWidthBits = 3;
const int kRvvWidthShift = 12;
const uint32_t kRvvWidthMask = (((1 << kRvvWidthBits) - 1) << kRvvWidthShift);

const int kRvvMopBits = 2;
const int kRvvMopShift = 26;
const uint32_t kRvvMopMask = (((1 << kRvvMopBits) - 1) << kRvvMopShift);

const int kRvvMewBits = 1;
const int kRvvMewShift = 28;
const uint32_t kRvvMewMask = (((1 << kRvvMewBits) - 1) << kRvvMewShift);

const int kRvvNfBits = 3;
const int kRvvNfShift = 29;
const uint32_t kRvvNfMask = (((1 << kRvvNfBits) - 1) << kRvvNfShift);

// RISCV Instruction bit masks
const uint32_t kBaseOpcodeMask = ((1 << kBaseOpcodeBits) - 1)
                                 << kBaseOpcodeShift;
const uint32_t kFunct3Mask = ((1 << kFunct3Bits) - 1) << kFunct3Shift;
const uint32_t kFunct5Mask = ((1 << kFunct5Bits) - 1) << kFunct5Shift;
const uint32_t kFunct7Mask = ((1 << kFunct7Bits) - 1) << kFunct7Shift;
const uint32_t kFunct2Mask = 0b11 << kFunct7Shift;
const uint32_t kRTypeMask = kBaseOpcodeMask | kFunct3Mask | kFunct7Mask;
const uint32_t kRATypeMask = kBaseOpcodeMask | kFunct3Mask | kFunct5Mask;
const uint32_t kRFPTypeMask = kBaseOpcodeMask | kFunct7Mask;
const uint32_t kR4TypeMask = kBaseOpcodeMask | kFunct3Mask | kFunct2Mask;
const uint32_t kITypeMask = kBaseOpcodeMask | kFunct3Mask;
const uint32_t kSTypeMask = kBaseOpcodeMask | kFunct3Mask;
const uint32_t kBTypeMask = kBaseOpcodeMask | kFunct3Mask;
const uint32_t kUTypeMask = kBaseOpcodeMask;
const uint32_t kJTypeMask = kBaseOpcodeMask;
const uint32_t kVTypeMask = kRvvFunct6Mask | kFunct3Mask | kBaseOpcodeMask;
const uint32_t kRs1FieldMask = ((1 << kRs1Bits) - 1) << kRs1Shift;
const uint32_t kRs2FieldMask = ((1 << kRs2Bits) - 1) << kRs2Shift;
const uint32_t kRs3FieldMask = ((1 << kRs3Bits) - 1) << kRs3Shift;
const uint32_t kRdFieldMask = ((1 << kRdBits) - 1) << kRdShift;
const uint32_t kBImm12Mask = kFunct7Mask | kRdFieldMask;
const uint32_t kImm20Mask = ((1 << kImm20Bits) - 1) << kImm20Shift;
const uint32_t kImm12Mask = ((1 << kImm12Bits) - 1) << kImm12Shift;
const uint32_t kImm11Mask = ((1 << kImm11Bits) - 1) << kImm11Shift;
const uint32_t kImm31_12Mask = ((1 << 20) - 1) << 12;
const uint32_t kImm19_0Mask = ((1 << 20) - 1);
const uint32_t kRvcOpcodeMask =
    0b11 | (((1 << kRvcFunct3Bits) - 1) << kRvcFunct3Shift);
const uint32_t kRvcFunct3Mask =
    (((1 << kRvcFunct3Bits) - 1) << kRvcFunct3Shift);
const uint32_t kRvcFunct4Mask =
    (((1 << kRvcFunct4Bits) - 1) << kRvcFunct4Shift);
const uint32_t kRvcFunct6Mask =
    (((1 << kRvcFunct6Bits) - 1) << kRvcFunct6Shift);
const uint32_t kRvcFunct2Mask =
    (((1 << kRvcFunct2Bits) - 1) << kRvcFunct2Shift);
const uint32_t kRvcFunct2BMask =
    (((1 << kRvcFunct2Bits) - 1) << kRvcFunct2BShift);
const uint32_t kCRTypeMask = kRvcOpcodeMask | kRvcFunct4Mask;
const uint32_t kCSTypeMask = kRvcOpcodeMask | kRvcFunct6Mask;
const uint32_t kCATypeMask = kRvcOpcodeMask | kRvcFunct6Mask | kRvcFunct2Mask;
const uint32_t kRvcBImm8Mask = (((1 << 5) - 1) << 2) | (((1 << 3) - 1) << 10);

// RISCV CSR related bit mask and shift
const int kFcsrFlagsBits = 5;
const uint32_t kFcsrFlagsMask = (1 << kFcsrFlagsBits) - 1;
const int kFcsrFrmBits = 3;
const int kFcsrFrmShift = kFcsrFlagsBits;
const uint32_t kFcsrFrmMask = ((1 << kFcsrFrmBits) - 1) << kFcsrFrmShift;
const int kFcsrBits = kFcsrFlagsBits + kFcsrFrmBits;
const uint32_t kFcsrMask = kFcsrFlagsMask | kFcsrFrmMask;

const int kNopByte = 0x00000013;
// Original MIPS constants
// TODO(RISCV): to be cleaned up
const int kImm16Shift = 0;
const int kImm16Bits = 16;
const uint32_t kImm16Mask = ((1 << kImm16Bits) - 1) << kImm16Shift;
// end of TODO(RISCV): to be cleaned up

// ----- RISCV Base Opcodes

enum BaseOpcode : uint32_t {};

// ----- RISC-V Opcodes and Function Fields.
enum Opcode : uint32_t {
  LOAD = 0b0000011,      // I form: LB LH LW LBU LHU
  LOAD_FP = 0b0000111,   // I form: FLW FLD FLQ
  MISC_MEM = 0b0001111,  // I special form: FENCE FENCE.I
  OP_IMM = 0b0010011,    // I form: ADDI SLTI SLTIU XORI ORI ANDI SLLI SRLI SARI
  // Note: SLLI/SRLI/SRAI I form first, then func3 001/101 => R type
  AUIPC = 0b0010111,      // U form: AUIPC
  OP_IMM_32 = 0b0011011,  // I form: ADDIW SLLIW SRLIW SRAIW
  // Note:  SRLIW SRAIW I form first, then func3 101 special shift encoding
  STORE = 0b0100011,     // S form: SB SH SW SD
  STORE_FP = 0b0100111,  // S form: FSW FSD FSQ
  AMO = 0b0101111,       // R form: All A instructions
  OP = 0b0110011,      // R: ADD SUB SLL SLT SLTU XOR SRL SRA OR AND and 32M set
  LUI = 0b0110111,     // U form: LUI
  OP_32 = 0b0111011,   // R: ADDW SUBW SLLW SRLW SRAW MULW DIVW DIVUW REMW REMUW
  MADD = 0b1000011,    // R4 type: FMADD.S FMADD.D FMADD.Q
  MSUB = 0b1000111,    // R4 type: FMSUB.S FMSUB.D FMSUB.Q
  NMSUB = 0b1001011,   // R4 type: FNMSUB.S FNMSUB.D FNMSUB.Q
  NMADD = 0b1001111,   // R4 type: FNMADD.S FNMADD.D FNMADD.Q
  OP_FP = 0b1010011,   // R type: Q ext
  BRANCH = 0b1100011,  // B form: BEQ BNE, BLT, BGE, BLTU BGEU
  JALR = 0b1100111,    // I form: JALR
  JAL = 0b1101111,     // J form: JAL
  SYSTEM = 0b1110011,  // I form: ECALL EBREAK Zicsr ext
  // C extension
  C0 = 0b00,
  C1 = 0b01,
  C2 = 0b10,
  FUNCT2_0 = 0b00,
  FUNCT2_1 = 0b01,
  FUNCT2_2 = 0b10,
  FUNCT2_3 = 0b11,

  // Note use RO (RiscV Opcode) prefix
  // RV32I Base Instruction Set
  RO_LUI = LUI,
  RO_AUIPC = AUIPC,
  RO_JAL = JAL,
  RO_JALR = JALR | (0b000 << kFunct3Shift),
  RO_BEQ = BRANCH | (0b000 << kFunct3Shift),
  RO_BNE = BRANCH | (0b001 << kFunct3Shift),
  RO_BLT = BRANCH | (0b100 << kFunct3Shift),
  RO_BGE = BRANCH | (0b101 << kFunct3Shift),
  RO_BLTU = BRANCH | (0b110 << kFunct3Shift),
  RO_BGEU = BRANCH | (0b111 << kFunct3Shift),
  RO_LB = LOAD | (0b000 << kFunct3Shift),
  RO_LH = LOAD | (0b001 << kFunct3Shift),
  RO_LW = LOAD | (0b010 << kFunct3Shift),
  RO_LBU = LOAD | (0b100 << kFunct3Shift),
  RO_LHU = LOAD | (0b101 << kFunct3Shift),
  RO_SB = STORE | (0b000 << kFunct3Shift),
  RO_SH = STORE | (0b001 << kFunct3Shift),
  RO_SW = STORE | (0b010 << kFunct3Shift),
  RO_ADDI = OP_IMM | (0b000 << kFunct3Shift),
  RO_SLTI = OP_IMM | (0b010 << kFunct3Shift),
  RO_SLTIU = OP_IMM | (0b011 << kFunct3Shift),
  RO_XORI = OP_IMM | (0b100 << kFunct3Shift),
  RO_ORI = OP_IMM | (0b110 << kFunct3Shift),
  RO_ANDI = OP_IMM | (0b111 << kFunct3Shift),
  RO_SLLI = OP_IMM | (0b001 << kFunct3Shift),
  RO_SRLI = OP_IMM | (0b101 << kFunct3Shift),
  // RO_SRAI = OP_IMM | (0b101 << kFunct3Shift), // Same as SRLI, use func7
  RO_ADD = OP | (0b000 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SUB = OP | (0b000 << kFunct3Shift) | (0b0100000 << kFunct7Shift),
  RO_SLL = OP | (0b001 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SLT = OP | (0b010 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SLTU = OP | (0b011 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_XOR = OP | (0b100 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRL = OP | (0b101 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRA = OP | (0b101 << kFunct3Shift) | (0b0100000 << kFunct7Shift),
  RO_OR = OP | (0b110 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_AND = OP | (0b111 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_FENCE = MISC_MEM | (0b000 << kFunct3Shift),
  RO_ECALL = SYSTEM | (0b000 << kFunct3Shift),
  // RO_EBREAK = SYSTEM | (0b000 << kFunct3Shift), // Same as ECALL, use imm12

  // RV64I Base Instruction Set (in addition to RV32I)
  RO_LWU = LOAD | (0b110 << kFunct3Shift),
  RO_LD = LOAD | (0b011 << kFunct3Shift),
  RO_SD = STORE | (0b011 << kFunct3Shift),
  RO_ADDIW = OP_IMM_32 | (0b000 << kFunct3Shift),
  RO_SLLIW = OP_IMM_32 | (0b001 << kFunct3Shift),
  RO_SRLIW = OP_IMM_32 | (0b101 << kFunct3Shift),
  // RO_SRAIW = OP_IMM_32 | (0b101 << kFunct3Shift), // Same as SRLIW, use func7
  RO_ADDW = OP_32 | (0b000 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SUBW = OP_32 | (0b000 << kFunct3Shift) | (0b0100000 << kFunct7Shift),
  RO_SLLW = OP_32 | (0b001 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRLW = OP_32 | (0b101 << kFunct3Shift) | (0b0000000 << kFunct7Shift),
  RO_SRAW = OP_32 | (0b101 << kFunct3Shift) | (0b0100000 << kFunct7Shift),

  // RV32/RV64 Zifencei Standard Extension
  RO_FENCE_I = MISC_MEM | (0b001 << kFunct3Shift),

  // RV32/RV64 Zicsr Standard Extension
  RO_CSRRW = SYSTEM | (0b001 << kFunct3Shift),
  RO_CSRRS = SYSTEM | (0b010 << kFunct3Shift),
  RO_CSRRC = SYSTEM | (0b011 << kFunct3Shift),
  RO_CSRRWI = SYSTEM | (0b101 << kFunct3Shift),
  RO_CSRRSI = SYSTEM | (0b110 << kFunct3Shift),
  RO_CSRRCI = SYSTEM | (0b111 << kFunct3Shift),

  // RV32M Standard Extension
  RO_MUL = OP | (0b000 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_MULH = OP | (0b001 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_MULHSU = OP | (0b010 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_MULHU = OP | (0b011 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_DIV = OP | (0b100 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_DIVU = OP | (0b101 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_REM = OP | (0b110 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_REMU = OP | (0b111 << kFunct3Shift) | (0b0000001 << kFunct7Shift),

  // RV64M Standard Extension (in addition to RV32M)
  RO_MULW = OP_32 | (0b000 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_DIVW = OP_32 | (0b100 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_DIVUW = OP_32 | (0b101 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_REMW = OP_32 | (0b110 << kFunct3Shift) | (0b0000001 << kFunct7Shift),
  RO_REMUW = OP_32 | (0b111 << kFunct3Shift) | (0b0000001 << kFunct7Shift),

  // RV32A Standard Extension
  RO_LR_W = AMO | (0b010 << kFunct3Shift) | (0b00010 << kFunct5Shift),
  RO_SC_W = AMO | (0b010 << kFunct3Shift) | (0b00011 << kFunct5Shift),
  RO_AMOSWAP_W = AMO | (0b010 << kFunct3Shift) | (0b00001 << kFunct5Shift),
  RO_AMOADD_W = AMO | (0b010 << kFunct3Shift) | (0b00000 << kFunct5Shift),
  RO_AMOXOR_W = AMO | (0b010 << kFunct3Shift) | (0b00100 << kFunct5Shift),
  RO_AMOAND_W = AMO | (0b010 << kFunct3Shift) | (0b01100 << kFunct5Shift),
  RO_AMOOR_W = AMO | (0b010 << kFunct3Shift) | (0b01000 << kFunct5Shift),
  RO_AMOMIN_W = AMO | (0b010 << kFunct3Shift) | (0b10000 << kFunct5Shift),
  RO_AMOMAX_W = AMO | (0b010 << kFunct3Shift) | (0b10100 << kFunct5Shift),
  RO_AMOMINU_W = AMO | (0b010 << kFunct3Shift) | (0b11000 << kFunct5Shift),
  RO_AMOMAXU_W = AMO | (0b010 << kFunct3Shift) | (0b11100 << kFunct5Shift),

  // RV64A Standard Extension (in addition to RV32A)
  RO_LR_D = AMO | (0b011 << kFunct3Shift) | (0b00010 << kFunct5Shift),
  RO_SC_D = AMO | (0b011 << kFunct3Shift) | (0b00011 << kFunct5Shift),
  RO_AMOSWAP_D = AMO | (0b011 << kFunct3Shift) | (0b00001 << kFunct5Shift),
  RO_AMOADD_D = AMO | (0b011 << kFunct3Shift) | (0b00000 << kFunct5Shift),
  RO_AMOXOR_D = AMO | (0b011 << kFunct3Shift) | (0b00100 << kFunct5Shift),
  RO_AMOAND_D = AMO | (0b011 << kFunct3Shift) | (0b01100 << kFunct5Shift),
  RO_AMOOR_D = AMO | (0b011 << kFunct3Shift) | (0b01000 << kFunct5Shift),
  RO_AMOMIN_D = AMO | (0b011 << kFunct3Shift) | (0b10000 << kFunct5Shift),
  RO_AMOMAX_D = AMO | (0b011 << kFunct3Shift) | (0b10100 << kFunct5Shift),
  RO_AMOMINU_D = AMO | (0b011 << kFunct3Shift) | (0b11000 << kFunct5Shift),
  RO_AMOMAXU_D = AMO | (0b011 << kFunct3Shift) | (0b11100 << kFunct5Shift),

  // RV32F Standard Extension
  RO_FLW = LOAD_FP | (0b010 << kFunct3Shift),
  RO_FSW = STORE_FP | (0b010 << kFunct3Shift),
  RO_FMADD_S = MADD | (0b00 << kFunct2Shift),
  RO_FMSUB_S = MSUB | (0b00 << kFunct2Shift),
  RO_FNMSUB_S = NMSUB | (0b00 << kFunct2Shift),
  RO_FNMADD_S = NMADD | (0b00 << kFunct2Shift),
  RO_FADD_S = OP_FP | (0b0000000 << kFunct7Shift),
  RO_FSUB_S = OP_FP | (0b0000100 << kFunct7Shift),
  RO_FMUL_S = OP_FP | (0b0001000 << kFunct7Shift),
  RO_FDIV_S = OP_FP | (0b0001100 << kFunct7Shift),
  RO_FSQRT_S = OP_FP | (0b0101100 << kFunct7Shift) | (0b00000 << kRs2Shift),
  RO_FSGNJ_S = OP_FP | (0b000 << kFunct3Shift) | (0b0010000 << kFunct7Shift),
  RO_FSGNJN_S = OP_FP | (0b001 << kFunct3Shift) | (0b0010000 << kFunct7Shift),
  RO_FSQNJX_S = OP_FP | (0b010 << kFunct3Shift) | (0b0010000 << kFunct7Shift),
  RO_FMIN_S = OP_FP | (0b000 << kFunct3Shift) | (0b0010100 << kFunct7Shift),
  RO_FMAX_S = OP_FP | (0b001 << kFunct3Shift) | (0b0010100 << kFunct7Shift),
  RO_FCVT_W_S = OP_FP | (0b1100000 << kFunct7Shift) | (0b00000 << kRs2Shift),
  RO_FCVT_WU_S = OP_FP | (0b1100000 << kFunct7Shift) | (0b00001 << kRs2Shift),
  RO_FMV = OP_FP | (0b1110000 << kFunct7Shift) | (0b000 << kFunct3Shift) |
           (0b00000 << kRs2Shift),
  RO_FEQ_S = OP_FP | (0b010 << kFunct3Shift) | (0b1010000 << kFunct7Shift),
  RO_FLT_S = OP_FP | (0b001 << kFunct3Shift) | (0b1010000 << kFunct7Shift),
  RO_FLE_S = OP_FP | (0b000 << kFunct3Shift) | (0b1010000 << kFunct7Shift),
  RO_FCLASS_S = OP_FP | (0b001 << kFunct3Shift) | (0b1110000 << kFunct7Shift),
  RO_FCVT_S_W = OP_FP | (0b1101000 << kFunct7Shift) | (0b00000 << kRs2Shift),
  RO_FCVT_S_WU = OP_FP | (0b1101000 << kFunct7Shift) | (0b00001 << kRs2Shift),
  RO_FMV_W_X = OP_FP | (0b000 << kFunct3Shift) | (0b1111000 << kFunct7Shift),

  // RV64F Standard Extension (in addition to RV32F)
  RO_FCVT_L_S = OP_FP | (0b1100000 << kFunct7Shift) | (0b00010 << kRs2Shift),
  RO_FCVT_LU_S = OP_FP | (0b1100000 << kFunct7Shift) | (0b00011 << kRs2Shift),
  RO_FCVT_S_L = OP_FP | (0b1101000 << kFunct7Shift) | (0b00010 << kRs2Shift),
  RO_FCVT_S_LU = OP_FP | (0b1101000 << kFunct7Shift) | (0b00011 << kRs2Shift),

  // RV32D Standard Extension
  RO_FLD = LOAD_FP | (0b011 << kFunct3Shift),
  RO_FSD = STORE_FP | (0b011 << kFunct3Shift),
  RO_FMADD_D = MADD | (0b01 << kFunct2Shift),
  RO_FMSUB_D = MSUB | (0b01 << kFunct2Shift),
  RO_FNMSUB_D = NMSUB | (0b01 << kFunct2Shift),
  RO_FNMADD_D = NMADD | (0b01 << kFunct2Shift),
  RO_FADD_D = OP_FP | (0b0000001 << kFunct7Shift),
  RO_FSUB_D = OP_FP | (0b0000101 << kFunct7Shift),
  RO_FMUL_D = OP_FP | (0b0001001 << kFunct7Shift),
  RO_FDIV_D = OP_FP | (0b0001101 << kFunct7Shift),
  RO_FSQRT_D = OP_FP | (0b0101101 << kFunct7Shift) | (0b00000 << kRs2Shift),
  RO_FSGNJ_D = OP_FP | (0b000 << kFunct3Shift) | (0b0010001 << kFunct7Shift),
  RO_FSGNJN_D = OP_FP | (0b001 << kFunct3Shift) | (0b0010001 << kFunct7Shift),
  RO_FSQNJX_D = OP_FP | (0b010 << kFunct3Shift) | (0b0010001 << kFunct7Shift),
  RO_FMIN_D = OP_FP | (0b000 << kFunct3Shift) | (0b0010101 << kFunct7Shift),
  RO_FMAX_D = OP_FP | (0b001 << kFunct3Shift) | (0b0010101 << kFunct7Shift),
  RO_FCVT_S_D = OP_FP | (0b0100000 << kFunct7Shift) | (0b00001 << kRs2Shift),
  RO_FCVT_D_S = OP_FP | (0b0100001 << kFunct7Shift) | (0b00000 << kRs2Shift),
  RO_FEQ_D = OP_FP | (0b010 << kFunct3Shift) | (0b1010001 << kFunct7Shift),
  RO_FLT_D = OP_FP | (0b001 << kFunct3Shift) | (0b1010001 << kFunct7Shift),
  RO_FLE_D = OP_FP | (0b000 << kFunct3Shift) | (0b1010001 << kFunct7Shift),
  RO_FCLASS_D = OP_FP | (0b001 << kFunct3Shift) | (0b1110001 << kFunct7Shift) |
                (0b00000 << kRs2Shift),
  RO_FCVT_W_D = OP_FP | (0b1100001 << kFunct7Shift) | (0b00000 << kRs2Shift),
  RO_FCVT_WU_D = OP_FP | (0b1100001 << kFunct7Shift) | (0b00001 << kRs2Shift),
  RO_FCVT_D_W = OP_FP | (0b1101001 << kFunct7Shift) | (0b00000 << kRs2Shift),
  RO_FCVT_D_WU = OP_FP | (0b1101001 << kFunct7Shift) | (0b00001 << kRs2Shift),

  // RV64D Standard Extension (in addition to RV32D)
  RO_FCVT_L_D = OP_FP | (0b1100001 << kFunct7Shift) | (0b00010 << kRs2Shift),
  RO_FCVT_LU_D = OP_FP | (0b1100001 << kFunct7Shift) | (0b00011 << kRs2Shift),
  RO_FMV_X_D = OP_FP | (0b000 << kFunct3Shift) | (0b1110001 << kFunct7Shift) |
               (0b00000 << kRs2Shift),
  RO_FCVT_D_L = OP_FP | (0b1101001 << kFunct7Shift) | (0b00010 << kRs2Shift),
  RO_FCVT_D_LU = OP_FP | (0b1101001 << kFunct7Shift) | (0b00011 << kRs2Shift),
  RO_FMV_D_X = OP_FP | (0b000 << kFunct3Shift) | (0b1111001 << kFunct7Shift) |
               (0b00000 << kRs2Shift),

  // RV64C Standard Extension
  RO_C_ADDI4SPN = C0 | (0b000 << kRvcFunct3Shift),
  RO_C_FLD = C0 | (0b001 << kRvcFunct3Shift),
  RO_C_LW = C0 | (0b010 << kRvcFunct3Shift),
  RO_C_LD = C0 | (0b011 << kRvcFunct3Shift),
  RO_C_FSD = C0 | (0b101 << kRvcFunct3Shift),
  RO_C_SW = C0 | (0b110 << kRvcFunct3Shift),
  RO_C_SD = C0 | (0b111 << kRvcFunct3Shift),
  RO_C_NOP_ADDI = C1 | (0b000 << kRvcFunct3Shift),
  RO_C_ADDIW = C1 | (0b001 << kRvcFunct3Shift),
  RO_C_LI = C1 | (0b010 << kRvcFunct3Shift),
  RO_C_SUB = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_0 << kRvcFunct2Shift),
  RO_C_XOR = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_1 << kRvcFunct2Shift),
  RO_C_OR = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_2 << kRvcFunct2Shift),
  RO_C_AND = C1 | (0b100011 << kRvcFunct6Shift) | (FUNCT2_3 << kRvcFunct2Shift),
  RO_C_SUBW =
      C1 | (0b100111 << kRvcFunct6Shift) | (FUNCT2_0 << kRvcFunct2Shift),
  RO_C_ADDW =
      C1 | (0b100111 << kRvcFunct6Shift) | (FUNCT2_1 << kRvcFunct2Shift),
  RO_C_LUI_ADD = C1 | (0b011 << kRvcFunct3Shift),
  RO_C_MISC_ALU = C1 | (0b100 << kRvcFunct3Shift),
  RO_C_J = C1 | (0b101 << kRvcFunct3Shift),
  RO_C_BEQZ = C1 | (0b110 << kRvcFunct3Shift),
  RO_C_BNEZ = C1 | (0b111 << kRvcFunct3Shift),
  RO_C_SLLI = C2 | (0b000 << kRvcFunct3Shift),
  RO_C_FLDSP = C2 | (0b001 << kRvcFunct3Shift),
  RO_C_LWSP = C2 | (0b010 << kRvcFunct3Shift),
  RO_C_LDSP = C2 | (0b011 << kRvcFunct3Shift),
  RO_C_JR_MV_ADD = C2 | (0b100 << kRvcFunct3Shift),
  RO_C_JR = C2 | (0b1000 << kRvcFunct4Shift),
  RO_C_MV = C2 | (0b1000 << kRvcFunct4Shift),
  RO_C_EBREAK = C2 | (0b1001 << kRvcFunct4Shift),
  RO_C_JALR = C2 | (0b1001 << kRvcFunct4Shift),
  RO_C_ADD = C2 | (0b1001 << kRvcFunct4Shift),
  RO_C_FSDSP = C2 | (0b101 << kRvcFunct3Shift),
  RO_C_SWSP = C2 | (0b110 << kRvcFunct3Shift),
  RO_C_SDSP = C2 | (0b111 << kRvcFunct3Shift),

  // RVV Extension
  OP_V = 0b1010111,
  OP_IVV = OP_V | (0b000 << kFunct3Shift),
  OP_FVV = OP_V | (0b001 << kFunct3Shift),
  OP_MVV = OP_V | (0b010 << kFunct3Shift),
  OP_IVI = OP_V | (0b011 << kFunct3Shift),
  OP_IVX = OP_V | (0b100 << kFunct3Shift),
  OP_FVF = OP_V | (0b101 << kFunct3Shift),
  OP_MVX = OP_V | (0b110 << kFunct3Shift),

  RO_V_VSETVLI = OP_V | (0b111 << kFunct3Shift) | 0b0 << 31,
  RO_V_VSETIVLI = OP_V | (0b111 << kFunct3Shift) | 0b11 << 30,
  RO_V_VSETVL = OP_V | (0b111 << kFunct3Shift) | 0b1 << 31,

  // RVV LOAD/STORE
  RO_V_VL = LOAD_FP | (0b00 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VLS = LOAD_FP | (0b10 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VLX = LOAD_FP | (0b11 << kRvvMopShift) | (0b000 << kRvvNfShift),

  RO_V_VS = STORE_FP | (0b00 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VSS = STORE_FP | (0b10 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VSX = STORE_FP | (0b11 << kRvvMopShift) | (0b000 << kRvvNfShift),
  RO_V_VSU = STORE_FP | (0b01 << kRvvMopShift) | (0b000 << kRvvNfShift),
  // THE kFunct6Shift is mop
  RO_V_VLSEG2 = LOAD_FP | (0b00 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VLSEG3 = LOAD_FP | (0b00 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VLSEG4 = LOAD_FP | (0b00 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VLSEG5 = LOAD_FP | (0b00 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VLSEG6 = LOAD_FP | (0b00 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VLSEG7 = LOAD_FP | (0b00 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VLSEG8 = LOAD_FP | (0b00 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VSSEG2 = STORE_FP | (0b00 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VSSEG3 = STORE_FP | (0b00 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VSSEG4 = STORE_FP | (0b00 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VSSEG5 = STORE_FP | (0b00 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VSSEG6 = STORE_FP | (0b00 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VSSEG7 = STORE_FP | (0b00 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VSSEG8 = STORE_FP | (0b00 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VLSSEG2 = LOAD_FP | (0b10 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VLSSEG3 = LOAD_FP | (0b10 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VLSSEG4 = LOAD_FP | (0b10 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VLSSEG5 = LOAD_FP | (0b10 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VLSSEG6 = LOAD_FP | (0b10 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VLSSEG7 = LOAD_FP | (0b10 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VLSSEG8 = LOAD_FP | (0b10 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VSSSEG2 = STORE_FP | (0b10 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VSSSEG3 = STORE_FP | (0b10 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VSSSEG4 = STORE_FP | (0b10 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VSSSEG5 = STORE_FP | (0b10 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VSSSEG6 = STORE_FP | (0b10 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VSSSEG7 = STORE_FP | (0b10 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VSSSEG8 = STORE_FP | (0b10 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VLXSEG2 = LOAD_FP | (0b11 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VLXSEG3 = LOAD_FP | (0b11 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VLXSEG4 = LOAD_FP | (0b11 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VLXSEG5 = LOAD_FP | (0b11 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VLXSEG6 = LOAD_FP | (0b11 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VLXSEG7 = LOAD_FP | (0b11 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VLXSEG8 = LOAD_FP | (0b11 << kRvvMopShift) | (0b111 << kRvvNfShift),

  RO_V_VSXSEG2 = STORE_FP | (0b11 << kRvvMopShift) | (0b001 << kRvvNfShift),
  RO_V_VSXSEG3 = STORE_FP | (0b11 << kRvvMopShift) | (0b010 << kRvvNfShift),
  RO_V_VSXSEG4 = STORE_FP | (0b11 << kRvvMopShift) | (0b011 << kRvvNfShift),
  RO_V_VSXSEG5 = STORE_FP | (0b11 << kRvvMopShift) | (0b100 << kRvvNfShift),
  RO_V_VSXSEG6 = STORE_FP | (0b11 << kRvvMopShift) | (0b101 << kRvvNfShift),
  RO_V_VSXSEG7 = STORE_FP | (0b11 << kRvvMopShift) | (0b110 << kRvvNfShift),
  RO_V_VSXSEG8 = STORE_FP | (0b11 << kRvvMopShift) | (0b111 << kRvvNfShift),

  // RVV Vector Arithmetic Instruction
  VADD_FUNCT6 = 0b000000,
  RO_V_VADD_VI = OP_IVI | (VADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADD_VV = OP_IVV | (VADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADD_VX = OP_IVX | (VADD_FUNCT6 << kRvvFunct6Shift),

  VSUB_FUNCT6 = 0b000010,
  RO_V_VSUB_VX = OP_IVX | (VSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSUB_VV = OP_IVV | (VSUB_FUNCT6 << kRvvFunct6Shift),

  VDIVU_FUNCT6 = 0b100000,
  RO_V_VDIVU_VX = OP_MVX | (VDIVU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VDIVU_VV = OP_MVV | (VDIVU_FUNCT6 << kRvvFunct6Shift),

  VDIV_FUNCT6 = 0b100001,
  RO_V_VDIV_VX = OP_MVX | (VDIV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VDIV_VV = OP_MVV | (VDIV_FUNCT6 << kRvvFunct6Shift),

  VREMU_FUNCT6 = 0b100010,
  RO_V_VREMU_VX = OP_MVX | (VREMU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VREMU_VV = OP_MVV | (VREMU_FUNCT6 << kRvvFunct6Shift),

  VREM_FUNCT6 = 0b100011,
  RO_V_VREM_VX = OP_MVX | (VREM_FUNCT6 << kRvvFunct6Shift),
  RO_V_VREM_VV = OP_MVV | (VREM_FUNCT6 << kRvvFunct6Shift),

  VMULHU_FUNCT6 = 0b100100,
  RO_V_VMULHU_VX = OP_MVX | (VMULHU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMULHU_VV = OP_MVV | (VMULHU_FUNCT6 << kRvvFunct6Shift),

  VMUL_FUNCT6 = 0b100101,
  RO_V_VMUL_VX = OP_MVX | (VMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMUL_VV = OP_MVV | (VMUL_FUNCT6 << kRvvFunct6Shift),

  VWMUL_FUNCT6 = 0b111011,
  RO_V_VWMUL_VX = OP_MVX | (VWMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWMUL_VV = OP_MVV | (VWMUL_FUNCT6 << kRvvFunct6Shift),

  VWMULU_FUNCT6 = 0b111000,
  RO_V_VWMULU_VX = OP_MVX | (VWMULU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWMULU_VV = OP_MVV | (VWMULU_FUNCT6 << kRvvFunct6Shift),

  VMULHSU_FUNCT6 = 0b100110,
  RO_V_VMULHSU_VX = OP_MVX | (VMULHSU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMULHSU_VV = OP_MVV | (VMULHSU_FUNCT6 << kRvvFunct6Shift),

  VMULH_FUNCT6 = 0b100111,
  RO_V_VMULH_VX = OP_MVX | (VMULH_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMULH_VV = OP_MVV | (VMULH_FUNCT6 << kRvvFunct6Shift),

  VWADD_FUNCT6 = 0b110001,
  RO_V_VWADD_VV = OP_MVV | (VWADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWADD_VX = OP_MVX | (VWADD_FUNCT6 << kRvvFunct6Shift),

  VWADDU_FUNCT6 = 0b110000,
  RO_V_VWADDU_VV = OP_MVV | (VWADDU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWADDU_VX = OP_MVX | (VWADDU_FUNCT6 << kRvvFunct6Shift),

  VWADDUW_FUNCT6 = 0b110101,
  RO_V_VWADDUW_VX = OP_MVX | (VWADDUW_FUNCT6 << kRvvFunct6Shift),
  RO_V_VWADDUW_VV = OP_MVV | (VWADDUW_FUNCT6 << kRvvFunct6Shift),

  VCOMPRESS_FUNCT6 = 0b010111,
  RO_V_VCOMPRESS_VV = OP_MVV | (VCOMPRESS_FUNCT6 << kRvvFunct6Shift),

  VSADDU_FUNCT6 = 0b100000,
  RO_V_VSADDU_VI = OP_IVI | (VSADDU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADDU_VV = OP_IVV | (VSADDU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADDU_VX = OP_IVX | (VSADDU_FUNCT6 << kRvvFunct6Shift),

  VSADD_FUNCT6 = 0b100001,
  RO_V_VSADD_VI = OP_IVI | (VSADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADD_VV = OP_IVV | (VSADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSADD_VX = OP_IVX | (VSADD_FUNCT6 << kRvvFunct6Shift),

  VSSUB_FUNCT6 = 0b100011,
  RO_V_VSSUB_VV = OP_IVV | (VSSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSSUB_VX = OP_IVX | (VSSUB_FUNCT6 << kRvvFunct6Shift),

  VSSUBU_FUNCT6 = 0b100010,
  RO_V_VSSUBU_VV = OP_IVV | (VSSUBU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSSUBU_VX = OP_IVX | (VSSUBU_FUNCT6 << kRvvFunct6Shift),

  VRSUB_FUNCT6 = 0b000011,
  RO_V_VRSUB_VX = OP_IVX | (VRSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRSUB_VI = OP_IVI | (VRSUB_FUNCT6 << kRvvFunct6Shift),

  VMINU_FUNCT6 = 0b000100,
  RO_V_VMINU_VX = OP_IVX | (VMINU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMINU_VV = OP_IVV | (VMINU_FUNCT6 << kRvvFunct6Shift),

  VMIN_FUNCT6 = 0b000101,
  RO_V_VMIN_VX = OP_IVX | (VMIN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMIN_VV = OP_IVV | (VMIN_FUNCT6 << kRvvFunct6Shift),

  VMAXU_FUNCT6 = 0b000110,
  RO_V_VMAXU_VX = OP_IVX | (VMAXU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMAXU_VV = OP_IVV | (VMAXU_FUNCT6 << kRvvFunct6Shift),

  VMAX_FUNCT6 = 0b000111,
  RO_V_VMAX_VX = OP_IVX | (VMAX_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMAX_VV = OP_IVV | (VMAX_FUNCT6 << kRvvFunct6Shift),

  VAND_FUNCT6 = 0b001001,
  RO_V_VAND_VI = OP_IVI | (VAND_FUNCT6 << kRvvFunct6Shift),
  RO_V_VAND_VV = OP_IVV | (VAND_FUNCT6 << kRvvFunct6Shift),
  RO_V_VAND_VX = OP_IVX | (VAND_FUNCT6 << kRvvFunct6Shift),

  VOR_FUNCT6 = 0b001010,
  RO_V_VOR_VI = OP_IVI | (VOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VOR_VV = OP_IVV | (VOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VOR_VX = OP_IVX | (VOR_FUNCT6 << kRvvFunct6Shift),

  VXOR_FUNCT6 = 0b001011,
  RO_V_VXOR_VI = OP_IVI | (VXOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VXOR_VV = OP_IVV | (VXOR_FUNCT6 << kRvvFunct6Shift),
  RO_V_VXOR_VX = OP_IVX | (VXOR_FUNCT6 << kRvvFunct6Shift),

  VRGATHER_FUNCT6 = 0b001100,
  RO_V_VRGATHER_VI = OP_IVI | (VRGATHER_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRGATHER_VV = OP_IVV | (VRGATHER_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRGATHER_VX = OP_IVX | (VRGATHER_FUNCT6 << kRvvFunct6Shift),

  VMV_FUNCT6 = 0b010111,
  RO_V_VMV_VI = OP_IVI | (VMV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMV_VV = OP_IVV | (VMV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMV_VX = OP_IVX | (VMV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMV_VF = OP_FVF | (VMV_FUNCT6 << kRvvFunct6Shift),

  RO_V_VMERGE_VI = RO_V_VMV_VI,
  RO_V_VMERGE_VV = RO_V_VMV_VV,
  RO_V_VMERGE_VX = RO_V_VMV_VX,

  VMSEQ_FUNCT6 = 0b011000,
  RO_V_VMSEQ_VI = OP_IVI | (VMSEQ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSEQ_VV = OP_IVV | (VMSEQ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSEQ_VX = OP_IVX | (VMSEQ_FUNCT6 << kRvvFunct6Shift),

  VMSNE_FUNCT6 = 0b011001,
  RO_V_VMSNE_VI = OP_IVI | (VMSNE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSNE_VV = OP_IVV | (VMSNE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSNE_VX = OP_IVX | (VMSNE_FUNCT6 << kRvvFunct6Shift),

  VMSLTU_FUNCT6 = 0b011010,
  RO_V_VMSLTU_VV = OP_IVV | (VMSLTU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLTU_VX = OP_IVX | (VMSLTU_FUNCT6 << kRvvFunct6Shift),

  VMSLT_FUNCT6 = 0b011011,
  RO_V_VMSLT_VV = OP_IVV | (VMSLT_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLT_VX = OP_IVX | (VMSLT_FUNCT6 << kRvvFunct6Shift),

  VMSLE_FUNCT6 = 0b011101,
  RO_V_VMSLE_VI = OP_IVI | (VMSLE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLE_VV = OP_IVV | (VMSLE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLE_VX = OP_IVX | (VMSLE_FUNCT6 << kRvvFunct6Shift),

  VMSLEU_FUNCT6 = 0b011100,
  RO_V_VMSLEU_VI = OP_IVI | (VMSLEU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLEU_VV = OP_IVV | (VMSLEU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSLEU_VX = OP_IVX | (VMSLEU_FUNCT6 << kRvvFunct6Shift),

  VMSGTU_FUNCT6 = 0b011110,
  RO_V_VMSGTU_VI = OP_IVI | (VMSGTU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSGTU_VX = OP_IVX | (VMSGTU_FUNCT6 << kRvvFunct6Shift),

  VMSGT_FUNCT6 = 0b011111,
  RO_V_VMSGT_VI = OP_IVI | (VMSGT_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMSGT_VX = OP_IVX | (VMSGT_FUNCT6 << kRvvFunct6Shift),

  VSLIDEUP_FUNCT6 = 0b001110,
  RO_V_VSLIDEUP_VI = OP_IVI | (VSLIDEUP_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLIDEUP_VX = OP_IVX | (VSLIDEUP_FUNCT6 << kRvvFunct6Shift),

  VSLIDEDOWN_FUNCT6 = 0b001111,
  RO_V_VSLIDEDOWN_VI = OP_IVI | (VSLIDEDOWN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLIDEDOWN_VX = OP_IVX | (VSLIDEDOWN_FUNCT6 << kRvvFunct6Shift),

  VSRL_FUNCT6 = 0b101000,
  RO_V_VSRL_VI = OP_IVI | (VSRL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRL_VV = OP_IVV | (VSRL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRL_VX = OP_IVX | (VSRL_FUNCT6 << kRvvFunct6Shift),

  VSRA_FUNCT6 = 0b101001,
  RO_V_VSRA_VI = OP_IVI | (VSRA_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRA_VV = OP_IVV | (VSRA_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSRA_VX = OP_IVX | (VSRA_FUNCT6 << kRvvFunct6Shift),

  VSLL_FUNCT6 = 0b100101,
  RO_V_VSLL_VI = OP_IVI | (VSLL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLL_VV = OP_IVV | (VSLL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSLL_VX = OP_IVX | (VSLL_FUNCT6 << kRvvFunct6Shift),

  VSMUL_FUNCT6 = 0b100111,
  RO_V_VSMUL_VV = OP_IVV | (VSMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VSMUL_VX = OP_IVX | (VSMUL_FUNCT6 << kRvvFunct6Shift),

  VADC_FUNCT6 = 0b010000,
  RO_V_VADC_VI = OP_IVI | (VADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADC_VV = OP_IVV | (VADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VADC_VX = OP_IVX | (VADC_FUNCT6 << kRvvFunct6Shift),

  VMADC_FUNCT6 = 0b010001,
  RO_V_VMADC_VI = OP_IVI | (VMADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMADC_VV = OP_IVV | (VMADC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMADC_VX = OP_IVX | (VMADC_FUNCT6 << kRvvFunct6Shift),

  VWXUNARY0_FUNCT6 = 0b010000,
  VRXUNARY0_FUNCT6 = 0b010000,
  VMUNARY0_FUNCT6 = 0b010100,

  RO_V_VWXUNARY0 = OP_MVV | (VWXUNARY0_FUNCT6 << kRvvFunct6Shift),
  RO_V_VRXUNARY0 = OP_MVX | (VRXUNARY0_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMUNARY0 = OP_MVV | (VMUNARY0_FUNCT6 << kRvvFunct6Shift),

  VID_V = 0b10001,

  VXUNARY0_FUNCT6 = 0b010010,
  RO_V_VXUNARY0 = OP_MVV | (VXUNARY0_FUNCT6 << kRvvFunct6Shift),

  VWFUNARY0_FUNCT6 = 0b010000,
  RO_V_VFMV_FS = OP_FVV | (VWFUNARY0_FUNCT6 << kRvvFunct6Shift),

  VRFUNARY0_FUNCT6 = 0b010000,
  RO_V_VFMV_SF = OP_FVF | (VRFUNARY0_FUNCT6 << kRvvFunct6Shift),

  VREDMAXU_FUNCT6 = 0b000110,
  RO_V_VREDMAXU = OP_MVV | (VREDMAXU_FUNCT6 << kRvvFunct6Shift),
  VREDMAX_FUNCT6 = 0b000111,
  RO_V_VREDMAX = OP_MVV | (VREDMAX_FUNCT6 << kRvvFunct6Shift),

  VREDMINU_FUNCT6 = 0b000100,
  RO_V_VREDMINU = OP_MVV | (VREDMINU_FUNCT6 << kRvvFunct6Shift),
  VREDMIN_FUNCT6 = 0b000101,
  RO_V_VREDMIN = OP_MVV | (VREDMIN_FUNCT6 << kRvvFunct6Shift),

  VFUNARY0_FUNCT6 = 0b010010,
  RO_V_VFUNARY0 = OP_FVV | (VFUNARY0_FUNCT6 << kRvvFunct6Shift),
  VFUNARY1_FUNCT6 = 0b010011,
  RO_V_VFUNARY1 = OP_FVV | (VFUNARY1_FUNCT6 << kRvvFunct6Shift),

  VFCVT_XU_F_V = 0b00000,
  VFCVT_X_F_V = 0b00001,
  VFCVT_F_XU_V = 0b00010,
  VFCVT_F_X_V = 0b00011,
  VFWCVT_XU_F_V = 0b01000,
  VFWCVT_X_F_V = 0b01001,
  VFWCVT_F_XU_V = 0b01010,
  VFWCVT_F_X_V = 0b01011,
  VFWCVT_F_F_V = 0b01100,
  VFNCVT_F_F_W = 0b10100,
  VFNCVT_X_F_W = 0b10001,
  VFNCVT_XU_F_W = 0b10000,

  VFCLASS_V = 0b10000,
  VFSQRT_V = 0b00000,
  VFRSQRT7_V = 0b00100,
  VFREC7_V = 0b00101,

  VFADD_FUNCT6 = 0b000000,
  RO_V_VFADD_VV = OP_FVV | (VFADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFADD_VF = OP_FVF | (VFADD_FUNCT6 << kRvvFunct6Shift),

  VFSUB_FUNCT6 = 0b000010,
  RO_V_VFSUB_VV = OP_FVV | (VFSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSUB_VF = OP_FVF | (VFSUB_FUNCT6 << kRvvFunct6Shift),

  VFDIV_FUNCT6 = 0b100000,
  RO_V_VFDIV_VV = OP_FVV | (VFDIV_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFDIV_VF = OP_FVF | (VFDIV_FUNCT6 << kRvvFunct6Shift),

  VFMUL_FUNCT6 = 0b100100,
  RO_V_VFMUL_VV = OP_FVV | (VFMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMUL_VF = OP_FVF | (VFMUL_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Add/Subtract Instructions
  VFWADD_FUNCT6 = 0b110000,
  RO_V_VFWADD_VV = OP_FVV | (VFWADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWADD_VF = OP_FVF | (VFWADD_FUNCT6 << kRvvFunct6Shift),

  VFWSUB_FUNCT6 = 0b110010,
  RO_V_VFWSUB_VV = OP_FVV | (VFWSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWSUB_VF = OP_FVF | (VFWSUB_FUNCT6 << kRvvFunct6Shift),

  VFWADD_W_FUNCT6 = 0b110100,
  RO_V_VFWADD_W_VV = OP_FVV | (VFWADD_W_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWADD_W_VF = OP_FVF | (VFWADD_W_FUNCT6 << kRvvFunct6Shift),

  VFWSUB_W_FUNCT6 = 0b110110,
  RO_V_VFWSUB_W_VV = OP_FVV | (VFWSUB_W_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWSUB_W_VF = OP_FVF | (VFWSUB_W_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Reduction Instructions
  VFWREDUSUM_FUNCT6 = 0b110001,
  RO_V_VFWREDUSUM_VV = OP_FVV | (VFWREDUSUM_FUNCT6 << kRvvFunct6Shift),

  VFWREDOSUM_FUNCT6 = 0b110011,
  RO_V_VFWREDOSUM_VV = OP_FVV | (VFWREDOSUM_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Multiply
  VFWMUL_FUNCT6 = 0b111000,
  RO_V_VFWMUL_VV = OP_FVV | (VFWMUL_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWMUL_VF = OP_FVF | (VFWMUL_FUNCT6 << kRvvFunct6Shift),

  VMFEQ_FUNCT6 = 0b011000,
  RO_V_VMFEQ_VV = OP_FVV | (VMFEQ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFEQ_VF = OP_FVF | (VMFEQ_FUNCT6 << kRvvFunct6Shift),

  VMFNE_FUNCT6 = 0b011100,
  RO_V_VMFNE_VV = OP_FVV | (VMFNE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFNE_VF = OP_FVF | (VMFNE_FUNCT6 << kRvvFunct6Shift),

  VMFLT_FUNCT6 = 0b011011,
  RO_V_VMFLT_VV = OP_FVV | (VMFLT_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFLT_VF = OP_FVF | (VMFLT_FUNCT6 << kRvvFunct6Shift),

  VMFLE_FUNCT6 = 0b011001,
  RO_V_VMFLE_VV = OP_FVV | (VMFLE_FUNCT6 << kRvvFunct6Shift),
  RO_V_VMFLE_VF = OP_FVF | (VMFLE_FUNCT6 << kRvvFunct6Shift),

  VMFGE_FUNCT6 = 0b011111,
  RO_V_VMFGE_VF = OP_FVF | (VMFGE_FUNCT6 << kRvvFunct6Shift),

  VMFGT_FUNCT6 = 0b011101,
  RO_V_VMFGT_VF = OP_FVF | (VMFGT_FUNCT6 << kRvvFunct6Shift),

  VFMAX_FUNCT6 = 0b000110,
  RO_V_VFMAX_VV = OP_FVV | (VFMAX_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMAX_VF = OP_FVF | (VFMAX_FUNCT6 << kRvvFunct6Shift),

  VFREDMAX_FUNCT6 = 0b0001111,
  RO_V_VFREDMAX_VV = OP_FVV | (VFREDMAX_FUNCT6 << kRvvFunct6Shift),

  VFMIN_FUNCT6 = 0b000100,
  RO_V_VFMIN_VV = OP_FVV | (VFMIN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMIN_VF = OP_FVF | (VFMIN_FUNCT6 << kRvvFunct6Shift),

  VFSGNJ_FUNCT6 = 0b001000,
  RO_V_VFSGNJ_VV = OP_FVV | (VFSGNJ_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSGNJ_VF = OP_FVF | (VFSGNJ_FUNCT6 << kRvvFunct6Shift),

  VFSGNJN_FUNCT6 = 0b001001,
  RO_V_VFSGNJN_VV = OP_FVV | (VFSGNJN_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSGNJN_VF = OP_FVF | (VFSGNJN_FUNCT6 << kRvvFunct6Shift),

  VFSGNJX_FUNCT6 = 0b001010,
  RO_V_VFSGNJX_VV = OP_FVV | (VFSGNJX_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFSGNJX_VF = OP_FVF | (VFSGNJX_FUNCT6 << kRvvFunct6Shift),

  VFMADD_FUNCT6 = 0b101000,
  RO_V_VFMADD_VV = OP_FVV | (VFMADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMADD_VF = OP_FVF | (VFMADD_FUNCT6 << kRvvFunct6Shift),

  VFNMADD_FUNCT6 = 0b101001,
  RO_V_VFNMADD_VV = OP_FVV | (VFNMADD_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMADD_VF = OP_FVF | (VFNMADD_FUNCT6 << kRvvFunct6Shift),

  VFMSUB_FUNCT6 = 0b101010,
  RO_V_VFMSUB_VV = OP_FVV | (VFMSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMSUB_VF = OP_FVF | (VFMSUB_FUNCT6 << kRvvFunct6Shift),

  VFNMSUB_FUNCT6 = 0b101011,
  RO_V_VFNMSUB_VV = OP_FVV | (VFNMSUB_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMSUB_VF = OP_FVF | (VFNMSUB_FUNCT6 << kRvvFunct6Shift),

  VFMACC_FUNCT6 = 0b101100,
  RO_V_VFMACC_VV = OP_FVV | (VFMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMACC_VF = OP_FVF | (VFMACC_FUNCT6 << kRvvFunct6Shift),

  VFNMACC_FUNCT6 = 0b101101,
  RO_V_VFNMACC_VV = OP_FVV | (VFNMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMACC_VF = OP_FVF | (VFNMACC_FUNCT6 << kRvvFunct6Shift),

  VFMSAC_FUNCT6 = 0b101110,
  RO_V_VFMSAC_VV = OP_FVV | (VFMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFMSAC_VF = OP_FVF | (VFMSAC_FUNCT6 << kRvvFunct6Shift),

  VFNMSAC_FUNCT6 = 0b101111,
  RO_V_VFNMSAC_VV = OP_FVV | (VFNMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFNMSAC_VF = OP_FVF | (VFNMSAC_FUNCT6 << kRvvFunct6Shift),

  // Vector Widening Floating-Point Fused Multiply-Add Instructions
  VFWMACC_FUNCT6 = 0b111100,
  RO_V_VFWMACC_VV = OP_FVV | (VFWMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWMACC_VF = OP_FVF | (VFWMACC_FUNCT6 << kRvvFunct6Shift),

  VFWNMACC_FUNCT6 = 0b111101,
  RO_V_VFWNMACC_VV = OP_FVV | (VFWNMACC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWNMACC_VF = OP_FVF | (VFWNMACC_FUNCT6 << kRvvFunct6Shift),

  VFWMSAC_FUNCT6 = 0b111110,
  RO_V_VFWMSAC_VV = OP_FVV | (VFWMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWMSAC_VF = OP_FVF | (VFWMSAC_FUNCT6 << kRvvFunct6Shift),

  VFWNMSAC_FUNCT6 = 0b111111,
  RO_V_VFWNMSAC_VV = OP_FVV | (VFWNMSAC_FUNCT6 << kRvvFunct6Shift),
  RO_V_VFWNMSAC_VF = OP_FVF | (VFWNMSAC_FUNCT6 << kRvvFunct6Shift),

  VNCLIP_FUNCT6 = 0b101111,
  RO_V_VNCLIP_WV = OP_IVV | (VNCLIP_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIP_WX = OP_IVX | (VNCLIP_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIP_WI = OP_IVI | (VNCLIP_FUNCT6 << kRvvFunct6Shift),

  VNCLIPU_FUNCT6 = 0b101110,
  RO_V_VNCLIPU_WV = OP_IVV | (VNCLIPU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIPU_WX = OP_IVX | (VNCLIPU_FUNCT6 << kRvvFunct6Shift),
  RO_V_VNCLIPU_WI = OP_IVI | (VNCLIPU_FUNCT6 << kRvvFunct6Shift),
};

// ----- Emulated conditions.
// On RISC-V we use this enum to abstract from conditional branch instructions.
// The 'U' prefix is used to specify unsigned comparisons.
// Opposite conditions must be paired as odd/even numbers
// because 'NegateCondition' function flips LSB to negate condition.
enum Condition {  // Any value < 0 is considered no_condition.
  kNoCondition = -1,
  overflow = 0,
  no_overflow = 1,
  Uless = 2,
  Ugreater_equal = 3,
  Uless_equal = 4,
  Ugreater = 5,
  equal = 6,
  not_equal = 7,  // Unordered or Not Equal.
  less = 8,
  greater_equal = 9,
  less_equal = 10,
  greater = 11,
  cc_always = 12,

  // Aliases.
  eq = equal,
  ne = not_equal,
  ge = greater_equal,
  lt = less,
  gt = greater,
  le = less_equal,
  al = cc_always,
  ult = Uless,
  uge = Ugreater_equal,
  ule = Uless_equal,
  ugt = Ugreater,
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
    default:
      return cc;
  }
}

// ----- Coprocessor conditions.
enum FPUCondition {
  kNoFPUCondition = -1,
  EQ = 0x02,  // Ordered and Equal
  NE = 0x03,  // Unordered or Not Equal
  LT = 0x04,  // Ordered and Less Than
  GE = 0x05,  // Ordered and Greater Than or Equal
  LE = 0x06,  // Ordered and Less Than or Equal
  GT = 0x07,  // Ordered and Greater Than
};

enum CheckForInexactConversion {
  kCheckForInexactConversion,
  kDontCheckForInexactConversion
};

enum class MaxMinKind : int { kMin = 0, kMax = 1 };

// ----------------------------------------------------------------------------
// RISCV flags

enum ControlStatusReg {
  csr_fflags = 0x001,   // Floating-Point Accrued Exceptions (RW)
  csr_frm = 0x002,      // Floating-Point Dynamic Rounding Mode (RW)
  csr_fcsr = 0x003,     // Floating-Point Control and Status Register (RW)
  csr_cycle = 0xc00,    // Cycle counter for RDCYCLE instruction (RO)
  csr_time = 0xc01,     // Timer for RDTIME instruction (RO)
  csr_instret = 0xc02,  // Insns-retired counter for RDINSTRET instruction (RO)
  csr_cycleh = 0xc80,   // Upper 32 bits of cycle, RV32I only (RO)
  csr_timeh = 0xc81,    // Upper 32 bits of time, RV32I only (RO)
  csr_instreth = 0xc82  // Upper 32 bits of instret, RV32I only (RO)
};

enum FFlagsMask {
  kInvalidOperation = 0b10000,  // NV: Invalid
  kDivideByZero = 0b1000,       // DZ:  Divide by Zero
  kOverflow = 0b100,            // OF: Overflow
  kUnderflow = 0b10,            // UF: Underflow
  kInexact = 0b1                // NX:  Inexact
};

enum RoundingMode {
  RNE = 0b000,  // Round to Nearest, ties to Even
  RTZ = 0b001,  // Round towards Zero
  RDN = 0b010,  // Round Down (towards -infinity)
  RUP = 0b011,  // Round Up (towards +infinity)
  RMM = 0b100,  // Round to Nearest, tiest to Max Magnitude
  DYN = 0b111   // In instruction's rm field, selects dynamic rounding mode;
                // In Rounding Mode register, Invalid
};

enum MemoryOdering {
  PSI = 0b1000,  // PI or SI
  PSO = 0b0100,  // PO or SO
  PSR = 0b0010,  // PR or SR
  PSW = 0b0001,  // PW or SW
  PSIORW = PSI | PSO | PSR | PSW
};

const int kFloat32ExponentBias = 127;
const int kFloat32MantissaBits = 23;
const int kFloat32ExponentBits = 8;
const int kFloat64ExponentBias = 1023;
const int kFloat64MantissaBits = 52;
const int kFloat64ExponentBits = 11;

enum FClassFlag {
  kNegativeInfinity = 1,
  kNegativeNormalNumber = 1 << 1,
  kNegativeSubnormalNumber = 1 << 2,
  kNegativeZero = 1 << 3,
  kPositiveZero = 1 << 4,
  kPositiveSubnormalNumber = 1 << 5,
  kPositiveNormalNumber = 1 << 6,
  kPositiveInfinity = 1 << 7,
  kSignalingNaN = 1 << 8,
  kQuietNaN = 1 << 9
};

#define RVV_SEW(V) \
  V(E8)            \
  V(E16)           \
  V(E32)           \
  V(E64)

#define DEFINE_FLAG(name) name,
enum VSew {
  RVV_SEW(DEFINE_FLAG)
#undef DEFINE_FLAG
};

#define RVV_LMUL(V) \
  V(m1)             \
  V(m2)             \
  V(m4)             \
  V(m8)             \
  V(RESERVERD)      \
  V(mf8)            \
  V(mf4)            \
  V(mf2)

enum Vlmul {
#define DEFINE_FLAG(name) name,
  RVV_LMUL(DEFINE_FLAG)
#undef DEFINE_FLAG
};

enum TailAgnosticType {
  ta = 0x1,  // Tail agnostic
  tu = 0x0,  // Tail undisturbed
};

enum MaskAgnosticType {
  ma = 0x1,  // Mask agnostic
  mu = 0x0,  // Mask undisturbed
};
enum MaskType {
  Mask = 0x0,  // use the mask
  NoMask = 0x1,
};

// -----------------------------------------------------------------------------
// Hints.

// Branch hints are not used on RISC-V.  They are defined so that they can
// appear in shared function signatures, but will be ignored in RISC-V
// implementations.
enum Hint { no_hint = 0 };

inline Hint NegateHint(Hint hint) { return no_hint; }

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.
// These constants are declared in assembler-riscv64.cc, as they use named
// registers and other constants.

// An Illegal instruction
const Instr kIllegalInstr = 0;  // All other bits are 0s (i.e., ecall)
// An ECALL instruction, used for redirected real time call
const Instr rtCallRedirInstr = SYSTEM;  // All other bits are 0s (i.e., ecall)
// An EBreak instruction, used for debugging and semi-hosting
const Instr kBreakInstr = SYSTEM | 1 << kImm12Shift;  // ebreak

constexpr uint8_t kInstrSize = 4;
constexpr uint8_t kShortInstrSize = 2;
constexpr uint8_t kInstrSizeLog2 = 2;

class InstructionBase {
 public:
  enum {
    // On RISC-V, PC cannot actually be directly accessed. We behave as if PC
    // was always the value of the current instruction being executed.
    kPCReadOffset = 0
  };

  // Instruction type.
  enum Type {
    kRType,
    kR4Type,  // Special R4 for Q extension
    kIType,
    kSType,
    kBType,
    kUType,
    kJType,
    // C extension
    kCRType,
    kCIType,
    kCSSType,
    kCIWType,
    kCLType,
    kCSType,
    kCAType,
    kCBType,
    kCJType,
    // V extension
    kVType,
    kVLType,
    kVSType,
    kVAMOType,
    kVIVVType,
    kVFVVType,
    kVMVVType,
    kVIVIType,
    kVIVXType,
    kVFVFType,
    kVMVXType,
    kVSETType,
    kUnsupported = -1
  };

  inline bool IsIllegalInstruction() const {
    uint16_t FirstHalfWord = *reinterpret_cast<const uint16_t*>(this);
    return FirstHalfWord == 0;
  }

  inline bool IsShortInstruction() const {
    uint8_t FirstByte = *reinterpret_cast<const uint8_t*>(this);
    return (FirstByte & 0x03) <= C2;
  }

  inline uint8_t InstructionSize() const {
    return (FLAG_riscv_c_extension && this->IsShortInstruction())
               ? kShortInstrSize
               : kInstrSize;
  }

  // Get the raw instruction bits.
  inline Instr InstructionBits() const {
    if (FLAG_riscv_c_extension && this->IsShortInstruction()) {
      return 0x0000FFFF & (*reinterpret_cast<const ShortInstr*>(this));
    }
    return *reinterpret_cast<const Instr*>(this);
  }

  // Set the raw instruction bits to value.
  inline void SetInstructionBits(Instr value) {
    *reinterpret_cast<Instr*>(this) = value;
  }

  // Read one particular bit out of the instruction bits.
  inline int Bit(int nr) const { return (InstructionBits() >> nr) & 1; }

  // Read a bit field out of the instruction bits.
  inline int Bits(int hi, int lo) const {
    return (InstructionBits() >> lo) & ((2U << (hi - lo)) - 1);
  }

  // Accessors for the different named fields used in the RISC-V encoding.
  inline Opcode BaseOpcodeValue() const {
    return static_cast<Opcode>(
        Bits(kBaseOpcodeShift + kBaseOpcodeBits - 1, kBaseOpcodeShift));
  }

  // Return the fields at their original place in the instruction encoding.
  inline Opcode BaseOpcodeFieldRaw() const {
    return static_cast<Opcode>(InstructionBits() & kBaseOpcodeMask);
  }

  // Safe to call within R-type instructions
  inline int Funct7FieldRaw() const { return InstructionBits() & kFunct7Mask; }

  // Safe to call within R-, I-, S-, or B-type instructions
  inline int Funct3FieldRaw() const { return InstructionBits() & kFunct3Mask; }

  // Safe to call within R-, I-, S-, or B-type instructions
  inline int Rs1FieldRawNoAssert() const {
    return InstructionBits() & kRs1FieldMask;
  }

  // Safe to call within R-, S-, or B-type instructions
  inline int Rs2FieldRawNoAssert() const {
    return InstructionBits() & kRs2FieldMask;
  }

  // Safe to call within R4-type instructions
  inline int Rs3FieldRawNoAssert() const {
    return InstructionBits() & kRs3FieldMask;
  }

  inline int32_t ITypeBits() const { return InstructionBits() & kITypeMask; }

  inline int32_t InstructionOpcodeType() const {
    if (IsShortInstruction()) {
      return InstructionBits() & kRvcOpcodeMask;
    } else {
      return InstructionBits() & kBaseOpcodeMask;
    }
  }

  // Get the encoding type of the instruction.
  Type InstructionType() const;

 protected:
  InstructionBase() {}
};

template <class T>
class InstructionGetters : public T {
 public:
  inline int BaseOpcode() const {
    return this->InstructionBits() & kBaseOpcodeMask;
  }

  inline int RvcOpcode() const {
    DCHECK(this->IsShortInstruction());
    return this->InstructionBits() & kRvcOpcodeMask;
  }

  inline int Rs1Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kRType ||
           this->InstructionType() == InstructionBase::kR4Type ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kSType ||
           this->InstructionType() == InstructionBase::kBType ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kVType);
    return this->Bits(kRs1Shift + kRs1Bits - 1, kRs1Shift);
  }

  inline int Rs2Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kRType ||
           this->InstructionType() == InstructionBase::kR4Type ||
           this->InstructionType() == InstructionBase::kSType ||
           this->InstructionType() == InstructionBase::kBType ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kVType);
    return this->Bits(kRs2Shift + kRs2Bits - 1, kRs2Shift);
  }

  inline int Rs3Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kR4Type);
    return this->Bits(kRs3Shift + kRs3Bits - 1, kRs3Shift);
  }

  inline int Vs1Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kVType ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kSType);
    return this->Bits(kVs1Shift + kVs1Bits - 1, kVs1Shift);
  }

  inline int Vs2Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kVType ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kSType);
    return this->Bits(kVs2Shift + kVs2Bits - 1, kVs2Shift);
  }

  inline int VdValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kVType ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kSType);
    return this->Bits(kVdShift + kVdBits - 1, kVdShift);
  }

  inline int RdValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kRType ||
           this->InstructionType() == InstructionBase::kR4Type ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kSType ||
           this->InstructionType() == InstructionBase::kUType ||
           this->InstructionType() == InstructionBase::kJType ||
           this->InstructionType() == InstructionBase::kVType);
    return this->Bits(kRdShift + kRdBits - 1, kRdShift);
  }

  inline int RvcRdValue() const {
    DCHECK(this->IsShortInstruction());
    return this->Bits(kRvcRdShift + kRvcRdBits - 1, kRvcRdShift);
  }

  inline int RvcRs1Value() const { return this->RvcRdValue(); }

  inline int RvcRs2Value() const {
    DCHECK(this->IsShortInstruction());
    return this->Bits(kRvcRs2Shift + kRvcRs2Bits - 1, kRvcRs2Shift);
  }

  inline int RvcRs1sValue() const {
    DCHECK(this->IsShortInstruction());
    return 0b1000 + this->Bits(kRvcRs1sShift + kRvcRs1sBits - 1, kRvcRs1sShift);
  }

  inline int RvcRs2sValue() const {
    DCHECK(this->IsShortInstruction());
    return 0b1000 + this->Bits(kRvcRs2sShift + kRvcRs2sBits - 1, kRvcRs2sShift);
  }

  inline int Funct7Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kRType);
    return this->Bits(kFunct7Shift + kFunct7Bits - 1, kFunct7Shift);
  }

  inline int Funct3Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kRType ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kSType ||
           this->InstructionType() == InstructionBase::kBType);
    return this->Bits(kFunct3Shift + kFunct3Bits - 1, kFunct3Shift);
  }

  inline int Funct5Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kRType &&
           this->BaseOpcode() == OP_FP);
    return this->Bits(kFunct5Shift + kFunct5Bits - 1, kFunct5Shift);
  }

  inline int RvcFunct6Value() const {
    DCHECK(this->IsShortInstruction());
    return this->Bits(kRvcFunct6Shift + kRvcFunct6Bits - 1, kRvcFunct6Shift);
  }

  inline int RvcFunct4Value() const {
    DCHECK(this->IsShortInstruction());
    return this->Bits(kRvcFunct4Shift + kRvcFunct4Bits - 1, kRvcFunct4Shift);
  }

  inline int RvcFunct3Value() const {
    DCHECK(this->IsShortInstruction());
    return this->Bits(kRvcFunct3Shift + kRvcFunct3Bits - 1, kRvcFunct3Shift);
  }

  inline int RvcFunct2Value() const {
    DCHECK(this->IsShortInstruction());
    return this->Bits(kRvcFunct2Shift + kRvcFunct2Bits - 1, kRvcFunct2Shift);
  }

  inline int RvcFunct2BValue() const {
    DCHECK(this->IsShortInstruction());
    return this->Bits(kRvcFunct2BShift + kRvcFunct2Bits - 1, kRvcFunct2BShift);
  }

  inline int CsrValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kIType &&
           this->BaseOpcode() == SYSTEM);
    return (this->Bits(kCsrShift + kCsrBits - 1, kCsrShift));
  }

  inline int RoundMode() const {
    DCHECK((this->InstructionType() == InstructionBase::kRType ||
            this->InstructionType() == InstructionBase::kR4Type) &&
           this->BaseOpcode() == OP_FP);
    return this->Bits(kFunct3Shift + kFunct3Bits - 1, kFunct3Shift);
  }

  inline int MemoryOrder(bool is_pred) const {
    DCHECK((this->InstructionType() == InstructionBase::kIType &&
            this->BaseOpcode() == MISC_MEM));
    if (is_pred) {
      return this->Bits(kPredOrderShift + kMemOrderBits - 1, kPredOrderShift);
    } else {
      return this->Bits(kSuccOrderShift + kMemOrderBits - 1, kSuccOrderShift);
    }
  }

  inline int Imm12Value() const {
    DCHECK(this->InstructionType() == InstructionBase::kIType);
    int Value = this->Bits(kImm12Shift + kImm12Bits - 1, kImm12Shift);
    return Value << 20 >> 20;
  }

  inline int32_t Imm12SExtValue() const {
    int32_t Value = this->Imm12Value() << 20 >> 20;
    return Value;
  }

  inline int BranchOffset() const {
    DCHECK(this->InstructionType() == InstructionBase::kBType);
    // | imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode |
    //  31          25                      11          7
    uint32_t Bits = this->InstructionBits();
    int16_t imm13 = ((Bits & 0xf00) >> 7) | ((Bits & 0x7e000000) >> 20) |
                    ((Bits & 0x80) << 4) | ((Bits & 0x80000000) >> 19);
    return imm13 << 19 >> 19;
  }

  inline int StoreOffset() const {
    DCHECK(this->InstructionType() == InstructionBase::kSType);
    // | imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | opcode |
    //  31       25                      11       7
    uint32_t Bits = this->InstructionBits();
    int16_t imm12 = ((Bits & 0xf80) >> 7) | ((Bits & 0xfe000000) >> 20);
    return imm12 << 20 >> 20;
  }

  inline int Imm20UValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kUType);
    // | imm[31:12] | rd | opcode |
    //  31        12
    int32_t Bits = this->InstructionBits();
    return Bits >> 12;
  }

  inline int Imm20JValue() const {
    DCHECK(this->InstructionType() == InstructionBase::kJType);
    // | imm[20|10:1|11|19:12] | rd | opcode |
    //  31                   12
    uint32_t Bits = this->InstructionBits();
    int32_t imm20 = ((Bits & 0x7fe00000) >> 20) | ((Bits & 0x100000) >> 9) |
                    (Bits & 0xff000) | ((Bits & 0x80000000) >> 11);
    return imm20 << 11 >> 11;
  }

  inline bool IsArithShift() const {
    // Valid only for right shift operations
    DCHECK((this->BaseOpcode() == OP || this->BaseOpcode() == OP_32 ||
            this->BaseOpcode() == OP_IMM || this->BaseOpcode() == OP_IMM_32) &&
           this->Funct3Value() == 0b101);
    return this->InstructionBits() & 0x40000000;
  }

  inline int Shamt() const {
    // Valid only for shift instructions (SLLI, SRLI, SRAI)
    DCHECK((this->InstructionBits() & kBaseOpcodeMask) == OP_IMM &&
           (this->Funct3Value() == 0b001 || this->Funct3Value() == 0b101));
    // | 0A0000 | shamt | rs1 | funct3 | rd | opcode |
    //  31       25    20
    return this->Bits(kImm12Shift + 5, kImm12Shift);
  }

  inline int Shamt32() const {
    // Valid only for shift instructions (SLLIW, SRLIW, SRAIW)
    DCHECK((this->InstructionBits() & kBaseOpcodeMask) == OP_IMM_32 &&
           (this->Funct3Value() == 0b001 || this->Funct3Value() == 0b101));
    // | 0A00000 | shamt | rs1 | funct3 | rd | opcode |
    //  31        24   20
    return this->Bits(kImm12Shift + 4, kImm12Shift);
  }

  inline int RvcImm6Value() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | imm[5] | rs1/rd | imm[4:0] | opcode |
    //  15         12              6        2
    uint32_t Bits = this->InstructionBits();
    int32_t imm6 = ((Bits & 0x1000) >> 7) | ((Bits & 0x7c) >> 2);
    return imm6 << 26 >> 26;
  }

  inline int RvcImm6Addi16spValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | nzimm[9] | 2 | nzimm[4|6|8:7|5] | opcode |
    //  15         12           6                2
    uint32_t Bits = this->InstructionBits();
    int32_t imm10 = ((Bits & 0x1000) >> 3) | ((Bits & 0x40) >> 2) |
                    ((Bits & 0x20) << 1) | ((Bits & 0x18) << 4) |
                    ((Bits & 0x4) << 3);
    DCHECK_NE(imm10, 0);
    return imm10 << 22 >> 22;
  }

  inline int RvcImm8Addi4spnValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | nzimm[11]  | rd' | opcode |
    //  15      13           5     2
    uint32_t Bits = this->InstructionBits();
    int32_t uimm10 = ((Bits & 0x20) >> 2) | ((Bits & 0x40) >> 4) |
                     ((Bits & 0x780) >> 1) | ((Bits & 0x1800) >> 7);
    DCHECK_NE(uimm10, 0);
    return uimm10;
  }

  inline int RvcShamt6() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | nzuimm[5] | rs1/rd | nzuimm[4:0] | opcode |
    //  15         12                 6           2
    int32_t imm6 = this->RvcImm6Value();
    return imm6 & 0x3f;
  }

  inline int RvcImm6LwspValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | uimm[5] | rs1 | uimm[4:2|7:6] | opcode |
    //  15         12            6             2
    uint32_t Bits = this->InstructionBits();
    int32_t imm8 =
        ((Bits & 0x1000) >> 7) | ((Bits & 0x70) >> 2) | ((Bits & 0xc) << 4);
    return imm8;
  }

  inline int RvcImm6LdspValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | uimm[5] | rs1 | uimm[4:3|8:6] | opcode |
    //  15         12            6             2
    uint32_t Bits = this->InstructionBits();
    int32_t imm9 =
        ((Bits & 0x1000) >> 7) | ((Bits & 0x60) >> 2) | ((Bits & 0x1c) << 4);
    return imm9;
  }

  inline int RvcImm6SwspValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | uimm[5:2|7:6] | rs2 | opcode |
    //  15       12            7
    uint32_t Bits = this->InstructionBits();
    int32_t imm8 = ((Bits & 0x1e00) >> 7) | ((Bits & 0x180) >> 1);
    return imm8;
  }

  inline int RvcImm6SdspValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | uimm[5:3|8:6] | rs2 | opcode |
    //  15       12            7
    uint32_t Bits = this->InstructionBits();
    int32_t imm9 = ((Bits & 0x1c00) >> 7) | ((Bits & 0x380) >> 1);
    return imm9;
  }

  inline int RvcImm5WValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | imm[5:3] | rs1 | imm[2|6] | rd | opcode |
    //  15       12       10     6          4     2
    uint32_t Bits = this->InstructionBits();
    int32_t imm7 =
        ((Bits & 0x1c00) >> 7) | ((Bits & 0x40) >> 4) | ((Bits & 0x20) << 1);
    return imm7;
  }

  inline int RvcImm5DValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | imm[5:3] | rs1 | imm[7:6] | rd | opcode |
    //  15       12        10    6          4     2
    uint32_t Bits = this->InstructionBits();
    int32_t imm8 = ((Bits & 0x1c00) >> 7) | ((Bits & 0x60) << 1);
    return imm8;
  }

  inline int RvcImm11CJValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | [11|4|9:8|10|6|7|3:1|5] | opcode |
    //  15      12                        2
    uint32_t Bits = this->InstructionBits();
    int32_t imm12 = ((Bits & 0x4) << 3) | ((Bits & 0x38) >> 2) |
                    ((Bits & 0x40) << 1) | ((Bits & 0x80) >> 1) |
                    ((Bits & 0x100) << 2) | ((Bits & 0x600) >> 1) |
                    ((Bits & 0x800) >> 7) | ((Bits & 0x1000) >> 1);
    return imm12 << 20 >> 20;
  }

  inline int RvcImm8BValue() const {
    DCHECK(this->IsShortInstruction());
    // | funct3 | imm[8|4:3] | rs1` | imm[7:6|2:1|5]  | opcode |
    //  15       12        10       7                 2
    uint32_t Bits = this->InstructionBits();
    int32_t imm9 = ((Bits & 0x4) << 3) | ((Bits & 0x18) >> 2) |
                   ((Bits & 0x60) << 1) | ((Bits & 0xc00) >> 7) |
                   ((Bits & 0x1000) >> 4);
    return imm9 << 23 >> 23;
  }

  inline int vl_vs_width() {
    int width = 0;
    if ((this->InstructionBits() & kBaseOpcodeMask) != LOAD_FP &&
        (this->InstructionBits() & kBaseOpcodeMask) != STORE_FP)
      return -1;
    switch (this->InstructionBits() & (kRvvWidthMask | kRvvMewMask)) {
      case 0x0:
        width = 8;
        break;
      case 0x00005000:
        width = 16;
        break;
      case 0x00006000:
        width = 32;
        break;
      case 0x00007000:
        width = 64;
        break;
      case 0x10000000:
        width = 128;
        break;
      case 0x10005000:
        width = 256;
        break;
      case 0x10006000:
        width = 512;
        break;
      case 0x10007000:
        width = 1024;
        break;
      default:
        width = -1;
        break;
    }
    return width;
  }

  inline uint32_t Rvvzimm() const {
    if ((this->InstructionBits() &
         (kBaseOpcodeMask | kFunct3Mask | 0x80000000)) == RO_V_VSETVLI) {
      uint32_t Bits = this->InstructionBits();
      uint32_t zimm = Bits & kRvvZimmMask;
      return zimm >> kRvvZimmShift;
    } else {
      DCHECK_EQ(this->InstructionBits() &
                    (kBaseOpcodeMask | kFunct3Mask | 0xC0000000),
                RO_V_VSETIVLI);
      uint32_t Bits = this->InstructionBits();
      uint32_t zimm = Bits & kRvvZimmMask;
      return (zimm >> kRvvZimmShift) & 0x3FF;
    }
  }

  inline uint32_t Rvvuimm() const {
    DCHECK_EQ(
        this->InstructionBits() & (kBaseOpcodeMask | kFunct3Mask | 0xC0000000),
        RO_V_VSETIVLI);
    uint32_t Bits = this->InstructionBits();
    uint32_t uimm = Bits & kRvvUimmMask;
    return uimm >> kRvvUimmShift;
  }

  inline uint32_t RvvVsew() const {
    uint32_t zimm = this->Rvvzimm();
    uint32_t vsew = (zimm >> 3) & 0x7;
    return vsew;
  }

  inline uint32_t RvvVlmul() const {
    uint32_t zimm = this->Rvvzimm();
    uint32_t vlmul = zimm & 0x7;
    return vlmul;
  }

  inline uint8_t RvvVM() const {
    DCHECK(this->InstructionType() == InstructionBase::kVType ||
           this->InstructionType() == InstructionBase::kIType ||
           this->InstructionType() == InstructionBase::kSType);
    return this->Bits(kRvvVmShift + kRvvVmBits - 1, kRvvVmShift);
  }

  inline const char* RvvSEW() const {
    uint32_t vsew = this->RvvVsew();
    switch (vsew) {
#define CAST_VSEW(name) \
  case name:            \
    return #name;
      RVV_SEW(CAST_VSEW)
      default:
        return "unknown";
#undef CAST_VSEW
    }
  }

  inline const char* RvvLMUL() const {
    uint32_t vlmul = this->RvvVlmul();
    switch (vlmul) {
#define CAST_VLMUL(name) \
  case name:             \
    return #name;
      RVV_LMUL(CAST_VLMUL)
      default:
        return "unknown";
#undef CAST_VLMUL
    }
  }

#define sext(x, len) (((int32_t)(x) << (32 - len)) >> (32 - len))
#define zext(x, len) (((uint32_t)(x) << (32 - len)) >> (32 - len))

  inline int32_t RvvSimm5() const {
    DCHECK(this->InstructionType() == InstructionBase::kVType);
    return sext(this->Bits(kRvvImm5Shift + kRvvImm5Bits - 1, kRvvImm5Shift),
                kRvvImm5Bits);
  }

  inline uint32_t RvvUimm5() const {
    DCHECK(this->InstructionType() == InstructionBase::kVType);
    uint32_t imm = this->Bits(kRvvImm5Shift + kRvvImm5Bits - 1, kRvvImm5Shift);
    return zext(imm, kRvvImm5Bits);
  }
#undef sext
#undef zext
  inline bool AqValue() const { return this->Bits(kAqShift, kAqShift); }

  inline bool RlValue() const { return this->Bits(kRlShift, kRlShift); }

  // Say if the instruction is a break or a trap.
  bool IsTrap() const;
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
// RISC-V assembly various constants.

// C/C++ argument slots size.
const int kCArgSlotCount = 0;

// TODO(plind): below should be based on kSystemPointerSize
// TODO(plind): find all usages and remove the needless instructions for n64.
const int kCArgsSlotsSize = kCArgSlotCount * kInstrSize * 2;

const int kInvalidStackOffset = -1;
const int kBranchReturnOffset = 2 * kInstrSize;

static const int kNegOffset = 0x00008000;

// -----------------------------------------------------------------------------
// Instructions.

template <class P>
bool InstructionGetters<P>::IsTrap() const {
  return (this->InstructionBits() == kBreakInstr);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_RISCV64_CONSTANTS_RISCV64_H_

// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef  V8_MIPS_CONSTANTS_H_
#define  V8_MIPS_CONSTANTS_H_

#include "checks.h"

// UNIMPLEMENTED_ macro for MIPS.
#define UNIMPLEMENTED_MIPS()                                                  \
  v8::internal::PrintF("%s, \tline %d: \tfunction %s not implemented. \n",    \
                       __FILE__, __LINE__, __func__)
#define UNSUPPORTED_MIPS() v8::internal::PrintF("Unsupported instruction.\n")


// Defines constants and accessor classes to assemble, disassemble and
// simulate MIPS32 instructions.
//
// See: MIPS32 Architecture For Programmers
//      Volume II: The MIPS32 Instruction Set
// Try www.cs.cornell.edu/courses/cs3410/2008fa/MIPS_Vol2.pdf.

namespace assembler {
namespace mips {

// -----------------------------------------------------------------------------
// Registers and FPURegister.

// Number of general purpose registers.
static const int kNumRegisters = 32;
static const int kInvalidRegister = -1;

// Number of registers with HI, LO, and pc.
static const int kNumSimuRegisters = 35;

// In the simulator, the PC register is simulated as the 34th register.
static const int kPCRegister = 34;

// Number coprocessor registers.
static const int kNumFPURegister = 32;
static const int kInvalidFPURegister = -1;

// Helper functions for converting between register numbers and names.
class Registers {
 public:
  // Return the name of the register.
  static const char* Name(int reg);

  // Lookup the register number for the name provided.
  static int Number(const char* name);

  struct RegisterAlias {
    int reg;
    const char *name;
  };

  static const int32_t kMaxValue = 0x7fffffff;
  static const int32_t kMinValue = 0x80000000;

 private:

  static const char* names_[kNumSimuRegisters];
  static const RegisterAlias aliases_[];
};

// Helper functions for converting between register numbers and names.
class FPURegister {
 public:
  // Return the name of the register.
  static const char* Name(int reg);

  // Lookup the register number for the name provided.
  static int Number(const char* name);

  struct RegisterAlias {
    int creg;
    const char *name;
  };

 private:

  static const char* names_[kNumFPURegister];
  static const RegisterAlias aliases_[];
};


// -----------------------------------------------------------------------------
// Instructions encoding constants.

// On MIPS all instructions are 32 bits.
typedef int32_t Instr;

typedef unsigned char byte_;

// Special Software Interrupt codes when used in the presence of the MIPS
// simulator.
enum SoftwareInterruptCodes {
  // Transition to C code.
  call_rt_redirected = 0xfffff
};

// ----- Fields offset and length.
static const int kOpcodeShift   = 26;
static const int kOpcodeBits    = 6;
static const int kRsShift       = 21;
static const int kRsBits        = 5;
static const int kRtShift       = 16;
static const int kRtBits        = 5;
static const int kRdShift       = 11;
static const int kRdBits        = 5;
static const int kSaShift       = 6;
static const int kSaBits        = 5;
static const int kFunctionShift = 0;
static const int kFunctionBits  = 6;

static const int kImm16Shift = 0;
static const int kImm16Bits  = 16;
static const int kImm26Shift = 0;
static const int kImm26Bits  = 26;

static const int kFsShift       = 11;
static const int kFsBits        = 5;
static const int kFtShift       = 16;
static const int kFtBits        = 5;

// ----- Miscellianous useful masks.
// Instruction bit masks.
static const int  kOpcodeMask   = ((1 << kOpcodeBits) - 1) << kOpcodeShift;
static const int  kImm16Mask    = ((1 << kImm16Bits) - 1) << kImm16Shift;
static const int  kImm26Mask    = ((1 << kImm26Bits) - 1) << kImm26Shift;
static const int  kRsFieldMask  = ((1 << kRsBits) - 1) << kRsShift;
static const int  kRtFieldMask  = ((1 << kRtBits) - 1) << kRtShift;
static const int  kRdFieldMask  = ((1 << kRdBits) - 1) << kRdShift;
static const int  kSaFieldMask  = ((1 << kSaBits) - 1) << kSaShift;
static const int  kFunctionFieldMask =
    ((1 << kFunctionBits) - 1) << kFunctionShift;
// Misc masks.
static const int  HIMask        =   0xffff << 16;
static const int  LOMask        =   0xffff;
static const int  signMask      =   0x80000000;


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
  LUI       =   ((1 << 3) + 7) << kOpcodeShift,

  COP1      =   ((2 << 3) + 1) << kOpcodeShift,  // Coprocessor 1 class
  BEQL      =   ((2 << 3) + 4) << kOpcodeShift,
  BNEL      =   ((2 << 3) + 5) << kOpcodeShift,
  BLEZL     =   ((2 << 3) + 6) << kOpcodeShift,
  BGTZL     =   ((2 << 3) + 7) << kOpcodeShift,

  SPECIAL2  =   ((3 << 3) + 4) << kOpcodeShift,

  LB        =   ((4 << 3) + 0) << kOpcodeShift,
  LW        =   ((4 << 3) + 3) << kOpcodeShift,
  LBU       =   ((4 << 3) + 4) << kOpcodeShift,
  SB        =   ((5 << 3) + 0) << kOpcodeShift,
  SW        =   ((5 << 3) + 3) << kOpcodeShift,

  LWC1      =   ((6 << 3) + 1) << kOpcodeShift,
  LDC1      =   ((6 << 3) + 5) << kOpcodeShift,

  SWC1      =   ((7 << 3) + 1) << kOpcodeShift,
  SDC1      =   ((7 << 3) + 5) << kOpcodeShift
};

enum SecondaryField {
  // SPECIAL Encoding of Function Field.
  SLL       =   ((0 << 3) + 0),
  SRL       =   ((0 << 3) + 2),
  SRA       =   ((0 << 3) + 3),
  SLLV      =   ((0 << 3) + 4),
  SRLV      =   ((0 << 3) + 6),
  SRAV      =   ((0 << 3) + 7),

  JR        =   ((1 << 3) + 0),
  JALR      =   ((1 << 3) + 1),
  BREAK     =   ((1 << 3) + 5),

  MFHI      =   ((2 << 3) + 0),
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
  TNE       =   ((6 << 3) + 6),

  // SPECIAL2 Encoding of Function Field.
  MUL       =   ((0 << 3) + 2),

  // REGIMM  encoding of rt Field.
  BLTZ      =   ((0 << 3) + 0) << 16,
  BGEZ      =   ((0 << 3) + 1) << 16,
  BLTZAL    =   ((2 << 3) + 0) << 16,
  BGEZAL    =   ((2 << 3) + 1) << 16,

  // COP1 Encoding of rs Field.
  MFC1      =   ((0 << 3) + 0) << 21,
  MFHC1     =   ((0 << 3) + 3) << 21,
  MTC1      =   ((0 << 3) + 4) << 21,
  MTHC1     =   ((0 << 3) + 7) << 21,
  BC1       =   ((1 << 3) + 0) << 21,
  S         =   ((2 << 3) + 0) << 21,
  D         =   ((2 << 3) + 1) << 21,
  W         =   ((2 << 3) + 4) << 21,
  L         =   ((2 << 3) + 5) << 21,
  PS        =   ((2 << 3) + 6) << 21,
  // COP1 Encoding of Function Field When rs=S.
  CVT_D_S   =   ((4 << 3) + 1),
  CVT_W_S   =   ((4 << 3) + 4),
  CVT_L_S   =   ((4 << 3) + 5),
  CVT_PS_S  =   ((4 << 3) + 6),
  // COP1 Encoding of Function Field When rs=D.
  CVT_S_D   =   ((4 << 3) + 0),
  CVT_W_D   =   ((4 << 3) + 4),
  CVT_L_D   =   ((4 << 3) + 5),
  // COP1 Encoding of Function Field When rs=W or L.
  CVT_S_W   =   ((4 << 3) + 0),
  CVT_D_W   =   ((4 << 3) + 1),
  CVT_S_L   =   ((4 << 3) + 0),
  CVT_D_L   =   ((4 << 3) + 1),
  // COP1 Encoding of Function Field When rs=PS.

  NULLSF    =   0
};


// ----- Emulated conditions.
// On MIPS we use this enum to abstract from conditionnal branch instructions.
// the 'U' prefix is used to specify unsigned comparisons.
enum Condition {
  // Any value < 0 is considered no_condition.
  no_condition  = -1,

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

  cc_always     = 16,

  // aliases
  carry         = Uless,
  not_carry     = Ugreater_equal,
  zero          = equal,
  eq            = equal,
  not_zero      = not_equal,
  ne            = not_equal,
  sign          = negative,
  not_sign      = positive,

  cc_default    = no_condition
};

// ----- Coprocessor conditions.
enum FPUCondition {
  F,    // False
  UN,   // Unordered
  EQ,   // Equal
  UEQ,  // Unordered or Equal
  OLT,  // Ordered or Less Than
  ULT,  // Unordered or Less Than
  OLE,  // Ordered or Less Than or Equal
  ULE   // Unordered or Less Than or Equal
};


// Break 0xfffff, reserved for redirected real time call.
const Instr rtCallRedirInstr = SPECIAL | BREAK | call_rt_redirected << 6;
// A nop instruction. (Encoding of sll 0 0 0).
const Instr nopInstr = 0;

class Instruction {
 public:
  enum {
    kInstructionSize = 4,
    kInstructionSizeLog2 = 2,
    // On MIPS PC cannot actually be directly accessed. We behave as if PC was
    // always the value of the current instruction being exectued.
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
  inline Opcode OpcodeField() const {
    return static_cast<Opcode>(
        Bits(kOpcodeShift + kOpcodeBits - 1, kOpcodeShift));
  }

  inline int RsField() const {
    ASSERT(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return Bits(kRsShift + kRsBits - 1, kRsShift);
  }

  inline int RtField() const {
    ASSERT(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return Bits(kRtShift + kRtBits - 1, kRtShift);
  }

  inline int RdField() const {
    ASSERT(InstructionType() == kRegisterType);
    return Bits(kRdShift + kRdBits - 1, kRdShift);
  }

  inline int SaField() const {
    ASSERT(InstructionType() == kRegisterType);
    return Bits(kSaShift + kSaBits - 1, kSaShift);
  }

  inline int FunctionField() const {
    ASSERT(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return Bits(kFunctionShift + kFunctionBits - 1, kFunctionShift);
  }

  inline int FsField() const {
    return Bits(kFsShift + kRsBits - 1, kFsShift);
  }

  inline int FtField() const {
    return Bits(kFtShift + kRsBits - 1, kFtShift);
  }

  // Return the fields at their original place in the instruction encoding.
  inline Opcode OpcodeFieldRaw() const {
    return static_cast<Opcode>(InstructionBits() & kOpcodeMask);
  }

  inline int RsFieldRaw() const {
    ASSERT(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return InstructionBits() & kRsFieldMask;
  }

  inline int RtFieldRaw() const {
    ASSERT(InstructionType() == kRegisterType ||
           InstructionType() == kImmediateType);
    return InstructionBits() & kRtFieldMask;
  }

  inline int RdFieldRaw() const {
    ASSERT(InstructionType() == kRegisterType);
    return InstructionBits() & kRdFieldMask;
  }

  inline int SaFieldRaw() const {
    ASSERT(InstructionType() == kRegisterType);
    return InstructionBits() & kSaFieldMask;
  }

  inline int FunctionFieldRaw() const {
    return InstructionBits() & kFunctionFieldMask;
  }

  // Get the secondary field according to the opcode.
  inline int SecondaryField() const {
    Opcode op = OpcodeFieldRaw();
    switch (op) {
      case SPECIAL:
      case SPECIAL2:
        return FunctionField();
      case COP1:
        return RsField();
      case REGIMM:
        return RtField();
      default:
        return NULLSF;
    }
  }

  inline int32_t Imm16Field() const {
    ASSERT(InstructionType() == kImmediateType);
    return Bits(kImm16Shift + kImm16Bits - 1, kImm16Shift);
  }

  inline int32_t Imm26Field() const {
    ASSERT(InstructionType() == kJumpType);
    return Bits(kImm16Shift + kImm26Bits - 1, kImm26Shift);
  }

  // Say if the instruction should not be used in a branch delay slot.
  bool IsForbiddenInBranchDelay();
  // Say if the instruction 'links'. eg: jal, bal.
  bool IsLinkingInstruction();
  // Say if the instruction is a break or a trap.
  bool IsTrap();

  // Instructions are read of out a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instruction.
  // Use the At(pc) function to create references to Instruction.
  static Instruction* At(byte_* pc) {
    return reinterpret_cast<Instruction*>(pc);
  }

 private:
  // We need to prevent the creation of instances of class Instruction.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instruction);
};


// -----------------------------------------------------------------------------
// MIPS assembly various constants.

static const int kArgsSlotsSize  = 4 * Instruction::kInstructionSize;
static const int kArgsSlotsNum   = 4;

static const int kBranchReturnOffset = 2 * Instruction::kInstructionSize;

static const int kDoubleAlignment = 2 * 8;
static const int kDoubleAlignmentMask = kDoubleAlignmentMask - 1;


} }   // namespace assembler::mips

#endif    // #ifndef V8_MIPS_CONSTANTS_H_


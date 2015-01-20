// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PPC_CONSTANTS_PPC_H_
#define V8_PPC_CONSTANTS_PPC_H_

namespace v8 {
namespace internal {

// Number of registers
const int kNumRegisters = 32;

// FP support.
const int kNumFPDoubleRegisters = 32;
const int kNumFPRegisters = kNumFPDoubleRegisters;

const int kNoRegister = -1;

// sign-extend the least significant 16-bits of value <imm>
#define SIGN_EXT_IMM16(imm) ((static_cast<int>(imm) << 16) >> 16)

// sign-extend the least significant 26-bits of value <imm>
#define SIGN_EXT_IMM26(imm) ((static_cast<int>(imm) << 6) >> 6)

// -----------------------------------------------------------------------------
// Conditions.

// Defines constants and accessor classes to assemble, disassemble and
// simulate PPC instructions.
//
// Section references in the code refer to the "PowerPC Microprocessor
// Family: The Programmer.s Reference Guide" from 10/95
// https://www-01.ibm.com/chips/techlib/techlib.nsf/techdocs/852569B20050FF778525699600741775/$file/prg.pdf
//

// Constants for specific fields are defined in their respective named enums.
// General constants are in an anonymous enum in class Instr.
enum Condition {
  kNoCondition = -1,
  eq = 0,         // Equal.
  ne = 1,         // Not equal.
  ge = 2,         // Greater or equal.
  lt = 3,         // Less than.
  gt = 4,         // Greater than.
  le = 5,         // Less then or equal
  unordered = 6,  // Floating-point unordered
  ordered = 7,
  overflow = 8,  // Summary overflow
  nooverflow = 9,
  al = 10  // Always.
};


inline Condition NegateCondition(Condition cond) {
  DCHECK(cond != al);
  return static_cast<Condition>(cond ^ ne);
}


// Commute a condition such that {a cond b == b cond' a}.
inline Condition CommuteCondition(Condition cond) {
  switch (cond) {
    case lt:
      return gt;
    case gt:
      return lt;
    case ge:
      return le;
    case le:
      return ge;
    default:
      return cond;
  }
}

// -----------------------------------------------------------------------------
// Instructions encoding.

// Instr is merely used by the Assembler to distinguish 32bit integers
// representing instructions from usual 32 bit values.
// Instruction objects are pointers to 32bit values, and provide methods to
// access the various ISA fields.
typedef int32_t Instr;

// Opcodes as defined in section 4.2 table 34 (32bit PowerPC)
enum Opcode {
  TWI = 3 << 26,       // Trap Word Immediate
  MULLI = 7 << 26,     // Multiply Low Immediate
  SUBFIC = 8 << 26,    // Subtract from Immediate Carrying
  CMPLI = 10 << 26,    // Compare Logical Immediate
  CMPI = 11 << 26,     // Compare Immediate
  ADDIC = 12 << 26,    // Add Immediate Carrying
  ADDICx = 13 << 26,   // Add Immediate Carrying and Record
  ADDI = 14 << 26,     // Add Immediate
  ADDIS = 15 << 26,    // Add Immediate Shifted
  BCX = 16 << 26,      // Branch Conditional
  SC = 17 << 26,       // System Call
  BX = 18 << 26,       // Branch
  EXT1 = 19 << 26,     // Extended code set 1
  RLWIMIX = 20 << 26,  // Rotate Left Word Immediate then Mask Insert
  RLWINMX = 21 << 26,  // Rotate Left Word Immediate then AND with Mask
  RLWNMX = 23 << 26,   // Rotate Left Word then AND with Mask
  ORI = 24 << 26,      // OR Immediate
  ORIS = 25 << 26,     // OR Immediate Shifted
  XORI = 26 << 26,     // XOR Immediate
  XORIS = 27 << 26,    // XOR Immediate Shifted
  ANDIx = 28 << 26,    // AND Immediate
  ANDISx = 29 << 26,   // AND Immediate Shifted
  EXT5 = 30 << 26,     // Extended code set 5 - 64bit only
  EXT2 = 31 << 26,     // Extended code set 2
  LWZ = 32 << 26,      // Load Word and Zero
  LWZU = 33 << 26,     // Load Word with Zero Update
  LBZ = 34 << 26,      // Load Byte and Zero
  LBZU = 35 << 26,     // Load Byte and Zero with Update
  STW = 36 << 26,      // Store
  STWU = 37 << 26,     // Store Word with Update
  STB = 38 << 26,      // Store Byte
  STBU = 39 << 26,     // Store Byte with Update
  LHZ = 40 << 26,      // Load Half and Zero
  LHZU = 41 << 26,     // Load Half and Zero with Update
  LHA = 42 << 26,      // Load Half Algebraic
  LHAU = 43 << 26,     // Load Half Algebraic with Update
  STH = 44 << 26,      // Store Half
  STHU = 45 << 26,     // Store Half with Update
  LMW = 46 << 26,      // Load Multiple Word
  STMW = 47 << 26,     // Store Multiple Word
  LFS = 48 << 26,      // Load Floating-Point Single
  LFSU = 49 << 26,     // Load Floating-Point Single with Update
  LFD = 50 << 26,      // Load Floating-Point Double
  LFDU = 51 << 26,     // Load Floating-Point Double with Update
  STFS = 52 << 26,     // Store Floating-Point Single
  STFSU = 53 << 26,    // Store Floating-Point Single with Update
  STFD = 54 << 26,     // Store Floating-Point Double
  STFDU = 55 << 26,    // Store Floating-Point Double with Update
  LD = 58 << 26,       // Load Double Word
  EXT3 = 59 << 26,     // Extended code set 3
  STD = 62 << 26,      // Store Double Word (optionally with Update)
  EXT4 = 63 << 26      // Extended code set 4
};

// Bits 10-1
enum OpcodeExt1 {
  MCRF = 0 << 1,      // Move Condition Register Field
  BCLRX = 16 << 1,    // Branch Conditional Link Register
  CRNOR = 33 << 1,    // Condition Register NOR)
  RFI = 50 << 1,      // Return from Interrupt
  CRANDC = 129 << 1,  // Condition Register AND with Complement
  ISYNC = 150 << 1,   // Instruction Synchronize
  CRXOR = 193 << 1,   // Condition Register XOR
  CRNAND = 225 << 1,  // Condition Register NAND
  CRAND = 257 << 1,   // Condition Register AND
  CREQV = 289 << 1,   // Condition Register Equivalent
  CRORC = 417 << 1,   // Condition Register OR with Complement
  CROR = 449 << 1,    // Condition Register OR
  BCCTRX = 528 << 1   // Branch Conditional to Count Register
};

// Bits 9-1 or 10-1
enum OpcodeExt2 {
  CMP = 0 << 1,
  TW = 4 << 1,
  SUBFCX = 8 << 1,
  ADDCX = 10 << 1,
  MULHWUX = 11 << 1,
  MFCR = 19 << 1,
  LWARX = 20 << 1,
  LDX = 21 << 1,
  LWZX = 23 << 1,  // load word zero w/ x-form
  SLWX = 24 << 1,
  CNTLZWX = 26 << 1,
  SLDX = 27 << 1,
  ANDX = 28 << 1,
  CMPL = 32 << 1,
  SUBFX = 40 << 1,
  MFVSRD = 51 << 1,  // Move From VSR Doubleword
  LDUX = 53 << 1,
  DCBST = 54 << 1,
  LWZUX = 55 << 1,  // load word zero w/ update x-form
  CNTLZDX = 58 << 1,
  ANDCX = 60 << 1,
  MULHWX = 75 << 1,
  DCBF = 86 << 1,
  LBZX = 87 << 1,  // load byte zero w/ x-form
  NEGX = 104 << 1,
  MFVSRWZ = 115 << 1,  // Move From VSR Word And Zero
  LBZUX = 119 << 1,    // load byte zero w/ update x-form
  NORX = 124 << 1,
  SUBFEX = 136 << 1,
  ADDEX = 138 << 1,
  STDX = 149 << 1,
  STWX = 151 << 1,    // store word w/ x-form
  MTVSRD = 179 << 1,  // Move To VSR Doubleword
  STDUX = 181 << 1,
  STWUX = 183 << 1,  // store word w/ update x-form
                     /*
    MTCRF
    MTMSR
    STWCXx
    SUBFZEX
  */
  ADDZEX = 202 << 1,  // Add to Zero Extended
                      /*
    MTSR
  */
  MTVSRWA = 211 << 1,  // Move To VSR Word Algebraic
  STBX = 215 << 1,     // store byte w/ x-form
  MULLD = 233 << 1,    // Multiply Low Double Word
  MULLW = 235 << 1,    // Multiply Low Word
  MTVSRWZ = 243 << 1,  // Move To VSR Word And Zero
  STBUX = 247 << 1,    // store byte w/ update x-form
  ADDX = 266 << 1,     // Add
  LHZX = 279 << 1,     // load half-word zero w/ x-form
  LHZUX = 311 << 1,    // load half-word zero w/ update x-form
  LHAX = 343 << 1,     // load half-word algebraic w/ x-form
  LHAUX = 375 << 1,    // load half-word algebraic w/ update x-form
  XORX = 316 << 1,     // Exclusive OR
  MFSPR = 339 << 1,    // Move from Special-Purpose-Register
  STHX = 407 << 1,     // store half-word w/ x-form
  STHUX = 439 << 1,    // store half-word w/ update x-form
  ORX = 444 << 1,      // Or
  MTSPR = 467 << 1,    // Move to Special-Purpose-Register
  DIVD = 489 << 1,     // Divide Double Word
  DIVW = 491 << 1,     // Divide Word

  // Below represent bits 10-1  (any value >= 512)
  LFSX = 535 << 1,    // load float-single w/ x-form
  SRWX = 536 << 1,    // Shift Right Word
  SRDX = 539 << 1,    // Shift Right Double Word
  LFSUX = 567 << 1,   // load float-single w/ update x-form
  SYNC = 598 << 1,    // Synchronize
  LFDX = 599 << 1,    // load float-double w/ x-form
  LFDUX = 631 << 1,   // load float-double w/ update X-form
  STFSX = 663 << 1,   // store float-single w/ x-form
  STFSUX = 695 << 1,  // store float-single w/ update x-form
  STFDX = 727 << 1,   // store float-double w/ x-form
  STFDUX = 759 << 1,  // store float-double w/ update x-form
  SRAW = 792 << 1,    // Shift Right Algebraic Word
  SRAD = 794 << 1,    // Shift Right Algebraic Double Word
  SRAWIX = 824 << 1,  // Shift Right Algebraic Word Immediate
  SRADIX = 413 << 2,  // Shift Right Algebraic Double Word Immediate
  EXTSH = 922 << 1,   // Extend Sign Halfword
  EXTSB = 954 << 1,   // Extend Sign Byte
  ICBI = 982 << 1,    // Instruction Cache Block Invalidate
  EXTSW = 986 << 1    // Extend Sign Word
};

// Some use Bits 10-1 and other only 5-1 for the opcode
enum OpcodeExt4 {
  // Bits 5-1
  FDIV = 18 << 1,   // Floating Divide
  FSUB = 20 << 1,   // Floating Subtract
  FADD = 21 << 1,   // Floating Add
  FSQRT = 22 << 1,  // Floating Square Root
  FSEL = 23 << 1,   // Floating Select
  FMUL = 25 << 1,   // Floating Multiply
  FMSUB = 28 << 1,  // Floating Multiply-Subtract
  FMADD = 29 << 1,  // Floating Multiply-Add

  // Bits 10-1
  FCMPU = 0 << 1,     // Floating Compare Unordered
  FRSP = 12 << 1,     // Floating-Point Rounding
  FCTIW = 14 << 1,    // Floating Convert to Integer Word X-form
  FCTIWZ = 15 << 1,   // Floating Convert to Integer Word with Round to Zero
  FNEG = 40 << 1,     // Floating Negate
  MCRFS = 64 << 1,    // Move to Condition Register from FPSCR
  FMR = 72 << 1,      // Floating Move Register
  MTFSFI = 134 << 1,  // Move to FPSCR Field Immediate
  FABS = 264 << 1,    // Floating Absolute Value
  FRIM = 488 << 1,    // Floating Round to Integer Minus
  MFFS = 583 << 1,    // move from FPSCR x-form
  MTFSF = 711 << 1,   // move to FPSCR fields XFL-form
  FCFID = 846 << 1,   // Floating convert from integer doubleword
  FCTID = 814 << 1,   // Floating convert from integer doubleword
  FCTIDZ = 815 << 1   // Floating convert from integer doubleword
};

enum OpcodeExt5 {
  // Bits 4-2
  RLDICL = 0 << 1,  // Rotate Left Double Word Immediate then Clear Left
  RLDICR = 2 << 1,  // Rotate Left Double Word Immediate then Clear Right
  RLDIC = 4 << 1,   // Rotate Left Double Word Immediate then Clear
  RLDIMI = 6 << 1,  // Rotate Left Double Word Immediate then Mask Insert
  // Bits 4-1
  RLDCL = 8 << 1,  // Rotate Left Double Word then Clear Left
  RLDCR = 9 << 1   // Rotate Left Double Word then Clear Right
};

// Instruction encoding bits and masks.
enum {
  // Instruction encoding bit
  B1 = 1 << 1,
  B4 = 1 << 4,
  B5 = 1 << 5,
  B7 = 1 << 7,
  B8 = 1 << 8,
  B9 = 1 << 9,
  B12 = 1 << 12,
  B18 = 1 << 18,
  B19 = 1 << 19,
  B20 = 1 << 20,
  B22 = 1 << 22,
  B23 = 1 << 23,
  B24 = 1 << 24,
  B25 = 1 << 25,
  B26 = 1 << 26,
  B27 = 1 << 27,
  B28 = 1 << 28,
  B6 = 1 << 6,
  B10 = 1 << 10,
  B11 = 1 << 11,
  B16 = 1 << 16,
  B17 = 1 << 17,
  B21 = 1 << 21,

  // Instruction bit masks
  kCondMask = 0x1F << 21,
  kOff12Mask = (1 << 12) - 1,
  kImm24Mask = (1 << 24) - 1,
  kOff16Mask = (1 << 16) - 1,
  kImm16Mask = (1 << 16) - 1,
  kImm26Mask = (1 << 26) - 1,
  kBOfieldMask = 0x1f << 21,
  kOpcodeMask = 0x3f << 26,
  kExt1OpcodeMask = 0x3ff << 1,
  kExt2OpcodeMask = 0x1f << 1,
  kExt5OpcodeMask = 0x3 << 2,
  kBOMask = 0x1f << 21,
  kBIMask = 0x1F << 16,
  kBDMask = 0x14 << 2,
  kAAMask = 0x01 << 1,
  kLKMask = 0x01,
  kRCMask = 0x01,
  kTOMask = 0x1f << 21
};

// the following is to differentiate different faked opcodes for
// the BOGUS PPC instruction we invented (when bit 25 is 0) or to mark
// different stub code (when bit 25 is 1)
//   - use primary opcode 1 for undefined instruction
//   - use bit 25 to indicate whether the opcode is for fake-arm
//     instr or stub-marker
//   - use the least significant 6-bit to indicate FAKE_OPCODE_T or
//     MARKER_T
#define FAKE_OPCODE 1 << 26
#define MARKER_SUBOPCODE_BIT 25
#define MARKER_SUBOPCODE 1 << MARKER_SUBOPCODE_BIT
#define FAKER_SUBOPCODE 0 << MARKER_SUBOPCODE_BIT

enum FAKE_OPCODE_T {
  fBKPT = 14,
  fLastFaker  // can't be more than 128 (2^^7)
};
#define FAKE_OPCODE_HIGH_BIT 7  // fake opcode has to fall into bit 0~7
#define F_NEXT_AVAILABLE_STUB_MARKER 369  // must be less than 2^^9 (512)
#define STUB_MARKER_HIGH_BIT 9  // stub marker has to fall into bit 0~9
// -----------------------------------------------------------------------------
// Addressing modes and instruction variants.

// Overflow Exception
enum OEBit {
  SetOE = 1 << 10,   // Set overflow exception
  LeaveOE = 0 << 10  // No overflow exception
};

// Record bit
enum RCBit {   // Bit 0
  SetRC = 1,   // LT,GT,EQ,SO
  LeaveRC = 0  // None
};

// Link bit
enum LKBit {   // Bit 0
  SetLK = 1,   // Load effective address of next instruction
  LeaveLK = 0  // No action
};

enum BOfield {        // Bits 25-21
  DCBNZF = 0 << 21,   // Decrement CTR; branch if CTR != 0 and condition false
  DCBEZF = 2 << 21,   // Decrement CTR; branch if CTR == 0 and condition false
  BF = 4 << 21,       // Branch if condition false
  DCBNZT = 8 << 21,   // Decrement CTR; branch if CTR != 0 and condition true
  DCBEZT = 10 << 21,  // Decrement CTR; branch if CTR == 0 and condition true
  BT = 12 << 21,      // Branch if condition true
  DCBNZ = 16 << 21,   // Decrement CTR; branch if CTR != 0
  DCBEZ = 18 << 21,   // Decrement CTR; branch if CTR == 0
  BA = 20 << 21       // Branch always
};

#if V8_OS_AIX
#undef CR_LT
#undef CR_GT
#undef CR_EQ
#undef CR_SO
#endif

enum CRBit { CR_LT = 0, CR_GT = 1, CR_EQ = 2, CR_SO = 3, CR_FU = 3 };

#define CRWIDTH 4

// -----------------------------------------------------------------------------
// Supervisor Call (svc) specific support.

// Special Software Interrupt codes when used in the presence of the PPC
// simulator.
// svc (formerly swi) provides a 24bit immediate value. Use bits 22:0 for
// standard SoftwareInterrupCode. Bit 23 is reserved for the stop feature.
enum SoftwareInterruptCodes {
  // transition to C code
  kCallRtRedirected = 0x10,
  // break point
  kBreakpoint = 0x821008,  // bits23-0 of 0x7d821008 = twge r2, r2
  // stop
  kStopCode = 1 << 23,
  // info
  kInfo = 0x9ff808  // bits23-0 of 0x7d9ff808 = twge r31, r31
};
const uint32_t kStopCodeMask = kStopCode - 1;
const uint32_t kMaxStopCode = kStopCode - 1;
const int32_t kDefaultStopCode = -1;

// FP rounding modes.
enum FPRoundingMode {
  RN = 0,  // Round to Nearest.
  RZ = 1,  // Round towards zero.
  RP = 2,  // Round towards Plus Infinity.
  RM = 3,  // Round towards Minus Infinity.

  // Aliases.
  kRoundToNearest = RN,
  kRoundToZero = RZ,
  kRoundToPlusInf = RP,
  kRoundToMinusInf = RM
};

const uint32_t kFPRoundingModeMask = 3;

enum CheckForInexactConversion {
  kCheckForInexactConversion,
  kDontCheckForInexactConversion
};

// -----------------------------------------------------------------------------
// Specific instructions, constants, and masks.
// These constants are declared in assembler-arm.cc, as they use named registers
// and other constants.


// add(sp, sp, 4) instruction (aka Pop())
extern const Instr kPopInstruction;

// str(r, MemOperand(sp, 4, NegPreIndex), al) instruction (aka push(r))
// register r is not encoded.
extern const Instr kPushRegPattern;

// ldr(r, MemOperand(sp, 4, PostIndex), al) instruction (aka pop(r))
// register r is not encoded.
extern const Instr kPopRegPattern;

// use TWI to indicate redirection call for simulation mode
const Instr rtCallRedirInstr = TWI;

// -----------------------------------------------------------------------------
// Instruction abstraction.

// The class Instruction enables access to individual fields defined in the PPC
// architecture instruction set encoding.
// Note that the Assembler uses typedef int32_t Instr.
//
// Example: Test whether the instruction at ptr does set the condition code
// bits.
//
// bool InstructionSetsConditionCodes(byte* ptr) {
//   Instruction* instr = Instruction::At(ptr);
//   int type = instr->TypeValue();
//   return ((type == 0) || (type == 1)) && instr->HasS();
// }
//
class Instruction {
 public:
  enum { kInstrSize = 4, kInstrSizeLog2 = 2, kPCReadOffset = 8 };

// Helper macro to define static accessors.
// We use the cast to char* trick to bypass the strict anti-aliasing rules.
#define DECLARE_STATIC_TYPED_ACCESSOR(return_type, Name) \
  static inline return_type Name(Instr instr) {          \
    char* temp = reinterpret_cast<char*>(&instr);        \
    return reinterpret_cast<Instruction*>(temp)->Name(); \
  }

#define DECLARE_STATIC_ACCESSOR(Name) DECLARE_STATIC_TYPED_ACCESSOR(int, Name)

  // Get the raw instruction bits.
  inline Instr InstructionBits() const {
    return *reinterpret_cast<const Instr*>(this);
  }

  // Set the raw instruction bits to value.
  inline void SetInstructionBits(Instr value) {
    *reinterpret_cast<Instr*>(this) = value;
  }

  // Read one particular bit out of the instruction bits.
  inline int Bit(int nr) const { return (InstructionBits() >> nr) & 1; }

  // Read a bit field's value out of the instruction bits.
  inline int Bits(int hi, int lo) const {
    return (InstructionBits() >> lo) & ((2 << (hi - lo)) - 1);
  }

  // Read a bit field out of the instruction bits.
  inline int BitField(int hi, int lo) const {
    return InstructionBits() & (((2 << (hi - lo)) - 1) << lo);
  }

  // Static support.

  // Read one particular bit out of the instruction bits.
  static inline int Bit(Instr instr, int nr) { return (instr >> nr) & 1; }

  // Read the value of a bit field out of the instruction bits.
  static inline int Bits(Instr instr, int hi, int lo) {
    return (instr >> lo) & ((2 << (hi - lo)) - 1);
  }


  // Read a bit field out of the instruction bits.
  static inline int BitField(Instr instr, int hi, int lo) {
    return instr & (((2 << (hi - lo)) - 1) << lo);
  }

  inline int RSValue() const { return Bits(25, 21); }
  inline int RTValue() const { return Bits(25, 21); }
  inline int RAValue() const { return Bits(20, 16); }
  DECLARE_STATIC_ACCESSOR(RAValue);
  inline int RBValue() const { return Bits(15, 11); }
  DECLARE_STATIC_ACCESSOR(RBValue);
  inline int RCValue() const { return Bits(10, 6); }
  DECLARE_STATIC_ACCESSOR(RCValue);

  inline int OpcodeValue() const { return static_cast<Opcode>(Bits(31, 26)); }
  inline Opcode OpcodeField() const {
    return static_cast<Opcode>(BitField(24, 21));
  }

  // Fields used in Software interrupt instructions
  inline SoftwareInterruptCodes SvcValue() const {
    return static_cast<SoftwareInterruptCodes>(Bits(23, 0));
  }

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

 private:
  static const char* names_[kNumRegisters];
  static const RegisterAlias aliases_[];
};

// Helper functions for converting between FP register numbers and names.
class FPRegisters {
 public:
  // Return the name of the register.
  static const char* Name(int reg);

  // Lookup the register number for the name provided.
  static int Number(const char* name);

 private:
  static const char* names_[kNumFPRegisters];
};
}
}  // namespace v8::internal

#endif  // V8_PPC_CONSTANTS_PPC_H_

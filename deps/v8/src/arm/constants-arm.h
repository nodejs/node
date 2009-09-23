// Copyright 2009 the V8 project authors. All rights reserved.
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

#ifndef V8_ARM_CONSTANTS_ARM_H_
#define V8_ARM_CONSTANTS_ARM_H_

// The simulator emulates the EABI so we define the USE_ARM_EABI macro if we
// are not running on real ARM hardware.  One reason for this is that the
// old ABI uses fp registers in the calling convention and the simulator does
// not simulate fp registers or coroutine instructions.
#if defined(__ARM_EABI__) || !defined(__arm__)
# define USE_ARM_EABI 1
#endif

// This means that interwork-compatible jump instructions are generated.  We
// want to generate them on the simulator too so it makes snapshots that can
// be used on real hardware.
#if defined(__THUMB_INTERWORK__) || !defined(__arm__)
# define USE_THUMB_INTERWORK 1
#endif

#if defined(__ARM_ARCH_5T__) || \
    defined(__ARM_ARCH_5TE__) || \
    defined(__ARM_ARCH_6__) || \
    defined(__ARM_ARCH_7A__) || \
    defined(__ARM_ARCH_7__)
# define CAN_USE_ARMV5_INSTRUCTIONS 1
# define CAN_USE_THUMB_INSTRUCTIONS 1
#endif

#if defined(__ARM_ARCH_6__) || \
    defined(__ARM_ARCH_7A__) || \
    defined(__ARM_ARCH_7__)
# define CAN_USE_ARMV6_INSTRUCTIONS 1
#endif

#if defined(__ARM_ARCH_7A__) || \
    defined(__ARM_ARCH_7__)
# define CAN_USE_ARMV7_INSTRUCTIONS 1
#endif

// Simulator should support ARM5 instructions.
#if !defined(__arm__)
# define CAN_USE_ARMV5_INSTRUCTIONS 1
# define CAN_USE_THUMB_INSTRUCTIONS 1
#endif

namespace assembler {
namespace arm {

// Number of registers in normal ARM mode.
static const int kNumRegisters = 16;

// PC is register 15.
static const int kPCRegister = 15;
static const int kNoRegister = -1;

// Defines constants and accessor classes to assemble, disassemble and
// simulate ARM instructions.
//
// Section references in the code refer to the "ARM Architecture Reference
// Manual" from July 2005 (available at http://www.arm.com/miscPDFs/14128.pdf)
//
// Constants for specific fields are defined in their respective named enums.
// General constants are in an anonymous enum in class Instr.

typedef unsigned char byte;

// Values for the condition field as defined in section A3.2
enum Condition {
  no_condition = -1,
  EQ =  0,  // equal
  NE =  1,  // not equal
  CS =  2,  // carry set/unsigned higher or same
  CC =  3,  // carry clear/unsigned lower
  MI =  4,  // minus/negative
  PL =  5,  // plus/positive or zero
  VS =  6,  // overflow
  VC =  7,  // no overflow
  HI =  8,  // unsigned higher
  LS =  9,  // unsigned lower or same
  GE = 10,  // signed greater than or equal
  LT = 11,  // signed less than
  GT = 12,  // signed greater than
  LE = 13,  // signed less than or equal
  AL = 14,  // always (unconditional)
  special_condition = 15,  // special condition (refer to section A3.2.1)
  max_condition = 16
};


// Opcodes for Data-processing instructions (instructions with a type 0 and 1)
// as defined in section A3.4
enum Opcode {
  no_operand = -1,
  AND =  0,  // Logical AND
  EOR =  1,  // Logical Exclusive OR
  SUB =  2,  // Subtract
  RSB =  3,  // Reverse Subtract
  ADD =  4,  // Add
  ADC =  5,  // Add with Carry
  SBC =  6,  // Subtract with Carry
  RSC =  7,  // Reverse Subtract with Carry
  TST =  8,  // Test
  TEQ =  9,  // Test Equivalence
  CMP = 10,  // Compare
  CMN = 11,  // Compare Negated
  ORR = 12,  // Logical (inclusive) OR
  MOV = 13,  // Move
  BIC = 14,  // Bit Clear
  MVN = 15,  // Move Not
  max_operand = 16
};


// Some special instructions encoded as a TEQ with S=0 (bit 20).
enum Opcode9Bits {
  BX   =  1,
  BXJ  =  2,
  BLX  =  3,
  BKPT =  7
};


// Some special instructions encoded as a CMN with S=0 (bit 20).
enum Opcode11Bits {
  CLZ  =  1
};


// S


// Shifter types for Data-processing operands as defined in section A5.1.2.
enum Shift {
  no_shift = -1,
  LSL = 0,  // Logical shift left
  LSR = 1,  // Logical shift right
  ASR = 2,  // Arithmetic shift right
  ROR = 3,  // Rotate right
  max_shift = 4
};


// Special Software Interrupt codes when used in the presence of the ARM
// simulator.
enum SoftwareInterruptCodes {
  // transition to C code
  call_rt_redirected = 0x10,
  // break point
  break_point = 0x20
};


typedef int32_t instr_t;


// The class Instr enables access to individual fields defined in the ARM
// architecture instruction set encoding as described in figure A3-1.
//
// Example: Test whether the instruction at ptr does set the condition code
// bits.
//
// bool InstructionSetsConditionCodes(byte* ptr) {
//   Instr* instr = Instr::At(ptr);
//   int type = instr->TypeField();
//   return ((type == 0) || (type == 1)) && instr->HasS();
// }
//
class Instr {
 public:
  enum {
    kInstrSize = 4,
    kInstrSizeLog2 = 2,
    kPCReadOffset = 8
  };

  // Get the raw instruction bits.
  inline instr_t InstructionBits() const {
    return *reinterpret_cast<const instr_t*>(this);
  }

  // Set the raw instruction bits to value.
  inline void SetInstructionBits(instr_t value) {
    *reinterpret_cast<instr_t*>(this) = value;
  }

  // Read one particular bit out of the instruction bits.
  inline int Bit(int nr) const {
    return (InstructionBits() >> nr) & 1;
  }

  // Read a bit field out of the instruction bits.
  inline int Bits(int hi, int lo) const {
    return (InstructionBits() >> lo) & ((2 << (hi - lo)) - 1);
  }


  // Accessors for the different named fields used in the ARM encoding.
  // The naming of these accessor corresponds to figure A3-1.
  // Generally applicable fields
  inline Condition ConditionField() const {
    return static_cast<Condition>(Bits(31, 28));
  }
  inline int TypeField() const { return Bits(27, 25); }

  inline int RnField() const { return Bits(19, 16); }
  inline int RdField() const { return Bits(15, 12); }

  // Fields used in Data processing instructions
  inline Opcode OpcodeField() const {
    return static_cast<Opcode>(Bits(24, 21));
  }
  inline int SField() const { return Bit(20); }
    // with register
  inline int RmField() const { return Bits(3, 0); }
  inline Shift ShiftField() const { return static_cast<Shift>(Bits(6, 5)); }
  inline int RegShiftField() const { return Bit(4); }
  inline int RsField() const { return Bits(11, 8); }
  inline int ShiftAmountField() const { return Bits(11, 7); }
    // with immediate
  inline int RotateField() const { return Bits(11, 8); }
  inline int Immed8Field() const { return Bits(7, 0); }

  // Fields used in Load/Store instructions
  inline int PUField() const { return Bits(24, 23); }
  inline int  BField() const { return Bit(22); }
  inline int  WField() const { return Bit(21); }
  inline int  LField() const { return Bit(20); }
    // with register uses same fields as Data processing instructions above
    // with immediate
  inline int Offset12Field() const { return Bits(11, 0); }
    // multiple
  inline int RlistField() const { return Bits(15, 0); }
    // extra loads and stores
  inline int SignField() const { return Bit(6); }
  inline int HField() const { return Bit(5); }
  inline int ImmedHField() const { return Bits(11, 8); }
  inline int ImmedLField() const { return Bits(3, 0); }

  // Fields used in Branch instructions
  inline int LinkField() const { return Bit(24); }
  inline int SImmed24Field() const { return ((InstructionBits() << 8) >> 8); }

  // Fields used in Software interrupt instructions
  inline SoftwareInterruptCodes SwiField() const {
    return static_cast<SoftwareInterruptCodes>(Bits(23, 0));
  }

  // Test for special encodings of type 0 instructions (extra loads and stores,
  // as well as multiplications).
  inline bool IsSpecialType0() const { return (Bit(7) == 1) && (Bit(4) == 1); }

  // Special accessors that test for existence of a value.
  inline bool HasS()    const { return SField() == 1; }
  inline bool HasB()    const { return BField() == 1; }
  inline bool HasW()    const { return WField() == 1; }
  inline bool HasL()    const { return LField() == 1; }
  inline bool HasSign() const { return SignField() == 1; }
  inline bool HasH()    const { return HField() == 1; }
  inline bool HasLink() const { return LinkField() == 1; }

  // Instructions are read of out a code stream. The only way to get a
  // reference to an instruction is to convert a pointer. There is no way
  // to allocate or create instances of class Instr.
  // Use the At(pc) function to create references to Instr.
  static Instr* At(byte* pc) { return reinterpret_cast<Instr*>(pc); }

 private:
  // We need to prevent the creation of instances of class Instr.
  DISALLOW_IMPLICIT_CONSTRUCTORS(Instr);
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
    const char *name;
  };

 private:
  static const char* names_[kNumRegisters];
  static const RegisterAlias aliases_[];
};



} }  // namespace assembler::arm

#endif  // V8_ARM_CONSTANTS_ARM_H_

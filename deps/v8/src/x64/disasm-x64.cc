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

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#include "v8.h"

#if defined(V8_TARGET_ARCH_X64)

#include "disasm.h"

namespace disasm {

enum OperandType {
  UNSET_OP_ORDER = 0,
  // Operand size decides between 16, 32 and 64 bit operands.
  REG_OPER_OP_ORDER = 1,  // Register destination, operand source.
  OPER_REG_OP_ORDER = 2,  // Operand destination, register source.
  // Fixed 8-bit operands.
  BYTE_SIZE_OPERAND_FLAG = 4,
  BYTE_REG_OPER_OP_ORDER = REG_OPER_OP_ORDER | BYTE_SIZE_OPERAND_FLAG,
  BYTE_OPER_REG_OP_ORDER = OPER_REG_OP_ORDER | BYTE_SIZE_OPERAND_FLAG
};

//------------------------------------------------------------------
// Tables
//------------------------------------------------------------------
struct ByteMnemonic {
  int b;  // -1 terminates, otherwise must be in range (0..255)
  OperandType op_order_;
  const char* mnem;
};


static ByteMnemonic two_operands_instr[] = {
  { 0x00, BYTE_OPER_REG_OP_ORDER, "add" },
  { 0x01, OPER_REG_OP_ORDER,      "add" },
  { 0x02, BYTE_REG_OPER_OP_ORDER, "add" },
  { 0x03, REG_OPER_OP_ORDER,      "add" },
  { 0x08, BYTE_OPER_REG_OP_ORDER, "or" },
  { 0x09, OPER_REG_OP_ORDER,      "or" },
  { 0x0A, BYTE_REG_OPER_OP_ORDER, "or" },
  { 0x0B, REG_OPER_OP_ORDER,      "or" },
  { 0x10, BYTE_OPER_REG_OP_ORDER, "adc" },
  { 0x11, OPER_REG_OP_ORDER,      "adc" },
  { 0x12, BYTE_REG_OPER_OP_ORDER, "adc" },
  { 0x13, REG_OPER_OP_ORDER,      "adc" },
  { 0x18, BYTE_OPER_REG_OP_ORDER, "sbb" },
  { 0x19, OPER_REG_OP_ORDER,      "sbb" },
  { 0x1A, BYTE_REG_OPER_OP_ORDER, "sbb" },
  { 0x1B, REG_OPER_OP_ORDER,      "sbb" },
  { 0x20, BYTE_OPER_REG_OP_ORDER, "and" },
  { 0x21, OPER_REG_OP_ORDER,      "and" },
  { 0x22, BYTE_REG_OPER_OP_ORDER, "and" },
  { 0x23, REG_OPER_OP_ORDER,      "and" },
  { 0x28, BYTE_OPER_REG_OP_ORDER, "sub" },
  { 0x29, OPER_REG_OP_ORDER,      "sub" },
  { 0x2A, BYTE_REG_OPER_OP_ORDER, "sub" },
  { 0x2B, REG_OPER_OP_ORDER,      "sub" },
  { 0x30, BYTE_OPER_REG_OP_ORDER, "xor" },
  { 0x31, OPER_REG_OP_ORDER,      "xor" },
  { 0x32, BYTE_REG_OPER_OP_ORDER, "xor" },
  { 0x33, REG_OPER_OP_ORDER,      "xor" },
  { 0x38, BYTE_OPER_REG_OP_ORDER, "cmp" },
  { 0x39, OPER_REG_OP_ORDER,      "cmp" },
  { 0x3A, BYTE_REG_OPER_OP_ORDER, "cmp" },
  { 0x3B, REG_OPER_OP_ORDER,      "cmp" },
  { 0x63, REG_OPER_OP_ORDER,      "movsxlq" },
  { 0x84, BYTE_REG_OPER_OP_ORDER, "test" },
  { 0x85, REG_OPER_OP_ORDER,      "test" },
  { 0x86, BYTE_REG_OPER_OP_ORDER, "xchg" },
  { 0x87, REG_OPER_OP_ORDER,      "xchg" },
  { 0x88, BYTE_OPER_REG_OP_ORDER, "mov" },
  { 0x89, OPER_REG_OP_ORDER,      "mov" },
  { 0x8A, BYTE_REG_OPER_OP_ORDER, "mov" },
  { 0x8B, REG_OPER_OP_ORDER,      "mov" },
  { 0x8D, REG_OPER_OP_ORDER,      "lea" },
  { -1, UNSET_OP_ORDER, "" }
};


static ByteMnemonic zero_operands_instr[] = {
  { 0xC3, UNSET_OP_ORDER, "ret" },
  { 0xC9, UNSET_OP_ORDER, "leave" },
  { 0xF4, UNSET_OP_ORDER, "hlt" },
  { 0xCC, UNSET_OP_ORDER, "int3" },
  { 0x60, UNSET_OP_ORDER, "pushad" },
  { 0x61, UNSET_OP_ORDER, "popad" },
  { 0x9C, UNSET_OP_ORDER, "pushfd" },
  { 0x9D, UNSET_OP_ORDER, "popfd" },
  { 0x9E, UNSET_OP_ORDER, "sahf" },
  { 0x99, UNSET_OP_ORDER, "cdq" },
  { 0x9B, UNSET_OP_ORDER, "fwait" },
  { 0xA4, UNSET_OP_ORDER, "movs" },
  { 0xA5, UNSET_OP_ORDER, "movs" },
  { 0xA6, UNSET_OP_ORDER, "cmps" },
  { 0xA7, UNSET_OP_ORDER, "cmps" },
  { -1, UNSET_OP_ORDER, "" }
};


static ByteMnemonic call_jump_instr[] = {
  { 0xE8, UNSET_OP_ORDER, "call" },
  { 0xE9, UNSET_OP_ORDER, "jmp" },
  { -1, UNSET_OP_ORDER, "" }
};


static ByteMnemonic short_immediate_instr[] = {
  { 0x05, UNSET_OP_ORDER, "add" },
  { 0x0D, UNSET_OP_ORDER, "or" },
  { 0x15, UNSET_OP_ORDER, "adc" },
  { 0x1D, UNSET_OP_ORDER, "sbb" },
  { 0x25, UNSET_OP_ORDER, "and" },
  { 0x2D, UNSET_OP_ORDER, "sub" },
  { 0x35, UNSET_OP_ORDER, "xor" },
  { 0x3D, UNSET_OP_ORDER, "cmp" },
  { -1, UNSET_OP_ORDER, "" }
};


static const char* conditional_code_suffix[] = {
  "o", "no", "c", "nc", "z", "nz", "na", "a",
  "s", "ns", "pe", "po", "l", "ge", "le", "g"
};


enum InstructionType {
  NO_INSTR,
  ZERO_OPERANDS_INSTR,
  TWO_OPERANDS_INSTR,
  JUMP_CONDITIONAL_SHORT_INSTR,
  REGISTER_INSTR,
  PUSHPOP_INSTR,  // Has implicit 64-bit operand size.
  MOVE_REG_INSTR,
  CALL_JUMP_INSTR,
  SHORT_IMMEDIATE_INSTR
};


enum Prefixes {
  ESCAPE_PREFIX = 0x0F,
  OPERAND_SIZE_OVERRIDE_PREFIX = 0x66,
  ADDRESS_SIZE_OVERRIDE_PREFIX = 0x67,
  REPNE_PREFIX = 0xF2,
  REP_PREFIX = 0xF3,
  REPEQ_PREFIX = REP_PREFIX
};


struct InstructionDesc {
  const char* mnem;
  InstructionType type;
  OperandType op_order_;
  bool byte_size_operation;  // Fixed 8-bit operation.
};


class InstructionTable {
 public:
  InstructionTable();
  const InstructionDesc& Get(byte x) const {
    return instructions_[x];
  }

 private:
  InstructionDesc instructions_[256];
  void Clear();
  void Init();
  void CopyTable(ByteMnemonic bm[], InstructionType type);
  void SetTableRange(InstructionType type, byte start, byte end, bool byte_size,
                     const char* mnem);
  void AddJumpConditionalShort();
};


InstructionTable::InstructionTable() {
  Clear();
  Init();
}


void InstructionTable::Clear() {
  for (int i = 0; i < 256; i++) {
    instructions_[i].mnem = "(bad)";
    instructions_[i].type = NO_INSTR;
    instructions_[i].op_order_ = UNSET_OP_ORDER;
    instructions_[i].byte_size_operation = false;
  }
}


void InstructionTable::Init() {
  CopyTable(two_operands_instr, TWO_OPERANDS_INSTR);
  CopyTable(zero_operands_instr, ZERO_OPERANDS_INSTR);
  CopyTable(call_jump_instr, CALL_JUMP_INSTR);
  CopyTable(short_immediate_instr, SHORT_IMMEDIATE_INSTR);
  AddJumpConditionalShort();
  SetTableRange(PUSHPOP_INSTR, 0x50, 0x57, false, "push");
  SetTableRange(PUSHPOP_INSTR, 0x58, 0x5F, false, "pop");
  SetTableRange(MOVE_REG_INSTR, 0xB8, 0xBF, false, "mov");
}


void InstructionTable::CopyTable(ByteMnemonic bm[], InstructionType type) {
  for (int i = 0; bm[i].b >= 0; i++) {
    InstructionDesc* id = &instructions_[bm[i].b];
    id->mnem = bm[i].mnem;
    OperandType op_order = bm[i].op_order_;
    id->op_order_ =
        static_cast<OperandType>(op_order & ~BYTE_SIZE_OPERAND_FLAG);
    ASSERT_EQ(NO_INSTR, id->type);  // Information not already entered
    id->type = type;
    id->byte_size_operation = ((op_order & BYTE_SIZE_OPERAND_FLAG) != 0);
  }
}


void InstructionTable::SetTableRange(InstructionType type,
                                     byte start,
                                     byte end,
                                     bool byte_size,
                                     const char* mnem) {
  for (byte b = start; b <= end; b++) {
    InstructionDesc* id = &instructions_[b];
    ASSERT_EQ(NO_INSTR, id->type);  // Information not already entered
    id->mnem = mnem;
    id->type = type;
    id->byte_size_operation = byte_size;
  }
}


void InstructionTable::AddJumpConditionalShort() {
  for (byte b = 0x70; b <= 0x7F; b++) {
    InstructionDesc* id = &instructions_[b];
    ASSERT_EQ(NO_INSTR, id->type);  // Information not already entered
    id->mnem = NULL;  // Computed depending on condition code.
    id->type = JUMP_CONDITIONAL_SHORT_INSTR;
  }
}


static InstructionTable instruction_table;

static InstructionDesc cmov_instructions[16] = {
  {"cmovo", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovno", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovc", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovnc", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovz", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovnz", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovna", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmova", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovs", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovns", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovpe", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovpo", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovl", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovge", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovle", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false},
  {"cmovg", TWO_OPERANDS_INSTR, REG_OPER_OP_ORDER, false}
};

//------------------------------------------------------------------------------
// DisassemblerX64 implementation.

enum UnimplementedOpcodeAction {
  CONTINUE_ON_UNIMPLEMENTED_OPCODE,
  ABORT_ON_UNIMPLEMENTED_OPCODE
};

// A new DisassemblerX64 object is created to disassemble each instruction.
// The object can only disassemble a single instruction.
class DisassemblerX64 {
 public:
  DisassemblerX64(const NameConverter& converter,
                  UnimplementedOpcodeAction unimplemented_action =
                      ABORT_ON_UNIMPLEMENTED_OPCODE)
      : converter_(converter),
        tmp_buffer_pos_(0),
        abort_on_unimplemented_(
            unimplemented_action == ABORT_ON_UNIMPLEMENTED_OPCODE),
        rex_(0),
        operand_size_(0),
        group_1_prefix_(0),
        byte_size_operand_(false) {
    tmp_buffer_[0] = '\0';
  }

  virtual ~DisassemblerX64() {
  }

  // Writes one disassembled instruction into 'buffer' (0-terminated).
  // Returns the length of the disassembled machine instruction in bytes.
  int InstructionDecode(v8::internal::Vector<char> buffer, byte* instruction);

 private:
  enum OperandSize {
    BYTE_SIZE = 0,
    WORD_SIZE = 1,
    DOUBLEWORD_SIZE = 2,
    QUADWORD_SIZE = 3
  };

  const NameConverter& converter_;
  v8::internal::EmbeddedVector<char, 128> tmp_buffer_;
  unsigned int tmp_buffer_pos_;
  bool abort_on_unimplemented_;
  // Prefixes parsed
  byte rex_;
  byte operand_size_;  // 0x66 or (if no group 3 prefix is present) 0x0.
  byte group_1_prefix_;  // 0xF2, 0xF3, or (if no group 1 prefix is present) 0.
  // Byte size operand override.
  bool byte_size_operand_;

  void setRex(byte rex) {
    ASSERT_EQ(0x40, rex & 0xF0);
    rex_ = rex;
  }

  bool rex() { return rex_ != 0; }

  bool rex_b() { return (rex_ & 0x01) != 0; }

  // Actual number of base register given the low bits and the rex.b state.
  int base_reg(int low_bits) { return low_bits | ((rex_ & 0x01) << 3); }

  bool rex_x() { return (rex_ & 0x02) != 0; }

  bool rex_r() { return (rex_ & 0x04) != 0; }

  bool rex_w() { return (rex_ & 0x08) != 0; }

  OperandSize operand_size() {
    if (byte_size_operand_) return BYTE_SIZE;
    if (rex_w()) return QUADWORD_SIZE;
    if (operand_size_ != 0) return WORD_SIZE;
    return DOUBLEWORD_SIZE;
  }

  char operand_size_code() {
    return "bwlq"[operand_size()];
  }

  const char* NameOfCPURegister(int reg) const {
    return converter_.NameOfCPURegister(reg);
  }

  const char* NameOfByteCPURegister(int reg) const {
    return converter_.NameOfByteCPURegister(reg);
  }

  const char* NameOfXMMRegister(int reg) const {
    return converter_.NameOfXMMRegister(reg);
  }

  const char* NameOfAddress(byte* addr) const {
    return converter_.NameOfAddress(addr);
  }

  // Disassembler helper functions.
  void get_modrm(byte data,
                 int* mod,
                 int* regop,
                 int* rm) {
    *mod = (data >> 6) & 3;
    *regop = ((data & 0x38) >> 3) | (rex_r() ? 8 : 0);
    *rm = (data & 7) | (rex_b() ? 8 : 0);
  }

  void get_sib(byte data,
               int* scale,
               int* index,
               int* base) {
    *scale = (data >> 6) & 3;
    *index = ((data >> 3) & 7) | (rex_x() ? 8 : 0);
    *base = (data & 7) | (rex_b() ? 8 : 0);
  }

  typedef const char* (DisassemblerX64::*RegisterNameMapping)(int reg) const;

  int PrintRightOperandHelper(byte* modrmp,
                              RegisterNameMapping register_name);
  int PrintRightOperand(byte* modrmp);
  int PrintRightByteOperand(byte* modrmp);
  int PrintRightXMMOperand(byte* modrmp);
  int PrintOperands(const char* mnem,
                    OperandType op_order,
                    byte* data);
  int PrintImmediate(byte* data, OperandSize size);
  int PrintImmediateOp(byte* data);
  const char* TwoByteMnemonic(byte opcode);
  int TwoByteOpcodeInstruction(byte* data);
  int F6F7Instruction(byte* data);
  int ShiftInstruction(byte* data);
  int JumpShort(byte* data);
  int JumpConditional(byte* data);
  int JumpConditionalShort(byte* data);
  int SetCC(byte* data);
  int FPUInstruction(byte* data);
  int MemoryFPUInstruction(int escape_opcode, int regop, byte* modrm_start);
  int RegisterFPUInstruction(int escape_opcode, byte modrm_byte);
  void AppendToBuffer(const char* format, ...);

  void UnimplementedInstruction() {
    if (abort_on_unimplemented_) {
      CHECK(false);
    } else {
      AppendToBuffer("'Unimplemented Instruction'");
    }
  }
};


void DisassemblerX64::AppendToBuffer(const char* format, ...) {
  v8::internal::Vector<char> buf = tmp_buffer_ + tmp_buffer_pos_;
  va_list args;
  va_start(args, format);
  int result = v8::internal::OS::VSNPrintF(buf, format, args);
  va_end(args);
  tmp_buffer_pos_ += result;
}


int DisassemblerX64::PrintRightOperandHelper(
    byte* modrmp,
    RegisterNameMapping register_name) {
  int mod, regop, rm;
  get_modrm(*modrmp, &mod, &regop, &rm);
  switch (mod) {
    case 0:
      if ((rm & 7) == 5) {
        int32_t disp = *reinterpret_cast<int32_t*>(modrmp + 1);
        AppendToBuffer("[0x%x]", disp);
        return 5;
      } else if ((rm & 7) == 4) {
        // Codes for SIB byte.
        byte sib = *(modrmp + 1);
        int scale, index, base;
        get_sib(sib, &scale, &index, &base);
        if (index == 4 && (base & 7) == 4 && scale == 0 /*times_1*/) {
          // index == rsp means no index. Only use sib byte with no index for
          // rsp and r12 base.
          AppendToBuffer("[%s]", NameOfCPURegister(base));
          return 2;
        } else if (base == 5) {
          // base == rbp means no base register (when mod == 0).
          int32_t disp = *reinterpret_cast<int32_t*>(modrmp + 2);
          AppendToBuffer("[%s*%d+0x%x]",
                         NameOfCPURegister(index),
                         1 << scale, disp);
          return 6;
        } else if (index != 4 && base != 5) {
          // [base+index*scale]
          AppendToBuffer("[%s+%s*%d]",
                         NameOfCPURegister(base),
                         NameOfCPURegister(index),
                         1 << scale);
          return 2;
        } else {
          UnimplementedInstruction();
          return 1;
        }
      } else {
        AppendToBuffer("[%s]", NameOfCPURegister(rm));
        return 1;
      }
      break;
    case 1:  // fall through
    case 2:
      if ((rm & 7) == 4) {
        byte sib = *(modrmp + 1);
        int scale, index, base;
        get_sib(sib, &scale, &index, &base);
        int disp = (mod == 2) ? *reinterpret_cast<int32_t*>(modrmp + 2)
                              : *reinterpret_cast<char*>(modrmp + 2);
        if (index == 4 && (base & 7) == 4 && scale == 0 /*times_1*/) {
          if (-disp > 0) {
            AppendToBuffer("[%s-0x%x]", NameOfCPURegister(base), -disp);
          } else {
            AppendToBuffer("[%s+0x%x]", NameOfCPURegister(base), disp);
          }
        } else {
          if (-disp > 0) {
            AppendToBuffer("[%s+%s*%d-0x%x]",
                           NameOfCPURegister(base),
                           NameOfCPURegister(index),
                           1 << scale,
                           -disp);
          } else {
            AppendToBuffer("[%s+%s*%d+0x%x]",
                           NameOfCPURegister(base),
                           NameOfCPURegister(index),
                           1 << scale,
                           disp);
          }
        }
        return mod == 2 ? 6 : 3;
      } else {
        // No sib.
        int disp = (mod == 2) ? *reinterpret_cast<int32_t*>(modrmp + 1)
                              : *reinterpret_cast<char*>(modrmp + 1);
        if (-disp > 0) {
        AppendToBuffer("[%s-0x%x]", NameOfCPURegister(rm), -disp);
        } else {
        AppendToBuffer("[%s+0x%x]", NameOfCPURegister(rm), disp);
        }
        return (mod == 2) ? 5 : 2;
      }
      break;
    case 3:
      AppendToBuffer("%s", (this->*register_name)(rm));
      return 1;
    default:
      UnimplementedInstruction();
      return 1;
  }
  UNREACHABLE();
}


int DisassemblerX64::PrintImmediate(byte* data, OperandSize size) {
  int64_t value;
  int count;
  switch (size) {
    case BYTE_SIZE:
      value = *data;
      count = 1;
      break;
    case WORD_SIZE:
      value = *reinterpret_cast<int16_t*>(data);
      count = 2;
      break;
    case DOUBLEWORD_SIZE:
      value = *reinterpret_cast<uint32_t*>(data);
      count = 4;
      break;
    case QUADWORD_SIZE:
      value = *reinterpret_cast<int32_t*>(data);
      count = 4;
      break;
    default:
      UNREACHABLE();
      value = 0;  // Initialize variables on all paths to satisfy the compiler.
      count = 0;
  }
  AppendToBuffer("%" V8_PTR_PREFIX "x", value);
  return count;
}


int DisassemblerX64::PrintRightOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp,
                                 &DisassemblerX64::NameOfCPURegister);
}


int DisassemblerX64::PrintRightByteOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp,
                                 &DisassemblerX64::NameOfByteCPURegister);
}


int DisassemblerX64::PrintRightXMMOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp,
                                 &DisassemblerX64::NameOfXMMRegister);
}


// Returns number of bytes used including the current *data.
// Writes instruction's mnemonic, left and right operands to 'tmp_buffer_'.
int DisassemblerX64::PrintOperands(const char* mnem,
                                   OperandType op_order,
                                   byte* data) {
  byte modrm = *data;
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  int advance = 0;
  const char* register_name =
      byte_size_operand_ ? NameOfByteCPURegister(regop)
                         : NameOfCPURegister(regop);
  switch (op_order) {
    case REG_OPER_OP_ORDER: {
      AppendToBuffer("%s%c %s,",
                     mnem,
                     operand_size_code(),
                     register_name);
      advance = byte_size_operand_ ? PrintRightByteOperand(data)
                                   : PrintRightOperand(data);
      break;
    }
    case OPER_REG_OP_ORDER: {
      AppendToBuffer("%s%c ", mnem, operand_size_code());
      advance = byte_size_operand_ ? PrintRightByteOperand(data)
                                   : PrintRightOperand(data);
      AppendToBuffer(",%s", register_name);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return advance;
}


// Returns number of bytes used by machine instruction, including *data byte.
// Writes immediate instructions to 'tmp_buffer_'.
int DisassemblerX64::PrintImmediateOp(byte* data) {
  bool byte_size_immediate = (*data & 0x02) != 0;
  byte modrm = *(data + 1);
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  const char* mnem = "Imm???";
  switch (regop) {
    case 0:
      mnem = "add";
      break;
    case 1:
      mnem = "or";
      break;
    case 2:
      mnem = "adc";
      break;
    case 4:
      mnem = "and";
      break;
    case 5:
      mnem = "sub";
      break;
    case 6:
      mnem = "xor";
      break;
    case 7:
      mnem = "cmp";
      break;
    default:
      UnimplementedInstruction();
  }
  AppendToBuffer("%s%c ", mnem, operand_size_code());
  int count = PrintRightOperand(data + 1);
  AppendToBuffer(",0x");
  OperandSize immediate_size = byte_size_immediate ? BYTE_SIZE : operand_size();
  count += PrintImmediate(data + 1 + count, immediate_size);
  return 1 + count;
}


// Returns number of bytes used, including *data.
int DisassemblerX64::F6F7Instruction(byte* data) {
  ASSERT(*data == 0xF7 || *data == 0xF6);
  byte modrm = *(data + 1);
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  if (mod == 3 && regop != 0) {
    const char* mnem = NULL;
    switch (regop) {
      case 2:
        mnem = "not";
        break;
      case 3:
        mnem = "neg";
        break;
      case 4:
        mnem = "mul";
        break;
      case 7:
        mnem = "idiv";
        break;
      default:
        UnimplementedInstruction();
    }
    AppendToBuffer("%s%c %s",
                   mnem,
                   operand_size_code(),
                   NameOfCPURegister(rm));
    return 2;
  } else if (regop == 0) {
    AppendToBuffer("test%c ", operand_size_code());
    int count = PrintRightOperand(data + 1);  // Use name of 64-bit register.
    AppendToBuffer(",0x");
    count += PrintImmediate(data + 1 + count, operand_size());
    return 1 + count;
  } else {
    UnimplementedInstruction();
    return 2;
  }
}


int DisassemblerX64::ShiftInstruction(byte* data) {
  byte op = *data & (~1);
  if (op != 0xD0 && op != 0xD2 && op != 0xC0) {
    UnimplementedInstruction();
    return 1;
  }
  byte modrm = *(data + 1);
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  regop &= 0x7;  // The REX.R bit does not affect the operation.
  int imm8 = -1;
  int num_bytes = 2;
  if (mod != 3) {
    UnimplementedInstruction();
    return num_bytes;
  }
  const char* mnem = NULL;
  switch (regop) {
    case 0:
      mnem = "rol";
      break;
    case 1:
      mnem = "ror";
      break;
    case 2:
      mnem = "rcl";
      break;
    case 3:
      mnem = "rcr";
      break;
    case 4:
      mnem = "shl";
      break;
    case 5:
      mnem = "shr";
      break;
    case 7:
      mnem = "sar";
      break;
    default:
      UnimplementedInstruction();
      return num_bytes;
  }
  ASSERT_NE(NULL, mnem);
  if (op == 0xD0) {
    imm8 = 1;
  } else if (op == 0xC0) {
    imm8 = *(data + 2);
    num_bytes = 3;
  }
  AppendToBuffer("%s%c %s,",
                 mnem,
                 operand_size_code(),
                 byte_size_operand_ ? NameOfByteCPURegister(rm)
                                    : NameOfCPURegister(rm));
  if (op == 0xD2) {
    AppendToBuffer("cl");
  } else {
    AppendToBuffer("%d", imm8);
  }
  return num_bytes;
}


// Returns number of bytes used, including *data.
int DisassemblerX64::JumpShort(byte* data) {
  ASSERT_EQ(0xEB, *data);
  byte b = *(data + 1);
  byte* dest = data + static_cast<int8_t>(b) + 2;
  AppendToBuffer("jmp %s", NameOfAddress(dest));
  return 2;
}


// Returns number of bytes used, including *data.
int DisassemblerX64::JumpConditional(byte* data) {
  ASSERT_EQ(0x0F, *data);
  byte cond = *(data + 1) & 0x0F;
  byte* dest = data + *reinterpret_cast<int32_t*>(data + 2) + 6;
  const char* mnem = conditional_code_suffix[cond];
  AppendToBuffer("j%s %s", mnem, NameOfAddress(dest));
  return 6;  // includes 0x0F
}


// Returns number of bytes used, including *data.
int DisassemblerX64::JumpConditionalShort(byte* data) {
  byte cond = *data & 0x0F;
  byte b = *(data + 1);
  byte* dest = data + static_cast<int8_t>(b) + 2;
  const char* mnem = conditional_code_suffix[cond];
  AppendToBuffer("j%s %s", mnem, NameOfAddress(dest));
  return 2;
}


// Returns number of bytes used, including *data.
int DisassemblerX64::SetCC(byte* data) {
  ASSERT_EQ(0x0F, *data);
  byte cond = *(data + 1) & 0x0F;
  const char* mnem = conditional_code_suffix[cond];
  AppendToBuffer("set%s%c ", mnem, operand_size_code());
  PrintRightByteOperand(data + 2);
  return 3;  // includes 0x0F
}


// Returns number of bytes used, including *data.
int DisassemblerX64::FPUInstruction(byte* data) {
  byte escape_opcode = *data;
  ASSERT_EQ(0xD8, escape_opcode & 0xF8);
  byte modrm_byte = *(data+1);

  if (modrm_byte >= 0xC0) {
    return RegisterFPUInstruction(escape_opcode, modrm_byte);
  } else {
    return MemoryFPUInstruction(escape_opcode, modrm_byte, data+1);
  }
}

int DisassemblerX64::MemoryFPUInstruction(int escape_opcode,
                                           int modrm_byte,
                                           byte* modrm_start) {
  const char* mnem = "?";
  int regop = (modrm_byte >> 3) & 0x7;  // reg/op field of modrm byte.
  switch (escape_opcode) {
    case 0xD9: switch (regop) {
        case 0: mnem = "fld_s"; break;
        case 3: mnem = "fstp_s"; break;
        case 7: mnem = "fstcw"; break;
        default: UnimplementedInstruction();
      }
      break;

    case 0xDB: switch (regop) {
        case 0: mnem = "fild_s"; break;
        case 1: mnem = "fisttp_s"; break;
        case 2: mnem = "fist_s"; break;
        case 3: mnem = "fistp_s"; break;
        default: UnimplementedInstruction();
      }
      break;

    case 0xDD: switch (regop) {
        case 0: mnem = "fld_d"; break;
        case 3: mnem = "fstp_d"; break;
        default: UnimplementedInstruction();
      }
      break;

    case 0xDF: switch (regop) {
        case 5: mnem = "fild_d"; break;
        case 7: mnem = "fistp_d"; break;
        default: UnimplementedInstruction();
      }
      break;

    default: UnimplementedInstruction();
  }
  AppendToBuffer("%s ", mnem);
  int count = PrintRightOperand(modrm_start);
  return count + 1;
}

int DisassemblerX64::RegisterFPUInstruction(int escape_opcode,
                                             byte modrm_byte) {
  bool has_register = false;  // Is the FPU register encoded in modrm_byte?
  const char* mnem = "?";

  switch (escape_opcode) {
    case 0xD8:
      UnimplementedInstruction();
      break;

    case 0xD9:
      switch (modrm_byte & 0xF8) {
        case 0xC0:
          mnem = "fld";
          has_register = true;
          break;
        case 0xC8:
          mnem = "fxch";
          has_register = true;
          break;
        default:
          switch (modrm_byte) {
            case 0xE0: mnem = "fchs"; break;
            case 0xE1: mnem = "fabs"; break;
            case 0xE4: mnem = "ftst"; break;
            case 0xE8: mnem = "fld1"; break;
            case 0xEB: mnem = "fldpi"; break;
            case 0xEE: mnem = "fldz"; break;
            case 0xF5: mnem = "fprem1"; break;
            case 0xF7: mnem = "fincstp"; break;
            case 0xF8: mnem = "fprem"; break;
            case 0xFE: mnem = "fsin"; break;
            case 0xFF: mnem = "fcos"; break;
            default: UnimplementedInstruction();
          }
      }
      break;

    case 0xDA:
      if (modrm_byte == 0xE9) {
        mnem = "fucompp";
      } else {
        UnimplementedInstruction();
      }
      break;

    case 0xDB:
      if ((modrm_byte & 0xF8) == 0xE8) {
        mnem = "fucomi";
        has_register = true;
      } else if (modrm_byte  == 0xE2) {
        mnem = "fclex";
      } else {
        UnimplementedInstruction();
      }
      break;

    case 0xDC:
      has_register = true;
      switch (modrm_byte & 0xF8) {
        case 0xC0: mnem = "fadd"; break;
        case 0xE8: mnem = "fsub"; break;
        case 0xC8: mnem = "fmul"; break;
        case 0xF8: mnem = "fdiv"; break;
        default: UnimplementedInstruction();
      }
      break;

    case 0xDD:
      has_register = true;
      switch (modrm_byte & 0xF8) {
        case 0xC0: mnem = "ffree"; break;
        case 0xD8: mnem = "fstp"; break;
        default: UnimplementedInstruction();
      }
      break;

    case 0xDE:
      if (modrm_byte  == 0xD9) {
        mnem = "fcompp";
      } else {
        has_register = true;
        switch (modrm_byte & 0xF8) {
          case 0xC0: mnem = "faddp"; break;
          case 0xE8: mnem = "fsubp"; break;
          case 0xC8: mnem = "fmulp"; break;
          case 0xF8: mnem = "fdivp"; break;
          default: UnimplementedInstruction();
        }
      }
      break;

    case 0xDF:
      if (modrm_byte == 0xE0) {
        mnem = "fnstsw_ax";
      } else if ((modrm_byte & 0xF8) == 0xE8) {
        mnem = "fucomip";
        has_register = true;
      }
      break;

    default: UnimplementedInstruction();
  }

  if (has_register) {
    AppendToBuffer("%s st%d", mnem, modrm_byte & 0x7);
  } else {
    AppendToBuffer("%s", mnem);
  }
  return 2;
}



// Handle all two-byte opcodes, which start with 0x0F.
// These instructions may be affected by an 0x66, 0xF2, or 0xF3 prefix.
// We do not use any three-byte opcodes, which start with 0x0F38 or 0x0F3A.
int DisassemblerX64::TwoByteOpcodeInstruction(byte* data) {
  byte opcode = *(data + 1);
  byte* current = data + 2;
  // At return, "current" points to the start of the next instruction.
  const char* mnemonic = TwoByteMnemonic(opcode);
  if (operand_size_ == 0x66) {
    // 0x66 0x0F prefix.
    int mod, regop, rm;
    if (opcode == 0x3A) {
      byte third_byte = *current;
      current = data + 3;
      if (third_byte == 0x17) {
        get_modrm(*current, &mod, &regop, &rm);
        AppendToBuffer("extractps ");  // reg/m32, xmm, imm8
        current += PrintRightOperand(current);
        AppendToBuffer(", %s, %d", NameOfCPURegister(regop), (*current) & 3);
        current += 1;
      } else {
        UnimplementedInstruction();
      }
    } else {
      get_modrm(*current, &mod, &regop, &rm);
      if (opcode == 0x6E) {
        AppendToBuffer("mov%c %s,",
                       rex_w() ? 'q' : 'd',
                       NameOfXMMRegister(regop));
        current += PrintRightOperand(current);
      } else if (opcode == 0x7E) {
        AppendToBuffer("mov%c ",
                       rex_w() ? 'q' : 'd');
        current += PrintRightOperand(current);
        AppendToBuffer(", %s", NameOfXMMRegister(regop));
      } else {
        const char* mnemonic = "?";
        if (opcode == 0x57) {
          mnemonic = "xorpd";
        } else if (opcode == 0x2E) {
          mnemonic = "ucomisd";
        } else if (opcode == 0x2F) {
          mnemonic = "comisd";
        } else {
          UnimplementedInstruction();
        }
        AppendToBuffer("%s %s,", mnemonic, NameOfXMMRegister(regop));
        current += PrintRightXMMOperand(current);
      }
    }
  } else if (group_1_prefix_ == 0xF2) {
    // Beginning of instructions with prefix 0xF2.

    if (opcode == 0x11 || opcode == 0x10) {
      // MOVSD: Move scalar double-precision fp to/from/between XMM registers.
      AppendToBuffer("movsd ");
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      if (opcode == 0x11) {
        current += PrintRightOperand(current);
        AppendToBuffer(",%s", NameOfXMMRegister(regop));
      } else {
        AppendToBuffer("%s,", NameOfXMMRegister(regop));
        current += PrintRightOperand(current);
      }
    } else if (opcode == 0x2A) {
      // CVTSI2SD: integer to XMM double conversion.
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      AppendToBuffer("%sd %s,", mnemonic, NameOfXMMRegister(regop));
      current += PrintRightOperand(current);
    } else if (opcode == 0x2C) {
      // CVTTSD2SI:
      // Convert with truncation scalar double-precision FP to integer.
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      AppendToBuffer("cvttsd2si%c %s,",
          operand_size_code(), NameOfCPURegister(regop));
      current += PrintRightXMMOperand(current);
    } else if (opcode == 0x2D) {
      // CVTSD2SI: Convert scalar double-precision FP to integer.
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      AppendToBuffer("cvtsd2si%c %s,",
          operand_size_code(), NameOfCPURegister(regop));
      current += PrintRightXMMOperand(current);
    } else if ((opcode & 0xF8) == 0x58 || opcode == 0x51) {
      // XMM arithmetic. Mnemonic was retrieved at the start of this function.
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      AppendToBuffer("%s %s,", mnemonic, NameOfXMMRegister(regop));
      current += PrintRightXMMOperand(current);
    } else {
      UnimplementedInstruction();
    }
  } else if (group_1_prefix_ == 0xF3) {
    // Instructions with prefix 0xF3.
    if (opcode == 0x11 || opcode == 0x10) {
      // MOVSS: Move scalar double-precision fp to/from/between XMM registers.
      AppendToBuffer("movss ");
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      if (opcode == 0x11) {
        current += PrintRightOperand(current);
        AppendToBuffer(",%s", NameOfXMMRegister(regop));
      } else {
        AppendToBuffer("%s,", NameOfXMMRegister(regop));
        current += PrintRightOperand(current);
      }
    } else if (opcode == 0x2A) {
      // CVTSI2SS: integer to XMM single conversion.
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      AppendToBuffer("%ss %s,", mnemonic, NameOfXMMRegister(regop));
      current += PrintRightOperand(current);
    } else if (opcode == 0x2C) {
      // CVTTSS2SI:
      // Convert with truncation scalar single-precision FP to dword integer.
      // Assert that mod is not 3, so source is memory, not an XMM register.
      ASSERT_NE(0xC0, *current & 0xC0);
      current += PrintOperands("cvttss2si", REG_OPER_OP_ORDER, current);
    } else if (opcode == 0x5A) {
      // CVTSS2SD:
      // Convert scalar single-precision FP to scalar double-precision FP.
      int mod, regop, rm;
      get_modrm(*current, &mod, &regop, &rm);
      AppendToBuffer("cvtss2sd %s,", NameOfXMMRegister(regop));
      current += PrintRightXMMOperand(current);
    } else {
      UnimplementedInstruction();
    }
  } else if (opcode == 0x1F) {
    // NOP
    int mod, regop, rm;
    get_modrm(*current, &mod, &regop, &rm);
    current++;
    if (regop == 4) {  // SIB byte present.
      current++;
    }
    if (mod == 1) {  // Byte displacement.
      current += 1;
    } else if (mod == 2) {  // 32-bit displacement.
      current += 4;
    }  // else no immediate displacement.
    AppendToBuffer("nop");
  } else if (opcode == 0xA2 || opcode == 0x31) {
    // RDTSC or CPUID
    AppendToBuffer("%s", mnemonic);

  } else if ((opcode & 0xF0) == 0x40) {
    // CMOVcc: conditional move.
    int condition = opcode & 0x0F;
    const InstructionDesc& idesc = cmov_instructions[condition];
    byte_size_operand_ = idesc.byte_size_operation;
    current += PrintOperands(idesc.mnem, idesc.op_order_, current);

  } else if ((opcode & 0xF0) == 0x80) {
    // Jcc: Conditional jump (branch).
    current = data + JumpConditional(data);

  } else if (opcode == 0xBE || opcode == 0xBF || opcode == 0xB6 ||
             opcode == 0xB7 || opcode == 0xAF) {
    // Size-extending moves, IMUL.
    current += PrintOperands(mnemonic, REG_OPER_OP_ORDER, current);

  } else if ((opcode & 0xF0) == 0x90) {
    // SETcc: Set byte on condition. Needs pointer to beginning of instruction.
    current = data + SetCC(data);

  } else if (opcode == 0xAB || opcode == 0xA5 || opcode == 0xAD) {
    // SHLD, SHRD (double-precision shift), BTS (bit set).
    AppendToBuffer("%s ", mnemonic);
    int mod, regop, rm;
    get_modrm(*current, &mod, &regop, &rm);
    current += PrintRightOperand(current);
    if (opcode == 0xAB) {
      AppendToBuffer(",%s", NameOfCPURegister(regop));
    } else {
      AppendToBuffer(",%s,cl", NameOfCPURegister(regop));
    }
  } else {
    UnimplementedInstruction();
  }
  return static_cast<int>(current - data);
}


// Mnemonics for two-byte opcode instructions starting with 0x0F.
// The argument is the second byte of the two-byte opcode.
// Returns NULL if the instruction is not handled here.
const char* DisassemblerX64::TwoByteMnemonic(byte opcode) {
  switch (opcode) {
    case 0x1F:
      return "nop";
    case 0x2A:  // F2/F3 prefix.
      return "cvtsi2s";
    case 0x31:
      return "rdtsc";
    case 0x51:  // F2 prefix.
      return "sqrtsd";
    case 0x58:  // F2 prefix.
      return "addsd";
    case 0x59:  // F2 prefix.
      return "mulsd";
    case 0x5C:  // F2 prefix.
      return "subsd";
    case 0x5E:  // F2 prefix.
      return "divsd";
    case 0xA2:
      return "cpuid";
    case 0xA5:
      return "shld";
    case 0xAB:
      return "bts";
    case 0xAD:
      return "shrd";
    case 0xAF:
      return "imul";
    case 0xB6:
      return "movzxb";
    case 0xB7:
      return "movzxw";
    case 0xBE:
      return "movsxb";
    case 0xBF:
      return "movsxw";
    default:
      return NULL;
  }
}


// Disassembles the instruction at instr, and writes it into out_buffer.
int DisassemblerX64::InstructionDecode(v8::internal::Vector<char> out_buffer,
                                       byte* instr) {
  tmp_buffer_pos_ = 0;  // starting to write as position 0
  byte* data = instr;
  bool processed = true;  // Will be set to false if the current instruction
                          // is not in 'instructions' table.
  byte current;

  // Scan for prefixes.
  while (true) {
    current = *data;
    if (current == OPERAND_SIZE_OVERRIDE_PREFIX) {  // Group 3 prefix.
      operand_size_ = current;
    } else if ((current & 0xF0) == 0x40) {  // REX prefix.
      setRex(current);
      if (rex_w()) AppendToBuffer("REX.W ");
    } else if ((current & 0xFE) == 0xF2) {  // Group 1 prefix (0xF2 or 0xF3).
      group_1_prefix_ = current;
    } else {  // Not a prefix - an opcode.
      break;
    }
    data++;
  }

  const InstructionDesc& idesc = instruction_table.Get(current);
  byte_size_operand_ = idesc.byte_size_operation;
  switch (idesc.type) {
    case ZERO_OPERANDS_INSTR:
      if (current >= 0xA4 && current <= 0xA7) {
        // String move or compare operations.
        if (group_1_prefix_ == REP_PREFIX) {
          // REP.
          AppendToBuffer("rep ");
        }
        if (rex_w()) AppendToBuffer("REX.W ");
        AppendToBuffer("%s%c", idesc.mnem, operand_size_code());
      } else {
        AppendToBuffer("%s", idesc.mnem, operand_size_code());
      }
      data++;
      break;

    case TWO_OPERANDS_INSTR:
      data++;
      data += PrintOperands(idesc.mnem, idesc.op_order_, data);
      break;

    case JUMP_CONDITIONAL_SHORT_INSTR:
      data += JumpConditionalShort(data);
      break;

    case REGISTER_INSTR:
      AppendToBuffer("%s%c %s",
                     idesc.mnem,
                     operand_size_code(),
                     NameOfCPURegister(base_reg(current & 0x07)));
      data++;
      break;
    case PUSHPOP_INSTR:
      AppendToBuffer("%s %s",
                     idesc.mnem,
                     NameOfCPURegister(base_reg(current & 0x07)));
      data++;
      break;
    case MOVE_REG_INSTR: {
      byte* addr = NULL;
      switch (operand_size()) {
        case WORD_SIZE:
          addr = reinterpret_cast<byte*>(*reinterpret_cast<int16_t*>(data + 1));
          data += 3;
          break;
        case DOUBLEWORD_SIZE:
          addr = reinterpret_cast<byte*>(*reinterpret_cast<int32_t*>(data + 1));
          data += 5;
          break;
        case QUADWORD_SIZE:
          addr = reinterpret_cast<byte*>(*reinterpret_cast<int64_t*>(data + 1));
          data += 9;
          break;
        default:
          UNREACHABLE();
      }
      AppendToBuffer("mov%c %s,%s",
                     operand_size_code(),
                     NameOfCPURegister(base_reg(current & 0x07)),
                     NameOfAddress(addr));
      break;
    }

    case CALL_JUMP_INSTR: {
      byte* addr = data + *reinterpret_cast<int32_t*>(data + 1) + 5;
      AppendToBuffer("%s %s", idesc.mnem, NameOfAddress(addr));
      data += 5;
      break;
    }

    case SHORT_IMMEDIATE_INSTR: {
      byte* addr =
          reinterpret_cast<byte*>(*reinterpret_cast<int32_t*>(data + 1));
      AppendToBuffer("%s rax, %s", idesc.mnem, NameOfAddress(addr));
      data += 5;
      break;
    }

    case NO_INSTR:
      processed = false;
      break;

    default:
      UNIMPLEMENTED();  // This type is not implemented.
  }

  // The first byte didn't match any of the simple opcodes, so we
  // need to do special processing on it.
  if (!processed) {
    switch (*data) {
      case 0xC2:
        AppendToBuffer("ret 0x%x", *reinterpret_cast<uint16_t*>(data + 1));
        data += 3;
        break;

      case 0x69:  // fall through
      case 0x6B: {
        int mod, regop, rm;
        get_modrm(*(data + 1), &mod, &regop, &rm);
        int32_t imm = *data == 0x6B ? *(data + 2)
            : *reinterpret_cast<int32_t*>(data + 2);
        AppendToBuffer("imul%c %s,%s,0x%x",
                       operand_size_code(),
                       NameOfCPURegister(regop),
                       NameOfCPURegister(rm), imm);
        data += 2 + (*data == 0x6B ? 1 : 4);
        break;
      }

      case 0x81:  // fall through
      case 0x83:  // 0x81 with sign extension bit set
        data += PrintImmediateOp(data);
        break;

      case 0x0F:
        data += TwoByteOpcodeInstruction(data);
        break;

      case 0x8F: {
        data++;
        int mod, regop, rm;
        get_modrm(*data, &mod, &regop, &rm);
        if (regop == 0) {
          AppendToBuffer("pop ");
          data += PrintRightOperand(data);
        }
      }
        break;

      case 0xFF: {
        data++;
        int mod, regop, rm;
        get_modrm(*data, &mod, &regop, &rm);
        const char* mnem = NULL;
        switch (regop) {
          case 0:
            mnem = "inc";
            break;
          case 1:
            mnem = "dec";
            break;
          case 2:
            mnem = "call";
            break;
          case 4:
            mnem = "jmp";
            break;
          case 6:
            mnem = "push";
            break;
          default:
            mnem = "???";
        }
        AppendToBuffer(((regop <= 1) ? "%s%c " : "%s "),
                       mnem,
                       operand_size_code());
        data += PrintRightOperand(data);
      }
        break;

      case 0xC7:  // imm32, fall through
      case 0xC6:  // imm8
      {
        bool is_byte = *data == 0xC6;
        data++;

        AppendToBuffer("mov%c ", is_byte ? 'b' : operand_size_code());
        data += PrintRightOperand(data);
        int32_t imm = is_byte ? *data : *reinterpret_cast<int32_t*>(data);
        AppendToBuffer(",0x%x", imm);
        data += is_byte ? 1 : 4;
      }
        break;

      case 0x80: {
        data++;
        AppendToBuffer("cmpb ");
        data += PrintRightOperand(data);
        int32_t imm = *data;
        AppendToBuffer(",0x%x", imm);
        data++;
      }
        break;

      case 0x88:  // 8bit, fall through
      case 0x89:  // 32bit
      {
        bool is_byte = *data == 0x88;
        int mod, regop, rm;
        data++;
        get_modrm(*data, &mod, &regop, &rm);
        AppendToBuffer("mov%c ", is_byte ? 'b' : operand_size_code());
        data += PrintRightOperand(data);
        AppendToBuffer(",%s", NameOfCPURegister(regop));
      }
        break;

      case 0x90:
      case 0x91:
      case 0x92:
      case 0x93:
      case 0x94:
      case 0x95:
      case 0x96:
      case 0x97: {
        int reg = (*data & 0x7) | (rex_b() ? 8 : 0);
        if (reg == 0) {
          AppendToBuffer("nop");  // Common name for xchg rax,rax.
        } else {
          AppendToBuffer("xchg%c rax, %s",
                         operand_size_code(),
                         NameOfCPURegister(reg));
        }
        data++;
      }
        break;

      case 0xFE: {
        data++;
        int mod, regop, rm;
        get_modrm(*data, &mod, &regop, &rm);
        if (regop == 1) {
          AppendToBuffer("decb ");
          data += PrintRightOperand(data);
        } else {
          UnimplementedInstruction();
        }
      }
        break;

      case 0x68:
        AppendToBuffer("push 0x%x", *reinterpret_cast<int32_t*>(data + 1));
        data += 5;
        break;

      case 0x6A:
        AppendToBuffer("push 0x%x", *reinterpret_cast<int8_t*>(data + 1));
        data += 2;
        break;

      case 0xA1:  // Fall through.
      case 0xA3:
        switch (operand_size()) {
          case DOUBLEWORD_SIZE: {
            const char* memory_location = NameOfAddress(
                reinterpret_cast<byte*>(
                    *reinterpret_cast<int32_t*>(data + 1)));
            if (*data == 0xA1) {  // Opcode 0xA1
              AppendToBuffer("movzxlq rax,(%s)", memory_location);
            } else {  // Opcode 0xA3
              AppendToBuffer("movzxlq (%s),rax", memory_location);
            }
            data += 5;
            break;
          }
          case QUADWORD_SIZE: {
            // New x64 instruction mov rax,(imm_64).
            const char* memory_location = NameOfAddress(
                *reinterpret_cast<byte**>(data + 1));
            if (*data == 0xA1) {  // Opcode 0xA1
              AppendToBuffer("movq rax,(%s)", memory_location);
            } else {  // Opcode 0xA3
              AppendToBuffer("movq (%s),rax", memory_location);
            }
            data += 9;
            break;
          }
          default:
            UnimplementedInstruction();
            data += 2;
        }
        break;

      case 0xA8:
        AppendToBuffer("test al,0x%x", *reinterpret_cast<uint8_t*>(data + 1));
        data += 2;
        break;

      case 0xA9: {
        int64_t value = 0;
        switch (operand_size()) {
          case WORD_SIZE:
            value = *reinterpret_cast<uint16_t*>(data + 1);
            data += 3;
            break;
          case DOUBLEWORD_SIZE:
            value = *reinterpret_cast<uint32_t*>(data + 1);
            data += 5;
            break;
          case QUADWORD_SIZE:
            value = *reinterpret_cast<int32_t*>(data + 1);
            data += 5;
            break;
          default:
            UNREACHABLE();
        }
        AppendToBuffer("test%c rax,0x%"V8_PTR_PREFIX"x",
                       operand_size_code(),
                       value);
        break;
      }
      case 0xD1:  // fall through
      case 0xD3:  // fall through
      case 0xC1:
        data += ShiftInstruction(data);
        break;
      case 0xD0:  // fall through
      case 0xD2:  // fall through
      case 0xC0:
        byte_size_operand_ = true;
        data += ShiftInstruction(data);
        break;

      case 0xD9:  // fall through
      case 0xDA:  // fall through
      case 0xDB:  // fall through
      case 0xDC:  // fall through
      case 0xDD:  // fall through
      case 0xDE:  // fall through
      case 0xDF:
        data += FPUInstruction(data);
        break;

      case 0xEB:
        data += JumpShort(data);
        break;

      case 0xF6:
        byte_size_operand_ = true;  // fall through
      case 0xF7:
        data += F6F7Instruction(data);
        break;

      default:
        UnimplementedInstruction();
        data += 1;
    }
  }  // !processed

  if (tmp_buffer_pos_ < sizeof tmp_buffer_) {
    tmp_buffer_[tmp_buffer_pos_] = '\0';
  }

  int instr_len = static_cast<int>(data - instr);
  ASSERT(instr_len > 0);  // Ensure progress.

  int outp = 0;
  // Instruction bytes.
  for (byte* bp = instr; bp < data; bp++) {
    outp += v8::internal::OS::SNPrintF(out_buffer + outp, "%02x", *bp);
  }
  for (int i = 6 - instr_len; i >= 0; i--) {
    outp += v8::internal::OS::SNPrintF(out_buffer + outp, "  ");
  }

  outp += v8::internal::OS::SNPrintF(out_buffer + outp, " %s",
                                     tmp_buffer_.start());
  return instr_len;
}

//------------------------------------------------------------------------------


static const char* cpu_regs[16] = {
  "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
};


static const char* byte_cpu_regs[16] = {
  "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
  "r8l", "r9l", "r10l", "r11l", "r12l", "r13l", "r14l", "r15l"
};


static const char* xmm_regs[16] = {
  "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
  "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"
};


const char* NameConverter::NameOfAddress(byte* addr) const {
  static v8::internal::EmbeddedVector<char, 32> tmp_buffer;
  v8::internal::OS::SNPrintF(tmp_buffer, "%p", addr);
  return tmp_buffer.start();
}


const char* NameConverter::NameOfConstant(byte* addr) const {
  return NameOfAddress(addr);
}


const char* NameConverter::NameOfCPURegister(int reg) const {
  if (0 <= reg && reg < 16)
    return cpu_regs[reg];
  return "noreg";
}


const char* NameConverter::NameOfByteCPURegister(int reg) const {
  if (0 <= reg && reg < 16)
    return byte_cpu_regs[reg];
  return "noreg";
}


const char* NameConverter::NameOfXMMRegister(int reg) const {
  if (0 <= reg && reg < 16)
    return xmm_regs[reg];
  return "noxmmreg";
}


const char* NameConverter::NameInCode(byte* addr) const {
  // X64 does not embed debug strings at the moment.
  UNREACHABLE();
  return "";
}

//------------------------------------------------------------------------------

Disassembler::Disassembler(const NameConverter& converter)
    : converter_(converter) { }

Disassembler::~Disassembler() { }


int Disassembler::InstructionDecode(v8::internal::Vector<char> buffer,
                                    byte* instruction) {
  DisassemblerX64 d(converter_, CONTINUE_ON_UNIMPLEMENTED_OPCODE);
  return d.InstructionDecode(buffer, instruction);
}


// The X64 assembler does not use constant pools.
int Disassembler::ConstantPoolSizeAt(byte* instruction) {
  return -1;
}


void Disassembler::Disassemble(FILE* f, byte* begin, byte* end) {
  NameConverter converter;
  Disassembler d(converter);
  for (byte* pc = begin; pc < end;) {
    v8::internal::EmbeddedVector<char, 128> buffer;
    buffer[0] = '\0';
    byte* prev_pc = pc;
    pc += d.InstructionDecode(buffer, pc);
    fprintf(f, "%p", prev_pc);
    fprintf(f, "    ");

    for (byte* bp = prev_pc; bp < pc; bp++) {
      fprintf(f, "%02x", *bp);
    }
    for (int i = 6 - static_cast<int>(pc - prev_pc); i >= 0; i--) {
      fprintf(f, "  ");
    }
    fprintf(f, "  %s\n", buffer.start());
  }
}

}  // namespace disasm

#endif  // V8_TARGET_ARCH_X64

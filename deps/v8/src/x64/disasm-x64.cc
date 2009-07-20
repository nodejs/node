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
#include "disasm.h"

namespace disasm {

enum OperandOrder {
  UNSET_OP_ORDER = 0, REG_OPER_OP_ORDER, OPER_REG_OP_ORDER
};

//------------------------------------------------------------------
// Tables
//------------------------------------------------------------------
struct ByteMnemonic {
  int b;  // -1 terminates, otherwise must be in range (0..255)
  OperandOrder op_order_;
  const char* mnem;
};


static ByteMnemonic two_operands_instr[] = {
  { 0x03, REG_OPER_OP_ORDER, "add" },
  { 0x21, OPER_REG_OP_ORDER, "and" },
  { 0x23, REG_OPER_OP_ORDER, "and" },
  { 0x3B, REG_OPER_OP_ORDER, "cmp" },
  { 0x8D, REG_OPER_OP_ORDER, "lea" },
  { 0x09, OPER_REG_OP_ORDER, "or" },
  { 0x0B, REG_OPER_OP_ORDER, "or" },
  { 0x1B, REG_OPER_OP_ORDER, "sbb" },
  { 0x29, OPER_REG_OP_ORDER, "sub" },
  { 0x2B, REG_OPER_OP_ORDER, "sub" },
  { 0x85, REG_OPER_OP_ORDER, "test" },
  { 0x31, OPER_REG_OP_ORDER, "xor" },
  { 0x33, REG_OPER_OP_ORDER, "xor" },
  { 0x87, REG_OPER_OP_ORDER, "xchg" },
  { 0x8A, REG_OPER_OP_ORDER, "movb" },
  { 0x8B, REG_OPER_OP_ORDER, "mov" },
  { -1, UNSET_OP_ORDER, "" }
};


static ByteMnemonic zero_operands_instr[] = {
  { 0xC3, UNSET_OP_ORDER, "ret" },
  { 0xC9, UNSET_OP_ORDER, "leave" },
  { 0x90, UNSET_OP_ORDER, "nop" },
  { 0xF4, UNSET_OP_ORDER, "hlt" },
  { 0xCC, UNSET_OP_ORDER, "int3" },
  { 0x60, UNSET_OP_ORDER, "pushad" },
  { 0x61, UNSET_OP_ORDER, "popad" },
  { 0x9C, UNSET_OP_ORDER, "pushfd" },
  { 0x9D, UNSET_OP_ORDER, "popfd" },
  { 0x9E, UNSET_OP_ORDER, "sahf" },
  { 0x99, UNSET_OP_ORDER, "cdq" },
  { 0x9B, UNSET_OP_ORDER, "fwait" },
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
  { 0x25, UNSET_OP_ORDER, "and" },
  { 0x2D, UNSET_OP_ORDER, "sub" },
  { 0x35, UNSET_OP_ORDER, "xor" },
  { 0x3D, UNSET_OP_ORDER, "cmp" },
  { -1, UNSET_OP_ORDER, "" }
};


static const char* conditional_code_suffix[] = {
  "o", "no", "c", "nc", "z", "nz", "a", "na",
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


struct InstructionDesc {
  const char* mnem;
  InstructionType type;
  OperandOrder op_order_;
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
  void SetTableRange(InstructionType type, byte start, byte end,
                     const char* mnem);
  void AddJumpConditionalShort();
};


InstructionTable::InstructionTable() {
  Clear();
  Init();
}


void InstructionTable::Clear() {
  for (int i = 0; i < 256; i++) {
    instructions_[i].mnem = "";
    instructions_[i].type = NO_INSTR;
    instructions_[i].op_order_ = UNSET_OP_ORDER;
  }
}


void InstructionTable::Init() {
  CopyTable(two_operands_instr, TWO_OPERANDS_INSTR);
  CopyTable(zero_operands_instr, ZERO_OPERANDS_INSTR);
  CopyTable(call_jump_instr, CALL_JUMP_INSTR);
  CopyTable(short_immediate_instr, SHORT_IMMEDIATE_INSTR);
  AddJumpConditionalShort();
  SetTableRange(PUSHPOP_INSTR, 0x50, 0x57, "push");
  SetTableRange(PUSHPOP_INSTR, 0x58, 0x5F, "pop");
  SetTableRange(MOVE_REG_INSTR, 0xB8, 0xBF, "mov");
}


void InstructionTable::CopyTable(ByteMnemonic bm[], InstructionType type) {
  for (int i = 0; bm[i].b >= 0; i++) {
    InstructionDesc* id = &instructions_[bm[i].b];
    id->mnem = bm[i].mnem;
    id->op_order_ = bm[i].op_order_;
    assert(id->type == NO_INSTR);  // Information already entered
    id->type = type;
  }
}


void InstructionTable::SetTableRange(InstructionType type, byte start,
                                     byte end, const char* mnem) {
  for (byte b = start; b <= end; b++) {
    InstructionDesc* id = &instructions_[b];
    assert(id->type == NO_INSTR);  // Information already entered
    id->mnem = mnem;
    id->type = type;
  }
}


void InstructionTable::AddJumpConditionalShort() {
  for (byte b = 0x70; b <= 0x7F; b++) {
    InstructionDesc* id = &instructions_[b];
    assert(id->type == NO_INSTR);  // Information already entered
    id->mnem = NULL;  // Computed depending on condition code.
    id->type = JUMP_CONDITIONAL_SHORT_INSTR;
  }
}


static InstructionTable instruction_table;


// The X64 disassembler implementation.
enum UnimplementedOpcodeAction {
  CONTINUE_ON_UNIMPLEMENTED_OPCODE,
  ABORT_ON_UNIMPLEMENTED_OPCODE
};


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
        operand_size_(0) {
    tmp_buffer_[0] = '\0';
  }

  virtual ~DisassemblerX64() {
  }

  // Writes one disassembled instruction into 'buffer' (0-terminated).
  // Returns the length of the disassembled machine instruction in bytes.
  int InstructionDecode(v8::internal::Vector<char> buffer, byte* instruction);

 private:

  const NameConverter& converter_;
  v8::internal::EmbeddedVector<char, 128> tmp_buffer_;
  unsigned int tmp_buffer_pos_;
  bool abort_on_unimplemented_;
  // Prefixes parsed
  byte rex_;
  byte operand_size_;

  void setOperandSizePrefix(byte prefix) {
    ASSERT_EQ(0x66, prefix);
    operand_size_ = prefix;
  }

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

  int operand_size() {
    return rex_w() ? 64 : (operand_size_ != 0) ? 16 : 32;
  }

  char operand_size_code() {
    return rex_w() ? 'q' : (operand_size_ != 0) ? 'w' : 'l';
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
    *base = data & 7 | (rex_b() ? 8 : 0);
  }

  typedef const char* (DisassemblerX64::*RegisterNameMapping)(int reg) const;

  int PrintRightOperandHelper(byte* modrmp,
                              RegisterNameMapping register_name);
  int PrintRightOperand(byte* modrmp);
  int PrintRightByteOperand(byte* modrmp);
  int PrintOperands(const char* mnem,
                    OperandOrder op_order,
                    byte* data);
  int PrintImmediateOp(byte* data);
  int F7Instruction(byte* data);
  int D1D3C1Instruction(byte* data);
  int JumpShort(byte* data);
  int JumpConditional(byte* data);
  int JumpConditionalShort(byte* data);
  int SetCC(byte* data);
  int FPUInstruction(byte* data);
  void AppendToBuffer(const char* format, ...);

  void UnimplementedInstruction() {
    if (abort_on_unimplemented_) {
      UNIMPLEMENTED();
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
          AppendToBuffer("[%s]", (this->*register_name)(base));
          return 2;
        } else if (base == 5) {
          // base == rbp means no base register (when mod == 0).
          int32_t disp = *reinterpret_cast<int32_t*>(modrmp + 2);
          AppendToBuffer("[%s*%d+0x%x]",
                         (this->*register_name)(index),
                         1 << scale, disp);
          return 6;
        } else if (index != 4 && base != 5) {
          // [base+index*scale]
          AppendToBuffer("[%s+%s*%d]",
                         (this->*register_name)(base),
                         (this->*register_name)(index),
                         1 << scale);
          return 2;
        } else {
          UnimplementedInstruction();
          return 1;
        }
      } else {
        AppendToBuffer("[%s]", (this->*register_name)(rm));
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
            AppendToBuffer("[%s-0x%x]", (this->*register_name)(base), -disp);
          } else {
            AppendToBuffer("[%s+0x%x]", (this->*register_name)(base), disp);
          }
        } else {
          if (-disp > 0) {
            AppendToBuffer("[%s+%s*%d-0x%x]",
                           (this->*register_name)(base),
                           (this->*register_name)(index),
                           1 << scale,
                           -disp);
          } else {
            AppendToBuffer("[%s+%s*%d+0x%x]",
                           (this->*register_name)(base),
                           (this->*register_name)(index),
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
        AppendToBuffer("[%s-0x%x]", (this->*register_name)(rm), -disp);
        } else {
        AppendToBuffer("[%s+0x%x]", (this->*register_name)(rm), disp);
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


int DisassemblerX64::PrintRightOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp,
                                 &DisassemblerX64::NameOfCPURegister);
}


int DisassemblerX64::PrintRightByteOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp,
                                 &DisassemblerX64::NameOfByteCPURegister);
}


// Returns number of bytes used including the current *data.
// Writes instruction's mnemonic, left and right operands to 'tmp_buffer_'.
int DisassemblerX64::PrintOperands(const char* mnem,
                                   OperandOrder op_order,
                                   byte* data) {
  byte modrm = *data;
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  int advance = 0;
  switch (op_order) {
    case REG_OPER_OP_ORDER: {
      AppendToBuffer("%s%c %s,",
                     mnem,
                     operand_size_code(),
                     NameOfCPURegister(regop));
      advance = PrintRightOperand(data);
      break;
    }
    case OPER_REG_OP_ORDER: {
      AppendToBuffer("%s%c ", mnem, operand_size_code());
      advance = PrintRightOperand(data);
      AppendToBuffer(",%s", NameOfCPURegister(regop));
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
  bool sign_extension_bit = (*data & 0x02) != 0;
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
  AppendToBuffer("%s ", mnem);
  int count = PrintRightOperand(data + 1);
  if (sign_extension_bit) {
    AppendToBuffer(",0x%x", *(data + 1 + count));
    return 1 + count + 1 /*int8*/;
  } else {
    AppendToBuffer(",0x%x", *reinterpret_cast<int32_t*>(data + 1 + count));
    return 1 + count + 4 /*int32_t*/;
  }
}


// Returns number of bytes used, including *data.
int DisassemblerX64::F7Instruction(byte* data) {
  assert(*data == 0xF7);
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
  } else if (mod == 3 && regop == 0) {
    int32_t imm = *reinterpret_cast<int32_t*>(data + 2);
    AppendToBuffer("test%c %s,0x%x",
                   operand_size_code(),
                   NameOfCPURegister(rm),
                   imm);
    return 6;
  } else if (regop == 0) {
    AppendToBuffer("test%c ", operand_size_code());
    int count = PrintRightOperand(data + 1);
    int32_t imm = *reinterpret_cast<int32_t*>(data + 1 + count);
    AppendToBuffer(",0x%x", imm);
    return 1 + count + 4 /*int32_t*/;
  } else {
    UnimplementedInstruction();
    return 2;
  }
}


int DisassemblerX64::D1D3C1Instruction(byte* data) {
  byte op = *data;
  assert(op == 0xD1 || op == 0xD3 || op == 0xC1);
  byte modrm = *(data + 1);
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  ASSERT(regop < 8);
  int imm8 = -1;
  int num_bytes = 2;
  if (mod == 3) {
    const char* mnem = NULL;
    if (op == 0xD1) {
      imm8 = 1;
      switch (regop) {
        case 2:
          mnem = "rcl";
          break;
        case 7:
          mnem = "sar";
          break;
        case 4:
          mnem = "shl";
          break;
        default:
          UnimplementedInstruction();
      }
    } else if (op == 0xC1) {
      imm8 = *(data + 2);
      num_bytes = 3;
      switch (regop) {
        case 2:
          mnem = "rcl";
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
      }
    } else if (op == 0xD3) {
      switch (regop) {
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
      }
    }
    assert(mnem != NULL);
    AppendToBuffer("%s%c %s,",
                   mnem,
                   operand_size_code(),
                   NameOfCPURegister(rm));
    if (imm8 > 0) {
      AppendToBuffer("%d", imm8);
    } else {
      AppendToBuffer("cl");
    }
  } else {
    UnimplementedInstruction();
  }
  return num_bytes;
}


// Returns number of bytes used, including *data.
int DisassemblerX64::JumpShort(byte* data) {
  assert(*data == 0xEB);
  byte b = *(data + 1);
  byte* dest = data + static_cast<int8_t>(b) + 2;
  AppendToBuffer("jmp %s", NameOfAddress(dest));
  return 2;
}


// Returns number of bytes used, including *data.
int DisassemblerX64::JumpConditional(byte* data) {
  assert(*data == 0x0F);
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
  assert(*data == 0x0F);
  byte cond = *(data + 1) & 0x0F;
  const char* mnem = conditional_code_suffix[cond];
  AppendToBuffer("set%s%c ", mnem, operand_size_code());
  PrintRightByteOperand(data + 2);
  return 3;  // includes 0x0F
}


// Returns number of bytes used, including *data.
int DisassemblerX64::FPUInstruction(byte* data) {
  byte b1 = *data;
  byte b2 = *(data + 1);
  if (b1 == 0xD9) {
    const char* mnem = NULL;
    switch (b2) {
      case 0xE8:
        mnem = "fld1";
        break;
      case 0xEE:
        mnem = "fldz";
        break;
      case 0xE1:
        mnem = "fabs";
        break;
      case 0xE0:
        mnem = "fchs";
        break;
      case 0xF8:
        mnem = "fprem";
        break;
      case 0xF5:
        mnem = "fprem1";
        break;
      case 0xF7:
        mnem = "fincstp";
        break;
      case 0xE4:
        mnem = "ftst";
        break;
    }
    if (mnem != NULL) {
      AppendToBuffer("%s", mnem);
      return 2;
    } else if ((b2 & 0xF8) == 0xC8) {
      AppendToBuffer("fxch st%d", b2 & 0x7);
      return 2;
    } else {
      int mod, regop, rm;
      get_modrm(*(data + 1), &mod, &regop, &rm);
      const char* mnem = "?";
      switch (regop) {
        case 0:
          mnem = "fld_s";
          break;
        case 3:
          mnem = "fstp_s";
          break;
        default:
          UnimplementedInstruction();
      }
      AppendToBuffer("%s ", mnem);
      int count = PrintRightOperand(data + 1);
      return count + 1;
    }
  } else if (b1 == 0xDD) {
    if ((b2 & 0xF8) == 0xC0) {
      AppendToBuffer("ffree st%d", b2 & 0x7);
      return 2;
    } else {
      int mod, regop, rm;
      get_modrm(*(data + 1), &mod, &regop, &rm);
      const char* mnem = "?";
      switch (regop) {
        case 0:
          mnem = "fld_d";
          break;
        case 3:
          mnem = "fstp_d";
          break;
        default:
          UnimplementedInstruction();
      }
      AppendToBuffer("%s ", mnem);
      int count = PrintRightOperand(data + 1);
      return count + 1;
    }
  } else if (b1 == 0xDB) {
    int mod, regop, rm;
    get_modrm(*(data + 1), &mod, &regop, &rm);
    const char* mnem = "?";
    switch (regop) {
      case 0:
        mnem = "fild_s";
        break;
      case 2:
        mnem = "fist_s";
        break;
      case 3:
        mnem = "fistp_s";
        break;
      default:
        UnimplementedInstruction();
    }
    AppendToBuffer("%s ", mnem);
    int count = PrintRightOperand(data + 1);
    return count + 1;
  } else if (b1 == 0xDF) {
    if (b2 == 0xE0) {
      AppendToBuffer("fnstsw_ax");
      return 2;
    }
    int mod, regop, rm;
    get_modrm(*(data + 1), &mod, &regop, &rm);
    const char* mnem = "?";
    switch (regop) {
      case 5:
        mnem = "fild_d";
        break;
      case 7:
        mnem = "fistp_d";
        break;
      default:
        UnimplementedInstruction();
    }
    AppendToBuffer("%s ", mnem);
    int count = PrintRightOperand(data + 1);
    return count + 1;
  } else if (b1 == 0xDC || b1 == 0xDE) {
    bool is_pop = (b1 == 0xDE);
    if (is_pop && b2 == 0xD9) {
      AppendToBuffer("fcompp");
      return 2;
    }
    const char* mnem = "FP0xDC";
    switch (b2 & 0xF8) {
      case 0xC0:
        mnem = "fadd";
        break;
      case 0xE8:
        mnem = "fsub";
        break;
      case 0xC8:
        mnem = "fmul";
        break;
      case 0xF8:
        mnem = "fdiv";
        break;
      default:
        UnimplementedInstruction();
    }
    AppendToBuffer("%s%s st%d", mnem, is_pop ? "p" : "", b2 & 0x7);
    return 2;
  } else if (b1 == 0xDA && b2 == 0xE9) {
    const char* mnem = "fucompp";
    AppendToBuffer("%s", mnem);
    return 2;
  }
  AppendToBuffer("Unknown FP instruction");
  return 2;
}

// Mnemonics for instructions 0xF0 byte.
// Returns NULL if the instruction is not handled here.
static const char* F0Mnem(byte f0byte) {
  switch (f0byte) {
    case 0x1F:
      return "nop";
    case 0x31:
      return "rdtsc";
    case 0xA2:
      return "cpuid";
    case 0xBE:
      return "movsxb";
    case 0xBF:
      return "movsxw";
    case 0xB6:
      return "movzxb";
    case 0xB7:
      return "movzxw";
    case 0xAF:
      return "imul";
    case 0xA5:
      return "shld";
    case 0xAD:
      return "shrd";
    case 0xAB:
      return "bts";
    default:
      return NULL;
  }
}

// Disassembled instruction '*instr' and writes it into 'out_buffer'.
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
    if (current == 0x66) {
      setOperandSizePrefix(current);
      data++;
    } else if ((current & 0xF0) == 0x40) {
      setRex(current);
      if (rex_w()) AppendToBuffer("REX.W ");
      data++;
    } else {
      break;
    }
  }

  const InstructionDesc& idesc = instruction_table.Get(current);
  switch (idesc.type) {
    case ZERO_OPERANDS_INSTR:
      AppendToBuffer(idesc.mnem);
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
        case 16:
          addr = reinterpret_cast<byte*>(*reinterpret_cast<int16_t*>(data + 1));
          data += 3;
          break;
        case 32:
          addr = reinterpret_cast<byte*>(*reinterpret_cast<int32_t*>(data + 1));
          data += 5;
          break;
        case 64:
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
        AppendToBuffer("imul %s,%s,0x%x", NameOfCPURegister(regop),
                       NameOfCPURegister(rm), imm);
        data += 2 + (*data == 0x6B ? 1 : 4);
      }
        break;

      case 0xF6: {
        int mod, regop, rm;
        get_modrm(*(data + 1), &mod, &regop, &rm);
        if (mod == 3 && regop == 0) {
          AppendToBuffer("testb %s,%d", NameOfCPURegister(rm), *(data + 2));
        } else {
          UnimplementedInstruction();
        }
        data += 3;
      }
        break;

      case 0x81:  // fall through
      case 0x83:  // 0x81 with sign extension bit set
        data += PrintImmediateOp(data);
        break;

      case 0x0F: {
        byte f0byte = *(data + 1);
        const char* f0mnem = F0Mnem(f0byte);
        if (f0byte == 0x1F) {
          data += 1;
          byte modrm = *data;
          data += 1;
          if (((modrm >> 3) & 7) == 4) {
            // SIB byte present.
            data += 1;
          }
          int mod = modrm >> 6;
          if (mod == 1) {
            // Byte displacement.
            data += 1;
          } else if (mod == 2) {
            // 32-bit displacement.
            data += 4;
          }
          AppendToBuffer("nop");
        } else  if (f0byte == 0xA2 || f0byte == 0x31) {
          AppendToBuffer("%s", f0mnem);
          data += 2;
        } else if ((f0byte & 0xF0) == 0x80) {
          data += JumpConditional(data);
        } else if (f0byte == 0xBE || f0byte == 0xBF || f0byte == 0xB6 || f0byte
            == 0xB7 || f0byte == 0xAF) {
          data += 2;
          data += PrintOperands(f0mnem, REG_OPER_OP_ORDER, data);
        } else if ((f0byte & 0xF0) == 0x90) {
          data += SetCC(data);
        } else {
          data += 2;
          if (f0byte == 0xAB || f0byte == 0xA5 || f0byte == 0xAD) {
            // shrd, shld, bts
            AppendToBuffer("%s ", f0mnem);
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            data += PrintRightOperand(data);
            if (f0byte == 0xAB) {
              AppendToBuffer(",%s", NameOfCPURegister(regop));
            } else {
              AppendToBuffer(",%s,cl", NameOfCPURegister(regop));
            }
          } else {
            UnimplementedInstruction();
          }
        }
      }
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
        int reg = current & 0x7 | (rex_b() ? 8 : 0);
        if (reg == 0) {
          AppendToBuffer("nop");  // Common name for xchg rax,rax.
        } else {
          AppendToBuffer("xchg%c rax, %s",
                         operand_size_code(),
                         NameOfByteCPURegister(reg));
        }
      }


      case 0xFE: {
        data++;
        int mod, regop, rm;
        get_modrm(*data, &mod, &regop, &rm);
        if (mod == 3 && regop == 1) {
          AppendToBuffer("decb %s", NameOfCPURegister(rm));
        } else {
          UnimplementedInstruction();
        }
        data++;
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

      case 0xA8:
        AppendToBuffer("test al,0x%x", *reinterpret_cast<uint8_t*>(data + 1));
        data += 2;
        break;

      case 0xA9:
        AppendToBuffer("test%c rax,0x%x",  // CHECKME!
                       operand_size_code(),
                       *reinterpret_cast<int32_t*>(data + 1));
        data += 5;
        break;

      case 0xD1:  // fall through
      case 0xD3:  // fall through
      case 0xC1:
        data += D1D3C1Instruction(data);
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

      case 0xF2:
        if (*(data + 1) == 0x0F) {
          byte b2 = *(data + 2);
          if (b2 == 0x11) {
            AppendToBuffer("movsd ");
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            data += PrintRightOperand(data);
            AppendToBuffer(",%s", NameOfXMMRegister(regop));
          } else if (b2 == 0x10) {
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movsd %s,", NameOfXMMRegister(regop));
            data += PrintRightOperand(data);
          } else {
            const char* mnem = "?";
            switch (b2) {
              case 0x2A:
                mnem = "cvtsi2sd";
                break;
              case 0x58:
                mnem = "addsd";
                break;
              case 0x59:
                mnem = "mulsd";
                break;
              case 0x5C:
                mnem = "subsd";
                break;
              case 0x5E:
                mnem = "divsd";
                break;
            }
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            if (b2 == 0x2A) {
              AppendToBuffer("%s %s,", mnem, NameOfXMMRegister(regop));
              data += PrintRightOperand(data);
            } else {
              AppendToBuffer("%s %s,%s", mnem, NameOfXMMRegister(regop),
                             NameOfXMMRegister(rm));
              data++;
            }
          }
        } else {
          UnimplementedInstruction();
        }
        break;

      case 0xF3:
        if (*(data + 1) == 0x0F && *(data + 2) == 0x2C) {
          data += 3;
          data += PrintOperands("cvttss2si", REG_OPER_OP_ORDER, data);
        } else {
          UnimplementedInstruction();
        }
        break;

      case 0xF7:
        data += F7Instruction(data);
        break;

      default:
        UnimplementedInstruction();
    }
  }  // !processed

  if (tmp_buffer_pos_ < sizeof tmp_buffer_) {
    tmp_buffer_[tmp_buffer_pos_] = '\0';
  }

  int instr_len = data - instr;
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
    for (int i = 6 - (pc - prev_pc); i >= 0; i--) {
      fprintf(f, "  ");
    }
    fprintf(f, "  %s\n", buffer.start());
  }
}

}  // namespace disasm

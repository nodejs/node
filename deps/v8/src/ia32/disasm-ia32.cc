// Copyright 2011 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_IA32

#include "disasm.h"

namespace disasm {

enum OperandOrder {
  UNSET_OP_ORDER = 0,
  REG_OPER_OP_ORDER,
  OPER_REG_OP_ORDER
};


//------------------------------------------------------------------
// Tables
//------------------------------------------------------------------
struct ByteMnemonic {
  int b;  // -1 terminates, otherwise must be in range (0..255)
  const char* mnem;
  OperandOrder op_order_;
};


static const ByteMnemonic two_operands_instr[] = {
  {0x01, "add", OPER_REG_OP_ORDER},
  {0x03, "add", REG_OPER_OP_ORDER},
  {0x09, "or", OPER_REG_OP_ORDER},
  {0x0B, "or", REG_OPER_OP_ORDER},
  {0x1B, "sbb", REG_OPER_OP_ORDER},
  {0x21, "and", OPER_REG_OP_ORDER},
  {0x23, "and", REG_OPER_OP_ORDER},
  {0x29, "sub", OPER_REG_OP_ORDER},
  {0x2A, "subb", REG_OPER_OP_ORDER},
  {0x2B, "sub", REG_OPER_OP_ORDER},
  {0x31, "xor", OPER_REG_OP_ORDER},
  {0x33, "xor", REG_OPER_OP_ORDER},
  {0x38, "cmpb", OPER_REG_OP_ORDER},
  {0x3A, "cmpb", REG_OPER_OP_ORDER},
  {0x3B, "cmp", REG_OPER_OP_ORDER},
  {0x84, "test_b", REG_OPER_OP_ORDER},
  {0x85, "test", REG_OPER_OP_ORDER},
  {0x87, "xchg", REG_OPER_OP_ORDER},
  {0x8A, "mov_b", REG_OPER_OP_ORDER},
  {0x8B, "mov", REG_OPER_OP_ORDER},
  {0x8D, "lea", REG_OPER_OP_ORDER},
  {-1, "", UNSET_OP_ORDER}
};


static const ByteMnemonic zero_operands_instr[] = {
  {0xC3, "ret", UNSET_OP_ORDER},
  {0xC9, "leave", UNSET_OP_ORDER},
  {0x90, "nop", UNSET_OP_ORDER},
  {0xF4, "hlt", UNSET_OP_ORDER},
  {0xCC, "int3", UNSET_OP_ORDER},
  {0x60, "pushad", UNSET_OP_ORDER},
  {0x61, "popad", UNSET_OP_ORDER},
  {0x9C, "pushfd", UNSET_OP_ORDER},
  {0x9D, "popfd", UNSET_OP_ORDER},
  {0x9E, "sahf", UNSET_OP_ORDER},
  {0x99, "cdq", UNSET_OP_ORDER},
  {0x9B, "fwait", UNSET_OP_ORDER},
  {0xFC, "cld", UNSET_OP_ORDER},
  {0xAB, "stos", UNSET_OP_ORDER},
  {-1, "", UNSET_OP_ORDER}
};


static const ByteMnemonic call_jump_instr[] = {
  {0xE8, "call", UNSET_OP_ORDER},
  {0xE9, "jmp", UNSET_OP_ORDER},
  {-1, "", UNSET_OP_ORDER}
};


static const ByteMnemonic short_immediate_instr[] = {
  {0x05, "add", UNSET_OP_ORDER},
  {0x0D, "or", UNSET_OP_ORDER},
  {0x15, "adc", UNSET_OP_ORDER},
  {0x25, "and", UNSET_OP_ORDER},
  {0x2D, "sub", UNSET_OP_ORDER},
  {0x35, "xor", UNSET_OP_ORDER},
  {0x3D, "cmp", UNSET_OP_ORDER},
  {-1, "", UNSET_OP_ORDER}
};


// Generally we don't want to generate these because they are subject to partial
// register stalls.  They are included for completeness and because the cmp
// variant is used by the RecordWrite stub.  Because it does not update the
// register it is not subject to partial register stalls.
static ByteMnemonic byte_immediate_instr[] = {
  {0x0c, "or", UNSET_OP_ORDER},
  {0x24, "and", UNSET_OP_ORDER},
  {0x34, "xor", UNSET_OP_ORDER},
  {0x3c, "cmp", UNSET_OP_ORDER},
  {-1, "", UNSET_OP_ORDER}
};


static const char* const jump_conditional_mnem[] = {
  /*0*/ "jo", "jno", "jc", "jnc",
  /*4*/ "jz", "jnz", "jna", "ja",
  /*8*/ "js", "jns", "jpe", "jpo",
  /*12*/ "jl", "jnl", "jng", "jg"
};


static const char* const set_conditional_mnem[] = {
  /*0*/ "seto", "setno", "setc", "setnc",
  /*4*/ "setz", "setnz", "setna", "seta",
  /*8*/ "sets", "setns", "setpe", "setpo",
  /*12*/ "setl", "setnl", "setng", "setg"
};


static const char* const conditional_move_mnem[] = {
  /*0*/ "cmovo", "cmovno", "cmovc", "cmovnc",
  /*4*/ "cmovz", "cmovnz", "cmovna", "cmova",
  /*8*/ "cmovs", "cmovns", "cmovpe", "cmovpo",
  /*12*/ "cmovl", "cmovnl", "cmovng", "cmovg"
};


enum InstructionType {
  NO_INSTR,
  ZERO_OPERANDS_INSTR,
  TWO_OPERANDS_INSTR,
  JUMP_CONDITIONAL_SHORT_INSTR,
  REGISTER_INSTR,
  MOVE_REG_INSTR,
  CALL_JUMP_INSTR,
  SHORT_IMMEDIATE_INSTR,
  BYTE_IMMEDIATE_INSTR
};


struct InstructionDesc {
  const char* mnem;
  InstructionType type;
  OperandOrder op_order_;
};


class InstructionTable {
 public:
  InstructionTable();
  const InstructionDesc& Get(byte x) const { return instructions_[x]; }
  static InstructionTable* get_instance() {
    static InstructionTable table;
    return &table;
  }

 private:
  InstructionDesc instructions_[256];
  void Clear();
  void Init();
  void CopyTable(const ByteMnemonic bm[], InstructionType type);
  void SetTableRange(InstructionType type,
                     byte start,
                     byte end,
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
  CopyTable(byte_immediate_instr, BYTE_IMMEDIATE_INSTR);
  AddJumpConditionalShort();
  SetTableRange(REGISTER_INSTR, 0x40, 0x47, "inc");
  SetTableRange(REGISTER_INSTR, 0x48, 0x4F, "dec");
  SetTableRange(REGISTER_INSTR, 0x50, 0x57, "push");
  SetTableRange(REGISTER_INSTR, 0x58, 0x5F, "pop");
  SetTableRange(REGISTER_INSTR, 0x91, 0x97, "xchg eax,");  // 0x90 is nop.
  SetTableRange(MOVE_REG_INSTR, 0xB8, 0xBF, "mov");
}


void InstructionTable::CopyTable(const ByteMnemonic bm[],
                                 InstructionType type) {
  for (int i = 0; bm[i].b >= 0; i++) {
    InstructionDesc* id = &instructions_[bm[i].b];
    id->mnem = bm[i].mnem;
    id->op_order_ = bm[i].op_order_;
    ASSERT_EQ(NO_INSTR, id->type);  // Information not already entered.
    id->type = type;
  }
}


void InstructionTable::SetTableRange(InstructionType type,
                                     byte start,
                                     byte end,
                                     const char* mnem) {
  for (byte b = start; b <= end; b++) {
    InstructionDesc* id = &instructions_[b];
    ASSERT_EQ(NO_INSTR, id->type);  // Information not already entered.
    id->mnem = mnem;
    id->type = type;
  }
}


void InstructionTable::AddJumpConditionalShort() {
  for (byte b = 0x70; b <= 0x7F; b++) {
    InstructionDesc* id = &instructions_[b];
    ASSERT_EQ(NO_INSTR, id->type);  // Information not already entered.
    id->mnem = jump_conditional_mnem[b & 0x0F];
    id->type = JUMP_CONDITIONAL_SHORT_INSTR;
  }
}


// The IA32 disassembler implementation.
class DisassemblerIA32 {
 public:
  DisassemblerIA32(const NameConverter& converter,
                   bool abort_on_unimplemented = true)
      : converter_(converter),
        instruction_table_(InstructionTable::get_instance()),
        tmp_buffer_pos_(0),
        abort_on_unimplemented_(abort_on_unimplemented) {
    tmp_buffer_[0] = '\0';
  }

  virtual ~DisassemblerIA32() {}

  // Writes one disassembled instruction into 'buffer' (0-terminated).
  // Returns the length of the disassembled machine instruction in bytes.
  int InstructionDecode(v8::internal::Vector<char> buffer, byte* instruction);

 private:
  const NameConverter& converter_;
  InstructionTable* instruction_table_;
  v8::internal::EmbeddedVector<char, 128> tmp_buffer_;
  unsigned int tmp_buffer_pos_;
  bool abort_on_unimplemented_;

  enum {
    eax = 0,
    ecx = 1,
    edx = 2,
    ebx = 3,
    esp = 4,
    ebp = 5,
    esi = 6,
    edi = 7
  };


  enum ShiftOpcodeExtension {
    kROL = 0,
    kROR = 1,
    kRCL = 2,
    kRCR = 3,
    kSHL = 4,
    KSHR = 5,
    kSAR = 7
  };


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
  static void get_modrm(byte data, int* mod, int* regop, int* rm) {
    *mod = (data >> 6) & 3;
    *regop = (data & 0x38) >> 3;
    *rm = data & 7;
  }


  static void get_sib(byte data, int* scale, int* index, int* base) {
    *scale = (data >> 6) & 3;
    *index = (data >> 3) & 7;
    *base = data & 7;
  }

  typedef const char* (DisassemblerIA32::*RegisterNameMapping)(int reg) const;

  int PrintRightOperandHelper(byte* modrmp, RegisterNameMapping register_name);
  int PrintRightOperand(byte* modrmp);
  int PrintRightByteOperand(byte* modrmp);
  int PrintRightXMMOperand(byte* modrmp);
  int PrintOperands(const char* mnem, OperandOrder op_order, byte* data);
  int PrintImmediateOp(byte* data);
  int F7Instruction(byte* data);
  int D1D3C1Instruction(byte* data);
  int JumpShort(byte* data);
  int JumpConditional(byte* data, const char* comment);
  int JumpConditionalShort(byte* data, const char* comment);
  int SetCC(byte* data);
  int CMov(byte* data);
  int FPUInstruction(byte* data);
  int MemoryFPUInstruction(int escape_opcode, int regop, byte* modrm_start);
  int RegisterFPUInstruction(int escape_opcode, byte modrm_byte);
  void AppendToBuffer(const char* format, ...);


  void UnimplementedInstruction() {
    if (abort_on_unimplemented_) {
      UNIMPLEMENTED();
    } else {
      AppendToBuffer("'Unimplemented Instruction'");
    }
  }
};


void DisassemblerIA32::AppendToBuffer(const char* format, ...) {
  v8::internal::Vector<char> buf = tmp_buffer_ + tmp_buffer_pos_;
  va_list args;
  va_start(args, format);
  int result = v8::internal::OS::VSNPrintF(buf, format, args);
  va_end(args);
  tmp_buffer_pos_ += result;
}

int DisassemblerIA32::PrintRightOperandHelper(
    byte* modrmp,
    RegisterNameMapping direct_register_name) {
  int mod, regop, rm;
  get_modrm(*modrmp, &mod, &regop, &rm);
  RegisterNameMapping register_name = (mod == 3) ? direct_register_name :
      &DisassemblerIA32::NameOfCPURegister;
  switch (mod) {
    case 0:
      if (rm == ebp) {
        int32_t disp = *reinterpret_cast<int32_t*>(modrmp+1);
        AppendToBuffer("[0x%x]", disp);
        return 5;
      } else if (rm == esp) {
        byte sib = *(modrmp + 1);
        int scale, index, base;
        get_sib(sib, &scale, &index, &base);
        if (index == esp && base == esp && scale == 0 /*times_1*/) {
          AppendToBuffer("[%s]", (this->*register_name)(rm));
          return 2;
        } else if (base == ebp) {
          int32_t disp = *reinterpret_cast<int32_t*>(modrmp + 2);
          AppendToBuffer("[%s*%d+0x%x]",
                         (this->*register_name)(index),
                         1 << scale,
                         disp);
          return 6;
        } else if (index != esp && base != ebp) {
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
      if (rm == esp) {
        byte sib = *(modrmp + 1);
        int scale, index, base;
        get_sib(sib, &scale, &index, &base);
        int disp =
            mod == 2 ? *reinterpret_cast<int32_t*>(modrmp + 2) : *(modrmp + 2);
        if (index == base && index == rm /*esp*/ && scale == 0 /*times_1*/) {
          AppendToBuffer("[%s+0x%x]", (this->*register_name)(rm), disp);
        } else {
          AppendToBuffer("[%s+%s*%d+0x%x]",
                         (this->*register_name)(base),
                         (this->*register_name)(index),
                         1 << scale,
                         disp);
        }
        return mod == 2 ? 6 : 3;
      } else {
        // No sib.
        int disp =
            mod == 2 ? *reinterpret_cast<int32_t*>(modrmp + 1) : *(modrmp + 1);
        AppendToBuffer("[%s+0x%x]", (this->*register_name)(rm), disp);
        return mod == 2 ? 5 : 2;
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


int DisassemblerIA32::PrintRightOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp, &DisassemblerIA32::NameOfCPURegister);
}


int DisassemblerIA32::PrintRightByteOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp,
                                 &DisassemblerIA32::NameOfByteCPURegister);
}


int DisassemblerIA32::PrintRightXMMOperand(byte* modrmp) {
  return PrintRightOperandHelper(modrmp,
                                 &DisassemblerIA32::NameOfXMMRegister);
}


// Returns number of bytes used including the current *data.
// Writes instruction's mnemonic, left and right operands to 'tmp_buffer_'.
int DisassemblerIA32::PrintOperands(const char* mnem,
                                    OperandOrder op_order,
                                    byte* data) {
  byte modrm = *data;
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  int advance = 0;
  switch (op_order) {
    case REG_OPER_OP_ORDER: {
      AppendToBuffer("%s %s,", mnem, NameOfCPURegister(regop));
      advance = PrintRightOperand(data);
      break;
    }
    case OPER_REG_OP_ORDER: {
      AppendToBuffer("%s ", mnem);
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
int DisassemblerIA32::PrintImmediateOp(byte* data) {
  bool sign_extension_bit = (*data & 0x02) != 0;
  byte modrm = *(data+1);
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  const char* mnem = "Imm???";
  switch (regop) {
    case 0: mnem = "add"; break;
    case 1: mnem = "or"; break;
    case 2: mnem = "adc"; break;
    case 4: mnem = "and"; break;
    case 5: mnem = "sub"; break;
    case 6: mnem = "xor"; break;
    case 7: mnem = "cmp"; break;
    default: UnimplementedInstruction();
  }
  AppendToBuffer("%s ", mnem);
  int count = PrintRightOperand(data+1);
  if (sign_extension_bit) {
    AppendToBuffer(",0x%x", *(data + 1 + count));
    return 1 + count + 1 /*int8*/;
  } else {
    AppendToBuffer(",0x%x", *reinterpret_cast<int32_t*>(data + 1 + count));
    return 1 + count + 4 /*int32_t*/;
  }
}


// Returns number of bytes used, including *data.
int DisassemblerIA32::F7Instruction(byte* data) {
  ASSERT_EQ(0xF7, *data);
  byte modrm = *(data+1);
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  if (mod == 3 && regop != 0) {
    const char* mnem = NULL;
    switch (regop) {
      case 2: mnem = "not"; break;
      case 3: mnem = "neg"; break;
      case 4: mnem = "mul"; break;
      case 5: mnem = "imul"; break;
      case 7: mnem = "idiv"; break;
      default: UnimplementedInstruction();
    }
    AppendToBuffer("%s %s", mnem, NameOfCPURegister(rm));
    return 2;
  } else if (mod == 3 && regop == eax) {
    int32_t imm = *reinterpret_cast<int32_t*>(data+2);
    AppendToBuffer("test %s,0x%x", NameOfCPURegister(rm), imm);
    return 6;
  } else if (regop == eax) {
    AppendToBuffer("test ");
    int count = PrintRightOperand(data+1);
    int32_t imm = *reinterpret_cast<int32_t*>(data+1+count);
    AppendToBuffer(",0x%x", imm);
    return 1+count+4 /*int32_t*/;
  } else {
    UnimplementedInstruction();
    return 2;
  }
}


int DisassemblerIA32::D1D3C1Instruction(byte* data) {
  byte op = *data;
  ASSERT(op == 0xD1 || op == 0xD3 || op == 0xC1);
  byte modrm = *(data+1);
  int mod, regop, rm;
  get_modrm(modrm, &mod, &regop, &rm);
  int imm8 = -1;
  int num_bytes = 2;
  if (mod == 3) {
    const char* mnem = NULL;
    switch (regop) {
      case kROL: mnem = "rol"; break;
      case kROR: mnem = "ror"; break;
      case kRCL: mnem = "rcl"; break;
      case kRCR: mnem = "rcr"; break;
      case kSHL: mnem = "shl"; break;
      case KSHR: mnem = "shr"; break;
      case kSAR: mnem = "sar"; break;
      default: UnimplementedInstruction();
    }
    if (op == 0xD1) {
      imm8 = 1;
    } else if (op == 0xC1) {
      imm8 = *(data+2);
      num_bytes = 3;
    } else if (op == 0xD3) {
      // Shift/rotate by cl.
    }
    ASSERT_NE(NULL, mnem);
    AppendToBuffer("%s %s,", mnem, NameOfCPURegister(rm));
    if (imm8 >= 0) {
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
int DisassemblerIA32::JumpShort(byte* data) {
  ASSERT_EQ(0xEB, *data);
  byte b = *(data+1);
  byte* dest = data + static_cast<int8_t>(b) + 2;
  AppendToBuffer("jmp %s", NameOfAddress(dest));
  return 2;
}


// Returns number of bytes used, including *data.
int DisassemblerIA32::JumpConditional(byte* data, const char* comment) {
  ASSERT_EQ(0x0F, *data);
  byte cond = *(data+1) & 0x0F;
  byte* dest = data + *reinterpret_cast<int32_t*>(data+2) + 6;
  const char* mnem = jump_conditional_mnem[cond];
  AppendToBuffer("%s %s", mnem, NameOfAddress(dest));
  if (comment != NULL) {
    AppendToBuffer(", %s", comment);
  }
  return 6;  // includes 0x0F
}


// Returns number of bytes used, including *data.
int DisassemblerIA32::JumpConditionalShort(byte* data, const char* comment) {
  byte cond = *data & 0x0F;
  byte b = *(data+1);
  byte* dest = data + static_cast<int8_t>(b) + 2;
  const char* mnem = jump_conditional_mnem[cond];
  AppendToBuffer("%s %s", mnem, NameOfAddress(dest));
  if (comment != NULL) {
    AppendToBuffer(", %s", comment);
  }
  return 2;
}


// Returns number of bytes used, including *data.
int DisassemblerIA32::SetCC(byte* data) {
  ASSERT_EQ(0x0F, *data);
  byte cond = *(data+1) & 0x0F;
  const char* mnem = set_conditional_mnem[cond];
  AppendToBuffer("%s ", mnem);
  PrintRightByteOperand(data+2);
  return 3;  // Includes 0x0F.
}


// Returns number of bytes used, including *data.
int DisassemblerIA32::CMov(byte* data) {
  ASSERT_EQ(0x0F, *data);
  byte cond = *(data + 1) & 0x0F;
  const char* mnem = conditional_move_mnem[cond];
  int op_size = PrintOperands(mnem, REG_OPER_OP_ORDER, data + 2);
  return 2 + op_size;  // includes 0x0F
}


// Returns number of bytes used, including *data.
int DisassemblerIA32::FPUInstruction(byte* data) {
  byte escape_opcode = *data;
  ASSERT_EQ(0xD8, escape_opcode & 0xF8);
  byte modrm_byte = *(data+1);

  if (modrm_byte >= 0xC0) {
    return RegisterFPUInstruction(escape_opcode, modrm_byte);
  } else {
    return MemoryFPUInstruction(escape_opcode, modrm_byte, data+1);
  }
}

int DisassemblerIA32::MemoryFPUInstruction(int escape_opcode,
                                           int modrm_byte,
                                           byte* modrm_start) {
  const char* mnem = "?";
  int regop = (modrm_byte >> 3) & 0x7;  // reg/op field of modrm byte.
  switch (escape_opcode) {
    case 0xD9: switch (regop) {
        case 0: mnem = "fld_s"; break;
        case 2: mnem = "fst_s"; break;
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
        case 1: mnem = "fisttp_d"; break;
        case 2: mnem = "fst_d"; break;
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

int DisassemblerIA32::RegisterFPUInstruction(int escape_opcode,
                                             byte modrm_byte) {
  bool has_register = false;  // Is the FPU register encoded in modrm_byte?
  const char* mnem = "?";

  switch (escape_opcode) {
    case 0xD8:
      has_register = true;
      switch (modrm_byte & 0xF8) {
        case 0xC0: mnem = "fadd_i"; break;
        case 0xE0: mnem = "fsub_i"; break;
        case 0xC8: mnem = "fmul_i"; break;
        case 0xF0: mnem = "fdiv_i"; break;
        default: UnimplementedInstruction();
      }
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
            case 0xED: mnem = "fldln2"; break;
            case 0xEE: mnem = "fldz"; break;
            case 0xF0: mnem = "f2xm1"; break;
            case 0xF1: mnem = "fyl2x"; break;
            case 0xF4: mnem = "fxtract"; break;
            case 0xF5: mnem = "fprem1"; break;
            case 0xF7: mnem = "fincstp"; break;
            case 0xF8: mnem = "fprem"; break;
            case 0xFC: mnem = "frndint"; break;
            case 0xFD: mnem = "fscale"; break;
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
      } else if (modrm_byte == 0xE3) {
        mnem = "fninit";
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
        case 0xD0: mnem = "fst"; break;
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


// Mnemonics for instructions 0xF0 byte.
// Returns NULL if the instruction is not handled here.
static const char* F0Mnem(byte f0byte) {
  switch (f0byte) {
    case 0x18: return "prefetch";
    case 0xA2: return "cpuid";
    case 0xBE: return "movsx_b";
    case 0xBF: return "movsx_w";
    case 0xB6: return "movzx_b";
    case 0xB7: return "movzx_w";
    case 0xAF: return "imul";
    case 0xA5: return "shld";
    case 0xAD: return "shrd";
    case 0xAC: return "shrd";  // 3-operand version.
    case 0xAB: return "bts";
    default: return NULL;
  }
}


// Disassembled instruction '*instr' and writes it into 'out_buffer'.
int DisassemblerIA32::InstructionDecode(v8::internal::Vector<char> out_buffer,
                                        byte* instr) {
  tmp_buffer_pos_ = 0;  // starting to write as position 0
  byte* data = instr;
  // Check for hints.
  const char* branch_hint = NULL;
  // We use these two prefixes only with branch prediction
  if (*data == 0x3E /*ds*/) {
    branch_hint = "predicted taken";
    data++;
  } else if (*data == 0x2E /*cs*/) {
    branch_hint = "predicted not taken";
    data++;
  }
  bool processed = true;  // Will be set to false if the current instruction
                          // is not in 'instructions' table.
  const InstructionDesc& idesc = instruction_table_->Get(*data);
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
      data += JumpConditionalShort(data, branch_hint);
      break;

    case REGISTER_INSTR:
      AppendToBuffer("%s %s", idesc.mnem, NameOfCPURegister(*data & 0x07));
      data++;
      break;

    case MOVE_REG_INSTR: {
      byte* addr = reinterpret_cast<byte*>(*reinterpret_cast<int32_t*>(data+1));
      AppendToBuffer("mov %s,%s",
                     NameOfCPURegister(*data & 0x07),
                     NameOfAddress(addr));
      data += 5;
      break;
    }

    case CALL_JUMP_INSTR: {
      byte* addr = data + *reinterpret_cast<int32_t*>(data+1) + 5;
      AppendToBuffer("%s %s", idesc.mnem, NameOfAddress(addr));
      data += 5;
      break;
    }

    case SHORT_IMMEDIATE_INSTR: {
      byte* addr = reinterpret_cast<byte*>(*reinterpret_cast<int32_t*>(data+1));
      AppendToBuffer("%s eax,%s", idesc.mnem, NameOfAddress(addr));
      data += 5;
      break;
    }

    case BYTE_IMMEDIATE_INSTR: {
      AppendToBuffer("%s al,0x%x", idesc.mnem, data[1]);
      data += 2;
      break;
    }

    case NO_INSTR:
      processed = false;
      break;

    default:
      UNIMPLEMENTED();  // This type is not implemented.
  }
  //----------------------------
  if (!processed) {
    switch (*data) {
      case 0xC2:
        AppendToBuffer("ret 0x%x", *reinterpret_cast<uint16_t*>(data+1));
        data += 3;
        break;

      case 0x69:  // fall through
      case 0x6B:
        { int mod, regop, rm;
          get_modrm(*(data+1), &mod, &regop, &rm);
          int32_t imm =
              *data == 0x6B ? *(data+2) : *reinterpret_cast<int32_t*>(data+2);
          AppendToBuffer("imul %s,%s,0x%x",
                         NameOfCPURegister(regop),
                         NameOfCPURegister(rm),
                         imm);
          data += 2 + (*data == 0x6B ? 1 : 4);
        }
        break;

      case 0xF6:
        { data++;
          int mod, regop, rm;
          get_modrm(*data, &mod, &regop, &rm);
          if (regop == eax) {
            AppendToBuffer("test_b ");
            data += PrintRightByteOperand(data);
            int32_t imm = *data;
            AppendToBuffer(",0x%x", imm);
            data++;
          } else {
            UnimplementedInstruction();
          }
        }
        break;

      case 0x81:  // fall through
      case 0x83:  // 0x81 with sign extension bit set
        data += PrintImmediateOp(data);
        break;

      case 0x0F:
        { byte f0byte = data[1];
          const char* f0mnem = F0Mnem(f0byte);
          if (f0byte == 0x18) {
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            const char* suffix[] = {"nta", "1", "2", "3"};
            AppendToBuffer("%s%s ", f0mnem, suffix[regop & 0x03]);
            data += PrintRightOperand(data);
          } else if (f0byte == 0x1F && data[2] == 0) {
            AppendToBuffer("nop");  // 3 byte nop.
            data += 3;
          } else if (f0byte == 0x1F && data[2] == 0x40 && data[3] == 0) {
            AppendToBuffer("nop");  // 4 byte nop.
            data += 4;
          } else if (f0byte == 0x1F && data[2] == 0x44 && data[3] == 0 &&
                     data[4] == 0) {
            AppendToBuffer("nop");  // 5 byte nop.
            data += 5;
          } else if (f0byte == 0x1F && data[2] == 0x80 && data[3] == 0 &&
                     data[4] == 0 && data[5] == 0 && data[6] == 0) {
            AppendToBuffer("nop");  // 7 byte nop.
            data += 7;
          } else if (f0byte == 0x1F && data[2] == 0x84 && data[3] == 0 &&
                     data[4] == 0 && data[5] == 0 && data[6] == 0 &&
                     data[7] == 0) {
            AppendToBuffer("nop");  // 8 byte nop.
            data += 8;
          } else if (f0byte == 0xA2 || f0byte == 0x31) {
            AppendToBuffer("%s", f0mnem);
            data += 2;
          } else if (f0byte == 0x28) {
            data += 2;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movaps %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (f0byte == 0x54) {
            data += 2;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("andps %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (f0byte == 0x57) {
            data += 2;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("xorps %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (f0byte == 0x50) {
            data += 2;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movmskps %s,%s",
                           NameOfCPURegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if ((f0byte & 0xF0) == 0x80) {
            data += JumpConditional(data, branch_hint);
          } else if (f0byte == 0xBE || f0byte == 0xBF || f0byte == 0xB6 ||
                     f0byte == 0xB7 || f0byte == 0xAF) {
            data += 2;
            data += PrintOperands(f0mnem, REG_OPER_OP_ORDER, data);
          } else if ((f0byte & 0xF0) == 0x90) {
            data += SetCC(data);
          } else if ((f0byte & 0xF0) == 0x40) {
            data += CMov(data);
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

      case 0x8F:
        { data++;
          int mod, regop, rm;
          get_modrm(*data, &mod, &regop, &rm);
          if (regop == eax) {
            AppendToBuffer("pop ");
            data += PrintRightOperand(data);
          }
        }
        break;

      case 0xFF:
        { data++;
          int mod, regop, rm;
          get_modrm(*data, &mod, &regop, &rm);
          const char* mnem = NULL;
          switch (regop) {
            case esi: mnem = "push"; break;
            case eax: mnem = "inc"; break;
            case ecx: mnem = "dec"; break;
            case edx: mnem = "call"; break;
            case esp: mnem = "jmp"; break;
            default: mnem = "???";
          }
          AppendToBuffer("%s ", mnem);
          data += PrintRightOperand(data);
        }
        break;

      case 0xC7:  // imm32, fall through
      case 0xC6:  // imm8
        { bool is_byte = *data == 0xC6;
          data++;
          if (is_byte) {
            AppendToBuffer("%s ", "mov_b");
            data += PrintRightByteOperand(data);
            int32_t imm = *data;
            AppendToBuffer(",0x%x", imm);
            data++;
          } else {
            AppendToBuffer("%s ", "mov");
            data += PrintRightOperand(data);
            int32_t imm = *reinterpret_cast<int32_t*>(data);
            AppendToBuffer(",0x%x", imm);
            data += 4;
          }
        }
        break;

      case 0x80:
        { data++;
          int mod, regop, rm;
          get_modrm(*data, &mod, &regop, &rm);
          const char* mnem = NULL;
          switch (regop) {
            case 5:  mnem = "subb"; break;
            case 7:  mnem = "cmpb"; break;
            default: UnimplementedInstruction();
          }
          AppendToBuffer("%s ", mnem);
          data += PrintRightByteOperand(data);
          int32_t imm = *data;
          AppendToBuffer(",0x%x", imm);
          data++;
        }
        break;

      case 0x88:  // 8bit, fall through
      case 0x89:  // 32bit
        { bool is_byte = *data == 0x88;
          int mod, regop, rm;
          data++;
          get_modrm(*data, &mod, &regop, &rm);
          if (is_byte) {
            AppendToBuffer("%s ", "mov_b");
            data += PrintRightByteOperand(data);
            AppendToBuffer(",%s", NameOfByteCPURegister(regop));
          } else {
            AppendToBuffer("%s ", "mov");
            data += PrintRightOperand(data);
            AppendToBuffer(",%s", NameOfCPURegister(regop));
          }
        }
        break;

      case 0x66:  // prefix
        while (*data == 0x66) data++;
        if (*data == 0xf && data[1] == 0x1f) {
          AppendToBuffer("nop");  // 0x66 prefix
        } else if (*data == 0x90) {
          AppendToBuffer("nop");  // 0x66 prefix
        } else if (*data == 0x8B) {
          data++;
          data += PrintOperands("mov_w", REG_OPER_OP_ORDER, data);
        } else if (*data == 0x89) {
          data++;
          int mod, regop, rm;
          get_modrm(*data, &mod, &regop, &rm);
          AppendToBuffer("mov_w ");
          data += PrintRightOperand(data);
          AppendToBuffer(",%s", NameOfCPURegister(regop));
        } else if (*data == 0x0F) {
          data++;
          if (*data == 0x38) {
            data++;
            if (*data == 0x17) {
              data++;
              int mod, regop, rm;
              get_modrm(*data, &mod, &regop, &rm);
              AppendToBuffer("ptest %s,%s",
                             NameOfXMMRegister(regop),
                             NameOfXMMRegister(rm));
              data++;
            } else if (*data == 0x2A) {
              // movntdqa
              data++;
              int mod, regop, rm;
              get_modrm(*data, &mod, &regop, &rm);
              AppendToBuffer("movntdqa %s,", NameOfXMMRegister(regop));
              data += PrintRightOperand(data);
            } else {
              UnimplementedInstruction();
            }
          } else if (*data == 0x3A) {
            data++;
            if (*data == 0x0B) {
              data++;
              int mod, regop, rm;
              get_modrm(*data, &mod, &regop, &rm);
              int8_t imm8 = static_cast<int8_t>(data[1]);
              AppendToBuffer("roundsd %s,%s,%d",
                             NameOfXMMRegister(regop),
                             NameOfXMMRegister(rm),
                             static_cast<int>(imm8));
              data += 2;
            } else if (*data == 0x16) {
              data++;
              int mod, regop, rm;
              get_modrm(*data, &mod, &regop, &rm);
              int8_t imm8 = static_cast<int8_t>(data[1]);
              AppendToBuffer("pextrd %s,%s,%d",
                             NameOfCPURegister(regop),
                             NameOfXMMRegister(rm),
                             static_cast<int>(imm8));
              data += 2;
            } else if (*data == 0x17) {
              data++;
              int mod, regop, rm;
              get_modrm(*data, &mod, &regop, &rm);
              int8_t imm8 = static_cast<int8_t>(data[1]);
              AppendToBuffer("extractps %s,%s,%d",
                             NameOfCPURegister(rm),
                             NameOfXMMRegister(regop),
                             static_cast<int>(imm8));
              data += 2;
            } else if (*data == 0x22) {
              data++;
              int mod, regop, rm;
              get_modrm(*data, &mod, &regop, &rm);
              int8_t imm8 = static_cast<int8_t>(data[1]);
              AppendToBuffer("pinsrd %s,%s,%d",
                             NameOfXMMRegister(regop),
                             NameOfCPURegister(rm),
                             static_cast<int>(imm8));
              data += 2;
            } else {
              UnimplementedInstruction();
            }
          } else if (*data == 0x2E || *data == 0x2F) {
            const char* mnem = (*data == 0x2E) ? "ucomisd" : "comisd";
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            if (mod == 0x3) {
              AppendToBuffer("%s %s,%s", mnem,
                             NameOfXMMRegister(regop),
                             NameOfXMMRegister(rm));
              data++;
            } else {
              AppendToBuffer("%s %s,", mnem, NameOfXMMRegister(regop));
              data += PrintRightOperand(data);
            }
          } else if (*data == 0x50) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movmskpd %s,%s",
                           NameOfCPURegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0x54) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("andpd %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0x56) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("orpd %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0x57) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("xorpd %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0x6E) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movd %s,", NameOfXMMRegister(regop));
            data += PrintRightOperand(data);
          } else if (*data == 0x6F) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movdqa %s,", NameOfXMMRegister(regop));
            data += PrintRightXMMOperand(data);
          } else if (*data == 0x70) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            int8_t imm8 = static_cast<int8_t>(data[1]);
            AppendToBuffer("pshufd %s,%s,%d",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm),
                           static_cast<int>(imm8));
            data += 2;
          } else if (*data == 0x76) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("pcmpeqd %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0x90) {
            data++;
            AppendToBuffer("nop");  // 2 byte nop.
          } else if (*data == 0xF3) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("psllq %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0x73) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            int8_t imm8 = static_cast<int8_t>(data[1]);
            ASSERT(regop == esi || regop == edx);
            AppendToBuffer("%s %s,%d",
                           (regop == esi) ? "psllq" : "psrlq",
                           NameOfXMMRegister(rm),
                           static_cast<int>(imm8));
            data += 2;
          } else if (*data == 0xD3) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("psrlq %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0x7F) {
            AppendToBuffer("movdqa ");
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            data += PrintRightXMMOperand(data);
            AppendToBuffer(",%s", NameOfXMMRegister(regop));
          } else if (*data == 0x7E) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movd ");
            data += PrintRightOperand(data);
            AppendToBuffer(",%s", NameOfXMMRegister(regop));
          } else if (*data == 0xDB) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("pand %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0xE7) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            if (mod == 3) {
              AppendToBuffer("movntdq ");
              data += PrintRightOperand(data);
              AppendToBuffer(",%s", NameOfXMMRegister(regop));
            } else {
              UnimplementedInstruction();
            }
          } else if (*data == 0xEF) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("pxor %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else if (*data == 0xEB) {
            data++;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("por %s,%s",
                           NameOfXMMRegister(regop),
                           NameOfXMMRegister(rm));
            data++;
          } else {
            UnimplementedInstruction();
          }
        } else {
          UnimplementedInstruction();
        }
        break;

      case 0xFE:
        { data++;
          int mod, regop, rm;
          get_modrm(*data, &mod, &regop, &rm);
          if (regop == ecx) {
            AppendToBuffer("dec_b ");
            data += PrintRightOperand(data);
          } else {
            UnimplementedInstruction();
          }
        }
        break;

      case 0x68:
        AppendToBuffer("push 0x%x", *reinterpret_cast<int32_t*>(data+1));
        data += 5;
        break;

      case 0x6A:
        AppendToBuffer("push 0x%x", *reinterpret_cast<int8_t*>(data + 1));
        data += 2;
        break;

      case 0xA8:
        AppendToBuffer("test al,0x%x", *reinterpret_cast<uint8_t*>(data+1));
        data += 2;
        break;

      case 0xA9:
        AppendToBuffer("test eax,0x%x", *reinterpret_cast<int32_t*>(data+1));
        data += 5;
        break;

      case 0xD1:  // fall through
      case 0xD3:  // fall through
      case 0xC1:
        data += D1D3C1Instruction(data);
        break;

      case 0xD8:  // fall through
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
        if (*(data+1) == 0x0F) {
          byte b2 = *(data+2);
          if (b2 == 0x11) {
            AppendToBuffer("movsd ");
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            data += PrintRightXMMOperand(data);
            AppendToBuffer(",%s", NameOfXMMRegister(regop));
          } else if (b2 == 0x10) {
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movsd %s,", NameOfXMMRegister(regop));
            data += PrintRightXMMOperand(data);
          } else  if (b2 == 0x5A) {
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("cvtsd2ss %s,", NameOfXMMRegister(regop));
            data += PrintRightXMMOperand(data);
          } else {
            const char* mnem = "?";
            switch (b2) {
              case 0x2A: mnem = "cvtsi2sd"; break;
              case 0x2C: mnem = "cvttsd2si"; break;
              case 0x2D: mnem = "cvtsd2si"; break;
              case 0x51: mnem = "sqrtsd"; break;
              case 0x58: mnem = "addsd"; break;
              case 0x59: mnem = "mulsd"; break;
              case 0x5C: mnem = "subsd"; break;
              case 0x5E: mnem = "divsd"; break;
            }
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            if (b2 == 0x2A) {
              AppendToBuffer("%s %s,", mnem, NameOfXMMRegister(regop));
              data += PrintRightOperand(data);
            } else if (b2 == 0x2C || b2 == 0x2D) {
              AppendToBuffer("%s %s,", mnem, NameOfCPURegister(regop));
              data += PrintRightXMMOperand(data);
            } else if (b2 == 0xC2) {
              // Intel manual 2A, Table 3-18.
              const char* const pseudo_op[] = {
                "cmpeqsd",
                "cmpltsd",
                "cmplesd",
                "cmpunordsd",
                "cmpneqsd",
                "cmpnltsd",
                "cmpnlesd",
                "cmpordsd"
              };
              AppendToBuffer("%s %s,%s",
                             pseudo_op[data[1]],
                             NameOfXMMRegister(regop),
                             NameOfXMMRegister(rm));
              data += 2;
            } else {
              AppendToBuffer("%s %s,", mnem, NameOfXMMRegister(regop));
              data += PrintRightXMMOperand(data);
            }
          }
        } else {
          UnimplementedInstruction();
        }
        break;

      case 0xF3:
        if (*(data+1) == 0x0F) {
          byte b2 = *(data+2);
          if (b2 == 0x11) {
            AppendToBuffer("movss ");
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            data += PrintRightXMMOperand(data);
            AppendToBuffer(",%s", NameOfXMMRegister(regop));
          } else if (b2 == 0x10) {
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movss %s,", NameOfXMMRegister(regop));
            data += PrintRightXMMOperand(data);
          } else if (b2 == 0x2C) {
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("cvttss2si %s,", NameOfCPURegister(regop));
            data += PrintRightXMMOperand(data);
          } else if (b2 == 0x5A) {
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("cvtss2sd %s,", NameOfXMMRegister(regop));
            data += PrintRightXMMOperand(data);
          } else  if (b2 == 0x6F) {
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            AppendToBuffer("movdqu %s,", NameOfXMMRegister(regop));
            data += PrintRightXMMOperand(data);
          } else  if (b2 == 0x7F) {
            AppendToBuffer("movdqu ");
            data += 3;
            int mod, regop, rm;
            get_modrm(*data, &mod, &regop, &rm);
            data += PrintRightXMMOperand(data);
            AppendToBuffer(",%s", NameOfXMMRegister(regop));
          } else {
            UnimplementedInstruction();
          }
        } else if (*(data+1) == 0xA5) {
          data += 2;
          AppendToBuffer("rep_movs");
        } else if (*(data+1) == 0xAB) {
          data += 2;
          AppendToBuffer("rep_stos");
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
  }

  if (tmp_buffer_pos_ < sizeof tmp_buffer_) {
    tmp_buffer_[tmp_buffer_pos_] = '\0';
  }

  int instr_len = data - instr;
  if (instr_len == 0) {
    printf("%02x", *data);
  }
  ASSERT(instr_len > 0);  // Ensure progress.

  int outp = 0;
  // Instruction bytes.
  for (byte* bp = instr; bp < data; bp++) {
    outp += v8::internal::OS::SNPrintF(out_buffer + outp,
                                       "%02x",
                                       *bp);
  }
  for (int i = 6 - instr_len; i >= 0; i--) {
    outp += v8::internal::OS::SNPrintF(out_buffer + outp,
                                       "  ");
  }

  outp += v8::internal::OS::SNPrintF(out_buffer + outp,
                                     " %s",
                                     tmp_buffer_.start());
  return instr_len;
}  // NOLINT (function is too long)


//------------------------------------------------------------------------------


static const char* cpu_regs[8] = {
  "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"
};


static const char* byte_cpu_regs[8] = {
  "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"
};


static const char* xmm_regs[8] = {
  "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7"
};


const char* NameConverter::NameOfAddress(byte* addr) const {
  v8::internal::OS::SNPrintF(tmp_buffer_, "%p", addr);
  return tmp_buffer_.start();
}


const char* NameConverter::NameOfConstant(byte* addr) const {
  return NameOfAddress(addr);
}


const char* NameConverter::NameOfCPURegister(int reg) const {
  if (0 <= reg && reg < 8) return cpu_regs[reg];
  return "noreg";
}


const char* NameConverter::NameOfByteCPURegister(int reg) const {
  if (0 <= reg && reg < 8) return byte_cpu_regs[reg];
  return "noreg";
}


const char* NameConverter::NameOfXMMRegister(int reg) const {
  if (0 <= reg && reg < 8) return xmm_regs[reg];
  return "noxmmreg";
}


const char* NameConverter::NameInCode(byte* addr) const {
  // IA32 does not embed debug strings at the moment.
  UNREACHABLE();
  return "";
}


//------------------------------------------------------------------------------

Disassembler::Disassembler(const NameConverter& converter)
    : converter_(converter) {}


Disassembler::~Disassembler() {}


int Disassembler::InstructionDecode(v8::internal::Vector<char> buffer,
                                    byte* instruction) {
  DisassemblerIA32 d(converter_, false /*do not crash if unimplemented*/);
  return d.InstructionDecode(buffer, instruction);
}


// The IA-32 assembler does not currently use constant pools.
int Disassembler::ConstantPoolSizeAt(byte* instruction) { return -1; }


/*static*/ void Disassembler::Disassemble(FILE* f, byte* begin, byte* end) {
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
      fprintf(f, "%02x",  *bp);
    }
    for (int i = 6 - (pc - prev_pc); i >= 0; i--) {
      fprintf(f, "  ");
    }
    fprintf(f, "  %s\n", buffer.start());
  }
}


}  // namespace disasm

#endif  // V8_TARGET_ARCH_IA32

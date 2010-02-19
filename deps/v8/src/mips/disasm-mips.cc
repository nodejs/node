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

// A Disassembler object is used to disassemble a block of code instruction by
// instruction. The default implementation of the NameConverter object can be
// overriden to modify register names or to do symbol lookup on addresses.
//
// The example below will disassemble a block of code and print it to stdout.
//
//   NameConverter converter;
//   Disassembler d(converter);
//   for (byte_* pc = begin; pc < end;) {
//     char buffer[128];
//     buffer[0] = '\0';
//     byte_* prev_pc = pc;
//     pc += d.InstructionDecode(buffer, sizeof buffer, pc);
//     printf("%p    %08x      %s\n",
//            prev_pc, *reinterpret_cast<int32_t*>(prev_pc), buffer);
//   }
//
// The Disassembler class also has a convenience method to disassemble a block
// of code into a FILE*, meaning that the above functionality could also be
// achieved by just calling Disassembler::Disassemble(stdout, begin, end);


#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifndef WIN32
#include <stdint.h>
#endif

#include "v8.h"

#include "constants-mips.h"
#include "disasm.h"
#include "macro-assembler.h"
#include "platform.h"

namespace assembler {
namespace mips {


namespace v8i = v8::internal;


//------------------------------------------------------------------------------

// Decoder decodes and disassembles instructions into an output buffer.
// It uses the converter to convert register names and call destinations into
// more informative description.
class Decoder {
 public:
  Decoder(const disasm::NameConverter& converter,
          v8::internal::Vector<char> out_buffer)
    : converter_(converter),
      out_buffer_(out_buffer),
      out_buffer_pos_(0) {
    out_buffer_[out_buffer_pos_] = '\0';
  }

  ~Decoder() {}

  // Writes one disassembled instruction into 'buffer' (0-terminated).
  // Returns the length of the disassembled machine instruction in bytes.
  int InstructionDecode(byte_* instruction);

 private:
  // Bottleneck functions to print into the out_buffer.
  void PrintChar(const char ch);
  void Print(const char* str);

  // Printing of common values.
  void PrintRegister(int reg);
  void PrintCRegister(int creg);
  void PrintRs(Instruction* instr);
  void PrintRt(Instruction* instr);
  void PrintRd(Instruction* instr);
  void PrintFs(Instruction* instr);
  void PrintFt(Instruction* instr);
  void PrintFd(Instruction* instr);
  void PrintSa(Instruction* instr);
  void PrintFunction(Instruction* instr);
  void PrintSecondaryField(Instruction* instr);
  void PrintUImm16(Instruction* instr);
  void PrintSImm16(Instruction* instr);
  void PrintXImm16(Instruction* instr);
  void PrintImm26(Instruction* instr);
  void PrintCode(Instruction* instr);   // For break and trap instructions.
  // Printing of instruction name.
  void PrintInstructionName(Instruction* instr);

  // Handle formatting of instructions and their options.
  int FormatRegister(Instruction* instr, const char* option);
  int FormatCRegister(Instruction* instr, const char* option);
  int FormatOption(Instruction* instr, const char* option);
  void Format(Instruction* instr, const char* format);
  void Unknown(Instruction* instr);

  // Each of these functions decodes one particular instruction type.
  void DecodeTypeRegister(Instruction* instr);
  void DecodeTypeImmediate(Instruction* instr);
  void DecodeTypeJump(Instruction* instr);

  const disasm::NameConverter& converter_;
  v8::internal::Vector<char> out_buffer_;
  int out_buffer_pos_;

  DISALLOW_COPY_AND_ASSIGN(Decoder);
};


// Support for assertions in the Decoder formatting functions.
#define STRING_STARTS_WITH(string, compare_string) \
  (strncmp(string, compare_string, strlen(compare_string)) == 0)


// Append the ch to the output buffer.
void Decoder::PrintChar(const char ch) {
  out_buffer_[out_buffer_pos_++] = ch;
}


// Append the str to the output buffer.
void Decoder::Print(const char* str) {
  char cur = *str++;
  while (cur != '\0' && (out_buffer_pos_ < (out_buffer_.length() - 1))) {
    PrintChar(cur);
    cur = *str++;
  }
  out_buffer_[out_buffer_pos_] = 0;
}


// Print the register name according to the active name converter.
void Decoder::PrintRegister(int reg) {
  Print(converter_.NameOfCPURegister(reg));
}


void Decoder::PrintRs(Instruction* instr) {
  int reg = instr->RsField();
  PrintRegister(reg);
}


void Decoder::PrintRt(Instruction* instr) {
  int reg = instr->RtField();
  PrintRegister(reg);
}


void Decoder::PrintRd(Instruction* instr) {
  int reg = instr->RdField();
  PrintRegister(reg);
}


// Print the Cregister name according to the active name converter.
void Decoder::PrintCRegister(int creg) {
  Print(converter_.NameOfXMMRegister(creg));
}


void Decoder::PrintFs(Instruction* instr) {
  int creg = instr->RsField();
  PrintCRegister(creg);
}


void Decoder::PrintFt(Instruction* instr) {
  int creg = instr->RtField();
  PrintCRegister(creg);
}


void Decoder::PrintFd(Instruction* instr) {
  int creg = instr->RdField();
  PrintCRegister(creg);
}


// Print the integer value of the sa field.
void Decoder::PrintSa(Instruction* instr) {
  int sa = instr->SaField();
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "%d", sa);
}


// Print 16-bit unsigned immediate value.
void Decoder::PrintUImm16(Instruction* instr) {
  int32_t imm = instr->Imm16Field();
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "%u", imm);
}


// Print 16-bit signed immediate value.
void Decoder::PrintSImm16(Instruction* instr) {
  int32_t imm = ((instr->Imm16Field())<<16)>>16;
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "%d", imm);
}


// Print 16-bit hexa immediate value.
void Decoder::PrintXImm16(Instruction* instr) {
  int32_t imm = instr->Imm16Field();
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "0x%x", imm);
}


// Print 26-bit immediate value.
void Decoder::PrintImm26(Instruction* instr) {
  int32_t imm = instr->Imm26Field();
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "%d", imm);
}


// Print 26-bit immediate value.
void Decoder::PrintCode(Instruction* instr) {
  if (instr->OpcodeFieldRaw() != SPECIAL)
    return;  // Not a break or trap instruction.
  switch (instr->FunctionFieldRaw()) {
    case BREAK: {
      int32_t code = instr->Bits(25, 6);
      out_buffer_pos_ +=
          v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%05x", code);
      break;
                }
    case TGE:
    case TGEU:
    case TLT:
    case TLTU:
    case TEQ:
    case TNE: {
      int32_t code = instr->Bits(15, 6);
      out_buffer_pos_ +=
          v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%03x", code);
      break;
    }
    default:  // Not a break or trap instruction.
    break;
  };
}


// Printing of instruction name.
void Decoder::PrintInstructionName(Instruction* instr) {
}


// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instruction* instr, const char* format) {
  ASSERT(format[0] == 'r');
  if (format[1] == 's') {  // 'rs: Rs register
    int reg = instr->RsField();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 't') {  // 'rt: rt register
    int reg = instr->RtField();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'rd: rd register
    int reg = instr->RdField();
    PrintRegister(reg);
    return 2;
  }
  UNREACHABLE();
  return -1;
}


// Handle all Cregister based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatCRegister(Instruction* instr, const char* format) {
  ASSERT(format[0] == 'f');
  if (format[1] == 's') {  // 'fs: fs register
    int reg = instr->RsField();
    PrintCRegister(reg);
    return 2;
  } else if (format[1] == 't') {  // 'ft: ft register
    int reg = instr->RtField();
    PrintCRegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'fd: fd register
    int reg = instr->RdField();
    PrintCRegister(reg);
    return 2;
  }
  UNREACHABLE();
  return -1;
}


// FormatOption takes a formatting string and interprets it based on
// the current instructions. The format string points to the first
// character of the option string (the option escape has already been
// consumed by the caller.)  FormatOption returns the number of
// characters that were consumed from the formatting string.
int Decoder::FormatOption(Instruction* instr, const char* format) {
  switch (format[0]) {
    case 'c': {   // 'code for break or trap instructions
      ASSERT(STRING_STARTS_WITH(format, "code"));
      PrintCode(instr);
      return 4;
    }
    case 'i': {   // 'imm16u or 'imm26
      if (format[3] == '1') {
        ASSERT(STRING_STARTS_WITH(format, "imm16"));
        if (format[5] == 's') {
          ASSERT(STRING_STARTS_WITH(format, "imm16s"));
          PrintSImm16(instr);
        } else if (format[5] == 'u') {
          ASSERT(STRING_STARTS_WITH(format, "imm16u"));
          PrintSImm16(instr);
        } else {
          ASSERT(STRING_STARTS_WITH(format, "imm16x"));
          PrintXImm16(instr);
        }
        return 6;
      } else {
        ASSERT(STRING_STARTS_WITH(format, "imm26"));
        PrintImm26(instr);
        return 5;
      }
    }
    case 'r': {   // 'r: registers
      return FormatRegister(instr, format);
    }
    case 'f': {   // 'f: Cregisters
      return FormatCRegister(instr, format);
    }
    case 's': {   // 'sa
      ASSERT(STRING_STARTS_WITH(format, "sa"));
      PrintSa(instr);
      return 2;
    }
  };
  UNREACHABLE();
  return -1;
}


// Format takes a formatting string for a whole instruction and prints it into
// the output buffer. All escaped options are handed to FormatOption to be
// parsed further.
void Decoder::Format(Instruction* instr, const char* format) {
  char cur = *format++;
  while ((cur != 0) && (out_buffer_pos_ < (out_buffer_.length() - 1))) {
    if (cur == '\'') {  // Single quote is used as the formatting escape.
      format += FormatOption(instr, format);
    } else {
      out_buffer_[out_buffer_pos_++] = cur;
    }
    cur = *format++;
  }
  out_buffer_[out_buffer_pos_]  = '\0';
}


// For currently unimplemented decodings the disassembler calls Unknown(instr)
// which will just print "unknown" of the instruction bits.
void Decoder::Unknown(Instruction* instr) {
  Format(instr, "unknown");
}


void Decoder::DecodeTypeRegister(Instruction* instr) {
  switch (instr->OpcodeFieldRaw()) {
    case COP1:    // Coprocessor instructions
      switch (instr->RsFieldRaw()) {
        case BC1:   // branch on coprocessor condition
          UNREACHABLE();
          break;
        case MFC1:
          Format(instr, "mfc1 'rt, 'fs");
          break;
        case MFHC1:
          Format(instr, "mfhc1  rt, 'fs");
          break;
        case MTC1:
          Format(instr, "mtc1 'rt, 'fs");
          break;
        case MTHC1:
          Format(instr, "mthc1  rt, 'fs");
          break;
        case S:
        case D:
          UNIMPLEMENTED_MIPS();
          break;
        case W:
          switch (instr->FunctionFieldRaw()) {
            case CVT_S_W:
              UNIMPLEMENTED_MIPS();
              break;
            case CVT_D_W:   // Convert word to double.
              Format(instr, "cvt.d.w  'fd, 'fs");
              break;
            default:
              UNREACHABLE();
          };
          break;
        case L:
        case PS:
          UNIMPLEMENTED_MIPS();
          break;
          break;
        default:
          UNREACHABLE();
      };
      break;
    case SPECIAL:
      switch (instr->FunctionFieldRaw()) {
        case JR:
          Format(instr, "jr   'rs");
          break;
        case JALR:
          Format(instr, "jalr 'rs");
          break;
        case SLL:
          if ( 0x0 == static_cast<int>(instr->InstructionBits()))
            Format(instr, "nop");
          else
            Format(instr, "sll  'rd, 'rt, 'sa");
          break;
        case SRL:
          Format(instr, "srl  'rd, 'rt, 'sa");
          break;
        case SRA:
          Format(instr, "sra  'rd, 'rt, 'sa");
          break;
        case SLLV:
          Format(instr, "sllv 'rd, 'rt, 'rs");
          break;
        case SRLV:
          Format(instr, "srlv 'rd, 'rt, 'rs");
          break;
        case SRAV:
          Format(instr, "srav 'rd, 'rt, 'rs");
          break;
        case MFHI:
          Format(instr, "mfhi 'rd");
          break;
        case MFLO:
          Format(instr, "mflo 'rd");
          break;
        case MULT:
          Format(instr, "mult 'rs, 'rt");
          break;
        case MULTU:
          Format(instr, "multu  'rs, 'rt");
          break;
        case DIV:
          Format(instr, "div  'rs, 'rt");
          break;
        case DIVU:
          Format(instr, "divu 'rs, 'rt");
          break;
        case ADD:
          Format(instr, "add  'rd, 'rs, 'rt");
          break;
        case ADDU:
          Format(instr, "addu 'rd, 'rs, 'rt");
          break;
        case SUB:
          Format(instr, "sub  'rd, 'rs, 'rt");
          break;
        case SUBU:
          Format(instr, "sub  'rd, 'rs, 'rt");
          break;
        case AND:
          Format(instr, "and  'rd, 'rs, 'rt");
          break;
        case OR:
          if (0 == instr->RsField()) {
            Format(instr, "mov  'rd, 'rt");
          } else if (0 == instr->RtField()) {
            Format(instr, "mov  'rd, 'rs");
          } else {
            Format(instr, "or   'rd, 'rs, 'rt");
          }
          break;
        case XOR:
          Format(instr, "xor  'rd, 'rs, 'rt");
          break;
        case NOR:
          Format(instr, "nor  'rd, 'rs, 'rt");
          break;
        case SLT:
          Format(instr, "slt  'rd, 'rs, 'rt");
          break;
        case SLTU:
          Format(instr, "sltu 'rd, 'rs, 'rt");
          break;
        case BREAK:
          Format(instr, "break, code: 'code");
          break;
        case TGE:
          Format(instr, "tge  'rs, 'rt, code: 'code");
          break;
        case TGEU:
          Format(instr, "tgeu 'rs, 'rt, code: 'code");
          break;
        case TLT:
          Format(instr, "tlt  'rs, 'rt, code: 'code");
          break;
        case TLTU:
          Format(instr, "tltu 'rs, 'rt, code: 'code");
          break;
        case TEQ:
          Format(instr, "teq  'rs, 'rt, code: 'code");
          break;
        case TNE:
          Format(instr, "tne  'rs, 'rt, code: 'code");
          break;
        default:
          UNREACHABLE();
      };
      break;
    case SPECIAL2:
      switch (instr->FunctionFieldRaw()) {
        case MUL:
          break;
        default:
          UNREACHABLE();
      };
      break;
    default:
      UNREACHABLE();
  };
}


void Decoder::DecodeTypeImmediate(Instruction* instr) {
  switch (instr->OpcodeFieldRaw()) {
    // ------------- REGIMM class.
    case REGIMM:
      switch (instr->RtFieldRaw()) {
        case BLTZ:
          Format(instr, "bltz 'rs, 'imm16u");
          break;
        case BLTZAL:
          Format(instr, "bltzal 'rs, 'imm16u");
          break;
        case BGEZ:
          Format(instr, "bgez 'rs, 'imm16u");
          break;
        case BGEZAL:
          Format(instr, "bgezal 'rs, 'imm16u");
          break;
        default:
          UNREACHABLE();
      };
    break;  // case REGIMM
    // ------------- Branch instructions.
    case BEQ:
      Format(instr, "beq  'rs, 'rt, 'imm16u");
      break;
    case BNE:
      Format(instr, "bne  'rs, 'rt, 'imm16u");
      break;
    case BLEZ:
      Format(instr, "blez 'rs, 'imm16u");
      break;
    case BGTZ:
      Format(instr, "bgtz 'rs, 'imm16u");
      break;
    // ------------- Arithmetic instructions.
    case ADDI:
      Format(instr, "addi   'rt, 'rs, 'imm16s");
      break;
    case ADDIU:
      Format(instr, "addiu  'rt, 'rs, 'imm16s");
      break;
    case SLTI:
      Format(instr, "slti   'rt, 'rs, 'imm16s");
      break;
    case SLTIU:
      Format(instr, "sltiu  'rt, 'rs, 'imm16u");
      break;
    case ANDI:
      Format(instr, "andi   'rt, 'rs, 'imm16x");
      break;
    case ORI:
      Format(instr, "ori    'rt, 'rs, 'imm16x");
      break;
    case XORI:
      Format(instr, "xori   'rt, 'rs, 'imm16x");
      break;
    case LUI:
      Format(instr, "lui    'rt, 'imm16x");
      break;
    // ------------- Memory instructions.
    case LB:
      Format(instr, "lb     'rt, 'imm16s('rs)");
      break;
    case LW:
      Format(instr, "lw     'rt, 'imm16s('rs)");
      break;
    case LBU:
      Format(instr, "lbu    'rt, 'imm16s('rs)");
      break;
    case SB:
      Format(instr, "sb     'rt, 'imm16s('rs)");
      break;
    case SW:
      Format(instr, "sw     'rt, 'imm16s('rs)");
      break;
    case LWC1:
      Format(instr, "lwc1   'ft, 'imm16s('rs)");
      break;
    case LDC1:
      Format(instr, "ldc1   'ft, 'imm16s('rs)");
      break;
    case SWC1:
      Format(instr, "swc1   'rt, 'imm16s('fs)");
      break;
    case SDC1:
      Format(instr, "sdc1   'rt, 'imm16s('fs)");
      break;
    default:
      UNREACHABLE();
      break;
  };
}


void Decoder::DecodeTypeJump(Instruction* instr) {
  switch (instr->OpcodeFieldRaw()) {
    case J:
      Format(instr, "j    'imm26");
      break;
    case JAL:
      Format(instr, "jal  'imm26");
      break;
    default:
      UNREACHABLE();
  }
}


// Disassemble the instruction at *instr_ptr into the output buffer.
int Decoder::InstructionDecode(byte_* instr_ptr) {
  Instruction* instr = Instruction::At(instr_ptr);
  // Print raw instruction bytes.
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "%08x       ",
                                       instr->InstructionBits());
  switch (instr->InstructionType()) {
    case Instruction::kRegisterType: {
      DecodeTypeRegister(instr);
      break;
    }
    case Instruction::kImmediateType: {
      DecodeTypeImmediate(instr);
      break;
    }
    case Instruction::kJumpType: {
      DecodeTypeJump(instr);
      break;
    }
    default: {
      UNSUPPORTED_MIPS();
    }
  }
  return Instruction::kInstructionSize;
}


} }  // namespace assembler::mips



//------------------------------------------------------------------------------

namespace disasm {

namespace v8i = v8::internal;


const char* NameConverter::NameOfAddress(byte_* addr) const {
  static v8::internal::EmbeddedVector<char, 32> tmp_buffer;
  v8::internal::OS::SNPrintF(tmp_buffer, "%p", addr);
  return tmp_buffer.start();
}


const char* NameConverter::NameOfConstant(byte_* addr) const {
  return NameOfAddress(addr);
}


const char* NameConverter::NameOfCPURegister(int reg) const {
  return assembler::mips::Registers::Name(reg);
}


const char* NameConverter::NameOfXMMRegister(int reg) const {
  return assembler::mips::FPURegister::Name(reg);
}


const char* NameConverter::NameOfByteCPURegister(int reg) const {
  UNREACHABLE();  // MIPS does not have the concept of a byte register
  return "nobytereg";
}


const char* NameConverter::NameInCode(byte_* addr) const {
  // The default name converter is called for unknown code. So we will not try
  // to access any memory.
  return "";
}


//------------------------------------------------------------------------------

Disassembler::Disassembler(const NameConverter& converter)
    : converter_(converter) {}


Disassembler::~Disassembler() {}


int Disassembler::InstructionDecode(v8::internal::Vector<char> buffer,
                                    byte_* instruction) {
  assembler::mips::Decoder d(converter_, buffer);
  return d.InstructionDecode(instruction);
}


int Disassembler::ConstantPoolSizeAt(byte_* instruction) {
  UNIMPLEMENTED_MIPS();
  return -1;
}


void Disassembler::Disassemble(FILE* f, byte_* begin, byte_* end) {
  NameConverter converter;
  Disassembler d(converter);
  for (byte_* pc = begin; pc < end;) {
    v8::internal::EmbeddedVector<char, 128> buffer;
    buffer[0] = '\0';
    byte_* prev_pc = pc;
    pc += d.InstructionDecode(buffer, pc);
    fprintf(f, "%p    %08x      %s\n",
            prev_pc, *reinterpret_cast<int32_t*>(prev_pc), buffer.start());
  }
}

#undef UNSUPPORTED

}  // namespace disasm


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

// A Disassembler object is used to disassemble a block of code instruction by
// instruction. The default implementation of the NameConverter object can be
// overriden to modify register names or to do symbol lookup on addresses.
//
// The example below will disassemble a block of code and print it to stdout.
//
//   NameConverter converter;
//   Disassembler d(converter);
//   for (byte* pc = begin; pc < end;) {
//     v8::internal::EmbeddedVector<char, 256> buffer;
//     byte* prev_pc = pc;
//     pc += d.InstructionDecode(buffer, pc);
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

#if defined(V8_TARGET_ARCH_MIPS)

#include "mips/constants-mips.h"
#include "disasm.h"
#include "macro-assembler.h"
#include "platform.h"

namespace v8 {
namespace internal {

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
  int InstructionDecode(byte* instruction);

 private:
  // Bottleneck functions to print into the out_buffer.
  void PrintChar(const char ch);
  void Print(const char* str);

  // Printing of common values.
  void PrintRegister(int reg);
  void PrintFPURegister(int freg);
  void PrintRs(Instruction* instr);
  void PrintRt(Instruction* instr);
  void PrintRd(Instruction* instr);
  void PrintFs(Instruction* instr);
  void PrintFt(Instruction* instr);
  void PrintFd(Instruction* instr);
  void PrintSa(Instruction* instr);
  void PrintSd(Instruction* instr);
  void PrintSs1(Instruction* instr);
  void PrintSs2(Instruction* instr);
  void PrintBc(Instruction* instr);
  void PrintCc(Instruction* instr);
  void PrintFunction(Instruction* instr);
  void PrintSecondaryField(Instruction* instr);
  void PrintUImm16(Instruction* instr);
  void PrintSImm16(Instruction* instr);
  void PrintXImm16(Instruction* instr);
  void PrintXImm26(Instruction* instr);
  void PrintCode(Instruction* instr);   // For break and trap instructions.
  // Printing of instruction name.
  void PrintInstructionName(Instruction* instr);

  // Handle formatting of instructions and their options.
  int FormatRegister(Instruction* instr, const char* option);
  int FormatFPURegister(Instruction* instr, const char* option);
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
  int reg = instr->RsValue();
  PrintRegister(reg);
}


void Decoder::PrintRt(Instruction* instr) {
  int reg = instr->RtValue();
  PrintRegister(reg);
}


void Decoder::PrintRd(Instruction* instr) {
  int reg = instr->RdValue();
  PrintRegister(reg);
}


// Print the FPUregister name according to the active name converter.
void Decoder::PrintFPURegister(int freg) {
  Print(converter_.NameOfXMMRegister(freg));
}


void Decoder::PrintFs(Instruction* instr) {
  int freg = instr->RsValue();
  PrintFPURegister(freg);
}


void Decoder::PrintFt(Instruction* instr) {
  int freg = instr->RtValue();
  PrintFPURegister(freg);
}


void Decoder::PrintFd(Instruction* instr) {
  int freg = instr->RdValue();
  PrintFPURegister(freg);
}


// Print the integer value of the sa field.
void Decoder::PrintSa(Instruction* instr) {
  int sa = instr->SaValue();
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", sa);
}


// Print the integer value of the rd field, when it is not used as reg.
void Decoder::PrintSd(Instruction* instr) {
  int sd = instr->RdValue();
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", sd);
}


// Print the integer value of the rd field, when used as 'ext' size.
void Decoder::PrintSs1(Instruction* instr) {
  int ss = instr->RdValue();
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", ss + 1);
}


// Print the integer value of the rd field, when used as 'ins' size.
void Decoder::PrintSs2(Instruction* instr) {
  int ss = instr->RdValue();
  int pos = instr->SaValue();
  out_buffer_pos_ +=
      OS::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", ss - pos + 1);
}


// Print the integer value of the cc field for the bc1t/f instructions.
void Decoder::PrintBc(Instruction* instr) {
  int cc = instr->FBccValue();
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", cc);
}


// Print the integer value of the cc field for the FP compare instructions.
void Decoder::PrintCc(Instruction* instr) {
  int cc = instr->FCccValue();
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "cc(%d)", cc);
}


// Print 16-bit unsigned immediate value.
void Decoder::PrintUImm16(Instruction* instr) {
  int32_t imm = instr->Imm16Value();
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", imm);
}


// Print 16-bit signed immediate value.
void Decoder::PrintSImm16(Instruction* instr) {
  int32_t imm = ((instr->Imm16Value()) << 16) >> 16;
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm);
}


// Print 16-bit hexa immediate value.
void Decoder::PrintXImm16(Instruction* instr) {
  int32_t imm = instr->Imm16Value();
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 26-bit immediate value.
void Decoder::PrintXImm26(Instruction* instr) {
  uint32_t imm = instr->Imm26Value() << kImmFieldShift;
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 26-bit immediate value.
void Decoder::PrintCode(Instruction* instr) {
  if (instr->OpcodeFieldRaw() != SPECIAL)
    return;  // Not a break or trap instruction.
  switch (instr->FunctionFieldRaw()) {
    case BREAK: {
      int32_t code = instr->Bits(25, 6);
      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "0x%05x (%d)", code, code);
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
          OS::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%03x", code);
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
  if (format[1] == 's') {  // 'rs: Rs register.
    int reg = instr->RsValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 't') {  // 'rt: rt register.
    int reg = instr->RtValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'rd: rd register.
    int reg = instr->RdValue();
    PrintRegister(reg);
    return 2;
  }
  UNREACHABLE();
  return -1;
}


// Handle all FPUregister based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatFPURegister(Instruction* instr, const char* format) {
  ASSERT(format[0] == 'f');
  if (format[1] == 's') {  // 'fs: fs register.
    int reg = instr->FsValue();
    PrintFPURegister(reg);
    return 2;
  } else if (format[1] == 't') {  // 'ft: ft register.
    int reg = instr->FtValue();
    PrintFPURegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'fd: fd register.
    int reg = instr->FdValue();
    PrintFPURegister(reg);
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
    case 'c': {   // 'code for break or trap instructions.
      ASSERT(STRING_STARTS_WITH(format, "code"));
      PrintCode(instr);
      return 4;
    }
    case 'i': {   // 'imm16u or 'imm26.
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
        ASSERT(STRING_STARTS_WITH(format, "imm26x"));
        PrintXImm26(instr);
        return 6;
      }
    }
    case 'r': {   // 'r: registers.
      return FormatRegister(instr, format);
    }
    case 'f': {   // 'f: FPUregisters.
      return FormatFPURegister(instr, format);
    }
    case 's': {   // 'sa.
      switch (format[1]) {
        case 'a': {
          ASSERT(STRING_STARTS_WITH(format, "sa"));
          PrintSa(instr);
          return 2;
        }
        case 'd': {
          ASSERT(STRING_STARTS_WITH(format, "sd"));
          PrintSd(instr);
          return 2;
        }
        case 's': {
          if (format[2] == '1') {
              ASSERT(STRING_STARTS_WITH(format, "ss1"));  /* ext size */
              PrintSs1(instr);
              return 3;
          } else {
              ASSERT(STRING_STARTS_WITH(format, "ss2"));  /* ins size */
              PrintSs2(instr);
              return 3;
          }
        }
      }
    }
    case 'b': {   // 'bc - Special for bc1 cc field.
      ASSERT(STRING_STARTS_WITH(format, "bc"));
      PrintBc(instr);
      return 2;
    }
    case 'C': {   // 'Cc - Special for c.xx.d cc field.
      ASSERT(STRING_STARTS_WITH(format, "Cc"));
      PrintCc(instr);
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
    case COP1:    // Coprocessor instructions.
      switch (instr->RsFieldRaw()) {
        case BC1:   // bc1 handled in DecodeTypeImmediate.
          UNREACHABLE();
          break;
        case MFC1:
          Format(instr, "mfc1    'rt, 'fs");
          break;
        case MFHC1:
          Format(instr, "mfhc1   'rt, 'fs");
          break;
        case MTC1:
          Format(instr, "mtc1    'rt, 'fs");
          break;
        // These are called "fs" too, although they are not FPU registers.
        case CTC1:
          Format(instr, "ctc1    'rt, 'fs");
          break;
        case CFC1:
          Format(instr, "cfc1    'rt, 'fs");
          break;
        case MTHC1:
          Format(instr, "mthc1   'rt, 'fs");
          break;
        case D:
          switch (instr->FunctionFieldRaw()) {
            case ADD_D:
              Format(instr, "add.d   'fd, 'fs, 'ft");
              break;
            case SUB_D:
              Format(instr, "sub.d   'fd, 'fs, 'ft");
              break;
            case MUL_D:
              Format(instr, "mul.d   'fd, 'fs, 'ft");
              break;
            case DIV_D:
              Format(instr, "div.d   'fd, 'fs, 'ft");
              break;
            case ABS_D:
              Format(instr, "abs.d   'fd, 'fs");
              break;
            case MOV_D:
              Format(instr, "mov.d   'fd, 'fs");
              break;
            case NEG_D:
              Format(instr, "neg.d   'fd, 'fs");
              break;
            case SQRT_D:
              Format(instr, "sqrt.d  'fd, 'fs");
              break;
            case CVT_W_D:
              Format(instr, "cvt.w.d 'fd, 'fs");
              break;
            case CVT_L_D: {
              if (mips32r2) {
                Format(instr, "cvt.l.d 'fd, 'fs");
              } else {
                Unknown(instr);
              }
              break;
            }
            case TRUNC_W_D:
              Format(instr, "trunc.w.d 'fd, 'fs");
              break;
            case TRUNC_L_D: {
              if (mips32r2) {
                Format(instr, "trunc.l.d 'fd, 'fs");
              } else {
                Unknown(instr);
              }
              break;
            }
            case ROUND_W_D:
              Format(instr, "round.w.d 'fd, 'fs");
              break;
            case FLOOR_W_D:
              Format(instr, "floor.w.d 'fd, 'fs");
              break;
            case CEIL_W_D:
              Format(instr, "ceil.w.d 'fd, 'fs");
              break;
            case CVT_S_D:
              Format(instr, "cvt.s.d 'fd, 'fs");
              break;
            case C_F_D:
              Format(instr, "c.f.d   'fs, 'ft, 'Cc");
              break;
            case C_UN_D:
              Format(instr, "c.un.d  'fs, 'ft, 'Cc");
              break;
            case C_EQ_D:
              Format(instr, "c.eq.d  'fs, 'ft, 'Cc");
              break;
            case C_UEQ_D:
              Format(instr, "c.ueq.d 'fs, 'ft, 'Cc");
              break;
            case C_OLT_D:
              Format(instr, "c.olt.d 'fs, 'ft, 'Cc");
              break;
            case C_ULT_D:
              Format(instr, "c.ult.d 'fs, 'ft, 'Cc");
              break;
            case C_OLE_D:
              Format(instr, "c.ole.d 'fs, 'ft, 'Cc");
              break;
            case C_ULE_D:
              Format(instr, "c.ule.d 'fs, 'ft, 'Cc");
              break;
            default:
              Format(instr, "unknown.cop1.d");
              break;
          }
          break;
        case S:
          UNIMPLEMENTED_MIPS();
          break;
        case W:
          switch (instr->FunctionFieldRaw()) {
            case CVT_S_W:   // Convert word to float (single).
              Format(instr, "cvt.s.w 'fd, 'fs");
              break;
            case CVT_D_W:   // Convert word to double.
              Format(instr, "cvt.d.w 'fd, 'fs");
              break;
            default:
              UNREACHABLE();
          }
          break;
        case L:
          switch (instr->FunctionFieldRaw()) {
            case CVT_D_L: {
              if (mips32r2) {
                Format(instr, "cvt.d.l 'fd, 'fs");
              } else {
                Unknown(instr);
              }
              break;
            }
            case CVT_S_L: {
              if (mips32r2) {
                Format(instr, "cvt.s.l 'fd, 'fs");
              } else {
                Unknown(instr);
              }
              break;
            }
            default:
              UNREACHABLE();
          }
          break;
        case PS:
          UNIMPLEMENTED_MIPS();
          break;
        default:
          UNREACHABLE();
      }
      break;
    case SPECIAL:
      switch (instr->FunctionFieldRaw()) {
        case JR:
          Format(instr, "jr      'rs");
          break;
        case JALR:
          Format(instr, "jalr    'rs");
          break;
        case SLL:
          if ( 0x0 == static_cast<int>(instr->InstructionBits()))
            Format(instr, "nop");
          else
            Format(instr, "sll     'rd, 'rt, 'sa");
          break;
        case SRL:
          if (instr->RsValue() == 0) {
            Format(instr, "srl     'rd, 'rt, 'sa");
          } else {
            if (mips32r2) {
              Format(instr, "rotr    'rd, 'rt, 'sa");
            } else {
              Unknown(instr);
            }
          }
          break;
        case SRA:
          Format(instr, "sra     'rd, 'rt, 'sa");
          break;
        case SLLV:
          Format(instr, "sllv    'rd, 'rt, 'rs");
          break;
        case SRLV:
          if (instr->SaValue() == 0) {
            Format(instr, "srlv    'rd, 'rt, 'rs");
          } else {
            if (mips32r2) {
              Format(instr, "rotrv   'rd, 'rt, 'rs");
            } else {
              Unknown(instr);
            }
          }
          break;
        case SRAV:
          Format(instr, "srav    'rd, 'rt, 'rs");
          break;
        case MFHI:
          Format(instr, "mfhi    'rd");
          break;
        case MFLO:
          Format(instr, "mflo    'rd");
          break;
        case MULT:
          Format(instr, "mult    'rs, 'rt");
          break;
        case MULTU:
          Format(instr, "multu   'rs, 'rt");
          break;
        case DIV:
          Format(instr, "div     'rs, 'rt");
          break;
        case DIVU:
          Format(instr, "divu    'rs, 'rt");
          break;
        case ADD:
          Format(instr, "add     'rd, 'rs, 'rt");
          break;
        case ADDU:
          Format(instr, "addu    'rd, 'rs, 'rt");
          break;
        case SUB:
          Format(instr, "sub     'rd, 'rs, 'rt");
          break;
        case SUBU:
          Format(instr, "subu    'rd, 'rs, 'rt");
          break;
        case AND:
          Format(instr, "and     'rd, 'rs, 'rt");
          break;
        case OR:
          if (0 == instr->RsValue()) {
            Format(instr, "mov     'rd, 'rt");
          } else if (0 == instr->RtValue()) {
            Format(instr, "mov     'rd, 'rs");
          } else {
            Format(instr, "or      'rd, 'rs, 'rt");
          }
          break;
        case XOR:
          Format(instr, "xor     'rd, 'rs, 'rt");
          break;
        case NOR:
          Format(instr, "nor     'rd, 'rs, 'rt");
          break;
        case SLT:
          Format(instr, "slt     'rd, 'rs, 'rt");
          break;
        case SLTU:
          Format(instr, "sltu    'rd, 'rs, 'rt");
          break;
        case BREAK:
          Format(instr, "break, code: 'code");
          break;
        case TGE:
          Format(instr, "tge     'rs, 'rt, code: 'code");
          break;
        case TGEU:
          Format(instr, "tgeu    'rs, 'rt, code: 'code");
          break;
        case TLT:
          Format(instr, "tlt     'rs, 'rt, code: 'code");
          break;
        case TLTU:
          Format(instr, "tltu    'rs, 'rt, code: 'code");
          break;
        case TEQ:
          Format(instr, "teq     'rs, 'rt, code: 'code");
          break;
        case TNE:
          Format(instr, "tne     'rs, 'rt, code: 'code");
          break;
        case MOVZ:
          Format(instr, "movz    'rd, 'rs, 'rt");
          break;
        case MOVN:
          Format(instr, "movn    'rd, 'rs, 'rt");
          break;
        case MOVCI:
          if (instr->Bit(16)) {
            Format(instr, "movt    'rd, 'rs, 'bc");
          } else {
            Format(instr, "movf    'rd, 'rs, 'bc");
          }
          break;
        default:
          UNREACHABLE();
      }
      break;
    case SPECIAL2:
      switch (instr->FunctionFieldRaw()) {
        case MUL:
          Format(instr, "mul     'rd, 'rs, 'rt");
          break;
        case CLZ:
          Format(instr, "clz     'rd, 'rs");
          break;
        default:
          UNREACHABLE();
      }
      break;
    case SPECIAL3:
      switch (instr->FunctionFieldRaw()) {
        case INS: {
          if (mips32r2) {
            Format(instr, "ins     'rt, 'rs, 'sa, 'ss2");
          } else {
            Unknown(instr);
          }
          break;
        }
        case EXT: {
          if (mips32r2) {
            Format(instr, "ext     'rt, 'rs, 'sa, 'ss1");
          } else {
            Unknown(instr);
          }
          break;
        }
        default:
          UNREACHABLE();
      }
      break;
    default:
      UNREACHABLE();
  }
}


void Decoder::DecodeTypeImmediate(Instruction* instr) {
  switch (instr->OpcodeFieldRaw()) {
    // ------------- REGIMM class.
    case COP1:
      switch (instr->RsFieldRaw()) {
        case BC1:
          if (instr->FBtrueValue()) {
            Format(instr, "bc1t    'bc, 'imm16u");
          } else {
            Format(instr, "bc1f    'bc, 'imm16u");
          }
          break;
        default:
          UNREACHABLE();
      };
      break;  // Case COP1.
    case REGIMM:
      switch (instr->RtFieldRaw()) {
        case BLTZ:
          Format(instr, "bltz    'rs, 'imm16u");
          break;
        case BLTZAL:
          Format(instr, "bltzal  'rs, 'imm16u");
          break;
        case BGEZ:
          Format(instr, "bgez    'rs, 'imm16u");
          break;
        case BGEZAL:
          Format(instr, "bgezal  'rs, 'imm16u");
          break;
        default:
          UNREACHABLE();
      }
    break;  // Case REGIMM.
    // ------------- Branch instructions.
    case BEQ:
      Format(instr, "beq     'rs, 'rt, 'imm16u");
      break;
    case BNE:
      Format(instr, "bne     'rs, 'rt, 'imm16u");
      break;
    case BLEZ:
      Format(instr, "blez    'rs, 'imm16u");
      break;
    case BGTZ:
      Format(instr, "bgtz    'rs, 'imm16u");
      break;
    // ------------- Arithmetic instructions.
    case ADDI:
      Format(instr, "addi    'rt, 'rs, 'imm16s");
      break;
    case ADDIU:
      Format(instr, "addiu   'rt, 'rs, 'imm16s");
      break;
    case SLTI:
      Format(instr, "slti    'rt, 'rs, 'imm16s");
      break;
    case SLTIU:
      Format(instr, "sltiu   'rt, 'rs, 'imm16u");
      break;
    case ANDI:
      Format(instr, "andi    'rt, 'rs, 'imm16x");
      break;
    case ORI:
      Format(instr, "ori     'rt, 'rs, 'imm16x");
      break;
    case XORI:
      Format(instr, "xori    'rt, 'rs, 'imm16x");
      break;
    case LUI:
      Format(instr, "lui     'rt, 'imm16x");
      break;
    // ------------- Memory instructions.
    case LB:
      Format(instr, "lb      'rt, 'imm16s('rs)");
      break;
    case LH:
      Format(instr, "lh      'rt, 'imm16s('rs)");
      break;
    case LWL:
      Format(instr, "lwl     'rt, 'imm16s('rs)");
      break;
    case LW:
      Format(instr, "lw      'rt, 'imm16s('rs)");
      break;
    case LBU:
      Format(instr, "lbu     'rt, 'imm16s('rs)");
      break;
    case LHU:
      Format(instr, "lhu     'rt, 'imm16s('rs)");
      break;
    case LWR:
      Format(instr, "lwr     'rt, 'imm16s('rs)");
      break;
    case SB:
      Format(instr, "sb      'rt, 'imm16s('rs)");
      break;
    case SH:
      Format(instr, "sh      'rt, 'imm16s('rs)");
      break;
    case SWL:
      Format(instr, "swl     'rt, 'imm16s('rs)");
      break;
    case SW:
      Format(instr, "sw      'rt, 'imm16s('rs)");
      break;
    case SWR:
      Format(instr, "swr     'rt, 'imm16s('rs)");
      break;
    case LWC1:
      Format(instr, "lwc1    'ft, 'imm16s('rs)");
      break;
    case LDC1:
      Format(instr, "ldc1    'ft, 'imm16s('rs)");
      break;
    case SWC1:
      Format(instr, "swc1    'ft, 'imm16s('rs)");
      break;
    case SDC1:
      Format(instr, "sdc1    'ft, 'imm16s('rs)");
      break;
    default:
      UNREACHABLE();
      break;
  };
}


void Decoder::DecodeTypeJump(Instruction* instr) {
  switch (instr->OpcodeFieldRaw()) {
    case J:
      Format(instr, "j       'imm26x");
      break;
    case JAL:
      Format(instr, "jal     'imm26x");
      break;
    default:
      UNREACHABLE();
  }
}


// Disassemble the instruction at *instr_ptr into the output buffer.
int Decoder::InstructionDecode(byte* instr_ptr) {
  Instruction* instr = Instruction::At(instr_ptr);
  // Print raw instruction bytes.
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
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
      Format(instr, "UNSUPPORTED");
      UNSUPPORTED_MIPS();
    }
  }
  return Instruction::kInstrSize;
}


} }  // namespace v8::internal



//------------------------------------------------------------------------------

namespace disasm {

const char* NameConverter::NameOfAddress(byte* addr) const {
  v8::internal::OS::SNPrintF(tmp_buffer_, "%p", addr);
  return tmp_buffer_.start();
}


const char* NameConverter::NameOfConstant(byte* addr) const {
  return NameOfAddress(addr);
}


const char* NameConverter::NameOfCPURegister(int reg) const {
  return v8::internal::Registers::Name(reg);
}


const char* NameConverter::NameOfXMMRegister(int reg) const {
  return v8::internal::FPURegisters::Name(reg);
}


const char* NameConverter::NameOfByteCPURegister(int reg) const {
  UNREACHABLE();  // MIPS does not have the concept of a byte register.
  return "nobytereg";
}


const char* NameConverter::NameInCode(byte* addr) const {
  // The default name converter is called for unknown code. So we will not try
  // to access any memory.
  return "";
}


//------------------------------------------------------------------------------

Disassembler::Disassembler(const NameConverter& converter)
    : converter_(converter) {}


Disassembler::~Disassembler() {}


int Disassembler::InstructionDecode(v8::internal::Vector<char> buffer,
                                    byte* instruction) {
  v8::internal::Decoder d(converter_, buffer);
  return d.InstructionDecode(instruction);
}


// The MIPS assembler does not currently use constant pools.
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
    fprintf(f, "%p    %08x      %s\n",
            prev_pc, *reinterpret_cast<int32_t*>(prev_pc), buffer.start());
  }
}


#undef UNSUPPORTED

}  // namespace disasm

#endif  // V8_TARGET_ARCH_MIPS

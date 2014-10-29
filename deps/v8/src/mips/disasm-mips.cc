// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS

#include "src/base/platform/platform.h"
#include "src/disasm.h"
#include "src/macro-assembler.h"
#include "src/mips/constants-mips.h"

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
  void PrintXImm21(Instruction* instr);
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
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", sa);
}


// Print the integer value of the rd field, when it is not used as reg.
void Decoder::PrintSd(Instruction* instr) {
  int sd = instr->RdValue();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", sd);
}


// Print the integer value of the rd field, when used as 'ext' size.
void Decoder::PrintSs1(Instruction* instr) {
  int ss = instr->RdValue();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", ss + 1);
}


// Print the integer value of the rd field, when used as 'ins' size.
void Decoder::PrintSs2(Instruction* instr) {
  int ss = instr->RdValue();
  int pos = instr->SaValue();
  out_buffer_pos_ +=
      SNPrintF(out_buffer_ + out_buffer_pos_, "%d", ss - pos + 1);
}


// Print the integer value of the cc field for the bc1t/f instructions.
void Decoder::PrintBc(Instruction* instr) {
  int cc = instr->FBccValue();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", cc);
}


// Print the integer value of the cc field for the FP compare instructions.
void Decoder::PrintCc(Instruction* instr) {
  int cc = instr->FCccValue();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "cc(%d)", cc);
}


// Print 16-bit unsigned immediate value.
void Decoder::PrintUImm16(Instruction* instr) {
  int32_t imm = instr->Imm16Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%u", imm);
}


// Print 16-bit signed immediate value.
void Decoder::PrintSImm16(Instruction* instr) {
  int32_t imm = ((instr->Imm16Value()) << 16) >> 16;
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm);
}


// Print 16-bit hexa immediate value.
void Decoder::PrintXImm16(Instruction* instr) {
  int32_t imm = instr->Imm16Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 21-bit immediate value.
void Decoder::PrintXImm21(Instruction* instr) {
  uint32_t imm = instr->Imm21Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 26-bit immediate value.
void Decoder::PrintXImm26(Instruction* instr) {
  uint32_t imm = instr->Imm26Value() << kImmFieldShift;
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 26-bit immediate value.
void Decoder::PrintCode(Instruction* instr) {
  if (instr->OpcodeFieldRaw() != SPECIAL)
    return;  // Not a break or trap instruction.
  switch (instr->FunctionFieldRaw()) {
    case BREAK: {
      int32_t code = instr->Bits(25, 6);
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
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
          SNPrintF(out_buffer_ + out_buffer_pos_, "0x%03x", code);
      break;
    }
    default:  // Not a break or trap instruction.
    break;
  }
}


// Printing of instruction name.
void Decoder::PrintInstructionName(Instruction* instr) {
}


// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instruction* instr, const char* format) {
  DCHECK(format[0] == 'r');
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
  DCHECK(format[0] == 'f');
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
  } else if (format[1] == 'r') {  // 'fr: fr register.
    int reg = instr->FrValue();
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
      DCHECK(STRING_STARTS_WITH(format, "code"));
      PrintCode(instr);
      return 4;
    }
    case 'i': {   // 'imm16u or 'imm26.
      if (format[3] == '1') {
        DCHECK(STRING_STARTS_WITH(format, "imm16"));
        if (format[5] == 's') {
          DCHECK(STRING_STARTS_WITH(format, "imm16s"));
          PrintSImm16(instr);
        } else if (format[5] == 'u') {
          DCHECK(STRING_STARTS_WITH(format, "imm16u"));
          PrintSImm16(instr);
        } else {
          DCHECK(STRING_STARTS_WITH(format, "imm16x"));
          PrintXImm16(instr);
        }
        return 6;
      } else if (format[3] == '2' && format[4] == '1') {
        DCHECK(STRING_STARTS_WITH(format, "imm21x"));
        PrintXImm21(instr);
        return 6;
      } else if (format[3] == '2' && format[4] == '6') {
        DCHECK(STRING_STARTS_WITH(format, "imm26x"));
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
          DCHECK(STRING_STARTS_WITH(format, "sa"));
          PrintSa(instr);
          return 2;
        }
        case 'd': {
          DCHECK(STRING_STARTS_WITH(format, "sd"));
          PrintSd(instr);
          return 2;
        }
        case 's': {
          if (format[2] == '1') {
              DCHECK(STRING_STARTS_WITH(format, "ss1"));  /* ext size */
              PrintSs1(instr);
              return 3;
          } else {
              DCHECK(STRING_STARTS_WITH(format, "ss2"));  /* ins size */
              PrintSs2(instr);
              return 3;
          }
        }
      }
    }
    case 'b': {   // 'bc - Special for bc1 cc field.
      DCHECK(STRING_STARTS_WITH(format, "bc"));
      PrintBc(instr);
      return 2;
    }
    case 'C': {   // 'Cc - Special for c.xx.d cc field.
      DCHECK(STRING_STARTS_WITH(format, "Cc"));
      PrintCc(instr);
      return 2;
    }
  }
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
            case CVT_L_D:
              Format(instr, "cvt.l.d 'fd, 'fs");
              break;
            case TRUNC_W_D:
              Format(instr, "trunc.w.d 'fd, 'fs");
              break;
            case TRUNC_L_D:
              Format(instr, "trunc.l.d 'fd, 'fs");
              break;
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
            case CVT_D_L:
              Format(instr, "cvt.d.l 'fd, 'fs");
              break;
            case CVT_S_L:
              Format(instr, "cvt.s.l 'fd, 'fs");
              break;
            case CMP_UN:
              Format(instr, "cmp.un.d  'fd,  'fs, 'ft");
              break;
            case CMP_EQ:
              Format(instr, "cmp.eq.d  'fd,  'fs, 'ft");
              break;
            case CMP_UEQ:
              Format(instr, "cmp.ueq.d  'fd,  'fs, 'ft");
              break;
            case CMP_LT:
              Format(instr, "cmp.lt.d  'fd,  'fs, 'ft");
              break;
            case CMP_ULT:
              Format(instr, "cmp.ult.d  'fd,  'fs, 'ft");
              break;
            case CMP_LE:
              Format(instr, "cmp.le.d  'fd,  'fs, 'ft");
              break;
            case CMP_ULE:
              Format(instr, "cmp.ule.d  'fd,  'fs, 'ft");
              break;
            case CMP_OR:
              Format(instr, "cmp.or.d  'fd,  'fs, 'ft");
              break;
            case CMP_UNE:
              Format(instr, "cmp.une.d  'fd,  'fs, 'ft");
              break;
            case CMP_NE:
              Format(instr, "cmp.ne.d  'fd,  'fs, 'ft");
              break;
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
    case COP1X:
      switch (instr->FunctionFieldRaw()) {
        case MADD_D:
          Format(instr, "madd.d  'fd, 'fr, 'fs, 'ft");
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
            if (IsMipsArchVariant(kMips32r2)) {
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
            if (IsMipsArchVariant(kMips32r2)) {
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
          if (instr->Bits(25, 16) == 0) {
            Format(instr, "mfhi    'rd");
          } else {
            if ((instr->FunctionFieldRaw() == CLZ_R6)
                && (instr->FdValue() == 1)) {
              Format(instr, "clz     'rd, 'rs");
            } else if ((instr->FunctionFieldRaw() == CLO_R6)
                && (instr->FdValue() == 1)) {
              Format(instr, "clo     'rd, 'rs");
            }
          }
          break;
        case MFLO:
          Format(instr, "mflo    'rd");
          break;
        case MULT:  // @Mips32r6 == MUL_MUH.
          if (!IsMipsArchVariant(kMips32r6)) {
            Format(instr, "mult    'rs, 'rt");
          } else {
            if (instr->SaValue() == MUL_OP) {
              Format(instr, "mul    'rd, 'rs, 'rt");
            } else {
              Format(instr, "muh    'rd, 'rs, 'rt");
            }
          }
          break;
        case MULTU:  // @Mips32r6 == MUL_MUH_U.
          if (!IsMipsArchVariant(kMips32r6)) {
            Format(instr, "multu   'rs, 'rt");
          } else {
            if (instr->SaValue() == MUL_OP) {
              Format(instr, "mulu   'rd, 'rs, 'rt");
            } else {
              Format(instr, "muhu   'rd, 'rs, 'rt");
            }
          }
          break;
        case DIV:  // @Mips32r6 == DIV_MOD.
          if (!IsMipsArchVariant(kMips32r6)) {
            Format(instr, "div     'rs, 'rt");
          } else {
            if (instr->SaValue() == DIV_OP) {
              Format(instr, "div    'rd, 'rs, 'rt");
            } else {
              Format(instr, "mod    'rd, 'rs, 'rt");
            }
          }
          break;
        case DIVU:  // @Mips32r6 == DIV_MOD_U.
          if (!IsMipsArchVariant(kMips32r6)) {
            Format(instr, "divu    'rs, 'rt");
          } else {
            if (instr->SaValue() == DIV_OP) {
              Format(instr, "divu   'rd, 'rs, 'rt");
            } else {
              Format(instr, "modu   'rd, 'rs, 'rt");
            }
          }
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
        case SELEQZ_S:
          Format(instr, "seleqz    'rd, 'rs, 'rt");
          break;
        case SELNEZ_S:
          Format(instr, "selnez    'rd, 'rs, 'rt");
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
          if (!IsMipsArchVariant(kMips32r6)) {
            Format(instr, "clz     'rd, 'rs");
          }
          break;
        default:
          UNREACHABLE();
      }
      break;
    case SPECIAL3:
      switch (instr->FunctionFieldRaw()) {
        case INS: {
          if (IsMipsArchVariant(kMips32r2)) {
            Format(instr, "ins     'rt, 'rs, 'sa, 'ss2");
          } else {
            Unknown(instr);
          }
          break;
        }
        case EXT: {
          if (IsMipsArchVariant(kMips32r2)) {
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
    case COP1:
      switch (instr->RsFieldRaw()) {
        case BC1:
          if (instr->FBtrueValue()) {
            Format(instr, "bc1t    'bc, 'imm16u");
          } else {
            Format(instr, "bc1f    'bc, 'imm16u");
          }
          break;
        case BC1EQZ:
          Format(instr, "bc1eqz    'ft, 'imm16u");
          break;
        case BC1NEZ:
          Format(instr, "bc1nez    'ft, 'imm16u");
          break;
        case W:  // CMP.S instruction.
          switch (instr->FunctionValue()) {
            case CMP_AF:
              Format(instr, "cmp.af.S    'ft, 'fs, 'fd");
              break;
            case CMP_UN:
              Format(instr, "cmp.un.S    'ft, 'fs, 'fd");
              break;
            case CMP_EQ:
              Format(instr, "cmp.eq.S    'ft, 'fs, 'fd");
              break;
            case CMP_UEQ:
              Format(instr, "cmp.ueq.S   'ft, 'fs, 'fd");
              break;
            case CMP_LT:
              Format(instr, "cmp.lt.S    'ft, 'fs, 'fd");
              break;
            case CMP_ULT:
              Format(instr, "cmp.ult.S   'ft, 'fs, 'fd");
              break;
            case CMP_LE:
              Format(instr, "cmp.le.S    'ft, 'fs, 'fd");
              break;
            case CMP_ULE:
              Format(instr, "cmp.ule.S   'ft, 'fs, 'fd");
              break;
            case CMP_OR:
              Format(instr, "cmp.or.S    'ft, 'fs, 'fd");
              break;
            case CMP_UNE:
              Format(instr, "cmp.une.S   'ft, 'fs, 'fd");
              break;
            case CMP_NE:
              Format(instr, "cmp.ne.S    'ft, 'fs, 'fd");
              break;
            default:
              UNREACHABLE();
          }
          break;
        case L:  // CMP.D instruction.
          switch (instr->FunctionValue()) {
            case CMP_AF:
              Format(instr, "cmp.af.D    'ft, 'fs, 'fd");
              break;
            case CMP_UN:
              Format(instr, "cmp.un.D    'ft, 'fs, 'fd");
              break;
            case CMP_EQ:
              Format(instr, "cmp.eq.D    'ft, 'fs, 'fd");
              break;
            case CMP_UEQ:
              Format(instr, "cmp.ueq.D   'ft, 'fs, 'fd");
              break;
            case CMP_LT:
              Format(instr, "cmp.lt.D    'ft, 'fs, 'fd");
              break;
            case CMP_ULT:
              Format(instr, "cmp.ult.D   'ft, 'fs, 'fd");
              break;
            case CMP_LE:
              Format(instr, "cmp.le.D    'ft, 'fs, 'fd");
              break;
            case CMP_ULE:
              Format(instr, "cmp.ule.D   'ft, 'fs, 'fd");
              break;
            case CMP_OR:
              Format(instr, "cmp.or.D    'ft, 'fs, 'fd");
              break;
            case CMP_UNE:
              Format(instr, "cmp.une.D   'ft, 'fs, 'fd");
              break;
            case CMP_NE:
              Format(instr, "cmp.ne.D    'ft, 'fs, 'fd");
              break;
            default:
              UNREACHABLE();
          }
          break;
        case S:
          switch (instr->FunctionValue()) {
            case SEL:
              Format(instr, "sel.S    'ft, 'fs, 'fd");
              break;
            case SELEQZ_C:
              Format(instr, "seleqz.S 'ft, 'fs, 'fd");
              break;
            case SELNEZ_C:
              Format(instr, "selnez.S 'ft, 'fs, 'fd");
              break;
            case MIN:
              Format(instr, "min.S    'ft, 'fs, 'fd");
              break;
            case MINA:
              Format(instr, "mina.S   'ft, 'fs, 'fd");
              break;
            case MAX:
              Format(instr, "max.S    'ft, 'fs, 'fd");
              break;
            case MAXA:
              Format(instr, "maxa.S   'ft, 'fs, 'fd");
              break;
            default:
              UNREACHABLE();
          }
          break;
        case D:
          switch (instr->FunctionValue()) {
            case SEL:
              Format(instr, "sel.D    'ft, 'fs, 'fd");
              break;
            case SELEQZ_C:
              Format(instr, "seleqz.D 'ft, 'fs, 'fd");
              break;
            case SELNEZ_C:
              Format(instr, "selnez.D 'ft, 'fs, 'fd");
              break;
            case MIN:
              Format(instr, "min.D    'ft, 'fs, 'fd");
              break;
            case MINA:
              Format(instr, "mina.D   'ft, 'fs, 'fd");
              break;
            case MAX:
              Format(instr, "max.D    'ft, 'fs, 'fd");
              break;
            case MAXA:
              Format(instr, "maxa.D   'ft, 'fs, 'fd");
              break;
            default:
              UNREACHABLE();
          }
          break;
        default:
          UNREACHABLE();
      }

      break;  // Case COP1.
    // ------------- REGIMM class.
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
        case BGEZALL:
          Format(instr, "bgezall 'rs, 'imm16u");
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
      if ((instr->RtFieldRaw() == 0)
          && (instr->RsFieldRaw() != 0)) {
        Format(instr, "blez    'rs, 'imm16u");
      } else if ((instr->RtFieldRaw() != instr->RsFieldRaw())
          && (instr->RsFieldRaw() != 0) && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bgeuc    'rs, 'rt, 'imm16u");
      } else if ((instr->RtFieldRaw() == instr->RsFieldRaw())
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bgezalc  'rs, 'imm16u");
      } else if ((instr->RsFieldRaw() == 0)
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "blezalc  'rs, 'imm16u");
      } else {
        UNREACHABLE();
      }
      break;
    case BGTZ:
      if ((instr->RtFieldRaw() == 0)
          && (instr->RsFieldRaw() != 0)) {
        Format(instr, "bgtz    'rs, 'imm16u");
      } else if ((instr->RtFieldRaw() != instr->RsFieldRaw())
          && (instr->RsFieldRaw() != 0) && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bltuc   'rs, 'rt, 'imm16u");
      } else if ((instr->RtFieldRaw() == instr->RsFieldRaw())
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bltzalc 'rt, 'imm16u");
      } else if ((instr->RsFieldRaw() == 0)
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bgtzalc 'rt, 'imm16u");
      } else {
        UNREACHABLE();
      }
      break;
    case BLEZL:
      if ((instr->RtFieldRaw() == instr->RsFieldRaw())
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bgezc    'rt, 'imm16u");
      } else if ((instr->RtFieldRaw() != instr->RsFieldRaw())
          && (instr->RsFieldRaw() != 0) && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bgec     'rs, 'rt, 'imm16u");
      } else if ((instr->RsFieldRaw() == 0)
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "blezc    'rt, 'imm16u");
      } else {
        UNREACHABLE();
      }
      break;
    case BGTZL:
      if ((instr->RtFieldRaw() == instr->RsFieldRaw())
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bltzc    'rt, 'imm16u");
      } else if ((instr->RtFieldRaw() != instr->RsFieldRaw())
          && (instr->RsFieldRaw() != 0) && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bltc     'rs, 'rt, 'imm16u");
      } else if ((instr->RsFieldRaw() == 0)
          && (instr->RtFieldRaw() != 0)) {
        Format(instr, "bgtzc    'rt, 'imm16u");
      } else {
        UNREACHABLE();
      }
      break;
    case BEQZC:
      if (instr->RsFieldRaw() != 0) {
        Format(instr, "beqzc   'rs, 'imm21x");
      }
      break;
    case BNEZC:
      if (instr->RsFieldRaw() != 0) {
        Format(instr, "bnezc   'rs, 'imm21x");
      }
      break;
    // ------------- Arithmetic instructions.
    case ADDI:
      if (!IsMipsArchVariant(kMips32r6)) {
        Format(instr, "addi    'rt, 'rs, 'imm16s");
      } else {
        // Check if BOVC or BEQC instruction.
        if (instr->RsFieldRaw() >= instr->RtFieldRaw()) {
          Format(instr, "bovc  'rs, 'rt, 'imm16s");
        } else if (instr->RsFieldRaw() < instr->RtFieldRaw()) {
          Format(instr, "beqc  'rs, 'rt, 'imm16s");
        } else {
          UNREACHABLE();
        }
      }
      break;
    case DADDI:
      if (IsMipsArchVariant(kMips32r6)) {
        // Check if BNVC or BNEC instruction.
        if (instr->RsFieldRaw() >= instr->RtFieldRaw()) {
          Format(instr, "bnvc  'rs, 'rt, 'imm16s");
        } else if (instr->RsFieldRaw() < instr->RtFieldRaw()) {
          Format(instr, "bnec  'rs, 'rt, 'imm16s");
        } else {
          UNREACHABLE();
        }
      }
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
      if (!IsMipsArchVariant(kMips32r6)) {
        Format(instr, "lui     'rt, 'imm16x");
      } else {
        if (instr->RsValue() != 0) {
          Format(instr, "aui     'rt, 'imm16x");
        } else {
          Format(instr, "lui     'rt, 'imm16x");
        }
      }
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
    case PREF:
      Format(instr, "pref    'rt, 'imm16s('rs)");
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
      printf("a 0x%x \n", instr->OpcodeFieldRaw());
      UNREACHABLE();
      break;
  }
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
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
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
  v8::internal::SNPrintF(tmp_buffer_, "%p", addr);
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
    v8::internal::PrintF(f, "%p    %08x      %s\n",
        prev_pc, *reinterpret_cast<int32_t*>(prev_pc), buffer.start());
  }
}


#undef UNSUPPORTED

}  // namespace disasm

#endif  // V8_TARGET_ARCH_MIPS

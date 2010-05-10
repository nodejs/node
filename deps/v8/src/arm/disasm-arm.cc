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

#include "constants-arm.h"
#include "disasm.h"
#include "macro-assembler.h"
#include "platform.h"


namespace assembler {
namespace arm {

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
  int InstructionDecode(byte* instruction);

 private:
  // Bottleneck functions to print into the out_buffer.
  void PrintChar(const char ch);
  void Print(const char* str);

  // Printing of common values.
  void PrintRegister(int reg);
  void PrintSRegister(int reg);
  void PrintDRegister(int reg);
  int FormatVFPRegister(Instr* instr, const char* format);
  int FormatVFPinstruction(Instr* instr, const char* format);
  void PrintCondition(Instr* instr);
  void PrintShiftRm(Instr* instr);
  void PrintShiftImm(Instr* instr);
  void PrintPU(Instr* instr);
  void PrintSoftwareInterrupt(SoftwareInterruptCodes swi);

  // Handle formatting of instructions and their options.
  int FormatRegister(Instr* instr, const char* option);
  int FormatOption(Instr* instr, const char* option);
  void Format(Instr* instr, const char* format);
  void Unknown(Instr* instr);

  // Each of these functions decodes one particular instruction type, a 3-bit
  // field in the instruction encoding.
  // Types 0 and 1 are combined as they are largely the same except for the way
  // they interpret the shifter operand.
  void DecodeType01(Instr* instr);
  void DecodeType2(Instr* instr);
  void DecodeType3(Instr* instr);
  void DecodeType4(Instr* instr);
  void DecodeType5(Instr* instr);
  void DecodeType6(Instr* instr);
  void DecodeType7(Instr* instr);
  void DecodeUnconditional(Instr* instr);
  // For VFP support.
  void DecodeTypeVFP(Instr* instr);
  void DecodeType6CoprocessorIns(Instr* instr);

  void DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(Instr* instr);
  void DecodeVCMP(Instr* instr);
  void DecodeVCVTBetweenDoubleAndSingle(Instr* instr);
  void DecodeVCVTBetweenFloatingPointAndInteger(Instr* instr);

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


// These condition names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
static const char* cond_names[max_condition] = {
  "eq", "ne", "cs" , "cc" , "mi" , "pl" , "vs" , "vc" ,
  "hi", "ls", "ge", "lt", "gt", "le", "", "invalid",
};


// Print the condition guarding the instruction.
void Decoder::PrintCondition(Instr* instr) {
  Print(cond_names[instr->ConditionField()]);
}


// Print the register name according to the active name converter.
void Decoder::PrintRegister(int reg) {
  Print(converter_.NameOfCPURegister(reg));
}

// Print the VFP S register name according to the active name converter.
void Decoder::PrintSRegister(int reg) {
  Print(assembler::arm::VFPRegisters::Name(reg, false));
}

// Print the  VFP D register name according to the active name converter.
void Decoder::PrintDRegister(int reg) {
  Print(assembler::arm::VFPRegisters::Name(reg, true));
}


// These shift names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
static const char* shift_names[max_shift] = {
  "lsl", "lsr", "asr", "ror"
};


// Print the register shift operands for the instruction. Generally used for
// data processing instructions.
void Decoder::PrintShiftRm(Instr* instr) {
  Shift shift = instr->ShiftField();
  int shift_amount = instr->ShiftAmountField();
  int rm = instr->RmField();

  PrintRegister(rm);

  if ((instr->RegShiftField() == 0) && (shift == LSL) && (shift_amount == 0)) {
    // Special case for using rm only.
    return;
  }
  if (instr->RegShiftField() == 0) {
    // by immediate
    if ((shift == ROR) && (shift_amount == 0)) {
      Print(", RRX");
      return;
    } else if (((shift == LSR) || (shift == ASR)) && (shift_amount == 0)) {
      shift_amount = 32;
    }
    out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                         ", %s #%d",
                                         shift_names[shift], shift_amount);
  } else {
    // by register
    int rs = instr->RsField();
    out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                         ", %s ", shift_names[shift]);
    PrintRegister(rs);
  }
}


// Print the immediate operand for the instruction. Generally used for data
// processing instructions.
void Decoder::PrintShiftImm(Instr* instr) {
  int rotate = instr->RotateField() * 2;
  int immed8 = instr->Immed8Field();
  int imm = (immed8 >> rotate) | (immed8 << (32 - rotate));
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "#%d", imm);
}


// Print PU formatting to reduce complexity of FormatOption.
void Decoder::PrintPU(Instr* instr) {
  switch (instr->PUField()) {
    case 0: {
      Print("da");
      break;
    }
    case 1: {
      Print("ia");
      break;
    }
    case 2: {
      Print("db");
      break;
    }
    case 3: {
      Print("ib");
      break;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
}


// Print SoftwareInterrupt codes. Factoring this out reduces the complexity of
// the FormatOption method.
void Decoder::PrintSoftwareInterrupt(SoftwareInterruptCodes swi) {
  switch (swi) {
    case call_rt_redirected:
      Print("call_rt_redirected");
      return;
    case break_point:
      Print("break_point");
      return;
    default:
      out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                           "%d",
                                           swi);
      return;
  }
}


// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instr* instr, const char* format) {
  ASSERT(format[0] == 'r');
  if (format[1] == 'n') {  // 'rn: Rn register
    int reg = instr->RnField();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'rd: Rd register
    int reg = instr->RdField();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 's') {  // 'rs: Rs register
    int reg = instr->RsField();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'm') {  // 'rm: Rm register
    int reg = instr->RmField();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 't') {  // 'rt: Rt register
    int reg = instr->RtField();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'l') {
    // 'rlist: register list for load and store multiple instructions
    ASSERT(STRING_STARTS_WITH(format, "rlist"));
    int rlist = instr->RlistField();
    int reg = 0;
    Print("{");
    // Print register list in ascending order, by scanning the bit mask.
    while (rlist != 0) {
      if ((rlist & 1) != 0) {
        PrintRegister(reg);
        if ((rlist >> 1) != 0) {
          Print(", ");
        }
      }
      reg++;
      rlist >>= 1;
    }
    Print("}");
    return 5;
  }
  UNREACHABLE();
  return -1;
}


// Handle all VFP register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatVFPRegister(Instr* instr, const char* format) {
  ASSERT((format[0] == 'S') || (format[0] == 'D'));

  if (format[1] == 'n') {
    int reg = instr->VnField();
    if (format[0] == 'S') PrintSRegister(((reg << 1) | instr->NField()));
    if (format[0] == 'D') PrintDRegister(reg);
    return 2;
  } else if (format[1] == 'm') {
    int reg = instr->VmField();
    if (format[0] == 'S') PrintSRegister(((reg << 1) | instr->MField()));
    if (format[0] == 'D') PrintDRegister(reg);
    return 2;
  } else if (format[1] == 'd') {
    int reg = instr->VdField();
    if (format[0] == 'S') PrintSRegister(((reg << 1) | instr->DField()));
    if (format[0] == 'D') PrintDRegister(reg);
    return 2;
  }

  UNREACHABLE();
  return -1;
}


int Decoder::FormatVFPinstruction(Instr* instr, const char* format) {
    Print(format);
    return 0;
}


// FormatOption takes a formatting string and interprets it based on
// the current instructions. The format string points to the first
// character of the option string (the option escape has already been
// consumed by the caller.)  FormatOption returns the number of
// characters that were consumed from the formatting string.
int Decoder::FormatOption(Instr* instr, const char* format) {
  switch (format[0]) {
    case 'a': {  // 'a: accumulate multiplies
      if (instr->Bit(21) == 0) {
        Print("ul");
      } else {
        Print("la");
      }
      return 1;
    }
    case 'b': {  // 'b: byte loads or stores
      if (instr->HasB()) {
        Print("b");
      }
      return 1;
    }
    case 'c': {  // 'cond: conditional execution
      ASSERT(STRING_STARTS_WITH(format, "cond"));
      PrintCondition(instr);
      return 4;
    }
    case 'h': {  // 'h: halfword operation for extra loads and stores
      if (instr->HasH()) {
        Print("h");
      } else {
        Print("b");
      }
      return 1;
    }
    case 'l': {  // 'l: branch and link
      if (instr->HasLink()) {
        Print("l");
      }
      return 1;
    }
    case 'm': {
      if (format[1] == 'e') {  // 'memop: load/store instructions
        ASSERT(STRING_STARTS_WITH(format, "memop"));
        if (instr->HasL()) {
          Print("ldr");
        } else if ((instr->Bits(27, 25) == 0) && (instr->Bit(20) == 0)) {
          if (instr->Bits(7, 4) == 0xf) {
            Print("strd");
          } else {
            Print("ldrd");
          }
        } else {
          Print("str");
        }
        return 5;
      }
      // 'msg: for simulator break instructions
      ASSERT(STRING_STARTS_WITH(format, "msg"));
      byte* str =
          reinterpret_cast<byte*>(instr->InstructionBits() & 0x0fffffff);
      out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                           "%s", converter_.NameInCode(str));
      return 3;
    }
    case 'o': {
      if ((format[3] == '1') && (format[4] == '2')) {
        // 'off12: 12-bit offset for load and store instructions
        ASSERT(STRING_STARTS_WITH(format, "off12"));
        out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                             "%d", instr->Offset12Field());
        return 5;
      } else if ((format[3] == '1') && (format[4] == '6')) {
        ASSERT(STRING_STARTS_WITH(format, "off16to20"));
        out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                           "%d", instr->Bits(20, 16) +1);
        return 9;
      } else if (format[3] == '7') {
        ASSERT(STRING_STARTS_WITH(format, "off7to11"));
        out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                            "%d", instr->ShiftAmountField());
        return 8;
      } else if (format[3] == '0') {
        // 'off0to3and8to19 16-bit immediate encoded in bits 19-8 and 3-0.
        ASSERT(STRING_STARTS_WITH(format, "off0to3and8to19"));
        out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                            "%d",
                                            (instr->Bits(19, 8) << 4) +
                                                instr->Bits(3, 0));
        return 15;
      }
      // 'off8: 8-bit offset for extra load and store instructions
      ASSERT(STRING_STARTS_WITH(format, "off8"));
      int offs8 = (instr->ImmedHField() << 4) | instr->ImmedLField();
      out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                           "%d", offs8);
      return 4;
    }
    case 'p': {  // 'pu: P and U bits for load and store instructions
      ASSERT(STRING_STARTS_WITH(format, "pu"));
      PrintPU(instr);
      return 2;
    }
    case 'r': {
      return FormatRegister(instr, format);
    }
    case 's': {
      if (format[1] == 'h') {  // 'shift_op or 'shift_rm
        if (format[6] == 'o') {  // 'shift_op
          ASSERT(STRING_STARTS_WITH(format, "shift_op"));
          if (instr->TypeField() == 0) {
            PrintShiftRm(instr);
          } else {
            ASSERT(instr->TypeField() == 1);
            PrintShiftImm(instr);
          }
          return 8;
        } else {  // 'shift_rm
          ASSERT(STRING_STARTS_WITH(format, "shift_rm"));
          PrintShiftRm(instr);
          return 8;
        }
      } else if (format[1] == 'w') {  // 'swi
        ASSERT(STRING_STARTS_WITH(format, "swi"));
        PrintSoftwareInterrupt(instr->SwiField());
        return 3;
      } else if (format[1] == 'i') {  // 'sign: signed extra loads and stores
        ASSERT(STRING_STARTS_WITH(format, "sign"));
        if (instr->HasSign()) {
          Print("s");
        }
        return 4;
      }
      // 's: S field of data processing instructions
      if (instr->HasS()) {
        Print("s");
      }
      return 1;
    }
    case 't': {  // 'target: target of branch instructions
      ASSERT(STRING_STARTS_WITH(format, "target"));
      int off = (instr->SImmed24Field() << 2) + 8;
      out_buffer_pos_ += v8i::OS::SNPrintF(
          out_buffer_ + out_buffer_pos_,
          "%+d -> %s",
          off,
          converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + off));
      return 6;
    }
    case 'u': {  // 'u: signed or unsigned multiplies
      // The manual gets the meaning of bit 22 backwards in the multiply
      // instruction overview on page A3.16.2.  The instructions that
      // exist in u and s variants are the following:
      // smull A4.1.87
      // umull A4.1.129
      // umlal A4.1.128
      // smlal A4.1.76
      // For these 0 means u and 1 means s.  As can be seen on their individual
      // pages.  The other 18 mul instructions have the bit set or unset in
      // arbitrary ways that are unrelated to the signedness of the instruction.
      // None of these 18 instructions exist in both a 'u' and an 's' variant.

      if (instr->Bit(22) == 0) {
        Print("u");
      } else {
        Print("s");
      }
      return 1;
    }
    case 'v': {
      return FormatVFPinstruction(instr, format);
    }
    case 'S':
    case 'D': {
      return FormatVFPRegister(instr, format);
    }
    case 'w': {  // 'w: W field of load and store instructions
      if (instr->HasW()) {
        Print("!");
      }
      return 1;
    }
    default: {
      UNREACHABLE();
      break;
    }
  }
  UNREACHABLE();
  return -1;
}


// Format takes a formatting string for a whole instruction and prints it into
// the output buffer. All escaped options are handed to FormatOption to be
// parsed further.
void Decoder::Format(Instr* instr, const char* format) {
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
void Decoder::Unknown(Instr* instr) {
  Format(instr, "unknown");
}


void Decoder::DecodeType01(Instr* instr) {
  int type = instr->TypeField();
  if ((type == 0) && instr->IsSpecialType0()) {
    // multiply instruction or extra loads and stores
    if (instr->Bits(7, 4) == 9) {
      if (instr->Bit(24) == 0) {
        // multiply instructions
        if (instr->Bit(23) == 0) {
          if (instr->Bit(21) == 0) {
            // The MUL instruction description (A 4.1.33) refers to Rd as being
            // the destination for the operation, but it confusingly uses the
            // Rn field to encode it.
            Format(instr, "mul'cond's 'rn, 'rm, 'rs");
          } else {
            // The MLA instruction description (A 4.1.28) refers to the order
            // of registers as "Rd, Rm, Rs, Rn". But confusingly it uses the
            // Rn field to encode the Rd register and the Rd field to encode
            // the Rn register.
            Format(instr, "mla'cond's 'rn, 'rm, 'rs, 'rd");
          }
        } else {
          // The signed/long multiply instructions use the terms RdHi and RdLo
          // when referring to the target registers. They are mapped to the Rn
          // and Rd fields as follows:
          // RdLo == Rd field
          // RdHi == Rn field
          // The order of registers is: <RdLo>, <RdHi>, <Rm>, <Rs>
          Format(instr, "'um'al'cond's 'rd, 'rn, 'rm, 'rs");
        }
      } else {
        Unknown(instr);  // not used by V8
      }
    } else if ((instr->Bit(20) == 0) && ((instr->Bits(7, 4) & 0xd) == 0xd)) {
      // ldrd, strd
      switch (instr->PUField()) {
        case 0: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond's 'rd, ['rn], -'rm");
          } else {
            Format(instr, "'memop'cond's 'rd, ['rn], #-'off8");
          }
          break;
        }
        case 1: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond's 'rd, ['rn], +'rm");
          } else {
            Format(instr, "'memop'cond's 'rd, ['rn], #+'off8");
          }
          break;
        }
        case 2: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond's 'rd, ['rn, -'rm]'w");
          } else {
            Format(instr, "'memop'cond's 'rd, ['rn, #-'off8]'w");
          }
          break;
        }
        case 3: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond's 'rd, ['rn, +'rm]'w");
          } else {
            Format(instr, "'memop'cond's 'rd, ['rn, #+'off8]'w");
          }
          break;
        }
        default: {
          // The PU field is a 2-bit field.
          UNREACHABLE();
          break;
        }
      }
    } else {
      // extra load/store instructions
      switch (instr->PUField()) {
        case 0: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], -'rm");
          } else {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], #-'off8");
          }
          break;
        }
        case 1: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], +'rm");
          } else {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], #+'off8");
          }
          break;
        }
        case 2: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn, -'rm]'w");
          } else {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn, #-'off8]'w");
          }
          break;
        }
        case 3: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn, +'rm]'w");
          } else {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn, #+'off8]'w");
          }
          break;
        }
        default: {
          // The PU field is a 2-bit field.
          UNREACHABLE();
          break;
        }
      }
      return;
    }
  } else if ((type == 0) && instr->IsMiscType0()) {
    if (instr->Bits(22, 21) == 1) {
      switch (instr->Bits(7, 4)) {
        case BX:
          Format(instr, "bx'cond 'rm");
          break;
        case BLX:
          Format(instr, "blx'cond 'rm");
          break;
        case BKPT:
          Format(instr, "bkpt 'off0to3and8to19");
          break;
        default:
          Unknown(instr);  // not used by V8
          break;
      }
    } else if (instr->Bits(22, 21) == 3) {
      switch (instr->Bits(7, 4)) {
        case CLZ:
          Format(instr, "clz'cond 'rd, 'rm");
          break;
        default:
          Unknown(instr);  // not used by V8
          break;
      }
    } else {
      Unknown(instr);  // not used by V8
    }
  } else {
    switch (instr->OpcodeField()) {
      case AND: {
        Format(instr, "and'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case EOR: {
        Format(instr, "eor'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case SUB: {
        Format(instr, "sub'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case RSB: {
        Format(instr, "rsb'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case ADD: {
        Format(instr, "add'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case ADC: {
        Format(instr, "adc'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case SBC: {
        Format(instr, "sbc'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case RSC: {
        Format(instr, "rsc'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case TST: {
        if (instr->HasS()) {
          Format(instr, "tst'cond 'rn, 'shift_op");
        } else {
          Unknown(instr);  // not used by V8
        }
        break;
      }
      case TEQ: {
        if (instr->HasS()) {
          Format(instr, "teq'cond 'rn, 'shift_op");
        } else {
          // Other instructions matching this pattern are handled in the
          // miscellaneous instructions part above.
          UNREACHABLE();
        }
        break;
      }
      case CMP: {
        if (instr->HasS()) {
          Format(instr, "cmp'cond 'rn, 'shift_op");
        } else {
          Unknown(instr);  // not used by V8
        }
        break;
      }
      case CMN: {
        if (instr->HasS()) {
          Format(instr, "cmn'cond 'rn, 'shift_op");
        } else {
          // Other instructions matching this pattern are handled in the
          // miscellaneous instructions part above.
          UNREACHABLE();
        }
        break;
      }
      case ORR: {
        Format(instr, "orr'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case MOV: {
        Format(instr, "mov'cond's 'rd, 'shift_op");
        break;
      }
      case BIC: {
        Format(instr, "bic'cond's 'rd, 'rn, 'shift_op");
        break;
      }
      case MVN: {
        Format(instr, "mvn'cond's 'rd, 'shift_op");
        break;
      }
      default: {
        // The Opcode field is a 4-bit field.
        UNREACHABLE();
        break;
      }
    }
  }
}


void Decoder::DecodeType2(Instr* instr) {
  switch (instr->PUField()) {
    case 0: {
      if (instr->HasW()) {
        Unknown(instr);  // not used in V8
      }
      Format(instr, "'memop'cond'b 'rd, ['rn], #-'off12");
      break;
    }
    case 1: {
      if (instr->HasW()) {
        Unknown(instr);  // not used in V8
      }
      Format(instr, "'memop'cond'b 'rd, ['rn], #+'off12");
      break;
    }
    case 2: {
      Format(instr, "'memop'cond'b 'rd, ['rn, #-'off12]'w");
      break;
    }
    case 3: {
      Format(instr, "'memop'cond'b 'rd, ['rn, #+'off12]'w");
      break;
    }
    default: {
      // The PU field is a 2-bit field.
      UNREACHABLE();
      break;
    }
  }
}


void Decoder::DecodeType3(Instr* instr) {
  switch (instr->PUField()) {
    case 0: {
      ASSERT(!instr->HasW());
      Format(instr, "'memop'cond'b 'rd, ['rn], -'shift_rm");
      break;
    }
    case 1: {
      ASSERT(!instr->HasW());
      Format(instr, "'memop'cond'b 'rd, ['rn], +'shift_rm");
      break;
    }
    case 2: {
      Format(instr, "'memop'cond'b 'rd, ['rn, -'shift_rm]'w");
      break;
    }
    case 3: {
      if (instr->HasW() && (instr->Bits(6, 4) == 0x5)) {
        uint32_t widthminus1 = static_cast<uint32_t>(instr->Bits(20, 16));
        uint32_t lsbit = static_cast<uint32_t>(instr->ShiftAmountField());
        uint32_t msbit = widthminus1 + lsbit;
        if (msbit <= 31) {
          Format(instr, "ubfx'cond 'rd, 'rm, #'off7to11, #'off16to20");
        } else {
          UNREACHABLE();
        }
      } else {
        Format(instr, "'memop'cond'b 'rd, ['rn, +'shift_rm]'w");
      }
      break;
    }
    default: {
      // The PU field is a 2-bit field.
      UNREACHABLE();
      break;
    }
  }
}


void Decoder::DecodeType4(Instr* instr) {
  ASSERT(instr->Bit(22) == 0);  // Privileged mode currently not supported.
  if (instr->HasL()) {
    Format(instr, "ldm'cond'pu 'rn'w, 'rlist");
  } else {
    Format(instr, "stm'cond'pu 'rn'w, 'rlist");
  }
}


void Decoder::DecodeType5(Instr* instr) {
  Format(instr, "b'l'cond 'target");
}


void Decoder::DecodeType6(Instr* instr) {
  DecodeType6CoprocessorIns(instr);
}


void Decoder::DecodeType7(Instr* instr) {
  if (instr->Bit(24) == 1) {
    Format(instr, "swi'cond 'swi");
  } else {
    DecodeTypeVFP(instr);
  }
}

void Decoder::DecodeUnconditional(Instr* instr) {
  if (instr->Bits(7, 4) == 0xB && instr->Bits(27, 25) == 0 && instr->HasL()) {
    Format(instr, "'memop'h'pu 'rd, ");
    bool immediate = instr->HasB();
    switch (instr->PUField()) {
      case 0: {
        // Post index, negative.
        if (instr->HasW()) {
          Unknown(instr);
          break;
        }
        if (immediate) {
          Format(instr, "['rn], #-'imm12");
        } else {
          Format(instr, "['rn], -'rm");
        }
        break;
      }
      case 1: {
        // Post index, positive.
        if (instr->HasW()) {
          Unknown(instr);
          break;
        }
        if (immediate) {
          Format(instr, "['rn], #+'imm12");
        } else {
          Format(instr, "['rn], +'rm");
        }
        break;
      }
      case 2: {
        // Pre index or offset, negative.
        if (immediate) {
          Format(instr, "['rn, #-'imm12]'w");
        } else {
          Format(instr, "['rn, -'rm]'w");
        }
        break;
      }
      case 3: {
        // Pre index or offset, positive.
        if (immediate) {
          Format(instr, "['rn, #+'imm12]'w");
        } else {
          Format(instr, "['rn, +'rm]'w");
        }
        break;
      }
      default: {
        // The PU field is a 2-bit field.
        UNREACHABLE();
        break;
      }
    }
    return;
  }
  Format(instr, "break 'msg");
}


// void Decoder::DecodeTypeVFP(Instr* instr)
// vmov: Sn = Rt
// vmov: Rt = Sn
// vcvt: Dd = Sm
// vcvt: Sd = Dm
// Dd = vadd(Dn, Dm)
// Dd = vsub(Dn, Dm)
// Dd = vmul(Dn, Dm)
// Dd = vdiv(Dn, Dm)
// vcmp(Dd, Dm)
// VMRS
void Decoder::DecodeTypeVFP(Instr* instr) {
  ASSERT((instr->TypeField() == 7) && (instr->Bit(24) == 0x0) );
  ASSERT(instr->Bits(11, 9) == 0x5);

  if (instr->Bit(4) == 0) {
    if (instr->Opc1Field() == 0x7) {
      // Other data processing instructions
      if ((instr->Opc2Field() == 0x7) && (instr->Opc3Field() == 0x3)) {
        DecodeVCVTBetweenDoubleAndSingle(instr);
      } else if ((instr->Opc2Field() == 0x8) && (instr->Opc3Field() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if (((instr->Opc2Field() >> 1) == 0x6) &&
                 (instr->Opc3Field() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if (((instr->Opc2Field() == 0x4) || (instr->Opc2Field() == 0x5)) &&
                 (instr->Opc3Field() & 0x1)) {
        DecodeVCMP(instr);
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if (instr->Opc1Field() == 0x3) {
      if (instr->SzField() == 0x1) {
        if (instr->Opc3Field() & 0x1) {
          Format(instr, "vsub.f64'cond 'Dd, 'Dn, 'Dm");
        } else {
          Format(instr, "vadd.f64'cond 'Dd, 'Dn, 'Dm");
        }
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if ((instr->Opc1Field() == 0x2) && !(instr->Opc3Field() & 0x1)) {
      if (instr->SzField() == 0x1) {
        Format(instr, "vmul.f64'cond 'Dd, 'Dn, 'Dm");
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if ((instr->Opc1Field() == 0x4) && !(instr->Opc3Field() & 0x1)) {
      if (instr->SzField() == 0x1) {
        Format(instr, "vdiv.f64'cond 'Dd, 'Dn, 'Dm");
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else {
      Unknown(instr);  // Not used by V8.
    }
  } else {
    if ((instr->VCField() == 0x0) &&
        (instr->VAField() == 0x0)) {
      DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(instr);
    } else if ((instr->VLField() == 0x1) &&
               (instr->VCField() == 0x0) &&
               (instr->VAField() == 0x7) &&
               (instr->Bits(19, 16) == 0x1)) {
      if (instr->Bits(15, 12) == 0xF)
        Format(instr, "vmrs'cond APSR, FPSCR");
      else
        Unknown(instr);  // Not used by V8.
    } else {
      Unknown(instr);  // Not used by V8.
    }
  }
}


void Decoder::DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(Instr* instr) {
  ASSERT((instr->Bit(4) == 1) && (instr->VCField() == 0x0) &&
         (instr->VAField() == 0x0));

  bool to_arm_register = (instr->VLField() == 0x1);

  if (to_arm_register) {
    Format(instr, "vmov'cond 'rt, 'Sn");
  } else {
    Format(instr, "vmov'cond 'Sn, 'rt");
  }
}


void Decoder::DecodeVCMP(Instr* instr) {
  ASSERT((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
  ASSERT(((instr->Opc2Field() == 0x4) || (instr->Opc2Field() == 0x5)) &&
         (instr->Opc3Field() & 0x1));

  // Comparison.
  bool dp_operation = (instr->SzField() == 1);
  bool raise_exception_for_qnan = (instr->Bit(7) == 0x1);

  if (dp_operation && !raise_exception_for_qnan) {
    Format(instr, "vcmp.f64'cond 'Dd, 'Dm");
  } else {
    Unknown(instr);  // Not used by V8.
  }
}


void Decoder::DecodeVCVTBetweenDoubleAndSingle(Instr* instr) {
  ASSERT((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
  ASSERT((instr->Opc2Field() == 0x7) && (instr->Opc3Field() == 0x3));

  bool double_to_single = (instr->SzField() == 1);

  if (double_to_single) {
    Format(instr, "vcvt.f32.f64'cond 'Sd, 'Dm");
  } else {
    Format(instr, "vcvt.f64.f32'cond 'Dd, 'Sm");
  }
}


void Decoder::DecodeVCVTBetweenFloatingPointAndInteger(Instr* instr) {
  ASSERT((instr->Bit(4) == 0) && (instr->Opc1Field() == 0x7));
  ASSERT(((instr->Opc2Field() == 0x8) && (instr->Opc3Field() & 0x1)) ||
         (((instr->Opc2Field() >> 1) == 0x6) && (instr->Opc3Field() & 0x1)));

  bool to_integer = (instr->Bit(18) == 1);
  bool dp_operation = (instr->SzField() == 1);
  if (to_integer) {
    bool unsigned_integer = (instr->Bit(16) == 0);

    if (dp_operation) {
      if (unsigned_integer) {
        Format(instr, "vcvt.u32.f64'cond 'Sd, 'Dm");
      } else {
        Format(instr, "vcvt.s32.f64'cond 'Sd, 'Dm");
      }
    } else {
      if (unsigned_integer) {
        Format(instr, "vcvt.u32.f32'cond 'Sd, 'Sm");
      } else {
        Format(instr, "vcvt.s32.f32'cond 'Sd, 'Sm");
      }
    }
  } else {
    bool unsigned_integer = (instr->Bit(7) == 0);

    if (dp_operation) {
      if (unsigned_integer) {
        Format(instr, "vcvt.f64.u32'cond 'Dd, 'Sm");
      } else {
        Format(instr, "vcvt.f64.s32'cond 'Dd, 'Sm");
      }
    } else {
      if (unsigned_integer) {
        Format(instr, "vcvt.f32.u32'cond 'Sd, 'Sm");
      } else {
        Format(instr, "vcvt.f32.s32'cond 'Sd, 'Sm");
      }
    }
  }
}


// Decode Type 6 coprocessor instructions.
// Dm = vmov(Rt, Rt2)
// <Rt, Rt2> = vmov(Dm)
// Ddst = MEM(Rbase + 4*offset).
// MEM(Rbase + 4*offset) = Dsrc.
void Decoder::DecodeType6CoprocessorIns(Instr* instr) {
  ASSERT((instr->TypeField() == 6));

  if (instr->CoprocessorField() == 0xA) {
    switch (instr->OpcodeField()) {
      case 0x8:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Sd, ['rn - 4*'off8]");
        } else {
          Format(instr, "vstr'cond 'Sd, ['rn - 4*'off8]");
        }
        break;
      case 0xC:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Sd, ['rn + 4*'off8]");
        } else {
          Format(instr, "vstr'cond 'Sd, ['rn + 4*'off8]");
        }
        break;
      default:
        Unknown(instr);  // Not used by V8.
        break;
    }
  } else if (instr->CoprocessorField() == 0xB) {
    switch (instr->OpcodeField()) {
      case 0x2:
        // Load and store double to two GP registers
        if (instr->Bits(7, 4) != 0x1) {
          Unknown(instr);  // Not used by V8.
        } else if (instr->HasL()) {
          Format(instr, "vmov'cond 'rt, 'rn, 'Dm");
        } else {
          Format(instr, "vmov'cond 'Dm, 'rt, 'rn");
        }
        break;
      case 0x8:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Dd, ['rn - 4*'off8]");
        } else {
          Format(instr, "vstr'cond 'Dd, ['rn - 4*'off8]");
        }
        break;
      case 0xC:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Dd, ['rn + 4*'off8]");
        } else {
          Format(instr, "vstr'cond 'Dd, ['rn + 4*'off8]");
        }
        break;
      default:
        Unknown(instr);  // Not used by V8.
        break;
    }
  } else {
    UNIMPLEMENTED();  // Not used by V8.
  }
}


// Disassemble the instruction at *instr_ptr into the output buffer.
int Decoder::InstructionDecode(byte* instr_ptr) {
  Instr* instr = Instr::At(instr_ptr);
  // Print raw instruction bytes.
  out_buffer_pos_ += v8i::OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                       "%08x       ",
                                       instr->InstructionBits());
  if (instr->ConditionField() == special_condition) {
    DecodeUnconditional(instr);
    return Instr::kInstrSize;
  }
  switch (instr->TypeField()) {
    case 0:
    case 1: {
      DecodeType01(instr);
      break;
    }
    case 2: {
      DecodeType2(instr);
      break;
    }
    case 3: {
      DecodeType3(instr);
      break;
    }
    case 4: {
      DecodeType4(instr);
      break;
    }
    case 5: {
      DecodeType5(instr);
      break;
    }
    case 6: {
      DecodeType6(instr);
      break;
    }
    case 7: {
      DecodeType7(instr);
      break;
    }
    default: {
      // The type field is 3-bits in the ARM encoding.
      UNREACHABLE();
      break;
    }
  }
  return Instr::kInstrSize;
}


} }  // namespace assembler::arm



//------------------------------------------------------------------------------

namespace disasm {

namespace v8i = v8::internal;


const char* NameConverter::NameOfAddress(byte* addr) const {
  static v8::internal::EmbeddedVector<char, 32> tmp_buffer;
  v8::internal::OS::SNPrintF(tmp_buffer, "%p", addr);
  return tmp_buffer.start();
}


const char* NameConverter::NameOfConstant(byte* addr) const {
  return NameOfAddress(addr);
}


const char* NameConverter::NameOfCPURegister(int reg) const {
  return assembler::arm::Registers::Name(reg);
}


const char* NameConverter::NameOfByteCPURegister(int reg) const {
  UNREACHABLE();  // ARM does not have the concept of a byte register
  return "nobytereg";
}


const char* NameConverter::NameOfXMMRegister(int reg) const {
  UNREACHABLE();  // ARM does not have any XMM registers
  return "noxmmreg";
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
  assembler::arm::Decoder d(converter_, buffer);
  return d.InstructionDecode(instruction);
}


int Disassembler::ConstantPoolSizeAt(byte* instruction) {
  int instruction_bits = *(reinterpret_cast<int*>(instruction));
  if ((instruction_bits & 0xfff00000) == 0x03000000) {
    return instruction_bits & 0x0000ffff;
  } else {
    return -1;
  }
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


}  // namespace disasm

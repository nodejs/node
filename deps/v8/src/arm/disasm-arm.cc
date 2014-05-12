// Copyright 2011 the V8 project authors. All rights reserved.
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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "v8.h"

#if V8_TARGET_ARCH_ARM

#include "constants-arm.h"
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
          Vector<char> out_buffer)
    : converter_(converter),
      out_buffer_(out_buffer),
      out_buffer_pos_(0) {
    out_buffer_[out_buffer_pos_] = '\0';
  }

  ~Decoder() {}

  // Writes one disassembled instruction into 'buffer' (0-terminated).
  // Returns the length of the disassembled machine instruction in bytes.
  int InstructionDecode(byte* instruction);

  static bool IsConstantPoolAt(byte* instr_ptr);
  static int ConstantPoolSizeAt(byte* instr_ptr);

 private:
  // Bottleneck functions to print into the out_buffer.
  void PrintChar(const char ch);
  void Print(const char* str);

  // Printing of common values.
  void PrintRegister(int reg);
  void PrintSRegister(int reg);
  void PrintDRegister(int reg);
  int FormatVFPRegister(Instruction* instr, const char* format);
  void PrintMovwMovt(Instruction* instr);
  int FormatVFPinstruction(Instruction* instr, const char* format);
  void PrintCondition(Instruction* instr);
  void PrintShiftRm(Instruction* instr);
  void PrintShiftImm(Instruction* instr);
  void PrintShiftSat(Instruction* instr);
  void PrintPU(Instruction* instr);
  void PrintSoftwareInterrupt(SoftwareInterruptCodes svc);

  // Handle formatting of instructions and their options.
  int FormatRegister(Instruction* instr, const char* option);
  void FormatNeonList(int Vd, int type);
  void FormatNeonMemory(int Rn, int align, int Rm);
  int FormatOption(Instruction* instr, const char* option);
  void Format(Instruction* instr, const char* format);
  void Unknown(Instruction* instr);

  // Each of these functions decodes one particular instruction type, a 3-bit
  // field in the instruction encoding.
  // Types 0 and 1 are combined as they are largely the same except for the way
  // they interpret the shifter operand.
  void DecodeType01(Instruction* instr);
  void DecodeType2(Instruction* instr);
  void DecodeType3(Instruction* instr);
  void DecodeType4(Instruction* instr);
  void DecodeType5(Instruction* instr);
  void DecodeType6(Instruction* instr);
  // Type 7 includes special Debugger instructions.
  int DecodeType7(Instruction* instr);
  // For VFP support.
  void DecodeTypeVFP(Instruction* instr);
  void DecodeType6CoprocessorIns(Instruction* instr);

  void DecodeSpecialCondition(Instruction* instr);

  void DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(Instruction* instr);
  void DecodeVCMP(Instruction* instr);
  void DecodeVCVTBetweenDoubleAndSingle(Instruction* instr);
  void DecodeVCVTBetweenFloatingPointAndInteger(Instruction* instr);

  const disasm::NameConverter& converter_;
  Vector<char> out_buffer_;
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
static const char* cond_names[kNumberOfConditions] = {
  "eq", "ne", "cs" , "cc" , "mi" , "pl" , "vs" , "vc" ,
  "hi", "ls", "ge", "lt", "gt", "le", "", "invalid",
};


// Print the condition guarding the instruction.
void Decoder::PrintCondition(Instruction* instr) {
  Print(cond_names[instr->ConditionValue()]);
}


// Print the register name according to the active name converter.
void Decoder::PrintRegister(int reg) {
  Print(converter_.NameOfCPURegister(reg));
}


// Print the VFP S register name according to the active name converter.
void Decoder::PrintSRegister(int reg) {
  Print(VFPRegisters::Name(reg, false));
}


// Print the VFP D register name according to the active name converter.
void Decoder::PrintDRegister(int reg) {
  Print(VFPRegisters::Name(reg, true));
}


// These shift names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
static const char* const shift_names[kNumberOfShifts] = {
  "lsl", "lsr", "asr", "ror"
};


// Print the register shift operands for the instruction. Generally used for
// data processing instructions.
void Decoder::PrintShiftRm(Instruction* instr) {
  ShiftOp shift = instr->ShiftField();
  int shift_index = instr->ShiftValue();
  int shift_amount = instr->ShiftAmountValue();
  int rm = instr->RmValue();

  PrintRegister(rm);

  if ((instr->RegShiftValue() == 0) && (shift == LSL) && (shift_amount == 0)) {
    // Special case for using rm only.
    return;
  }
  if (instr->RegShiftValue() == 0) {
    // by immediate
    if ((shift == ROR) && (shift_amount == 0)) {
      Print(", RRX");
      return;
    } else if (((shift == LSR) || (shift == ASR)) && (shift_amount == 0)) {
      shift_amount = 32;
    }
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    ", %s #%d",
                                    shift_names[shift_index],
                                    shift_amount);
  } else {
    // by register
    int rs = instr->RsValue();
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    ", %s ", shift_names[shift_index]);
    PrintRegister(rs);
  }
}


// Print the immediate operand for the instruction. Generally used for data
// processing instructions.
void Decoder::PrintShiftImm(Instruction* instr) {
  int rotate = instr->RotateValue() * 2;
  int immed8 = instr->Immed8Value();
  int imm = (immed8 >> rotate) | (immed8 << (32 - rotate));
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                  "#%d", imm);
}


// Print the optional shift and immediate used by saturating instructions.
void Decoder::PrintShiftSat(Instruction* instr) {
  int shift = instr->Bits(11, 7);
  if (shift > 0) {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    ", %s #%d",
                                    shift_names[instr->Bit(6) * 2],
                                    instr->Bits(11, 7));
  }
}


// Print PU formatting to reduce complexity of FormatOption.
void Decoder::PrintPU(Instruction* instr) {
  switch (instr->PUField()) {
    case da_x: {
      Print("da");
      break;
    }
    case ia_x: {
      Print("ia");
      break;
    }
    case db_x: {
      Print("db");
      break;
    }
    case ib_x: {
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
void Decoder::PrintSoftwareInterrupt(SoftwareInterruptCodes svc) {
  switch (svc) {
    case kCallRtRedirected:
      Print("call rt redirected");
      return;
    case kBreakpoint:
      Print("breakpoint");
      return;
    default:
      if (svc >= kStopCode) {
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "%d - 0x%x",
                                        svc & kStopCodeMask,
                                        svc & kStopCodeMask);
      } else {
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "%d",
                                        svc);
      }
      return;
  }
}


// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instruction* instr, const char* format) {
  ASSERT(format[0] == 'r');
  if (format[1] == 'n') {  // 'rn: Rn register
    int reg = instr->RnValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'rd: Rd register
    int reg = instr->RdValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 's') {  // 'rs: Rs register
    int reg = instr->RsValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'm') {  // 'rm: Rm register
    int reg = instr->RmValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 't') {  // 'rt: Rt register
    int reg = instr->RtValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'l') {
    // 'rlist: register list for load and store multiple instructions
    ASSERT(STRING_STARTS_WITH(format, "rlist"));
    int rlist = instr->RlistValue();
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
int Decoder::FormatVFPRegister(Instruction* instr, const char* format) {
  ASSERT((format[0] == 'S') || (format[0] == 'D'));

  VFPRegPrecision precision =
      format[0] == 'D' ? kDoublePrecision : kSinglePrecision;

  int retval = 2;
  int reg = -1;
  if (format[1] == 'n') {
    reg = instr->VFPNRegValue(precision);
  } else if (format[1] == 'm') {
    reg = instr->VFPMRegValue(precision);
  } else if (format[1] == 'd') {
    if ((instr->TypeValue() == 7) &&
        (instr->Bit(24) == 0x0) &&
        (instr->Bits(11, 9) == 0x5) &&
        (instr->Bit(4) == 0x1)) {
      // vmov.32 has Vd in a different place.
      reg = instr->Bits(19, 16) | (instr->Bit(7) << 4);
    } else {
      reg = instr->VFPDRegValue(precision);
    }

    if (format[2] == '+') {
      int immed8 = instr->Immed8Value();
      if (format[0] == 'S') reg += immed8 - 1;
      if (format[0] == 'D') reg += (immed8 / 2 - 1);
    }
    if (format[2] == '+') retval = 3;
  } else {
    UNREACHABLE();
  }

  if (precision == kSinglePrecision) {
    PrintSRegister(reg);
  } else {
    PrintDRegister(reg);
  }

  return retval;
}


int Decoder::FormatVFPinstruction(Instruction* instr, const char* format) {
    Print(format);
    return 0;
}


void Decoder::FormatNeonList(int Vd, int type) {
  if (type == nlt_1) {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "{d%d}", Vd);
  } else if (type == nlt_2) {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "{d%d, d%d}", Vd, Vd + 1);
  } else if (type == nlt_3) {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "{d%d, d%d, d%d}", Vd, Vd + 1, Vd + 2);
  } else if (type == nlt_4) {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                            "{d%d, d%d, d%d, d%d}", Vd, Vd + 1, Vd + 2, Vd + 3);
  }
}


void Decoder::FormatNeonMemory(int Rn, int align, int Rm) {
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                  "[r%d", Rn);
  if (align != 0) {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    ":%d", (1 << align) << 6);
  }
  if (Rm == 15) {
    Print("]");
  } else if (Rm == 13) {
    Print("]!");
  } else {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "], r%d", Rm);
  }
}


// Print the movw or movt instruction.
void Decoder::PrintMovwMovt(Instruction* instr) {
  int imm = instr->ImmedMovwMovtValue();
  int rd = instr->RdValue();
  PrintRegister(rd);
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                  ", #%d", imm);
}


// FormatOption takes a formatting string and interprets it based on
// the current instructions. The format string points to the first
// character of the option string (the option escape has already been
// consumed by the caller.)  FormatOption returns the number of
// characters that were consumed from the formatting string.
int Decoder::FormatOption(Instruction* instr, const char* format) {
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
    case 'd': {  // 'd: vmov double immediate.
      double d = instr->DoubleImmedVmov();
      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "#%g", d);
      return 1;
    }
    case 'f': {  // 'f: bitfield instructions - v7 and above.
      uint32_t lsbit = instr->Bits(11, 7);
      uint32_t width = instr->Bits(20, 16) + 1;
      if (instr->Bit(21) == 0) {
        // BFC/BFI:
        // Bits 20-16 represent most-significant bit. Covert to width.
        width -= lsbit;
        ASSERT(width > 0);
      }
      ASSERT((width + lsbit) <= 32);
      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "#%d, #%d", lsbit, width);
      return 1;
    }
    case 'h': {  // 'h: halfword operation for extra loads and stores
      if (instr->HasH()) {
        Print("h");
      } else {
        Print("b");
      }
      return 1;
    }
    case 'i': {  // 'i: immediate value from adjacent bits.
      // Expects tokens in the form imm%02d@%02d, i.e. imm05@07, imm10@16
      int width = (format[3] - '0') * 10 + (format[4] - '0');
      int lsb   = (format[6] - '0') * 10 + (format[7] - '0');

      ASSERT((width >= 1) && (width <= 32));
      ASSERT((lsb >= 0) && (lsb <= 31));
      ASSERT((width + lsb) <= 32);

      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "%d",
                                      instr->Bits(width + lsb - 1, lsb));
      return 8;
    }
    case 'l': {  // 'l: branch and link
      if (instr->HasLink()) {
        Print("l");
      }
      return 1;
    }
    case 'm': {
      if (format[1] == 'w') {
        // 'mw: movt/movw instructions.
        PrintMovwMovt(instr);
        return 2;
      }
      if (format[1] == 'e') {  // 'memop: load/store instructions.
        ASSERT(STRING_STARTS_WITH(format, "memop"));
        if (instr->HasL()) {
          Print("ldr");
        } else {
          if ((instr->Bits(27, 25) == 0) && (instr->Bit(20) == 0) &&
              (instr->Bits(7, 6) == 3) && (instr->Bit(4) == 1)) {
            if (instr->Bit(5) == 1) {
              Print("strd");
            } else {
              Print("ldrd");
            }
            return 5;
          }
          Print("str");
        }
        return 5;
      }
      // 'msg: for simulator break instructions
      ASSERT(STRING_STARTS_WITH(format, "msg"));
      byte* str =
          reinterpret_cast<byte*>(instr->InstructionBits() & 0x0fffffff);
      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "%s", converter_.NameInCode(str));
      return 3;
    }
    case 'o': {
      if ((format[3] == '1') && (format[4] == '2')) {
        // 'off12: 12-bit offset for load and store instructions
        ASSERT(STRING_STARTS_WITH(format, "off12"));
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "%d", instr->Offset12Value());
        return 5;
      } else if (format[3] == '0') {
        // 'off0to3and8to19 16-bit immediate encoded in bits 19-8 and 3-0.
        ASSERT(STRING_STARTS_WITH(format, "off0to3and8to19"));
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "%d",
                                        (instr->Bits(19, 8) << 4) +
                                        instr->Bits(3, 0));
        return 15;
      }
      // 'off8: 8-bit offset for extra load and store instructions
      ASSERT(STRING_STARTS_WITH(format, "off8"));
      int offs8 = (instr->ImmedHValue() << 4) | instr->ImmedLValue();
      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
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
      if (format[1] == 'h') {  // 'shift_op or 'shift_rm or 'shift_sat.
        if (format[6] == 'o') {  // 'shift_op
          ASSERT(STRING_STARTS_WITH(format, "shift_op"));
          if (instr->TypeValue() == 0) {
            PrintShiftRm(instr);
          } else {
            ASSERT(instr->TypeValue() == 1);
            PrintShiftImm(instr);
          }
          return 8;
        } else if (format[6] == 's') {  // 'shift_sat.
          ASSERT(STRING_STARTS_WITH(format, "shift_sat"));
          PrintShiftSat(instr);
          return 9;
        } else {  // 'shift_rm
          ASSERT(STRING_STARTS_WITH(format, "shift_rm"));
          PrintShiftRm(instr);
          return 8;
        }
      } else if (format[1] == 'v') {  // 'svc
        ASSERT(STRING_STARTS_WITH(format, "svc"));
        PrintSoftwareInterrupt(instr->SvcValue());
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
      int off = (instr->SImmed24Value() << 2) + 8;
      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "%+d -> %s",
                                      off,
                                      converter_.NameOfAddress(
                                        reinterpret_cast<byte*>(instr) + off));
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


// The disassembler may end up decoding data inlined in the code. We do not want
// it to crash if the data does not ressemble any known instruction.
#define VERIFY(condition) \
if(!(condition)) {        \
  Unknown(instr);         \
  return;                 \
}


// For currently unimplemented decodings the disassembler calls Unknown(instr)
// which will just print "unknown" of the instruction bits.
void Decoder::Unknown(Instruction* instr) {
  Format(instr, "unknown");
}


void Decoder::DecodeType01(Instruction* instr) {
  int type = instr->TypeValue();
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
            if (instr->Bit(22) == 0) {
              // The MLA instruction description (A 4.1.28) refers to the order
              // of registers as "Rd, Rm, Rs, Rn". But confusingly it uses the
              // Rn field to encode the Rd register and the Rd field to encode
              // the Rn register.
              Format(instr, "mla'cond's 'rn, 'rm, 'rs, 'rd");
            } else {
              // The MLS instruction description (A 4.1.29) refers to the order
              // of registers as "Rd, Rm, Rs, Rn". But confusingly it uses the
              // Rn field to encode the Rd register and the Rd field to encode
              // the Rn register.
              Format(instr, "mls'cond's 'rn, 'rm, 'rs, 'rd");
            }
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
        case da_x: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond's 'rd, ['rn], -'rm");
          } else {
            Format(instr, "'memop'cond's 'rd, ['rn], #-'off8");
          }
          break;
        }
        case ia_x: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond's 'rd, ['rn], +'rm");
          } else {
            Format(instr, "'memop'cond's 'rd, ['rn], #+'off8");
          }
          break;
        }
        case db_x: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond's 'rd, ['rn, -'rm]'w");
          } else {
            Format(instr, "'memop'cond's 'rd, ['rn, #-'off8]'w");
          }
          break;
        }
        case ib_x: {
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
        case da_x: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], -'rm");
          } else {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], #-'off8");
          }
          break;
        }
        case ia_x: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], +'rm");
          } else {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn], #+'off8");
          }
          break;
        }
        case db_x: {
          if (instr->Bit(22) == 0) {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn, -'rm]'w");
          } else {
            Format(instr, "'memop'cond'sign'h 'rd, ['rn, #-'off8]'w");
          }
          break;
        }
        case ib_x: {
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
      switch (instr->BitField(7, 4)) {
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
      switch (instr->BitField(7, 4)) {
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
  } else if ((type == 1) && instr->IsNopType1()) {
    Format(instr, "nop'cond");
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
          Format(instr, "movw'cond 'mw");
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
          Format(instr, "movt'cond 'mw");
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


void Decoder::DecodeType2(Instruction* instr) {
  switch (instr->PUField()) {
    case da_x: {
      if (instr->HasW()) {
        Unknown(instr);  // not used in V8
        return;
      }
      Format(instr, "'memop'cond'b 'rd, ['rn], #-'off12");
      break;
    }
    case ia_x: {
      if (instr->HasW()) {
        Unknown(instr);  // not used in V8
        return;
      }
      Format(instr, "'memop'cond'b 'rd, ['rn], #+'off12");
      break;
    }
    case db_x: {
      Format(instr, "'memop'cond'b 'rd, ['rn, #-'off12]'w");
      break;
    }
    case ib_x: {
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


void Decoder::DecodeType3(Instruction* instr) {
  switch (instr->PUField()) {
    case da_x: {
      VERIFY(!instr->HasW());
      Format(instr, "'memop'cond'b 'rd, ['rn], -'shift_rm");
      break;
    }
    case ia_x: {
      if (instr->Bit(4) == 0) {
        Format(instr, "'memop'cond'b 'rd, ['rn], +'shift_rm");
      } else {
        if (instr->Bit(5) == 0) {
          switch (instr->Bits(22, 21)) {
            case 0:
              if (instr->Bit(20) == 0) {
                if (instr->Bit(6) == 0) {
                  Format(instr, "pkhbt'cond 'rd, 'rn, 'rm, lsl #'imm05@07");
                } else {
                  if (instr->Bits(11, 7) == 0) {
                    Format(instr, "pkhtb'cond 'rd, 'rn, 'rm, asr #32");
                  } else {
                    Format(instr, "pkhtb'cond 'rd, 'rn, 'rm, asr #'imm05@07");
                  }
                }
              } else {
                UNREACHABLE();
              }
              break;
            case 1:
              UNREACHABLE();
              break;
            case 2:
              UNREACHABLE();
              break;
            case 3:
              Format(instr, "usat 'rd, #'imm05@16, 'rm'shift_sat");
              break;
          }
        } else {
          switch (instr->Bits(22, 21)) {
            case 0:
              UNREACHABLE();
              break;
            case 1:
              UNREACHABLE();
              break;
            case 2:
              if ((instr->Bit(20) == 0) && (instr->Bits(9, 6) == 1)) {
                if (instr->Bits(19, 16) == 0xF) {
                  switch (instr->Bits(11, 10)) {
                    case 0:
                      Format(instr, "uxtb16'cond 'rd, 'rm");
                      break;
                    case 1:
                      Format(instr, "uxtb16'cond 'rd, 'rm, ror #8");
                      break;
                    case 2:
                      Format(instr, "uxtb16'cond 'rd, 'rm, ror #16");
                      break;
                    case 3:
                      Format(instr, "uxtb16'cond 'rd, 'rm, ror #24");
                      break;
                  }
                } else {
                  UNREACHABLE();
                }
              } else {
                UNREACHABLE();
              }
              break;
            case 3:
              if ((instr->Bit(20) == 0) && (instr->Bits(9, 6) == 1)) {
                if (instr->Bits(19, 16) == 0xF) {
                  switch (instr->Bits(11, 10)) {
                    case 0:
                      Format(instr, "uxtb'cond 'rd, 'rm");
                      break;
                    case 1:
                      Format(instr, "uxtb'cond 'rd, 'rm, ror #8");
                      break;
                    case 2:
                      Format(instr, "uxtb'cond 'rd, 'rm, ror #16");
                      break;
                    case 3:
                      Format(instr, "uxtb'cond 'rd, 'rm, ror #24");
                      break;
                  }
                } else {
                  switch (instr->Bits(11, 10)) {
                    case 0:
                      Format(instr, "uxtab'cond 'rd, 'rn, 'rm");
                      break;
                    case 1:
                      Format(instr, "uxtab'cond 'rd, 'rn, 'rm, ror #8");
                      break;
                    case 2:
                      Format(instr, "uxtab'cond 'rd, 'rn, 'rm, ror #16");
                      break;
                    case 3:
                      Format(instr, "uxtab'cond 'rd, 'rn, 'rm, ror #24");
                      break;
                  }
                }
              } else {
                UNREACHABLE();
              }
              break;
          }
        }
      }
      break;
    }
    case db_x: {
      if (FLAG_enable_sudiv) {
        if (!instr->HasW()) {
          if (instr->Bits(5, 4) == 0x1) {
            if ((instr->Bit(22) == 0x0) && (instr->Bit(20) == 0x1)) {
              // SDIV (in V8 notation matching ARM ISA format) rn = rm/rs
              Format(instr, "sdiv'cond'b 'rn, 'rm, 'rs");
              break;
            }
          }
        }
      }
      Format(instr, "'memop'cond'b 'rd, ['rn, -'shift_rm]'w");
      break;
    }
    case ib_x: {
      if (instr->HasW() && (instr->Bits(6, 4) == 0x5)) {
        uint32_t widthminus1 = static_cast<uint32_t>(instr->Bits(20, 16));
        uint32_t lsbit = static_cast<uint32_t>(instr->Bits(11, 7));
        uint32_t msbit = widthminus1 + lsbit;
        if (msbit <= 31) {
          if (instr->Bit(22)) {
            Format(instr, "ubfx'cond 'rd, 'rm, 'f");
          } else {
            Format(instr, "sbfx'cond 'rd, 'rm, 'f");
          }
        } else {
          UNREACHABLE();
        }
      } else if (!instr->HasW() && (instr->Bits(6, 4) == 0x1)) {
        uint32_t lsbit = static_cast<uint32_t>(instr->Bits(11, 7));
        uint32_t msbit = static_cast<uint32_t>(instr->Bits(20, 16));
        if (msbit >= lsbit) {
          if (instr->RmValue() == 15) {
            Format(instr, "bfc'cond 'rd, 'f");
          } else {
            Format(instr, "bfi'cond 'rd, 'rm, 'f");
          }
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


void Decoder::DecodeType4(Instruction* instr) {
  if (instr->Bit(22) != 0) {
    // Privileged mode currently not supported.
    Unknown(instr);
  } else {
    if (instr->HasL()) {
      Format(instr, "ldm'cond'pu 'rn'w, 'rlist");
    } else {
      Format(instr, "stm'cond'pu 'rn'w, 'rlist");
    }
  }
}


void Decoder::DecodeType5(Instruction* instr) {
  Format(instr, "b'l'cond 'target");
}


void Decoder::DecodeType6(Instruction* instr) {
  DecodeType6CoprocessorIns(instr);
}


int Decoder::DecodeType7(Instruction* instr) {
  if (instr->Bit(24) == 1) {
    if (instr->SvcValue() >= kStopCode) {
      Format(instr, "stop'cond 'svc");
      // Also print the stop message. Its address is encoded
      // in the following 4 bytes.
      out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "\n  %p  %08x       stop message: %s",
                                      reinterpret_cast<int32_t*>(instr
                                                     + Instruction::kInstrSize),
                                      *reinterpret_cast<char**>(instr
                                                    + Instruction::kInstrSize),
                                      *reinterpret_cast<char**>(instr
                                                    + Instruction::kInstrSize));
      // We have decoded 2 * Instruction::kInstrSize bytes.
      return 2 * Instruction::kInstrSize;
    } else {
      Format(instr, "svc'cond 'svc");
    }
  } else {
    DecodeTypeVFP(instr);
  }
  return Instruction::kInstrSize;
}


// void Decoder::DecodeTypeVFP(Instruction* instr)
// vmov: Sn = Rt
// vmov: Rt = Sn
// vcvt: Dd = Sm
// vcvt: Sd = Dm
// vcvt.f64.s32 Dd, Dd, #<fbits>
// Dd = vabs(Dm)
// Dd = vneg(Dm)
// Dd = vadd(Dn, Dm)
// Dd = vsub(Dn, Dm)
// Dd = vmul(Dn, Dm)
// Dd = vmla(Dn, Dm)
// Dd = vmls(Dn, Dm)
// Dd = vdiv(Dn, Dm)
// vcmp(Dd, Dm)
// vmrs
// vmsr
// Dd = vsqrt(Dm)
void Decoder::DecodeTypeVFP(Instruction* instr) {
  VERIFY((instr->TypeValue() == 7) && (instr->Bit(24) == 0x0) );
  VERIFY(instr->Bits(11, 9) == 0x5);

  if (instr->Bit(4) == 0) {
    if (instr->Opc1Value() == 0x7) {
      // Other data processing instructions
      if ((instr->Opc2Value() == 0x0) && (instr->Opc3Value() == 0x1)) {
        // vmov register to register.
        if (instr->SzValue() == 0x1) {
          Format(instr, "vmov'cond.f64 'Dd, 'Dm");
        } else {
          Format(instr, "vmov'cond.f32 'Sd, 'Sm");
        }
      } else if ((instr->Opc2Value() == 0x0) && (instr->Opc3Value() == 0x3)) {
        // vabs
        Format(instr, "vabs'cond.f64 'Dd, 'Dm");
      } else if ((instr->Opc2Value() == 0x1) && (instr->Opc3Value() == 0x1)) {
        // vneg
        Format(instr, "vneg'cond.f64 'Dd, 'Dm");
      } else if ((instr->Opc2Value() == 0x7) && (instr->Opc3Value() == 0x3)) {
        DecodeVCVTBetweenDoubleAndSingle(instr);
      } else if ((instr->Opc2Value() == 0x8) && (instr->Opc3Value() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if ((instr->Opc2Value() == 0xA) && (instr->Opc3Value() == 0x3) &&
                 (instr->Bit(8) == 1)) {
        // vcvt.f64.s32 Dd, Dd, #<fbits>
        int fraction_bits = 32 - ((instr->Bits(3, 0) << 1) | instr->Bit(5));
        Format(instr, "vcvt'cond.f64.s32 'Dd, 'Dd");
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        ", #%d", fraction_bits);
      } else if (((instr->Opc2Value() >> 1) == 0x6) &&
                 (instr->Opc3Value() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if (((instr->Opc2Value() == 0x4) || (instr->Opc2Value() == 0x5)) &&
                 (instr->Opc3Value() & 0x1)) {
        DecodeVCMP(instr);
      } else if (((instr->Opc2Value() == 0x1)) && (instr->Opc3Value() == 0x3)) {
        Format(instr, "vsqrt'cond.f64 'Dd, 'Dm");
      } else if (instr->Opc3Value() == 0x0) {
        if (instr->SzValue() == 0x1) {
          Format(instr, "vmov'cond.f64 'Dd, 'd");
        } else {
          Unknown(instr);  // Not used by V8.
        }
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if (instr->Opc1Value() == 0x3) {
      if (instr->SzValue() == 0x1) {
        if (instr->Opc3Value() & 0x1) {
          Format(instr, "vsub'cond.f64 'Dd, 'Dn, 'Dm");
        } else {
          Format(instr, "vadd'cond.f64 'Dd, 'Dn, 'Dm");
        }
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if ((instr->Opc1Value() == 0x2) && !(instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vmul'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if ((instr->Opc1Value() == 0x0) && !(instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vmla'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if ((instr->Opc1Value() == 0x0) && (instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vmls'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else if ((instr->Opc1Value() == 0x4) && !(instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vdiv'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Unknown(instr);  // Not used by V8.
      }
    } else {
      Unknown(instr);  // Not used by V8.
    }
  } else {
    if ((instr->VCValue() == 0x0) &&
        (instr->VAValue() == 0x0)) {
      DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(instr);
    } else if ((instr->VLValue() == 0x0) &&
               (instr->VCValue() == 0x1) &&
               (instr->Bit(23) == 0x0)) {
      if (instr->Bit(21) == 0x0) {
        Format(instr, "vmov'cond.32 'Dd[0], 'rt");
      } else {
        Format(instr, "vmov'cond.32 'Dd[1], 'rt");
      }
    } else if ((instr->VLValue() == 0x1) &&
               (instr->VCValue() == 0x1) &&
               (instr->Bit(23) == 0x0)) {
      if (instr->Bit(21) == 0x0) {
        Format(instr, "vmov'cond.32 'rt, 'Dd[0]");
      } else {
        Format(instr, "vmov'cond.32 'rt, 'Dd[1]");
      }
    } else if ((instr->VCValue() == 0x0) &&
               (instr->VAValue() == 0x7) &&
               (instr->Bits(19, 16) == 0x1)) {
      if (instr->VLValue() == 0) {
        if (instr->Bits(15, 12) == 0xF) {
          Format(instr, "vmsr'cond FPSCR, APSR");
        } else {
          Format(instr, "vmsr'cond FPSCR, 'rt");
        }
      } else {
        if (instr->Bits(15, 12) == 0xF) {
          Format(instr, "vmrs'cond APSR, FPSCR");
        } else {
          Format(instr, "vmrs'cond 'rt, FPSCR");
        }
      }
    }
  }
}


void Decoder::DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(
    Instruction* instr) {
  VERIFY((instr->Bit(4) == 1) && (instr->VCValue() == 0x0) &&
         (instr->VAValue() == 0x0));

  bool to_arm_register = (instr->VLValue() == 0x1);

  if (to_arm_register) {
    Format(instr, "vmov'cond 'rt, 'Sn");
  } else {
    Format(instr, "vmov'cond 'Sn, 'rt");
  }
}


void Decoder::DecodeVCMP(Instruction* instr) {
  VERIFY((instr->Bit(4) == 0) && (instr->Opc1Value() == 0x7));
  VERIFY(((instr->Opc2Value() == 0x4) || (instr->Opc2Value() == 0x5)) &&
         (instr->Opc3Value() & 0x1));

  // Comparison.
  bool dp_operation = (instr->SzValue() == 1);
  bool raise_exception_for_qnan = (instr->Bit(7) == 0x1);

  if (dp_operation && !raise_exception_for_qnan) {
    if (instr->Opc2Value() == 0x4) {
      Format(instr, "vcmp'cond.f64 'Dd, 'Dm");
    } else if (instr->Opc2Value() == 0x5) {
      Format(instr, "vcmp'cond.f64 'Dd, #0.0");
    } else {
      Unknown(instr);  // invalid
    }
  } else {
    Unknown(instr);  // Not used by V8.
  }
}


void Decoder::DecodeVCVTBetweenDoubleAndSingle(Instruction* instr) {
  VERIFY((instr->Bit(4) == 0) && (instr->Opc1Value() == 0x7));
  VERIFY((instr->Opc2Value() == 0x7) && (instr->Opc3Value() == 0x3));

  bool double_to_single = (instr->SzValue() == 1);

  if (double_to_single) {
    Format(instr, "vcvt'cond.f32.f64 'Sd, 'Dm");
  } else {
    Format(instr, "vcvt'cond.f64.f32 'Dd, 'Sm");
  }
}


void Decoder::DecodeVCVTBetweenFloatingPointAndInteger(Instruction* instr) {
  VERIFY((instr->Bit(4) == 0) && (instr->Opc1Value() == 0x7));
  VERIFY(((instr->Opc2Value() == 0x8) && (instr->Opc3Value() & 0x1)) ||
         (((instr->Opc2Value() >> 1) == 0x6) && (instr->Opc3Value() & 0x1)));

  bool to_integer = (instr->Bit(18) == 1);
  bool dp_operation = (instr->SzValue() == 1);
  if (to_integer) {
    bool unsigned_integer = (instr->Bit(16) == 0);

    if (dp_operation) {
      if (unsigned_integer) {
        Format(instr, "vcvt'cond.u32.f64 'Sd, 'Dm");
      } else {
        Format(instr, "vcvt'cond.s32.f64 'Sd, 'Dm");
      }
    } else {
      if (unsigned_integer) {
        Format(instr, "vcvt'cond.u32.f32 'Sd, 'Sm");
      } else {
        Format(instr, "vcvt'cond.s32.f32 'Sd, 'Sm");
      }
    }
  } else {
    bool unsigned_integer = (instr->Bit(7) == 0);

    if (dp_operation) {
      if (unsigned_integer) {
        Format(instr, "vcvt'cond.f64.u32 'Dd, 'Sm");
      } else {
        Format(instr, "vcvt'cond.f64.s32 'Dd, 'Sm");
      }
    } else {
      if (unsigned_integer) {
        Format(instr, "vcvt'cond.f32.u32 'Sd, 'Sm");
      } else {
        Format(instr, "vcvt'cond.f32.s32 'Sd, 'Sm");
      }
    }
  }
}


// Decode Type 6 coprocessor instructions.
// Dm = vmov(Rt, Rt2)
// <Rt, Rt2> = vmov(Dm)
// Ddst = MEM(Rbase + 4*offset).
// MEM(Rbase + 4*offset) = Dsrc.
void Decoder::DecodeType6CoprocessorIns(Instruction* instr) {
  VERIFY(instr->TypeValue() == 6);

  if (instr->CoprocessorValue() == 0xA) {
    switch (instr->OpcodeValue()) {
      case 0x8:
      case 0xA:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Sd, ['rn - 4*'imm08@00]");
        } else {
          Format(instr, "vstr'cond 'Sd, ['rn - 4*'imm08@00]");
        }
        break;
      case 0xC:
      case 0xE:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Sd, ['rn + 4*'imm08@00]");
        } else {
          Format(instr, "vstr'cond 'Sd, ['rn + 4*'imm08@00]");
        }
        break;
      case 0x4:
      case 0x5:
      case 0x6:
      case 0x7:
      case 0x9:
      case 0xB: {
        bool to_vfp_register = (instr->VLValue() == 0x1);
        if (to_vfp_register) {
          Format(instr, "vldm'cond'pu 'rn'w, {'Sd-'Sd+}");
        } else {
          Format(instr, "vstm'cond'pu 'rn'w, {'Sd-'Sd+}");
        }
        break;
      }
      default:
        Unknown(instr);  // Not used by V8.
    }
  } else if (instr->CoprocessorValue() == 0xB) {
    switch (instr->OpcodeValue()) {
      case 0x2:
        // Load and store double to two GP registers
        if (instr->Bits(7, 6) != 0 || instr->Bit(4) != 1) {
          Unknown(instr);  // Not used by V8.
        } else if (instr->HasL()) {
          Format(instr, "vmov'cond 'rt, 'rn, 'Dm");
        } else {
          Format(instr, "vmov'cond 'Dm, 'rt, 'rn");
        }
        break;
      case 0x8:
      case 0xA:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Dd, ['rn - 4*'imm08@00]");
        } else {
          Format(instr, "vstr'cond 'Dd, ['rn - 4*'imm08@00]");
        }
        break;
      case 0xC:
      case 0xE:
        if (instr->HasL()) {
          Format(instr, "vldr'cond 'Dd, ['rn + 4*'imm08@00]");
        } else {
          Format(instr, "vstr'cond 'Dd, ['rn + 4*'imm08@00]");
        }
        break;
      case 0x4:
      case 0x5:
      case 0x6:
      case 0x7:
      case 0x9:
      case 0xB: {
        bool to_vfp_register = (instr->VLValue() == 0x1);
        if (to_vfp_register) {
          Format(instr, "vldm'cond'pu 'rn'w, {'Dd-'Dd+}");
        } else {
          Format(instr, "vstm'cond'pu 'rn'w, {'Dd-'Dd+}");
        }
        break;
      }
      default:
        Unknown(instr);  // Not used by V8.
    }
  } else {
    Unknown(instr);  // Not used by V8.
  }
}


void Decoder::DecodeSpecialCondition(Instruction* instr) {
  switch (instr->SpecialValue()) {
    case 5:
      if ((instr->Bits(18, 16) == 0) && (instr->Bits(11, 6) == 0x28) &&
          (instr->Bit(4) == 1)) {
        // vmovl signed
        if ((instr->VdValue() & 1) != 0) Unknown(instr);
        int Vd = (instr->Bit(22) << 3) | (instr->VdValue() >> 1);
        int Vm = (instr->Bit(5) << 4) | instr->VmValue();
        int imm3 = instr->Bits(21, 19);
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vmovl.s%d q%d, d%d", imm3*8, Vd, Vm);
      } else {
        Unknown(instr);
      }
      break;
    case 7:
      if ((instr->Bits(18, 16) == 0) && (instr->Bits(11, 6) == 0x28) &&
          (instr->Bit(4) == 1)) {
        // vmovl unsigned
        if ((instr->VdValue() & 1) != 0) Unknown(instr);
        int Vd = (instr->Bit(22) << 3) | (instr->VdValue() >> 1);
        int Vm = (instr->Bit(5) << 4) | instr->VmValue();
        int imm3 = instr->Bits(21, 19);
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vmovl.u%d q%d, d%d", imm3*8, Vd, Vm);
      } else {
        Unknown(instr);
      }
      break;
    case 8:
      if (instr->Bits(21, 20) == 0) {
        // vst1
        int Vd = (instr->Bit(22) << 4) | instr->VdValue();
        int Rn = instr->VnValue();
        int type = instr->Bits(11, 8);
        int size = instr->Bits(7, 6);
        int align = instr->Bits(5, 4);
        int Rm = instr->VmValue();
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vst1.%d ", (1 << size) << 3);
        FormatNeonList(Vd, type);
        Print(", ");
        FormatNeonMemory(Rn, align, Rm);
      } else if (instr->Bits(21, 20) == 2) {
        // vld1
        int Vd = (instr->Bit(22) << 4) | instr->VdValue();
        int Rn = instr->VnValue();
        int type = instr->Bits(11, 8);
        int size = instr->Bits(7, 6);
        int align = instr->Bits(5, 4);
        int Rm = instr->VmValue();
        out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vld1.%d ", (1 << size) << 3);
        FormatNeonList(Vd, type);
        Print(", ");
        FormatNeonMemory(Rn, align, Rm);
      } else {
        Unknown(instr);
      }
      break;
    case 0xA:
    case 0xB:
      if ((instr->Bits(22, 20) == 5) && (instr->Bits(15, 12) == 0xf)) {
        int Rn = instr->Bits(19, 16);
        int offset = instr->Bits(11, 0);
        if (offset == 0) {
          out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "pld [r%d]", Rn);
        } else if (instr->Bit(23) == 0) {
          out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "pld [r%d, #-%d]", Rn, offset);
        } else {
          out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "pld [r%d, #+%d]", Rn, offset);
        }
      } else {
        Unknown(instr);
      }
      break;
    default:
      Unknown(instr);
      break;
  }
}

#undef VERIFIY

bool Decoder::IsConstantPoolAt(byte* instr_ptr) {
  int instruction_bits = *(reinterpret_cast<int*>(instr_ptr));
  return (instruction_bits & kConstantPoolMarkerMask) == kConstantPoolMarker;
}


int Decoder::ConstantPoolSizeAt(byte* instr_ptr) {
  if (IsConstantPoolAt(instr_ptr)) {
    int instruction_bits = *(reinterpret_cast<int*>(instr_ptr));
    return DecodeConstantPoolLength(instruction_bits);
  } else {
    return -1;
  }
}


// Disassemble the instruction at *instr_ptr into the output buffer.
int Decoder::InstructionDecode(byte* instr_ptr) {
  Instruction* instr = Instruction::At(instr_ptr);
  // Print raw instruction bytes.
  out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                  "%08x       ",
                                  instr->InstructionBits());
  if (instr->ConditionField() == kSpecialCondition) {
    DecodeSpecialCondition(instr);
    return Instruction::kInstrSize;
  }
  int instruction_bits = *(reinterpret_cast<int*>(instr_ptr));
  if ((instruction_bits & kConstantPoolMarkerMask) == kConstantPoolMarker) {
    out_buffer_pos_ += OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "constant pool begin (length %d)",
                                    DecodeConstantPoolLength(instruction_bits));
    return Instruction::kInstrSize;
  } else if (instruction_bits == kCodeAgeJumpInstruction) {
    // The code age prologue has a constant immediatly following the jump
    // instruction.
    Instruction* target = Instruction::At(instr_ptr + Instruction::kInstrSize);
    DecodeType2(instr);
    OS::SNPrintF(out_buffer_ + out_buffer_pos_,
                 " (0x%08x)", target->InstructionBits());
    return 2 * Instruction::kInstrSize;
  }
  switch (instr->TypeValue()) {
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
      return DecodeType7(instr);
    }
    default: {
      // The type field is 3-bits in the ARM encoding.
      UNREACHABLE();
      break;
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
  v8::internal::Decoder d(converter_, buffer);
  return d.InstructionDecode(instruction);
}


int Disassembler::ConstantPoolSizeAt(byte* instruction) {
  return v8::internal::Decoder::ConstantPoolSizeAt(instruction);
}


void Disassembler::Disassemble(FILE* f, byte* begin, byte* end) {
  NameConverter converter;
  Disassembler d(converter);
  for (byte* pc = begin; pc < end;) {
    v8::internal::EmbeddedVector<char, 128> buffer;
    buffer[0] = '\0';
    byte* prev_pc = pc;
    pc += d.InstructionDecode(buffer, pc);
    v8::internal::PrintF(
        f, "%p    %08x      %s\n",
        prev_pc, *reinterpret_cast<int32_t*>(prev_pc), buffer.start());
  }
}


}  // namespace disasm

#endif  // V8_TARGET_ARCH_ARM

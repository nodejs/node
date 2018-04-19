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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if V8_TARGET_ARCH_ARM

#include "src/arm/constants-arm.h"
#include "src/base/bits.h"
#include "src/base/platform/platform.h"
#include "src/disasm.h"
#include "src/macro-assembler.h"


namespace v8 {
namespace internal {

const auto GetRegConfig = RegisterConfiguration::Default;

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
  // CP15 coprocessor instructions.
  void DecodeTypeCP15(Instruction* instr);
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
static const char* const cond_names[kNumberOfConditions] = {
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
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                ", %s #%d",
                                shift_names[shift_index],
                                shift_amount);
  } else {
    // by register
    int rs = instr->RsValue();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                ", %s ", shift_names[shift_index]);
    PrintRegister(rs);
  }
}


// Print the immediate operand for the instruction. Generally used for data
// processing instructions.
void Decoder::PrintShiftImm(Instruction* instr) {
  int rotate = instr->RotateValue() * 2;
  int immed8 = instr->Immed8Value();
  int imm = base::bits::RotateRight32(immed8, rotate);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "#%d", imm);
}


// Print the optional shift and immediate used by saturating instructions.
void Decoder::PrintShiftSat(Instruction* instr) {
  int shift = instr->Bits(11, 7);
  if (shift > 0) {
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
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
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "%d - 0x%x",
                                    svc & kStopCodeMask,
                                    svc & kStopCodeMask);
      } else {
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "%d",
                                    svc);
      }
      return;
  }
}


// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instruction* instr, const char* format) {
  DCHECK_EQ(format[0], 'r');
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
    DCHECK(STRING_STARTS_WITH(format, "rlist"));
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
}


// Handle all VFP register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatVFPRegister(Instruction* instr, const char* format) {
  DCHECK((format[0] == 'S') || (format[0] == 'D'));

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
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                "{d%d}", Vd);
  } else if (type == nlt_2) {
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                "{d%d, d%d}", Vd, Vd + 1);
  } else if (type == nlt_3) {
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                "{d%d, d%d, d%d}", Vd, Vd + 1, Vd + 2);
  } else if (type == nlt_4) {
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                        "{d%d, d%d, d%d, d%d}", Vd, Vd + 1, Vd + 2, Vd + 3);
  }
}


void Decoder::FormatNeonMemory(int Rn, int align, int Rm) {
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "[%s",
                              converter_.NameOfCPURegister(Rn));
  if (align != 0) {
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                ":%d", (1 << align) << 6);
  }
  if (Rm == 15) {
    Print("]");
  } else if (Rm == 13) {
    Print("]!");
  } else {
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "], %s",
                                converter_.NameOfCPURegister(Rm));
  }
}


// Print the movw or movt instruction.
void Decoder::PrintMovwMovt(Instruction* instr) {
  int imm = instr->ImmedMovwMovtValue();
  int rd = instr->RdValue();
  PrintRegister(rd);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, ", #%d", imm);
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
      DCHECK(STRING_STARTS_WITH(format, "cond"));
      PrintCondition(instr);
      return 4;
    }
    case 'd': {  // 'd: vmov double immediate.
      double d = instr->DoubleImmedVmov().get_scalar();
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "#%g", d);
      return 1;
    }
    case 'f': {  // 'f: bitfield instructions - v7 and above.
      uint32_t lsbit = instr->Bits(11, 7);
      uint32_t width = instr->Bits(20, 16) + 1;
      if (instr->Bit(21) == 0) {
        // BFC/BFI:
        // Bits 20-16 represent most-significant bit. Covert to width.
        width -= lsbit;
        DCHECK_GT(width, 0);
      }
      DCHECK_LE(width + lsbit, 32);
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
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

      DCHECK((width >= 1) && (width <= 32));
      DCHECK((lsb >= 0) && (lsb <= 31));
      DCHECK_LE(width + lsb, 32);

      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
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
        DCHECK(STRING_STARTS_WITH(format, "memop"));
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
      DCHECK(STRING_STARTS_WITH(format, "msg"));
      byte* str =
          reinterpret_cast<byte*>(instr->InstructionBits() & 0x0FFFFFFF);
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                  "%s", converter_.NameInCode(str));
      return 3;
    }
    case 'o': {
      if ((format[3] == '1') && (format[4] == '2')) {
        // 'off12: 12-bit offset for load and store instructions
        DCHECK(STRING_STARTS_WITH(format, "off12"));
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "%d", instr->Offset12Value());
        return 5;
      } else if (format[3] == '0') {
        // 'off0to3and8to19 16-bit immediate encoded in bits 19-8 and 3-0.
        DCHECK(STRING_STARTS_WITH(format, "off0to3and8to19"));
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "%d",
                                    (instr->Bits(19, 8) << 4) +
                                    instr->Bits(3, 0));
        return 15;
      }
      // 'off8: 8-bit offset for extra load and store instructions
      DCHECK(STRING_STARTS_WITH(format, "off8"));
      int offs8 = (instr->ImmedHValue() << 4) | instr->ImmedLValue();
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", offs8);
      return 4;
    }
    case 'p': {  // 'pu: P and U bits for load and store instructions
      DCHECK(STRING_STARTS_WITH(format, "pu"));
      PrintPU(instr);
      return 2;
    }
    case 'r': {
      return FormatRegister(instr, format);
    }
    case 's': {
      if (format[1] == 'h') {  // 'shift_op or 'shift_rm or 'shift_sat.
        if (format[6] == 'o') {  // 'shift_op
          DCHECK(STRING_STARTS_WITH(format, "shift_op"));
          if (instr->TypeValue() == 0) {
            PrintShiftRm(instr);
          } else {
            DCHECK_EQ(instr->TypeValue(), 1);
            PrintShiftImm(instr);
          }
          return 8;
        } else if (format[6] == 's') {  // 'shift_sat.
          DCHECK(STRING_STARTS_WITH(format, "shift_sat"));
          PrintShiftSat(instr);
          return 9;
        } else {  // 'shift_rm
          DCHECK(STRING_STARTS_WITH(format, "shift_rm"));
          PrintShiftRm(instr);
          return 8;
        }
      } else if (format[1] == 'v') {  // 'svc
        DCHECK(STRING_STARTS_WITH(format, "svc"));
        PrintSoftwareInterrupt(instr->SvcValue());
        return 3;
      } else if (format[1] == 'i') {  // 'sign: signed extra loads and stores
        DCHECK(STRING_STARTS_WITH(format, "sign"));
        if (instr->HasSign()) {
          Print("s");
        }
        return 4;
      } else if (format[1] == 'p') {
        if (format[8] == '_') {  // 'spec_reg_fields
          DCHECK(STRING_STARTS_WITH(format, "spec_reg_fields"));
          Print("_");
          int mask = instr->Bits(19, 16);
          if (mask == 0) Print("(none)");
          if ((mask & 0x8) != 0) Print("f");
          if ((mask & 0x4) != 0) Print("s");
          if ((mask & 0x2) != 0) Print("x");
          if ((mask & 0x1) != 0) Print("c");
          return 15;
        } else {  // 'spec_reg
          DCHECK(STRING_STARTS_WITH(format, "spec_reg"));
          if (instr->Bit(22) == 0) {
            Print("CPSR");
          } else {
            Print("SPSR");
          }
          return 8;
        }
      }
      // 's: S field of data processing instructions
      if (instr->HasS()) {
        Print("s");
      }
      return 1;
    }
    case 't': {  // 'target: target of branch instructions
      DCHECK(STRING_STARTS_WITH(format, "target"));
      int off = (instr->SImmed24Value() << 2) + 8;
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
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
    case 'A': {
      // Print pc-relative address.
      int offset = instr->Offset12Value();
      byte* pc = reinterpret_cast<byte*>(instr) + Instruction::kPCReadOffset;
      byte* addr;
      switch (instr->PUField()) {
        case db_x: {
          addr = pc - offset;
          break;
        }
        case ib_x: {
          addr = pc + offset;
          break;
        }
        default: {
          UNREACHABLE();
          return -1;
        }
      }
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%p",
                                  static_cast<void*>(addr));
      return 1;
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
// it to crash if the data does not resemble any known instruction.
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
        if (instr->Bits(24, 23) == 3) {
          if (instr->Bit(20) == 1) {
            // ldrex
            switch (instr->Bits(22, 21)) {
              case 0:
                Format(instr, "ldrex'cond 'rt, ['rn]");
                break;
              case 2:
                Format(instr, "ldrexb'cond 'rt, ['rn]");
                break;
              case 3:
                Format(instr, "ldrexh'cond 'rt, ['rn]");
                break;
              default:
                UNREACHABLE();
                break;
            }
          } else {
            // strex
            // The instruction is documented as strex rd, rt, [rn], but the
            // "rt" register is using the rm bits.
            switch (instr->Bits(22, 21)) {
              case 0:
                Format(instr, "strex'cond 'rd, 'rm, ['rn]");
                break;
              case 2:
                Format(instr, "strexb'cond 'rd, 'rm, ['rn]");
                break;
              case 3:
                Format(instr, "strexh'cond 'rd, 'rm, ['rn]");
                break;
              default:
                UNREACHABLE();
                break;
            }
          }
        } else {
          Unknown(instr);  // not used by V8
        }
      }
    } else if ((instr->Bit(20) == 0) && ((instr->Bits(7, 4) & 0xD) == 0xD)) {
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
    if ((instr->Bits(27, 23) == 2) && (instr->Bits(21, 20) == 2) &&
        (instr->Bits(15, 4) == 0xF00)) {
      Format(instr, "msr'cond 'spec_reg'spec_reg_fields, 'rm");
    } else if ((instr->Bits(27, 23) == 2) && (instr->Bits(21, 20) == 0) &&
               (instr->Bits(11, 0) == 0)) {
      Format(instr, "mrs'cond 'rd, 'spec_reg");
    } else if (instr->Bits(22, 21) == 1) {
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
  } else if ((type == 1) && instr->IsNopLikeType1()) {
    if (instr->BitField(7, 0) == 0) {
      Format(instr, "nop'cond");
    } else if (instr->BitField(7, 0) == 20) {
      Format(instr, "csdb");
    } else {
      Unknown(instr);  // Not used in V8.
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
      if (instr->HasL() && (instr->RnValue() == kPCRegister)) {
        Format(instr, "'memop'cond'b 'rd, [pc, #-'off12]'w (addr 'A)");
      } else {
        Format(instr, "'memop'cond'b 'rd, ['rn, #-'off12]'w");
      }
      break;
    }
    case ib_x: {
      if (instr->HasL() && (instr->RnValue() == kPCRegister)) {
        Format(instr, "'memop'cond'b 'rd, [pc, #+'off12]'w (addr 'A)");
      } else {
        Format(instr, "'memop'cond'b 'rd, ['rn, #+'off12]'w");
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
              if (instr->Bits(9, 6) == 1) {
                if (instr->Bit(20) == 0) {
                  if (instr->Bits(19, 16) == 0xF) {
                    switch (instr->Bits(11, 10)) {
                      case 0:
                        Format(instr, "sxtb'cond 'rd, 'rm");
                        break;
                      case 1:
                        Format(instr, "sxtb'cond 'rd, 'rm, ror #8");
                        break;
                      case 2:
                        Format(instr, "sxtb'cond 'rd, 'rm, ror #16");
                        break;
                      case 3:
                        Format(instr, "sxtb'cond 'rd, 'rm, ror #24");
                        break;
                    }
                  } else {
                    switch (instr->Bits(11, 10)) {
                      case 0:
                        Format(instr, "sxtab'cond 'rd, 'rn, 'rm");
                        break;
                      case 1:
                        Format(instr, "sxtab'cond 'rd, 'rn, 'rm, ror #8");
                        break;
                      case 2:
                        Format(instr, "sxtab'cond 'rd, 'rn, 'rm, ror #16");
                        break;
                      case 3:
                        Format(instr, "sxtab'cond 'rd, 'rn, 'rm, ror #24");
                        break;
                    }
                  }
                } else {
                  if (instr->Bits(19, 16) == 0xF) {
                    switch (instr->Bits(11, 10)) {
                      case 0:
                        Format(instr, "sxth'cond 'rd, 'rm");
                        break;
                      case 1:
                        Format(instr, "sxth'cond 'rd, 'rm, ror #8");
                        break;
                      case 2:
                        Format(instr, "sxth'cond 'rd, 'rm, ror #16");
                        break;
                      case 3:
                        Format(instr, "sxth'cond 'rd, 'rm, ror #24");
                        break;
                    }
                  } else {
                    switch (instr->Bits(11, 10)) {
                      case 0:
                        Format(instr, "sxtah'cond 'rd, 'rn, 'rm");
                        break;
                      case 1:
                        Format(instr, "sxtah'cond 'rd, 'rn, 'rm, ror #8");
                        break;
                      case 2:
                        Format(instr, "sxtah'cond 'rd, 'rn, 'rm, ror #16");
                        break;
                      case 3:
                        Format(instr, "sxtah'cond 'rd, 'rn, 'rm, ror #24");
                        break;
                    }
                  }
                }
              } else {
                UNREACHABLE();
              }
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
              if ((instr->Bits(9, 6) == 1)) {
                if ((instr->Bit(20) == 0)) {
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
                  if (instr->Bits(19, 16) == 0xF) {
                    switch (instr->Bits(11, 10)) {
                      case 0:
                        Format(instr, "uxth'cond 'rd, 'rm");
                        break;
                      case 1:
                        Format(instr, "uxth'cond 'rd, 'rm, ror #8");
                        break;
                      case 2:
                        Format(instr, "uxth'cond 'rd, 'rm, ror #16");
                        break;
                      case 3:
                        Format(instr, "uxth'cond 'rd, 'rm, ror #24");
                        break;
                    }
                  } else {
                    switch (instr->Bits(11, 10)) {
                      case 0:
                        Format(instr, "uxtah'cond 'rd, 'rn, 'rm");
                        break;
                      case 1:
                        Format(instr, "uxtah'cond 'rd, 'rn, 'rm, ror #8");
                        break;
                      case 2:
                        Format(instr, "uxtah'cond 'rd, 'rn, 'rm, ror #16");
                        break;
                      case 3:
                        Format(instr, "uxtah'cond 'rd, 'rn, 'rm, ror #24");
                        break;
                    }
                  }
                }
              } else {
                // PU == 0b01, BW == 0b11, Bits(9, 6) != 0b0001
                if ((instr->Bits(20, 16) == 0x1F) &&
                    (instr->Bits(11, 4) == 0xF3)) {
                  Format(instr, "rbit'cond 'rd, 'rm");
                } else {
                  UNREACHABLE();
                }
              }
              break;
          }
        }
      }
      break;
    }
    case db_x: {
      if (instr->Bits(22, 20) == 0x5) {
        if (instr->Bits(7, 4) == 0x1) {
          if (instr->Bits(15, 12) == 0xF) {
            Format(instr, "smmul'cond 'rn, 'rm, 'rs");
          } else {
            // SMMLA (in V8 notation matching ARM ISA format)
            Format(instr, "smmla'cond 'rn, 'rm, 'rs, 'rd");
          }
          break;
        }
      }
      if (instr->Bits(5, 4) == 0x1) {
        if ((instr->Bit(22) == 0x0) && (instr->Bit(20) == 0x1)) {
          if (instr->Bit(21) == 0x1) {
            // UDIV (in V8 notation matching ARM ISA format) rn = rm/rs
            Format(instr, "udiv'cond'b 'rn, 'rm, 'rs");
          } else {
            // SDIV (in V8 notation matching ARM ISA format) rn = rm/rs
            Format(instr, "sdiv'cond'b 'rn, 'rm, 'rs");
          }
          break;
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
    } else {
      Format(instr, "svc'cond 'svc");
    }
  } else {
    switch (instr->CoprocessorValue()) {
      case 10:  // Fall through.
      case 11:
        DecodeTypeVFP(instr);
        break;
      case 15:
        DecodeTypeCP15(instr);
        break;
      default:
        Unknown(instr);
        break;
    }
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
// Sd = vabs(Sm)
// Dd = vneg(Dm)
// Sd = vneg(Sm)
// Dd = vadd(Dn, Dm)
// Sd = vadd(Sn, Sm)
// Dd = vsub(Dn, Dm)
// Sd = vsub(Sn, Sm)
// Dd = vmul(Dn, Dm)
// Sd = vmul(Sn, Sm)
// Dd = vmla(Dn, Dm)
// Sd = vmla(Sn, Sm)
// Dd = vmls(Dn, Dm)
// Sd = vmls(Sn, Sm)
// Dd = vdiv(Dn, Dm)
// Sd = vdiv(Sn, Sm)
// vcmp(Dd, Dm)
// vcmp(Sd, Sm)
// Dd = vsqrt(Dm)
// Sd = vsqrt(Sm)
// vmrs
// vmsr
// Qd = vdup.size(Qd, Rt)
// vmov.size: Dd[i] = Rt
// vmov.sign.size: Rt = Dn[i]
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
        if (instr->SzValue() == 0x1) {
          Format(instr, "vabs'cond.f64 'Dd, 'Dm");
        } else {
          Format(instr, "vabs'cond.f32 'Sd, 'Sm");
        }
      } else if ((instr->Opc2Value() == 0x1) && (instr->Opc3Value() == 0x1)) {
        // vneg
        if (instr->SzValue() == 0x1) {
          Format(instr, "vneg'cond.f64 'Dd, 'Dm");
        } else {
          Format(instr, "vneg'cond.f32 'Sd, 'Sm");
        }
      } else if ((instr->Opc2Value() == 0x7) && (instr->Opc3Value() == 0x3)) {
        DecodeVCVTBetweenDoubleAndSingle(instr);
      } else if ((instr->Opc2Value() == 0x8) && (instr->Opc3Value() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if ((instr->Opc2Value() == 0xA) && (instr->Opc3Value() == 0x3) &&
                 (instr->Bit(8) == 1)) {
        // vcvt.f64.s32 Dd, Dd, #<fbits>
        int fraction_bits = 32 - ((instr->Bits(3, 0) << 1) | instr->Bit(5));
        Format(instr, "vcvt'cond.f64.s32 'Dd, 'Dd");
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    ", #%d", fraction_bits);
      } else if (((instr->Opc2Value() >> 1) == 0x6) &&
                 (instr->Opc3Value() & 0x1)) {
        DecodeVCVTBetweenFloatingPointAndInteger(instr);
      } else if (((instr->Opc2Value() == 0x4) || (instr->Opc2Value() == 0x5)) &&
                 (instr->Opc3Value() & 0x1)) {
        DecodeVCMP(instr);
      } else if (((instr->Opc2Value() == 0x1)) && (instr->Opc3Value() == 0x3)) {
        if (instr->SzValue() == 0x1) {
          Format(instr, "vsqrt'cond.f64 'Dd, 'Dm");
        } else {
          Format(instr, "vsqrt'cond.f32 'Sd, 'Sm");
        }
      } else if (instr->Opc3Value() == 0x0) {
        if (instr->SzValue() == 0x1) {
          Format(instr, "vmov'cond.f64 'Dd, 'd");
        } else {
          Format(instr, "vmov'cond.f32 'Sd, 'd");
        }
      } else if (((instr->Opc2Value() == 0x6)) && instr->Opc3Value() == 0x3) {
        // vrintz - round towards zero (truncate)
        if (instr->SzValue() == 0x1) {
          Format(instr, "vrintz'cond.f64.f64 'Dd, 'Dm");
        } else {
          Format(instr, "vrintz'cond.f32.f32 'Sd, 'Sm");
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
        if (instr->Opc3Value() & 0x1) {
          Format(instr, "vsub'cond.f32 'Sd, 'Sn, 'Sm");
        } else {
          Format(instr, "vadd'cond.f32 'Sd, 'Sn, 'Sm");
        }
      }
    } else if ((instr->Opc1Value() == 0x2) && !(instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vmul'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Format(instr, "vmul'cond.f32 'Sd, 'Sn, 'Sm");
      }
    } else if ((instr->Opc1Value() == 0x0) && !(instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vmla'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Format(instr, "vmla'cond.f32 'Sd, 'Sn, 'Sm");
      }
    } else if ((instr->Opc1Value() == 0x0) && (instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vmls'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Format(instr, "vmls'cond.f32 'Sd, 'Sn, 'Sm");
      }
    } else if ((instr->Opc1Value() == 0x4) && !(instr->Opc3Value() & 0x1)) {
      if (instr->SzValue() == 0x1) {
        Format(instr, "vdiv'cond.f64 'Dd, 'Dn, 'Dm");
      } else {
        Format(instr, "vdiv'cond.f32 'Sd, 'Sn, 'Sm");
      }
    } else {
      Unknown(instr);  // Not used by V8.
    }
  } else {
    if ((instr->VCValue() == 0x0) &&
        (instr->VAValue() == 0x0)) {
      DecodeVMOVBetweenCoreAndSinglePrecisionRegisters(instr);
    } else if ((instr->VLValue() == 0x0) && (instr->VCValue() == 0x1)) {
      const char* rt_name = converter_.NameOfCPURegister(instr->RtValue());
      if (instr->Bit(23) == 0) {
        int opc1_opc2 = (instr->Bits(22, 21) << 2) | instr->Bits(6, 5);
        if ((opc1_opc2 & 0xB) == 0) {
          // NeonS32/NeonU32
          if (instr->Bit(21) == 0x0) {
            Format(instr, "vmov'cond.32 'Dd[0], 'rt");
          } else {
            Format(instr, "vmov'cond.32 'Dd[1], 'rt");
          }
        } else {
          int vd = instr->VFPNRegValue(kDoublePrecision);
          if ((opc1_opc2 & 0x8) != 0) {
            // NeonS8 / NeonU8
            int i = opc1_opc2 & 0x7;
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vmov.8 d%d[%d], %s", vd, i, rt_name);
          } else if ((opc1_opc2 & 0x1) != 0) {
            // NeonS16 / NeonU16
            int i = (opc1_opc2 >> 1) & 0x3;
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vmov.16 d%d[%d], %s", vd, i, rt_name);
          } else {
            Unknown(instr);
          }
        }
      } else {
        int size = 32;
        if (instr->Bit(5) != 0) {
          size = 16;
        } else if (instr->Bit(22) != 0) {
          size = 8;
        }
        int Vd = instr->VFPNRegValue(kSimd128Precision);
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "vdup.%i q%d, %s", size, Vd, rt_name);
      }
    } else if ((instr->VLValue() == 0x1) && (instr->VCValue() == 0x1)) {
      int opc1_opc2 = (instr->Bits(22, 21) << 2) | instr->Bits(6, 5);
      if ((opc1_opc2 & 0xB) == 0) {
        // NeonS32 / NeonU32
        if (instr->Bit(21) == 0x0) {
          Format(instr, "vmov'cond.32 'rt, 'Dd[0]");
        } else {
          Format(instr, "vmov'cond.32 'rt, 'Dd[1]");
        }
      } else {
        char sign = instr->Bit(23) != 0 ? 'u' : 's';
        const char* rt_name = converter_.NameOfCPURegister(instr->RtValue());
        int vn = instr->VFPNRegValue(kDoublePrecision);
        if ((opc1_opc2 & 0x8) != 0) {
          // NeonS8 / NeonU8
          int i = opc1_opc2 & 0x7;
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "vmov.%c8 %s, d%d[%d]",
                       sign, rt_name, vn, i);
        } else if ((opc1_opc2 & 0x1) != 0) {
          // NeonS16 / NeonU16
          int i = (opc1_opc2 >> 1) & 0x3;
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "vmov.%c16 %s, d%d[%d]",
                       sign, rt_name, vn, i);
        } else {
          Unknown(instr);
        }
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
    } else {
      Unknown(instr);  // Not used by V8.
    }
  }
}

void Decoder::DecodeTypeCP15(Instruction* instr) {
  VERIFY((instr->TypeValue() == 7) && (instr->Bit(24) == 0x0));
  VERIFY(instr->CoprocessorValue() == 15);

  if (instr->Bit(4) == 1) {
    int crn = instr->Bits(19, 16);
    int crm = instr->Bits(3, 0);
    int opc1 = instr->Bits(23, 21);
    int opc2 = instr->Bits(7, 5);
    if ((opc1 == 0) && (crn == 7)) {
      // ARMv6 memory barrier operations.
      // Details available in ARM DDI 0406C.b, B3-1750.
      if ((crm == 10) && (opc2 == 5)) {
        Format(instr, "mcr'cond (CP15DMB)");
      } else if ((crm == 10) && (opc2 == 4)) {
        Format(instr, "mcr'cond (CP15DSB)");
      } else if ((crm == 5) && (opc2 == 4)) {
        Format(instr, "mcr'cond (CP15ISB)");
      } else {
        Unknown(instr);
      }
    } else {
      Unknown(instr);
    }
  } else {
    Unknown(instr);
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
  } else if (!raise_exception_for_qnan) {
    if (instr->Opc2Value() == 0x4) {
      Format(instr, "vcmp'cond.f32 'Sd, 'Sm");
    } else if (instr->Opc2Value() == 0x5) {
      Format(instr, "vcmp'cond.f32 'Sd, #0.0");
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


static const char* const barrier_option_names[] = {
    "invalid", "oshld", "oshst", "osh", "invalid", "nshld", "nshst", "nsh",
    "invalid", "ishld", "ishst", "ish", "invalid", "ld",    "st",    "sy",
};


void Decoder::DecodeSpecialCondition(Instruction* instr) {
  switch (instr->SpecialValue()) {
    case 4: {
      int Vd, Vm, Vn;
      if (instr->Bit(6) == 0) {
        Vd = instr->VFPDRegValue(kDoublePrecision);
        Vm = instr->VFPMRegValue(kDoublePrecision);
        Vn = instr->VFPNRegValue(kDoublePrecision);
      } else {
        Vd = instr->VFPDRegValue(kSimd128Precision);
        Vm = instr->VFPMRegValue(kSimd128Precision);
        Vn = instr->VFPNRegValue(kSimd128Precision);
      }
      int size = kBitsPerByte * (1 << instr->Bits(21, 20));
      switch (instr->Bits(11, 8)) {
        case 0x0: {
          if (instr->Bit(4) == 1) {
            // vqadd.s<size> Qd, Qm, Qn.
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_,
                         "vqadd.s%d q%d, q%d, q%d", size, Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0x1: {
          if (instr->Bits(21, 20) == 2 && instr->Bit(6) == 1 &&
              instr->Bit(4) == 1) {
            if (Vm == Vn) {
              // vmov Qd, Qm
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "vmov q%d, q%d", Vd, Vm);
            } else {
              // vorr Qd, Qm, Qn.
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "vorr q%d, q%d, q%d", Vd, Vn, Vm);
            }
          } else if (instr->Bits(21, 20) == 0 && instr->Bit(6) == 1 &&
                     instr->Bit(4) == 1) {
            // vand Qd, Qm, Qn.
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vand q%d, q%d, q%d", Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0x2: {
          if (instr->Bit(4) == 1) {
            // vqsub.s<size> Qd, Qm, Qn.
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_,
                         "vqsub.s%d q%d, q%d, q%d", size, Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0x3: {
          const char* op = (instr->Bit(4) == 1) ? "vcge" : "vcgt";
          // vcge/vcgt.s<size> Qd, Qm, Qn.
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s.s%d q%d, q%d, q%d",
                       op, size, Vd, Vn, Vm);
          break;
        }
        case 0x6: {
          // vmin/vmax.s<size> Qd, Qm, Qn.
          const char* op = instr->Bit(4) == 1 ? "vmin" : "vmax";
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s.s%d q%d, q%d, q%d",
                       op, size, Vd, Vn, Vm);
          break;
        }
        case 0x8: {
          const char* op = (instr->Bit(4) == 0) ? "vadd" : "vtst";
          // vadd/vtst.i<size> Qd, Qm, Qn.
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s.i%d q%d, q%d, q%d",
                       op, size, Vd, Vn, Vm);
          break;
        }
        case 0x9: {
          if (instr->Bit(6) == 1 && instr->Bit(4) == 1) {
            // vmul.i<size> Qd, Qm, Qn.
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_,
                         "vmul.i%d q%d, q%d, q%d", size, Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0xA: {
          // vpmin/vpmax.s<size> Dd, Dm, Dn.
          const char* op = instr->Bit(4) == 1 ? "vpmin" : "vpmax";
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s.s%d d%d, d%d, d%d",
                       op, size, Vd, Vn, Vm);
          break;
        }
        case 0xB: {
          // vpadd.i<size> Dd, Dm, Dn.
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "vpadd.i%d d%d, d%d, d%d",
                       size, Vd, Vn, Vm);
          break;
        }
        case 0xD: {
          if (instr->Bit(4) == 0) {
            const char* op = (instr->Bits(21, 20) == 0) ? "vadd" : "vsub";
            // vadd/vsub.f32 Qd, Qm, Qn.
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "%s.f32 q%d, q%d, q%d", op, Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0xE: {
          if (instr->Bits(21, 20) == 0 && instr->Bit(4) == 0) {
            // vceq.f32 Qd, Qm, Qn.
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vceq.f32 q%d, q%d, q%d", Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0xF: {
          if (instr->Bit(20) == 0 && instr->Bit(6) == 1) {
            if (instr->Bit(4) == 1) {
              // vrecps/vrsqrts.f32 Qd, Qm, Qn.
              const char* op = instr->Bit(21) == 0 ? "vrecps" : "vrsqrts";
              out_buffer_pos_ +=
                  SNPrintF(out_buffer_ + out_buffer_pos_,
                           "%s.f32 q%d, q%d, q%d", op, Vd, Vn, Vm);
            } else {
              // vmin/max.f32 Qd, Qm, Qn.
              const char* op = instr->Bit(21) == 1 ? "vmin" : "vmax";
              out_buffer_pos_ +=
                  SNPrintF(out_buffer_ + out_buffer_pos_,
                           "%s.f32 q%d, q%d, q%d", op, Vd, Vn, Vm);
            }
          } else {
            Unknown(instr);
          }
          break;
        }
        default:
          Unknown(instr);
          break;
      }
      break;
    }
    case 5:
      if ((instr->Bits(18, 16) == 0) && (instr->Bits(11, 6) == 0x28) &&
          (instr->Bit(4) == 1)) {
        // vmovl signed
        if ((instr->VdValue() & 1) != 0) Unknown(instr);
        int Vd = (instr->Bit(22) << 3) | (instr->VdValue() >> 1);
        int Vm = (instr->Bit(5) << 4) | instr->VmValue();
        int imm3 = instr->Bits(21, 19);
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "vmovl.s%d q%d, d%d", imm3 * 8, Vd, Vm);
      } else if (instr->Bits(21, 20) == 3 && instr->Bit(4) == 0) {
        // vext.8 Qd, Qm, Qn, imm4
        int imm4 = instr->Bits(11, 8);
        int Vd = instr->VFPDRegValue(kSimd128Precision);
        int Vm = instr->VFPMRegValue(kSimd128Precision);
        int Vn = instr->VFPNRegValue(kSimd128Precision);
        out_buffer_pos_ +=
            SNPrintF(out_buffer_ + out_buffer_pos_, "vext.8 q%d, q%d, q%d, #%d",
                     Vd, Vn, Vm, imm4);
      } else if (instr->Bits(11, 7) == 0xA && instr->Bit(4) == 1) {
        // vshl.i<size> Qd, Qm, shift
        int size = base::bits::RoundDownToPowerOfTwo32(instr->Bits(21, 16));
        int shift = instr->Bits(21, 16) - size;
        int Vd = instr->VFPDRegValue(kSimd128Precision);
        int Vm = instr->VFPMRegValue(kSimd128Precision);
        out_buffer_pos_ +=
            SNPrintF(out_buffer_ + out_buffer_pos_, "vshl.i%d q%d, q%d, #%d",
                     size, Vd, Vm, shift);
      } else if (instr->Bits(11, 7) == 0 && instr->Bit(4) == 1) {
        // vshr.s<size> Qd, Qm, shift
        int size = base::bits::RoundDownToPowerOfTwo32(instr->Bits(21, 16));
        int shift = 2 * size - instr->Bits(21, 16);
        int Vd = instr->VFPDRegValue(kSimd128Precision);
        int Vm = instr->VFPMRegValue(kSimd128Precision);
        out_buffer_pos_ +=
            SNPrintF(out_buffer_ + out_buffer_pos_, "vshr.s%d q%d, q%d, #%d",
                     size, Vd, Vm, shift);
      } else {
        Unknown(instr);
      }
      break;
    case 6: {
      int Vd, Vm, Vn;
      if (instr->Bit(6) == 0) {
        Vd = instr->VFPDRegValue(kDoublePrecision);
        Vm = instr->VFPMRegValue(kDoublePrecision);
        Vn = instr->VFPNRegValue(kDoublePrecision);
      } else {
        Vd = instr->VFPDRegValue(kSimd128Precision);
        Vm = instr->VFPMRegValue(kSimd128Precision);
        Vn = instr->VFPNRegValue(kSimd128Precision);
      }
      int size = kBitsPerByte * (1 << instr->Bits(21, 20));
      switch (instr->Bits(11, 8)) {
        case 0x0: {
          if (instr->Bit(4) == 1) {
            // vqadd.u<size> Qd, Qm, Qn.
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_,
                         "vqadd.u%d q%d, q%d, q%d", size, Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0x1: {
          if (instr->Bits(21, 20) == 1 && instr->Bit(4) == 1) {
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vbsl q%d, q%d, q%d", Vd, Vn, Vm);
          } else if (instr->Bits(21, 20) == 0 && instr->Bit(4) == 1) {
            if (instr->Bit(6) == 0) {
              // veor Dd, Dn, Dm
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "veor d%d, d%d, d%d", Vd, Vn, Vm);

            } else {
              // veor Qd, Qn, Qm
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "veor q%d, q%d, q%d", Vd, Vn, Vm);
            }
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0x2: {
          if (instr->Bit(4) == 1) {
            // vqsub.u<size> Qd, Qm, Qn.
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_,
                         "vqsub.u%d q%d, q%d, q%d", size, Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0x3: {
          const char* op = (instr->Bit(4) == 1) ? "vcge" : "vcgt";
          // vcge/vcgt.u<size> Qd, Qm, Qn.
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s.u%d q%d, q%d, q%d",
                       op, size, Vd, Vn, Vm);
          break;
        }
        case 0x6: {
          // vmin/vmax.u<size> Qd, Qm, Qn.
          const char* op = instr->Bit(4) == 1 ? "vmin" : "vmax";
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s.u%d q%d, q%d, q%d",
                       op, size, Vd, Vn, Vm);
          break;
        }
        case 0x8: {
          if (instr->Bit(4) == 0) {
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_,
                         "vsub.i%d q%d, q%d, q%d", size, Vd, Vn, Vm);
          } else {
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_,
                         "vceq.i%d q%d, q%d, q%d", size, Vd, Vn, Vm);
          }
          break;
        }
        case 0xA: {
          // vpmin/vpmax.u<size> Dd, Dm, Dn.
          const char* op = instr->Bit(4) == 1 ? "vpmin" : "vpmax";
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s.u%d d%d, d%d, d%d",
                       op, size, Vd, Vn, Vm);
          break;
        }
        case 0xD: {
          if (instr->Bits(21, 20) == 0 && instr->Bit(6) == 1 &&
              instr->Bit(4) == 1) {
            // vmul.f32 Qd, Qm, Qn
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vmul.f32 q%d, q%d, q%d", Vd, Vn, Vm);
          } else if (instr->Bits(21, 20) == 0 && instr->Bit(6) == 0 &&
                     instr->Bit(4) == 0) {
            // vpadd.f32 Dd, Dm, Dn.
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vpadd.f32 d%d, d%d, d%d", Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        case 0xE: {
          if (instr->Bit(20) == 0 && instr->Bit(4) == 0) {
            const char* op = (instr->Bit(21) == 0) ? "vcge" : "vcgt";
            // vcge/vcgt.f32 Qd, Qm, Qn.
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "%s.f32 q%d, q%d, q%d", op, Vd, Vn, Vm);
          } else {
            Unknown(instr);
          }
          break;
        }
        default:
          Unknown(instr);
          break;
      }
      break;
    }
    case 7:
      if ((instr->Bits(18, 16) == 0) && (instr->Bits(11, 6) == 0x28) &&
          (instr->Bit(4) == 1)) {
        // vmovl unsigned
        if ((instr->VdValue() & 1) != 0) Unknown(instr);
        int Vd = (instr->Bit(22) << 3) | (instr->VdValue() >> 1);
        int Vm = (instr->Bit(5) << 4) | instr->VmValue();
        int imm3 = instr->Bits(21, 19);
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "vmovl.u%d q%d, d%d", imm3 * 8, Vd, Vm);
      } else if (instr->Opc1Value() == 7 && instr->Bit(4) == 0) {
        if (instr->Bits(11, 7) == 0x18) {
          int Vm = instr->VFPMRegValue(kDoublePrecision);
          int imm4 = instr->Bits(19, 16);
          int size = 0, index = 0;
          if ((imm4 & 0x1) != 0) {
            size = 8;
            index = imm4 >> 1;
          } else if ((imm4 & 0x2) != 0) {
            size = 16;
            index = imm4 >> 2;
          } else {
            size = 32;
            index = imm4 >> 3;
          }
          if (instr->Bit(6) == 0) {
            int Vd = instr->VFPDRegValue(kDoublePrecision);
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_, "vdup.%i d%d, d%d[%d]",
                         size, Vd, Vm, index);
          } else {
            int Vd = instr->VFPDRegValue(kSimd128Precision);
            out_buffer_pos_ +=
                SNPrintF(out_buffer_ + out_buffer_pos_, "vdup.%i q%d, d%d[%d]",
                         size, Vd, Vm, index);
          }
        } else if (instr->Bits(11, 10) == 0x2) {
          int Vd = instr->VFPDRegValue(kDoublePrecision);
          int Vn = instr->VFPNRegValue(kDoublePrecision);
          int Vm = instr->VFPMRegValue(kDoublePrecision);
          int len = instr->Bits(9, 8);
          NeonListOperand list(DwVfpRegister::from_code(Vn), len + 1);
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "%s d%d, ",
                       instr->Bit(6) == 0 ? "vtbl.8" : "vtbx.8", Vd);
          FormatNeonList(Vn, list.type());
          Print(", ");
          PrintDRegister(Vm);
        } else if (instr->Bits(17, 16) == 0x2 && instr->Bits(11, 8) == 0x2 &&
                   instr->Bits(7, 6) != 0) {
          // vqmovn.<type><size> Dd, Qm.
          int Vd = instr->VFPDRegValue(kDoublePrecision);
          int Vm = instr->VFPMRegValue(kSimd128Precision);
          char type = instr->Bit(6) != 0 ? 'u' : 's';
          int size = 2 * kBitsPerByte * (1 << instr->Bits(19, 18));
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "vqmovn.%c%i d%d, q%d",
                       type, size, Vd, Vm);
        } else {
          int Vd, Vm;
          if (instr->Bit(6) == 0) {
            Vd = instr->VFPDRegValue(kDoublePrecision);
            Vm = instr->VFPMRegValue(kDoublePrecision);
          } else {
            Vd = instr->VFPDRegValue(kSimd128Precision);
            Vm = instr->VFPMRegValue(kSimd128Precision);
          }
          if (instr->Bits(17, 16) == 0x2 && instr->Bits(11, 7) == 0) {
            if (instr->Bit(6) == 0) {
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "vswp d%d, d%d", Vd, Vm);
            } else {
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "vswp q%d, q%d", Vd, Vm);
            }
          } else if (instr->Bits(19, 16) == 0 && instr->Bits(11, 6) == 0x17) {
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vmvn q%d, q%d", Vd, Vm);
          } else if (instr->Bits(19, 16) == 0xB && instr->Bits(11, 9) == 0x3 &&
                     instr->Bit(6) == 1) {
            const char* suffix = nullptr;
            int op = instr->Bits(8, 7);
            switch (op) {
              case 0:
                suffix = "f32.s32";
                break;
              case 1:
                suffix = "f32.u32";
                break;
              case 2:
                suffix = "s32.f32";
                break;
              case 3:
                suffix = "u32.f32";
                break;
            }
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vcvt.%s q%d, q%d", suffix, Vd, Vm);
          } else if (instr->Bits(17, 16) == 0x2 && instr->Bits(11, 8) == 0x1) {
            int size = kBitsPerByte * (1 << instr->Bits(19, 18));
            const char* op = instr->Bit(7) != 0 ? "vzip" : "vuzp";
            if (instr->Bit(6) == 0) {
              // vzip/vuzp.<size> Dd, Dm.
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "%s.%d d%d, d%d", op, size, Vd, Vm);
            } else {
              // vzip/vuzp.<size> Qd, Qm.
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "%s.%d q%d, q%d", op, size, Vd, Vm);
            }
          } else if (instr->Bits(17, 16) == 0 && instr->Bits(11, 9) == 0 &&
                     instr->Bit(6) == 1) {
            int size = kBitsPerByte * (1 << instr->Bits(19, 18));
            int op = kBitsPerByte
                     << (static_cast<int>(Neon64) - instr->Bits(8, 7));
            // vrev<op>.<size> Qd, Qm.
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "vrev%d.%d q%d, q%d", op, size, Vd, Vm);
          } else if (instr->Bits(17, 16) == 0x2 && instr->Bits(11, 7) == 0x1) {
            int size = kBitsPerByte * (1 << instr->Bits(19, 18));
            if (instr->Bit(6) == 0) {
              // vtrn.<size> Dd, Dm.
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "vtrn.%d d%d, d%d", size, Vd, Vm);
            } else {
              // vtrn.<size> Qd, Qm.
              out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                          "vtrn.%d q%d, q%d", size, Vd, Vm);
            }
          } else if (instr->Bits(17, 16) == 0x1 && instr->Bit(11) == 0 &&
                     instr->Bit(6) == 1) {
            int size = kBitsPerByte * (1 << instr->Bits(19, 18));
            char type = instr->Bit(10) != 0 ? 'f' : 's';
            if (instr->Bits(9, 6) == 0xD) {
              // vabs<type>.<size> Qd, Qm.
              out_buffer_pos_ +=
                  SNPrintF(out_buffer_ + out_buffer_pos_, "vabs.%c%d q%d, q%d",
                           type, size, Vd, Vm);
            } else if (instr->Bits(9, 6) == 0xF) {
              // vneg<type>.<size> Qd, Qm.
              out_buffer_pos_ +=
                  SNPrintF(out_buffer_ + out_buffer_pos_, "vneg.%c%d q%d, q%d",
                           type, size, Vd, Vm);
            } else {
              Unknown(instr);
            }
          } else if (instr->Bits(19, 18) == 0x2 && instr->Bits(11, 8) == 0x5 &&
                     instr->Bit(6) == 1) {
            // vrecpe/vrsqrte.f32 Qd, Qm.
            const char* op = instr->Bit(7) == 0 ? "vrecpe" : "vrsqrte";
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                        "%s.f32 q%d, q%d", op, Vd, Vm);
          } else {
            Unknown(instr);
          }
        }
      } else if (instr->Bits(11, 7) == 0 && instr->Bit(4) == 1 &&
                 instr->Bit(6) == 1) {
        // vshr.u<size> Qd, Qm, shift
        int size = base::bits::RoundDownToPowerOfTwo32(instr->Bits(21, 16));
        int shift = 2 * size - instr->Bits(21, 16);
        int Vd = instr->VFPDRegValue(kSimd128Precision);
        int Vm = instr->VFPMRegValue(kSimd128Precision);
        out_buffer_pos_ +=
            SNPrintF(out_buffer_ + out_buffer_pos_, "vshr.u%d q%d, q%d, #%d",
                     size, Vd, Vm, shift);
      } else if (instr->Bit(10) == 1 && instr->Bit(6) == 0 &&
                 instr->Bit(4) == 1) {
        // vsli.<size> Dd, Dm, shift
        // vsri.<size> Dd, Dm, shift
        int imm7 = instr->Bits(21, 16);
        if (instr->Bit(7) != 0) imm7 += 64;
        int size = base::bits::RoundDownToPowerOfTwo32(imm7);
        int shift;
        char direction;
        if (instr->Bit(8) == 1) {
          shift = imm7 - size;
          direction = 'l';  // vsli
        } else {
          shift = 2 * size - imm7;
          direction = 'r';  // vsri
        }
        int Vd = instr->VFPDRegValue(kDoublePrecision);
        int Vm = instr->VFPMRegValue(kDoublePrecision);
        out_buffer_pos_ +=
            SNPrintF(out_buffer_ + out_buffer_pos_, "vs%ci.%d d%d, d%d, #%d",
                     direction, size, Vd, Vm, shift);
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
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "vst1.%d ",
                                    (1 << size) << 3);
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
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "vld1.%d ",
                                    (1 << size) << 3);
        FormatNeonList(Vd, type);
        Print(", ");
        FormatNeonMemory(Rn, align, Rm);
      } else {
        Unknown(instr);
      }
      break;
    case 0xA:
    case 0xB:
      if ((instr->Bits(22, 20) == 5) && (instr->Bits(15, 12) == 0xF)) {
        const char* rn_name = converter_.NameOfCPURegister(instr->Bits(19, 16));
        int offset = instr->Bits(11, 0);
        if (offset == 0) {
          out_buffer_pos_ +=
              SNPrintF(out_buffer_ + out_buffer_pos_, "pld [%s]", rn_name);
        } else if (instr->Bit(23) == 0) {
          out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "pld [%s, #-%d]", rn_name, offset);
        } else {
          out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                      "pld [%s, #+%d]", rn_name, offset);
        }
      } else if (instr->SpecialValue() == 0xA && instr->Bits(22, 20) == 7) {
        int option = instr->Bits(3, 0);
        switch (instr->Bits(7, 4)) {
          case 4:
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "dsb %s",
                                        barrier_option_names[option]);
            break;
          case 5:
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "dmb %s",
                                        barrier_option_names[option]);
            break;
          case 6:
            out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "isb %s",
                                        barrier_option_names[option]);
            break;
          default:
            Unknown(instr);
        }
      } else {
        Unknown(instr);
      }
      break;
    case 0x1D:
      if (instr->Opc1Value() == 0x7 && instr->Bits(19, 18) == 0x2 &&
          instr->Bits(11, 9) == 0x5 && instr->Bits(7, 6) == 0x1 &&
          instr->Bit(4) == 0x0) {
        // VRINTA, VRINTN, VRINTP, VRINTM (floating-point)
        bool dp_operation = (instr->SzValue() == 1);
        int rounding_mode = instr->Bits(17, 16);
        switch (rounding_mode) {
          case 0x0:
            if (dp_operation) {
              Format(instr, "vrinta.f64.f64 'Dd, 'Dm");
            } else {
              Format(instr, "vrinta.f32.f32 'Sd, 'Sm");
            }
            break;
          case 0x1:
            if (dp_operation) {
              Format(instr, "vrintn.f64.f64 'Dd, 'Dm");
            } else {
              Format(instr, "vrintn.f32.f32 'Sd, 'Sm");
            }
            break;
          case 0x2:
            if (dp_operation) {
              Format(instr, "vrintp.f64.f64 'Dd, 'Dm");
            } else {
              Format(instr, "vrintp.f32.f32 'Sd, 'Sm");
            }
            break;
          case 0x3:
            if (dp_operation) {
              Format(instr, "vrintm.f64.f64 'Dd, 'Dm");
            } else {
              Format(instr, "vrintm.f32.f32 'Sd, 'Sm");
            }
            break;
          default:
            UNREACHABLE();  // Case analysis is exhaustive.
            break;
        }
      } else if ((instr->Opc1Value() == 0x4) && (instr->Bits(11, 9) == 0x5) &&
                 (instr->Bit(4) == 0x0)) {
        // VMAXNM, VMINNM (floating-point)
        if (instr->SzValue() == 0x1) {
          if (instr->Bit(6) == 0x1) {
            Format(instr, "vminnm.f64 'Dd, 'Dn, 'Dm");
          } else {
            Format(instr, "vmaxnm.f64 'Dd, 'Dn, 'Dm");
          }
        } else {
          if (instr->Bit(6) == 0x1) {
            Format(instr, "vminnm.f32 'Sd, 'Sn, 'Sm");
          } else {
            Format(instr, "vmaxnm.f32 'Sd, 'Sn, 'Sm");
          }
        }
      } else {
        Unknown(instr);
      }
      break;
    case 0x1C:
      if ((instr->Bits(11, 9) == 0x5) && (instr->Bit(6) == 0) &&
          (instr->Bit(4) == 0)) {
        // VSEL* (floating-point)
        bool dp_operation = (instr->SzValue() == 1);
        switch (instr->Bits(21, 20)) {
          case 0x0:
            if (dp_operation) {
              Format(instr, "vseleq.f64 'Dd, 'Dn, 'Dm");
            } else {
              Format(instr, "vseleq.f32 'Sd, 'Sn, 'Sm");
            }
            break;
          case 0x1:
            if (dp_operation) {
              Format(instr, "vselvs.f64 'Dd, 'Dn, 'Dm");
            } else {
              Format(instr, "vselvs.f32 'Sd, 'Sn, 'Sm");
            }
            break;
          case 0x2:
            if (dp_operation) {
              Format(instr, "vselge.f64 'Dd, 'Dn, 'Dm");
            } else {
              Format(instr, "vselge.f32 'Sd, 'Sn, 'Sm");
            }
            break;
          case 0x3:
            if (dp_operation) {
              Format(instr, "vselgt.f64 'Dd, 'Dn, 'Dm");
            } else {
              Format(instr, "vselgt.f32 'Sd, 'Sn, 'Sm");
            }
            break;
          default:
            UNREACHABLE();  // Case analysis is exhaustive.
            break;
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
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                              "%08x       ",
                              instr->InstructionBits());
  if (instr->ConditionField() == kSpecialCondition) {
    DecodeSpecialCondition(instr);
    return Instruction::kInstrSize;
  }
  int instruction_bits = *(reinterpret_cast<int*>(instr_ptr));
  if ((instruction_bits & kConstantPoolMarkerMask) == kConstantPoolMarker) {
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_,
                                "constant pool begin (length %d)",
                                DecodeConstantPoolLength(instruction_bits));
    return Instruction::kInstrSize;
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


}  // namespace internal
}  // namespace v8


//------------------------------------------------------------------------------

namespace disasm {


const char* NameConverter::NameOfAddress(byte* addr) const {
  v8::internal::SNPrintF(tmp_buffer_, "%p", static_cast<void*>(addr));
  return tmp_buffer_.start();
}


const char* NameConverter::NameOfConstant(byte* addr) const {
  return NameOfAddress(addr);
}


const char* NameConverter::NameOfCPURegister(int reg) const {
  return v8::internal::GetRegConfig()->GetGeneralRegisterName(reg);
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
    v8::internal::PrintF(f, "%p    %08x      %s\n", static_cast<void*>(prev_pc),
                         *reinterpret_cast<int32_t*>(prev_pc), buffer.start());
  }
}


}  // namespace disasm

#endif  // V8_TARGET_ARCH_ARM

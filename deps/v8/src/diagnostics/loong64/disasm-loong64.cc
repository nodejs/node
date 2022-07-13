// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if V8_TARGET_ARCH_LOONG64

#include "src/base/platform/platform.h"
#include "src/base/strings.h"
#include "src/base/vector.h"
#include "src/codegen/loong64/constants-loong64.h"
#include "src/codegen/macro-assembler.h"
#include "src/diagnostics/disasm.h"

namespace v8 {
namespace internal {

//------------------------------------------------------------------------------

// Decoder decodes and disassembles instructions into an output buffer.
// It uses the converter to convert register names and call destinations into
// more informative description.
class Decoder {
 public:
  Decoder(const disasm::NameConverter& converter,
          v8::base::Vector<char> out_buffer)
      : converter_(converter), out_buffer_(out_buffer), out_buffer_pos_(0) {
    out_buffer_[out_buffer_pos_] = '\0';
  }

  ~Decoder() {}

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

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
  void PrintFPUStatusRegister(int freg);
  void PrintRj(Instruction* instr);
  void PrintRk(Instruction* instr);
  void PrintRd(Instruction* instr);
  void PrintFj(Instruction* instr);
  void PrintFk(Instruction* instr);
  void PrintFd(Instruction* instr);
  void PrintFa(Instruction* instr);
  void PrintSa2(Instruction* instr);
  void PrintSa3(Instruction* instr);
  void PrintUi5(Instruction* instr);
  void PrintUi6(Instruction* instr);
  void PrintUi12(Instruction* instr);
  void PrintMsbw(Instruction* instr);
  void PrintLsbw(Instruction* instr);
  void PrintMsbd(Instruction* instr);
  void PrintLsbd(Instruction* instr);
  //  void PrintCond(Instruction* instr);
  void PrintSi12(Instruction* instr);
  void PrintSi14(Instruction* instr);
  void PrintSi16(Instruction* instr);
  void PrintSi20(Instruction* instr);
  void PrintXi12(Instruction* instr);
  void PrintXi20(Instruction* instr);
  void PrintCj(Instruction* instr);
  void PrintCd(Instruction* instr);
  void PrintCa(Instruction* instr);
  void PrintCode(Instruction* instr);
  void PrintHint5(Instruction* instr);
  void PrintHint15(Instruction* instr);
  void PrintPCOffs16(Instruction* instr);
  void PrintPCOffs21(Instruction* instr);
  void PrintPCOffs26(Instruction* instr);
  void PrintOffs16(Instruction* instr);
  void PrintOffs21(Instruction* instr);
  void PrintOffs26(Instruction* instr);

  // Handle formatting of instructions and their options.
  int FormatRegister(Instruction* instr, const char* option);
  int FormatFPURegister(Instruction* instr, const char* option);
  int FormatOption(Instruction* instr, const char* option);
  void Format(Instruction* instr, const char* format);
  void Unknown(Instruction* instr);
  int DecodeBreakInstr(Instruction* instr);

  // Each of these functions decodes one particular instruction type.
  int InstructionDecode(Instruction* instr);
  void DecodeTypekOp6(Instruction* instr);
  void DecodeTypekOp7(Instruction* instr);
  void DecodeTypekOp8(Instruction* instr);
  void DecodeTypekOp10(Instruction* instr);
  void DecodeTypekOp12(Instruction* instr);
  void DecodeTypekOp14(Instruction* instr);
  int DecodeTypekOp17(Instruction* instr);
  void DecodeTypekOp22(Instruction* instr);

  const disasm::NameConverter& converter_;
  v8::base::Vector<char> out_buffer_;
  int out_buffer_pos_;
};

// Support for assertions in the Decoder formatting functions.
#define STRING_STARTS_WITH(string, compare_string) \
  (strncmp(string, compare_string, strlen(compare_string)) == 0)

// Append the ch to the output buffer.
void Decoder::PrintChar(const char ch) { out_buffer_[out_buffer_pos_++] = ch; }

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

void Decoder::PrintRj(Instruction* instr) {
  int reg = instr->RjValue();
  PrintRegister(reg);
}

void Decoder::PrintRk(Instruction* instr) {
  int reg = instr->RkValue();
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

void Decoder::PrintFj(Instruction* instr) {
  int freg = instr->FjValue();
  PrintFPURegister(freg);
}

void Decoder::PrintFk(Instruction* instr) {
  int freg = instr->FkValue();
  PrintFPURegister(freg);
}

void Decoder::PrintFd(Instruction* instr) {
  int freg = instr->FdValue();
  PrintFPURegister(freg);
}

void Decoder::PrintFa(Instruction* instr) {
  int freg = instr->FaValue();
  PrintFPURegister(freg);
}

// Print the integer value of the sa field.
void Decoder::PrintSa2(Instruction* instr) {
  int sa = instr->Sa2Value();
  uint32_t opcode = (instr->InstructionBits() >> 18) << 18;
  if (opcode == ALSL || opcode == ALSL_D) {
    sa += 1;
  }
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", sa);
}

void Decoder::PrintSa3(Instruction* instr) {
  int sa = instr->Sa3Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d", sa);
}

void Decoder::PrintUi5(Instruction* instr) {
  int ui = instr->Ui5Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi6(Instruction* instr) {
  int ui = instr->Ui6Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi12(Instruction* instr) {
  int ui = instr->Ui12Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintXi12(Instruction* instr) {
  int xi = instr->Ui12Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", xi);
}

void Decoder::PrintXi20(Instruction* instr) {
  int xi = instr->Si20Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", xi);
}

void Decoder::PrintMsbd(Instruction* instr) {
  int msbd = instr->MsbdValue();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", msbd);
}

void Decoder::PrintLsbd(Instruction* instr) {
  int lsbd = instr->LsbdValue();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", lsbd);
}

void Decoder::PrintMsbw(Instruction* instr) {
  int msbw = instr->MsbwValue();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", msbw);
}

void Decoder::PrintLsbw(Instruction* instr) {
  int lsbw = instr->LsbwValue();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", lsbw);
}

void Decoder::PrintSi12(Instruction* instr) {
  int si = ((instr->Si12Value()) << (32 - kSi12Bits)) >> (32 - kSi12Bits);
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si12Value());
}

void Decoder::PrintSi14(Instruction* instr) {
  int si = ((instr->Si14Value()) << (32 - kSi14Bits)) >> (32 - kSi14Bits);
  si <<= 2;
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si14Value() << 2);
}

void Decoder::PrintSi16(Instruction* instr) {
  int si = ((instr->Si16Value()) << (32 - kSi16Bits)) >> (32 - kSi16Bits);
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si16Value());
}

void Decoder::PrintSi20(Instruction* instr) {
  int si = ((instr->Si20Value()) << (32 - kSi20Bits)) >> (32 - kSi20Bits);
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si20Value());
}

void Decoder::PrintCj(Instruction* instr) {
  int cj = instr->CjValue();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", cj);
}

void Decoder::PrintCd(Instruction* instr) {
  int cd = instr->CdValue();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", cd);
}

void Decoder::PrintCa(Instruction* instr) {
  int ca = instr->CaValue();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ca);
}

void Decoder::PrintCode(Instruction* instr) {
  int code = instr->CodeValue();
  out_buffer_pos_ +=
      base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x(%u)", code, code);
}

void Decoder::PrintHint5(Instruction* instr) {
  int hint = instr->Hint5Value();
  out_buffer_pos_ +=
      base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x(%u)", hint, hint);
}

void Decoder::PrintHint15(Instruction* instr) {
  int hint = instr->Hint15Value();
  out_buffer_pos_ +=
      base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x(%u)", hint, hint);
}

void Decoder::PrintPCOffs16(Instruction* instr) {
  int n_bits = 2;
  int offs = instr->Offs16Value();
  int target = ((offs << n_bits) << (32 - kOffsLowBits - n_bits)) >>
               (32 - kOffsLowBits - n_bits);
  out_buffer_pos_ += base::SNPrintF(
      out_buffer_ + out_buffer_pos_, "%s",
      converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + target));
}

void Decoder::PrintPCOffs21(Instruction* instr) {
  int n_bits = 2;
  int offs = instr->Offs21Value();
  int target =
      ((offs << n_bits) << (32 - kOffsLowBits - kOffs21HighBits - n_bits)) >>
      (32 - kOffsLowBits - kOffs21HighBits - n_bits);
  out_buffer_pos_ += base::SNPrintF(
      out_buffer_ + out_buffer_pos_, "%s",
      converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + target));
}

void Decoder::PrintPCOffs26(Instruction* instr) {
  int n_bits = 2;
  int offs = instr->Offs26Value();
  int target =
      ((offs << n_bits) << (32 - kOffsLowBits - kOffs26HighBits - n_bits)) >>
      (32 - kOffsLowBits - kOffs26HighBits - n_bits);
  out_buffer_pos_ += base::SNPrintF(
      out_buffer_ + out_buffer_pos_, "%s",
      converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + target));
}

void Decoder::PrintOffs16(Instruction* instr) {
  int offs = instr->Offs16Value();
  out_buffer_pos_ +=
      base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", offs << 2);
}

void Decoder::PrintOffs21(Instruction* instr) {
  int offs = instr->Offs21Value();
  out_buffer_pos_ +=
      base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", offs << 2);
}

void Decoder::PrintOffs26(Instruction* instr) {
  int offs = instr->Offs26Value();
  out_buffer_pos_ +=
      base::SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", offs << 2);
}

// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instruction* instr, const char* format) {
  DCHECK_EQ(format[0], 'r');
  if (format[1] == 'j') {  // 'rj: Rj register.
    int reg = instr->RjValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'k') {  // 'rk: rk register.
    int reg = instr->RkValue();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'rd: rd register.
    int reg = instr->RdValue();
    PrintRegister(reg);
    return 2;
  }
  UNREACHABLE();
}

// Handle all FPUregister based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatFPURegister(Instruction* instr, const char* format) {
  DCHECK_EQ(format[0], 'f');
  if (format[1] == 'j') {  // 'fj: fj register.
    int reg = instr->FjValue();
    PrintFPURegister(reg);
    return 2;
  } else if (format[1] == 'k') {  // 'fk: fk register.
    int reg = instr->FkValue();
    PrintFPURegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'fd: fd register.
    int reg = instr->FdValue();
    PrintFPURegister(reg);
    return 2;
  } else if (format[1] == 'a') {  // 'fa: fa register.
    int reg = instr->FaValue();
    PrintFPURegister(reg);
    return 2;
  }
  UNREACHABLE();
}

// FormatOption takes a formatting string and interprets it based on
// the current instructions. The format string points to the first
// character of the option string (the option escape has already been
// consumed by the caller.)  FormatOption returns the number of
// characters that were consumed from the formatting string.
int Decoder::FormatOption(Instruction* instr, const char* format) {
  switch (format[0]) {
    case 'c': {
      switch (format[1]) {
        case 'a':
          DCHECK(STRING_STARTS_WITH(format, "ca"));
          PrintCa(instr);
          return 2;
        case 'd':
          DCHECK(STRING_STARTS_WITH(format, "cd"));
          PrintCd(instr);
          return 2;
        case 'j':
          DCHECK(STRING_STARTS_WITH(format, "cj"));
          PrintCj(instr);
          return 2;
        case 'o':
          DCHECK(STRING_STARTS_WITH(format, "code"));
          PrintCode(instr);
          return 4;
      }
    }
    case 'f': {
      return FormatFPURegister(instr, format);
    }
    case 'h': {
      if (format[4] == '5') {
        DCHECK(STRING_STARTS_WITH(format, "hint5"));
        PrintHint5(instr);
        return 5;
      } else if (format[4] == '1') {
        DCHECK(STRING_STARTS_WITH(format, "hint15"));
        PrintHint15(instr);
        return 6;
      }
      break;
    }
    case 'l': {
      switch (format[3]) {
        case 'w':
          DCHECK(STRING_STARTS_WITH(format, "lsbw"));
          PrintLsbw(instr);
          return 4;
        case 'd':
          DCHECK(STRING_STARTS_WITH(format, "lsbd"));
          PrintLsbd(instr);
          return 4;
        default:
          return 0;
      }
    }
    case 'm': {
      if (format[3] == 'w') {
        DCHECK(STRING_STARTS_WITH(format, "msbw"));
        PrintMsbw(instr);
      } else if (format[3] == 'd') {
        DCHECK(STRING_STARTS_WITH(format, "msbd"));
        PrintMsbd(instr);
      }
      return 4;
    }
    case 'o': {
      if (format[1] == 'f') {
        if (format[4] == '1') {
          DCHECK(STRING_STARTS_WITH(format, "offs16"));
          PrintOffs16(instr);
          return 6;
        } else if (format[4] == '2') {
          if (format[5] == '1') {
            DCHECK(STRING_STARTS_WITH(format, "offs21"));
            PrintOffs21(instr);
            return 6;
          } else if (format[5] == '6') {
            DCHECK(STRING_STARTS_WITH(format, "offs26"));
            PrintOffs26(instr);
            return 6;
          }
        }
      }
      break;
    }
    case 'p': {
      if (format[6] == '1') {
        DCHECK(STRING_STARTS_WITH(format, "pcoffs16"));
        PrintPCOffs16(instr);
        return 8;
      } else if (format[6] == '2') {
        if (format[7] == '1') {
          DCHECK(STRING_STARTS_WITH(format, "pcoffs21"));
          PrintPCOffs21(instr);
          return 8;
        } else if (format[7] == '6') {
          DCHECK(STRING_STARTS_WITH(format, "pcoffs26"));
          PrintPCOffs26(instr);
          return 8;
        }
      }
      break;
    }
    case 'r': {
      return FormatRegister(instr, format);
    }
    case 's': {
      switch (format[1]) {
        case 'a':
          if (format[2] == '2') {
            DCHECK(STRING_STARTS_WITH(format, "sa2"));
            PrintSa2(instr);
          } else if (format[2] == '3') {
            DCHECK(STRING_STARTS_WITH(format, "sa3"));
            PrintSa3(instr);
          }
          return 3;
        case 'i':
          if (format[2] == '2') {
            DCHECK(STRING_STARTS_WITH(format, "si20"));
            PrintSi20(instr);
            return 4;
          } else if (format[2] == '1') {
            switch (format[3]) {
              case '2':
                DCHECK(STRING_STARTS_WITH(format, "si12"));
                PrintSi12(instr);
                return 4;
              case '4':
                DCHECK(STRING_STARTS_WITH(format, "si14"));
                PrintSi14(instr);
                return 4;
              case '6':
                DCHECK(STRING_STARTS_WITH(format, "si16"));
                PrintSi16(instr);
                return 4;
              default:
                break;
            }
          }
          break;
        default:
          break;
      }
      break;
    }
    case 'u': {
      if (format[2] == '5') {
        DCHECK(STRING_STARTS_WITH(format, "ui5"));
        PrintUi5(instr);
        return 3;
      } else if (format[2] == '6') {
        DCHECK(STRING_STARTS_WITH(format, "ui6"));
        PrintUi6(instr);
        return 3;
      } else if (format[2] == '1') {
        DCHECK(STRING_STARTS_WITH(format, "ui12"));
        PrintUi12(instr);
        return 4;
      }
      break;
    }
    case 'x': {
      if (format[2] == '2') {
        DCHECK(STRING_STARTS_WITH(format, "xi20"));
        PrintXi20(instr);
        return 4;
      } else if (format[3] == '2') {
        DCHECK(STRING_STARTS_WITH(format, "xi12"));
        PrintXi12(instr);
        return 4;
      }
      break;
    }
    default:
      UNREACHABLE();
  }
  return 0;
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
  out_buffer_[out_buffer_pos_] = '\0';
}

// For currently unimplemented decodings the disassembler calls Unknown(instr)
// which will just print "unknown" of the instruction bits.
void Decoder::Unknown(Instruction* instr) { Format(instr, "unknown"); }

int Decoder::DecodeBreakInstr(Instruction* instr) {
  // This is already known to be BREAK instr, just extract the code.
  /*if (instr->Bits(14, 0) == static_cast<int>(kMaxStopCode)) {
    // This is stop(msg).
    Format(instr, "break, code: 'code");
    out_buffer_pos_ += SNPrintF(
        out_buffer_ + out_buffer_pos_, "\n%p       %08" PRIx64,
        static_cast<void*>(reinterpret_cast<int32_t*>(instr + kInstrSize)),
        reinterpret_cast<uint64_t>(
            *reinterpret_cast<char**>(instr + kInstrSize)));
    // Size 3: the break_ instr, plus embedded 64-bit char pointer.
    return 3 * kInstrSize;
  } else {
    Format(instr, "break, code: 'code");
    return kInstrSize;
  }*/
  Format(instr, "break        code: 'code");
  return kInstrSize;
}  //===================================================

void Decoder::DecodeTypekOp6(Instruction* instr) {
  switch (instr->Bits(31, 26) << 26) {
    case ADDU16I_D:
      Format(instr, "addu16i.d    'rd, 'rj, 'si16");
      break;
    case BEQZ:
      Format(instr, "beqz         'rj, 'offs21 -> 'pcoffs21");
      break;
    case BNEZ:
      Format(instr, "bnez         'rj, 'offs21 -> 'pcoffs21");
      break;
    case BCZ:
      if (instr->Bit(8))
        Format(instr, "bcnez        fcc'cj, 'offs21 -> 'pcoffs21");
      else
        Format(instr, "bceqz        fcc'cj, 'offs21 -> 'pcoffs21");
      break;
    case JIRL:
      Format(instr, "jirl         'rd, 'rj, 'offs16");
      break;
    case B:
      Format(instr, "b            'offs26 -> 'pcoffs26");
      break;
    case BL:
      Format(instr, "bl           'offs26 -> 'pcoffs26");
      break;
    case BEQ:
      Format(instr, "beq          'rj, 'rd, 'offs16 -> 'pcoffs16");
      break;
    case BNE:
      Format(instr, "bne          'rj, 'rd, 'offs16 -> 'pcoffs16");
      break;
    case BLT:
      Format(instr, "blt          'rj, 'rd, 'offs16 -> 'pcoffs16");
      break;
    case BGE:
      Format(instr, "bge          'rj, 'rd, 'offs16 -> 'pcoffs16");
      break;
    case BLTU:
      Format(instr, "bltu         'rj, 'rd, 'offs16 -> 'pcoffs16");
      break;
    case BGEU:
      Format(instr, "bgeu         'rj, 'rd, 'offs16 -> 'pcoffs16");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp7(Instruction* instr) {
  switch (instr->Bits(31, 25) << 25) {
    case LU12I_W:
      Format(instr, "lu12i.w      'rd, 'xi20");
      break;
    case LU32I_D:
      Format(instr, "lu32i.d      'rd, 'xi20");
      break;
    case PCADDI:
      Format(instr, "pcaddi       'rd, 'xi20");
      break;
    case PCALAU12I:
      Format(instr, "pcalau12i    'rd, 'xi20");
      break;
    case PCADDU12I:
      Format(instr, "pcaddu12i    'rd, 'xi20");
      break;
    case PCADDU18I:
      Format(instr, "pcaddu18i    'rd, 'xi20");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp8(Instruction* instr) {
  switch (instr->Bits(31, 24) << 24) {
    case LDPTR_W:
      Format(instr, "ldptr.w      'rd, 'rj, 'si14");
      break;
    case STPTR_W:
      Format(instr, "stptr.w      'rd, 'rj, 'si14");
      break;
    case LDPTR_D:
      Format(instr, "ldptr.d      'rd, 'rj, 'si14");
      break;
    case STPTR_D:
      Format(instr, "stptr.d      'rd, 'rj, 'si14");
      break;
    case LL_W:
      Format(instr, "ll.w         'rd, 'rj, 'si14");
      break;
    case SC_W:
      Format(instr, "sc.w         'rd, 'rj, 'si14");
      break;
    case LL_D:
      Format(instr, "ll.d         'rd, 'rj, 'si14");
      break;
    case SC_D:
      Format(instr, "sc.d         'rd, 'rj, 'si14");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp10(Instruction* instr) {
  switch (instr->Bits(31, 22) << 22) {
    case BSTR_W: {
      if (instr->Bit(21) != 0) {
        if (instr->Bit(15) == 0) {
          Format(instr, "bstrins.w    'rd, 'rj, 'msbw, 'lsbw");
        } else {
          Format(instr, "bstrpick.w   'rd, 'rj, 'msbw, 'lsbw");
        }
      }
      break;
    }
    case BSTRINS_D:
      Format(instr, "bstrins.d    'rd, 'rj, 'msbd, 'lsbd");
      break;
    case BSTRPICK_D:
      Format(instr, "bstrpick.d   'rd, 'rj, 'msbd, 'lsbd");
      break;
    case SLTI:
      Format(instr, "slti         'rd, 'rj, 'si12");
      break;
    case SLTUI:
      Format(instr, "sltui        'rd, 'rj, 'si12");
      break;
    case ADDI_W:
      Format(instr, "addi.w       'rd, 'rj, 'si12");
      break;
    case ADDI_D:
      Format(instr, "addi.d       'rd, 'rj, 'si12");
      break;
    case LU52I_D:
      Format(instr, "lu52i.d      'rd, 'rj, 'xi12");
      break;
    case ANDI:
      Format(instr, "andi         'rd, 'rj, 'xi12");
      break;
    case ORI:
      Format(instr, "ori          'rd, 'rj, 'xi12");
      break;
    case XORI:
      Format(instr, "xori         'rd, 'rj, 'xi12");
      break;
    case LD_B:
      Format(instr, "ld.b         'rd, 'rj, 'si12");
      break;
    case LD_H:
      Format(instr, "ld.h         'rd, 'rj, 'si12");
      break;
    case LD_W:
      Format(instr, "ld.w         'rd, 'rj, 'si12");
      break;
    case LD_D:
      Format(instr, "ld.d         'rd, 'rj, 'si12");
      break;
    case ST_B:
      Format(instr, "st.b         'rd, 'rj, 'si12");
      break;
    case ST_H:
      Format(instr, "st.h         'rd, 'rj, 'si12");
      break;
    case ST_W:
      Format(instr, "st.w         'rd, 'rj, 'si12");
      break;
    case ST_D:
      Format(instr, "st.d         'rd, 'rj, 'si12");
      break;
    case LD_BU:
      Format(instr, "ld.bu        'rd, 'rj, 'si12");
      break;
    case LD_HU:
      Format(instr, "ld.hu        'rd, 'rj, 'si12");
      break;
    case LD_WU:
      Format(instr, "ld.wu        'rd, 'rj, 'si12");
      break;
    case FLD_S:
      Format(instr, "fld.s        'fd, 'rj, 'si12");
      break;
    case FST_S:
      Format(instr, "fst.s        'fd, 'rj, 'si12");
      break;
    case FLD_D:
      Format(instr, "fld.d        'fd, 'rj, 'si12");
      break;
    case FST_D:
      Format(instr, "fst.d        'fd, 'rj, 'si12");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp12(Instruction* instr) {
  switch (instr->Bits(31, 20) << 20) {
    case FMADD_S:
      Format(instr, "fmadd.s      'fd, 'fj, 'fk, 'fa");
      break;
    case FMADD_D:
      Format(instr, "fmadd.d      'fd, 'fj, 'fk, 'fa");
      break;
    case FMSUB_S:
      Format(instr, "fmsub.s      'fd, 'fj, 'fk, 'fa");
      break;
    case FMSUB_D:
      Format(instr, "fmsub.d      'fd, 'fj, 'fk, 'fa");
      break;
    case FNMADD_S:
      Format(instr, "fnmadd.s     'fd, 'fj, 'fk, 'fa");
      break;
    case FNMADD_D:
      Format(instr, "fnmadd.d     'fd, 'fj, 'fk, 'fa");
      break;
    case FNMSUB_S:
      Format(instr, "fnmsub.s     'fd, 'fj, 'fk, 'fa");
      break;
    case FNMSUB_D:
      Format(instr, "fnmsub.d     'fd, 'fj, 'fk, 'fa");
      break;
    case FCMP_COND_S:
      switch (instr->Bits(19, 15)) {
        case CAF:
          Format(instr, "fcmp.caf.s   fcc'cd, 'fj, 'fk");
          break;
        case SAF:
          Format(instr, "fcmp.saf.s   fcc'cd, 'fj, 'fk");
          break;
        case CLT:
          Format(instr, "fcmp.clt.s   fcc'cd, 'fj, 'fk");
          break;
        case CEQ:
          Format(instr, "fcmp.ceq.s   fcc'cd, 'fj, 'fk");
          break;
        case SEQ:
          Format(instr, "fcmp.seq.s   fcc'cd, 'fj, 'fk");
          break;
        case CLE:
          Format(instr, "fcmp.cle.s   fcc'cd, 'fj, 'fk");
          break;
        case SLE:
          Format(instr, "fcmp.sle.s   fcc'cd, 'fj, 'fk");
          break;
        case CUN:
          Format(instr, "fcmp.cun.s   fcc'cd, 'fj, 'fk");
          break;
        case SUN:
          Format(instr, "fcmp.sun.s   fcc'cd, 'fj, 'fk");
          break;
        case CULT:
          Format(instr, "fcmp.cult.s  fcc'cd, 'fj, 'fk");
          break;
        case SULT:
          Format(instr, "fcmp.sult.s  fcc'cd, 'fj, 'fk");
          break;
        case CUEQ:
          Format(instr, "fcmp.cueq.s  fcc'cd, 'fj, 'fk");
          break;
        case SUEQ:
          Format(instr, "fcmp.sueq.s  fcc'cd, 'fj, 'fk");
          break;
        case CULE:
          Format(instr, "fcmp.cule.s  fcc'cd, 'fj, 'fk");
          break;
        case SULE:
          Format(instr, "fcmp.sule.s  fcc'cd, 'fj, 'fk");
          break;
        case CNE:
          Format(instr, "fcmp.cne.s   fcc'cd, 'fj, 'fk");
          break;
        case SNE:
          Format(instr, "fcmp.sne.s   fcc'cd, 'fj, 'fk");
          break;
        case COR:
          Format(instr, "fcmp.cor.s   fcc'cd, 'fj, 'fk");
          break;
        case SOR:
          Format(instr, "fcmp.sor.s   fcc'cd, 'fj, 'fk");
          break;
        case CUNE:
          Format(instr, "fcmp.cune.s  fcc'cd, 'fj, 'fk");
          break;
        case SUNE:
          Format(instr, "fcmp.sune.s  fcc'cd, 'fj, 'fk");
          break;
        default:
          UNREACHABLE();
      }
      break;
    case FCMP_COND_D:
      switch (instr->Bits(19, 15)) {
        case CAF:
          Format(instr, "fcmp.caf.d   fcc'cd, 'fj, 'fk");
          break;
        case SAF:
          Format(instr, "fcmp.saf.d   fcc'cd, 'fj, 'fk");
          break;
        case CLT:
          Format(instr, "fcmp.clt.d   fcc'cd, 'fj, 'fk");
          break;
        case CEQ:
          Format(instr, "fcmp.ceq.d   fcc'cd, 'fj, 'fk");
          break;
        case SEQ:
          Format(instr, "fcmp.seq.d   fcc'cd, 'fj, 'fk");
          break;
        case CLE:
          Format(instr, "fcmp.cle.d   fcc'cd, 'fj, 'fk");
          break;
        case SLE:
          Format(instr, "fcmp.sle.d   fcc'cd, 'fj, 'fk");
          break;
        case CUN:
          Format(instr, "fcmp.cun.d   fcc'cd, 'fj, 'fk");
          break;
        case SUN:
          Format(instr, "fcmp.sun.d   fcc'cd, 'fj, 'fk");
          break;
        case CULT:
          Format(instr, "fcmp.cult.d  fcc'cd, 'fj, 'fk");
          break;
        case SULT:
          Format(instr, "fcmp.sult.d  fcc'cd, 'fj, 'fk");
          break;
        case CUEQ:
          Format(instr, "fcmp.cueq.d  fcc'cd, 'fj, 'fk");
          break;
        case SUEQ:
          Format(instr, "fcmp.sueq.d  fcc'cd, 'fj, 'fk");
          break;
        case CULE:
          Format(instr, "fcmp.cule.d  fcc'cd, 'fj, 'fk");
          break;
        case SULE:
          Format(instr, "fcmp.sule.d  fcc'cd, 'fj, 'fk");
          break;
        case CNE:
          Format(instr, "fcmp.cne.d   fcc'cd, 'fj, 'fk");
          break;
        case SNE:
          Format(instr, "fcmp.sne.d   fcc'cd, 'fj, 'fk");
          break;
        case COR:
          Format(instr, "fcmp.cor.d   fcc'cd, 'fj, 'fk");
          break;
        case SOR:
          Format(instr, "fcmp.sor.d   fcc'cd, 'fj, 'fk");
          break;
        case CUNE:
          Format(instr, "fcmp.cune.d  fcc'cd, 'fj, 'fk");
          break;
        case SUNE:
          Format(instr, "fcmp.sune.d  fcc'cd, 'fj, 'fk");
          break;
        default:
          UNREACHABLE();
      }
      break;
    case FSEL:
      Format(instr, "fsel         'fd, 'fj, 'fk, fcc'ca");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp14(Instruction* instr) {
  switch (instr->Bits(31, 18) << 18) {
    case ALSL:
      if (instr->Bit(17))
        Format(instr, "alsl.wu      'rd, 'rj, 'rk, 'sa2");
      else
        Format(instr, "alsl.w       'rd, 'rj, 'rk, 'sa2");
      break;
    case BYTEPICK_W:
      Format(instr, "bytepick.w   'rd, 'rj, 'rk, 'sa2");
      break;
    case BYTEPICK_D:
      Format(instr, "bytepick.d   'rd, 'rj, 'rk, 'sa3");
      break;
    case ALSL_D:
      Format(instr, "alsl.d       'rd, 'rj, 'rk, 'sa2");
      break;
    case SLLI:
      if (instr->Bit(16))
        Format(instr, "slli.d       'rd, 'rj, 'ui6");
      else
        Format(instr, "slli.w       'rd, 'rj, 'ui5");
      break;
    case SRLI:
      if (instr->Bit(16))
        Format(instr, "srli.d       'rd, 'rj, 'ui6");
      else
        Format(instr, "srli.w       'rd, 'rj, 'ui5");
      break;
    case SRAI:
      if (instr->Bit(16))
        Format(instr, "srai.d       'rd, 'rj, 'ui6");
      else
        Format(instr, "srai.w       'rd, 'rj, 'ui5");
      break;
    case ROTRI:
      if (instr->Bit(16))
        Format(instr, "rotri.d      'rd, 'rj, 'ui6");
      else
        Format(instr, "rotri.w      'rd, 'rj, 'ui5");
      break;
    default:
      UNREACHABLE();
  }
}

int Decoder::DecodeTypekOp17(Instruction* instr) {
  switch (instr->Bits(31, 15) << 15) {
    case ADD_W:
      Format(instr, "add.w        'rd, 'rj, 'rk");
      break;
    case ADD_D:
      Format(instr, "add.d        'rd, 'rj, 'rk");
      break;
    case SUB_W:
      Format(instr, "sub.w        'rd, 'rj, 'rk");
      break;
    case SUB_D:
      Format(instr, "sub.d        'rd, 'rj, 'rk");
      break;
    case SLT:
      Format(instr, "slt          'rd, 'rj, 'rk");
      break;
    case SLTU:
      Format(instr, "sltu         'rd, 'rj, 'rk");
      break;
    case MASKEQZ:
      Format(instr, "maskeqz      'rd, 'rj, 'rk");
      break;
    case MASKNEZ:
      Format(instr, "masknez      'rd, 'rj, 'rk");
      break;
    case NOR:
      Format(instr, "nor          'rd, 'rj, 'rk");
      break;
    case AND:
      Format(instr, "and          'rd, 'rj, 'rk");
      break;
    case OR:
      Format(instr, "or           'rd, 'rj, 'rk");
      break;
    case XOR:
      Format(instr, "xor          'rd, 'rj, 'rk");
      break;
    case ORN:
      Format(instr, "orn          'rd, 'rj, 'rk");
      break;
    case ANDN:
      Format(instr, "andn         'rd, 'rj, 'rk");
      break;
    case SLL_W:
      Format(instr, "sll.w        'rd, 'rj, 'rk");
      break;
    case SRL_W:
      Format(instr, "srl.w        'rd, 'rj, 'rk");
      break;
    case SRA_W:
      Format(instr, "sra.w        'rd, 'rj, 'rk");
      break;
    case SLL_D:
      Format(instr, "sll.d        'rd, 'rj, 'rk");
      break;
    case SRL_D:
      Format(instr, "srl.d        'rd, 'rj, 'rk");
      break;
    case SRA_D:
      Format(instr, "sra.d        'rd, 'rj, 'rk");
      break;
    case ROTR_D:
      Format(instr, "rotr.d       'rd, 'rj, 'rk");
      break;
    case ROTR_W:
      Format(instr, "rotr.w       'rd, 'rj, 'rk");
      break;
    case MUL_W:
      Format(instr, "mul.w        'rd, 'rj, 'rk");
      break;
    case MULH_W:
      Format(instr, "mulh.w       'rd, 'rj, 'rk");
      break;
    case MULH_WU:
      Format(instr, "mulh.wu      'rd, 'rj, 'rk");
      break;
    case MUL_D:
      Format(instr, "mul.d        'rd, 'rj, 'rk");
      break;
    case MULH_D:
      Format(instr, "mulh.d       'rd, 'rj, 'rk");
      break;
    case MULH_DU:
      Format(instr, "mulh.du      'rd, 'rj, 'rk");
      break;
    case MULW_D_W:
      Format(instr, "mulw.d.w     'rd, 'rj, 'rk");
      break;
    case MULW_D_WU:
      Format(instr, "mulw.d.wu    'rd, 'rj, 'rk");
      break;
    case DIV_W:
      Format(instr, "div.w        'rd, 'rj, 'rk");
      break;
    case MOD_W:
      Format(instr, "mod.w        'rd, 'rj, 'rk");
      break;
    case DIV_WU:
      Format(instr, "div.wu       'rd, 'rj, 'rk");
      break;
    case MOD_WU:
      Format(instr, "mod.wu       'rd, 'rj, 'rk");
      break;
    case DIV_D:
      Format(instr, "div.d        'rd, 'rj, 'rk");
      break;
    case MOD_D:
      Format(instr, "mod.d        'rd, 'rj, 'rk");
      break;
    case DIV_DU:
      Format(instr, "div.du       'rd, 'rj, 'rk");
      break;
    case MOD_DU:
      Format(instr, "mod.du       'rd, 'rj, 'rk");
      break;
    case BREAK:
      return DecodeBreakInstr(instr);
    case FADD_S:
      Format(instr, "fadd.s       'fd, 'fj, 'fk");
      break;
    case FADD_D:
      Format(instr, "fadd.d       'fd, 'fj, 'fk");
      break;
    case FSUB_S:
      Format(instr, "fsub.s       'fd, 'fj, 'fk");
      break;
    case FSUB_D:
      Format(instr, "fsub.d       'fd, 'fj, 'fk");
      break;
    case FMUL_S:
      Format(instr, "fmul.s       'fd, 'fj, 'fk");
      break;
    case FMUL_D:
      Format(instr, "fmul.d       'fd, 'fj, 'fk");
      break;
    case FDIV_S:
      Format(instr, "fdiv.s       'fd, 'fj, 'fk");
      break;
    case FDIV_D:
      Format(instr, "fdiv.d       'fd, 'fj, 'fk");
      break;
    case FMAX_S:
      Format(instr, "fmax.s       'fd, 'fj, 'fk");
      break;
    case FMAX_D:
      Format(instr, "fmax.d       'fd, 'fj, 'fk");
      break;
    case FMIN_S:
      Format(instr, "fmin.s       'fd, 'fj, 'fk");
      break;
    case FMIN_D:
      Format(instr, "fmin.d       'fd, 'fj, 'fk");
      break;
    case FMAXA_S:
      Format(instr, "fmaxa.s      'fd, 'fj, 'fk");
      break;
    case FMAXA_D:
      Format(instr, "fmaxa.d      'fd, 'fj, 'fk");
      break;
    case FMINA_S:
      Format(instr, "fmina.s      'fd, 'fj, 'fk");
      break;
    case FMINA_D:
      Format(instr, "fmina.d      'fd, 'fj, 'fk");
      break;
    case LDX_B:
      Format(instr, "ldx.b        'rd, 'rj, 'rk");
      break;
    case LDX_H:
      Format(instr, "ldx.h        'rd, 'rj, 'rk");
      break;
    case LDX_W:
      Format(instr, "ldx.w        'rd, 'rj, 'rk");
      break;
    case LDX_D:
      Format(instr, "ldx.d        'rd, 'rj, 'rk");
      break;
    case STX_B:
      Format(instr, "stx.b        'rd, 'rj, 'rk");
      break;
    case STX_H:
      Format(instr, "stx.h        'rd, 'rj, 'rk");
      break;
    case STX_W:
      Format(instr, "stx.w        'rd, 'rj, 'rk");
      break;
    case STX_D:
      Format(instr, "stx.d        'rd, 'rj, 'rk");
      break;
    case LDX_BU:
      Format(instr, "ldx.bu       'rd, 'rj, 'rk");
      break;
    case LDX_HU:
      Format(instr, "ldx.hu       'rd, 'rj, 'rk");
      break;
    case LDX_WU:
      Format(instr, "ldx.wu       'rd, 'rj, 'rk");
      break;
    case FLDX_S:
      Format(instr, "fldx.s       'fd, 'rj, 'rk");
      break;
    case FLDX_D:
      Format(instr, "fldx.d       'fd, 'rj, 'rk");
      break;
    case FSTX_S:
      Format(instr, "fstx.s       'fd, 'rj, 'rk");
      break;
    case FSTX_D:
      Format(instr, "fstx.d       'fd, 'rj, 'rk");
      break;
    case AMSWAP_W:
      Format(instr, "amswap.w     'rd, 'rk, 'rj");
      break;
    case AMSWAP_D:
      Format(instr, "amswap.d     'rd, 'rk, 'rj");
      break;
    case AMADD_W:
      Format(instr, "amadd.w      'rd, 'rk, 'rj");
      break;
    case AMADD_D:
      Format(instr, "amadd.d      'rd, 'rk, 'rj");
      break;
    case AMAND_W:
      Format(instr, "amand.w      'rd, 'rk, 'rj");
      break;
    case AMAND_D:
      Format(instr, "amand.d      'rd, 'rk, 'rj");
      break;
    case AMOR_W:
      Format(instr, "amor.w       'rd, 'rk, 'rj");
      break;
    case AMOR_D:
      Format(instr, "amor.d       'rd, 'rk, 'rj");
      break;
    case AMXOR_W:
      Format(instr, "amxor.w      'rd, 'rk, 'rj");
      break;
    case AMXOR_D:
      Format(instr, "amxor.d      'rd, 'rk, 'rj");
      break;
    case AMMAX_W:
      Format(instr, "ammax.w      'rd, 'rk, 'rj");
      break;
    case AMMAX_D:
      Format(instr, "ammax.d      'rd, 'rk, 'rj");
      break;
    case AMMIN_W:
      Format(instr, "ammin.w      'rd, 'rk, 'rj");
      break;
    case AMMIN_D:
      Format(instr, "ammin.d      'rd, 'rk, 'rj");
      break;
    case AMMAX_WU:
      Format(instr, "ammax.wu     'rd, 'rk, 'rj");
      break;
    case AMMAX_DU:
      Format(instr, "ammax.du     'rd, 'rk, 'rj");
      break;
    case AMMIN_WU:
      Format(instr, "ammin.wu     'rd, 'rk, 'rj");
      break;
    case AMMIN_DU:
      Format(instr, "ammin.du     'rd, 'rk, 'rj");
      break;
    case AMSWAP_DB_W:
      Format(instr, "amswap_db.w  'rd, 'rk, 'rj");
      break;
    case AMSWAP_DB_D:
      Format(instr, "amswap_db.d  'rd, 'rk, 'rj");
      break;
    case AMADD_DB_W:
      Format(instr, "amadd_db.w   'rd, 'rk, 'rj");
      break;
    case AMADD_DB_D:
      Format(instr, "amadd_db.d   'rd, 'rk, 'rj");
      break;
    case AMAND_DB_W:
      Format(instr, "amand_db.w   'rd, 'rk, 'rj");
      break;
    case AMAND_DB_D:
      Format(instr, "amand_db.d   'rd, 'rk, 'rj");
      break;
    case AMOR_DB_W:
      Format(instr, "amor_db.w    'rd, 'rk, 'rj");
      break;
    case AMOR_DB_D:
      Format(instr, "amor_db.d    'rd, 'rk, 'rj");
      break;
    case AMXOR_DB_W:
      Format(instr, "amxor_db.w   'rd, 'rk, 'rj");
      break;
    case AMXOR_DB_D:
      Format(instr, "amxor_db.d   'rd, 'rk, 'rj");
      break;
    case AMMAX_DB_W:
      Format(instr, "ammax_db.w   'rd, 'rk, 'rj");
      break;
    case AMMAX_DB_D:
      Format(instr, "ammax_db.d   'rd, 'rk, 'rj");
      break;
    case AMMIN_DB_W:
      Format(instr, "ammin_db.w   'rd, 'rk, 'rj");
      break;
    case AMMIN_DB_D:
      Format(instr, "ammin_db.d   'rd, 'rk, 'rj");
      break;
    case AMMAX_DB_WU:
      Format(instr, "ammax_db.wu  'rd, 'rk, 'rj");
      break;
    case AMMAX_DB_DU:
      Format(instr, "ammax_db.du  'rd, 'rk, 'rj");
      break;
    case AMMIN_DB_WU:
      Format(instr, "ammin_db.wu  'rd, 'rk, 'rj");
      break;
    case AMMIN_DB_DU:
      Format(instr, "ammin_db.du  'rd, 'rk, 'rj");
      break;
    case DBAR:
      Format(instr, "dbar         'hint15");
      break;
    case IBAR:
      Format(instr, "ibar         'hint15");
      break;
    case FSCALEB_S:
      Format(instr, "fscaleb.s    'fd, 'fj, 'fk");
      break;
    case FSCALEB_D:
      Format(instr, "fscaleb.d    'fd, 'fj, 'fk");
      break;
    case FCOPYSIGN_S:
      Format(instr, "fcopysign.s  'fd, 'fj, 'fk");
      break;
    case FCOPYSIGN_D:
      Format(instr, "fcopysign.d  'fd, 'fj, 'fk");
      break;
    default:
      UNREACHABLE();
  }
  return kInstrSize;
}

void Decoder::DecodeTypekOp22(Instruction* instr) {
  switch (instr->Bits(31, 10) << 10) {
    case CLZ_W:
      Format(instr, "clz.w        'rd, 'rj");
      break;
    case CTZ_W:
      Format(instr, "ctz.w        'rd, 'rj");
      break;
    case CLZ_D:
      Format(instr, "clz.d        'rd, 'rj");
      break;
    case CTZ_D:
      Format(instr, "ctz.d        'rd, 'rj");
      break;
    case REVB_2H:
      Format(instr, "revb.2h      'rd, 'rj");
      break;
    case REVB_4H:
      Format(instr, "revb.4h      'rd, 'rj");
      break;
    case REVB_2W:
      Format(instr, "revb.2w      'rd, 'rj");
      break;
    case REVB_D:
      Format(instr, "revb.d       'rd, 'rj");
      break;
    case REVH_2W:
      Format(instr, "revh.2w      'rd, 'rj");
      break;
    case REVH_D:
      Format(instr, "revh.d       'rd, 'rj");
      break;
    case BITREV_4B:
      Format(instr, "bitrev.4b    'rd, 'rj");
      break;
    case BITREV_8B:
      Format(instr, "bitrev.8b    'rd, 'rj");
      break;
    case BITREV_W:
      Format(instr, "bitrev.w     'rd, 'rj");
      break;
    case BITREV_D:
      Format(instr, "bitrev.d     'rd, 'rj");
      break;
    case EXT_W_B:
      Format(instr, "ext.w.b      'rd, 'rj");
      break;
    case EXT_W_H:
      Format(instr, "ext.w.h      'rd, 'rj");
      break;
    case FABS_S:
      Format(instr, "fabs.s       'fd, 'fj");
      break;
    case FABS_D:
      Format(instr, "fabs.d       'fd, 'fj");
      break;
    case FNEG_S:
      Format(instr, "fneg.s       'fd, 'fj");
      break;
    case FNEG_D:
      Format(instr, "fneg.d       'fd, 'fj");
      break;
    case FSQRT_S:
      Format(instr, "fsqrt.s      'fd, 'fj");
      break;
    case FSQRT_D:
      Format(instr, "fsqrt.d      'fd, 'fj");
      break;
    case FMOV_S:
      Format(instr, "fmov.s       'fd, 'fj");
      break;
    case FMOV_D:
      Format(instr, "fmov.d       'fd, 'fj");
      break;
    case MOVGR2FR_W:
      Format(instr, "movgr2fr.w   'fd, 'rj");
      break;
    case MOVGR2FR_D:
      Format(instr, "movgr2fr.d   'fd, 'rj");
      break;
    case MOVGR2FRH_W:
      Format(instr, "movgr2frh.w  'fd, 'rj");
      break;
    case MOVFR2GR_S:
      Format(instr, "movfr2gr.s   'rd, 'fj");
      break;
    case MOVFR2GR_D:
      Format(instr, "movfr2gr.d   'rd, 'fj");
      break;
    case MOVFRH2GR_S:
      Format(instr, "movfrh2gr.s  'rd, 'fj");
      break;
    case MOVGR2FCSR:
      Format(instr, "movgr2fcsr   fcsr, 'rj");
      break;
    case MOVFCSR2GR:
      Format(instr, "movfcsr2gr   'rd, fcsr");
      break;
    case FCVT_S_D:
      Format(instr, "fcvt.s.d     'fd, 'fj");
      break;
    case FCVT_D_S:
      Format(instr, "fcvt.d.s     'fd, 'fj");
      break;
    case FTINTRM_W_S:
      Format(instr, "ftintrm.w.s  'fd, 'fj");
      break;
    case FTINTRM_W_D:
      Format(instr, "ftintrm.w.d  'fd, 'fj");
      break;
    case FTINTRM_L_S:
      Format(instr, "ftintrm.l.s  'fd, 'fj");
      break;
    case FTINTRM_L_D:
      Format(instr, "ftintrm.l.d  'fd, 'fj");
      break;
    case FTINTRP_W_S:
      Format(instr, "ftintrp.w.s  'fd, 'fj");
      break;
    case FTINTRP_W_D:
      Format(instr, "ftintrp.w.d  'fd, 'fj");
      break;
    case FTINTRP_L_S:
      Format(instr, "ftintrp.l.s  'fd, 'fj");
      break;
    case FTINTRP_L_D:
      Format(instr, "ftintrp.l.d  'fd, 'fj");
      break;
    case FTINTRZ_W_S:
      Format(instr, "ftintrz.w.s  'fd, 'fj");
      break;
    case FTINTRZ_W_D:
      Format(instr, "ftintrz.w.d  'fd, 'fj");
      break;
    case FTINTRZ_L_S:
      Format(instr, "ftintrz.l.s  'fd, 'fj");
      break;
    case FTINTRZ_L_D:
      Format(instr, "ftintrz.l.d  'fd, 'fj");
      break;
    case FTINTRNE_W_S:
      Format(instr, "ftintrne.w.s 'fd, 'fj");
      break;
    case FTINTRNE_W_D:
      Format(instr, "ftintrne.w.d 'fd, 'fj");
      break;
    case FTINTRNE_L_S:
      Format(instr, "ftintrne.l.s 'fd, 'fj");
      break;
    case FTINTRNE_L_D:
      Format(instr, "ftintrne.l.d 'fd, 'fj");
      break;
    case FTINT_W_S:
      Format(instr, "ftint.w.s    'fd, 'fj");
      break;
    case FTINT_W_D:
      Format(instr, "ftint.w.d    'fd, 'fj");
      break;
    case FTINT_L_S:
      Format(instr, "ftint.l.s    'fd, 'fj");
      break;
    case FTINT_L_D:
      Format(instr, "ftint.l.d    'fd, 'fj");
      break;
    case FFINT_S_W:
      Format(instr, "ffint.s.w    'fd, 'fj");
      break;
    case FFINT_S_L:
      Format(instr, "ffint.s.l    'fd, 'fj");
      break;
    case FFINT_D_W:
      Format(instr, "ffint.d.w    'fd, 'fj");
      break;
    case FFINT_D_L:
      Format(instr, "ffint.d.l    'fd, 'fj");
      break;
    case FRINT_S:
      Format(instr, "frint.s      'fd, 'fj");
      break;
    case FRINT_D:
      Format(instr, "frint.d      'fd, 'fj");
      break;
    case MOVFR2CF:
      Format(instr, "movfr2cf     fcc'cd, 'fj");
      break;
    case MOVCF2FR:
      Format(instr, "movcf2fr     'fd, fcc'cj");
      break;
    case MOVGR2CF:
      Format(instr, "movgr2cf     fcc'cd, 'rj");
      break;
    case MOVCF2GR:
      Format(instr, "movcf2gr     'rd, fcc'cj");
      break;
    case FRECIP_S:
      Format(instr, "frecip.s     'fd, 'fj");
      break;
    case FRECIP_D:
      Format(instr, "frecip.d     'fd, 'fj");
      break;
    case FRSQRT_S:
      Format(instr, "frsqrt.s     'fd, 'fj");
      break;
    case FRSQRT_D:
      Format(instr, "frsqrt.d     'fd, 'fj");
      break;
    case FCLASS_S:
      Format(instr, "fclass.s     'fd, 'fj");
      break;
    case FCLASS_D:
      Format(instr, "fclass.d     'fd, 'fj");
      break;
    case FLOGB_S:
      Format(instr, "flogb.s      'fd, 'fj");
      break;
    case FLOGB_D:
      Format(instr, "flogb.d      'fd, 'fj");
      break;
    case CLO_W:
      Format(instr, "clo.w        'rd, 'rj");
      break;
    case CTO_W:
      Format(instr, "cto.w        'rd, 'rj");
      break;
    case CLO_D:
      Format(instr, "clo.d        'rd, 'rj");
      break;
    case CTO_D:
      Format(instr, "cto.d        'rd, 'rj");
      break;
    default:
      UNREACHABLE();
  }
}

int Decoder::InstructionDecode(byte* instr_ptr) {
  Instruction* instr = Instruction::At(instr_ptr);
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_,
                                    "%08x       ", instr->InstructionBits());
  switch (instr->InstructionType()) {
    case Instruction::kOp6Type: {
      DecodeTypekOp6(instr);
      break;
    }
    case Instruction::kOp7Type: {
      DecodeTypekOp7(instr);
      break;
    }
    case Instruction::kOp8Type: {
      DecodeTypekOp8(instr);
      break;
    }
    case Instruction::kOp10Type: {
      DecodeTypekOp10(instr);
      break;
    }
    case Instruction::kOp12Type: {
      DecodeTypekOp12(instr);
      break;
    }
    case Instruction::kOp14Type: {
      DecodeTypekOp14(instr);
      break;
    }
    case Instruction::kOp17Type: {
      return DecodeTypekOp17(instr);
    }
    case Instruction::kOp22Type: {
      DecodeTypekOp22(instr);
      break;
    }
    case Instruction::kUnsupported: {
      Format(instr, "UNSUPPORTED");
      break;
    }
    default: {
      Format(instr, "UNSUPPORTED");
      break;
    }
  }
  return kInstrSize;
}

}  // namespace internal
}  // namespace v8

//------------------------------------------------------------------------------

namespace disasm {

const char* NameConverter::NameOfAddress(byte* addr) const {
  v8::base::SNPrintF(tmp_buffer_, "%p", static_cast<void*>(addr));
  return tmp_buffer_.begin();
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
  UNREACHABLE();
}

const char* NameConverter::NameInCode(byte* addr) const {
  // The default name converter is called for unknown code. So we will not try
  // to access any memory.
  return "";
}

//------------------------------------------------------------------------------

int Disassembler::InstructionDecode(v8::base::Vector<char> buffer,
                                    byte* instruction) {
  v8::internal::Decoder d(converter_, buffer);
  return d.InstructionDecode(instruction);
}

int Disassembler::ConstantPoolSizeAt(byte* instruction) { return -1; }

void Disassembler::Disassemble(FILE* f, byte* begin, byte* end,
                               UnimplementedOpcodeAction unimplemented_action) {
  NameConverter converter;
  Disassembler d(converter, unimplemented_action);
  for (byte* pc = begin; pc < end;) {
    v8::base::EmbeddedVector<char, 128> buffer;
    buffer[0] = '\0';
    byte* prev_pc = pc;
    pc += d.InstructionDecode(buffer, pc);
    v8::internal::PrintF(f, "%p    %08x      %s\n", static_cast<void*>(prev_pc),
                         *reinterpret_cast<int32_t*>(prev_pc), buffer.begin());
  }
}

#undef STRING_STARTS_WITH

}  // namespace disasm

#endif  // V8_TARGET_ARCH_LOONG64

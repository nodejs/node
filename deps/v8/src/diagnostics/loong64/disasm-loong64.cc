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
  int InstructionDecode(uint8_t* instruction);

 private:
  // Bottleneck functions to print into the out_buffer.
  void PrintChar(const char ch);
  void Print(const char* str);

  // Printing of common values.
  void PrintRegister(int reg);
  void PrintFPURegister(int freg);
  void PrintFPUStatusRegister(int freg);
  void PrintVRegister(int vreg);
  void PrintRj(Instruction* instr);
  void PrintRk(Instruction* instr);
  void PrintRd(Instruction* instr);
  void PrintFj(Instruction* instr);
  void PrintFk(Instruction* instr);
  void PrintFd(Instruction* instr);
  void PrintFa(Instruction* instr);
  void PrintVj(Instruction* instr);
  void PrintVk(Instruction* instr);
  void PrintVd(Instruction* instr);
  void PrintVa(Instruction* instr);
  void PrintSa2(Instruction* instr);
  void PrintSa3(Instruction* instr);
  void PrintUi1(Instruction* instr);
  void PrintUi2(Instruction* instr);
  void PrintUi3(Instruction* instr);
  void PrintUi4(Instruction* instr);
  void PrintUi5(Instruction* instr);
  void PrintUi6(Instruction* instr);
  void PrintUi7(Instruction* instr);
  void PrintUi8(Instruction* instr);
  void PrintUi12(Instruction* instr);
  void PrintMsbw(Instruction* instr);
  void PrintLsbw(Instruction* instr);
  void PrintMsbd(Instruction* instr);
  void PrintLsbd(Instruction* instr);
  // void PrintCond(Instruction* instr);
  void PrintSi5(Instruction* instr);
  void PrintSi8(Instruction* instr);
  void PrintSi9(Instruction* instr);
  void PrintSi10(Instruction* instr);
  void PrintSi11(Instruction* instr);
  void PrintSi12(Instruction* instr);
  void PrintSi13(Instruction* instr);
  void PrintSi14(Instruction* instr);
  void PrintSi16(Instruction* instr);
  void PrintSi20(Instruction* instr);
  void PrintXi12(Instruction* instr);
  void PrintXi20(Instruction* instr);
  void PrintIdx1(Instruction* instr);
  void PrintIdx2(Instruction* instr);
  void PrintIdx3(Instruction* instr);
  void PrintIdx4(Instruction* instr);
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
  int FormatVRegister(Instruction* instr, const char* option);
  int FormatOption(Instruction* instr, const char* option);
  void Format(Instruction* instr, const char* format);
  void Unknown(Instruction* instr);
  int DecodeBreakInstr(Instruction* instr);

  // Each of these functions decodes one particular instruction type.
  void InstructionDecode(Instruction* instr);
  void DecodeTypekOp6(Instruction* instr);
  void DecodeTypekOp7(Instruction* instr);
  void DecodeTypekOp8(Instruction* instr);
  void DecodeTypekOp10(Instruction* instr);
  void DecodeTypekOp11(Instruction* instr);
  void DecodeTypekOp12(Instruction* instr);
  void DecodeTypekOp13(Instruction* instr);
  void DecodeTypekOp14(Instruction* instr);
  void DecodeTypekOp15(Instruction* instr);
  void DecodeTypekOp16(Instruction* instr);
  int DecodeTypekOp17(Instruction* instr);
  void DecodeTypekOp18(Instruction* instr);
  void DecodeTypekOp19(Instruction* instr);
  void DecodeTypekOp20(Instruction* instr);
  void DecodeTypekOp21(Instruction* instr);
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

// Print the VRegister name according to the active name converter.
void Decoder::PrintVRegister(int vreg) { Print(VRegisters::Name(vreg)); }

void Decoder::PrintVj(Instruction* instr) {
  int vreg = instr->VjValue();
  PrintVRegister(vreg);
}

void Decoder::PrintVk(Instruction* instr) {
  int vreg = instr->VkValue();
  PrintVRegister(vreg);
}

void Decoder::PrintVd(Instruction* instr) {
  int vreg = instr->VdValue();
  PrintVRegister(vreg);
}

void Decoder::PrintVa(Instruction* instr) {
  int vreg = instr->VaValue();
  PrintVRegister(vreg);
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

void Decoder::PrintUi1(Instruction* instr) {
  int ui = instr->Ui1Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi2(Instruction* instr) {
  int ui = instr->Ui2Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi3(Instruction* instr) {
  int ui = instr->Ui3Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi4(Instruction* instr) {
  int ui = instr->Ui4Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi5(Instruction* instr) {
  int ui = instr->Ui5Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi6(Instruction* instr) {
  int ui = instr->Ui6Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi7(Instruction* instr) {
  int ui = instr->Ui7Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", ui);
}

void Decoder::PrintUi8(Instruction* instr) {
  int ui = instr->Ui8Value();
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

void Decoder::PrintSi5(Instruction* instr) {
  int si = ((instr->Si5Value()) << (32 - kSi5Bits)) >> (32 - kSi5Bits);
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si5Value());
}

void Decoder::PrintSi8(Instruction* instr) {
  int si = ((instr->Si8Value()) << (32 - kSi8Bits)) >> (32 - kSi8Bits);
  int shift = 0;
  if (((instr->InstructionBits() >> 19) << 19) == VSTELM_D)
    shift = 3;
  else if (((instr->InstructionBits() >> 20) << 20) == VSTELM_W)
    shift = 2;
  else if (((instr->InstructionBits() >> 21) << 21) == VSTELM_H)
    shift = 1;
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si << shift, instr->Si8Value() << shift);
}

void Decoder::PrintSi9(Instruction* instr) {
  int si = ((instr->Si9Value()) << (32 - kSi9Bits)) >> (32 - kSi9Bits);
  si <<= 3;
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si9Value() << 3);
}

void Decoder::PrintSi10(Instruction* instr) {
  int si = ((instr->Si10Value()) << (32 - kSi10Bits)) >> (32 - kSi10Bits);
  si <<= 2;
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si10Value() << 2);
}

void Decoder::PrintSi11(Instruction* instr) {
  int si = ((instr->Si11Value()) << (32 - kSi11Bits)) >> (32 - kSi11Bits);
  si <<= 1;
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si11Value() << 1);
}

void Decoder::PrintSi12(Instruction* instr) {
  int si = ((instr->Si12Value()) << (32 - kSi12Bits)) >> (32 - kSi12Bits);
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si12Value());
}

void Decoder::PrintSi13(Instruction* instr) {
  int si = ((instr->Si13Value()) << (32 - kSi13Bits)) >> (32 - kSi13Bits);
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%d(0x%x)",
                                    si, instr->Si13Value());
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

void Decoder::PrintIdx1(Instruction* instr) {
  int idx = instr->Idx1Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", idx);
}

void Decoder::PrintIdx2(Instruction* instr) {
  int idx = instr->Idx2Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", idx);
}

void Decoder::PrintIdx3(Instruction* instr) {
  int idx = instr->Idx3Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", idx);
}

void Decoder::PrintIdx4(Instruction* instr) {
  int idx = instr->Idx4Value();
  out_buffer_pos_ += base::SNPrintF(out_buffer_ + out_buffer_pos_, "%u", idx);
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
      converter_.NameOfAddress(reinterpret_cast<uint8_t*>(instr) + target));
}

void Decoder::PrintPCOffs21(Instruction* instr) {
  int n_bits = 2;
  int offs = instr->Offs21Value();
  int target =
      ((offs << n_bits) << (32 - kOffsLowBits - kOffs21HighBits - n_bits)) >>
      (32 - kOffsLowBits - kOffs21HighBits - n_bits);
  out_buffer_pos_ += base::SNPrintF(
      out_buffer_ + out_buffer_pos_, "%s",
      converter_.NameOfAddress(reinterpret_cast<uint8_t*>(instr) + target));
}

void Decoder::PrintPCOffs26(Instruction* instr) {
  int n_bits = 2;
  int offs = instr->Offs26Value();
  int target =
      ((offs << n_bits) << (32 - kOffsLowBits - kOffs26HighBits - n_bits)) >>
      (32 - kOffsLowBits - kOffs26HighBits - n_bits);
  out_buffer_pos_ += base::SNPrintF(
      out_buffer_ + out_buffer_pos_, "%s",
      converter_.NameOfAddress(reinterpret_cast<uint8_t*>(instr) + target));
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

// Handle all vector register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatVRegister(Instruction* instr, const char* format) {
  DCHECK_EQ(format[0], 'v');
  if (format[1] == 'j') {  // 'vj: vj register.
    int reg = instr->VjValue();
    PrintVRegister(reg);
    return 2;
  } else if (format[1] == 'k') {  // 'vk: vk register.
    int reg = instr->VkValue();
    PrintVRegister(reg);
    return 2;
  } else if (format[1] == 'd') {  // 'vd: vd register.
    int reg = instr->VdValue();
    PrintVRegister(reg);
    return 2;
  } else if (format[1] == 'a') {  // 'va: va register.
    int reg = instr->VaValue();
    PrintVRegister(reg);
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
    case 'i': {
      switch (format[3]) {
        case '1':
          DCHECK(STRING_STARTS_WITH(format, "idx1"));
          PrintIdx1(instr);
          return 4;
        case '2':
          DCHECK(STRING_STARTS_WITH(format, "idx2"));
          PrintIdx2(instr);
          return 4;
        case '3':
          DCHECK(STRING_STARTS_WITH(format, "idx3"));
          PrintIdx3(instr);
          return 4;
        case '4':
          DCHECK(STRING_STARTS_WITH(format, "idx4"));
          PrintIdx4(instr);
          return 4;
        default:
          return 0;
      }
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
              case '0':
                DCHECK(STRING_STARTS_WITH(format, "si10"));
                PrintSi10(instr);
                return 4;
              case '1':
                DCHECK(STRING_STARTS_WITH(format, "si11"));
                PrintSi11(instr);
                return 4;
              case '2':
                DCHECK(STRING_STARTS_WITH(format, "si12"));
                PrintSi12(instr);
                return 4;
              case '3':
                DCHECK(STRING_STARTS_WITH(format, "si13"));
                PrintSi13(instr);
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
          } else if (format[2] == '5') {
            DCHECK(STRING_STARTS_WITH(format, "si5"));
            PrintSi5(instr);
            return 3;
          } else if (format[2] == '8') {
            DCHECK(STRING_STARTS_WITH(format, "si8"));
            PrintSi8(instr);
            return 3;
          } else if (format[2] == '9') {
            DCHECK(STRING_STARTS_WITH(format, "si9"));
            PrintSi9(instr);
            return 3;
          }
          break;
        default:
          break;
      }
      break;
    }
    case 'u': {
      if (format[2] == '1') {
        if (format[3] == '2') {
          DCHECK(STRING_STARTS_WITH(format, "ui12"));
          PrintUi12(instr);
          return 4;
        } else {
          DCHECK(STRING_STARTS_WITH(format, "ui1"));
          PrintUi1(instr);
          return 3;
        }
      } else if (format[2] == '2') {
        DCHECK(STRING_STARTS_WITH(format, "ui2"));
        PrintUi2(instr);
        return 3;
      } else if (format[2] == '3') {
        DCHECK(STRING_STARTS_WITH(format, "ui3"));
        PrintUi3(instr);
        return 3;
      } else if (format[2] == '4') {
        DCHECK(STRING_STARTS_WITH(format, "ui4"));
        PrintUi4(instr);
        return 3;
      } else if (format[2] == '5') {
        DCHECK(STRING_STARTS_WITH(format, "ui5"));
        PrintUi5(instr);
        return 3;
      } else if (format[2] == '6') {
        DCHECK(STRING_STARTS_WITH(format, "ui6"));
        PrintUi6(instr);
        return 3;
      } else if (format[2] == '7') {
        DCHECK(STRING_STARTS_WITH(format, "ui7"));
        PrintUi7(instr);
        return 3;
      } else if (format[2] == '8') {
        DCHECK(STRING_STARTS_WITH(format, "ui8"));
        PrintUi8(instr);
        return 3;
      }
      break;
    }
    case 'v': {
      return FormatVRegister(instr, format);
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
      printf("%s \n", format);
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
    case VLD:
      Format(instr, "vld          'vd, 'rj, 'si12");
      break;
    case VST:
      Format(instr, "vst          'vd, 'rj, 'si12");
      break;
    case VLDREPL_B:
      Format(instr, "vldrepl.b    'vd, 'rj, 'si12");
      break;
    case VSTELM_B:
      Format(instr, "vstelm.b     'vd, 'rj, 'si8, 'idx4");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp11(Instruction* instr) {
  switch (instr->Bits(31, 21) << 21) {
    case VLDREPL_H:
      Format(instr, "vldrepl.h    'vd, 'rj, 'si11");
      break;
    case VSTELM_H:
      Format(instr, "vstelm.h     'vd, 'rj, 'si8, 'idx3");
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
    case VFMADD_S:
      Format(instr, "vfmadd.s     'vd, 'vj, 'vk, 'va");
      break;
    case VFMADD_D:
      Format(instr, "vfmadd.d     'vd, 'vj, 'vk, 'va");
      break;
    case VFMSUB_S:
      Format(instr, "vfmsub.s     'vd, 'vj, 'vk, 'va");
      break;
    case VFMSUB_D:
      Format(instr, "vfmsub.d     'vd, 'vj, 'vk, 'va");
      break;
    case VFNMADD_S:
      Format(instr, "vfnmadd.s    'vd, 'vj, 'vk, 'va");
      break;
    case VFNMADD_D:
      Format(instr, "vfnmadd.d    'vd, 'vj, 'vk, 'va");
      break;
    case VFNMSUB_S:
      Format(instr, "vfnmsub.s    'vd, 'vj, 'vk, 'va");
      break;
    case VFNMSUB_D:
      Format(instr, "vfnmsub.d    'vd, 'vj, 'vk, 'va");
      break;
    case VFCMP_COND_S:
      switch (instr->Bits(19, 15)) {
        case CAF:
          Format(instr, "vfcmp.caf.s  'vd, 'vj, 'vk");
          break;
        case SAF:
          Format(instr, "vfcmp.saf.s  'vd, 'vj, 'vk");
          break;
        case CLT:
          Format(instr, "vfcmp.clt.s  'vd, 'vj, 'vk");
          break;
        case SLT_:
          Format(instr, "vfcmp.slt.s  'vd, 'vj, 'vk");
          break;
        case CEQ:
          Format(instr, "vfcmp.ceq.s  'vd, 'vj, 'vk");
          break;
        case SEQ:
          Format(instr, "vfcmp.seq.s  'vd, 'vj, 'vk");
          break;
        case CLE:
          Format(instr, "vfcmp.cle.s  'vd, 'vj, 'vk");
          break;
        case SLE:
          Format(instr, "vfcmp.sle.s  'vd, 'vj, 'vk");
          break;
        case CUN:
          Format(instr, "vfcmp.cun.s  'vd, 'vj, 'vk");
          break;
        case SUN:
          Format(instr, "vfcmp.sun.s  'vd, 'vj, 'vk");
          break;
        case CULT:
          Format(instr, "vfcmp.cult.s 'vd, 'vj, 'vk");
          break;
        case SULT:
          Format(instr, "vfcmp.sult.s 'vd, 'vj, 'vk");
          break;
        case CUEQ:
          Format(instr, "vfcmp.cueq.s 'vd, 'vj, 'vk");
          break;
        case SUEQ:
          Format(instr, "vfcmp.sueq.s 'vd, 'vj, 'vk");
          break;
        case CULE:
          Format(instr, "vfcmp.cule.s 'vd, 'vj, 'vk");
          break;
        case SULE:
          Format(instr, "vfcmp.sule.s 'vd, 'vj, 'vk");
          break;
        case CNE:
          Format(instr, "vfcmp.cne.s  'vd, 'vj, 'vk");
          break;
        case SNE:
          Format(instr, "vfcmp.sne.s  'vd, 'vj, 'vk");
          break;
        case COR:
          Format(instr, "vfcmp.cor.s  'vd, 'vj, 'vk");
          break;
        case SOR:
          Format(instr, "vfcmp.sor.s  'vd, 'vj, 'vk");
          break;
        case CUNE:
          Format(instr, "vfcmp.cune.s 'vd, 'vj, 'vk");
          break;
        case SUNE:
          Format(instr, "vfcmp.sune.s 'vd, 'vj, 'vk");
          break;
        default:
          UNREACHABLE();
      }
      break;
    case VFCMP_COND_D:
      switch (instr->Bits(19, 15)) {
        case CAF:
          Format(instr, "vfcmp.caf.d  'vd, 'vj, 'vk");
          break;
        case SAF:
          Format(instr, "vfcmp.saf.d  'vd, 'vj, 'vk");
          break;
        case CLT:
          Format(instr, "vfcmp.clt.d  'vd, 'vj, 'vk");
          break;
        case SLT_:
          Format(instr, "vfcmp.slt.d  'vd, 'vj, 'vk");
          break;
        case CEQ:
          Format(instr, "vfcmp.ceq.d  'vd, 'vj, 'vk");
          break;
        case SEQ:
          Format(instr, "vfcmp.seq.d  'vd, 'vj, 'vk");
          break;
        case CLE:
          Format(instr, "vfcmp.cle.d  'vd, 'vj, 'vk");
          break;
        case SLE:
          Format(instr, "vfcmp.sle.d  'vd, 'vj, 'vk");
          break;
        case CUN:
          Format(instr, "vfcmp.cun.d  'vd, 'vj, 'vk");
          break;
        case SUN:
          Format(instr, "vfcmp.sun.d  'vd, 'vj, 'vk");
          break;
        case CULT:
          Format(instr, "vfcmp.cult.d 'vd, 'vj, 'vk");
          break;
        case SULT:
          Format(instr, "vfcmp.sult.d 'vd, 'vj, 'vk");
          break;
        case CUEQ:
          Format(instr, "vfcmp.cueq.d 'vd, 'vj, 'vk");
          break;
        case SUEQ:
          Format(instr, "vfcmp.sueq.d 'vd, 'vj, 'vk");
          break;
        case CULE:
          Format(instr, "vfcmp.cule.d 'vd, 'vj, 'vk");
          break;
        case SULE:
          Format(instr, "vfcmp.sule.d 'vd, 'vj, 'vk");
          break;
        case CNE:
          Format(instr, "vfcmp.cne.d  'vd, 'vj, 'vk");
          break;
        case SNE:
          Format(instr, "vfcmp.sne.d  'vd, 'vj, 'vk");
          break;
        case COR:
          Format(instr, "vfcmp.cor.d  'vd, 'vj, 'vk");
          break;
        case SOR:
          Format(instr, "vfcmp.sor.d  'vd, 'vj, 'vk");
          break;
        case CUNE:
          Format(instr, "vfcmp.cune.d 'vd, 'vj, 'vk");
          break;
        case SUNE:
          Format(instr, "vfcmp.sune.d 'vd, 'vj, 'vk");
          break;
        default:
          UNREACHABLE();
      }
      break;
    case VBITSEL_V:
      Format(instr, "vbitsel.v    'vd, 'vj, 'vk, 'va");
      break;
    case VSHUF_B:
      Format(instr, "vshuf.b      'vd, 'vj, 'vk, 'va");
      break;
    case VLDREPL_W:
      Format(instr, "vldrepl.w    'vd, 'rj, 'si10");
      break;
    case VSTELM_W:
      Format(instr, "vstelm.w     'vd, 'rj, 'si8, 'idx2");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp13(Instruction* instr) {
  switch (instr->Bits(31, 19) << 19) {
    case VLDREPL_D:
      Format(instr, "vldrepl.d    'vd, 'rj, 'si9");
      break;
    case VSTELM_D:
      Format(instr, "vstelm.d     'vd, 'rj, 'si8, 'idx1");
      break;
    default:
      UNREACHABLE();
  }
}

#define OP14_2R_U8_LIST(V)                     \
  V(VEXTRINS_D, "vextrins.d   'vd, 'vj, 'ui8") \
  V(VEXTRINS_W, "vextrins.w   'vd, 'vj, 'ui8") \
  V(VEXTRINS_H, "vextrins.h   'vd, 'vj, 'ui8") \
  V(VEXTRINS_B, "vextrins.b   'vd, 'vj, 'ui8") \
  V(VSHUF4I_B, "vshuf4i.b    'vd, 'vj, 'ui8")  \
  V(VSHUF4I_H, "vshuf4i.h    'vd, 'vj, 'ui8")  \
  V(VSHUF4I_W, "vshuf4i.w    'vd, 'vj, 'ui8")  \
  V(VSHUF4I_D, "vshuf4i.d    'vd, 'vj, 'ui8")  \
  V(VBITSELI_B, "vbitseli.b   'vd, 'vj, 'ui8") \
  V(VANDI_B, "vandi.b      'vd, 'vj, 'ui8")    \
  V(VORI_B, "vori.b       'vd, 'vj, 'ui8")     \
  V(VXORI_B, "vxori.b      'vd, 'vj, 'ui8")    \
  V(VNORI_B, "vnori.b      'vd, 'vj, 'ui8")    \
  V(VPERMI_W, "vpermi.w     'vd, 'vj, 'ui8")

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
#define FORMAT(A, B)  \
  case A:             \
    Format(instr, B); \
    break;
      OP14_2R_U8_LIST(FORMAT)
#undef FORMAT
    case VLDI:
      Format(instr, "vldi         'vd, 'si13");
      break;
    default:
      UNREACHABLE();
  }
}
#undef OP14_2R_U8_LIST

#define OP15_2R_U7_LIST(V)                         \
  V(VSRLNI_D_Q, "vsrlni.d.q   'vd, 'vj, 'ui7")     \
  V(VSRLRNI_D_Q, "vsrlrni.d.q  'vd, 'vj, 'ui7")    \
  V(VSSRLNI_D_Q, "vssrlni.d.q  'vd, 'vj, 'ui7")    \
  V(VSSRLNI_DU_Q, "vssrlni.du.q 'vd, 'vj, 'ui7")   \
  V(VSSRLRNI_D_Q, "vssrlrni.d.q 'vd, 'vj, 'ui7")   \
  V(VSSRLRNI_DU_Q, "vssrlrni.du.q 'vd, 'vj, 'ui7") \
  V(VSRANI_D_Q, "vsrani.d.q   'vd, 'vj, 'ui7")     \
  V(VSRARNI_D_Q, "vsrarni.d.q  'vd, 'vj, 'ui7")    \
  V(VSSRANI_D_Q, "vssrani.d.q  'vd, 'vj, 'ui7")    \
  V(VSSRANI_DU_Q, "vssrani.du.q 'vd, 'vj, 'ui7")   \
  V(VSSRARNI_D_Q, "vssrarni.d.q 'vd, 'vj, 'ui7")   \
  V(VSSRARNI_DU_Q, "vssrarni.du.q 'vd, 'vj, 'ui7")

void Decoder::DecodeTypekOp15(Instruction* instr) {
  switch (instr->Bits(31, 17) << 17) {
#define FORMAT(A, B)  \
  case A:             \
    Format(instr, B); \
    break;
    OP15_2R_U7_LIST(FORMAT)
#undef FORMAT
    default:
      UNREACHABLE();
  }
}
#undef OP15_2R_U7_LIST

#define OP16_2R_U6_LIST(V)                         \
  V(VROTRI_D, "vrotri.d     'vd, 'vj, 'ui6")       \
  V(VSRLRI_D, "vsrlri.d     'vd, 'vj, 'ui6")       \
  V(VSRARI_D, "vsrari.d     'vd, 'vj, 'ui6")       \
  V(VBITCLRI_D, "vbitclri.d   'vd, 'vj, 'ui6")     \
  V(VBITSETI_D, "vbitseti.d   'vd, 'vj, 'ui6")     \
  V(VBITREVI_D, "vbitrevi.d   'vd, 'vj, 'ui6")     \
  V(VSAT_D, "vsat.d       'vd, 'vj, 'ui6")         \
  V(VSAT_DU, "vsat.du      'vd, 'vj, 'ui6")        \
  V(VSLLI_D, "vslli.d      'vd, 'vj, 'ui6")        \
  V(VSRLI_D, "vsrli.d      'vd, 'vj, 'ui6")        \
  V(VSRAI_D, "vsrai.d      'vd, 'vj, 'ui6")        \
  V(VSRLNI_W_D, "vsrlni.w.d   'vd, 'vj, 'ui6")     \
  V(VSRLRNI_W_D, "vsrlrni.w.d  'vd, 'vj, 'ui6")    \
  V(VSSRLNI_W_D, "vssrlni.w.d  'vd, 'vj, 'ui6")    \
  V(VSSRLNI_WU_D, "vssrlni.wu.d 'vd, 'vj, 'ui6")   \
  V(VSSRLRNI_W_D, "vssrlrni.w.d 'vd, 'vj, 'ui6")   \
  V(VSSRLRNI_WU_D, "vssrlrni.wu.d 'vd, 'vj, 'ui6") \
  V(VSRANI_W_D, "vsrani.w.d   'vd, 'vj, 'ui6")     \
  V(VSRARNI_W_D, "vsrarni.w.d  'vd, 'vj, 'ui6")    \
  V(VSSRANI_W_D, "vssrani.w.d  'vd, 'vj, 'ui6")    \
  V(VSSRANI_WU_D, "vssrani.wu.d 'vd, 'vj, 'ui6")   \
  V(VSSRARNI_W_D, "vssrarni.w.d 'vd, 'vj, 'ui6")   \
  V(VSSRARNI_WU_D, "vssrarni.wu.d 'vd, 'vj, 'ui6")

void Decoder::DecodeTypekOp16(Instruction* instr) {
  switch (instr->Bits(31, 16) << 16) {
#define FORMAT(A, B)  \
  case A:             \
    Format(instr, B); \
    break;
    OP16_2R_U6_LIST(FORMAT)
#undef FORMAT
    default:
      UNREACHABLE();
  }
}
#undef OP16_2R_U6_LIST

#define OP17_3R_LIST(V)                               \
  V(ADD_W, "add.w        'rd, 'rj, 'rk")              \
  V(ADD_D, "add.d        'rd, 'rj, 'rk")              \
  V(SUB_W, "sub.w        'rd, 'rj, 'rk")              \
  V(SUB_D, "sub.d        'rd, 'rj, 'rk")              \
  V(SLT, "slt          'rd, 'rj, 'rk")                \
  V(SLTU, "sltu         'rd, 'rj, 'rk")               \
  V(MASKEQZ, "maskeqz      'rd, 'rj, 'rk")            \
  V(MASKNEZ, "masknez      'rd, 'rj, 'rk")            \
  V(NOR, "nor          'rd, 'rj, 'rk")                \
  V(AND, "and          'rd, 'rj, 'rk")                \
  V(OR, "or           'rd, 'rj, 'rk")                 \
  V(XOR, "xor          'rd, 'rj, 'rk")                \
  V(ORN, "orn          'rd, 'rj, 'rk")                \
  V(ANDN, "andn         'rd, 'rj, 'rk")               \
  V(SLL_W, "sll.w        'rd, 'rj, 'rk")              \
  V(SRL_W, "srl.w        'rd, 'rj, 'rk")              \
  V(SRA_W, "sra.w        'rd, 'rj, 'rk")              \
  V(SLL_D, "sll.d        'rd, 'rj, 'rk")              \
  V(SRL_D, "srl.d        'rd, 'rj, 'rk")              \
  V(SRA_D, "sra.d        'rd, 'rj, 'rk")              \
  V(ROTR_D, "rotr.d       'rd, 'rj, 'rk")             \
  V(ROTR_W, "rotr.w       'rd, 'rj, 'rk")             \
  V(MUL_W, "mul.w        'rd, 'rj, 'rk")              \
  V(MULH_W, "mulh.w       'rd, 'rj, 'rk")             \
  V(MULH_WU, "mulh.wu      'rd, 'rj, 'rk")            \
  V(MUL_D, "mul.d        'rd, 'rj, 'rk")              \
  V(MULH_D, "mulh.d       'rd, 'rj, 'rk")             \
  V(MULH_DU, "mulh.du      'rd, 'rj, 'rk")            \
  V(MULW_D_W, "mulw.d.w     'rd, 'rj, 'rk")           \
  V(MULW_D_WU, "mulw.d.wu    'rd, 'rj, 'rk")          \
  V(DIV_W, "div.w        'rd, 'rj, 'rk")              \
  V(MOD_W, "mod.w        'rd, 'rj, 'rk")              \
  V(DIV_WU, "div.wu       'rd, 'rj, 'rk")             \
  V(MOD_WU, "mod.wu       'rd, 'rj, 'rk")             \
  V(DIV_D, "div.d        'rd, 'rj, 'rk")              \
  V(MOD_D, "mod.d        'rd, 'rj, 'rk")              \
  V(DIV_DU, "div.du       'rd, 'rj, 'rk")             \
  V(MOD_DU, "mod.du       'rd, 'rj, 'rk")             \
                                                      \
  V(FADD_S, "fadd.s       'fd, 'fj, 'fk")             \
  V(FADD_D, "fadd.d       'fd, 'fj, 'fk")             \
  V(FSUB_S, "fsub.s       'fd, 'fj, 'fk")             \
  V(FSUB_D, "fsub.d       'fd, 'fj, 'fk")             \
  V(FMUL_S, "fmul.s       'fd, 'fj, 'fk")             \
  V(FMUL_D, "fmul.d       'fd, 'fj, 'fk")             \
  V(FDIV_S, "fdiv.s       'fd, 'fj, 'fk")             \
  V(FDIV_D, "fdiv.d       'fd, 'fj, 'fk")             \
  V(FMAX_S, "fmax.s       'fd, 'fj, 'fk")             \
  V(FMAX_D, "fmax.d       'fd, 'fj, 'fk")             \
  V(FMIN_S, "fmin.s       'fd, 'fj, 'fk")             \
  V(FMIN_D, "fmin.d       'fd, 'fj, 'fk")             \
  V(FMAXA_S, "fmaxa.s      'fd, 'fj, 'fk")            \
  V(FMAXA_D, "fmaxa.d      'fd, 'fj, 'fk")            \
  V(FMINA_S, "fmina.s      'fd, 'fj, 'fk")            \
  V(FMINA_D, "fmina.d      'fd, 'fj, 'fk")            \
                                                      \
  V(LDX_B, "ldx.b        'rd, 'rj, 'rk")              \
  V(LDX_H, "ldx.h        'rd, 'rj, 'rk")              \
  V(LDX_W, "ldx.w        'rd, 'rj, 'rk")              \
  V(LDX_D, "ldx.d        'rd, 'rj, 'rk")              \
  V(STX_B, "stx.b        'rd, 'rj, 'rk")              \
  V(STX_H, "stx.h        'rd, 'rj, 'rk")              \
  V(STX_W, "stx.w        'rd, 'rj, 'rk")              \
  V(STX_D, "stx.d        'rd, 'rj, 'rk")              \
  V(LDX_BU, "ldx.bu       'rd, 'rj, 'rk")             \
  V(LDX_HU, "ldx.hu       'rd, 'rj, 'rk")             \
  V(LDX_WU, "ldx.wu       'rd, 'rj, 'rk")             \
  V(FLDX_S, "fldx.s       'fd, 'rj, 'rk")             \
  V(FLDX_D, "fldx.d       'fd, 'rj, 'rk")             \
  V(FSTX_S, "fstx.s       'fd, 'rj, 'rk")             \
  V(FSTX_D, "fstx.d       'fd, 'rj, 'rk")             \
                                                      \
  V(AMSWAP_W, "amswap.w     'rd, 'rk, 'rj")           \
  V(AMSWAP_D, "amswap.d     'rd, 'rk, 'rj")           \
  V(AMADD_W, "amadd.w      'rd, 'rk, 'rj")            \
  V(AMADD_D, "amadd.d      'rd, 'rk, 'rj")            \
  V(AMAND_W, "amand.w      'rd, 'rk, 'rj")            \
  V(AMAND_D, "amand.d      'rd, 'rk, 'rj")            \
  V(AMOR_W, "amor.w       'rd, 'rk, 'rj")             \
  V(AMOR_D, "amor.d       'rd, 'rk, 'rj")             \
  V(AMXOR_W, "amxor.w      'rd, 'rk, 'rj")            \
  V(AMXOR_D, "amxor.d      'rd, 'rk, 'rj")            \
  V(AMMAX_W, "ammax.w      'rd, 'rk, 'rj")            \
  V(AMMAX_D, "ammax.d      'rd, 'rk, 'rj")            \
  V(AMMIN_W, "ammin.w      'rd, 'rk, 'rj")            \
  V(AMMIN_D, "ammin.d      'rd, 'rk, 'rj")            \
  V(AMMAX_WU, "ammax.wu     'rd, 'rk, 'rj")           \
  V(AMMAX_DU, "ammax.du     'rd, 'rk, 'rj")           \
  V(AMMIN_WU, "ammin.wu     'rd, 'rk, 'rj")           \
  V(AMMIN_DU, "ammin.du     'rd, 'rk, 'rj")           \
  V(AMSWAP_DB_W, "amswap_db.w  'rd, 'rk, 'rj")        \
  V(AMSWAP_DB_D, "amswap_db.d  'rd, 'rk, 'rj")        \
  V(AMADD_DB_W, "amadd_db.w   'rd, 'rk, 'rj")         \
  V(AMADD_DB_D, "amadd_db.d   'rd, 'rk, 'rj")         \
  V(AMAND_DB_W, "amand_db.w   'rd, 'rk, 'rj")         \
  V(AMAND_DB_D, "amand_db.d   'rd, 'rk, 'rj")         \
  V(AMOR_DB_W, "amor_db.w    'rd, 'rk, 'rj")          \
  V(AMOR_DB_D, "amor_db.d    'rd, 'rk, 'rj")          \
  V(AMXOR_DB_W, "amxor_db.w   'rd, 'rk, 'rj")         \
  V(AMXOR_DB_D, "amxor_db.d   'rd, 'rk, 'rj")         \
  V(AMMAX_DB_W, "ammax_db.w   'rd, 'rk, 'rj")         \
  V(AMMAX_DB_D, "ammax_db.d   'rd, 'rk, 'rj")         \
  V(AMMIN_DB_W, "ammin_db.w   'rd, 'rk, 'rj")         \
  V(AMMIN_DB_D, "ammin_db.d   'rd, 'rk, 'rj")         \
  V(AMMAX_DB_WU, "ammax_db.wu  'rd, 'rk, 'rj")        \
  V(AMMAX_DB_DU, "ammax_db.du  'rd, 'rk, 'rj")        \
  V(AMMIN_DB_WU, "ammin_db.wu  'rd, 'rk, 'rj")        \
  V(AMMIN_DB_DU, "ammin_db.du  'rd, 'rk, 'rj")        \
  V(FSCALEB_S, "fscaleb.s    'fd, 'fj, 'fk")          \
  V(FSCALEB_D, "fscaleb.d    'fd, 'fj, 'fk")          \
  V(FCOPYSIGN_S, "fcopysign.s  'fd, 'fj, 'fk")        \
  V(FCOPYSIGN_D, "fcopysign.d  'fd, 'fj, 'fk")        \
  V(VLDX, "vldx         'vd, 'rj, 'rk")               \
  V(VSTX, "vstx         'vd, 'rj, 'rk")               \
  V(VSEQ_B, "vseq.b       'vd, 'vj, 'vk")             \
  V(VSEQ_H, "vseq.h       'vd, 'vj, 'vk")             \
  V(VSEQ_W, "vseq.w       'vd, 'vj, 'vk")             \
  V(VSEQ_D, "vseq.d       'vd, 'vj, 'vk")             \
  V(VSLE_B, "vsle.b       'vd, 'vj, 'vk")             \
  V(VSLE_H, "vsle.h       'vd, 'vj, 'vk")             \
  V(VSLE_W, "vsle.w       'vd, 'vj, 'vk")             \
  V(VSLE_D, "vsle.d       'vd, 'vj, 'vk")             \
  V(VSLE_BU, "vsle.bu      'vd, 'vj, 'vk")            \
  V(VSLE_HU, "vsle.hu      'vd, 'vj, 'vk")            \
  V(VSLE_WU, "vsle.wu      'vd, 'vj, 'vk")            \
  V(VSLE_DU, "vsle.du      'vd, 'vj, 'vk")            \
  V(VSLT_B, "vslt.b       'vd, 'vj, 'vk")             \
  V(VSLT_H, "vslt.h       'vd, 'vj, 'vk")             \
  V(VSLT_W, "vslt.w       'vd, 'vj, 'vk")             \
  V(VSLT_D, "vslt.d       'vd, 'vj, 'vk")             \
  V(VSLT_BU, "vslt.bu      'vd, 'vj, 'vk")            \
  V(VSLT_HU, "vslt.hu      'vd, 'vj, 'vk")            \
  V(VSLT_WU, "vslt.wu      'vd, 'vj, 'vk")            \
  V(VSLT_DU, "vslt.du      'vd, 'vj, 'vk")            \
  V(VADD_B, "vadd.b       'vd, 'vj, 'vk")             \
  V(VADD_H, "vadd.h       'vd, 'vj, 'vk")             \
  V(VADD_W, "vadd.w       'vd, 'vj, 'vk")             \
  V(VADD_D, "vadd.d       'vd, 'vj, 'vk")             \
  V(VSUB_B, "vsub.b       'vd, 'vj, 'vk")             \
  V(VSUB_H, "vsub.h       'vd, 'vj, 'vk")             \
  V(VSUB_W, "vsub.w       'vd, 'vj, 'vk")             \
  V(VSUB_D, "vsub.d       'vd, 'vj, 'vk")             \
  V(VADDWEV_H_B, "vaddwev.h.b  'vd, 'vj, 'vk")        \
  V(VADDWEV_W_H, "vaddwev.w.h  'vd, 'vj, 'vk")        \
  V(VADDWEV_D_W, "vaddwev.d.w  'vd, 'vj, 'vk")        \
  V(VADDWEV_Q_D, "vaddwev.q.d  'vd, 'vj, 'vk")        \
  V(VSUBWEV_H_B, "vsubwev.h.b  'vd, 'vj, 'vk")        \
  V(VSUBWEV_W_H, "vsubwev.w.h  'vd, 'vj, 'vk")        \
  V(VSUBWEV_D_W, "vsubwev.d.w  'vd, 'vj, 'vk")        \
  V(VSUBWEV_Q_D, "vsubwev.q.d  'vd, 'vj, 'vk")        \
  V(VADDWOD_H_B, "vaddwod.h.b  'vd, 'vj, 'vk")        \
  V(VADDWOD_W_H, "vaddwod.w.h  'vd, 'vj, 'vk")        \
  V(VADDWOD_D_W, "vaddwod.d.w  'vd, 'vj, 'vk")        \
  V(VADDWOD_Q_D, "vaddwod.q.d  'vd, 'vj, 'vk")        \
  V(VSUBWOD_H_B, "vsubwod.h.b  'vd, 'vj, 'vk")        \
  V(VSUBWOD_W_H, "vsubwod.w.h  'vd, 'vj, 'vk")        \
  V(VSUBWOD_D_W, "vsubwod.d.w  'vd, 'vj, 'vk")        \
  V(VSUBWOD_Q_D, "vsubwod.q.d  'vd, 'vj, 'vk")        \
  V(VADDWEV_H_BU, "vaddwev.h.bu 'vd, 'vj, 'vk")       \
  V(VADDWEV_W_HU, "vaddwev.w.hu 'vd, 'vj, 'vk")       \
  V(VADDWEV_D_WU, "vaddwev.d.wu 'vd, 'vj, 'vk")       \
  V(VADDWEV_Q_DU, "vaddwev.q.du 'vd, 'vj, 'vk")       \
  V(VSUBWEV_H_BU, "vsubwev.h.bu 'vd, 'vj, 'vk")       \
  V(VSUBWEV_W_HU, "vsubwev.w.hu 'vd, 'vj, 'vk")       \
  V(VSUBWEV_D_WU, "vsubwev.d.wu 'vd, 'vj, 'vk")       \
  V(VSUBWEV_Q_DU, "vsubwev.q.du 'vd, 'vj, 'vk")       \
  V(VADDWOD_H_BU, "vaddwod.h.bu 'vd, 'vj, 'vk")       \
  V(VADDWOD_W_HU, "vaddwod.w.hu 'vd, 'vj, 'vk")       \
  V(VADDWOD_D_WU, "vaddwod.d.wu 'vd, 'vj, 'vk")       \
  V(VADDWOD_Q_DU, "vaddwod.q.du 'vd, 'vj, 'vk")       \
  V(VSUBWOD_H_BU, "vsubwod.h.bu 'vd, 'vj, 'vk")       \
  V(VSUBWOD_W_HU, "vsubwod.w.hu 'vd, 'vj, 'vk")       \
  V(VSUBWOD_D_WU, "vsubwod.d.wu 'vd, 'vj, 'vk")       \
  V(VSUBWOD_Q_DU, "vsubwod.q.du 'vd, 'vj, 'vk")       \
  V(VADDWEV_H_BU_B, "vaddwev.h.bu.b 'vd, 'vj, 'vk")   \
  V(VADDWEV_W_HU_H, "vaddwev.w.hu.h 'vd, 'vj, 'vk")   \
  V(VADDWEV_D_WU_W, "vaddwev.d.wu.w 'vd, 'vj, 'vk")   \
  V(VADDWEV_Q_DU_D, "vaddwev.q.du.d 'vd, 'vj, 'vk")   \
  V(VADDWOD_H_BU_B, "vaddwod.h.bu.b 'vd, 'vj, 'vk")   \
  V(VADDWOD_W_HU_H, "vaddwod.w.hu.h 'vd, 'vj, 'vk")   \
  V(VADDWOD_D_WU_W, "vaddwod.d.wu.w 'vd, 'vj, 'vk")   \
  V(VADDWOD_Q_DU_D, "vaddwod.q.du.d 'vd, 'vj, 'vk")   \
  V(VSADD_B, "vsadd.b      'vd, 'vj, 'vk")            \
  V(VSADD_H, "vsadd.h      'vd, 'vj, 'vk")            \
  V(VSADD_W, "vsadd.w      'vd, 'vj, 'vk")            \
  V(VSADD_D, "vsadd.d      'vd, 'vj, 'vk")            \
  V(VSSUB_B, "vssub.b      'vd, 'vj, 'vk")            \
  V(VSSUB_H, "vssub.h      'vd, 'vj, 'vk")            \
  V(VSSUB_W, "vssub.w      'vd, 'vj, 'vk")            \
  V(VSSUB_D, "vssub.d      'vd, 'vj, 'vk")            \
  V(VSADD_BU, "vsadd.bu     'vd, 'vj, 'vk")           \
  V(VSADD_HU, "vsadd.hu     'vd, 'vj, 'vk")           \
  V(VSADD_WU, "vsadd.wu     'vd, 'vj, 'vk")           \
  V(VSADD_DU, "vsadd.du     'vd, 'vj, 'vk")           \
  V(VSSUB_BU, "vssub.bu     'vd, 'vj, 'vk")           \
  V(VSSUB_HU, "vssub.hu     'vd, 'vj, 'vk")           \
  V(VSSUB_WU, "vssub.wu     'vd, 'vj, 'vk")           \
  V(VSSUB_DU, "vssub.du     'vd, 'vj, 'vk")           \
  V(VHADDW_H_B, "vhaddw.h.b   'vd, 'vj, 'vk")         \
  V(VHADDW_W_H, "vhaddw.w.h   'vd, 'vj, 'vk")         \
  V(VHADDW_D_W, "vhaddw.d.w   'vd, 'vj, 'vk")         \
  V(VHADDW_Q_D, "vhaddw.q.d   'vd, 'vj, 'vk")         \
  V(VHSUBW_H_B, "vhsubw.h.b   'vd, 'vj, 'vk")         \
  V(VHSUBW_W_H, "vhsubw.w.h   'vd, 'vj, 'vk")         \
  V(VHSUBW_D_W, "vhsubw.d.w   'vd, 'vj, 'vk")         \
  V(VHSUBW_Q_D, "vhsubw.q.d   'vd, 'vj, 'vk")         \
  V(VHADDW_HU_BU, "vhaddw.hu.bu 'vd, 'vj, 'vk")       \
  V(VHADDW_WU_HU, "vhaddw.wu.hu 'vd, 'vj, 'vk")       \
  V(VHADDW_DU_WU, "vhaddw.du.wu 'vd, 'vj, 'vk")       \
  V(VHADDW_QU_DU, "vhaddw.qu.du 'vd, 'vj, 'vk")       \
  V(VHSUBW_HU_BU, "vhsubw.hu.bu 'vd, 'vj, 'vk")       \
  V(VHSUBW_WU_HU, "vhsubw.wu.hu 'vd, 'vj, 'vk")       \
  V(VHSUBW_DU_WU, "vhsubw.du.wu 'vd, 'vj, 'vk")       \
  V(VHSUBW_QU_DU, "vhsubw.qu.du 'vd, 'vj, 'vk")       \
  V(VADDA_B, "vadda.b      'vd, 'vj, 'vk")            \
  V(VADDA_H, "vadda.h      'vd, 'vj, 'vk")            \
  V(VADDA_W, "vadda.w      'vd, 'vj, 'vk")            \
  V(VADDA_D, "vadda.d      'vd, 'vj, 'vk")            \
  V(VABSD_B, "vabsd.b      'vd, 'vj, 'vk")            \
  V(VABSD_H, "vabsd.h      'vd, 'vj, 'vk")            \
  V(VABSD_W, "vabsd.w      'vd, 'vj, 'vk")            \
  V(VABSD_D, "vabsd.d      'vd, 'vj, 'vk")            \
  V(VABSD_BU, "vabsd.bu     'vd, 'vj, 'vk")           \
  V(VABSD_HU, "vabsd.hu     'vd, 'vj, 'vk")           \
  V(VABSD_WU, "vabsd.wu     'vd, 'vj, 'vk")           \
  V(VABSD_DU, "vabsd.du     'vd, 'vj, 'vk")           \
  V(VAVG_B, "vavg.b       'vd, 'vj, 'vk")             \
  V(VAVG_H, "vavg.h       'vd, 'vj, 'vk")             \
  V(VAVG_W, "vavg.w       'vd, 'vj, 'vk")             \
  V(VAVG_D, "vavg.d       'vd, 'vj, 'vk")             \
  V(VAVG_BU, "vavg.bu      'vd, 'vj, 'vk")            \
  V(VAVG_HU, "vavg.hu      'vd, 'vj, 'vk")            \
  V(VAVG_WU, "vavg.wu      'vd, 'vj, 'vk")            \
  V(VAVG_DU, "vavg.du      'vd, 'vj, 'vk")            \
  V(VAVGR_B, "vavgr.b      'vd, 'vj, 'vk")            \
  V(VAVGR_H, "vavgr.h      'vd, 'vj, 'vk")            \
  V(VAVGR_W, "vavgr.w      'vd, 'vj, 'vk")            \
  V(VAVGR_D, "vavgr.d      'vd, 'vj, 'vk")            \
  V(VAVGR_BU, "vavgr.bu     'vd, 'vj, 'vk")           \
  V(VAVGR_HU, "vavgr.hu     'vd, 'vj, 'vk")           \
  V(VAVGR_WU, "vavgr.wu     'vd, 'vj, 'vk")           \
  V(VAVGR_DU, "vavgr.du     'vd, 'vj, 'vk")           \
  V(VMAX_B, "vmax.b       'vd, 'vj, 'vk")             \
  V(VMAX_H, "vmax.h       'vd, 'vj, 'vk")             \
  V(VMAX_W, "vmax.w       'vd, 'vj, 'vk")             \
  V(VMAX_D, "vmax.d       'vd, 'vj, 'vk")             \
  V(VMIN_B, "vmin.b       'vd, 'vj, 'vk")             \
  V(VMIN_H, "vmin.h       'vd, 'vj, 'vk")             \
  V(VMIN_W, "vmin.w       'vd, 'vj, 'vk")             \
  V(VMIN_D, "vmin.d       'vd, 'vj, 'vk")             \
  V(VMAX_BU, "vmax.bu      'vd, 'vj, 'vk")            \
  V(VMAX_HU, "vmax.hu      'vd, 'vj, 'vk")            \
  V(VMAX_WU, "vmax.wu      'vd, 'vj, 'vk")            \
  V(VMAX_DU, "vmax.du      'vd, 'vj, 'vk")            \
  V(VMIN_BU, "vmin.bu      'vd, 'vj, 'vk")            \
  V(VMIN_HU, "vmin.hu      'vd, 'vj, 'vk")            \
  V(VMIN_WU, "vmin.wu      'vd, 'vj, 'vk")            \
  V(VMIN_DU, "vmin.du      'vd, 'vj, 'vk")            \
  V(VMUL_B, "vmul.b       'vd, 'vj, 'vk")             \
  V(VMUL_H, "vmul.h       'vd, 'vj, 'vk")             \
  V(VMUL_W, "vmul.w       'vd, 'vj, 'vk")             \
  V(VMUL_D, "vmul.d       'vd, 'vj, 'vk")             \
  V(VMUH_B, "vmuh.b       'vd, 'vj, 'vk")             \
  V(VMUH_H, "vmuh.h       'vd, 'vj, 'vk")             \
  V(VMUH_W, "vmuh.w       'vd, 'vj, 'vk")             \
  V(VMUH_D, "vmuh.d       'vd, 'vj, 'vk")             \
  V(VMUH_BU, "vmuh.bu      'vd, 'vj, 'vk")            \
  V(VMUH_HU, "vmuh.hu      'vd, 'vj, 'vk")            \
  V(VMUH_WU, "vmuh.wu      'vd, 'vj, 'vk")            \
  V(VMUH_DU, "vmuh.du      'vd, 'vj, 'vk")            \
  V(VMULWEV_H_B, "vmulwev.h.b  'vd, 'vj, 'vk")        \
  V(VMULWEV_W_H, "vmulwev.w.h  'vd, 'vj, 'vk")        \
  V(VMULWEV_D_W, "vmulwev.d.w  'vd, 'vj, 'vk")        \
  V(VMULWEV_Q_D, "vmulwev.q.d  'vd, 'vj, 'vk")        \
  V(VMULWOD_H_B, "vmulwod.h.b  'vd, 'vj, 'vk")        \
  V(VMULWOD_W_H, "vmulwod.w.h  'vd, 'vj, 'vk")        \
  V(VMULWOD_D_W, "vmulwod.d.w  'vd, 'vj, 'vk")        \
  V(VMULWOD_Q_D, "vmulwod.q.d  'vd, 'vj, 'vk")        \
  V(VMULWEV_H_BU, "vmulwev.h.bu 'vd, 'vj, 'vk")       \
  V(VMULWEV_W_HU, "vmulwev.w.hu 'vd, 'vj, 'vk")       \
  V(VMULWEV_D_WU, "vmulwev.d.wu 'vd, 'vj, 'vk")       \
  V(VMULWEV_Q_DU, "vmulwev.q.du 'vd, 'vj, 'vk")       \
  V(VMULWOD_H_BU, "vmulwod.h.bu 'vd, 'vj, 'vk")       \
  V(VMULWOD_W_HU, "vmulwod.w.hu 'vd, 'vj, 'vk")       \
  V(VMULWOD_D_WU, "vmulwod.d.wu 'vd, 'vj, 'vk")       \
  V(VMULWOD_Q_DU, "vmulwod.q.du 'vd, 'vj, 'vk")       \
  V(VMULWEV_H_BU_B, "vmulwev.h.bu.b 'vd, 'vj, 'vk")   \
  V(VMULWEV_W_HU_H, "vmulwev.w.hu.h 'vd, 'vj, 'vk")   \
  V(VMULWEV_D_WU_W, "vmulwev.d.wu.w 'vd, 'vj, 'vk")   \
  V(VMULWEV_Q_DU_D, "vmulwev.q.du.d 'vd, 'vj, 'vk")   \
  V(VMULWOD_H_BU_B, "vmulwod.h.bu.b 'vd, 'vj, 'vk")   \
  V(VMULWOD_W_HU_H, "vmulwod.w.hu.h 'vd, 'vj, 'vk")   \
  V(VMULWOD_D_WU_W, "vmulwod.d.wu.w 'vd, 'vj, 'vk")   \
  V(VMULWOD_Q_DU_D, "vmulwod.q.du.d 'vd, 'vj, 'vk")   \
  V(VMADD_B, "vmadd.b      'vd, 'vj, 'vk")            \
  V(VMADD_H, "vmadd.h      'vd, 'vj, 'vk")            \
  V(VMADD_W, "vmadd.w      'vd, 'vj, 'vk")            \
  V(VMADD_D, "vmadd.d      'vd, 'vj, 'vk")            \
  V(VMSUB_B, "vmsub.b      'vd, 'vj, 'vk")            \
  V(VMSUB_H, "vmsub.h      'vd, 'vj, 'vk")            \
  V(VMSUB_W, "vmsub.w      'vd, 'vj, 'vk")            \
  V(VMSUB_D, "vmsub.d      'vd, 'vj, 'vk")            \
  V(VMADDWEV_H_B, "vmaddwev.h.b 'vd, 'vj, 'vk")       \
  V(VMADDWEV_W_H, "vmaddwev.w.h 'vd, 'vj, 'vk")       \
  V(VMADDWEV_D_W, "vmaddwev.d.w 'vd, 'vj, 'vk")       \
  V(VMADDWEV_Q_D, "vmaddwev.q.d 'vd, 'vj, 'vk")       \
  V(VMADDWOD_H_B, "vmaddwod.h.b 'vd, 'vj, 'vk")       \
  V(VMADDWOD_W_H, "vmaddwod.w.h 'vd, 'vj, 'vk")       \
  V(VMADDWOD_D_W, "vmaddwod.d.w 'vd, 'vj, 'vk")       \
  V(VMADDWOD_Q_D, "vmaddwod.q.d 'vd, 'vj, 'vk")       \
  V(VMADDWEV_H_BU, "vmaddwev.h.bu 'vd, 'vj, 'vk")     \
  V(VMADDWEV_W_HU, "vmaddwev.w.hu 'vd, 'vj, 'vk")     \
  V(VMADDWEV_D_WU, "vmaddwev.d.wu 'vd, 'vj, 'vk")     \
  V(VMADDWEV_Q_DU, "vmaddwev.q.du 'vd, 'vj, 'vk")     \
  V(VMADDWOD_H_BU, "vmaddwod.h.bu 'vd, 'vj, 'vk")     \
  V(VMADDWOD_W_HU, "vmaddwod.w.hu 'vd, 'vj, 'vk")     \
  V(VMADDWOD_D_WU, "vmaddwod.d.wu 'vd, 'vj, 'vk")     \
  V(VMADDWOD_Q_DU, "vmaddwod.q.du 'vd, 'vj, 'vk")     \
  V(VMADDWEV_H_BU_B, "vmaddwev.h.bu.b 'vd, 'vj, 'vk") \
  V(VMADDWEV_W_HU_H, "vmaddwev.w.hu.h 'vd, 'vj, 'vk") \
  V(VMADDWEV_D_WU_W, "vmaddwev.d.wu.w 'vd, 'vj, 'vk") \
  V(VMADDWEV_Q_DU_D, "vmaddwev.q.du.d 'vd, 'vj, 'vk") \
  V(VMADDWOD_H_BU_B, "vmaddwod.h.bu.b 'vd, 'vj, 'vk") \
  V(VMADDWOD_W_HU_H, "vmaddwod.w.hu.h 'vd, 'vj, 'vk") \
  V(VMADDWOD_D_WU_W, "vmaddwod.d.wu.w 'vd, 'vj, 'vk") \
  V(VMADDWOD_Q_DU_D, "vmaddwod.q.du.d 'vd, 'vj, 'vk") \
  V(VDIV_B, "vdiv.b       'vd, 'vj, 'vk")             \
  V(VDIV_H, "vdiv.h       'vd, 'vj, 'vk")             \
  V(VDIV_W, "vdiv.w       'vd, 'vj, 'vk")             \
  V(VDIV_D, "vdiv.d       'vd, 'vj, 'vk")             \
  V(VMOD_B, "vmod.b       'vd, 'vj, 'vk")             \
  V(VMOD_H, "vmod.h       'vd, 'vj, 'vk")             \
  V(VMOD_W, "vmod.w       'vd, 'vj, 'vk")             \
  V(VMOD_D, "vmod.d       'vd, 'vj, 'vk")             \
  V(VDIV_BU, "vdiv.bu      'vd, 'vj, 'vk")            \
  V(VDIV_HU, "vdiv.hu      'vd, 'vj, 'vk")            \
  V(VDIV_WU, "vdiv.wu      'vd, 'vj, 'vk")            \
  V(VDIV_DU, "vdiv.du      'vd, 'vj, 'vk")            \
  V(VMOD_BU, "vmod.bu      'vd, 'vj, 'vk")            \
  V(VMOD_HU, "vmod.hu      'vd, 'vj, 'vk")            \
  V(VMOD_WU, "vmod.wu      'vd, 'vj, 'vk")            \
  V(VMOD_DU, "vmod.du      'vd, 'vj, 'vk")            \
  V(VSLL_B, "vsll.b       'vd, 'vj, 'vk")             \
  V(VSLL_H, "vsll.h       'vd, 'vj, 'vk")             \
  V(VSLL_W, "vsll.w       'vd, 'vj, 'vk")             \
  V(VSLL_D, "vsll.d       'vd, 'vj, 'vk")             \
  V(VSRL_B, "vsrl.b       'vd, 'vj, 'vk")             \
  V(VSRL_H, "vsrl.h       'vd, 'vj, 'vk")             \
  V(VSRL_W, "vsrl.w       'vd, 'vj, 'vk")             \
  V(VSRL_D, "vsrl.d       'vd, 'vj, 'vk")             \
  V(VSRA_B, "vsra.b       'vd, 'vj, 'vk")             \
  V(VSRA_H, "vsra.h       'vd, 'vj, 'vk")             \
  V(VSRA_W, "vsra.w       'vd, 'vj, 'vk")             \
  V(VSRA_D, "vsra.d       'vd, 'vj, 'vk")             \
  V(VROTR_B, "vrotr.b      'vd, 'vj, 'vk")            \
  V(VROTR_H, "vrotr.h      'vd, 'vj, 'vk")            \
  V(VROTR_W, "vrotr.w      'vd, 'vj, 'vk")            \
  V(VROTR_D, "vrotr.d      'vd, 'vj, 'vk")            \
  V(VSRLR_B, "vsrlr.b      'vd, 'vj, 'vk")            \
  V(VSRLR_H, "vsrlr.h      'vd, 'vj, 'vk")            \
  V(VSRLR_W, "vsrlr.w      'vd, 'vj, 'vk")            \
  V(VSRLR_D, "vsrlr.d      'vd, 'vj, 'vk")            \
  V(VSRAR_B, "vsrar.b      'vd, 'vj, 'vk")            \
  V(VSRAR_H, "vsrar.h      'vd, 'vj, 'vk")            \
  V(VSRAR_W, "vsrar.w      'vd, 'vj, 'vk")            \
  V(VSRAR_D, "vsrar.d      'vd, 'vj, 'vk")            \
  V(VSRLN_B_H, "vsrln.b.h    'vd, 'vj, 'vk")          \
  V(VSRLN_H_W, "vsrln.h.w    'vd, 'vj, 'vk")          \
  V(VSRLN_W_D, "vsrln.w.d    'vd, 'vj, 'vk")          \
  V(VSRAN_B_H, "vsran.b.h    'vd, 'vj, 'vk")          \
  V(VSRAN_H_W, "vsran.h.w    'vd, 'vj, 'vk")          \
  V(VSRAN_W_D, "vsran.w.d    'vd, 'vj, 'vk")          \
  V(VSRLRN_B_H, "vsrlrn.b.h   'vd, 'vj, 'vk")         \
  V(VSRLRN_H_W, "vsrlrn.h.w   'vd, 'vj, 'vk")         \
  V(VSRLRN_W_D, "vsrlrn.w.d   'vd, 'vj, 'vk")         \
  V(VSRARN_B_H, "vsrarn.b.h   'vd, 'vj, 'vk")         \
  V(VSRARN_H_W, "vsrarn.h.w   'vd, 'vj, 'vk")         \
  V(VSRARN_W_D, "vsrarn.w.d   'vd, 'vj, 'vk")         \
  V(VSSRLN_B_H, "vssrln.b.h   'vd, 'vj, 'vk")         \
  V(VSSRLN_H_W, "vssrln.h.w   'vd, 'vj, 'vk")         \
  V(VSSRLN_W_D, "vssrln.w.d   'vd, 'vj, 'vk")         \
  V(VSSRAN_B_H, "vssran.b.h   'vd, 'vj, 'vk")         \
  V(VSSRAN_H_W, "vssran.h.w   'vd, 'vj, 'vk")         \
  V(VSSRAN_W_D, "vssran.w.d   'vd, 'vj, 'vk")         \
  V(VSSRLRN_B_H, "vssrlrn.b.h  'vd, 'vj, 'vk")        \
  V(VSSRLRN_H_W, "vssrlrn.h.w  'vd, 'vj, 'vk")        \
  V(VSSRLRN_W_D, "vssrlrn.w.d  'vd, 'vj, 'vk")        \
  V(VSSRARN_B_H, "vssrarn.b.h  'vd, 'vj, 'vk")        \
  V(VSSRARN_H_W, "vssrarn.h.w  'vd, 'vj, 'vk")        \
  V(VSSRARN_W_D, "vssrarn.w.d  'vd, 'vj, 'vk")        \
  V(VSSRLN_BU_H, "vssrln.bu.h  'vd, 'vj, 'vk")        \
  V(VSSRLN_HU_W, "vssrln.hu.w  'vd, 'vj, 'vk")        \
  V(VSSRLN_WU_D, "vssrln.wu.d  'vd, 'vj, 'vk")        \
  V(VSSRAN_BU_H, "vssran.bu.h  'vd, 'vj, 'vk")        \
  V(VSSRAN_HU_W, "vssran.hu.w  'vd, 'vj, 'vk")        \
  V(VSSRAN_WU_D, "vssran.wu.d  'vd, 'vj, 'vk")        \
  V(VSSRLRN_BU_H, "vssrlrn.bu.h 'vd, 'vj, 'vk")       \
  V(VSSRLRN_HU_W, "vssrlrn.hu.w 'vd, 'vj, 'vk")       \
  V(VSSRLRN_WU_D, "vssrlrn.wu.d 'vd, 'vj, 'vk")       \
  V(VSSRARN_BU_H, "vssrarn.bu.h 'vd, 'vj, 'vk")       \
  V(VSSRARN_HU_W, "vssrarn.hu.w 'vd, 'vj, 'vk")       \
  V(VSSRARN_WU_D, "vssrarn.wu.d 'vd, 'vj, 'vk")       \
  V(VBITCLR_B, "vbitclr.b    'vd, 'vj, 'vk")          \
  V(VBITCLR_H, "vbitclr.h    'vd, 'vj, 'vk")          \
  V(VBITCLR_W, "vbitclr.w    'vd, 'vj, 'vk")          \
  V(VBITCLR_D, "vbitclr.d    'vd, 'vj, 'vk")          \
  V(VBITSET_B, "vbitset.b    'vd, 'vj, 'vk")          \
  V(VBITSET_H, "vbitset.h    'vd, 'vj, 'vk")          \
  V(VBITSET_W, "vbitset.w    'vd, 'vj, 'vk")          \
  V(VBITSET_D, "vbitset.d    'vd, 'vj, 'vk")          \
  V(VBITREV_B, "vbitrev.b    'vd, 'vj, 'vk")          \
  V(VBITREV_H, "vbitrev.h    'vd, 'vj, 'vk")          \
  V(VBITREV_W, "vbitrev.w    'vd, 'vj, 'vk")          \
  V(VBITREV_D, "vbitrev.d    'vd, 'vj, 'vk")          \
  V(VPACKEV_B, "vpackev.b    'vd, 'vj, 'vk")          \
  V(VPACKEV_H, "vpackev.h    'vd, 'vj, 'vk")          \
  V(VPACKEV_W, "vpackev.w    'vd, 'vj, 'vk")          \
  V(VPACKEV_D, "vpackev.d    'vd, 'vj, 'vk")          \
  V(VPACKOD_B, "vpackod.b    'vd, 'vj, 'vk")          \
  V(VPACKOD_H, "vpackod.h    'vd, 'vj, 'vk")          \
  V(VPACKOD_W, "vpackod.w    'vd, 'vj, 'vk")          \
  V(VPACKOD_D, "vpackod.d    'vd, 'vj, 'vk")          \
  V(VILVL_B, "vilvl.b      'vd, 'vj, 'vk")            \
  V(VILVL_H, "vilvl.h      'vd, 'vj, 'vk")            \
  V(VILVL_W, "vilvl.w      'vd, 'vj, 'vk")            \
  V(VILVL_D, "vilvl.d      'vd, 'vj, 'vk")            \
  V(VILVH_B, "vilvh.b      'vd, 'vj, 'vk")            \
  V(VILVH_H, "vilvh.h      'vd, 'vj, 'vk")            \
  V(VILVH_W, "vilvh.w      'vd, 'vj, 'vk")            \
  V(VILVH_D, "vilvh.d      'vd, 'vj, 'vk")            \
  V(VPICKEV_B, "vpickev.b    'vd, 'vj, 'vk")          \
  V(VPICKEV_H, "vpickev.h    'vd, 'vj, 'vk")          \
  V(VPICKEV_W, "vpickev.w    'vd, 'vj, 'vk")          \
  V(VPICKEV_D, "vpickev.d    'vd, 'vj, 'vk")          \
  V(VPICKOD_B, "vpickod.b    'vd, 'vj, 'vk")          \
  V(VPICKOD_H, "vpickod.h    'vd, 'vj, 'vk")          \
  V(VPICKOD_W, "vpickod.w    'vd, 'vj, 'vk")          \
  V(VPICKOD_D, "vpickod.d    'vd, 'vj, 'vk")          \
  V(VREPLVE_B, "vreplve.b    'vd, 'vj, 'rk")          \
  V(VREPLVE_H, "vreplve.h    'vd, 'vj, 'rk")          \
  V(VREPLVE_W, "vreplve.w    'vd, 'vj, 'rk")          \
  V(VREPLVE_D, "vreplve.d    'vd, 'vj, 'rk")          \
  V(VAND_V, "vand.v       'vd, 'vj, 'vk")             \
  V(VOR_V, "vor.v        'vd, 'vj, 'vk")              \
  V(VXOR_V, "vxor.v       'vd, 'vj, 'vk")             \
  V(VNOR_V, "vnor.v       'vd, 'vj, 'vk")             \
  V(VANDN_V, "vandn.v      'vd, 'vj, 'vk")            \
  V(VORN_V, "vorn.v       'vd, 'vj, 'vk")             \
  V(VFRSTP_B, "vfrstp.b     'vd, 'vj, 'vk")           \
  V(VFRSTP_H, "vfrstp.h     'vd, 'vj, 'vk")           \
  V(VADD_Q, "vadd.q       'vd, 'vj, 'vk")             \
  V(VSUB_Q, "vsub.q       'vd, 'vj, 'vk")             \
  V(VSIGNCOV_B, "vsigncov.b   'vd, 'vj, 'vk")         \
  V(VSIGNCOV_H, "vsigncov.h   'vd, 'vj, 'vk")         \
  V(VSIGNCOV_W, "vsigncov.w   'vd, 'vj, 'vk")         \
  V(VSIGNCOV_D, "vsigncov.d   'vd, 'vj, 'vk")         \
  V(VFADD_S, "vfadd.s      'vd, 'vj, 'vk")            \
  V(VFADD_D, "vfadd.d      'vd, 'vj, 'vk")            \
  V(VFSUB_S, "vfsub.s      'vd, 'vj, 'vk")            \
  V(VFSUB_D, "vfsub.d      'vd, 'vj, 'vk")            \
  V(VFMUL_S, "vfmul.s      'vd, 'vj, 'vk")            \
  V(VFMUL_D, "vfmul.d      'vd, 'vj, 'vk")            \
  V(VFDIV_S, "vfdiv.s      'vd, 'vj, 'vk")            \
  V(VFDIV_D, "vfdiv.d      'vd, 'vj, 'vk")            \
  V(VFMAX_S, "vfmax.s      'vd, 'vj, 'vk")            \
  V(VFMAX_D, "vfmax.d      'vd, 'vj, 'vk")            \
  V(VFMIN_S, "vfmin.s      'vd, 'vj, 'vk")            \
  V(VFMIN_D, "vfmin.d      'vd, 'vj, 'vk")            \
  V(VFMAXA_S, "vfmaxa.s     'vd, 'vj, 'vk")           \
  V(VFMAXA_D, "vfmaxa.d     'vd, 'vj, 'vk")           \
  V(VFMINA_S, "vfmina.s     'vd, 'vj, 'vk")           \
  V(VFMINA_D, "vfmina.d     'vd, 'vj, 'vk")           \
  V(VFCVT_H_S, "vfcvt.h.s    'vd, 'vj, 'vk")          \
  V(VFCVT_S_D, "vfcvt.s.d    'vd, 'vj, 'vk")          \
  V(VFFINT_S_L, "vffint.s.l   'vd, 'vj, 'vk")         \
  V(VFTINT_W_D, "vftint.w.d   'vd, 'vj, 'vk")         \
  V(VFTINTRM_W_D, "vftintrm.w.d 'vd, 'vj, 'vk")       \
  V(VFTINTRP_W_D, "vftintrp.w.d 'vd, 'vj, 'vk")       \
  V(VFTINTRZ_W_D, "vftintrz.w.d 'vd, 'vj, 'vk")       \
  V(VFTINTRNE_W_D, "vftintrne.w.d 'vd, 'vj, 'vk")     \
  V(VSHUF_H, "vshuf.h      'vd, 'vj, 'vk")            \
  V(VSHUF_W, "vshuf.w      'vd, 'vj, 'vk")            \
  V(VSHUF_D, "vshuf.d      'vd, 'vj, 'vk")

#define OP17_2R_I_LIST(V)                          \
  V(VSEQI_B, "vseqi.b      'vd, 'vj, 'si5")        \
  V(VSEQI_H, "vseqi.h      'vd, 'vj, 'si5")        \
  V(VSEQI_W, "vseqi.w      'vd, 'vj, 'si5")        \
  V(VSEQI_D, "vseqi.d      'vd, 'vj, 'si5")        \
  V(VSLEI_B, "vslei.b      'vd, 'vj, 'si5")        \
  V(VSLEI_H, "vslei.h      'vd, 'vj, 'si5")        \
  V(VSLEI_W, "vslei.w      'vd, 'vj, 'si5")        \
  V(VSLEI_D, "vslei.d      'vd, 'vj, 'si5")        \
  V(VSLEI_BU, "vslei.bu     'vd, 'vj, 'ui5")       \
  V(VSLEI_HU, "vslei.hu     'vd, 'vj, 'ui5")       \
  V(VSLEI_WU, "vslei.wu     'vd, 'vj, 'ui5")       \
  V(VSLEI_DU, "vslei.du     'vd, 'vj, 'ui5")       \
  V(VSLTI_B, "vslti.b      'vd, 'vj, 'si5")        \
  V(VSLTI_H, "vslti.h      'vd, 'vj, 'si5")        \
  V(VSLTI_W, "vslti.w      'vd, 'vj, 'si5")        \
  V(VSLTI_D, "vslti.d      'vd, 'vj, 'si5")        \
  V(VSLTI_BU, "vslti.bu     'vd, 'vj, 'ui5")       \
  V(VSLTI_HU, "vslti.hu     'vd, 'vj, 'ui5")       \
  V(VSLTI_WU, "vslti.wu     'vd, 'vj, 'ui5")       \
  V(VSLTI_DU, "vslti.du     'vd, 'vj, 'ui5")       \
  V(VADDI_BU, "vaddi.bu     'vd, 'vj, 'ui5")       \
  V(VADDI_HU, "vaddi.hu     'vd, 'vj, 'ui5")       \
  V(VADDI_WU, "vaddi.wu     'vd, 'vj, 'ui5")       \
  V(VADDI_DU, "vaddi.du     'vd, 'vj, 'ui5")       \
  V(VSUBI_BU, "vsubi.bu     'vd, 'vj, 'ui5")       \
  V(VSUBI_HU, "vsubi.hu     'vd, 'vj, 'ui5")       \
  V(VSUBI_WU, "vsubi.wu     'vd, 'vj, 'ui5")       \
  V(VSUBI_DU, "vsubi.du     'vd, 'vj, 'ui5")       \
  V(VBSLL_V, "vbsll.v      'vd, 'vj, 'ui5")        \
  V(VBSRL_V, "vbsrl.v      'vd, 'vj, 'ui5")        \
  V(VMAXI_B, "vmaxi.b      'vd, 'vj, 'si5")        \
  V(VMAXI_H, "vmaxi.h      'vd, 'vj, 'si5")        \
  V(VMAXI_W, "vmaxi.w      'vd, 'vj, 'si5")        \
  V(VMAXI_D, "vmaxi.d      'vd, 'vj, 'si5")        \
  V(VMINI_B, "vmini.b      'vd, 'vj, 'si5")        \
  V(VMINI_H, "vmini.h      'vd, 'vj, 'si5")        \
  V(VMINI_W, "vmini.w      'vd, 'vj, 'si5")        \
  V(VMINI_D, "vmini.d      'vd, 'vj, 'si5")        \
  V(VMAXI_BU, "vmaxi.bu     'vd, 'vj, 'ui5")       \
  V(VMAXI_HU, "vmaxi.hu     'vd, 'vj, 'ui5")       \
  V(VMAXI_WU, "vmaxi.wu     'vd, 'vj, 'ui5")       \
  V(VMAXI_DU, "vmaxi.du     'vd, 'vj, 'ui5")       \
  V(VMINI_BU, "vmini.bu     'vd, 'vj, 'ui5")       \
  V(VMINI_HU, "vmini.hu     'vd, 'vj, 'ui5")       \
  V(VMINI_WU, "vmini.wu     'vd, 'vj, 'ui5")       \
  V(VMINI_DU, "vmini.du     'vd, 'vj, 'ui5")       \
  V(VFRSTPI_B, "vfrstpi.b    'vd, 'vj, 'ui5")      \
  V(VFRSTPI_H, "vfrstpi.h    'vd, 'vj, 'ui5")      \
  V(VROTRI_W, "vrotri.w     'vd, 'vj, 'ui5")       \
  V(VSRLRI_W, "vsrlri.w     'vd, 'vj, 'ui5")       \
  V(VSRARI_W, "vsrari.w     'vd, 'vj, 'ui5")       \
  V(VSLLWIL_D_W, "vsllwil.d.w  'vd, 'vj, 'ui5")    \
  V(VSLLWIL_DU_WU, "vsllwil.du.wu 'vd, 'vj, 'ui5") \
  V(VBITCLRI_W, "vbitclri.w   'vd, 'vj, 'ui5")     \
  V(VBITSETI_W, "vbitseti.w   'vd, 'vj, 'ui5")     \
  V(VBITREVI_W, "vbitrevi.w   'vd, 'vj, 'ui5")     \
  V(VSAT_W, "vsat.w       'vd, 'vj, 'ui5")         \
  V(VSAT_WU, "vsat.wu      'vd, 'vj, 'ui5")        \
  V(VSLLI_W, "vslli.w      'vd, 'vj, 'ui5")        \
  V(VSRLI_W, "vsrli.w      'vd, 'vj, 'ui5")        \
  V(VSRAI_W, "vsrai.w      'vd, 'vj, 'ui5")        \
  V(VSRLNI_H_W, "vsrlni.h.w   'vd, 'vj, 'ui5")     \
  V(VSRLRNI_H_W, "vsrlrni.h.w  'vd, 'vj, 'ui5")    \
  V(VSSRLNI_H_W, "vssrlni.h.w  'vd, 'vj, 'ui5")    \
  V(VSSRLNI_HU_W, "vssrlni.hu.w 'vd, 'vj, 'ui5")   \
  V(VSSRLRNI_H_W, "vssrlrni.h.w 'vd, 'vj, 'ui5")   \
  V(VSSRLRNI_HU_W, "vssrlrni.hu.w 'vd, 'vj, 'ui5") \
  V(VSRANI_H_W, "vsrani.h.w   'vd, 'vj, 'ui5")     \
  V(VSRARNI_H_W, "vsrarni.h.w  'vd, 'vj, 'ui5")    \
  V(VSSRANI_H_W, "vssrani.h.w  'vd, 'vj, 'ui5")    \
  V(VSSRANI_HU_W, "vssrani.hu.w 'vd, 'vj, 'ui5")   \
  V(VSSRARNI_H_W, "vssrarni.h.w 'vd, 'vj, 'ui5")   \
  V(VSSRARNI_HU_W, "vssrarni.hu.w 'vd, 'vj, 'ui5")

int Decoder::DecodeTypekOp17(Instruction* instr) {
  switch (instr->Bits(31, 15) << 15) {
#define FORMAT(A, B)  \
  case A:             \
    Format(instr, B); \
    break;
    OP17_3R_LIST(FORMAT)
    OP17_2R_I_LIST(FORMAT)
#undef FORMAT
    case BREAK:
      return DecodeBreakInstr(instr);
    case DBAR:
      Format(instr, "dbar         'hint15");
      break;
    case IBAR:
      Format(instr, "ibar         'hint15");
      break;
    default:
      UNREACHABLE();
  }
  return kInstrSize;
}
#undef OP17_3R_LIST
#undef OP17_2R_I_LIST

#define OP18_2R_U4(V)                              \
  V(VROTRI_H, "vrotri.h     'vd, 'vj, 'ui4")       \
  V(VSRLRI_H, "vsrlri.h     'vd, 'vj, 'ui4")       \
  V(VSRARI_H, "vsrari.h     'vd, 'vj, 'ui4")       \
  V(VINSGR2VR_B, "vinsgr2vr.b  'vd, 'rj, 'ui4")    \
  V(VPICKVE2GR_B, "vpickve2gr.b 'rd, 'vj, 'ui4")   \
  V(VPICKVE2GR_BU, "vpickve2gr.bu 'rd, 'vj, 'ui4") \
  V(VREPLVEI_B, "vreplvei.b   'vd, 'vj, 'ui4")     \
  V(VSLLWIL_W_H, "vsllwil.w.h  'vd, 'vj, 'ui4")    \
  V(VSLLWIL_WU_HU, "vsllwil.wu.hu 'vd, 'vj, 'ui4") \
  V(VBITCLRI_H, "vbitclri.h   'vd, 'vj, 'ui4")     \
  V(VBITSETI_H, "vbitseti.h   'vd, 'vj, 'ui4")     \
  V(VBITREVI_H, "vbitrevi.h   'vd, 'vj, 'ui4")     \
  V(VSAT_H, "vsat.h       'vd, 'vj, 'ui4")         \
  V(VSAT_HU, "vsat.hu      'vd, 'vj, 'ui4")        \
  V(VSLLI_H, "vslli.h      'vd, 'vj, 'ui4")        \
  V(VSRLI_H, "vsrli.h      'vd, 'vj, 'ui4")        \
  V(VSRAI_H, "vsrai.h      'vd, 'vj, 'ui4")        \
  V(VSRLNI_B_H, "vsrlni.b.h   'vd, 'vj, 'ui4")     \
  V(VSRLRNI_B_H, "vsrlrni.b.h  'vd, 'vj, 'ui4")    \
  V(VSSRLNI_B_H, "vssrlni.b.h  'vd, 'vj, 'ui4")    \
  V(VSSRLNI_BU_H, "vssrlni.bu.h 'vd, 'vj, 'ui4")   \
  V(VSSRLRNI_B_H, "vssrlrni.b.h 'vd, 'vj, 'ui4")   \
  V(VSSRLRNI_BU_H, "vssrlrni.bu.h 'vd, 'vj, 'ui4") \
  V(VSRANI_B_H, "vsrani.b.h   'vd, 'vj, 'ui4")     \
  V(VSRARNI_B_H, "vsrarni.b.h  'vd, 'vj, 'ui4")    \
  V(VSSRANI_B_H, "vssrani.b.h  'vd, 'vj, 'ui4")    \
  V(VSSRANI_BU_H, "vssrani.bu.h 'vd, 'vj, 'ui4")   \
  V(VSSRARNI_B_H, "vssrarni.b.h 'vd, 'vj, 'ui4")   \
  V(VSSRARNI_BU_H, "vssrarni.bu.h 'vd, 'vj, 'ui4")

void Decoder::DecodeTypekOp18(Instruction* instr) {
  switch (instr->Bits(31, 14) << 14) {
#define FORMAT(A, B)  \
  case A:             \
    Format(instr, B); \
    break;
    OP18_2R_U4(FORMAT)
#undef FORMAT
    default:
      UNREACHABLE();
  }
}
#undef OP18_2R_U4

#define OP19_2R_U3(V)                              \
  V(VROTRI_B, "vrotri.b     'vd, 'vj, 'ui3")       \
  V(VSRLRI_B, "vsrlri.b     'vd, 'vj, 'ui3")       \
  V(VSRARI_B, "vsrari.b     'vd, 'vj, 'ui3")       \
  V(VINSGR2VR_H, "vinsgr2vr.h  'vd, 'rj, 'ui3")    \
  V(VPICKVE2GR_H, "vpickve2gr.h 'rd, 'vj, 'ui3")   \
  V(VPICKVE2GR_HU, "vpickve2gr.hu 'rd, 'vj, 'ui3") \
  V(VREPLVEI_H, "vreplvei.h   'vd, 'vj, 'ui3")     \
  V(VSLLWIL_H_B, "vsllwil.h.b  'vd, 'vj, 'ui3")    \
  V(VSLLWIL_HU_BU, "vsllwil.hu.bu 'vd, 'vj, 'ui3") \
  V(VBITCLRI_B, "vbitclri.b   'vd, 'vj, 'ui3")     \
  V(VBITSETI_B, "vbitseti.b   'vd, 'vj, 'ui3")     \
  V(VBITREVI_B, "vbitrevi.b   'vd, 'vj, 'ui3")     \
  V(VSAT_B, "vsat.b       'vd, 'vj, 'ui3")         \
  V(VSAT_BU, "vsat.bu      'vd, 'vj, 'ui3")        \
  V(VSLLI_B, "vslli.b      'vd, 'vj, 'ui3")        \
  V(VSRLI_B, "vsrli.b      'vd, 'vj, 'ui3")        \
  V(VSRAI_B, "vsrai.b      'vd, 'vj, 'ui3")

void Decoder::DecodeTypekOp19(Instruction* instr) {
  switch (instr->Bits(31, 13) << 13) {
#define FORMAT(A, B)  \
  case A:             \
    Format(instr, B); \
    break;
    OP19_2R_U3(FORMAT)
#undef FORMAT
    default:
      UNREACHABLE();
  }
}
#undef OP19_2R_U3

void Decoder::DecodeTypekOp20(Instruction* instr) {
  switch (instr->Bits(31, 12) << 12) {
    case VINSGR2VR_W:
      Format(instr, "vinsgr2vr.w  'vd, 'rj, 'ui2");
      break;
    case VPICKVE2GR_W:
      Format(instr, "vpickve2gr.w 'rd, 'vj, 'ui2");
      break;
    case VPICKVE2GR_WU:
      Format(instr, "vpickve2gr.wu 'rd, 'vj, 'ui2");
      break;
    case VREPLVEI_W:
      Format(instr, "vreplvei.w   'vd, 'vj, 'ui2");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypekOp21(Instruction* instr) {
  switch (instr->Bits(31, 11) << 11) {
    case VINSGR2VR_D:
      Format(instr, "vinsgr2vr.d  'vd, 'rj, 'ui1");
      break;
    case VPICKVE2GR_D:
      Format(instr, "vpickve2gr.d 'rd, 'vj, 'ui1");
      break;
    case VPICKVE2GR_DU:
      Format(instr, "vpickve2gr.du 'rd, 'vj, 'ui1");
      break;
    case VREPLVEI_D:
      Format(instr, "vreplvei.d   'vd, 'vj, 'ui1");
      break;
    default:
      UNREACHABLE();
  }
}

#define OP22_2R_LIST(V)                        \
  V(CLZ_W, "clz.w        'rd, 'rj")            \
  V(CTZ_W, "ctz.w        'rd, 'rj")            \
  V(CLZ_D, "clz.d        'rd, 'rj")            \
  V(CTZ_D, "ctz.d        'rd, 'rj")            \
  V(REVB_2H, "revb.2h      'rd, 'rj")          \
  V(REVB_4H, "revb.4h      'rd, 'rj")          \
  V(REVB_2W, "revb.2w      'rd, 'rj")          \
  V(REVB_D, "revb.d       'rd, 'rj")           \
  V(REVH_2W, "revh.2w      'rd, 'rj")          \
  V(REVH_D, "revh.d       'rd, 'rj")           \
  V(BITREV_4B, "bitrev.4b    'rd, 'rj")        \
  V(BITREV_8B, "bitrev.8b    'rd, 'rj")        \
  V(BITREV_W, "bitrev.w     'rd, 'rj")         \
  V(BITREV_D, "bitrev.d     'rd, 'rj")         \
  V(EXT_W_B, "ext.w.b      'rd, 'rj")          \
  V(EXT_W_H, "ext.w.h      'rd, 'rj")          \
  V(CLO_W, "clo.w        'rd, 'rj")            \
  V(CTO_W, "cto.w        'rd, 'rj")            \
  V(CLO_D, "clo.d        'rd, 'rj")            \
  V(CTO_D, "cto.d        'rd, 'rj")            \
                                               \
  V(FABS_S, "fabs.s       'fd, 'fj")           \
  V(FABS_D, "fabs.d       'fd, 'fj")           \
  V(FNEG_S, "fneg.s       'fd, 'fj")           \
  V(FNEG_D, "fneg.d       'fd, 'fj")           \
  V(FSQRT_S, "fsqrt.s      'fd, 'fj")          \
  V(FSQRT_D, "fsqrt.d      'fd, 'fj")          \
  V(FMOV_S, "fmov.s       'fd, 'fj")           \
  V(FMOV_D, "fmov.d       'fd, 'fj")           \
  V(FCVT_S_D, "fcvt.s.d     'fd, 'fj")         \
  V(FCVT_D_S, "fcvt.d.s     'fd, 'fj")         \
  V(FTINTRM_W_S, "ftintrm.w.s  'fd, 'fj")      \
  V(FTINTRM_W_D, "ftintrm.w.d  'fd, 'fj")      \
  V(FTINTRM_L_S, "ftintrm.l.s  'fd, 'fj")      \
  V(FTINTRM_L_D, "ftintrm.l.d  'fd, 'fj")      \
  V(FTINTRP_W_S, "ftintrp.w.s  'fd, 'fj")      \
  V(FTINTRP_W_D, "ftintrp.w.d  'fd, 'fj")      \
  V(FTINTRP_L_S, "ftintrp.l.s  'fd, 'fj")      \
  V(FTINTRP_L_D, "ftintrp.l.d  'fd, 'fj")      \
  V(FTINTRZ_W_S, "ftintrz.w.s  'fd, 'fj")      \
  V(FTINTRZ_W_D, "ftintrz.w.d  'fd, 'fj")      \
  V(FTINTRZ_L_S, "ftintrz.l.s  'fd, 'fj")      \
  V(FTINTRZ_L_D, "ftintrz.l.d  'fd, 'fj")      \
  V(FTINTRNE_W_S, "ftintrne.w.s 'fd, 'fj")     \
  V(FTINTRNE_W_D, "ftintrne.w.d 'fd, 'fj")     \
  V(FTINTRNE_L_S, "ftintrne.l.s 'fd, 'fj")     \
  V(FTINTRNE_L_D, "ftintrne.l.d 'fd, 'fj")     \
  V(FTINT_W_S, "ftint.w.s    'fd, 'fj")        \
  V(FTINT_W_D, "ftint.w.d    'fd, 'fj")        \
  V(FTINT_L_S, "ftint.l.s    'fd, 'fj")        \
  V(FTINT_L_D, "ftint.l.d    'fd, 'fj")        \
  V(FFINT_S_W, "ffint.s.w    'fd, 'fj")        \
  V(FFINT_S_L, "ffint.s.l    'fd, 'fj")        \
  V(FFINT_D_W, "ffint.d.w    'fd, 'fj")        \
  V(FFINT_D_L, "ffint.d.l    'fd, 'fj")        \
  V(FRINT_S, "frint.s      'fd, 'fj")          \
  V(FRINT_D, "frint.d      'fd, 'fj")          \
  V(FRECIP_S, "frecip.s     'fd, 'fj")         \
  V(FRECIP_D, "frecip.d     'fd, 'fj")         \
  V(FRSQRT_S, "frsqrt.s     'fd, 'fj")         \
  V(FRSQRT_D, "frsqrt.d     'fd, 'fj")         \
  V(FCLASS_S, "fclass.s     'fd, 'fj")         \
  V(FCLASS_D, "fclass.d     'fd, 'fj")         \
  V(FLOGB_S, "flogb.s      'fd, 'fj")          \
  V(FLOGB_D, "flogb.d      'fd, 'fj")          \
                                               \
  V(MOVGR2FR_W, "movgr2fr.w   'fd, 'rj")       \
  V(MOVGR2FR_D, "movgr2fr.d   'fd, 'rj")       \
  V(MOVGR2FRH_W, "movgr2frh.w  'fd, 'rj")      \
  V(MOVFR2GR_S, "movfr2gr.s   'rd, 'fj")       \
  V(MOVFR2GR_D, "movfr2gr.d   'rd, 'fj")       \
  V(MOVFRH2GR_S, "movfrh2gr.s  'rd, 'fj")      \
  V(MOVGR2FCSR, "movgr2fcsr   fcsr, 'rj")      \
  V(MOVFCSR2GR, "movfcsr2gr   'rd, fcsr")      \
  V(MOVFR2CF, "movfr2cf     fcc'cd, 'fj")      \
  V(MOVCF2FR, "movcf2fr     'fd, fcc'cj")      \
  V(MOVGR2CF, "movgr2cf     fcc'cd, 'rj")      \
  V(MOVCF2GR, "movcf2gr     'rd, fcc'cj")      \
                                               \
  V(VCLO_B, "vclo.b       'vd, 'vj")           \
  V(VCLO_H, "vclo.h       'vd, 'vj")           \
  V(VCLO_W, "vclo.w       'vd, 'vj")           \
  V(VCLO_D, "vclo.d       'vd, 'vj")           \
  V(VCLZ_B, "vclz.b       'vd, 'vj")           \
  V(VCLZ_H, "vclz.h       'vd, 'vj")           \
  V(VCLZ_W, "vclz.w       'vd, 'vj")           \
  V(VCLZ_D, "vclz.d       'vd, 'vj")           \
  V(VPCNT_B, "vpcnt.b      'vd, 'vj")          \
  V(VPCNT_H, "vpcnt.h      'vd, 'vj")          \
  V(VPCNT_W, "vpcnt.w      'vd, 'vj")          \
  V(VPCNT_D, "vpcnt.d      'vd, 'vj")          \
  V(VNEG_B, "vneg.b       'vd, 'vj")           \
  V(VNEG_H, "vneg.h       'vd, 'vj")           \
  V(VNEG_W, "vneg.w       'vd, 'vj")           \
  V(VNEG_D, "vneg.d       'vd, 'vj")           \
  V(VMSKLTZ_B, "vmskltz.b    'vd, 'vj")        \
  V(VMSKLTZ_H, "vmskltz.h    'vd, 'vj")        \
  V(VMSKLTZ_W, "vmskltz.w    'vd, 'vj")        \
  V(VMSKLTZ_D, "vmskltz.d    'vd, 'vj")        \
  V(VMSKGEZ_B, "vmskgez.b    'vd, 'vj")        \
  V(VMSKNZ_B, "vmsknz.b     'vd, 'vj")         \
  V(VFLOGB_S, "vflogb.s     'vd, 'vj")         \
  V(VFLOGB_D, "vflogb.d     'vd, 'vj")         \
  V(VFCLASS_S, "vfclass.s    'vd, 'vj")        \
  V(VFCLASS_D, "vfclass.d    'vd, 'vj")        \
  V(VFSQRT_S, "vfsqrt.s     'vd, 'vj")         \
  V(VFSQRT_D, "vfsqrt.d     'vd, 'vj")         \
  V(VFRECIP_S, "vfrecip.s    'vd, 'vj")        \
  V(VFRECIP_D, "vfrecip.d    'vd, 'vj")        \
  V(VFRSQRT_S, "vfrsqrt.s    'vd, 'vj")        \
  V(VFRSQRT_D, "vfrsqrt.d    'vd, 'vj")        \
  V(VFRINT_S, "vfrint.s     'vd, 'vj")         \
  V(VFRINT_D, "vfrint.d     'vd, 'vj")         \
  V(VFRINTRM_S, "vfrintrm.s   'vd, 'vj")       \
  V(VFRINTRM_D, "vfrintrm.d   'vd, 'vj")       \
  V(VFRINTRP_S, "vfrintrp.s   'vd, 'vj")       \
  V(VFRINTRP_D, "vfrintrp.d   'vd, 'vj")       \
  V(VFRINTRZ_S, "vfrintrz.s   'vd, 'vj")       \
  V(VFRINTRZ_D, "vfrintrz.d   'vd, 'vj")       \
  V(VFRINTRNE_S, "vfrintrne.s  'vd, 'vj")      \
  V(VFRINTRNE_D, "vfrintrne.d  'vd, 'vj")      \
  V(VFCVTL_S_H, "vfcvtl.s.h   'vd, 'vj")       \
  V(VFCVTH_S_H, "vfcvth.s.h   'vd, 'vj")       \
  V(VFCVTL_D_S, "vfcvtl.d.s   'vd, 'vj")       \
  V(VFCVTH_D_S, "vfcvth.d.s   'vd, 'vj")       \
  V(VFFINT_S_W, "vffint.s.w   'vd, 'vj")       \
  V(VFFINT_S_WU, "vffint.s.wu  'vd, 'vj")      \
  V(VFFINT_D_L, "vffint.d.l   'vd, 'vj")       \
  V(VFFINT_D_LU, "vffint.d.lu  'vd, 'vj")      \
  V(VFFINTL_D_W, "vffintl.d.w  'vd, 'vj")      \
  V(VFFINTH_D_W, "vffinth.d.w  'vd, 'vj")      \
  V(VFTINT_W_S, "vftint.w.s   'vd, 'vj")       \
  V(VFTINT_L_D, "vftint.l.d   'vd, 'vj")       \
  V(VFTINTRM_W_S, "vftintrm.w.s 'vd, 'vj")     \
  V(VFTINTRM_L_D, "vftintrm.l.d 'vd, 'vj")     \
  V(VFTINTRP_W_S, "vftintrp.w.s 'vd, 'vj")     \
  V(VFTINTRP_L_D, "vftintrp.l.d 'vd, 'vj")     \
  V(VFTINTRZ_W_S, "vftintrz.w.s 'vd, 'vj")     \
  V(VFTINTRZ_L_D, "vftintrz.l.d 'vd, 'vj")     \
  V(VFTINTRNE_W_S, "vftintrne.w.s 'vd, 'vj")   \
  V(VFTINTRNE_L_D, "vftintrne.l.d 'vd, 'vj")   \
  V(VFTINT_WU_S, "vftint.wu.s  'vd, 'vj")      \
  V(VFTINT_LU_D, "vftint.lu.d  'vd, 'vj")      \
  V(VFTINTRZ_WU_S, "vftintrz.wu.s 'vd, 'vj")   \
  V(VFTINTRZ_LU_D, "vftintrz.lu.d 'vd, 'vj")   \
  V(VFTINTL_L_S, "vftintl.l.s  'vd, 'vj")      \
  V(VFTINTH_L_S, "vftinth.l.s  'vd, 'vj")      \
  V(VFTINTRML_L_S, "vftintrml.l.s 'vd, 'vj")   \
  V(VFTINTRMH_L_S, "vftintrmh.l.s 'vd, 'vj")   \
  V(VFTINTRPL_L_S, "vftintrpl.l.s 'vd, 'vj")   \
  V(VFTINTRPH_L_S, "vftintrph.l.s 'vd, 'vj")   \
  V(VFTINTRZL_L_S, "vftintrzl.l.s 'vd, 'vj")   \
  V(VFTINTRZH_L_S, "vftintrzh.l.s 'vd, 'vj")   \
  V(VFTINTRNEL_L_S, "vftintrnel.l.s 'vd, 'vj") \
  V(VFTINTRNEH_L_S, "vftintrneh.l.s 'vd, 'vj") \
  V(VEXTH_H_B, "vexth.h.b    'vd, 'vj")        \
  V(VEXTH_W_H, "vexth.w.h    'vd, 'vj")        \
  V(VEXTH_D_W, "vexth.d.w    'vd, 'vj")        \
  V(VEXTH_Q_D, "vexth.q.d    'vd, 'vj")        \
  V(VEXTH_HU_BU, "vexth.hu.bu  'vd, 'vj")      \
  V(VEXTH_WU_HU, "vexth.wu.hu  'vd, 'vj")      \
  V(VEXTH_DU_WU, "vexth.du.wu  'vd, 'vj")      \
  V(VEXTH_QU_DU, "vexth.qu.du  'vd, 'vj")      \
                                               \
  V(VREPLGR2VR_B, "vreplgr2vr.b 'vd, 'rj")     \
  V(VREPLGR2VR_H, "vreplgr2vr.h 'vd, 'rj")     \
  V(VREPLGR2VR_W, "vreplgr2vr.w 'vd, 'rj")     \
  V(VREPLGR2VR_D, "vreplgr2vr.d 'vd, 'rj")     \
  V(VEXTL_Q_D, "vextl.q.d    'vd, 'vj")        \
  V(VEXTL_QU_DU, "vextl.qu.du  'vd, 'vj")

void Decoder::DecodeTypekOp22(Instruction* instr) {
  switch (instr->Bits(31, 10) << 10) {
#define FORMAT(A, B)  \
  case A:             \
    Format(instr, B); \
    break;
    OP22_2R_LIST(FORMAT)
#undef FORMAT
    case VSETEQZ_V:
      Format(instr, "vseteqz.v    fcc'cd, 'vj");
      break;
    case VSETNEZ_V:
      Format(instr, "vsetnez.v    fcc'cd, 'vj");
      break;
    case VSETANYEQZ_B:
      Format(instr, "vsetanyeqz.b fcc'cd, 'vj");
      break;
    case VSETANYEQZ_H:
      Format(instr, "vsetanyeqz.h fcc'cd, 'vj");
      break;
    case VSETANYEQZ_W:
      Format(instr, "vsetanyeqz.w fcc'cd, 'vj");
      break;
    case VSETANYEQZ_D:
      Format(instr, "vsetanyeqz.d fcc'cd, 'vj");
      break;
    case VSETALLNEZ_B:
      Format(instr, "vsetallnez.b fcc'cd, 'vj");
      break;
    case VSETALLNEZ_H:
      Format(instr, "vsetallnez.h fcc'cd, 'vj");
      break;
    case VSETALLNEZ_W:
      Format(instr, "vsetallnez.w fcc'cd, 'vj");
      break;
    case VSETALLNEZ_D:
      Format(instr, "vsetallnez.d fcc'cd, 'vj");
      break;
    default:
      UNREACHABLE();
  }
}

#undef OP22_2R_LIST

int Decoder::InstructionDecode(uint8_t* instr_ptr) {
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
    case Instruction::kOp11Type: {
      DecodeTypekOp11(instr);
      break;
    }
    case Instruction::kOp12Type: {
      DecodeTypekOp12(instr);
      break;
    }
    case Instruction::kOp13Type: {
      DecodeTypekOp13(instr);
      break;
    }
    case Instruction::kOp14Type: {
      DecodeTypekOp14(instr);
      break;
    }
    case Instruction::kOp15Type: {
      DecodeTypekOp15(instr);
      break;
    }
    case Instruction::kOp16Type: {
      DecodeTypekOp16(instr);
      break;
    }
    case Instruction::kOp17Type: {
      DecodeTypekOp17(instr);
      break;
    }
    case Instruction::kOp18Type: {
      DecodeTypekOp18(instr);
      break;
    }
    case Instruction::kOp19Type: {
      DecodeTypekOp19(instr);
      break;
    }
    case Instruction::kOp20Type: {
      DecodeTypekOp20(instr);
      break;
    }
    case Instruction::kOp21Type: {
      DecodeTypekOp21(instr);
      break;
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

const char* NameConverter::NameOfAddress(uint8_t* addr) const {
  v8::base::SNPrintF(tmp_buffer_, "%p", static_cast<void*>(addr));
  return tmp_buffer_.begin();
}

const char* NameConverter::NameOfConstant(uint8_t* addr) const {
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

const char* NameConverter::NameInCode(uint8_t* addr) const {
  // The default name converter is called for unknown code. So we will not try
  // to access any memory.
  return "";
}

//------------------------------------------------------------------------------

int Disassembler::InstructionDecode(v8::base::Vector<char> buffer,
                                    uint8_t* instruction) {
  v8::internal::Decoder d(converter_, buffer);
  return d.InstructionDecode(instruction);
}

int Disassembler::ConstantPoolSizeAt(uint8_t* instruction) { return -1; }

void Disassembler::Disassemble(FILE* f, uint8_t* begin, uint8_t* end,
                               UnimplementedOpcodeAction unimplemented_action) {
  NameConverter converter;
  Disassembler d(converter, unimplemented_action);
  for (uint8_t* pc = begin; pc < end;) {
    v8::base::EmbeddedVector<char, 128> buffer;
    buffer[0] = '\0';
    uint8_t* prev_pc = pc;
    pc += d.InstructionDecode(buffer, pc);
    v8::internal::PrintF(f, "%p    %08x      %s\n", static_cast<void*>(prev_pc),
                         *reinterpret_cast<int32_t*>(prev_pc), buffer.begin());
  }
}

#undef STRING_STARTS_WITH

}  // namespace disasm

#endif  // V8_TARGET_ARCH_LOONG64

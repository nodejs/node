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
  void PrintMSARegister(int wreg);
  void PrintFPUStatusRegister(int freg);
  void PrintMSAControlRegister(int creg);
  void PrintRs(Instruction* instr);
  void PrintRt(Instruction* instr);
  void PrintRd(Instruction* instr);
  void PrintFs(Instruction* instr);
  void PrintFt(Instruction* instr);
  void PrintFd(Instruction* instr);
  void PrintSa(Instruction* instr);
  void PrintLsaSa(Instruction* instr);
  void PrintSd(Instruction* instr);
  void PrintSs1(Instruction* instr);
  void PrintSs2(Instruction* instr);
  void PrintBc(Instruction* instr);
  void PrintCc(Instruction* instr);
  void PrintBp2(Instruction* instr);
  void PrintFunction(Instruction* instr);
  void PrintSecondaryField(Instruction* instr);
  void PrintUImm9(Instruction* instr);
  void PrintSImm9(Instruction* instr);
  void PrintUImm16(Instruction* instr);
  void PrintSImm16(Instruction* instr);
  void PrintXImm16(Instruction* instr);
  void PrintPCImm16(Instruction* instr, int delta_pc, int n_bits);
  void PrintXImm18(Instruction* instr);
  void PrintSImm18(Instruction* instr);
  void PrintXImm19(Instruction* instr);
  void PrintSImm19(Instruction* instr);
  void PrintXImm21(Instruction* instr);
  void PrintSImm21(Instruction* instr);
  void PrintPCImm21(Instruction* instr, int delta_pc, int n_bits);
  void PrintXImm26(Instruction* instr);
  void PrintSImm26(Instruction* instr);
  void PrintPCImm26(Instruction* instr, int delta_pc, int n_bits);
  void PrintPCImm26(Instruction* instr);
  void PrintCode(Instruction* instr);   // For break and trap instructions.
  void PrintFormat(Instruction* instr);  // For floating format postfix.
  void PrintMsaDataFormat(Instruction* instr);
  void PrintMsaXImm8(Instruction* instr);
  void PrintMsaImm8(Instruction* instr);
  void PrintMsaImm5(Instruction* instr);
  void PrintMsaSImm5(Instruction* instr);
  void PrintMsaSImm10(Instruction* instr, bool is_mi10 = false);
  void PrintMsaImmBit(Instruction* instr);
  void PrintMsaImmElm(Instruction* instr);
  void PrintMsaCopy(Instruction* instr);
  // Printing of instruction name.
  void PrintInstructionName(Instruction* instr);

  // Handle formatting of instructions and their options.
  int FormatRegister(Instruction* instr, const char* option);
  int FormatFPURegister(Instruction* instr, const char* option);
  int FormatMSARegister(Instruction* instr, const char* option);
  int FormatOption(Instruction* instr, const char* option);
  void Format(Instruction* instr, const char* format);
  void Unknown(Instruction* instr);


  // Each of these functions decodes one particular instruction type.
  bool DecodeTypeRegisterRsType(Instruction* instr);
  void DecodeTypeRegisterSRsType(Instruction* instr);
  void DecodeTypeRegisterDRsType(Instruction* instr);
  void DecodeTypeRegisterLRsType(Instruction* instr);
  void DecodeTypeRegisterWRsType(Instruction* instr);
  void DecodeTypeRegisterSPECIAL(Instruction* instr);
  void DecodeTypeRegisterSPECIAL2(Instruction* instr);
  void DecodeTypeRegisterSPECIAL3(Instruction* instr);
  void DecodeTypeRegister(Instruction* instr);
  void DecodeTypeImmediate(Instruction* instr);
  void DecodeTypeImmediateSPECIAL3(Instruction* instr);
  void DecodeTypeJump(Instruction* instr);
  void DecodeTypeMsaI8(Instruction* instr);
  void DecodeTypeMsaI5(Instruction* instr);
  void DecodeTypeMsaI10(Instruction* instr);
  void DecodeTypeMsaELM(Instruction* instr);
  void DecodeTypeMsaBIT(Instruction* instr);
  void DecodeTypeMsaMI10(Instruction* instr);
  void DecodeTypeMsa3R(Instruction* instr);
  void DecodeTypeMsa3RF(Instruction* instr);
  void DecodeTypeMsaVec(Instruction* instr);
  void DecodeTypeMsa2R(Instruction* instr);
  void DecodeTypeMsa2RF(Instruction* instr);

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

void Decoder::PrintMSARegister(int wreg) { Print(MSARegisters::Name(wreg)); }

void Decoder::PrintFPUStatusRegister(int freg) {
  switch (freg) {
    case kFCSRRegister:
      Print("FCSR");
      break;
    default:
      Print(converter_.NameOfXMMRegister(freg));
  }
}

void Decoder::PrintMSAControlRegister(int creg) {
  switch (creg) {
    case kMSAIRRegister:
      Print("MSAIR");
      break;
    case kMSACSRRegister:
      Print("MSACSR");
      break;
    default:
      Print("no_msacreg");
  }
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


// Print the integer value of the sa field of a lsa instruction.
void Decoder::PrintLsaSa(Instruction* instr) {
  int sa = instr->LsaSaValue() + 1;
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


void Decoder::PrintBp2(Instruction* instr) {
  int bp2 = instr->Bp2Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", bp2);
}

// Print 9-bit unsigned immediate value.
void Decoder::PrintUImm9(Instruction* instr) {
  int32_t imm = instr->Imm9Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%u", imm);
}

// Print 9-bit signed immediate value.
void Decoder::PrintSImm9(Instruction* instr) {
  int32_t imm = ((instr->Imm9Value()) << 23) >> 23;
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm);
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


// Print absoulte address for 16-bit offset or immediate value.
// The absolute address is calculated according following expression:
//      PC + delta_pc + (offset << n_bits)
void Decoder::PrintPCImm16(Instruction* instr, int delta_pc, int n_bits) {
  int16_t offset = instr->Imm16Value();
  out_buffer_pos_ +=
      SNPrintF(out_buffer_ + out_buffer_pos_, "%s",
               converter_.NameOfAddress(reinterpret_cast<byte*>(instr) +
                                        delta_pc + (offset << n_bits)));
}


// Print 18-bit signed immediate value.
void Decoder::PrintSImm18(Instruction* instr) {
  int32_t imm =
      ((instr->Imm18Value()) << (32 - kImm18Bits)) >> (32 - kImm18Bits);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm);
}


// Print 18-bit hexa immediate value.
void Decoder::PrintXImm18(Instruction* instr) {
  int32_t imm = instr->Imm18Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 19-bit hexa immediate value.
void Decoder::PrintXImm19(Instruction* instr) {
  int32_t imm = instr->Imm19Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 19-bit signed immediate value.
void Decoder::PrintSImm19(Instruction* instr) {
  int32_t imm19 = instr->Imm19Value();
  // set sign
  imm19 <<= (32 - kImm19Bits);
  imm19 >>= (32 - kImm19Bits);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm19);
}


// Print 21-bit immediate value.
void Decoder::PrintXImm21(Instruction* instr) {
  uint32_t imm = instr->Imm21Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}


// Print 21-bit signed immediate value.
void Decoder::PrintSImm21(Instruction* instr) {
  int32_t imm21 = instr->Imm21Value();
  // set sign
  imm21 <<= (32 - kImm21Bits);
  imm21 >>= (32 - kImm21Bits);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm21);
}


// Print absoulte address for 21-bit offset or immediate value.
// The absolute address is calculated according following expression:
//      PC + delta_pc + (offset << n_bits)
void Decoder::PrintPCImm21(Instruction* instr, int delta_pc, int n_bits) {
  int32_t imm21 = instr->Imm21Value();
  // set sign
  imm21 <<= (32 - kImm21Bits);
  imm21 >>= (32 - kImm21Bits);
  out_buffer_pos_ +=
      SNPrintF(out_buffer_ + out_buffer_pos_, "%s",
               converter_.NameOfAddress(reinterpret_cast<byte*>(instr) +
                                        delta_pc + (imm21 << n_bits)));
}


// Print 26-bit hex immediate value.
void Decoder::PrintXImm26(Instruction* instr) {
  uint32_t target = static_cast<uint32_t>(instr->Imm26Value())
                    << kImmFieldShift;
  target = (reinterpret_cast<uint32_t>(instr) & ~0xFFFFFFF) | target;
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", target);
}


// Print 26-bit signed immediate value.
void Decoder::PrintSImm26(Instruction* instr) {
  int32_t imm26 = instr->Imm26Value();
  // set sign
  imm26 <<= (32 - kImm26Bits);
  imm26 >>= (32 - kImm26Bits);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm26);
}


// Print absoulte address for 26-bit offset or immediate value.
// The absolute address is calculated according following expression:
//      PC + delta_pc + (offset << n_bits)
void Decoder::PrintPCImm26(Instruction* instr, int delta_pc, int n_bits) {
  int32_t imm26 = instr->Imm26Value();
  // set sign
  imm26 <<= (32 - kImm26Bits);
  imm26 >>= (32 - kImm26Bits);
  out_buffer_pos_ +=
      SNPrintF(out_buffer_ + out_buffer_pos_, "%s",
               converter_.NameOfAddress(reinterpret_cast<byte*>(instr) +
                                        delta_pc + (imm26 << n_bits)));
}


// Print absoulte address for 26-bit offset or immediate value.
// The absolute address is calculated according following expression:
//      PC[GPRLEN-1 .. 28] || instr_index26 || 00
void Decoder::PrintPCImm26(Instruction* instr) {
  int32_t imm26 = instr->Imm26Value();
  uint32_t pc_mask = ~0xFFFFFFF;
  uint32_t pc = ((uint32_t)(instr + 1) & pc_mask) | (imm26 << 2);
  out_buffer_pos_ +=
      SNPrintF(out_buffer_ + out_buffer_pos_, "%s",
               converter_.NameOfAddress((reinterpret_cast<byte*>(pc))));
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

void Decoder::PrintMsaXImm8(Instruction* instr) {
  int32_t imm = instr->MsaImm8Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", imm);
}

void Decoder::PrintMsaImm8(Instruction* instr) {
  int32_t imm = instr->MsaImm8Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%u", imm);
}

void Decoder::PrintMsaImm5(Instruction* instr) {
  int32_t imm = instr->MsaImm5Value();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%u", imm);
}

void Decoder::PrintMsaSImm5(Instruction* instr) {
  int32_t imm = instr->MsaImm5Value();
  imm <<= (32 - kMsaImm5Bits);
  imm >>= (32 - kMsaImm5Bits);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm);
}

void Decoder::PrintMsaSImm10(Instruction* instr, bool is_mi10) {
  int32_t imm = is_mi10 ? instr->MsaImmMI10Value() : instr->MsaImm10Value();
  imm <<= (32 - kMsaImm10Bits);
  imm >>= (32 - kMsaImm10Bits);
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", imm);
}

void Decoder::PrintMsaImmBit(Instruction* instr) {
  int32_t m = instr->MsaBitMValue();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%u", m);
}

void Decoder::PrintMsaImmElm(Instruction* instr) {
  int32_t n = instr->MsaElmNValue();
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%u", n);
}

void Decoder::PrintMsaCopy(Instruction* instr) {
  int32_t rd = instr->WdValue();
  int32_t ws = instr->WsValue();
  int32_t n = instr->MsaElmNValue();
  out_buffer_pos_ +=
      SNPrintF(out_buffer_ + out_buffer_pos_, "%s, %s[%u]",
               converter_.NameOfCPURegister(rd), MSARegisters::Name(ws), n);
}

void Decoder::PrintFormat(Instruction* instr) {
  char formatLetter = ' ';
  switch (instr->RsFieldRaw()) {
    case S:
      formatLetter = 's';
      break;
    case D:
      formatLetter = 'd';
      break;
    case W:
      formatLetter = 'w';
      break;
    case L:
      formatLetter = 'l';
      break;
    default:
      UNREACHABLE();
      break;
  }
  PrintChar(formatLetter);
}

void Decoder::PrintMsaDataFormat(Instruction* instr) {
  DCHECK(instr->IsMSAInstr());
  char df = ' ';
  if (instr->IsMSABranchInstr()) {
    switch (instr->RsFieldRaw()) {
      case BZ_V:
      case BNZ_V:
        df = 'v';
        break;
      case BZ_B:
      case BNZ_B:
        df = 'b';
        break;
      case BZ_H:
      case BNZ_H:
        df = 'h';
        break;
      case BZ_W:
      case BNZ_W:
        df = 'w';
        break;
      case BZ_D:
      case BNZ_D:
        df = 'd';
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else {
    char DF[] = {'b', 'h', 'w', 'd'};
    switch (instr->MSAMinorOpcodeField()) {
      case kMsaMinorI5:
      case kMsaMinorI10:
      case kMsaMinor3R:
        df = DF[instr->Bits(22, 21)];
        break;
      case kMsaMinorMI10:
        df = DF[instr->Bits(1, 0)];
        break;
      case kMsaMinorBIT:
        df = DF[instr->MsaBitDf()];
        break;
      case kMsaMinorELM:
        df = DF[instr->MsaElmDf()];
        break;
      case kMsaMinor3RF: {
        uint32_t opcode = instr->InstructionBits() & kMsa3RFMask;
        switch (opcode) {
          case FEXDO:
          case FTQ:
          case MUL_Q:
          case MADD_Q:
          case MSUB_Q:
          case MULR_Q:
          case MADDR_Q:
          case MSUBR_Q:
            df = DF[1 + instr->Bit(21)];
            break;
          default:
            df = DF[2 + instr->Bit(21)];
            break;
        }
      } break;
      case kMsaMinor2R:
        df = DF[instr->Bits(17, 16)];
        break;
      case kMsaMinor2RF:
        df = DF[2 + instr->Bit(16)];
        break;
      default:
        UNREACHABLE();
        break;
    }
  }

  PrintChar(df);
}

// Printing of instruction name.
void Decoder::PrintInstructionName(Instruction* instr) {
}


// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instruction* instr, const char* format) {
  DCHECK_EQ(format[0], 'r');
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
}


// Handle all FPUregister based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatFPURegister(Instruction* instr, const char* format) {
  DCHECK_EQ(format[0], 'f');
  if ((CTC1 == instr->RsFieldRaw()) || (CFC1 == instr->RsFieldRaw())) {
    if (format[1] == 's') {  // 'fs: fs register.
      int reg = instr->FsValue();
      PrintFPUStatusRegister(reg);
      return 2;
    } else if (format[1] == 't') {  // 'ft: ft register.
      int reg = instr->FtValue();
      PrintFPUStatusRegister(reg);
      return 2;
    } else if (format[1] == 'd') {  // 'fd: fd register.
      int reg = instr->FdValue();
      PrintFPUStatusRegister(reg);
      return 2;
    } else if (format[1] == 'r') {  // 'fr: fr register.
      int reg = instr->FrValue();
      PrintFPUStatusRegister(reg);
      return 2;
    }
  } else {
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
  }
  UNREACHABLE();
}

// Handle all MSARegister based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatMSARegister(Instruction* instr, const char* format) {
  DCHECK_EQ(format[0], 'w');
  if (format[1] == 's') {
    int reg = instr->WsValue();
    PrintMSARegister(reg);
    return 2;
  } else if (format[1] == 't') {
    int reg = instr->WtValue();
    PrintMSARegister(reg);
    return 2;
  } else if (format[1] == 'd') {
    int reg = instr->WdValue();
    PrintMSARegister(reg);
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
    case 'c': {   // 'code for break or trap instructions.
      DCHECK(STRING_STARTS_WITH(format, "code"));
      PrintCode(instr);
      return 4;
    }
    case 'i': {   // 'imm16u or 'imm26.
      if (format[3] == '1') {
        if (format[4] == '6') {
          DCHECK(STRING_STARTS_WITH(format, "imm16"));
          switch (format[5]) {
            case 's':
              DCHECK(STRING_STARTS_WITH(format, "imm16s"));
              PrintSImm16(instr);
              break;
            case 'u':
              DCHECK(STRING_STARTS_WITH(format, "imm16u"));
              PrintSImm16(instr);
              break;
            case 'x':
              DCHECK(STRING_STARTS_WITH(format, "imm16x"));
              PrintXImm16(instr);
              break;
            case 'p': {  // The PC relative address.
              DCHECK(STRING_STARTS_WITH(format, "imm16p"));
              int delta_pc = 0;
              int n_bits = 0;
              switch (format[6]) {
                case '4': {
                  DCHECK(STRING_STARTS_WITH(format, "imm16p4"));
                  delta_pc = 4;
                  switch (format[8]) {
                    case '2':
                      DCHECK(STRING_STARTS_WITH(format, "imm16p4s2"));
                      n_bits = 2;
                      PrintPCImm16(instr, delta_pc, n_bits);
                      return 9;
                  }
                }
              }
            }
          }
          return 6;
        } else if (format[4] == '8') {
          DCHECK(STRING_STARTS_WITH(format, "imm18"));
          switch (format[5]) {
            case 's':
              DCHECK(STRING_STARTS_WITH(format, "imm18s"));
              PrintSImm18(instr);
              break;
            case 'x':
              DCHECK(STRING_STARTS_WITH(format, "imm18x"));
              PrintXImm18(instr);
              break;
          }
          return 6;
        } else if (format[4] == '9') {
          DCHECK(STRING_STARTS_WITH(format, "imm19"));
          switch (format[5]) {
            case 's':
              DCHECK(STRING_STARTS_WITH(format, "imm19s"));
              PrintSImm19(instr);
              break;
            case 'x':
              DCHECK(STRING_STARTS_WITH(format, "imm19x"));
              PrintXImm19(instr);
              break;
          }
          return 6;
        } else if (format[4] == '0' && format[5] == 's') {
          DCHECK(STRING_STARTS_WITH(format, "imm10s"));
          if (format[6] == '1') {
            DCHECK(STRING_STARTS_WITH(format, "imm10s1"));
            PrintMsaSImm10(instr, false);
          } else if (format[6] == '2') {
            DCHECK(STRING_STARTS_WITH(format, "imm10s2"));
            PrintMsaSImm10(instr, true);
          }
          return 7;
        }
      } else if (format[3] == '2' && format[4] == '1') {
        DCHECK(STRING_STARTS_WITH(format, "imm21"));
        switch (format[5]) {
          case 's':
            DCHECK(STRING_STARTS_WITH(format, "imm21s"));
            PrintSImm21(instr);
            break;
          case 'x':
            DCHECK(STRING_STARTS_WITH(format, "imm21x"));
            PrintXImm21(instr);
            break;
          case 'p': {  // The PC relative address.
            DCHECK(STRING_STARTS_WITH(format, "imm21p"));
            int delta_pc = 0;
            int n_bits = 0;
            switch (format[6]) {
              case '4': {
                DCHECK(STRING_STARTS_WITH(format, "imm21p4"));
                delta_pc = 4;
                switch (format[8]) {
                  case '2':
                    DCHECK(STRING_STARTS_WITH(format, "imm21p4s2"));
                    n_bits = 2;
                    PrintPCImm21(instr, delta_pc, n_bits);
                    return 9;
                }
              }
            }
          }
        }
        return 6;
      } else if (format[3] == '2' && format[4] == '6') {
        DCHECK(STRING_STARTS_WITH(format, "imm26"));
        switch (format[5]) {
          case 's':
            DCHECK(STRING_STARTS_WITH(format, "imm26s"));
            PrintSImm26(instr);
            break;
          case 'x':
            DCHECK(STRING_STARTS_WITH(format, "imm26x"));
            PrintXImm26(instr);
            break;
          case 'p': {  // The PC relative address.
            DCHECK(STRING_STARTS_WITH(format, "imm26p"));
            int delta_pc = 0;
            int n_bits = 0;
            switch (format[6]) {
              case '4': {
                DCHECK(STRING_STARTS_WITH(format, "imm26p4"));
                delta_pc = 4;
                switch (format[8]) {
                  case '2':
                    DCHECK(STRING_STARTS_WITH(format, "imm26p4s2"));
                    n_bits = 2;
                    PrintPCImm26(instr, delta_pc, n_bits);
                    return 9;
                }
              }
            }
          }
          case 'j': {  // Absolute address for jump instructions.
            DCHECK(STRING_STARTS_WITH(format, "imm26j"));
            PrintPCImm26(instr);
            break;
          }
        }
        return 6;
      } else if (format[3] == '5') {
        DCHECK(STRING_STARTS_WITH(format, "imm5"));
        if (format[4] == 'u') {
          DCHECK(STRING_STARTS_WITH(format, "imm5u"));
          PrintMsaImm5(instr);
        } else if (format[4] == 's') {
          DCHECK(STRING_STARTS_WITH(format, "imm5s"));
          PrintMsaSImm5(instr);
        }
        return 5;
      } else if (format[3] == '8') {
        DCHECK(STRING_STARTS_WITH(format, "imm8"));
        PrintMsaImm8(instr);
        return 4;
      } else if (format[3] == '9') {
        DCHECK(STRING_STARTS_WITH(format, "imm9"));
        if (format[4] == 'u') {
          DCHECK(STRING_STARTS_WITH(format, "imm9u"));
          PrintUImm9(instr);
        } else if (format[4] == 's') {
          DCHECK(STRING_STARTS_WITH(format, "imm9s"));
          PrintSImm9(instr);
        }
        return 5;
      } else if (format[3] == 'b') {
        DCHECK(STRING_STARTS_WITH(format, "immb"));
        PrintMsaImmBit(instr);
        return 4;
      } else if (format[3] == 'e') {
        DCHECK(STRING_STARTS_WITH(format, "imme"));
        PrintMsaImmElm(instr);
        return 4;
      }
    }
    case 'r': {   // 'r: registers.
      return FormatRegister(instr, format);
    }
    case 'f': {   // 'f: FPUregisters.
      return FormatFPURegister(instr, format);
    }
    case 'w': {  // 'w: MSA Register
      return FormatMSARegister(instr, format);
    }
    case 's': {   // 'sa.
      switch (format[1]) {
        case 'a':
          if (format[2] == '2') {
            DCHECK(STRING_STARTS_WITH(format, "sa2"));  // 'sa2
            PrintLsaSa(instr);
            return 3;
          } else {
            DCHECK(STRING_STARTS_WITH(format, "sa"));
            PrintSa(instr);
            return 2;
          }
          break;
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
    case 'b': {
      switch (format[1]) {
        case 'c': {  // 'bc - Special for bc1 cc field.
          DCHECK(STRING_STARTS_WITH(format, "bc"));
          PrintBc(instr);
          return 2;
        }
        case 'p': {
          switch (format[2]) {
            case '2': {  // 'bp2
              DCHECK(STRING_STARTS_WITH(format, "bp2"));
              PrintBp2(instr);
              return 3;
            }
          }
        }
      }
    }
    case 'C': {   // 'Cc - Special for c.xx.d cc field.
      DCHECK(STRING_STARTS_WITH(format, "Cc"));
      PrintCc(instr);
      return 2;
    }
    case 't':
      if (instr->IsMSAInstr()) {
        PrintMsaDataFormat(instr);
      } else {
        PrintFormat(instr);
      }
      return 1;
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


// For currently unimplemented decodings the disassembler calls Unknown(instr)
// which will just print "unknown" of the instruction bits.
void Decoder::Unknown(Instruction* instr) {
  Format(instr, "unknown");
}


bool Decoder::DecodeTypeRegisterRsType(Instruction* instr) {
  switch (instr->FunctionFieldRaw()) {
    case RINT:
      Format(instr, "rint.'t    'fd, 'fs");
      break;
    case MIN:
      Format(instr, "min.'t    'fd, 'fs, 'ft");
      break;
    case MAX:
      Format(instr, "max.'t    'fd, 'fs, 'ft");
      break;
    case MINA:
      Format(instr, "mina.'t   'fd, 'fs, 'ft");
      break;
    case MAXA:
      Format(instr, "maxa.'t   'fd, 'fs, 'ft");
      break;
    case SEL:
      Format(instr, "sel.'t      'fd, 'fs, 'ft");
      break;
    case SELEQZ_C:
      Format(instr, "seleqz.'t    'fd, 'fs, 'ft");
      break;
    case SELNEZ_C:
      Format(instr, "selnez.'t    'fd, 'fs, 'ft");
      break;
    case MOVZ_C:
      Format(instr, "movz.'t    'fd, 'fs, 'rt");
      break;
    case MOVN_C:
      Format(instr, "movn.'t    'fd, 'fs, 'rt");
      break;
    case MOVF:
      if (instr->Bit(16)) {
        Format(instr, "movt.'t    'fd, 'fs, 'Cc");
      } else {
        Format(instr, "movf.'t    'fd, 'fs, 'Cc");
      }
      break;
    case ADD_D:
      Format(instr, "add.'t   'fd, 'fs, 'ft");
      break;
    case SUB_D:
      Format(instr, "sub.'t   'fd, 'fs, 'ft");
      break;
    case MUL_D:
      Format(instr, "mul.'t   'fd, 'fs, 'ft");
      break;
    case DIV_D:
      Format(instr, "div.'t   'fd, 'fs, 'ft");
      break;
    case ABS_D:
      Format(instr, "abs.'t   'fd, 'fs");
      break;
    case MOV_D:
      Format(instr, "mov.'t   'fd, 'fs");
      break;
    case NEG_D:
      Format(instr, "neg.'t   'fd, 'fs");
      break;
    case SQRT_D:
      Format(instr, "sqrt.'t  'fd, 'fs");
      break;
    case RECIP_D:
      Format(instr, "recip.'t  'fd, 'fs");
      break;
    case RSQRT_D:
      Format(instr, "rsqrt.'t  'fd, 'fs");
      break;
    case CVT_W_D:
      Format(instr, "cvt.w.'t 'fd, 'fs");
      break;
    case CVT_L_D:
      Format(instr, "cvt.l.'t 'fd, 'fs");
      break;
    case TRUNC_W_D:
      Format(instr, "trunc.w.'t 'fd, 'fs");
      break;
    case TRUNC_L_D:
      Format(instr, "trunc.l.'t 'fd, 'fs");
      break;
    case ROUND_W_D:
      Format(instr, "round.w.'t 'fd, 'fs");
      break;
    case ROUND_L_D:
      Format(instr, "round.l.'t 'fd, 'fs");
      break;
    case FLOOR_W_D:
      Format(instr, "floor.w.'t 'fd, 'fs");
      break;
    case FLOOR_L_D:
      Format(instr, "floor.l.'t 'fd, 'fs");
      break;
    case CEIL_W_D:
      Format(instr, "ceil.w.'t 'fd, 'fs");
      break;
    case CLASS_D:
      Format(instr, "class.'t 'fd, 'fs");
      break;
    case CEIL_L_D:
      Format(instr, "ceil.l.'t 'fd, 'fs");
      break;
    case CVT_S_D:
      Format(instr, "cvt.s.'t 'fd, 'fs");
      break;
    case C_F_D:
      Format(instr, "c.f.'t   'fs, 'ft, 'Cc");
      break;
    case C_UN_D:
      Format(instr, "c.un.'t  'fs, 'ft, 'Cc");
      break;
    case C_EQ_D:
      Format(instr, "c.eq.'t  'fs, 'ft, 'Cc");
      break;
    case C_UEQ_D:
      Format(instr, "c.ueq.'t 'fs, 'ft, 'Cc");
      break;
    case C_OLT_D:
      Format(instr, "c.olt.'t 'fs, 'ft, 'Cc");
      break;
    case C_ULT_D:
      Format(instr, "c.ult.'t 'fs, 'ft, 'Cc");
      break;
    case C_OLE_D:
      Format(instr, "c.ole.'t 'fs, 'ft, 'Cc");
      break;
    case C_ULE_D:
      Format(instr, "c.ule.'t 'fs, 'ft, 'Cc");
      break;
    default:
      return false;
  }
  return true;
}


void Decoder::DecodeTypeRegisterSRsType(Instruction* instr) {
  if (!DecodeTypeRegisterRsType(instr)) {
    switch (instr->FunctionFieldRaw()) {
      case CVT_D_S:
        Format(instr, "cvt.d.'t 'fd, 'fs");
        break;
      case MADDF_S:
        Format(instr, "maddf.s  'fd, 'fs, 'ft");
        break;
      case MSUBF_S:
        Format(instr, "msubf.s  'fd, 'fs, 'ft");
        break;
      default:
        Format(instr, "unknown.cop1.'t");
        break;
    }
  }
}


void Decoder::DecodeTypeRegisterDRsType(Instruction* instr) {
  if (!DecodeTypeRegisterRsType(instr)) {
    switch (instr->FunctionFieldRaw()) {
      case MADDF_D:
        Format(instr, "maddf.d  'fd, 'fs, 'ft");
        break;
      case MSUBF_D:
        Format(instr, "msubf.d  'fd, 'fs, 'ft");
        break;
      default:
        Format(instr, "unknown.cop1.'t");
        break;
    }
  }
}


void Decoder::DecodeTypeRegisterLRsType(Instruction* instr) {
  switch (instr->FunctionFieldRaw()) {
    case CVT_D_L:
      Format(instr, "cvt.d.l 'fd, 'fs");
      break;
    case CVT_S_L:
      Format(instr, "cvt.s.l 'fd, 'fs");
      break;
    case CMP_AF:
      Format(instr, "cmp.af.d  'fd,  'fs, 'ft");
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
}


void Decoder::DecodeTypeRegisterWRsType(Instruction* instr) {
  switch (instr->FunctionValue()) {
    case CVT_S_W:  // Convert word to float (single).
      Format(instr, "cvt.s.w 'fd, 'fs");
      break;
    case CVT_D_W:  // Convert word to double.
      Format(instr, "cvt.d.w 'fd, 'fs");
      break;
    case CMP_AF:
      Format(instr, "cmp.af.s    'fd, 'fs, 'ft");
      break;
    case CMP_UN:
      Format(instr, "cmp.un.s    'fd, 'fs, 'ft");
      break;
    case CMP_EQ:
      Format(instr, "cmp.eq.s    'fd, 'fs, 'ft");
      break;
    case CMP_UEQ:
      Format(instr, "cmp.ueq.s   'fd, 'fs, 'ft");
      break;
    case CMP_LT:
      Format(instr, "cmp.lt.s    'fd, 'fs, 'ft");
      break;
    case CMP_ULT:
      Format(instr, "cmp.ult.s   'fd, 'fs, 'ft");
      break;
    case CMP_LE:
      Format(instr, "cmp.le.s    'fd, 'fs, 'ft");
      break;
    case CMP_ULE:
      Format(instr, "cmp.ule.s   'fd, 'fs, 'ft");
      break;
    case CMP_OR:
      Format(instr, "cmp.or.s    'fd, 'fs, 'ft");
      break;
    case CMP_UNE:
      Format(instr, "cmp.une.s   'fd, 'fs, 'ft");
      break;
    case CMP_NE:
      Format(instr, "cmp.ne.s    'fd, 'fs, 'ft");
      break;
    default:
      UNREACHABLE();
  }
}


void Decoder::DecodeTypeRegisterSPECIAL(Instruction* instr) {
  switch (instr->FunctionFieldRaw()) {
    case JR:
      Format(instr, "jr      'rs");
      break;
    case JALR:
      Format(instr, "jalr    'rs, 'rd");
      break;
    case SLL:
      if (0x0 == static_cast<int>(instr->InstructionBits()))
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
    case LSA:
      Format(instr, "lsa     'rd, 'rt, 'rs, 'sa2");
      break;
    case MFHI:
      if (instr->Bits(25, 16) == 0) {
        Format(instr, "mfhi    'rd");
      } else {
        if ((instr->FunctionFieldRaw() == CLZ_R6) && (instr->FdValue() == 1)) {
          Format(instr, "clz     'rd, 'rs");
        } else if ((instr->FunctionFieldRaw() == CLO_R6) &&
                   (instr->FdValue() == 1)) {
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
    case SYNC:
      Format(instr, "sync");
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
}


void Decoder::DecodeTypeRegisterSPECIAL2(Instruction* instr) {
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
}


void Decoder::DecodeTypeRegisterSPECIAL3(Instruction* instr) {
  switch (instr->FunctionFieldRaw()) {
    case INS: {
      if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
        Format(instr, "ins     'rt, 'rs, 'sa, 'ss2");
      } else {
        Unknown(instr);
      }
      break;
    }
    case EXT: {
      if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
        Format(instr, "ext     'rt, 'rs, 'sa, 'ss1");
      } else {
        Unknown(instr);
      }
      break;
    }
    case BSHFL: {
      int sa = instr->SaFieldRaw() >> kSaShift;
      switch (sa) {
        case BITSWAP: {
          if (IsMipsArchVariant(kMips32r6)) {
            Format(instr, "bitswap 'rd, 'rt");
          } else {
            Unknown(instr);
          }
          break;
        }
        case SEB: {
          if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
            Format(instr, "seb     'rd, 'rt");
          } else {
            Unknown(instr);
          }
          break;
        }
        case SEH: {
          if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
            Format(instr, "seh     'rd, 'rt");
          } else {
            Unknown(instr);
          }
          break;
        }
        case WSBH: {
          if (IsMipsArchVariant(kMips32r2) || IsMipsArchVariant(kMips32r6)) {
            Format(instr, "wsbh    'rd, 'rt");
          } else {
            Unknown(instr);
          }
          break;
        }
        default: {
          sa >>= kBp2Bits;
          switch (sa) {
            case ALIGN: {
              if (IsMipsArchVariant(kMips32r6)) {
                Format(instr, "align  'rd, 'rs, 'rt, 'bp2");
              } else {
                Unknown(instr);
              }
              break;
            }
            default:
              UNREACHABLE();
              break;
          }
        }
      }
      break;
    }
    default:
      UNREACHABLE();
  }
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
        case S:
          DecodeTypeRegisterSRsType(instr);
          break;
        case D:
          DecodeTypeRegisterDRsType(instr);
          break;
        case L:
          DecodeTypeRegisterLRsType(instr);
          break;
        case W:
          DecodeTypeRegisterWRsType(instr);
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
        case MADD_S:
          Format(instr, "madd.s  'fd, 'fr, 'fs, 'ft");
          break;
        case MADD_D:
          Format(instr, "madd.d  'fd, 'fr, 'fs, 'ft");
          break;
        case MSUB_S:
          Format(instr, "msub.s  'fd, 'fr, 'fs, 'ft");
          break;
        case MSUB_D:
          Format(instr, "msub.d  'fd, 'fr, 'fs, 'ft");
          break;
        default:
          UNREACHABLE();
      }
      break;
    case SPECIAL:
      DecodeTypeRegisterSPECIAL(instr);
      break;
    case SPECIAL2:
      DecodeTypeRegisterSPECIAL2(instr);
      break;
    case SPECIAL3:
      DecodeTypeRegisterSPECIAL3(instr);
      break;
    case MSA:
      switch (instr->MSAMinorOpcodeField()) {
        case kMsaMinor3R:
          DecodeTypeMsa3R(instr);
          break;
        case kMsaMinor3RF:
          DecodeTypeMsa3RF(instr);
          break;
        case kMsaMinorVEC:
          DecodeTypeMsaVec(instr);
          break;
        case kMsaMinor2R:
          DecodeTypeMsa2R(instr);
          break;
        case kMsaMinor2RF:
          DecodeTypeMsa2RF(instr);
          break;
        case kMsaMinorELM:
          DecodeTypeMsaELM(instr);
          break;
        default:
          UNREACHABLE();
      }
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeImmediateSPECIAL3(Instruction* instr) {
  switch (instr->FunctionFieldRaw()) {
    case LL_R6: {
      if (IsMipsArchVariant(kMips32r6)) {
        Format(instr, "ll     'rt, 'imm9s('rs)");
      } else {
        Unknown(instr);
      }
      break;
    }
    case SC_R6: {
      if (IsMipsArchVariant(kMips32r6)) {
        Format(instr, "sc     'rt, 'imm9s('rs)");
      } else {
        Unknown(instr);
      }
      break;
    }
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
            Format(instr, "bc1t    'bc, 'imm16u -> 'imm16p4s2");
          } else {
            Format(instr, "bc1f    'bc, 'imm16u -> 'imm16p4s2");
          }
          break;
        case BC1EQZ:
          Format(instr, "bc1eqz    'ft, 'imm16u -> 'imm16p4s2");
          break;
        case BC1NEZ:
          Format(instr, "bc1nez    'ft, 'imm16u -> 'imm16p4s2");
          break;
        case BZ_V:
        case BZ_B:
        case BZ_H:
        case BZ_W:
        case BZ_D:
          Format(instr, "bz.'t  'wt, 'imm16s -> 'imm16p4s2");
          break;
        case BNZ_V:
        case BNZ_B:
        case BNZ_H:
        case BNZ_W:
        case BNZ_D:
          Format(instr, "bnz.'t  'wt, 'imm16s -> 'imm16p4s2");
          break;
        default:
          UNREACHABLE();
      }

      break;  // Case COP1.
    // ------------- REGIMM class.
    case REGIMM:
      switch (instr->RtFieldRaw()) {
        case BLTZ:
          Format(instr, "bltz    'rs, 'imm16u -> 'imm16p4s2");
          break;
        case BLTZAL:
          Format(instr, "bltzal  'rs, 'imm16u -> 'imm16p4s2");
          break;
        case BGEZ:
          Format(instr, "bgez    'rs, 'imm16u -> 'imm16p4s2");
          break;
        case BGEZAL: {
          if (instr->RsValue() == 0)
            Format(instr, "bal     'imm16s -> 'imm16p4s2");
          else
            Format(instr, "bgezal  'rs, 'imm16u -> 'imm16p4s2");
          break;
        }
        case BGEZALL:
          Format(instr, "bgezall 'rs, 'imm16u -> 'imm16p4s2");
          break;
        default:
          UNREACHABLE();
      }
    break;  // Case REGIMM.
    // ------------- Branch instructions.
    case BEQ:
      Format(instr, "beq     'rs, 'rt, 'imm16u -> 'imm16p4s2");
      break;
    case BC:
      Format(instr, "bc      'imm26s -> 'imm26p4s2");
      break;
    case BALC:
      Format(instr, "balc    'imm26s -> 'imm26p4s2");
      break;
    case BNE:
      Format(instr, "bne     'rs, 'rt, 'imm16u -> 'imm16p4s2");
      break;
    case BLEZ:
      if ((instr->RtValue() == 0) && (instr->RsValue() != 0)) {
        Format(instr, "blez    'rs, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RtValue() != instr->RsValue()) &&
                 (instr->RsValue() != 0) && (instr->RtValue() != 0)) {
        Format(instr, "bgeuc   'rs, 'rt, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RtValue() == instr->RsValue()) &&
                 (instr->RtValue() != 0)) {
        Format(instr, "bgezalc 'rs, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RsValue() == 0) && (instr->RtValue() != 0)) {
        Format(instr, "blezalc 'rt, 'imm16u -> 'imm16p4s2");
      } else {
        UNREACHABLE();
      }
      break;
    case BGTZ:
      if ((instr->RtValue() == 0) && (instr->RsValue() != 0)) {
        Format(instr, "bgtz    'rs, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RtValue() != instr->RsValue()) &&
                 (instr->RsValue() != 0) && (instr->RtValue() != 0)) {
        Format(instr, "bltuc   'rs, 'rt, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RtValue() == instr->RsValue()) &&
                 (instr->RtValue() != 0)) {
        Format(instr, "bltzalc 'rt, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RsValue() == 0) && (instr->RtValue() != 0)) {
        Format(instr, "bgtzalc 'rt, 'imm16u -> 'imm16p4s2");
      } else {
        UNREACHABLE();
      }
      break;
    case BLEZL:
      if ((instr->RtValue() == instr->RsValue()) && (instr->RtValue() != 0)) {
        Format(instr, "bgezc    'rt, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RtValue() != instr->RsValue()) &&
                 (instr->RsValue() != 0) && (instr->RtValue() != 0)) {
        Format(instr, "bgec     'rs, 'rt, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RsValue() == 0) && (instr->RtValue() != 0)) {
        Format(instr, "blezc    'rt, 'imm16u -> 'imm16p4s2");
      } else {
        UNREACHABLE();
      }
      break;
    case BGTZL:
      if ((instr->RtValue() == instr->RsValue()) && (instr->RtValue() != 0)) {
        Format(instr, "bltzc    'rt, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RtValue() != instr->RsValue()) &&
                 (instr->RsValue() != 0) && (instr->RtValue() != 0)) {
        Format(instr, "bltc    'rs, 'rt, 'imm16u -> 'imm16p4s2");
      } else if ((instr->RsValue() == 0) && (instr->RtValue() != 0)) {
        Format(instr, "bgtzc    'rt, 'imm16u -> 'imm16p4s2");
      } else {
        UNREACHABLE();
      }
      break;
    case POP66:
      if (instr->RsValue() == JIC) {
        Format(instr, "jic     'rt, 'imm16s");
      } else {
        Format(instr, "beqzc   'rs, 'imm21s -> 'imm21p4s2");
      }
      break;
    case POP76:
      if (instr->RsValue() == JIALC) {
        Format(instr, "jialc   'rt, 'imm16s");
      } else {
        Format(instr, "bnezc   'rs, 'imm21s -> 'imm21p4s2");
      }
      break;
    // ------------- Arithmetic instructions.
    case ADDI:
      if (!IsMipsArchVariant(kMips32r6)) {
        Format(instr, "addi    'rt, 'rs, 'imm16s");
      } else {
        int rs_reg = instr->RsValue();
        int rt_reg = instr->RtValue();
        // Check if BOVC, BEQZALC or BEQC instruction.
        if (rs_reg >= rt_reg) {
          Format(instr, "bovc  'rs, 'rt, 'imm16s -> 'imm16p4s2");
        } else {
          DCHECK_GT(rt_reg, 0);
          if (rs_reg == 0) {
            Format(instr, "beqzalc 'rt, 'imm16s -> 'imm16p4s2");
          } else {
            Format(instr, "beqc    'rs, 'rt, 'imm16s -> 'imm16p4s2");
          }
        }
      }
      break;
    case DADDI:
      if (IsMipsArchVariant(kMips32r6)) {
        int rs_reg = instr->RsValue();
        int rt_reg = instr->RtValue();
        // Check if BNVC, BNEZALC or BNEC instruction.
        if (rs_reg >= rt_reg) {
          Format(instr, "bnvc  'rs, 'rt, 'imm16s -> 'imm16p4s2");
        } else {
          DCHECK_GT(rt_reg, 0);
          if (rs_reg == 0) {
            Format(instr, "bnezalc 'rt, 'imm16s -> 'imm16p4s2");
          } else {
            Format(instr, "bnec  'rs, 'rt, 'imm16s -> 'imm16p4s2");
          }
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
          Format(instr, "aui     'rt, 'rs, 'imm16x");
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
    case LL:
      if (IsMipsArchVariant(kMips32r6)) {
        Unknown(instr);
      } else {
        Format(instr, "ll     'rt, 'imm16s('rs)");
      }
      break;
    case SC:
      if (IsMipsArchVariant(kMips32r6)) {
        Unknown(instr);
      } else {
        Format(instr, "sc     'rt, 'imm16s('rs)");
      }
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
    case PCREL: {
      int32_t imm21 = instr->Imm21Value();
      // rt field: 5-bits checking
      uint8_t rt = (imm21 >> kImm16Bits);
      switch (rt) {
        case ALUIPC:
          Format(instr, "aluipc  'rs, 'imm16s");
          break;
        case AUIPC:
          Format(instr, "auipc   'rs, 'imm16s");
          break;
        default: {
          // rt field: checking of the most significant 2-bits
          rt = (imm21 >> kImm19Bits);
          switch (rt) {
            case LWPC:
              Format(instr, "lwpc    'rs, 'imm19s");
              break;
            case ADDIUPC:
              Format(instr, "addiupc 'rs, 'imm19s");
              break;
            default:
              UNREACHABLE();
              break;
          }
        }
      }
      break;
    }
    case SPECIAL3:
      DecodeTypeImmediateSPECIAL3(instr);
      break;
    case MSA:
      switch (instr->MSAMinorOpcodeField()) {
        case kMsaMinorI8:
          DecodeTypeMsaI8(instr);
          break;
        case kMsaMinorI5:
          DecodeTypeMsaI5(instr);
          break;
        case kMsaMinorI10:
          DecodeTypeMsaI10(instr);
          break;
        case kMsaMinorELM:
          DecodeTypeMsaELM(instr);
          break;
        case kMsaMinorBIT:
          DecodeTypeMsaBIT(instr);
          break;
        case kMsaMinorMI10:
          DecodeTypeMsaMI10(instr);
          break;
        default:
          UNREACHABLE();
          break;
      }
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
      Format(instr, "j       'imm26x -> 'imm26j");
      break;
    case JAL:
      Format(instr, "jal     'imm26x -> 'imm26j");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsaI8(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsaI8Mask;

  switch (opcode) {
    case ANDI_B:
      Format(instr, "andi.b  'wd, 'ws, 'imm8");
      break;
    case ORI_B:
      Format(instr, "ori.b  'wd, 'ws, 'imm8");
      break;
    case NORI_B:
      Format(instr, "nori.b  'wd, 'ws, 'imm8");
      break;
    case XORI_B:
      Format(instr, "xori.b  'wd, 'ws, 'imm8");
      break;
    case BMNZI_B:
      Format(instr, "bmnzi.b  'wd, 'ws, 'imm8");
      break;
    case BMZI_B:
      Format(instr, "bmzi.b  'wd, 'ws, 'imm8");
      break;
    case BSELI_B:
      Format(instr, "bseli.b  'wd, 'ws, 'imm8");
      break;
    case SHF_B:
      Format(instr, "shf.b  'wd, 'ws, 'imm8");
      break;
    case SHF_H:
      Format(instr, "shf.h  'wd, 'ws, 'imm8");
      break;
    case SHF_W:
      Format(instr, "shf.w  'wd, 'ws, 'imm8");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsaI5(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsaI5Mask;

  switch (opcode) {
    case ADDVI:
      Format(instr, "addvi.'t  'wd, 'ws, 'imm5u");
      break;
    case SUBVI:
      Format(instr, "subvi.'t  'wd, 'ws, 'imm5u");
      break;
    case MAXI_S:
      Format(instr, "maxi_s.'t  'wd, 'ws, 'imm5s");
      break;
    case MAXI_U:
      Format(instr, "maxi_u.'t  'wd, 'ws, 'imm5u");
      break;
    case MINI_S:
      Format(instr, "mini_s.'t  'wd, 'ws, 'imm5s");
      break;
    case MINI_U:
      Format(instr, "mini_u.'t  'wd, 'ws, 'imm5u");
      break;
    case CEQI:
      Format(instr, "ceqi.'t  'wd, 'ws, 'imm5s");
      break;
    case CLTI_S:
      Format(instr, "clti_s.'t  'wd, 'ws, 'imm5s");
      break;
    case CLTI_U:
      Format(instr, "clti_u.'t  'wd, 'ws, 'imm5u");
      break;
    case CLEI_S:
      Format(instr, "clei_s.'t  'wd, 'ws, 'imm5s");
      break;
    case CLEI_U:
      Format(instr, "clei_u.'t  'wd, 'ws, 'imm5u");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsaI10(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsaI5Mask;
  if (opcode == LDI) {
    Format(instr, "ldi.'t  'wd, 'imm10s1");
  } else {
    UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsaELM(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsaELMMask;
  switch (opcode) {
    case SLDI:
      if (instr->Bits(21, 16) == 0x3E) {
        Format(instr, "ctcmsa  ");
        PrintMSAControlRegister(instr->WdValue());
        Print(", ");
        PrintRegister(instr->WsValue());
      } else {
        Format(instr, "sldi.'t  'wd, 'ws['imme]");
      }
      break;
    case SPLATI:
      if (instr->Bits(21, 16) == 0x3E) {
        Format(instr, "cfcmsa  ");
        PrintRegister(instr->WdValue());
        Print(", ");
        PrintMSAControlRegister(instr->WsValue());
      } else {
        Format(instr, "splati.'t  'wd, 'ws['imme]");
      }
      break;
    case COPY_S:
      if (instr->Bits(21, 16) == 0x3E) {
        Format(instr, "move.v  'wd, 'ws");
      } else {
        Format(instr, "copy_s.'t  ");
        PrintMsaCopy(instr);
      }
      break;
    case COPY_U:
      Format(instr, "copy_u.'t  ");
      PrintMsaCopy(instr);
      break;
    case INSERT:
      Format(instr, "insert.'t  'wd['imme], ");
      PrintRegister(instr->WsValue());
      break;
    case INSVE:
      Format(instr, "insve.'t  'wd['imme], 'ws[0]");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsaBIT(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsaBITMask;

  switch (opcode) {
    case SLLI:
      Format(instr, "slli.'t  'wd, 'ws, 'immb");
      break;
    case SRAI:
      Format(instr, "srai.'t  'wd, 'ws, 'immb");
      break;
    case SRLI:
      Format(instr, "srli.'t  'wd, 'ws, 'immb");
      break;
    case BCLRI:
      Format(instr, "bclri.'t  'wd, 'ws, 'immb");
      break;
    case BSETI:
      Format(instr, "bseti.'t  'wd, 'ws, 'immb");
      break;
    case BNEGI:
      Format(instr, "bnegi.'t  'wd, 'ws, 'immb");
      break;
    case BINSLI:
      Format(instr, "binsli.'t  'wd, 'ws, 'immb");
      break;
    case BINSRI:
      Format(instr, "binsri.'t  'wd, 'ws, 'immb");
      break;
    case SAT_S:
      Format(instr, "sat_s.'t  'wd, 'ws, 'immb");
      break;
    case SAT_U:
      Format(instr, "sat_u.'t  'wd, 'ws, 'immb");
      break;
    case SRARI:
      Format(instr, "srari.'t  'wd, 'ws, 'immb");
      break;
    case SRLRI:
      Format(instr, "srlri.'t  'wd, 'ws, 'immb");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsaMI10(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsaMI10Mask;
  if (opcode == MSA_LD) {
    Format(instr, "ld.'t  'wd, 'imm10s2(");
    PrintRegister(instr->WsValue());
    Print(")");
  } else if (opcode == MSA_ST) {
    Format(instr, "st.'t  'wd, 'imm10s2(");
    PrintRegister(instr->WsValue());
    Print(")");
  } else {
    UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsa3R(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsa3RMask;
  switch (opcode) {
    case SLL_MSA:
      Format(instr, "sll.'t  'wd, 'ws, 'wt");
      break;
    case SRA_MSA:
      Format(instr, "sra.'t  'wd, 'ws, 'wt");
      break;
    case SRL_MSA:
      Format(instr, "srl.'t  'wd, 'ws, 'wt");
      break;
    case BCLR:
      Format(instr, "bclr.'t  'wd, 'ws, 'wt");
      break;
    case BSET:
      Format(instr, "bset.'t  'wd, 'ws, 'wt");
      break;
    case BNEG:
      Format(instr, "bneg.'t  'wd, 'ws, 'wt");
      break;
    case BINSL:
      Format(instr, "binsl.'t  'wd, 'ws, 'wt");
      break;
    case BINSR:
      Format(instr, "binsr.'t  'wd, 'ws, 'wt");
      break;
    case ADDV:
      Format(instr, "addv.'t  'wd, 'ws, 'wt");
      break;
    case SUBV:
      Format(instr, "subv.'t  'wd, 'ws, 'wt");
      break;
    case MAX_S:
      Format(instr, "max_s.'t  'wd, 'ws, 'wt");
      break;
    case MAX_U:
      Format(instr, "max_u.'t  'wd, 'ws, 'wt");
      break;
    case MIN_S:
      Format(instr, "min_s.'t  'wd, 'ws, 'wt");
      break;
    case MIN_U:
      Format(instr, "min_u.'t  'wd, 'ws, 'wt");
      break;
    case MAX_A:
      Format(instr, "max_a.'t  'wd, 'ws, 'wt");
      break;
    case MIN_A:
      Format(instr, "min_a.'t  'wd, 'ws, 'wt");
      break;
    case CEQ:
      Format(instr, "ceq.'t  'wd, 'ws, 'wt");
      break;
    case CLT_S:
      Format(instr, "clt_s.'t  'wd, 'ws, 'wt");
      break;
    case CLT_U:
      Format(instr, "clt_u.'t  'wd, 'ws, 'wt");
      break;
    case CLE_S:
      Format(instr, "cle_s.'t  'wd, 'ws, 'wt");
      break;
    case CLE_U:
      Format(instr, "cle_u.'t  'wd, 'ws, 'wt");
      break;
    case ADD_A:
      Format(instr, "add_a.'t  'wd, 'ws, 'wt");
      break;
    case ADDS_A:
      Format(instr, "adds_a.'t  'wd, 'ws, 'wt");
      break;
    case ADDS_S:
      Format(instr, "adds_s.'t  'wd, 'ws, 'wt");
      break;
    case ADDS_U:
      Format(instr, "adds_u.'t  'wd, 'ws, 'wt");
      break;
    case AVE_S:
      Format(instr, "ave_s.'t  'wd, 'ws, 'wt");
      break;
    case AVE_U:
      Format(instr, "ave_u.'t  'wd, 'ws, 'wt");
      break;
    case AVER_S:
      Format(instr, "aver_s.'t  'wd, 'ws, 'wt");
      break;
    case AVER_U:
      Format(instr, "aver_u.'t  'wd, 'ws, 'wt");
      break;
    case SUBS_S:
      Format(instr, "subs_s.'t  'wd, 'ws, 'wt");
      break;
    case SUBS_U:
      Format(instr, "subs_u.'t  'wd, 'ws, 'wt");
      break;
    case SUBSUS_U:
      Format(instr, "subsus_u.'t  'wd, 'ws, 'wt");
      break;
    case SUBSUU_S:
      Format(instr, "subsuu_s.'t  'wd, 'ws, 'wt");
      break;
    case ASUB_S:
      Format(instr, "asub_s.'t  'wd, 'ws, 'wt");
      break;
    case ASUB_U:
      Format(instr, "asub_u.'t  'wd, 'ws, 'wt");
      break;
    case MULV:
      Format(instr, "mulv.'t  'wd, 'ws, 'wt");
      break;
    case MADDV:
      Format(instr, "maddv.'t  'wd, 'ws, 'wt");
      break;
    case MSUBV:
      Format(instr, "msubv.'t  'wd, 'ws, 'wt");
      break;
    case DIV_S_MSA:
      Format(instr, "div_s.'t  'wd, 'ws, 'wt");
      break;
    case DIV_U:
      Format(instr, "div_u.'t  'wd, 'ws, 'wt");
      break;
    case MOD_S:
      Format(instr, "mod_s.'t  'wd, 'ws, 'wt");
      break;
    case MOD_U:
      Format(instr, "mod_u.'t  'wd, 'ws, 'wt");
      break;
    case DOTP_S:
      Format(instr, "dotp_s.'t  'wd, 'ws, 'wt");
      break;
    case DOTP_U:
      Format(instr, "dotp_u.'t  'wd, 'ws, 'wt");
      break;
    case DPADD_S:
      Format(instr, "dpadd_s.'t  'wd, 'ws, 'wt");
      break;
    case DPADD_U:
      Format(instr, "dpadd_u.'t  'wd, 'ws, 'wt");
      break;
    case DPSUB_S:
      Format(instr, "dpsub_s.'t  'wd, 'ws, 'wt");
      break;
    case DPSUB_U:
      Format(instr, "dpsub_u.'t  'wd, 'ws, 'wt");
      break;
    case SLD:
      Format(instr, "sld.'t  'wd, 'ws['rt]");
      break;
    case SPLAT:
      Format(instr, "splat.'t  'wd, 'ws['rt]");
      break;
    case PCKEV:
      Format(instr, "pckev.'t  'wd, 'ws, 'wt");
      break;
    case PCKOD:
      Format(instr, "pckod.'t  'wd, 'ws, 'wt");
      break;
    case ILVL:
      Format(instr, "ilvl.'t  'wd, 'ws, 'wt");
      break;
    case ILVR:
      Format(instr, "ilvr.'t  'wd, 'ws, 'wt");
      break;
    case ILVEV:
      Format(instr, "ilvev.'t  'wd, 'ws, 'wt");
      break;
    case ILVOD:
      Format(instr, "ilvod.'t  'wd, 'ws, 'wt");
      break;
    case VSHF:
      Format(instr, "vshf.'t  'wd, 'ws, 'wt");
      break;
    case SRAR:
      Format(instr, "srar.'t  'wd, 'ws, 'wt");
      break;
    case SRLR:
      Format(instr, "srlr.'t  'wd, 'ws, 'wt");
      break;
    case HADD_S:
      Format(instr, "hadd_s.'t  'wd, 'ws, 'wt");
      break;
    case HADD_U:
      Format(instr, "hadd_u.'t  'wd, 'ws, 'wt");
      break;
    case HSUB_S:
      Format(instr, "hsub_s.'t  'wd, 'ws, 'wt");
      break;
    case HSUB_U:
      Format(instr, "hsub_u.'t  'wd, 'ws, 'wt");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsa3RF(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsa3RFMask;
  switch (opcode) {
    case FCAF:
      Format(instr, "fcaf.'t  'wd, 'ws, 'wt");
      break;
    case FCUN:
      Format(instr, "fcun.'t  'wd, 'ws, 'wt");
      break;
    case FCEQ:
      Format(instr, "fceq.'t  'wd, 'ws, 'wt");
      break;
    case FCUEQ:
      Format(instr, "fcueq.'t  'wd, 'ws, 'wt");
      break;
    case FCLT:
      Format(instr, "fclt.'t  'wd, 'ws, 'wt");
      break;
    case FCULT:
      Format(instr, "fcult.'t  'wd, 'ws, 'wt");
      break;
    case FCLE:
      Format(instr, "fcle.'t  'wd, 'ws, 'wt");
      break;
    case FCULE:
      Format(instr, "fcule.'t  'wd, 'ws, 'wt");
      break;
    case FSAF:
      Format(instr, "fsaf.'t  'wd, 'ws, 'wt");
      break;
    case FSUN:
      Format(instr, "fsun.'t  'wd, 'ws, 'wt");
      break;
    case FSEQ:
      Format(instr, "fseq.'t  'wd, 'ws, 'wt");
      break;
    case FSUEQ:
      Format(instr, "fsueq.'t  'wd, 'ws, 'wt");
      break;
    case FSLT:
      Format(instr, "fslt.'t  'wd, 'ws, 'wt");
      break;
    case FSULT:
      Format(instr, "fsult.'t  'wd, 'ws, 'wt");
      break;
    case FSLE:
      Format(instr, "fsle.'t  'wd, 'ws, 'wt");
      break;
    case FSULE:
      Format(instr, "fsule.'t  'wd, 'ws, 'wt");
      break;
    case FADD:
      Format(instr, "fadd.'t  'wd, 'ws, 'wt");
      break;
    case FSUB:
      Format(instr, "fsub.'t  'wd, 'ws, 'wt");
      break;
    case FMUL:
      Format(instr, "fmul.'t  'wd, 'ws, 'wt");
      break;
    case FDIV:
      Format(instr, "fdiv.'t  'wd, 'ws, 'wt");
      break;
    case FMADD:
      Format(instr, "fmadd.'t  'wd, 'ws, 'wt");
      break;
    case FMSUB:
      Format(instr, "fmsub.'t  'wd, 'ws, 'wt");
      break;
    case FEXP2:
      Format(instr, "fexp2.'t  'wd, 'ws, 'wt");
      break;
    case FEXDO:
      Format(instr, "fexdo.'t  'wd, 'ws, 'wt");
      break;
    case FTQ:
      Format(instr, "ftq.'t  'wd, 'ws, 'wt");
      break;
    case FMIN:
      Format(instr, "fmin.'t  'wd, 'ws, 'wt");
      break;
    case FMIN_A:
      Format(instr, "fmin_a.'t  'wd, 'ws, 'wt");
      break;
    case FMAX:
      Format(instr, "fmax.'t  'wd, 'ws, 'wt");
      break;
    case FMAX_A:
      Format(instr, "fmax_a.'t  'wd, 'ws, 'wt");
      break;
    case FCOR:
      Format(instr, "fcor.'t  'wd, 'ws, 'wt");
      break;
    case FCUNE:
      Format(instr, "fcune.'t  'wd, 'ws, 'wt");
      break;
    case FCNE:
      Format(instr, "fcne.'t  'wd, 'ws, 'wt");
      break;
    case MUL_Q:
      Format(instr, "mul_q.'t  'wd, 'ws, 'wt");
      break;
    case MADD_Q:
      Format(instr, "madd_q.'t  'wd, 'ws, 'wt");
      break;
    case MSUB_Q:
      Format(instr, "msub_q.'t  'wd, 'ws, 'wt");
      break;
    case FSOR:
      Format(instr, "fsor.'t  'wd, 'ws, 'wt");
      break;
    case FSUNE:
      Format(instr, "fsune.'t  'wd, 'ws, 'wt");
      break;
    case FSNE:
      Format(instr, "fsne.'t  'wd, 'ws, 'wt");
      break;
    case MULR_Q:
      Format(instr, "mulr_q.'t  'wd, 'ws, 'wt");
      break;
    case MADDR_Q:
      Format(instr, "maddr_q.'t  'wd, 'ws, 'wt");
      break;
    case MSUBR_Q:
      Format(instr, "msubr_q.'t  'wd, 'ws, 'wt");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsaVec(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsaVECMask;
  switch (opcode) {
    case AND_V:
      Format(instr, "and.v  'wd, 'ws, 'wt");
      break;
    case OR_V:
      Format(instr, "or.v  'wd, 'ws, 'wt");
      break;
    case NOR_V:
      Format(instr, "nor.v  'wd, 'ws, 'wt");
      break;
    case XOR_V:
      Format(instr, "xor.v  'wd, 'ws, 'wt");
      break;
    case BMNZ_V:
      Format(instr, "bmnz.v  'wd, 'ws, 'wt");
      break;
    case BMZ_V:
      Format(instr, "bmz.v  'wd, 'ws, 'wt");
      break;
    case BSEL_V:
      Format(instr, "bsel.v  'wd, 'ws, 'wt");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsa2R(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsa2RMask;
  switch (opcode) {
    case FILL: {
      Format(instr, "fill.'t  'wd, ");
      PrintRegister(instr->WsValue());  // rs value is in ws field
    } break;
    case PCNT:
      Format(instr, "pcnt.'t  'wd, 'ws");
      break;
    case NLOC:
      Format(instr, "nloc.'t  'wd, 'ws");
      break;
    case NLZC:
      Format(instr, "nlzc.'t  'wd, 'ws");
      break;
    default:
      UNREACHABLE();
  }
}

void Decoder::DecodeTypeMsa2RF(Instruction* instr) {
  uint32_t opcode = instr->InstructionBits() & kMsa2RFMask;
  switch (opcode) {
    case FCLASS:
      Format(instr, "fclass.'t  'wd, 'ws");
      break;
    case FTRUNC_S:
      Format(instr, "ftrunc_s.'t  'wd, 'ws");
      break;
    case FTRUNC_U:
      Format(instr, "ftrunc_u.'t  'wd, 'ws");
      break;
    case FSQRT:
      Format(instr, "fsqrt.'t  'wd, 'ws");
      break;
    case FRSQRT:
      Format(instr, "frsqrt.'t  'wd, 'ws");
      break;
    case FRCP:
      Format(instr, "frcp.'t  'wd, 'ws");
      break;
    case FRINT:
      Format(instr, "frint.'t  'wd, 'ws");
      break;
    case FLOG2:
      Format(instr, "flog2.'t  'wd, 'ws");
      break;
    case FEXUPL:
      Format(instr, "fexupl.'t  'wd, 'ws");
      break;
    case FEXUPR:
      Format(instr, "fexupr.'t  'wd, 'ws");
      break;
    case FFQL:
      Format(instr, "ffql.'t  'wd, 'ws");
      break;
    case FFQR:
      Format(instr, "ffqr.'t  'wd, 'ws");
      break;
    case FTINT_S:
      Format(instr, "ftint_s.'t  'wd, 'ws");
      break;
    case FTINT_U:
      Format(instr, "ftint_u.'t  'wd, 'ws");
      break;
    case FFINT_S:
      Format(instr, "ffint_s.'t  'wd, 'ws");
      break;
    case FFINT_U:
      Format(instr, "ffint_u.'t  'wd, 'ws");
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
    v8::internal::PrintF(f, "%p    %08x      %s\n", static_cast<void*>(prev_pc),
                         *reinterpret_cast<int32_t*>(prev_pc), buffer.start());
  }
}


#undef UNSUPPORTED

}  // namespace disasm

#endif  // V8_TARGET_ARCH_MIPS

// Copyright 2014 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_S390

#include "src/base/platform/platform.h"
#include "src/disasm.h"
#include "src/macro-assembler.h"
#include "src/s390/constants-s390.h"

namespace v8 {
namespace internal {

const auto GetRegConfig = RegisterConfiguration::Crankshaft;

//------------------------------------------------------------------------------

// Decoder decodes and disassembles instructions into an output buffer.
// It uses the converter to convert register names and call destinations into
// more informative description.
class Decoder {
 public:
  Decoder(const disasm::NameConverter& converter, Vector<char> out_buffer)
      : converter_(converter), out_buffer_(out_buffer), out_buffer_pos_(0) {
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
  void PrintDRegister(int reg);
  void PrintSoftwareInterrupt(SoftwareInterruptCodes svc);

  // Handle formatting of instructions and their options.
  int FormatRegister(Instruction* instr, const char* option);
  int FormatFloatingRegister(Instruction* instr, const char* option);
  int FormatMask(Instruction* instr, const char* option);
  int FormatDisplacement(Instruction* instr, const char* option);
  int FormatImmediate(Instruction* instr, const char* option);
  int FormatOption(Instruction* instr, const char* option);
  void Format(Instruction* instr, const char* format);
  void Unknown(Instruction* instr);
  void UnknownFormat(Instruction* instr, const char* opcname);

  bool DecodeTwoByte(Instruction* instr);
  bool DecodeFourByte(Instruction* instr);
  bool DecodeSixByte(Instruction* instr);

  const disasm::NameConverter& converter_;
  Vector<char> out_buffer_;
  int out_buffer_pos_;

  DISALLOW_COPY_AND_ASSIGN(Decoder);
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

// Print the double FP register name according to the active name converter.
void Decoder::PrintDRegister(int reg) {
  Print(GetRegConfig()->GetDoubleRegisterName(reg));
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
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d - 0x%x",
                                    svc & kStopCodeMask, svc & kStopCodeMask);
      } else {
        out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", svc);
      }
      return;
  }
}

// Handle all register based formatting in this function to reduce the
// complexity of FormatOption.
int Decoder::FormatRegister(Instruction* instr, const char* format) {
  DCHECK(format[0] == 'r');

  if (format[1] == '1') {  // 'r1: register resides in bit 8-11
    RRInstruction* rrinstr = reinterpret_cast<RRInstruction*>(instr);
    int reg = rrinstr->R1Value();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == '2') {  // 'r2: register resides in bit 12-15
    RRInstruction* rrinstr = reinterpret_cast<RRInstruction*>(instr);
    int reg = rrinstr->R2Value();
    // indicating it is a r0 for displacement, in which case the offset
    // should be 0.
    if (format[2] == 'd') {
      if (reg == 0) return 4;
      PrintRegister(reg);
      return 3;
    } else {
      PrintRegister(reg);
      return 2;
    }
  } else if (format[1] == '3') {  // 'r3: register resides in bit 16-19
    RSInstruction* rsinstr = reinterpret_cast<RSInstruction*>(instr);
    int reg = rsinstr->B2Value();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == '4') {  // 'r4: register resides in bit 20-23
    RSInstruction* rsinstr = reinterpret_cast<RSInstruction*>(instr);
    int reg = rsinstr->B2Value();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == '5') {  // 'r5: register resides in bit 24-28
    RREInstruction* rreinstr = reinterpret_cast<RREInstruction*>(instr);
    int reg = rreinstr->R1Value();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == '6') {  // 'r6: register resides in bit 29-32
    RREInstruction* rreinstr = reinterpret_cast<RREInstruction*>(instr);
    int reg = rreinstr->R2Value();
    PrintRegister(reg);
    return 2;
  } else if (format[1] == '7') {  // 'r6: register resides in bit 32-35
    SSInstruction* ssinstr = reinterpret_cast<SSInstruction*>(instr);
    int reg = ssinstr->B2Value();
    PrintRegister(reg);
    return 2;
  }

  UNREACHABLE();
  return -1;
}

int Decoder::FormatFloatingRegister(Instruction* instr, const char* format) {
  DCHECK(format[0] == 'f');

  // reuse 1, 5 and 6 because it is coresponding
  if (format[1] == '1') {  // 'r1: register resides in bit 8-11
    RRInstruction* rrinstr = reinterpret_cast<RRInstruction*>(instr);
    int reg = rrinstr->R1Value();
    PrintDRegister(reg);
    return 2;
  } else if (format[1] == '2') {  // 'f2: register resides in bit 12-15
    RRInstruction* rrinstr = reinterpret_cast<RRInstruction*>(instr);
    int reg = rrinstr->R2Value();
    PrintDRegister(reg);
    return 2;
  } else if (format[1] == '3') {  // 'f3: register resides in bit 16-19
    RRDInstruction* rrdinstr = reinterpret_cast<RRDInstruction*>(instr);
    int reg = rrdinstr->R1Value();
    PrintDRegister(reg);
    return 2;
  } else if (format[1] == '5') {  // 'f5: register resides in bit 24-28
    RREInstruction* rreinstr = reinterpret_cast<RREInstruction*>(instr);
    int reg = rreinstr->R1Value();
    PrintDRegister(reg);
    return 2;
  } else if (format[1] == '6') {  // 'f6: register resides in bit 29-32
    RREInstruction* rreinstr = reinterpret_cast<RREInstruction*>(instr);
    int reg = rreinstr->R2Value();
    PrintDRegister(reg);
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
    case 'o': {
      if (instr->Bit(10) == 1) {
        Print("o");
      }
      return 1;
    }
    case '.': {
      if (instr->Bit(0) == 1) {
        Print(".");
      } else {
        Print(" ");  // ensure consistent spacing
      }
      return 1;
    }
    case 'r': {
      return FormatRegister(instr, format);
    }
    case 'f': {
      return FormatFloatingRegister(instr, format);
    }
    case 'i': {  // int16
      return FormatImmediate(instr, format);
    }
    case 'u': {  // uint16
      int32_t value = instr->Bits(15, 0);
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
      return 6;
    }
    case 'l': {
      // Link (LK) Bit 0
      if (instr->Bit(0) == 1) {
        Print("l");
      }
      return 1;
    }
    case 'a': {
      // Absolute Address Bit 1
      if (instr->Bit(1) == 1) {
        Print("a");
      }
      return 1;
    }
    case 't': {  // 'target: target of branch instructions
      // target26 or target16
      DCHECK(STRING_STARTS_WITH(format, "target"));
      if ((format[6] == '2') && (format[7] == '6')) {
        int off = ((instr->Bits(25, 2)) << 8) >> 6;
        out_buffer_pos_ += SNPrintF(
            out_buffer_ + out_buffer_pos_, "%+d -> %s", off,
            converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + off));
        return 8;
      } else if ((format[6] == '1') && (format[7] == '6')) {
        int off = ((instr->Bits(15, 2)) << 18) >> 16;
        out_buffer_pos_ += SNPrintF(
            out_buffer_ + out_buffer_pos_, "%+d -> %s", off,
            converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + off));
        return 8;
      }
      case 'm': {
        return FormatMask(instr, format);
      }
    }
    case 'd': {  // ds value for offset
      return FormatDisplacement(instr, format);
    }
    default: {
      UNREACHABLE();
      break;
    }
  }

  UNREACHABLE();
  return -1;
}

int Decoder::FormatMask(Instruction* instr, const char* format) {
  DCHECK(format[0] == 'm');
  int32_t value = 0;
  if ((format[1] == '1')) {  // prints the mask format in bits 8-12
    value = reinterpret_cast<RRInstruction*>(instr)->R1Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", value);
    return 2;
  } else if (format[1] == '2') {  // mask format in bits 16-19
    value = reinterpret_cast<RXInstruction*>(instr)->B2Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", value);
    return 2;
  } else if (format[1] == '3') {  // mask format in bits 20-23
    value = reinterpret_cast<RRFInstruction*>(instr)->M4Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "0x%x", value);
    return 2;
  }

  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
  return 2;
}

int Decoder::FormatDisplacement(Instruction* instr, const char* format) {
  DCHECK(format[0] == 'd');

  if (format[1] == '1') {  // displacement in 20-31
    RSInstruction* rsinstr = reinterpret_cast<RSInstruction*>(instr);
    uint16_t value = rsinstr->D2Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);

    return 2;
  } else if (format[1] == '2') {  // displacement in 20-39
    RXYInstruction* rxyinstr = reinterpret_cast<RXYInstruction*>(instr);
    int32_t value = rxyinstr->D2Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '4') {  // SS displacement 2 36-47
    SSInstruction* ssInstr = reinterpret_cast<SSInstruction*>(instr);
    uint16_t value = ssInstr->D2Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '3') {  // SS displacement 1 20 - 32
    SSInstruction* ssInstr = reinterpret_cast<SSInstruction*>(instr);
    uint16_t value = ssInstr->D1Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else {  // s390 specific
    int32_t value = SIGN_EXT_IMM16(instr->Bits(15, 0) & ~3);
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 1;
  }
}

int Decoder::FormatImmediate(Instruction* instr, const char* format) {
  DCHECK(format[0] == 'i');

  if (format[1] == '1') {  // immediate in 16-31
    RIInstruction* riinstr = reinterpret_cast<RIInstruction*>(instr);
    int16_t value = riinstr->I2Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '2') {  // immediate in 16-48
    RILInstruction* rilinstr = reinterpret_cast<RILInstruction*>(instr);
    int32_t value = rilinstr->I2Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '3') {  // immediate in I format
    IInstruction* iinstr = reinterpret_cast<IInstruction*>(instr);
    int8_t value = iinstr->IValue();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '4') {  // immediate in 16-31, but outputs as offset
    RIInstruction* riinstr = reinterpret_cast<RIInstruction*>(instr);
    int16_t value = riinstr->I2Value() * 2;
    if (value >= 0)
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "*+");
    else
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "*");

    out_buffer_pos_ += SNPrintF(
        out_buffer_ + out_buffer_pos_, "%d -> %s", value,
        converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + value));
    return 2;
  } else if (format[1] == '5') {  // immediate in 16-31, but outputs as offset
    RILInstruction* rilinstr = reinterpret_cast<RILInstruction*>(instr);
    int32_t value = rilinstr->I2Value() * 2;
    if (value >= 0)
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "*+");
    else
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "*");

    out_buffer_pos_ += SNPrintF(
        out_buffer_ + out_buffer_pos_, "%d -> %s", value,
        converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + value));
    return 2;
  } else if (format[1] == '6') {  // unsigned immediate in 16-31
    RIInstruction* riinstr = reinterpret_cast<RIInstruction*>(instr);
    uint16_t value = riinstr->I2UnsignedValue();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '7') {  // unsigned immediate in 16-47
    RILInstruction* rilinstr = reinterpret_cast<RILInstruction*>(instr);
    uint32_t value = rilinstr->I2UnsignedValue();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '8') {  // unsigned immediate in 8-15
    SSInstruction* ssinstr = reinterpret_cast<SSInstruction*>(instr);
    uint8_t value = ssinstr->Length();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == '9') {  // unsigned immediate in 16-23
    RIEInstruction* rie_instr = reinterpret_cast<RIEInstruction*>(instr);
    uint8_t value = rie_instr->I3Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == 'a') {  // unsigned immediate in 24-31
    RIEInstruction* rie_instr = reinterpret_cast<RIEInstruction*>(instr);
    uint8_t value = rie_instr->I4Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == 'b') {  // unsigned immediate in 32-39
    RIEInstruction* rie_instr = reinterpret_cast<RIEInstruction*>(instr);
    uint8_t value = rie_instr->I5Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == 'c') {  // signed immediate in 8-15
    SSInstruction* ssinstr = reinterpret_cast<SSInstruction*>(instr);
    int8_t value = ssinstr->Length();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == 'd') {  // signed immediate in 32-47
    SILInstruction* silinstr = reinterpret_cast<SILInstruction*>(instr);
    int16_t value = silinstr->I2Value();
    out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%d", value);
    return 2;
  } else if (format[1] == 'e') {  // immediate in 16-47, but outputs as offset
    RILInstruction* rilinstr = reinterpret_cast<RILInstruction*>(instr);
    int32_t value = rilinstr->I2Value() * 2;
    if (value >= 0)
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "*+");
    else
      out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "*");

    out_buffer_pos_ += SNPrintF(
        out_buffer_ + out_buffer_pos_, "%d -> %s", value,
        converter_.NameOfAddress(reinterpret_cast<byte*>(instr) + value));
    return 2;
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
  out_buffer_[out_buffer_pos_] = '\0';
}

// The disassembler may end up decoding data inlined in the code. We do not want
// it to crash if the data does not ressemble any known instruction.
#define VERIFY(condition) \
  if (!(condition)) {     \
    Unknown(instr);       \
    return;               \
  }

// For currently unimplemented decodings the disassembler calls Unknown(instr)
// which will just print "unknown" of the instruction bits.
void Decoder::Unknown(Instruction* instr) { Format(instr, "unknown"); }

// For currently unimplemented decodings the disassembler calls
// UnknownFormat(instr) which will just print opcode name of the
// instruction bits.
void Decoder::UnknownFormat(Instruction* instr, const char* name) {
  char buffer[100];
  snprintf(buffer, sizeof(buffer), "%s (unknown-format)", name);
  Format(instr, buffer);
}

// Disassembles Two Byte S390 Instructions
// @return true if successfully decoded
bool Decoder::DecodeTwoByte(Instruction* instr) {
  // Print the Instruction bits.
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%04x           ",
                              instr->InstructionBits<TwoByteInstr>());

  Opcode opcode = instr->S390OpcodeValue();
  switch (opcode) {
    case AR:
      Format(instr, "ar\t'r1,'r2");
      break;
    case SR:
      Format(instr, "sr\t'r1,'r2");
      break;
    case MR:
      Format(instr, "mr\t'r1,'r2");
      break;
    case DR:
      Format(instr, "dr\t'r1,'r2");
      break;
    case OR:
      Format(instr, "or\t'r1,'r2");
      break;
    case NR:
      Format(instr, "nr\t'r1,'r2");
      break;
    case XR:
      Format(instr, "xr\t'r1,'r2");
      break;
    case LR:
      Format(instr, "lr\t'r1,'r2");
      break;
    case CR:
      Format(instr, "cr\t'r1,'r2");
      break;
    case CLR:
      Format(instr, "clr\t'r1,'r2");
      break;
    case BCR:
      Format(instr, "bcr\t'm1,'r2");
      break;
    case LTR:
      Format(instr, "ltr\t'r1,'r2");
      break;
    case ALR:
      Format(instr, "alr\t'r1,'r2");
      break;
    case SLR:
      Format(instr, "slr\t'r1,'r2");
      break;
    case LNR:
      Format(instr, "lnr\t'r1,'r2");
      break;
    case LCR:
      Format(instr, "lcr\t'r1,'r2");
      break;
    case BASR:
      Format(instr, "basr\t'r1,'r2");
      break;
    case LDR:
      Format(instr, "ldr\t'f1,'f2");
      break;
    case BKPT:
      Format(instr, "bkpt");
      break;
    default:
      return false;
  }
  return true;
}

// Disassembles Four Byte S390 Instructions
// @return true if successfully decoded
bool Decoder::DecodeFourByte(Instruction* instr) {
  // Print the Instruction bits.
  out_buffer_pos_ += SNPrintF(out_buffer_ + out_buffer_pos_, "%08x       ",
                              instr->InstructionBits<FourByteInstr>());

  Opcode opcode = instr->S390OpcodeValue();
  switch (opcode) {
    case AHI:
      Format(instr, "ahi\t'r1,'i1");
      break;
    case AGHI:
      Format(instr, "aghi\t'r1,'i1");
      break;
    case LHI:
      Format(instr, "lhi\t'r1,'i1");
      break;
    case LGHI:
      Format(instr, "lghi\t'r1,'i1");
      break;
    case MHI:
      Format(instr, "mhi\t'r1,'i1");
      break;
    case MGHI:
      Format(instr, "mghi\t'r1,'i1");
      break;
    case CHI:
      Format(instr, "chi\t'r1,'i1");
      break;
    case CGHI:
      Format(instr, "cghi\t'r1,'i1");
      break;
    case BRAS:
      Format(instr, "bras\t'r1,'i1");
      break;
    case BRC:
      Format(instr, "brc\t'm1,'i4");
      break;
    case BRCT:
      Format(instr, "brct\t'r1,'i4");
      break;
    case BRCTG:
      Format(instr, "brctg\t'r1,'i4");
      break;
    case IIHH:
      Format(instr, "iihh\t'r1,'i1");
      break;
    case IIHL:
      Format(instr, "iihl\t'r1,'i1");
      break;
    case IILH:
      Format(instr, "iilh\t'r1,'i1");
      break;
    case IILL:
      Format(instr, "iill\t'r1,'i1");
      break;
    case OILL:
      Format(instr, "oill\t'r1,'i1");
      break;
    case TMLL:
      Format(instr, "tmll\t'r1,'i1");
      break;
    case STM:
      Format(instr, "stm\t'r1,'r2,'d1('r3)");
      break;
    case LM:
      Format(instr, "lm\t'r1,'r2,'d1('r3)");
      break;
    case SLL:
      Format(instr, "sll\t'r1,'d1('r3)");
      break;
    case SRL:
      Format(instr, "srl\t'r1,'d1('r3)");
      break;
    case SLA:
      Format(instr, "sla\t'r1,'d1('r3)");
      break;
    case SRA:
      Format(instr, "sra\t'r1,'d1('r3)");
      break;
    case SLDL:
      Format(instr, "sldl\t'r1,'d1('r3)");
      break;
    case AGR:
      Format(instr, "agr\t'r5,'r6");
      break;
    case AGFR:
      Format(instr, "agfr\t'r5,'r6");
      break;
    case ARK:
      Format(instr, "ark\t'r5,'r6,'r3");
      break;
    case AGRK:
      Format(instr, "agrk\t'r5,'r6,'r3");
      break;
    case SGR:
      Format(instr, "sgr\t'r5,'r6");
      break;
    case SGFR:
      Format(instr, "sgfr\t'r5,'r6");
      break;
    case SRK:
      Format(instr, "srk\t'r5,'r6,'r3");
      break;
    case SGRK:
      Format(instr, "sgrk\t'r5,'r6,'r3");
      break;
    case NGR:
      Format(instr, "ngr\t'r5,'r6");
      break;
    case NRK:
      Format(instr, "nrk\t'r5,'r6,'r3");
      break;
    case NGRK:
      Format(instr, "ngrk\t'r5,'r6,'r3");
      break;
    case NILL:
      Format(instr, "nill\t'r1,'i1");
      break;
    case NILH:
      Format(instr, "nilh\t'r1,'i1");
      break;
    case OGR:
      Format(instr, "ogr\t'r5,'r6");
      break;
    case ORK:
      Format(instr, "ork\t'r5,'r6,'r3");
      break;
    case OGRK:
      Format(instr, "ogrk\t'r5,'r6,'r3");
      break;
    case XGR:
      Format(instr, "xgr\t'r5,'r6");
      break;
    case XRK:
      Format(instr, "xrk\t'r5,'r6,'r3");
      break;
    case XGRK:
      Format(instr, "xgrk\t'r5,'r6,'r3");
      break;
    case CGR:
      Format(instr, "cgr\t'r5,'r6");
      break;
    case CLGR:
      Format(instr, "clgr\t'r5,'r6");
      break;
    case LLGFR:
      Format(instr, "llgfr\t'r5,'r6");
      break;
    case LBR:
      Format(instr, "lbr\t'r5,'r6");
      break;
    case LEDBR:
      Format(instr, "ledbr\t'f5,'f6");
      break;
    case LDEBR:
      Format(instr, "ldebr\t'f5,'f6");
      break;
    case LTGR:
      Format(instr, "ltgr\t'r5,'r6");
      break;
    case LTDBR:
      Format(instr, "ltdbr\t'f5,'f6");
      break;
    case LTEBR:
      Format(instr, "ltebr\t'f5,'f6");
      break;
    case LRVR:
      Format(instr, "lrvr\t'r5,'r6");
      break;
    case LRVGR:
      Format(instr, "lrvgr\t'r5,'r6");
      break;
    case LGR:
      Format(instr, "lgr\t'r5,'r6");
      break;
    case LGDR:
      Format(instr, "lgdr\t'r5,'f6");
      break;
    case LGFR:
      Format(instr, "lgfr\t'r5,'r6");
      break;
    case LTGFR:
      Format(instr, "ltgfr\t'r5,'r6");
      break;
    case LCGR:
      Format(instr, "lcgr\t'r5,'r6");
      break;
    case MSR:
      Format(instr, "msr\t'r5,'r6");
      break;
    case LGBR:
      Format(instr, "lgbr\t'r5,'r6");
      break;
    case LGHR:
      Format(instr, "lghr\t'r5,'r6");
      break;
    case MSGR:
      Format(instr, "msgr\t'r5,'r6");
      break;
    case DSGR:
      Format(instr, "dsgr\t'r5,'r6");
      break;
    case LZDR:
      Format(instr, "lzdr\t'f5");
      break;
    case MLR:
      Format(instr, "mlr\t'r5,'r6");
      break;
    case MLGR:
      Format(instr, "mlgr\t'r5,'r6");
      break;
    case ALCR:
      Format(instr, "alcr\t'r5,'r6");
      break;
    case ALGR:
      Format(instr, "algr\t'r5,'r6");
      break;
    case ALRK:
      Format(instr, "alrk\t'r5,'r6,'r3");
      break;
    case ALGRK:
      Format(instr, "algrk\t'r5,'r6,'r3");
      break;
    case SLGR:
      Format(instr, "slgr\t'r5,'r6");
      break;
    case SLBR:
      Format(instr, "slbr\t'r5,'r6");
      break;
    case DLR:
      Format(instr, "dlr\t'r5,'r6");
      break;
    case DLGR:
      Format(instr, "dlgr\t'r5,'r6");
      break;
    case SLRK:
      Format(instr, "slrk\t'r5,'r6,'r3");
      break;
    case SLGRK:
      Format(instr, "slgrk\t'r5,'r6,'r3");
      break;
    case LHR:
      Format(instr, "lhr\t'r5,'r6");
      break;
    case LLHR:
      Format(instr, "llhr\t'r5,'r6");
      break;
    case LLGHR:
      Format(instr, "llghr\t'r5,'r6");
      break;
    case LOCR:
      Format(instr, "locr\t'm1,'r5,'r6");
      break;
    case LOCGR:
      Format(instr, "locgr\t'm1,'r5,'r6");
      break;
    case LNGR:
      Format(instr, "lngr\t'r5,'r6");
      break;
    case A:
      Format(instr, "a\t'r1,'d1('r2d,'r3)");
      break;
    case S:
      Format(instr, "s\t'r1,'d1('r2d,'r3)");
      break;
    case M:
      Format(instr, "m\t'r1,'d1('r2d,'r3)");
      break;
    case D:
      Format(instr, "d\t'r1,'d1('r2d,'r3)");
      break;
    case O:
      Format(instr, "o\t'r1,'d1('r2d,'r3)");
      break;
    case N:
      Format(instr, "n\t'r1,'d1('r2d,'r3)");
      break;
    case L:
      Format(instr, "l\t'r1,'d1('r2d,'r3)");
      break;
    case C:
      Format(instr, "c\t'r1,'d1('r2d,'r3)");
      break;
    case AH:
      Format(instr, "ah\t'r1,'d1('r2d,'r3)");
      break;
    case SH:
      Format(instr, "sh\t'r1,'d1('r2d,'r3)");
      break;
    case MH:
      Format(instr, "mh\t'r1,'d1('r2d,'r3)");
      break;
    case AL:
      Format(instr, "al\t'r1,'d1('r2d,'r3)");
      break;
    case SL:
      Format(instr, "sl\t'r1,'d1('r2d,'r3)");
      break;
    case LA:
      Format(instr, "la\t'r1,'d1('r2d,'r3)");
      break;
    case CH:
      Format(instr, "ch\t'r1,'d1('r2d,'r3)");
      break;
    case CL:
      Format(instr, "cl\t'r1,'d1('r2d,'r3)");
      break;
    case CLI:
      Format(instr, "cli\t'd1('r3),'i8");
      break;
    case TM:
      Format(instr, "tm\t'd1('r3),'i8");
      break;
    case BC:
      Format(instr, "bc\t'm1,'d1('r2d,'r3)");
      break;
    case BCT:
      Format(instr, "bct\t'r1,'d1('r2d,'r3)");
      break;
    case ST:
      Format(instr, "st\t'r1,'d1('r2d,'r3)");
      break;
    case STC:
      Format(instr, "stc\t'r1,'d1('r2d,'r3)");
      break;
    case IC_z:
      Format(instr, "ic\t'r1,'d1('r2d,'r3)");
      break;
    case LD:
      Format(instr, "ld\t'f1,'d1('r2d,'r3)");
      break;
    case LE:
      Format(instr, "le\t'f1,'d1('r2d,'r3)");
      break;
    case LDGR:
      Format(instr, "ldgr\t'f5,'r6");
      break;
    case MS:
      Format(instr, "ms\t'r1,'d1('r2d,'r3)");
      break;
    case STE:
      Format(instr, "ste\t'f1,'d1('r2d,'r3)");
      break;
    case STD:
      Format(instr, "std\t'f1,'d1('r2d,'r3)");
      break;
    case CFDBR:
      Format(instr, "cfdbr\t'r5,'m2,'f6");
      break;
    case CDFBR:
      Format(instr, "cdfbr\t'f5,'m2,'r6");
      break;
    case CFEBR:
      Format(instr, "cfebr\t'r5,'m2,'f6");
      break;
    case CEFBR:
      Format(instr, "cefbr\t'f5,'m2,'r6");
      break;
    case CELFBR:
      Format(instr, "celfbr\t'f5,'m2,'r6");
      break;
    case CGEBR:
      Format(instr, "cgebr\t'r5,'m2,'f6");
      break;
    case CGDBR:
      Format(instr, "cgdbr\t'r5,'m2,'f6");
      break;
    case CEGBR:
      Format(instr, "cegbr\t'f5,'m2,'r6");
      break;
    case CDGBR:
      Format(instr, "cdgbr\t'f5,'m2,'r6");
      break;
    case CDLFBR:
      Format(instr, "cdlfbr\t'f5,'m2,'r6");
      break;
    case CDLGBR:
      Format(instr, "cdlgbr\t'f5,'m2,'r6");
      break;
    case CELGBR:
      Format(instr, "celgbr\t'f5,'m2,'r6");
      break;
    case CLFDBR:
      Format(instr, "clfdbr\t'r5,'m2,'f6");
      break;
    case CLFEBR:
      Format(instr, "clfebr\t'r5,'m2,'f6");
      break;
    case CLGEBR:
      Format(instr, "clgebr\t'r5,'m2,'f6");
      break;
    case CLGDBR:
      Format(instr, "clgdbr\t'r5,'m2,'f6");
      break;
    case AEBR:
      Format(instr, "aebr\t'f5,'f6");
      break;
    case SEBR:
      Format(instr, "sebr\t'f5,'f6");
      break;
    case MEEBR:
      Format(instr, "meebr\t'f5,'f6");
      break;
    case DEBR:
      Format(instr, "debr\t'f5,'f6");
      break;
    case ADBR:
      Format(instr, "adbr\t'f5,'f6");
      break;
    case SDBR:
      Format(instr, "sdbr\t'f5,'f6");
      break;
    case MDBR:
      Format(instr, "mdbr\t'f5,'f6");
      break;
    case DDBR:
      Format(instr, "ddbr\t'f5,'f6");
      break;
    case CDBR:
      Format(instr, "cdbr\t'f5,'f6");
      break;
    case CEBR:
      Format(instr, "cebr\t'f5,'f6");
      break;
    case SQDBR:
      Format(instr, "sqdbr\t'f5,'f6");
      break;
    case SQEBR:
      Format(instr, "sqebr\t'f5,'f6");
      break;
    case LCDBR:
      Format(instr, "lcdbr\t'f5,'f6");
      break;
    case LCEBR:
      Format(instr, "lcebr\t'f5,'f6");
      break;
    case STH:
      Format(instr, "sth\t'r1,'d1('r2d,'r3)");
      break;
    case SRDA:
      Format(instr, "srda\t'r1,'d1('r3)");
      break;
    case SRDL:
      Format(instr, "srdl\t'r1,'d1('r3)");
      break;
    case MADBR:
      Format(instr, "madbr\t'f3,'f5,'f6");
      break;
    case MSDBR:
      Format(instr, "msdbr\t'f3,'f5,'f6");
      break;
    case FLOGR:
      Format(instr, "flogr\t'r5,'r6");
      break;
    case FIEBRA:
      Format(instr, "fiebra\t'f5,'m2,'f6,'m3");
      break;
    case FIDBRA:
      Format(instr, "fidbra\t'f5,'m2,'f6,'m3");
      break;
    // TRAP4 is used in calling to native function. it will not be generated
    // in native code.
    case TRAP4: {
      Format(instr, "trap4");
      break;
    }
    default:
      return false;
  }
  return true;
}

// Disassembles Six Byte S390 Instructions
// @return true if successfully decoded
bool Decoder::DecodeSixByte(Instruction* instr) {
  // Print the Instruction bits.
  out_buffer_pos_ +=
      SNPrintF(out_buffer_ + out_buffer_pos_, "%012" PRIx64 "   ",
               instr->InstructionBits<SixByteInstr>());

  Opcode opcode = instr->S390OpcodeValue();
  switch (opcode) {
    case LLILF:
      Format(instr, "llilf\t'r1,'i7");
      break;
    case LLIHF:
      Format(instr, "llihf\t'r1,'i7");
      break;
    case AFI:
      Format(instr, "afi\t'r1,'i7");
      break;
    case ASI:
      Format(instr, "asi\t'd2('r3),'ic");
      break;
    case AGSI:
      Format(instr, "agsi\t'd2('r3),'ic");
      break;
    case ALFI:
      Format(instr, "alfi\t'r1,'i7");
      break;
    case AHIK:
      Format(instr, "ahik\t'r1,'r2,'i1");
      break;
    case AGHIK:
      Format(instr, "aghik\t'r1,'r2,'i1");
      break;
    case CLGFI:
      Format(instr, "clgfi\t'r1,'i7");
      break;
    case CLFI:
      Format(instr, "clfi\t'r1,'i7");
      break;
    case CFI:
      Format(instr, "cfi\t'r1,'i2");
      break;
    case CGFI:
      Format(instr, "cgfi\t'r1,'i2");
      break;
    case BRASL:
      Format(instr, "brasl\t'r1,'ie");
      break;
    case BRCL:
      Format(instr, "brcl\t'm1,'i5");
      break;
    case IIHF:
      Format(instr, "iihf\t'r1,'i7");
      break;
    case LGFI:
      Format(instr, "lgfi\t'r1,'i7");
      break;
    case IILF:
      Format(instr, "iilf\t'r1,'i7");
      break;
    case XIHF:
      Format(instr, "xihf\t'r1,'i7");
      break;
    case XILF:
      Format(instr, "xilf\t'r1,'i7");
      break;
    case SLLK:
      Format(instr, "sllk\t'r1,'r2,'d2('r3)");
      break;
    case SLLG:
      Format(instr, "sllg\t'r1,'r2,'d2('r3)");
      break;
    case RLL:
      Format(instr, "rll\t'r1,'r2,'d2('r3)");
      break;
    case RLLG:
      Format(instr, "rllg\t'r1,'r2,'d2('r3)");
      break;
    case SRLK:
      Format(instr, "srlk\t'r1,'r2,'d2('r3)");
      break;
    case SRLG:
      Format(instr, "srlg\t'r1,'r2,'d2('r3)");
      break;
    case SLAK:
      Format(instr, "slak\t'r1,'r2,'d2('r3)");
      break;
    case SLAG:
      Format(instr, "slag\t'r1,'r2,'d2('r3)");
      break;
    case SRAK:
      Format(instr, "srak\t'r1,'r2,'d2('r3)");
      break;
    case SRAG:
      Format(instr, "srag\t'r1,'r2,'d2('r3)");
      break;
    case RISBG:
      Format(instr, "risbg\t'r1,'r2,'i9,'ia,'ib");
      break;
    case RISBGN:
      Format(instr, "risbgn\t'r1,'r2,'i9,'ia,'ib");
      break;
    case LOCG:
      Format(instr, "locg\t'm2,'r1,'d2('r3)");
      break;
    case LOC:
      Format(instr, "loc\t'm2,'r1,'d2('r3)");
      break;
    case LMY:
      Format(instr, "lmy\t'r1,'r2,'d2('r3)");
      break;
    case LMG:
      Format(instr, "lmg\t'r1,'r2,'d2('r3)");
      break;
    case STMY:
      Format(instr, "stmy\t'r1,'r2,'d2('r3)");
      break;
    case STMG:
      Format(instr, "stmg\t'r1,'r2,'d2('r3)");
      break;
    case LT:
      Format(instr, "lt\t'r1,'d2('r2d,'r3)");
      break;
    case LTG:
      Format(instr, "ltg\t'r1,'d2('r2d,'r3)");
      break;
    case ML:
      Format(instr, "ml\t'r1,'d2('r2d,'r3)");
      break;
    case AY:
      Format(instr, "ay\t'r1,'d2('r2d,'r3)");
      break;
    case SY:
      Format(instr, "sy\t'r1,'d2('r2d,'r3)");
      break;
    case NY:
      Format(instr, "ny\t'r1,'d2('r2d,'r3)");
      break;
    case OY:
      Format(instr, "oy\t'r1,'d2('r2d,'r3)");
      break;
    case XY:
      Format(instr, "xy\t'r1,'d2('r2d,'r3)");
      break;
    case CY:
      Format(instr, "cy\t'r1,'d2('r2d,'r3)");
      break;
    case AHY:
      Format(instr, "ahy\t'r1,'d2('r2d,'r3)");
      break;
    case SHY:
      Format(instr, "shy\t'r1,'d2('r2d,'r3)");
      break;
    case LGH:
      Format(instr, "lgh\t'r1,'d2('r2d,'r3)");
      break;
    case AG:
      Format(instr, "ag\t'r1,'d2('r2d,'r3)");
      break;
    case AGF:
      Format(instr, "agf\t'r1,'d2('r2d,'r3)");
      break;
    case SG:
      Format(instr, "sg\t'r1,'d2('r2d,'r3)");
      break;
    case NG:
      Format(instr, "ng\t'r1,'d2('r2d,'r3)");
      break;
    case OG:
      Format(instr, "og\t'r1,'d2('r2d,'r3)");
      break;
    case XG:
      Format(instr, "xg\t'r1,'d2('r2d,'r3)");
      break;
    case CG:
      Format(instr, "cg\t'r1,'d2('r2d,'r3)");
      break;
    case LB:
      Format(instr, "lb\t'r1,'d2('r2d,'r3)");
      break;
    case LRVH:
      Format(instr, "lrvh\t'r1,'d2('r2d,'r3)");
      break;
    case LRV:
      Format(instr, "lrv\t'r1,'d2('r2d,'r3)");
      break;
    case LRVG:
      Format(instr, "lrvg\t'r1,'d2('r2d,'r3)");
      break;
    case LG:
      Format(instr, "lg\t'r1,'d2('r2d,'r3)");
      break;
    case LGF:
      Format(instr, "lgf\t'r1,'d2('r2d,'r3)");
      break;
    case LLGF:
      Format(instr, "llgf\t'r1,'d2('r2d,'r3)");
      break;
    case LY:
      Format(instr, "ly\t'r1,'d2('r2d,'r3)");
      break;
    case ALY:
      Format(instr, "aly\t'r1,'d2('r2d,'r3)");
      break;
    case ALG:
      Format(instr, "alg\t'r1,'d2('r2d,'r3)");
      break;
    case SLG:
      Format(instr, "slg\t'r1,'d2('r2d,'r3)");
      break;
    case SGF:
      Format(instr, "sgf\t'r1,'d2('r2d,'r3)");
      break;
    case SLY:
      Format(instr, "sly\t'r1,'d2('r2d,'r3)");
      break;
    case LLH:
      Format(instr, "llh\t'r1,'d2('r2d,'r3)");
      break;
    case LLGH:
      Format(instr, "llgh\t'r1,'d2('r2d,'r3)");
      break;
    case LLC:
      Format(instr, "llc\t'r1,'d2('r2d,'r3)");
      break;
    case LLGC:
      Format(instr, "llgc\t'r1,'d2('r2d,'r3)");
      break;
    case LDEB:
      Format(instr, "ldeb\t'f1,'d2('r2d,'r3)");
      break;
    case LAY:
      Format(instr, "lay\t'r1,'d2('r2d,'r3)");
      break;
    case LARL:
      Format(instr, "larl\t'r1,'i5");
      break;
    case LGB:
      Format(instr, "lgb\t'r1,'d2('r2d,'r3)");
      break;
    case CHY:
      Format(instr, "chy\t'r1,'d2('r2d,'r3)");
      break;
    case CLY:
      Format(instr, "cly\t'r1,'d2('r2d,'r3)");
      break;
    case CLIY:
      Format(instr, "cliy\t'd2('r3),'i8");
      break;
    case TMY:
      Format(instr, "tmy\t'd2('r3),'i8");
      break;
    case CLG:
      Format(instr, "clg\t'r1,'d2('r2d,'r3)");
      break;
    case BCTG:
      Format(instr, "bctg\t'r1,'d2('r2d,'r3)");
      break;
    case STY:
      Format(instr, "sty\t'r1,'d2('r2d,'r3)");
      break;
    case STRVH:
      Format(instr, "strvh\t'r1,'d2('r2d,'r3)");
      break;
    case STRV:
      Format(instr, "strv\t'r1,'d2('r2d,'r3)");
      break;
    case STRVG:
      Format(instr, "strvg\t'r1,'d2('r2d,'r3)");
      break;
    case STG:
      Format(instr, "stg\t'r1,'d2('r2d,'r3)");
      break;
    case ICY:
      Format(instr, "icy\t'r1,'d2('r2d,'r3)");
      break;
    case MVC:
      Format(instr, "mvc\t'd3('i8,'r3),'d4('r7)");
      break;
    case MVHI:
      Format(instr, "mvhi\t'd3('r3),'id");
      break;
    case MVGHI:
      Format(instr, "mvghi\t'd3('r3),'id");
      break;
    case ALGFI:
      Format(instr, "algfi\t'r1,'i7");
      break;
    case SLGFI:
      Format(instr, "slgfi\t'r1,'i7");
      break;
    case SLFI:
      Format(instr, "slfi\t'r1,'i7");
      break;
    case NIHF:
      Format(instr, "nihf\t'r1,'i7");
      break;
    case NILF:
      Format(instr, "nilf\t'r1,'i7");
      break;
    case OIHF:
      Format(instr, "oihf\t'r1,'i7");
      break;
    case OILF:
      Format(instr, "oilf\t'r1,'i7");
      break;
    case MSFI:
      Format(instr, "msfi\t'r1,'i7");
      break;
    case MSGFI:
      Format(instr, "msgfi\t'r1,'i7");
      break;
    case LDY:
      Format(instr, "ldy\t'f1,'d2('r2d,'r3)");
      break;
    case LEY:
      Format(instr, "ley\t'f1,'d2('r2d,'r3)");
      break;
    case MSG:
      Format(instr, "msg\t'r1,'d2('r2d,'r3)");
      break;
    case MSY:
      Format(instr, "msy\t'r1,'d2('r2d,'r3)");
      break;
    case STEY:
      Format(instr, "stey\t'f1,'d2('r2d,'r3)");
      break;
    case STDY:
      Format(instr, "stdy\t'f1,'d2('r2d,'r3)");
      break;
    case ADB:
      Format(instr, "adb\t'r1,'d1('r2d, 'r3)");
      break;
    case SDB:
      Format(instr, "sdb\t'r1,'d1('r2d, 'r3)");
      break;
    case MDB:
      Format(instr, "mdb\t'r1,'d1('r2d, 'r3)");
      break;
    case DDB:
      Format(instr, "ddb\t'r1,'d1('r2d, 'r3)");
      break;
    case SQDB:
      Format(instr, "sqdb\t'r1,'d1('r2d, 'r3)");
      break;
    default:
      return false;
  }
  return true;
}

#undef VERIFIY

// Disassemble the instruction at *instr_ptr into the output buffer.
int Decoder::InstructionDecode(byte* instr_ptr) {
  Instruction* instr = Instruction::At(instr_ptr);
  int instrLength = instr->InstructionLength();

  if (2 == instrLength)
    DecodeTwoByte(instr);
  else if (4 == instrLength)
    DecodeFourByte(instr);
  else
    DecodeSixByte(instr);

  return instrLength;
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
  UNREACHABLE();  // S390 does not have the concept of a byte register
  return "nobytereg";
}

const char* NameConverter::NameOfXMMRegister(int reg) const {
  // S390 does not have XMM register
  // TODO(joransiu): Consider update this for Vector Regs
  UNREACHABLE();
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

// The S390 assembler does not currently use constant pools.
int Disassembler::ConstantPoolSizeAt(byte* instruction) { return -1; }

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

#endif  // V8_TARGET_ARCH_S390

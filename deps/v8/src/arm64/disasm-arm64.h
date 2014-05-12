// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_DISASM_ARM64_H
#define V8_ARM64_DISASM_ARM64_H

#include "v8.h"

#include "globals.h"
#include "utils.h"
#include "instructions-arm64.h"
#include "decoder-arm64.h"

namespace v8 {
namespace internal {


class Disassembler: public DecoderVisitor {
 public:
  Disassembler();
  Disassembler(char* text_buffer, int buffer_size);
  virtual ~Disassembler();
  char* GetOutput();

  // Declare all Visitor functions.
  #define DECLARE(A)  void Visit##A(Instruction* instr);
  VISITOR_LIST(DECLARE)
  #undef DECLARE

 protected:
  virtual void ProcessOutput(Instruction* instr);

  void Format(Instruction* instr, const char* mnemonic, const char* format);
  void Substitute(Instruction* instr, const char* string);
  int SubstituteField(Instruction* instr, const char* format);
  int SubstituteRegisterField(Instruction* instr, const char* format);
  int SubstituteImmediateField(Instruction* instr, const char* format);
  int SubstituteLiteralField(Instruction* instr, const char* format);
  int SubstituteBitfieldImmediateField(Instruction* instr, const char* format);
  int SubstituteShiftField(Instruction* instr, const char* format);
  int SubstituteExtendField(Instruction* instr, const char* format);
  int SubstituteConditionField(Instruction* instr, const char* format);
  int SubstitutePCRelAddressField(Instruction* instr, const char* format);
  int SubstituteBranchTargetField(Instruction* instr, const char* format);
  int SubstituteLSRegOffsetField(Instruction* instr, const char* format);
  int SubstitutePrefetchField(Instruction* instr, const char* format);
  int SubstituteBarrierField(Instruction* instr, const char* format);

  bool RdIsZROrSP(Instruction* instr) const {
    return (instr->Rd() == kZeroRegCode);
  }

  bool RnIsZROrSP(Instruction* instr) const {
    return (instr->Rn() == kZeroRegCode);
  }

  bool RmIsZROrSP(Instruction* instr) const {
    return (instr->Rm() == kZeroRegCode);
  }

  bool RaIsZROrSP(Instruction* instr) const {
    return (instr->Ra() == kZeroRegCode);
  }

  bool IsMovzMovnImm(unsigned reg_size, uint64_t value);

  void ResetOutput();
  void AppendToOutput(const char* string, ...);

  char* buffer_;
  uint32_t buffer_pos_;
  uint32_t buffer_size_;
  bool own_buffer_;
};


class PrintDisassembler: public Disassembler {
 public:
  explicit PrintDisassembler(FILE* stream) : stream_(stream) { }
  ~PrintDisassembler() { }

  virtual void ProcessOutput(Instruction* instr);

 private:
  FILE *stream_;
};


} }  // namespace v8::internal

#endif  // V8_ARM64_DISASM_ARM64_H

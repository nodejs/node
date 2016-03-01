// Copyright 2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_MACRO_ASSEMBLER_TRACER_H_
#define V8_REGEXP_REGEXP_MACRO_ASSEMBLER_TRACER_H_

#include "src/regexp/regexp-macro-assembler.h"

namespace v8 {
namespace internal {

// Decorator on a RegExpMacroAssembler that write all calls.
class RegExpMacroAssemblerTracer: public RegExpMacroAssembler {
 public:
  RegExpMacroAssemblerTracer(Isolate* isolate, RegExpMacroAssembler* assembler);
  virtual ~RegExpMacroAssemblerTracer();
  virtual void AbortedCodeGeneration();
  virtual int stack_limit_slack() { return assembler_->stack_limit_slack(); }
  virtual bool CanReadUnaligned() { return assembler_->CanReadUnaligned(); }
  virtual void AdvanceCurrentPosition(int by);  // Signed cp change.
  virtual void AdvanceRegister(int reg, int by);  // r[reg] += by.
  virtual void Backtrack();
  virtual void Bind(Label* label);
  virtual void CheckAtStart(Label* on_at_start);
  virtual void CheckCharacter(unsigned c, Label* on_equal);
  virtual void CheckCharacterAfterAnd(unsigned c,
                                      unsigned and_with,
                                      Label* on_equal);
  virtual void CheckCharacterGT(uc16 limit, Label* on_greater);
  virtual void CheckCharacterLT(uc16 limit, Label* on_less);
  virtual void CheckGreedyLoop(Label* on_tos_equals_current_position);
  virtual void CheckNotAtStart(int cp_offset, Label* on_not_at_start);
  virtual void CheckNotBackReference(int start_reg, bool read_backward,
                                     Label* on_no_match);
  virtual void CheckNotBackReferenceIgnoreCase(int start_reg,
                                               bool read_backward,
                                               Label* on_no_match);
  virtual void CheckNotCharacter(unsigned c, Label* on_not_equal);
  virtual void CheckNotCharacterAfterAnd(unsigned c,
                                         unsigned and_with,
                                         Label* on_not_equal);
  virtual void CheckNotCharacterAfterMinusAnd(uc16 c,
                                              uc16 minus,
                                              uc16 and_with,
                                              Label* on_not_equal);
  virtual void CheckCharacterInRange(uc16 from,
                                     uc16 to,
                                     Label* on_in_range);
  virtual void CheckCharacterNotInRange(uc16 from,
                                        uc16 to,
                                        Label* on_not_in_range);
  virtual void CheckBitInTable(Handle<ByteArray> table, Label* on_bit_set);
  virtual void CheckPosition(int cp_offset, Label* on_outside_input);
  virtual bool CheckSpecialCharacterClass(uc16 type,
                                          Label* on_no_match);
  virtual void Fail();
  virtual Handle<HeapObject> GetCode(Handle<String> source);
  virtual void GoTo(Label* label);
  virtual void IfRegisterGE(int reg, int comparand, Label* if_ge);
  virtual void IfRegisterLT(int reg, int comparand, Label* if_lt);
  virtual void IfRegisterEqPos(int reg, Label* if_eq);
  virtual IrregexpImplementation Implementation();
  virtual void LoadCurrentCharacter(int cp_offset,
                                    Label* on_end_of_input,
                                    bool check_bounds = true,
                                    int characters = 1);
  virtual void PopCurrentPosition();
  virtual void PopRegister(int register_index);
  virtual void PushBacktrack(Label* label);
  virtual void PushCurrentPosition();
  virtual void PushRegister(int register_index,
                            StackCheckFlag check_stack_limit);
  virtual void ReadCurrentPositionFromRegister(int reg);
  virtual void ReadStackPointerFromRegister(int reg);
  virtual void SetCurrentPositionFromEnd(int by);
  virtual void SetRegister(int register_index, int to);
  virtual bool Succeed();
  virtual void WriteCurrentPositionToRegister(int reg, int cp_offset);
  virtual void ClearRegisters(int reg_from, int reg_to);
  virtual void WriteStackPointerToRegister(int reg);

 private:
  RegExpMacroAssembler* assembler_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_MACRO_ASSEMBLER_TRACER_H_

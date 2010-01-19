// Copyright 2008 the V8 project authors. All rights reserved.
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

#ifndef V8_REGEXP_MACRO_ASSEMBLER_TRACER_H_
#define V8_REGEXP_MACRO_ASSEMBLER_TRACER_H_

namespace v8 {
namespace internal {

// Decorator on a RegExpMacroAssembler that write all calls.
class RegExpMacroAssemblerTracer: public RegExpMacroAssembler {
 public:
  explicit RegExpMacroAssemblerTracer(RegExpMacroAssembler* assembler);
  virtual ~RegExpMacroAssemblerTracer();
  virtual int stack_limit_slack() { return assembler_->stack_limit_slack(); }
  virtual bool CanReadUnaligned() { return assembler_->CanReadUnaligned(); }
  virtual void AdvanceCurrentPosition(int by);  // Signed cp change.
  virtual void AdvanceRegister(int reg, int by);  // r[reg] += by.
  virtual void Backtrack();
  virtual void Bind(Label* label);
  virtual void CheckAtStart(Label* on_at_start);
  virtual void CheckCharacter(uint32_t c, Label* on_equal);
  virtual void CheckCharacterAfterAnd(uint32_t c,
                                      uint32_t and_with,
                                      Label* on_equal);
  virtual void CheckCharacterGT(uc16 limit, Label* on_greater);
  virtual void CheckCharacterLT(uc16 limit, Label* on_less);
  virtual void CheckCharacters(
      Vector<const uc16> str,
      int cp_offset,
      Label* on_failure,
      bool check_end_of_string);
  virtual void CheckGreedyLoop(Label* on_tos_equals_current_position);
  virtual void CheckNotAtStart(Label* on_not_at_start);
  virtual void CheckNotBackReference(int start_reg, Label* on_no_match);
  virtual void CheckNotBackReferenceIgnoreCase(int start_reg,
                                               Label* on_no_match);
  virtual void CheckNotRegistersEqual(int reg1, int reg2, Label* on_not_equal);
  virtual void CheckNotCharacter(uint32_t c, Label* on_not_equal);
  virtual void CheckNotCharacterAfterAnd(uint32_t c,
                                         uint32_t and_with,
                                         Label* on_not_equal);
  virtual void CheckNotCharacterAfterMinusAnd(uc16 c,
                                              uc16 minus,
                                              uc16 and_with,
                                              Label* on_not_equal);
  virtual bool CheckSpecialCharacterClass(uc16 type,
                                          Label* on_no_match);
  virtual void Fail();
  virtual Handle<Object> GetCode(Handle<String> source);
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
  virtual void SetRegister(int register_index, int to);
  virtual void Succeed();
  virtual void WriteCurrentPositionToRegister(int reg, int cp_offset);
  virtual void ClearRegisters(int reg_from, int reg_to);
  virtual void WriteStackPointerToRegister(int reg);
 private:
  RegExpMacroAssembler* assembler_;
};

}}  // namespace v8::internal

#endif  // V8_REGEXP_MACRO_ASSEMBLER_TRACER_H_

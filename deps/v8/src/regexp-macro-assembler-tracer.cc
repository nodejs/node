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

#include "v8.h"
#include "ast.h"
#include "regexp-macro-assembler.h"
#include "regexp-macro-assembler-tracer.h"

namespace v8 {
namespace internal {

RegExpMacroAssemblerTracer::RegExpMacroAssemblerTracer(
    RegExpMacroAssembler* assembler) :
  assembler_(assembler) {
  unsigned int type = assembler->Implementation();
  ASSERT(type < 3);
  const char* impl_names[3] = {"IA32", "ARM", "Bytecode"};
  PrintF("RegExpMacroAssembler%s();\n", impl_names[type]);
}


RegExpMacroAssemblerTracer::~RegExpMacroAssemblerTracer() {
}


void RegExpMacroAssemblerTracer::Bind(Label* label) {
  PrintF("label[%08x]: (Bind)\n", label, label);
  assembler_->Bind(label);
}


void RegExpMacroAssemblerTracer::AdvanceCurrentPosition(int by) {
  PrintF(" AdvanceCurrentPosition(by=%d);\n", by);
  assembler_->AdvanceCurrentPosition(by);
}


void RegExpMacroAssemblerTracer::CheckGreedyLoop(Label* label) {
  PrintF(" CheckGreedyLoop(label[%08x]);\n\n", label);
  assembler_->CheckGreedyLoop(label);
}


void RegExpMacroAssemblerTracer::PopCurrentPosition() {
  PrintF(" PopCurrentPosition();\n");
  assembler_->PopCurrentPosition();
}


void RegExpMacroAssemblerTracer::PushCurrentPosition() {
  PrintF(" PushCurrentPosition();\n");
  assembler_->PushCurrentPosition();
}


void RegExpMacroAssemblerTracer::Backtrack() {
  PrintF(" Backtrack();\n");
  assembler_->Backtrack();
}


void RegExpMacroAssemblerTracer::GoTo(Label* label) {
  PrintF(" GoTo(label[%08x]);\n\n", label);
  assembler_->GoTo(label);
}


void RegExpMacroAssemblerTracer::PushBacktrack(Label* label) {
  PrintF(" PushBacktrack(label[%08x]);\n",
         label);
  assembler_->PushBacktrack(label);
}


void RegExpMacroAssemblerTracer::Succeed() {
  PrintF(" Succeed();\n");
  assembler_->Succeed();
}


void RegExpMacroAssemblerTracer::Fail() {
  PrintF(" Fail();\n");
  assembler_->Fail();
}


void RegExpMacroAssemblerTracer::PopRegister(int register_index) {
  PrintF(" PopRegister(register=%d);\n", register_index);
  assembler_->PopRegister(register_index);
}


void RegExpMacroAssemblerTracer::PushRegister(
    int register_index,
    StackCheckFlag check_stack_limit) {
  PrintF(" PushRegister(register=%d, %s);\n",
         register_index,
         check_stack_limit ? "check stack limit" : "");
  assembler_->PushRegister(register_index, check_stack_limit);
}


void RegExpMacroAssemblerTracer::AdvanceRegister(int reg, int by) {
  PrintF(" AdvanceRegister(register=%d, by=%d);\n", reg, by);
  assembler_->AdvanceRegister(reg, by);
}


void RegExpMacroAssemblerTracer::SetRegister(int register_index, int to) {
  PrintF(" SetRegister(register=%d, to=%d);\n", register_index, to);
  assembler_->SetRegister(register_index, to);
}


void RegExpMacroAssemblerTracer::WriteCurrentPositionToRegister(int reg,
                                                                int cp_offset) {
  PrintF(" WriteCurrentPositionToRegister(register=%d,cp_offset=%d);\n",
         reg,
         cp_offset);
  assembler_->WriteCurrentPositionToRegister(reg, cp_offset);
}


void RegExpMacroAssemblerTracer::ClearRegisters(int reg_from, int reg_to) {
  PrintF(" ClearRegister(from=%d, to=%d);\n", reg_from, reg_to);
  assembler_->ClearRegisters(reg_from, reg_to);
}


void RegExpMacroAssemblerTracer::ReadCurrentPositionFromRegister(int reg) {
  PrintF(" ReadCurrentPositionFromRegister(register=%d);\n", reg);
  assembler_->ReadCurrentPositionFromRegister(reg);
}


void RegExpMacroAssemblerTracer::WriteStackPointerToRegister(int reg) {
  PrintF(" WriteStackPointerToRegister(register=%d);\n", reg);
  assembler_->WriteStackPointerToRegister(reg);
}


void RegExpMacroAssemblerTracer::ReadStackPointerFromRegister(int reg) {
  PrintF(" ReadStackPointerFromRegister(register=%d);\n", reg);
  assembler_->ReadStackPointerFromRegister(reg);
}


void RegExpMacroAssemblerTracer::LoadCurrentCharacter(int cp_offset,
                                                      Label* on_end_of_input,
                                                      bool check_bounds,
                                                      int characters) {
  const char* check_msg = check_bounds ? "" : " (unchecked)";
  PrintF(" LoadCurrentCharacter(cp_offset=%d, label[%08x]%s (%d chars));\n",
         cp_offset,
         on_end_of_input,
         check_msg,
         characters);
  assembler_->LoadCurrentCharacter(cp_offset,
                                   on_end_of_input,
                                   check_bounds,
                                   characters);
}


void RegExpMacroAssemblerTracer::CheckCharacterLT(uc16 limit, Label* on_less) {
  PrintF(" CheckCharacterLT(c='u%04x', label[%08x]);\n", limit, on_less);
  assembler_->CheckCharacterLT(limit, on_less);
}


void RegExpMacroAssemblerTracer::CheckCharacterGT(uc16 limit,
                                                  Label* on_greater) {
  PrintF(" CheckCharacterGT(c='u%04x', label[%08x]);\n", limit, on_greater);
  assembler_->CheckCharacterGT(limit, on_greater);
}


void RegExpMacroAssemblerTracer::CheckCharacter(uint32_t c, Label* on_equal) {
  PrintF(" CheckCharacter(c='u%04x', label[%08x]);\n", c, on_equal);
  assembler_->CheckCharacter(c, on_equal);
}


void RegExpMacroAssemblerTracer::CheckAtStart(Label* on_at_start) {
  PrintF(" CheckAtStart(label[%08x]);\n", on_at_start);
  assembler_->CheckAtStart(on_at_start);
}


void RegExpMacroAssemblerTracer::CheckNotAtStart(Label* on_not_at_start) {
  PrintF(" CheckNotAtStart(label[%08x]);\n", on_not_at_start);
  assembler_->CheckNotAtStart(on_not_at_start);
}


void RegExpMacroAssemblerTracer::CheckNotCharacter(uint32_t c,
                                                   Label* on_not_equal) {
  PrintF(" CheckNotCharacter(c='u%04x', label[%08x]);\n", c, on_not_equal);
  assembler_->CheckNotCharacter(c, on_not_equal);
}


void RegExpMacroAssemblerTracer::CheckCharacterAfterAnd(
    uint32_t c,
    uint32_t mask,
    Label* on_equal) {
  PrintF(" CheckCharacterAfterAnd(c='u%04x', mask=0x%04x, label[%08x]);\n",
         c,
         mask,
         on_equal);
  assembler_->CheckCharacterAfterAnd(c, mask, on_equal);
}


void RegExpMacroAssemblerTracer::CheckNotCharacterAfterAnd(
    uint32_t c,
    uint32_t mask,
    Label* on_not_equal) {
  PrintF(" CheckNotCharacterAfterAnd(c='u%04x', mask=0x%04x, label[%08x]);\n",
         c,
         mask,
         on_not_equal);
  assembler_->CheckNotCharacterAfterAnd(c, mask, on_not_equal);
}


void RegExpMacroAssemblerTracer::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  PrintF(" CheckNotCharacterAfterMinusAnd(c='u%04x', minus=%04x, mask=0x%04x, "
             "label[%08x]);\n",
         c,
         minus,
         mask,
         on_not_equal);
  assembler_->CheckNotCharacterAfterMinusAnd(c, minus, mask, on_not_equal);
}


void RegExpMacroAssemblerTracer::CheckNotBackReference(int start_reg,
                                                       Label* on_no_match) {
  PrintF(" CheckNotBackReference(register=%d, label[%08x]);\n", start_reg,
         on_no_match);
  assembler_->CheckNotBackReference(start_reg, on_no_match);
}


void RegExpMacroAssemblerTracer::CheckNotBackReferenceIgnoreCase(
    int start_reg,
    Label* on_no_match) {
  PrintF(" CheckNotBackReferenceIgnoreCase(register=%d, label[%08x]);\n",
         start_reg, on_no_match);
  assembler_->CheckNotBackReferenceIgnoreCase(start_reg, on_no_match);
}


void RegExpMacroAssemblerTracer::CheckNotRegistersEqual(int reg1,
                                                        int reg2,
                                                        Label* on_not_equal) {
  PrintF(" CheckNotRegistersEqual(reg1=%d, reg2=%d, label[%08x]);\n",
         reg1,
         reg2,
         on_not_equal);
  assembler_->CheckNotRegistersEqual(reg1, reg2, on_not_equal);
}


void RegExpMacroAssemblerTracer::CheckCharacters(Vector<const uc16> str,
                                                 int cp_offset,
                                                 Label* on_failure,
                                                 bool check_end_of_string) {
  PrintF(" %s(str=\"",
         check_end_of_string ? "CheckCharacters" : "CheckCharactersUnchecked");
  for (int i = 0; i < str.length(); i++) {
    PrintF("u%04x", str[i]);
  }
  PrintF("\", cp_offset=%d, label[%08x])\n", cp_offset, on_failure);
  assembler_->CheckCharacters(str, cp_offset, on_failure, check_end_of_string);
}


bool RegExpMacroAssemblerTracer::CheckSpecialCharacterClass(
    uc16 type,
    int cp_offset,
    bool check_offset,
    Label* on_no_match) {
  bool supported = assembler_->CheckSpecialCharacterClass(type,
                                                          cp_offset,
                                                          check_offset,
                                                          on_no_match);
  PrintF(" CheckSpecialCharacterClass(type='%c', offset=%d, "
             "check_offset=%s, label[%08x]): %s;\n",
         type,
         cp_offset,
         check_offset ? "true" : "false",
         on_no_match,
         supported ? "true" : "false");
  return supported;
}


void RegExpMacroAssemblerTracer::IfRegisterLT(int register_index,
                                              int comparand, Label* if_lt) {
  PrintF(" IfRegisterLT(register=%d, number=%d, label[%08x]);\n",
         register_index, comparand, if_lt);
  assembler_->IfRegisterLT(register_index, comparand, if_lt);
}


void RegExpMacroAssemblerTracer::IfRegisterEqPos(int register_index,
                                                 Label* if_eq) {
  PrintF(" IfRegisterEqPos(register=%d, label[%08x]);\n",
         register_index, if_eq);
  assembler_->IfRegisterEqPos(register_index, if_eq);
}


void RegExpMacroAssemblerTracer::IfRegisterGE(int register_index,
                                              int comparand, Label* if_ge) {
  PrintF(" IfRegisterGE(register=%d, number=%d, label[%08x]);\n",
         register_index, comparand, if_ge);
  assembler_->IfRegisterGE(register_index, comparand, if_ge);
}


RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerTracer::Implementation() {
  return assembler_->Implementation();
}


Handle<Object> RegExpMacroAssemblerTracer::GetCode(Handle<String> source) {
  PrintF(" GetCode(%s);\n", *(source->ToCString()));
  return assembler_->GetCode(source);
}

}}  // namespace v8::internal

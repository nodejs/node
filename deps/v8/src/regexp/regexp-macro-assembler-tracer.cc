// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-macro-assembler-tracer.h"

#include "src/ast/ast.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RegExpMacroAssemblerTracer::RegExpMacroAssemblerTracer(
    Isolate* isolate, RegExpMacroAssembler* assembler)
    : RegExpMacroAssembler(isolate, assembler->zone()), assembler_(assembler) {
  IrregexpImplementation type = assembler->Implementation();
  DCHECK_LT(type, 9);
  const char* impl_names[] = {"IA32", "ARM", "ARM64", "MIPS",    "S390",
                              "PPC",  "X64", "X87",   "Bytecode"};
  PrintF("RegExpMacroAssembler%s();\n", impl_names[type]);
}

RegExpMacroAssemblerTracer::~RegExpMacroAssemblerTracer() = default;

void RegExpMacroAssemblerTracer::AbortedCodeGeneration() {
  PrintF(" AbortedCodeGeneration\n");
  assembler_->AbortedCodeGeneration();
}


// This is used for printing out debugging information.  It makes an integer
// that is closely related to the address of an object.
static int LabelToInt(Label* label) {
  return static_cast<int>(reinterpret_cast<intptr_t>(label));
}


void RegExpMacroAssemblerTracer::Bind(Label* label) {
  PrintF("label[%08x]: (Bind)\n", LabelToInt(label));
  assembler_->Bind(label);
}


void RegExpMacroAssemblerTracer::AdvanceCurrentPosition(int by) {
  PrintF(" AdvanceCurrentPosition(by=%d);\n", by);
  assembler_->AdvanceCurrentPosition(by);
}


void RegExpMacroAssemblerTracer::CheckGreedyLoop(Label* label) {
  PrintF(" CheckGreedyLoop(label[%08x]);\n\n", LabelToInt(label));
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
  PrintF(" GoTo(label[%08x]);\n\n", LabelToInt(label));
  assembler_->GoTo(label);
}


void RegExpMacroAssemblerTracer::PushBacktrack(Label* label) {
  PrintF(" PushBacktrack(label[%08x]);\n", LabelToInt(label));
  assembler_->PushBacktrack(label);
}


bool RegExpMacroAssemblerTracer::Succeed() {
  bool restart = assembler_->Succeed();
  PrintF(" Succeed();%s\n", restart ? " [restart for global match]" : "");
  return restart;
}


void RegExpMacroAssemblerTracer::Fail() {
  PrintF(" Fail();");
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


void RegExpMacroAssemblerTracer::SetCurrentPositionFromEnd(int by) {
  PrintF(" SetCurrentPositionFromEnd(by=%d);\n", by);
  assembler_->SetCurrentPositionFromEnd(by);
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
         LabelToInt(on_end_of_input),
         check_msg,
         characters);
  assembler_->LoadCurrentCharacter(cp_offset,
                                   on_end_of_input,
                                   check_bounds,
                                   characters);
}


class PrintablePrinter {
 public:
  explicit PrintablePrinter(uc16 character) : character_(character) { }

  const char* operator*() {
    if (character_ >= ' ' && character_ <= '~') {
      buffer_[0] = '(';
      buffer_[1] = static_cast<char>(character_);
      buffer_[2] = ')';
      buffer_[3] = '\0';
    } else {
      buffer_[0] = '\0';
    }
    return &buffer_[0];
  }

 private:
  uc16 character_;
  char buffer_[4];
};


void RegExpMacroAssemblerTracer::CheckCharacterLT(uc16 limit, Label* on_less) {
  PrintablePrinter printable(limit);
  PrintF(" CheckCharacterLT(c=0x%04x%s, label[%08x]);\n",
         limit,
         *printable,
         LabelToInt(on_less));
  assembler_->CheckCharacterLT(limit, on_less);
}


void RegExpMacroAssemblerTracer::CheckCharacterGT(uc16 limit,
                                                  Label* on_greater) {
  PrintablePrinter printable(limit);
  PrintF(" CheckCharacterGT(c=0x%04x%s, label[%08x]);\n",
         limit,
         *printable,
         LabelToInt(on_greater));
  assembler_->CheckCharacterGT(limit, on_greater);
}


void RegExpMacroAssemblerTracer::CheckCharacter(unsigned c, Label* on_equal) {
  PrintablePrinter printable(c);
  PrintF(" CheckCharacter(c=0x%04x%s, label[%08x]);\n",
         c,
         *printable,
         LabelToInt(on_equal));
  assembler_->CheckCharacter(c, on_equal);
}


void RegExpMacroAssemblerTracer::CheckAtStart(Label* on_at_start) {
  PrintF(" CheckAtStart(label[%08x]);\n", LabelToInt(on_at_start));
  assembler_->CheckAtStart(on_at_start);
}


void RegExpMacroAssemblerTracer::CheckNotAtStart(int cp_offset,
                                                 Label* on_not_at_start) {
  PrintF(" CheckNotAtStart(cp_offset=%d, label[%08x]);\n", cp_offset,
         LabelToInt(on_not_at_start));
  assembler_->CheckNotAtStart(cp_offset, on_not_at_start);
}


void RegExpMacroAssemblerTracer::CheckNotCharacter(unsigned c,
                                                   Label* on_not_equal) {
  PrintablePrinter printable(c);
  PrintF(" CheckNotCharacter(c=0x%04x%s, label[%08x]);\n",
         c,
         *printable,
         LabelToInt(on_not_equal));
  assembler_->CheckNotCharacter(c, on_not_equal);
}


void RegExpMacroAssemblerTracer::CheckCharacterAfterAnd(
    unsigned c,
    unsigned mask,
    Label* on_equal) {
  PrintablePrinter printable(c);
  PrintF(" CheckCharacterAfterAnd(c=0x%04x%s, mask=0x%04x, label[%08x]);\n",
         c,
         *printable,
         mask,
         LabelToInt(on_equal));
  assembler_->CheckCharacterAfterAnd(c, mask, on_equal);
}


void RegExpMacroAssemblerTracer::CheckNotCharacterAfterAnd(
    unsigned c,
    unsigned mask,
    Label* on_not_equal) {
  PrintablePrinter printable(c);
  PrintF(" CheckNotCharacterAfterAnd(c=0x%04x%s, mask=0x%04x, label[%08x]);\n",
         c,
         *printable,
         mask,
         LabelToInt(on_not_equal));
  assembler_->CheckNotCharacterAfterAnd(c, mask, on_not_equal);
}


void RegExpMacroAssemblerTracer::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  PrintF(" CheckNotCharacterAfterMinusAnd(c=0x%04x, minus=%04x, mask=0x%04x, "
             "label[%08x]);\n",
         c,
         minus,
         mask,
         LabelToInt(on_not_equal));
  assembler_->CheckNotCharacterAfterMinusAnd(c, minus, mask, on_not_equal);
}


void RegExpMacroAssemblerTracer::CheckCharacterInRange(
    uc16 from,
    uc16 to,
    Label* on_not_in_range) {
  PrintablePrinter printable_from(from);
  PrintablePrinter printable_to(to);
  PrintF(" CheckCharacterInRange(from=0x%04x%s, to=0x%04x%s, label[%08x]);\n",
         from,
         *printable_from,
         to,
         *printable_to,
         LabelToInt(on_not_in_range));
  assembler_->CheckCharacterInRange(from, to, on_not_in_range);
}


void RegExpMacroAssemblerTracer::CheckCharacterNotInRange(
    uc16 from,
    uc16 to,
    Label* on_in_range) {
  PrintablePrinter printable_from(from);
  PrintablePrinter printable_to(to);
  PrintF(
      " CheckCharacterNotInRange(from=0x%04x%s," " to=%04x%s, label[%08x]);\n",
      from,
      *printable_from,
      to,
      *printable_to,
      LabelToInt(on_in_range));
  assembler_->CheckCharacterNotInRange(from, to, on_in_range);
}


void RegExpMacroAssemblerTracer::CheckBitInTable(
    Handle<ByteArray> table, Label* on_bit_set) {
  PrintF(" CheckBitInTable(label[%08x] ", LabelToInt(on_bit_set));
  for (int i = 0; i < kTableSize; i++) {
    PrintF("%c", table->get(i) != 0 ? 'X' : '.');
    if (i % 32 == 31 && i != kTableMask) {
      PrintF("\n                                 ");
    }
  }
  PrintF(");\n");
  assembler_->CheckBitInTable(table, on_bit_set);
}


void RegExpMacroAssemblerTracer::CheckNotBackReference(int start_reg,
                                                       bool read_backward,
                                                       Label* on_no_match) {
  PrintF(" CheckNotBackReference(register=%d, %s, label[%08x]);\n", start_reg,
         read_backward ? "backward" : "forward", LabelToInt(on_no_match));
  assembler_->CheckNotBackReference(start_reg, read_backward, on_no_match);
}


void RegExpMacroAssemblerTracer::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, bool unicode, Label* on_no_match) {
  PrintF(" CheckNotBackReferenceIgnoreCase(register=%d, %s %s, label[%08x]);\n",
         start_reg, read_backward ? "backward" : "forward",
         unicode ? "unicode" : "non-unicode", LabelToInt(on_no_match));
  assembler_->CheckNotBackReferenceIgnoreCase(start_reg, read_backward, unicode,
                                              on_no_match);
}


void RegExpMacroAssemblerTracer::CheckPosition(int cp_offset,
                                               Label* on_outside_input) {
  PrintF(" CheckPosition(cp_offset=%d, label[%08x]);\n", cp_offset,
         LabelToInt(on_outside_input));
  assembler_->CheckPosition(cp_offset, on_outside_input);
}


bool RegExpMacroAssemblerTracer::CheckSpecialCharacterClass(
    uc16 type,
    Label* on_no_match) {
  bool supported = assembler_->CheckSpecialCharacterClass(type,
                                                          on_no_match);
  PrintF(" CheckSpecialCharacterClass(type='%c', label[%08x]): %s;\n",
         type,
         LabelToInt(on_no_match),
         supported ? "true" : "false");
  return supported;
}


void RegExpMacroAssemblerTracer::IfRegisterLT(int register_index,
                                              int comparand, Label* if_lt) {
  PrintF(" IfRegisterLT(register=%d, number=%d, label[%08x]);\n",
         register_index, comparand, LabelToInt(if_lt));
  assembler_->IfRegisterLT(register_index, comparand, if_lt);
}


void RegExpMacroAssemblerTracer::IfRegisterEqPos(int register_index,
                                                 Label* if_eq) {
  PrintF(" IfRegisterEqPos(register=%d, label[%08x]);\n",
         register_index, LabelToInt(if_eq));
  assembler_->IfRegisterEqPos(register_index, if_eq);
}


void RegExpMacroAssemblerTracer::IfRegisterGE(int register_index,
                                              int comparand, Label* if_ge) {
  PrintF(" IfRegisterGE(register=%d, number=%d, label[%08x]);\n",
         register_index, comparand, LabelToInt(if_ge));
  assembler_->IfRegisterGE(register_index, comparand, if_ge);
}


RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerTracer::Implementation() {
  return assembler_->Implementation();
}


Handle<HeapObject> RegExpMacroAssemblerTracer::GetCode(Handle<String> source) {
  PrintF(" GetCode(%s);\n", source->ToCString().get());
  return assembler_->GetCode(source);
}

}  // namespace internal
}  // namespace v8

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-macro-assembler-tracer.h"

#include "src/objects/fixed-array-inl.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

RegExpMacroAssemblerTracer::RegExpMacroAssemblerTracer(
    std::unique_ptr<RegExpMacroAssembler>&& assembler)
    : RegExpMacroAssembler(*assembler), assembler_(std::move(assembler)) {
  PrintF("RegExpMacroAssembler%s();\n",
         ImplementationToString(assembler_->Implementation()));
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

void RegExpMacroAssemblerTracer::CheckFixedLengthLoop(Label* label) {
  PrintF(" CheckFixedLengthLoop(label[%08x]);\n\n", LabelToInt(label));
  assembler_->CheckFixedLengthLoop(label);
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
  PrintF(" PushRegister(register=%d, %s);\n", register_index,
         check_stack_limit == StackCheckFlag::kCheckStackLimit
             ? "check stack limit"
             : "");
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

void RegExpMacroAssemblerTracer::LoadCurrentCharacterImpl(
    int cp_offset, Label* on_end_of_input, bool check_bounds, int characters,
    int eats_at_least) {
  const char* check_msg = check_bounds ? "" : " (unchecked)";
  PrintF(
      " LoadCurrentCharacter(cp_offset=%d, label[%08x]%s (%d chars) (eats at "
      "least %d));\n",
      cp_offset, LabelToInt(on_end_of_input), check_msg, characters,
      eats_at_least);
  assembler_->LoadCurrentCharacter(cp_offset, on_end_of_input, check_bounds,
                                   characters, eats_at_least);
}

namespace {

class PrintablePrinter {
 public:
  explicit PrintablePrinter(base::uc16 character) : character_(character) {}

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
  base::uc16 character_;
  char buffer_[4];
};

}  // namespace

void RegExpMacroAssemblerTracer::CheckCharacterLT(base::uc16 limit,
                                                  Label* on_less) {
  PrintablePrinter printable(limit);
  PrintF(" CheckCharacterLT(c=0x%04x%s, label[%08x]);\n",
         limit,
         *printable,
         LabelToInt(on_less));
  assembler_->CheckCharacterLT(limit, on_less);
}

void RegExpMacroAssemblerTracer::CheckCharacterGT(base::uc16 limit,
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

void RegExpMacroAssemblerTracer::CheckAtStart(int cp_offset,
                                              Label* on_at_start) {
  PrintF(" CheckAtStart(cp_offset=%d, label[%08x]);\n", cp_offset,
         LabelToInt(on_at_start));
  assembler_->CheckAtStart(cp_offset, on_at_start);
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
    base::uc16 c, base::uc16 minus, base::uc16 mask, Label* on_not_equal) {
  PrintF(" CheckNotCharacterAfterMinusAnd(c=0x%04x, minus=%04x, mask=0x%04x, "
             "label[%08x]);\n",
         c,
         minus,
         mask,
         LabelToInt(on_not_equal));
  assembler_->CheckNotCharacterAfterMinusAnd(c, minus, mask, on_not_equal);
}

void RegExpMacroAssemblerTracer::CheckCharacterInRange(base::uc16 from,
                                                       base::uc16 to,
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

void RegExpMacroAssemblerTracer::CheckCharacterNotInRange(base::uc16 from,
                                                          base::uc16 to,
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

namespace {

void PrintRangeArray(const ZoneList<CharacterRange>* ranges) {
  for (int i = 0; i < ranges->length(); i++) {
    base::uc16 from = ranges->at(i).from();
    base::uc16 to = ranges->at(i).to();
    PrintablePrinter printable_from(from);
    PrintablePrinter printable_to(to);
    PrintF("        [from=0x%04x%s, to=%04x%s],\n", from, *printable_from, to,
           *printable_to);
  }
}

}  // namespace

bool RegExpMacroAssemblerTracer::CheckCharacterInRangeArray(
    const ZoneList<CharacterRange>* ranges, Label* on_in_range) {
  PrintF(
      " CheckCharacterInRangeArray(\n"
      "        label[%08x]);\n",
      LabelToInt(on_in_range));
  PrintRangeArray(ranges);
  return assembler_->CheckCharacterInRangeArray(ranges, on_in_range);
}

bool RegExpMacroAssemblerTracer::CheckCharacterNotInRangeArray(
    const ZoneList<CharacterRange>* ranges, Label* on_not_in_range) {
  bool emitted =
      assembler_->CheckCharacterNotInRangeArray(ranges, on_not_in_range);
  if (emitted) {
    PrintF(
        " CheckCharacterNotInRangeArray(\n"
        "        label[%08x]);\n",
        LabelToInt(on_not_in_range));
    PrintRangeArray(ranges);
  }
  return emitted;
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

namespace {

void PrintTables(Handle<ByteArray> table, Handle<ByteArray> nibble_table) {
  for (int i = 0; i < RegExpMacroAssembler::kTableSize; i++) {
    PrintF("%c", table->get(i) != 0 ? 'X' : '.');
    if (i % 32 == 31 && i != RegExpMacroAssembler::kTableMask) {
      PrintF("\n  ");
    }
  }
  static_assert(RegExpMacroAssembler::kTableSize == 128);
  static constexpr int kRows = 16;
  static_assert(kRows * kBitsPerByte == RegExpMacroAssembler::kTableSize);
  if (!nibble_table.is_null()) {
    PrintF("\n");
    PrintF("  +----------------\n");
    PrintF("  |");
    for (int j = 0; j < kBitsPerByte; j++) {
      PrintF(" %x", j);
    }
    PrintF("\n--+----------------");
    for (int i = 0; i < kRows; i++) {
      int r = nibble_table->get(i);
      PrintF("\n%x |", i);
      for (int j = 0; j < kBitsPerByte; j++) {
        PrintF(" %c", (r & (1 << j)) == 0 ? '.' : 'X');
      }
    }
  }
}

}  // namespace

void RegExpMacroAssemblerTracer::SkipUntilBitInTable(
    int cp_offset, Handle<ByteArray> table, Handle<ByteArray> nibble_table,
    int advance_by, Label* on_match, Label* on_no_match) {
  PrintF(
      "SkipUntilBitInTable(cp_offset=%d, advance_by=%d, on_match=label[%08x], "
      "on_no_match=label[%08x]\n  ",
      cp_offset, advance_by, LabelToInt(on_match), LabelToInt(on_no_match));
  PrintTables(table, nibble_table);
  PrintF(");\n");
  assembler_->SkipUntilBitInTable(cp_offset, table, nibble_table, advance_by,
                                  on_match, on_no_match);
}

void RegExpMacroAssemblerTracer::SkipUntilCharAnd(
    int cp_offset, int advance_by, unsigned character, unsigned mask,
    int eats_at_least, Label* on_match, Label* on_no_match) {
  PrintablePrinter printable(character);
  PrintF(
      "SkipUntilCharAnd(cp_offset=%d, advance_by=%d, character=0x%04x%s, "
      "mask=0x%04x, eats_at_least=%d, on_match=label[%08x], "
      "on_no_match=label[%08x]\n",
      cp_offset, advance_by, character, *printable, mask, eats_at_least,
      LabelToInt(on_match), LabelToInt(on_no_match));
  assembler_->SkipUntilCharAnd(cp_offset, advance_by, character, mask,
                               eats_at_least, on_match, on_no_match);
}

void RegExpMacroAssemblerTracer::SkipUntilChar(int cp_offset, int advance_by,
                                               unsigned character,
                                               Label* on_match,
                                               Label* on_no_match) {
  PrintablePrinter printable(character);
  PrintF(
      "SkipUntilChar(cp_offset=%d, advance_by=%d, character=0x%04x%s, "
      "on_match=label[%08x], "
      "on_no_match=label[%08x]\n",
      cp_offset, advance_by, character, *printable, LabelToInt(on_match),
      LabelToInt(on_no_match));
  assembler_->SkipUntilChar(cp_offset, advance_by, character, on_match,
                            on_no_match);
}

void RegExpMacroAssemblerTracer::SkipUntilCharPosChecked(
    int cp_offset, int advance_by, unsigned character, int eats_at_least,
    Label* on_match, Label* on_no_match) {
  PrintablePrinter printable(character);
  PrintF(
      "SkipUntilCharPosChecked(cp_offset=%d, advance_by=%d, "
      "character=0x%04x%s, eats_at_least=%d, on_match=label[%08x], "
      "on_no_match=label[%08x]\n",
      cp_offset, advance_by, character, *printable, eats_at_least,
      LabelToInt(on_match), LabelToInt(on_no_match));
  assembler_->SkipUntilCharPosChecked(cp_offset, advance_by, character,
                                      eats_at_least, on_match, on_no_match);
}

void RegExpMacroAssemblerTracer::SkipUntilCharOrChar(
    int cp_offset, int advance_by, unsigned char1, unsigned char2,
    Label* on_match, Label* on_no_match) {
  PrintablePrinter printable1(char1);
  PrintablePrinter printable2(char2);
  PrintF(
      "SkipUntilCharOrChar(cp_offset=%d, advance_by=%d, char1=0x%04x%s, "
      "char2=0x%04x%s, on_match=label[%08x], "
      "on_no_match=label[%08x]\n",
      cp_offset, advance_by, char1, *printable1, char2, *printable2,
      LabelToInt(on_match), LabelToInt(on_no_match));
  assembler_->SkipUntilCharOrChar(cp_offset, advance_by, char1, char2, on_match,
                                  on_no_match);
}

void RegExpMacroAssemblerTracer::SkipUntilGtOrNotBitInTable(
    int cp_offset, int advance_by, unsigned character, Handle<ByteArray> table,
    Label* on_match, Label* on_no_match) {
  PrintablePrinter printable(character);
  PrintF(
      "SkipUntilGtOrNotBitInTable(cp_offset=%d, advance_by=%d, "
      "character=0x%04x%s, on_match=label[%08x], "
      "on_no_match=label[%08x]\n  ",
      cp_offset, advance_by, character, *printable, LabelToInt(on_match),
      LabelToInt(on_no_match));
  for (int i = 0; i < kTableSize; i++) {
    PrintF("%c", table->get(i) != 0 ? 'X' : '.');
    if (i % 32 == 31 && i != kTableMask) {
      PrintF("\n  ");
    }
  }
  PrintF(");\n");
  assembler_->SkipUntilGtOrNotBitInTable(cp_offset, advance_by, character,
                                         table, on_match, on_no_match);
}

void RegExpMacroAssemblerTracer::SkipUntilOneOfMasked(
    int cp_offset, int advance_by, unsigned both_chars, unsigned both_mask,
    int max_offset, unsigned chars1, unsigned mask1, unsigned chars2,
    unsigned mask2, Label* on_match1, Label* on_match2, Label* on_failure) {
  PrintablePrinter printable_both(both_chars);
  PrintablePrinter printable_chars1(chars1);
  PrintablePrinter printable_chars2(chars2);
  PrintF(
      "SkipUntilOneOfMasked(cp_offset=%d, advance_by=%d, both_chars=0x%04x%s, "
      "both_mask=0x%04x, max_offset=%d, chars1=0x%04x%s, "
      "mask1=0x%04x, chars2=0x%04x%s, mask2=0x%04x, on_match1=label[%08x], "
      "on_no_match2=label[%08x], on_failure=label[%08x]\n",
      cp_offset, advance_by, both_chars, *printable_both, both_mask, max_offset,
      chars1, *printable_chars1, mask1, chars2, *printable_chars2, mask2,
      LabelToInt(on_match1), LabelToInt(on_match2), LabelToInt(on_failure));
  assembler_->SkipUntilOneOfMasked(cp_offset, advance_by, both_chars, both_mask,
                                   max_offset, chars1, mask1, chars2, mask2,
                                   on_match1, on_match2, on_failure);
}

void RegExpMacroAssemblerTracer::SkipUntilOneOfMasked3(
    const SkipUntilOneOfMasked3Args& args) {
  PrintF(
      "SkipUntilOneOfMasked3(bc0_cp_offset=%d, bc0_advance_by=%d, "
      "bc1_cp_offset=%d, bc1_on_failure=label[%08x], "
      "bc2_cp_offset=%d, "
      "bc3_characters=0x%04x%s, bc3_mask=0x%04x, "
      "bc4_by=%d, "
      "bc5_cp_offset=%d, "
      "bc6_characters=0x%04x%s, bc6_mask=0x%04x, bc6_on_equal=label[%08x], "
      "bc7_characters=0x%04x%s, bc7_mask=0x%04x, bc7_on_equal=label[%08x], "
      "bc8_characters=0x%04x%s, bc8_mask=0x%04x, "
      "fallthrough_jump_target=label[%08x]\n  ",
      args.bc0_cp_offset, args.bc0_advance_by, args.bc1_cp_offset,
      LabelToInt(args.bc1_on_failure), args.bc2_cp_offset, args.bc3_characters,
      *PrintablePrinter(args.bc3_characters), args.bc3_mask, args.bc4_by,
      args.bc5_cp_offset, args.bc6_characters,
      *PrintablePrinter(args.bc6_characters), args.bc6_mask,
      LabelToInt(args.bc6_on_equal), args.bc7_characters,
      *PrintablePrinter(args.bc7_characters), args.bc7_mask,
      LabelToInt(args.bc7_on_equal), args.bc8_characters,
      *PrintablePrinter(args.bc8_characters), args.bc8_mask,
      LabelToInt(args.fallthrough_jump_target));
  PrintTables(args.bc0_table, args.bc0_nibble_table);
  PrintF(");\n");
  assembler_->SkipUntilOneOfMasked3(args);
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

void RegExpMacroAssemblerTracer::CheckSpecialClassRanges(
    StandardCharacterSet type, Label* on_no_match) {
  PrintF(" CheckSpecialClassRanges(type='%c', label[%08x])\n",
         static_cast<char>(type), LabelToInt(on_no_match));
  assembler_->CheckSpecialClassRanges(type, on_no_match);
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

void RegExpMacroAssemblerTracer::set_global_mode(GlobalMode mode) {
  RegExpMacroAssembler::set_global_mode(mode);
  assembler_->set_global_mode(mode);
}

void RegExpMacroAssemblerTracer::set_slow_safe(bool ssc) {
  RegExpMacroAssembler::set_slow_safe(ssc);
  assembler_->set_slow_safe(ssc);
}

void RegExpMacroAssemblerTracer::set_backtrack_limit(uint32_t backtrack_limit) {
  RegExpMacroAssembler::set_backtrack_limit(backtrack_limit);
  assembler_->set_backtrack_limit(backtrack_limit);
}

void RegExpMacroAssemblerTracer::set_can_fallback(bool val) {
  RegExpMacroAssembler::set_can_fallback(val);
  assembler_->set_can_fallback(val);
}

RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerTracer::Implementation() {
  return assembler_->Implementation();
}

DirectHandle<HeapObject> RegExpMacroAssemblerTracer::GetCode(
    DirectHandle<String> source, RegExpFlags flags) {
  DirectHandle<String> flags_str =
      JSRegExp::StringFromFlags(isolate(), JSRegExp::AsJSRegExpFlags(flags));
  PrintF(" GetCode('%s', '%s');\n", source->ToCString().get(),
         flags_str->ToCString().get());
  return assembler_->GetCode(source, flags);
}

}  // namespace internal
}  // namespace v8

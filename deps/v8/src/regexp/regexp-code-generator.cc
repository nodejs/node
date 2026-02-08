// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-code-generator.h"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#include "src/codegen/label.h"
#include "src/codegen/macro-assembler.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/objects/fixed-array-inl.h"
#include "src/regexp/regexp-bytecode-iterator-inl.h"
#include "src/regexp/regexp-bytecodes-inl.h"

namespace v8 {
namespace internal {

#define __ masm_->

RegExpCodeGenerator::RegExpCodeGenerator(
    Isolate* isolate, RegExpMacroAssembler* masm,
    DirectHandle<TrustedByteArray> bytecode)
    : isolate_(isolate),
      zone_(isolate_->allocator(), ZONE_NAME),
      masm_(masm),
      bytecode_(bytecode),
      iter_(bytecode_),
      labels_(zone_.AllocateArray<Label>(bytecode_->length())),
      jump_targets_(bytecode_->length(), &zone_),
      indirect_jump_targets_(bytecode_->length(), &zone_),
      has_unsupported_bytecode_(false) {}

RegExpCodeGenerator::Result RegExpCodeGenerator::Assemble(
    DirectHandle<String> source, RegExpFlags flags) {
  USE(isolate_);
  USE(masm_);
  PreVisitBytecodes();
  iter_.reset();
  VisitBytecodes();
  if (has_unsupported_bytecode_) return Result::UnsupportedBytecode();
  DirectHandle<Code> code = CheckedCast<Code>(masm_->GetCode(source, flags));
  return Result{code};
}

template <typename Operands, typename Operands::Operand Operand>
auto RegExpCodeGenerator::GetArgumentValue() {
  constexpr RegExpBytecodeOperandType op_type = Operands::Type(Operand);
  auto value = Operands::template Get<Operand>(bytecode_,
                                               iter_.current_offset(), &zone_);

  // If the operand is a JumpTarget, we return the Label created during the
  // first pass instead of an offset to the bytecode.
  if constexpr (op_type == ReBcOpType::kJumpTarget) {
    return GetLabel(value);
  } else {
    return value;
  }
}

template <typename Operands>
auto RegExpCodeGenerator::GetArgumentValuesAsTuple() {
  constexpr auto filtered_ops = Operands::GetOperandsTuple();
  return std::apply(
      [&](auto... ops) {
        return std::make_tuple(GetArgumentValue<Operands, ops.value>()...);
      },
      filtered_ops);
}

#ifdef V8_CODE_COMMENTS

#define VISIT_COMMENT(bc)                                                  \
  std::stringstream comment_stream;                                        \
  comment_stream << std::hex << iter_.current_offset() << std::dec << ": " \
                 << bc;                                                    \
  ASM_CODE_COMMENT_STRING(NativeMasm(), comment_stream.str())
#else
#define VISIT_COMMENT(bc)
#endif

#define INIT(Name, ...)                                                      \
  VISIT_COMMENT(#Name);                                                      \
  using Operands [[maybe_unused]] =                                          \
      RegExpBytecodeOperands<RegExpBytecode::k##Name>;                       \
  __VA_OPT__(auto argument_tuple = GetArgumentValuesAsTuple<Operands>();     \
             static_assert(std::tuple_size_v<decltype(argument_tuple)> ==    \
                               Operands::kCount,                             \
                           "Number of arguments to VISIT doesn't match the " \
                           "bytecodes operands count");                      \
             auto [__VA_ARGS__] = argument_tuple;)                           \
  static_assert((IS_VA_EMPTY(__VA_ARGS__)) == (Operands::kCount == 0),       \
                "Number of arguments to VISIT doesn't match the bytecodes "  \
                "operands count")

// These weird looking macros are required for clang-format and cpplint to not
// interfere/complain about our logic of opening/closing blocks in our macros.
#define OPEN_BLOCK {
#define CLOSE_BLOCK }
// This is just a hack to for the closing curly braces for the first VISIT.
// Only VISIT macros are allowed between VISIT_METHODS_START() and
// VISIT_METHODS_END(). If helper functions are needed, close the current VISIT
// block with VISIT_METHODS_END() and start a new one afterwards.
#define VISIT_METHODS_START() namespace OPEN_BLOCK
#define VISIT_METHODS_END() CLOSE_BLOCK

#define VISIT(Name, ...)                                     \
  CLOSE_BLOCK                                                \
  template <>                                                \
  void RegExpCodeGenerator::Visit<RegExpBytecode::k##Name>() \
      OPEN_BLOCK INIT(Name __VA_OPT__(, ) __VA_ARGS__);

// Basic Bytecodes

VISIT_METHODS_START()

VISIT(PushCurrentPosition) {
  __ PushCurrentPosition();
}

VISIT(PushBacktrack, label) { __ PushBacktrack(label); }

VISIT(WriteCurrentPositionToRegister, register_index, cp_offset) {
  __ WriteCurrentPositionToRegister(register_index, cp_offset);
}

VISIT(ReadCurrentPositionFromRegister, register_index) {
  __ ReadCurrentPositionFromRegister(register_index);
}

VISIT(WriteStackPointerToRegister, register_index) {
  __ WriteStackPointerToRegister(register_index);
}

VISIT(ReadStackPointerFromRegister, register_index) {
  __ ReadStackPointerFromRegister(register_index);
}

VISIT(SetRegister, register_index, value) {
  __ SetRegister(register_index, value);
}

VISIT(ClearRegisters, from_register, to_register) {
  __ ClearRegisters(from_register, to_register);
}

VISIT(AdvanceRegister, register_index, by) {
  __ AdvanceRegister(register_index, by);
}

VISIT(PopCurrentPosition) {
  __ PopCurrentPosition();
}

VISIT(PushRegister, register_index, stack_check) {
  __ PushRegister(register_index, stack_check);
}

VISIT(PopRegister, register_index) { __ PopRegister(register_index); }

VISIT(Fail) {
  __ Fail();
}

VISIT(Succeed) {
  __ Succeed();
}

VISIT(AdvanceCurrentPosition, by) { __ AdvanceCurrentPosition(by); }

VISIT(GoTo, label) { __ GoTo(label); }

VISIT(LoadCurrentCharacter, cp_offset, on_failure) {
  __ LoadCurrentCharacter(cp_offset, on_failure);
}

VISIT(CheckPosition, cp_offset, on_failure) {
  __ CheckPosition(cp_offset, on_failure);
}

VISIT(CheckSpecialClassRanges, character_set, on_no_match) {
  __ CheckSpecialClassRanges(character_set, on_no_match);
}

VISIT(CheckCharacter, character, on_equal) {
  __ CheckCharacter(character, on_equal);
}

VISIT(CheckNotCharacter, character, on_not_equal) {
  __ CheckNotCharacter(character, on_not_equal);
}

VISIT(CheckCharacterAfterAnd, character, mask, on_equal) {
  __ CheckCharacterAfterAnd(character, mask, on_equal);
}

VISIT(CheckNotCharacterAfterAnd, character, mask, on_not_equal) {
  __ CheckNotCharacterAfterAnd(character, mask, on_not_equal);
}

VISIT(CheckNotCharacterAfterMinusAnd, character, minus, mask, on_not_equal) {
  __ CheckNotCharacterAfterMinusAnd(character, minus, mask, on_not_equal);
}

VISIT(CheckCharacterInRange, from, to, on_in_range) {
  __ CheckCharacterInRange(from, to, on_in_range);
}

VISIT(CheckCharacterNotInRange, from, to, on_not_in_range) {
  __ CheckCharacterNotInRange(from, to, on_not_in_range);
}

VISIT(CheckCharacterLT, limit, on_less) { __ CheckCharacterLT(limit, on_less); }

VISIT(CheckCharacterGT, limit, on_greater) {
  __ CheckCharacterGT(limit, on_greater);
}

VISIT(IfRegisterLT, register_index, comparand, on_less_than) {
  __ IfRegisterLT(register_index, comparand, on_less_than);
}

VISIT(IfRegisterGE, register_index, comparand, on_greater_or_equal) {
  __ IfRegisterGE(register_index, comparand, on_greater_or_equal);
}

VISIT(IfRegisterEqPos, register_index, on_eq) {
  __ IfRegisterEqPos(register_index, on_eq);
}

VISIT(CheckAtStart, cp_offset, on_at_start) {
  __ CheckAtStart(cp_offset, on_at_start);
}

VISIT(CheckNotAtStart, cp_offset, on_not_at_start) {
  __ CheckNotAtStart(cp_offset, on_not_at_start);
}

VISIT(CheckFixedLengthLoop, on_tos_equals_current_position) {
  __ CheckFixedLengthLoop(on_tos_equals_current_position);
}

VISIT(SetCurrentPositionFromEnd, by) { __ SetCurrentPositionFromEnd(by); }

VISIT_METHODS_END()

// Special Bytecodes

VISIT_METHODS_START()

VISIT(Backtrack, return_code) {
  USE(return_code);  // Intentionally ignored. Only used in the interpreter.
  __ Backtrack();
}

VISIT_METHODS_END()

namespace {

// Convert the 16-byte (128 bit) |table_data| to a 128-byte ByteArray. Every bit
// in |table_data| is translated to its own byte (set to 0 or 1) in the
// ByteArray. Optionally also populates a nibble_table used for SIMD variants
// (see BoyerMooreLookahead::GetSkipTable).
Handle<ByteArray> CreateBitTableByteArray(
    Isolate* isolate, const ZoneVector<uint8_t> table_data,
    Handle<ByteArray> nibble_table = Handle<ByteArray>::null()) {
  Handle<ByteArray> table =
      isolate->factory()->NewByteArray(RegExpMacroAssembler::kTableSize);
  const bool fill_nibble_table = !nibble_table.is_null();
  if (fill_nibble_table) {
    DCHECK_EQ(nibble_table->length(),
              RegExpMacroAssembler::kTableSize / kBitsPerByte);
    std::memset(nibble_table->begin(), 0, nibble_table->length());
  }
  for (int i = 0; i < RegExpMacroAssembler::kTableSize / kBitsPerByte; i++) {
    uint8_t byte = table_data[i];
    for (int j = 0; j < kBitsPerByte; j++) {
      bool bit_set = (byte & (1 << j)) != 0;
      // bit_index is the ASCII char code that we want to check against.
      const int bit_index = i * kBitsPerByte + j;
      table->set(bit_index, bit_set);
      if (fill_nibble_table && bit_set) {
        int lo_nibble = bit_index & 0x0f;
        int hi_nibble = (bit_index >> 4) & 0x07;
        int row = nibble_table->get(lo_nibble);
        row |= 1 << hi_nibble;
        nibble_table->set(lo_nibble, row);
      }
    }
  }
  return table;
}

}  // namespace

VISIT_METHODS_START()

VISIT(CheckBitInTable, on_bit_set, table_data) {
  Handle<ByteArray> table = CreateBitTableByteArray(isolate_, table_data);
  __ CheckBitInTable(table, on_bit_set);
}

VISIT(LoadCurrentCharacterUnchecked, cp_offset) {
  static constexpr int kChars = 1;
  __ LoadCurrentCharacterImpl(cp_offset, nullptr, false, kChars, kChars);
}

VISIT(Load2CurrentChars, cp_offset, on_failure) {
  static constexpr int kChars = 2;
  __ LoadCurrentCharacterImpl(cp_offset, on_failure, true, kChars, kChars);
}

VISIT(Load2CurrentCharsUnchecked, cp_offset) {
  static constexpr int kChars = 2;
  __ LoadCurrentCharacterImpl(cp_offset, nullptr, false, kChars, kChars);
}

VISIT(Load4CurrentChars, cp_offset, on_failure) {
  static constexpr int kChars = 4;
  __ LoadCurrentCharacterImpl(cp_offset, on_failure, true, kChars, kChars);
}

VISIT(Load4CurrentCharsUnchecked, cp_offset) {
  static constexpr int kChars = 4;
  __ LoadCurrentCharacterImpl(cp_offset, nullptr, false, kChars, kChars);
}

VISIT(Check4Chars, characters, on_equal) {
  __ CheckCharacter(characters, on_equal);
}

VISIT(CheckNot4Chars, characters, on_not_equal) {
  __ CheckNotCharacter(characters, on_not_equal);
}

VISIT(AndCheck4Chars, characters, mask, on_equal) {
  __ CheckCharacterAfterAnd(characters, mask, on_equal);
}

VISIT(AndCheckNot4Chars, characters, mask, on_not_equal) {
  __ CheckNotCharacterAfterAnd(characters, mask, on_not_equal);
}

VISIT(AdvanceCpAndGoto, by, on_goto) {
  __ AdvanceCurrentPosition(by);
  __ GoTo(on_goto);
}

VISIT(CheckNotBackRef, start_reg, on_not_equal) {
  __ CheckNotBackReference(start_reg, false, on_not_equal);
}

VISIT(CheckNotBackRefNoCase, start_reg, on_not_equal) {
  __ CheckNotBackReferenceIgnoreCase(start_reg, false, false, on_not_equal);
}

VISIT(CheckNotBackRefNoCaseUnicode, start_reg, on_not_equal) {
  __ CheckNotBackReferenceIgnoreCase(start_reg, false, true, on_not_equal);
}

VISIT(CheckNotBackRefBackward, start_reg, on_not_equal) {
  __ CheckNotBackReference(start_reg, true, on_not_equal);
}

VISIT(CheckNotBackRefNoCaseBackward, start_reg, on_not_equal) {
  __ CheckNotBackReferenceIgnoreCase(start_reg, true, false, on_not_equal);
}

VISIT(CheckNotBackRefNoCaseUnicodeBackward, start_reg, on_not_equal) {
  __ CheckNotBackReferenceIgnoreCase(start_reg, true, true, on_not_equal);
}

// Bytecodes generated by peephole optimization.

VISIT(SkipUntilBitInTable, cp_offset, advance_by, table_data, on_match,
      on_no_match) {
  // Nibble table is optionally constructed if we use SIMD.
  Handle<ByteArray> nibble_table;
  if (masm_->SkipUntilBitInTableUseSimd(advance_by)) {
    static_assert(RegExpMacroAssembler::kTableSize == 128);
    nibble_table = isolate_->factory()->NewByteArray(
        RegExpMacroAssembler::kTableSize / kBitsPerByte, AllocationType::kOld);
  }
  Handle<ByteArray> table =
      CreateBitTableByteArray(isolate_, table_data, nibble_table);
  __ SkipUntilBitInTable(cp_offset, table, nibble_table, advance_by, on_match,
                         on_no_match);
}

VISIT(SkipUntilCharAnd, cp_offset, advance_by, character, mask, eats_at_least,
      on_match, on_no_match) {
  __ SkipUntilCharAnd(cp_offset, advance_by, character, mask, eats_at_least,
                      on_match, on_no_match);
}

VISIT(SkipUntilChar, cp_offset, advance_by, character, on_match, on_no_match) {
  __ SkipUntilChar(cp_offset, advance_by, character, on_match, on_no_match);
}

VISIT(SkipUntilCharPosChecked, cp_offset, advance_by, character, eats_at_least,
      on_match, on_no_match) {
  __ SkipUntilCharPosChecked(cp_offset, advance_by, character, eats_at_least,
                             on_match, on_no_match);
}

VISIT(SkipUntilCharOrChar, cp_offset, advance_by, char1, char2, on_match,
      on_no_match) {
  __ SkipUntilCharOrChar(cp_offset, advance_by, char1, char2, on_match,
                         on_no_match);
}

VISIT(SkipUntilGtOrNotBitInTable, cp_offset, advance_by, character, table_data,
      on_match, on_no_match) {
  Handle<ByteArray> table = CreateBitTableByteArray(isolate_, table_data);
  __ SkipUntilGtOrNotBitInTable(cp_offset, advance_by, character, table,
                                on_match, on_no_match);
}

VISIT(SkipUntilOneOfMasked, cp_offset, advance_by, both_chars, both_mask,
      max_offset, chars1, mask1, chars2, mask2, on_match1, on_match2,
      on_failure) {
  __ SkipUntilOneOfMasked(cp_offset, advance_by, both_chars, both_mask,
                          max_offset, chars1, mask1, chars2, mask2, on_match1,
                          on_match2, on_failure);
}

VISIT(SkipUntilOneOfMasked3, bc0_cp_offset, bc0_advance_by, bc0_table,
      bc1_cp_offset, bc1_on_failure, bc2_cp_offset, bc3_characters, bc3_mask,
      bc4_by, bc5_cp_offset, bc6_characters, bc6_mask, bc6_on_equal,
      bc7_characters, bc7_mask, bc7_on_equal, bc8_characters, bc8_mask,
      fallthrough_jump_target) {
  RegExpMacroAssembler::SkipUntilOneOfMasked3Args args = {
      .bc0_cp_offset = bc0_cp_offset,
      .bc0_advance_by = bc0_advance_by,
      .bc0_table = {},
      .bc0_nibble_table = {},
      .bc1_cp_offset = bc1_cp_offset,
      .bc1_on_failure = bc1_on_failure,
      .bc2_cp_offset = bc2_cp_offset,
      .bc3_characters = bc3_characters,
      .bc3_mask = bc3_mask,
      .bc4_by = bc4_by,
      .bc5_cp_offset = bc5_cp_offset,
      .bc6_characters = bc6_characters,
      .bc6_mask = bc6_mask,
      .bc6_on_equal = bc6_on_equal,
      .bc7_characters = bc7_characters,
      .bc7_mask = bc7_mask,
      .bc7_on_equal = bc7_on_equal,
      .bc8_characters = bc8_characters,
      .bc8_mask = bc8_mask,
      .fallthrough_jump_target = fallthrough_jump_target,
  };

  // The nibble table is optionally constructed if we use SIMD.
  Handle<ByteArray> nibble_table;
  if (masm_->SkipUntilOneOfMasked3UseSimd(args)) {
    static_assert(RegExpMacroAssembler::kTableSize == 128);
    nibble_table = isolate_->factory()->NewByteArray(
        RegExpMacroAssembler::kTableSize / kBitsPerByte, AllocationType::kOld);
  }
  Handle<ByteArray> table =
      CreateBitTableByteArray(isolate_, bc0_table, nibble_table);
  args.bc0_table = table;
  args.bc0_nibble_table = nibble_table;

  __ SkipUntilOneOfMasked3(args);
}

VISIT_METHODS_END()

#undef VISIT
#undef VISIT_METHODS_END
#undef VISIT_METHODS_START
#undef CLOSE_BLOCK
#undef OPEN_BLOCK
#undef INIT
#undef VISIT_COMMENT

template <RegExpBytecode bc>
void RegExpCodeGenerator::Visit() {
  // TODO(437003349): Remove fallback. All bytecodes need to be implemented
  // from now on.
  if (v8_flags.trace_regexp_assembler) {
    std::cout << "RegExp Code Generator: Unsupported Bytecode "
              << RegExpBytecodes::Name(bc) << std::endl;
  }
  has_unsupported_bytecode_ = true;
  UNREACHABLE();
}

void RegExpCodeGenerator::PreVisitBytecodes() {
  DisallowGarbageCollection no_gc;
  iter_.ForEachBytecode([&]<RegExpBytecode bc>() {
    using Operands = RegExpBytecodeOperands<bc>;
    auto ensure_label = [&]<auto operand>() {
      const uint8_t* pc = iter_.current_address();
      uint32_t offset = Operands::template Get<operand>(pc, no_gc);
      if (!jump_targets_.Contains(offset)) {
        jump_targets_.Add(offset);
        if constexpr (bc == RegExpBytecode::kPushBacktrack) {
          DCHECK(!indirect_jump_targets_.Contains(offset));
          indirect_jump_targets_.Add(offset);
        }
        Label* label = &labels_[offset];
        new (label) Label();
      }
    };
    Operands::template ForEachOperandOfType<
        RegExpBytecodeOperandType::kJumpTarget>(ensure_label);
  });
}

void RegExpCodeGenerator::VisitBytecodes() {
  for (; !iter_.done() && !has_unsupported_bytecode_; iter_.advance()) {
    if (jump_targets_.Contains(iter_.current_offset())) {
      if (indirect_jump_targets_.Contains(iter_.current_offset())) {
        __ BindJumpTarget(&labels_[iter_.current_offset()]);
      } else {
        __ Bind(&labels_[iter_.current_offset()]);
      }
    }
    RegExpBytecodes::DispatchOnBytecode(
        iter_.current_bytecode(), [this]<RegExpBytecode bc>() { Visit<bc>(); });
  }
}

Label* RegExpCodeGenerator::GetLabel(uint32_t offset) const {
  DCHECK(jump_targets_.Contains(offset));
  return &labels_[offset];
}

MacroAssembler* RegExpCodeGenerator::NativeMasm() {
  MacroAssembler* masm = masm_->masm();
  DCHECK_NOT_NULL(masm);
  return masm;
}

}  // namespace internal
}  // namespace v8

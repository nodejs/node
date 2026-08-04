// Copyright 2008-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-bytecode-generator.h"

#include <limits>
#include <tuple>
#include <type_traits>

#include "src/ast/ast.h"
#include "src/objects/fixed-array-inl.h"
#include "src/regexp/regexp-bytecode-generator-inl.h"
#include "src/regexp/regexp-bytecode-peephole.h"
#include "src/regexp/regexp-bytecodes-inl.h"
#include "src/regexp/regexp-macro-assembler.h"

namespace v8 {
namespace internal {

// Used to decide whether we use the `Char` or `4Chars` variant of a bytecode.
static constexpr int kMaxSingleCharValue =
    RegExpOperandTypeTraits<RegExpBytecodeOperandType::kChar>::kMaxValue;

// TODO(jgruber): Move all Writer methods before Generator methods.

RegExpBytecodeWriter::RegExpBytecodeWriter(Zone* zone)
    : buffer_(zone),
      pc_(0),
      jump_edges_(zone)
#ifdef DEBUG
      ,
      end_of_bc_(0),
      pc_within_bc_(0)
#endif
{
}

void RegExpBytecodeWriter::ExpandBuffer(size_t new_size) {
  // TODO(jgruber): It's not necessary to default-initialize new elements.
  buffer_.resize(new_size);
}

void RegExpBytecodeWriter::Reset() {
  // We keep the buffer_ storage; the next pass will overwrite its contents.
  jump_edges_.clear();
  ResetPc(0);
}

void RegExpBytecodeWriter::EmitRawBytecodeStream(const uint8_t* data, int len) {
  EnsureCapacity(len);
  // Must start at a bytecode boundary.
  DCHECK_EQ(pc_within_bc_, end_of_bc_);
  // We cannot check whether we also end at a boundary since we don't know what
  // data contains. Let's at least verify alignment.
  // TODO(jgruber): We could use RegExpBytecodeIterator to verify in DEBUG.
  DCHECK(IsAligned(len, kBytecodeAlignment));
  MemCopy(buffer_.data() + pc_, data, len);
  // End at a bytecode boundary, update bookkeeping.
  pc_ += len;
#ifdef DEBUG
  pc_within_bc_ = pc_;
  end_of_bc_ = pc_;
#endif
}

void RegExpBytecodeWriter::EmitRawBytecodeStream(
    const RegExpBytecodeWriter* src_writer, int src_offset, int length) {
  const int start_pc = pc_;

  EmitRawBytecodeStream(src_writer->buffer().data() + src_offset, length);

  // Copy jumps in range.
  const auto& src_edges = src_writer->jump_edges();
  auto jump_iter = src_edges.lower_bound(src_offset);
  // Iterate over all jumps that start in the copied range.
  while (jump_iter != src_edges.end() &&
         jump_iter->first < src_offset + length) {
    int old_source = jump_iter->first;
    int old_target = jump_iter->second;
    int new_source = start_pc + (old_source - src_offset);
    jump_edges_.emplace(new_source, old_target);
    jump_iter++;
  }
}

void RegExpBytecodeWriter::Finalize(RegExpBytecode bc) {
  int size = RegExpBytecodes::Size(bc);
  EMIT_PADDING(size);
  pc_ += size;
#ifdef DEBUG
  DCHECK_EQ(pc_within_bc_, end_of_bc_);
  pc_within_bc_ = pc_;
  end_of_bc_ = pc_;
#endif
}

RegExpBytecodeGenerator::RegExpBytecodeGenerator(Isolate* isolate, Zone* zone,
                                                 Mode mode)
    : RegExpMacroAssembler(isolate, zone, mode),
      RegExpBytecodeWriter(zone),
      isolate_(isolate) {}

RegExpBytecodeGenerator::~RegExpBytecodeGenerator() {
  if (backtrack_.is_linked()) backtrack_.Unuse();
}

RegExpBytecodeGenerator::IrregexpImplementation
RegExpBytecodeGenerator::Implementation() {
  return kBytecodeImplementation;
}

template <RegExpBytecode bytecode, typename... Args>
void RegExpBytecodeWriter::Emit(Args... args) {
  using Operands = RegExpBytecodeOperands<bytecode>;
  static_assert(sizeof...(Args) == Operands::kCount,
                "Wrong number of operands");

  auto arguments_tuple = std::make_tuple(args...);
  EmitBytecode(bytecode);
  Operands::ForEachOperandWithIndex([&]<auto op, size_t index>() {
    constexpr RegExpBytecodeOperandType type = Operands::Type(op);
    constexpr int offset = Operands::Offset(op);
    auto value = std::get<index>(arguments_tuple);
    EmitOperand<type>(value, offset);
  });
  Finalize(bytecode);
}

namespace {

// Helper to get the underlying type of an enum, or the type itself if it isn't
// an enum.
template <typename T>
struct get_underlying_or_self {
  using type = T;
};

template <typename T>
  requires std::is_enum_v<T>
struct get_underlying_or_self<T> {
  using type = std::underlying_type_t<T>;
};

}  // namespace

template <RegExpBytecodeOperandType OperandType, typename T>
auto RegExpBytecodeWriter::GetCheckedBasicOperandValue(T value) {
  static_assert(RegExpOperandTypeTraits<OperandType>::kIsBasic);
  using Traits = RegExpOperandTypeTraits<OperandType>;
  using EnumOrCType = Traits::kCType;
  using CType = get_underlying_or_self<EnumOrCType>::type;
  if constexpr (std::is_enum_v<EnumOrCType>) {
    static_assert(std::is_same_v<T, EnumOrCType>);
  } else {
    static_assert(std::is_convertible_v<T, CType>);
  }
  DCHECK_GE(value, Traits::kMinValue);
  DCHECK_LE(value, Traits::kMaxValue);
  return static_cast<CType>(value);
}

template <RegExpBytecodeOperandType OperandType, typename T>
void RegExpBytecodeWriter::EmitOperand(T value, int offset) {
  if constexpr (OperandType == RegExpBytecodeOperandType::kJumpTarget) {
    jump_edges_.emplace(pc_ + offset, static_cast<int>(value));
  }
  Emit(GetCheckedBasicOperandValue<OperandType>(value), offset);
}

void RegExpBytecodeWriter::PatchJump(int target, int absolute_offset) {
  DCHECK(jump_edges_.contains(absolute_offset));
  OverwriteValue<uint32_t>(target, absolute_offset);
  jump_edges_[absolute_offset] = target;
}

template <typename T>
void RegExpBytecodeWriter::EmitOperand(RegExpBytecodeOperandType type, T value,
                                       int offset) {
  switch (type) {
#define CASE(Name, ...)                    \
  case RegExpBytecodeOperandType::k##Name: \
    return EmitOperand<ReBcOpType::k##Name>(value, offset);
    BYTECODE_OPERAND_TYPE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

template <>
void RegExpBytecodeWriter::EmitOperand<ReBcOpType::kJumpTarget>(Label* label,
                                                                int offset) {
  DCHECK_NOT_NULL(label);
  const int current_pc = pc_ + offset;
  int pos = 0;
  if (label->is_bound()) {
    pos = label->pos();
    jump_edges_.emplace(current_pc, pos);
  } else {
    if (label->is_linked()) {
      pos = label->pos();
    }
    label->link_to(current_pc);
  }
  Emit<uint32_t>(pos, offset);
}

template <>
void RegExpBytecodeWriter::EmitOperand<ReBcOpType::kBitTable>(
    Handle<ByteArray> table, int offset) {
  for (int i = 0; i < RegExpMacroAssembler::kTableSize; i += kBitsPerByte) {
    uint8_t byte = 0;
    for (int j = 0; j < kBitsPerByte; j++) {
      if (table->get(i + j) != 0) byte |= 1 << j;
    }
    Emit(byte, offset + i / kBitsPerByte);
  }
}

template <>
void RegExpBytecodeWriter::EmitOperand<ReBcOpType::kBitTable>(
    const uint8_t* src, int offset) {
  // The emitted table operand is 16 bytes long.
  static_assert(RegExpMacroAssembler::kTableSize / kBitsPerByte == 16);
  const uint32_t* cursor = reinterpret_cast<const uint32_t*>(src);
  static constexpr int kWordCount =
      (RegExpMacroAssembler::kTableSize / (kBitsPerByte * kInt32Size));
  for (int i = 0; i < kWordCount; i++) {
    Emit(cursor[i], offset + i * kInt32Size);
  }
}

template <RegExpBytecode bytecode, typename... Args>
void RegExpBytecodeGenerator::Emit(Args... args) {
  // Converts nullptr labels into our internal backtrack_ label.
  auto fix_label = [this](auto arg) {
    if constexpr (std::is_convertible_v<decltype(arg), Label*>) {
      Label* l = static_cast<Label*>(arg);
      return l ? l : &backtrack_;
    } else {
      return arg;
    }
  };
  RegExpBytecodeWriter::Emit<bytecode>(fix_label(args)...);
}

void RegExpBytecodeGenerator::Bind(Label* l) {
  DCHECK(!l->is_bound());
  if (l->is_linked()) {
    int pos = l->pos();
    while (pos != 0) {
      int fixup = pos;
      pos = *reinterpret_cast<int32_t*>(buffer_.data() + fixup);
      OverwriteValue<uint32_t>(pc_, fixup);
      jump_edges().emplace(fixup, pc_);
    }
  }
  l->bind_to(pc_);
}

void RegExpBytecodeGenerator::PopRegister(int register_index) {
  Emit<RegExpBytecode::kPopRegister>(register_index);
}

void RegExpBytecodeGenerator::PushRegister(int register_index,
                                           StackCheckFlag check_stack_limit) {
  Emit<RegExpBytecode::kPushRegister>(register_index, check_stack_limit);
}

void RegExpBytecodeGenerator::WriteCurrentPositionToRegister(int register_index,
                                                             int cp_offset) {
  Emit<RegExpBytecode::kWriteCurrentPositionToRegister>(register_index,
                                                        cp_offset);
}

void RegExpBytecodeGenerator::ClearRegisters(int reg_from, int reg_to) {
  DCHECK_LE(reg_from, reg_to);
  Emit<RegExpBytecode::kClearRegisters>(reg_from, reg_to);
}

void RegExpBytecodeGenerator::ReadCurrentPositionFromRegister(
    int register_index) {
  Emit<RegExpBytecode::kReadCurrentPositionFromRegister>(register_index);
}

void RegExpBytecodeGenerator::WriteStackPointerToRegister(int register_index) {
  Emit<RegExpBytecode::kWriteStackPointerToRegister>(register_index);
}

void RegExpBytecodeGenerator::ReadStackPointerFromRegister(int register_index) {
  Emit<RegExpBytecode::kReadStackPointerFromRegister>(register_index);
}

void RegExpBytecodeGenerator::SetCurrentPositionFromEnd(int by) {
  Emit<RegExpBytecode::kSetCurrentPositionFromEnd>(by);
}

void RegExpBytecodeGenerator::SetRegister(int register_index, int to) {
  Emit<RegExpBytecode::kSetRegister>(register_index, to);
}

void RegExpBytecodeGenerator::AdvanceRegister(int register_index, int by) {
  Emit<RegExpBytecode::kAdvanceRegister>(register_index, by);
}

void RegExpBytecodeGenerator::PopCurrentPosition() {
  Emit<RegExpBytecode::kPopCurrentPosition>();
}

void RegExpBytecodeGenerator::PushCurrentPosition() {
  Emit<RegExpBytecode::kPushCurrentPosition>();
}

void RegExpBytecodeGenerator::Backtrack() {
  int error_code =
      can_fallback() ? RegExp::RE_FALLBACK_TO_EXPERIMENTAL : RegExp::RE_FAILURE;
  Emit<RegExpBytecode::kBacktrack>(error_code);
}

void RegExpBytecodeGenerator::GoTo(Label* label) {
  Emit<RegExpBytecode::kGoTo>(label);
}

void RegExpBytecodeGenerator::PushBacktrack(Label* label) {
  Emit<RegExpBytecode::kPushBacktrack>(label);
}

bool RegExpBytecodeGenerator::Succeed() {
  Emit<RegExpBytecode::kSucceed>();
  return false;  // Restart matching for global regexp not supported.
}

void RegExpBytecodeGenerator::Fail() { Emit<RegExpBytecode::kFail>(); }

void RegExpBytecodeGenerator::AdvanceCurrentPosition(int by) {
  Emit<RegExpBytecode::kAdvanceCurrentPosition>(by);
}

void RegExpBytecodeGenerator::CheckFixedLengthLoop(
    Label* on_tos_equals_current_position) {
  Emit<RegExpBytecode::kCheckFixedLengthLoop>(on_tos_equals_current_position);
}

void RegExpBytecodeGenerator::CheckPosition(int cp_offset,
                                            Label* on_outside_input) {
  Emit<RegExpBytecode::kCheckPosition>(cp_offset, on_outside_input);
}

void RegExpBytecodeGenerator::CheckSpecialClassRanges(StandardCharacterSet type,
                                                      Label* on_no_match) {
  DCHECK(CanOptimizeSpecialClassRanges(type));
  Emit<RegExpBytecode::kCheckSpecialClassRanges>(type, on_no_match);
}

void RegExpBytecodeGenerator::LoadCurrentCharacterImpl(int cp_offset,
                                                       Label* on_failure,
                                                       bool check_bounds,
                                                       int characters,
                                                       int eats_at_least) {
  DCHECK_GE(eats_at_least, characters);
  if (eats_at_least > characters && check_bounds) {
    Emit<RegExpBytecode::kCheckPosition>(cp_offset + eats_at_least - 1,
                                         on_failure);
    check_bounds = false;  // Load below doesn't need to check.
  }

  CHECK(base::IsInRange(cp_offset, kMinCPOffset, kMaxCPOffset));
  if (check_bounds) {
    if (characters == 4) {
      Emit<RegExpBytecode::kLoad4CurrentChars>(cp_offset, on_failure);
    } else if (characters == 2) {
      Emit<RegExpBytecode::kLoad2CurrentChars>(cp_offset, on_failure);
    } else {
      DCHECK_EQ(1, characters);
      Emit<RegExpBytecode::kLoadCurrentCharacter>(cp_offset, on_failure);
    }
  } else {
    if (characters == 4) {
      Emit<RegExpBytecode::kLoad4CurrentCharsUnchecked>(cp_offset);
    } else if (characters == 2) {
      Emit<RegExpBytecode::kLoad2CurrentCharsUnchecked>(cp_offset);
    } else {
      DCHECK_EQ(1, characters);
      Emit<RegExpBytecode::kLoadCurrentCharacterUnchecked>(cp_offset);
    }
  }
}

void RegExpBytecodeGenerator::CheckCharacterLT(base::uc16 limit,
                                               Label* on_less) {
  Emit<RegExpBytecode::kCheckCharacterLT>(limit, on_less);
}

void RegExpBytecodeGenerator::CheckCharacterGT(base::uc16 limit,
                                               Label* on_greater) {
  Emit<RegExpBytecode::kCheckCharacterGT>(limit, on_greater);
}

void RegExpBytecodeGenerator::CheckCharacter(uint32_t c, Label* on_equal) {
  if (c > kMaxSingleCharValue) {
    Emit<RegExpBytecode::kCheck4Chars>(c, on_equal);
  } else {
    Emit<RegExpBytecode::kCheckCharacter>(c, on_equal);
  }
}

void RegExpBytecodeGenerator::CheckAtStart(int cp_offset, Label* on_at_start) {
  Emit<RegExpBytecode::kCheckAtStart>(cp_offset, on_at_start);
}

void RegExpBytecodeGenerator::CheckNotAtStart(int cp_offset,
                                              Label* on_not_at_start) {
  Emit<RegExpBytecode::kCheckNotAtStart>(cp_offset, on_not_at_start);
}

void RegExpBytecodeGenerator::CheckNotCharacter(uint32_t c,
                                                Label* on_not_equal) {
  if (c > kMaxSingleCharValue) {
    Emit<RegExpBytecode::kCheckNot4Chars>(c, on_not_equal);
  } else {
    Emit<RegExpBytecode::kCheckNotCharacter>(c, on_not_equal);
  }
}

void RegExpBytecodeGenerator::CheckCharacterAfterAnd(uint32_t c, uint32_t mask,
                                                     Label* on_equal) {
  // TODO(pthier): This is super hacky. We could still check for 4 characters
  // (with the last 2 being 0 after masking them), but not emit AndCheck4Chars.
  // This is rather confusing and should be changed.
  if (c > kMaxSingleCharValue) {
    Emit<RegExpBytecode::kAndCheck4Chars>(c, mask, on_equal);
  } else {
    Emit<RegExpBytecode::kCheckCharacterAfterAnd>(c, mask, on_equal);
  }
}

void RegExpBytecodeGenerator::CheckNotCharacterAfterAnd(uint32_t c,
                                                        uint32_t mask,
                                                        Label* on_not_equal) {
  // TODO(pthier): This is super hacky. We could still check for 4 characters
  // (with the last 2 being 0 after masking them), but not emit AndCheck4Chars.
  // This is rather confusing and should be changed.
  if (c > kMaxSingleCharValue) {
    Emit<RegExpBytecode::kAndCheckNot4Chars>(c, mask, on_not_equal);
  } else {
    Emit<RegExpBytecode::kCheckNotCharacterAfterAnd>(c, mask, on_not_equal);
  }
}

void RegExpBytecodeGenerator::CheckNotCharacterAfterMinusAnd(
    base::uc16 c, base::uc16 minus, base::uc16 mask, Label* on_not_equal) {
  Emit<RegExpBytecode::kCheckNotCharacterAfterMinusAnd>(c, minus, mask,
                                                        on_not_equal);
}

void RegExpBytecodeGenerator::CheckCharacterInRange(base::uc16 from,
                                                    base::uc16 to,
                                                    Label* on_in_range) {
  Emit<RegExpBytecode::kCheckCharacterInRange>(from, to, on_in_range);
}

void RegExpBytecodeGenerator::CheckCharacterNotInRange(base::uc16 from,
                                                       base::uc16 to,
                                                       Label* on_not_in_range) {
  Emit<RegExpBytecode::kCheckCharacterNotInRange>(from, to, on_not_in_range);
}

void RegExpBytecodeGenerator::CheckBitInTable(Handle<ByteArray> table,
                                              Label* on_bit_set) {
  Emit<RegExpBytecode::kCheckBitInTable>(on_bit_set, table);
}

void RegExpBytecodeGenerator::SkipUntilBitInTable(
    int cp_offset, Handle<ByteArray> table, Handle<ByteArray> nibble_table,
    int advance_by, Label* on_match, Label* on_no_match) {
  Emit<RegExpBytecode::kSkipUntilBitInTable>(cp_offset, advance_by, table,
                                             on_match, on_no_match);
}

void RegExpBytecodeGenerator::SkipUntilCharAnd(int cp_offset, int advance_by,
                                               unsigned character,
                                               unsigned mask, int eats_at_least,
                                               Label* on_match,
                                               Label* on_no_match) {
  Emit<RegExpBytecode::kSkipUntilCharAnd>(cp_offset, advance_by, character,
                                          mask, eats_at_least, on_match,
                                          on_no_match);
}

void RegExpBytecodeGenerator::SkipUntilChar(int cp_offset, int advance_by,
                                            unsigned character, Label* on_match,
                                            Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void RegExpBytecodeGenerator::SkipUntilCharPosChecked(
    int cp_offset, int advance_by, unsigned character, int eats_at_least,
    Label* on_match, Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void RegExpBytecodeGenerator::SkipUntilCharOrChar(int cp_offset, int advance_by,
                                                  unsigned char1,
                                                  unsigned char2,
                                                  Label* on_match,
                                                  Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void RegExpBytecodeGenerator::SkipUntilGtOrNotBitInTable(
    int cp_offset, int advance_by, unsigned character, Handle<ByteArray> table,
    Label* on_match, Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void RegExpBytecodeGenerator::SkipUntilOneOfMasked(
    int cp_offset, int advance_by, unsigned both_chars, unsigned both_mask,
    int max_offset, unsigned chars1, unsigned mask1, unsigned chars2,
    unsigned mask2, Label* on_match1, Label* on_match2, Label* on_failure) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void RegExpBytecodeGenerator::SkipUntilOneOfMasked3(
    const SkipUntilOneOfMasked3Args& args) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void RegExpBytecodeGenerator::CheckNotBackReference(int start_reg,
                                                    bool read_backward,
                                                    Label* on_not_equal) {
  if (read_backward) {
    Emit<RegExpBytecode::kCheckNotBackRefBackward>(start_reg, on_not_equal);
  } else {
    Emit<RegExpBytecode::kCheckNotBackRef>(start_reg, on_not_equal);
  }
}

void RegExpBytecodeGenerator::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, bool unicode, Label* on_not_equal) {
  if (read_backward) {
    if (unicode) {
      Emit<RegExpBytecode::kCheckNotBackRefNoCaseUnicodeBackward>(start_reg,
                                                                  on_not_equal);
    } else {
      Emit<RegExpBytecode::kCheckNotBackRefNoCaseBackward>(start_reg,
                                                           on_not_equal);
    }
  } else {
    if (unicode) {
      Emit<RegExpBytecode::kCheckNotBackRefNoCaseUnicode>(start_reg,
                                                          on_not_equal);
    } else {
      Emit<RegExpBytecode::kCheckNotBackRefNoCase>(start_reg, on_not_equal);
    }
  }
}

void RegExpBytecodeGenerator::IfRegisterLT(int register_index, int comparand,
                                           Label* on_less_than) {
  Emit<RegExpBytecode::kIfRegisterLT>(register_index, comparand, on_less_than);
}

void RegExpBytecodeGenerator::IfRegisterGE(int register_index, int comparand,
                                           Label* on_greater_or_equal) {
  Emit<RegExpBytecode::kIfRegisterGE>(register_index, comparand,
                                      on_greater_or_equal);
}

void RegExpBytecodeGenerator::IfRegisterEqPos(int register_index,
                                              Label* on_equal) {
  Emit<RegExpBytecode::kIfRegisterEqPos>(register_index, on_equal);
}

DirectHandle<HeapObject> RegExpBytecodeGenerator::GetCode(
    DirectHandle<String> source, RegExpFlags flags) {
  Bind(&backtrack_);
  Backtrack();

  DirectHandle<TrustedByteArray> array;
  if (v8_flags.regexp_peephole_optimization) {
    array = RegExpBytecodePeepholeOptimization::OptimizeBytecode(
        isolate_, zone(), source, this);
  } else {
    array = isolate_->factory()->NewTrustedByteArray(length());
    CopyBufferTo(array->begin());
  }

  return array;
}

void RegExpBytecodeWriter::CopyBufferTo(uint8_t* a) const {
  MemCopy(a, buffer_.data(), length());
}

// Instantiate template methods.
#define CASE(Name, ...)                                                    \
  template V8_EXPORT_PRIVATE void                                          \
  RegExpBytecodeWriter::EmitOperand<RegExpBytecodeOperandType::k##Name>(   \
      RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name>::kCType, \
      int);
BASIC_BYTECODE_OPERAND_TYPE_LIST(CASE)
BASIC_BYTECODE_OPERAND_TYPE_LIMITS_LIST(CASE)
#undef CASE

}  // namespace internal
}  // namespace v8

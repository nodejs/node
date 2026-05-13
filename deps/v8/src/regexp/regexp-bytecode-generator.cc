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
namespace regexp {

// Used to decide whether we use the `Char` or `4Chars` variant of a bytecode.
static constexpr int kMaxSingleCharValue =
    OperandTypeTraits<BytecodeOperandType::kChar>::kMaxValue;

// TODO(jgruber): Move all Writer methods before Generator methods.

BytecodeWriter::BytecodeWriter(Zone* zone)
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

void BytecodeWriter::ExpandBuffer(size_t new_size) {
  // TODO(jgruber): It's not necessary to default-initialize new elements.
  buffer_.resize(new_size);
}

void BytecodeWriter::EmitRawBytecodeStream(const uint8_t* data, int len) {
  EnsureCapacity(len);
  // Must start at a bytecode boundary.
  DCHECK_EQ(pc_within_bc_, end_of_bc_);
  // We cannot check whether we also end at a boundary since we don't know what
  // data contains. Let's at least verify alignment.
  // TODO(jgruber): We could use BytecodeIterator to verify in DEBUG.
  DCHECK(IsAligned(len, kBytecodeAlignment));
  MemCopy(buffer_.data() + pc_, data, len);
  // End at a bytecode boundary, update bookkeeping.
  pc_ += len;
#ifdef DEBUG
  pc_within_bc_ = pc_;
  end_of_bc_ = pc_;
#endif
}

void BytecodeWriter::EmitRawBytecodeStream(const BytecodeWriter* src_writer,
                                           int src_offset, int length) {
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

void BytecodeWriter::Finalize(Bytecode bc) {
  int size = Bytecodes::Size(bc);
  EMIT_PADDING(size);
  pc_ += size;
#ifdef DEBUG
  DCHECK_EQ(pc_within_bc_, end_of_bc_);
  pc_within_bc_ = pc_;
  end_of_bc_ = pc_;
#endif
}

BytecodeGenerator::BytecodeGenerator(Isolate* isolate, Zone* zone, Mode mode)
    : RegExpMacroAssembler(isolate, zone, mode),
      BytecodeWriter(zone),
      isolate_(isolate) {}

BytecodeGenerator::~BytecodeGenerator() {
  if (backtrack_.is_linked()) backtrack_.Unuse();
}

BytecodeGenerator::IrregexpImplementation BytecodeGenerator::Implementation() {
  return kBytecodeImplementation;
}

template <Bytecode bytecode, typename... Args>
void BytecodeWriter::Emit(Args... args) {
  using Operands = BytecodeOperands<bytecode>;
  static_assert(sizeof...(Args) == Operands::kCount,
                "Wrong number of operands");

  auto arguments_tuple = std::make_tuple(args...);
  EmitBytecode(bytecode);
  Operands::ForEachOperandWithIndex([&]<auto op, size_t index>() {
    constexpr BytecodeOperandType type = Operands::Type(op);
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

template <BytecodeOperandType OperandType, typename T>
auto BytecodeWriter::GetCheckedBasicOperandValue(T value) {
  static_assert(OperandTypeTraits<OperandType>::kIsBasic);
  using Traits = OperandTypeTraits<OperandType>;
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

template <BytecodeOperandType OperandType, typename T>
void BytecodeWriter::EmitOperand(T value, int offset) {
  if constexpr (OperandType == BytecodeOperandType::kJumpTarget) {
    jump_edges_.emplace(pc_ + offset, static_cast<int>(value));
  }
  Emit(GetCheckedBasicOperandValue<OperandType>(value), offset);
}

void BytecodeWriter::PatchJump(int target, int absolute_offset) {
  DCHECK(jump_edges_.contains(absolute_offset));
  OverwriteValue<uint32_t>(target, absolute_offset);
  jump_edges_[absolute_offset] = target;
}

template <typename T>
void BytecodeWriter::EmitOperand(BytecodeOperandType type, T value,
                                 int offset) {
  switch (type) {
#define CASE(Name, ...)              \
  case BytecodeOperandType::k##Name: \
    return EmitOperand<ReBcOpType::k##Name>(value, offset);
    BYTECODE_OPERAND_TYPE_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

template <>
void BytecodeWriter::EmitOperand<ReBcOpType::kJumpTarget>(Label* label,
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
void BytecodeWriter::EmitOperand<ReBcOpType::kBitTable>(Handle<ByteArray> table,
                                                        int offset) {
  for (int i = 0; i < RegExpMacroAssembler::kTableSize; i += kBitsPerByte) {
    uint8_t byte = 0;
    for (int j = 0; j < kBitsPerByte; j++) {
      if (table->get(i + j) != 0) byte |= 1 << j;
    }
    Emit(byte, offset + i / kBitsPerByte);
  }
}

template <>
void BytecodeWriter::EmitOperand<ReBcOpType::kBitTable>(const uint8_t* src,
                                                        int offset) {
  // The emitted table operand is 16 bytes long.
  static_assert(RegExpMacroAssembler::kTableSize / kBitsPerByte == 16);
  const uint32_t* cursor = reinterpret_cast<const uint32_t*>(src);
  static constexpr int kWordCount =
      (RegExpMacroAssembler::kTableSize / (kBitsPerByte * kInt32Size));
  for (int i = 0; i < kWordCount; i++) {
    Emit(cursor[i], offset + i * kInt32Size);
  }
}

template <Bytecode bytecode, typename... Args>
void BytecodeGenerator::Emit(Args... args) {
  // Converts nullptr labels into our internal backtrack_ label.
  auto fix_label = [this](auto arg) {
    if constexpr (std::is_convertible_v<decltype(arg), Label*>) {
      Label* l = static_cast<Label*>(arg);
      return l ? l : &backtrack_;
    } else {
      return arg;
    }
  };
  BytecodeWriter::Emit<bytecode>(fix_label(args)...);
}

void BytecodeGenerator::Bind(Label* l) {
  advance_current_end_ = kInvalidPC;
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

void BytecodeGenerator::PopRegister(int register_index) {
  Emit<Bytecode::kPopRegister>(register_index);
}

void BytecodeGenerator::PushRegister(int register_index,
                                     StackCheckFlag check_stack_limit) {
  Emit<Bytecode::kPushRegister>(register_index, check_stack_limit);
}

void BytecodeGenerator::WriteCurrentPositionToRegister(int register_index,
                                                       int cp_offset) {
  Emit<Bytecode::kWriteCurrentPositionToRegister>(register_index, cp_offset);
}

void BytecodeGenerator::ClearRegisters(int reg_from, int reg_to) {
  DCHECK_LE(reg_from, reg_to);
  Emit<Bytecode::kClearRegisters>(reg_from, reg_to);
}

void BytecodeGenerator::ReadCurrentPositionFromRegister(int register_index) {
  Emit<Bytecode::kReadCurrentPositionFromRegister>(register_index);
}

void BytecodeGenerator::WriteStackPointerToRegister(int register_index) {
  Emit<Bytecode::kWriteStackPointerToRegister>(register_index);
}

void BytecodeGenerator::ReadStackPointerFromRegister(int register_index) {
  Emit<Bytecode::kReadStackPointerFromRegister>(register_index);
}

void BytecodeGenerator::SetCurrentPositionFromEnd(int by) {
  Emit<Bytecode::kSetCurrentPositionFromEnd>(by);
}

void BytecodeGenerator::SetRegister(int register_index, int to) {
  Emit<Bytecode::kSetRegister>(register_index, to);
}

void BytecodeGenerator::AdvanceRegister(int register_index, int by) {
  Emit<Bytecode::kAdvanceRegister>(register_index, by);
}

void BytecodeGenerator::PopCurrentPosition() {
  Emit<Bytecode::kPopCurrentPosition>();
}

void BytecodeGenerator::PushCurrentPosition() {
  Emit<Bytecode::kPushCurrentPosition>();
}

void BytecodeGenerator::Backtrack() {
  int error_code =
      can_fallback() ? RegExp::RE_FALLBACK_TO_EXPERIMENTAL : RegExp::RE_FAILURE;
  Emit<Bytecode::kBacktrack>(error_code);
}

void BytecodeGenerator::GoTo(Label* label) {
  if (advance_current_end_ == pc_) {
    // Fuse the preceding AdvanceCurrentPosition into kAdvanceCpAndGoto.
    ResetPc(advance_current_start_);
    Emit<Bytecode::kAdvanceCpAndGoto>(advance_current_offset_, label);
    advance_current_end_ = kInvalidPC;
  } else {
    Emit<Bytecode::kGoTo>(label);
  }
}

void BytecodeGenerator::PushBacktrack(Label* label) {
  Emit<Bytecode::kPushBacktrack>(label);
}

bool BytecodeGenerator::Succeed() {
  Emit<Bytecode::kSucceed>();
  return false;  // Restart matching for global regexp not supported.
}

void BytecodeGenerator::Fail() { Emit<Bytecode::kFail>(); }

void BytecodeGenerator::AdvanceCurrentPosition(int by) {
  advance_current_start_ = pc_;
  advance_current_offset_ = by;
  Emit<Bytecode::kAdvanceCurrentPosition>(by);
  advance_current_end_ = pc_;
}

void BytecodeGenerator::CheckFixedLengthLoop(
    Label* on_tos_equals_current_position) {
  Emit<Bytecode::kCheckFixedLengthLoop>(on_tos_equals_current_position);
}

void BytecodeGenerator::CheckPosition(int cp_offset, Label* on_outside_input) {
  Emit<Bytecode::kCheckPosition>(cp_offset, on_outside_input);
}

void BytecodeGenerator::CheckSpecialClassRanges(StandardCharacterSet type,
                                                Label* on_no_match) {
  DCHECK(CanOptimizeSpecialClassRanges(type));
  Emit<Bytecode::kCheckSpecialClassRanges>(type, on_no_match);
}

void BytecodeGenerator::LoadCurrentCharacterImpl(int cp_offset,
                                                 Label* on_failure,
                                                 bool check_bounds,
                                                 int characters,
                                                 int eats_at_least) {
  DCHECK_GE(eats_at_least, characters);
  if (eats_at_least > characters && check_bounds) {
    // eats_at_least + cp_offset can overflow BytecodeOperandType::kOffset.
    // Only emit this optimization if the computed value fits into the expected
    // range for the CheckPosition BC.
    using CheckPositionOps = BytecodeOperands<Bytecode::kCheckPosition>;
    using OffsetTraits =
        OperandTypeTraits<CheckPositionOps::Type(CheckPositionOps::cp_offset)>;
    using OffsetType = OffsetTraits::kCType;
    base::internal::CheckedNumeric<OffsetType> checked_offset = cp_offset;
    checked_offset += eats_at_least - 1;
    OffsetType total_offset;
    if (checked_offset.AssignIfValid(&total_offset) &&
        base::IsInRange(total_offset, OffsetTraits::kMinValue,
                        OffsetTraits::kMaxValue)) {
      Emit<Bytecode::kCheckPosition>(total_offset, on_failure);
      check_bounds = false;  // Load below doesn't need to check.
    }
  }

  CHECK(base::IsInRange(cp_offset, kMinCPOffset, kMaxCPOffset));
  if (check_bounds) {
    if (characters == 4) {
      Emit<Bytecode::kLoad4CurrentChars>(cp_offset, on_failure);
    } else if (characters == 2) {
      Emit<Bytecode::kLoad2CurrentChars>(cp_offset, on_failure);
    } else {
      DCHECK_EQ(1, characters);
      Emit<Bytecode::kLoadCurrentCharacter>(cp_offset, on_failure);
    }
  } else {
    if (characters == 4) {
      Emit<Bytecode::kLoad4CurrentCharsUnchecked>(cp_offset);
    } else if (characters == 2) {
      Emit<Bytecode::kLoad2CurrentCharsUnchecked>(cp_offset);
    } else {
      DCHECK_EQ(1, characters);
      Emit<Bytecode::kLoadCurrentCharacterUnchecked>(cp_offset);
    }
  }
}

void BytecodeGenerator::CheckCharacterLT(base::uc16 limit, Label* on_less) {
  Emit<Bytecode::kCheckCharacterLT>(limit, on_less);
}

void BytecodeGenerator::CheckCharacterGT(base::uc16 limit, Label* on_greater) {
  Emit<Bytecode::kCheckCharacterGT>(limit, on_greater);
}

void BytecodeGenerator::CheckCharacter(uint32_t c, Label* on_equal) {
  if (c > kMaxSingleCharValue) {
    Emit<Bytecode::kCheck4Chars>(c, on_equal);
  } else {
    Emit<Bytecode::kCheckCharacter>(c, on_equal);
  }
}

void BytecodeGenerator::CheckAtStart(int cp_offset, Label* on_at_start) {
  Emit<Bytecode::kCheckAtStart>(cp_offset, on_at_start);
}

void BytecodeGenerator::CheckNotAtStart(int cp_offset, Label* on_not_at_start) {
  Emit<Bytecode::kCheckNotAtStart>(cp_offset, on_not_at_start);
}

void BytecodeGenerator::CheckNotCharacter(uint32_t c, Label* on_not_equal) {
  if (c > kMaxSingleCharValue) {
    Emit<Bytecode::kCheckNot4Chars>(c, on_not_equal);
  } else {
    Emit<Bytecode::kCheckNotCharacter>(c, on_not_equal);
  }
}

void BytecodeGenerator::CheckCharacterAfterAnd(uint32_t c, uint32_t mask,
                                               Label* on_equal) {
  // TODO(pthier): This is super hacky. We could still check for 4 characters
  // (with the last 2 being 0 after masking them), but not emit AndCheck4Chars.
  // This is rather confusing and should be changed.
  if (c > kMaxSingleCharValue) {
    Emit<Bytecode::kAndCheck4Chars>(c, mask, on_equal);
  } else {
    Emit<Bytecode::kCheckCharacterAfterAnd>(c, mask, on_equal);
  }
}

void BytecodeGenerator::CheckNotCharacterAfterAnd(uint32_t c, uint32_t mask,
                                                  Label* on_not_equal) {
  // TODO(pthier): This is super hacky. We could still check for 4 characters
  // (with the last 2 being 0 after masking them), but not emit AndCheck4Chars.
  // This is rather confusing and should be changed.
  if (c > kMaxSingleCharValue) {
    Emit<Bytecode::kAndCheckNot4Chars>(c, mask, on_not_equal);
  } else {
    Emit<Bytecode::kCheckNotCharacterAfterAnd>(c, mask, on_not_equal);
  }
}

void BytecodeGenerator::CheckNotCharacterAfterMinusAnd(base::uc16 c,
                                                       base::uc16 minus,
                                                       base::uc16 mask,
                                                       Label* on_not_equal) {
  Emit<Bytecode::kCheckNotCharacterAfterMinusAnd>(c, minus, mask, on_not_equal);
}

void BytecodeGenerator::CheckCharacterInRange(base::uc16 from, base::uc16 to,
                                              Label* on_in_range) {
  Emit<Bytecode::kCheckCharacterInRange>(from, to, on_in_range);
}

void BytecodeGenerator::CheckCharacterNotInRange(base::uc16 from, base::uc16 to,
                                                 Label* on_not_in_range) {
  Emit<Bytecode::kCheckCharacterNotInRange>(from, to, on_not_in_range);
}

void BytecodeGenerator::CheckBitInTable(Handle<ByteArray> table,
                                        Label* on_bit_set) {
  Emit<Bytecode::kCheckBitInTable>(on_bit_set, table);
}

void BytecodeGenerator::SkipUntilBitInTable(int cp_offset,
                                            Handle<ByteArray> table,
                                            Handle<ByteArray> nibble_table,
                                            int advance_by, Label* on_match,
                                            Label* on_no_match) {
  Emit<Bytecode::kSkipUntilBitInTable>(cp_offset, advance_by, table, on_match,
                                       on_no_match);
}

void BytecodeGenerator::SkipUntilCharAnd(int cp_offset, int advance_by,
                                         unsigned character, unsigned mask,
                                         int eats_at_least, Label* on_match,
                                         Label* on_no_match) {
  Emit<Bytecode::kSkipUntilCharAnd>(cp_offset, advance_by, character, mask,
                                    eats_at_least, on_match, on_no_match);
}

void BytecodeGenerator::SkipUntilChar(int cp_offset, int advance_by,
                                      unsigned character, Label* on_match,
                                      Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void BytecodeGenerator::SkipUntilCharPosChecked(int cp_offset, int advance_by,
                                                unsigned character,
                                                int eats_at_least,
                                                Label* on_match,
                                                Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void BytecodeGenerator::SkipUntilCharOrChar(int cp_offset, int advance_by,
                                            unsigned char1, unsigned char2,
                                            Label* on_match,
                                            Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void BytecodeGenerator::SkipUntilGtOrNotBitInTable(
    int cp_offset, int advance_by, unsigned character, Handle<ByteArray> table,
    Label* on_match, Label* on_no_match) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void BytecodeGenerator::SkipUntilOneOfMasked(
    int cp_offset, int advance_by, unsigned both_chars, unsigned both_mask,
    int max_offset, unsigned chars1, unsigned mask1, unsigned chars2,
    unsigned mask2, Label* on_match1, Label* on_match2, Label* on_failure) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void BytecodeGenerator::SkipUntilOneOfMasked3(
    const SkipUntilOneOfMasked3Args& args) {
  // Only generated by peephole optimization.
  UNREACHABLE();
}

void BytecodeGenerator::CheckNotBackReference(int start_reg, bool read_backward,
                                              Label* on_not_equal) {
  if (read_backward) {
    Emit<Bytecode::kCheckNotBackRefBackward>(start_reg, on_not_equal);
  } else {
    Emit<Bytecode::kCheckNotBackRef>(start_reg, on_not_equal);
  }
}

void BytecodeGenerator::CheckNotBackReferenceIgnoreCase(int start_reg,
                                                        bool read_backward,
                                                        bool unicode,
                                                        Label* on_not_equal) {
  if (read_backward) {
    if (unicode) {
      Emit<Bytecode::kCheckNotBackRefNoCaseUnicodeBackward>(start_reg,
                                                            on_not_equal);
    } else {
      Emit<Bytecode::kCheckNotBackRefNoCaseBackward>(start_reg, on_not_equal);
    }
  } else {
    if (unicode) {
      Emit<Bytecode::kCheckNotBackRefNoCaseUnicode>(start_reg, on_not_equal);
    } else {
      Emit<Bytecode::kCheckNotBackRefNoCase>(start_reg, on_not_equal);
    }
  }
}

void BytecodeGenerator::IfRegisterLT(int register_index, int comparand,
                                     Label* on_less_than) {
  Emit<Bytecode::kIfRegisterLT>(register_index, comparand, on_less_than);
}

void BytecodeGenerator::IfRegisterGE(int register_index, int comparand,
                                     Label* on_greater_or_equal) {
  Emit<Bytecode::kIfRegisterGE>(register_index, comparand, on_greater_or_equal);
}

void BytecodeGenerator::IfRegisterEqPos(int register_index, Label* on_equal) {
  Emit<Bytecode::kIfRegisterEqPos>(register_index, on_equal);
}

DirectHandle<HeapObject> BytecodeGenerator::GetCode(
    DirectHandle<RegExpData> re_data, Flags flags) {
  Bind(&backtrack_);
  Backtrack();

  DirectHandle<TrustedByteArray> array;
  if (v8_flags.regexp_peephole_optimization) {
    array = BytecodePeepholeOptimization::OptimizeBytecode(isolate_, zone(),
                                                           re_data, this);
  } else {
    array =
        isolate_->factory()->NewTrustedByteArray(static_cast<uint32_t>(pc_));
    CopyBufferTo(array->begin());
  }

  return array;
}

void BytecodeWriter::CopyBufferTo(uint8_t* a) const {
  MemCopy(a, buffer_.data(), length());
}

// Instantiate template methods.
#define CASE(Name, ...)                                      \
  template V8_EXPORT_PRIVATE void                            \
  BytecodeWriter::EmitOperand<BytecodeOperandType::k##Name>( \
      OperandTypeTraits<BytecodeOperandType::k##Name>::kCType, int);
BASIC_BYTECODE_OPERAND_TYPE_LIST(CASE)
BASIC_BYTECODE_OPERAND_TYPE_LIMITS_LIST(CASE)
#undef CASE

}  // namespace regexp
}  // namespace internal
}  // namespace v8

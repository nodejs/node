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
auto RegExpCodeGenerator::GetArgumentValue(const uint8_t* pc) const {
  constexpr RegExpBytecodeOperandType op_type = Operands::Type(Operand);
  auto value = Operands::template Get<Operand>(pc);

  // If the operand is a Label, we return the Label created during the first
  // pass instead of an offset to the bytecode.
  if constexpr (op_type == ReBcOpType::kLabel) {
    return GetLabel(value);
  } else {
    return value;
  }
}

// Calls |func| passing all operand ids (except paddings) as a template
// parameter pack.
template <typename Operands, typename Func>
void DispatchByOperand(Func&& func) {
  // Filter padding operands.
  auto filtered_ops = []<std::size_t... Is>(std::index_sequence<Is...>) {
    return std::tuple_cat([]<std::size_t I>() {
      using Operand = typename Operands::Operand;
      constexpr auto id = static_cast<Operand>(I);
      if constexpr (Operands::Type(id) == ReBcOpType::kPadding1 ||
                    Operands::Type(id) == ReBcOpType::kPadding2) {
        return std::tuple<>();
      } else {
        return std::tuple(std::integral_constant<Operand, id>{});
      }
    }.template operator()<Is>()...);
  }(std::make_index_sequence<Operands::kCount>{});

  std::apply([&](auto... ops) { func.template operator()<ops.value...>(); },
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

#define GENERATE_VISIT_METHOD(Name, Enum, OperandsTuple, Types)       \
  template <>                                                         \
  void RegExpCodeGenerator::Visit<RegExpBytecode::k##Name>() {        \
    using Operands = RegExpBytecodeOperands<RegExpBytecode::k##Name>; \
    const uint8_t* pc = iter_.current_address();                      \
    VISIT_COMMENT(#Name);                                             \
                                                                      \
    DispatchByOperand<Operands>([&]<auto... operand_ids>() {          \
      __ Name(GetArgumentValue<Operands, operand_ids>(pc)...);        \
    });                                                               \
  }

BASIC_BYTECODE_LIST(GENERATE_VISIT_METHOD)

#undef GENERATE_VISIT_METHOD

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kBacktrack>() {
  VISIT_COMMENT("Backtrack");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kBacktrack>;
  // Operand |return_code| is ignored intentionally. It is only used in the
  // interpreter.
  static_assert(Operands::kCount == 1);
  __ Backtrack();
}

namespace {

// Convert the 16-byte (128 bit) |table_data| to a 128-byte ByteArray. Every bit
// in |table_data| is translated to its own byte (set to 0 or 1) in the
// ByteArray .
Handle<ByteArray> CreateBitTableByteArray(Isolate* isolate,
                                          const uint8_t* table_data) {
  Handle<ByteArray> table =
      isolate->factory()->NewByteArray(RegExpMacroAssembler::kTableSize);
  for (int i = 0; i < RegExpMacroAssembler::kTableSize / kBitsPerByte; i++) {
    uint8_t byte = table_data[i];
    for (int j = 0; j < kBitsPerByte; j++) {
      bool bit_set = (byte & (1 << j)) != 0;
      table->set(i * kBitsPerByte + j, bit_set);
    }
  }
  return table;
}

}  // namespace

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kCheckBitInTable>() {
  VISIT_COMMENT("CheckBitInTable");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kCheckBitInTable>;
  const uint8_t* pc = iter_.current_address();
  auto on_bit_set = GetArgumentValue<Operands, Operands::on_bit_set>(pc);
  const uint8_t* table_data = GetArgumentValue<Operands, Operands::table>(pc);
  static_assert(Operands::kCount == 2);
  Handle<ByteArray> table = CreateBitTableByteArray(isolate_, table_data);
  __ CheckBitInTable(table, on_bit_set);
}

template <>
void RegExpCodeGenerator::Visit<
    RegExpBytecode::kLoadCurrentCharacterUnchecked>() {
  VISIT_COMMENT("LoadCurrentCharacterUnchecked");
  using Operands =
      RegExpBytecodeOperands<RegExpBytecode::kLoadCurrentCharacterUnchecked>;
  const uint8_t* pc = iter_.current_address();
  auto cp_offset = GetArgumentValue<Operands, Operands::cp_offset>(pc);
  static_assert(Operands::kCount == 1);
  static constexpr int kChars = 1;
  __ LoadCurrentCharacterImpl(cp_offset, nullptr, false, kChars, kChars);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kLoad2CurrentChars>() {
  VISIT_COMMENT("Load2CurrentChars");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kLoad2CurrentChars>;
  const uint8_t* pc = iter_.current_address();
  auto cp_offset = GetArgumentValue<Operands, Operands::cp_offset>(pc);
  auto on_failure = GetArgumentValue<Operands, Operands::on_failure>(pc);
  static_assert(Operands::kCount == 2);
  static constexpr int kChars = 2;
  __ LoadCurrentCharacterImpl(cp_offset, on_failure, true, kChars, kChars);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kLoad2CurrentCharsUnchecked>() {
  VISIT_COMMENT("Load2CurrentCharsUnchecked");
  using Operands =
      RegExpBytecodeOperands<RegExpBytecode::kLoad2CurrentCharsUnchecked>;
  const uint8_t* pc = iter_.current_address();
  auto cp_offset = GetArgumentValue<Operands, Operands::cp_offset>(pc);
  static_assert(Operands::kCount == 1);
  static constexpr int kChars = 2;
  __ LoadCurrentCharacterImpl(cp_offset, nullptr, false, kChars, kChars);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kLoad4CurrentChars>() {
  VISIT_COMMENT("Load4CurrentChars");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kLoad4CurrentChars>;
  const uint8_t* pc = iter_.current_address();
  auto cp_offset = GetArgumentValue<Operands, Operands::cp_offset>(pc);
  auto on_failure = GetArgumentValue<Operands, Operands::on_failure>(pc);
  static_assert(Operands::kCount == 2);
  static constexpr int kChars = 4;
  __ LoadCurrentCharacterImpl(cp_offset, on_failure, true, kChars, kChars);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kLoad4CurrentCharsUnchecked>() {
  VISIT_COMMENT("Load4CurrentCharsUnchecked");
  using Operands =
      RegExpBytecodeOperands<RegExpBytecode::kLoad4CurrentCharsUnchecked>;
  const uint8_t* pc = iter_.current_address();
  auto cp_offset = GetArgumentValue<Operands, Operands::cp_offset>(pc);
  static_assert(Operands::kCount == 1);
  static constexpr int kChars = 4;
  __ LoadCurrentCharacterImpl(cp_offset, nullptr, false, kChars, kChars);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kCheck4Chars>() {
  VISIT_COMMENT("Check4Chars");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kCheck4Chars>;
  const uint8_t* pc = iter_.current_address();
  auto characters = GetArgumentValue<Operands, Operands::characters>(pc);
  auto on_equal = GetArgumentValue<Operands, Operands::on_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckCharacter(characters, on_equal);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kCheckNot4Chars>() {
  VISIT_COMMENT("CheckNot4Chars");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kCheckNot4Chars>;
  const uint8_t* pc = iter_.current_address();
  auto characters = GetArgumentValue<Operands, Operands::characters>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckNotCharacter(characters, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kAndCheck4Chars>() {
  VISIT_COMMENT("AndCheck4Chars");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kAndCheck4Chars>;
  const uint8_t* pc = iter_.current_address();
  auto characters = GetArgumentValue<Operands, Operands::characters>(pc);
  auto mask = GetArgumentValue<Operands, Operands::mask>(pc);
  auto on_equal = GetArgumentValue<Operands, Operands::on_equal>(pc);
  static_assert(Operands::kCount == 3);
  __ CheckCharacterAfterAnd(characters, mask, on_equal);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kAndCheckNot4Chars>() {
  VISIT_COMMENT("AndCheckNot4Chars");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kAndCheckNot4Chars>;
  const uint8_t* pc = iter_.current_address();
  auto characters = GetArgumentValue<Operands, Operands::characters>(pc);
  auto mask = GetArgumentValue<Operands, Operands::mask>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 3);
  __ CheckNotCharacterAfterAnd(characters, mask, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kAdvanceCpAndGoto>() {
  VISIT_COMMENT("AdvanceCpAndGoto");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kAdvanceCpAndGoto>;
  const uint8_t* pc = iter_.current_address();
  auto by = GetArgumentValue<Operands, Operands::by>(pc);
  auto on_goto = GetArgumentValue<Operands, Operands::on_goto>(pc);
  static_assert(Operands::kCount == 2);
  __ AdvanceCurrentPosition(by);
  __ GoTo(on_goto);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kCheckNotBackRef>() {
  VISIT_COMMENT("CheckNotBackRef");
  using Operands = RegExpBytecodeOperands<RegExpBytecode::kCheckNotBackRef>;
  const uint8_t* pc = iter_.current_address();
  auto start_reg = GetArgumentValue<Operands, Operands::start_reg>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckNotBackReference(start_reg, false, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kCheckNotBackRefNoCase>() {
  VISIT_COMMENT("CheckNotBackRefNoCase");
  using Operands =
      RegExpBytecodeOperands<RegExpBytecode::kCheckNotBackRefNoCase>;
  const uint8_t* pc = iter_.current_address();
  auto start_reg = GetArgumentValue<Operands, Operands::start_reg>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckNotBackReferenceIgnoreCase(start_reg, false, false, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<
    RegExpBytecode::kCheckNotBackRefNoCaseUnicode>() {
  VISIT_COMMENT("CheckNotBackRefNoCaseUnicode");
  using Operands =
      RegExpBytecodeOperands<RegExpBytecode::kCheckNotBackRefNoCaseUnicode>;
  const uint8_t* pc = iter_.current_address();
  auto start_reg = GetArgumentValue<Operands, Operands::start_reg>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckNotBackReferenceIgnoreCase(start_reg, false, true, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kCheckNotBackRefBackward>() {
  VISIT_COMMENT("CheckNotBackRefBackward");
  using Operands =
      RegExpBytecodeOperands<RegExpBytecode::kCheckNotBackRefBackward>;
  const uint8_t* pc = iter_.current_address();
  auto start_reg = GetArgumentValue<Operands, Operands::start_reg>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckNotBackReference(start_reg, true, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<
    RegExpBytecode::kCheckNotBackRefNoCaseBackward>() {
  VISIT_COMMENT("CheckNotBackRefNoCaseBackward");
  using Operands =
      RegExpBytecodeOperands<RegExpBytecode::kCheckNotBackRefNoCaseBackward>;
  const uint8_t* pc = iter_.current_address();
  auto start_reg = GetArgumentValue<Operands, Operands::start_reg>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckNotBackReferenceIgnoreCase(start_reg, true, false, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<
    RegExpBytecode::kCheckNotBackRefNoCaseUnicodeBackward>() {
  VISIT_COMMENT("CheckNotBackRefNoCaseUnicodeBackward");
  using Operands = RegExpBytecodeOperands<
      RegExpBytecode::kCheckNotBackRefNoCaseUnicodeBackward>;
  const uint8_t* pc = iter_.current_address();
  auto start_reg = GetArgumentValue<Operands, Operands::start_reg>(pc);
  auto on_not_equal = GetArgumentValue<Operands, Operands::on_not_equal>(pc);
  static_assert(Operands::kCount == 2);
  __ CheckNotBackReferenceIgnoreCase(start_reg, true, true, on_not_equal);
}

template <>
void RegExpCodeGenerator::Visit<RegExpBytecode::kCheckNotRegsEqual>() {
  // Unused bytecode.
  UNREACHABLE();
}

template <RegExpBytecode bc>
void RegExpCodeGenerator::Visit() {
  if (v8_flags.trace_regexp_assembler) {
    std::cout << "RegExp Code Generator: Unsupported Bytecode "
              << RegExpBytecodes::Name(bc) << std::endl;
  }
  has_unsupported_bytecode_ = true;
}

void RegExpCodeGenerator::PreVisitBytecodes() {
  iter_.ForEachBytecode([&]<RegExpBytecode bc>() {
    using Operands = RegExpBytecodeOperands<bc>;
    auto ensure_label = [&]<auto operand>() {
      const uint8_t* pc = iter_.current_address();
      uint32_t offset = Operands::template Get<operand>(pc);
      if (!jump_targets_.Contains(offset)) {
        jump_targets_.Add(offset);
        Label* label = &labels_[offset];
        new (label) Label();
      }
    };
    Operands::template ForEachOperandOfType<RegExpBytecodeOperandType::kLabel>(
        ensure_label);
  });
}

void RegExpCodeGenerator::VisitBytecodes() {
  for (; !iter_.done() && !has_unsupported_bytecode_; iter_.advance()) {
    if (jump_targets_.Contains(iter_.current_offset())) {
      __ Bind(&labels_[iter_.current_offset()]);
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

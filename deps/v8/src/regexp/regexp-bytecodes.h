// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODES_H_
#define V8_REGEXP_REGEXP_BYTECODES_H_

#include "src/base/bounds.h"
#include "src/base/macros.h"
#include "src/base/strings.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

// Basic operand types that have a direct mapping to a C-type.
// Getters/Setters for these are fully auto-generated.
// Format: V(Name, C type)
#define BASIC_BYTECODE_OPERAND_TYPE_LIST(V) \
  V(Int16, int16_t)                         \
  V(Int32, int32_t)                         \
  V(Uint32, uint32_t)                       \
  V(Char, base::uc16)                       \
  V(JumpTarget, uint32_t)

#define BASIC_BYTECODE_OPERAND_TYPE_LIMITS_LIST(V)                           \
  V(Offset, int16_t, RegExpMacroAssembler::kMinCPOffset,                     \
    RegExpMacroAssembler::kMaxCPOffset)                                      \
  V(Register, uint16_t, 0, RegExpMacroAssembler::kMaxRegister)               \
  V(StackCheckFlag, RegExpMacroAssembler::StackCheckFlag,                    \
    RegExpMacroAssembler::StackCheckFlag::kNoStackLimitCheck,                \
    RegExpMacroAssembler::StackCheckFlag::kCheckStackLimit)                  \
  V(StandardCharacterSet, StandardCharacterSet,                              \
    StandardCharacterSet::kEverything, StandardCharacterSet::kWord)

// Special operand types that don't have a direct mapping to a C-type.
// Getters/Setters for these types need to be specialized manually.
// Format: V(Name, Size in bytes, Alignment in bytes)
#define SPECIAL_BYTECODE_OPERAND_TYPE_LIST(V) V(BitTable, 16, 1)

#define BYTECODE_OPERAND_TYPE_LIST(V)        \
  BASIC_BYTECODE_OPERAND_TYPE_LIST(V)        \
  BASIC_BYTECODE_OPERAND_TYPE_LIMITS_LIST(V) \
  SPECIAL_BYTECODE_OPERAND_TYPE_LIST(V)

enum class RegExpBytecodeOperandType : uint8_t {
#define DECLARE_OPERAND(Name, ...) k##Name,
  BYTECODE_OPERAND_TYPE_LIST(DECLARE_OPERAND)
#undef DECLARE_OPERAND
};

using ReBcOpType = RegExpBytecodeOperandType;

// Bytecode properties, used for analysis.
enum class RegExpBytecodeFlag : uint32_t {
  // Control flow.
  //
  // This bytecode doesn't fall through, i.e. it either terminates or jumps
  // unconditionally.
  kNoFallthrough = 1 << 0,
  // This bytecode has a kJumpTarget operand but doesn't branch. Note that we
  // don't have an explicit kBranch annotation; whether a bytecode branches is
  // inferred from the presence of kJumpTarget operands.
  kNoBranchDespiteJumpTargetOperand = 1 << 1,

  // Data flow.
  // TODO(jgruber): Consider adding verification based on these flags when
  // emitting bytecodes.
  //
  // This bytecode loads from the subject string into the current_character
  // register.
  kLoadsCC = 1 << 2,
  // This bytecode uses the current_character register's value.
  kUsesCC = 1 << 3,
};
using RegExpBytecodeFlags = base::Flags<RegExpBytecodeFlag>;
DEFINE_OPERATORS_FOR_FLAGS(RegExpBytecodeFlags)

using ReBcFlag = RegExpBytecodeFlag;

// Bytecodes that indicate something is invalid. These don't have a direct
// equivalent in RegExpMacroAssembler.
// It's a requirement that BREAK has an enum value of 0 (as e.g. jumps to offset
// 0 are considered invalid).
// Format: V(CamelName, (OperandNames...), (OperandTypes...), (Flags...))
#define INVALID_BYTECODE_LIST(V) V(Break, (), (), ())

// Format: V(CamelName, (OperandNames...), (OperandTypes...), (Flags...))
#define BASIC_BYTECODE_LIST(V)                                                 \
  V(PushCurrentPosition, (), (), ())                                           \
  V(PushBacktrack, (label), (ReBcOpType::kJumpTarget),                         \
    (ReBcFlag::kNoBranchDespiteJumpTargetOperand))                             \
  V(WriteCurrentPositionToRegister, (register_index, cp_offset),               \
    (ReBcOpType::kRegister, ReBcOpType::kOffset), ())                          \
  V(ReadCurrentPositionFromRegister, (register_index),                         \
    (ReBcOpType::kRegister), ())                                               \
  V(WriteStackPointerToRegister, (register_index), (ReBcOpType::kRegister),    \
    ())                                                                        \
  V(ReadStackPointerFromRegister, (register_index), (ReBcOpType::kRegister),   \
    ())                                                                        \
  V(SetRegister, (register_index, value),                                      \
    (ReBcOpType::kRegister, ReBcOpType::kInt32), ())                           \
  /* Clear registers in the range from_register to to_register (inclusive) */  \
  V(ClearRegisters, (from_register, to_register),                              \
    (ReBcOpType::kRegister, ReBcOpType::kRegister), ())                        \
  V(AdvanceRegister, (register_index, by),                                     \
    (ReBcOpType::kRegister, ReBcOpType::kOffset), ())                          \
  V(PopCurrentPosition, (), (), ())                                            \
  /* TODO(pthier): PushRegister fits into 4 byte once the restrictions due */  \
  /* to the old layout are lifted                                          */  \
  V(PushRegister, (register_index, stack_check),                               \
    (ReBcOpType::kRegister, ReBcOpType::kStackCheckFlag), ())                  \
  V(PopRegister, (register_index), (ReBcOpType::kRegister), ())                \
  V(Fail, (), (), (ReBcFlag::kNoFallthrough))                                  \
  V(Succeed, (), (), (ReBcFlag::kNoFallthrough))                               \
  V(AdvanceCurrentPosition, (by), (ReBcOpType::kOffset), ())                   \
  /* Jump to another bytecode given its offset.                             */ \
  V(GoTo, (label), (ReBcOpType::kJumpTarget), (ReBcFlag::kNoFallthrough))      \
  /* Check if offset is in range and load character at given offset.        */ \
  V(LoadCurrentCharacter, (cp_offset, on_failure),                             \
    (ReBcOpType::kOffset, ReBcOpType::kJumpTarget), (ReBcFlag::kLoadsCC))      \
  /* Checks if current position + given offset is in range.                 */ \
  /* I.e. jumps to |on_failure| if current pos + |cp_offset| >= subject len */ \
  V(CheckPosition, (cp_offset, on_failure),                                    \
    (ReBcOpType::kOffset, ReBcOpType::kJumpTarget), ())                        \
  V(CheckSpecialClassRanges, (character_set, on_no_match),                     \
    (ReBcOpType::kStandardCharacterSet, ReBcOpType::kJumpTarget),              \
    (ReBcFlag::kUsesCC))                                                       \
  /* Check if current character is equal to a given character               */ \
  V(CheckCharacter, (character, on_equal),                                     \
    (ReBcOpType::kChar, ReBcOpType::kJumpTarget), (ReBcFlag::kUsesCC))         \
  V(CheckNotCharacter, (character, on_not_equal),                              \
    (ReBcOpType::kChar, ReBcOpType::kJumpTarget), (ReBcFlag::kUsesCC))         \
  /* Checks if the current character combined with mask (bitwise and)       */ \
  /* matches a character (e.g. used when two characters in a disjunction    */ \
  /* differ by only a single bit                                            */ \
  /* TODO(pthier): mask should be kChar */                                     \
  V(CheckCharacterAfterAnd, (character, mask, on_equal),                       \
    (ReBcOpType::kChar, ReBcOpType::kUint32, ReBcOpType::kJumpTarget),         \
    (ReBcFlag::kUsesCC))                                                       \
  /* TODO(pthier): mask should be kChar */                                     \
  V(CheckNotCharacterAfterAnd, (character, mask, on_not_equal),                \
    (ReBcOpType::kChar, ReBcOpType::kUint32, ReBcOpType::kJumpTarget),         \
    (ReBcFlag::kUsesCC))                                                       \
  V(CheckNotCharacterAfterMinusAnd, (character, minus, mask, on_not_equal),    \
    (ReBcOpType::kChar, ReBcOpType::kChar, ReBcOpType::kChar,                  \
     ReBcOpType::kJumpTarget),                                                 \
    (ReBcFlag::kUsesCC))                                                       \
  V(CheckCharacterInRange, (from, to, on_in_range),                            \
    (ReBcOpType::kChar, ReBcOpType::kChar, ReBcOpType::kJumpTarget),           \
    (ReBcFlag::kUsesCC))                                                       \
  V(CheckCharacterNotInRange, (from, to, on_not_in_range),                     \
    (ReBcOpType::kChar, ReBcOpType::kChar, ReBcOpType::kJumpTarget),           \
    (ReBcFlag::kUsesCC))                                                       \
  V(CheckCharacterLT, (limit, on_less),                                        \
    (ReBcOpType::kChar, ReBcOpType::kJumpTarget), (ReBcFlag::kUsesCC))         \
  V(CheckCharacterGT, (limit, on_greater),                                     \
    (ReBcOpType::kChar, ReBcOpType::kJumpTarget), (ReBcFlag::kUsesCC))         \
  V(IfRegisterLT, (register_index, comparand, on_less_than),                   \
    (ReBcOpType::kRegister, ReBcOpType::kInt32, ReBcOpType::kJumpTarget), ())  \
  V(IfRegisterGE, (register_index, comparand, on_greater_or_equal),            \
    (ReBcOpType::kRegister, ReBcOpType::kInt32, ReBcOpType::kJumpTarget), ())  \
  V(IfRegisterEqPos, (register_index, on_eq),                                  \
    (ReBcOpType::kRegister, ReBcOpType::kJumpTarget), ())                      \
  V(CheckAtStart, (cp_offset, on_at_start),                                    \
    (ReBcOpType::kOffset, ReBcOpType::kJumpTarget), ())                        \
  V(CheckNotAtStart, (cp_offset, on_not_at_start),                             \
    (ReBcOpType::kOffset, ReBcOpType::kJumpTarget), ())                        \
  /* Checks if the current position matches top of backtrack stack          */ \
  V(CheckFixedLengthLoop, (on_tos_equals_current_position),                    \
    (ReBcOpType::kJumpTarget), ())                                             \
  /* Advance character pointer by given offset and jump to another bytecode.*/ \
  V(SetCurrentPositionFromEnd, (by), (ReBcOpType::kOffset), ())

// Bytecodes dealing with multiple characters, introduced due to special logic
// in the bytecode-generator or requiring additional logic when assembling;
// e.g. they have arguments only used in the interpreter, different
// MacroAssembler names, non-default MacroAssembler arguments that need to be
// provided, etc.
// These share a method with Basic Bytecodes in RegExpMacroAssembler.
// Format: V(CamelName, (OperandNames...), (OperandTypes...), (Flags...))
#define SPECIAL_BYTECODE_LIST(V)                                               \
  V(Backtrack, (return_code), (ReBcOpType::kInt16),                            \
    (ReBcFlag::kNoFallthrough))                                                \
  /* Load character at given offset without range checks.                   */ \
  V(LoadCurrentCharacterUnchecked, (cp_offset), (ReBcOpType::kOffset),         \
    (ReBcFlag::kLoadsCC))                                                      \
  /* Checks if the current character matches any of the characters encoded  */ \
  /* in a bit table. Similar to/inspired by boyer moore string search       */ \
  /* Todo(pthier): Change order to (table, label) and move to Basic */         \
  V(CheckBitInTable, (on_bit_set, table),                                      \
    (ReBcOpType::kJumpTarget, ReBcOpType::kBitTable), (ReBcFlag::kUsesCC))     \
  V(Load2CurrentChars, (cp_offset, on_failure),                                \
    (ReBcOpType::kOffset, ReBcOpType::kJumpTarget), (ReBcFlag::kLoadsCC))      \
  V(Load2CurrentCharsUnchecked, (cp_offset), (ReBcOpType::kOffset),            \
    (ReBcFlag::kLoadsCC))                                                      \
  V(Load4CurrentChars, (cp_offset, on_failure),                                \
    (ReBcOpType::kOffset, ReBcOpType::kJumpTarget), (ReBcFlag::kLoadsCC))      \
  V(Load4CurrentCharsUnchecked, (cp_offset), (ReBcOpType::kOffset),            \
    (ReBcFlag::kLoadsCC))                                                      \
  V(Check4Chars, (characters, on_equal),                                       \
    (ReBcOpType::kUint32, ReBcOpType::kJumpTarget), (ReBcFlag::kUsesCC))       \
  V(CheckNot4Chars, (characters, on_not_equal),                                \
    (ReBcOpType::kUint32, ReBcOpType::kJumpTarget), (ReBcFlag::kUsesCC))       \
  V(AndCheck4Chars, (characters, mask, on_equal),                              \
    (ReBcOpType::kUint32, ReBcOpType::kUint32, ReBcOpType::kJumpTarget),       \
    (ReBcFlag::kUsesCC))                                                       \
  V(AndCheckNot4Chars, (characters, mask, on_not_equal),                       \
    (ReBcOpType::kUint32, ReBcOpType::kUint32, ReBcOpType::kJumpTarget),       \
    (ReBcFlag::kUsesCC))                                                       \
  V(AdvanceCpAndGoto, (by, on_goto),                                           \
    (ReBcOpType::kOffset, ReBcOpType::kJumpTarget),                            \
    (ReBcFlag::kNoFallthrough))                                                \
  /* TODO(pthier): CheckNotBackRef variants could be merged into a single */   \
  /* Bytecode without increasing the size */                                   \
  V(CheckNotBackRef, (start_reg, on_not_equal),                                \
    (ReBcOpType::kRegister, ReBcOpType::kJumpTarget), ())                      \
  V(CheckNotBackRefNoCase, (start_reg, on_not_equal),                          \
    (ReBcOpType::kRegister, ReBcOpType::kJumpTarget), ())                      \
  V(CheckNotBackRefNoCaseUnicode, (start_reg, on_not_equal),                   \
    (ReBcOpType::kRegister, ReBcOpType::kJumpTarget), ())                      \
  V(CheckNotBackRefBackward, (start_reg, on_not_equal),                        \
    (ReBcOpType::kRegister, ReBcOpType::kJumpTarget), ())                      \
  V(CheckNotBackRefNoCaseBackward, (start_reg, on_not_equal),                  \
    (ReBcOpType::kRegister, ReBcOpType::kJumpTarget), ())                      \
  V(CheckNotBackRefNoCaseUnicodeBackward, (start_reg, on_not_equal),           \
    (ReBcOpType::kRegister, ReBcOpType::kJumpTarget), ())

// Bytecodes generated by peephole optimization. These don't have a direct
// equivalent in the RegExpMacroAssembler.
// All peephole generated bytecodes should have a default implementation in
// RegExpMacroAssembler, that maps the optimized sequence back to the basic
// sequence they were created from.
// Format: V(CamelName, (OperandNames...), (OperandTypes...), (Flags...))
#define PEEPHOLE_BYTECODE_LIST(V)                                              \
  /* Combination of:                                                        */ \
  /* LoadCurrentCharacter, CheckBitInTable and AdvanceCpAndGoto             */ \
  V(SkipUntilBitInTable,                                                       \
    (cp_offset, advance_by, table, on_match, on_no_match),                     \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kBitTable,          \
     ReBcOpType::kJumpTarget, ReBcOpType::kJumpTarget),                        \
    (ReBcFlag::kLoadsCC))                                                      \
  /* Combination of:                                                        */ \
  /* CheckPosition, LoadCurrentCharacterUnchecked, CheckCharacterAfterAnd   */ \
  /* and AdvanceCpAndGoto                                                   */ \
  /* TODO(pthier): mask should be kChar */                                     \
  /* TODO(pthier): eats_at_least should be Offset                           */ \
  V(SkipUntilCharAnd,                                                          \
    (cp_offset, advance_by, character, mask, eats_at_least, on_match,          \
     on_no_match),                                                             \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kUint32, ReBcOpType::kOffset, ReBcOpType::kJumpTarget,        \
     ReBcOpType::kJumpTarget),                                                 \
    (ReBcFlag::kLoadsCC))                                                      \
  /* Combination of:                                                        */ \
  /* LoadCurrentCharacter, CheckCharacter and AdvanceCpAndGoto */              \
  V(SkipUntilChar, (cp_offset, advance_by, character, on_match, on_no_match),  \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kJumpTarget, ReBcOpType::kJumpTarget),                        \
    (ReBcFlag::kLoadsCC))                                                      \
  /* Combination of:                                                        */ \
  /* CheckPosition, LoadCurrentCharacterUnchecked, CheckCharacter           */ \
  /* and AdvanceCpAndGoto                                                   */ \
  V(SkipUntilCharPosChecked,                                                   \
    (cp_offset, advance_by, character, eats_at_least, on_match, on_no_match),  \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kOffset, ReBcOpType::kJumpTarget, ReBcOpType::kJumpTarget),   \
    (ReBcFlag::kLoadsCC))                                                      \
  /* TODO(pthier): eats_at_least should be Offset instead of Uint32         */ \
  /* Combination of:                                                        */ \
  /* LoadCurrentCharacter, CheckCharacter, CheckCharacter and               */ \
  /* AdvanceCpAndGoto                                                       */ \
  V(SkipUntilCharOrChar,                                                       \
    (cp_offset, advance_by, char1, char2, on_match, on_no_match),              \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kChar, ReBcOpType::kJumpTarget, ReBcOpType::kJumpTarget),     \
    (ReBcFlag::kLoadsCC))                                                      \
  /* Combination of:                                                        */ \
  /* LoadCurrentCharacter, CheckCharacterGT, CheckBitInTable, GoTo and      */ \
  /* AdvanceCpAndGoto                                                       */ \
  V(SkipUntilGtOrNotBitInTable,                                                \
    (cp_offset, advance_by, character, table, on_match, on_no_match),          \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kBitTable, ReBcOpType::kJumpTarget, ReBcOpType::kJumpTarget), \
    (ReBcFlag::kLoadsCC))                                                      \
  /* Combination of:                                                        */ \
  /* CheckPosition, Load4CurrentCharsUnchecked, AndCheck4Chars,             */ \
  /* AdvanceCpAndGoto, AndCheck4Chars, AndCheckNot4Chars                    */ \
  /* This pattern is common for finding a match from an alternative with    */ \
  /* few different characters. E.g. /[ab]bbbc|[de]eeef/.                    */ \
  V(SkipUntilOneOfMasked,                                                      \
    (cp_offset, advance_by, both_chars, both_mask, max_offset, chars1, mask1,  \
     chars2, mask2, on_match1, on_match2, on_failure),                         \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kUint32,            \
     ReBcOpType::kUint32, ReBcOpType::kOffset, ReBcOpType::kUint32,            \
     ReBcOpType::kUint32, ReBcOpType::kUint32, ReBcOpType::kUint32,            \
     ReBcOpType::kJumpTarget, ReBcOpType::kJumpTarget,                         \
     ReBcOpType::kJumpTarget),                                                 \
    (ReBcFlag::kLoadsCC))                                                      \
  /* Combination of:                                                        */ \
  /* SkipUntilBitInTable, CheckPosition, LoadCurrentCharacter,              */ \
  /* CheckCharacterAfterAnd, AdvanceCurrentPosition, LoadCurrentCharacter,  */ \
  /* CheckCharacterAfterAnd, CheckCharacterAfterAnd,                        */ \
  /* CheckNotCharacterAfterAnd.                                             */ \
  /* This pattern is common for finding a match from an alternative, e.g.:  */ \
  /* /<script|<style|<link/i.                                               */ \
  V(SkipUntilOneOfMasked3,                                                     \
    (bc0_cp_offset, bc0_advance_by, bc0_table, bc1_cp_offset, bc1_on_failure,  \
     bc2_cp_offset, bc3_characters, bc3_mask, bc4_by, bc5_cp_offset,           \
     bc6_characters, bc6_mask, bc6_on_equal, bc7_characters, bc7_mask,         \
     bc7_on_equal, bc8_characters, bc8_mask, fallthrough_jump_target),         \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kBitTable,          \
     ReBcOpType::kOffset, ReBcOpType::kJumpTarget, ReBcOpType::kOffset,        \
     ReBcOpType::kUint32, ReBcOpType::kUint32, ReBcOpType::kOffset,            \
     ReBcOpType::kOffset, ReBcOpType::kUint32, ReBcOpType::kUint32,            \
     ReBcOpType::kJumpTarget, ReBcOpType::kUint32, ReBcOpType::kUint32,        \
     ReBcOpType::kJumpTarget, ReBcOpType::kUint32, ReBcOpType::kUint32,        \
     ReBcOpType::kJumpTarget),                                                 \
    (ReBcFlag::kLoadsCC))

#define REGEXP_BYTECODE_LIST(V) \
  INVALID_BYTECODE_LIST(V)      \
  BASIC_BYTECODE_LIST(V)        \
  SPECIAL_BYTECODE_LIST(V)      \
  PEEPHOLE_BYTECODE_LIST(V)

enum class RegExpBytecode : uint8_t {
#define DECLARE_BYTECODE(CamelName, ...) k##CamelName,
  REGEXP_BYTECODE_LIST(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
#define COUNT_BYTECODE(x, ...) +1
  // The COUNT_BYTECODE macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last real bytecode.
  kLast = -1 REGEXP_BYTECODE_LIST(COUNT_BYTECODE)
};

// Bytecode is 4-byte aligned.
// We can pack operands if multiple operands fit into 4 bytes.
static constexpr int kBytecodeAlignment = 4;

template <RegExpBytecode bc>
class RegExpBytecodeOperands;

class RegExpBytecodes final : public AllStatic {
 public:
  static constexpr int kCount = static_cast<uint8_t>(RegExpBytecode::kLast) + 1;
  static constexpr uint8_t ToByte(RegExpBytecode bc) {
    return static_cast<uint8_t>(bc);
  }
  static constexpr RegExpBytecode FromByte(uint8_t byte) {
    DCHECK(IsValid(byte));
    return static_cast<RegExpBytecode>(byte);
  }
  // Extract the bytecode from the given `ptr`, which must point at the
  // word32-aligned region containing the bytecode. Endian-ness independent.
  static constexpr RegExpBytecode FromPtr(const void* ptr) {
    if (!std::is_constant_evaluated()) {
      DCHECK(IsAligned(reinterpret_cast<Address>(ptr), kBytecodeAlignment));
    }
    return FromByte(*static_cast<const uint8_t*>(ptr));
  }
  static constexpr bool IsValid(uint8_t byte) { return byte < kCount; }
  static constexpr bool IsValidJumpTarget(uint8_t byte) {
    return IsValid(byte) && FromByte(byte) != RegExpBytecode::kBreak;
  }

  // Calls |f| templatized by RegExpBytecode. This allows the usage of the
  // functions template argument in other templates.
  // Example:
  // RegExpBytecode bc = <runtime value>;
  // DispatchOnBytecode(bc, []<RegExpBytecode bc>() { DoFancyStuff<bc>(); });
  template <typename Func>
  static decltype(auto) DispatchOnBytecode(RegExpBytecode bytecode, Func&& f);

  static constexpr const char* Name(RegExpBytecode bytecode);
  static constexpr const char* Name(uint8_t bytecode);

  static constexpr uint8_t Size(RegExpBytecode bytecode);
  static constexpr uint8_t Size(uint8_t bytecode);
  static constexpr uint8_t Size(RegExpBytecodeOperandType type);

  static constexpr RegExpBytecodeFlags Flags(RegExpBytecode bytecode);
  static constexpr RegExpBytecodeFlags Flags(uint8_t bytecode);
};

class RegExpBytecodeAnalysis;

void RegExpBytecodeDisassembleSingle(const uint8_t* code_base,
                                     const uint8_t* pc);
void RegExpBytecodeDisassemble(const uint8_t* code_base, uint32_t length,
                               const char* pattern);
void RegExpBytecodeDisassemble(const uint8_t* code_base, uint32_t length,
                               const char* pattern,
                               RegExpBytecodeAnalysis* analysis);

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODES_H_

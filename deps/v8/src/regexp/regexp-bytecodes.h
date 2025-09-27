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

// Maximum number of bytecodes that will be used (next power of 2 of actually
// defined bytecodes).
// All slots between the last actually defined bytecode and maximum id will be
// filled with BREAKs, indicating an invalid operation. This way using
// BYTECODE_MASK guarantees no OOB access to the dispatch table.
constexpr int kRegExpPaddedBytecodeCount = 1 << 6;
constexpr int BYTECODE_MASK = kRegExpPaddedBytecodeCount - 1;
// The first argument is packed in with the byte code in one word, but so it
// has 24 bits, but it can be positive and negative so only use 23 bits for
// positive values.
const unsigned int MAX_FIRST_ARG = 0x7fffffu;
const int BYTECODE_SHIFT = 8;
static_assert(1 << BYTECODE_SHIFT > BYTECODE_MASK);

// Basic operand types that have a direct mapping to a C-type.
// Getters/Setters for these are fully auto-generated.
// Format: V(Name, C type)
#define BASIC_BYTECODE_OPERAND_TYPE_LIST(V)                 \
  V(Int16, int16_t)                                         \
  V(Int32, int32_t)                                         \
  V(Uint32, uint32_t)                                       \
  V(Char, base::uc16)                                       \
  /* TODO(pthier): Consider renaming Label to JumpTarget */ \
  V(Label, uint32_t)                                        \
  V(Offset, int16_t)                                        \
  V(Register, uint16_t)                                     \
  V(StackCheckFlag, RegExpMacroAssembler::StackCheckFlag)

// Special operand types that don't have a direct mapping to a C-type.
// Getters/Setters for these types need to be specialized manually.
#define SPECIAL_BYTECODE_OPERAND_TYPE_LIST(V)                              \
  V(BitTable, 16)                                                          \
  /* TODO(pthier): padding is only required for backwards compatibility    \
  with the old layout. It can be removed after everything is using the new \
  layout. */                                                               \
  V(Padding1, 1)                                                           \
  V(Padding2, 2)

#define BYTECODE_OPERAND_TYPE_LIST(V) \
  BASIC_BYTECODE_OPERAND_TYPE_LIST(V) \
  SPECIAL_BYTECODE_OPERAND_TYPE_LIST(V)

enum class RegExpBytecodeOperandType : uint8_t {
#define DECLARE_OPERAND(Name, ...) k##Name,
  BYTECODE_OPERAND_TYPE_LIST(DECLARE_OPERAND)
#undef DECLARE_OPERAND
};

using ReBcOpType = RegExpBytecodeOperandType;

// Bytecodes that indicate something is invalid. These don't have a direct
// equivalent in RegExpMacroAssembler.
// It's a requirement that BREAK has an enum value of 0 (as e.g. jumps to offset
// 0 are considered invalid).
// Format: V(CamelName, SNAKE_NAME, (OperandNames...), // (OperandTypes...))
// TODO(pthier): SNAKE_NAME is temporary to static_assert that the new bytecode
// enum and bytecode layouts are compatible with the old one. Remove once all
// uses have been migrated.
#define INVALID_BYTECODE_LIST(V) V(Break, BREAK, (), ())

// Basic Bytecodes. These have a direct equivalent in the RegExpMacroAssembler.
// Format: V(CamelName, SNAKE_NAME, (OperandNames...), (OperandTypes...))
// TODO(pthier): SNAKE_NAME is temporary to static_assert that the new bytecode
// enum and bytecode layouts are compatible with the old one. Remove once all
// uses have been migrated.
#define BASIC_BYTECODE_LIST(V)                                                 \
  V(PushCurrentPosition, PUSH_CP, (), ())                                      \
  V(PushBacktrack, PUSH_BT, (on_bt_pushed), (ReBcOpType::kLabel))              \
  V(WriteCurrentPositionToRegister, SET_REGISTER_TO_CP,                        \
    (register_index, cp_offset), (ReBcOpType::kRegister, ReBcOpType::kOffset)) \
  V(ReadCurrentPositionFromRegister, SET_CP_TO_REGISTER, (register_index),     \
    (ReBcOpType::kRegister))                                                   \
  V(WriteStackPointerToRegister, SET_REGISTER_TO_SP, (register_index),         \
    (ReBcOpType::kRegister))                                                   \
  V(ReadStackPointerFromRegister, SET_SP_TO_REGISTER, (register_index),        \
    (ReBcOpType::kRegister))                                                   \
  V(SetRegister, SET_REGISTER, (register_index, value),                        \
    (ReBcOpType::kRegister, ReBcOpType::kInt32))                               \
  /* Clear registers in the range from_register to to_register (inclusive) */  \
  V(ClearRegisters, CLEAR_REGISTERS, (padding, from_register, to_register),    \
    (ReBcOpType::kPadding2, ReBcOpType::kRegister, ReBcOpType::kRegister))     \
  V(AdvanceRegister, ADVANCE_REGISTER, (register_index, by),                   \
    (ReBcOpType::kRegister, ReBcOpType::kOffset))                              \
  V(PopCurrentPosition, POP_CP, (), ())                                        \
  /* TODO(pthier): PushRegister fits into 4 byte once the restrictions due */  \
  /* to the old layout are lifted                                          */  \
  V(PushRegister, PUSH_REGISTER, (register_index, padding, stack_check),       \
    (ReBcOpType::kRegister, ReBcOpType::kPadding1,                             \
     ReBcOpType::kStackCheckFlag))                                             \
  V(PopRegister, POP_REGISTER, (register_index), (ReBcOpType::kRegister))      \
  V(Fail, FAIL, (), ())                                                        \
  V(Succeed, SUCCEED, (), ())                                                  \
  V(AdvanceCurrentPosition, ADVANCE_CP, (by), (ReBcOpType::kOffset))           \
  /* Jump to another bytecode given its offset.                             */ \
  V(GoTo, GOTO, (label), (ReBcOpType::kLabel))                                 \
  /* Check if offset is in range and load character at given offset.        */ \
  V(LoadCurrentCharacter, LOAD_CURRENT_CHAR, (cp_offset, on_failure),          \
    (ReBcOpType::kOffset, ReBcOpType::kLabel))                                 \
  /* Checks if current position + given offset is in range.                 */ \
  /* I.e. jumps to |on_failure| if current pos + |cp_offset| >= subject len */ \
  V(CheckPosition, CHECK_CURRENT_POSITION, (cp_offset, on_failure),            \
    (ReBcOpType::kOffset, ReBcOpType::kLabel))                                 \
  /* Check if current character is equal to a given character               */ \
  V(CheckCharacter, CHECK_CHAR, (character, on_equal),                         \
    (ReBcOpType::kChar, ReBcOpType::kLabel))                                   \
  V(CheckNotCharacter, CHECK_NOT_CHAR, (character, on_not_equal),              \
    (ReBcOpType::kChar, ReBcOpType::kLabel))                                   \
  /* Checks if the current character combined with mask (bitwise and)       */ \
  /* matches a character (e.g. used when two characters in a disjunction    */ \
  /* differ by only a single bit                                            */ \
  V(CheckCharacterAfterAnd, AND_CHECK_CHAR, (character, mask, on_equal),       \
    (ReBcOpType::kChar, ReBcOpType::kChar, ReBcOpType::kLabel))                \
  V(CheckNotCharacterAfterAnd, AND_CHECK_NOT_CHAR,                             \
    (character, mask, on_not_equal),                                           \
    (ReBcOpType::kChar, ReBcOpType::kChar, ReBcOpType::kLabel))                \
  V(CheckNotCharacterAfterMinusAnd, MINUS_AND_CHECK_NOT_CHAR,                  \
    (character, minus, mask, on_not_equal),                                    \
    (ReBcOpType::kChar, ReBcOpType::kChar, ReBcOpType::kChar,                  \
     ReBcOpType::kLabel))                                                      \
  V(CheckCharacterInRange, CHECK_CHAR_IN_RANGE,                                \
    (padding, from, to, on_in_range),                                          \
    (ReBcOpType::kPadding2, ReBcOpType::kChar, ReBcOpType::kChar,              \
     ReBcOpType::kLabel))                                                      \
  V(CheckCharacterNotInRange, CHECK_CHAR_NOT_IN_RANGE,                         \
    (padding, from, to, on_not_in_range),                                      \
    (ReBcOpType::kPadding2, ReBcOpType::kChar, ReBcOpType::kChar,              \
     ReBcOpType::kLabel))                                                      \
  V(CheckCharacterLT, CHECK_LT, (limit, on_less),                              \
    (ReBcOpType::kChar, ReBcOpType::kLabel))                                   \
  V(CheckCharacterGT, CHECK_GT, (limit, on_greater),                           \
    (ReBcOpType::kChar, ReBcOpType::kLabel))                                   \
  V(IfRegisterLT, CHECK_REGISTER_LT,                                           \
    (register_index, comparand, on_less_than),                                 \
    (ReBcOpType::kRegister, ReBcOpType::kInt32, ReBcOpType::kLabel))           \
  V(IfRegisterGE, CHECK_REGISTER_GE,                                           \
    (register_index, comparand, on_greater_or_equal),                          \
    (ReBcOpType::kRegister, ReBcOpType::kInt32, ReBcOpType::kLabel))           \
  V(IfRegisterEqPos, CHECK_REGISTER_EQ_POS, (register_index, on_eq),           \
    (ReBcOpType::kRegister, ReBcOpType::kLabel))                               \
  V(CheckAtStart, CHECK_AT_START, (cp_offset, on_at_start),                    \
    (ReBcOpType::kOffset, ReBcOpType::kLabel))                                 \
  V(CheckNotAtStart, CHECK_NOT_AT_START, (cp_offset, on_not_at_start),         \
    (ReBcOpType::kOffset, ReBcOpType::kLabel))                                 \
  /* Checks if the current position matches top of backtrack stack          */ \
  V(CheckFixedLengthLoop, CHECK_FIXED_LENGTH,                                  \
    (on_tos_equals_current_position), (ReBcOpType::kLabel))                    \
  /* Advance character pointer by given offset and jump to another bytecode.*/ \
  V(SetCurrentPositionFromEnd, SET_CURRENT_POSITION_FROM_END, (by),            \
    (ReBcOpType::kOffset))

// Bytecodes dealing with multiple characters, introduced due to special logic
// in the bytecode-generator or requiring additional logic when assembling;
// e.g. they have arguments only used in the interpreter, different
// MacroAssembler names, non-default MacroAssembler arguments that need to be
// provided, etc.
// These share a method with Basic Bytecodes in RegExpMacroAssembler.
// Format: V(CamelName, SNAKE_NAME, (OperandNames...), (OperandTypes...))
// TODO(pthier): SNAKE_NAME is temporary to static_assert that the new bytecode
// enum and bytecode layouts are compatible with the old one. Remove once all
// uses have been migrated.
#define SPECIAL_BYTECODE_LIST(V)                                               \
  V(Backtrack, POP_BT, (return_code), (ReBcOpType::kInt16))                    \
  /* Load character at given offset without range checks.                   */ \
  V(LoadCurrentCharacterUnchecked, LOAD_CURRENT_CHAR_UNCHECKED, (cp_offset),   \
    (ReBcOpType::kOffset))                                                     \
  /* Checks if the current character matches any of the characters encoded  */ \
  /* in a bit table. Similar to/inspired by boyer moore string search       */ \
  /* Todo(pthier): Change order to (table, label) and move to Basic */         \
  V(CheckBitInTable, CHECK_BIT_IN_TABLE, (on_bit_set, table),                  \
    (ReBcOpType::kLabel, ReBcOpType::kBitTable))                               \
  V(Load2CurrentChars, LOAD_2_CURRENT_CHARS, (cp_offset, on_failure),          \
    (ReBcOpType::kOffset, ReBcOpType::kLabel))                                 \
  V(Load2CurrentCharsUnchecked, LOAD_2_CURRENT_CHARS_UNCHECKED, (cp_offset),   \
    (ReBcOpType::kOffset))                                                     \
  V(Load4CurrentChars, LOAD_4_CURRENT_CHARS, (cp_offset, on_failure),          \
    (ReBcOpType::kOffset, ReBcOpType::kLabel))                                 \
  V(Load4CurrentCharsUnchecked, LOAD_4_CURRENT_CHARS_UNCHECKED, (cp_offset),   \
    (ReBcOpType::kOffset))                                                     \
  V(Check4Chars, CHECK_4_CHARS, (characters, on_equal),                        \
    (ReBcOpType::kUint32, ReBcOpType::kLabel))                                 \
  V(CheckNot4Chars, CHECK_NOT_4_CHARS, (characters, on_not_equal),             \
    (ReBcOpType::kUint32, ReBcOpType::kLabel))                                 \
  V(AndCheck4Chars, AND_CHECK_4_CHARS, (characters, mask, on_equal),           \
    (ReBcOpType::kUint32, ReBcOpType::kUint32, ReBcOpType::kLabel))            \
  V(AndCheckNot4Chars, AND_CHECK_NOT_4_CHARS,                                  \
    (characters, mask, on_not_equal),                                          \
    (ReBcOpType::kUint32, ReBcOpType::kUint32, ReBcOpType::kLabel))            \
  V(AdvanceCpAndGoto, ADVANCE_CP_AND_GOTO, (by, on_goto),                      \
    (ReBcOpType::kOffset, ReBcOpType::kLabel))                                 \
  /* TODO(pthier): CheckNotBackRef variants could be merged into a single */   \
  /* Bytecode without increasing the size */                                   \
  V(CheckNotBackRef, CHECK_NOT_BACK_REF, (start_reg, on_not_equal),            \
    (ReBcOpType::kRegister, ReBcOpType::kLabel))                               \
  V(CheckNotBackRefNoCase, CHECK_NOT_BACK_REF_NO_CASE,                         \
    (start_reg, on_not_equal), (ReBcOpType::kRegister, ReBcOpType::kLabel))    \
  V(CheckNotBackRefNoCaseUnicode, CHECK_NOT_BACK_REF_NO_CASE_UNICODE,          \
    (start_reg, on_not_equal), (ReBcOpType::kRegister, ReBcOpType::kLabel))    \
  V(CheckNotBackRefBackward, CHECK_NOT_BACK_REF_BACKWARD,                      \
    (start_reg, on_not_equal), (ReBcOpType::kRegister, ReBcOpType::kLabel))    \
  V(CheckNotBackRefNoCaseBackward, CHECK_NOT_BACK_REF_NO_CASE_BACKWARD,        \
    (start_reg, on_not_equal), (ReBcOpType::kRegister, ReBcOpType::kLabel))    \
  V(CheckNotBackRefNoCaseUnicodeBackward,                                      \
    CHECK_NOT_BACK_REF_NO_CASE_UNICODE_BACKWARD, (start_reg, on_not_equal),    \
    (ReBcOpType::kRegister, ReBcOpType::kLabel))                               \
  V(CheckNotRegsEqual, CHECK_NOT_REGS_EQUAL, (reg1, reg2, on_not_equal),       \
    (ReBcOpType::kRegister, ReBcOpType::kRegister,                             \
     ReBcOpType::kLabel)) /* TODO(pthier): This Bytecode is unused. */

// Bytecodes generated by peephole optimization. These don't have a direct
// equivalent in the RegExpMacroAssembler.
// Format: V(CamelName, SNAKE_NAME, // (OperandNames...), (OperandTypes...))
// TODO(pthier): SNAKE_NAME is temporary to static_assert that the new bytecode
// enum and bytecode layouts are compatible with the old one. Remove once all
// uses have been migrated.
#define PEEPHOLE_BYTECODE_LIST(V)                                              \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_BIT_IN_TABLE and ADVANCE_CP_AND_GOTO          */ \
  V(SkipUntilBitInTable, SKIP_UNTIL_BIT_IN_TABLE,                              \
    (cp_offset, advance_by, table, on_match, on_no_match),                     \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kBitTable,          \
     ReBcOpType::kLabel, ReBcOpType::kLabel))                                  \
  /* Combination of:                                                        */ \
  /* CHECK_CURRENT_POSITION, LOAD_CURRENT_CHAR_UNCHECKED, AND_CHECK_CHAR    */ \
  /* and ADVANCE_CP_AND_GOTO                                                */ \
  V(SkipUntilCharAnd, SKIP_UNTIL_CHAR_AND,                                     \
    (cp_offset, advance_by, character, mask, eats_at_least, on_match,          \
     on_no_match),                                                             \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kChar, ReBcOpType::kUint32, ReBcOpType::kLabel,               \
     ReBcOpType::kLabel)) /* TODO(pthier): eats_at_least should be Offset */   \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_CHAR and ADVANCE_CP_AND_GOTO                  */ \
  V(SkipUntilChar, SKIP_UNTIL_CHAR,                                            \
    (cp_offset, advance_by, character, on_match, on_no_match),                 \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kLabel, ReBcOpType::kLabel))                                  \
  /* Combination of:                                                        */ \
  /* CHECK_CURRENT_POSITION, LOAD_CURRENT_CHAR_UNCHECKED, CHECK_CHAR        */ \
  /* and ADVANCE_CP_AND_GOTO                                                */ \
  V(SkipUntilCharPosChecked, SKIP_UNTIL_CHAR_POS_CHECKED,                      \
    (cp_offset, advance_by, character, eats_at_least, on_match, on_no_match),  \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kUint32, ReBcOpType::kLabel, ReBcOpType::kLabel))             \
  /* TODO(pthier): eats_at_least should be Offset instead of Uint32 */         \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_CHAR, CHECK_CHAR and ADVANCE_CP_AND_GOTO      */ \
  V(SkipUntilCharOrChar, SKIP_UNTIL_CHAR_OR_CHAR,                              \
    (cp_offset, advance_by, padding, char1, char2, on_match, on_no_match),     \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kPadding2,          \
     ReBcOpType::kChar, ReBcOpType::kChar, ReBcOpType::kLabel,                 \
     ReBcOpType::kLabel))                                                      \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_GT, CHECK_BIT_IN_TABLE, GOTO and              */ \
  /* and ADVANCE_CP_AND_GOTO                                                */ \
  V(SkipUntilGtOrNotBitInTable, SKIP_UNTIL_GT_OR_NOT_BIT_IN_TABLE,             \
    (cp_offset, advance_by, character, table, on_match, on_no_match),          \
    (ReBcOpType::kOffset, ReBcOpType::kOffset, ReBcOpType::kChar,              \
     ReBcOpType::kBitTable, ReBcOpType::kLabel, ReBcOpType::kLabel))

#define REGEXP_BYTECODE_LIST(V) \
  INVALID_BYTECODE_LIST(V)      \
  BASIC_BYTECODE_LIST(V)        \
  SPECIAL_BYTECODE_LIST(V)      \
  PEEPHOLE_BYTECODE_LIST(V)

// The list of bytecodes, in format: V(Name, Code, ByteLength).
// TODO(pthier): Argument offsets of bytecodes should be easily accessible by
// name or at least by position.
// TODO(jgruber): More precise types (e.g. int32/uint32 instead of value32).
#define BYTECODE_ITERATOR(V)                                                   \
  V(BREAK, 0, 4)              /* bc8                                        */ \
  V(PUSH_CP, 1, 4)            /* bc8 pad24                                  */ \
  V(PUSH_BT, 2, 8)            /* bc8 pad24 offset32                         */ \
  V(SET_REGISTER_TO_CP, 3, 8) /* bc8 reg_idx24 offset32                     */ \
  V(SET_CP_TO_REGISTER, 4, 4) /* bc8 reg_idx24                              */ \
  V(SET_REGISTER_TO_SP, 5, 4) /* bc8 reg_idx24                              */ \
  V(SET_SP_TO_REGISTER, 6, 4) /* bc8 reg_idx24                              */ \
  V(SET_REGISTER, 7, 8)       /* bc8 reg_idx24 value32                      */ \
  V(CLEAR_REGISTERS, 8, 8)    /* bc8 pad24 reg_idx26 reg_idx16              */ \
  V(ADVANCE_REGISTER, 9, 8)   /* bc8 reg_idx24 value32                      */ \
  V(POP_CP, 10, 4)            /* bc8 pad24                                  */ \
  V(PUSH_REGISTER, 11, 8)     /* bc8 reg_idx24 pad16 int16                  */ \
  V(POP_REGISTER, 12, 4)      /* bc8 reg_idx24                              */ \
  V(FAIL, 13, 4)              /* bc8 pad24                                  */ \
  V(SUCCEED, 14, 4)           /* bc8 pad24                                  */ \
  V(ADVANCE_CP, 15, 4)        /* bc8 offset24                               */ \
  /* Jump to another bytecode given its offset.                             */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x10 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   0x00 (unused) Padding                                   */ \
  /* 0x20 - 0x3F:   Address of bytecode to jump to                          */ \
  V(GOTO, 16, 8) /* bc8 pad24 addr32                                        */ \
  /* Check if offset is in range and load character at given offset.        */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x11 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Offset from current position                            */ \
  /* 0x20 - 0x3F:   Address of bytecode when load is out of range           */ \
  V(LOAD_CURRENT_CHAR, 17, 8) /* bc8 offset24 addr32                        */ \
  /* Checks if current position + given offset is in range.                 */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x34 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Offset from current position                            */ \
  /* 0x20 - 0x3F:   Address of bytecode when position is out of range       */ \
  V(CHECK_CURRENT_POSITION, 18, 8) /* bc8 idx24 addr32         */              \
  /* Check if current character is equal to a given character               */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x19 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x0F:   0x00 (unused) Padding                                   */ \
  /* 0x10 - 0x1F:   Character to check                                      */ \
  /* 0x20 - 0x3F:   Address of bytecode when matched                        */ \
  V(CHECK_CHAR, 19, 8)     /* bc8 pad8 uint16 addr32                        */ \
  V(CHECK_NOT_CHAR, 20, 8) /* bc8 pad8 uint16 addr32                        */ \
  /* Checks if the current character combined with mask (bitwise and)       */ \
  /* matches a character (e.g. used when two characters in a disjunction    */ \
  /* differ by only a single bit                                            */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x1c (fixed) Bytecode                                   */ \
  /* 0x08 - 0x0F:   0x00 (unused) Padding                                   */ \
  /* 0x10 - 0x1F:   Character to match against (after mask aplied)          */ \
  /* 0x20 - 0x3F:   Bitmask bitwise and combined with current character     */ \
  /* 0x40 - 0x5F:   Address of bytecode when matched                        */ \
  V(AND_CHECK_CHAR, 21, 12)     /* bc8 pad8 uint16 uint32 addr32            */ \
  V(AND_CHECK_NOT_CHAR, 22, 12) /* bc8 pad8 uint16 uint32 addr32            */ \
  V(MINUS_AND_CHECK_NOT_CHAR, 23,                                              \
    12) /* bc8 pad8 base::uc16 base::uc16 base::uc16 addr32                 */ \
  V(CHECK_CHAR_IN_RANGE, 24, 12) /* bc8 pad24 base::uc16 base::uc16 addr32  */ \
  V(CHECK_CHAR_NOT_IN_RANGE, 25,                                               \
    12) /* bc8 pad24 base::uc16 base::uc16 addr32                           */ \
  V(CHECK_LT, 26, 8)              /* bc8 pad8 base::uc16 addr32             */ \
  V(CHECK_GT, 27, 8)              /* bc8 pad8 base::uc16 addr32             */ \
  V(CHECK_REGISTER_LT, 28, 12)    /* bc8 reg_idx24 value32 addr32           */ \
  V(CHECK_REGISTER_GE, 29, 12)    /* bc8 reg_idx24 value32 addr32           */ \
  V(CHECK_REGISTER_EQ_POS, 30, 8) /* bc8 reg_idx24 addr32                   */ \
  V(CHECK_AT_START, 31, 8)        /* bc8 pad24 addr32                       */ \
  V(CHECK_NOT_AT_START, 32, 8)    /* bc8 offset24 addr32                    */ \
  /* Checks if the current position matches top of backtrack stack          */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x31 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   0x00 (unused) Padding                                   */ \
  /* 0x20 - 0x3F:   Address of bytecode when current matches tos            */ \
  V(CHECK_FIXED_LENGTH, 33, 8) /* bc8 pad24 addr32                          */ \
  /* Advance character pointer by given offset and jump to another bytecode.*/ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x32 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Number of characters to advance                         */ \
  /* 0x20 - 0x3F:   Address of bytecode to jump to                          */ \
  V(SET_CURRENT_POSITION_FROM_END, 34, 4) /* bc8 idx24                      */ \
  V(POP_BT, 35, 4)                        /* bc8 pad24                      */ \
  /* Load character at given offset without range checks.                   */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x12 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Offset from current position                            */ \
  V(LOAD_CURRENT_CHAR_UNCHECKED, 36, 4) /* bc8 offset24                   */   \
  /* Checks if the current character matches any of the characters encoded  */ \
  /* in a bit table. Similar to/inspired by boyer moore string search       */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x22 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   0x00 (unused) Padding                                   */ \
  /* 0x20 - 0x3F:   Address of bytecode when bit is set                     */ \
  /* 0x40 - 0xBF:   Bit table                                               */ \
  V(CHECK_BIT_IN_TABLE, 37, 24)            /* bc8 pad24 addr32 bits128      */ \
  V(LOAD_2_CURRENT_CHARS, 38, 8)           /* bc8 offset24 addr32           */ \
  V(LOAD_2_CURRENT_CHARS_UNCHECKED, 39, 4) /* bc8 offset24                  */ \
  V(LOAD_4_CURRENT_CHARS, 40, 8)           /* bc8 offset24 addr32           */ \
  V(LOAD_4_CURRENT_CHARS_UNCHECKED, 41, 4) /* bc8 offset24                  */ \
  V(CHECK_4_CHARS, 42, 12)                 /* bc8 pad24 uint32 addr32       */ \
  V(CHECK_NOT_4_CHARS, 43, 12)             /* bc8 pad24 uint32 addr32       */ \
  V(AND_CHECK_4_CHARS, 44, 16)             /* bc8 pad24 uint32 uint32 addr32*/ \
  V(AND_CHECK_NOT_4_CHARS, 45, 16)         /* bc8 pad24 uint32 uint32 addr32*/ \
  V(ADVANCE_CP_AND_GOTO, 46, 8)            /* bc8 offset24 addr32           */ \
  V(CHECK_NOT_BACK_REF, 47, 8)             /* bc8 reg_idx24 addr32     */      \
  V(CHECK_NOT_BACK_REF_NO_CASE, 48, 8)     /* bc8 reg_idx24 addr32     */      \
  V(CHECK_NOT_BACK_REF_NO_CASE_UNICODE, 49, 8)                                 \
  V(CHECK_NOT_BACK_REF_BACKWARD, 50, 8)         /* bc8 reg_idx24 addr32     */ \
  V(CHECK_NOT_BACK_REF_NO_CASE_BACKWARD, 51, 8) /* bc8 reg_idx24 addr32     */ \
  V(CHECK_NOT_BACK_REF_NO_CASE_UNICODE_BACKWARD, 52, 8)                        \
  V(CHECK_NOT_REGS_EQUAL, 53, 12) /* bc8 regidx24 reg_idx32 addr32          */ \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_BIT_IN_TABLE and ADVANCE_CP_AND_GOTO          */ \
  /* Emitted by RegExpBytecodePeepholeOptimization.                         */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07    0x35 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F    Load character offset from current position             */ \
  /* 0x20 - 0x3F    Number of characters to advance                         */ \
  /* 0x40 - 0xBF    Bit Table                                               */ \
  /* 0xC0 - 0xDF    Address of bytecode when character is matched           */ \
  /* 0xE0 - 0xFF    Address of bytecode when no match                       */ \
  V(SKIP_UNTIL_BIT_IN_TABLE, 54, 32)                                           \
  /* Combination of:                                                        */ \
  /* CHECK_CURRENT_POSITION, LOAD_CURRENT_CHAR_UNCHECKED, AND_CHECK_CHAR    */ \
  /* and ADVANCE_CP_AND_GOTO                                                */ \
  /* Emitted by RegExpBytecodePeepholeOptimization.                         */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07    0x36 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F    Load character offset from current position             */ \
  /* 0x20 - 0x2F    Number of characters to advance                         */ \
  /* 0x30 - 0x3F    Character to match against (after mask applied)         */ \
  /* 0x40 - 0x5F:   Bitmask bitwise and combined with current character     */ \
  /* 0x60 - 0x7F    Minimum number of characters this pattern consumes      */ \
  /* 0x80 - 0x9F    Address of bytecode when character is matched           */ \
  /* 0xA0 - 0xBF    Address of bytecode when no match                       */ \
  V(SKIP_UNTIL_CHAR_AND, 55, 24)                                               \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_CHAR and ADVANCE_CP_AND_GOTO                  */ \
  /* Emitted by RegExpBytecodePeepholeOptimization.                         */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07    0x37 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F    Load character offset from current position             */ \
  /* 0x20 - 0x2F    Number of characters to advance                         */ \
  /* 0x30 - 0x3F    Character to match                                      */ \
  /* 0x40 - 0x5F    Address of bytecode when character is matched           */ \
  /* 0x60 - 0x7F    Address of bytecode when no match                       */ \
  V(SKIP_UNTIL_CHAR, 56, 16)                                                   \
  /* Combination of:                                                        */ \
  /* CHECK_CURRENT_POSITION, LOAD_CURRENT_CHAR_UNCHECKED, CHECK_CHAR        */ \
  /* and ADVANCE_CP_AND_GOTO                                                */ \
  /* Emitted by RegExpBytecodePeepholeOptimization.                         */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07    0x38 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F    Load character offset from current position             */ \
  /* 0x20 - 0x2F    Number of characters to advance                         */ \
  /* 0x30 - 0x3F    Character to match                                      */ \
  /* 0x40 - 0x5F    Minimum number of characters this pattern consumes      */ \
  /* 0x60 - 0x7F    Address of bytecode when character is matched           */ \
  /* 0x80 - 0x9F    Address of bytecode when no match                       */ \
  V(SKIP_UNTIL_CHAR_POS_CHECKED, 57, 20)                                       \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_CHAR, CHECK_CHAR and ADVANCE_CP_AND_GOTO      */ \
  /* Emitted by RegExpBytecodePeepholeOptimization.                         */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07    0x39 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F    Load character offset from current position             */ \
  /* 0x20 - 0x3F    Number of characters to advance                         */ \
  /* 0x40 - 0x4F    Character to match                                      */ \
  /* 0x50 - 0x5F    Other Character to match                                */ \
  /* 0x60 - 0x7F    Address of bytecode when either character is matched    */ \
  /* 0x80 - 0x9F    Address of bytecode when no match                       */ \
  V(SKIP_UNTIL_CHAR_OR_CHAR, 58, 20)                                           \
  /* Combination of:                                                        */ \
  /* LOAD_CURRENT_CHAR, CHECK_GT, CHECK_BIT_IN_TABLE, GOTO and              */ \
  /* and ADVANCE_CP_AND_GOTO                                                */ \
  /* Emitted by RegExpBytecodePeepholeOptimization.                         */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07    0x3A (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F    Load character offset from current position             */ \
  /* 0x20 - 0x2F    Number of characters to advance                         */ \
  /* 0x30 - 0x3F    Character to check if it is less than current char      */ \
  /* 0x40 - 0xBF    Bit Table                                               */ \
  /* 0xC0 - 0xDF    Address of bytecode when character is matched           */ \
  /* 0xE0 - 0xFF    Address of bytecode when no match                       */ \
  V(SKIP_UNTIL_GT_OR_NOT_BIT_IN_TABLE, 59, 32)

#define COUNT(...) +1
static constexpr int kRegExpBytecodeCount = BYTECODE_ITERATOR(COUNT);
#undef COUNT

enum class RegExpBytecode : uint8_t {
#define DECLARE_BYTECODE(CamelName, ...) k##CamelName,
  REGEXP_BYTECODE_LIST(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
#define COUNT_BYTECODE(x, ...) +1
  // The COUNT_BYTECODE macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last real bytecode.
  kLast = -1 REGEXP_BYTECODE_LIST(COUNT_BYTECODE)
};

template <RegExpBytecode bc>
class RegExpBytecodeOperands;

class RegExpBytecodes final : public AllStatic {
 public:
  static constexpr int kCount = static_cast<uint8_t>(RegExpBytecode::kLast) + 1;
  static constexpr uint8_t ToByte(RegExpBytecode bc) {
    return static_cast<uint8_t>(bc);
  }
  static constexpr RegExpBytecode FromByte(uint8_t byte) {
    DCHECK_LT(byte, kCount);
    return static_cast<RegExpBytecode>(byte);
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
};

// Just making sure we assigned values above properly. They should be
// contiguous, strictly increasing, and start at 0.
// TODO(jgruber): Do not explicitly assign values, instead generate them
// implicitly from the list order.
static_assert(kRegExpBytecodeCount == 60);

#define DECLARE_BYTECODES(name, code, length) \
  static constexpr int BC_##name = code;
BYTECODE_ITERATOR(DECLARE_BYTECODES)
#undef DECLARE_BYTECODES

static constexpr int kRegExpBytecodeLengths[] = {
#define DECLARE_BYTECODE_LENGTH(name, code, length) length,
    BYTECODE_ITERATOR(DECLARE_BYTECODE_LENGTH)
#undef DECLARE_BYTECODE_LENGTH
};

inline constexpr int RegExpBytecodeLength(int bytecode) {
  DCHECK(base::IsInRange(bytecode, 0, kRegExpBytecodeCount - 1));
  return kRegExpBytecodeLengths[bytecode];
}

static constexpr const char* const kRegExpBytecodeNames[] = {
#define DECLARE_BYTECODE_NAME(name, ...) #name,
    BYTECODE_ITERATOR(DECLARE_BYTECODE_NAME)
#undef DECLARE_BYTECODE_NAME
};

inline constexpr const char* RegExpBytecodeName(int bytecode) {
  DCHECK(base::IsInRange(bytecode, 0, kRegExpBytecodeCount - 1));
  return kRegExpBytecodeNames[bytecode];
}

void RegExpBytecodeDisassembleSingle(const uint8_t* code_base,
                                     const uint8_t* pc);
void RegExpBytecodeDisassemble(const uint8_t* code_base, int length,
                               const char* pattern);

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODES_H_

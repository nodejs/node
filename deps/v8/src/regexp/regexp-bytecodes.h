// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODES_H_
#define V8_REGEXP_REGEXP_BYTECODES_H_

#include "src/base/macros.h"
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
STATIC_ASSERT(1 << BYTECODE_SHIFT > BYTECODE_MASK);

// TODO(pthier): Argument offsets of bytecodes should be easily accessible by
// name or at least by position.
#define BYTECODE_ITERATOR(V)                                                   \
  V(BREAK, 0, 4)              /* bc8                                        */ \
  V(PUSH_CP, 1, 4)            /* bc8 pad24                                  */ \
  V(PUSH_BT, 2, 8)            /* bc8 pad24 offset32                         */ \
  V(PUSH_REGISTER, 3, 4)      /* bc8 reg_idx24                              */ \
  V(SET_REGISTER_TO_CP, 4, 8) /* bc8 reg_idx24 offset32                     */ \
  V(SET_CP_TO_REGISTER, 5, 4) /* bc8 reg_idx24                              */ \
  V(SET_REGISTER_TO_SP, 6, 4) /* bc8 reg_idx24                              */ \
  V(SET_SP_TO_REGISTER, 7, 4) /* bc8 reg_idx24                              */ \
  V(SET_REGISTER, 8, 8)       /* bc8 reg_idx24 value32                      */ \
  V(ADVANCE_REGISTER, 9, 8)   /* bc8 reg_idx24 value32                      */ \
  V(POP_CP, 10, 4)            /* bc8 pad24                                  */ \
  V(POP_BT, 11, 4)            /* bc8 pad24                                  */ \
  V(POP_REGISTER, 12, 4)      /* bc8 reg_idx24                              */ \
  V(FAIL, 13, 4)              /* bc8 pad24                                  */ \
  V(SUCCEED, 14, 4)           /* bc8 pad24                                  */ \
  V(ADVANCE_CP, 15, 4)        /* bc8 offset24                               */ \
  /* Jump to another bytecode given its offset.                             */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x10 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   0x00 (unused) Padding                                   */ \
  /* 0x20 - 0x3F:   Address of bytecode to jump to                          */ \
  V(GOTO, 16, 8) /* bc8 pad24 addr32                           */              \
  /* Check if offset is in range and load character at given offset.        */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x11 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Offset from current position                            */ \
  /* 0x20 - 0x3F:   Address of bytecode when load is out of range           */ \
  V(LOAD_CURRENT_CHAR, 17, 8) /* bc8 offset24 addr32                        */ \
  /* Load character at given offset without range checks.                   */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x12 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Offset from current position                            */ \
  V(LOAD_CURRENT_CHAR_UNCHECKED, 18, 4)    /* bc8 offset24 */                  \
  V(LOAD_2_CURRENT_CHARS, 19, 8)           /* bc8 offset24 addr32 */           \
  V(LOAD_2_CURRENT_CHARS_UNCHECKED, 20, 4) /* bc8 offset24 */                  \
  V(LOAD_4_CURRENT_CHARS, 21, 8)           /* bc8 offset24 addr32 */           \
  V(LOAD_4_CURRENT_CHARS_UNCHECKED, 22, 4) /* bc8 offset24 */                  \
  V(CHECK_4_CHARS, 23, 12) /* bc8 pad24 uint32 addr32                    */    \
  /* Check if current character is equal to a given character               */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x19 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x0F:   0x00 (unused) Padding                                   */ \
  /* 0x10 - 0x1F:   Character to check                                      */ \
  /* 0x20 - 0x3F:   Address of bytecode when matched                        */ \
  V(CHECK_CHAR, 24, 8) /* bc8 pad8 uint16 addr32                     */        \
  V(CHECK_NOT_4_CHARS, 25, 12) /* bc8 pad24 uint32 addr32 */                   \
  V(CHECK_NOT_CHAR, 26, 8) /* bc8 pad8 uint16 addr32                     */    \
  V(AND_CHECK_4_CHARS, 27, 16) /* bc8 pad24 uint32 uint32 addr32 */            \
  /* Checks if the current character combined with mask (bitwise and)       */ \
  /* matches a character (e.g. used when two characters in a disjunction    */ \
  /* differ by only a single bit                                            */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x1c (fixed) Bytecode                                   */ \
  /* 0x08 - 0x0F:   0x00 (unused) Padding                                   */ \
  /* 0x10 - 0x1F:   Character to match against (after mask aplied)          */ \
  /* 0x20 - 0x3F:   Bitmask bitwise and combined with current character     */ \
  /* 0x40 - 0x5F:   Address of bytecode when matched                        */ \
  V(AND_CHECK_CHAR, 28, 12)           /* bc8 pad8 uint16 uint32 addr32      */ \
  V(AND_CHECK_NOT_4_CHARS, 29, 16)    /* bc8 pad24 uint32 uint32 addr32 */     \
  V(AND_CHECK_NOT_CHAR, 30, 12)       /* bc8 pad8 uint16 uint32 addr32 */      \
  V(MINUS_AND_CHECK_NOT_CHAR, 31, 12) /* bc8 pad8 uc16 uc16 uc16 addr32 */     \
  V(CHECK_CHAR_IN_RANGE, 32, 12)      /* bc8 pad24 uc16 uc16 addr32 */         \
  V(CHECK_CHAR_NOT_IN_RANGE, 33, 12)  /* bc8 pad24 uc16 uc16 addr32 */         \
  /* Checks if the current character matches any of the characters encoded  */ \
  /* in a bit table. Similar to/inspired by boyer moore string search       */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x22 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   0x00 (unused) Padding                                   */ \
  /* 0x20 - 0x3F:   Address of bytecode when bit is set                     */ \
  /* 0x40 - 0xBF:   Bit table                                               */ \
  V(CHECK_BIT_IN_TABLE, 34, 24) /* bc8 pad24 addr32 bits128           */       \
  V(CHECK_LT, 35, 8) /* bc8 pad8 uc16 addr32                       */          \
  V(CHECK_GT, 36, 8) /* bc8 pad8 uc16 addr32                       */          \
  V(CHECK_NOT_BACK_REF, 37, 8)         /* bc8 reg_idx24 addr32 */              \
  V(CHECK_NOT_BACK_REF_NO_CASE, 38, 8) /* bc8 reg_idx24 addr32 */              \
  V(CHECK_NOT_BACK_REF_NO_CASE_UNICODE, 39, 8)                                 \
  V(CHECK_NOT_BACK_REF_BACKWARD, 40, 8)         /* bc8 reg_idx24 addr32 */     \
  V(CHECK_NOT_BACK_REF_NO_CASE_BACKWARD, 41, 8) /* bc8 reg_idx24 addr32 */     \
  V(CHECK_NOT_BACK_REF_NO_CASE_UNICODE_BACKWARD, 42, 8)                        \
  V(CHECK_NOT_REGS_EQUAL, 43, 12) /* bc8 regidx24 reg_idx32 addr32 */          \
  V(CHECK_REGISTER_LT, 44, 12)    /* bc8 reg_idx24 value32 addr32 */           \
  V(CHECK_REGISTER_GE, 45, 12)    /* bc8 reg_idx24 value32 addr32 */           \
  V(CHECK_REGISTER_EQ_POS, 46, 8) /* bc8 reg_idx24 addr32 */                   \
  V(CHECK_AT_START, 47, 8) /* bc8 pad24 addr32                           */    \
  V(CHECK_NOT_AT_START, 48, 8) /* bc8 offset24 addr32 */                       \
  /* Checks if the current position matches top of backtrack stack          */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x31 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   0x00 (unused) Padding                                   */ \
  /* 0x20 - 0x3F:   Address of bytecode when current matches tos            */ \
  V(CHECK_GREEDY, 49, 8) /* bc8 pad24 addr32                           */      \
  /* Advance character pointer by given offset and jump to another bytecode.*/ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x32 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Number of characters to advance                         */ \
  /* 0x20 - 0x3F:   Address of bytecode to jump to                          */ \
  V(ADVANCE_CP_AND_GOTO, 50, 8) /* bc8 offset24 addr32                    */   \
  V(SET_CURRENT_POSITION_FROM_END, 51, 4) /* bc8 idx24 */                      \
  /* Checks if current position + given offset is in range.                 */ \
  /* Bit Layout:                                                            */ \
  /* 0x00 - 0x07:   0x34 (fixed) Bytecode                                   */ \
  /* 0x08 - 0x1F:   Offset from current position                            */ \
  /* 0x20 - 0x3F:   Address of bytecode when position is out of range       */ \
  V(CHECK_CURRENT_POSITION, 52, 8) /* bc8 idx24 addr32                     */  \
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
  V(SKIP_UNTIL_BIT_IN_TABLE, 53, 32)                                           \
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
  V(SKIP_UNTIL_CHAR_AND, 54, 24)                                               \
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
  V(SKIP_UNTIL_CHAR, 55, 16)                                                   \
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
  V(SKIP_UNTIL_CHAR_POS_CHECKED, 56, 20)                                       \
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
  V(SKIP_UNTIL_CHAR_OR_CHAR, 57, 20)                                           \
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
  V(SKIP_UNTIL_GT_OR_NOT_BIT_IN_TABLE, 58, 32)

#define COUNT(...) +1
static constexpr int kRegExpBytecodeCount = BYTECODE_ITERATOR(COUNT);
#undef COUNT

// Just making sure we assigned values above properly. They should be
// contiguous, strictly increasing, and start at 0.
// TODO(jgruber): Do not explicitly assign values, instead generate them
// implicitly from the list order.
STATIC_ASSERT(kRegExpBytecodeCount == 59);

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
  return kRegExpBytecodeLengths[bytecode];
}

static const char* const kRegExpBytecodeNames[] = {
#define DECLARE_BYTECODE_NAME(name, ...) #name,
    BYTECODE_ITERATOR(DECLARE_BYTECODE_NAME)
#undef DECLARE_BYTECODE_NAME
};

inline const char* RegExpBytecodeName(int bytecode) {
  return kRegExpBytecodeNames[bytecode];
}

void RegExpBytecodeDisassembleSingle(const byte* code_base, const byte* pc);
void RegExpBytecodeDisassemble(const byte* code_base, int length,
                               const char* pattern);

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODES_H_

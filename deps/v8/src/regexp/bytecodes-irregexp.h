// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef V8_REGEXP_BYTECODES_IRREGEXP_H_
#define V8_REGEXP_BYTECODES_IRREGEXP_H_

#ifdef V8_INTERPRETED_REGEXP

namespace v8 {
namespace internal {


const int BYTECODE_MASK = 0xff;
// The first argument is packed in with the byte code in one word, but so it
// has 24 bits, but it can be positive and negative so only use 23 bits for
// positive values.
const unsigned int MAX_FIRST_ARG = 0x7fffffu;
const int BYTECODE_SHIFT = 8;

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
  V(GOTO, 16, 8)              /* bc8 pad24 addr32                           */ \
  V(LOAD_CURRENT_CHAR, 17, 8) /* bc8 offset24 addr32                        */ \
  V(LOAD_CURRENT_CHAR_UNCHECKED, 18, 4)    /* bc8 offset24 */                  \
  V(LOAD_2_CURRENT_CHARS, 19, 8)           /* bc8 offset24 addr32 */           \
  V(LOAD_2_CURRENT_CHARS_UNCHECKED, 20, 4) /* bc8 offset24 */                  \
  V(LOAD_4_CURRENT_CHARS, 21, 8)           /* bc8 offset24 addr32 */           \
  V(LOAD_4_CURRENT_CHARS_UNCHECKED, 22, 4) /* bc8 offset24 */                  \
  V(CHECK_4_CHARS, 23, 12) /* bc8 pad24 uint32 addr32                    */    \
  V(CHECK_CHAR, 24, 8)     /* bc8 pad8 uint16 addr32                     */    \
  V(CHECK_NOT_4_CHARS, 25, 12) /* bc8 pad24 uint32 addr32 */                   \
  V(CHECK_NOT_CHAR, 26, 8) /* bc8 pad8 uint16 addr32                     */    \
  V(AND_CHECK_4_CHARS, 27, 16) /* bc8 pad24 uint32 uint32 addr32 */            \
  V(AND_CHECK_CHAR, 28, 12) /* bc8 pad8 uint16 uint32 addr32              */   \
  V(AND_CHECK_NOT_4_CHARS, 29, 16)    /* bc8 pad24 uint32 uint32 addr32 */     \
  V(AND_CHECK_NOT_CHAR, 30, 12)       /* bc8 pad8 uint16 uint32 addr32 */      \
  V(MINUS_AND_CHECK_NOT_CHAR, 31, 12) /* bc8 pad8 uc16 uc16 uc16 addr32 */     \
  V(CHECK_CHAR_IN_RANGE, 32, 12)      /* bc8 pad24 uc16 uc16 addr32 */         \
  V(CHECK_CHAR_NOT_IN_RANGE, 33, 12)  /* bc8 pad24 uc16 uc16 addr32 */         \
  V(CHECK_BIT_IN_TABLE, 34, 24)       /* bc8 pad24 addr32 bits128 */           \
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
  V(CHECK_GREEDY, 49, 8) /* bc8 pad24 addr32                           */      \
  V(ADVANCE_CP_AND_GOTO, 50, 8)           /* bc8 offset24 addr32 */            \
  V(SET_CURRENT_POSITION_FROM_END, 51, 4) /* bc8 idx24 */

#define DECLARE_BYTECODES(name, code, length) \
  static const int BC_##name = code;
BYTECODE_ITERATOR(DECLARE_BYTECODES)
#undef DECLARE_BYTECODES

#define DECLARE_BYTECODE_LENGTH(name, code, length) \
  static const int BC_##name##_LENGTH = length;
BYTECODE_ITERATOR(DECLARE_BYTECODE_LENGTH)
#undef DECLARE_BYTECODE_LENGTH

}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETED_REGEXP

#endif  // V8_REGEXP_BYTECODES_IRREGEXP_H_

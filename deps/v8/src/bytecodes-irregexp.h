// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef V8_BYTECODES_IRREGEXP_H_
#define V8_BYTECODES_IRREGEXP_H_

namespace v8 {
namespace internal {


const int BYTECODE_MASK = 0xff;
// The first argument is packed in with the byte code in one word, but so it
// has 24 bits, but it can be positive and negative so only use 23 bits for
// positive values.
const unsigned int MAX_FIRST_ARG = 0x7fffffu;
const int BYTECODE_SHIFT = 8;

#define BYTECODE_ITERATOR(V)                                                   \
V(BREAK,              0, 4)   /* bc8                                        */ \
V(PUSH_CP,            1, 4)   /* bc8 pad24                                  */ \
V(PUSH_BT,            2, 8)   /* bc8 pad24 offset32                         */ \
V(PUSH_REGISTER,      3, 4)   /* bc8 reg_idx24                              */ \
V(SET_REGISTER_TO_CP, 4, 8)   /* bc8 reg_idx24 offset32                     */ \
V(SET_CP_TO_REGISTER, 5, 4)   /* bc8 reg_idx24                              */ \
V(SET_REGISTER_TO_SP, 6, 4)   /* bc8 reg_idx24                              */ \
V(SET_SP_TO_REGISTER, 7, 4)   /* bc8 reg_idx24                              */ \
V(SET_REGISTER,       8, 8)   /* bc8 reg_idx24 value32                      */ \
V(ADVANCE_REGISTER,   9, 8)   /* bc8 reg_idx24 value32                      */ \
V(POP_CP,            10, 4)   /* bc8 pad24                                  */ \
V(POP_BT,            11, 4)   /* bc8 pad24                                  */ \
V(POP_REGISTER,      12, 4)   /* bc8 reg_idx24                              */ \
V(FAIL,              13, 4)   /* bc8 pad24                                  */ \
V(SUCCEED,           14, 4)   /* bc8 pad24                                  */ \
V(ADVANCE_CP,        15, 4)   /* bc8 offset24                               */ \
V(GOTO,              16, 8)   /* bc8 pad24 addr32                           */ \
V(LOAD_CURRENT_CHAR, 17, 8)   /* bc8 offset24 addr32                        */ \
V(LOAD_CURRENT_CHAR_UNCHECKED, 18, 4) /* bc8 offset24                       */ \
V(LOAD_2_CURRENT_CHARS, 19, 8) /* bc8 offset24 addr32                       */ \
V(LOAD_2_CURRENT_CHARS_UNCHECKED, 20, 4) /* bc8 offset24                    */ \
V(LOAD_4_CURRENT_CHARS, 21, 8) /* bc8 offset24 addr32                       */ \
V(LOAD_4_CURRENT_CHARS_UNCHECKED, 22, 4) /* bc8 offset24                    */ \
V(CHECK_4_CHARS,     23, 12)  /* bc8 pad24 uint32 addr32                    */ \
V(CHECK_CHAR,        24, 8)   /* bc8 pad8 uint16 addr32                     */ \
V(CHECK_NOT_4_CHARS, 25, 12)  /* bc8 pad24 uint32 addr32                    */ \
V(CHECK_NOT_CHAR,    26, 8)   /* bc8 pad8 uint16 addr32                     */ \
V(AND_CHECK_4_CHARS, 27, 16)  /* bc8 pad24 uint32 uint32 addr32             */ \
V(AND_CHECK_CHAR,    28, 12)  /* bc8 pad8 uint16 uint32 addr32              */ \
V(AND_CHECK_NOT_4_CHARS, 29, 16) /* bc8 pad24 uint32 uint32 addr32          */ \
V(AND_CHECK_NOT_CHAR, 30, 12) /* bc8 pad8 uint16 uint32 addr32              */ \
V(MINUS_AND_CHECK_NOT_CHAR, 31, 12) /* bc8 pad8 uc16 uc16 uc16 addr32       */ \
V(CHECK_CHAR_IN_RANGE, 32, 12) /* bc8 pad24 uc16 uc16 addr32                */ \
V(CHECK_CHAR_NOT_IN_RANGE, 33, 12) /* bc8 pad24 uc16 uc16 addr32            */ \
V(CHECK_BIT_IN_TABLE, 34, 24) /* bc8 pad24 addr32 bits128                   */ \
V(CHECK_LT,          35, 8)   /* bc8 pad8 uc16 addr32                       */ \
V(CHECK_GT,          36, 8)   /* bc8 pad8 uc16 addr32                       */ \
V(CHECK_NOT_BACK_REF, 37, 8)  /* bc8 reg_idx24 addr32                       */ \
V(CHECK_NOT_BACK_REF_NO_CASE, 38, 8) /* bc8 reg_idx24 addr32                */ \
V(CHECK_NOT_REGS_EQUAL, 39, 12) /* bc8 regidx24 reg_idx32 addr32            */ \
V(CHECK_REGISTER_LT, 40, 12)  /* bc8 reg_idx24 value32 addr32               */ \
V(CHECK_REGISTER_GE, 41, 12)  /* bc8 reg_idx24 value32 addr32               */ \
V(CHECK_REGISTER_EQ_POS, 42, 8) /* bc8 reg_idx24 addr32                     */ \
V(CHECK_AT_START,    43, 8)   /* bc8 pad24 addr32                           */ \
V(CHECK_NOT_AT_START, 44, 8)  /* bc8 pad24 addr32                           */ \
V(CHECK_GREEDY,      45, 8)   /* bc8 pad24 addr32                           */ \
V(ADVANCE_CP_AND_GOTO, 46, 8) /* bc8 offset24 addr32                        */ \
V(SET_CURRENT_POSITION_FROM_END, 47, 4) /* bc8 idx24                        */

#define DECLARE_BYTECODES(name, code, length) \
  static const int BC_##name = code;
BYTECODE_ITERATOR(DECLARE_BYTECODES)
#undef DECLARE_BYTECODES

#define DECLARE_BYTECODE_LENGTH(name, code, length) \
  static const int BC_##name##_LENGTH = length;
BYTECODE_ITERATOR(DECLARE_BYTECODE_LENGTH)
#undef DECLARE_BYTECODE_LENGTH
} }

#endif  // V8_BYTECODES_IRREGEXP_H_

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

// A simple interpreter for the Irregexp byte code.


#include "v8.h"
#include "unicode.h"
#include "utils.h"
#include "ast.h"
#include "bytecodes-irregexp.h"
#include "interpreter-irregexp.h"


namespace v8 {
namespace internal {


typedef unibrow::Mapping<unibrow::Ecma262Canonicalize> Canonicalize;

static bool BackRefMatchesNoCase(Canonicalize* interp_canonicalize,
                                 int from,
                                 int current,
                                 int len,
                                 Vector<const uc16> subject) {
  for (int i = 0; i < len; i++) {
    unibrow::uchar old_char = subject[from++];
    unibrow::uchar new_char = subject[current++];
    if (old_char == new_char) continue;
    unibrow::uchar old_string[1] = { old_char };
    unibrow::uchar new_string[1] = { new_char };
    interp_canonicalize->get(old_char, '\0', old_string);
    interp_canonicalize->get(new_char, '\0', new_string);
    if (old_string[0] != new_string[0]) {
      return false;
    }
  }
  return true;
}


static bool BackRefMatchesNoCase(Canonicalize* interp_canonicalize,
                                 int from,
                                 int current,
                                 int len,
                                 Vector<const char> subject) {
  for (int i = 0; i < len; i++) {
    unsigned int old_char = subject[from++];
    unsigned int new_char = subject[current++];
    if (old_char == new_char) continue;
    if (old_char - 'A' <= 'Z' - 'A') old_char |= 0x20;
    if (new_char - 'A' <= 'Z' - 'A') new_char |= 0x20;
    if (old_char != new_char) return false;
  }
  return true;
}


#ifdef DEBUG
static void TraceInterpreter(const byte* code_base,
                             const byte* pc,
                             int stack_depth,
                             int current_position,
                             uint32_t current_char,
                             int bytecode_length,
                             const char* bytecode_name) {
  if (FLAG_trace_regexp_bytecodes) {
    bool printable = (current_char < 127 && current_char >= 32);
    const char* format =
        printable ?
        "pc = %02x, sp = %d, curpos = %d, curchar = %08x (%c), bc = %s" :
        "pc = %02x, sp = %d, curpos = %d, curchar = %08x .%c., bc = %s";
    PrintF(format,
           pc - code_base,
           stack_depth,
           current_position,
           current_char,
           printable ? current_char : '.',
           bytecode_name);
    for (int i = 0; i < bytecode_length; i++) {
      printf(", %02x", pc[i]);
    }
    printf(" ");
    for (int i = 1; i < bytecode_length; i++) {
      unsigned char b = pc[i];
      if (b < 127 && b >= 32) {
        printf("%c", b);
      } else {
        printf(".");
      }
    }
    printf("\n");
  }
}


#define BYTECODE(name)                                                      \
  case BC_##name:                                                           \
    TraceInterpreter(code_base,                                             \
                     pc,                                                    \
                     static_cast<int>(backtrack_sp - backtrack_stack_base), \
                     current,                                               \
                     current_char,                                          \
                     BC_##name##_LENGTH,                                    \
                     #name);
#else
#define BYTECODE(name)                                                      \
  case BC_##name:
#endif


static int32_t Load32Aligned(const byte* pc) {
  ASSERT((reinterpret_cast<intptr_t>(pc) & 3) == 0);
  return *reinterpret_cast<const int32_t *>(pc);
}


static int32_t Load16Aligned(const byte* pc) {
  ASSERT((reinterpret_cast<intptr_t>(pc) & 1) == 0);
  return *reinterpret_cast<const uint16_t *>(pc);
}


// A simple abstraction over the backtracking stack used by the interpreter.
// This backtracking stack does not grow automatically, but it ensures that the
// the memory held by the stack is released or remembered in a cache if the
// matching terminates.
class BacktrackStack {
 public:
  explicit BacktrackStack(Isolate* isolate) : isolate_(isolate) {
    if (isolate->irregexp_interpreter_backtrack_stack_cache() != NULL) {
      // If the cache is not empty reuse the previously allocated stack.
      data_ = isolate->irregexp_interpreter_backtrack_stack_cache();
      isolate->set_irregexp_interpreter_backtrack_stack_cache(NULL);
    } else {
      // Cache was empty. Allocate a new backtrack stack.
      data_ = NewArray<int>(kBacktrackStackSize);
    }
  }

  ~BacktrackStack() {
    if (isolate_->irregexp_interpreter_backtrack_stack_cache() == NULL) {
      // The cache is empty. Keep this backtrack stack around.
      isolate_->set_irregexp_interpreter_backtrack_stack_cache(data_);
    } else {
      // A backtrack stack was already cached, just release this one.
      DeleteArray(data_);
    }
  }

  int* data() const { return data_; }

  int max_size() const { return kBacktrackStackSize; }

 private:
  static const int kBacktrackStackSize = 10000;

  int* data_;
  Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(BacktrackStack);
};


template <typename Char>
static bool RawMatch(Isolate* isolate,
                     const byte* code_base,
                     Vector<const Char> subject,
                     int* registers,
                     int current,
                     uint32_t current_char) {
  const byte* pc = code_base;
  // BacktrackStack ensures that the memory allocated for the backtracking stack
  // is returned to the system or cached if there is no stack being cached at
  // the moment.
  BacktrackStack backtrack_stack(isolate);
  int* backtrack_stack_base = backtrack_stack.data();
  int* backtrack_sp = backtrack_stack_base;
  int backtrack_stack_space = backtrack_stack.max_size();
#ifdef DEBUG
  if (FLAG_trace_regexp_bytecodes) {
    PrintF("\n\nStart bytecode interpreter\n\n");
  }
#endif
  while (true) {
    int32_t insn = Load32Aligned(pc);
    switch (insn & BYTECODE_MASK) {
      BYTECODE(BREAK)
        UNREACHABLE();
        return false;
      BYTECODE(PUSH_CP)
        if (--backtrack_stack_space < 0) {
          return false;  // No match on backtrack stack overflow.
        }
        *backtrack_sp++ = current;
        pc += BC_PUSH_CP_LENGTH;
        break;
      BYTECODE(PUSH_BT)
        if (--backtrack_stack_space < 0) {
          return false;  // No match on backtrack stack overflow.
        }
        *backtrack_sp++ = Load32Aligned(pc + 4);
        pc += BC_PUSH_BT_LENGTH;
        break;
      BYTECODE(PUSH_REGISTER)
        if (--backtrack_stack_space < 0) {
          return false;  // No match on backtrack stack overflow.
        }
        *backtrack_sp++ = registers[insn >> BYTECODE_SHIFT];
        pc += BC_PUSH_REGISTER_LENGTH;
        break;
      BYTECODE(SET_REGISTER)
        registers[insn >> BYTECODE_SHIFT] = Load32Aligned(pc + 4);
        pc += BC_SET_REGISTER_LENGTH;
        break;
      BYTECODE(ADVANCE_REGISTER)
        registers[insn >> BYTECODE_SHIFT] += Load32Aligned(pc + 4);
        pc += BC_ADVANCE_REGISTER_LENGTH;
        break;
      BYTECODE(SET_REGISTER_TO_CP)
        registers[insn >> BYTECODE_SHIFT] = current + Load32Aligned(pc + 4);
        pc += BC_SET_REGISTER_TO_CP_LENGTH;
        break;
      BYTECODE(SET_CP_TO_REGISTER)
        current = registers[insn >> BYTECODE_SHIFT];
        pc += BC_SET_CP_TO_REGISTER_LENGTH;
        break;
      BYTECODE(SET_REGISTER_TO_SP)
        registers[insn >> BYTECODE_SHIFT] =
            static_cast<int>(backtrack_sp - backtrack_stack_base);
        pc += BC_SET_REGISTER_TO_SP_LENGTH;
        break;
      BYTECODE(SET_SP_TO_REGISTER)
        backtrack_sp = backtrack_stack_base + registers[insn >> BYTECODE_SHIFT];
        backtrack_stack_space = backtrack_stack.max_size() -
            static_cast<int>(backtrack_sp - backtrack_stack_base);
        pc += BC_SET_SP_TO_REGISTER_LENGTH;
        break;
      BYTECODE(POP_CP)
        backtrack_stack_space++;
        --backtrack_sp;
        current = *backtrack_sp;
        pc += BC_POP_CP_LENGTH;
        break;
      BYTECODE(POP_BT)
        backtrack_stack_space++;
        --backtrack_sp;
        pc = code_base + *backtrack_sp;
        break;
      BYTECODE(POP_REGISTER)
        backtrack_stack_space++;
        --backtrack_sp;
        registers[insn >> BYTECODE_SHIFT] = *backtrack_sp;
        pc += BC_POP_REGISTER_LENGTH;
        break;
      BYTECODE(FAIL)
        return false;
      BYTECODE(SUCCEED)
        return true;
      BYTECODE(ADVANCE_CP)
        current += insn >> BYTECODE_SHIFT;
        pc += BC_ADVANCE_CP_LENGTH;
        break;
      BYTECODE(GOTO)
        pc = code_base + Load32Aligned(pc + 4);
        break;
      BYTECODE(ADVANCE_CP_AND_GOTO)
        current += insn >> BYTECODE_SHIFT;
        pc = code_base + Load32Aligned(pc + 4);
        break;
      BYTECODE(CHECK_GREEDY)
        if (current == backtrack_sp[-1]) {
          backtrack_sp--;
          backtrack_stack_space++;
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_GREEDY_LENGTH;
        }
        break;
      BYTECODE(LOAD_CURRENT_CHAR) {
        int pos = current + (insn >> BYTECODE_SHIFT);
        if (pos >= subject.length()) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          current_char = subject[pos];
          pc += BC_LOAD_CURRENT_CHAR_LENGTH;
        }
        break;
      }
      BYTECODE(LOAD_CURRENT_CHAR_UNCHECKED) {
        int pos = current + (insn >> BYTECODE_SHIFT);
        current_char = subject[pos];
        pc += BC_LOAD_CURRENT_CHAR_UNCHECKED_LENGTH;
        break;
      }
      BYTECODE(LOAD_2_CURRENT_CHARS) {
        int pos = current + (insn >> BYTECODE_SHIFT);
        if (pos + 2 > subject.length()) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          Char next = subject[pos + 1];
          current_char =
              (subject[pos] | (next << (kBitsPerByte * sizeof(Char))));
          pc += BC_LOAD_2_CURRENT_CHARS_LENGTH;
        }
        break;
      }
      BYTECODE(LOAD_2_CURRENT_CHARS_UNCHECKED) {
        int pos = current + (insn >> BYTECODE_SHIFT);
        Char next = subject[pos + 1];
        current_char = (subject[pos] | (next << (kBitsPerByte * sizeof(Char))));
        pc += BC_LOAD_2_CURRENT_CHARS_UNCHECKED_LENGTH;
        break;
      }
      BYTECODE(LOAD_4_CURRENT_CHARS) {
        ASSERT(sizeof(Char) == 1);
        int pos = current + (insn >> BYTECODE_SHIFT);
        if (pos + 4 > subject.length()) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          Char next1 = subject[pos + 1];
          Char next2 = subject[pos + 2];
          Char next3 = subject[pos + 3];
          current_char = (subject[pos] |
                          (next1 << 8) |
                          (next2 << 16) |
                          (next3 << 24));
          pc += BC_LOAD_4_CURRENT_CHARS_LENGTH;
        }
        break;
      }
      BYTECODE(LOAD_4_CURRENT_CHARS_UNCHECKED) {
        ASSERT(sizeof(Char) == 1);
        int pos = current + (insn >> BYTECODE_SHIFT);
        Char next1 = subject[pos + 1];
        Char next2 = subject[pos + 2];
        Char next3 = subject[pos + 3];
        current_char = (subject[pos] |
                        (next1 << 8) |
                        (next2 << 16) |
                        (next3 << 24));
        pc += BC_LOAD_4_CURRENT_CHARS_UNCHECKED_LENGTH;
        break;
      }
      BYTECODE(CHECK_4_CHARS) {
        uint32_t c = Load32Aligned(pc + 4);
        if (c == current_char) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_CHECK_4_CHARS_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_CHAR) {
        uint32_t c = (insn >> BYTECODE_SHIFT);
        if (c == current_char) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_CHAR_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_NOT_4_CHARS) {
        uint32_t c = Load32Aligned(pc + 4);
        if (c != current_char) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_CHECK_NOT_4_CHARS_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_NOT_CHAR) {
        uint32_t c = (insn >> BYTECODE_SHIFT);
        if (c != current_char) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_NOT_CHAR_LENGTH;
        }
        break;
      }
      BYTECODE(AND_CHECK_4_CHARS) {
        uint32_t c = Load32Aligned(pc + 4);
        if (c == (current_char & Load32Aligned(pc + 8))) {
          pc = code_base + Load32Aligned(pc + 12);
        } else {
          pc += BC_AND_CHECK_4_CHARS_LENGTH;
        }
        break;
      }
      BYTECODE(AND_CHECK_CHAR) {
        uint32_t c = (insn >> BYTECODE_SHIFT);
        if (c == (current_char & Load32Aligned(pc + 4))) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_AND_CHECK_CHAR_LENGTH;
        }
        break;
      }
      BYTECODE(AND_CHECK_NOT_4_CHARS) {
        uint32_t c = Load32Aligned(pc + 4);
        if (c != (current_char & Load32Aligned(pc + 8))) {
          pc = code_base + Load32Aligned(pc + 12);
        } else {
          pc += BC_AND_CHECK_NOT_4_CHARS_LENGTH;
        }
        break;
      }
      BYTECODE(AND_CHECK_NOT_CHAR) {
        uint32_t c = (insn >> BYTECODE_SHIFT);
        if (c != (current_char & Load32Aligned(pc + 4))) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_AND_CHECK_NOT_CHAR_LENGTH;
        }
        break;
      }
      BYTECODE(MINUS_AND_CHECK_NOT_CHAR) {
        uint32_t c = (insn >> BYTECODE_SHIFT);
        uint32_t minus = Load16Aligned(pc + 4);
        uint32_t mask = Load16Aligned(pc + 6);
        if (c != ((current_char - minus) & mask)) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_MINUS_AND_CHECK_NOT_CHAR_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_LT) {
        uint32_t limit = (insn >> BYTECODE_SHIFT);
        if (current_char < limit) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_LT_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_GT) {
        uint32_t limit = (insn >> BYTECODE_SHIFT);
        if (current_char > limit) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_GT_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_REGISTER_LT)
        if (registers[insn >> BYTECODE_SHIFT] < Load32Aligned(pc + 4)) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_CHECK_REGISTER_LT_LENGTH;
        }
        break;
      BYTECODE(CHECK_REGISTER_GE)
        if (registers[insn >> BYTECODE_SHIFT] >= Load32Aligned(pc + 4)) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_CHECK_REGISTER_GE_LENGTH;
        }
        break;
      BYTECODE(CHECK_REGISTER_EQ_POS)
        if (registers[insn >> BYTECODE_SHIFT] == current) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_REGISTER_EQ_POS_LENGTH;
        }
        break;
      BYTECODE(LOOKUP_MAP1) {
        // Look up character in a bitmap.  If we find a 0, then jump to the
        // location at pc + 8.  Otherwise fall through!
        int index = current_char - (insn >> BYTECODE_SHIFT);
        byte map = code_base[Load32Aligned(pc + 4) + (index >> 3)];
        map = ((map >> (index & 7)) & 1);
        if (map == 0) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_LOOKUP_MAP1_LENGTH;
        }
        break;
      }
      BYTECODE(LOOKUP_MAP2) {
        // Look up character in a half-nibble map.  If we find 00, then jump to
        // the location at pc + 8.   If we find 01 then jump to location at
        // pc + 11, etc.
        int index = (current_char - (insn >> BYTECODE_SHIFT)) << 1;
        byte map = code_base[Load32Aligned(pc + 3) + (index >> 3)];
        map = ((map >> (index & 7)) & 3);
        if (map < 2) {
          if (map == 0) {
            pc = code_base + Load32Aligned(pc + 8);
          } else {
            pc = code_base + Load32Aligned(pc + 12);
          }
        } else {
          if (map == 2) {
            pc = code_base + Load32Aligned(pc + 16);
          } else {
            pc = code_base + Load32Aligned(pc + 20);
          }
        }
        break;
      }
      BYTECODE(LOOKUP_MAP8) {
        // Look up character in a byte map.  Use the byte as an index into a
        // table that follows this instruction immediately.
        int index = current_char - (insn >> BYTECODE_SHIFT);
        byte map = code_base[Load32Aligned(pc + 4) + index];
        const byte* new_pc = code_base + Load32Aligned(pc + 8) + (map << 2);
        pc = code_base + Load32Aligned(new_pc);
        break;
      }
      BYTECODE(LOOKUP_HI_MAP8) {
        // Look up high byte of this character in a byte map.  Use the byte as
        // an index into a table that follows this instruction immediately.
        int index = (current_char >> 8) - (insn >> BYTECODE_SHIFT);
        byte map = code_base[Load32Aligned(pc + 4) + index];
        const byte* new_pc = code_base + Load32Aligned(pc + 8) + (map << 2);
        pc = code_base + Load32Aligned(new_pc);
        break;
      }
      BYTECODE(CHECK_NOT_REGS_EQUAL)
        if (registers[insn >> BYTECODE_SHIFT] ==
            registers[Load32Aligned(pc + 4)]) {
          pc += BC_CHECK_NOT_REGS_EQUAL_LENGTH;
        } else {
          pc = code_base + Load32Aligned(pc + 8);
        }
        break;
      BYTECODE(CHECK_NOT_BACK_REF) {
        int from = registers[insn >> BYTECODE_SHIFT];
        int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
        if (from < 0 || len <= 0) {
          pc += BC_CHECK_NOT_BACK_REF_LENGTH;
          break;
        }
        if (current + len > subject.length()) {
          pc = code_base + Load32Aligned(pc + 4);
          break;
        } else {
          int i;
          for (i = 0; i < len; i++) {
            if (subject[from + i] != subject[current + i]) {
              pc = code_base + Load32Aligned(pc + 4);
              break;
            }
          }
          if (i < len) break;
          current += len;
        }
        pc += BC_CHECK_NOT_BACK_REF_LENGTH;
        break;
      }
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE) {
        int from = registers[insn >> BYTECODE_SHIFT];
        int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
        if (from < 0 || len <= 0) {
          pc += BC_CHECK_NOT_BACK_REF_NO_CASE_LENGTH;
          break;
        }
        if (current + len > subject.length()) {
          pc = code_base + Load32Aligned(pc + 4);
          break;
        } else {
          if (BackRefMatchesNoCase(isolate->interp_canonicalize_mapping(),
                                   from, current, len, subject)) {
            current += len;
            pc += BC_CHECK_NOT_BACK_REF_NO_CASE_LENGTH;
          } else {
            pc = code_base + Load32Aligned(pc + 4);
          }
        }
        break;
      }
      BYTECODE(CHECK_AT_START)
        if (current == 0) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_AT_START_LENGTH;
        }
        break;
      BYTECODE(CHECK_NOT_AT_START)
        if (current == 0) {
          pc += BC_CHECK_NOT_AT_START_LENGTH;
        } else {
          pc = code_base + Load32Aligned(pc + 4);
        }
        break;
      BYTECODE(SET_CURRENT_POSITION_FROM_END) {
        int by = static_cast<uint32_t>(insn) >> BYTECODE_SHIFT;
        if (subject.length() - current > by) {
          current = subject.length() - by;
          current_char = subject[current - 1];
        }
        pc += BC_SET_CURRENT_POSITION_FROM_END_LENGTH;
        break;
      }
      default:
        UNREACHABLE();
        break;
    }
  }
}


bool IrregexpInterpreter::Match(Isolate* isolate,
                                Handle<ByteArray> code_array,
                                Handle<String> subject,
                                int* registers,
                                int start_position) {
  ASSERT(subject->IsFlat());

  AssertNoAllocation a;
  const byte* code_base = code_array->GetDataStartAddress();
  uc16 previous_char = '\n';
  String::FlatContent subject_content = subject->GetFlatContent();
  if (subject_content.IsAscii()) {
    Vector<const char> subject_vector = subject_content.ToAsciiVector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate,
                    code_base,
                    subject_vector,
                    registers,
                    start_position,
                    previous_char);
  } else {
    ASSERT(subject_content.IsTwoByte());
    Vector<const uc16> subject_vector = subject_content.ToUC16Vector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate,
                    code_base,
                    subject_vector,
                    registers,
                    start_position,
                    previous_char);
  }
}

} }  // namespace v8::internal

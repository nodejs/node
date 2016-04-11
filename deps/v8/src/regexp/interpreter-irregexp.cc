// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#include "src/regexp/interpreter-irregexp.h"

#include "src/ast/ast.h"
#include "src/regexp/bytecodes-irregexp.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/unicode.h"
#include "src/utils.h"

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
                                 Vector<const uint8_t> subject) {
  for (int i = 0; i < len; i++) {
    unsigned int old_char = subject[from++];
    unsigned int new_char = subject[current++];
    if (old_char == new_char) continue;
    // Convert both characters to lower case.
    old_char |= 0x20;
    new_char |= 0x20;
    if (old_char != new_char) return false;
    // Not letters in the ASCII range and Latin-1 range.
    if (!(old_char - 'a' <= 'z' - 'a') &&
        !(old_char - 224 <= 254 - 224 && old_char != 247)) {
      return false;
    }
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
  DCHECK((reinterpret_cast<intptr_t>(pc) & 3) == 0);
  return *reinterpret_cast<const int32_t *>(pc);
}


static int32_t Load16Aligned(const byte* pc) {
  DCHECK((reinterpret_cast<intptr_t>(pc) & 1) == 0);
  return *reinterpret_cast<const uint16_t *>(pc);
}


// A simple abstraction over the backtracking stack used by the interpreter.
// This backtracking stack does not grow automatically, but it ensures that the
// the memory held by the stack is released or remembered in a cache if the
// matching terminates.
class BacktrackStack {
 public:
  BacktrackStack() { data_ = NewArray<int>(kBacktrackStackSize); }

  ~BacktrackStack() {
    DeleteArray(data_);
  }

  int* data() const { return data_; }

  int max_size() const { return kBacktrackStackSize; }

 private:
  static const int kBacktrackStackSize = 10000;

  int* data_;

  DISALLOW_COPY_AND_ASSIGN(BacktrackStack);
};


template <typename Char>
static RegExpImpl::IrregexpResult RawMatch(Isolate* isolate,
                                           const byte* code_base,
                                           Vector<const Char> subject,
                                           int* registers,
                                           int current,
                                           uint32_t current_char) {
  const byte* pc = code_base;
  // BacktrackStack ensures that the memory allocated for the backtracking stack
  // is returned to the system or cached if there is no stack being cached at
  // the moment.
  BacktrackStack backtrack_stack;
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
        return RegExpImpl::RE_FAILURE;
      BYTECODE(PUSH_CP)
        if (--backtrack_stack_space < 0) {
          return RegExpImpl::RE_EXCEPTION;
        }
        *backtrack_sp++ = current;
        pc += BC_PUSH_CP_LENGTH;
        break;
      BYTECODE(PUSH_BT)
        if (--backtrack_stack_space < 0) {
          return RegExpImpl::RE_EXCEPTION;
        }
        *backtrack_sp++ = Load32Aligned(pc + 4);
        pc += BC_PUSH_BT_LENGTH;
        break;
      BYTECODE(PUSH_REGISTER)
        if (--backtrack_stack_space < 0) {
          return RegExpImpl::RE_EXCEPTION;
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
        return RegExpImpl::RE_FAILURE;
      BYTECODE(SUCCEED)
        return RegExpImpl::RE_SUCCESS;
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
        if (pos >= subject.length() || pos < 0) {
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
        if (pos + 2 > subject.length() || pos < 0) {
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
        DCHECK(sizeof(Char) == 1);
        int pos = current + (insn >> BYTECODE_SHIFT);
        if (pos + 4 > subject.length() || pos < 0) {
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
        DCHECK(sizeof(Char) == 1);
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
      BYTECODE(CHECK_CHAR_IN_RANGE) {
        uint32_t from = Load16Aligned(pc + 4);
        uint32_t to = Load16Aligned(pc + 6);
        if (from <= current_char && current_char <= to) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_CHECK_CHAR_IN_RANGE_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_CHAR_NOT_IN_RANGE) {
        uint32_t from = Load16Aligned(pc + 4);
        uint32_t to = Load16Aligned(pc + 6);
        if (from > current_char || current_char > to) {
          pc = code_base + Load32Aligned(pc + 8);
        } else {
          pc += BC_CHECK_CHAR_NOT_IN_RANGE_LENGTH;
        }
        break;
      }
      BYTECODE(CHECK_BIT_IN_TABLE) {
        int mask = RegExpMacroAssembler::kTableMask;
        byte b = pc[8 + ((current_char & mask) >> kBitsPerByteLog2)];
        int bit = (current_char & (kBitsPerByte - 1));
        if ((b & (1 << bit)) != 0) {
          pc = code_base + Load32Aligned(pc + 4);
        } else {
          pc += BC_CHECK_BIT_IN_TABLE_LENGTH;
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
        if (from >= 0 && len > 0) {
          if (current + len > subject.length() ||
              CompareChars(&subject[from], &subject[current], len) != 0) {
            pc = code_base + Load32Aligned(pc + 4);
            break;
          }
          current += len;
        }
        pc += BC_CHECK_NOT_BACK_REF_LENGTH;
        break;
      }
      BYTECODE(CHECK_NOT_BACK_REF_BACKWARD) {
        int from = registers[insn >> BYTECODE_SHIFT];
        int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
        if (from >= 0 && len > 0) {
          if (current - len < 0 ||
              CompareChars(&subject[from], &subject[current - len], len) != 0) {
            pc = code_base + Load32Aligned(pc + 4);
            break;
          }
          current -= len;
        }
        pc += BC_CHECK_NOT_BACK_REF_BACKWARD_LENGTH;
        break;
      }
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE) {
        int from = registers[insn >> BYTECODE_SHIFT];
        int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
        if (from >= 0 && len > 0) {
          if (current + len > subject.length() ||
              !BackRefMatchesNoCase(isolate->interp_canonicalize_mapping(),
                                    from, current, len, subject)) {
            pc = code_base + Load32Aligned(pc + 4);
            break;
          }
          current += len;
        }
        pc += BC_CHECK_NOT_BACK_REF_NO_CASE_LENGTH;
        break;
      }
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_BACKWARD) {
        int from = registers[insn >> BYTECODE_SHIFT];
        int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
        if (from >= 0 && len > 0) {
          if (current - len < 0 ||
              !BackRefMatchesNoCase(isolate->interp_canonicalize_mapping(),
                                    from, current - len, len, subject)) {
            pc = code_base + Load32Aligned(pc + 4);
            break;
          }
          current -= len;
        }
        pc += BC_CHECK_NOT_BACK_REF_NO_CASE_BACKWARD_LENGTH;
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
        if (current + (insn >> BYTECODE_SHIFT) == 0) {
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


RegExpImpl::IrregexpResult IrregexpInterpreter::Match(
    Isolate* isolate,
    Handle<ByteArray> code_array,
    Handle<String> subject,
    int* registers,
    int start_position) {
  DCHECK(subject->IsFlat());

  DisallowHeapAllocation no_gc;
  const byte* code_base = code_array->GetDataStartAddress();
  uc16 previous_char = '\n';
  String::FlatContent subject_content = subject->GetFlatContent();
  if (subject_content.IsOneByte()) {
    Vector<const uint8_t> subject_vector = subject_content.ToOneByteVector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate,
                    code_base,
                    subject_vector,
                    registers,
                    start_position,
                    previous_char);
  } else {
    DCHECK(subject_content.IsTwoByte());
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

}  // namespace internal
}  // namespace v8

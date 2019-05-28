// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#include "src/regexp/interpreter-irregexp.h"

#include "src/ast/ast.h"
#include "src/objects-inl.h"
#include "src/regexp/bytecodes-irregexp.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/unicode.h"
#include "src/utils.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uchar.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

static bool BackRefMatchesNoCase(Isolate* isolate, int from, int current,
                                 int len, Vector<const uc16> subject,
                                 bool unicode) {
  Address offset_a =
      reinterpret_cast<Address>(const_cast<uc16*>(&subject.at(from)));
  Address offset_b =
      reinterpret_cast<Address>(const_cast<uc16*>(&subject.at(current)));
  size_t length = len * kUC16Size;
  return RegExpMacroAssembler::CaseInsensitiveCompareUC16(
             offset_a, offset_b, length, unicode ? nullptr : isolate) == 1;
}


static bool BackRefMatchesNoCase(Isolate* isolate, int from, int current,
                                 int len, Vector<const uint8_t> subject,
                                 bool unicode) {
  // For Latin1 characters the unicode flag makes no difference.
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
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(pc) & 3);
  return *reinterpret_cast<const int32_t *>(pc);
}


static int32_t Load16Aligned(const byte* pc) {
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(pc) & 1);
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

namespace {

IrregexpInterpreter::Result StackOverflow(Isolate* isolate) {
  // We abort interpreter execution after the stack overflow is thrown, and thus
  // allow allocation here despite the outer DisallowHeapAllocationScope.
  AllowHeapAllocation yes_gc;
  isolate->StackOverflow();
  return IrregexpInterpreter::EXCEPTION;
}

// Runs all pending interrupts. Callers must update unhandlified object
// references after this function completes.
IrregexpInterpreter::Result HandleInterrupts(Isolate* isolate,
                                             Handle<String> subject_string) {
  DisallowHeapAllocation no_gc;

  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
    // A real stack overflow.
    return StackOverflow(isolate);
  }

  const bool was_one_byte =
      String::IsOneByteRepresentationUnderneath(*subject_string);

  Object result;
  {
    AllowHeapAllocation yes_gc;
    result = isolate->stack_guard()->HandleInterrupts();
  }

  if (result->IsException(isolate)) {
    return IrregexpInterpreter::EXCEPTION;
  }

  // If we changed between a LATIN1 and a UC16 string, we need to restart
  // regexp matching with the appropriate template instantiation of RawMatch.
  if (String::IsOneByteRepresentationUnderneath(*subject_string) !=
      was_one_byte) {
    return IrregexpInterpreter::RETRY;
  }

  return IrregexpInterpreter::SUCCESS;
}

template <typename Char>
void UpdateCodeAndSubjectReferences(Isolate* isolate,
                                    Handle<ByteArray> code_array,
                                    Handle<String> subject_string,
                                    const byte** code_base_out,
                                    const byte** pc_out,
                                    Vector<const Char>* subject_string_out) {
  DisallowHeapAllocation no_gc;

  if (*code_base_out != code_array->GetDataStartAddress()) {
    const intptr_t pc_offset = *pc_out - *code_base_out;
    DCHECK_GT(pc_offset, 0);
    *code_base_out = code_array->GetDataStartAddress();
    *pc_out = *code_base_out + pc_offset;
  }

  DCHECK(subject_string->IsFlat());
  *subject_string_out = subject_string->GetCharVector<Char>(no_gc);
}

template <typename Char>
IrregexpInterpreter::Result RawMatch(Isolate* isolate,
                                     Handle<ByteArray> code_array,
                                     Handle<String> subject_string,
                                     Vector<const Char> subject, int* registers,
                                     int current, uint32_t current_char) {
  DisallowHeapAllocation no_gc;

  const byte* pc = code_array->GetDataStartAddress();
  const byte* code_base = pc;

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
      BYTECODE(PUSH_CP)
        if (--backtrack_stack_space < 0) {
          return StackOverflow(isolate);
        }
        *backtrack_sp++ = current;
        pc += BC_PUSH_CP_LENGTH;
        break;
      BYTECODE(PUSH_BT)
        if (--backtrack_stack_space < 0) {
          return StackOverflow(isolate);
        }
        *backtrack_sp++ = Load32Aligned(pc + 4);
        pc += BC_PUSH_BT_LENGTH;
        break;
      BYTECODE(PUSH_REGISTER)
        if (--backtrack_stack_space < 0) {
          return StackOverflow(isolate);
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
        // clang-format off
      BYTECODE(POP_BT) {
        IrregexpInterpreter::Result return_code = HandleInterrupts(
            isolate, subject_string);
        if (return_code != IrregexpInterpreter::SUCCESS) return return_code;

        UpdateCodeAndSubjectReferences(isolate, code_array, subject_string,
            &code_base, &pc, &subject);

        backtrack_stack_space++;
        --backtrack_sp;
        pc = code_base + *backtrack_sp;
        break;
      }
      BYTECODE(POP_REGISTER)  // clang-format on
        backtrack_stack_space++;
        --backtrack_sp;
        registers[insn >> BYTECODE_SHIFT] = *backtrack_sp;
        pc += BC_POP_REGISTER_LENGTH;
        break;
      BYTECODE(FAIL)
      return IrregexpInterpreter::FAILURE;
      BYTECODE(SUCCEED)
      return IrregexpInterpreter::SUCCESS;
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
        DCHECK_EQ(1, sizeof(Char));
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
        DCHECK_EQ(1, sizeof(Char));
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
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_UNICODE)
      V8_FALLTHROUGH;
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE) {
        bool unicode =
            (insn & BYTECODE_MASK) == BC_CHECK_NOT_BACK_REF_NO_CASE_UNICODE;
        int from = registers[insn >> BYTECODE_SHIFT];
        int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
        if (from >= 0 && len > 0) {
          if (current + len > subject.length() ||
              !BackRefMatchesNoCase(isolate, from, current, len, subject,
                                    unicode)) {
            pc = code_base + Load32Aligned(pc + 4);
            break;
          }
          current += len;
        }
        pc += BC_CHECK_NOT_BACK_REF_NO_CASE_LENGTH;
        break;
      }
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_UNICODE_BACKWARD)
      V8_FALLTHROUGH;
      BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_BACKWARD) {
        bool unicode = (insn & BYTECODE_MASK) ==
                       BC_CHECK_NOT_BACK_REF_NO_CASE_UNICODE_BACKWARD;
        int from = registers[insn >> BYTECODE_SHIFT];
        int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
        if (from >= 0 && len > 0) {
          if (current - len < 0 ||
              !BackRefMatchesNoCase(isolate, from, current - len, len, subject,
                                    unicode)) {
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

}  // namespace

// static
IrregexpInterpreter::Result IrregexpInterpreter::Match(
    Isolate* isolate, Handle<ByteArray> code_array,
    Handle<String> subject_string, int* registers, int start_position) {
  DCHECK(subject_string->IsFlat());

  // Note: Heap allocation *is* allowed in two situations:
  // 1. When creating & throwing a stack overflow exception. The interpreter
  //    aborts afterwards, and thus possible-moved objects are never used.
  // 2. When handling interrupts. We manually relocate unhandlified references
  //    after interrupts have run.
  DisallowHeapAllocation no_gc;

  uc16 previous_char = '\n';
  String::FlatContent subject_content = subject_string->GetFlatContent(no_gc);
  if (subject_content.IsOneByte()) {
    Vector<const uint8_t> subject_vector = subject_content.ToOneByteVector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate, code_array, subject_string, subject_vector,
                    registers, start_position, previous_char);
  } else {
    DCHECK(subject_content.IsTwoByte());
    Vector<const uc16> subject_vector = subject_content.ToUC16Vector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate, code_array, subject_string, subject_vector,
                    registers, start_position, previous_char);
  }
}

}  // namespace internal
}  // namespace v8

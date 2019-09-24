// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#include "src/regexp/regexp-interpreter.h"

#include "src/ast/ast.h"
#include "src/base/small-vector.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/objects-inl.h"
#include "src/regexp/regexp-bytecodes.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp.h"
#include "src/strings/unicode.h"
#include "src/utils/utils.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uchar.h"
#endif  // V8_INTL_SUPPORT

// Use token threaded dispatch iff the compiler supports computed gotos and the
// build argument v8_enable_regexp_interpreter_threaded_dispatch was set.
#if V8_HAS_COMPUTED_GOTO && \
    defined(V8_ENABLE_REGEXP_INTERPRETER_THREADED_DISPATCH)
#define V8_USE_COMPUTED_GOTO 1
#endif  // V8_HAS_COMPUTED_GOTO

namespace v8 {
namespace internal {

namespace {

bool BackRefMatchesNoCase(Isolate* isolate, int from, int current, int len,
                          Vector<const uc16> subject, bool unicode) {
  Address offset_a =
      reinterpret_cast<Address>(const_cast<uc16*>(&subject.at(from)));
  Address offset_b =
      reinterpret_cast<Address>(const_cast<uc16*>(&subject.at(current)));
  size_t length = len * kUC16Size;
  return RegExpMacroAssembler::CaseInsensitiveCompareUC16(
             offset_a, offset_b, length, unicode ? nullptr : isolate) == 1;
}

bool BackRefMatchesNoCase(Isolate* isolate, int from, int current, int len,
                          Vector<const uint8_t> subject, bool unicode) {
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

void DisassembleSingleBytecode(const byte* code_base, const byte* pc) {
  PrintF("%s", RegExpBytecodeName(*pc));

  // Args and the bytecode as hex.
  for (int i = 0; i < RegExpBytecodeLength(*pc); i++) {
    PrintF(", %02x", pc[i]);
  }
  PrintF(" ");

  // Args as ascii.
  for (int i = 1; i < RegExpBytecodeLength(*pc); i++) {
    unsigned char b = pc[i];
    PrintF("%c", std::isprint(b) ? b : '.');
  }
  PrintF("\n");
}

#ifdef DEBUG
void MaybeTraceInterpreter(const byte* code_base, const byte* pc,
                           int stack_depth, int current_position,
                           uint32_t current_char, int bytecode_length,
                           const char* bytecode_name) {
  if (FLAG_trace_regexp_bytecodes) {
    const bool printable = std::isprint(current_char);
    const char* format =
        printable
            ? "pc = %02x, sp = %d, curpos = %d, curchar = %08x (%c), bc = "
            : "pc = %02x, sp = %d, curpos = %d, curchar = %08x .%c., bc = ";
    PrintF(format, pc - code_base, stack_depth, current_position, current_char,
           printable ? current_char : '.');

    DisassembleSingleBytecode(code_base, pc);
  }
}
#endif  // DEBUG

int32_t Load32Aligned(const byte* pc) {
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(pc) & 3);
  return *reinterpret_cast<const int32_t*>(pc);
}

int32_t Load16Aligned(const byte* pc) {
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(pc) & 1);
  return *reinterpret_cast<const uint16_t*>(pc);
}

// A simple abstraction over the backtracking stack used by the interpreter.
//
// Despite the name 'backtracking' stack, it's actually used as a generic stack
// that stores both program counters (= offsets into the bytecode) and generic
// integer values.
class BacktrackStack {
 public:
  BacktrackStack() = default;

  void push(int v) { data_.emplace_back(v); }
  int peek() const {
    DCHECK(!data_.empty());
    return data_.back();
  }
  int pop() {
    int v = peek();
    data_.pop_back();
    return v;
  }

  // The 'sp' is the index of the first empty element in the stack.
  int sp() const { return static_cast<int>(data_.size()); }
  void set_sp(int new_sp) {
    DCHECK_LE(new_sp, sp());
    data_.resize_no_init(new_sp);
  }

 private:
  // Semi-arbitrary. Should be large enough for common cases to remain in the
  // static stack-allocated backing store, but small enough not to waste space.
  static constexpr int kStaticCapacity = 64;

  base::SmallVector<int, kStaticCapacity> data_;

  DISALLOW_COPY_AND_ASSIGN(BacktrackStack);
};

IrregexpInterpreter::Result StackOverflow(Isolate* isolate,
                                          RegExp::CallOrigin call_origin) {
  CHECK(call_origin == RegExp::CallOrigin::kFromRuntime);
  // We abort interpreter execution after the stack overflow is thrown, and thus
  // allow allocation here despite the outer DisallowHeapAllocationScope.
  AllowHeapAllocation yes_gc;
  isolate->StackOverflow();
  return IrregexpInterpreter::EXCEPTION;
}

template <typename Char>
void UpdateCodeAndSubjectReferences(
    Isolate* isolate, Handle<ByteArray> code_array,
    Handle<String> subject_string, ByteArray* code_array_out,
    const byte** code_base_out, const byte** pc_out, String* subject_string_out,
    Vector<const Char>* subject_string_vector_out) {
  DisallowHeapAllocation no_gc;

  if (*code_base_out != code_array->GetDataStartAddress()) {
    *code_array_out = *code_array;
    const intptr_t pc_offset = *pc_out - *code_base_out;
    DCHECK_GT(pc_offset, 0);
    *code_base_out = code_array->GetDataStartAddress();
    *pc_out = *code_base_out + pc_offset;
  }

  DCHECK(subject_string->IsFlat());
  *subject_string_out = *subject_string;
  *subject_string_vector_out = subject_string->GetCharVector<Char>(no_gc);
}

// Runs all pending interrupts and updates unhandlified object references if
// necessary.
template <typename Char>
IrregexpInterpreter::Result HandleInterrupts(
    Isolate* isolate, RegExp::CallOrigin call_origin, ByteArray* code_array_out,
    String* subject_string_out, const byte** code_base_out,
    Vector<const Char>* subject_string_vector_out, const byte** pc_out) {
  DisallowHeapAllocation no_gc;

  StackLimitCheck check(isolate);
  bool js_has_overflowed = check.JsHasOverflowed();

  if (call_origin == RegExp::CallOrigin::kFromJs) {
    // Direct calls from JavaScript can be interrupted in two ways:
    // 1. A real stack overflow, in which case we let the caller throw the
    //    exception.
    // 2. The stack guard was used to interrupt execution for another purpose,
    //    forcing the call through the runtime system.
    if (js_has_overflowed) {
      return IrregexpInterpreter::EXCEPTION;
    } else if (check.InterruptRequested()) {
      return IrregexpInterpreter::RETRY;
    }
  } else {
    DCHECK(call_origin == RegExp::CallOrigin::kFromRuntime);
    // Prepare for possible GC.
    HandleScope handles(isolate);
    Handle<ByteArray> code_handle(*code_array_out, isolate);
    Handle<String> subject_handle(*subject_string_out, isolate);

    if (js_has_overflowed) {
      return StackOverflow(isolate, call_origin);
    } else if (check.InterruptRequested()) {
      const bool was_one_byte =
          String::IsOneByteRepresentationUnderneath(*subject_string_out);
      Object result;
      {
        AllowHeapAllocation yes_gc;
        result = isolate->stack_guard()->HandleInterrupts();
      }
      if (result.IsException(isolate)) {
        return IrregexpInterpreter::EXCEPTION;
      }

      // If we changed between a LATIN1 and a UC16 string, we need to restart
      // regexp matching with the appropriate template instantiation of
      // RawMatch.
      if (String::IsOneByteRepresentationUnderneath(*subject_handle) !=
          was_one_byte) {
        return IrregexpInterpreter::RETRY;
      }

      UpdateCodeAndSubjectReferences(
          isolate, code_handle, subject_handle, code_array_out, code_base_out,
          pc_out, subject_string_out, subject_string_vector_out);
    }
  }

  return IrregexpInterpreter::SUCCESS;
}

// If computed gotos are supported by the compiler, we can get addresses to
// labels directly in C/C++. Every bytecode handler has its own label and we
// store the addresses in a dispatch table indexed by bytecode. To execute the
// next handler we simply jump (goto) directly to its address.
#if V8_USE_COMPUTED_GOTO
#define BC_LABEL(name) BC_##name:
#define DECODE()                                                   \
  do {                                                             \
    next_insn = Load32Aligned(next_pc);                            \
    next_handler_addr = dispatch_table[next_insn & BYTECODE_MASK]; \
  } while (false)
#define DISPATCH()  \
  pc = next_pc;     \
  insn = next_insn; \
  goto* next_handler_addr
// Without computed goto support, we fall back to a simple switch-based
// dispatch (A large switch statement inside a loop with a case for every
// bytecode).
#else  // V8_USE_COMPUTED_GOTO
#define BC_LABEL(name) case BC_##name:
#define DECODE() next_insn = Load32Aligned(next_pc)
#define DISPATCH()  \
  pc = next_pc;     \
  insn = next_insn; \
  break
#endif  // V8_USE_COMPUTED_GOTO

// ADVANCE/SET_PC_FROM_OFFSET are separated from DISPATCH, because ideally some
// instructions can be executed between ADVANCE/SET_PC_FROM_OFFSET and DISPATCH.
// We want those two macros as far apart as possible, because the goto in
// DISPATCH is dependent on a memory load in ADVANCE/SET_PC_FROM_OFFSET. If we
// don't hit the cache and have to fetch the next handler address from physical
// memory, instructions between ADVANCE/SET_PC_FROM_OFFSET and DISPATCH can
// potentially be executed unconditionally, reducing memory stall.
#define ADVANCE(name)                             \
  next_pc = pc + RegExpBytecodeLength(BC_##name); \
  DECODE()
#define SET_PC_FROM_OFFSET(offset) \
  next_pc = code_base + offset;    \
  DECODE()

#ifdef DEBUG
#define BYTECODE(name)                                                \
  BC_LABEL(name)                                                      \
  MaybeTraceInterpreter(code_base, pc, backtrack_stack.sp(), current, \
                        current_char, RegExpBytecodeLength(BC_##name), #name);
#else
#define BYTECODE(name) BC_LABEL(name)
#endif  // DEBUG

template <typename Char>
IrregexpInterpreter::Result RawMatch(Isolate* isolate, ByteArray code_array,
                                     String subject_string,
                                     Vector<const Char> subject, int* registers,
                                     int current, uint32_t current_char,
                                     RegExp::CallOrigin call_origin) {
  DisallowHeapAllocation no_gc;

#if V8_USE_COMPUTED_GOTO
#define DECLARE_DISPATCH_TABLE_ENTRY(name, code, length) &&BC_##name,
  static const void* const dispatch_table[] = {
      BYTECODE_ITERATOR(DECLARE_DISPATCH_TABLE_ENTRY)};
#undef DECLARE_DISPATCH_TABLE_ENTRY
#endif

  const byte* pc = code_array.GetDataStartAddress();
  const byte* code_base = pc;

  BacktrackStack backtrack_stack;

#ifdef DEBUG
  if (FLAG_trace_regexp_bytecodes) {
    PrintF("\n\nStart bytecode interpreter\n\n");
  }
#endif

  while (true) {
    const byte* next_pc = pc;
    int32_t insn;
    int32_t next_insn;
#if V8_USE_COMPUTED_GOTO
    const void* next_handler_addr;
    DECODE();
    DISPATCH();
#else
    insn = Load32Aligned(pc);
    switch (insn & BYTECODE_MASK) {
#endif  // V8_USE_COMPUTED_GOTO
    BYTECODE(BREAK) { UNREACHABLE(); }
    BYTECODE(PUSH_CP) {
      ADVANCE(PUSH_CP);
      backtrack_stack.push(current);
      DISPATCH();
    }
    BYTECODE(PUSH_BT) {
      ADVANCE(PUSH_BT);
      backtrack_stack.push(Load32Aligned(pc + 4));
      DISPATCH();
    }
    BYTECODE(PUSH_REGISTER) {
      ADVANCE(PUSH_REGISTER);
      backtrack_stack.push(registers[insn >> BYTECODE_SHIFT]);
      DISPATCH();
    }
    BYTECODE(SET_REGISTER) {
      ADVANCE(SET_REGISTER);
      registers[insn >> BYTECODE_SHIFT] = Load32Aligned(pc + 4);
      DISPATCH();
    }
    BYTECODE(ADVANCE_REGISTER) {
      ADVANCE(ADVANCE_REGISTER);
      registers[insn >> BYTECODE_SHIFT] += Load32Aligned(pc + 4);
      DISPATCH();
    }
    BYTECODE(SET_REGISTER_TO_CP) {
      ADVANCE(SET_REGISTER_TO_CP);
      registers[insn >> BYTECODE_SHIFT] = current + Load32Aligned(pc + 4);
      DISPATCH();
    }
    BYTECODE(SET_CP_TO_REGISTER) {
      ADVANCE(SET_CP_TO_REGISTER);
      current = registers[insn >> BYTECODE_SHIFT];
      DISPATCH();
    }
    BYTECODE(SET_REGISTER_TO_SP) {
      ADVANCE(SET_REGISTER_TO_SP);
      registers[insn >> BYTECODE_SHIFT] = backtrack_stack.sp();
      DISPATCH();
    }
    BYTECODE(SET_SP_TO_REGISTER) {
      ADVANCE(SET_SP_TO_REGISTER);
      backtrack_stack.set_sp(registers[insn >> BYTECODE_SHIFT]);
      DISPATCH();
    }
    BYTECODE(POP_CP) {
      ADVANCE(POP_CP);
      current = backtrack_stack.pop();
      DISPATCH();
    }
    BYTECODE(POP_BT) {
      IrregexpInterpreter::Result return_code =
          HandleInterrupts(isolate, call_origin, &code_array, &subject_string,
                           &code_base, &subject, &pc);
      if (return_code != IrregexpInterpreter::SUCCESS) return return_code;

      SET_PC_FROM_OFFSET(backtrack_stack.pop());
      DISPATCH();
    }
    BYTECODE(POP_REGISTER) {
      ADVANCE(POP_REGISTER);
      registers[insn >> BYTECODE_SHIFT] = backtrack_stack.pop();
      DISPATCH();
    }
    BYTECODE(FAIL) { return IrregexpInterpreter::FAILURE; }
    BYTECODE(SUCCEED) { return IrregexpInterpreter::SUCCESS; }
    BYTECODE(ADVANCE_CP) {
      ADVANCE(ADVANCE_CP);
      current += insn >> BYTECODE_SHIFT;
      DISPATCH();
    }
    BYTECODE(GOTO) {
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      DISPATCH();
    }
    BYTECODE(ADVANCE_CP_AND_GOTO) {
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      current += insn >> BYTECODE_SHIFT;
      DISPATCH();
    }
    BYTECODE(CHECK_GREEDY) {
      if (current == backtrack_stack.peek()) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
        backtrack_stack.pop();
      } else {
        ADVANCE(CHECK_GREEDY);
      }
      DISPATCH();
    }
    BYTECODE(LOAD_CURRENT_CHAR) {
      int pos = current + (insn >> BYTECODE_SHIFT);
      if (pos >= subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(LOAD_CURRENT_CHAR);
        current_char = subject[pos];
      }
      DISPATCH();
    }
    BYTECODE(LOAD_CURRENT_CHAR_UNCHECKED) {
      ADVANCE(LOAD_CURRENT_CHAR_UNCHECKED);
      int pos = current + (insn >> BYTECODE_SHIFT);
      current_char = subject[pos];
      DISPATCH();
    }
    BYTECODE(LOAD_2_CURRENT_CHARS) {
      int pos = current + (insn >> BYTECODE_SHIFT);
      if (pos + 2 > subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(LOAD_2_CURRENT_CHARS);
        Char next = subject[pos + 1];
        current_char = (subject[pos] | (next << (kBitsPerByte * sizeof(Char))));
      }
      DISPATCH();
    }
    BYTECODE(LOAD_2_CURRENT_CHARS_UNCHECKED) {
      ADVANCE(LOAD_2_CURRENT_CHARS_UNCHECKED);
      int pos = current + (insn >> BYTECODE_SHIFT);
      Char next = subject[pos + 1];
      current_char = (subject[pos] | (next << (kBitsPerByte * sizeof(Char))));
      DISPATCH();
    }
    BYTECODE(LOAD_4_CURRENT_CHARS) {
      DCHECK_EQ(1, sizeof(Char));
      int pos = current + (insn >> BYTECODE_SHIFT);
      if (pos + 4 > subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(LOAD_4_CURRENT_CHARS);
        Char next1 = subject[pos + 1];
        Char next2 = subject[pos + 2];
        Char next3 = subject[pos + 3];
        current_char =
            (subject[pos] | (next1 << 8) | (next2 << 16) | (next3 << 24));
      }
      DISPATCH();
    }
    BYTECODE(LOAD_4_CURRENT_CHARS_UNCHECKED) {
      ADVANCE(LOAD_4_CURRENT_CHARS_UNCHECKED);
      DCHECK_EQ(1, sizeof(Char));
      int pos = current + (insn >> BYTECODE_SHIFT);
      Char next1 = subject[pos + 1];
      Char next2 = subject[pos + 2];
      Char next3 = subject[pos + 3];
      current_char =
          (subject[pos] | (next1 << 8) | (next2 << 16) | (next3 << 24));
      DISPATCH();
    }
    BYTECODE(CHECK_4_CHARS) {
      uint32_t c = Load32Aligned(pc + 4);
      if (c == current_char) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(CHECK_4_CHARS);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_CHAR) {
      uint32_t c = (insn >> BYTECODE_SHIFT);
      if (c == current_char) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_CHAR);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_4_CHARS) {
      uint32_t c = Load32Aligned(pc + 4);
      if (c != current_char) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(CHECK_NOT_4_CHARS);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_CHAR) {
      uint32_t c = (insn >> BYTECODE_SHIFT);
      if (c != current_char) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_NOT_CHAR);
      }
      DISPATCH();
    }
    BYTECODE(AND_CHECK_4_CHARS) {
      uint32_t c = Load32Aligned(pc + 4);
      if (c == (current_char & Load32Aligned(pc + 8))) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 12));
      } else {
        ADVANCE(AND_CHECK_4_CHARS);
      }
      DISPATCH();
    }
    BYTECODE(AND_CHECK_CHAR) {
      uint32_t c = (insn >> BYTECODE_SHIFT);
      if (c == (current_char & Load32Aligned(pc + 4))) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(AND_CHECK_CHAR);
      }
      DISPATCH();
    }
    BYTECODE(AND_CHECK_NOT_4_CHARS) {
      uint32_t c = Load32Aligned(pc + 4);
      if (c != (current_char & Load32Aligned(pc + 8))) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 12));
      } else {
        ADVANCE(AND_CHECK_NOT_4_CHARS);
      }
      DISPATCH();
    }
    BYTECODE(AND_CHECK_NOT_CHAR) {
      uint32_t c = (insn >> BYTECODE_SHIFT);
      if (c != (current_char & Load32Aligned(pc + 4))) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(AND_CHECK_NOT_CHAR);
      }
      DISPATCH();
    }
    BYTECODE(MINUS_AND_CHECK_NOT_CHAR) {
      uint32_t c = (insn >> BYTECODE_SHIFT);
      uint32_t minus = Load16Aligned(pc + 4);
      uint32_t mask = Load16Aligned(pc + 6);
      if (c != ((current_char - minus) & mask)) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(MINUS_AND_CHECK_NOT_CHAR);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_CHAR_IN_RANGE) {
      uint32_t from = Load16Aligned(pc + 4);
      uint32_t to = Load16Aligned(pc + 6);
      if (from <= current_char && current_char <= to) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(CHECK_CHAR_IN_RANGE);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_CHAR_NOT_IN_RANGE) {
      uint32_t from = Load16Aligned(pc + 4);
      uint32_t to = Load16Aligned(pc + 6);
      if (from > current_char || current_char > to) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(CHECK_CHAR_NOT_IN_RANGE);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_BIT_IN_TABLE) {
      int mask = RegExpMacroAssembler::kTableMask;
      byte b = pc[8 + ((current_char & mask) >> kBitsPerByteLog2)];
      int bit = (current_char & (kBitsPerByte - 1));
      if ((b & (1 << bit)) != 0) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_BIT_IN_TABLE);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_LT) {
      uint32_t limit = (insn >> BYTECODE_SHIFT);
      if (current_char < limit) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_LT);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_GT) {
      uint32_t limit = (insn >> BYTECODE_SHIFT);
      if (current_char > limit) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_GT);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_REGISTER_LT) {
      if (registers[insn >> BYTECODE_SHIFT] < Load32Aligned(pc + 4)) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(CHECK_REGISTER_LT);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_REGISTER_GE) {
      if (registers[insn >> BYTECODE_SHIFT] >= Load32Aligned(pc + 4)) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      } else {
        ADVANCE(CHECK_REGISTER_GE);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_REGISTER_EQ_POS) {
      if (registers[insn >> BYTECODE_SHIFT] == current) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_REGISTER_EQ_POS);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_REGS_EQUAL) {
      if (registers[insn >> BYTECODE_SHIFT] ==
          registers[Load32Aligned(pc + 4)]) {
        ADVANCE(CHECK_NOT_REGS_EQUAL);
      } else {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
      }
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_BACK_REF) {
      int from = registers[insn >> BYTECODE_SHIFT];
      int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
      if (from >= 0 && len > 0) {
        if (current + len > subject.length() ||
            CompareChars(&subject[from], &subject[current], len) != 0) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
          DISPATCH();
        }
        current += len;
      }
      ADVANCE(CHECK_NOT_BACK_REF);
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_BACK_REF_BACKWARD) {
      int from = registers[insn >> BYTECODE_SHIFT];
      int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
      if (from >= 0 && len > 0) {
        if (current - len < 0 ||
            CompareChars(&subject[from], &subject[current - len], len) != 0) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
          DISPATCH();
        }
        current -= len;
      }
      ADVANCE(CHECK_NOT_BACK_REF_BACKWARD);
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_UNICODE) {
      int from = registers[insn >> BYTECODE_SHIFT];
      int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
      if (from >= 0 && len > 0) {
        if (current + len > subject.length() ||
            !BackRefMatchesNoCase(isolate, from, current, len, subject, true)) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
          DISPATCH();
        }
        current += len;
      }
      ADVANCE(CHECK_NOT_BACK_REF_NO_CASE_UNICODE);
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_BACK_REF_NO_CASE) {
      int from = registers[insn >> BYTECODE_SHIFT];
      int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
      if (from >= 0 && len > 0) {
        if (current + len > subject.length() ||
            !BackRefMatchesNoCase(isolate, from, current, len, subject,
                                  false)) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
          DISPATCH();
        }
        current += len;
      }
      ADVANCE(CHECK_NOT_BACK_REF_NO_CASE);
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_UNICODE_BACKWARD) {
      int from = registers[insn >> BYTECODE_SHIFT];
      int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
      if (from >= 0 && len > 0) {
        if (current - len < 0 ||
            !BackRefMatchesNoCase(isolate, from, current - len, len, subject,
                                  true)) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
          DISPATCH();
        }
        current -= len;
      }
      ADVANCE(CHECK_NOT_BACK_REF_NO_CASE_UNICODE_BACKWARD);
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_BACK_REF_NO_CASE_BACKWARD) {
      int from = registers[insn >> BYTECODE_SHIFT];
      int len = registers[(insn >> BYTECODE_SHIFT) + 1] - from;
      if (from >= 0 && len > 0) {
        if (current - len < 0 ||
            !BackRefMatchesNoCase(isolate, from, current - len, len, subject,
                                  false)) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
          DISPATCH();
        }
        current -= len;
      }
      ADVANCE(CHECK_NOT_BACK_REF_NO_CASE_BACKWARD);
      DISPATCH();
    }
    BYTECODE(CHECK_AT_START) {
      if (current + (insn >> BYTECODE_SHIFT) == 0) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_AT_START);
      }
      DISPATCH();
    }
    BYTECODE(CHECK_NOT_AT_START) {
      if (current + (insn >> BYTECODE_SHIFT) == 0) {
        ADVANCE(CHECK_NOT_AT_START);
      } else {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      }
      DISPATCH();
    }
    BYTECODE(SET_CURRENT_POSITION_FROM_END) {
      ADVANCE(SET_CURRENT_POSITION_FROM_END);
      int by = static_cast<uint32_t>(insn) >> BYTECODE_SHIFT;
      if (subject.length() - current > by) {
        current = subject.length() - by;
        current_char = subject[current - 1];
      }
      DISPATCH();
    }
    BYTECODE(CHECK_CURRENT_POSITION) {
      int pos = current + (insn >> BYTECODE_SHIFT);
      if (pos > subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(Load32Aligned(pc + 4));
      } else {
        ADVANCE(CHECK_CURRENT_POSITION);
      }
      DISPATCH();
    }
#if V8_USE_COMPUTED_GOTO
// Lint gets confused a lot if we just use !V8_USE_COMPUTED_GOTO or ifndef
// V8_USE_COMPUTED_GOTO here.
#else
      default:
        UNREACHABLE();
    }
#endif  // V8_USE_COMPUTED_GOTO
  }
}

#undef BYTECODE
#undef DISPATCH
#undef DECODE
#undef SET_PC_FROM_OFFSET
#undef ADVANCE
#undef BC_LABEL
#undef V8_USE_COMPUTED_GOTO

}  // namespace

// static
void IrregexpInterpreter::Disassemble(ByteArray byte_array,
                                      const std::string& pattern) {
  DisallowHeapAllocation no_gc;

  PrintF("[generated bytecode for regexp pattern: '%s']\n", pattern.c_str());

  const byte* const code_base = byte_array.GetDataStartAddress();
  const int byte_array_length = byte_array.length();
  ptrdiff_t offset = 0;

  while (offset < byte_array_length) {
    const byte* const pc = code_base + offset;
    PrintF("%p  %4" V8PRIxPTRDIFF "  ", pc, offset);
    DisassembleSingleBytecode(code_base, pc);
    offset += RegExpBytecodeLength(*pc);
  }
}

// static
IrregexpInterpreter::Result IrregexpInterpreter::Match(
    Isolate* isolate, JSRegExp regexp, String subject_string, int* registers,
    int registers_length, int start_position, RegExp::CallOrigin call_origin) {
  if (FLAG_regexp_tier_up) {
    regexp.MarkTierUpForNextExec();
  }

  bool is_one_byte = String::IsOneByteRepresentationUnderneath(subject_string);
  ByteArray code_array = ByteArray::cast(regexp.Bytecode(is_one_byte));

  return MatchInternal(isolate, code_array, subject_string, registers,
                       registers_length, start_position, call_origin);
}

IrregexpInterpreter::Result IrregexpInterpreter::MatchInternal(
    Isolate* isolate, ByteArray code_array, String subject_string,
    int* registers, int registers_length, int start_position,
    RegExp::CallOrigin call_origin) {
  DCHECK(subject_string.IsFlat());

  // Note: Heap allocation *is* allowed in two situations if calling from
  // Runtime:
  // 1. When creating & throwing a stack overflow exception. The interpreter
  //    aborts afterwards, and thus possible-moved objects are never used.
  // 2. When handling interrupts. We manually relocate unhandlified references
  //    after interrupts have run.
  DisallowHeapAllocation no_gc;

  // Reset registers to -1 (=undefined).
  // This is necessary because registers are only written when a
  // capture group matched.
  // Resetting them ensures that previous matches are cleared.
  memset(registers, -1, sizeof(registers[0]) * registers_length);

  uc16 previous_char = '\n';
  String::FlatContent subject_content = subject_string.GetFlatContent(no_gc);
  if (subject_content.IsOneByte()) {
    Vector<const uint8_t> subject_vector = subject_content.ToOneByteVector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate, code_array, subject_string, subject_vector,
                    registers, start_position, previous_char, call_origin);
  } else {
    DCHECK(subject_content.IsTwoByte());
    Vector<const uc16> subject_vector = subject_content.ToUC16Vector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate, code_array, subject_string, subject_vector,
                    registers, start_position, previous_char, call_origin);
  }
}

// This method is called through an external reference from RegExpExecInternal
// builtin.
IrregexpInterpreter::Result IrregexpInterpreter::MatchForCallFromJs(
    Address subject, int32_t start_position, Address, Address, int* registers,
    int32_t registers_length, Address, RegExp::CallOrigin call_origin,
    Isolate* isolate, Address regexp) {
  DCHECK_NOT_NULL(isolate);
  DCHECK_NOT_NULL(registers);
  DCHECK(call_origin == RegExp::CallOrigin::kFromJs);

  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate);

  String subject_string = String::cast(Object(subject));
  JSRegExp regexp_obj = JSRegExp::cast(Object(regexp));

  return Match(isolate, regexp_obj, subject_string, registers, registers_length,
               start_position, call_origin);
}

IrregexpInterpreter::Result IrregexpInterpreter::MatchForCallFromRuntime(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject_string,
    int* registers, int registers_length, int start_position) {
  return Match(isolate, *regexp, *subject_string, registers, registers_length,
               start_position, RegExp::CallOrigin::kFromRuntime);
}

}  // namespace internal
}  // namespace v8

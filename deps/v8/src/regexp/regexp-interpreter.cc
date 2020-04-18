// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#include "src/regexp/regexp-interpreter.h"

#include "src/ast/ast.h"
#include "src/base/small-vector.h"
#include "src/logging/counters.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/objects-inl.h"
#include "src/regexp/regexp-bytecodes.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"  // For kMaximumStackSize.
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

    RegExpBytecodeDisassembleSingle(code_base, pc);
  }
}
#endif  // DEBUG

int32_t Load32Aligned(const byte* pc) {
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(pc) & 3);
  return *reinterpret_cast<const int32_t*>(pc);
}

// TODO(jgruber): Rename to Load16AlignedUnsigned.
uint32_t Load16Aligned(const byte* pc) {
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(pc) & 1);
  return *reinterpret_cast<const uint16_t*>(pc);
}

int32_t Load16AlignedSigned(const byte* pc) {
  DCHECK_EQ(0, reinterpret_cast<intptr_t>(pc) & 1);
  return *reinterpret_cast<const int16_t*>(pc);
}

// A simple abstraction over the backtracking stack used by the interpreter.
//
// Despite the name 'backtracking' stack, it's actually used as a generic stack
// that stores both program counters (= offsets into the bytecode) and generic
// integer values.
class BacktrackStack {
 public:
  BacktrackStack() = default;

  V8_WARN_UNUSED_RESULT bool push(int v) {
    data_.emplace_back(v);
    return (static_cast<int>(data_.size()) <= kMaxSize);
  }
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

  using ValueT = int;
  base::SmallVector<ValueT, kStaticCapacity> data_;

  static constexpr int kMaxSize =
      RegExpStack::kMaximumStackSize / sizeof(ValueT);

  DISALLOW_COPY_AND_ASSIGN(BacktrackStack);
};

IrregexpInterpreter::Result ThrowStackOverflow(Isolate* isolate,
                                               RegExp::CallOrigin call_origin) {
  CHECK(call_origin == RegExp::CallOrigin::kFromRuntime);
  // We abort interpreter execution after the stack overflow is thrown, and thus
  // allow allocation here despite the outer DisallowHeapAllocationScope.
  AllowHeapAllocation yes_gc;
  isolate->StackOverflow();
  return IrregexpInterpreter::EXCEPTION;
}

// Only throws if called from the runtime, otherwise just returns the EXCEPTION
// status code.
IrregexpInterpreter::Result MaybeThrowStackOverflow(
    Isolate* isolate, RegExp::CallOrigin call_origin) {
  if (call_origin == RegExp::CallOrigin::kFromRuntime) {
    return ThrowStackOverflow(isolate, call_origin);
  } else {
    return IrregexpInterpreter::EXCEPTION;
  }
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
      return ThrowStackOverflow(isolate, call_origin);
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

bool CheckBitInTable(const uint32_t current_char, const byte* const table) {
  int mask = RegExpMacroAssembler::kTableMask;
  int b = table[(current_char & mask) >> kBitsPerByteLog2];
  int bit = (current_char & (kBitsPerByte - 1));
  return (b & (1 << bit)) != 0;
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
  goto switch_dispatch_continuation
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
                                     RegExp::CallOrigin call_origin,
                                     const uint32_t backtrack_limit) {
  DisallowHeapAllocation no_gc;

#if V8_USE_COMPUTED_GOTO

// We have to make sure that no OOB access to the dispatch table is possible and
// all values are valid label addresses.
// Otherwise jumps to arbitrary addresses could potentially happen.
// This is ensured as follows:
// Every index to the dispatch table gets masked using BYTECODE_MASK in
// DECODE(). This way we can only get values between 0 (only the least
// significant byte of an integer is used) and kRegExpPaddedBytecodeCount - 1
// (BYTECODE_MASK is defined to be exactly this value).
// All entries from kRegExpBytecodeCount to kRegExpPaddedBytecodeCount have to
// be filled with BREAKs (invalid operation).

// Fill dispatch table from last defined bytecode up to the next power of two
// with BREAK (invalid operation).
// TODO(pthier): Find a way to fill up automatically (at compile time)
// 59 real bytecodes -> 5 fillers
#define BYTECODE_FILLER_ITERATOR(V) \
  V(BREAK) /* 1 */                  \
  V(BREAK) /* 2 */                  \
  V(BREAK) /* 3 */                  \
  V(BREAK) /* 4 */                  \
  V(BREAK) /* 5 */

#define COUNT(...) +1
  static constexpr int kRegExpBytecodeFillerCount =
      BYTECODE_FILLER_ITERATOR(COUNT);
#undef COUNT

  // Make sure kRegExpPaddedBytecodeCount is actually the closest possible power
  // of two.
  DCHECK_EQ(kRegExpPaddedBytecodeCount,
            base::bits::RoundUpToPowerOfTwo32(kRegExpBytecodeCount));

  // Make sure every bytecode we get by using BYTECODE_MASK is well defined.
  STATIC_ASSERT(kRegExpBytecodeCount <= kRegExpPaddedBytecodeCount);
  STATIC_ASSERT(kRegExpBytecodeCount + kRegExpBytecodeFillerCount ==
                kRegExpPaddedBytecodeCount);

#define DECLARE_DISPATCH_TABLE_ENTRY(name, ...) &&BC_##name,
  static const void* const dispatch_table[kRegExpPaddedBytecodeCount] = {
      BYTECODE_ITERATOR(DECLARE_DISPATCH_TABLE_ENTRY)
          BYTECODE_FILLER_ITERATOR(DECLARE_DISPATCH_TABLE_ENTRY)};
#undef DECLARE_DISPATCH_TABLE_ENTRY
#undef BYTECODE_FILLER_ITERATOR

#endif  // V8_USE_COMPUTED_GOTO

  const byte* pc = code_array.GetDataStartAddress();
  const byte* code_base = pc;

  BacktrackStack backtrack_stack;

  uint32_t backtrack_count = 0;

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
      if (!backtrack_stack.push(current)) {
        return MaybeThrowStackOverflow(isolate, call_origin);
      }
      DISPATCH();
    }
    BYTECODE(PUSH_BT) {
      ADVANCE(PUSH_BT);
      if (!backtrack_stack.push(Load32Aligned(pc + 4))) {
        return MaybeThrowStackOverflow(isolate, call_origin);
      }
      DISPATCH();
    }
    BYTECODE(PUSH_REGISTER) {
      ADVANCE(PUSH_REGISTER);
      if (!backtrack_stack.push(registers[insn >> BYTECODE_SHIFT])) {
        return MaybeThrowStackOverflow(isolate, call_origin);
      }
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
      STATIC_ASSERT(JSRegExp::kNoBacktrackLimit == 0);
      if (++backtrack_count == backtrack_limit) {
        // Exceeded limits are treated as a failed match.
        return IrregexpInterpreter::FAILURE;
      }

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
    BYTECODE(FAIL) {
      isolate->counters()->regexp_backtracks()->AddSample(
          static_cast<int>(backtrack_count));
      return IrregexpInterpreter::FAILURE;
    }
    BYTECODE(SUCCEED) {
      isolate->counters()->regexp_backtracks()->AddSample(
          static_cast<int>(backtrack_count));
      return IrregexpInterpreter::SUCCESS;
    }
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
      if (CheckBitInTable(current_char, pc + 8)) {
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
    BYTECODE(SKIP_UNTIL_CHAR) {
      int load_offset = (insn >> BYTECODE_SHIFT);
      int32_t advance = Load16AlignedSigned(pc + 4);
      uint32_t c = Load16Aligned(pc + 6);
      while (static_cast<uintptr_t>(current + load_offset) <
             static_cast<uintptr_t>(subject.length())) {
        current_char = subject[current + load_offset];
        if (c == current_char) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 8));
          DISPATCH();
        }
        current += advance;
      }
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 12));
      DISPATCH();
    }
    BYTECODE(SKIP_UNTIL_CHAR_AND) {
      int load_offset = (insn >> BYTECODE_SHIFT);
      int32_t advance = Load16AlignedSigned(pc + 4);
      uint16_t c = Load16Aligned(pc + 6);
      uint32_t mask = Load32Aligned(pc + 8);
      int32_t maximum_offset = Load32Aligned(pc + 12);
      while (static_cast<uintptr_t>(current + maximum_offset) <=
             static_cast<uintptr_t>(subject.length())) {
        current_char = subject[current + load_offset];
        if (c == (current_char & mask)) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 16));
          DISPATCH();
        }
        current += advance;
      }
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 20));
      DISPATCH();
    }
    BYTECODE(SKIP_UNTIL_CHAR_POS_CHECKED) {
      int load_offset = (insn >> BYTECODE_SHIFT);
      int32_t advance = Load16AlignedSigned(pc + 4);
      uint16_t c = Load16Aligned(pc + 6);
      int32_t maximum_offset = Load32Aligned(pc + 8);
      while (static_cast<uintptr_t>(current + maximum_offset) <=
             static_cast<uintptr_t>(subject.length())) {
        current_char = subject[current + load_offset];
        if (c == current_char) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 12));
          DISPATCH();
        }
        current += advance;
      }
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 16));
      DISPATCH();
    }
    BYTECODE(SKIP_UNTIL_BIT_IN_TABLE) {
      int load_offset = (insn >> BYTECODE_SHIFT);
      int32_t advance = Load16AlignedSigned(pc + 4);
      const byte* table = pc + 8;
      while (static_cast<uintptr_t>(current + load_offset) <
             static_cast<uintptr_t>(subject.length())) {
        current_char = subject[current + load_offset];
        if (CheckBitInTable(current_char, table)) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 24));
          DISPATCH();
        }
        current += advance;
      }
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 28));
      DISPATCH();
    }
    BYTECODE(SKIP_UNTIL_GT_OR_NOT_BIT_IN_TABLE) {
      int load_offset = (insn >> BYTECODE_SHIFT);
      int32_t advance = Load16AlignedSigned(pc + 4);
      uint16_t limit = Load16Aligned(pc + 6);
      const byte* table = pc + 8;
      while (static_cast<uintptr_t>(current + load_offset) <
             static_cast<uintptr_t>(subject.length())) {
        current_char = subject[current + load_offset];
        if (current_char > limit) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 24));
          DISPATCH();
        }
        if (!CheckBitInTable(current_char, table)) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 24));
          DISPATCH();
        }
        current += advance;
      }
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 28));
      DISPATCH();
    }
    BYTECODE(SKIP_UNTIL_CHAR_OR_CHAR) {
      int load_offset = (insn >> BYTECODE_SHIFT);
      int32_t advance = Load32Aligned(pc + 4);
      uint16_t c = Load16Aligned(pc + 8);
      uint16_t c2 = Load16Aligned(pc + 10);
      while (static_cast<uintptr_t>(current + load_offset) <
             static_cast<uintptr_t>(subject.length())) {
        current_char = subject[current + load_offset];
        // The two if-statements below are split up intentionally, as combining
        // them seems to result in register allocation behaving quite
        // differently and slowing down the resulting code.
        if (c == current_char) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 12));
          DISPATCH();
        }
        if (c2 == current_char) {
          SET_PC_FROM_OFFSET(Load32Aligned(pc + 12));
          DISPATCH();
        }
        current += advance;
      }
      SET_PC_FROM_OFFSET(Load32Aligned(pc + 16));
      DISPATCH();
    }
#if V8_USE_COMPUTED_GOTO
// Lint gets confused a lot if we just use !V8_USE_COMPUTED_GOTO or ifndef
// V8_USE_COMPUTED_GOTO here.
#else
      default:
        UNREACHABLE();
    }
  // Label we jump to in DISPATCH(). There must be no instructions between the
  // end of the switch, this label and the end of the loop.
  switch_dispatch_continuation : {}
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
IrregexpInterpreter::Result IrregexpInterpreter::Match(
    Isolate* isolate, JSRegExp regexp, String subject_string, int* registers,
    int registers_length, int start_position, RegExp::CallOrigin call_origin) {
  if (FLAG_regexp_tier_up) {
    regexp.TierUpTick();
  }

  bool is_one_byte = String::IsOneByteRepresentationUnderneath(subject_string);
  ByteArray code_array = ByteArray::cast(regexp.Bytecode(is_one_byte));

  return MatchInternal(isolate, code_array, subject_string, registers,
                       registers_length, start_position, call_origin,
                       regexp.BacktrackLimit());
}

IrregexpInterpreter::Result IrregexpInterpreter::MatchInternal(
    Isolate* isolate, ByteArray code_array, String subject_string,
    int* registers, int registers_length, int start_position,
    RegExp::CallOrigin call_origin, uint32_t backtrack_limit) {
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
                    registers, start_position, previous_char, call_origin,
                    backtrack_limit);
  } else {
    DCHECK(subject_content.IsTwoByte());
    Vector<const uc16> subject_vector = subject_content.ToUC16Vector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate, code_array, subject_string, subject_vector,
                    registers, start_position, previous_char, call_origin,
                    backtrack_limit);
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

  if (regexp_obj.MarkedForTierUp()) {
    // Returning RETRY will re-enter through runtime, where actual recompilation
    // for tier-up takes place.
    return IrregexpInterpreter::RETRY;
  }

  // In generated code, registers are allocated on the stack. The given
  // `registers` argument is only guaranteed to hold enough space for permanent
  // registers (i.e. for captures), and not for temporary registers used only
  // during matcher execution. We match that behavior in the interpreter by
  // using a SmallVector as internal register storage.
  static constexpr int kBaseRegisterArraySize = 64;  // Arbitrary.
  const int internal_register_count =
      Smi::ToInt(regexp_obj.DataAt(JSRegExp::kIrregexpMaxRegisterCountIndex));
  base::SmallVector<int, kBaseRegisterArraySize> internal_registers(
      internal_register_count);

  Result result =
      Match(isolate, regexp_obj, subject_string, internal_registers.data(),
            internal_register_count, start_position, call_origin);

  // Copy capture registers to the output array.
  if (result == IrregexpInterpreter::SUCCESS) {
    CHECK_GE(internal_registers.size(), registers_length);
    MemCopy(registers, internal_registers.data(),
            registers_length * sizeof(registers[0]));
  }

  return result;
}

IrregexpInterpreter::Result IrregexpInterpreter::MatchForCallFromRuntime(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject_string,
    int* registers, int registers_length, int start_position) {
  return Match(isolate, *regexp, *subject_string, registers, registers_length,
               start_position, RegExp::CallOrigin::kFromRuntime);
}

}  // namespace internal
}  // namespace v8

// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#include "src/regexp/regexp-interpreter.h"

#include <limits>

#include "src/base/small-vector.h"
#include "src/base/strings.h"
#include "src/execution/isolate.h"
#include "src/logging/counters.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/string-inl.h"
#include "src/regexp/regexp-bytecodes-inl.h"
#include "src/regexp/regexp-bytecodes.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"  // For kMaximumStackSize.
#include "src/regexp/regexp-utils.h"
#include "src/regexp/regexp.h"
#include "src/strings/unicode.h"
#include "src/utils/memcopy.h"
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
                          base::Vector<const base::uc16> subject,
                          bool unicode) {
  Address offset_a =
      reinterpret_cast<Address>(const_cast<base::uc16*>(&subject.at(from)));
  Address offset_b =
      reinterpret_cast<Address>(const_cast<base::uc16*>(&subject.at(current)));
  size_t length = len * base::kUC16Size;

  bool result = unicode
                    ? RegExpMacroAssembler::CaseInsensitiveCompareUnicode(
                          offset_a, offset_b, length, isolate)
                    : RegExpMacroAssembler::CaseInsensitiveCompareNonUnicode(
                          offset_a, offset_b, length, isolate);
  return result == 1;
}

bool BackRefMatchesNoCase(Isolate* isolate, int from, int current, int len,
                          base::Vector<const uint8_t> subject, bool unicode) {
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

#ifdef ENABLE_DISASSEMBLER
void MaybeTraceInterpreter(const uint8_t* code_base, const uint8_t* pc,
                           int stack_depth, int current_position,
                           uint32_t current_char, int bytecode_length,
                           const char* bytecode_name) {
  if (v8_flags.trace_regexp_bytecodes) {
    // The behaviour of std::isprint is undefined if the value isn't
    // representable as unsigned char.
    const bool is_single_char =
        current_char <= std::numeric_limits<unsigned char>::max();
    const bool printable = is_single_char ? std::isprint(current_char) : false;
    const char* format =
        printable
            ? "pc = %02x, sp = %d, curpos = %d, curchar = %08x (%c), bc = "
            : "pc = %02x, sp = %d, curpos = %d, curchar = %08x .%c., bc = ";
    PrintF(format, pc - code_base, stack_depth, current_position, current_char,
           printable ? current_char : '.');

    RegExpBytecodeDisassembleSingle(code_base, pc);
  }
}
#endif  // ENABLE_DISASSEMBLER

template <class Char>
constexpr int BitsPerChar() {
  return kBitsPerByte * sizeof(Char);
}

template <class Char>
uint32_t Load2Characters(const base::Vector<const Char>& string, int index) {
  return string[index] | (string[index + 1] << BitsPerChar<Char>());
}

uint32_t Load4Characters(const base::Vector<const uint8_t>& string, int index) {
  return string[index] | (string[index + 1] << 8) | (string[index + 2] << 16) |
         (string[index + 3] << 24);
}

uint32_t Load4Characters(const base::Vector<const base::uc16>&, int) {
  UNREACHABLE();
}

// A simple abstraction over the backtracking stack used by the interpreter.
//
// Despite the name 'backtracking' stack, it's actually used as a generic stack
// that stores both program counters (= offsets into the bytecode) and generic
// integer values.
class BacktrackStack {
 public:
  BacktrackStack() = default;
  BacktrackStack(const BacktrackStack&) = delete;
  BacktrackStack& operator=(const BacktrackStack&) = delete;

  V8_WARN_UNUSED_RESULT bool push(int v) {
    data_.emplace_back(v);
    return (static_cast<int>(data_.size()) <= kMaxSize);
  }
  int peek() const {
    SBXCHECK(!data_.empty());
    return data_.back();
  }
  int pop() {
    int v = peek();
    data_.pop_back();
    return v;
  }

  // The 'sp' is the index of the first empty element in the stack.
  int sp() const { return static_cast<int>(data_.size()); }
  void set_sp(uint32_t new_sp) {
    DCHECK_LE(new_sp, sp());
    data_.resize(new_sp);
  }

 private:
  // Semi-arbitrary. Should be large enough for common cases to remain in the
  // static stack-allocated backing store, but small enough not to waste space.
  static constexpr int kStaticCapacity = 64;

  using ValueT = int;
  base::SmallVector<ValueT, kStaticCapacity> data_;

  static constexpr int kMaxSize =
      RegExpStack::kMaximumStackSize / sizeof(ValueT);
};

// Registers used during interpreter execution. These consist of output
// registers in indices [0, output_register_count[ which will contain matcher
// results as a {start,end} index tuple for each capture (where the whole match
// counts as implicit capture 0); and internal registers in indices
// [output_register_count, total_register_count[.
class InterpreterRegisters {
 public:
  using RegisterT = int;
  static constexpr int kNoMatchValue = -1;

  InterpreterRegisters(int total_register_count, RegisterT* output_registers,
                       int output_register_count)
      : registers_(total_register_count),
        output_registers_(output_registers),
        total_register_count_(total_register_count),
        output_register_count_(output_register_count) {
    // TODO(jgruber): Use int32_t consistently for registers. Currently, CSA
    // uses int32_t while runtime uses int.
    static_assert(sizeof(int) == sizeof(int32_t));
    SBXCHECK_GE(output_register_count, 2);  // At least 2 for the match itself.
    SBXCHECK_GE(total_register_count, output_register_count);
    SBXCHECK_LE(total_register_count, RegExpMacroAssembler::kMaxRegisterCount);
    DCHECK_NOT_NULL(output_registers);

    // Initialize the output register region to -1 signifying 'no match'.
    std::memset(registers_.data(), kNoMatchValue,
                output_register_count * sizeof(RegisterT));
    USE(total_register_count_);
  }

  const RegisterT& operator[](size_t index) const {
    SBXCHECK_LT(index, total_register_count_);
    return registers_[index];
  }
  RegisterT& operator[](size_t index) {
    SBXCHECK_LT(index, total_register_count_);
    return registers_[index];
  }

  void CopyToOutputRegisters() {
    MemCopy(output_registers_, registers_.data(),
            output_register_count_ * sizeof(RegisterT));
  }

 private:
  static constexpr int kStaticCapacity = 64;  // Arbitrary.
  base::SmallVector<RegisterT, kStaticCapacity> registers_;
  RegisterT* const output_registers_;
  const int total_register_count_;
  const int output_register_count_;
};

IrregexpInterpreter::Result ThrowStackOverflow(Isolate* isolate,
                                               RegExp::CallOrigin call_origin) {
  CHECK(call_origin == RegExp::CallOrigin::kFromRuntime);
  // We abort interpreter execution after the stack overflow is thrown, and thus
  // allow allocation here despite the outer DisallowGarbageCollectionScope.
  AllowGarbageCollection yes_gc;
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
    Isolate* isolate, DirectHandle<TrustedByteArray> code_array,
    DirectHandle<String> subject_string,
    Tagged<TrustedByteArray>* code_array_out, const uint8_t** code_base_out,
    const uint8_t** pc_out, Tagged<String>* subject_string_out,
    base::Vector<const Char>* subject_string_vector_out) {
  DisallowGarbageCollection no_gc;

  if (*code_base_out != code_array->begin()) {
    *code_array_out = *code_array;
    const intptr_t pc_offset = *pc_out - *code_base_out;
    DCHECK_GT(pc_offset, 0);
    *code_base_out = code_array->begin();
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
    Isolate* isolate, RegExp::CallOrigin call_origin,
    Tagged<TrustedByteArray>* code_array_out,
    Tagged<String>* subject_string_out, const uint8_t** code_base_out,
    base::Vector<const Char>* subject_string_vector_out,
    const uint8_t** pc_out) {
  DisallowGarbageCollection no_gc;

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
    DirectHandle<TrustedByteArray> code_handle(*code_array_out, isolate);
    DirectHandle<String> subject_handle(*subject_string_out, isolate);

    if (js_has_overflowed) {
      return ThrowStackOverflow(isolate, call_origin);
    } else if (check.InterruptRequested()) {
      const bool was_one_byte =
          String::IsOneByteRepresentationUnderneath(*subject_string_out);
      Tagged<Object> result;
      {
        AllowGarbageCollection yes_gc;
        result = isolate->stack_guard()->HandleInterrupts();
      }
      if (IsExceptionHole(result, isolate)) {
        return IrregexpInterpreter::EXCEPTION;
      }

      // If we changed between a LATIN1 and a UC16 string, we need to
      // restart regexp matching with the appropriate template instantiation of
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

bool CheckBitInTable(const uint32_t current_char, const uint8_t* const table) {
  int mask = RegExpMacroAssembler::kTableMask;
  int b = table[(current_char & mask) >> kBitsPerByteLog2];
  int bit = (current_char & (kBitsPerByte - 1));
  return (b & (1 << bit)) != 0;
}

// Returns true iff 0 <= index < length.
bool IndexIsInBounds(int index, int length) {
  DCHECK_GE(length, 0);
  return static_cast<uintptr_t>(index) < static_cast<uintptr_t>(length);
}

// If computed gotos are supported by the compiler, we can get addresses to
// labels directly in C/C++. Every bytecode handler has its own label and we
// store the addresses in a dispatch table indexed by bytecode. To execute the
// next handler we simply jump (goto) directly to its address.
#if V8_USE_COMPUTED_GOTO
#define BC_LABEL(name) BC_k##name:
#define DECODE()                                                          \
  do {                                                                    \
    RegExpBytecode next_bc = RegExpBytecodes::FromPtr(next_pc);           \
    next_handler_addr =                                                   \
        dispatch_table[RegExpBytecodes::ToByte(next_bc) & kBytecodeMask]; \
  } while (false)
#define DISPATCH()  \
  pc = next_pc;     \
  goto* next_handler_addr
// Without computed goto support, we fall back to a simple switch-based
// dispatch (A large switch statement inside a loop with a case for every
// bytecode).
#else  // V8_USE_COMPUTED_GOTO
#define BC_LABEL(name) case RegExpBytecode::k##name:
#define DECODE() ((void)0)
#define DISPATCH()  \
  pc = next_pc;     \
  goto switch_dispatch_continuation
#endif  // V8_USE_COMPUTED_GOTO

// ADVANCE/SET_PC_FROM_OFFSET are separated from DISPATCH, because ideally some
// instructions can be executed between ADVANCE/SET_PC_FROM_OFFSET and DISPATCH.
// We want those two macros as far apart as possible, because the goto in
// DISPATCH is dependent on a memory load in ADVANCE/SET_PC_FROM_OFFSET. If we
// don't hit the cache and have to fetch the next handler address from physical
// memory, instructions between ADVANCE/SET_PC_FROM_OFFSET and DISPATCH can
// potentially be executed unconditionally, reducing memory stall.
#define ADVANCE()                                   \
  next_pc = pc + RegExpBytecodes::Size(current_bc); \
  DECODE()

#define SET_PC_FROM_OFFSET(offset) \
  next_pc = code_base + offset;    \
  DECODE()

// Current position mutations.
#define SET_CURRENT_POSITION(value)                        \
  do {                                                     \
    current = (value);                                     \
    DCHECK(base::IsInRange(current, 0, subject.length())); \
  } while (false)
#define ADVANCE_CURRENT_POSITION(by) SET_CURRENT_POSITION(current + (by))

// These weird looking macros are required for clang-format and cpplint to not
// interfere/complain about our logic of opening/closing blocks in our macros.
#define OPEN_BLOCK {
#define CLOSE_BLOCK }
#define BYTECODES_START() OPEN_BLOCK
#define BYTECODES_END() CLOSE_BLOCK

#ifdef ENABLE_DISASSEMBLER
#define BYTECODE(Name, ...)                                              \
  CLOSE_BLOCK                                                            \
  BC_LABEL(Name) OPEN_BLOCK INIT(Name __VA_OPT__(, ) __VA_ARGS__);       \
  MaybeTraceInterpreter(code_base, pc, backtrack_stack.sp(), current,    \
                        current_char, RegExpBytecodes::Size(current_bc), \
                        #Name);
#else
#define BYTECODE(Name, ...) \
  CLOSE_BLOCK               \
  BC_LABEL(Name) OPEN_BLOCK INIT(Name __VA_OPT__(, ) __VA_ARGS__);
#endif  // ENABLE_DISASSEMBLER

#define INIT(Name, ...)                                                     \
  constexpr RegExpBytecode current_bc = RegExpBytecode::k##Name;            \
  using Operands = RegExpBytecodeOperands<current_bc>;                      \
  __VA_OPT__(auto argument_tuple = std::apply(                              \
                 [&](auto... ops) {                                         \
                   return std::make_tuple(                                  \
                       Operands::template Get<ops.value>(pc, no_gc)...);    \
                 },                                                         \
                 Operands::GetOperandsTuple());                             \
             auto [__VA_ARGS__] = argument_tuple;)                          \
  static_assert((IS_VA_EMPTY(__VA_ARGS__)) == (Operands::kCount == 0),      \
                "Number of arguments to VISIT doesn't match the bytecodes " \
                "operands count")

namespace {

template <typename Char>
bool CheckSpecialClassRanges(uint32_t current_char,
                             StandardCharacterSet character_set) {
  constexpr bool is_one_byte = sizeof(Char) == 1;
  switch (character_set) {
    case StandardCharacterSet::kWhitespace:
      DCHECK(is_one_byte);
      if (current_char == ' ' || base::IsInRange(current_char, '\t', '\r') ||
          current_char == 0xA0) {
        return true;
      }
      return false;
    case StandardCharacterSet::kNotWhitespace:
      UNREACHABLE();
    case StandardCharacterSet::kWord: {
      if constexpr (!is_one_byte) {
        if (current_char > 'z') {
          return false;
        }
      }
      base::Vector<const uint8_t> word_character_map =
          RegExpMacroAssembler::word_character_map();
      DCHECK_EQ(0,
                word_character_map[0]);  // Character '\0' is not a word char.
      return word_character_map[current_char] != 0;
      return true;
    }
    case StandardCharacterSet::kNotWord: {
      if constexpr (!is_one_byte) {
        if (current_char > 'z') {
          return true;
        }
      }
      base::Vector<const uint8_t> word_character_map =
          RegExpMacroAssembler::word_character_map();
      DCHECK_EQ(0,
                word_character_map[0]);  // Character '\0' is not a word char.
      return word_character_map[current_char] == 0;
    }
    case StandardCharacterSet::kDigit:
      if (base::IsInRange(current_char, '0', '9')) {
        return true;
      }
      return false;
    case StandardCharacterSet::kNotDigit:
      if (base::IsInRange(current_char, '0', '9')) {
        return false;
      }
      return true;
    case StandardCharacterSet::kLineTerminator: {
      if (current_char == '\n' || current_char == '\r') {
        return true;
      }
      if constexpr (!is_one_byte) {
        if (current_char == 0x2028 || current_char == 0x2029) {
          return true;
        }
      }
      return false;
    }
    case StandardCharacterSet::kNotLineTerminator: {
      const bool is_one_byte_match =
          current_char != '\n' && current_char != '\r';
      if constexpr (is_one_byte) {
        if (is_one_byte_match) {
          return true;
        }
      } else {
        if (is_one_byte_match && current_char != 0x2028 &&
            current_char != 0x2029) {
          return true;
        }
      }
      return false;
    }
    case StandardCharacterSet::kEverything:
      return true;
  }
}

}  // namespace

template <typename Char>
IrregexpInterpreter::Result RawMatch(
    Isolate* isolate, Tagged<TrustedByteArray>* code_array,
    Tagged<String>* subject_string, base::Vector<const Char> subject,
    int* output_registers, int output_register_count, int total_register_count,
    int current, uint32_t current_char, RegExp::CallOrigin call_origin,
    const uint32_t backtrack_limit) {
  DisallowGarbageCollection no_gc;

#if V8_USE_COMPUTED_GOTO

  // Maximum number of bytecodes that will be used (next power of 2 of actually
  // defined bytecodes).
  // All slots between the last actually defined bytecode and maximum id will be
  // filled with kBreaks, indicating an invalid operation. This way using
  // kBytecodeMask guarantees no OOB access to the dispatch table.
  constexpr int kPaddedBytecodeCount =
      base::bits::RoundUpToPowerOfTwo32(RegExpBytecodes::kCount);
  constexpr int kBytecodeMask = kPaddedBytecodeCount - 1;
  static_assert(std::numeric_limits<uint8_t>::max() >= kBytecodeMask);

  // We have to make sure that no OOB access to the dispatch table is possible
  // and all values are valid label addresses. Otherwise jumps to arbitrary
  // addresses could potentially happen. This is ensured as follows: Every index
  // to the dispatch table gets masked using kBytecodeMask in DECODE(). This way
  // we can only get values between 0 (only the least significant byte of an
  // integer is used) and kPaddedBytecodeCount - 1 (kBytecodeMask is defined to
  // be exactly this value). All entries from RegExpBytecodes::kCount to
  // kRegExpPaddedBytecodeCount are automatically filled with kBreak (invalid
  // operation).

#define DECLARE_DISPATCH_TABLE_ENTRY(name, ...) &&BC_k##name,
  static const void* const unsafe_dispatch_table[RegExpBytecodes::kCount] = {
      REGEXP_BYTECODE_LIST(DECLARE_DISPATCH_TABLE_ENTRY)};
#undef DECLARE_DISPATCH_TABLE_ENTRY
#undef BYTECODE_FILLER_ITERATOR

  static const void* const filler_entry = &&BC_kBreak;
  static const std::array<const void*, kPaddedBytecodeCount> dispatch_table =
      [=]() {
        std::array<const void*, kPaddedBytecodeCount> table;

        size_t i = 0;
        // Copy all valid Bytecodes to the dispatch table.
        for (; i < RegExpBytecodes::kCount; ++i) {
          table[i] = unsafe_dispatch_table[i];
        }
        // Fill dispatch table from last defined bytecode up to the next power
        // of two with kBreak (invalid operation).
        for (; i < kPaddedBytecodeCount; ++i) {
          table[i] = filler_entry;
        }
        return table;
      }();

#endif  // V8_USE_COMPUTED_GOTO

  const uint8_t* pc = (*code_array)->begin();
  const uint8_t* code_base = pc;

  InterpreterRegisters registers(total_register_count, output_registers,
                                 output_register_count);
  BacktrackStack backtrack_stack;

  uint32_t backtrack_count = 0;

#ifdef ENABLE_DISASSEMBLER
  if (v8_flags.trace_regexp_bytecodes) {
    PrintF("\n\nStart bytecode interpreter\n\n");
  }
#endif

  while (true) {
    const uint8_t* next_pc = pc;
#if V8_USE_COMPUTED_GOTO
    const void* next_handler_addr;
    DECODE();
    DISPATCH();
#else
    switch (RegExpBytecodes::FromPtr(pc)) {
#endif  // V8_USE_COMPUTED_GOTO
    BYTECODES_START()
    BYTECODE(Break) { UNREACHABLE(); }
    BYTECODE(PushCurrentPosition) {
      ADVANCE();
      if (!backtrack_stack.push(current)) {
        return MaybeThrowStackOverflow(isolate, call_origin);
      }
      DISPATCH();
    }
    BYTECODE(PushBacktrack, label) {
      ADVANCE();
      if (!backtrack_stack.push(label)) {
        return MaybeThrowStackOverflow(isolate, call_origin);
      }
      DISPATCH();
    }
    BYTECODE(PushRegister, register_index, stack_check) {
      ADVANCE();
      USE(stack_check);  // Unused in interpreter.
      if (!backtrack_stack.push(registers[register_index])) {
        return MaybeThrowStackOverflow(isolate, call_origin);
      }
      DISPATCH();
    }
    BYTECODE(SetRegister, register_index, value) {
      ADVANCE();
      registers[register_index] = value;
      DISPATCH();
    }
    BYTECODE(ClearRegisters, from_register, to_register) {
      ADVANCE();
      SBXCHECK_LE(from_register, to_register);
      for (uint16_t i = from_register; i <= to_register; ++i) {
        registers[i] = InterpreterRegisters::kNoMatchValue;
      }
      DISPATCH();
    }
    BYTECODE(AdvanceRegister, register_index, by) {
      ADVANCE();
      registers[register_index] += by;
      DISPATCH();
    }
    BYTECODE(WriteCurrentPositionToRegister, register_index, cp_offset) {
      ADVANCE();
      registers[register_index] = current + cp_offset;
      DISPATCH();
    }
    BYTECODE(ReadCurrentPositionFromRegister, register_index) {
      ADVANCE();
      SET_CURRENT_POSITION(registers[register_index]);
      DISPATCH();
    }
    BYTECODE(WriteStackPointerToRegister, register_index) {
      ADVANCE();
      registers[register_index] = backtrack_stack.sp();
      DISPATCH();
    }
    BYTECODE(ReadStackPointerFromRegister, register_index) {
      ADVANCE();
      backtrack_stack.set_sp(registers[register_index]);
      DISPATCH();
    }
    BYTECODE(PopCurrentPosition) {
      ADVANCE();
      SET_CURRENT_POSITION(backtrack_stack.pop());
      DISPATCH();
    }
    BYTECODE(Backtrack, return_code) {
      static_assert(JSRegExp::kNoBacktrackLimit == 0);
      if (++backtrack_count == backtrack_limit) {
        return static_cast<IrregexpInterpreter::Result>(return_code);
      }

      IrregexpInterpreter::Result result =
          HandleInterrupts(isolate, call_origin, code_array, subject_string,
                           &code_base, &subject, &pc);
      if (result != IrregexpInterpreter::SUCCESS) return result;

      SET_PC_FROM_OFFSET(backtrack_stack.pop());
      DISPATCH();
    }
    BYTECODE(PopRegister, register_index) {
      ADVANCE();
      registers[register_index] = backtrack_stack.pop();
      DISPATCH();
    }
    BYTECODE(Fail) {
      isolate->counters()->regexp_backtracks()->AddSample(
          static_cast<int>(backtrack_count));
      return IrregexpInterpreter::FAILURE;
    }
    BYTECODE(Succeed) {
      isolate->counters()->regexp_backtracks()->AddSample(
          static_cast<int>(backtrack_count));
      registers.CopyToOutputRegisters();
      return IrregexpInterpreter::SUCCESS;
    }
    BYTECODE(AdvanceCurrentPosition, by) {
      ADVANCE();
      ADVANCE_CURRENT_POSITION(by);
      DISPATCH();
    }
    BYTECODE(GoTo, label) {
      SET_PC_FROM_OFFSET(label);
      DISPATCH();
    }
    BYTECODE(AdvanceCpAndGoto, by, on_goto) {
      SET_PC_FROM_OFFSET(on_goto);
      ADVANCE_CURRENT_POSITION(by);
      DISPATCH();
    }
    BYTECODE(CheckFixedLengthLoop, on_tos_equals_current_position) {
      if (current == backtrack_stack.peek()) {
        SET_PC_FROM_OFFSET(on_tos_equals_current_position);
        backtrack_stack.pop();
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(LoadCurrentCharacter, cp_offset, on_failure) {
      int pos = current + cp_offset;
      if (pos >= subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(on_failure);
      } else {
        ADVANCE();
        current_char = subject[pos];
      }
      DISPATCH();
    }
    BYTECODE(LoadCurrentCharacterUnchecked, cp_offset) {
      ADVANCE();
      int pos = current + cp_offset;
      current_char = subject[pos];
      DISPATCH();
    }
    BYTECODE(Load2CurrentChars, cp_offset, on_failure) {
      int pos = current + cp_offset;
      if (pos + 2 > subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(on_failure);
      } else {
        ADVANCE();
        current_char = Load2Characters(subject, pos);
      }
      DISPATCH();
    }
    BYTECODE(Load2CurrentCharsUnchecked, cp_offset) {
      ADVANCE();
      int pos = current + cp_offset;
      current_char = Load2Characters(subject, pos);
      DISPATCH();
    }
    BYTECODE(Load4CurrentChars, cp_offset, on_failure) {
      DCHECK_EQ(1, sizeof(Char));
      int pos = current + cp_offset;
      if (pos + 4 > subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(on_failure);
      } else {
        ADVANCE();
        current_char = Load4Characters(subject, pos);
      }
      DISPATCH();
    }
    BYTECODE(Load4CurrentCharsUnchecked, cp_offset) {
      ADVANCE();
      DCHECK_EQ(1, sizeof(Char));
      int pos = current + cp_offset;
      current_char = Load4Characters(subject, pos);
      DISPATCH();
    }
    BYTECODE(Check4Chars, characters, on_equal) {
      if (characters == current_char) {
        SET_PC_FROM_OFFSET(on_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckCharacter, character, on_equal) {
      if (character == current_char) {
        SET_PC_FROM_OFFSET(on_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckNot4Chars, characters, on_not_equal) {
      if (characters != current_char) {
        SET_PC_FROM_OFFSET(on_not_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckNotCharacter, character, on_not_equal) {
      if (character != current_char) {
        SET_PC_FROM_OFFSET(on_not_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(AndCheck4Chars, characters, mask, on_equal) {
      if (characters == (current_char & mask)) {
        SET_PC_FROM_OFFSET(on_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckCharacterAfterAnd, character, mask, on_equal) {
      if (character == (current_char & mask)) {
        SET_PC_FROM_OFFSET(on_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(AndCheckNot4Chars, characters, mask, on_not_equal) {
      if (characters != (current_char & mask)) {
        SET_PC_FROM_OFFSET(on_not_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckNotCharacterAfterAnd, character, mask, on_not_equal) {
      if (character != (current_char & mask)) {
        SET_PC_FROM_OFFSET(on_not_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckNotCharacterAfterMinusAnd, character, minus, mask,
             on_not_equal) {
      if (character != ((current_char - minus) & mask)) {
        SET_PC_FROM_OFFSET(on_not_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckCharacterInRange, from, to, on_in_range) {
      if (from <= current_char && current_char <= to) {
        SET_PC_FROM_OFFSET(on_in_range);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckCharacterNotInRange, from, to, on_not_in_range) {
      if (from > current_char || current_char > to) {
        SET_PC_FROM_OFFSET(on_not_in_range);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckBitInTable, on_bit_set, table) {
      if (CheckBitInTable(current_char, table)) {
        SET_PC_FROM_OFFSET(on_bit_set);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckCharacterLT, limit, on_less) {
      if (current_char < limit) {
        SET_PC_FROM_OFFSET(on_less);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckCharacterGT, limit, on_greater) {
      if (current_char > limit) {
        SET_PC_FROM_OFFSET(on_greater);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(IfRegisterLT, register_index, comparand, on_less_than) {
      if (registers[register_index] < comparand) {
        SET_PC_FROM_OFFSET(on_less_than);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(IfRegisterGE, register_index, comparand, on_greater_or_equal) {
      if (registers[register_index] >= comparand) {
        SET_PC_FROM_OFFSET(on_greater_or_equal);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(IfRegisterEqPos, register_index, on_eq) {
      if (registers[register_index] == current) {
        SET_PC_FROM_OFFSET(on_eq);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckNotBackRef, start_reg, on_not_equal) {
      int from = registers[start_reg];
      int len = registers[start_reg + 1] - from;
      if (from >= 0 && len > 0) {
        if (current + len > subject.length() ||
            !CompareCharsEqual(&subject[from], &subject[current], len)) {
          SET_PC_FROM_OFFSET(on_not_equal);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(len);
      }
      ADVANCE();
      DISPATCH();
    }
    BYTECODE(CheckNotBackRefBackward, start_reg, on_not_equal) {
      int from = registers[start_reg];
      int len = registers[start_reg + 1] - from;
      if (from >= 0 && len > 0) {
        if (current - len < 0 ||
            !CompareCharsEqual(&subject[from], &subject[current - len], len)) {
          SET_PC_FROM_OFFSET(on_not_equal);
          DISPATCH();
        }
        SET_CURRENT_POSITION(current - len);
      }
      ADVANCE();
      DISPATCH();
    }
    BYTECODE(CheckNotBackRefNoCaseUnicode, start_reg, on_not_equal) {
      int from = registers[start_reg];
      int len = registers[start_reg + 1] - from;
      if (from >= 0 && len > 0) {
        if (current + len > subject.length() ||
            !BackRefMatchesNoCase(isolate, from, current, len, subject, true)) {
          SET_PC_FROM_OFFSET(on_not_equal);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(len);
      }
      ADVANCE();
      DISPATCH();
    }
    BYTECODE(CheckNotBackRefNoCase, start_reg, on_not_equal) {
      int from = registers[start_reg];
      int len = registers[start_reg + 1] - from;
      if (from >= 0 && len > 0) {
        if (current + len > subject.length() ||
            !BackRefMatchesNoCase(isolate, from, current, len, subject,
                                  false)) {
          SET_PC_FROM_OFFSET(on_not_equal);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(len);
      }
      ADVANCE();
      DISPATCH();
    }
    BYTECODE(CheckNotBackRefNoCaseUnicodeBackward, start_reg, on_not_equal) {
      int from = registers[start_reg];
      int len = registers[start_reg + 1] - from;
      if (from >= 0 && len > 0) {
        if (current - len < 0 ||
            !BackRefMatchesNoCase(isolate, from, current - len, len, subject,
                                  true)) {
          SET_PC_FROM_OFFSET(on_not_equal);
          DISPATCH();
        }
        SET_CURRENT_POSITION(current - len);
      }
      ADVANCE();
      DISPATCH();
    }
    BYTECODE(CheckNotBackRefNoCaseBackward, start_reg, on_not_equal) {
      int from = registers[start_reg];
      int len = registers[start_reg + 1] - from;
      if (from >= 0 && len > 0) {
        if (current - len < 0 ||
            !BackRefMatchesNoCase(isolate, from, current - len, len, subject,
                                  false)) {
          SET_PC_FROM_OFFSET(on_not_equal);
          DISPATCH();
        }
        SET_CURRENT_POSITION(current - len);
      }
      ADVANCE();
      DISPATCH();
    }
    BYTECODE(CheckAtStart, cp_offset, on_at_start) {
      if (current + cp_offset == 0) {
        SET_PC_FROM_OFFSET(on_at_start);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckNotAtStart, cp_offset, on_not_at_start) {
      if (current + cp_offset == 0) {
        ADVANCE();
      } else {
        SET_PC_FROM_OFFSET(on_not_at_start);
      }
      DISPATCH();
    }
    BYTECODE(SetCurrentPositionFromEnd, by) {
      ADVANCE();
      if (subject.length() - current > by) {
        SET_CURRENT_POSITION(subject.length() - by);
        current_char = subject[current - 1];
      }
      DISPATCH();
    }
    BYTECODE(CheckPosition, cp_offset, on_failure) {
      int pos = current + cp_offset;
      if (pos >= subject.length() || pos < 0) {
        SET_PC_FROM_OFFSET(on_failure);
      } else {
        ADVANCE();
      }
      DISPATCH();
    }
    BYTECODE(CheckSpecialClassRanges, character_set, on_no_match) {
      const bool match =
          CheckSpecialClassRanges<Char>(current_char, character_set);
      if (match) {
        ADVANCE();
      } else {
        SET_PC_FROM_OFFSET(on_no_match);
      }
      DISPATCH();
    }
    BYTECODE(SkipUntilChar, cp_offset, advance_by, character, on_match,
             on_no_match) {
      while (IndexIsInBounds(current + cp_offset, subject.length())) {
        current_char = subject[current + cp_offset];
        if (character == current_char) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(advance_by);
      }
      SET_PC_FROM_OFFSET(on_no_match);
      DISPATCH();
    }
    BYTECODE(SkipUntilCharAnd, cp_offset, advance_by, character, mask,
             eats_at_least, on_match, on_no_match) {
      while (IndexIsInBounds(current + eats_at_least, subject.length())) {
        current_char = subject[current + cp_offset];
        if (character == (current_char & mask)) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(advance_by);
      }
      SET_PC_FROM_OFFSET(on_no_match);
      DISPATCH();
    }
    BYTECODE(SkipUntilCharPosChecked, cp_offset, advance_by, character,
             eats_at_least, on_match, on_no_match) {
      while (IndexIsInBounds(current + eats_at_least, subject.length())) {
        current_char = subject[current + cp_offset];
        if (character == current_char) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(advance_by);
      }
      SET_PC_FROM_OFFSET(on_no_match);
      DISPATCH();
    }
    BYTECODE(SkipUntilBitInTable, cp_offset, advance_by, table, on_match,
             on_no_match) {
      while (IndexIsInBounds(current + cp_offset, subject.length())) {
        current_char = subject[current + cp_offset];
        if (CheckBitInTable(current_char, table)) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(advance_by);
      }
      SET_PC_FROM_OFFSET(on_no_match);
      DISPATCH();
    }
    BYTECODE(SkipUntilGtOrNotBitInTable, cp_offset, advance_by, character,
             table, on_match, on_no_match) {
      while (IndexIsInBounds(current + cp_offset, subject.length())) {
        current_char = subject[current + cp_offset];
        if (current_char > character) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        if (!CheckBitInTable(current_char, table)) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(advance_by);
      }
      SET_PC_FROM_OFFSET(on_no_match);
      DISPATCH();
    }
    BYTECODE(SkipUntilCharOrChar, cp_offset, advance_by, char1, char2, on_match,
             on_no_match) {
      while (IndexIsInBounds(current + cp_offset, subject.length())) {
        current_char = subject[current + cp_offset];
        // The two if-statements below are split up intentionally, as combining
        // them seems to result in register allocation behaving quite
        // differently and slowing down the resulting code.
        if (char1 == current_char) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        if (char2 == current_char) {
          SET_PC_FROM_OFFSET(on_match);
          DISPATCH();
        }
        ADVANCE_CURRENT_POSITION(advance_by);
      }
      SET_PC_FROM_OFFSET(on_no_match);
      DISPATCH();
    }
    BYTECODE(SkipUntilOneOfMasked, cp_offset, advance_by, both_chars, both_mask,
             max_offset, chars1, mask1, chars2, mask2, on_match1, on_match2,
             on_failure) {
      DCHECK_GE(cp_offset, 0);
      DCHECK_GE(max_offset, cp_offset);
      // We should only get here in 1-byte mode.
      DCHECK_EQ(1, sizeof(Char));
      while (IndexIsInBounds(current + max_offset, subject.length())) {
        int pos = current + cp_offset;
        current_char = Load4Characters(subject, pos);
        if (both_chars == (current_char & both_mask)) {
          if (chars1 == (current_char & mask1)) {
            SET_PC_FROM_OFFSET(on_match1);
            DISPATCH();
          }
          if (chars2 == (current_char & mask2)) {
            SET_PC_FROM_OFFSET(on_match2);
            DISPATCH();
          }
        }
        ADVANCE_CURRENT_POSITION(advance_by);
      }
      SET_PC_FROM_OFFSET(on_failure);
      DISPATCH();
    }
    BYTECODE(SkipUntilOneOfMasked3, bc0_cp_offset, bc0_advance_by, bc0_table,
             bc1_cp_offset, bc1_on_failure, bc2_cp_offset, bc3_characters,
             bc3_mask, bc4_by, bc5_cp_offset, bc6_characters, bc6_mask,
             bc6_on_equal, bc7_characters, bc7_mask, bc7_on_equal,
             bc8_characters, bc8_mask, fallthrough_jump_target) {
      // We should only get here in 1-byte mode.
      DCHECK_EQ(1, sizeof(Char));

      while (true) {
        // bcO: kSkipUntilBitInTable
        // on_match and on_no_match are constrained to jump to bc1.
        while (IndexIsInBounds(current + bc0_cp_offset, subject.length())) {
          current_char = subject[current + bc0_cp_offset];
          if (CheckBitInTable(current_char, bc0_table)) {
            break;
          }
          ADVANCE_CURRENT_POSITION(bc0_advance_by);
        }

        // bc1: kCheckPosition
        if (!IndexIsInBounds(current + bc1_cp_offset, subject.length())) {
          SET_PC_FROM_OFFSET(bc1_on_failure);
          DISPATCH();
        }

        // bc2: Load4CurrentCharsUnchecked
        int pos = current + bc2_cp_offset;
        current_char = Load4Characters(subject, pos);

        // bc3: AndCheck4Chars
        // on_equal is constrained to jump to bc5.
        if (bc3_characters == (current_char & bc3_mask)) {
          // bc5: Load4CurrentChars
          // on_failure is constrained to jump to bc4.
          DCHECK_GE(bc5_cp_offset, 0);
          if (current + bc5_cp_offset + 4 > subject.length()) {
            // bc4: AdvanceCpAndGoto
            // on_goto is constrained to jump back to bc0.
            ADVANCE_CURRENT_POSITION(bc4_by);
            continue;
          }
          // TODO(jgruber): Usually we can reuse some of the bytes loaded above.
          pos = current + bc5_cp_offset;
          current_char = Load4Characters(subject, pos);

          // bc6: AndCheck4Chars
          if (bc6_characters == (current_char & bc6_mask)) {
            SET_PC_FROM_OFFSET(bc6_on_equal);
            DISPATCH();
          }
          // bc7: AndCheck4Chars
          if (bc7_characters == (current_char & bc7_mask)) {
            SET_PC_FROM_OFFSET(bc7_on_equal);
            DISPATCH();
          }
          // bc8: AndCheckNot4Chars
          // on_not_equal is constrained to jump to bc4.
          if (bc8_characters == (current_char & bc8_mask)) {
            SET_PC_FROM_OFFSET(fallthrough_jump_target);
            DISPATCH();
          }
        }

        // bc4: AdvanceCpAndGoto
        // on_goto is constrained to jump back to bc0.
        ADVANCE_CURRENT_POSITION(bc4_by);
      }

      UNREACHABLE();
    }
    BYTECODES_END()
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

#undef OPEN_BLOCK
#undef CLOSE_BLOCK
#undef BYTECODES_START
#undef BYTECODES_END
#undef BYTECODE
#undef ADVANCE_CURRENT_POSITION
#undef SET_CURRENT_POSITION
#undef DISPATCH
#undef DECODE
#undef SET_PC_FROM_OFFSET
#undef ADVANCE
#undef BC_LABEL
#undef V8_USE_COMPUTED_GOTO

}  // namespace

// static
int IrregexpInterpreter::Match(Isolate* isolate,
                               Tagged<IrRegExpData> regexp_data,
                               Tagged<String> subject_string,
                               int* output_registers, int output_register_count,
                               int start_position,
                               RegExp::CallOrigin call_origin) {
  if (v8_flags.regexp_tier_up) regexp_data->TierUpTick();

  bool is_any_unicode =
      IsEitherUnicode(JSRegExp::AsRegExpFlags(regexp_data->flags()));
  bool is_one_byte = String::IsOneByteRepresentationUnderneath(subject_string);
  Tagged<TrustedByteArray> code_array = regexp_data->bytecode(is_one_byte);
  int total_register_count = regexp_data->max_register_count();

  // MatchInternal only supports returning a single match per call. In global
  // mode, i.e. when output_registers has space for more than one match, we
  // need to keep running until all matches are filled in.
  int registers_per_match =
      JSRegExp::RegistersForCaptureCount(regexp_data->capture_count());
  DCHECK_LE(registers_per_match, output_register_count);
  int number_of_matches_in_output_registers =
      output_register_count / registers_per_match;

  int backtrack_limit = regexp_data->backtrack_limit();

  int num_matches = 0;
  int* current_output_registers = output_registers;
  for (int i = 0; i < number_of_matches_in_output_registers; i++) {
    auto current_result = MatchInternal(
        isolate, &code_array, &subject_string, current_output_registers,
        registers_per_match, total_register_count, start_position, call_origin,
        backtrack_limit);

    if (current_result == SUCCESS) {
      // Fall through.
    } else if (current_result == FAILURE) {
      break;
    } else {
      DCHECK(current_result == EXCEPTION ||
             current_result == FALLBACK_TO_EXPERIMENTAL ||
             current_result == RETRY);
      return current_result;
    }

    // Found a match. Advance the index.

    num_matches++;

    int next_start_position = current_output_registers[1];
    if (next_start_position == current_output_registers[0]) {
      // Zero-length matches.
      // TODO(jgruber): Use AdvanceStringIndex based on flat contents instead.
      next_start_position = static_cast<int>(RegExpUtils::AdvanceStringIndex(
          subject_string, next_start_position, is_any_unicode));
      if (next_start_position > static_cast<int>(subject_string->length())) {
        break;
      }
    }

    start_position = next_start_position;
    current_output_registers += registers_per_match;
  }

  return num_matches;
}

IrregexpInterpreter::Result IrregexpInterpreter::MatchInternal(
    Isolate* isolate, Tagged<TrustedByteArray>* code_array,
    Tagged<String>* subject_string, int* output_registers,
    int output_register_count, int total_register_count, int start_position,
    RegExp::CallOrigin call_origin, uint32_t backtrack_limit) {
  DCHECK((*subject_string)->IsFlat());

  // Note: Heap allocation *is* allowed in two situations if calling from
  // Runtime:
  // 1. When creating & throwing a stack overflow exception. The interpreter
  //    aborts afterwards, and thus possible-moved objects are never used.
  // 2. When handling interrupts. We manually relocate unhandlified references
  //    after interrupts have run.
  DisallowGarbageCollection no_gc;

  base::uc16 previous_char = '\n';
  String::FlatContent subject_content =
      (*subject_string)->GetFlatContent(no_gc);
  // Because interrupts can result in GC and string content relocation, the
  // checksum verification in FlatContent may fail even though this code is
  // safe. See (2) above.
  subject_content.UnsafeDisableChecksumVerification();
  if (subject_content.IsOneByte()) {
    base::Vector<const uint8_t> subject_vector =
        subject_content.ToOneByteVector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate, code_array, subject_string, subject_vector,
                    output_registers, output_register_count,
                    total_register_count, start_position, previous_char,
                    call_origin, backtrack_limit);
  } else {
    DCHECK(subject_content.IsTwoByte());
    base::Vector<const base::uc16> subject_vector =
        subject_content.ToUC16Vector();
    if (start_position != 0) previous_char = subject_vector[start_position - 1];
    return RawMatch(isolate, code_array, subject_string, subject_vector,
                    output_registers, output_register_count,
                    total_register_count, start_position, previous_char,
                    call_origin, backtrack_limit);
  }
}

#ifndef COMPILING_IRREGEXP_FOR_EXTERNAL_EMBEDDER

// This method is called through an external reference from RegExpExecInternal
// builtin.
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
// Hardware sandboxing is incompatible with ASAN, see crbug.com/432168626.
DISABLE_ASAN
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
int IrregexpInterpreter::MatchForCallFromJs(
    Address subject, int32_t start_position, Address, Address,
    int* output_registers, int32_t output_register_count,
    RegExp::CallOrigin call_origin, Isolate* isolate, Address regexp_data) {
  // TODO(422992937): investigate running the interpreter in sandboxed mode.
  ExitSandboxScope unsandboxed;

  DCHECK_NOT_NULL(isolate);
  DCHECK_NOT_NULL(output_registers);
  DCHECK(call_origin == RegExp::CallOrigin::kFromJs);

  DisallowGarbageCollection no_gc;
  DisallowJavascriptExecution no_js(isolate);
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  Tagged<String> subject_string = Cast<String>(Tagged<Object>(subject));
  Tagged<IrRegExpData> regexp_data_obj =
      SbxCast<IrRegExpData>(Tagged<Object>(regexp_data));

  if (regexp_data_obj->MarkedForTierUp()) {
    // Returning RETRY will re-enter through runtime, where actual recompilation
    // for tier-up takes place.
    return IrregexpInterpreter::RETRY;
  }

  return Match(isolate, regexp_data_obj, subject_string, output_registers,
               output_register_count, start_position, call_origin);
}

#endif  // !COMPILING_IRREGEXP_FOR_EXTERNAL_EMBEDDER

int IrregexpInterpreter::MatchForCallFromRuntime(
    Isolate* isolate, DirectHandle<IrRegExpData> regexp_data,
    DirectHandle<String> subject_string, int* output_registers,
    int output_register_count, int start_position) {
  return Match(isolate, *regexp_data, *subject_string, output_registers,
               output_register_count, start_position,
               RegExp::CallOrigin::kFromRuntime);
}

}  // namespace internal
}  // namespace v8

// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/regexp/arm64/regexp-macro-assembler-arm64.h"

#include "src/code-stubs.h"
#include "src/log.h"
#include "src/macro-assembler.h"
#include "src/profiler/cpu-profiler.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"
#include "src/unicode.h"

namespace v8 {
namespace internal {

#ifndef V8_INTERPRETED_REGEXP
/*
 * This assembler uses the following register assignment convention:
 * - w19     : Used to temporarely store a value before a call to C code.
 *             See CheckNotBackReferenceIgnoreCase.
 * - x20     : Pointer to the current code object (Code*),
 *             it includes the heap object tag.
 * - w21     : Current position in input, as negative offset from
 *             the end of the string. Please notice that this is
 *             the byte offset, not the character offset!
 * - w22     : Currently loaded character. Must be loaded using
 *             LoadCurrentCharacter before using any of the dispatch methods.
 * - x23     : Points to tip of backtrack stack.
 * - w24     : Position of the first character minus one: non_position_value.
 *             Used to initialize capture registers.
 * - x25     : Address at the end of the input string: input_end.
 *             Points to byte after last character in input.
 * - x26     : Address at the start of the input string: input_start.
 * - w27     : Where to start in the input string.
 * - x28     : Output array pointer.
 * - x29/fp  : Frame pointer. Used to access arguments, local variables and
 *             RegExp registers.
 * - x16/x17 : IP registers, used by assembler. Very volatile.
 * - csp     : Points to tip of C stack.
 *
 * - x0-x7   : Used as a cache to store 32 bit capture registers. These
 *             registers need to be retained every time a call to C code
 *             is done.
 *
 * The remaining registers are free for computations.
 * Each call to a public method should retain this convention.
 *
 * The stack will have the following structure:
 *
 *  Location    Name               Description
 *              (as referred to in
 *              the code)
 *
 *  - fp[104]   isolate            Address of the current isolate.
 *  - fp[96]    return_address     Secondary link/return address
 *                                 used by an exit frame if this is a
 *                                 native call.
 *  ^^^ csp when called ^^^
 *  - fp[88]    lr                 Return from the RegExp code.
 *  - fp[80]    r29                Old frame pointer (CalleeSaved).
 *  - fp[0..72] r19-r28            Backup of CalleeSaved registers.
 *  - fp[-8]    direct_call        1 => Direct call from JavaScript code.
 *                                 0 => Call through the runtime system.
 *  - fp[-16]   stack_base         High end of the memory area to use as
 *                                 the backtracking stack.
 *  - fp[-24]   output_size        Output may fit multiple sets of matches.
 *  - fp[-32]   input              Handle containing the input string.
 *  - fp[-40]   success_counter
 *  ^^^^^^^^^^^^^ From here and downwards we store 32 bit values ^^^^^^^^^^^^^
 *  - fp[-44]   register N         Capture registers initialized with
 *  - fp[-48]   register N + 1     non_position_value.
 *              ...                The first kNumCachedRegisters (N) registers
 *              ...                are cached in x0 to x7.
 *              ...                Only positions must be stored in the first
 *  -           ...                num_saved_registers_ registers.
 *  -           ...
 *  -           register N + num_registers - 1
 *  ^^^^^^^^^ csp ^^^^^^^^^
 *
 * The first num_saved_registers_ registers are initialized to point to
 * "character -1" in the string (i.e., char_size() bytes before the first
 * character of the string). The remaining registers start out as garbage.
 *
 * The data up to the return address must be placed there by the calling
 * code and the remaining arguments are passed in registers, e.g. by calling the
 * code entry as cast to a function with the signature:
 * int (*match)(String* input,
 *              int start_offset,
 *              Address input_start,
 *              Address input_end,
 *              int* output,
 *              int output_size,
 *              Address stack_base,
 *              bool direct_call = false,
 *              Address secondary_return_address,  // Only used by native call.
 *              Isolate* isolate)
 * The call is performed by NativeRegExpMacroAssembler::Execute()
 * (in regexp-macro-assembler.cc) via the CALL_GENERATED_REGEXP_CODE macro
 * in arm64/simulator-arm64.h.
 * When calling as a non-direct call (i.e., from C++ code), the return address
 * area is overwritten with the LR register by the RegExp code. When doing a
 * direct call from generated code, the return address is placed there by
 * the calling code, as in a normal exit frame.
 */

#define __ ACCESS_MASM(masm_)

RegExpMacroAssemblerARM64::RegExpMacroAssemblerARM64(Isolate* isolate,
                                                     Zone* zone, Mode mode,
                                                     int registers_to_save)
    : NativeRegExpMacroAssembler(isolate, zone),
      masm_(new MacroAssembler(isolate, NULL, kRegExpCodeSize,
                               CodeObjectRequired::kYes)),
      mode_(mode),
      num_registers_(registers_to_save),
      num_saved_registers_(registers_to_save),
      entry_label_(),
      start_label_(),
      success_label_(),
      backtrack_label_(),
      exit_label_() {
  __ SetStackPointer(csp);
  DCHECK_EQ(0, registers_to_save % 2);
  // We can cache at most 16 W registers in x0-x7.
  STATIC_ASSERT(kNumCachedRegisters <= 16);
  STATIC_ASSERT((kNumCachedRegisters % 2) == 0);
  __ B(&entry_label_);   // We'll write the entry code later.
  __ Bind(&start_label_);  // And then continue from here.
}


RegExpMacroAssemblerARM64::~RegExpMacroAssemblerARM64() {
  delete masm_;
  // Unuse labels in case we throw away the assembler without calling GetCode.
  entry_label_.Unuse();
  start_label_.Unuse();
  success_label_.Unuse();
  backtrack_label_.Unuse();
  exit_label_.Unuse();
  check_preempt_label_.Unuse();
  stack_overflow_label_.Unuse();
}

int RegExpMacroAssemblerARM64::stack_limit_slack()  {
  return RegExpStack::kStackLimitSlack;
}


void RegExpMacroAssemblerARM64::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    __ Add(current_input_offset(),
           current_input_offset(), by * char_size());
  }
}


void RegExpMacroAssemblerARM64::AdvanceRegister(int reg, int by) {
  DCHECK((reg >= 0) && (reg < num_registers_));
  if (by != 0) {
    Register to_advance;
    RegisterState register_state = GetRegisterState(reg);
    switch (register_state) {
      case STACKED:
        __ Ldr(w10, register_location(reg));
        __ Add(w10, w10, by);
        __ Str(w10, register_location(reg));
        break;
      case CACHED_LSW:
        to_advance = GetCachedRegister(reg);
        __ Add(to_advance, to_advance, by);
        break;
      case CACHED_MSW:
        to_advance = GetCachedRegister(reg);
        __ Add(to_advance, to_advance,
               static_cast<int64_t>(by) << kWRegSizeInBits);
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}


void RegExpMacroAssemblerARM64::Backtrack() {
  CheckPreemption();
  Pop(w10);
  __ Add(x10, code_pointer(), Operand(w10, UXTW));
  __ Br(x10);
}


void RegExpMacroAssemblerARM64::Bind(Label* label) {
  __ Bind(label);
}


void RegExpMacroAssemblerARM64::CheckCharacter(uint32_t c, Label* on_equal) {
  CompareAndBranchOrBacktrack(current_character(), c, eq, on_equal);
}


void RegExpMacroAssemblerARM64::CheckCharacterGT(uc16 limit,
                                                 Label* on_greater) {
  CompareAndBranchOrBacktrack(current_character(), limit, hi, on_greater);
}


void RegExpMacroAssemblerARM64::CheckAtStart(Label* on_at_start) {
  __ Add(w10, current_input_offset(), Operand(-char_size()));
  __ Cmp(w10, string_start_minus_one());
  BranchOrBacktrack(eq, on_at_start);
}


void RegExpMacroAssemblerARM64::CheckNotAtStart(int cp_offset,
                                                Label* on_not_at_start) {
  __ Add(w10, current_input_offset(),
         Operand(-char_size() + cp_offset * char_size()));
  __ Cmp(w10, string_start_minus_one());
  BranchOrBacktrack(ne, on_not_at_start);
}


void RegExpMacroAssemblerARM64::CheckCharacterLT(uc16 limit, Label* on_less) {
  CompareAndBranchOrBacktrack(current_character(), limit, lo, on_less);
}


void RegExpMacroAssemblerARM64::CheckCharacters(Vector<const uc16> str,
                                              int cp_offset,
                                              Label* on_failure,
                                              bool check_end_of_string) {
  // This method is only ever called from the cctests.

  if (check_end_of_string) {
    // Is last character of required match inside string.
    CheckPosition(cp_offset + str.length() - 1, on_failure);
  }

  Register characters_address = x11;

  __ Add(characters_address,
         input_end(),
         Operand(current_input_offset(), SXTW));
  if (cp_offset != 0) {
    __ Add(characters_address, characters_address, cp_offset * char_size());
  }

  for (int i = 0; i < str.length(); i++) {
    if (mode_ == LATIN1) {
      __ Ldrb(w10, MemOperand(characters_address, 1, PostIndex));
      DCHECK(str[i] <= String::kMaxOneByteCharCode);
    } else {
      __ Ldrh(w10, MemOperand(characters_address, 2, PostIndex));
    }
    CompareAndBranchOrBacktrack(w10, str[i], ne, on_failure);
  }
}


void RegExpMacroAssemblerARM64::CheckGreedyLoop(Label* on_equal) {
  __ Ldr(w10, MemOperand(backtrack_stackpointer()));
  __ Cmp(current_input_offset(), w10);
  __ Cset(x11, eq);
  __ Add(backtrack_stackpointer(),
         backtrack_stackpointer(), Operand(x11, LSL, kWRegSizeLog2));
  BranchOrBacktrack(eq, on_equal);
}


void RegExpMacroAssemblerARM64::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, Label* on_no_match) {
  Label fallthrough;

  Register capture_start_offset = w10;
  // Save the capture length in a callee-saved register so it will
  // be preserved if we call a C helper.
  Register capture_length = w19;
  DCHECK(kCalleeSaved.IncludesAliasOf(capture_length));

  // Find length of back-referenced capture.
  DCHECK((start_reg % 2) == 0);
  if (start_reg < kNumCachedRegisters) {
    __ Mov(capture_start_offset.X(), GetCachedRegister(start_reg));
    __ Lsr(x11, GetCachedRegister(start_reg), kWRegSizeInBits);
  } else {
    __ Ldp(w11, capture_start_offset, capture_location(start_reg, x10));
  }
  __ Sub(capture_length, w11, capture_start_offset);  // Length to check.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ CompareAndBranch(capture_length, Operand(0), eq, &fallthrough);

  // Check that there are enough characters left in the input.
  if (read_backward) {
    __ Add(w12, string_start_minus_one(), capture_length);
    __ Cmp(current_input_offset(), w12);
    BranchOrBacktrack(le, on_no_match);
  } else {
    __ Cmn(capture_length, current_input_offset());
    BranchOrBacktrack(gt, on_no_match);
  }

  if (mode_ == LATIN1) {
    Label success;
    Label fail;
    Label loop_check;

    Register capture_start_address = x12;
    Register capture_end_addresss = x13;
    Register current_position_address = x14;

    __ Add(capture_start_address,
           input_end(),
           Operand(capture_start_offset, SXTW));
    __ Add(capture_end_addresss,
           capture_start_address,
           Operand(capture_length, SXTW));
    __ Add(current_position_address,
           input_end(),
           Operand(current_input_offset(), SXTW));
    if (read_backward) {
      // Offset by length when matching backwards.
      __ Sub(current_position_address, current_position_address,
             Operand(capture_length, SXTW));
    }

    Label loop;
    __ Bind(&loop);
    __ Ldrb(w10, MemOperand(capture_start_address, 1, PostIndex));
    __ Ldrb(w11, MemOperand(current_position_address, 1, PostIndex));
    __ Cmp(w10, w11);
    __ B(eq, &loop_check);

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ Orr(w10, w10, 0x20);  // Convert capture character to lower-case.
    __ Orr(w11, w11, 0x20);  // Also convert input character.
    __ Cmp(w11, w10);
    __ B(ne, &fail);
    __ Sub(w10, w10, 'a');
    __ Cmp(w10, 'z' - 'a');  // Is w10 a lowercase letter?
    __ B(ls, &loop_check);  // In range 'a'-'z'.
    // Latin-1: Check for values in range [224,254] but not 247.
    __ Sub(w10, w10, 224 - 'a');
    __ Cmp(w10, 254 - 224);
    __ Ccmp(w10, 247 - 224, ZFlag, ls);  // Check for 247.
    __ B(eq, &fail);  // Weren't Latin-1 letters.

    __ Bind(&loop_check);
    __ Cmp(capture_start_address, capture_end_addresss);
    __ B(lt, &loop);
    __ B(&success);

    __ Bind(&fail);
    BranchOrBacktrack(al, on_no_match);

    __ Bind(&success);
    // Compute new value of character position after the matched part.
    __ Sub(current_input_offset().X(), current_position_address, input_end());
    if (read_backward) {
      __ Sub(current_input_offset().X(), current_input_offset().X(),
             Operand(capture_length, SXTW));
    }
    if (masm_->emit_debug_code()) {
      __ Cmp(current_input_offset().X(), Operand(current_input_offset(), SXTW));
      __ Ccmp(current_input_offset(), 0, NoFlag, eq);
      // The current input offset should be <= 0, and fit in a W register.
      __ Check(le, kOffsetOutOfRange);
    }
  } else {
    DCHECK(mode_ == UC16);
    int argument_count = 4;

    // The cached registers need to be retained.
    CPURegList cached_registers(CPURegister::kRegister, kXRegSizeInBits, 0, 7);
    DCHECK((cached_registers.Count() * 2) == kNumCachedRegisters);
    __ PushCPURegList(cached_registers);

    // Put arguments into arguments registers.
    // Parameters are
    //   x0: Address byte_offset1 - Address captured substring's start.
    //   x1: Address byte_offset2 - Address of current character position.
    //   w2: size_t byte_length - length of capture in bytes(!)
    //   x3: Isolate* isolate

    // Address of start of capture.
    __ Add(x0, input_end(), Operand(capture_start_offset, SXTW));
    // Length of capture.
    __ Mov(w2, capture_length);
    // Address of current input position.
    __ Add(x1, input_end(), Operand(current_input_offset(), SXTW));
    if (read_backward) {
      __ Sub(x1, x1, Operand(capture_length, SXTW));
    }
    // Isolate.
    __ Mov(x3, ExternalReference::isolate_address(isolate()));

    {
      AllowExternalCallThatCantCauseGC scope(masm_);
      ExternalReference function =
          ExternalReference::re_case_insensitive_compare_uc16(isolate());
      __ CallCFunction(function, argument_count);
    }

    // Check if function returned non-zero for success or zero for failure.
    // x0 is one of the registers used as a cache so it must be tested before
    // the cache is restored.
    __ Cmp(x0, 0);
    __ PopCPURegList(cached_registers);
    BranchOrBacktrack(eq, on_no_match);

    // On success, advance position by length of capture.
    if (read_backward) {
      __ Sub(current_input_offset(), current_input_offset(), capture_length);
    } else {
      __ Add(current_input_offset(), current_input_offset(), capture_length);
    }
  }

  __ Bind(&fallthrough);
}

void RegExpMacroAssemblerARM64::CheckNotBackReference(int start_reg,
                                                      bool read_backward,
                                                      Label* on_no_match) {
  Label fallthrough;

  Register capture_start_address = x12;
  Register capture_end_address = x13;
  Register current_position_address = x14;
  Register capture_length = w15;

  // Find length of back-referenced capture.
  DCHECK((start_reg % 2) == 0);
  if (start_reg < kNumCachedRegisters) {
    __ Mov(x10, GetCachedRegister(start_reg));
    __ Lsr(x11, GetCachedRegister(start_reg), kWRegSizeInBits);
  } else {
    __ Ldp(w11, w10, capture_location(start_reg, x10));
  }
  __ Sub(capture_length, w11, w10);  // Length to check.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ CompareAndBranch(capture_length, Operand(0), eq, &fallthrough);

  // Check that there are enough characters left in the input.
  if (read_backward) {
    __ Add(w12, string_start_minus_one(), capture_length);
    __ Cmp(current_input_offset(), w12);
    BranchOrBacktrack(le, on_no_match);
  } else {
    __ Cmn(capture_length, current_input_offset());
    BranchOrBacktrack(gt, on_no_match);
  }

  // Compute pointers to match string and capture string
  __ Add(capture_start_address, input_end(), Operand(w10, SXTW));
  __ Add(capture_end_address,
         capture_start_address,
         Operand(capture_length, SXTW));
  __ Add(current_position_address,
         input_end(),
         Operand(current_input_offset(), SXTW));
  if (read_backward) {
    // Offset by length when matching backwards.
    __ Sub(current_position_address, current_position_address,
           Operand(capture_length, SXTW));
  }

  Label loop;
  __ Bind(&loop);
  if (mode_ == LATIN1) {
    __ Ldrb(w10, MemOperand(capture_start_address, 1, PostIndex));
    __ Ldrb(w11, MemOperand(current_position_address, 1, PostIndex));
  } else {
    DCHECK(mode_ == UC16);
    __ Ldrh(w10, MemOperand(capture_start_address, 2, PostIndex));
    __ Ldrh(w11, MemOperand(current_position_address, 2, PostIndex));
  }
  __ Cmp(w10, w11);
  BranchOrBacktrack(ne, on_no_match);
  __ Cmp(capture_start_address, capture_end_address);
  __ B(lt, &loop);

  // Move current character position to position after match.
  __ Sub(current_input_offset().X(), current_position_address, input_end());
  if (read_backward) {
    __ Sub(current_input_offset().X(), current_input_offset().X(),
           Operand(capture_length, SXTW));
  }

  if (masm_->emit_debug_code()) {
    __ Cmp(current_input_offset().X(), Operand(current_input_offset(), SXTW));
    __ Ccmp(current_input_offset(), 0, NoFlag, eq);
    // The current input offset should be <= 0, and fit in a W register.
    __ Check(le, kOffsetOutOfRange);
  }
  __ Bind(&fallthrough);
}


void RegExpMacroAssemblerARM64::CheckNotCharacter(unsigned c,
                                                  Label* on_not_equal) {
  CompareAndBranchOrBacktrack(current_character(), c, ne, on_not_equal);
}


void RegExpMacroAssemblerARM64::CheckCharacterAfterAnd(uint32_t c,
                                                       uint32_t mask,
                                                       Label* on_equal) {
  __ And(w10, current_character(), mask);
  CompareAndBranchOrBacktrack(w10, c, eq, on_equal);
}


void RegExpMacroAssemblerARM64::CheckNotCharacterAfterAnd(unsigned c,
                                                          unsigned mask,
                                                          Label* on_not_equal) {
  __ And(w10, current_character(), mask);
  CompareAndBranchOrBacktrack(w10, c, ne, on_not_equal);
}


void RegExpMacroAssemblerARM64::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  DCHECK(minus < String::kMaxUtf16CodeUnit);
  __ Sub(w10, current_character(), minus);
  __ And(w10, w10, mask);
  CompareAndBranchOrBacktrack(w10, c, ne, on_not_equal);
}


void RegExpMacroAssemblerARM64::CheckCharacterInRange(
    uc16 from,
    uc16 to,
    Label* on_in_range) {
  __ Sub(w10, current_character(), from);
  // Unsigned lower-or-same condition.
  CompareAndBranchOrBacktrack(w10, to - from, ls, on_in_range);
}


void RegExpMacroAssemblerARM64::CheckCharacterNotInRange(
    uc16 from,
    uc16 to,
    Label* on_not_in_range) {
  __ Sub(w10, current_character(), from);
  // Unsigned higher condition.
  CompareAndBranchOrBacktrack(w10, to - from, hi, on_not_in_range);
}


void RegExpMacroAssemblerARM64::CheckBitInTable(
    Handle<ByteArray> table,
    Label* on_bit_set) {
  __ Mov(x11, Operand(table));
  if ((mode_ != LATIN1) || (kTableMask != String::kMaxOneByteCharCode)) {
    __ And(w10, current_character(), kTableMask);
    __ Add(w10, w10, ByteArray::kHeaderSize - kHeapObjectTag);
  } else {
    __ Add(w10, current_character(), ByteArray::kHeaderSize - kHeapObjectTag);
  }
  __ Ldrb(w11, MemOperand(x11, w10, UXTW));
  CompareAndBranchOrBacktrack(w11, 0, ne, on_bit_set);
}


bool RegExpMacroAssemblerARM64::CheckSpecialCharacterClass(uc16 type,
                                                           Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check
  switch (type) {
  case 's':
    // Match space-characters
    if (mode_ == LATIN1) {
      // One byte space characters are '\t'..'\r', ' ' and \u00a0.
      Label success;
      // Check for ' ' or 0x00a0.
      __ Cmp(current_character(), ' ');
      __ Ccmp(current_character(), 0x00a0, ZFlag, ne);
      __ B(eq, &success);
      // Check range 0x09..0x0d.
      __ Sub(w10, current_character(), '\t');
      CompareAndBranchOrBacktrack(w10, '\r' - '\t', hi, on_no_match);
      __ Bind(&success);
      return true;
    }
    return false;
  case 'S':
    // The emitted code for generic character classes is good enough.
    return false;
  case 'd':
    // Match ASCII digits ('0'..'9').
    __ Sub(w10, current_character(), '0');
    CompareAndBranchOrBacktrack(w10, '9' - '0', hi, on_no_match);
    return true;
  case 'D':
    // Match ASCII non-digits.
    __ Sub(w10, current_character(), '0');
    CompareAndBranchOrBacktrack(w10, '9' - '0', ls, on_no_match);
    return true;
  case '.': {
    // Match non-newlines (not 0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029)
    // Here we emit the conditional branch only once at the end to make branch
    // prediction more efficient, even though we could branch out of here
    // as soon as a character matches.
    __ Cmp(current_character(), 0x0a);
    __ Ccmp(current_character(), 0x0d, ZFlag, ne);
    if (mode_ == UC16) {
      __ Sub(w10, current_character(), 0x2028);
      // If the Z flag was set we clear the flags to force a branch.
      __ Ccmp(w10, 0x2029 - 0x2028, NoFlag, ne);
      // ls -> !((C==1) && (Z==0))
      BranchOrBacktrack(ls, on_no_match);
    } else {
      BranchOrBacktrack(eq, on_no_match);
    }
    return true;
  }
  case 'n': {
    // Match newlines (0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029)
    // We have to check all 4 newline characters before emitting
    // the conditional branch.
    __ Cmp(current_character(), 0x0a);
    __ Ccmp(current_character(), 0x0d, ZFlag, ne);
    if (mode_ == UC16) {
      __ Sub(w10, current_character(), 0x2028);
      // If the Z flag was set we clear the flags to force a fall-through.
      __ Ccmp(w10, 0x2029 - 0x2028, NoFlag, ne);
      // hi -> (C==1) && (Z==0)
      BranchOrBacktrack(hi, on_no_match);
    } else {
      BranchOrBacktrack(ne, on_no_match);
    }
    return true;
  }
  case 'w': {
    if (mode_ != LATIN1) {
      // Table is 256 entries, so all Latin1 characters can be tested.
      CompareAndBranchOrBacktrack(current_character(), 'z', hi, on_no_match);
    }
    ExternalReference map = ExternalReference::re_word_character_map();
    __ Mov(x10, map);
    __ Ldrb(w10, MemOperand(x10, current_character(), UXTW));
    CompareAndBranchOrBacktrack(w10, 0, eq, on_no_match);
    return true;
  }
  case 'W': {
    Label done;
    if (mode_ != LATIN1) {
      // Table is 256 entries, so all Latin1 characters can be tested.
      __ Cmp(current_character(), 'z');
      __ B(hi, &done);
    }
    ExternalReference map = ExternalReference::re_word_character_map();
    __ Mov(x10, map);
    __ Ldrb(w10, MemOperand(x10, current_character(), UXTW));
    CompareAndBranchOrBacktrack(w10, 0, ne, on_no_match);
    __ Bind(&done);
    return true;
  }
  case '*':
    // Match any character.
    return true;
  // No custom implementation (yet): s(UC16), S(UC16).
  default:
    return false;
  }
}


void RegExpMacroAssemblerARM64::Fail() {
  __ Mov(w0, FAILURE);
  __ B(&exit_label_);
}


Handle<HeapObject> RegExpMacroAssemblerARM64::GetCode(Handle<String> source) {
  Label return_w0;
  // Finalize code - write the entry point code now we know how many
  // registers we need.

  // Entry code:
  __ Bind(&entry_label_);

  // Arguments on entry:
  // x0:  String*  input
  // x1:  int      start_offset
  // x2:  byte*    input_start
  // x3:  byte*    input_end
  // x4:  int*     output array
  // x5:  int      output array size
  // x6:  Address  stack_base
  // x7:  int      direct_call

  // The stack pointer should be csp on entry.
  //  csp[8]:  address of the current isolate
  //  csp[0]:  secondary link/return address used by native call

  // Tell the system that we have a stack frame.  Because the type is MANUAL, no
  // code is generated.
  FrameScope scope(masm_, StackFrame::MANUAL);

  // Push registers on the stack, only push the argument registers that we need.
  CPURegList argument_registers(x0, x5, x6, x7);

  CPURegList registers_to_retain = kCalleeSaved;
  DCHECK(kCalleeSaved.Count() == 11);
  registers_to_retain.Combine(lr);

  DCHECK(csp.Is(__ StackPointer()));
  __ PushCPURegList(registers_to_retain);
  __ PushCPURegList(argument_registers);

  // Set frame pointer in place.
  __ Add(frame_pointer(), csp, argument_registers.Count() * kPointerSize);

  // Initialize callee-saved registers.
  __ Mov(start_offset(), w1);
  __ Mov(input_start(), x2);
  __ Mov(input_end(), x3);
  __ Mov(output_array(), x4);

  // Set the number of registers we will need to allocate, that is:
  //   - success_counter (X register)
  //   - (num_registers_ - kNumCachedRegisters) (W registers)
  int num_wreg_to_allocate = num_registers_ - kNumCachedRegisters;
  // Do not allocate registers on the stack if they can all be cached.
  if (num_wreg_to_allocate < 0) { num_wreg_to_allocate = 0; }
  // Make room for the success_counter.
  num_wreg_to_allocate += 2;

  // Make sure the stack alignment will be respected.
  int alignment = masm_->ActivationFrameAlignment();
  DCHECK_EQ(alignment % 16, 0);
  int align_mask = (alignment / kWRegSize) - 1;
  num_wreg_to_allocate = (num_wreg_to_allocate + align_mask) & ~align_mask;

  // Check if we have space on the stack.
  Label stack_limit_hit;
  Label stack_ok;

  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ Mov(x10, stack_limit);
  __ Ldr(x10, MemOperand(x10));
  __ Subs(x10, csp, x10);

  // Handle it if the stack pointer is already below the stack limit.
  __ B(ls, &stack_limit_hit);

  // Check if there is room for the variable number of registers above
  // the stack limit.
  __ Cmp(x10, num_wreg_to_allocate * kWRegSize);
  __ B(hs, &stack_ok);

  // Exit with OutOfMemory exception. There is not enough space on the stack
  // for our working registers.
  __ Mov(w0, EXCEPTION);
  __ B(&return_w0);

  __ Bind(&stack_limit_hit);
  CallCheckStackGuardState(x10);
  // If returned value is non-zero, we exit with the returned value as result.
  __ Cbnz(w0, &return_w0);

  __ Bind(&stack_ok);

  // Allocate space on stack.
  __ Claim(num_wreg_to_allocate, kWRegSize);

  // Initialize success_counter with 0.
  __ Str(wzr, MemOperand(frame_pointer(), kSuccessCounter));

  // Find negative length (offset of start relative to end).
  __ Sub(x10, input_start(), input_end());
  if (masm_->emit_debug_code()) {
    // Check that the input string length is < 2^30.
    __ Neg(x11, x10);
    __ Cmp(x11, (1<<30) - 1);
    __ Check(ls, kInputStringTooLong);
  }
  __ Mov(current_input_offset(), w10);

  // The non-position value is used as a clearing value for the
  // capture registers, it corresponds to the position of the first character
  // minus one.
  __ Sub(string_start_minus_one(), current_input_offset(), char_size());
  __ Sub(string_start_minus_one(), string_start_minus_one(),
         Operand(start_offset(), LSL, (mode_ == UC16) ? 1 : 0));
  // We can store this value twice in an X register for initializing
  // on-stack registers later.
  __ Orr(twice_non_position_value(), string_start_minus_one().X(),
         Operand(string_start_minus_one().X(), LSL, kWRegSizeInBits));

  // Initialize code pointer register.
  __ Mov(code_pointer(), Operand(masm_->CodeObject()));

  Label load_char_start_regexp, start_regexp;
  // Load newline if index is at start, previous character otherwise.
  __ Cbnz(start_offset(), &load_char_start_regexp);
  __ Mov(current_character(), '\n');
  __ B(&start_regexp);

  // Global regexp restarts matching here.
  __ Bind(&load_char_start_regexp);
  // Load previous char as initial value of current character register.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ Bind(&start_regexp);
  // Initialize on-stack registers.
  if (num_saved_registers_ > 0) {
    ClearRegisters(0, num_saved_registers_ - 1);
  }

  // Initialize backtrack stack pointer.
  __ Ldr(backtrack_stackpointer(), MemOperand(frame_pointer(), kStackBase));

  // Execute
  __ B(&start_label_);

  if (backtrack_label_.is_linked()) {
    __ Bind(&backtrack_label_);
    Backtrack();
  }

  if (success_label_.is_linked()) {
    Register first_capture_start = w15;

    // Save captures when successful.
    __ Bind(&success_label_);

    if (num_saved_registers_ > 0) {
      // V8 expects the output to be an int32_t array.
      Register capture_start = w12;
      Register capture_end = w13;
      Register input_length = w14;

      // Copy captures to output.

      // Get string length.
      __ Sub(x10, input_end(), input_start());
      if (masm_->emit_debug_code()) {
        // Check that the input string length is < 2^30.
        __ Cmp(x10, (1<<30) - 1);
        __ Check(ls, kInputStringTooLong);
      }
      // input_start has a start_offset offset on entry. We need to include
      // it when computing the length of the whole string.
      if (mode_ == UC16) {
        __ Add(input_length, start_offset(), Operand(w10, LSR, 1));
      } else {
        __ Add(input_length, start_offset(), w10);
      }

      // Copy the results to the output array from the cached registers first.
      for (int i = 0;
           (i < num_saved_registers_) && (i < kNumCachedRegisters);
           i += 2) {
        __ Mov(capture_start.X(), GetCachedRegister(i));
        __ Lsr(capture_end.X(), capture_start.X(), kWRegSizeInBits);
        if ((i == 0) && global_with_zero_length_check()) {
          // Keep capture start for the zero-length check later.
          __ Mov(first_capture_start, capture_start);
        }
        // Offsets need to be relative to the start of the string.
        if (mode_ == UC16) {
          __ Add(capture_start, input_length, Operand(capture_start, ASR, 1));
          __ Add(capture_end, input_length, Operand(capture_end, ASR, 1));
        } else {
          __ Add(capture_start, input_length, capture_start);
          __ Add(capture_end, input_length, capture_end);
        }
        // The output pointer advances for a possible global match.
        __ Stp(capture_start,
               capture_end,
               MemOperand(output_array(), kPointerSize, PostIndex));
      }

      // Only carry on if there are more than kNumCachedRegisters capture
      // registers.
      int num_registers_left_on_stack =
          num_saved_registers_ - kNumCachedRegisters;
      if (num_registers_left_on_stack > 0) {
        Register base = x10;
        // There are always an even number of capture registers. A couple of
        // registers determine one match with two offsets.
        DCHECK_EQ(0, num_registers_left_on_stack % 2);
        __ Add(base, frame_pointer(), kFirstCaptureOnStack);

        // We can unroll the loop here, we should not unroll for less than 2
        // registers.
        STATIC_ASSERT(kNumRegistersToUnroll > 2);
        if (num_registers_left_on_stack <= kNumRegistersToUnroll) {
          for (int i = 0; i < num_registers_left_on_stack / 2; i++) {
            __ Ldp(capture_end,
                   capture_start,
                   MemOperand(base, -kPointerSize, PostIndex));
            if ((i == 0) && global_with_zero_length_check()) {
              // Keep capture start for the zero-length check later.
              __ Mov(first_capture_start, capture_start);
            }
            // Offsets need to be relative to the start of the string.
            if (mode_ == UC16) {
              __ Add(capture_start,
                     input_length,
                     Operand(capture_start, ASR, 1));
              __ Add(capture_end, input_length, Operand(capture_end, ASR, 1));
            } else {
              __ Add(capture_start, input_length, capture_start);
              __ Add(capture_end, input_length, capture_end);
            }
            // The output pointer advances for a possible global match.
            __ Stp(capture_start,
                   capture_end,
                   MemOperand(output_array(), kPointerSize, PostIndex));
          }
        } else {
          Label loop, start;
          __ Mov(x11, num_registers_left_on_stack);

          __ Ldp(capture_end,
                 capture_start,
                 MemOperand(base, -kPointerSize, PostIndex));
          if (global_with_zero_length_check()) {
            __ Mov(first_capture_start, capture_start);
          }
          __ B(&start);

          __ Bind(&loop);
          __ Ldp(capture_end,
                 capture_start,
                 MemOperand(base, -kPointerSize, PostIndex));
          __ Bind(&start);
          if (mode_ == UC16) {
            __ Add(capture_start, input_length, Operand(capture_start, ASR, 1));
            __ Add(capture_end, input_length, Operand(capture_end, ASR, 1));
          } else {
            __ Add(capture_start, input_length, capture_start);
            __ Add(capture_end, input_length, capture_end);
          }
          // The output pointer advances for a possible global match.
          __ Stp(capture_start,
                 capture_end,
                 MemOperand(output_array(), kPointerSize, PostIndex));
          __ Sub(x11, x11, 2);
          __ Cbnz(x11, &loop);
        }
      }
    }

    if (global()) {
      Register success_counter = w0;
      Register output_size = x10;
      // Restart matching if the regular expression is flagged as global.

      // Increment success counter.
      __ Ldr(success_counter, MemOperand(frame_pointer(), kSuccessCounter));
      __ Add(success_counter, success_counter, 1);
      __ Str(success_counter, MemOperand(frame_pointer(), kSuccessCounter));

      // Capture results have been stored, so the number of remaining global
      // output registers is reduced by the number of stored captures.
      __ Ldr(output_size, MemOperand(frame_pointer(), kOutputSize));
      __ Sub(output_size, output_size, num_saved_registers_);
      // Check whether we have enough room for another set of capture results.
      __ Cmp(output_size, num_saved_registers_);
      __ B(lt, &return_w0);

      // The output pointer is already set to the next field in the output
      // array.
      // Update output size on the frame before we restart matching.
      __ Str(output_size, MemOperand(frame_pointer(), kOutputSize));

      if (global_with_zero_length_check()) {
        // Special case for zero-length matches.
        __ Cmp(current_input_offset(), first_capture_start);
        // Not a zero-length match, restart.
        __ B(ne, &load_char_start_regexp);
        // Offset from the end is zero if we already reached the end.
        __ Cbz(current_input_offset(), &return_w0);
        // Advance current position after a zero-length match.
        __ Add(current_input_offset(),
               current_input_offset(),
               Operand((mode_ == UC16) ? 2 : 1));
      }

      __ B(&load_char_start_regexp);
    } else {
      __ Mov(w0, SUCCESS);
    }
  }

  if (exit_label_.is_linked()) {
    // Exit and return w0
    __ Bind(&exit_label_);
    if (global()) {
      __ Ldr(w0, MemOperand(frame_pointer(), kSuccessCounter));
    }
  }

  __ Bind(&return_w0);

  // Set stack pointer back to first register to retain
  DCHECK(csp.Is(__ StackPointer()));
  __ Mov(csp, fp);
  __ AssertStackConsistency();

  // Restore registers.
  __ PopCPURegList(registers_to_retain);

  __ Ret();

  Label exit_with_exception;
  // Registers x0 to x7 are used to store the first captures, they need to be
  // retained over calls to C++ code.
  CPURegList cached_registers(CPURegister::kRegister, kXRegSizeInBits, 0, 7);
  DCHECK((cached_registers.Count() * 2) == kNumCachedRegisters);

  if (check_preempt_label_.is_linked()) {
    __ Bind(&check_preempt_label_);
    SaveLinkRegister();
    // The cached registers need to be retained.
    __ PushCPURegList(cached_registers);
    CallCheckStackGuardState(x10);
    // Returning from the regexp code restores the stack (csp <- fp)
    // so we don't need to drop the link register from it before exiting.
    __ Cbnz(w0, &return_w0);
    // Reset the cached registers.
    __ PopCPURegList(cached_registers);
    RestoreLinkRegister();
    __ Ret();
  }

  if (stack_overflow_label_.is_linked()) {
    __ Bind(&stack_overflow_label_);
    SaveLinkRegister();
    // The cached registers need to be retained.
    __ PushCPURegList(cached_registers);
    // Call GrowStack(backtrack_stackpointer(), &stack_base)
    __ Mov(x2, ExternalReference::isolate_address(isolate()));
    __ Add(x1, frame_pointer(), kStackBase);
    __ Mov(x0, backtrack_stackpointer());
    ExternalReference grow_stack =
        ExternalReference::re_grow_stack(isolate());
    __ CallCFunction(grow_stack, 3);
    // If return NULL, we have failed to grow the stack, and
    // must exit with a stack-overflow exception.
    // Returning from the regexp code restores the stack (csp <- fp)
    // so we don't need to drop the link register from it before exiting.
    __ Cbz(w0, &exit_with_exception);
    // Otherwise use return value as new stack pointer.
    __ Mov(backtrack_stackpointer(), x0);
    // Reset the cached registers.
    __ PopCPURegList(cached_registers);
    RestoreLinkRegister();
    __ Ret();
  }

  if (exit_with_exception.is_linked()) {
    __ Bind(&exit_with_exception);
    __ Mov(w0, EXCEPTION);
    __ B(&return_w0);
  }

  CodeDesc code_desc;
  masm_->GetCode(&code_desc);
  Handle<Code> code = isolate()->factory()->NewCode(
      code_desc, Code::ComputeFlags(Code::REGEXP), masm_->CodeObject());
  PROFILE(masm_->isolate(), RegExpCodeCreateEvent(*code, *source));
  return Handle<HeapObject>::cast(code);
}


void RegExpMacroAssemblerARM64::GoTo(Label* to) {
  BranchOrBacktrack(al, to);
}

void RegExpMacroAssemblerARM64::IfRegisterGE(int reg, int comparand,
                                             Label* if_ge) {
  Register to_compare = GetRegister(reg, w10);
  CompareAndBranchOrBacktrack(to_compare, comparand, ge, if_ge);
}


void RegExpMacroAssemblerARM64::IfRegisterLT(int reg, int comparand,
                                             Label* if_lt) {
  Register to_compare = GetRegister(reg, w10);
  CompareAndBranchOrBacktrack(to_compare, comparand, lt, if_lt);
}


void RegExpMacroAssemblerARM64::IfRegisterEqPos(int reg, Label* if_eq) {
  Register to_compare = GetRegister(reg, w10);
  __ Cmp(to_compare, current_input_offset());
  BranchOrBacktrack(eq, if_eq);
}

RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerARM64::Implementation() {
  return kARM64Implementation;
}


void RegExpMacroAssemblerARM64::LoadCurrentCharacter(int cp_offset,
                                                     Label* on_end_of_input,
                                                     bool check_bounds,
                                                     int characters) {
  // TODO(pielan): Make sure long strings are caught before this, and not
  // just asserted in debug mode.
  // Be sane! (And ensure that an int32_t can be used to index the string)
  DCHECK(cp_offset < (1<<30));
  if (check_bounds) {
    if (cp_offset >= 0) {
      CheckPosition(cp_offset + characters - 1, on_end_of_input);
    } else {
      CheckPosition(cp_offset, on_end_of_input);
    }
  }
  LoadCurrentCharacterUnchecked(cp_offset, characters);
}


void RegExpMacroAssemblerARM64::PopCurrentPosition() {
  Pop(current_input_offset());
}


void RegExpMacroAssemblerARM64::PopRegister(int register_index) {
  Pop(w10);
  StoreRegister(register_index, w10);
}


void RegExpMacroAssemblerARM64::PushBacktrack(Label* label) {
  if (label->is_bound()) {
    int target = label->pos();
    __ Mov(w10, target + Code::kHeaderSize - kHeapObjectTag);
  } else {
    __ Adr(x10, label, MacroAssembler::kAdrFar);
    __ Sub(x10, x10, code_pointer());
    if (masm_->emit_debug_code()) {
      __ Cmp(x10, kWRegMask);
      // The code offset has to fit in a W register.
      __ Check(ls, kOffsetOutOfRange);
    }
  }
  Push(w10);
  CheckStackLimit();
}


void RegExpMacroAssemblerARM64::PushCurrentPosition() {
  Push(current_input_offset());
}


void RegExpMacroAssemblerARM64::PushRegister(int register_index,
                                             StackCheckFlag check_stack_limit) {
  Register to_push = GetRegister(register_index, w10);
  Push(to_push);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerARM64::ReadCurrentPositionFromRegister(int reg) {
  Register cached_register;
  RegisterState register_state = GetRegisterState(reg);
  switch (register_state) {
    case STACKED:
      __ Ldr(current_input_offset(), register_location(reg));
      break;
    case CACHED_LSW:
      cached_register = GetCachedRegister(reg);
      __ Mov(current_input_offset(), cached_register.W());
      break;
    case CACHED_MSW:
      cached_register = GetCachedRegister(reg);
      __ Lsr(current_input_offset().X(), cached_register, kWRegSizeInBits);
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void RegExpMacroAssemblerARM64::ReadStackPointerFromRegister(int reg) {
  Register read_from = GetRegister(reg, w10);
  __ Ldr(x11, MemOperand(frame_pointer(), kStackBase));
  __ Add(backtrack_stackpointer(), x11, Operand(read_from, SXTW));
}


void RegExpMacroAssemblerARM64::SetCurrentPositionFromEnd(int by) {
  Label after_position;
  __ Cmp(current_input_offset(), -by * char_size());
  __ B(ge, &after_position);
  __ Mov(current_input_offset(), -by * char_size());
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ Bind(&after_position);
}


void RegExpMacroAssemblerARM64::SetRegister(int register_index, int to) {
  DCHECK(register_index >= num_saved_registers_);  // Reserved for positions!
  Register set_to = wzr;
  if (to != 0) {
    set_to = w10;
    __ Mov(set_to, to);
  }
  StoreRegister(register_index, set_to);
}


bool RegExpMacroAssemblerARM64::Succeed() {
  __ B(&success_label_);
  return global();
}


void RegExpMacroAssemblerARM64::WriteCurrentPositionToRegister(int reg,
                                                               int cp_offset) {
  Register position = current_input_offset();
  if (cp_offset != 0) {
    position = w10;
    __ Add(position, current_input_offset(), cp_offset * char_size());
  }
  StoreRegister(reg, position);
}


void RegExpMacroAssemblerARM64::ClearRegisters(int reg_from, int reg_to) {
  DCHECK(reg_from <= reg_to);
  int num_registers = reg_to - reg_from + 1;

  // If the first capture register is cached in a hardware register but not
  // aligned on a 64-bit one, we need to clear the first one specifically.
  if ((reg_from < kNumCachedRegisters) && ((reg_from % 2) != 0)) {
    StoreRegister(reg_from, string_start_minus_one());
    num_registers--;
    reg_from++;
  }

  // Clear cached registers in pairs as far as possible.
  while ((num_registers >= 2) && (reg_from < kNumCachedRegisters)) {
    DCHECK(GetRegisterState(reg_from) == CACHED_LSW);
    __ Mov(GetCachedRegister(reg_from), twice_non_position_value());
    reg_from += 2;
    num_registers -= 2;
  }

  if ((num_registers % 2) == 1) {
    StoreRegister(reg_from, string_start_minus_one());
    num_registers--;
    reg_from++;
  }

  if (num_registers > 0) {
    // If there are some remaining registers, they are stored on the stack.
    DCHECK(reg_from >= kNumCachedRegisters);

    // Move down the indexes of the registers on stack to get the correct offset
    // in memory.
    reg_from -= kNumCachedRegisters;
    reg_to -= kNumCachedRegisters;
    // We should not unroll the loop for less than 2 registers.
    STATIC_ASSERT(kNumRegistersToUnroll > 2);
    // We position the base pointer to (reg_from + 1).
    int base_offset = kFirstRegisterOnStack -
        kWRegSize - (kWRegSize * reg_from);
    if (num_registers > kNumRegistersToUnroll) {
      Register base = x10;
      __ Add(base, frame_pointer(), base_offset);

      Label loop;
      __ Mov(x11, num_registers);
      __ Bind(&loop);
      __ Str(twice_non_position_value(),
             MemOperand(base, -kPointerSize, PostIndex));
      __ Sub(x11, x11, 2);
      __ Cbnz(x11, &loop);
    } else {
      for (int i = reg_from; i <= reg_to; i += 2) {
        __ Str(twice_non_position_value(),
               MemOperand(frame_pointer(), base_offset));
        base_offset -= kWRegSize * 2;
      }
    }
  }
}


void RegExpMacroAssemblerARM64::WriteStackPointerToRegister(int reg) {
  __ Ldr(x10, MemOperand(frame_pointer(), kStackBase));
  __ Sub(x10, backtrack_stackpointer(), x10);
  if (masm_->emit_debug_code()) {
    __ Cmp(x10, Operand(w10, SXTW));
    // The stack offset needs to fit in a W register.
    __ Check(eq, kOffsetOutOfRange);
  }
  StoreRegister(reg, w10);
}


// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  return *reinterpret_cast<T*>(re_frame + frame_offset);
}


template <typename T>
static T* frame_entry_address(Address re_frame, int frame_offset) {
  return reinterpret_cast<T*>(re_frame + frame_offset);
}


int RegExpMacroAssemblerARM64::CheckStackGuardState(
    Address* return_address, Code* re_code, Address re_frame, int start_index,
    const byte** input_start, const byte** input_end) {
  return NativeRegExpMacroAssembler::CheckStackGuardState(
      frame_entry<Isolate*>(re_frame, kIsolate), start_index,
      frame_entry<int>(re_frame, kDirectCall) == 1, return_address, re_code,
      frame_entry_address<String*>(re_frame, kInput), input_start, input_end);
}


void RegExpMacroAssemblerARM64::CheckPosition(int cp_offset,
                                              Label* on_outside_input) {
  if (cp_offset >= 0) {
    CompareAndBranchOrBacktrack(current_input_offset(),
                                -cp_offset * char_size(), ge, on_outside_input);
  } else {
    __ Add(w12, current_input_offset(), Operand(cp_offset * char_size()));
    __ Cmp(w12, string_start_minus_one());
    BranchOrBacktrack(le, on_outside_input);
  }
}


bool RegExpMacroAssemblerARM64::CanReadUnaligned() {
  // TODO(pielan): See whether or not we should disable unaligned accesses.
  return !slow_safe();
}


// Private methods:

void RegExpMacroAssemblerARM64::CallCheckStackGuardState(Register scratch) {
  // Allocate space on the stack to store the return address. The
  // CheckStackGuardState C++ function will override it if the code
  // moved. Allocate extra space for 2 arguments passed by pointers.
  // AAPCS64 requires the stack to be 16 byte aligned.
  int alignment = masm_->ActivationFrameAlignment();
  DCHECK_EQ(alignment % 16, 0);
  int align_mask = (alignment / kXRegSize) - 1;
  int xreg_to_claim = (3 + align_mask) & ~align_mask;

  DCHECK(csp.Is(__ StackPointer()));
  __ Claim(xreg_to_claim);

  // CheckStackGuardState needs the end and start addresses of the input string.
  __ Poke(input_end(), 2 * kPointerSize);
  __ Add(x5, csp, 2 * kPointerSize);
  __ Poke(input_start(), kPointerSize);
  __ Add(x4, csp, kPointerSize);

  __ Mov(w3, start_offset());
  // RegExp code frame pointer.
  __ Mov(x2, frame_pointer());
  // Code* of self.
  __ Mov(x1, Operand(masm_->CodeObject()));

  // We need to pass a pointer to the return address as first argument.
  // The DirectCEntry stub will place the return address on the stack before
  // calling so the stack pointer will point to it.
  __ Mov(x0, csp);

  ExternalReference check_stack_guard_state =
      ExternalReference::re_check_stack_guard_state(isolate());
  __ Mov(scratch, check_stack_guard_state);
  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm_, scratch);

  // The input string may have been moved in memory, we need to reload it.
  __ Peek(input_start(), kPointerSize);
  __ Peek(input_end(), 2 * kPointerSize);

  DCHECK(csp.Is(__ StackPointer()));
  __ Drop(xreg_to_claim);

  // Reload the Code pointer.
  __ Mov(code_pointer(), Operand(masm_->CodeObject()));
}

void RegExpMacroAssemblerARM64::BranchOrBacktrack(Condition condition,
                                                  Label* to) {
  if (condition == al) {  // Unconditional.
    if (to == NULL) {
      Backtrack();
      return;
    }
    __ B(to);
    return;
  }
  if (to == NULL) {
    to = &backtrack_label_;
  }
  __ B(condition, to);
}

void RegExpMacroAssemblerARM64::CompareAndBranchOrBacktrack(Register reg,
                                                            int immediate,
                                                            Condition condition,
                                                            Label* to) {
  if ((immediate == 0) && ((condition == eq) || (condition == ne))) {
    if (to == NULL) {
      to = &backtrack_label_;
    }
    if (condition == eq) {
      __ Cbz(reg, to);
    } else {
      __ Cbnz(reg, to);
    }
  } else {
    __ Cmp(reg, immediate);
    BranchOrBacktrack(condition, to);
  }
}


void RegExpMacroAssemblerARM64::CheckPreemption() {
  // Check for preemption.
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ Mov(x10, stack_limit);
  __ Ldr(x10, MemOperand(x10));
  DCHECK(csp.Is(__ StackPointer()));
  __ Cmp(csp, x10);
  CallIf(&check_preempt_label_, ls);
}


void RegExpMacroAssemblerARM64::CheckStackLimit() {
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit(isolate());
  __ Mov(x10, stack_limit);
  __ Ldr(x10, MemOperand(x10));
  __ Cmp(backtrack_stackpointer(), x10);
  CallIf(&stack_overflow_label_, ls);
}


void RegExpMacroAssemblerARM64::Push(Register source) {
  DCHECK(source.Is32Bits());
  DCHECK(!source.is(backtrack_stackpointer()));
  __ Str(source,
         MemOperand(backtrack_stackpointer(),
                    -static_cast<int>(kWRegSize),
                    PreIndex));
}


void RegExpMacroAssemblerARM64::Pop(Register target) {
  DCHECK(target.Is32Bits());
  DCHECK(!target.is(backtrack_stackpointer()));
  __ Ldr(target,
         MemOperand(backtrack_stackpointer(), kWRegSize, PostIndex));
}


Register RegExpMacroAssemblerARM64::GetCachedRegister(int register_index) {
  DCHECK(register_index < kNumCachedRegisters);
  return Register::Create(register_index / 2, kXRegSizeInBits);
}


Register RegExpMacroAssemblerARM64::GetRegister(int register_index,
                                                Register maybe_result) {
  DCHECK(maybe_result.Is32Bits());
  DCHECK(register_index >= 0);
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  Register result;
  RegisterState register_state = GetRegisterState(register_index);
  switch (register_state) {
    case STACKED:
      __ Ldr(maybe_result, register_location(register_index));
      result = maybe_result;
      break;
    case CACHED_LSW:
      result = GetCachedRegister(register_index).W();
      break;
    case CACHED_MSW:
      __ Lsr(maybe_result.X(), GetCachedRegister(register_index),
             kWRegSizeInBits);
      result = maybe_result;
      break;
    default:
      UNREACHABLE();
      break;
  }
  DCHECK(result.Is32Bits());
  return result;
}


void RegExpMacroAssemblerARM64::StoreRegister(int register_index,
                                              Register source) {
  DCHECK(source.Is32Bits());
  DCHECK(register_index >= 0);
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }

  Register cached_register;
  RegisterState register_state = GetRegisterState(register_index);
  switch (register_state) {
    case STACKED:
      __ Str(source, register_location(register_index));
      break;
    case CACHED_LSW:
      cached_register = GetCachedRegister(register_index);
      if (!source.Is(cached_register.W())) {
        __ Bfi(cached_register, source.X(), 0, kWRegSizeInBits);
      }
      break;
    case CACHED_MSW:
      cached_register = GetCachedRegister(register_index);
      __ Bfi(cached_register, source.X(), kWRegSizeInBits, kWRegSizeInBits);
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void RegExpMacroAssemblerARM64::CallIf(Label* to, Condition condition) {
  Label skip_call;
  if (condition != al) __ B(&skip_call, NegateCondition(condition));
  __ Bl(to);
  __ Bind(&skip_call);
}


void RegExpMacroAssemblerARM64::RestoreLinkRegister() {
  DCHECK(csp.Is(__ StackPointer()));
  __ Pop(lr, xzr);
  __ Add(lr, lr, Operand(masm_->CodeObject()));
}


void RegExpMacroAssemblerARM64::SaveLinkRegister() {
  DCHECK(csp.Is(__ StackPointer()));
  __ Sub(lr, lr, Operand(masm_->CodeObject()));
  __ Push(xzr, lr);
}


MemOperand RegExpMacroAssemblerARM64::register_location(int register_index) {
  DCHECK(register_index < (1<<30));
  DCHECK(register_index >= kNumCachedRegisters);
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  register_index -= kNumCachedRegisters;
  int offset = kFirstRegisterOnStack - register_index * kWRegSize;
  return MemOperand(frame_pointer(), offset);
}

MemOperand RegExpMacroAssemblerARM64::capture_location(int register_index,
                                                     Register scratch) {
  DCHECK(register_index < (1<<30));
  DCHECK(register_index < num_saved_registers_);
  DCHECK(register_index >= kNumCachedRegisters);
  DCHECK_EQ(register_index % 2, 0);
  register_index -= kNumCachedRegisters;
  int offset = kFirstCaptureOnStack - register_index * kWRegSize;
  // capture_location is used with Stp instructions to load/store 2 registers.
  // The immediate field in the encoding is limited to 7 bits (signed).
  if (is_int7(offset)) {
    return MemOperand(frame_pointer(), offset);
  } else {
    __ Add(scratch, frame_pointer(), offset);
    return MemOperand(scratch);
  }
}

void RegExpMacroAssemblerARM64::LoadCurrentCharacterUnchecked(int cp_offset,
                                                              int characters) {
  Register offset = current_input_offset();

  // The ldr, str, ldrh, strh instructions can do unaligned accesses, if the CPU
  // and the operating system running on the target allow it.
  // If unaligned load/stores are not supported then this function must only
  // be used to load a single character at a time.

  // ARMv8 supports unaligned accesses but V8 or the kernel can decide to
  // disable it.
  // TODO(pielan): See whether or not we should disable unaligned accesses.
  if (!CanReadUnaligned()) {
    DCHECK(characters == 1);
  }

  if (cp_offset != 0) {
    if (masm_->emit_debug_code()) {
      __ Mov(x10, cp_offset * char_size());
      __ Add(x10, x10, Operand(current_input_offset(), SXTW));
      __ Cmp(x10, Operand(w10, SXTW));
      // The offset needs to fit in a W register.
      __ Check(eq, kOffsetOutOfRange);
    } else {
      __ Add(w10, current_input_offset(), cp_offset * char_size());
    }
    offset = w10;
  }

  if (mode_ == LATIN1) {
    if (characters == 4) {
      __ Ldr(current_character(), MemOperand(input_end(), offset, SXTW));
    } else if (characters == 2) {
      __ Ldrh(current_character(), MemOperand(input_end(), offset, SXTW));
    } else {
      DCHECK(characters == 1);
      __ Ldrb(current_character(), MemOperand(input_end(), offset, SXTW));
    }
  } else {
    DCHECK(mode_ == UC16);
    if (characters == 2) {
      __ Ldr(current_character(), MemOperand(input_end(), offset, SXTW));
    } else {
      DCHECK(characters == 1);
      __ Ldrh(current_character(), MemOperand(input_end(), offset, SXTW));
    }
  }
}

#endif  // V8_INTERPRETED_REGEXP

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64

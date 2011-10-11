// Copyright 2006-2010 the V8 project authors. All rights reserved.
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

#include "v8.h"

#if defined(V8_TARGET_ARCH_MIPS)

#include "unicode.h"
#include "log.h"
#include "code-stubs.h"
#include "regexp-stack.h"
#include "macro-assembler.h"
#include "regexp-macro-assembler.h"
#include "mips/regexp-macro-assembler-mips.h"

namespace v8 {
namespace internal {

#ifndef V8_INTERPRETED_REGEXP
/*
 * This assembler uses the following register assignment convention
 * - t1 : Pointer to current code object (Code*) including heap object tag.
 * - t2 : Current position in input, as negative offset from end of string.
 *        Please notice that this is the byte offset, not the character offset!
 * - t3 : Currently loaded character. Must be loaded using
 *        LoadCurrentCharacter before using any of the dispatch methods.
 * - t4 : points to tip of backtrack stack
 * - t5 : Unused.
 * - t6 : End of input (points to byte after last character in input).
 * - fp : Frame pointer. Used to access arguments, local variables and
 *         RegExp registers.
 * - sp : points to tip of C stack.
 *
 * The remaining registers are free for computations.
 * Each call to a public method should retain this convention.
 *
 * The stack will have the following structure:
 *
 *  - fp[56]  direct_call  (if 1, direct call from JavaScript code,
 *                          if 0, call through the runtime system).
 *  - fp[52]  stack_area_base (High end of the memory area to use as
 *                             backtracking stack).
 *  - fp[48]  int* capture_array (int[num_saved_registers_], for output).
 *  - fp[44]  secondary link/return address used by native call.
 *  --- sp when called ---
 *  - fp[40]  return address (lr).
 *  - fp[36]  old frame pointer (r11).
 *  - fp[0..32]  backup of registers s0..s7.
 *  --- frame pointer ----
 *  - fp[-4]  end of input       (Address of end of string).
 *  - fp[-8]  start of input     (Address of first character in string).
 *  - fp[-12] start index        (character index of start).
 *  - fp[-16] void* input_string (location of a handle containing the string).
 *  - fp[-20] Offset of location before start of input (effectively character
 *            position -1). Used to initialize capture registers to a
 *            non-position.
 *  - fp[-24] At start (if 1, we are starting at the start of the
 *    string, otherwise 0)
 *  - fp[-28] register 0         (Only positions must be stored in the first
 *  -         register 1          num_saved_registers_ registers)
 *  -         ...
 *  -         register num_registers-1
 *  --- sp ---
 *
 * The first num_saved_registers_ registers are initialized to point to
 * "character -1" in the string (i.e., char_size() bytes before the first
 * character of the string). The remaining registers start out as garbage.
 *
 * The data up to the return address must be placed there by the calling
 * code and the remaining arguments are passed in registers, e.g. by calling the
 * code entry as cast to a function with the signature:
 * int (*match)(String* input_string,
 *              int start_index,
 *              Address start,
 *              Address end,
 *              Address secondary_return_address,  // Only used by native call.
 *              int* capture_output_array,
 *              byte* stack_area_base,
 *              bool direct_call = false)
 * The call is performed by NativeRegExpMacroAssembler::Execute()
 * (in regexp-macro-assembler.cc) via the CALL_GENERATED_REGEXP_CODE macro
 * in mips/simulator-mips.h.
 * When calling as a non-direct call (i.e., from C++ code), the return address
 * area is overwritten with the ra register by the RegExp code. When doing a
 * direct call from generated code, the return address is placed there by
 * the calling code, as in a normal exit frame.
 */

#define __ ACCESS_MASM(masm_)

RegExpMacroAssemblerMIPS::RegExpMacroAssemblerMIPS(
    Mode mode,
    int registers_to_save)
    : masm_(new MacroAssembler(Isolate::Current(), NULL, kRegExpCodeSize)),
      mode_(mode),
      num_registers_(registers_to_save),
      num_saved_registers_(registers_to_save),
      entry_label_(),
      start_label_(),
      success_label_(),
      backtrack_label_(),
      exit_label_(),
      internal_failure_label_() {
  ASSERT_EQ(0, registers_to_save % 2);
  __ jmp(&entry_label_);   // We'll write the entry code later.
  // If the code gets too big or corrupted, an internal exception will be
  // raised, and we will exit right away.
  __ bind(&internal_failure_label_);
  __ li(v0, Operand(FAILURE));
  __ Ret();
  __ bind(&start_label_);  // And then continue from here.
}


RegExpMacroAssemblerMIPS::~RegExpMacroAssemblerMIPS() {
  delete masm_;
  // Unuse labels in case we throw away the assembler without calling GetCode.
  entry_label_.Unuse();
  start_label_.Unuse();
  success_label_.Unuse();
  backtrack_label_.Unuse();
  exit_label_.Unuse();
  check_preempt_label_.Unuse();
  stack_overflow_label_.Unuse();
  internal_failure_label_.Unuse();
}


int RegExpMacroAssemblerMIPS::stack_limit_slack()  {
  return RegExpStack::kStackLimitSlack;
}


void RegExpMacroAssemblerMIPS::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    __ Addu(current_input_offset(),
           current_input_offset(), Operand(by * char_size()));
  }
}


void RegExpMacroAssemblerMIPS::AdvanceRegister(int reg, int by) {
  ASSERT(reg >= 0);
  ASSERT(reg < num_registers_);
  if (by != 0) {
    __ lw(a0, register_location(reg));
    __ Addu(a0, a0, Operand(by));
    __ sw(a0, register_location(reg));
  }
}


void RegExpMacroAssemblerMIPS::Backtrack() {
  CheckPreemption();
  // Pop Code* offset from backtrack stack, add Code* and jump to location.
  Pop(a0);
  __ Addu(a0, a0, code_pointer());
  __ Jump(a0);
}


void RegExpMacroAssemblerMIPS::Bind(Label* label) {
  __ bind(label);
}


void RegExpMacroAssemblerMIPS::CheckCharacter(uint32_t c, Label* on_equal) {
  BranchOrBacktrack(on_equal, eq, current_character(), Operand(c));
}


void RegExpMacroAssemblerMIPS::CheckCharacterGT(uc16 limit, Label* on_greater) {
  BranchOrBacktrack(on_greater, gt, current_character(), Operand(limit));
}


void RegExpMacroAssemblerMIPS::CheckAtStart(Label* on_at_start) {
  Label not_at_start;
  // Did we start the match at the start of the string at all?
  __ lw(a0, MemOperand(frame_pointer(), kAtStart));
  BranchOrBacktrack(&not_at_start, eq, a0, Operand(zero_reg));

  // If we did, are we still at the start of the input?
  __ lw(a1, MemOperand(frame_pointer(), kInputStart));
  __ Addu(a0, end_of_input_address(), Operand(current_input_offset()));
  BranchOrBacktrack(on_at_start, eq, a0, Operand(a1));
  __ bind(&not_at_start);
}


void RegExpMacroAssemblerMIPS::CheckNotAtStart(Label* on_not_at_start) {
  // Did we start the match at the start of the string at all?
  __ lw(a0, MemOperand(frame_pointer(), kAtStart));
  BranchOrBacktrack(on_not_at_start, eq, a0, Operand(zero_reg));
  // If we did, are we still at the start of the input?
  __ lw(a1, MemOperand(frame_pointer(), kInputStart));
  __ Addu(a0, end_of_input_address(), Operand(current_input_offset()));
  BranchOrBacktrack(on_not_at_start, ne, a0, Operand(a1));
}


void RegExpMacroAssemblerMIPS::CheckCharacterLT(uc16 limit, Label* on_less) {
  BranchOrBacktrack(on_less, lt, current_character(), Operand(limit));
}


void RegExpMacroAssemblerMIPS::CheckCharacters(Vector<const uc16> str,
                                              int cp_offset,
                                              Label* on_failure,
                                              bool check_end_of_string) {
  if (on_failure == NULL) {
    // Instead of inlining a backtrack for each test, (re)use the global
    // backtrack target.
    on_failure = &backtrack_label_;
  }

  if (check_end_of_string) {
    // Is last character of required match inside string.
    CheckPosition(cp_offset + str.length() - 1, on_failure);
  }

  __ Addu(a0, end_of_input_address(), Operand(current_input_offset()));
  if (cp_offset != 0) {
    int byte_offset = cp_offset * char_size();
    __ Addu(a0, a0, Operand(byte_offset));
  }

  // a0 : Address of characters to match against str.
  int stored_high_byte = 0;
  for (int i = 0; i < str.length(); i++) {
    if (mode_ == ASCII) {
      __ lbu(a1, MemOperand(a0, 0));
      __ addiu(a0, a0, char_size());
      ASSERT(str[i] <= String::kMaxAsciiCharCode);
      BranchOrBacktrack(on_failure, ne, a1, Operand(str[i]));
    } else {
      __ lhu(a1, MemOperand(a0, 0));
      __ addiu(a0, a0, char_size());
      uc16 match_char = str[i];
      int match_high_byte = (match_char >> 8);
      if (match_high_byte == 0) {
        BranchOrBacktrack(on_failure, ne, a1, Operand(str[i]));
      } else {
        if (match_high_byte != stored_high_byte) {
          __ li(a2, Operand(match_high_byte));
          stored_high_byte = match_high_byte;
        }
        __ Addu(a3, a2, Operand(match_char & 0xff));
        BranchOrBacktrack(on_failure, ne, a1, Operand(a3));
      }
    }
  }
}


void RegExpMacroAssemblerMIPS::CheckGreedyLoop(Label* on_equal) {
  Label backtrack_non_equal;
  __ lw(a0, MemOperand(backtrack_stackpointer(), 0));
  __ Branch(&backtrack_non_equal, ne, current_input_offset(), Operand(a0));
  __ Addu(backtrack_stackpointer(),
          backtrack_stackpointer(),
          Operand(kPointerSize));
  __ bind(&backtrack_non_equal);
  BranchOrBacktrack(on_equal, eq, current_input_offset(), Operand(a0));
}


void RegExpMacroAssemblerMIPS::CheckNotBackReferenceIgnoreCase(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  __ lw(a0, register_location(start_reg));  // Index of start of capture.
  __ lw(a1, register_location(start_reg + 1));  // Index of end of capture.
  __ Subu(a1, a1, a0);  // Length of capture.

  // If length is zero, either the capture is empty or it is not participating.
  // In either case succeed immediately.
  __ Branch(&fallthrough, eq, a1, Operand(zero_reg));

  __ Addu(t5, a1, current_input_offset());
  // Check that there are enough characters left in the input.
  BranchOrBacktrack(on_no_match, gt, t5, Operand(zero_reg));

  if (mode_ == ASCII) {
    Label success;
    Label fail;
    Label loop_check;

    // a0 - offset of start of capture.
    // a1 - length of capture.
    __ Addu(a0, a0, Operand(end_of_input_address()));
    __ Addu(a2, end_of_input_address(), Operand(current_input_offset()));
    __ Addu(a1, a0, Operand(a1));

    // a0 - Address of start of capture.
    // a1 - Address of end of capture.
    // a2 - Address of current input position.

    Label loop;
    __ bind(&loop);
    __ lbu(a3, MemOperand(a0, 0));
    __ addiu(a0, a0, char_size());
    __ lbu(t0, MemOperand(a2, 0));
    __ addiu(a2, a2, char_size());

    __ Branch(&loop_check, eq, t0, Operand(a3));

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ Or(a3, a3, Operand(0x20));  // Convert capture character to lower-case.
    __ Or(t0, t0, Operand(0x20));  // Also convert input character.
    __ Branch(&fail, ne, t0, Operand(a3));
    __ Subu(a3, a3, Operand('a'));
    __ Branch(&fail, hi, a3, Operand('z' - 'a'));  // Is a3 a lowercase letter?

    __ bind(&loop_check);
    __ Branch(&loop, lt, a0, Operand(a1));
    __ jmp(&success);

    __ bind(&fail);
    GoTo(on_no_match);

    __ bind(&success);
    // Compute new value of character position after the matched part.
    __ Subu(current_input_offset(), a2, end_of_input_address());
  } else {
    ASSERT(mode_ == UC16);
    // Put regexp engine registers on stack.
    RegList regexp_registers_to_retain = current_input_offset().bit() |
        current_character().bit() | backtrack_stackpointer().bit();
    __ MultiPush(regexp_registers_to_retain);

    int argument_count = 4;
    __ PrepareCallCFunction(argument_count, a2);

    // a0 - offset of start of capture.
    // a1 - length of capture.

    // Put arguments into arguments registers.
    // Parameters are
    //   a0: Address byte_offset1 - Address captured substring's start.
    //   a1: Address byte_offset2 - Address of current character position.
    //   a2: size_t byte_length - length of capture in bytes(!).
    //   a3: Isolate* isolate.

    // Address of start of capture.
    __ Addu(a0, a0, Operand(end_of_input_address()));
    // Length of capture.
    __ mov(a2, a1);
    // Save length in callee-save register for use on return.
    __ mov(s3, a1);
    // Address of current input position.
    __ Addu(a1, current_input_offset(), Operand(end_of_input_address()));
    // Isolate.
    __ li(a3, Operand(ExternalReference::isolate_address()));

    ExternalReference function =
        ExternalReference::re_case_insensitive_compare_uc16(masm_->isolate());
    __ CallCFunction(function, argument_count);

    // Restore regexp engine registers.
    __ MultiPop(regexp_registers_to_retain);
    __ li(code_pointer(), Operand(masm_->CodeObject()));
    __ lw(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));

    // Check if function returned non-zero for success or zero for failure.
    BranchOrBacktrack(on_no_match, eq, v0, Operand(zero_reg));
    // On success, increment position by length of capture.
    __ Addu(current_input_offset(), current_input_offset(), Operand(s3));
  }

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerMIPS::CheckNotBackReference(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  Label success;

  // Find length of back-referenced capture.
  __ lw(a0, register_location(start_reg));
  __ lw(a1, register_location(start_reg + 1));
  __ Subu(a1, a1, a0);  // Length to check.
  // Succeed on empty capture (including no capture).
  __ Branch(&fallthrough, eq, a1, Operand(zero_reg));

  __ Addu(t5, a1, current_input_offset());
  // Check that there are enough characters left in the input.
  BranchOrBacktrack(on_no_match, gt, t5, Operand(zero_reg));

  // Compute pointers to match string and capture string.
  __ Addu(a0, a0, Operand(end_of_input_address()));
  __ Addu(a2, end_of_input_address(), Operand(current_input_offset()));
  __ Addu(a1, a1, Operand(a0));

  Label loop;
  __ bind(&loop);
  if (mode_ == ASCII) {
    __ lbu(a3, MemOperand(a0, 0));
    __ addiu(a0, a0, char_size());
    __ lbu(t0, MemOperand(a2, 0));
    __ addiu(a2, a2, char_size());
  } else {
    ASSERT(mode_ == UC16);
    __ lhu(a3, MemOperand(a0, 0));
    __ addiu(a0, a0, char_size());
    __ lhu(t0, MemOperand(a2, 0));
    __ addiu(a2, a2, char_size());
  }
  BranchOrBacktrack(on_no_match, ne, a3, Operand(t0));
  __ Branch(&loop, lt, a0, Operand(a1));

  // Move current character position to position after match.
  __ Subu(current_input_offset(), a2, end_of_input_address());
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerMIPS::CheckNotRegistersEqual(int reg1,
                                                      int reg2,
                                                      Label* on_not_equal) {
  UNIMPLEMENTED_MIPS();
}


void RegExpMacroAssemblerMIPS::CheckNotCharacter(uint32_t c,
                                                Label* on_not_equal) {
  BranchOrBacktrack(on_not_equal, ne, current_character(), Operand(c));
}


void RegExpMacroAssemblerMIPS::CheckCharacterAfterAnd(uint32_t c,
                                                     uint32_t mask,
                                                     Label* on_equal) {
  __ And(a0, current_character(), Operand(mask));
  BranchOrBacktrack(on_equal, eq, a0, Operand(c));
}


void RegExpMacroAssemblerMIPS::CheckNotCharacterAfterAnd(uint32_t c,
                                                        uint32_t mask,
                                                        Label* on_not_equal) {
  __ And(a0, current_character(), Operand(mask));
  BranchOrBacktrack(on_not_equal, ne, a0, Operand(c));
}


void RegExpMacroAssemblerMIPS::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  UNIMPLEMENTED_MIPS();
}


bool RegExpMacroAssemblerMIPS::CheckSpecialCharacterClass(uc16 type,
                                                         Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check.
  switch (type) {
  case 's':
    // Match space-characters.
    if (mode_ == ASCII) {
      // ASCII space characters are '\t'..'\r' and ' '.
      Label success;
      __ Branch(&success, eq, current_character(), Operand(' '));
      // Check range 0x09..0x0d.
      __ Subu(a0, current_character(), Operand('\t'));
      BranchOrBacktrack(on_no_match, hi, a0, Operand('\r' - '\t'));
      __ bind(&success);
      return true;
    }
    return false;
  case 'S':
    // Match non-space characters.
    if (mode_ == ASCII) {
      // ASCII space characters are '\t'..'\r' and ' '.
      BranchOrBacktrack(on_no_match, eq, current_character(), Operand(' '));
      __ Subu(a0, current_character(), Operand('\t'));
      BranchOrBacktrack(on_no_match, ls, a0, Operand('\r' - '\t'));
      return true;
    }
    return false;
  case 'd':
    // Match ASCII digits ('0'..'9').
    __ Subu(a0, current_character(), Operand('0'));
    BranchOrBacktrack(on_no_match, hi, a0, Operand('9' - '0'));
    return true;
  case 'D':
    // Match non ASCII-digits.
    __ Subu(a0, current_character(), Operand('0'));
    BranchOrBacktrack(on_no_match, ls, a0, Operand('9' - '0'));
    return true;
  case '.': {
    // Match non-newlines (not 0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029).
    __ Xor(a0, current_character(), Operand(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c.
    __ Subu(a0, a0, Operand(0x0b));
    BranchOrBacktrack(on_no_match, ls, a0, Operand(0x0c - 0x0b));
    if (mode_ == UC16) {
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ Subu(a0, a0, Operand(0x2028 - 0x0b));
      BranchOrBacktrack(on_no_match, ls, a0, Operand(1));
    }
    return true;
  }
  case 'n': {
    // Match newlines (0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029).
    __ Xor(a0, current_character(), Operand(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c.
    __ Subu(a0, a0, Operand(0x0b));
    if (mode_ == ASCII) {
      BranchOrBacktrack(on_no_match, hi, a0, Operand(0x0c - 0x0b));
    } else {
      Label done;
      BranchOrBacktrack(&done, ls, a0, Operand(0x0c - 0x0b));
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ Subu(a0, a0, Operand(0x2028 - 0x0b));
      BranchOrBacktrack(on_no_match, hi, a0, Operand(1));
      __ bind(&done);
    }
    return true;
  }
  case 'w': {
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      BranchOrBacktrack(on_no_match, hi, current_character(), Operand('z'));
    }
    ExternalReference map = ExternalReference::re_word_character_map();
    __ li(a0, Operand(map));
    __ Addu(a0, a0, current_character());
    __ lbu(a0, MemOperand(a0, 0));
    BranchOrBacktrack(on_no_match, eq, a0, Operand(zero_reg));
    return true;
  }
  case 'W': {
    Label done;
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      __ Branch(&done, hi, current_character(), Operand('z'));
    }
    ExternalReference map = ExternalReference::re_word_character_map();
    __ li(a0, Operand(map));
    __ Addu(a0, a0, current_character());
    __ lbu(a0, MemOperand(a0, 0));
    BranchOrBacktrack(on_no_match, ne, a0, Operand(zero_reg));
    if (mode_ != ASCII) {
      __ bind(&done);
    }
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


void RegExpMacroAssemblerMIPS::Fail() {
  __ li(v0, Operand(FAILURE));
  __ jmp(&exit_label_);
}


Handle<HeapObject> RegExpMacroAssemblerMIPS::GetCode(Handle<String> source) {
  if (masm_->has_exception()) {
    // If the code gets corrupted due to long regular expressions and lack of
    // space on trampolines, an internal exception flag is set. If this case
    // is detected, we will jump into exit sequence right away.
    __ bind_to(&entry_label_, internal_failure_label_.pos());
  } else {
    // Finalize code - write the entry point code now we know how many
    // registers we need.

    // Entry code:
    __ bind(&entry_label_);
    // Push arguments
    // Save callee-save registers.
    // Start new stack frame.
    // Store link register in existing stack-cell.
    // Order here should correspond to order of offset constants in header file.
    RegList registers_to_retain = s0.bit() | s1.bit() | s2.bit() |
        s3.bit() | s4.bit() | s5.bit() | s6.bit() | s7.bit() | fp.bit();
    RegList argument_registers = a0.bit() | a1.bit() | a2.bit() | a3.bit();
    __ MultiPush(argument_registers | registers_to_retain | ra.bit());
    // Set frame pointer in space for it if this is not a direct call
    // from generated code.
    __ Addu(frame_pointer(), sp, Operand(4 * kPointerSize));
    __ push(a0);  // Make room for "position - 1" constant (value irrelevant).
    __ push(a0);  // Make room for "at start" constant (value irrelevant).

    // Check if we have space on the stack for registers.
    Label stack_limit_hit;
    Label stack_ok;

    ExternalReference stack_limit =
        ExternalReference::address_of_stack_limit(masm_->isolate());
    __ li(a0, Operand(stack_limit));
    __ lw(a0, MemOperand(a0));
    __ Subu(a0, sp, a0);
    // Handle it if the stack pointer is already below the stack limit.
    __ Branch(&stack_limit_hit, le, a0, Operand(zero_reg));
    // Check if there is room for the variable number of registers above
    // the stack limit.
    __ Branch(&stack_ok, hs, a0, Operand(num_registers_ * kPointerSize));
    // Exit with OutOfMemory exception. There is not enough space on the stack
    // for our working registers.
    __ li(v0, Operand(EXCEPTION));
    __ jmp(&exit_label_);

    __ bind(&stack_limit_hit);
    CallCheckStackGuardState(a0);
    // If returned value is non-zero, we exit with the returned value as result.
    __ Branch(&exit_label_, ne, v0, Operand(zero_reg));

    __ bind(&stack_ok);
    // Allocate space on stack for registers.
    __ Subu(sp, sp, Operand(num_registers_ * kPointerSize));
    // Load string end.
    __ lw(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
    // Load input start.
    __ lw(a0, MemOperand(frame_pointer(), kInputStart));
    // Find negative length (offset of start relative to end).
    __ Subu(current_input_offset(), a0, end_of_input_address());
    // Set a0 to address of char before start of the input string
    // (effectively string position -1).
    __ lw(a1, MemOperand(frame_pointer(), kStartIndex));
    __ Subu(a0, current_input_offset(), Operand(char_size()));
    __ sll(t5, a1, (mode_ == UC16) ? 1 : 0);
    __ Subu(a0, a0, t5);
    // Store this value in a local variable, for use when clearing
    // position registers.
    __ sw(a0, MemOperand(frame_pointer(), kInputStartMinusOne));

    // Determine whether the start index is zero, that is at the start of the
    // string, and store that value in a local variable.
    __ mov(t5, a1);
    __ li(a1, Operand(1));
    __ movn(a1, zero_reg, t5);
    __ sw(a1, MemOperand(frame_pointer(), kAtStart));

    if (num_saved_registers_ > 0) {  // Always is, if generated from a regexp.
      // Fill saved registers with initial value = start offset - 1.

      // Address of register 0.
      __ Addu(a1, frame_pointer(), Operand(kRegisterZero));
      __ li(a2, Operand(num_saved_registers_));
      Label init_loop;
      __ bind(&init_loop);
      __ sw(a0, MemOperand(a1));
      __ Addu(a1, a1, Operand(-kPointerSize));
      __ Subu(a2, a2, Operand(1));
      __ Branch(&init_loop, ne, a2, Operand(zero_reg));
    }

    // Initialize backtrack stack pointer.
    __ lw(backtrack_stackpointer(), MemOperand(frame_pointer(), kStackHighEnd));
    // Initialize code pointer register
    __ li(code_pointer(), Operand(masm_->CodeObject()));
    // Load previous char as initial value of current character register.
    Label at_start;
    __ lw(a0, MemOperand(frame_pointer(), kAtStart));
    __ Branch(&at_start, ne, a0, Operand(zero_reg));
    LoadCurrentCharacterUnchecked(-1, 1);  // Load previous char.
    __ jmp(&start_label_);
    __ bind(&at_start);
    __ li(current_character(), Operand('\n'));
    __ jmp(&start_label_);


    // Exit code:
    if (success_label_.is_linked()) {
      // Save captures when successful.
      __ bind(&success_label_);
      if (num_saved_registers_ > 0) {
        // Copy captures to output.
        __ lw(a1, MemOperand(frame_pointer(), kInputStart));
        __ lw(a0, MemOperand(frame_pointer(), kRegisterOutput));
        __ lw(a2, MemOperand(frame_pointer(), kStartIndex));
        __ Subu(a1, end_of_input_address(), a1);
        // a1 is length of input in bytes.
        if (mode_ == UC16) {
          __ srl(a1, a1, 1);
        }
        // a1 is length of input in characters.
        __ Addu(a1, a1, Operand(a2));
        // a1 is length of string in characters.

        ASSERT_EQ(0, num_saved_registers_ % 2);
        // Always an even number of capture registers. This allows us to
        // unroll the loop once to add an operation between a load of a register
        // and the following use of that register.
        for (int i = 0; i < num_saved_registers_; i += 2) {
          __ lw(a2, register_location(i));
          __ lw(a3, register_location(i + 1));
          if (mode_ == UC16) {
            __ sra(a2, a2, 1);
            __ Addu(a2, a2, a1);
            __ sra(a3, a3, 1);
            __ Addu(a3, a3, a1);
          } else {
            __ Addu(a2, a1, Operand(a2));
            __ Addu(a3, a1, Operand(a3));
          }
          __ sw(a2, MemOperand(a0));
          __ Addu(a0, a0, kPointerSize);
          __ sw(a3, MemOperand(a0));
          __ Addu(a0, a0, kPointerSize);
        }
      }
      __ li(v0, Operand(SUCCESS));
    }
    // Exit and return v0.
    __ bind(&exit_label_);
    // Skip sp past regexp registers and local variables..
    __ mov(sp, frame_pointer());
    // Restore registers s0..s7 and return (restoring ra to pc).
    __ MultiPop(registers_to_retain | ra.bit());
    __ Ret();

    // Backtrack code (branch target for conditional backtracks).
    if (backtrack_label_.is_linked()) {
      __ bind(&backtrack_label_);
      Backtrack();
    }

    Label exit_with_exception;

    // Preempt-code.
    if (check_preempt_label_.is_linked()) {
      SafeCallTarget(&check_preempt_label_);
      // Put regexp engine registers on stack.
      RegList regexp_registers_to_retain = current_input_offset().bit() |
          current_character().bit() | backtrack_stackpointer().bit();
      __ MultiPush(regexp_registers_to_retain);
      CallCheckStackGuardState(a0);
      __ MultiPop(regexp_registers_to_retain);
      // If returning non-zero, we should end execution with the given
      // result as return value.
      __ Branch(&exit_label_, ne, v0, Operand(zero_reg));

      // String might have moved: Reload end of string from frame.
      __ lw(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
      __ li(code_pointer(), Operand(masm_->CodeObject()));
      SafeReturn();
    }

    // Backtrack stack overflow code.
    if (stack_overflow_label_.is_linked()) {
      SafeCallTarget(&stack_overflow_label_);
      // Reached if the backtrack-stack limit has been hit.
      // Put regexp engine registers on stack first.
      RegList regexp_registers = current_input_offset().bit() |
          current_character().bit();
      __ MultiPush(regexp_registers);
      Label grow_failed;
      // Call GrowStack(backtrack_stackpointer(), &stack_base)
      static const int num_arguments = 3;
      __ PrepareCallCFunction(num_arguments, a0);
      __ mov(a0, backtrack_stackpointer());
      __ Addu(a1, frame_pointer(), Operand(kStackHighEnd));
      __ li(a2, Operand(ExternalReference::isolate_address()));
      ExternalReference grow_stack =
          ExternalReference::re_grow_stack(masm_->isolate());
      __ CallCFunction(grow_stack, num_arguments);
      // Restore regexp registers.
      __ MultiPop(regexp_registers);
      // If return NULL, we have failed to grow the stack, and
      // must exit with a stack-overflow exception.
      __ Branch(&exit_with_exception, eq, v0, Operand(zero_reg));
      // Otherwise use return value as new stack pointer.
      __ mov(backtrack_stackpointer(), v0);
      // Restore saved registers and continue.
      __ li(code_pointer(), Operand(masm_->CodeObject()));
      __ lw(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
      SafeReturn();
    }

    if (exit_with_exception.is_linked()) {
      // If any of the code above needed to exit with an exception.
      __ bind(&exit_with_exception);
      // Exit with Result EXCEPTION(-1) to signal thrown exception.
      __ li(v0, Operand(EXCEPTION));
      __ jmp(&exit_label_);
    }
  }

  CodeDesc code_desc;
  masm_->GetCode(&code_desc);
  Handle<Code> code = FACTORY->NewCode(code_desc,
                                       Code::ComputeFlags(Code::REGEXP),
                                       masm_->CodeObject());
  LOG(Isolate::Current(), RegExpCodeCreateEvent(*code, *source));
  return Handle<HeapObject>::cast(code);
}


void RegExpMacroAssemblerMIPS::GoTo(Label* to) {
  if (to == NULL) {
    Backtrack();
    return;
  }
  __ jmp(to);
  return;
}


void RegExpMacroAssemblerMIPS::IfRegisterGE(int reg,
                                           int comparand,
                                           Label* if_ge) {
  __ lw(a0, register_location(reg));
    BranchOrBacktrack(if_ge, ge, a0, Operand(comparand));
}


void RegExpMacroAssemblerMIPS::IfRegisterLT(int reg,
                                           int comparand,
                                           Label* if_lt) {
  __ lw(a0, register_location(reg));
  BranchOrBacktrack(if_lt, lt, a0, Operand(comparand));
}


void RegExpMacroAssemblerMIPS::IfRegisterEqPos(int reg,
                                              Label* if_eq) {
  __ lw(a0, register_location(reg));
  BranchOrBacktrack(if_eq, eq, a0, Operand(current_input_offset()));
}


RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerMIPS::Implementation() {
  return kMIPSImplementation;
}


void RegExpMacroAssemblerMIPS::LoadCurrentCharacter(int cp_offset,
                                                   Label* on_end_of_input,
                                                   bool check_bounds,
                                                   int characters) {
  ASSERT(cp_offset >= -1);      // ^ and \b can look behind one character.
  ASSERT(cp_offset < (1<<30));  // Be sane! (And ensure negation works).
  if (check_bounds) {
    CheckPosition(cp_offset + characters - 1, on_end_of_input);
  }
  LoadCurrentCharacterUnchecked(cp_offset, characters);
}


void RegExpMacroAssemblerMIPS::PopCurrentPosition() {
  Pop(current_input_offset());
}


void RegExpMacroAssemblerMIPS::PopRegister(int register_index) {
  Pop(a0);
  __ sw(a0, register_location(register_index));
}


void RegExpMacroAssemblerMIPS::PushBacktrack(Label* label) {
  if (label->is_bound()) {
    int target = label->pos();
    __ li(a0, Operand(target + Code::kHeaderSize - kHeapObjectTag));
  } else {
    Label after_constant;
    __ Branch(&after_constant);
    int offset = masm_->pc_offset();
    int cp_offset = offset + Code::kHeaderSize - kHeapObjectTag;
    __ emit(0);
    masm_->label_at_put(label, offset);
    __ bind(&after_constant);
    if (is_int16(cp_offset)) {
      __ lw(a0, MemOperand(code_pointer(), cp_offset));
    } else {
      __ Addu(a0, code_pointer(), cp_offset);
      __ lw(a0, MemOperand(a0, 0));
    }
  }
  Push(a0);
  CheckStackLimit();
}


void RegExpMacroAssemblerMIPS::PushCurrentPosition() {
  Push(current_input_offset());
}


void RegExpMacroAssemblerMIPS::PushRegister(int register_index,
                                           StackCheckFlag check_stack_limit) {
  __ lw(a0, register_location(register_index));
  Push(a0);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerMIPS::ReadCurrentPositionFromRegister(int reg) {
  __ lw(current_input_offset(), register_location(reg));
}


void RegExpMacroAssemblerMIPS::ReadStackPointerFromRegister(int reg) {
  __ lw(backtrack_stackpointer(), register_location(reg));
  __ lw(a0, MemOperand(frame_pointer(), kStackHighEnd));
  __ Addu(backtrack_stackpointer(), backtrack_stackpointer(), Operand(a0));
}


void RegExpMacroAssemblerMIPS::SetCurrentPositionFromEnd(int by) {
  Label after_position;
  __ Branch(&after_position,
            ge,
            current_input_offset(),
            Operand(-by * char_size()));
  __ li(current_input_offset(), -by * char_size());
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&after_position);
}


void RegExpMacroAssemblerMIPS::SetRegister(int register_index, int to) {
  ASSERT(register_index >= num_saved_registers_);  // Reserved for positions!
  __ li(a0, Operand(to));
  __ sw(a0, register_location(register_index));
}


void RegExpMacroAssemblerMIPS::Succeed() {
  __ jmp(&success_label_);
}


void RegExpMacroAssemblerMIPS::WriteCurrentPositionToRegister(int reg,
                                                             int cp_offset) {
  if (cp_offset == 0) {
    __ sw(current_input_offset(), register_location(reg));
  } else {
    __ Addu(a0, current_input_offset(), Operand(cp_offset * char_size()));
    __ sw(a0, register_location(reg));
  }
}


void RegExpMacroAssemblerMIPS::ClearRegisters(int reg_from, int reg_to) {
  ASSERT(reg_from <= reg_to);
  __ lw(a0, MemOperand(frame_pointer(), kInputStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ sw(a0, register_location(reg));
  }
}


void RegExpMacroAssemblerMIPS::WriteStackPointerToRegister(int reg) {
  __ lw(a1, MemOperand(frame_pointer(), kStackHighEnd));
  __ Subu(a0, backtrack_stackpointer(), a1);
  __ sw(a0, register_location(reg));
}


// Private methods:

void RegExpMacroAssemblerMIPS::CallCheckStackGuardState(Register scratch) {
  static const int num_arguments = 3;
  __ PrepareCallCFunction(num_arguments, scratch);
  __ mov(a2, frame_pointer());
  // Code* of self.
  __ li(a1, Operand(masm_->CodeObject()));
  // a0 becomes return address pointer.
  ExternalReference stack_guard_check =
      ExternalReference::re_check_stack_guard_state(masm_->isolate());
  CallCFunctionUsingStub(stack_guard_check, num_arguments);
}


// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  return reinterpret_cast<T&>(Memory::int32_at(re_frame + frame_offset));
}


int RegExpMacroAssemblerMIPS::CheckStackGuardState(Address* return_address,
                                                   Code* re_code,
                                                   Address re_frame) {
  Isolate* isolate = frame_entry<Isolate*>(re_frame, kIsolate);
  ASSERT(isolate == Isolate::Current());
  if (isolate->stack_guard()->IsStackOverflow()) {
    isolate->StackOverflow();
    return EXCEPTION;
  }

  // If not real stack overflow the stack guard was used to interrupt
  // execution for another purpose.

  // If this is a direct call from JavaScript retry the RegExp forcing the call
  // through the runtime system. Currently the direct call cannot handle a GC.
  if (frame_entry<int>(re_frame, kDirectCall) == 1) {
    return RETRY;
  }

  // Prepare for possible GC.
  HandleScope handles(isolate);
  Handle<Code> code_handle(re_code);

  Handle<String> subject(frame_entry<String*>(re_frame, kInputString));
  // Current string.
  bool is_ascii = subject->IsAsciiRepresentationUnderneath();

  ASSERT(re_code->instruction_start() <= *return_address);
  ASSERT(*return_address <=
      re_code->instruction_start() + re_code->instruction_size());

  MaybeObject* result = Execution::HandleStackGuardInterrupt();

  if (*code_handle != re_code) {  // Return address no longer valid.
    int delta = code_handle->address() - re_code->address();
    // Overwrite the return address on the stack.
    *return_address += delta;
  }

  if (result->IsException()) {
    return EXCEPTION;
  }

  Handle<String> subject_tmp = subject;
  int slice_offset = 0;

  // Extract the underlying string and the slice offset.
  if (StringShape(*subject_tmp).IsCons()) {
    subject_tmp = Handle<String>(ConsString::cast(*subject_tmp)->first());
  } else if (StringShape(*subject_tmp).IsSliced()) {
    SlicedString* slice = SlicedString::cast(*subject_tmp);
    subject_tmp = Handle<String>(slice->parent());
    slice_offset = slice->offset();
  }

  // String might have changed.
  if (subject_tmp->IsAsciiRepresentation() != is_ascii) {
    // If we changed between an ASCII and an UC16 string, the specialized
    // code cannot be used, and we need to restart regexp matching from
    // scratch (including, potentially, compiling a new version of the code).
    return RETRY;
  }

  // Otherwise, the content of the string might have moved. It must still
  // be a sequential or external string with the same content.
  // Update the start and end pointers in the stack frame to the current
  // location (whether it has actually moved or not).
  ASSERT(StringShape(*subject_tmp).IsSequential() ||
      StringShape(*subject_tmp).IsExternal());

  // The original start address of the characters to match.
  const byte* start_address = frame_entry<const byte*>(re_frame, kInputStart);

  // Find the current start address of the same character at the current string
  // position.
  int start_index = frame_entry<int>(re_frame, kStartIndex);
  const byte* new_address = StringCharacterPosition(*subject_tmp,
                                                    start_index + slice_offset);

  if (start_address != new_address) {
    // If there is a difference, update the object pointer and start and end
    // addresses in the RegExp stack frame to match the new value.
    const byte* end_address = frame_entry<const byte* >(re_frame, kInputEnd);
    int byte_length = static_cast<int>(end_address - start_address);
    frame_entry<const String*>(re_frame, kInputString) = *subject;
    frame_entry<const byte*>(re_frame, kInputStart) = new_address;
    frame_entry<const byte*>(re_frame, kInputEnd) = new_address + byte_length;
  }

  return 0;
}


MemOperand RegExpMacroAssemblerMIPS::register_location(int register_index) {
  ASSERT(register_index < (1<<30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return MemOperand(frame_pointer(),
                    kRegisterZero - register_index * kPointerSize);
}


void RegExpMacroAssemblerMIPS::CheckPosition(int cp_offset,
                                            Label* on_outside_input) {
  BranchOrBacktrack(on_outside_input,
                    ge,
                    current_input_offset(),
                    Operand(-cp_offset * char_size()));
}


void RegExpMacroAssemblerMIPS::BranchOrBacktrack(Label* to,
                                                 Condition condition,
                                                 Register rs,
                                                 const Operand& rt) {
  if (condition == al) {  // Unconditional.
    if (to == NULL) {
      Backtrack();
      return;
    }
    __ jmp(to);
    return;
  }
  if (to == NULL) {
    __ Branch(&backtrack_label_, condition, rs, rt);
    return;
  }
  __ Branch(to, condition, rs, rt);
}


void RegExpMacroAssemblerMIPS::SafeCall(Label* to, Condition cond, Register rs,
                                           const Operand& rt) {
  __ BranchAndLink(to, cond, rs, rt);
}


void RegExpMacroAssemblerMIPS::SafeReturn() {
  __ pop(ra);
  __ Addu(t5, ra, Operand(masm_->CodeObject()));
  __ Jump(t5);
}


void RegExpMacroAssemblerMIPS::SafeCallTarget(Label* name) {
  __ bind(name);
  __ Subu(ra, ra, Operand(masm_->CodeObject()));
  __ push(ra);
}


void RegExpMacroAssemblerMIPS::Push(Register source) {
  ASSERT(!source.is(backtrack_stackpointer()));
  __ Addu(backtrack_stackpointer(),
          backtrack_stackpointer(),
          Operand(-kPointerSize));
  __ sw(source, MemOperand(backtrack_stackpointer()));
}


void RegExpMacroAssemblerMIPS::Pop(Register target) {
  ASSERT(!target.is(backtrack_stackpointer()));
  __ lw(target, MemOperand(backtrack_stackpointer()));
  __ Addu(backtrack_stackpointer(), backtrack_stackpointer(), kPointerSize);
}


void RegExpMacroAssemblerMIPS::CheckPreemption() {
  // Check for preemption.
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(masm_->isolate());
  __ li(a0, Operand(stack_limit));
  __ lw(a0, MemOperand(a0));
  SafeCall(&check_preempt_label_, ls, sp, Operand(a0));
}


void RegExpMacroAssemblerMIPS::CheckStackLimit() {
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit(masm_->isolate());

  __ li(a0, Operand(stack_limit));
  __ lw(a0, MemOperand(a0));
  SafeCall(&stack_overflow_label_, ls, backtrack_stackpointer(), Operand(a0));
}


void RegExpMacroAssemblerMIPS::CallCFunctionUsingStub(
    ExternalReference function,
    int num_arguments) {
  // Must pass all arguments in registers. The stub pushes on the stack.
  ASSERT(num_arguments <= 4);
  __ li(code_pointer(), Operand(function));
  RegExpCEntryStub stub;
  __ CallStub(&stub);
  if (OS::ActivationFrameAlignment() != 0) {
    __ lw(sp, MemOperand(sp, 16));
  }
  __ li(code_pointer(), Operand(masm_->CodeObject()));
}


void RegExpMacroAssemblerMIPS::LoadCurrentCharacterUnchecked(int cp_offset,
                                                            int characters) {
  Register offset = current_input_offset();
  if (cp_offset != 0) {
    __ Addu(a0, current_input_offset(), Operand(cp_offset * char_size()));
    offset = a0;
  }
  // We assume that we cannot do unaligned loads on MIPS, so this function
  // must only be used to load a single character at a time.
  ASSERT(characters == 1);
  __ Addu(t5, end_of_input_address(), Operand(offset));
  if (mode_ == ASCII) {
    __ lbu(current_character(), MemOperand(t5, 0));
  } else {
    ASSERT(mode_ == UC16);
    __ lhu(current_character(), MemOperand(t5, 0));
  }
}


void RegExpCEntryStub::Generate(MacroAssembler* masm_) {
  int stack_alignment = OS::ActivationFrameAlignment();
  if (stack_alignment < kPointerSize) stack_alignment = kPointerSize;
  // Stack is already aligned for call, so decrement by alignment
  // to make room for storing the return address.
  __ Subu(sp, sp, Operand(stack_alignment));
  __ sw(ra, MemOperand(sp, 0));
  __ mov(a0, sp);
  __ mov(t9, t1);
  __ Call(t9);
  __ lw(ra, MemOperand(sp, 0));
  __ Addu(sp, sp, Operand(stack_alignment));
  __ Jump(ra);
}


#undef __

#endif  // V8_INTERPRETED_REGEXP

}}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS

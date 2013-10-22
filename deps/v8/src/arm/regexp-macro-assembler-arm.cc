// Copyright 2012 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_ARM

#include "cpu-profiler.h"
#include "unicode.h"
#include "log.h"
#include "code-stubs.h"
#include "regexp-stack.h"
#include "macro-assembler.h"
#include "regexp-macro-assembler.h"
#include "arm/regexp-macro-assembler-arm.h"

namespace v8 {
namespace internal {

#ifndef V8_INTERPRETED_REGEXP
/*
 * This assembler uses the following register assignment convention
 * - r4 : Temporarily stores the index of capture start after a matching pass
 *        for a global regexp.
 * - r5 : Pointer to current code object (Code*) including heap object tag.
 * - r6 : Current position in input, as negative offset from end of string.
 *        Please notice that this is the byte offset, not the character offset!
 * - r7 : Currently loaded character. Must be loaded using
 *        LoadCurrentCharacter before using any of the dispatch methods.
 * - r8 : Points to tip of backtrack stack
 * - r9 : Unused, might be used by C code and expected unchanged.
 * - r10 : End of input (points to byte after last character in input).
 * - r11 : Frame pointer. Used to access arguments, local variables and
 *         RegExp registers.
 * - r12 : IP register, used by assembler. Very volatile.
 * - r13/sp : Points to tip of C stack.
 *
 * The remaining registers are free for computations.
 * Each call to a public method should retain this convention.
 *
 * The stack will have the following structure:
 *  - fp[56]  Isolate* isolate   (address of the current isolate)
 *  - fp[52]  direct_call        (if 1, direct call from JavaScript code,
 *                                if 0, call through the runtime system).
 *  - fp[48]  stack_area_base    (high end of the memory area to use as
 *                                backtracking stack).
 *  - fp[44]  capture array size (may fit multiple sets of matches)
 *  - fp[40]  int* capture_array (int[num_saved_registers_], for output).
 *  - fp[36]  secondary link/return address used by native call.
 *  --- sp when called ---
 *  - fp[32]  return address     (lr).
 *  - fp[28]  old frame pointer  (r11).
 *  - fp[0..24]  backup of registers r4..r10.
 *  --- frame pointer ----
 *  - fp[-4]  end of input       (address of end of string).
 *  - fp[-8]  start of input     (address of first character in string).
 *  - fp[-12] start index        (character index of start).
 *  - fp[-16] void* input_string (location of a handle containing the string).
 *  - fp[-20] success counter    (only for global regexps to count matches).
 *  - fp[-24] Offset of location before start of input (effectively character
 *            position -1). Used to initialize capture registers to a
 *            non-position.
 *  - fp[-28] At start (if 1, we are starting at the start of the
 *    string, otherwise 0)
 *  - fp[-32] register 0         (Only positions must be stored in the first
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
 * in arm/simulator-arm.h.
 * When calling as a non-direct call (i.e., from C++ code), the return address
 * area is overwritten with the LR register by the RegExp code. When doing a
 * direct call from generated code, the return address is placed there by
 * the calling code, as in a normal exit frame.
 */

#define __ ACCESS_MASM(masm_)

RegExpMacroAssemblerARM::RegExpMacroAssemblerARM(
    Mode mode,
    int registers_to_save,
    Zone* zone)
    : NativeRegExpMacroAssembler(zone),
      masm_(new MacroAssembler(zone->isolate(), NULL, kRegExpCodeSize)),
      mode_(mode),
      num_registers_(registers_to_save),
      num_saved_registers_(registers_to_save),
      entry_label_(),
      start_label_(),
      success_label_(),
      backtrack_label_(),
      exit_label_() {
  ASSERT_EQ(0, registers_to_save % 2);
  __ jmp(&entry_label_);   // We'll write the entry code later.
  __ bind(&start_label_);  // And then continue from here.
}


RegExpMacroAssemblerARM::~RegExpMacroAssemblerARM() {
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


int RegExpMacroAssemblerARM::stack_limit_slack()  {
  return RegExpStack::kStackLimitSlack;
}


void RegExpMacroAssemblerARM::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    __ add(current_input_offset(),
           current_input_offset(), Operand(by * char_size()));
  }
}


void RegExpMacroAssemblerARM::AdvanceRegister(int reg, int by) {
  ASSERT(reg >= 0);
  ASSERT(reg < num_registers_);
  if (by != 0) {
    __ ldr(r0, register_location(reg));
    __ add(r0, r0, Operand(by));
    __ str(r0, register_location(reg));
  }
}


void RegExpMacroAssemblerARM::Backtrack() {
  CheckPreemption();
  // Pop Code* offset from backtrack stack, add Code* and jump to location.
  Pop(r0);
  __ add(pc, r0, Operand(code_pointer()));
}


void RegExpMacroAssemblerARM::Bind(Label* label) {
  __ bind(label);
}


void RegExpMacroAssemblerARM::CheckCharacter(uint32_t c, Label* on_equal) {
  __ cmp(current_character(), Operand(c));
  BranchOrBacktrack(eq, on_equal);
}


void RegExpMacroAssemblerARM::CheckCharacterGT(uc16 limit, Label* on_greater) {
  __ cmp(current_character(), Operand(limit));
  BranchOrBacktrack(gt, on_greater);
}


void RegExpMacroAssemblerARM::CheckAtStart(Label* on_at_start) {
  Label not_at_start;
  // Did we start the match at the start of the string at all?
  __ ldr(r0, MemOperand(frame_pointer(), kStartIndex));
  __ cmp(r0, Operand::Zero());
  BranchOrBacktrack(ne, &not_at_start);

  // If we did, are we still at the start of the input?
  __ ldr(r1, MemOperand(frame_pointer(), kInputStart));
  __ add(r0, end_of_input_address(), Operand(current_input_offset()));
  __ cmp(r0, r1);
  BranchOrBacktrack(eq, on_at_start);
  __ bind(&not_at_start);
}


void RegExpMacroAssemblerARM::CheckNotAtStart(Label* on_not_at_start) {
  // Did we start the match at the start of the string at all?
  __ ldr(r0, MemOperand(frame_pointer(), kStartIndex));
  __ cmp(r0, Operand::Zero());
  BranchOrBacktrack(ne, on_not_at_start);
  // If we did, are we still at the start of the input?
  __ ldr(r1, MemOperand(frame_pointer(), kInputStart));
  __ add(r0, end_of_input_address(), Operand(current_input_offset()));
  __ cmp(r0, r1);
  BranchOrBacktrack(ne, on_not_at_start);
}


void RegExpMacroAssemblerARM::CheckCharacterLT(uc16 limit, Label* on_less) {
  __ cmp(current_character(), Operand(limit));
  BranchOrBacktrack(lt, on_less);
}


void RegExpMacroAssemblerARM::CheckGreedyLoop(Label* on_equal) {
  __ ldr(r0, MemOperand(backtrack_stackpointer(), 0));
  __ cmp(current_input_offset(), r0);
  __ add(backtrack_stackpointer(),
         backtrack_stackpointer(), Operand(kPointerSize), LeaveCC, eq);
  BranchOrBacktrack(eq, on_equal);
}


void RegExpMacroAssemblerARM::CheckNotBackReferenceIgnoreCase(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  __ ldr(r0, register_location(start_reg));  // Index of start of capture
  __ ldr(r1, register_location(start_reg + 1));  // Index of end of capture
  __ sub(r1, r1, r0, SetCC);  // Length of capture.

  // If length is zero, either the capture is empty or it is not participating.
  // In either case succeed immediately.
  __ b(eq, &fallthrough);

  // Check that there are enough characters left in the input.
  __ cmn(r1, Operand(current_input_offset()));
  BranchOrBacktrack(gt, on_no_match);

  if (mode_ == ASCII) {
    Label success;
    Label fail;
    Label loop_check;

    // r0 - offset of start of capture
    // r1 - length of capture
    __ add(r0, r0, Operand(end_of_input_address()));
    __ add(r2, end_of_input_address(), Operand(current_input_offset()));
    __ add(r1, r0, Operand(r1));

    // r0 - Address of start of capture.
    // r1 - Address of end of capture
    // r2 - Address of current input position.

    Label loop;
    __ bind(&loop);
    __ ldrb(r3, MemOperand(r0, char_size(), PostIndex));
    __ ldrb(r4, MemOperand(r2, char_size(), PostIndex));
    __ cmp(r4, r3);
    __ b(eq, &loop_check);

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ orr(r3, r3, Operand(0x20));  // Convert capture character to lower-case.
    __ orr(r4, r4, Operand(0x20));  // Also convert input character.
    __ cmp(r4, r3);
    __ b(ne, &fail);
    __ sub(r3, r3, Operand('a'));
    __ cmp(r3, Operand('z' - 'a'));  // Is r3 a lowercase letter?
    __ b(ls, &loop_check);  // In range 'a'-'z'.
    // Latin-1: Check for values in range [224,254] but not 247.
    __ sub(r3, r3, Operand(224 - 'a'));
    __ cmp(r3, Operand(254 - 224));
    __ b(hi, &fail);  // Weren't Latin-1 letters.
    __ cmp(r3, Operand(247 - 224));  // Check for 247.
    __ b(eq, &fail);

    __ bind(&loop_check);
    __ cmp(r0, r1);
    __ b(lt, &loop);
    __ jmp(&success);

    __ bind(&fail);
    BranchOrBacktrack(al, on_no_match);

    __ bind(&success);
    // Compute new value of character position after the matched part.
    __ sub(current_input_offset(), r2, end_of_input_address());
  } else {
    ASSERT(mode_ == UC16);
    int argument_count = 4;
    __ PrepareCallCFunction(argument_count, r2);

    // r0 - offset of start of capture
    // r1 - length of capture

    // Put arguments into arguments registers.
    // Parameters are
    //   r0: Address byte_offset1 - Address captured substring's start.
    //   r1: Address byte_offset2 - Address of current character position.
    //   r2: size_t byte_length - length of capture in bytes(!)
    //   r3: Isolate* isolate

    // Address of start of capture.
    __ add(r0, r0, Operand(end_of_input_address()));
    // Length of capture.
    __ mov(r2, Operand(r1));
    // Save length in callee-save register for use on return.
    __ mov(r4, Operand(r1));
    // Address of current input position.
    __ add(r1, current_input_offset(), Operand(end_of_input_address()));
    // Isolate.
    __ mov(r3, Operand(ExternalReference::isolate_address(isolate())));

    {
      AllowExternalCallThatCantCauseGC scope(masm_);
      ExternalReference function =
          ExternalReference::re_case_insensitive_compare_uc16(isolate());
      __ CallCFunction(function, argument_count);
    }

    // Check if function returned non-zero for success or zero for failure.
    __ cmp(r0, Operand::Zero());
    BranchOrBacktrack(eq, on_no_match);
    // On success, increment position by length of capture.
    __ add(current_input_offset(), current_input_offset(), Operand(r4));
  }

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerARM::CheckNotBackReference(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  Label success;

  // Find length of back-referenced capture.
  __ ldr(r0, register_location(start_reg));
  __ ldr(r1, register_location(start_reg + 1));
  __ sub(r1, r1, r0, SetCC);  // Length to check.
  // Succeed on empty capture (including no capture).
  __ b(eq, &fallthrough);

  // Check that there are enough characters left in the input.
  __ cmn(r1, Operand(current_input_offset()));
  BranchOrBacktrack(gt, on_no_match);

  // Compute pointers to match string and capture string
  __ add(r0, r0, Operand(end_of_input_address()));
  __ add(r2, end_of_input_address(), Operand(current_input_offset()));
  __ add(r1, r1, Operand(r0));

  Label loop;
  __ bind(&loop);
  if (mode_ == ASCII) {
    __ ldrb(r3, MemOperand(r0, char_size(), PostIndex));
    __ ldrb(r4, MemOperand(r2, char_size(), PostIndex));
  } else {
    ASSERT(mode_ == UC16);
    __ ldrh(r3, MemOperand(r0, char_size(), PostIndex));
    __ ldrh(r4, MemOperand(r2, char_size(), PostIndex));
  }
  __ cmp(r3, r4);
  BranchOrBacktrack(ne, on_no_match);
  __ cmp(r0, r1);
  __ b(lt, &loop);

  // Move current character position to position after match.
  __ sub(current_input_offset(), r2, end_of_input_address());
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerARM::CheckNotCharacter(unsigned c,
                                                Label* on_not_equal) {
  __ cmp(current_character(), Operand(c));
  BranchOrBacktrack(ne, on_not_equal);
}


void RegExpMacroAssemblerARM::CheckCharacterAfterAnd(uint32_t c,
                                                     uint32_t mask,
                                                     Label* on_equal) {
  if (c == 0) {
    __ tst(current_character(), Operand(mask));
  } else {
    __ and_(r0, current_character(), Operand(mask));
    __ cmp(r0, Operand(c));
  }
  BranchOrBacktrack(eq, on_equal);
}


void RegExpMacroAssemblerARM::CheckNotCharacterAfterAnd(unsigned c,
                                                        unsigned mask,
                                                        Label* on_not_equal) {
  if (c == 0) {
    __ tst(current_character(), Operand(mask));
  } else {
    __ and_(r0, current_character(), Operand(mask));
    __ cmp(r0, Operand(c));
  }
  BranchOrBacktrack(ne, on_not_equal);
}


void RegExpMacroAssemblerARM::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  ASSERT(minus < String::kMaxUtf16CodeUnit);
  __ sub(r0, current_character(), Operand(minus));
  __ and_(r0, r0, Operand(mask));
  __ cmp(r0, Operand(c));
  BranchOrBacktrack(ne, on_not_equal);
}


void RegExpMacroAssemblerARM::CheckCharacterInRange(
    uc16 from,
    uc16 to,
    Label* on_in_range) {
  __ sub(r0, current_character(), Operand(from));
  __ cmp(r0, Operand(to - from));
  BranchOrBacktrack(ls, on_in_range);  // Unsigned lower-or-same condition.
}


void RegExpMacroAssemblerARM::CheckCharacterNotInRange(
    uc16 from,
    uc16 to,
    Label* on_not_in_range) {
  __ sub(r0, current_character(), Operand(from));
  __ cmp(r0, Operand(to - from));
  BranchOrBacktrack(hi, on_not_in_range);  // Unsigned higher condition.
}


void RegExpMacroAssemblerARM::CheckBitInTable(
    Handle<ByteArray> table,
    Label* on_bit_set) {
  __ mov(r0, Operand(table));
  if (mode_ != ASCII || kTableMask != String::kMaxOneByteCharCode) {
    __ and_(r1, current_character(), Operand(kTableSize - 1));
    __ add(r1, r1, Operand(ByteArray::kHeaderSize - kHeapObjectTag));
  } else {
    __ add(r1,
           current_character(),
           Operand(ByteArray::kHeaderSize - kHeapObjectTag));
  }
  __ ldrb(r0, MemOperand(r0, r1));
  __ cmp(r0, Operand::Zero());
  BranchOrBacktrack(ne, on_bit_set);
}


bool RegExpMacroAssemblerARM::CheckSpecialCharacterClass(uc16 type,
                                                         Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check
  switch (type) {
  case 's':
    // Match space-characters
    if (mode_ == ASCII) {
      // One byte space characters are '\t'..'\r', ' ' and \u00a0.
      Label success;
      __ cmp(current_character(), Operand(' '));
      __ b(eq, &success);
      // Check range 0x09..0x0d
      __ sub(r0, current_character(), Operand('\t'));
      __ cmp(r0, Operand('\r' - '\t'));
      __ b(ls, &success);
      // \u00a0 (NBSP).
      __ cmp(r0, Operand(0x00a0 - '\t'));
      BranchOrBacktrack(ne, on_no_match);
      __ bind(&success);
      return true;
    }
    return false;
  case 'S':
    // The emitted code for generic character classes is good enough.
    return false;
  case 'd':
    // Match ASCII digits ('0'..'9')
    __ sub(r0, current_character(), Operand('0'));
    __ cmp(r0, Operand('9' - '0'));
    BranchOrBacktrack(hi, on_no_match);
    return true;
  case 'D':
    // Match non ASCII-digits
    __ sub(r0, current_character(), Operand('0'));
    __ cmp(r0, Operand('9' - '0'));
    BranchOrBacktrack(ls, on_no_match);
    return true;
  case '.': {
    // Match non-newlines (not 0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029)
    __ eor(r0, current_character(), Operand(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ sub(r0, r0, Operand(0x0b));
    __ cmp(r0, Operand(0x0c - 0x0b));
    BranchOrBacktrack(ls, on_no_match);
    if (mode_ == UC16) {
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ sub(r0, r0, Operand(0x2028 - 0x0b));
      __ cmp(r0, Operand(1));
      BranchOrBacktrack(ls, on_no_match);
    }
    return true;
  }
  case 'n': {
    // Match newlines (0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029)
    __ eor(r0, current_character(), Operand(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ sub(r0, r0, Operand(0x0b));
    __ cmp(r0, Operand(0x0c - 0x0b));
    if (mode_ == ASCII) {
      BranchOrBacktrack(hi, on_no_match);
    } else {
      Label done;
      __ b(ls, &done);
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ sub(r0, r0, Operand(0x2028 - 0x0b));
      __ cmp(r0, Operand(1));
      BranchOrBacktrack(hi, on_no_match);
      __ bind(&done);
    }
    return true;
  }
  case 'w': {
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      __ cmp(current_character(), Operand('z'));
      BranchOrBacktrack(hi, on_no_match);
    }
    ExternalReference map = ExternalReference::re_word_character_map();
    __ mov(r0, Operand(map));
    __ ldrb(r0, MemOperand(r0, current_character()));
    __ cmp(r0, Operand::Zero());
    BranchOrBacktrack(eq, on_no_match);
    return true;
  }
  case 'W': {
    Label done;
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      __ cmp(current_character(), Operand('z'));
      __ b(hi, &done);
    }
    ExternalReference map = ExternalReference::re_word_character_map();
    __ mov(r0, Operand(map));
    __ ldrb(r0, MemOperand(r0, current_character()));
    __ cmp(r0, Operand::Zero());
    BranchOrBacktrack(ne, on_no_match);
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


void RegExpMacroAssemblerARM::Fail() {
  __ mov(r0, Operand(FAILURE));
  __ jmp(&exit_label_);
}


Handle<HeapObject> RegExpMacroAssemblerARM::GetCode(Handle<String> source) {
  Label return_r0;
  // Finalize code - write the entry point code now we know how many
  // registers we need.

  // Entry code:
  __ bind(&entry_label_);

  // Tell the system that we have a stack frame.  Because the type is MANUAL, no
  // is generated.
  FrameScope scope(masm_, StackFrame::MANUAL);

  // Actually emit code to start a new stack frame.
  // Push arguments
  // Save callee-save registers.
  // Start new stack frame.
  // Store link register in existing stack-cell.
  // Order here should correspond to order of offset constants in header file.
  RegList registers_to_retain = r4.bit() | r5.bit() | r6.bit() |
      r7.bit() | r8.bit() | r9.bit() | r10.bit() | fp.bit();
  RegList argument_registers = r0.bit() | r1.bit() | r2.bit() | r3.bit();
  __ stm(db_w, sp, argument_registers | registers_to_retain | lr.bit());
  // Set frame pointer in space for it if this is not a direct call
  // from generated code.
  __ add(frame_pointer(), sp, Operand(4 * kPointerSize));
  __ mov(r0, Operand::Zero());
  __ push(r0);  // Make room for success counter and initialize it to 0.
  __ push(r0);  // Make room for "position - 1" constant (value is irrelevant).
  // Check if we have space on the stack for registers.
  Label stack_limit_hit;
  Label stack_ok;

  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ mov(r0, Operand(stack_limit));
  __ ldr(r0, MemOperand(r0));
  __ sub(r0, sp, r0, SetCC);
  // Handle it if the stack pointer is already below the stack limit.
  __ b(ls, &stack_limit_hit);
  // Check if there is room for the variable number of registers above
  // the stack limit.
  __ cmp(r0, Operand(num_registers_ * kPointerSize));
  __ b(hs, &stack_ok);
  // Exit with OutOfMemory exception. There is not enough space on the stack
  // for our working registers.
  __ mov(r0, Operand(EXCEPTION));
  __ jmp(&return_r0);

  __ bind(&stack_limit_hit);
  CallCheckStackGuardState(r0);
  __ cmp(r0, Operand::Zero());
  // If returned value is non-zero, we exit with the returned value as result.
  __ b(ne, &return_r0);

  __ bind(&stack_ok);

  // Allocate space on stack for registers.
  __ sub(sp, sp, Operand(num_registers_ * kPointerSize));
  // Load string end.
  __ ldr(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
  // Load input start.
  __ ldr(r0, MemOperand(frame_pointer(), kInputStart));
  // Find negative length (offset of start relative to end).
  __ sub(current_input_offset(), r0, end_of_input_address());
  // Set r0 to address of char before start of the input string
  // (effectively string position -1).
  __ ldr(r1, MemOperand(frame_pointer(), kStartIndex));
  __ sub(r0, current_input_offset(), Operand(char_size()));
  __ sub(r0, r0, Operand(r1, LSL, (mode_ == UC16) ? 1 : 0));
  // Store this value in a local variable, for use when clearing
  // position registers.
  __ str(r0, MemOperand(frame_pointer(), kInputStartMinusOne));

  // Initialize code pointer register
  __ mov(code_pointer(), Operand(masm_->CodeObject()));

  Label load_char_start_regexp, start_regexp;
  // Load newline if index is at start, previous character otherwise.
  __ cmp(r1, Operand::Zero());
  __ b(ne, &load_char_start_regexp);
  __ mov(current_character(), Operand('\n'), LeaveCC, eq);
  __ jmp(&start_regexp);

  // Global regexp restarts matching here.
  __ bind(&load_char_start_regexp);
  // Load previous char as initial value of current character register.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&start_regexp);

  // Initialize on-stack registers.
  if (num_saved_registers_ > 0) {  // Always is, if generated from a regexp.
    // Fill saved registers with initial value = start offset - 1
    if (num_saved_registers_ > 8) {
      // Address of register 0.
      __ add(r1, frame_pointer(), Operand(kRegisterZero));
      __ mov(r2, Operand(num_saved_registers_));
      Label init_loop;
      __ bind(&init_loop);
      __ str(r0, MemOperand(r1, kPointerSize, NegPostIndex));
      __ sub(r2, r2, Operand(1), SetCC);
      __ b(ne, &init_loop);
    } else {
      for (int i = 0; i < num_saved_registers_; i++) {
        __ str(r0, register_location(i));
      }
    }
  }

  // Initialize backtrack stack pointer.
  __ ldr(backtrack_stackpointer(), MemOperand(frame_pointer(), kStackHighEnd));

  __ jmp(&start_label_);

  // Exit code:
  if (success_label_.is_linked()) {
    // Save captures when successful.
    __ bind(&success_label_);
    if (num_saved_registers_ > 0) {
      // copy captures to output
      __ ldr(r1, MemOperand(frame_pointer(), kInputStart));
      __ ldr(r0, MemOperand(frame_pointer(), kRegisterOutput));
      __ ldr(r2, MemOperand(frame_pointer(), kStartIndex));
      __ sub(r1, end_of_input_address(), r1);
      // r1 is length of input in bytes.
      if (mode_ == UC16) {
        __ mov(r1, Operand(r1, LSR, 1));
      }
      // r1 is length of input in characters.
      __ add(r1, r1, Operand(r2));
      // r1 is length of string in characters.

      ASSERT_EQ(0, num_saved_registers_ % 2);
      // Always an even number of capture registers. This allows us to
      // unroll the loop once to add an operation between a load of a register
      // and the following use of that register.
      for (int i = 0; i < num_saved_registers_; i += 2) {
        __ ldr(r2, register_location(i));
        __ ldr(r3, register_location(i + 1));
        if (i == 0 && global_with_zero_length_check()) {
          // Keep capture start in r4 for the zero-length check later.
          __ mov(r4, r2);
        }
        if (mode_ == UC16) {
          __ add(r2, r1, Operand(r2, ASR, 1));
          __ add(r3, r1, Operand(r3, ASR, 1));
        } else {
          __ add(r2, r1, Operand(r2));
          __ add(r3, r1, Operand(r3));
        }
        __ str(r2, MemOperand(r0, kPointerSize, PostIndex));
        __ str(r3, MemOperand(r0, kPointerSize, PostIndex));
      }
    }

    if (global()) {
      // Restart matching if the regular expression is flagged as global.
      __ ldr(r0, MemOperand(frame_pointer(), kSuccessfulCaptures));
      __ ldr(r1, MemOperand(frame_pointer(), kNumOutputRegisters));
      __ ldr(r2, MemOperand(frame_pointer(), kRegisterOutput));
      // Increment success counter.
      __ add(r0, r0, Operand(1));
      __ str(r0, MemOperand(frame_pointer(), kSuccessfulCaptures));
      // Capture results have been stored, so the number of remaining global
      // output registers is reduced by the number of stored captures.
      __ sub(r1, r1, Operand(num_saved_registers_));
      // Check whether we have enough room for another set of capture results.
      __ cmp(r1, Operand(num_saved_registers_));
      __ b(lt, &return_r0);

      __ str(r1, MemOperand(frame_pointer(), kNumOutputRegisters));
      // Advance the location for output.
      __ add(r2, r2, Operand(num_saved_registers_ * kPointerSize));
      __ str(r2, MemOperand(frame_pointer(), kRegisterOutput));

      // Prepare r0 to initialize registers with its value in the next run.
      __ ldr(r0, MemOperand(frame_pointer(), kInputStartMinusOne));

      if (global_with_zero_length_check()) {
        // Special case for zero-length matches.
        // r4: capture start index
        __ cmp(current_input_offset(), r4);
        // Not a zero-length match, restart.
        __ b(ne, &load_char_start_regexp);
        // Offset from the end is zero if we already reached the end.
        __ cmp(current_input_offset(), Operand::Zero());
        __ b(eq, &exit_label_);
        // Advance current position after a zero-length match.
        __ add(current_input_offset(),
               current_input_offset(),
               Operand((mode_ == UC16) ? 2 : 1));
      }

      __ b(&load_char_start_regexp);
    } else {
      __ mov(r0, Operand(SUCCESS));
    }
  }

  // Exit and return r0
  __ bind(&exit_label_);
  if (global()) {
    __ ldr(r0, MemOperand(frame_pointer(), kSuccessfulCaptures));
  }

  __ bind(&return_r0);
  // Skip sp past regexp registers and local variables..
  __ mov(sp, frame_pointer());
  // Restore registers r4..r11 and return (restoring lr to pc).
  __ ldm(ia_w, sp, registers_to_retain | pc.bit());

  // Backtrack code (branch target for conditional backtracks).
  if (backtrack_label_.is_linked()) {
    __ bind(&backtrack_label_);
    Backtrack();
  }

  Label exit_with_exception;

  // Preempt-code
  if (check_preempt_label_.is_linked()) {
    SafeCallTarget(&check_preempt_label_);

    CallCheckStackGuardState(r0);
    __ cmp(r0, Operand::Zero());
    // If returning non-zero, we should end execution with the given
    // result as return value.
    __ b(ne, &return_r0);

    // String might have moved: Reload end of string from frame.
    __ ldr(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
    SafeReturn();
  }

  // Backtrack stack overflow code.
  if (stack_overflow_label_.is_linked()) {
    SafeCallTarget(&stack_overflow_label_);
    // Reached if the backtrack-stack limit has been hit.
    Label grow_failed;

    // Call GrowStack(backtrack_stackpointer(), &stack_base)
    static const int num_arguments = 3;
    __ PrepareCallCFunction(num_arguments, r0);
    __ mov(r0, backtrack_stackpointer());
    __ add(r1, frame_pointer(), Operand(kStackHighEnd));
    __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));
    ExternalReference grow_stack =
        ExternalReference::re_grow_stack(isolate());
    __ CallCFunction(grow_stack, num_arguments);
    // If return NULL, we have failed to grow the stack, and
    // must exit with a stack-overflow exception.
    __ cmp(r0, Operand::Zero());
    __ b(eq, &exit_with_exception);
    // Otherwise use return value as new stack pointer.
    __ mov(backtrack_stackpointer(), r0);
    // Restore saved registers and continue.
    SafeReturn();
  }

  if (exit_with_exception.is_linked()) {
    // If any of the code above needed to exit with an exception.
    __ bind(&exit_with_exception);
    // Exit with Result EXCEPTION(-1) to signal thrown exception.
    __ mov(r0, Operand(EXCEPTION));
    __ jmp(&return_r0);
  }

  CodeDesc code_desc;
  masm_->GetCode(&code_desc);
  Handle<Code> code = isolate()->factory()->NewCode(
      code_desc, Code::ComputeFlags(Code::REGEXP), masm_->CodeObject());
  PROFILE(masm_->isolate(), RegExpCodeCreateEvent(*code, *source));
  return Handle<HeapObject>::cast(code);
}


void RegExpMacroAssemblerARM::GoTo(Label* to) {
  BranchOrBacktrack(al, to);
}


void RegExpMacroAssemblerARM::IfRegisterGE(int reg,
                                           int comparand,
                                           Label* if_ge) {
  __ ldr(r0, register_location(reg));
  __ cmp(r0, Operand(comparand));
  BranchOrBacktrack(ge, if_ge);
}


void RegExpMacroAssemblerARM::IfRegisterLT(int reg,
                                           int comparand,
                                           Label* if_lt) {
  __ ldr(r0, register_location(reg));
  __ cmp(r0, Operand(comparand));
  BranchOrBacktrack(lt, if_lt);
}


void RegExpMacroAssemblerARM::IfRegisterEqPos(int reg,
                                              Label* if_eq) {
  __ ldr(r0, register_location(reg));
  __ cmp(r0, Operand(current_input_offset()));
  BranchOrBacktrack(eq, if_eq);
}


RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerARM::Implementation() {
  return kARMImplementation;
}


void RegExpMacroAssemblerARM::LoadCurrentCharacter(int cp_offset,
                                                   Label* on_end_of_input,
                                                   bool check_bounds,
                                                   int characters) {
  ASSERT(cp_offset >= -1);      // ^ and \b can look behind one character.
  ASSERT(cp_offset < (1<<30));  // Be sane! (And ensure negation works)
  if (check_bounds) {
    CheckPosition(cp_offset + characters - 1, on_end_of_input);
  }
  LoadCurrentCharacterUnchecked(cp_offset, characters);
}


void RegExpMacroAssemblerARM::PopCurrentPosition() {
  Pop(current_input_offset());
}


void RegExpMacroAssemblerARM::PopRegister(int register_index) {
  Pop(r0);
  __ str(r0, register_location(register_index));
}


void RegExpMacroAssemblerARM::PushBacktrack(Label* label) {
  __ mov_label_offset(r0, label);
  Push(r0);
  CheckStackLimit();
}


void RegExpMacroAssemblerARM::PushCurrentPosition() {
  Push(current_input_offset());
}


void RegExpMacroAssemblerARM::PushRegister(int register_index,
                                           StackCheckFlag check_stack_limit) {
  __ ldr(r0, register_location(register_index));
  Push(r0);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerARM::ReadCurrentPositionFromRegister(int reg) {
  __ ldr(current_input_offset(), register_location(reg));
}


void RegExpMacroAssemblerARM::ReadStackPointerFromRegister(int reg) {
  __ ldr(backtrack_stackpointer(), register_location(reg));
  __ ldr(r0, MemOperand(frame_pointer(), kStackHighEnd));
  __ add(backtrack_stackpointer(), backtrack_stackpointer(), Operand(r0));
}


void RegExpMacroAssemblerARM::SetCurrentPositionFromEnd(int by) {
  Label after_position;
  __ cmp(current_input_offset(), Operand(-by * char_size()));
  __ b(ge, &after_position);
  __ mov(current_input_offset(), Operand(-by * char_size()));
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&after_position);
}


void RegExpMacroAssemblerARM::SetRegister(int register_index, int to) {
  ASSERT(register_index >= num_saved_registers_);  // Reserved for positions!
  __ mov(r0, Operand(to));
  __ str(r0, register_location(register_index));
}


bool RegExpMacroAssemblerARM::Succeed() {
  __ jmp(&success_label_);
  return global();
}


void RegExpMacroAssemblerARM::WriteCurrentPositionToRegister(int reg,
                                                             int cp_offset) {
  if (cp_offset == 0) {
    __ str(current_input_offset(), register_location(reg));
  } else {
    __ add(r0, current_input_offset(), Operand(cp_offset * char_size()));
    __ str(r0, register_location(reg));
  }
}


void RegExpMacroAssemblerARM::ClearRegisters(int reg_from, int reg_to) {
  ASSERT(reg_from <= reg_to);
  __ ldr(r0, MemOperand(frame_pointer(), kInputStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ str(r0, register_location(reg));
  }
}


void RegExpMacroAssemblerARM::WriteStackPointerToRegister(int reg) {
  __ ldr(r1, MemOperand(frame_pointer(), kStackHighEnd));
  __ sub(r0, backtrack_stackpointer(), r1);
  __ str(r0, register_location(reg));
}


// Private methods:

void RegExpMacroAssemblerARM::CallCheckStackGuardState(Register scratch) {
  __ PrepareCallCFunction(3, scratch);

  // RegExp code frame pointer.
  __ mov(r2, frame_pointer());
  // Code* of self.
  __ mov(r1, Operand(masm_->CodeObject()));

  // We need to make room for the return address on the stack.
  int stack_alignment = OS::ActivationFrameAlignment();
  ASSERT(IsAligned(stack_alignment, kPointerSize));
  __ sub(sp, sp, Operand(stack_alignment));

  // r0 will point to the return address, placed by DirectCEntry.
  __ mov(r0, sp);

  ExternalReference stack_guard_check =
      ExternalReference::re_check_stack_guard_state(isolate());
  __ mov(ip, Operand(stack_guard_check));
  DirectCEntryStub stub;
  stub.GenerateCall(masm_, ip);

  // Drop the return address from the stack.
  __ add(sp, sp, Operand(stack_alignment));

  ASSERT(stack_alignment != 0);
  __ ldr(sp, MemOperand(sp, 0));

  __ mov(code_pointer(), Operand(masm_->CodeObject()));
}


// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  return reinterpret_cast<T&>(Memory::int32_at(re_frame + frame_offset));
}


int RegExpMacroAssemblerARM::CheckStackGuardState(Address* return_address,
                                                  Code* re_code,
                                                  Address re_frame) {
  Isolate* isolate = frame_entry<Isolate*>(re_frame, kIsolate);
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
  bool is_ascii = subject->IsOneByteRepresentationUnderneath();

  ASSERT(re_code->instruction_start() <= *return_address);
  ASSERT(*return_address <=
      re_code->instruction_start() + re_code->instruction_size());

  MaybeObject* result = Execution::HandleStackGuardInterrupt(isolate);

  if (*code_handle != re_code) {  // Return address no longer valid
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
  if (subject_tmp->IsOneByteRepresentation() != is_ascii) {
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
  } else if (frame_entry<const String*>(re_frame, kInputString) != *subject) {
    // Subject string might have been a ConsString that underwent
    // short-circuiting during GC. That will not change start_address but
    // will change pointer inside the subject handle.
    frame_entry<const String*>(re_frame, kInputString) = *subject;
  }

  return 0;
}


MemOperand RegExpMacroAssemblerARM::register_location(int register_index) {
  ASSERT(register_index < (1<<30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return MemOperand(frame_pointer(),
                    kRegisterZero - register_index * kPointerSize);
}


void RegExpMacroAssemblerARM::CheckPosition(int cp_offset,
                                            Label* on_outside_input) {
  __ cmp(current_input_offset(), Operand(-cp_offset * char_size()));
  BranchOrBacktrack(ge, on_outside_input);
}


void RegExpMacroAssemblerARM::BranchOrBacktrack(Condition condition,
                                                Label* to) {
  if (condition == al) {  // Unconditional.
    if (to == NULL) {
      Backtrack();
      return;
    }
    __ jmp(to);
    return;
  }
  if (to == NULL) {
    __ b(condition, &backtrack_label_);
    return;
  }
  __ b(condition, to);
}


void RegExpMacroAssemblerARM::SafeCall(Label* to, Condition cond) {
  __ bl(to, cond);
}


void RegExpMacroAssemblerARM::SafeReturn() {
  __ pop(lr);
  __ add(pc, lr, Operand(masm_->CodeObject()));
}


void RegExpMacroAssemblerARM::SafeCallTarget(Label* name) {
  __ bind(name);
  __ sub(lr, lr, Operand(masm_->CodeObject()));
  __ push(lr);
}


void RegExpMacroAssemblerARM::Push(Register source) {
  ASSERT(!source.is(backtrack_stackpointer()));
  __ str(source,
         MemOperand(backtrack_stackpointer(), kPointerSize, NegPreIndex));
}


void RegExpMacroAssemblerARM::Pop(Register target) {
  ASSERT(!target.is(backtrack_stackpointer()));
  __ ldr(target,
         MemOperand(backtrack_stackpointer(), kPointerSize, PostIndex));
}


void RegExpMacroAssemblerARM::CheckPreemption() {
  // Check for preemption.
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ mov(r0, Operand(stack_limit));
  __ ldr(r0, MemOperand(r0));
  __ cmp(sp, r0);
  SafeCall(&check_preempt_label_, ls);
}


void RegExpMacroAssemblerARM::CheckStackLimit() {
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit(isolate());
  __ mov(r0, Operand(stack_limit));
  __ ldr(r0, MemOperand(r0));
  __ cmp(backtrack_stackpointer(), Operand(r0));
  SafeCall(&stack_overflow_label_, ls);
}


bool RegExpMacroAssemblerARM::CanReadUnaligned() {
  return CpuFeatures::IsSupported(UNALIGNED_ACCESSES) && !slow_safe();
}


void RegExpMacroAssemblerARM::LoadCurrentCharacterUnchecked(int cp_offset,
                                                            int characters) {
  Register offset = current_input_offset();
  if (cp_offset != 0) {
    // r4 is not being used to store the capture start index at this point.
    __ add(r4, current_input_offset(), Operand(cp_offset * char_size()));
    offset = r4;
  }
  // The ldr, str, ldrh, strh instructions can do unaligned accesses, if the CPU
  // and the operating system running on the target allow it.
  // If unaligned load/stores are not supported then this function must only
  // be used to load a single character at a time.
  if (!CanReadUnaligned()) {
    ASSERT(characters == 1);
  }

  if (mode_ == ASCII) {
    if (characters == 4) {
      __ ldr(current_character(), MemOperand(end_of_input_address(), offset));
    } else if (characters == 2) {
      __ ldrh(current_character(), MemOperand(end_of_input_address(), offset));
    } else {
      ASSERT(characters == 1);
      __ ldrb(current_character(), MemOperand(end_of_input_address(), offset));
    }
  } else {
    ASSERT(mode_ == UC16);
    if (characters == 2) {
      __ ldr(current_character(), MemOperand(end_of_input_address(), offset));
    } else {
      ASSERT(characters == 1);
      __ ldrh(current_character(), MemOperand(end_of_input_address(), offset));
    }
  }
}


#undef __

#endif  // V8_INTERPRETED_REGEXP

}}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

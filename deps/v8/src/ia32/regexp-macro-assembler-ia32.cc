// Copyright 2008-2009 the V8 project authors. All rights reserved.
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
#include "unicode.h"
#include "log.h"
#include "ast.h"
#include "regexp-stack.h"
#include "macro-assembler.h"
#include "regexp-macro-assembler.h"
#include "ia32/macro-assembler-ia32.h"
#include "ia32/regexp-macro-assembler-ia32.h"

namespace v8 {
namespace internal {

#ifdef V8_NATIVE_REGEXP
/*
 * This assembler uses the following register assignment convention
 * - edx : current character. Must be loaded using LoadCurrentCharacter
 *         before using any of the dispatch methods.
 * - edi : current position in input, as negative offset from end of string.
 *         Please notice that this is the byte offset, not the character offset!
 * - esi : end of input (points to byte after last character in input).
 * - ebp : frame pointer. Used to access arguments, local variables and
 *         RegExp registers.
 * - esp : points to tip of C stack.
 * - ecx : points to tip of backtrack stack
 *
 * The registers eax, ebx and ecx are free to use for computations.
 *
 * Each call to a public method should retain this convention.
 * The stack will have the following structure:
 *       - direct_call          (if 1, direct call from JavaScript code, if 0
 *                               call through the runtime system)
 *       - stack_area_base      (High end of the memory area to use as
 *                               backtracking stack)
 *       - int* capture_array   (int[num_saved_registers_], for output).
 *       - end of input         (Address of end of string)
 *       - start of input       (Address of first character in string)
 *       - start index          (character index of start)
 *       - String* input_string (location of a handle containing the string)
 *       --- frame alignment (if applicable) ---
 *       - return address
 * ebp-> - old ebp
 *       - backup of caller esi
 *       - backup of caller edi
 *       - backup of caller ebx
 *       - Offset of location before start of input (effectively character
 *         position -1). Used to initialize capture registers to a non-position.
 *       - Boolean at start (if 1, we are starting at the start of the string,
 *         otherwise 0)
 *       - register 0  ebp[-4]  (Only positions must be stored in the first
 *       - register 1  ebp[-8]   num_saved_registers_ registers)
 *       - ...
 *
 * The first num_saved_registers_ registers are initialized to point to
 * "character -1" in the string (i.e., char_size() bytes before the first
 * character of the string). The remaining registers starts out as garbage.
 *
 * The data up to the return address must be placed there by the calling
 * code, by calling the code entry as cast to a function with the signature:
 * int (*match)(String* input_string,
 *              int start_index,
 *              Address start,
 *              Address end,
 *              int* capture_output_array,
 *              bool at_start,
 *              byte* stack_area_base,
 *              bool direct_call)
 */

#define __ ACCESS_MASM(masm_)

RegExpMacroAssemblerIA32::RegExpMacroAssemblerIA32(
    Mode mode,
    int registers_to_save)
    : masm_(new MacroAssembler(NULL, kRegExpCodeSize)),
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


RegExpMacroAssemblerIA32::~RegExpMacroAssemblerIA32() {
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


int RegExpMacroAssemblerIA32::stack_limit_slack()  {
  return RegExpStack::kStackLimitSlack;
}


void RegExpMacroAssemblerIA32::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    Label inside_string;
    __ add(Operand(edi), Immediate(by * char_size()));
  }
}


void RegExpMacroAssemblerIA32::AdvanceRegister(int reg, int by) {
  ASSERT(reg >= 0);
  ASSERT(reg < num_registers_);
  if (by != 0) {
    __ add(register_location(reg), Immediate(by));
  }
}


void RegExpMacroAssemblerIA32::Backtrack() {
  CheckPreemption();
  // Pop Code* offset from backtrack stack, add Code* and jump to location.
  Pop(ebx);
  __ add(Operand(ebx), Immediate(masm_->CodeObject()));
  __ jmp(Operand(ebx));
}


void RegExpMacroAssemblerIA32::Bind(Label* label) {
  __ bind(label);
}


void RegExpMacroAssemblerIA32::CheckCharacter(uint32_t c, Label* on_equal) {
  __ cmp(current_character(), c);
  BranchOrBacktrack(equal, on_equal);
}


void RegExpMacroAssemblerIA32::CheckCharacterGT(uc16 limit, Label* on_greater) {
  __ cmp(current_character(), limit);
  BranchOrBacktrack(greater, on_greater);
}


void RegExpMacroAssemblerIA32::CheckAtStart(Label* on_at_start) {
  Label not_at_start;
  // Did we start the match at the start of the string at all?
  __ cmp(Operand(ebp, kAtStart), Immediate(0));
  BranchOrBacktrack(equal, &not_at_start);
  // If we did, are we still at the start of the input?
  __ lea(eax, Operand(esi, edi, times_1, 0));
  __ cmp(eax, Operand(ebp, kInputStart));
  BranchOrBacktrack(equal, on_at_start);
  __ bind(&not_at_start);
}


void RegExpMacroAssemblerIA32::CheckNotAtStart(Label* on_not_at_start) {
  // Did we start the match at the start of the string at all?
  __ cmp(Operand(ebp, kAtStart), Immediate(0));
  BranchOrBacktrack(equal, on_not_at_start);
  // If we did, are we still at the start of the input?
  __ lea(eax, Operand(esi, edi, times_1, 0));
  __ cmp(eax, Operand(ebp, kInputStart));
  BranchOrBacktrack(not_equal, on_not_at_start);
}


void RegExpMacroAssemblerIA32::CheckCharacterLT(uc16 limit, Label* on_less) {
  __ cmp(current_character(), limit);
  BranchOrBacktrack(less, on_less);
}


void RegExpMacroAssemblerIA32::CheckCharacters(Vector<const uc16> str,
                                               int cp_offset,
                                               Label* on_failure,
                                               bool check_end_of_string) {
  int byte_length = str.length() * char_size();
  int byte_offset = cp_offset * char_size();
  if (check_end_of_string) {
    // Check that there are at least str.length() characters left in the input.
    __ cmp(Operand(edi), Immediate(-(byte_offset + byte_length)));
    BranchOrBacktrack(greater, on_failure);
  }

  if (on_failure == NULL) {
    // Instead of inlining a backtrack, (re)use the global backtrack target.
    on_failure = &backtrack_label_;
  }

  for (int i = 0; i < str.length(); i++) {
    if (mode_ == ASCII) {
      __ cmpb(Operand(esi, edi, times_1, byte_offset + i),
              static_cast<int8_t>(str[i]));
    } else {
      ASSERT(mode_ == UC16);
      __ cmpw(Operand(esi, edi, times_1, byte_offset + i * sizeof(uc16)),
              Immediate(str[i]));
    }
    BranchOrBacktrack(not_equal, on_failure);
  }
}


void RegExpMacroAssemblerIA32::CheckGreedyLoop(Label* on_equal) {
  Label fallthrough;
  __ cmp(edi, Operand(backtrack_stackpointer(), 0));
  __ j(not_equal, &fallthrough);
  __ add(Operand(backtrack_stackpointer()), Immediate(kPointerSize));  // Pop.
  BranchOrBacktrack(no_condition, on_equal);
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerIA32::CheckNotBackReferenceIgnoreCase(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  __ mov(edx, register_location(start_reg));  // Index of start of capture
  __ mov(ebx, register_location(start_reg + 1));  // Index of end of capture
  __ sub(ebx, Operand(edx));  // Length of capture.

  // The length of a capture should not be negative. This can only happen
  // if the end of the capture is unrecorded, or at a point earlier than
  // the start of the capture.
  BranchOrBacktrack(less, on_no_match, not_taken);

  // If length is zero, either the capture is empty or it is completely
  // uncaptured. In either case succeed immediately.
  __ j(equal, &fallthrough);

  if (mode_ == ASCII) {
    Label success;
    Label fail;
    Label loop_increment;
    // Save register contents to make the registers available below.
    __ push(edi);
    __ push(backtrack_stackpointer());
    // After this, the eax, ecx, and edi registers are available.

    __ add(edx, Operand(esi));  // Start of capture
    __ add(edi, Operand(esi));  // Start of text to match against capture.
    __ add(ebx, Operand(edi));  // End of text to match against capture.

    Label loop;
    __ bind(&loop);
    __ movzx_b(eax, Operand(edi, 0));
    __ cmpb_al(Operand(edx, 0));
    __ j(equal, &loop_increment);

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ or_(eax, 0x20);  // Convert match character to lower-case.
    __ lea(ecx, Operand(eax, -'a'));
    __ cmp(ecx, static_cast<int32_t>('z' - 'a'));  // Is eax a lowercase letter?
    __ j(above, &fail);
    // Also convert capture character.
    __ movzx_b(ecx, Operand(edx, 0));
    __ or_(ecx, 0x20);

    __ cmp(eax, Operand(ecx));
    __ j(not_equal, &fail);

    __ bind(&loop_increment);
    // Increment pointers into match and capture strings.
    __ add(Operand(edx), Immediate(1));
    __ add(Operand(edi), Immediate(1));
    // Compare to end of match, and loop if not done.
    __ cmp(edi, Operand(ebx));
    __ j(below, &loop, taken);
    __ jmp(&success);

    __ bind(&fail);
    // Restore original values before failing.
    __ pop(backtrack_stackpointer());
    __ pop(edi);
    BranchOrBacktrack(no_condition, on_no_match);

    __ bind(&success);
    // Restore original value before continuing.
    __ pop(backtrack_stackpointer());
    // Drop original value of character position.
    __ add(Operand(esp), Immediate(kPointerSize));
    // Compute new value of character position after the matched part.
    __ sub(edi, Operand(esi));
  } else {
    ASSERT(mode_ == UC16);
    // Save registers before calling C function.
    __ push(esi);
    __ push(edi);
    __ push(backtrack_stackpointer());
    __ push(ebx);

    static const int argument_count = 3;
    __ PrepareCallCFunction(argument_count, ecx);
    // Put arguments into allocated stack area, last argument highest on stack.
    // Parameters are
    //   Address byte_offset1 - Address captured substring's start.
    //   Address byte_offset2 - Address of current character position.
    //   size_t byte_length - length of capture in bytes(!)

    // Set byte_length.
    __ mov(Operand(esp, 2 * kPointerSize), ebx);
    // Set byte_offset2.
    // Found by adding negative string-end offset of current position (edi)
    // to end of string.
    __ add(edi, Operand(esi));
    __ mov(Operand(esp, 1 * kPointerSize), edi);
    // Set byte_offset1.
    // Start of capture, where edx already holds string-end negative offset.
    __ add(edx, Operand(esi));
    __ mov(Operand(esp, 0 * kPointerSize), edx);

    ExternalReference compare =
        ExternalReference::re_case_insensitive_compare_uc16();
    __ CallCFunction(compare, argument_count);
    // Pop original values before reacting on result value.
    __ pop(ebx);
    __ pop(backtrack_stackpointer());
    __ pop(edi);
    __ pop(esi);

    // Check if function returned non-zero for success or zero for failure.
    __ or_(eax, Operand(eax));
    BranchOrBacktrack(zero, on_no_match);
    // On success, increment position by length of capture.
    __ add(edi, Operand(ebx));
  }
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerIA32::CheckNotBackReference(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  Label success;
  Label fail;

  // Find length of back-referenced capture.
  __ mov(edx, register_location(start_reg));
  __ mov(eax, register_location(start_reg + 1));
  __ sub(eax, Operand(edx));  // Length to check.
  // Fail on partial or illegal capture (start of capture after end of capture).
  BranchOrBacktrack(less, on_no_match);
  // Succeed on empty capture (including no capture)
  __ j(equal, &fallthrough);

  // Check that there are sufficient characters left in the input.
  __ mov(ebx, edi);
  __ add(ebx, Operand(eax));
  BranchOrBacktrack(greater, on_no_match);

  // Save register to make it available below.
  __ push(backtrack_stackpointer());

  // Compute pointers to match string and capture string
  __ lea(ebx, Operand(esi, edi, times_1, 0));  // Start of match.
  __ add(edx, Operand(esi));  // Start of capture.
  __ lea(ecx, Operand(eax, ebx, times_1, 0));  // End of match

  Label loop;
  __ bind(&loop);
  if (mode_ == ASCII) {
    __ movzx_b(eax, Operand(edx, 0));
    __ cmpb_al(Operand(ebx, 0));
  } else {
    ASSERT(mode_ == UC16);
    __ movzx_w(eax, Operand(edx, 0));
    __ cmpw_ax(Operand(ebx, 0));
  }
  __ j(not_equal, &fail);
  // Increment pointers into capture and match string.
  __ add(Operand(edx), Immediate(char_size()));
  __ add(Operand(ebx), Immediate(char_size()));
  // Check if we have reached end of match area.
  __ cmp(ebx, Operand(ecx));
  __ j(below, &loop);
  __ jmp(&success);

  __ bind(&fail);
  // Restore backtrack stackpointer.
  __ pop(backtrack_stackpointer());
  BranchOrBacktrack(no_condition, on_no_match);

  __ bind(&success);
  // Move current character position to position after match.
  __ mov(edi, ecx);
  __ sub(Operand(edi), esi);
  // Restore backtrack stackpointer.
  __ pop(backtrack_stackpointer());

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerIA32::CheckNotRegistersEqual(int reg1,
                                                      int reg2,
                                                      Label* on_not_equal) {
  __ mov(eax, register_location(reg1));
  __ cmp(eax, register_location(reg2));
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerIA32::CheckNotCharacter(uint32_t c,
                                                 Label* on_not_equal) {
  __ cmp(current_character(), c);
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerIA32::CheckCharacterAfterAnd(uint32_t c,
                                                      uint32_t mask,
                                                      Label* on_equal) {
  __ mov(eax, current_character());
  __ and_(eax, mask);
  __ cmp(eax, c);
  BranchOrBacktrack(equal, on_equal);
}


void RegExpMacroAssemblerIA32::CheckNotCharacterAfterAnd(uint32_t c,
                                                         uint32_t mask,
                                                         Label* on_not_equal) {
  __ mov(eax, current_character());
  __ and_(eax, mask);
  __ cmp(eax, c);
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerIA32::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  ASSERT(minus < String::kMaxUC16CharCode);
  __ lea(eax, Operand(current_character(), -minus));
  __ and_(eax, mask);
  __ cmp(eax, c);
  BranchOrBacktrack(not_equal, on_not_equal);
}


bool RegExpMacroAssemblerIA32::CheckSpecialCharacterClass(uc16 type,
                                                          Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check
  switch (type) {
  case 's':
    // Match space-characters
    if (mode_ == ASCII) {
      // ASCII space characters are '\t'..'\r' and ' '.
      Label success;
      __ cmp(current_character(), ' ');
      __ j(equal, &success);
      // Check range 0x09..0x0d
      __ lea(eax, Operand(current_character(), -'\t'));
      __ cmp(eax, '\r' - '\t');
      BranchOrBacktrack(above, on_no_match);
      __ bind(&success);
      return true;
    }
    return false;
  case 'S':
    // Match non-space characters.
    if (mode_ == ASCII) {
      // ASCII space characters are '\t'..'\r' and ' '.
      __ cmp(current_character(), ' ');
      BranchOrBacktrack(equal, on_no_match);
      __ lea(eax, Operand(current_character(), -'\t'));
      __ cmp(eax, '\r' - '\t');
      BranchOrBacktrack(below_equal, on_no_match);
      return true;
    }
    return false;
  case 'd':
    // Match ASCII digits ('0'..'9')
    __ lea(eax, Operand(current_character(), -'0'));
    __ cmp(eax, '9' - '0');
    BranchOrBacktrack(above, on_no_match);
    return true;
  case 'D':
    // Match non ASCII-digits
    __ lea(eax, Operand(current_character(), -'0'));
    __ cmp(eax, '9' - '0');
    BranchOrBacktrack(below_equal, on_no_match);
    return true;
  case '.': {
    // Match non-newlines (not 0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029)
    __ mov(Operand(eax), current_character());
    __ xor_(Operand(eax), Immediate(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ sub(Operand(eax), Immediate(0x0b));
    __ cmp(eax, 0x0c - 0x0b);
    BranchOrBacktrack(below_equal, on_no_match);
    if (mode_ == UC16) {
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ sub(Operand(eax), Immediate(0x2028 - 0x0b));
      __ cmp(eax, 0x2029 - 0x2028);
      BranchOrBacktrack(below_equal, on_no_match);
    }
    return true;
  }
  case 'w': {
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      __ cmp(Operand(current_character()), Immediate('z'));
      BranchOrBacktrack(above, on_no_match);
    }
    ASSERT_EQ(0, word_character_map[0]);  // Character '\0' is not a word char.
    ExternalReference word_map = ExternalReference::re_word_character_map();
    __ test_b(current_character(),
              Operand::StaticArray(current_character(), times_1, word_map));
    BranchOrBacktrack(zero, on_no_match);
    return true;
  }
  case 'W': {
    Label done;
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      __ cmp(Operand(current_character()), Immediate('z'));
      __ j(above, &done);
    }
    ASSERT_EQ(0, word_character_map[0]);  // Character '\0' is not a word char.
    ExternalReference word_map = ExternalReference::re_word_character_map();
    __ test_b(current_character(),
              Operand::StaticArray(current_character(), times_1, word_map));
    BranchOrBacktrack(not_zero, on_no_match);
    if (mode_ != ASCII) {
      __ bind(&done);
    }
    return true;
  }
  // Non-standard classes (with no syntactic shorthand) used internally.
  case '*':
    // Match any character.
    return true;
  case 'n': {
    // Match newlines (0x0a('\n'), 0x0d('\r'), 0x2028 or 0x2029).
    // The opposite of '.'.
    __ mov(Operand(eax), current_character());
    __ xor_(Operand(eax), Immediate(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ sub(Operand(eax), Immediate(0x0b));
    __ cmp(eax, 0x0c - 0x0b);
    if (mode_ == ASCII) {
      BranchOrBacktrack(above, on_no_match);
    } else {
      Label done;
      BranchOrBacktrack(below_equal, &done);
      ASSERT_EQ(UC16, mode_);
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ sub(Operand(eax), Immediate(0x2028 - 0x0b));
      __ cmp(eax, 1);
      BranchOrBacktrack(above, on_no_match);
      __ bind(&done);
    }
    return true;
  }
  // No custom implementation (yet): s(UC16), S(UC16).
  default:
    return false;
  }
}


void RegExpMacroAssemblerIA32::Fail() {
  ASSERT(FAILURE == 0);  // Return value for failure is zero.
  __ xor_(eax, Operand(eax));  // zero eax.
  __ jmp(&exit_label_);
}


Handle<Object> RegExpMacroAssemblerIA32::GetCode(Handle<String> source) {
  // Finalize code - write the entry point code now we know how many
  // registers we need.

  // Entry code:
  __ bind(&entry_label_);
  // Start new stack frame.
  __ push(ebp);
  __ mov(ebp, esp);
  // Save callee-save registers. Order here should correspond to order of
  // kBackup_ebx etc.
  __ push(esi);
  __ push(edi);
  __ push(ebx);  // Callee-save on MacOS.
  __ push(Immediate(0));  // Make room for "input start - 1" constant.
  __ push(Immediate(0));  // Make room for "at start" constant.

  // Check if we have space on the stack for registers.
  Label stack_limit_hit;
  Label stack_ok;

  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit();
  __ mov(ecx, esp);
  __ sub(ecx, Operand::StaticVariable(stack_limit));
  // Handle it if the stack pointer is already below the stack limit.
  __ j(below_equal, &stack_limit_hit, not_taken);
  // Check if there is room for the variable number of registers above
  // the stack limit.
  __ cmp(ecx, num_registers_ * kPointerSize);
  __ j(above_equal, &stack_ok, taken);
  // Exit with OutOfMemory exception. There is not enough space on the stack
  // for our working registers.
  __ mov(eax, EXCEPTION);
  __ jmp(&exit_label_);

  __ bind(&stack_limit_hit);
  CallCheckStackGuardState(ebx);
  __ or_(eax, Operand(eax));
  // If returned value is non-zero, we exit with the returned value as result.
  __ j(not_zero, &exit_label_);

  __ bind(&stack_ok);

  // Allocate space on stack for registers.
  __ sub(Operand(esp), Immediate(num_registers_ * kPointerSize));
  // Load string length.
  __ mov(esi, Operand(ebp, kInputEnd));
  // Load input position.
  __ mov(edi, Operand(ebp, kInputStart));
  // Set up edi to be negative offset from string end.
  __ sub(edi, Operand(esi));
  // Set eax to address of char before start of input
  // (effectively string position -1).
  __ lea(eax, Operand(edi, -char_size()));
  // Store this value in a local variable, for use when clearing
  // position registers.
  __ mov(Operand(ebp, kInputStartMinusOne), eax);

  // Determine whether the start index is zero, that is at the start of the
  // string, and store that value in a local variable.
  __ mov(ebx, Operand(ebp, kStartIndex));
  __ xor_(Operand(ecx), ecx);  // setcc only operates on cl (lower byte of ecx).
  __ test(ebx, Operand(ebx));
  __ setcc(zero, ecx);  // 1 if 0 (start of string), 0 if positive.
  __ mov(Operand(ebp, kAtStart), ecx);

  if (num_saved_registers_ > 0) {  // Always is, if generated from a regexp.
    // Fill saved registers with initial value = start offset - 1
    // Fill in stack push order, to avoid accessing across an unwritten
    // page (a problem on Windows).
    __ mov(ecx, kRegisterZero);
    Label init_loop;
    __ bind(&init_loop);
    __ mov(Operand(ebp, ecx, times_1, +0), eax);
    __ sub(Operand(ecx), Immediate(kPointerSize));
    __ cmp(ecx, kRegisterZero - num_saved_registers_ * kPointerSize);
    __ j(greater, &init_loop);
  }
  // Ensure that we have written to each stack page, in order. Skipping a page
  // on Windows can cause segmentation faults. Assuming page size is 4k.
  const int kPageSize = 4096;
  const int kRegistersPerPage = kPageSize / kPointerSize;
  for (int i = num_saved_registers_ + kRegistersPerPage - 1;
      i < num_registers_;
      i += kRegistersPerPage) {
    __ mov(register_location(i), eax);  // One write every page.
  }


  // Initialize backtrack stack pointer.
  __ mov(backtrack_stackpointer(), Operand(ebp, kStackHighEnd));
  // Load previous char as initial value of current-character.
  Label at_start;
  __ cmp(Operand(ebp, kAtStart), Immediate(0));
  __ j(not_equal, &at_start);
  LoadCurrentCharacterUnchecked(-1, 1);  // Load previous char.
  __ jmp(&start_label_);
  __ bind(&at_start);
  __ mov(current_character(), '\n');
  __ jmp(&start_label_);


  // Exit code:
  if (success_label_.is_linked()) {
    // Save captures when successful.
    __ bind(&success_label_);
    if (num_saved_registers_ > 0) {
      // copy captures to output
      __ mov(ebx, Operand(ebp, kRegisterOutput));
      __ mov(ecx, Operand(ebp, kInputEnd));
      __ sub(ecx, Operand(ebp, kInputStart));
      for (int i = 0; i < num_saved_registers_; i++) {
        __ mov(eax, register_location(i));
        __ add(eax, Operand(ecx));  // Convert to index from start, not end.
        if (mode_ == UC16) {
          __ sar(eax, 1);  // Convert byte index to character index.
        }
        __ mov(Operand(ebx, i * kPointerSize), eax);
      }
    }
    __ mov(eax, Immediate(SUCCESS));
  }
  // Exit and return eax
  __ bind(&exit_label_);
  // Skip esp past regexp registers.
  __ lea(esp, Operand(ebp, kBackup_ebx));
  // Restore callee-save registers.
  __ pop(ebx);
  __ pop(edi);
  __ pop(esi);
  // Exit function frame, restore previous one.
  __ pop(ebp);
  __ ret(0);

  // Backtrack code (branch target for conditional backtracks).
  if (backtrack_label_.is_linked()) {
    __ bind(&backtrack_label_);
    Backtrack();
  }

  Label exit_with_exception;

  // Preempt-code
  if (check_preempt_label_.is_linked()) {
    SafeCallTarget(&check_preempt_label_);

    __ push(backtrack_stackpointer());
    __ push(edi);

    CallCheckStackGuardState(ebx);
    __ or_(eax, Operand(eax));
    // If returning non-zero, we should end execution with the given
    // result as return value.
    __ j(not_zero, &exit_label_);

    __ pop(edi);
    __ pop(backtrack_stackpointer());
    // String might have moved: Reload esi from frame.
    __ mov(esi, Operand(ebp, kInputEnd));
    SafeReturn();
  }

  // Backtrack stack overflow code.
  if (stack_overflow_label_.is_linked()) {
    SafeCallTarget(&stack_overflow_label_);
    // Reached if the backtrack-stack limit has been hit.

    Label grow_failed;
    // Save registers before calling C function
    __ push(esi);
    __ push(edi);

    // Call GrowStack(backtrack_stackpointer())
    static const int num_arguments = 2;
    __ PrepareCallCFunction(num_arguments, ebx);
    __ lea(eax, Operand(ebp, kStackHighEnd));
    __ mov(Operand(esp, 1 * kPointerSize), eax);
    __ mov(Operand(esp, 0 * kPointerSize), backtrack_stackpointer());
    ExternalReference grow_stack = ExternalReference::re_grow_stack();
    __ CallCFunction(grow_stack, num_arguments);
    // If return NULL, we have failed to grow the stack, and
    // must exit with a stack-overflow exception.
    __ or_(eax, Operand(eax));
    __ j(equal, &exit_with_exception);
    // Otherwise use return value as new stack pointer.
    __ mov(backtrack_stackpointer(), eax);
    // Restore saved registers and continue.
    __ pop(edi);
    __ pop(esi);
    SafeReturn();
  }

  if (exit_with_exception.is_linked()) {
    // If any of the code above needed to exit with an exception.
    __ bind(&exit_with_exception);
    // Exit with Result EXCEPTION(-1) to signal thrown exception.
    __ mov(eax, EXCEPTION);
    __ jmp(&exit_label_);
  }

  CodeDesc code_desc;
  masm_->GetCode(&code_desc);
  Handle<Code> code = Factory::NewCode(code_desc,
                                       NULL,
                                       Code::ComputeFlags(Code::REGEXP),
                                       masm_->CodeObject());
  LOG(RegExpCodeCreateEvent(*code, *source));
  return Handle<Object>::cast(code);
}


void RegExpMacroAssemblerIA32::GoTo(Label* to) {
  BranchOrBacktrack(no_condition, to);
}


void RegExpMacroAssemblerIA32::IfRegisterGE(int reg,
                                            int comparand,
                                            Label* if_ge) {
  __ cmp(register_location(reg), Immediate(comparand));
  BranchOrBacktrack(greater_equal, if_ge);
}


void RegExpMacroAssemblerIA32::IfRegisterLT(int reg,
                                            int comparand,
                                            Label* if_lt) {
  __ cmp(register_location(reg), Immediate(comparand));
  BranchOrBacktrack(less, if_lt);
}


void RegExpMacroAssemblerIA32::IfRegisterEqPos(int reg,
                                               Label* if_eq) {
  __ cmp(edi, register_location(reg));
  BranchOrBacktrack(equal, if_eq);
}


RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerIA32::Implementation() {
  return kIA32Implementation;
}


void RegExpMacroAssemblerIA32::LoadCurrentCharacter(int cp_offset,
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


void RegExpMacroAssemblerIA32::PopCurrentPosition() {
  Pop(edi);
}


void RegExpMacroAssemblerIA32::PopRegister(int register_index) {
  Pop(eax);
  __ mov(register_location(register_index), eax);
}


void RegExpMacroAssemblerIA32::PushBacktrack(Label* label) {
  Push(Immediate::CodeRelativeOffset(label));
  CheckStackLimit();
}


void RegExpMacroAssemblerIA32::PushCurrentPosition() {
  Push(edi);
}


void RegExpMacroAssemblerIA32::PushRegister(int register_index,
                                            StackCheckFlag check_stack_limit) {
  __ mov(eax, register_location(register_index));
  Push(eax);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerIA32::ReadCurrentPositionFromRegister(int reg) {
  __ mov(edi, register_location(reg));
}


void RegExpMacroAssemblerIA32::ReadStackPointerFromRegister(int reg) {
  __ mov(backtrack_stackpointer(), register_location(reg));
  __ add(backtrack_stackpointer(), Operand(ebp, kStackHighEnd));
}


void RegExpMacroAssemblerIA32::SetRegister(int register_index, int to) {
  ASSERT(register_index >= num_saved_registers_);  // Reserved for positions!
  __ mov(register_location(register_index), Immediate(to));
}


void RegExpMacroAssemblerIA32::Succeed() {
  __ jmp(&success_label_);
}


void RegExpMacroAssemblerIA32::WriteCurrentPositionToRegister(int reg,
                                                              int cp_offset) {
  if (cp_offset == 0) {
    __ mov(register_location(reg), edi);
  } else {
    __ lea(eax, Operand(edi, cp_offset * char_size()));
    __ mov(register_location(reg), eax);
  }
}


void RegExpMacroAssemblerIA32::ClearRegisters(int reg_from, int reg_to) {
  ASSERT(reg_from <= reg_to);
  __ mov(eax, Operand(ebp, kInputStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ mov(register_location(reg), eax);
  }
}


void RegExpMacroAssemblerIA32::WriteStackPointerToRegister(int reg) {
  __ mov(eax, backtrack_stackpointer());
  __ sub(eax, Operand(ebp, kStackHighEnd));
  __ mov(register_location(reg), eax);
}


// Private methods:

void RegExpMacroAssemblerIA32::CallCheckStackGuardState(Register scratch) {
  static const int num_arguments = 3;
  __ PrepareCallCFunction(num_arguments, scratch);
  // RegExp code frame pointer.
  __ mov(Operand(esp, 2 * kPointerSize), ebp);
  // Code* of self.
  __ mov(Operand(esp, 1 * kPointerSize), Immediate(masm_->CodeObject()));
  // Next address on the stack (will be address of return address).
  __ lea(eax, Operand(esp, -kPointerSize));
  __ mov(Operand(esp, 0 * kPointerSize), eax);
  ExternalReference check_stack_guard =
      ExternalReference::re_check_stack_guard_state();
  __ CallCFunction(check_stack_guard, num_arguments);
}


// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  return reinterpret_cast<T&>(Memory::int32_at(re_frame + frame_offset));
}


int RegExpMacroAssemblerIA32::CheckStackGuardState(Address* return_address,
                                                   Code* re_code,
                                                   Address re_frame) {
  if (StackGuard::IsStackOverflow()) {
    Top::StackOverflow();
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
  HandleScope handles;
  Handle<Code> code_handle(re_code);

  Handle<String> subject(frame_entry<String*>(re_frame, kInputString));
  // Current string.
  bool is_ascii = subject->IsAsciiRepresentation();

  ASSERT(re_code->instruction_start() <= *return_address);
  ASSERT(*return_address <=
      re_code->instruction_start() + re_code->instruction_size());

  Object* result = Execution::HandleStackGuardInterrupt();

  if (*code_handle != re_code) {  // Return address no longer valid
    int delta = *code_handle - re_code;
    // Overwrite the return address on the stack.
    *return_address += delta;
  }

  if (result->IsException()) {
    return EXCEPTION;
  }

  // String might have changed.
  if (subject->IsAsciiRepresentation() != is_ascii) {
    // If we changed between an ASCII and an UC16 string, the specialized
    // code cannot be used, and we need to restart regexp matching from
    // scratch (including, potentially, compiling a new version of the code).
    return RETRY;
  }

  // Otherwise, the content of the string might have moved. It must still
  // be a sequential or external string with the same content.
  // Update the start and end pointers in the stack frame to the current
  // location (whether it has actually moved or not).
  ASSERT(StringShape(*subject).IsSequential() ||
      StringShape(*subject).IsExternal());

  // The original start address of the characters to match.
  const byte* start_address = frame_entry<const byte*>(re_frame, kInputStart);

  // Find the current start address of the same character at the current string
  // position.
  int start_index = frame_entry<int>(re_frame, kStartIndex);
  const byte* new_address = StringCharacterPosition(*subject, start_index);

  if (start_address != new_address) {
    // If there is a difference, update the object pointer and start and end
    // addresses in the RegExp stack frame to match the new value.
    const byte* end_address = frame_entry<const byte* >(re_frame, kInputEnd);
    int byte_length = end_address - start_address;
    frame_entry<const String*>(re_frame, kInputString) = *subject;
    frame_entry<const byte*>(re_frame, kInputStart) = new_address;
    frame_entry<const byte*>(re_frame, kInputEnd) = new_address + byte_length;
  }

  return 0;
}


Operand RegExpMacroAssemblerIA32::register_location(int register_index) {
  ASSERT(register_index < (1<<30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return Operand(ebp, kRegisterZero - register_index * kPointerSize);
}


void RegExpMacroAssemblerIA32::CheckPosition(int cp_offset,
                                             Label* on_outside_input) {
  __ cmp(edi, -cp_offset * char_size());
  BranchOrBacktrack(greater_equal, on_outside_input);
}


void RegExpMacroAssemblerIA32::BranchOrBacktrack(Condition condition,
                                                 Label* to,
                                                 Hint hint) {
  if (condition < 0) {  // No condition
    if (to == NULL) {
      Backtrack();
      return;
    }
    __ jmp(to);
    return;
  }
  if (to == NULL) {
    __ j(condition, &backtrack_label_, hint);
    return;
  }
  __ j(condition, to, hint);
}


void RegExpMacroAssemblerIA32::SafeCall(Label* to) {
  __ call(to);
}


void RegExpMacroAssemblerIA32::SafeReturn() {
  __ add(Operand(esp, 0), Immediate(masm_->CodeObject()));
  __ ret(0);
}


void RegExpMacroAssemblerIA32::SafeCallTarget(Label* name) {
  __ bind(name);
  __ sub(Operand(esp, 0), Immediate(masm_->CodeObject()));
}


void RegExpMacroAssemblerIA32::Push(Register source) {
  ASSERT(!source.is(backtrack_stackpointer()));
  // Notice: This updates flags, unlike normal Push.
  __ sub(Operand(backtrack_stackpointer()), Immediate(kPointerSize));
  __ mov(Operand(backtrack_stackpointer(), 0), source);
}


void RegExpMacroAssemblerIA32::Push(Immediate value) {
  // Notice: This updates flags, unlike normal Push.
  __ sub(Operand(backtrack_stackpointer()), Immediate(kPointerSize));
  __ mov(Operand(backtrack_stackpointer(), 0), value);
}


void RegExpMacroAssemblerIA32::Pop(Register target) {
  ASSERT(!target.is(backtrack_stackpointer()));
  __ mov(target, Operand(backtrack_stackpointer(), 0));
  // Notice: This updates flags, unlike normal Pop.
  __ add(Operand(backtrack_stackpointer()), Immediate(kPointerSize));
}


void RegExpMacroAssemblerIA32::CheckPreemption() {
  // Check for preemption.
  Label no_preempt;
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit();
  __ cmp(esp, Operand::StaticVariable(stack_limit));
  __ j(above, &no_preempt, taken);

  SafeCall(&check_preempt_label_);

  __ bind(&no_preempt);
}


void RegExpMacroAssemblerIA32::CheckStackLimit() {
  Label no_stack_overflow;
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit();
  __ cmp(backtrack_stackpointer(), Operand::StaticVariable(stack_limit));
  __ j(above, &no_stack_overflow);

  SafeCall(&stack_overflow_label_);

  __ bind(&no_stack_overflow);
}


void RegExpMacroAssemblerIA32::LoadCurrentCharacterUnchecked(int cp_offset,
                                                             int characters) {
  if (mode_ == ASCII) {
    if (characters == 4) {
      __ mov(current_character(), Operand(esi, edi, times_1, cp_offset));
    } else if (characters == 2) {
      __ movzx_w(current_character(), Operand(esi, edi, times_1, cp_offset));
    } else {
      ASSERT(characters == 1);
      __ movzx_b(current_character(), Operand(esi, edi, times_1, cp_offset));
    }
  } else {
    ASSERT(mode_ == UC16);
    if (characters == 2) {
      __ mov(current_character(),
             Operand(esi, edi, times_1, cp_offset * sizeof(uc16)));
    } else {
      ASSERT(characters == 1);
      __ movzx_w(current_character(),
                 Operand(esi, edi, times_1, cp_offset * sizeof(uc16)));
    }
  }
}


#undef __

#endif  // V8_NATIVE_REGEXP

}}  // namespace v8::internal

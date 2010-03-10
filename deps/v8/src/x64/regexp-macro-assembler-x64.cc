// Copyright 2009 the V8 project authors. All rights reserved.
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
#include "serialize.h"
#include "unicode.h"
#include "log.h"
#include "ast.h"
#include "regexp-stack.h"
#include "macro-assembler.h"
#include "regexp-macro-assembler.h"
#include "x64/macro-assembler-x64.h"
#include "x64/regexp-macro-assembler-x64.h"

namespace v8 {
namespace internal {

#ifdef V8_NATIVE_REGEXP

/*
 * This assembler uses the following register assignment convention
 * - rdx : currently loaded character(s) as ASCII or UC16. Must be loaded using
 *         LoadCurrentCharacter before using any of the dispatch methods.
 * - rdi : current position in input, as negative offset from end of string.
 *         Please notice that this is the byte offset, not the character
 *         offset! Is always a 32-bit signed (negative) offset, but must be
 *         maintained sign-extended to 64 bits, since it is used as index.
 * - rsi : end of input (points to byte after last character in input),
 *         so that rsi+rdi points to the current character.
 * - rbp : frame pointer. Used to access arguments, local variables and
 *         RegExp registers.
 * - rsp : points to tip of C stack.
 * - rcx : points to tip of backtrack stack. The backtrack stack contains
 *         only 32-bit values. Most are offsets from some base (e.g., character
 *         positions from end of string or code location from Code* pointer).
 * - r8  : code object pointer. Used to convert between absolute and
 *         code-object-relative addresses.
 *
 * The registers rax, rbx, r9 and r11 are free to use for computations.
 * If changed to use r12+, they should be saved as callee-save registers.
 *
 * Each call to a C++ method should retain these registers.
 *
 * The stack will have the following content, in some order, indexable from the
 * frame pointer (see, e.g., kStackHighEnd):
 *    - direct_call          (if 1, direct call from JavaScript code, if 0 call
 *                            through the runtime system)
 *    - stack_area_base      (High end of the memory area to use as
 *                            backtracking stack)
 *    - int* capture_array   (int[num_saved_registers_], for output).
 *    - end of input         (Address of end of string)
 *    - start of input       (Address of first character in string)
 *    - start index          (character index of start)
 *    - String* input_string (input string)
 *    - return address
 *    - backup of callee save registers (rbx, possibly rsi and rdi).
 *    - Offset of location before start of input (effectively character
 *      position -1). Used to initialize capture registers to a non-position.
 *    - At start of string (if 1, we are starting at the start of the
 *      string, otherwise 0)
 *    - register 0  rbp[-n]   (Only positions must be stored in the first
 *    - register 1  rbp[-n-8]  num_saved_registers_ registers)
 *    - ...
 *
 * The first num_saved_registers_ registers are initialized to point to
 * "character -1" in the string (i.e., char_size() bytes before the first
 * character of the string). The remaining registers starts out uninitialized.
 *
 * The first seven values must be provided by the calling code by
 * calling the code's entry address cast to a function pointer with the
 * following signature:
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

RegExpMacroAssemblerX64::RegExpMacroAssemblerX64(
    Mode mode,
    int registers_to_save)
    : masm_(new MacroAssembler(NULL, kRegExpCodeSize)),
      code_relative_fixup_positions_(4),
      mode_(mode),
      num_registers_(registers_to_save),
      num_saved_registers_(registers_to_save),
      entry_label_(),
      start_label_(),
      success_label_(),
      backtrack_label_(),
      exit_label_() {
  ASSERT_EQ(0, registers_to_save % 2);
  __ jmp(&entry_label_);   // We'll write the entry code when we know more.
  __ bind(&start_label_);  // And then continue from here.
}


RegExpMacroAssemblerX64::~RegExpMacroAssemblerX64() {
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


int RegExpMacroAssemblerX64::stack_limit_slack()  {
  return RegExpStack::kStackLimitSlack;
}


void RegExpMacroAssemblerX64::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    Label inside_string;
    __ addq(rdi, Immediate(by * char_size()));
  }
}


void RegExpMacroAssemblerX64::AdvanceRegister(int reg, int by) {
  ASSERT(reg >= 0);
  ASSERT(reg < num_registers_);
  if (by != 0) {
    __ addq(register_location(reg), Immediate(by));
  }
}


void RegExpMacroAssemblerX64::Backtrack() {
  CheckPreemption();
  // Pop Code* offset from backtrack stack, add Code* and jump to location.
  Pop(rbx);
  __ addq(rbx, code_object_pointer());
  __ jmp(rbx);
}


void RegExpMacroAssemblerX64::Bind(Label* label) {
  __ bind(label);
}


void RegExpMacroAssemblerX64::CheckCharacter(uint32_t c, Label* on_equal) {
  __ cmpl(current_character(), Immediate(c));
  BranchOrBacktrack(equal, on_equal);
}


void RegExpMacroAssemblerX64::CheckCharacterGT(uc16 limit, Label* on_greater) {
  __ cmpl(current_character(), Immediate(limit));
  BranchOrBacktrack(greater, on_greater);
}


void RegExpMacroAssemblerX64::CheckAtStart(Label* on_at_start) {
  Label not_at_start;
  // Did we start the match at the start of the string at all?
  __ cmpb(Operand(rbp, kAtStart), Immediate(0));
  BranchOrBacktrack(equal, &not_at_start);
  // If we did, are we still at the start of the input?
  __ lea(rax, Operand(rsi, rdi, times_1, 0));
  __ cmpq(rax, Operand(rbp, kInputStart));
  BranchOrBacktrack(equal, on_at_start);
  __ bind(&not_at_start);
}


void RegExpMacroAssemblerX64::CheckNotAtStart(Label* on_not_at_start) {
  // Did we start the match at the start of the string at all?
  __ cmpb(Operand(rbp, kAtStart), Immediate(0));
  BranchOrBacktrack(equal, on_not_at_start);
  // If we did, are we still at the start of the input?
  __ lea(rax, Operand(rsi, rdi, times_1, 0));
  __ cmpq(rax, Operand(rbp, kInputStart));
  BranchOrBacktrack(not_equal, on_not_at_start);
}


void RegExpMacroAssemblerX64::CheckCharacterLT(uc16 limit, Label* on_less) {
  __ cmpl(current_character(), Immediate(limit));
  BranchOrBacktrack(less, on_less);
}


void RegExpMacroAssemblerX64::CheckCharacters(Vector<const uc16> str,
                                              int cp_offset,
                                              Label* on_failure,
                                              bool check_end_of_string) {
  int byte_length = str.length() * char_size();
  int byte_offset = cp_offset * char_size();
  if (check_end_of_string) {
    // Check that there are at least str.length() characters left in the input.
    __ cmpl(rdi, Immediate(-(byte_offset + byte_length)));
    BranchOrBacktrack(greater, on_failure);
  }

  if (on_failure == NULL) {
    // Instead of inlining a backtrack, (re)use the global backtrack target.
    on_failure = &backtrack_label_;
  }

  // TODO(lrn): Test multiple characters at a time by loading 4 or 8 bytes
  // at a time.
  for (int i = 0; i < str.length(); i++) {
    if (mode_ == ASCII) {
      __ cmpb(Operand(rsi, rdi, times_1, byte_offset + i),
              Immediate(static_cast<int8_t>(str[i])));
    } else {
      ASSERT(mode_ == UC16);
      __ cmpw(Operand(rsi, rdi, times_1, byte_offset + i * sizeof(uc16)),
              Immediate(str[i]));
    }
    BranchOrBacktrack(not_equal, on_failure);
  }
}


void RegExpMacroAssemblerX64::CheckGreedyLoop(Label* on_equal) {
  Label fallthrough;
  __ cmpl(rdi, Operand(backtrack_stackpointer(), 0));
  __ j(not_equal, &fallthrough);
  Drop();
  BranchOrBacktrack(no_condition, on_equal);
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerX64::CheckNotBackReferenceIgnoreCase(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  __ movq(rdx, register_location(start_reg));  // Offset of start of capture
  __ movq(rbx, register_location(start_reg + 1));  // Offset of end of capture
  __ subq(rbx, rdx);  // Length of capture.

  // -----------------------
  // rdx  = Start offset of capture.
  // rbx = Length of capture

  // If length is negative, this code will fail (it's a symptom of a partial or
  // illegal capture where start of capture after end of capture).
  // This must not happen (no back-reference can reference a capture that wasn't
  // closed before in the reg-exp, and we must not generate code that can cause
  // this condition).

  // If length is zero, either the capture is empty or it is nonparticipating.
  // In either case succeed immediately.
  __ j(equal, &fallthrough);

  if (mode_ == ASCII) {
    Label loop_increment;
    if (on_no_match == NULL) {
      on_no_match = &backtrack_label_;
    }

    __ lea(r9, Operand(rsi, rdx, times_1, 0));
    __ lea(r11, Operand(rsi, rdi, times_1, 0));
    __ addq(rbx, r9);  // End of capture
    // ---------------------
    // r11 - current input character address
    // r9 - current capture character address
    // rbx - end of capture

    Label loop;
    __ bind(&loop);
    __ movzxbl(rdx, Operand(r9, 0));
    __ movzxbl(rax, Operand(r11, 0));
    // al - input character
    // dl - capture character
    __ cmpb(rax, rdx);
    __ j(equal, &loop_increment);

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    // I.e., if or-ing with 0x20 makes values equal and in range 'a'-'z', it's
    // a match.
    __ or_(rax, Immediate(0x20));  // Convert match character to lower-case.
    __ or_(rdx, Immediate(0x20));  // Convert capture character to lower-case.
    __ cmpb(rax, rdx);
    __ j(not_equal, on_no_match);  // Definitely not equal.
    __ subb(rax, Immediate('a'));
    __ cmpb(rax, Immediate('z' - 'a'));
    __ j(above, on_no_match);  // Weren't letters anyway.

    __ bind(&loop_increment);
    // Increment pointers into match and capture strings.
    __ addq(r11, Immediate(1));
    __ addq(r9, Immediate(1));
    // Compare to end of capture, and loop if not done.
    __ cmpq(r9, rbx);
    __ j(below, &loop);

    // Compute new value of character position after the matched part.
    __ movq(rdi, r11);
    __ subq(rdi, rsi);
  } else {
    ASSERT(mode_ == UC16);
    // Save important/volatile registers before calling C function.
#ifndef _WIN64
    // Caller save on Linux and callee save in Windows.
    __ push(rsi);
    __ push(rdi);
#endif
    __ push(backtrack_stackpointer());

    static const int num_arguments = 3;
    __ PrepareCallCFunction(num_arguments);

    // Put arguments into parameter registers. Parameters are
    //   Address byte_offset1 - Address captured substring's start.
    //   Address byte_offset2 - Address of current character position.
    //   size_t byte_length - length of capture in bytes(!)
#ifdef _WIN64
    // Compute and set byte_offset1 (start of capture).
    __ lea(rcx, Operand(rsi, rdx, times_1, 0));
    // Set byte_offset2.
    __ lea(rdx, Operand(rsi, rdi, times_1, 0));
    // Set byte_length.
    __ movq(r8, rbx);
#else  // AMD64 calling convention
    // Compute byte_offset2 (current position = rsi+rdi).
    __ lea(rax, Operand(rsi, rdi, times_1, 0));
    // Compute and set byte_offset1 (start of capture).
    __ lea(rdi, Operand(rsi, rdx, times_1, 0));
    // Set byte_offset2.
    __ movq(rsi, rax);
    // Set byte_length.
    __ movq(rdx, rbx);
#endif
    ExternalReference compare =
        ExternalReference::re_case_insensitive_compare_uc16();
    __ CallCFunction(compare, num_arguments);

    // Restore original values before reacting on result value.
    __ Move(code_object_pointer(), masm_->CodeObject());
    __ pop(backtrack_stackpointer());
#ifndef _WIN64
    __ pop(rdi);
    __ pop(rsi);
#endif

    // Check if function returned non-zero for success or zero for failure.
    __ testq(rax, rax);
    BranchOrBacktrack(zero, on_no_match);
    // On success, increment position by length of capture.
    // Requires that rbx is callee save (true for both Win64 and AMD64 ABIs).
    __ addq(rdi, rbx);
  }
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerX64::CheckNotBackReference(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;

  // Find length of back-referenced capture.
  __ movq(rdx, register_location(start_reg));
  __ movq(rax, register_location(start_reg + 1));
  __ subq(rax, rdx);  // Length to check.

  // Fail on partial or illegal capture (start of capture after end of capture).
  // This must not happen (no back-reference can reference a capture that wasn't
  // closed before in the reg-exp).
  __ Check(greater_equal, "Invalid capture referenced");

  // Succeed on empty capture (including non-participating capture)
  __ j(equal, &fallthrough);

  // -----------------------
  // rdx - Start of capture
  // rax - length of capture

  // Check that there are sufficient characters left in the input.
  __ movl(rbx, rdi);
  __ addl(rbx, rax);
  BranchOrBacktrack(greater, on_no_match);

  // Compute pointers to match string and capture string
  __ lea(rbx, Operand(rsi, rdi, times_1, 0));  // Start of match.
  __ addq(rdx, rsi);  // Start of capture.
  __ lea(r9, Operand(rdx, rax, times_1, 0));  // End of capture

  // -----------------------
  // rbx - current capture character address.
  // rbx - current input character address .
  // r9 - end of input to match (capture length after rbx).

  Label loop;
  __ bind(&loop);
  if (mode_ == ASCII) {
    __ movzxbl(rax, Operand(rdx, 0));
    __ cmpb(rax, Operand(rbx, 0));
  } else {
    ASSERT(mode_ == UC16);
    __ movzxwl(rax, Operand(rdx, 0));
    __ cmpw(rax, Operand(rbx, 0));
  }
  BranchOrBacktrack(not_equal, on_no_match);
  // Increment pointers into capture and match string.
  __ addq(rbx, Immediate(char_size()));
  __ addq(rdx, Immediate(char_size()));
  // Check if we have reached end of match area.
  __ cmpq(rdx, r9);
  __ j(below, &loop);

  // Success.
  // Set current character position to position after match.
  __ movq(rdi, rbx);
  __ subq(rdi, rsi);

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerX64::CheckNotRegistersEqual(int reg1,
                                                     int reg2,
                                                     Label* on_not_equal) {
  __ movq(rax, register_location(reg1));
  __ cmpq(rax, register_location(reg2));
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerX64::CheckNotCharacter(uint32_t c,
                                                Label* on_not_equal) {
  __ cmpl(current_character(), Immediate(c));
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerX64::CheckCharacterAfterAnd(uint32_t c,
                                                     uint32_t mask,
                                                     Label* on_equal) {
  __ movl(rax, current_character());
  __ and_(rax, Immediate(mask));
  __ cmpl(rax, Immediate(c));
  BranchOrBacktrack(equal, on_equal);
}


void RegExpMacroAssemblerX64::CheckNotCharacterAfterAnd(uint32_t c,
                                                        uint32_t mask,
                                                        Label* on_not_equal) {
  __ movl(rax, current_character());
  __ and_(rax, Immediate(mask));
  __ cmpl(rax, Immediate(c));
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerX64::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  ASSERT(minus < String::kMaxUC16CharCode);
  __ lea(rax, Operand(current_character(), -minus));
  __ and_(rax, Immediate(mask));
  __ cmpl(rax, Immediate(c));
  BranchOrBacktrack(not_equal, on_not_equal);
}


bool RegExpMacroAssemblerX64::CheckSpecialCharacterClass(uc16 type,
                                                         Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check, using the sequence:
  //   lea(rax, Operand(current_character(), -min)) or sub(rax, Immediate(min))
  //   cmp(rax, Immediate(max - min))
  switch (type) {
  case 's':
    // Match space-characters
    if (mode_ == ASCII) {
      // ASCII space characters are '\t'..'\r' and ' '.
      Label success;
      __ cmpl(current_character(), Immediate(' '));
      __ j(equal, &success);
      // Check range 0x09..0x0d
      __ lea(rax, Operand(current_character(), -'\t'));
      __ cmpl(rax, Immediate('\r' - '\t'));
      BranchOrBacktrack(above, on_no_match);
      __ bind(&success);
      return true;
    }
    return false;
  case 'S':
    // Match non-space characters.
    if (mode_ == ASCII) {
      // ASCII space characters are '\t'..'\r' and ' '.
      __ cmpl(current_character(), Immediate(' '));
      BranchOrBacktrack(equal, on_no_match);
      __ lea(rax, Operand(current_character(), -'\t'));
      __ cmpl(rax, Immediate('\r' - '\t'));
      BranchOrBacktrack(below_equal, on_no_match);
      return true;
    }
    return false;
  case 'd':
    // Match ASCII digits ('0'..'9')
    __ lea(rax, Operand(current_character(), -'0'));
    __ cmpl(rax, Immediate('9' - '0'));
    BranchOrBacktrack(above, on_no_match);
    return true;
  case 'D':
    // Match non ASCII-digits
    __ lea(rax, Operand(current_character(), -'0'));
    __ cmpl(rax, Immediate('9' - '0'));
    BranchOrBacktrack(below_equal, on_no_match);
    return true;
  case '.': {
    // Match non-newlines (not 0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029)
    __ movl(rax, current_character());
    __ xor_(rax, Immediate(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ subl(rax, Immediate(0x0b));
    __ cmpl(rax, Immediate(0x0c - 0x0b));
    BranchOrBacktrack(below_equal, on_no_match);
    if (mode_ == UC16) {
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ subl(rax, Immediate(0x2028 - 0x0b));
      __ cmpl(rax, Immediate(0x2029 - 0x2028));
      BranchOrBacktrack(below_equal, on_no_match);
    }
    return true;
  }
  case 'n': {
    // Match newlines (0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029)
    __ movl(rax, current_character());
    __ xor_(rax, Immediate(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ subl(rax, Immediate(0x0b));
    __ cmpl(rax, Immediate(0x0c - 0x0b));
    if (mode_ == ASCII) {
      BranchOrBacktrack(above, on_no_match);
    } else {
      Label done;
      BranchOrBacktrack(below_equal, &done);
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ subl(rax, Immediate(0x2028 - 0x0b));
      __ cmpl(rax, Immediate(0x2029 - 0x2028));
      BranchOrBacktrack(above, on_no_match);
      __ bind(&done);
    }
    return true;
  }
  case 'w': {
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      __ cmpl(current_character(), Immediate('z'));
      BranchOrBacktrack(above, on_no_match);
    }
    __ movq(rbx, ExternalReference::re_word_character_map());
    ASSERT_EQ(0, word_character_map[0]);  // Character '\0' is not a word char.
    ExternalReference word_map = ExternalReference::re_word_character_map();
    __ testb(Operand(rbx, current_character(), times_1, 0),
             current_character());
    BranchOrBacktrack(zero, on_no_match);
    return true;
  }
  case 'W': {
    Label done;
    if (mode_ != ASCII) {
      // Table is 128 entries, so all ASCII characters can be tested.
      __ cmpl(current_character(), Immediate('z'));
      __ j(above, &done);
    }
    __ movq(rbx, ExternalReference::re_word_character_map());
    ASSERT_EQ(0, word_character_map[0]);  // Character '\0' is not a word char.
    ExternalReference word_map = ExternalReference::re_word_character_map();
    __ testb(Operand(rbx, current_character(), times_1, 0),
             current_character());
    BranchOrBacktrack(not_zero, on_no_match);
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


void RegExpMacroAssemblerX64::Fail() {
  ASSERT(FAILURE == 0);  // Return value for failure is zero.
  __ xor_(rax, rax);  // zero rax.
  __ jmp(&exit_label_);
}


Handle<Object> RegExpMacroAssemblerX64::GetCode(Handle<String> source) {
  // Finalize code - write the entry point code now we know how many
  // registers we need.
  // Entry code:
  __ bind(&entry_label_);
  // Start new stack frame.
  __ push(rbp);
  __ movq(rbp, rsp);
  // Save parameters and callee-save registers. Order here should correspond
  //  to order of kBackup_ebx etc.
#ifdef _WIN64
  // MSVC passes arguments in rcx, rdx, r8, r9, with backing stack slots.
  // Store register parameters in pre-allocated stack slots,
  __ movq(Operand(rbp, kInputString), rcx);
  __ movq(Operand(rbp, kStartIndex), rdx);  // Passed as int32 in edx.
  __ movq(Operand(rbp, kInputStart), r8);
  __ movq(Operand(rbp, kInputEnd), r9);
  // Callee-save on Win64.
  __ push(rsi);
  __ push(rdi);
  __ push(rbx);
#else
  // GCC passes arguments in rdi, rsi, rdx, rcx, r8, r9 (and then on stack).
  // Push register parameters on stack for reference.
  ASSERT_EQ(kInputString, -1 * kPointerSize);
  ASSERT_EQ(kStartIndex, -2 * kPointerSize);
  ASSERT_EQ(kInputStart, -3 * kPointerSize);
  ASSERT_EQ(kInputEnd, -4 * kPointerSize);
  ASSERT_EQ(kRegisterOutput, -5 * kPointerSize);
  ASSERT_EQ(kStackHighEnd, -6 * kPointerSize);
  __ push(rdi);
  __ push(rsi);
  __ push(rdx);
  __ push(rcx);
  __ push(r8);
  __ push(r9);

  __ push(rbx);  // Callee-save
#endif

  __ push(Immediate(0));  // Make room for "input start - 1" constant.
  __ push(Immediate(0));  // Make room for "at start" constant.

  // Check if we have space on the stack for registers.
  Label stack_limit_hit;
  Label stack_ok;

  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit();
  __ movq(rcx, rsp);
  __ movq(kScratchRegister, stack_limit);
  __ subq(rcx, Operand(kScratchRegister, 0));
  // Handle it if the stack pointer is already below the stack limit.
  __ j(below_equal, &stack_limit_hit);
  // Check if there is room for the variable number of registers above
  // the stack limit.
  __ cmpq(rcx, Immediate(num_registers_ * kPointerSize));
  __ j(above_equal, &stack_ok);
  // Exit with OutOfMemory exception. There is not enough space on the stack
  // for our working registers.
  __ movq(rax, Immediate(EXCEPTION));
  __ jmp(&exit_label_);

  __ bind(&stack_limit_hit);
  __ Move(code_object_pointer(), masm_->CodeObject());
  CallCheckStackGuardState();  // Preserves no registers beside rbp and rsp.
  __ testq(rax, rax);
  // If returned value is non-zero, we exit with the returned value as result.
  __ j(not_zero, &exit_label_);

  __ bind(&stack_ok);

  // Allocate space on stack for registers.
  __ subq(rsp, Immediate(num_registers_ * kPointerSize));
  // Load string length.
  __ movq(rsi, Operand(rbp, kInputEnd));
  // Load input position.
  __ movq(rdi, Operand(rbp, kInputStart));
  // Set up rdi to be negative offset from string end.
  __ subq(rdi, rsi);
  // Set rax to address of char before start of input
  // (effectively string position -1).
  __ lea(rax, Operand(rdi, -char_size()));
  // Store this value in a local variable, for use when clearing
  // position registers.
  __ movq(Operand(rbp, kInputStartMinusOne), rax);

  // Determine whether the start index is zero, that is at the start of the
  // string, and store that value in a local variable.
  __ movq(rbx, Operand(rbp, kStartIndex));
  __ xor_(rcx, rcx);  // setcc only operates on cl (lower byte of rcx).
  __ testq(rbx, rbx);
  __ setcc(zero, rcx);  // 1 if 0 (start of string), 0 if positive.
  __ movq(Operand(rbp, kAtStart), rcx);

  if (num_saved_registers_ > 0) {
    // Fill saved registers with initial value = start offset - 1
    // Fill in stack push order, to avoid accessing across an unwritten
    // page (a problem on Windows).
    __ movq(rcx, Immediate(kRegisterZero));
    Label init_loop;
    __ bind(&init_loop);
    __ movq(Operand(rbp, rcx, times_1, 0), rax);
    __ subq(rcx, Immediate(kPointerSize));
    __ cmpq(rcx,
            Immediate(kRegisterZero - num_saved_registers_ * kPointerSize));
    __ j(greater, &init_loop);
  }
  // Ensure that we have written to each stack page, in order. Skipping a page
  // on Windows can cause segmentation faults. Assuming page size is 4k.
  const int kPageSize = 4096;
  const int kRegistersPerPage = kPageSize / kPointerSize;
  for (int i = num_saved_registers_ + kRegistersPerPage - 1;
      i < num_registers_;
      i += kRegistersPerPage) {
    __ movq(register_location(i), rax);  // One write every page.
  }

  // Initialize backtrack stack pointer.
  __ movq(backtrack_stackpointer(), Operand(rbp, kStackHighEnd));
  // Initialize code object pointer.
  __ Move(code_object_pointer(), masm_->CodeObject());
  // Load previous char as initial value of current-character.
  Label at_start;
  __ cmpb(Operand(rbp, kAtStart), Immediate(0));
  __ j(not_equal, &at_start);
  LoadCurrentCharacterUnchecked(-1, 1);  // Load previous char.
  __ jmp(&start_label_);
  __ bind(&at_start);
  __ movq(current_character(), Immediate('\n'));
  __ jmp(&start_label_);


  // Exit code:
  if (success_label_.is_linked()) {
    // Save captures when successful.
    __ bind(&success_label_);
    if (num_saved_registers_ > 0) {
      // copy captures to output
      __ movq(rbx, Operand(rbp, kRegisterOutput));
      __ movq(rcx, Operand(rbp, kInputEnd));
      __ subq(rcx, Operand(rbp, kInputStart));
      for (int i = 0; i < num_saved_registers_; i++) {
        __ movq(rax, register_location(i));
        __ addq(rax, rcx);  // Convert to index from start, not end.
        if (mode_ == UC16) {
          __ sar(rax, Immediate(1));  // Convert byte index to character index.
        }
        __ movl(Operand(rbx, i * kIntSize), rax);
      }
    }
    __ movq(rax, Immediate(SUCCESS));
  }

  // Exit and return rax
  __ bind(&exit_label_);

#ifdef _WIN64
  // Restore callee save registers.
  __ lea(rsp, Operand(rbp, kLastCalleeSaveRegister));
  __ pop(rbx);
  __ pop(rdi);
  __ pop(rsi);
  // Stack now at rbp.
#else
  // Restore callee save register.
  __ movq(rbx, Operand(rbp, kBackup_rbx));
  // Skip rsp to rbp.
  __ movq(rsp, rbp);
#endif
  // Exit function frame, restore previous one.
  __ pop(rbp);
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
    __ push(rdi);

    CallCheckStackGuardState();
    __ testq(rax, rax);
    // If returning non-zero, we should end execution with the given
    // result as return value.
    __ j(not_zero, &exit_label_);

    // Restore registers.
    __ Move(code_object_pointer(), masm_->CodeObject());
    __ pop(rdi);
    __ pop(backtrack_stackpointer());
    // String might have moved: Reload esi from frame.
    __ movq(rsi, Operand(rbp, kInputEnd));
    SafeReturn();
  }

  // Backtrack stack overflow code.
  if (stack_overflow_label_.is_linked()) {
    SafeCallTarget(&stack_overflow_label_);
    // Reached if the backtrack-stack limit has been hit.

    Label grow_failed;
    // Save registers before calling C function
#ifndef _WIN64
    // Callee-save in Microsoft 64-bit ABI, but not in AMD64 ABI.
    __ push(rsi);
    __ push(rdi);
#endif

    // Call GrowStack(backtrack_stackpointer())
    static const int num_arguments = 2;
    __ PrepareCallCFunction(num_arguments);
#ifdef _WIN64
    // Microsoft passes parameters in rcx, rdx.
    // First argument, backtrack stackpointer, is already in rcx.
    __ lea(rdx, Operand(rbp, kStackHighEnd));  // Second argument
#else
    // AMD64 ABI passes parameters in rdi, rsi.
    __ movq(rdi, backtrack_stackpointer());   // First argument.
    __ lea(rsi, Operand(rbp, kStackHighEnd));  // Second argument.
#endif
    ExternalReference grow_stack = ExternalReference::re_grow_stack();
    __ CallCFunction(grow_stack, num_arguments);
    // If return NULL, we have failed to grow the stack, and
    // must exit with a stack-overflow exception.
    __ testq(rax, rax);
    __ j(equal, &exit_with_exception);
    // Otherwise use return value as new stack pointer.
    __ movq(backtrack_stackpointer(), rax);
    // Restore saved registers and continue.
    __ Move(code_object_pointer(), masm_->CodeObject());
#ifndef _WIN64
    __ pop(rdi);
    __ pop(rsi);
#endif
    SafeReturn();
  }

  if (exit_with_exception.is_linked()) {
    // If any of the code above needed to exit with an exception.
    __ bind(&exit_with_exception);
    // Exit with Result EXCEPTION(-1) to signal thrown exception.
    __ movq(rax, Immediate(EXCEPTION));
    __ jmp(&exit_label_);
  }

  FixupCodeRelativePositions();

  CodeDesc code_desc;
  masm_->GetCode(&code_desc);
  Handle<Code> code = Factory::NewCode(code_desc,
                                       NULL,
                                       Code::ComputeFlags(Code::REGEXP),
                                       masm_->CodeObject());
  LOG(RegExpCodeCreateEvent(*code, *source));
  return Handle<Object>::cast(code);
}


void RegExpMacroAssemblerX64::GoTo(Label* to) {
  BranchOrBacktrack(no_condition, to);
}


void RegExpMacroAssemblerX64::IfRegisterGE(int reg,
                                           int comparand,
                                           Label* if_ge) {
  __ cmpq(register_location(reg), Immediate(comparand));
  BranchOrBacktrack(greater_equal, if_ge);
}


void RegExpMacroAssemblerX64::IfRegisterLT(int reg,
                                           int comparand,
                                           Label* if_lt) {
  __ cmpq(register_location(reg), Immediate(comparand));
  BranchOrBacktrack(less, if_lt);
}


void RegExpMacroAssemblerX64::IfRegisterEqPos(int reg,
                                              Label* if_eq) {
  __ cmpq(rdi, register_location(reg));
  BranchOrBacktrack(equal, if_eq);
}


RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerX64::Implementation() {
  return kX64Implementation;
}


void RegExpMacroAssemblerX64::LoadCurrentCharacter(int cp_offset,
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


void RegExpMacroAssemblerX64::PopCurrentPosition() {
  Pop(rdi);
}


void RegExpMacroAssemblerX64::PopRegister(int register_index) {
  Pop(rax);
  __ movq(register_location(register_index), rax);
}


void RegExpMacroAssemblerX64::PushBacktrack(Label* label) {
  Push(label);
  CheckStackLimit();
}


void RegExpMacroAssemblerX64::PushCurrentPosition() {
  Push(rdi);
}


void RegExpMacroAssemblerX64::PushRegister(int register_index,
                                           StackCheckFlag check_stack_limit) {
  __ movq(rax, register_location(register_index));
  Push(rax);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerX64::ReadCurrentPositionFromRegister(int reg) {
  __ movq(rdi, register_location(reg));
}


void RegExpMacroAssemblerX64::ReadStackPointerFromRegister(int reg) {
  __ movq(backtrack_stackpointer(), register_location(reg));
  __ addq(backtrack_stackpointer(), Operand(rbp, kStackHighEnd));
}


void RegExpMacroAssemblerX64::SetRegister(int register_index, int to) {
  ASSERT(register_index >= num_saved_registers_);  // Reserved for positions!
  __ movq(register_location(register_index), Immediate(to));
}


void RegExpMacroAssemblerX64::Succeed() {
  __ jmp(&success_label_);
}


void RegExpMacroAssemblerX64::WriteCurrentPositionToRegister(int reg,
                                                             int cp_offset) {
  if (cp_offset == 0) {
    __ movq(register_location(reg), rdi);
  } else {
    __ lea(rax, Operand(rdi, cp_offset * char_size()));
    __ movq(register_location(reg), rax);
  }
}


void RegExpMacroAssemblerX64::ClearRegisters(int reg_from, int reg_to) {
  ASSERT(reg_from <= reg_to);
  __ movq(rax, Operand(rbp, kInputStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ movq(register_location(reg), rax);
  }
}


void RegExpMacroAssemblerX64::WriteStackPointerToRegister(int reg) {
  __ movq(rax, backtrack_stackpointer());
  __ subq(rax, Operand(rbp, kStackHighEnd));
  __ movq(register_location(reg), rax);
}


// Private methods:

void RegExpMacroAssemblerX64::CallCheckStackGuardState() {
  // This function call preserves no register values. Caller should
  // store anything volatile in a C call or overwritten by this function.
  static const int num_arguments = 3;
  __ PrepareCallCFunction(num_arguments);
#ifdef _WIN64
  // Second argument: Code* of self. (Do this before overwriting r8).
  __ movq(rdx, code_object_pointer());
  // Third argument: RegExp code frame pointer.
  __ movq(r8, rbp);
  // First argument: Next address on the stack (will be address of
  // return address).
  __ lea(rcx, Operand(rsp, -kPointerSize));
#else
  // Third argument: RegExp code frame pointer.
  __ movq(rdx, rbp);
  // Second argument: Code* of self.
  __ movq(rsi, code_object_pointer());
  // First argument: Next address on the stack (will be address of
  // return address).
  __ lea(rdi, Operand(rsp, -kPointerSize));
#endif
  ExternalReference stack_check =
      ExternalReference::re_check_stack_guard_state();
  __ CallCFunction(stack_check, num_arguments);
}


// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  return reinterpret_cast<T&>(Memory::int32_at(re_frame + frame_offset));
}


int RegExpMacroAssemblerX64::CheckStackGuardState(Address* return_address,
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
    intptr_t delta = *code_handle - re_code;
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
    int byte_length = static_cast<int>(end_address - start_address);
    frame_entry<const String*>(re_frame, kInputString) = *subject;
    frame_entry<const byte*>(re_frame, kInputStart) = new_address;
    frame_entry<const byte*>(re_frame, kInputEnd) = new_address + byte_length;
  }

  return 0;
}


Operand RegExpMacroAssemblerX64::register_location(int register_index) {
  ASSERT(register_index < (1<<30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return Operand(rbp, kRegisterZero - register_index * kPointerSize);
}


void RegExpMacroAssemblerX64::CheckPosition(int cp_offset,
                                            Label* on_outside_input) {
  __ cmpl(rdi, Immediate(-cp_offset * char_size()));
  BranchOrBacktrack(greater_equal, on_outside_input);
}


void RegExpMacroAssemblerX64::BranchOrBacktrack(Condition condition,
                                                Label* to) {
  if (condition < 0) {  // No condition
    if (to == NULL) {
      Backtrack();
      return;
    }
    __ jmp(to);
    return;
  }
  if (to == NULL) {
    __ j(condition, &backtrack_label_);
    return;
  }
  __ j(condition, to);
}


void RegExpMacroAssemblerX64::SafeCall(Label* to) {
  __ call(to);
}


void RegExpMacroAssemblerX64::SafeCallTarget(Label* label) {
  __ bind(label);
  __ subq(Operand(rsp, 0), code_object_pointer());
}


void RegExpMacroAssemblerX64::SafeReturn() {
  __ addq(Operand(rsp, 0), code_object_pointer());
  __ ret(0);
}


void RegExpMacroAssemblerX64::Push(Register source) {
  ASSERT(!source.is(backtrack_stackpointer()));
  // Notice: This updates flags, unlike normal Push.
  __ subq(backtrack_stackpointer(), Immediate(kIntSize));
  __ movl(Operand(backtrack_stackpointer(), 0), source);
}


void RegExpMacroAssemblerX64::Push(Immediate value) {
  // Notice: This updates flags, unlike normal Push.
  __ subq(backtrack_stackpointer(), Immediate(kIntSize));
  __ movl(Operand(backtrack_stackpointer(), 0), value);
}


void RegExpMacroAssemblerX64::FixupCodeRelativePositions() {
  for (int i = 0, n = code_relative_fixup_positions_.length(); i < n; i++) {
    int position = code_relative_fixup_positions_[i];
    // The position succeeds a relative label offset from position.
    // Patch the relative offset to be relative to the Code object pointer
    // instead.
    int patch_position = position - kIntSize;
    int offset = masm_->long_at(patch_position);
    masm_->long_at_put(patch_position,
                       offset
                       + position
                       + Code::kHeaderSize
                       - kHeapObjectTag);
  }
  code_relative_fixup_positions_.Clear();
}


void RegExpMacroAssemblerX64::Push(Label* backtrack_target) {
  __ subq(backtrack_stackpointer(), Immediate(kIntSize));
  __ movl(Operand(backtrack_stackpointer(), 0), backtrack_target);
  MarkPositionForCodeRelativeFixup();
}


void RegExpMacroAssemblerX64::Pop(Register target) {
  ASSERT(!target.is(backtrack_stackpointer()));
  __ movsxlq(target, Operand(backtrack_stackpointer(), 0));
  // Notice: This updates flags, unlike normal Pop.
  __ addq(backtrack_stackpointer(), Immediate(kIntSize));
}


void RegExpMacroAssemblerX64::Drop() {
  __ addq(backtrack_stackpointer(), Immediate(kIntSize));
}


void RegExpMacroAssemblerX64::CheckPreemption() {
  // Check for preemption.
  Label no_preempt;
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit();
  __ load_rax(stack_limit);
  __ cmpq(rsp, rax);
  __ j(above, &no_preempt);

  SafeCall(&check_preempt_label_);

  __ bind(&no_preempt);
}


void RegExpMacroAssemblerX64::CheckStackLimit() {
  Label no_stack_overflow;
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit();
  __ load_rax(stack_limit);
  __ cmpq(backtrack_stackpointer(), rax);
  __ j(above, &no_stack_overflow);

  SafeCall(&stack_overflow_label_);

  __ bind(&no_stack_overflow);
}


void RegExpMacroAssemblerX64::LoadCurrentCharacterUnchecked(int cp_offset,
                                                            int characters) {
  if (mode_ == ASCII) {
    if (characters == 4) {
      __ movl(current_character(), Operand(rsi, rdi, times_1, cp_offset));
    } else if (characters == 2) {
      __ movzxwl(current_character(), Operand(rsi, rdi, times_1, cp_offset));
    } else {
      ASSERT(characters == 1);
      __ movzxbl(current_character(), Operand(rsi, rdi, times_1, cp_offset));
    }
  } else {
    ASSERT(mode_ == UC16);
    if (characters == 2) {
      __ movl(current_character(),
              Operand(rsi, rdi, times_1, cp_offset * sizeof(uc16)));
    } else {
      ASSERT(characters == 1);
      __ movzxwl(current_character(),
                 Operand(rsi, rdi, times_1, cp_offset * sizeof(uc16)));
    }
  }
}

#undef __

#endif  // V8_NATIVE_REGEXP

}}  // namespace v8::internal

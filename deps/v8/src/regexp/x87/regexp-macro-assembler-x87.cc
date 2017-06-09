// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X87

#include "src/regexp/x87/regexp-macro-assembler-x87.h"

#include "src/log.h"
#include "src/macro-assembler.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"
#include "src/unicode.h"

namespace v8 {
namespace internal {

#ifndef V8_INTERPRETED_REGEXP
/*
 * This assembler uses the following register assignment convention
 * - edx : Current character.  Must be loaded using LoadCurrentCharacter
 *         before using any of the dispatch methods.  Temporarily stores the
 *         index of capture start after a matching pass for a global regexp.
 * - edi : Current position in input, as negative offset from end of string.
 *         Please notice that this is the byte offset, not the character offset!
 * - esi : end of input (points to byte after last character in input).
 * - ebp : Frame pointer.  Used to access arguments, local variables and
 *         RegExp registers.
 * - esp : Points to tip of C stack.
 * - ecx : Points to tip of backtrack stack
 *
 * The registers eax and ebx are free to use for computations.
 *
 * Each call to a public method should retain this convention.
 * The stack will have the following structure:
 *       - Isolate* isolate     (address of the current isolate)
 *       - direct_call          (if 1, direct call from JavaScript code, if 0
 *                               call through the runtime system)
 *       - stack_area_base      (high end of the memory area to use as
 *                               backtracking stack)
 *       - capture array size   (may fit multiple sets of matches)
 *       - int* capture_array   (int[num_saved_registers_], for output).
 *       - end of input         (address of end of string)
 *       - start of input       (address of first character in string)
 *       - start index          (character index of start)
 *       - String* input_string (location of a handle containing the string)
 *       --- frame alignment (if applicable) ---
 *       - return address
 * ebp-> - old ebp
 *       - backup of caller esi
 *       - backup of caller edi
 *       - backup of caller ebx
 *       - success counter      (only for global regexps to count matches).
 *       - Offset of location before start of input (effectively character
 *         string start - 1). Used to initialize capture registers to a
 *         non-position.
 *       - register 0  ebp[-4]  (only positions must be stored in the first
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
 *              int num_capture_registers,
 *              byte* stack_area_base,
 *              bool direct_call = false,
 *              Isolate* isolate);
 */

#define __ ACCESS_MASM(masm_)

RegExpMacroAssemblerX87::RegExpMacroAssemblerX87(Isolate* isolate, Zone* zone,
                                                 Mode mode,
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
  DCHECK_EQ(0, registers_to_save % 2);
  __ jmp(&entry_label_);   // We'll write the entry code later.
  __ bind(&start_label_);  // And then continue from here.
}


RegExpMacroAssemblerX87::~RegExpMacroAssemblerX87() {
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


int RegExpMacroAssemblerX87::stack_limit_slack()  {
  return RegExpStack::kStackLimitSlack;
}


void RegExpMacroAssemblerX87::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    __ add(edi, Immediate(by * char_size()));
  }
}


void RegExpMacroAssemblerX87::AdvanceRegister(int reg, int by) {
  DCHECK(reg >= 0);
  DCHECK(reg < num_registers_);
  if (by != 0) {
    __ add(register_location(reg), Immediate(by));
  }
}


void RegExpMacroAssemblerX87::Backtrack() {
  CheckPreemption();
  // Pop Code* offset from backtrack stack, add Code* and jump to location.
  Pop(ebx);
  __ add(ebx, Immediate(masm_->CodeObject()));
  __ jmp(ebx);
}


void RegExpMacroAssemblerX87::Bind(Label* label) {
  __ bind(label);
}


void RegExpMacroAssemblerX87::CheckCharacter(uint32_t c, Label* on_equal) {
  __ cmp(current_character(), c);
  BranchOrBacktrack(equal, on_equal);
}


void RegExpMacroAssemblerX87::CheckCharacterGT(uc16 limit, Label* on_greater) {
  __ cmp(current_character(), limit);
  BranchOrBacktrack(greater, on_greater);
}


void RegExpMacroAssemblerX87::CheckAtStart(Label* on_at_start) {
  __ lea(eax, Operand(edi, -char_size()));
  __ cmp(eax, Operand(ebp, kStringStartMinusOne));
  BranchOrBacktrack(equal, on_at_start);
}


void RegExpMacroAssemblerX87::CheckNotAtStart(int cp_offset,
                                              Label* on_not_at_start) {
  __ lea(eax, Operand(edi, -char_size() + cp_offset * char_size()));
  __ cmp(eax, Operand(ebp, kStringStartMinusOne));
  BranchOrBacktrack(not_equal, on_not_at_start);
}


void RegExpMacroAssemblerX87::CheckCharacterLT(uc16 limit, Label* on_less) {
  __ cmp(current_character(), limit);
  BranchOrBacktrack(less, on_less);
}


void RegExpMacroAssemblerX87::CheckGreedyLoop(Label* on_equal) {
  Label fallthrough;
  __ cmp(edi, Operand(backtrack_stackpointer(), 0));
  __ j(not_equal, &fallthrough);
  __ add(backtrack_stackpointer(), Immediate(kPointerSize));  // Pop.
  BranchOrBacktrack(no_condition, on_equal);
  __ bind(&fallthrough);
}

void RegExpMacroAssemblerX87::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, bool unicode, Label* on_no_match) {
  Label fallthrough;
  __ mov(edx, register_location(start_reg));  // Index of start of capture
  __ mov(ebx, register_location(start_reg + 1));  // Index of end of capture
  __ sub(ebx, edx);  // Length of capture.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ j(equal, &fallthrough);

  // Check that there are sufficient characters left in the input.
  if (read_backward) {
    __ mov(eax, Operand(ebp, kStringStartMinusOne));
    __ add(eax, ebx);
    __ cmp(edi, eax);
    BranchOrBacktrack(less_equal, on_no_match);
  } else {
    __ mov(eax, edi);
    __ add(eax, ebx);
    BranchOrBacktrack(greater, on_no_match);
  }

  if (mode_ == LATIN1) {
    Label success;
    Label fail;
    Label loop_increment;
    // Save register contents to make the registers available below.
    __ push(edi);
    __ push(backtrack_stackpointer());
    // After this, the eax, ecx, and edi registers are available.

    __ add(edx, esi);  // Start of capture
    __ add(edi, esi);  // Start of text to match against capture.
    if (read_backward) {
      __ sub(edi, ebx);  // Offset by length when matching backwards.
    }
    __ add(ebx, edi);  // End of text to match against capture.

    Label loop;
    __ bind(&loop);
    __ movzx_b(eax, Operand(edi, 0));
    __ cmpb_al(Operand(edx, 0));
    __ j(equal, &loop_increment);

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ or_(eax, 0x20);  // Convert match character to lower-case.
    __ lea(ecx, Operand(eax, -'a'));
    __ cmp(ecx, static_cast<int32_t>('z' - 'a'));  // Is eax a lowercase letter?
    Label convert_capture;
    __ j(below_equal, &convert_capture);  // In range 'a'-'z'.
    // Latin-1: Check for values in range [224,254] but not 247.
    __ sub(ecx, Immediate(224 - 'a'));
    __ cmp(ecx, Immediate(254 - 224));
    __ j(above, &fail);  // Weren't Latin-1 letters.
    __ cmp(ecx, Immediate(247 - 224));  // Check for 247.
    __ j(equal, &fail);
    __ bind(&convert_capture);
    // Also convert capture character.
    __ movzx_b(ecx, Operand(edx, 0));
    __ or_(ecx, 0x20);

    __ cmp(eax, ecx);
    __ j(not_equal, &fail);

    __ bind(&loop_increment);
    // Increment pointers into match and capture strings.
    __ add(edx, Immediate(1));
    __ add(edi, Immediate(1));
    // Compare to end of match, and loop if not done.
    __ cmp(edi, ebx);
    __ j(below, &loop);
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
    __ add(esp, Immediate(kPointerSize));
    // Compute new value of character position after the matched part.
    __ sub(edi, esi);
    if (read_backward) {
      // Subtract match length if we matched backward.
      __ add(edi, register_location(start_reg));
      __ sub(edi, register_location(start_reg + 1));
    }
  } else {
    DCHECK(mode_ == UC16);
    // Save registers before calling C function.
    __ push(esi);
    __ push(edi);
    __ push(backtrack_stackpointer());
    __ push(ebx);

    static const int argument_count = 4;
    __ PrepareCallCFunction(argument_count, ecx);
    // Put arguments into allocated stack area, last argument highest on stack.
    // Parameters are
    //   Address byte_offset1 - Address captured substring's start.
    //   Address byte_offset2 - Address of current character position.
    //   size_t byte_length - length of capture in bytes(!)
//   Isolate* isolate or 0 if unicode flag.

    // Set isolate.
#ifdef V8_INTL_SUPPORT
    if (unicode) {
      __ mov(Operand(esp, 3 * kPointerSize), Immediate(0));
    } else  // NOLINT
#endif      // V8_INTL_SUPPORT
    {
      __ mov(Operand(esp, 3 * kPointerSize),
             Immediate(ExternalReference::isolate_address(isolate())));
    }
    // Set byte_length.
    __ mov(Operand(esp, 2 * kPointerSize), ebx);
    // Set byte_offset2.
    // Found by adding negative string-end offset of current position (edi)
    // to end of string.
    __ add(edi, esi);
    if (read_backward) {
      __ sub(edi, ebx);  // Offset by length when matching backwards.
    }
    __ mov(Operand(esp, 1 * kPointerSize), edi);
    // Set byte_offset1.
    // Start of capture, where edx already holds string-end negative offset.
    __ add(edx, esi);
    __ mov(Operand(esp, 0 * kPointerSize), edx);

    {
      AllowExternalCallThatCantCauseGC scope(masm_);
      ExternalReference compare =
          ExternalReference::re_case_insensitive_compare_uc16(isolate());
      __ CallCFunction(compare, argument_count);
    }
    // Pop original values before reacting on result value.
    __ pop(ebx);
    __ pop(backtrack_stackpointer());
    __ pop(edi);
    __ pop(esi);

    // Check if function returned non-zero for success or zero for failure.
    __ or_(eax, eax);
    BranchOrBacktrack(zero, on_no_match);
    // On success, advance position by length of capture.
    if (read_backward) {
      __ sub(edi, ebx);
    } else {
      __ add(edi, ebx);
    }
  }
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerX87::CheckNotBackReference(int start_reg,
                                                    bool read_backward,
                                                    Label* on_no_match) {
  Label fallthrough;
  Label success;
  Label fail;

  // Find length of back-referenced capture.
  __ mov(edx, register_location(start_reg));
  __ mov(eax, register_location(start_reg + 1));
  __ sub(eax, edx);  // Length to check.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ j(equal, &fallthrough);

  // Check that there are sufficient characters left in the input.
  if (read_backward) {
    __ mov(ebx, Operand(ebp, kStringStartMinusOne));
    __ add(ebx, eax);
    __ cmp(edi, ebx);
    BranchOrBacktrack(less_equal, on_no_match);
  } else {
    __ mov(ebx, edi);
    __ add(ebx, eax);
    BranchOrBacktrack(greater, on_no_match);
  }

  // Save register to make it available below.
  __ push(backtrack_stackpointer());

  // Compute pointers to match string and capture string
  __ add(edx, esi);  // Start of capture.
  __ lea(ebx, Operand(esi, edi, times_1, 0));  // Start of match.
  if (read_backward) {
    __ sub(ebx, eax);  // Offset by length when matching backwards.
  }
  __ lea(ecx, Operand(eax, ebx, times_1, 0));  // End of match

  Label loop;
  __ bind(&loop);
  if (mode_ == LATIN1) {
    __ movzx_b(eax, Operand(edx, 0));
    __ cmpb_al(Operand(ebx, 0));
  } else {
    DCHECK(mode_ == UC16);
    __ movzx_w(eax, Operand(edx, 0));
    __ cmpw_ax(Operand(ebx, 0));
  }
  __ j(not_equal, &fail);
  // Increment pointers into capture and match string.
  __ add(edx, Immediate(char_size()));
  __ add(ebx, Immediate(char_size()));
  // Check if we have reached end of match area.
  __ cmp(ebx, ecx);
  __ j(below, &loop);
  __ jmp(&success);

  __ bind(&fail);
  // Restore backtrack stackpointer.
  __ pop(backtrack_stackpointer());
  BranchOrBacktrack(no_condition, on_no_match);

  __ bind(&success);
  // Move current character position to position after match.
  __ mov(edi, ecx);
  __ sub(edi, esi);
  if (read_backward) {
    // Subtract match length if we matched backward.
    __ add(edi, register_location(start_reg));
    __ sub(edi, register_location(start_reg + 1));
  }
  // Restore backtrack stackpointer.
  __ pop(backtrack_stackpointer());

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerX87::CheckNotCharacter(uint32_t c,
                                                 Label* on_not_equal) {
  __ cmp(current_character(), c);
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerX87::CheckCharacterAfterAnd(uint32_t c,
                                                      uint32_t mask,
                                                      Label* on_equal) {
  if (c == 0) {
    __ test(current_character(), Immediate(mask));
  } else {
    __ mov(eax, mask);
    __ and_(eax, current_character());
    __ cmp(eax, c);
  }
  BranchOrBacktrack(equal, on_equal);
}


void RegExpMacroAssemblerX87::CheckNotCharacterAfterAnd(uint32_t c,
                                                         uint32_t mask,
                                                         Label* on_not_equal) {
  if (c == 0) {
    __ test(current_character(), Immediate(mask));
  } else {
    __ mov(eax, mask);
    __ and_(eax, current_character());
    __ cmp(eax, c);
  }
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerX87::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  DCHECK(minus < String::kMaxUtf16CodeUnit);
  __ lea(eax, Operand(current_character(), -minus));
  if (c == 0) {
    __ test(eax, Immediate(mask));
  } else {
    __ and_(eax, mask);
    __ cmp(eax, c);
  }
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerX87::CheckCharacterInRange(
    uc16 from,
    uc16 to,
    Label* on_in_range) {
  __ lea(eax, Operand(current_character(), -from));
  __ cmp(eax, to - from);
  BranchOrBacktrack(below_equal, on_in_range);
}


void RegExpMacroAssemblerX87::CheckCharacterNotInRange(
    uc16 from,
    uc16 to,
    Label* on_not_in_range) {
  __ lea(eax, Operand(current_character(), -from));
  __ cmp(eax, to - from);
  BranchOrBacktrack(above, on_not_in_range);
}


void RegExpMacroAssemblerX87::CheckBitInTable(
    Handle<ByteArray> table,
    Label* on_bit_set) {
  __ mov(eax, Immediate(table));
  Register index = current_character();
  if (mode_ != LATIN1 || kTableMask != String::kMaxOneByteCharCode) {
    __ mov(ebx, kTableSize - 1);
    __ and_(ebx, current_character());
    index = ebx;
  }
  __ cmpb(FieldOperand(eax, index, times_1, ByteArray::kHeaderSize),
          Immediate(0));
  BranchOrBacktrack(not_equal, on_bit_set);
}


bool RegExpMacroAssemblerX87::CheckSpecialCharacterClass(uc16 type,
                                                          Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check
  switch (type) {
  case 's':
    // Match space-characters
    if (mode_ == LATIN1) {
      // One byte space characters are '\t'..'\r', ' ' and \u00a0.
      Label success;
      __ cmp(current_character(), ' ');
      __ j(equal, &success, Label::kNear);
      // Check range 0x09..0x0d
      __ lea(eax, Operand(current_character(), -'\t'));
      __ cmp(eax, '\r' - '\t');
      __ j(below_equal, &success, Label::kNear);
      // \u00a0 (NBSP).
      __ cmp(eax, 0x00a0 - '\t');
      BranchOrBacktrack(not_equal, on_no_match);
      __ bind(&success);
      return true;
    }
    return false;
  case 'S':
    // The emitted code for generic character classes is good enough.
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
    __ mov(eax, current_character());
    __ xor_(eax, Immediate(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ sub(eax, Immediate(0x0b));
    __ cmp(eax, 0x0c - 0x0b);
    BranchOrBacktrack(below_equal, on_no_match);
    if (mode_ == UC16) {
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ sub(eax, Immediate(0x2028 - 0x0b));
      __ cmp(eax, 0x2029 - 0x2028);
      BranchOrBacktrack(below_equal, on_no_match);
    }
    return true;
  }
  case 'w': {
    if (mode_ != LATIN1) {
      // Table is 256 entries, so all Latin1 characters can be tested.
      __ cmp(current_character(), Immediate('z'));
      BranchOrBacktrack(above, on_no_match);
    }
    DCHECK_EQ(0, word_character_map[0]);  // Character '\0' is not a word char.
    ExternalReference word_map = ExternalReference::re_word_character_map();
    __ test_b(current_character(),
              Operand::StaticArray(current_character(), times_1, word_map));
    BranchOrBacktrack(zero, on_no_match);
    return true;
  }
  case 'W': {
    Label done;
    if (mode_ != LATIN1) {
      // Table is 256 entries, so all Latin1 characters can be tested.
      __ cmp(current_character(), Immediate('z'));
      __ j(above, &done);
    }
    DCHECK_EQ(0, word_character_map[0]);  // Character '\0' is not a word char.
    ExternalReference word_map = ExternalReference::re_word_character_map();
    __ test_b(current_character(),
              Operand::StaticArray(current_character(), times_1, word_map));
    BranchOrBacktrack(not_zero, on_no_match);
    if (mode_ != LATIN1) {
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
    __ mov(eax, current_character());
    __ xor_(eax, Immediate(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c
    __ sub(eax, Immediate(0x0b));
    __ cmp(eax, 0x0c - 0x0b);
    if (mode_ == LATIN1) {
      BranchOrBacktrack(above, on_no_match);
    } else {
      Label done;
      BranchOrBacktrack(below_equal, &done);
      DCHECK_EQ(UC16, mode_);
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ sub(eax, Immediate(0x2028 - 0x0b));
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


void RegExpMacroAssemblerX87::Fail() {
  STATIC_ASSERT(FAILURE == 0);  // Return value for failure is zero.
  if (!global()) {
    __ Move(eax, Immediate(FAILURE));
  }
  __ jmp(&exit_label_);
}


Handle<HeapObject> RegExpMacroAssemblerX87::GetCode(Handle<String> source) {
  Label return_eax;
  // Finalize code - write the entry point code now we know how many
  // registers we need.

  // Entry code:
  __ bind(&entry_label_);

  // Tell the system that we have a stack frame.  Because the type is MANUAL, no
  // code is generated.
  FrameScope scope(masm_, StackFrame::MANUAL);

  // Actually emit code to start a new stack frame.
  __ push(ebp);
  __ mov(ebp, esp);
  // Save callee-save registers. Order here should correspond to order of
  // kBackup_ebx etc.
  __ push(esi);
  __ push(edi);
  __ push(ebx);  // Callee-save on MacOS.
  __ push(Immediate(0));  // Number of successful matches in a global regexp.
  __ push(Immediate(0));  // Make room for "string start - 1" constant.

  // Check if we have space on the stack for registers.
  Label stack_limit_hit;
  Label stack_ok;

  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ mov(ecx, esp);
  __ sub(ecx, Operand::StaticVariable(stack_limit));
  // Handle it if the stack pointer is already below the stack limit.
  __ j(below_equal, &stack_limit_hit);
  // Check if there is room for the variable number of registers above
  // the stack limit.
  __ cmp(ecx, num_registers_ * kPointerSize);
  __ j(above_equal, &stack_ok);
  // Exit with OutOfMemory exception. There is not enough space on the stack
  // for our working registers.
  __ mov(eax, EXCEPTION);
  __ jmp(&return_eax);

  __ bind(&stack_limit_hit);
  CallCheckStackGuardState(ebx);
  __ or_(eax, eax);
  // If returned value is non-zero, we exit with the returned value as result.
  __ j(not_zero, &return_eax);

  __ bind(&stack_ok);
  // Load start index for later use.
  __ mov(ebx, Operand(ebp, kStartIndex));

  // Allocate space on stack for registers.
  __ sub(esp, Immediate(num_registers_ * kPointerSize));
  // Load string length.
  __ mov(esi, Operand(ebp, kInputEnd));
  // Load input position.
  __ mov(edi, Operand(ebp, kInputStart));
  // Set up edi to be negative offset from string end.
  __ sub(edi, esi);

  // Set eax to address of char before start of the string.
  // (effectively string position -1).
  __ neg(ebx);
  if (mode_ == UC16) {
    __ lea(eax, Operand(edi, ebx, times_2, -char_size()));
  } else {
    __ lea(eax, Operand(edi, ebx, times_1, -char_size()));
  }
  // Store this value in a local variable, for use when clearing
  // position registers.
  __ mov(Operand(ebp, kStringStartMinusOne), eax);

#if V8_OS_WIN
  // Ensure that we write to each stack page, in order. Skipping a page
  // on Windows can cause segmentation faults. Assuming page size is 4k.
  const int kPageSize = 4096;
  const int kRegistersPerPage = kPageSize / kPointerSize;
  for (int i = num_saved_registers_ + kRegistersPerPage - 1;
      i < num_registers_;
      i += kRegistersPerPage) {
    __ mov(register_location(i), eax);  // One write every page.
  }
#endif  // V8_OS_WIN

  Label load_char_start_regexp, start_regexp;
  // Load newline if index is at start, previous character otherwise.
  __ cmp(Operand(ebp, kStartIndex), Immediate(0));
  __ j(not_equal, &load_char_start_regexp, Label::kNear);
  __ mov(current_character(), '\n');
  __ jmp(&start_regexp, Label::kNear);

  // Global regexp restarts matching here.
  __ bind(&load_char_start_regexp);
  // Load previous char as initial value of current character register.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&start_regexp);

  // Initialize on-stack registers.
  if (num_saved_registers_ > 0) {  // Always is, if generated from a regexp.
    // Fill saved registers with initial value = start offset - 1
    // Fill in stack push order, to avoid accessing across an unwritten
    // page (a problem on Windows).
    if (num_saved_registers_ > 8) {
      __ mov(ecx, kRegisterZero);
      Label init_loop;
      __ bind(&init_loop);
      __ mov(Operand(ebp, ecx, times_1, 0), eax);
      __ sub(ecx, Immediate(kPointerSize));
      __ cmp(ecx, kRegisterZero - num_saved_registers_ * kPointerSize);
      __ j(greater, &init_loop);
    } else {  // Unroll the loop.
      for (int i = 0; i < num_saved_registers_; i++) {
        __ mov(register_location(i), eax);
      }
    }
  }

  // Initialize backtrack stack pointer.
  __ mov(backtrack_stackpointer(), Operand(ebp, kStackHighEnd));

  __ jmp(&start_label_);

  // Exit code:
  if (success_label_.is_linked()) {
    // Save captures when successful.
    __ bind(&success_label_);
    if (num_saved_registers_ > 0) {
      // copy captures to output
      __ mov(ebx, Operand(ebp, kRegisterOutput));
      __ mov(ecx, Operand(ebp, kInputEnd));
      __ mov(edx, Operand(ebp, kStartIndex));
      __ sub(ecx, Operand(ebp, kInputStart));
      if (mode_ == UC16) {
        __ lea(ecx, Operand(ecx, edx, times_2, 0));
      } else {
        __ add(ecx, edx);
      }
      for (int i = 0; i < num_saved_registers_; i++) {
        __ mov(eax, register_location(i));
        if (i == 0 && global_with_zero_length_check()) {
          // Keep capture start in edx for the zero-length check later.
          __ mov(edx, eax);
        }
        // Convert to index from start of string, not end.
        __ add(eax, ecx);
        if (mode_ == UC16) {
          __ sar(eax, 1);  // Convert byte index to character index.
        }
        __ mov(Operand(ebx, i * kPointerSize), eax);
      }
    }

    if (global()) {
      // Restart matching if the regular expression is flagged as global.
      // Increment success counter.
      __ inc(Operand(ebp, kSuccessfulCaptures));
      // Capture results have been stored, so the number of remaining global
      // output registers is reduced by the number of stored captures.
      __ mov(ecx, Operand(ebp, kNumOutputRegisters));
      __ sub(ecx, Immediate(num_saved_registers_));
      // Check whether we have enough room for another set of capture results.
      __ cmp(ecx, Immediate(num_saved_registers_));
      __ j(less, &exit_label_);

      __ mov(Operand(ebp, kNumOutputRegisters), ecx);
      // Advance the location for output.
      __ add(Operand(ebp, kRegisterOutput),
             Immediate(num_saved_registers_ * kPointerSize));

      // Prepare eax to initialize registers with its value in the next run.
      __ mov(eax, Operand(ebp, kStringStartMinusOne));

      if (global_with_zero_length_check()) {
        // Special case for zero-length matches.
        // edx: capture start index
        __ cmp(edi, edx);
        // Not a zero-length match, restart.
        __ j(not_equal, &load_char_start_regexp);
        // edi (offset from the end) is zero if we already reached the end.
        __ test(edi, edi);
        __ j(zero, &exit_label_, Label::kNear);
        // Advance current position after a zero-length match.
        Label advance;
        __ bind(&advance);
        if (mode_ == UC16) {
          __ add(edi, Immediate(2));
        } else {
          __ inc(edi);
        }
        if (global_unicode()) CheckNotInSurrogatePair(0, &advance);
      }
      __ jmp(&load_char_start_regexp);
    } else {
      __ mov(eax, Immediate(SUCCESS));
    }
  }

  __ bind(&exit_label_);
  if (global()) {
    // Return the number of successful captures.
    __ mov(eax, Operand(ebp, kSuccessfulCaptures));
  }

  __ bind(&return_eax);
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
    __ or_(eax, eax);
    // If returning non-zero, we should end execution with the given
    // result as return value.
    __ j(not_zero, &return_eax);

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
    static const int num_arguments = 3;
    __ PrepareCallCFunction(num_arguments, ebx);
    __ mov(Operand(esp, 2 * kPointerSize),
           Immediate(ExternalReference::isolate_address(isolate())));
    __ lea(eax, Operand(ebp, kStackHighEnd));
    __ mov(Operand(esp, 1 * kPointerSize), eax);
    __ mov(Operand(esp, 0 * kPointerSize), backtrack_stackpointer());
    ExternalReference grow_stack =
        ExternalReference::re_grow_stack(isolate());
    __ CallCFunction(grow_stack, num_arguments);
    // If return NULL, we have failed to grow the stack, and
    // must exit with a stack-overflow exception.
    __ or_(eax, eax);
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
    __ jmp(&return_eax);
  }

  CodeDesc code_desc;
  masm_->GetCode(&code_desc);
  Handle<Code> code =
      isolate()->factory()->NewCode(code_desc,
                                    Code::ComputeFlags(Code::REGEXP),
                                    masm_->CodeObject());
  PROFILE(masm_->isolate(),
          RegExpCodeCreateEvent(AbstractCode::cast(*code), *source));
  return Handle<HeapObject>::cast(code);
}


void RegExpMacroAssemblerX87::GoTo(Label* to) {
  BranchOrBacktrack(no_condition, to);
}


void RegExpMacroAssemblerX87::IfRegisterGE(int reg,
                                            int comparand,
                                            Label* if_ge) {
  __ cmp(register_location(reg), Immediate(comparand));
  BranchOrBacktrack(greater_equal, if_ge);
}


void RegExpMacroAssemblerX87::IfRegisterLT(int reg,
                                            int comparand,
                                            Label* if_lt) {
  __ cmp(register_location(reg), Immediate(comparand));
  BranchOrBacktrack(less, if_lt);
}


void RegExpMacroAssemblerX87::IfRegisterEqPos(int reg,
                                               Label* if_eq) {
  __ cmp(edi, register_location(reg));
  BranchOrBacktrack(equal, if_eq);
}


RegExpMacroAssembler::IrregexpImplementation
    RegExpMacroAssemblerX87::Implementation() {
  return kX87Implementation;
}


void RegExpMacroAssemblerX87::LoadCurrentCharacter(int cp_offset,
                                                    Label* on_end_of_input,
                                                    bool check_bounds,
                                                    int characters) {
  DCHECK(cp_offset < (1<<30));  // Be sane! (And ensure negation works)
  if (check_bounds) {
    if (cp_offset >= 0) {
      CheckPosition(cp_offset + characters - 1, on_end_of_input);
    } else {
      CheckPosition(cp_offset, on_end_of_input);
    }
  }
  LoadCurrentCharacterUnchecked(cp_offset, characters);
}


void RegExpMacroAssemblerX87::PopCurrentPosition() {
  Pop(edi);
}


void RegExpMacroAssemblerX87::PopRegister(int register_index) {
  Pop(eax);
  __ mov(register_location(register_index), eax);
}


void RegExpMacroAssemblerX87::PushBacktrack(Label* label) {
  Push(Immediate::CodeRelativeOffset(label));
  CheckStackLimit();
}


void RegExpMacroAssemblerX87::PushCurrentPosition() {
  Push(edi);
}


void RegExpMacroAssemblerX87::PushRegister(int register_index,
                                            StackCheckFlag check_stack_limit) {
  __ mov(eax, register_location(register_index));
  Push(eax);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerX87::ReadCurrentPositionFromRegister(int reg) {
  __ mov(edi, register_location(reg));
}


void RegExpMacroAssemblerX87::ReadStackPointerFromRegister(int reg) {
  __ mov(backtrack_stackpointer(), register_location(reg));
  __ add(backtrack_stackpointer(), Operand(ebp, kStackHighEnd));
}

void RegExpMacroAssemblerX87::SetCurrentPositionFromEnd(int by)  {
  Label after_position;
  __ cmp(edi, -by * char_size());
  __ j(greater_equal, &after_position, Label::kNear);
  __ mov(edi, -by * char_size());
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&after_position);
}


void RegExpMacroAssemblerX87::SetRegister(int register_index, int to) {
  DCHECK(register_index >= num_saved_registers_);  // Reserved for positions!
  __ mov(register_location(register_index), Immediate(to));
}


bool RegExpMacroAssemblerX87::Succeed() {
  __ jmp(&success_label_);
  return global();
}


void RegExpMacroAssemblerX87::WriteCurrentPositionToRegister(int reg,
                                                              int cp_offset) {
  if (cp_offset == 0) {
    __ mov(register_location(reg), edi);
  } else {
    __ lea(eax, Operand(edi, cp_offset * char_size()));
    __ mov(register_location(reg), eax);
  }
}


void RegExpMacroAssemblerX87::ClearRegisters(int reg_from, int reg_to) {
  DCHECK(reg_from <= reg_to);
  __ mov(eax, Operand(ebp, kStringStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ mov(register_location(reg), eax);
  }
}


void RegExpMacroAssemblerX87::WriteStackPointerToRegister(int reg) {
  __ mov(eax, backtrack_stackpointer());
  __ sub(eax, Operand(ebp, kStackHighEnd));
  __ mov(register_location(reg), eax);
}


// Private methods:

void RegExpMacroAssemblerX87::CallCheckStackGuardState(Register scratch) {
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
      ExternalReference::re_check_stack_guard_state(isolate());
  __ CallCFunction(check_stack_guard, num_arguments);
}


// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  return reinterpret_cast<T&>(Memory::int32_at(re_frame + frame_offset));
}


template <typename T>
static T* frame_entry_address(Address re_frame, int frame_offset) {
  return reinterpret_cast<T*>(re_frame + frame_offset);
}


int RegExpMacroAssemblerX87::CheckStackGuardState(Address* return_address,
                                                   Code* re_code,
                                                   Address re_frame) {
  return NativeRegExpMacroAssembler::CheckStackGuardState(
      frame_entry<Isolate*>(re_frame, kIsolate),
      frame_entry<int>(re_frame, kStartIndex),
      frame_entry<int>(re_frame, kDirectCall) == 1, return_address, re_code,
      frame_entry_address<String*>(re_frame, kInputString),
      frame_entry_address<const byte*>(re_frame, kInputStart),
      frame_entry_address<const byte*>(re_frame, kInputEnd));
}


Operand RegExpMacroAssemblerX87::register_location(int register_index) {
  DCHECK(register_index < (1<<30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return Operand(ebp, kRegisterZero - register_index * kPointerSize);
}


void RegExpMacroAssemblerX87::CheckPosition(int cp_offset,
                                             Label* on_outside_input) {
  if (cp_offset >= 0) {
    __ cmp(edi, -cp_offset * char_size());
    BranchOrBacktrack(greater_equal, on_outside_input);
  } else {
    __ lea(eax, Operand(edi, cp_offset * char_size()));
    __ cmp(eax, Operand(ebp, kStringStartMinusOne));
    BranchOrBacktrack(less_equal, on_outside_input);
  }
}


void RegExpMacroAssemblerX87::BranchOrBacktrack(Condition condition,
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


void RegExpMacroAssemblerX87::SafeCall(Label* to) {
  Label return_to;
  __ push(Immediate::CodeRelativeOffset(&return_to));
  __ jmp(to);
  __ bind(&return_to);
}


void RegExpMacroAssemblerX87::SafeReturn() {
  __ pop(ebx);
  __ add(ebx, Immediate(masm_->CodeObject()));
  __ jmp(ebx);
}


void RegExpMacroAssemblerX87::SafeCallTarget(Label* name) {
  __ bind(name);
}


void RegExpMacroAssemblerX87::Push(Register source) {
  DCHECK(!source.is(backtrack_stackpointer()));
  // Notice: This updates flags, unlike normal Push.
  __ sub(backtrack_stackpointer(), Immediate(kPointerSize));
  __ mov(Operand(backtrack_stackpointer(), 0), source);
}


void RegExpMacroAssemblerX87::Push(Immediate value) {
  // Notice: This updates flags, unlike normal Push.
  __ sub(backtrack_stackpointer(), Immediate(kPointerSize));
  __ mov(Operand(backtrack_stackpointer(), 0), value);
}


void RegExpMacroAssemblerX87::Pop(Register target) {
  DCHECK(!target.is(backtrack_stackpointer()));
  __ mov(target, Operand(backtrack_stackpointer(), 0));
  // Notice: This updates flags, unlike normal Pop.
  __ add(backtrack_stackpointer(), Immediate(kPointerSize));
}


void RegExpMacroAssemblerX87::CheckPreemption() {
  // Check for preemption.
  Label no_preempt;
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ cmp(esp, Operand::StaticVariable(stack_limit));
  __ j(above, &no_preempt);

  SafeCall(&check_preempt_label_);

  __ bind(&no_preempt);
}


void RegExpMacroAssemblerX87::CheckStackLimit() {
  Label no_stack_overflow;
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit(isolate());
  __ cmp(backtrack_stackpointer(), Operand::StaticVariable(stack_limit));
  __ j(above, &no_stack_overflow);

  SafeCall(&stack_overflow_label_);

  __ bind(&no_stack_overflow);
}


void RegExpMacroAssemblerX87::LoadCurrentCharacterUnchecked(int cp_offset,
                                                             int characters) {
  if (mode_ == LATIN1) {
    if (characters == 4) {
      __ mov(current_character(), Operand(esi, edi, times_1, cp_offset));
    } else if (characters == 2) {
      __ movzx_w(current_character(), Operand(esi, edi, times_1, cp_offset));
    } else {
      DCHECK(characters == 1);
      __ movzx_b(current_character(), Operand(esi, edi, times_1, cp_offset));
    }
  } else {
    DCHECK(mode_ == UC16);
    if (characters == 2) {
      __ mov(current_character(),
             Operand(esi, edi, times_1, cp_offset * sizeof(uc16)));
    } else {
      DCHECK(characters == 1);
      __ movzx_w(current_character(),
                 Operand(esi, edi, times_1, cp_offset * sizeof(uc16)));
    }
  }
}


#undef __

#endif  // V8_INTERPRETED_REGEXP

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87

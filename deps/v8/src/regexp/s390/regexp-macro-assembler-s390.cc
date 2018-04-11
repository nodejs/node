// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_S390

#include "src/assembler-inl.h"
#include "src/base/bits.h"
#include "src/code-stubs.h"
#include "src/log.h"
#include "src/macro-assembler.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"
#include "src/regexp/s390/regexp-macro-assembler-s390.h"
#include "src/unicode.h"

namespace v8 {
namespace internal {

#ifndef V8_INTERPRETED_REGEXP
/*
 * This assembler uses the following register assignment convention
 * - r6: Temporarily stores the index of capture start after a matching pass
 *        for a global regexp.
 * - r7: Pointer to current code object (Code*) including heap object tag.
 * - r8: Current position in input, as negative offset from end of string.
 *        Please notice that this is the byte offset, not the character offset!
 * - r9: Currently loaded character. Must be loaded using
 *        LoadCurrentCharacter before using any of the dispatch methods.
 * - r13: Points to tip of backtrack stack
 * - r10: End of input (points to byte after last character in input).
 * - r11: Frame pointer. Used to access arguments, local variables and
 *         RegExp registers.
 * - r12: IP register, used by assembler. Very volatile.
 * - r15/sp : Points to tip of C stack.
 *
 * The remaining registers are free for computations.
 * Each call to a public method should retain this convention.
 *
 * The stack will have the following structure:
 *  - fp[108] Isolate* isolate   (address of the current isolate)
 *  - fp[104] direct_call        (if 1, direct call from JavaScript code,
 *                                if 0, call through the runtime system).
 *  - fp[100] stack_area_base    (high end of the memory area to use as
 *                                backtracking stack).
 *  - fp[96]  capture array size (may fit multiple sets of matches)
 *  - fp[0..96] zLinux ABI register saving area
 *  --- sp when called ---
 *  --- frame pointer ----
 *  - fp[-4]  direct_call        (if 1, direct call from JavaScript code,
 *                                if 0, call through the runtime system).
 *  - fp[-8]  stack_area_base    (high end of the memory area to use as
 *                                backtracking stack).
 *  - fp[-12] capture array size (may fit multiple sets of matches)
 *  - fp[-16] int* capture_array (int[num_saved_registers_], for output).
 *  - fp[-20] end of input       (address of end of string).
 *  - fp[-24] start of input     (address of first character in string).
 *  - fp[-28] start index        (character index of start).
 *  - fp[-32] void* input_string (location of a handle containing the string).
 *  - fp[-36] success counter    (only for global regexps to count matches).
 *  - fp[-40] Offset of location before start of input (effectively character
 *            string start - 1). Used to initialize capture registers to a
 *            non-position.
 *  - fp[-44] At start (if 1, we are starting at the start of the
 *    string, otherwise 0)
 *  - fp[-48] register 0         (Only positions must be stored in the first
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
 *              int* capture_output_array,
 *              int num_capture_registers,
 *              byte* stack_area_base,
 *              bool direct_call = false,
 *              Isolate* isolate);
 * The call is performed by NativeRegExpMacroAssembler::Execute()
 * (in regexp-macro-assembler.cc) via the GeneratedCode wrapper.
 */

#define __ ACCESS_MASM(masm_)

RegExpMacroAssemblerS390::RegExpMacroAssemblerS390(Isolate* isolate, Zone* zone,
                                                   Mode mode,
                                                   int registers_to_save)
    : NativeRegExpMacroAssembler(isolate, zone),
      masm_(new MacroAssembler(isolate, nullptr, kRegExpCodeSize,
                               CodeObjectRequired::kYes)),
      mode_(mode),
      num_registers_(registers_to_save),
      num_saved_registers_(registers_to_save),
      entry_label_(),
      start_label_(),
      success_label_(),
      backtrack_label_(),
      exit_label_(),
      internal_failure_label_() {
  DCHECK_EQ(0, registers_to_save % 2);

  __ b(&entry_label_);  // We'll write the entry code later.
  // If the code gets too big or corrupted, an internal exception will be
  // raised, and we will exit right away.
  __ bind(&internal_failure_label_);
  __ LoadImmP(r2, Operand(FAILURE));
  __ Ret();
  __ bind(&start_label_);  // And then continue from here.
}

RegExpMacroAssemblerS390::~RegExpMacroAssemblerS390() {
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

int RegExpMacroAssemblerS390::stack_limit_slack() {
  return RegExpStack::kStackLimitSlack;
}

void RegExpMacroAssemblerS390::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    __ AddP(current_input_offset(), Operand(by * char_size()));
  }
}

void RegExpMacroAssemblerS390::AdvanceRegister(int reg, int by) {
  DCHECK_LE(0, reg);
  DCHECK_GT(num_registers_, reg);
  if (by != 0) {
    if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT) && is_int8(by)) {
      __ AddMI(register_location(reg), Operand(by));
    } else {
      __ LoadP(r2, register_location(reg), r0);
      __ mov(r0, Operand(by));
      __ AddRR(r2, r0);
      __ StoreP(r2, register_location(reg));
    }
  }
}

void RegExpMacroAssemblerS390::Backtrack() {
  CheckPreemption();
  // Pop Code* offset from backtrack stack, add Code* and jump to location.
  Pop(r2);
  __ AddP(r2, code_pointer());
  __ b(r2);
}

void RegExpMacroAssemblerS390::Bind(Label* label) { __ bind(label); }

void RegExpMacroAssemblerS390::CheckCharacter(uint32_t c, Label* on_equal) {
  __ CmpLogicalP(current_character(), Operand(c));
  BranchOrBacktrack(eq, on_equal);
}

void RegExpMacroAssemblerS390::CheckCharacterGT(uc16 limit, Label* on_greater) {
  __ CmpLogicalP(current_character(), Operand(limit));
  BranchOrBacktrack(gt, on_greater);
}

void RegExpMacroAssemblerS390::CheckAtStart(Label* on_at_start) {
  __ LoadP(r3, MemOperand(frame_pointer(), kStringStartMinusOne));
  __ AddP(r2, current_input_offset(), Operand(-char_size()));
  __ CmpP(r2, r3);
  BranchOrBacktrack(eq, on_at_start);
}

void RegExpMacroAssemblerS390::CheckNotAtStart(int cp_offset,
                                               Label* on_not_at_start) {
  __ LoadP(r3, MemOperand(frame_pointer(), kStringStartMinusOne));
  __ AddP(r2, current_input_offset(),
          Operand(-char_size() + cp_offset * char_size()));
  __ CmpP(r2, r3);
  BranchOrBacktrack(ne, on_not_at_start);
}

void RegExpMacroAssemblerS390::CheckCharacterLT(uc16 limit, Label* on_less) {
  __ CmpLogicalP(current_character(), Operand(limit));
  BranchOrBacktrack(lt, on_less);
}

void RegExpMacroAssemblerS390::CheckGreedyLoop(Label* on_equal) {
  Label backtrack_non_equal;
  __ CmpP(current_input_offset(), MemOperand(backtrack_stackpointer(), 0));
  __ bne(&backtrack_non_equal);
  __ AddP(backtrack_stackpointer(), Operand(kPointerSize));

  BranchOrBacktrack(al, on_equal);
  __ bind(&backtrack_non_equal);
}

void RegExpMacroAssemblerS390::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, bool unicode, Label* on_no_match) {
  Label fallthrough;
  __ LoadP(r2, register_location(start_reg));      // Index of start of
                                                   // capture
  __ LoadP(r3, register_location(start_reg + 1));  // Index of end
  __ SubP(r3, r3, r2);

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ beq(&fallthrough);

  // Check that there are enough characters left in the input.
  if (read_backward) {
    __ LoadP(r5, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ AddP(r5, r5, r3);
    __ CmpP(current_input_offset(), r5);
    BranchOrBacktrack(le, on_no_match);
  } else {
    __ AddP(r0, r3, current_input_offset());
    BranchOrBacktrack(gt, on_no_match);
  }

  if (mode_ == LATIN1) {
    Label success;
    Label fail;
    Label loop_check;

    // r2 - offset of start of capture
    // r3 - length of capture
    __ AddP(r2, end_of_input_address());
    __ AddP(r4, current_input_offset(), end_of_input_address());
    if (read_backward) {
      __ SubP(r4, r4, r3);  // Offset by length when matching backwards.
    }
    __ mov(r1, Operand::Zero());

    // r1 - Loop index
    // r2 - Address of start of capture.
    // r4 - Address of current input position.

    Label loop;
    __ bind(&loop);
    __ LoadlB(r5, MemOperand(r2, r1));
    __ LoadlB(r6, MemOperand(r4, r1));

    __ CmpP(r6, r5);
    __ beq(&loop_check);

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ Or(r5, Operand(0x20));  // Convert capture character to lower-case.
    __ Or(r6, Operand(0x20));  // Also convert input character.
    __ CmpP(r6, r5);
    __ bne(&fail);
    __ SubP(r5, Operand('a'));
    __ CmpLogicalP(r5, Operand('z' - 'a'));  // Is r5 a lowercase letter?
    __ ble(&loop_check);                     // In range 'a'-'z'.
    // Latin-1: Check for values in range [224,254] but not 247.
    __ SubP(r5, Operand(224 - 'a'));
    __ CmpLogicalP(r5, Operand(254 - 224));
    __ bgt(&fail);                           // Weren't Latin-1 letters.
    __ CmpLogicalP(r5, Operand(247 - 224));  // Check for 247.
    __ beq(&fail);

    __ bind(&loop_check);
    __ la(r1, MemOperand(r1, char_size()));
    __ CmpP(r1, r3);
    __ blt(&loop);
    __ b(&success);

    __ bind(&fail);
    BranchOrBacktrack(al, on_no_match);

    __ bind(&success);
    // Compute new value of character position after the matched part.
    __ SubP(current_input_offset(), r4, end_of_input_address());
    if (read_backward) {
      __ LoadP(r2, register_location(start_reg));  // Index of start of capture
      __ LoadP(r3,
               register_location(start_reg + 1));  // Index of end of capture
      __ AddP(current_input_offset(), current_input_offset(), r2);
      __ SubP(current_input_offset(), current_input_offset(), r3);
    }
    __ AddP(current_input_offset(), r1);
  } else {
    DCHECK(mode_ == UC16);
    int argument_count = 4;
    __ PrepareCallCFunction(argument_count, r4);

    // r2 - offset of start of capture
    // r3 - length of capture

    // Put arguments into arguments registers.
    // Parameters are
    //   r2: Address byte_offset1 - Address captured substring's start.
    //   r3: Address byte_offset2 - Address of current character position.
    //   r4: size_t byte_length - length of capture in bytes(!)
    //   r5: Isolate* isolate or 0 if unicode flag.

    // Address of start of capture.
    __ AddP(r2, end_of_input_address());
    // Length of capture.
    __ LoadRR(r4, r3);
    // Save length in callee-save register for use on return.
    __ LoadRR(r6, r3);
    // Address of current input position.
    __ AddP(r3, current_input_offset(), end_of_input_address());
    if (read_backward) {
      __ SubP(r3, r3, r6);
    }
// Isolate.
#ifdef V8_INTL_SUPPORT
    if (unicode) {
      __ LoadImmP(r5, Operand::Zero());
    } else  // NOLINT
#endif      // V8_INTL_SUPPORT
    {
      __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));
    }

    {
      AllowExternalCallThatCantCauseGC scope(masm_);
      ExternalReference function =
          ExternalReference::re_case_insensitive_compare_uc16(isolate());
      __ CallCFunction(function, argument_count);
    }

    // Check if function returned non-zero for success or zero for failure.
    __ CmpP(r2, Operand::Zero());
    BranchOrBacktrack(eq, on_no_match);

    // On success, advance position by length of capture.
    if (read_backward) {
      __ SubP(current_input_offset(), current_input_offset(), r6);
    } else {
      __ AddP(current_input_offset(), current_input_offset(), r6);
    }
  }

  __ bind(&fallthrough);
}

void RegExpMacroAssemblerS390::CheckNotBackReference(int start_reg,
                                                     bool read_backward,
                                                     Label* on_no_match) {
  Label fallthrough;
  Label success;

  // Find length of back-referenced capture.
  __ LoadP(r2, register_location(start_reg));
  __ LoadP(r3, register_location(start_reg + 1));
  __ SubP(r3, r3, r2);  // Length to check.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ beq(&fallthrough);

  // Check that there are enough characters left in the input.
  if (read_backward) {
    __ LoadP(r5, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ AddP(r5, r5, r3);
    __ CmpP(current_input_offset(), r5);
    BranchOrBacktrack(le, on_no_match);
  } else {
    __ AddP(r0, r3, current_input_offset());
    BranchOrBacktrack(gt, on_no_match, cr0);
  }

  // r2 - offset of start of capture
  // r3 - length of capture
  __ la(r2, MemOperand(r2, end_of_input_address()));
  __ la(r4, MemOperand(current_input_offset(), end_of_input_address()));
  if (read_backward) {
    __ SubP(r4, r4, r3);  // Offset by length when matching backwards.
  }
  __ mov(r1, Operand::Zero());

  Label loop;
  __ bind(&loop);
  if (mode_ == LATIN1) {
    __ LoadlB(r5, MemOperand(r2, r1));
    __ LoadlB(r6, MemOperand(r4, r1));
  } else {
    DCHECK(mode_ == UC16);
    __ LoadLogicalHalfWordP(r5, MemOperand(r2, r1));
    __ LoadLogicalHalfWordP(r6, MemOperand(r4, r1));
  }
  __ la(r1, MemOperand(r1, char_size()));
  __ CmpP(r5, r6);
  BranchOrBacktrack(ne, on_no_match);
  __ CmpP(r1, r3);
  __ blt(&loop);

  // Move current character position to position after match.
  __ SubP(current_input_offset(), r4, end_of_input_address());
  if (read_backward) {
    __ LoadP(r2, register_location(start_reg));  // Index of start of capture
    __ LoadP(r3, register_location(start_reg + 1));  // Index of end of capture
    __ AddP(current_input_offset(), current_input_offset(), r2);
    __ SubP(current_input_offset(), current_input_offset(), r3);
  }
  __ AddP(current_input_offset(), r1);

  __ bind(&fallthrough);
}

void RegExpMacroAssemblerS390::CheckNotCharacter(unsigned c,
                                                 Label* on_not_equal) {
  __ CmpLogicalP(current_character(), Operand(c));
  BranchOrBacktrack(ne, on_not_equal);
}

void RegExpMacroAssemblerS390::CheckCharacterAfterAnd(uint32_t c, uint32_t mask,
                                                      Label* on_equal) {
  __ AndP(r2, current_character(), Operand(mask));
  if (c != 0) {
    __ CmpLogicalP(r2, Operand(c));
  }
  BranchOrBacktrack(eq, on_equal);
}

void RegExpMacroAssemblerS390::CheckNotCharacterAfterAnd(unsigned c,
                                                         unsigned mask,
                                                         Label* on_not_equal) {
  __ AndP(r2, current_character(), Operand(mask));
  if (c != 0) {
    __ CmpLogicalP(r2, Operand(c));
  }
  BranchOrBacktrack(ne, on_not_equal);
}

void RegExpMacroAssemblerS390::CheckNotCharacterAfterMinusAnd(
    uc16 c, uc16 minus, uc16 mask, Label* on_not_equal) {
  DCHECK_GT(String::kMaxUtf16CodeUnit, minus);
  __ lay(r2, MemOperand(current_character(), -minus));
  __ And(r2, Operand(mask));
  if (c != 0) {
    __ CmpLogicalP(r2, Operand(c));
  }
  BranchOrBacktrack(ne, on_not_equal);
}

void RegExpMacroAssemblerS390::CheckCharacterInRange(uc16 from, uc16 to,
                                                     Label* on_in_range) {
  __ lay(r2, MemOperand(current_character(), -from));
  __ CmpLogicalP(r2, Operand(to - from));
  BranchOrBacktrack(le, on_in_range);  // Unsigned lower-or-same condition.
}

void RegExpMacroAssemblerS390::CheckCharacterNotInRange(
    uc16 from, uc16 to, Label* on_not_in_range) {
  __ lay(r2, MemOperand(current_character(), -from));
  __ CmpLogicalP(r2, Operand(to - from));
  BranchOrBacktrack(gt, on_not_in_range);  // Unsigned higher condition.
}

void RegExpMacroAssemblerS390::CheckBitInTable(Handle<ByteArray> table,
                                               Label* on_bit_set) {
  __ mov(r2, Operand(table));
  Register index = current_character();
  if (mode_ != LATIN1 || kTableMask != String::kMaxOneByteCharCode) {
    __ AndP(r3, current_character(), Operand(kTableSize - 1));
    index = r3;
  }
  __ LoadlB(r2,
            MemOperand(r2, index, (ByteArray::kHeaderSize - kHeapObjectTag)));
  __ CmpP(r2, Operand::Zero());
  BranchOrBacktrack(ne, on_bit_set);
}

bool RegExpMacroAssemblerS390::CheckSpecialCharacterClass(uc16 type,
                                                          Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check
  switch (type) {
    case 's':
      // Match space-characters
      if (mode_ == LATIN1) {
        // One byte space characters are '\t'..'\r', ' ' and \u00a0.
        Label success;
        __ CmpP(current_character(), Operand(' '));
        __ beq(&success);
        // Check range 0x09..0x0D
        __ SubP(r2, current_character(), Operand('\t'));
        __ CmpLogicalP(r2, Operand('\r' - '\t'));
        __ ble(&success);
        // \u00a0 (NBSP).
        __ CmpLogicalP(r2, Operand(0x00A0 - '\t'));
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
      __ SubP(r2, current_character(), Operand('0'));
      __ CmpLogicalP(r2, Operand('9' - '0'));
      BranchOrBacktrack(gt, on_no_match);
      return true;
    case 'D':
      // Match non ASCII-digits
      __ SubP(r2, current_character(), Operand('0'));
      __ CmpLogicalP(r2, Operand('9' - '0'));
      BranchOrBacktrack(le, on_no_match);
      return true;
    case '.': {
      // Match non-newlines (not 0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029)
      __ XorP(r2, current_character(), Operand(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C
      __ SubP(r2, Operand(0x0B));
      __ CmpLogicalP(r2, Operand(0x0C - 0x0B));
      BranchOrBacktrack(le, on_no_match);
      if (mode_ == UC16) {
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ SubP(r2, Operand(0x2028 - 0x0B));
        __ CmpLogicalP(r2, Operand(1));
        BranchOrBacktrack(le, on_no_match);
      }
      return true;
    }
    case 'n': {
      // Match newlines (0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029)
      __ XorP(r2, current_character(), Operand(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C
      __ SubP(r2, Operand(0x0B));
      __ CmpLogicalP(r2, Operand(0x0C - 0x0B));
      if (mode_ == LATIN1) {
        BranchOrBacktrack(gt, on_no_match);
      } else {
        Label done;
        __ ble(&done);
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ SubP(r2, Operand(0x2028 - 0x0B));
        __ CmpLogicalP(r2, Operand(1));
        BranchOrBacktrack(gt, on_no_match);
        __ bind(&done);
      }
      return true;
    }
    case 'w': {
      if (mode_ != LATIN1) {
        // Table is 1256 entries, so all LATIN1 characters can be tested.
        __ CmpP(current_character(), Operand('z'));
        BranchOrBacktrack(gt, on_no_match);
      }
      ExternalReference map = ExternalReference::re_word_character_map();
      __ mov(r2, Operand(map));
      __ LoadlB(r2, MemOperand(r2, current_character()));
      __ CmpLogicalP(r2, Operand::Zero());
      BranchOrBacktrack(eq, on_no_match);
      return true;
    }
    case 'W': {
      Label done;
      if (mode_ != LATIN1) {
        // Table is 256 entries, so all LATIN characters can be tested.
        __ CmpLogicalP(current_character(), Operand('z'));
        __ bgt(&done);
      }
      ExternalReference map = ExternalReference::re_word_character_map();
      __ mov(r2, Operand(map));
      __ LoadlB(r2, MemOperand(r2, current_character()));
      __ CmpLogicalP(r2, Operand::Zero());
      BranchOrBacktrack(ne, on_no_match);
      if (mode_ != LATIN1) {
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

void RegExpMacroAssemblerS390::Fail() {
  __ LoadImmP(r2, Operand(FAILURE));
  __ b(&exit_label_);
}

Handle<HeapObject> RegExpMacroAssemblerS390::GetCode(Handle<String> source) {
  Label return_r2;

  // Finalize code - write the entry point code now we know how many
  // registers we need.

  // Entry code:
  __ bind(&entry_label_);

  // Tell the system that we have a stack frame.  Because the type
  // is MANUAL, no is generated.
  FrameScope scope(masm_, StackFrame::MANUAL);

  // Ensure register assigments are consistent with callee save mask
  DCHECK(r6.bit() & kRegExpCalleeSaved);
  DCHECK(code_pointer().bit() & kRegExpCalleeSaved);
  DCHECK(current_input_offset().bit() & kRegExpCalleeSaved);
  DCHECK(current_character().bit() & kRegExpCalleeSaved);
  DCHECK(backtrack_stackpointer().bit() & kRegExpCalleeSaved);
  DCHECK(end_of_input_address().bit() & kRegExpCalleeSaved);
  DCHECK(frame_pointer().bit() & kRegExpCalleeSaved);

  // zLinux ABI
  //    Incoming parameters:
  //          r2: input_string
  //          r3: start_index
  //          r4: start addr
  //          r5: end addr
  //          r6: capture output arrray
  //    Requires us to save the callee-preserved registers r6-r13
  //    General convention is to also save r14 (return addr) and
  //    sp/r15 as well in a single STM/STMG
  __ StoreMultipleP(r6, sp, MemOperand(sp, 6 * kPointerSize));

  // Load stack parameters from caller stack frame
  __ LoadMultipleP(r7, r9,
                   MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));
  // r7 = capture array size
  // r8 = stack area base
  // r9 = direct call

  // Actually emit code to start a new stack frame.
  // Push arguments
  // Save callee-save registers.
  // Start new stack frame.
  // Store link register in existing stack-cell.
  // Order here should correspond to order of offset constants in header file.
  //
  // Set frame pointer in space for it if this is not a direct call
  // from generated code.
  __ LoadRR(frame_pointer(), sp);
  __ lay(sp, MemOperand(sp, -10 * kPointerSize));
  __ mov(r1, Operand::Zero());  // success counter
  __ LoadRR(r0, r1);            // offset of location
  __ StoreMultipleP(r0, r9, MemOperand(sp, 0));

  // Check if we have space on the stack for registers.
  Label stack_limit_hit;
  Label stack_ok;

  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ mov(r2, Operand(stack_limit));
  __ LoadP(r2, MemOperand(r2));
  __ SubP(r2, sp, r2);
  // Handle it if the stack pointer is already below the stack limit.
  __ ble(&stack_limit_hit);
  // Check if there is room for the variable number of registers above
  // the stack limit.
  __ CmpLogicalP(r2, Operand(num_registers_ * kPointerSize));
  __ bge(&stack_ok);
  // Exit with OutOfMemory exception. There is not enough space on the stack
  // for our working registers.
  __ mov(r2, Operand(EXCEPTION));
  __ b(&return_r2);

  __ bind(&stack_limit_hit);
  CallCheckStackGuardState(r2);
  __ CmpP(r2, Operand::Zero());
  // If returned value is non-zero, we exit with the returned value as result.
  __ bne(&return_r2);

  __ bind(&stack_ok);

  // Allocate space on stack for registers.
  __ lay(sp, MemOperand(sp, (-num_registers_ * kPointerSize)));
  // Load string end.
  __ LoadP(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
  // Load input start.
  __ LoadP(r4, MemOperand(frame_pointer(), kInputStart));
  // Find negative length (offset of start relative to end).
  __ SubP(current_input_offset(), r4, end_of_input_address());
  __ LoadP(r3, MemOperand(frame_pointer(), kStartIndex));
  // Set r1 to address of char before start of the input string
  // (effectively string position -1).
  __ LoadRR(r1, r4);
  __ SubP(r1, current_input_offset(), Operand(char_size()));
  if (mode_ == UC16) {
    __ ShiftLeftP(r0, r3, Operand(1));
    __ SubP(r1, r1, r0);
  } else {
    __ SubP(r1, r1, r3);
  }
  // Store this value in a local variable, for use when clearing
  // position registers.
  __ StoreP(r1, MemOperand(frame_pointer(), kStringStartMinusOne));

  // Initialize code pointer register
  __ mov(code_pointer(), Operand(masm_->CodeObject()));

  Label load_char_start_regexp, start_regexp;
  // Load newline if index is at start, previous character otherwise.
  __ CmpP(r3, Operand::Zero());
  __ bne(&load_char_start_regexp);
  __ mov(current_character(), Operand('\n'));
  __ b(&start_regexp);

  // Global regexp restarts matching here.
  __ bind(&load_char_start_regexp);
  // Load previous char as initial value of current character register.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&start_regexp);

  // Initialize on-stack registers.
  if (num_saved_registers_ > 0) {  // Always is, if generated from a regexp.
    // Fill saved registers with initial value = start offset - 1
    if (num_saved_registers_ > 8) {
      // One slot beyond address of register 0.
      __ lay(r3, MemOperand(frame_pointer(), kRegisterZero + kPointerSize));
      __ LoadImmP(r4, Operand(num_saved_registers_));
      Label init_loop;
      __ bind(&init_loop);
      __ StoreP(r1, MemOperand(r3, -kPointerSize));
      __ lay(r3, MemOperand(r3, -kPointerSize));
      __ BranchOnCount(r4, &init_loop);
    } else {
      for (int i = 0; i < num_saved_registers_; i++) {
        __ StoreP(r1, register_location(i));
      }
    }
  }

  // Initialize backtrack stack pointer.
  __ LoadP(backtrack_stackpointer(),
           MemOperand(frame_pointer(), kStackHighEnd));

  __ b(&start_label_);

  // Exit code:
  if (success_label_.is_linked()) {
    // Save captures when successful.
    __ bind(&success_label_);
    if (num_saved_registers_ > 0) {
      // copy captures to output
      __ LoadP(r0, MemOperand(frame_pointer(), kInputStart));
      __ LoadP(r2, MemOperand(frame_pointer(), kRegisterOutput));
      __ LoadP(r4, MemOperand(frame_pointer(), kStartIndex));
      __ SubP(r0, end_of_input_address(), r0);
      // r0 is length of input in bytes.
      if (mode_ == UC16) {
        __ ShiftRightP(r0, r0, Operand(1));
      }
      // r0 is length of input in characters.
      __ AddP(r0, r4);
      // r0 is length of string in characters.

      DCHECK_EQ(0, num_saved_registers_ % 2);
      // Always an even number of capture registers. This allows us to
      // unroll the loop once to add an operation between a load of a register
      // and the following use of that register.
      __ lay(r2, MemOperand(r2, num_saved_registers_ * kIntSize));
      for (int i = 0; i < num_saved_registers_;) {
        if ((false) && i < num_saved_registers_ - 4) {
          // TODO(john.yan): Can be optimized by SIMD instructions
          __ LoadMultipleP(r3, r6, register_location(i + 3));
          if (mode_ == UC16) {
            __ ShiftRightArithP(r3, r3, Operand(1));
            __ ShiftRightArithP(r4, r4, Operand(1));
            __ ShiftRightArithP(r5, r5, Operand(1));
            __ ShiftRightArithP(r6, r6, Operand(1));
          }
          __ AddP(r3, r0);
          __ AddP(r4, r0);
          __ AddP(r5, r0);
          __ AddP(r6, r0);
          __ StoreW(r3,
                    MemOperand(r2, -(num_saved_registers_ - i - 3) * kIntSize));
          __ StoreW(r4,
                    MemOperand(r2, -(num_saved_registers_ - i - 2) * kIntSize));
          __ StoreW(r5,
                    MemOperand(r2, -(num_saved_registers_ - i - 1) * kIntSize));
          __ StoreW(r6, MemOperand(r2, -(num_saved_registers_ - i) * kIntSize));
          i += 4;
        } else {
          __ LoadMultipleP(r3, r4, register_location(i + 1));
          if (mode_ == UC16) {
            __ ShiftRightArithP(r3, r3, Operand(1));
            __ ShiftRightArithP(r4, r4, Operand(1));
          }
          __ AddP(r3, r0);
          __ AddP(r4, r0);
          __ StoreW(r3,
                    MemOperand(r2, -(num_saved_registers_ - i - 1) * kIntSize));
          __ StoreW(r4, MemOperand(r2, -(num_saved_registers_ - i) * kIntSize));
          i += 2;
        }
      }
      if (global_with_zero_length_check()) {
        // Keep capture start in r6 for the zero-length check later.
        __ LoadP(r6, register_location(0));
      }
    }

    if (global()) {
      // Restart matching if the regular expression is flagged as global.
      __ LoadP(r2, MemOperand(frame_pointer(), kSuccessfulCaptures));
      __ LoadP(r3, MemOperand(frame_pointer(), kNumOutputRegisters));
      __ LoadP(r4, MemOperand(frame_pointer(), kRegisterOutput));
      // Increment success counter.
      __ AddP(r2, Operand(1));
      __ StoreP(r2, MemOperand(frame_pointer(), kSuccessfulCaptures));
      // Capture results have been stored, so the number of remaining global
      // output registers is reduced by the number of stored captures.
      __ SubP(r3, Operand(num_saved_registers_));
      // Check whether we have enough room for another set of capture results.
      __ CmpP(r3, Operand(num_saved_registers_));
      __ blt(&return_r2);

      __ StoreP(r3, MemOperand(frame_pointer(), kNumOutputRegisters));
      // Advance the location for output.
      __ AddP(r4, Operand(num_saved_registers_ * kIntSize));
      __ StoreP(r4, MemOperand(frame_pointer(), kRegisterOutput));

      // Prepare r2 to initialize registers with its value in the next run.
      __ LoadP(r2, MemOperand(frame_pointer(), kStringStartMinusOne));

      if (global_with_zero_length_check()) {
        // Special case for zero-length matches.
        // r6: capture start index
        __ CmpP(current_input_offset(), r6);
        // Not a zero-length match, restart.
        __ bne(&load_char_start_regexp);
        // Offset from the end is zero if we already reached the end.
        __ CmpP(current_input_offset(), Operand::Zero());
        __ beq(&exit_label_);
        // Advance current position after a zero-length match.
        Label advance;
        __ bind(&advance);
        __ AddP(current_input_offset(), Operand((mode_ == UC16) ? 2 : 1));
        if (global_unicode()) CheckNotInSurrogatePair(0, &advance);
      }

      __ b(&load_char_start_regexp);
    } else {
      __ LoadImmP(r2, Operand(SUCCESS));
    }
  }

  // Exit and return r2
  __ bind(&exit_label_);
  if (global()) {
    __ LoadP(r2, MemOperand(frame_pointer(), kSuccessfulCaptures));
  }

  __ bind(&return_r2);
  // Skip sp past regexp registers and local variables..
  __ LoadRR(sp, frame_pointer());
  // Restore registers r6..r15.
  __ LoadMultipleP(r6, sp, MemOperand(sp, 6 * kPointerSize));

  __ b(r14);

  // Backtrack code (branch target for conditional backtracks).
  if (backtrack_label_.is_linked()) {
    __ bind(&backtrack_label_);
    Backtrack();
  }

  Label exit_with_exception;

  // Preempt-code
  if (check_preempt_label_.is_linked()) {
    SafeCallTarget(&check_preempt_label_);

    CallCheckStackGuardState(r2);
    __ CmpP(r2, Operand::Zero());
    // If returning non-zero, we should end execution with the given
    // result as return value.
    __ bne(&return_r2);

    // String might have moved: Reload end of string from frame.
    __ LoadP(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
    SafeReturn();
  }

  // Backtrack stack overflow code.
  if (stack_overflow_label_.is_linked()) {
    SafeCallTarget(&stack_overflow_label_);
    // Reached if the backtrack-stack limit has been hit.
    Label grow_failed;

    // Call GrowStack(backtrack_stackpointer(), &stack_base)
    static const int num_arguments = 3;
    __ PrepareCallCFunction(num_arguments, r2);
    __ LoadRR(r2, backtrack_stackpointer());
    __ AddP(r3, frame_pointer(), Operand(kStackHighEnd));
    __ mov(r4, Operand(ExternalReference::isolate_address(isolate())));
    ExternalReference grow_stack = ExternalReference::re_grow_stack(isolate());
    __ CallCFunction(grow_stack, num_arguments);
    // If return nullptr, we have failed to grow the stack, and
    // must exit with a stack-overflow exception.
    __ CmpP(r2, Operand::Zero());
    __ beq(&exit_with_exception);
    // Otherwise use return value as new stack pointer.
    __ LoadRR(backtrack_stackpointer(), r2);
    // Restore saved registers and continue.
    SafeReturn();
  }

  if (exit_with_exception.is_linked()) {
    // If any of the code above needed to exit with an exception.
    __ bind(&exit_with_exception);
    // Exit with Result EXCEPTION(-1) to signal thrown exception.
    __ LoadImmP(r2, Operand(EXCEPTION));
    __ b(&return_r2);
  }

  CodeDesc code_desc;
  masm_->GetCode(isolate(), &code_desc);
  Handle<Code> code = isolate()->factory()->NewCode(code_desc, Code::REGEXP,
                                                    masm_->CodeObject());
  PROFILE(masm_->isolate(),
          RegExpCodeCreateEvent(AbstractCode::cast(*code), *source));
  return Handle<HeapObject>::cast(code);
}

void RegExpMacroAssemblerS390::GoTo(Label* to) { BranchOrBacktrack(al, to); }

void RegExpMacroAssemblerS390::IfRegisterGE(int reg, int comparand,
                                            Label* if_ge) {
  __ LoadP(r2, register_location(reg), r0);
  __ CmpP(r2, Operand(comparand));
  BranchOrBacktrack(ge, if_ge);
}

void RegExpMacroAssemblerS390::IfRegisterLT(int reg, int comparand,
                                            Label* if_lt) {
  __ LoadP(r2, register_location(reg), r0);
  __ CmpP(r2, Operand(comparand));
  BranchOrBacktrack(lt, if_lt);
}

void RegExpMacroAssemblerS390::IfRegisterEqPos(int reg, Label* if_eq) {
  __ LoadP(r2, register_location(reg), r0);
  __ CmpP(r2, current_input_offset());
  BranchOrBacktrack(eq, if_eq);
}

RegExpMacroAssembler::IrregexpImplementation
RegExpMacroAssemblerS390::Implementation() {
  return kS390Implementation;
}

void RegExpMacroAssemblerS390::LoadCurrentCharacter(int cp_offset,
                                                    Label* on_end_of_input,
                                                    bool check_bounds,
                                                    int characters) {
  DCHECK(cp_offset < (1 << 30));  // Be sane! (And ensure negation works)
  if (check_bounds) {
    if (cp_offset >= 0) {
      CheckPosition(cp_offset + characters - 1, on_end_of_input);
    } else {
      CheckPosition(cp_offset, on_end_of_input);
    }
  }
  LoadCurrentCharacterUnchecked(cp_offset, characters);
}

void RegExpMacroAssemblerS390::PopCurrentPosition() {
  Pop(current_input_offset());
}

void RegExpMacroAssemblerS390::PopRegister(int register_index) {
  Pop(r2);
  __ StoreP(r2, register_location(register_index));
}

void RegExpMacroAssemblerS390::PushBacktrack(Label* label) {
  if (label->is_bound()) {
    int target = label->pos();
    __ mov(r2, Operand(target + Code::kHeaderSize - kHeapObjectTag));
  } else {
    masm_->load_label_offset(r2, label);
  }
  Push(r2);
  CheckStackLimit();
}

void RegExpMacroAssemblerS390::PushCurrentPosition() {
  Push(current_input_offset());
}

void RegExpMacroAssemblerS390::PushRegister(int register_index,
                                            StackCheckFlag check_stack_limit) {
  __ LoadP(r2, register_location(register_index), r0);
  Push(r2);
  if (check_stack_limit) CheckStackLimit();
}

void RegExpMacroAssemblerS390::ReadCurrentPositionFromRegister(int reg) {
  __ LoadP(current_input_offset(), register_location(reg), r0);
}

void RegExpMacroAssemblerS390::ReadStackPointerFromRegister(int reg) {
  __ LoadP(backtrack_stackpointer(), register_location(reg), r0);
  __ LoadP(r2, MemOperand(frame_pointer(), kStackHighEnd));
  __ AddP(backtrack_stackpointer(), r2);
}

void RegExpMacroAssemblerS390::SetCurrentPositionFromEnd(int by) {
  Label after_position;
  __ CmpP(current_input_offset(), Operand(-by * char_size()));
  __ bge(&after_position);
  __ mov(current_input_offset(), Operand(-by * char_size()));
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&after_position);
}

void RegExpMacroAssemblerS390::SetRegister(int register_index, int to) {
  DCHECK(register_index >= num_saved_registers_);  // Reserved for positions!
  __ mov(r2, Operand(to));
  __ StoreP(r2, register_location(register_index));
}

bool RegExpMacroAssemblerS390::Succeed() {
  __ b(&success_label_);
  return global();
}

void RegExpMacroAssemblerS390::WriteCurrentPositionToRegister(int reg,
                                                              int cp_offset) {
  if (cp_offset == 0) {
    __ StoreP(current_input_offset(), register_location(reg));
  } else {
    __ AddP(r2, current_input_offset(), Operand(cp_offset * char_size()));
    __ StoreP(r2, register_location(reg));
  }
}

void RegExpMacroAssemblerS390::ClearRegisters(int reg_from, int reg_to) {
  DCHECK(reg_from <= reg_to);
  __ LoadP(r2, MemOperand(frame_pointer(), kStringStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ StoreP(r2, register_location(reg));
  }
}

void RegExpMacroAssemblerS390::WriteStackPointerToRegister(int reg) {
  __ LoadP(r3, MemOperand(frame_pointer(), kStackHighEnd));
  __ SubP(r2, backtrack_stackpointer(), r3);
  __ StoreP(r2, register_location(reg));
}

// Private methods:

void RegExpMacroAssemblerS390::CallCheckStackGuardState(Register scratch) {
  static const int num_arguments = 3;
  __ PrepareCallCFunction(num_arguments, scratch);
  // RegExp code frame pointer.
  __ LoadRR(r4, frame_pointer());
  // Code* of self.
  __ mov(r3, Operand(masm_->CodeObject()));
  // r2 becomes return address pointer.
  __ lay(r2, MemOperand(sp, kStackFrameRASlot * kPointerSize));
  ExternalReference stack_guard_check =
      ExternalReference::re_check_stack_guard_state(isolate());
  CallCFunctionUsingStub(stack_guard_check, num_arguments);
}

// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  DCHECK_EQ(kPointerSize, sizeof(T));
#ifdef V8_TARGET_ARCH_S390X
  return reinterpret_cast<T&>(Memory::uint64_at(re_frame + frame_offset));
#else
  return reinterpret_cast<T&>(Memory::uint32_at(re_frame + frame_offset));
#endif
}

template <typename T>
static T* frame_entry_address(Address re_frame, int frame_offset) {
  return reinterpret_cast<T*>(re_frame + frame_offset);
}

int RegExpMacroAssemblerS390::CheckStackGuardState(Address* return_address,
                                                   Code* re_code,
                                                   Address re_frame) {
  return NativeRegExpMacroAssembler::CheckStackGuardState(
      frame_entry<Isolate*>(re_frame, kIsolate),
      frame_entry<intptr_t>(re_frame, kStartIndex),
      frame_entry<intptr_t>(re_frame, kDirectCall) == 1, return_address,
      re_code, frame_entry_address<String*>(re_frame, kInputString),
      frame_entry_address<const byte*>(re_frame, kInputStart),
      frame_entry_address<const byte*>(re_frame, kInputEnd));
}

MemOperand RegExpMacroAssemblerS390::register_location(int register_index) {
  DCHECK(register_index < (1 << 30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return MemOperand(frame_pointer(),
                    kRegisterZero - register_index * kPointerSize);
}

void RegExpMacroAssemblerS390::CheckPosition(int cp_offset,
                                             Label* on_outside_input) {
  if (cp_offset >= 0) {
    __ CmpP(current_input_offset(), Operand(-cp_offset * char_size()));
    BranchOrBacktrack(ge, on_outside_input);
  } else {
    __ LoadP(r3, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ AddP(r2, current_input_offset(), Operand(cp_offset * char_size()));
    __ CmpP(r2, r3);
    BranchOrBacktrack(le, on_outside_input);
  }
}

void RegExpMacroAssemblerS390::BranchOrBacktrack(Condition condition, Label* to,
                                                 CRegister cr) {
  if (condition == al) {  // Unconditional.
    if (to == nullptr) {
      Backtrack();
      return;
    }
    __ b(to);
    return;
  }
  if (to == nullptr) {
    __ b(condition, &backtrack_label_);
    return;
  }
  __ b(condition, to);
}

void RegExpMacroAssemblerS390::SafeCall(Label* to, Condition cond,
                                        CRegister cr) {
  Label skip;
  __ b(NegateCondition(cond), &skip);
  __ b(r14, to);
  __ bind(&skip);
}

void RegExpMacroAssemblerS390::SafeReturn() {
  __ pop(r14);
  __ mov(ip, Operand(masm_->CodeObject()));
  __ AddP(r14, ip);
  __ Ret();
}

void RegExpMacroAssemblerS390::SafeCallTarget(Label* name) {
  __ bind(name);
  __ CleanseP(r14);
  __ LoadRR(r0, r14);
  __ mov(ip, Operand(masm_->CodeObject()));
  __ SubP(r0, r0, ip);
  __ push(r0);
}

void RegExpMacroAssemblerS390::Push(Register source) {
  DCHECK(source != backtrack_stackpointer());
  __ lay(backtrack_stackpointer(),
         MemOperand(backtrack_stackpointer(), -kPointerSize));
  __ StoreP(source, MemOperand(backtrack_stackpointer()));
}

void RegExpMacroAssemblerS390::Pop(Register target) {
  DCHECK(target != backtrack_stackpointer());
  __ LoadP(target, MemOperand(backtrack_stackpointer()));
  __ la(backtrack_stackpointer(),
        MemOperand(backtrack_stackpointer(), kPointerSize));
}

void RegExpMacroAssemblerS390::CheckPreemption() {
  // Check for preemption.
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(isolate());
  __ mov(r2, Operand(stack_limit));
  __ CmpLogicalP(sp, MemOperand(r2));
  SafeCall(&check_preempt_label_, le);
}

void RegExpMacroAssemblerS390::CheckStackLimit() {
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit(isolate());
  __ mov(r2, Operand(stack_limit));
  __ CmpLogicalP(backtrack_stackpointer(), MemOperand(r2));
  SafeCall(&stack_overflow_label_, le);
}

void RegExpMacroAssemblerS390::CallCFunctionUsingStub(
    ExternalReference function, int num_arguments) {
  // Must pass all arguments in registers. The stub pushes on the stack.
  DCHECK_GE(8, num_arguments);
  __ mov(code_pointer(), Operand(function));
  Label ret;
  __ larl(r14, &ret);
  __ StoreP(r14, MemOperand(sp, kStackFrameRASlot * kPointerSize));
  __ b(code_pointer());
  __ bind(&ret);
  if (base::OS::ActivationFrameAlignment() > kPointerSize) {
    __ LoadP(sp, MemOperand(sp, (kNumRequiredStackFrameSlots * kPointerSize)));
  } else {
    __ la(sp, MemOperand(sp, (kNumRequiredStackFrameSlots * kPointerSize)));
  }
  __ mov(code_pointer(), Operand(masm_->CodeObject()));
}


void RegExpMacroAssemblerS390::LoadCurrentCharacterUnchecked(int cp_offset,
                                                             int characters) {
  if (mode_ == LATIN1) {
    // using load reverse for big-endian platforms
    if (characters == 4) {
#if V8_TARGET_LITTLE_ENDIAN
      __ LoadlW(current_character(),
                MemOperand(current_input_offset(), end_of_input_address(),
                           cp_offset * char_size()));
#else
      __ LoadLogicalReversedWordP(current_character(),
                MemOperand(current_input_offset(), end_of_input_address(),
                           cp_offset * char_size()));
#endif
    } else if (characters == 2) {
#if V8_TARGET_LITTLE_ENDIAN
      __ LoadLogicalHalfWordP(current_character(),
                MemOperand(current_input_offset(), end_of_input_address(),
                           cp_offset * char_size()));
#else
      __ LoadLogicalReversedHalfWordP(current_character(),
                MemOperand(current_input_offset(), end_of_input_address(),
                           cp_offset * char_size()));
#endif
    } else {
      DCHECK_EQ(1, characters);
      __ LoadlB(current_character(),
                MemOperand(current_input_offset(), end_of_input_address(),
                           cp_offset * char_size()));
    }
  } else {
    DCHECK(mode_ == UC16);
    if (characters == 2) {
      __ LoadlW(current_character(),
                MemOperand(current_input_offset(), end_of_input_address(),
                           cp_offset * char_size()));
#if !V8_TARGET_LITTLE_ENDIAN
      // need to swap the order of the characters for big-endian platforms
      __ rll(current_character(), current_character(), Operand(16));
#endif
    } else {
      DCHECK_EQ(1, characters);
      __ LoadLogicalHalfWordP(
          current_character(),
                MemOperand(current_input_offset(), end_of_input_address(),
                           cp_offset * char_size()));
    }
  }
}

#undef __

#endif  // V8_INTERPRETED_REGEXP
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

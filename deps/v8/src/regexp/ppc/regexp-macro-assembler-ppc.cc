// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

#include "src/regexp/ppc/regexp-macro-assembler-ppc.h"

#include "src/base/bits.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/logging/log.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/strings/unicode.h"

namespace v8 {
namespace internal {

/*
 * This assembler uses the following register assignment convention
 * - r25: Temporarily stores the index of capture start after a matching pass
 *        for a global regexp.
 * - r26: Pointer to current Code object including heap object tag.
 * - r27: Current position in input, as negative offset from end of string.
 *        Please notice that this is the byte offset, not the character offset!
 * - r28: Currently loaded character. Must be loaded using
 *        LoadCurrentCharacter before using any of the dispatch methods.
 * - r29: Points to tip of backtrack stack
 * - r30: End of input (points to byte after last character in input).
 * - r31: Frame pointer. Used to access arguments, local variables and
 *         RegExp registers.
 * - r12: IP register, used by assembler. Very volatile.
 * - r1/sp : Points to tip of C stack.
 *
 * The remaining registers are free for computations.
 * Each call to a public method should retain this convention.
 *
 * The stack will have the following structure
 *  - fp[44]  Address regexp     (address of the JSRegExp object; unused in
 *                                native code, passed to match signature of
 *                                the interpreter):
 *  - fp[40]  Isolate* isolate   (address of the current isolate)
 *  - fp[36]  lr save area (currently unused)
 *  - fp[32]  backchain    (currently unused)
 *  --- sp when called ---
 *  - fp[28]  return address     (lr).
 *  - fp[24]  old frame pointer  (r31).
 *  - fp[0..20]  backup of registers r25..r30
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
 * int (*match)(String input_string,
 *              int start_index,
 *              Address start,
 *              Address end,
 *              int* capture_output_array,
 *              int num_capture_registers,
 *              byte* stack_area_base,
 *              bool direct_call = false,
 *              Isolate* isolate,
 *              Address regexp);
 * The call is performed by NativeRegExpMacroAssembler::Execute()
 * (in regexp-macro-assembler.cc) via the GeneratedCode wrapper.
 */

#define __ ACCESS_MASM(masm_)

const int RegExpMacroAssemblerPPC::kRegExpCodeSize;

RegExpMacroAssemblerPPC::RegExpMacroAssemblerPPC(Isolate* isolate, Zone* zone,
                                                 Mode mode,
                                                 int registers_to_save)
    : NativeRegExpMacroAssembler(isolate, zone),
      masm_(new MacroAssembler(isolate, CodeObjectRequired::kYes,
                               NewAssemblerBuffer(kRegExpCodeSize))),
      mode_(mode),
      num_registers_(registers_to_save),
      num_saved_registers_(registers_to_save),
      entry_label_(),
      start_label_(),
      success_label_(),
      backtrack_label_(),
      exit_label_(),
      internal_failure_label_() {
  masm_->set_root_array_available(false);

  DCHECK_EQ(0, registers_to_save % 2);


  __ b(&entry_label_);  // We'll write the entry code later.
  // If the code gets too big or corrupted, an internal exception will be
  // raised, and we will exit right away.
  __ bind(&internal_failure_label_);
  __ li(r3, Operand(FAILURE));
  __ Ret();
  __ bind(&start_label_);  // And then continue from here.
}

RegExpMacroAssemblerPPC::~RegExpMacroAssemblerPPC() {
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


int RegExpMacroAssemblerPPC::stack_limit_slack() {
  return RegExpStack::kStackLimitSlack;
}


void RegExpMacroAssemblerPPC::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    if (is_int16(by * char_size())) {
      __ addi(current_input_offset(), current_input_offset(),
              Operand(by * char_size()));
    } else {
      __ mov(r0, Operand(by * char_size()));
      __ add(current_input_offset(), r0, current_input_offset());
    }
  }
}


void RegExpMacroAssemblerPPC::AdvanceRegister(int reg, int by) {
  DCHECK_LE(0, reg);
  DCHECK_GT(num_registers_, reg);
  if (by != 0) {
    __ LoadP(r3, register_location(reg), r0);
    __ mov(r0, Operand(by));
    __ add(r3, r3, r0);
    __ StoreP(r3, register_location(reg), r0);
  }
}


void RegExpMacroAssemblerPPC::Backtrack() {
  CheckPreemption();
  if (has_backtrack_limit()) {
    Label next;
    __ LoadP(r3, MemOperand(frame_pointer(), kBacktrackCount), r0);
    __ addi(r3, r3, Operand(1));
    __ StoreP(r3, MemOperand(frame_pointer(), kBacktrackCount), r0);
    __ cmpi(r3, Operand(backtrack_limit()));
    __ bne(&next);

    // Exceeded limits are treated as a failed match.
    Fail();

    __ bind(&next);
  }
  // Pop Code offset from backtrack stack, add Code and jump to location.
  Pop(r3);
  __ add(r3, r3, code_pointer());
  __ Jump(r3);
}


void RegExpMacroAssemblerPPC::Bind(Label* label) { __ bind(label); }


void RegExpMacroAssemblerPPC::CheckCharacter(uint32_t c, Label* on_equal) {
  __ Cmpli(current_character(), Operand(c), r0);
  BranchOrBacktrack(eq, on_equal);
}


void RegExpMacroAssemblerPPC::CheckCharacterGT(uc16 limit, Label* on_greater) {
  __ Cmpli(current_character(), Operand(limit), r0);
  BranchOrBacktrack(gt, on_greater);
}

void RegExpMacroAssemblerPPC::CheckAtStart(int cp_offset, Label* on_at_start) {
  __ LoadP(r4, MemOperand(frame_pointer(), kStringStartMinusOne));
  __ addi(r3, current_input_offset(),
          Operand(-char_size() + cp_offset * char_size()));
  __ cmp(r3, r4);
  BranchOrBacktrack(eq, on_at_start);
}

void RegExpMacroAssemblerPPC::CheckNotAtStart(int cp_offset,
                                              Label* on_not_at_start) {
  __ LoadP(r4, MemOperand(frame_pointer(), kStringStartMinusOne));
  __ addi(r3, current_input_offset(),
          Operand(-char_size() + cp_offset * char_size()));
  __ cmp(r3, r4);
  BranchOrBacktrack(ne, on_not_at_start);
}


void RegExpMacroAssemblerPPC::CheckCharacterLT(uc16 limit, Label* on_less) {
  __ Cmpli(current_character(), Operand(limit), r0);
  BranchOrBacktrack(lt, on_less);
}


void RegExpMacroAssemblerPPC::CheckGreedyLoop(Label* on_equal) {
  Label backtrack_non_equal;
  __ LoadP(r3, MemOperand(backtrack_stackpointer(), 0));
  __ cmp(current_input_offset(), r3);
  __ bne(&backtrack_non_equal);
  __ addi(backtrack_stackpointer(), backtrack_stackpointer(),
          Operand(kPointerSize));

  __ bind(&backtrack_non_equal);
  BranchOrBacktrack(eq, on_equal);
}

void RegExpMacroAssemblerPPC::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, Label* on_no_match) {
  Label fallthrough;
  __ LoadP(r3, register_location(start_reg), r0);  // Index of start of capture
  __ LoadP(r4, register_location(start_reg + 1), r0);  // Index of end
  __ sub(r4, r4, r3, LeaveOE, SetRC);                  // Length of capture.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ beq(&fallthrough, cr0);

  // Check that there are enough characters left in the input.
  if (read_backward) {
    __ LoadP(r6, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ add(r6, r6, r4);
    __ cmp(current_input_offset(), r6);
    BranchOrBacktrack(le, on_no_match);
  } else {
    __ add(r0, r4, current_input_offset(), LeaveOE, SetRC);
    BranchOrBacktrack(gt, on_no_match, cr0);
  }

  if (mode_ == LATIN1) {
    Label success;
    Label fail;
    Label loop_check;

    // r3 - offset of start of capture
    // r4 - length of capture
    __ add(r3, r3, end_of_input_address());
    __ add(r5, end_of_input_address(), current_input_offset());
    if (read_backward) {
      __ sub(r5, r5, r4);  // Offset by length when matching backwards.
    }
    __ add(r4, r3, r4);

    // r3 - Address of start of capture.
    // r4 - Address of end of capture
    // r5 - Address of current input position.

    Label loop;
    __ bind(&loop);
    __ lbz(r6, MemOperand(r3));
    __ addi(r3, r3, Operand(char_size()));
    __ lbz(r25, MemOperand(r5));
    __ addi(r5, r5, Operand(char_size()));
    __ cmp(r25, r6);
    __ beq(&loop_check);

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ ori(r6, r6, Operand(0x20));  // Convert capture character to lower-case.
    __ ori(r25, r25, Operand(0x20));  // Also convert input character.
    __ cmp(r25, r6);
    __ bne(&fail);
    __ subi(r6, r6, Operand('a'));
    __ cmpli(r6, Operand('z' - 'a'));  // Is r6 a lowercase letter?
    __ ble(&loop_check);               // In range 'a'-'z'.
    // Latin-1: Check for values in range [224,254] but not 247.
    __ subi(r6, r6, Operand(224 - 'a'));
    __ cmpli(r6, Operand(254 - 224));
    __ bgt(&fail);                    // Weren't Latin-1 letters.
    __ cmpi(r6, Operand(247 - 224));  // Check for 247.
    __ beq(&fail);

    __ bind(&loop_check);
    __ cmp(r3, r4);
    __ blt(&loop);
    __ b(&success);

    __ bind(&fail);
    BranchOrBacktrack(al, on_no_match);

    __ bind(&success);
    // Compute new value of character position after the matched part.
    __ sub(current_input_offset(), r5, end_of_input_address());
    if (read_backward) {
      __ LoadP(r3, register_location(start_reg));  // Index of start of capture
      __ LoadP(r4,
               register_location(start_reg + 1));  // Index of end of capture
      __ add(current_input_offset(), current_input_offset(), r3);
      __ sub(current_input_offset(), current_input_offset(), r4);
    }
  } else {
    DCHECK(mode_ == UC16);
    int argument_count = 4;
    __ PrepareCallCFunction(argument_count, r5);

    // r3 - offset of start of capture
    // r4 - length of capture

    // Put arguments into arguments registers.
    // Parameters are
    //   r3: Address byte_offset1 - Address captured substring's start.
    //   r4: Address byte_offset2 - Address of current character position.
    //   r5: size_t byte_length - length of capture in bytes(!)
    //   r6: Isolate* isolate.

    // Address of start of capture.
    __ add(r3, r3, end_of_input_address());
    // Length of capture.
    __ mr(r5, r4);
    // Save length in callee-save register for use on return.
    __ mr(r25, r4);
    // Address of current input position.
    __ add(r4, current_input_offset(), end_of_input_address());
    if (read_backward) {
      __ sub(r4, r4, r25);
    }
    // Isolate.
    __ mov(r6, Operand(ExternalReference::isolate_address(isolate())));

    {
      AllowExternalCallThatCantCauseGC scope(masm_);
      ExternalReference function =
          ExternalReference::re_case_insensitive_compare_uc16(isolate());
      __ CallCFunction(function, argument_count);
    }

    // Check if function returned non-zero for success or zero for failure.
    __ cmpi(r3, Operand::Zero());
    BranchOrBacktrack(eq, on_no_match);

    // On success, advance position by length of capture.
    if (read_backward) {
      __ sub(current_input_offset(), current_input_offset(), r25);
    } else {
      __ add(current_input_offset(), current_input_offset(), r25);
    }
  }

  __ bind(&fallthrough);
}

void RegExpMacroAssemblerPPC::CheckNotBackReference(int start_reg,
                                                    bool read_backward,
                                                    Label* on_no_match) {
  Label fallthrough;

  // Find length of back-referenced capture.
  __ LoadP(r3, register_location(start_reg), r0);
  __ LoadP(r4, register_location(start_reg + 1), r0);
  __ sub(r4, r4, r3, LeaveOE, SetRC);  // Length to check.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ beq(&fallthrough, cr0);

  // Check that there are enough characters left in the input.
  if (read_backward) {
    __ LoadP(r6, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ add(r6, r6, r4);
    __ cmp(current_input_offset(), r6);
    BranchOrBacktrack(le, on_no_match);
  } else {
    __ add(r0, r4, current_input_offset(), LeaveOE, SetRC);
    BranchOrBacktrack(gt, on_no_match, cr0);
  }

  // r3 - offset of start of capture
  // r4 - length of capture
  __ add(r3, r3, end_of_input_address());
  __ add(r5, end_of_input_address(), current_input_offset());
  if (read_backward) {
    __ sub(r5, r5, r4);  // Offset by length when matching backwards.
  }
  __ add(r4, r4, r3);

  Label loop;
  __ bind(&loop);
  if (mode_ == LATIN1) {
    __ lbz(r6, MemOperand(r3));
    __ addi(r3, r3, Operand(char_size()));
    __ lbz(r25, MemOperand(r5));
    __ addi(r5, r5, Operand(char_size()));
  } else {
    DCHECK(mode_ == UC16);
    __ lhz(r6, MemOperand(r3));
    __ addi(r3, r3, Operand(char_size()));
    __ lhz(r25, MemOperand(r5));
    __ addi(r5, r5, Operand(char_size()));
  }
  __ cmp(r6, r25);
  BranchOrBacktrack(ne, on_no_match);
  __ cmp(r3, r4);
  __ blt(&loop);

  // Move current character position to position after match.
  __ sub(current_input_offset(), r5, end_of_input_address());
  if (read_backward) {
    __ LoadP(r3, register_location(start_reg));  // Index of start of capture
    __ LoadP(r4, register_location(start_reg + 1));  // Index of end of capture
    __ add(current_input_offset(), current_input_offset(), r3);
    __ sub(current_input_offset(), current_input_offset(), r4);
  }

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerPPC::CheckNotCharacter(unsigned c,
                                                Label* on_not_equal) {
  __ Cmpli(current_character(), Operand(c), r0);
  BranchOrBacktrack(ne, on_not_equal);
}


void RegExpMacroAssemblerPPC::CheckCharacterAfterAnd(uint32_t c, uint32_t mask,
                                                     Label* on_equal) {
  __ mov(r0, Operand(mask));
  if (c == 0) {
    __ and_(r3, current_character(), r0, SetRC);
  } else {
    __ and_(r3, current_character(), r0);
    __ Cmpli(r3, Operand(c), r0, cr0);
  }
  BranchOrBacktrack(eq, on_equal, cr0);
}


void RegExpMacroAssemblerPPC::CheckNotCharacterAfterAnd(unsigned c,
                                                        unsigned mask,
                                                        Label* on_not_equal) {
  __ mov(r0, Operand(mask));
  if (c == 0) {
    __ and_(r3, current_character(), r0, SetRC);
  } else {
    __ and_(r3, current_character(), r0);
    __ Cmpli(r3, Operand(c), r0, cr0);
  }
  BranchOrBacktrack(ne, on_not_equal, cr0);
}


void RegExpMacroAssemblerPPC::CheckNotCharacterAfterMinusAnd(
    uc16 c, uc16 minus, uc16 mask, Label* on_not_equal) {
  DCHECK_GT(String::kMaxUtf16CodeUnit, minus);
  __ subi(r3, current_character(), Operand(minus));
  __ mov(r0, Operand(mask));
  __ and_(r3, r3, r0);
  __ Cmpli(r3, Operand(c), r0);
  BranchOrBacktrack(ne, on_not_equal);
}


void RegExpMacroAssemblerPPC::CheckCharacterInRange(uc16 from, uc16 to,
                                                    Label* on_in_range) {
  __ mov(r0, Operand(from));
  __ sub(r3, current_character(), r0);
  __ Cmpli(r3, Operand(to - from), r0);
  BranchOrBacktrack(le, on_in_range);  // Unsigned lower-or-same condition.
}


void RegExpMacroAssemblerPPC::CheckCharacterNotInRange(uc16 from, uc16 to,
                                                       Label* on_not_in_range) {
  __ mov(r0, Operand(from));
  __ sub(r3, current_character(), r0);
  __ Cmpli(r3, Operand(to - from), r0);
  BranchOrBacktrack(gt, on_not_in_range);  // Unsigned higher condition.
}


void RegExpMacroAssemblerPPC::CheckBitInTable(Handle<ByteArray> table,
                                              Label* on_bit_set) {
  __ mov(r3, Operand(table));
  if (mode_ != LATIN1 || kTableMask != String::kMaxOneByteCharCode) {
    __ andi(r4, current_character(), Operand(kTableSize - 1));
    __ addi(r4, r4, Operand(ByteArray::kHeaderSize - kHeapObjectTag));
  } else {
    __ addi(r4, current_character(),
            Operand(ByteArray::kHeaderSize - kHeapObjectTag));
  }
  __ lbzx(r3, MemOperand(r3, r4));
  __ cmpi(r3, Operand::Zero());
  BranchOrBacktrack(ne, on_bit_set);
}


bool RegExpMacroAssemblerPPC::CheckSpecialCharacterClass(uc16 type,
                                                         Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check
  switch (type) {
    case 's':
      // Match space-characters
      if (mode_ == LATIN1) {
        // One byte space characters are '\t'..'\r', ' ' and \u00a0.
        Label success;
        __ cmpi(current_character(), Operand(' '));
        __ beq(&success);
        // Check range 0x09..0x0D
        __ subi(r3, current_character(), Operand('\t'));
        __ cmpli(r3, Operand('\r' - '\t'));
        __ ble(&success);
        // \u00a0 (NBSP).
        __ cmpi(r3, Operand(0x00A0 - '\t'));
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
      __ subi(r3, current_character(), Operand('0'));
      __ cmpli(r3, Operand('9' - '0'));
      BranchOrBacktrack(gt, on_no_match);
      return true;
    case 'D':
      // Match non ASCII-digits
      __ subi(r3, current_character(), Operand('0'));
      __ cmpli(r3, Operand('9' - '0'));
      BranchOrBacktrack(le, on_no_match);
      return true;
    case '.': {
      // Match non-newlines (not 0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029)
      __ xori(r3, current_character(), Operand(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C
      __ subi(r3, r3, Operand(0x0B));
      __ cmpli(r3, Operand(0x0C - 0x0B));
      BranchOrBacktrack(le, on_no_match);
      if (mode_ == UC16) {
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ subi(r3, r3, Operand(0x2028 - 0x0B));
        __ cmpli(r3, Operand(1));
        BranchOrBacktrack(le, on_no_match);
      }
      return true;
    }
    case 'n': {
      // Match newlines (0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029)
      __ xori(r3, current_character(), Operand(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C
      __ subi(r3, r3, Operand(0x0B));
      __ cmpli(r3, Operand(0x0C - 0x0B));
      if (mode_ == LATIN1) {
        BranchOrBacktrack(gt, on_no_match);
      } else {
        Label done;
        __ ble(&done);
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ subi(r3, r3, Operand(0x2028 - 0x0B));
        __ cmpli(r3, Operand(1));
        BranchOrBacktrack(gt, on_no_match);
        __ bind(&done);
      }
      return true;
    }
    case 'w': {
      if (mode_ != LATIN1) {
        // Table is 256 entries, so all Latin1 characters can be tested.
        __ cmpi(current_character(), Operand('z'));
        BranchOrBacktrack(gt, on_no_match);
      }
      ExternalReference map =
          ExternalReference::re_word_character_map(isolate());
      __ mov(r3, Operand(map));
      __ lbzx(r3, MemOperand(r3, current_character()));
      __ cmpli(r3, Operand::Zero());
      BranchOrBacktrack(eq, on_no_match);
      return true;
    }
    case 'W': {
      Label done;
      if (mode_ != LATIN1) {
        // Table is 256 entries, so all Latin1 characters can be tested.
        __ cmpli(current_character(), Operand('z'));
        __ bgt(&done);
      }
      ExternalReference map =
          ExternalReference::re_word_character_map(isolate());
      __ mov(r3, Operand(map));
      __ lbzx(r3, MemOperand(r3, current_character()));
      __ cmpli(r3, Operand::Zero());
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


void RegExpMacroAssemblerPPC::Fail() {
  __ li(r3, Operand(FAILURE));
  __ b(&exit_label_);
}


Handle<HeapObject> RegExpMacroAssemblerPPC::GetCode(Handle<String> source) {
  Label return_r3;

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

    // Tell the system that we have a stack frame.  Because the type
    // is MANUAL, no is generated.
    FrameScope scope(masm_, StackFrame::MANUAL);

    // Ensure register assigments are consistent with callee save mask
    DCHECK(r25.bit() & kRegExpCalleeSaved);
    DCHECK(code_pointer().bit() & kRegExpCalleeSaved);
    DCHECK(current_input_offset().bit() & kRegExpCalleeSaved);
    DCHECK(current_character().bit() & kRegExpCalleeSaved);
    DCHECK(backtrack_stackpointer().bit() & kRegExpCalleeSaved);
    DCHECK(end_of_input_address().bit() & kRegExpCalleeSaved);
    DCHECK(frame_pointer().bit() & kRegExpCalleeSaved);

    // Actually emit code to start a new stack frame.
    // Push arguments
    // Save callee-save registers.
    // Start new stack frame.
    // Store link register in existing stack-cell.
    // Order here should correspond to order of offset constants in header file.
    RegList registers_to_retain = kRegExpCalleeSaved;
    RegList argument_registers = r3.bit() | r4.bit() | r5.bit() | r6.bit() |
                                 r7.bit() | r8.bit() | r9.bit() | r10.bit();
    __ mflr(r0);
    __ push(r0);
    __ MultiPush(argument_registers | registers_to_retain);
    // Set frame pointer in space for it if this is not a direct call
    // from generated code.
    __ addi(frame_pointer(), sp, Operand(8 * kPointerSize));

    STATIC_ASSERT(kSuccessfulCaptures == kInputString - kSystemPointerSize);
    __ li(r3, Operand::Zero());
    __ push(r3);  // Make room for success counter and initialize it to 0.
    STATIC_ASSERT(kStringStartMinusOne ==
                  kSuccessfulCaptures - kSystemPointerSize);
    __ push(r3);  // Make room for "string start - 1" constant.
    STATIC_ASSERT(kBacktrackCount == kStringStartMinusOne - kSystemPointerSize);
    __ push(r3);  // The backtrack counter.

    // Check if we have space on the stack for registers.
    Label stack_limit_hit;
    Label stack_ok;

    ExternalReference stack_limit =
        ExternalReference::address_of_jslimit(isolate());
    __ mov(r3, Operand(stack_limit));
    __ LoadP(r3, MemOperand(r3));
    __ sub(r3, sp, r3, LeaveOE, SetRC);
    // Handle it if the stack pointer is already below the stack limit.
    __ ble(&stack_limit_hit, cr0);
    // Check if there is room for the variable number of registers above
    // the stack limit.
    __ Cmpli(r3, Operand(num_registers_ * kPointerSize), r0);
    __ bge(&stack_ok);
    // Exit with OutOfMemory exception. There is not enough space on the stack
    // for our working registers.
    __ li(r3, Operand(EXCEPTION));
    __ b(&return_r3);

    __ bind(&stack_limit_hit);
    CallCheckStackGuardState(r3);
    __ cmpi(r3, Operand::Zero());
    // If returned value is non-zero, we exit with the returned value as result.
    __ bne(&return_r3);

    __ bind(&stack_ok);

    // Allocate space on stack for registers.
    __ Add(sp, sp, -num_registers_ * kPointerSize, r0);
    // Load string end.
    __ LoadP(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
    // Load input start.
    __ LoadP(r3, MemOperand(frame_pointer(), kInputStart));
    // Find negative length (offset of start relative to end).
    __ sub(current_input_offset(), r3, end_of_input_address());
    // Set r3 to address of char before start of the input string
    // (effectively string position -1).
    __ LoadP(r4, MemOperand(frame_pointer(), kStartIndex));
    __ subi(r3, current_input_offset(), Operand(char_size()));
    if (mode_ == UC16) {
      __ ShiftLeftImm(r0, r4, Operand(1));
      __ sub(r3, r3, r0);
    } else {
      __ sub(r3, r3, r4);
    }
    // Store this value in a local variable, for use when clearing
    // position registers.
    __ StoreP(r3, MemOperand(frame_pointer(), kStringStartMinusOne));

    // Initialize code pointer register
    __ mov(code_pointer(), Operand(masm_->CodeObject()));

    Label load_char_start_regexp, start_regexp;
    // Load newline if index is at start, previous character otherwise.
    __ cmpi(r4, Operand::Zero());
    __ bne(&load_char_start_regexp);
    __ li(current_character(), Operand('\n'));
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
        __ addi(r4, frame_pointer(), Operand(kRegisterZero + kPointerSize));
        __ mov(r5, Operand(num_saved_registers_));
        __ mtctr(r5);
        Label init_loop;
        __ bind(&init_loop);
        __ StorePU(r3, MemOperand(r4, -kPointerSize));
        __ bdnz(&init_loop);
      } else {
        for (int i = 0; i < num_saved_registers_; i++) {
          __ StoreP(r3, register_location(i), r0);
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
        __ LoadP(r4, MemOperand(frame_pointer(), kInputStart));
        __ LoadP(r3, MemOperand(frame_pointer(), kRegisterOutput));
        __ LoadP(r5, MemOperand(frame_pointer(), kStartIndex));
        __ sub(r4, end_of_input_address(), r4);
        // r4 is length of input in bytes.
        if (mode_ == UC16) {
          __ ShiftRightImm(r4, r4, Operand(1));
        }
        // r4 is length of input in characters.
        __ add(r4, r4, r5);
        // r4 is length of string in characters.

        DCHECK_EQ(0, num_saved_registers_ % 2);
        // Always an even number of capture registers. This allows us to
        // unroll the loop once to add an operation between a load of a register
        // and the following use of that register.
        for (int i = 0; i < num_saved_registers_; i += 2) {
          __ LoadP(r5, register_location(i), r0);
          __ LoadP(r6, register_location(i + 1), r0);
          if (i == 0 && global_with_zero_length_check()) {
            // Keep capture start in r25 for the zero-length check later.
            __ mr(r25, r5);
          }
          if (mode_ == UC16) {
            __ ShiftRightArithImm(r5, r5, 1);
            __ add(r5, r4, r5);
            __ ShiftRightArithImm(r6, r6, 1);
            __ add(r6, r4, r6);
          } else {
            __ add(r5, r4, r5);
            __ add(r6, r4, r6);
          }
          __ stw(r5, MemOperand(r3));
          __ addi(r3, r3, Operand(kIntSize));
          __ stw(r6, MemOperand(r3));
          __ addi(r3, r3, Operand(kIntSize));
        }
      }

      if (global()) {
        // Restart matching if the regular expression is flagged as global.
        __ LoadP(r3, MemOperand(frame_pointer(), kSuccessfulCaptures));
        __ LoadP(r4, MemOperand(frame_pointer(), kNumOutputRegisters));
        __ LoadP(r5, MemOperand(frame_pointer(), kRegisterOutput));
        // Increment success counter.
        __ addi(r3, r3, Operand(1));
        __ StoreP(r3, MemOperand(frame_pointer(), kSuccessfulCaptures));
        // Capture results have been stored, so the number of remaining global
        // output registers is reduced by the number of stored captures.
        __ subi(r4, r4, Operand(num_saved_registers_));
        // Check whether we have enough room for another set of capture results.
        __ cmpi(r4, Operand(num_saved_registers_));
        __ blt(&return_r3);

        __ StoreP(r4, MemOperand(frame_pointer(), kNumOutputRegisters));
        // Advance the location for output.
        __ addi(r5, r5, Operand(num_saved_registers_ * kIntSize));
        __ StoreP(r5, MemOperand(frame_pointer(), kRegisterOutput));

        // Prepare r3 to initialize registers with its value in the next run.
        __ LoadP(r3, MemOperand(frame_pointer(), kStringStartMinusOne));

        if (global_with_zero_length_check()) {
          // Special case for zero-length matches.
          // r25: capture start index
          __ cmp(current_input_offset(), r25);
          // Not a zero-length match, restart.
          __ bne(&load_char_start_regexp);
          // Offset from the end is zero if we already reached the end.
          __ cmpi(current_input_offset(), Operand::Zero());
          __ beq(&exit_label_);
          // Advance current position after a zero-length match.
          Label advance;
          __ bind(&advance);
          __ addi(current_input_offset(), current_input_offset(),
                  Operand((mode_ == UC16) ? 2 : 1));
          if (global_unicode()) CheckNotInSurrogatePair(0, &advance);
        }

        __ b(&load_char_start_regexp);
      } else {
        __ li(r3, Operand(SUCCESS));
      }
    }

    // Exit and return r3
    __ bind(&exit_label_);
    if (global()) {
      __ LoadP(r3, MemOperand(frame_pointer(), kSuccessfulCaptures));
    }

    __ bind(&return_r3);
    // Skip sp past regexp registers and local variables..
    __ mr(sp, frame_pointer());
    // Restore registers r25..r31 and return (restoring lr to pc).
    __ MultiPop(registers_to_retain);
    __ pop(r0);
    __ mtlr(r0);
    __ blr();

    // Backtrack code (branch target for conditional backtracks).
    if (backtrack_label_.is_linked()) {
      __ bind(&backtrack_label_);
      Backtrack();
    }

    Label exit_with_exception;

    // Preempt-code
    if (check_preempt_label_.is_linked()) {
      SafeCallTarget(&check_preempt_label_);

      CallCheckStackGuardState(r3);
      __ cmpi(r3, Operand::Zero());
      // If returning non-zero, we should end execution with the given
      // result as return value.
      __ bne(&return_r3);

      // String might have moved: Reload end of string from frame.
      __ LoadP(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
      SafeReturn();
    }

    // Backtrack stack overflow code.
    if (stack_overflow_label_.is_linked()) {
      SafeCallTarget(&stack_overflow_label_);

      // Call GrowStack(backtrack_stackpointer(), &stack_base)
      static const int num_arguments = 3;
      __ PrepareCallCFunction(num_arguments, r3);
      __ mr(r3, backtrack_stackpointer());
      __ addi(r4, frame_pointer(), Operand(kStackHighEnd));
      __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));
      ExternalReference grow_stack =
          ExternalReference::re_grow_stack(isolate());
      __ CallCFunction(grow_stack, num_arguments);
      // If return nullptr, we have failed to grow the stack, and
      // must exit with a stack-overflow exception.
      __ cmpi(r3, Operand::Zero());
      __ beq(&exit_with_exception);
      // Otherwise use return value as new stack pointer.
      __ mr(backtrack_stackpointer(), r3);
      // Restore saved registers and continue.
      SafeReturn();
    }

    if (exit_with_exception.is_linked()) {
      // If any of the code above needed to exit with an exception.
      __ bind(&exit_with_exception);
      // Exit with Result EXCEPTION(-1) to signal thrown exception.
      __ li(r3, Operand(EXCEPTION));
      __ b(&return_r3);
    }
  }

  CodeDesc code_desc;
  masm_->GetCode(isolate(), &code_desc);
  Handle<Code> code = Factory::CodeBuilder(isolate(), code_desc, Code::REGEXP)
                          .set_self_reference(masm_->CodeObject())
                          .Build();
  PROFILE(masm_->isolate(),
          RegExpCodeCreateEvent(Handle<AbstractCode>::cast(code), source));
  return Handle<HeapObject>::cast(code);
}


void RegExpMacroAssemblerPPC::GoTo(Label* to) { BranchOrBacktrack(al, to); }


void RegExpMacroAssemblerPPC::IfRegisterGE(int reg, int comparand,
                                           Label* if_ge) {
  __ LoadP(r3, register_location(reg), r0);
  __ Cmpi(r3, Operand(comparand), r0);
  BranchOrBacktrack(ge, if_ge);
}


void RegExpMacroAssemblerPPC::IfRegisterLT(int reg, int comparand,
                                           Label* if_lt) {
  __ LoadP(r3, register_location(reg), r0);
  __ Cmpi(r3, Operand(comparand), r0);
  BranchOrBacktrack(lt, if_lt);
}


void RegExpMacroAssemblerPPC::IfRegisterEqPos(int reg, Label* if_eq) {
  __ LoadP(r3, register_location(reg), r0);
  __ cmp(r3, current_input_offset());
  BranchOrBacktrack(eq, if_eq);
}


RegExpMacroAssembler::IrregexpImplementation
RegExpMacroAssemblerPPC::Implementation() {
  return kPPCImplementation;
}

void RegExpMacroAssemblerPPC::LoadCurrentCharacterImpl(int cp_offset,
                                                       Label* on_end_of_input,
                                                       bool check_bounds,
                                                       int characters,
                                                       int eats_at_least) {
  // It's possible to preload a small number of characters when each success
  // path requires a large number of characters, but not the reverse.
  DCHECK_GE(eats_at_least, characters);

  DCHECK(cp_offset < (1 << 30));  // Be sane! (And ensure negation works)
  if (check_bounds) {
    if (cp_offset >= 0) {
      CheckPosition(cp_offset + eats_at_least - 1, on_end_of_input);
    } else {
      CheckPosition(cp_offset, on_end_of_input);
    }
  }
  LoadCurrentCharacterUnchecked(cp_offset, characters);
}

void RegExpMacroAssemblerPPC::PopCurrentPosition() {
  Pop(current_input_offset());
}


void RegExpMacroAssemblerPPC::PopRegister(int register_index) {
  Pop(r3);
  __ StoreP(r3, register_location(register_index), r0);
}


void RegExpMacroAssemblerPPC::PushBacktrack(Label* label) {
  __ mov_label_offset(r3, label);
  Push(r3);
  CheckStackLimit();
}


void RegExpMacroAssemblerPPC::PushCurrentPosition() {
  Push(current_input_offset());
}


void RegExpMacroAssemblerPPC::PushRegister(int register_index,
                                           StackCheckFlag check_stack_limit) {
  __ LoadP(r3, register_location(register_index), r0);
  Push(r3);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerPPC::ReadCurrentPositionFromRegister(int reg) {
  __ LoadP(current_input_offset(), register_location(reg), r0);
}


void RegExpMacroAssemblerPPC::ReadStackPointerFromRegister(int reg) {
  __ LoadP(backtrack_stackpointer(), register_location(reg), r0);
  __ LoadP(r3, MemOperand(frame_pointer(), kStackHighEnd));
  __ add(backtrack_stackpointer(), backtrack_stackpointer(), r3);
}


void RegExpMacroAssemblerPPC::SetCurrentPositionFromEnd(int by) {
  Label after_position;
  __ Cmpi(current_input_offset(), Operand(-by * char_size()), r0);
  __ bge(&after_position);
  __ mov(current_input_offset(), Operand(-by * char_size()));
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&after_position);
}


void RegExpMacroAssemblerPPC::SetRegister(int register_index, int to) {
  DCHECK(register_index >= num_saved_registers_);  // Reserved for positions!
  __ mov(r3, Operand(to));
  __ StoreP(r3, register_location(register_index), r0);
}


bool RegExpMacroAssemblerPPC::Succeed() {
  __ b(&success_label_);
  return global();
}


void RegExpMacroAssemblerPPC::WriteCurrentPositionToRegister(int reg,
                                                             int cp_offset) {
  if (cp_offset == 0) {
    __ StoreP(current_input_offset(), register_location(reg), r0);
  } else {
    __ mov(r0, Operand(cp_offset * char_size()));
    __ add(r3, current_input_offset(), r0);
    __ StoreP(r3, register_location(reg), r0);
  }
}


void RegExpMacroAssemblerPPC::ClearRegisters(int reg_from, int reg_to) {
  DCHECK(reg_from <= reg_to);
  __ LoadP(r3, MemOperand(frame_pointer(), kStringStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ StoreP(r3, register_location(reg), r0);
  }
}


void RegExpMacroAssemblerPPC::WriteStackPointerToRegister(int reg) {
  __ LoadP(r4, MemOperand(frame_pointer(), kStackHighEnd));
  __ sub(r3, backtrack_stackpointer(), r4);
  __ StoreP(r3, register_location(reg), r0);
}


// Private methods:

void RegExpMacroAssemblerPPC::CallCheckStackGuardState(Register scratch) {
  DCHECK(!isolate()->IsGeneratingEmbeddedBuiltins());
  DCHECK(!masm_->options().isolate_independent_code);

  int frame_alignment = masm_->ActivationFrameAlignment();
  int stack_space = kNumRequiredStackFrameSlots;
  int stack_passed_arguments = 1;  // space for return address pointer

  // The following stack manipulation logic is similar to
  // PrepareCallCFunction.  However, we need an extra slot on the
  // stack to house the return address parameter.
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for stack arguments
    // -- preserving original value of sp.
    __ mr(scratch, sp);
    __ addi(sp, sp, Operand(-(stack_passed_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    __ ClearRightImm(sp, sp,
                     Operand(base::bits::WhichPowerOfTwo(frame_alignment)));
    __ StoreP(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    // Make room for stack arguments
    stack_space += stack_passed_arguments;
  }

  // Allocate frame with required slots to make ABI work.
  __ li(r0, Operand::Zero());
  __ StorePU(r0, MemOperand(sp, -stack_space * kPointerSize));

  // RegExp code frame pointer.
  __ mr(r5, frame_pointer());
  // Code of self.
  __ mov(r4, Operand(masm_->CodeObject()));
  // r3 will point to the return address, placed by DirectCEntry.
  __ addi(r3, sp, Operand(kStackFrameExtraParamSlot * kPointerSize));

  ExternalReference stack_guard_check =
      ExternalReference::re_check_stack_guard_state(isolate());
  __ mov(ip, Operand(stack_guard_check));

  EmbeddedData d = EmbeddedData::FromBlob();
  CHECK(Builtins::IsIsolateIndependent(Builtins::kDirectCEntry));
  Address entry = d.InstructionStartOfBuiltin(Builtins::kDirectCEntry);
  __ mov(r0, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  __ Call(r0);

  // Restore the stack pointer
  stack_space = kNumRequiredStackFrameSlots + stack_passed_arguments;
  if (frame_alignment > kPointerSize) {
    __ LoadP(sp, MemOperand(sp, stack_space * kPointerSize));
  } else {
    __ addi(sp, sp, Operand(stack_space * kPointerSize));
  }

  __ mov(code_pointer(), Operand(masm_->CodeObject()));
}


// Helper function for reading a value out of a stack frame.
template <typename T>
static T& frame_entry(Address re_frame, int frame_offset) {
  return reinterpret_cast<T&>(Memory<int32_t>(re_frame + frame_offset));
}


template <typename T>
static T* frame_entry_address(Address re_frame, int frame_offset) {
  return reinterpret_cast<T*>(re_frame + frame_offset);
}

int RegExpMacroAssemblerPPC::CheckStackGuardState(Address* return_address,
                                                  Address raw_code,
                                                  Address re_frame) {
  Code re_code = Code::cast(Object(raw_code));
  return NativeRegExpMacroAssembler::CheckStackGuardState(
      frame_entry<Isolate*>(re_frame, kIsolate),
      frame_entry<intptr_t>(re_frame, kStartIndex),
      static_cast<RegExp::CallOrigin>(
          frame_entry<intptr_t>(re_frame, kDirectCall)),
      return_address, re_code,
      frame_entry_address<Address>(re_frame, kInputString),
      frame_entry_address<const byte*>(re_frame, kInputStart),
      frame_entry_address<const byte*>(re_frame, kInputEnd));
}


MemOperand RegExpMacroAssemblerPPC::register_location(int register_index) {
  DCHECK(register_index < (1 << 30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return MemOperand(frame_pointer(),
                    kRegisterZero - register_index * kPointerSize);
}


void RegExpMacroAssemblerPPC::CheckPosition(int cp_offset,
                                            Label* on_outside_input) {
  if (cp_offset >= 0) {
    __ Cmpi(current_input_offset(), Operand(-cp_offset * char_size()), r0);
    BranchOrBacktrack(ge, on_outside_input);
  } else {
    __ LoadP(r4, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ addi(r3, current_input_offset(), Operand(cp_offset * char_size()));
    __ cmp(r3, r4);
    BranchOrBacktrack(le, on_outside_input);
  }
}


void RegExpMacroAssemblerPPC::BranchOrBacktrack(Condition condition, Label* to,
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
    __ b(condition, &backtrack_label_, cr);
    return;
  }
  __ b(condition, to, cr);
}


void RegExpMacroAssemblerPPC::SafeCall(Label* to, Condition cond,
                                       CRegister cr) {
  __ b(cond, to, cr, SetLK);
}


void RegExpMacroAssemblerPPC::SafeReturn() {
  __ pop(r0);
  __ mov(ip, Operand(masm_->CodeObject()));
  __ add(r0, r0, ip);
  __ mtlr(r0);
  __ blr();
}


void RegExpMacroAssemblerPPC::SafeCallTarget(Label* name) {
  __ bind(name);
  __ mflr(r0);
  __ mov(ip, Operand(masm_->CodeObject()));
  __ sub(r0, r0, ip);
  __ push(r0);
}


void RegExpMacroAssemblerPPC::Push(Register source) {
  DCHECK(source != backtrack_stackpointer());
  __ StorePU(source, MemOperand(backtrack_stackpointer(), -kPointerSize));
}


void RegExpMacroAssemblerPPC::Pop(Register target) {
  DCHECK(target != backtrack_stackpointer());
  __ LoadP(target, MemOperand(backtrack_stackpointer()));
  __ addi(backtrack_stackpointer(), backtrack_stackpointer(),
          Operand(kPointerSize));
}


void RegExpMacroAssemblerPPC::CheckPreemption() {
  // Check for preemption.
  ExternalReference stack_limit =
      ExternalReference::address_of_jslimit(isolate());
  __ mov(r3, Operand(stack_limit));
  __ LoadP(r3, MemOperand(r3));
  __ cmpl(sp, r3);
  SafeCall(&check_preempt_label_, le);
}


void RegExpMacroAssemblerPPC::CheckStackLimit() {
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit_address(isolate());
  __ mov(r3, Operand(stack_limit));
  __ LoadP(r3, MemOperand(r3));
  __ cmpl(backtrack_stackpointer(), r3);
  SafeCall(&stack_overflow_label_, le);
}


void RegExpMacroAssemblerPPC::LoadCurrentCharacterUnchecked(int cp_offset,
                                                            int characters) {
  Register offset = current_input_offset();
  if (cp_offset != 0) {
    // r25 is not being used to store the capture start index at this point.
    if (is_int16(cp_offset * char_size())) {
      __ addi(r25, current_input_offset(), Operand(cp_offset * char_size()));
    } else {
      __ mov(r25, Operand(cp_offset * char_size()));
      __ add(r25, r25, current_input_offset());
    }
    offset = r25;
  }
  // The lwz, stw, lhz, sth instructions can do unaligned accesses, if the CPU
  // and the operating system running on the target allow it.
  // We assume we don't want to do unaligned loads on PPC, so this function
  // must only be used to load a single character at a time.

  __ add(current_character(), end_of_input_address(), offset);
#if V8_TARGET_LITTLE_ENDIAN
  if (mode_ == LATIN1) {
    if (characters == 4) {
      __ lwz(current_character(), MemOperand(current_character()));
    } else if (characters == 2) {
      __ lhz(current_character(), MemOperand(current_character()));
    } else {
      DCHECK_EQ(1, characters);
      __ lbz(current_character(), MemOperand(current_character()));
    }
  } else {
    DCHECK(mode_ == UC16);
    if (characters == 2) {
      __ lwz(current_character(), MemOperand(current_character()));
    } else {
      DCHECK_EQ(1, characters);
      __ lhz(current_character(), MemOperand(current_character()));
    }
  }
#else
  if (mode_ == LATIN1) {
    if (characters == 4) {
      __ lwbrx(current_character(), MemOperand(r0, current_character()));
    } else if (characters == 2) {
      __ lhbrx(current_character(), MemOperand(r0, current_character()));
    } else {
      DCHECK_EQ(1, characters);
      __ lbz(current_character(), MemOperand(current_character()));
    }
  } else {
    DCHECK(mode_ == UC16);
    if (characters == 2) {
      __ lwz(current_character(), MemOperand(current_character()));
      __ rlwinm(current_character(), current_character(), 16, 0, 31);
    } else {
      DCHECK_EQ(1, characters);
      __ lhz(current_character(), MemOperand(current_character()));
    }
  }
#endif
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  //  V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

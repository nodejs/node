// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_RISCV64

#include "src/regexp/riscv64/regexp-macro-assembler-riscv64.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/logging/log.h"
#include "src/objects/objects-inl.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/strings/unicode.h"

namespace v8 {
namespace internal {

/* clang-format off
 *
 * This assembler uses the following register assignment convention
 * - t4 : Temporarily stores the index of capture start after a matching pass
 *        for a global regexp.
 * - a5 : Pointer to current Code object including heap object tag.
 * - a6 : Current position in input, as negative offset from end of string.
 *        Please notice that this is the byte offset, not the character offset!
 * - a7 : Currently loaded character. Must be loaded using
 *        LoadCurrentCharacter before using any of the dispatch methods.
 * - t0 : Points to tip of backtrack stack
 * - t1 : Unused.
 * - t2 : End of input (points to byte after last character in input).
 * - fp : Frame pointer. Used to access arguments, local variables and
 *         RegExp registers.
 * - sp : Points to tip of C stack.
 *
 * The remaining registers are free for computations.
 * Each call to a public method should retain this convention.
 *
 * The stack will have the following structure:
 *
 *  - fp[80]  Isolate* isolate   (address of the current isolate)               kIsolate
 *                                                                              kStackFrameHeader
 *  --- sp when called ---
 *  - fp[72]  ra                 Return from RegExp code (ra).                  kReturnAddress
 *  - fp[64]  s9, old-fp         Old fp, callee saved(s9).
 *  - fp[0..63]  fp..s7          Callee-saved registers fp..s7.
 *  --- frame pointer ----
 *  - fp[-8]  direct_call        (1 = direct call from JS, 0 = from runtime)    kDirectCall
 *  - fp[-16] stack_base         (Top of backtracking stack).                   kStackHighEnd
 *  - fp[-24] capture array size (may fit multiple sets of matches)             kNumOutputRegisters
 *  - fp[-32] int* capture_array (int[num_saved_registers_], for output).       kRegisterOutput
 *  - fp[-40] end of input       (address of end of string).                    kInputEnd
 *  - fp[-48] start of input     (address of first character in string).        kInputStart
 *  - fp[-56] start index        (character index of start).                    kStartIndex
 *  - fp[-64] void* input_string (location of a handle containing the string).  kInputString
 *  - fp[-72] success counter    (only for global regexps to count matches).    kSuccessfulCaptures
 *  - fp[-80] Offset of location before start of input (effectively character   kStringStartMinusOne
 *            position -1). Used to initialize capture registers to a
 *            non-position.
 *  --------- The following output registers are 32-bit values. ---------
 *  - fp[-88] register 0         (Only positions must be stored in the first    kRegisterZero
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
 *              Isolate* isolate);
 * The call is performed by NativeRegExpMacroAssembler::Execute()
 * (in regexp-macro-assembler.cc) via the GeneratedCode wrapper.
 *
 * clang-format on
 */

#define __ ACCESS_MASM(masm_)

const int RegExpMacroAssemblerRISCV::kRegExpCodeSize;

RegExpMacroAssemblerRISCV::RegExpMacroAssemblerRISCV(Isolate* isolate,
                                                     Zone* zone, Mode mode,
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
  __ jmp(&entry_label_);  // We'll write the entry code later.
  // If the code gets too big or corrupted, an internal exception will be
  // raised, and we will exit right away.
  __ bind(&internal_failure_label_);
  __ li(a0, Operand(FAILURE));
  __ Ret();
  __ bind(&start_label_);  // And then continue from here.
}

RegExpMacroAssemblerRISCV::~RegExpMacroAssemblerRISCV() {
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
  fallback_label_.Unuse();
}

int RegExpMacroAssemblerRISCV::stack_limit_slack() {
  return RegExpStack::kStackLimitSlack;
}

void RegExpMacroAssemblerRISCV::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    __ Add64(current_input_offset(), current_input_offset(),
             Operand(by * char_size()));
  }
}

void RegExpMacroAssemblerRISCV::AdvanceRegister(int reg, int by) {
  DCHECK_LE(0, reg);
  DCHECK_GT(num_registers_, reg);
  if (by != 0) {
    __ Ld(a0, register_location(reg));
    __ Add64(a0, a0, Operand(by));
    __ Sd(a0, register_location(reg));
  }
}

void RegExpMacroAssemblerRISCV::Backtrack() {
  CheckPreemption();
  if (has_backtrack_limit()) {
    Label next;
    __ Ld(a0, MemOperand(frame_pointer(), kBacktrackCount));
    __ Add64(a0, a0, Operand(1));
    __ Sd(a0, MemOperand(frame_pointer(), kBacktrackCount));
    __ Branch(&next, ne, a0, Operand(backtrack_limit()));

    // Backtrack limit exceeded.
    if (can_fallback()) {
      __ jmp(&fallback_label_);
    } else {
      // Can't fallback, so we treat it as a failed match.
      Fail();
    }

    __ bind(&next);
  }
  // Pop Code offset from backtrack stack, add Code and jump to location.
  Pop(a0);
  __ Add64(a0, a0, code_pointer());
  __ Jump(a0);
}

void RegExpMacroAssemblerRISCV::Bind(Label* label) { __ bind(label); }

void RegExpMacroAssemblerRISCV::CheckCharacter(uint32_t c, Label* on_equal) {
  BranchOrBacktrack(on_equal, eq, current_character(), Operand(c));
}

void RegExpMacroAssemblerRISCV::CheckCharacterGT(uc16 limit,
                                                 Label* on_greater) {
  BranchOrBacktrack(on_greater, gt, current_character(), Operand(limit));
}

void RegExpMacroAssemblerRISCV::CheckAtStart(int cp_offset,
                                             Label* on_at_start) {
  __ Ld(a1, MemOperand(frame_pointer(), kStringStartMinusOne));
  __ Add64(a0, current_input_offset(),
           Operand(-char_size() + cp_offset * char_size()));
  BranchOrBacktrack(on_at_start, eq, a0, Operand(a1));
}

void RegExpMacroAssemblerRISCV::CheckNotAtStart(int cp_offset,
                                                Label* on_not_at_start) {
  __ Ld(a1, MemOperand(frame_pointer(), kStringStartMinusOne));
  __ Add64(a0, current_input_offset(),
           Operand(-char_size() + cp_offset * char_size()));
  BranchOrBacktrack(on_not_at_start, ne, a0, Operand(a1));
}

void RegExpMacroAssemblerRISCV::CheckCharacterLT(uc16 limit, Label* on_less) {
  BranchOrBacktrack(on_less, lt, current_character(), Operand(limit));
}

void RegExpMacroAssemblerRISCV::CheckGreedyLoop(Label* on_equal) {
  Label backtrack_non_equal;
  __ Lw(a0, MemOperand(backtrack_stackpointer(), 0));
  __ Branch(&backtrack_non_equal, ne, current_input_offset(), Operand(a0));
  __ Add64(backtrack_stackpointer(), backtrack_stackpointer(),
           Operand(kIntSize));
  __ bind(&backtrack_non_equal);
  BranchOrBacktrack(on_equal, eq, current_input_offset(), Operand(a0));
}

void RegExpMacroAssemblerRISCV::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, bool unicode, Label* on_no_match) {
  Label fallthrough;
  __ Ld(a0, register_location(start_reg));      // Index of start of capture.
  __ Ld(a1, register_location(start_reg + 1));  // Index of end of capture.
  __ Sub64(a1, a1, a0);                         // Length of capture.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ Branch(&fallthrough, eq, a1, Operand(zero_reg));

  if (read_backward) {
    __ Ld(t1, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ Add64(t1, t1, a1);
    BranchOrBacktrack(on_no_match, le, current_input_offset(), Operand(t1));
  } else {
    __ Add64(t1, a1, current_input_offset());
    // Check that there are enough characters left in the input.
    BranchOrBacktrack(on_no_match, gt, t1, Operand(zero_reg));
  }

  if (mode_ == LATIN1) {
    Label success;
    Label fail;
    Label loop_check;

    // a0 - offset of start of capture.
    // a1 - length of capture.
    __ Add64(a0, a0, Operand(end_of_input_address()));
    __ Add64(a2, end_of_input_address(), Operand(current_input_offset()));
    if (read_backward) {
      __ Sub64(a2, a2, Operand(a1));
    }
    __ Add64(a1, a0, Operand(a1));

    // a0 - Address of start of capture.
    // a1 - Address of end of capture.
    // a2 - Address of current input position.

    Label loop;
    __ bind(&loop);
    __ Lbu(a3, MemOperand(a0, 0));
    __ addi(a0, a0, char_size());
    __ Lbu(a4, MemOperand(a2, 0));
    __ addi(a2, a2, char_size());

    __ Branch(&loop_check, eq, a4, Operand(a3));

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ Or(a3, a3, Operand(0x20));  // Convert capture character to lower-case.
    __ Or(a4, a4, Operand(0x20));  // Also convert input character.
    __ Branch(&fail, ne, a4, Operand(a3));
    __ Sub64(a3, a3, Operand('a'));
    __ Branch(&loop_check, Uless_equal, a3, Operand('z' - 'a'));
    // Latin-1: Check for values in range [224,254] but not 247.
    __ Sub64(a3, a3, Operand(224 - 'a'));
    // Weren't Latin-1 letters.
    __ Branch(&fail, Ugreater, a3, Operand(254 - 224));
    // Check for 247.
    __ Branch(&fail, eq, a3, Operand(247 - 224));

    __ bind(&loop_check);
    __ Branch(&loop, lt, a0, Operand(a1));
    __ jmp(&success);

    __ bind(&fail);
    GoTo(on_no_match);

    __ bind(&success);
    // Compute new value of character position after the matched part.
    __ Sub64(current_input_offset(), a2, end_of_input_address());
    if (read_backward) {
      __ Ld(t1, register_location(start_reg));  // Index of start of capture.
      __ Ld(a2, register_location(start_reg + 1));  // Index of end of capture.
      __ Add64(current_input_offset(), current_input_offset(), Operand(t1));
      __ Sub64(current_input_offset(), current_input_offset(), Operand(a2));
    }
  } else {
    DCHECK(mode_ == UC16);
    // Put regexp engine registers on stack.
    RegList regexp_registers_to_retain = current_input_offset().bit() |
                                         current_character().bit() |
                                         backtrack_stackpointer().bit();
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
    __ Add64(a0, a0, Operand(end_of_input_address()));
    // Length of capture.
    __ mv(a2, a1);
    // Save length in callee-save register for use on return.
    __ mv(s3, a1);
    // Address of current input position.
    __ Add64(a1, current_input_offset(), Operand(end_of_input_address()));
    if (read_backward) {
      __ Sub64(a1, a1, Operand(s3));
    }
    // Isolate.
    __ li(a3, Operand(ExternalReference::isolate_address(masm_->isolate())));

    {
      AllowExternalCallThatCantCauseGC scope(masm_);
      ExternalReference function =
          unicode ? ExternalReference::re_case_insensitive_compare_unicode(
                        isolate())
                  : ExternalReference::re_case_insensitive_compare_non_unicode(
                        isolate());
      __ CallCFunction(function, argument_count);
    }

    // Restore regexp engine registers.
    __ MultiPop(regexp_registers_to_retain);
    __ li(code_pointer(), Operand(masm_->CodeObject()), CONSTANT_SIZE);
    __ Ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));

    // Check if function returned non-zero for success or zero for failure.
    BranchOrBacktrack(on_no_match, eq, a0, Operand(zero_reg));
    // On success, increment position by length of capture.
    if (read_backward) {
      __ Sub64(current_input_offset(), current_input_offset(), Operand(s3));
    } else {
      __ Add64(current_input_offset(), current_input_offset(), Operand(s3));
    }
  }

  __ bind(&fallthrough);
}

void RegExpMacroAssemblerRISCV::CheckNotBackReference(int start_reg,
                                                      bool read_backward,
                                                      Label* on_no_match) {
  Label fallthrough;

  // Find length of back-referenced capture.
  __ Ld(a0, register_location(start_reg));
  __ Ld(a1, register_location(start_reg + 1));
  __ Sub64(a1, a1, a0);  // Length to check.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ Branch(&fallthrough, eq, a1, Operand(zero_reg));

  if (read_backward) {
    __ Ld(t1, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ Add64(t1, t1, a1);
    BranchOrBacktrack(on_no_match, le, current_input_offset(), Operand(t1));
  } else {
    __ Add64(t1, a1, current_input_offset());
    // Check that there are enough characters left in the input.
    BranchOrBacktrack(on_no_match, gt, t1, Operand(zero_reg));
  }

  // Compute pointers to match string and capture string.
  __ Add64(a0, a0, Operand(end_of_input_address()));
  __ Add64(a2, end_of_input_address(), Operand(current_input_offset()));
  if (read_backward) {
    __ Sub64(a2, a2, Operand(a1));
  }
  __ Add64(a1, a1, Operand(a0));

  Label loop;
  __ bind(&loop);
  if (mode_ == LATIN1) {
    __ Lbu(a3, MemOperand(a0, 0));
    __ addi(a0, a0, char_size());
    __ Lbu(a4, MemOperand(a2, 0));
    __ addi(a2, a2, char_size());
  } else {
    DCHECK(mode_ == UC16);
    __ Lhu(a3, MemOperand(a0, 0));
    __ addi(a0, a0, char_size());
    __ Lhu(a4, MemOperand(a2, 0));
    __ addi(a2, a2, char_size());
  }
  BranchOrBacktrack(on_no_match, ne, a3, Operand(a4));
  __ Branch(&loop, lt, a0, Operand(a1));

  // Move current character position to position after match.
  __ Sub64(current_input_offset(), a2, end_of_input_address());
  if (read_backward) {
    __ Ld(t1, register_location(start_reg));      // Index of start of capture.
    __ Ld(a2, register_location(start_reg + 1));  // Index of end of capture.
    __ Add64(current_input_offset(), current_input_offset(), Operand(t1));
    __ Sub64(current_input_offset(), current_input_offset(), Operand(a2));
  }
  __ bind(&fallthrough);
}

void RegExpMacroAssemblerRISCV::CheckNotCharacter(uint32_t c,
                                                  Label* on_not_equal) {
  BranchOrBacktrack(on_not_equal, ne, current_character(), Operand(c));
}

void RegExpMacroAssemblerRISCV::CheckCharacterAfterAnd(uint32_t c,
                                                       uint32_t mask,
                                                       Label* on_equal) {
  __ And(a0, current_character(), Operand(mask));
  Operand rhs = (c == 0) ? Operand(zero_reg) : Operand(c);
  BranchOrBacktrack(on_equal, eq, a0, rhs);
}

void RegExpMacroAssemblerRISCV::CheckNotCharacterAfterAnd(uint32_t c,
                                                          uint32_t mask,
                                                          Label* on_not_equal) {
  __ And(a0, current_character(), Operand(mask));
  Operand rhs = (c == 0) ? Operand(zero_reg) : Operand(c);
  BranchOrBacktrack(on_not_equal, ne, a0, rhs);
}

void RegExpMacroAssemblerRISCV::CheckNotCharacterAfterMinusAnd(
    uc16 c, uc16 minus, uc16 mask, Label* on_not_equal) {
  DCHECK_GT(String::kMaxUtf16CodeUnit, minus);
  __ Sub64(a0, current_character(), Operand(minus));
  __ And(a0, a0, Operand(mask));
  BranchOrBacktrack(on_not_equal, ne, a0, Operand(c));
}

void RegExpMacroAssemblerRISCV::CheckCharacterInRange(uc16 from, uc16 to,
                                                      Label* on_in_range) {
  __ Sub64(a0, current_character(), Operand(from));
  // Unsigned lower-or-same condition.
  BranchOrBacktrack(on_in_range, Uless_equal, a0, Operand(to - from));
}

void RegExpMacroAssemblerRISCV::CheckCharacterNotInRange(
    uc16 from, uc16 to, Label* on_not_in_range) {
  __ Sub64(a0, current_character(), Operand(from));
  // Unsigned higher condition.
  BranchOrBacktrack(on_not_in_range, Ugreater, a0, Operand(to - from));
}

void RegExpMacroAssemblerRISCV::CheckBitInTable(Handle<ByteArray> table,
                                                Label* on_bit_set) {
  __ li(a0, Operand(table));
  if (mode_ != LATIN1 || kTableMask != String::kMaxOneByteCharCode) {
    __ And(a1, current_character(), Operand(kTableSize - 1));
    __ Add64(a0, a0, a1);
  } else {
    __ Add64(a0, a0, current_character());
  }

  __ Lbu(a0, FieldMemOperand(a0, ByteArray::kHeaderSize));
  BranchOrBacktrack(on_bit_set, ne, a0, Operand(zero_reg));
}

bool RegExpMacroAssemblerRISCV::CheckSpecialCharacterClass(uc16 type,
                                                           Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check.
  switch (type) {
    case 's':
      // Match space-characters.
      if (mode_ == LATIN1) {
        // One byte space characters are '\t'..'\r', ' ' and \u00a0.
        Label success;
        __ Branch(&success, eq, current_character(), Operand(' '));
        // Check range 0x09..0x0D.
        __ Sub64(a0, current_character(), Operand('\t'));
        __ Branch(&success, Uless_equal, a0, Operand('\r' - '\t'));
        // \u00a0 (NBSP).
        BranchOrBacktrack(on_no_match, ne, a0, Operand(0x00A0 - '\t'));
        __ bind(&success);
        return true;
      }
      return false;
    case 'S':
      // The emitted code for generic character classes is good enough.
      return false;
    case 'd':
      // Match Latin1 digits ('0'..'9').
      __ Sub64(a0, current_character(), Operand('0'));
      BranchOrBacktrack(on_no_match, Ugreater, a0, Operand('9' - '0'));
      return true;
    case 'D':
      // Match non Latin1-digits.
      __ Sub64(a0, current_character(), Operand('0'));
      BranchOrBacktrack(on_no_match, Uless_equal, a0, Operand('9' - '0'));
      return true;
    case '.': {
      // Match non-newlines (not 0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029).
      __ Xor(a0, current_character(), Operand(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C.
      __ Sub64(a0, a0, Operand(0x0B));
      BranchOrBacktrack(on_no_match, Uless_equal, a0, Operand(0x0C - 0x0B));
      if (mode_ == UC16) {
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ Sub64(a0, a0, Operand(0x2028 - 0x0B));
        BranchOrBacktrack(on_no_match, Uless_equal, a0, Operand(1));
      }
      return true;
    }
    case 'n': {
      // Match newlines (0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029).
      __ Xor(a0, current_character(), Operand(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C.
      __ Sub64(a0, a0, Operand(0x0B));
      if (mode_ == LATIN1) {
        BranchOrBacktrack(on_no_match, Ugreater, a0, Operand(0x0C - 0x0B));
      } else {
        Label done;
        BranchOrBacktrack(&done, Uless_equal, a0, Operand(0x0C - 0x0B));
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ Sub64(a0, a0, Operand(0x2028 - 0x0B));
        BranchOrBacktrack(on_no_match, Ugreater, a0, Operand(1));
        __ bind(&done);
      }
      return true;
    }
    case 'w': {
      if (mode_ != LATIN1) {
        // Table is 256 entries, so all Latin1 characters can be tested.
        BranchOrBacktrack(on_no_match, Ugreater, current_character(),
                          Operand('z'));
      }
      ExternalReference map =
          ExternalReference::re_word_character_map(isolate());
      __ li(a0, Operand(map));
      __ Add64(a0, a0, current_character());
      __ Lbu(a0, MemOperand(a0, 0));
      BranchOrBacktrack(on_no_match, eq, a0, Operand(zero_reg));
      return true;
    }
    case 'W': {
      Label done;
      if (mode_ != LATIN1) {
        // Table is 256 entries, so all Latin1 characters can be tested.
        __ Branch(&done, Ugreater, current_character(), Operand('z'));
      }
      ExternalReference map =
          ExternalReference::re_word_character_map(isolate());
      __ li(a0, Operand(map));
      __ Add64(a0, a0, current_character());
      __ Lbu(a0, MemOperand(a0, 0));
      BranchOrBacktrack(on_no_match, ne, a0, Operand(zero_reg));
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

void RegExpMacroAssemblerRISCV::Fail() {
  __ li(a0, Operand(FAILURE));
  __ jmp(&exit_label_);
}

Handle<HeapObject> RegExpMacroAssemblerRISCV::GetCode(Handle<String> source) {
  Label return_a0;
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

    // Tell the system that we have a stack frame.  Because the type is MANUAL,
    // no is generated.
    FrameScope scope(masm_, StackFrame::MANUAL);

    // Actually emit code to start a new stack frame.
    // Push arguments
    // Save callee-save registers.
    // Start new stack frame.
    // Store link register in existing stack-cell.
    // Order here should correspond to order of offset constants in header file.
    // TODO(plind): we save fp..s11, but ONLY use s3 here - use the regs
    // or dont save.
    RegList registers_to_retain =
        fp.bit() | s1.bit() | s2.bit() | s3.bit() | s4.bit() | s5.bit() |
        s6.bit() | s7.bit() | s8.bit() /*| s9.bit() | s10.bit() | s11.bit()*/;
    DCHECK(NumRegs(registers_to_retain) == kNumCalleeRegsToRetain);

    // The remaining arguments are passed in registers, e.g.by calling the code
    // entry as cast to a function with the signature:
    //
    // *int(*match)(String input_string,      // a0
    //             int start_index,           // a1
    //             Address start,             // a2
    //             Address end,               // a3
    //             int*capture_output_array,  // a4
    //             int num_capture_registers, // a5
    //             byte* stack_area_base,     // a6
    //             bool direct_call = false,  // a7
    //             Isolate * isolate);        // on the stack
    RegList argument_registers = a0.bit() | a1.bit() | a2.bit() | a3.bit() |
                                 a4.bit() | a5.bit() | a6.bit() | a7.bit();

    // According to MultiPush implementation, registers will be pushed in the
    // order of ra, fp, then s8, ..., s1, and finally a7,...a0
    __ MultiPush(ra.bit() | registers_to_retain | argument_registers);

    // Set frame pointer in space for it if this is not a direct call
    // from generated code.
    __ Add64(frame_pointer(), sp,
             Operand(NumRegs(argument_registers) * kPointerSize));

    STATIC_ASSERT(kSuccessfulCaptures == kInputString - kSystemPointerSize);
    __ mv(a0, zero_reg);
    __ push(a0);  // Make room for success counter and initialize it to 0.
    STATIC_ASSERT(kStringStartMinusOne ==
                  kSuccessfulCaptures - kSystemPointerSize);
    __ push(a0);  // Make room for "string start - 1" constant.
    STATIC_ASSERT(kBacktrackCount == kStringStartMinusOne - kSystemPointerSize);
    __ push(a0);  // The backtrack counter

    // Check if we have space on the stack for registers.
    Label stack_limit_hit;
    Label stack_ok;

    ExternalReference stack_limit =
        ExternalReference::address_of_jslimit(masm_->isolate());
    __ li(a0, Operand(stack_limit));
    __ Ld(a0, MemOperand(a0));
    __ Sub64(a0, sp, a0);
    // Handle it if the stack pointer is already below the stack limit.
    __ Branch(&stack_limit_hit, le, a0, Operand(zero_reg));
    // Check if there is room for the variable number of registers above
    // the stack limit.
    __ Branch(&stack_ok, Ugreater_equal, a0,
              Operand(num_registers_ * kPointerSize));
    // Exit with OutOfMemory exception. There is not enough space on the stack
    // for our working registers.
    __ li(a0, Operand(EXCEPTION));
    __ jmp(&return_a0);

    __ bind(&stack_limit_hit);
    CallCheckStackGuardState(a0);
    // If returned value is non-zero, we exit with the returned value as result.
    __ Branch(&return_a0, ne, a0, Operand(zero_reg));

    __ bind(&stack_ok);
    // Allocate space on stack for registers.
    __ Sub64(sp, sp, Operand(num_registers_ * kPointerSize));
    // Load string end.
    __ Ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
    // Load input start.
    __ Ld(a0, MemOperand(frame_pointer(), kInputStart));
    // Find negative length (offset of start relative to end).
    __ Sub64(current_input_offset(), a0, end_of_input_address());
    // Set a0 to address of char before start of the input string
    // (effectively string position -1).
    __ Ld(a1, MemOperand(frame_pointer(), kStartIndex));
    __ Sub64(a0, current_input_offset(), Operand(char_size()));
    __ slli(t1, a1, (mode_ == UC16) ? 1 : 0);
    __ Sub64(a0, a0, t1);
    // Store this value in a local variable, for use when clearing
    // position registers.
    __ Sd(a0, MemOperand(frame_pointer(), kStringStartMinusOne));

    // Initialize code pointer register
    __ li(code_pointer(), Operand(masm_->CodeObject()), CONSTANT_SIZE);

    Label load_char_start_regexp, start_regexp;
    // Load newline if index is at start, previous character otherwise.
    __ Branch(&load_char_start_regexp, ne, a1, Operand(zero_reg));
    __ li(current_character(), Operand('\n'));
    __ jmp(&start_regexp);

    // Global regexp restarts matching here.
    __ bind(&load_char_start_regexp);
    // Load previous char as initial value of current character register.
    LoadCurrentCharacterUnchecked(-1, 1);
    __ bind(&start_regexp);

    // Initialize on-stack registers.
    if (num_saved_registers_ > 0) {  // Always is, if generated from a regexp.
      // Fill saved registers with initial value = start offset - 1.
      if (num_saved_registers_ > 8) {
        // Address of register 0.
        __ Add64(a1, frame_pointer(), Operand(kRegisterZero));
        __ li(a2, Operand(num_saved_registers_));
        Label init_loop;
        __ bind(&init_loop);
        __ Sd(a0, MemOperand(a1));
        __ Add64(a1, a1, Operand(-kPointerSize));
        __ Sub64(a2, a2, Operand(1));
        __ Branch(&init_loop, ne, a2, Operand(zero_reg));
      } else {
        for (int i = 0; i < num_saved_registers_; i++) {
          __ Sd(a0, register_location(i));
        }
      }
    }

    // Initialize backtrack stack pointer.
    __ Ld(backtrack_stackpointer(), MemOperand(frame_pointer(), kStackHighEnd));

    __ jmp(&start_label_);

    // Exit code:
    if (success_label_.is_linked()) {
      // Save captures when successful.
      __ bind(&success_label_);
      if (num_saved_registers_ > 0) {
        // Copy captures to output.
        __ Ld(a1, MemOperand(frame_pointer(), kInputStart));
        __ Ld(a0, MemOperand(frame_pointer(), kRegisterOutput));
        __ Ld(a2, MemOperand(frame_pointer(), kStartIndex));
        __ Sub64(a1, end_of_input_address(), a1);
        // a1 is length of input in bytes.
        if (mode_ == UC16) {
          __ srli(a1, a1, 1);
        }
        // a1 is length of input in characters.
        __ Add64(a1, a1, Operand(a2));
        // a1 is length of string in characters.

        DCHECK_EQ(0, num_saved_registers_ % 2);
        // Always an even number of capture registers. This allows us to
        // unroll the loop once to add an operation between a load of a register
        // and the following use of that register.
        for (int i = 0; i < num_saved_registers_; i += 2) {
          __ Ld(a2, register_location(i));
          __ Ld(a3, register_location(i + 1));
          if (i == 0 && global_with_zero_length_check()) {
            // Keep capture start in a4 for the zero-length check later.
            __ mv(t4, a2);
          }
          if (mode_ == UC16) {
            __ srai(a2, a2, 1);
            __ Add64(a2, a2, a1);
            __ srai(a3, a3, 1);
            __ Add64(a3, a3, a1);
          } else {
            __ Add64(a2, a1, Operand(a2));
            __ Add64(a3, a1, Operand(a3));
          }
          // V8 expects the output to be an int32_t array.
          __ Sw(a2, MemOperand(a0));
          __ Add64(a0, a0, kIntSize);
          __ Sw(a3, MemOperand(a0));
          __ Add64(a0, a0, kIntSize);
        }
      }

      if (global()) {
        // Restart matching if the regular expression is flagged as global.
        __ Ld(a0, MemOperand(frame_pointer(), kSuccessfulCaptures));
        __ Ld(a1, MemOperand(frame_pointer(), kNumOutputRegisters));
        __ Ld(a2, MemOperand(frame_pointer(), kRegisterOutput));
        // Increment success counter.
        __ Add64(a0, a0, 1);
        __ Sd(a0, MemOperand(frame_pointer(), kSuccessfulCaptures));
        // Capture results have been stored, so the number of remaining global
        // output registers is reduced by the number of stored captures.
        __ Sub64(a1, a1, num_saved_registers_);
        // Check whether we have enough room for another set of capture results.
        __ Branch(&return_a0, lt, a1, Operand(num_saved_registers_));

        __ Sd(a1, MemOperand(frame_pointer(), kNumOutputRegisters));
        // Advance the location for output.
        __ Add64(a2, a2, num_saved_registers_ * kIntSize);
        __ Sd(a2, MemOperand(frame_pointer(), kRegisterOutput));

        // Prepare a0 to initialize registers with its value in the next run.
        __ Ld(a0, MemOperand(frame_pointer(), kStringStartMinusOne));

        if (global_with_zero_length_check()) {
          // Special case for zero-length matches.
          // t4: capture start index
          // Not a zero-length match, restart.
          __ Branch(&load_char_start_regexp, ne, current_input_offset(),
                    Operand(t4));
          // Offset from the end is zero if we already reached the end.
          __ Branch(&exit_label_, eq, current_input_offset(),
                    Operand(zero_reg));
          // Advance current position after a zero-length match.
          Label advance;
          __ bind(&advance);
          __ Add64(current_input_offset(), current_input_offset(),
                   Operand((mode_ == UC16) ? 2 : 1));
          if (global_unicode()) CheckNotInSurrogatePair(0, &advance);
        }

        __ Branch(&load_char_start_regexp);
      } else {
        __ li(a0, Operand(SUCCESS));
      }
    }
    // Exit and return a0.
    __ bind(&exit_label_);
    if (global()) {
      __ Ld(a0, MemOperand(frame_pointer(), kSuccessfulCaptures));
    }

    __ bind(&return_a0);
    // Skip sp past regexp registers and local variables..
    __ mv(sp, frame_pointer());

    // Restore registers fp..s11 and return (restoring ra to pc).
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
                                           current_character().bit() |
                                           backtrack_stackpointer().bit();
      __ MultiPush(regexp_registers_to_retain);
      CallCheckStackGuardState(a0);
      __ MultiPop(regexp_registers_to_retain);
      // If returning non-zero, we should end execution with the given
      // result as return value.
      __ Branch(&return_a0, ne, a0, Operand(zero_reg));

      // String might have moved: Reload end of string from frame.
      __ Ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
      __ li(code_pointer(), Operand(masm_->CodeObject()), CONSTANT_SIZE);
      SafeReturn();
    }

    // Backtrack stack overflow code.
    if (stack_overflow_label_.is_linked()) {
      SafeCallTarget(&stack_overflow_label_);
      // Reached if the backtrack-stack limit has been hit.
      // Put regexp engine registers on stack first.
      RegList regexp_registers =
          current_input_offset().bit() | current_character().bit();
      __ MultiPush(regexp_registers);

      // Call GrowStack(backtrack_stackpointer(), &stack_base)
      static const int num_arguments = 3;
      __ PrepareCallCFunction(num_arguments, a0);
      __ mv(a0, backtrack_stackpointer());
      __ Add64(a1, frame_pointer(), Operand(kStackHighEnd));
      __ li(a2, Operand(ExternalReference::isolate_address(masm_->isolate())));
      ExternalReference grow_stack =
          ExternalReference::re_grow_stack(masm_->isolate());
      __ CallCFunction(grow_stack, num_arguments);
      // Restore regexp registers.
      __ MultiPop(regexp_registers);
      // If return nullptr, we have failed to grow the stack, and
      // must exit with a stack-overflow exception.
      __ Branch(&exit_with_exception, eq, a0, Operand(zero_reg));
      // Otherwise use return value as new stack pointer.
      __ mv(backtrack_stackpointer(), a0);
      // Restore saved registers and continue.
      __ li(code_pointer(), Operand(masm_->CodeObject()), CONSTANT_SIZE);
      __ Ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
      SafeReturn();
    }

    if (exit_with_exception.is_linked()) {
      // If any of the code above needed to exit with an exception.
      __ bind(&exit_with_exception);
      // Exit with Result EXCEPTION(-1) to signal thrown exception.
      __ li(a0, Operand(EXCEPTION));
      __ jmp(&return_a0);
    }

    if (fallback_label_.is_linked()) {
      __ bind(&fallback_label_);
      __ li(a0, Operand(FALLBACK_TO_EXPERIMENTAL));
      __ jmp(&return_a0);
    }
  }

  CodeDesc code_desc;
  masm_->GetCode(isolate(), &code_desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate(), code_desc, CodeKind::REGEXP)
          .set_self_reference(masm_->CodeObject())
          .Build();
  LOG(masm_->isolate(),
      RegExpCodeCreateEvent(Handle<AbstractCode>::cast(code), source));
  return Handle<HeapObject>::cast(code);
}

void RegExpMacroAssemblerRISCV::GoTo(Label* to) {
  if (to == nullptr) {
    Backtrack();
    return;
  }
  __ jmp(to);
  return;
}

void RegExpMacroAssemblerRISCV::IfRegisterGE(int reg, int comparand,
                                             Label* if_ge) {
  __ Ld(a0, register_location(reg));
  BranchOrBacktrack(if_ge, ge, a0, Operand(comparand));
}

void RegExpMacroAssemblerRISCV::IfRegisterLT(int reg, int comparand,
                                             Label* if_lt) {
  __ Ld(a0, register_location(reg));
  BranchOrBacktrack(if_lt, lt, a0, Operand(comparand));
}

void RegExpMacroAssemblerRISCV::IfRegisterEqPos(int reg, Label* if_eq) {
  __ Ld(a0, register_location(reg));
  BranchOrBacktrack(if_eq, eq, a0, Operand(current_input_offset()));
}

RegExpMacroAssembler::IrregexpImplementation
RegExpMacroAssemblerRISCV::Implementation() {
  return kRISCVImplementation;
}

void RegExpMacroAssemblerRISCV::PopCurrentPosition() {
  Pop(current_input_offset());
}

void RegExpMacroAssemblerRISCV::PopRegister(int register_index) {
  Pop(a0);
  __ Sd(a0, register_location(register_index));
}

void RegExpMacroAssemblerRISCV::PushBacktrack(Label* label) {
  if (label->is_bound()) {
    int target = label->pos();
    __ li(a0, Operand(target + Code::kHeaderSize - kHeapObjectTag));
  } else {
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm_);
    Label after_constant;
    __ Branch(&after_constant);
    int offset = masm_->pc_offset();
    int cp_offset = offset + Code::kHeaderSize - kHeapObjectTag;
    __ emit(0);
    masm_->label_at_put(label, offset);
    __ bind(&after_constant);
    if (is_int16(cp_offset)) {
      __ Lwu(a0, MemOperand(code_pointer(), cp_offset));
    } else {
      __ Add64(a0, code_pointer(), cp_offset);
      __ Lwu(a0, MemOperand(a0, 0));
    }
  }
  Push(a0);
  CheckStackLimit();
}

void RegExpMacroAssemblerRISCV::PushCurrentPosition() {
  Push(current_input_offset());
}

void RegExpMacroAssemblerRISCV::PushRegister(int register_index,
                                             StackCheckFlag check_stack_limit) {
  __ Ld(a0, register_location(register_index));
  Push(a0);
  if (check_stack_limit) CheckStackLimit();
}

void RegExpMacroAssemblerRISCV::ReadCurrentPositionFromRegister(int reg) {
  __ Ld(current_input_offset(), register_location(reg));
}

void RegExpMacroAssemblerRISCV::ReadStackPointerFromRegister(int reg) {
  __ Ld(backtrack_stackpointer(), register_location(reg));
  __ Ld(a0, MemOperand(frame_pointer(), kStackHighEnd));
  __ Add64(backtrack_stackpointer(), backtrack_stackpointer(), Operand(a0));
}

void RegExpMacroAssemblerRISCV::SetCurrentPositionFromEnd(int by) {
  Label after_position;
  __ Branch(&after_position, ge, current_input_offset(),
            Operand(-by * char_size()));
  __ li(current_input_offset(), -by * char_size());
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&after_position);
}

void RegExpMacroAssemblerRISCV::SetRegister(int register_index, int to) {
  DCHECK(register_index >= num_saved_registers_);  // Reserved for positions!
  __ li(a0, Operand(to));
  __ Sd(a0, register_location(register_index));
}

bool RegExpMacroAssemblerRISCV::Succeed() {
  __ jmp(&success_label_);
  return global();
}

void RegExpMacroAssemblerRISCV::WriteCurrentPositionToRegister(int reg,
                                                               int cp_offset) {
  if (cp_offset == 0) {
    __ Sd(current_input_offset(), register_location(reg));
  } else {
    __ Add64(a0, current_input_offset(), Operand(cp_offset * char_size()));
    __ Sd(a0, register_location(reg));
  }
}

void RegExpMacroAssemblerRISCV::ClearRegisters(int reg_from, int reg_to) {
  DCHECK(reg_from <= reg_to);
  __ Ld(a0, MemOperand(frame_pointer(), kStringStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ Sd(a0, register_location(reg));
  }
}

void RegExpMacroAssemblerRISCV::WriteStackPointerToRegister(int reg) {
  __ Ld(a1, MemOperand(frame_pointer(), kStackHighEnd));
  __ Sub64(a0, backtrack_stackpointer(), a1);
  __ Sd(a0, register_location(reg));
}

bool RegExpMacroAssemblerRISCV::CanReadUnaligned() { return false; }

// Private methods:

void RegExpMacroAssemblerRISCV::CallCheckStackGuardState(Register scratch) {
  DCHECK(!isolate()->IsGeneratingEmbeddedBuiltins());
  DCHECK(!masm_->options().isolate_independent_code);

  int stack_alignment = base::OS::ActivationFrameAlignment();

  // Align the stack pointer and save the original sp value on the stack.
  __ mv(scratch, sp);
  __ Sub64(sp, sp, Operand(kPointerSize));
  DCHECK(base::bits::IsPowerOfTwo(stack_alignment));
  __ And(sp, sp, Operand(-stack_alignment));
  __ Sd(scratch, MemOperand(sp));

  __ mv(a2, frame_pointer());
  // Code of self.
  __ li(a1, Operand(masm_->CodeObject()), CONSTANT_SIZE);

  // We need to make room for the return address on the stack.
  DCHECK(IsAligned(stack_alignment, kPointerSize));
  __ Sub64(sp, sp, Operand(stack_alignment));

  // The stack pointer now points to cell where the return address will be
  // written. Arguments are in registers, meaning we treat the return address as
  // argument 5. Since DirectCEntry will handle allocating space for the C
  // argument slots, we don't need to care about that here. This is how the
  // stack will look (sp meaning the value of sp at this moment):
  // [sp + 3] - empty slot if needed for alignment.
  // [sp + 2] - saved sp.
  // [sp + 1] - second word reserved for return value.
  // [sp + 0] - first word reserved for return value.

  // a0 will point to the return address, placed by DirectCEntry.
  __ mv(a0, sp);

  ExternalReference stack_guard_check =
      ExternalReference::re_check_stack_guard_state(masm_->isolate());
  __ li(t6, Operand(stack_guard_check));

  EmbeddedData d = EmbeddedData::FromBlob();
  CHECK(Builtins::IsIsolateIndependent(Builtins::kDirectCEntry));
  Address entry = d.InstructionStartOfBuiltin(Builtins::kDirectCEntry);
  __ li(kScratchReg, Operand(entry, RelocInfo::OFF_HEAP_TARGET));
  __ Call(kScratchReg);

  // DirectCEntry allocated space for the C argument slots so we have to
  // drop them with the return address from the stack with loading saved sp.
  // At this point stack must look:
  // [sp + 7] - empty slot if needed for alignment.
  // [sp + 6] - saved sp.
  // [sp + 5] - second word reserved for return value.
  // [sp + 4] - first word reserved for return value.
  // [sp + 3] - C argument slot.
  // [sp + 2] - C argument slot.
  // [sp + 1] - C argument slot.
  // [sp + 0] - C argument slot.
  __ Ld(sp, MemOperand(sp, stack_alignment + kCArgsSlotsSize));

  __ li(code_pointer(), Operand(masm_->CodeObject()));
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

int64_t RegExpMacroAssemblerRISCV::CheckStackGuardState(Address* return_address,
                                                        Address raw_code,
                                                        Address re_frame) {
  Code re_code = Code::cast(Object(raw_code));
  return NativeRegExpMacroAssembler::CheckStackGuardState(
      frame_entry<Isolate*>(re_frame, kIsolate),
      static_cast<int>(frame_entry<int64_t>(re_frame, kStartIndex)),
      static_cast<RegExp::CallOrigin>(
          frame_entry<int64_t>(re_frame, kDirectCall)),
      return_address, re_code,
      frame_entry_address<Address>(re_frame, kInputString),
      frame_entry_address<const byte*>(re_frame, kInputStart),
      frame_entry_address<const byte*>(re_frame, kInputEnd));
}

MemOperand RegExpMacroAssemblerRISCV::register_location(int register_index) {
  DCHECK(register_index < (1 << 30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return MemOperand(frame_pointer(),
                    kRegisterZero - register_index * kPointerSize);
}

void RegExpMacroAssemblerRISCV::CheckPosition(int cp_offset,
                                              Label* on_outside_input) {
  if (cp_offset >= 0) {
    BranchOrBacktrack(on_outside_input, ge, current_input_offset(),
                      Operand(-cp_offset * char_size()));
  } else {
    __ Ld(a1, MemOperand(frame_pointer(), kStringStartMinusOne));
    __ Add64(a0, current_input_offset(), Operand(cp_offset * char_size()));
    BranchOrBacktrack(on_outside_input, le, a0, Operand(a1));
  }
}

void RegExpMacroAssemblerRISCV::BranchOrBacktrack(Label* to,
                                                  Condition condition,
                                                  Register rs,
                                                  const Operand& rt) {
  if (condition == al) {  // Unconditional.
    if (to == nullptr) {
      Backtrack();
      return;
    }
    __ jmp(to);
    return;
  }
  if (to == nullptr) {
    __ Branch(&backtrack_label_, condition, rs, rt);
    return;
  }
  __ Branch(to, condition, rs, rt);
}

void RegExpMacroAssemblerRISCV::SafeCall(Label* to, Condition cond, Register rs,
                                         const Operand& rt) {
  __ BranchAndLink(to, cond, rs, rt);
}

void RegExpMacroAssemblerRISCV::SafeReturn() {
  __ pop(ra);
  __ Add64(t1, ra, Operand(masm_->CodeObject()));
  __ Jump(t1);
}

void RegExpMacroAssemblerRISCV::SafeCallTarget(Label* name) {
  __ bind(name);
  __ Sub64(ra, ra, Operand(masm_->CodeObject()));
  __ push(ra);
}

void RegExpMacroAssemblerRISCV::Push(Register source) {
  DCHECK(source != backtrack_stackpointer());
  __ Add64(backtrack_stackpointer(), backtrack_stackpointer(),
           Operand(-kIntSize));
  __ Sw(source, MemOperand(backtrack_stackpointer()));
}

void RegExpMacroAssemblerRISCV::Pop(Register target) {
  DCHECK(target != backtrack_stackpointer());
  __ Lw(target, MemOperand(backtrack_stackpointer()));
  __ Add64(backtrack_stackpointer(), backtrack_stackpointer(), kIntSize);
}

void RegExpMacroAssemblerRISCV::CheckPreemption() {
  // Check for preemption.
  ExternalReference stack_limit =
      ExternalReference::address_of_jslimit(masm_->isolate());
  __ li(a0, Operand(stack_limit));
  __ Ld(a0, MemOperand(a0));
  SafeCall(&check_preempt_label_, Uless_equal, sp, Operand(a0));
}

void RegExpMacroAssemblerRISCV::CheckStackLimit() {
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit_address(
          masm_->isolate());

  __ li(a0, Operand(stack_limit));
  __ Ld(a0, MemOperand(a0));
  SafeCall(&stack_overflow_label_, Uless_equal, backtrack_stackpointer(),
           Operand(a0));
}

void RegExpMacroAssemblerRISCV::LoadCurrentCharacterUnchecked(int cp_offset,
                                                              int characters) {
  Register offset = current_input_offset();
  if (cp_offset != 0) {
    // t4 is not being used to store the capture start index at this point.
    __ Add64(t4, current_input_offset(), Operand(cp_offset * char_size()));
    offset = t4;
  }
  // We assume that we cannot do unaligned loads on RISC-V, so this function
  // must only be used to load a single character at a time.
  DCHECK_EQ(1, characters);
  __ Add64(t1, end_of_input_address(), Operand(offset));
  if (mode_ == LATIN1) {
    __ Lbu(current_character(), MemOperand(t1, 0));
  } else {
    DCHECK(mode_ == UC16);
    __ Lhu(current_character(), MemOperand(t1, 0));
  }
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_RISCV64

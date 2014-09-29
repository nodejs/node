// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/code-stubs.h"
#include "src/log.h"
#include "src/macro-assembler.h"
#include "src/regexp-macro-assembler.h"
#include "src/regexp-stack.h"
#include "src/unicode.h"

#include "src/mips64/regexp-macro-assembler-mips64.h"

namespace v8 {
namespace internal {

#ifndef V8_INTERPRETED_REGEXP
/*
 * This assembler uses the following register assignment convention
 * - t3 : Temporarily stores the index of capture start after a matching pass
 *        for a global regexp.
 * - a5 : Pointer to current code object (Code*) including heap object tag.
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
 * TODO(plind): O32 documented here with intent of having single 32/64 codebase
 *              in the future.
 *
 * The O32 stack will have the following structure:
 *
 *  - fp[76]  Isolate* isolate   (address of the current isolate)
 *  - fp[72]  direct_call  (if 1, direct call from JavaScript code,
 *                          if 0, call through the runtime system).
 *  - fp[68]  stack_area_base (High end of the memory area to use as
 *                             backtracking stack).
 *  - fp[64]  capture array size (may fit multiple sets of matches)
 *  - fp[60]  int* capture_array (int[num_saved_registers_], for output).
 *  - fp[44..59]  MIPS O32 four argument slots
 *  - fp[40]  secondary link/return address used by native call.
 *  --- sp when called ---
 *  - fp[36]  return address      (lr).
 *  - fp[32]  old frame pointer   (r11).
 *  - fp[0..31]  backup of registers s0..s7.
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
 *
 * The N64 stack will have the following structure:
 *
 *  - fp[88]  Isolate* isolate   (address of the current isolate)               kIsolate
 *  - fp[80]  secondary link/return address used by exit frame on native call.  kSecondaryReturnAddress
                                                                                kStackFrameHeader
 *  --- sp when called ---
 *  - fp[72]  ra                 Return from RegExp code (ra).                  kReturnAddress
 *  - fp[64]  s9, old-fp         Old fp, callee saved(s9).
 *  - fp[0..63]  s0..s7          Callee-saved registers s0..s7.
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
 *  - fp[-80] Offset of location before start of input (effectively character   kInputStartMinusOne
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
 * int (*match)(String* input_string,
 *              int start_index,
 *              Address start,
 *              Address end,
 *              Address secondary_return_address,  // Only used by native call.
 *              int* capture_output_array,
 *              byte* stack_area_base,
 *              bool direct_call = false,
 *              void* return_address,
 *              Isolate* isolate);
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
      exit_label_(),
      internal_failure_label_() {
  DCHECK_EQ(0, registers_to_save % 2);
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
    __ Daddu(current_input_offset(),
            current_input_offset(), Operand(by * char_size()));
  }
}


void RegExpMacroAssemblerMIPS::AdvanceRegister(int reg, int by) {
  DCHECK(reg >= 0);
  DCHECK(reg < num_registers_);
  if (by != 0) {
    __ ld(a0, register_location(reg));
    __ Daddu(a0, a0, Operand(by));
    __ sd(a0, register_location(reg));
  }
}


void RegExpMacroAssemblerMIPS::Backtrack() {
  CheckPreemption();
  // Pop Code* offset from backtrack stack, add Code* and jump to location.
  Pop(a0);
  __ Daddu(a0, a0, code_pointer());
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
  __ lw(a0, MemOperand(frame_pointer(), kStartIndex));
  BranchOrBacktrack(&not_at_start, ne, a0, Operand(zero_reg));

  // If we did, are we still at the start of the input?
  __ ld(a1, MemOperand(frame_pointer(), kInputStart));
  __ Daddu(a0, end_of_input_address(), Operand(current_input_offset()));
  BranchOrBacktrack(on_at_start, eq, a0, Operand(a1));
  __ bind(&not_at_start);
}


void RegExpMacroAssemblerMIPS::CheckNotAtStart(Label* on_not_at_start) {
  // Did we start the match at the start of the string at all?
  __ lw(a0, MemOperand(frame_pointer(), kStartIndex));
  BranchOrBacktrack(on_not_at_start, ne, a0, Operand(zero_reg));
  // If we did, are we still at the start of the input?
  __ ld(a1, MemOperand(frame_pointer(), kInputStart));
  __ Daddu(a0, end_of_input_address(), Operand(current_input_offset()));
  BranchOrBacktrack(on_not_at_start, ne, a0, Operand(a1));
}


void RegExpMacroAssemblerMIPS::CheckCharacterLT(uc16 limit, Label* on_less) {
  BranchOrBacktrack(on_less, lt, current_character(), Operand(limit));
}


void RegExpMacroAssemblerMIPS::CheckGreedyLoop(Label* on_equal) {
  Label backtrack_non_equal;
  __ lw(a0, MemOperand(backtrack_stackpointer(), 0));
  __ Branch(&backtrack_non_equal, ne, current_input_offset(), Operand(a0));
  __ Daddu(backtrack_stackpointer(),
          backtrack_stackpointer(),
          Operand(kIntSize));
  __ bind(&backtrack_non_equal);
  BranchOrBacktrack(on_equal, eq, current_input_offset(), Operand(a0));
}


void RegExpMacroAssemblerMIPS::CheckNotBackReferenceIgnoreCase(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  __ ld(a0, register_location(start_reg));  // Index of start of capture.
  __ ld(a1, register_location(start_reg + 1));  // Index of end of capture.
  __ Dsubu(a1, a1, a0);  // Length of capture.

  // If length is zero, either the capture is empty or it is not participating.
  // In either case succeed immediately.
  __ Branch(&fallthrough, eq, a1, Operand(zero_reg));

  __ Daddu(t1, a1, current_input_offset());
  // Check that there are enough characters left in the input.
  BranchOrBacktrack(on_no_match, gt, t1, Operand(zero_reg));

  if (mode_ == ASCII) {
    Label success;
    Label fail;
    Label loop_check;

    // a0 - offset of start of capture.
    // a1 - length of capture.
    __ Daddu(a0, a0, Operand(end_of_input_address()));
    __ Daddu(a2, end_of_input_address(), Operand(current_input_offset()));
    __ Daddu(a1, a0, Operand(a1));

    // a0 - Address of start of capture.
    // a1 - Address of end of capture.
    // a2 - Address of current input position.

    Label loop;
    __ bind(&loop);
    __ lbu(a3, MemOperand(a0, 0));
    __ daddiu(a0, a0, char_size());
    __ lbu(a4, MemOperand(a2, 0));
    __ daddiu(a2, a2, char_size());

    __ Branch(&loop_check, eq, a4, Operand(a3));

    // Mismatch, try case-insensitive match (converting letters to lower-case).
    __ Or(a3, a3, Operand(0x20));  // Convert capture character to lower-case.
    __ Or(a4, a4, Operand(0x20));  // Also convert input character.
    __ Branch(&fail, ne, a4, Operand(a3));
    __ Dsubu(a3, a3, Operand('a'));
    __ Branch(&loop_check, ls, a3, Operand('z' - 'a'));
    // Latin-1: Check for values in range [224,254] but not 247.
    __ Dsubu(a3, a3, Operand(224 - 'a'));
    // Weren't Latin-1 letters.
    __ Branch(&fail, hi, a3, Operand(254 - 224));
    // Check for 247.
    __ Branch(&fail, eq, a3, Operand(247 - 224));

    __ bind(&loop_check);
    __ Branch(&loop, lt, a0, Operand(a1));
    __ jmp(&success);

    __ bind(&fail);
    GoTo(on_no_match);

    __ bind(&success);
    // Compute new value of character position after the matched part.
    __ Dsubu(current_input_offset(), a2, end_of_input_address());
  } else {
    DCHECK(mode_ == UC16);
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
    __ Daddu(a0, a0, Operand(end_of_input_address()));
    // Length of capture.
    __ mov(a2, a1);
    // Save length in callee-save register for use on return.
    __ mov(s3, a1);
    // Address of current input position.
    __ Daddu(a1, current_input_offset(), Operand(end_of_input_address()));
    // Isolate.
    __ li(a3, Operand(ExternalReference::isolate_address(masm_->isolate())));

    {
      AllowExternalCallThatCantCauseGC scope(masm_);
      ExternalReference function =
          ExternalReference::re_case_insensitive_compare_uc16(masm_->isolate());
      __ CallCFunction(function, argument_count);
    }

    // Restore regexp engine registers.
    __ MultiPop(regexp_registers_to_retain);
    __ li(code_pointer(), Operand(masm_->CodeObject()), CONSTANT_SIZE);
    __ ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));

    // Check if function returned non-zero for success or zero for failure.
    BranchOrBacktrack(on_no_match, eq, v0, Operand(zero_reg));
    // On success, increment position by length of capture.
    __ Daddu(current_input_offset(), current_input_offset(), Operand(s3));
  }

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerMIPS::CheckNotBackReference(
    int start_reg,
    Label* on_no_match) {
  Label fallthrough;
  Label success;

  // Find length of back-referenced capture.
  __ ld(a0, register_location(start_reg));
  __ ld(a1, register_location(start_reg + 1));
  __ Dsubu(a1, a1, a0);  // Length to check.
  // Succeed on empty capture (including no capture).
  __ Branch(&fallthrough, eq, a1, Operand(zero_reg));

  __ Daddu(t1, a1, current_input_offset());
  // Check that there are enough characters left in the input.
  BranchOrBacktrack(on_no_match, gt, t1, Operand(zero_reg));

  // Compute pointers to match string and capture string.
  __ Daddu(a0, a0, Operand(end_of_input_address()));
  __ Daddu(a2, end_of_input_address(), Operand(current_input_offset()));
  __ Daddu(a1, a1, Operand(a0));

  Label loop;
  __ bind(&loop);
  if (mode_ == ASCII) {
    __ lbu(a3, MemOperand(a0, 0));
    __ daddiu(a0, a0, char_size());
    __ lbu(a4, MemOperand(a2, 0));
    __ daddiu(a2, a2, char_size());
  } else {
    DCHECK(mode_ == UC16);
    __ lhu(a3, MemOperand(a0, 0));
    __ daddiu(a0, a0, char_size());
    __ lhu(a4, MemOperand(a2, 0));
    __ daddiu(a2, a2, char_size());
  }
  BranchOrBacktrack(on_no_match, ne, a3, Operand(a4));
  __ Branch(&loop, lt, a0, Operand(a1));

  // Move current character position to position after match.
  __ Dsubu(current_input_offset(), a2, end_of_input_address());
  __ bind(&fallthrough);
}


void RegExpMacroAssemblerMIPS::CheckNotCharacter(uint32_t c,
                                                 Label* on_not_equal) {
  BranchOrBacktrack(on_not_equal, ne, current_character(), Operand(c));
}


void RegExpMacroAssemblerMIPS::CheckCharacterAfterAnd(uint32_t c,
                                                      uint32_t mask,
                                                      Label* on_equal) {
  __ And(a0, current_character(), Operand(mask));
  Operand rhs = (c == 0) ? Operand(zero_reg) : Operand(c);
  BranchOrBacktrack(on_equal, eq, a0, rhs);
}


void RegExpMacroAssemblerMIPS::CheckNotCharacterAfterAnd(uint32_t c,
                                                         uint32_t mask,
                                                         Label* on_not_equal) {
  __ And(a0, current_character(), Operand(mask));
  Operand rhs = (c == 0) ? Operand(zero_reg) : Operand(c);
  BranchOrBacktrack(on_not_equal, ne, a0, rhs);
}


void RegExpMacroAssemblerMIPS::CheckNotCharacterAfterMinusAnd(
    uc16 c,
    uc16 minus,
    uc16 mask,
    Label* on_not_equal) {
  DCHECK(minus < String::kMaxUtf16CodeUnit);
  __ Dsubu(a0, current_character(), Operand(minus));
  __ And(a0, a0, Operand(mask));
  BranchOrBacktrack(on_not_equal, ne, a0, Operand(c));
}


void RegExpMacroAssemblerMIPS::CheckCharacterInRange(
    uc16 from,
    uc16 to,
    Label* on_in_range) {
  __ Dsubu(a0, current_character(), Operand(from));
  // Unsigned lower-or-same condition.
  BranchOrBacktrack(on_in_range, ls, a0, Operand(to - from));
}


void RegExpMacroAssemblerMIPS::CheckCharacterNotInRange(
    uc16 from,
    uc16 to,
    Label* on_not_in_range) {
  __ Dsubu(a0, current_character(), Operand(from));
  // Unsigned higher condition.
  BranchOrBacktrack(on_not_in_range, hi, a0, Operand(to - from));
}


void RegExpMacroAssemblerMIPS::CheckBitInTable(
    Handle<ByteArray> table,
    Label* on_bit_set) {
  __ li(a0, Operand(table));
  if (mode_ != ASCII || kTableMask != String::kMaxOneByteCharCode) {
    __ And(a1, current_character(), Operand(kTableSize - 1));
    __ Daddu(a0, a0, a1);
  } else {
    __ Daddu(a0, a0, current_character());
  }

  __ lbu(a0, FieldMemOperand(a0, ByteArray::kHeaderSize));
  BranchOrBacktrack(on_bit_set, ne, a0, Operand(zero_reg));
}


bool RegExpMacroAssemblerMIPS::CheckSpecialCharacterClass(uc16 type,
                                                          Label* on_no_match) {
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check.
  switch (type) {
  case 's':
    // Match space-characters.
    if (mode_ == ASCII) {
      // One byte space characters are '\t'..'\r', ' ' and \u00a0.
      Label success;
      __ Branch(&success, eq, current_character(), Operand(' '));
      // Check range 0x09..0x0d.
      __ Dsubu(a0, current_character(), Operand('\t'));
      __ Branch(&success, ls, a0, Operand('\r' - '\t'));
      // \u00a0 (NBSP).
      BranchOrBacktrack(on_no_match, ne, a0, Operand(0x00a0 - '\t'));
      __ bind(&success);
      return true;
    }
    return false;
  case 'S':
    // The emitted code for generic character classes is good enough.
    return false;
  case 'd':
    // Match ASCII digits ('0'..'9').
    __ Dsubu(a0, current_character(), Operand('0'));
    BranchOrBacktrack(on_no_match, hi, a0, Operand('9' - '0'));
    return true;
  case 'D':
    // Match non ASCII-digits.
    __ Dsubu(a0, current_character(), Operand('0'));
    BranchOrBacktrack(on_no_match, ls, a0, Operand('9' - '0'));
    return true;
  case '.': {
    // Match non-newlines (not 0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029).
    __ Xor(a0, current_character(), Operand(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c.
    __ Dsubu(a0, a0, Operand(0x0b));
    BranchOrBacktrack(on_no_match, ls, a0, Operand(0x0c - 0x0b));
    if (mode_ == UC16) {
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ Dsubu(a0, a0, Operand(0x2028 - 0x0b));
      BranchOrBacktrack(on_no_match, ls, a0, Operand(1));
    }
    return true;
  }
  case 'n': {
    // Match newlines (0x0a('\n'), 0x0d('\r'), 0x2028 and 0x2029).
    __ Xor(a0, current_character(), Operand(0x01));
    // See if current character is '\n'^1 or '\r'^1, i.e., 0x0b or 0x0c.
    __ Dsubu(a0, a0, Operand(0x0b));
    if (mode_ == ASCII) {
      BranchOrBacktrack(on_no_match, hi, a0, Operand(0x0c - 0x0b));
    } else {
      Label done;
      BranchOrBacktrack(&done, ls, a0, Operand(0x0c - 0x0b));
      // Compare original value to 0x2028 and 0x2029, using the already
      // computed (current_char ^ 0x01 - 0x0b). I.e., check for
      // 0x201d (0x2028 - 0x0b) or 0x201e.
      __ Dsubu(a0, a0, Operand(0x2028 - 0x0b));
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
    __ Daddu(a0, a0, current_character());
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
    __ Daddu(a0, a0, current_character());
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
  Label return_v0;
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
    // TODO(plind): we save s0..s7, but ONLY use s3 here - use the regs
    // or dont save.
    RegList registers_to_retain = s0.bit() | s1.bit() | s2.bit() |
        s3.bit() | s4.bit() | s5.bit() | s6.bit() | s7.bit() | fp.bit();
    RegList argument_registers = a0.bit() | a1.bit() | a2.bit() | a3.bit();

    if (kMipsAbi == kN64) {
      // TODO(plind): Should probably alias a4-a7, for clarity.
      argument_registers |= a4.bit() | a5.bit() | a6.bit() | a7.bit();
    }

    __ MultiPush(argument_registers | registers_to_retain | ra.bit());
    // Set frame pointer in space for it if this is not a direct call
    // from generated code.
    // TODO(plind): this 8 is the # of argument regs, should have definition.
    __ Daddu(frame_pointer(), sp, Operand(8 * kPointerSize));
    __ mov(a0, zero_reg);
    __ push(a0);  // Make room for success counter and initialize it to 0.
    __ push(a0);  // Make room for "position - 1" constant (value irrelevant).

    // Check if we have space on the stack for registers.
    Label stack_limit_hit;
    Label stack_ok;

    ExternalReference stack_limit =
        ExternalReference::address_of_stack_limit(masm_->isolate());
    __ li(a0, Operand(stack_limit));
    __ ld(a0, MemOperand(a0));
    __ Dsubu(a0, sp, a0);
    // Handle it if the stack pointer is already below the stack limit.
    __ Branch(&stack_limit_hit, le, a0, Operand(zero_reg));
    // Check if there is room for the variable number of registers above
    // the stack limit.
    __ Branch(&stack_ok, hs, a0, Operand(num_registers_ * kPointerSize));
    // Exit with OutOfMemory exception. There is not enough space on the stack
    // for our working registers.
    __ li(v0, Operand(EXCEPTION));
    __ jmp(&return_v0);

    __ bind(&stack_limit_hit);
    CallCheckStackGuardState(a0);
    // If returned value is non-zero, we exit with the returned value as result.
    __ Branch(&return_v0, ne, v0, Operand(zero_reg));

    __ bind(&stack_ok);
    // Allocate space on stack for registers.
    __ Dsubu(sp, sp, Operand(num_registers_ * kPointerSize));
    // Load string end.
    __ ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
    // Load input start.
    __ ld(a0, MemOperand(frame_pointer(), kInputStart));
    // Find negative length (offset of start relative to end).
    __ Dsubu(current_input_offset(), a0, end_of_input_address());
    // Set a0 to address of char before start of the input string
    // (effectively string position -1).
    __ ld(a1, MemOperand(frame_pointer(), kStartIndex));
    __ Dsubu(a0, current_input_offset(), Operand(char_size()));
    __ dsll(t1, a1, (mode_ == UC16) ? 1 : 0);
    __ Dsubu(a0, a0, t1);
    // Store this value in a local variable, for use when clearing
    // position registers.
    __ sd(a0, MemOperand(frame_pointer(), kInputStartMinusOne));

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
        __ Daddu(a1, frame_pointer(), Operand(kRegisterZero));
        __ li(a2, Operand(num_saved_registers_));
        Label init_loop;
        __ bind(&init_loop);
        __ sd(a0, MemOperand(a1));
        __ Daddu(a1, a1, Operand(-kPointerSize));
        __ Dsubu(a2, a2, Operand(1));
        __ Branch(&init_loop, ne, a2, Operand(zero_reg));
      } else {
        for (int i = 0; i < num_saved_registers_; i++) {
          __ sd(a0, register_location(i));
        }
      }
    }

    // Initialize backtrack stack pointer.
    __ ld(backtrack_stackpointer(), MemOperand(frame_pointer(), kStackHighEnd));

    __ jmp(&start_label_);


    // Exit code:
    if (success_label_.is_linked()) {
      // Save captures when successful.
      __ bind(&success_label_);
      if (num_saved_registers_ > 0) {
        // Copy captures to output.
        __ ld(a1, MemOperand(frame_pointer(), kInputStart));
        __ ld(a0, MemOperand(frame_pointer(), kRegisterOutput));
        __ ld(a2, MemOperand(frame_pointer(), kStartIndex));
        __ Dsubu(a1, end_of_input_address(), a1);
        // a1 is length of input in bytes.
        if (mode_ == UC16) {
          __ dsrl(a1, a1, 1);
        }
        // a1 is length of input in characters.
        __ Daddu(a1, a1, Operand(a2));
        // a1 is length of string in characters.

        DCHECK_EQ(0, num_saved_registers_ % 2);
        // Always an even number of capture registers. This allows us to
        // unroll the loop once to add an operation between a load of a register
        // and the following use of that register.
        for (int i = 0; i < num_saved_registers_; i += 2) {
          __ ld(a2, register_location(i));
          __ ld(a3, register_location(i + 1));
          if (i == 0 && global_with_zero_length_check()) {
            // Keep capture start in a4 for the zero-length check later.
            __ mov(t3, a2);
          }
          if (mode_ == UC16) {
            __ dsra(a2, a2, 1);
            __ Daddu(a2, a2, a1);
            __ dsra(a3, a3, 1);
            __ Daddu(a3, a3, a1);
          } else {
            __ Daddu(a2, a1, Operand(a2));
            __ Daddu(a3, a1, Operand(a3));
          }
          // V8 expects the output to be an int32_t array.
          __ sw(a2, MemOperand(a0));
          __ Daddu(a0, a0, kIntSize);
          __ sw(a3, MemOperand(a0));
          __ Daddu(a0, a0, kIntSize);
        }
      }

      if (global()) {
        // Restart matching if the regular expression is flagged as global.
        __ ld(a0, MemOperand(frame_pointer(), kSuccessfulCaptures));
        __ lw(a1, MemOperand(frame_pointer(), kNumOutputRegisters));
        __ ld(a2, MemOperand(frame_pointer(), kRegisterOutput));
        // Increment success counter.
        __ Daddu(a0, a0, 1);
        __ sd(a0, MemOperand(frame_pointer(), kSuccessfulCaptures));
        // Capture results have been stored, so the number of remaining global
        // output registers is reduced by the number of stored captures.
        __ Dsubu(a1, a1, num_saved_registers_);
        // Check whether we have enough room for another set of capture results.
        __ mov(v0, a0);
        __ Branch(&return_v0, lt, a1, Operand(num_saved_registers_));

        __ sd(a1, MemOperand(frame_pointer(), kNumOutputRegisters));
        // Advance the location for output.
        __ Daddu(a2, a2, num_saved_registers_ * kIntSize);
        __ sd(a2, MemOperand(frame_pointer(), kRegisterOutput));

        // Prepare a0 to initialize registers with its value in the next run.
        __ ld(a0, MemOperand(frame_pointer(), kInputStartMinusOne));

        if (global_with_zero_length_check()) {
          // Special case for zero-length matches.
          // t3: capture start index
          // Not a zero-length match, restart.
          __ Branch(
              &load_char_start_regexp, ne, current_input_offset(), Operand(t3));
          // Offset from the end is zero if we already reached the end.
          __ Branch(&exit_label_, eq, current_input_offset(),
                    Operand(zero_reg));
          // Advance current position after a zero-length match.
          __ Daddu(current_input_offset(),
                  current_input_offset(),
                  Operand((mode_ == UC16) ? 2 : 1));
        }

        __ Branch(&load_char_start_regexp);
      } else {
        __ li(v0, Operand(SUCCESS));
      }
    }
    // Exit and return v0.
    __ bind(&exit_label_);
    if (global()) {
      __ ld(v0, MemOperand(frame_pointer(), kSuccessfulCaptures));
    }

    __ bind(&return_v0);
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
      __ Branch(&return_v0, ne, v0, Operand(zero_reg));

      // String might have moved: Reload end of string from frame.
      __ ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
      __ li(code_pointer(), Operand(masm_->CodeObject()), CONSTANT_SIZE);
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
      __ Daddu(a1, frame_pointer(), Operand(kStackHighEnd));
      __ li(a2, Operand(ExternalReference::isolate_address(masm_->isolate())));
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
      __ li(code_pointer(), Operand(masm_->CodeObject()), CONSTANT_SIZE);
      __ ld(end_of_input_address(), MemOperand(frame_pointer(), kInputEnd));
      SafeReturn();
    }

    if (exit_with_exception.is_linked()) {
      // If any of the code above needed to exit with an exception.
      __ bind(&exit_with_exception);
      // Exit with Result EXCEPTION(-1) to signal thrown exception.
      __ li(v0, Operand(EXCEPTION));
      __ jmp(&return_v0);
    }
  }

  CodeDesc code_desc;
  masm_->GetCode(&code_desc);
  Handle<Code> code = isolate()->factory()->NewCode(
      code_desc, Code::ComputeFlags(Code::REGEXP), masm_->CodeObject());
  LOG(masm_->isolate(), RegExpCodeCreateEvent(*code, *source));
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
  __ ld(a0, register_location(reg));
    BranchOrBacktrack(if_ge, ge, a0, Operand(comparand));
}


void RegExpMacroAssemblerMIPS::IfRegisterLT(int reg,
                                            int comparand,
                                            Label* if_lt) {
  __ ld(a0, register_location(reg));
  BranchOrBacktrack(if_lt, lt, a0, Operand(comparand));
}


void RegExpMacroAssemblerMIPS::IfRegisterEqPos(int reg,
                                               Label* if_eq) {
  __ ld(a0, register_location(reg));
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
  DCHECK(cp_offset >= -1);      // ^ and \b can look behind one character.
  DCHECK(cp_offset < (1<<30));  // Be sane! (And ensure negation works).
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
  __ sd(a0, register_location(register_index));
}


void RegExpMacroAssemblerMIPS::PushBacktrack(Label* label) {
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
      __ lwu(a0, MemOperand(code_pointer(), cp_offset));
    } else {
      __ Daddu(a0, code_pointer(), cp_offset);
      __ lwu(a0, MemOperand(a0, 0));
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
  __ ld(a0, register_location(register_index));
  Push(a0);
  if (check_stack_limit) CheckStackLimit();
}


void RegExpMacroAssemblerMIPS::ReadCurrentPositionFromRegister(int reg) {
  __ ld(current_input_offset(), register_location(reg));
}


void RegExpMacroAssemblerMIPS::ReadStackPointerFromRegister(int reg) {
  __ ld(backtrack_stackpointer(), register_location(reg));
  __ ld(a0, MemOperand(frame_pointer(), kStackHighEnd));
  __ Daddu(backtrack_stackpointer(), backtrack_stackpointer(), Operand(a0));
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
  DCHECK(register_index >= num_saved_registers_);  // Reserved for positions!
  __ li(a0, Operand(to));
  __ sd(a0, register_location(register_index));
}


bool RegExpMacroAssemblerMIPS::Succeed() {
  __ jmp(&success_label_);
  return global();
}


void RegExpMacroAssemblerMIPS::WriteCurrentPositionToRegister(int reg,
                                                              int cp_offset) {
  if (cp_offset == 0) {
    __ sd(current_input_offset(), register_location(reg));
  } else {
    __ Daddu(a0, current_input_offset(), Operand(cp_offset * char_size()));
    __ sd(a0, register_location(reg));
  }
}


void RegExpMacroAssemblerMIPS::ClearRegisters(int reg_from, int reg_to) {
  DCHECK(reg_from <= reg_to);
  __ ld(a0, MemOperand(frame_pointer(), kInputStartMinusOne));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ sd(a0, register_location(reg));
  }
}


void RegExpMacroAssemblerMIPS::WriteStackPointerToRegister(int reg) {
  __ ld(a1, MemOperand(frame_pointer(), kStackHighEnd));
  __ Dsubu(a0, backtrack_stackpointer(), a1);
  __ sd(a0, register_location(reg));
}


bool RegExpMacroAssemblerMIPS::CanReadUnaligned() {
  return false;
}


// Private methods:

void RegExpMacroAssemblerMIPS::CallCheckStackGuardState(Register scratch) {
  int stack_alignment = base::OS::ActivationFrameAlignment();

  // Align the stack pointer and save the original sp value on the stack.
  __ mov(scratch, sp);
  __ Dsubu(sp, sp, Operand(kPointerSize));
  DCHECK(IsPowerOf2(stack_alignment));
  __ And(sp, sp, Operand(-stack_alignment));
  __ sd(scratch, MemOperand(sp));

  __ mov(a2, frame_pointer());
  // Code* of self.
  __ li(a1, Operand(masm_->CodeObject()), CONSTANT_SIZE);

  // We need to make room for the return address on the stack.
  DCHECK(IsAligned(stack_alignment, kPointerSize));
  __ Dsubu(sp, sp, Operand(stack_alignment));

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are in registers, meaning we teat the return address as
  // argument 5. Since DirectCEntryStub will handleallocating space for the C
  // argument slots, we don't need to care about that here. This is how the
  // stack will look (sp meaning the value of sp at this moment):
  // [sp + 3] - empty slot if needed for alignment.
  // [sp + 2] - saved sp.
  // [sp + 1] - second word reserved for return value.
  // [sp + 0] - first word reserved for return value.

  // a0 will point to the return address, placed by DirectCEntry.
  __ mov(a0, sp);

  ExternalReference stack_guard_check =
      ExternalReference::re_check_stack_guard_state(masm_->isolate());
  __ li(t9, Operand(stack_guard_check));
  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm_, t9);

  // DirectCEntryStub allocated space for the C argument slots so we have to
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
  __ ld(sp, MemOperand(sp, stack_alignment + kCArgsSlotsSize));

  __ li(code_pointer(), Operand(masm_->CodeObject()));
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
  StackLimitCheck check(isolate);
  if (check.JsHasOverflowed()) {
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

  DCHECK(re_code->instruction_start() <= *return_address);
  DCHECK(*return_address <=
      re_code->instruction_start() + re_code->instruction_size());

  Object* result = isolate->stack_guard()->HandleInterrupts();

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
  DCHECK(StringShape(*subject_tmp).IsSequential() ||
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


MemOperand RegExpMacroAssemblerMIPS::register_location(int register_index) {
  DCHECK(register_index < (1<<30));
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


void RegExpMacroAssemblerMIPS::SafeCall(Label* to,
                                        Condition cond,
                                        Register rs,
                                        const Operand& rt) {
  __ BranchAndLink(to, cond, rs, rt);
}


void RegExpMacroAssemblerMIPS::SafeReturn() {
  __ pop(ra);
  __ Daddu(t1, ra, Operand(masm_->CodeObject()));
  __ Jump(t1);
}


void RegExpMacroAssemblerMIPS::SafeCallTarget(Label* name) {
  __ bind(name);
  __ Dsubu(ra, ra, Operand(masm_->CodeObject()));
  __ push(ra);
}


void RegExpMacroAssemblerMIPS::Push(Register source) {
  DCHECK(!source.is(backtrack_stackpointer()));
  __ Daddu(backtrack_stackpointer(),
          backtrack_stackpointer(),
          Operand(-kIntSize));
  __ sw(source, MemOperand(backtrack_stackpointer()));
}


void RegExpMacroAssemblerMIPS::Pop(Register target) {
  DCHECK(!target.is(backtrack_stackpointer()));
  __ lw(target, MemOperand(backtrack_stackpointer()));
  __ Daddu(backtrack_stackpointer(), backtrack_stackpointer(), kIntSize);
}


void RegExpMacroAssemblerMIPS::CheckPreemption() {
  // Check for preemption.
  ExternalReference stack_limit =
      ExternalReference::address_of_stack_limit(masm_->isolate());
  __ li(a0, Operand(stack_limit));
  __ ld(a0, MemOperand(a0));
  SafeCall(&check_preempt_label_, ls, sp, Operand(a0));
}


void RegExpMacroAssemblerMIPS::CheckStackLimit() {
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit(masm_->isolate());

  __ li(a0, Operand(stack_limit));
  __ ld(a0, MemOperand(a0));
  SafeCall(&stack_overflow_label_, ls, backtrack_stackpointer(), Operand(a0));
}


void RegExpMacroAssemblerMIPS::LoadCurrentCharacterUnchecked(int cp_offset,
                                                             int characters) {
  Register offset = current_input_offset();
  if (cp_offset != 0) {
    // t3 is not being used to store the capture start index at this point.
    __ Daddu(t3, current_input_offset(), Operand(cp_offset * char_size()));
    offset = t3;
  }
  // We assume that we cannot do unaligned loads on MIPS, so this function
  // must only be used to load a single character at a time.
  DCHECK(characters == 1);
  __ Daddu(t1, end_of_input_address(), Operand(offset));
  if (mode_ == ASCII) {
    __ lbu(current_character(), MemOperand(t1, 0));
  } else {
    DCHECK(mode_ == UC16);
    __ lhu(current_character(), MemOperand(t1, 0));
  }
}

#undef __

#endif  // V8_INTERPRETED_REGEXP

}}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS64

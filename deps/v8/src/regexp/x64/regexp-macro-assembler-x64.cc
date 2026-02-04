// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/regexp/x64/regexp-macro-assembler-x64.h"

#include "src/codegen/code-desc.h"
#include "src/codegen/macro-assembler.h"
#include "src/heap/factory.h"
#include "src/logging/log.h"
#include "src/objects/code-inl.h"
#include "src/regexp/regexp-compiler.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/regexp/regexp-stack.h"

namespace v8 {
namespace internal {

/*
 * This assembler uses the following register assignment convention
 * - rdx : Currently loaded character(s) as Latin1 or UC16.  Must be loaded
 *         using LoadCurrentCharacter before using any of the dispatch methods.
 *         Temporarily stores the index of capture start after a matching pass
 *         for a global regexp.
 * - rdi : Current position in input, as negative offset from end of string.
 *         Please notice that this is the byte offset, not the character
 *         offset!  Is always a 32-bit signed (negative) offset, but must be
 *         maintained sign-extended to 64 bits, since it is used as index.
 * - rsi : End of input (points to byte after last character in input),
 *         so that rsi+rdi points to the current character.
 * - rbp : Frame pointer.  Used to access arguments, local variables and
 *         RegExp registers.
 * - rsp : Points to tip of C stack.
 * - rbx : Points to tip of backtrack stack.  The backtrack stack contains
 *         only 32-bit values.  Most are offsets from some base (e.g., character
 *         positions from end of string or code location from InstructionStream
 * pointer).
 * - r8  : InstructionStream object pointer.  Used to convert between absolute
 * and code-object-relative addresses.
 *
 * The registers rax, rcx, r9, r11, and r12 are free to use for computations.
 * If changed to use r13+, they should be saved as callee-save registers.
 * The macro assembler special register r13 (kRootRegister) isn't special
 * during execution of RegExp code (it doesn't hold the value assumed when
 * creating JS code), so Root related macro operations can be used.
 *
 * xmm0 - xmm5 are free to use. On Windows, xmm6 - xmm15 are callee-saved and
 * therefore need to be saved/restored.
 *
 * Each call to a C++ method should retain these registers.
 *
 * The stack will have the following content, in some order, indexable from the
 * frame pointer (see, e.g., kDirectCallOffset):
 *    - Address regexp       (address of the JSRegExp object; unused in native
 *                            code, passed to match signature of interpreter)
 *    - Isolate* isolate     (address of the current isolate)
 *    - direct_call          (if 1, direct call from JavaScript code, if 0 call
 *                            through the runtime system)
 *    - capture array size   (may fit multiple sets of matches)
 *    - int* capture_array   (int[num_saved_registers_], for output).
 *    - end of input         (address of end of string)
 *    - start of input       (address of first character in string)
 *    - start index          (character index of start)
 *    - String input_string  (input string)
 *    - return address
 *    - backup of callee save registers (rbx, possibly rsi and rdi).
 *    - success counter      (only useful for global regexp to count matches)
 *    - Offset of location before start of input (effectively character
 *      string start - 1).  Used to initialize capture registers to a
 *      non-position.
 *    - At start of string (if 1, we are starting at the start of the
 *      string, otherwise 0)
 *    - register 0  rbp[-n]   (Only positions must be stored in the first
 *    - register 1  rbp[-n-8]  num_saved_registers_ registers)
 *    - ...
 *
 * The first num_saved_registers_ registers are initialized to point to
 * "character -1" in the string (i.e., char_size() bytes before the first
 * character of the string).  The remaining registers starts out uninitialized.
 *
 * The argument values must be provided by the calling code by calling the
 * code's entry address cast to a function pointer with the following signature:
 * int (*match)(String input_string,
 *              int start_index,
 *              Address start,
 *              Address end,
 *              int* capture_output_array,
 *              int num_capture_registers,
 *              bool direct_call = false,
 *              Isolate* isolate,
 *              Address regexp);
 */

#define __ ACCESS_MASM((&masm_))

const int RegExpMacroAssemblerX64::kRegExpCodeSize;

RegExpMacroAssemblerX64::RegExpMacroAssemblerX64(Isolate* isolate, Zone* zone,
                                                 Mode mode,
                                                 int registers_to_save)
    : NativeRegExpMacroAssembler(isolate, zone, mode),
      masm_(isolate, CodeObjectRequired::kYes,
            NewAssemblerBuffer(kRegExpCodeSize)),
      no_root_array_scope_(&masm_),
      code_relative_fixup_positions_(zone),
      num_registers_(registers_to_save),
      num_saved_registers_(registers_to_save),
      entry_label_(),
      start_label_(),
      success_label_(),
      backtrack_label_(),
      exit_label_() {
  DCHECK_EQ(0, registers_to_save % 2);
  __ CodeEntry();
  __ jmp(&entry_label_);   // We'll write the entry code when we know more.
  __ bind(&start_label_);  // And then continue from here.
}

RegExpMacroAssemblerX64::~RegExpMacroAssemblerX64() {
  // Unuse labels in case we throw away the assembler without calling GetCode.
  entry_label_.Unuse();
  start_label_.Unuse();
  success_label_.Unuse();
  backtrack_label_.Unuse();
  exit_label_.Unuse();
  check_preempt_label_.Unuse();
  stack_overflow_label_.Unuse();
  fallback_label_.Unuse();
}

void RegExpMacroAssemblerX64::AdvanceCurrentPosition(int by) {
  if (by != 0) {
    __ addq(rdi, Immediate(by * char_size()));
  }
}


void RegExpMacroAssemblerX64::AdvanceRegister(int reg, int by) {
  DCHECK_LE(0, reg);
  DCHECK_GT(num_registers_, reg);
  if (by != 0) {
    __ addq(register_location(reg), Immediate(by));
  }
}


void RegExpMacroAssemblerX64::Backtrack() {
  CheckPreemption();
  if (has_backtrack_limit()) {
    Label next;
    __ incq(Operand(rbp, kBacktrackCountOffset));
    __ cmpq(Operand(rbp, kBacktrackCountOffset), Immediate(backtrack_limit()));
    __ j(not_equal, &next);

    // Backtrack limit exceeded.
    if (can_fallback()) {
      __ jmp(&fallback_label_);
    } else {
      // Can't fallback, so we treat it as a failed match.
      Fail();
    }

    __ bind(&next);
  }
  // Pop InstructionStream offset from backtrack stack, add InstructionStream
  // and jump to location.
  Pop(rcx);
  __ addq(rcx, code_object_pointer());

  // TODO(sroettger): This jump needs an endbr64 instruction but the code is
  // performance sensitive. Needs more thought how to do this in a fast way.
  __ jmp(rcx, /*notrack=*/true);
}


void RegExpMacroAssemblerX64::Bind(Label* label) {
  __ bind(label);
}


void RegExpMacroAssemblerX64::CheckCharacter(uint32_t c, Label* on_equal) {
  __ cmpl(current_character(), Immediate(c));
  BranchOrBacktrack(equal, on_equal);
}

void RegExpMacroAssemblerX64::CheckCharacterGT(base::uc16 limit,
                                               Label* on_greater) {
  __ cmpl(current_character(), Immediate(limit));
  BranchOrBacktrack(greater, on_greater);
}

void RegExpMacroAssemblerX64::CheckAtStart(int cp_offset, Label* on_at_start) {
  __ leaq(rax, Operand(rdi, -char_size() + cp_offset * char_size()));
  __ cmpq(rax, Operand(rbp, kStringStartMinusOneOffset));
  BranchOrBacktrack(equal, on_at_start);
}

void RegExpMacroAssemblerX64::CheckNotAtStart(int cp_offset,
                                              Label* on_not_at_start) {
  __ leaq(rax, Operand(rdi, -char_size() + cp_offset * char_size()));
  __ cmpq(rax, Operand(rbp, kStringStartMinusOneOffset));
  BranchOrBacktrack(not_equal, on_not_at_start);
}

void RegExpMacroAssemblerX64::CheckCharacterLT(base::uc16 limit,
                                               Label* on_less) {
  __ cmpl(current_character(), Immediate(limit));
  BranchOrBacktrack(less, on_less);
}

void RegExpMacroAssemblerX64::CheckFixedLengthLoop(Label* on_equal) {
  Label fallthrough;
  __ cmpl(rdi, Operand(backtrack_stackpointer(), 0));
  __ j(not_equal, &fallthrough);
  Drop();
  BranchOrBacktrack(on_equal);
  __ bind(&fallthrough);
}

void RegExpMacroAssemblerX64::CallCFunctionFromIrregexpCode(
    ExternalReference function, int num_arguments) {
  // Irregexp code must not set fast_c_call_caller_fp and fast_c_call_caller_pc
  // since
  //
  // 1. it may itself have been called using CallCFunction and nested calls are
  //    unsupported, and
  // 2. it may itself have been called directly from C where the frame pointer
  //    might not be set (-fomit-frame-pointer), and thus frame iteration would
  //    fail.
  //
  // See also: crbug.com/v8/12670#c17.
  __ CallCFunction(function, num_arguments, SetIsolateDataSlots::kNo);
}

// Push (pop) caller-saved registers used by irregexp.
void RegExpMacroAssemblerX64::PushCallerSavedRegisters() {
#ifndef V8_TARGET_OS_WIN
  // Callee-save in Microsoft 64-bit ABI, but not in AMD64 ABI.
  __ pushq(rsi);
  __ pushq(rdi);
#endif
}

void RegExpMacroAssemblerX64::PopCallerSavedRegisters() {
#ifndef V8_TARGET_OS_WIN
  __ popq(rdi);
  __ popq(rsi);
#endif
}

void RegExpMacroAssemblerX64::CheckNotBackReferenceIgnoreCase(
    int start_reg, bool read_backward, bool unicode, Label* on_no_match) {
  Label fallthrough;
  ReadPositionFromRegister(rdx, start_reg);  // Offset of start of capture
  ReadPositionFromRegister(rcx, start_reg + 1);  // Offset of end of capture
  __ subq(rcx, rdx);                             // Length of capture.

  // -----------------------
  // rdx  = Start offset of capture.
  // rcx = Length of capture

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ j(equal, &fallthrough);

  // -----------------------
  // rdx - Start of capture
  // rcx - length of capture
  // Check that there are sufficient characters left in the input.
  if (read_backward) {
    __ movl(rax, Operand(rbp, kStringStartMinusOneOffset));
    __ addl(rax, rcx);
    __ cmpl(rdi, rax);
    BranchOrBacktrack(less_equal, on_no_match);
  } else {
    __ movl(rax, rdi);
    __ addl(rax, rcx);
    BranchOrBacktrack(greater, on_no_match);
  }

  if (mode() == LATIN1) {
    Label loop_increment;
    if (on_no_match == nullptr) {
      on_no_match = &backtrack_label_;
    }

    __ leaq(r9, Operand(rsi, rdx, times_1, 0));
    __ leaq(r11, Operand(rsi, rdi, times_1, 0));
    if (read_backward) {
      __ subq(r11, rcx);  // Offset by length when matching backwards.
    }
    __ addq(rcx, r9);  // End of capture
    // ---------------------
    // r11 - current input character address
    // r9 - current capture character address
    // rcx - end of capture

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
    __ orq(rax, Immediate(0x20));  // Convert match character to lower-case.
    __ orq(rdx, Immediate(0x20));  // Convert capture character to lower-case.
    __ cmpb(rax, rdx);
    __ j(not_equal, on_no_match);  // Definitely not equal.
    __ subb(rax, Immediate('a'));
    __ cmpb(rax, Immediate('z' - 'a'));
    __ j(below_equal, &loop_increment);  // In range 'a'-'z'.
    // Latin-1: Check for values in range [224,254] but not 247.
    __ subb(rax, Immediate(224 - 'a'));
    __ cmpb(rax, Immediate(254 - 224));
    __ j(above, on_no_match);  // Weren't Latin-1 letters.
    __ cmpb(rax, Immediate(247 - 224));  // Check for 247.
    __ j(equal, on_no_match);
    __ bind(&loop_increment);
    // Increment pointers into match and capture strings.
    __ addq(r11, Immediate(1));
    __ addq(r9, Immediate(1));
    // Compare to end of capture, and loop if not done.
    __ cmpq(r9, rcx);
    __ j(below, &loop);

    // Compute new value of character position after the matched part.
    __ movq(rdi, r11);
    __ subq(rdi, rsi);
    if (read_backward) {
      // Subtract match length if we matched backward.
      __ addq(rdi, register_location(start_reg));
      __ subq(rdi, register_location(start_reg + 1));
    }
  } else {
    DCHECK(mode() == UC16);
    PushCallerSavedRegisters();

    static const int num_arguments = 4;
    __ PrepareCallCFunction(num_arguments);

    static const Register kSavedByteLength = r12;

    // Put arguments into parameter registers. Parameters are
    //   Address byte_offset1 - Address captured substring's start.
    //   Address byte_offset2 - Address of current character position.
    //   size_t byte_length - length of capture in bytes(!)
    //   Isolate* isolate.
#ifdef V8_TARGET_OS_WIN
    DCHECK(rcx == kCArgRegs[0]);
    DCHECK(rdx == kCArgRegs[1]);
    DCHECK(r8 == kCArgRegs[2]);
    DCHECK(r9 == kCArgRegs[3]);
    __ movq(r8, rcx);  // Length of capture argument.
    __ movq(kSavedByteLength,
            rcx);  // Save byte_length in callee-saved register.
    // Compute and set byte_offset1 (start of capture).
    __ leaq(rcx, Operand(rsi, rdx, times_1, 0));
    // Set byte_offset2.
    __ leaq(rdx, Operand(rsi, rdi, times_1, 0));
    if (read_backward) {
      __ subq(rdx, r8);
    }
#else  // AMD64 calling convention
    DCHECK(rdi == kCArgRegs[0]);
    DCHECK(rsi == kCArgRegs[1]);
    DCHECK(rdx == kCArgRegs[2]);
    DCHECK(rcx == kCArgRegs[3]);
    // Compute byte_offset2 (current position = rsi+rdi).
    __ leaq(rax, Operand(rsi, rdi, times_1, 0));
    // Compute and set byte_offset1 (start of capture).
    __ leaq(rdi, Operand(rsi, rdx, times_1, 0));
    __ movq(rdx, rcx);  // Length of capture argument.
    __ movq(kSavedByteLength,
            rcx);  // Save byte_length in callee-saved register.
    // Set byte_offset2.
    __ movq(rsi, rax);
    if (read_backward) {
      __ subq(rsi, rdx);
    }
#endif  // V8_TARGET_OS_WIN

    // Isolate.
    __ LoadAddress(kCArgRegs[3], ExternalReference::isolate_address(isolate()));

    {
      AllowExternalCallThatCantCauseGC scope(&masm_);
      ExternalReference compare =
          unicode
              ? ExternalReference::re_case_insensitive_compare_unicode()
              : ExternalReference::re_case_insensitive_compare_non_unicode();
      CallCFunctionFromIrregexpCode(compare, num_arguments);
    }

    // Restore original values before reacting on result value.
    __ Move(code_object_pointer(), masm_.CodeObject());
    PopCallerSavedRegisters();

    // Check if function returned non-zero for success or zero for failure.
    __ testq(rax, rax);
    BranchOrBacktrack(zero, on_no_match);
    // On success, advance position by length of capture.
    if (read_backward) {
      __ subq(rdi, kSavedByteLength);
    } else {
      __ addq(rdi, kSavedByteLength);
    }
  }
  __ bind(&fallthrough);
}

void RegExpMacroAssemblerX64::CheckNotBackReference(int start_reg,
                                                    bool read_backward,
                                                    Label* on_no_match) {
  Label fallthrough;

  // Find length of back-referenced capture.
  ReadPositionFromRegister(rdx, start_reg);  // Offset of start of capture
  ReadPositionFromRegister(rax, start_reg + 1);  // Offset of end of capture
  __ subq(rax, rdx);                             // Length to check.

  // At this point, the capture registers are either both set or both cleared.
  // If the capture length is zero, then the capture is either empty or cleared.
  // Fall through in both cases.
  __ j(equal, &fallthrough);

  // -----------------------
  // rdx - Start of capture
  // rax - length of capture
  // Check that there are sufficient characters left in the input.
  if (read_backward) {
    __ movl(rcx, Operand(rbp, kStringStartMinusOneOffset));
    __ addl(rcx, rax);
    __ cmpl(rdi, rcx);
    BranchOrBacktrack(less_equal, on_no_match);
  } else {
    __ movl(rcx, rdi);
    __ addl(rcx, rax);
    BranchOrBacktrack(greater, on_no_match);
  }

  // Compute pointers to match string and capture string
  __ leaq(rcx, Operand(rsi, rdi, times_1, 0));  // Start of match.
  if (read_backward) {
    __ subq(rcx, rax);  // Offset by length when matching backwards.
  }
  __ addq(rdx, rsi);                           // Start of capture.
  __ leaq(r9, Operand(rdx, rax, times_1, 0));  // End of capture

  // -----------------------
  // rdx - current capture character address.
  // rcx - current input character address .
  // r9 - end of input to match (capture length after rcx).

  Label loop;
  __ bind(&loop);
  if (mode() == LATIN1) {
    __ movzxbl(rax, Operand(rdx, 0));
    __ cmpb(rax, Operand(rcx, 0));
  } else {
    DCHECK(mode() == UC16);
    __ movzxwl(rax, Operand(rdx, 0));
    __ cmpw(rax, Operand(rcx, 0));
  }
  BranchOrBacktrack(not_equal, on_no_match);
  // Increment pointers into capture and match string.
  __ addq(rcx, Immediate(char_size()));
  __ addq(rdx, Immediate(char_size()));
  // Check if we have reached end of match area.
  __ cmpq(rdx, r9);
  __ j(below, &loop);

  // Success.
  // Set current character position to position after match.
  __ movq(rdi, rcx);
  __ subq(rdi, rsi);
  if (read_backward) {
    // Subtract match length if we matched backward.
    __ addq(rdi, register_location(start_reg));
    __ subq(rdi, register_location(start_reg + 1));
  }

  __ bind(&fallthrough);
}


void RegExpMacroAssemblerX64::CheckNotCharacter(uint32_t c,
                                                Label* on_not_equal) {
  __ cmpl(current_character(), Immediate(c));
  BranchOrBacktrack(not_equal, on_not_equal);
}


void RegExpMacroAssemblerX64::CheckCharacterAfterAnd(uint32_t c,
                                                     uint32_t mask,
                                                     Label* on_equal) {
  if (c == 0) {
    __ testl(current_character(), Immediate(mask));
  } else {
    __ Move(rax, mask);
    __ andq(rax, current_character());
    __ cmpl(rax, Immediate(c));
  }
  BranchOrBacktrack(equal, on_equal);
}


void RegExpMacroAssemblerX64::CheckNotCharacterAfterAnd(uint32_t c,
                                                        uint32_t mask,
                                                        Label* on_not_equal) {
  if (c == 0) {
    __ testl(current_character(), Immediate(mask));
  } else {
    __ Move(rax, mask);
    __ andq(rax, current_character());
    __ cmpl(rax, Immediate(c));
  }
  BranchOrBacktrack(not_equal, on_not_equal);
}

void RegExpMacroAssemblerX64::CheckNotCharacterAfterMinusAnd(
    base::uc16 c, base::uc16 minus, base::uc16 mask, Label* on_not_equal) {
  DCHECK_GT(String::kMaxUtf16CodeUnit, minus);
  __ leal(rax, Operand(current_character(), -minus));
  __ andl(rax, Immediate(mask));
  __ cmpl(rax, Immediate(c));
  BranchOrBacktrack(not_equal, on_not_equal);
}

void RegExpMacroAssemblerX64::CheckCharacterInRange(base::uc16 from,
                                                    base::uc16 to,
                                                    Label* on_in_range) {
  __ leal(rax, Operand(current_character(), -from));
  __ cmpl(rax, Immediate(to - from));
  BranchOrBacktrack(below_equal, on_in_range);
}

void RegExpMacroAssemblerX64::CheckCharacterNotInRange(base::uc16 from,
                                                       base::uc16 to,
                                                       Label* on_not_in_range) {
  __ leal(rax, Operand(current_character(), -from));
  __ cmpl(rax, Immediate(to - from));
  BranchOrBacktrack(above, on_not_in_range);
}

void RegExpMacroAssemblerX64::CallIsCharacterInRangeArray(
    const ZoneList<CharacterRange>* ranges) {
  PushCallerSavedRegisters();

  static const int kNumArguments = 2;
  __ PrepareCallCFunction(kNumArguments);

  __ Move(kCArgRegs[0], current_character());
  __ Move(kCArgRegs[1], GetOrAddRangeArray(ranges));

  {
    // We have a frame (set up in GetCode), but the assembler doesn't know.
    FrameScope scope(&masm_, StackFrame::MANUAL);
    CallCFunctionFromIrregexpCode(
        ExternalReference::re_is_character_in_range_array(), kNumArguments);
  }

  PopCallerSavedRegisters();
  __ Move(code_object_pointer(), masm_.CodeObject());
}

bool RegExpMacroAssemblerX64::CheckCharacterInRangeArray(
    const ZoneList<CharacterRange>* ranges, Label* on_in_range) {
  CallIsCharacterInRangeArray(ranges);
  __ testq(rax, rax);
  BranchOrBacktrack(not_zero, on_in_range);
  return true;
}

bool RegExpMacroAssemblerX64::CheckCharacterNotInRangeArray(
    const ZoneList<CharacterRange>* ranges, Label* on_not_in_range) {
  CallIsCharacterInRangeArray(ranges);
  __ testq(rax, rax);
  BranchOrBacktrack(zero, on_not_in_range);
  return true;
}

void RegExpMacroAssemblerX64::CheckBitInTable(
    Handle<ByteArray> table,
    Label* on_bit_set) {
  __ Move(rax, table);
  Register index = current_character();
  if (mode() != LATIN1 || kTableMask != String::kMaxOneByteCharCode) {
    __ movq(rcx, current_character());
    __ andq(rcx, Immediate(kTableMask));
    index = rcx;
  }
  __ cmpb(FieldOperand(rax, index, times_1, OFFSET_OF_DATA_START(ByteArray)),
          Immediate(0));
  BranchOrBacktrack(not_equal, on_bit_set);
}

void RegExpMacroAssemblerX64::EmitSkipUntilBitInTableSimdHelper(
    int cp_offset, int advance_by, Handle<ByteArray> nibble_table_handle,
    int max_on_match_lookahead, Label* scalar_fallback,
    base::FunctionRef<void(Register, Register)> on_match) {
  // This function uses rax and r11 as scratch, and {xmm0..5} for simd.

  Label simd_loop, advance_vector, process_next_bit;
  static constexpr int kVectorSize = 16;
  // Currently this helper is only used for LATIN1.
  DCHECK_EQ(mode(), RegExpMacroAssembler::LATIN1);
  const int kCharsPerVector = kVectorSize / char_size();

  // Hoist the bounds check, there is one more copy before the loop backedge.
  //
  // Fallback to scalar version if there are less than kCharsPerVector chars
  // left in the subject. We subtract 1 because CheckPosition assumes we are
  // reading 1 character plus cp_offset. So the -1 is the the character that is
  // assumed to be read by default.
  static constexpr int kCheckPositionOffset = -1;
  CheckPosition(cp_offset + kCharsPerVector + kCheckPositionOffset +
                    max_on_match_lookahead,
                scalar_fallback);

  // Hoist constants.
  XMMRegister nibble_table = xmm0;
  __ Move(r11, nibble_table_handle);
  __ Movdqu(nibble_table, FieldOperand(r11, OFFSET_OF_DATA_START(ByteArray)));
  XMMRegister nibble_mask = xmm1;
  __ Move(r11, 0x0f0f0f0f'0f0f0f0f);
  __ movq(nibble_mask, r11);
  __ Movddup(nibble_mask, nibble_mask);
  XMMRegister hi_nibble_lookup_mask = xmm2;
  __ Move(r11, 0x80402010'08040201);
  __ movq(hi_nibble_lookup_mask, r11);
  __ Movddup(hi_nibble_lookup_mask, hi_nibble_lookup_mask);

  __ bind(&simd_loop);
  // Load next characters into vector.
  XMMRegister input_vec = xmm3;
  __ Movdqu(input_vec, Operand(rsi, rdi, times_1, cp_offset));

  // Extract low nibbles.
  // lo_nibbles = input & 0x0f
  XMMRegister lo_nibbles = xmm4;
  if (CpuFeatures::IsSupported(AVX)) {
    __ Andps(lo_nibbles, nibble_mask, input_vec);
  } else {
    __ Movdqa(lo_nibbles, nibble_mask);
    __ Andps(lo_nibbles, lo_nibbles, input_vec);
  }
  // Extract high nibbles.
  // hi_nibbles = (input >> 4) & 0x0f
  __ Psrlw(input_vec, uint8_t{4});
  XMMRegister hi_nibbles = ReassignRegister(input_vec);
  __ Andps(hi_nibbles, hi_nibbles, nibble_mask);

  // Get rows of nibbles table based on low nibbles.
  // row = nibble_table[lo_nibbles]
  XMMRegister row = xmm5;
  __ Pshufb(row, nibble_table, lo_nibbles);

  // Check if high nibble is set in row.
  // bitmask = 1 << (hi_nibbles & 0x7)
  //         = hi_nibble_lookup_mask[hi_nibbles] & 0x7
  // Note: The hi_nibbles & 0x7 part is implicitly executed, because the
  // input table contains two repeats of the 0x80402010'08040201 pattern.
  // The pshufb instruction also sets the output to zero if bit 7 is set
  // in the input, but that never happens because of the andps with the
  // nibble mask above.
  XMMRegister bitmask = ReassignRegister(lo_nibbles);
  __ Pshufb(bitmask, hi_nibble_lookup_mask, hi_nibbles);

  // result = row & bitmask == bitmask
  XMMRegister result = ReassignRegister(row);
  __ Andps(result, result, bitmask);
  __ Pcmpeqb(result, result, bitmask);

  // Check if any bit is set.
  // Copy the most significant bit of each result byte to r11.
  __ Pmovmskb(r11, result);
  __ testl(r11, r11);
  __ j(zero, &advance_vector);

  __ bind(&process_next_bit);
  // Extract the position of the lowest set bit.
  __ bsfl(rax, r11);

  if (mode() == RegExpMacroAssembler::UC16) {
    // Make sure that we skip an even number of bytes in 2-byte subjects.
    // Odd skips can happen if the higher byte produced a match.
    // False positives should be rare and are no problem in general, as the
    // following instructions will check for an exact match.
    __ andl(rax, Immediate(0xfffe));
  }

  // We've found a match and loaded the bit position into rax. r11 MUST be
  // preserved across this call if execution continues in the fallthrough path.
  on_match(rax, r11);

  // Clear the lowest set bit.
  if (__ IsEnabled(BMI1)) {
    __ blsrl(r11, r11);
  } else {
    // The fallback sequence is:
    // r11 = r11 & (r11 - 1)
    __ leal(rax, Operand(r11, -1));
    __ andl(r11, rax);
  }
  __ j(not_zero, &process_next_bit);

  // The maximum lookahead for boyer moore is less than vector size, so we can
  // ignore advance_by in the vectorized version.
  DCHECK_LE(advance_by, regexp_compiler_constants::kMaxLookaheadForBoyerMoore);
  DCHECK_LT(regexp_compiler_constants::kMaxLookaheadForBoyerMoore,
            kCharsPerVector);

  __ bind(&advance_vector);
  AdvanceCurrentPosition(kCharsPerVector);
  CheckPosition(cp_offset + kCharsPerVector + kCheckPositionOffset +
                    max_on_match_lookahead,
                scalar_fallback);
  __ jmp(&simd_loop);
}

void RegExpMacroAssemblerX64::SkipUntilBitInTable(
    int cp_offset, Handle<ByteArray> table,
    Handle<ByteArray> nibble_table_array, int advance_by, Label* on_match,
    Label* on_no_match) {
  Label scalar_repeat;

  const bool use_simd = SkipUntilBitInTableUseSimd(advance_by);
  if (use_simd) {
    DCHECK(!nibble_table_array.is_null());
    Label scalar;
    EmitSkipUntilBitInTableSimdHelper(
        cp_offset, advance_by, nibble_table_array, 0, &scalar,
        [&](Register index, Register callee_saved) {
          // No need to push callee_saved since we never fall through.
          __ addq(rdi, index);
          __ jmp(on_match);
        });
    Bind(&scalar);
  }

  // Scalar version.
  Register table_reg = r9;
  __ Move(table_reg, table);

  Bind(&scalar_repeat);
  CheckPosition(cp_offset, on_no_match);
  LoadCurrentCharacterUnchecked(cp_offset, 1);
  Register index = current_character();
  if (mode() != LATIN1 || kTableMask != String::kMaxOneByteCharCode) {
    index = rcx;
    __ movq(index, current_character());
    __ andq(index, Immediate(kTableMask));
  }
  __ cmpb(
      FieldOperand(table_reg, index, times_1, OFFSET_OF_DATA_START(ByteArray)),
      Immediate(0));
  __ j(not_equal, on_match);
  AdvanceCurrentPosition(advance_by);
  __ jmp(&scalar_repeat);
}

bool RegExpMacroAssemblerX64::SkipUntilBitInTableUseSimd(int advance_by) {
  // To use the SIMD variant we require SSSE3 as there is no shuffle equivalent
  // in older extensions.
  // In addition we only use SIMD instead of the scalar version if we advance by
  // 1 byte in each iteration. For higher values the scalar version performs
  // better.
  // We only implemented the SIMD version in one-byte mode.
  return v8_flags.regexp_simd && advance_by * char_size() == 1 &&
         CpuFeatures::IsSupported(SSSE3);
}

namespace {

void Pcmpeq(MacroAssembler* masm, XMMRegister dest, XMMRegister src1,
            XMMRegister src2, int width) {
  switch (width) {
    case 1:
      masm->Pcmpeqb(dest, src1, src2);
      break;
    case 2:
      masm->Pcmpeqw(dest, src1, src2);
      break;
    case 4:
      masm->Pcmpeqd(dest, src1, src2);
      break;
    default:
      UNREACHABLE();
  }
}

}  // namespace

void RegExpMacroAssemblerX64::SkipUntilOneOfMasked(
    int cp_offset, int advance_by, unsigned both_chars, unsigned both_mask,
    int max_offset, unsigned chars1, unsigned mask1, unsigned chars2,
    unsigned mask2, Label* on_match1, Label* on_match2, Label* on_failure) {
  Label scalar_repeat;
  const bool use_simd = SkipUntilOneOfMaskedUseSimd(advance_by);
  // Number of characters loaded and width of mask/chars to check.
  // TODO(pthier): Support/optimize other variants.
  static constexpr int character_count = 4;
  DCHECK_EQ(mode(), LATIN1);  // TODO(pthier): Support 2-byte.
  if (use_simd) {
    // We load the 16 characters from the subject into 4 different vector
    // registers, each offset by 1. This is required as we want to check 4
    // contiguous characters.
    // E.g. "This is a sample subject" will be loaded as:
    // input_vec1 = "This is a sample"
    // input_vec2 = "his is a sample "
    // input_vec3 = "is is a sample s"
    // input_vec4 = "s is a sample su"
    // We then check each of these vectors against both_mask and both_chars
    // (each containing 4 characters). Whenever we find a match, we simply
    // delegate the task of finding the exact match index to the scalar version
    // (getting the correct index from vector registers is complicated and
    // slower).
    Label simd_repeat, scalar, scalar_after_simd, found;
    static constexpr int kVectorSize = 16;
    const int kCharsPerVector = kVectorSize / char_size();

    // Fallback to scalar version if there are less than kCharsPerVector +
    // character_count - 1 chars left in the subject. We subtract 1 from
    // kCharsPerVector because CheckPosition assumes we are reading 1 character
    // plus max_offset. So the -1 is the the character that is assumed to be
    // read by default.
    const int max_stride_offset =
        max_offset + kCharsPerVector - 1 + character_count - 1;
    CheckPosition(max_stride_offset, &scalar);

    // Save callee-saved XMM registers.
    // TODO(pthier): Consider saving callee-saved XMM registers in the
    // prologue if more optimizations need them.
    __ subq(rsp, Immediate(6 * kVectorSize));
    __ movupd(Operand(rsp, 0), xmm6);
    __ movupd(Operand(rsp, 1 * kVectorSize), xmm7);
    __ movupd(Operand(rsp, 2 * kVectorSize), xmm8);
    __ movupd(Operand(rsp, 3 * kVectorSize), xmm9);
    __ movupd(Operand(rsp, 4 * kVectorSize), xmm10);
    __ movupd(Operand(rsp, 5 * kVectorSize), xmm11);

    // Load a 32-bit immediate and duplicate the value across all 4 lanes of a
    // 128-bit XMM register.
    // I.e. 0xAABBCCDD becomes 0xAABBCCDDAABBCCDDAABBCCDDAABBCCDD.
    auto splat_imm32 = [this](XMMRegister dst, uint32_t imm) {
      Register scratch = r11;
      __ Move(scratch, (static_cast<uint64_t>(imm) << 32) | imm);
      __ movq(dst, scratch);
      if (CpuFeatures::IsSupported(SSE3)) {
        CpuFeatureScope sse3_scope(masm(), SSE3);
        __ Movddup(dst, dst);
      } else {
        __ shufpd(dst, dst, 0);
      }
    };

    // Load constants.
    XMMRegister both_mask_vec = xmm0;
    splat_imm32(both_mask_vec, both_mask);
    XMMRegister both_chars_vec = xmm1;
    splat_imm32(both_chars_vec, both_chars);
    XMMRegister mask1_vec = xmm2;
    splat_imm32(mask1_vec, mask1);
    XMMRegister chars1_vec = xmm3;
    splat_imm32(chars1_vec, chars1);
    XMMRegister mask2_vec = xmm4;
    splat_imm32(mask2_vec, mask2);
    XMMRegister chars2_vec = xmm5;
    splat_imm32(chars2_vec, chars2);

    Bind(&simd_repeat);

    // Load next characters into vectors.
    XMMRegister input_vec1 = xmm6;
    XMMRegister input_vec2 = xmm7;
    XMMRegister input_vec3 = xmm8;
    XMMRegister input_vec4 = xmm9;

    __ Movdqu(input_vec1, Operand(rsi, rdi, times_1, cp_offset));
    __ Movdqu(input_vec2, Operand(rsi, rdi, times_1, cp_offset + 1));
    __ Movdqu(input_vec3, Operand(rsi, rdi, times_1, cp_offset + 2));
    __ Movdqu(input_vec4, Operand(rsi, rdi, times_1, cp_offset + 3));

    // Helper to check if any of 4 input vectors matches. I.e. computes (input &
    // mask) == characters for each input. If any input matched, |result| is set
    // to a value != 0. We don't try to compute the exact match index, as this
    // is rather expensive. Instead we use the scalar version to find the exact
    // match index within a block.
    XMMRegister result = xmm10;
    auto AndCheck4CharsSimd =
        [this, input_vec1, input_vec2, input_vec3, input_vec4](
            XMMRegister res, XMMRegister characters, XMMRegister mask) {
          XMMRegister tmp = xmm11;
          if (CpuFeatures::IsSupported(AVX)) {
            CpuFeatureScope avx_scope(masm(), AVX);
            __ Andps(tmp, mask, input_vec1);
            Pcmpeq(masm(), res, tmp, characters, character_count);
            __ Andps(tmp, mask, input_vec2);
            Pcmpeq(masm(), tmp, tmp, characters, character_count);
            __ Orps(res, res, tmp);
            __ Andps(tmp, mask, input_vec3);
            Pcmpeq(masm(), tmp, tmp, characters, character_count);
            __ Orps(res, res, tmp);
            __ Andps(tmp, mask, input_vec4);
            Pcmpeq(masm(), tmp, tmp, characters, character_count);
            __ Orps(res, res, tmp);
          } else {
            __ Movdqa(tmp, mask);
            __ Andps(tmp, tmp, input_vec1);
            Pcmpeq(masm(), tmp, tmp, characters, character_count);
            __ Movdqa(res, tmp);
            __ Movdqa(tmp, mask);
            __ Andps(tmp, tmp, input_vec2);
            Pcmpeq(masm(), tmp, tmp, characters, character_count);
            __ Orps(res, res, tmp);
            __ Movdqa(tmp, mask);
            __ Andps(tmp, tmp, input_vec3);
            Pcmpeq(masm(), tmp, tmp, characters, character_count);
            __ Orps(res, res, tmp);
            __ Movdqa(tmp, mask);
            __ Andps(tmp, tmp, input_vec4);
            Pcmpeq(masm(), tmp, tmp, characters, character_count);
            __ Orps(res, res, tmp);
          }
        };

    // Tests if any bit was set. Sets ZF to 0 if any bit was set.
    auto TestAnySet = [this](XMMRegister reg) {
      Register scratch = r11;
      if (CpuFeatures::IsSupported(SSE4_1)) {
        CpuFeatureScope sse4_scope(masm(), SSE4_1);
        __ Ptest(reg, reg);
      } else {
        __ Pmovmskb(scratch, reg);
        __ testl(scratch, scratch);
      }
    };

    // Check both_chars with both_mask.
    AndCheck4CharsSimd(result, both_chars_vec, both_mask_vec);
    TestAnySet(result);
    __ j(not_zero, &found);

    AdvanceCurrentPosition(kCharsPerVector);
    CheckPosition(max_stride_offset, &scalar_after_simd);
    __ jmp(&simd_repeat);

    Bind(&found);
    // Check chars1 with mask1.
    AndCheck4CharsSimd(result, chars1_vec, mask1_vec);
    TestAnySet(result);
    __ j(not_zero, &scalar_after_simd);
    // Check chars2 with mask2.
    AndCheck4CharsSimd(result, chars2_vec, mask2_vec);
    TestAnySet(result);
    __ j(not_zero, &scalar_after_simd);
    AdvanceCurrentPosition(kCharsPerVector);
    CheckPosition(max_stride_offset, &scalar_after_simd);
    __ jmp(&simd_repeat);

    Bind(&scalar_after_simd);
    // Restore callee-saved XMM registers.
    __ movupd(xmm11, Operand(rsp, 5 * kVectorSize));
    __ movupd(xmm10, Operand(rsp, 4 * kVectorSize));
    __ movupd(xmm9, Operand(rsp, 3 * kVectorSize));
    __ movupd(xmm8, Operand(rsp, 2 * kVectorSize));
    __ movupd(xmm7, Operand(rsp, 1 * kVectorSize));
    __ movupd(xmm6, Operand(rsp, 0));
    __ addq(rsp, Immediate(6 * kVectorSize));

    Bind(&scalar);
  }

  // Scalar version.
  {
    Label found;
    Bind(&scalar_repeat);
    DCHECK_GE(max_offset, cp_offset + character_count);
    CheckPosition(max_offset, on_failure);
    LoadCurrentCharacterUnchecked(cp_offset, character_count);

    if (both_chars == 0) {
      __ testl(current_character(), Immediate(both_mask));
    } else {
      __ Move(rax, both_mask);
      __ andq(rax, current_character());
      __ cmpl(rax, Immediate(both_chars));
    }

    __ j(equal, &found);
    AdvanceCurrentPosition(advance_by);
    __ jmp(&scalar_repeat);

    Bind(&found);
    __ Move(rax, mask1);
    __ andq(rax, current_character());
    __ cmpl(rax, Immediate(chars1));
    __ j(equal, on_match1);

    __ Move(rax, mask2);
    __ andq(rax, current_character());
    __ cmpl(rax, Immediate(chars2));
    __ j(equal, on_match2);
    AdvanceCurrentPosition(advance_by);
    __ jmp(&scalar_repeat);
  }
}

bool RegExpMacroAssemblerX64::SkipUntilOneOfMaskedUseSimd(int advance_by) {
  // We only use SIMD instead of the scalar version if we advance by 1 byte
  // in each iteration. For higher values the scalar version performs better.
  return v8_flags.regexp_simd && advance_by * char_size() == 1;
}

bool RegExpMacroAssemblerX64::SkipUntilOneOfMasked3UseSimd(
    const SkipUntilOneOfMasked3Args& args) {
  // To use the SIMD variant we require SSSE3 as there is no shuffle equivalent
  // in older extensions.
  return v8_flags.regexp_simd && char_size() == 1 &&
         CpuFeatures::IsSupported(SSSE3);
}

void RegExpMacroAssemblerX64::SkipUntilOneOfMasked3(
    const SkipUntilOneOfMasked3Args& args) {
  // See the definition of the BC_SKIP_UNTIL_ONE_OF_MASKED3 peephole bytecode
  // for more context. The initial bytecode sequence is:
  //
  // sequence offset name
  // bc0   0  SKIP_UNTIL_BIT_IN_TABLE
  // bc1  20  CHECK_CURRENT_POSITION
  // bc2  28  LOAD_4_CURRENT_CHARS_UNCHECKED
  // bc3  2c  AND_CHECK_4_CHARS
  // bc4  3c  ADVANCE_CP_AND_GOTO
  // bc5  48  LOAD_4_CURRENT_CHARS
  // bc6  4c  AND_CHECK_4_CHARS
  // bc7  5c  AND_CHECK_4_CHARS
  // bc8  6c  AND_CHECK_NOT_4_CHARS

  if (!SkipUntilOneOfMasked3UseSimd(args)) {
    // If we cannot optimize the entire sequence, fall back to the generic
    // implementation. Note we may still use simd to optimize a subsequence
    // there.
    RegExpMacroAssembler::SkipUntilOneOfMasked3(args);
    return;
  }

  DCHECK(!args.bc0_nibble_table.is_null());
  DCHECK(!args.bc0_table.is_null());

  Label scalar_fallback;

  // We need to load 4 chars at bc2 and bc5.
  static constexpr int kCharsPerLoad = 4;
  int max_on_match_lookahead =
      std::max(args.bc2_cp_offset, args.bc5_cp_offset) + kCharsPerLoad;

  EmitSkipUntilBitInTableSimdHelper(
      args.bc0_cp_offset, args.bc0_advance_by, args.bc0_nibble_table,
      max_on_match_lookahead, &scalar_fallback,
      [&](Register index, Register callee_saved) {
        // SkipUntilBitInTable has matched at offset `index`. Bounds checks
        // have ensured we can safely perform the below loads without checks.
        //
        // The following inner checks are done using simple scalar code.
        Label bc5_load, continue_outer_loop, pop_and_goto_bc6_on_equal,
            pop_and_goto_bc7_on_equal;

        // The current position is temporarily advanced for this inner block.
        // If no match is found, it is reverted to the previous state before
        // returning to simd code for the next loop iteration.
        // The callee_saved register must be preserved across the inner block.
        static constexpr int kPushedRegisters = 2;
        __ pushq(rdi);
        __ pushq(callee_saved);
        __ addq(rdi, index);

        // bc2: Load.
        LoadCurrentCharacter(args.bc2_cp_offset, nullptr, false, kCharsPerLoad);

        // bc3: Check.
        CheckCharacterAfterAnd(args.bc3_characters, args.bc3_mask, &bc5_load);
        GoTo(&continue_outer_loop);

        Bind(&bc5_load);
        // bc5: Load.
        LoadCurrentCharacter(args.bc5_cp_offset, nullptr, false, kCharsPerLoad);

        // bc6, bc7, bc8.
        CheckCharacterAfterAnd(args.bc6_characters, args.bc6_mask,
                               &pop_and_goto_bc6_on_equal);
        CheckCharacterAfterAnd(args.bc7_characters, args.bc7_mask,
                               &pop_and_goto_bc7_on_equal);
        CheckNotCharacterAfterAnd(args.bc8_characters, args.bc8_mask,
                                  &continue_outer_loop);

        // Success cases:
        __ Drop(kPushedRegisters);
        GoTo(args.fallthrough_jump_target);

        Bind(&pop_and_goto_bc6_on_equal);
        __ Drop(kPushedRegisters);
        GoTo(args.bc6_on_equal);

        Bind(&pop_and_goto_bc7_on_equal);
        __ Drop(kPushedRegisters);
        GoTo(args.bc7_on_equal);

        Bind(&continue_outer_loop);
        // Restore the previous current position before continuing.
        __ popq(callee_saved);
        __ popq(rdi);
      });

  Bind(&scalar_fallback);
  // TODO(jgruber): The fallback is only reached when close to the end of the
  // subject string, and simd can no longer be used. Jump directly to the scalar
  // path.
  RegExpMacroAssembler::SkipUntilOneOfMasked3(args);
}

void RegExpMacroAssemblerX64::CheckSpecialClassRanges(StandardCharacterSet type,
                                                      Label* on_no_match) {
  DCHECK(CanOptimizeSpecialClassRanges(type));
  // Range checks (c in min..max) are generally implemented by an unsigned
  // (c - min) <= (max - min) check, using the sequence:
  //   leal(rax, Operand(current_character(), -min)) or sub(rax, Immediate(min))
  //   cmpl(rax, Immediate(max - min))
  // TODO(jgruber): No custom implementation (yet): s(UC16), S(UC16).
  switch (type) {
    case StandardCharacterSet::kWhitespace: {
      // Match space-characters.
      DCHECK_EQ(mode(), LATIN1);
      // One byte space characters are '\t'..'\r', ' ' and \u00a0.
      Label success;
      __ cmpl(current_character(), Immediate(' '));
      __ j(equal, &success, Label::kNear);
      // Check range 0x09..0x0D.
      __ leal(rax, Operand(current_character(), -'\t'));
      __ cmpl(rax, Immediate('\r' - '\t'));
      __ j(below_equal, &success, Label::kNear);
      // \u00a0 (NBSP).
      __ cmpl(rax, Immediate(0x00A0 - '\t'));
      BranchOrBacktrack(not_equal, on_no_match);
      __ bind(&success);
      break;
    }
    case StandardCharacterSet::kNotWhitespace:
      UNREACHABLE();
    case StandardCharacterSet::kDigit:
      // Match ASCII digits ('0'..'9').
      __ leal(rax, Operand(current_character(), -'0'));
      __ cmpl(rax, Immediate('9' - '0'));
      BranchOrBacktrack(above, on_no_match);
      break;
    case StandardCharacterSet::kNotDigit:
      // Match non ASCII-digits.
      __ leal(rax, Operand(current_character(), -'0'));
      __ cmpl(rax, Immediate('9' - '0'));
      BranchOrBacktrack(below_equal, on_no_match);
      break;
    case StandardCharacterSet::kNotLineTerminator: {
      // Match non-newlines (not 0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029).
      __ movl(rax, current_character());
      __ xorl(rax, Immediate(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C.
      __ subl(rax, Immediate(0x0B));
      __ cmpl(rax, Immediate(0x0C - 0x0B));
      BranchOrBacktrack(below_equal, on_no_match);
      if (mode() == UC16) {
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ subl(rax, Immediate(0x2028 - 0x0B));
        __ cmpl(rax, Immediate(0x2029 - 0x2028));
        BranchOrBacktrack(below_equal, on_no_match);
      }
      break;
    }
    case StandardCharacterSet::kLineTerminator: {
      // Match newlines (0x0A('\n'), 0x0D('\r'), 0x2028 and 0x2029).
      __ movl(rax, current_character());
      __ xorl(rax, Immediate(0x01));
      // See if current character is '\n'^1 or '\r'^1, i.e., 0x0B or 0x0C.
      __ subl(rax, Immediate(0x0B));
      __ cmpl(rax, Immediate(0x0C - 0x0B));
      if (mode() == LATIN1) {
        BranchOrBacktrack(above, on_no_match);
      } else {
        Label done;
        BranchOrBacktrack(below_equal, &done);
        // Compare original value to 0x2028 and 0x2029, using the already
        // computed (current_char ^ 0x01 - 0x0B). I.e., check for
        // 0x201D (0x2028 - 0x0B) or 0x201E.
        __ subl(rax, Immediate(0x2028 - 0x0B));
        __ cmpl(rax, Immediate(0x2029 - 0x2028));
        BranchOrBacktrack(above, on_no_match);
        __ bind(&done);
      }
      break;
    }
    case StandardCharacterSet::kWord: {
      if (mode() != LATIN1) {
        // Table is 256 entries, so all Latin1 characters can be tested.
        __ cmpl(current_character(), Immediate('z'));
        BranchOrBacktrack(above, on_no_match);
      }
      __ Move(rcx, ExternalReference::re_word_character_map());
      DCHECK_EQ(0,
                word_character_map()[0]);  // Character '\0' is not a word char.
      __ testb(Operand(rcx, current_character(), times_1, 0),
               current_character());
      BranchOrBacktrack(zero, on_no_match);
      break;
    }
    case StandardCharacterSet::kNotWord: {
      Label done;
      if (mode() != LATIN1) {
        // Table is 256 entries, so all Latin1 characters can be tested.
        __ cmpl(current_character(), Immediate('z'));
        __ j(above, &done);
      }
      __ Move(rcx, ExternalReference::re_word_character_map());
      DCHECK_EQ(0,
                word_character_map()[0]);  // Character '\0' is not a word char.
      __ testb(Operand(rcx, current_character(), times_1, 0),
               current_character());
      BranchOrBacktrack(not_zero, on_no_match);
      if (mode() != LATIN1) {
        __ bind(&done);
      }
      break;
    }

    case StandardCharacterSet::kEverything:
      // Match any character.
      break;
  }
}

void RegExpMacroAssemblerX64::BindJumpTarget(Label* label) {
  Bind(label);
  // TODO(sroettger): There should be an endbr64 instruction here, but it needs
  // more thought how to avoid perf regressions.
}

void RegExpMacroAssemblerX64::Fail() {
  static_assert(FAILURE == 0);  // Return value for failure is zero.
  if (!global()) {
    __ Move(rax, FAILURE);
  }
  __ jmp(&exit_label_);
}

void RegExpMacroAssemblerX64::LoadRegExpStackPointerFromMemory(Register dst) {
  ExternalReference ref =
      ExternalReference::address_of_regexp_stack_stack_pointer(isolate());
  __ movq(dst, __ ExternalReferenceAsOperand(ref, dst));
}

void RegExpMacroAssemblerX64::StoreRegExpStackPointerToMemory(
    Register src, Register scratch) {
  ExternalReference ref =
      ExternalReference::address_of_regexp_stack_stack_pointer(isolate());
  __ movq(__ ExternalReferenceAsOperand(ref, scratch), src);
}

void RegExpMacroAssemblerX64::PushRegExpBasePointer(Register stack_pointer,
                                                    Register scratch) {
  ExternalReference ref =
      ExternalReference::address_of_regexp_stack_memory_top_address(isolate());
  __ movq(scratch, __ ExternalReferenceAsOperand(ref, scratch));
  __ subq(scratch, stack_pointer);
  __ movq(Operand(rbp, kRegExpStackBasePointerOffset), scratch);
}

void RegExpMacroAssemblerX64::PopRegExpBasePointer(Register stack_pointer_out,
                                                   Register scratch) {
  ExternalReference ref =
      ExternalReference::address_of_regexp_stack_memory_top_address(isolate());
  __ movq(scratch, Operand(rbp, kRegExpStackBasePointerOffset));
  __ movq(stack_pointer_out,
          __ ExternalReferenceAsOperand(ref, stack_pointer_out));
  __ subq(stack_pointer_out, scratch);
  StoreRegExpStackPointerToMemory(stack_pointer_out, scratch);
}

DirectHandle<HeapObject> RegExpMacroAssemblerX64::GetCode(
    DirectHandle<String> source, RegExpFlags flags) {
  Label return_rax;
  // Finalize code - write the entry point code now we know how many registers
  // we need.
  __ bind(&entry_label_);

  // Tell the system that we have a stack frame. Because the type is MANUAL, no
  // physical frame is generated.
  FrameScope scope(&masm_, StackFrame::MANUAL);

#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  __ AssertInSandboxedExecutionMode();
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT

  // Actually emit code to start a new stack frame. This pushes the frame type
  // marker into the stack slot at kFrameTypeOffset.
  static_assert(kFrameTypeOffset == -1 * kSystemPointerSize);
  __ EnterFrame(StackFrame::IRREGEXP);

  // Save parameters and callee-save registers. Order here should correspond
  //  to order of kBackup_ebx etc.
#ifdef V8_TARGET_OS_WIN
  // MSVC passes arguments in rcx, rdx, r8, r9, with backing stack slots.
  // Store register parameters in pre-allocated stack slots.
  __ movq(Operand(rbp, kInputStringOffset), kCArgRegs[0]);
  __ movq(Operand(rbp, kStartIndexOffset),
          kCArgRegs[1]);  // Passed as int32 in edx.
  __ movq(Operand(rbp, kInputStartOffset), kCArgRegs[2]);
  __ movq(Operand(rbp, kInputEndOffset), kCArgRegs[3]);

  static_assert(kNumCalleeSaveRegisters == 4);
  static_assert(kBackupRsiOffset == -2 * kSystemPointerSize);
  static_assert(kBackupRdiOffset == -3 * kSystemPointerSize);
  static_assert(kBackupRbxOffset == -4 * kSystemPointerSize);
  static_assert(kBackupR12Offset == -5 * kSystemPointerSize);
  __ pushq(rsi);
  __ pushq(rdi);
  __ pushq(rbx);
  __ pushq(r12);
#else
  // GCC passes arguments in rdi, rsi, rdx, rcx, r8, r9 (and then on stack).
  // Push register parameters on stack for reference.
  static_assert(kInputStringOffset == -2 * kSystemPointerSize);
  static_assert(kStartIndexOffset == -3 * kSystemPointerSize);
  static_assert(kInputStartOffset == -4 * kSystemPointerSize);
  static_assert(kInputEndOffset == -5 * kSystemPointerSize);
  static_assert(kRegisterOutputOffset == -6 * kSystemPointerSize);
  static_assert(kNumOutputRegistersOffset == -7 * kSystemPointerSize);
  __ pushq(kCArgRegs[0]);
  __ pushq(kCArgRegs[1]);
  __ pushq(kCArgRegs[2]);
  __ pushq(kCArgRegs[3]);
  __ pushq(r8);
  __ pushq(r9);

  static_assert(kNumCalleeSaveRegisters == 2);
  static_assert(kBackupRbxOffset == -8 * kSystemPointerSize);
  static_assert(kBackupR12Offset == -9 * kSystemPointerSize);
  __ pushq(rbx);
  __ pushq(r12);
#endif

  static_assert(kSuccessfulCapturesOffset ==
                kLastCalleeSaveRegister - kSystemPointerSize);
  __ Push(Immediate(0));  // Number of successful matches in a global regexp.
  static_assert(kStringStartMinusOneOffset ==
                kSuccessfulCapturesOffset - kSystemPointerSize);
  __ Push(Immediate(0));  // Make room for "string start - 1" constant.
  static_assert(kBacktrackCountOffset ==
                kStringStartMinusOneOffset - kSystemPointerSize);
  __ Push(Immediate(0));  // The backtrack counter.
  static_assert(kRegExpStackBasePointerOffset ==
                kBacktrackCountOffset - kSystemPointerSize);
  __ Push(Immediate(0));  // The regexp stack base ptr.

  // Initialize backtrack stack pointer. It must not be clobbered from here on.
  // Note the backtrack_stackpointer is callee-saved.
  static_assert(backtrack_stackpointer() == rbx);
  LoadRegExpStackPointerFromMemory(backtrack_stackpointer());

  // Store the regexp base pointer - we'll later restore it / write it to
  // memory when returning from this irregexp code object.
  PushRegExpBasePointer(backtrack_stackpointer(), kScratchRegister);

  {
    // Check if we have space on the stack for registers.
    Label stack_limit_hit, stack_ok;

    ExternalReference stack_limit =
        ExternalReference::address_of_jslimit(isolate());
    __ movq(r9, rsp);
    __ Move(kScratchRegister, stack_limit);
    __ subq(r9, Operand(kScratchRegister, 0));
    Immediate extra_space_for_variables(num_registers_ * kSystemPointerSize);

    // Handle it if the stack pointer is already below the stack limit.
    __ j(below_equal, &stack_limit_hit);
    // Check if there is room for the variable number of registers above
    // the stack limit.
    __ cmpq(r9, extra_space_for_variables);
    __ j(above_equal, &stack_ok);
    // Exit with OutOfMemory exception. There is not enough space on the stack
    // for our working registers.
    __ Move(rax, EXCEPTION);
    __ jmp(&return_rax);

    __ bind(&stack_limit_hit);
    __ Move(code_object_pointer(), masm_.CodeObject());
    __ pushq(backtrack_stackpointer());
    // CallCheckStackGuardState preserves no registers beside rbp and rsp.
    CallCheckStackGuardState(extra_space_for_variables);
    __ popq(backtrack_stackpointer());
    __ testq(rax, rax);
    // If returned value is non-zero, we exit with the returned value as result.
    __ j(not_zero, &return_rax);

    __ bind(&stack_ok);
  }

  // Allocate space on stack for registers.
  __ AllocateStackSpace(num_registers_ * kSystemPointerSize);
  // Load string length.
  __ movq(rsi, Operand(rbp, kInputEndOffset));
  // Load input position.
  __ movq(rdi, Operand(rbp, kInputStartOffset));
  // Set up rdi to be negative offset from string end.
  __ subq(rdi, rsi);
  // Set rax to address of char before start of the string
  // (effectively string position -1).
  __ movq(rcx, Operand(rbp, kStartIndexOffset));
  __ negq(rcx);
  __ leaq(rax, Operand(rdi, rcx, CharSizeScaleFactor(), -char_size()));
  // Store this value in a local variable, for use when clearing
  // position registers.
  __ movq(Operand(rbp, kStringStartMinusOneOffset), rax);

  // Initialize code object pointer.
  __ Move(code_object_pointer(), masm_.CodeObject());

  Label load_char_start_regexp;  // Execution restarts here for global regexps.
  {
    Label start_regexp;

    // Load newline if index is at start, previous character otherwise.
    __ cmpl(Operand(rbp, kStartIndexOffset), Immediate(0));
    __ j(not_equal, &load_char_start_regexp, Label::kNear);
    __ Move(current_character(), '\n');
    __ jmp(&start_regexp, Label::kNear);

    // Global regexp restarts matching here.
    __ bind(&load_char_start_regexp);
    // Load previous char as initial value of current character register.
    LoadCurrentCharacterUnchecked(-1, 1);

    __ bind(&start_regexp);
  }

  // Initialize on-stack registers.
  if (num_saved_registers_ > 0) {
    // Fill saved registers with initial value = start offset - 1
    // Fill in stack push order, to avoid accessing across an unwritten
    // page (a problem on Windows).
    if (num_saved_registers_ > 8) {
      __ Move(r9, kRegisterZeroOffset);
      Label init_loop;
      __ bind(&init_loop);
      __ movq(Operand(rbp, r9, times_1, 0), rax);
      __ subq(r9, Immediate(kSystemPointerSize));
      __ cmpq(r9, Immediate(kRegisterZeroOffset -
                            num_saved_registers_ * kSystemPointerSize));
      __ j(greater, &init_loop);
    } else {  // Unroll the loop.
      for (int i = 0; i < num_saved_registers_; i++) {
        __ movq(register_location(i), rax);
      }
    }
  }

  __ jmp(&start_label_);

  // Exit code:
  if (success_label_.is_linked()) {
    // Save captures when successful.
    __ bind(&success_label_);
    if (num_saved_registers_ > 0) {
      // copy captures to output
      __ movq(rdx, Operand(rbp, kStartIndexOffset));
      __ movq(rbx, Operand(rbp, kRegisterOutputOffset));
      __ movq(rcx, Operand(rbp, kInputEndOffset));
      __ subq(rcx, Operand(rbp, kInputStartOffset));
      if (mode() == UC16) {
        __ leaq(rcx, Operand(rcx, rdx, CharSizeScaleFactor(), 0));
      } else {
        __ addq(rcx, rdx);
      }
      for (int i = 0; i < num_saved_registers_; i++) {
        __ movq(rax, register_location(i));
        if (i == 0 && global_with_zero_length_check()) {
          // Keep capture start in rdx for the zero-length check later.
          __ movq(rdx, rax);
        }
        __ addq(rax, rcx);  // Convert to index from start, not end.
        if (mode() == UC16) {
          __ sarq(rax, Immediate(1));  // Convert byte index to character index.
        }
        __ movl(Operand(rbx, i * kIntSize), rax);
      }
    }

    if (global()) {
      // Restart matching if the regular expression is flagged as global.
      // Increment success counter.
      __ incq(Operand(rbp, kSuccessfulCapturesOffset));
      // Capture results have been stored, so the number of remaining global
      // output registers is reduced by the number of stored captures.
      __ movsxlq(rcx, Operand(rbp, kNumOutputRegistersOffset));
      __ subq(rcx, Immediate(num_saved_registers_));
      // Check whether we have enough room for another set of capture results.
      __ cmpq(rcx, Immediate(num_saved_registers_));
      __ j(less, &exit_label_);

      __ movq(Operand(rbp, kNumOutputRegistersOffset), rcx);
      // Advance the location for output.
      __ addq(Operand(rbp, kRegisterOutputOffset),
              Immediate(num_saved_registers_ * kIntSize));

      // Restore the original regexp stack pointer value (effectively, pop the
      // stored base pointer).
      PopRegExpBasePointer(backtrack_stackpointer(), kScratchRegister);

      Label reload_string_start_minus_one;

      if (global_with_zero_length_check()) {
        // Special case for zero-length matches.
        // rdx: capture start index
        __ cmpq(rdi, rdx);
        // Not a zero-length match, restart.
        __ j(not_equal, &reload_string_start_minus_one);
        // rdi (offset from the end) is zero if we already reached the end.
        __ testq(rdi, rdi);
        __ j(zero, &exit_label_, Label::kNear);
        // Advance current position after a zero-length match.
        Label advance;
        __ bind(&advance);
        if (mode() == UC16) {
          __ addq(rdi, Immediate(2));
        } else {
          __ incq(rdi);
        }
        if (global_unicode()) CheckNotInSurrogatePair(0, &advance);
      }

      __ bind(&reload_string_start_minus_one);
      // Prepare rax to initialize registers with its value in the next run.
      // Must be immediately before the jump to avoid clobbering.
      __ movq(rax, Operand(rbp, kStringStartMinusOneOffset));

      __ jmp(&load_char_start_regexp);
    } else {
      __ Move(rax, SUCCESS);
    }
  }

  __ bind(&exit_label_);
  if (global()) {
    // Return the number of successful captures.
    __ movq(rax, Operand(rbp, kSuccessfulCapturesOffset));
  }

  __ bind(&return_rax);
  // Restore the original regexp stack pointer value (effectively, pop the
  // stored base pointer).
  PopRegExpBasePointer(backtrack_stackpointer(), kScratchRegister);

#ifdef V8_TARGET_OS_WIN
  // Restore callee save registers.
  __ leaq(rsp, Operand(rbp, kLastCalleeSaveRegister));
  static_assert(kNumCalleeSaveRegisters == 4);
  static_assert(kBackupRsiOffset == -2 * kSystemPointerSize);
  static_assert(kBackupRdiOffset == -3 * kSystemPointerSize);
  static_assert(kBackupRbxOffset == -4 * kSystemPointerSize);
  static_assert(kBackupR12Offset == -5 * kSystemPointerSize);
  __ popq(r12);
  __ popq(rbx);
  __ popq(rdi);
  __ popq(rsi);
#else
  // Restore callee save register.
  static_assert(kNumCalleeSaveRegisters == 2);
  __ movq(r12, Operand(rbp, kBackupR12Offset));
  __ movq(rbx, Operand(rbp, kBackupRbxOffset));
#endif

  __ LeaveFrame(StackFrame::IRREGEXP);
  __ ret(0);

  // Backtrack code (branch target for conditional backtracks).
  if (backtrack_label_.is_linked()) {
    __ bind(&backtrack_label_);
    Backtrack();
  }

  Label exit_with_exception;

  // Preempt-code.
  if (check_preempt_label_.is_linked()) {
    SafeCallTarget(&check_preempt_label_);

    __ pushq(rdi);

    StoreRegExpStackPointerToMemory(backtrack_stackpointer(), kScratchRegister);

    CallCheckStackGuardState();
    __ testq(rax, rax);
    // If returning non-zero, we should end execution with the given
    // result as return value.
    __ j(not_zero, &return_rax);

    // Restore registers.
    __ Move(code_object_pointer(), masm_.CodeObject());
    __ popq(rdi);

    LoadRegExpStackPointerFromMemory(backtrack_stackpointer());

    // String might have moved: Reload esi from frame.
    __ movq(rsi, Operand(rbp, kInputEndOffset));
    SafeReturn();
  }

  // Backtrack stack overflow code.
  if (stack_overflow_label_.is_linked()) {
    SafeCallTarget(&stack_overflow_label_);
    // Reached if the backtrack-stack limit has been hit.

    PushCallerSavedRegisters();

    // Call GrowStack(isolate).

    StoreRegExpStackPointerToMemory(backtrack_stackpointer(), kScratchRegister);

    static constexpr int kNumArguments = 1;
    __ PrepareCallCFunction(kNumArguments);
    __ LoadAddress(kCArgRegs[0], ExternalReference::isolate_address(isolate()));

    ExternalReference grow_stack = ExternalReference::re_grow_stack();
    CallCFunctionFromIrregexpCode(grow_stack, kNumArguments);
    // If nullptr is returned, we have failed to grow the stack, and must exit
    // with a stack-overflow exception.
    __ testq(rax, rax);
    __ j(equal, &exit_with_exception);
    PopCallerSavedRegisters();
    // Otherwise use return value as new stack pointer.
    __ movq(backtrack_stackpointer(), rax);
    // Restore saved registers and continue.
    __ Move(code_object_pointer(), masm_.CodeObject());
    SafeReturn();
  }

  if (exit_with_exception.is_linked()) {
    // If any of the code above needed to exit with an exception.
    __ bind(&exit_with_exception);
    // Exit with Result EXCEPTION(-1) to signal thrown exception.
    __ Move(rax, EXCEPTION);
    __ jmp(&return_rax);
  }

  if (fallback_label_.is_linked()) {
    __ bind(&fallback_label_);
    __ Move(rax, FALLBACK_TO_EXPERIMENTAL);
    __ jmp(&return_rax);
  }

  FixupCodeRelativePositions();

  CodeDesc code_desc;
  Isolate* isolate = this->isolate();
  masm_.GetCode(isolate, &code_desc);
  DirectHandle<Code> code =
      Factory::CodeBuilder(isolate, code_desc, CodeKind::REGEXP)
          .set_self_reference(masm_.CodeObject())
          .set_empty_source_position_table()
          .Build();
  PROFILE(isolate,
          RegExpCodeCreateEvent(Cast<AbstractCode>(code), source, flags));
  return Cast<HeapObject>(code);
}

void RegExpMacroAssemblerX64::GoTo(Label* to) { BranchOrBacktrack(to); }

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
  CheckStackLimit();
}


void RegExpMacroAssemblerX64::PushRegister(int register_index,
                                           StackCheckFlag check_stack_limit) {
  __ movq(rax, register_location(register_index));
  Push(rax);
  if (check_stack_limit == StackCheckFlag::kCheckStackLimit) {
    CheckStackLimit();
  } else if (V8_UNLIKELY(v8_flags.slow_debug_code)) {
    AssertAboveStackLimitMinusSlack();
  }
}

void RegExpMacroAssemblerX64::ReadCurrentPositionFromRegister(int reg) {
  __ movq(rdi, register_location(reg));
}


void RegExpMacroAssemblerX64::ReadPositionFromRegister(Register dst, int reg) {
  __ movq(dst, register_location(reg));
}

// Preserves a position-independent representation of the stack pointer in reg:
// reg = top - sp.
void RegExpMacroAssemblerX64::WriteStackPointerToRegister(int reg) {
  ExternalReference stack_top_address =
      ExternalReference::address_of_regexp_stack_memory_top_address(isolate());
  __ movq(rax, __ ExternalReferenceAsOperand(stack_top_address, rax));
  __ subq(rax, backtrack_stackpointer());
  __ movq(register_location(reg), rax);
}

void RegExpMacroAssemblerX64::ReadStackPointerFromRegister(int reg) {
  ExternalReference stack_top_address =
      ExternalReference::address_of_regexp_stack_memory_top_address(isolate());
  __ movq(backtrack_stackpointer(),
          __ ExternalReferenceAsOperand(stack_top_address,
                                        backtrack_stackpointer()));
  __ subq(backtrack_stackpointer(), register_location(reg));
}

void RegExpMacroAssemblerX64::SetCurrentPositionFromEnd(int by) {
  Label after_position;
  __ cmpq(rdi, Immediate(-by * char_size()));
  __ j(greater_equal, &after_position, Label::kNear);
  __ Move(rdi, -by * char_size());
  // On RegExp code entry (where this operation is used), the character before
  // the current position is expected to be already loaded.
  // We have advanced the position, so it's safe to read backwards.
  LoadCurrentCharacterUnchecked(-1, 1);
  __ bind(&after_position);
}


void RegExpMacroAssemblerX64::SetRegister(int register_index, int to) {
  DCHECK(register_index >= num_saved_registers_);  // Reserved for positions!
  __ movq(register_location(register_index), Immediate(to));
}


bool RegExpMacroAssemblerX64::Succeed() {
  __ jmp(&success_label_);
  return global();
}


void RegExpMacroAssemblerX64::WriteCurrentPositionToRegister(int reg,
                                                             int cp_offset) {
  if (cp_offset == 0) {
    __ movq(register_location(reg), rdi);
  } else {
    __ leaq(rax, Operand(rdi, cp_offset * char_size()));
    __ movq(register_location(reg), rax);
  }
}


void RegExpMacroAssemblerX64::ClearRegisters(int reg_from, int reg_to) {
  DCHECK(reg_from <= reg_to);
  __ movq(rax, Operand(rbp, kStringStartMinusOneOffset));
  for (int reg = reg_from; reg <= reg_to; reg++) {
    __ movq(register_location(reg), rax);
  }
}

// Private methods:

void RegExpMacroAssemblerX64::CallCheckStackGuardState(Immediate extra_space) {
  // This function call preserves no register values. Caller should
  // store anything volatile in a C call or overwritten by this function.
  static const int num_arguments = 4;
  __ PrepareCallCFunction(num_arguments);
#ifdef V8_TARGET_OS_WIN
  // Fourth argument: Extra space for variables.
  DCHECK_EQ(r9, kCArgRegs[3]);
  __ movq(kCArgRegs[3], extra_space);
  // Second argument: InstructionStream of self. (Do this before overwriting
  // r8 (kCArgRegs[2])).
  DCHECK_EQ(rdx, kCArgRegs[1]);
  __ movq(kCArgRegs[1], code_object_pointer());
  // Third argument: RegExp code frame pointer.
  DCHECK_EQ(r8, kCArgRegs[2]);
  __ movq(kCArgRegs[2], rbp);
  // First argument: Next address on the stack (will be address of
  // return address).
  DCHECK_EQ(rcx, kCArgRegs[0]);
  __ leaq(kCArgRegs[0], Operand(rsp, -kSystemPointerSize));
#else
  // Fourth argument: Extra space for variables.
  DCHECK_EQ(rcx, kCArgRegs[3]);
  __ movq(kCArgRegs[3], extra_space);
  // Third argument: RegExp code frame pointer.
  DCHECK_EQ(rdx, kCArgRegs[2]);
  __ movq(kCArgRegs[2], rbp);
  // Second argument: InstructionStream of self.
  DCHECK_EQ(rsi, kCArgRegs[1]);
  __ movq(kCArgRegs[1], code_object_pointer());
  // First argument: Next address on the stack (will be address of
  // return address).
  DCHECK_EQ(rdi, kCArgRegs[0]);
  __ leaq(kCArgRegs[0], Operand(rsp, -kSystemPointerSize));
#endif
  ExternalReference stack_check =
      ExternalReference::re_check_stack_guard_state();
  CallCFunctionFromIrregexpCode(stack_check, num_arguments);
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

int RegExpMacroAssemblerX64::CheckStackGuardState(Address* return_address,
                                                  Address raw_code,
                                                  Address re_frame,
                                                  uintptr_t extra_space) {
  Tagged<InstructionStream> re_code =
      SbxCast<InstructionStream>(Tagged<Object>(raw_code));
  return NativeRegExpMacroAssembler::CheckStackGuardState(
      frame_entry<Isolate*>(re_frame, kIsolateOffset),
      frame_entry<int>(re_frame, kStartIndexOffset),
      static_cast<RegExp::CallOrigin>(
          frame_entry<int>(re_frame, kDirectCallOffset)),
      return_address, re_code,
      frame_entry_address<Address>(re_frame, kInputStringOffset),
      frame_entry_address<const uint8_t*>(re_frame, kInputStartOffset),
      frame_entry_address<const uint8_t*>(re_frame, kInputEndOffset),
      extra_space);
}

Operand RegExpMacroAssemblerX64::register_location(int register_index) {
  DCHECK(register_index < (1<<30));
  if (num_registers_ <= register_index) {
    num_registers_ = register_index + 1;
  }
  return Operand(rbp,
                 kRegisterZeroOffset - register_index * kSystemPointerSize);
}


void RegExpMacroAssemblerX64::CheckPosition(int cp_offset,
                                            Label* on_outside_input) {
  if (cp_offset >= 0) {
    __ cmpl(rdi, Immediate(-cp_offset * char_size()));
    BranchOrBacktrack(greater_equal, on_outside_input);
  } else {
    __ leaq(rax, Operand(rdi, cp_offset * char_size()));
    __ cmpq(rax, Operand(rbp, kStringStartMinusOneOffset));
    BranchOrBacktrack(less_equal, on_outside_input);
  }
}

void RegExpMacroAssemblerX64::BranchOrBacktrack(Label* to) {
  if (to == nullptr) {
    Backtrack();
    return;
  }
  __ jmp(to);
}

void RegExpMacroAssemblerX64::BranchOrBacktrack(Condition condition,
                                                Label* to) {
  __ j(condition, to ? to : &backtrack_label_);
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
  DCHECK(source != backtrack_stackpointer());
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
  for (int position : code_relative_fixup_positions_) {
    // The position succeeds a relative label offset from position.
    // Patch the relative offset to be relative to the InstructionStream object
    // pointer instead.
    int patch_position = position - kIntSize;
    int offset = masm_.long_at(patch_position);
    masm_.long_at_put(
        patch_position,
        offset + position + InstructionStream::kHeaderSize - kHeapObjectTag);
  }
  code_relative_fixup_positions_.Rewind(0);
}


void RegExpMacroAssemblerX64::Push(Label* backtrack_target) {
  __ subq(backtrack_stackpointer(), Immediate(kIntSize));
  __ movl(Operand(backtrack_stackpointer(), 0), backtrack_target);
  MarkPositionForCodeRelativeFixup();
}


void RegExpMacroAssemblerX64::Pop(Register target) {
  DCHECK(target != backtrack_stackpointer());
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
      ExternalReference::address_of_jslimit(isolate());
  __ load_rax(stack_limit);
  __ cmpq(rsp, rax);
  __ j(above, &no_preempt);

  SafeCall(&check_preempt_label_);

  __ bind(&no_preempt);
}


void RegExpMacroAssemblerX64::CheckStackLimit() {
  Label no_stack_overflow;
  ExternalReference stack_limit =
      ExternalReference::address_of_regexp_stack_limit_address(isolate());
  __ load_rax(stack_limit);
  __ cmpq(backtrack_stackpointer(), rax);
  __ j(above, &no_stack_overflow);

  SafeCall(&stack_overflow_label_);

  __ bind(&no_stack_overflow);
}

void RegExpMacroAssemblerX64::AssertAboveStackLimitMinusSlack() {
  DCHECK(v8_flags.slow_debug_code);
  Label no_stack_overflow;
  ASM_CODE_COMMENT_STRING(&masm_, "AssertAboveStackLimitMinusSlack");
  auto l = ExternalReference::address_of_regexp_stack_limit_address(isolate());
  __ load_rax(l);
  __ subq(rax, Immediate(RegExpStack::kStackLimitSlackSize));
  __ cmpq(backtrack_stackpointer(), rax);
  __ j(above, &no_stack_overflow);
  __ int3();
  __ bind(&no_stack_overflow);
}

void RegExpMacroAssemblerX64::LoadCurrentCharacterUnchecked(int cp_offset,
                                                            int characters) {
  if (mode() == LATIN1) {
    if (characters == 4) {
      __ movl(current_character(), Operand(rsi, rdi, times_1, cp_offset));
    } else if (characters == 2) {
      __ movzxwl(current_character(), Operand(rsi, rdi, times_1, cp_offset));
    } else {
      DCHECK_EQ(1, characters);
      __ movzxbl(current_character(), Operand(rsi, rdi, times_1, cp_offset));
    }
  } else {
    DCHECK(mode() == UC16);
    if (characters == 2) {
      __ movl(current_character(),
              Operand(rsi, rdi, times_1, cp_offset * sizeof(base::uc16)));
    } else {
      DCHECK_EQ(1, characters);
      __ movzxwl(current_character(),
                 Operand(rsi, rdi, times_1, cp_offset * sizeof(base::uc16)));
    }
  }
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

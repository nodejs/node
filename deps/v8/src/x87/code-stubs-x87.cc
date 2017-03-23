// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X87

#include "src/code-stubs.h"
#include "src/api-arguments.h"
#include "src/base/bits.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/isolate.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-macro-assembler.h"
#include "src/runtime/runtime.h"
#include "src/x87/code-stubs-x87.h"
#include "src/x87/frames-x87.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ArrayNArgumentsConstructorStub::Generate(MacroAssembler* masm) {
  __ pop(ecx);
  __ mov(MemOperand(esp, eax, times_4, 0), edi);
  __ push(edi);
  __ push(ebx);
  __ push(ecx);
  __ add(eax, Immediate(3));
  __ TailCallRuntime(Runtime::kNewArray);
}

void HydrogenCodeStub::GenerateLightweightMiss(MacroAssembler* masm,
                                               ExternalReference miss) {
  // Update the static counter each time a new code stub is generated.
  isolate()->counters()->code_stubs()->Increment();

  CallInterfaceDescriptor descriptor = GetCallInterfaceDescriptor();
  int param_count = descriptor.GetRegisterParameterCount();
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    DCHECK(param_count == 0 ||
           eax.is(descriptor.GetRegisterParameter(param_count - 1)));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ push(descriptor.GetRegisterParameter(i));
    }
    __ CallExternalReference(miss, param_count);
  }

  __ ret(0);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  __ pushad();
  if (save_doubles()) {
    // Save FPU stat in m108byte.
    __ sub(esp, Immediate(108));
    __ fnsave(Operand(esp, 0));
  }
  const int argument_count = 1;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, ecx);
  __ mov(Operand(esp, 0 * kPointerSize),
         Immediate(ExternalReference::isolate_address(isolate())));
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(isolate()),
      argument_count);
  if (save_doubles()) {
    // Restore FPU stat in m108byte.
    __ frstor(Operand(esp, 0));
    __ add(esp, Immediate(108));
  }
  __ popad();
  __ ret(0);
}


class FloatingPointHelper : public AllStatic {
 public:
  enum ArgLocation {
    ARGS_ON_STACK,
    ARGS_IN_REGISTERS
  };

  // Code pattern for loading a floating point value. Input value must
  // be either a smi or a heap number object (fp value). Requirements:
  // operand in register number. Returns operand as floating point number
  // on FPU stack.
  static void LoadFloatOperand(MacroAssembler* masm, Register number);

  // Test if operands are smi or number objects (fp). Requirements:
  // operand_1 in eax, operand_2 in edx; falls through on float
  // operands, jumps to the non_float label otherwise.
  static void CheckFloatOperands(MacroAssembler* masm,
                                 Label* non_float,
                                 Register scratch);
};


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Register input_reg = this->source();
  Register final_result_reg = this->destination();
  DCHECK(is_truncating());

  Label check_negative, process_64_bits, done, done_no_stash;

  int double_offset = offset();

  // Account for return address and saved regs if input is esp.
  if (input_reg.is(esp)) double_offset += 3 * kPointerSize;

  MemOperand mantissa_operand(MemOperand(input_reg, double_offset));
  MemOperand exponent_operand(MemOperand(input_reg,
                                         double_offset + kDoubleSize / 2));

  Register scratch1;
  {
    Register scratch_candidates[3] = { ebx, edx, edi };
    for (int i = 0; i < 3; i++) {
      scratch1 = scratch_candidates[i];
      if (!final_result_reg.is(scratch1) && !input_reg.is(scratch1)) break;
    }
  }
  // Since we must use ecx for shifts below, use some other register (eax)
  // to calculate the result if ecx is the requested return register.
  Register result_reg = final_result_reg.is(ecx) ? eax : final_result_reg;
  // Save ecx if it isn't the return register and therefore volatile, or if it
  // is the return register, then save the temp register we use in its stead for
  // the result.
  Register save_reg = final_result_reg.is(ecx) ? eax : ecx;
  __ push(scratch1);
  __ push(save_reg);

  bool stash_exponent_copy = !input_reg.is(esp);
  __ mov(scratch1, mantissa_operand);
  __ mov(ecx, exponent_operand);
  if (stash_exponent_copy) __ push(ecx);

  __ and_(ecx, HeapNumber::kExponentMask);
  __ shr(ecx, HeapNumber::kExponentShift);
  __ lea(result_reg, MemOperand(ecx, -HeapNumber::kExponentBias));
  __ cmp(result_reg, Immediate(HeapNumber::kMantissaBits));
  __ j(below, &process_64_bits);

  // Result is entirely in lower 32-bits of mantissa
  int delta = HeapNumber::kExponentBias + Double::kPhysicalSignificandSize;
  __ sub(ecx, Immediate(delta));
  __ xor_(result_reg, result_reg);
  __ cmp(ecx, Immediate(31));
  __ j(above, &done);
  __ shl_cl(scratch1);
  __ jmp(&check_negative);

  __ bind(&process_64_bits);
  // Result must be extracted from shifted 32-bit mantissa
  __ sub(ecx, Immediate(delta));
  __ neg(ecx);
  if (stash_exponent_copy) {
    __ mov(result_reg, MemOperand(esp, 0));
  } else {
    __ mov(result_reg, exponent_operand);
  }
  __ and_(result_reg,
          Immediate(static_cast<uint32_t>(Double::kSignificandMask >> 32)));
  __ add(result_reg,
         Immediate(static_cast<uint32_t>(Double::kHiddenBit >> 32)));
  __ shrd_cl(scratch1, result_reg);
  __ shr_cl(result_reg);
  __ test(ecx, Immediate(32));
  {
    Label skip_mov;
    __ j(equal, &skip_mov, Label::kNear);
    __ mov(scratch1, result_reg);
    __ bind(&skip_mov);
  }

  // If the double was negative, negate the integer result.
  __ bind(&check_negative);
  __ mov(result_reg, scratch1);
  __ neg(result_reg);
  if (stash_exponent_copy) {
    __ cmp(MemOperand(esp, 0), Immediate(0));
  } else {
    __ cmp(exponent_operand, Immediate(0));
  }
  {
    Label skip_mov;
    __ j(less_equal, &skip_mov, Label::kNear);
    __ mov(result_reg, scratch1);
    __ bind(&skip_mov);
  }

  // Restore registers
  __ bind(&done);
  if (stash_exponent_copy) {
    __ add(esp, Immediate(kDoubleSize / 2));
  }
  __ bind(&done_no_stash);
  if (!final_result_reg.is(result_reg)) {
    DCHECK(final_result_reg.is(ecx));
    __ mov(final_result_reg, result_reg);
  }
  __ pop(save_reg);
  __ pop(scratch1);
  __ ret(0);
}


void FloatingPointHelper::LoadFloatOperand(MacroAssembler* masm,
                                           Register number) {
  Label load_smi, done;

  __ JumpIfSmi(number, &load_smi, Label::kNear);
  __ fld_d(FieldOperand(number, HeapNumber::kValueOffset));
  __ jmp(&done, Label::kNear);

  __ bind(&load_smi);
  __ SmiUntag(number);
  __ push(number);
  __ fild_s(Operand(esp, 0));
  __ pop(number);

  __ bind(&done);
}


void FloatingPointHelper::CheckFloatOperands(MacroAssembler* masm,
                                             Label* non_float,
                                             Register scratch) {
  Label test_other, done;
  // Test if both operands are floats or smi -> scratch=k_is_float;
  // Otherwise scratch = k_not_float.
  __ JumpIfSmi(edx, &test_other, Label::kNear);
  __ mov(scratch, FieldOperand(edx, HeapObject::kMapOffset));
  Factory* factory = masm->isolate()->factory();
  __ cmp(scratch, factory->heap_number_map());
  __ j(not_equal, non_float);  // argument in edx is not a number -> NaN

  __ bind(&test_other);
  __ JumpIfSmi(eax, &done, Label::kNear);
  __ mov(scratch, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(scratch, factory->heap_number_map());
  __ j(not_equal, non_float);  // argument in eax is not a number -> NaN

  // Fall-through: Both operands are numbers.
  __ bind(&done);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  const Register scratch = ecx;

  // Load the double_exponent into x87 FPU
  __ fld_d(Operand(esp, 0 * kDoubleSize + 4));
  // Load the double_base into x87 FPU
  __ fld_d(Operand(esp, 1 * kDoubleSize + 4));

  // Call ieee754 runtime directly.
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ PrepareCallCFunction(4, scratch);
    // Put the double_base parameter in call stack
    __ fstp_d(Operand(esp, 0 * kDoubleSize));
    // Put the double_exponent parameter in call stack
    __ fstp_d(Operand(esp, 1 * kDoubleSize));
    __ CallCFunction(ExternalReference::power_double_double_function(isolate()),
                     4);
  }
  // Return value is in st(0) on ia32.
  __ ret(0);
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadDescriptor::ReceiverRegister();
  // With careful management, we won't have to save slot and vector on
  // the stack. Simply handle the possibly missing case first.
  // TODO(mvstanton): this code can be more efficient.
  __ cmp(FieldOperand(receiver, JSFunction::kPrototypeOrInitialMapOffset),
         Immediate(isolate()->factory()->the_hole_value()));
  __ j(equal, &miss);
  __ TryGetFunctionPrototype(receiver, eax, ebx, &miss);
  __ ret(0);

  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


void LoadIndexedStringStub::Generate(MacroAssembler* masm) {
  // Return address is on the stack.
  Label miss;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register index = LoadDescriptor::NameRegister();
  Register scratch = edi;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));
  Register result = eax;
  DCHECK(!result.is(scratch));
  DCHECK(!scratch.is(LoadWithVectorDescriptor::VectorRegister()) &&
         result.is(LoadDescriptor::SlotRegister()));

  // StringCharAtGenerator doesn't use the result register until it's passed
  // the different miss possibilities. If it did, we would have a conflict
  // when FLAG_vector_ics is true.

  StringCharAtGenerator char_at_generator(receiver, index, scratch, result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &miss,  // When index out of range.
                                          RECEIVER_IS_STRING);
  char_at_generator.GenerateFast(masm);
  __ ret(0);

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, PART_OF_IC_HANDLER, call_helper);

  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::KEYED_LOAD_IC));
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExec);
#else  // V8_INTERPRETED_REGEXP

  // Stack frame on entry.
  //  esp[0]: return address
  //  esp[4]: last_match_info (expected JSArray)
  //  esp[8]: previous index
  //  esp[12]: subject string
  //  esp[16]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime;
  Factory* factory = isolate()->factory();

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ mov(ebx, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ test(ebx, ebx);
  __ j(zero, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(eax, &runtime);
  __ CmpObjectType(eax, JS_REGEXP_TYPE, ecx);
  __ j(not_equal, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ test(ecx, Immediate(kSmiTagMask));
    __ Check(not_zero, kUnexpectedTypeForRegExpDataFixedArrayExpected);
    __ CmpObjectType(ecx, FIXED_ARRAY_TYPE, ebx);
    __ Check(equal, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // ecx: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ mov(ebx, FieldOperand(ecx, JSRegExp::kDataTagOffset));
  __ cmp(ebx, Immediate(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ j(not_equal, &runtime);

  // ecx: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // Multiplying by 2 comes for free since edx is smi-tagged.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ cmp(edx, Isolate::kJSRegexpStaticOffsetsVectorSize - 2);
  __ j(above, &runtime);

  // Reset offset for possibly sliced string.
  __ Move(edi, Immediate(0));
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ JumpIfSmi(eax, &runtime);
  __ mov(edx, eax);  // Make a copy of the original subject string.

  // eax: subject string
  // edx: subject string
  // ecx: RegExp data (FixedArray)
  // Handle subject string according to its encoding and representation:
  // (1) Sequential two byte?  If yes, go to (9).
  // (2) Sequential one byte?  If yes, go to (5).
  // (3) Sequential or cons?  If not, go to (6).
  // (4) Cons string.  If the string is flat, replace subject with first string
  //     and go to (1). Otherwise bail out to runtime.
  // (5) One byte sequential.  Load regexp code for one byte.
  // (E) Carry on.
  /// [...]

  // Deferred code at the end of the stub:
  // (6) Long external string?  If not, go to (10).
  // (7) External string.  Make it, offset-wise, look like a sequential string.
  // (8) Is the external string one byte?  If yes, go to (5).
  // (9) Two byte sequential.  Load regexp code for two byte. Go to (E).
  // (10) Short external string or not a string?  If yes, bail out to runtime.
  // (11) Sliced string.  Replace subject with parent. Go to (1).

  Label seq_one_byte_string /* 5 */, seq_two_byte_string /* 9 */,
      external_string /* 7 */, check_underlying /* 1 */,
      not_seq_nor_cons /* 6 */, check_code /* E */, not_long_external /* 10 */;

  __ bind(&check_underlying);
  // (1) Sequential two byte?  If yes, go to (9).
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));

  __ and_(ebx, kIsNotStringMask |
               kStringRepresentationMask |
               kStringEncodingMask |
               kShortExternalStringMask);
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string);  // Go to (9).

  // (2) Sequential one byte?  If yes, go to (5).
  // Any other sequential string must be one byte.
  __ and_(ebx, Immediate(kIsNotStringMask |
                         kStringRepresentationMask |
                         kShortExternalStringMask));
  __ j(zero, &seq_one_byte_string, Label::kNear);  // Go to (5).

  // (3) Sequential or cons?  If not, go to (6).
  // We check whether the subject string is a cons, since sequential strings
  // have already been covered.
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  __ cmp(ebx, Immediate(kExternalStringTag));
  __ j(greater_equal, &not_seq_nor_cons);  // Go to (6).

  // (4) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ cmp(FieldOperand(eax, ConsString::kSecondOffset), factory->empty_string());
  __ j(not_equal, &runtime);
  __ mov(eax, FieldOperand(eax, ConsString::kFirstOffset));
  __ jmp(&check_underlying);

  // eax: sequential subject string (or look-alike, external string)
  // edx: original subject string
  // ecx: RegExp data (FixedArray)
  // (5) One byte sequential.  Load regexp code for one byte.
  __ bind(&seq_one_byte_string);
  // Load previous index and check range before edx is overwritten.  We have
  // to use edx instead of eax here because it might have been only made to
  // look like a sequential string when it actually is an external string.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ JumpIfNotSmi(ebx, &runtime);
  __ cmp(ebx, FieldOperand(edx, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataOneByteCodeOffset));
  __ Move(ecx, Immediate(1));  // Type is one byte.

  // (E) Carry on.  String handling is done.
  __ bind(&check_code);
  // edx: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(edx, &runtime);

  // eax: subject string
  // ebx: previous index (smi)
  // edx: code
  // ecx: encoding of subject string (1 if one_byte, 0 if two_byte);
  // All checks done. Now push arguments for native regexp code.
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->regexp_entry_native(), 1);

  // Isolates: note we add an additional parameter here (isolate pointer).
  static const int kRegExpExecuteArguments = 9;
  __ EnterApiExitFrame(kRegExpExecuteArguments);

  // Argument 9: Pass current isolate address.
  __ mov(Operand(esp, 8 * kPointerSize),
      Immediate(ExternalReference::isolate_address(isolate())));

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ mov(Operand(esp, 7 * kPointerSize), Immediate(1));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ mov(esi, Operand::StaticVariable(address_of_regexp_stack_memory_address));
  __ add(esi, Operand::StaticVariable(address_of_regexp_stack_memory_size));
  __ mov(Operand(esp, 6 * kPointerSize), esi);

  // Argument 6: Set the number of capture registers to zero to force global
  // regexps to behave as non-global.  This does not affect non-global regexps.
  __ mov(Operand(esp, 5 * kPointerSize), Immediate(0));

  // Argument 5: static offsets vector buffer.
  __ mov(Operand(esp, 4 * kPointerSize),
         Immediate(ExternalReference::address_of_static_offsets_vector(
             isolate())));

  // Argument 2: Previous index.
  __ SmiUntag(ebx);
  __ mov(Operand(esp, 1 * kPointerSize), ebx);

  // Argument 1: Original subject string.
  // The original subject is in the previous stack frame. Therefore we have to
  // use ebp, which points exactly to one pointer size below the previous esp.
  // (Because creating a new stack frame pushes the previous ebp onto the stack
  // and thereby moves up esp by one kPointerSize.)
  __ mov(esi, Operand(ebp, kSubjectOffset + kPointerSize));
  __ mov(Operand(esp, 0 * kPointerSize), esi);

  // esi: original subject string
  // eax: underlying subject string
  // ebx: previous index
  // ecx: encoding of subject string (1 if one_byte 0 if two_byte);
  // edx: code
  // Argument 4: End of string data
  // Argument 3: Start of string data
  // Prepare start and end index of the input.
  // Load the length from the original sliced string if that is the case.
  __ mov(esi, FieldOperand(esi, String::kLengthOffset));
  __ add(esi, edi);  // Calculate input end wrt offset.
  __ SmiUntag(edi);
  __ add(ebx, edi);  // Calculate input start wrt offset.

  // ebx: start index of the input string
  // esi: end index of the input string
  Label setup_two_byte, setup_rest;
  __ test(ecx, ecx);
  __ j(zero, &setup_two_byte, Label::kNear);
  __ SmiUntag(esi);
  __ lea(ecx, FieldOperand(eax, esi, times_1, SeqOneByteString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_1, SeqOneByteString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.
  __ jmp(&setup_rest, Label::kNear);

  __ bind(&setup_two_byte);
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);  // esi is smi (powered by 2).
  __ lea(ecx, FieldOperand(eax, esi, times_1, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Argument 4.
  __ lea(ecx, FieldOperand(eax, ebx, times_2, SeqTwoByteString::kHeaderSize));
  __ mov(Operand(esp, 2 * kPointerSize), ecx);  // Argument 3.

  __ bind(&setup_rest);

  // Locate the code entry and call it.
  __ add(edx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(edx);

  // Drop arguments and come back to JS mode.
  __ LeaveApiExitFrame(true);

  // Check the result.
  Label success;
  __ cmp(eax, 1);
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ j(equal, &success);
  Label failure;
  __ cmp(eax, NativeRegExpMacroAssembler::FAILURE);
  __ j(equal, &failure);
  __ cmp(eax, NativeRegExpMacroAssembler::EXCEPTION);
  // If not exception it can only be retry. Handle that in the runtime system.
  __ j(not_equal, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate());
  __ mov(edx, Immediate(isolate()->factory()->the_hole_value()));
  __ mov(eax, Operand::StaticVariable(pending_exception));
  __ cmp(edx, eax);
  __ j(equal, &runtime);

  // For exception, throw the exception again.
  __ TailCallRuntime(Runtime::kRegExpExecReThrow);

  __ bind(&failure);
  // For failure to match, return null.
  __ mov(eax, factory->null_value());
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ mov(eax, Operand(esp, kJSRegExpOffset));
  __ mov(ecx, FieldOperand(eax, JSRegExp::kDataOffset));
  __ mov(edx, FieldOperand(ecx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(edx, Immediate(2));  // edx was a smi.

  // edx: Number of capture registers
  // Check that the last match info is a FixedArray.
  __ mov(ebx, Operand(esp, kLastMatchInfoOffset));
  __ JumpIfSmi(ebx, &runtime);
  // Check that the object has fast elements.
  __ mov(eax, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(eax, factory->fixed_array_map());
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ mov(eax, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ SmiUntag(eax);
  __ sub(eax, Immediate(RegExpMatchInfo::kLastMatchOverhead));
  __ cmp(edx, eax);
  __ j(greater, &runtime);

  // ebx: last_match_info backing store (FixedArray)
  // edx: number of capture registers
  // Store the capture count.
  __ SmiTag(edx);  // Number of capture registers to smi.
  __ mov(FieldOperand(ebx, RegExpMatchInfo::kNumberOfCapturesOffset), edx);
  __ SmiUntag(edx);  // Number of capture registers back from smi.
  // Store last subject and last input.
  __ mov(eax, Operand(esp, kSubjectOffset));
  __ mov(ecx, eax);
  __ mov(FieldOperand(ebx, RegExpMatchInfo::kLastSubjectOffset), eax);
  __ RecordWriteField(ebx, RegExpMatchInfo::kLastSubjectOffset, eax, edi,
                      kDontSaveFPRegs);
  __ mov(eax, ecx);
  __ mov(FieldOperand(ebx, RegExpMatchInfo::kLastInputOffset), eax);
  __ RecordWriteField(ebx, RegExpMatchInfo::kLastInputOffset, eax, edi,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ mov(ecx, Immediate(address_of_static_offsets_vector));

  // ebx: last_match_info backing store (FixedArray)
  // ecx: offsets vector
  // edx: number of capture registers
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wrapping after zero.
  __ bind(&next_capture);
  __ sub(edx, Immediate(1));
  __ j(negative, &done, Label::kNear);
  // Read the value from the static offsets vector buffer.
  __ mov(edi, Operand(ecx, edx, times_int_size, 0));
  __ SmiTag(edi);
  // Store the smi value in the last match info.
  __ mov(FieldOperand(ebx, edx, times_pointer_size,
                      RegExpMatchInfo::kFirstCaptureOffset),
         edi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ mov(eax, ebx);
  __ ret(4 * kPointerSize);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec);

  // Deferred code for string handling.
  // (6) Long external string?  If not, go to (10).
  __ bind(&not_seq_nor_cons);
  // Compare flags are still set from (3).
  __ j(greater, &not_long_external, Label::kNear);  // Go to (10).

  // (7) External string.  Short external strings have been ruled out.
  __ bind(&external_string);
  // Reload instance type.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ test_b(ebx, Immediate(kIsIndirectStringMask));
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  __ mov(eax, FieldOperand(eax, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ sub(eax, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // (8) Is the external string one byte?  If yes, go to (5).
  __ test_b(ebx, Immediate(kStringEncodingMask));
  __ j(not_zero, &seq_one_byte_string);  // Go to (5).

  // eax: sequential subject string (or look-alike, external string)
  // edx: original subject string
  // ecx: RegExp data (FixedArray)
  // (9) Two byte sequential.  Load regexp code for two byte. Go to (E).
  __ bind(&seq_two_byte_string);
  // Load previous index and check range before edx is overwritten.  We have
  // to use edx instead of eax here because it might have been only made to
  // look like a sequential string when it actually is an external string.
  __ mov(ebx, Operand(esp, kPreviousIndexOffset));
  __ JumpIfNotSmi(ebx, &runtime);
  __ cmp(ebx, FieldOperand(edx, String::kLengthOffset));
  __ j(above_equal, &runtime);
  __ mov(edx, FieldOperand(ecx, JSRegExp::kDataUC16CodeOffset));
  __ Move(ecx, Immediate(0));  // Type is two byte.
  __ jmp(&check_code);  // Go to (E).

  // (10) Not a string or a short external string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  // Catch non-string subject or short external string.
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ test(ebx, Immediate(kIsNotStringMask | kShortExternalStringTag));
  __ j(not_zero, &runtime);

  // (11) Sliced string.  Replace subject with parent.  Go to (1).
  // Load offset into edi and replace subject string with parent.
  __ mov(edi, FieldOperand(eax, SlicedString::kOffsetOffset));
  __ mov(eax, FieldOperand(eax, SlicedString::kParentOffset));
  __ jmp(&check_underlying);  // Go to (1).
#endif  // V8_INTERPRETED_REGEXP
}


static int NegativeComparisonResult(Condition cc) {
  DCHECK(cc != equal);
  DCHECK((cc == less) || (cc == less_equal)
      || (cc == greater) || (cc == greater_equal));
  return (cc == greater || cc == greater_equal) ? LESS : GREATER;
}


static void CheckInputType(MacroAssembler* masm, Register input,
                           CompareICState::State expected, Label* fail) {
  Label ok;
  if (expected == CompareICState::SMI) {
    __ JumpIfNotSmi(input, fail);
  } else if (expected == CompareICState::NUMBER) {
    __ JumpIfSmi(input, &ok);
    __ cmp(FieldOperand(input, HeapObject::kMapOffset),
           Immediate(masm->isolate()->factory()->heap_number_map()));
    __ j(not_equal, fail);
  }
  // We could be strict about internalized/non-internalized here, but as long as
  // hydrogen doesn't care, the stub doesn't have to care either.
  __ bind(&ok);
}


static void BranchIfNotInternalizedString(MacroAssembler* masm,
                                          Label* label,
                                          Register object,
                                          Register scratch) {
  __ JumpIfSmi(object, label);
  __ mov(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzx_b(scratch, FieldOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ test(scratch, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, label);
}


void CompareICStub::GenerateGeneric(MacroAssembler* masm) {
  Label runtime_call, check_unequal_objects;
  Condition cc = GetCondition();

  Label miss;
  CheckInputType(masm, edx, left(), &miss);
  CheckInputType(masm, eax, right(), &miss);

  // Compare two smis.
  Label non_smi, smi_done;
  __ mov(ecx, edx);
  __ or_(ecx, eax);
  __ JumpIfNotSmi(ecx, &non_smi, Label::kNear);
  __ sub(edx, eax);  // Return on the result of the subtraction.
  __ j(no_overflow, &smi_done, Label::kNear);
  __ not_(edx);  // Correct sign in case of overflow. edx is never 0 here.
  __ bind(&smi_done);
  __ mov(eax, edx);
  __ ret(0);
  __ bind(&non_smi);

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Identical objects can be compared fast, but there are some tricky cases
  // for NaN and undefined.
  Label generic_heap_number_comparison;
  {
    Label not_identical;
    __ cmp(eax, edx);
    __ j(not_equal, &not_identical);

    if (cc != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      __ cmp(edx, isolate()->factory()->undefined_value());
      Label check_for_nan;
      __ j(not_equal, &check_for_nan, Label::kNear);
      __ Move(eax, Immediate(Smi::FromInt(NegativeComparisonResult(cc))));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Compare heap numbers in a general way,
    // to handle NaNs correctly.
    __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
           Immediate(isolate()->factory()->heap_number_map()));
    __ j(equal, &generic_heap_number_comparison, Label::kNear);
    if (cc != equal) {
      __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
      __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
      // Call runtime on identical JSObjects.  Otherwise return equal.
      __ cmpb(ecx, Immediate(FIRST_JS_RECEIVER_TYPE));
      __ j(above_equal, &runtime_call, Label::kFar);
      // Call runtime on identical symbols since we need to throw a TypeError.
      __ cmpb(ecx, Immediate(SYMBOL_TYPE));
      __ j(equal, &runtime_call, Label::kFar);
      // Call runtime on identical SIMD values since we must throw a TypeError.
      __ cmpb(ecx, Immediate(SIMD128_VALUE_TYPE));
      __ j(equal, &runtime_call, Label::kFar);
    }
    __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
    __ ret(0);


    __ bind(&not_identical);
  }

  // Strict equality can quickly decide whether objects are equal.
  // Non-strict object equality is slower, so it is handled later in the stub.
  if (cc == equal && strict()) {
    Label slow;  // Fallthrough label.
    Label not_smis;
    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    // If either is a Smi (we know that not both are), then they can only
    // be equal if the other is a HeapNumber. If so, use the slow case.
    STATIC_ASSERT(kSmiTag == 0);
    DCHECK_EQ(static_cast<Smi*>(0), Smi::kZero);
    __ mov(ecx, Immediate(kSmiTagMask));
    __ and_(ecx, eax);
    __ test(ecx, edx);
    __ j(not_zero, &not_smis, Label::kNear);
    // One operand is a smi.

    // Check whether the non-smi is a heap number.
    STATIC_ASSERT(kSmiTagMask == 1);
    // ecx still holds eax & kSmiTag, which is either zero or one.
    __ sub(ecx, Immediate(0x01));
    __ mov(ebx, edx);
    __ xor_(ebx, eax);
    __ and_(ebx, ecx);  // ebx holds either 0 or eax ^ edx.
    __ xor_(ebx, eax);
    // if eax was smi, ebx is now edx, else eax.

    // Check if the non-smi operand is a heap number.
    __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
           Immediate(isolate()->factory()->heap_number_map()));
    // If heap number, handle it in the slow case.
    __ j(equal, &slow, Label::kNear);
    // Return non-equal (ebx is not zero)
    __ mov(eax, ebx);
    __ ret(0);

    __ bind(&not_smis);
    // If either operand is a JSObject or an oddball value, then they are not
    // equal since their pointers are different
    // There is no test for undetectability in strict equality.

    // Get the type of the first operand.
    // If the first object is a JS object, we have done pointer comparison.
    Label first_non_object;
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
    __ j(below, &first_non_object, Label::kNear);

    // Return non-zero (eax is not zero)
    Label return_not_equal;
    STATIC_ASSERT(kHeapObjectTag != 0);
    __ bind(&return_not_equal);
    __ ret(0);

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    __ CmpObjectType(edx, FIRST_JS_RECEIVER_TYPE, ecx);
    __ j(above_equal, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(equal, &return_not_equal);

    // Fall through to the general case.
    __ bind(&slow);
  }

  // Generate the number comparison code.
  Label non_number_comparison;
  Label unordered;
  __ bind(&generic_heap_number_comparison);
  FloatingPointHelper::CheckFloatOperands(
      masm, &non_number_comparison, ebx);
  FloatingPointHelper::LoadFloatOperand(masm, eax);
  FloatingPointHelper::LoadFloatOperand(masm, edx);
  __ FCmp();

  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);

  Label below_label, above_label;
  // Return a result of -1, 0, or 1, based on EFLAGS.
  __ j(below, &below_label, Label::kNear);
  __ j(above, &above_label, Label::kNear);

  __ Move(eax, Immediate(0));
  __ ret(0);

  __ bind(&below_label);
  __ mov(eax, Immediate(Smi::FromInt(-1)));
  __ ret(0);

  __ bind(&above_label);
  __ mov(eax, Immediate(Smi::FromInt(1)));
  __ ret(0);

  // If one of the numbers was NaN, then the result is always false.
  // The cc is never not-equal.
  __ bind(&unordered);
  DCHECK(cc != not_equal);
  if (cc == less || cc == less_equal) {
    __ mov(eax, Immediate(Smi::FromInt(1)));
  } else {
    __ mov(eax, Immediate(Smi::FromInt(-1)));
  }
  __ ret(0);

  // The number comparison code did not provide a valid result.
  __ bind(&non_number_comparison);

  // Fast negative check for internalized-to-internalized equality.
  Label check_for_strings;
  if (cc == equal) {
    BranchIfNotInternalizedString(masm, &check_for_strings, eax, ecx);
    BranchIfNotInternalizedString(masm, &check_for_strings, edx, ecx);

    // We've already checked for object identity, so if both operands
    // are internalized they aren't equal. Register eax already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialOneByteStrings(edx, eax, ecx, ebx,
                                           &check_unequal_objects);

  // Inline comparison of one-byte strings.
  if (cc == equal) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, edx, eax, ecx, ebx);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, edx, eax, ecx, ebx,
                                                    edi);
  }
#ifdef DEBUG
  __ Abort(kUnexpectedFallThroughFromStringComparison);
#endif

  __ bind(&check_unequal_objects);
  if (cc == equal && !strict()) {
    // Non-strict equality.  Objects are unequal if
    // they are both JSObjects and not undetectable,
    // and their pointers are different.
    Label return_equal, return_unequal, undetectable;
    // At most one is a smi, so we can test for smi by adding the two.
    // A smi plus a heap object has the low bit set, a heap object plus
    // a heap object has the low bit clear.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagMask == 1);
    __ lea(ecx, Operand(eax, edx, times_1, 0));
    __ test(ecx, Immediate(kSmiTagMask));
    __ j(not_zero, &runtime_call);

    __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
    __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));

    __ test_b(FieldOperand(ebx, Map::kBitFieldOffset),
              Immediate(1 << Map::kIsUndetectable));
    __ j(not_zero, &undetectable, Label::kNear);
    __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
              Immediate(1 << Map::kIsUndetectable));
    __ j(not_zero, &return_unequal, Label::kNear);

    __ CmpInstanceType(ebx, FIRST_JS_RECEIVER_TYPE);
    __ j(below, &runtime_call, Label::kNear);
    __ CmpInstanceType(ecx, FIRST_JS_RECEIVER_TYPE);
    __ j(below, &runtime_call, Label::kNear);

    __ bind(&return_unequal);
    // Return non-equal by returning the non-zero object pointer in eax.
    __ ret(0);  // eax, edx were pushed

    __ bind(&undetectable);
    __ test_b(FieldOperand(ecx, Map::kBitFieldOffset),
              Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal, Label::kNear);

    // If both sides are JSReceivers, then the result is false according to
    // the HTML specification, which says that only comparisons with null or
    // undefined are affected by special casing for document.all.
    __ CmpInstanceType(ebx, ODDBALL_TYPE);
    __ j(zero, &return_equal, Label::kNear);
    __ CmpInstanceType(ecx, ODDBALL_TYPE);
    __ j(not_zero, &return_unequal, Label::kNear);

    __ bind(&return_equal);
    __ Move(eax, Immediate(EQUAL));
    __ ret(0);  // eax, edx were pushed
  }
  __ bind(&runtime_call);

  if (cc == equal) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(esi);
      __ Call(strict() ? isolate()->builtins()->StrictEqual()
                       : isolate()->builtins()->Equal(),
              RelocInfo::CODE_TARGET);
      __ Pop(esi);
    }
    // Turn true into 0 and false into some non-zero value.
    STATIC_ASSERT(EQUAL == 0);
    __ sub(eax, Immediate(isolate()->factory()->true_value()));
    __ Ret();
  } else {
    // Push arguments below the return address.
    __ pop(ecx);
    __ push(edx);
    __ push(eax);
    __ push(Immediate(Smi::FromInt(NegativeComparisonResult(cc))));

    // Restore return address on the stack.
    __ push(ecx);
    // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
    // tagged as a small integer.
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


static void CallStubInRecordCallTarget(MacroAssembler* masm, CodeStub* stub) {
  // eax : number of arguments to the construct function
  // ebx : feedback vector
  // edx : slot in feedback vector (Smi)
  // edi : the function to call

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Number-of-arguments register must be smi-tagged to call out.
    __ SmiTag(eax);
    __ push(eax);
    __ push(edi);
    __ push(edx);
    __ push(ebx);
    __ push(esi);

    __ CallStub(stub);

    __ pop(esi);
    __ pop(ebx);
    __ pop(edx);
    __ pop(edi);
    __ pop(eax);
    __ SmiUntag(eax);
  }
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // eax : number of arguments to the construct function
  // ebx : feedback vector
  // edx : slot in feedback vector (Smi)
  // edi : the function to call
  Isolate* isolate = masm->isolate();
  Label initialize, done, miss, megamorphic, not_array_function;

  // Load the cache state into ecx.
  __ mov(ecx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  // We don't know if ecx is a WeakCell or a Symbol, but it's harmless to read
  // at this position in a symbol (see static asserts in feedback-vector.h).
  Label check_allocation_site;
  __ cmp(edi, FieldOperand(ecx, WeakCell::kValueOffset));
  __ j(equal, &done, Label::kFar);
  __ CompareRoot(ecx, Heap::kmegamorphic_symbolRootIndex);
  __ j(equal, &done, Label::kFar);
  __ CompareRoot(FieldOperand(ecx, HeapObject::kMapOffset),
                 Heap::kWeakCellMapRootIndex);
  __ j(not_equal, &check_allocation_site);

  // If the weak cell is cleared, we have a new chance to become monomorphic.
  __ JumpIfSmi(FieldOperand(ecx, WeakCell::kValueOffset), &initialize);
  __ jmp(&megamorphic);

  __ bind(&check_allocation_site);
  // If we came here, we need to see if we are the array function.
  // If we didn't have a matching function, and we didn't find the megamorph
  // sentinel, then we have in the slot either some other function or an
  // AllocationSite.
  __ CompareRoot(FieldOperand(ecx, 0), Heap::kAllocationSiteMapRootIndex);
  __ j(not_equal, &miss);

  // Make sure the function is the Array() function
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(not_equal, &megamorphic);
  __ jmp(&done, Label::kFar);

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(ecx, Heap::kuninitialized_symbolRootIndex);
  __ j(equal, &initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ mov(
      FieldOperand(ebx, edx, times_half_pointer_size, FixedArray::kHeaderSize),
      Immediate(FeedbackVector::MegamorphicSentinel(isolate)));
  __ jmp(&done, Label::kFar);

  // An uninitialized cache is patched with the function or sentinel to
  // indicate the ElementsKind if function is the Array constructor.
  __ bind(&initialize);
  // Make sure the function is the Array() function
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(not_equal, &not_array_function);

  // The target function is the Array constructor,
  // Create an AllocationSite if we don't already have it, store it in the
  // slot.
  CreateAllocationSiteStub create_stub(isolate);
  CallStubInRecordCallTarget(masm, &create_stub);
  __ jmp(&done);

  __ bind(&not_array_function);
  CreateWeakCellStub weak_cell_stub(isolate);
  CallStubInRecordCallTarget(masm, &weak_cell_stub);

  __ bind(&done);
  // Increment the call count for all function calls.
  __ add(FieldOperand(ebx, edx, times_half_pointer_size,
                      FixedArray::kHeaderSize + kPointerSize),
         Immediate(Smi::FromInt(1)));
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // eax : number of arguments
  // ebx : feedback vector
  // edx : slot in feedback vector (Smi, for RecordCallTarget)
  // edi : constructor function

  Label non_function;
  // Check that function is not a smi.
  __ JumpIfSmi(edi, &non_function);
  // Check that function is a JSFunction.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &non_function);

  GenerateRecordCallTarget(masm);

  Label feedback_register_initialized;
  // Put the AllocationSite from the feedback vector into ebx, or undefined.
  __ mov(ebx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));
  Handle<Map> allocation_site_map = isolate()->factory()->allocation_site_map();
  __ cmp(FieldOperand(ebx, 0), Immediate(allocation_site_map));
  __ j(equal, &feedback_register_initialized);
  __ mov(ebx, isolate()->factory()->undefined_value());
  __ bind(&feedback_register_initialized);

  __ AssertUndefinedOrAllocationSite(ebx);

  // Pass new target to construct stub.
  __ mov(edx, edi);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ecx, FieldOperand(ecx, SharedFunctionInfo::kConstructStubOffset));
  __ lea(ecx, FieldOperand(ecx, Code::kHeaderSize));
  __ jmp(ecx);

  __ bind(&non_function);
  __ mov(edx, edi);
  __ Jump(isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

static void IncrementCallCount(MacroAssembler* masm, Register feedback_vector,
                               Register slot) {
  __ add(FieldOperand(feedback_vector, slot, times_half_pointer_size,
                      FixedArray::kHeaderSize + kPointerSize),
         Immediate(Smi::FromInt(1)));
}

void CallICStub::HandleArrayCase(MacroAssembler* masm, Label* miss) {
  // eax - number of arguments
  // edi - function
  // edx - slot id
  // ebx - vector
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(not_equal, miss);

  // Reload ecx.
  __ mov(ecx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, ebx, edx);

  __ mov(ebx, ecx);
  __ mov(edx, edi);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);

  // Unreachable.
}


void CallICStub::Generate(MacroAssembler* masm) {
  // edi - number of arguments
  // edi - function
  // edx - slot id
  // ebx - vector
  Isolate* isolate = masm->isolate();
  Label extra_checks_or_miss, call, call_function, call_count_incremented;

  // The checks. First, does edi match the recorded monomorphic target?
  __ mov(ecx, FieldOperand(ebx, edx, times_half_pointer_size,
                           FixedArray::kHeaderSize));

  // We don't know that we have a weak cell. We might have a private symbol
  // or an AllocationSite, but the memory is safe to examine.
  // AllocationSite::kTransitionInfoOffset - contains a Smi or pointer to
  // FixedArray.
  // WeakCell::kValueOffset - contains a JSFunction or Smi(0)
  // Symbol::kHashFieldSlot - if the low bit is 1, then the hash is not
  // computed, meaning that it can't appear to be a pointer. If the low bit is
  // 0, then hash is computed, but the 0 bit prevents the field from appearing
  // to be a pointer.
  STATIC_ASSERT(WeakCell::kSize >= kPointerSize);
  STATIC_ASSERT(AllocationSite::kTransitionInfoOffset ==
                    WeakCell::kValueOffset &&
                WeakCell::kValueOffset == Symbol::kHashFieldSlot);

  __ cmp(edi, FieldOperand(ecx, WeakCell::kValueOffset));
  __ j(not_equal, &extra_checks_or_miss);

  // The compare above could have been a SMI/SMI comparison. Guard against this
  // convincing us that we have a monomorphic JSFunction.
  __ JumpIfSmi(edi, &extra_checks_or_miss);

  __ bind(&call_function);

  // Increment the call count for monomorphic function calls.
  IncrementCallCount(masm, ebx, edx);

  __ Jump(masm->isolate()->builtins()->CallFunction(convert_mode(),
                                                    tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&extra_checks_or_miss);
  Label uninitialized, miss, not_allocation_site;

  __ cmp(ecx, Immediate(FeedbackVector::MegamorphicSentinel(isolate)));
  __ j(equal, &call);

  // Check if we have an allocation site.
  __ CompareRoot(FieldOperand(ecx, HeapObject::kMapOffset),
                 Heap::kAllocationSiteMapRootIndex);
  __ j(not_equal, &not_allocation_site);

  // We have an allocation site.
  HandleArrayCase(masm, &miss);

  __ bind(&not_allocation_site);

  // The following cases attempt to handle MISS cases without going to the
  // runtime.
  if (FLAG_trace_ic) {
    __ jmp(&miss);
  }

  __ cmp(ecx, Immediate(FeedbackVector::UninitializedSentinel(isolate)));
  __ j(equal, &uninitialized);

  // We are going megamorphic. If the feedback is a JSFunction, it is fine
  // to handle it here. More complex cases are dealt with in the runtime.
  __ AssertNotSmi(ecx);
  __ CmpObjectType(ecx, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &miss);
  __ mov(
      FieldOperand(ebx, edx, times_half_pointer_size, FixedArray::kHeaderSize),
      Immediate(FeedbackVector::MegamorphicSentinel(isolate)));

  __ bind(&call);

  // Increment the call count for megamorphic function calls.
  IncrementCallCount(masm, ebx, edx);

  __ bind(&call_count_incremented);

  __ Jump(masm->isolate()->builtins()->Call(convert_mode(), tail_call_mode()),
          RelocInfo::CODE_TARGET);

  __ bind(&uninitialized);

  // We are going monomorphic, provided we actually have a JSFunction.
  __ JumpIfSmi(edi, &miss);

  // Goto miss case if we do not have a function.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, ecx);
  __ j(not_equal, &miss);

  // Make sure the function is not the Array() function, which requires special
  // behavior on MISS.
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, ecx);
  __ cmp(edi, ecx);
  __ j(equal, &miss);

  // Make sure the function belongs to the same native context.
  __ mov(ecx, FieldOperand(edi, JSFunction::kContextOffset));
  __ mov(ecx, ContextOperand(ecx, Context::NATIVE_CONTEXT_INDEX));
  __ cmp(ecx, NativeContextOperand());
  __ j(not_equal, &miss);

  // Store the function. Use a stub since we need a frame for allocation.
  // eax - number of arguments
  // ebx - vector
  // edx - slot
  // edi - function
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    CreateWeakCellStub create_stub(isolate);
    __ SmiTag(eax);
    __ push(eax);
    __ push(ebx);
    __ push(edx);
    __ push(edi);
    __ push(esi);
    __ CallStub(&create_stub);
    __ pop(esi);
    __ pop(edi);
    __ pop(edx);
    __ pop(ebx);
    __ pop(eax);
    __ SmiUntag(eax);
  }

  __ jmp(&call_function);

  // We are here because tracing is on or we encountered a MISS case we can't
  // handle here.
  __ bind(&miss);
  GenerateMiss(masm);

  __ jmp(&call_count_incremented);

  // Unreachable
  __ int3();
}


void CallICStub::GenerateMiss(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);

  // Preserve the number of arguments.
  __ SmiTag(eax);
  __ push(eax);

  // Push the function and feedback info.
  __ push(edi);
  __ push(ebx);
  __ push(edx);

  // Call the entry.
  __ CallRuntime(Runtime::kCallIC_Miss);

  // Move result to edi and exit the internal frame.
  __ mov(edi, eax);

  // Restore number of arguments.
  __ pop(eax);
  __ SmiUntag(eax);
}


bool CEntryStub::NeedsImmovableCode() {
  return false;
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  // It is important that the store buffer overflow stubs are generated first.
  CommonArrayConstructorStub::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  CreateWeakCellStub::GenerateAheadOfTime(isolate);
  BinaryOpICStub::GenerateAheadOfTime(isolate);
  BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
  StoreFastElementStub::GenerateAheadOfTime(isolate);
}


void CodeStub::GenerateFPStubs(Isolate* isolate) {
  CEntryStub save_doubles(isolate, 1, kSaveFPRegs);
  // Stubs might already be in the snapshot, detect that and don't regenerate,
  // which would lead to code stub initialization state being messed up.
  Code* save_doubles_code;
  if (!save_doubles.FindCodeInCache(&save_doubles_code)) {
    save_doubles_code = *(save_doubles.GetCode());
  }
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // eax: number of arguments including receiver
  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // esi: current context (C callee-saved)
  // edi: JS function of the caller (C callee-saved)
  //
  // If argv_in_register():
  // ecx: pointer to the first argument

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Reserve space on the stack for the three arguments passed to the call. If
  // result size is greater than can be returned in registers, also reserve
  // space for the hidden argument for the result location, and space for the
  // result itself.
  int arg_stack_space = result_size() < 3 ? 3 : 4 + result_size();

  // Enter the exit frame that transitions from JavaScript to C++.
  if (argv_in_register()) {
    DCHECK(!save_doubles());
    DCHECK(!is_builtin_exit());
    __ EnterApiExitFrame(arg_stack_space);

    // Move argc and argv into the correct registers.
    __ mov(esi, ecx);
    __ mov(edi, eax);
  } else {
    __ EnterExitFrame(
        arg_stack_space, save_doubles(),
        is_builtin_exit() ? StackFrame::BUILTIN_EXIT : StackFrame::EXIT);
  }

  // ebx: pointer to C function  (C callee-saved)
  // ebp: frame pointer  (restored after C call)
  // esp: stack pointer  (restored after C call)
  // edi: number of arguments including receiver  (C callee-saved)
  // esi: pointer to the first argument (C callee-saved)

  // Result returned in eax, or eax+edx if result size is 2.

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }
  // Call C function.
  if (result_size() <= 2) {
    __ mov(Operand(esp, 0 * kPointerSize), edi);  // argc.
    __ mov(Operand(esp, 1 * kPointerSize), esi);  // argv.
    __ mov(Operand(esp, 2 * kPointerSize),
           Immediate(ExternalReference::isolate_address(isolate())));
  } else {
    DCHECK_EQ(3, result_size());
    // Pass a pointer to the result location as the first argument.
    __ lea(eax, Operand(esp, 4 * kPointerSize));
    __ mov(Operand(esp, 0 * kPointerSize), eax);
    __ mov(Operand(esp, 1 * kPointerSize), edi);  // argc.
    __ mov(Operand(esp, 2 * kPointerSize), esi);  // argv.
    __ mov(Operand(esp, 3 * kPointerSize),
           Immediate(ExternalReference::isolate_address(isolate())));
  }
  __ call(ebx);

  if (result_size() > 2) {
    DCHECK_EQ(3, result_size());
#ifndef _WIN32
    // Restore the "hidden" argument on the stack which was popped by caller.
    __ sub(esp, Immediate(kPointerSize));
#endif
    // Read result values stored on stack. Result is stored above the arguments.
    __ mov(kReturnRegister0, Operand(esp, 4 * kPointerSize));
    __ mov(kReturnRegister1, Operand(esp, 5 * kPointerSize));
    __ mov(kReturnRegister2, Operand(esp, 6 * kPointerSize));
  }
  // Result is in eax, edx:eax or edi:edx:eax - do not destroy these registers!

  // Check result for exception sentinel.
  Label exception_returned;
  __ cmp(eax, isolate()->factory()->exception());
  __ j(equal, &exception_returned);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    __ push(edx);
    __ mov(edx, Immediate(isolate()->factory()->the_hole_value()));
    Label okay;
    ExternalReference pending_exception_address(
        Isolate::kPendingExceptionAddress, isolate());
    __ cmp(edx, Operand::StaticVariable(pending_exception_address));
    // Cannot use check here as it attempts to generate call into runtime.
    __ j(equal, &okay, Label::kNear);
    __ int3();
    __ bind(&okay);
    __ pop(edx);
  }

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles(), !argv_in_register());
  __ ret(0);

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address(
      Isolate::kPendingHandlerContextAddress, isolate());
  ExternalReference pending_handler_code_address(
      Isolate::kPendingHandlerCodeAddress, isolate());
  ExternalReference pending_handler_offset_address(
      Isolate::kPendingHandlerOffsetAddress, isolate());
  ExternalReference pending_handler_fp_address(
      Isolate::kPendingHandlerFPAddress, isolate());
  ExternalReference pending_handler_sp_address(
      Isolate::kPendingHandlerSPAddress, isolate());

  // Ask the runtime for help to determine the handler. This will set eax to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler(Runtime::kUnwindAndFindExceptionHandler,
                                 isolate());
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, eax);
    __ mov(Operand(esp, 0 * kPointerSize), Immediate(0));  // argc.
    __ mov(Operand(esp, 1 * kPointerSize), Immediate(0));  // argv.
    __ mov(Operand(esp, 2 * kPointerSize),
           Immediate(ExternalReference::isolate_address(isolate())));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ mov(esi, Operand::StaticVariable(pending_handler_context_address));
  __ mov(esp, Operand::StaticVariable(pending_handler_sp_address));
  __ mov(ebp, Operand::StaticVariable(pending_handler_fp_address));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (esi == 0) for non-JS frames.
  Label skip;
  __ test(esi, esi);
  __ j(zero, &skip, Label::kNear);
  __ mov(Operand(ebp, StandardFrameConstants::kContextOffset), esi);
  __ bind(&skip);

  // Compute the handler entry address and jump to it.
  __ mov(edi, Operand::StaticVariable(pending_handler_code_address));
  __ mov(edx, Operand::StaticVariable(pending_handler_offset_address));
  // Check whether it's a turbofanned exception handler code before jump to it.
  Label not_turbo;
  __ push(eax);
  __ mov(eax, Operand(edi, Code::kKindSpecificFlags1Offset - kHeapObjectTag));
  __ and_(eax, Immediate(1 << Code::kIsTurbofannedBit));
  __ j(zero, &not_turbo);
  __ fninit();
  __ fld1();
  __ bind(&not_turbo);
  __ pop(eax);
  __ lea(edi, FieldOperand(edi, edx, times_1, Code::kHeaderSize));
  __ jmp(edi);
}


void JSEntryStub::Generate(MacroAssembler* masm) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Set up frame.
  __ push(ebp);
  __ mov(ebp, esp);

  // Push marker in two places.
  int marker = type();
  __ push(Immediate(Smi::FromInt(marker)));  // marker
  ExternalReference context_address(Isolate::kContextAddress, isolate());
  __ push(Operand::StaticVariable(context_address));  // context
  // Save callee-saved registers (C calling conventions).
  __ push(edi);
  __ push(esi);
  __ push(ebx);

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, isolate());
  __ push(Operand::StaticVariable(c_entry_fp));

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ cmp(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ j(not_equal, &not_outermost_js, Label::kNear);
  __ mov(Operand::StaticVariable(js_entry_sp), ebp);
  __ push(Immediate(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ jmp(&invoke, Label::kNear);
  __ bind(&not_outermost_js);
  __ push(Immediate(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME)));

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate());
  __ mov(Operand::StaticVariable(pending_exception), eax);
  __ mov(eax, Immediate(isolate()->factory()->exception()));
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushStackHandler();

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. Notice that we cannot store a
  // reference to the trampoline code directly in this stub, because the
  // builtin stubs may not have been generated yet.
  if (type() == StackFrame::ENTRY_CONSTRUCT) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate());
    __ mov(edx, Immediate(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate());
    __ mov(edx, Immediate(entry));
  }
  __ mov(edx, Operand(edx, 0));  // deref address
  __ lea(edx, FieldOperand(edx, Code::kHeaderSize));
  __ call(edx);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ pop(ebx);
  __ cmp(ebx, Immediate(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ j(not_equal, &not_outermost_js_2);
  __ mov(Operand::StaticVariable(js_entry_sp), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  __ pop(Operand::StaticVariable(ExternalReference(
      Isolate::kCEntryFPAddress, isolate())));

  // Restore callee-saved registers (C calling conventions).
  __ pop(ebx);
  __ pop(esi);
  __ pop(edi);
  __ add(esp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(ebp);
  __ ret(0);
}


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  // If the receiver is a smi trigger the non-string case.
  if (check_mode_ == RECEIVER_IS_UNKNOWN) {
    __ JumpIfSmi(object_, receiver_not_string_);

    // Fetch the instance type of the receiver into result register.
    __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
    __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
    // If the receiver is not a string trigger the non-string case.
    __ test(result_, Immediate(kIsNotStringMask));
    __ j(not_zero, receiver_not_string_);
  }

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ cmp(index_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  __ SmiUntag(index_);

  Factory* factory = masm->isolate()->factory();
  StringCharLoadGenerator::Generate(
      masm, factory, object_, index_, result_, &call_runtime_);

  __ SmiTag(result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm, EmbedMode embed_mode,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharCodeAtSlowCase);

  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              masm->isolate()->factory()->heap_number_map(),
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ push(LoadWithVectorDescriptor::VectorRegister());
    __ push(LoadDescriptor::SlotRegister());
  }
  __ push(object_);
  __ push(index_);  // Consumed by runtime conversion function.
  __ CallRuntime(Runtime::kNumberToSmi);
  if (!index_.is(eax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ mov(index_, eax);
  }
  __ pop(object_);
  if (embed_mode == PART_OF_IC_HANDLER) {
    __ pop(LoadDescriptor::SlotRegister());
    __ pop(LoadWithVectorDescriptor::VectorRegister());
  }
  // Reload the instance type.
  __ mov(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzx_b(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ SmiTag(index_);
  __ push(index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT);
  if (!result_.is(eax)) {
    __ mov(result_, eax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiShiftSize == 0);
  DCHECK(base::bits::IsPowerOfTwo32(String::kMaxOneByteCharCodeU + 1));
  __ test(code_, Immediate(kSmiTagMask |
                           ((~String::kMaxOneByteCharCodeU) << kSmiTagSize)));
  __ j(not_zero, &slow_case_);

  Factory* factory = masm->isolate()->factory();
  __ Move(result_, Immediate(factory->single_character_string_cache()));
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiShiftSize == 0);
  // At this point code register contains smi tagged one byte char code.
  __ mov(result_, FieldOperand(result_,
                               code_, times_half_pointer_size,
                               FixedArray::kHeaderSize));
  __ cmp(result_, factory->undefined_value());
  __ j(equal, &slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharFromCodeSlowCase);

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kStringCharFromCode);
  if (!result_.is(eax)) {
    __ mov(result_, eax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}

void StringHelper::GenerateFlatOneByteStringEquals(MacroAssembler* masm,
                                                   Register left,
                                                   Register right,
                                                   Register scratch1,
                                                   Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ mov(length, FieldOperand(left, String::kLengthOffset));
  __ cmp(length, FieldOperand(right, String::kLengthOffset));
  __ j(equal, &check_zero_length, Label::kNear);
  __ bind(&strings_not_equal);
  __ Move(eax, Immediate(Smi::FromInt(NOT_EQUAL)));
  __ ret(0);

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ test(length, length);
  __ j(not_zero, &compare_chars, Label::kNear);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  // Compare characters.
  __ bind(&compare_chars);
  GenerateOneByteCharsCompareLoop(masm, left, right, length, scratch2,
                                  &strings_not_equal, Label::kNear);

  // Characters are equal.
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);
}


void StringHelper::GenerateCompareFlatOneByteStrings(
    MacroAssembler* masm, Register left, Register right, Register scratch1,
    Register scratch2, Register scratch3) {
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_compare_native(), 1);

  // Find minimum length.
  Label left_shorter;
  __ mov(scratch1, FieldOperand(left, String::kLengthOffset));
  __ mov(scratch3, scratch1);
  __ sub(scratch3, FieldOperand(right, String::kLengthOffset));

  Register length_delta = scratch3;

  __ j(less_equal, &left_shorter, Label::kNear);
  // Right string is shorter. Change scratch1 to be length of right string.
  __ sub(scratch1, length_delta);
  __ bind(&left_shorter);

  Register min_length = scratch1;

  // If either length is zero, just compare lengths.
  Label compare_lengths;
  __ test(min_length, min_length);
  __ j(zero, &compare_lengths, Label::kNear);

  // Compare characters.
  Label result_not_equal;
  GenerateOneByteCharsCompareLoop(masm, left, right, min_length, scratch2,
                                  &result_not_equal, Label::kNear);

  // Compare lengths -  strings up to min-length are equal.
  __ bind(&compare_lengths);
  __ test(length_delta, length_delta);
  Label length_not_equal;
  __ j(not_zero, &length_not_equal, Label::kNear);

  // Result is EQUAL.
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  Label result_greater;
  Label result_less;
  __ bind(&length_not_equal);
  __ j(greater, &result_greater, Label::kNear);
  __ jmp(&result_less, Label::kNear);
  __ bind(&result_not_equal);
  __ j(above, &result_greater, Label::kNear);
  __ bind(&result_less);

  // Result is LESS.
  __ Move(eax, Immediate(Smi::FromInt(LESS)));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Move(eax, Immediate(Smi::FromInt(GREATER)));
  __ ret(0);
}


void StringHelper::GenerateOneByteCharsCompareLoop(
    MacroAssembler* masm, Register left, Register right, Register length,
    Register scratch, Label* chars_not_equal,
    Label::Distance chars_not_equal_near) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ lea(left,
         FieldOperand(left, length, times_1, SeqOneByteString::kHeaderSize));
  __ lea(right,
         FieldOperand(right, length, times_1, SeqOneByteString::kHeaderSize));
  __ neg(length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ mov_b(scratch, Operand(left, index, times_1, 0));
  __ cmpb(scratch, Operand(right, index, times_1, 0));
  __ j(not_equal, chars_not_equal, chars_not_equal_near);
  __ inc(index);
  __ j(not_zero, &loop);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edx    : left
  //  -- eax    : right
  //  -- esp[0] : return address
  // -----------------------------------

  // Load ecx with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ mov(ecx, isolate()->factory()->undefined_value());

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_equal, kExpectedAllocationSite);
    __ cmp(FieldOperand(ecx, HeapObject::kMapOffset),
           isolate()->factory()->allocation_site_map());
    __ Assert(equal, kExpectedAllocationSite);
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(isolate(), state());
  __ TailCallStub(&stub);
}


void CompareICStub::GenerateBooleans(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::BOOLEAN, state());
  Label miss;
  Label::Distance const miss_distance =
      masm->emit_debug_code() ? Label::kFar : Label::kNear;

  __ JumpIfSmi(edx, &miss, miss_distance);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ JumpIfSmi(eax, &miss, miss_distance);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ JumpIfNotRoot(ecx, Heap::kBooleanMapRootIndex, &miss, miss_distance);
  __ JumpIfNotRoot(ebx, Heap::kBooleanMapRootIndex, &miss, miss_distance);
  if (!Token::IsEqualityOp(op())) {
    __ mov(eax, FieldOperand(eax, Oddball::kToNumberOffset));
    __ AssertSmi(eax);
    __ mov(edx, FieldOperand(edx, Oddball::kToNumberOffset));
    __ AssertSmi(edx);
    __ xchg(eax, edx);
  }
  __ sub(eax, edx);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::SMI);
  Label miss;
  __ mov(ecx, edx);
  __ or_(ecx, eax);
  __ JumpIfNotSmi(ecx, &miss, Label::kNear);

  if (GetCondition() == equal) {
    // For equality we do not care about the sign of the result.
    __ sub(eax, edx);
  } else {
    Label done;
    __ sub(edx, eax);
    __ j(no_overflow, &done, Label::kNear);
    // Correct sign of result in case of overflow.
    __ not_(edx);
    __ bind(&done);
    __ mov(eax, edx);
  }
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateNumbers(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::NUMBER);

  Label generic_stub, check_left;
  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss;

  if (left() == CompareICState::SMI) {
    __ JumpIfNotSmi(edx, &miss);
  }
  if (right() == CompareICState::SMI) {
    __ JumpIfNotSmi(eax, &miss);
  }

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved or SSE2 or CMOV is unsupported.
  __ JumpIfSmi(eax, &check_left, Label::kNear);
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
         isolate()->factory()->heap_number_map());
  __ j(not_equal, &maybe_undefined1, Label::kNear);

  __ bind(&check_left);
  __ JumpIfSmi(edx, &generic_stub, Label::kNear);
  __ cmp(FieldOperand(edx, HeapObject::kMapOffset),
         isolate()->factory()->heap_number_map());
  __ j(not_equal, &maybe_undefined2, Label::kNear);

  __ bind(&unordered);
  __ bind(&generic_stub);
  CompareICStub stub(isolate(), op(), CompareICState::GENERIC,
                     CompareICState::GENERIC, CompareICState::GENERIC);
  __ jmp(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ cmp(eax, Immediate(isolate()->factory()->undefined_value()));
    __ j(not_equal, &miss);
    __ JumpIfSmi(edx, &unordered);
    __ CmpObjectType(edx, HEAP_NUMBER_TYPE, ecx);
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op())) {
    __ cmp(edx, Immediate(isolate()->factory()->undefined_value()));
    __ j(equal, &unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::INTERNALIZED_STRING);
  DCHECK(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;

  // Check that both operands are heap objects.
  Label miss;
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss, Label::kNear);

  // Check that both operands are internalized strings.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ or_(tmp1, tmp2);
  __ test(tmp1, Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  __ j(not_zero, &miss, Label::kNear);

  // Internalized strings are compared by identity.
  Label done;
  __ cmp(left, right);
  // Make sure eax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(eax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::UNIQUE_NAME);
  DCHECK(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;

  // Check that both operands are heap objects.
  Label miss;
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss, Label::kNear);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueNameInstanceType(tmp1, &miss, Label::kNear);
  __ JumpIfNotUniqueNameInstanceType(tmp2, &miss, Label::kNear);

  // Unique names are compared by identity.
  Label done;
  __ cmp(left, right);
  // Make sure eax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(eax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state() == CompareICState::STRING);
  Label miss;

  bool equality = Token::IsEqualityOp(op());

  // Registers containing left and right operands respectively.
  Register left = edx;
  Register right = eax;
  Register tmp1 = ecx;
  Register tmp2 = ebx;
  Register tmp3 = edi;

  // Check that both operands are heap objects.
  __ mov(tmp1, left);
  STATIC_ASSERT(kSmiTag == 0);
  __ and_(tmp1, right);
  __ JumpIfSmi(tmp1, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ mov(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ mov(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzx_b(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzx_b(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  __ mov(tmp3, tmp1);
  STATIC_ASSERT(kNotStringTag != 0);
  __ or_(tmp3, tmp2);
  __ test(tmp3, Immediate(kIsNotStringMask));
  __ j(not_zero, &miss);

  // Fast check for identical strings.
  Label not_same;
  __ cmp(left, right);
  __ j(not_equal, &not_same, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(eax, Immediate(Smi::FromInt(EQUAL)));
  __ ret(0);

  // Handle not identical strings.
  __ bind(&not_same);

  // Check that both strings are internalized. If they are, we're done
  // because we already know they are not identical.  But in the case of
  // non-equality compare, we still need to determine the order. We
  // also know they are both strings.
  if (equality) {
    Label do_compare;
    STATIC_ASSERT(kInternalizedTag == 0);
    __ or_(tmp1, tmp2);
    __ test(tmp1, Immediate(kIsNotInternalizedMask));
    __ j(not_zero, &do_compare, Label::kNear);
    // Make sure eax is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(eax));
    __ ret(0);
    __ bind(&do_compare);
  }

  // Check that both strings are sequential one-byte.
  Label runtime;
  __ JumpIfNotBothSequentialOneByteStrings(left, right, tmp1, tmp2, &runtime);

  // Compare flat one byte strings. Returns when done.
  if (equality) {
    StringHelper::GenerateFlatOneByteStringEquals(masm, left, right, tmp1,
                                                  tmp2);
  } else {
    StringHelper::GenerateCompareFlatOneByteStrings(masm, left, right, tmp1,
                                                    tmp2, tmp3);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  if (equality) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(left);
      __ Push(right);
      __ CallRuntime(Runtime::kStringEqual);
    }
    __ sub(eax, Immediate(masm->isolate()->factory()->true_value()));
    __ Ret();
  } else {
    __ pop(tmp1);  // Return address.
    __ push(left);
    __ push(right);
    __ push(tmp1);
    __ TailCallRuntime(Runtime::kStringCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateReceivers(MacroAssembler* masm) {
  DCHECK_EQ(CompareICState::RECEIVER, state());
  Label miss;
  __ mov(ecx, edx);
  __ and_(ecx, eax);
  __ JumpIfSmi(ecx, &miss, Label::kNear);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  __ CmpObjectType(eax, FIRST_JS_RECEIVER_TYPE, ecx);
  __ j(below, &miss, Label::kNear);
  __ CmpObjectType(edx, FIRST_JS_RECEIVER_TYPE, ecx);
  __ j(below, &miss, Label::kNear);

  DCHECK_EQ(equal, GetCondition());
  __ sub(eax, edx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateKnownReceivers(MacroAssembler* masm) {
  Label miss;
  Handle<WeakCell> cell = Map::WeakCellForMap(known_map_);
  __ mov(ecx, edx);
  __ and_(ecx, eax);
  __ JumpIfSmi(ecx, &miss, Label::kNear);

  __ GetWeakValue(edi, cell);
  __ cmp(edi, FieldOperand(eax, HeapObject::kMapOffset));
  __ j(not_equal, &miss, Label::kNear);
  __ cmp(edi, FieldOperand(edx, HeapObject::kMapOffset));
  __ j(not_equal, &miss, Label::kNear);

  if (Token::IsEqualityOp(op())) {
    __ sub(eax, edx);
    __ ret(0);
  } else {
    __ PopReturnAddressTo(ecx);
    __ Push(edx);
    __ Push(eax);
    __ Push(Immediate(Smi::FromInt(NegativeComparisonResult(GetCondition()))));
    __ PushReturnAddressFrom(ecx);
    __ TailCallRuntime(Runtime::kCompare);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void CompareICStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(edx);  // Preserve edx and eax.
    __ push(eax);
    __ push(edx);  // And also use them as the arguments.
    __ push(eax);
    __ push(Immediate(Smi::FromInt(op())));
    __ CallRuntime(Runtime::kCompareIC_Miss);
    // Compute the entry point of the rewritten stub.
    __ lea(edi, FieldOperand(eax, Code::kHeaderSize));
    __ pop(eax);
    __ pop(edx);
  }

  // Do a tail call to the rewritten stub.
  __ jmp(edi);
}


// Helper function used to check that the dictionary doesn't contain
// the property. This function may return false negatives, so miss_label
// must always call a backup property check that is complete.
// This function is safe to call if the receiver has fast properties.
// Name must be a unique name and receiver must be a heap object.
void NameDictionaryLookupStub::GenerateNegativeLookup(MacroAssembler* masm,
                                                      Label* miss,
                                                      Label* done,
                                                      Register properties,
                                                      Handle<Name> name,
                                                      Register r0) {
  DCHECK(name->IsUniqueName());

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the hole value).
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = r0;
    // Capacity is smi 2^n.
    __ mov(index, FieldOperand(properties, kCapacityOffset));
    __ dec(index);
    __ and_(index,
            Immediate(Smi::FromInt(name->Hash() +
                                   NameDictionary::GetProbeOffset(i))));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index, Operand(index, index, times_2, 0));  // index *= 3.
    Register entity_name = r0;
    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ mov(entity_name, Operand(properties, index, times_half_pointer_size,
                                kElementsStartOffset - kHeapObjectTag));
    __ cmp(entity_name, masm->isolate()->factory()->undefined_value());
    __ j(equal, done);

    // Stop if found the property.
    __ cmp(entity_name, Handle<Name>(name));
    __ j(equal, miss);

    Label good;
    // Check for the hole and skip.
    __ cmp(entity_name, masm->isolate()->factory()->the_hole_value());
    __ j(equal, &good, Label::kNear);

    // Check if the entry name is not a unique name.
    __ mov(entity_name, FieldOperand(entity_name, HeapObject::kMapOffset));
    __ JumpIfNotUniqueNameInstanceType(
        FieldOperand(entity_name, Map::kInstanceTypeOffset), miss);
    __ bind(&good);
  }

  NameDictionaryLookupStub stub(masm->isolate(), properties, r0, r0,
                                NEGATIVE_LOOKUP);
  __ push(Immediate(Handle<Object>(name)));
  __ push(Immediate(name->Hash()));
  __ CallStub(&stub);
  __ test(r0, r0);
  __ j(not_zero, miss);
  __ jmp(done);
}

void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Stack frame on entry:
  //  esp[0 * kPointerSize]: return address.
  //  esp[1 * kPointerSize]: key's hash.
  //  esp[2 * kPointerSize]: key.
  // Registers:
  //  dictionary_: NameDictionary to probe.
  //  result_: used as scratch.
  //  index_: will hold an index of entry if lookup is successful.
  //          might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  Register scratch = result();

  __ mov(scratch, FieldOperand(dictionary(), kCapacityOffset));
  __ dec(scratch);
  __ SmiUntag(scratch);
  __ push(scratch);

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ mov(scratch, Operand(esp, 2 * kPointerSize));
    if (i > 0) {
      __ add(scratch, Immediate(NameDictionary::GetProbeOffset(i)));
    }
    __ and_(scratch, Operand(esp, 0));

    // Scale the index by multiplying by the entry size.
    STATIC_ASSERT(NameDictionary::kEntrySize == 3);
    __ lea(index(), Operand(scratch, scratch, times_2, 0));  // index *= 3.

    // Having undefined at this place means the name is not contained.
    STATIC_ASSERT(kSmiTagSize == 1);
    __ mov(scratch, Operand(dictionary(), index(), times_pointer_size,
                            kElementsStartOffset - kHeapObjectTag));
    __ cmp(scratch, isolate()->factory()->undefined_value());
    __ j(equal, &not_in_dictionary);

    // Stop if found the property.
    __ cmp(scratch, Operand(esp, 3 * kPointerSize));
    __ j(equal, &in_dictionary);

    if (i != kTotalProbes - 1 && mode() == NEGATIVE_LOOKUP) {
      // If we hit a key that is not a unique name during negative
      // lookup we have to bailout as this key might be equal to the
      // key we are looking for.

      // Check if the entry name is not a unique name.
      __ mov(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
      __ JumpIfNotUniqueNameInstanceType(
          FieldOperand(scratch, Map::kInstanceTypeOffset),
          &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode() == POSITIVE_LOOKUP) {
    __ mov(result(), Immediate(0));
    __ Drop(1);
    __ ret(2 * kPointerSize);
  }

  __ bind(&in_dictionary);
  __ mov(result(), Immediate(1));
  __ Drop(1);
  __ ret(2 * kPointerSize);

  __ bind(&not_in_dictionary);
  __ mov(result(), Immediate(0));
  __ Drop(1);
  __ ret(2 * kPointerSize);
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub(isolate, kDontSaveFPRegs);
  stub.GetCode();
  StoreBufferOverflowStub stub2(isolate, kSaveFPRegs);
  stub2.GetCode();
}


// Takes the input in 3 registers: address_ value_ and object_.  A pointer to
// the value has just been written into the object, now this stub makes sure
// we keep the GC informed.  The word in the object where the value has been
// written is in the address register.
void RecordWriteStub::Generate(MacroAssembler* masm) {
  Label skip_to_incremental_noncompacting;
  Label skip_to_incremental_compacting;

  // The first two instructions are generated with labels so as to get the
  // offset fixed up correctly by the bind(Label*) call.  We patch it back and
  // forth between a compare instructions (a nop in this position) and the
  // real branch when we start and stop incremental heap marking.
  __ jmp(&skip_to_incremental_noncompacting, Label::kNear);
  __ jmp(&skip_to_incremental_compacting, Label::kFar);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&skip_to_incremental_noncompacting);
  GenerateIncremental(masm, INCREMENTAL);

  __ bind(&skip_to_incremental_compacting);
  GenerateIncremental(masm, INCREMENTAL_COMPACTION);

  // Initial mode of the stub is expected to be STORE_BUFFER_ONLY.
  // Will be checked in IncrementalMarking::ActivateGeneratedStub.
  masm->set_byte_at(0, kTwoByteNopInstruction);
  masm->set_byte_at(2, kFiveByteNopInstruction);
}


void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  regs_.Save(masm);

  if (remembered_set_action() == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ mov(regs_.scratch0(), Operand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),  // Value.
                           regs_.scratch0(),
                           &dont_need_remembered_set);

    __ JumpIfInNewSpace(regs_.object(), regs_.scratch0(),
                        &dont_need_remembered_set);

    // First notify the incremental marker if necessary, then update the
    // remembered set.
    CheckNeedsToInformIncrementalMarker(
        masm,
        kUpdateRememberedSetOnNoNeedToInformIncrementalMarker,
        mode);
    InformIncrementalMarker(masm);
    regs_.Restore(masm);
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);

    __ bind(&dont_need_remembered_set);
  }

  CheckNeedsToInformIncrementalMarker(
      masm,
      kReturnOnNoNeedToInformIncrementalMarker,
      mode);
  InformIncrementalMarker(masm);
  regs_.Restore(masm);
  __ ret(0);
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode());
  int argument_count = 3;
  __ PrepareCallCFunction(argument_count, regs_.scratch0());
  __ mov(Operand(esp, 0 * kPointerSize), regs_.object());
  __ mov(Operand(esp, 1 * kPointerSize), regs_.address());  // Slot.
  __ mov(Operand(esp, 2 * kPointerSize),
         Immediate(ExternalReference::isolate_address(isolate())));

  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(
      ExternalReference::incremental_marking_record_write_function(isolate()),
      argument_count);

  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode());
}


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label object_is_black, need_incremental, need_incremental_pop_object;

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(),
                 regs_.scratch0(),
                 regs_.scratch1(),
                 &object_is_black,
                 Label::kNear);

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&object_is_black);

  // Get the value from the slot.
  __ mov(regs_.scratch0(), Operand(regs_.address(), 0));

  if (mode == INCREMENTAL_COMPACTION) {
    Label ensure_not_white;

    __ CheckPageFlag(regs_.scratch0(),  // Contains value.
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kEvacuationCandidateMask,
                     zero,
                     &ensure_not_white,
                     Label::kNear);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kSkipEvacuationSlotsRecordingMask,
                     not_zero,
                     &ensure_not_white,
                     Label::kNear);

    __ jmp(&need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need an extra register for this, so we push the object register
  // temporarily.
  __ push(regs_.object());
  __ JumpIfWhite(regs_.scratch0(),  // The value.
                 regs_.scratch1(),  // Scratch.
                 regs_.object(),    // Scratch.
                 &need_incremental_pop_object, Label::kNear);
  __ pop(regs_.object());

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object(), address(), value(), save_fp_regs_mode(),
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&need_incremental_pop_object);
  __ pop(regs_.object());

  __ bind(&need_incremental);

  // Fall through when we need to inform the incremental marker.
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(isolate(), 1, kSaveFPRegs);
  __ call(ces.GetCode(), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrameConstants::kArgumentsLengthOffset;
  __ mov(ebx, MemOperand(ebp, parameter_count_offset));
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ pop(ecx);
  int additional_offset =
      function_mode() == JS_FUNCTION_STUB_MODE ? kPointerSize : 0;
  __ lea(esp, MemOperand(esp, ebx, times_pointer_size, additional_offset));
  __ jmp(ecx);  // Return to IC Miss stub, continuation still on stack.
}

void CallICTrampolineStub::Generate(MacroAssembler* masm) {
  __ EmitLoadFeedbackVector(ebx);
  CallICStub stub(isolate(), state());
  __ jmp(stub.GetCode(), RelocInfo::CODE_TARGET);
}

void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    ProfileEntryHookStub stub(masm->isolate());
    masm->CallStub(&stub);
  }
}

void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // Save volatile registers.
  const int kNumSavedRegisters = 3;
  __ push(eax);
  __ push(ecx);
  __ push(edx);

  // Calculate and push the original stack pointer.
  __ lea(eax, Operand(esp, (kNumSavedRegisters + 1) * kPointerSize));
  __ push(eax);

  // Retrieve our return address and use it to calculate the calling
  // function's address.
  __ mov(eax, Operand(esp, (kNumSavedRegisters + 1) * kPointerSize));
  __ sub(eax, Immediate(Assembler::kCallInstructionLength));
  __ push(eax);

  // Call the entry hook.
  DCHECK(isolate()->function_entry_hook() != NULL);
  __ call(FUNCTION_ADDR(isolate()->function_entry_hook()),
          RelocInfo::RUNTIME_ENTRY);
  __ add(esp, Immediate(2 * kPointerSize));

  // Restore ecx.
  __ pop(edx);
  __ pop(ecx);
  __ pop(eax);

  __ ret(0);
}

template <class T>
static void CreateArrayDispatch(MacroAssembler* masm,
                                AllocationSiteOverrideMode mode) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    T stub(masm->isolate(), GetInitialFastElementsKind(), mode);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmp(edx, kind);
      __ j(not_equal, &next);
      T stub(masm->isolate(), kind);
      __ TailCallStub(&stub);
      __ bind(&next);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}

static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  // ebx - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // edx - kind (if mode != DISABLE_ALLOCATION_SITES)
  // eax - number of arguments
  // edi - constructor?
  // esp[0] - return address
  // esp[4] - last argument
  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    STATIC_ASSERT(FAST_DOUBLE_ELEMENTS == 4);
    STATIC_ASSERT(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ test_b(edx, Immediate(1));
    __ j(not_zero, &normal_sequence);
  }

  // look at the first argument
  __ mov(ecx, Operand(esp, kPointerSize));
  __ test(ecx, ecx);
  __ j(zero, &normal_sequence);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(
        masm->isolate(), holey_initial, DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);

    __ bind(&normal_sequence);
    ArraySingleArgumentConstructorStub stub(masm->isolate(), initial,
                                            DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry.
    __ inc(edx);

    if (FLAG_debug_code) {
      Handle<Map> allocation_site_map =
          masm->isolate()->factory()->allocation_site_map();
      __ cmp(FieldOperand(ebx, 0), Immediate(allocation_site_map));
      __ Assert(equal, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r3
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ add(FieldOperand(ebx, AllocationSite::kTransitionInfoOffset),
           Immediate(Smi::FromInt(kFastElementsKindPackedToHoley)));

    __ bind(&normal_sequence);
    int last_index =
        GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      Label next;
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ cmp(edx, kind);
      __ j(not_equal, &next);
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub);
      __ bind(&next);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}

template <class T>
static void ArrayConstructorStubAheadOfTimeHelper(Isolate* isolate) {
  int to_index =
      GetSequenceIndexFromFastElementsKind(TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= to_index; ++i) {
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    T stub(isolate, kind);
    stub.GetCode();
    if (AllocationSite::GetMode(kind) != DONT_TRACK_ALLOCATION_SITE) {
      T stub1(isolate, kind, DISABLE_ALLOCATION_SITES);
      stub1.GetCode();
    }
  }
}

void CommonArrayConstructorStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  ArrayConstructorStubAheadOfTimeHelper<ArrayNoArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArraySingleArgumentConstructorStub>(
      isolate);
  ArrayNArgumentsConstructorStub stub(isolate);
  stub.GetCode();

  ElementsKind kinds[2] = {FAST_ELEMENTS, FAST_HOLEY_ELEMENTS};
  for (int i = 0; i < 2; i++) {
    // For internal arrays we only need a few things
    InternalArrayNoArgumentConstructorStub stubh1(isolate, kinds[i]);
    stubh1.GetCode();
    InternalArraySingleArgumentConstructorStub stubh2(isolate, kinds[i]);
    stubh2.GetCode();
  }
}

void ArrayConstructorStub::GenerateDispatchToArrayStub(
    MacroAssembler* masm, AllocationSiteOverrideMode mode) {
  Label not_zero_case, not_one_case;
  __ test(eax, eax);
  __ j(not_zero, &not_zero_case);
  CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

  __ bind(&not_zero_case);
  __ cmp(eax, 1);
  __ j(greater, &not_one_case);
  CreateArrayDispatchOneArgument(masm, mode);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc (only if argument_count() is ANY or MORE_THAN_ONE)
  //  -- ebx : AllocationSite or undefined
  //  -- edi : constructor
  //  -- edx : Original constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------
  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in ebx or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(ebx);
  }

  Label subclassing;

  // Enter the context of the Array function.
  __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));

  __ cmp(edx, edi);
  __ j(not_equal, &subclassing);

  Label no_info;
  // If the feedback vector is the undefined value call an array constructor
  // that doesn't use AllocationSites.
  __ cmp(ebx, isolate()->factory()->undefined_value());
  __ j(equal, &no_info);

  // Only look at the lower 16 bits of the transition info.
  __ mov(edx, FieldOperand(ebx, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(edx);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ and_(edx, Immediate(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);

  // Subclassing.
  __ bind(&subclassing);
  __ mov(Operand(esp, eax, times_pointer_size, kPointerSize), edi);
  __ add(eax, Immediate(3));
  __ PopReturnAddressTo(ecx);
  __ Push(edx);
  __ Push(ebx);
  __ PushReturnAddressFrom(ecx);
  __ JumpToExternalReference(ExternalReference(Runtime::kNewArray, isolate()));
}

void InternalArrayConstructorStub::GenerateCase(MacroAssembler* masm,
                                                ElementsKind kind) {
  Label not_zero_case, not_one_case;
  Label normal_sequence;

  __ test(eax, eax);
  __ j(not_zero, &not_zero_case);
  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0);

  __ bind(&not_zero_case);
  __ cmp(eax, 1);
  __ j(greater, &not_one_case);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ mov(ecx, Operand(esp, kPointerSize));
    __ test(ecx, ecx);
    __ j(zero, &normal_sequence);

    InternalArraySingleArgumentConstructorStub stub1_holey(
        isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey);
  }

  __ bind(&normal_sequence);
  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);

  __ bind(&not_one_case);
  ArrayNArgumentsConstructorStub stubN(isolate());
  __ TailCallStub(&stubN);
}

void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax : argc
  //  -- edi : constructor
  //  -- esp[0] : return address
  //  -- esp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ test(ecx, Immediate(kSmiTagMask));
    __ Assert(not_zero, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(ecx, MAP_TYPE, ecx);
    __ Assert(equal, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ mov(ecx, FieldOperand(edi, JSFunction::kPrototypeOrInitialMapOffset));

  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following masking takes care of that anyway.
  __ mov(ecx, FieldOperand(ecx, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(ecx);

  if (FLAG_debug_code) {
    Label done;
    __ cmp(ecx, Immediate(FAST_ELEMENTS));
    __ j(equal, &done);
    __ cmp(ecx, Immediate(FAST_HOLEY_ELEMENTS));
    __ Assert(equal, kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ cmp(ecx, Immediate(FAST_ELEMENTS));
  __ j(equal, &fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}

void FastNewRestParameterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi    : function
  //  -- esi    : context
  //  -- ebp    : frame pointer
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertFunction(edi);

  // Make edx point to the JavaScript frame.
  __ mov(edx, ebp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ mov(edx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ cmp(edi, Operand(edx, StandardFrameConstants::kFunctionOffset));
    __ j(equal, &ok);
    __ Abort(kInvalidFrameForFastNewRestArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have rest parameters (only possible if we have an
  // arguments adaptor frame below the function frame).
  Label no_rest_parameters;
  __ mov(ebx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(not_equal, &no_rest_parameters, Label::kNear);

  // Check if the arguments adaptor frame contains more arguments than
  // specified by the function's internal formal parameter count.
  Label rest_parameters;
  __ mov(ecx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(eax, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ sub(eax,
         FieldOperand(ecx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ j(greater, &rest_parameters);

  // Return an empty rest parameter array.
  __ bind(&no_rest_parameters);
  {
    // ----------- S t a t e -------------
    //  -- esi    : context
    //  -- esp[0] : return address
    // -----------------------------------

    // Allocate an empty rest parameter array.
    Label allocate, done_allocate;
    __ Allocate(JSArray::kSize, eax, edx, ecx, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the rest parameter array in rax.
    __ LoadGlobalFunction(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, ecx);
    __ mov(FieldOperand(eax, JSArray::kMapOffset), ecx);
    __ mov(ecx, isolate()->factory()->empty_fixed_array());
    __ mov(FieldOperand(eax, JSArray::kPropertiesOffset), ecx);
    __ mov(FieldOperand(eax, JSArray::kElementsOffset), ecx);
    __ mov(FieldOperand(eax, JSArray::kLengthOffset), Immediate(Smi::kZero));
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ Ret();

    // Fall back to %AllocateInNewSpace.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(Smi::FromInt(JSArray::kSize));
      __ CallRuntime(Runtime::kAllocateInNewSpace);
    }
    __ jmp(&done_allocate);
  }

  __ bind(&rest_parameters);
  {
    // Compute the pointer to the first rest parameter (skippping the receiver).
    __ lea(ebx,
           Operand(ebx, eax, times_half_pointer_size,
                   StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));

    // ----------- S t a t e -------------
    //  -- esi    : context
    //  -- eax    : number of rest parameters (tagged)
    //  -- ebx    : pointer to first rest parameters
    //  -- esp[0] : return address
    // -----------------------------------

    // Allocate space for the rest parameter array plus the backing store.
    Label allocate, done_allocate;
    __ lea(ecx, Operand(eax, times_half_pointer_size,
                        JSArray::kSize + FixedArray::kHeaderSize));
    __ Allocate(ecx, edx, edi, no_reg, &allocate, NO_ALLOCATION_FLAGS);
    __ bind(&done_allocate);

    // Setup the elements array in edx.
    __ mov(FieldOperand(edx, FixedArray::kMapOffset),
           isolate()->factory()->fixed_array_map());
    __ mov(FieldOperand(edx, FixedArray::kLengthOffset), eax);
    {
      Label loop, done_loop;
      __ Move(ecx, Smi::kZero);
      __ bind(&loop);
      __ cmp(ecx, eax);
      __ j(equal, &done_loop, Label::kNear);
      __ mov(edi, Operand(ebx, 0 * kPointerSize));
      __ mov(FieldOperand(edx, ecx, times_half_pointer_size,
                          FixedArray::kHeaderSize),
             edi);
      __ sub(ebx, Immediate(1 * kPointerSize));
      __ add(ecx, Immediate(Smi::FromInt(1)));
      __ jmp(&loop);
      __ bind(&done_loop);
    }

    // Setup the rest parameter array in edi.
    __ lea(edi,
           Operand(edx, eax, times_half_pointer_size, FixedArray::kHeaderSize));
    __ LoadGlobalFunction(Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX, ecx);
    __ mov(FieldOperand(edi, JSArray::kMapOffset), ecx);
    __ mov(FieldOperand(edi, JSArray::kPropertiesOffset),
           isolate()->factory()->empty_fixed_array());
    __ mov(FieldOperand(edi, JSArray::kElementsOffset), edx);
    __ mov(FieldOperand(edi, JSArray::kLengthOffset), eax);
    STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
    __ mov(eax, edi);
    __ Ret();

    // Fall back to %AllocateInNewSpace (if not too big).
    Label too_big_for_new_space;
    __ bind(&allocate);
    __ cmp(ecx, Immediate(kMaxRegularHeapObjectSize));
    __ j(greater, &too_big_for_new_space);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(ecx);
      __ Push(eax);
      __ Push(ebx);
      __ Push(ecx);
      __ CallRuntime(Runtime::kAllocateInNewSpace);
      __ mov(edx, eax);
      __ Pop(ebx);
      __ Pop(eax);
    }
    __ jmp(&done_allocate);

    // Fall back to %NewRestParameter.
    __ bind(&too_big_for_new_space);
    __ PopReturnAddressTo(ecx);
    // We reload the function from the caller frame due to register pressure
    // within this stub. This is the slow path, hence reloading is preferable.
    if (skip_stub_frame()) {
      // For Ignition we need to skip the handler/stub frame to reach the
      // JavaScript frame for the function.
      __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
      __ Push(Operand(edx, StandardFrameConstants::kFunctionOffset));
    } else {
      __ Push(Operand(ebp, StandardFrameConstants::kFunctionOffset));
    }
    __ PushReturnAddressFrom(ecx);
    __ TailCallRuntime(Runtime::kNewRestParameter);
  }
}

void FastNewSloppyArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi    : function
  //  -- esi    : context
  //  -- ebp    : frame pointer
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertFunction(edi);

  // Make ecx point to the JavaScript frame.
  __ mov(ecx, ebp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ mov(ecx, Operand(ecx, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ cmp(edi, Operand(ecx, StandardFrameConstants::kFunctionOffset));
    __ j(equal, &ok);
    __ Abort(kInvalidFrameForFastNewSloppyArgumentsStub);
    __ bind(&ok);
  }

  // TODO(bmeurer): Cleanup to match the FastNewStrictArgumentsStub.
  __ mov(ebx, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
  __ mov(ebx,
         FieldOperand(ebx, SharedFunctionInfo::kFormalParameterCountOffset));
  __ lea(edx, Operand(ecx, ebx, times_half_pointer_size,
                      StandardFrameConstants::kCallerSPOffset));

  // ebx : number of parameters (tagged)
  // edx : parameters pointer
  // edi : function
  // ecx : JavaScript frame pointer.
  // esp[0] : return address

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ mov(eax, Operand(ecx, StandardFrameConstants::kCallerFPOffset));
  __ mov(eax, Operand(eax, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ cmp(eax, Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adaptor_frame, Label::kNear);

  // No adaptor, parameter count = argument count.
  __ mov(ecx, ebx);
  __ push(ebx);
  __ jmp(&try_allocate, Label::kNear);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ push(ebx);
  __ mov(edx, Operand(ecx, StandardFrameConstants::kCallerFPOffset));
  __ mov(ecx, Operand(edx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ lea(edx,
         Operand(edx, ecx, times_2, StandardFrameConstants::kCallerSPOffset));

  // ebx = parameter count (tagged)
  // ecx = argument count (smi-tagged)
  // Compute the mapped parameter count = min(ebx, ecx) in ebx.
  __ cmp(ebx, ecx);
  __ j(less_equal, &try_allocate, Label::kNear);
  __ mov(ebx, ecx);

  // Save mapped parameter count and function.
  __ bind(&try_allocate);
  __ push(edi);
  __ push(ebx);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  Label no_parameter_map;
  __ test(ebx, ebx);
  __ j(zero, &no_parameter_map, Label::kNear);
  __ lea(ebx, Operand(ebx, times_2, kParameterMapHeaderSize));
  __ bind(&no_parameter_map);

  // 2. Backing store.
  __ lea(ebx, Operand(ebx, ecx, times_2, FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ add(ebx, Immediate(JSSloppyArgumentsObject::kSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(ebx, eax, edi, no_reg, &runtime, NO_ALLOCATION_FLAGS);

  // eax = address of new object(s) (tagged)
  // ecx = argument count (smi-tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[4] = function
  // esp[8] = parameter count (tagged)
  // Get the arguments map from the current native context into edi.
  Label has_mapped_parameters, instantiate;
  __ mov(edi, NativeContextOperand());
  __ mov(ebx, Operand(esp, 0 * kPointerSize));
  __ test(ebx, ebx);
  __ j(not_zero, &has_mapped_parameters, Label::kNear);
  __ mov(
      edi,
      Operand(edi, Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX)));
  __ jmp(&instantiate, Label::kNear);

  __ bind(&has_mapped_parameters);
  __ mov(edi, Operand(edi, Context::SlotOffset(
                               Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX)));
  __ bind(&instantiate);

  // eax = address of new object (tagged)
  // ebx = mapped parameter count (tagged)
  // ecx = argument count (smi-tagged)
  // edi = address of arguments map (tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[4] = function
  // esp[8] = parameter count (tagged)
  // Copy the JS object part.
  __ mov(FieldOperand(eax, JSObject::kMapOffset), edi);
  __ mov(FieldOperand(eax, JSObject::kPropertiesOffset),
         masm->isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(eax, JSObject::kElementsOffset),
         masm->isolate()->factory()->empty_fixed_array());

  // Set up the callee in-object property.
  STATIC_ASSERT(JSSloppyArgumentsObject::kCalleeIndex == 1);
  __ mov(edi, Operand(esp, 1 * kPointerSize));
  __ AssertNotSmi(edi);
  __ mov(FieldOperand(eax, JSSloppyArgumentsObject::kCalleeOffset), edi);

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(ecx);
  __ mov(FieldOperand(eax, JSSloppyArgumentsObject::kLengthOffset), ecx);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, edi will point there, otherwise to the
  // backing store.
  __ lea(edi, Operand(eax, JSSloppyArgumentsObject::kSize));
  __ mov(FieldOperand(eax, JSObject::kElementsOffset), edi);

  // eax = address of new object (tagged)
  // ebx = mapped parameter count (tagged)
  // ecx = argument count (tagged)
  // edx = address of receiver argument
  // edi = address of parameter map or backing store (tagged)
  // esp[0] = mapped parameter count (tagged)
  // esp[4] = function
  // esp[8] = parameter count (tagged)
  // Free two registers.
  __ push(edx);
  __ push(eax);

  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ test(ebx, ebx);
  __ j(zero, &skip_parameter_map);

  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(isolate()->factory()->sloppy_arguments_elements_map()));
  __ lea(eax, Operand(ebx, reinterpret_cast<intptr_t>(Smi::FromInt(2))));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), eax);
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize + 0 * kPointerSize), esi);
  __ lea(eax, Operand(edi, ebx, times_2, kParameterMapHeaderSize));
  __ mov(FieldOperand(edi, FixedArray::kHeaderSize + 1 * kPointerSize), eax);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ push(ecx);
  __ mov(eax, Operand(esp, 3 * kPointerSize));
  __ mov(ebx, Immediate(Smi::FromInt(Context::MIN_CONTEXT_SLOTS)));
  __ add(ebx, Operand(esp, 5 * kPointerSize));
  __ sub(ebx, eax);
  __ mov(ecx, isolate()->factory()->the_hole_value());
  __ mov(edx, edi);
  __ lea(edi, Operand(edi, eax, times_2, kParameterMapHeaderSize));
  // eax = loop variable (tagged)
  // ebx = mapping index (tagged)
  // ecx = the hole value
  // edx = address of parameter map (tagged)
  // edi = address of backing store (tagged)
  // esp[0] = argument count (tagged)
  // esp[4] = address of new object (tagged)
  // esp[8] = address of receiver argument
  // esp[12] = mapped parameter count (tagged)
  // esp[16] = function
  // esp[20] = parameter count (tagged)
  __ jmp(&parameters_test, Label::kNear);

  __ bind(&parameters_loop);
  __ sub(eax, Immediate(Smi::FromInt(1)));
  __ mov(FieldOperand(edx, eax, times_2, kParameterMapHeaderSize), ebx);
  __ mov(FieldOperand(edi, eax, times_2, FixedArray::kHeaderSize), ecx);
  __ add(ebx, Immediate(Smi::FromInt(1)));
  __ bind(&parameters_test);
  __ test(eax, eax);
  __ j(not_zero, &parameters_loop, Label::kNear);
  __ pop(ecx);

  __ bind(&skip_parameter_map);

  // ecx = argument count (tagged)
  // edi = address of backing store (tagged)
  // esp[0] = address of new object (tagged)
  // esp[4] = address of receiver argument
  // esp[8] = mapped parameter count (tagged)
  // esp[12] = function
  // esp[16] = parameter count (tagged)
  // Copy arguments header and remaining slots (if there are any).
  __ mov(FieldOperand(edi, FixedArray::kMapOffset),
         Immediate(isolate()->factory()->fixed_array_map()));
  __ mov(FieldOperand(edi, FixedArray::kLengthOffset), ecx);

  Label arguments_loop, arguments_test;
  __ mov(ebx, Operand(esp, 2 * kPointerSize));
  __ mov(edx, Operand(esp, 1 * kPointerSize));
  __ sub(edx, ebx);  // Is there a smarter way to do negative scaling?
  __ sub(edx, ebx);
  __ jmp(&arguments_test, Label::kNear);

  __ bind(&arguments_loop);
  __ sub(edx, Immediate(kPointerSize));
  __ mov(eax, Operand(edx, 0));
  __ mov(FieldOperand(edi, ebx, times_2, FixedArray::kHeaderSize), eax);
  __ add(ebx, Immediate(Smi::FromInt(1)));

  __ bind(&arguments_test);
  __ cmp(ebx, ecx);
  __ j(less, &arguments_loop, Label::kNear);

  // Restore.
  __ pop(eax);  // Address of arguments object.
  __ Drop(4);

  // Return.
  __ ret(0);

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ pop(eax);   // Remove saved mapped parameter count.
  __ pop(edi);   // Pop saved function.
  __ pop(eax);   // Remove saved parameter count.
  __ pop(eax);   // Pop return address.
  __ push(edi);  // Push function.
  __ push(edx);  // Push parameters pointer.
  __ push(ecx);  // Push parameter count.
  __ push(eax);  // Push return address.
  __ TailCallRuntime(Runtime::kNewSloppyArguments);
}

void FastNewStrictArgumentsStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi    : function
  //  -- esi    : context
  //  -- ebp    : frame pointer
  //  -- esp[0] : return address
  // -----------------------------------
  __ AssertFunction(edi);

  // Make edx point to the JavaScript frame.
  __ mov(edx, ebp);
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ mov(edx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
  }
  if (FLAG_debug_code) {
    Label ok;
    __ cmp(edi, Operand(edx, StandardFrameConstants::kFunctionOffset));
    __ j(equal, &ok);
    __ Abort(kInvalidFrameForFastNewStrictArgumentsStub);
    __ bind(&ok);
  }

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ mov(ebx, Operand(edx, StandardFrameConstants::kCallerFPOffset));
  __ cmp(Operand(ebx, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &arguments_adaptor, Label::kNear);
  {
    __ mov(eax, FieldOperand(edi, JSFunction::kSharedFunctionInfoOffset));
    __ mov(eax,
           FieldOperand(eax, SharedFunctionInfo::kFormalParameterCountOffset));
    __ lea(ebx,
           Operand(edx, eax, times_half_pointer_size,
                   StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));
  }
  __ jmp(&arguments_done, Label::kNear);
  __ bind(&arguments_adaptor);
  {
    __ mov(eax, Operand(ebx, ArgumentsAdaptorFrameConstants::kLengthOffset));
    __ lea(ebx,
           Operand(ebx, eax, times_half_pointer_size,
                   StandardFrameConstants::kCallerSPOffset - 1 * kPointerSize));
  }
  __ bind(&arguments_done);

  // ----------- S t a t e -------------
  //  -- eax    : number of arguments (tagged)
  //  -- ebx    : pointer to the first argument
  //  -- esi    : context
  //  -- esp[0] : return address
  // -----------------------------------

  // Allocate space for the strict arguments object plus the backing store.
  Label allocate, done_allocate;
  __ lea(ecx,
         Operand(eax, times_half_pointer_size,
                 JSStrictArgumentsObject::kSize + FixedArray::kHeaderSize));
  __ Allocate(ecx, edx, edi, no_reg, &allocate, NO_ALLOCATION_FLAGS);
  __ bind(&done_allocate);

  // Setup the elements array in edx.
  __ mov(FieldOperand(edx, FixedArray::kMapOffset),
         isolate()->factory()->fixed_array_map());
  __ mov(FieldOperand(edx, FixedArray::kLengthOffset), eax);
  {
    Label loop, done_loop;
    __ Move(ecx, Smi::kZero);
    __ bind(&loop);
    __ cmp(ecx, eax);
    __ j(equal, &done_loop, Label::kNear);
    __ mov(edi, Operand(ebx, 0 * kPointerSize));
    __ mov(FieldOperand(edx, ecx, times_half_pointer_size,
                        FixedArray::kHeaderSize),
           edi);
    __ sub(ebx, Immediate(1 * kPointerSize));
    __ add(ecx, Immediate(Smi::FromInt(1)));
    __ jmp(&loop);
    __ bind(&done_loop);
  }

  // Setup the rest parameter array in edi.
  __ lea(edi,
         Operand(edx, eax, times_half_pointer_size, FixedArray::kHeaderSize));
  __ LoadGlobalFunction(Context::STRICT_ARGUMENTS_MAP_INDEX, ecx);
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kMapOffset), ecx);
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kPropertiesOffset),
         isolate()->factory()->empty_fixed_array());
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kElementsOffset), edx);
  __ mov(FieldOperand(edi, JSStrictArgumentsObject::kLengthOffset), eax);
  STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
  __ mov(eax, edi);
  __ Ret();

  // Fall back to %AllocateInNewSpace (if not too big).
  Label too_big_for_new_space;
  __ bind(&allocate);
  __ cmp(ecx, Immediate(kMaxRegularHeapObjectSize));
  __ j(greater, &too_big_for_new_space);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(ecx);
    __ Push(eax);
    __ Push(ebx);
    __ Push(ecx);
    __ CallRuntime(Runtime::kAllocateInNewSpace);
    __ mov(edx, eax);
    __ Pop(ebx);
    __ Pop(eax);
  }
  __ jmp(&done_allocate);

  // Fall back to %NewStrictArguments.
  __ bind(&too_big_for_new_space);
  __ PopReturnAddressTo(ecx);
  // We reload the function from the caller frame due to register pressure
  // within this stub. This is the slow path, hence reloading is preferable.
  if (skip_stub_frame()) {
    // For Ignition we need to skip the handler/stub frame to reach the
    // JavaScript frame for the function.
    __ mov(edx, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
    __ Push(Operand(edx, StandardFrameConstants::kFunctionOffset));
  } else {
    __ Push(Operand(ebp, StandardFrameConstants::kFunctionOffset));
  }
  __ PushReturnAddressFrom(ecx);
  __ TailCallRuntime(Runtime::kNewStrictArguments);
}

// Generates an Operand for saving parameters after PrepareCallApiFunction.
static Operand ApiParameterOperand(int index) {
  return Operand(esp, index * kPointerSize);
}


// Prepares stack to put arguments (aligns and so on). Reserves
// space for return value if needed (assumes the return value is a handle).
// Arguments must be stored in ApiParameterOperand(0), ApiParameterOperand(1)
// etc. Saves context (esi). If space was reserved for return value then
// stores the pointer to the reserved slot into esi.
static void PrepareCallApiFunction(MacroAssembler* masm, int argc) {
  __ EnterApiExitFrame(argc);
  if (__ emit_debug_code()) {
    __ mov(esi, Immediate(bit_cast<int32_t>(kZapValue)));
  }
}


// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Clobbers ebx, edi and
// caller-save registers.  Restores context.  On return removes
// stack_space * kPointerSize (GCed).
static void CallApiFunctionAndReturn(MacroAssembler* masm,
                                     Register function_address,
                                     ExternalReference thunk_ref,
                                     Operand thunk_last_arg, int stack_space,
                                     Operand* stack_space_operand,
                                     Operand return_value_operand,
                                     Operand* context_restore_operand) {
  Isolate* isolate = masm->isolate();

  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  ExternalReference limit_address =
      ExternalReference::handle_scope_limit_address(isolate);
  ExternalReference level_address =
      ExternalReference::handle_scope_level_address(isolate);

  DCHECK(edx.is(function_address));
  // Allocate HandleScope in callee-save registers.
  __ mov(ebx, Operand::StaticVariable(next_address));
  __ mov(edi, Operand::StaticVariable(limit_address));
  __ add(Operand::StaticVariable(level_address), Immediate(1));

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, eax);
    __ mov(Operand(esp, 0),
           Immediate(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_enter_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }


  Label profiler_disabled;
  Label end_profiler_check;
  __ mov(eax, Immediate(ExternalReference::is_profiling_address(isolate)));
  __ cmpb(Operand(eax, 0), Immediate(0));
  __ j(zero, &profiler_disabled);

  // Additional parameter is the address of the actual getter function.
  __ mov(thunk_last_arg, function_address);
  // Call the api function.
  __ mov(eax, Immediate(thunk_ref));
  __ call(eax);
  __ jmp(&end_profiler_check);

  __ bind(&profiler_disabled);
  // Call the api function.
  __ call(function_address);
  __ bind(&end_profiler_check);

  if (FLAG_log_timer_events) {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ PushSafepointRegisters();
    __ PrepareCallCFunction(1, eax);
    __ mov(Operand(esp, 0),
           Immediate(ExternalReference::isolate_address(isolate)));
    __ CallCFunction(ExternalReference::log_leave_external_function(isolate),
                     1);
    __ PopSafepointRegisters();
  }

  Label prologue;
  // Load the value from ReturnValue
  __ mov(eax, return_value_operand);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  __ bind(&prologue);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ mov(Operand::StaticVariable(next_address), ebx);
  __ sub(Operand::StaticVariable(level_address), Immediate(1));
  __ Assert(above_equal, kInvalidHandleScopeLevel);
  __ cmp(edi, Operand::StaticVariable(limit_address));
  __ j(not_equal, &delete_allocated_handles);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    __ mov(esi, *context_restore_operand);
  }
  if (stack_space_operand != nullptr) {
    __ mov(ebx, *stack_space_operand);
  }
  __ LeaveApiExitFrame(!restore_context);

  // Check if the function scheduled an exception.
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate);
  __ cmp(Operand::StaticVariable(scheduled_exception_address),
         Immediate(isolate->factory()->the_hole_value()));
  __ j(not_equal, &promote_scheduled_exception);

#if DEBUG
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = eax;
  Register map = ecx;

  __ JumpIfSmi(return_value, &ok, Label::kNear);
  __ mov(map, FieldOperand(return_value, HeapObject::kMapOffset));

  __ CmpInstanceType(map, LAST_NAME_TYPE);
  __ j(below_equal, &ok, Label::kNear);

  __ CmpInstanceType(map, FIRST_JS_RECEIVER_TYPE);
  __ j(above_equal, &ok, Label::kNear);

  __ cmp(map, isolate->factory()->heap_number_map());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->undefined_value());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->true_value());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->false_value());
  __ j(equal, &ok, Label::kNear);

  __ cmp(return_value, isolate->factory()->null_value());
  __ j(equal, &ok, Label::kNear);

  __ Abort(kAPICallReturnedInvalidObject);

  __ bind(&ok);
#endif

  if (stack_space_operand != nullptr) {
    DCHECK_EQ(0, stack_space);
    __ pop(ecx);
    __ add(esp, ebx);
    __ jmp(ecx);
  } else {
    __ ret(stack_space * kPointerSize);
  }

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  ExternalReference delete_extensions =
      ExternalReference::delete_handle_scope_extensions(isolate);
  __ bind(&delete_allocated_handles);
  __ mov(Operand::StaticVariable(limit_address), edi);
  __ mov(edi, eax);
  __ mov(Operand(esp, 0),
         Immediate(ExternalReference::isolate_address(isolate)));
  __ mov(eax, Immediate(delete_extensions));
  __ call(eax);
  __ mov(eax, edi);
  __ jmp(&leave_exit_frame);
}

void CallApiCallbackStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- edi                 : callee
  //  -- ebx                 : call_data
  //  -- ecx                 : holder
  //  -- edx                 : api_function_address
  //  -- esi                 : context
  //  --
  //  -- esp[0]              : return address
  //  -- esp[4]              : last argument
  //  -- ...
  //  -- esp[argc * 4]       : first argument
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  Register callee = edi;
  Register call_data = ebx;
  Register holder = ecx;
  Register api_function_address = edx;
  Register context = esi;
  Register return_address = eax;

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kNewTargetIndex == 7);
  STATIC_ASSERT(FCA::kArgsLength == 8);

  __ pop(return_address);

  // new target
  __ PushRoot(Heap::kUndefinedValueRootIndex);

  // context save.
  __ push(context);

  // callee
  __ push(callee);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  if (!call_data_undefined()) {
    // return value
    __ push(Immediate(masm->isolate()->factory()->undefined_value()));
    // return value default
    __ push(Immediate(masm->isolate()->factory()->undefined_value()));
  } else {
    // return value
    __ push(scratch);
    // return value default
    __ push(scratch);
  }
  // isolate
  __ push(Immediate(reinterpret_cast<int>(masm->isolate())));
  // holder
  __ push(holder);

  __ mov(scratch, esp);

  // push return address
  __ push(return_address);

  if (!is_lazy()) {
    // load context from callee
    __ mov(context, FieldOperand(callee, JSFunction::kContextOffset));
  }

  // API function gets reference to the v8::Arguments. If CPU profiler
  // is enabled wrapper function will be called and we need to pass
  // address of the callback as additional parameter, always allocate
  // space for it.
  const int kApiArgc = 1 + 1;

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 3;

  PrepareCallApiFunction(masm, kApiArgc + kApiStackSpace);

  // FunctionCallbackInfo::implicit_args_.
  __ mov(ApiParameterOperand(2), scratch);
  __ add(scratch, Immediate((argc() + FCA::kArgsLength - 1) * kPointerSize));
  // FunctionCallbackInfo::values_.
  __ mov(ApiParameterOperand(3), scratch);
  // FunctionCallbackInfo::length_.
  __ Move(ApiParameterOperand(4), Immediate(argc()));

  // v8::InvocationCallback's argument.
  __ lea(scratch, ApiParameterOperand(2));
  __ mov(ApiParameterOperand(0), scratch);

  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(masm->isolate());

  Operand context_restore_operand(ebp,
                                  (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument
  int return_value_offset = 0;
  if (is_store()) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  Operand return_value_operand(ebp, return_value_offset * kPointerSize);
  int stack_space = 0;
  Operand length_operand = ApiParameterOperand(4);
  Operand* stack_space_operand = &length_operand;
  stack_space = argc() + FCA::kArgsLength + 1;
  stack_space_operand = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           ApiParameterOperand(1), stack_space,
                           stack_space_operand, return_value_operand,
                           &context_restore_operand);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  STATIC_ASSERT(PropertyCallbackArguments::kShouldThrowOnErrorIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 6);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 7);

  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = ebx;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  __ pop(scratch);  // Pop return address to extend the frame.
  __ push(receiver);
  __ push(FieldOperand(callback, AccessorInfo::kDataOffset));
  __ PushRoot(Heap::kUndefinedValueRootIndex);  // ReturnValue
  // ReturnValue default value
  __ PushRoot(Heap::kUndefinedValueRootIndex);
  __ push(Immediate(ExternalReference::isolate_address(isolate())));
  __ push(holder);
  __ push(Immediate(Smi::kZero));  // should_throw_on_error -> false
  __ push(FieldOperand(callback, AccessorInfo::kNameOffset));
  __ push(scratch);  // Restore return address.

  // v8::PropertyCallbackInfo::args_ array and name handle.
  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  // Allocate v8::PropertyCallbackInfo object, arguments for callback and
  // space for optional callback address parameter (in case CPU profiler is
  // active) in non-GCed stack space.
  const int kApiArgc = 3 + 1;

  // Load address of v8::PropertyAccessorInfo::args_ array.
  __ lea(scratch, Operand(esp, 2 * kPointerSize));

  PrepareCallApiFunction(masm, kApiArgc);
  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  Operand info_object = ApiParameterOperand(3);
  __ mov(info_object, scratch);

  // Name as handle.
  __ sub(scratch, Immediate(kPointerSize));
  __ mov(ApiParameterOperand(0), scratch);
  // Arguments pointer.
  __ lea(scratch, info_object);
  __ mov(ApiParameterOperand(1), scratch);
  // Reserve space for optional callback address parameter.
  Operand thunk_last_arg = ApiParameterOperand(2);

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());

  __ mov(scratch, FieldOperand(callback, AccessorInfo::kJsGetterOffset));
  Register function_address = edx;
  __ mov(function_address,
         FieldOperand(scratch, Foreign::kForeignAddressOffset));
  // +3 is to skip prolog, return address and name handle.
  Operand return_value_operand(
      ebp, (PropertyCallbackArguments::kReturnValueOffset + 3) * kPointerSize);
  CallApiFunctionAndReturn(masm, function_address, thunk_ref, thunk_last_arg,
                           kStackUnwindSpace, nullptr, return_value_operand,
                           NULL);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87

// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_MIPS)

#include "bootstrapper.h"
#include "code-stubs.h"
#include "codegen.h"
#include "regexp-macro-assembler.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)

static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cc,
                                          bool never_nan_nan);
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* rhs_not_nan,
                                    Label* slow,
                                    bool strict);
static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm, Condition cc);
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs);


// Check if the operand is a heap number.
static void EmitCheckForHeapNumber(MacroAssembler* masm, Register operand,
                                   Register scratch1, Register scratch2,
                                   Label* not_a_heap_number) {
  __ lw(scratch1, FieldMemOperand(operand, HeapObject::kMapOffset));
  __ LoadRoot(scratch2, Heap::kHeapNumberMapRootIndex);
  __ Branch(not_a_heap_number, ne, scratch1, Operand(scratch2));
}


void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in a0.
  Label check_heap_number, call_builtin;
  __ JumpIfNotSmi(a0, &check_heap_number);
  __ mov(v0, a0);
  __ Ret();

  __ bind(&check_heap_number);
  EmitCheckForHeapNumber(masm, a0, a1, t0, &call_builtin);
  __ mov(v0, a0);
  __ Ret();

  __ bind(&call_builtin);
  __ push(a0);
  __ InvokeBuiltin(Builtins::TO_NUMBER, JUMP_FUNCTION);
}


void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Create a new closure from the given function info in new
  // space. Set the context to the current context in cp.
  Label gc;

  // Pop the function info from the stack.
  __ pop(a3);

  // Attempt to allocate new JSFunction in new space.
  __ AllocateInNewSpace(JSFunction::kSize,
                        v0,
                        a1,
                        a2,
                        &gc,
                        TAG_OBJECT);

  int map_index = strict_mode_ == kStrictMode
      ? Context::STRICT_MODE_FUNCTION_MAP_INDEX
      : Context::FUNCTION_MAP_INDEX;

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ lw(a2, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ lw(a2, FieldMemOperand(a2, GlobalObject::kGlobalContextOffset));
  __ lw(a2, MemOperand(a2, Context::SlotOffset(map_index)));
  __ sw(a2, FieldMemOperand(v0, HeapObject::kMapOffset));

  // Initialize the rest of the function. We don't have to update the
  // write barrier because the allocated object is in new space.
  __ LoadRoot(a1, Heap::kEmptyFixedArrayRootIndex);
  __ LoadRoot(a2, Heap::kTheHoleValueRootIndex);
  __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
  __ sw(a1, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(a1, FieldMemOperand(v0, JSObject::kElementsOffset));
  __ sw(a2, FieldMemOperand(v0, JSFunction::kPrototypeOrInitialMapOffset));
  __ sw(a3, FieldMemOperand(v0, JSFunction::kSharedFunctionInfoOffset));
  __ sw(cp, FieldMemOperand(v0, JSFunction::kContextOffset));
  __ sw(a1, FieldMemOperand(v0, JSFunction::kLiteralsOffset));
  __ sw(t0, FieldMemOperand(v0, JSFunction::kNextFunctionLinkOffset));

  // Initialize the code pointer in the function to be the one
  // found in the shared function info object.
  __ lw(a3, FieldMemOperand(a3, SharedFunctionInfo::kCodeOffset));
  __ Addu(a3, a3, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ sw(a3, FieldMemOperand(v0, JSFunction::kCodeEntryOffset));

  // Return result. The argument function info has been popped already.
  __ Ret();

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ LoadRoot(t0, Heap::kFalseValueRootIndex);
  __ Push(cp, a3, t0);
  __ TailCallRuntime(Runtime::kNewClosure, 3, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;

  // Attempt to allocate the context in new space.
  __ AllocateInNewSpace(FixedArray::SizeFor(length),
                        v0,
                        a1,
                        a2,
                        &gc,
                        TAG_OBJECT);

  // Load the function from the stack.
  __ lw(a3, MemOperand(sp, 0));

  // Setup the object header.
  __ LoadRoot(a2, Heap::kFunctionContextMapRootIndex);
  __ sw(a2, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ li(a2, Operand(Smi::FromInt(length)));
  __ sw(a2, FieldMemOperand(v0, FixedArray::kLengthOffset));

  // Setup the fixed slots.
  __ li(a1, Operand(Smi::FromInt(0)));
  __ sw(a3, MemOperand(v0, Context::SlotOffset(Context::CLOSURE_INDEX)));
  __ sw(cp, MemOperand(v0, Context::SlotOffset(Context::PREVIOUS_INDEX)));
  __ sw(a1, MemOperand(v0, Context::SlotOffset(Context::EXTENSION_INDEX)));

  // Copy the global object from the previous context.
  __ lw(a1, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ sw(a1, MemOperand(v0, Context::SlotOffset(Context::GLOBAL_INDEX)));

  // Initialize the rest of the slots to undefined.
  __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ sw(a1, MemOperand(v0, Context::SlotOffset(i)));
  }

  // Remove the on-stack argument and return.
  __ mov(cp, v0);
  __ Pop();
  __ Ret();

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kNewFunctionContext, 1, 1);
}


void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  // [sp]: constant elements.
  // [sp + kPointerSize]: literal index.
  // [sp + (2 * kPointerSize)]: literals array.

  // All sizes here are multiples of kPointerSize.
  int elements_size = (length_ > 0) ? FixedArray::SizeFor(length_) : 0;
  int size = JSArray::kSize + elements_size;

  // Load boilerplate object into r3 and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ lw(a3, MemOperand(sp, 2 * kPointerSize));
  __ lw(a0, MemOperand(sp, 1 * kPointerSize));
  __ Addu(a3, a3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ sll(t0, a0, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(t0, a3, t0);
  __ lw(a3, MemOperand(t0));
  __ LoadRoot(t1, Heap::kUndefinedValueRootIndex);
  __ Branch(&slow_case, eq, a3, Operand(t1));

  if (FLAG_debug_code) {
    const char* message;
    Heap::RootListIndex expected_map_index;
    if (mode_ == CLONE_ELEMENTS) {
      message = "Expected (writable) fixed array";
      expected_map_index = Heap::kFixedArrayMapRootIndex;
    } else {
      ASSERT(mode_ == COPY_ON_WRITE_ELEMENTS);
      message = "Expected copy-on-write fixed array";
      expected_map_index = Heap::kFixedCOWArrayMapRootIndex;
    }
    __ push(a3);
    __ lw(a3, FieldMemOperand(a3, JSArray::kElementsOffset));
    __ lw(a3, FieldMemOperand(a3, HeapObject::kMapOffset));
    __ LoadRoot(at, expected_map_index);
    __ Assert(eq, message, a3, Operand(at));
    __ pop(a3);
  }

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  // Return new object in v0.
  __ AllocateInNewSpace(size,
                        v0,
                        a1,
                        a2,
                        &slow_case,
                        TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length_ == 0)) {
      __ lw(a1, FieldMemOperand(a3, i));
      __ sw(a1, FieldMemOperand(v0, i));
    }
  }

  if (length_ > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ lw(a3, FieldMemOperand(a3, JSArray::kElementsOffset));
    __ Addu(a2, v0, Operand(JSArray::kSize));
    __ sw(a2, FieldMemOperand(v0, JSArray::kElementsOffset));

    // Copy the elements array.
    __ CopyFields(a2, a3, a1.bit(), elements_size / kPointerSize);
  }

  // Return and remove the on-stack parameters.
  __ Addu(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&slow_case);
  __ TailCallRuntime(Runtime::kCreateArrayLiteralShallow, 3, 1);
}


// Takes a Smi and converts to an IEEE 64 bit floating point value in two
// registers.  The format is 1 sign bit, 11 exponent bits (biased 1023) and
// 52 fraction bits (20 in the first word, 32 in the second).  Zeros is a
// scratch register.  Destroys the source register.  No GC occurs during this
// stub so you don't have to set up the frame.
class ConvertToDoubleStub : public CodeStub {
 public:
  ConvertToDoubleStub(Register result_reg_1,
                      Register result_reg_2,
                      Register source_reg,
                      Register scratch_reg)
      : result1_(result_reg_1),
        result2_(result_reg_2),
        source_(source_reg),
        zeros_(scratch_reg) { }

 private:
  Register result1_;
  Register result2_;
  Register source_;
  Register zeros_;

  // Minor key encoding in 16 bits.
  class ModeBits: public BitField<OverwriteMode, 0, 2> {};
  class OpBits: public BitField<Token::Value, 2, 14> {};

  Major MajorKey() { return ConvertToDouble; }
  int MinorKey() {
    // Encode the parameters in a unique 16 bit value.
    return  result1_.code() +
           (result2_.code() << 4) +
           (source_.code() << 8) +
           (zeros_.code() << 12);
  }

  void Generate(MacroAssembler* masm);
};


void ConvertToDoubleStub::Generate(MacroAssembler* masm) {
#ifndef BIG_ENDIAN_FLOATING_POINT
  Register exponent = result1_;
  Register mantissa = result2_;
#else
  Register exponent = result2_;
  Register mantissa = result1_;
#endif
  Label not_special;
  // Convert from Smi to integer.
  __ sra(source_, source_, kSmiTagSize);
  // Move sign bit from source to destination.  This works because the sign bit
  // in the exponent word of the double has the same position and polarity as
  // the 2's complement sign bit in a Smi.
  STATIC_ASSERT(HeapNumber::kSignMask == 0x80000000u);
  __ And(exponent, source_, Operand(HeapNumber::kSignMask));
  // Subtract from 0 if source was negative.
  __ subu(at, zero_reg, source_);
  __ movn(source_, at, exponent);

  // We have -1, 0 or 1, which we treat specially. Register source_ contains
  // absolute value: it is either equal to 1 (special case of -1 and 1),
  // greater than 1 (not a special case) or less than 1 (special case of 0).
  __ Branch(&not_special, gt, source_, Operand(1));

  // For 1 or -1 we need to or in the 0 exponent (biased to 1023).
  static const uint32_t exponent_word_for_1 =
      HeapNumber::kExponentBias << HeapNumber::kExponentShift;
  // Safe to use 'at' as dest reg here.
  __ Or(at, exponent, Operand(exponent_word_for_1));
  __ movn(exponent, at, source_);  // Write exp when source not 0.
  // 1, 0 and -1 all have 0 for the second word.
  __ mov(mantissa, zero_reg);
  __ Ret();

  __ bind(&not_special);
  // Count leading zeros.
  // Gets the wrong answer for 0, but we already checked for that case above.
  __ clz(zeros_, source_);
  // Compute exponent and or it into the exponent register.
  // We use mantissa as a scratch register here.
  __ li(mantissa, Operand(31 + HeapNumber::kExponentBias));
  __ subu(mantissa, mantissa, zeros_);
  __ sll(mantissa, mantissa, HeapNumber::kExponentShift);
  __ Or(exponent, exponent, mantissa);

  // Shift up the source chopping the top bit off.
  __ Addu(zeros_, zeros_, Operand(1));
  // This wouldn't work for 1.0 or -1.0 as the shift would be 32 which means 0.
  __ sllv(source_, source_, zeros_);
  // Compute lower part of fraction (last 12 bits).
  __ sll(mantissa, source_, HeapNumber::kMantissaBitsInTopWord);
  // And the top (top 20 bits).
  __ srl(source_, source_, 32 - HeapNumber::kMantissaBitsInTopWord);
  __ or_(exponent, exponent, source_);

  __ Ret();
}


void FloatingPointHelper::LoadSmis(MacroAssembler* masm,
                                   FloatingPointHelper::Destination destination,
                                   Register scratch1,
                                   Register scratch2) {
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    __ sra(scratch1, a0, kSmiTagSize);
    __ mtc1(scratch1, f14);
    __ cvt_d_w(f14, f14);
    __ sra(scratch1, a1, kSmiTagSize);
    __ mtc1(scratch1, f12);
    __ cvt_d_w(f12, f12);
    if (destination == kCoreRegisters) {
      __ Move(a2, a3, f14);
      __ Move(a0, a1, f12);
    }
  } else {
    ASSERT(destination == kCoreRegisters);
    // Write Smi from a0 to a3 and a2 in double format.
    __ mov(scratch1, a0);
    ConvertToDoubleStub stub1(a3, a2, scratch1, scratch2);
    __ push(ra);
    __ Call(stub1.GetCode());
    // Write Smi from a1 to a1 and a0 in double format.
    __ mov(scratch1, a1);
    ConvertToDoubleStub stub2(a1, a0, scratch1, scratch2);
    __ Call(stub2.GetCode());
    __ pop(ra);
  }
}


void FloatingPointHelper::LoadOperands(
    MacroAssembler* masm,
    FloatingPointHelper::Destination destination,
    Register heap_number_map,
    Register scratch1,
    Register scratch2,
    Label* slow) {

  // Load right operand (a0) to f12 or a2/a3.
  LoadNumber(masm, destination,
             a0, f14, a2, a3, heap_number_map, scratch1, scratch2, slow);

  // Load left operand (a1) to f14 or a0/a1.
  LoadNumber(masm, destination,
             a1, f12, a0, a1, heap_number_map, scratch1, scratch2, slow);
}


void FloatingPointHelper::LoadNumber(MacroAssembler* masm,
                                     Destination destination,
                                     Register object,
                                     FPURegister dst,
                                     Register dst1,
                                     Register dst2,
                                     Register heap_number_map,
                                     Register scratch1,
                                     Register scratch2,
                                     Label* not_number) {
  if (FLAG_debug_code) {
    __ AbortIfNotRootValue(heap_number_map,
                           Heap::kHeapNumberMapRootIndex,
                           "HeapNumberMap register clobbered.");
  }

  Label is_smi, done;

  __ JumpIfSmi(object, &is_smi);
  __ JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_number);

  // Handle loading a double from a heap number.
  if (CpuFeatures::IsSupported(FPU) &&
      destination == kFPURegisters) {
    CpuFeatures::Scope scope(FPU);
    // Load the double from tagged HeapNumber to double register.

    // ARM uses a workaround here because of the unaligned HeapNumber
    // kValueOffset. On MIPS this workaround is built into ldc1 so there's no
    // point in generating even more instructions.
    __ ldc1(dst, FieldMemOperand(object, HeapNumber::kValueOffset));
  } else {
    ASSERT(destination == kCoreRegisters);
    // Load the double from heap number to dst1 and dst2 in double format.
    __ lw(dst1, FieldMemOperand(object, HeapNumber::kValueOffset));
    __ lw(dst2, FieldMemOperand(object,
        HeapNumber::kValueOffset + kPointerSize));
  }
  __ Branch(&done);

  // Handle loading a double from a smi.
  __ bind(&is_smi);
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    // Convert smi to double using FPU instructions.
    __ SmiUntag(scratch1, object);
    __ mtc1(scratch1, dst);
    __ cvt_d_w(dst, dst);
    if (destination == kCoreRegisters) {
      // Load the converted smi to dst1 and dst2 in double format.
      __ Move(dst1, dst2, dst);
    }
  } else {
    ASSERT(destination == kCoreRegisters);
    // Write smi to dst1 and dst2 double format.
    __ mov(scratch1, object);
    ConvertToDoubleStub stub(dst2, dst1, scratch1, scratch2);
    __ push(ra);
    __ Call(stub.GetCode());
    __ pop(ra);
  }

  __ bind(&done);
}


void FloatingPointHelper::ConvertNumberToInt32(MacroAssembler* masm,
                                               Register object,
                                               Register dst,
                                               Register heap_number_map,
                                               Register scratch1,
                                               Register scratch2,
                                               Register scratch3,
                                               FPURegister double_scratch,
                                               Label* not_number) {
  if (FLAG_debug_code) {
    __ AbortIfNotRootValue(heap_number_map,
                           Heap::kHeapNumberMapRootIndex,
                           "HeapNumberMap register clobbered.");
  }
  Label is_smi;
  Label done;
  Label not_in_int32_range;

  __ JumpIfSmi(object, &is_smi);
  __ lw(scratch1, FieldMemOperand(object, HeapNumber::kMapOffset));
  __ Branch(not_number, ne, scratch1, Operand(heap_number_map));
  __ ConvertToInt32(object,
                    dst,
                    scratch1,
                    scratch2,
                    double_scratch,
                    &not_in_int32_range);
  __ jmp(&done);

  __ bind(&not_in_int32_range);
  __ lw(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
  __ lw(scratch2, FieldMemOperand(object, HeapNumber::kMantissaOffset));

  __ EmitOutOfInt32RangeTruncate(dst,
                                 scratch1,
                                 scratch2,
                                 scratch3);

  __ jmp(&done);

  __ bind(&is_smi);
  __ SmiUntag(dst, object);
  __ bind(&done);
}


void FloatingPointHelper::ConvertIntToDouble(MacroAssembler* masm,
                                             Register int_scratch,
                                             Destination destination,
                                             FPURegister double_dst,
                                             Register dst1,
                                             Register dst2,
                                             Register scratch2,
                                             FPURegister single_scratch) {
  ASSERT(!int_scratch.is(scratch2));
  ASSERT(!int_scratch.is(dst1));
  ASSERT(!int_scratch.is(dst2));

  Label done;

  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    __ mtc1(int_scratch, single_scratch);
    __ cvt_d_w(double_dst, single_scratch);
    if (destination == kCoreRegisters) {
      __ Move(dst1, dst2, double_dst);
    }
  } else {
    Label fewer_than_20_useful_bits;
    // Expected output:
    // |         dst2            |         dst1            |
    // | s |   exp   |              mantissa               |

    // Check for zero.
    __ mov(dst2, int_scratch);
    __ mov(dst1, int_scratch);
    __ Branch(&done, eq, int_scratch, Operand(zero_reg));

    // Preload the sign of the value.
    __ And(dst2, int_scratch, Operand(HeapNumber::kSignMask));
    // Get the absolute value of the object (as an unsigned integer).
    Label skip_sub;
    __ Branch(&skip_sub, ge, dst2, Operand(zero_reg));
    __ Subu(int_scratch, zero_reg, int_scratch);
    __ bind(&skip_sub);

    // Get mantisssa[51:20].

    // Get the position of the first set bit.
    __ clz(dst1, int_scratch);
    __ li(scratch2, 31);
    __ Subu(dst1, scratch2, dst1);

    // Set the exponent.
    __ Addu(scratch2, dst1, Operand(HeapNumber::kExponentBias));
    __ Ins(dst2, scratch2,
        HeapNumber::kExponentShift, HeapNumber::kExponentBits);

    // Clear the first non null bit.
    __ li(scratch2, Operand(1));
    __ sllv(scratch2, scratch2, dst1);
    __ li(at, -1);
    __ Xor(scratch2, scratch2, at);
    __ And(int_scratch, int_scratch, scratch2);

    // Get the number of bits to set in the lower part of the mantissa.
    __ Subu(scratch2, dst1, Operand(HeapNumber::kMantissaBitsInTopWord));
    __ Branch(&fewer_than_20_useful_bits, lt, scratch2, Operand(zero_reg));
    // Set the higher 20 bits of the mantissa.
    __ srlv(at, int_scratch, scratch2);
    __ or_(dst2, dst2, at);
    __ li(at, 32);
    __ subu(scratch2, at, scratch2);
    __ sllv(dst1, int_scratch, scratch2);
    __ Branch(&done);

    __ bind(&fewer_than_20_useful_bits);
    __ li(at, HeapNumber::kMantissaBitsInTopWord);
    __ subu(scratch2, at, dst1);
    __ sllv(scratch2, int_scratch, scratch2);
    __ Or(dst2, dst2, scratch2);
    // Set dst1 to 0.
    __ mov(dst1, zero_reg);
  }
  __ bind(&done);
}


void FloatingPointHelper::LoadNumberAsInt32Double(MacroAssembler* masm,
                                                  Register object,
                                                  Destination destination,
                                                  FPURegister double_dst,
                                                  Register dst1,
                                                  Register dst2,
                                                  Register heap_number_map,
                                                  Register scratch1,
                                                  Register scratch2,
                                                  FPURegister single_scratch,
                                                  Label* not_int32) {
  ASSERT(!scratch1.is(object) && !scratch2.is(object));
  ASSERT(!scratch1.is(scratch2));
  ASSERT(!heap_number_map.is(object) &&
         !heap_number_map.is(scratch1) &&
         !heap_number_map.is(scratch2));

  Label done, obj_is_not_smi;

  __ JumpIfNotSmi(object, &obj_is_not_smi);
  __ SmiUntag(scratch1, object);
  ConvertIntToDouble(masm, scratch1, destination, double_dst, dst1, dst2,
                     scratch2, single_scratch);
  __ Branch(&done);

  __ bind(&obj_is_not_smi);
  if (FLAG_debug_code) {
    __ AbortIfNotRootValue(heap_number_map,
                           Heap::kHeapNumberMapRootIndex,
                           "HeapNumberMap register clobbered.");
  }
  __ JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_int32);

  // Load the number.
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    // Load the double value.
    __ ldc1(double_dst, FieldMemOperand(object, HeapNumber::kValueOffset));

    // NOTE: ARM uses a MacroAssembler function here (EmitVFPTruncate).
    // On MIPS a lot of things cannot be implemented the same way so right
    // now it makes a lot more sense to just do things manually.

    // Save FCSR.
    __ cfc1(scratch1, FCSR);
    // Disable FPU exceptions.
    __ ctc1(zero_reg, FCSR);
    __ trunc_w_d(single_scratch, double_dst);
    // Retrieve FCSR.
    __ cfc1(scratch2, FCSR);
    // Restore FCSR.
    __ ctc1(scratch1, FCSR);

    // Check for inexact conversion or exception.
    __ And(scratch2, scratch2, kFCSRFlagMask);

    // Jump to not_int32 if the operation did not succeed.
    __ Branch(not_int32, ne, scratch2, Operand(zero_reg));

    if (destination == kCoreRegisters) {
      __ Move(dst1, dst2, double_dst);
    }

  } else {
    ASSERT(!scratch1.is(object) && !scratch2.is(object));
    // Load the double value in the destination registers.
    __ lw(dst2, FieldMemOperand(object, HeapNumber::kExponentOffset));
    __ lw(dst1, FieldMemOperand(object, HeapNumber::kMantissaOffset));

    // Check for 0 and -0.
    __ And(scratch1, dst1, Operand(~HeapNumber::kSignMask));
    __ Or(scratch1, scratch1, Operand(dst2));
    __ Branch(&done, eq, scratch1, Operand(zero_reg));

    // Check that the value can be exactly represented by a 32-bit integer.
    // Jump to not_int32 if that's not the case.
    DoubleIs32BitInteger(masm, dst1, dst2, scratch1, scratch2, not_int32);

    // dst1 and dst2 were trashed. Reload the double value.
    __ lw(dst2, FieldMemOperand(object, HeapNumber::kExponentOffset));
    __ lw(dst1, FieldMemOperand(object, HeapNumber::kMantissaOffset));
  }

  __ bind(&done);
}


void FloatingPointHelper::LoadNumberAsInt32(MacroAssembler* masm,
                                            Register object,
                                            Register dst,
                                            Register heap_number_map,
                                            Register scratch1,
                                            Register scratch2,
                                            Register scratch3,
                                            FPURegister double_scratch,
                                            Label* not_int32) {
  ASSERT(!dst.is(object));
  ASSERT(!scratch1.is(object) && !scratch2.is(object) && !scratch3.is(object));
  ASSERT(!scratch1.is(scratch2) &&
         !scratch1.is(scratch3) &&
         !scratch2.is(scratch3));

  Label done;

  // Untag the object into the destination register.
  __ SmiUntag(dst, object);
  // Just return if the object is a smi.
  __ JumpIfSmi(object, &done);

  if (FLAG_debug_code) {
    __ AbortIfNotRootValue(heap_number_map,
                           Heap::kHeapNumberMapRootIndex,
                           "HeapNumberMap register clobbered.");
  }
  __ JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_int32);

  // Object is a heap number.
  // Convert the floating point value to a 32-bit integer.
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    // Load the double value.
    __ ldc1(double_scratch, FieldMemOperand(object, HeapNumber::kValueOffset));

    // NOTE: ARM uses a MacroAssembler function here (EmitVFPTruncate).
    // On MIPS a lot of things cannot be implemented the same way so right
    // now it makes a lot more sense to just do things manually.

    // Save FCSR.
    __ cfc1(scratch1, FCSR);
    // Disable FPU exceptions.
    __ ctc1(zero_reg, FCSR);
    __ trunc_w_d(double_scratch, double_scratch);
    // Retrieve FCSR.
    __ cfc1(scratch2, FCSR);
    // Restore FCSR.
    __ ctc1(scratch1, FCSR);

    // Check for inexact conversion or exception.
    __ And(scratch2, scratch2, kFCSRFlagMask);

    // Jump to not_int32 if the operation did not succeed.
    __ Branch(not_int32, ne, scratch2, Operand(zero_reg));
    // Get the result in the destination register.
    __ mfc1(dst, double_scratch);

  } else {
    // Load the double value in the destination registers.
    __ lw(scratch2, FieldMemOperand(object, HeapNumber::kExponentOffset));
    __ lw(scratch1, FieldMemOperand(object, HeapNumber::kMantissaOffset));

    // Check for 0 and -0.
    __ And(dst, scratch1, Operand(~HeapNumber::kSignMask));
    __ Or(dst, scratch2, Operand(dst));
    __ Branch(&done, eq, dst, Operand(zero_reg));

    DoubleIs32BitInteger(masm, scratch1, scratch2, dst, scratch3, not_int32);

    // Registers state after DoubleIs32BitInteger.
    // dst: mantissa[51:20].
    // scratch2: 1

    // Shift back the higher bits of the mantissa.
    __ srlv(dst, dst, scratch3);
    // Set the implicit first bit.
    __ li(at, 32);
    __ subu(scratch3, at, scratch3);
    __ sllv(scratch2, scratch2, scratch3);
    __ Or(dst, dst, scratch2);
    // Set the sign.
    __ lw(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
    __ And(scratch1, scratch1, Operand(HeapNumber::kSignMask));
    Label skip_sub;
    __ Branch(&skip_sub, ge, scratch1, Operand(zero_reg));
    __ Subu(dst, zero_reg, dst);
    __ bind(&skip_sub);
  }

  __ bind(&done);
}


void FloatingPointHelper::DoubleIs32BitInteger(MacroAssembler* masm,
                                               Register src1,
                                               Register src2,
                                               Register dst,
                                               Register scratch,
                                               Label* not_int32) {
  // Get exponent alone in scratch.
  __ Ext(scratch,
         src1,
         HeapNumber::kExponentShift,
         HeapNumber::kExponentBits);

  // Substract the bias from the exponent.
  __ Subu(scratch, scratch, Operand(HeapNumber::kExponentBias));

  // src1: higher (exponent) part of the double value.
  // src2: lower (mantissa) part of the double value.
  // scratch: unbiased exponent.

  // Fast cases. Check for obvious non 32-bit integer values.
  // Negative exponent cannot yield 32-bit integers.
  __ Branch(not_int32, lt, scratch, Operand(zero_reg));
  // Exponent greater than 31 cannot yield 32-bit integers.
  // Also, a positive value with an exponent equal to 31 is outside of the
  // signed 32-bit integer range.
  // Another way to put it is that if (exponent - signbit) > 30 then the
  // number cannot be represented as an int32.
  Register tmp = dst;
  __ srl(at, src1, 31);
  __ subu(tmp, scratch, at);
  __ Branch(not_int32, gt, tmp, Operand(30));
  // - Bits [21:0] in the mantissa are not null.
  __ And(tmp, src2, 0x3fffff);
  __ Branch(not_int32, ne, tmp, Operand(zero_reg));

  // Otherwise the exponent needs to be big enough to shift left all the
  // non zero bits left. So we need the (30 - exponent) last bits of the
  // 31 higher bits of the mantissa to be null.
  // Because bits [21:0] are null, we can check instead that the
  // (32 - exponent) last bits of the 32 higher bits of the mantisssa are null.

  // Get the 32 higher bits of the mantissa in dst.
  __ Ext(dst,
         src2,
         HeapNumber::kMantissaBitsInTopWord,
         32 - HeapNumber::kMantissaBitsInTopWord);
  __ sll(at, src1, HeapNumber::kNonMantissaBitsInTopWord);
  __ or_(dst, dst, at);

  // Create the mask and test the lower bits (of the higher bits).
  __ li(at, 32);
  __ subu(scratch, at, scratch);
  __ li(src2, 1);
  __ sllv(src1, src2, scratch);
  __ Subu(src1, src1, Operand(1));
  __ And(src1, dst, src1);
  __ Branch(not_int32, ne, src1, Operand(zero_reg));
}


void FloatingPointHelper::CallCCodeForDoubleOperation(
    MacroAssembler* masm,
    Token::Value op,
    Register heap_number_result,
    Register scratch) {
  // Using core registers:
  // a0: Left value (least significant part of mantissa).
  // a1: Left value (sign, exponent, top of mantissa).
  // a2: Right value (least significant part of mantissa).
  // a3: Right value (sign, exponent, top of mantissa).

  // Assert that heap_number_result is saved.
  // We currently always use s0 to pass it.
  ASSERT(heap_number_result.is(s0));

  // Push the current return address before the C call.
  __ push(ra);
  __ PrepareCallCFunction(4, scratch);  // Two doubles are 4 arguments.
  if (!IsMipsSoftFloatABI) {
    CpuFeatures::Scope scope(FPU);
    // We are not using MIPS FPU instructions, and parameters for the runtime
    // function call are prepaired in a0-a3 registers, but function we are
    // calling is compiled with hard-float flag and expecting hard float ABI
    // (parameters in f12/f14 registers). We need to copy parameters from
    // a0-a3 registers to f12/f14 register pairs.
    __ Move(f12, a0, a1);
    __ Move(f14, a2, a3);
  }
  // Call C routine that may not cause GC or other trouble.
  __ CallCFunction(ExternalReference::double_fp_operation(op, masm->isolate()),
                   4);
  // Store answer in the overwritable heap number.
  if (!IsMipsSoftFloatABI) {
    CpuFeatures::Scope scope(FPU);
    // Double returned in register f0.
    __ sdc1(f0, FieldMemOperand(heap_number_result, HeapNumber::kValueOffset));
  } else {
    // Double returned in registers v0 and v1.
    __ sw(v1, FieldMemOperand(heap_number_result, HeapNumber::kExponentOffset));
    __ sw(v0, FieldMemOperand(heap_number_result, HeapNumber::kMantissaOffset));
  }
  // Place heap_number_result in v0 and return to the pushed return address.
  __ mov(v0, heap_number_result);
  __ pop(ra);
  __ Ret();
}


// See comment for class, this does NOT work for int32's that are in Smi range.
void WriteInt32ToHeapNumberStub::Generate(MacroAssembler* masm) {
  Label max_negative_int;
  // the_int_ has the answer which is a signed int32 but not a Smi.
  // We test for the special value that has a different exponent.
  STATIC_ASSERT(HeapNumber::kSignMask == 0x80000000u);
  // Test sign, and save for later conditionals.
  __ And(sign_, the_int_, Operand(0x80000000u));
  __ Branch(&max_negative_int, eq, the_int_, Operand(0x80000000u));

  // Set up the correct exponent in scratch_.  All non-Smi int32s have the same.
  // A non-Smi integer is 1.xxx * 2^30 so the exponent is 30 (biased).
  uint32_t non_smi_exponent =
      (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
  __ li(scratch_, Operand(non_smi_exponent));
  // Set the sign bit in scratch_ if the value was negative.
  __ or_(scratch_, scratch_, sign_);
  // Subtract from 0 if the value was negative.
  __ subu(at, zero_reg, the_int_);
  __ movn(the_int_, at, sign_);
  // We should be masking the implict first digit of the mantissa away here,
  // but it just ends up combining harmlessly with the last digit of the
  // exponent that happens to be 1.  The sign bit is 0 so we shift 10 to get
  // the most significant 1 to hit the last bit of the 12 bit sign and exponent.
  ASSERT(((1 << HeapNumber::kExponentShift) & non_smi_exponent) != 0);
  const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
  __ srl(at, the_int_, shift_distance);
  __ or_(scratch_, scratch_, at);
  __ sw(scratch_, FieldMemOperand(the_heap_number_,
                                   HeapNumber::kExponentOffset));
  __ sll(scratch_, the_int_, 32 - shift_distance);
  __ sw(scratch_, FieldMemOperand(the_heap_number_,
                                   HeapNumber::kMantissaOffset));
  __ Ret();

  __ bind(&max_negative_int);
  // The max negative int32 is stored as a positive number in the mantissa of
  // a double because it uses a sign bit instead of using two's complement.
  // The actual mantissa bits stored are all 0 because the implicit most
  // significant 1 bit is not stored.
  non_smi_exponent += 1 << HeapNumber::kExponentShift;
  __ li(scratch_, Operand(HeapNumber::kSignMask | non_smi_exponent));
  __ sw(scratch_,
        FieldMemOperand(the_heap_number_, HeapNumber::kExponentOffset));
  __ mov(scratch_, zero_reg);
  __ sw(scratch_,
        FieldMemOperand(the_heap_number_, HeapNumber::kMantissaOffset));
  __ Ret();
}


// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cc,
                                          bool never_nan_nan) {
  Label not_identical;
  Label heap_number, return_equal;
  Register exp_mask_reg = t5;

  __ Branch(&not_identical, ne, a0, Operand(a1));

  // The two objects are identical. If we know that one of them isn't NaN then
  // we now know they test equal.
  if (cc != eq || !never_nan_nan) {
    __ li(exp_mask_reg, Operand(HeapNumber::kExponentMask));

    // Test for NaN. Sadly, we can't just compare to factory->nan_value(),
    // so we do the second best thing - test it ourselves.
    // They are both equal and they are not both Smis so both of them are not
    // Smis. If it's not a heap number, then return equal.
    if (cc == less || cc == greater) {
      __ GetObjectType(a0, t4, t4);
      __ Branch(slow, greater, t4, Operand(FIRST_SPEC_OBJECT_TYPE));
    } else {
      __ GetObjectType(a0, t4, t4);
      __ Branch(&heap_number, eq, t4, Operand(HEAP_NUMBER_TYPE));
      // Comparing JS objects with <=, >= is complicated.
      if (cc != eq) {
      __ Branch(slow, greater, t4, Operand(FIRST_SPEC_OBJECT_TYPE));
        // Normally here we fall through to return_equal, but undefined is
        // special: (undefined == undefined) == true, but
        // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
        if (cc == less_equal || cc == greater_equal) {
          __ Branch(&return_equal, ne, t4, Operand(ODDBALL_TYPE));
          __ LoadRoot(t2, Heap::kUndefinedValueRootIndex);
          __ Branch(&return_equal, ne, a0, Operand(t2));
          if (cc == le) {
            // undefined <= undefined should fail.
            __ li(v0, Operand(GREATER));
          } else  {
            // undefined >= undefined should fail.
            __ li(v0, Operand(LESS));
          }
          __ Ret();
        }
      }
    }
  }

  __ bind(&return_equal);
  if (cc == less) {
    __ li(v0, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cc == greater) {
    __ li(v0, Operand(LESS));     // Things aren't greater than themselves.
  } else {
    __ mov(v0, zero_reg);         // Things are <=, >=, ==, === themselves.
  }
  __ Ret();

  if (cc != eq || !never_nan_nan) {
    // For less and greater we don't have to check for NaN since the result of
    // x < x is false regardless.  For the others here is some code to check
    // for NaN.
    if (cc != lt && cc != gt) {
      __ bind(&heap_number);
      // It is a heap number, so return non-equal if it's NaN and equal if it's
      // not NaN.

      // The representation of NaN values has all exponent bits (52..62) set,
      // and not all mantissa bits (0..51) clear.
      // Read top bits of double representation (second word of value).
      __ lw(t2, FieldMemOperand(a0, HeapNumber::kExponentOffset));
      // Test that exponent bits are all set.
      __ And(t3, t2, Operand(exp_mask_reg));
      // If all bits not set (ne cond), then not a NaN, objects are equal.
      __ Branch(&return_equal, ne, t3, Operand(exp_mask_reg));

      // Shift out flag and all exponent bits, retaining only mantissa.
      __ sll(t2, t2, HeapNumber::kNonMantissaBitsInTopWord);
      // Or with all low-bits of mantissa.
      __ lw(t3, FieldMemOperand(a0, HeapNumber::kMantissaOffset));
      __ Or(v0, t3, Operand(t2));
      // For equal we already have the right value in v0:  Return zero (equal)
      // if all bits in mantissa are zero (it's an Infinity) and non-zero if
      // not (it's a NaN).  For <= and >= we need to load v0 with the failing
      // value if it's a NaN.
      if (cc != eq) {
        // All-zero means Infinity means equal.
        __ Ret(eq, v0, Operand(zero_reg));
        if (cc == le) {
          __ li(v0, Operand(GREATER));  // NaN <= NaN should fail.
        } else {
          __ li(v0, Operand(LESS));     // NaN >= NaN should fail.
        }
      }
      __ Ret();
    }
    // No fall through here.
  }

  __ bind(&not_identical);
}


static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* both_loaded_as_doubles,
                                    Label* slow,
                                    bool strict) {
  ASSERT((lhs.is(a0) && rhs.is(a1)) ||
         (lhs.is(a1) && rhs.is(a0)));

  Label lhs_is_smi;
  __ And(t0, lhs, Operand(kSmiTagMask));
  __ Branch(&lhs_is_smi, eq, t0, Operand(zero_reg));
  // Rhs is a Smi.
  // Check whether the non-smi is a heap number.
  __ GetObjectType(lhs, t4, t4);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed. Return non-equal (lhs is already not zero).
    __ mov(v0, lhs);
    __ Ret(ne, t4, Operand(HEAP_NUMBER_TYPE));
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number. Call
    // the runtime.
    __ Branch(slow, ne, t4, Operand(HEAP_NUMBER_TYPE));
  }

  // Rhs is a smi, lhs is a number.
  // Convert smi rhs to double.
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    __ sra(at, rhs, kSmiTagSize);
    __ mtc1(at, f14);
    __ cvt_d_w(f14, f14);
    __ ldc1(f12, FieldMemOperand(lhs, HeapNumber::kValueOffset));
  } else {
    // Load lhs to a double in a2, a3.
    __ lw(a3, FieldMemOperand(lhs, HeapNumber::kValueOffset + 4));
    __ lw(a2, FieldMemOperand(lhs, HeapNumber::kValueOffset));

    // Write Smi from rhs to a1 and a0 in double format. t5 is scratch.
    __ mov(t6, rhs);
    ConvertToDoubleStub stub1(a1, a0, t6, t5);
    __ push(ra);
    __ Call(stub1.GetCode());

    __ pop(ra);
  }

  // We now have both loaded as doubles.
  __ jmp(both_loaded_as_doubles);

  __ bind(&lhs_is_smi);
  // Lhs is a Smi.  Check whether the non-smi is a heap number.
  __ GetObjectType(rhs, t4, t4);
  if (strict) {
    // If lhs was not a number and rhs was a Smi then strict equality cannot
    // succeed. Return non-equal.
    __ li(v0, Operand(1));
    __ Ret(ne, t4, Operand(HEAP_NUMBER_TYPE));
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number. Call
    // the runtime.
    __ Branch(slow, ne, t4, Operand(HEAP_NUMBER_TYPE));
  }

  // Lhs is a smi, rhs is a number.
  // Convert smi lhs to double.
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    __ sra(at, lhs, kSmiTagSize);
    __ mtc1(at, f12);
    __ cvt_d_w(f12, f12);
    __ ldc1(f14, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  } else {
    // Convert lhs to a double format. t5 is scratch.
    __ mov(t6, lhs);
    ConvertToDoubleStub stub2(a3, a2, t6, t5);
    __ push(ra);
    __ Call(stub2.GetCode());
    __ pop(ra);
    // Load rhs to a double in a1, a0.
    if (rhs.is(a0)) {
      __ lw(a1, FieldMemOperand(rhs, HeapNumber::kValueOffset + 4));
      __ lw(a0, FieldMemOperand(rhs, HeapNumber::kValueOffset));
    } else {
      __ lw(a0, FieldMemOperand(rhs, HeapNumber::kValueOffset));
      __ lw(a1, FieldMemOperand(rhs, HeapNumber::kValueOffset + 4));
    }
  }
  // Fall through to both_loaded_as_doubles.
}


void EmitNanCheck(MacroAssembler* masm, Condition cc) {
  bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    // Lhs and rhs are already loaded to f12 and f14 register pairs.
    __ Move(t0, t1, f14);
    __ Move(t2, t3, f12);
  } else {
    // Lhs and rhs are already loaded to GP registers.
    __ mov(t0, a0);  // a0 has LS 32 bits of rhs.
    __ mov(t1, a1);  // a1 has MS 32 bits of rhs.
    __ mov(t2, a2);  // a2 has LS 32 bits of lhs.
    __ mov(t3, a3);  // a3 has MS 32 bits of lhs.
  }
  Register rhs_exponent = exp_first ? t0 : t1;
  Register lhs_exponent = exp_first ? t2 : t3;
  Register rhs_mantissa = exp_first ? t1 : t0;
  Register lhs_mantissa = exp_first ? t3 : t2;
  Label one_is_nan, neither_is_nan;
  Label lhs_not_nan_exp_mask_is_loaded;

  Register exp_mask_reg = t4;
  __ li(exp_mask_reg, HeapNumber::kExponentMask);
  __ and_(t5, lhs_exponent, exp_mask_reg);
  __ Branch(&lhs_not_nan_exp_mask_is_loaded, ne, t5, Operand(exp_mask_reg));

  __ sll(t5, lhs_exponent, HeapNumber::kNonMantissaBitsInTopWord);
  __ Branch(&one_is_nan, ne, t5, Operand(zero_reg));

  __ Branch(&one_is_nan, ne, lhs_mantissa, Operand(zero_reg));

  __ li(exp_mask_reg, HeapNumber::kExponentMask);
  __ bind(&lhs_not_nan_exp_mask_is_loaded);
  __ and_(t5, rhs_exponent, exp_mask_reg);

  __ Branch(&neither_is_nan, ne, t5, Operand(exp_mask_reg));

  __ sll(t5, rhs_exponent, HeapNumber::kNonMantissaBitsInTopWord);
  __ Branch(&one_is_nan, ne, t5, Operand(zero_reg));

  __ Branch(&neither_is_nan, eq, rhs_mantissa, Operand(zero_reg));

  __ bind(&one_is_nan);
  // NaN comparisons always fail.
  // Load whatever we need in v0 to make the comparison fail.
  if (cc == lt || cc == le) {
    __ li(v0, Operand(GREATER));
  } else {
    __ li(v0, Operand(LESS));
  }
  __ Ret();  // Return.

  __ bind(&neither_is_nan);
}


static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm, Condition cc) {
  // f12 and f14 have the two doubles.  Neither is a NaN.
  // Call a native function to do a comparison between two non-NaNs.
  // Call C routine that may not cause GC or other trouble.
  // We use a call_was and return manually because we need arguments slots to
  // be freed.

  Label return_result_not_equal, return_result_equal;
  if (cc == eq) {
    // Doubles are not equal unless they have the same bit pattern.
    // Exception: 0 and -0.
    bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
    if (CpuFeatures::IsSupported(FPU)) {
      CpuFeatures::Scope scope(FPU);
      // Lhs and rhs are already loaded to f12 and f14 register pairs.
      __ Move(t0, t1, f14);
      __ Move(t2, t3, f12);
    } else {
      // Lhs and rhs are already loaded to GP registers.
      __ mov(t0, a0);  // a0 has LS 32 bits of rhs.
      __ mov(t1, a1);  // a1 has MS 32 bits of rhs.
      __ mov(t2, a2);  // a2 has LS 32 bits of lhs.
      __ mov(t3, a3);  // a3 has MS 32 bits of lhs.
    }
    Register rhs_exponent = exp_first ? t0 : t1;
    Register lhs_exponent = exp_first ? t2 : t3;
    Register rhs_mantissa = exp_first ? t1 : t0;
    Register lhs_mantissa = exp_first ? t3 : t2;

    __ xor_(v0, rhs_mantissa, lhs_mantissa);
    __ Branch(&return_result_not_equal, ne, v0, Operand(zero_reg));

    __ subu(v0, rhs_exponent, lhs_exponent);
    __ Branch(&return_result_equal, eq, v0, Operand(zero_reg));
    // 0, -0 case.
    __ sll(rhs_exponent, rhs_exponent, kSmiTagSize);
    __ sll(lhs_exponent, lhs_exponent, kSmiTagSize);
    __ or_(t4, rhs_exponent, lhs_exponent);
    __ or_(t4, t4, rhs_mantissa);

    __ Branch(&return_result_not_equal, ne, t4, Operand(zero_reg));

    __ bind(&return_result_equal);
    __ li(v0, Operand(EQUAL));
    __ Ret();
  }

  __ bind(&return_result_not_equal);

  if (!CpuFeatures::IsSupported(FPU)) {
    __ push(ra);
    __ PrepareCallCFunction(4, t4);  // Two doubles count as 4 arguments.
    if (!IsMipsSoftFloatABI) {
      // We are not using MIPS FPU instructions, and parameters for the runtime
      // function call are prepaired in a0-a3 registers, but function we are
      // calling is compiled with hard-float flag and expecting hard float ABI
      // (parameters in f12/f14 registers). We need to copy parameters from
      // a0-a3 registers to f12/f14 register pairs.
      __ Move(f12, a0, a1);
      __ Move(f14, a2, a3);
    }
    __ CallCFunction(ExternalReference::compare_doubles(masm->isolate()), 4);
    __ pop(ra);  // Because this function returns int, result is in v0.
    __ Ret();
  } else {
    CpuFeatures::Scope scope(FPU);
    Label equal, less_than;
    __ c(EQ, D, f12, f14);
    __ bc1t(&equal);
    __ nop();

    __ c(OLT, D, f12, f14);
    __ bc1t(&less_than);
    __ nop();

    // Not equal, not less, not NaN, must be greater.
    __ li(v0, Operand(GREATER));
    __ Ret();

    __ bind(&equal);
    __ li(v0, Operand(EQUAL));
    __ Ret();

    __ bind(&less_than);
    __ li(v0, Operand(LESS));
    __ Ret();
  }
}


static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs) {
    // If either operand is a JS object or an oddball value, then they are
    // not equal since their pointers are different.
    // There is no test for undetectability in strict equality.
    STATIC_ASSERT(LAST_TYPE == LAST_CALLABLE_SPEC_OBJECT_TYPE);
    Label first_non_object;
    // Get the type of the first operand into a2 and compare it with
    // FIRST_SPEC_OBJECT_TYPE.
    __ GetObjectType(lhs, a2, a2);
    __ Branch(&first_non_object, less, a2, Operand(FIRST_SPEC_OBJECT_TYPE));

    // Return non-zero.
    Label return_not_equal;
    __ bind(&return_not_equal);
    __ li(v0, Operand(1));
    __ Ret();

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ Branch(&return_not_equal, eq, a2, Operand(ODDBALL_TYPE));

    __ GetObjectType(rhs, a3, a3);
    __ Branch(&return_not_equal, greater, a3, Operand(FIRST_SPEC_OBJECT_TYPE));

    // Check for oddballs: true, false, null, undefined.
    __ Branch(&return_not_equal, eq, a3, Operand(ODDBALL_TYPE));

    // Now that we have the types we might as well check for symbol-symbol.
    // Ensure that no non-strings have the symbol bit set.
    STATIC_ASSERT(LAST_TYPE < kNotStringTag + kIsSymbolMask);
    STATIC_ASSERT(kSymbolTag != 0);
    __ And(t2, a2, Operand(a3));
    __ And(t0, t2, Operand(kIsSymbolMask));
    __ Branch(&return_not_equal, ne, t0, Operand(zero_reg));
}


static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm,
                                       Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers,
                                       Label* slow) {
  __ GetObjectType(lhs, a3, a2);
  __ Branch(not_heap_numbers, ne, a2, Operand(HEAP_NUMBER_TYPE));
  __ lw(a2, FieldMemOperand(rhs, HeapObject::kMapOffset));
  // If first was a heap number & second wasn't, go to slow case.
  __ Branch(slow, ne, a3, Operand(a2));

  // Both are heap numbers. Load them up then jump to the code we have
  // for that.
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    __ ldc1(f12, FieldMemOperand(lhs, HeapNumber::kValueOffset));
    __ ldc1(f14, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  } else {
    __ lw(a2, FieldMemOperand(lhs, HeapNumber::kValueOffset));
    __ lw(a3, FieldMemOperand(lhs, HeapNumber::kValueOffset + 4));
    if (rhs.is(a0)) {
      __ lw(a1, FieldMemOperand(rhs, HeapNumber::kValueOffset + 4));
      __ lw(a0, FieldMemOperand(rhs, HeapNumber::kValueOffset));
    } else {
      __ lw(a0, FieldMemOperand(rhs, HeapNumber::kValueOffset));
      __ lw(a1, FieldMemOperand(rhs, HeapNumber::kValueOffset + 4));
    }
  }
  __ jmp(both_loaded_as_doubles);
}


// Fast negative check for symbol-to-symbol equality.
static void EmitCheckForSymbolsOrObjects(MacroAssembler* masm,
                                         Register lhs,
                                         Register rhs,
                                         Label* possible_strings,
                                         Label* not_both_strings) {
  ASSERT((lhs.is(a0) && rhs.is(a1)) ||
         (lhs.is(a1) && rhs.is(a0)));

  // a2 is object type of lhs.
  // Ensure that no non-strings have the symbol bit set.
  Label object_test;
  STATIC_ASSERT(kSymbolTag != 0);
  __ And(at, a2, Operand(kIsNotStringMask));
  __ Branch(&object_test, ne, at, Operand(zero_reg));
  __ And(at, a2, Operand(kIsSymbolMask));
  __ Branch(possible_strings, eq, at, Operand(zero_reg));
  __ GetObjectType(rhs, a3, a3);
  __ Branch(not_both_strings, ge, a3, Operand(FIRST_NONSTRING_TYPE));
  __ And(at, a3, Operand(kIsSymbolMask));
  __ Branch(possible_strings, eq, at, Operand(zero_reg));

  // Both are symbols. We already checked they weren't the same pointer
  // so they are not equal.
  __ li(v0, Operand(1));   // Non-zero indicates not equal.
  __ Ret();

  __ bind(&object_test);
  __ Branch(not_both_strings, lt, a2, Operand(FIRST_SPEC_OBJECT_TYPE));
  __ GetObjectType(rhs, a2, a3);
  __ Branch(not_both_strings, lt, a3, Operand(FIRST_SPEC_OBJECT_TYPE));

  // If both objects are undetectable, they are equal.  Otherwise, they
  // are not equal, since they are different objects and an object is not
  // equal to undefined.
  __ lw(a3, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ lbu(a2, FieldMemOperand(a2, Map::kBitFieldOffset));
  __ lbu(a3, FieldMemOperand(a3, Map::kBitFieldOffset));
  __ and_(a0, a2, a3);
  __ And(a0, a0, Operand(1 << Map::kIsUndetectable));
  __ Xor(v0, a0, Operand(1 << Map::kIsUndetectable));
  __ Ret();
}


void NumberToStringStub::GenerateLookupNumberStringCache(MacroAssembler* masm,
                                                         Register object,
                                                         Register result,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Register scratch3,
                                                         bool object_is_smi,
                                                         Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch3;

  // Load the number string cache.
  __ LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  __ lw(mask, FieldMemOperand(number_string_cache, FixedArray::kLengthOffset));
  // Divide length by two (length is a smi).
  __ sra(mask, mask, kSmiTagSize + 1);
  __ Addu(mask, mask, -1);  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Isolate* isolate = masm->isolate();
  Label is_smi;
  Label load_result_from_cache;
  if (!object_is_smi) {
    __ JumpIfSmi(object, &is_smi);
    if (CpuFeatures::IsSupported(FPU)) {
      CpuFeatures::Scope scope(FPU);
      __ CheckMap(object,
                  scratch1,
                  Heap::kHeapNumberMapRootIndex,
                  not_found,
                  DONT_DO_SMI_CHECK);

      STATIC_ASSERT(8 == kDoubleSize);
      __ Addu(scratch1,
              object,
              Operand(HeapNumber::kValueOffset - kHeapObjectTag));
      __ lw(scratch2, MemOperand(scratch1, kPointerSize));
      __ lw(scratch1, MemOperand(scratch1, 0));
      __ Xor(scratch1, scratch1, Operand(scratch2));
      __ And(scratch1, scratch1, Operand(mask));

      // Calculate address of entry in string cache: each entry consists
      // of two pointer sized fields.
      __ sll(scratch1, scratch1, kPointerSizeLog2 + 1);
      __ Addu(scratch1, number_string_cache, scratch1);

      Register probe = mask;
      __ lw(probe,
             FieldMemOperand(scratch1, FixedArray::kHeaderSize));
      __ JumpIfSmi(probe, not_found);
      __ ldc1(f12, FieldMemOperand(object, HeapNumber::kValueOffset));
      __ ldc1(f14, FieldMemOperand(probe, HeapNumber::kValueOffset));
      __ c(EQ, D, f12, f14);
      __ bc1t(&load_result_from_cache);
      __ nop();   // bc1t() requires explicit fill of branch delay slot.
      __ Branch(not_found);
    } else {
      // Note that there is no cache check for non-FPU case, even though
      // it seems there could be. May be a tiny opimization for non-FPU
      // cores.
      __ Branch(not_found);
    }
  }

  __ bind(&is_smi);
  Register scratch = scratch1;
  __ sra(scratch, object, 1);   // Shift away the tag.
  __ And(scratch, mask, Operand(scratch));

  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  __ sll(scratch, scratch, kPointerSizeLog2 + 1);
  __ Addu(scratch, number_string_cache, scratch);

  // Check if the entry is the smi we are looking for.
  Register probe = mask;
  __ lw(probe, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  __ Branch(not_found, ne, object, Operand(probe));

  // Get the result from the cache.
  __ bind(&load_result_from_cache);
  __ lw(result,
         FieldMemOperand(scratch, FixedArray::kHeaderSize + kPointerSize));

  __ IncrementCounter(isolate->counters()->number_to_string_native(),
                      1,
                      scratch1,
                      scratch2);
}


void NumberToStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  __ lw(a1, MemOperand(sp, 0));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, a1, v0, a2, a3, t0, false, &runtime);
  __ Addu(sp, sp, Operand(1 * kPointerSize));
  __ Ret();

  __ bind(&runtime);
  // Handle number to string in the runtime system if not found in the cache.
  __ TailCallRuntime(Runtime::kNumberToString, 1, 1);
}


// On entry lhs_ (lhs) and rhs_ (rhs) are the things to be compared.
// On exit, v0 is 0, positive, or negative (smi) to indicate the result
// of the comparison.
void CompareStub::Generate(MacroAssembler* masm) {
  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles;


  if (include_smi_compare_) {
    Label not_two_smis, smi_done;
    __ Or(a2, a1, a0);
    __ JumpIfNotSmi(a2, &not_two_smis);
    __ sra(a1, a1, 1);
    __ sra(a0, a0, 1);
    __ Subu(v0, a1, a0);
    __ Ret();
    __ bind(&not_two_smis);
  } else if (FLAG_debug_code) {
    __ Or(a2, a1, a0);
    __ And(a2, a2, kSmiTagMask);
    __ Assert(ne, "CompareStub: unexpected smi operands.",
        a2, Operand(zero_reg));
  }


  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Handle the case where the objects are identical.  Either returns the answer
  // or goes to slow.  Only falls through if the objects were not identical.
  EmitIdenticalObjectComparison(masm, &slow, cc_, never_nan_nan_);

  // If either is a Smi (we know that not both are), then they can only
  // be strictly equal if the other is a HeapNumber.
  STATIC_ASSERT(kSmiTag == 0);
  ASSERT_EQ(0, Smi::FromInt(0));
  __ And(t2, lhs_, Operand(rhs_));
  __ JumpIfNotSmi(t2, &not_smis, t0);
  // One operand is a smi. EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to rhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison and the numbers have been loaded into f12 and f14 as doubles,
  // or in GP registers (a0, a1, a2, a3) depending on the presence of the FPU.
  EmitSmiNonsmiComparison(masm, lhs_, rhs_,
                          &both_loaded_as_doubles, &slow, strict_);

  __ bind(&both_loaded_as_doubles);
  // f12, f14 are the double representations of the left hand side
  // and the right hand side if we have FPU. Otherwise a2, a3 represent
  // left hand side and a0, a1 represent right hand side.

  Isolate* isolate = masm->isolate();
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    Label nan;
    __ li(t0, Operand(LESS));
    __ li(t1, Operand(GREATER));
    __ li(t2, Operand(EQUAL));

    // Check if either rhs or lhs is NaN.
    __ c(UN, D, f12, f14);
    __ bc1t(&nan);
    __ nop();

    // Check if LESS condition is satisfied. If true, move conditionally
    // result to v0.
    __ c(OLT, D, f12, f14);
    __ movt(v0, t0);
    // Use previous check to store conditionally to v0 oposite condition
    // (GREATER). If rhs is equal to lhs, this will be corrected in next
    // check.
    __ movf(v0, t1);
    // Check if EQUAL condition is satisfied. If true, move conditionally
    // result to v0.
    __ c(EQ, D, f12, f14);
    __ movt(v0, t2);

    __ Ret();

    __ bind(&nan);
    // NaN comparisons always fail.
    // Load whatever we need in v0 to make the comparison fail.
    if (cc_ == lt || cc_ == le) {
      __ li(v0, Operand(GREATER));
    } else {
      __ li(v0, Operand(LESS));
    }
    __ Ret();
  } else {
    // Checks for NaN in the doubles we have loaded.  Can return the answer or
    // fall through if neither is a NaN.  Also binds rhs_not_nan.
    EmitNanCheck(masm, cc_);

    // Compares two doubles that are not NaNs. Returns the answer.
    // Never falls through.
    EmitTwoNonNanDoubleComparison(masm, cc_);
  }

  __ bind(&not_smis);
  // At this point we know we are dealing with two different objects,
  // and neither of them is a Smi. The objects are in lhs_ and rhs_.
  if (strict_) {
    // This returns non-equal for some object types, or falls through if it
    // was not lucky.
    EmitStrictTwoHeapObjectCompare(masm, lhs_, rhs_);
  }

  Label check_for_symbols;
  Label flat_string_check;
  // Check for heap-number-heap-number comparison. Can jump to slow case,
  // or load both doubles and jump to the code that handles
  // that case. If the inputs are not doubles then jumps to check_for_symbols.
  // In this case a2 will contain the type of lhs_.
  EmitCheckForTwoHeapNumbers(masm,
                             lhs_,
                             rhs_,
                             &both_loaded_as_doubles,
                             &check_for_symbols,
                             &flat_string_check);

  __ bind(&check_for_symbols);
  if (cc_ == eq && !strict_) {
    // Returns an answer for two symbols or two detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that a2 is the type of lhs_ on entry.
    EmitCheckForSymbolsOrObjects(masm, lhs_, rhs_, &flat_string_check, &slow);
  }

  // Check for both being sequential ASCII strings, and inline if that is the
  // case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialAsciiStrings(lhs_, rhs_, a2, a3, &slow);

  __ IncrementCounter(isolate->counters()->string_compare_native(), 1, a2, a3);
  if (cc_ == eq) {
    StringCompareStub::GenerateFlatAsciiStringEquals(masm,
                                                     lhs_,
                                                     rhs_,
                                                     a2,
                                                     a3,
                                                     t0);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                       lhs_,
                                                       rhs_,
                                                       a2,
                                                       a3,
                                                       t0,
                                                       t1);
  }
  // Never falls through to here.

  __ bind(&slow);
  // Prepare for call to builtin. Push object pointers, a0 (lhs) first,
  // a1 (rhs) second.
  __ Push(lhs_, rhs_);
  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript native;
  if (cc_ == eq) {
    native = strict_ ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    native = Builtins::COMPARE;
    int ncr;  // NaN compare result.
    if (cc_ == lt || cc_ == le) {
      ncr = GREATER;
    } else {
      ASSERT(cc_ == gt || cc_ == ge);  // Remaining cases.
      ncr = LESS;
    }
    __ li(a0, Operand(Smi::FromInt(ncr)));
    __ push(a0);
  }

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(native, JUMP_FUNCTION);
}


// The stub returns zero for false, and a non-zero value for true.
void ToBooleanStub::Generate(MacroAssembler* masm) {
  // This stub uses FPU instructions.
  CpuFeatures::Scope scope(FPU);

  Label false_result;
  Label not_heap_number;
  Register scratch0 = t5.is(tos_) ? t3 : t5;

  // undefined -> false
  __ LoadRoot(scratch0, Heap::kUndefinedValueRootIndex);
  __ Branch(&false_result, eq, tos_, Operand(scratch0));

  // Boolean -> its value
  __ LoadRoot(scratch0, Heap::kFalseValueRootIndex);
  __ Branch(&false_result, eq, tos_, Operand(scratch0));
  __ LoadRoot(scratch0, Heap::kTrueValueRootIndex);
  // "tos_" is a register and contains a non-zero value.  Hence we implicitly
  // return true if the equal condition is satisfied.
  __ Ret(eq, tos_, Operand(scratch0));

  // Smis: 0 -> false, all other -> true
  __ And(scratch0, tos_, tos_);
  __ Branch(&false_result, eq, scratch0, Operand(zero_reg));
  __ And(scratch0, tos_, Operand(kSmiTagMask));
  // "tos_" is a register and contains a non-zero value.  Hence we implicitly
  // return true if the not equal condition is satisfied.
  __ Ret(eq, scratch0, Operand(zero_reg));

  // 'null' -> false
  __ LoadRoot(scratch0, Heap::kNullValueRootIndex);
  __ Branch(&false_result, eq, tos_, Operand(scratch0));

  // HeapNumber => false if +0, -0, or NaN.
  __ lw(scratch0, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ LoadRoot(at, Heap::kHeapNumberMapRootIndex);
  __ Branch(&not_heap_number, ne, scratch0, Operand(at));

  __ ldc1(f12, FieldMemOperand(tos_, HeapNumber::kValueOffset));
  __ fcmp(f12, 0.0, UEQ);

  // "tos_" is a register, and contains a non zero value by default.
  // Hence we only need to overwrite "tos_" with zero to return false for
  // FP_ZERO or FP_NAN cases. Otherwise, by default it returns true.
  __ movt(tos_, zero_reg);
  __ Ret();

  __ bind(&not_heap_number);

  // It can be an undetectable object.
  // Undetectable => false.
  __ lw(at, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ lbu(scratch0, FieldMemOperand(at, Map::kBitFieldOffset));
  __ And(scratch0, scratch0, Operand(1 << Map::kIsUndetectable));
  __ Branch(&false_result, eq, scratch0, Operand(1 << Map::kIsUndetectable));

  // JavaScript object => true.
  __ lw(scratch0, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ lbu(scratch0, FieldMemOperand(scratch0, Map::kInstanceTypeOffset));

  // "tos_" is a register and contains a non-zero value.
  // Hence we implicitly return true if the greater than
  // condition is satisfied.
  __ Ret(ge, scratch0, Operand(FIRST_SPEC_OBJECT_TYPE));

  // Check for string.
  __ lw(scratch0, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ lbu(scratch0, FieldMemOperand(scratch0, Map::kInstanceTypeOffset));
  // "tos_" is a register and contains a non-zero value.
  // Hence we implicitly return true if the greater than
  // condition is satisfied.
  __ Ret(ge, scratch0, Operand(FIRST_NONSTRING_TYPE));

  // String value => false iff empty, i.e., length is zero.
  __ lw(tos_, FieldMemOperand(tos_, String::kLengthOffset));
  // If length is zero, "tos_" contains zero ==> false.
  // If length is not zero, "tos_" contains a non-zero value ==> true.
  __ Ret();

  // Return 0 in "tos_" for false.
  __ bind(&false_result);
  __ mov(tos_, zero_reg);
  __ Ret();
}


void UnaryOpStub::PrintName(StringStream* stream) {
  const char* op_name = Token::Name(op_);
  const char* overwrite_name = NULL;  // Make g++ happy.
  switch (mode_) {
    case UNARY_NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case UNARY_OVERWRITE: overwrite_name = "Overwrite"; break;
  }
  stream->Add("UnaryOpStub_%s_%s_%s",
              op_name,
              overwrite_name,
              UnaryOpIC::GetName(operand_type_));
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::Generate(MacroAssembler* masm) {
  switch (operand_type_) {
    case UnaryOpIC::UNINITIALIZED:
      GenerateTypeTransition(masm);
      break;
    case UnaryOpIC::SMI:
      GenerateSmiStub(masm);
      break;
    case UnaryOpIC::HEAP_NUMBER:
      GenerateHeapNumberStub(masm);
      break;
    case UnaryOpIC::GENERIC:
      GenerateGenericStub(masm);
      break;
  }
}


void UnaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  // Argument is in a0 and v0 at this point, so we can overwrite a0.
  __ li(a2, Operand(Smi::FromInt(op_)));
  __ li(a1, Operand(Smi::FromInt(mode_)));
  __ li(a0, Operand(Smi::FromInt(operand_type_)));
  __ Push(v0, a2, a1, a0);

  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kUnaryOp_Patch), masm->isolate()), 4, 1);
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
  switch (op_) {
    case Token::SUB:
      GenerateSmiStubSub(masm);
      break;
    case Token::BIT_NOT:
      GenerateSmiStubBitNot(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void UnaryOpStub::GenerateSmiStubSub(MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeSub(masm, &non_smi, &slow);
  __ bind(&non_smi);
  __ bind(&slow);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateSmiStubBitNot(MacroAssembler* masm) {
  Label non_smi;
  GenerateSmiCodeBitNot(masm, &non_smi);
  __ bind(&non_smi);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateSmiCodeSub(MacroAssembler* masm,
                                     Label* non_smi,
                                     Label* slow) {
  __ JumpIfNotSmi(a0, non_smi);

  // The result of negating zero or the smallest negative smi is not a smi.
  __ And(t0, a0, ~0x80000000);
  __ Branch(slow, eq, t0, Operand(zero_reg));

  // Return '0 - value'.
  __ Subu(v0, zero_reg, a0);
  __ Ret();
}


void UnaryOpStub::GenerateSmiCodeBitNot(MacroAssembler* masm,
                                        Label* non_smi) {
  __ JumpIfNotSmi(a0, non_smi);

  // Flip bits and revert inverted smi-tag.
  __ Neg(v0, a0);
  __ And(v0, v0, ~kSmiTagMask);
  __ Ret();
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::GenerateHeapNumberStub(MacroAssembler* masm) {
  switch (op_) {
    case Token::SUB:
      GenerateHeapNumberStubSub(masm);
      break;
    case Token::BIT_NOT:
      GenerateHeapNumberStubBitNot(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void UnaryOpStub::GenerateHeapNumberStubSub(MacroAssembler* masm) {
  Label non_smi, slow, call_builtin;
  GenerateSmiCodeSub(masm, &non_smi, &call_builtin);
  __ bind(&non_smi);
  GenerateHeapNumberCodeSub(masm, &slow);
  __ bind(&slow);
  GenerateTypeTransition(masm);
  __ bind(&call_builtin);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateHeapNumberStubBitNot(MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeBitNot(masm, &non_smi);
  __ bind(&non_smi);
  GenerateHeapNumberCodeBitNot(masm, &slow);
  __ bind(&slow);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateHeapNumberCodeSub(MacroAssembler* masm,
                                            Label* slow) {
  EmitCheckForHeapNumber(masm, a0, a1, t2, slow);
  // a0 is a heap number.  Get a new heap number in a1.
  if (mode_ == UNARY_OVERWRITE) {
    __ lw(a2, FieldMemOperand(a0, HeapNumber::kExponentOffset));
    __ Xor(a2, a2, Operand(HeapNumber::kSignMask));  // Flip sign.
    __ sw(a2, FieldMemOperand(a0, HeapNumber::kExponentOffset));
  } else {
    Label slow_allocate_heapnumber, heapnumber_allocated;
    __ AllocateHeapNumber(a1, a2, a3, t2, &slow_allocate_heapnumber);
    __ jmp(&heapnumber_allocated);

    __ bind(&slow_allocate_heapnumber);
    __ EnterInternalFrame();
    __ push(a0);
    __ CallRuntime(Runtime::kNumberAlloc, 0);
    __ mov(a1, v0);
    __ pop(a0);
    __ LeaveInternalFrame();

    __ bind(&heapnumber_allocated);
    __ lw(a3, FieldMemOperand(a0, HeapNumber::kMantissaOffset));
    __ lw(a2, FieldMemOperand(a0, HeapNumber::kExponentOffset));
    __ sw(a3, FieldMemOperand(a1, HeapNumber::kMantissaOffset));
    __ Xor(a2, a2, Operand(HeapNumber::kSignMask));  // Flip sign.
    __ sw(a2, FieldMemOperand(a1, HeapNumber::kExponentOffset));
    __ mov(v0, a1);
  }
  __ Ret();
}


void UnaryOpStub::GenerateHeapNumberCodeBitNot(
    MacroAssembler* masm,
    Label* slow) {
  Label impossible;

  EmitCheckForHeapNumber(masm, a0, a1, t2, slow);
  // Convert the heap number in a0 to an untagged integer in a1.
  __ ConvertToInt32(a0, a1, a2, a3, f0, slow);

  // Do the bitwise operation and check if the result fits in a smi.
  Label try_float;
  __ Neg(a1, a1);
  __ Addu(a2, a1, Operand(0x40000000));
  __ Branch(&try_float, lt, a2, Operand(zero_reg));

  // Tag the result as a smi and we're done.
  __ SmiTag(v0, a1);
  __ Ret();

  // Try to store the result in a heap number.
  __ bind(&try_float);
  if (mode_ == UNARY_NO_OVERWRITE) {
    Label slow_allocate_heapnumber, heapnumber_allocated;
    // Allocate a new heap number without zapping v0, which we need if it fails.
    __ AllocateHeapNumber(a2, a3, t0, t2, &slow_allocate_heapnumber);
    __ jmp(&heapnumber_allocated);

    __ bind(&slow_allocate_heapnumber);
    __ EnterInternalFrame();
    __ push(v0);  // Push the heap number, not the untagged int32.
    __ CallRuntime(Runtime::kNumberAlloc, 0);
    __ mov(a2, v0);  // Move the new heap number into a2.
    // Get the heap number into v0, now that the new heap number is in a2.
    __ pop(v0);
    __ LeaveInternalFrame();

    // Convert the heap number in v0 to an untagged integer in a1.
    // This can't go slow-case because it's the same number we already
    // converted once again.
    __ ConvertToInt32(v0, a1, a3, t0, f0, &impossible);
    // Negate the result.
    __ Xor(a1, a1, -1);

    __ bind(&heapnumber_allocated);
    __ mov(v0, a2);  // Move newly allocated heap number to v0.
  }

  if (CpuFeatures::IsSupported(FPU)) {
    // Convert the int32 in a1 to the heap number in v0. a2 is corrupted.
    CpuFeatures::Scope scope(FPU);
    __ mtc1(a1, f0);
    __ cvt_d_w(f0, f0);
    __ sdc1(f0, FieldMemOperand(v0, HeapNumber::kValueOffset));
    __ Ret();
  } else {
    // WriteInt32ToHeapNumberStub does not trigger GC, so we do not
    // have to set up a frame.
    WriteInt32ToHeapNumberStub stub(a1, v0, a2, a3);
    __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
  }

  __ bind(&impossible);
  if (FLAG_debug_code) {
    __ stop("Incorrect assumption in bit-not stub");
  }
}


// TODO(svenpanne): Use virtual functions instead of switch.
void UnaryOpStub::GenerateGenericStub(MacroAssembler* masm) {
  switch (op_) {
    case Token::SUB:
      GenerateGenericStubSub(masm);
      break;
    case Token::BIT_NOT:
      GenerateGenericStubBitNot(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void UnaryOpStub::GenerateGenericStubSub(MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeSub(masm, &non_smi, &slow);
  __ bind(&non_smi);
  GenerateHeapNumberCodeSub(masm, &slow);
  __ bind(&slow);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateGenericStubBitNot(MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeBitNot(masm, &non_smi);
  __ bind(&non_smi);
  GenerateHeapNumberCodeBitNot(masm, &slow);
  __ bind(&slow);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateGenericCodeFallback(
    MacroAssembler* masm) {
  // Handle the slow case by jumping to the JavaScript builtin.
  __ push(a0);
  switch (op_) {
    case Token::SUB:
      __ InvokeBuiltin(Builtins::UNARY_MINUS, JUMP_FUNCTION);
      break;
    case Token::BIT_NOT:
      __ InvokeBuiltin(Builtins::BIT_NOT, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


void BinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  Label get_result;

  __ Push(a1, a0);

  __ li(a2, Operand(Smi::FromInt(MinorKey())));
  __ li(a1, Operand(Smi::FromInt(op_)));
  __ li(a0, Operand(Smi::FromInt(operands_type_)));
  __ Push(a2, a1, a0);

  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch),
                        masm->isolate()),
      5,
      1);
}


void BinaryOpStub::GenerateTypeTransitionWithSavedArgs(
    MacroAssembler* masm) {
  UNIMPLEMENTED();
}


void BinaryOpStub::Generate(MacroAssembler* masm) {
  switch (operands_type_) {
    case BinaryOpIC::UNINITIALIZED:
      GenerateTypeTransition(masm);
      break;
    case BinaryOpIC::SMI:
      GenerateSmiStub(masm);
      break;
    case BinaryOpIC::INT32:
      GenerateInt32Stub(masm);
      break;
    case BinaryOpIC::HEAP_NUMBER:
      GenerateHeapNumberStub(masm);
      break;
    case BinaryOpIC::ODDBALL:
      GenerateOddballStub(masm);
      break;
    case BinaryOpIC::BOTH_STRING:
      GenerateBothStringStub(masm);
      break;
    case BinaryOpIC::STRING:
      GenerateStringStub(masm);
      break;
    case BinaryOpIC::GENERIC:
      GenerateGeneric(masm);
      break;
    default:
      UNREACHABLE();
  }
}


void BinaryOpStub::PrintName(StringStream* stream) {
  const char* op_name = Token::Name(op_);
  const char* overwrite_name;
  switch (mode_) {
    case NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case OVERWRITE_RIGHT: overwrite_name = "OverwriteRight"; break;
    case OVERWRITE_LEFT: overwrite_name = "OverwriteLeft"; break;
    default: overwrite_name = "UnknownOverwrite"; break;
  }
  stream->Add("BinaryOpStub_%s_%s_%s",
              op_name,
              overwrite_name,
              BinaryOpIC::GetName(operands_type_));
}



void BinaryOpStub::GenerateSmiSmiOperation(MacroAssembler* masm) {
  Register left = a1;
  Register right = a0;

  Register scratch1 = t0;
  Register scratch2 = t1;

  ASSERT(right.is(a0));
  STATIC_ASSERT(kSmiTag == 0);

  Label not_smi_result;
  switch (op_) {
    case Token::ADD:
      __ AdduAndCheckForOverflow(v0, left, right, scratch1);
      __ RetOnNoOverflow(scratch1);
      // No need to revert anything - right and left are intact.
      break;
    case Token::SUB:
      __ SubuAndCheckForOverflow(v0, left, right, scratch1);
      __ RetOnNoOverflow(scratch1);
      // No need to revert anything - right and left are intact.
      break;
    case Token::MUL: {
      // Remove tag from one of the operands. This way the multiplication result
      // will be a smi if it fits the smi range.
      __ SmiUntag(scratch1, right);
      // Do multiplication.
      // lo = lower 32 bits of scratch1 * left.
      // hi = higher 32 bits of scratch1 * left.
      __ Mult(left, scratch1);
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ mflo(scratch1);
      __ mfhi(scratch2);
      __ sra(scratch1, scratch1, 31);
      __ Branch(&not_smi_result, ne, scratch1, Operand(scratch2));
      // Go slow on zero result to handle -0.
      __ mflo(v0);
      __ Ret(ne, v0, Operand(zero_reg));
      // We need -0 if we were multiplying a negative number with 0 to get 0.
      // We know one of them was zero.
      __ Addu(scratch2, right, left);
      Label skip;
      // ARM uses the 'pl' condition, which is 'ge'.
      // Negating it results in 'lt'.
      __ Branch(&skip, lt, scratch2, Operand(zero_reg));
      ASSERT(Smi::FromInt(0) == 0);
      __ mov(v0, zero_reg);
      __ Ret();  // Return smi 0 if the non-zero one was positive.
      __ bind(&skip);
      // We fall through here if we multiplied a negative number with 0, because
      // that would mean we should produce -0.
      }
      break;
    case Token::DIV: {
      Label done;
      __ SmiUntag(scratch2, right);
      __ SmiUntag(scratch1, left);
      __ Div(scratch1, scratch2);
      // A minor optimization: div may be calculated asynchronously, so we check
      // for division by zero before getting the result.
      __ Branch(&not_smi_result, eq, scratch2, Operand(zero_reg));
      // If the result is 0, we need to make sure the dividsor (right) is
      // positive, otherwise it is a -0 case.
      // Quotient is in 'lo', remainder is in 'hi'.
      // Check for no remainder first.
      __ mfhi(scratch1);
      __ Branch(&not_smi_result, ne, scratch1, Operand(zero_reg));
      __ mflo(scratch1);
      __ Branch(&done, ne, scratch1, Operand(zero_reg));
      __ Branch(&not_smi_result, lt, scratch2, Operand(zero_reg));
      __ bind(&done);
      // Check that the signed result fits in a Smi.
      __ Addu(scratch2, scratch1, Operand(0x40000000));
      __ Branch(&not_smi_result, lt, scratch2, Operand(zero_reg));
      __ SmiTag(v0, scratch1);
      __ Ret();
      }
      break;
    case Token::MOD: {
      Label done;
      __ SmiUntag(scratch2, right);
      __ SmiUntag(scratch1, left);
      __ Div(scratch1, scratch2);
      // A minor optimization: div may be calculated asynchronously, so we check
      // for division by 0 before calling mfhi.
      // Check for zero on the right hand side.
      __ Branch(&not_smi_result, eq, scratch2, Operand(zero_reg));
      // If the result is 0, we need to make sure the dividend (left) is
      // positive (or 0), otherwise it is a -0 case.
      // Remainder is in 'hi'.
      __ mfhi(scratch2);
      __ Branch(&done, ne, scratch2, Operand(zero_reg));
      __ Branch(&not_smi_result, lt, scratch1, Operand(zero_reg));
      __ bind(&done);
      // Check that the signed result fits in a Smi.
      __ Addu(scratch1, scratch2, Operand(0x40000000));
      __ Branch(&not_smi_result, lt, scratch1, Operand(zero_reg));
      __ SmiTag(v0, scratch2);
      __ Ret();
      }
      break;
    case Token::BIT_OR:
      __ Or(v0, left, Operand(right));
      __ Ret();
      break;
    case Token::BIT_AND:
      __ And(v0, left, Operand(right));
      __ Ret();
      break;
    case Token::BIT_XOR:
      __ Xor(v0, left, Operand(right));
      __ Ret();
      break;
    case Token::SAR:
      // Remove tags from right operand.
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ srav(scratch1, left, scratch1);
      // Smi tag result.
      __ And(v0, scratch1, Operand(~kSmiTagMask));
      __ Ret();
      break;
    case Token::SHR:
      // Remove tags from operands. We can't do this on a 31 bit number
      // because then the 0s get shifted into bit 30 instead of bit 31.
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ srlv(v0, scratch1, scratch2);
      // Unsigned shift is not allowed to produce a negative number, so
      // check the sign bit and the sign bit after Smi tagging.
      __ And(scratch1, v0, Operand(0xc0000000));
      __ Branch(&not_smi_result, ne, scratch1, Operand(zero_reg));
      // Smi tag result.
      __ SmiTag(v0);
      __ Ret();
      break;
    case Token::SHL:
      // Remove tags from operands.
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ sllv(scratch1, scratch1, scratch2);
      // Check that the signed result fits in a Smi.
      __ Addu(scratch2, scratch1, Operand(0x40000000));
      __ Branch(&not_smi_result, lt, scratch2, Operand(zero_reg));
      __ SmiTag(v0, scratch1);
      __ Ret();
      break;
    default:
      UNREACHABLE();
  }
  __ bind(&not_smi_result);
}


void BinaryOpStub::GenerateFPOperation(MacroAssembler* masm,
                                       bool smi_operands,
                                       Label* not_numbers,
                                       Label* gc_required) {
  Register left = a1;
  Register right = a0;
  Register scratch1 = t3;
  Register scratch2 = t5;
  Register scratch3 = t0;

  ASSERT(smi_operands || (not_numbers != NULL));
  if (smi_operands && FLAG_debug_code) {
    __ AbortIfNotSmi(left);
    __ AbortIfNotSmi(right);
  }

  Register heap_number_map = t2;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD: {
      // Load left and right operands into f12 and f14 or a0/a1 and a2/a3
      // depending on whether FPU is available or not.
      FloatingPointHelper::Destination destination =
          CpuFeatures::IsSupported(FPU) &&
          op_ != Token::MOD ?
              FloatingPointHelper::kFPURegisters :
              FloatingPointHelper::kCoreRegisters;

      // Allocate new heap number for result.
      Register result = s0;
      GenerateHeapResultAllocation(
          masm, result, heap_number_map, scratch1, scratch2, gc_required);

      // Load the operands.
      if (smi_operands) {
        FloatingPointHelper::LoadSmis(masm, destination, scratch1, scratch2);
      } else {
        FloatingPointHelper::LoadOperands(masm,
                                          destination,
                                          heap_number_map,
                                          scratch1,
                                          scratch2,
                                          not_numbers);
      }

      // Calculate the result.
      if (destination == FloatingPointHelper::kFPURegisters) {
        // Using FPU registers:
        // f12: Left value.
        // f14: Right value.
        CpuFeatures::Scope scope(FPU);
        switch (op_) {
        case Token::ADD:
          __ add_d(f10, f12, f14);
          break;
        case Token::SUB:
          __ sub_d(f10, f12, f14);
          break;
        case Token::MUL:
          __ mul_d(f10, f12, f14);
          break;
        case Token::DIV:
          __ div_d(f10, f12, f14);
          break;
        default:
          UNREACHABLE();
        }

        // ARM uses a workaround here because of the unaligned HeapNumber
        // kValueOffset. On MIPS this workaround is built into sdc1 so
        // there's no point in generating even more instructions.
        __ sdc1(f10, FieldMemOperand(result, HeapNumber::kValueOffset));
        __ mov(v0, result);
        __ Ret();
      } else {
        // Call the C function to handle the double operation.
        FloatingPointHelper::CallCCodeForDoubleOperation(masm,
                                                         op_,
                                                         result,
                                                         scratch1);
        if (FLAG_debug_code) {
          __ stop("Unreachable code.");
        }
      }
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::SAR:
    case Token::SHR:
    case Token::SHL: {
      if (smi_operands) {
        __ SmiUntag(a3, left);
        __ SmiUntag(a2, right);
      } else {
        // Convert operands to 32-bit integers. Right in a2 and left in a3.
        FloatingPointHelper::ConvertNumberToInt32(masm,
                                                  left,
                                                  a3,
                                                  heap_number_map,
                                                  scratch1,
                                                  scratch2,
                                                  scratch3,
                                                  f0,
                                                  not_numbers);
        FloatingPointHelper::ConvertNumberToInt32(masm,
                                                  right,
                                                  a2,
                                                  heap_number_map,
                                                  scratch1,
                                                  scratch2,
                                                  scratch3,
                                                  f0,
                                                  not_numbers);
      }
      Label result_not_a_smi;
      switch (op_) {
        case Token::BIT_OR:
          __ Or(a2, a3, Operand(a2));
          break;
        case Token::BIT_XOR:
          __ Xor(a2, a3, Operand(a2));
          break;
        case Token::BIT_AND:
          __ And(a2, a3, Operand(a2));
          break;
        case Token::SAR:
          // Use only the 5 least significant bits of the shift count.
          __ GetLeastBitsFromInt32(a2, a2, 5);
          __ srav(a2, a3, a2);
          break;
        case Token::SHR:
          // Use only the 5 least significant bits of the shift count.
          __ GetLeastBitsFromInt32(a2, a2, 5);
          __ srlv(a2, a3, a2);
          // SHR is special because it is required to produce a positive answer.
          // The code below for writing into heap numbers isn't capable of
          // writing the register as an unsigned int so we go to slow case if we
          // hit this case.
          if (CpuFeatures::IsSupported(FPU)) {
            __ Branch(&result_not_a_smi, lt, a2, Operand(zero_reg));
          } else {
            __ Branch(not_numbers, lt, a2, Operand(zero_reg));
          }
          break;
        case Token::SHL:
          // Use only the 5 least significant bits of the shift count.
          __ GetLeastBitsFromInt32(a2, a2, 5);
          __ sllv(a2, a3, a2);
          break;
        default:
          UNREACHABLE();
      }
      // Check that the *signed* result fits in a smi.
      __ Addu(a3, a2, Operand(0x40000000));
      __ Branch(&result_not_a_smi, lt, a3, Operand(zero_reg));
      __ SmiTag(v0, a2);
      __ Ret();

      // Allocate new heap number for result.
      __ bind(&result_not_a_smi);
      Register result = t1;
      if (smi_operands) {
        __ AllocateHeapNumber(
            result, scratch1, scratch2, heap_number_map, gc_required);
      } else {
        GenerateHeapResultAllocation(
            masm, result, heap_number_map, scratch1, scratch2, gc_required);
      }

      // a2: Answer as signed int32.
      // t1: Heap number to write answer into.

      // Nothing can go wrong now, so move the heap number to v0, which is the
      // result.
      __ mov(v0, t1);

      if (CpuFeatures::IsSupported(FPU)) {
        // Convert the int32 in a2 to the heap number in a0. As
        // mentioned above SHR needs to always produce a positive result.
        CpuFeatures::Scope scope(FPU);
        __ mtc1(a2, f0);
        if (op_ == Token::SHR) {
          __ Cvt_d_uw(f0, f0, f22);
        } else {
          __ cvt_d_w(f0, f0);
        }
        // ARM uses a workaround here because of the unaligned HeapNumber
        // kValueOffset. On MIPS this workaround is built into sdc1 so
        // there's no point in generating even more instructions.
        __ sdc1(f0, FieldMemOperand(v0, HeapNumber::kValueOffset));
        __ Ret();
      } else {
        // Tail call that writes the int32 in a2 to the heap number in v0, using
        // a3 and a0 as scratch. v0 is preserved and returned.
        WriteInt32ToHeapNumberStub stub(a2, v0, a3, a0);
        __ TailCallStub(&stub);
      }
      break;
    }
    default:
      UNREACHABLE();
  }
}


// Generate the smi code. If the operation on smis are successful this return is
// generated. If the result is not a smi and heap number allocation is not
// requested the code falls through. If number allocation is requested but a
// heap number cannot be allocated the code jumps to the lable gc_required.
void BinaryOpStub::GenerateSmiCode(
    MacroAssembler* masm,
    Label* use_runtime,
    Label* gc_required,
    SmiCodeGenerateHeapNumberResults allow_heapnumber_results) {
  Label not_smis;

  Register left = a1;
  Register right = a0;
  Register scratch1 = t3;
  Register scratch2 = t5;

  // Perform combined smi check on both operands.
  __ Or(scratch1, left, Operand(right));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(scratch1, &not_smis);

  // If the smi-smi operation results in a smi return is generated.
  GenerateSmiSmiOperation(masm);

  // If heap number results are possible generate the result in an allocated
  // heap number.
  if (allow_heapnumber_results == ALLOW_HEAPNUMBER_RESULTS) {
    GenerateFPOperation(masm, true, use_runtime, gc_required);
  }
  __ bind(&not_smis);
}


void BinaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
  Label not_smis, call_runtime;

  if (result_type_ == BinaryOpIC::UNINITIALIZED ||
      result_type_ == BinaryOpIC::SMI) {
    // Only allow smi results.
    GenerateSmiCode(masm, &call_runtime, NULL, NO_HEAPNUMBER_RESULTS);
  } else {
    // Allow heap number result and don't make a transition if a heap number
    // cannot be allocated.
    GenerateSmiCode(masm,
                    &call_runtime,
                    &call_runtime,
                    ALLOW_HEAPNUMBER_RESULTS);
  }

  // Code falls through if the result is not returned as either a smi or heap
  // number.
  GenerateTypeTransition(masm);

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void BinaryOpStub::GenerateStringStub(MacroAssembler* masm) {
  ASSERT(operands_type_ == BinaryOpIC::STRING);
  // Try to add arguments as strings, otherwise, transition to the generic
  // BinaryOpIC type.
  GenerateAddStrings(masm);
  GenerateTypeTransition(masm);
}


void BinaryOpStub::GenerateBothStringStub(MacroAssembler* masm) {
  Label call_runtime;
  ASSERT(operands_type_ == BinaryOpIC::BOTH_STRING);
  ASSERT(op_ == Token::ADD);
  // If both arguments are strings, call the string add stub.
  // Otherwise, do a transition.

  // Registers containing left and right operands respectively.
  Register left = a1;
  Register right = a0;

  // Test if left operand is a string.
  __ JumpIfSmi(left, &call_runtime);
  __ GetObjectType(left, a2, a2);
  __ Branch(&call_runtime, ge, a2, Operand(FIRST_NONSTRING_TYPE));

  // Test if right operand is a string.
  __ JumpIfSmi(right, &call_runtime);
  __ GetObjectType(right, a2, a2);
  __ Branch(&call_runtime, ge, a2, Operand(FIRST_NONSTRING_TYPE));

  StringAddStub string_add_stub(NO_STRING_CHECK_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_stub);

  __ bind(&call_runtime);
  GenerateTypeTransition(masm);
}


void BinaryOpStub::GenerateInt32Stub(MacroAssembler* masm) {
  ASSERT(operands_type_ == BinaryOpIC::INT32);

  Register left = a1;
  Register right = a0;
  Register scratch1 = t3;
  Register scratch2 = t5;
  FPURegister double_scratch = f0;
  FPURegister single_scratch = f6;

  Register heap_number_result = no_reg;
  Register heap_number_map = t2;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  Label call_runtime;
  // Labels for type transition, used for wrong input or output types.
  // Both label are currently actually bound to the same position. We use two
  // different label to differentiate the cause leading to type transition.
  Label transition;

  // Smi-smi fast case.
  Label skip;
  __ Or(scratch1, left, right);
  __ JumpIfNotSmi(scratch1, &skip);
  GenerateSmiSmiOperation(masm);
  // Fall through if the result is not a smi.
  __ bind(&skip);

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD: {
      // Load both operands and check that they are 32-bit integer.
      // Jump to type transition if they are not. The registers a0 and a1 (right
      // and left) are preserved for the runtime call.
      FloatingPointHelper::Destination destination =
          (CpuFeatures::IsSupported(FPU) && op_ != Token::MOD)
              ? FloatingPointHelper::kFPURegisters
              : FloatingPointHelper::kCoreRegisters;

      FloatingPointHelper::LoadNumberAsInt32Double(masm,
                                                   right,
                                                   destination,
                                                   f14,
                                                   a2,
                                                   a3,
                                                   heap_number_map,
                                                   scratch1,
                                                   scratch2,
                                                   f2,
                                                   &transition);
      FloatingPointHelper::LoadNumberAsInt32Double(masm,
                                                   left,
                                                   destination,
                                                   f12,
                                                   t0,
                                                   t1,
                                                   heap_number_map,
                                                   scratch1,
                                                   scratch2,
                                                   f2,
                                                   &transition);

      if (destination == FloatingPointHelper::kFPURegisters) {
        CpuFeatures::Scope scope(FPU);
        Label return_heap_number;
        switch (op_) {
          case Token::ADD:
            __ add_d(f10, f12, f14);
            break;
          case Token::SUB:
            __ sub_d(f10, f12, f14);
            break;
          case Token::MUL:
            __ mul_d(f10, f12, f14);
            break;
          case Token::DIV:
            __ div_d(f10, f12, f14);
            break;
          default:
            UNREACHABLE();
        }

        if (op_ != Token::DIV) {
          // These operations produce an integer result.
          // Try to return a smi if we can.
          // Otherwise return a heap number if allowed, or jump to type
          // transition.

          // NOTE: ARM uses a MacroAssembler function here (EmitVFPTruncate).
          // On MIPS a lot of things cannot be implemented the same way so right
          // now it makes a lot more sense to just do things manually.

          // Save FCSR.
          __ cfc1(scratch1, FCSR);
          // Disable FPU exceptions.
          __ ctc1(zero_reg, FCSR);
          __ trunc_w_d(single_scratch, f10);
          // Retrieve FCSR.
          __ cfc1(scratch2, FCSR);
          // Restore FCSR.
          __ ctc1(scratch1, FCSR);

          // Check for inexact conversion or exception.
          __ And(scratch2, scratch2, kFCSRFlagMask);

          if (result_type_ <= BinaryOpIC::INT32) {
            // If scratch2 != 0, result does not fit in a 32-bit integer.
            __ Branch(&transition, ne, scratch2, Operand(zero_reg));
          }

          // Check if the result fits in a smi.
          __ mfc1(scratch1, single_scratch);
          __ Addu(scratch2, scratch1, Operand(0x40000000));
          // If not try to return a heap number.
          __ Branch(&return_heap_number, lt, scratch2, Operand(zero_reg));
          // Check for minus zero. Return heap number for minus zero.
          Label not_zero;
          __ Branch(&not_zero, ne, scratch1, Operand(zero_reg));
          __ mfc1(scratch2, f11);
          __ And(scratch2, scratch2, HeapNumber::kSignMask);
          __ Branch(&return_heap_number, ne, scratch2, Operand(zero_reg));
          __ bind(&not_zero);

          // Tag the result and return.
          __ SmiTag(v0, scratch1);
          __ Ret();
        } else {
          // DIV just falls through to allocating a heap number.
        }

        __ bind(&return_heap_number);
        // Return a heap number, or fall through to type transition or runtime
        // call if we can't.
        if (result_type_ >= ((op_ == Token::DIV) ? BinaryOpIC::HEAP_NUMBER
                                                 : BinaryOpIC::INT32)) {
          // We are using FPU registers so s0 is available.
          heap_number_result = s0;
          GenerateHeapResultAllocation(masm,
                                       heap_number_result,
                                       heap_number_map,
                                       scratch1,
                                       scratch2,
                                       &call_runtime);
          __ mov(v0, heap_number_result);
          __ sdc1(f10, FieldMemOperand(v0, HeapNumber::kValueOffset));
          __ Ret();
        }

        // A DIV operation expecting an integer result falls through
        // to type transition.

      } else {
        // We preserved a0 and a1 to be able to call runtime.
        // Save the left value on the stack.
        __ Push(t1, t0);

        Label pop_and_call_runtime;

        // Allocate a heap number to store the result.
        heap_number_result = s0;
        GenerateHeapResultAllocation(masm,
                                     heap_number_result,
                                     heap_number_map,
                                     scratch1,
                                     scratch2,
                                     &pop_and_call_runtime);

        // Load the left value from the value saved on the stack.
        __ Pop(a1, a0);

        // Call the C function to handle the double operation.
        FloatingPointHelper::CallCCodeForDoubleOperation(
            masm, op_, heap_number_result, scratch1);
        if (FLAG_debug_code) {
          __ stop("Unreachable code.");
        }

        __ bind(&pop_and_call_runtime);
        __ Drop(2);
        __ Branch(&call_runtime);
      }

      break;
    }

    case Token::BIT_OR:
    case Token::BIT_XOR:
    case Token::BIT_AND:
    case Token::SAR:
    case Token::SHR:
    case Token::SHL: {
      Label return_heap_number;
      Register scratch3 = t1;
      // Convert operands to 32-bit integers. Right in a2 and left in a3. The
      // registers a0 and a1 (right and left) are preserved for the runtime
      // call.
      FloatingPointHelper::LoadNumberAsInt32(masm,
                                             left,
                                             a3,
                                             heap_number_map,
                                             scratch1,
                                             scratch2,
                                             scratch3,
                                             f0,
                                             &transition);
      FloatingPointHelper::LoadNumberAsInt32(masm,
                                             right,
                                             a2,
                                             heap_number_map,
                                             scratch1,
                                             scratch2,
                                             scratch3,
                                             f0,
                                             &transition);

      // The ECMA-262 standard specifies that, for shift operations, only the
      // 5 least significant bits of the shift value should be used.
      switch (op_) {
        case Token::BIT_OR:
          __ Or(a2, a3, Operand(a2));
          break;
        case Token::BIT_XOR:
          __ Xor(a2, a3, Operand(a2));
          break;
        case Token::BIT_AND:
          __ And(a2, a3, Operand(a2));
          break;
        case Token::SAR:
          __ And(a2, a2, Operand(0x1f));
          __ srav(a2, a3, a2);
          break;
        case Token::SHR:
          __ And(a2, a2, Operand(0x1f));
          __ srlv(a2, a3, a2);
          // SHR is special because it is required to produce a positive answer.
          // We only get a negative result if the shift value (a2) is 0.
          // This result cannot be respresented as a signed 32-bit integer, try
          // to return a heap number if we can.
          // The non FPU code does not support this special case, so jump to
          // runtime if we don't support it.
          if (CpuFeatures::IsSupported(FPU)) {
            __ Branch((result_type_ <= BinaryOpIC::INT32)
                        ? &transition
                        : &return_heap_number,
                       lt,
                       a2,
                       Operand(zero_reg));
          } else {
            __ Branch((result_type_ <= BinaryOpIC::INT32)
                        ? &transition
                        : &call_runtime,
                       lt,
                       a2,
                       Operand(zero_reg));
          }
          break;
        case Token::SHL:
          __ And(a2, a2, Operand(0x1f));
          __ sllv(a2, a3, a2);
          break;
        default:
          UNREACHABLE();
      }

      // Check if the result fits in a smi.
      __ Addu(scratch1, a2, Operand(0x40000000));
      // If not try to return a heap number. (We know the result is an int32.)
      __ Branch(&return_heap_number, lt, scratch1, Operand(zero_reg));
      // Tag the result and return.
      __ SmiTag(v0, a2);
      __ Ret();

      __ bind(&return_heap_number);
      heap_number_result = t1;
      GenerateHeapResultAllocation(masm,
                                   heap_number_result,
                                   heap_number_map,
                                   scratch1,
                                   scratch2,
                                   &call_runtime);

      if (CpuFeatures::IsSupported(FPU)) {
        CpuFeatures::Scope scope(FPU);

        if (op_ != Token::SHR) {
          // Convert the result to a floating point value.
          __ mtc1(a2, double_scratch);
          __ cvt_d_w(double_scratch, double_scratch);
        } else {
          // The result must be interpreted as an unsigned 32-bit integer.
          __ mtc1(a2, double_scratch);
          __ Cvt_d_uw(double_scratch, double_scratch, single_scratch);
        }

        // Store the result.
        __ mov(v0, heap_number_result);
        __ sdc1(double_scratch, FieldMemOperand(v0, HeapNumber::kValueOffset));
        __ Ret();
      } else {
        // Tail call that writes the int32 in a2 to the heap number in v0, using
        // a3 and a1 as scratch. v0 is preserved and returned.
        __ mov(a0, t1);
        WriteInt32ToHeapNumberStub stub(a2, v0, a3, a1);
        __ TailCallStub(&stub);
      }

      break;
    }

    default:
      UNREACHABLE();
  }

  // We never expect DIV to yield an integer result, so we always generate
  // type transition code for DIV operations expecting an integer result: the
  // code will fall through to this type transition.
  if (transition.is_linked() ||
      ((op_ == Token::DIV) && (result_type_ <= BinaryOpIC::INT32))) {
    __ bind(&transition);
    GenerateTypeTransition(masm);
  }

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void BinaryOpStub::GenerateOddballStub(MacroAssembler* masm) {
  Label call_runtime;

  if (op_ == Token::ADD) {
    // Handle string addition here, because it is the only operation
    // that does not do a ToNumber conversion on the operands.
    GenerateAddStrings(masm);
  }

  // Convert oddball arguments to numbers.
  Label check, done;
  __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
  __ Branch(&check, ne, a1, Operand(t0));
  if (Token::IsBitOp(op_)) {
    __ li(a1, Operand(Smi::FromInt(0)));
  } else {
    __ LoadRoot(a1, Heap::kNanValueRootIndex);
  }
  __ jmp(&done);
  __ bind(&check);
  __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
  __ Branch(&done, ne, a0, Operand(t0));
  if (Token::IsBitOp(op_)) {
    __ li(a0, Operand(Smi::FromInt(0)));
  } else {
    __ LoadRoot(a0, Heap::kNanValueRootIndex);
  }
  __ bind(&done);

  GenerateHeapNumberStub(masm);
}


void BinaryOpStub::GenerateHeapNumberStub(MacroAssembler* masm) {
  Label call_runtime;
  GenerateFPOperation(masm, false, &call_runtime, &call_runtime);

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void BinaryOpStub::GenerateGeneric(MacroAssembler* masm) {
  Label call_runtime, call_string_add_or_runtime;

  GenerateSmiCode(masm, &call_runtime, &call_runtime, ALLOW_HEAPNUMBER_RESULTS);

  GenerateFPOperation(masm, false, &call_string_add_or_runtime, &call_runtime);

  __ bind(&call_string_add_or_runtime);
  if (op_ == Token::ADD) {
    GenerateAddStrings(masm);
  }

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void BinaryOpStub::GenerateAddStrings(MacroAssembler* masm) {
  ASSERT(op_ == Token::ADD);
  Label left_not_string, call_runtime;

  Register left = a1;
  Register right = a0;

  // Check if left argument is a string.
  __ JumpIfSmi(left, &left_not_string);
  __ GetObjectType(left, a2, a2);
  __ Branch(&left_not_string, ge, a2, Operand(FIRST_NONSTRING_TYPE));

  StringAddStub string_add_left_stub(NO_STRING_CHECK_LEFT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_left_stub);

  // Left operand is not a string, test right.
  __ bind(&left_not_string);
  __ JumpIfSmi(right, &call_runtime);
  __ GetObjectType(right, a2, a2);
  __ Branch(&call_runtime, ge, a2, Operand(FIRST_NONSTRING_TYPE));

  StringAddStub string_add_right_stub(NO_STRING_CHECK_RIGHT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_right_stub);

  // At least one argument is not a string.
  __ bind(&call_runtime);
}


void BinaryOpStub::GenerateCallRuntime(MacroAssembler* masm) {
  GenerateRegisterArgsPush(masm);
  switch (op_) {
    case Token::ADD:
      __ InvokeBuiltin(Builtins::ADD, JUMP_FUNCTION);
      break;
    case Token::SUB:
      __ InvokeBuiltin(Builtins::SUB, JUMP_FUNCTION);
      break;
    case Token::MUL:
      __ InvokeBuiltin(Builtins::MUL, JUMP_FUNCTION);
      break;
    case Token::DIV:
      __ InvokeBuiltin(Builtins::DIV, JUMP_FUNCTION);
      break;
    case Token::MOD:
      __ InvokeBuiltin(Builtins::MOD, JUMP_FUNCTION);
      break;
    case Token::BIT_OR:
      __ InvokeBuiltin(Builtins::BIT_OR, JUMP_FUNCTION);
      break;
    case Token::BIT_AND:
      __ InvokeBuiltin(Builtins::BIT_AND, JUMP_FUNCTION);
      break;
    case Token::BIT_XOR:
      __ InvokeBuiltin(Builtins::BIT_XOR, JUMP_FUNCTION);
      break;
    case Token::SAR:
      __ InvokeBuiltin(Builtins::SAR, JUMP_FUNCTION);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_FUNCTION);
      break;
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


void BinaryOpStub::GenerateHeapResultAllocation(
    MacroAssembler* masm,
    Register result,
    Register heap_number_map,
    Register scratch1,
    Register scratch2,
    Label* gc_required) {

  // Code below will scratch result if allocation fails. To keep both arguments
  // intact for the runtime call result cannot be one of these.
  ASSERT(!result.is(a0) && !result.is(a1));

  if (mode_ == OVERWRITE_LEFT || mode_ == OVERWRITE_RIGHT) {
    Label skip_allocation, allocated;
    Register overwritable_operand = mode_ == OVERWRITE_LEFT ? a1 : a0;
    // If the overwritable operand is already an object, we skip the
    // allocation of a heap number.
    __ JumpIfNotSmi(overwritable_operand, &skip_allocation);
    // Allocate a heap number for the result.
    __ AllocateHeapNumber(
        result, scratch1, scratch2, heap_number_map, gc_required);
    __ Branch(&allocated);
    __ bind(&skip_allocation);
    // Use object holding the overwritable operand for result.
    __ mov(result, overwritable_operand);
    __ bind(&allocated);
  } else {
    ASSERT(mode_ == NO_OVERWRITE);
    __ AllocateHeapNumber(
        result, scratch1, scratch2, heap_number_map, gc_required);
  }
}


void BinaryOpStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
  __ Push(a1, a0);
}



void TranscendentalCacheStub::Generate(MacroAssembler* masm) {
  // Untagged case: double input in f4, double result goes
  //   into f4.
  // Tagged case: tagged input on top of stack and in a0,
  //   tagged result (heap number) goes into v0.

  Label input_not_smi;
  Label loaded;
  Label calculate;
  Label invalid_cache;
  const Register scratch0 = t5;
  const Register scratch1 = t3;
  const Register cache_entry = a0;
  const bool tagged = (argument_type_ == TAGGED);

  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);

    if (tagged) {
      // Argument is a number and is on stack and in a0.
      // Load argument and check if it is a smi.
      __ JumpIfNotSmi(a0, &input_not_smi);

      // Input is a smi. Convert to double and load the low and high words
      // of the double into a2, a3.
      __ sra(t0, a0, kSmiTagSize);
      __ mtc1(t0, f4);
      __ cvt_d_w(f4, f4);
      __ Move(a2, a3, f4);
      __ Branch(&loaded);

      __ bind(&input_not_smi);
      // Check if input is a HeapNumber.
      __ CheckMap(a0,
                  a1,
                  Heap::kHeapNumberMapRootIndex,
                  &calculate,
                  DONT_DO_SMI_CHECK);
      // Input is a HeapNumber. Store the
      // low and high words into a2, a3.
      __ lw(a2, FieldMemOperand(a0, HeapNumber::kValueOffset));
      __ lw(a3, FieldMemOperand(a0, HeapNumber::kValueOffset + 4));
    } else {
      // Input is untagged double in f4. Output goes to f4.
      __ Move(a2, a3, f4);
    }
    __ bind(&loaded);
    // a2 = low 32 bits of double value.
    // a3 = high 32 bits of double value.
    // Compute hash (the shifts are arithmetic):
    //   h = (low ^ high); h ^= h >> 16; h ^= h >> 8; h = h & (cacheSize - 1);
    __ Xor(a1, a2, a3);
    __ sra(t0, a1, 16);
    __ Xor(a1, a1, t0);
    __ sra(t0, a1, 8);
    __ Xor(a1, a1, t0);
    ASSERT(IsPowerOf2(TranscendentalCache::SubCache::kCacheSize));
    __ And(a1, a1, Operand(TranscendentalCache::SubCache::kCacheSize - 1));

    // a2 = low 32 bits of double value.
    // a3 = high 32 bits of double value.
    // a1 = TranscendentalCache::hash(double value).
    __ li(cache_entry, Operand(
        ExternalReference::transcendental_cache_array_address(
            masm->isolate())));
    // a0 points to cache array.
    __ lw(cache_entry, MemOperand(cache_entry, type_ * sizeof(
        Isolate::Current()->transcendental_cache()->caches_[0])));
    // a0 points to the cache for the type type_.
    // If NULL, the cache hasn't been initialized yet, so go through runtime.
    __ Branch(&invalid_cache, eq, cache_entry, Operand(zero_reg));

#ifdef DEBUG
    // Check that the layout of cache elements match expectations.
    { TranscendentalCache::SubCache::Element test_elem[2];
      char* elem_start = reinterpret_cast<char*>(&test_elem[0]);
      char* elem2_start = reinterpret_cast<char*>(&test_elem[1]);
      char* elem_in0 = reinterpret_cast<char*>(&(test_elem[0].in[0]));
      char* elem_in1 = reinterpret_cast<char*>(&(test_elem[0].in[1]));
      char* elem_out = reinterpret_cast<char*>(&(test_elem[0].output));
      CHECK_EQ(12, elem2_start - elem_start);  // Two uint_32's and a pointer.
      CHECK_EQ(0, elem_in0 - elem_start);
      CHECK_EQ(kIntSize, elem_in1 - elem_start);
      CHECK_EQ(2 * kIntSize, elem_out - elem_start);
    }
#endif

    // Find the address of the a1'st entry in the cache, i.e., &a0[a1*12].
    __ sll(t0, a1, 1);
    __ Addu(a1, a1, t0);
    __ sll(t0, a1, 2);
    __ Addu(cache_entry, cache_entry, t0);

    // Check if cache matches: Double value is stored in uint32_t[2] array.
    __ lw(t0, MemOperand(cache_entry, 0));
    __ lw(t1, MemOperand(cache_entry, 4));
    __ lw(t2, MemOperand(cache_entry, 8));
    __ Addu(cache_entry, cache_entry, 12);
    __ Branch(&calculate, ne, a2, Operand(t0));
    __ Branch(&calculate, ne, a3, Operand(t1));
    // Cache hit. Load result, cleanup and return.
    if (tagged) {
      // Pop input value from stack and load result into v0.
      __ Drop(1);
      __ mov(v0, t2);
    } else {
      // Load result into f4.
      __ ldc1(f4, FieldMemOperand(t2, HeapNumber::kValueOffset));
    }
    __ Ret();
  }  // if (CpuFeatures::IsSupported(FPU))

  __ bind(&calculate);
  if (tagged) {
    __ bind(&invalid_cache);
    __ TailCallExternalReference(ExternalReference(RuntimeFunction(),
                                                   masm->isolate()),
                                 1,
                                 1);
  } else {
    if (!CpuFeatures::IsSupported(FPU)) UNREACHABLE();
    CpuFeatures::Scope scope(FPU);

    Label no_update;
    Label skip_cache;
    const Register heap_number_map = t2;

    // Call C function to calculate the result and update the cache.
    // Register a0 holds precalculated cache entry address; preserve
    // it on the stack and pop it into register cache_entry after the
    // call.
    __ push(cache_entry);
    GenerateCallCFunction(masm, scratch0);
    __ GetCFunctionDoubleResult(f4);

    // Try to update the cache. If we cannot allocate a
    // heap number, we return the result without updating.
    __ pop(cache_entry);
    __ LoadRoot(t1, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(t2, scratch0, scratch1, t1, &no_update);
    __ sdc1(f4, FieldMemOperand(t2, HeapNumber::kValueOffset));

    __ sw(a2, MemOperand(cache_entry, 0 * kPointerSize));
    __ sw(a3, MemOperand(cache_entry, 1 * kPointerSize));
    __ sw(t2, MemOperand(cache_entry, 2 * kPointerSize));

    __ mov(v0, cache_entry);
    __ Ret();

    __ bind(&invalid_cache);
    // The cache is invalid. Call runtime which will recreate the
    // cache.
    __ LoadRoot(t1, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(a0, scratch0, scratch1, t1, &skip_cache);
    __ sdc1(f4, FieldMemOperand(a0, HeapNumber::kValueOffset));
    __ EnterInternalFrame();
    __ push(a0);
    __ CallRuntime(RuntimeFunction(), 1);
    __ LeaveInternalFrame();
    __ ldc1(f4, FieldMemOperand(v0, HeapNumber::kValueOffset));
    __ Ret();

    __ bind(&skip_cache);
    // Call C function to calculate the result and answer directly
    // without updating the cache.
    GenerateCallCFunction(masm, scratch0);
    __ GetCFunctionDoubleResult(f4);
    __ bind(&no_update);

    // We return the value in f4 without adding it to the cache, but
    // we cause a scavenging GC so that future allocations will succeed.
    __ EnterInternalFrame();

    // Allocate an aligned object larger than a HeapNumber.
    ASSERT(4 * kPointerSize >= HeapNumber::kSize);
    __ li(scratch0, Operand(4 * kPointerSize));
    __ push(scratch0);
    __ CallRuntimeSaveDoubles(Runtime::kAllocateInNewSpace);
    __ LeaveInternalFrame();
    __ Ret();
  }
}


void TranscendentalCacheStub::GenerateCallCFunction(MacroAssembler* masm,
                                                    Register scratch) {
  __ push(ra);
  __ PrepareCallCFunction(2, scratch);
  if (IsMipsSoftFloatABI) {
    __ Move(v0, v1, f4);
  } else {
    __ mov_d(f12, f4);
  }
  switch (type_) {
    case TranscendentalCache::SIN:
      __ CallCFunction(
          ExternalReference::math_sin_double_function(masm->isolate()), 2);
      break;
    case TranscendentalCache::COS:
      __ CallCFunction(
          ExternalReference::math_cos_double_function(masm->isolate()), 2);
      break;
    case TranscendentalCache::LOG:
      __ CallCFunction(
          ExternalReference::math_log_double_function(masm->isolate()), 2);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  __ pop(ra);
}


Runtime::FunctionId TranscendentalCacheStub::RuntimeFunction() {
  switch (type_) {
    // Add more cases when necessary.
    case TranscendentalCache::SIN: return Runtime::kMath_sin;
    case TranscendentalCache::COS: return Runtime::kMath_cos;
    case TranscendentalCache::LOG: return Runtime::kMath_log;
    default:
      UNIMPLEMENTED();
      return Runtime::kAbort;
  }
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  __ TailCallRuntime(Runtime::kStackGuard, 0, 1);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  Label call_runtime;

  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);

    Label base_not_smi;
    Label exponent_not_smi;
    Label convert_exponent;

    const Register base = a0;
    const Register exponent = a2;
    const Register heapnumbermap = t1;
    const Register heapnumber = s0;  // Callee-saved register.
    const Register scratch = t2;
    const Register scratch2 = t3;

    // Alocate FP values in the ABI-parameter-passing regs.
    const DoubleRegister double_base = f12;
    const DoubleRegister double_exponent = f14;
    const DoubleRegister double_result = f0;
    const DoubleRegister double_scratch = f2;

    __ LoadRoot(heapnumbermap, Heap::kHeapNumberMapRootIndex);
    __ lw(base, MemOperand(sp, 1 * kPointerSize));
    __ lw(exponent, MemOperand(sp, 0 * kPointerSize));

    // Convert base to double value and store it in f0.
    __ JumpIfNotSmi(base, &base_not_smi);
    // Base is a Smi. Untag and convert it.
    __ SmiUntag(base);
    __ mtc1(base, double_scratch);
    __ cvt_d_w(double_base, double_scratch);
    __ Branch(&convert_exponent);

    __ bind(&base_not_smi);
    __ lw(scratch, FieldMemOperand(base, JSObject::kMapOffset));
    __ Branch(&call_runtime, ne, scratch, Operand(heapnumbermap));
    // Base is a heapnumber. Load it into double register.
    __ ldc1(double_base, FieldMemOperand(base, HeapNumber::kValueOffset));

    __ bind(&convert_exponent);
    __ JumpIfNotSmi(exponent, &exponent_not_smi);
    __ SmiUntag(exponent);

    // The base is in a double register and the exponent is
    // an untagged smi. Allocate a heap number and call a
    // C function for integer exponents. The register containing
    // the heap number is callee-saved.
    __ AllocateHeapNumber(heapnumber,
                          scratch,
                          scratch2,
                          heapnumbermap,
                          &call_runtime);
    __ push(ra);
    __ PrepareCallCFunction(3, scratch);
    __ SetCallCDoubleArguments(double_base, exponent);
    __ CallCFunction(
        ExternalReference::power_double_int_function(masm->isolate()), 3);
    __ pop(ra);
    __ GetCFunctionDoubleResult(double_result);
    __ sdc1(double_result,
            FieldMemOperand(heapnumber, HeapNumber::kValueOffset));
    __ mov(v0, heapnumber);
    __ DropAndRet(2 * kPointerSize);

    __ bind(&exponent_not_smi);
    __ lw(scratch, FieldMemOperand(exponent, JSObject::kMapOffset));
    __ Branch(&call_runtime, ne, scratch, Operand(heapnumbermap));
    // Exponent is a heapnumber. Load it into double register.
    __ ldc1(double_exponent,
            FieldMemOperand(exponent, HeapNumber::kValueOffset));

    // The base and the exponent are in double registers.
    // Allocate a heap number and call a C function for
    // double exponents. The register containing
    // the heap number is callee-saved.
    __ AllocateHeapNumber(heapnumber,
                          scratch,
                          scratch2,
                          heapnumbermap,
                          &call_runtime);
    __ push(ra);
    __ PrepareCallCFunction(4, scratch);
    // ABI (o32) for func(double a, double b): a in f12, b in f14.
    ASSERT(double_base.is(f12));
    ASSERT(double_exponent.is(f14));
    __ SetCallCDoubleArguments(double_base, double_exponent);
    __ CallCFunction(
        ExternalReference::power_double_double_function(masm->isolate()), 4);
    __ pop(ra);
    __ GetCFunctionDoubleResult(double_result);
    __ sdc1(double_result,
            FieldMemOperand(heapnumber, HeapNumber::kValueOffset));
    __ mov(v0, heapnumber);
    __ DropAndRet(2 * kPointerSize);
  }

  __ bind(&call_runtime);
  __ TailCallRuntime(Runtime::kMath_pow_cfunction, 2, 1);
}


bool CEntryStub::NeedsImmovableCode() {
  return true;
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  __ Throw(v0);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  __ ThrowUncatchable(type, v0);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate) {
  // v0: result parameter for PerformGC, if any
  // s0: number of arguments including receiver (C callee-saved)
  // s1: pointer to the first argument          (C callee-saved)
  // s2: pointer to builtin function            (C callee-saved)

  if (do_gc) {
    // Move result passed in v0 into a0 to call PerformGC.
    __ mov(a0, v0);
    __ PrepareCallCFunction(1, a1);
    __ CallCFunction(
        ExternalReference::perform_gc_function(masm->isolate()), 1);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth(masm->isolate());
  if (always_allocate) {
    __ li(a0, Operand(scope_depth));
    __ lw(a1, MemOperand(a0));
    __ Addu(a1, a1, Operand(1));
    __ sw(a1, MemOperand(a0));
  }

  // Prepare arguments for C routine: a0 = argc, a1 = argv
  __ mov(a0, s0);
  __ mov(a1, s1);

  // We are calling compiled C/C++ code. a0 and a1 hold our two arguments. We
  // also need to reserve the 4 argument slots on the stack.

  __ AssertStackIsAligned();

  __ li(a2, Operand(ExternalReference::isolate_address()));

  // To let the GC traverse the return address of the exit frames, we need to
  // know where the return address is. The CEntryStub is unmovable, so
  // we can store the address on the stack to be able to find it again and
  // we never have to restore it, because it will not change.
  { Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm);
    // This branch-and-link sequence is needed to find the current PC on mips,
    // saved to the ra register.
    // Use masm-> here instead of the double-underscore macro since extra
    // coverage code can interfere with the proper calculation of ra.
    Label find_ra;
    masm->bal(&find_ra);  // bal exposes branch delay slot.
    masm->nop();  // Branch delay slot nop.
    masm->bind(&find_ra);

    // Adjust the value in ra to point to the correct return location, 2nd
    // instruction past the real call into C code (the jalr(t9)), and push it.
    // This is the return address of the exit frame.
    const int kNumInstructionsToJump = 6;
    masm->Addu(ra, ra, kNumInstructionsToJump * kPointerSize);
    masm->sw(ra, MemOperand(sp));  // This spot was reserved in EnterExitFrame.
    masm->Subu(sp, sp, kCArgsSlotsSize);
    // Stack is still aligned.

    // Call the C routine.
    masm->mov(t9, s2);  // Function pointer to t9 to conform to ABI for PIC.
    masm->jalr(t9);
    masm->nop();    // Branch delay slot nop.
    // Make sure the stored 'ra' points to this position.
    ASSERT_EQ(kNumInstructionsToJump,
              masm->InstructionsGeneratedSince(&find_ra));
  }

  // Restore stack (remove arg slots).
  __ Addu(sp, sp, kCArgsSlotsSize);

  if (always_allocate) {
    // It's okay to clobber a2 and a3 here. v0 & v1 contain result.
    __ li(a2, Operand(scope_depth));
    __ lw(a3, MemOperand(a2));
    __ Subu(a3, a3, Operand(1));
    __ sw(a3, MemOperand(a2));
  }

  // Check for failure result.
  Label failure_returned;
  STATIC_ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  __ addiu(a2, v0, 1);
  __ andi(t0, a2, kFailureTagMask);
  __ Branch(&failure_returned, eq, t0, Operand(zero_reg));

  // Exit C frame and return.
  // v0:v1: result
  // sp: stack pointer
  // fp: frame pointer
  __ LeaveExitFrame(save_doubles_, s0);
  __ Ret();

  // Check if we should retry or throw exception.
  Label retry;
  __ bind(&failure_returned);
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ andi(t0, v0, ((1 << kFailureTypeTagSize) - 1) << kFailureTagSize);
  __ Branch(&retry, eq, t0, Operand(zero_reg));

  // Special handling of out of memory exceptions.
  Failure* out_of_memory = Failure::OutOfMemoryException();
  __ Branch(throw_out_of_memory_exception, eq,
            v0, Operand(reinterpret_cast<int32_t>(out_of_memory)));

  // Retrieve the pending exception and clear the variable.
  __ li(t0,
        Operand(ExternalReference::the_hole_value_location(masm->isolate())));
  __ lw(a3, MemOperand(t0));
  __ li(t0, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      masm->isolate())));
  __ lw(v0, MemOperand(t0));
  __ sw(a3, MemOperand(t0));

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ Branch(throw_termination_exception, eq,
            v0, Operand(masm->isolate()->factory()->termination_exception()));

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  __ bind(&retry);
  // Last failure (v0) will be moved to (a0) for parameter when retrying.
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // a0: number of arguments including receiver
  // a1: pointer to builtin function
  // fp: frame pointer    (restored after C call)
  // sp: stack pointer    (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  // Compute the argv pointer in a callee-saved register.
  __ sll(s1, a0, kPointerSizeLog2);
  __ Addu(s1, sp, s1);
  __ Subu(s1, s1, Operand(kPointerSize));

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(save_doubles_);

  // Setup argc and the builtin function in callee-saved registers.
  __ mov(s0, a0);
  __ mov(s2, a1);

  // s0: number of arguments (C callee-saved)
  // s1: pointer to first argument (C callee-saved)
  // s2: pointer to builtin function (C callee-saved)

  Label throw_normal_exception;
  Label throw_termination_exception;
  Label throw_out_of_memory_exception;

  // Call into the runtime system.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               false,
               false);

  // Do space-specific GC and retry runtime call.
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               false);

  // Do full GC and retry runtime call one final time.
  Failure* failure = Failure::InternalError();
  __ li(v0, Operand(reinterpret_cast<int32_t>(failure)));
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               true);

  __ bind(&throw_out_of_memory_exception);
  GenerateThrowUncatchable(masm, OUT_OF_MEMORY);

  __ bind(&throw_termination_exception);
  GenerateThrowUncatchable(masm, TERMINATION);

  __ bind(&throw_normal_exception);
  GenerateThrowTOS(masm);
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, exit;

  // Registers:
  // a0: entry address
  // a1: function
  // a2: reveiver
  // a3: argc
  //
  // Stack:
  // 4 args slots
  // args

  // Save callee saved registers on the stack.
  __ MultiPush(kCalleeSaved | ra.bit());

  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    // Save callee-saved FPU registers.
    __ MultiPushFPU(kCalleeSavedFPU);
  }

  // Load argv in s0 register.
  int offset_to_argv = (kNumCalleeSaved + 1) * kPointerSize;
  if (CpuFeatures::IsSupported(FPU)) {
    offset_to_argv += kNumCalleeSavedFPU * kDoubleSize;
  }

  __ lw(s0, MemOperand(sp, offset_to_argv + kCArgsSlotsSize));

  // We build an EntryFrame.
  __ li(t3, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ li(t2, Operand(Smi::FromInt(marker)));
  __ li(t1, Operand(Smi::FromInt(marker)));
  __ li(t0, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                      masm->isolate())));
  __ lw(t0, MemOperand(t0));
  __ Push(t3, t2, t1, t0);
  // Setup frame pointer for the frame to be pushed.
  __ addiu(fp, sp, -EntryFrameConstants::kCallerFPOffset);

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: reveiver_pointer
  // a3: argc
  // s0: argv
  //
  // Stack:
  // caller fp          |
  // function slot      | entry frame
  // context slot       |
  // bad fp (0xff...f)  |
  // callee saved registers + ra
  // 4 args slots
  // args

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress,
                                masm->isolate());
  __ li(t1, Operand(ExternalReference(js_entry_sp)));
  __ lw(t2, MemOperand(t1));
  __ Branch(&non_outermost_js, ne, t2, Operand(zero_reg));
  __ sw(fp, MemOperand(t1));
  __ li(t0, Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  Label cont;
  __ b(&cont);
  __ nop();   // Branch delay slot nop.
  __ bind(&non_outermost_js);
  __ li(t0, Operand(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME)));
  __ bind(&cont);
  __ push(t0);

  // Call a faked try-block that does the invoke.
  __ bal(&invoke);  // bal exposes branch delay slot.
  __ nop();   // Branch delay slot nop.

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  // Coming in here the fp will be invalid because the PushTryHandler below
  // sets it to 0 to signal the existence of the JSEntry frame.
  __ li(t0, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      masm->isolate())));
  __ sw(v0, MemOperand(t0));  // We come back from 'invoke'. result is in v0.
  __ li(v0, Operand(reinterpret_cast<int32_t>(Failure::Exception())));
  __ b(&exit);  // b exposes branch delay slot.
  __ nop();   // Branch delay slot nop.

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bal(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Clear any pending exceptions.
  __ li(t0,
        Operand(ExternalReference::the_hole_value_location(masm->isolate())));
  __ lw(t1, MemOperand(t0));
  __ li(t0, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      masm->isolate())));
  __ sw(t1, MemOperand(t0));

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Registers:
  // a0: entry_address
  // a1: function
  // a2: reveiver_pointer
  // a3: argc
  // s0: argv
  //
  // Stack:
  // handler frame
  // entry frame
  // callee saved registers + ra
  // 4 args slots
  // args

  if (is_construct) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      masm->isolate());
    __ li(t0, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, masm->isolate());
    __ li(t0, Operand(entry));
  }
  __ lw(t9, MemOperand(t0));  // Deref address.

  // Call JSEntryTrampoline.
  __ addiu(t9, t9, Code::kHeaderSize - kHeapObjectTag);
  __ Call(t9);

  // Unlink this frame from the handler chain.
  __ PopTryHandler();

  __ bind(&exit);  // v0 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(t1);
  __ Branch(&non_outermost_js_2, ne, t1,
            Operand(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME)));
  __ li(t1, Operand(ExternalReference(js_entry_sp)));
  __ sw(zero_reg, MemOperand(t1));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(t1);
  __ li(t0, Operand(ExternalReference(Isolate::kCEntryFPAddress,
                                      masm->isolate())));
  __ sw(t1, MemOperand(t0));

  // Reset the stack to the callee saved registers.
  __ addiu(sp, sp, -EntryFrameConstants::kCallerFPOffset);

  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    // Restore callee-saved fpu registers.
    __ MultiPopFPU(kCalleeSavedFPU);
  }

  // Restore callee saved registers from the stack.
  __ MultiPop(kCalleeSaved | ra.bit());
  // Return.
  __ Jump(ra);
}


// Uses registers a0 to t0.
// Expected input (depending on whether args are in registers or on the stack):
// * object: a0 or at sp + 1 * kPointerSize.
// * function: a1 or at sp.
//
// Inlined call site patching is a crankshaft-specific feature that is not
// implemented on MIPS.
void InstanceofStub::Generate(MacroAssembler* masm) {
  // This is a crankshaft-specific feature that has not been implemented yet.
  ASSERT(!HasCallSiteInlineCheck());
  // Call site inlining and patching implies arguments in registers.
  ASSERT(HasArgsInRegisters() || !HasCallSiteInlineCheck());
  // ReturnTrueFalse is only implemented for inlined call sites.
  ASSERT(!ReturnTrueFalseObject() || HasCallSiteInlineCheck());

  // Fixed register usage throughout the stub:
  const Register object = a0;  // Object (lhs).
  Register map = a3;  // Map of the object.
  const Register function = a1;  // Function (rhs).
  const Register prototype = t0;  // Prototype of the function.
  const Register inline_site = t5;
  const Register scratch = a2;

  Label slow, loop, is_instance, is_not_instance, not_js_object;

  if (!HasArgsInRegisters()) {
    __ lw(object, MemOperand(sp, 1 * kPointerSize));
    __ lw(function, MemOperand(sp, 0));
  }

  // Check that the left hand is a JS object and load map.
  __ JumpIfSmi(object, &not_js_object);
  __ IsObjectJSObjectType(object, map, scratch, &not_js_object);

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck()) {
    Label miss;
    __ LoadRoot(t1, Heap::kInstanceofCacheFunctionRootIndex);
    __ Branch(&miss, ne, function, Operand(t1));
    __ LoadRoot(t1, Heap::kInstanceofCacheMapRootIndex);
    __ Branch(&miss, ne, map, Operand(t1));
    __ LoadRoot(v0, Heap::kInstanceofCacheAnswerRootIndex);
    __ DropAndRet(HasArgsInRegisters() ? 0 : 2);

    __ bind(&miss);
  }

  // Get the prototype of the function.
  __ TryGetFunctionPrototype(function, prototype, scratch, &slow);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(prototype, &slow);
  __ IsObjectJSObjectType(prototype, scratch, scratch, &slow);

  // Update the global instanceof or call site inlined cache with the current
  // map and function. The cached answer will be set when it is known below.
  if (!HasCallSiteInlineCheck()) {
    __ StoreRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
    __ StoreRoot(map, Heap::kInstanceofCacheMapRootIndex);
  } else {
    UNIMPLEMENTED_MIPS();
  }

  // Register mapping: a3 is object map and t0 is function prototype.
  // Get prototype of object into a2.
  __ lw(scratch, FieldMemOperand(map, Map::kPrototypeOffset));

  // We don't need map any more. Use it as a scratch register.
  Register scratch2 = map;
  map = no_reg;

  // Loop through the prototype chain looking for the function prototype.
  __ LoadRoot(scratch2, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ Branch(&is_instance, eq, scratch, Operand(prototype));
  __ Branch(&is_not_instance, eq, scratch, Operand(scratch2));
  __ lw(scratch, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ lw(scratch, FieldMemOperand(scratch, Map::kPrototypeOffset));
  __ Branch(&loop);

  __ bind(&is_instance);
  ASSERT(Smi::FromInt(0) == 0);
  if (!HasCallSiteInlineCheck()) {
    __ mov(v0, zero_reg);
    __ StoreRoot(v0, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    UNIMPLEMENTED_MIPS();
  }
  __ DropAndRet(HasArgsInRegisters() ? 0 : 2);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    __ li(v0, Operand(Smi::FromInt(1)));
    __ StoreRoot(v0, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    UNIMPLEMENTED_MIPS();
  }
  __ DropAndRet(HasArgsInRegisters() ? 0 : 2);

  Label object_not_null, object_not_null_or_smi;
  __ bind(&not_js_object);
  // Before null, smi and string value checks, check that the rhs is a function
  // as for a non-function rhs an exception needs to be thrown.
  __ JumpIfSmi(function, &slow);
  __ GetObjectType(function, scratch2, scratch);
  __ Branch(&slow, ne, scratch, Operand(JS_FUNCTION_TYPE));

  // Null is not instance of anything.
  __ Branch(&object_not_null, ne, scratch,
      Operand(masm->isolate()->factory()->null_value()));
  __ li(v0, Operand(Smi::FromInt(1)));
  __ DropAndRet(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null);
  // Smi values are not instances of anything.
  __ JumpIfNotSmi(object, &object_not_null_or_smi);
  __ li(v0, Operand(Smi::FromInt(1)));
  __ DropAndRet(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null_or_smi);
  // String values are not instances of anything.
  __ IsObjectJSStringType(object, scratch, &slow);
  __ li(v0, Operand(Smi::FromInt(1)));
  __ DropAndRet(HasArgsInRegisters() ? 0 : 2);

  // Slow-case.  Tail call builtin.
  __ bind(&slow);
  if (!ReturnTrueFalseObject()) {
    if (HasArgsInRegisters()) {
      __ Push(a0, a1);
    }
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
  } else {
    __ EnterInternalFrame();
    __ Push(a0, a1);
    __ InvokeBuiltin(Builtins::INSTANCE_OF, CALL_FUNCTION);
    __ LeaveInternalFrame();
    __ mov(a0, v0);
    __ LoadRoot(v0, Heap::kTrueValueRootIndex);
    __ DropAndRet(HasArgsInRegisters() ? 0 : 2, eq, a0, Operand(zero_reg));
    __ LoadRoot(v0, Heap::kFalseValueRootIndex);
    __ DropAndRet(HasArgsInRegisters() ? 0 : 2);
  }
}


Register InstanceofStub::left() { return a0; }


Register InstanceofStub::right() { return a1; }


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The displacement is the offset of the last parameter (if any)
  // relative to the frame pointer.
  static const int kDisplacement =
      StandardFrameConstants::kCallerSPOffset - kPointerSize;

  // Check that the key is a smiGenerateReadElement.
  Label slow;
  __ JumpIfNotSmi(a1, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ lw(a2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(a3, MemOperand(a2, StandardFrameConstants::kContextOffset));
  __ Branch(&adaptor,
            eq,
            a3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Check index (a1) against formal parameters count limit passed in
  // through register a0. Use unsigned comparison to get negative
  // check for free.
  __ Branch(&slow, hs, a1, Operand(a0));

  // Read the argument from the stack and return it.
  __ subu(a3, a0, a1);
  __ sll(t3, a3, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(a3, fp, Operand(t3));
  __ lw(v0, MemOperand(a3, kDisplacement));
  __ Ret();

  // Arguments adaptor case: Check index (a1) against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ lw(a0, MemOperand(a2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Branch(&slow, Ugreater_equal, a1, Operand(a0));

  // Read the argument from the adaptor frame and return it.
  __ subu(a3, a0, a1);
  __ sll(t3, a3, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(a3, a2, Operand(t3));
  __ lw(v0, MemOperand(a3, kDisplacement));
  __ Ret();

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ push(a1);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictSlow(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[4] : receiver displacement
  // sp[8] : function
  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ lw(a3, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(a2, MemOperand(a3, StandardFrameConstants::kContextOffset));
  __ Branch(&runtime, ne,
            a2, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Patch the arguments.length and the parameters pointer in the current frame.
  __ lw(a2, MemOperand(a3, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ sw(a2, MemOperand(sp, 0 * kPointerSize));
  __ sll(t3, a2, 1);
  __ Addu(a3, a3, Operand(t3));
  __ addiu(a3, a3, StandardFrameConstants::kCallerSPOffset);
  __ sw(a3, MemOperand(sp, 1 * kPointerSize));

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictFast(MacroAssembler* masm) {
  // Stack layout:
  //  sp[0] : number of parameters (tagged)
  //  sp[4] : address of receiver argument
  //  sp[8] : function
  // Registers used over whole function:
  //  t2 : allocated object (tagged)
  //  t5 : mapped parameter count (tagged)

  __ lw(a1, MemOperand(sp, 0 * kPointerSize));
  // a1 = parameter count (tagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  Label adaptor_frame, try_allocate;
  __ lw(a3, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(a2, MemOperand(a3, StandardFrameConstants::kContextOffset));
  __ Branch(&adaptor_frame, eq, a2,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // No adaptor, parameter count = argument count.
  __ mov(a2, a1);
  __ b(&try_allocate);
  __ nop();   // Branch delay slot nop.

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ lw(a2, MemOperand(a3, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ sll(t6, a2, 1);
  __ Addu(a3, a3, Operand(t6));
  __ Addu(a3, a3, Operand(StandardFrameConstants::kCallerSPOffset));
  __ sw(a3, MemOperand(sp, 1 * kPointerSize));

  // a1 = parameter count (tagged)
  // a2 = argument count (tagged)
  // Compute the mapped parameter count = min(a1, a2) in a1.
  Label skip_min;
  __ Branch(&skip_min, lt, a1, Operand(a2));
  __ mov(a1, a2);
  __ bind(&skip_min);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  // If there are no mapped parameters, we do not need the parameter_map.
  Label param_map_size;
  ASSERT_EQ(0, Smi::FromInt(0));
  __ Branch(USE_DELAY_SLOT, &param_map_size, eq, a1, Operand(zero_reg));
  __ mov(t5, zero_reg);  // In delay slot: param map size = 0 when a1 == 0.
  __ sll(t5, a1, 1);
  __ addiu(t5, t5, kParameterMapHeaderSize);
  __ bind(&param_map_size);

  // 2. Backing store.
  __ sll(t6, a2, 1);
  __ Addu(t5, t5, Operand(t6));
  __ Addu(t5, t5, Operand(FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ Addu(t5, t5, Operand(Heap::kArgumentsObjectSize));

  // Do the allocation of all three objects in one go.
  __ AllocateInNewSpace(t5, v0, a3, t0, &runtime, TAG_OBJECT);

  // v0 = address of new object(s) (tagged)
  // a2 = argument count (tagged)
  // Get the arguments boilerplate from the current (global) context into t0.
  const int kNormalOffset =
      Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX);
  const int kAliasedOffset =
      Context::SlotOffset(Context::ALIASED_ARGUMENTS_BOILERPLATE_INDEX);

  __ lw(t0, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ lw(t0, FieldMemOperand(t0, GlobalObject::kGlobalContextOffset));
  Label skip2_ne, skip2_eq;
  __ Branch(&skip2_ne, ne, a1, Operand(zero_reg));
  __ lw(t0, MemOperand(t0, kNormalOffset));
  __ bind(&skip2_ne);

  __ Branch(&skip2_eq, eq, a1, Operand(zero_reg));
  __ lw(t0, MemOperand(t0, kAliasedOffset));
  __ bind(&skip2_eq);

  // v0 = address of new object (tagged)
  // a1 = mapped parameter count (tagged)
  // a2 = argument count (tagged)
  // t0 = address of boilerplate object (tagged)
  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ lw(a3, FieldMemOperand(t0, i));
    __ sw(a3, FieldMemOperand(v0, i));
  }

  // Setup the callee in-object property.
  STATIC_ASSERT(Heap::kArgumentsCalleeIndex == 1);
  __ lw(a3, MemOperand(sp, 2 * kPointerSize));
  const int kCalleeOffset = JSObject::kHeaderSize +
      Heap::kArgumentsCalleeIndex * kPointerSize;
  __ sw(a3, FieldMemOperand(v0, kCalleeOffset));

  // Use the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  const int kLengthOffset = JSObject::kHeaderSize +
      Heap::kArgumentsLengthIndex * kPointerSize;
  __ sw(a2, FieldMemOperand(v0, kLengthOffset));

  // Setup the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, t0 will point there, otherwise
  // it will point to the backing store.
  __ Addu(t0, v0, Operand(Heap::kArgumentsObjectSize));
  __ sw(t0, FieldMemOperand(v0, JSObject::kElementsOffset));

  // v0 = address of new object (tagged)
  // a1 = mapped parameter count (tagged)
  // a2 = argument count (tagged)
  // t0 = address of parameter map or backing store (tagged)
  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  Label skip3;
  __ Branch(&skip3, ne, a1, Operand(Smi::FromInt(0)));
  // Move backing store address to a3, because it is
  // expected there when filling in the unmapped arguments.
  __ mov(a3, t0);
  __ bind(&skip3);

  __ Branch(&skip_parameter_map, eq, a1, Operand(Smi::FromInt(0)));

  __ LoadRoot(t2, Heap::kNonStrictArgumentsElementsMapRootIndex);
  __ sw(t2, FieldMemOperand(t0, FixedArray::kMapOffset));
  __ Addu(t2, a1, Operand(Smi::FromInt(2)));
  __ sw(t2, FieldMemOperand(t0, FixedArray::kLengthOffset));
  __ sw(cp, FieldMemOperand(t0, FixedArray::kHeaderSize + 0 * kPointerSize));
  __ sll(t6, a1, 1);
  __ Addu(t2, t0, Operand(t6));
  __ Addu(t2, t2, Operand(kParameterMapHeaderSize));
  __ sw(t2, FieldMemOperand(t0, FixedArray::kHeaderSize + 1 * kPointerSize));

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ mov(t2, a1);
  __ lw(t5, MemOperand(sp, 0 * kPointerSize));
  __ Addu(t5, t5, Operand(Smi::FromInt(Context::MIN_CONTEXT_SLOTS)));
  __ Subu(t5, t5, Operand(a1));
  __ LoadRoot(t3, Heap::kTheHoleValueRootIndex);
  __ sll(t6, t2, 1);
  __ Addu(a3, t0, Operand(t6));
  __ Addu(a3, a3, Operand(kParameterMapHeaderSize));

  // t2 = loop variable (tagged)
  // a1 = mapping index (tagged)
  // a3 = address of backing store (tagged)
  // t0 = address of parameter map (tagged)
  // t1 = temporary scratch (a.o., for address calculation)
  // t3 = the hole value
  __ jmp(&parameters_test);

  __ bind(&parameters_loop);
  __ Subu(t2, t2, Operand(Smi::FromInt(1)));
  __ sll(t1, t2, 1);
  __ Addu(t1, t1, Operand(kParameterMapHeaderSize - kHeapObjectTag));
  __ Addu(t6, t0, t1);
  __ sw(t5, MemOperand(t6));
  __ Subu(t1, t1, Operand(kParameterMapHeaderSize - FixedArray::kHeaderSize));
  __ Addu(t6, a3, t1);
  __ sw(t3, MemOperand(t6));
  __ Addu(t5, t5, Operand(Smi::FromInt(1)));
  __ bind(&parameters_test);
  __ Branch(&parameters_loop, ne, t2, Operand(Smi::FromInt(0)));

  __ bind(&skip_parameter_map);
  // a2 = argument count (tagged)
  // a3 = address of backing store (tagged)
  // t1 = scratch
  // Copy arguments header and remaining slots (if there are any).
  __ LoadRoot(t1, Heap::kFixedArrayMapRootIndex);
  __ sw(t1, FieldMemOperand(a3, FixedArray::kMapOffset));
  __ sw(a2, FieldMemOperand(a3, FixedArray::kLengthOffset));

  Label arguments_loop, arguments_test;
  __ mov(t5, a1);
  __ lw(t0, MemOperand(sp, 1 * kPointerSize));
  __ sll(t6, t5, 1);
  __ Subu(t0, t0, Operand(t6));
  __ jmp(&arguments_test);

  __ bind(&arguments_loop);
  __ Subu(t0, t0, Operand(kPointerSize));
  __ lw(t2, MemOperand(t0, 0));
  __ sll(t6, t5, 1);
  __ Addu(t1, a3, Operand(t6));
  __ sw(t2, FieldMemOperand(t1, FixedArray::kHeaderSize));
  __ Addu(t5, t5, Operand(Smi::FromInt(1)));

  __ bind(&arguments_test);
  __ Branch(&arguments_loop, lt, t5, Operand(a2));

  // Return and remove the on-stack parameters.
  __ Addu(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  // a2 = argument count (taggged)
  __ bind(&runtime);
  __ sw(a2, MemOperand(sp, 0 * kPointerSize));  // Patch argument count.
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewStrict(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[4] : receiver displacement
  // sp[8] : function
  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ lw(a2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(a3, MemOperand(a2, StandardFrameConstants::kContextOffset));
  __ Branch(&adaptor_frame,
            eq,
            a3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Get the length from the frame.
  __ lw(a1, MemOperand(sp, 0));
  __ Branch(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ lw(a1, MemOperand(a2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ sw(a1, MemOperand(sp, 0));
  __ sll(at, a1, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(a3, a2, Operand(at));

  __ Addu(a3, a3, Operand(StandardFrameConstants::kCallerSPOffset));
  __ sw(a3, MemOperand(sp, 1 * kPointerSize));

  // Try the new space allocation. Start out with computing the size
  // of the arguments object and the elements array in words.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ Branch(&add_arguments_object, eq, a1, Operand(zero_reg));
  __ srl(a1, a1, kSmiTagSize);

  __ Addu(a1, a1, Operand(FixedArray::kHeaderSize / kPointerSize));
  __ bind(&add_arguments_object);
  __ Addu(a1, a1, Operand(Heap::kArgumentsObjectSizeStrict / kPointerSize));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(a1,
                        v0,
                        a2,
                        a3,
                        &runtime,
                        static_cast<AllocationFlags>(TAG_OBJECT |
                                                     SIZE_IN_WORDS));

  // Get the arguments boilerplate from the current (global) context.
  __ lw(t0, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ lw(t0, FieldMemOperand(t0, GlobalObject::kGlobalContextOffset));
  __ lw(t0, MemOperand(t0, Context::SlotOffset(
      Context::STRICT_MODE_ARGUMENTS_BOILERPLATE_INDEX)));

  // Copy the JS object part.
  __ CopyFields(v0, t0, a3.bit(), JSObject::kHeaderSize / kPointerSize);

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ lw(a1, MemOperand(sp, 0 * kPointerSize));
  __ sw(a1, FieldMemOperand(v0, JSObject::kHeaderSize +
      Heap::kArgumentsLengthIndex * kPointerSize));

  Label done;
  __ Branch(&done, eq, a1, Operand(zero_reg));

  // Get the parameters pointer from the stack.
  __ lw(a2, MemOperand(sp, 1 * kPointerSize));

  // Setup the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ Addu(t0, v0, Operand(Heap::kArgumentsObjectSizeStrict));
  __ sw(t0, FieldMemOperand(v0, JSObject::kElementsOffset));
  __ LoadRoot(a3, Heap::kFixedArrayMapRootIndex);
  __ sw(a3, FieldMemOperand(t0, FixedArray::kMapOffset));
  __ sw(a1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // Untag the length for the loop.
  __ srl(a1, a1, kSmiTagSize);

  // Copy the fixed array slots.
  Label loop;
  // Setup t0 to point to the first array slot.
  __ Addu(t0, t0, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ bind(&loop);
  // Pre-decrement a2 with kPointerSize on each iteration.
  // Pre-decrement in order to skip receiver.
  __ Addu(a2, a2, Operand(-kPointerSize));
  __ lw(a3, MemOperand(a2));
  // Post-increment t0 with kPointerSize on each iteration.
  __ sw(a3, MemOperand(t0));
  __ Addu(t0, t0, Operand(kPointerSize));
  __ Subu(a1, a1, Operand(1));
  __ Branch(&loop, ne, a1, Operand(zero_reg));

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ Addu(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewStrictArgumentsFast, 3, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#else  // V8_INTERPRETED_REGEXP
  if (!FLAG_regexp_entry_native) {
    __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
    return;
  }

  // Stack frame on entry.
  //  sp[0]: last_match_info (expected JSArray)
  //  sp[4]: previous index
  //  sp[8]: subject string
  //  sp[12]: JSRegExp object

  static const int kLastMatchInfoOffset = 0 * kPointerSize;
  static const int kPreviousIndexOffset = 1 * kPointerSize;
  static const int kSubjectOffset = 2 * kPointerSize;
  static const int kJSRegExpOffset = 3 * kPointerSize;

  Label runtime, invoke_regexp;

  // Allocation of registers for this function. These are in callee save
  // registers and will be preserved by the call to the native RegExp code, as
  // this code is called using the normal C calling convention. When calling
  // directly from generated code the native RegExp code will not do a GC and
  // therefore the content of these registers are safe to use after the call.
  // MIPS - using s0..s2, since we are not using CEntry Stub.
  Register subject = s0;
  Register regexp_data = s1;
  Register last_match_info_elements = s2;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(
          masm->isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(masm->isolate());
  __ li(a0, Operand(address_of_regexp_stack_memory_size));
  __ lw(a0, MemOperand(a0, 0));
  __ Branch(&runtime, eq, a0, Operand(zero_reg));

  // Check that the first argument is a JSRegExp object.
  __ lw(a0, MemOperand(sp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfSmi(a0, &runtime);
  __ GetObjectType(a0, a1, a1);
  __ Branch(&runtime, ne, a1, Operand(JS_REGEXP_TYPE));

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ lw(regexp_data, FieldMemOperand(a0, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ And(t0, regexp_data, Operand(kSmiTagMask));
    __ Check(nz,
             "Unexpected type for RegExp data, FixedArray expected",
             t0,
             Operand(zero_reg));
    __ GetObjectType(regexp_data, a0, a0);
    __ Check(eq,
             "Unexpected type for RegExp data, FixedArray expected",
             a0,
             Operand(FIXED_ARRAY_TYPE));
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ lw(a0, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  __ Branch(&runtime, ne, a0, Operand(Smi::FromInt(JSRegExp::IRREGEXP)));

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ lw(a2,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2. This
  // uses the asumption that smis are 2 * their untagged value.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ Addu(a2, a2, Operand(2));  // a2 was a smi.
  // Check that the static offsets vector buffer is large enough.
  __ Branch(&runtime, hi, a2, Operand(OffsetsVector::kStaticOffsetsVectorSize));

  // a2: Number of capture registers
  // regexp_data: RegExp data (FixedArray)
  // Check that the second argument is a string.
  __ lw(subject, MemOperand(sp, kSubjectOffset));
  __ JumpIfSmi(subject, &runtime);
  __ GetObjectType(subject, a0, a0);
  __ And(a0, a0, Operand(kIsNotStringMask));
  STATIC_ASSERT(kStringTag == 0);
  __ Branch(&runtime, ne, a0, Operand(zero_reg));

  // Get the length of the string to r3.
  __ lw(a3, FieldMemOperand(subject, String::kLengthOffset));

  // a2: Number of capture registers
  // a3: Length of subject string as a smi
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check that the third argument is a positive smi less than the subject
  // string length. A negative value will be greater (unsigned comparison).
  __ lw(a0, MemOperand(sp, kPreviousIndexOffset));
  __ And(at, a0, Operand(kSmiTagMask));
  __ Branch(&runtime, ne, at, Operand(zero_reg));
  __ Branch(&runtime, ls, a3, Operand(a0));

  // a2: Number of capture registers
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check that the fourth object is a JSArray object.
  __ lw(a0, MemOperand(sp, kLastMatchInfoOffset));
  __ JumpIfSmi(a0, &runtime);
  __ GetObjectType(a0, a1, a1);
  __ Branch(&runtime, ne, a1, Operand(JS_ARRAY_TYPE));
  // Check that the JSArray is in fast case.
  __ lw(last_match_info_elements,
         FieldMemOperand(a0, JSArray::kElementsOffset));
  __ lw(a0, FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ Branch(&runtime, ne, a0, Operand(
      masm->isolate()->factory()->fixed_array_map()));
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ lw(a0,
         FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ Addu(a2, a2, Operand(RegExpImpl::kLastMatchOverhead));
  __ sra(at, a0, kSmiTagSize);  // Untag length for comparison.
  __ Branch(&runtime, gt, a2, Operand(at));

  // Reset offset for possibly sliced string.
  __ mov(t0, zero_reg);
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_string;
  __ lw(a0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbu(a0, FieldMemOperand(a0, Map::kInstanceTypeOffset));
  // First check for flat string.
  __ And(a1, a0, Operand(kIsNotStringMask | kStringRepresentationMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ Branch(&seq_string, eq, a1, Operand(zero_reg));

  // subject: Subject string
  // a0: instance type if Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check for flat cons string or sliced string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  // In the case of a sliced string its offset has to be taken into account.
  Label cons_string, check_encoding;
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  __ Branch(&cons_string, lt, a1, Operand(kExternalStringTag));
  __ Branch(&runtime, eq, a1, Operand(kExternalStringTag));

  // String is sliced.
  __ lw(t0, FieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ sra(t0, t0, kSmiTagSize);
  __ lw(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  // t5: offset of sliced string, smi-tagged.
  __ jmp(&check_encoding);
  // String is a cons string, check whether it is flat.
  __ bind(&cons_string);
  __ lw(a0, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ LoadRoot(a1, Heap::kEmptyStringRootIndex);
  __ Branch(&runtime, ne, a0, Operand(a1));
  __ lw(subject, FieldMemOperand(subject, ConsString::kFirstOffset));
  // Is first part of cons or parent of slice a flat string?
  __ bind(&check_encoding);
  __ lw(a0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ lbu(a0, FieldMemOperand(a0, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kSeqStringTag == 0);
  __ And(at, a0, Operand(kStringRepresentationMask));
  __ Branch(&runtime, ne, at, Operand(zero_reg));

  __ bind(&seq_string);
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // a0: Instance type of subject string
  STATIC_ASSERT(kStringEncodingMask == 4);
  STATIC_ASSERT(kAsciiStringTag == 4);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // Find the code object based on the assumptions above.
  __ And(a0, a0, Operand(kStringEncodingMask));  // Non-zero for ascii.
  __ lw(t9, FieldMemOperand(regexp_data, JSRegExp::kDataAsciiCodeOffset));
  __ sra(a3, a0, 2);  // a3 is 1 for ascii, 0 for UC16 (usyed below).
  __ lw(t1, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset));
  __ movz(t9, t1, a0);  // If UC16 (a0 is 0), replace t9 w/kDataUC16CodeOffset.

  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(t9, &runtime);

  // a3: encoding of subject string (1 if ASCII, 0 if two_byte);
  // t9: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ lw(a1, MemOperand(sp, kPreviousIndexOffset));
  __ sra(a1, a1, kSmiTagSize);  // Untag the Smi.

  // a1: previous index
  // a3: encoding of subject string (1 if ASCII, 0 if two_byte);
  // t9: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(masm->isolate()->counters()->regexp_entry_native(),
                      1, a0, a2);

  // Isolates: note we add an additional parameter here (isolate pointer).
  static const int kRegExpExecuteArguments = 8;
  static const int kParameterRegisters = 4;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers, meaning we
  // treat the return address as argument 5. Thus every argument after that
  // needs to be shifted back by 1. Since DirectCEntryStub will handle
  // allocating space for the c argument slots, we don't need to calculate
  // that into the argument positions on the stack. This is how the stack will
  // look (sp meaning the value of sp at this moment):
  // [sp + 4] - Argument 8
  // [sp + 3] - Argument 7
  // [sp + 2] - Argument 6
  // [sp + 1] - Argument 5
  // [sp + 0] - saved ra

  // Argument 8: Pass current isolate address.
  // CFunctionArgumentOperand handles MIPS stack argument slots.
  __ li(a0, Operand(ExternalReference::isolate_address()));
  __ sw(a0, MemOperand(sp, 4 * kPointerSize));

  // Argument 7: Indicate that this is a direct call from JavaScript.
  __ li(a0, Operand(1));
  __ sw(a0, MemOperand(sp, 3 * kPointerSize));

  // Argument 6: Start (high end) of backtracking stack memory area.
  __ li(a0, Operand(address_of_regexp_stack_memory_address));
  __ lw(a0, MemOperand(a0, 0));
  __ li(a2, Operand(address_of_regexp_stack_memory_size));
  __ lw(a2, MemOperand(a2, 0));
  __ addu(a0, a0, a2);
  __ sw(a0, MemOperand(sp, 2 * kPointerSize));

  // Argument 5: static offsets vector buffer.
  __ li(a0, Operand(
        ExternalReference::address_of_static_offsets_vector(masm->isolate())));
  __ sw(a0, MemOperand(sp, 1 * kPointerSize));

  // For arguments 4 and 3 get string length, calculate start of string data
  // and calculate the shift of the index (0 for ASCII and 1 for two byte).
  STATIC_ASSERT(SeqAsciiString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ Addu(t2, subject, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ Xor(a3, a3, Operand(1));  // 1 for 2-byte str, 0 for 1-byte.
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to two pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and moves up sp by 2 * kPointerSize.)
  __ lw(subject, MemOperand(fp, kSubjectOffset + 2 * kPointerSize));
  // If slice offset is not 0, load the length from the original sliced string.
  // Argument 4, a3: End of string data
  // Argument 3, a2: Start of string data
  // Prepare start and end index of the input.
  __ sllv(t1, t0, a3);
  __ addu(t0, t2, t1);
  __ sllv(t1, a1, a3);
  __ addu(a2, t0, t1);

  __ lw(t2, FieldMemOperand(subject, String::kLengthOffset));
  __ sra(t2, t2, kSmiTagSize);
  __ sllv(t1, t2, a3);
  __ addu(a3, t0, t1);
  // Argument 2 (a1): Previous index.
  // Already there

  // Argument 1 (a0): Subject string.
  __ mov(a0, subject);

  // Locate the code entry and call it.
  __ Addu(t9, t9, Operand(Code::kHeaderSize - kHeapObjectTag));
  DirectCEntryStub stub;
  stub.GenerateCall(masm, t9);

  __ LeaveExitFrame(false, no_reg);

  // v0: result
  // subject: subject string (callee saved)
  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)

  // Check the result.

  Label success;
  __ Branch(&success, eq,
            v0, Operand(NativeRegExpMacroAssembler::SUCCESS));
  Label failure;
  __ Branch(&failure, eq,
            v0, Operand(NativeRegExpMacroAssembler::FAILURE));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ Branch(&runtime, ne,
            v0, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ li(a1, Operand(
      ExternalReference::the_hole_value_location(masm->isolate())));
  __ lw(a1, MemOperand(a1, 0));
  __ li(a2, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                      masm->isolate())));
  __ lw(v0, MemOperand(a2, 0));
  __ Branch(&runtime, eq, v0, Operand(a1));

  __ sw(a1, MemOperand(a2, 0));  // Clear pending exception.

  // Check if the exception is a termination. If so, throw as uncatchable.
  __ LoadRoot(a0, Heap::kTerminationExceptionRootIndex);
  Label termination_exception;
  __ Branch(&termination_exception, eq, v0, Operand(a0));

  __ Throw(v0);  // Expects thrown value in v0.

  __ bind(&termination_exception);
  __ ThrowUncatchable(TERMINATION, v0);  // Expects thrown value in v0.

  __ bind(&failure);
  // For failure and exception return null.
  __ li(v0, Operand(masm->isolate()->factory()->null_value()));
  __ Addu(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Process the result from the native regexp code.
  __ bind(&success);
  __ lw(a1,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ Addu(a1, a1, Operand(2));  // a1 was a smi.

  // a1: number of capture registers
  // subject: subject string
  // Store the capture count.
  __ sll(a2, a1, kSmiTagSize + kSmiShiftSize);  // To smi.
  __ sw(a2, FieldMemOperand(last_match_info_elements,
                             RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ mov(a3, last_match_info_elements);  // Moved up to reduce latency.
  __ sw(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastSubjectOffset));
  __ RecordWrite(a3, Operand(RegExpImpl::kLastSubjectOffset), a2, t0);
  __ sw(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastInputOffset));
  __ mov(a3, last_match_info_elements);
  __ RecordWrite(a3, Operand(RegExpImpl::kLastInputOffset), a2, t0);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(masm->isolate());
  __ li(a2, Operand(address_of_static_offsets_vector));

  // a1: number of capture registers
  // a2: offsets vector
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wrapping after zero.
  __ Addu(a0,
         last_match_info_elements,
         Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag));
  __ bind(&next_capture);
  __ Subu(a1, a1, Operand(1));
  __ Branch(&done, lt, a1, Operand(zero_reg));
  // Read the value from the static offsets vector buffer.
  __ lw(a3, MemOperand(a2, 0));
  __ addiu(a2, a2, kPointerSize);
  // Store the smi value in the last match info.
  __ sll(a3, a3, kSmiTagSize);  // Convert to Smi.
  __ sw(a3, MemOperand(a0, 0));
  __ Branch(&next_capture, USE_DELAY_SLOT);
  __ addiu(a0, a0, kPointerSize);   // In branch delay slot.

  __ bind(&done);

  // Return last match info.
  __ lw(v0, MemOperand(sp, kLastMatchInfoOffset));
  __ Addu(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#endif  // V8_INTERPRETED_REGEXP
}


void RegExpConstructResultStub::Generate(MacroAssembler* masm) {
  const int kMaxInlineLength = 100;
  Label slowcase;
  Label done;
  __ lw(a1, MemOperand(sp, kPointerSize * 2));
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  __ JumpIfNotSmi(a1, &slowcase);
  __ Branch(&slowcase, hi, a1, Operand(Smi::FromInt(kMaxInlineLength)));
  // Smi-tagging is equivalent to multiplying by 2.
  // Allocate RegExpResult followed by FixedArray with size in ebx.
  // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
  // Elements:  [Map][Length][..elements..]
  // Size of JSArray with two in-object properties and the header of a
  // FixedArray.
  int objects_size =
      (JSRegExpResult::kSize + FixedArray::kHeaderSize) / kPointerSize;
  __ srl(t1, a1, kSmiTagSize + kSmiShiftSize);
  __ Addu(a2, t1, Operand(objects_size));
  __ AllocateInNewSpace(
      a2,  // In: Size, in words.
      v0,  // Out: Start of allocation (tagged).
      a3,  // Scratch register.
      t0,  // Scratch register.
      &slowcase,
      static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));
  // v0: Start of allocated area, object-tagged.
  // a1: Number of elements in array, as smi.
  // t1: Number of elements, untagged.

  // Set JSArray map to global.regexp_result_map().
  // Set empty properties FixedArray.
  // Set elements to point to FixedArray allocated right after the JSArray.
  // Interleave operations for better latency.
  __ lw(a2, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ Addu(a3, v0, Operand(JSRegExpResult::kSize));
  __ li(t0, Operand(masm->isolate()->factory()->empty_fixed_array()));
  __ lw(a2, FieldMemOperand(a2, GlobalObject::kGlobalContextOffset));
  __ sw(a3, FieldMemOperand(v0, JSObject::kElementsOffset));
  __ lw(a2, ContextOperand(a2, Context::REGEXP_RESULT_MAP_INDEX));
  __ sw(t0, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(a2, FieldMemOperand(v0, HeapObject::kMapOffset));

  // Set input, index and length fields from arguments.
  __ lw(a1, MemOperand(sp, kPointerSize * 0));
  __ sw(a1, FieldMemOperand(v0, JSRegExpResult::kInputOffset));
  __ lw(a1, MemOperand(sp, kPointerSize * 1));
  __ sw(a1, FieldMemOperand(v0, JSRegExpResult::kIndexOffset));
  __ lw(a1, MemOperand(sp, kPointerSize * 2));
  __ sw(a1, FieldMemOperand(v0, JSArray::kLengthOffset));

  // Fill out the elements FixedArray.
  // v0: JSArray, tagged.
  // a3: FixedArray, tagged.
  // t1: Number of elements in array, untagged.

  // Set map.
  __ li(a2, Operand(masm->isolate()->factory()->fixed_array_map()));
  __ sw(a2, FieldMemOperand(a3, HeapObject::kMapOffset));
  // Set FixedArray length.
  __ sll(t2, t1, kSmiTagSize);
  __ sw(t2, FieldMemOperand(a3, FixedArray::kLengthOffset));
  // Fill contents of fixed-array with the-hole.
  __ li(a2, Operand(masm->isolate()->factory()->the_hole_value()));
  __ Addu(a3, a3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // Fill fixed array elements with hole.
  // v0: JSArray, tagged.
  // a2: the hole.
  // a3: Start of elements in FixedArray.
  // t1: Number of elements to fill.
  Label loop;
  __ sll(t1, t1, kPointerSizeLog2);  // Convert num elements to num bytes.
  __ addu(t1, t1, a3);  // Point past last element to store.
  __ bind(&loop);
  __ Branch(&done, ge, a3, Operand(t1));  // Break when a3 past end of elem.
  __ sw(a2, MemOperand(a3));
  __ Branch(&loop, USE_DELAY_SLOT);
  __ addiu(a3, a3, kPointerSize);  // In branch delay slot.

  __ bind(&done);
  __ Addu(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&slowcase);
  __ TailCallRuntime(Runtime::kRegExpConstructResult, 3, 1);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

  // The receiver might implicitly be the global object. This is
  // indicated by passing the hole as the receiver to the call
  // function stub.
  if (ReceiverMightBeImplicit()) {
    Label call;
    // Get the receiver from the stack.
    // function, receiver [, arguments]
    __ lw(t0, MemOperand(sp, argc_ * kPointerSize));
    // Call as function is indicated with the hole.
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Branch(&call, ne, t0, Operand(at));
    // Patch the receiver on the stack with the global receiver object.
    __ lw(a1, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
    __ lw(a1, FieldMemOperand(a1, GlobalObject::kGlobalReceiverOffset));
    __ sw(a1, MemOperand(sp, argc_ * kPointerSize));
    __ bind(&call);
  }

  // Get the function to call from the stack.
  // function, receiver [, arguments]
  __ lw(a1, MemOperand(sp, (argc_ + 1) * kPointerSize));

  // Check that the function is really a JavaScript function.
  // a1: pushed function (to be verified)
  __ JumpIfSmi(a1, &slow);
  // Get the map of the function object.
  __ GetObjectType(a1, a2, a2);
  __ Branch(&slow, ne, a2, Operand(JS_FUNCTION_TYPE));

  // Fast-case: Invoke the function now.
  // a1: pushed function
  ParameterCount actual(argc_);

  if (ReceiverMightBeImplicit()) {
    Label call_as_function;
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Branch(&call_as_function, eq, t0, Operand(at));
    __ InvokeFunction(a1,
                      actual,
                      JUMP_FUNCTION,
                      NullCallWrapper(),
                      CALL_AS_METHOD);
    __ bind(&call_as_function);
  }
  __ InvokeFunction(a1,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper(),
                    CALL_AS_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ sw(a1, MemOperand(sp, argc_ * kPointerSize));
  __ li(a0, Operand(argc_));  // Setup the number of arguments.
  __ mov(a2, zero_reg);
  __ GetBuiltinEntry(a3, Builtins::CALL_NON_FUNCTION);
  __ SetCallKind(t1, CALL_AS_METHOD);
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


// Unfortunately you have to run without snapshots to see most of these
// names in the profile since most compare stubs end up in the snapshot.
void CompareStub::PrintName(StringStream* stream) {
  ASSERT((lhs_.is(a0) && rhs_.is(a1)) ||
         (lhs_.is(a1) && rhs_.is(a0)));
  const char* cc_name;
  switch (cc_) {
    case lt: cc_name = "LT"; break;
    case gt: cc_name = "GT"; break;
    case le: cc_name = "LE"; break;
    case ge: cc_name = "GE"; break;
    case eq: cc_name = "EQ"; break;
    case ne: cc_name = "NE"; break;
    default: cc_name = "UnknownCondition"; break;
  }
  bool is_equality = cc_ == eq || cc_ == ne;
  stream->Add("CompareStub_%s", cc_name);
  stream->Add(lhs_.is(a0) ? "_a0" : "_a1");
  stream->Add(rhs_.is(a0) ? "_a0" : "_a1");
  if (strict_ && is_equality) stream->Add("_STRICT");
  if (never_nan_nan_ && is_equality) stream->Add("_NO_NAN");
  if (!include_number_compare_) stream->Add("_NO_NUMBER");
  if (!include_smi_compare_) stream->Add("_NO_SMI");
}


int CompareStub::MinorKey() {
  // Encode the two parameters in a unique 16 bit value.
  ASSERT(static_cast<unsigned>(cc_) < (1 << 14));
  ASSERT((lhs_.is(a0) && rhs_.is(a1)) ||
         (lhs_.is(a1) && rhs_.is(a0)));
  return ConditionField::encode(static_cast<unsigned>(cc_))
         | RegisterField::encode(lhs_.is(a0))
         | StrictField::encode(strict_)
         | NeverNanNanField::encode(cc_ == eq ? never_nan_nan_ : false)
         | IncludeSmiCompareField::encode(include_smi_compare_);
}


// StringCharCodeAtGenerator.
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  Label flat_string;
  Label ascii_string;
  Label got_char_code;
  Label sliced_string;

  ASSERT(!t0.is(scratch_));
  ASSERT(!t0.is(index_));
  ASSERT(!t0.is(result_));
  ASSERT(!t0.is(object_));

  // If the receiver is a smi trigger the non-string case.
  __ JumpIfSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ lw(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ And(t0, result_, Operand(kIsNotStringMask));
  __ Branch(receiver_not_string_, ne, t0, Operand(zero_reg));

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);

  // Put smi-tagged index into scratch register.
  __ mov(scratch_, index_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ lw(t0, FieldMemOperand(object_, String::kLengthOffset));
  __ Branch(index_out_of_range_, ls, t0, Operand(scratch_));

  // We need special handling for non-flat strings.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ And(t0, result_, Operand(kStringRepresentationMask));
  __ Branch(&flat_string, eq, t0, Operand(zero_reg));

  // Handle non-flat strings.
  __ And(result_, result_, Operand(kStringRepresentationMask));
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  __ Branch(&sliced_string, gt, result_, Operand(kExternalStringTag));
  __ Branch(&call_runtime_, eq, result_, Operand(kExternalStringTag));

  // ConsString.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  Label assure_seq_string;
  __ lw(result_, FieldMemOperand(object_, ConsString::kSecondOffset));
  __ LoadRoot(t0, Heap::kEmptyStringRootIndex);
  __ Branch(&call_runtime_, ne, result_, Operand(t0));

  // Get the first of the two strings and load its instance type.
  __ lw(object_, FieldMemOperand(object_, ConsString::kFirstOffset));
  __ jmp(&assure_seq_string);

  // SlicedString, unpack and add offset.
  __ bind(&sliced_string);
  __ lw(result_, FieldMemOperand(object_, SlicedString::kOffsetOffset));
  __ addu(scratch_, scratch_, result_);
  __ lw(object_, FieldMemOperand(object_, SlicedString::kParentOffset));

  // Assure that we are dealing with a sequential string. Go to runtime if not.
  __ bind(&assure_seq_string);
  __ lw(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  // Check that parent is not an external string. Go to runtime otherwise.
  STATIC_ASSERT(kSeqStringTag == 0);

  __ And(t0, result_, Operand(kStringRepresentationMask));
  __ Branch(&call_runtime_, ne, t0, Operand(zero_reg));

  // Check for 1-byte or 2-byte string.
  __ bind(&flat_string);
  STATIC_ASSERT((kStringEncodingMask & kAsciiStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ And(t0, result_, Operand(kStringEncodingMask));
  __ Branch(&ascii_string, ne, t0, Operand(zero_reg));

  // 2-byte string.
  // Load the 2-byte character code into the result register. We can
  // add without shifting since the smi tag size is the log2 of the
  // number of bytes in a two-byte character.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1 && kSmiShiftSize == 0);
  __ Addu(scratch_, object_, Operand(scratch_));
  __ lhu(result_, FieldMemOperand(scratch_, SeqTwoByteString::kHeaderSize));
  __ Branch(&got_char_code);

  // ASCII string.
  // Load the byte into the result register.
  __ bind(&ascii_string);

  __ srl(t0, scratch_, kSmiTagSize);
  __ Addu(scratch_, object_, t0);

  __ lbu(result_, FieldMemOperand(scratch_, SeqAsciiString::kHeaderSize));

  __ bind(&got_char_code);
  __ sll(result_, result_, kSmiTagSize);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharCodeAt slow case");

  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              scratch_,
              Heap::kHeapNumberMapRootIndex,
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  // Consumed by runtime conversion function:
  __ Push(object_, index_, index_);
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    ASSERT(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }

  // Save the conversion result before the pop instructions below
  // have a chance to overwrite it.

  __ Move(scratch_, v0);

  __ pop(index_);
  __ pop(object_);
  // Reload the instance type.
  __ lw(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ lbu(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(scratch_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ Branch(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);

  __ Move(result_, v0);

  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharCodeAt slow case");
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.

  ASSERT(!t0.is(result_));
  ASSERT(!t0.is(code_));

  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiShiftSize == 0);
  ASSERT(IsPowerOf2(String::kMaxAsciiCharCode + 1));
  __ And(t0,
         code_,
         Operand(kSmiTagMask |
                 ((~String::kMaxAsciiCharCode) << kSmiTagSize)));
  __ Branch(&slow_case_, ne, t0, Operand(zero_reg));

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged ASCII char code.
  STATIC_ASSERT(kSmiTag == 0);
  __ sll(t0, code_, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(result_, result_, t0);
  __ lw(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
  __ Branch(&slow_case_, eq, result_, Operand(t0));
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharFromCode slow case");

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  __ Move(result_, v0);

  call_helper.AfterCall(masm);
  __ Branch(&exit_);

  __ Abort("Unexpected fallthrough from CharFromCode slow case");
}


// -------------------------------------------------------------------------
// StringCharAtGenerator

void StringCharAtGenerator::GenerateFast(MacroAssembler* masm) {
  char_code_at_generator_.GenerateFast(masm);
  char_from_code_generator_.GenerateFast(masm);
}


void StringCharAtGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  char_code_at_generator_.GenerateSlow(masm, call_helper);
  char_from_code_generator_.GenerateSlow(masm, call_helper);
}


class StringHelper : public AllStatic {
 public:
  // Generate code for copying characters using a simple loop. This should only
  // be used in places where the number of characters is small and the
  // additional setup and checking in GenerateCopyCharactersLong adds too much
  // overhead. Copying of overlapping regions is not supported.
  // Dest register ends at the position after the last character written.
  static void GenerateCopyCharacters(MacroAssembler* masm,
                                     Register dest,
                                     Register src,
                                     Register count,
                                     Register scratch,
                                     bool ascii);

  // Generate code for copying a large number of characters. This function
  // is allowed to spend extra time setting up conditions to make copying
  // faster. Copying of overlapping regions is not supported.
  // Dest register ends at the position after the last character written.
  static void GenerateCopyCharactersLong(MacroAssembler* masm,
                                         Register dest,
                                         Register src,
                                         Register count,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Register scratch4,
                                         Register scratch5,
                                         int flags);


  // Probe the symbol table for a two character string. If the string is
  // not found by probing a jump to the label not_found is performed. This jump
  // does not guarantee that the string is not in the symbol table. If the
  // string is found the code falls through with the string in register r0.
  // Contents of both c1 and c2 registers are modified. At the exit c1 is
  // guaranteed to contain halfword with low and high bytes equal to
  // initial contents of c1 and c2 respectively.
  static void GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                                   Register c1,
                                                   Register c2,
                                                   Register scratch1,
                                                   Register scratch2,
                                                   Register scratch3,
                                                   Register scratch4,
                                                   Register scratch5,
                                                   Label* not_found);

  // Generate string hash.
  static void GenerateHashInit(MacroAssembler* masm,
                               Register hash,
                               Register character);

  static void GenerateHashAddCharacter(MacroAssembler* masm,
                                       Register hash,
                                       Register character);

  static void GenerateHashGetHash(MacroAssembler* masm,
                                  Register hash);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StringHelper);
};


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          Register scratch,
                                          bool ascii) {
  Label loop;
  Label done;
  // This loop just copies one character at a time, as it is only used for
  // very short strings.
  if (!ascii) {
    __ addu(count, count, count);
  }
  __ Branch(&done, eq, count, Operand(zero_reg));
  __ addu(count, dest, count);  // Count now points to the last dest byte.

  __ bind(&loop);
  __ lbu(scratch, MemOperand(src));
  __ addiu(src, src, 1);
  __ sb(scratch, MemOperand(dest));
  __ addiu(dest, dest, 1);
  __ Branch(&loop, lt, dest, Operand(count));

  __ bind(&done);
}


enum CopyCharactersFlags {
  COPY_ASCII = 1,
  DEST_ALWAYS_ALIGNED = 2
};


void StringHelper::GenerateCopyCharactersLong(MacroAssembler* masm,
                                              Register dest,
                                              Register src,
                                              Register count,
                                              Register scratch1,
                                              Register scratch2,
                                              Register scratch3,
                                              Register scratch4,
                                              Register scratch5,
                                              int flags) {
  bool ascii = (flags & COPY_ASCII) != 0;
  bool dest_always_aligned = (flags & DEST_ALWAYS_ALIGNED) != 0;

  if (dest_always_aligned && FLAG_debug_code) {
    // Check that destination is actually word aligned if the flag says
    // that it is.
    __ And(scratch4, dest, Operand(kPointerAlignmentMask));
    __ Check(eq,
             "Destination of copy not aligned.",
             scratch4,
             Operand(zero_reg));
  }

  const int kReadAlignment = 4;
  const int kReadAlignmentMask = kReadAlignment - 1;
  // Ensure that reading an entire aligned word containing the last character
  // of a string will not read outside the allocated area (because we pad up
  // to kObjectAlignment).
  STATIC_ASSERT(kObjectAlignment >= kReadAlignment);
  // Assumes word reads and writes are little endian.
  // Nothing to do for zero characters.
  Label done;

  if (!ascii) {
    __ addu(count, count, count);
  }
  __ Branch(&done, eq, count, Operand(zero_reg));

  Label byte_loop;
  // Must copy at least eight bytes, otherwise just do it one byte at a time.
  __ Subu(scratch1, count, Operand(8));
  __ Addu(count, dest, Operand(count));
  Register limit = count;  // Read until src equals this.
  __ Branch(&byte_loop, lt, scratch1, Operand(zero_reg));

  if (!dest_always_aligned) {
    // Align dest by byte copying. Copies between zero and three bytes.
    __ And(scratch4, dest, Operand(kReadAlignmentMask));
    Label dest_aligned;
    __ Branch(&dest_aligned, eq, scratch4, Operand(zero_reg));
    Label aligned_loop;
    __ bind(&aligned_loop);
    __ lbu(scratch1, MemOperand(src));
    __ addiu(src, src, 1);
    __ sb(scratch1, MemOperand(dest));
    __ addiu(dest, dest, 1);
    __ addiu(scratch4, scratch4, 1);
    __ Branch(&aligned_loop, le, scratch4, Operand(kReadAlignmentMask));
    __ bind(&dest_aligned);
  }

  Label simple_loop;

  __ And(scratch4, src, Operand(kReadAlignmentMask));
  __ Branch(&simple_loop, eq, scratch4, Operand(zero_reg));

  // Loop for src/dst that are not aligned the same way.
  // This loop uses lwl and lwr instructions. These instructions
  // depend on the endianness, and the implementation assumes little-endian.
  {
    Label loop;
    __ bind(&loop);
    __ lwr(scratch1, MemOperand(src));
    __ Addu(src, src, Operand(kReadAlignment));
    __ lwl(scratch1, MemOperand(src, -1));
    __ sw(scratch1, MemOperand(dest));
    __ Addu(dest, dest, Operand(kReadAlignment));
    __ Subu(scratch2, limit, dest);
    __ Branch(&loop, ge, scratch2, Operand(kReadAlignment));
  }

  __ Branch(&byte_loop);

  // Simple loop.
  // Copy words from src to dest, until less than four bytes left.
  // Both src and dest are word aligned.
  __ bind(&simple_loop);
  {
    Label loop;
    __ bind(&loop);
    __ lw(scratch1, MemOperand(src));
    __ Addu(src, src, Operand(kReadAlignment));
    __ sw(scratch1, MemOperand(dest));
    __ Addu(dest, dest, Operand(kReadAlignment));
    __ Subu(scratch2, limit, dest);
    __ Branch(&loop, ge, scratch2, Operand(kReadAlignment));
  }

  // Copy bytes from src to dest until dest hits limit.
  __ bind(&byte_loop);
  // Test if dest has already reached the limit.
  __ Branch(&done, ge, dest, Operand(limit));
  __ lbu(scratch1, MemOperand(src));
  __ addiu(src, src, 1);
  __ sb(scratch1, MemOperand(dest));
  __ addiu(dest, dest, 1);
  __ Branch(&byte_loop);

  __ bind(&done);
}


void StringHelper::GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                                        Register c1,
                                                        Register c2,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4,
                                                        Register scratch5,
                                                        Label* not_found) {
  // Register scratch3 is the general scratch register in this function.
  Register scratch = scratch3;

  // Make sure that both characters are not digits as such strings has a
  // different hash algorithm. Don't try to look for these in the symbol table.
  Label not_array_index;
  __ Subu(scratch, c1, Operand(static_cast<int>('0')));
  __ Branch(&not_array_index,
            Ugreater,
            scratch,
            Operand(static_cast<int>('9' - '0')));
  __ Subu(scratch, c2, Operand(static_cast<int>('0')));

  // If check failed combine both characters into single halfword.
  // This is required by the contract of the method: code at the
  // not_found branch expects this combination in c1 register.
  Label tmp;
  __ sll(scratch1, c2, kBitsPerByte);
  __ Branch(&tmp, Ugreater, scratch, Operand(static_cast<int>('9' - '0')));
  __ Or(c1, c1, scratch1);
  __ bind(&tmp);
  __ Branch(not_found,
            Uless_equal,
            scratch,
            Operand(static_cast<int>('9' - '0')));

  __ bind(&not_array_index);
  // Calculate the two character string hash.
  Register hash = scratch1;
  StringHelper::GenerateHashInit(masm, hash, c1);
  StringHelper::GenerateHashAddCharacter(masm, hash, c2);
  StringHelper::GenerateHashGetHash(masm, hash);

  // Collect the two characters in a register.
  Register chars = c1;
  __ sll(scratch, c2, kBitsPerByte);
  __ Or(chars, chars, scratch);

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load symbol table.
  // Load address of first element of the symbol table.
  Register symbol_table = c2;
  __ LoadRoot(symbol_table, Heap::kSymbolTableRootIndex);

  Register undefined = scratch4;
  __ LoadRoot(undefined, Heap::kUndefinedValueRootIndex);

  // Calculate capacity mask from the symbol table capacity.
  Register mask = scratch2;
  __ lw(mask, FieldMemOperand(symbol_table, SymbolTable::kCapacityOffset));
  __ sra(mask, mask, 1);
  __ Addu(mask, mask, -1);

  // Calculate untagged address of the first element of the symbol table.
  Register first_symbol_table_element = symbol_table;
  __ Addu(first_symbol_table_element, symbol_table,
         Operand(SymbolTable::kElementsStartOffset - kHeapObjectTag));

  // Registers.
  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string
  // mask:  capacity mask
  // first_symbol_table_element: address of the first element of
  //                             the symbol table
  // undefined: the undefined object
  // scratch: -

  // Perform a number of probes in the symbol table.
  static const int kProbes = 4;
  Label found_in_symbol_table;
  Label next_probe[kProbes];
  Register candidate = scratch5;  // Scratch register contains candidate.
  for (int i = 0; i < kProbes; i++) {
    // Calculate entry in symbol table.
    if (i > 0) {
      __ Addu(candidate, hash, Operand(SymbolTable::GetProbeOffset(i)));
    } else {
      __ mov(candidate, hash);
    }

    __ And(candidate, candidate, Operand(mask));

    // Load the entry from the symble table.
    STATIC_ASSERT(SymbolTable::kEntrySize == 1);
    __ sll(scratch, candidate, kPointerSizeLog2);
    __ Addu(scratch, scratch, first_symbol_table_element);
    __ lw(candidate, MemOperand(scratch));

    // If entry is undefined no string with this hash can be found.
    Label is_string;
    __ GetObjectType(candidate, scratch, scratch);
    __ Branch(&is_string, ne, scratch, Operand(ODDBALL_TYPE));

    __ Branch(not_found, eq, undefined, Operand(candidate));
    // Must be null (deleted entry).
    if (FLAG_debug_code) {
      __ LoadRoot(scratch, Heap::kNullValueRootIndex);
      __ Assert(eq, "oddball in symbol table is not undefined or null",
          scratch, Operand(candidate));
    }
    __ jmp(&next_probe[i]);

    __ bind(&is_string);

    // Check that the candidate is a non-external ASCII string.  The instance
    // type is still in the scratch register from the CompareObjectType
    // operation.
    __ JumpIfInstanceTypeIsNotSequentialAscii(scratch, scratch, &next_probe[i]);

    // If length is not 2 the string is not a candidate.
    __ lw(scratch, FieldMemOperand(candidate, String::kLengthOffset));
    __ Branch(&next_probe[i], ne, scratch, Operand(Smi::FromInt(2)));

    // Check if the two characters match.
    // Assumes that word load is little endian.
    __ lhu(scratch, FieldMemOperand(candidate, SeqAsciiString::kHeaderSize));
    __ Branch(&found_in_symbol_table, eq, chars, Operand(scratch));
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = candidate;
  __ bind(&found_in_symbol_table);
  __ mov(v0, result);
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character) {
  // hash = seed + character + ((seed + character) << 10);
  __ LoadRoot(hash, Heap::kHashSeedRootIndex);
  // Untag smi seed and add the character.
  __ SmiUntag(hash);
  __ addu(hash, hash, character);
  __ sll(at, hash, 10);
  __ addu(hash, hash, at);
  // hash ^= hash >> 6;
  __ sra(at, hash, 6);
  __ xor_(hash, hash, at);
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character) {
  // hash += character;
  __ addu(hash, hash, character);
  // hash += hash << 10;
  __ sll(at, hash, 10);
  __ addu(hash, hash, at);
  // hash ^= hash >> 6;
  __ sra(at, hash, 6);
  __ xor_(hash, hash, at);
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash) {
  // hash += hash << 3;
  __ sll(at, hash, 3);
  __ addu(hash, hash, at);
  // hash ^= hash >> 11;
  __ sra(at, hash, 11);
  __ xor_(hash, hash, at);
  // hash += hash << 15;
  __ sll(at, hash, 15);
  __ addu(hash, hash, at);

  // if (hash == 0) hash = 27;
  __ ori(at, zero_reg, 27);
  __ movz(hash, at, hash);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label sub_string_runtime;
  // Stack frame on entry.
  //  ra: return address
  //  sp[0]: to
  //  sp[4]: from
  //  sp[8]: string

  // This stub is called from the native-call %_SubString(...), so
  // nothing can be assumed about the arguments. It is tested that:
  //  "string" is a sequential string,
  //  both "from" and "to" are smis, and
  //  0 <= from <= to <= string.length.
  // If any of these assumptions fail, we call the runtime system.

  static const int kToOffset = 0 * kPointerSize;
  static const int kFromOffset = 1 * kPointerSize;
  static const int kStringOffset = 2 * kPointerSize;

  Register to = t2;
  Register from = t3;

  // Check bounds and smi-ness.
  __ lw(to, MemOperand(sp, kToOffset));
  __ lw(from, MemOperand(sp, kFromOffset));
  STATIC_ASSERT(kFromOffset == kToOffset + 4);
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);

  __ JumpIfNotSmi(from, &sub_string_runtime);
  __ JumpIfNotSmi(to, &sub_string_runtime);

  __ sra(a3, from, kSmiTagSize);  // Remove smi tag.
  __ sra(t5, to, kSmiTagSize);  // Remove smi tag.

  // a3: from index (untagged smi)
  // t5: to index (untagged smi)

  __ Branch(&sub_string_runtime, lt, a3, Operand(zero_reg));  // From < 0.

  __ subu(a2, t5, a3);
  __ Branch(&sub_string_runtime, gt, a3, Operand(t5));  // Fail if from > to.

  // Special handling of sub-strings of length 1 and 2. One character strings
  // are handled in the runtime system (looked up in the single character
  // cache). Two character strings are looked for in the symbol cache in
  // generated code.
  __ Branch(&sub_string_runtime, lt, a2, Operand(2));

  // Both to and from are smis.

  // a2: result string length
  // a3: from index (untagged smi)
  // t2: (a.k.a. to): to (smi)
  // t3: (a.k.a. from): from offset (smi)
  // t5: to index (untagged smi)

  // Make sure first argument is a sequential (or flat) string.
  __ lw(v0, MemOperand(sp, kStringOffset));
  __ Branch(&sub_string_runtime, eq, v0, Operand(kSmiTagMask));

  __ lw(a1, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(a1, Map::kInstanceTypeOffset));
  __ And(t4, v0, Operand(kIsNotStringMask));

  __ Branch(&sub_string_runtime, ne, t4, Operand(zero_reg));

  // Short-cut for the case of trivial substring.
  Label return_v0;
  // v0: original string
  // a2: result string length
  __ lw(t0, FieldMemOperand(v0, String::kLengthOffset));
  __ sra(t0, t0, 1);
  __ Branch(&return_v0, eq, a2, Operand(t0));

  Label create_slice;
  if (FLAG_string_slices) {
    __ Branch(&create_slice, ge, a2, Operand(SlicedString::kMinLength));
  }

  // v0: original string
  // a1: instance type
  // a2: result string length
  // a3: from index (untagged smi)
  // t2: (a.k.a. to): to (smi)
  // t3: (a.k.a. from): from offset (smi)
  // t5: to index (untagged smi)

  Label seq_string;
  __ And(t0, a1, Operand(kStringRepresentationMask));
  STATIC_ASSERT(kSeqStringTag < kConsStringTag);
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kConsStringTag < kSlicedStringTag);

  // Slices and external strings go to runtime.
  __ Branch(&sub_string_runtime, gt, t0, Operand(kConsStringTag));

  // Sequential strings are handled directly.
  __ Branch(&seq_string, lt, t0, Operand(kConsStringTag));

  // Cons string. Try to recurse (once) on the first substring.
  // (This adds a little more generality than necessary to handle flattened
  // cons strings, but not much).
  __ lw(v0, FieldMemOperand(v0, ConsString::kFirstOffset));
  __ lw(t0, FieldMemOperand(v0, HeapObject::kMapOffset));
  __ lbu(a1, FieldMemOperand(t0, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kSeqStringTag == 0);
  // Cons, slices and external strings go to runtime.
  __ Branch(&sub_string_runtime, ne, a1, Operand(kStringRepresentationMask));

  // Definitly a sequential string.
  __ bind(&seq_string);

  // v0: original string
  // a1: instance type
  // a2: result string length
  // a3: from index (untagged smi)
  // t2: (a.k.a. to): to (smi)
  // t3: (a.k.a. from): from offset (smi)
  // t5: to index (untagged smi)

  __ lw(t0, FieldMemOperand(v0, String::kLengthOffset));
  __ Branch(&sub_string_runtime, lt, t0, Operand(to));  // Fail if to > length.
  to = no_reg;

  // v0: original string or left hand side of the original cons string.
  // a1: instance type
  // a2: result string length
  // a3: from index (untagged smi)
  // t3: (a.k.a. from): from offset (smi)
  // t5: to index (untagged smi)

  // Check for flat ASCII string.
  Label non_ascii_flat;
  STATIC_ASSERT(kTwoByteStringTag == 0);

  __ And(t4, a1, Operand(kStringEncodingMask));
  __ Branch(&non_ascii_flat, eq, t4, Operand(zero_reg));

  Label result_longer_than_two;
  __ Branch(&result_longer_than_two, gt, a2, Operand(2));

  // Sub string of length 2 requested.
  // Get the two characters forming the sub string.
  __ Addu(v0, v0, Operand(a3));
  __ lbu(a3, FieldMemOperand(v0, SeqAsciiString::kHeaderSize));
  __ lbu(t0, FieldMemOperand(v0, SeqAsciiString::kHeaderSize + 1));

  // Try to lookup two character string in symbol table.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, a3, t0, a1, t1, t2, t3, t4, &make_two_character_string);
  Counters* counters = masm->isolate()->counters();
  __ jmp(&return_v0);

  // a2: result string length.
  // a3: two characters combined into halfword in little endian byte order.
  __ bind(&make_two_character_string);
  __ AllocateAsciiString(v0, a2, t0, t1, t4, &sub_string_runtime);
  __ sh(a3, FieldMemOperand(v0, SeqAsciiString::kHeaderSize));
  __ jmp(&return_v0);

  __ bind(&result_longer_than_two);

  // Locate 'from' character of string.
  __ Addu(t1, v0, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ sra(t4, from, 1);
  __ Addu(t1, t1, t4);

  // Allocate the result.
  __ AllocateAsciiString(v0, a2, t4, t0, a1, &sub_string_runtime);

  // v0: result string
  // a2: result string length
  // a3: from index (untagged smi)
  // t1: first character of substring to copy
  // t3: (a.k.a. from): from offset (smi)
  // Locate first character of result.
  __ Addu(a1, v0, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));

  // v0: result string
  // a1: first character of result string
  // a2: result string length
  // t1: first character of substring to copy
  STATIC_ASSERT((SeqAsciiString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharactersLong(
      masm, a1, t1, a2, a3, t0, t2, t3, t4, COPY_ASCII | DEST_ALWAYS_ALIGNED);
  __ jmp(&return_v0);

  __ bind(&non_ascii_flat);
  // a2: result string length
  // t1: string
  // t3: (a.k.a. from): from offset (smi)
  // Check for flat two byte string.

  // Locate 'from' character of string.
  __ Addu(t1, v0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // As "from" is a smi it is 2 times the value which matches the size of a two
  // byte character.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ Addu(t1, t1, Operand(from));

  // Allocate the result.
  __ AllocateTwoByteString(v0, a2, a1, a3, t0, &sub_string_runtime);

  // v0: result string
  // a2: result string length
  // t1: first character of substring to copy
  // Locate first character of result.
  __ Addu(a1, v0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  from = no_reg;

  // v0: result string.
  // a1: first character of result.
  // a2: result length.
  // t1: first character of substring to copy.
  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharactersLong(
      masm, a1, t1, a2, a3, t0, t2, t3, t4, DEST_ALWAYS_ALIGNED);
  __ jmp(&return_v0);

  if (FLAG_string_slices) {
    __ bind(&create_slice);
    // v0: original string
    // a1: instance type
    // a2: length
    // a3: from index (untagged smi)
    // t2 (a.k.a. to): to (smi)
    // t3 (a.k.a. from): from offset (smi)
    Label allocate_slice, sliced_string, seq_string;
    STATIC_ASSERT(kSeqStringTag == 0);
    __ And(t4, a1, Operand(kStringRepresentationMask));
    __ Branch(&seq_string, eq, t4, Operand(zero_reg));
    STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
    STATIC_ASSERT(kIsIndirectStringMask != 0);
    __ And(t4, a1, Operand(kIsIndirectStringMask));
    // External string.  Jump to runtime.
    __ Branch(&sub_string_runtime, eq, t4, Operand(zero_reg));

    __ And(t4, a1, Operand(kSlicedNotConsMask));
    __ Branch(&sliced_string, ne, t4, Operand(zero_reg));
    // Cons string.  Check whether it is flat, then fetch first part.
    __ lw(t1, FieldMemOperand(v0, ConsString::kSecondOffset));
    __ LoadRoot(t5, Heap::kEmptyStringRootIndex);
    __ Branch(&sub_string_runtime, ne, t1, Operand(t5));
    __ lw(t1, FieldMemOperand(v0, ConsString::kFirstOffset));
    __ jmp(&allocate_slice);

    __ bind(&sliced_string);
    // Sliced string.  Fetch parent and correct start index by offset.
    __ lw(t1, FieldMemOperand(v0, SlicedString::kOffsetOffset));
    __ addu(t3, t3, t1);
    __ lw(t1, FieldMemOperand(v0, SlicedString::kParentOffset));
    __ jmp(&allocate_slice);

    __ bind(&seq_string);
    // Sequential string.  Just move string to the right register.
    __ mov(t1, v0);

    __ bind(&allocate_slice);
    // a1: instance type of original string
    // a2: length
    // t1: underlying subject string
    // t3 (a.k.a. from): from offset (smi)
    // Allocate new sliced string.  At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string.  It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyways due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kAsciiStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ And(t4, a1, Operand(kStringEncodingMask));
    __ Branch(&two_byte_slice, eq, t4, Operand(zero_reg));
    __ AllocateAsciiSlicedString(v0, a2, a3, t0, &sub_string_runtime);
    __ jmp(&set_slice_header);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(v0, a2, a3, t0, &sub_string_runtime);
    __ bind(&set_slice_header);
    __ sw(t3, FieldMemOperand(v0, SlicedString::kOffsetOffset));
    __ sw(t1, FieldMemOperand(v0, SlicedString::kParentOffset));
  }

  __ bind(&return_v0);
  __ IncrementCounter(counters->sub_string_native(), 1, a3, t0);
  __ Addu(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Just jump to runtime to create the sub string.
  __ bind(&sub_string_runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);
}


void StringCompareStub::GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                                      Register left,
                                                      Register right,
                                                      Register scratch1,
                                                      Register scratch2,
                                                      Register scratch3) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ lw(length, FieldMemOperand(left, String::kLengthOffset));
  __ lw(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ Branch(&check_zero_length, eq, length, Operand(scratch2));
  __ bind(&strings_not_equal);
  __ li(v0, Operand(Smi::FromInt(NOT_EQUAL)));
  __ Ret();

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&compare_chars, ne, length, Operand(zero_reg));
  __ li(v0, Operand(Smi::FromInt(EQUAL)));
  __ Ret();

  // Compare characters.
  __ bind(&compare_chars);

  GenerateAsciiCharsCompareLoop(masm,
                                left, right, length, scratch2, scratch3, v0,
                                &strings_not_equal);

  // Characters are equal.
  __ li(v0, Operand(Smi::FromInt(EQUAL)));
  __ Ret();
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4) {
  Label result_not_equal, compare_lengths;
  // Find minimum length and length difference.
  __ lw(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ lw(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ Subu(scratch3, scratch1, Operand(scratch2));
  Register length_delta = scratch3;
  __ slt(scratch4, scratch2, scratch1);
  __ movn(scratch1, scratch2, scratch4);
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&compare_lengths, eq, min_length, Operand(zero_reg));

  // Compare loop.
  GenerateAsciiCharsCompareLoop(masm,
                                left, right, min_length, scratch2, scratch4, v0,
                                &result_not_equal);

  // Compare lengths - strings up to min-length are equal.
  __ bind(&compare_lengths);
  ASSERT(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use length_delta as result if it's zero.
  __ mov(scratch2, length_delta);
  __ mov(scratch4, zero_reg);
  __ mov(v0, zero_reg);

  __ bind(&result_not_equal);
  // Conditionally update the result based either on length_delta or
  // the last comparion performed in the loop above.
  Label ret;
  __ Branch(&ret, eq, scratch2, Operand(scratch4));
  __ li(v0, Operand(Smi::FromInt(GREATER)));
  __ Branch(&ret, gt, scratch2, Operand(scratch4));
  __ li(v0, Operand(Smi::FromInt(LESS)));
  __ bind(&ret);
  __ Ret();
}


void StringCompareStub::GenerateAsciiCharsCompareLoop(
    MacroAssembler* masm,
    Register left,
    Register right,
    Register length,
    Register scratch1,
    Register scratch2,
    Register scratch3,
    Label* chars_not_equal) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ Addu(scratch1, length,
          Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ Addu(left, left, Operand(scratch1));
  __ Addu(right, right, Operand(scratch1));
  __ Subu(length, zero_reg, length);
  Register index = length;  // index = -length;


  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ Addu(scratch3, left, index);
  __ lbu(scratch1, MemOperand(scratch3));
  __ Addu(scratch3, right, index);
  __ lbu(scratch2, MemOperand(scratch3));
  __ Branch(chars_not_equal, ne, scratch1, Operand(scratch2));
  __ Addu(index, index, 1);
  __ Branch(&loop, ne, index, Operand(zero_reg));
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  Counters* counters = masm->isolate()->counters();

  // Stack frame on entry.
  //  sp[0]: right string
  //  sp[4]: left string
  __ lw(a1, MemOperand(sp, 1 * kPointerSize));  // Left.
  __ lw(a0, MemOperand(sp, 0 * kPointerSize));  // Right.

  Label not_same;
  __ Branch(&not_same, ne, a0, Operand(a1));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ li(v0, Operand(Smi::FromInt(EQUAL)));
  __ IncrementCounter(counters->string_compare_native(), 1, a1, a2);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&not_same);

  // Check that both objects are sequential ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(a1, a0, a2, a3, &runtime);

  // Compare flat ASCII strings natively. Remove arguments from stack first.
  __ IncrementCounter(counters->string_compare_native(), 1, a2, a3);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  GenerateCompareFlatAsciiStrings(masm, a1, a0, a2, a3, t0, t1);

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label string_add_runtime, call_builtin;
  Builtins::JavaScript builtin_id = Builtins::ADD;

  Counters* counters = masm->isolate()->counters();

  // Stack on entry:
  // sp[0]: second argument (right).
  // sp[4]: first argument (left).

  // Load the two arguments.
  __ lw(a0, MemOperand(sp, 1 * kPointerSize));  // First argument.
  __ lw(a1, MemOperand(sp, 0 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (flags_ == NO_STRING_ADD_FLAGS) {
    __ JumpIfEitherSmi(a0, a1, &string_add_runtime);
    // Load instance types.
    __ lw(t0, FieldMemOperand(a0, HeapObject::kMapOffset));
    __ lw(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
    __ lbu(t0, FieldMemOperand(t0, Map::kInstanceTypeOffset));
    __ lbu(t1, FieldMemOperand(t1, Map::kInstanceTypeOffset));
    STATIC_ASSERT(kStringTag == 0);
    // If either is not a string, go to runtime.
    __ Or(t4, t0, Operand(t1));
    __ And(t4, t4, Operand(kIsNotStringMask));
    __ Branch(&string_add_runtime, ne, t4, Operand(zero_reg));
  } else {
    // Here at least one of the arguments is definitely a string.
    // We convert the one that is not known to be a string.
    if ((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) != 0);
      GenerateConvertArgument(
          masm, 1 * kPointerSize, a0, a2, a3, t0, t1, &call_builtin);
      builtin_id = Builtins::STRING_ADD_RIGHT;
    } else if ((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) != 0);
      GenerateConvertArgument(
          masm, 0 * kPointerSize, a1, a2, a3, t0, t1, &call_builtin);
      builtin_id = Builtins::STRING_ADD_LEFT;
    }
  }

  // Both arguments are strings.
  // a0: first string
  // a1: second string
  // t0: first string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // t1: second string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  {
    Label strings_not_empty;
    // Check if either of the strings are empty. In that case return the other.
    // These tests use zero-length check on string-length whch is an Smi.
    // Assert that Smi::FromInt(0) is really 0.
    STATIC_ASSERT(kSmiTag == 0);
    ASSERT(Smi::FromInt(0) == 0);
    __ lw(a2, FieldMemOperand(a0, String::kLengthOffset));
    __ lw(a3, FieldMemOperand(a1, String::kLengthOffset));
    __ mov(v0, a0);       // Assume we'll return first string (from a0).
    __ movz(v0, a1, a2);  // If first is empty, return second (from a1).
    __ slt(t4, zero_reg, a2);   // if (a2 > 0) t4 = 1.
    __ slt(t5, zero_reg, a3);   // if (a3 > 0) t5 = 1.
    __ and_(t4, t4, t5);        // Branch if both strings were non-empty.
    __ Branch(&strings_not_empty, ne, t4, Operand(zero_reg));

    __ IncrementCounter(counters->string_add_native(), 1, a2, a3);
    __ Addu(sp, sp, Operand(2 * kPointerSize));
    __ Ret();

    __ bind(&strings_not_empty);
  }

  // Untag both string-lengths.
  __ sra(a2, a2, kSmiTagSize);
  __ sra(a3, a3, kSmiTagSize);

  // Both strings are non-empty.
  // a0: first string
  // a1: second string
  // a2: length of first string
  // a3: length of second string
  // t0: first string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // t1: second string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // Look at the length of the result of adding the two strings.
  Label string_add_flat_result, longer_than_two;
  // Adding two lengths can't overflow.
  STATIC_ASSERT(String::kMaxLength < String::kMaxLength * 2);
  __ Addu(t2, a2, Operand(a3));
  // Use the symbol table when adding two one character strings, as it
  // helps later optimizations to return a symbol here.
  __ Branch(&longer_than_two, ne, t2, Operand(2));

  // Check that both strings are non-external ASCII strings.
  if (flags_ != NO_STRING_ADD_FLAGS) {
    __ lw(t0, FieldMemOperand(a0, HeapObject::kMapOffset));
    __ lw(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
    __ lbu(t0, FieldMemOperand(t0, Map::kInstanceTypeOffset));
    __ lbu(t1, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  }
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(t0, t1, t2, t3,
                                                 &string_add_runtime);

  // Get the two characters forming the sub string.
  __ lbu(a2, FieldMemOperand(a0, SeqAsciiString::kHeaderSize));
  __ lbu(a3, FieldMemOperand(a1, SeqAsciiString::kHeaderSize));

  // Try to lookup two character string in symbol table. If it is not found
  // just allocate a new one.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, a2, a3, t2, t3, t0, t1, t4, &make_two_character_string);
  __ IncrementCounter(counters->string_add_native(), 1, a2, a3);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&make_two_character_string);
  // Resulting string has length 2 and first chars of two strings
  // are combined into single halfword in a2 register.
  // So we can fill resulting string without two loops by a single
  // halfword store instruction (which assumes that processor is
  // in a little endian mode).
  __ li(t2, Operand(2));
  __ AllocateAsciiString(v0, t2, t0, t1, t4, &string_add_runtime);
  __ sh(a2, FieldMemOperand(v0, SeqAsciiString::kHeaderSize));
  __ IncrementCounter(counters->string_add_native(), 1, a2, a3);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ Branch(&string_add_flat_result, lt, t2,
           Operand(String::kMinNonFlatLength));
  // Handle exceptionally long strings in the runtime system.
  STATIC_ASSERT((String::kMaxLength & 0x80000000) == 0);
  ASSERT(IsPowerOf2(String::kMaxLength + 1));
  // kMaxLength + 1 is representable as shifted literal, kMaxLength is not.
  __ Branch(&string_add_runtime, hs, t2, Operand(String::kMaxLength + 1));

  // If result is not supposed to be flat, allocate a cons string object.
  // If both strings are ASCII the result is an ASCII cons string.
  if (flags_ != NO_STRING_ADD_FLAGS) {
    __ lw(t0, FieldMemOperand(a0, HeapObject::kMapOffset));
    __ lw(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
    __ lbu(t0, FieldMemOperand(t0, Map::kInstanceTypeOffset));
    __ lbu(t1, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  }
  Label non_ascii, allocated, ascii_data;
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // Branch to non_ascii if either string-encoding field is zero (non-ascii).
  __ And(t4, t0, Operand(t1));
  __ And(t4, t4, Operand(kStringEncodingMask));
  __ Branch(&non_ascii, eq, t4, Operand(zero_reg));

  // Allocate an ASCII cons string.
  __ bind(&ascii_data);
  __ AllocateAsciiConsString(t3, t2, t0, t1, &string_add_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ sw(a0, FieldMemOperand(t3, ConsString::kFirstOffset));
  __ sw(a1, FieldMemOperand(t3, ConsString::kSecondOffset));
  __ mov(v0, t3);
  __ IncrementCounter(counters->string_add_native(), 1, a2, a3);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only ASCII characters.
  // t0: first instance type.
  // t1: second instance type.
  // Branch to if _both_ instances have kAsciiDataHintMask set.
  __ And(at, t0, Operand(kAsciiDataHintMask));
  __ and_(at, at, t1);
  __ Branch(&ascii_data, ne, at, Operand(zero_reg));

  __ xor_(t0, t0, t1);
  STATIC_ASSERT(kAsciiStringTag != 0 && kAsciiDataHintTag != 0);
  __ And(t0, t0, Operand(kAsciiStringTag | kAsciiDataHintTag));
  __ Branch(&ascii_data, eq, t0, Operand(kAsciiStringTag | kAsciiDataHintTag));

  // Allocate a two byte cons string.
  __ AllocateTwoByteConsString(t3, t2, t0, t1, &string_add_runtime);
  __ Branch(&allocated);

  // Handle creating a flat result. First check that both strings are
  // sequential and that they have the same encoding.
  // a0: first string
  // a1: second string
  // a2: length of first string
  // a3: length of second string
  // t0: first string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // t1: second string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // t2: sum of lengths.
  __ bind(&string_add_flat_result);
  if (flags_ != NO_STRING_ADD_FLAGS) {
    __ lw(t0, FieldMemOperand(a0, HeapObject::kMapOffset));
    __ lw(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
    __ lbu(t0, FieldMemOperand(t0, Map::kInstanceTypeOffset));
    __ lbu(t1, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  }
  // Check that both strings are sequential, meaning that we
  // branch to runtime if either string tag is non-zero.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ Or(t4, t0, Operand(t1));
  __ And(t4, t4, Operand(kStringRepresentationMask));
  __ Branch(&string_add_runtime, ne, t4, Operand(zero_reg));

  // Now check if both strings have the same encoding (ASCII/Two-byte).
  // a0: first string
  // a1: second string
  // a2: length of first string
  // a3: length of second string
  // t0: first string instance type
  // t1: second string instance type
  // t2: sum of lengths.
  Label non_ascii_string_add_flat_result;
  ASSERT(IsPowerOf2(kStringEncodingMask));  // Just one bit to test.
  __ xor_(t3, t1, t0);
  __ And(t3, t3, Operand(kStringEncodingMask));
  __ Branch(&string_add_runtime, ne, t3, Operand(zero_reg));
  // And see if it's ASCII (0) or two-byte (1).
  __ And(t3, t0, Operand(kStringEncodingMask));
  __ Branch(&non_ascii_string_add_flat_result, eq, t3, Operand(zero_reg));

  // Both strings are sequential ASCII strings. We also know that they are
  // short (since the sum of the lengths is less than kMinNonFlatLength).
  // t2: length of resulting flat string
  __ AllocateAsciiString(t3, t2, t0, t1, t4, &string_add_runtime);
  // Locate first character of result.
  __ Addu(t2, t3, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ Addu(a0, a0, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // a0: first character of first string.
  // a1: second string.
  // a2: length of first string.
  // a3: length of second string.
  // t2: first character of result.
  // t3: result string.
  StringHelper::GenerateCopyCharacters(masm, t2, a0, a2, t0, true);

  // Load second argument and locate first character.
  __ Addu(a1, a1, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // a1: first character of second string.
  // a3: length of second string.
  // t2: next character of result.
  // t3: result string.
  StringHelper::GenerateCopyCharacters(masm, t2, a1, a3, t0, true);
  __ mov(v0, t3);
  __ IncrementCounter(counters->string_add_native(), 1, a2, a3);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii_string_add_flat_result);
  // Both strings are sequential two byte strings.
  // a0: first string.
  // a1: second string.
  // a2: length of first string.
  // a3: length of second string.
  // t2: sum of length of strings.
  __ AllocateTwoByteString(t3, t2, t0, t1, t4, &string_add_runtime);
  // a0: first string.
  // a1: second string.
  // a2: length of first string.
  // a3: length of second string.
  // t3: result string.

  // Locate first character of result.
  __ Addu(t2, t3, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ Addu(a0, a0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // a0: first character of first string.
  // a1: second string.
  // a2: length of first string.
  // a3: length of second string.
  // t2: first character of result.
  // t3: result string.
  StringHelper::GenerateCopyCharacters(masm, t2, a0, a2, t0, false);

  // Locate first character of second argument.
  __ Addu(a1, a1, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // a1: first character of second string.
  // a3: length of second string.
  // t2: next character of result (after copy of first string).
  // t3: result string.
  StringHelper::GenerateCopyCharacters(masm, t2, a1, a3, t0, false);

  __ mov(v0, t3);
  __ IncrementCounter(counters->string_add_native(), 1, a2, a3);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  // Just jump to runtime to add the two strings.
  __ bind(&string_add_runtime);
  __ TailCallRuntime(Runtime::kStringAdd, 2, 1);

  if (call_builtin.is_linked()) {
    __ bind(&call_builtin);
    __ InvokeBuiltin(builtin_id, JUMP_FUNCTION);
  }
}


void StringAddStub::GenerateConvertArgument(MacroAssembler* masm,
                                            int stack_offset,
                                            Register arg,
                                            Register scratch1,
                                            Register scratch2,
                                            Register scratch3,
                                            Register scratch4,
                                            Label* slow) {
  // First check if the argument is already a string.
  Label not_string, done;
  __ JumpIfSmi(arg, &not_string);
  __ GetObjectType(arg, scratch1, scratch1);
  __ Branch(&done, lt, scratch1, Operand(FIRST_NONSTRING_TYPE));

  // Check the number to string cache.
  Label not_cached;
  __ bind(&not_string);
  // Puts the cached result into scratch1.
  NumberToStringStub::GenerateLookupNumberStringCache(masm,
                                                      arg,
                                                      scratch1,
                                                      scratch2,
                                                      scratch3,
                                                      scratch4,
                                                      false,
                                                      &not_cached);
  __ mov(arg, scratch1);
  __ sw(arg, MemOperand(sp, stack_offset));
  __ jmp(&done);

  // Check if the argument is a safe string wrapper.
  __ bind(&not_cached);
  __ JumpIfSmi(arg, slow);
  __ GetObjectType(arg, scratch1, scratch2);  // map -> scratch1.
  __ Branch(slow, ne, scratch2, Operand(JS_VALUE_TYPE));
  __ lbu(scratch2, FieldMemOperand(scratch1, Map::kBitField2Offset));
  __ li(scratch4, 1 << Map::kStringWrapperSafeForDefaultValueOf);
  __ And(scratch2, scratch2, scratch4);
  __ Branch(slow, ne, scratch2, Operand(scratch4));
  __ lw(arg, FieldMemOperand(arg, JSValue::kValueOffset));
  __ sw(arg, MemOperand(sp, stack_offset));

  __ bind(&done);
}


void ICCompareStub::GenerateSmis(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SMIS);
  Label miss;
  __ Or(a2, a1, a0);
  __ JumpIfNotSmi(a2, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    __ Subu(v0, a0, a1);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(a1);
    __ SmiUntag(a0);
    __ Subu(v0, a1, a0);
  }
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateHeapNumbers(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::HEAP_NUMBERS);

  Label generic_stub;
  Label unordered;
  Label miss;
  __ And(a2, a1, Operand(a0));
  __ JumpIfSmi(a2, &generic_stub);

  __ GetObjectType(a0, a2, a2);
  __ Branch(&miss, ne, a2, Operand(HEAP_NUMBER_TYPE));
  __ GetObjectType(a1, a2, a2);
  __ Branch(&miss, ne, a2, Operand(HEAP_NUMBER_TYPE));

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved or FPU is unsupported.
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);

    // Load left and right operand.
    __ Subu(a2, a1, Operand(kHeapObjectTag));
    __ ldc1(f0, MemOperand(a2, HeapNumber::kValueOffset));
    __ Subu(a2, a0, Operand(kHeapObjectTag));
    __ ldc1(f2, MemOperand(a2, HeapNumber::kValueOffset));

    Label fpu_eq, fpu_lt, fpu_gt;
    // Compare operands (test if unordered).
    __ c(UN, D, f0, f2);
    // Don't base result on status bits when a NaN is involved.
    __ bc1t(&unordered);
    __ nop();

    // Test if equal.
    __ c(EQ, D, f0, f2);
    __ bc1t(&fpu_eq);
    __ nop();

    // Test if unordered or less (unordered case is already handled).
    __ c(ULT, D, f0, f2);
    __ bc1t(&fpu_lt);
    __ nop();

    // Otherwise it's greater.
    __ bc1f(&fpu_gt);
    __ nop();

    // Return a result of -1, 0, or 1.
    __ bind(&fpu_eq);
    __ li(v0, Operand(EQUAL));
    __ Ret();

    __ bind(&fpu_lt);
    __ li(v0, Operand(LESS));
    __ Ret();

    __ bind(&fpu_gt);
    __ li(v0, Operand(GREATER));
    __ Ret();

    __ bind(&unordered);
  }

  CompareStub stub(GetCondition(), strict(), NO_COMPARE_FLAGS, a1, a0);
  __ bind(&generic_stub);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateSymbols(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SYMBOLS);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = a1;
  Register right = a0;
  Register tmp1 = a2;
  Register tmp2 = a3;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are symbols.
  __ lw(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ lw(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kSymbolTag != 0);
  __ And(tmp1, tmp1, Operand(tmp2));
  __ And(tmp1, tmp1, kIsSymbolMask);
  __ Branch(&miss, eq, tmp1, Operand(zero_reg));
  // Make sure a0 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  ASSERT(right.is(a0));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(v0, right);
  // Symbols are compared by identity.
  __ Ret(ne, left, Operand(right));
  __ li(v0, Operand(Smi::FromInt(EQUAL)));
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateStrings(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::STRINGS);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = a1;
  Register right = a0;
  Register tmp1 = a2;
  Register tmp2 = a3;
  Register tmp3 = t0;
  Register tmp4 = t1;
  Register tmp5 = t2;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ lw(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ lw(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ lbu(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ lbu(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ Or(tmp3, tmp1, tmp2);
  __ And(tmp5, tmp3, Operand(kIsNotStringMask));
  __ Branch(&miss, ne, tmp5, Operand(zero_reg));

  // Fast check for identical strings.
  Label left_ne_right;
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(&left_ne_right, ne, left, Operand(right), USE_DELAY_SLOT);
  __ mov(v0, zero_reg);  // In the delay slot.
  __ Ret();
  __ bind(&left_ne_right);

  // Handle not identical strings.

  // Check that both strings are symbols. If they are, we're done
  // because we already know they are not identical.
  ASSERT(GetCondition() == eq);
  STATIC_ASSERT(kSymbolTag != 0);
  __ And(tmp3, tmp1, Operand(tmp2));
  __ And(tmp5, tmp3, Operand(kIsSymbolMask));
  Label is_symbol;
  __ Branch(&is_symbol, eq, tmp5, Operand(zero_reg), USE_DELAY_SLOT);
  __ mov(v0, a0);  // In the delay slot.
  // Make sure a0 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  ASSERT(right.is(a0));
  __ Ret();
  __ bind(&is_symbol);

  // Check that both strings are sequential ASCII.
  Label runtime;
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(tmp1, tmp2, tmp3, tmp4,
                                                  &runtime);

  // Compare flat ASCII strings. Returns when done.
  StringCompareStub::GenerateFlatAsciiStringEquals(
      masm, left, right, tmp1, tmp2, tmp3);

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ Push(left, right);
  __ TailCallRuntime(Runtime::kStringEquals, 2, 1);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateObjects(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::OBJECTS);
  Label miss;
  __ And(a2, a1, Operand(a0));
  __ JumpIfSmi(a2, &miss);

  __ GetObjectType(a0, a2, a2);
  __ Branch(&miss, ne, a2, Operand(JS_OBJECT_TYPE));
  __ GetObjectType(a1, a2, a2);
  __ Branch(&miss, ne, a2, Operand(JS_OBJECT_TYPE));

  ASSERT(GetCondition() == eq);
  __ Subu(v0, a0, Operand(a1));
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateMiss(MacroAssembler* masm) {
  __ Push(a1, a0);
  __ push(ra);

  // Call the runtime system in a fresh internal frame.
  ExternalReference miss = ExternalReference(IC_Utility(IC::kCompareIC_Miss),
                                             masm->isolate());
  __ EnterInternalFrame();
  __ Push(a1, a0);
  __ li(t0, Operand(Smi::FromInt(op_)));
  __ push(t0);
  __ CallExternalReference(miss, 3);
  __ LeaveInternalFrame();
  // Compute the entry point of the rewritten stub.
  __ Addu(a2, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
  // Restore registers.
  __ pop(ra);
  __ pop(a0);
  __ pop(a1);
  __ Jump(a2);
}


void DirectCEntryStub::Generate(MacroAssembler* masm) {
  // No need to pop or drop anything, LeaveExitFrame will restore the old
  // stack, thus dropping the allocated space for the return value.
  // The saved ra is after the reserved stack space for the 4 args.
  __ lw(t9, MemOperand(sp, kCArgsSlotsSize));

  if (FLAG_debug_code && EnableSlowAsserts()) {
    // In case of an error the return address may point to a memory area
    // filled with kZapValue by the GC.
    // Dereference the address and check for this.
    __ lw(t0, MemOperand(t9));
    __ Assert(ne, "Received invalid return address.", t0,
        Operand(reinterpret_cast<uint32_t>(kZapValue)));
  }
  __ Jump(t9);
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    ExternalReference function) {
  __ li(t9, Operand(function));
  this->GenerateCall(masm, t9);
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    Register target) {
  __ Move(t9, target);
  __ AssertStackIsAligned();
  // Allocate space for arg slots.
  __ Subu(sp, sp, kCArgsSlotsSize);

  // Block the trampoline pool through the whole function to make sure the
  // number of generated instructions is constant.
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm);

  // We need to get the current 'pc' value, which is not available on MIPS.
  Label find_ra;
  masm->bal(&find_ra);  // ra = pc + 8.
  masm->nop();  // Branch delay slot nop.
  masm->bind(&find_ra);

  const int kNumInstructionsToJump = 6;
  masm->addiu(ra, ra, kNumInstructionsToJump * kPointerSize);
  // Push return address (accessible to GC through exit frame pc).
  // This spot for ra was reserved in EnterExitFrame.
  masm->sw(ra, MemOperand(sp, kCArgsSlotsSize));
  masm->li(ra, Operand(reinterpret_cast<intptr_t>(GetCode().location()),
                    RelocInfo::CODE_TARGET), true);
  // Call the function.
  masm->Jump(t9);
  // Make sure the stored 'ra' points to this position.
  ASSERT_EQ(kNumInstructionsToJump, masm->InstructionsGeneratedSince(&find_ra));
}


MaybeObject* StringDictionaryLookupStub::GenerateNegativeLookup(
    MacroAssembler* masm,
    Label* miss,
    Label* done,
    Register receiver,
    Register properties,
    String* name,
    Register scratch0) {
// If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  for (int i = 0; i < kInlinedProbes; i++) {
    // scratch0 points to properties hash.
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = scratch0;
    // Capacity is smi 2^n.
    __ lw(index, FieldMemOperand(properties, kCapacityOffset));
    __ Subu(index, index, Operand(1));
    __ And(index, index, Operand(
         Smi::FromInt(name->Hash() + StringDictionary::GetProbeOffset(i))));

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    // index *= 3.
    __ mov(at, index);
    __ sll(index, index, 1);
    __ Addu(index, index, at);

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    ASSERT_EQ(kSmiTagSize, 1);
    Register tmp = properties;

    __ sll(scratch0, index, 1);
    __ Addu(tmp, properties, scratch0);
    __ lw(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

    ASSERT(!tmp.is(entity_name));
    __ LoadRoot(tmp, Heap::kUndefinedValueRootIndex);
    __ Branch(done, eq, entity_name, Operand(tmp));

    if (i != kInlinedProbes - 1) {
      // Stop if found the property.
      __ Branch(miss, eq, entity_name, Operand(Handle<String>(name)));

      // Check if the entry name is not a symbol.
      __ lw(entity_name, FieldMemOperand(entity_name, HeapObject::kMapOffset));
      __ lbu(entity_name,
             FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
      __ And(scratch0, entity_name, Operand(kIsSymbolMask));
      __ Branch(miss, eq, scratch0, Operand(zero_reg));

      // Restore the properties.
      __ lw(properties,
            FieldMemOperand(receiver, JSObject::kPropertiesOffset));
    }
  }

  const int spill_mask =
      (ra.bit() | t2.bit() | t1.bit() | t0.bit() | a3.bit() |
       a2.bit() | a1.bit() | a0.bit());

  __ MultiPush(spill_mask);
  __ lw(a0, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ li(a1, Operand(Handle<String>(name)));
  StringDictionaryLookupStub stub(NEGATIVE_LOOKUP);
  MaybeObject* result = masm->TryCallStub(&stub);
  if (result->IsFailure()) return result;
  __ MultiPop(spill_mask);

  __ Branch(done, eq, v0, Operand(zero_reg));
  __ Branch(miss, ne, v0, Operand(zero_reg));
  return result;
}


// Probe the string dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found. Jump to
// the |miss| label otherwise.
// If lookup was successful |scratch2| will be equal to elements + 4 * index.
void StringDictionaryLookupStub::GeneratePositiveLookup(MacroAssembler* masm,
                                                        Label* miss,
                                                        Label* done,
                                                        Register elements,
                                                        Register name,
                                                        Register scratch1,
                                                        Register scratch2) {
  // Assert that name contains a string.
  if (FLAG_debug_code) __ AbortIfNotString(name);

  // Compute the capacity mask.
  __ lw(scratch1, FieldMemOperand(elements, kCapacityOffset));
  __ sra(scratch1, scratch1, kSmiTagSize);  // convert smi to int
  __ Subu(scratch1, scratch1, Operand(1));

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ lw(scratch2, FieldMemOperand(name, String::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      ASSERT(StringDictionary::GetProbeOffset(i) <
             1 << (32 - String::kHashFieldOffset));
      __ Addu(scratch2, scratch2, Operand(
           StringDictionary::GetProbeOffset(i) << String::kHashShift));
    }
    __ srl(scratch2, scratch2, String::kHashShift);
    __ And(scratch2, scratch1, scratch2);

    // Scale the index by multiplying by the element size.
    ASSERT(StringDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.

    __ mov(at, scratch2);
    __ sll(scratch2, scratch2, 1);
    __ Addu(scratch2, scratch2, at);

    // Check if the key is identical to the name.
    __ sll(at, scratch2, 2);
    __ Addu(scratch2, elements, at);
    __ lw(at, FieldMemOperand(scratch2, kElementsStartOffset));
    __ Branch(done, eq, name, Operand(at));
  }

  const int spill_mask =
      (ra.bit() | t2.bit() | t1.bit() | t0.bit() |
       a3.bit() | a2.bit() | a1.bit() | a0.bit()) &
      ~(scratch1.bit() | scratch2.bit());

  __ MultiPush(spill_mask);
  __ Move(a0, elements);
  __ Move(a1, name);
  StringDictionaryLookupStub stub(POSITIVE_LOOKUP);
  __ CallStub(&stub);
  __ mov(scratch2, a2);
  __ MultiPop(spill_mask);

  __ Branch(done, ne, v0, Operand(zero_reg));
  __ Branch(miss, eq, v0, Operand(zero_reg));
}


void StringDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // Registers:
  //  result: StringDictionary to probe
  //  a1: key
  //  : StringDictionary to probe.
  //  index_: will hold an index of entry if lookup is successful.
  //          might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Register result = v0;
  Register dictionary = a0;
  Register key = a1;
  Register index = a2;
  Register mask = a3;
  Register hash = t0;
  Register undefined = t1;
  Register entry_key = t2;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ lw(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ sra(mask, mask, kSmiTagSize);
  __ Subu(mask, mask, Operand(1));

  __ lw(hash, FieldMemOperand(key, String::kHashFieldOffset));

  __ LoadRoot(undefined, Heap::kUndefinedValueRootIndex);

  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    // Capacity is smi 2^n.
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      ASSERT(StringDictionary::GetProbeOffset(i) <
             1 << (32 - String::kHashFieldOffset));
      __ Addu(index, hash, Operand(
           StringDictionary::GetProbeOffset(i) << String::kHashShift));
    } else {
      __ mov(index, hash);
    }
    __ srl(index, index, String::kHashShift);
    __ And(index, mask, index);

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    // index *= 3.
    __ mov(at, index);
    __ sll(index, index, 1);
    __ Addu(index, index, at);


    ASSERT_EQ(kSmiTagSize, 1);
    __ sll(index, index, 2);
    __ Addu(index, index, dictionary);
    __ lw(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ Branch(&not_in_dictionary, eq, entry_key, Operand(undefined));

    // Stop if found the property.
    __ Branch(&in_dictionary, eq, entry_key, Operand(key));

    if (i != kTotalProbes - 1 && mode_ == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a symbol.
      __ lw(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ lbu(entry_key,
             FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
      __ And(result, entry_key, Operand(kIsSymbolMask));
      __ Branch(&maybe_in_dictionary, eq, result, Operand(zero_reg));
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode_ == POSITIVE_LOOKUP) {
    __ mov(result, zero_reg);
    __ Ret();
  }

  __ bind(&in_dictionary);
  __ li(result, 1);
  __ Ret();

  __ bind(&not_in_dictionary);
  __ mov(result, zero_reg);
  __ Ret();
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS

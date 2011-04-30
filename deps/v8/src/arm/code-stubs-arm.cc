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

#if defined(V8_TARGET_ARCH_ARM)

#include "bootstrapper.h"
#include "code-stubs.h"
#include "regexp-macro-assembler.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)

static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cond,
                                          bool never_nan_nan);
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict);
static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm, Condition cond);
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs);


void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in eax.
  Label check_heap_number, call_builtin;
  __ tst(r0, Operand(kSmiTagMask));
  __ b(ne, &check_heap_number);
  __ Ret();

  __ bind(&check_heap_number);
  __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(r1, ip);
  __ b(ne, &call_builtin);
  __ Ret();

  __ bind(&call_builtin);
  __ push(r0);
  __ InvokeBuiltin(Builtins::TO_NUMBER, JUMP_JS);
}


void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Create a new closure from the given function info in new
  // space. Set the context to the current context in cp.
  Label gc;

  // Pop the function info from the stack.
  __ pop(r3);

  // Attempt to allocate new JSFunction in new space.
  __ AllocateInNewSpace(JSFunction::kSize,
                        r0,
                        r1,
                        r2,
                        &gc,
                        TAG_OBJECT);

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ ldr(r2, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalContextOffset));
  __ ldr(r2, MemOperand(r2, Context::SlotOffset(Context::FUNCTION_MAP_INDEX)));
  __ str(r2, FieldMemOperand(r0, HeapObject::kMapOffset));

  // Initialize the rest of the function. We don't have to update the
  // write barrier because the allocated object is in new space.
  __ LoadRoot(r1, Heap::kEmptyFixedArrayRootIndex);
  __ LoadRoot(r2, Heap::kTheHoleValueRootIndex);
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
  __ str(r1, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r1, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ str(r2, FieldMemOperand(r0, JSFunction::kPrototypeOrInitialMapOffset));
  __ str(r3, FieldMemOperand(r0, JSFunction::kSharedFunctionInfoOffset));
  __ str(cp, FieldMemOperand(r0, JSFunction::kContextOffset));
  __ str(r1, FieldMemOperand(r0, JSFunction::kLiteralsOffset));
  __ str(r4, FieldMemOperand(r0, JSFunction::kNextFunctionLinkOffset));


  // Initialize the code pointer in the function to be the one
  // found in the shared function info object.
  __ ldr(r3, FieldMemOperand(r3, SharedFunctionInfo::kCodeOffset));
  __ add(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ str(r3, FieldMemOperand(r0, JSFunction::kCodeEntryOffset));

  // Return result. The argument function info has been popped already.
  __ Ret();

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ LoadRoot(r4, Heap::kFalseValueRootIndex);
  __ Push(cp, r3, r4);
  __ TailCallRuntime(Runtime::kNewClosure, 3, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;

  // Attempt to allocate the context in new space.
  __ AllocateInNewSpace(FixedArray::SizeFor(length),
                        r0,
                        r1,
                        r2,
                        &gc,
                        TAG_OBJECT);

  // Load the function from the stack.
  __ ldr(r3, MemOperand(sp, 0));

  // Setup the object header.
  __ LoadRoot(r2, Heap::kContextMapRootIndex);
  __ str(r2, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ mov(r2, Operand(Smi::FromInt(length)));
  __ str(r2, FieldMemOperand(r0, FixedArray::kLengthOffset));

  // Setup the fixed slots.
  __ mov(r1, Operand(Smi::FromInt(0)));
  __ str(r3, MemOperand(r0, Context::SlotOffset(Context::CLOSURE_INDEX)));
  __ str(r0, MemOperand(r0, Context::SlotOffset(Context::FCONTEXT_INDEX)));
  __ str(r1, MemOperand(r0, Context::SlotOffset(Context::PREVIOUS_INDEX)));
  __ str(r1, MemOperand(r0, Context::SlotOffset(Context::EXTENSION_INDEX)));

  // Copy the global object from the surrounding context.
  __ ldr(r1, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ str(r1, MemOperand(r0, Context::SlotOffset(Context::GLOBAL_INDEX)));

  // Initialize the rest of the slots to undefined.
  __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ str(r1, MemOperand(r0, Context::SlotOffset(i)));
  }

  // Remove the on-stack argument and return.
  __ mov(cp, r0);
  __ pop();
  __ Ret();

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kNewContext, 1, 1);
}


void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [sp]: constant elements.
  // [sp + kPointerSize]: literal index.
  // [sp + (2 * kPointerSize)]: literals array.

  // All sizes here are multiples of kPointerSize.
  int elements_size = (length_ > 0) ? FixedArray::SizeFor(length_) : 0;
  int size = JSArray::kSize + elements_size;

  // Load boilerplate object into r3 and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ ldr(r3, MemOperand(sp, 2 * kPointerSize));
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));
  __ add(r3, r3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r3, MemOperand(r3, r0, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r3, ip);
  __ b(eq, &slow_case);

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
    __ push(r3);
    __ ldr(r3, FieldMemOperand(r3, JSArray::kElementsOffset));
    __ ldr(r3, FieldMemOperand(r3, HeapObject::kMapOffset));
    __ LoadRoot(ip, expected_map_index);
    __ cmp(r3, ip);
    __ Assert(eq, message);
    __ pop(r3);
  }

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  __ AllocateInNewSpace(size,
                        r0,
                        r1,
                        r2,
                        &slow_case,
                        TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length_ == 0)) {
      __ ldr(r1, FieldMemOperand(r3, i));
      __ str(r1, FieldMemOperand(r0, i));
    }
  }

  if (length_ > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ ldr(r3, FieldMemOperand(r3, JSArray::kElementsOffset));
    __ add(r2, r0, Operand(JSArray::kSize));
    __ str(r2, FieldMemOperand(r0, JSArray::kElementsOffset));

    // Copy the elements array.
    __ CopyFields(r2, r3, r1.bit(), elements_size / kPointerSize);
  }

  // Return and remove the on-stack parameters.
  __ add(sp, sp, Operand(3 * kPointerSize));
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

  const char* GetName() { return "ConvertToDoubleStub"; }

#ifdef DEBUG
  void Print() { PrintF("ConvertToDoubleStub\n"); }
#endif
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
  __ mov(source_, Operand(source_, ASR, kSmiTagSize));
  // Move sign bit from source to destination.  This works because the sign bit
  // in the exponent word of the double has the same position and polarity as
  // the 2's complement sign bit in a Smi.
  STATIC_ASSERT(HeapNumber::kSignMask == 0x80000000u);
  __ and_(exponent, source_, Operand(HeapNumber::kSignMask), SetCC);
  // Subtract from 0 if source was negative.
  __ rsb(source_, source_, Operand(0, RelocInfo::NONE), LeaveCC, ne);

  // We have -1, 0 or 1, which we treat specially. Register source_ contains
  // absolute value: it is either equal to 1 (special case of -1 and 1),
  // greater than 1 (not a special case) or less than 1 (special case of 0).
  __ cmp(source_, Operand(1));
  __ b(gt, &not_special);

  // For 1 or -1 we need to or in the 0 exponent (biased to 1023).
  static const uint32_t exponent_word_for_1 =
      HeapNumber::kExponentBias << HeapNumber::kExponentShift;
  __ orr(exponent, exponent, Operand(exponent_word_for_1), LeaveCC, eq);
  // 1, 0 and -1 all have 0 for the second word.
  __ mov(mantissa, Operand(0, RelocInfo::NONE));
  __ Ret();

  __ bind(&not_special);
  // Count leading zeros.  Uses mantissa for a scratch register on pre-ARM5.
  // Gets the wrong answer for 0, but we already checked for that case above.
  __ CountLeadingZeros(zeros_, source_, mantissa);
  // Compute exponent and or it into the exponent register.
  // We use mantissa as a scratch register here.  Use a fudge factor to
  // divide the constant 31 + HeapNumber::kExponentBias, 0x41d, into two parts
  // that fit in the ARM's constant field.
  int fudge = 0x400;
  __ rsb(mantissa, zeros_, Operand(31 + HeapNumber::kExponentBias - fudge));
  __ add(mantissa, mantissa, Operand(fudge));
  __ orr(exponent,
         exponent,
         Operand(mantissa, LSL, HeapNumber::kExponentShift));
  // Shift up the source chopping the top bit off.
  __ add(zeros_, zeros_, Operand(1));
  // This wouldn't work for 1.0 or -1.0 as the shift would be 32 which means 0.
  __ mov(source_, Operand(source_, LSL, zeros_));
  // Compute lower part of fraction (last 12 bits).
  __ mov(mantissa, Operand(source_, LSL, HeapNumber::kMantissaBitsInTopWord));
  // And the top (top 20 bits).
  __ orr(exponent,
         exponent,
         Operand(source_, LSR, 32 - HeapNumber::kMantissaBitsInTopWord));
  __ Ret();
}


class FloatingPointHelper : public AllStatic {
 public:

  enum Destination {
    kVFPRegisters,
    kCoreRegisters
  };


  // Loads smis from r0 and r1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either d7 and d6 or in r2/r3 and r0/r1 respectively. If the destination is
  // floating point registers VFP3 must be supported. If core registers are
  // requested when VFP3 is supported d6 and d7 will be scratched.
  static void LoadSmis(MacroAssembler* masm,
                       Destination destination,
                       Register scratch1,
                       Register scratch2);

  // Loads objects from r0 and r1 (right and left in binary operations) into
  // floating point registers. Depending on the destination the values ends up
  // either d7 and d6 or in r2/r3 and r0/r1 respectively. If the destination is
  // floating point registers VFP3 must be supported. If core registers are
  // requested when VFP3 is supported d6 and d7 will still be scratched. If
  // either r0 or r1 is not a number (not smi and not heap number object) the
  // not_number label is jumped to with r0 and r1 intact.
  static void LoadOperands(MacroAssembler* masm,
                           FloatingPointHelper::Destination destination,
                           Register heap_number_map,
                           Register scratch1,
                           Register scratch2,
                           Label* not_number);

  // Loads the number from object into dst as a 32-bit integer if possible. If
  // the object cannot be converted to a 32-bit integer control continues at
  // the label not_int32. If VFP is supported double_scratch is used
  // but not scratch2.
  // Floating point value in the 32-bit integer range will be rounded
  // to an integer.
  static void LoadNumberAsInteger(MacroAssembler* masm,
                                  Register object,
                                  Register dst,
                                  Register heap_number_map,
                                  Register scratch1,
                                  Register scratch2,
                                  DwVfpRegister double_scratch,
                                  Label* not_int32);

  // Load the number from object into double_dst in the double format.
  // Control will jump to not_int32 if the value cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be loaded.
  static void LoadNumberAsInt32Double(MacroAssembler* masm,
                                      Register object,
                                      Destination destination,
                                      DwVfpRegister double_dst,
                                      Register dst1,
                                      Register dst2,
                                      Register heap_number_map,
                                      Register scratch1,
                                      Register scratch2,
                                      SwVfpRegister single_scratch,
                                      Label* not_int32);

  // Loads the number from object into dst as a 32-bit integer.
  // Control will jump to not_int32 if the object cannot be exactly represented
  // by a 32-bit integer.
  // Floating point value in the 32-bit integer range that are not exact integer
  // won't be converted.
  // scratch3 is not used when VFP3 is supported.
  static void LoadNumberAsInt32(MacroAssembler* masm,
                                Register object,
                                Register dst,
                                Register heap_number_map,
                                Register scratch1,
                                Register scratch2,
                                Register scratch3,
                                DwVfpRegister double_scratch,
                                Label* not_int32);

  // Generate non VFP3 code to check if a double can be exactly represented by a
  // 32-bit integer. This does not check for 0 or -0, which need
  // to be checked for separately.
  // Control jumps to not_int32 if the value is not a 32-bit integer, and falls
  // through otherwise.
  // src1 and src2 will be cloberred.
  //
  // Expected input:
  // - src1: higher (exponent) part of the double value.
  // - src2: lower (mantissa) part of the double value.
  // Output status:
  // - dst: 32 higher bits of the mantissa. (mantissa[51:20])
  // - src2: contains 1.
  // - other registers are clobbered.
  static void DoubleIs32BitInteger(MacroAssembler* masm,
                                   Register src1,
                                   Register src2,
                                   Register dst,
                                   Register scratch,
                                   Label* not_int32);

  // Generates code to call a C function to do a double operation using core
  // registers. (Used when VFP3 is not supported.)
  // This code never falls through, but returns with a heap number containing
  // the result in r0.
  // Register heapnumber_result must be a heap number in which the
  // result of the operation will be stored.
  // Requires the following layout on entry:
  // r0: Left value (least significant part of mantissa).
  // r1: Left value (sign, exponent, top of mantissa).
  // r2: Right value (least significant part of mantissa).
  // r3: Right value (sign, exponent, top of mantissa).
  static void CallCCodeForDoubleOperation(MacroAssembler* masm,
                                          Token::Value op,
                                          Register heap_number_result,
                                          Register scratch);

 private:
  static void LoadNumber(MacroAssembler* masm,
                         FloatingPointHelper::Destination destination,
                         Register object,
                         DwVfpRegister dst,
                         Register dst1,
                         Register dst2,
                         Register heap_number_map,
                         Register scratch1,
                         Register scratch2,
                         Label* not_number);
};


void FloatingPointHelper::LoadSmis(MacroAssembler* masm,
                                   FloatingPointHelper::Destination destination,
                                   Register scratch1,
                                   Register scratch2) {
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    __ mov(scratch1, Operand(r0, ASR, kSmiTagSize));
    __ vmov(d7.high(), scratch1);
    __ vcvt_f64_s32(d7, d7.high());
    __ mov(scratch1, Operand(r1, ASR, kSmiTagSize));
    __ vmov(d6.high(), scratch1);
    __ vcvt_f64_s32(d6, d6.high());
    if (destination == kCoreRegisters) {
      __ vmov(r2, r3, d7);
      __ vmov(r0, r1, d6);
    }
  } else {
    ASSERT(destination == kCoreRegisters);
    // Write Smi from r0 to r3 and r2 in double format.
    __ mov(scratch1, Operand(r0));
    ConvertToDoubleStub stub1(r3, r2, scratch1, scratch2);
    __ push(lr);
    __ Call(stub1.GetCode(), RelocInfo::CODE_TARGET);
    // Write Smi from r1 to r1 and r0 in double format.  r9 is scratch.
    __ mov(scratch1, Operand(r1));
    ConvertToDoubleStub stub2(r1, r0, scratch1, scratch2);
    __ Call(stub2.GetCode(), RelocInfo::CODE_TARGET);
    __ pop(lr);
  }
}


void FloatingPointHelper::LoadOperands(
    MacroAssembler* masm,
    FloatingPointHelper::Destination destination,
    Register heap_number_map,
    Register scratch1,
    Register scratch2,
    Label* slow) {

  // Load right operand (r0) to d6 or r2/r3.
  LoadNumber(masm, destination,
             r0, d7, r2, r3, heap_number_map, scratch1, scratch2, slow);

  // Load left operand (r1) to d7 or r0/r1.
  LoadNumber(masm, destination,
             r1, d6, r0, r1, heap_number_map, scratch1, scratch2, slow);
}


void FloatingPointHelper::LoadNumber(MacroAssembler* masm,
                                     Destination destination,
                                     Register object,
                                     DwVfpRegister dst,
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
  if (CpuFeatures::IsSupported(VFP3) && destination == kVFPRegisters) {
    CpuFeatures::Scope scope(VFP3);
    // Load the double from tagged HeapNumber to double register.
    __ sub(scratch1, object, Operand(kHeapObjectTag));
    __ vldr(dst, scratch1, HeapNumber::kValueOffset);
  } else {
    ASSERT(destination == kCoreRegisters);
    // Load the double from heap number to dst1 and dst2 in double format.
    __ Ldrd(dst1, dst2, FieldMemOperand(object, HeapNumber::kValueOffset));
  }
  __ jmp(&done);

  // Handle loading a double from a smi.
  __ bind(&is_smi);
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    // Convert smi to double using VFP instructions.
    __ SmiUntag(scratch1, object);
    __ vmov(dst.high(), scratch1);
    __ vcvt_f64_s32(dst, dst.high());
    if (destination == kCoreRegisters) {
      // Load the converted smi to dst1 and dst2 in double format.
      __ vmov(dst1, dst2, dst);
    }
  } else {
    ASSERT(destination == kCoreRegisters);
    // Write smi to dst1 and dst2 double format.
    __ mov(scratch1, Operand(object));
    ConvertToDoubleStub stub(dst2, dst1, scratch1, scratch2);
    __ push(lr);
    __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
    __ pop(lr);
  }

  __ bind(&done);
}


void FloatingPointHelper::LoadNumberAsInteger(MacroAssembler* masm,
                                              Register object,
                                              Register dst,
                                              Register heap_number_map,
                                              Register scratch1,
                                              Register scratch2,
                                              DwVfpRegister double_scratch,
                                              Label* not_int32) {
  if (FLAG_debug_code) {
    __ AbortIfNotRootValue(heap_number_map,
                           Heap::kHeapNumberMapRootIndex,
                           "HeapNumberMap register clobbered.");
  }
  Label is_smi, done;
  __ JumpIfSmi(object, &is_smi);
  __ ldr(scratch1, FieldMemOperand(object, HeapNumber::kMapOffset));
  __ cmp(scratch1, heap_number_map);
  __ b(ne, not_int32);
  __ ConvertToInt32(
      object, dst, scratch1, scratch2, double_scratch, not_int32);
  __ jmp(&done);
  __ bind(&is_smi);
  __ SmiUntag(dst, object);
  __ bind(&done);
}


void FloatingPointHelper::LoadNumberAsInt32Double(MacroAssembler* masm,
                                                  Register object,
                                                  Destination destination,
                                                  DwVfpRegister double_dst,
                                                  Register dst1,
                                                  Register dst2,
                                                  Register heap_number_map,
                                                  Register scratch1,
                                                  Register scratch2,
                                                  SwVfpRegister single_scratch,
                                                  Label* not_int32) {
  ASSERT(!scratch1.is(object) && !scratch2.is(object));
  ASSERT(!scratch1.is(scratch2));
  ASSERT(!heap_number_map.is(object) &&
         !heap_number_map.is(scratch1) &&
         !heap_number_map.is(scratch2));

  Label done, obj_is_not_smi;

  __ JumpIfNotSmi(object, &obj_is_not_smi);
  __ SmiUntag(scratch1, object);
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    __ vmov(single_scratch, scratch1);
    __ vcvt_f64_s32(double_dst, single_scratch);
    if (destination == kCoreRegisters) {
      __ vmov(dst1, dst2, double_dst);
    }
  } else {
    Label fewer_than_20_useful_bits;
    // Expected output:
    // |         dst1            |         dst2            |
    // | s |   exp   |              mantissa               |

    // Check for zero.
    __ cmp(scratch1, Operand(0));
    __ mov(dst1, scratch1);
    __ mov(dst2, scratch1);
    __ b(eq, &done);

    // Preload the sign of the value.
    __ and_(dst1, scratch1, Operand(HeapNumber::kSignMask), SetCC);
    // Get the absolute value of the object (as an unsigned integer).
    __ rsb(scratch1, scratch1, Operand(0), SetCC, mi);

    // Get mantisssa[51:20].

    // Get the position of the first set bit.
    __ CountLeadingZeros(dst2, scratch1, scratch2);
    __ rsb(dst2, dst2, Operand(31));

    // Set the exponent.
    __ add(scratch2, dst2, Operand(HeapNumber::kExponentBias));
    __ Bfi(dst1, scratch2, scratch2,
        HeapNumber::kExponentShift, HeapNumber::kExponentBits);

    // Clear the first non null bit.
    __ mov(scratch2, Operand(1));
    __ bic(scratch1, scratch1, Operand(scratch2, LSL, dst2));

    __ cmp(dst2, Operand(HeapNumber::kMantissaBitsInTopWord));
    // Get the number of bits to set in the lower part of the mantissa.
    __ sub(scratch2, dst2, Operand(HeapNumber::kMantissaBitsInTopWord), SetCC);
    __ b(mi, &fewer_than_20_useful_bits);
    // Set the higher 20 bits of the mantissa.
    __ orr(dst1, dst1, Operand(scratch1, LSR, scratch2));
    __ rsb(scratch2, scratch2, Operand(32));
    __ mov(dst2, Operand(scratch1, LSL, scratch2));
    __ b(&done);

    __ bind(&fewer_than_20_useful_bits);
    __ rsb(scratch2, dst2, Operand(HeapNumber::kMantissaBitsInTopWord));
    __ mov(scratch2, Operand(scratch1, LSL, scratch2));
    __ orr(dst1, dst1, scratch2);
    // Set dst2 to 0.
    __ mov(dst2, Operand(0));
  }

  __ b(&done);

  __ bind(&obj_is_not_smi);
  if (FLAG_debug_code) {
    __ AbortIfNotRootValue(heap_number_map,
                           Heap::kHeapNumberMapRootIndex,
                           "HeapNumberMap register clobbered.");
  }
  __ JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_int32);

  // Load the number.
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    // Load the double value.
    __ sub(scratch1, object, Operand(kHeapObjectTag));
    __ vldr(double_dst, scratch1, HeapNumber::kValueOffset);

    __ EmitVFPTruncate(kRoundToZero,
                       single_scratch,
                       double_dst,
                       scratch1,
                       scratch2,
                       kCheckForInexactConversion);

    // Jump to not_int32 if the operation did not succeed.
    __ b(ne, not_int32);

    if (destination == kCoreRegisters) {
      __ vmov(dst1, dst2, double_dst);
    }

  } else {
    ASSERT(!scratch1.is(object) && !scratch2.is(object));
    // Load the double value in the destination registers..
    __ Ldrd(dst1, dst2, FieldMemOperand(object, HeapNumber::kValueOffset));

    // Check for 0 and -0.
    __ bic(scratch1, dst1, Operand(HeapNumber::kSignMask));
    __ orr(scratch1, scratch1, Operand(dst2));
    __ cmp(scratch1, Operand(0));
    __ b(eq, &done);

    // Check that the value can be exactly represented by a 32-bit integer.
    // Jump to not_int32 if that's not the case.
    DoubleIs32BitInteger(masm, dst1, dst2, scratch1, scratch2, not_int32);

    // dst1 and dst2 were trashed. Reload the double value.
    __ Ldrd(dst1, dst2, FieldMemOperand(object, HeapNumber::kValueOffset));
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
                                            DwVfpRegister double_scratch,
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
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    SwVfpRegister single_scratch = double_scratch.low();
    // Load the double value.
    __ sub(scratch1, object, Operand(kHeapObjectTag));
    __ vldr(double_scratch, scratch1, HeapNumber::kValueOffset);

    __ EmitVFPTruncate(kRoundToZero,
                       single_scratch,
                       double_scratch,
                       scratch1,
                       scratch2,
                       kCheckForInexactConversion);

    // Jump to not_int32 if the operation did not succeed.
    __ b(ne, not_int32);
    // Get the result in the destination register.
    __ vmov(dst, single_scratch);

  } else {
    // Load the double value in the destination registers.
    __ ldr(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
    __ ldr(scratch2, FieldMemOperand(object, HeapNumber::kMantissaOffset));

    // Check for 0 and -0.
    __ bic(dst, scratch1, Operand(HeapNumber::kSignMask));
    __ orr(dst, scratch2, Operand(dst));
    __ cmp(dst, Operand(0));
    __ b(eq, &done);

    DoubleIs32BitInteger(masm, scratch1, scratch2, dst, scratch3, not_int32);

    // Registers state after DoubleIs32BitInteger.
    // dst: mantissa[51:20].
    // scratch2: 1

    // Shift back the higher bits of the mantissa.
    __ mov(dst, Operand(dst, LSR, scratch3));
    // Set the implicit first bit.
    __ rsb(scratch3, scratch3, Operand(32));
    __ orr(dst, dst, Operand(scratch2, LSL, scratch3));
    // Set the sign.
    __ ldr(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
    __ tst(scratch1, Operand(HeapNumber::kSignMask));
    __ rsb(dst, dst, Operand(0), LeaveCC, mi);
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
  __ Ubfx(scratch,
          src1,
          HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);

  // Substract the bias from the exponent.
  __ sub(scratch, scratch, Operand(HeapNumber::kExponentBias), SetCC);

  // src1: higher (exponent) part of the double value.
  // src2: lower (mantissa) part of the double value.
  // scratch: unbiased exponent.

  // Fast cases. Check for obvious non 32-bit integer values.
  // Negative exponent cannot yield 32-bit integers.
  __ b(mi, not_int32);
  // Exponent greater than 31 cannot yield 32-bit integers.
  // Also, a positive value with an exponent equal to 31 is outside of the
  // signed 32-bit integer range.
  __ tst(src1, Operand(HeapNumber::kSignMask));
  __ cmp(scratch, Operand(30), eq);  // Executed for positive. If exponent is 30
                                     // the gt condition will be "correct" and
                                     // the next instruction will be skipped.
  __ cmp(scratch, Operand(31), ne);  // Executed for negative and positive where
                                     // exponent is not 30.
  __ b(gt, not_int32);
  // - Bits [21:0] in the mantissa are not null.
  __ tst(src2, Operand(0x3fffff));
  __ b(ne, not_int32);

  // Otherwise the exponent needs to be big enough to shift left all the
  // non zero bits left. So we need the (30 - exponent) last bits of the
  // 31 higher bits of the mantissa to be null.
  // Because bits [21:0] are null, we can check instead that the
  // (32 - exponent) last bits of the 32 higher bits of the mantisssa are null.

  // Get the 32 higher bits of the mantissa in dst.
  __ Ubfx(dst,
          src2,
          HeapNumber::kMantissaBitsInTopWord,
          32 - HeapNumber::kMantissaBitsInTopWord);
  __ orr(dst,
         dst,
         Operand(src1, LSL, HeapNumber::kNonMantissaBitsInTopWord));

  // Create the mask and test the lower bits (of the higher bits).
  __ rsb(scratch, scratch, Operand(32));
  __ mov(src2, Operand(1));
  __ mov(src1, Operand(src2, LSL, scratch));
  __ sub(src1, src1, Operand(1));
  __ tst(dst, src1);
  __ b(ne, not_int32);
}


void FloatingPointHelper::CallCCodeForDoubleOperation(
    MacroAssembler* masm,
    Token::Value op,
    Register heap_number_result,
    Register scratch) {
  // Using core registers:
  // r0: Left value (least significant part of mantissa).
  // r1: Left value (sign, exponent, top of mantissa).
  // r2: Right value (least significant part of mantissa).
  // r3: Right value (sign, exponent, top of mantissa).

  // Assert that heap_number_result is callee-saved.
  // We currently always use r5 to pass it.
  ASSERT(heap_number_result.is(r5));

  // Push the current return address before the C call. Return will be
  // through pop(pc) below.
  __ push(lr);
  __ PrepareCallCFunction(4, scratch);  // Two doubles are 4 arguments.
  // Call C routine that may not cause GC or other trouble.
  __ CallCFunction(ExternalReference::double_fp_operation(op), 4);
  // Store answer in the overwritable heap number.
#if !defined(USE_ARM_EABI)
  // Double returned in fp coprocessor register 0 and 1, encoded as
  // register cr8.  Offsets must be divisible by 4 for coprocessor so we
  // need to substract the tag from heap_number_result.
  __ sub(scratch, heap_number_result, Operand(kHeapObjectTag));
  __ stc(p1, cr8, MemOperand(scratch, HeapNumber::kValueOffset));
#else
  // Double returned in registers 0 and 1.
  __ Strd(r0, r1, FieldMemOperand(heap_number_result,
                                  HeapNumber::kValueOffset));
#endif
  // Place heap_number_result in r0 and return to the pushed return address.
  __ mov(r0, Operand(heap_number_result));
  __ pop(pc);
}


// See comment for class.
void WriteInt32ToHeapNumberStub::Generate(MacroAssembler* masm) {
  Label max_negative_int;
  // the_int_ has the answer which is a signed int32 but not a Smi.
  // We test for the special value that has a different exponent.  This test
  // has the neat side effect of setting the flags according to the sign.
  STATIC_ASSERT(HeapNumber::kSignMask == 0x80000000u);
  __ cmp(the_int_, Operand(0x80000000u));
  __ b(eq, &max_negative_int);
  // Set up the correct exponent in scratch_.  All non-Smi int32s have the same.
  // A non-Smi integer is 1.xxx * 2^30 so the exponent is 30 (biased).
  uint32_t non_smi_exponent =
      (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
  __ mov(scratch_, Operand(non_smi_exponent));
  // Set the sign bit in scratch_ if the value was negative.
  __ orr(scratch_, scratch_, Operand(HeapNumber::kSignMask), LeaveCC, cs);
  // Subtract from 0 if the value was negative.
  __ rsb(the_int_, the_int_, Operand(0, RelocInfo::NONE), LeaveCC, cs);
  // We should be masking the implict first digit of the mantissa away here,
  // but it just ends up combining harmlessly with the last digit of the
  // exponent that happens to be 1.  The sign bit is 0 so we shift 10 to get
  // the most significant 1 to hit the last bit of the 12 bit sign and exponent.
  ASSERT(((1 << HeapNumber::kExponentShift) & non_smi_exponent) != 0);
  const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
  __ orr(scratch_, scratch_, Operand(the_int_, LSR, shift_distance));
  __ str(scratch_, FieldMemOperand(the_heap_number_,
                                   HeapNumber::kExponentOffset));
  __ mov(scratch_, Operand(the_int_, LSL, 32 - shift_distance));
  __ str(scratch_, FieldMemOperand(the_heap_number_,
                                   HeapNumber::kMantissaOffset));
  __ Ret();

  __ bind(&max_negative_int);
  // The max negative int32 is stored as a positive number in the mantissa of
  // a double because it uses a sign bit instead of using two's complement.
  // The actual mantissa bits stored are all 0 because the implicit most
  // significant 1 bit is not stored.
  non_smi_exponent += 1 << HeapNumber::kExponentShift;
  __ mov(ip, Operand(HeapNumber::kSignMask | non_smi_exponent));
  __ str(ip, FieldMemOperand(the_heap_number_, HeapNumber::kExponentOffset));
  __ mov(ip, Operand(0, RelocInfo::NONE));
  __ str(ip, FieldMemOperand(the_heap_number_, HeapNumber::kMantissaOffset));
  __ Ret();
}


// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cond,
                                          bool never_nan_nan) {
  Label not_identical;
  Label heap_number, return_equal;
  __ cmp(r0, r1);
  __ b(ne, &not_identical);

  // The two objects are identical.  If we know that one of them isn't NaN then
  // we now know they test equal.
  if (cond != eq || !never_nan_nan) {
    // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
    // so we do the second best thing - test it ourselves.
    // They are both equal and they are not both Smis so both of them are not
    // Smis.  If it's not a heap number, then return equal.
    if (cond == lt || cond == gt) {
      __ CompareObjectType(r0, r4, r4, FIRST_JS_OBJECT_TYPE);
      __ b(ge, slow);
    } else {
      __ CompareObjectType(r0, r4, r4, HEAP_NUMBER_TYPE);
      __ b(eq, &heap_number);
      // Comparing JS objects with <=, >= is complicated.
      if (cond != eq) {
        __ cmp(r4, Operand(FIRST_JS_OBJECT_TYPE));
        __ b(ge, slow);
        // Normally here we fall through to return_equal, but undefined is
        // special: (undefined == undefined) == true, but
        // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
        if (cond == le || cond == ge) {
          __ cmp(r4, Operand(ODDBALL_TYPE));
          __ b(ne, &return_equal);
          __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
          __ cmp(r0, r2);
          __ b(ne, &return_equal);
          if (cond == le) {
            // undefined <= undefined should fail.
            __ mov(r0, Operand(GREATER));
          } else  {
            // undefined >= undefined should fail.
            __ mov(r0, Operand(LESS));
          }
          __ Ret();
        }
      }
    }
  }

  __ bind(&return_equal);
  if (cond == lt) {
    __ mov(r0, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cond == gt) {
    __ mov(r0, Operand(LESS));     // Things aren't greater than themselves.
  } else {
    __ mov(r0, Operand(EQUAL));    // Things are <=, >=, ==, === themselves.
  }
  __ Ret();

  if (cond != eq || !never_nan_nan) {
    // For less and greater we don't have to check for NaN since the result of
    // x < x is false regardless.  For the others here is some code to check
    // for NaN.
    if (cond != lt && cond != gt) {
      __ bind(&heap_number);
      // It is a heap number, so return non-equal if it's NaN and equal if it's
      // not NaN.

      // The representation of NaN values has all exponent bits (52..62) set,
      // and not all mantissa bits (0..51) clear.
      // Read top bits of double representation (second word of value).
      __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
      // Test that exponent bits are all set.
      __ Sbfx(r3, r2, HeapNumber::kExponentShift, HeapNumber::kExponentBits);
      // NaNs have all-one exponents so they sign extend to -1.
      __ cmp(r3, Operand(-1));
      __ b(ne, &return_equal);

      // Shift out flag and all exponent bits, retaining only mantissa.
      __ mov(r2, Operand(r2, LSL, HeapNumber::kNonMantissaBitsInTopWord));
      // Or with all low-bits of mantissa.
      __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
      __ orr(r0, r3, Operand(r2), SetCC);
      // For equal we already have the right value in r0:  Return zero (equal)
      // if all bits in mantissa are zero (it's an Infinity) and non-zero if
      // not (it's a NaN).  For <= and >= we need to load r0 with the failing
      // value if it's a NaN.
      if (cond != eq) {
        // All-zero means Infinity means equal.
        __ Ret(eq);
        if (cond == le) {
          __ mov(r0, Operand(GREATER));  // NaN <= NaN should fail.
        } else {
          __ mov(r0, Operand(LESS));     // NaN >= NaN should fail.
        }
      }
      __ Ret();
    }
    // No fall through here.
  }

  __ bind(&not_identical);
}


// See comment at call site.
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict) {
  ASSERT((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  Label rhs_is_smi;
  __ tst(rhs, Operand(kSmiTagMask));
  __ b(eq, &rhs_is_smi);

  // Lhs is a Smi.  Check whether the rhs is a heap number.
  __ CompareObjectType(rhs, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If rhs is not a number and lhs is a Smi then strict equality cannot
    // succeed.  Return non-equal
    // If rhs is r0 then there is already a non zero value in it.
    if (!rhs.is(r0)) {
      __ mov(r0, Operand(NOT_EQUAL), LeaveCC, ne);
    }
    __ Ret(ne);
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Lhs is a smi, rhs is a number.
  if (CpuFeatures::IsSupported(VFP3)) {
    // Convert lhs to a double in d7.
    CpuFeatures::Scope scope(VFP3);
    __ SmiToDoubleVFPRegister(lhs, d7, r7, s15);
    // Load the double from rhs, tagged HeapNumber r0, to d6.
    __ sub(r7, rhs, Operand(kHeapObjectTag));
    __ vldr(d6, r7, HeapNumber::kValueOffset);
  } else {
    __ push(lr);
    // Convert lhs to a double in r2, r3.
    __ mov(r7, Operand(lhs));
    ConvertToDoubleStub stub1(r3, r2, r7, r6);
    __ Call(stub1.GetCode(), RelocInfo::CODE_TARGET);
    // Load rhs to a double in r0, r1.
    __ Ldrd(r0, r1, FieldMemOperand(rhs, HeapNumber::kValueOffset));
    __ pop(lr);
  }

  // We now have both loaded as doubles but we can skip the lhs nan check
  // since it's a smi.
  __ jmp(lhs_not_nan);

  __ bind(&rhs_is_smi);
  // Rhs is a smi.  Check whether the non-smi lhs is a heap number.
  __ CompareObjectType(lhs, r4, r4, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs is not a number and rhs is a smi then strict equality cannot
    // succeed.  Return non-equal.
    // If lhs is r0 then there is already a non zero value in it.
    if (!lhs.is(r0)) {
      __ mov(r0, Operand(NOT_EQUAL), LeaveCC, ne);
    }
    __ Ret(ne);
  } else {
    // Smi compared non-strictly with a non-smi non-heap-number.  Call
    // the runtime.
    __ b(ne, slow);
  }

  // Rhs is a smi, lhs is a heap number.
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    // Load the double from lhs, tagged HeapNumber r1, to d7.
    __ sub(r7, lhs, Operand(kHeapObjectTag));
    __ vldr(d7, r7, HeapNumber::kValueOffset);
    // Convert rhs to a double in d6              .
    __ SmiToDoubleVFPRegister(rhs, d6, r7, s13);
  } else {
    __ push(lr);
    // Load lhs to a double in r2, r3.
    __ Ldrd(r2, r3, FieldMemOperand(lhs, HeapNumber::kValueOffset));
    // Convert rhs to a double in r0, r1.
    __ mov(r7, Operand(rhs));
    ConvertToDoubleStub stub2(r1, r0, r7, r6);
    __ Call(stub2.GetCode(), RelocInfo::CODE_TARGET);
    __ pop(lr);
  }
  // Fall through to both_loaded_as_doubles.
}


void EmitNanCheck(MacroAssembler* masm, Label* lhs_not_nan, Condition cond) {
  bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
  Register rhs_exponent = exp_first ? r0 : r1;
  Register lhs_exponent = exp_first ? r2 : r3;
  Register rhs_mantissa = exp_first ? r1 : r0;
  Register lhs_mantissa = exp_first ? r3 : r2;
  Label one_is_nan, neither_is_nan;

  __ Sbfx(r4,
          lhs_exponent,
          HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);
  // NaNs have all-one exponents so they sign extend to -1.
  __ cmp(r4, Operand(-1));
  __ b(ne, lhs_not_nan);
  __ mov(r4,
         Operand(lhs_exponent, LSL, HeapNumber::kNonMantissaBitsInTopWord),
         SetCC);
  __ b(ne, &one_is_nan);
  __ cmp(lhs_mantissa, Operand(0, RelocInfo::NONE));
  __ b(ne, &one_is_nan);

  __ bind(lhs_not_nan);
  __ Sbfx(r4,
          rhs_exponent,
          HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);
  // NaNs have all-one exponents so they sign extend to -1.
  __ cmp(r4, Operand(-1));
  __ b(ne, &neither_is_nan);
  __ mov(r4,
         Operand(rhs_exponent, LSL, HeapNumber::kNonMantissaBitsInTopWord),
         SetCC);
  __ b(ne, &one_is_nan);
  __ cmp(rhs_mantissa, Operand(0, RelocInfo::NONE));
  __ b(eq, &neither_is_nan);

  __ bind(&one_is_nan);
  // NaN comparisons always fail.
  // Load whatever we need in r0 to make the comparison fail.
  if (cond == lt || cond == le) {
    __ mov(r0, Operand(GREATER));
  } else {
    __ mov(r0, Operand(LESS));
  }
  __ Ret();

  __ bind(&neither_is_nan);
}


// See comment at call site.
static void EmitTwoNonNanDoubleComparison(MacroAssembler* masm,
                                          Condition cond) {
  bool exp_first = (HeapNumber::kExponentOffset == HeapNumber::kValueOffset);
  Register rhs_exponent = exp_first ? r0 : r1;
  Register lhs_exponent = exp_first ? r2 : r3;
  Register rhs_mantissa = exp_first ? r1 : r0;
  Register lhs_mantissa = exp_first ? r3 : r2;

  // r0, r1, r2, r3 have the two doubles.  Neither is a NaN.
  if (cond == eq) {
    // Doubles are not equal unless they have the same bit pattern.
    // Exception: 0 and -0.
    __ cmp(rhs_mantissa, Operand(lhs_mantissa));
    __ orr(r0, rhs_mantissa, Operand(lhs_mantissa), LeaveCC, ne);
    // Return non-zero if the numbers are unequal.
    __ Ret(ne);

    __ sub(r0, rhs_exponent, Operand(lhs_exponent), SetCC);
    // If exponents are equal then return 0.
    __ Ret(eq);

    // Exponents are unequal.  The only way we can return that the numbers
    // are equal is if one is -0 and the other is 0.  We already dealt
    // with the case where both are -0 or both are 0.
    // We start by seeing if the mantissas (that are equal) or the bottom
    // 31 bits of the rhs exponent are non-zero.  If so we return not
    // equal.
    __ orr(r4, lhs_mantissa, Operand(lhs_exponent, LSL, kSmiTagSize), SetCC);
    __ mov(r0, Operand(r4), LeaveCC, ne);
    __ Ret(ne);
    // Now they are equal if and only if the lhs exponent is zero in its
    // low 31 bits.
    __ mov(r0, Operand(rhs_exponent, LSL, kSmiTagSize));
    __ Ret();
  } else {
    // Call a native function to do a comparison between two non-NaNs.
    // Call C routine that may not cause GC or other trouble.
    __ push(lr);
    __ PrepareCallCFunction(4, r5);  // Two doubles count as 4 arguments.
    __ CallCFunction(ExternalReference::compare_doubles(), 4);
    __ pop(pc);  // Return.
  }
}


// See comment at call site.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs) {
    ASSERT((lhs.is(r0) && rhs.is(r1)) ||
           (lhs.is(r1) && rhs.is(r0)));

    // If either operand is a JSObject or an oddball value, then they are
    // not equal since their pointers are different.
    // There is no test for undetectability in strict equality.
    STATIC_ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
    Label first_non_object;
    // Get the type of the first operand into r2 and compare it with
    // FIRST_JS_OBJECT_TYPE.
    __ CompareObjectType(rhs, r2, r2, FIRST_JS_OBJECT_TYPE);
    __ b(lt, &first_non_object);

    // Return non-zero (r0 is not zero)
    Label return_not_equal;
    __ bind(&return_not_equal);
    __ Ret();

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ cmp(r2, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);

    __ CompareObjectType(lhs, r3, r3, FIRST_JS_OBJECT_TYPE);
    __ b(ge, &return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ cmp(r3, Operand(ODDBALL_TYPE));
    __ b(eq, &return_not_equal);

    // Now that we have the types we might as well check for symbol-symbol.
    // Ensure that no non-strings have the symbol bit set.
    STATIC_ASSERT(LAST_TYPE < kNotStringTag + kIsSymbolMask);
    STATIC_ASSERT(kSymbolTag != 0);
    __ and_(r2, r2, Operand(r3));
    __ tst(r2, Operand(kIsSymbolMask));
    __ b(ne, &return_not_equal);
}


// See comment at call site.
static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm,
                                       Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers,
                                       Label* slow) {
  ASSERT((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  __ CompareObjectType(rhs, r3, r2, HEAP_NUMBER_TYPE);
  __ b(ne, not_heap_numbers);
  __ ldr(r2, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ cmp(r2, r3);
  __ b(ne, slow);  // First was a heap number, second wasn't.  Go slow case.

  // Both are heap numbers.  Load them up then jump to the code we have
  // for that.
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);
    __ sub(r7, rhs, Operand(kHeapObjectTag));
    __ vldr(d6, r7, HeapNumber::kValueOffset);
    __ sub(r7, lhs, Operand(kHeapObjectTag));
    __ vldr(d7, r7, HeapNumber::kValueOffset);
  } else {
    __ Ldrd(r2, r3, FieldMemOperand(lhs, HeapNumber::kValueOffset));
    __ Ldrd(r0, r1, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  }
  __ jmp(both_loaded_as_doubles);
}


// Fast negative check for symbol-to-symbol equality.
static void EmitCheckForSymbolsOrObjects(MacroAssembler* masm,
                                         Register lhs,
                                         Register rhs,
                                         Label* possible_strings,
                                         Label* not_both_strings) {
  ASSERT((lhs.is(r0) && rhs.is(r1)) ||
         (lhs.is(r1) && rhs.is(r0)));

  // r2 is object type of rhs.
  // Ensure that no non-strings have the symbol bit set.
  Label object_test;
  STATIC_ASSERT(kSymbolTag != 0);
  __ tst(r2, Operand(kIsNotStringMask));
  __ b(ne, &object_test);
  __ tst(r2, Operand(kIsSymbolMask));
  __ b(eq, possible_strings);
  __ CompareObjectType(lhs, r3, r3, FIRST_NONSTRING_TYPE);
  __ b(ge, not_both_strings);
  __ tst(r3, Operand(kIsSymbolMask));
  __ b(eq, possible_strings);

  // Both are symbols.  We already checked they weren't the same pointer
  // so they are not equal.
  __ mov(r0, Operand(NOT_EQUAL));
  __ Ret();

  __ bind(&object_test);
  __ cmp(r2, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(lt, not_both_strings);
  __ CompareObjectType(lhs, r2, r3, FIRST_JS_OBJECT_TYPE);
  __ b(lt, not_both_strings);
  // If both objects are undetectable, they are equal. Otherwise, they
  // are not equal, since they are different objects and an object is not
  // equal to undefined.
  __ ldr(r3, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ ldrb(r2, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ ldrb(r3, FieldMemOperand(r3, Map::kBitFieldOffset));
  __ and_(r0, r2, Operand(r3));
  __ and_(r0, r0, Operand(1 << Map::kIsUndetectable));
  __ eor(r0, r0, Operand(1 << Map::kIsUndetectable));
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
  __ ldr(mask, FieldMemOperand(number_string_cache, FixedArray::kLengthOffset));
  // Divide length by two (length is a smi).
  __ mov(mask, Operand(mask, ASR, kSmiTagSize + 1));
  __ sub(mask, mask, Operand(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  if (!object_is_smi) {
    __ JumpIfSmi(object, &is_smi);
    if (CpuFeatures::IsSupported(VFP3)) {
      CpuFeatures::Scope scope(VFP3);
      __ CheckMap(object,
                  scratch1,
                  Heap::kHeapNumberMapRootIndex,
                  not_found,
                  true);

      STATIC_ASSERT(8 == kDoubleSize);
      __ add(scratch1,
             object,
             Operand(HeapNumber::kValueOffset - kHeapObjectTag));
      __ ldm(ia, scratch1, scratch1.bit() | scratch2.bit());
      __ eor(scratch1, scratch1, Operand(scratch2));
      __ and_(scratch1, scratch1, Operand(mask));

      // Calculate address of entry in string cache: each entry consists
      // of two pointer sized fields.
      __ add(scratch1,
             number_string_cache,
             Operand(scratch1, LSL, kPointerSizeLog2 + 1));

      Register probe = mask;
      __ ldr(probe,
             FieldMemOperand(scratch1, FixedArray::kHeaderSize));
      __ JumpIfSmi(probe, not_found);
      __ sub(scratch2, object, Operand(kHeapObjectTag));
      __ vldr(d0, scratch2, HeapNumber::kValueOffset);
      __ sub(probe, probe, Operand(kHeapObjectTag));
      __ vldr(d1, probe, HeapNumber::kValueOffset);
      __ VFPCompareAndSetFlags(d0, d1);
      __ b(ne, not_found);  // The cache did not contain this value.
      __ b(&load_result_from_cache);
    } else {
      __ b(not_found);
    }
  }

  __ bind(&is_smi);
  Register scratch = scratch1;
  __ and_(scratch, mask, Operand(object, ASR, 1));
  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  __ add(scratch,
         number_string_cache,
         Operand(scratch, LSL, kPointerSizeLog2 + 1));

  // Check if the entry is the smi we are looking for.
  Register probe = mask;
  __ ldr(probe, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  __ cmp(object, probe);
  __ b(ne, not_found);

  // Get the result from the cache.
  __ bind(&load_result_from_cache);
  __ ldr(result,
         FieldMemOperand(scratch, FixedArray::kHeaderSize + kPointerSize));
  __ IncrementCounter(&Counters::number_to_string_native,
                      1,
                      scratch1,
                      scratch2);
}


void NumberToStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  __ ldr(r1, MemOperand(sp, 0));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, r1, r0, r2, r3, r4, false, &runtime);
  __ add(sp, sp, Operand(1 * kPointerSize));
  __ Ret();

  __ bind(&runtime);
  // Handle number to string in the runtime system if not found in the cache.
  __ TailCallRuntime(Runtime::kNumberToStringSkipCache, 1, 1);
}


// On entry lhs_ and rhs_ are the values to be compared.
// On exit r0 is 0, positive or negative to indicate the result of
// the comparison.
void CompareStub::Generate(MacroAssembler* masm) {
  ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
         (lhs_.is(r1) && rhs_.is(r0)));

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles, lhs_not_nan;

  if (include_smi_compare_) {
    Label not_two_smis, smi_done;
    __ orr(r2, r1, r0);
    __ tst(r2, Operand(kSmiTagMask));
    __ b(ne, &not_two_smis);
    __ mov(r1, Operand(r1, ASR, 1));
    __ sub(r0, r1, Operand(r0, ASR, 1));
    __ Ret();
    __ bind(&not_two_smis);
  } else if (FLAG_debug_code) {
    __ orr(r2, r1, r0);
    __ tst(r2, Operand(kSmiTagMask));
    __ Assert(ne, "CompareStub: unexpected smi operands.");
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
  __ and_(r2, lhs_, Operand(rhs_));
  __ tst(r2, Operand(kSmiTagMask));
  __ b(ne, &not_smis);
  // One operand is a smi.  EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to lhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison.  If VFP3 is supported the double values of the numbers have
  // been loaded into d7 and d6.  Otherwise, the double values have been loaded
  // into r0, r1, r2, and r3.
  EmitSmiNonsmiComparison(masm, lhs_, rhs_, &lhs_not_nan, &slow, strict_);

  __ bind(&both_loaded_as_doubles);
  // The arguments have been converted to doubles and stored in d6 and d7, if
  // VFP3 is supported, or in r0, r1, r2, and r3.
  if (CpuFeatures::IsSupported(VFP3)) {
    __ bind(&lhs_not_nan);
    CpuFeatures::Scope scope(VFP3);
    Label no_nan;
    // ARMv7 VFP3 instructions to implement double precision comparison.
    __ VFPCompareAndSetFlags(d7, d6);
    Label nan;
    __ b(vs, &nan);
    __ mov(r0, Operand(EQUAL), LeaveCC, eq);
    __ mov(r0, Operand(LESS), LeaveCC, lt);
    __ mov(r0, Operand(GREATER), LeaveCC, gt);
    __ Ret();

    __ bind(&nan);
    // If one of the sides was a NaN then the v flag is set.  Load r0 with
    // whatever it takes to make the comparison fail, since comparisons with NaN
    // always fail.
    if (cc_ == lt || cc_ == le) {
      __ mov(r0, Operand(GREATER));
    } else {
      __ mov(r0, Operand(LESS));
    }
    __ Ret();
  } else {
    // Checks for NaN in the doubles we have loaded.  Can return the answer or
    // fall through if neither is a NaN.  Also binds lhs_not_nan.
    EmitNanCheck(masm, &lhs_not_nan, cc_);
    // Compares two doubles in r0, r1, r2, r3 that are not NaNs.  Returns the
    // answer.  Never falls through.
    EmitTwoNonNanDoubleComparison(masm, cc_);
  }

  __ bind(&not_smis);
  // At this point we know we are dealing with two different objects,
  // and neither of them is a Smi.  The objects are in rhs_ and lhs_.
  if (strict_) {
    // This returns non-equal for some object types, or falls through if it
    // was not lucky.
    EmitStrictTwoHeapObjectCompare(masm, lhs_, rhs_);
  }

  Label check_for_symbols;
  Label flat_string_check;
  // Check for heap-number-heap-number comparison.  Can jump to slow case,
  // or load both doubles into r0, r1, r2, r3 and jump to the code that handles
  // that case.  If the inputs are not doubles then jumps to check_for_symbols.
  // In this case r2 will contain the type of rhs_.  Never falls through.
  EmitCheckForTwoHeapNumbers(masm,
                             lhs_,
                             rhs_,
                             &both_loaded_as_doubles,
                             &check_for_symbols,
                             &flat_string_check);

  __ bind(&check_for_symbols);
  // In the strict case the EmitStrictTwoHeapObjectCompare already took care of
  // symbols.
  if (cc_ == eq && !strict_) {
    // Returns an answer for two symbols or two detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that r2 is the type of rhs_ on entry.
    EmitCheckForSymbolsOrObjects(masm, lhs_, rhs_, &flat_string_check, &slow);
  }

  // Check for both being sequential ASCII strings, and inline if that is the
  // case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialAsciiStrings(lhs_, rhs_, r2, r3, &slow);

  __ IncrementCounter(&Counters::string_compare_native, 1, r2, r3);
  StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                     lhs_,
                                                     rhs_,
                                                     r2,
                                                     r3,
                                                     r4,
                                                     r5);
  // Never falls through to here.

  __ bind(&slow);

  __ Push(lhs_, rhs_);
  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript native;
  if (cc_ == eq) {
    native = strict_ ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    native = Builtins::COMPARE;
    int ncr;  // NaN compare result
    if (cc_ == lt || cc_ == le) {
      ncr = GREATER;
    } else {
      ASSERT(cc_ == gt || cc_ == ge);  // remaining cases
      ncr = LESS;
    }
    __ mov(r0, Operand(Smi::FromInt(ncr)));
    __ push(r0);
  }

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(native, JUMP_JS);
}


// This stub does not handle the inlined cases (Smis, Booleans, undefined).
// The stub returns zero for false, and a non-zero value for true.
void ToBooleanStub::Generate(MacroAssembler* masm) {
  // This stub uses VFP3 instructions.
  ASSERT(CpuFeatures::IsEnabled(VFP3));

  Label false_result;
  Label not_heap_number;
  Register scratch = r9.is(tos_) ? r7 : r9;

  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(tos_, ip);
  __ b(eq, &false_result);

  // HeapNumber => false iff +0, -0, or NaN.
  __ ldr(scratch, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, ip);
  __ b(&not_heap_number, ne);

  __ sub(ip, tos_, Operand(kHeapObjectTag));
  __ vldr(d1, ip, HeapNumber::kValueOffset);
  __ VFPCompareAndSetFlags(d1, 0.0);
  // "tos_" is a register, and contains a non zero value by default.
  // Hence we only need to overwrite "tos_" with zero to return false for
  // FP_ZERO or FP_NAN cases. Otherwise, by default it returns true.
  __ mov(tos_, Operand(0, RelocInfo::NONE), LeaveCC, eq);  // for FP_ZERO
  __ mov(tos_, Operand(0, RelocInfo::NONE), LeaveCC, vs);  // for FP_NAN
  __ Ret();

  __ bind(&not_heap_number);

  // Check if the value is 'null'.
  // 'null' => false.
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(tos_, ip);
  __ b(&false_result, eq);

  // It can be an undetectable object.
  // Undetectable => false.
  __ ldr(ip, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(ip, Map::kBitFieldOffset));
  __ and_(scratch, scratch, Operand(1 << Map::kIsUndetectable));
  __ cmp(scratch, Operand(1 << Map::kIsUndetectable));
  __ b(&false_result, eq);

  // JavaScript object => true.
  __ ldr(scratch, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ cmp(scratch, Operand(FIRST_JS_OBJECT_TYPE));
  // "tos_" is a register and contains a non-zero value.
  // Hence we implicitly return true if the greater than
  // condition is satisfied.
  __ Ret(gt);

  // Check for string
  __ ldr(scratch, FieldMemOperand(tos_, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  __ cmp(scratch, Operand(FIRST_NONSTRING_TYPE));
  // "tos_" is a register and contains a non-zero value.
  // Hence we implicitly return true if the greater than
  // condition is satisfied.
  __ Ret(gt);

  // String value => false iff empty, i.e., length is zero
  __ ldr(tos_, FieldMemOperand(tos_, String::kLengthOffset));
  // If length is zero, "tos_" contains zero ==> false.
  // If length is not zero, "tos_" contains a non-zero value ==> true.
  __ Ret();

  // Return 0 in "tos_" for false .
  __ bind(&false_result);
  __ mov(tos_, Operand(0, RelocInfo::NONE));
  __ Ret();
}


// We fall into this code if the operands were Smis, but the result was
// not (eg. overflow).  We branch into this code (to the not_smi label) if
// the operands were not both Smi.  The operands are in r0 and r1.  In order
// to call the C-implemented binary fp operation routines we need to end up
// with the double precision floating point operands in r0 and r1 (for the
// value in r1) and r2 and r3 (for the value in r0).
void GenericBinaryOpStub::HandleBinaryOpSlowCases(
    MacroAssembler* masm,
    Label* not_smi,
    Register lhs,
    Register rhs,
    const Builtins::JavaScript& builtin) {
  Label slow, slow_reverse, do_the_call;
  bool use_fp_registers = CpuFeatures::IsSupported(VFP3) && Token::MOD != op_;

  ASSERT((lhs.is(r0) && rhs.is(r1)) || (lhs.is(r1) && rhs.is(r0)));
  Register heap_number_map = r6;

  if (ShouldGenerateSmiCode()) {
    __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

    // Smi-smi case (overflow).
    // Since both are Smis there is no heap number to overwrite, so allocate.
    // The new heap number is in r5.  r3 and r7 are scratch.
    __ AllocateHeapNumber(
        r5, r3, r7, heap_number_map, lhs.is(r0) ? &slow_reverse : &slow);

    // If we have floating point hardware, inline ADD, SUB, MUL, and DIV,
    // using registers d7 and d6 for the double values.
    if (CpuFeatures::IsSupported(VFP3)) {
      CpuFeatures::Scope scope(VFP3);
      __ mov(r7, Operand(rhs, ASR, kSmiTagSize));
      __ vmov(s15, r7);
      __ vcvt_f64_s32(d7, s15);
      __ mov(r7, Operand(lhs, ASR, kSmiTagSize));
      __ vmov(s13, r7);
      __ vcvt_f64_s32(d6, s13);
      if (!use_fp_registers) {
        __ vmov(r2, r3, d7);
        __ vmov(r0, r1, d6);
      }
    } else {
      // Write Smi from rhs to r3 and r2 in double format.  r9 is scratch.
      __ mov(r7, Operand(rhs));
      ConvertToDoubleStub stub1(r3, r2, r7, r9);
      __ push(lr);
      __ Call(stub1.GetCode(), RelocInfo::CODE_TARGET);
      // Write Smi from lhs to r1 and r0 in double format.  r9 is scratch.
      __ mov(r7, Operand(lhs));
      ConvertToDoubleStub stub2(r1, r0, r7, r9);
      __ Call(stub2.GetCode(), RelocInfo::CODE_TARGET);
      __ pop(lr);
    }
    __ jmp(&do_the_call);  // Tail call.  No return.
  }

  // We branch here if at least one of r0 and r1 is not a Smi.
  __ bind(not_smi);
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  // After this point we have the left hand side in r1 and the right hand side
  // in r0.
  if (lhs.is(r0)) {
    __ Swap(r0, r1, ip);
  }

  // The type transition also calculates the answer.
  bool generate_code_to_calculate_answer = true;

  if (ShouldGenerateFPCode()) {
    // DIV has neither SmiSmi fast code nor specialized slow code.
    // So don't try to patch a DIV Stub.
    if (runtime_operands_type_ == BinaryOpIC::DEFAULT) {
      switch (op_) {
        case Token::ADD:
        case Token::SUB:
        case Token::MUL:
          GenerateTypeTransition(masm);  // Tail call.
          generate_code_to_calculate_answer = false;
          break;

        case Token::DIV:
          // DIV has neither SmiSmi fast code nor specialized slow code.
          // So don't try to patch a DIV Stub.
          break;

        default:
          break;
      }
    }

    if (generate_code_to_calculate_answer) {
      Label r0_is_smi, r1_is_smi, finished_loading_r0, finished_loading_r1;
      if (mode_ == NO_OVERWRITE) {
        // In the case where there is no chance of an overwritable float we may
        // as well do the allocation immediately while r0 and r1 are untouched.
        __ AllocateHeapNumber(r5, r3, r7, heap_number_map, &slow);
      }

      // Move r0 to a double in r2-r3.
      __ tst(r0, Operand(kSmiTagMask));
      __ b(eq, &r0_is_smi);  // It's a Smi so don't check it's a heap number.
      __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
      __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
      __ cmp(r4, heap_number_map);
      __ b(ne, &slow);
      if (mode_ == OVERWRITE_RIGHT) {
        __ mov(r5, Operand(r0));  // Overwrite this heap number.
      }
      if (use_fp_registers) {
        CpuFeatures::Scope scope(VFP3);
        // Load the double from tagged HeapNumber r0 to d7.
        __ sub(r7, r0, Operand(kHeapObjectTag));
        __ vldr(d7, r7, HeapNumber::kValueOffset);
      } else {
        // Calling convention says that second double is in r2 and r3.
        __ Ldrd(r2, r3, FieldMemOperand(r0, HeapNumber::kValueOffset));
      }
      __ jmp(&finished_loading_r0);
      __ bind(&r0_is_smi);
      if (mode_ == OVERWRITE_RIGHT) {
        // We can't overwrite a Smi so get address of new heap number into r5.
      __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
      }

      if (CpuFeatures::IsSupported(VFP3)) {
        CpuFeatures::Scope scope(VFP3);
        // Convert smi in r0 to double in d7.
        __ mov(r7, Operand(r0, ASR, kSmiTagSize));
        __ vmov(s15, r7);
        __ vcvt_f64_s32(d7, s15);
        if (!use_fp_registers) {
          __ vmov(r2, r3, d7);
        }
      } else {
        // Write Smi from r0 to r3 and r2 in double format.
        __ mov(r7, Operand(r0));
        ConvertToDoubleStub stub3(r3, r2, r7, r4);
        __ push(lr);
        __ Call(stub3.GetCode(), RelocInfo::CODE_TARGET);
        __ pop(lr);
      }

      // HEAP_NUMBERS stub is slower than GENERIC on a pair of smis.
      // r0 is known to be a smi. If r1 is also a smi then switch to GENERIC.
      Label r1_is_not_smi;
      if ((runtime_operands_type_ == BinaryOpIC::HEAP_NUMBERS) &&
          HasSmiSmiFastPath()) {
        __ tst(r1, Operand(kSmiTagMask));
        __ b(ne, &r1_is_not_smi);
        GenerateTypeTransition(masm);  // Tail call.
      }

      __ bind(&finished_loading_r0);

      // Move r1 to a double in r0-r1.
      __ tst(r1, Operand(kSmiTagMask));
      __ b(eq, &r1_is_smi);  // It's a Smi so don't check it's a heap number.
      __ bind(&r1_is_not_smi);
      __ ldr(r4, FieldMemOperand(r1, HeapNumber::kMapOffset));
      __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
      __ cmp(r4, heap_number_map);
      __ b(ne, &slow);
      if (mode_ == OVERWRITE_LEFT) {
        __ mov(r5, Operand(r1));  // Overwrite this heap number.
      }
      if (use_fp_registers) {
        CpuFeatures::Scope scope(VFP3);
        // Load the double from tagged HeapNumber r1 to d6.
        __ sub(r7, r1, Operand(kHeapObjectTag));
        __ vldr(d6, r7, HeapNumber::kValueOffset);
      } else {
        // Calling convention says that first double is in r0 and r1.
        __ Ldrd(r0, r1, FieldMemOperand(r1, HeapNumber::kValueOffset));
      }
      __ jmp(&finished_loading_r1);
      __ bind(&r1_is_smi);
      if (mode_ == OVERWRITE_LEFT) {
        // We can't overwrite a Smi so get address of new heap number into r5.
      __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
      }

      if (CpuFeatures::IsSupported(VFP3)) {
        CpuFeatures::Scope scope(VFP3);
        // Convert smi in r1 to double in d6.
        __ mov(r7, Operand(r1, ASR, kSmiTagSize));
        __ vmov(s13, r7);
        __ vcvt_f64_s32(d6, s13);
        if (!use_fp_registers) {
          __ vmov(r0, r1, d6);
        }
      } else {
        // Write Smi from r1 to r1 and r0 in double format.
        __ mov(r7, Operand(r1));
        ConvertToDoubleStub stub4(r1, r0, r7, r9);
        __ push(lr);
        __ Call(stub4.GetCode(), RelocInfo::CODE_TARGET);
        __ pop(lr);
      }

      __ bind(&finished_loading_r1);
    }

    if (generate_code_to_calculate_answer || do_the_call.is_linked()) {
      __ bind(&do_the_call);
      // If we are inlining the operation using VFP3 instructions for
      // add, subtract, multiply, or divide, the arguments are in d6 and d7.
      if (use_fp_registers) {
        CpuFeatures::Scope scope(VFP3);
        // ARMv7 VFP3 instructions to implement
        // double precision, add, subtract, multiply, divide.

        if (Token::MUL == op_) {
          __ vmul(d5, d6, d7);
        } else if (Token::DIV == op_) {
          __ vdiv(d5, d6, d7);
        } else if (Token::ADD == op_) {
          __ vadd(d5, d6, d7);
        } else if (Token::SUB == op_) {
          __ vsub(d5, d6, d7);
        } else {
          UNREACHABLE();
        }
        __ sub(r0, r5, Operand(kHeapObjectTag));
        __ vstr(d5, r0, HeapNumber::kValueOffset);
        __ add(r0, r0, Operand(kHeapObjectTag));
        __ Ret();
      } else {
        // If we did not inline the operation, then the arguments are in:
        // r0: Left value (least significant part of mantissa).
        // r1: Left value (sign, exponent, top of mantissa).
        // r2: Right value (least significant part of mantissa).
        // r3: Right value (sign, exponent, top of mantissa).
        // r5: Address of heap number for result.

        __ push(lr);   // For later.
        __ PrepareCallCFunction(4, r4);  // Two doubles count as 4 arguments.
        // Call C routine that may not cause GC or other trouble. r5 is callee
        // save.
        __ CallCFunction(ExternalReference::double_fp_operation(op_), 4);
        // Store answer in the overwritable heap number.
    #if !defined(USE_ARM_EABI)
        // Double returned in fp coprocessor register 0 and 1, encoded as
        // register cr8.  Offsets must be divisible by 4 for coprocessor so we
        // need to substract the tag from r5.
        __ sub(r4, r5, Operand(kHeapObjectTag));
        __ stc(p1, cr8, MemOperand(r4, HeapNumber::kValueOffset));
    #else
        // Double returned in registers 0 and 1.
        __ Strd(r0, r1, FieldMemOperand(r5, HeapNumber::kValueOffset));
    #endif
        __ mov(r0, Operand(r5));
        // And we are done.
        __ pop(pc);
      }
    }
  }

  if (!generate_code_to_calculate_answer &&
      !slow_reverse.is_linked() &&
      !slow.is_linked()) {
    return;
  }

  if (lhs.is(r0)) {
    __ b(&slow);
    __ bind(&slow_reverse);
    __ Swap(r0, r1, ip);
  }

  heap_number_map = no_reg;  // Don't use this any more from here on.

  // We jump to here if something goes wrong (one param is not a number of any
  // sort or new-space allocation fails).
  __ bind(&slow);

  // Push arguments to the stack
  __ Push(r1, r0);

  if (Token::ADD == op_) {
    // Test for string arguments before calling runtime.
    // r1 : first argument
    // r0 : second argument
    // sp[0] : second argument
    // sp[4] : first argument

    Label not_strings, not_string1, string1, string1_smi2;
    __ tst(r1, Operand(kSmiTagMask));
    __ b(eq, &not_string1);
    __ CompareObjectType(r1, r2, r2, FIRST_NONSTRING_TYPE);
    __ b(ge, &not_string1);

    // First argument is a a string, test second.
    __ tst(r0, Operand(kSmiTagMask));
    __ b(eq, &string1_smi2);
    __ CompareObjectType(r0, r2, r2, FIRST_NONSTRING_TYPE);
    __ b(ge, &string1);

    // First and second argument are strings.
    StringAddStub string_add_stub(NO_STRING_CHECK_IN_STUB);
    __ TailCallStub(&string_add_stub);

    __ bind(&string1_smi2);
    // First argument is a string, second is a smi. Try to lookup the number
    // string for the smi in the number string cache.
    NumberToStringStub::GenerateLookupNumberStringCache(
        masm, r0, r2, r4, r5, r6, true, &string1);

    // Replace second argument on stack and tailcall string add stub to make
    // the result.
    __ str(r2, MemOperand(sp, 0));
    __ TailCallStub(&string_add_stub);

    // Only first argument is a string.
    __ bind(&string1);
    __ InvokeBuiltin(Builtins::STRING_ADD_LEFT, JUMP_JS);

    // First argument was not a string, test second.
    __ bind(&not_string1);
    __ tst(r0, Operand(kSmiTagMask));
    __ b(eq, &not_strings);
    __ CompareObjectType(r0, r2, r2, FIRST_NONSTRING_TYPE);
    __ b(ge, &not_strings);

    // Only second argument is a string.
    __ InvokeBuiltin(Builtins::STRING_ADD_RIGHT, JUMP_JS);

    __ bind(&not_strings);
  }

  __ InvokeBuiltin(builtin, JUMP_JS);  // Tail call.  No return.
}


// For bitwise ops where the inputs are not both Smis we here try to determine
// whether both inputs are either Smis or at least heap numbers that can be
// represented by a 32 bit signed value.  We truncate towards zero as required
// by the ES spec.  If this is the case we do the bitwise op and see if the
// result is a Smi.  If so, great, otherwise we try to find a heap number to
// write the answer into (either by allocating or by overwriting).
// On entry the operands are in lhs and rhs.  On exit the answer is in r0.
void GenericBinaryOpStub::HandleNonSmiBitwiseOp(MacroAssembler* masm,
                                                Register lhs,
                                                Register rhs) {
  Label slow, result_not_a_smi;
  Label rhs_is_smi, lhs_is_smi;
  Label done_checking_rhs, done_checking_lhs;

  Register heap_number_map = r6;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  __ tst(lhs, Operand(kSmiTagMask));
  __ b(eq, &lhs_is_smi);  // It's a Smi so don't check it's a heap number.
  __ ldr(r4, FieldMemOperand(lhs, HeapNumber::kMapOffset));
  __ cmp(r4, heap_number_map);
  __ b(ne, &slow);
  __ ConvertToInt32(lhs, r3, r5, r4, d0, &slow);
  __ jmp(&done_checking_lhs);
  __ bind(&lhs_is_smi);
  __ mov(r3, Operand(lhs, ASR, 1));
  __ bind(&done_checking_lhs);

  __ tst(rhs, Operand(kSmiTagMask));
  __ b(eq, &rhs_is_smi);  // It's a Smi so don't check it's a heap number.
  __ ldr(r4, FieldMemOperand(rhs, HeapNumber::kMapOffset));
  __ cmp(r4, heap_number_map);
  __ b(ne, &slow);
  __ ConvertToInt32(rhs, r2, r5, r4, d0, &slow);
  __ jmp(&done_checking_rhs);
  __ bind(&rhs_is_smi);
  __ mov(r2, Operand(rhs, ASR, 1));
  __ bind(&done_checking_rhs);

  ASSERT(((lhs.is(r0) && rhs.is(r1)) || (lhs.is(r1) && rhs.is(r0))));

  // r0 and r1: Original operands (Smi or heap numbers).
  // r2 and r3: Signed int32 operands.
  switch (op_) {
    case Token::BIT_OR:  __ orr(r2, r2, Operand(r3)); break;
    case Token::BIT_XOR: __ eor(r2, r2, Operand(r3)); break;
    case Token::BIT_AND: __ and_(r2, r2, Operand(r3)); break;
    case Token::SAR:
      // Use only the 5 least significant bits of the shift count.
      __ and_(r2, r2, Operand(0x1f));
      __ mov(r2, Operand(r3, ASR, r2));
      break;
    case Token::SHR:
      // Use only the 5 least significant bits of the shift count.
      __ and_(r2, r2, Operand(0x1f));
      __ mov(r2, Operand(r3, LSR, r2), SetCC);
      // SHR is special because it is required to produce a positive answer.
      // The code below for writing into heap numbers isn't capable of writing
      // the register as an unsigned int so we go to slow case if we hit this
      // case.
      if (CpuFeatures::IsSupported(VFP3)) {
        __ b(mi, &result_not_a_smi);
      } else {
        __ b(mi, &slow);
      }
      break;
    case Token::SHL:
      // Use only the 5 least significant bits of the shift count.
      __ and_(r2, r2, Operand(0x1f));
      __ mov(r2, Operand(r3, LSL, r2));
      break;
    default: UNREACHABLE();
  }
  // check that the *signed* result fits in a smi
  __ add(r3, r2, Operand(0x40000000), SetCC);
  __ b(mi, &result_not_a_smi);
  __ mov(r0, Operand(r2, LSL, kSmiTagSize));
  __ Ret();

  Label have_to_allocate, got_a_heap_number;
  __ bind(&result_not_a_smi);
  switch (mode_) {
    case OVERWRITE_RIGHT: {
      __ tst(rhs, Operand(kSmiTagMask));
      __ b(eq, &have_to_allocate);
      __ mov(r5, Operand(rhs));
      break;
    }
    case OVERWRITE_LEFT: {
      __ tst(lhs, Operand(kSmiTagMask));
      __ b(eq, &have_to_allocate);
      __ mov(r5, Operand(lhs));
      break;
    }
    case NO_OVERWRITE: {
      // Get a new heap number in r5.  r4 and r7 are scratch.
      __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
    }
    default: break;
  }
  __ bind(&got_a_heap_number);
  // r2: Answer as signed int32.
  // r5: Heap number to write answer into.

  // Nothing can go wrong now, so move the heap number to r0, which is the
  // result.
  __ mov(r0, Operand(r5));

  if (CpuFeatures::IsSupported(VFP3)) {
    // Convert the int32 in r2 to the heap number in r0. r3 is corrupted.
    CpuFeatures::Scope scope(VFP3);
    __ vmov(s0, r2);
    if (op_ == Token::SHR) {
      __ vcvt_f64_u32(d0, s0);
    } else {
      __ vcvt_f64_s32(d0, s0);
    }
    __ sub(r3, r0, Operand(kHeapObjectTag));
    __ vstr(d0, r3, HeapNumber::kValueOffset);
    __ Ret();
  } else {
    // Tail call that writes the int32 in r2 to the heap number in r0, using
    // r3 as scratch.  r0 is preserved and returned.
    WriteInt32ToHeapNumberStub stub(r2, r0, r3);
    __ TailCallStub(&stub);
  }

  if (mode_ != NO_OVERWRITE) {
    __ bind(&have_to_allocate);
    // Get a new heap number in r5.  r4 and r7 are scratch.
    __ AllocateHeapNumber(r5, r4, r7, heap_number_map, &slow);
    __ jmp(&got_a_heap_number);
  }

  // If all else failed then we go to the runtime system.
  __ bind(&slow);
  __ Push(lhs, rhs);  // Restore stack.
  switch (op_) {
    case Token::BIT_OR:
      __ InvokeBuiltin(Builtins::BIT_OR, JUMP_JS);
      break;
    case Token::BIT_AND:
      __ InvokeBuiltin(Builtins::BIT_AND, JUMP_JS);
      break;
    case Token::BIT_XOR:
      __ InvokeBuiltin(Builtins::BIT_XOR, JUMP_JS);
      break;
    case Token::SAR:
      __ InvokeBuiltin(Builtins::SAR, JUMP_JS);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_JS);
      break;
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_JS);
      break;
    default:
      UNREACHABLE();
  }
}




// This function takes the known int in a register for the cases
// where it doesn't know a good trick, and may deliver
// a result that needs shifting.
static void MultiplyByKnownIntInStub(
    MacroAssembler* masm,
    Register result,
    Register source,
    Register known_int_register,   // Smi tagged.
    int known_int,
    int* required_shift) {  // Including Smi tag shift
  switch (known_int) {
    case 3:
      __ add(result, source, Operand(source, LSL, 1));
      *required_shift = 1;
      break;
    case 5:
      __ add(result, source, Operand(source, LSL, 2));
      *required_shift = 1;
      break;
    case 6:
      __ add(result, source, Operand(source, LSL, 1));
      *required_shift = 2;
      break;
    case 7:
      __ rsb(result, source, Operand(source, LSL, 3));
      *required_shift = 1;
      break;
    case 9:
      __ add(result, source, Operand(source, LSL, 3));
      *required_shift = 1;
      break;
    case 10:
      __ add(result, source, Operand(source, LSL, 2));
      *required_shift = 2;
      break;
    default:
      ASSERT(!IsPowerOf2(known_int));  // That would be very inefficient.
      __ mul(result, source, known_int_register);
      *required_shift = 0;
  }
}


// This uses versions of the sum-of-digits-to-see-if-a-number-is-divisible-by-3
// trick.  See http://en.wikipedia.org/wiki/Divisibility_rule
// Takes the sum of the digits base (mask + 1) repeatedly until we have a
// number from 0 to mask.  On exit the 'eq' condition flags are set if the
// answer is exactly the mask.
void IntegerModStub::DigitSum(MacroAssembler* masm,
                              Register lhs,
                              int mask,
                              int shift,
                              Label* entry) {
  ASSERT(mask > 0);
  ASSERT(mask <= 0xff);  // This ensures we don't need ip to use it.
  Label loop;
  __ bind(&loop);
  __ and_(ip, lhs, Operand(mask));
  __ add(lhs, ip, Operand(lhs, LSR, shift));
  __ bind(entry);
  __ cmp(lhs, Operand(mask));
  __ b(gt, &loop);
}


void IntegerModStub::DigitSum(MacroAssembler* masm,
                              Register lhs,
                              Register scratch,
                              int mask,
                              int shift1,
                              int shift2,
                              Label* entry) {
  ASSERT(mask > 0);
  ASSERT(mask <= 0xff);  // This ensures we don't need ip to use it.
  Label loop;
  __ bind(&loop);
  __ bic(scratch, lhs, Operand(mask));
  __ and_(ip, lhs, Operand(mask));
  __ add(lhs, ip, Operand(lhs, LSR, shift1));
  __ add(lhs, lhs, Operand(scratch, LSR, shift2));
  __ bind(entry);
  __ cmp(lhs, Operand(mask));
  __ b(gt, &loop);
}


// Splits the number into two halves (bottom half has shift bits).  The top
// half is subtracted from the bottom half.  If the result is negative then
// rhs is added.
void IntegerModStub::ModGetInRangeBySubtraction(MacroAssembler* masm,
                                                Register lhs,
                                                int shift,
                                                int rhs) {
  int mask = (1 << shift) - 1;
  __ and_(ip, lhs, Operand(mask));
  __ sub(lhs, ip, Operand(lhs, LSR, shift), SetCC);
  __ add(lhs, lhs, Operand(rhs), LeaveCC, mi);
}


void IntegerModStub::ModReduce(MacroAssembler* masm,
                               Register lhs,
                               int max,
                               int denominator) {
  int limit = denominator;
  while (limit * 2 <= max) limit *= 2;
  while (limit >= denominator) {
    __ cmp(lhs, Operand(limit));
    __ sub(lhs, lhs, Operand(limit), LeaveCC, ge);
    limit >>= 1;
  }
}


void IntegerModStub::ModAnswer(MacroAssembler* masm,
                               Register result,
                               Register shift_distance,
                               Register mask_bits,
                               Register sum_of_digits) {
  __ add(result, mask_bits, Operand(sum_of_digits, LSL, shift_distance));
  __ Ret();
}


// See comment for class.
void IntegerModStub::Generate(MacroAssembler* masm) {
  __ mov(lhs_, Operand(lhs_, LSR, shift_distance_));
  __ bic(odd_number_, odd_number_, Operand(1));
  __ mov(odd_number_, Operand(odd_number_, LSL, 1));
  // We now have (odd_number_ - 1) * 2 in the register.
  // Build a switch out of branches instead of data because it avoids
  // having to teach the assembler about intra-code-object pointers
  // that are not in relative branch instructions.
  Label mod3, mod5, mod7, mod9, mod11, mod13, mod15, mod17, mod19;
  Label mod21, mod23, mod25;
  { Assembler::BlockConstPoolScope block_const_pool(masm);
    __ add(pc, pc, Operand(odd_number_));
    // When you read pc it is always 8 ahead, but when you write it you always
    // write the actual value.  So we put in two nops to take up the slack.
    __ nop();
    __ nop();
    __ b(&mod3);
    __ b(&mod5);
    __ b(&mod7);
    __ b(&mod9);
    __ b(&mod11);
    __ b(&mod13);
    __ b(&mod15);
    __ b(&mod17);
    __ b(&mod19);
    __ b(&mod21);
    __ b(&mod23);
    __ b(&mod25);
  }

  // For each denominator we find a multiple that is almost only ones
  // when expressed in binary.  Then we do the sum-of-digits trick for
  // that number.  If the multiple is not 1 then we have to do a little
  // more work afterwards to get the answer into the 0-denominator-1
  // range.
  DigitSum(masm, lhs_, 3, 2, &mod3);  // 3 = b11.
  __ sub(lhs_, lhs_, Operand(3), LeaveCC, eq);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0xf, 4, &mod5);  // 5 * 3 = b1111.
  ModGetInRangeBySubtraction(masm, lhs_, 2, 5);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 7, 3, &mod7);  // 7 = b111.
  __ sub(lhs_, lhs_, Operand(7), LeaveCC, eq);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0x3f, 6, &mod9);  // 7 * 9 = b111111.
  ModGetInRangeBySubtraction(masm, lhs_, 3, 9);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0x3f, 6, 3, &mod11);  // 5 * 11 = b110111.
  ModReduce(masm, lhs_, 0x3f, 11);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0xff, 8, 5, &mod13);  // 19 * 13 = b11110111.
  ModReduce(masm, lhs_, 0xff, 13);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0xf, 4, &mod15);  // 15 = b1111.
  __ sub(lhs_, lhs_, Operand(15), LeaveCC, eq);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0xff, 8, &mod17);  // 15 * 17 = b11111111.
  ModGetInRangeBySubtraction(masm, lhs_, 4, 17);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0xff, 8, 5, &mod19);  // 13 * 19 = b11110111.
  ModReduce(masm, lhs_, 0xff, 19);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, 0x3f, 6, &mod21);  // 3 * 21 = b111111.
  ModReduce(masm, lhs_, 0x3f, 21);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0xff, 8, 7, &mod23);  // 11 * 23 = b11111101.
  ModReduce(masm, lhs_, 0xff, 23);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);

  DigitSum(masm, lhs_, r5, 0x7f, 7, 6, &mod25);  // 5 * 25 = b1111101.
  ModReduce(masm, lhs_, 0x7f, 25);
  ModAnswer(masm, result_, shift_distance_, mask_bits_, lhs_);
}


void GenericBinaryOpStub::Generate(MacroAssembler* masm) {
  // lhs_ : x
  // rhs_ : y
  // r0   : result

  Register result = r0;
  Register lhs = lhs_;
  Register rhs = rhs_;

  // This code can't cope with other register allocations yet.
  ASSERT(result.is(r0) &&
         ((lhs.is(r0) && rhs.is(r1)) ||
          (lhs.is(r1) && rhs.is(r0))));

  Register smi_test_reg = r7;
  Register scratch = r9;

  // All ops need to know whether we are dealing with two Smis.  Set up
  // smi_test_reg to tell us that.
  if (ShouldGenerateSmiCode()) {
    __ orr(smi_test_reg, lhs, Operand(rhs));
  }

  switch (op_) {
    case Token::ADD: {
      Label not_smi;
      // Fast path.
      if (ShouldGenerateSmiCode()) {
        STATIC_ASSERT(kSmiTag == 0);  // Adjust code below.
        __ tst(smi_test_reg, Operand(kSmiTagMask));
        __ b(ne, &not_smi);
        __ add(r0, r1, Operand(r0), SetCC);  // Add y optimistically.
        // Return if no overflow.
        __ Ret(vc);
        __ sub(r0, r0, Operand(r1));  // Revert optimistic add.
      }
      HandleBinaryOpSlowCases(masm, &not_smi, lhs, rhs, Builtins::ADD);
      break;
    }

    case Token::SUB: {
      Label not_smi;
      // Fast path.
      if (ShouldGenerateSmiCode()) {
        STATIC_ASSERT(kSmiTag == 0);  // Adjust code below.
        __ tst(smi_test_reg, Operand(kSmiTagMask));
        __ b(ne, &not_smi);
        if (lhs.is(r1)) {
          __ sub(r0, r1, Operand(r0), SetCC);  // Subtract y optimistically.
          // Return if no overflow.
          __ Ret(vc);
          __ sub(r0, r1, Operand(r0));  // Revert optimistic subtract.
        } else {
          __ sub(r0, r0, Operand(r1), SetCC);  // Subtract y optimistically.
          // Return if no overflow.
          __ Ret(vc);
          __ add(r0, r0, Operand(r1));  // Revert optimistic subtract.
        }
      }
      HandleBinaryOpSlowCases(masm, &not_smi, lhs, rhs, Builtins::SUB);
      break;
    }

    case Token::MUL: {
      Label not_smi, slow;
      if (ShouldGenerateSmiCode()) {
        STATIC_ASSERT(kSmiTag == 0);  // adjust code below
        __ tst(smi_test_reg, Operand(kSmiTagMask));
        Register scratch2 = smi_test_reg;
        smi_test_reg = no_reg;
        __ b(ne, &not_smi);
        // Remove tag from one operand (but keep sign), so that result is Smi.
        __ mov(ip, Operand(rhs, ASR, kSmiTagSize));
        // Do multiplication
        // scratch = lower 32 bits of ip * lhs.
        __ smull(scratch, scratch2, lhs, ip);
        // Go slow on overflows (overflow bit is not set).
        __ mov(ip, Operand(scratch, ASR, 31));
        // No overflow if higher 33 bits are identical.
        __ cmp(ip, Operand(scratch2));
        __ b(ne, &slow);
        // Go slow on zero result to handle -0.
        __ tst(scratch, Operand(scratch));
        __ mov(result, Operand(scratch), LeaveCC, ne);
        __ Ret(ne);
        // We need -0 if we were multiplying a negative number with 0 to get 0.
        // We know one of them was zero.
        __ add(scratch2, rhs, Operand(lhs), SetCC);
        __ mov(result, Operand(Smi::FromInt(0)), LeaveCC, pl);
        __ Ret(pl);  // Return Smi 0 if the non-zero one was positive.
        // Slow case.  We fall through here if we multiplied a negative number
        // with 0, because that would mean we should produce -0.
        __ bind(&slow);
      }
      HandleBinaryOpSlowCases(masm, &not_smi, lhs, rhs, Builtins::MUL);
      break;
    }

    case Token::DIV:
    case Token::MOD: {
      Label not_smi;
      if (ShouldGenerateSmiCode() && specialized_on_rhs_) {
        Label lhs_is_unsuitable;
        __ JumpIfNotSmi(lhs, &not_smi);
        if (IsPowerOf2(constant_rhs_)) {
          if (op_ == Token::MOD) {
            __ and_(rhs,
                    lhs,
                    Operand(0x80000000u | ((constant_rhs_ << kSmiTagSize) - 1)),
                    SetCC);
            // We now have the answer, but if the input was negative we also
            // have the sign bit.  Our work is done if the result is
            // positive or zero:
            if (!rhs.is(r0)) {
              __ mov(r0, rhs, LeaveCC, pl);
            }
            __ Ret(pl);
            // A mod of a negative left hand side must return a negative number.
            // Unfortunately if the answer is 0 then we must return -0.  And we
            // already optimistically trashed rhs so we may need to restore it.
            __ eor(rhs, rhs, Operand(0x80000000u), SetCC);
            // Next two instructions are conditional on the answer being -0.
            __ mov(rhs, Operand(Smi::FromInt(constant_rhs_)), LeaveCC, eq);
            __ b(eq, &lhs_is_unsuitable);
            // We need to subtract the dividend.  Eg. -3 % 4 == -3.
            __ sub(result, rhs, Operand(Smi::FromInt(constant_rhs_)));
          } else {
            ASSERT(op_ == Token::DIV);
            __ tst(lhs,
                   Operand(0x80000000u | ((constant_rhs_ << kSmiTagSize) - 1)));
            __ b(ne, &lhs_is_unsuitable);  // Go slow on negative or remainder.
            int shift = 0;
            int d = constant_rhs_;
            while ((d & 1) == 0) {
              d >>= 1;
              shift++;
            }
            __ mov(r0, Operand(lhs, LSR, shift));
            __ bic(r0, r0, Operand(kSmiTagMask));
          }
        } else {
          // Not a power of 2.
          __ tst(lhs, Operand(0x80000000u));
          __ b(ne, &lhs_is_unsuitable);
          // Find a fixed point reciprocal of the divisor so we can divide by
          // multiplying.
          double divisor = 1.0 / constant_rhs_;
          int shift = 32;
          double scale = 4294967296.0;  // 1 << 32.
          uint32_t mul;
          // Maximise the precision of the fixed point reciprocal.
          while (true) {
            mul = static_cast<uint32_t>(scale * divisor);
            if (mul >= 0x7fffffff) break;
            scale *= 2.0;
            shift++;
          }
          mul++;
          Register scratch2 = smi_test_reg;
          smi_test_reg = no_reg;
          __ mov(scratch2, Operand(mul));
          __ umull(scratch, scratch2, scratch2, lhs);
          __ mov(scratch2, Operand(scratch2, LSR, shift - 31));
          // scratch2 is lhs / rhs.  scratch2 is not Smi tagged.
          // rhs is still the known rhs.  rhs is Smi tagged.
          // lhs is still the unkown lhs.  lhs is Smi tagged.
          int required_scratch_shift = 0;  // Including the Smi tag shift of 1.
          // scratch = scratch2 * rhs.
          MultiplyByKnownIntInStub(masm,
                                   scratch,
                                   scratch2,
                                   rhs,
                                   constant_rhs_,
                                   &required_scratch_shift);
          // scratch << required_scratch_shift is now the Smi tagged rhs *
          // (lhs / rhs) where / indicates integer division.
          if (op_ == Token::DIV) {
            __ cmp(lhs, Operand(scratch, LSL, required_scratch_shift));
            __ b(ne, &lhs_is_unsuitable);  // There was a remainder.
            __ mov(result, Operand(scratch2, LSL, kSmiTagSize));
          } else {
            ASSERT(op_ == Token::MOD);
            __ sub(result, lhs, Operand(scratch, LSL, required_scratch_shift));
          }
        }
        __ Ret();
        __ bind(&lhs_is_unsuitable);
      } else if (op_ == Token::MOD &&
                 runtime_operands_type_ != BinaryOpIC::HEAP_NUMBERS &&
                 runtime_operands_type_ != BinaryOpIC::STRINGS) {
        // Do generate a bit of smi code for modulus even though the default for
        // modulus is not to do it, but as the ARM processor has no coprocessor
        // support for modulus checking for smis makes sense.  We can handle
        // 1 to 25 times any power of 2.  This covers over half the numbers from
        // 1 to 100 including all of the first 25.  (Actually the constants < 10
        // are handled above by reciprocal multiplication.  We only get here for
        // those cases if the right hand side is not a constant or for cases
        // like 192 which is 3*2^6 and ends up in the 3 case in the integer mod
        // stub.)
        Label slow;
        Label not_power_of_2;
        ASSERT(!ShouldGenerateSmiCode());
        STATIC_ASSERT(kSmiTag == 0);  // Adjust code below.
        // Check for two positive smis.
        __ orr(smi_test_reg, lhs, Operand(rhs));
        __ tst(smi_test_reg, Operand(0x80000000u | kSmiTagMask));
        __ b(ne, &slow);
        // Check that rhs is a power of two and not zero.
        Register mask_bits = r3;
        __ sub(scratch, rhs, Operand(1), SetCC);
        __ b(mi, &slow);
        __ and_(mask_bits, rhs, Operand(scratch), SetCC);
        __ b(ne, &not_power_of_2);
        // Calculate power of two modulus.
        __ and_(result, lhs, Operand(scratch));
        __ Ret();

        __ bind(&not_power_of_2);
        __ eor(scratch, scratch, Operand(mask_bits));
        // At least two bits are set in the modulus.  The high one(s) are in
        // mask_bits and the low one is scratch + 1.
        __ and_(mask_bits, scratch, Operand(lhs));
        Register shift_distance = scratch;
        scratch = no_reg;

        // The rhs consists of a power of 2 multiplied by some odd number.
        // The power-of-2 part we handle by putting the corresponding bits
        // from the lhs in the mask_bits register, and the power in the
        // shift_distance register.  Shift distance is never 0 due to Smi
        // tagging.
        __ CountLeadingZeros(r4, shift_distance, shift_distance);
        __ rsb(shift_distance, r4, Operand(32));

        // Now we need to find out what the odd number is. The last bit is
        // always 1.
        Register odd_number = r4;
        __ mov(odd_number, Operand(rhs, LSR, shift_distance));
        __ cmp(odd_number, Operand(25));
        __ b(gt, &slow);

        IntegerModStub stub(
            result, shift_distance, odd_number, mask_bits, lhs, r5);
        __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);  // Tail call.

        __ bind(&slow);
      }
      HandleBinaryOpSlowCases(
          masm,
          &not_smi,
          lhs,
          rhs,
          op_ == Token::MOD ? Builtins::MOD : Builtins::DIV);
      break;
    }

    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHR:
    case Token::SHL: {
      Label slow;
      STATIC_ASSERT(kSmiTag == 0);  // adjust code below
      __ tst(smi_test_reg, Operand(kSmiTagMask));
      __ b(ne, &slow);
      Register scratch2 = smi_test_reg;
      smi_test_reg = no_reg;
      switch (op_) {
        case Token::BIT_OR:  __ orr(result, rhs, Operand(lhs)); break;
        case Token::BIT_AND: __ and_(result, rhs, Operand(lhs)); break;
        case Token::BIT_XOR: __ eor(result, rhs, Operand(lhs)); break;
        case Token::SAR:
          // Remove tags from right operand.
          __ GetLeastBitsFromSmi(scratch2, rhs, 5);
          __ mov(result, Operand(lhs, ASR, scratch2));
          // Smi tag result.
          __ bic(result, result, Operand(kSmiTagMask));
          break;
        case Token::SHR:
          // Remove tags from operands.  We can't do this on a 31 bit number
          // because then the 0s get shifted into bit 30 instead of bit 31.
          __ mov(scratch, Operand(lhs, ASR, kSmiTagSize));  // x
          __ GetLeastBitsFromSmi(scratch2, rhs, 5);
          __ mov(scratch, Operand(scratch, LSR, scratch2));
          // Unsigned shift is not allowed to produce a negative number, so
          // check the sign bit and the sign bit after Smi tagging.
          __ tst(scratch, Operand(0xc0000000));
          __ b(ne, &slow);
          // Smi tag result.
          __ mov(result, Operand(scratch, LSL, kSmiTagSize));
          break;
        case Token::SHL:
          // Remove tags from operands.
          __ mov(scratch, Operand(lhs, ASR, kSmiTagSize));  // x
          __ GetLeastBitsFromSmi(scratch2, rhs, 5);
          __ mov(scratch, Operand(scratch, LSL, scratch2));
          // Check that the signed result fits in a Smi.
          __ add(scratch2, scratch, Operand(0x40000000), SetCC);
          __ b(mi, &slow);
          __ mov(result, Operand(scratch, LSL, kSmiTagSize));
          break;
        default: UNREACHABLE();
      }
      __ Ret();
      __ bind(&slow);
      HandleNonSmiBitwiseOp(masm, lhs, rhs);
      break;
    }

    default: UNREACHABLE();
  }
  // This code should be unreachable.
  __ stop("Unreachable");

  // Generate an unreachable reference to the DEFAULT stub so that it can be
  // found at the end of this stub when clearing ICs at GC.
  // TODO(kaznacheev): Check performance impact and get rid of this.
  if (runtime_operands_type_ != BinaryOpIC::DEFAULT) {
    GenericBinaryOpStub uninit(MinorKey(), BinaryOpIC::DEFAULT);
    __ CallStub(&uninit);
  }
}


void GenericBinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  Label get_result;

  __ Push(r1, r0);

  __ mov(r2, Operand(Smi::FromInt(MinorKey())));
  __ mov(r1, Operand(Smi::FromInt(op_)));
  __ mov(r0, Operand(Smi::FromInt(runtime_operands_type_)));
  __ Push(r2, r1, r0);

  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch)),
      5,
      1);
}


Handle<Code> GetBinaryOpStub(int key, BinaryOpIC::TypeInfo type_info) {
  GenericBinaryOpStub stub(key, type_info);
  return stub.GetCode();
}


Handle<Code> GetTypeRecordingBinaryOpStub(int key,
    TRBinaryOpIC::TypeInfo type_info,
    TRBinaryOpIC::TypeInfo result_type_info) {
  TypeRecordingBinaryOpStub stub(key, type_info, result_type_info);
  return stub.GetCode();
}


void TypeRecordingBinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  Label get_result;

  __ Push(r1, r0);

  __ mov(r2, Operand(Smi::FromInt(MinorKey())));
  __ mov(r1, Operand(Smi::FromInt(op_)));
  __ mov(r0, Operand(Smi::FromInt(operands_type_)));
  __ Push(r2, r1, r0);

  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kTypeRecordingBinaryOp_Patch)),
      5,
      1);
}


void TypeRecordingBinaryOpStub::GenerateTypeTransitionWithSavedArgs(
    MacroAssembler* masm) {
  UNIMPLEMENTED();
}


void TypeRecordingBinaryOpStub::Generate(MacroAssembler* masm) {
  switch (operands_type_) {
    case TRBinaryOpIC::UNINITIALIZED:
      GenerateTypeTransition(masm);
      break;
    case TRBinaryOpIC::SMI:
      GenerateSmiStub(masm);
      break;
    case TRBinaryOpIC::INT32:
      GenerateInt32Stub(masm);
      break;
    case TRBinaryOpIC::HEAP_NUMBER:
      GenerateHeapNumberStub(masm);
      break;
    case TRBinaryOpIC::ODDBALL:
      GenerateOddballStub(masm);
      break;
    case TRBinaryOpIC::STRING:
      GenerateStringStub(masm);
      break;
    case TRBinaryOpIC::GENERIC:
      GenerateGeneric(masm);
      break;
    default:
      UNREACHABLE();
  }
}


const char* TypeRecordingBinaryOpStub::GetName() {
  if (name_ != NULL) return name_;
  const int kMaxNameLength = 100;
  name_ = Bootstrapper::AllocateAutoDeletedArray(kMaxNameLength);
  if (name_ == NULL) return "OOM";
  const char* op_name = Token::Name(op_);
  const char* overwrite_name;
  switch (mode_) {
    case NO_OVERWRITE: overwrite_name = "Alloc"; break;
    case OVERWRITE_RIGHT: overwrite_name = "OverwriteRight"; break;
    case OVERWRITE_LEFT: overwrite_name = "OverwriteLeft"; break;
    default: overwrite_name = "UnknownOverwrite"; break;
  }

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "TypeRecordingBinaryOpStub_%s_%s_%s",
               op_name,
               overwrite_name,
               TRBinaryOpIC::GetName(operands_type_));
  return name_;
}


void TypeRecordingBinaryOpStub::GenerateSmiSmiOperation(
    MacroAssembler* masm) {
  Register left = r1;
  Register right = r0;
  Register scratch1 = r7;
  Register scratch2 = r9;

  ASSERT(right.is(r0));
  STATIC_ASSERT(kSmiTag == 0);

  Label not_smi_result;
  switch (op_) {
    case Token::ADD:
      __ add(right, left, Operand(right), SetCC);  // Add optimistically.
      __ Ret(vc);
      __ sub(right, right, Operand(left));  // Revert optimistic add.
      break;
    case Token::SUB:
      __ sub(right, left, Operand(right), SetCC);  // Subtract optimistically.
      __ Ret(vc);
      __ sub(right, left, Operand(right));  // Revert optimistic subtract.
      break;
    case Token::MUL:
      // Remove tag from one of the operands. This way the multiplication result
      // will be a smi if it fits the smi range.
      __ SmiUntag(ip, right);
      // Do multiplication
      // scratch1 = lower 32 bits of ip * left.
      // scratch2 = higher 32 bits of ip * left.
      __ smull(scratch1, scratch2, left, ip);
      // Check for overflowing the smi range - no overflow if higher 33 bits of
      // the result are identical.
      __ mov(ip, Operand(scratch1, ASR, 31));
      __ cmp(ip, Operand(scratch2));
      __ b(ne, &not_smi_result);
      // Go slow on zero result to handle -0.
      __ tst(scratch1, Operand(scratch1));
      __ mov(right, Operand(scratch1), LeaveCC, ne);
      __ Ret(ne);
      // We need -0 if we were multiplying a negative number with 0 to get 0.
      // We know one of them was zero.
      __ add(scratch2, right, Operand(left), SetCC);
      __ mov(right, Operand(Smi::FromInt(0)), LeaveCC, pl);
      __ Ret(pl);  // Return smi 0 if the non-zero one was positive.
      // We fall through here if we multiplied a negative number with 0, because
      // that would mean we should produce -0.
      break;
    case Token::DIV:
      // Check for power of two on the right hand side.
      __ JumpIfNotPowerOfTwoOrZero(right, scratch1, &not_smi_result);
      // Check for positive and no remainder (scratch1 contains right - 1).
      __ orr(scratch2, scratch1, Operand(0x80000000u));
      __ tst(left, scratch2);
      __ b(ne, &not_smi_result);

      // Perform division by shifting.
      __ CountLeadingZeros(scratch1, scratch1, scratch2);
      __ rsb(scratch1, scratch1, Operand(31));
      __ mov(right, Operand(left, LSR, scratch1));
      __ Ret();
      break;
    case Token::MOD:
      // Check for two positive smis.
      __ orr(scratch1, left, Operand(right));
      __ tst(scratch1, Operand(0x80000000u | kSmiTagMask));
      __ b(ne, &not_smi_result);

      // Check for power of two on the right hand side.
      __ JumpIfNotPowerOfTwoOrZero(right, scratch1, &not_smi_result);

      // Perform modulus by masking.
      __ and_(right, left, Operand(scratch1));
      __ Ret();
      break;
    case Token::BIT_OR:
      __ orr(right, left, Operand(right));
      __ Ret();
      break;
    case Token::BIT_AND:
      __ and_(right, left, Operand(right));
      __ Ret();
      break;
    case Token::BIT_XOR:
      __ eor(right, left, Operand(right));
      __ Ret();
      break;
    case Token::SAR:
      // Remove tags from right operand.
      __ GetLeastBitsFromSmi(scratch1, right, 5);
      __ mov(right, Operand(left, ASR, scratch1));
      // Smi tag result.
      __ bic(right, right, Operand(kSmiTagMask));
      __ Ret();
      break;
    case Token::SHR:
      // Remove tags from operands. We can't do this on a 31 bit number
      // because then the 0s get shifted into bit 30 instead of bit 31.
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ mov(scratch1, Operand(scratch1, LSR, scratch2));
      // Unsigned shift is not allowed to produce a negative number, so
      // check the sign bit and the sign bit after Smi tagging.
      __ tst(scratch1, Operand(0xc0000000));
      __ b(ne, &not_smi_result);
      // Smi tag result.
      __ SmiTag(right, scratch1);
      __ Ret();
      break;
    case Token::SHL:
      // Remove tags from operands.
      __ SmiUntag(scratch1, left);
      __ GetLeastBitsFromSmi(scratch2, right, 5);
      __ mov(scratch1, Operand(scratch1, LSL, scratch2));
      // Check that the signed result fits in a Smi.
      __ add(scratch2, scratch1, Operand(0x40000000), SetCC);
      __ b(mi, &not_smi_result);
      __ SmiTag(right, scratch1);
      __ Ret();
      break;
    default:
      UNREACHABLE();
  }
  __ bind(&not_smi_result);
}


void TypeRecordingBinaryOpStub::GenerateFPOperation(MacroAssembler* masm,
                                                    bool smi_operands,
                                                    Label* not_numbers,
                                                    Label* gc_required) {
  Register left = r1;
  Register right = r0;
  Register scratch1 = r7;
  Register scratch2 = r9;

  ASSERT(smi_operands || (not_numbers != NULL));
  if (smi_operands && FLAG_debug_code) {
    __ AbortIfNotSmi(left);
    __ AbortIfNotSmi(right);
  }

  Register heap_number_map = r6;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD: {
      // Load left and right operands into d6 and d7 or r0/r1 and r2/r3
      // depending on whether VFP3 is available or not.
      FloatingPointHelper::Destination destination =
          CpuFeatures::IsSupported(VFP3) && op_ != Token::MOD ?
          FloatingPointHelper::kVFPRegisters :
          FloatingPointHelper::kCoreRegisters;

      // Allocate new heap number for result.
      Register result = r5;
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
      if (destination == FloatingPointHelper::kVFPRegisters) {
        // Using VFP registers:
        // d6: Left value
        // d7: Right value
        CpuFeatures::Scope scope(VFP3);
        switch (op_) {
          case Token::ADD:
            __ vadd(d5, d6, d7);
            break;
          case Token::SUB:
            __ vsub(d5, d6, d7);
            break;
          case Token::MUL:
            __ vmul(d5, d6, d7);
            break;
          case Token::DIV:
            __ vdiv(d5, d6, d7);
            break;
          default:
            UNREACHABLE();
        }

        __ sub(r0, result, Operand(kHeapObjectTag));
        __ vstr(d5, r0, HeapNumber::kValueOffset);
        __ add(r0, r0, Operand(kHeapObjectTag));
        __ Ret();
      } else {
        // Call the C function to handle the double operation.
        FloatingPointHelper::CallCCodeForDoubleOperation(masm,
                                                         op_,
                                                         result,
                                                         scratch1);
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
        __ SmiUntag(r3, left);
        __ SmiUntag(r2, right);
      } else {
        // Convert operands to 32-bit integers. Right in r2 and left in r3.
        FloatingPointHelper::LoadNumberAsInteger(masm,
                                                 left,
                                                 r3,
                                                 heap_number_map,
                                                 scratch1,
                                                 scratch2,
                                                 d0,
                                                 not_numbers);
        FloatingPointHelper::LoadNumberAsInteger(masm,
                                                 right,
                                                 r2,
                                                 heap_number_map,
                                                 scratch1,
                                                 scratch2,
                                                 d0,
                                                 not_numbers);
      }

      Label result_not_a_smi;
      switch (op_) {
        case Token::BIT_OR:
          __ orr(r2, r3, Operand(r2));
          break;
        case Token::BIT_XOR:
          __ eor(r2, r3, Operand(r2));
          break;
        case Token::BIT_AND:
          __ and_(r2, r3, Operand(r2));
          break;
        case Token::SAR:
          // Use only the 5 least significant bits of the shift count.
          __ GetLeastBitsFromInt32(r2, r2, 5);
          __ mov(r2, Operand(r3, ASR, r2));
          break;
        case Token::SHR:
          // Use only the 5 least significant bits of the shift count.
          __ GetLeastBitsFromInt32(r2, r2, 5);
          __ mov(r2, Operand(r3, LSR, r2), SetCC);
          // SHR is special because it is required to produce a positive answer.
          // The code below for writing into heap numbers isn't capable of
          // writing the register as an unsigned int so we go to slow case if we
          // hit this case.
          if (CpuFeatures::IsSupported(VFP3)) {
            __ b(mi, &result_not_a_smi);
          } else {
            __ b(mi, not_numbers);
          }
          break;
        case Token::SHL:
          // Use only the 5 least significant bits of the shift count.
          __ GetLeastBitsFromInt32(r2, r2, 5);
          __ mov(r2, Operand(r3, LSL, r2));
          break;
        default:
          UNREACHABLE();
      }

      // Check that the *signed* result fits in a smi.
      __ add(r3, r2, Operand(0x40000000), SetCC);
      __ b(mi, &result_not_a_smi);
      __ SmiTag(r0, r2);
      __ Ret();

      // Allocate new heap number for result.
      __ bind(&result_not_a_smi);
      Register result = r5;
      if (smi_operands) {
        __ AllocateHeapNumber(
            result, scratch1, scratch2, heap_number_map, gc_required);
      } else {
        GenerateHeapResultAllocation(
            masm, result, heap_number_map, scratch1, scratch2, gc_required);
      }

      // r2: Answer as signed int32.
      // r5: Heap number to write answer into.

      // Nothing can go wrong now, so move the heap number to r0, which is the
      // result.
      __ mov(r0, Operand(r5));

      if (CpuFeatures::IsSupported(VFP3)) {
        // Convert the int32 in r2 to the heap number in r0. r3 is corrupted. As
        // mentioned above SHR needs to always produce a positive result.
        CpuFeatures::Scope scope(VFP3);
        __ vmov(s0, r2);
        if (op_ == Token::SHR) {
          __ vcvt_f64_u32(d0, s0);
        } else {
          __ vcvt_f64_s32(d0, s0);
        }
        __ sub(r3, r0, Operand(kHeapObjectTag));
        __ vstr(d0, r3, HeapNumber::kValueOffset);
        __ Ret();
      } else {
        // Tail call that writes the int32 in r2 to the heap number in r0, using
        // r3 as scratch. r0 is preserved and returned.
        WriteInt32ToHeapNumberStub stub(r2, r0, r3);
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
void TypeRecordingBinaryOpStub::GenerateSmiCode(MacroAssembler* masm,
    Label* gc_required,
    SmiCodeGenerateHeapNumberResults allow_heapnumber_results) {
  Label not_smis;

  Register left = r1;
  Register right = r0;
  Register scratch1 = r7;
  Register scratch2 = r9;

  // Perform combined smi check on both operands.
  __ orr(scratch1, left, Operand(right));
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(scratch1, Operand(kSmiTagMask));
  __ b(ne, &not_smis);

  // If the smi-smi operation results in a smi return is generated.
  GenerateSmiSmiOperation(masm);

  // If heap number results are possible generate the result in an allocated
  // heap number.
  if (allow_heapnumber_results == ALLOW_HEAPNUMBER_RESULTS) {
    GenerateFPOperation(masm, true, NULL, gc_required);
  }
  __ bind(&not_smis);
}


void TypeRecordingBinaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
  Label not_smis, call_runtime;

  if (result_type_ == TRBinaryOpIC::UNINITIALIZED ||
      result_type_ == TRBinaryOpIC::SMI) {
    // Only allow smi results.
    GenerateSmiCode(masm, NULL, NO_HEAPNUMBER_RESULTS);
  } else {
    // Allow heap number result and don't make a transition if a heap number
    // cannot be allocated.
    GenerateSmiCode(masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS);
  }

  // Code falls through if the result is not returned as either a smi or heap
  // number.
  GenerateTypeTransition(masm);

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void TypeRecordingBinaryOpStub::GenerateStringStub(MacroAssembler* masm) {
  ASSERT(operands_type_ == TRBinaryOpIC::STRING);
  ASSERT(op_ == Token::ADD);
  // Try to add arguments as strings, otherwise, transition to the generic
  // TRBinaryOpIC type.
  GenerateAddStrings(masm);
  GenerateTypeTransition(masm);
}


void TypeRecordingBinaryOpStub::GenerateInt32Stub(MacroAssembler* masm) {
  ASSERT(operands_type_ == TRBinaryOpIC::INT32);

  Register left = r1;
  Register right = r0;
  Register scratch1 = r7;
  Register scratch2 = r9;
  DwVfpRegister double_scratch = d0;
  SwVfpRegister single_scratch = s3;

  Register heap_number_result = no_reg;
  Register heap_number_map = r6;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  Label call_runtime;
  // Labels for type transition, used for wrong input or output types.
  // Both label are currently actually bound to the same position. We use two
  // different label to differentiate the cause leading to type transition.
  Label transition;

  // Smi-smi fast case.
  Label skip;
  __ orr(scratch1, left, right);
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
    // Jump to type transition if they are not. The registers r0 and r1 (right
    // and left) are preserved for the runtime call.
    FloatingPointHelper::Destination destination =
        CpuFeatures::IsSupported(VFP3) && op_ != Token::MOD ?
        FloatingPointHelper::kVFPRegisters :
        FloatingPointHelper::kCoreRegisters;

    FloatingPointHelper::LoadNumberAsInt32Double(masm,
                                                 right,
                                                 destination,
                                                 d7,
                                                 r2,
                                                 r3,
                                                 heap_number_map,
                                                 scratch1,
                                                 scratch2,
                                                 s0,
                                                 &transition);
    FloatingPointHelper::LoadNumberAsInt32Double(masm,
                                                 left,
                                                 destination,
                                                 d6,
                                                 r4,
                                                 r5,
                                                 heap_number_map,
                                                 scratch1,
                                                 scratch2,
                                                 s0,
                                                 &transition);

      if (destination == FloatingPointHelper::kVFPRegisters) {
        CpuFeatures::Scope scope(VFP3);
        Label return_heap_number;
        switch (op_) {
          case Token::ADD:
            __ vadd(d5, d6, d7);
            break;
          case Token::SUB:
            __ vsub(d5, d6, d7);
            break;
          case Token::MUL:
            __ vmul(d5, d6, d7);
            break;
          case Token::DIV:
            __ vdiv(d5, d6, d7);
            break;
          default:
            UNREACHABLE();
        }

        if (op_ != Token::DIV) {
          // These operations produce an integer result.
          // Try to return a smi if we can.
          // Otherwise return a heap number if allowed, or jump to type
          // transition.

          __ EmitVFPTruncate(kRoundToZero,
                             single_scratch,
                             d5,
                             scratch1,
                             scratch2);

          if (result_type_ <= TRBinaryOpIC::INT32) {
            // If the ne condition is set, result does
            // not fit in a 32-bit integer.
            __ b(ne, &transition);
          }

          // Check if the result fits in a smi.
          __ vmov(scratch1, single_scratch);
          __ add(scratch2, scratch1, Operand(0x40000000), SetCC);
          // If not try to return a heap number.
          __ b(mi, &return_heap_number);
          // Tag the result and return.
          __ SmiTag(r0, scratch1);
          __ Ret();
        }

        if (result_type_ >= (op_ == Token::DIV) ? TRBinaryOpIC::HEAP_NUMBER
                                                : TRBinaryOpIC::INT32) {
          __ bind(&return_heap_number);
          // We are using vfp registers so r5 is available.
          heap_number_result = r5;
          GenerateHeapResultAllocation(masm,
                                       heap_number_result,
                                       heap_number_map,
                                       scratch1,
                                       scratch2,
                                       &call_runtime);
          __ sub(r0, heap_number_result, Operand(kHeapObjectTag));
          __ vstr(d5, r0, HeapNumber::kValueOffset);
          __ mov(r0, heap_number_result);
          __ Ret();
        }

        // A DIV operation expecting an integer result falls through
        // to type transition.

      } else {
        // We preserved r0 and r1 to be able to call runtime.
        // Save the left value on the stack.
        __ Push(r5, r4);

        Label pop_and_call_runtime;

        // Allocate a heap number to store the result.
        heap_number_result = r5;
        GenerateHeapResultAllocation(masm,
                                     heap_number_result,
                                     heap_number_map,
                                     scratch1,
                                     scratch2,
                                     &pop_and_call_runtime);

        // Load the left value from the value saved on the stack.
        __ Pop(r1, r0);

        // Call the C function to handle the double operation.
        FloatingPointHelper::CallCCodeForDoubleOperation(
            masm, op_, heap_number_result, scratch1);

        __ bind(&pop_and_call_runtime);
        __ Drop(2);
        __ b(&call_runtime);
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
      Register scratch3 = r5;
      // Convert operands to 32-bit integers. Right in r2 and left in r3. The
      // registers r0 and r1 (right and left) are preserved for the runtime
      // call.
      FloatingPointHelper::LoadNumberAsInt32(masm,
                                             left,
                                             r3,
                                             heap_number_map,
                                             scratch1,
                                             scratch2,
                                             scratch3,
                                             d0,
                                             &transition);
      FloatingPointHelper::LoadNumberAsInt32(masm,
                                             right,
                                             r2,
                                             heap_number_map,
                                             scratch1,
                                             scratch2,
                                             scratch3,
                                             d0,
                                             &transition);

      // The ECMA-262 standard specifies that, for shift operations, only the
      // 5 least significant bits of the shift value should be used.
      switch (op_) {
        case Token::BIT_OR:
          __ orr(r2, r3, Operand(r2));
          break;
        case Token::BIT_XOR:
          __ eor(r2, r3, Operand(r2));
          break;
        case Token::BIT_AND:
          __ and_(r2, r3, Operand(r2));
          break;
        case Token::SAR:
          __ and_(r2, r2, Operand(0x1f));
          __ mov(r2, Operand(r3, ASR, r2));
          break;
        case Token::SHR:
          __ and_(r2, r2, Operand(0x1f));
          __ mov(r2, Operand(r3, LSR, r2), SetCC);
          // SHR is special because it is required to produce a positive answer.
          // We only get a negative result if the shift value (r2) is 0.
          // This result cannot be respresented as a signed 32-bit integer, try
          // to return a heap number if we can.
          // The non vfp3 code does not support this special case, so jump to
          // runtime if we don't support it.
          if (CpuFeatures::IsSupported(VFP3)) {
            __ b(mi,
                 (result_type_ <= TRBinaryOpIC::INT32) ? &transition
                                                       : &return_heap_number);
          } else {
            __ b(mi, (result_type_ <= TRBinaryOpIC::INT32) ? &transition
                                                           : &call_runtime);
          }
          break;
        case Token::SHL:
          __ and_(r2, r2, Operand(0x1f));
          __ mov(r2, Operand(r3, LSL, r2));
          break;
        default:
          UNREACHABLE();
      }

      // Check if the result fits in a smi.
      __ add(scratch1, r2, Operand(0x40000000), SetCC);
      // If not try to return a heap number. (We know the result is an int32.)
      __ b(mi, &return_heap_number);
      // Tag the result and return.
      __ SmiTag(r0, r2);
      __ Ret();

      __ bind(&return_heap_number);
      if (CpuFeatures::IsSupported(VFP3)) {
        CpuFeatures::Scope scope(VFP3);
        heap_number_result = r5;
        GenerateHeapResultAllocation(masm,
                                     heap_number_result,
                                     heap_number_map,
                                     scratch1,
                                     scratch2,
                                     &call_runtime);

        if (op_ != Token::SHR) {
          // Convert the result to a floating point value.
          __ vmov(double_scratch.low(), r2);
          __ vcvt_f64_s32(double_scratch, double_scratch.low());
        } else {
          // The result must be interpreted as an unsigned 32-bit integer.
          __ vmov(double_scratch.low(), r2);
          __ vcvt_f64_u32(double_scratch, double_scratch.low());
        }

        // Store the result.
        __ sub(r0, heap_number_result, Operand(kHeapObjectTag));
        __ vstr(double_scratch, r0, HeapNumber::kValueOffset);
        __ mov(r0, heap_number_result);
        __ Ret();
      } else {
        // Tail call that writes the int32 in r2 to the heap number in r0, using
        // r3 as scratch. r0 is preserved and returned.
        WriteInt32ToHeapNumberStub stub(r2, r0, r3);
        __ TailCallStub(&stub);
      }

      break;
    }

    default:
      UNREACHABLE();
  }

  if (transition.is_linked()) {
    __ bind(&transition);
    GenerateTypeTransition(masm);
  }

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void TypeRecordingBinaryOpStub::GenerateOddballStub(MacroAssembler* masm) {
  Label call_runtime;

  if (op_ == Token::ADD) {
    // Handle string addition here, because it is the only operation
    // that does not do a ToNumber conversion on the operands.
    GenerateAddStrings(masm);
  }

  // Convert oddball arguments to numbers.
  Label check, done;
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r1, ip);
  __ b(ne, &check);
  if (Token::IsBitOp(op_)) {
    __ mov(r1, Operand(Smi::FromInt(0)));
  } else {
    __ LoadRoot(r1, Heap::kNanValueRootIndex);
  }
  __ jmp(&done);
  __ bind(&check);
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r0, ip);
  __ b(ne, &done);
  if (Token::IsBitOp(op_)) {
    __ mov(r0, Operand(Smi::FromInt(0)));
  } else {
    __ LoadRoot(r0, Heap::kNanValueRootIndex);
  }
  __ bind(&done);

  GenerateHeapNumberStub(masm);
}


void TypeRecordingBinaryOpStub::GenerateHeapNumberStub(MacroAssembler* masm) {
  Label not_numbers, call_runtime;
  GenerateFPOperation(masm, false, &not_numbers, &call_runtime);

  __ bind(&not_numbers);
  GenerateTypeTransition(masm);

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void TypeRecordingBinaryOpStub::GenerateGeneric(MacroAssembler* masm) {
  Label call_runtime, call_string_add_or_runtime;

  GenerateSmiCode(masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS);

  GenerateFPOperation(masm, false, &call_string_add_or_runtime, &call_runtime);

  __ bind(&call_string_add_or_runtime);
  if (op_ == Token::ADD) {
    GenerateAddStrings(masm);
  }

  __ bind(&call_runtime);
  GenerateCallRuntime(masm);
}


void TypeRecordingBinaryOpStub::GenerateAddStrings(MacroAssembler* masm) {
  ASSERT(op_ == Token::ADD);
  Label left_not_string, call_runtime;

  Register left = r1;
  Register right = r0;

  // Check if left argument is a string.
  __ JumpIfSmi(left, &left_not_string);
  __ CompareObjectType(left, r2, r2, FIRST_NONSTRING_TYPE);
  __ b(ge, &left_not_string);

  StringAddStub string_add_left_stub(NO_STRING_CHECK_LEFT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_left_stub);

  // Left operand is not a string, test right.
  __ bind(&left_not_string);
  __ JumpIfSmi(right, &call_runtime);
  __ CompareObjectType(right, r2, r2, FIRST_NONSTRING_TYPE);
  __ b(ge, &call_runtime);

  StringAddStub string_add_right_stub(NO_STRING_CHECK_RIGHT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_right_stub);

  // At least one argument is not a string.
  __ bind(&call_runtime);
}


void TypeRecordingBinaryOpStub::GenerateCallRuntime(MacroAssembler* masm) {
  GenerateRegisterArgsPush(masm);
  switch (op_) {
    case Token::ADD:
      __ InvokeBuiltin(Builtins::ADD, JUMP_JS);
      break;
    case Token::SUB:
      __ InvokeBuiltin(Builtins::SUB, JUMP_JS);
      break;
    case Token::MUL:
      __ InvokeBuiltin(Builtins::MUL, JUMP_JS);
      break;
    case Token::DIV:
      __ InvokeBuiltin(Builtins::DIV, JUMP_JS);
      break;
    case Token::MOD:
      __ InvokeBuiltin(Builtins::MOD, JUMP_JS);
      break;
    case Token::BIT_OR:
      __ InvokeBuiltin(Builtins::BIT_OR, JUMP_JS);
      break;
    case Token::BIT_AND:
      __ InvokeBuiltin(Builtins::BIT_AND, JUMP_JS);
      break;
    case Token::BIT_XOR:
      __ InvokeBuiltin(Builtins::BIT_XOR, JUMP_JS);
      break;
    case Token::SAR:
      __ InvokeBuiltin(Builtins::SAR, JUMP_JS);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_JS);
      break;
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_JS);
      break;
    default:
      UNREACHABLE();
  }
}


void TypeRecordingBinaryOpStub::GenerateHeapResultAllocation(
    MacroAssembler* masm,
    Register result,
    Register heap_number_map,
    Register scratch1,
    Register scratch2,
    Label* gc_required) {

  // Code below will scratch result if allocation fails. To keep both arguments
  // intact for the runtime call result cannot be one of these.
  ASSERT(!result.is(r0) && !result.is(r1));

  if (mode_ == OVERWRITE_LEFT || mode_ == OVERWRITE_RIGHT) {
    Label skip_allocation, allocated;
    Register overwritable_operand = mode_ == OVERWRITE_LEFT ? r1 : r0;
    // If the overwritable operand is already an object, we skip the
    // allocation of a heap number.
    __ JumpIfNotSmi(overwritable_operand, &skip_allocation);
    // Allocate a heap number for the result.
    __ AllocateHeapNumber(
        result, scratch1, scratch2, heap_number_map, gc_required);
    __ b(&allocated);
    __ bind(&skip_allocation);
    // Use object holding the overwritable operand for result.
    __ mov(result, Operand(overwritable_operand));
    __ bind(&allocated);
  } else {
    ASSERT(mode_ == NO_OVERWRITE);
    __ AllocateHeapNumber(
        result, scratch1, scratch2, heap_number_map, gc_required);
  }
}


void TypeRecordingBinaryOpStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
  __ Push(r1, r0);
}


void TranscendentalCacheStub::Generate(MacroAssembler* masm) {
  // Argument is a number and is on stack and in r0.
  Label runtime_call;
  Label input_not_smi;
  Label loaded;

  if (CpuFeatures::IsSupported(VFP3)) {
    // Load argument and check if it is a smi.
    __ JumpIfNotSmi(r0, &input_not_smi);

    CpuFeatures::Scope scope(VFP3);
    // Input is a smi. Convert to double and load the low and high words
    // of the double into r2, r3.
    __ IntegerToDoubleConversionWithVFP3(r0, r3, r2);
    __ b(&loaded);

    __ bind(&input_not_smi);
    // Check if input is a HeapNumber.
    __ CheckMap(r0,
                r1,
                Heap::kHeapNumberMapRootIndex,
                &runtime_call,
                true);
    // Input is a HeapNumber. Load it to a double register and store the
    // low and high words into r2, r3.
    __ Ldrd(r2, r3, FieldMemOperand(r0, HeapNumber::kValueOffset));

    __ bind(&loaded);
    // r2 = low 32 bits of double value
    // r3 = high 32 bits of double value
    // Compute hash (the shifts are arithmetic):
    //   h = (low ^ high); h ^= h >> 16; h ^= h >> 8; h = h & (cacheSize - 1);
    __ eor(r1, r2, Operand(r3));
    __ eor(r1, r1, Operand(r1, ASR, 16));
    __ eor(r1, r1, Operand(r1, ASR, 8));
    ASSERT(IsPowerOf2(TranscendentalCache::kCacheSize));
    __ And(r1, r1, Operand(TranscendentalCache::kCacheSize - 1));

    // r2 = low 32 bits of double value.
    // r3 = high 32 bits of double value.
    // r1 = TranscendentalCache::hash(double value).
    __ mov(r0,
           Operand(ExternalReference::transcendental_cache_array_address()));
    // r0 points to cache array.
    __ ldr(r0, MemOperand(r0, type_ * sizeof(TranscendentalCache::caches_[0])));
    // r0 points to the cache for the type type_.
    // If NULL, the cache hasn't been initialized yet, so go through runtime.
    __ cmp(r0, Operand(0, RelocInfo::NONE));
    __ b(eq, &runtime_call);

#ifdef DEBUG
    // Check that the layout of cache elements match expectations.
    { TranscendentalCache::Element test_elem[2];
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

    // Find the address of the r1'st entry in the cache, i.e., &r0[r1*12].
    __ add(r1, r1, Operand(r1, LSL, 1));
    __ add(r0, r0, Operand(r1, LSL, 2));
    // Check if cache matches: Double value is stored in uint32_t[2] array.
    __ ldm(ia, r0, r4.bit()| r5.bit() | r6.bit());
    __ cmp(r2, r4);
    __ b(ne, &runtime_call);
    __ cmp(r3, r5);
    __ b(ne, &runtime_call);
    // Cache hit. Load result, pop argument and return.
    __ mov(r0, Operand(r6));
    __ pop();
    __ Ret();
  }

  __ bind(&runtime_call);
  __ TailCallExternalReference(ExternalReference(RuntimeFunction()), 1, 1);
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


void GenericUnaryOpStub::Generate(MacroAssembler* masm) {
  Label slow, done;

  Register heap_number_map = r6;
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  if (op_ == Token::SUB) {
    if (include_smi_code_) {
      // Check whether the value is a smi.
      Label try_float;
      __ tst(r0, Operand(kSmiTagMask));
      __ b(ne, &try_float);

      // Go slow case if the value of the expression is zero
      // to make sure that we switch between 0 and -0.
      if (negative_zero_ == kStrictNegativeZero) {
        // If we have to check for zero, then we can check for the max negative
        // smi while we are at it.
        __ bic(ip, r0, Operand(0x80000000), SetCC);
        __ b(eq, &slow);
        __ rsb(r0, r0, Operand(0, RelocInfo::NONE));
        __ Ret();
      } else {
        // The value of the expression is a smi and 0 is OK for -0.  Try
        // optimistic subtraction '0 - value'.
        __ rsb(r0, r0, Operand(0, RelocInfo::NONE), SetCC);
        __ Ret(vc);
        // We don't have to reverse the optimistic neg since the only case
        // where we fall through is the minimum negative Smi, which is the case
        // where the neg leaves the register unchanged.
        __ jmp(&slow);  // Go slow on max negative Smi.
      }
      __ bind(&try_float);
    } else if (FLAG_debug_code) {
      __ tst(r0, Operand(kSmiTagMask));
      __ Assert(ne, "Unexpected smi operand.");
    }

    __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
    __ cmp(r1, heap_number_map);
    __ b(ne, &slow);
    // r0 is a heap number.  Get a new heap number in r1.
    if (overwrite_ == UNARY_OVERWRITE) {
      __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
      __ eor(r2, r2, Operand(HeapNumber::kSignMask));  // Flip sign.
      __ str(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
    } else {
      __ AllocateHeapNumber(r1, r2, r3, r6, &slow);
      __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
      __ ldr(r2, FieldMemOperand(r0, HeapNumber::kExponentOffset));
      __ str(r3, FieldMemOperand(r1, HeapNumber::kMantissaOffset));
      __ eor(r2, r2, Operand(HeapNumber::kSignMask));  // Flip sign.
      __ str(r2, FieldMemOperand(r1, HeapNumber::kExponentOffset));
      __ mov(r0, Operand(r1));
    }
  } else if (op_ == Token::BIT_NOT) {
    if (include_smi_code_) {
      Label non_smi;
      __ JumpIfNotSmi(r0, &non_smi);
      __ mvn(r0, Operand(r0));
      // Bit-clear inverted smi-tag.
      __ bic(r0, r0, Operand(kSmiTagMask));
      __ Ret();
      __ bind(&non_smi);
    } else if (FLAG_debug_code) {
      __ tst(r0, Operand(kSmiTagMask));
      __ Assert(ne, "Unexpected smi operand.");
    }

    // Check if the operand is a heap number.
    __ ldr(r1, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
    __ cmp(r1, heap_number_map);
    __ b(ne, &slow);

    // Convert the heap number is r0 to an untagged integer in r1.
    __ ConvertToInt32(r0, r1, r2, r3, d0, &slow);

    // Do the bitwise operation (move negated) and check if the result
    // fits in a smi.
    Label try_float;
    __ mvn(r1, Operand(r1));
    __ add(r2, r1, Operand(0x40000000), SetCC);
    __ b(mi, &try_float);
    __ mov(r0, Operand(r1, LSL, kSmiTagSize));
    __ b(&done);

    __ bind(&try_float);
    if (!overwrite_ == UNARY_OVERWRITE) {
      // Allocate a fresh heap number, but don't overwrite r0 until
      // we're sure we can do it without going through the slow case
      // that needs the value in r0.
      __ AllocateHeapNumber(r2, r3, r4, r6, &slow);
      __ mov(r0, Operand(r2));
    }

    if (CpuFeatures::IsSupported(VFP3)) {
      // Convert the int32 in r1 to the heap number in r0. r2 is corrupted.
      CpuFeatures::Scope scope(VFP3);
      __ vmov(s0, r1);
      __ vcvt_f64_s32(d0, s0);
      __ sub(r2, r0, Operand(kHeapObjectTag));
      __ vstr(d0, r2, HeapNumber::kValueOffset);
    } else {
      // WriteInt32ToHeapNumberStub does not trigger GC, so we do not
      // have to set up a frame.
      WriteInt32ToHeapNumberStub stub(r1, r0, r2);
      __ push(lr);
      __ Call(stub.GetCode(), RelocInfo::CODE_TARGET);
      __ pop(lr);
    }
  } else {
    UNIMPLEMENTED();
  }

  __ bind(&done);
  __ Ret();

  // Handle the slow case by jumping to the JavaScript builtin.
  __ bind(&slow);
  __ push(r0);
  switch (op_) {
    case Token::SUB:
      __ InvokeBuiltin(Builtins::UNARY_MINUS, JUMP_JS);
      break;
    case Token::BIT_NOT:
      __ InvokeBuiltin(Builtins::BIT_NOT, JUMP_JS);
      break;
    default:
      UNREACHABLE();
  }
}


void CEntryStub::GenerateThrowTOS(MacroAssembler* masm) {
  __ Throw(r0);
}


void CEntryStub::GenerateThrowUncatchable(MacroAssembler* masm,
                                          UncatchableExceptionType type) {
  __ ThrowUncatchable(type, r0);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate) {
  // r0: result parameter for PerformGC, if any
  // r4: number of arguments including receiver  (C callee-saved)
  // r5: pointer to builtin function  (C callee-saved)
  // r6: pointer to the first argument (C callee-saved)

  if (do_gc) {
    // Passing r0.
    __ PrepareCallCFunction(1, r1);
    __ CallCFunction(ExternalReference::perform_gc_function(), 1);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth();
  if (always_allocate) {
    __ mov(r0, Operand(scope_depth));
    __ ldr(r1, MemOperand(r0));
    __ add(r1, r1, Operand(1));
    __ str(r1, MemOperand(r0));
  }

  // Call C built-in.
  // r0 = argc, r1 = argv
  __ mov(r0, Operand(r4));
  __ mov(r1, Operand(r6));

#if defined(V8_HOST_ARCH_ARM)
  int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (FLAG_debug_code) {
    if (frame_alignment > kPointerSize) {
      Label alignment_as_expected;
      ASSERT(IsPowerOf2(frame_alignment));
      __ tst(sp, Operand(frame_alignment_mask));
      __ b(eq, &alignment_as_expected);
      // Don't use Check here, as it will call Runtime_Abort re-entering here.
      __ stop("Unexpected alignment");
      __ bind(&alignment_as_expected);
    }
  }
#endif

  // TODO(1242173): To let the GC traverse the return address of the exit
  // frames, we need to know where the return address is. Right now,
  // we store it on the stack to be able to find it again, but we never
  // restore from it in case of changes, which makes it impossible to
  // support moving the C entry code stub. This should be fixed, but currently
  // this is OK because the CEntryStub gets generated so early in the V8 boot
  // sequence that it is not moving ever.

  // Compute the return address in lr to return to after the jump below. Pc is
  // already at '+ 8' from the current instruction but return is after three
  // instructions so add another 4 to pc to get the return address.
  masm->add(lr, pc, Operand(4));
  __ str(lr, MemOperand(sp, 0));
  masm->Jump(r5);

  if (always_allocate) {
    // It's okay to clobber r2 and r3 here. Don't mess with r0 and r1
    // though (contain the result).
    __ mov(r2, Operand(scope_depth));
    __ ldr(r3, MemOperand(r2));
    __ sub(r3, r3, Operand(1));
    __ str(r3, MemOperand(r2));
  }

  // check for failure result
  Label failure_returned;
  STATIC_ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
  // Lower 2 bits of r2 are 0 iff r0 has failure tag.
  __ add(r2, r0, Operand(1));
  __ tst(r2, Operand(kFailureTagMask));
  __ b(eq, &failure_returned);

  // Exit C frame and return.
  // r0:r1: result
  // sp: stack pointer
  // fp: frame pointer
  //  Callee-saved register r4 still holds argc.
  __ LeaveExitFrame(save_doubles_, r4);
  __ mov(pc, lr);

  // check if we should retry or throw exception
  Label retry;
  __ bind(&failure_returned);
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ tst(r0, Operand(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ b(eq, &retry);

  // Special handling of out of memory exceptions.
  Failure* out_of_memory = Failure::OutOfMemoryException();
  __ cmp(r0, Operand(reinterpret_cast<int32_t>(out_of_memory)));
  __ b(eq, throw_out_of_memory_exception);

  // Retrieve the pending exception and clear the variable.
  __ mov(ip, Operand(ExternalReference::the_hole_value_location()));
  __ ldr(r3, MemOperand(ip));
  __ mov(ip, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ ldr(r0, MemOperand(ip));
  __ str(r3, MemOperand(ip));

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ cmp(r0, Operand(Factory::termination_exception()));
  __ b(eq, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  __ bind(&retry);  // pass last failure (r0) as parameter (r0) when retrying
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function
  // r0: number of arguments including receiver
  // r1: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)

  // Result returned in r0 or r0+r1 by default.

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  // Compute the argv pointer in a callee-saved register.
  __ add(r6, sp, Operand(r0, LSL, kPointerSizeLog2));
  __ sub(r6, r6, Operand(kPointerSize));

  // Enter the exit frame that transitions from JavaScript to C++.
  __ EnterExitFrame(save_doubles_);

  // Setup argc and the builtin function in callee-saved registers.
  __ mov(r4, Operand(r0));
  __ mov(r5, Operand(r1));

  // r4: number of arguments (C callee-saved)
  // r5: pointer to builtin function (C callee-saved)
  // r6: pointer to first argument (C callee-saved)

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
  __ mov(r0, Operand(reinterpret_cast<int32_t>(failure)));
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
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // [sp+0]: argv

  Label invoke, exit;

  // Called from C, so do not pop argc and args on exit (preserve sp)
  // No need to save register-passed args
  // Save callee-saved registers (incl. cp and fp), sp, and lr
  __ stm(db_w, sp, kCalleeSaved | lr.bit());

  // Get address of argv, see stm above.
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  __ ldr(r4, MemOperand(sp, (kNumCalleeSaved + 1) * kPointerSize));  // argv

  // Push a frame with special values setup to mark it as an entry frame.
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  __ mov(r8, Operand(-1));  // Push a bad frame pointer to fail if it is used.
  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ mov(r7, Operand(Smi::FromInt(marker)));
  __ mov(r6, Operand(Smi::FromInt(marker)));
  __ mov(r5, Operand(ExternalReference(Top::k_c_entry_fp_address)));
  __ ldr(r5, MemOperand(r5));
  __ Push(r8, r7, r6, r5);

  // Setup frame pointer for the frame to be pushed.
  __ add(fp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Top::k_js_entry_sp_address);
  __ mov(r5, Operand(ExternalReference(js_entry_sp)));
  __ ldr(r6, MemOperand(r5));
  __ cmp(r6, Operand(0, RelocInfo::NONE));
  __ str(fp, MemOperand(r5), eq);
#endif

  // Call a faked try-block that does the invoke.
  __ bl(&invoke);

  // Caught exception: Store result (exception) in the pending
  // exception field in the JSEnv and return a failure sentinel.
  // Coming in here the fp will be invalid because the PushTryHandler below
  // sets it to 0 to signal the existence of the JSEntry frame.
  __ mov(ip, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ str(r0, MemOperand(ip));
  __ mov(r0, Operand(reinterpret_cast<int32_t>(Failure::Exception())));
  __ b(&exit);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r0-r4, r5-r7 are available.
  __ PushTryHandler(IN_JS_ENTRY, JS_ENTRY_HANDLER);
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the bl(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Clear any pending exceptions.
  __ mov(ip, Operand(ExternalReference::the_hole_value_location()));
  __ ldr(r5, MemOperand(ip));
  __ mov(ip, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ str(r5, MemOperand(ip));

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  if (is_construct) {
    ExternalReference construct_entry(Builtins::JSConstructEntryTrampoline);
    __ mov(ip, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::JSEntryTrampoline);
    __ mov(ip, Operand(entry));
  }
  __ ldr(ip, MemOperand(ip));  // deref address

  // Branch and link to JSEntryTrampoline.  We don't use the double underscore
  // macro for the add instruction because we don't want the coverage tool
  // inserting instructions here after we read the pc.
  __ mov(lr, Operand(pc));
  masm->add(pc, ip, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Unlink this frame from the handler chain. When reading the
  // address of the next handler, there is no need to use the address
  // displacement since the current stack pointer (sp) points directly
  // to the stack handler.
  __ ldr(r3, MemOperand(sp, StackHandlerConstants::kNextOffset));
  __ mov(ip, Operand(ExternalReference(Top::k_handler_address)));
  __ str(r3, MemOperand(ip));
  // No need to restore registers
  __ add(sp, sp, Operand(StackHandlerConstants::kSize));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // If current FP value is the same as js_entry_sp value, it means that
  // the current function is the outermost.
  __ mov(r5, Operand(ExternalReference(js_entry_sp)));
  __ ldr(r6, MemOperand(r5));
  __ cmp(fp, Operand(r6));
  __ mov(r6, Operand(0, RelocInfo::NONE), LeaveCC, eq);
  __ str(r6, MemOperand(r5), eq);
#endif

  __ bind(&exit);  // r0 holds result
  // Restore the top frame descriptors from the stack.
  __ pop(r3);
  __ mov(ip, Operand(ExternalReference(Top::k_c_entry_fp_address)));
  __ str(r3, MemOperand(ip));

  // Reset the stack to the callee saved registers.
  __ add(sp, sp, Operand(-EntryFrameConstants::kCallerFPOffset));

  // Restore callee-saved registers and return.
#ifdef DEBUG
  if (FLAG_debug_code) {
    __ mov(lr, Operand(pc));
  }
#endif
  __ ldm(ia_w, sp, kCalleeSaved | pc.bit());
}


// Uses registers r0 to r4.
// Expected input (depending on whether args are in registers or on the stack):
// * object: r0 or at sp + 1 * kPointerSize.
// * function: r1 or at sp.
//
// An inlined call site may have been generated before calling this stub.
// In this case the offset to the inline site to patch is passed on the stack,
// in the safepoint slot for register r4.
// (See LCodeGen::DoInstanceOfKnownGlobal)
void InstanceofStub::Generate(MacroAssembler* masm) {
  // Call site inlining and patching implies arguments in registers.
  ASSERT(HasArgsInRegisters() || !HasCallSiteInlineCheck());
  // ReturnTrueFalse is only implemented for inlined call sites.
  ASSERT(!ReturnTrueFalseObject() || HasCallSiteInlineCheck());

  // Fixed register usage throughout the stub:
  const Register object = r0;  // Object (lhs).
  Register map = r3;  // Map of the object.
  const Register function = r1;  // Function (rhs).
  const Register prototype = r4;  // Prototype of the function.
  const Register inline_site = r9;
  const Register scratch = r2;

  const int32_t kDeltaToLoadBoolResult = 3 * kPointerSize;

  Label slow, loop, is_instance, is_not_instance, not_js_object;

  if (!HasArgsInRegisters()) {
    __ ldr(object, MemOperand(sp, 1 * kPointerSize));
    __ ldr(function, MemOperand(sp, 0));
  }

  // Check that the left hand is a JS object and load map.
  __ JumpIfSmi(object, &not_js_object);
  __ IsObjectJSObjectType(object, map, scratch, &not_js_object);

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck()) {
    Label miss;
    __ LoadRoot(ip, Heap::kInstanceofCacheFunctionRootIndex);
    __ cmp(function, ip);
    __ b(ne, &miss);
    __ LoadRoot(ip, Heap::kInstanceofCacheMapRootIndex);
    __ cmp(map, ip);
    __ b(ne, &miss);
    __ LoadRoot(r0, Heap::kInstanceofCacheAnswerRootIndex);
    __ Ret(HasArgsInRegisters() ? 0 : 2);

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
    ASSERT(HasArgsInRegisters());
    // Patch the (relocated) inlined map check.

    // The offset was stored in r4 safepoint slot.
    // (See LCodeGen::DoDeferredLInstanceOfKnownGlobal)
    __ LoadFromSafepointRegisterSlot(scratch, r4);
    __ sub(inline_site, lr, scratch);
    // Get the map location in scratch and patch it.
    __ GetRelocatedValueLocation(inline_site, scratch);
    __ str(map, MemOperand(scratch));
  }

  // Register mapping: r3 is object map and r4 is function prototype.
  // Get prototype of object into r2.
  __ ldr(scratch, FieldMemOperand(map, Map::kPrototypeOffset));

  // We don't need map any more. Use it as a scratch register.
  Register scratch2 = map;
  map = no_reg;

  // Loop through the prototype chain looking for the function prototype.
  __ LoadRoot(scratch2, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ cmp(scratch, Operand(prototype));
  __ b(eq, &is_instance);
  __ cmp(scratch, scratch2);
  __ b(eq, &is_not_instance);
  __ ldr(scratch, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ ldr(scratch, FieldMemOperand(scratch, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  if (!HasCallSiteInlineCheck()) {
    __ mov(r0, Operand(Smi::FromInt(0)));
    __ StoreRoot(r0, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Patch the call site to return true.
    __ LoadRoot(r0, Heap::kTrueValueRootIndex);
    __ add(inline_site, inline_site, Operand(kDeltaToLoadBoolResult));
    // Get the boolean result location in scratch and patch it.
    __ GetRelocatedValueLocation(inline_site, scratch);
    __ str(r0, MemOperand(scratch));

    if (!ReturnTrueFalseObject()) {
      __ mov(r0, Operand(Smi::FromInt(0)));
    }
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    __ mov(r0, Operand(Smi::FromInt(1)));
    __ StoreRoot(r0, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Patch the call site to return false.
    __ LoadRoot(r0, Heap::kFalseValueRootIndex);
    __ add(inline_site, inline_site, Operand(kDeltaToLoadBoolResult));
    // Get the boolean result location in scratch and patch it.
    __ GetRelocatedValueLocation(inline_site, scratch);
    __ str(r0, MemOperand(scratch));

    if (!ReturnTrueFalseObject()) {
      __ mov(r0, Operand(Smi::FromInt(1)));
    }
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  Label object_not_null, object_not_null_or_smi;
  __ bind(&not_js_object);
  // Before null, smi and string value checks, check that the rhs is a function
  // as for a non-function rhs an exception needs to be thrown.
  __ JumpIfSmi(function, &slow);
  __ CompareObjectType(function, scratch2, scratch, JS_FUNCTION_TYPE);
  __ b(ne, &slow);

  // Null is not instance of anything.
  __ cmp(scratch, Operand(Factory::null_value()));
  __ b(ne, &object_not_null);
  __ mov(r0, Operand(Smi::FromInt(1)));
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null);
  // Smi values are not instances of anything.
  __ JumpIfNotSmi(object, &object_not_null_or_smi);
  __ mov(r0, Operand(Smi::FromInt(1)));
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null_or_smi);
  // String values are not instances of anything.
  __ IsObjectJSStringType(object, scratch, &slow);
  __ mov(r0, Operand(Smi::FromInt(1)));
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  // Slow-case.  Tail call builtin.
  __ bind(&slow);
  if (!ReturnTrueFalseObject()) {
    if (HasArgsInRegisters()) {
      __ Push(r0, r1);
    }
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_JS);
  } else {
    __ EnterInternalFrame();
    __ Push(r0, r1);
    __ InvokeBuiltin(Builtins::INSTANCE_OF, CALL_JS);
    __ LeaveInternalFrame();
    __ cmp(r0, Operand(0));
    __ LoadRoot(r0, Heap::kTrueValueRootIndex, eq);
    __ LoadRoot(r0, Heap::kFalseValueRootIndex, ne);
    __ Ret(HasArgsInRegisters() ? 0 : 2);
  }
}


Register InstanceofStub::left() { return r0; }


Register InstanceofStub::right() { return r1; }


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The displacement is the offset of the last parameter (if any)
  // relative to the frame pointer.
  static const int kDisplacement =
      StandardFrameConstants::kCallerSPOffset - kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(r1, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &adaptor);

  // Check index against formal parameters count limit passed in
  // through register r0. Use unsigned comparison to get negative
  // check for free.
  __ cmp(r1, r0);
  __ b(hs, &slow);

  // Read the argument from the stack and return it.
  __ sub(r3, r0, r1);
  __ add(r3, fp, Operand(r3, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(r0, MemOperand(r3, kDisplacement));
  __ Jump(lr);

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ ldr(r0, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmp(r1, r0);
  __ b(cs, &slow);

  // Read the argument from the adaptor frame and return it.
  __ sub(r3, r0, r1);
  __ add(r3, r2, Operand(r3, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(r0, MemOperand(r3, kDisplacement));
  __ Jump(lr);

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ push(r1);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewObject(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[4] : receiver displacement
  // sp[8] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ ldr(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(r3, MemOperand(r2, StandardFrameConstants::kContextOffset));
  __ cmp(r3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(eq, &adaptor_frame);

  // Get the length from the frame.
  __ ldr(r1, MemOperand(sp, 0));
  __ b(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ ldr(r1, MemOperand(r2, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ str(r1, MemOperand(sp, 0));
  __ add(r3, r2, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(r3, r3, Operand(StandardFrameConstants::kCallerSPOffset));
  __ str(r3, MemOperand(sp, 1 * kPointerSize));

  // Try the new space allocation. Start out with computing the size
  // of the arguments object and the elements array in words.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ cmp(r1, Operand(0, RelocInfo::NONE));
  __ b(eq, &add_arguments_object);
  __ mov(r1, Operand(r1, LSR, kSmiTagSize));
  __ add(r1, r1, Operand(FixedArray::kHeaderSize / kPointerSize));
  __ bind(&add_arguments_object);
  __ add(r1, r1, Operand(Heap::kArgumentsObjectSize / kPointerSize));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(
      r1,
      r0,
      r2,
      r3,
      &runtime,
      static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));

  // Get the arguments boilerplate from the current (global) context.
  int offset = Context::SlotOffset(Context::ARGUMENTS_BOILERPLATE_INDEX);
  __ ldr(r4, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ ldr(r4, FieldMemOperand(r4, GlobalObject::kGlobalContextOffset));
  __ ldr(r4, MemOperand(r4, offset));

  // Copy the JS object part.
  __ CopyFields(r0, r4, r3.bit(), JSObject::kHeaderSize / kPointerSize);

  // Setup the callee in-object property.
  STATIC_ASSERT(Heap::arguments_callee_index == 0);
  __ ldr(r3, MemOperand(sp, 2 * kPointerSize));
  __ str(r3, FieldMemOperand(r0, JSObject::kHeaderSize));

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::arguments_length_index == 1);
  __ ldr(r1, MemOperand(sp, 0 * kPointerSize));
  __ str(r1, FieldMemOperand(r0, JSObject::kHeaderSize + kPointerSize));

  // If there are no actual arguments, we're done.
  Label done;
  __ cmp(r1, Operand(0, RelocInfo::NONE));
  __ b(eq, &done);

  // Get the parameters pointer from the stack.
  __ ldr(r2, MemOperand(sp, 1 * kPointerSize));

  // Setup the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ add(r4, r0, Operand(Heap::kArgumentsObjectSize));
  __ str(r4, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ LoadRoot(r3, Heap::kFixedArrayMapRootIndex);
  __ str(r3, FieldMemOperand(r4, FixedArray::kMapOffset));
  __ str(r1, FieldMemOperand(r4, FixedArray::kLengthOffset));
  __ mov(r1, Operand(r1, LSR, kSmiTagSize));  // Untag the length for the loop.

  // Copy the fixed array slots.
  Label loop;
  // Setup r4 to point to the first array slot.
  __ add(r4, r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ bind(&loop);
  // Pre-decrement r2 with kPointerSize on each iteration.
  // Pre-decrement in order to skip receiver.
  __ ldr(r3, MemOperand(r2, kPointerSize, NegPreIndex));
  // Post-increment r4 with kPointerSize on each iteration.
  __ str(r3, MemOperand(r4, kPointerSize, PostIndex));
  __ sub(r1, r1, Operand(1));
  __ cmp(r1, Operand(0, RelocInfo::NONE));
  __ b(ne, &loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
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
  Register subject = r4;
  Register regexp_data = r5;
  Register last_match_info_elements = r6;

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address();
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size();
  __ mov(r0, Operand(address_of_regexp_stack_memory_size));
  __ ldr(r0, MemOperand(r0, 0));
  __ tst(r0, Operand(r0));
  __ b(eq, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ ldr(r0, MemOperand(sp, kJSRegExpOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  __ CompareObjectType(r0, r1, r1, JS_REGEXP_TYPE);
  __ b(ne, &runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ ldr(regexp_data, FieldMemOperand(r0, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ tst(regexp_data, Operand(kSmiTagMask));
    __ Check(ne, "Unexpected type for RegExp data, FixedArray expected");
    __ CompareObjectType(regexp_data, r0, r0, FIXED_ARRAY_TYPE);
    __ Check(eq, "Unexpected type for RegExp data, FixedArray expected");
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ ldr(r0, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  __ cmp(r0, Operand(Smi::FromInt(JSRegExp::IRREGEXP)));
  __ b(ne, &runtime);

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ ldr(r2,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2. This
  // uses the asumption that smis are 2 * their untagged value.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(r2, r2, Operand(2));  // r2 was a smi.
  // Check that the static offsets vector buffer is large enough.
  __ cmp(r2, Operand(OffsetsVector::kStaticOffsetsVectorSize));
  __ b(hi, &runtime);

  // r2: Number of capture registers
  // regexp_data: RegExp data (FixedArray)
  // Check that the second argument is a string.
  __ ldr(subject, MemOperand(sp, kSubjectOffset));
  __ tst(subject, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  Condition is_string = masm->IsObjectStringType(subject, r0);
  __ b(NegateCondition(is_string), &runtime);
  // Get the length of the string to r3.
  __ ldr(r3, FieldMemOperand(subject, String::kLengthOffset));

  // r2: Number of capture registers
  // r3: Length of subject string as a smi
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check that the third argument is a positive smi less than the subject
  // string length. A negative value will be greater (unsigned comparison).
  __ ldr(r0, MemOperand(sp, kPreviousIndexOffset));
  __ tst(r0, Operand(kSmiTagMask));
  __ b(ne, &runtime);
  __ cmp(r3, Operand(r0));
  __ b(ls, &runtime);

  // r2: Number of capture registers
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check that the fourth object is a JSArray object.
  __ ldr(r0, MemOperand(sp, kLastMatchInfoOffset));
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  __ CompareObjectType(r0, r1, r1, JS_ARRAY_TYPE);
  __ b(ne, &runtime);
  // Check that the JSArray is in fast case.
  __ ldr(last_match_info_elements,
         FieldMemOperand(r0, JSArray::kElementsOffset));
  __ ldr(r0, FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
  __ cmp(r0, ip);
  __ b(ne, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ ldr(r0,
         FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ add(r2, r2, Operand(RegExpImpl::kLastMatchOverhead));
  __ cmp(r2, Operand(r0, ASR, kSmiTagSize));
  __ b(gt, &runtime);

  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_string;
  __ ldr(r0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));
  // First check for flat string.
  __ tst(r0, Operand(kIsNotStringMask | kStringRepresentationMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ b(eq, &seq_string);

  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Check for flat cons string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  STATIC_ASSERT(kExternalStringTag !=0);
  STATIC_ASSERT((kConsStringTag & kExternalStringTag) == 0);
  __ tst(r0, Operand(kIsNotStringMask | kExternalStringTag));
  __ b(ne, &runtime);
  __ ldr(r0, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ LoadRoot(r1, Heap::kEmptyStringRootIndex);
  __ cmp(r0, r1);
  __ b(ne, &runtime);
  __ ldr(subject, FieldMemOperand(subject, ConsString::kFirstOffset));
  __ ldr(r0, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));
  // Is first part a flat string?
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(r0, Operand(kStringRepresentationMask));
  __ b(ne, &runtime);

  __ bind(&seq_string);
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // r0: Instance type of subject string
  STATIC_ASSERT(4 == kAsciiStringTag);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  // Find the code object based on the assumptions above.
  __ and_(r0, r0, Operand(kStringEncodingMask));
  __ mov(r3, Operand(r0, ASR, 2), SetCC);
  __ ldr(r7, FieldMemOperand(regexp_data, JSRegExp::kDataAsciiCodeOffset), ne);
  __ ldr(r7, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset), eq);

  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // the hole.
  __ CompareObjectType(r7, r0, r0, CODE_TYPE);
  __ b(ne, &runtime);

  // r3: encoding of subject string (1 if ascii, 0 if two_byte);
  // r7: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ ldr(r1, MemOperand(sp, kPreviousIndexOffset));
  __ mov(r1, Operand(r1, ASR, kSmiTagSize));

  // r1: previous index
  // r3: encoding of subject string (1 if ascii, 0 if two_byte);
  // r7: code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(&Counters::regexp_entry_native, 1, r0, r2);

  static const int kRegExpExecuteArguments = 7;
  static const int kParameterRegisters = 4;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers.

  // Argument 7 (sp[12]): Indicate that this is a direct call from JavaScript.
  __ mov(r0, Operand(1));
  __ str(r0, MemOperand(sp, 3 * kPointerSize));

  // Argument 6 (sp[8]): Start (high end) of backtracking stack memory area.
  __ mov(r0, Operand(address_of_regexp_stack_memory_address));
  __ ldr(r0, MemOperand(r0, 0));
  __ mov(r2, Operand(address_of_regexp_stack_memory_size));
  __ ldr(r2, MemOperand(r2, 0));
  __ add(r0, r0, Operand(r2));
  __ str(r0, MemOperand(sp, 2 * kPointerSize));

  // Argument 5 (sp[4]): static offsets vector buffer.
  __ mov(r0, Operand(ExternalReference::address_of_static_offsets_vector()));
  __ str(r0, MemOperand(sp, 1 * kPointerSize));

  // For arguments 4 and 3 get string length, calculate start of string data and
  // calculate the shift of the index (0 for ASCII and 1 for two byte).
  __ ldr(r0, FieldMemOperand(subject, String::kLengthOffset));
  __ mov(r0, Operand(r0, ASR, kSmiTagSize));
  STATIC_ASSERT(SeqAsciiString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ add(r9, subject, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ eor(r3, r3, Operand(1));
  // Argument 4 (r3): End of string data
  // Argument 3 (r2): Start of string data
  __ add(r2, r9, Operand(r1, LSL, r3));
  __ add(r3, r9, Operand(r0, LSL, r3));

  // Argument 2 (r1): Previous index.
  // Already there

  // Argument 1 (r0): Subject string.
  __ mov(r0, subject);

  // Locate the code entry and call it.
  __ add(r7, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  DirectCEntryStub stub;
  stub.GenerateCall(masm, r7);

  __ LeaveExitFrame(false, no_reg);

  // r0: result
  // subject: subject string (callee saved)
  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)

  // Check the result.
  Label success;

  __ cmp(r0, Operand(NativeRegExpMacroAssembler::SUCCESS));
  __ b(eq, &success);
  Label failure;
  __ cmp(r0, Operand(NativeRegExpMacroAssembler::FAILURE));
  __ b(eq, &failure);
  __ cmp(r0, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ b(ne, &runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ mov(r1, Operand(ExternalReference::the_hole_value_location()));
  __ ldr(r1, MemOperand(r1, 0));
  __ mov(r2, Operand(ExternalReference(Top::k_pending_exception_address)));
  __ ldr(r0, MemOperand(r2, 0));
  __ cmp(r0, r1);
  __ b(eq, &runtime);

  __ str(r1, MemOperand(r2, 0));  // Clear pending exception.

  // Check if the exception is a termination. If so, throw as uncatchable.
  __ LoadRoot(ip, Heap::kTerminationExceptionRootIndex);
  __ cmp(r0, ip);
  Label termination_exception;
  __ b(eq, &termination_exception);

  __ Throw(r0);  // Expects thrown value in r0.

  __ bind(&termination_exception);
  __ ThrowUncatchable(TERMINATION, r0);  // Expects thrown value in r0.

  __ bind(&failure);
  // For failure and exception return null.
  __ mov(r0, Operand(Factory::null_value()));
  __ add(sp, sp, Operand(4 * kPointerSize));
  __ Ret();

  // Process the result from the native regexp code.
  __ bind(&success);
  __ ldr(r1,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  __ add(r1, r1, Operand(2));  // r1 was a smi.

  // r1: number of capture registers
  // r4: subject string
  // Store the capture count.
  __ mov(r2, Operand(r1, LSL, kSmiTagSize + kSmiShiftSize));  // To smi.
  __ str(r2, FieldMemOperand(last_match_info_elements,
                             RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ mov(r3, last_match_info_elements);  // Moved up to reduce latency.
  __ str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastSubjectOffset));
  __ RecordWrite(r3, Operand(RegExpImpl::kLastSubjectOffset), r2, r7);
  __ str(subject,
         FieldMemOperand(last_match_info_elements,
                         RegExpImpl::kLastInputOffset));
  __ mov(r3, last_match_info_elements);
  __ RecordWrite(r3, Operand(RegExpImpl::kLastInputOffset), r2, r7);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector();
  __ mov(r2, Operand(address_of_static_offsets_vector));

  // r1: number of capture registers
  // r2: offsets vector
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ add(r0,
         last_match_info_elements,
         Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag));
  __ bind(&next_capture);
  __ sub(r1, r1, Operand(1), SetCC);
  __ b(mi, &done);
  // Read the value from the static offsets vector buffer.
  __ ldr(r3, MemOperand(r2, kPointerSize, PostIndex));
  // Store the smi value in the last match info.
  __ mov(r3, Operand(r3, LSL, kSmiTagSize));
  __ str(r3, MemOperand(r0, kPointerSize, PostIndex));
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ ldr(r0, MemOperand(sp, kLastMatchInfoOffset));
  __ add(sp, sp, Operand(4 * kPointerSize));
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
  __ ldr(r1, MemOperand(sp, kPointerSize * 2));
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  __ tst(r1, Operand(kSmiTagMask));
  __ b(ne, &slowcase);
  __ cmp(r1, Operand(Smi::FromInt(kMaxInlineLength)));
  __ b(hi, &slowcase);
  // Smi-tagging is equivalent to multiplying by 2.
  // Allocate RegExpResult followed by FixedArray with size in ebx.
  // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
  // Elements:  [Map][Length][..elements..]
  // Size of JSArray with two in-object properties and the header of a
  // FixedArray.
  int objects_size =
      (JSRegExpResult::kSize + FixedArray::kHeaderSize) / kPointerSize;
  __ mov(r5, Operand(r1, LSR, kSmiTagSize + kSmiShiftSize));
  __ add(r2, r5, Operand(objects_size));
  __ AllocateInNewSpace(
      r2,  // In: Size, in words.
      r0,  // Out: Start of allocation (tagged).
      r3,  // Scratch register.
      r4,  // Scratch register.
      &slowcase,
      static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));
  // r0: Start of allocated area, object-tagged.
  // r1: Number of elements in array, as smi.
  // r5: Number of elements, untagged.

  // Set JSArray map to global.regexp_result_map().
  // Set empty properties FixedArray.
  // Set elements to point to FixedArray allocated right after the JSArray.
  // Interleave operations for better latency.
  __ ldr(r2, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ add(r3, r0, Operand(JSRegExpResult::kSize));
  __ mov(r4, Operand(Factory::empty_fixed_array()));
  __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalContextOffset));
  __ str(r3, FieldMemOperand(r0, JSObject::kElementsOffset));
  __ ldr(r2, ContextOperand(r2, Context::REGEXP_RESULT_MAP_INDEX));
  __ str(r4, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r2, FieldMemOperand(r0, HeapObject::kMapOffset));

  // Set input, index and length fields from arguments.
  __ ldr(r1, MemOperand(sp, kPointerSize * 0));
  __ str(r1, FieldMemOperand(r0, JSRegExpResult::kInputOffset));
  __ ldr(r1, MemOperand(sp, kPointerSize * 1));
  __ str(r1, FieldMemOperand(r0, JSRegExpResult::kIndexOffset));
  __ ldr(r1, MemOperand(sp, kPointerSize * 2));
  __ str(r1, FieldMemOperand(r0, JSArray::kLengthOffset));

  // Fill out the elements FixedArray.
  // r0: JSArray, tagged.
  // r3: FixedArray, tagged.
  // r5: Number of elements in array, untagged.

  // Set map.
  __ mov(r2, Operand(Factory::fixed_array_map()));
  __ str(r2, FieldMemOperand(r3, HeapObject::kMapOffset));
  // Set FixedArray length.
  __ mov(r6, Operand(r5, LSL, kSmiTagSize));
  __ str(r6, FieldMemOperand(r3, FixedArray::kLengthOffset));
  // Fill contents of fixed-array with the-hole.
  __ mov(r2, Operand(Factory::the_hole_value()));
  __ add(r3, r3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // Fill fixed array elements with hole.
  // r0: JSArray, tagged.
  // r2: the hole.
  // r3: Start of elements in FixedArray.
  // r5: Number of elements to fill.
  Label loop;
  __ tst(r5, Operand(r5));
  __ bind(&loop);
  __ b(le, &done);  // Jump if r1 is negative or zero.
  __ sub(r5, r5, Operand(1), SetCC);
  __ str(r2, MemOperand(r3, r5, LSL, kPointerSizeLog2));
  __ jmp(&loop);

  __ bind(&done);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&slowcase);
  __ TailCallRuntime(Runtime::kRegExpConstructResult, 3, 1);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  Label slow;

  // If the receiver might be a value (string, number or boolean) check for this
  // and box it if it is.
  if (ReceiverMightBeValue()) {
    // Get the receiver from the stack.
    // function, receiver [, arguments]
    Label receiver_is_value, receiver_is_js_object;
    __ ldr(r1, MemOperand(sp, argc_ * kPointerSize));

    // Check if receiver is a smi (which is a number value).
    __ JumpIfSmi(r1, &receiver_is_value);

    // Check if the receiver is a valid JS object.
    __ CompareObjectType(r1, r2, r2, FIRST_JS_OBJECT_TYPE);
    __ b(ge, &receiver_is_js_object);

    // Call the runtime to box the value.
    __ bind(&receiver_is_value);
    __ EnterInternalFrame();
    __ push(r1);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_JS);
    __ LeaveInternalFrame();
    __ str(r0, MemOperand(sp, argc_ * kPointerSize));

    __ bind(&receiver_is_js_object);
  }

  // Get the function to call from the stack.
  // function, receiver [, arguments]
  __ ldr(r1, MemOperand(sp, (argc_ + 1) * kPointerSize));

  // Check that the function is really a JavaScript function.
  // r1: pushed function (to be verified)
  __ JumpIfSmi(r1, &slow);
  // Get the map of the function object.
  __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
  __ b(ne, &slow);

  // Fast-case: Invoke the function now.
  // r1: pushed function
  ParameterCount actual(argc_);
  __ InvokeFunction(r1, actual, JUMP_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ str(r1, MemOperand(sp, argc_ * kPointerSize));
  __ mov(r0, Operand(argc_));  // Setup the number of arguments.
  __ mov(r2, Operand(0, RelocInfo::NONE));
  __ GetBuiltinEntry(r3, Builtins::CALL_NON_FUNCTION);
  __ Jump(Handle<Code>(Builtins::builtin(Builtins::ArgumentsAdaptorTrampoline)),
          RelocInfo::CODE_TARGET);
}


// Unfortunately you have to run without snapshots to see most of these
// names in the profile since most compare stubs end up in the snapshot.
const char* CompareStub::GetName() {
  ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
         (lhs_.is(r1) && rhs_.is(r0)));

  if (name_ != NULL) return name_;
  const int kMaxNameLength = 100;
  name_ = Bootstrapper::AllocateAutoDeletedArray(kMaxNameLength);
  if (name_ == NULL) return "OOM";

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

  const char* lhs_name = lhs_.is(r0) ? "_r0" : "_r1";
  const char* rhs_name = rhs_.is(r0) ? "_r0" : "_r1";

  const char* strict_name = "";
  if (strict_ && (cc_ == eq || cc_ == ne)) {
    strict_name = "_STRICT";
  }

  const char* never_nan_nan_name = "";
  if (never_nan_nan_ && (cc_ == eq || cc_ == ne)) {
    never_nan_nan_name = "_NO_NAN";
  }

  const char* include_number_compare_name = "";
  if (!include_number_compare_) {
    include_number_compare_name = "_NO_NUMBER";
  }

  const char* include_smi_compare_name = "";
  if (!include_smi_compare_) {
    include_smi_compare_name = "_NO_SMI";
  }

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "CompareStub_%s%s%s%s%s%s",
               cc_name,
               lhs_name,
               rhs_name,
               strict_name,
               never_nan_nan_name,
               include_number_compare_name,
               include_smi_compare_name);
  return name_;
}


int CompareStub::MinorKey() {
  // Encode the three parameters in a unique 16 bit value. To avoid duplicate
  // stubs the never NaN NaN condition is only taken into account if the
  // condition is equals.
  ASSERT((static_cast<unsigned>(cc_) >> 28) < (1 << 12));
  ASSERT((lhs_.is(r0) && rhs_.is(r1)) ||
         (lhs_.is(r1) && rhs_.is(r0)));
  return ConditionField::encode(static_cast<unsigned>(cc_) >> 28)
         | RegisterField::encode(lhs_.is(r0))
         | StrictField::encode(strict_)
         | NeverNanNanField::encode(cc_ == eq ? never_nan_nan_ : false)
         | IncludeNumberCompareField::encode(include_number_compare_)
         | IncludeSmiCompareField::encode(include_smi_compare_);
}


// StringCharCodeAtGenerator
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  Label flat_string;
  Label ascii_string;
  Label got_char_code;

  // If the receiver is a smi trigger the non-string case.
  __ JumpIfSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ tst(result_, Operand(kIsNotStringMask));
  __ b(ne, receiver_not_string_);

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);

  // Put smi-tagged index into scratch register.
  __ mov(scratch_, index_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ ldr(ip, FieldMemOperand(object_, String::kLengthOffset));
  __ cmp(ip, Operand(scratch_));
  __ b(ls, index_out_of_range_);

  // We need special handling for non-flat strings.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(result_, Operand(kStringRepresentationMask));
  __ b(eq, &flat_string);

  // Handle non-flat strings.
  __ tst(result_, Operand(kIsConsStringMask));
  __ b(eq, &call_runtime_);

  // ConsString.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ ldr(result_, FieldMemOperand(object_, ConsString::kSecondOffset));
  __ LoadRoot(ip, Heap::kEmptyStringRootIndex);
  __ cmp(result_, Operand(ip));
  __ b(ne, &call_runtime_);
  // Get the first of the two strings and load its instance type.
  __ ldr(object_, FieldMemOperand(object_, ConsString::kFirstOffset));
  __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  // If the first cons component is also non-flat, then go to runtime.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(result_, Operand(kStringRepresentationMask));
  __ b(ne, &call_runtime_);

  // Check for 1-byte or 2-byte string.
  __ bind(&flat_string);
  STATIC_ASSERT(kAsciiStringTag != 0);
  __ tst(result_, Operand(kStringEncodingMask));
  __ b(ne, &ascii_string);

  // 2-byte string.
  // Load the 2-byte character code into the result register. We can
  // add without shifting since the smi tag size is the log2 of the
  // number of bytes in a two-byte character.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1 && kSmiShiftSize == 0);
  __ add(scratch_, object_, Operand(scratch_));
  __ ldrh(result_, FieldMemOperand(scratch_, SeqTwoByteString::kHeaderSize));
  __ jmp(&got_char_code);

  // ASCII string.
  // Load the byte into the result register.
  __ bind(&ascii_string);
  __ add(scratch_, object_, Operand(scratch_, LSR, kSmiTagSize));
  __ ldrb(result_, FieldMemOperand(scratch_, SeqAsciiString::kHeaderSize));

  __ bind(&got_char_code);
  __ mov(result_, Operand(result_, LSL, kSmiTagSize));
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
              true);
  call_helper.BeforeCall(masm);
  __ Push(object_, index_);
  __ push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    ASSERT(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  // Save the conversion result before the pop instructions below
  // have a chance to overwrite it.
  __ Move(scratch_, r0);
  __ pop(index_);
  __ pop(object_);
  // Reload the instance type.
  __ ldr(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ ldrb(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(scratch_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);
  __ Move(result_, r0);
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharCodeAt slow case");
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiShiftSize == 0);
  ASSERT(IsPowerOf2(String::kMaxAsciiCharCode + 1));
  __ tst(code_,
         Operand(kSmiTagMask |
                 ((~String::kMaxAsciiCharCode) << kSmiTagSize)));
  __ b(ne, &slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  // At this point code register contains smi tagged ascii char code.
  STATIC_ASSERT(kSmiTag == 0);
  __ add(result_, result_, Operand(code_, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(result_, Operand(ip));
  __ b(eq, &slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm, const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharFromCode slow case");

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  __ Move(result_, r0);
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

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
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (!ascii) {
    __ add(count, count, Operand(count), SetCC);
  } else {
    __ cmp(count, Operand(0, RelocInfo::NONE));
  }
  __ b(eq, &done);

  __ bind(&loop);
  __ ldrb(scratch, MemOperand(src, 1, PostIndex));
  // Perform sub between load and dependent store to get the load time to
  // complete.
  __ sub(count, count, Operand(1), SetCC);
  __ strb(scratch, MemOperand(dest, 1, PostIndex));
  // last iteration.
  __ b(gt, &loop);

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
    __ tst(dest, Operand(kPointerAlignmentMask));
    __ Check(eq, "Destination of copy not aligned.");
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
    __ add(count, count, Operand(count), SetCC);
  } else {
    __ cmp(count, Operand(0, RelocInfo::NONE));
  }
  __ b(eq, &done);

  // Assume that you cannot read (or write) unaligned.
  Label byte_loop;
  // Must copy at least eight bytes, otherwise just do it one byte at a time.
  __ cmp(count, Operand(8));
  __ add(count, dest, Operand(count));
  Register limit = count;  // Read until src equals this.
  __ b(lt, &byte_loop);

  if (!dest_always_aligned) {
    // Align dest by byte copying. Copies between zero and three bytes.
    __ and_(scratch4, dest, Operand(kReadAlignmentMask), SetCC);
    Label dest_aligned;
    __ b(eq, &dest_aligned);
    __ cmp(scratch4, Operand(2));
    __ ldrb(scratch1, MemOperand(src, 1, PostIndex));
    __ ldrb(scratch2, MemOperand(src, 1, PostIndex), le);
    __ ldrb(scratch3, MemOperand(src, 1, PostIndex), lt);
    __ strb(scratch1, MemOperand(dest, 1, PostIndex));
    __ strb(scratch2, MemOperand(dest, 1, PostIndex), le);
    __ strb(scratch3, MemOperand(dest, 1, PostIndex), lt);
    __ bind(&dest_aligned);
  }

  Label simple_loop;

  __ sub(scratch4, dest, Operand(src));
  __ and_(scratch4, scratch4, Operand(0x03), SetCC);
  __ b(eq, &simple_loop);
  // Shift register is number of bits in a source word that
  // must be combined with bits in the next source word in order
  // to create a destination word.

  // Complex loop for src/dst that are not aligned the same way.
  {
    Label loop;
    __ mov(scratch4, Operand(scratch4, LSL, 3));
    Register left_shift = scratch4;
    __ and_(src, src, Operand(~3));  // Round down to load previous word.
    __ ldr(scratch1, MemOperand(src, 4, PostIndex));
    // Store the "shift" most significant bits of scratch in the least
    // signficant bits (i.e., shift down by (32-shift)).
    __ rsb(scratch2, left_shift, Operand(32));
    Register right_shift = scratch2;
    __ mov(scratch1, Operand(scratch1, LSR, right_shift));

    __ bind(&loop);
    __ ldr(scratch3, MemOperand(src, 4, PostIndex));
    __ sub(scratch5, limit, Operand(dest));
    __ orr(scratch1, scratch1, Operand(scratch3, LSL, left_shift));
    __ str(scratch1, MemOperand(dest, 4, PostIndex));
    __ mov(scratch1, Operand(scratch3, LSR, right_shift));
    // Loop if four or more bytes left to copy.
    // Compare to eight, because we did the subtract before increasing dst.
    __ sub(scratch5, scratch5, Operand(8), SetCC);
    __ b(ge, &loop);
  }
  // There is now between zero and three bytes left to copy (negative that
  // number is in scratch5), and between one and three bytes already read into
  // scratch1 (eight times that number in scratch4). We may have read past
  // the end of the string, but because objects are aligned, we have not read
  // past the end of the object.
  // Find the minimum of remaining characters to move and preloaded characters
  // and write those as bytes.
  __ add(scratch5, scratch5, Operand(4), SetCC);
  __ b(eq, &done);
  __ cmp(scratch4, Operand(scratch5, LSL, 3), ne);
  // Move minimum of bytes read and bytes left to copy to scratch4.
  __ mov(scratch5, Operand(scratch4, LSR, 3), LeaveCC, lt);
  // Between one and three (value in scratch5) characters already read into
  // scratch ready to write.
  __ cmp(scratch5, Operand(2));
  __ strb(scratch1, MemOperand(dest, 1, PostIndex));
  __ mov(scratch1, Operand(scratch1, LSR, 8), LeaveCC, ge);
  __ strb(scratch1, MemOperand(dest, 1, PostIndex), ge);
  __ mov(scratch1, Operand(scratch1, LSR, 8), LeaveCC, gt);
  __ strb(scratch1, MemOperand(dest, 1, PostIndex), gt);
  // Copy any remaining bytes.
  __ b(&byte_loop);

  // Simple loop.
  // Copy words from src to dst, until less than four bytes left.
  // Both src and dest are word aligned.
  __ bind(&simple_loop);
  {
    Label loop;
    __ bind(&loop);
    __ ldr(scratch1, MemOperand(src, 4, PostIndex));
    __ sub(scratch3, limit, Operand(dest));
    __ str(scratch1, MemOperand(dest, 4, PostIndex));
    // Compare to 8, not 4, because we do the substraction before increasing
    // dest.
    __ cmp(scratch3, Operand(8));
    __ b(ge, &loop);
  }

  // Copy bytes from src to dst until dst hits limit.
  __ bind(&byte_loop);
  __ cmp(dest, Operand(limit));
  __ ldrb(scratch1, MemOperand(src, 1, PostIndex), lt);
  __ b(ge, &done);
  __ strb(scratch1, MemOperand(dest, 1, PostIndex));
  __ b(&byte_loop);

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
  __ sub(scratch, c1, Operand(static_cast<int>('0')));
  __ cmp(scratch, Operand(static_cast<int>('9' - '0')));
  __ b(hi, &not_array_index);
  __ sub(scratch, c2, Operand(static_cast<int>('0')));
  __ cmp(scratch, Operand(static_cast<int>('9' - '0')));

  // If check failed combine both characters into single halfword.
  // This is required by the contract of the method: code at the
  // not_found branch expects this combination in c1 register
  __ orr(c1, c1, Operand(c2, LSL, kBitsPerByte), LeaveCC, ls);
  __ b(ls, not_found);

  __ bind(&not_array_index);
  // Calculate the two character string hash.
  Register hash = scratch1;
  StringHelper::GenerateHashInit(masm, hash, c1);
  StringHelper::GenerateHashAddCharacter(masm, hash, c2);
  StringHelper::GenerateHashGetHash(masm, hash);

  // Collect the two characters in a register.
  Register chars = c1;
  __ orr(chars, chars, Operand(c2, LSL, kBitsPerByte));

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load symbol table
  // Load address of first element of the symbol table.
  Register symbol_table = c2;
  __ LoadRoot(symbol_table, Heap::kSymbolTableRootIndex);

  // Load undefined value
  Register undefined = scratch4;
  __ LoadRoot(undefined, Heap::kUndefinedValueRootIndex);

  // Calculate capacity mask from the symbol table capacity.
  Register mask = scratch2;
  __ ldr(mask, FieldMemOperand(symbol_table, SymbolTable::kCapacityOffset));
  __ mov(mask, Operand(mask, ASR, 1));
  __ sub(mask, mask, Operand(1));

  // Calculate untagged address of the first element of the symbol table.
  Register first_symbol_table_element = symbol_table;
  __ add(first_symbol_table_element, symbol_table,
         Operand(SymbolTable::kElementsStartOffset - kHeapObjectTag));

  // Registers
  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string
  // mask:  capacity mask
  // first_symbol_table_element: address of the first element of
  //                             the symbol table
  // scratch: -

  // Perform a number of probes in the symbol table.
  static const int kProbes = 4;
  Label found_in_symbol_table;
  Label next_probe[kProbes];
  for (int i = 0; i < kProbes; i++) {
    Register candidate = scratch5;  // Scratch register contains candidate.

    // Calculate entry in symbol table.
    if (i > 0) {
      __ add(candidate, hash, Operand(SymbolTable::GetProbeOffset(i)));
    } else {
      __ mov(candidate, hash);
    }

    __ and_(candidate, candidate, Operand(mask));

    // Load the entry from the symble table.
    STATIC_ASSERT(SymbolTable::kEntrySize == 1);
    __ ldr(candidate,
           MemOperand(first_symbol_table_element,
                      candidate,
                      LSL,
                      kPointerSizeLog2));

    // If entry is undefined no string with this hash can be found.
    __ cmp(candidate, undefined);
    __ b(eq, not_found);

    // If length is not 2 the string is not a candidate.
    __ ldr(scratch, FieldMemOperand(candidate, String::kLengthOffset));
    __ cmp(scratch, Operand(Smi::FromInt(2)));
    __ b(ne, &next_probe[i]);

    // Check that the candidate is a non-external ascii string.
    __ ldr(scratch, FieldMemOperand(candidate, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(scratch, scratch,
                                              &next_probe[i]);

    // Check if the two characters match.
    // Assumes that word load is little endian.
    __ ldrh(scratch, FieldMemOperand(candidate, SeqAsciiString::kHeaderSize));
    __ cmp(chars, scratch);
    __ b(eq, &found_in_symbol_table);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = scratch;
  __ bind(&found_in_symbol_table);
  __ Move(r0, result);
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character) {
  // hash = character + (character << 10);
  __ add(hash, character, Operand(character, LSL, 10));
  // hash ^= hash >> 6;
  __ eor(hash, hash, Operand(hash, ASR, 6));
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character) {
  // hash += character;
  __ add(hash, hash, Operand(character));
  // hash += hash << 10;
  __ add(hash, hash, Operand(hash, LSL, 10));
  // hash ^= hash >> 6;
  __ eor(hash, hash, Operand(hash, ASR, 6));
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash) {
  // hash += hash << 3;
  __ add(hash, hash, Operand(hash, LSL, 3));
  // hash ^= hash >> 11;
  __ eor(hash, hash, Operand(hash, ASR, 11));
  // hash += hash << 15;
  __ add(hash, hash, Operand(hash, LSL, 15), SetCC);

  // if (hash == 0) hash = 27;
  __ mov(hash, Operand(27), LeaveCC, ne);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  lr: return address
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


  // Check bounds and smi-ness.
  Register to = r6;
  Register from = r7;
  __ Ldrd(to, from, MemOperand(sp, kToOffset));
  STATIC_ASSERT(kFromOffset == kToOffset + 4);
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize + kSmiShiftSize == 1);
  // I.e., arithmetic shift right by one un-smi-tags.
  __ mov(r2, Operand(to, ASR, 1), SetCC);
  __ mov(r3, Operand(from, ASR, 1), SetCC, cc);
  // If either to or from had the smi tag bit set, then carry is set now.
  __ b(cs, &runtime);  // Either "from" or "to" is not a smi.
  __ b(mi, &runtime);  // From is negative.

  // Both to and from are smis.

  __ sub(r2, r2, Operand(r3), SetCC);
  __ b(mi, &runtime);  // Fail if from > to.
  // Special handling of sub-strings of length 1 and 2. One character strings
  // are handled in the runtime system (looked up in the single character
  // cache). Two character strings are looked for in the symbol cache.
  __ cmp(r2, Operand(2));
  __ b(lt, &runtime);

  // r2: length
  // r3: from index (untaged smi)
  // r6 (a.k.a. to): to (smi)
  // r7 (a.k.a. from): from offset (smi)

  // Make sure first argument is a sequential (or flat) string.
  __ ldr(r5, MemOperand(sp, kStringOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(r5, Operand(kSmiTagMask));
  __ b(eq, &runtime);
  Condition is_string = masm->IsObjectStringType(r5, r1);
  __ b(NegateCondition(is_string), &runtime);

  // r1: instance type
  // r2: length
  // r3: from index (untagged smi)
  // r5: string
  // r6 (a.k.a. to): to (smi)
  // r7 (a.k.a. from): from offset (smi)
  Label seq_string;
  __ and_(r4, r1, Operand(kStringRepresentationMask));
  STATIC_ASSERT(kSeqStringTag < kConsStringTag);
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  __ cmp(r4, Operand(kConsStringTag));
  __ b(gt, &runtime);  // External strings go to runtime.
  __ b(lt, &seq_string);  // Sequential strings are handled directly.

  // Cons string. Try to recurse (once) on the first substring.
  // (This adds a little more generality than necessary to handle flattened
  // cons strings, but not much).
  __ ldr(r5, FieldMemOperand(r5, ConsString::kFirstOffset));
  __ ldr(r4, FieldMemOperand(r5, HeapObject::kMapOffset));
  __ ldrb(r1, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  __ tst(r1, Operand(kStringRepresentationMask));
  STATIC_ASSERT(kSeqStringTag == 0);
  __ b(ne, &runtime);  // Cons and External strings go to runtime.

  // Definitly a sequential string.
  __ bind(&seq_string);

  // r1: instance type.
  // r2: length
  // r3: from index (untaged smi)
  // r5: string
  // r6 (a.k.a. to): to (smi)
  // r7 (a.k.a. from): from offset (smi)
  __ ldr(r4, FieldMemOperand(r5, String::kLengthOffset));
  __ cmp(r4, Operand(to));
  __ b(lt, &runtime);  // Fail if to > length.
  to = no_reg;

  // r1: instance type.
  // r2: result string length.
  // r3: from index (untaged smi)
  // r5: string.
  // r7 (a.k.a. from): from offset (smi)
  // Check for flat ascii string.
  Label non_ascii_flat;
  __ tst(r1, Operand(kStringEncodingMask));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ b(eq, &non_ascii_flat);

  Label result_longer_than_two;
  __ cmp(r2, Operand(2));
  __ b(gt, &result_longer_than_two);

  // Sub string of length 2 requested.
  // Get the two characters forming the sub string.
  __ add(r5, r5, Operand(r3));
  __ ldrb(r3, FieldMemOperand(r5, SeqAsciiString::kHeaderSize));
  __ ldrb(r4, FieldMemOperand(r5, SeqAsciiString::kHeaderSize + 1));

  // Try to lookup two character string in symbol table.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, r3, r4, r1, r5, r6, r7, r9, &make_two_character_string);
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // r2: result string length.
  // r3: two characters combined into halfword in little endian byte order.
  __ bind(&make_two_character_string);
  __ AllocateAsciiString(r0, r2, r4, r5, r9, &runtime);
  __ strh(r3, FieldMemOperand(r0, SeqAsciiString::kHeaderSize));
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&result_longer_than_two);

  // Allocate the result.
  __ AllocateAsciiString(r0, r2, r3, r4, r1, &runtime);

  // r0: result string.
  // r2: result string length.
  // r5: string.
  // r7 (a.k.a. from): from offset (smi)
  // Locate first character of result.
  __ add(r1, r0, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Locate 'from' character of string.
  __ add(r5, r5, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ add(r5, r5, Operand(from, ASR, 1));

  // r0: result string.
  // r1: first character of result string.
  // r2: result string length.
  // r5: first character of sub string to copy.
  STATIC_ASSERT((SeqAsciiString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharactersLong(masm, r1, r5, r2, r3, r4, r6, r7, r9,
                                           COPY_ASCII | DEST_ALWAYS_ALIGNED);
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii_flat);
  // r2: result string length.
  // r5: string.
  // r7 (a.k.a. from): from offset (smi)
  // Check for flat two byte string.

  // Allocate the result.
  __ AllocateTwoByteString(r0, r2, r1, r3, r4, &runtime);

  // r0: result string.
  // r2: result string length.
  // r5: string.
  // Locate first character of result.
  __ add(r1, r0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Locate 'from' character of string.
  __ add(r5, r5, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // As "from" is a smi it is 2 times the value which matches the size of a two
  // byte character.
  __ add(r5, r5, Operand(from));
  from = no_reg;

  // r0: result string.
  // r1: first character of result.
  // r2: result length.
  // r5: first character of string to copy.
  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharactersLong(
      masm, r1, r5, r2, r3, r4, r6, r7, r9, DEST_ALWAYS_ALIGNED);
  __ IncrementCounter(&Counters::sub_string_native, 1, r3, r4);
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Ret();

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4) {
  Label compare_lengths;
  // Find minimum length and length difference.
  __ ldr(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ ldr(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ sub(scratch3, scratch1, Operand(scratch2), SetCC);
  Register length_delta = scratch3;
  __ mov(scratch1, scratch2, LeaveCC, gt);
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(min_length, Operand(min_length));
  __ b(eq, &compare_lengths);

  // Untag smi.
  __ mov(min_length, Operand(min_length, ASR, kSmiTagSize));

  // Setup registers so that we only need to increment one register
  // in the loop.
  __ add(scratch2, min_length,
         Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ add(left, left, Operand(scratch2));
  __ add(right, right, Operand(scratch2));
  // Registers left and right points to the min_length character of strings.
  __ rsb(min_length, min_length, Operand(-1));
  Register index = min_length;
  // Index starts at -min_length.

  {
    // Compare loop.
    Label loop;
    __ bind(&loop);
    // Compare characters.
    __ add(index, index, Operand(1), SetCC);
    __ ldrb(scratch2, MemOperand(left, index), ne);
    __ ldrb(scratch4, MemOperand(right, index), ne);
    // Skip to compare lengths with eq condition true.
    __ b(eq, &compare_lengths);
    __ cmp(scratch2, scratch4);
    __ b(eq, &loop);
    // Fallthrough with eq condition false.
  }
  // Compare lengths -  strings up to min-length are equal.
  __ bind(&compare_lengths);
  ASSERT(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use zero length_delta as result.
  __ mov(r0, Operand(length_delta), SetCC, eq);
  // Fall through to here if characters compare not-equal.
  __ mov(r0, Operand(Smi::FromInt(GREATER)), LeaveCC, gt);
  __ mov(r0, Operand(Smi::FromInt(LESS)), LeaveCC, lt);
  __ Ret();
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  sp[0]: right string
  //  sp[4]: left string
  __ Ldrd(r0 , r1, MemOperand(sp));  // Load right in r0, left in r1.

  Label not_same;
  __ cmp(r0, r1);
  __ b(ne, &not_same);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ mov(r0, Operand(Smi::FromInt(EQUAL)));
  __ IncrementCounter(&Counters::string_compare_native, 1, r1, r2);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&not_same);

  // Check that both objects are sequential ascii strings.
  __ JumpIfNotBothSequentialAsciiStrings(r1, r0, r2, r3, &runtime);

  // Compare flat ascii strings natively. Remove arguments from stack first.
  __ IncrementCounter(&Counters::string_compare_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  GenerateCompareFlatAsciiStrings(masm, r1, r0, r2, r3, r4, r5);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label string_add_runtime, call_builtin;
  Builtins::JavaScript builtin_id = Builtins::ADD;

  // Stack on entry:
  // sp[0]: second argument (right).
  // sp[4]: first argument (left).

  // Load the two arguments.
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));  // First argument.
  __ ldr(r1, MemOperand(sp, 0 * kPointerSize));  // Second argument.

  // Make sure that both arguments are strings if not known in advance.
  if (flags_ == NO_STRING_ADD_FLAGS) {
    __ JumpIfEitherSmi(r0, r1, &string_add_runtime);
    // Load instance types.
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
    STATIC_ASSERT(kStringTag == 0);
    // If either is not a string, go to runtime.
    __ tst(r4, Operand(kIsNotStringMask));
    __ tst(r5, Operand(kIsNotStringMask), eq);
    __ b(ne, &string_add_runtime);
  } else {
    // Here at least one of the arguments is definitely a string.
    // We convert the one that is not known to be a string.
    if ((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) != 0);
      GenerateConvertArgument(
          masm, 1 * kPointerSize, r0, r2, r3, r4, r5, &call_builtin);
      builtin_id = Builtins::STRING_ADD_RIGHT;
    } else if ((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) != 0);
      GenerateConvertArgument(
          masm, 0 * kPointerSize, r1, r2, r3, r4, r5, &call_builtin);
      builtin_id = Builtins::STRING_ADD_LEFT;
    }
  }

  // Both arguments are strings.
  // r0: first string
  // r1: second string
  // r4: first string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // r5: second string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  {
    Label strings_not_empty;
    // Check if either of the strings are empty. In that case return the other.
    __ ldr(r2, FieldMemOperand(r0, String::kLengthOffset));
    __ ldr(r3, FieldMemOperand(r1, String::kLengthOffset));
    STATIC_ASSERT(kSmiTag == 0);
    __ cmp(r2, Operand(Smi::FromInt(0)));  // Test if first string is empty.
    __ mov(r0, Operand(r1), LeaveCC, eq);  // If first is empty, return second.
    STATIC_ASSERT(kSmiTag == 0);
     // Else test if second string is empty.
    __ cmp(r3, Operand(Smi::FromInt(0)), ne);
    __ b(ne, &strings_not_empty);  // If either string was empty, return r0.

    __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
    __ add(sp, sp, Operand(2 * kPointerSize));
    __ Ret();

    __ bind(&strings_not_empty);
  }

  __ mov(r2, Operand(r2, ASR, kSmiTagSize));
  __ mov(r3, Operand(r3, ASR, kSmiTagSize));
  // Both strings are non-empty.
  // r0: first string
  // r1: second string
  // r2: length of first string
  // r3: length of second string
  // r4: first string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // r5: second string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // Look at the length of the result of adding the two strings.
  Label string_add_flat_result, longer_than_two;
  // Adding two lengths can't overflow.
  STATIC_ASSERT(String::kMaxLength < String::kMaxLength * 2);
  __ add(r6, r2, Operand(r3));
  // Use the runtime system when adding two one character strings, as it
  // contains optimizations for this specific case using the symbol table.
  __ cmp(r6, Operand(2));
  __ b(ne, &longer_than_two);

  // Check that both strings are non-external ascii strings.
  if (flags_ != NO_STRING_ADD_FLAGS) {
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
  }
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(r4, r5, r6, r7,
                                                  &string_add_runtime);

  // Get the two characters forming the sub string.
  __ ldrb(r2, FieldMemOperand(r0, SeqAsciiString::kHeaderSize));
  __ ldrb(r3, FieldMemOperand(r1, SeqAsciiString::kHeaderSize));

  // Try to lookup two character string in symbol table. If it is not found
  // just allocate a new one.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, r2, r3, r6, r7, r4, r5, r9, &make_two_character_string);
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&make_two_character_string);
  // Resulting string has length 2 and first chars of two strings
  // are combined into single halfword in r2 register.
  // So we can fill resulting string without two loops by a single
  // halfword store instruction (which assumes that processor is
  // in a little endian mode)
  __ mov(r6, Operand(2));
  __ AllocateAsciiString(r0, r6, r4, r5, r9, &string_add_runtime);
  __ strh(r2, FieldMemOperand(r0, SeqAsciiString::kHeaderSize));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ cmp(r6, Operand(String::kMinNonFlatLength));
  __ b(lt, &string_add_flat_result);
  // Handle exceptionally long strings in the runtime system.
  STATIC_ASSERT((String::kMaxLength & 0x80000000) == 0);
  ASSERT(IsPowerOf2(String::kMaxLength + 1));
  // kMaxLength + 1 is representable as shifted literal, kMaxLength is not.
  __ cmp(r6, Operand(String::kMaxLength + 1));
  __ b(hs, &string_add_runtime);

  // If result is not supposed to be flat, allocate a cons string object.
  // If both strings are ascii the result is an ascii cons string.
  if (flags_ != NO_STRING_ADD_FLAGS) {
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
  }
  Label non_ascii, allocated, ascii_data;
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ tst(r4, Operand(kStringEncodingMask));
  __ tst(r5, Operand(kStringEncodingMask), ne);
  __ b(eq, &non_ascii);

  // Allocate an ASCII cons string.
  __ bind(&ascii_data);
  __ AllocateAsciiConsString(r7, r6, r4, r5, &string_add_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ str(r0, FieldMemOperand(r7, ConsString::kFirstOffset));
  __ str(r1, FieldMemOperand(r7, ConsString::kSecondOffset));
  __ mov(r0, Operand(r7));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only ascii characters.
  // r4: first instance type.
  // r5: second instance type.
  __ tst(r4, Operand(kAsciiDataHintMask));
  __ tst(r5, Operand(kAsciiDataHintMask), ne);
  __ b(ne, &ascii_data);
  __ eor(r4, r4, Operand(r5));
  STATIC_ASSERT(kAsciiStringTag != 0 && kAsciiDataHintTag != 0);
  __ and_(r4, r4, Operand(kAsciiStringTag | kAsciiDataHintTag));
  __ cmp(r4, Operand(kAsciiStringTag | kAsciiDataHintTag));
  __ b(eq, &ascii_data);

  // Allocate a two byte cons string.
  __ AllocateTwoByteConsString(r7, r6, r4, r5, &string_add_runtime);
  __ jmp(&allocated);

  // Handle creating a flat result. First check that both strings are
  // sequential and that they have the same encoding.
  // r0: first string
  // r1: second string
  // r2: length of first string
  // r3: length of second string
  // r4: first string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // r5: second string instance type (if flags_ == NO_STRING_ADD_FLAGS)
  // r6: sum of lengths.
  __ bind(&string_add_flat_result);
  if (flags_ != NO_STRING_ADD_FLAGS) {
    __ ldr(r4, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ ldr(r5, FieldMemOperand(r1, HeapObject::kMapOffset));
    __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
    __ ldrb(r5, FieldMemOperand(r5, Map::kInstanceTypeOffset));
  }
  // Check that both strings are sequential.
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(r4, Operand(kStringRepresentationMask));
  __ tst(r5, Operand(kStringRepresentationMask), eq);
  __ b(ne, &string_add_runtime);
  // Now check if both strings have the same encoding (ASCII/Two-byte).
  // r0: first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: sum of lengths..
  Label non_ascii_string_add_flat_result;
  ASSERT(IsPowerOf2(kStringEncodingMask));  // Just one bit to test.
  __ eor(r7, r4, Operand(r5));
  __ tst(r7, Operand(kStringEncodingMask));
  __ b(ne, &string_add_runtime);
  // And see if it's ASCII or two-byte.
  __ tst(r4, Operand(kStringEncodingMask));
  __ b(eq, &non_ascii_string_add_flat_result);

  // Both strings are sequential ASCII strings. We also know that they are
  // short (since the sum of the lengths is less than kMinNonFlatLength).
  // r6: length of resulting flat string
  __ AllocateAsciiString(r7, r6, r4, r5, r9, &string_add_runtime);
  // Locate first character of result.
  __ add(r6, r7, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ add(r0, r0, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // r0: first character of first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: first character of result.
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r0, r2, r4, true);

  // Load second argument and locate first character.
  __ add(r1, r1, Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  // r1: first character of second string.
  // r3: length of second string.
  // r6: next character of result.
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r1, r3, r4, true);
  __ mov(r0, Operand(r7));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  __ bind(&non_ascii_string_add_flat_result);
  // Both strings are sequential two byte strings.
  // r0: first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: sum of length of strings.
  __ AllocateTwoByteString(r7, r6, r4, r5, r9, &string_add_runtime);
  // r0: first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r7: result string.

  // Locate first character of result.
  __ add(r6, r7, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  // Locate first character of first argument.
  __ add(r0, r0, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // r0: first character of first string.
  // r1: second string.
  // r2: length of first string.
  // r3: length of second string.
  // r6: first character of result.
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r0, r2, r4, false);

  // Locate first character of second argument.
  __ add(r1, r1, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // r1: first character of second string.
  // r3: length of second string.
  // r6: next character of result (after copy of first string).
  // r7: result string.
  StringHelper::GenerateCopyCharacters(masm, r6, r1, r3, r4, false);

  __ mov(r0, Operand(r7));
  __ IncrementCounter(&Counters::string_add_native, 1, r2, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  // Just jump to runtime to add the two strings.
  __ bind(&string_add_runtime);
  __ TailCallRuntime(Runtime::kStringAdd, 2, 1);

  if (call_builtin.is_linked()) {
    __ bind(&call_builtin);
    __ InvokeBuiltin(builtin_id, JUMP_JS);
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
  __ CompareObjectType(arg, scratch1, scratch1, FIRST_NONSTRING_TYPE);
  __ b(lt, &done);

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
  __ str(arg, MemOperand(sp, stack_offset));
  __ jmp(&done);

  // Check if the argument is a safe string wrapper.
  __ bind(&not_cached);
  __ JumpIfSmi(arg, slow);
  __ CompareObjectType(
      arg, scratch1, scratch2, JS_VALUE_TYPE);  // map -> scratch1.
  __ b(ne, slow);
  __ ldrb(scratch2, FieldMemOperand(scratch1, Map::kBitField2Offset));
  __ and_(scratch2,
          scratch2, Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ cmp(scratch2,
         Operand(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ b(ne, slow);
  __ ldr(arg, FieldMemOperand(arg, JSValue::kValueOffset));
  __ str(arg, MemOperand(sp, stack_offset));

  __ bind(&done);
}


void StringCharAtStub::Generate(MacroAssembler* masm) {
  // Expects two arguments (object, index) on the stack:
  //  lr: return address
  //  sp[0]: index
  //  sp[4]: object
  Register object = r1;
  Register index = r0;
  Register scratch1 = r2;
  Register scratch2 = r3;
  Register result = r0;

  // Get object and index from the stack.
  __ pop(index);
  __ pop(object);

  Label need_conversion;
  Label index_out_of_range;
  Label done;
  StringCharAtGenerator generator(object,
                                  index,
                                  scratch1,
                                  scratch2,
                                  result,
                                  &need_conversion,
                                  &need_conversion,
                                  &index_out_of_range,
                                  STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm);
  __ b(&done);

  __ bind(&index_out_of_range);
  // When the index is out of range, the spec requires us to return
  // the empty string.
  __ LoadRoot(result, Heap::kEmptyStringRootIndex);
  __ jmp(&done);

  __ bind(&need_conversion);
  // Move smi zero into the result register, which will trigger
  // conversion.
  __ mov(result, Operand(Smi::FromInt(0)));
  __ b(&done);

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm, call_helper);

  __ bind(&done);
  __ Ret();
}


void ICCompareStub::GenerateSmis(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SMIS);
  Label miss;
  __ orr(r2, r1, r0);
  __ tst(r2, Operand(kSmiTagMask));
  __ b(ne, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    __ sub(r0, r0, r1, SetCC);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(r1);
    __ sub(r0, r1, SmiUntagOperand(r0));
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
  __ and_(r2, r1, Operand(r0));
  __ tst(r2, Operand(kSmiTagMask));
  __ b(eq, &generic_stub);

  __ CompareObjectType(r0, r2, r2, HEAP_NUMBER_TYPE);
  __ b(ne, &miss);
  __ CompareObjectType(r1, r2, r2, HEAP_NUMBER_TYPE);
  __ b(ne, &miss);

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved or VFP3 is unsupported.
  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);

    // Load left and right operand
    __ sub(r2, r1, Operand(kHeapObjectTag));
    __ vldr(d0, r2, HeapNumber::kValueOffset);
    __ sub(r2, r0, Operand(kHeapObjectTag));
    __ vldr(d1, r2, HeapNumber::kValueOffset);

    // Compare operands
    __ VFPCompareAndSetFlags(d0, d1);

    // Don't base result on status bits when a NaN is involved.
    __ b(vs, &unordered);

    // Return a result of -1, 0, or 1, based on status bits.
    __ mov(r0, Operand(EQUAL), LeaveCC, eq);
    __ mov(r0, Operand(LESS), LeaveCC, lt);
    __ mov(r0, Operand(GREATER), LeaveCC, gt);
    __ Ret();

    __ bind(&unordered);
  }

  CompareStub stub(GetCondition(), strict(), NO_COMPARE_FLAGS, r1, r0);
  __ bind(&generic_stub);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateObjects(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::OBJECTS);
  Label miss;
  __ and_(r2, r1, Operand(r0));
  __ tst(r2, Operand(kSmiTagMask));
  __ b(eq, &miss);

  __ CompareObjectType(r0, r2, r2, JS_OBJECT_TYPE);
  __ b(ne, &miss);
  __ CompareObjectType(r1, r2, r2, JS_OBJECT_TYPE);
  __ b(ne, &miss);

  ASSERT(GetCondition() == eq);
  __ sub(r0, r0, Operand(r1));
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateMiss(MacroAssembler* masm) {
  __ Push(r1, r0);
  __ push(lr);

  // Call the runtime system in a fresh internal frame.
  ExternalReference miss = ExternalReference(IC_Utility(IC::kCompareIC_Miss));
  __ EnterInternalFrame();
  __ Push(r1, r0);
  __ mov(ip, Operand(Smi::FromInt(op_)));
  __ push(ip);
  __ CallExternalReference(miss, 3);
  __ LeaveInternalFrame();
  // Compute the entry point of the rewritten stub.
  __ add(r2, r0, Operand(Code::kHeaderSize - kHeapObjectTag));
  // Restore registers.
  __ pop(lr);
  __ pop(r0);
  __ pop(r1);
  __ Jump(r2);
}


void DirectCEntryStub::Generate(MacroAssembler* masm) {
  __ ldr(pc, MemOperand(sp, 0));
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    ExternalReference function) {
  __ mov(lr, Operand(reinterpret_cast<intptr_t>(GetCode().location()),
                     RelocInfo::CODE_TARGET));
  __ mov(r2, Operand(function));
  // Push return address (accessible to GC through exit frame pc).
  __ str(pc, MemOperand(sp, 0));
  __ Jump(r2);  // Call the api function.
}


void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    Register target) {
  __ mov(lr, Operand(reinterpret_cast<intptr_t>(GetCode().location()),
                     RelocInfo::CODE_TARGET));
  // Push return address (accessible to GC through exit frame pc).
  __ str(pc, MemOperand(sp, 0));
  __ Jump(target);  // Call the C++ function.
}


void GenerateFastPixelArrayLoad(MacroAssembler* masm,
                                Register receiver,
                                Register key,
                                Register elements_map,
                                Register elements,
                                Register scratch1,
                                Register scratch2,
                                Register result,
                                Label* not_pixel_array,
                                Label* key_not_smi,
                                Label* out_of_range) {
  // Register use:
  //
  // receiver - holds the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // elements - set to be the receiver's elements on exit.
  //
  // elements_map - set to be the map of the receiver's elements
  //            on exit.
  //
  // result   - holds the result of the pixel array load on exit,
  //            tagged as a smi if successful.
  //
  // Scratch registers:
  //
  // scratch1 - used a scratch register in map check, if map
  //            check is successful, contains the length of the
  //            pixel array, the pointer to external elements and
  //            the untagged result.
  //
  // scratch2 - holds the untaged key.

  // Some callers already have verified that the key is a smi.  key_not_smi is
  // set to NULL as a sentinel for that case.  Otherwise, add an explicit check
  // to ensure the key is a smi must be added.
  if (key_not_smi != NULL) {
    __ JumpIfNotSmi(key, key_not_smi);
  } else {
    if (FLAG_debug_code) {
      __ AbortIfNotSmi(key);
    }
  }
  __ SmiUntag(scratch2, key);

  // Verify that the receiver has pixel array elements.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ CheckMap(elements, scratch1, Heap::kPixelArrayMapRootIndex,
              not_pixel_array, true);

  // Key must be in range of the pixel array.
  __ ldr(scratch1, FieldMemOperand(elements, PixelArray::kLengthOffset));
  __ cmp(scratch2, scratch1);
  __ b(hs, out_of_range);  // unsigned check handles negative keys.

  // Perform the indexed load and tag the result as a smi.
  __ ldr(scratch1,
         FieldMemOperand(elements, PixelArray::kExternalPointerOffset));
  __ ldrb(scratch1, MemOperand(scratch1, scratch2));
  __ SmiTag(r0, scratch1);
  __ Ret();
}


void GenerateFastPixelArrayStore(MacroAssembler* masm,
                                 Register receiver,
                                 Register key,
                                 Register value,
                                 Register elements,
                                 Register elements_map,
                                 Register scratch1,
                                 Register scratch2,
                                 bool load_elements_from_receiver,
                                 bool load_elements_map_from_elements,
                                 Label* key_not_smi,
                                 Label* value_not_smi,
                                 Label* not_pixel_array,
                                 Label* out_of_range) {
  // Register use:
  //   receiver - holds the receiver and is unchanged unless the
  //              store succeeds.
  //   key - holds the key (must be a smi) and is unchanged.
  //   value - holds the value (must be a smi) and is unchanged.
  //   elements - holds the element object of the receiver on entry if
  //              load_elements_from_receiver is false, otherwise used
  //              internally to store the pixel arrays elements and
  //              external array pointer.
  //   elements_map - holds the map of the element object if
  //              load_elements_map_from_elements is false, otherwise
  //              loaded with the element map.
  //
  Register external_pointer = elements;
  Register untagged_key = scratch1;
  Register untagged_value = scratch2;

  if (load_elements_from_receiver) {
    __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  }

  // By passing NULL as not_pixel_array, callers signal that they have already
  // verified that the receiver has pixel array elements.
  if (not_pixel_array != NULL) {
    if (load_elements_map_from_elements) {
      __ ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
    }
    __ LoadRoot(ip, Heap::kPixelArrayMapRootIndex);
    __ cmp(elements_map, ip);
    __ b(ne, not_pixel_array);
  } else {
    if (FLAG_debug_code) {
      // Map check should have already made sure that elements is a pixel array.
      __ ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
      __ LoadRoot(ip, Heap::kPixelArrayMapRootIndex);
      __ cmp(elements_map, ip);
      __ Assert(eq, "Elements isn't a pixel array");
    }
  }

  // Some callers already have verified that the key is a smi.  key_not_smi is
  // set to NULL as a sentinel for that case.  Otherwise, add an explicit check
  // to ensure the key is a smi must be added.
  if (key_not_smi != NULL) {
    __ JumpIfNotSmi(key, key_not_smi);
  } else {
    if (FLAG_debug_code) {
      __ AbortIfNotSmi(key);
    }
  }

  __ SmiUntag(untagged_key, key);

  // Perform bounds check.
  __ ldr(scratch2, FieldMemOperand(elements, PixelArray::kLengthOffset));
  __ cmp(untagged_key, scratch2);
  __ b(hs, out_of_range);  // unsigned check handles negative keys.

  __ JumpIfNotSmi(value, value_not_smi);
  __ SmiUntag(untagged_value, value);

  // Clamp the value to [0..255].
  __ Usat(untagged_value, 8, Operand(untagged_value));
  // Get the pointer to the external array. This clobbers elements.
  __ ldr(external_pointer,
         FieldMemOperand(elements, PixelArray::kExternalPointerOffset));
  __ strb(untagged_value, MemOperand(external_pointer, untagged_key));
  __ Ret();
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

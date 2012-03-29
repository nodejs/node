// Copyright 2012 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_X64)

#include "bootstrapper.h"
#include "code-stubs.h"
#include "regexp-macro-assembler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void ToNumberStub::Generate(MacroAssembler* masm) {
  // The ToNumber stub takes one argument in eax.
  Label check_heap_number, call_builtin;
  __ SmiTest(rax);
  __ j(not_zero, &check_heap_number, Label::kNear);
  __ Ret();

  __ bind(&check_heap_number);
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &call_builtin, Label::kNear);
  __ Ret();

  __ bind(&call_builtin);
  __ pop(rcx);  // Pop return address.
  __ push(rax);
  __ push(rcx);  // Push return address.
  __ InvokeBuiltin(Builtins::TO_NUMBER, JUMP_FUNCTION);
}


void FastNewClosureStub::Generate(MacroAssembler* masm) {
  // Create a new closure from the given function info in new
  // space. Set the context to the current context in rsi.
  Label gc;
  __ AllocateInNewSpace(JSFunction::kSize, rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function info from the stack.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));

  int map_index = (language_mode_ == CLASSIC_MODE)
      ? Context::FUNCTION_MAP_INDEX
      : Context::STRICT_MODE_FUNCTION_MAP_INDEX;

  // Compute the function map in the current global context and set that
  // as the map of the allocated object.
  __ movq(rcx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rcx, FieldOperand(rcx, GlobalObject::kGlobalContextOffset));
  __ movq(rcx, Operand(rcx, Context::SlotOffset(map_index)));
  __ movq(FieldOperand(rax, JSObject::kMapOffset), rcx);

  // Initialize the rest of the function. We don't have to update the
  // write barrier because the allocated object is in new space.
  __ LoadRoot(rbx, Heap::kEmptyFixedArrayRootIndex);
  __ LoadRoot(rcx, Heap::kTheHoleValueRootIndex);
  __ LoadRoot(rdi, Heap::kUndefinedValueRootIndex);
  __ movq(FieldOperand(rax, JSObject::kPropertiesOffset), rbx);
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rbx);
  __ movq(FieldOperand(rax, JSFunction::kPrototypeOrInitialMapOffset), rcx);
  __ movq(FieldOperand(rax, JSFunction::kSharedFunctionInfoOffset), rdx);
  __ movq(FieldOperand(rax, JSFunction::kContextOffset), rsi);
  __ movq(FieldOperand(rax, JSFunction::kLiteralsOffset), rbx);
  __ movq(FieldOperand(rax, JSFunction::kNextFunctionLinkOffset), rdi);

  // Initialize the code pointer in the function to be the one
  // found in the shared function info object.
  __ movq(rdx, FieldOperand(rdx, SharedFunctionInfo::kCodeOffset));
  __ lea(rdx, FieldOperand(rdx, Code::kHeaderSize));
  __ movq(FieldOperand(rax, JSFunction::kCodeEntryOffset), rdx);


  // Return and remove the on-stack parameter.
  __ ret(1 * kPointerSize);

  // Create a new closure through the slower runtime call.
  __ bind(&gc);
  __ pop(rcx);  // Temporarily remove return address.
  __ pop(rdx);
  __ push(rsi);
  __ push(rdx);
  __ PushRoot(Heap::kFalseValueRootIndex);
  __ push(rcx);  // Restore return address.
  __ TailCallRuntime(Runtime::kNewClosure, 3, 1);
}


void FastNewContextStub::Generate(MacroAssembler* masm) {
  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ AllocateInNewSpace((length * kPointerSize) + FixedArray::kHeaderSize,
                        rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));

  // Set up the object header.
  __ LoadRoot(kScratchRegister, Heap::kFunctionContextMapRootIndex);
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), kScratchRegister);
  __ Move(FieldOperand(rax, FixedArray::kLengthOffset), Smi::FromInt(length));

  // Set up the fixed slots.
  __ Set(rbx, 0);  // Set to NULL.
  __ movq(Operand(rax, Context::SlotOffset(Context::CLOSURE_INDEX)), rcx);
  __ movq(Operand(rax, Context::SlotOffset(Context::PREVIOUS_INDEX)), rsi);
  __ movq(Operand(rax, Context::SlotOffset(Context::EXTENSION_INDEX)), rbx);

  // Copy the global object from the previous context.
  __ movq(rbx, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(Operand(rax, Context::SlotOffset(Context::GLOBAL_INDEX)), rbx);

  // Initialize the rest of the slots to undefined.
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; i++) {
    __ movq(Operand(rax, Context::SlotOffset(i)), rbx);
  }

  // Return and remove the on-stack parameter.
  __ movq(rsi, rax);
  __ ret(1 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kNewFunctionContext, 1, 1);
}


void FastNewBlockContextStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [rsp + (1 * kPointerSize)]: function
  // [rsp + (2 * kPointerSize)]: serialized scope info

  // Try to allocate the context in new space.
  Label gc;
  int length = slots_ + Context::MIN_CONTEXT_SLOTS;
  __ AllocateInNewSpace(FixedArray::SizeFor(length),
                        rax, rbx, rcx, &gc, TAG_OBJECT);

  // Get the function from the stack.
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));

  // Get the serialized scope info from the stack.
  __ movq(rbx, Operand(rsp, 2 * kPointerSize));

  // Set up the object header.
  __ LoadRoot(kScratchRegister, Heap::kBlockContextMapRootIndex);
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), kScratchRegister);
  __ Move(FieldOperand(rax, FixedArray::kLengthOffset), Smi::FromInt(length));

  // If this block context is nested in the global context we get a smi
  // sentinel instead of a function. The block context should get the
  // canonical empty function of the global context as its closure which
  // we still have to look up.
  Label after_sentinel;
  __ JumpIfNotSmi(rcx, &after_sentinel, Label::kNear);
  if (FLAG_debug_code) {
    const char* message = "Expected 0 as a Smi sentinel";
    __ cmpq(rcx, Immediate(0));
    __ Assert(equal, message);
  }
  __ movq(rcx, GlobalObjectOperand());
  __ movq(rcx, FieldOperand(rcx, GlobalObject::kGlobalContextOffset));
  __ movq(rcx, ContextOperand(rcx, Context::CLOSURE_INDEX));
  __ bind(&after_sentinel);

  // Set up the fixed slots.
  __ movq(ContextOperand(rax, Context::CLOSURE_INDEX), rcx);
  __ movq(ContextOperand(rax, Context::PREVIOUS_INDEX), rsi);
  __ movq(ContextOperand(rax, Context::EXTENSION_INDEX), rbx);

  // Copy the global object from the previous context.
  __ movq(rbx, ContextOperand(rsi, Context::GLOBAL_INDEX));
  __ movq(ContextOperand(rax, Context::GLOBAL_INDEX), rbx);

  // Initialize the rest of the slots to the hole value.
  __ LoadRoot(rbx, Heap::kTheHoleValueRootIndex);
  for (int i = 0; i < slots_; i++) {
    __ movq(ContextOperand(rax, i + Context::MIN_CONTEXT_SLOTS), rbx);
  }

  // Return and remove the on-stack parameter.
  __ movq(rsi, rax);
  __ ret(2 * kPointerSize);

  // Need to collect. Call into runtime system.
  __ bind(&gc);
  __ TailCallRuntime(Runtime::kPushBlockContext, 2, 1);
}


static void GenerateFastCloneShallowArrayCommon(
    MacroAssembler* masm,
    int length,
    FastCloneShallowArrayStub::Mode mode,
    Label* fail) {
  // Registers on entry:
  //
  // rcx: boilerplate literal array.
  ASSERT(mode != FastCloneShallowArrayStub::CLONE_ANY_ELEMENTS);

  // All sizes here are multiples of kPointerSize.
  int elements_size = 0;
  if (length > 0) {
    elements_size = mode == FastCloneShallowArrayStub::CLONE_DOUBLE_ELEMENTS
        ? FixedDoubleArray::SizeFor(length)
        : FixedArray::SizeFor(length);
  }
  int size = JSArray::kSize + elements_size;

  // Allocate both the JS array and the elements array in one big
  // allocation. This avoids multiple limit checks.
  __ AllocateInNewSpace(size, rax, rbx, rdx, fail, TAG_OBJECT);

  // Copy the JS array part.
  for (int i = 0; i < JSArray::kSize; i += kPointerSize) {
    if ((i != JSArray::kElementsOffset) || (length == 0)) {
      __ movq(rbx, FieldOperand(rcx, i));
      __ movq(FieldOperand(rax, i), rbx);
    }
  }

  if (length > 0) {
    // Get hold of the elements array of the boilerplate and setup the
    // elements pointer in the resulting object.
    __ movq(rcx, FieldOperand(rcx, JSArray::kElementsOffset));
    __ lea(rdx, Operand(rax, JSArray::kSize));
    __ movq(FieldOperand(rax, JSArray::kElementsOffset), rdx);

    // Copy the elements array.
    if (mode == FastCloneShallowArrayStub::CLONE_ELEMENTS) {
      for (int i = 0; i < elements_size; i += kPointerSize) {
        __ movq(rbx, FieldOperand(rcx, i));
        __ movq(FieldOperand(rdx, i), rbx);
      }
    } else {
      ASSERT(mode == FastCloneShallowArrayStub::CLONE_DOUBLE_ELEMENTS);
      int i;
      for (i = 0; i < FixedDoubleArray::kHeaderSize; i += kPointerSize) {
        __ movq(rbx, FieldOperand(rcx, i));
        __ movq(FieldOperand(rdx, i), rbx);
      }
      while (i < elements_size) {
        __ movsd(xmm0, FieldOperand(rcx, i));
        __ movsd(FieldOperand(rdx, i), xmm0);
        i += kDoubleSize;
      }
      ASSERT(i == elements_size);
    }
  }
}

void FastCloneShallowArrayStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [rsp + kPointerSize]: constant elements.
  // [rsp + (2 * kPointerSize)]: literal index.
  // [rsp + (3 * kPointerSize)]: literals array.

  // Load boilerplate object into rcx and check if we need to create a
  // boilerplate.
  __ movq(rcx, Operand(rsp, 3 * kPointerSize));
  __ movq(rax, Operand(rsp, 2 * kPointerSize));
  SmiIndex index = masm->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ movq(rcx,
          FieldOperand(rcx, index.reg, index.scale, FixedArray::kHeaderSize));
  __ CompareRoot(rcx, Heap::kUndefinedValueRootIndex);
  Label slow_case;
  __ j(equal, &slow_case);

  FastCloneShallowArrayStub::Mode mode = mode_;
  // rcx is boilerplate object.
  Factory* factory = masm->isolate()->factory();
  if (mode == CLONE_ANY_ELEMENTS) {
    Label double_elements, check_fast_elements;
    __ movq(rbx, FieldOperand(rcx, JSArray::kElementsOffset));
    __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
           factory->fixed_cow_array_map());
    __ j(not_equal, &check_fast_elements);
    GenerateFastCloneShallowArrayCommon(masm, 0,
                                        COPY_ON_WRITE_ELEMENTS, &slow_case);
    __ ret(3 * kPointerSize);

    __ bind(&check_fast_elements);
    __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
           factory->fixed_array_map());
    __ j(not_equal, &double_elements);
    GenerateFastCloneShallowArrayCommon(masm, length_,
                                        CLONE_ELEMENTS, &slow_case);
    __ ret(3 * kPointerSize);

    __ bind(&double_elements);
    mode = CLONE_DOUBLE_ELEMENTS;
    // Fall through to generate the code to handle double elements.
  }

  if (FLAG_debug_code) {
    const char* message;
    Heap::RootListIndex expected_map_index;
    if (mode == CLONE_ELEMENTS) {
      message = "Expected (writable) fixed array";
      expected_map_index = Heap::kFixedArrayMapRootIndex;
    } else if (mode == CLONE_DOUBLE_ELEMENTS) {
      message = "Expected (writable) fixed double array";
      expected_map_index = Heap::kFixedDoubleArrayMapRootIndex;
    } else {
      ASSERT(mode == COPY_ON_WRITE_ELEMENTS);
      message = "Expected copy-on-write fixed array";
      expected_map_index = Heap::kFixedCOWArrayMapRootIndex;
    }
    __ push(rcx);
    __ movq(rcx, FieldOperand(rcx, JSArray::kElementsOffset));
    __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                   expected_map_index);
    __ Assert(equal, message);
    __ pop(rcx);
  }

  GenerateFastCloneShallowArrayCommon(masm, length_, mode, &slow_case);
  __ ret(3 * kPointerSize);

  __ bind(&slow_case);
  __ TailCallRuntime(Runtime::kCreateArrayLiteralShallow, 3, 1);
}


void FastCloneShallowObjectStub::Generate(MacroAssembler* masm) {
  // Stack layout on entry:
  //
  // [rsp + kPointerSize]: object literal flags.
  // [rsp + (2 * kPointerSize)]: constant properties.
  // [rsp + (3 * kPointerSize)]: literal index.
  // [rsp + (4 * kPointerSize)]: literals array.

  // Load boilerplate object into ecx and check if we need to create a
  // boilerplate.
  Label slow_case;
  __ movq(rcx, Operand(rsp, 4 * kPointerSize));
  __ movq(rax, Operand(rsp, 3 * kPointerSize));
  SmiIndex index = masm->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ movq(rcx,
          FieldOperand(rcx, index.reg, index.scale, FixedArray::kHeaderSize));
  __ CompareRoot(rcx, Heap::kUndefinedValueRootIndex);
  __ j(equal, &slow_case);

  // Check that the boilerplate contains only fast properties and we can
  // statically determine the instance size.
  int size = JSObject::kHeaderSize + length_ * kPointerSize;
  __ movq(rax, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movzxbq(rax, FieldOperand(rax, Map::kInstanceSizeOffset));
  __ cmpq(rax, Immediate(size >> kPointerSizeLog2));
  __ j(not_equal, &slow_case);

  // Allocate the JS object and copy header together with all in-object
  // properties from the boilerplate.
  __ AllocateInNewSpace(size, rax, rbx, rdx, &slow_case, TAG_OBJECT);
  for (int i = 0; i < size; i += kPointerSize) {
    __ movq(rbx, FieldOperand(rcx, i));
    __ movq(FieldOperand(rax, i), rbx);
  }

  // Return and remove the on-stack parameters.
  __ ret(4 * kPointerSize);

  __ bind(&slow_case);
  __ TailCallRuntime(Runtime::kCreateObjectLiteralShallow, 4, 1);
}


// The stub expects its argument on the stack and returns its result in tos_:
// zero for false, and a non-zero value for true.
void ToBooleanStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  Label patch;
  const Register argument = rax;
  const Register map = rdx;

  if (!types_.IsEmpty()) {
    __ movq(argument, Operand(rsp, 1 * kPointerSize));
  }

  // undefined -> false
  CheckOddball(masm, UNDEFINED, Heap::kUndefinedValueRootIndex, false);

  // Boolean -> its value
  CheckOddball(masm, BOOLEAN, Heap::kFalseValueRootIndex, false);
  CheckOddball(masm, BOOLEAN, Heap::kTrueValueRootIndex, true);

  // 'null' -> false.
  CheckOddball(masm, NULL_TYPE, Heap::kNullValueRootIndex, false);

  if (types_.Contains(SMI)) {
    // Smis: 0 -> false, all other -> true
    Label not_smi;
    __ JumpIfNotSmi(argument, &not_smi, Label::kNear);
    // argument contains the correct return value already
    if (!tos_.is(argument)) {
      __ movq(tos_, argument);
    }
    __ ret(1 * kPointerSize);
    __ bind(&not_smi);
  } else if (types_.NeedsMap()) {
    // If we need a map later and have a Smi -> patch.
    __ JumpIfSmi(argument, &patch, Label::kNear);
  }

  if (types_.NeedsMap()) {
    __ movq(map, FieldOperand(argument, HeapObject::kMapOffset));

    if (types_.CanBeUndetectable()) {
      __ testb(FieldOperand(map, Map::kBitFieldOffset),
               Immediate(1 << Map::kIsUndetectable));
      // Undetectable -> false.
      Label not_undetectable;
      __ j(zero, &not_undetectable, Label::kNear);
      __ Set(tos_, 0);
      __ ret(1 * kPointerSize);
      __ bind(&not_undetectable);
    }
  }

  if (types_.Contains(SPEC_OBJECT)) {
    // spec object -> true.
    Label not_js_object;
    __ CmpInstanceType(map, FIRST_SPEC_OBJECT_TYPE);
    __ j(below, &not_js_object, Label::kNear);
    // argument contains the correct return value already.
    if (!tos_.is(argument)) {
      __ Set(tos_, 1);
    }
    __ ret(1 * kPointerSize);
    __ bind(&not_js_object);
  }

  if (types_.Contains(STRING)) {
    // String value -> false iff empty.
    Label not_string;
    __ CmpInstanceType(map, FIRST_NONSTRING_TYPE);
    __ j(above_equal, &not_string, Label::kNear);
    __ movq(tos_, FieldOperand(argument, String::kLengthOffset));
    __ ret(1 * kPointerSize);  // the string length is OK as the return value
    __ bind(&not_string);
  }

  if (types_.Contains(HEAP_NUMBER)) {
    // heap number -> false iff +0, -0, or NaN.
    Label not_heap_number, false_result;
    __ CompareRoot(map, Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &not_heap_number, Label::kNear);
    __ xorps(xmm0, xmm0);
    __ ucomisd(xmm0, FieldOperand(argument, HeapNumber::kValueOffset));
    __ j(zero, &false_result, Label::kNear);
    // argument contains the correct return value already.
    if (!tos_.is(argument)) {
      __ Set(tos_, 1);
    }
    __ ret(1 * kPointerSize);
    __ bind(&false_result);
    __ Set(tos_, 0);
    __ ret(1 * kPointerSize);
    __ bind(&not_heap_number);
  }

  __ bind(&patch);
  GenerateTypeTransition(masm);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  __ PushCallerSaved(save_doubles_);
  const int argument_count = 1;
  __ PrepareCallCFunction(argument_count);
#ifdef _WIN64
  __ LoadAddress(rcx, ExternalReference::isolate_address());
#else
  __ LoadAddress(rdi, ExternalReference::isolate_address());
#endif

  AllowExternalCallThatCantCauseGC scope(masm);
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(masm->isolate()),
      argument_count);
  __ PopCallerSaved(save_doubles_);
  __ ret(0);
}


void ToBooleanStub::CheckOddball(MacroAssembler* masm,
                                 Type type,
                                 Heap::RootListIndex value,
                                 bool result) {
  const Register argument = rax;
  if (types_.Contains(type)) {
    // If we see an expected oddball, return its ToBoolean value tos_.
    Label different_value;
    __ CompareRoot(argument, value);
    __ j(not_equal, &different_value, Label::kNear);
    if (!result) {
      // If we have to return zero, there is no way around clearing tos_.
      __ Set(tos_, 0);
    } else if (!tos_.is(argument)) {
      // If we have to return non-zero, we can re-use the argument if it is the
      // same register as the result, because we never see Smi-zero here.
      __ Set(tos_, 1);
    }
    __ ret(1 * kPointerSize);
    __ bind(&different_value);
  }
}


void ToBooleanStub::GenerateTypeTransition(MacroAssembler* masm) {
  __ pop(rcx);  // Get return address, operand is now on top of stack.
  __ Push(Smi::FromInt(tos_.code()));
  __ Push(Smi::FromInt(types_.ToByte()));
  __ push(rcx);  // Push return address.
  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kToBoolean_Patch), masm->isolate()),
      3,
      1);
}


class FloatingPointHelper : public AllStatic {
 public:
  // Load the operands from rdx and rax into xmm0 and xmm1, as doubles.
  // If the operands are not both numbers, jump to not_numbers.
  // Leaves rdx and rax unchanged.  SmiOperands assumes both are smis.
  // NumberOperands assumes both are smis or heap numbers.
  static void LoadSSE2SmiOperands(MacroAssembler* masm);
  static void LoadSSE2NumberOperands(MacroAssembler* masm);
  static void LoadSSE2UnknownOperands(MacroAssembler* masm,
                                      Label* not_numbers);

  // Takes the operands in rdx and rax and loads them as integers in rax
  // and rcx.
  static void LoadAsIntegers(MacroAssembler* masm,
                             Label* operand_conversion_failure,
                             Register heap_number_map);
  // As above, but we know the operands to be numbers. In that case,
  // conversion can't fail.
  static void LoadNumbersAsIntegers(MacroAssembler* masm);

  // Tries to convert two values to smis losslessly.
  // This fails if either argument is not a Smi nor a HeapNumber,
  // or if it's a HeapNumber with a value that can't be converted
  // losslessly to a Smi. In that case, control transitions to the
  // on_not_smis label.
  // On success, either control goes to the on_success label (if one is
  // provided), or it falls through at the end of the code (if on_success
  // is NULL).
  // On success, both first and second holds Smi tagged values.
  // One of first or second must be non-Smi when entering.
  static void NumbersToSmis(MacroAssembler* masm,
                            Register first,
                            Register second,
                            Register scratch1,
                            Register scratch2,
                            Register scratch3,
                            Label* on_success,
                            Label* on_not_smis);
};


// Get the integer part of a heap number.
// Overwrites the contents of rdi, rbx and rcx. Result cannot be rdi or rbx.
void IntegerConvert(MacroAssembler* masm,
                    Register result,
                    Register source) {
  // Result may be rcx. If result and source are the same register, source will
  // be overwritten.
  ASSERT(!result.is(rdi) && !result.is(rbx));
  // TODO(lrn): When type info reaches here, if value is a 32-bit integer, use
  // cvttsd2si (32-bit version) directly.
  Register double_exponent = rbx;
  Register double_value = rdi;
  Label done, exponent_63_plus;
  // Get double and extract exponent.
  __ movq(double_value, FieldOperand(source, HeapNumber::kValueOffset));
  // Clear result preemptively, in case we need to return zero.
  __ xorl(result, result);
  __ movq(xmm0, double_value);  // Save copy in xmm0 in case we need it there.
  // Double to remove sign bit, shift exponent down to least significant bits.
  // and subtract bias to get the unshifted, unbiased exponent.
  __ lea(double_exponent, Operand(double_value, double_value, times_1, 0));
  __ shr(double_exponent, Immediate(64 - HeapNumber::kExponentBits));
  __ subl(double_exponent, Immediate(HeapNumber::kExponentBias));
  // Check whether the exponent is too big for a 63 bit unsigned integer.
  __ cmpl(double_exponent, Immediate(63));
  __ j(above_equal, &exponent_63_plus, Label::kNear);
  // Handle exponent range 0..62.
  __ cvttsd2siq(result, xmm0);
  __ jmp(&done, Label::kNear);

  __ bind(&exponent_63_plus);
  // Exponent negative or 63+.
  __ cmpl(double_exponent, Immediate(83));
  // If exponent negative or above 83, number contains no significant bits in
  // the range 0..2^31, so result is zero, and rcx already holds zero.
  __ j(above, &done, Label::kNear);

  // Exponent in rage 63..83.
  // Mantissa * 2^exponent contains bits in the range 2^0..2^31, namely
  // the least significant exponent-52 bits.

  // Negate low bits of mantissa if value is negative.
  __ addq(double_value, double_value);  // Move sign bit to carry.
  __ sbbl(result, result);  // And convert carry to -1 in result register.
  // if scratch2 is negative, do (scratch2-1)^-1, otherwise (scratch2-0)^0.
  __ addl(double_value, result);
  // Do xor in opposite directions depending on where we want the result
  // (depending on whether result is rcx or not).

  if (result.is(rcx)) {
    __ xorl(double_value, result);
    // Left shift mantissa by (exponent - mantissabits - 1) to save the
    // bits that have positional values below 2^32 (the extra -1 comes from the
    // doubling done above to move the sign bit into the carry flag).
    __ leal(rcx, Operand(double_exponent, -HeapNumber::kMantissaBits - 1));
    __ shll_cl(double_value);
    __ movl(result, double_value);
  } else {
    // As the then-branch, but move double-value to result before shifting.
    __ xorl(result, double_value);
    __ leal(rcx, Operand(double_exponent, -HeapNumber::kMantissaBits - 1));
    __ shll_cl(result);
  }

  __ bind(&done);
}


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
  __ pop(rcx);  // Save return address.

  __ push(rax);  // the operand
  __ Push(Smi::FromInt(op_));
  __ Push(Smi::FromInt(mode_));
  __ Push(Smi::FromInt(operand_type_));

  __ push(rcx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
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
  Label slow;
  GenerateSmiCodeSub(masm, &slow, &slow, Label::kNear, Label::kNear);
  __ bind(&slow);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateSmiStubBitNot(MacroAssembler* masm) {
  Label non_smi;
  GenerateSmiCodeBitNot(masm, &non_smi, Label::kNear);
  __ bind(&non_smi);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateSmiCodeSub(MacroAssembler* masm,
                                     Label* non_smi,
                                     Label* slow,
                                     Label::Distance non_smi_near,
                                     Label::Distance slow_near) {
  Label done;
  __ JumpIfNotSmi(rax, non_smi, non_smi_near);
  __ SmiNeg(rax, rax, &done, Label::kNear);
  __ jmp(slow, slow_near);
  __ bind(&done);
  __ ret(0);
}


void UnaryOpStub::GenerateSmiCodeBitNot(MacroAssembler* masm,
                                        Label* non_smi,
                                        Label::Distance non_smi_near) {
  __ JumpIfNotSmi(rax, non_smi, non_smi_near);
  __ SmiNot(rax, rax);
  __ ret(0);
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
  GenerateSmiCodeSub(masm, &non_smi, &call_builtin, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeSub(masm, &slow);
  __ bind(&slow);
  GenerateTypeTransition(masm);
  __ bind(&call_builtin);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateHeapNumberStubBitNot(
    MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeBitNot(masm, &non_smi, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeBitNot(masm, &slow);
  __ bind(&slow);
  GenerateTypeTransition(masm);
}


void UnaryOpStub::GenerateHeapNumberCodeSub(MacroAssembler* masm,
                                            Label* slow) {
  // Check if the operand is a heap number.
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, slow);

  // Operand is a float, negate its value by flipping the sign bit.
  if (mode_ == UNARY_OVERWRITE) {
    __ Set(kScratchRegister, 0x01);
    __ shl(kScratchRegister, Immediate(63));
    __ xor_(FieldOperand(rax, HeapNumber::kValueOffset), kScratchRegister);
  } else {
    // Allocate a heap number before calculating the answer,
    // so we don't have an untagged double around during GC.
    Label slow_allocate_heapnumber, heapnumber_allocated;
    __ AllocateHeapNumber(rcx, rbx, &slow_allocate_heapnumber);
    __ jmp(&heapnumber_allocated);

    __ bind(&slow_allocate_heapnumber);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ push(rax);
      __ CallRuntime(Runtime::kNumberAlloc, 0);
      __ movq(rcx, rax);
      __ pop(rax);
    }
    __ bind(&heapnumber_allocated);
    // rcx: allocated 'empty' number

    // Copy the double value to the new heap number, flipping the sign.
    __ movq(rdx, FieldOperand(rax, HeapNumber::kValueOffset));
    __ Set(kScratchRegister, 0x01);
    __ shl(kScratchRegister, Immediate(63));
    __ xor_(rdx, kScratchRegister);  // Flip sign.
    __ movq(FieldOperand(rcx, HeapNumber::kValueOffset), rdx);
    __ movq(rax, rcx);
  }
  __ ret(0);
}


void UnaryOpStub::GenerateHeapNumberCodeBitNot(MacroAssembler* masm,
                                               Label* slow) {
  // Check if the operand is a heap number.
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, slow);

  // Convert the heap number in rax to an untagged integer in rcx.
  IntegerConvert(masm, rax, rax);

  // Do the bitwise operation and smi tag the result.
  __ notl(rax);
  __ Integer32ToSmi(rax, rax);
  __ ret(0);
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
  GenerateSmiCodeSub(masm, &non_smi, &slow, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeSub(masm, &slow);
  __ bind(&slow);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateGenericStubBitNot(MacroAssembler* masm) {
  Label non_smi, slow;
  GenerateSmiCodeBitNot(masm, &non_smi, Label::kNear);
  __ bind(&non_smi);
  GenerateHeapNumberCodeBitNot(masm, &slow);
  __ bind(&slow);
  GenerateGenericCodeFallback(masm);
}


void UnaryOpStub::GenerateGenericCodeFallback(MacroAssembler* masm) {
  // Handle the slow case by jumping to the JavaScript builtin.
  __ pop(rcx);  // pop return address
  __ push(rax);
  __ push(rcx);  // push return address
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


void BinaryOpStub::GenerateTypeTransition(MacroAssembler* masm) {
  __ pop(rcx);  // Save return address.
  __ push(rdx);
  __ push(rax);
  // Left and right arguments are now on top.
  // Push this stub's key. Although the operation and the type info are
  // encoded into the key, the encoding is opaque, so push them too.
  __ Push(Smi::FromInt(MinorKey()));
  __ Push(Smi::FromInt(op_));
  __ Push(Smi::FromInt(operands_type_));

  __ push(rcx);  // Push return address.

  // Patch the caller to an appropriate specialized stub and return the
  // operation result to the caller of the stub.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(IC::kBinaryOp_Patch),
                        masm->isolate()),
      5,
      1);
}


void BinaryOpStub::Generate(MacroAssembler* masm) {
  // Explicitly allow generation of nested stubs. It is safe here because
  // generation code does not use any raw pointers.
  AllowStubCallsScope allow_stub_calls(masm, true);

  switch (operands_type_) {
    case BinaryOpIC::UNINITIALIZED:
      GenerateTypeTransition(masm);
      break;
    case BinaryOpIC::SMI:
      GenerateSmiStub(masm);
      break;
    case BinaryOpIC::INT32:
      UNREACHABLE();
      // The int32 case is identical to the Smi case.  We avoid creating this
      // ic state on x64.
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


void BinaryOpStub::GenerateSmiCode(
    MacroAssembler* masm,
    Label* slow,
    SmiCodeGenerateHeapNumberResults allow_heapnumber_results) {

  // Arguments to BinaryOpStub are in rdx and rax.
  Register left = rdx;
  Register right = rax;

  // We only generate heapnumber answers for overflowing calculations
  // for the four basic arithmetic operations and logical right shift by 0.
  bool generate_inline_heapnumber_results =
      (allow_heapnumber_results == ALLOW_HEAPNUMBER_RESULTS) &&
      (op_ == Token::ADD || op_ == Token::SUB ||
       op_ == Token::MUL || op_ == Token::DIV || op_ == Token::SHR);

  // Smi check of both operands.  If op is BIT_OR, the check is delayed
  // until after the OR operation.
  Label not_smis;
  Label use_fp_on_smis;
  Label fail;

  if (op_ != Token::BIT_OR) {
    Comment smi_check_comment(masm, "-- Smi check arguments");
    __ JumpIfNotBothSmi(left, right, &not_smis);
  }

  Label smi_values;
  __ bind(&smi_values);
  // Perform the operation.
  Comment perform_smi(masm, "-- Perform smi operation");
  switch (op_) {
    case Token::ADD:
      ASSERT(right.is(rax));
      __ SmiAdd(right, right, left, &use_fp_on_smis);  // ADD is commutative.
      break;

    case Token::SUB:
      __ SmiSub(left, left, right, &use_fp_on_smis);
      __ movq(rax, left);
      break;

    case Token::MUL:
      ASSERT(right.is(rax));
      __ SmiMul(right, right, left, &use_fp_on_smis);  // MUL is commutative.
      break;

    case Token::DIV:
      // SmiDiv will not accept left in rdx or right in rax.
      left = rcx;
      right = rbx;
      __ movq(rbx, rax);
      __ movq(rcx, rdx);
      __ SmiDiv(rax, left, right, &use_fp_on_smis);
      break;

    case Token::MOD:
      // SmiMod will not accept left in rdx or right in rax.
      left = rcx;
      right = rbx;
      __ movq(rbx, rax);
      __ movq(rcx, rdx);
      __ SmiMod(rax, left, right, &use_fp_on_smis);
      break;

    case Token::BIT_OR: {
      ASSERT(right.is(rax));
      __ SmiOrIfSmis(right, right, left, &not_smis);  // BIT_OR is commutative.
      break;
      }
    case Token::BIT_XOR:
      ASSERT(right.is(rax));
      __ SmiXor(right, right, left);  // BIT_XOR is commutative.
      break;

    case Token::BIT_AND:
      ASSERT(right.is(rax));
      __ SmiAnd(right, right, left);  // BIT_AND is commutative.
      break;

    case Token::SHL:
      __ SmiShiftLeft(left, left, right);
      __ movq(rax, left);
      break;

    case Token::SAR:
      __ SmiShiftArithmeticRight(left, left, right);
      __ movq(rax, left);
      break;

    case Token::SHR:
      __ SmiShiftLogicalRight(left, left, right, &use_fp_on_smis);
      __ movq(rax, left);
      break;

    default:
      UNREACHABLE();
  }

  // 5. Emit return of result in rax.  Some operations have registers pushed.
  __ ret(0);

  if (use_fp_on_smis.is_linked()) {
    // 6. For some operations emit inline code to perform floating point
    //    operations on known smis (e.g., if the result of the operation
    //    overflowed the smi range).
    __ bind(&use_fp_on_smis);
    if (op_ == Token::DIV || op_ == Token::MOD) {
      // Restore left and right to rdx and rax.
      __ movq(rdx, rcx);
      __ movq(rax, rbx);
    }

    if (generate_inline_heapnumber_results) {
      __ AllocateHeapNumber(rcx, rbx, slow);
      Comment perform_float(masm, "-- Perform float operation on smis");
      if (op_ == Token::SHR) {
        __ SmiToInteger32(left, left);
        __ cvtqsi2sd(xmm0, left);
      } else {
        FloatingPointHelper::LoadSSE2SmiOperands(masm);
        switch (op_) {
        case Token::ADD: __ addsd(xmm0, xmm1); break;
        case Token::SUB: __ subsd(xmm0, xmm1); break;
        case Token::MUL: __ mulsd(xmm0, xmm1); break;
        case Token::DIV: __ divsd(xmm0, xmm1); break;
        default: UNREACHABLE();
        }
      }
      __ movsd(FieldOperand(rcx, HeapNumber::kValueOffset), xmm0);
      __ movq(rax, rcx);
      __ ret(0);
    } else {
      __ jmp(&fail);
    }
  }

  // 7. Non-smi operands reach the end of the code generated by
  //    GenerateSmiCode, and fall through to subsequent code,
  //    with the operands in rdx and rax.
  //    But first we check if non-smi values are HeapNumbers holding
  //    values that could be smi.
  __ bind(&not_smis);
  Comment done_comment(masm, "-- Enter non-smi code");
  FloatingPointHelper::NumbersToSmis(masm, left, right, rbx, rdi, rcx,
                                     &smi_values, &fail);
  __ jmp(&smi_values);
  __ bind(&fail);
}


void BinaryOpStub::GenerateFloatingPointCode(MacroAssembler* masm,
                                             Label* allocation_failure,
                                             Label* non_numeric_failure) {
  switch (op_) {
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV: {
      FloatingPointHelper::LoadSSE2UnknownOperands(masm, non_numeric_failure);

      switch (op_) {
        case Token::ADD: __ addsd(xmm0, xmm1); break;
        case Token::SUB: __ subsd(xmm0, xmm1); break;
        case Token::MUL: __ mulsd(xmm0, xmm1); break;
        case Token::DIV: __ divsd(xmm0, xmm1); break;
        default: UNREACHABLE();
      }
      GenerateHeapResultAllocation(masm, allocation_failure);
      __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm0);
      __ ret(0);
      break;
    }
    case Token::MOD: {
      // For MOD we jump to the allocation_failure label, to call runtime.
      __ jmp(allocation_failure);
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SAR:
    case Token::SHL:
    case Token::SHR: {
      Label non_smi_shr_result;
      Register heap_number_map = r9;
      __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
      FloatingPointHelper::LoadAsIntegers(masm, non_numeric_failure,
                                          heap_number_map);
      switch (op_) {
        case Token::BIT_OR:  __ orl(rax, rcx); break;
        case Token::BIT_AND: __ andl(rax, rcx); break;
        case Token::BIT_XOR: __ xorl(rax, rcx); break;
        case Token::SAR: __ sarl_cl(rax); break;
        case Token::SHL: __ shll_cl(rax); break;
        case Token::SHR: {
          __ shrl_cl(rax);
          // Check if result is negative. This can only happen for a shift
          // by zero.
          __ testl(rax, rax);
          __ j(negative, &non_smi_shr_result);
          break;
        }
        default: UNREACHABLE();
      }
      STATIC_ASSERT(kSmiValueSize == 32);
      // Tag smi result and return.
      __ Integer32ToSmi(rax, rax);
      __ Ret();

      // Logical shift right can produce an unsigned int32 that is not
      // an int32, and so is not in the smi range.  Allocate a heap number
      // in that case.
      if (op_ == Token::SHR) {
        __ bind(&non_smi_shr_result);
        Label allocation_failed;
        __ movl(rbx, rax);  // rbx holds result value (uint32 value as int64).
        // Allocate heap number in new space.
        // Not using AllocateHeapNumber macro in order to reuse
        // already loaded heap_number_map.
        __ AllocateInNewSpace(HeapNumber::kSize,
                              rax,
                              rdx,
                              no_reg,
                              &allocation_failed,
                              TAG_OBJECT);
        // Set the map.
        if (FLAG_debug_code) {
          __ AbortIfNotRootValue(heap_number_map,
                                 Heap::kHeapNumberMapRootIndex,
                                 "HeapNumberMap register clobbered.");
        }
        __ movq(FieldOperand(rax, HeapObject::kMapOffset),
                heap_number_map);
        __ cvtqsi2sd(xmm0, rbx);
        __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm0);
        __ Ret();

        __ bind(&allocation_failed);
        // We need tagged values in rdx and rax for the following code,
        // not int32 in rax and rcx.
        __ Integer32ToSmi(rax, rcx);
        __ Integer32ToSmi(rdx, rbx);
        __ jmp(allocation_failure);
      }
      break;
    }
    default: UNREACHABLE(); break;
  }
  // No fall-through from this generated code.
  if (FLAG_debug_code) {
    __ Abort("Unexpected fall-through in "
             "BinaryStub::GenerateFloatingPointCode.");
  }
}


void BinaryOpStub::GenerateStringAddCode(MacroAssembler* masm) {
  ASSERT(op_ == Token::ADD);
  Label left_not_string, call_runtime;

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;

  // Test if left operand is a string.
  __ JumpIfSmi(left, &left_not_string, Label::kNear);
  __ CmpObjectType(left, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &left_not_string, Label::kNear);
  StringAddStub string_add_left_stub(NO_STRING_CHECK_LEFT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_left_stub);

  // Left operand is not a string, test right.
  __ bind(&left_not_string);
  __ JumpIfSmi(right, &call_runtime, Label::kNear);
  __ CmpObjectType(right, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &call_runtime, Label::kNear);

  StringAddStub string_add_right_stub(NO_STRING_CHECK_RIGHT_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_right_stub);

  // Neither argument is a string.
  __ bind(&call_runtime);
}


void BinaryOpStub::GenerateCallRuntimeCode(MacroAssembler* masm) {
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
    case Token::SHL:
      __ InvokeBuiltin(Builtins::SHL, JUMP_FUNCTION);
      break;
    case Token::SHR:
      __ InvokeBuiltin(Builtins::SHR, JUMP_FUNCTION);
      break;
    default:
      UNREACHABLE();
  }
}


void BinaryOpStub::GenerateSmiStub(MacroAssembler* masm) {
  Label call_runtime;
  if (result_type_ == BinaryOpIC::UNINITIALIZED ||
      result_type_ == BinaryOpIC::SMI) {
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

  if (call_runtime.is_linked()) {
    __ bind(&call_runtime);
    GenerateCallRuntimeCode(masm);
  }
}


void BinaryOpStub::GenerateStringStub(MacroAssembler* masm) {
  ASSERT(operands_type_ == BinaryOpIC::STRING);
  ASSERT(op_ == Token::ADD);
  GenerateStringAddCode(masm);
  // Try to add arguments as strings, otherwise, transition to the generic
  // BinaryOpIC type.
  GenerateTypeTransition(masm);
}


void BinaryOpStub::GenerateBothStringStub(MacroAssembler* masm) {
  Label call_runtime;
  ASSERT(operands_type_ == BinaryOpIC::BOTH_STRING);
  ASSERT(op_ == Token::ADD);
  // If both arguments are strings, call the string add stub.
  // Otherwise, do a transition.

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;

  // Test if left operand is a string.
  __ JumpIfSmi(left, &call_runtime);
  __ CmpObjectType(left, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &call_runtime);

  // Test if right operand is a string.
  __ JumpIfSmi(right, &call_runtime);
  __ CmpObjectType(right, FIRST_NONSTRING_TYPE, rcx);
  __ j(above_equal, &call_runtime);

  StringAddStub string_add_stub(NO_STRING_CHECK_IN_STUB);
  GenerateRegisterArgsPush(masm);
  __ TailCallStub(&string_add_stub);

  __ bind(&call_runtime);
  GenerateTypeTransition(masm);
}


void BinaryOpStub::GenerateOddballStub(MacroAssembler* masm) {
  Label call_runtime;

  if (op_ == Token::ADD) {
    // Handle string addition here, because it is the only operation
    // that does not do a ToNumber conversion on the operands.
    GenerateStringAddCode(masm);
  }

  // Convert oddball arguments to numbers.
  Label check, done;
  __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &check, Label::kNear);
  if (Token::IsBitOp(op_)) {
    __ xor_(rdx, rdx);
  } else {
    __ LoadRoot(rdx, Heap::kNanValueRootIndex);
  }
  __ jmp(&done, Label::kNear);
  __ bind(&check);
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, &done, Label::kNear);
  if (Token::IsBitOp(op_)) {
    __ xor_(rax, rax);
  } else {
    __ LoadRoot(rax, Heap::kNanValueRootIndex);
  }
  __ bind(&done);

  GenerateHeapNumberStub(masm);
}


void BinaryOpStub::GenerateHeapNumberStub(MacroAssembler* masm) {
  Label gc_required, not_number;
  GenerateFloatingPointCode(masm, &gc_required, &not_number);

  __ bind(&not_number);
  GenerateTypeTransition(masm);

  __ bind(&gc_required);
  GenerateCallRuntimeCode(masm);
}


void BinaryOpStub::GenerateGeneric(MacroAssembler* masm) {
  Label call_runtime, call_string_add_or_runtime;

  GenerateSmiCode(masm, &call_runtime, ALLOW_HEAPNUMBER_RESULTS);

  GenerateFloatingPointCode(masm, &call_runtime, &call_string_add_or_runtime);

  __ bind(&call_string_add_or_runtime);
  if (op_ == Token::ADD) {
    GenerateStringAddCode(masm);
  }

  __ bind(&call_runtime);
  GenerateCallRuntimeCode(masm);
}


void BinaryOpStub::GenerateHeapResultAllocation(MacroAssembler* masm,
                                                Label* alloc_failure) {
  Label skip_allocation;
  OverwriteMode mode = mode_;
  switch (mode) {
    case OVERWRITE_LEFT: {
      // If the argument in rdx is already an object, we skip the
      // allocation of a heap number.
      __ JumpIfNotSmi(rdx, &skip_allocation);
      // Allocate a heap number for the result. Keep eax and edx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(rbx, rcx, alloc_failure);
      // Now rdx can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ movq(rdx, rbx);
      __ bind(&skip_allocation);
      // Use object in rdx as a result holder
      __ movq(rax, rdx);
      break;
    }
    case OVERWRITE_RIGHT:
      // If the argument in rax is already an object, we skip the
      // allocation of a heap number.
      __ JumpIfNotSmi(rax, &skip_allocation);
      // Fall through!
    case NO_OVERWRITE:
      // Allocate a heap number for the result. Keep rax and rdx intact
      // for the possible runtime call.
      __ AllocateHeapNumber(rbx, rcx, alloc_failure);
      // Now rax can be overwritten losing one of the arguments as we are
      // now done and will not need it any more.
      __ movq(rax, rbx);
      __ bind(&skip_allocation);
      break;
    default: UNREACHABLE();
  }
}


void BinaryOpStub::GenerateRegisterArgsPush(MacroAssembler* masm) {
  __ pop(rcx);
  __ push(rdx);
  __ push(rax);
  __ push(rcx);
}


void TranscendentalCacheStub::Generate(MacroAssembler* masm) {
  // TAGGED case:
  //   Input:
  //     rsp[8]: argument (should be number).
  //     rsp[0]: return address.
  //   Output:
  //     rax: tagged double result.
  // UNTAGGED case:
  //   Input::
  //     rsp[0]: return address.
  //     xmm1: untagged double input argument
  //   Output:
  //     xmm1: untagged double result.

  Label runtime_call;
  Label runtime_call_clear_stack;
  Label skip_cache;
  const bool tagged = (argument_type_ == TAGGED);
  if (tagged) {
    Label input_not_smi, loaded;
    // Test that rax is a number.
    __ movq(rax, Operand(rsp, kPointerSize));
    __ JumpIfNotSmi(rax, &input_not_smi, Label::kNear);
    // Input is a smi. Untag and load it onto the FPU stack.
    // Then load the bits of the double into rbx.
    __ SmiToInteger32(rax, rax);
    __ subq(rsp, Immediate(kDoubleSize));
    __ cvtlsi2sd(xmm1, rax);
    __ movsd(Operand(rsp, 0), xmm1);
    __ movq(rbx, xmm1);
    __ movq(rdx, xmm1);
    __ fld_d(Operand(rsp, 0));
    __ addq(rsp, Immediate(kDoubleSize));
    __ jmp(&loaded, Label::kNear);

    __ bind(&input_not_smi);
    // Check if input is a HeapNumber.
    __ LoadRoot(rbx, Heap::kHeapNumberMapRootIndex);
    __ cmpq(rbx, FieldOperand(rax, HeapObject::kMapOffset));
    __ j(not_equal, &runtime_call);
    // Input is a HeapNumber. Push it on the FPU stack and load its
    // bits into rbx.
    __ fld_d(FieldOperand(rax, HeapNumber::kValueOffset));
    __ movq(rbx, FieldOperand(rax, HeapNumber::kValueOffset));
    __ movq(rdx, rbx);

    __ bind(&loaded);
  } else {  // UNTAGGED.
    __ movq(rbx, xmm1);
    __ movq(rdx, xmm1);
  }

  // ST[0] == double value, if TAGGED.
  // rbx = bits of double value.
  // rdx = also bits of double value.
  // Compute hash (h is 32 bits, bits are 64 and the shifts are arithmetic):
  //   h = h0 = bits ^ (bits >> 32);
  //   h ^= h >> 16;
  //   h ^= h >> 8;
  //   h = h & (cacheSize - 1);
  // or h = (h0 ^ (h0 >> 8) ^ (h0 >> 16) ^ (h0 >> 24)) & (cacheSize - 1)
  __ sar(rdx, Immediate(32));
  __ xorl(rdx, rbx);
  __ movl(rcx, rdx);
  __ movl(rax, rdx);
  __ movl(rdi, rdx);
  __ sarl(rdx, Immediate(8));
  __ sarl(rcx, Immediate(16));
  __ sarl(rax, Immediate(24));
  __ xorl(rcx, rdx);
  __ xorl(rax, rdi);
  __ xorl(rcx, rax);
  ASSERT(IsPowerOf2(TranscendentalCache::SubCache::kCacheSize));
  __ andl(rcx, Immediate(TranscendentalCache::SubCache::kCacheSize - 1));

  // ST[0] == double value.
  // rbx = bits of double value.
  // rcx = TranscendentalCache::hash(double value).
  ExternalReference cache_array =
      ExternalReference::transcendental_cache_array_address(masm->isolate());
  __ movq(rax, cache_array);
  int cache_array_index =
      type_ * sizeof(Isolate::Current()->transcendental_cache()->caches_[0]);
  __ movq(rax, Operand(rax, cache_array_index));
  // rax points to the cache for the type type_.
  // If NULL, the cache hasn't been initialized yet, so go through runtime.
  __ testq(rax, rax);
  __ j(zero, &runtime_call_clear_stack);  // Only clears stack if TAGGED.
#ifdef DEBUG
  // Check that the layout of cache elements match expectations.
  {  // NOLINT - doesn't like a single brace on a line.
    TranscendentalCache::SubCache::Element test_elem[2];
    char* elem_start = reinterpret_cast<char*>(&test_elem[0]);
    char* elem2_start = reinterpret_cast<char*>(&test_elem[1]);
    char* elem_in0  = reinterpret_cast<char*>(&(test_elem[0].in[0]));
    char* elem_in1  = reinterpret_cast<char*>(&(test_elem[0].in[1]));
    char* elem_out = reinterpret_cast<char*>(&(test_elem[0].output));
    // Two uint_32's and a pointer per element.
    CHECK_EQ(16, static_cast<int>(elem2_start - elem_start));
    CHECK_EQ(0, static_cast<int>(elem_in0 - elem_start));
    CHECK_EQ(kIntSize, static_cast<int>(elem_in1 - elem_start));
    CHECK_EQ(2 * kIntSize, static_cast<int>(elem_out - elem_start));
  }
#endif
  // Find the address of the rcx'th entry in the cache, i.e., &rax[rcx*16].
  __ addl(rcx, rcx);
  __ lea(rcx, Operand(rax, rcx, times_8, 0));
  // Check if cache matches: Double value is stored in uint32_t[2] array.
  Label cache_miss;
  __ cmpq(rbx, Operand(rcx, 0));
  __ j(not_equal, &cache_miss, Label::kNear);
  // Cache hit!
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->transcendental_cache_hit(), 1);
  __ movq(rax, Operand(rcx, 2 * kIntSize));
  if (tagged) {
    __ fstp(0);  // Clear FPU stack.
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
    __ Ret();
  }

  __ bind(&cache_miss);
  __ IncrementCounter(counters->transcendental_cache_miss(), 1);
  // Update cache with new value.
  if (tagged) {
  __ AllocateHeapNumber(rax, rdi, &runtime_call_clear_stack);
  } else {  // UNTAGGED.
    __ AllocateHeapNumber(rax, rdi, &skip_cache);
    __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm1);
    __ fld_d(FieldOperand(rax, HeapNumber::kValueOffset));
  }
  GenerateOperation(masm, type_);
  __ movq(Operand(rcx, 0), rbx);
  __ movq(Operand(rcx, 2 * kIntSize), rax);
  __ fstp_d(FieldOperand(rax, HeapNumber::kValueOffset));
  if (tagged) {
    __ ret(kPointerSize);
  } else {  // UNTAGGED.
    __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
    __ Ret();

    // Skip cache and return answer directly, only in untagged case.
    __ bind(&skip_cache);
    __ subq(rsp, Immediate(kDoubleSize));
    __ movsd(Operand(rsp, 0), xmm1);
    __ fld_d(Operand(rsp, 0));
    GenerateOperation(masm, type_);
    __ fstp_d(Operand(rsp, 0));
    __ movsd(xmm1, Operand(rsp, 0));
    __ addq(rsp, Immediate(kDoubleSize));
    // We return the value in xmm1 without adding it to the cache, but
    // we cause a scavenging GC so that future allocations will succeed.
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      // Allocate an unused object bigger than a HeapNumber.
      __ Push(Smi::FromInt(2 * kDoubleSize));
      __ CallRuntimeSaveDoubles(Runtime::kAllocateInNewSpace);
    }
    __ Ret();
  }

  // Call runtime, doing whatever allocation and cleanup is necessary.
  if (tagged) {
    __ bind(&runtime_call_clear_stack);
    __ fstp(0);
    __ bind(&runtime_call);
    __ TailCallExternalReference(
        ExternalReference(RuntimeFunction(), masm->isolate()), 1, 1);
  } else {  // UNTAGGED.
    __ bind(&runtime_call_clear_stack);
    __ bind(&runtime_call);
    __ AllocateHeapNumber(rax, rdi, &skip_cache);
    __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), xmm1);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ push(rax);
      __ CallRuntime(RuntimeFunction(), 1);
    }
    __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
    __ Ret();
  }
}


Runtime::FunctionId TranscendentalCacheStub::RuntimeFunction() {
  switch (type_) {
    // Add more cases when necessary.
    case TranscendentalCache::SIN: return Runtime::kMath_sin;
    case TranscendentalCache::COS: return Runtime::kMath_cos;
    case TranscendentalCache::TAN: return Runtime::kMath_tan;
    case TranscendentalCache::LOG: return Runtime::kMath_log;
    default:
      UNIMPLEMENTED();
      return Runtime::kAbort;
  }
}


void TranscendentalCacheStub::GenerateOperation(
    MacroAssembler* masm, TranscendentalCache::Type type) {
  // Registers:
  // rax: Newly allocated HeapNumber, which must be preserved.
  // rbx: Bits of input double. Must be preserved.
  // rcx: Pointer to cache entry. Must be preserved.
  // st(0): Input double
  Label done;
  if (type == TranscendentalCache::SIN ||
      type == TranscendentalCache::COS ||
      type == TranscendentalCache::TAN) {
    // Both fsin and fcos require arguments in the range +/-2^63 and
    // return NaN for infinities and NaN. They can share all code except
    // the actual fsin/fcos operation.
    Label in_range;
    // If argument is outside the range -2^63..2^63, fsin/cos doesn't
    // work. We must reduce it to the appropriate range.
    __ movq(rdi, rbx);
    // Move exponent and sign bits to low bits.
    __ shr(rdi, Immediate(HeapNumber::kMantissaBits));
    // Remove sign bit.
    __ andl(rdi, Immediate((1 << HeapNumber::kExponentBits) - 1));
    int supported_exponent_limit = (63 + HeapNumber::kExponentBias);
    __ cmpl(rdi, Immediate(supported_exponent_limit));
    __ j(below, &in_range);
    // Check for infinity and NaN. Both return NaN for sin.
    __ cmpl(rdi, Immediate(0x7ff));
    Label non_nan_result;
    __ j(not_equal, &non_nan_result, Label::kNear);
    // Input is +/-Infinity or NaN. Result is NaN.
    __ fstp(0);
    // NaN is represented by 0x7ff8000000000000.
    __ subq(rsp, Immediate(kPointerSize));
    __ movl(Operand(rsp, 4), Immediate(0x7ff80000));
    __ movl(Operand(rsp, 0), Immediate(0x00000000));
    __ fld_d(Operand(rsp, 0));
    __ addq(rsp, Immediate(kPointerSize));
    __ jmp(&done);

    __ bind(&non_nan_result);

    // Use fpmod to restrict argument to the range +/-2*PI.
    __ movq(rdi, rax);  // Save rax before using fnstsw_ax.
    __ fldpi();
    __ fadd(0);
    __ fld(1);
    // FPU Stack: input, 2*pi, input.
    {
      Label no_exceptions;
      __ fwait();
      __ fnstsw_ax();
      // Clear if Illegal Operand or Zero Division exceptions are set.
      __ testl(rax, Immediate(5));  // #IO and #ZD flags of FPU status word.
      __ j(zero, &no_exceptions);
      __ fnclex();
      __ bind(&no_exceptions);
    }

    // Compute st(0) % st(1)
    {
      Label partial_remainder_loop;
      __ bind(&partial_remainder_loop);
      __ fprem1();
      __ fwait();
      __ fnstsw_ax();
      __ testl(rax, Immediate(0x400));  // Check C2 bit of FPU status word.
      // If C2 is set, computation only has partial result. Loop to
      // continue computation.
      __ j(not_zero, &partial_remainder_loop);
  }
    // FPU Stack: input, 2*pi, input % 2*pi
    __ fstp(2);
    // FPU Stack: input % 2*pi, 2*pi,
    __ fstp(0);
    // FPU Stack: input % 2*pi
    __ movq(rax, rdi);  // Restore rax, pointer to the new HeapNumber.
    __ bind(&in_range);
    switch (type) {
      case TranscendentalCache::SIN:
        __ fsin();
        break;
      case TranscendentalCache::COS:
        __ fcos();
        break;
      case TranscendentalCache::TAN:
        // FPTAN calculates tangent onto st(0) and pushes 1.0 onto the
        // FP register stack.
        __ fptan();
        __ fstp(0);  // Pop FP register stack.
        break;
      default:
        UNREACHABLE();
    }
    __ bind(&done);
  } else {
    ASSERT(type == TranscendentalCache::LOG);
    __ fldln2();
    __ fxch();
    __ fyl2x();
  }
}


// Input: rdx, rax are the left and right objects of a bit op.
// Output: rax, rcx are left and right integers for a bit op.
void FloatingPointHelper::LoadNumbersAsIntegers(MacroAssembler* masm) {
  // Check float operands.
  Label done;
  Label rax_is_smi;
  Label rax_is_object;
  Label rdx_is_object;

  __ JumpIfNotSmi(rdx, &rdx_is_object);
  __ SmiToInteger32(rdx, rdx);
  __ JumpIfSmi(rax, &rax_is_smi);

  __ bind(&rax_is_object);
  IntegerConvert(masm, rcx, rax);  // Uses rdi, rcx and rbx.
  __ jmp(&done);

  __ bind(&rdx_is_object);
  IntegerConvert(masm, rdx, rdx);  // Uses rdi, rcx and rbx.
  __ JumpIfNotSmi(rax, &rax_is_object);
  __ bind(&rax_is_smi);
  __ SmiToInteger32(rcx, rax);

  __ bind(&done);
  __ movl(rax, rdx);
}


// Input: rdx, rax are the left and right objects of a bit op.
// Output: rax, rcx are left and right integers for a bit op.
// Jump to conversion_failure: rdx and rax are unchanged.
void FloatingPointHelper::LoadAsIntegers(MacroAssembler* masm,
                                         Label* conversion_failure,
                                         Register heap_number_map) {
  // Check float operands.
  Label arg1_is_object, check_undefined_arg1;
  Label arg2_is_object, check_undefined_arg2;
  Label load_arg2, done;

  __ JumpIfNotSmi(rdx, &arg1_is_object);
  __ SmiToInteger32(r8, rdx);
  __ jmp(&load_arg2);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg1);
  __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ Set(r8, 0);
  __ jmp(&load_arg2);

  __ bind(&arg1_is_object);
  __ cmpq(FieldOperand(rdx, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, &check_undefined_arg1);
  // Get the untagged integer version of the rdx heap number in rcx.
  IntegerConvert(masm, r8, rdx);

  // Here r8 has the untagged integer, rax has a Smi or a heap number.
  __ bind(&load_arg2);
  // Test if arg2 is a Smi.
  __ JumpIfNotSmi(rax, &arg2_is_object);
  __ SmiToInteger32(rcx, rax);
  __ jmp(&done);

  // If the argument is undefined it converts to zero (ECMA-262, section 9.5).
  __ bind(&check_undefined_arg2);
  __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
  __ j(not_equal, conversion_failure);
  __ Set(rcx, 0);
  __ jmp(&done);

  __ bind(&arg2_is_object);
  __ cmpq(FieldOperand(rax, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, &check_undefined_arg2);
  // Get the untagged integer version of the rax heap number in rcx.
  IntegerConvert(masm, rcx, rax);
  __ bind(&done);
  __ movl(rax, r8);
}


void FloatingPointHelper::LoadSSE2SmiOperands(MacroAssembler* masm) {
  __ SmiToInteger32(kScratchRegister, rdx);
  __ cvtlsi2sd(xmm0, kScratchRegister);
  __ SmiToInteger32(kScratchRegister, rax);
  __ cvtlsi2sd(xmm1, kScratchRegister);
}


void FloatingPointHelper::LoadSSE2NumberOperands(MacroAssembler* masm) {
  Label load_smi_rdx, load_nonsmi_rax, load_smi_rax, done;
  // Load operand in rdx into xmm0.
  __ JumpIfSmi(rdx, &load_smi_rdx);
  __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  // Load operand in rax into xmm1.
  __ JumpIfSmi(rax, &load_smi_rax);
  __ bind(&load_nonsmi_rax);
  __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_rdx);
  __ SmiToInteger32(kScratchRegister, rdx);
  __ cvtlsi2sd(xmm0, kScratchRegister);
  __ JumpIfNotSmi(rax, &load_nonsmi_rax);

  __ bind(&load_smi_rax);
  __ SmiToInteger32(kScratchRegister, rax);
  __ cvtlsi2sd(xmm1, kScratchRegister);

  __ bind(&done);
}


void FloatingPointHelper::LoadSSE2UnknownOperands(MacroAssembler* masm,
                                                  Label* not_numbers) {
  Label load_smi_rdx, load_nonsmi_rax, load_smi_rax, load_float_rax, done;
  // Load operand in rdx into xmm0, or branch to not_numbers.
  __ LoadRoot(rcx, Heap::kHeapNumberMapRootIndex);
  __ JumpIfSmi(rdx, &load_smi_rdx);
  __ cmpq(FieldOperand(rdx, HeapObject::kMapOffset), rcx);
  __ j(not_equal, not_numbers);  // Argument in rdx is not a number.
  __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  // Load operand in rax into xmm1, or branch to not_numbers.
  __ JumpIfSmi(rax, &load_smi_rax);

  __ bind(&load_nonsmi_rax);
  __ cmpq(FieldOperand(rax, HeapObject::kMapOffset), rcx);
  __ j(not_equal, not_numbers);
  __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));
  __ jmp(&done);

  __ bind(&load_smi_rdx);
  __ SmiToInteger32(kScratchRegister, rdx);
  __ cvtlsi2sd(xmm0, kScratchRegister);
  __ JumpIfNotSmi(rax, &load_nonsmi_rax);

  __ bind(&load_smi_rax);
  __ SmiToInteger32(kScratchRegister, rax);
  __ cvtlsi2sd(xmm1, kScratchRegister);
  __ bind(&done);
}


void FloatingPointHelper::NumbersToSmis(MacroAssembler* masm,
                                        Register first,
                                        Register second,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        Label* on_success,
                                        Label* on_not_smis)   {
  Register heap_number_map = scratch3;
  Register smi_result = scratch1;
  Label done;

  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);

  Label first_smi;
  __ JumpIfSmi(first, &first_smi, Label::kNear);
  __ cmpq(FieldOperand(first, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, on_not_smis);
  // Convert HeapNumber to smi if possible.
  __ movsd(xmm0, FieldOperand(first, HeapNumber::kValueOffset));
  __ movq(scratch2, xmm0);
  __ cvttsd2siq(smi_result, xmm0);
  // Check if conversion was successful by converting back and
  // comparing to the original double's bits.
  __ cvtlsi2sd(xmm1, smi_result);
  __ movq(kScratchRegister, xmm1);
  __ cmpq(scratch2, kScratchRegister);
  __ j(not_equal, on_not_smis);
  __ Integer32ToSmi(first, smi_result);

  __ JumpIfSmi(second, (on_success != NULL) ? on_success : &done);
  __ bind(&first_smi);
  if (FLAG_debug_code) {
    // Second should be non-smi if we get here.
    __ AbortIfSmi(second);
  }
  __ cmpq(FieldOperand(second, HeapObject::kMapOffset), heap_number_map);
  __ j(not_equal, on_not_smis);
  // Convert second to smi, if possible.
  __ movsd(xmm0, FieldOperand(second, HeapNumber::kValueOffset));
  __ movq(scratch2, xmm0);
  __ cvttsd2siq(smi_result, xmm0);
  __ cvtlsi2sd(xmm1, smi_result);
  __ movq(kScratchRegister, xmm1);
  __ cmpq(scratch2, kScratchRegister);
  __ j(not_equal, on_not_smis);
  __ Integer32ToSmi(second, smi_result);
  if (on_success != NULL) {
    __ jmp(on_success);
  } else {
    __ bind(&done);
  }
}


void MathPowStub::Generate(MacroAssembler* masm) {
  // Choose register conforming to calling convention (when bailing out).
#ifdef _WIN64
  const Register exponent = rdx;
#else
  const Register exponent = rdi;
#endif
  const Register base = rax;
  const Register scratch = rcx;
  const XMMRegister double_result = xmm3;
  const XMMRegister double_base = xmm2;
  const XMMRegister double_exponent = xmm1;
  const XMMRegister double_scratch = xmm4;

  Label call_runtime, done, exponent_not_smi, int_exponent;

  // Save 1 in double_result - we need this several times later on.
  __ movq(scratch, Immediate(1));
  __ cvtlsi2sd(double_result, scratch);

  if (exponent_type_ == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack.
    __ movq(base, Operand(rsp, 2 * kPointerSize));
    __ movq(exponent, Operand(rsp, 1 * kPointerSize));
    __ JumpIfSmi(base, &base_is_smi, Label::kNear);
    __ CompareRoot(FieldOperand(base, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);

    __ movsd(double_base, FieldOperand(base, HeapNumber::kValueOffset));
    __ jmp(&unpack_exponent, Label::kNear);

    __ bind(&base_is_smi);
    __ SmiToInteger32(base, base);
    __ cvtlsi2sd(double_base, base);
    __ bind(&unpack_exponent);

    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiToInteger32(exponent, exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ CompareRoot(FieldOperand(exponent, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(not_equal, &call_runtime);
    __ movsd(double_exponent, FieldOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type_ == TAGGED) {
    __ JumpIfNotSmi(exponent, &exponent_not_smi, Label::kNear);
    __ SmiToInteger32(exponent, exponent);
    __ jmp(&int_exponent);

    __ bind(&exponent_not_smi);
    __ movsd(double_exponent, FieldOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type_ != INTEGER) {
    Label fast_power;
    // Detect integer exponents stored as double.
    __ cvttsd2si(exponent, double_exponent);
    // Skip to runtime if possibly NaN (indicated by the indefinite integer).
    __ cmpl(exponent, Immediate(0x80000000u));
    __ j(equal, &call_runtime);
    __ cvtlsi2sd(double_scratch, exponent);
    // Already ruled out NaNs for exponent.
    __ ucomisd(double_exponent, double_scratch);
    __ j(equal, &int_exponent);

    if (exponent_type_ == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label continue_sqrt, continue_rsqrt, not_plus_half;
      // Test for 0.5.
      // Load double_scratch with 0.5.
      __ movq(scratch, V8_UINT64_C(0x3FE0000000000000), RelocInfo::NONE);
      __ movq(double_scratch, scratch);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &not_plus_half, Label::kNear);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      // According to IEEE-754, double-precision -Infinity has the highest
      // 12 bits set and the lowest 52 bits cleared.
      __ movq(scratch, V8_UINT64_C(0xFFF0000000000000), RelocInfo::NONE);
      __ movq(double_scratch, scratch);
      __ ucomisd(double_scratch, double_base);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_sqrt, Label::kNear);
      __ j(carry, &continue_sqrt, Label::kNear);

      // Set result to Infinity in the special case.
      __ xorps(double_result, double_result);
      __ subsd(double_result, double_scratch);
      __ jmp(&done);

      __ bind(&continue_sqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ xorps(double_scratch, double_scratch);
      __ addsd(double_scratch, double_base);  // Convert -0 to 0.
      __ sqrtsd(double_result, double_scratch);
      __ jmp(&done);

      // Test for -0.5.
      __ bind(&not_plus_half);
      // Load double_scratch with -0.5 by substracting 1.
      __ subsd(double_scratch, double_result);
      // Already ruled out NaNs for exponent.
      __ ucomisd(double_scratch, double_exponent);
      __ j(not_equal, &fast_power, Label::kNear);

      // Calculates reciprocal of square root of base.  Check for the special
      // case of Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      // According to IEEE-754, double-precision -Infinity has the highest
      // 12 bits set and the lowest 52 bits cleared.
      __ movq(scratch, V8_UINT64_C(0xFFF0000000000000), RelocInfo::NONE);
      __ movq(double_scratch, scratch);
      __ ucomisd(double_scratch, double_base);
      // Comparing -Infinity with NaN results in "unordered", which sets the
      // zero flag as if both were equal.  However, it also sets the carry flag.
      __ j(not_equal, &continue_rsqrt, Label::kNear);
      __ j(carry, &continue_rsqrt, Label::kNear);

      // Set result to 0 in the special case.
      __ xorps(double_result, double_result);
      __ jmp(&done);

      __ bind(&continue_rsqrt);
      // sqrtsd returns -0 when input is -0.  ECMA spec requires +0.
      __ xorps(double_exponent, double_exponent);
      __ addsd(double_exponent, double_base);  // Convert -0 to +0.
      __ sqrtsd(double_exponent, double_exponent);
      __ divsd(double_result, double_exponent);
      __ jmp(&done);
    }

    // Using FPU instructions to calculate power.
    Label fast_power_failed;
    __ bind(&fast_power);
    __ fnclex();  // Clear flags to catch exceptions later.
    // Transfer (B)ase and (E)xponent onto the FPU register stack.
    __ subq(rsp, Immediate(kDoubleSize));
    __ movsd(Operand(rsp, 0), double_exponent);
    __ fld_d(Operand(rsp, 0));  // E
    __ movsd(Operand(rsp, 0), double_base);
    __ fld_d(Operand(rsp, 0));  // B, E

    // Exponent is in st(1) and base is in st(0)
    // B ^ E = (2^(E * log2(B)) - 1) + 1 = (2^X - 1) + 1 for X = E * log2(B)
    // FYL2X calculates st(1) * log2(st(0))
    __ fyl2x();    // X
    __ fld(0);     // X, X
    __ frndint();  // rnd(X), X
    __ fsub(1);    // rnd(X), X-rnd(X)
    __ fxch(1);    // X - rnd(X), rnd(X)
    // F2XM1 calculates 2^st(0) - 1 for -1 < st(0) < 1
    __ f2xm1();    // 2^(X-rnd(X)) - 1, rnd(X)
    __ fld1();     // 1, 2^(X-rnd(X)) - 1, rnd(X)
    __ faddp(1);   // 1, 2^(X-rnd(X)), rnd(X)
    // FSCALE calculates st(0) * 2^st(1)
    __ fscale();   // 2^X, rnd(X)
    __ fstp(1);
    // Bail out to runtime in case of exceptions in the status word.
    __ fnstsw_ax();
    __ testb(rax, Immediate(0x5F));  // Check for all but precision exception.
    __ j(not_zero, &fast_power_failed, Label::kNear);
    __ fstp_d(Operand(rsp, 0));
    __ movsd(double_result, Operand(rsp, 0));
    __ addq(rsp, Immediate(kDoubleSize));
    __ jmp(&done);

    __ bind(&fast_power_failed);
    __ fninit();
    __ addq(rsp, Immediate(kDoubleSize));
    __ jmp(&call_runtime);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);
  const XMMRegister double_scratch2 = double_exponent;
  // Back up exponent as we need to check if exponent is negative later.
  __ movq(scratch, exponent);  // Back up exponent.
  __ movsd(double_scratch, double_base);  // Back up base.
  __ movsd(double_scratch2, double_result);  // Load double_exponent with 1.

  // Get absolute value of exponent.
  Label no_neg, while_true, no_multiply;
  __ testl(scratch, scratch);
  __ j(positive, &no_neg, Label::kNear);
  __ negl(scratch);
  __ bind(&no_neg);

  __ bind(&while_true);
  __ shrl(scratch, Immediate(1));
  __ j(not_carry, &no_multiply, Label::kNear);
  __ mulsd(double_result, double_scratch);
  __ bind(&no_multiply);

  __ mulsd(double_scratch, double_scratch);
  __ j(not_zero, &while_true);

  // If the exponent is negative, return 1/result.
  __ testl(exponent, exponent);
  __ j(greater, &done);
  __ divsd(double_scratch2, double_result);
  __ movsd(double_result, double_scratch2);
  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ xorps(double_scratch2, double_scratch2);
  __ ucomisd(double_scratch2, double_result);
  // double_exponent aliased as double_scratch2 has already been overwritten
  // and may not have contained the exponent value in the first place when the
  // input was a smi.  We reset it with exponent value before bailing out.
  __ j(not_equal, &done);
  __ cvtlsi2sd(double_exponent, exponent);

  // Returning or bailing out.
  Counters* counters = masm->isolate()->counters();
  if (exponent_type_ == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMath_pow_cfunction, 2, 1);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in eax.
    __ bind(&done);
    __ AllocateHeapNumber(rax, rcx, &call_runtime);
    __ movsd(FieldOperand(rax, HeapNumber::kValueOffset), double_result);
    __ IncrementCounter(counters->math_pow(), 1);
    __ ret(2 * kPointerSize);
  } else {
    __ bind(&call_runtime);
    // Move base to the correct argument register.  Exponent is already in xmm1.
    __ movsd(xmm0, double_base);
    ASSERT(double_exponent.is(xmm1));
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(2);
      __ CallCFunction(
          ExternalReference::power_double_double_function(masm->isolate()), 2);
    }
    // Return value is in xmm0.
    __ movsd(double_result, xmm0);
    // Restore context register.
    __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

    __ bind(&done);
    __ IncrementCounter(counters->math_pow(), 1);
    __ ret(0);
  }
}


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The key is in rdx and the parameter count is in rax.

  // The displacement is used for skipping the frame pointer on the
  // stack. It is the offset of the last parameter (if any) relative
  // to the frame pointer.
  static const int kDisplacement = 1 * kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(rdx, &slow);

  // Check if the calling frame is an arguments adaptor frame.  We look at the
  // context offset, and if the frame is not a regular one, then we find a
  // Smi instead of the context.  We can't use SmiCompare here, because that
  // only works for comparing two smis.
  Label adaptor;
  __ movq(rbx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ Cmp(Operand(rbx, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor);

  // Check index against formal parameters count limit passed in
  // through register rax. Use unsigned comparison to get negative
  // check for free.
  __ cmpq(rdx, rax);
  __ j(above_equal, &slow);

  // Read the argument from the stack and return it.
  SmiIndex index = masm->SmiToIndex(rax, rax, kPointerSizeLog2);
  __ lea(rbx, Operand(rbp, index.reg, index.scale, 0));
  index = masm->SmiToNegativeIndex(rdx, rdx, kPointerSizeLog2);
  __ movq(rax, Operand(rbx, index.reg, index.scale, kDisplacement));
  __ Ret();

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ movq(rcx, Operand(rbx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ cmpq(rdx, rcx);
  __ j(above_equal, &slow);

  // Read the argument from the stack and return it.
  index = masm->SmiToIndex(rax, rcx, kPointerSizeLog2);
  __ lea(rbx, Operand(rbx, index.reg, index.scale, 0));
  index = masm->SmiToNegativeIndex(rdx, rdx, kPointerSizeLog2);
  __ movq(rax, Operand(rbx, index.reg, index.scale, kDisplacement));
  __ Ret();

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ pop(rbx);  // Return address.
  __ push(rdx);
  __ push(rbx);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictFast(MacroAssembler* masm) {
  // Stack layout:
  //  rsp[0] : return address
  //  rsp[8] : number of parameters (tagged)
  //  rsp[16] : receiver displacement
  //  rsp[24] : function
  // Registers used over the whole function:
  //  rbx: the mapped parameter count (untagged)
  //  rax: the allocated object (tagged).

  Factory* factory = masm->isolate()->factory();

  __ SmiToInteger64(rbx, Operand(rsp, 1 * kPointerSize));
  // rbx = parameter count (untagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  Label adaptor_frame, try_allocate;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movq(rcx, Operand(rdx, StandardFrameConstants::kContextOffset));
  __ Cmp(rcx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor_frame);

  // No adaptor, parameter count = argument count.
  __ movq(rcx, rbx);
  __ jmp(&try_allocate, Label::kNear);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ SmiToInteger64(rcx,
                    Operand(rdx,
                            ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ lea(rdx, Operand(rdx, rcx, times_pointer_size,
                      StandardFrameConstants::kCallerSPOffset));
  __ movq(Operand(rsp, 2 * kPointerSize), rdx);

  // rbx = parameter count (untagged)
  // rcx = argument count (untagged)
  // Compute the mapped parameter count = min(rbx, rcx) in rbx.
  __ cmpq(rbx, rcx);
  __ j(less_equal, &try_allocate, Label::kNear);
  __ movq(rbx, rcx);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  Label no_parameter_map;
  __ xor_(r8, r8);
  __ testq(rbx, rbx);
  __ j(zero, &no_parameter_map, Label::kNear);
  __ lea(r8, Operand(rbx, times_pointer_size, kParameterMapHeaderSize));
  __ bind(&no_parameter_map);

  // 2. Backing store.
  __ lea(r8, Operand(r8, rcx, times_pointer_size, FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ addq(r8, Immediate(Heap::kArgumentsObjectSize));

  // Do the allocation of all three objects in one go.
  __ AllocateInNewSpace(r8, rax, rdx, rdi, &runtime, TAG_OBJECT);

  // rax = address of new object(s) (tagged)
  // rcx = argument count (untagged)
  // Get the arguments boilerplate from the current (global) context into rdi.
  Label has_mapped_parameters, copy;
  __ movq(rdi, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rdi, FieldOperand(rdi, GlobalObject::kGlobalContextOffset));
  __ testq(rbx, rbx);
  __ j(not_zero, &has_mapped_parameters, Label::kNear);

  const int kIndex = Context::ARGUMENTS_BOILERPLATE_INDEX;
  __ movq(rdi, Operand(rdi, Context::SlotOffset(kIndex)));
  __ jmp(&copy, Label::kNear);

  const int kAliasedIndex = Context::ALIASED_ARGUMENTS_BOILERPLATE_INDEX;
  __ bind(&has_mapped_parameters);
  __ movq(rdi, Operand(rdi, Context::SlotOffset(kAliasedIndex)));
  __ bind(&copy);

  // rax = address of new object (tagged)
  // rbx = mapped parameter count (untagged)
  // rcx = argument count (untagged)
  // rdi = address of boilerplate object (tagged)
  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ movq(rdx, FieldOperand(rdi, i));
    __ movq(FieldOperand(rax, i), rdx);
  }

  // Set up the callee in-object property.
  STATIC_ASSERT(Heap::kArgumentsCalleeIndex == 1);
  __ movq(rdx, Operand(rsp, 3 * kPointerSize));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize +
                       Heap::kArgumentsCalleeIndex * kPointerSize),
          rdx);

  // Use the length (smi tagged) and set that as an in-object property too.
  // Note: rcx is tagged from here on.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ Integer32ToSmi(rcx, rcx);
  __ movq(FieldOperand(rax, JSObject::kHeaderSize +
                       Heap::kArgumentsLengthIndex * kPointerSize),
          rcx);

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, edi will point there, otherwise to the
  // backing store.
  __ lea(rdi, Operand(rax, Heap::kArgumentsObjectSize));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rdi);

  // rax = address of new object (tagged)
  // rbx = mapped parameter count (untagged)
  // rcx = argument count (tagged)
  // rdi = address of parameter map or backing store (tagged)

  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map;
  __ testq(rbx, rbx);
  __ j(zero, &skip_parameter_map);

  __ LoadRoot(kScratchRegister, Heap::kNonStrictArgumentsElementsMapRootIndex);
  // rbx contains the untagged argument count. Add 2 and tag to write.
  __ movq(FieldOperand(rdi, FixedArray::kMapOffset), kScratchRegister);
  __ Integer64PlusConstantToSmi(r9, rbx, 2);
  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), r9);
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize + 0 * kPointerSize), rsi);
  __ lea(r9, Operand(rdi, rbx, times_pointer_size, kParameterMapHeaderSize));
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize + 1 * kPointerSize), r9);

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;

  // Load tagged parameter count into r9.
  __ Integer32ToSmi(r9, rbx);
  __ Move(r8, Smi::FromInt(Context::MIN_CONTEXT_SLOTS));
  __ addq(r8, Operand(rsp, 1 * kPointerSize));
  __ subq(r8, r9);
  __ Move(r11, factory->the_hole_value());
  __ movq(rdx, rdi);
  __ lea(rdi, Operand(rdi, rbx, times_pointer_size, kParameterMapHeaderSize));
  // r9 = loop variable (tagged)
  // r8 = mapping index (tagged)
  // r11 = the hole value
  // rdx = address of parameter map (tagged)
  // rdi = address of backing store (tagged)
  __ jmp(&parameters_test, Label::kNear);

  __ bind(&parameters_loop);
  __ SmiSubConstant(r9, r9, Smi::FromInt(1));
  __ SmiToInteger64(kScratchRegister, r9);
  __ movq(FieldOperand(rdx, kScratchRegister,
                       times_pointer_size,
                       kParameterMapHeaderSize),
          r8);
  __ movq(FieldOperand(rdi, kScratchRegister,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          r11);
  __ SmiAddConstant(r8, r8, Smi::FromInt(1));
  __ bind(&parameters_test);
  __ SmiTest(r9);
  __ j(not_zero, &parameters_loop, Label::kNear);

  __ bind(&skip_parameter_map);

  // rcx = argument count (tagged)
  // rdi = address of backing store (tagged)
  // Copy arguments header and remaining slots (if there are any).
  __ Move(FieldOperand(rdi, FixedArray::kMapOffset),
          factory->fixed_array_map());
  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), rcx);

  Label arguments_loop, arguments_test;
  __ movq(r8, rbx);
  __ movq(rdx, Operand(rsp, 2 * kPointerSize));
  // Untag rcx for the loop below.
  __ SmiToInteger64(rcx, rcx);
  __ lea(kScratchRegister, Operand(r8, times_pointer_size, 0));
  __ subq(rdx, kScratchRegister);
  __ jmp(&arguments_test, Label::kNear);

  __ bind(&arguments_loop);
  __ subq(rdx, Immediate(kPointerSize));
  __ movq(r9, Operand(rdx, 0));
  __ movq(FieldOperand(rdi, r8,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          r9);
  __ addq(r8, Immediate(1));

  __ bind(&arguments_test);
  __ cmpq(r8, rcx);
  __ j(less, &arguments_loop, Label::kNear);

  // Return and remove the on-stack parameters.
  __ ret(3 * kPointerSize);

  // Do the runtime call to allocate the arguments object.
  // rcx = argument count (untagged)
  __ bind(&runtime);
  __ Integer32ToSmi(rcx, rcx);
  __ movq(Operand(rsp, 1 * kPointerSize), rcx);  // Patch argument count.
  __ TailCallRuntime(Runtime::kNewStrictArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewNonStrictSlow(MacroAssembler* masm) {
  // esp[0] : return address
  // esp[8] : number of parameters
  // esp[16] : receiver displacement
  // esp[24] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movq(rcx, Operand(rdx, StandardFrameConstants::kContextOffset));
  __ Cmp(rcx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &runtime);

  // Patch the arguments.length and the parameters pointer.
  __ movq(rcx, Operand(rdx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ movq(Operand(rsp, 1 * kPointerSize), rcx);
  __ SmiToInteger64(rcx, rcx);
  __ lea(rdx, Operand(rdx, rcx, times_pointer_size,
              StandardFrameConstants::kCallerSPOffset));
  __ movq(Operand(rsp, 2 * kPointerSize), rdx);

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewArgumentsFast, 3, 1);
}


void ArgumentsAccessStub::GenerateNewStrict(MacroAssembler* masm) {
  // rsp[0] : return address
  // rsp[8] : number of parameters
  // rsp[16] : receiver displacement
  // rsp[24] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ movq(rdx, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ movq(rcx, Operand(rdx, StandardFrameConstants::kContextOffset));
  __ Cmp(rcx, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(equal, &adaptor_frame);

  // Get the length from the frame.
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));
  __ SmiToInteger64(rcx, rcx);
  __ jmp(&try_allocate);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ movq(rcx, Operand(rdx, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ movq(Operand(rsp, 1 * kPointerSize), rcx);
  __ SmiToInteger64(rcx, rcx);
  __ lea(rdx, Operand(rdx, rcx, times_pointer_size,
                      StandardFrameConstants::kCallerSPOffset));
  __ movq(Operand(rsp, 2 * kPointerSize), rdx);

  // Try the new space allocation. Start out with computing the size of
  // the arguments object and the elements array.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ testq(rcx, rcx);
  __ j(zero, &add_arguments_object, Label::kNear);
  __ lea(rcx, Operand(rcx, times_pointer_size, FixedArray::kHeaderSize));
  __ bind(&add_arguments_object);
  __ addq(rcx, Immediate(Heap::kArgumentsObjectSizeStrict));

  // Do the allocation of both objects in one go.
  __ AllocateInNewSpace(rcx, rax, rdx, rbx, &runtime, TAG_OBJECT);

  // Get the arguments boilerplate from the current (global) context.
  __ movq(rdi, Operand(rsi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ movq(rdi, FieldOperand(rdi, GlobalObject::kGlobalContextOffset));
  const int offset =
      Context::SlotOffset(Context::STRICT_MODE_ARGUMENTS_BOILERPLATE_INDEX);
  __ movq(rdi, Operand(rdi, offset));

  // Copy the JS object part.
  for (int i = 0; i < JSObject::kHeaderSize; i += kPointerSize) {
    __ movq(rbx, FieldOperand(rdi, i));
    __ movq(FieldOperand(rax, i), rbx);
  }

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ movq(rcx, Operand(rsp, 1 * kPointerSize));
  __ movq(FieldOperand(rax, JSObject::kHeaderSize +
                       Heap::kArgumentsLengthIndex * kPointerSize),
          rcx);

  // If there are no actual arguments, we're done.
  Label done;
  __ testq(rcx, rcx);
  __ j(zero, &done);

  // Get the parameters pointer from the stack.
  __ movq(rdx, Operand(rsp, 2 * kPointerSize));

  // Set up the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ lea(rdi, Operand(rax, Heap::kArgumentsObjectSizeStrict));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rdi);
  __ LoadRoot(kScratchRegister, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(rdi, FixedArray::kMapOffset), kScratchRegister);


  __ movq(FieldOperand(rdi, FixedArray::kLengthOffset), rcx);
  // Untag the length for the loop below.
  __ SmiToInteger64(rcx, rcx);

  // Copy the fixed array slots.
  Label loop;
  __ bind(&loop);
  __ movq(rbx, Operand(rdx, -1 * kPointerSize));  // Skip receiver.
  __ movq(FieldOperand(rdi, FixedArray::kHeaderSize), rbx);
  __ addq(rdi, Immediate(kPointerSize));
  __ subq(rdx, Immediate(kPointerSize));
  __ decq(rcx);
  __ j(not_zero, &loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ ret(3 * kPointerSize);

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

  // Stack frame on entry.
  //  rsp[0]: return address
  //  rsp[8]: last_match_info (expected JSArray)
  //  rsp[16]: previous index
  //  rsp[24]: subject string
  //  rsp[32]: JSRegExp object

  static const int kLastMatchInfoOffset = 1 * kPointerSize;
  static const int kPreviousIndexOffset = 2 * kPointerSize;
  static const int kSubjectOffset = 3 * kPointerSize;
  static const int kJSRegExpOffset = 4 * kPointerSize;

  Label runtime;
  // Ensure that a RegExp stack is allocated.
  Isolate* isolate = masm->isolate();
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate);
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate);
  __ Load(kScratchRegister, address_of_regexp_stack_memory_size);
  __ testq(kScratchRegister, kScratchRegister);
  __ j(zero, &runtime);

  // Check that the first argument is a JSRegExp object.
  __ movq(rax, Operand(rsp, kJSRegExpOffset));
  __ JumpIfSmi(rax, &runtime);
  __ CmpObjectType(rax, JS_REGEXP_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the RegExp has been compiled (data contains a fixed array).
  __ movq(rax, FieldOperand(rax, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    Condition is_smi = masm->CheckSmi(rax);
    __ Check(NegateCondition(is_smi),
        "Unexpected type for RegExp data, FixedArray expected");
    __ CmpObjectType(rax, FIXED_ARRAY_TYPE, kScratchRegister);
    __ Check(equal, "Unexpected type for RegExp data, FixedArray expected");
  }

  // rax: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ SmiToInteger32(rbx, FieldOperand(rax, JSRegExp::kDataTagOffset));
  __ cmpl(rbx, Immediate(JSRegExp::IRREGEXP));
  __ j(not_equal, &runtime);

  // rax: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ SmiToInteger32(rdx,
                    FieldOperand(rax, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ leal(rdx, Operand(rdx, rdx, times_1, 2));
  // Check that the static offsets vector buffer is large enough.
  __ cmpl(rdx, Immediate(OffsetsVector::kStaticOffsetsVectorSize));
  __ j(above, &runtime);

  // rax: RegExp data (FixedArray)
  // rdx: Number of capture registers
  // Check that the second argument is a string.
  __ movq(rdi, Operand(rsp, kSubjectOffset));
  __ JumpIfSmi(rdi, &runtime);
  Condition is_string = masm->IsObjectStringType(rdi, rbx, rbx);
  __ j(NegateCondition(is_string), &runtime);

  // rdi: Subject string.
  // rax: RegExp data (FixedArray).
  // rdx: Number of capture registers.
  // Check that the third argument is a positive smi less than the string
  // length. A negative value will be greater (unsigned comparison).
  __ movq(rbx, Operand(rsp, kPreviousIndexOffset));
  __ JumpIfNotSmi(rbx, &runtime);
  __ SmiCompare(rbx, FieldOperand(rdi, String::kLengthOffset));
  __ j(above_equal, &runtime);

  // rax: RegExp data (FixedArray)
  // rdx: Number of capture registers
  // Check that the fourth object is a JSArray object.
  __ movq(rdi, Operand(rsp, kLastMatchInfoOffset));
  __ JumpIfSmi(rdi, &runtime);
  __ CmpObjectType(rdi, JS_ARRAY_TYPE, kScratchRegister);
  __ j(not_equal, &runtime);
  // Check that the JSArray is in fast case.
  __ movq(rbx, FieldOperand(rdi, JSArray::kElementsOffset));
  __ movq(rdi, FieldOperand(rbx, HeapObject::kMapOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information. Ensure no overflow in add.
  STATIC_ASSERT(FixedArray::kMaxLength < kMaxInt - FixedArray::kLengthOffset);
  __ SmiToInteger32(rdi, FieldOperand(rbx, FixedArray::kLengthOffset));
  __ addl(rdx, Immediate(RegExpImpl::kLastMatchOverhead));
  __ cmpl(rdx, rdi);
  __ j(greater, &runtime);

  // Reset offset for possibly sliced string.
  __ Set(r14, 0);
  // rax: RegExp data (FixedArray)
  // Check the representation and encoding of the subject string.
  Label seq_ascii_string, seq_two_byte_string, check_code;
  __ movq(rdi, Operand(rsp, kSubjectOffset));
  // Make a copy of the original subject string.
  __ movq(r15, rdi);
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  // First check for flat two byte string.
  __ andb(rbx, Immediate(kIsNotStringMask |
                         kStringRepresentationMask |
                         kStringEncodingMask |
                         kShortExternalStringMask));
  STATIC_ASSERT((kStringTag | kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string, Label::kNear);
  // Any other flat string must be a flat ASCII string.  None of the following
  // string type tests will succeed if subject is not a string or a short
  // external string.
  __ andb(rbx, Immediate(kIsNotStringMask |
                         kStringRepresentationMask |
                         kShortExternalStringMask));
  __ j(zero, &seq_ascii_string, Label::kNear);

  // rbx: whether subject is a string and if yes, its string representation
  // Check for flat cons string or sliced string.
  // A flat cons string is a cons string where the second part is the empty
  // string. In that case the subject string is just the first part of the cons
  // string. Also in this case the first part of the cons string is known to be
  // a sequential string or an external string.
  // In the case of a sliced string its offset has to be taken into account.
  Label cons_string, external_string, check_encoding;
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  __ cmpq(rbx, Immediate(kExternalStringTag));
  __ j(less, &cons_string, Label::kNear);
  __ j(equal, &external_string);

  // Catch non-string subject or short external string.
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ testb(rbx, Immediate(kIsNotStringMask | kShortExternalStringMask));
  __ j(not_zero, &runtime);

  // String is sliced.
  __ SmiToInteger32(r14, FieldOperand(rdi, SlicedString::kOffsetOffset));
  __ movq(rdi, FieldOperand(rdi, SlicedString::kParentOffset));
  // r14: slice offset
  // r15: original subject string
  // rdi: parent string
  __ jmp(&check_encoding, Label::kNear);
  // String is a cons string, check whether it is flat.
  __ bind(&cons_string);
  __ CompareRoot(FieldOperand(rdi, ConsString::kSecondOffset),
                 Heap::kEmptyStringRootIndex);
  __ j(not_equal, &runtime);
  __ movq(rdi, FieldOperand(rdi, ConsString::kFirstOffset));
  // rdi: first part of cons string or parent of sliced string.
  // rbx: map of first part of cons string or map of parent of sliced string.
  // Is first part of cons or parent of slice a flat two byte string?
  __ bind(&check_encoding);
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rbx, Map::kInstanceTypeOffset),
           Immediate(kStringRepresentationMask | kStringEncodingMask));
  STATIC_ASSERT((kSeqStringTag | kTwoByteStringTag) == 0);
  __ j(zero, &seq_two_byte_string, Label::kNear);
  // Any other flat string must be sequential ASCII or external.
  __ testb(FieldOperand(rbx, Map::kInstanceTypeOffset),
           Immediate(kStringRepresentationMask));
  __ j(not_zero, &external_string);

  __ bind(&seq_ascii_string);
  // rdi: subject string (sequential ASCII)
  // rax: RegExp data (FixedArray)
  __ movq(r11, FieldOperand(rax, JSRegExp::kDataAsciiCodeOffset));
  __ Set(rcx, 1);  // Type is ASCII.
  __ jmp(&check_code, Label::kNear);

  __ bind(&seq_two_byte_string);
  // rdi: subject string (flat two-byte)
  // rax: RegExp data (FixedArray)
  __ movq(r11, FieldOperand(rax, JSRegExp::kDataUC16CodeOffset));
  __ Set(rcx, 0);  // Type is two byte.

  __ bind(&check_code);
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // smi (code flushing support)
  __ JumpIfSmi(r11, &runtime);

  // rdi: subject string
  // rcx: encoding of subject string (1 if ASCII, 0 if two_byte);
  // r11: code
  // Load used arguments before starting to push arguments for call to native
  // RegExp code to avoid handling changing stack height.
  __ SmiToInteger64(rbx, Operand(rsp, kPreviousIndexOffset));

  // rdi: subject string
  // rbx: previous index
  // rcx: encoding of subject string (1 if ASCII 0 if two_byte);
  // r11: code
  // All checks done. Now push arguments for native regexp code.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->regexp_entry_native(), 1);

  // Isolates: note we add an additional parameter here (isolate pointer).
  static const int kRegExpExecuteArguments = 8;
  int argument_slots_on_stack =
      masm->ArgumentStackSlotsForCFunctionCall(kRegExpExecuteArguments);
  __ EnterApiExitFrame(argument_slots_on_stack);

  // Argument 8: Pass current isolate address.
  // __ movq(Operand(rsp, (argument_slots_on_stack - 1) * kPointerSize),
  //     Immediate(ExternalReference::isolate_address()));
  __ LoadAddress(kScratchRegister, ExternalReference::isolate_address());
  __ movq(Operand(rsp, (argument_slots_on_stack - 1) * kPointerSize),
          kScratchRegister);

  // Argument 7: Indicate that this is a direct call from JavaScript.
  __ movq(Operand(rsp, (argument_slots_on_stack - 2) * kPointerSize),
          Immediate(1));

  // Argument 6: Start (high end) of backtracking stack memory area.
  __ movq(kScratchRegister, address_of_regexp_stack_memory_address);
  __ movq(r9, Operand(kScratchRegister, 0));
  __ movq(kScratchRegister, address_of_regexp_stack_memory_size);
  __ addq(r9, Operand(kScratchRegister, 0));
  // Argument 6 passed in r9 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 3) * kPointerSize), r9);
#endif

  // Argument 5: static offsets vector buffer.
  __ LoadAddress(r8,
                 ExternalReference::address_of_static_offsets_vector(isolate));
  // Argument 5 passed in r8 on Linux and on the stack on Windows.
#ifdef _WIN64
  __ movq(Operand(rsp, (argument_slots_on_stack - 4) * kPointerSize), r8);
#endif

  // First four arguments are passed in registers on both Linux and Windows.
#ifdef _WIN64
  Register arg4 = r9;
  Register arg3 = r8;
  Register arg2 = rdx;
  Register arg1 = rcx;
#else
  Register arg4 = rcx;
  Register arg3 = rdx;
  Register arg2 = rsi;
  Register arg1 = rdi;
#endif

  // Keep track on aliasing between argX defined above and the registers used.
  // rdi: subject string
  // rbx: previous index
  // rcx: encoding of subject string (1 if ASCII 0 if two_byte);
  // r11: code
  // r14: slice offset
  // r15: original subject string

  // Argument 2: Previous index.
  __ movq(arg2, rbx);

  // Argument 4: End of string data
  // Argument 3: Start of string data
  Label setup_two_byte, setup_rest, got_length, length_not_from_slice;
  // Prepare start and end index of the input.
  // Load the length from the original sliced string if that is the case.
  __ addq(rbx, r14);
  __ SmiToInteger32(arg3, FieldOperand(r15, String::kLengthOffset));
  __ addq(r14, arg3);  // Using arg3 as scratch.

  // rbx: start index of the input
  // r14: end index of the input
  // r15: original subject string
  __ testb(rcx, rcx);  // Last use of rcx as encoding of subject string.
  __ j(zero, &setup_two_byte, Label::kNear);
  __ lea(arg4, FieldOperand(rdi, r14, times_1, SeqAsciiString::kHeaderSize));
  __ lea(arg3, FieldOperand(rdi, rbx, times_1, SeqAsciiString::kHeaderSize));
  __ jmp(&setup_rest, Label::kNear);
  __ bind(&setup_two_byte);
  __ lea(arg4, FieldOperand(rdi, r14, times_2, SeqTwoByteString::kHeaderSize));
  __ lea(arg3, FieldOperand(rdi, rbx, times_2, SeqTwoByteString::kHeaderSize));
  __ bind(&setup_rest);

  // Argument 1: Original subject string.
  // The original subject is in the previous stack frame. Therefore we have to
  // use rbp, which points exactly to one pointer size below the previous rsp.
  // (Because creating a new stack frame pushes the previous rbp onto the stack
  // and thereby moves up rsp by one kPointerSize.)
  __ movq(arg1, r15);

  // Locate the code entry and call it.
  __ addq(r11, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(r11);

  __ LeaveApiExitFrame();

  // Check the result.
  Label success;
  Label exception;
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::SUCCESS));
  __ j(equal, &success, Label::kNear);
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::EXCEPTION));
  __ j(equal, &exception);
  __ cmpl(rax, Immediate(NativeRegExpMacroAssembler::FAILURE));
  // If none of the above, it can only be retry.
  // Handle that in the runtime system.
  __ j(not_equal, &runtime);

  // For failure return null.
  __ LoadRoot(rax, Heap::kNullValueRootIndex);
  __ ret(4 * kPointerSize);

  // Load RegExp data.
  __ bind(&success);
  __ movq(rax, Operand(rsp, kJSRegExpOffset));
  __ movq(rcx, FieldOperand(rax, JSRegExp::kDataOffset));
  __ SmiToInteger32(rax,
                    FieldOperand(rcx, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  __ leal(rdx, Operand(rax, rax, times_1, 2));

  // rdx: Number of capture registers
  // Load last_match_info which is still known to be a fast case JSArray.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ movq(rbx, FieldOperand(rax, JSArray::kElementsOffset));

  // rbx: last_match_info backing store (FixedArray)
  // rdx: number of capture registers
  // Store the capture count.
  __ Integer32ToSmi(kScratchRegister, rdx);
  __ movq(FieldOperand(rbx, RegExpImpl::kLastCaptureCountOffset),
          kScratchRegister);
  // Store last subject and last input.
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(FieldOperand(rbx, RegExpImpl::kLastSubjectOffset), rax);
  __ RecordWriteField(rbx,
                      RegExpImpl::kLastSubjectOffset,
                      rax,
                      rdi,
                      kDontSaveFPRegs);
  __ movq(rax, Operand(rsp, kSubjectOffset));
  __ movq(FieldOperand(rbx, RegExpImpl::kLastInputOffset), rax);
  __ RecordWriteField(rbx,
                      RegExpImpl::kLastInputOffset,
                      rax,
                      rdi,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  __ LoadAddress(rcx,
                 ExternalReference::address_of_static_offsets_vector(isolate));

  // rbx: last_match_info backing store (FixedArray)
  // rcx: offsets vector
  // rdx: number of capture registers
  Label next_capture, done;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ bind(&next_capture);
  __ subq(rdx, Immediate(1));
  __ j(negative, &done, Label::kNear);
  // Read the value from the static offsets vector buffer and make it a smi.
  __ movl(rdi, Operand(rcx, rdx, times_int_size, 0));
  __ Integer32ToSmi(rdi, rdi);
  // Store the smi value in the last match info.
  __ movq(FieldOperand(rbx,
                       rdx,
                       times_pointer_size,
                       RegExpImpl::kFirstCaptureOffset),
          rdi);
  __ jmp(&next_capture);
  __ bind(&done);

  // Return last match info.
  __ movq(rax, Operand(rsp, kLastMatchInfoOffset));
  __ ret(4 * kPointerSize);

  __ bind(&exception);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, isolate);
  Operand pending_exception_operand =
      masm->ExternalOperand(pending_exception_address, rbx);
  __ movq(rax, pending_exception_operand);
  __ LoadRoot(rdx, Heap::kTheHoleValueRootIndex);
  __ cmpq(rax, rdx);
  __ j(equal, &runtime);
  __ movq(pending_exception_operand, rdx);

  __ CompareRoot(rax, Heap::kTerminationExceptionRootIndex);
  Label termination_exception;
  __ j(equal, &termination_exception, Label::kNear);
  __ Throw(rax);

  __ bind(&termination_exception);
  __ ThrowUncatchable(rax);

  // External string.  Short external strings have already been ruled out.
  // rdi: subject string (expected to be external)
  // rbx: scratch
  __ bind(&external_string);
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ testb(rbx, Immediate(kIsIndirectStringMask));
    __ Assert(zero, "external string expected, but not found");
  }
  __ movq(rdi, FieldOperand(rdi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqAsciiString::kHeaderSize);
  __ subq(rdi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ testb(rbx, Immediate(kStringEncodingMask));
  __ j(not_zero, &seq_ascii_string);
  __ jmp(&seq_two_byte_string);

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kRegExpExec, 4, 1);
#endif  // V8_INTERPRETED_REGEXP
}


void RegExpConstructResultStub::Generate(MacroAssembler* masm) {
  const int kMaxInlineLength = 100;
  Label slowcase;
  Label done;
  __ movq(r8, Operand(rsp, kPointerSize * 3));
  __ JumpIfNotSmi(r8, &slowcase);
  __ SmiToInteger32(rbx, r8);
  __ cmpl(rbx, Immediate(kMaxInlineLength));
  __ j(above, &slowcase);
  // Smi-tagging is equivalent to multiplying by 2.
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  // Allocate RegExpResult followed by FixedArray with size in rbx.
  // JSArray:   [Map][empty properties][Elements][Length-smi][index][input]
  // Elements:  [Map][Length][..elements..]
  __ AllocateInNewSpace(JSRegExpResult::kSize + FixedArray::kHeaderSize,
                        times_pointer_size,
                        rbx,  // In: Number of elements.
                        rax,  // Out: Start of allocation (tagged).
                        rcx,  // Out: End of allocation.
                        rdx,  // Scratch register
                        &slowcase,
                        TAG_OBJECT);
  // rax: Start of allocated area, object-tagged.
  // rbx: Number of array elements as int32.
  // r8: Number of array elements as smi.

  // Set JSArray map to global.regexp_result_map().
  __ movq(rdx, ContextOperand(rsi, Context::GLOBAL_INDEX));
  __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalContextOffset));
  __ movq(rdx, ContextOperand(rdx, Context::REGEXP_RESULT_MAP_INDEX));
  __ movq(FieldOperand(rax, HeapObject::kMapOffset), rdx);

  // Set empty properties FixedArray.
  __ LoadRoot(kScratchRegister, Heap::kEmptyFixedArrayRootIndex);
  __ movq(FieldOperand(rax, JSObject::kPropertiesOffset), kScratchRegister);

  // Set elements to point to FixedArray allocated right after the JSArray.
  __ lea(rcx, Operand(rax, JSRegExpResult::kSize));
  __ movq(FieldOperand(rax, JSObject::kElementsOffset), rcx);

  // Set input, index and length fields from arguments.
  __ movq(r8, Operand(rsp, kPointerSize * 1));
  __ movq(FieldOperand(rax, JSRegExpResult::kInputOffset), r8);
  __ movq(r8, Operand(rsp, kPointerSize * 2));
  __ movq(FieldOperand(rax, JSRegExpResult::kIndexOffset), r8);
  __ movq(r8, Operand(rsp, kPointerSize * 3));
  __ movq(FieldOperand(rax, JSArray::kLengthOffset), r8);

  // Fill out the elements FixedArray.
  // rax: JSArray.
  // rcx: FixedArray.
  // rbx: Number of elements in array as int32.

  // Set map.
  __ LoadRoot(kScratchRegister, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(rcx, HeapObject::kMapOffset), kScratchRegister);
  // Set length.
  __ Integer32ToSmi(rdx, rbx);
  __ movq(FieldOperand(rcx, FixedArray::kLengthOffset), rdx);
  // Fill contents of fixed-array with the-hole.
  __ LoadRoot(rdx, Heap::kTheHoleValueRootIndex);
  __ lea(rcx, FieldOperand(rcx, FixedArray::kHeaderSize));
  // Fill fixed array elements with hole.
  // rax: JSArray.
  // rbx: Number of elements in array that remains to be filled, as int32.
  // rcx: Start of elements in FixedArray.
  // rdx: the hole.
  Label loop;
  __ testl(rbx, rbx);
  __ bind(&loop);
  __ j(less_equal, &done);  // Jump if rcx is negative or zero.
  __ subl(rbx, Immediate(1));
  __ movq(Operand(rcx, rbx, times_pointer_size, 0), rdx);
  __ jmp(&loop);

  __ bind(&done);
  __ ret(3 * kPointerSize);

  __ bind(&slowcase);
  __ TailCallRuntime(Runtime::kRegExpConstructResult, 3, 1);
}


void NumberToStringStub::GenerateLookupNumberStringCache(MacroAssembler* masm,
                                                         Register object,
                                                         Register result,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         bool object_is_smi,
                                                         Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch1;
  Register scratch = scratch2;

  // Load the number string cache.
  __ LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  __ SmiToInteger32(
      mask, FieldOperand(number_string_cache, FixedArray::kLengthOffset));
  __ shrl(mask, Immediate(1));
  __ subq(mask, Immediate(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  Factory* factory = masm->isolate()->factory();
  if (!object_is_smi) {
    __ JumpIfSmi(object, &is_smi);
    __ CheckMap(object,
                factory->heap_number_map(),
                not_found,
                DONT_DO_SMI_CHECK);

    STATIC_ASSERT(8 == kDoubleSize);
    __ movl(scratch, FieldOperand(object, HeapNumber::kValueOffset + 4));
    __ xor_(scratch, FieldOperand(object, HeapNumber::kValueOffset));
    GenerateConvertHashCodeToIndex(masm, scratch, mask);

    Register index = scratch;
    Register probe = mask;
    __ movq(probe,
            FieldOperand(number_string_cache,
                         index,
                         times_1,
                         FixedArray::kHeaderSize));
    __ JumpIfSmi(probe, not_found);
    __ movsd(xmm0, FieldOperand(object, HeapNumber::kValueOffset));
    __ movsd(xmm1, FieldOperand(probe, HeapNumber::kValueOffset));
    __ ucomisd(xmm0, xmm1);
    __ j(parity_even, not_found);  // Bail out if NaN is involved.
    __ j(not_equal, not_found);  // The cache did not contain this value.
    __ jmp(&load_result_from_cache);
  }

  __ bind(&is_smi);
  __ SmiToInteger32(scratch, object);
  GenerateConvertHashCodeToIndex(masm, scratch, mask);

  Register index = scratch;
  // Check if the entry is the smi we are looking for.
  __ cmpq(object,
          FieldOperand(number_string_cache,
                       index,
                       times_1,
                       FixedArray::kHeaderSize));
  __ j(not_equal, not_found);

  // Get the result from the cache.
  __ bind(&load_result_from_cache);
  __ movq(result,
          FieldOperand(number_string_cache,
                       index,
                       times_1,
                       FixedArray::kHeaderSize + kPointerSize));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->number_to_string_native(), 1);
}


void NumberToStringStub::GenerateConvertHashCodeToIndex(MacroAssembler* masm,
                                                        Register hash,
                                                        Register mask) {
  __ and_(hash, mask);
  // Each entry in string cache consists of two pointer sized fields,
  // but times_twice_pointer_size (multiplication by 16) scale factor
  // is not supported by addrmode on x64 platform.
  // So we have to premultiply entry index before lookup.
  __ shl(hash, Immediate(kPointerSizeLog2 + 1));
}


void NumberToStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  __ movq(rbx, Operand(rsp, kPointerSize));

  // Generate code to lookup number in the number string cache.
  GenerateLookupNumberStringCache(masm, rbx, rax, r8, r9, false, &runtime);
  __ ret(1 * kPointerSize);

  __ bind(&runtime);
  // Handle number to string in the runtime system if not found in the cache.
  __ TailCallRuntime(Runtime::kNumberToStringSkipCache, 1, 1);
}


static int NegativeComparisonResult(Condition cc) {
  ASSERT(cc != equal);
  ASSERT((cc == less) || (cc == less_equal)
      || (cc == greater) || (cc == greater_equal));
  return (cc == greater || cc == greater_equal) ? LESS : GREATER;
}


void CompareStub::Generate(MacroAssembler* masm) {
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));

  Label check_unequal_objects, done;
  Factory* factory = masm->isolate()->factory();

  // Compare two smis if required.
  if (include_smi_compare_) {
    Label non_smi, smi_done;
    __ JumpIfNotBothSmi(rax, rdx, &non_smi);
    __ subq(rdx, rax);
    __ j(no_overflow, &smi_done);
    __ not_(rdx);  // Correct sign in case of overflow. rdx cannot be 0 here.
    __ bind(&smi_done);
    __ movq(rax, rdx);
    __ ret(0);
    __ bind(&non_smi);
  } else if (FLAG_debug_code) {
    Label ok;
    __ JumpIfNotSmi(rdx, &ok);
    __ JumpIfNotSmi(rax, &ok);
    __ Abort("CompareStub: smi operands");
    __ bind(&ok);
  }

  // The compare stub returns a positive, negative, or zero 64-bit integer
  // value in rax, corresponding to result of comparing the two inputs.
  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Two identical objects are equal unless they are both NaN or undefined.
  {
    Label not_identical;
    __ cmpq(rax, rdx);
    __ j(not_equal, &not_identical, Label::kNear);

    if (cc_ != equal) {
      // Check for undefined.  undefined OP undefined is false even though
      // undefined == undefined.
      Label check_for_nan;
      __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
      __ j(not_equal, &check_for_nan, Label::kNear);
      __ Set(rax, NegativeComparisonResult(cc_));
      __ ret(0);
      __ bind(&check_for_nan);
    }

    // Test for NaN. Sadly, we can't just compare to FACTORY->nan_value(),
    // so we do the second best thing - test it ourselves.
    // Note: if cc_ != equal, never_nan_nan_ is not used.
    // We cannot set rax to EQUAL until just before return because
    // rax must be unchanged on jump to not_identical.
    if (never_nan_nan_ && (cc_ == equal)) {
      __ Set(rax, EQUAL);
      __ ret(0);
    } else {
      Label heap_number;
      // If it's not a heap number, then return equal for (in)equality operator.
      __ Cmp(FieldOperand(rdx, HeapObject::kMapOffset),
             factory->heap_number_map());
      __ j(equal, &heap_number, Label::kNear);
      if (cc_ != equal) {
        // Call runtime on identical objects.  Otherwise return equal.
        __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
        __ j(above_equal, &not_identical, Label::kNear);
      }
      __ Set(rax, EQUAL);
      __ ret(0);

      __ bind(&heap_number);
      // It is a heap number, so return  equal if it's not NaN.
      // For NaN, return 1 for every condition except greater and
      // greater-equal.  Return -1 for them, so the comparison yields
      // false for all conditions except not-equal.
      __ Set(rax, EQUAL);
      __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
      __ ucomisd(xmm0, xmm0);
      __ setcc(parity_even, rax);
      // rax is 0 for equal non-NaN heapnumbers, 1 for NaNs.
      if (cc_ == greater_equal || cc_ == greater) {
        __ neg(rax);
      }
      __ ret(0);
    }

    __ bind(&not_identical);
  }

  if (cc_ == equal) {  // Both strict and non-strict.
    Label slow;  // Fallthrough label.

    // If we're doing a strict equality comparison, we don't have to do
    // type conversion, so we generate code to do fast comparison for objects
    // and oddballs. Non-smi numbers and strings still go through the usual
    // slow-case code.
    if (strict_) {
      // If either is a Smi (we know that not both are), then they can only
      // be equal if the other is a HeapNumber. If so, use the slow case.
      {
        Label not_smis;
        __ SelectNonSmi(rbx, rax, rdx, &not_smis);

        // Check if the non-smi operand is a heap number.
        __ Cmp(FieldOperand(rbx, HeapObject::kMapOffset),
               factory->heap_number_map());
        // If heap number, handle it in the slow case.
        __ j(equal, &slow);
        // Return non-equal.  ebx (the lower half of rbx) is not zero.
        __ movq(rax, rbx);
        __ ret(0);

        __ bind(&not_smis);
      }

      // If either operand is a JSObject or an oddball value, then they are not
      // equal since their pointers are different
      // There is no test for undetectability in strict equality.

      // If the first object is a JS object, we have done pointer comparison.
      STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);
      Label first_non_object;
      __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
      __ j(below, &first_non_object, Label::kNear);
      // Return non-zero (eax (not rax) is not zero)
      Label return_not_equal;
      STATIC_ASSERT(kHeapObjectTag != 0);
      __ bind(&return_not_equal);
      __ ret(0);

      __ bind(&first_non_object);
      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      __ CmpObjectType(rdx, FIRST_SPEC_OBJECT_TYPE, rcx);
      __ j(above_equal, &return_not_equal);

      // Check for oddballs: true, false, null, undefined.
      __ CmpInstanceType(rcx, ODDBALL_TYPE);
      __ j(equal, &return_not_equal);

      // Fall through to the general case.
    }
    __ bind(&slow);
  }

  // Generate the number comparison code.
  if (include_number_compare_) {
    Label non_number_comparison;
    Label unordered;
    FloatingPointHelper::LoadSSE2UnknownOperands(masm, &non_number_comparison);
    __ xorl(rax, rax);
    __ xorl(rcx, rcx);
    __ ucomisd(xmm0, xmm1);

    // Don't base result on EFLAGS when a NaN is involved.
    __ j(parity_even, &unordered, Label::kNear);
    // Return a result of -1, 0, or 1, based on EFLAGS.
    __ setcc(above, rax);
    __ setcc(below, rcx);
    __ subq(rax, rcx);
    __ ret(0);

    // If one of the numbers was NaN, then the result is always false.
    // The cc is never not-equal.
    __ bind(&unordered);
    ASSERT(cc_ != not_equal);
    if (cc_ == less || cc_ == less_equal) {
      __ Set(rax, 1);
    } else {
      __ Set(rax, -1);
    }
    __ ret(0);

    // The number comparison code did not provide a valid result.
    __ bind(&non_number_comparison);
  }

  // Fast negative check for symbol-to-symbol equality.
  Label check_for_strings;
  if (cc_ == equal) {
    BranchIfNonSymbol(masm, &check_for_strings, rax, kScratchRegister);
    BranchIfNonSymbol(masm, &check_for_strings, rdx, kScratchRegister);

    // We've already checked for object identity, so if both operands
    // are symbols they aren't equal. Register eax (not rax) already holds a
    // non-zero value, which indicates not equal, so just return.
    __ ret(0);
  }

  __ bind(&check_for_strings);

  __ JumpIfNotBothSequentialAsciiStrings(
      rdx, rax, rcx, rbx, &check_unequal_objects);

  // Inline comparison of ASCII strings.
  if (cc_ == equal) {
    StringCompareStub::GenerateFlatAsciiStringEquals(masm,
                                                     rdx,
                                                     rax,
                                                     rcx,
                                                     rbx);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                       rdx,
                                                       rax,
                                                       rcx,
                                                       rbx,
                                                       rdi,
                                                       r8);
  }

#ifdef DEBUG
  __ Abort("Unexpected fall-through from string comparison");
#endif

  __ bind(&check_unequal_objects);
  if (cc_ == equal && !strict_) {
    // Not strict equality.  Objects are unequal if
    // they are both JSObjects and not undetectable,
    // and their pointers are different.
    Label not_both_objects, return_unequal;
    // At most one is a smi, so we can test for smi by adding the two.
    // A smi plus a heap object has the low bit set, a heap object plus
    // a heap object has the low bit clear.
    STATIC_ASSERT(kSmiTag == 0);
    STATIC_ASSERT(kSmiTagMask == 1);
    __ lea(rcx, Operand(rax, rdx, times_1, 0));
    __ testb(rcx, Immediate(kSmiTagMask));
    __ j(not_zero, &not_both_objects, Label::kNear);
    __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rbx);
    __ j(below, &not_both_objects, Label::kNear);
    __ CmpObjectType(rdx, FIRST_SPEC_OBJECT_TYPE, rcx);
    __ j(below, &not_both_objects, Label::kNear);
    __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal, Label::kNear);
    __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
             Immediate(1 << Map::kIsUndetectable));
    __ j(zero, &return_unequal, Label::kNear);
    // The objects are both undetectable, so they both compare as the value
    // undefined, and are equal.
    __ Set(rax, EQUAL);
    __ bind(&return_unequal);
    // Return non-equal by returning the non-zero object pointer in rax,
    // or return equal if we fell through to here.
    __ ret(0);
    __ bind(&not_both_objects);
  }

  // Push arguments below the return address to prepare jump to builtin.
  __ pop(rcx);
  __ push(rdx);
  __ push(rax);

  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript builtin;
  if (cc_ == equal) {
    builtin = strict_ ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    builtin = Builtins::COMPARE;
    __ Push(Smi::FromInt(NegativeComparisonResult(cc_)));
  }

  // Restore return address on the stack.
  __ push(rcx);

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(builtin, JUMP_FUNCTION);
}


void CompareStub::BranchIfNonSymbol(MacroAssembler* masm,
                                    Label* label,
                                    Register object,
                                    Register scratch) {
  __ JumpIfSmi(object, label);
  __ movq(scratch, FieldOperand(object, HeapObject::kMapOffset));
  __ movzxbq(scratch,
             FieldOperand(scratch, Map::kInstanceTypeOffset));
  // Ensure that no non-strings have the symbol bit set.
  STATIC_ASSERT(LAST_TYPE < kNotStringTag + kIsSymbolMask);
  STATIC_ASSERT(kSymbolTag != 0);
  __ testb(scratch, Immediate(kIsSymbolMask));
  __ j(zero, label);
}


void StackCheckStub::Generate(MacroAssembler* masm) {
  __ TailCallRuntime(Runtime::kStackGuard, 0, 1);
}


void InterruptStub::Generate(MacroAssembler* masm) {
  __ TailCallRuntime(Runtime::kInterrupt, 0, 1);
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a global property cell.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // rbx : cache cell for call target
  // rdi : the function to call
  Isolate* isolate = masm->isolate();
  Label initialize, done;

  // Load the cache state into rcx.
  __ movq(rcx, FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  __ cmpq(rcx, rdi);
  __ j(equal, &done, Label::kNear);
  __ Cmp(rcx, TypeFeedbackCells::MegamorphicSentinel(isolate));
  __ j(equal, &done, Label::kNear);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ Cmp(rcx, TypeFeedbackCells::UninitializedSentinel(isolate));
  __ j(equal, &initialize, Label::kNear);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ Move(FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset),
          TypeFeedbackCells::MegamorphicSentinel(isolate));
  __ jmp(&done, Label::kNear);

  // An uninitialized cache is patched with the function.
  __ bind(&initialize);
  __ movq(FieldOperand(rbx, JSGlobalPropertyCell::kValueOffset), rdi);
  // No need for a write barrier here - cells are rescanned.

  __ bind(&done);
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  // rdi : the function to call
  // rbx : cache cell for call target
  Label slow, non_function;

  // The receiver might implicitly be the global object. This is
  // indicated by passing the hole as the receiver to the call
  // function stub.
  if (ReceiverMightBeImplicit()) {
    Label call;
    // Get the receiver from the stack.
    // +1 ~ return address
    __ movq(rax, Operand(rsp, (argc_ + 1) * kPointerSize));
    // Call as function is indicated with the hole.
    __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
    __ j(not_equal, &call, Label::kNear);
    // Patch the receiver on the stack with the global receiver object.
    __ movq(rbx, GlobalObjectOperand());
    __ movq(rbx, FieldOperand(rbx, GlobalObject::kGlobalReceiverOffset));
    __ movq(Operand(rsp, (argc_ + 1) * kPointerSize), rbx);
    __ bind(&call);
  }

  // Check that the function really is a JavaScript function.
  __ JumpIfSmi(rdi, &non_function);
  // Goto slow case if we do not have a function.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &slow);

  // Fast-case: Just invoke the function.
  ParameterCount actual(argc_);

  if (ReceiverMightBeImplicit()) {
    Label call_as_function;
    __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
    __ j(equal, &call_as_function);
    __ InvokeFunction(rdi,
                      actual,
                      JUMP_FUNCTION,
                      NullCallWrapper(),
                      CALL_AS_METHOD);
    __ bind(&call_as_function);
  }
  __ InvokeFunction(rdi,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper(),
                    CALL_AS_FUNCTION);

  // Slow-case: Non-function called.
  __ bind(&slow);
  // Check for function proxy.
  __ CmpInstanceType(rcx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function);
  __ pop(rcx);
  __ push(rdi);  // put proxy as additional argument under return address
  __ push(rcx);
  __ Set(rax, argc_ + 1);
  __ Set(rbx, 0);
  __ SetCallKind(rcx, CALL_AS_METHOD);
  __ GetBuiltinEntry(rdx, Builtins::CALL_FUNCTION_PROXY);
  {
    Handle<Code> adaptor =
      masm->isolate()->builtins()->ArgumentsAdaptorTrampoline();
    __ jmp(adaptor, RelocInfo::CODE_TARGET);
  }

  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ bind(&non_function);
  __ movq(Operand(rsp, (argc_ + 1) * kPointerSize), rdi);
  __ Set(rax, argc_);
  __ Set(rbx, 0);
  __ SetCallKind(rcx, CALL_AS_METHOD);
  __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION);
  Handle<Code> adaptor =
      Isolate::Current()->builtins()->ArgumentsAdaptorTrampoline();
  __ Jump(adaptor, RelocInfo::CODE_TARGET);
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // rax : number of arguments
  // rbx : cache cell for call target
  // rdi : constructor function
  Label slow, non_function_call;

  // Check that function is not a smi.
  __ JumpIfSmi(rdi, &non_function_call);
  // Check that function is a JSFunction.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(not_equal, &slow);

  if (RecordCallTarget()) {
    GenerateRecordCallTarget(masm);
  }

  // Jump to the function-specific construct stub.
  __ movq(rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movq(rbx, FieldOperand(rbx, SharedFunctionInfo::kConstructStubOffset));
  __ lea(rbx, FieldOperand(rbx, Code::kHeaderSize));
  __ jmp(rbx);

  // rdi: called object
  // rax: number of arguments
  // rcx: object map
  Label do_call;
  __ bind(&slow);
  __ CmpInstanceType(rcx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function_call);
  __ GetBuiltinEntry(rdx, Builtins::CALL_FUNCTION_PROXY_AS_CONSTRUCTOR);
  __ jmp(&do_call);

  __ bind(&non_function_call);
  __ GetBuiltinEntry(rdx, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ bind(&do_call);
  // Set expected number of arguments to zero (not changing rax).
  __ Set(rbx, 0);
  __ SetCallKind(rcx, CALL_AS_METHOD);
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


bool CEntryStub::NeedsImmovableCode() {
  return false;
}


bool CEntryStub::IsPregenerated() {
#ifdef _WIN64
  return result_size_ == 1;
#else
  return true;
#endif
}


void CodeStub::GenerateStubsAheadOfTime() {
  CEntryStub::GenerateAheadOfTime();
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime();
  // It is important that the store buffer overflow stubs are generated first.
  RecordWriteStub::GenerateFixedRegStubsAheadOfTime();
}


void CodeStub::GenerateFPStubs() {
}


void CEntryStub::GenerateAheadOfTime() {
  CEntryStub stub(1, kDontSaveFPRegs);
  stub.GetCode()->set_is_pregenerated(true);
  CEntryStub save_doubles(1, kSaveFPRegs);
  save_doubles.GetCode()->set_is_pregenerated(true);
}


void CEntryStub::GenerateCore(MacroAssembler* masm,
                              Label* throw_normal_exception,
                              Label* throw_termination_exception,
                              Label* throw_out_of_memory_exception,
                              bool do_gc,
                              bool always_allocate_scope) {
  // rax: result parameter for PerformGC, if any.
  // rbx: pointer to C function  (C callee-saved).
  // rbp: frame pointer  (restored after C call).
  // rsp: stack pointer  (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: pointer to the first argument (C callee-saved).
  //      This pointer is reused in LeaveExitFrame(), so it is stored in a
  //      callee-saved register.

  // Simple results returned in rax (both AMD64 and Win64 calling conventions).
  // Complex results must be written to address passed as first argument.
  // AMD64 calling convention: a struct of two pointers in rax+rdx

  // Check stack alignment.
  if (FLAG_debug_code) {
    __ CheckStackAlignment();
  }

  if (do_gc) {
    // Pass failure code returned from last attempt as first argument to
    // PerformGC. No need to use PrepareCallCFunction/CallCFunction here as the
    // stack is known to be aligned. This function takes one argument which is
    // passed in register.
#ifdef _WIN64
    __ movq(rcx, rax);
#else  // _WIN64
    __ movq(rdi, rax);
#endif
    __ movq(kScratchRegister,
            FUNCTION_ADDR(Runtime::PerformGC),
            RelocInfo::RUNTIME_ENTRY);
    __ call(kScratchRegister);
  }

  ExternalReference scope_depth =
      ExternalReference::heap_always_allocate_scope_depth(masm->isolate());
  if (always_allocate_scope) {
    Operand scope_depth_operand = masm->ExternalOperand(scope_depth);
    __ incl(scope_depth_operand);
  }

  // Call C function.
#ifdef _WIN64
  // Windows 64-bit ABI passes arguments in rcx, rdx, r8, r9
  // Store Arguments object on stack, below the 4 WIN64 ABI parameter slots.
  __ movq(StackSpaceOperand(0), r14);  // argc.
  __ movq(StackSpaceOperand(1), r15);  // argv.
  if (result_size_ < 2) {
    // Pass a pointer to the Arguments object as the first argument.
    // Return result in single register (rax).
    __ lea(rcx, StackSpaceOperand(0));
    __ LoadAddress(rdx, ExternalReference::isolate_address());
  } else {
    ASSERT_EQ(2, result_size_);
    // Pass a pointer to the result location as the first argument.
    __ lea(rcx, StackSpaceOperand(2));
    // Pass a pointer to the Arguments object as the second argument.
    __ lea(rdx, StackSpaceOperand(0));
    __ LoadAddress(r8, ExternalReference::isolate_address());
  }

#else  // _WIN64
  // GCC passes arguments in rdi, rsi, rdx, rcx, r8, r9.
  __ movq(rdi, r14);  // argc.
  __ movq(rsi, r15);  // argv.
  __ movq(rdx, ExternalReference::isolate_address());
#endif
  __ call(rbx);
  // Result is in rax - do not destroy this register!

  if (always_allocate_scope) {
    Operand scope_depth_operand = masm->ExternalOperand(scope_depth);
    __ decl(scope_depth_operand);
  }

  // Check for failure result.
  Label failure_returned;
  STATIC_ASSERT(((kFailureTag + 1) & kFailureTagMask) == 0);
#ifdef _WIN64
  // If return value is on the stack, pop it to registers.
  if (result_size_ > 1) {
    ASSERT_EQ(2, result_size_);
    // Read result values stored on stack. Result is stored
    // above the four argument mirror slots and the two
    // Arguments object slots.
    __ movq(rax, Operand(rsp, 6 * kPointerSize));
    __ movq(rdx, Operand(rsp, 7 * kPointerSize));
  }
#endif
  __ lea(rcx, Operand(rax, 1));
  // Lower 2 bits of rcx are 0 iff rax has failure tag.
  __ testl(rcx, Immediate(kFailureTagMask));
  __ j(zero, &failure_returned);

  // Exit the JavaScript to C++ exit frame.
  __ LeaveExitFrame(save_doubles_);
  __ ret(0);

  // Handling of failure.
  __ bind(&failure_returned);

  Label retry;
  // If the returned exception is RETRY_AFTER_GC continue at retry label
  STATIC_ASSERT(Failure::RETRY_AFTER_GC == 0);
  __ testl(rax, Immediate(((1 << kFailureTypeTagSize) - 1) << kFailureTagSize));
  __ j(zero, &retry, Label::kNear);

  // Special handling of out of memory exceptions.
  __ movq(kScratchRegister, Failure::OutOfMemoryException(), RelocInfo::NONE);
  __ cmpq(rax, kScratchRegister);
  __ j(equal, throw_out_of_memory_exception);

  // Retrieve the pending exception and clear the variable.
  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, masm->isolate());
  Operand pending_exception_operand =
      masm->ExternalOperand(pending_exception_address);
  __ movq(rax, pending_exception_operand);
  __ LoadRoot(rdx, Heap::kTheHoleValueRootIndex);
  __ movq(pending_exception_operand, rdx);

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  __ CompareRoot(rax, Heap::kTerminationExceptionRootIndex);
  __ j(equal, throw_termination_exception);

  // Handle normal exception.
  __ jmp(throw_normal_exception);

  // Retry.
  __ bind(&retry);
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // rax: number of arguments including receiver
  // rbx: pointer to C function  (C callee-saved)
  // rbp: frame pointer of calling JS frame (restored after C call)
  // rsp: stack pointer  (restored after C call)
  // rsi: current context (restored)

  // NOTE: Invocations of builtins may return failure objects
  // instead of a proper result. The builtin entry handles
  // this by performing a garbage collection and retrying the
  // builtin once.

  // Enter the exit frame that transitions from JavaScript to C++.
#ifdef _WIN64
  int arg_stack_space = (result_size_ < 2 ? 2 : 4);
#else
  int arg_stack_space = 0;
#endif
  __ EnterExitFrame(arg_stack_space, save_doubles_);

  // rax: Holds the context at this point, but should not be used.
  //      On entry to code generated by GenerateCore, it must hold
  //      a failure result if the collect_garbage argument to GenerateCore
  //      is true.  This failure result can be the result of code
  //      generated by a previous call to GenerateCore.  The value
  //      of rax is then passed to Runtime::PerformGC.
  // rbx: pointer to builtin function  (C callee-saved).
  // rbp: frame pointer of exit frame  (restored after C call).
  // rsp: stack pointer (restored after C call).
  // r14: number of arguments including receiver (C callee-saved).
  // r15: argv pointer (C callee-saved).

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
  __ movq(rax, failure, RelocInfo::NONE);
  GenerateCore(masm,
               &throw_normal_exception,
               &throw_termination_exception,
               &throw_out_of_memory_exception,
               true,
               true);

  __ bind(&throw_out_of_memory_exception);
  // Set external caught exception to false.
  Isolate* isolate = masm->isolate();
  ExternalReference external_caught(Isolate::kExternalCaughtExceptionAddress,
                                    isolate);
  __ Set(rax, static_cast<int64_t>(false));
  __ Store(external_caught, rax);

  // Set pending exception and rax to out of memory exception.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate);
  __ movq(rax, Failure::OutOfMemoryException(), RelocInfo::NONE);
  __ Store(pending_exception, rax);
  // Fall through to the next label.

  __ bind(&throw_termination_exception);
  __ ThrowUncatchable(rax);

  __ bind(&throw_normal_exception);
  __ Throw(rax);
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  Label invoke, handler_entry, exit;
  Label not_outermost_js, not_outermost_js_2;
  {  // NOLINT. Scope block confuses linter.
    MacroAssembler::NoRootArrayScope uninitialized_root_register(masm);
    // Set up frame.
    __ push(rbp);
    __ movq(rbp, rsp);

    // Push the stack frame type marker twice.
    int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
    // Scratch register is neither callee-save, nor an argument register on any
    // platform. It's free to use at this point.
    // Cannot use smi-register for loading yet.
    __ movq(kScratchRegister,
            reinterpret_cast<uint64_t>(Smi::FromInt(marker)),
            RelocInfo::NONE);
    __ push(kScratchRegister);  // context slot
    __ push(kScratchRegister);  // function slot
    // Save callee-saved registers (X64/Win64 calling conventions).
    __ push(r12);
    __ push(r13);
    __ push(r14);
    __ push(r15);
#ifdef _WIN64
    __ push(rdi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
    __ push(rsi);  // Only callee save in Win64 ABI, argument in AMD64 ABI.
#endif
    __ push(rbx);
    // TODO(X64): On Win64, if we ever use XMM6-XMM15, the low low 64 bits are
    // callee save as well.

    // Set up the roots and smi constant registers.
    // Needs to be done before any further smi loads.
    __ InitializeSmiConstantRegister();
    __ InitializeRootRegister();
  }

  Isolate* isolate = masm->isolate();

  // Save copies of the top frame descriptor on the stack.
  ExternalReference c_entry_fp(Isolate::kCEntryFPAddress, isolate);
  {
    Operand c_entry_fp_operand = masm->ExternalOperand(c_entry_fp);
    __ push(c_entry_fp_operand);
  }

  // If this is the outermost JS call, set js_entry_sp value.
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate);
  __ Load(rax, js_entry_sp);
  __ testq(rax, rax);
  __ j(not_zero, &not_outermost_js);
  __ Push(Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ movq(rax, rbp);
  __ Store(js_entry_sp, rax);
  Label cont;
  __ jmp(&cont);
  __ bind(&not_outermost_js);
  __ Push(Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME));
  __ bind(&cont);

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ jmp(&invoke);
  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.
  ExternalReference pending_exception(Isolate::kPendingExceptionAddress,
                                      isolate);
  __ Store(pending_exception, rax);
  __ movq(rax, Failure::Exception(), RelocInfo::NONE);
  __ jmp(&exit);

  // Invoke: Link this frame into the handler chain.  There's only one
  // handler block in this code object, so its index is 0.
  __ bind(&invoke);
  __ PushTryHandler(StackHandler::JS_ENTRY, 0);

  // Clear any pending exceptions.
  __ LoadRoot(rax, Heap::kTheHoleValueRootIndex);
  __ Store(pending_exception, rax);

  // Fake a receiver (NULL).
  __ push(Immediate(0));  // receiver

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return. We load the address from an
  // external reference instead of inlining the call target address directly
  // in the code, because the builtin stubs may not have been generated yet
  // at the time this code is generated.
  if (is_construct) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate);
    __ Load(rax, construct_entry);
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate);
    __ Load(rax, entry);
  }
  __ lea(kScratchRegister, FieldOperand(rax, Code::kHeaderSize));
  __ call(kScratchRegister);

  // Unlink this frame from the handler chain.
  __ PopTryHandler();

  __ bind(&exit);
  // Check if the current stack frame is marked as the outermost JS frame.
  __ pop(rbx);
  __ Cmp(rbx, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ j(not_equal, &not_outermost_js_2);
  __ movq(kScratchRegister, js_entry_sp);
  __ movq(Operand(kScratchRegister, 0), Immediate(0));
  __ bind(&not_outermost_js_2);

  // Restore the top frame descriptor from the stack.
  { Operand c_entry_fp_operand = masm->ExternalOperand(c_entry_fp);
    __ pop(c_entry_fp_operand);
  }

  // Restore callee-saved registers (X64 conventions).
  __ pop(rbx);
#ifdef _WIN64
  // Callee save on in Win64 ABI, arguments/volatile in AMD64 ABI.
  __ pop(rsi);
  __ pop(rdi);
#endif
  __ pop(r15);
  __ pop(r14);
  __ pop(r13);
  __ pop(r12);
  __ addq(rsp, Immediate(2 * kPointerSize));  // remove markers

  // Restore frame pointer and return.
  __ pop(rbp);
  __ ret(0);
}


void InstanceofStub::Generate(MacroAssembler* masm) {
  // Implements "value instanceof function" operator.
  // Expected input state with no inline cache:
  //   rsp[0] : return address
  //   rsp[1] : function pointer
  //   rsp[2] : value
  // Expected input state with an inline one-element cache:
  //   rsp[0] : return address
  //   rsp[1] : offset from return address to location of inline cache
  //   rsp[2] : function pointer
  //   rsp[3] : value
  // Returns a bitwise zero to indicate that the value
  // is and instance of the function and anything else to
  // indicate that the value is not an instance.

  static const int kOffsetToMapCheckValue = 2;
  static const int kOffsetToResultValue = 18;
  // The last 4 bytes of the instruction sequence
  //   movq(rdi, FieldOperand(rax, HeapObject::kMapOffset))
  //   Move(kScratchRegister, FACTORY->the_hole_value())
  // in front of the hole value address.
  static const unsigned int kWordBeforeMapCheckValue = 0xBA49FF78;
  // The last 4 bytes of the instruction sequence
  //   __ j(not_equal, &cache_miss);
  //   __ LoadRoot(ToRegister(instr->result()), Heap::kTheHoleValueRootIndex);
  // before the offset of the hole value in the root array.
  static const unsigned int kWordBeforeResultValue = 0x458B4909;
  // Only the inline check flag is supported on X64.
  ASSERT(flags_ == kNoFlags || HasCallSiteInlineCheck());
  int extra_stack_space = HasCallSiteInlineCheck() ? kPointerSize : 0;

  // Get the object - go slow case if it's a smi.
  Label slow;

  __ movq(rax, Operand(rsp, 2 * kPointerSize + extra_stack_space));
  __ JumpIfSmi(rax, &slow);

  // Check that the left hand is a JS object. Leave its map in rax.
  __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rax);
  __ j(below, &slow);
  __ CmpInstanceType(rax, LAST_SPEC_OBJECT_TYPE);
  __ j(above, &slow);

  // Get the prototype of the function.
  __ movq(rdx, Operand(rsp, 1 * kPointerSize + extra_stack_space));
  // rdx is function, rax is map.

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck()) {
    // Look up the function and the map in the instanceof cache.
    Label miss;
    __ CompareRoot(rdx, Heap::kInstanceofCacheFunctionRootIndex);
    __ j(not_equal, &miss, Label::kNear);
    __ CompareRoot(rax, Heap::kInstanceofCacheMapRootIndex);
    __ j(not_equal, &miss, Label::kNear);
    __ LoadRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
    __ ret(2 * kPointerSize);
    __ bind(&miss);
  }

  __ TryGetFunctionPrototype(rdx, rbx, &slow, true);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(rbx, &slow);
  __ CmpObjectType(rbx, FIRST_SPEC_OBJECT_TYPE, kScratchRegister);
  __ j(below, &slow);
  __ CmpInstanceType(kScratchRegister, LAST_SPEC_OBJECT_TYPE);
  __ j(above, &slow);

  // Register mapping:
  //   rax is object map.
  //   rdx is function.
  //   rbx is function prototype.
  if (!HasCallSiteInlineCheck()) {
    __ StoreRoot(rdx, Heap::kInstanceofCacheFunctionRootIndex);
    __ StoreRoot(rax, Heap::kInstanceofCacheMapRootIndex);
  } else {
    // Get return address and delta to inlined map check.
    __ movq(kScratchRegister, Operand(rsp, 0 * kPointerSize));
    __ subq(kScratchRegister, Operand(rsp, 1 * kPointerSize));
    if (FLAG_debug_code) {
      __ movl(rdi, Immediate(kWordBeforeMapCheckValue));
      __ cmpl(Operand(kScratchRegister, kOffsetToMapCheckValue - 4), rdi);
      __ Assert(equal, "InstanceofStub unexpected call site cache (check).");
    }
    __ movq(kScratchRegister,
            Operand(kScratchRegister, kOffsetToMapCheckValue));
    __ movq(Operand(kScratchRegister, 0), rax);
  }

  __ movq(rcx, FieldOperand(rax, Map::kPrototypeOffset));

  // Loop through the prototype chain looking for the function prototype.
  Label loop, is_instance, is_not_instance;
  __ LoadRoot(kScratchRegister, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ cmpq(rcx, rbx);
  __ j(equal, &is_instance, Label::kNear);
  __ cmpq(rcx, kScratchRegister);
  // The code at is_not_instance assumes that kScratchRegister contains a
  // non-zero GCable value (the null object in this case).
  __ j(equal, &is_not_instance, Label::kNear);
  __ movq(rcx, FieldOperand(rcx, HeapObject::kMapOffset));
  __ movq(rcx, FieldOperand(rcx, Map::kPrototypeOffset));
  __ jmp(&loop);

  __ bind(&is_instance);
  if (!HasCallSiteInlineCheck()) {
    __ xorl(rax, rax);
    // Store bitwise zero in the cache.  This is a Smi in GC terms.
    STATIC_ASSERT(kSmiTag == 0);
    __ StoreRoot(rax, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Store offset of true in the root array at the inline check site.
    int true_offset = 0x100 +
        (Heap::kTrueValueRootIndex << kPointerSizeLog2) - kRootRegisterBias;
    // Assert it is a 1-byte signed value.
    ASSERT(true_offset >= 0 && true_offset < 0x100);
    __ movl(rax, Immediate(true_offset));
    __ movq(kScratchRegister, Operand(rsp, 0 * kPointerSize));
    __ subq(kScratchRegister, Operand(rsp, 1 * kPointerSize));
    __ movb(Operand(kScratchRegister, kOffsetToResultValue), rax);
    if (FLAG_debug_code) {
      __ movl(rax, Immediate(kWordBeforeResultValue));
      __ cmpl(Operand(kScratchRegister, kOffsetToResultValue - 4), rax);
      __ Assert(equal, "InstanceofStub unexpected call site cache (mov).");
    }
    __ Set(rax, 0);
  }
  __ ret(2 * kPointerSize + extra_stack_space);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    // We have to store a non-zero value in the cache.
    __ StoreRoot(kScratchRegister, Heap::kInstanceofCacheAnswerRootIndex);
  } else {
    // Store offset of false in the root array at the inline check site.
    int false_offset = 0x100 +
        (Heap::kFalseValueRootIndex << kPointerSizeLog2) - kRootRegisterBias;
    // Assert it is a 1-byte signed value.
    ASSERT(false_offset >= 0 && false_offset < 0x100);
    __ movl(rax, Immediate(false_offset));
    __ movq(kScratchRegister, Operand(rsp, 0 * kPointerSize));
    __ subq(kScratchRegister, Operand(rsp, 1 * kPointerSize));
    __ movb(Operand(kScratchRegister, kOffsetToResultValue), rax);
    if (FLAG_debug_code) {
      __ movl(rax, Immediate(kWordBeforeResultValue));
      __ cmpl(Operand(kScratchRegister, kOffsetToResultValue - 4), rax);
      __ Assert(equal, "InstanceofStub unexpected call site cache (mov)");
    }
  }
  __ ret(2 * kPointerSize + extra_stack_space);

  // Slow-case: Go through the JavaScript implementation.
  __ bind(&slow);
  if (HasCallSiteInlineCheck()) {
    // Remove extra value from the stack.
    __ pop(rcx);
    __ pop(rax);
    __ push(rcx);
  }
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
}


// Passing arguments in registers is not supported.
Register InstanceofStub::left() { return no_reg; }


Register InstanceofStub::right() { return no_reg; }


int CompareStub::MinorKey() {
  // Encode the three parameters in a unique 16 bit value. To avoid duplicate
  // stubs the never NaN NaN condition is only taken into account if the
  // condition is equals.
  ASSERT(static_cast<unsigned>(cc_) < (1 << 12));
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));
  return ConditionField::encode(static_cast<unsigned>(cc_))
         | RegisterField::encode(false)    // lhs_ and rhs_ are not used
         | StrictField::encode(strict_)
         | NeverNanNanField::encode(cc_ == equal ? never_nan_nan_ : false)
         | IncludeNumberCompareField::encode(include_number_compare_)
         | IncludeSmiCompareField::encode(include_smi_compare_);
}


// Unfortunately you have to run without snapshots to see most of these
// names in the profile since most compare stubs end up in the snapshot.
void CompareStub::PrintName(StringStream* stream) {
  ASSERT(lhs_.is(no_reg) && rhs_.is(no_reg));
  const char* cc_name;
  switch (cc_) {
    case less: cc_name = "LT"; break;
    case greater: cc_name = "GT"; break;
    case less_equal: cc_name = "LE"; break;
    case greater_equal: cc_name = "GE"; break;
    case equal: cc_name = "EQ"; break;
    case not_equal: cc_name = "NE"; break;
    default: cc_name = "UnknownCondition"; break;
  }
  bool is_equality = cc_ == equal || cc_ == not_equal;
  stream->Add("CompareStub_%s", cc_name);
  if (strict_ && is_equality) stream->Add("_STRICT");
  if (never_nan_nan_ && is_equality) stream->Add("_NO_NAN");
  if (!include_number_compare_) stream->Add("_NO_NUMBER");
  if (!include_smi_compare_) stream->Add("_NO_SMI");
}


// -------------------------------------------------------------------------
// StringCharCodeAtGenerator

void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
  Label flat_string;
  Label ascii_string;
  Label got_char_code;
  Label sliced_string;

  // If the receiver is a smi trigger the non-string case.
  __ JumpIfSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ movq(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ testb(result_, Immediate(kIsNotStringMask));
  __ j(not_zero, receiver_not_string_);

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ SmiCompare(index_, FieldOperand(object_, String::kLengthOffset));
  __ j(above_equal, index_out_of_range_);

  __ SmiToInteger32(index_, index_);

  StringCharLoadGenerator::Generate(
      masm, object_, index_, result_, &call_runtime_);

  __ Integer32ToSmi(result_, result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharCodeAt slow case");

  Factory* factory = masm->isolate()->factory();
  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              factory->heap_number_map(),
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    ASSERT(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  if (!index_.is(rax)) {
    // Save the conversion result before the pop instructions below
    // have a chance to overwrite it.
    __ movq(index_, rax);
  }
  __ pop(object_);
  // Reload the instance type.
  __ movq(result_, FieldOperand(object_, HeapObject::kMapOffset));
  __ movzxbl(result_, FieldOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ jmp(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ Integer32ToSmi(index_, index_);
  __ push(index_);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);
  if (!result_.is(rax)) {
    __ movq(result_, rax);
  }
  call_helper.AfterCall(masm);
  __ jmp(&exit_);

  __ Abort("Unexpected fallthrough from CharCodeAt slow case");
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  __ JumpIfNotSmi(code_, &slow_case_);
  __ SmiCompare(code_, Smi::FromInt(String::kMaxAsciiCharCode));
  __ j(above, &slow_case_);

  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  SmiIndex index = masm->SmiToIndex(kScratchRegister, code_, kPointerSizeLog2);
  __ movq(result_, FieldOperand(result_, index.reg, index.scale,
                                FixedArray::kHeaderSize));
  __ CompareRoot(result_, Heap::kUndefinedValueRootIndex);
  __ j(equal, &slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort("Unexpected fallthrough to CharFromCode slow case");

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  if (!result_.is(rax)) {
    __ movq(result_, rax);
  }
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
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  char_code_at_generator_.GenerateSlow(masm, call_helper);
  char_from_code_generator_.GenerateSlow(masm, call_helper);
}


void StringAddStub::Generate(MacroAssembler* masm) {
  Label call_runtime, call_builtin;
  Builtins::JavaScript builtin_id = Builtins::ADD;

  // Load the two arguments.
  __ movq(rax, Operand(rsp, 2 * kPointerSize));  // First argument (left).
  __ movq(rdx, Operand(rsp, 1 * kPointerSize));  // Second argument (right).

  // Make sure that both arguments are strings if not known in advance.
  if (flags_ == NO_STRING_ADD_FLAGS) {
    __ JumpIfSmi(rax, &call_runtime);
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, r8);
    __ j(above_equal, &call_runtime);

    // First argument is a a string, test second.
    __ JumpIfSmi(rdx, &call_runtime);
    __ CmpObjectType(rdx, FIRST_NONSTRING_TYPE, r9);
    __ j(above_equal, &call_runtime);
  } else {
    // Here at least one of the arguments is definitely a string.
    // We convert the one that is not known to be a string.
    if ((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) != 0);
      GenerateConvertArgument(masm, 2 * kPointerSize, rax, rbx, rcx, rdi,
                              &call_builtin);
      builtin_id = Builtins::STRING_ADD_RIGHT;
    } else if ((flags_ & NO_STRING_CHECK_RIGHT_IN_STUB) == 0) {
      ASSERT((flags_ & NO_STRING_CHECK_LEFT_IN_STUB) != 0);
      GenerateConvertArgument(masm, 1 * kPointerSize, rdx, rbx, rcx, rdi,
                              &call_builtin);
      builtin_id = Builtins::STRING_ADD_LEFT;
    }
  }

  // Both arguments are strings.
  // rax: first string
  // rdx: second string
  // Check if either of the strings are empty. In that case return the other.
  Label second_not_zero_length, both_not_zero_length;
  __ movq(rcx, FieldOperand(rdx, String::kLengthOffset));
  __ SmiTest(rcx);
  __ j(not_zero, &second_not_zero_length, Label::kNear);
  // Second string is empty, result is first string which is already in rax.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);
  __ bind(&second_not_zero_length);
  __ movq(rbx, FieldOperand(rax, String::kLengthOffset));
  __ SmiTest(rbx);
  __ j(not_zero, &both_not_zero_length, Label::kNear);
  // First string is empty, result is second string which is in rdx.
  __ movq(rax, rdx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Both strings are non-empty.
  // rax: first string
  // rbx: length of first string
  // rcx: length of second string
  // rdx: second string
  // r8: map of first string (if flags_ == NO_STRING_ADD_FLAGS)
  // r9: map of second string (if flags_ == NO_STRING_ADD_FLAGS)
  Label string_add_flat_result, longer_than_two;
  __ bind(&both_not_zero_length);

  // If arguments where known to be strings, maps are not loaded to r8 and r9
  // by the code above.
  if (flags_ != NO_STRING_ADD_FLAGS) {
    __ movq(r8, FieldOperand(rax, HeapObject::kMapOffset));
    __ movq(r9, FieldOperand(rdx, HeapObject::kMapOffset));
  }
  // Get the instance types of the two strings as they will be needed soon.
  __ movzxbl(r8, FieldOperand(r8, Map::kInstanceTypeOffset));
  __ movzxbl(r9, FieldOperand(r9, Map::kInstanceTypeOffset));

  // Look at the length of the result of adding the two strings.
  STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue / 2);
  __ SmiAdd(rbx, rbx, rcx);
  // Use the symbol table when adding two one character strings, as it
  // helps later optimizations to return a symbol here.
  __ SmiCompare(rbx, Smi::FromInt(2));
  __ j(not_equal, &longer_than_two);

  // Check that both strings are non-external ASCII strings.
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(r8, r9, rbx, rcx,
                                                  &call_runtime);

  // Get the two characters forming the sub string.
  __ movzxbq(rbx, FieldOperand(rax, SeqAsciiString::kHeaderSize));
  __ movzxbq(rcx, FieldOperand(rdx, SeqAsciiString::kHeaderSize));

  // Try to lookup two character string in symbol table. If it is not found
  // just allocate a new one.
  Label make_two_character_string, make_flat_ascii_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, rbx, rcx, r14, r11, rdi, r15, &make_two_character_string);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&make_two_character_string);
  __ Set(rdi, 2);
  __ AllocateAsciiString(rax, rdi, r8, r9, r11, &call_runtime);
  // rbx - first byte: first character
  // rbx - second byte: *maybe* second character
  // Make sure that the second byte of rbx contains the second character.
  __ movzxbq(rcx, FieldOperand(rdx, SeqAsciiString::kHeaderSize));
  __ shll(rcx, Immediate(kBitsPerByte));
  __ orl(rbx, rcx);
  // Write both characters to the new string.
  __ movw(FieldOperand(rax, SeqAsciiString::kHeaderSize), rbx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&longer_than_two);
  // Check if resulting string will be flat.
  __ SmiCompare(rbx, Smi::FromInt(ConsString::kMinLength));
  __ j(below, &string_add_flat_result);
  // Handle exceptionally long strings in the runtime system.
  STATIC_ASSERT((String::kMaxLength & 0x80000000) == 0);
  __ SmiCompare(rbx, Smi::FromInt(String::kMaxLength));
  __ j(above, &call_runtime);

  // If result is not supposed to be flat, allocate a cons string object. If
  // both strings are ASCII the result is an ASCII cons string.
  // rax: first string
  // rbx: length of resulting flat string
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of second string
  Label non_ascii, allocated, ascii_data;
  __ movl(rcx, r8);
  __ and_(rcx, r9);
  STATIC_ASSERT((kStringEncodingMask & kAsciiStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ testl(rcx, Immediate(kStringEncodingMask));
  __ j(zero, &non_ascii);
  __ bind(&ascii_data);
  // Allocate an ASCII cons string.
  __ AllocateAsciiConsString(rcx, rdi, no_reg, &call_runtime);
  __ bind(&allocated);
  // Fill the fields of the cons string.
  __ movq(FieldOperand(rcx, ConsString::kLengthOffset), rbx);
  __ movq(FieldOperand(rcx, ConsString::kHashFieldOffset),
          Immediate(String::kEmptyHashField));
  __ movq(FieldOperand(rcx, ConsString::kFirstOffset), rax);
  __ movq(FieldOperand(rcx, ConsString::kSecondOffset), rdx);
  __ movq(rax, rcx);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);
  __ bind(&non_ascii);
  // At least one of the strings is two-byte. Check whether it happens
  // to contain only ASCII characters.
  // rcx: first instance type AND second instance type.
  // r8: first instance type.
  // r9: second instance type.
  __ testb(rcx, Immediate(kAsciiDataHintMask));
  __ j(not_zero, &ascii_data);
  __ xor_(r8, r9);
  STATIC_ASSERT(kAsciiStringTag != 0 && kAsciiDataHintTag != 0);
  __ andb(r8, Immediate(kAsciiStringTag | kAsciiDataHintTag));
  __ cmpb(r8, Immediate(kAsciiStringTag | kAsciiDataHintTag));
  __ j(equal, &ascii_data);
  // Allocate a two byte cons string.
  __ AllocateTwoByteConsString(rcx, rdi, no_reg, &call_runtime);
  __ jmp(&allocated);

  // We cannot encounter sliced strings or cons strings here since:
  STATIC_ASSERT(SlicedString::kMinLength >= ConsString::kMinLength);
  // Handle creating a flat result from either external or sequential strings.
  // Locate the first characters' locations.
  // rax: first string
  // rbx: length of resulting flat string as smi
  // rdx: second string
  // r8: instance type of first string
  // r9: instance type of first string
  Label first_prepared, second_prepared;
  Label first_is_sequential, second_is_sequential;
  __ bind(&string_add_flat_result);

  __ SmiToInteger32(r14, FieldOperand(rax, SeqString::kLengthOffset));
  // r14: length of first string
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(r8, Immediate(kStringRepresentationMask));
  __ j(zero, &first_is_sequential, Label::kNear);
  // Rule out short external string and load string resource.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ testb(r8, Immediate(kShortExternalStringMask));
  __ j(not_zero, &call_runtime);
  __ movq(rcx, FieldOperand(rax, ExternalString::kResourceDataOffset));
  __ jmp(&first_prepared, Label::kNear);
  __ bind(&first_is_sequential);
  STATIC_ASSERT(SeqAsciiString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ lea(rcx, FieldOperand(rax, SeqAsciiString::kHeaderSize));
  __ bind(&first_prepared);

  // Check whether both strings have same encoding.
  __ xorl(r8, r9);
  __ testb(r8, Immediate(kStringEncodingMask));
  __ j(not_zero, &call_runtime);

  __ SmiToInteger32(r15, FieldOperand(rdx, SeqString::kLengthOffset));
  // r15: length of second string
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(r9, Immediate(kStringRepresentationMask));
  __ j(zero, &second_is_sequential, Label::kNear);
  // Rule out short external string and load string resource.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ testb(r9, Immediate(kShortExternalStringMask));
  __ j(not_zero, &call_runtime);
  __ movq(rdx, FieldOperand(rdx, ExternalString::kResourceDataOffset));
  __ jmp(&second_prepared, Label::kNear);
  __ bind(&second_is_sequential);
  STATIC_ASSERT(SeqAsciiString::kHeaderSize == SeqTwoByteString::kHeaderSize);
  __ lea(rdx, FieldOperand(rdx, SeqAsciiString::kHeaderSize));
  __ bind(&second_prepared);

  Label non_ascii_string_add_flat_result;
  // r9: instance type of second string
  // First string and second string have the same encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ SmiToInteger32(rbx, rbx);
  __ testb(r9, Immediate(kStringEncodingMask));
  __ j(zero, &non_ascii_string_add_flat_result);

  __ bind(&make_flat_ascii_string);
  // Both strings are ASCII strings. As they are short they are both flat.
  __ AllocateAsciiString(rax, rbx, rdi, r8, r9, &call_runtime);
  // rax: result string
  // Locate first character of result.
  __ lea(rbx, FieldOperand(rax, SeqAsciiString::kHeaderSize));
  // rcx: first char of first string
  // rbx: first character of result
  // r14: length of first string
  StringHelper::GenerateCopyCharacters(masm, rbx, rcx, r14, true);
  // rbx: next character of result
  // rdx: first char of second string
  // r15: length of second string
  StringHelper::GenerateCopyCharacters(masm, rbx, rdx, r15, true);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&non_ascii_string_add_flat_result);
  // Both strings are ASCII strings. As they are short they are both flat.
  __ AllocateTwoByteString(rax, rbx, rdi, r8, r9, &call_runtime);
  // rax: result string
  // Locate first character of result.
  __ lea(rbx, FieldOperand(rax, SeqTwoByteString::kHeaderSize));
  // rcx: first char of first string
  // rbx: first character of result
  // r14: length of first string
  StringHelper::GenerateCopyCharacters(masm, rbx, rcx, r14, false);
  // rbx: next character of result
  // rdx: first char of second string
  // r15: length of second string
  StringHelper::GenerateCopyCharacters(masm, rbx, rdx, r15, false);
  __ IncrementCounter(counters->string_add_native(), 1);
  __ ret(2 * kPointerSize);

  // Just jump to runtime to add the two strings.
  __ bind(&call_runtime);
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
                                            Label* slow) {
  // First check if the argument is already a string.
  Label not_string, done;
  __ JumpIfSmi(arg, &not_string);
  __ CmpObjectType(arg, FIRST_NONSTRING_TYPE, scratch1);
  __ j(below, &done);

  // Check the number to string cache.
  Label not_cached;
  __ bind(&not_string);
  // Puts the cached result into scratch1.
  NumberToStringStub::GenerateLookupNumberStringCache(masm,
                                                      arg,
                                                      scratch1,
                                                      scratch2,
                                                      scratch3,
                                                      false,
                                                      &not_cached);
  __ movq(arg, scratch1);
  __ movq(Operand(rsp, stack_offset), arg);
  __ jmp(&done);

  // Check if the argument is a safe string wrapper.
  __ bind(&not_cached);
  __ JumpIfSmi(arg, slow);
  __ CmpObjectType(arg, JS_VALUE_TYPE, scratch1);  // map -> scratch1.
  __ j(not_equal, slow);
  __ testb(FieldOperand(scratch1, Map::kBitField2Offset),
           Immediate(1 << Map::kStringWrapperSafeForDefaultValueOf));
  __ j(zero, slow);
  __ movq(arg, FieldOperand(arg, JSValue::kValueOffset));
  __ movq(Operand(rsp, stack_offset), arg);

  __ bind(&done);
}


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          bool ascii) {
  Label loop;
  __ bind(&loop);
  // This loop just copies one character at a time, as it is only used for very
  // short strings.
  if (ascii) {
    __ movb(kScratchRegister, Operand(src, 0));
    __ movb(Operand(dest, 0), kScratchRegister);
    __ incq(src);
    __ incq(dest);
  } else {
    __ movzxwl(kScratchRegister, Operand(src, 0));
    __ movw(Operand(dest, 0), kScratchRegister);
    __ addq(src, Immediate(2));
    __ addq(dest, Immediate(2));
  }
  __ decl(count);
  __ j(not_zero, &loop);
}


void StringHelper::GenerateCopyCharactersREP(MacroAssembler* masm,
                                             Register dest,
                                             Register src,
                                             Register count,
                                             bool ascii) {
  // Copy characters using rep movs of doublewords. Align destination on 4 byte
  // boundary before starting rep movs. Copy remaining characters after running
  // rep movs.
  // Count is positive int32, dest and src are character pointers.
  ASSERT(dest.is(rdi));  // rep movs destination
  ASSERT(src.is(rsi));  // rep movs source
  ASSERT(count.is(rcx));  // rep movs count

  // Nothing to do for zero characters.
  Label done;
  __ testl(count, count);
  __ j(zero, &done, Label::kNear);

  // Make count the number of bytes to copy.
  if (!ascii) {
    STATIC_ASSERT(2 == sizeof(uc16));
    __ addl(count, count);
  }

  // Don't enter the rep movs if there are less than 4 bytes to copy.
  Label last_bytes;
  __ testl(count, Immediate(~7));
  __ j(zero, &last_bytes, Label::kNear);

  // Copy from edi to esi using rep movs instruction.
  __ movl(kScratchRegister, count);
  __ shr(count, Immediate(3));  // Number of doublewords to copy.
  __ repmovsq();

  // Find number of bytes left.
  __ movl(count, kScratchRegister);
  __ and_(count, Immediate(7));

  // Check if there are more bytes to copy.
  __ bind(&last_bytes);
  __ testl(count, count);
  __ j(zero, &done, Label::kNear);

  // Copy remaining characters.
  Label loop;
  __ bind(&loop);
  __ movb(kScratchRegister, Operand(src, 0));
  __ movb(Operand(dest, 0), kScratchRegister);
  __ incq(src);
  __ incq(dest);
  __ decl(count);
  __ j(not_zero, &loop);

  __ bind(&done);
}

void StringHelper::GenerateTwoCharacterSymbolTableProbe(MacroAssembler* masm,
                                                        Register c1,
                                                        Register c2,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4,
                                                        Label* not_found) {
  // Register scratch3 is the general scratch register in this function.
  Register scratch = scratch3;

  // Make sure that both characters are not digits as such strings has a
  // different hash algorithm. Don't try to look for these in the symbol table.
  Label not_array_index;
  __ leal(scratch, Operand(c1, -'0'));
  __ cmpl(scratch, Immediate(static_cast<int>('9' - '0')));
  __ j(above, &not_array_index, Label::kNear);
  __ leal(scratch, Operand(c2, -'0'));
  __ cmpl(scratch, Immediate(static_cast<int>('9' - '0')));
  __ j(below_equal, not_found);

  __ bind(&not_array_index);
  // Calculate the two character string hash.
  Register hash = scratch1;
  GenerateHashInit(masm, hash, c1, scratch);
  GenerateHashAddCharacter(masm, hash, c2, scratch);
  GenerateHashGetHash(masm, hash, scratch);

  // Collect the two characters in a register.
  Register chars = c1;
  __ shl(c2, Immediate(kBitsPerByte));
  __ orl(chars, c2);

  // chars: two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:  hash of two character string.

  // Load the symbol table.
  Register symbol_table = c2;
  __ LoadRoot(symbol_table, Heap::kSymbolTableRootIndex);

  // Calculate capacity mask from the symbol table capacity.
  Register mask = scratch2;
  __ SmiToInteger32(mask,
                    FieldOperand(symbol_table, SymbolTable::kCapacityOffset));
  __ decl(mask);

  Register map = scratch4;

  // Registers
  // chars:        two character string, char 1 in byte 0 and char 2 in byte 1.
  // hash:         hash of two character string (32-bit int)
  // symbol_table: symbol table
  // mask:         capacity mask (32-bit int)
  // map:          -
  // scratch:      -

  // Perform a number of probes in the symbol table.
  static const int kProbes = 4;
  Label found_in_symbol_table;
  Label next_probe[kProbes];
  Register candidate = scratch;  // Scratch register contains candidate.
  for (int i = 0; i < kProbes; i++) {
    // Calculate entry in symbol table.
    __ movl(scratch, hash);
    if (i > 0) {
      __ addl(scratch, Immediate(SymbolTable::GetProbeOffset(i)));
    }
    __ andl(scratch, mask);

    // Load the entry from the symbol table.
    STATIC_ASSERT(SymbolTable::kEntrySize == 1);
    __ movq(candidate,
            FieldOperand(symbol_table,
                         scratch,
                         times_pointer_size,
                         SymbolTable::kElementsStartOffset));

    // If entry is undefined no string with this hash can be found.
    Label is_string;
    __ CmpObjectType(candidate, ODDBALL_TYPE, map);
    __ j(not_equal, &is_string, Label::kNear);

    __ CompareRoot(candidate, Heap::kUndefinedValueRootIndex);
    __ j(equal, not_found);
    // Must be the hole (deleted entry).
    if (FLAG_debug_code) {
      __ LoadRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
      __ cmpq(kScratchRegister, candidate);
      __ Assert(equal, "oddball in symbol table is not undefined or the hole");
    }
    __ jmp(&next_probe[i]);

    __ bind(&is_string);

    // If length is not 2 the string is not a candidate.
    __ SmiCompare(FieldOperand(candidate, String::kLengthOffset),
                  Smi::FromInt(2));
    __ j(not_equal, &next_probe[i]);

    // We use kScratchRegister as a temporary register in assumption that
    // JumpIfInstanceTypeIsNotSequentialAscii does not use it implicitly
    Register temp = kScratchRegister;

    // Check that the candidate is a non-external ASCII string.
    __ movzxbl(temp, FieldOperand(map, Map::kInstanceTypeOffset));
    __ JumpIfInstanceTypeIsNotSequentialAscii(
        temp, temp, &next_probe[i]);

    // Check if the two characters match.
    __ movl(temp, FieldOperand(candidate, SeqAsciiString::kHeaderSize));
    __ andl(temp, Immediate(0x0000ffff));
    __ cmpl(chars, temp);
    __ j(equal, &found_in_symbol_table);
    __ bind(&next_probe[i]);
  }

  // No matching 2 character string found by probing.
  __ jmp(not_found);

  // Scratch register contains result when we fall through to here.
  Register result = candidate;
  __ bind(&found_in_symbol_table);
  if (!result.is(rax)) {
    __ movq(rax, result);
  }
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character,
                                    Register scratch) {
  // hash = (seed + character) + ((seed + character) << 10);
  __ LoadRoot(scratch, Heap::kHashSeedRootIndex);
  __ SmiToInteger32(scratch, scratch);
  __ addl(scratch, character);
  __ movl(hash, scratch);
  __ shll(scratch, Immediate(10));
  __ addl(hash, scratch);
  // hash ^= hash >> 6;
  __ movl(scratch, hash);
  __ shrl(scratch, Immediate(6));
  __ xorl(hash, scratch);
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character,
                                            Register scratch) {
  // hash += character;
  __ addl(hash, character);
  // hash += hash << 10;
  __ movl(scratch, hash);
  __ shll(scratch, Immediate(10));
  __ addl(hash, scratch);
  // hash ^= hash >> 6;
  __ movl(scratch, hash);
  __ shrl(scratch, Immediate(6));
  __ xorl(hash, scratch);
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash,
                                       Register scratch) {
  // hash += hash << 3;
  __ leal(hash, Operand(hash, hash, times_8, 0));
  // hash ^= hash >> 11;
  __ movl(scratch, hash);
  __ shrl(scratch, Immediate(11));
  __ xorl(hash, scratch);
  // hash += hash << 15;
  __ movl(scratch, hash);
  __ shll(scratch, Immediate(15));
  __ addl(hash, scratch);

  __ andl(hash, Immediate(String::kHashBitMask));

  // if (hash == 0) hash = 27;
  Label hash_not_zero;
  __ j(not_zero, &hash_not_zero);
  __ Set(hash, StringHasher::kZeroHash);
  __ bind(&hash_not_zero);
}

void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]: return address
  //  rsp[8]: to
  //  rsp[16]: from
  //  rsp[24]: string

  const int kToOffset = 1 * kPointerSize;
  const int kFromOffset = kToOffset + kPointerSize;
  const int kStringOffset = kFromOffset + kPointerSize;
  const int kArgumentsSize = (kStringOffset + kPointerSize) - kToOffset;

  // Make sure first argument is a string.
  __ movq(rax, Operand(rsp, kStringOffset));
  STATIC_ASSERT(kSmiTag == 0);
  __ testl(rax, Immediate(kSmiTagMask));
  __ j(zero, &runtime);
  Condition is_string = masm->IsObjectStringType(rax, rbx, rbx);
  __ j(NegateCondition(is_string), &runtime);

  // rax: string
  // rbx: instance type
  // Calculate length of sub string using the smi values.
  Label result_longer_than_two;
  __ movq(rcx, Operand(rsp, kToOffset));
  __ movq(rdx, Operand(rsp, kFromOffset));
  __ JumpUnlessBothNonNegativeSmi(rcx, rdx, &runtime);

  __ SmiSub(rcx, rcx, rdx);  // Overflow doesn't happen.
  __ cmpq(FieldOperand(rax, String::kLengthOffset), rcx);
  Label not_original_string;
  __ j(not_equal, &not_original_string, Label::kNear);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(kArgumentsSize);
  __ bind(&not_original_string);
  // Special handling of sub-strings of length 1 and 2. One character strings
  // are handled in the runtime system (looked up in the single character
  // cache). Two character strings are looked for in the symbol cache.
  __ SmiToInteger32(rcx, rcx);
  __ cmpl(rcx, Immediate(2));
  __ j(greater, &result_longer_than_two);
  __ j(less, &runtime);

  // Sub string of length 2 requested.
  // rax: string
  // rbx: instance type
  // rcx: sub string length (value is 2)
  // rdx: from index (smi)
  __ JumpIfInstanceTypeIsNotSequentialAscii(rbx, rbx, &runtime);

  // Get the two characters forming the sub string.
  __ SmiToInteger32(rdx, rdx);  // From index is no longer smi.
  __ movzxbq(rbx, FieldOperand(rax, rdx, times_1, SeqAsciiString::kHeaderSize));
  __ movzxbq(rdi,
             FieldOperand(rax, rdx, times_1, SeqAsciiString::kHeaderSize + 1));

  // Try to lookup two character string in symbol table.
  Label make_two_character_string;
  StringHelper::GenerateTwoCharacterSymbolTableProbe(
      masm, rbx, rdi, r9, r11, r14, r15, &make_two_character_string);
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);

  __ bind(&make_two_character_string);
  // Set up registers for allocating the two character string.
  __ movzxwq(rbx, FieldOperand(rax, rdx, times_1, SeqAsciiString::kHeaderSize));
  __ AllocateAsciiString(rax, rcx, r11, r14, r15, &runtime);
  __ movw(FieldOperand(rax, SeqAsciiString::kHeaderSize), rbx);
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(3 * kPointerSize);

  __ bind(&result_longer_than_two);
  // rax: string
  // rbx: instance type
  // rcx: sub string length
  // rdx: from index (smi)
  // Deal with different string types: update the index if necessary
  // and put the underlying string into edi.
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ testb(rbx, Immediate(kIsIndirectStringMask));
  __ j(zero, &seq_or_external_string, Label::kNear);

  __ testb(rbx, Immediate(kSlicedNotConsMask));
  __ j(not_zero, &sliced_string, Label::kNear);
  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  __ CompareRoot(FieldOperand(rax, ConsString::kSecondOffset),
                 Heap::kEmptyStringRootIndex);
  __ j(not_equal, &runtime);
  __ movq(rdi, FieldOperand(rax, ConsString::kFirstOffset));
  // Update instance type.
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and correct start index by offset.
  __ addq(rdx, FieldOperand(rax, SlicedString::kOffsetOffset));
  __ movq(rdi, FieldOperand(rax, SlicedString::kParentOffset));
  // Update instance type.
  __ movq(rbx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ movzxbl(rbx, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ jmp(&underlying_unpacked, Label::kNear);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the correct register.
  __ movq(rdi, rax);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // rdi: underlying subject string
    // rbx: instance type of underlying subject string
    // rdx: adjusted start index (smi)
    // rcx: length
    // If coming from the make_two_character_string path, the string
    // is too short to be sliced anyways.
    __ cmpq(rcx, Immediate(SlicedString::kMinLength));
    // Short slice.  Copy instead of slicing.
    __ j(less, &copy_routine);
    // Allocate new sliced string.  At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string.  It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyways due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kAsciiStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ testb(rbx, Immediate(kStringEncodingMask));
    __ j(zero, &two_byte_slice, Label::kNear);
    __ AllocateAsciiSlicedString(rax, rbx, r14, &runtime);
    __ jmp(&set_slice_header, Label::kNear);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(rax, rbx, r14, &runtime);
    __ bind(&set_slice_header);
    __ Integer32ToSmi(rcx, rcx);
    __ movq(FieldOperand(rax, SlicedString::kLengthOffset), rcx);
    __ movq(FieldOperand(rax, SlicedString::kHashFieldOffset),
           Immediate(String::kEmptyHashField));
    __ movq(FieldOperand(rax, SlicedString::kParentOffset), rdi);
    __ movq(FieldOperand(rax, SlicedString::kOffsetOffset), rdx);
    __ IncrementCounter(counters->sub_string_native(), 1);
    __ ret(kArgumentsSize);

    __ bind(&copy_routine);
  }

  // rdi: underlying subject string
  // rbx: instance type of underlying subject string
  // rdx: adjusted start index (smi)
  // rcx: length
  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label two_byte_sequential, sequential_string;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(rbx, Immediate(kExternalStringTag));
  __ j(zero, &sequential_string);

  // Handle external string.
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ testb(rbx, Immediate(kShortExternalStringMask));
  __ j(not_zero, &runtime);
  __ movq(rdi, FieldOperand(rdi, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqAsciiString::kHeaderSize);
  __ subq(rdi, Immediate(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&sequential_string);
  STATIC_ASSERT((kAsciiStringTag & kStringEncodingMask) != 0);
  __ testb(rbx, Immediate(kStringEncodingMask));
  __ j(zero, &two_byte_sequential);

  // Allocate the result.
  __ AllocateAsciiString(rax, rcx, r11, r14, r15, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(r14, rsi);  // esi used by following code.
  {  // Locate character of sub string start.
    SmiIndex smi_as_index = masm->SmiToIndex(rdx, rdx, times_1);
    __ lea(rsi, Operand(rdi, smi_as_index.reg, smi_as_index.scale,
                        SeqAsciiString::kHeaderSize - kHeapObjectTag));
  }
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqAsciiString::kHeaderSize));

  // rax: result string
  // rcx: result length
  // rdi: first character of result
  // rsi: character of sub string start
  // r14: original value of rsi
  StringHelper::GenerateCopyCharactersREP(masm, rdi, rsi, rcx, true);
  __ movq(rsi, r14);  // Restore rsi.
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(kArgumentsSize);

  __ bind(&two_byte_sequential);
  // Allocate the result.
  __ AllocateTwoByteString(rax, rcx, r11, r14, r15, &runtime);

  // rax: result string
  // rcx: result string length
  __ movq(r14, rsi);  // esi used by following code.
  {  // Locate character of sub string start.
    SmiIndex smi_as_index = masm->SmiToIndex(rdx, rdx, times_2);
    __ lea(rsi, Operand(rdi, smi_as_index.reg, smi_as_index.scale,
                        SeqAsciiString::kHeaderSize - kHeapObjectTag));
  }
  // Locate first character of result.
  __ lea(rdi, FieldOperand(rax, SeqTwoByteString::kHeaderSize));

  // rax: result string
  // rcx: result length
  // rdi: first character of result
  // rsi: character of sub string start
  // r14: original value of rsi
  StringHelper::GenerateCopyCharactersREP(masm, rdi, rsi, rcx, false);
  __ movq(rsi, r14);  // Restore esi.
  __ IncrementCounter(counters->sub_string_native(), 1);
  __ ret(kArgumentsSize);

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);
}


void StringCompareStub::GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                                      Register left,
                                                      Register right,
                                                      Register scratch1,
                                                      Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label check_zero_length;
  __ movq(length, FieldOperand(left, String::kLengthOffset));
  __ SmiCompare(length, FieldOperand(right, String::kLengthOffset));
  __ j(equal, &check_zero_length, Label::kNear);
  __ Move(rax, Smi::FromInt(NOT_EQUAL));
  __ ret(0);

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ SmiTest(length);
  __ j(not_zero, &compare_chars, Label::kNear);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Compare characters.
  __ bind(&compare_chars);
  Label strings_not_equal;
  GenerateAsciiCharsCompareLoop(masm, left, right, length, scratch2,
                                &strings_not_equal, Label::kNear);

  // Characters are equal.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Characters are not equal.
  __ bind(&strings_not_equal);
  __ Move(rax, Smi::FromInt(NOT_EQUAL));
  __ ret(0);
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3,
                                                        Register scratch4) {
  // Ensure that you can always subtract a string length from a non-negative
  // number (e.g. another length).
  STATIC_ASSERT(String::kMaxLength < 0x7fffffff);

  // Find minimum length and length difference.
  __ movq(scratch1, FieldOperand(left, String::kLengthOffset));
  __ movq(scratch4, scratch1);
  __ SmiSub(scratch4,
            scratch4,
            FieldOperand(right, String::kLengthOffset));
  // Register scratch4 now holds left.length - right.length.
  const Register length_difference = scratch4;
  Label left_shorter;
  __ j(less, &left_shorter, Label::kNear);
  // The right string isn't longer that the left one.
  // Get the right string's length by subtracting the (non-negative) difference
  // from the left string's length.
  __ SmiSub(scratch1, scratch1, length_difference);
  __ bind(&left_shorter);
  // Register scratch1 now holds Min(left.length, right.length).
  const Register min_length = scratch1;

  Label compare_lengths;
  // If min-length is zero, go directly to comparing lengths.
  __ SmiTest(min_length);
  __ j(zero, &compare_lengths, Label::kNear);

  // Compare loop.
  Label result_not_equal;
  GenerateAsciiCharsCompareLoop(masm, left, right, min_length, scratch2,
                                &result_not_equal, Label::kNear);

  // Completed loop without finding different characters.
  // Compare lengths (precomputed).
  __ bind(&compare_lengths);
  __ SmiTest(length_difference);
  __ j(not_zero, &result_not_equal, Label::kNear);

  // Result is EQUAL.
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  Label result_greater;
  __ bind(&result_not_equal);
  // Unequal comparison of left to right, either character or length.
  __ j(greater, &result_greater, Label::kNear);

  // Result is LESS.
  __ Move(rax, Smi::FromInt(LESS));
  __ ret(0);

  // Result is GREATER.
  __ bind(&result_greater);
  __ Move(rax, Smi::FromInt(GREATER));
  __ ret(0);
}


void StringCompareStub::GenerateAsciiCharsCompareLoop(
    MacroAssembler* masm,
    Register left,
    Register right,
    Register length,
    Register scratch,
    Label* chars_not_equal,
    Label::Distance near_jump) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiToInteger32(length, length);
  __ lea(left,
         FieldOperand(left, length, times_1, SeqAsciiString::kHeaderSize));
  __ lea(right,
         FieldOperand(right, length, times_1, SeqAsciiString::kHeaderSize));
  __ neg(length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ movb(scratch, Operand(left, index, times_1, 0));
  __ cmpb(scratch, Operand(right, index, times_1, 0));
  __ j(not_equal, chars_not_equal, near_jump);
  __ incq(index);
  __ j(not_zero, &loop);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  rsp[0]: return address
  //  rsp[8]: right string
  //  rsp[16]: left string

  __ movq(rdx, Operand(rsp, 2 * kPointerSize));  // left
  __ movq(rax, Operand(rsp, 1 * kPointerSize));  // right

  // Check for identity.
  Label not_same;
  __ cmpq(rdx, rax);
  __ j(not_equal, &not_same, Label::kNear);
  __ Move(rax, Smi::FromInt(EQUAL));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_compare_native(), 1);
  __ ret(2 * kPointerSize);

  __ bind(&not_same);

  // Check that both are sequential ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(rdx, rax, rcx, rbx, &runtime);

  // Inline comparison of ASCII strings.
  __ IncrementCounter(counters->string_compare_native(), 1);
  // Drop arguments from the stack
  __ pop(rcx);
  __ addq(rsp, Immediate(2 * kPointerSize));
  __ push(rcx);
  GenerateCompareFlatAsciiStrings(masm, rdx, rax, rcx, rbx, rdi, r8);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void ICCompareStub::GenerateSmis(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SMIS);
  Label miss;
  __ JumpIfNotBothSmi(rdx, rax, &miss, Label::kNear);

  if (GetCondition() == equal) {
    // For equality we do not care about the sign of the result.
    __ subq(rax, rdx);
  } else {
    Label done;
    __ subq(rdx, rax);
    __ j(no_overflow, &done, Label::kNear);
    // Correct sign of result in case of overflow.
    __ SmiNot(rdx, rdx);
    __ bind(&done);
    __ movq(rax, rdx);
  }
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateHeapNumbers(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::HEAP_NUMBERS);

  Label generic_stub;
  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss;
  Condition either_smi = masm->CheckEitherSmi(rax, rdx);
  __ j(either_smi, &generic_stub, Label::kNear);

  __ CmpObjectType(rax, HEAP_NUMBER_TYPE, rcx);
  __ j(not_equal, &maybe_undefined1, Label::kNear);
  __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rcx);
  __ j(not_equal, &maybe_undefined2, Label::kNear);

  // Load left and right operand
  __ movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  __ movsd(xmm1, FieldOperand(rax, HeapNumber::kValueOffset));

  // Compare operands
  __ ucomisd(xmm0, xmm1);

  // Don't base result on EFLAGS when a NaN is involved.
  __ j(parity_even, &unordered, Label::kNear);

  // Return a result of -1, 0, or 1, based on EFLAGS.
  // Performing mov, because xor would destroy the flag register.
  __ movl(rax, Immediate(0));
  __ movl(rcx, Immediate(0));
  __ setcc(above, rax);  // Add one to zero if carry clear and not equal.
  __ sbbq(rax, rcx);  // Subtract one if below (aka. carry set).
  __ ret(0);

  __ bind(&unordered);
  CompareStub stub(GetCondition(), strict(), NO_COMPARE_FLAGS);
  __ bind(&generic_stub);
  __ jmp(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ Cmp(rax, masm->isolate()->factory()->undefined_value());
    __ j(not_equal, &miss);
    __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rcx);
    __ j(not_equal, &maybe_undefined2, Label::kNear);
    __ jmp(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ Cmp(rdx, masm->isolate()->factory()->undefined_value());
    __ j(equal, &unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateSymbols(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::SYMBOLS);
  ASSERT(GetCondition() == equal);

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;

  // Check that both operands are heap objects.
  Label miss;
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss, Label::kNear);

  // Check that both operands are symbols.
  __ movq(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movq(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbq(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbq(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kSymbolTag != 0);
  __ and_(tmp1, tmp2);
  __ testb(tmp1, Immediate(kIsSymbolMask));
  __ j(zero, &miss, Label::kNear);

  // Symbols are compared by identity.
  Label done;
  __ cmpq(left, right);
  // Make sure rax is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  ASSERT(right.is(rax));
  __ j(not_equal, &done, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ bind(&done);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateStrings(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::STRINGS);
  Label miss;

  bool equality = Token::IsEqualityOp(op_);

  // Registers containing left and right operands respectively.
  Register left = rdx;
  Register right = rax;
  Register tmp1 = rcx;
  Register tmp2 = rbx;
  Register tmp3 = rdi;

  // Check that both operands are heap objects.
  Condition cond = masm->CheckEitherSmi(left, right, tmp1);
  __ j(cond, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ movq(tmp1, FieldOperand(left, HeapObject::kMapOffset));
  __ movq(tmp2, FieldOperand(right, HeapObject::kMapOffset));
  __ movzxbq(tmp1, FieldOperand(tmp1, Map::kInstanceTypeOffset));
  __ movzxbq(tmp2, FieldOperand(tmp2, Map::kInstanceTypeOffset));
  __ movq(tmp3, tmp1);
  STATIC_ASSERT(kNotStringTag != 0);
  __ or_(tmp3, tmp2);
  __ testb(tmp3, Immediate(kIsNotStringMask));
  __ j(not_zero, &miss);

  // Fast check for identical strings.
  Label not_same;
  __ cmpq(left, right);
  __ j(not_equal, &not_same, Label::kNear);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ Move(rax, Smi::FromInt(EQUAL));
  __ ret(0);

  // Handle not identical strings.
  __ bind(&not_same);

  // Check that both strings are symbols. If they are, we're done
  // because we already know they are not identical.
  if (equality) {
    Label do_compare;
    STATIC_ASSERT(kSymbolTag != 0);
    __ and_(tmp1, tmp2);
    __ testb(tmp1, Immediate(kIsSymbolMask));
    __ j(zero, &do_compare, Label::kNear);
    // Make sure rax is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    ASSERT(right.is(rax));
    __ ret(0);
    __ bind(&do_compare);
  }

  // Check that both strings are sequential ASCII.
  Label runtime;
  __ JumpIfNotBothSequentialAsciiStrings(left, right, tmp1, tmp2, &runtime);

  // Compare flat ASCII strings. Returns when done.
  if (equality) {
    StringCompareStub::GenerateFlatAsciiStringEquals(
        masm, left, right, tmp1, tmp2);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(
        masm, left, right, tmp1, tmp2, tmp3, kScratchRegister);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ pop(tmp1);  // Return address.
  __ push(left);
  __ push(right);
  __ push(tmp1);
  if (equality) {
    __ TailCallRuntime(Runtime::kStringEquals, 2, 1);
  } else {
    __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateObjects(MacroAssembler* masm) {
  ASSERT(state_ == CompareIC::OBJECTS);
  Label miss;
  Condition either_smi = masm->CheckEitherSmi(rdx, rax);
  __ j(either_smi, &miss, Label::kNear);

  __ CmpObjectType(rax, JS_OBJECT_TYPE, rcx);
  __ j(not_equal, &miss, Label::kNear);
  __ CmpObjectType(rdx, JS_OBJECT_TYPE, rcx);
  __ j(not_equal, &miss, Label::kNear);

  ASSERT(GetCondition() == equal);
  __ subq(rax, rdx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateKnownObjects(MacroAssembler* masm) {
  Label miss;
  Condition either_smi = masm->CheckEitherSmi(rdx, rax);
  __ j(either_smi, &miss, Label::kNear);

  __ movq(rcx, FieldOperand(rax, HeapObject::kMapOffset));
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ Cmp(rcx, known_map_);
  __ j(not_equal, &miss, Label::kNear);
  __ Cmp(rbx, known_map_);
  __ j(not_equal, &miss, Label::kNear);

  __ subq(rax, rdx);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    ExternalReference miss =
        ExternalReference(IC_Utility(IC::kCompareIC_Miss), masm->isolate());

    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(rdx);
    __ push(rax);
    __ push(rdx);
    __ push(rax);
    __ Push(Smi::FromInt(op_));
    __ CallExternalReference(miss, 3);

    // Compute the entry point of the rewritten stub.
    __ lea(rdi, FieldOperand(rax, Code::kHeaderSize));
    __ pop(rax);
    __ pop(rdx);
  }

  // Do a tail call to the rewritten stub.
  __ jmp(rdi);
}


void StringDictionaryLookupStub::GenerateNegativeLookup(MacroAssembler* masm,
                                                        Label* miss,
                                                        Label* done,
                                                        Register properties,
                                                        Handle<String> name,
                                                        Register r0) {
  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the hole value).
  for (int i = 0; i < kInlinedProbes; i++) {
    // r0 points to properties hash.
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = r0;
    // Capacity is smi 2^n.
    __ SmiToInteger32(index, FieldOperand(properties, kCapacityOffset));
    __ decl(index);
    __ and_(index,
            Immediate(name->Hash() + StringDictionary::GetProbeOffset(i)));

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    __ lea(index, Operand(index, index, times_2, 0));  // index *= 3.

    Register entity_name = r0;
    // Having undefined at this place means the name is not contained.
    ASSERT_EQ(kSmiTagSize, 1);
    __ movq(entity_name, Operand(properties,
                                 index,
                                 times_pointer_size,
                                 kElementsStartOffset - kHeapObjectTag));
    __ Cmp(entity_name, masm->isolate()->factory()->undefined_value());
    __ j(equal, done);

    // Stop if found the property.
    __ Cmp(entity_name, Handle<String>(name));
    __ j(equal, miss);

    Label the_hole;
    // Check for the hole and skip.
    __ CompareRoot(entity_name, Heap::kTheHoleValueRootIndex);
    __ j(equal, &the_hole, Label::kNear);

    // Check if the entry name is not a symbol.
    __ movq(entity_name, FieldOperand(entity_name, HeapObject::kMapOffset));
    __ testb(FieldOperand(entity_name, Map::kInstanceTypeOffset),
             Immediate(kIsSymbolMask));
    __ j(zero, miss);

    __ bind(&the_hole);
  }

  StringDictionaryLookupStub stub(properties,
                                  r0,
                                  r0,
                                  StringDictionaryLookupStub::NEGATIVE_LOOKUP);
  __ Push(Handle<Object>(name));
  __ push(Immediate(name->Hash()));
  __ CallStub(&stub);
  __ testq(r0, r0);
  __ j(not_zero, miss);
  __ jmp(done);
}


// Probe the string dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found leaving the
// index into the dictionary in |r1|. Jump to the |miss| label
// otherwise.
void StringDictionaryLookupStub::GeneratePositiveLookup(MacroAssembler* masm,
                                                        Label* miss,
                                                        Label* done,
                                                        Register elements,
                                                        Register name,
                                                        Register r0,
                                                        Register r1) {
  ASSERT(!elements.is(r0));
  ASSERT(!elements.is(r1));
  ASSERT(!name.is(r0));
  ASSERT(!name.is(r1));

  // Assert that name contains a string.
  if (FLAG_debug_code) __ AbortIfNotString(name);

  __ SmiToInteger32(r0, FieldOperand(elements, kCapacityOffset));
  __ decl(r0);

  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ movl(r1, FieldOperand(name, String::kHashFieldOffset));
    __ shrl(r1, Immediate(String::kHashShift));
    if (i > 0) {
      __ addl(r1, Immediate(StringDictionary::GetProbeOffset(i)));
    }
    __ and_(r1, r0);

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    __ lea(r1, Operand(r1, r1, times_2, 0));  // r1 = r1 * 3

    // Check if the key is identical to the name.
    __ cmpq(name, Operand(elements, r1, times_pointer_size,
                          kElementsStartOffset - kHeapObjectTag));
    __ j(equal, done);
  }

  StringDictionaryLookupStub stub(elements,
                                  r0,
                                  r1,
                                  POSITIVE_LOOKUP);
  __ push(name);
  __ movl(r0, FieldOperand(name, String::kHashFieldOffset));
  __ shrl(r0, Immediate(String::kHashShift));
  __ push(r0);
  __ CallStub(&stub);

  __ testq(r0, r0);
  __ j(zero, miss);
  __ jmp(done);
}


void StringDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Stack frame on entry:
  //  esp[0 * kPointerSize]: return address.
  //  esp[1 * kPointerSize]: key's hash.
  //  esp[2 * kPointerSize]: key.
  // Registers:
  //  dictionary_: StringDictionary to probe.
  //  result_: used as scratch.
  //  index_: will hold an index of entry if lookup is successful.
  //          might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  Register scratch = result_;

  __ SmiToInteger32(scratch, FieldOperand(dictionary_, kCapacityOffset));
  __ decl(scratch);
  __ push(scratch);

  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the null value).
  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ movq(scratch, Operand(rsp, 2 * kPointerSize));
    if (i > 0) {
      __ addl(scratch, Immediate(StringDictionary::GetProbeOffset(i)));
    }
    __ and_(scratch, Operand(rsp, 0));

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    __ lea(index_, Operand(scratch, scratch, times_2, 0));  // index *= 3.

    // Having undefined at this place means the name is not contained.
    __ movq(scratch, Operand(dictionary_,
                             index_,
                             times_pointer_size,
                             kElementsStartOffset - kHeapObjectTag));

    __ Cmp(scratch, masm->isolate()->factory()->undefined_value());
    __ j(equal, &not_in_dictionary);

    // Stop if found the property.
    __ cmpq(scratch, Operand(rsp, 3 * kPointerSize));
    __ j(equal, &in_dictionary);

    if (i != kTotalProbes - 1 && mode_ == NEGATIVE_LOOKUP) {
      // If we hit a non symbol key during negative lookup
      // we have to bailout as this key might be equal to the
      // key we are looking for.

      // Check if the entry name is not a symbol.
      __ movq(scratch, FieldOperand(scratch, HeapObject::kMapOffset));
      __ testb(FieldOperand(scratch, Map::kInstanceTypeOffset),
               Immediate(kIsSymbolMask));
      __ j(zero, &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode_ == POSITIVE_LOOKUP) {
    __ movq(scratch, Immediate(0));
    __ Drop(1);
    __ ret(2 * kPointerSize);
  }

  __ bind(&in_dictionary);
  __ movq(scratch, Immediate(1));
  __ Drop(1);
  __ ret(2 * kPointerSize);

  __ bind(&not_in_dictionary);
  __ movq(scratch, Immediate(0));
  __ Drop(1);
  __ ret(2 * kPointerSize);
}


struct AheadOfTimeWriteBarrierStubList {
  Register object, value, address;
  RememberedSetAction action;
};


#define REG(Name) { kRegister_ ## Name ## _Code }

struct AheadOfTimeWriteBarrierStubList kAheadOfTime[] = {
  // Used in RegExpExecStub.
  { REG(rbx), REG(rax), REG(rdi), EMIT_REMEMBERED_SET },
  // Used in CompileArrayPushCall.
  { REG(rbx), REG(rcx), REG(rdx), EMIT_REMEMBERED_SET },
  // Used in CompileStoreGlobal.
  { REG(rbx), REG(rcx), REG(rdx), OMIT_REMEMBERED_SET },
  // Used in StoreStubCompiler::CompileStoreField and
  // KeyedStoreStubCompiler::CompileStoreField via GenerateStoreField.
  { REG(rdx), REG(rcx), REG(rbx), EMIT_REMEMBERED_SET },
  // GenerateStoreField calls the stub with two different permutations of
  // registers.  This is the second.
  { REG(rbx), REG(rcx), REG(rdx), EMIT_REMEMBERED_SET },
  // StoreIC::GenerateNormal via GenerateDictionaryStore.
  { REG(rbx), REG(r8), REG(r9), EMIT_REMEMBERED_SET },
  // KeyedStoreIC::GenerateGeneric.
  { REG(rbx), REG(rdx), REG(rcx), EMIT_REMEMBERED_SET},
  // KeyedStoreStubCompiler::GenerateStoreFastElement.
  { REG(rdi), REG(rbx), REG(rcx), EMIT_REMEMBERED_SET},
  { REG(rdx), REG(rdi), REG(rbx), EMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateSmiOnlyToObject
  // and ElementsTransitionGenerator::GenerateSmiOnlyToObject
  // and ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(rdx), REG(rbx), REG(rdi), EMIT_REMEMBERED_SET},
  { REG(rdx), REG(rbx), REG(rdi), OMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateSmiOnlyToDouble
  // and ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(rdx), REG(r11), REG(r15), EMIT_REMEMBERED_SET},
  // ElementsTransitionGenerator::GenerateDoubleToObject
  { REG(r11), REG(rax), REG(r15), EMIT_REMEMBERED_SET},
  // StoreArrayLiteralElementStub::Generate
  { REG(rbx), REG(rax), REG(rcx), EMIT_REMEMBERED_SET},
  // Null termination.
  { REG(no_reg), REG(no_reg), REG(no_reg), EMIT_REMEMBERED_SET}
};

#undef REG

bool RecordWriteStub::IsPregenerated() {
  for (AheadOfTimeWriteBarrierStubList* entry = kAheadOfTime;
       !entry->object.is(no_reg);
       entry++) {
    if (object_.is(entry->object) &&
        value_.is(entry->value) &&
        address_.is(entry->address) &&
        remembered_set_action_ == entry->action &&
        save_fp_regs_mode_ == kDontSaveFPRegs) {
      return true;
    }
  }
  return false;
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime() {
  StoreBufferOverflowStub stub1(kDontSaveFPRegs);
  stub1.GetCode()->set_is_pregenerated(true);
  StoreBufferOverflowStub stub2(kSaveFPRegs);
  stub2.GetCode()->set_is_pregenerated(true);
}


void RecordWriteStub::GenerateFixedRegStubsAheadOfTime() {
  for (AheadOfTimeWriteBarrierStubList* entry = kAheadOfTime;
       !entry->object.is(no_reg);
       entry++) {
    RecordWriteStub stub(entry->object,
                         entry->value,
                         entry->address,
                         entry->action,
                         kDontSaveFPRegs);
    stub.GetCode()->set_is_pregenerated(true);
  }
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
  // See RecordWriteStub::Patch for details.
  __ jmp(&skip_to_incremental_noncompacting, Label::kNear);
  __ jmp(&skip_to_incremental_compacting, Label::kFar);

  if (remembered_set_action_ == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
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

  if (remembered_set_action_ == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ movq(regs_.scratch0(), Operand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),
                           regs_.scratch0(),
                           &dont_need_remembered_set);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch0(),
                     1 << MemoryChunk::SCAN_ON_SCAVENGE,
                     not_zero,
                     &dont_need_remembered_set);

    // First notify the incremental marker if necessary, then update the
    // remembered set.
    CheckNeedsToInformIncrementalMarker(
        masm, kUpdateRememberedSetOnNoNeedToInformIncrementalMarker, mode);
    InformIncrementalMarker(masm, mode);
    regs_.Restore(masm);
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);

    __ bind(&dont_need_remembered_set);
  }

  CheckNeedsToInformIncrementalMarker(
      masm, kReturnOnNoNeedToInformIncrementalMarker, mode);
  InformIncrementalMarker(masm, mode);
  regs_.Restore(masm);
  __ ret(0);
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm, Mode mode) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode_);
#ifdef _WIN64
  Register arg3 = r8;
  Register arg2 = rdx;
  Register arg1 = rcx;
#else
  Register arg3 = rdx;
  Register arg2 = rsi;
  Register arg1 = rdi;
#endif
  Register address =
      arg1.is(regs_.address()) ? kScratchRegister : regs_.address();
  ASSERT(!address.is(regs_.object()));
  ASSERT(!address.is(arg1));
  __ Move(address, regs_.address());
  __ Move(arg1, regs_.object());
  if (mode == INCREMENTAL_COMPACTION) {
    // TODO(gc) Can we just set address arg2 in the beginning?
    __ Move(arg2, address);
  } else {
    ASSERT(mode == INCREMENTAL);
    __ movq(arg2, Operand(address, 0));
  }
  __ LoadAddress(arg3, ExternalReference::isolate_address());
  int argument_count = 3;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count);
  if (mode == INCREMENTAL_COMPACTION) {
    __ CallCFunction(
        ExternalReference::incremental_evacuation_record_write_function(
            masm->isolate()),
        argument_count);
  } else {
    ASSERT(mode == INCREMENTAL);
    __ CallCFunction(
        ExternalReference::incremental_marking_record_write_function(
            masm->isolate()),
        argument_count);
  }
  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode_);
}


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label on_black;
  Label need_incremental;
  Label need_incremental_pop_object;

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(),
                 regs_.scratch0(),
                 regs_.scratch1(),
                 &on_black,
                 Label::kNear);

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&on_black);

  // Get the value from the slot.
  __ movq(regs_.scratch0(), Operand(regs_.address(), 0));

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
                     zero,
                     &need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need an extra register for this, so we push the object register
  // temporarily.
  __ push(regs_.object());
  __ EnsureNotWhite(regs_.scratch0(),  // The value.
                    regs_.scratch1(),  // Scratch.
                    regs_.object(),  // Scratch.
                    &need_incremental_pop_object,
                    Label::kNear);
  __ pop(regs_.object());

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ ret(0);
  }

  __ bind(&need_incremental_pop_object);
  __ pop(regs_.object());

  __ bind(&need_incremental);

  // Fall through when we need to inform the incremental marker.
}


void StoreArrayLiteralElementStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : element value to store
  //  -- rbx    : array literal
  //  -- rdi    : map of array literal
  //  -- rcx    : element index as smi
  //  -- rdx    : array literal index in function
  //  -- rsp[0] : return address
  // -----------------------------------

  Label element_done;
  Label double_elements;
  Label smi_element;
  Label slow_elements;
  Label fast_elements;

  __ CheckFastElements(rdi, &double_elements);

  // FAST_SMI_ONLY_ELEMENTS or FAST_ELEMENTS
  __ JumpIfSmi(rax, &smi_element);
  __ CheckFastSmiOnlyElements(rdi, &fast_elements);

  // Store into the array literal requires a elements transition. Call into
  // the runtime.

  __ bind(&slow_elements);
  __ pop(rdi);  // Pop return address and remember to put back later for tail
                // call.
  __ push(rbx);
  __ push(rcx);
  __ push(rax);
  __ movq(rbx, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(rbx, JSFunction::kLiteralsOffset));
  __ push(rdx);
  __ push(rdi);  // Return return address so that tail call returns to right
                 // place.
  __ TailCallRuntime(Runtime::kStoreArrayLiteralElement, 5, 1);

  // Array literal has ElementsKind of FAST_ELEMENTS and value is an object.
  __ bind(&fast_elements);
  __ SmiToInteger32(kScratchRegister, rcx);
  __ movq(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
  __ lea(rcx, FieldOperand(rbx, kScratchRegister, times_pointer_size,
                           FixedArrayBase::kHeaderSize));
  __ movq(Operand(rcx, 0), rax);
  // Update the write barrier for the array store.
  __ RecordWrite(rbx, rcx, rax,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ ret(0);

  // Array literal has ElementsKind of FAST_SMI_ONLY_ELEMENTS or
  // FAST_ELEMENTS, and value is Smi.
  __ bind(&smi_element);
  __ SmiToInteger32(kScratchRegister, rcx);
  __ movq(rbx, FieldOperand(rbx, JSObject::kElementsOffset));
  __ movq(FieldOperand(rbx, kScratchRegister, times_pointer_size,
                       FixedArrayBase::kHeaderSize), rax);
  __ ret(0);

  // Array literal has ElementsKind of FAST_DOUBLE_ELEMENTS.
  __ bind(&double_elements);

  __ movq(r9, FieldOperand(rbx, JSObject::kElementsOffset));
  __ SmiToInteger32(r11, rcx);
  __ StoreNumberToDoubleElements(rax,
                                 r9,
                                 r11,
                                 xmm0,
                                 &slow_elements);
  __ ret(0);
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64

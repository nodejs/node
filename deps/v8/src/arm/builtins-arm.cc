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

#if defined(V8_TARGET_ARCH_ARM)

#include "codegen.h"
#include "debug.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "runtime.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  // ----------- S t a t e -------------
  //  -- r0                 : number of arguments excluding receiver
  //  -- r1                 : called function (only guaranteed when
  //                          extra_args requires it)
  //  -- cp                 : context
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument (argc == r0)
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ push(r1);
  } else {
    ASSERT(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects r0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ add(r0, r0, Operand(num_extra_args + 1));
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the native context.

  __ ldr(result,
         MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ ldr(result,
         FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the InternalArray function from the native context.
  __ ldr(result,
         MemOperand(result,
                    Context::SlotOffset(
                        Context::INTERNAL_ARRAY_FUNCTION_INDEX)));
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the native context.

  __ ldr(result,
         MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ ldr(result,
         FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the Array function from the native context.
  __ ldr(result,
         MemOperand(result,
                    Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
}


// Allocate an empty JSArray. The allocated array is put into the result
// register. An elements backing store is allocated with size initial_capacity
// and filled with the hole values.
static void AllocateEmptyJSArray(MacroAssembler* masm,
                                 Register array_function,
                                 Register result,
                                 Register scratch1,
                                 Register scratch2,
                                 Register scratch3,
                                 Label* gc_required) {
  const int initial_capacity = JSArray::kPreallocatedArrayElements;
  STATIC_ASSERT(initial_capacity >= 0);
  __ LoadInitialArrayMap(array_function, scratch2, scratch1, false);

  // Allocate the JSArray object together with space for a fixed array with the
  // requested elements.
  int size = JSArray::kSize;
  if (initial_capacity > 0) {
    size += FixedArray::SizeFor(initial_capacity);
  }
  __ AllocateInNewSpace(size,
                        result,
                        scratch2,
                        scratch3,
                        gc_required,
                        TAG_OBJECT);

  // Allocated the JSArray. Now initialize the fields except for the elements
  // array.
  // result: JSObject
  // scratch1: initial map
  // scratch2: start of next object
  __ str(scratch1, FieldMemOperand(result, JSObject::kMapOffset));
  __ LoadRoot(scratch1, Heap::kEmptyFixedArrayRootIndex);
  __ str(scratch1, FieldMemOperand(result, JSArray::kPropertiesOffset));
  // Field JSArray::kElementsOffset is initialized later.
  __ mov(scratch3,  Operand(0, RelocInfo::NONE));
  __ str(scratch3, FieldMemOperand(result, JSArray::kLengthOffset));

  if (initial_capacity == 0) {
    __ str(scratch1, FieldMemOperand(result, JSArray::kElementsOffset));
    return;
  }

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // scratch2: start of next object
  __ add(scratch1, result, Operand(JSArray::kSize));
  __ str(scratch1, FieldMemOperand(result, JSArray::kElementsOffset));

  // Clear the heap tag on the elements array.
  __ sub(scratch1, scratch1, Operand(kHeapObjectTag));

  // Initialize the FixedArray and fill it with holes. FixedArray length is
  // stored as a smi.
  // result: JSObject
  // scratch1: elements array (untagged)
  // scratch2: start of next object
  __ LoadRoot(scratch3, Heap::kFixedArrayMapRootIndex);
  STATIC_ASSERT(0 * kPointerSize == FixedArray::kMapOffset);
  __ str(scratch3, MemOperand(scratch1, kPointerSize, PostIndex));
  __ mov(scratch3,  Operand(Smi::FromInt(initial_capacity)));
  STATIC_ASSERT(1 * kPointerSize == FixedArray::kLengthOffset);
  __ str(scratch3, MemOperand(scratch1, kPointerSize, PostIndex));

  // Fill the FixedArray with the hole value. Inline the code if short.
  STATIC_ASSERT(2 * kPointerSize == FixedArray::kHeaderSize);
  __ LoadRoot(scratch3, Heap::kTheHoleValueRootIndex);
  static const int kLoopUnfoldLimit = 4;
  if (initial_capacity <= kLoopUnfoldLimit) {
    for (int i = 0; i < initial_capacity; i++) {
      __ str(scratch3, MemOperand(scratch1, kPointerSize, PostIndex));
    }
  } else {
    Label loop, entry;
    __ add(scratch2, scratch1, Operand(initial_capacity * kPointerSize));
    __ b(&entry);
    __ bind(&loop);
    __ str(scratch3, MemOperand(scratch1, kPointerSize, PostIndex));
    __ bind(&entry);
    __ cmp(scratch1, scratch2);
    __ b(lt, &loop);
  }
}

// Allocate a JSArray with the number of elements stored in a register. The
// register array_function holds the built-in Array function and the register
// array_size holds the size of the array as a smi. The allocated array is put
// into the result register and beginning and end of the FixedArray elements
// storage is put into registers elements_array_storage and elements_array_end
// (see  below for when that is not the case). If the parameter fill_with_holes
// is true the allocated elements backing store is filled with the hole values
// otherwise it is left uninitialized. When the backing store is filled the
// register elements_array_storage is scratched.
static void AllocateJSArray(MacroAssembler* masm,
                            Register array_function,  // Array function.
                            Register array_size,  // As a smi, cannot be 0.
                            Register result,
                            Register elements_array_storage,
                            Register elements_array_end,
                            Register scratch1,
                            Register scratch2,
                            bool fill_with_hole,
                            Label* gc_required) {
  // Load the initial map from the array function.
  __ LoadInitialArrayMap(array_function, scratch2,
                         elements_array_storage, fill_with_hole);

  if (FLAG_debug_code) {  // Assert that array size is not zero.
    __ tst(array_size, array_size);
    __ Assert(ne, "array size is unexpectedly 0");
  }

  // Allocate the JSArray object together with space for a FixedArray with the
  // requested number of elements.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ mov(elements_array_end,
         Operand((JSArray::kSize + FixedArray::kHeaderSize) / kPointerSize));
  __ add(elements_array_end,
         elements_array_end,
         Operand(array_size, ASR, kSmiTagSize));
  __ AllocateInNewSpace(
      elements_array_end,
      result,
      scratch1,
      scratch2,
      gc_required,
      static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));

  // Allocated the JSArray. Now initialize the fields except for the elements
  // array.
  // result: JSObject
  // elements_array_storage: initial map
  // array_size: size of array (smi)
  __ str(elements_array_storage, FieldMemOperand(result, JSObject::kMapOffset));
  __ LoadRoot(elements_array_storage, Heap::kEmptyFixedArrayRootIndex);
  __ str(elements_array_storage,
         FieldMemOperand(result, JSArray::kPropertiesOffset));
  // Field JSArray::kElementsOffset is initialized later.
  __ str(array_size, FieldMemOperand(result, JSArray::kLengthOffset));

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // array_size: size of array (smi)
  __ add(elements_array_storage, result, Operand(JSArray::kSize));
  __ str(elements_array_storage,
         FieldMemOperand(result, JSArray::kElementsOffset));

  // Clear the heap tag on the elements array.
  STATIC_ASSERT(kSmiTag == 0);
  __ sub(elements_array_storage,
         elements_array_storage,
         Operand(kHeapObjectTag));
  // Initialize the fixed array and fill it with holes. FixedArray length is
  // stored as a smi.
  // result: JSObject
  // elements_array_storage: elements array (untagged)
  // array_size: size of array (smi)
  __ LoadRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  ASSERT_EQ(0 * kPointerSize, FixedArray::kMapOffset);
  __ str(scratch1, MemOperand(elements_array_storage, kPointerSize, PostIndex));
  STATIC_ASSERT(kSmiTag == 0);
  ASSERT_EQ(1 * kPointerSize, FixedArray::kLengthOffset);
  __ str(array_size,
         MemOperand(elements_array_storage, kPointerSize, PostIndex));

  // Calculate elements array and elements array end.
  // result: JSObject
  // elements_array_storage: elements array element storage
  // array_size: smi-tagged size of elements array
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ add(elements_array_end,
         elements_array_storage,
         Operand(array_size, LSL, kPointerSizeLog2 - kSmiTagSize));

  // Fill the allocated FixedArray with the hole value if requested.
  // result: JSObject
  // elements_array_storage: elements array element storage
  // elements_array_end: start of next object
  if (fill_with_hole) {
    Label loop, entry;
    __ LoadRoot(scratch1, Heap::kTheHoleValueRootIndex);
    __ jmp(&entry);
    __ bind(&loop);
    __ str(scratch1,
           MemOperand(elements_array_storage, kPointerSize, PostIndex));
    __ bind(&entry);
    __ cmp(elements_array_storage, elements_array_end);
    __ b(lt, &loop);
  }
}

// Create a new array for the built-in Array function. This function allocates
// the JSArray object and the FixedArray elements array and initializes these.
// If the Array cannot be constructed in native code the runtime is called. This
// function assumes the following state:
//   r0: argc
//   r1: constructor (built-in Array function)
//   lr: return address
//   sp[0]: last argument
// This function is used for both construct and normal calls of Array. The only
// difference between handling a construct call and a normal call is that for a
// construct call the constructor function in r1 needs to be preserved for
// entering the generic code. In both cases argc in r0 needs to be preserved.
// Both registers are preserved by this code so no need to differentiate between
// construct call and normal call.
static void ArrayNativeCode(MacroAssembler* masm,
                            Label* call_generic_code) {
  Counters* counters = masm->isolate()->counters();
  Label argc_one_or_more, argc_two_or_more, not_empty_array, empty_array,
      has_non_smi_element, finish, cant_transition_map, not_double;

  // Check for array construction with zero arguments or one.
  __ cmp(r0, Operand(0, RelocInfo::NONE));
  __ b(ne, &argc_one_or_more);

  // Handle construction of an empty array.
  __ bind(&empty_array);
  AllocateEmptyJSArray(masm,
                       r1,
                       r2,
                       r3,
                       r4,
                       r5,
                       call_generic_code);
  __ IncrementCounter(counters->array_function_native(), 1, r3, r4);
  // Set up return value, remove receiver from stack and return.
  __ mov(r0, r2);
  __ add(sp, sp, Operand(kPointerSize));
  __ Jump(lr);

  // Check for one argument. Bail out if argument is not smi or if it is
  // negative.
  __ bind(&argc_one_or_more);
  __ cmp(r0, Operand(1));
  __ b(ne, &argc_two_or_more);
  STATIC_ASSERT(kSmiTag == 0);
  __ ldr(r2, MemOperand(sp));  // Get the argument from the stack.
  __ tst(r2, r2);
  __ b(ne, &not_empty_array);
  __ Drop(1);  // Adjust stack.
  __ mov(r0, Operand(0));  // Treat this as a call with argc of zero.
  __ b(&empty_array);

  __ bind(&not_empty_array);
  __ and_(r3, r2, Operand(kIntptrSignBit | kSmiTagMask), SetCC);
  __ b(ne, call_generic_code);

  // Handle construction of an empty array of a certain size. Bail out if size
  // is too large to actually allocate an elements array.
  STATIC_ASSERT(kSmiTag == 0);
  __ cmp(r2, Operand(JSObject::kInitialMaxFastElementArray << kSmiTagSize));
  __ b(ge, call_generic_code);

  // r0: argc
  // r1: constructor
  // r2: array_size (smi)
  // sp[0]: argument
  AllocateJSArray(masm,
                  r1,
                  r2,
                  r3,
                  r4,
                  r5,
                  r6,
                  r7,
                  true,
                  call_generic_code);
  __ IncrementCounter(counters->array_function_native(), 1, r2, r4);
  // Set up return value, remove receiver and argument from stack and return.
  __ mov(r0, r3);
  __ add(sp, sp, Operand(2 * kPointerSize));
  __ Jump(lr);

  // Handle construction of an array from a list of arguments.
  __ bind(&argc_two_or_more);
  __ mov(r2, Operand(r0, LSL, kSmiTagSize));  // Convet argc to a smi.

  // r0: argc
  // r1: constructor
  // r2: array_size (smi)
  // sp[0]: last argument
  AllocateJSArray(masm,
                  r1,
                  r2,
                  r3,
                  r4,
                  r5,
                  r6,
                  r7,
                  false,
                  call_generic_code);
  __ IncrementCounter(counters->array_function_native(), 1, r2, r6);

  // Fill arguments as array elements. Copy from the top of the stack (last
  // element) to the array backing store filling it backwards. Note:
  // elements_array_end points after the backing store therefore PreIndex is
  // used when filling the backing store.
  // r0: argc
  // r3: JSArray
  // r4: elements_array storage start (untagged)
  // r5: elements_array_end (untagged)
  // sp[0]: last argument
  Label loop, entry;
  __ mov(r7, sp);
  __ jmp(&entry);
  __ bind(&loop);
  __ ldr(r2, MemOperand(r7, kPointerSize, PostIndex));
  if (FLAG_smi_only_arrays) {
    __ JumpIfNotSmi(r2, &has_non_smi_element);
  }
  __ str(r2, MemOperand(r5, -kPointerSize, PreIndex));
  __ bind(&entry);
  __ cmp(r4, r5);
  __ b(lt, &loop);

  __ bind(&finish);
  __ mov(sp, r7);

  // Remove caller arguments and receiver from the stack, setup return value and
  // return.
  // r0: argc
  // r3: JSArray
  // sp[0]: receiver
  __ add(sp, sp, Operand(kPointerSize));
  __ mov(r0, r3);
  __ Jump(lr);

  __ bind(&has_non_smi_element);
  // Double values are handled by the runtime.
  __ CheckMap(
      r2, r9, Heap::kHeapNumberMapRootIndex, &not_double, DONT_DO_SMI_CHECK);
  __ bind(&cant_transition_map);
  __ UndoAllocationInNewSpace(r3, r4);
  __ b(call_generic_code);

  __ bind(&not_double);
  // Transition FAST_SMI_ELEMENTS to FAST_ELEMENTS.
  // r3: JSArray
  __ ldr(r2, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_ELEMENTS,
                                         r2,
                                         r9,
                                         &cant_transition_map);
  __ str(r2, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ RecordWriteField(r3,
                      HeapObject::kMapOffset,
                      r2,
                      r9,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  Label loop2;
  __ sub(r7, r7, Operand(kPointerSize));
  __ bind(&loop2);
  __ ldr(r2, MemOperand(r7, kPointerSize, PostIndex));
  __ str(r2, MemOperand(r5, -kPointerSize, PreIndex));
  __ cmp(r4, r5);
  __ b(lt, &loop2);
  __ b(&finish);
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r1);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ tst(r2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for InternalArray function");
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, "Unexpected initial map for InternalArray function");
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  ArrayNativeCode(masm, &generic_array_code);

  // Jump to the generic array code if the specialized code cannot handle the
  // construction.
  __ bind(&generic_array_code);

  Handle<Code> array_code =
      masm->isolate()->builtins()->InternalArrayCodeGeneric();
  __ Jump(array_code, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ tst(r2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for Array function");
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, "Unexpected initial map for Array function");
  }

  // Run the native code for the Array function called as a normal function.
  ArrayNativeCode(masm, &generic_array_code);

  // Jump to the generic array code if the specialized code cannot handle
  // the construction.
  __ bind(&generic_array_code);

  Handle<Code> array_code =
      masm->isolate()->builtins()->ArrayCodeGeneric();
  __ Jump(array_code, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArrayConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_constructor;

  if (FLAG_debug_code) {
    // The array construct code is only set for the builtin and internal
    // Array functions which always have a map.
    // Initial map for the builtin Array function should be a map.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
    __ tst(r2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for Array function");
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, "Unexpected initial map for Array function");
  }

  // Run the native code for the Array function called as a constructor.
  ArrayNativeCode(masm, &generic_constructor);

  // Jump to the generic construct code in case the specialized code cannot
  // handle the construction.
  __ bind(&generic_constructor);
  Handle<Code> generic_construct_stub =
      masm->isolate()->builtins()->JSConstructStubGeneric();
  __ Jump(generic_construct_stub, RelocInfo::CODE_TARGET);
}


void Builtins::Generate_StringConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_ctor_calls(), 1, r2, r3);

  Register function = r1;
  if (FLAG_debug_code) {
    __ LoadGlobalFunction(Context::STRING_FUNCTION_INDEX, r2);
    __ cmp(function, Operand(r2));
    __ Assert(eq, "Unexpected String function");
  }

  // Load the first arguments in r0 and get rid of the rest.
  Label no_arguments;
  __ cmp(r0, Operand(0, RelocInfo::NONE));
  __ b(eq, &no_arguments);
  // First args = sp[(argc - 1) * 4].
  __ sub(r0, r0, Operand(1));
  __ ldr(r0, MemOperand(sp, r0, LSL, kPointerSizeLog2, PreIndex));
  // sp now point to args[0], drop args[0] + receiver.
  __ Drop(2);

  Register argument = r2;
  Label not_cached, argument_is_string;
  NumberToStringStub::GenerateLookupNumberStringCache(
      masm,
      r0,        // Input.
      argument,  // Result.
      r3,        // Scratch.
      r4,        // Scratch.
      r5,        // Scratch.
      false,     // Is it a Smi?
      &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1, r3, r4);
  __ bind(&argument_is_string);

  // ----------- S t a t e -------------
  //  -- r2     : argument converted to string
  //  -- r1     : constructor function
  //  -- lr     : return address
  // -----------------------------------

  Label gc_required;
  __ AllocateInNewSpace(JSValue::kSize,
                        r0,  // Result.
                        r3,  // Scratch.
                        r4,  // Scratch.
                        &gc_required,
                        TAG_OBJECT);

  // Initialising the String Object.
  Register map = r3;
  __ LoadGlobalFunctionInitialMap(function, map, r4);
  if (FLAG_debug_code) {
    __ ldrb(r4, FieldMemOperand(map, Map::kInstanceSizeOffset));
    __ cmp(r4, Operand(JSValue::kSize >> kPointerSizeLog2));
    __ Assert(eq, "Unexpected string wrapper instance size");
    __ ldrb(r4, FieldMemOperand(map, Map::kUnusedPropertyFieldsOffset));
    __ cmp(r4, Operand(0, RelocInfo::NONE));
    __ Assert(eq, "Unexpected unused properties of string wrapper");
  }
  __ str(map, FieldMemOperand(r0, HeapObject::kMapOffset));

  __ LoadRoot(r3, Heap::kEmptyFixedArrayRootIndex);
  __ str(r3, FieldMemOperand(r0, JSObject::kPropertiesOffset));
  __ str(r3, FieldMemOperand(r0, JSObject::kElementsOffset));

  __ str(argument, FieldMemOperand(r0, JSValue::kValueOffset));

  // Ensure the object is fully initialized.
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);

  __ Ret();

  // The argument was not found in the number to string cache. Check
  // if it's a string already before calling the conversion builtin.
  Label convert_argument;
  __ bind(&not_cached);
  __ JumpIfSmi(r0, &convert_argument);

  // Is it a String?
  __ ldr(r2, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ ldrb(r3, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ tst(r3, Operand(kIsNotStringMask));
  __ b(ne, &convert_argument);
  __ mov(argument, r0);
  __ IncrementCounter(counters->string_ctor_conversions(), 1, r3, r4);
  __ b(&argument_is_string);

  // Invoke the conversion builtin and put the result into r2.
  __ bind(&convert_argument);
  __ push(function);  // Preserve the function.
  __ IncrementCounter(counters->string_ctor_conversions(), 1, r3, r4);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(r0);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
  }
  __ pop(function);
  __ mov(argument, r0);
  __ b(&argument_is_string);

  // Load the empty string into r2, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ bind(&no_arguments);
  __ LoadRoot(argument, Heap::kEmptyStringRootIndex);
  __ Drop(1);
  __ b(&argument_is_string);

  // At this point the argument is already a string. Call runtime to
  // create a string wrapper.
  __ bind(&gc_required);
  __ IncrementCounter(counters->string_ctor_gc_required(), 1, r3, r4);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(argument);
    __ CallRuntime(Runtime::kNewStringWrapper, 1);
  }
  __ Ret();
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kCodeOffset));
  __ add(r2, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ mov(pc, r2);
}


void Builtins::Generate_InRecompileQueue(MacroAssembler* masm) {
  GenerateTailCallToSharedCode(masm);
}


void Builtins::Generate_ParallelRecompile(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push a copy of the function onto the stack.
    __ push(r1);
    // Push call kind information.
    __ push(r5);

    __ push(r1);  // Function is also the parameter to the runtime call.
    __ CallRuntime(Runtime::kParallelRecompile, 1);

    // Restore call kind information.
    __ pop(r5);
    // Restore receiver.
    __ pop(r1);

    // Tear down internal frame.
  }

  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool count_constructions) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Should never count constructions for api objects.
  ASSERT(!is_api_function || !count_constructions);

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the two incoming parameters on the stack.
    __ mov(r0, Operand(r0, LSL, kSmiTagSize));
    __ push(r0);  // Smi-tagged arguments count.
    __ push(r1);  // Constructor function.

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      Label undo_allocation;
#ifdef ENABLE_DEBUGGER_SUPPORT
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ mov(r2, Operand(debug_step_in_fp));
      __ ldr(r2, MemOperand(r2));
      __ tst(r2, r2);
      __ b(ne, &rt_call);
#endif

      // Load the initial map and verify that it is in fact a map.
      // r1: constructor function
      __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(r2, &rt_call);
      __ CompareObjectType(r2, r3, r4, MAP_TYPE);
      __ b(ne, &rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // r1: constructor function
      // r2: initial map
      __ CompareInstanceType(r2, r3, JS_FUNCTION_TYPE);
      __ b(eq, &rt_call);

      if (count_constructions) {
        Label allocate;
        // Decrease generous allocation count.
        __ ldr(r3, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
        MemOperand constructor_count =
            FieldMemOperand(r3, SharedFunctionInfo::kConstructionCountOffset);
        __ ldrb(r4, constructor_count);
        __ sub(r4, r4, Operand(1), SetCC);
        __ strb(r4, constructor_count);
        __ b(ne, &allocate);

        __ Push(r1, r2);

        __ push(r1);  // constructor
        // The call will replace the stub, so the countdown is only done once.
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ pop(r2);
        __ pop(r1);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // r1: constructor function
      // r2: initial map
      __ ldrb(r3, FieldMemOperand(r2, Map::kInstanceSizeOffset));
      __ AllocateInNewSpace(r3, r4, r5, r6, &rt_call, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // r1: constructor function
      // r2: initial map
      // r3: object size
      // r4: JSObject (not tagged)
      __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
      __ mov(r5, r4);
      ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
      __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
      ASSERT_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
      __ str(r6, MemOperand(r5, kPointerSize, PostIndex));
      ASSERT_EQ(2 * kPointerSize, JSObject::kElementsOffset);
      __ str(r6, MemOperand(r5, kPointerSize, PostIndex));

      // Fill all the in-object properties with the appropriate filler.
      // r1: constructor function
      // r2: initial map
      // r3: object size (in words)
      // r4: JSObject (not tagged)
      // r5: First in-object property of JSObject (not tagged)
      __ add(r6, r4, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
      ASSERT_EQ(3 * kPointerSize, JSObject::kHeaderSize);
      __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);
      if (count_constructions) {
        __ ldr(r0, FieldMemOperand(r2, Map::kInstanceSizesOffset));
        __ Ubfx(r0, r0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
                kBitsPerByte);
        __ add(r0, r5, Operand(r0, LSL, kPointerSizeLog2));
        // r0: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ cmp(r0, r6);
          __ Assert(le, "Unexpected number of pre-allocated property fields.");
        }
        __ InitializeFieldsWithFiller(r5, r0, r7);
        // To allow for truncation.
        __ LoadRoot(r7, Heap::kOnePointerFillerMapRootIndex);
      }
      __ InitializeFieldsWithFiller(r5, r6, r7);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on. Any
      // failures need to undo the allocation, so that the heap is in a
      // consistent state and verifiable.
      __ add(r4, r4, Operand(kHeapObjectTag));

      // Check if a non-empty properties array is needed. Continue with
      // allocated object if not fall through to runtime call if it is.
      // r1: constructor function
      // r4: JSObject
      // r5: start of next object (not tagged)
      __ ldrb(r3, FieldMemOperand(r2, Map::kUnusedPropertyFieldsOffset));
      // The field instance sizes contains both pre-allocated property fields
      // and in-object properties.
      __ ldr(r0, FieldMemOperand(r2, Map::kInstanceSizesOffset));
      __ Ubfx(r6, r0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
              kBitsPerByte);
      __ add(r3, r3, Operand(r6));
      __ Ubfx(r6, r0, Map::kInObjectPropertiesByte * kBitsPerByte,
              kBitsPerByte);
      __ sub(r3, r3, Operand(r6), SetCC);

      // Done if no extra properties are to be allocated.
      __ b(eq, &allocated);
      __ Assert(pl, "Property allocation count failed.");

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // r1: constructor
      // r3: number of elements in properties array
      // r4: JSObject
      // r5: start of next object
      __ add(r0, r3, Operand(FixedArray::kHeaderSize / kPointerSize));
      __ AllocateInNewSpace(
          r0,
          r5,
          r6,
          r2,
          &undo_allocation,
          static_cast<AllocationFlags>(RESULT_CONTAINS_TOP | SIZE_IN_WORDS));

      // Initialize the FixedArray.
      // r1: constructor
      // r3: number of elements in properties array
      // r4: JSObject
      // r5: FixedArray (not tagged)
      __ LoadRoot(r6, Heap::kFixedArrayMapRootIndex);
      __ mov(r2, r5);
      ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
      __ str(r6, MemOperand(r2, kPointerSize, PostIndex));
      ASSERT_EQ(1 * kPointerSize, FixedArray::kLengthOffset);
      __ mov(r0, Operand(r3, LSL, kSmiTagSize));
      __ str(r0, MemOperand(r2, kPointerSize, PostIndex));

      // Initialize the fields to undefined.
      // r1: constructor function
      // r2: First element of FixedArray (not tagged)
      // r3: number of elements in properties array
      // r4: JSObject
      // r5: FixedArray (not tagged)
      __ add(r6, r2, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
      ASSERT_EQ(2 * kPointerSize, FixedArray::kHeaderSize);
      { Label loop, entry;
        if (count_constructions) {
          __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);
        } else if (FLAG_debug_code) {
          __ LoadRoot(r8, Heap::kUndefinedValueRootIndex);
          __ cmp(r7, r8);
          __ Assert(eq, "Undefined value not loaded.");
        }
        __ b(&entry);
        __ bind(&loop);
        __ str(r7, MemOperand(r2, kPointerSize, PostIndex));
        __ bind(&entry);
        __ cmp(r2, r6);
        __ b(lt, &loop);
      }

      // Store the initialized FixedArray into the properties field of
      // the JSObject
      // r1: constructor function
      // r4: JSObject
      // r5: FixedArray (not tagged)
      __ add(r5, r5, Operand(kHeapObjectTag));  // Add the heap tag.
      __ str(r5, FieldMemOperand(r4, JSObject::kPropertiesOffset));

      // Continue with JSObject being successfully allocated
      // r1: constructor function
      // r4: JSObject
      __ jmp(&allocated);

      // Undo the setting of the new top so that the heap is verifiable. For
      // example, the map's unused properties potentially do not match the
      // allocated objects unused properties.
      // r4: JSObject (previous new top)
      __ bind(&undo_allocation);
      __ UndoAllocationInNewSpace(r4, r5);
    }

    // Allocate the new receiver object using the runtime call.
    // r1: constructor function
    __ bind(&rt_call);
    __ push(r1);  // argument for Runtime_NewObject
    __ CallRuntime(Runtime::kNewObject, 1);
    __ mov(r4, r0);

    // Receiver for constructor call allocated.
    // r4: JSObject
    __ bind(&allocated);
    __ push(r4);
    __ push(r4);

    // Reload the number of arguments and the constructor from the stack.
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: constructor function
    // sp[3]: number of arguments (smi-tagged)
    __ ldr(r1, MemOperand(sp, 2 * kPointerSize));
    __ ldr(r3, MemOperand(sp, 3 * kPointerSize));

    // Set up pointer to last argument.
    __ add(r2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Set up number of arguments for function call below
    __ mov(r0, Operand(r3, LSR, kSmiTagSize));

    // Copy arguments and receiver to the expression stack.
    // r0: number of arguments
    // r1: constructor function
    // r2: address of last argument (caller sp)
    // r3: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: constructor function
    // sp[3]: number of arguments (smi-tagged)
    Label loop, entry;
    __ b(&entry);
    __ bind(&loop);
    __ ldr(ip, MemOperand(r2, r3, LSL, kPointerSizeLog2 - 1));
    __ push(ip);
    __ bind(&entry);
    __ sub(r3, r3, Operand(2), SetCC);
    __ b(ge, &loop);

    // Call the function.
    // r0: number of arguments
    // r1: constructor function
    if (is_api_function) {
      __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      ParameterCount expected(0);
      __ InvokeCode(code, expected, expected,
                    RelocInfo::CODE_TARGET, CALL_FUNCTION, CALL_AS_METHOD);
    } else {
      ParameterCount actual(r0);
      __ InvokeFunction(r1, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function && !count_constructions) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    // r0: result
    // sp[0]: receiver
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    // r0: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ JumpIfSmi(r0, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    __ CompareObjectType(r0, r3, r3, FIRST_SPEC_OBJECT_TYPE);
    __ b(ge, &exit);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ ldr(r0, MemOperand(sp));

    // Remove receiver from the stack, remove caller arguments, and
    // return.
    __ bind(&exit);
    // r0: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ ldr(r1, MemOperand(sp, 2 * kPointerSize));

    // Leave construct frame.
  }

  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - 1));
  __ add(sp, sp, Operand(kPointerSize));
  __ IncrementCounter(isolate->counters()->constructed_objects(), 1, r1, r2);
  __ Jump(lr);
}


void Builtins::Generate_JSConstructStubCountdown(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, true);
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r0: code entry
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  // r5-r7, cp may be clobbered

  // Clear the context before we push it when entering the internal frame.
  __ mov(cp, Operand(0, RelocInfo::NONE));

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Set up the context from the function argument.
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ push(r1);
    __ push(r2);

    // Copy arguments to the stack in a loop.
    // r1: function
    // r3: argc
    // r4: argv, i.e. points to first arg
    Label loop, entry;
    __ add(r2, r4, Operand(r3, LSL, kPointerSizeLog2));
    // r2 points past last arg.
    __ b(&entry);
    __ bind(&loop);
    __ ldr(r0, MemOperand(r4, kPointerSize, PostIndex));  // read next parameter
    __ ldr(r0, MemOperand(r0));  // dereference handle
    __ push(r0);  // push parameter
    __ bind(&entry);
    __ cmp(r4, r2);
    __ b(ne, &loop);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mov(r5, Operand(r4));
    __ mov(r6, Operand(r4));
    __ mov(r7, Operand(r4));
    if (kR9Available == 1) {
      __ mov(r9, Operand(r4));
    }

    // Invoke the code and pass argc as r0.
    __ mov(r0, Operand(r3));
    if (is_construct) {
      CallConstructStub stub(NO_CALL_FUNCTION_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(r0);
      __ InvokeFunction(r1, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }
    // Exit the JS frame and remove the parameters (except function), and
    // return.
    // Respect ABI stack constraint.
  }
  __ Jump(lr);

  // r0: result
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_LazyCompile(MacroAssembler* masm) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve the function.
    __ push(r1);
    // Push call kind information.
    __ push(r5);

    // Push the function on the stack as the argument to the runtime function.
    __ push(r1);
    __ CallRuntime(Runtime::kLazyCompile, 1);
    // Calculate the entry point.
    __ add(r2, r0, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Restore call kind information.
    __ pop(r5);
    // Restore saved function.
    __ pop(r1);

    // Tear down internal frame.
  }

  // Do a tail-call of the compiled function.
  __ Jump(r2);
}


void Builtins::Generate_LazyRecompile(MacroAssembler* masm) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve the function.
    __ push(r1);
    // Push call kind information.
    __ push(r5);

    // Push the function on the stack as the argument to the runtime function.
    __ push(r1);
    __ CallRuntime(Runtime::kLazyRecompile, 1);
    // Calculate the entry point.
    __ add(r2, r0, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Restore call kind information.
    __ pop(r5);
    // Restore saved function.
    __ pop(r1);

    // Tear down internal frame.
  }

  // Do a tail-call of the compiled function.
  __ Jump(r2);
}


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass the function and deoptimization type to the runtime system.
    __ mov(r0, Operand(Smi::FromInt(static_cast<int>(type))));
    __ push(r0);
    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
  }

  // Get the full codegen state from the stack and untag it -> r6.
  __ ldr(r6, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(r6);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ cmp(r6, Operand(FullCodeGenerator::NO_REGISTERS));
  __ b(ne, &with_tos_register);
  __ add(sp, sp, Operand(1 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&with_tos_register);
  __ ldr(r0, MemOperand(sp, 1 * kPointerSize));
  __ cmp(r6, Operand(FullCodeGenerator::TOS_REG));
  __ b(ne, &unknown_state);
  __ add(sp, sp, Operand(2 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&unknown_state);
  __ stop("no cases left");
}


void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


void Builtins::Generate_NotifyOSR(MacroAssembler* masm) {
  // For now, we are relying on the fact that Runtime::NotifyOSR
  // doesn't do any garbage collection which allows us to save/restore
  // the registers without worrying about which of them contain
  // pointers. This seems a bit fragile.
  __ stm(db_w, sp, kJSCallerSaved | kCalleeSaved | lr.bit() | fp.bit());
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyOSR, 0);
  }
  __ ldm(ia_w, sp, kJSCallerSaved | kCalleeSaved | lr.bit() | fp.bit());
  __ Ret();
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  CpuFeatures::TryForceFeatureScope scope(VFP3);
  if (!CPU::SupportsCrankshaft()) {
    __ Abort("Unreachable code: Cannot optimize without VFP3 support.");
    return;
  }

  // Lookup the function in the JavaScript frame and push it as an
  // argument to the on-stack replacement function.
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(r0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the result was -1 it means that we couldn't optimize the
  // function. Just return and continue in the unoptimized version.
  Label skip;
  __ cmp(r0, Operand(Smi::FromInt(-1)));
  __ b(ne, &skip);
  __ Ret();

  __ bind(&skip);
  // Untag the AST id and push it on the stack.
  __ SmiUntag(r0);
  __ push(r0);

  // Generate the code for doing the frame-to-frame translation using
  // the deoptimizer infrastructure.
  Deoptimizer::EntryGenerator generator(masm, Deoptimizer::OSR);
  generator.Generate();
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r0: actual number of arguments
  { Label done;
    __ cmp(r0, Operand(0));
    __ b(ne, &done);
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ push(r2);
    __ add(r0, r0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack, check
  //    if it is a function.
  // r0: actual number of arguments
  Label slow, non_function;
  __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  __ JumpIfSmi(r1, &non_function);
  __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
  __ b(ne, &slow);

  // 3a. Patch the first argument if necessary when calling a function.
  // r0: actual number of arguments
  // r1: function
  Label shift_arguments;
  __ mov(r4, Operand(0, RelocInfo::NONE));  // indicate regular JS_FUNCTION
  { Label convert_to_object, use_global_receiver, patch_receiver;
    // Change context eagerly in case we need the global receiver.
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r3, FieldMemOperand(r2, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(r3, Operand(1 << (SharedFunctionInfo::kStrictModeFunction +
                             kSmiTagSize)));
    __ b(ne, &shift_arguments);

    // Do not transform the receiver for native (Compilerhints already in r3).
    __ tst(r3, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
    __ b(ne, &shift_arguments);

    // Compute the receiver in non-strict mode.
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ ldr(r2, MemOperand(r2, -kPointerSize));
    // r0: actual number of arguments
    // r1: function
    // r2: first argument
    __ JumpIfSmi(r2, &convert_to_object);

    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ cmp(r2, r3);
    __ b(eq, &use_global_receiver);
    __ LoadRoot(r3, Heap::kNullValueRootIndex);
    __ cmp(r2, r3);
    __ b(eq, &use_global_receiver);

    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CompareObjectType(r2, r3, r3, FIRST_SPEC_OBJECT_TYPE);
    __ b(ge, &shift_arguments);

    __ bind(&convert_to_object);

    {
      // Enter an internal frame in order to preserve argument count.
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ mov(r0, Operand(r0, LSL, kSmiTagSize));  // Smi-tagged.
      __ push(r0);

      __ push(r2);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ mov(r2, r0);

      __ pop(r0);
      __ mov(r0, Operand(r0, ASR, kSmiTagSize));

      // Exit the internal frame.
    }

    // Restore the function to r1, and the flag to r4.
    __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    __ mov(r4, Operand(0, RelocInfo::NONE));
    __ jmp(&patch_receiver);

    // Use the global receiver object from the called function as the
    // receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
    __ ldr(r2, FieldMemOperand(cp, kGlobalIndex));
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kNativeContextOffset));
    __ ldr(r2, FieldMemOperand(r2, kGlobalIndex));
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ add(r3, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r2, MemOperand(r3, -kPointerSize));

    __ jmp(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ mov(r4, Operand(1, RelocInfo::NONE));  // indicate function proxy
  __ cmp(r2, Operand(JS_FUNCTION_PROXY_TYPE));
  __ b(eq, &shift_arguments);
  __ bind(&non_function);
  __ mov(r4, Operand(2, RelocInfo::NONE));  // indicate non-function

  // 3c. Patch the first argument when calling a non-function.  The
  //     CALL_NON_FUNCTION builtin expects the non-function callee as
  //     receiver, so overwrite the first argument which will ultimately
  //     become the receiver.
  // r0: actual number of arguments
  // r1: function
  // r4: call type (0: JS function, 1: function proxy, 2: non-function)
  __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));
  __ str(r1, MemOperand(r2, -kPointerSize));

  // 4. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r0: actual number of arguments
  // r1: function
  // r4: call type (0: JS function, 1: function proxy, 2: non-function)
  __ bind(&shift_arguments);
  { Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));

    __ bind(&loop);
    __ ldr(ip, MemOperand(r2, -kPointerSize));
    __ str(ip, MemOperand(r2));
    __ sub(r2, r2, Operand(kPointerSize));
    __ cmp(r2, sp);
    __ b(ne, &loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ sub(r0, r0, Operand(1));
    __ pop();
  }

  // 5a. Call non-function via tail call to CALL_NON_FUNCTION builtin,
  //     or a function proxy via CALL_FUNCTION_PROXY.
  // r0: actual number of arguments
  // r1: function
  // r4: call type (0: JS function, 1: function proxy, 2: non-function)
  { Label function, non_proxy;
    __ tst(r4, r4);
    __ b(eq, &function);
    // Expected number of arguments is 0 for CALL_NON_FUNCTION.
    __ mov(r2, Operand(0, RelocInfo::NONE));
    __ SetCallKind(r5, CALL_AS_METHOD);
    __ cmp(r4, Operand(1));
    __ b(ne, &non_proxy);

    __ push(r1);  // re-add proxy object as additional argument
    __ add(r0, r0, Operand(1));
    __ GetBuiltinEntry(r3, Builtins::CALL_FUNCTION_PROXY);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    __ bind(&non_proxy);
    __ GetBuiltinEntry(r3, Builtins::CALL_NON_FUNCTION);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
    __ bind(&function);
  }

  // 5b. Get the code to call from the function and check that the number of
  //     expected arguments matches what we're providing.  If so, jump
  //     (tail-call) to the code in register edx without checking arguments.
  // r0: actual number of arguments
  // r1: function
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2,
         FieldMemOperand(r3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ mov(r2, Operand(r2, ASR, kSmiTagSize));
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ SetCallKind(r5, CALL_AS_METHOD);
  __ cmp(r2, r0);  // Check formal and actual parameter counts.
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET,
          ne);

  ParameterCount expected(0);
  __ InvokeCode(r3, expected, expected, JUMP_FUNCTION,
                NullCallWrapper(), CALL_AS_METHOD);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  const int kIndexOffset    = -5 * kPointerSize;
  const int kLimitOffset    = -4 * kPointerSize;
  const int kArgsOffset     =  2 * kPointerSize;
  const int kRecvOffset     =  3 * kPointerSize;
  const int kFunctionOffset =  4 * kPointerSize;

  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);

    __ ldr(r0, MemOperand(fp, kFunctionOffset));  // get the function
    __ push(r0);
    __ ldr(r0, MemOperand(fp, kArgsOffset));  // get the args array
    __ push(r0);
    __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);

    // Check the stack for overflow. We are not trying to catch
    // interruptions (e.g. debug break and preemption) here, so the "real stack
    // limit" is checked.
    Label okay;
    __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
    // Make r2 the space we have left. The stack might already be overflowed
    // here which will cause r2 to become negative.
    __ sub(r2, sp, r2);
    // Check if the arguments will overflow the stack.
    __ cmp(r2, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
    __ b(gt, &okay);  // Signed comparison.

    // Out of stack space.
    __ ldr(r1, MemOperand(fp, kFunctionOffset));
    __ push(r1);
    __ push(r0);
    __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_FUNCTION);
    // End of stack check.

    // Push current limit and index.
    __ bind(&okay);
    __ push(r0);  // limit
    __ mov(r1, Operand(0, RelocInfo::NONE));  // initial index
    __ push(r1);

    // Get the receiver.
    __ ldr(r0, MemOperand(fp, kRecvOffset));

    // Check that the function is a JS function (otherwise it must be a proxy).
    Label push_receiver;
    __ ldr(r1, MemOperand(fp, kFunctionOffset));
    __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
    __ b(ne, &push_receiver);

    // Change context eagerly to get the right global object if necessary.
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
    // Load the shared function info while the function is still in r1.
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));

    // Compute the receiver.
    // Do not transform the receiver for strict mode functions.
    Label call_to_object, use_global_receiver;
    __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(r2, Operand(1 << (SharedFunctionInfo::kStrictModeFunction +
                             kSmiTagSize)));
    __ b(ne, &push_receiver);

    // Do not transform the receiver for strict mode functions.
    __ tst(r2, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
    __ b(ne, &push_receiver);

    // Compute the receiver in non-strict mode.
    __ JumpIfSmi(r0, &call_to_object);
    __ LoadRoot(r1, Heap::kNullValueRootIndex);
    __ cmp(r0, r1);
    __ b(eq, &use_global_receiver);
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ cmp(r0, r1);
    __ b(eq, &use_global_receiver);

    // Check if the receiver is already a JavaScript object.
    // r0: receiver
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CompareObjectType(r0, r1, r1, FIRST_SPEC_OBJECT_TYPE);
    __ b(ge, &push_receiver);

    // Convert the receiver to a regular object.
    // r0: receiver
    __ bind(&call_to_object);
    __ push(r0);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ b(&push_receiver);

    // Use the current global receiver object as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalOffset =
        Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
    __ ldr(r0, FieldMemOperand(cp, kGlobalOffset));
    __ ldr(r0, FieldMemOperand(r0, GlobalObject::kNativeContextOffset));
    __ ldr(r0, FieldMemOperand(r0, kGlobalOffset));
    __ ldr(r0, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));

    // Push the receiver.
    // r0: receiver
    __ bind(&push_receiver);
    __ push(r0);

    // Copy all arguments from the array to the stack.
    Label entry, loop;
    __ ldr(r0, MemOperand(fp, kIndexOffset));
    __ b(&entry);

    // Load the current argument from the arguments array and push it to the
    // stack.
    // r0: current argument index
    __ bind(&loop);
    __ ldr(r1, MemOperand(fp, kArgsOffset));
    __ push(r1);
    __ push(r0);

    // Call the runtime to access the property in the arguments array.
    __ CallRuntime(Runtime::kGetProperty, 2);
    __ push(r0);

    // Use inline caching to access the arguments.
    __ ldr(r0, MemOperand(fp, kIndexOffset));
    __ add(r0, r0, Operand(1 << kSmiTagSize));
    __ str(r0, MemOperand(fp, kIndexOffset));

    // Test if the copy loop has finished copying all the elements from the
    // arguments object.
    __ bind(&entry);
    __ ldr(r1, MemOperand(fp, kLimitOffset));
    __ cmp(r0, r1);
    __ b(ne, &loop);

    // Invoke the function.
    Label call_proxy;
    ParameterCount actual(r0);
    __ mov(r0, Operand(r0, ASR, kSmiTagSize));
    __ ldr(r1, MemOperand(fp, kFunctionOffset));
    __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
    __ b(ne, &call_proxy);
    __ InvokeFunction(r1, actual, CALL_FUNCTION,
                      NullCallWrapper(), CALL_AS_METHOD);

    frame_scope.GenerateLeaveFrame();
    __ add(sp, sp, Operand(3 * kPointerSize));
    __ Jump(lr);

    // Invoke the function proxy.
    __ bind(&call_proxy);
    __ push(r1);  // add function proxy as last argument
    __ add(r0, r0, Operand(1));
    __ mov(r2, Operand(0, RelocInfo::NONE));
    __ SetCallKind(r5, CALL_AS_METHOD);
    __ GetBuiltinEntry(r3, Builtins::CALL_FUNCTION_PROXY);
    __ Call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    // Tear down the internal frame and remove function, receiver and args.
  }
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Jump(lr);
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));
  __ mov(r4, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ stm(db_w, sp, r0.bit() | r1.bit() | r4.bit() | fp.bit() | lr.bit());
  __ add(fp, sp, Operand(3 * kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ldr(r1, MemOperand(fp, -3 * kPointerSize));
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ add(sp, sp, Operand(kPointerSize));  // adjust for receiver
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : actual number of arguments
  //  -- r1 : function (passed through to callee)
  //  -- r2 : expected number of arguments
  //  -- r3 : code entry to call
  //  -- r5 : call kind information
  // -----------------------------------

  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ cmp(r0, r2);
  __ b(lt, &too_few);
  __ cmp(r2, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ b(eq, &dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address into r2.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ add(r0, fp, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
    // adjust for return address and receiver
    __ add(r0, r0, Operand(2 * kPointerSize));
    __ sub(r2, r0, Operand(r2, LSL, kPointerSizeLog2));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: copy end address
    // r3: code entry to call

    Label copy;
    __ bind(&copy);
    __ ldr(ip, MemOperand(r0, 0));
    __ push(ip);
    __ cmp(r0, r2);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address is fp.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ add(r0, fp, Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ ldr(ip, MemOperand(r0, 2 * kPointerSize));
    __ push(ip);
    __ cmp(r0, fp);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    // Fill the remaining expected arguments with undefined.
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ sub(r2, fp, Operand(r2, LSL, kPointerSizeLog2));
    __ sub(r2, r2, Operand(4 * kPointerSize));  // Adjust for frame.

    Label fill;
    __ bind(&fill);
    __ push(ip);
    __ cmp(sp, r2);
    __ b(ne, &fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ Call(r3);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Jump(lr);


  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ Jump(r3);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

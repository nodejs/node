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

#if defined(V8_TARGET_ARCH_MIPS)

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
  //  -- a0                 : number of arguments excluding receiver
  //  -- a1                 : called function (only guaranteed when
  //  --                      extra_args requires it)
  //  -- cp                 : context
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * agrc]       : receiver
  // -----------------------------------

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ push(a1);
  } else {
    ASSERT(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects s0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ Addu(s0, a0, num_extra_args + 1);
  __ sll(s1, s0, kPointerSizeLog2);
  __ Subu(s1, s1, kPointerSize);
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the global context.

  __ lw(result, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ lw(result,
        FieldMemOperand(result, GlobalObject::kGlobalContextOffset));
  // Load the InternalArray function from the global context.
  __ lw(result,
         MemOperand(result,
                    Context::SlotOffset(
                        Context::INTERNAL_ARRAY_FUNCTION_INDEX)));
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the global context.

  __ lw(result, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ lw(result,
        FieldMemOperand(result, GlobalObject::kGlobalContextOffset));
  // Load the Array function from the global context.
  __ lw(result,
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
  __ LoadInitialArrayMap(array_function, scratch2, scratch1);

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
  __ sw(scratch1, FieldMemOperand(result, JSObject::kMapOffset));
  __ LoadRoot(scratch1, Heap::kEmptyFixedArrayRootIndex);
  __ sw(scratch1, FieldMemOperand(result, JSArray::kPropertiesOffset));
  // Field JSArray::kElementsOffset is initialized later.
  __ mov(scratch3,  zero_reg);
  __ sw(scratch3, FieldMemOperand(result, JSArray::kLengthOffset));

  if (initial_capacity == 0) {
    __ sw(scratch1, FieldMemOperand(result, JSArray::kElementsOffset));
    return;
  }

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // scratch2: start of next object
  __ Addu(scratch1, result, Operand(JSArray::kSize));
  __ sw(scratch1, FieldMemOperand(result, JSArray::kElementsOffset));

  // Clear the heap tag on the elements array.
  __ And(scratch1, scratch1, Operand(~kHeapObjectTagMask));

  // Initialize the FixedArray and fill it with holes. FixedArray length is
  // stored as a smi.
  // result: JSObject
  // scratch1: elements array (untagged)
  // scratch2: start of next object
  __ LoadRoot(scratch3, Heap::kFixedArrayMapRootIndex);
  STATIC_ASSERT(0 * kPointerSize == FixedArray::kMapOffset);
  __ sw(scratch3, MemOperand(scratch1));
  __ Addu(scratch1, scratch1, kPointerSize);
  __ li(scratch3,  Operand(Smi::FromInt(initial_capacity)));
  STATIC_ASSERT(1 * kPointerSize == FixedArray::kLengthOffset);
  __ sw(scratch3, MemOperand(scratch1));
  __ Addu(scratch1, scratch1, kPointerSize);

  // Fill the FixedArray with the hole value. Inline the code if short.
  STATIC_ASSERT(2 * kPointerSize == FixedArray::kHeaderSize);
  __ LoadRoot(scratch3, Heap::kTheHoleValueRootIndex);
  static const int kLoopUnfoldLimit = 4;
  if (initial_capacity <= kLoopUnfoldLimit) {
    for (int i = 0; i < initial_capacity; i++) {
      __ sw(scratch3, MemOperand(scratch1, i * kPointerSize));
    }
  } else {
    Label loop, entry;
    __ Addu(scratch2, scratch1, Operand(initial_capacity * kPointerSize));
    __ Branch(&entry);
    __ bind(&loop);
    __ sw(scratch3, MemOperand(scratch1));
    __ Addu(scratch1, scratch1, kPointerSize);
    __ bind(&entry);
    __ Branch(&loop, lt, scratch1, Operand(scratch2));
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
  __ LoadInitialArrayMap(array_function, scratch2, elements_array_storage);

  if (FLAG_debug_code) {  // Assert that array size is not zero.
    __ Assert(
        ne, "array size is unexpectedly 0", array_size, Operand(zero_reg));
  }

  // Allocate the JSArray object together with space for a FixedArray with the
  // requested number of elements.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  __ li(elements_array_end,
        (JSArray::kSize + FixedArray::kHeaderSize) / kPointerSize);
  __ sra(scratch1, array_size, kSmiTagSize);
  __ Addu(elements_array_end, elements_array_end, scratch1);
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
  __ sw(elements_array_storage, FieldMemOperand(result, JSObject::kMapOffset));
  __ LoadRoot(elements_array_storage, Heap::kEmptyFixedArrayRootIndex);
  __ sw(elements_array_storage,
         FieldMemOperand(result, JSArray::kPropertiesOffset));
  // Field JSArray::kElementsOffset is initialized later.
  __ sw(array_size, FieldMemOperand(result, JSArray::kLengthOffset));

  // Calculate the location of the elements array and set elements array member
  // of the JSArray.
  // result: JSObject
  // array_size: size of array (smi)
  __ Addu(elements_array_storage, result, Operand(JSArray::kSize));
  __ sw(elements_array_storage,
         FieldMemOperand(result, JSArray::kElementsOffset));

  // Clear the heap tag on the elements array.
  __ And(elements_array_storage,
          elements_array_storage,
          Operand(~kHeapObjectTagMask));
  // Initialize the fixed array and fill it with holes. FixedArray length is
  // stored as a smi.
  // result: JSObject
  // elements_array_storage: elements array (untagged)
  // array_size: size of array (smi)
  __ LoadRoot(scratch1, Heap::kFixedArrayMapRootIndex);
  ASSERT_EQ(0 * kPointerSize, FixedArray::kMapOffset);
  __ sw(scratch1, MemOperand(elements_array_storage));
  __ Addu(elements_array_storage, elements_array_storage, kPointerSize);

  // Length of the FixedArray is the number of pre-allocated elements if
  // the actual JSArray has length 0 and the size of the JSArray for non-empty
  // JSArrays. The length of a FixedArray is stored as a smi.
  STATIC_ASSERT(kSmiTag == 0);

  ASSERT_EQ(1 * kPointerSize, FixedArray::kLengthOffset);
  __ sw(array_size, MemOperand(elements_array_storage));
  __ Addu(elements_array_storage, elements_array_storage, kPointerSize);

  // Calculate elements array and elements array end.
  // result: JSObject
  // elements_array_storage: elements array element storage
  // array_size: smi-tagged size of elements array
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ sll(elements_array_end, array_size, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(elements_array_end, elements_array_storage, elements_array_end);

  // Fill the allocated FixedArray with the hole value if requested.
  // result: JSObject
  // elements_array_storage: elements array element storage
  // elements_array_end: start of next object
  if (fill_with_hole) {
    Label loop, entry;
    __ LoadRoot(scratch1, Heap::kTheHoleValueRootIndex);
    __ Branch(&entry);
    __ bind(&loop);
    __ sw(scratch1, MemOperand(elements_array_storage));
    __ Addu(elements_array_storage, elements_array_storage, kPointerSize);

    __ bind(&entry);
    __ Branch(&loop, lt, elements_array_storage, Operand(elements_array_end));
  }
}


// Create a new array for the built-in Array function. This function allocates
// the JSArray object and the FixedArray elements array and initializes these.
// If the Array cannot be constructed in native code the runtime is called. This
// function assumes the following state:
//   a0: argc
//   a1: constructor (built-in Array function)
//   ra: return address
//   sp[0]: last argument
// This function is used for both construct and normal calls of Array. The only
// difference between handling a construct call and a normal call is that for a
// construct call the constructor function in a1 needs to be preserved for
// entering the generic code. In both cases argc in a0 needs to be preserved.
// Both registers are preserved by this code so no need to differentiate between
// construct call and normal call.
static void ArrayNativeCode(MacroAssembler* masm,
                            Label* call_generic_code) {
  Counters* counters = masm->isolate()->counters();
  Label argc_one_or_more, argc_two_or_more, not_empty_array, empty_array,
      has_non_smi_element, finish, cant_transition_map, not_double;

  // Check for array construction with zero arguments or one.
  __ Branch(&argc_one_or_more, ne, a0, Operand(zero_reg));
  // Handle construction of an empty array.
  __ bind(&empty_array);
  AllocateEmptyJSArray(masm,
                       a1,
                       a2,
                       a3,
                       t0,
                       t1,
                       call_generic_code);
  __ IncrementCounter(counters->array_function_native(), 1, a3, t0);
  // Set up return value, remove receiver from stack and return.
  __ mov(v0, a2);
  __ Addu(sp, sp, Operand(kPointerSize));
  __ Ret();

  // Check for one argument. Bail out if argument is not smi or if it is
  // negative.
  __ bind(&argc_one_or_more);
  __ Branch(&argc_two_or_more, ne, a0, Operand(1));

  STATIC_ASSERT(kSmiTag == 0);
  __ lw(a2, MemOperand(sp));  // Get the argument from the stack.
  __ Branch(&not_empty_array, ne, a2, Operand(zero_reg));
  __ Drop(1);  // Adjust stack.
  __ mov(a0, zero_reg);  // Treat this as a call with argc of zero.
  __ Branch(&empty_array);

  __ bind(&not_empty_array);
  __ And(a3, a2, Operand(kIntptrSignBit | kSmiTagMask));
  __ Branch(call_generic_code, eq, a3, Operand(zero_reg));

  // Handle construction of an empty array of a certain size. Bail out if size
  // is too large to actually allocate an elements array.
  STATIC_ASSERT(kSmiTag == 0);
  __ Branch(call_generic_code, Ugreater_equal, a2,
            Operand(JSObject::kInitialMaxFastElementArray << kSmiTagSize));

  // a0: argc
  // a1: constructor
  // a2: array_size (smi)
  // sp[0]: argument
  AllocateJSArray(masm,
                  a1,
                  a2,
                  a3,
                  t0,
                  t1,
                  t2,
                  t3,
                  true,
                  call_generic_code);
  __ IncrementCounter(counters->array_function_native(), 1, a2, t0);

  // Set up return value, remove receiver and argument from stack and return.
  __ mov(v0, a3);
  __ Addu(sp, sp, Operand(2 * kPointerSize));
  __ Ret();

  // Handle construction of an array from a list of arguments.
  __ bind(&argc_two_or_more);
  __ sll(a2, a0, kSmiTagSize);  // Convert argc to a smi.

  // a0: argc
  // a1: constructor
  // a2: array_size (smi)
  // sp[0]: last argument
  AllocateJSArray(masm,
                  a1,
                  a2,
                  a3,
                  t0,
                  t1,
                  t2,
                  t3,
                  false,
                  call_generic_code);
  __ IncrementCounter(counters->array_function_native(), 1, a2, t2);

  // Fill arguments as array elements. Copy from the top of the stack (last
  // element) to the array backing store filling it backwards. Note:
  // elements_array_end points after the backing store.
  // a0: argc
  // a3: JSArray
  // t0: elements_array storage start (untagged)
  // t1: elements_array_end (untagged)
  // sp[0]: last argument

  Label loop, entry;
  __ Branch(USE_DELAY_SLOT, &entry);
  __ mov(t3, sp);
  __ bind(&loop);
  __ lw(a2, MemOperand(t3));
  if (FLAG_smi_only_arrays) {
    __ JumpIfNotSmi(a2, &has_non_smi_element);
  }
  __ Addu(t3, t3, kPointerSize);
  __ Addu(t1, t1, -kPointerSize);
  __ sw(a2, MemOperand(t1));
  __ bind(&entry);
  __ Branch(&loop, lt, t0, Operand(t1));

  __ bind(&finish);
  __ mov(sp, t3);

  // Remove caller arguments and receiver from the stack, setup return value and
  // return.
  // a0: argc
  // a3: JSArray
  // sp[0]: receiver
  __ Addu(sp, sp, Operand(kPointerSize));
  __ mov(v0, a3);
  __ Ret();

  __ bind(&has_non_smi_element);
  // Double values are handled by the runtime.
  __ CheckMap(
      a2, t5, Heap::kHeapNumberMapRootIndex, &not_double, DONT_DO_SMI_CHECK);
  __ bind(&cant_transition_map);
  __ UndoAllocationInNewSpace(a3, t0);
  __ Branch(call_generic_code);

  __ bind(&not_double);
  // Transition FAST_SMI_ONLY_ELEMENTS to FAST_ELEMENTS.
  // a3: JSArray
  __ lw(a2, FieldMemOperand(a3, HeapObject::kMapOffset));
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ONLY_ELEMENTS,
                                         FAST_ELEMENTS,
                                         a2,
                                         t5,
                                         &cant_transition_map);
  __ sw(a2, FieldMemOperand(a3, HeapObject::kMapOffset));
  __ RecordWriteField(a3,
                      HeapObject::kMapOffset,
                      a2,
                      t5,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  Label loop2;
  __ bind(&loop2);
  __ lw(a2, MemOperand(t3));
  __ Addu(t3, t3, kPointerSize);
  __ Subu(t1, t1, kPointerSize);
  __ sw(a2, MemOperand(t1));
  __ Branch(&loop2, lt, t0, Operand(t1));
  __ Branch(&finish);
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, a1);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ And(t0, a2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for InternalArray function",
              t0, Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, "Unexpected initial map for InternalArray function",
              t0, Operand(MAP_TYPE));
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
  //  -- a0     : number of arguments
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, a1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ And(t0, a2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for Array function (1)",
              t0, Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, "Unexpected initial map for Array function (2)",
              t0, Operand(MAP_TYPE));
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
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_constructor;

  if (FLAG_debug_code) {
    // The array construct code is only set for the builtin and internal
    // Array functions which always have a map.
    // Initial map for the builtin Array function should be a map.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ And(t0, a2, Operand(kSmiTagMask));
    __ Assert(ne, "Unexpected initial map for Array function (3)",
              t0, Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, "Unexpected initial map for Array function (4)",
              t0, Operand(MAP_TYPE));
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
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_ctor_calls(), 1, a2, a3);

  Register function = a1;
  if (FLAG_debug_code) {
    __ LoadGlobalFunction(Context::STRING_FUNCTION_INDEX, a2);
    __ Assert(eq, "Unexpected String function", function, Operand(a2));
  }

  // Load the first arguments in a0 and get rid of the rest.
  Label no_arguments;
  __ Branch(&no_arguments, eq, a0, Operand(zero_reg));
  // First args = sp[(argc - 1) * 4].
  __ Subu(a0, a0, Operand(1));
  __ sll(a0, a0, kPointerSizeLog2);
  __ Addu(sp, a0, sp);
  __ lw(a0, MemOperand(sp));
  // sp now point to args[0], drop args[0] + receiver.
  __ Drop(2);

  Register argument = a2;
  Label not_cached, argument_is_string;
  NumberToStringStub::GenerateLookupNumberStringCache(
      masm,
      a0,        // Input.
      argument,  // Result.
      a3,        // Scratch.
      t0,        // Scratch.
      t1,        // Scratch.
      false,     // Is it a Smi?
      &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1, a3, t0);
  __ bind(&argument_is_string);

  // ----------- S t a t e -------------
  //  -- a2     : argument converted to string
  //  -- a1     : constructor function
  //  -- ra     : return address
  // -----------------------------------

  Label gc_required;
  __ AllocateInNewSpace(JSValue::kSize,
                        v0,  // Result.
                        a3,  // Scratch.
                        t0,  // Scratch.
                        &gc_required,
                        TAG_OBJECT);

  // Initialising the String Object.
  Register map = a3;
  __ LoadGlobalFunctionInitialMap(function, map, t0);
  if (FLAG_debug_code) {
    __ lbu(t0, FieldMemOperand(map, Map::kInstanceSizeOffset));
    __ Assert(eq, "Unexpected string wrapper instance size",
        t0, Operand(JSValue::kSize >> kPointerSizeLog2));
    __ lbu(t0, FieldMemOperand(map, Map::kUnusedPropertyFieldsOffset));
    __ Assert(eq, "Unexpected unused properties of string wrapper",
        t0, Operand(zero_reg));
  }
  __ sw(map, FieldMemOperand(v0, HeapObject::kMapOffset));

  __ LoadRoot(a3, Heap::kEmptyFixedArrayRootIndex);
  __ sw(a3, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sw(a3, FieldMemOperand(v0, JSObject::kElementsOffset));

  __ sw(argument, FieldMemOperand(v0, JSValue::kValueOffset));

  // Ensure the object is fully initialized.
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);

  __ Ret();

  // The argument was not found in the number to string cache. Check
  // if it's a string already before calling the conversion builtin.
  Label convert_argument;
  __ bind(&not_cached);
  __ JumpIfSmi(a0, &convert_argument);

  // Is it a String?
  __ lw(a2, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ lbu(a3, FieldMemOperand(a2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ And(t0, a3, Operand(kIsNotStringMask));
  __ Branch(&convert_argument, ne, t0, Operand(zero_reg));
  __ mov(argument, a0);
  __ IncrementCounter(counters->string_ctor_conversions(), 1, a3, t0);
  __ Branch(&argument_is_string);

  // Invoke the conversion builtin and put the result into a2.
  __ bind(&convert_argument);
  __ push(function);  // Preserve the function.
  __ IncrementCounter(counters->string_ctor_conversions(), 1, a3, t0);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(v0);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
  }
  __ pop(function);
  __ mov(argument, v0);
  __ Branch(&argument_is_string);

  // Load the empty string into a2, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ bind(&no_arguments);
  __ LoadRoot(argument, Heap::kEmptyStringRootIndex);
  __ Drop(1);
  __ Branch(&argument_is_string);

  // At this point the argument is already a string. Call runtime to
  // create a string wrapper.
  __ bind(&gc_required);
  __ IncrementCounter(counters->string_ctor_gc_required(), 1, a3, t0);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(argument);
    __ CallRuntime(Runtime::kNewStringWrapper, 1);
  }
  __ Ret();
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool count_constructions) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Should never count constructions for api objects.
  ASSERT(!is_api_function || !count_constructions);

  Isolate* isolate = masm->isolate();

  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the two incoming parameters on the stack.
    __ sll(a0, a0, kSmiTagSize);  // Tag arguments count.
    __ MultiPushReversed(a0.bit() | a1.bit());

    // Use t7 to hold undefined, which is used in several places below.
    __ LoadRoot(t7, Heap::kUndefinedValueRootIndex);

    Label rt_call, allocated;
    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    if (FLAG_inline_new) {
      Label undo_allocation;
#ifdef ENABLE_DEBUGGER_SUPPORT
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ li(a2, Operand(debug_step_in_fp));
      __ lw(a2, MemOperand(a2));
      __ Branch(&rt_call, ne, a2, Operand(zero_reg));
#endif

      // Load the initial map and verify that it is in fact a map.
      // a1: constructor function
      __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(a2, &rt_call);
      __ GetObjectType(a2, a3, t4);
      __ Branch(&rt_call, ne, t4, Operand(MAP_TYPE));

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // a1: constructor function
      // a2: initial map
      __ lbu(a3, FieldMemOperand(a2, Map::kInstanceTypeOffset));
      __ Branch(&rt_call, eq, a3, Operand(JS_FUNCTION_TYPE));

      if (count_constructions) {
        Label allocate;
        // Decrease generous allocation count.
        __ lw(a3, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
        MemOperand constructor_count =
           FieldMemOperand(a3, SharedFunctionInfo::kConstructionCountOffset);
        __ lbu(t0, constructor_count);
        __ Subu(t0, t0, Operand(1));
        __ sb(t0, constructor_count);
        __ Branch(&allocate, ne, t0, Operand(zero_reg));

        __ Push(a1, a2);

        __ push(a1);  // Constructor.
        // The call will replace the stub, so the countdown is only done once.
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ pop(a2);
        __ pop(a1);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // a1: constructor function
      // a2: initial map
      __ lbu(a3, FieldMemOperand(a2, Map::kInstanceSizeOffset));
      __ AllocateInNewSpace(a3, t4, t5, t6, &rt_call, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // a1: constructor function
      // a2: initial map
      // a3: object size
      // t4: JSObject (not tagged)
      __ LoadRoot(t6, Heap::kEmptyFixedArrayRootIndex);
      __ mov(t5, t4);
      __ sw(a2, MemOperand(t5, JSObject::kMapOffset));
      __ sw(t6, MemOperand(t5, JSObject::kPropertiesOffset));
      __ sw(t6, MemOperand(t5, JSObject::kElementsOffset));
      __ Addu(t5, t5, Operand(3*kPointerSize));
      ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
      ASSERT_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
      ASSERT_EQ(2 * kPointerSize, JSObject::kElementsOffset);

      // Fill all the in-object properties with appropriate filler.
      // a1: constructor function
      // a2: initial map
      // a3: object size (in words)
      // t4: JSObject (not tagged)
      // t5: First in-object property of JSObject (not tagged)
      __ sll(t0, a3, kPointerSizeLog2);
      __ addu(t6, t4, t0);   // End of object.
      ASSERT_EQ(3 * kPointerSize, JSObject::kHeaderSize);
      __ LoadRoot(t7, Heap::kUndefinedValueRootIndex);
      if (count_constructions) {
        __ lw(a0, FieldMemOperand(a2, Map::kInstanceSizesOffset));
        __ Ext(a0, a0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
                kBitsPerByte);
        __ sll(t0, a0, kPointerSizeLog2);
        __ addu(a0, t5, t0);
        // a0: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ Assert(le, "Unexpected number of pre-allocated property fields.",
              a0, Operand(t6));
        }
        __ InitializeFieldsWithFiller(t5, a0, t7);
        // To allow for truncation.
        __ LoadRoot(t7, Heap::kOnePointerFillerMapRootIndex);
      }
      __ InitializeFieldsWithFiller(t5, t6, t7);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on. Any
      // failures need to undo the allocation, so that the heap is in a
      // consistent state and verifiable.
      __ Addu(t4, t4, Operand(kHeapObjectTag));

      // Check if a non-empty properties array is needed. Continue with
      // allocated object if not fall through to runtime call if it is.
      // a1: constructor function
      // t4: JSObject
      // t5: start of next object (not tagged)
      __ lbu(a3, FieldMemOperand(a2, Map::kUnusedPropertyFieldsOffset));
      // The field instance sizes contains both pre-allocated property fields
      // and in-object properties.
      __ lw(a0, FieldMemOperand(a2, Map::kInstanceSizesOffset));
      __ Ext(t6, a0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
             kBitsPerByte);
      __ Addu(a3, a3, Operand(t6));
      __ Ext(t6, a0, Map::kInObjectPropertiesByte * kBitsPerByte,
              kBitsPerByte);
      __ subu(a3, a3, t6);

      // Done if no extra properties are to be allocated.
      __ Branch(&allocated, eq, a3, Operand(zero_reg));
      __ Assert(greater_equal, "Property allocation count failed.",
          a3, Operand(zero_reg));

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // a1: constructor
      // a3: number of elements in properties array
      // t4: JSObject
      // t5: start of next object
      __ Addu(a0, a3, Operand(FixedArray::kHeaderSize / kPointerSize));
      __ AllocateInNewSpace(
          a0,
          t5,
          t6,
          a2,
          &undo_allocation,
          static_cast<AllocationFlags>(RESULT_CONTAINS_TOP | SIZE_IN_WORDS));

      // Initialize the FixedArray.
      // a1: constructor
      // a3: number of elements in properties array (untagged)
      // t4: JSObject
      // t5: start of next object
      __ LoadRoot(t6, Heap::kFixedArrayMapRootIndex);
      __ mov(a2, t5);
      __ sw(t6, MemOperand(a2, JSObject::kMapOffset));
      __ sll(a0, a3, kSmiTagSize);
      __ sw(a0, MemOperand(a2, FixedArray::kLengthOffset));
      __ Addu(a2, a2, Operand(2 * kPointerSize));

      ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
      ASSERT_EQ(1 * kPointerSize, FixedArray::kLengthOffset);

      // Initialize the fields to undefined.
      // a1: constructor
      // a2: First element of FixedArray (not tagged)
      // a3: number of elements in properties array
      // t4: JSObject
      // t5: FixedArray (not tagged)
      __ sll(t3, a3, kPointerSizeLog2);
      __ addu(t6, a2, t3);  // End of object.
      ASSERT_EQ(2 * kPointerSize, FixedArray::kHeaderSize);
      { Label loop, entry;
        if (count_constructions) {
          __ LoadRoot(t7, Heap::kUndefinedValueRootIndex);
        } else if (FLAG_debug_code) {
          __ LoadRoot(t8, Heap::kUndefinedValueRootIndex);
          __ Assert(eq, "Undefined value not loaded.", t7, Operand(t8));
        }
        __ jmp(&entry);
        __ bind(&loop);
        __ sw(t7, MemOperand(a2));
        __ addiu(a2, a2, kPointerSize);
        __ bind(&entry);
        __ Branch(&loop, less, a2, Operand(t6));
      }

      // Store the initialized FixedArray into the properties field of
      // the JSObject.
      // a1: constructor function
      // t4: JSObject
      // t5: FixedArray (not tagged)
      __ Addu(t5, t5, Operand(kHeapObjectTag));  // Add the heap tag.
      __ sw(t5, FieldMemOperand(t4, JSObject::kPropertiesOffset));

      // Continue with JSObject being successfully allocated.
      // a1: constructor function
      // a4: JSObject
      __ jmp(&allocated);

      // Undo the setting of the new top so that the heap is verifiable. For
      // example, the map's unused properties potentially do not match the
      // allocated objects unused properties.
      // t4: JSObject (previous new top)
      __ bind(&undo_allocation);
      __ UndoAllocationInNewSpace(t4, t5);
    }

    __ bind(&rt_call);
    // Allocate the new receiver object using the runtime call.
    // a1: constructor function
    __ push(a1);  // Argument for Runtime_NewObject.
    __ CallRuntime(Runtime::kNewObject, 1);
    __ mov(t4, v0);

    // Receiver for constructor call allocated.
    // t4: JSObject
    __ bind(&allocated);
    __ push(t4);
    __ push(t4);

    // Reload the number of arguments from the stack.
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: constructor function
    // sp[3]: number of arguments (smi-tagged)
    __ lw(a1, MemOperand(sp, 2 * kPointerSize));
    __ lw(a3, MemOperand(sp, 3 * kPointerSize));

    // Set up pointer to last argument.
    __ Addu(a2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Set up number of arguments for function call below.
    __ srl(a0, a3, kSmiTagSize);

    // Copy arguments and receiver to the expression stack.
    // a0: number of arguments
    // a1: constructor function
    // a2: address of last argument (caller sp)
    // a3: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: constructor function
    // sp[3]: number of arguments (smi-tagged)
    Label loop, entry;
    __ jmp(&entry);
    __ bind(&loop);
    __ sll(t0, a3, kPointerSizeLog2 - kSmiTagSize);
    __ Addu(t0, a2, Operand(t0));
    __ lw(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Addu(a3, a3, Operand(-2));
    __ Branch(&loop, greater_equal, a3, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments
    // a1: constructor function
    if (is_api_function) {
      __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      ParameterCount expected(0);
      __ InvokeCode(code, expected, expected,
                    RelocInfo::CODE_TARGET, CALL_FUNCTION, CALL_AS_METHOD);
    } else {
      ParameterCount actual(a0);
      __ InvokeFunction(a1, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function && !count_constructions) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    // v0: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ JumpIfSmi(v0, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    __ GetObjectType(v0, a3, a3);
    __ Branch(&exit, greater_equal, a3, Operand(FIRST_SPEC_OBJECT_TYPE));

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ lw(v0, MemOperand(sp));

    // Remove receiver from the stack, remove caller arguments, and
    // return.
    __ bind(&exit);
    // v0: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ lw(a1, MemOperand(sp, 2 * kPointerSize));

    // Leave construct frame.
  }

  __ sll(t0, a1, kPointerSizeLog2 - 1);
  __ Addu(sp, sp, t0);
  __ Addu(sp, sp, kPointerSize);
  __ IncrementCounter(isolate->counters()->constructed_objects(), 1, a1, a2);
  __ Ret();
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
  // Called from JSEntryStub::GenerateBody

  // ----------- S t a t e -------------
  //  -- a0: code entry
  //  -- a1: function
  //  -- a2: receiver_pointer
  //  -- a3: argc
  //  -- s0: argv
  // -----------------------------------

  // Clear the context before we push it when entering the JS frame.
  __ mov(cp, zero_reg);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Set up the context from the function argument.
    __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

    // Push the function and the receiver onto the stack.
    __ Push(a1, a2);

    // Copy arguments to the stack in a loop.
    // a3: argc
    // s0: argv, i.e. points to first arg
    Label loop, entry;
    __ sll(t0, a3, kPointerSizeLog2);
    __ addu(t2, s0, t0);
    __ b(&entry);
    __ nop();   // Branch delay slot nop.
    // t2 points past last arg.
    __ bind(&loop);
    __ lw(t0, MemOperand(s0));  // Read next parameter.
    __ addiu(s0, s0, kPointerSize);
    __ lw(t0, MemOperand(t0));  // Dereference handle.
    __ push(t0);  // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, s0, Operand(t2));

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ mov(s1, t0);
    __ mov(s2, t0);
    __ mov(s3, t0);
    __ mov(s4, t0);
    __ mov(s5, t0);
    // s6 holds the root address. Do not clobber.
    // s7 is cp. Do not init.

    // Invoke the code and pass argc as a0.
    __ mov(a0, a3);
    if (is_construct) {
      CallConstructStub stub(NO_CALL_FUNCTION_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(a0);
      __ InvokeFunction(a1, actual, CALL_FUNCTION,
                        NullCallWrapper(), CALL_AS_METHOD);
    }

    // Leave internal frame.
  }

  __ Jump(ra);
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
    __ push(a1);
    // Push call kind information.
    __ push(t1);

    // Push the function on the stack as the argument to the runtime function.
    __ push(a1);
    // Call the runtime function.
    __ CallRuntime(Runtime::kLazyCompile, 1);
    // Calculate the entry point.
    __ addiu(t9, v0, Code::kHeaderSize - kHeapObjectTag);

    // Restore call kind information.
    __ pop(t1);
    // Restore saved function.
    __ pop(a1);

    // Tear down temporary frame.
  }

  // Do a tail-call of the compiled function.
  __ Jump(t9);
}


void Builtins::Generate_LazyRecompile(MacroAssembler* masm) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve the function.
    __ push(a1);
    // Push call kind information.
    __ push(t1);

    // Push the function on the stack as the argument to the runtime function.
    __ push(a1);
    __ CallRuntime(Runtime::kLazyRecompile, 1);
    // Calculate the entry point.
    __ Addu(t9, v0, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Restore call kind information.
    __ pop(t1);
    // Restore saved function.
    __ pop(a1);

    // Tear down temporary frame.
  }

  // Do a tail-call of the compiled function.
  __ Jump(t9);
}


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass the function and deoptimization type to the runtime system.
    __ li(a0, Operand(Smi::FromInt(static_cast<int>(type))));
    __ push(a0);
    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
  }

  // Get the full codegen state from the stack and untag it -> t2.
  __ lw(t2, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(t2);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ Branch(&with_tos_register,
            ne, t2, Operand(FullCodeGenerator::NO_REGISTERS));
  __ Addu(sp, sp, Operand(1 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&with_tos_register);
  __ lw(v0, MemOperand(sp, 1 * kPointerSize));
  __ Branch(&unknown_state, ne, t2, Operand(FullCodeGenerator::TOS_REG));

  __ Addu(sp, sp, Operand(2 * kPointerSize));  // Remove state.
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
  RegList saved_regs =
      (kJSCallerSaved | kCalleeSaved | ra.bit() | fp.bit()) & ~sp.bit();
  __ MultiPush(saved_regs);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyOSR, 0);
  }
  __ MultiPop(saved_regs);
  __ Ret();
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  CpuFeatures::TryForceFeatureScope scope(VFP3);
  if (!CpuFeatures::IsSupported(FPU)) {
    __ Abort("Unreachable code: Cannot optimize without FPU support.");
    return;
  }

  // Lookup the function in the JavaScript frame and push it as an
  // argument to the on-stack replacement function.
  __ lw(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(a0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the result was -1 it means that we couldn't optimize the
  // function. Just return and continue in the unoptimized version.
  __ Ret(eq, v0, Operand(Smi::FromInt(-1)));

  // Untag the AST id and push it on the stack.
  __ SmiUntag(v0);
  __ push(v0);

  // Generate the code for doing the frame-to-frame translation using
  // the deoptimizer infrastructure.
  Deoptimizer::EntryGenerator generator(masm, Deoptimizer::OSR);
  generator.Generate();
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // a0: actual number of arguments
  { Label done;
    __ Branch(&done, ne, a0, Operand(zero_reg));
    __ LoadRoot(t2, Heap::kUndefinedValueRootIndex);
    __ push(t2);
    __ Addu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack, check
  //    if it is a function.
  // a0: actual number of arguments
  Label slow, non_function;
  __ sll(at, a0, kPointerSizeLog2);
  __ addu(at, sp, at);
  __ lw(a1, MemOperand(at));
  __ JumpIfSmi(a1, &non_function);
  __ GetObjectType(a1, a2, a2);
  __ Branch(&slow, ne, a2, Operand(JS_FUNCTION_TYPE));

  // 3a. Patch the first argument if necessary when calling a function.
  // a0: actual number of arguments
  // a1: function
  Label shift_arguments;
  __ li(t0, Operand(0, RelocInfo::NONE));  // Indicate regular JS_FUNCTION.
  { Label convert_to_object, use_global_receiver, patch_receiver;
    // Change context eagerly in case we need the global receiver.
    __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a3, FieldMemOperand(a2, SharedFunctionInfo::kCompilerHintsOffset));
    __ And(t3, a3, Operand(1 << (SharedFunctionInfo::kStrictModeFunction +
                                 kSmiTagSize)));
    __ Branch(&shift_arguments, ne, t3, Operand(zero_reg));

    // Do not transform the receiver for native (Compilerhints already in a3).
    __ And(t3, a3, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
    __ Branch(&shift_arguments, ne, t3, Operand(zero_reg));

    // Compute the receiver in non-strict mode.
    // Load first argument in a2. a2 = -kPointerSize(sp + n_args << 2).
    __ sll(at, a0, kPointerSizeLog2);
    __ addu(a2, sp, at);
    __ lw(a2, MemOperand(a2, -kPointerSize));
    // a0: actual number of arguments
    // a1: function
    // a2: first argument
    __ JumpIfSmi(a2, &convert_to_object, t2);

    __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
    __ Branch(&use_global_receiver, eq, a2, Operand(a3));
    __ LoadRoot(a3, Heap::kNullValueRootIndex);
    __ Branch(&use_global_receiver, eq, a2, Operand(a3));

    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ GetObjectType(a2, a3, a3);
    __ Branch(&shift_arguments, ge, a3, Operand(FIRST_SPEC_OBJECT_TYPE));

    __ bind(&convert_to_object);
    // Enter an internal frame in order to preserve argument count.
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ sll(a0, a0, kSmiTagSize);  // Smi tagged.
      __ push(a0);

      __ push(a2);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ mov(a2, v0);

      __ pop(a0);
      __ sra(a0, a0, kSmiTagSize);  // Un-tag.
      // Leave internal frame.
    }
    // Restore the function to a1, and the flag to t0.
    __ sll(at, a0, kPointerSizeLog2);
    __ addu(at, sp, at);
    __ lw(a1, MemOperand(at));
    __ li(t0, Operand(0, RelocInfo::NONE));
    __ Branch(&patch_receiver);

    // Use the global receiver object from the called function as the
    // receiver.
    __ bind(&use_global_receiver);
    const int kGlobalIndex =
        Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
    __ lw(a2, FieldMemOperand(cp, kGlobalIndex));
    __ lw(a2, FieldMemOperand(a2, GlobalObject::kGlobalContextOffset));
    __ lw(a2, FieldMemOperand(a2, kGlobalIndex));
    __ lw(a2, FieldMemOperand(a2, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ sll(at, a0, kPointerSizeLog2);
    __ addu(a3, sp, at);
    __ sw(a2, MemOperand(a3, -kPointerSize));

    __ Branch(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ li(t0, Operand(1, RelocInfo::NONE));  // Indicate function proxy.
  __ Branch(&shift_arguments, eq, a2, Operand(JS_FUNCTION_PROXY_TYPE));

  __ bind(&non_function);
  __ li(t0, Operand(2, RelocInfo::NONE));  // Indicate non-function.

  // 3c. Patch the first argument when calling a non-function.  The
  //     CALL_NON_FUNCTION builtin expects the non-function callee as
  //     receiver, so overwrite the first argument which will ultimately
  //     become the receiver.
  // a0: actual number of arguments
  // a1: function
  // t0: call type (0: JS function, 1: function proxy, 2: non-function)
  __ sll(at, a0, kPointerSizeLog2);
  __ addu(a2, sp, at);
  __ sw(a1, MemOperand(a2, -kPointerSize));

  // 4. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  // t0: call type (0: JS function, 1: function proxy, 2: non-function)
  __ bind(&shift_arguments);
  { Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ sll(at, a0, kPointerSizeLog2);
    __ addu(a2, sp, at);

    __ bind(&loop);
    __ lw(at, MemOperand(a2, -kPointerSize));
    __ sw(at, MemOperand(a2));
    __ Subu(a2, a2, Operand(kPointerSize));
    __ Branch(&loop, ne, a2, Operand(sp));
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Subu(a0, a0, Operand(1));
    __ Pop();
  }

  // 5a. Call non-function via tail call to CALL_NON_FUNCTION builtin,
  //     or a function proxy via CALL_FUNCTION_PROXY.
  // a0: actual number of arguments
  // a1: function
  // t0: call type (0: JS function, 1: function proxy, 2: non-function)
  { Label function, non_proxy;
    __ Branch(&function, eq, t0, Operand(zero_reg));
    // Expected number of arguments is 0 for CALL_NON_FUNCTION.
    __ mov(a2, zero_reg);
    __ SetCallKind(t1, CALL_AS_METHOD);
    __ Branch(&non_proxy, ne, t0, Operand(1));

    __ push(a1);  // Re-add proxy object as additional argument.
    __ Addu(a0, a0, Operand(1));
    __ GetBuiltinEntry(a3, Builtins::CALL_FUNCTION_PROXY);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    __ bind(&non_proxy);
    __ GetBuiltinEntry(a3, Builtins::CALL_NON_FUNCTION);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
    __ bind(&function);
  }

  // 5b. Get the code to call from the function and check that the number of
  //     expected arguments matches what we're providing.  If so, jump
  //     (tail-call) to the code in register edx without checking arguments.
  // a0: actual number of arguments
  // a1: function
  __ lw(a3, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2,
         FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  __ sra(a2, a2, kSmiTagSize);
  __ lw(a3, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ SetCallKind(t1, CALL_AS_METHOD);
  // Check formal and actual parameter counts.
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET, ne, a2, Operand(a0));

  ParameterCount expected(0);
  __ InvokeCode(a3, expected, expected, JUMP_FUNCTION,
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
    __ lw(a0, MemOperand(fp, kFunctionOffset));  // Get the function.
    __ push(a0);
    __ lw(a0, MemOperand(fp, kArgsOffset));  // Get the args array.
    __ push(a0);
    // Returns (in v0) number of arguments to copy to stack as Smi.
    __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);

    // Check the stack for overflow. We are not trying to catch
    // interruptions (e.g. debug break and preemption) here, so the "real stack
    // limit" is checked.
    Label okay;
    __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
    // Make a2 the space we have left. The stack might already be overflowed
    // here which will cause a2 to become negative.
    __ subu(a2, sp, a2);
    // Check if the arguments will overflow the stack.
    __ sll(t3, v0, kPointerSizeLog2 - kSmiTagSize);
    __ Branch(&okay, gt, a2, Operand(t3));  // Signed comparison.

    // Out of stack space.
    __ lw(a1, MemOperand(fp, kFunctionOffset));
    __ push(a1);
    __ push(v0);
    __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_FUNCTION);
    // End of stack check.

    // Push current limit and index.
    __ bind(&okay);
    __ push(v0);  // Limit.
    __ mov(a1, zero_reg);  // Initial index.
    __ push(a1);

    // Get the receiver.
    __ lw(a0, MemOperand(fp, kRecvOffset));

    // Check that the function is a JS function (otherwise it must be a proxy).
    Label push_receiver;
    __ lw(a1, MemOperand(fp, kFunctionOffset));
    __ GetObjectType(a1, a2, a2);
    __ Branch(&push_receiver, ne, a2, Operand(JS_FUNCTION_TYPE));

    // Change context eagerly to get the right global object if necessary.
    __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
    // Load the shared function info while the function is still in a1.
    __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));

    // Compute the receiver.
    // Do not transform the receiver for strict mode functions.
    Label call_to_object, use_global_receiver;
    __ lw(a2, FieldMemOperand(a2, SharedFunctionInfo::kCompilerHintsOffset));
    __ And(t3, a2, Operand(1 << (SharedFunctionInfo::kStrictModeFunction +
                                 kSmiTagSize)));
    __ Branch(&push_receiver, ne, t3, Operand(zero_reg));

    // Do not transform the receiver for native (Compilerhints already in a2).
    __ And(t3, a2, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
    __ Branch(&push_receiver, ne, t3, Operand(zero_reg));

    // Compute the receiver in non-strict mode.
    __ JumpIfSmi(a0, &call_to_object);
    __ LoadRoot(a1, Heap::kNullValueRootIndex);
    __ Branch(&use_global_receiver, eq, a0, Operand(a1));
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ Branch(&use_global_receiver, eq, a0, Operand(a2));

    // Check if the receiver is already a JavaScript object.
    // a0: receiver
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ GetObjectType(a0, a1, a1);
    __ Branch(&push_receiver, ge, a1, Operand(FIRST_SPEC_OBJECT_TYPE));

    // Convert the receiver to a regular object.
    // a0: receiver
    __ bind(&call_to_object);
    __ push(a0);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ mov(a0, v0);  // Put object in a0 to match other paths to push_receiver.
    __ Branch(&push_receiver);

    // Use the current global receiver object as the receiver.
    __ bind(&use_global_receiver);
    const int kGlobalOffset =
        Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
    __ lw(a0, FieldMemOperand(cp, kGlobalOffset));
    __ lw(a0, FieldMemOperand(a0, GlobalObject::kGlobalContextOffset));
    __ lw(a0, FieldMemOperand(a0, kGlobalOffset));
    __ lw(a0, FieldMemOperand(a0, GlobalObject::kGlobalReceiverOffset));

    // Push the receiver.
    // a0: receiver
    __ bind(&push_receiver);
    __ push(a0);

    // Copy all arguments from the array to the stack.
    Label entry, loop;
    __ lw(a0, MemOperand(fp, kIndexOffset));
    __ Branch(&entry);

    // Load the current argument from the arguments array and push it to the
    // stack.
    // a0: current argument index
    __ bind(&loop);
    __ lw(a1, MemOperand(fp, kArgsOffset));
    __ push(a1);
    __ push(a0);

    // Call the runtime to access the property in the arguments array.
    __ CallRuntime(Runtime::kGetProperty, 2);
    __ push(v0);

    // Use inline caching to access the arguments.
    __ lw(a0, MemOperand(fp, kIndexOffset));
    __ Addu(a0, a0, Operand(1 << kSmiTagSize));
    __ sw(a0, MemOperand(fp, kIndexOffset));

    // Test if the copy loop has finished copying all the elements from the
    // arguments object.
    __ bind(&entry);
    __ lw(a1, MemOperand(fp, kLimitOffset));
    __ Branch(&loop, ne, a0, Operand(a1));

    // Invoke the function.
    Label call_proxy;
    ParameterCount actual(a0);
    __ sra(a0, a0, kSmiTagSize);
    __ lw(a1, MemOperand(fp, kFunctionOffset));
    __ GetObjectType(a1, a2, a2);
    __ Branch(&call_proxy, ne, a2, Operand(JS_FUNCTION_TYPE));

    __ InvokeFunction(a1, actual, CALL_FUNCTION,
                      NullCallWrapper(), CALL_AS_METHOD);

    frame_scope.GenerateLeaveFrame();
    __ Ret(USE_DELAY_SLOT);
    __ Addu(sp, sp, Operand(3 * kPointerSize));  // In delay slot.

    // Invoke the function proxy.
    __ bind(&call_proxy);
    __ push(a1);  // Add function proxy as last argument.
    __ Addu(a0, a0, Operand(1));
    __ li(a2, Operand(0, RelocInfo::NONE));
    __ SetCallKind(t1, CALL_AS_METHOD);
    __ GetBuiltinEntry(a3, Builtins::CALL_FUNCTION_PROXY);
    __ Call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
    // Tear down the internal frame and remove function, receiver and args.
  }

  __ Ret(USE_DELAY_SLOT);
  __ Addu(sp, sp, Operand(3 * kPointerSize));  // In delay slot.
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ sll(a0, a0, kSmiTagSize);
  __ li(t0, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | t0.bit() | fp.bit() | ra.bit());
  __ Addu(fp, sp, Operand(3 * kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ lw(a1, MemOperand(fp, -3 * kPointerSize));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ sll(t0, a1, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(sp, sp, t0);
  // Adjust for the receiver.
  __ Addu(sp, sp, Operand(kPointerSize));
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // State setup as expected by MacroAssembler::InvokePrologue.
  // ----------- S t a t e -------------
  //  -- a0: actual arguments count
  //  -- a1: function (passed through to callee)
  //  -- a2: expected arguments count
  //  -- a3: callee code entry
  //  -- t1: call kind information
  // -----------------------------------

  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ Branch(&dont_adapt_arguments, eq,
      a2, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  // We use Uless as the number of argument should always be greater than 0.
  __ Branch(&too_few, Uless, a0, Operand(a2));

  {  // Enough parameters: actual >= expected.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: code entry to call
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into a0 and copy end address into a2.
    __ sll(a0, a0, kPointerSizeLog2 - kSmiTagSize);
    __ Addu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ sll(a2, a2, kPointerSizeLog2);
    __ subu(a2, a0, a2);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: copy end address
    // a3: code entry to call

    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));
    __ push(t0);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a2));
    __ addiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into a0 and copy end address is fp.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: code entry to call
    __ sll(a0, a0, kPointerSizeLog2 - kSmiTagSize);
    __ Addu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address. Also adjust for return address.
    __ Addu(t3, fp, kPointerSize);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: code entry to call
    // t3: copy end address
    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Subu(sp, sp, kPointerSize);
    __ Subu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(t3));
    __ sw(t0, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: code entry to call
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ sll(t2, a2, kPointerSizeLog2);
    __ Subu(a2, fp, Operand(t2));
    __ Addu(a2, a2, Operand(-4 * kPointerSize));  // Adjust for frame.

    Label fill;
    __ bind(&fill);
    __ Subu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(a2));
    __ sw(t0, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);

  __ Call(a3);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();


  // -------------------------------------------
  // Don't adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ Jump(a3);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS

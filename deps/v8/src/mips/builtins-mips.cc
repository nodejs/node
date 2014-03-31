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

#if V8_TARGET_ARCH_MIPS

#include "codegen.h"
#include "debug.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "runtime.h"
#include "stub-cache.h"

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
  // Load the native context.

  __ lw(result,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ lw(result,
        FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the InternalArray function from the native context.
  __ lw(result,
         MemOperand(result,
                    Context::SlotOffset(
                        Context::INTERNAL_ARRAY_FUNCTION_INDEX)));
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the native context.

  __ lw(result,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ lw(result,
        FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the Array function from the native context.
  __ lw(result,
        MemOperand(result,
                   Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
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
    __ SmiTst(a2, t0);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction,
              t0, Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction,
              t0, Operand(MAP_TYPE));
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // Tail call a stub.
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
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
    __ SmiTst(a2, t0);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction1,
              t0, Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction2,
              t0, Operand(MAP_TYPE));
  }

  // Run the native code for the Array function called as a normal function.
  // Tail call a stub.
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
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
    __ Assert(eq, kUnexpectedStringFunction, function, Operand(a2));
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
  __ LookupNumberStringCache(a0,        // Input.
                             argument,  // Result.
                             a3,        // Scratch.
                             t0,        // Scratch.
                             t1,        // Scratch.
                             &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1, a3, t0);
  __ bind(&argument_is_string);

  // ----------- S t a t e -------------
  //  -- a2     : argument converted to string
  //  -- a1     : constructor function
  //  -- ra     : return address
  // -----------------------------------

  Label gc_required;
  __ Allocate(JSValue::kSize,
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
    __ Assert(eq, kUnexpectedStringWrapperInstanceSize,
        t0, Operand(JSValue::kSize >> kPointerSizeLog2));
    __ lbu(t0, FieldMemOperand(map, Map::kUnusedPropertyFieldsOffset));
    __ Assert(eq, kUnexpectedUnusedPropertiesOfStringWrapper,
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
    __ push(a0);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
  }
  __ pop(function);
  __ mov(argument, v0);
  __ Branch(&argument_is_string);

  // Load the empty string into a2, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ bind(&no_arguments);
  __ LoadRoot(argument, Heap::kempty_stringRootIndex);
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


static void CallRuntimePassFunction(
    MacroAssembler* masm, Runtime::FunctionId function_id) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  // Push call kind information and function as parameter to the runtime call.
  __ Push(a1, a1);

  __ CallRuntime(function_id, 1);
  // Restore call kind information and receiver.
  __ Pop(a1);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2, FieldMemOperand(a2, SharedFunctionInfo::kCodeOffset));
  __ Addu(at, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ Addu(at, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(t0, Heap::kStackLimitRootIndex);
  __ Branch(&ok, hs, sp, Operand(t0));

  CallRuntimePassFunction(masm, Runtime::kHiddenTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool count_constructions,
                                           bool create_memento) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a2     : allocation site or undefined
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Should never count constructions for api objects.
  ASSERT(!is_api_function || !count_constructions);

  // Should never create mementos for api functions.
  ASSERT(!is_api_function || !create_memento);

  // Should never create mementos before slack tracking is finished.
  ASSERT(!count_constructions || !create_memento);

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

    if (create_memento) {
      __ AssertUndefinedOrAllocationSite(a2, a3);
      __ push(a2);
    }

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

        __ Push(a1, a2, a1);  // a1 = Constructor.
        // The call will replace the stub, so the countdown is only done once.
        __ CallRuntime(Runtime::kHiddenFinalizeInstanceSize, 1);

        __ Pop(a1, a2);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // a1: constructor function
      // a2: initial map
      __ lbu(a3, FieldMemOperand(a2, Map::kInstanceSizeOffset));
      if (create_memento) {
        __ Addu(a3, a3, Operand(AllocationMemento::kSize / kPointerSize));
      }

      __ Allocate(a3, t4, t5, t6, &rt_call, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // a1: constructor function
      // a2: initial map
      // a3: object size (not including memento if create_memento)
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
      // a3: object size (in words, including memento if create_memento)
      // t4: JSObject (not tagged)
      // t5: First in-object property of JSObject (not tagged)
      ASSERT_EQ(3 * kPointerSize, JSObject::kHeaderSize);

      if (count_constructions) {
        __ LoadRoot(t7, Heap::kUndefinedValueRootIndex);
        __ lw(a0, FieldMemOperand(a2, Map::kInstanceSizesOffset));
        __ Ext(a0, a0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
                kBitsPerByte);
        __ sll(at, a0, kPointerSizeLog2);
        __ addu(a0, t5, at);
        __ sll(at, a3, kPointerSizeLog2);
        __ Addu(t6, t4, Operand(at));   // End of object.
        // a0: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ Assert(le, kUnexpectedNumberOfPreAllocatedPropertyFields,
              a0, Operand(t6));
        }
        __ InitializeFieldsWithFiller(t5, a0, t7);
        // To allow for truncation.
        __ LoadRoot(t7, Heap::kOnePointerFillerMapRootIndex);
        __ InitializeFieldsWithFiller(t5, t6, t7);
      } else if (create_memento) {
        __ Subu(t7, a3, Operand(AllocationMemento::kSize / kPointerSize));
        __ sll(at, t7, kPointerSizeLog2);
        __ Addu(a0, t4, Operand(at));  // End of object.
        __ LoadRoot(t7, Heap::kUndefinedValueRootIndex);
        __ InitializeFieldsWithFiller(t5, a0, t7);

        // Fill in memento fields.
        // t5: points to the allocated but uninitialized memento.
        __ LoadRoot(t7, Heap::kAllocationMementoMapRootIndex);
        ASSERT_EQ(0 * kPointerSize, AllocationMemento::kMapOffset);
        __ sw(t7, MemOperand(t5));
        __ Addu(t5, t5, kPointerSize);
        // Load the AllocationSite.
        __ lw(t7, MemOperand(sp, 2 * kPointerSize));
        ASSERT_EQ(1 * kPointerSize, AllocationMemento::kAllocationSiteOffset);
        __ sw(t7, MemOperand(t5));
        __ Addu(t5, t5, kPointerSize);
      } else {
        __ LoadRoot(t7, Heap::kUndefinedValueRootIndex);
        __ sll(at, a3, kPointerSizeLog2);
        __ Addu(a0, t4, Operand(at));  // End of object.
        __ InitializeFieldsWithFiller(t5, a0, t7);
      }

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
      __ Assert(greater_equal, kPropertyAllocationCountFailed,
          a3, Operand(zero_reg));

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // a1: constructor
      // a3: number of elements in properties array
      // t4: JSObject
      // t5: start of next object
      __ Addu(a0, a3, Operand(FixedArray::kHeaderSize / kPointerSize));
      __ Allocate(
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
          __ Assert(eq, kUndefinedValueNotLoaded, t7, Operand(t8));
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

    // Allocate the new receiver object using the runtime call.
    // a1: constructor function
    __ bind(&rt_call);
    if (create_memento) {
      // Get the cell or allocation site.
      __ lw(a2, MemOperand(sp, 2 * kPointerSize));
      __ push(a2);
    }

    __ push(a1);  // Argument for Runtime_NewObject.
    if (create_memento) {
      __ CallRuntime(Runtime::kHiddenNewObjectWithAllocationSite, 2);
    } else {
      __ CallRuntime(Runtime::kHiddenNewObject, 1);
    }
    __ mov(t4, v0);

    // If we ended up using the runtime, and we want a memento, then the
    // runtime call made it for us, and we shouldn't do create count
    // increment.
    Label count_incremented;
    if (create_memento) {
      __ jmp(&count_incremented);
    }

    // Receiver for constructor call allocated.
    // t4: JSObject

    if (create_memento) {
      __ lw(a2, MemOperand(sp, kPointerSize * 2));
      __ LoadRoot(t5, Heap::kUndefinedValueRootIndex);
      __ Branch(&count_incremented, eq, a2, Operand(t5));
      // a2 is an AllocationSite. We are creating a memento from it, so we
      // need to increment the memento create count.
      __ lw(a3, FieldMemOperand(a2,
                                AllocationSite::kPretenureCreateCountOffset));
      __ Addu(a3, a3, Operand(Smi::FromInt(1)));
      __ sw(a3, FieldMemOperand(a2,
                                AllocationSite::kPretenureCreateCountOffset));
      __ bind(&count_incremented);
    }

    __ bind(&allocated);
    __ Push(t4, t4);

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
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(a0);
      __ InvokeFunction(a1, actual, CALL_FUNCTION, NullCallWrapper());
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
    __ GetObjectType(v0, a1, a3);
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
  Generate_JSConstructStubHelper(masm, false, true, false);
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false, FLAG_pretenuring_call_new);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false, false);
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
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

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
      // No type feedback cell is available
      __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
      CallConstructStub stub(NO_CALL_FUNCTION_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(a0);
      __ InvokeFunction(a1, actual, CALL_FUNCTION, NullCallWrapper());
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


void Builtins::Generate_CompileUnoptimized(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kHiddenCompileUnoptimized);
  GenerateTailCallToReturnedCode(masm);
}


static void CallCompileOptimized(MacroAssembler* masm, bool concurrent) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  // Push function as parameter to the runtime call.
  __ Push(a1, a1);
  // Whether to compile in a background thread.
  __ Push(masm->isolate()->factory()->ToBoolean(concurrent));

  __ CallRuntime(Runtime::kHiddenCompileOptimized, 2);
  // Restore receiver.
  __ Pop(a1);
}


void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  CallCompileOptimized(masm, false);
  GenerateTailCallToReturnedCode(masm);
}


void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  CallCompileOptimized(masm, true);
  GenerateTailCallToReturnedCode(masm);
}



static void GenerateMakeCodeYoungAgainCommon(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Set a0 to point to the head of the PlatformCodeAge sequence.
  __ Subu(a0, a0,
      Operand((kNoCodeAgeSequenceLength - 1) * Assembler::kInstrSize));

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   a0 - contains return address (beginning of patch sequence)
  //   a1 - isolate
  RegList saved_regs =
      (a0.bit() | a1.bit() | ra.bit() | fp.bit()) & ~sp.bit();
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(saved_regs);
  __ PrepareCallCFunction(2, 0, a2);
  __ li(a1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ MultiPop(saved_regs);
  __ Jump(a0);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                 \
void Builtins::Generate_Make##C##CodeYoungAgainEvenMarking(  \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}                                                            \
void Builtins::Generate_Make##C##CodeYoungAgainOddMarking(   \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR


void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, as in GenerateMakeCodeYoungAgainCommon, we are relying on the fact
  // that make_code_young doesn't do any garbage collection which allows us to
  // save/restore the registers without worrying about which of them contain
  // pointers.

  // Set a0 to point to the head of the PlatformCodeAge sequence.
  __ Subu(a0, a0,
      Operand((kNoCodeAgeSequenceLength - 1) * Assembler::kInstrSize));

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   a0 - contains return address (beginning of patch sequence)
  //   a1 - isolate
  RegList saved_regs =
      (a0.bit() | a1.bit() | ra.bit() | fp.bit()) & ~sp.bit();
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(saved_regs);
  __ PrepareCallCFunction(2, 0, a2);
  __ li(a1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
      2);
  __ MultiPop(saved_regs);

  // Perform prologue operations usually performed by the young code stub.
  __ Push(ra, fp, cp, a1);
  __ Addu(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Jump to point after the code-age stub.
  __ Addu(a0, a0, Operand((kNoCodeAgeSequenceLength) * Assembler::kInstrSize));
  __ Jump(a0);
}


void Builtins::Generate_MarkCodeAsExecutedTwice(MacroAssembler* masm) {
  GenerateMakeCodeYoungAgainCommon(masm);
}


static void Generate_NotifyStubFailureHelper(MacroAssembler* masm,
                                             SaveFPRegsMode save_doubles) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ MultiPush(kJSCallerSaved | kCalleeSaved);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kHiddenNotifyStubFailure, 0, save_doubles);
    __ MultiPop(kJSCallerSaved | kCalleeSaved);
  }

  __ Addu(sp, sp, Operand(kPointerSize));  // Ignore state
  __ Jump(ra);  // Jump to miss handler
}


void Builtins::Generate_NotifyStubFailure(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kDontSaveFPRegs);
}


void Builtins::Generate_NotifyStubFailureSaveDoubles(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kSaveFPRegs);
}


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass the function and deoptimization type to the runtime system.
    __ li(a0, Operand(Smi::FromInt(static_cast<int>(type))));
    __ push(a0);
    __ CallRuntime(Runtime::kHiddenNotifyDeoptimized, 1);
  }

  // Get the full codegen state from the stack and untag it -> t2.
  __ lw(t2, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(t2);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ Branch(&with_tos_register,
            ne, t2, Operand(FullCodeGenerator::NO_REGISTERS));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Addu(sp, sp, Operand(1 * kPointerSize));  // Remove state.

  __ bind(&with_tos_register);
  __ lw(v0, MemOperand(sp, 1 * kPointerSize));
  __ Branch(&unknown_state, ne, t2, Operand(FullCodeGenerator::TOS_REG));

  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Addu(sp, sp, Operand(2 * kPointerSize));  // Remove state.

  __ bind(&unknown_state);
  __ stop("no cases left");
}


void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_NotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ lw(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(a0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the code object is null, just return to the unoptimized code.
  __ Ret(eq, v0, Operand(Smi::FromInt(0)));

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ lw(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ lw(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
      DeoptimizationInputData::kOsrPcOffsetIndex) - kHeapObjectTag));
  __ SmiUntag(a1);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ addu(v0, v0, a1);
  __ addiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}


void Builtins::Generate_OsrAfterStackCheck(MacroAssembler* masm) {
  // We check the stack limit as indicator that recompilation might be done.
  Label ok;
  __ LoadRoot(at, Heap::kStackLimitRootIndex);
  __ Branch(&ok, hs, sp, Operand(at));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kHiddenStackGuard, 0);
  }
  __ Jump(masm->isolate()->builtins()->OnStackReplacement(),
          RelocInfo::CODE_TARGET);

  __ bind(&ok);
  __ Ret();
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
  __ li(t0, Operand(0, RelocInfo::NONE32));  // Indicate regular JS_FUNCTION.
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

    // Compute the receiver in sloppy mode.
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
      __ Push(a0, a2);
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
    __ li(t0, Operand(0, RelocInfo::NONE32));
    __ Branch(&patch_receiver);

    __ bind(&use_global_receiver);
    __ lw(a2, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
    __ lw(a2, FieldMemOperand(a2, GlobalObject::kGlobalReceiverOffset));

    __ bind(&patch_receiver);
    __ sll(at, a0, kPointerSizeLog2);
    __ addu(a3, sp, at);
    __ sw(a2, MemOperand(a3, -kPointerSize));

    __ Branch(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ li(t0, Operand(1, RelocInfo::NONE32));  // Indicate function proxy.
  __ Branch(&shift_arguments, eq, a2, Operand(JS_FUNCTION_PROXY_TYPE));

  __ bind(&non_function);
  __ li(t0, Operand(2, RelocInfo::NONE32));  // Indicate non-function.

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
    __ Branch(&non_proxy, ne, t0, Operand(1));

    __ push(a1);  // Re-add proxy object as additional argument.
    __ Addu(a0, a0, Operand(1));
    __ GetBuiltinFunction(a1, Builtins::CALL_FUNCTION_PROXY);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    __ bind(&non_proxy);
    __ GetBuiltinFunction(a1, Builtins::CALL_NON_FUNCTION);
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
  // Check formal and actual parameter counts.
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET, ne, a2, Operand(a0));

  __ lw(a3, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  ParameterCount expected(0);
  __ InvokeCode(a3, expected, expected, JUMP_FUNCTION, NullCallWrapper());
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  const int kIndexOffset    =
      StandardFrameConstants::kExpressionsOffset - (2 * kPointerSize);
  const int kLimitOffset    =
      StandardFrameConstants::kExpressionsOffset - (1 * kPointerSize);
  const int kArgsOffset     = 2 * kPointerSize;
  const int kRecvOffset     = 3 * kPointerSize;
  const int kFunctionOffset = 4 * kPointerSize;

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
    __ Push(a1, v0);
    __ InvokeBuiltin(Builtins::APPLY_OVERFLOW, CALL_FUNCTION);
    // End of stack check.

    // Push current limit and index.
    __ bind(&okay);
    __ mov(a1, zero_reg);
    __ Push(v0, a1);  // Limit and initial index.

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

    // Compute the receiver in sloppy mode.
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

    __ bind(&use_global_receiver);
    __ lw(a0, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
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
    __ Push(a1, a0);

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

    // Call the function.
    Label call_proxy;
    ParameterCount actual(a0);
    __ sra(a0, a0, kSmiTagSize);
    __ lw(a1, MemOperand(fp, kFunctionOffset));
    __ GetObjectType(a1, a2, a2);
    __ Branch(&call_proxy, ne, a2, Operand(JS_FUNCTION_TYPE));

    __ InvokeFunction(a1, actual, CALL_FUNCTION, NullCallWrapper());

    frame_scope.GenerateLeaveFrame();
    __ Ret(USE_DELAY_SLOT);
    __ Addu(sp, sp, Operand(3 * kPointerSize));  // In delay slot.

    // Call the function proxy.
    __ bind(&call_proxy);
    __ push(a1);  // Add function proxy as last argument.
    __ Addu(a0, a0, Operand(1));
    __ li(a2, Operand(0, RelocInfo::NONE32));
    __ GetBuiltinFunction(a1, Builtins::CALL_FUNCTION_PROXY);
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
  __ Addu(fp, sp,
      Operand(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ lw(a1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                             kPointerSize)));
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
  // -----------------------------------

  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ lw(a3, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
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
    // Adjust for frame.
    __ Subu(a2, a2, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

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

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/codegen.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen.h"
#include "src/runtime/runtime.h"

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
  //  -- sp[8 * (argc - 1)] : first argument
  //  -- sp[8 * agrc]       : receiver
  // -----------------------------------

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ push(a1);
  } else {
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects s0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ Daddu(s0, a0, num_extra_args + 1);
  __ dsll(s1, s0, kPointerSizeLog2);
  __ Dsubu(s1, s1, kPointerSize);
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the native context.

  __ ld(result,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ ld(result,
        FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the InternalArray function from the native context.
  __ ld(result,
         MemOperand(result,
                    Context::SlotOffset(
                        Context::INTERNAL_ARRAY_FUNCTION_INDEX)));
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the native context.

  __ ld(result,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ ld(result,
        FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the Array function from the native context.
  __ ld(result,
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
    __ ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, a4);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction,
              a4, Operand(zero_reg));
    __ GetObjectType(a2, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction,
              a4, Operand(MAP_TYPE));
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
    __ ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, a4);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction1,
              a4, Operand(zero_reg));
    __ GetObjectType(a2, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction2,
              a4, Operand(MAP_TYPE));
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
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
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
  // First args = sp[(argc - 1) * 8].
  __ Dsubu(a0, a0, Operand(1));
  __ dsll(a0, a0, kPointerSizeLog2);
  __ Daddu(sp, a0, sp);
  __ ld(a0, MemOperand(sp));
  // sp now point to args[0], drop args[0] + receiver.
  __ Drop(2);

  Register argument = a2;
  Label not_cached, argument_is_string;
  __ LookupNumberStringCache(a0,        // Input.
                             argument,  // Result.
                             a3,        // Scratch.
                             a4,        // Scratch.
                             a5,        // Scratch.
                             &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1, a3, a4);
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
              a4,  // Scratch.
              &gc_required,
              TAG_OBJECT);

  // Initialising the String Object.
  Register map = a3;
  __ LoadGlobalFunctionInitialMap(function, map, a4);
  if (FLAG_debug_code) {
    __ lbu(a4, FieldMemOperand(map, Map::kInstanceSizeOffset));
    __ Assert(eq, kUnexpectedStringWrapperInstanceSize,
        a4, Operand(JSValue::kSize >> kPointerSizeLog2));
    __ lbu(a4, FieldMemOperand(map, Map::kUnusedPropertyFieldsOffset));
    __ Assert(eq, kUnexpectedUnusedPropertiesOfStringWrapper,
        a4, Operand(zero_reg));
  }
  __ sd(map, FieldMemOperand(v0, HeapObject::kMapOffset));

  __ LoadRoot(a3, Heap::kEmptyFixedArrayRootIndex);
  __ sd(a3, FieldMemOperand(v0, JSObject::kPropertiesOffset));
  __ sd(a3, FieldMemOperand(v0, JSObject::kElementsOffset));

  __ sd(argument, FieldMemOperand(v0, JSValue::kValueOffset));

  // Ensure the object is fully initialized.
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);

  __ Ret();

  // The argument was not found in the number to string cache. Check
  // if it's a string already before calling the conversion builtin.
  Label convert_argument;
  __ bind(&not_cached);
  __ JumpIfSmi(a0, &convert_argument);

  // Is it a String?
  __ ld(a2, FieldMemOperand(a0, HeapObject::kMapOffset));
  __ lbu(a3, FieldMemOperand(a2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ And(a4, a3, Operand(kIsNotStringMask));
  __ Branch(&convert_argument, ne, a4, Operand(zero_reg));
  __ mov(argument, a0);
  __ IncrementCounter(counters->string_ctor_conversions(), 1, a3, a4);
  __ Branch(&argument_is_string);

  // Invoke the conversion builtin and put the result into a2.
  __ bind(&convert_argument);
  __ push(function);  // Preserve the function.
  __ IncrementCounter(counters->string_ctor_conversions(), 1, a3, a4);
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
  __ IncrementCounter(counters->string_ctor_gc_required(), 1, a3, a4);
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
  __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(a2, FieldMemOperand(a2, SharedFunctionInfo::kCodeOffset));
  __ Daddu(at, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ Daddu(at, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(a4, Heap::kStackLimitRootIndex);
  __ Branch(&ok, hs, sp, Operand(a4));

  CallRuntimePassFunction(masm, Runtime::kTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool create_memento) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a2     : allocation site or undefined
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Should never create mementos for api functions.
  DCHECK(!is_api_function || !create_memento);

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
    // Tag arguments count.
    __ dsll32(a0, a0, 0);
    __ MultiPushReversed(a0.bit() | a1.bit());

    Label rt_call, allocated;
    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    if (FLAG_inline_new) {
      Label undo_allocation;
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ li(a2, Operand(debug_step_in_fp));
      __ ld(a2, MemOperand(a2));
      __ Branch(&rt_call, ne, a2, Operand(zero_reg));

      // Load the initial map and verify that it is in fact a map.
      // a1: constructor function
      __ ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(a2, &rt_call);
      __ GetObjectType(a2, a3, t0);
      __ Branch(&rt_call, ne, t0, Operand(MAP_TYPE));

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // a1: constructor function
      // a2: initial map
      __ lbu(a3, FieldMemOperand(a2, Map::kInstanceTypeOffset));
      __ Branch(&rt_call, eq, a3, Operand(JS_FUNCTION_TYPE));

      if (!is_api_function) {
        Label allocate;
        MemOperand bit_field3 = FieldMemOperand(a2, Map::kBitField3Offset);
        // Check if slack tracking is enabled.
        __ lwu(a4, bit_field3);
        __ DecodeField<Map::ConstructionCount>(a6, a4);
        __ Branch(&allocate,
                  eq,
                  a6,
                  Operand(static_cast<int64_t>(JSFunction::kNoSlackTracking)));
        // Decrease generous allocation count.
        __ Dsubu(a4, a4, Operand(1 << Map::ConstructionCount::kShift));
        __ Branch(USE_DELAY_SLOT,
            &allocate, ne, a6, Operand(JSFunction::kFinishSlackTracking));
        __ sw(a4, bit_field3);  // In delay slot.

        __ Push(a1, a2, a1);  // a1 = Constructor.
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ Pop(a1, a2);
        // Slack tracking counter is kNoSlackTracking after runtime call.
        DCHECK(JSFunction::kNoSlackTracking == 0);
        __ mov(a6, zero_reg);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // a1: constructor function
      // a2: initial map
      __ lbu(a3, FieldMemOperand(a2, Map::kInstanceSizeOffset));
      if (create_memento) {
        __ Daddu(a3, a3, Operand(AllocationMemento::kSize / kPointerSize));
      }

      __ Allocate(a3, t0, t1, t2, &rt_call, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // a1: constructor function
      // a2: initial map
      // a3: object size (not including memento if create_memento)
      // t0: JSObject (not tagged)
      __ LoadRoot(t2, Heap::kEmptyFixedArrayRootIndex);
      __ mov(t1, t0);
      __ sd(a2, MemOperand(t1, JSObject::kMapOffset));
      __ sd(t2, MemOperand(t1, JSObject::kPropertiesOffset));
      __ sd(t2, MemOperand(t1, JSObject::kElementsOffset));
      __ Daddu(t1, t1, Operand(3*kPointerSize));
      DCHECK_EQ(0 * kPointerSize, JSObject::kMapOffset);
      DCHECK_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
      DCHECK_EQ(2 * kPointerSize, JSObject::kElementsOffset);

      // Fill all the in-object properties with appropriate filler.
      // a1: constructor function
      // a2: initial map
      // a3: object size (in words, including memento if create_memento)
      // t0: JSObject (not tagged)
      // t1: First in-object property of JSObject (not tagged)
      // a6: slack tracking counter (non-API function case)
      DCHECK_EQ(3 * kPointerSize, JSObject::kHeaderSize);

      // Use t3 to hold undefined, which is used in several places below.
      __ LoadRoot(t3, Heap::kUndefinedValueRootIndex);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ Branch(&no_inobject_slack_tracking,
                  eq,
                  a6,
                  Operand(static_cast<int64_t>(JSFunction::kNoSlackTracking)));

        // Allocate object with a slack.
        __ lwu(a0, FieldMemOperand(a2, Map::kInstanceSizesOffset));
        __ Ext(a0, a0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
                kBitsPerByte);
        __ dsll(at, a0, kPointerSizeLog2);
        __ daddu(a0, t1, at);
        // a0: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ dsll(at, a3, kPointerSizeLog2);
          __ Daddu(t2, t0, Operand(at));   // End of object.
          __ Assert(le, kUnexpectedNumberOfPreAllocatedPropertyFields,
              a0, Operand(t2));
        }
        __ InitializeFieldsWithFiller(t1, a0, t3);
        // To allow for truncation.
        __ LoadRoot(t3, Heap::kOnePointerFillerMapRootIndex);
        // Fill the remaining fields with one pointer filler map.

        __ bind(&no_inobject_slack_tracking);
      }

      if (create_memento) {
        __ Dsubu(a0, a3, Operand(AllocationMemento::kSize / kPointerSize));
        __ dsll(a0, a0, kPointerSizeLog2);
        __ Daddu(a0, t0, Operand(a0));  // End of object.
        __ InitializeFieldsWithFiller(t1, a0, t3);

        // Fill in memento fields.
        // t1: points to the allocated but uninitialized memento.
        __ LoadRoot(t3, Heap::kAllocationMementoMapRootIndex);
        DCHECK_EQ(0 * kPointerSize, AllocationMemento::kMapOffset);
        __ sd(t3, MemOperand(t1));
        __ Daddu(t1, t1, kPointerSize);
        // Load the AllocationSite.
        __ ld(t3, MemOperand(sp, 2 * kPointerSize));
        DCHECK_EQ(1 * kPointerSize, AllocationMemento::kAllocationSiteOffset);
        __ sd(t3, MemOperand(t1));
        __ Daddu(t1, t1, kPointerSize);
      } else {
        __ dsll(at, a3, kPointerSizeLog2);
        __ Daddu(a0, t0, Operand(at));  // End of object.
        __ InitializeFieldsWithFiller(t1, a0, t3);
      }

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on. Any
      // failures need to undo the allocation, so that the heap is in a
      // consistent state and verifiable.
      __ Daddu(t0, t0, Operand(kHeapObjectTag));

      // Check if a non-empty properties array is needed. Continue with
      // allocated object if not fall through to runtime call if it is.
      // a1: constructor function
      // t0: JSObject
      // t1: start of next object (not tagged)
      __ lbu(a3, FieldMemOperand(a2, Map::kUnusedPropertyFieldsOffset));
      // The field instance sizes contains both pre-allocated property fields
      // and in-object properties.
      __ lw(a0, FieldMemOperand(a2, Map::kInstanceSizesOffset));
      __ Ext(t2, a0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
             kBitsPerByte);
      __ Daddu(a3, a3, Operand(t2));
      __ Ext(t2, a0, Map::kInObjectPropertiesByte * kBitsPerByte,
              kBitsPerByte);
      __ dsubu(a3, a3, t2);

      // Done if no extra properties are to be allocated.
      __ Branch(&allocated, eq, a3, Operand(zero_reg));
      __ Assert(greater_equal, kPropertyAllocationCountFailed,
          a3, Operand(zero_reg));

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // a1: constructor
      // a3: number of elements in properties array
      // t0: JSObject
      // t1: start of next object
      __ Daddu(a0, a3, Operand(FixedArray::kHeaderSize / kPointerSize));
      __ Allocate(
          a0,
          t1,
          t2,
          a2,
          &undo_allocation,
          static_cast<AllocationFlags>(RESULT_CONTAINS_TOP | SIZE_IN_WORDS));

      // Initialize the FixedArray.
      // a1: constructor
      // a3: number of elements in properties array (untagged)
      // t0: JSObject
      // t1: start of next object
      __ LoadRoot(t2, Heap::kFixedArrayMapRootIndex);
      __ mov(a2, t1);
      __ sd(t2, MemOperand(a2, JSObject::kMapOffset));
      // Tag number of elements.
      __ dsll32(a0, a3, 0);
      __ sd(a0, MemOperand(a2, FixedArray::kLengthOffset));
      __ Daddu(a2, a2, Operand(2 * kPointerSize));

      DCHECK_EQ(0 * kPointerSize, JSObject::kMapOffset);
      DCHECK_EQ(1 * kPointerSize, FixedArray::kLengthOffset);

      // Initialize the fields to undefined.
      // a1: constructor
      // a2: First element of FixedArray (not tagged)
      // a3: number of elements in properties array
      // t0: JSObject
      // t1: FixedArray (not tagged)
      __ dsll(a7, a3, kPointerSizeLog2);
      __ daddu(t2, a2, a7);  // End of object.
      DCHECK_EQ(2 * kPointerSize, FixedArray::kHeaderSize);
      { Label loop, entry;
        if (!is_api_function || create_memento) {
          __ LoadRoot(t3, Heap::kUndefinedValueRootIndex);
        } else if (FLAG_debug_code) {
          __ LoadRoot(a6, Heap::kUndefinedValueRootIndex);
          __ Assert(eq, kUndefinedValueNotLoaded, t3, Operand(a6));
        }
        __ jmp(&entry);
        __ bind(&loop);
        __ sd(t3, MemOperand(a2));
        __ daddiu(a2, a2, kPointerSize);
        __ bind(&entry);
        __ Branch(&loop, less, a2, Operand(t2));
      }

      // Store the initialized FixedArray into the properties field of
      // the JSObject.
      // a1: constructor function
      // t0: JSObject
      // t1: FixedArray (not tagged)
      __ Daddu(t1, t1, Operand(kHeapObjectTag));  // Add the heap tag.
      __ sd(t1, FieldMemOperand(t0, JSObject::kPropertiesOffset));

      // Continue with JSObject being successfully allocated.
      // a1: constructor function
      // a4: JSObject
      __ jmp(&allocated);

      // Undo the setting of the new top so that the heap is verifiable. For
      // example, the map's unused properties potentially do not match the
      // allocated objects unused properties.
      // t0: JSObject (previous new top)
      __ bind(&undo_allocation);
      __ UndoAllocationInNewSpace(t0, t1);
    }

    // Allocate the new receiver object using the runtime call.
    // a1: constructor function
    __ bind(&rt_call);
    if (create_memento) {
      // Get the cell or allocation site.
      __ ld(a2, MemOperand(sp, 2 * kPointerSize));
      __ push(a2);
    }

    __ push(a1);  // Argument for Runtime_NewObject.
    if (create_memento) {
      __ CallRuntime(Runtime::kNewObjectWithAllocationSite, 2);
    } else {
      __ CallRuntime(Runtime::kNewObject, 1);
    }
    __ mov(t0, v0);

    // If we ended up using the runtime, and we want a memento, then the
    // runtime call made it for us, and we shouldn't do create count
    // increment.
    Label count_incremented;
    if (create_memento) {
      __ jmp(&count_incremented);
    }

    // Receiver for constructor call allocated.
    // t0: JSObject
    __ bind(&allocated);

    if (create_memento) {
      __ ld(a2, MemOperand(sp, kPointerSize * 2));
      __ LoadRoot(t1, Heap::kUndefinedValueRootIndex);
      __ Branch(&count_incremented, eq, a2, Operand(t1));
      // a2 is an AllocationSite. We are creating a memento from it, so we
      // need to increment the memento create count.
      __ ld(a3, FieldMemOperand(a2,
                                AllocationSite::kPretenureCreateCountOffset));
      __ Daddu(a3, a3, Operand(Smi::FromInt(1)));
      __ sd(a3, FieldMemOperand(a2,
                                AllocationSite::kPretenureCreateCountOffset));
      __ bind(&count_incremented);
    }

    __ Push(t0, t0);

    // Reload the number of arguments from the stack.
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: constructor function
    // sp[3]: number of arguments (smi-tagged)
    __ ld(a1, MemOperand(sp, 2 * kPointerSize));
    __ ld(a3, MemOperand(sp, 3 * kPointerSize));

    // Set up pointer to last argument.
    __ Daddu(a2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Set up number of arguments for function call below.
    __ SmiUntag(a0, a3);

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
    __ SmiUntag(a3);
    __ jmp(&entry);
    __ bind(&loop);
    __ dsll(a4, a3, kPointerSizeLog2);
    __ Daddu(a4, a2, Operand(a4));
    __ ld(a5, MemOperand(a4));
    __ push(a5);
    __ bind(&entry);
    __ Daddu(a3, a3, Operand(-1));
    __ Branch(&loop, greater_equal, a3, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments
    // a1: constructor function
    if (is_api_function) {
      __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(a0);
      __ InvokeFunction(a1, actual, CALL_FUNCTION, NullCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ ld(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

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
    __ ld(v0, MemOperand(sp));

    // Remove receiver from the stack, remove caller arguments, and
    // return.
    __ bind(&exit);
    // v0: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ ld(a1, MemOperand(sp, 2 * kPointerSize));

    // Leave construct frame.
  }

  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  __ Daddu(sp, sp, kPointerSize);
  __ IncrementCounter(isolate->counters()->constructed_objects(), 1, a1, a2);
  __ Ret();
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, FLAG_pretenuring_call_new);
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
  ProfileEntryHookStub::MaybeCallEntryHook(masm);
  // Clear the context before we push it when entering the JS frame.
  __ mov(cp, zero_reg);

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Set up the context from the function argument.
    __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

    // Push the function and the receiver onto the stack.
    __ Push(a1, a2);

    // Copy arguments to the stack in a loop.
    // a3: argc
    // s0: argv, i.e. points to first arg
    Label loop, entry;
    // TODO(plind): At least on simulator, argc in a3 is an int32_t with junk
    //    in upper bits. Should fix the root cause, rather than use below
    //    workaround to clear upper bits.
    __ dsll32(a3, a3, 0);  // int32_t -> int64_t.
    __ dsrl32(a3, a3, 0);
    __ dsll(a4, a3, kPointerSizeLog2);
    __ daddu(a6, s0, a4);
    __ b(&entry);
    __ nop();   // Branch delay slot nop.
    // a6 points past last arg.
    __ bind(&loop);
    __ ld(a4, MemOperand(s0));  // Read next parameter.
    __ daddiu(s0, s0, kPointerSize);
    __ ld(a4, MemOperand(a4));  // Dereference handle.
    __ push(a4);  // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, s0, Operand(a6));

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(a4, Heap::kUndefinedValueRootIndex);
    __ mov(s1, a4);
    __ mov(s2, a4);
    __ mov(s3, a4);
    __ mov(s4, a4);
    __ mov(s5, a4);
    // s6 holds the root address. Do not clobber.
    // s7 is cp. Do not init.

    // Invoke the code and pass argc as a0.
    __ mov(a0, a3);
    if (is_construct) {
      // No type feedback cell is available
      __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
      CallConstructStub stub(masm->isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
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


void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kCompileLazy);
  GenerateTailCallToReturnedCode(masm);
}


static void CallCompileOptimized(MacroAssembler* masm, bool concurrent) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  // Push function as parameter to the runtime call.
  __ Push(a1, a1);
  // Whether to compile in a background thread.
  __ Push(masm->isolate()->factory()->ToBoolean(concurrent));

  __ CallRuntime(Runtime::kCompileOptimized, 2);
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
  __ Dsubu(a0, a0,
      Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

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
  __ Dsubu(a0, a0,
      Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

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
  __ Daddu(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Jump to point after the code-age stub.
  __ Daddu(a0, a0, Operand((kNoCodeAgeSequenceLength)));
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
    __ CallRuntime(Runtime::kNotifyStubFailure, 0, save_doubles);
    __ MultiPop(kJSCallerSaved | kCalleeSaved);
  }

  __ Daddu(sp, sp, Operand(kPointerSize));  // Ignore state
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
    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
  }

  // Get the full codegen state from the stack and untag it -> a6.
  __ ld(a6, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(a6);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ Branch(&with_tos_register,
            ne, a6, Operand(FullCodeGenerator::NO_REGISTERS));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Daddu(sp, sp, Operand(1 * kPointerSize));  // Remove state.

  __ bind(&with_tos_register);
  __ ld(v0, MemOperand(sp, 1 * kPointerSize));
  __ Branch(&unknown_state, ne, a6, Operand(FullCodeGenerator::TOS_REG));

  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Daddu(sp, sp, Operand(2 * kPointerSize));  // Remove state.

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
  __ ld(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
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
  __ Uld(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ ld(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
      DeoptimizationInputData::kOsrPcOffsetIndex) - kHeapObjectTag));
  __ SmiUntag(a1);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ daddu(v0, v0, a1);
  __ daddiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

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
    __ CallRuntime(Runtime::kStackGuard, 0);
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
    __ LoadRoot(a6, Heap::kUndefinedValueRootIndex);
    __ push(a6);
    __ Daddu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack, check
  //    if it is a function.
  // a0: actual number of arguments
  Label slow, non_function;
  __ dsll(at, a0, kPointerSizeLog2);
  __ daddu(at, sp, at);
  __ ld(a1, MemOperand(at));
  __ JumpIfSmi(a1, &non_function);
  __ GetObjectType(a1, a2, a2);
  __ Branch(&slow, ne, a2, Operand(JS_FUNCTION_TYPE));

  // 3a. Patch the first argument if necessary when calling a function.
  // a0: actual number of arguments
  // a1: function
  Label shift_arguments;
  __ li(a4, Operand(0, RelocInfo::NONE32));  // Indicate regular JS_FUNCTION.
  { Label convert_to_object, use_global_proxy, patch_receiver;
    // Change context eagerly in case we need the global receiver.
    __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kStrictModeByteOffset));
    __ And(a7, a3, Operand(1 << SharedFunctionInfo::kStrictModeBitWithinByte));
    __ Branch(&shift_arguments, ne, a7, Operand(zero_reg));

    // Do not transform the receiver for native (Compilerhints already in a3).
    __ lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kNativeByteOffset));
    __ And(a7, a3, Operand(1 << SharedFunctionInfo::kNativeBitWithinByte));
    __ Branch(&shift_arguments, ne, a7, Operand(zero_reg));

    // Compute the receiver in sloppy mode.
    // Load first argument in a2. a2 = -kPointerSize(sp + n_args << 2).
    __ dsll(at, a0, kPointerSizeLog2);
    __ daddu(a2, sp, at);
    __ ld(a2, MemOperand(a2, -kPointerSize));
    // a0: actual number of arguments
    // a1: function
    // a2: first argument
    __ JumpIfSmi(a2, &convert_to_object, a6);

    __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
    __ Branch(&use_global_proxy, eq, a2, Operand(a3));
    __ LoadRoot(a3, Heap::kNullValueRootIndex);
    __ Branch(&use_global_proxy, eq, a2, Operand(a3));

    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ GetObjectType(a2, a3, a3);
    __ Branch(&shift_arguments, ge, a3, Operand(FIRST_SPEC_OBJECT_TYPE));

    __ bind(&convert_to_object);
    // Enter an internal frame in order to preserve argument count.
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(a0);
      __ Push(a0, a2);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ mov(a2, v0);

      __ pop(a0);
      __ SmiUntag(a0);
      // Leave internal frame.
    }
    // Restore the function to a1, and the flag to a4.
    __ dsll(at, a0, kPointerSizeLog2);
    __ daddu(at, sp, at);
    __ ld(a1, MemOperand(at));
    __ Branch(USE_DELAY_SLOT, &patch_receiver);
    __ li(a4, Operand(0, RelocInfo::NONE32));

    __ bind(&use_global_proxy);
    __ ld(a2, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
    __ ld(a2, FieldMemOperand(a2, GlobalObject::kGlobalProxyOffset));

    __ bind(&patch_receiver);
    __ dsll(at, a0, kPointerSizeLog2);
    __ daddu(a3, sp, at);
    __ sd(a2, MemOperand(a3, -kPointerSize));

    __ Branch(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ li(a4, Operand(1, RelocInfo::NONE32));  // Indicate function proxy.
  __ Branch(&shift_arguments, eq, a2, Operand(JS_FUNCTION_PROXY_TYPE));

  __ bind(&non_function);
  __ li(a4, Operand(2, RelocInfo::NONE32));  // Indicate non-function.

  // 3c. Patch the first argument when calling a non-function.  The
  //     CALL_NON_FUNCTION builtin expects the non-function callee as
  //     receiver, so overwrite the first argument which will ultimately
  //     become the receiver.
  // a0: actual number of arguments
  // a1: function
  // a4: call type (0: JS function, 1: function proxy, 2: non-function)
  __ dsll(at, a0, kPointerSizeLog2);
  __ daddu(a2, sp, at);
  __ sd(a1, MemOperand(a2, -kPointerSize));

  // 4. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  // a4: call type (0: JS function, 1: function proxy, 2: non-function)
  __ bind(&shift_arguments);
  { Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ dsll(at, a0, kPointerSizeLog2);
    __ daddu(a2, sp, at);

    __ bind(&loop);
    __ ld(at, MemOperand(a2, -kPointerSize));
    __ sd(at, MemOperand(a2));
    __ Dsubu(a2, a2, Operand(kPointerSize));
    __ Branch(&loop, ne, a2, Operand(sp));
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Dsubu(a0, a0, Operand(1));
    __ Pop();
  }

  // 5a. Call non-function via tail call to CALL_NON_FUNCTION builtin,
  //     or a function proxy via CALL_FUNCTION_PROXY.
  // a0: actual number of arguments
  // a1: function
  // a4: call type (0: JS function, 1: function proxy, 2: non-function)
  { Label function, non_proxy;
    __ Branch(&function, eq, a4, Operand(zero_reg));
    // Expected number of arguments is 0 for CALL_NON_FUNCTION.
    __ mov(a2, zero_reg);
    __ Branch(&non_proxy, ne, a4, Operand(1));

    __ push(a1);  // Re-add proxy object as additional argument.
    __ Daddu(a0, a0, Operand(1));
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
  __ ld(a3, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  // The argument count is stored as int32_t on 64-bit platforms.
  // TODO(plind): Smi on 32-bit platforms.
  __ lw(a2,
         FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  // Check formal and actual parameter counts.
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET, ne, a2, Operand(a0));

  __ ld(a3, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
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
    __ ld(a0, MemOperand(fp, kFunctionOffset));  // Get the function.
    __ push(a0);
    __ ld(a0, MemOperand(fp, kArgsOffset));  // Get the args array.
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
    __ dsubu(a2, sp, a2);
    // Check if the arguments will overflow the stack.
    __ SmiScale(a7, v0, kPointerSizeLog2);
    __ Branch(&okay, gt, a2, Operand(a7));  // Signed comparison.

    // Out of stack space.
    __ ld(a1, MemOperand(fp, kFunctionOffset));
    __ Push(a1, v0);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    // End of stack check.

    // Push current limit and index.
    __ bind(&okay);
    __ mov(a1, zero_reg);
    __ Push(v0, a1);  // Limit and initial index.

    // Get the receiver.
    __ ld(a0, MemOperand(fp, kRecvOffset));

    // Check that the function is a JS function (otherwise it must be a proxy).
    Label push_receiver;
    __ ld(a1, MemOperand(fp, kFunctionOffset));
    __ GetObjectType(a1, a2, a2);
    __ Branch(&push_receiver, ne, a2, Operand(JS_FUNCTION_TYPE));

    // Change context eagerly to get the right global object if necessary.
    __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
    // Load the shared function info while the function is still in a1.
    __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));

    // Compute the receiver.
    // Do not transform the receiver for strict mode functions.
    Label call_to_object, use_global_proxy;
    __ lbu(a7, FieldMemOperand(a2, SharedFunctionInfo::kStrictModeByteOffset));
    __ And(a7, a7, Operand(1 << SharedFunctionInfo::kStrictModeBitWithinByte));
    __ Branch(&push_receiver, ne, a7, Operand(zero_reg));

    // Do not transform the receiver for native (Compilerhints already in a2).
    __ lbu(a7, FieldMemOperand(a2, SharedFunctionInfo::kNativeByteOffset));
    __ And(a7, a7, Operand(1 << SharedFunctionInfo::kNativeBitWithinByte));
    __ Branch(&push_receiver, ne, a7, Operand(zero_reg));

    // Compute the receiver in sloppy mode.
    __ JumpIfSmi(a0, &call_to_object);
    __ LoadRoot(a1, Heap::kNullValueRootIndex);
    __ Branch(&use_global_proxy, eq, a0, Operand(a1));
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ Branch(&use_global_proxy, eq, a0, Operand(a2));

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

    __ bind(&use_global_proxy);
    __ ld(a0, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
    __ ld(a0, FieldMemOperand(a0, GlobalObject::kGlobalProxyOffset));

    // Push the receiver.
    // a0: receiver
    __ bind(&push_receiver);
    __ push(a0);

    // Copy all arguments from the array to the stack.
    Label entry, loop;
    __ ld(a0, MemOperand(fp, kIndexOffset));
    __ Branch(&entry);

    // Load the current argument from the arguments array and push it to the
    // stack.
    // a0: current argument index
    __ bind(&loop);
    __ ld(a1, MemOperand(fp, kArgsOffset));
    __ Push(a1, a0);

    // Call the runtime to access the property in the arguments array.
    __ CallRuntime(Runtime::kGetProperty, 2);
    __ push(v0);

    // Use inline caching to access the arguments.
    __ ld(a0, MemOperand(fp, kIndexOffset));
    __ Daddu(a0, a0, Operand(Smi::FromInt(1)));
    __ sd(a0, MemOperand(fp, kIndexOffset));

    // Test if the copy loop has finished copying all the elements from the
    // arguments object.
    __ bind(&entry);
    __ ld(a1, MemOperand(fp, kLimitOffset));
    __ Branch(&loop, ne, a0, Operand(a1));

    // Call the function.
    Label call_proxy;
    ParameterCount actual(a0);
    __ SmiUntag(a0);
    __ ld(a1, MemOperand(fp, kFunctionOffset));
    __ GetObjectType(a1, a2, a2);
    __ Branch(&call_proxy, ne, a2, Operand(JS_FUNCTION_TYPE));

    __ InvokeFunction(a1, actual, CALL_FUNCTION, NullCallWrapper());

    frame_scope.GenerateLeaveFrame();
    __ Ret(USE_DELAY_SLOT);
    __ Daddu(sp, sp, Operand(3 * kPointerSize));  // In delay slot.

    // Call the function proxy.
    __ bind(&call_proxy);
    __ push(a1);  // Add function proxy as last argument.
    __ Daddu(a0, a0, Operand(1));
    __ li(a2, Operand(0, RelocInfo::NONE32));
    __ GetBuiltinFunction(a1, Builtins::CALL_FUNCTION_PROXY);
    __ Call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
    // Tear down the internal frame and remove function, receiver and args.
  }

  __ Ret(USE_DELAY_SLOT);
  __ Daddu(sp, sp, Operand(3 * kPointerSize));  // In delay slot.
}


static void ArgumentAdaptorStackCheck(MacroAssembler* masm,
                                      Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- a0 : actual number of arguments
  //  -- a1 : function (passed through to callee)
  //  -- a2 : expected number of arguments
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(a5, Heap::kRealStackLimitRootIndex);
  // Make a5 the space we have left. The stack might already be overflowed
  // here which will cause a5 to become negative.
  __ dsubu(a5, sp, a5);
  // Check if the arguments will overflow the stack.
  __ dsll(at, a2, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(stack_overflow, le, a5, Operand(at));
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  // __ sll(a0, a0, kSmiTagSize);
  __ dsll32(a0, a0, 0);
  __ li(a4, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | a4.bit() | fp.bit() | ra.bit());
  __ Daddu(fp, sp,
      Operand(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ld(a1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                             kPointerSize)));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  // Adjust for the receiver.
  __ Daddu(sp, sp, Operand(kPointerSize));
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // State setup as expected by MacroAssembler::InvokePrologue.
  // ----------- S t a t e -------------
  //  -- a0: actual arguments count
  //  -- a1: function (passed through to callee)
  //  -- a2: expected arguments count
  // -----------------------------------

  Label stack_overflow;
  ArgumentAdaptorStackCheck(masm, &stack_overflow);
  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ ld(a3, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
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
    __ SmiScale(a0, a0, kPointerSizeLog2);
    __ Daddu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Daddu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ dsll(a2, a2, kPointerSizeLog2);
    __ dsubu(a2, a0, a2);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: copy end address
    // a3: code entry to call

    Label copy;
    __ bind(&copy);
    __ ld(a4, MemOperand(a0));
    __ push(a4);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a2));
    __ daddiu(a0, a0, -kPointerSize);  // In delay slot.

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
    __ SmiScale(a0, a0, kPointerSizeLog2);
    __ Daddu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Daddu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address. Also adjust for return address.
    __ Daddu(a7, fp, kPointerSize);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: code entry to call
    // a7: copy end address
    Label copy;
    __ bind(&copy);
    __ ld(a4, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Dsubu(sp, sp, kPointerSize);
    __ Dsubu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a7));
    __ sd(a4, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: code entry to call
    __ LoadRoot(a4, Heap::kUndefinedValueRootIndex);
    __ dsll(a6, a2, kPointerSizeLog2);
    __ Dsubu(a2, fp, Operand(a6));
    // Adjust for frame.
    __ Dsubu(a2, a2, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ Dsubu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(a2));
    __ sd(a4, MemOperand(sp));
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

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    EnterArgumentsAdaptorFrame(masm);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    __ break_(0xCC);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS64

// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM

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
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
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
    __ SmiTst(r2);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction);
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
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
    __ SmiTst(r2);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(r2, r3, r4, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
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
    __ Assert(eq, kUnexpectedStringFunction);
  }

  // Load the first arguments in r0 and get rid of the rest.
  Label no_arguments;
  __ cmp(r0, Operand::Zero());
  __ b(eq, &no_arguments);
  // First args = sp[(argc - 1) * 4].
  __ sub(r0, r0, Operand(1));
  __ ldr(r0, MemOperand(sp, r0, LSL, kPointerSizeLog2, PreIndex));
  // sp now point to args[0], drop args[0] + receiver.
  __ Drop(2);

  Register argument = r2;
  Label not_cached, argument_is_string;
  __ LookupNumberStringCache(r0,        // Input.
                             argument,  // Result.
                             r3,        // Scratch.
                             r4,        // Scratch.
                             r5,        // Scratch.
                             &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1, r3, r4);
  __ bind(&argument_is_string);

  // ----------- S t a t e -------------
  //  -- r2     : argument converted to string
  //  -- r1     : constructor function
  //  -- lr     : return address
  // -----------------------------------

  Label gc_required;
  __ Allocate(JSValue::kSize,
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
    __ Assert(eq, kUnexpectedStringWrapperInstanceSize);
    __ ldrb(r4, FieldMemOperand(map, Map::kUnusedPropertyFieldsOffset));
    __ cmp(r4, Operand::Zero());
    __ Assert(eq, kUnexpectedUnusedPropertiesOfStringWrapper);
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
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ push(r0);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
  }
  __ pop(function);
  __ mov(argument, r0);
  __ b(&argument_is_string);

  // Load the empty string into r2, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ bind(&no_arguments);
  __ LoadRoot(argument, Heap::kempty_stringRootIndex);
  __ Drop(1);
  __ b(&argument_is_string);

  // At this point the argument is already a string. Call runtime to
  // create a string wrapper.
  __ bind(&gc_required);
  __ IncrementCounter(counters->string_ctor_gc_required(), 1, r3, r4);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ push(argument);
    __ CallRuntime(Runtime::kNewStringWrapper, 1);
  }
  __ Ret();
}


static void CallRuntimePassFunction(
    MacroAssembler* masm, Runtime::FunctionId function_id) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  __ push(r1);
  // Push function as parameter to the runtime call.
  __ Push(r1);

  __ CallRuntime(function_id, 1);
  // Restore receiver.
  __ pop(r1);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kCodeOffset));
  __ add(r2, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r2);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ add(r0, r0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(r0);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmp(sp, Operand(ip));
  __ b(hs, &ok);

  CallRuntimePassFunction(masm, Runtime::kTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool create_memento) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- r2     : allocation site or undefined
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Should never create mementos for api functions.
  DCHECK(!is_api_function || !create_memento);

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    if (create_memento) {
      __ AssertUndefinedOrAllocationSite(r2, r3);
      __ push(r2);
    }

    // Preserve the two incoming parameters on the stack.
    __ SmiTag(r0);
    __ push(r0);  // Smi-tagged arguments count.
    __ push(r1);  // Constructor function.

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      Label undo_allocation;
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ mov(r2, Operand(debug_step_in_fp));
      __ ldr(r2, MemOperand(r2));
      __ tst(r2, r2);
      __ b(ne, &rt_call);

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

      if (!is_api_function) {
        Label allocate;
        MemOperand bit_field3 = FieldMemOperand(r2, Map::kBitField3Offset);
        // Check if slack tracking is enabled.
        __ ldr(r4, bit_field3);
        __ DecodeField<Map::ConstructionCount>(r3, r4);
        __ cmp(r3, Operand(JSFunction::kNoSlackTracking));
        __ b(eq, &allocate);
        // Decrease generous allocation count.
        __ sub(r4, r4, Operand(1 << Map::ConstructionCount::kShift));
        __ str(r4, bit_field3);
        __ cmp(r3, Operand(JSFunction::kFinishSlackTracking));
        __ b(ne, &allocate);

        __ push(r1);

        __ Push(r2, r1);  // r1 = constructor
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ pop(r2);
        __ pop(r1);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // r1: constructor function
      // r2: initial map
      __ ldrb(r3, FieldMemOperand(r2, Map::kInstanceSizeOffset));
      if (create_memento) {
        __ add(r3, r3, Operand(AllocationMemento::kSize / kPointerSize));
      }

      __ Allocate(r3, r4, r5, r6, &rt_call, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // r1: constructor function
      // r2: initial map
      // r3: object size (not including memento if create_memento)
      // r4: JSObject (not tagged)
      __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
      __ mov(r5, r4);
      DCHECK_EQ(0 * kPointerSize, JSObject::kMapOffset);
      __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
      DCHECK_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
      __ str(r6, MemOperand(r5, kPointerSize, PostIndex));
      DCHECK_EQ(2 * kPointerSize, JSObject::kElementsOffset);
      __ str(r6, MemOperand(r5, kPointerSize, PostIndex));

      // Fill all the in-object properties with the appropriate filler.
      // r1: constructor function
      // r2: initial map
      // r3: object size (in words, including memento if create_memento)
      // r4: JSObject (not tagged)
      // r5: First in-object property of JSObject (not tagged)
      DCHECK_EQ(3 * kPointerSize, JSObject::kHeaderSize);
      __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ ldr(ip, FieldMemOperand(r2, Map::kBitField3Offset));
        __ DecodeField<Map::ConstructionCount>(ip);
        __ cmp(ip, Operand(JSFunction::kNoSlackTracking));
        __ b(eq, &no_inobject_slack_tracking);

        // Allocate object with a slack.
        __ ldr(r0, FieldMemOperand(r2, Map::kInstanceSizesOffset));
        __ Ubfx(r0, r0, Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
                kBitsPerByte);
        __ add(r0, r5, Operand(r0, LSL, kPointerSizeLog2));
        // r0: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ add(ip, r4, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
          __ cmp(r0, ip);
          __ Assert(le, kUnexpectedNumberOfPreAllocatedPropertyFields);
        }
        __ InitializeFieldsWithFiller(r5, r0, r6);
        // To allow for truncation.
        __ LoadRoot(r6, Heap::kOnePointerFillerMapRootIndex);
        // Fill the remaining fields with one pointer filler map.

        __ bind(&no_inobject_slack_tracking);
      }

      if (create_memento) {
        __ sub(ip, r3, Operand(AllocationMemento::kSize / kPointerSize));
        __ add(r0, r4, Operand(ip, LSL, kPointerSizeLog2));  // End of object.
        __ InitializeFieldsWithFiller(r5, r0, r6);

        // Fill in memento fields.
        // r5: points to the allocated but uninitialized memento.
        __ LoadRoot(r6, Heap::kAllocationMementoMapRootIndex);
        DCHECK_EQ(0 * kPointerSize, AllocationMemento::kMapOffset);
        __ str(r6, MemOperand(r5, kPointerSize, PostIndex));
        // Load the AllocationSite
        __ ldr(r6, MemOperand(sp, 2 * kPointerSize));
        DCHECK_EQ(1 * kPointerSize, AllocationMemento::kAllocationSiteOffset);
        __ str(r6, MemOperand(r5, kPointerSize, PostIndex));
      } else {
        __ add(r0, r4, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
        __ InitializeFieldsWithFiller(r5, r0, r6);
      }

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
      __ Assert(pl, kPropertyAllocationCountFailed);

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // r1: constructor
      // r3: number of elements in properties array
      // r4: JSObject
      // r5: start of next object
      __ add(r0, r3, Operand(FixedArray::kHeaderSize / kPointerSize));
      __ Allocate(
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
      DCHECK_EQ(0 * kPointerSize, JSObject::kMapOffset);
      __ str(r6, MemOperand(r2, kPointerSize, PostIndex));
      DCHECK_EQ(1 * kPointerSize, FixedArray::kLengthOffset);
      __ SmiTag(r0, r3);
      __ str(r0, MemOperand(r2, kPointerSize, PostIndex));

      // Initialize the fields to undefined.
      // r1: constructor function
      // r2: First element of FixedArray (not tagged)
      // r3: number of elements in properties array
      // r4: JSObject
      // r5: FixedArray (not tagged)
      __ add(r6, r2, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
      DCHECK_EQ(2 * kPointerSize, FixedArray::kHeaderSize);
      { Label loop, entry;
        __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
        __ b(&entry);
        __ bind(&loop);
        __ str(r0, MemOperand(r2, kPointerSize, PostIndex));
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
    if (create_memento) {
      // Get the cell or allocation site.
      __ ldr(r2, MemOperand(sp, 2 * kPointerSize));
      __ push(r2);
    }

    __ push(r1);  // argument for Runtime_NewObject
    if (create_memento) {
      __ CallRuntime(Runtime::kNewObjectWithAllocationSite, 2);
    } else {
      __ CallRuntime(Runtime::kNewObject, 1);
    }
    __ mov(r4, r0);

    // If we ended up using the runtime, and we want a memento, then the
    // runtime call made it for us, and we shouldn't do create count
    // increment.
    Label count_incremented;
    if (create_memento) {
      __ jmp(&count_incremented);
    }

    // Receiver for constructor call allocated.
    // r4: JSObject
    __ bind(&allocated);

    if (create_memento) {
      __ ldr(r2, MemOperand(sp, kPointerSize * 2));
      __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
      __ cmp(r2, r5);
      __ b(eq, &count_incremented);
      // r2 is an AllocationSite. We are creating a memento from it, so we
      // need to increment the memento create count.
      __ ldr(r3, FieldMemOperand(r2,
                                 AllocationSite::kPretenureCreateCountOffset));
      __ add(r3, r3, Operand(Smi::FromInt(1)));
      __ str(r3, FieldMemOperand(r2,
                                 AllocationSite::kPretenureCreateCountOffset));
      __ bind(&count_incremented);
    }

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
    __ SmiUntag(r0, r3);

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
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(r0);
      __ InvokeFunction(r1, actual, CALL_FUNCTION, NullCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function) {
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
    __ CompareObjectType(r0, r1, r3, FIRST_SPEC_OBJECT_TYPE);
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


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, FLAG_pretenuring_call_new);
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
  // r5-r6, r8 (if not FLAG_enable_ool_constant_pool) and cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Clear the context before we push it when entering the internal frame.
  __ mov(cp, Operand::Zero());

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
    if (!FLAG_enable_ool_constant_pool) {
      __ mov(r8, Operand(r4));
    }
    if (kR9Available == 1) {
      __ mov(r9, Operand(r4));
    }

    // Invoke the code and pass argc as r0.
    __ mov(r0, Operand(r3));
    if (is_construct) {
      // No type feedback cell is available
      __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
      CallConstructStub stub(masm->isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(r0);
      __ InvokeFunction(r1, actual, CALL_FUNCTION, NullCallWrapper());
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


void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kCompileLazy);
  GenerateTailCallToReturnedCode(masm);
}


static void CallCompileOptimized(MacroAssembler* masm, bool concurrent) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  __ push(r1);
  // Push function as parameter to the runtime call.
  __ Push(r1);
  // Whether to compile in a background thread.
  __ Push(masm->isolate()->factory()->ToBoolean(concurrent));

  __ CallRuntime(Runtime::kCompileOptimized, 2);
  // Restore receiver.
  __ pop(r1);
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

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r0 - contains return address (beginning of patch sequence)
  //   r1 - isolate
  FrameScope scope(masm, StackFrame::MANUAL);
  __ stm(db_w, sp, r0.bit() | r1.bit() | fp.bit() | lr.bit());
  __ PrepareCallCFunction(2, 0, r2);
  __ mov(r1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ ldm(ia_w, sp, r0.bit() | r1.bit() | fp.bit() | lr.bit());
  __ mov(pc, r0);
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

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r0 - contains return address (beginning of patch sequence)
  //   r1 - isolate
  FrameScope scope(masm, StackFrame::MANUAL);
  __ stm(db_w, sp, r0.bit() | r1.bit() | fp.bit() | lr.bit());
  __ PrepareCallCFunction(2, 0, r2);
  __ mov(r1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(ExternalReference::get_mark_code_as_executed_function(
        masm->isolate()), 2);
  __ ldm(ia_w, sp, r0.bit() | r1.bit() | fp.bit() | lr.bit());

  // Perform prologue operations usually performed by the young code stub.
  __ PushFixedFrame(r1);
  __ add(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Jump to point after the code-age stub.
  __ add(r0, r0, Operand(kNoCodeAgeSequenceLength));
  __ mov(pc, r0);
}


void Builtins::Generate_MarkCodeAsExecutedTwice(MacroAssembler* masm) {
  GenerateMakeCodeYoungAgainCommon(masm);
}


static void Generate_NotifyStubFailureHelper(MacroAssembler* masm,
                                             SaveFPRegsMode save_doubles) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ stm(db_w, sp, kJSCallerSaved | kCalleeSaved);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, 0, save_doubles);
    __ ldm(ia_w, sp, kJSCallerSaved | kCalleeSaved);
  }

  __ add(sp, sp, Operand(kPointerSize));  // Ignore state
  __ mov(pc, lr);  // Jump to miss handler
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
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
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


void Builtins::Generate_NotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the code object is null, just return to the unoptimized code.
  Label skip;
  __ cmp(r0, Operand(Smi::FromInt(0)));
  __ b(ne, &skip);
  __ Ret();

  __ bind(&skip);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ ldr(r1, FieldMemOperand(r0, Code::kDeoptimizationDataOffset));

  { ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    if (FLAG_enable_ool_constant_pool) {
      __ ldr(pp, FieldMemOperand(r0, Code::kConstantPoolOffset));
    }

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ ldr(r1, FieldMemOperand(r1, FixedArray::OffsetOfElementAt(
        DeoptimizationInputData::kOsrPcOffsetIndex)));

    // Compute the target address = code_obj + header_size + osr_offset
    // <entry_addr> = <code_obj> + #header_size + <osr_offset>
    __ add(r0, r0, Operand::SmiUntag(r1));
    __ add(lr, r0, Operand(Code::kHeaderSize - kHeapObjectTag));

    // And "return" to the OSR entry point of the function.
    __ Ret();
  }
}


void Builtins::Generate_OsrAfterStackCheck(MacroAssembler* masm) {
  // We check the stack limit as indicator that recompilation might be done.
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmp(sp, Operand(ip));
  __ b(hs, &ok);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kStackGuard, 0);
  }
  __ Jump(masm->isolate()->builtins()->OnStackReplacement(),
          RelocInfo::CODE_TARGET);

  __ bind(&ok);
  __ Ret();
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r0: actual number of arguments
  { Label done;
    __ cmp(r0, Operand::Zero());
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
  __ mov(r4, Operand::Zero());  // indicate regular JS_FUNCTION
  { Label convert_to_object, use_global_proxy, patch_receiver;
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

    // Compute the receiver in sloppy mode.
    __ add(r2, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ ldr(r2, MemOperand(r2, -kPointerSize));
    // r0: actual number of arguments
    // r1: function
    // r2: first argument
    __ JumpIfSmi(r2, &convert_to_object);

    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ cmp(r2, r3);
    __ b(eq, &use_global_proxy);
    __ LoadRoot(r3, Heap::kNullValueRootIndex);
    __ cmp(r2, r3);
    __ b(eq, &use_global_proxy);

    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CompareObjectType(r2, r3, r3, FIRST_SPEC_OBJECT_TYPE);
    __ b(ge, &shift_arguments);

    __ bind(&convert_to_object);

    {
      // Enter an internal frame in order to preserve argument count.
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(r0);
      __ push(r0);

      __ push(r2);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ mov(r2, r0);

      __ pop(r0);
      __ SmiUntag(r0);

      // Exit the internal frame.
    }

    // Restore the function to r1, and the flag to r4.
    __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    __ mov(r4, Operand::Zero());
    __ jmp(&patch_receiver);

    __ bind(&use_global_proxy);
  __ ldr(r2, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
  __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalProxyOffset));

    __ bind(&patch_receiver);
    __ add(r3, sp, Operand(r0, LSL, kPointerSizeLog2));
    __ str(r2, MemOperand(r3, -kPointerSize));

    __ jmp(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ mov(r4, Operand(1, RelocInfo::NONE32));  // indicate function proxy
  __ cmp(r2, Operand(JS_FUNCTION_PROXY_TYPE));
  __ b(eq, &shift_arguments);
  __ bind(&non_function);
  __ mov(r4, Operand(2, RelocInfo::NONE32));  // indicate non-function

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
    __ mov(r2, Operand::Zero());
    __ cmp(r4, Operand(1));
    __ b(ne, &non_proxy);

    __ push(r1);  // re-add proxy object as additional argument
    __ add(r0, r0, Operand(1));
    __ GetBuiltinFunction(r1, Builtins::CALL_FUNCTION_PROXY);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    __ bind(&non_proxy);
    __ GetBuiltinFunction(r1, Builtins::CALL_NON_FUNCTION);
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
  __ SmiUntag(r2);
  __ cmp(r2, r0);  // Check formal and actual parameter counts.
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET,
          ne);

  __ ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  ParameterCount expected(0);
  __ InvokeCode(r3, expected, expected, JUMP_FUNCTION, NullCallWrapper());
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
    FrameAndConstantPoolScope frame_scope(masm, StackFrame::INTERNAL);

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
    __ cmp(r2, Operand::PointerOffsetFromSmiKey(r0));
    __ b(gt, &okay);  // Signed comparison.

    // Out of stack space.
    __ ldr(r1, MemOperand(fp, kFunctionOffset));
    __ Push(r1, r0);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    // End of stack check.

    // Push current limit and index.
    __ bind(&okay);
    __ push(r0);  // limit
    __ mov(r1, Operand::Zero());  // initial index
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
    Label call_to_object, use_global_proxy;
    __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(r2, Operand(1 << (SharedFunctionInfo::kStrictModeFunction +
                             kSmiTagSize)));
    __ b(ne, &push_receiver);

    // Do not transform the receiver for strict mode functions.
    __ tst(r2, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
    __ b(ne, &push_receiver);

    // Compute the receiver in sloppy mode.
    __ JumpIfSmi(r0, &call_to_object);
    __ LoadRoot(r1, Heap::kNullValueRootIndex);
    __ cmp(r0, r1);
    __ b(eq, &use_global_proxy);
    __ LoadRoot(r1, Heap::kUndefinedValueRootIndex);
    __ cmp(r0, r1);
    __ b(eq, &use_global_proxy);

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

    __ bind(&use_global_proxy);
    __ ldr(r0, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
    __ ldr(r0, FieldMemOperand(r0, GlobalObject::kGlobalProxyOffset));

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
    __ Push(r1, r0);

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

    // Call the function.
    Label call_proxy;
    ParameterCount actual(r0);
    __ SmiUntag(r0);
    __ ldr(r1, MemOperand(fp, kFunctionOffset));
    __ CompareObjectType(r1, r2, r2, JS_FUNCTION_TYPE);
    __ b(ne, &call_proxy);
    __ InvokeFunction(r1, actual, CALL_FUNCTION, NullCallWrapper());

    frame_scope.GenerateLeaveFrame();
    __ add(sp, sp, Operand(3 * kPointerSize));
    __ Jump(lr);

    // Call the function proxy.
    __ bind(&call_proxy);
    __ push(r1);  // add function proxy as last argument
    __ add(r0, r0, Operand(1));
    __ mov(r2, Operand::Zero());
    __ GetBuiltinFunction(r1, Builtins::CALL_FUNCTION_PROXY);
    __ Call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    // Tear down the internal frame and remove function, receiver and args.
  }
  __ add(sp, sp, Operand(3 * kPointerSize));
  __ Jump(lr);
}


static void ArgumentAdaptorStackCheck(MacroAssembler* masm,
                                      Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- r0 : actual number of arguments
  //  -- r1 : function (passed through to callee)
  //  -- r2 : expected number of arguments
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(r5, Heap::kRealStackLimitRootIndex);
  // Make r5 the space we have left. The stack might already be overflowed
  // here which will cause r5 to become negative.
  __ sub(r5, sp, r5);
  // Check if the arguments will overflow the stack.
  __ cmp(r5, Operand(r2, LSL, kPointerSizeLog2));
  __ b(le, stack_overflow);  // Signed comparison.
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(r0);
  __ mov(r4, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ stm(db_w, sp, r0.bit() | r1.bit() | r4.bit() |
                   (FLAG_enable_ool_constant_pool ? pp.bit() : 0) |
                   fp.bit() | lr.bit());
  __ add(fp, sp,
         Operand(StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ ldr(r1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                              kPointerSize)));

  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR);
  __ add(sp, sp, Operand::PointerOffsetFromSmiKey(r1));
  __ add(sp, sp, Operand(kPointerSize));  // adjust for receiver
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : actual number of arguments
  //  -- r1 : function (passed through to callee)
  //  -- r2 : expected number of arguments
  // -----------------------------------

  Label stack_overflow;
  ArgumentAdaptorStackCheck(masm, &stack_overflow);
  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
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
    __ add(r0, fp, Operand::PointerOffsetFromSmiKey(r0));
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
    __ add(r0, fp, Operand::PointerOffsetFromSmiKey(r0));

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
    // Adjust for frame.
    __ sub(r2, r2, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                           2 * kPointerSize));

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

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    EnterArgumentsAdaptorFrame(masm);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    __ bkpt(0);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

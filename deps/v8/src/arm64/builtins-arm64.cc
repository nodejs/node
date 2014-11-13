// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/codegen.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the native context.
  __ Ldr(result, GlobalObjectMemOperand());
  __ Ldr(result,
         FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the InternalArray function from the native context.
  __ Ldr(result,
         MemOperand(result,
                    Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the native context.
  __ Ldr(result, GlobalObjectMemOperand());
  __ Ldr(result,
         FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the InternalArray function from the native context.
  __ Ldr(result, ContextMemOperand(result,
                                   Context::INTERNAL_ARRAY_FUNCTION_INDEX));
}


void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  // ----------- S t a t e -------------
  //  -- x0                 : number of arguments excluding receiver
  //  -- x1                 : called function (only guaranteed when
  //                          extra_args requires it)
  //  -- cp                 : context
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument (argc == x0)
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ Push(x1);
  } else {
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects x0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ Add(x0, x0, num_extra_args + 1);
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_InternalArrayCode");
  Label generic_array_code;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, x1);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ Ldr(x10, FieldMemOperand(x1, JSFunction::kPrototypeOrInitialMapOffset));
    __ Tst(x10, kSmiTagMask);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction);
    __ CompareObjectType(x10, x11, x12, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ArrayCode");
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, x1);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ Ldr(x10, FieldMemOperand(x1, JSFunction::kPrototypeOrInitialMapOffset));
    __ Tst(x10, kSmiTagMask);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction);
    __ CompareObjectType(x10, x11, x12, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  // Run the native code for the Array function called as a normal function.
  __ LoadRoot(x2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_StringConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_StringConstructCode");
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_ctor_calls(), 1, x10, x11);

  Register argc = x0;
  Register function = x1;
  if (FLAG_debug_code) {
    __ LoadGlobalFunction(Context::STRING_FUNCTION_INDEX, x10);
    __ Cmp(function, x10);
    __ Assert(eq, kUnexpectedStringFunction);
  }

  // Load the first arguments in x0 and get rid of the rest.
  Label no_arguments;
  __ Cbz(argc, &no_arguments);
  // First args = sp[(argc - 1) * 8].
  __ Sub(argc, argc, 1);
  __ Drop(argc, kXRegSize);
  // jssp now point to args[0], load and drop args[0] + receiver.
  Register arg = argc;
  __ Ldr(arg, MemOperand(jssp, 2 * kPointerSize, PostIndex));
  argc = NoReg;

  Register argument = x2;
  Label not_cached, argument_is_string;
  __ LookupNumberStringCache(arg,        // Input.
                             argument,   // Result.
                             x10,        // Scratch.
                             x11,        // Scratch.
                             x12,        // Scratch.
                             &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1, x10, x11);
  __ Bind(&argument_is_string);

  // ----------- S t a t e -------------
  //  -- x2     : argument converted to string
  //  -- x1     : constructor function
  //  -- lr     : return address
  // -----------------------------------

  Label gc_required;
  Register new_obj = x0;
  __ Allocate(JSValue::kSize, new_obj, x10, x11, &gc_required, TAG_OBJECT);

  // Initialize the String object.
  Register map = x3;
  __ LoadGlobalFunctionInitialMap(function, map, x10);
  if (FLAG_debug_code) {
    __ Ldrb(x4, FieldMemOperand(map, Map::kInstanceSizeOffset));
    __ Cmp(x4, JSValue::kSize >> kPointerSizeLog2);
    __ Assert(eq, kUnexpectedStringWrapperInstanceSize);
    __ Ldrb(x4, FieldMemOperand(map, Map::kUnusedPropertyFieldsOffset));
    __ Cmp(x4, 0);
    __ Assert(eq, kUnexpectedUnusedPropertiesOfStringWrapper);
  }
  __ Str(map, FieldMemOperand(new_obj, HeapObject::kMapOffset));

  Register empty = x3;
  __ LoadRoot(empty, Heap::kEmptyFixedArrayRootIndex);
  __ Str(empty, FieldMemOperand(new_obj, JSObject::kPropertiesOffset));
  __ Str(empty, FieldMemOperand(new_obj, JSObject::kElementsOffset));

  __ Str(argument, FieldMemOperand(new_obj, JSValue::kValueOffset));

  // Ensure the object is fully initialized.
  STATIC_ASSERT(JSValue::kSize == (4 * kPointerSize));

  __ Ret();

  // The argument was not found in the number to string cache. Check
  // if it's a string already before calling the conversion builtin.
  Label convert_argument;
  __ Bind(&not_cached);
  __ JumpIfSmi(arg, &convert_argument);

  // Is it a String?
  __ Ldr(x10, FieldMemOperand(x0, HeapObject::kMapOffset));
  __ Ldrb(x11, FieldMemOperand(x10, Map::kInstanceTypeOffset));
  __ Tbnz(x11, MaskToBit(kIsNotStringMask), &convert_argument);
  __ Mov(argument, arg);
  __ IncrementCounter(counters->string_ctor_string_value(), 1, x10, x11);
  __ B(&argument_is_string);

  // Invoke the conversion builtin and put the result into x2.
  __ Bind(&convert_argument);
  __ Push(function);  // Preserve the function.
  __ IncrementCounter(counters->string_ctor_conversions(), 1, x10, x11);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(arg);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
  }
  __ Pop(function);
  __ Mov(argument, x0);
  __ B(&argument_is_string);

  // Load the empty string into x2, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ Bind(&no_arguments);
  __ LoadRoot(argument, Heap::kempty_stringRootIndex);
  __ Drop(1);
  __ B(&argument_is_string);

  // At this point the argument is already a string. Call runtime to create a
  // string wrapper.
  __ Bind(&gc_required);
  __ IncrementCounter(counters->string_ctor_gc_required(), 1, x10, x11);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(argument);
    __ CallRuntime(Runtime::kNewStringWrapper, 1);
  }
  __ Ret();
}


static void CallRuntimePassFunction(MacroAssembler* masm,
                                    Runtime::FunctionId function_id) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  //   - Push a copy of the function onto the stack.
  //   - Push another copy as a parameter to the runtime call.
  __ Push(x1, x1);

  __ CallRuntime(function_id, 1);

  //   - Restore receiver.
  __ Pop(x1);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x2, FieldMemOperand(x2, SharedFunctionInfo::kCodeOffset));
  __ Add(x2, x2, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x2);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ Add(x0, x0, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x0);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere. However, not
  // checking may delay installing ready functions, and always checking would be
  // quite expensive. A good compromise is to first check against stack limit as
  // a cue for an interrupt signal.
  Label ok;
  __ CompareRoot(masm->StackPointer(), Heap::kStackLimitRootIndex);
  __ B(hs, &ok);

  CallRuntimePassFunction(masm, Runtime::kTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ Bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool create_memento) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x2     : allocation site or undefined
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubHelper");
  // Should never create mementos for api functions.
  DCHECK(!is_api_function || !create_memento);

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the three incoming parameters on the stack.
    if (create_memento) {
      __ AssertUndefinedOrAllocationSite(x2, x10);
      __ Push(x2);
    }

    Register argc = x0;
    Register constructor = x1;
    // x1: constructor function
    __ SmiTag(argc);
    __ Push(argc, constructor);
    // sp[0] : Constructor function.
    // sp[1]: number of arguments (smi-tagged)

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      Label undo_allocation;
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ Mov(x2, Operand(debug_step_in_fp));
      __ Ldr(x2, MemOperand(x2));
      __ Cbnz(x2, &rt_call);
      // Load the initial map and verify that it is in fact a map.
      Register init_map = x2;
      __ Ldr(init_map,
             FieldMemOperand(constructor,
                             JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(init_map, &rt_call);
      __ JumpIfNotObjectType(init_map, x10, x11, MAP_TYPE, &rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the initial
      // map's instance type would be JS_FUNCTION_TYPE.
      __ CompareInstanceType(init_map, x10, JS_FUNCTION_TYPE);
      __ B(eq, &rt_call);

      Register constructon_count = x14;
      if (!is_api_function) {
        Label allocate;
        MemOperand bit_field3 =
            FieldMemOperand(init_map, Map::kBitField3Offset);
        // Check if slack tracking is enabled.
        __ Ldr(x4, bit_field3);
        __ DecodeField<Map::ConstructionCount>(constructon_count, x4);
        __ Cmp(constructon_count, Operand(JSFunction::kNoSlackTracking));
        __ B(eq, &allocate);
        // Decrease generous allocation count.
        __ Subs(x4, x4, Operand(1 << Map::ConstructionCount::kShift));
        __ Str(x4, bit_field3);
        __ Cmp(constructon_count, Operand(JSFunction::kFinishSlackTracking));
        __ B(ne, &allocate);

        // Push the constructor and map to the stack, and the constructor again
        // as argument to the runtime call.
        __ Push(constructor, init_map, constructor);
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);
        __ Pop(init_map, constructor);
        __ Mov(constructon_count, Operand(JSFunction::kNoSlackTracking));
        __ Bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      Register obj_size = x3;
      Register new_obj = x4;
      __ Ldrb(obj_size, FieldMemOperand(init_map, Map::kInstanceSizeOffset));
      if (create_memento) {
        __ Add(x7, obj_size,
               Operand(AllocationMemento::kSize / kPointerSize));
        __ Allocate(x7, new_obj, x10, x11, &rt_call, SIZE_IN_WORDS);
      } else {
        __ Allocate(obj_size, new_obj, x10, x11, &rt_call, SIZE_IN_WORDS);
      }

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // NB. the object pointer is not tagged, so MemOperand is used.
      Register empty = x5;
      __ LoadRoot(empty, Heap::kEmptyFixedArrayRootIndex);
      __ Str(init_map, MemOperand(new_obj, JSObject::kMapOffset));
      STATIC_ASSERT(JSObject::kElementsOffset ==
          (JSObject::kPropertiesOffset + kPointerSize));
      __ Stp(empty, empty, MemOperand(new_obj, JSObject::kPropertiesOffset));

      Register first_prop = x5;
      __ Add(first_prop, new_obj, JSObject::kHeaderSize);

      // Fill all of the in-object properties with the appropriate filler.
      Register filler = x7;
      __ LoadRoot(filler, Heap::kUndefinedValueRootIndex);

      // Obtain number of pre-allocated property fields and in-object
      // properties.
      Register prealloc_fields = x10;
      Register inobject_props = x11;
      Register inst_sizes = x11;
      __ Ldr(inst_sizes, FieldMemOperand(init_map, Map::kInstanceSizesOffset));
      __ Ubfx(prealloc_fields, inst_sizes,
              Map::kPreAllocatedPropertyFieldsByte * kBitsPerByte,
              kBitsPerByte);
      __ Ubfx(inobject_props, inst_sizes,
              Map::kInObjectPropertiesByte * kBitsPerByte, kBitsPerByte);

      // Calculate number of property fields in the object.
      Register prop_fields = x6;
      __ Sub(prop_fields, obj_size, JSObject::kHeaderSize / kPointerSize);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ Cmp(constructon_count, Operand(JSFunction::kNoSlackTracking));
        __ B(eq, &no_inobject_slack_tracking);
        constructon_count = NoReg;

        // Fill the pre-allocated fields with undef.
        __ FillFields(first_prop, prealloc_fields, filler);

        // Update first_prop register to be the offset of the first field after
        // pre-allocated fields.
        __ Add(first_prop, first_prop,
               Operand(prealloc_fields, LSL, kPointerSizeLog2));

        if (FLAG_debug_code) {
          Register obj_end = x14;
          __ Add(obj_end, new_obj, Operand(obj_size, LSL, kPointerSizeLog2));
          __ Cmp(first_prop, obj_end);
          __ Assert(le, kUnexpectedNumberOfPreAllocatedPropertyFields);
        }

        // Fill the remaining fields with one pointer filler map.
        __ LoadRoot(filler, Heap::kOnePointerFillerMapRootIndex);
        __ Sub(prop_fields, prop_fields, prealloc_fields);

        __ bind(&no_inobject_slack_tracking);
      }
      if (create_memento) {
        // Fill the pre-allocated fields with undef.
        __ FillFields(first_prop, prop_fields, filler);
        __ Add(first_prop, new_obj, Operand(obj_size, LSL, kPointerSizeLog2));
        __ LoadRoot(x14, Heap::kAllocationMementoMapRootIndex);
        DCHECK_EQ(0 * kPointerSize, AllocationMemento::kMapOffset);
        __ Str(x14, MemOperand(first_prop, kPointerSize, PostIndex));
        // Load the AllocationSite
        __ Peek(x14, 2 * kXRegSize);
        DCHECK_EQ(1 * kPointerSize, AllocationMemento::kAllocationSiteOffset);
        __ Str(x14, MemOperand(first_prop, kPointerSize, PostIndex));
        first_prop = NoReg;
      } else {
        // Fill all of the property fields with undef.
        __ FillFields(first_prop, prop_fields, filler);
        first_prop = NoReg;
        prop_fields = NoReg;
      }

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on. Any
      // failures need to undo the allocation, so that the heap is in a
      // consistent state and verifiable.
      __ Add(new_obj, new_obj, kHeapObjectTag);

      // Check if a non-empty properties array is needed. Continue with
      // allocated object if not, or fall through to runtime call if it is.
      Register element_count = x3;
      __ Ldrb(element_count,
              FieldMemOperand(init_map, Map::kUnusedPropertyFieldsOffset));
      // The field instance sizes contains both pre-allocated property fields
      // and in-object properties.
      __ Add(element_count, element_count, prealloc_fields);
      __ Subs(element_count, element_count, inobject_props);

      // Done if no extra properties are to be allocated.
      __ B(eq, &allocated);
      __ Assert(pl, kPropertyAllocationCountFailed);

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      Register new_array = x5;
      Register array_size = x6;
      __ Add(array_size, element_count, FixedArray::kHeaderSize / kPointerSize);
      __ Allocate(array_size, new_array, x11, x12, &undo_allocation,
                  static_cast<AllocationFlags>(RESULT_CONTAINS_TOP |
                                               SIZE_IN_WORDS));

      Register array_map = x10;
      __ LoadRoot(array_map, Heap::kFixedArrayMapRootIndex);
      __ Str(array_map, MemOperand(new_array, FixedArray::kMapOffset));
      __ SmiTag(x0, element_count);
      __ Str(x0, MemOperand(new_array, FixedArray::kLengthOffset));

      // Initialize the fields to undefined.
      Register elements = x10;
      __ Add(elements, new_array, FixedArray::kHeaderSize);
      __ FillFields(elements, element_count, filler);

      // Store the initialized FixedArray into the properties field of the
      // JSObject.
      __ Add(new_array, new_array, kHeapObjectTag);
      __ Str(new_array, FieldMemOperand(new_obj, JSObject::kPropertiesOffset));

      // Continue with JSObject being successfully allocated.
      __ B(&allocated);

      // Undo the setting of the new top so that the heap is verifiable. For
      // example, the map's unused properties potentially do not match the
      // allocated objects unused properties.
      __ Bind(&undo_allocation);
      __ UndoAllocationInNewSpace(new_obj, x14);
    }

    // Allocate the new receiver object using the runtime call.
    __ Bind(&rt_call);
    Label count_incremented;
    if (create_memento) {
      // Get the cell or allocation site.
      __ Peek(x4, 2 * kXRegSize);
      __ Push(x4);
      __ Push(constructor);  // Argument for Runtime_NewObject.
      __ CallRuntime(Runtime::kNewObjectWithAllocationSite, 2);
      __ Mov(x4, x0);
      // If we ended up using the runtime, and we want a memento, then the
      // runtime call made it for us, and we shouldn't do create count
      // increment.
      __ jmp(&count_incremented);
    } else {
      __ Push(constructor);  // Argument for Runtime_NewObject.
      __ CallRuntime(Runtime::kNewObject, 1);
      __ Mov(x4, x0);
    }

    // Receiver for constructor call allocated.
    // x4: JSObject
    __ Bind(&allocated);

    if (create_memento) {
      __ Peek(x10, 2 * kXRegSize);
      __ JumpIfRoot(x10, Heap::kUndefinedValueRootIndex, &count_incremented);
      // r2 is an AllocationSite. We are creating a memento from it, so we
      // need to increment the memento create count.
      __ Ldr(x5, FieldMemOperand(x10,
                                 AllocationSite::kPretenureCreateCountOffset));
      __ Add(x5, x5, Operand(Smi::FromInt(1)));
      __ Str(x5, FieldMemOperand(x10,
                                 AllocationSite::kPretenureCreateCountOffset));
      __ bind(&count_incremented);
    }

    __ Push(x4, x4);

    // Reload the number of arguments from the stack.
    // Set it up in x0 for the function call below.
    // jssp[0]: receiver
    // jssp[1]: receiver
    // jssp[2]: constructor function
    // jssp[3]: number of arguments (smi-tagged)
    __ Peek(constructor, 2 * kXRegSize);  // Load constructor.
    __ Peek(argc, 3 * kXRegSize);  // Load number of arguments.
    __ SmiUntag(argc);

    // Set up pointer to last argument.
    __ Add(x2, fp, StandardFrameConstants::kCallerSPOffset);

    // Copy arguments and receiver to the expression stack.
    // Copy 2 values every loop to use ldp/stp.
    // x0: number of arguments
    // x1: constructor function
    // x2: address of last argument (caller sp)
    // jssp[0]: receiver
    // jssp[1]: receiver
    // jssp[2]: constructor function
    // jssp[3]: number of arguments (smi-tagged)
    // Compute the start address of the copy in x3.
    __ Add(x3, x2, Operand(argc, LSL, kPointerSizeLog2));
    Label loop, entry, done_copying_arguments;
    __ B(&entry);
    __ Bind(&loop);
    __ Ldp(x10, x11, MemOperand(x3, -2 * kPointerSize, PreIndex));
    __ Push(x11, x10);
    __ Bind(&entry);
    __ Cmp(x3, x2);
    __ B(gt, &loop);
    // Because we copied values 2 by 2 we may have copied one extra value.
    // Drop it if that is the case.
    __ B(eq, &done_copying_arguments);
    __ Drop(1);
    __ Bind(&done_copying_arguments);

    // Call the function.
    // x0: number of arguments
    // x1: constructor function
    if (is_api_function) {
      __ Ldr(cp, FieldMemOperand(constructor, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(argc);
      __ InvokeFunction(constructor, actual, CALL_FUNCTION, NullCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore the context from the frame.
    // x0: result
    // jssp[0]: receiver
    // jssp[1]: constructor function
    // jssp[2]: number of arguments (smi-tagged)
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    // x0: result
    // jssp[0]: receiver (newly allocated object)
    // jssp[1]: constructor function
    // jssp[2]: number of arguments (smi-tagged)
    __ JumpIfSmi(x0, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    __ JumpIfObjectType(x0, x1, x3, FIRST_SPEC_OBJECT_TYPE, &exit, ge);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ Bind(&use_receiver);
    __ Peek(x0, 0);

    // Remove the receiver from the stack, remove caller arguments, and
    // return.
    __ Bind(&exit);
    // x0: result
    // jssp[0]: receiver (newly allocated object)
    // jssp[1]: constructor function
    // jssp[2]: number of arguments (smi-tagged)
    __ Peek(x1, 2 * kXRegSize);

    // Leave construct frame.
  }

  __ DropBySMI(x1);
  __ Drop(1);
  __ IncrementCounter(isolate->counters()->constructed_objects(), 1, x1, x2);
  __ Ret();
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, FLAG_pretenuring_call_new);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false);
}


// Input:
//   x0: code entry.
//   x1: function.
//   x2: receiver.
//   x3: argc.
//   x4: argv.
// Output:
//   x0: result.
static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from JSEntryStub::GenerateBody().
  Register function = x1;
  Register receiver = x2;
  Register argc = x3;
  Register argv = x4;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Clear the context before we push it when entering the internal frame.
  __ Mov(cp, 0);

  {
    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Set up the context from the function argument.
    __ Ldr(cp, FieldMemOperand(function, JSFunction::kContextOffset));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(function, receiver);

    // Copy arguments to the stack in a loop, in reverse order.
    // x3: argc.
    // x4: argv.
    Label loop, entry;
    // Compute the copy end address.
    __ Add(x10, argv, Operand(argc, LSL, kPointerSizeLog2));

    __ B(&entry);
    __ Bind(&loop);
    __ Ldr(x11, MemOperand(argv, kPointerSize, PostIndex));
    __ Ldr(x12, MemOperand(x11));  // Dereference the handle.
    __ Push(x12);  // Push the argument.
    __ Bind(&entry);
    __ Cmp(x10, argv);
    __ B(ne, &loop);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    // The original values have been saved in JSEntryStub::GenerateBody().
    __ LoadRoot(x19, Heap::kUndefinedValueRootIndex);
    __ Mov(x20, x19);
    __ Mov(x21, x19);
    __ Mov(x22, x19);
    __ Mov(x23, x19);
    __ Mov(x24, x19);
    __ Mov(x25, x19);
    // Don't initialize the reserved registers.
    // x26 : root register (root).
    // x27 : context pointer (cp).
    // x28 : JS stack pointer (jssp).
    // x29 : frame pointer (fp).

    __ Mov(x0, argc);
    if (is_construct) {
      // No type feedback cell is available.
      __ LoadRoot(x2, Heap::kUndefinedValueRootIndex);

      CallConstructStub stub(masm->isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(x0);
      __ InvokeFunction(function, actual, CALL_FUNCTION, NullCallWrapper());
    }
    // Exit the JS internal frame and remove the parameters (except function),
    // and return.
  }

  // Result is in x0. Return.
  __ Ret();
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
  Register function = x1;

  // Preserve function. At the same time, push arguments for
  // kCompileOptimized.
  __ LoadObject(x10, masm->isolate()->factory()->ToBoolean(concurrent));
  __ Push(function, function, x10);

  __ CallRuntime(Runtime::kCompileOptimized, 2);

  // Restore receiver.
  __ Pop(function);
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
  // internal frame to make the code fast, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // The following caller-saved registers must be saved and restored when
  // calling through to the runtime:
  //   x0 - The address from which to resume execution.
  //   x1 - isolate
  //   lr - The return address for the JSFunction itself. It has not yet been
  //        preserved on the stack because the frame setup code was replaced
  //        with a call to this stub, to handle code ageing.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Push(x0, x1, fp, lr);
    __ Mov(x1, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(
        ExternalReference::get_make_code_young_function(masm->isolate()), 2);
    __ Pop(lr, fp, x1, x0);
  }

  // The calling function has been made young again, so return to execute the
  // real frame set-up code.
  __ Br(x0);
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

  // The following caller-saved registers must be saved and restored when
  // calling through to the runtime:
  //   x0 - The address from which to resume execution.
  //   x1 - isolate
  //   lr - The return address for the JSFunction itself. It has not yet been
  //        preserved on the stack because the frame setup code was replaced
  //        with a call to this stub, to handle code ageing.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Push(x0, x1, fp, lr);
    __ Mov(x1, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(
        ExternalReference::get_mark_code_as_executed_function(
            masm->isolate()), 2);
    __ Pop(lr, fp, x1, x0);

    // Perform prologue operations usually performed by the young code stub.
    __ EmitFrameSetupForCodeAgePatching(masm);
  }

  // Jump to point after the code-age stub.
  __ Add(x0, x0, kNoCodeAgeSequenceLength);
  __ Br(x0);
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
    // TODO(jbramley): Is it correct (and appropriate) to use safepoint
    // registers here? According to the comment above, we should only need to
    // preserve the registers with parameters.
    __ PushXRegList(kSafepointSavedRegisters);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, 0, save_doubles);
    __ PopXRegList(kSafepointSavedRegisters);
  }

  // Ignore state (pushed by Deoptimizer::EntryGenerator::Generate).
  __ Drop(1);

  // Jump to the miss handler. Deoptimizer::EntryGenerator::Generate loads this
  // into lr before it jumps here.
  __ Br(lr);
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
    // Pass the deoptimization type to the runtime system.
    __ Mov(x0, Smi::FromInt(static_cast<int>(type)));
    __ Push(x0);
    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
  }

  // Get the full codegen state from the stack and untag it.
  Register state = x6;
  __ Peek(state, 0);
  __ SmiUntag(state);

  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ CompareAndBranch(
      state, FullCodeGenerator::NO_REGISTERS, ne, &with_tos_register);
  __ Drop(1);  // Remove state.
  __ Ret();

  __ Bind(&with_tos_register);
  // Reload TOS register.
  __ Peek(x0, kPointerSize);
  __ CompareAndBranch(state, FullCodeGenerator::TOS_REG, ne, &unknown_state);
  __ Drop(2);  // Remove state and TOS.
  __ Ret();

  __ Bind(&unknown_state);
  __ Abort(kInvalidFullCodegenState);
}


void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


void Builtins::Generate_NotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ Ldr(x0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ Push(x0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the code object is null, just return to the unoptimized code.
  Label skip;
  __ CompareAndBranch(x0, Smi::FromInt(0), ne, &skip);
  __ Ret();

  __ Bind(&skip);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ Ldr(x1, MemOperand(x0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ Ldrsw(w1, UntagSmiFieldMemOperand(x1, FixedArray::OffsetOfElementAt(
      DeoptimizationInputData::kOsrPcOffsetIndex)));

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Add(x0, x0, x1);
  __ Add(lr, x0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}


void Builtins::Generate_OsrAfterStackCheck(MacroAssembler* masm) {
  // We check the stack limit as indicator that recompilation might be done.
  Label ok;
  __ CompareRoot(jssp, Heap::kStackLimitRootIndex);
  __ B(hs, &ok);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kStackGuard, 0);
  }
  __ Jump(masm->isolate()->builtins()->OnStackReplacement(),
          RelocInfo::CODE_TARGET);

  __ Bind(&ok);
  __ Ret();
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  enum {
    call_type_JS_func = 0,
    call_type_func_proxy = 1,
    call_type_non_func = 2
  };
  Register argc = x0;
  Register function = x1;
  Register call_type = x4;
  Register scratch1 = x10;
  Register scratch2 = x11;
  Register receiver_type = x13;

  ASM_LOCATION("Builtins::Generate_FunctionCall");
  // 1. Make sure we have at least one argument.
  { Label done;
    __ Cbnz(argc, &done);
    __ LoadRoot(scratch1, Heap::kUndefinedValueRootIndex);
    __ Push(scratch1);
    __ Mov(argc, 1);
    __ Bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack, check
  //    if it is a function.
  Label slow, non_function;
  __ Peek(function, Operand(argc, LSL, kXRegSizeLog2));
  __ JumpIfSmi(function, &non_function);
  __ JumpIfNotObjectType(function, scratch1, receiver_type,
                         JS_FUNCTION_TYPE, &slow);

  // 3a. Patch the first argument if necessary when calling a function.
  Label shift_arguments;
  __ Mov(call_type, static_cast<int>(call_type_JS_func));
  { Label convert_to_object, use_global_proxy, patch_receiver;
    // Change context eagerly in case we need the global receiver.
    __ Ldr(cp, FieldMemOperand(function, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    // Also do not transform the receiver for native (Compilerhints already in
    // x3).
    __ Ldr(scratch1,
           FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(scratch2.W(),
           FieldMemOperand(scratch1, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestAndBranchIfAnySet(
        scratch2.W(),
        (1 << SharedFunctionInfo::kStrictModeFunction) |
        (1 << SharedFunctionInfo::kNative),
        &shift_arguments);

    // Compute the receiver in sloppy mode.
    Register receiver = x2;
    __ Sub(scratch1, argc, 1);
    __ Peek(receiver, Operand(scratch1, LSL, kXRegSizeLog2));
    __ JumpIfSmi(receiver, &convert_to_object);

    __ JumpIfRoot(receiver, Heap::kUndefinedValueRootIndex,
                  &use_global_proxy);
    __ JumpIfRoot(receiver, Heap::kNullValueRootIndex, &use_global_proxy);

    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ JumpIfObjectType(receiver, scratch1, scratch2,
                        FIRST_SPEC_OBJECT_TYPE, &shift_arguments, ge);

    __ Bind(&convert_to_object);

    {
      // Enter an internal frame in order to preserve argument count.
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(argc);

      __ Push(argc, receiver);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ Mov(receiver, x0);

      __ Pop(argc);
      __ SmiUntag(argc);

      // Exit the internal frame.
    }

    // Restore the function and flag in the registers.
    __ Peek(function, Operand(argc, LSL, kXRegSizeLog2));
    __ Mov(call_type, static_cast<int>(call_type_JS_func));
    __ B(&patch_receiver);

    __ Bind(&use_global_proxy);
    __ Ldr(receiver, GlobalObjectMemOperand());
    __ Ldr(receiver,
           FieldMemOperand(receiver, GlobalObject::kGlobalProxyOffset));


    __ Bind(&patch_receiver);
    __ Sub(scratch1, argc, 1);
    __ Poke(receiver, Operand(scratch1, LSL, kXRegSizeLog2));

    __ B(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ Bind(&slow);
  __ Mov(call_type, static_cast<int>(call_type_func_proxy));
  __ Cmp(receiver_type, JS_FUNCTION_PROXY_TYPE);
  __ B(eq, &shift_arguments);
  __ Bind(&non_function);
  __ Mov(call_type, static_cast<int>(call_type_non_func));

  // 3c. Patch the first argument when calling a non-function.  The
  //     CALL_NON_FUNCTION builtin expects the non-function callee as
  //     receiver, so overwrite the first argument which will ultimately
  //     become the receiver.
  // call type (0: JS function, 1: function proxy, 2: non-function)
  __ Sub(scratch1, argc, 1);
  __ Poke(function, Operand(scratch1, LSL, kXRegSizeLog2));

  // 4. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // call type (0: JS function, 1: function proxy, 2: non-function)
  __ Bind(&shift_arguments);
  { Label loop;
    // Calculate the copy start address (destination). Copy end address is jssp.
    __ Add(scratch2, jssp, Operand(argc, LSL, kPointerSizeLog2));
    __ Sub(scratch1, scratch2, kPointerSize);

    __ Bind(&loop);
    __ Ldr(x12, MemOperand(scratch1, -kPointerSize, PostIndex));
    __ Str(x12, MemOperand(scratch2, -kPointerSize, PostIndex));
    __ Cmp(scratch1, jssp);
    __ B(ge, &loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Sub(argc, argc, 1);
    __ Drop(1);
  }

  // 5a. Call non-function via tail call to CALL_NON_FUNCTION builtin,
  //     or a function proxy via CALL_FUNCTION_PROXY.
  // call type (0: JS function, 1: function proxy, 2: non-function)
  { Label js_function, non_proxy;
    __ Cbz(call_type, &js_function);
    // Expected number of arguments is 0 for CALL_NON_FUNCTION.
    __ Mov(x2, 0);
    __ Cmp(call_type, static_cast<int>(call_type_func_proxy));
    __ B(ne, &non_proxy);

    __ Push(function);  // Re-add proxy object as additional argument.
    __ Add(argc, argc, 1);
    __ GetBuiltinFunction(function, Builtins::CALL_FUNCTION_PROXY);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    __ Bind(&non_proxy);
    __ GetBuiltinFunction(function, Builtins::CALL_NON_FUNCTION);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
    __ Bind(&js_function);
  }

  // 5b. Get the code to call from the function and check that the number of
  //     expected arguments matches what we're providing.  If so, jump
  //     (tail-call) to the code in register edx without checking arguments.
  __ Ldr(x3, FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
  __ Ldrsw(x2,
           FieldMemOperand(x3,
             SharedFunctionInfo::kFormalParameterCountOffset));
  Label dont_adapt_args;
  __ Cmp(x2, argc);  // Check formal and actual parameter counts.
  __ B(eq, &dont_adapt_args);
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
  __ Bind(&dont_adapt_args);

  __ Ldr(x3, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
  ParameterCount expected(0);
  __ InvokeCode(x3, expected, expected, JUMP_FUNCTION, NullCallWrapper());
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_FunctionApply");
  const int kIndexOffset    =
      StandardFrameConstants::kExpressionsOffset - (2 * kPointerSize);
  const int kLimitOffset    =
      StandardFrameConstants::kExpressionsOffset - (1 * kPointerSize);
  const int kArgsOffset     =  2 * kPointerSize;
  const int kReceiverOffset =  3 * kPointerSize;
  const int kFunctionOffset =  4 * kPointerSize;

  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);

    Register args = x12;
    Register receiver = x14;
    Register function = x15;

    // Get the length of the arguments via a builtin call.
    __ Ldr(function, MemOperand(fp, kFunctionOffset));
    __ Ldr(args, MemOperand(fp, kArgsOffset));
    __ Push(function, args);
    __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);
    Register argc = x0;

    // Check the stack for overflow.
    // We are not trying to catch interruptions (e.g. debug break and
    // preemption) here, so the "real stack limit" is checked.
    Label enough_stack_space;
    __ LoadRoot(x10, Heap::kRealStackLimitRootIndex);
    __ Ldr(function, MemOperand(fp, kFunctionOffset));
    // Make x10 the space we have left. The stack might already be overflowed
    // here which will cause x10 to become negative.
    // TODO(jbramley): Check that the stack usage here is safe.
    __ Sub(x10, jssp, x10);
    // Check if the arguments will overflow the stack.
    __ Cmp(x10, Operand::UntagSmiAndScale(argc, kPointerSizeLog2));
    __ B(gt, &enough_stack_space);
    // There is not enough stack space, so use a builtin to throw an appropriate
    // error.
    __ Push(function, argc);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    // We should never return from the APPLY_OVERFLOW builtin.
    if (__ emit_debug_code()) {
      __ Unreachable();
    }

    __ Bind(&enough_stack_space);
    // Push current limit and index.
    __ Mov(x1, 0);  // Initial index.
    __ Push(argc, x1);

    Label push_receiver;
    __ Ldr(receiver, MemOperand(fp, kReceiverOffset));

    // Check that the function is a JS function. Otherwise it must be a proxy.
    // When it is not the function proxy will be invoked later.
    __ JumpIfNotObjectType(function, x10, x11, JS_FUNCTION_TYPE,
                           &push_receiver);

    // Change context eagerly to get the right global object if necessary.
    __ Ldr(cp, FieldMemOperand(function, JSFunction::kContextOffset));
    // Load the shared function info.
    __ Ldr(x2, FieldMemOperand(function,
                               JSFunction::kSharedFunctionInfoOffset));

    // Compute and push the receiver.
    // Do not transform the receiver for strict mode functions.
    Label convert_receiver_to_object, use_global_proxy;
    __ Ldr(w10, FieldMemOperand(x2, SharedFunctionInfo::kCompilerHintsOffset));
    __ Tbnz(x10, SharedFunctionInfo::kStrictModeFunction, &push_receiver);
    // Do not transform the receiver for native functions.
    __ Tbnz(x10, SharedFunctionInfo::kNative, &push_receiver);

    // Compute the receiver in sloppy mode.
    __ JumpIfSmi(receiver, &convert_receiver_to_object);
    __ JumpIfRoot(receiver, Heap::kNullValueRootIndex, &use_global_proxy);
    __ JumpIfRoot(receiver, Heap::kUndefinedValueRootIndex,
                  &use_global_proxy);

    // Check if the receiver is already a JavaScript object.
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ JumpIfObjectType(receiver, x10, x11, FIRST_SPEC_OBJECT_TYPE,
                        &push_receiver, ge);

    // Call a builtin to convert the receiver to a regular object.
    __ Bind(&convert_receiver_to_object);
    __ Push(receiver);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ Mov(receiver, x0);
    __ B(&push_receiver);

    __ Bind(&use_global_proxy);
    __ Ldr(x10, GlobalObjectMemOperand());
    __ Ldr(receiver, FieldMemOperand(x10, GlobalObject::kGlobalProxyOffset));

    // Push the receiver
    __ Bind(&push_receiver);
    __ Push(receiver);

    // Copy all arguments from the array to the stack.
    Label entry, loop;
    Register current = x0;
    __ Ldr(current, MemOperand(fp, kIndexOffset));
    __ B(&entry);

    __ Bind(&loop);
    // Load the current argument from the arguments array and push it.
    // TODO(all): Couldn't we optimize this for JS arrays?

    __ Ldr(x1, MemOperand(fp, kArgsOffset));
    __ Push(x1, current);

    // Call the runtime to access the property in the arguments array.
    __ CallRuntime(Runtime::kGetProperty, 2);
    __ Push(x0);

    // Use inline caching to access the arguments.
    __ Ldr(current, MemOperand(fp, kIndexOffset));
    __ Add(current, current, Smi::FromInt(1));
    __ Str(current, MemOperand(fp, kIndexOffset));

    // Test if the copy loop has finished copying all the elements from the
    // arguments object.
    __ Bind(&entry);
    __ Ldr(x1, MemOperand(fp, kLimitOffset));
    __ Cmp(current, x1);
    __ B(ne, &loop);

    // At the end of the loop, the number of arguments is stored in 'current',
    // represented as a smi.

    function = x1;  // From now on we want the function to be kept in x1;
    __ Ldr(function, MemOperand(fp, kFunctionOffset));

    // Call the function.
    Label call_proxy;
    ParameterCount actual(current);
    __ SmiUntag(current);
    __ JumpIfNotObjectType(function, x10, x11, JS_FUNCTION_TYPE, &call_proxy);
    __ InvokeFunction(function, actual, CALL_FUNCTION, NullCallWrapper());
    frame_scope.GenerateLeaveFrame();
    __ Drop(3);
    __ Ret();

    // Call the function proxy.
    __ Bind(&call_proxy);
    // x0 : argc
    // x1 : function
    __ Push(function);  // Add function proxy as last argument.
    __ Add(x0, x0, 1);
    __ Mov(x2, 0);
    __ GetBuiltinFunction(x1, Builtins::CALL_FUNCTION_PROXY);
    __ Call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
  }
  __ Drop(3);
  __ Ret();
}


static void ArgumentAdaptorStackCheck(MacroAssembler* masm,
                                      Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  // -----------------------------------
  // Check the stack for overflow.
  // We are not trying to catch interruptions (e.g. debug break and
  // preemption) here, so the "real stack limit" is checked.
  Label enough_stack_space;
  __ LoadRoot(x10, Heap::kRealStackLimitRootIndex);
  // Make x10 the space we have left. The stack might already be overflowed
  // here which will cause x10 to become negative.
  __ Sub(x10, jssp, x10);
  // Check if the arguments will overflow the stack.
  __ Cmp(x10, Operand(x2, LSL, kPointerSizeLog2));
  __ B(le, stack_overflow);
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(x10, x0);
  __ Mov(x11, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ Push(lr, fp);
  __ Push(x11, x1, x10);
  __ Add(fp, jssp,
         StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize);
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then drop the parameters and the receiver.
  __ Ldr(x10, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                               kPointerSize)));
  __ Mov(jssp, fp);
  __ Pop(fp, lr);
  __ DropBySMI(x10, kXRegSize);
  __ Drop(1);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_ArgumentsAdaptorTrampoline");
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  // -----------------------------------

  Label stack_overflow;
  ArgumentAdaptorStackCheck(masm, &stack_overflow);

  Register argc_actual = x0;  // Excluding the receiver.
  Register argc_expected = x2;  // Excluding the receiver.
  Register function = x1;
  Register code_entry = x3;

  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ Ldr(code_entry, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
  __ Cmp(argc_actual, argc_expected);
  __ B(lt, &too_few);
  __ Cmp(argc_expected, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ B(eq, &dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    EnterArgumentsAdaptorFrame(masm);

    Register copy_start = x10;
    Register copy_end = x11;
    Register copy_to = x12;
    Register scratch1 = x13, scratch2 = x14;

    __ Lsl(argc_expected, argc_expected, kPointerSizeLog2);

    // Adjust for fp, lr, and the receiver.
    __ Add(copy_start, fp, 3 * kPointerSize);
    __ Add(copy_start, copy_start, Operand(argc_actual, LSL, kPointerSizeLog2));
    __ Sub(copy_end, copy_start, argc_expected);
    __ Sub(copy_end, copy_end, kPointerSize);
    __ Mov(copy_to, jssp);

    // Claim space for the arguments, the receiver, and one extra slot.
    // The extra slot ensures we do not write under jssp. It will be popped
    // later.
    __ Add(scratch1, argc_expected, 2 * kPointerSize);
    __ Claim(scratch1, 1);

    // Copy the arguments (including the receiver) to the new stack frame.
    Label copy_2_by_2;
    __ Bind(&copy_2_by_2);
    __ Ldp(scratch1, scratch2,
           MemOperand(copy_start, - 2 * kPointerSize, PreIndex));
    __ Stp(scratch1, scratch2,
           MemOperand(copy_to, - 2 * kPointerSize, PreIndex));
    __ Cmp(copy_start, copy_end);
    __ B(hi, &copy_2_by_2);

    // Correct the space allocated for the extra slot.
    __ Drop(1);

    __ B(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ Bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    Register copy_from = x10;
    Register copy_end = x11;
    Register copy_to = x12;
    Register scratch1 = x13, scratch2 = x14;

    __ Lsl(argc_expected, argc_expected, kPointerSizeLog2);
    __ Lsl(argc_actual, argc_actual, kPointerSizeLog2);

    // Adjust for fp, lr, and the receiver.
    __ Add(copy_from, fp, 3 * kPointerSize);
    __ Add(copy_from, copy_from, argc_actual);
    __ Mov(copy_to, jssp);
    __ Sub(copy_end, copy_to, 1 * kPointerSize);   // Adjust for the receiver.
    __ Sub(copy_end, copy_end, argc_actual);

    // Claim space for the arguments, the receiver, and one extra slot.
    // The extra slot ensures we do not write under jssp. It will be popped
    // later.
    __ Add(scratch1, argc_expected, 2 * kPointerSize);
    __ Claim(scratch1, 1);

    // Copy the arguments (including the receiver) to the new stack frame.
    Label copy_2_by_2;
    __ Bind(&copy_2_by_2);
    __ Ldp(scratch1, scratch2,
           MemOperand(copy_from, - 2 * kPointerSize, PreIndex));
    __ Stp(scratch1, scratch2,
           MemOperand(copy_to, - 2 * kPointerSize, PreIndex));
    __ Cmp(copy_to, copy_end);
    __ B(hi, &copy_2_by_2);

    __ Mov(copy_to, copy_end);

    // Fill the remaining expected arguments with undefined.
    __ LoadRoot(scratch1, Heap::kUndefinedValueRootIndex);
    __ Add(copy_end, jssp, kPointerSize);

    Label fill;
    __ Bind(&fill);
    __ Stp(scratch1, scratch1,
           MemOperand(copy_to, - 2 * kPointerSize, PreIndex));
    __ Cmp(copy_to, copy_end);
    __ B(hi, &fill);

    // Correct the space allocated for the extra slot.
    __ Drop(1);
  }

  // Arguments have been adapted. Now call the entry point.
  __ Bind(&invoke);
  __ Call(code_entry);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // Call the entry point without adapting the arguments.
  __ Bind(&dont_adapt_arguments);
  __ Jump(code_entry);

  __ Bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    EnterArgumentsAdaptorFrame(masm);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    __ Unreachable();
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM

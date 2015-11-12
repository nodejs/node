// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/frames-arm64.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
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
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument (argc == x0)
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(x1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  // TODO(bmeurer): Can we make this more robust?
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

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
  __ Mov(x3, x1);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_StringConstructor");

  // 1. Load the first argument into x0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ Cbz(x0, &no_arguments);
    __ Sub(x0, x0, 1);
    __ Drop(x0);
    __ Ldr(x0, MemOperand(jssp, 2 * kPointerSize, PostIndex));
  }

  // 2a. At least one argument, return x0 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(x0, &to_string);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CompareObjectType(x0, x1, x1, FIRST_NONSTRING_TYPE);
    __ B(hi, &to_string);
    __ B(eq, &symbol_descriptive_string);
    __ Ret();
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ Bind(&no_arguments);
  {
    __ LoadRoot(x0, Heap::kempty_stringRootIndex);
    __ Drop(1);
    __ Ret();
  }

  // 3a. Convert x0 to a string.
  __ Bind(&to_string);
  {
    ToStringStub stub(masm->isolate());
    __ TailCallStub(&stub);
  }

  // 3b. Convert symbol in x0 to a string.
  __ Bind(&symbol_descriptive_string);
  {
    __ Push(x0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString, 1, 1);
  }
}


// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_StringConstructor_ConstructStub");

  // 1. Load the first argument into x2 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ Cbz(x0, &no_arguments);
    __ Sub(x0, x0, 1);
    __ Drop(x0);
    __ Ldr(x2, MemOperand(jssp, 2 * kPointerSize, PostIndex));
    __ B(&done);
    __ Bind(&no_arguments);
    __ Drop(1);
    __ LoadRoot(x2, Heap::kempty_stringRootIndex);
    __ Bind(&done);
  }

  // 2. Make sure x2 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(x2, &convert);
    __ JumpIfObjectType(x2, x3, x3, FIRST_NONSTRING_TYPE, &done_convert, lo);
    __ Bind(&convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(x1);
      __ Move(x0, x2);
      __ CallStub(&stub);
      __ Move(x2, x0);
      __ Pop(x1);
    }
    __ Bind(&done_convert);
  }

  // 3. Allocate a JSValue wrapper for the string.
  {
    // ----------- S t a t e -------------
    //  -- x1 : constructor function
    //  -- x2 : the first argument
    //  -- lr : return address
    // -----------------------------------

    Label allocate, done_allocate;
    __ Allocate(JSValue::kSize, x0, x3, x4, &allocate, TAG_OBJECT);
    __ Bind(&done_allocate);

    // Initialize the JSValue in eax.
    __ LoadGlobalFunctionInitialMap(x1, x3, x4);
    __ Str(x3, FieldMemOperand(x0, HeapObject::kMapOffset));
    __ LoadRoot(x3, Heap::kEmptyFixedArrayRootIndex);
    __ Str(x3, FieldMemOperand(x0, JSObject::kPropertiesOffset));
    __ Str(x3, FieldMemOperand(x0, JSObject::kElementsOffset));
    __ Str(x2, FieldMemOperand(x0, JSValue::kValueOffset));
    STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
    __ Ret();

    // Fallback to the runtime to allocate in new space.
    __ Bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(x1, x2);
      __ Push(Smi::FromInt(JSValue::kSize));
      __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
      __ Pop(x2, x1);
    }
    __ B(&done_allocate);
  }
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
                                           bool is_api_function) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x2     : allocation site or undefined
  //  -- x3    : original constructor
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubHelper");

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the four incoming parameters on the stack.
    Register argc = x0;
    Register constructor = x1;
    Register allocation_site = x2;
    Register original_constructor = x3;

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(allocation_site, x10);
    __ SmiTag(argc);
    __ Push(allocation_site, argc, constructor, original_constructor);
    // sp[0]: new.target
    // sp[1]: Constructor function.
    // sp[2]: number of arguments (smi-tagged)
    // sp[3]: allocation site

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ Mov(x2, Operand(debug_step_in_fp));
      __ Ldr(x2, MemOperand(x2));
      __ Cbnz(x2, &rt_call);

      // Fall back to runtime if the original constructor and function differ.
      __ Cmp(constructor, original_constructor);
      __ B(ne, &rt_call);

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
        __ DecodeField<Map::Counter>(constructon_count, x4);
        __ Cmp(constructon_count, Operand(Map::kSlackTrackingCounterEnd));
        __ B(lt, &allocate);
        // Decrease generous allocation count.
        __ Subs(x4, x4, Operand(1 << Map::Counter::kShift));
        __ Str(x4, bit_field3);
        __ Cmp(constructon_count, Operand(Map::kSlackTrackingCounterEnd));
        __ B(ne, &allocate);

        // Push the constructor and map to the stack, and the constructor again
        // as argument to the runtime call.
        __ Push(constructor, init_map, constructor);
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);
        __ Pop(init_map, constructor);
        __ Mov(constructon_count, Operand(Map::kSlackTrackingCounterEnd - 1));
        __ Bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      Label rt_call_reload_new_target;
      Register obj_size = x3;
      Register new_obj = x4;
      __ Ldrb(obj_size, FieldMemOperand(init_map, Map::kInstanceSizeOffset));
      __ Allocate(obj_size, new_obj, x10, x11, &rt_call_reload_new_target,
                  SIZE_IN_WORDS);

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
      Register unused_props = x10;
      Register inobject_props = x11;
      Register inst_sizes_or_attrs = x11;
      Register prealloc_fields = x10;
      __ Ldr(inst_sizes_or_attrs,
             FieldMemOperand(init_map, Map::kInstanceAttributesOffset));
      __ Ubfx(unused_props, inst_sizes_or_attrs,
              Map::kUnusedPropertyFieldsByte * kBitsPerByte, kBitsPerByte);
      __ Ldr(inst_sizes_or_attrs,
             FieldMemOperand(init_map, Map::kInstanceSizesOffset));
      __ Ubfx(
          inobject_props, inst_sizes_or_attrs,
          Map::kInObjectPropertiesOrConstructorFunctionIndexByte * kBitsPerByte,
          kBitsPerByte);
      __ Sub(prealloc_fields, inobject_props, unused_props);

      // Calculate number of property fields in the object.
      Register prop_fields = x6;
      __ Sub(prop_fields, obj_size, JSObject::kHeaderSize / kPointerSize);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ Cmp(constructon_count, Operand(Map::kSlackTrackingCounterEnd));
        __ B(lt, &no_inobject_slack_tracking);
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

      // Fill all of the property fields with undef.
      __ FillFields(first_prop, prop_fields, filler);
      first_prop = NoReg;
      prop_fields = NoReg;

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on.
      __ Add(new_obj, new_obj, kHeapObjectTag);

      // Continue with JSObject being successfully allocated.
      __ B(&allocated);

      // Reload the original constructor and fall-through.
      __ Bind(&rt_call_reload_new_target);
      __ Peek(x3, 0 * kXRegSize);
    }

    // Allocate the new receiver object using the runtime call.
    // x1: constructor function
    // x3: original constructor
    __ Bind(&rt_call);
    __ Push(constructor, original_constructor);  // arguments 1-2
    __ CallRuntime(Runtime::kNewObject, 2);
    __ Mov(x4, x0);

    // Receiver for constructor call allocated.
    // x4: JSObject
    __ Bind(&allocated);

    // Restore the parameters.
    __ Pop(original_constructor);
    __ Pop(constructor);

    // Reload the number of arguments from the stack.
    // Set it up in x0 for the function call below.
    // jssp[0]: number of arguments (smi-tagged)
    __ Peek(argc, 0);  // Load number of arguments.
    __ SmiUntag(argc);

    __ Push(original_constructor, x4, x4);

    // Set up pointer to last argument.
    __ Add(x2, fp, StandardFrameConstants::kCallerSPOffset);

    // Copy arguments and receiver to the expression stack.
    // Copy 2 values every loop to use ldp/stp.
    // x0: number of arguments
    // x1: constructor function
    // x2: address of last argument (caller sp)
    // jssp[0]: receiver
    // jssp[1]: receiver
    // jssp[2]: new.target
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
    // jssp[1]: new.target
    // jssp[2]: number of arguments (smi-tagged)
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    // x0: result
    // jssp[0]: receiver (newly allocated object)
    // jssp[1]: number of arguments (smi-tagged)
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
    // jssp[1]: new.target (original constructor)
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
  Generate_JSConstructStubHelper(masm, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true);
}


void Builtins::Generate_JSConstructStubForDerived(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x2     : allocation site or undefined
  //  -- x3    : original constructor
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_JSConstructStubForDerived");

  {
    FrameScope frame_scope(masm, StackFrame::CONSTRUCT);

    __ AssertUndefinedOrAllocationSite(x2, x10);
    __ Mov(x4, x0);
    __ SmiTag(x4);
    __ LoadRoot(x10, Heap::kTheHoleValueRootIndex);
    __ Push(x2, x4, x3, x10);
    // sp[0]: receiver (the hole)
    // sp[1]: new.target
    // sp[2]: number of arguments
    // sp[3]: allocation site

    // Set up pointer to last argument.
    __ Add(x2, fp, StandardFrameConstants::kCallerSPOffset);

    // Copy arguments and receiver to the expression stack.
    // Copy 2 values every loop to use ldp/stp.
    // x0: number of arguments
    // x1: constructor function
    // x2: address of last argument (caller sp)
    // jssp[0]: receiver
    // jssp[1]: new.target
    // jssp[2]: number of arguments (smi-tagged)
    // Compute the start address of the copy in x4.
    __ Add(x4, x2, Operand(x0, LSL, kPointerSizeLog2));
    Label loop, entry, done_copying_arguments;
    __ B(&entry);
    __ Bind(&loop);
    __ Ldp(x10, x11, MemOperand(x4, -2 * kPointerSize, PreIndex));
    __ Push(x11, x10);
    __ Bind(&entry);
    __ Cmp(x4, x2);
    __ B(gt, &loop);
    // Because we copied values 2 by 2 we may have copied one extra value.
    // Drop it if that is the case.
    __ B(eq, &done_copying_arguments);
    __ Drop(1);
    __ Bind(&done_copying_arguments);

    // Handle step in.
    Label skip_step_in;
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address(masm->isolate());
    __ Mov(x2, Operand(debug_step_in_fp));
    __ Ldr(x2, MemOperand(x2));
    __ Cbz(x2, &skip_step_in);

    __ Push(x0, x1, x1);
    __ CallRuntime(Runtime::kHandleStepInForDerivedConstructors, 1);
    __ Pop(x1, x0);

    __ bind(&skip_step_in);

    // Call the function.
    // x0: number of arguments
    // x1: constructor function
    ParameterCount actual(x0);
    __ InvokeFunction(x1, actual, CALL_FUNCTION, NullCallWrapper());


    // Restore the context from the frame.
    // x0: result
    // jssp[0]: number of arguments (smi-tagged)
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // Load number of arguments (smi), skipping over new.target.
    __ Peek(x1, kPointerSize);

    // Leave construct frame
  }

  __ DropBySMI(x1);
  __ Drop(1);
  __ Ret();
}


enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };


// Clobbers x10, x15; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow.
  // We are not trying to catch interruptions (e.g. debug break and
  // preemption) here, so the "real stack limit" is checked.
  Label enough_stack_space;
  __ LoadRoot(x10, Heap::kRealStackLimitRootIndex);
  // Make x10 the space we have left. The stack might already be overflowed
  // here which will cause x10 to become negative.
  // TODO(jbramley): Check that the stack usage here is safe.
  __ Sub(x10, jssp, x10);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ Cmp(x10, Operand::UntagSmiAndScale(argc, kPointerSizeLog2));
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ Cmp(x10, Operand(argc, LSL, kPointerSizeLog2));
  }
  __ B(gt, &enough_stack_space);
  __ CallRuntime(Runtime::kThrowStackOverflow, 0);
  // We should never return from the APPLY_OVERFLOW builtin.
  if (__ emit_debug_code()) {
    __ Unreachable();
  }

  __ Bind(&enough_stack_space);
}


// Input:
//   x0: new.target.
//   x1: function.
//   x2: receiver.
//   x3: argc.
//   x4: argv.
// Output:
//   x0: result.
static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from JSEntryStub::GenerateBody().
  Register new_target = x0;
  Register function = x1;
  Register receiver = x2;
  Register argc = x3;
  Register argv = x4;
  Register scratch = x10;

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Clear the context before we push it when entering the internal frame.
  __ Mov(cp, 0);

  {
    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    __ Mov(scratch, Operand(ExternalReference(Isolate::kContextAddress,
                                              masm->isolate())));
    __ Ldr(cp, MemOperand(scratch));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(function, receiver);

    // Check if we have enough stack space to push all arguments.
    // Expects argument count in eax. Clobbers ecx, edx, edi.
    Generate_CheckStackOverflow(masm, argc, kArgcIsUntaggedInt);

    // Copy arguments to the stack in a loop, in reverse order.
    // x3: argc.
    // x4: argv.
    Label loop, entry;
    // Compute the copy end address.
    __ Add(scratch, argv, Operand(argc, LSL, kPointerSizeLog2));

    __ B(&entry);
    __ Bind(&loop);
    __ Ldr(x11, MemOperand(argv, kPointerSize, PostIndex));
    __ Ldr(x12, MemOperand(x11));  // Dereference the handle.
    __ Push(x12);  // Push the argument.
    __ Bind(&entry);
    __ Cmp(scratch, argv);
    __ B(ne, &loop);

    __ Mov(scratch, argc);
    __ Mov(argc, new_target);
    __ Mov(new_target, scratch);
    // x0: argc.
    // x3: new.target.

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

    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

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


// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   - x1: the JS function object being called.
//   - cp: our context.
//   - fp: our caller's frame pointer.
//   - jssp: stack pointer.
//   - lr: return address.
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-arm64.h for its layout.
// TODO(rmcilroy): We will need to include the current bytecode pointer in the
// frame.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ Push(lr, fp, cp, x1);
  __ Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);

  // Get the bytecode array from the function object and load the pointer to the
  // first entry into kInterpreterBytecodeRegister.
  __ Ldr(x0, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(x0, SharedFunctionInfo::kFunctionDataOffset));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister,
                    kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, x0, x0,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ Ldr(w11, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    DCHECK(jssp.Is(__ StackPointer()));
    __ Sub(x10, jssp, Operand(x11));
    __ CompareRoot(x10, Heap::kRealStackLimitRootIndex);
    __ B(hs, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ Bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // Note: there should always be at least one stack slot for the return
    // register in the register file.
    Label loop_header;
    __ LoadRoot(x10, Heap::kUndefinedValueRootIndex);
    // TODO(rmcilroy): Ensure we always have an even number of registers to
    // allow stack to be 16 bit aligned (and remove need for jssp).
    __ Lsr(x11, x11, kPointerSizeLog2);
    __ PushMultipleTimes(x10, x11);
    __ Bind(&loop_header);
  }

  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's prologue:
  //  - Support profiler (specifically profiling_counter).
  //  - Call ProfileEntryHookStub when isolate has a function_entry_hook.
  //  - Allow simulator stop operations if FLAG_stop_at is set.
  //  - Deal with sloppy mode functions which need to replace the
  //    receiver with the global proxy when called as functions (without an
  //    explicit receiver object).
  //  - Code aging of the BytecodeArray object.
  //  - Supporting FLAG_trace.
  //
  // The following items are also not done here, and will probably be done using
  // explicit bytecodes instead:
  //  - Allocating a new local context if applicable.
  //  - Setting up a local binding to the this function, which is used in
  //    derived constructors with super calls.
  //  - Setting new.target if required.
  //  - Dealing with REST parameters (only if
  //    https://codereview.chromium.org/1235153006 doesn't land by then).
  //  - Dealing with argument objects.

  // Perform stack guard check.
  {
    Label ok;
    __ CompareRoot(jssp, Heap::kStackLimitRootIndex);
    __ B(hs, &ok);
    __ CallRuntime(Runtime::kStackGuard, 0);
    __ Bind(&ok);
  }

  // Load accumulator, register file, bytecode offset, dispatch table into
  // registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ Sub(kInterpreterRegisterFileRegister, fp,
         Operand(kPointerSize + StandardFrameConstants::kFixedFrameSizeFromFp));
  __ Mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterDispatchTableRegister,
              Heap::kInterpreterTableRootIndex);
  __ Add(kInterpreterDispatchTableRegister, kInterpreterDispatchTableRegister,
         Operand(FixedArray::kHeaderSize - kHeapObjectTag));

  // Dispatch to the first bytecode handler for the function.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x1, LSL, kPointerSizeLog2));
  __ Ldr(ip0, MemOperand(kInterpreterDispatchTableRegister, x1));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ Add(ip0, ip0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(ip0);
}


void Builtins::Generate_InterpreterExitTrampoline(MacroAssembler* masm) {
  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's EmitReturnSequence.
  //  - Supporting FLAG_trace for Runtime::TraceExit.
  //  - Support profiler (specifically decrementing profiling_counter
  //    appropriately and calling out to HandleInterrupts if necessary).

  // The return value is in accumulator, which is already in x0.

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments and return.
  __ Ldr(w1, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                             BytecodeArray::kParameterSizeOffset));
  __ Drop(x1, 1);
  __ Ret();
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


void Builtins::Generate_MarkCodeAsToBeExecutedOnce(MacroAssembler* masm) {
  Generate_MarkCodeAsExecutedOnce(masm);
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
  Register argc = x0;
  Register function = x1;
  Register scratch1 = x10;
  Register scratch2 = x11;

  ASM_LOCATION("Builtins::Generate_FunctionCall");
  // 1. Make sure we have at least one argument.
  {
    Label done;
    __ Cbnz(argc, &done);
    __ LoadRoot(scratch1, Heap::kUndefinedValueRootIndex);
    __ Push(scratch1);
    __ Mov(argc, 1);
    __ Bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  __ Peek(function, Operand(argc, LSL, kXRegSizeLog2));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  {
    Label loop;
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

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


static void Generate_PushAppliedArguments(MacroAssembler* masm,
                                          const int vectorOffset,
                                          const int argumentsOffset,
                                          const int indexOffset,
                                          const int limitOffset) {
  Label entry, loop;
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();
  Register slot = LoadDescriptor::SlotRegister();
  Register vector = LoadWithVectorDescriptor::VectorRegister();

  __ Ldr(key, MemOperand(fp, indexOffset));
  __ B(&entry);

  // Load the current argument from the arguments array.
  __ Bind(&loop);
  __ Ldr(receiver, MemOperand(fp, argumentsOffset));

  // Use inline caching to speed up access to arguments.
  int slot_index = TypeFeedbackVector::PushAppliedArgumentsIndex();
  __ Mov(slot, Operand(Smi::FromInt(slot_index)));
  __ Ldr(vector, MemOperand(fp, vectorOffset));
  Handle<Code> ic =
      KeyedLoadICStub(masm->isolate(), LoadICState(kNoExtraICState)).GetCode();
  __ Call(ic, RelocInfo::CODE_TARGET);

  // Push the nth argument.
  __ Push(x0);

  __ Ldr(key, MemOperand(fp, indexOffset));
  __ Add(key, key, Smi::FromInt(1));
  __ Str(key, MemOperand(fp, indexOffset));

  // Test if the copy loop has finished copying all the elements from the
  // arguments object.
  __ Bind(&entry);
  __ Ldr(x1, MemOperand(fp, limitOffset));
  __ Cmp(key, x1);
  __ B(ne, &loop);

  // On exit, the pushed arguments count is in x0, untagged
  __ Mov(x0, key);
  __ SmiUntag(x0);
}


static void Generate_ApplyHelper(MacroAssembler* masm, bool targetIsArgument) {
  const int kFormalParameters = targetIsArgument ? 3 : 2;
  const int kStackSize = kFormalParameters + 1;

  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);

    const int kArgumentsOffset =  kFPOnStackSize + kPCOnStackSize;
    const int kReceiverOffset = kArgumentsOffset + kPointerSize;
    const int kFunctionOffset = kReceiverOffset + kPointerSize;
    const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);

    Register args = x12;
    Register receiver = x14;
    Register function = x15;
    Register apply_function = x1;

    // Push the vector.
    __ Ldr(
        apply_function,
        FieldMemOperand(apply_function, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(apply_function,
           FieldMemOperand(apply_function,
                           SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(apply_function);

    // Get the length of the arguments via a builtin call.
    __ Ldr(function, MemOperand(fp, kFunctionOffset));
    __ Ldr(args, MemOperand(fp, kArgumentsOffset));
    __ Push(function, args);
    if (targetIsArgument) {
      __ InvokeBuiltin(Context::REFLECT_APPLY_PREPARE_BUILTIN_INDEX,
                       CALL_FUNCTION);
    } else {
      __ InvokeBuiltin(Context::APPLY_PREPARE_BUILTIN_INDEX, CALL_FUNCTION);
    }
    Register argc = x0;

    Generate_CheckStackOverflow(masm, argc, kArgcIsSmiTagged);

    // Push current limit, index and receiver.
    __ Mov(x1, 0);  // Initial index.
    __ Ldr(receiver, MemOperand(fp, kReceiverOffset));
    __ Push(argc, x1, receiver);

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // At the end of the loop, the number of arguments is stored in x0, untagged

    // Call the callable.
    // TODO(bmeurer): This should be a tail call according to ES6.
    __ Ldr(x1, MemOperand(fp, kFunctionOffset));
    __ Call(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
  __ Drop(kStackSize);
  __ Ret();
}


static void Generate_ConstructHelper(MacroAssembler* masm) {
  const int kFormalParameters = 3;
  const int kStackSize = kFormalParameters + 1;

  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);

    const int kNewTargetOffset = kFPOnStackSize + kPCOnStackSize;
    const int kArgumentsOffset =  kNewTargetOffset + kPointerSize;
    const int kFunctionOffset = kArgumentsOffset + kPointerSize;
    const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);

    // Is x11 safe to use?
    Register newTarget = x11;
    Register args = x12;
    Register function = x15;
    Register construct_function = x1;

    // Push the vector.
    __ Ldr(construct_function,
           FieldMemOperand(construct_function,
                           JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(construct_function,
           FieldMemOperand(construct_function,
                           SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(construct_function);

    // If newTarget is not supplied, set it to constructor
    Label validate_arguments;
    __ Ldr(x0, MemOperand(fp, kNewTargetOffset));
    __ CompareRoot(x0, Heap::kUndefinedValueRootIndex);
    __ B(ne, &validate_arguments);
    __ Ldr(x0, MemOperand(fp, kFunctionOffset));
    __ Str(x0, MemOperand(fp, kNewTargetOffset));

    // Validate arguments
    __ Bind(&validate_arguments);
    __ Ldr(function, MemOperand(fp, kFunctionOffset));
    __ Ldr(args, MemOperand(fp, kArgumentsOffset));
    __ Ldr(newTarget, MemOperand(fp, kNewTargetOffset));
    __ Push(function, args, newTarget);
    __ InvokeBuiltin(Context::REFLECT_CONSTRUCT_PREPARE_BUILTIN_INDEX,
                     CALL_FUNCTION);
    Register argc = x0;

    Generate_CheckStackOverflow(masm, argc, kArgcIsSmiTagged);

    // Push current limit and index & constructor function as callee.
    __ Mov(x1, 0);  // Initial index.
    __ Push(argc, x1, function);

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Use undefined feedback vector
    __ LoadRoot(x2, Heap::kUndefinedValueRootIndex);
    __ Ldr(x1, MemOperand(fp, kFunctionOffset));
    __ Ldr(x4, MemOperand(fp, kNewTargetOffset));

    // Call the function.
    CallConstructStub stub(masm->isolate(), SUPER_CONSTRUCTOR_CALL);
    __ Call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);

    // Leave internal frame.
  }
  __ Drop(kStackSize);
  __ Ret();
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_FunctionApply");
  Generate_ApplyHelper(masm, false);
}


void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_ReflectApply");
  Generate_ApplyHelper(masm, true);
}


void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_ReflectConstruct");
  Generate_ConstructHelper(masm);
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


// static
void Builtins::Generate_CallFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  // -----------------------------------

  Label convert, convert_global_proxy, convert_to_object, done_convert;
  __ AssertFunction(x1);
  // TODO(bmeurer): Throw a TypeError if function's [[FunctionKind]] internal
  // slot is "classConstructor".
  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));
  __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  __ Ldr(w3, FieldMemOperand(x2, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestAndBranchIfAnySet(w3,
                           (1 << SharedFunctionInfo::kNative) |
                               (1 << SharedFunctionInfo::kStrictModeFunction),
                           &done_convert);
  {
    __ Peek(x3, Operand(x0, LSL, kXRegSizeLog2));

    // ----------- S t a t e -------------
    //  -- x0 : the number of arguments (not including the receiver)
    //  -- x1 : the function to call (checked to be a JSFunction)
    //  -- x2 : the shared function info.
    //  -- x3 : the receiver
    //  -- cp : the function context.
    // -----------------------------------

    Label convert_receiver;
    __ JumpIfSmi(x3, &convert_to_object);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(x3, x4, x4, FIRST_JS_RECEIVER_TYPE);
    __ B(hs, &done_convert);
    __ JumpIfRoot(x3, Heap::kUndefinedValueRootIndex, &convert_global_proxy);
    __ JumpIfNotRoot(x3, Heap::kNullValueRootIndex, &convert_to_object);
    __ Bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(x3);
    }
    __ B(&convert_receiver);
    __ Bind(&convert_to_object);
    {
      // Convert receiver using ToObject.
      // TODO(bmeurer): Inline the allocation here to avoid building the frame
      // in the fast case? (fall back to AllocateInNewSpace?)
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(x0);
      __ Push(x0, x1);
      __ Mov(x0, x3);
      ToObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ Mov(x3, x0);
      __ Pop(x1, x0);
      __ SmiUntag(x0);
    }
    __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
    __ Bind(&convert_receiver);
    __ Poke(x3, Operand(x0, LSL, kXRegSizeLog2));
  }
  __ Bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  //  -- x2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ Ldrsw(
      x2, FieldMemOperand(x2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ Ldr(x3, FieldMemOperand(x1, JSFunction::kCodeEntryOffset));
  ParameterCount actual(x0);
  ParameterCount expected(x2);
  __ InvokeCode(x3, expected, actual, JUMP_FUNCTION, NullCallWrapper());
}


// static
void Builtins::Generate_Call(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(x1, &non_callable);
  __ Bind(&non_smi);
  __ CompareObjectType(x1, x4, x5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET,
          eq);
  __ Cmp(x5, JS_FUNCTION_PROXY_TYPE);
  __ B(ne, &non_function);

  // 1. Call to function proxy.
  // TODO(neis): This doesn't match the ES6 spec for [[Call]] on proxies.
  __ Ldr(x1, FieldMemOperand(x1, JSFunctionProxy::kCallTrapOffset));
  __ AssertNotSmi(x1);
  __ B(&non_smi);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ Bind(&non_function);
  // Check if target has a [[Call]] internal method.
  __ Ldrb(x4, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x4, 1 << Map::kIsCallable, &non_callable);
  // Overwrite the original receiver with the (original) target.
  __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, x1);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (checked to be a JSFunction)
  //  -- x3 : the original constructor (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(x1);
  __ AssertFunction(x3);

  // Calling convention for function specific ConstructStubs require
  // x2 to contain either an AllocationSite or undefined.
  __ LoadRoot(x2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ Ldr(x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x4, FieldMemOperand(x4, SharedFunctionInfo::kConstructStubOffset));
  __ Add(x4, x4, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x4);
}


// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (checked to be a JSFunctionProxy)
  //  -- x3 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // TODO(neis): This doesn't match the ES6 spec for [[Construct]] on proxies.
  __ Ldr(x1, FieldMemOperand(x1, JSFunctionProxy::kConstructTrapOffset));
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (can be any Object)
  //  -- x3 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target has a [[Construct]] internal method.
  Label non_constructor;
  __ JumpIfSmi(x1, &non_constructor);
  __ Ldr(x4, FieldMemOperand(x1, HeapObject::kMapOffset));
  __ Ldrb(x2, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x2, 1 << Map::kIsConstructor, &non_constructor);

  // Dispatch based on instance type.
  __ CompareInstanceType(x4, x5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq);
  __ Cmp(x5, JS_FUNCTION_PROXY_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadGlobalFunction(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, x1);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_PushArgsAndCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- x1 : the target to call (can be any Object).

  // Find the address of the last argument.
  __ add(x3, x0, Operand(1));  // Add one for receiver.
  __ lsl(x3, x3, kPointerSizeLog2);
  __ sub(x4, x2, x3);

  // Push the arguments.
  Label loop_header, loop_check;
  __ Mov(x5, jssp);
  __ Claim(x3, 1);
  __ B(&loop_check);
  __ Bind(&loop_header);
  // TODO(rmcilroy): Push two at a time once we ensure we keep stack aligned.
  __ Ldr(x3, MemOperand(x2, -kPointerSize, PostIndex));
  __ Str(x3, MemOperand(x5, -kPointerSize, PreIndex));
  __ Bind(&loop_check);
  __ Cmp(x2, x4);
  __ B(gt, &loop_header);

  // Call the target.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
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

    __ Lsl(scratch2, argc_expected, kPointerSizeLog2);

    // Adjust for fp, lr, and the receiver.
    __ Add(copy_start, fp, 3 * kPointerSize);
    __ Add(copy_start, copy_start, Operand(argc_actual, LSL, kPointerSizeLog2));
    __ Sub(copy_end, copy_start, scratch2);
    __ Sub(copy_end, copy_end, kPointerSize);
    __ Mov(copy_to, jssp);

    // Claim space for the arguments, the receiver, and one extra slot.
    // The extra slot ensures we do not write under jssp. It will be popped
    // later.
    __ Add(scratch1, scratch2, 2 * kPointerSize);
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

    Register copy_from = x10;
    Register copy_end = x11;
    Register copy_to = x12;
    Register scratch1 = x13, scratch2 = x14;

    // If the function is strong we need to throw an error.
    Label no_strong_error;
    __ Ldr(scratch1,
           FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(scratch2.W(),
           FieldMemOperand(scratch1, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestAndBranchIfAllClear(scratch2.W(),
                               (1 << SharedFunctionInfo::kStrongModeFunction),
                               &no_strong_error);

    // What we really care about is the required number of arguments.
    DCHECK_EQ(kPointerSize, kInt64Size);
    __ Ldr(scratch2.W(),
           FieldMemOperand(scratch1, SharedFunctionInfo::kLengthOffset));
    __ Cmp(argc_actual, Operand(scratch2, LSR, 1));
    __ B(ge, &no_strong_error);

    {
      FrameScope frame(masm, StackFrame::MANUAL);
      EnterArgumentsAdaptorFrame(masm);
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments, 0);
    }

    __ Bind(&no_strong_error);
    EnterArgumentsAdaptorFrame(masm);

    __ Lsl(scratch2, argc_expected, kPointerSizeLog2);
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
    __ Add(scratch1, scratch2, 2 * kPointerSize);
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
  __ Mov(argc_actual, argc_expected);
  // x0 : expected number of arguments
  // x1 : function (passed through to callee)
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
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ Unreachable();
  }
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

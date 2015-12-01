// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
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
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument (argc == r0)
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  // TODO(bmeurer): Can we make this more robust?
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

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

  __ mov(r3, r1);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ sub(r0, r0, Operand(1), SetCC);
    __ b(lo, &no_arguments);
    __ ldr(r0, MemOperand(sp, r0, LSL, kPointerSizeLog2, PreIndex));
    __ Drop(2);
  }

  // 2a. At least one argument, return r0 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(r0, &to_string);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CompareObjectType(r0, r1, r1, FIRST_NONSTRING_TYPE);
    __ b(hi, &to_string);
    __ b(eq, &symbol_descriptive_string);
    __ Ret();
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(r0, Heap::kempty_stringRootIndex);
    __ Ret(1);
  }

  // 3a. Convert r0 to a string.
  __ bind(&to_string);
  {
    ToStringStub stub(masm->isolate());
    __ TailCallStub(&stub);
  }

  // 3b. Convert symbol in r0 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Push(r0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString, 1, 1);
  }
}


// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0                     : number of arguments
  //  -- r1                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r0 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ sub(r0, r0, Operand(1), SetCC);
    __ b(lo, &no_arguments);
    __ ldr(r0, MemOperand(sp, r0, LSL, kPointerSizeLog2, PreIndex));
    __ Drop(2);
    __ b(&done);
    __ bind(&no_arguments);
    __ LoadRoot(r0, Heap::kempty_stringRootIndex);
    __ Drop(1);
    __ bind(&done);
  }

  // 2. Make sure r0 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(r0, &convert);
    __ CompareObjectType(r0, r2, r2, FIRST_NONSTRING_TYPE);
    __ b(lo, &done_convert);
    __ bind(&convert);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(r1);
      __ CallStub(&stub);
      __ Pop(r1);
    }
    __ bind(&done_convert);
  }

  // 3. Allocate a JSValue wrapper for the string.
  {
    // ----------- S t a t e -------------
    //  -- r0 : the first argument
    //  -- r1 : constructor function
    //  -- lr : return address
    // -----------------------------------

    Label allocate, done_allocate;
    __ Move(r2, r0);
    __ Allocate(JSValue::kSize, r0, r3, r4, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Initialize the JSValue in r0.
    __ LoadGlobalFunctionInitialMap(r1, r3, r4);
    __ str(r3, FieldMemOperand(r0, HeapObject::kMapOffset));
    __ LoadRoot(r3, Heap::kEmptyFixedArrayRootIndex);
    __ str(r3, FieldMemOperand(r0, JSObject::kPropertiesOffset));
    __ str(r3, FieldMemOperand(r0, JSObject::kElementsOffset));
    __ str(r2, FieldMemOperand(r0, JSValue::kValueOffset));
    STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
    __ Ret();

    // Fallback to the runtime to allocate in new space.
    __ bind(&allocate);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Move(r3, Smi::FromInt(JSValue::kSize));
      __ Push(r1, r2, r3);
      __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
      __ Pop(r1, r2);
    }
    __ b(&done_allocate);
  }
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
                                           bool is_api_function) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- r2     : allocation site or undefined
  //  -- r3     : original constructor
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(r2, r4);
    __ push(r2);
    __ SmiTag(r0);
    __ push(r0);
    __ push(r1);
    __ push(r3);

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ mov(r2, Operand(debug_step_in_fp));
      __ ldr(r2, MemOperand(r2));
      __ tst(r2, r2);
      __ b(ne, &rt_call);

      // Fall back to runtime if the original constructor and function differ.
      __ cmp(r1, r3);
      __ b(ne, &rt_call);

      // Load the initial map and verify that it is in fact a map.
      // r1: constructor function
      __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(r2, &rt_call);
      __ CompareObjectType(r2, r5, r4, MAP_TYPE);
      __ b(ne, &rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // r1: constructor function
      // r2: initial map
      __ CompareInstanceType(r2, r5, JS_FUNCTION_TYPE);
      __ b(eq, &rt_call);

      if (!is_api_function) {
        Label allocate;
        MemOperand bit_field3 = FieldMemOperand(r2, Map::kBitField3Offset);
        // Check if slack tracking is enabled.
        __ ldr(r4, bit_field3);
        __ DecodeField<Map::Counter>(r3, r4);
        __ cmp(r3, Operand(Map::kSlackTrackingCounterEnd));
        __ b(lt, &allocate);
        // Decrease generous allocation count.
        __ sub(r4, r4, Operand(1 << Map::Counter::kShift));
        __ str(r4, bit_field3);
        __ cmp(r3, Operand(Map::kSlackTrackingCounterEnd));
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
      Label rt_call_reload_new_target;
      __ ldrb(r3, FieldMemOperand(r2, Map::kInstanceSizeOffset));

      __ Allocate(r3, r4, r5, r6, &rt_call_reload_new_target, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // r1: constructor function
      // r2: initial map
      // r3: object size
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
      // r3: object size
      // r4: JSObject (not tagged)
      // r5: First in-object property of JSObject (not tagged)
      DCHECK_EQ(3 * kPointerSize, JSObject::kHeaderSize);
      __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ ldr(ip, FieldMemOperand(r2, Map::kBitField3Offset));
        __ DecodeField<Map::Counter>(ip);
        __ cmp(ip, Operand(Map::kSlackTrackingCounterEnd));
        __ b(lt, &no_inobject_slack_tracking);

        // Allocate object with a slack.
        __ ldr(r0, FieldMemOperand(r2, Map::kInstanceSizesOffset));
        __ Ubfx(r0, r0, Map::kInObjectPropertiesOrConstructorFunctionIndexByte *
                            kBitsPerByte,
                kBitsPerByte);
        __ ldr(r2, FieldMemOperand(r2, Map::kInstanceAttributesOffset));
        __ Ubfx(r2, r2, Map::kUnusedPropertyFieldsByte * kBitsPerByte,
                kBitsPerByte);
        __ sub(r0, r0, Operand(r2));
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

      __ add(r0, r4, Operand(r3, LSL, kPointerSizeLog2));  // End of object.
      __ InitializeFieldsWithFiller(r5, r0, r6);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on.
      __ add(r4, r4, Operand(kHeapObjectTag));

      // Continue with JSObject being successfully allocated
      // r4: JSObject
      __ jmp(&allocated);

      // Reload the original constructor and fall-through.
      __ bind(&rt_call_reload_new_target);
      __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
    }

    // Allocate the new receiver object using the runtime call.
    // r1: constructor function
    // r3: original constructor
    __ bind(&rt_call);

    __ push(r1);  // argument 2/1: constructor function
    __ push(r3);  // argument 3/2: original constructor
    __ CallRuntime(Runtime::kNewObject, 2);
    __ mov(r4, r0);

    // Receiver for constructor call allocated.
    // r4: JSObject
    __ bind(&allocated);

    // Restore the parameters.
    __ pop(r3);
    __ pop(r1);

    // Retrieve smi-tagged arguments count from the stack.
    __ ldr(r0, MemOperand(sp));
    __ SmiUntag(r0);

    // Push new.target onto the construct frame. This is stored just below the
    // receiver on the stack.
    __ push(r3);
    __ push(r4);
    __ push(r4);

    // Set up pointer to last argument.
    __ add(r2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r0: number of arguments
    // r1: constructor function
    // r2: address of last argument (caller sp)
    // r3: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: new.target
    // sp[3]: number of arguments (smi-tagged)
    Label loop, entry;
    __ SmiTag(r3, r0);
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
    // sp[1]: new.target
    // sp[2]: number of arguments (smi-tagged)
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    // r0: result
    // sp[0]: receiver
    // sp[1]: new.target
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
    // sp[1]: new.target (original constructor)
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
  Generate_JSConstructStubHelper(masm, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true);
}


void Builtins::Generate_JSConstructStubForDerived(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0     : number of arguments
  //  -- r1     : constructor function
  //  -- r2     : allocation site or undefined
  //  -- r3     : original constructor
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  {
    FrameScope frame_scope(masm, StackFrame::CONSTRUCT);

    __ AssertUndefinedOrAllocationSite(r2, r4);
    __ push(r2);

    __ mov(r4, r0);
    __ SmiTag(r4);
    __ push(r4);  // Smi-tagged arguments count.

    // Push new.target.
    __ push(r3);

    // receiver is the hole.
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ push(ip);

    // Set up pointer to last argument.
    __ add(r2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r0: number of arguments
    // r1: constructor function
    // r2: address of last argument (caller sp)
    // r4: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: new.target
    // sp[2]: number of arguments (smi-tagged)
    Label loop, entry;
    __ b(&entry);
    __ bind(&loop);
    __ ldr(ip, MemOperand(r2, r4, LSL, kPointerSizeLog2 - 1));
    __ push(ip);
    __ bind(&entry);
    __ sub(r4, r4, Operand(2), SetCC);
    __ b(ge, &loop);

    // Handle step in.
    Label skip_step_in;
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address(masm->isolate());
    __ mov(r2, Operand(debug_step_in_fp));
    __ ldr(r2, MemOperand(r2));
    __ tst(r2, r2);
    __ b(eq, &skip_step_in);

    __ Push(r0);
    __ Push(r1);
    __ Push(r1);
    __ CallRuntime(Runtime::kHandleStepInForDerivedConstructors, 1);
    __ Pop(r1);
    __ Pop(r0);

    __ bind(&skip_step_in);

    // Call the function.
    // r0: number of arguments
    // r1: constructor function
    ParameterCount actual(r0);
    __ InvokeFunction(r1, actual, CALL_FUNCTION, NullCallWrapper());

    // Restore context from the frame.
    // r0: result
    // sp[0]: number of arguments (smi-tagged)
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Get arguments count, skipping over new.target.
    __ ldr(r1, MemOperand(sp, kPointerSize));

    // Leave construct frame.
  }

  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2 - 1));
  __ add(sp, sp, Operand(kPointerSize));
  __ Jump(lr);
}


enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };


// Clobbers r2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
  // Make r2 the space we have left. The stack might already be overflowed
  // here which will cause r2 to become negative.
  __ sub(r2, sp, r2);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ cmp(r2, Operand::PointerOffsetFromSmiKey(argc));
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ cmp(r2, Operand(argc, LSL, kPointerSizeLog2));
  }
  __ b(gt, &okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow, 0);

  __ bind(&okay);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r0: new.target
  // r1: function
  // r2: receiver
  // r3: argc
  // r4: argv
  // r5-r6, r8 (if !FLAG_enable_embedded_constant_pool) and cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Clear the context before we push it when entering the internal frame.
  __ mov(cp, Operand::Zero());

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ ldr(cp, MemOperand(cp));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(r1, r2);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r2.
    Generate_CheckStackOverflow(masm, r3, kArgcIsUntaggedInt);

    // Remember new.target.
    __ mov(r5, r0);

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

    // Setup new.target and argc.
    __ mov(r0, Operand(r3));
    __ mov(r3, Operand(r5));

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ mov(r5, Operand(r4));
    __ mov(r6, Operand(r4));
    if (!FLAG_enable_embedded_constant_pool) {
      __ mov(r8, Operand(r4));
    }
    if (kR9Available == 1) {
      __ mov(r9, Operand(r4));
    }

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

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


// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r1: the JS function object being called.
//   o cp: our context
//   o pp: the caller's constant pool pointer (if enabled)
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-arm.h for its layout.
// TODO(rmcilroy): We will need to include the current bytecode pointer in the
// frame.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushFixedFrame(r1);
  __ add(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Get the bytecode array from the function object and load the pointer to the
  // first entry into kInterpreterBytecodeRegister.
  __ ldr(r0, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(r0, SharedFunctionInfo::kFunctionDataOffset));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r0, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ ldr(r4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ sub(r9, sp, Operand(r4));
    __ LoadRoot(r2, Heap::kRealStackLimitRootIndex);
    __ cmp(r9, Operand(r2));
    __ b(hs, &ok);
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
    __ b(&loop_check, al);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(r9);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ sub(r4, r4, Operand(kPointerSize), SetCC);
    __ b(&loop_header, ge);
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
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmp(sp, Operand(ip));
    __ b(hs, &ok);
    __ CallRuntime(Runtime::kStackGuard, 0);
    __ bind(&ok);
  }

  // Load accumulator, register file, bytecode offset, dispatch table into
  // registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ sub(kInterpreterRegisterFileRegister, fp,
         Operand(kPointerSize + StandardFrameConstants::kFixedFrameSizeFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterDispatchTableRegister,
              Heap::kInterpreterTableRootIndex);
  __ add(kInterpreterDispatchTableRegister, kInterpreterDispatchTableRegister,
         Operand(FixedArray::kHeaderSize - kHeapObjectTag));

  // Dispatch to the first bytecode handler for the function.
  __ ldrb(r1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ldr(ip, MemOperand(kInterpreterDispatchTableRegister, r1, LSL,
                        kPointerSizeLog2));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ add(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(ip);
}


void Builtins::Generate_InterpreterExitTrampoline(MacroAssembler* masm) {
  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's EmitReturnSequence.
  //  - Supporting FLAG_trace for Runtime::TraceExit.
  //  - Support profiler (specifically decrementing profiling_counter
  //    appropriately and calling out to HandleInterrupts if necessary).

  // The return value is in accumulator, which is already in r0.

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments and return.
  __ ldr(ip, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                             BytecodeArray::kParameterSizeOffset));
  __ add(sp, sp, ip, LeaveCC);
  __ Jump(lr);
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
  __ LoadRoot(
      ip, concurrent ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ push(ip);

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


void Builtins::Generate_MarkCodeAsToBeExecutedOnce(MacroAssembler* masm) {
  Generate_MarkCodeAsExecutedOnce(masm);
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
    __ add(r0, r0, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start

    if (FLAG_enable_embedded_constant_pool) {
      __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r0);
    }

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ ldr(r1, FieldMemOperand(r1, FixedArray::OffsetOfElementAt(
        DeoptimizationInputData::kOsrPcOffsetIndex)));

    // Compute the target address = code start + osr_offset
    __ add(lr, r0, Operand::SmiUntag(r1));

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


// static
void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r0: actual number of arguments
  {
    Label done;
    __ cmp(r0, Operand::Zero());
    __ b(ne, &done);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ add(r0, r0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  // r0: actual number of arguments
  __ ldr(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r0: actual number of arguments
  // r1: callable
  {
    Label loop;
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

  __ ldr(key, MemOperand(fp, indexOffset));
  __ b(&entry);

  // Load the current argument from the arguments array.
  __ bind(&loop);
  __ ldr(receiver, MemOperand(fp, argumentsOffset));

  // Use inline caching to speed up access to arguments.
  int slot_index = TypeFeedbackVector::PushAppliedArgumentsIndex();
  __ mov(slot, Operand(Smi::FromInt(slot_index)));
  __ ldr(vector, MemOperand(fp, vectorOffset));
  Handle<Code> ic =
      KeyedLoadICStub(masm->isolate(), LoadICState(kNoExtraICState)).GetCode();
  __ Call(ic, RelocInfo::CODE_TARGET);

  // Push the nth argument.
  __ push(r0);

  __ ldr(key, MemOperand(fp, indexOffset));
  __ add(key, key, Operand(1 << kSmiTagSize));
  __ str(key, MemOperand(fp, indexOffset));

  // Test if the copy loop has finished copying all the elements from the
  // arguments object.
  __ bind(&entry);
  __ ldr(r1, MemOperand(fp, limitOffset));
  __ cmp(key, r1);
  __ b(ne, &loop);

  // On exit, the pushed arguments count is in r0, untagged
  __ mov(r0, key);
  __ SmiUntag(r0);
}


// Used by FunctionApply and ReflectApply
static void Generate_ApplyHelper(MacroAssembler* masm, bool targetIsArgument) {
  const int kFormalParameters = targetIsArgument ? 3 : 2;
  const int kStackSize = kFormalParameters + 1;

  {
    FrameAndConstantPoolScope frame_scope(masm, StackFrame::INTERNAL);
    const int kArgumentsOffset = kFPOnStackSize + kPCOnStackSize;
    const int kReceiverOffset = kArgumentsOffset + kPointerSize;
    const int kFunctionOffset = kReceiverOffset + kPointerSize;
    const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ ldr(r1, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r1, FieldMemOperand(r1, SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(r1);

    __ ldr(r0, MemOperand(fp, kFunctionOffset));  // get the function
    __ ldr(r1, MemOperand(fp, kArgumentsOffset));  // get the args array
    __ Push(r0, r1);
    if (targetIsArgument) {
      __ InvokeBuiltin(Context::REFLECT_APPLY_PREPARE_BUILTIN_INDEX,
                       CALL_FUNCTION);
    } else {
      __ InvokeBuiltin(Context::APPLY_PREPARE_BUILTIN_INDEX, CALL_FUNCTION);
    }

    Generate_CheckStackOverflow(masm, r0, kArgcIsSmiTagged);

    // Push current limit and index.
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);
    __ mov(r1, Operand::Zero());
    __ ldr(r2, MemOperand(fp, kReceiverOffset));
    __ Push(r0, r1, r2);  // limit, initial index and receiver.

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Call the callable.
    // TODO(bmeurer): This should be a tail call according to ES6.
    __ ldr(r1, MemOperand(fp, kFunctionOffset));
    __ Call(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);

    // Tear down the internal frame and remove function, receiver and args.
  }
  __ add(sp, sp, Operand(kStackSize * kPointerSize));
  __ Jump(lr);
}


static void Generate_ConstructHelper(MacroAssembler* masm) {
  const int kFormalParameters = 3;
  const int kStackSize = kFormalParameters + 1;

  {
    FrameAndConstantPoolScope frame_scope(masm, StackFrame::INTERNAL);
    const int kNewTargetOffset = kFPOnStackSize + kPCOnStackSize;
    const int kArgumentsOffset = kNewTargetOffset + kPointerSize;
    const int kFunctionOffset = kArgumentsOffset + kPointerSize;
    static const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ ldr(r1, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r1, FieldMemOperand(r1, SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(r1);

    // If newTarget is not supplied, set it to constructor
    Label validate_arguments;
    __ ldr(r0, MemOperand(fp, kNewTargetOffset));
    __ CompareRoot(r0, Heap::kUndefinedValueRootIndex);
    __ b(ne, &validate_arguments);
    __ ldr(r0, MemOperand(fp, kFunctionOffset));
    __ str(r0, MemOperand(fp, kNewTargetOffset));

    // Validate arguments
    __ bind(&validate_arguments);
    __ ldr(r0, MemOperand(fp, kFunctionOffset));  // get the function
    __ push(r0);
    __ ldr(r0, MemOperand(fp, kArgumentsOffset));  // get the args array
    __ push(r0);
    __ ldr(r0, MemOperand(fp, kNewTargetOffset));  // get the new.target
    __ push(r0);
    __ InvokeBuiltin(Context::REFLECT_CONSTRUCT_PREPARE_BUILTIN_INDEX,
                     CALL_FUNCTION);

    Generate_CheckStackOverflow(masm, r0, kArgcIsSmiTagged);

    // Push current limit and index.
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);
    __ push(r0);  // limit
    __ mov(r1, Operand::Zero());  // initial index
    __ push(r1);
    // Push the constructor function as callee.
    __ ldr(r0, MemOperand(fp, kFunctionOffset));
    __ push(r0);

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Use undefined feedback vector
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ ldr(r1, MemOperand(fp, kFunctionOffset));
    __ ldr(r4, MemOperand(fp, kNewTargetOffset));

    // Call the function.
    CallConstructStub stub(masm->isolate(), SUPER_CONSTRUCTOR_CALL);
    __ Call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);

    // Leave internal frame.
  }
  __ add(sp, sp, Operand(kStackSize * kPointerSize));
  __ Jump(lr);
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  Generate_ApplyHelper(masm, false);
}


void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  Generate_ApplyHelper(masm, true);
}


void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  Generate_ConstructHelper(masm);
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
                       (FLAG_enable_embedded_constant_pool ? pp.bit() : 0) |
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


// static
void Builtins::Generate_CallFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSFunction)
  // -----------------------------------

  Label convert, convert_global_proxy, convert_to_object, done_convert;
  __ AssertFunction(r1);
  // TODO(bmeurer): Throw a TypeError if function's [[FunctionKind]] internal
  // slot is "classConstructor".
  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  __ ldrb(r3, FieldMemOperand(r2, SharedFunctionInfo::kNativeByteOffset));
  __ tst(r3, Operand((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                     (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ b(ne, &done_convert);
  {
    __ ldr(r3, MemOperand(sp, r0, LSL, kPointerSizeLog2));

    // ----------- S t a t e -------------
    //  -- r0 : the number of arguments (not including the receiver)
    //  -- r1 : the function to call (checked to be a JSFunction)
    //  -- r2 : the shared function info.
    //  -- r3 : the receiver
    //  -- cp : the function context.
    // -----------------------------------

    Label convert_receiver;
    __ JumpIfSmi(r3, &convert_to_object);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r3, r4, r4, FIRST_JS_RECEIVER_TYPE);
    __ b(hs, &done_convert);
    __ JumpIfRoot(r3, Heap::kUndefinedValueRootIndex, &convert_global_proxy);
    __ JumpIfNotRoot(r3, Heap::kNullValueRootIndex, &convert_to_object);
    __ bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r3);
    }
    __ b(&convert_receiver);
    __ bind(&convert_to_object);
    {
      // Convert receiver using ToObject.
      // TODO(bmeurer): Inline the allocation here to avoid building the frame
      // in the fast case? (fall back to AllocateInNewSpace?)
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(r0);
      __ Push(r0, r1);
      __ mov(r0, r3);
      ToObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ mov(r3, r0);
      __ Pop(r0, r1);
      __ SmiUntag(r0);
    }
    __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ bind(&convert_receiver);
    __ str(r3, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the function to call (checked to be a JSFunction)
  //  -- r2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ ldr(r2,
         FieldMemOperand(r2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ SmiUntag(r2);
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  ParameterCount actual(r0);
  ParameterCount expected(r2);
  __ InvokeCode(r3, expected, actual, JUMP_FUNCTION, NullCallWrapper());
}


// static
void Builtins::Generate_Call(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r1, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r1, r4, r5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET,
          eq);
  __ cmp(r5, Operand(JS_FUNCTION_PROXY_TYPE));
  __ b(ne, &non_function);

  // 1. Call to function proxy.
  // TODO(neis): This doesn't match the ES6 spec for [[Call]] on proxies.
  __ ldr(r1, FieldMemOperand(r1, JSFunctionProxy::kCallTrapOffset));
  __ AssertNotSmi(r1);
  __ b(&non_smi);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Check if target has a [[Call]] internal method.
  __ ldrb(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r4, Operand(1 << Map::kIsCallable));
  __ b(eq, &non_callable);
  // Overwrite the original receiver the (original) target.
  __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r1);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (checked to be a JSFunction)
  //  -- r3 : the original constructor (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r1);
  __ AssertFunction(r3);

  // Calling convention for function specific ConstructStubs require
  // r2 to contain either an AllocationSite or undefined.
  __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kConstructStubOffset));
  __ add(pc, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
}


// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (checked to be a JSFunctionProxy)
  //  -- r3 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // TODO(neis): This doesn't match the ES6 spec for [[Construct]] on proxies.
  __ ldr(r1, FieldMemOperand(r1, JSFunctionProxy::kConstructTrapOffset));
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r1 : the constructor to call (can be any Object)
  //  -- r3 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target has a [[Construct]] internal method.
  Label non_constructor;
  __ JumpIfSmi(r1, &non_constructor);
  __ ldr(r4, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldrb(r2, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(r2, Operand(1 << Map::kIsConstructor));
  __ b(eq, &non_constructor);

  // Dispatch based on instance type.
  __ CompareInstanceType(r4, r5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq);
  __ cmp(r5, Operand(JS_FUNCTION_PROXY_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ str(r1, MemOperand(sp, r0, LSL, kPointerSizeLog2));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadGlobalFunction(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r1);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_PushArgsAndCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0 : the number of arguments (not including the receiver)
  //  -- r2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r1 : the target to call (can be any Object).

  // Find the address of the last argument.
  __ add(r3, r0, Operand(1));  // Add one for receiver.
  __ mov(r3, Operand(r3, LSL, kPointerSizeLog2));
  __ sub(r3, r2, r3);

  // Push the arguments.
  Label loop_header, loop_check;
  __ b(al, &loop_check);
  __ bind(&loop_header);
  __ ldr(r4, MemOperand(r2, -kPointerSize, PostIndex));
  __ push(r4);
  __ bind(&loop_check);
  __ cmp(r2, r3);
  __ b(gt, &loop_header);

  // Call the target.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
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

    // Calculate copy start address into r0 and copy end address into r4.
    // r0: actual number of arguments as a smi
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    __ add(r0, fp, Operand::PointerOffsetFromSmiKey(r0));
    // adjust for return address and receiver
    __ add(r0, r0, Operand(2 * kPointerSize));
    __ sub(r4, r0, Operand(r2, LSL, kPointerSizeLog2));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r0: copy start address
    // r1: function
    // r2: expected number of arguments
    // r3: code entry to call
    // r4: copy end address

    Label copy;
    __ bind(&copy);
    __ ldr(ip, MemOperand(r0, 0));
    __ push(ip);
    __ cmp(r0, r4);  // Compare before moving to next argument.
    __ sub(r0, r0, Operand(kPointerSize));
    __ b(ne, &copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);

    // If the function is strong we need to throw an error.
    Label no_strong_error;
    __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ ldr(r5, FieldMemOperand(r4, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(r5, Operand(1 << (SharedFunctionInfo::kStrongModeFunction +
                             kSmiTagSize)));
    __ b(eq, &no_strong_error);

    // What we really care about is the required number of arguments.
    __ ldr(r4, FieldMemOperand(r4, SharedFunctionInfo::kLengthOffset));
    __ cmp(r0, Operand::SmiUntag(r4));
    __ b(ge, &no_strong_error);

    {
      FrameScope frame(masm, StackFrame::MANUAL);
      EnterArgumentsAdaptorFrame(masm);
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments, 0);
    }

    __ bind(&no_strong_error);
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
    __ sub(r4, fp, Operand(r2, LSL, kPointerSizeLog2));
    // Adjust for frame.
    __ sub(r4, r4, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                           2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(ip);
    __ cmp(sp, r4);
    __ b(ne, &fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(r0, r2);
  // r0 : expected number of arguments
  // r1 : function (passed through to callee)
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
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ bkpt(0);
  }
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

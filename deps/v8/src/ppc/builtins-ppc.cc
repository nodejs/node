// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm, CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  // ----------- S t a t e -------------
  //  -- r3                 : number of arguments excluding receiver
  //  -- r4                 : called function (only guaranteed when
  //                          extra_args requires it)
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument (argc == r0)
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r4);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  // TODO(bmeurer): Can we make this more robust?
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ push(r4);
  } else {
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects r0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ addi(r3, r3, Operand(num_extra_args + 1));
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the native context.

  __ LoadP(result,
           MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(result, FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the InternalArray function from the native context.
  __ LoadP(result,
           MemOperand(result, Context::SlotOffset(
                                  Context::INTERNAL_ARRAY_FUNCTION_INDEX)));
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the native context.

  __ LoadP(result,
           MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(result, FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the Array function from the native context.
  __ LoadP(
      result,
      MemOperand(result, Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r4);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ LoadP(r5, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r5, r0);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction, cr0);
    __ CompareObjectType(r5, r6, r7, MAP_TYPE);
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
  //  -- r3     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r4);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ LoadP(r5, FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r5, r0);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r5, r6, r7, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  __ mr(r6, r4);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                     : number of arguments
  //  -- r4                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r3 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ cmpi(r3, Operand::Zero());
    __ beq(&no_arguments);
    __ subi(r3, r3, Operand(1));
    __ ShiftLeftImm(r3, r3, Operand(kPointerSizeLog2));
    __ LoadPUX(r3, MemOperand(sp, r3));
    __ Drop(2);
  }

  // 2a. At least one argument, return r3 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(r3, &to_string);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CompareObjectType(r3, r4, r4, FIRST_NONSTRING_TYPE);
    __ bgt(&to_string);
    __ beq(&symbol_descriptive_string);
    __ Ret();
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(r3, Heap::kempty_stringRootIndex);
    __ Ret(1);
  }

  // 3a. Convert r3 to a string.
  __ bind(&to_string);
  {
    ToStringStub stub(masm->isolate());
    __ TailCallStub(&stub);
  }

  // 3b. Convert symbol in r3 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Push(r3);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString, 1, 1);
  }
}


// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3                     : number of arguments
  //  -- r4                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r3 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ cmpi(r3, Operand::Zero());
    __ beq(&no_arguments);
    __ subi(r3, r3, Operand(1));
    __ ShiftLeftImm(r3, r3, Operand(kPointerSizeLog2));
    __ LoadPUX(r3, MemOperand(sp, r3));
    __ Drop(2);
    __ b(&done);
    __ bind(&no_arguments);
    __ LoadRoot(r3, Heap::kempty_stringRootIndex);
    __ Drop(1);
    __ bind(&done);
  }

  // 2. Make sure r3 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(r3, &convert);
    __ CompareObjectType(r3, r5, r5, FIRST_NONSTRING_TYPE);
    __ blt(&done_convert);
    __ bind(&convert);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ push(r4);
      __ CallStub(&stub);
      __ pop(r4);
    }
    __ bind(&done_convert);
  }

  // 3. Allocate a JSValue wrapper for the string.
  {
    // ----------- S t a t e -------------
    //  -- r3 : the first argument
    //  -- r4 : constructor function
    //  -- lr : return address
    // -----------------------------------

    Label allocate, done_allocate;
    __ mr(r5, r3);
    __ Allocate(JSValue::kSize, r3, r6, r7, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Initialize the JSValue in r3.
    __ LoadGlobalFunctionInitialMap(r4, r6, r7);
    __ StoreP(r6, FieldMemOperand(r3, HeapObject::kMapOffset), r0);
    __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
    __ StoreP(r6, FieldMemOperand(r3, JSObject::kPropertiesOffset), r0);
    __ StoreP(r6, FieldMemOperand(r3, JSObject::kElementsOffset), r0);
    __ StoreP(r5, FieldMemOperand(r3, JSValue::kValueOffset), r0);
    STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
    __ Ret();

    // Fallback to the runtime to allocate in new space.
    __ bind(&allocate);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ LoadSmiLiteral(r6, Smi::FromInt(JSValue::kSize));
      __ Push(r4, r5, r6);
      __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
      __ Pop(r4, r5);
    }
    __ b(&done_allocate);
  }
}


static void CallRuntimePassFunction(MacroAssembler* masm,
                                    Runtime::FunctionId function_id) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  // Push function as parameter to the runtime call.
  __ Push(r4, r4);

  __ CallRuntime(function_id, 1);
  // Restore reciever.
  __ Pop(r4);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ addi(ip, r3, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmpl(sp, ip);
  __ bge(&ok);

  CallRuntimePassFunction(masm, Runtime::kTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function) {
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- r4     : constructor function
  //  -- r5     : allocation site or undefined
  //  -- r6     : original constructor
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(r5, r7);
    __ SmiTag(r3);
    __ Push(r5, r3, r4, r6);

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ mov(r5, Operand(debug_step_in_fp));
      __ LoadP(r5, MemOperand(r5));
      __ cmpi(r5, Operand::Zero());
      __ bne(&rt_call);

      // Fall back to runtime if the original constructor and function differ.
      __ cmp(r4, r6);
      __ bne(&rt_call);

      // Load the initial map and verify that it is in fact a map.
      // r4: constructor function
      __ LoadP(r5,
               FieldMemOperand(r4, JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(r5, &rt_call);
      __ CompareObjectType(r5, r8, r7, MAP_TYPE);
      __ bne(&rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // r4: constructor function
      // r5: initial map
      __ CompareInstanceType(r5, r8, JS_FUNCTION_TYPE);
      __ beq(&rt_call);

      if (!is_api_function) {
        Label allocate;
        MemOperand bit_field3 = FieldMemOperand(r5, Map::kBitField3Offset);
        // Check if slack tracking is enabled.
        __ lwz(r7, bit_field3);
        __ DecodeField<Map::Counter>(r11, r7);
        __ cmpi(r11, Operand(Map::kSlackTrackingCounterEnd));
        __ blt(&allocate);
        // Decrease generous allocation count.
        __ Add(r7, r7, -(1 << Map::Counter::kShift), r0);
        __ stw(r7, bit_field3);
        __ cmpi(r11, Operand(Map::kSlackTrackingCounterEnd));
        __ bne(&allocate);

        __ push(r4);

        __ Push(r5, r4);  // r4 = constructor
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ Pop(r4, r5);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // r4: constructor function
      // r5: initial map
      Label rt_call_reload_new_target;
      __ lbz(r6, FieldMemOperand(r5, Map::kInstanceSizeOffset));

      __ Allocate(r6, r7, r8, r9, &rt_call_reload_new_target, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // r4: constructor function
      // r5: initial map
      // r6: object size
      // r7: JSObject (not tagged)
      __ LoadRoot(r9, Heap::kEmptyFixedArrayRootIndex);
      __ mr(r8, r7);
      __ StoreP(r5, MemOperand(r8, JSObject::kMapOffset));
      __ StoreP(r9, MemOperand(r8, JSObject::kPropertiesOffset));
      __ StoreP(r9, MemOperand(r8, JSObject::kElementsOffset));
      __ addi(r8, r8, Operand(JSObject::kElementsOffset + kPointerSize));

      __ ShiftLeftImm(r9, r6, Operand(kPointerSizeLog2));
      __ add(r9, r7, r9);  // End of object.

      // Fill all the in-object properties with the appropriate filler.
      // r4: constructor function
      // r5: initial map
      // r6: object size
      // r7: JSObject (not tagged)
      // r8: First in-object property of JSObject (not tagged)
      // r9: End of object
      DCHECK_EQ(3 * kPointerSize, JSObject::kHeaderSize);
      __ LoadRoot(r10, Heap::kUndefinedValueRootIndex);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ cmpi(r11, Operand(Map::kSlackTrackingCounterEnd));
        __ blt(&no_inobject_slack_tracking);

        // Allocate object with a slack.
        __ lbz(
            r3,
            FieldMemOperand(
                r5, Map::kInObjectPropertiesOrConstructorFunctionIndexOffset));
        __ lbz(r5, FieldMemOperand(r5, Map::kUnusedPropertyFieldsOffset));
        __ sub(r3, r3, r5);
        if (FLAG_debug_code) {
          __ ShiftLeftImm(r0, r3, Operand(kPointerSizeLog2));
          __ add(r0, r8, r0);
          // r0: offset of first field after pre-allocated fields
          __ cmp(r0, r9);
          __ Assert(le, kUnexpectedNumberOfPreAllocatedPropertyFields);
        }
        {
          Label done;
          __ cmpi(r3, Operand::Zero());
          __ beq(&done);
          __ InitializeNFieldsWithFiller(r8, r3, r10);
          __ bind(&done);
        }
        // To allow for truncation.
        __ LoadRoot(r10, Heap::kOnePointerFillerMapRootIndex);
        // Fill the remaining fields with one pointer filler map.

        __ bind(&no_inobject_slack_tracking);
      }

      __ InitializeFieldsWithFiller(r8, r9, r10);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on.
      __ addi(r7, r7, Operand(kHeapObjectTag));

      // Continue with JSObject being successfully allocated
      // r7: JSObject
      __ b(&allocated);

      // Reload the original constructor and fall-through.
      __ bind(&rt_call_reload_new_target);
      __ LoadP(r6, MemOperand(sp, 0 * kPointerSize));
    }

    // Allocate the new receiver object using the runtime call.
    // r4: constructor function
    // r6: original constructor
    __ bind(&rt_call);
    __ Push(r4, r6);
    __ CallRuntime(Runtime::kNewObject, 2);
    __ mr(r7, r3);

    // Receiver for constructor call allocated.
    // r7: JSObject
    __ bind(&allocated);

    // Restore the parameters.
    __ Pop(r4, ip);

    // Retrieve smi-tagged arguments count from the stack.
    __ LoadP(r6, MemOperand(sp));

    // Push new.target onto the construct frame. This is stored just below the
    // receiver on the stack.
    __ Push(ip, r7, r7);

    // Set up pointer to last argument.
    __ addi(r5, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r4: constructor function
    // r5: address of last argument (caller sp)
    // r6: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: new.target
    // sp[3]: number of arguments (smi-tagged)
    Label loop, no_args;
    __ SmiUntag(r3, r6, SetRC);
    __ beq(&no_args, cr0);
    __ ShiftLeftImm(ip, r3, Operand(kPointerSizeLog2));
    __ sub(sp, sp, ip);
    __ mtctr(r3);
    __ bind(&loop);
    __ subi(ip, ip, Operand(kPointerSize));
    __ LoadPX(r0, MemOperand(r5, ip));
    __ StorePX(r0, MemOperand(sp, ip));
    __ bdnz(&loop);
    __ bind(&no_args);

    // Call the function.
    // r3: number of arguments
    // r4: constructor function
    if (is_api_function) {
      __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));
      Handle<Code> code = masm->isolate()->builtins()->HandleApiCallConstruct();
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(r3);
      __ InvokeFunction(r4, actual, CALL_FUNCTION, NullCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    // r3: result
    // sp[0]: receiver
    // sp[1]: new.target
    // sp[2]: number of arguments (smi-tagged)
    __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    // r3: result
    // sp[0]: receiver
    // sp[1]: new.target
    // sp[2]: number of arguments (smi-tagged)
    __ JumpIfSmi(r3, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    __ CompareObjectType(r3, r4, r6, FIRST_SPEC_OBJECT_TYPE);
    __ bge(&exit);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ LoadP(r3, MemOperand(sp));

    // Remove receiver from the stack, remove caller arguments, and
    // return.
    __ bind(&exit);
    // r3: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: new.target (original constructor)
    // sp[2]: number of arguments (smi-tagged)
    __ LoadP(r4, MemOperand(sp, 2 * kPointerSize));

    // Leave construct frame.
  }

  __ SmiToPtrArrayOffset(r4, r4);
  __ add(sp, sp, r4);
  __ addi(sp, sp, Operand(kPointerSize));
  __ IncrementCounter(isolate->counters()->constructed_objects(), 1, r4, r5);
  __ blr();
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true);
}


void Builtins::Generate_JSConstructStubForDerived(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3     : number of arguments
  //  -- r4     : constructor function
  //  -- r5     : allocation site or undefined
  //  -- r6     : original constructor
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    __ AssertUndefinedOrAllocationSite(r5, r7);

    // Smi-tagged arguments count.
    __ mr(r7, r3);
    __ SmiTag(r7, SetRC);

    // receiver is the hole.
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);

    // allocation site, smi arguments count, new.target, receiver
    __ Push(r5, r7, r6, ip);

    // Set up pointer to last argument.
    __ addi(r5, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r3: number of arguments
    // r4: constructor function
    // r5: address of last argument (caller sp)
    // r7: number of arguments (smi-tagged)
    // cr0: compare against zero of arguments
    // sp[0]: receiver
    // sp[1]: new.target
    // sp[2]: number of arguments (smi-tagged)
    Label loop, no_args;
    __ beq(&no_args, cr0);
    __ ShiftLeftImm(ip, r3, Operand(kPointerSizeLog2));
    __ mtctr(r3);
    __ bind(&loop);
    __ subi(ip, ip, Operand(kPointerSize));
    __ LoadPX(r0, MemOperand(r5, ip));
    __ push(r0);
    __ bdnz(&loop);
    __ bind(&no_args);

    // Handle step in.
    Label skip_step_in;
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address(masm->isolate());
    __ mov(r5, Operand(debug_step_in_fp));
    __ LoadP(r5, MemOperand(r5));
    __ and_(r0, r5, r5, SetRC);
    __ beq(&skip_step_in, cr0);

    __ Push(r3, r4, r4);
    __ CallRuntime(Runtime::kHandleStepInForDerivedConstructors, 1);
    __ Pop(r3, r4);

    __ bind(&skip_step_in);

    // Call the function.
    // r3: number of arguments
    // r4: constructor function
    ParameterCount actual(r3);
    __ InvokeFunction(r4, actual, CALL_FUNCTION, NullCallWrapper());

    // Restore context from the frame.
    // r3: result
    // sp[0]: number of arguments (smi-tagged)
    __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Get arguments count, skipping over new.target.
    __ LoadP(r4, MemOperand(sp, kPointerSize));

    // Leave construct frame.
  }

  __ SmiToPtrArrayOffset(r4, r4);
  __ add(sp, sp, r4);
  __ addi(sp, sp, Operand(kPointerSize));
  __ blr();
}


enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };


// Clobbers r5; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r5, Heap::kRealStackLimitRootIndex);
  // Make r5 the space we have left. The stack might already be overflowed
  // here which will cause r5 to become negative.
  __ sub(r5, sp, r5);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ SmiToPtrArrayOffset(r0, argc);
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ ShiftLeftImm(r0, argc, Operand(kPointerSizeLog2));
  }
  __ cmp(r5, r0);
  __ bgt(&okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow, 0);

  __ bind(&okay);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r3: new.target
  // r4: function
  // r5: receiver
  // r6: argc
  // r7: argv
  // r0,r8-r9, cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Clear the context before we push it when entering the internal frame.
  __ li(cp, Operand::Zero());

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ LoadP(cp, MemOperand(cp));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(r4, r5);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r5.
    Generate_CheckStackOverflow(masm, r6, kArgcIsUntaggedInt);

    // Copy arguments to the stack in a loop.
    // r4: function
    // r6: argc
    // r7: argv, i.e. points to first arg
    Label loop, entry;
    __ ShiftLeftImm(r0, r6, Operand(kPointerSizeLog2));
    __ add(r5, r7, r0);
    // r5 points past last arg.
    __ b(&entry);
    __ bind(&loop);
    __ LoadP(r8, MemOperand(r7));  // read next parameter
    __ addi(r7, r7, Operand(kPointerSize));
    __ LoadP(r0, MemOperand(r8));  // dereference handle
    __ push(r0);                   // push parameter
    __ bind(&entry);
    __ cmp(r7, r5);
    __ bne(&loop);

    // Setup new.target and argc.
    __ mr(r7, r3);
    __ mr(r3, r6);
    __ mr(r6, r7);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);
    __ mr(r14, r7);
    __ mr(r15, r7);
    __ mr(r16, r7);
    __ mr(r17, r7);

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS frame and remove the parameters (except function), and
    // return.
  }
  __ blr();

  // r3: result
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
//   o r4: the JS function object being called.
//   o cp: our context
//   o pp: the caller's constant pool pointer (if enabled)
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-ppc.h for its layout.
// TODO(rmcilroy): We will need to include the current bytecode pointer in the
// frame.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushFixedFrame(r4);
  __ addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Get the bytecode array from the function object and load the pointer to the
  // first entry into kInterpreterBytecodeRegister.
  __ LoadP(r3, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r3, SharedFunctionInfo::kFunctionDataOffset));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ TestIfSmi(kInterpreterBytecodeArrayRegister, r0);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r3, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ lwz(r5, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ sub(r6, sp, r5);
    __ LoadRoot(r0, Heap::kRealStackLimitRootIndex);
    __ cmpl(r6, r0);
    __ bge(&ok);
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
    __ ShiftRightImm(r5, r5, Operand(kPointerSizeLog2), SetRC);
    __ beq(&no_args, cr0);
    __ mtctr(r5);
    __ bind(&loop);
    __ push(r6);
    __ bdnz(&loop);
    __ bind(&no_args);
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
    __ LoadRoot(r0, Heap::kStackLimitRootIndex);
    __ cmp(sp, r0);
    __ bge(&ok);
    __ CallRuntime(Runtime::kStackGuard, 0);
    __ bind(&ok);
  }

  // Load accumulator, register file, bytecode offset, dispatch table into
  // registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ subi(
      kInterpreterRegisterFileRegister, fp,
      Operand(kPointerSize + StandardFrameConstants::kFixedFrameSizeFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterDispatchTableRegister,
              Heap::kInterpreterTableRootIndex);
  __ addi(kInterpreterDispatchTableRegister, kInterpreterDispatchTableRegister,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag));

  // Dispatch to the first bytecode handler for the function.
  __ lbzx(r4, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftImm(ip, r4, Operand(kPointerSizeLog2));
  __ LoadPX(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ addi(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(ip);
}


void Builtins::Generate_InterpreterExitTrampoline(MacroAssembler* masm) {
  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's EmitReturnSequence.
  //  - Supporting FLAG_trace for Runtime::TraceExit.
  //  - Support profiler (specifically decrementing profiling_counter
  //    appropriately and calling out to HandleInterrupts if necessary).

  // The return value is in accumulator, which is already in r3.

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments and return.
  __ lwz(r0, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                             BytecodeArray::kParameterSizeOffset));
  __ add(sp, sp, r0);
  __ blr();
}


void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kCompileLazy);
  GenerateTailCallToReturnedCode(masm);
}


static void CallCompileOptimized(MacroAssembler* masm, bool concurrent) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  // Push function as parameter to the runtime call.
  __ Push(r4, r4);
  // Whether to compile in a background thread.
  __ LoadRoot(
      r0, concurrent ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ push(r0);

  __ CallRuntime(Runtime::kCompileOptimized, 2);
  // Restore receiver.
  __ pop(r4);
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

  // Point r3 at the start of the PlatformCodeAge sequence.
  __ mr(r3, ip);

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r3 - contains return address (beginning of patch sequence)
  //   r4 - isolate
  //   lr - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ mflr(r0);
  __ MultiPush(r0.bit() | r3.bit() | r4.bit() | fp.bit());
  __ PrepareCallCFunction(2, 0, r5);
  __ mov(r4, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ MultiPop(r0.bit() | r3.bit() | r4.bit() | fp.bit());
  __ mtlr(r0);
  __ mr(ip, r3);
  __ Jump(ip);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                  \
  void Builtins::Generate_Make##C##CodeYoungAgainEvenMarking( \
      MacroAssembler* masm) {                                 \
    GenerateMakeCodeYoungAgainCommon(masm);                   \
  }                                                           \
  void Builtins::Generate_Make##C##CodeYoungAgainOddMarking(  \
      MacroAssembler* masm) {                                 \
    GenerateMakeCodeYoungAgainCommon(masm);                   \
  }
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR


void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Point r3 at the start of the PlatformCodeAge sequence.
  __ mr(r3, ip);

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r3 - contains return address (beginning of patch sequence)
  //   r4 - isolate
  //   lr - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ mflr(r0);
  __ MultiPush(r0.bit() | r3.bit() | r4.bit() | fp.bit());
  __ PrepareCallCFunction(2, 0, r5);
  __ mov(r4, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
      2);
  __ MultiPop(r0.bit() | r3.bit() | r4.bit() | fp.bit());
  __ mtlr(r0);
  __ mr(ip, r3);

  // Perform prologue operations usually performed by the young code stub.
  __ PushFixedFrame(r4);
  __ addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Jump to point after the code-age stub.
  __ addi(r3, ip, Operand(kNoCodeAgeSequenceLength));
  __ Jump(r3);
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
    __ MultiPush(kJSCallerSaved | kCalleeSaved);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, 0, save_doubles);
    __ MultiPop(kJSCallerSaved | kCalleeSaved);
  }

  __ addi(sp, sp, Operand(kPointerSize));  // Ignore state
  __ blr();                                // Jump to miss handler
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
    __ LoadSmiLiteral(r3, Smi::FromInt(static_cast<int>(type)));
    __ push(r3);
    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
  }

  // Get the full codegen state from the stack and untag it -> r9.
  __ LoadP(r9, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(r9);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ cmpi(r9, Operand(FullCodeGenerator::NO_REGISTERS));
  __ bne(&with_tos_register);
  __ addi(sp, sp, Operand(1 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&with_tos_register);
  __ LoadP(r3, MemOperand(sp, 1 * kPointerSize));
  __ cmpi(r9, Operand(FullCodeGenerator::TOS_REG));
  __ bne(&unknown_state);
  __ addi(sp, sp, Operand(2 * kPointerSize));  // Remove state.
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
  __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r3);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the code object is null, just return to the unoptimized code.
  Label skip;
  __ CmpSmiLiteral(r3, Smi::FromInt(0), r0);
  __ bne(&skip);
  __ Ret();

  __ bind(&skip);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadP(r4, FieldMemOperand(r3, Code::kDeoptimizationDataOffset));

  {
    ConstantPoolUnavailableScope constant_pool_unavailable(masm);
    __ addi(r3, r3, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start

    if (FLAG_enable_embedded_constant_pool) {
      __ LoadConstantPoolPointerRegisterFromCodeTargetAddress(r3);
    }

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ LoadP(r4, FieldMemOperand(
                     r4, FixedArray::OffsetOfElementAt(
                             DeoptimizationInputData::kOsrPcOffsetIndex)));
    __ SmiUntag(r4);

    // Compute the target address = code start + osr_offset
    __ add(r0, r3, r4);

    // And "return" to the OSR entry point of the function.
    __ mtlr(r0);
    __ blr();
  }
}


void Builtins::Generate_OsrAfterStackCheck(MacroAssembler* masm) {
  // We check the stack limit as indicator that recompilation might be done.
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmpl(sp, ip);
  __ bge(&ok);
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
  // r3: actual number of arguments
  {
    Label done;
    __ cmpi(r3, Operand::Zero());
    __ bne(&done);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ addi(r3, r3, Operand(1));
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  // r3: actual number of arguments
  __ ShiftLeftImm(r5, r3, Operand(kPointerSizeLog2));
  __ LoadPX(r4, MemOperand(sp, r5));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r3: actual number of arguments
  // r4: callable
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ add(r5, sp, r5);


    __ mtctr(r3);
    __ bind(&loop);
    __ LoadP(ip, MemOperand(r5, -kPointerSize));
    __ StoreP(ip, MemOperand(r5));
    __ subi(r5, r5, Operand(kPointerSize));
    __ bdnz(&loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ subi(r3, r3, Operand(1));
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
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();
  Register slot = LoadDescriptor::SlotRegister();
  Register vector = LoadWithVectorDescriptor::VectorRegister();

  // Copy all arguments from the array to the stack.
  Label entry, loop;
  __ LoadP(key, MemOperand(fp, indexOffset));
  __ b(&entry);
  __ bind(&loop);
  __ LoadP(receiver, MemOperand(fp, argumentsOffset));

  // Use inline caching to speed up access to arguments.
  int slot_index = TypeFeedbackVector::PushAppliedArgumentsIndex();
  __ LoadSmiLiteral(slot, Smi::FromInt(slot_index));
  __ LoadP(vector, MemOperand(fp, vectorOffset));
  Handle<Code> ic =
      KeyedLoadICStub(masm->isolate(), LoadICState(kNoExtraICState)).GetCode();
  __ Call(ic, RelocInfo::CODE_TARGET);

  // Push the nth argument.
  __ push(r3);

  // Update the index on the stack and in register key.
  __ LoadP(key, MemOperand(fp, indexOffset));
  __ AddSmiLiteral(key, key, Smi::FromInt(1), r0);
  __ StoreP(key, MemOperand(fp, indexOffset));

  // Test if the copy loop has finished copying all the elements from the
  // arguments object.
  __ bind(&entry);
  __ LoadP(r0, MemOperand(fp, limitOffset));
  __ cmp(key, r0);
  __ bne(&loop);

  // On exit, the pushed arguments count is in r3, untagged
  __ SmiUntag(r3, key);
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
    __ LoadP(r4, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ LoadP(r4,
             FieldMemOperand(r4, SharedFunctionInfo::kFeedbackVectorOffset));
    __ push(r4);

    __ LoadP(r3, MemOperand(fp, kFunctionOffset));  // get the function
    __ LoadP(r4, MemOperand(fp, kArgumentsOffset));  // get the args array
    __ Push(r3, r4);
    if (targetIsArgument) {
      __ InvokeBuiltin(Context::REFLECT_APPLY_PREPARE_BUILTIN_INDEX,
                       CALL_FUNCTION);
    } else {
      __ InvokeBuiltin(Context::APPLY_PREPARE_BUILTIN_INDEX, CALL_FUNCTION);
    }

    Generate_CheckStackOverflow(masm, r3, kArgcIsSmiTagged);

    // Push current limit and index.
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);
    __ li(r4, Operand::Zero());
    __ LoadP(r5, MemOperand(fp, kReceiverOffset));
    __ Push(r3, r4, r5);  // limit, initial index and receiver.

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Call the callable.
    // TODO(bmeurer): This should be a tail call according to ES6.
    __ LoadP(r4, MemOperand(fp, kFunctionOffset));
    __ Call(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);

    // Tear down the internal frame and remove function, receiver and args.
  }
  __ addi(sp, sp, Operand(kStackSize * kPointerSize));
  __ blr();
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
    __ LoadP(r4, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ LoadP(r4,
             FieldMemOperand(r4, SharedFunctionInfo::kFeedbackVectorOffset));
    __ push(r4);

    // If newTarget is not supplied, set it to constructor
    Label validate_arguments;
    __ LoadP(r3, MemOperand(fp, kNewTargetOffset));
    __ CompareRoot(r3, Heap::kUndefinedValueRootIndex);
    __ bne(&validate_arguments);
    __ LoadP(r3, MemOperand(fp, kFunctionOffset));
    __ StoreP(r3, MemOperand(fp, kNewTargetOffset));

    // Validate arguments
    __ bind(&validate_arguments);
    __ LoadP(r3, MemOperand(fp, kFunctionOffset));  // get the function
    __ push(r3);
    __ LoadP(r3, MemOperand(fp, kArgumentsOffset));  // get the args array
    __ push(r3);
    __ LoadP(r3, MemOperand(fp, kNewTargetOffset));  // get the new.target
    __ push(r3);
    __ InvokeBuiltin(Context::REFLECT_CONSTRUCT_PREPARE_BUILTIN_INDEX,
                     CALL_FUNCTION);

    Generate_CheckStackOverflow(masm, r3, kArgcIsSmiTagged);

    // Push current limit and index.
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);
    __ li(r4, Operand::Zero());
    __ Push(r3, r4);  // limit and initial index.
    // Push the constructor function as callee
    __ LoadP(r3, MemOperand(fp, kFunctionOffset));
    __ push(r3);

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Use undefined feedback vector
    __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
    __ LoadP(r4, MemOperand(fp, kFunctionOffset));
    __ LoadP(r7, MemOperand(fp, kNewTargetOffset));

    // Call the function.
    CallConstructStub stub(masm->isolate(), SUPER_CONSTRUCTOR_CALL);
    __ Call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);

    // Leave internal frame.
  }
  __ addi(sp, sp, Operand(kStackSize * kPointerSize));
  __ blr();
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
  //  -- r3 : actual number of arguments
  //  -- r4 : function (passed through to callee)
  //  -- r5 : expected number of arguments
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(r8, Heap::kRealStackLimitRootIndex);
  // Make r8 the space we have left. The stack might already be overflowed
  // here which will cause r8 to become negative.
  __ sub(r8, sp, r8);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftImm(r0, r5, Operand(kPointerSizeLog2));
  __ cmp(r8, r0);
  __ ble(stack_overflow);  // Signed comparison.
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(r3);
  __ LoadSmiLiteral(r7, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ mflr(r0);
  __ push(r0);
  if (FLAG_enable_embedded_constant_pool) {
    __ Push(fp, kConstantPoolRegister, r7, r4, r3);
  } else {
    __ Push(fp, r7, r4, r3);
  }
  __ addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                          kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ LoadP(r4, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                                kPointerSize)));
  int stack_adjustment = kPointerSize;  // adjust for receiver
  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR, stack_adjustment);
  __ SmiToPtrArrayOffset(r0, r4);
  __ add(sp, sp, r0);
}


// static
void Builtins::Generate_CallFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSFunction)
  // -----------------------------------

  Label convert, convert_global_proxy, convert_to_object, done_convert;
  __ AssertFunction(r4);
  // TODO(bmeurer): Throw a TypeError if function's [[FunctionKind]] internal
  // slot is "classConstructor".
  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));
  __ LoadP(r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  __ lbz(r6, FieldMemOperand(r5, SharedFunctionInfo::kNativeByteOffset));
  __ andi(r0, r6, Operand((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                          (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ bne(&done_convert, cr0);
  {
    __ ShiftLeftImm(r6, r3, Operand(kPointerSizeLog2));
    __ LoadPX(r6, MemOperand(sp, r6));

    // ----------- S t a t e -------------
    //  -- r3 : the number of arguments (not including the receiver)
    //  -- r4 : the function to call (checked to be a JSFunction)
    //  -- r5 : the shared function info.
    //  -- r6 : the receiver
    //  -- cp : the function context.
    // -----------------------------------

    Label convert_receiver;
    __ JumpIfSmi(r6, &convert_to_object);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r6, r7, r7, FIRST_JS_RECEIVER_TYPE);
    __ bge(&done_convert);
    __ JumpIfRoot(r6, Heap::kUndefinedValueRootIndex, &convert_global_proxy);
    __ JumpIfNotRoot(r6, Heap::kNullValueRootIndex, &convert_to_object);
    __ bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r6);
    }
    __ b(&convert_receiver);
    __ bind(&convert_to_object);
    {
      // Convert receiver using ToObject.
      // TODO(bmeurer): Inline the allocation here to avoid building the frame
      // in the fast case? (fall back to AllocateInNewSpace?)
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(r3);
      __ Push(r3, r4);
      __ mr(r3, r6);
      ToObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ mr(r6, r3);
      __ Pop(r3, r4);
      __ SmiUntag(r3);
    }
    __ LoadP(r5, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ bind(&convert_receiver);
    __ ShiftLeftImm(r7, r3, Operand(kPointerSizeLog2));
    __ StorePX(r6, MemOperand(sp, r7));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the function to call (checked to be a JSFunction)
  //  -- r5 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ LoadWordArith(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
#if !V8_TARGET_ARCH_PPC64
  __ SmiUntag(r5);
#endif
  __ LoadP(r6, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));
  ParameterCount actual(r3);
  ParameterCount expected(r5);
  __ InvokeCode(r6, expected, actual, JUMP_FUNCTION, NullCallWrapper());
}


// static
void Builtins::Generate_Call(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r4, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r4, r7, r8, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET,
          eq);
  __ cmpi(r8, Operand(JS_FUNCTION_PROXY_TYPE));
  __ bne(&non_function);

  // 1. Call to function proxy.
  // TODO(neis): This doesn't match the ES6 spec for [[Call]] on proxies.
  __ LoadP(r4, FieldMemOperand(r4, JSFunctionProxy::kCallTrapOffset));
  __ AssertNotSmi(r4);
  __ b(&non_smi);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Check if target has a [[Call]] internal method.
  __ lbz(r7, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r7, Map::kIsCallable, r0);
  __ beq(&non_callable, cr0);
  // Overwrite the original receiver the (original) target.
  __ ShiftLeftImm(r8, r3, Operand(kPointerSizeLog2));
  __ StorePX(r4, MemOperand(sp, r8));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r4);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (checked to be a JSFunction)
  //  -- r6 : the original constructor (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r4);
  __ AssertFunction(r6);

  // Calling convention for function specific ConstructStubs require
  // r5 to contain either an AllocationSite or undefined.
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r7, FieldMemOperand(r7, SharedFunctionInfo::kConstructStubOffset));
  __ addi(ip, r7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}


// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (checked to be a JSFunctionProxy)
  //  -- r6 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // TODO(neis): This doesn't match the ES6 spec for [[Construct]] on proxies.
  __ LoadP(r4, FieldMemOperand(r4, JSFunctionProxy::kConstructTrapOffset));
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r4 : the constructor to call (can be any Object)
  //  -- r6 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target has a [[Construct]] internal method.
  Label non_constructor;
  __ JumpIfSmi(r4, &non_constructor);
  __ LoadP(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
  __ lbz(r5, FieldMemOperand(r7, Map::kBitFieldOffset));
  __ TestBit(r5, Map::kIsConstructor, r0);
  __ beq(&non_constructor, cr0);

  // Dispatch based on instance type.
  __ CompareInstanceType(r7, r8, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq);
  __ cmpi(r8, Operand(JS_FUNCTION_PROXY_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ ShiftLeftImm(r8, r3, Operand(kPointerSizeLog2));
    __ StorePX(r4, MemOperand(sp, r8));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadGlobalFunction(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r4);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r4);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_PushArgsAndCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : the number of arguments (not including the receiver)
  //  -- r5 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r4 : the target to call (can be any Object).

  // Calculate number of arguments (add one for receiver).
  __ addi(r6, r3, Operand(1));

  // Push the arguments.
  Label loop;
  __ addi(r5, r5, Operand(kPointerSize));  // Bias up for LoadPU
  __ mtctr(r6);
  __ bind(&loop);
  __ LoadPU(r6, MemOperand(r5, -kPointerSize));
  __ push(r6);
  __ bdnz(&loop);

  // Call the target.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : actual number of arguments
  //  -- r4 : function (passed through to callee)
  //  -- r5 : expected number of arguments
  // -----------------------------------

  Label stack_overflow;
  ArgumentAdaptorStackCheck(masm, &stack_overflow);
  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));
  __ cmp(r3, r5);
  __ blt(&too_few);
  __ cmpi(r5, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ beq(&dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r3 and copy end address into r6.
    // r3: actual number of arguments as a smi
    // r4: function
    // r5: expected number of arguments
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r3, r3);
    __ add(r3, r3, fp);
    // adjust for return address and receiver
    __ addi(r3, r3, Operand(2 * kPointerSize));
    __ ShiftLeftImm(r6, r5, Operand(kPointerSizeLog2));
    __ sub(r6, r3, r6);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r3: copy start address
    // r4: function
    // r5: expected number of arguments
    // r6: copy end address
    // ip: code entry to call

    Label copy;
    __ bind(&copy);
    __ LoadP(r0, MemOperand(r3, 0));
    __ push(r0);
    __ cmp(r3, r6);  // Compare before moving to next argument.
    __ subi(r3, r3, Operand(kPointerSize));
    __ bne(&copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);

    // If the function is strong we need to throw an error.
    Label no_strong_error;
    __ LoadP(r7, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
    __ lwz(r8, FieldMemOperand(r7, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestBit(r8,
#if V8_TARGET_ARCH_PPC64
               SharedFunctionInfo::kStrongModeFunction,
#else
               SharedFunctionInfo::kStrongModeFunction + kSmiTagSize,
#endif
               r0);
    __ beq(&no_strong_error, cr0);

    // What we really care about is the required number of arguments.
    __ lwz(r7, FieldMemOperand(r7, SharedFunctionInfo::kLengthOffset));
#if V8_TARGET_ARCH_PPC64
    // See commment near kLenghtOffset in src/objects.h
    __ srawi(r7, r7, kSmiTagSize);
#else
    __ SmiUntag(r7);
#endif
    __ cmp(r3, r7);
    __ bge(&no_strong_error);

    {
      FrameScope frame(masm, StackFrame::MANUAL);
      EnterArgumentsAdaptorFrame(masm);
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments, 0);
    }

    __ bind(&no_strong_error);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address is fp.
    // r3: actual number of arguments as a smi
    // r4: function
    // r5: expected number of arguments
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r3, r3);
    __ add(r3, r3, fp);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r3: copy start address
    // r4: function
    // r5: expected number of arguments
    // ip: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ LoadP(r0, MemOperand(r3, 2 * kPointerSize));
    __ push(r0);
    __ cmp(r3, fp);  // Compare before moving to next argument.
    __ subi(r3, r3, Operand(kPointerSize));
    __ bne(&copy);

    // Fill the remaining expected arguments with undefined.
    // r4: function
    // r5: expected number of arguments
    // ip: code entry to call
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    __ ShiftLeftImm(r6, r5, Operand(kPointerSizeLog2));
    __ sub(r6, fp, r6);
    // Adjust for frame.
    __ subi(r6, r6, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(r0);
    __ cmp(sp, r6);
    __ bne(&fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mr(r3, r5);
  // r3 : expected number of arguments
  // r4 : function (passed through to callee)
  __ CallJSEntry(ip);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ blr();


  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ JumpToJSEntry(ip);

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

#endif  // V8_TARGET_ARCH_PPC

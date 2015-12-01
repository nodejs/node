// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS64

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
  //  -- a0                 : number of arguments excluding receiver
  //  -- a1                 : called function (only guaranteed when
  //  --                      extra_args requires it)
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[8 * (argc - 1)] : first argument
  //  -- sp[8 * agrc]       : receiver
  // -----------------------------------
  __ AssertFunction(a1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  // TODO(bmeurer): Can we make this more robust?
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ push(a1);
  } else {
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects a0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ Daddu(a0, a0, num_extra_args + 1);
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
  __ mov(a3, a1);
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));
    __ dsll(a0, a0, kPointerSizeLog2);
    __ Daddu(sp, a0, sp);
    __ ld(a0, MemOperand(sp));
    __ Drop(2);
  }

  // 2a. At least one argument, return a0 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(a0, &to_string);
    __ GetObjectType(a0, a1, a1);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ Subu(a1, a1, Operand(FIRST_NONSTRING_TYPE));
    __ Branch(&symbol_descriptive_string, eq, a1, Operand(zero_reg));
    __ Branch(&to_string, gt, a1, Operand(zero_reg));
    __ Ret(USE_DELAY_SLOT);
    __ mov(v0, a0);
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(v0, Heap::kempty_stringRootIndex);
    __ DropAndRet(1);
  }

  // 3a. Convert a0 to a string.
  __ bind(&to_string);
  {
    ToStringStub stub(masm->isolate());
    __ TailCallStub(&stub);
  }

  // 3b. Convert symbol in a0 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Push(a0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString, 1, 1);
  }
}


void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));
    __ dsll(a0, a0, kPointerSizeLog2);
    __ Daddu(sp, a0, sp);
    __ ld(a0, MemOperand(sp));
    __ Drop(2);
    __ jmp(&done);
    __ bind(&no_arguments);
    __ LoadRoot(a0, Heap::kempty_stringRootIndex);
    __ Drop(1);
    __ bind(&done);
  }

  // 2. Make sure a0 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(a0, &convert);
    __ GetObjectType(a0, a2, a2);
    __ And(t0, a2, Operand(kIsNotStringMask));
    __ Branch(&done_convert, eq, t0, Operand(zero_reg));
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(a1);
      __ CallStub(&stub);
      __ Move(a0, v0);
      __ Pop(a1);
    }
    __ bind(&done_convert);
  }

  // 3. Allocate a JSValue wrapper for the string.
  {
    // ----------- S t a t e -------------
    //  -- a0 : the first argument
    //  -- a1 : constructor function
    //  -- ra : return address
    // -----------------------------------

    Label allocate, done_allocate;
    __ Allocate(JSValue::kSize, v0, a2, a3, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Initialize the JSValue in eax.
    __ LoadGlobalFunctionInitialMap(a1, a2, a3);
    __ sd(a2, FieldMemOperand(v0, HeapObject::kMapOffset));
    __ LoadRoot(a3, Heap::kEmptyFixedArrayRootIndex);
    __ sd(a3, FieldMemOperand(v0, JSObject::kPropertiesOffset));
    __ sd(a3, FieldMemOperand(v0, JSObject::kElementsOffset));
    __ sd(a0, FieldMemOperand(v0, JSValue::kValueOffset));
    STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
    __ Ret();

    // Fallback to the runtime to allocate in new space.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Move(a2, Smi::FromInt(JSValue::kSize));
      __ Push(a0, a1, a2);
      __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
      __ Pop(a0, a1);
    }
    __ jmp(&done_allocate);
  }
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
                                           bool is_api_function) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a2     : allocation site or undefined
  //  -- a3     : original constructor
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(a2, t0);
    __ SmiTag(a0);
    __ Push(a2, a0, a1, a3);

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ li(a2, Operand(debug_step_in_fp));
      __ ld(a2, MemOperand(a2));
      __ Branch(&rt_call, ne, a2, Operand(zero_reg));

      // Fall back to runtime if the original constructor and function differ.
      __ Branch(&rt_call, ne, a1, Operand(a3));

      // Load the initial map and verify that it is in fact a map.
      // a1: constructor function
      __ ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(a2, &rt_call);
      __ GetObjectType(a2, t1, t0);
      __ Branch(&rt_call, ne, t0, Operand(MAP_TYPE));

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // a1: constructor function
      // a2: initial map
      __ lbu(t1, FieldMemOperand(a2, Map::kInstanceTypeOffset));
      __ Branch(&rt_call, eq, t1, Operand(JS_FUNCTION_TYPE));

      if (!is_api_function) {
        Label allocate;
        MemOperand bit_field3 = FieldMemOperand(a2, Map::kBitField3Offset);
        // Check if slack tracking is enabled.
        __ lwu(a4, bit_field3);
        __ DecodeField<Map::Counter>(a6, a4);
        __ Branch(&allocate, lt, a6,
                  Operand(static_cast<int64_t>(Map::kSlackTrackingCounterEnd)));
        // Decrease generous allocation count.
        __ Dsubu(a4, a4, Operand(1 << Map::Counter::kShift));
        __ Branch(USE_DELAY_SLOT, &allocate, ne, a6,
                  Operand(Map::kSlackTrackingCounterEnd));
        __ sw(a4, bit_field3);  // In delay slot.

        __ Push(a1, a2, a1);  // a1 = Constructor.
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ Pop(a1, a2);
        __ li(a6, Operand(Map::kSlackTrackingCounterEnd - 1));

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // a1: constructor function
      // a2: initial map
      Label rt_call_reload_new_target;
      __ lbu(a3, FieldMemOperand(a2, Map::kInstanceSizeOffset));

      __ Allocate(a3, t0, t1, t2, &rt_call_reload_new_target, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // a1: constructor function
      // a2: initial map
      // a3: object size
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
      // a3: object size (in words)
      // t0: JSObject (not tagged)
      // t1: First in-object property of JSObject (not tagged)
      // a6: slack tracking counter (non-API function case)
      DCHECK_EQ(3 * kPointerSize, JSObject::kHeaderSize);

      // Use t3 to hold undefined, which is used in several places below.
      __ LoadRoot(t3, Heap::kUndefinedValueRootIndex);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ Branch(&no_inobject_slack_tracking, lt, a6,
                  Operand(static_cast<int64_t>(Map::kSlackTrackingCounterEnd)));

        // Allocate object with a slack.
        __ lbu(
            a0,
            FieldMemOperand(
                a2, Map::kInObjectPropertiesOrConstructorFunctionIndexOffset));
        __ lbu(a2, FieldMemOperand(a2, Map::kUnusedPropertyFieldsOffset));
        __ dsubu(a0, a0, a2);
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

      __ dsll(at, a3, kPointerSizeLog2);
      __ Daddu(a0, t0, Operand(at));  // End of object.
      __ InitializeFieldsWithFiller(t1, a0, t3);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on.
      __ Daddu(t0, t0, Operand(kHeapObjectTag));

      // Continue with JSObject being successfully allocated.
      // a4: JSObject
      __ jmp(&allocated);

      // Reload the original constructor and fall-through.
      __ bind(&rt_call_reload_new_target);
      __ ld(a3, MemOperand(sp, 0 * kPointerSize));
    }

    // Allocate the new receiver object using the runtime call.
    // a1: constructor function
    // a3: original constructor
    __ bind(&rt_call);

    __ Push(a1, a3);  // arguments 2-3 / 1-2
    __ CallRuntime(Runtime::kNewObject, 2);
    __ mov(t0, v0);

    // Receiver for constructor call allocated.
    // t0: JSObject
    __ bind(&allocated);

    // Restore the parameters.
    __ Pop(a3);  // new.target
    __ Pop(a1);

    __ ld(a0, MemOperand(sp));
    __ SmiUntag(a0);

    __ Push(a3, t0, t0);

    // Set up pointer to last argument.
    __ Daddu(a2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // a0: number of arguments
    // a1: constructor function
    // a2: address of last argument (caller sp)
    // a3: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: new.target
    // sp[3]: number of arguments (smi-tagged)
    Label loop, entry;
    __ mov(a3, a0);
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
    // sp[1]: new.target
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
    // sp[1]: new.target (original constructor)
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
  Generate_JSConstructStubHelper(masm, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true);
}


void Builtins::Generate_JSConstructStubForDerived(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a2     : allocation site or undefined
  //  -- a3     : original constructor
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  {
    FrameScope frame_scope(masm, StackFrame::CONSTRUCT);

    __ AssertUndefinedOrAllocationSite(a2, t0);
    __ push(a2);

    __ mov(a4, a0);
    __ SmiTag(a4);
    __ push(a4);  // Smi-tagged arguments count.

    // Push new.target.
    __ push(a3);

    // receiver is the hole.
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ push(at);

    // Set up pointer to last argument.
    __ Daddu(a2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // a0: number of arguments
    // a1: constructor function
    // a2: address of last argument (caller sp)
    // a4: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: new.target
    // sp[2]: number of arguments (smi-tagged)
    Label loop, entry;
    __ SmiUntag(a4);
    __ jmp(&entry);
    __ bind(&loop);
    __ dsll(at, a4, kPointerSizeLog2);
    __ Daddu(at, a2, Operand(at));
    __ ld(at, MemOperand(at));
    __ push(at);
    __ bind(&entry);
    __ Daddu(a4, a4, Operand(-1));
    __ Branch(&loop, ge, a4, Operand(zero_reg));

    // Handle step in.
    Label skip_step_in;
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address(masm->isolate());
    __ li(a2, Operand(debug_step_in_fp));
    __ ld(a2, MemOperand(a2));
    __ Branch(&skip_step_in, eq, a2, Operand(zero_reg));

    __ Push(a0, a1, a1);
    __ CallRuntime(Runtime::kHandleStepInForDerivedConstructors, 1);
    __ Pop(a0, a1);

    __ bind(&skip_step_in);


    // Call the function.
    // a0: number of arguments
    // a1: constructor function
    ParameterCount actual(a0);
    __ InvokeFunction(a1, actual, CALL_FUNCTION, NullCallWrapper());

    // Restore context from the frame.
    // v0: result
    // sp[0]: new.target
    // sp[1]: number of arguments (smi-tagged)
    __ ld(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    __ ld(a1, MemOperand(sp, kPointerSize));

    // Leave construct frame.
  }

  __ SmiScale(at, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, Operand(at));
  __ Daddu(sp, sp, Operand(kPointerSize));
  __ Jump(ra);
}


enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };


// Clobbers a2; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
  // Make a2 the space we have left. The stack might already be overflowed
  // here which will cause r2 to become negative.
  __ dsubu(a2, sp, a2);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ SmiScale(a7, v0, kPointerSizeLog2);
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ dsll(a7, argc, kPointerSizeLog2);
  }
  __ Branch(&okay, gt, a2, Operand(a7));  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow, 0);

  __ bind(&okay);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from JSEntryStub::GenerateBody

  // ----------- S t a t e -------------
  //  -- a0: new.target
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

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ li(cp, Operand(context_address));
    __ ld(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(a1, a2);

    // Check if we have enough stack space to push all arguments.
    // Clobbers a2.
    Generate_CheckStackOverflow(masm, a3, kArgcIsUntaggedInt);

    // Remember new.target.
    __ mov(a5, a0);

    // Copy arguments to the stack in a loop.
    // a3: argc
    // s0: argv, i.e. points to first arg
    Label loop, entry;
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

    // Setup new.target and argc.
    __ mov(a0, a3);
    __ mov(a3, a5);

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

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

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


// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o a1: the JS function object being called.
//   o cp: our context
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds a JS frame. Please see JavaScriptFrameConstants in
// frames-mips.h for its layout.
// TODO(rmcilroy): We will need to include the current bytecode pointer in the
// frame.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);

  __ Push(ra, fp, cp, a1);
  __ Daddu(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  // Get the bytecode array from the function object and load the pointer to the
  // first entry into kInterpreterBytecodeRegister.
  __ ld(a0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(a0, SharedFunctionInfo::kFunctionDataOffset));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister, a4);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a4,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a4, a4);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a4,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ lw(a4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ Dsubu(a5, sp, Operand(a4));
    __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
    __ Branch(&ok, hs, a5, Operand(a2));
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(a5, Heap::kUndefinedValueRootIndex);
    __ Branch(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(a5);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ Dsubu(a4, a4, Operand(kPointerSize));
    __ Branch(&loop_header, ge, a4, Operand(zero_reg));
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
    __ LoadRoot(at, Heap::kStackLimitRootIndex);
    __ Branch(&ok, hs, sp, Operand(at));
    __ CallRuntime(Runtime::kStackGuard, 0);
    __ bind(&ok);
  }

  // Load bytecode offset and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ Dsubu(
      kInterpreterRegisterFileRegister, fp,
      Operand(kPointerSize + StandardFrameConstants::kFixedFrameSizeFromFp));
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterDispatchTableRegister,
              Heap::kInterpreterTableRootIndex);
  __ Daddu(kInterpreterDispatchTableRegister, kInterpreterDispatchTableRegister,
           Operand(FixedArray::kHeaderSize - kHeapObjectTag));

  // Dispatch to the first bytecode handler for the function.
  __ Daddu(a0, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ lbu(a0, MemOperand(a0));
  __ dsll(at, a0, kPointerSizeLog2);
  __ Daddu(at, kInterpreterDispatchTableRegister, at);
  __ ld(at, MemOperand(at));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ Daddu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(at);
}


void Builtins::Generate_InterpreterExitTrampoline(MacroAssembler* masm) {
  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's EmitReturnSequence.
  //  - Supporting FLAG_trace for Runtime::TraceExit.
  //  - Support profiler (specifically decrementing profiling_counter
  //    appropriately and calling out to HandleInterrupts if necessary).

  // The return value is in accumulator, which is already in v0.

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments and return.
  __ lw(at, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                            BytecodeArray::kParameterSizeOffset));
  __ Daddu(sp, sp, at);
  __ Jump(ra);
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
  __ LoadRoot(
      at, concurrent ? Heap::kTrueValueRootIndex : Heap::kFalseValueRootIndex);
  __ push(at);

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
  __ ld(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

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


// static
void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // a0: actual number of arguments
  {
    Label done;
    __ Branch(&done, ne, a0, Operand(zero_reg));
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ Daddu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack.
  // a0: actual number of arguments
  __ dsll(at, a0, kPointerSizeLog2);
  __ daddu(at, sp, at);
  __ ld(a1, MemOperand(at));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  {
    Label loop;
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

  __ ld(key, MemOperand(fp, indexOffset));
  __ Branch(&entry);

  // Load the current argument from the arguments array.
  __ bind(&loop);
  __ ld(receiver, MemOperand(fp, argumentsOffset));

  // Use inline caching to speed up access to arguments.
  int slot_index = TypeFeedbackVector::PushAppliedArgumentsIndex();
  __ li(slot, Operand(Smi::FromInt(slot_index)));
  __ ld(vector, MemOperand(fp, vectorOffset));
  Handle<Code> ic =
      KeyedLoadICStub(masm->isolate(), LoadICState(kNoExtraICState)).GetCode();
  __ Call(ic, RelocInfo::CODE_TARGET);

  __ push(v0);

  // Use inline caching to access the arguments.
  __ ld(key, MemOperand(fp, indexOffset));
  __ Daddu(key, key, Operand(Smi::FromInt(1)));
  __ sd(key, MemOperand(fp, indexOffset));

  // Test if the copy loop has finished copying all the elements from the
  // arguments object.
  __ bind(&entry);
  __ ld(a1, MemOperand(fp, limitOffset));
  __ Branch(&loop, ne, key, Operand(a1));

  // On exit, the pushed arguments count is in a0, untagged
  __ mov(a0, key);
  __ SmiUntag(a0);
}


// Used by FunctionApply and ReflectApply
static void Generate_ApplyHelper(MacroAssembler* masm, bool targetIsArgument) {
  const int kFormalParameters = targetIsArgument ? 3 : 2;
  const int kStackSize = kFormalParameters + 1;

  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    const int kArgumentsOffset = kFPOnStackSize + kPCOnStackSize;
    const int kReceiverOffset = kArgumentsOffset + kPointerSize;
    const int kFunctionOffset = kReceiverOffset + kPointerSize;
    const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ ld(a1, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ ld(a1, FieldMemOperand(a1, SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(a1);

    __ ld(a0, MemOperand(fp, kFunctionOffset));  // Get the function.
    __ ld(a1, MemOperand(fp, kArgumentsOffset));  // Get the args array.
    __ Push(a0, a1);

    // Returns (in v0) number of arguments to copy to stack as Smi.
    if (targetIsArgument) {
      __ InvokeBuiltin(Context::REFLECT_APPLY_PREPARE_BUILTIN_INDEX,
                       CALL_FUNCTION);
    } else {
      __ InvokeBuiltin(Context::APPLY_PREPARE_BUILTIN_INDEX, CALL_FUNCTION);
    }

    // Returns the result in v0.
    Generate_CheckStackOverflow(masm, v0, kArgcIsSmiTagged);

    // Push current limit and index.
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);
    __ mov(a1, zero_reg);
    __ ld(a2, MemOperand(fp, kReceiverOffset));
    __ Push(v0, a1, a2);  // limit, initial index and receiver.

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Call the callable.
    // TODO(bmeurer): This should be a tail call according to ES6.
    __ ld(a1, MemOperand(fp, kFunctionOffset));
    __ Call(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);

    // Tear down the internal frame and remove function, receiver and args.
  }

  __ Ret(USE_DELAY_SLOT);
  __ Daddu(sp, sp, Operand(kStackSize * kPointerSize));  // In delay slot.
}


static void Generate_ConstructHelper(MacroAssembler* masm) {
  const int kFormalParameters = 3;
  const int kStackSize = kFormalParameters + 1;

  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    const int kNewTargetOffset = kFPOnStackSize + kPCOnStackSize;
    const int kArgumentsOffset = kNewTargetOffset + kPointerSize;
    const int kFunctionOffset = kArgumentsOffset + kPointerSize;
    const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ ld(a1, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ ld(a1, FieldMemOperand(a1, SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(a1);

    // If newTarget is not supplied, set it to constructor
    Label validate_arguments;
    __ ld(a0, MemOperand(fp, kNewTargetOffset));
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(&validate_arguments, ne, a0, Operand(at));
    __ ld(a0, MemOperand(fp, kFunctionOffset));
    __ sd(a0, MemOperand(fp, kNewTargetOffset));

    // Validate arguments
    __ bind(&validate_arguments);
    __ ld(a0, MemOperand(fp, kFunctionOffset));  // get the function
    __ push(a0);
    __ ld(a0, MemOperand(fp, kArgumentsOffset));  // get the args array
    __ push(a0);
    __ ld(a0, MemOperand(fp, kNewTargetOffset));  // get the new.target
    __ push(a0);
    // Returns argument count in v0.
    __ InvokeBuiltin(Context::REFLECT_CONSTRUCT_PREPARE_BUILTIN_INDEX,
                     CALL_FUNCTION);

    // Returns result in v0.
    Generate_CheckStackOverflow(masm, v0, kArgcIsSmiTagged);

    // Push current limit and index.
    const int kIndexOffset = kVectorOffset - (2 * kPointerSize);
    const int kLimitOffset = kVectorOffset - (1 * kPointerSize);
    __ push(v0);  // limit
    __ mov(a1, zero_reg);  // initial index
    __ push(a1);
    // Push the constructor function as callee.
    __ ld(a0, MemOperand(fp, kFunctionOffset));
    __ push(a0);

    // Copy all arguments from the array to the stack.
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Use undefined feedback vector
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ ld(a1, MemOperand(fp, kFunctionOffset));
    __ ld(a4, MemOperand(fp, kNewTargetOffset));

    // Call the function.
    CallConstructStub stub(masm->isolate(), SUPER_CONSTRUCTOR_CALL);
    __ Call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);

    // Leave internal frame.
  }
  __ jr(ra);
  __ Daddu(sp, sp, Operand(kStackSize * kPointerSize));  // In delay slot.
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


// static
void Builtins::Generate_CallFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  // -----------------------------------

  Label convert, convert_global_proxy, convert_to_object, done_convert;
  __ AssertFunction(a1);
  // TODO(bmeurer): Throw a TypeError if function's [[FunctionKind]] internal
  // slot is "classConstructor".
  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  __ lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kNativeByteOffset));
  __ And(at, a3, Operand((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                         (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ Branch(&done_convert, ne, at, Operand(zero_reg));
  {
    __ dsll(at, a0, kPointerSizeLog2);
    __ daddu(at, sp, at);
    __ ld(a3, MemOperand(at));

    // ----------- S t a t e -------------
    //  -- a0 : the number of arguments (not including the receiver)
    //  -- a1 : the function to call (checked to be a JSFunction)
    //  -- a2 : the shared function info.
    //  -- a3 : the receiver
    //  -- cp : the function context.
    // -----------------------------------

    Label convert_receiver;
    __ JumpIfSmi(a3, &convert_to_object);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ GetObjectType(a3, a4, a4);
    __ Branch(&done_convert, hs, a4, Operand(FIRST_JS_RECEIVER_TYPE));
    __ JumpIfRoot(a3, Heap::kUndefinedValueRootIndex, &convert_global_proxy);
    __ JumpIfNotRoot(a3, Heap::kNullValueRootIndex, &convert_to_object);
    __ bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(a3);
    }
    __ Branch(&convert_receiver);
    __ bind(&convert_to_object);
    {
      // Convert receiver using ToObject.
      // TODO(bmeurer): Inline the allocation here to avoid building the frame
      // in the fast case? (fall back to AllocateInNewSpace?)
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(a0);
      __ Push(a0, a1);
      __ mov(a0, a3);
      ToObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ mov(a3, v0);
      __ Pop(a0, a1);
      __ SmiUntag(a0);
    }
    __ ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ bind(&convert_receiver);
    __ dsll(at, a0, kPointerSizeLog2);
    __ daddu(at, sp, at);
    __ sd(a3, MemOperand(at));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  //  -- a2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ lw(a2,
        FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ ld(a3, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  ParameterCount actual(a0);
  ParameterCount expected(a2);
  __ InvokeCode(a3, expected, actual, JUMP_FUNCTION, NullCallWrapper());
}


// static
void Builtins::Generate_Call(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(a1, &non_callable);
  __ bind(&non_smi);
  __ GetObjectType(a1, t1, t2);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET,
          eq, t2, Operand(JS_FUNCTION_TYPE));
  __ Branch(&non_function, ne, t2, Operand(JS_FUNCTION_PROXY_TYPE));

  // 1. Call to function proxy.
  // TODO(neis): This doesn't match the ES6 spec for [[Call]] on proxies.
  __ ld(a1, FieldMemOperand(a1, JSFunctionProxy::kCallTrapOffset));
  __ AssertNotSmi(a1);
  __ Branch(&non_smi);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Check if target has a [[Call]] internal method.
  __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t1, t1, Operand(1 << Map::kIsCallable));
  __ Branch(&non_callable, eq, t1, Operand(zero_reg));
  // Overwrite the original receiver with the (original) target.
  __ dsll(at, a0, kPointerSizeLog2);
  __ daddu(at, sp, at);
  __ sd(a1, MemOperand(at));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, a1);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSFunction)
  //  -- a3 : the original constructor (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(a1);
  __ AssertFunction(a3);

  // Calling convention for function specific ConstructStubs require
  // a2 to contain either an AllocationSite or undefined.
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kConstructStubOffset));
  __ Daddu(at, a4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}


// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSFunctionProxy)
  //  -- a3 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // TODO(neis): This doesn't match the ES6 spec for [[Construct]] on proxies.
  __ ld(a1, FieldMemOperand(a1, JSFunctionProxy::kConstructTrapOffset));
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (can be any Object)
  //  -- a3 : the original constructor (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target has a [[Construct]] internal method.
  Label non_constructor;
  __ JumpIfSmi(a1, &non_constructor);
  __ ld(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(t2, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t2, t2, Operand(1 << Map::kIsCallable));
  __ Branch(&non_constructor, eq, t2, Operand(zero_reg));

  // Dispatch based on instance type.
  __ lbu(t2, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq, t2, Operand(JS_FUNCTION_PROXY_TYPE));

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ dsll(at, a0, kPointerSizeLog2);
    __ daddu(at, sp, at);
    __ sd(a1, MemOperand(at));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadGlobalFunction(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, a1);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_PushArgsAndCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).

  // Find the address of the last argument.
  __ Daddu(a3, a0, Operand(1));  // Add one for receiver.
  __ dsll(a3, a3, kPointerSizeLog2);
  __ Dsubu(a3, a2, Operand(a3));

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ ld(a4, MemOperand(a2));
  __ Daddu(a2, a2, Operand(-kPointerSize));
  __ push(a4);
  __ bind(&loop_check);
  __ Branch(&loop_header, gt, a2, Operand(a3));

  // Call the target.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
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

    // Calculate copy start address into a0 and copy end address into a4.
    __ SmiScale(a0, a0, kPointerSizeLog2);
    __ Daddu(a0, fp, a0);
    // Adjust for return address and receiver.
    __ Daddu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ dsll(a4, a2, kPointerSizeLog2);
    __ dsubu(a4, a0, a4);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: code entry to call
    // a4: copy end address

    Label copy;
    __ bind(&copy);
    __ ld(a5, MemOperand(a0));
    __ push(a5);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a4));
    __ daddiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);

    // If the function is strong we need to throw an error.
    Label no_strong_error;
    __ ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lbu(a5, FieldMemOperand(a4, SharedFunctionInfo::kStrongModeByteOffset));
    __ And(a5, a5, Operand(1 << SharedFunctionInfo::kStrongModeBitWithinByte));
    __ Branch(&no_strong_error, eq, a5, Operand(zero_reg));

    // What we really care about is the required number of arguments.
    DCHECK_EQ(kPointerSize, kInt64Size);
    __ lw(a5, FieldMemOperand(a4, SharedFunctionInfo::kLengthOffset));
    __ srl(a5, a5, 1);
    __ Branch(&no_strong_error, ge, a0, Operand(a5));

    {
      FrameScope frame(masm, StackFrame::MANUAL);
      EnterArgumentsAdaptorFrame(masm);
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments, 0);
    }

    __ bind(&no_strong_error);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into a0 and copy end address into a7.
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
    __ LoadRoot(a5, Heap::kUndefinedValueRootIndex);
    __ dsll(a6, a2, kPointerSizeLog2);
    __ Dsubu(a4, fp, Operand(a6));
    // Adjust for frame.
    __ Dsubu(a4, a4, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                             2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ Dsubu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(a4));
    __ sd(a5, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(a0, a2);
  // a0 : expected number of arguments
  // a1 : function (passed through to callee)
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
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ break_(0xCC);
  }
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64

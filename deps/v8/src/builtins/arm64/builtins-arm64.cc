// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/frames-arm64.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/codegen.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/objects-inl.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the InternalArray function from the native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
}

// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the InternalArray function from the native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  // ----------- S t a t e -------------
  //  -- x0                 : number of arguments excluding receiver
  //  -- x1                 : target
  //  -- x3                 : new target
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(x1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  // JumpToExternalReference expects x0 to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ Add(x0, x0, num_extra_args + 1);

  // Insert extra arguments.
  __ SmiTag(x0);
  __ Push(x0, x1, x3);
  __ SmiUntag(x0);

  __ JumpToExternalReference(ExternalReference(address, masm->isolate()),
                             exit_frame_type == BUILTIN_EXIT);
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
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_NumberConstructor");

  // 1. Load the first argument into x0.
  Label no_arguments;
  {
    __ Cbz(x0, &no_arguments);
    __ Mov(x2, x0);  // Store argc in x2.
    __ Sub(x0, x0, 1);
    __ Ldr(x0, MemOperand(jssp, x0, LSL, kPointerSizeLog2));
  }

  // 2a. Convert first argument to number.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(x2);
    __ EnterBuiltinFrame(cp, x1, x2);
    __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, x1, x2);
    __ SmiUntag(x2);
  }

  {
    // Drop all arguments.
    __ Drop(x2);
  }

  // 2b. No arguments, return +0 (already in x0).
  __ Bind(&no_arguments);
  __ Drop(1);
  __ Ret();
}

// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- x3                     : new target
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_NumberConstructor_ConstructStub");

  // 1. Make sure we operate in the context of the called function.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  // 2. Load the first argument into x2.
  {
    Label no_arguments, done;
    __ Move(x6, x0);  // Store argc in x6.
    __ Cbz(x0, &no_arguments);
    __ Sub(x0, x0, 1);
    __ Ldr(x2, MemOperand(jssp, x0, LSL, kPointerSizeLog2));
    __ B(&done);
    __ Bind(&no_arguments);
    __ Mov(x2, Smi::kZero);
    __ Bind(&done);
  }

  // 3. Make sure x2 is a number.
  {
    Label done_convert;
    __ JumpIfSmi(x2, &done_convert);
    __ JumpIfObjectType(x2, x4, x4, HEAP_NUMBER_TYPE, &done_convert, eq);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(x6);
      __ EnterBuiltinFrame(cp, x1, x6);
      __ Push(x3);
      __ Move(x0, x2);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ Move(x2, x0);
      __ Pop(x3);
      __ LeaveBuiltinFrame(cp, x1, x6);
      __ SmiUntag(x6);
    }
    __ Bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ Cmp(x1, x3);
  __ B(ne, &new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(x0, x1, x2, x4, x5, &new_object);
  __ B(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(x6);
    __ EnterBuiltinFrame(cp, x1, x6);
    __ Push(x2);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(x2);
    __ LeaveBuiltinFrame(cp, x1, x6);
    __ SmiUntag(x6);
  }
  __ Str(x2, FieldMemOperand(x0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(x6);
    __ Drop(1);
    __ Ret();
  }
}

// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_StringConstructor");

  // 1. Load the first argument into x0.
  Label no_arguments;
  {
    __ Cbz(x0, &no_arguments);
    __ Mov(x2, x0);  // Store argc in x2.
    __ Sub(x0, x0, 1);
    __ Ldr(x0, MemOperand(jssp, x0, LSL, kPointerSizeLog2));
  }

  // 2a. At least one argument, return x0 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label drop_frame_and_ret, to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(x0, &to_string);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CompareObjectType(x0, x3, x3, FIRST_NONSTRING_TYPE);
    __ B(hi, &to_string);
    __ B(eq, &symbol_descriptive_string);
    __ b(&drop_frame_and_ret);
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
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(x2);
    __ EnterBuiltinFrame(cp, x1, x2);
    __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, x1, x2);
    __ SmiUntag(x2);
  }
  __ b(&drop_frame_and_ret);

  // 3b. Convert symbol in x0 to a string.
  __ Bind(&symbol_descriptive_string);
  {
    __ Drop(x2);
    __ Drop(1);
    __ Push(x0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(x2);
    __ Drop(1);
    __ Ret();
  }
}

// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- x3                     : new target
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_StringConstructor_ConstructStub");

  // 1. Make sure we operate in the context of the called function.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  // 2. Load the first argument into x2.
  {
    Label no_arguments, done;
    __ mov(x6, x0);  // Store argc in x6.
    __ Cbz(x0, &no_arguments);
    __ Sub(x0, x0, 1);
    __ Ldr(x2, MemOperand(jssp, x0, LSL, kPointerSizeLog2));
    __ B(&done);
    __ Bind(&no_arguments);
    __ LoadRoot(x2, Heap::kempty_stringRootIndex);
    __ Bind(&done);
  }

  // 3. Make sure x2 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(x2, &convert);
    __ JumpIfObjectType(x2, x4, x4, FIRST_NONSTRING_TYPE, &done_convert, lo);
    __ Bind(&convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(x6);
      __ EnterBuiltinFrame(cp, x1, x6);
      __ Push(x3);
      __ Move(x0, x2);
      __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
      __ Move(x2, x0);
      __ Pop(x3);
      __ LeaveBuiltinFrame(cp, x1, x6);
      __ SmiUntag(x6);
    }
    __ Bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ Cmp(x1, x3);
  __ B(ne, &new_object);

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(x0, x1, x2, x4, x5, &new_object);
  __ B(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(x6);
    __ EnterBuiltinFrame(cp, x1, x6);
    __ Push(x2);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(x2);
    __ LeaveBuiltinFrame(cp, x1, x6);
    __ SmiUntag(x6);
  }
  __ Str(x2, FieldMemOperand(x0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(x6);
    __ Drop(1);
    __ Ret();
  }
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x2, FieldMemOperand(x2, SharedFunctionInfo::kCodeOffset));
  __ Add(x2, x2, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x2);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee)
  //  -- x1 : target function (preserved for callee)
  //  -- x3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function and the new target.
    // Push another copy as a parameter to the runtime call.
    __ SmiTag(x0);
    __ Push(x0, x1, x3, x1);

    __ CallRuntime(function_id, 1);
    __ Move(x2, x0);

    // Restore target function and new target.
    __ Pop(x3, x1, x0);
    __ SmiUntag(x0);
  }

  __ Add(x2, x2, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x2);
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

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ Bind(&ok);
  GenerateTailCallToSharedCode(masm);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  Label post_instantiation_deopt_entry;

  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x3     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubHelper");

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(x0);
    __ Push(cp, x0);
    __ SmiUntag(x0);

    __ PushRoot(Heap::kTheHoleValueRootIndex);

    // Set up pointer to last argument.
    __ Add(x2, fp, StandardFrameConstants::kCallerSPOffset);

    // Copy arguments and receiver to the expression stack.
    // Copy 2 values every loop to use ldp/stp.

    // Compute pointer behind the first argument.
    __ Add(x4, x2, Operand(x0, LSL, kPointerSizeLog2));
    Label loop, entry, done_copying_arguments;
    // ----------- S t a t e -------------
    //  --                        x0: number of arguments (untagged)
    //  --                        x1: constructor function
    //  --                        x3: new target
    //  --                        x2: pointer to last argument (caller sp)
    //  --                        x4: pointer to argument last copied
    //  --        sp[0*kPointerSize]: the hole (receiver)
    //  --        sp[1*kPointerSize]: number of arguments (tagged)
    //  --        sp[2*kPointerSize]: context
    // -----------------------------------
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

    // Call the function.
    // x0: number of arguments
    // x1: constructor function
    // x3: new target
    ParameterCount actual(x0);
    __ InvokeFunction(x1, x3, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ Peek(x1, 0);
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ DropBySMI(x1);
  __ Drop(1);
  __ Ret();
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x3     : new target
  //  -- lr     : return address
  //  -- cp     : context pointer
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  ASM_LOCATION("Builtins::Generate_JSConstructStubHelper");

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(x0);
    __ Push(cp, x0, x1, x3);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  -- x1 and sp[1*kPointerSize]: constructor function
    //  --        sp[2*kPointerSize]: number of arguments (tagged)
    //  --        sp[3*kPointerSize]: context
    // -----------------------------------

    __ Ldr(x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
    __ Ldrb(x4,
            FieldMemOperand(x4, SharedFunctionInfo::kFunctionKindByteOffset));
    __ tst(x4, Operand(SharedFunctionInfo::kDerivedConstructorBitsWithinByte));
    __ B(ne, &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        x4, x5);
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ B(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(x0, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                          x0: receiver
    //  -- Slot 3 / sp[0*kPointerSize]: new target
    //  -- Slot 2 / sp[1*kPointerSize]: constructor function
    //  -- Slot 1 / sp[2*kPointerSize]: number of arguments (tagged)
    //  -- Slot 0 / sp[3*kPointerSize]: context
    // -----------------------------------
    // Deoptimizer enters here.
    masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
        masm->pc_offset());
    __ bind(&post_instantiation_deopt_entry);

    // Restore new target.
    __ Pop(x3);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(x0, x0);

    // ----------- S t a t e -------------
    //  --                 x3: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments (tagged)
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ Ldr(x1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ Ldr(x0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(x0);

    // Set up pointer to last argument.
    __ Add(x2, fp, StandardFrameConstants::kCallerSPOffset);

    // Copy arguments and receiver to the expression stack.
    // Copy 2 values every loop to use ldp/stp.

    // Compute pointer behind the first argument.
    __ Add(x4, x2, Operand(x0, LSL, kPointerSizeLog2));
    Label loop, entry, done_copying_arguments;
    // ----------- S t a t e -------------
    //  --                        x0: number of arguments (untagged)
    //  --                        x3: new target
    //  --                        x2: pointer to last argument (caller sp)
    //  --                        x4: pointer to argument last copied
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  -- x1 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------
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

    // Call the function.
    ParameterCount actual(x0);
    __ InvokeFunction(x1, x3, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // ----------- S t a t e -------------
    //  --                 x0: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: constructor function
    //  -- sp[2*kPointerSize]: number of arguments
    //  -- sp[3*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ Ldr(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ CompareRoot(x0, Heap::kUndefinedValueRootIndex);
    __ B(eq, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(x0, &other_result);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ JumpIfObjectType(x0, x4, x5, FIRST_JS_RECEIVER_TYPE, &leave_frame, ge);

    __ Bind(&other_result);
    // The result is now neither undefined nor an object.
    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ Ldr(x4, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
      __ Ldr(x4, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
      __ Ldrb(x4,
              FieldMemOperand(x4, SharedFunctionInfo::kFunctionKindByteOffset));
      __ tst(x4, Operand(SharedFunctionInfo::kClassConstructorBitsWithinByte));
      __ B(eq, &use_receiver);

    } else {
      __ B(&use_receiver);
    }

    __ Bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ Bind(&use_receiver);
    __ Peek(x0, 0 * kPointerSize);
    __ CompareRoot(x0, Heap::kTheHoleValueRootIndex);
    __ B(eq, &do_throw);

    __ Bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ Ldr(x1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ DropBySMI(x1);
  __ Drop(1);
  __ Ret();
}
}  // namespace

void Builtins::Generate_JSConstructStubGenericRestrictedReturn(
    MacroAssembler* masm) {
  Generate_JSConstructStubGeneric(masm, true);
}
void Builtins::Generate_JSConstructStubGenericUnrestrictedReturn(
    MacroAssembler* masm) {
  Generate_JSConstructStubGeneric(masm, false);
}
void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}
void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(x1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the value to pass to the generator
  //  -- x1 : the JSGeneratorObject to resume
  //  -- x2 : the resume mode (tagged)
  //  -- x3 : the SuspendFlags of the earlier suspend call (tagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiUntag(x3);
  __ AssertGeneratorObject(x1, x3);

  // Store input value into generator object.
  Label async_await, done_store_input;

  __ And(x3, x3, Operand(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ Cmp(x3, Operand(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ B(eq, &async_await);

  __ Str(x0, FieldMemOperand(x1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(x1, JSGeneratorObject::kInputOrDebugPosOffset, x0, x3,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ b(&done_store_input);

  __ Bind(&async_await);
  __ Str(x0, FieldMemOperand(
                 x1, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset));
  __ RecordWriteField(x1, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset,
                      x0, x3, kLRHasNotBeenSaved, kDontSaveFPRegs);

  __ Bind(&done_store_input);
  // `x3` no longer holds SuspendFlags

  // Store resume mode into generator object.
  __ Str(x2, FieldMemOperand(x1, JSGeneratorObject::kResumeModeOffset));

  // Load suspended function and context.
  __ Ldr(x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  __ Ldr(cp, FieldMemOperand(x4, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ Mov(x10, Operand(debug_hook));
  __ Ldrsb(x10, MemOperand(x10));
  __ CompareAndBranch(x10, Operand(0), ne, &prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ Mov(x10, Operand(debug_suspended_generator));
  __ Ldr(x10, MemOperand(x10));
  __ CompareAndBranch(x10, Operand(x1), eq,
                      &prepare_step_in_suspended_generator);
  __ Bind(&stepping_prepared);

  // Push receiver.
  __ Ldr(x5, FieldMemOperand(x1, JSGeneratorObject::kReceiverOffset));
  __ Push(x5);

  // ----------- S t a t e -------------
  //  -- x1      : the JSGeneratorObject to resume
  //  -- x2      : the resume mode (tagged)
  //  -- x4      : generator function
  //  -- cp      : generator context
  //  -- lr      : return address
  //  -- jssp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ Ldr(x10, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w10,
         FieldMemOperand(x10, SharedFunctionInfo::kFormalParameterCountOffset));
  __ LoadRoot(x11, Heap::kTheHoleValueRootIndex);
  __ PushMultipleTimes(x11, w10);

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ Ldr(x3, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(x3, FieldMemOperand(x3, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(x3, x3, x3, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ Ldr(x0, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(w0, FieldMemOperand(
                   x0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(x3, x1);
    __ Move(x1, x4);
    __ Ldr(x5, FieldMemOperand(x1, JSFunction::kCodeEntryOffset));
    __ Jump(x5);
  }

  __ Bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1, x2, x4);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(x2, x1);
    __ Ldr(x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  }
  __ B(&stepping_prepared);

  __ Bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1, x2);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(x2, x1);
    __ Ldr(x4, FieldMemOperand(x1, JSGeneratorObject::kFunctionOffset));
  }
  __ B(&stepping_prepared);
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
  __ CallRuntime(Runtime::kThrowStackOverflow);
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
    __ Push(x12);                  // Push the argument.
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

static void ReplaceClosureEntryWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code_entry, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  Register native_context = scratch1;

  // Store code entry in the closure.
  __ Add(optimized_code_entry, optimized_code_entry,
         Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Str(optimized_code_entry,
         FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, optimized_code_entry, scratch2);

  // Link the closure into the optimized function list.
  __ Ldr(native_context, NativeContextMemOperand());
  __ Ldr(scratch2,
         ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ Str(scratch2,
         FieldMemOperand(closure, JSFunction::kNextFunctionLinkOffset));
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, scratch2,
                      scratch3, kLRHasNotBeenSaved, kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ Str(closure,
         ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ Mov(scratch2, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, scratch2,
                            scratch3, kLRHasNotBeenSaved, kDontSaveFPRegs);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ ldr(args_count,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ldr(args_count.W(),
         FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments.
  __ Drop(args_count, 1);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   - x1: the JS function object being called.
//   - x3: the new target
//   - cp: our context.
//   - fp: our caller's frame pointer.
//   - jssp: stack pointer.
//   - lr: return address.
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ Push(lr, fp, cp, x1);
  __ Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);

  // First check if there is optimized code in the feedback vector which we
  // could call instead.
  Label switch_to_optimized_code;
  Register optimized_code_entry = x7;
  __ Ldr(x0, FieldMemOperand(x1, JSFunction::kFeedbackVectorOffset));
  __ Ldr(x0, FieldMemOperand(x0, Cell::kValueOffset));
  __ Ldr(
      optimized_code_entry,
      FieldMemOperand(x0, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                              FeedbackVector::kHeaderSize));
  __ Ldr(optimized_code_entry,
         FieldMemOperand(optimized_code_entry, WeakCell::kValueOffset));
  __ JumpIfNotSmi(optimized_code_entry, &switch_to_optimized_code);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ Ldr(x0, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  Register debug_info = kInterpreterBytecodeArrayRegister;
  Label load_debug_bytecode_array, bytecode_array_loaded;
  DCHECK(!debug_info.is(x0));
  __ Ldr(debug_info, FieldMemOperand(x0, SharedFunctionInfo::kDebugInfoOffset));
  __ JumpIfNotSmi(debug_info, &load_debug_bytecode_array);
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(x0, SharedFunctionInfo::kFunctionDataOffset));
  __ Bind(&bytecode_array_loaded);

  // Check whether we should continue to use the interpreter.
  // TODO(rmcilroy) Remove self healing once liveedit only has to deal with
  // Ignition bytecode.
  Label switch_to_different_code_kind;
  __ Ldr(x0, FieldMemOperand(x0, SharedFunctionInfo::kCodeOffset));
  __ Cmp(x0, Operand(masm->CodeObject()));  // Self-reference to this code.
  __ B(ne, &switch_to_different_code_kind);

  // Increment invocation count for the function.
  __ Ldr(x11, FieldMemOperand(x1, JSFunction::kFeedbackVectorOffset));
  __ Ldr(x11, FieldMemOperand(x11, Cell::kValueOffset));
  __ Ldr(x10, FieldMemOperand(
                  x11, FeedbackVector::kInvocationCountIndex * kPointerSize +
                           FeedbackVector::kHeaderSize));
  __ Add(x10, x10, Operand(Smi::FromInt(1)));
  __ Str(x10, FieldMemOperand(
                  x11, FeedbackVector::kInvocationCountIndex * kPointerSize +
                           FeedbackVector::kHeaderSize));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister,
                    kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, x0, x0,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ Mov(x10, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ Strb(x10, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                               BytecodeArray::kBytecodeAgeOffset));

  // Load the initial bytecode offset.
  __ Mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push new.target, bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(x0, kInterpreterBytecodeOffsetRegister);
  __ Push(x3, kInterpreterBytecodeArrayRegister, x0);

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
    __ CallRuntime(Runtime::kThrowStackOverflow);
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

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ Mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x1, LSL, kPointerSizeLog2));
  __ Ldr(ip0, MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Call(ip0);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in x0.
  LeaveInterpreterFrame(masm, x2);
  __ Ret();

  // Load debug copy of the bytecode array.
  __ Bind(&load_debug_bytecode_array);
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex));
  __ B(&bytecode_array_loaded);

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  __ Ldr(x7, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x7, FieldMemOperand(x7, SharedFunctionInfo::kCodeOffset));
  __ Add(x7, x7, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Str(x7, FieldMemOperand(x1, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(x1, x7, x5);
  __ Jump(x7);

  // If there is optimized code on the type feedback vector, check if it is good
  // to run, and if so, self heal the closure and call the optimized code.
  __ bind(&switch_to_optimized_code);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  Label gotta_call_runtime;

  // Check if the optimized code is marked for deopt.
  __ Ldr(w8, FieldMemOperand(optimized_code_entry,
                             Code::kKindSpecificFlags1Offset));
  __ TestAndBranchIfAnySet(w8, 1 << Code::kMarkedForDeoptimizationBit,
                           &gotta_call_runtime);

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  ReplaceClosureEntryWithOptimizedCode(masm, optimized_code_entry, x1, x4, x5,
                                       x13);
  __ Jump(optimized_code_entry);

  // Optimized code is marked for deopt, bailout to the CompileLazy runtime
  // function which will clear the feedback vector's optimized code slot.
  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch,
                                        Label* stack_overflow) {
  // Check the stack for overflow.
  // We are not trying to catch interruptions (e.g. debug break and
  // preemption) here, so the "real stack limit" is checked.
  Label enough_stack_space;
  __ LoadRoot(scratch, Heap::kRealStackLimitRootIndex);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ Sub(scratch, jssp, scratch);
  // Check if the arguments will overflow the stack.
  __ Cmp(scratch, Operand(num_args, LSL, kPointerSizeLog2));
  __ B(le, stack_overflow);
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register last_arg, Register stack_addr,
                                         Register scratch) {
  __ Mov(scratch, num_args);
  __ lsl(scratch, scratch, kPointerSizeLog2);
  __ sub(last_arg, index, scratch);

  // Set stack pointer and where to stop.
  __ Mov(stack_addr, jssp);
  __ Claim(scratch, 1);

  // Push the arguments.
  Label loop_header, loop_check;
  __ B(&loop_check);
  __ Bind(&loop_header);
  // TODO(rmcilroy): Push two at a time once we ensure we keep stack aligned.
  __ Ldr(scratch, MemOperand(index, -kPointerSize, PostIndex));
  __ Str(scratch, MemOperand(stack_addr, -kPointerSize, PreIndex));
  __ Bind(&loop_check);
  __ Cmp(index, last_arg);
  __ B(gt, &loop_header);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    TailCallMode tail_call_mode, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  // Add one for the receiver.
  __ add(x3, x0, Operand(1));

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, x3, x6, &stack_overflow);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ Mov(x3, x0);  // Argument count is correct.
  }

  // Push the arguments. x2, x4, x5, x6 will be modified.
  Generate_InterpreterPushArgs(masm, x3, x2, x4, x5, x6);

  // Call the target.
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ Jump(masm->isolate()->builtins()->CallFunction(ConvertReceiverMode::kAny,
                                                      tail_call_mode),
            RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(masm->isolate()->builtins()->CallWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                              tail_call_mode),
            RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- x0 : argument count (not including receiver)
  // -- x3 : new target
  // -- x1 : constructor to call
  // -- x2 : allocation site feedback if available, undefined otherwise
  // -- x4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver.
  __ Push(xzr);

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, x0, x7, &stack_overflow);

  // Push the arguments. x5, x4, x6, x7 will be modified.
  Generate_InterpreterPushArgs(masm, x0, x4, x5, x6, x7);

  __ AssertUndefinedOrAllocationSite(x2, x6);
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ AssertFunction(x1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ Ldr(x4, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(x4, FieldMemOperand(x4, SharedFunctionInfo::kConstructStubOffset));
    __ Add(x4, x4, Code::kHeaderSize - kHeapObjectTag);
    __ Br(x4);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with x0, x1, and x3 unmodified.
    __ Jump(masm->isolate()->builtins()->ConstructWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with x0, x1, and x3 unmodified.
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructArray(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  // -- x0 : argument count (not including receiver)
  // -- x1 : target to call verified to be Array function
  // -- x2 : allocation site feedback if available, undefined otherwise.
  // -- x3 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver.
  __ Push(xzr);

  // Add a stack check before pushing arguments.
  Generate_StackOverflowCheck(masm, x0, x7, &stack_overflow);

  // Push the arguments. x3, x5, x6, x7 will be modified.
  Generate_InterpreterPushArgs(masm, x0, x3, x5, x6, x7);

  // Array constructor expects constructor in x3. It is same as call target.
  __ mov(x3, x1);

  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ LoadObject(x1, masm->isolate()->builtins()->InterpreterEntryTrampoline());
  __ Add(lr, x1, Operand(interpreter_entry_return_pc_offset->value() +
                         Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ Mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ Ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister,
                    kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, x1, x1,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ Ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x1, LSL, kPointerSizeLog2));
  __ Ldr(ip0, MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Jump(ip0);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ Ldr(x1, MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ldr(x2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister, x1, x2);
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ Mov(x2, x0);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ Str(x2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee)
  //  -- x3 : new target (preserved for callee)
  //  -- x1 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;
  Label try_shared;

  Register closure = x1;
  Register index = x2;

  // Do we have a valid feedback vector?
  __ Ldr(index, FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ Ldr(index, FieldMemOperand(index, Cell::kValueOffset));
  __ JumpIfRoot(index, Heap::kUndefinedValueRootIndex, &gotta_call_runtime);

  // Is optimized code available in the feedback vector?
  Register entry = x7;
  __ Ldr(entry, FieldMemOperand(
                    index, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                               FeedbackVector::kHeaderSize));
  __ Ldr(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found code, check if it is marked for deopt, if so call into runtime to
  // clear the optimized code slot.
  __ Ldr(w8, FieldMemOperand(entry, Code::kKindSpecificFlags1Offset));
  __ TestAndBranchIfAnySet(w8, 1 << Code::kMarkedForDeoptimizationBit,
                           &gotta_call_runtime);

  // Code is good, get it into the closure and tail call.
  ReplaceClosureEntryWithOptimizedCode(masm, entry, closure, x4, x5, x13);
  __ Jump(entry);

  // We found no optimized code.
  Register temp = x5;
  __ Bind(&try_shared);
  __ Ldr(entry,
         FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ Ldrb(temp, FieldMemOperand(
                    entry, SharedFunctionInfo::kMarkedForTierUpByteOffset));
  __ TestAndBranchIfAnySet(
      temp, 1 << SharedFunctionInfo::kMarkedForTierUpBitWithinByte,
      &gotta_call_runtime);

  // If SFI points to anything other than CompileLazy, install that.
  __ Ldr(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(temp, masm->CodeObject());
  __ Cmp(entry, temp);
  __ B(eq, &gotta_call_runtime);

  // Install the SFI's code entry.
  __ Add(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Str(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, x5);
  __ Jump(entry);

  __ Bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm,
                                 Runtime::kCompileOptimized_NotConcurrent);
}

void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileOptimized_Concurrent);
}

void Builtins::Generate_InstantiateAsmJs(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee)
  //  -- x1 : new target (preserved for callee)
  //  -- x3 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(x4, x0);
    // Push a copy of the target function and the new target.
    __ SmiTag(x0);
    // Push another copy as a parameter to the runtime call.
    __ Push(x0, x1, x3, x1);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ cmp(x4, Operand(j));
        __ B(ne, &over);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ ldr(x4, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                      i * kPointerSize));
        __ push(x4);
      }
      for (int i = 0; i < 3 - j; ++i) {
        __ PushRoot(Heap::kUndefinedValueRootIndex);
      }
      if (j < 3) {
        __ jmp(&args_done);
        __ bind(&over);
      }
    }
    __ bind(&args_done);

    // Call runtime, on success unwind frame, and parent frame.
    __ CallRuntime(Runtime::kInstantiateAsmJs, 4);
    // A smi 0 is returned on failure, an object on success.
    __ JumpIfSmi(x0, &failed);

    __ Drop(2);
    __ pop(x4);
    __ SmiUntag(x4);
    scope.GenerateLeaveFrame();

    __ add(x4, x4, Operand(1));
    __ Drop(x4);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(x3, x1, x0);
    __ SmiUntag(x0);
  }
  // On failure, tail call back to regular js.
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
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
  //   x3 - new target
  //   lr - The return address for the JSFunction itself. It has not yet been
  //        preserved on the stack because the frame setup code was replaced
  //        with a call to this stub, to handle code ageing.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Push(x0, x1, x3, fp, lr);
    __ Mov(x1, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(
        ExternalReference::get_make_code_young_function(masm->isolate()), 2);
    __ Pop(lr, fp, x3, x1, x0);
  }

  // The calling function has been made young again, so return to execute the
  // real frame set-up code.
  __ Br(x0);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                              \
  void Builtins::Generate_Make##C##CodeYoungAgain(MacroAssembler* masm) { \
    GenerateMakeCodeYoungAgainCommon(masm);                               \
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
  //   x3 - new target
  //   lr - The return address for the JSFunction itself. It has not yet been
  //        preserved on the stack because the frame setup code was replaced
  //        with a call to this stub, to handle code ageing.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Push(x0, x1, x3, fp, lr);
    __ Mov(x1, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(
        ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
        2);
    __ Pop(lr, fp, x3, x1, x0);

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
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
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
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Get the full codegen state from the stack and untag it.
  Register state = x6;
  __ Peek(state, 0);
  __ SmiUntag(state);

  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ CompareAndBranch(state,
                      static_cast<int>(Deoptimizer::BailoutState::NO_REGISTERS),
                      ne, &with_tos_register);
  __ Drop(1);  // Remove state.
  __ Ret();

  __ Bind(&with_tos_register);
  // Reload TOS register.
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), x0.code());
  __ Peek(x0, kPointerSize);
  __ CompareAndBranch(state,
                      static_cast<int>(Deoptimizer::BailoutState::TOS_REGISTER),
                      ne, &unknown_state);
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

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ Ldr(x0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ Ldr(x0, MemOperand(x0, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ Ldr(x0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ Push(x0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CompareAndBranch(x0, Smi::kZero, ne, &skip);
  __ Ret();

  __ Bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ Ldr(x1, MemOperand(x0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ Ldrsw(w1, UntagSmiFieldMemOperand(
                   x1, FixedArray::OffsetOfElementAt(
                           DeoptimizationInputData::kOsrPcOffsetIndex)));

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ Add(x0, x0, x1);
  __ Add(lr, x0, Code::kHeaderSize - kHeapObjectTag);

  // And "return" to the OSR entry point of the function.
  __ Ret();
}

void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, false);
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  Generate_OnStackReplacementHelper(masm, true);
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- jssp[0]  : argArray (if argc == 2)
  //  -- jssp[8]  : thisArg  (if argc >= 1)
  //  -- jssp[16] : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_FunctionPrototypeApply");

  Register argc = x0;
  Register arg_array = x0;
  Register receiver = x1;
  Register this_arg = x2;
  Register undefined_value = x3;
  Register null_value = x4;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);

  // 1. Load receiver into x1, argArray into x0 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    // Claim (2 - argc) dummy arguments from the stack, to put the stack in a
    // consistent state for a simple pop operation.
    __ Claim(2);
    __ Drop(argc);

    // ----------- S t a t e -------------
    //  -- x0       : argc
    //  -- jssp[0]  : argArray (dummy value if argc <= 1)
    //  -- jssp[8]  : thisArg  (dummy value if argc == 0)
    //  -- jssp[16] : receiver
    // -----------------------------------
    __ Cmp(argc, 1);
    __ Pop(arg_array, this_arg);               // Overwrites argc.
    __ CmovX(this_arg, undefined_value, lo);   // undefined if argc == 0.
    __ CmovX(arg_array, undefined_value, ls);  // undefined if argc <= 1.

    __ Peek(receiver, 0);
    __ Poke(this_arg, 0);
  }

  // ----------- S t a t e -------------
  //  -- x0      : argArray
  //  -- x1      : receiver
  //  -- x3      : undefined root value
  //  -- jssp[0] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(receiver, &receiver_not_callable);
  __ Ldr(x10, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Ldrb(w10, FieldMemOperand(x10, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x10, 1 << Map::kIsCallable,
                             &receiver_not_callable);

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ Cmp(arg_array, null_value);
  __ Ccmp(arg_array, undefined_value, ZFlag, ne);
  __ B(eq, &no_arguments);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target in x3).
  DCHECK(undefined_value.Is(x3));
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ Bind(&no_arguments);
  {
    __ Mov(x0, 0);
    DCHECK(receiver.Is(x1));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ Bind(&receiver_not_callable);
  {
    __ Poke(receiver, 0);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  Register argc = x0;
  Register function = x1;
  Register scratch1 = x10;
  Register scratch2 = x11;

  ASM_LOCATION("Builtins::Generate_FunctionPrototypeCall");

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

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- jssp[0]  : argumentsList (if argc == 3)
  //  -- jssp[8]  : thisArgument  (if argc >= 2)
  //  -- jssp[16] : target        (if argc >= 1)
  //  -- jssp[24] : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ReflectApply");

  Register argc = x0;
  Register arguments_list = x0;
  Register target = x1;
  Register this_argument = x2;
  Register undefined_value = x3;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);

  // 1. Load target into x1 (if present), argumentsList into x0 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    // Claim (3 - argc) dummy arguments from the stack, to put the stack in a
    // consistent state for a simple pop operation.
    __ Claim(3);
    __ Drop(argc);

    // ----------- S t a t e -------------
    //  -- x0       : argc
    //  -- jssp[0]  : argumentsList (dummy value if argc <= 2)
    //  -- jssp[8]  : thisArgument  (dummy value if argc <= 1)
    //  -- jssp[16] : target        (dummy value if argc == 0)
    //  -- jssp[24] : receiver
    // -----------------------------------
    __ Adds(x10, argc, 0);  // Preserve argc, and set the Z flag if it is zero.
    __ Pop(arguments_list, this_argument, target);  // Overwrites argc.
    __ CmovX(target, undefined_value, eq);          // undefined if argc == 0.
    __ Cmp(x10, 2);
    __ CmovX(this_argument, undefined_value, lo);   // undefined if argc <= 1.
    __ CmovX(arguments_list, undefined_value, ls);  // undefined if argc <= 2.

    __ Poke(this_argument, 0);  // Overwrite receiver.
  }

  // ----------- S t a t e -------------
  //  -- x0      : argumentsList
  //  -- x1      : target
  //  -- jssp[0] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(target, &target_not_callable);
  __ Ldr(x10, FieldMemOperand(target, HeapObject::kMapOffset));
  __ Ldr(x10, FieldMemOperand(x10, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x10, 1 << Map::kIsCallable, &target_not_callable);

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target in x3).
  DCHECK(undefined_value.Is(x3));
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ Bind(&target_not_callable);
  {
    __ Poke(target, 0);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0       : argc
  //  -- jssp[0]  : new.target (optional)
  //  -- jssp[8]  : argumentsList
  //  -- jssp[16] : target
  //  -- jssp[24] : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ReflectConstruct");

  Register argc = x0;
  Register arguments_list = x0;
  Register target = x1;
  Register new_target = x3;
  Register undefined_value = x4;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);

  // 1. Load target into x1 (if present), argumentsList into x0 (if present),
  // new.target into x3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    // Claim (3 - argc) dummy arguments from the stack, to put the stack in a
    // consistent state for a simple pop operation.
    __ Claim(3);
    __ Drop(argc);

    // ----------- S t a t e -------------
    //  -- x0       : argc
    //  -- jssp[0]  : new.target    (dummy value if argc <= 2)
    //  -- jssp[8]  : argumentsList (dummy value if argc <= 1)
    //  -- jssp[16] : target        (dummy value if argc == 0)
    //  -- jssp[24] : receiver
    // -----------------------------------
    __ Adds(x10, argc, 0);  // Preserve argc, and set the Z flag if it is zero.
    __ Pop(new_target, arguments_list, target);  // Overwrites argc.
    __ CmovX(target, undefined_value, eq);       // undefined if argc == 0.
    __ Cmp(x10, 2);
    __ CmovX(arguments_list, undefined_value, lo);  // undefined if argc <= 1.
    __ CmovX(new_target, target, ls);               // target if argc <= 2.

    __ Poke(undefined_value, 0);  // Overwrite receiver.
  }

  // ----------- S t a t e -------------
  //  -- x0      : argumentsList
  //  -- x1      : target
  //  -- x3      : new.target
  //  -- jssp[0] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(target, &target_not_constructor);
  __ Ldr(x10, FieldMemOperand(target, HeapObject::kMapOffset));
  __ Ldrb(x10, FieldMemOperand(x10, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x10, 1 << Map::kIsConstructor,
                             &target_not_constructor);

  // 3. Make sure the new.target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(new_target, &new_target_not_constructor);
  __ Ldr(x10, FieldMemOperand(new_target, HeapObject::kMapOffset));
  __ Ldrb(x10, FieldMemOperand(x10, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x10, 1 << Map::kIsConstructor,
                             &new_target_not_constructor);

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ Bind(&target_not_constructor);
  {
    __ Poke(target, 0);
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ Bind(&new_target_not_constructor);
  {
    __ Poke(new_target, 0);
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(x10, x0);
  __ Mov(x11, StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR));
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
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0      : argumentsList
  //  -- x1      : target
  //  -- x3      : new.target (checked to be constructor or undefined)
  //  -- jssp[0] : thisArgument
  // -----------------------------------

  Register arguments_list = x0;
  Register target = x1;
  Register new_target = x3;

  Register args = x0;
  Register len = x2;

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(arguments_list, &create_runtime);

    // Load native context.
    Register native_context = x4;
    __ Ldr(native_context, NativeContextMemOperand());

    // Load the map of argumentsList.
    Register arguments_list_map = x2;
    __ Ldr(arguments_list_map,
           FieldMemOperand(arguments_list, HeapObject::kMapOffset));

    // Check if argumentsList is an (unmodified) arguments object.
    __ Ldr(x10, ContextMemOperand(native_context,
                                  Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ Ldr(x11, ContextMemOperand(native_context,
                                  Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ Cmp(arguments_list_map, x10);
    __ Ccmp(arguments_list_map, x11, ZFlag, ne);
    __ B(eq, &create_arguments);

    // Check if argumentsList is a fast JSArray.
    __ CompareInstanceType(arguments_list_map, x10, JS_ARRAY_TYPE);
    __ B(eq, &create_array);

    // Ask the runtime to create the list (actually a FixedArray).
    __ Bind(&create_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(target, new_target, arguments_list);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ Pop(new_target, target);
      __ Ldrsw(len, UntagSmiFieldMemOperand(arguments_list,
                                            FixedArray::kLengthOffset));
    }
    __ B(&done_create);

    // Try to create the list from an arguments object.
    __ Bind(&create_arguments);
    __ Ldrsw(len, UntagSmiFieldMemOperand(arguments_list,
                                          JSArgumentsObject::kLengthOffset));
    __ Ldr(x10, FieldMemOperand(arguments_list, JSObject::kElementsOffset));
    __ Ldrsw(x11, UntagSmiFieldMemOperand(x10, FixedArray::kLengthOffset));
    __ CompareAndBranch(len, x11, ne, &create_runtime);
    __ Mov(args, x10);
    __ B(&done_create);

    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ Bind(&create_holey_array);
    //  -- x2 : arguments_list_map
    //  -- x4 : native_context
    Register arguments_list_prototype = x2;
    __ Ldr(arguments_list_prototype,
           FieldMemOperand(arguments_list_map, Map::kPrototypeOffset));
    __ Ldr(x10, ContextMemOperand(native_context,
                                  Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ Cmp(arguments_list_prototype, x10);
    __ B(ne, &create_runtime);
    __ LoadRoot(x10, Heap::kArrayProtectorRootIndex);
    __ Ldrsw(x11, UntagSmiFieldMemOperand(x10, PropertyCell::kValueOffset));
    __ Cmp(x11, Isolate::kProtectorValid);
    __ B(ne, &create_runtime);
    __ Ldrsw(len,
             UntagSmiFieldMemOperand(arguments_list, JSArray::kLengthOffset));
    __ Ldr(args, FieldMemOperand(arguments_list, JSArray::kElementsOffset));
    __ B(&done_create);

    // Try to create the list from a JSArray object.
    __ Bind(&create_array);
    __ Ldr(x10, FieldMemOperand(arguments_list_map, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(x10);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    // Check if it is a holey array, the order of the cmp is important as
    // anything higher than FAST_HOLEY_ELEMENTS will fall back to runtime.
    __ Cmp(x10, FAST_HOLEY_ELEMENTS);
    __ B(hi, &create_runtime);
    // Only FAST_XXX after this point, FAST_HOLEY_XXX are odd values.
    __ Tbnz(x10, 0, &create_holey_array);
    // FAST_SMI_ELEMENTS or FAST_ELEMENTS after this point.
    __ Ldrsw(len,
             UntagSmiFieldMemOperand(arguments_list, JSArray::kLengthOffset));
    __ Ldr(args, FieldMemOperand(arguments_list, JSArray::kElementsOffset));

    __ Bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(x10, Heap::kRealStackLimitRootIndex);
    // Make x10 the space we have left. The stack might already be overflowed
    // here which will cause x10 to become negative.
    __ Sub(x10, masm->StackPointer(), x10);
    // Check if the arguments will overflow the stack.
    __ Cmp(x10, Operand(len, LSL, kPointerSizeLog2));
    __ B(gt, &done);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ Bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- x0      : args (a FixedArray built from argumentsList)
  //  -- x1      : target
  //  -- x2      : len (number of elements to push from args)
  //  -- x3      : new.target (checked to be constructor or undefined)
  //  -- jssp[0] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label done, push, loop;
    Register src = x4;

    __ Add(src, args, FixedArray::kHeaderSize - kHeapObjectTag);
    __ Mov(x0, len);  // The 'len' argument for Call() or Construct().
    __ Cbz(len, &done);
    Register the_hole_value = x11;
    Register undefined_value = x12;
    // We do not use the CompareRoot macro as it would do a LoadRoot behind the
    // scenes and we want to avoid that in a loop.
    __ LoadRoot(the_hole_value, Heap::kTheHoleValueRootIndex);
    __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);
    __ Claim(len);
    __ Bind(&loop);
    __ Sub(len, len, 1);
    __ Ldr(x10, MemOperand(src, kPointerSize, PostIndex));
    __ Cmp(x10, the_hole_value);
    __ Csel(x10, x10, undefined_value, ne);
    __ Poke(x10, Operand(len, LSL, kPointerSizeLog2));
    __ Cbnz(len, &loop);
    __ Bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- x0              : argument count (len)
  //  -- x1              : target
  //  -- x3              : new.target (checked to be constructor or undefined)
  //  -- jssp[0]         : args[len-1]
  //  -- jssp[8]         : args[len-2]
  //      ...            :  ...
  //  -- jssp[8*(len-2)] : args[1]
  //  -- jssp[8*(len-1)] : args[0]
  // -----------------------------------

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    __ CompareRoot(new_target, Heap::kUndefinedValueRootIndex);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET, eq);
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_ForwardVarargs(MacroAssembler* masm,
                                       Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x3 : the new.target (for [[Construct]] calls)
  //  -- x1 : the target to call (can be any Object)
  //  -- x2 : start index (to support rest parameters)
  // -----------------------------------

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ Ldr(x5, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(x4, MemOperand(x5, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Cmp(x4, StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR));
  __ B(eq, &arguments_adaptor);
  {
    __ Ldr(x6, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ Ldr(x6, FieldMemOperand(x6, JSFunction::kSharedFunctionInfoOffset));
    __ Ldrsw(x6, FieldMemOperand(
                     x6, SharedFunctionInfo::kFormalParameterCountOffset));
    __ Mov(x5, fp);
  }
  __ B(&arguments_done);
  __ Bind(&arguments_adaptor);
  {
    // Just load the length from ArgumentsAdaptorFrame.
    __ Ldrsw(x6, UntagSmiMemOperand(
                     x5, ArgumentsAdaptorFrameConstants::kLengthOffset));
  }
  __ Bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ Subs(x6, x6, x2);
  __ B(le, &stack_done);
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, x6, x2, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ Add(x5, x5, kPointerSize);
      __ Add(x0, x0, x6);
      __ bind(&loop);
      {
        __ Ldr(x4, MemOperand(x5, x6, LSL, kPointerSizeLog2));
        __ Push(x4);
        __ Subs(x6, x6, 1);
        __ B(ne, &loop);
      }
    }
  }
  __ B(&stack_done);
  __ Bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ Bind(&stack_done);

  __ Jump(code, RelocInfo::CODE_TARGET);
}

namespace {

// Drops top JavaScript frame and an arguments adaptor frame below it (if
// present) preserving all the arguments prepared for current call.
// Does nothing if debugger is currently active.
// ES6 14.6.3. PrepareForTailCall
//
// Stack structure for the function g() tail calling f():
//
// ------- Caller frame: -------
// |  ...
// |  g()'s arg M
// |  ...
// |  g()'s arg 1
// |  g()'s receiver arg
// |  g()'s caller pc
// ------- g()'s frame: -------
// |  g()'s caller fp      <- fp
// |  g()'s context
// |  function pointer: g
// |  -------------------------
// |  ...
// |  ...
// |  f()'s arg N
// |  ...
// |  f()'s arg 1
// |  f()'s receiver arg   <- sp (f()'s caller pc is not on the stack yet!)
// ----------------------
//
void PrepareForTailCall(MacroAssembler* masm, Register args_reg,
                        Register scratch1, Register scratch2,
                        Register scratch3) {
  DCHECK(!AreAliased(args_reg, scratch1, scratch2, scratch3));
  Comment cmnt(masm, "[ PrepareForTailCall");

  // Prepare for tail call only if ES2015 tail call elimination is enabled.
  Label done;
  ExternalReference is_tail_call_elimination_enabled =
      ExternalReference::is_tail_call_elimination_enabled_address(
          masm->isolate());
  __ Mov(scratch1, Operand(is_tail_call_elimination_enabled));
  __ Ldrb(scratch1, MemOperand(scratch1));
  __ Cmp(scratch1, Operand(0));
  __ B(eq, &done);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ Ldr(scratch3,
           MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ Cmp(scratch3, Operand(StackFrame::TypeToMarker(StackFrame::STUB)));
    __ B(ne, &no_interpreter_frame);
    __ Ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ Ldr(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(scratch3,
         MemOperand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Cmp(scratch3,
         Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ B(ne, &no_arguments_adaptor);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(fp, scratch2);
  __ Ldr(caller_args_count_reg,
         MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ B(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ Ldr(scratch1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ Ldr(scratch1,
         FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldrsw(caller_args_count_reg,
           FieldMemOperand(scratch1,
                           SharedFunctionInfo::kFormalParameterCountOffset));
  __ bind(&formal_parameter_count_loaded);

  ParameterCount callee_args_count(args_reg);
  __ PrepareForTailCall(callee_args_count, caller_args_count_reg, scratch2,
                        scratch3);
  __ bind(&done);
}
}  // namespace

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode,
                                     TailCallMode tail_call_mode) {
  ASM_LOCATION("Builtins::Generate_CallFunction");
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(x1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that function is not a "classConstructor".
  Label class_constructor;
  __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(w3, FieldMemOperand(x2, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestAndBranchIfAnySet(w3, FunctionKind::kClassConstructor
                                   << SharedFunctionInfo::kFunctionKindShift,
                           &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ TestAndBranchIfAnySet(w3,
                           (1 << SharedFunctionInfo::kNative) |
                               (1 << SharedFunctionInfo::kStrictModeFunction),
                           &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- x0 : the number of arguments (not including the receiver)
    //  -- x1 : the function to call (checked to be a JSFunction)
    //  -- x2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(x3);
    } else {
      Label convert_to_object, convert_receiver;
      __ Peek(x3, Operand(x0, LSL, kXRegSizeLog2));
      __ JumpIfSmi(x3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(x3, x4, x4, FIRST_JS_RECEIVER_TYPE);
      __ B(hs, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(x3, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(x3, Heap::kNullValueRootIndex, &convert_to_object);
        __ Bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(x3);
        }
        __ B(&convert_receiver);
      }
      __ Bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(x0);
        __ Push(x0, x1);
        __ Mov(x0, x3);
        __ Push(cp);
        __ Call(masm->isolate()->builtins()->ToObject(),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ Mov(x3, x0);
        __ Pop(x1, x0);
        __ SmiUntag(x0);
      }
      __ Ldr(x2, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
      __ Bind(&convert_receiver);
    }
    __ Poke(x3, Operand(x0, LSL, kXRegSizeLog2));
  }
  __ Bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSFunction)
  //  -- x2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, x0, x3, x4, x5);
  }

  __ Ldrsw(
      x2, FieldMemOperand(x2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(x0);
  ParameterCount expected(x2);
  __ InvokeFunctionCode(x1, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(x1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : target (checked to be a JSBoundFunction)
  //  -- x3 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into x2 and length of that into x4.
  Label no_bound_arguments;
  __ Ldr(x2, FieldMemOperand(x1, JSBoundFunction::kBoundArgumentsOffset));
  __ Ldrsw(x4, UntagSmiFieldMemOperand(x2, FixedArray::kLengthOffset));
  __ Cmp(x4, 0);
  __ B(eq, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- x0 : the number of arguments (not including the receiver)
    //  -- x1 : target (checked to be a JSBoundFunction)
    //  -- x2 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- x3 : new.target (only in case of [[Construct]])
    //  -- x4 : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ Claim(x4);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(jssp, Heap::kRealStackLimitRootIndex);
      __ B(gt, &done);  // Signed comparison.
      // Restore the stack pointer.
      __ Drop(x4);
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ Bind(&done);
    }

    // Relocate arguments down the stack.
    {
      Label loop, done_loop;
      __ Mov(x5, 0);
      __ Bind(&loop);
      __ Cmp(x5, x0);
      __ B(gt, &done_loop);
      __ Peek(x10, Operand(x4, LSL, kPointerSizeLog2));
      __ Poke(x10, Operand(x5, LSL, kPointerSizeLog2));
      __ Add(x4, x4, 1);
      __ Add(x5, x5, 1);
      __ B(&loop);
      __ Bind(&done_loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ Ldrsw(x4, UntagSmiFieldMemOperand(x2, FixedArray::kLengthOffset));
      __ Add(x2, x2, FixedArray::kHeaderSize - kHeapObjectTag);
      __ Bind(&loop);
      __ Sub(x4, x4, 1);
      __ Ldr(x10, MemOperand(x2, x4, LSL, kPointerSizeLog2));
      __ Poke(x10, Operand(x0, LSL, kPointerSizeLog2));
      __ Add(x0, x0, 1);
      __ Cmp(x4, 0);
      __ B(gt, &loop);
    }
  }
  __ Bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(x1);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, x0, x3, x4, x5);
  }

  // Patch the receiver to [[BoundThis]].
  __ Ldr(x10, FieldMemOperand(x1, JSBoundFunction::kBoundThisOffset));
  __ Poke(x10, Operand(x0, LSL, kPointerSizeLog2));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ Ldr(x1, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Mov(x10,
         ExternalReference(Builtins::kCall_ReceiverIsAny, masm->isolate()));
  __ Ldr(x11, MemOperand(x10));
  __ Add(x12, x11, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x12);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(x1, &non_callable);
  __ Bind(&non_smi);
  __ CompareObjectType(x1, x4, x5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
          RelocInfo::CODE_TARGET, eq);
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ Ldrb(x4, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x4, 1 << Map::kIsCallable, &non_callable);

  __ Cmp(x5, JS_PROXY_TYPE);
  __ B(ne, &non_function);

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, x0, x3, x4, x5);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ Push(x1);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ Add(x0, x0, Operand(2));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ Bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, x1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void CheckSpreadAndPushToStack(MacroAssembler* masm) {
  Register argc = x0;
  Register constructor = x1;
  Register new_target = x3;

  Register scratch = x2;
  Register scratch2 = x6;

  Register spread = x4;
  Register spread_map = x5;

  Register spread_len = x5;

  Label runtime_call, push_args;
  __ Peek(spread, 0);
  __ JumpIfSmi(spread, &runtime_call);
  __ Ldr(spread_map, FieldMemOperand(spread, HeapObject::kMapOffset));

  // Check that the spread is an array.
  __ CompareInstanceType(spread_map, scratch, JS_ARRAY_TYPE);
  __ B(ne, &runtime_call);

  // Check that we have the original ArrayPrototype.
  __ Ldr(scratch, FieldMemOperand(spread_map, Map::kPrototypeOffset));
  __ Ldr(scratch2, NativeContextMemOperand());
  __ Ldr(scratch2,
         ContextMemOperand(scratch2, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
  __ Cmp(scratch, scratch2);
  __ B(ne, &runtime_call);

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  __ LoadRoot(scratch, Heap::kArrayIteratorProtectorRootIndex);
  __ Ldr(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ Cmp(scratch, Smi::FromInt(Isolate::kProtectorValid));
  __ B(ne, &runtime_call);

  // Check that the map of the initial array iterator hasn't changed.
  __ Ldr(scratch2, NativeContextMemOperand());
  __ Ldr(scratch,
         ContextMemOperand(scratch2,
                           Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_INDEX));
  __ Ldr(scratch, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ Ldr(scratch2,
         ContextMemOperand(
             scratch2, Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_MAP_INDEX));
  __ Cmp(scratch, scratch2);
  __ B(ne, &runtime_call);

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  Label no_protector_check;
  __ Ldr(scratch, FieldMemOperand(spread_map, Map::kBitField2Offset));
  __ DecodeField<Map::ElementsKindBits>(scratch);
  __ Cmp(scratch, FAST_HOLEY_ELEMENTS);
  __ B(hi, &runtime_call);
  // For non-FastHoley kinds, we can skip the protector check.
  __ Cmp(scratch, FAST_SMI_ELEMENTS);
  __ B(eq, &no_protector_check);
  __ Cmp(scratch, FAST_ELEMENTS);
  __ B(eq, &no_protector_check);
  // Check the ArrayProtector cell.
  __ LoadRoot(scratch, Heap::kArrayProtectorRootIndex);
  __ Ldr(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ Cmp(scratch, Smi::FromInt(Isolate::kProtectorValid));
  __ B(ne, &runtime_call);

  __ Bind(&no_protector_check);
  // Load the FixedArray backing store, but use the length from the array.
  __ Ldrsw(spread_len, UntagSmiFieldMemOperand(spread, JSArray::kLengthOffset));
  __ Ldr(spread, FieldMemOperand(spread, JSArray::kElementsOffset));
  __ B(&push_args);

  __ Bind(&runtime_call);
  {
    // Call the builtin for the result of the spread.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(argc);
    __ Push(constructor, new_target, argc, spread);
    __ CallRuntime(Runtime::kSpreadIterableFixed);
    __ Mov(spread, x0);
    __ Pop(argc, new_target, constructor);
    __ SmiUntag(argc);
  }

  {
    // Calculate the new nargs including the result of the spread.
    __ Ldrsw(spread_len,
             UntagSmiFieldMemOperand(spread, FixedArray::kLengthOffset));

    __ Bind(&push_args);
    // argc += spread_len - 1. Subtract 1 for the spread itself.
    __ Add(argc, argc, spread_len);
    __ Sub(argc, argc, 1);

    // Pop the spread argument off the stack.
    __ Pop(scratch);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(scratch, Heap::kRealStackLimitRootIndex);
    // Make scratch the space we have left. The stack might already be
    // overflowed here which will cause scratch to become negative.
    __ Sub(scratch, masm->StackPointer(), scratch);
    // Check if the arguments will overflow the stack.
    __ Cmp(scratch, Operand(spread_len, LSL, kPointerSizeLog2));
    __ B(gt, &done);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ Bind(&done);
  }

  // Put the evaluated spread onto the stack as additional arguments.
  {
    __ Mov(scratch, 0);
    Label done, push, loop;
    __ Bind(&loop);
    __ Cmp(scratch, spread_len);
    __ B(eq, &done);
    __ Add(scratch2, spread, Operand(scratch, LSL, kPointerSizeLog2));
    __ Ldr(scratch2, FieldMemOperand(scratch2, FixedArray::kHeaderSize));
    __ JumpIfNotRoot(scratch2, Heap::kTheHoleValueRootIndex, &push);
    __ LoadRoot(scratch2, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(scratch2);
    __ Add(scratch, scratch, Operand(1));
    __ B(&loop);
    __ Bind(&done);
  }
}

// static
void Builtins::Generate_CallWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (can be any Object)
  // -----------------------------------

  // CheckSpreadAndPushToStack will push r3 to save it.
  __ LoadRoot(x3, Heap::kUndefinedValueRootIndex);
  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            TailCallMode::kDisallow),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (checked to be a JSFunction)
  //  -- x3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(x1);

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
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSBoundFunction)
  //  -- x3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(x1);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ Cmp(x1, x3);
    __ B(ne, &done);
    __ Ldr(x3,
           FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ Bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ Ldr(x1, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Mov(x10, ExternalReference(Builtins::kConstruct, masm->isolate()));
  __ Ldr(x11, MemOperand(x10));
  __ Add(x12, x11, Code::kHeaderSize - kHeapObjectTag);
  __ Br(x12);
}

// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (checked to be a JSProxy)
  //  -- x3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ Push(x1);
  __ Push(x3);
  // Include the pushed new_target, constructor and the receiver.
  __ Add(x0, x0, 3);
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (can be any Object)
  //  -- x3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(x1, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(x1, x4, x5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ Ldrb(x2, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x2, 1 << Map::kIsConstructor, &non_constructor);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructBoundFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ Cmp(x5, JS_PROXY_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, x1);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(masm->isolate()->builtins()->ConstructedNonConstructable(),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the constructor to call (can be any Object)
  //  -- x3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_AllocateInNewSpace");
  // ----------- S t a t e -------------
  //  -- x1 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(x1);
  __ Push(x1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_AllocateInOldSpace");
  // ----------- S t a t e -------------
  //  -- x1 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(x1);
  __ Move(x2, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(x1, x2);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_Abort");
  // ----------- S t a t e -------------
  //  -- x1 : message_id as Smi
  //  -- lr : return address
  // -----------------------------------
  MacroAssembler::NoUseRealAbortsScope no_use_real_aborts(masm);
  __ Push(x1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_ArgumentsAdaptorTrampoline");
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  //  -- x3 : new target (passed through to callee)
  // -----------------------------------

  Register argc_actual = x0;    // Excluding the receiver.
  Register argc_expected = x2;  // Excluding the receiver.
  Register function = x1;
  Register code_entry = x10;

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ Cmp(argc_actual, argc_expected);
  __ B(lt, &too_few);
  __ Cmp(argc_expected, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ B(eq, &dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, x2, x10, &stack_overflow);

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
           MemOperand(copy_start, -2 * kPointerSize, PreIndex));
    __ Stp(scratch1, scratch2,
           MemOperand(copy_to, -2 * kPointerSize, PreIndex));
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

    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, x2, x10, &stack_overflow);

    __ Lsl(scratch2, argc_expected, kPointerSizeLog2);
    __ Lsl(argc_actual, argc_actual, kPointerSizeLog2);

    // Adjust for fp, lr, and the receiver.
    __ Add(copy_from, fp, 3 * kPointerSize);
    __ Add(copy_from, copy_from, argc_actual);
    __ Mov(copy_to, jssp);
    __ Sub(copy_end, copy_to, 1 * kPointerSize);  // Adjust for the receiver.
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
           MemOperand(copy_from, -2 * kPointerSize, PreIndex));
    __ Stp(scratch1, scratch2,
           MemOperand(copy_to, -2 * kPointerSize, PreIndex));
    __ Cmp(copy_to, copy_end);
    __ B(hi, &copy_2_by_2);

    __ Mov(copy_to, copy_end);

    // Fill the remaining expected arguments with undefined.
    __ LoadRoot(scratch1, Heap::kUndefinedValueRootIndex);
    __ Add(copy_end, jssp, kPointerSize);

    Label fill;
    __ Bind(&fill);
    __ Stp(scratch1, scratch1,
           MemOperand(copy_to, -2 * kPointerSize, PreIndex));
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
  // x3 : new target (passed through to callee)
  __ Ldr(code_entry, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
  __ Call(code_entry);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // Call the entry point without adapting the arguments.
  __ Bind(&dont_adapt_arguments);
  __ Ldr(code_entry, FieldMemOperand(function, JSFunction::kCodeEntryOffset));
  __ Jump(code_entry);

  __ Bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ Unreachable();
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // Wasm code uses the csp. This builtin excepts to use the jssp.
  // Thus, move csp to jssp when entering this builtin (called from wasm).
  DCHECK(masm->StackPointer().is(jssp));
  __ Move(jssp, csp);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    const RegList gp_regs = x0.Bit() | x1.Bit() | x2.Bit() | x3.Bit() |
                            x4.Bit() | x5.Bit() | x6.Bit() | x7.Bit();
    const RegList fp_regs = d0.Bit() | d1.Bit() | d2.Bit() | d3.Bit() |
                            d4.Bit() | d5.Bit() | d6.Bit() | d7.Bit();
    __ PushXRegList(gp_regs);
    __ PushDRegList(fp_regs);

    // Initialize cp register with kZero, CEntryStub will use it to set the
    // current context on the isolate.
    __ Move(cp, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);
    // Store returned instruction start in x8.
    __ Add(x8, x0, Code::kHeaderSize - kHeapObjectTag);

    // Restore registers.
    __ PopDRegList(fp_regs);
    __ PopXRegList(gp_regs);
  }
  // Move back to csp land. jssp now has the same value as when entering this
  // function, but csp might have changed in the runtime call.
  __ Move(csp, jssp);
  // Now jump to the instructions of the returned code object.
  __ Jump(x8);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

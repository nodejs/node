// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/codegen.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/frame-constants.h"
#include "src/frames.h"
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
  __ Mov(x5, ExternalReference(address, masm->isolate()));
  if (exit_frame_type == BUILTIN_EXIT) {
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK(exit_frame_type == EXIT);
    __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithExitFrame),
            RelocInfo::CODE_TARGET);
  }
}

namespace {

void AdaptorWithExitFrameType(MacroAssembler* masm,
                              Builtins::ExitFrameType exit_frame_type) {
  // ----------- S t a t e -------------
  //  -- x0                 : number of arguments excluding receiver
  //  -- x1                 : target
  //  -- x3                 : new target
  //  -- x5                 : entry point
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

  // CEntryStub expects x0 to contain the number of arguments including the
  // receiver and the extra arguments.
  const int num_extra_args = 3;
  __ Add(x0, x0, num_extra_args + 1);

  // Insert extra arguments.
  __ SmiTag(x0);
  __ Push(x0, x1, x3);
  __ SmiUntag(x0);

  // Jump to the C entry runtime stub directly here instead of using
  // JumpToExternalReference. We have already loaded entry point to x5
  // in Generate_adaptor.
  __ mov(x1, x5);
  CEntryStub stub(masm->isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                  exit_frame_type == Builtins::BUILTIN_EXIT);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}
}  // namespace

void Builtins::Generate_AdaptorWithExitFrame(MacroAssembler* masm) {
  AdaptorWithExitFrameType(masm, EXIT);
}

void Builtins::Generate_AdaptorWithBuiltinExitFrame(MacroAssembler* masm) {
  AdaptorWithExitFrameType(masm, BUILTIN_EXIT);
}

void Builtins::Generate_InternalArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_InternalArrayConstructor");
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

void Builtins::Generate_ArrayConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_ArrayConstructor");
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
    __ Call(BUILTIN_CODE(masm->isolate(), ToNumber), RelocInfo::CODE_TARGET);
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
      __ Call(BUILTIN_CODE(masm->isolate(), ToNumber), RelocInfo::CODE_TARGET);
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
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
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
    __ Call(BUILTIN_CODE(masm->isolate(), ToString), RelocInfo::CODE_TARGET);
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
      __ Call(BUILTIN_CODE(masm->isolate(), ToString), RelocInfo::CODE_TARGET);
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
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
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
    __ InvokeFunction(x1, x3, actual, CALL_FUNCTION);

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
    __ Ldr(w4, FieldMemOperand(x4, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(w4, Operand(SharedFunctionInfo::kDerivedConstructorMask));
    __ B(ne, &not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        x4, x5);
    __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject),
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
    __ InvokeFunction(x1, x3, actual, CALL_FUNCTION);

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

    // The result is now neither undefined nor an object.
    __ Bind(&other_result);
    __ Ldr(x4, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ Ldr(x4, FieldMemOperand(x4, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(w4, FieldMemOperand(x4, SharedFunctionInfo::kCompilerHintsOffset));
    __ tst(w4, Operand(SharedFunctionInfo::kClassConstructorMask));

    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ B(eq, &use_receiver);
    } else {
      __ B(ne, &use_receiver);
      __ CallRuntime(
          Runtime::kIncrementUseCounterConstructorReturnNonUndefinedPrimitive);
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
  //  -- lr : return address
  // -----------------------------------
  __ AssertGeneratorObject(x1);

  // Store input value into generator object.
  __ Str(x0, FieldMemOperand(x1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(x1, JSGeneratorObject::kInputOrDebugPosOffset, x0, x3,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);

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
    __ Ldr(x5, FieldMemOperand(x1, JSFunction::kCodeOffset));
    __ Add(x5, x5, Code::kHeaderSize - kHeapObjectTag);
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
    __ Mov(scratch, Operand(ExternalReference(IsolateAddressId::kContextAddress,
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
                               ? BUILTIN_CODE(masm->isolate(), Construct)
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

static void ReplaceClosureCodeWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  Register native_context = scratch1;

  // Store code entry in the closure.
  __ Str(optimized_code, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ Mov(scratch1, optimized_code);  // Write barrier clobbers scratch1 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, scratch1, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

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

// Tail-call |function_id| if |smi_entry| == |marker|
static void TailCallRuntimeIfMarkerEquals(MacroAssembler* masm,
                                          Register smi_entry,
                                          OptimizationMarker marker,
                                          Runtime::FunctionId function_id) {
  Label no_match;
  __ CompareAndBranch(smi_entry, Operand(Smi::FromEnum(marker)), ne, &no_match);
  GenerateTailCallToReturnedCode(masm, function_id);
  __ bind(&no_match);
}

static void MaybeTailCallOptimizedCodeSlot(MacroAssembler* masm,
                                           Register feedback_vector,
                                           Register scratch1, Register scratch2,
                                           Register scratch3) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee if needed, and caller)
  //  -- x3 : new target (preserved for callee if needed, and caller)
  //  -- x1 : target function (preserved for callee if needed, and caller)
  //  -- feedback vector (preserved for caller if needed)
  // -----------------------------------
  DCHECK(
      !AreAliased(feedback_vector, x0, x1, x3, scratch1, scratch2, scratch3));

  Label optimized_code_slot_is_cell, fallthrough;

  Register closure = x1;
  Register optimized_code_entry = scratch1;

  __ Ldr(
      optimized_code_entry,
      FieldMemOperand(feedback_vector, FeedbackVector::kOptimizedCodeOffset));

  // Check if the code entry is a Smi. If yes, we interpret it as an
  // optimisation marker. Otherwise, interpret is as a weak cell to a code
  // object.
  __ JumpIfNotSmi(optimized_code_entry, &optimized_code_slot_is_cell);

  {
    // Optimized code slot is a Smi optimization marker.

    // Fall through if no optimization trigger.
    __ CompareAndBranch(optimized_code_entry,
                        Operand(Smi::FromEnum(OptimizationMarker::kNone)), eq,
                        &fallthrough);

    TailCallRuntimeIfMarkerEquals(masm, optimized_code_entry,
                                  OptimizationMarker::kCompileOptimized,
                                  Runtime::kCompileOptimized_NotConcurrent);
    TailCallRuntimeIfMarkerEquals(
        masm, optimized_code_entry,
        OptimizationMarker::kCompileOptimizedConcurrent,
        Runtime::kCompileOptimized_Concurrent);

    {
      // Otherwise, the marker is InOptimizationQueue, so fall through hoping
      // that an interrupt will eventually update the slot with optimized code.
      if (FLAG_debug_code) {
        __ Cmp(
            optimized_code_entry,
            Operand(Smi::FromEnum(OptimizationMarker::kInOptimizationQueue)));
        __ Assert(eq, kExpectedOptimizationSentinel);
      }
      __ B(&fallthrough);
    }
  }

  {
    // Optimized code slot is a WeakCell.
    __ bind(&optimized_code_slot_is_cell);

    __ Ldr(optimized_code_entry,
           FieldMemOperand(optimized_code_entry, WeakCell::kValueOffset));
    __ JumpIfSmi(optimized_code_entry, &fallthrough);

    // Check if the optimized code is marked for deopt. If it is, call the
    // runtime to clear it.
    Label found_deoptimized_code;
    __ Ldr(scratch2, FieldMemOperand(optimized_code_entry,
                                     Code::kKindSpecificFlags1Offset));
    __ TestAndBranchIfAnySet(scratch2, 1 << Code::kMarkedForDeoptimizationBit,
                             &found_deoptimized_code);

    // Optimized code is good, get it into the closure and link the closure into
    // the optimized functions list, then tail call the optimized code.
    // The feedback vector is no longer used, so re-use it as a scratch
    // register.
    ReplaceClosureCodeWithOptimizedCode(masm, optimized_code_entry, closure,
                                        scratch2, scratch3, feedback_vector);
    __ Add(optimized_code_entry, optimized_code_entry,
           Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(optimized_code_entry);

    // Optimized code slot contains deoptimized code, evict it and re-enter the
    // closure's code.
    __ bind(&found_deoptimized_code);
    GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
  }

  // Fall-through if the optimized code cell is clear and there is no
  // optimization marker.
  __ bind(&fallthrough);
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation.
static void AdvanceBytecodeOffset(MacroAssembler* masm, Register bytecode_array,
                                  Register bytecode_offset, Register bytecode,
                                  Register scratch1) {
  Register bytecode_size_table = scratch1;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode));

  __ Mov(
      bytecode_size_table,
      Operand(ExternalReference::bytecode_size_table_address(masm->isolate())));

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label load_size, extra_wide;
  STATIC_ASSERT(0 == static_cast<int>(interpreter::Bytecode::kWide));
  STATIC_ASSERT(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  __ Cmp(bytecode, Operand(0x1));
  __ B(hi, &load_size);
  __ B(eq, &extra_wide);

  // Load the next bytecode and update table to the wide scaled table.
  __ Add(bytecode_offset, bytecode_offset, Operand(1));
  __ Ldrb(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ Add(bytecode_size_table, bytecode_size_table,
         Operand(kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ B(&load_size);

  __ Bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ Add(bytecode_offset, bytecode_offset, Operand(1));
  __ Ldrb(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ Add(bytecode_size_table, bytecode_size_table,
         Operand(2 * kIntSize * interpreter::Bytecodes::kBytecodeCount));
  __ B(&load_size);

  // Load the size of the current bytecode.
  __ Bind(&load_size);
  __ Ldr(scratch1.W(), MemOperand(bytecode_size_table, bytecode, LSL, 2));
  __ Add(bytecode_offset, bytecode_offset, scratch1);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   - x1: the JS function object being called.
//   - x3: the incoming new target or generator object
//   - cp: our context.
//   - fp: our caller's frame pointer.
//   - jssp: stack pointer.
//   - lr: return address.
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  Register closure = x1;
  Register feedback_vector = x2;

  // Load the feedback vector from the closure.
  __ Ldr(feedback_vector,
         FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ Ldr(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));
  // Read off the optimized code slot in the feedback vector, and if there
  // is optimized code or an optimization marker, call that instead.
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, x7, x4, x5);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ Push(lr, fp, cp, closure);
  __ Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  Label maybe_load_debug_bytecode_array, bytecode_array_loaded;
  __ Ldr(x0, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(x0, SharedFunctionInfo::kFunctionDataOffset));
  __ Ldr(x11, FieldMemOperand(x0, SharedFunctionInfo::kDebugInfoOffset));
  __ JumpIfNotSmi(x11, &maybe_load_debug_bytecode_array);
  __ Bind(&bytecode_array_loaded);

  // Increment invocation count for the function.
  __ Ldr(x11, FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ Ldr(x11, FieldMemOperand(x11, Cell::kValueOffset));
  __ Ldr(w10, FieldMemOperand(x11, FeedbackVector::kInvocationCountOffset));
  __ Add(w10, w10, Operand(1));
  __ Str(w10, FieldMemOperand(x11, FeedbackVector::kInvocationCountOffset));

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

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(x0, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, x0);

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

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in x3.
  Label no_incoming_new_target_or_generator_register;
  __ Ldrsw(x10,
           FieldMemOperand(
               kInterpreterBytecodeArrayRegister,
               BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ Cbz(x10, &no_incoming_new_target_or_generator_register);
  __ Str(x3, MemOperand(fp, x10, LSL, kPointerSizeLog2));
  __ Bind(&no_incoming_new_target_or_generator_register);

  // Load accumulator with undefined.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x1, LSL, kPointerSizeLog2));
  __ Ldr(ip0, MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Call(ip0);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ Ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Check if we should return.
  Label do_return;
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ Cmp(x1, Operand(static_cast<int>(interpreter::Bytecode::kReturn)));
  __ B(&do_return, eq);

  // Advance to the next bytecode and dispatch.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, x1, x2);
  __ B(&do_dispatch);

  __ bind(&do_return);
  // The return value is in x0.
  LeaveInterpreterFrame(masm, x2);
  __ Ret();

  // Load debug copy of the bytecode array if it exists.
  // kInterpreterBytecodeArrayRegister is already loaded with
  // SharedFunctionInfo::kFunctionDataOffset.
  __ Bind(&maybe_load_debug_bytecode_array);
  __ Ldrsw(x10, UntagSmiFieldMemOperand(x11, DebugInfo::kFlagsOffset));
  __ TestAndBranchIfAllClear(x10, DebugInfo::kHasBreakInfo,
                             &bytecode_array_loaded);
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(x11, DebugInfo::kDebugBytecodeArrayOffset));
  __ B(&bytecode_array_loaded);
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
    InterpreterPushArgsMode mode) {
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

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(x2);         // Pass the spread in a register
    __ Sub(x0, x0, 1);  // Subtract one for spread
  }

  // Call the target.
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ Jump(
        masm->isolate()->builtins()->CallFunction(ConvertReceiverMode::kAny),
        RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
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

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Pop(x2);         // Pass the spread in a register
    __ Sub(x0, x0, 1);  // Subtract one for spread
  } else {
    __ AssertUndefinedOrAllocationSite(x2, x6);
  }

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
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with x0, x1, and x3 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

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
  __ LoadObject(x1, BUILTIN_CODE(masm->isolate(), InterpreterEntryTrampoline));
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
  // Get bytecode array and bytecode offset from the stack frame.
  __ ldr(kInterpreterBytecodeArrayRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ ldr(kInterpreterBytecodeOffsetRegister,
         MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Load the current bytecode.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  AdvanceBytecodeOffset(masm, kInterpreterBytecodeArrayRegister,
                        kInterpreterBytecodeOffsetRegister, x1, x2);

  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(x2, kInterpreterBytecodeOffsetRegister);
  __ Str(x2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CheckOptimizationMarker(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee)
  //  -- x3 : new target (preserved for callee)
  //  -- x1 : target function (preserved for callee)
  // -----------------------------------
  Register closure = x1;

  // Get the feedback vector.
  Register feedback_vector = x2;
  __ Ldr(feedback_vector,
         FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ Ldr(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));

  // The feedback vector must be defined.
  if (FLAG_debug_code) {
    __ CompareRoot(feedback_vector, Heap::kUndefinedValueRootIndex);
    __ Assert(ne, BailoutReason::kExpectedFeedbackVector);
  }

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, x7, x4, x5);

  // Otherwise, tail call the SFI code.
  GenerateTailCallToSharedCode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : argument count (preserved for callee)
  //  -- x3 : new target (preserved for callee)
  //  -- x1 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;

  Register closure = x1;
  Register feedback_vector = x2;

  // Do we have a valid feedback vector?
  __ Ldr(feedback_vector,
         FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ Ldr(feedback_vector, FieldMemOperand(feedback_vector, Cell::kValueOffset));
  __ JumpIfRoot(feedback_vector, Heap::kUndefinedValueRootIndex,
                &gotta_call_runtime);

  // Is there an optimization marker or optimized code in the feedback vector?
  MaybeTailCallOptimizedCodeSlot(masm, feedback_vector, x7, x4, x5);

  // We found no optimized code.
  Register entry = x7;
  __ Ldr(entry,
         FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));

  // If SFI points to anything other than CompileLazy, install that.
  __ Ldr(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(x5, masm->CodeObject());
  __ Cmp(entry, x5);
  __ B(eq, &gotta_call_runtime);

  // Install the SFI's code entry.
  __ Str(entry, FieldMemOperand(closure, JSFunction::kCodeOffset));
  __ Mov(x10, entry);  // Write barrier clobbers x10 below.
  __ RecordWriteField(closure, JSFunction::kCodeOffset, x10, x5,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Add(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(entry);

  __ Bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
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
  // On failure, tail call back to regular js by re-calling the function
  // which has be reset to the compile lazy builtin.
  __ Ldr(x4, FieldMemOperand(x1, JSFunction::kCodeOffset));
  __ Add(x4, x4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(x4);
}

void Builtins::Generate_NotifyBuiltinContinuation(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve possible return result from lazy deopt.
    __ Push(x0);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, false);
    __ Pop(x0);
  }

  // Ignore state (pushed by Deoptimizer::EntryGenerator::Generate).
  __ Drop(1);

  // Jump to the ContinueToBuiltin stub. Deoptimizer::EntryGenerator::Generate
  // loads this into lr before it jumps here.
  __ Br(lr);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  if (with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point.
    __ Str(x0, MemOperand(
                   jssp,
                   config->num_allocatable_general_registers() * kPointerSize +
                       BuiltinContinuationFrameConstants::kFixedFrameSize));
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ Pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }
  __ ldr(fp,
         MemOperand(jssp,
                    BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(ip0);
  __ Add(jssp, jssp,
         Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(lr);
  __ Add(ip0, ip0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Br(ip0);
}
}  // namespace

void Builtins::Generate_ContinueToCodeStubBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, false);
}

void Builtins::Generate_ContinueToCodeStubBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, false, true);
}

void Builtins::Generate_ContinueToJavaScriptBuiltin(MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, false);
}

void Builtins::Generate_ContinueToJavaScriptBuiltinWithResult(
    MacroAssembler* masm) {
  Generate_ContinueToBuiltinHelper(masm, true, true);
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
  Register arg_array = x2;
  Register receiver = x1;
  Register this_arg = x0;
  Register undefined_value = x3;
  Register null_value = x4;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);
  __ LoadRoot(null_value, Heap::kNullValueRootIndex);

  // 1. Load receiver into x1, argArray into x2 (if present), remove all
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
  //  -- x2      : argArray
  //  -- x1      : receiver
  //  -- jssp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ Cmp(arg_array, null_value);
  __ Ccmp(arg_array, undefined_value, ZFlag, ne);
  __ B(eq, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ Bind(&no_arguments);
  {
    __ Mov(x0, 0);
    DCHECK(receiver.Is(x1));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
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
  Register arguments_list = x2;
  Register target = x1;
  Register this_argument = x4;
  Register undefined_value = x3;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);

  // 1. Load target into x1 (if present), argumentsList into x2 (if present),
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
  //  -- x2      : argumentsList
  //  -- x1      : target
  //  -- jssp[0] : thisArgument
  // -----------------------------------

  // 2. We don't need to check explicitly for callable target here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Apply the target to the given argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);
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
  Register arguments_list = x2;
  Register target = x1;
  Register new_target = x3;
  Register undefined_value = x4;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);

  // 1. Load target into x1 (if present), argumentsList into x2 (if present),
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
  //  -- x2      : argumentsList
  //  -- x1      : target
  //  -- x3      : new.target
  //  -- jssp[0] : receiver (undefined)
  // -----------------------------------

  // 2. We don't need to check explicitly for constructor target here,
  // since that's the first thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 3. We don't need to check explicitly for constructor new.target here,
  // since that's the second thing the Construct/ConstructWithArrayLike
  // builtins will do.

  // 4. Construct the target with the given new.target and argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithArrayLike),
          RelocInfo::CODE_TARGET);
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ Push(lr, fp);
  __ Mov(x11, StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR));
  __ Push(x11, x1);  // x1: function
  // We do not yet push the number of arguments, to maintain a 16-byte aligned
  // stack pointer. This is done in step (3) in
  // Generate_ArgumentsAdaptorTrampoline.
  __ Add(fp, jssp, StandardFrameConstants::kFixedFrameSizeFromFp);
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

  // Drop actual parameters and receiver.
  // TODO(all): This will need to be rounded up to a multiple of two when using
  // the CSP, as we will have claimed an even number of slots in total for the
  // parameters.
  __ DropBySMI(x10, kXRegSize);
  __ Drop(1);
}

// static
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- x1 : target
  //  -- x0 : number of parameters on the stack (not including the receiver)
  //  -- x2 : arguments list (a FixedArray)
  //  -- x4 : len (number of elements to push from args)
  //  -- x3 : new.target (for [[Construct]])
  // -----------------------------------
  __ AssertFixedArray(x2);

  Register arguments_list = x2;
  Register argc = x0;
  Register len = x4;

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

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label done, push, loop;
    Register src = x5;

    __ Add(src, arguments_list, FixedArray::kHeaderSize - kHeapObjectTag);
    __ Add(argc, argc, len);  // The 'len' argument for Call() or Construct().
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

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x3 : the new.target (for [[Construct]] calls)
  //  -- x1 : the target to call (can be any Object)
  //  -- x2 : start index (to support rest parameters)
  // -----------------------------------

  // Check if new.target has a [[Construct]] internal method.
  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(x3, &new_target_not_constructor);
    __ Ldr(x5, FieldMemOperand(x3, HeapObject::kMapOffset));
    __ Ldrb(x5, FieldMemOperand(x5, Map::kBitFieldOffset));
    __ TestAndBranchIfAnySet(x5, 1 << Map::kIsConstructor,
                             &new_target_constructor);
    __ Bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(x3);
      __ CallRuntime(Runtime::kThrowNotConstructor);
    }
    __ Bind(&new_target_constructor);
  }

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

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
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
  __ TestAndBranchIfAnySet(w3, SharedFunctionInfo::kClassConstructorMask,
                           &class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ TestAndBranchIfAnySet(w3,
                           SharedFunctionInfo::IsNativeBit::kMask |
                               SharedFunctionInfo::IsStrictBit::kMask,
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
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
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

  __ Ldrsw(
      x2, FieldMemOperand(x2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(x0);
  ParameterCount expected(x2);
  __ InvokeFunctionCode(x1, no_reg, expected, actual, JUMP_FUNCTION);

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
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(x1);

  // Patch the receiver to [[BoundThis]].
  __ Ldr(x10, FieldMemOperand(x1, JSBoundFunction::kBoundThisOffset));
  __ Poke(x10, Operand(x0, LSL, kPointerSizeLog2));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ Ldr(x1, FieldMemOperand(x1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(x1, &non_callable);
  __ Bind(&non_smi);
  __ CompareObjectType(x1, x4, x5, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, eq);
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ Ldrb(x4, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x4, 1 << Map::kIsCallable, &non_callable);

  // Check if target is a proxy and call CallProxy external builtin
  __ Cmp(x5, JS_PROXY_TYPE);
  __ B(ne, &non_function);
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ Bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ Poke(x1, Operand(x0, LSL, kXRegSizeLog2));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, x1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
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
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
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
  Label non_constructor, non_proxy;
  __ JumpIfSmi(x1, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(x1, x4, x5, JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ Ldrb(x2, FieldMemOperand(x4, Map::kBitFieldOffset));
  __ TestAndBranchIfAllClear(x2, 1 << Map::kIsConstructor, &non_constructor);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Cmp(x5, JS_BOUND_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ Cmp(x5, JS_PROXY_TYPE);
  __ B(ne, &non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
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
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
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

// static
void Builtins::Generate_AbortJS(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_AbortJS");
  // ----------- S t a t e -------------
  //  -- x1 : message as String object
  //  -- lr : return address
  // -----------------------------------
  MacroAssembler::NoUseRealAbortsScope no_use_real_aborts(masm);
  __ Push(x1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbortJS);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_ArgumentsAdaptorTrampoline");
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  //  -- x3 : new target (passed through to callee)
  // -----------------------------------

  // The frame we are about to construct will look like:
  //
  //  slot      Adaptor frame
  //       +-----------------+--------------------------------
  //  -n-1 |    receiver     |                            ^
  //       |  (parameter 0)  |                            |
  //       |- - - - - - - - -|                            |
  //  -n   |                 |                          Caller
  //  ...  |       ...       |                       frame slots --> actual args
  //  -2   |  parameter n-1  |                            |
  //       |- - - - - - - - -|                            |
  //  -1   |   parameter n   |                            v
  //  -----+-----------------+--------------------------------
  //   0   |   return addr   |                            ^
  //       |- - - - - - - - -|                            |
  //   1   | saved frame ptr | <-- frame ptr              |
  //       |- - - - - - - - -|                            |
  //   2   |Frame Type Marker|                            |
  //       |- - - - - - - - -|                            |
  //   3   |    function     |                          Callee
  //       |- - - - - - - - -|                        frame slots
  //   4   |     num of      |                            |
  //       |   actual args   |                            |
  //       |- - - - - - - - -|                            |
  //  [5]  |    [padding]    |                            |
  //       |-----------------+----                        |
  // 5+pad |    receiver     |   ^                        |
  //       |  (parameter 0)  |   |                        |
  //       |- - - - - - - - -|   |                        |
  // 6+pad |   parameter 1   |   |                        |
  //       |- - - - - - - - -| Frame slots ----> expected args
  // 7+pad |   parameter 2   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |                 |   |                        |
  //  ...  |       ...       |   |                        |
  //       |   parameter m   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |   [undefined]   |   |                        |
  //       |- - - - - - - - -|   |                        |
  //       |                 |   |                        |
  //       |       ...       |   |                        |
  //       |   [undefined]   |   v   <-- stack ptr        v
  //  -----+-----------------+---------------------------------
  //
  // There is an optional slot of padding to ensure stack alignment.
  // If the number of expected arguments is larger than the number of actual
  // arguments, the remaining expected slots will be filled with undefined.

  Register argc_actual = x0;    // Excluding the receiver.
  Register argc_expected = x2;  // Excluding the receiver.
  Register function = x1;
  Register code_entry = x10;

  Label dont_adapt_arguments, stack_overflow;

  Label enough_arguments;
  __ Cmp(argc_expected, SharedFunctionInfo::kDontAdaptArgumentsSentinel);
  __ B(eq, &dont_adapt_arguments);

  EnterArgumentsAdaptorFrame(masm);

  Register copy_from = x10;
  Register copy_end = x11;
  Register copy_to = x12;
  Register argc_to_copy = x13;
  Register argc_unused_actual = x14;
  Register scratch1 = x15, scratch2 = x16;

  // We need slots for the expected arguments, with two extra slots for the
  // number of actual arguments and the receiver.
  __ RecordComment("-- Stack check --");
  __ Add(scratch1, argc_expected, 2);
  Generate_StackOverflowCheck(masm, scratch1, scratch2, &stack_overflow);

  // Round up number of slots to be even, to maintain stack alignment.
  __ RecordComment("-- Allocate callee frame slots --");
  __ Add(scratch1, scratch1, 1);
  __ Bic(scratch1, scratch1, 1);
  __ Claim(scratch1, kPointerSize);

  __ Mov(copy_to, jssp);

  // Preparing the expected arguments is done in four steps, the order of
  // which is chosen so we can use LDP/STP and avoid conditional branches as
  // much as possible.

  // (1) If we don't have enough arguments, fill the remaining expected
  // arguments with undefined, otherwise skip this step.
  __ Subs(scratch1, argc_actual, argc_expected);
  __ Csel(argc_unused_actual, xzr, scratch1, lt);
  __ Csel(argc_to_copy, argc_expected, argc_actual, ge);
  __ B(ge, &enough_arguments);

  // Fill the remaining expected arguments with undefined.
  __ RecordComment("-- Fill slots with undefined --");
  __ Sub(copy_end, copy_to, Operand(scratch1, LSL, kPointerSizeLog2));
  __ LoadRoot(scratch1, Heap::kUndefinedValueRootIndex);

  Label fill;
  __ Bind(&fill);
  __ Stp(scratch1, scratch1, MemOperand(copy_to, 2 * kPointerSize, PostIndex));
  // We might write one slot extra, but that is ok because we'll overwrite it
  // below.
  __ Cmp(copy_end, copy_to);
  __ B(hi, &fill);

  // Correct copy_to, for the case where we wrote one additional slot.
  __ Mov(copy_to, copy_end);

  __ Bind(&enough_arguments);
  // (2) Copy all of the actual arguments, or as many as we need.
  __ RecordComment("-- Copy actual arguments --");
  __ Add(copy_end, copy_to, Operand(argc_to_copy, LSL, kPointerSizeLog2));
  __ Add(copy_from, fp, 2 * kPointerSize);
  // Adjust for difference between actual and expected arguments.
  __ Add(copy_from, copy_from,
         Operand(argc_unused_actual, LSL, kPointerSizeLog2));

  // Copy arguments. We use load/store pair instructions, so we might overshoot
  // by one slot, but since we copy the arguments starting from the last one, if
  // we do overshoot, the extra slot will be overwritten later by the receiver.
  Label copy_2_by_2;
  __ Bind(&copy_2_by_2);
  __ Ldp(scratch1, scratch2,
         MemOperand(copy_from, 2 * kPointerSize, PostIndex));
  __ Stp(scratch1, scratch2, MemOperand(copy_to, 2 * kPointerSize, PostIndex));
  __ Cmp(copy_end, copy_to);
  __ B(hi, &copy_2_by_2);

  // (3) Store number of actual arguments and padding. The padding might be
  // unnecessary, in which case it will be overwritten by the receiver.
  __ RecordComment("-- Store number of args and padding --");
  __ SmiTag(scratch1, argc_actual);
  __ Stp(xzr, scratch1, MemOperand(fp, -4 * kPointerSize));

  // (4) Store receiver. Calculate target address from jssp to avoid checking
  // for padding. Storing the receiver will overwrite either the extra slot
  // we copied with the actual arguments, if we did copy one, or the padding we
  // stored above.
  __ RecordComment("-- Store receiver --");
  __ Add(copy_from, fp, 2 * kPointerSize);
  __ Ldr(scratch1, MemOperand(copy_from, argc_actual, LSL, kPointerSizeLog2));
  __ Str(scratch1, MemOperand(jssp, argc_expected, LSL, kPointerSizeLog2));

  // Arguments have been adapted. Now call the entry point.
  __ RecordComment("-- Call entry point --");
  __ Mov(argc_actual, argc_expected);
  // x0 : expected number of arguments
  // x1 : function (passed through to callee)
  // x3 : new target (passed through to callee)
  __ Ldr(code_entry, FieldMemOperand(function, JSFunction::kCodeOffset));
  __ Add(code_entry, code_entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(code_entry);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // Call the entry point without adapting the arguments.
  __ RecordComment("-- Call without adapting args --");
  __ Bind(&dont_adapt_arguments);
  __ Ldr(code_entry, FieldMemOperand(function, JSFunction::kCodeOffset));
  __ Add(code_entry, code_entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(code_entry);

  __ Bind(&stack_overflow);
  __ RecordComment("-- Stack overflow --");
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
    const RegList gp_regs = x0.bit() | x1.bit() | x2.bit() | x3.bit() |
                            x4.bit() | x5.bit() | x6.bit() | x7.bit();
    const RegList fp_regs = d0.bit() | d1.bit() | d2.bit() | d3.bit() |
                            d4.bit() | d5.bit() | d6.bit() | d7.bit();
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

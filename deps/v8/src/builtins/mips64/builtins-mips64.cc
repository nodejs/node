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

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address,
                                ExitFrameType exit_frame_type) {
  // ----------- S t a t e -------------
  //  -- a0                 : number of arguments excluding receiver
  //  -- a1                 : target
  //  -- a3                 : new.target
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
  __ Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // JumpToExternalReference expects a0 to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ Daddu(a0, a0, num_extra_args + 1);

  // Insert extra arguments.
  __ SmiTag(a0);
  __ Push(a0, a1, a3);
  __ SmiUntag(a0);

  __ JumpToExternalReference(ExternalReference(address, masm->isolate()),
                             PROTECT, exit_frame_type == BUILTIN_EXIT);
}

// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the InternalArray function from the native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}

// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the Array function from the native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
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
    __ Ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, a4);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction, a4,
              Operand(zero_reg));
    __ GetObjectType(a2, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction, a4,
              Operand(MAP_TYPE));
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
    __ Ld(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, a4);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction1, a4,
              Operand(zero_reg));
    __ GetObjectType(a2, a3, a4);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction2, a4,
              Operand(MAP_TYPE));
  }

  // Run the native code for the Array function called as a normal function.
  // Tail call a stub.
  __ mov(a3, a1);
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(t1, a0, Operand(1));  // In delay slot.
    __ mov(t0, a0);                // Store argc in t0.
    __ Dlsa(at, sp, t1, kPointerSizeLog2);
    __ Ld(a0, MemOperand(at));
  }

  // 2a. Convert first argument to number.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(t0);
    __ EnterBuiltinFrame(cp, a1, t0);
    __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, a1, t0);
    __ SmiUntag(t0);
  }

  {
    // Drop all arguments including the receiver.
    __ Dlsa(sp, sp, t0, kPointerSizeLog2);
    __ DropAndRet(1);
  }

  // 2b. No arguments, return +0.
  __ bind(&no_arguments);
  __ Move(v0, Smi::kZero);
  __ DropAndRet(1);
}

void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- a3                     : new target
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // 2. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ mov(t0, a0);  // Store argc in t0.
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));  // In delay slot.
    __ Dlsa(at, sp, a0, kPointerSizeLog2);
    __ Ld(a0, MemOperand(at));
    __ jmp(&done);
    __ bind(&no_arguments);
    __ Move(a0, Smi::kZero);
    __ bind(&done);
  }

  // 3. Make sure a0 is a number.
  {
    Label done_convert;
    __ JumpIfSmi(a0, &done_convert);
    __ GetObjectType(a0, a2, a2);
    __ Branch(&done_convert, eq, a2, Operand(HEAP_NUMBER_TYPE));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(t0);
      __ EnterBuiltinFrame(cp, a1, t0);
      __ Push(a3);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ Move(a0, v0);
      __ Pop(a3);
      __ LeaveBuiltinFrame(cp, a1, t0);
      __ SmiUntag(t0);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ Branch(&new_object, ne, a1, Operand(a3));

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(v0, a1, a0, a2, t1, &new_object);
  __ jmp(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(t0);
    __ EnterBuiltinFrame(cp, a1, t0);
    __ Push(a0);
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(a0);
    __ LeaveBuiltinFrame(cp, a1, t0);
    __ SmiUntag(t0);
  }
  __ Sd(a0, FieldMemOperand(v0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Dlsa(sp, sp, t0, kPointerSizeLog2);
    __ DropAndRet(1);
  }
}

// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(t1, a0, Operand(1));  // In delay slot.
    __ mov(t0, a0);                // Store argc in t0.
    __ Dlsa(at, sp, t1, kPointerSizeLog2);
    __ Ld(a0, MemOperand(at));
  }

  // 2a. At least one argument, return a0 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label drop_frame_and_ret, to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(a0, &to_string);
    __ GetObjectType(a0, t1, t1);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ Subu(t1, t1, Operand(FIRST_NONSTRING_TYPE));
    __ Branch(&symbol_descriptive_string, eq, t1, Operand(zero_reg));
    __ Branch(&to_string, gt, t1, Operand(zero_reg));
    __ mov(v0, a0);
    __ jmp(&drop_frame_and_ret);
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
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(t0);
    __ EnterBuiltinFrame(cp, a1, t0);
    __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, a1, t0);
    __ SmiUntag(t0);
  }
  __ jmp(&drop_frame_and_ret);

  // 3b. Convert symbol in a0 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Dlsa(sp, sp, t0, kPointerSizeLog2);
    __ Drop(1);
    __ Push(a0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    __ Dlsa(sp, sp, t0, kPointerSizeLog2);
    __ DropAndRet(1);
  }
}

void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- a3                     : new target
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // 2. Load the first argument into a0 and get rid of the rest (including the
  // receiver).
  {
    Label no_arguments, done;
    __ mov(t0, a0);  // Store argc in t0.
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Dsubu(a0, a0, Operand(1));
    __ Dlsa(at, sp, a0, kPointerSizeLog2);
    __ Ld(a0, MemOperand(at));
    __ jmp(&done);
    __ bind(&no_arguments);
    __ LoadRoot(a0, Heap::kempty_stringRootIndex);
    __ bind(&done);
  }

  // 3. Make sure a0 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(a0, &convert);
    __ GetObjectType(a0, a2, a2);
    __ And(t1, a2, Operand(kIsNotStringMask));
    __ Branch(&done_convert, eq, t1, Operand(zero_reg));
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(t0);
      __ EnterBuiltinFrame(cp, a1, t0);
      __ Push(a3);
      __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
      __ Move(a0, v0);
      __ Pop(a3);
      __ LeaveBuiltinFrame(cp, a1, t0);
      __ SmiUntag(t0);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ Branch(&new_object, ne, a1, Operand(a3));

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(v0, a1, a0, a2, t1, &new_object);
  __ jmp(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(t0);
    __ EnterBuiltinFrame(cp, a1, t0);
    __ Push(a0);
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(a0);
    __ LeaveBuiltinFrame(cp, a1, t0);
    __ SmiUntag(t0);
  }
  __ Sd(a0, FieldMemOperand(v0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Dlsa(sp, sp, t0, kPointerSizeLog2);
    __ DropAndRet(1);
  }
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ Ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Ld(a2, FieldMemOperand(a2, SharedFunctionInfo::kCodeOffset));
  __ Daddu(at, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the function onto the stack.
    // Push a copy of the target function and the new target.
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    __ CallRuntime(function_id, 1);
    // Restore target function and new target.
    __ Pop(a0, a1, a3);
    __ SmiUntag(a0);
  }

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

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a3     : new target
  //  -- cp     : context
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0);
    __ SmiUntag(a0);

    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);

    // Set up pointer to last argument.
    __ Daddu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(t3, a0);
    // ----------- S t a t e -------------
    //  --                        a0: number of arguments (untagged)
    //  --                        a3: new target
    //  --                        t2: pointer to last argument
    //  --                        t3: counter
    //  --        sp[0*kPointerSize]: the hole (receiver)
    //  --        sp[1*kPointerSize]: number of arguments (tagged)
    //  --        sp[2*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Dlsa(t0, t2, t3, kPointerSizeLog2);
    __ Ld(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Daddu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments (untagged)
    // a1: constructor function
    // a3: new target
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Restore context from the frame.
    __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ Ld(a1, MemOperand(sp));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  __ Daddu(sp, sp, kPointerSize);
  __ Ret();
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  --      a0: number of arguments (untagged)
  //  --      a1: constructor function
  //  --      a3: new target
  //  --      cp: context
  //  --      ra: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0, a1, a3);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  -- a1 and sp[1*kPointerSize]: constructor function
    //  --        sp[2*kPointerSize]: number of arguments (tagged)
    //  --        sp[3*kPointerSize]: context
    // -----------------------------------

    __ Ld(t2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lbu(t2,
           FieldMemOperand(t2, SharedFunctionInfo::kFunctionKindByteOffset));
    __ And(t2, t2,
           Operand(SharedFunctionInfo::kDerivedConstructorBitsWithinByte));
    __ Branch(&not_create_implicit_receiver, ne, t2, Operand(zero_reg));

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        t2, t3);
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Branch(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(v0, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                          v0: receiver
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
    __ Pop(a3);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(v0, v0);

    // ----------- S t a t e -------------
    //  --                 r3: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments (tagged)
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ Ld(a1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ Ld(a0, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(a0);

    // Set up pointer to last argument.
    __ Daddu(t2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ mov(t3, a0);
    // ----------- S t a t e -------------
    //  --                        a0: number of arguments (untagged)
    //  --                        a3: new target
    //  --                        t2: pointer to last argument
    //  --                        t3: counter
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  -- a1 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------
    __ jmp(&entry);
    __ bind(&loop);
    __ Dlsa(t0, t2, t3, kPointerSizeLog2);
    __ Ld(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Daddu(t3, t3, Operand(-1));
    __ Branch(&loop, greater_equal, t3, Operand(zero_reg));

    // Call the function.
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // ----------- S t a t e -------------
    //  --                 v0: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: constructor function
    //  -- sp[2*kPointerSize]: number of arguments
    //  -- sp[3*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ Ld(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(v0, Heap::kUndefinedValueRootIndex, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(v0, &other_result);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    __ GetObjectType(v0, t2, t2);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ Branch(&leave_frame, greater_equal, t2, Operand(FIRST_JS_RECEIVER_TYPE));

    __ bind(&other_result);
    // The result is now neither undefined nor an object.
    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ Ld(a1, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
      __ Ld(t2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ lbu(t2,
             FieldMemOperand(t2, SharedFunctionInfo::kFunctionKindByteOffset));
      __ And(t2, t2,
             Operand(SharedFunctionInfo::kClassConstructorBitsWithinByte));
      __ Branch(&use_receiver, eq, t2, Operand(zero_reg));
    } else {
      __ Branch(&use_receiver);
    }

    __ bind(&do_throw);
    __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ Ld(v0, MemOperand(sp, 0 * kPointerSize));
    __ JumpIfRoot(v0, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ Ld(a1, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  __ Daddu(sp, sp, kPointerSize);
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

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- a2 : the resume mode (tagged)
  //  -- a3 : the SuspendFlags of the earlier suspend call (tagged)
  //  -- ra : return address
  // -----------------------------------
  __ SmiUntag(a3);
  __ AssertGeneratorObject(a1, a3);

  // Store input value into generator object.
  Label async_await, done_store_input;

  __ And(t8, a3, Operand(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ Branch(&async_await, equal, t8,
            Operand(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));

  __ Sd(v0, FieldMemOperand(a1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOrDebugPosOffset, v0, a3,
                      kRAHasNotBeenSaved, kDontSaveFPRegs);
  __ jmp(&done_store_input);

  __ bind(&async_await);
  __ Sd(v0, FieldMemOperand(
                a1, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset,
                      v0, a3, kRAHasNotBeenSaved, kDontSaveFPRegs);

  __ bind(&done_store_input);
  // `a3` no longer holds SuspendFlags

  // Store resume mode into generator object.
  __ Sd(a2, FieldMemOperand(a1, JSGeneratorObject::kResumeModeOffset));

  // Load suspended function and context.
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ Ld(cp, FieldMemOperand(a4, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ li(a5, Operand(debug_hook));
  __ Lb(a5, MemOperand(a5));
  __ Branch(&prepare_step_in_if_stepping, ne, a5, Operand(zero_reg));

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ li(a5, Operand(debug_suspended_generator));
  __ Ld(a5, MemOperand(a5));
  __ Branch(&prepare_step_in_suspended_generator, eq, a1, Operand(a5));
  __ bind(&stepping_prepared);

  // Push receiver.
  __ Ld(a5, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
  __ Push(a5);

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- a2    : the resume mode (tagged)
  //  -- a4    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ Ld(a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
  __ Lw(a3,
        FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ Dsubu(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ Ld(a3, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ Ld(a3, FieldMemOperand(a3, SharedFunctionInfo::kFunctionDataOffset));
    __ GetObjectType(a3, a3, a3);
    __ Assert(eq, kMissingBytecodeArray, a3, Operand(BYTECODE_ARRAY_TYPE));
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ Ld(a0, FieldMemOperand(a4, JSFunction::kSharedFunctionInfoOffset));
    __ Lw(a0,
          FieldMemOperand(a0, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(a3, a1);
    __ Move(a1, a4);
    __ Ld(a2, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
    __ Jump(a2);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a2, a4);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(a1, a2);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a2);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(a1, a2);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ Ld(a4, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  __ Push(a1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
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
  __ CallRuntime(Runtime::kThrowStackOverflow);

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

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ li(cp, Operand(context_address));
    __ Ld(cp, MemOperand(cp));

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
    __ Dlsa(a6, s0, a3, kPointerSizeLog2);
    __ b(&entry);
    __ nop();  // Branch delay slot nop.
    // a6 points past last arg.
    __ bind(&loop);
    __ Ld(a4, MemOperand(s0));  // Read next parameter.
    __ daddiu(s0, s0, kPointerSize);
    __ Ld(a4, MemOperand(a4));  // Dereference handle.
    __ push(a4);                // Push parameter.
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

static void ReplaceClosureEntryWithOptimizedCode(
    MacroAssembler* masm, Register optimized_code_entry, Register closure,
    Register scratch1, Register scratch2, Register scratch3) {
  Register native_context = scratch1;

  // Store code entry in the closure.
  __ Daddu(optimized_code_entry, optimized_code_entry,
           Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Sd(optimized_code_entry,
        FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, optimized_code_entry, scratch2);

  // Link the closure into the optimized function list.
  __ Ld(native_context, NativeContextMemOperand());
  __ Ld(scratch2,
        ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ Sd(scratch2,
        FieldMemOperand(closure, JSFunction::kNextFunctionLinkOffset));
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, scratch2,
                      scratch3, kRAHasNotBeenSaved, kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ Sd(closure,
        ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  // Save closure before the write barrier.
  __ mov(scratch2, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, closure,
                            scratch3, kRAHasNotBeenSaved, kDontSaveFPRegs);
  __ mov(closure, scratch2);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ Ld(args_count,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Lw(t0, FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments.
  __ Daddu(sp, sp, args_count);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o a1: the JS function object being called.
//   o a3: the new target
//   o cp: our context
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o ra: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(a1);

  // First check if there is optimized code in the feedback vector which we
  // could call instead.
  Label switch_to_optimized_code;
  Register optimized_code_entry = a4;
  __ Ld(a0, FieldMemOperand(a1, JSFunction::kFeedbackVectorOffset));
  __ Ld(a0, FieldMemOperand(a0, Cell::kValueOffset));
  __ Ld(optimized_code_entry,
        FieldMemOperand(a0, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                                FeedbackVector::kHeaderSize));
  __ Ld(optimized_code_entry,
        FieldMemOperand(optimized_code_entry, WeakCell::kValueOffset));
  __ JumpIfNotSmi(optimized_code_entry, &switch_to_optimized_code);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ Ld(a0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  Label load_debug_bytecode_array, bytecode_array_loaded;
  Register debug_info = kInterpreterBytecodeArrayRegister;
  DCHECK(!debug_info.is(a0));
  __ Ld(debug_info, FieldMemOperand(a0, SharedFunctionInfo::kDebugInfoOffset));
  __ JumpIfNotSmi(debug_info, &load_debug_bytecode_array);
  __ Ld(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(a0, SharedFunctionInfo::kFunctionDataOffset));
  __ bind(&bytecode_array_loaded);

  // Check whether we should continue to use the interpreter.
  // TODO(rmcilroy) Remove self healing once liveedit only has to deal with
  // Ignition bytecode.
  Label switch_to_different_code_kind;
  __ Ld(a0, FieldMemOperand(a0, SharedFunctionInfo::kCodeOffset));
  __ Branch(&switch_to_different_code_kind, ne, a0,
            Operand(masm->CodeObject()));  // Self-reference to this code.

  // Increment invocation count for the function.
  __ Ld(a0, FieldMemOperand(a1, JSFunction::kFeedbackVectorOffset));
  __ Ld(a0, FieldMemOperand(a0, Cell::kValueOffset));
  __ Ld(a4, FieldMemOperand(
                a0, FeedbackVector::kInvocationCountIndex * kPointerSize +
                        FeedbackVector::kHeaderSize));
  __ Daddu(a4, a4, Operand(Smi::FromInt(1)));
  __ Sd(a4, FieldMemOperand(
                a0, FeedbackVector::kInvocationCountIndex * kPointerSize +
                        FeedbackVector::kHeaderSize));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ SmiTst(kInterpreterBytecodeArrayRegister, a4);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a4,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a4, a4);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a4,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Reset code age.
  DCHECK_EQ(0, BytecodeArray::kNoAgeBytecodeAge);
  __ sb(zero_reg, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                  BytecodeArray::kBytecodeAgeOffset));

  // Load initial bytecode offset.
  __ li(kInterpreterBytecodeOffsetRegister,
        Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push new.target, bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(a4, kInterpreterBytecodeOffsetRegister);
  __ Push(a3, kInterpreterBytecodeArrayRegister, a4);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ Lw(a4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ Dsubu(a5, sp, Operand(a4));
    __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
    __ Branch(&ok, hs, a5, Operand(a2));
    __ CallRuntime(Runtime::kThrowStackOverflow);
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

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ Daddu(a0, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a0, MemOperand(a0));
  __ Dlsa(at, kInterpreterDispatchTableRegister, a0, kPointerSizeLog2);
  __ Ld(at, MemOperand(at));
  __ Call(at);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in v0.
  LeaveInterpreterFrame(masm, t0);
  __ Jump(ra);

  // Load debug copy of the bytecode array.
  __ bind(&load_debug_bytecode_array);
  __ Ld(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex));
  __ Branch(&bytecode_array_loaded);

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  __ Ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kCodeOffset));
  __ Daddu(a4, a4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Sd(a4, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(a1, a4, a5);
  __ Jump(a4);

  // If there is optimized code on the type feedback vector, check if it is good
  // to run, and if so, self heal the closure and call the optimized code.
  __ bind(&switch_to_optimized_code);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  Label gotta_call_runtime;

  // Check if the optimized code is marked for deopt.
  __ Lw(a5,
        FieldMemOperand(optimized_code_entry, Code::kKindSpecificFlags1Offset));
  __ And(a5, a5, Operand(1 << Code::kMarkedForDeoptimizationBit));
  __ Branch(&gotta_call_runtime, ne, a5, Operand(zero_reg));

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  ReplaceClosureEntryWithOptimizedCode(masm, optimized_code_entry, a1, t3, a5,
                                       t0);
  __ Jump(optimized_code_entry);

  // Optimized code is marked for deopt, bailout to the CompileLazy runtime
  // function which will clear the feedback vector's optimized code slot.
  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch1, Register scratch2,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(scratch1, Heap::kRealStackLimitRootIndex);
  // Make scratch1 the space we have left. The stack might already be overflowed
  // here which will cause scratch1 to become negative.
  __ dsubu(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ dsll(scratch2, num_args, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(stack_overflow, le, scratch1, Operand(scratch2));
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register scratch, Register scratch2) {
  // Find the address of the last argument.
  __ mov(scratch2, num_args);
  __ dsll(scratch2, scratch2, kPointerSizeLog2);
  __ Dsubu(scratch2, index, Operand(scratch2));

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ Ld(scratch, MemOperand(index));
  __ Daddu(index, index, Operand(-kPointerSize));
  __ push(scratch);
  __ bind(&loop_check);
  __ Branch(&loop_header, gt, index, Operand(scratch2));
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    TailCallMode tail_call_mode, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  __ Daddu(a3, a0, Operand(1));  // Add one for receiver.

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ Dsubu(a3, a3, Operand(1));  // Subtract one for receiver.
  }

  Generate_StackOverflowCheck(masm, a3, a4, t0, &stack_overflow);

  // This function modifies a2, t0 and a4.
  Generate_InterpreterPushArgs(masm, a3, a2, a4, t0);

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
    // Unreachable code.
    __ break_(0xCC);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- a0 : argument count (not including receiver)
  // -- a3 : new target
  // -- a1 : constructor to call
  // -- a2 : allocation site feedback if available, undefined otherwise.
  // -- a4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver.
  __ push(zero_reg);

  Generate_StackOverflowCheck(masm, a0, a5, t0, &stack_overflow);

  // This function modifies t0, a4 and a5.
  Generate_InterpreterPushArgs(masm, a0, a4, a5, t0);

  __ AssertUndefinedOrAllocationSite(a2, t0);
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ AssertFunction(a1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ Ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ Ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kConstructStubOffset));
    __ Daddu(at, a4, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(at);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(masm->isolate()->builtins()->ConstructWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with a0, a1, and a3 unmodified.
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructArray(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call checked to be Array function.
  //  -- a2 : allocation site feedback.
  //  -- a3 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver.
  __ push(zero_reg);

  Generate_StackOverflowCheck(masm, a0, a5, a6, &stack_overflow);

  // This function modifies a3, a5 and a6.
  Generate_InterpreterPushArgs(masm, a0, a3, a5, a6);

  // ArrayConstructor stub expects constructor in a3. Set it here.
  __ mov(a3, a1);

  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ break_(0xCC);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ li(t0, Operand(masm->isolate()->builtins()->InterpreterEntryTrampoline()));
  __ Daddu(ra, t0, Operand(interpreter_entry_return_pc_offset->value() +
                           Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ Ld(kInterpreterBytecodeArrayRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ SmiTst(kInterpreterBytecodeArrayRegister, at);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, at,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, a1, a1);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, a1,
              Operand(BYTECODE_ARRAY_TYPE));
  }

  // Get the target bytecode offset from the frame.
  __ Lw(
      kInterpreterBytecodeOffsetRegister,
      UntagSmiMemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  // Dispatch to the target bytecode.
  __ Daddu(a1, kInterpreterBytecodeArrayRegister,
           kInterpreterBytecodeOffsetRegister);
  __ Lbu(a1, MemOperand(a1));
  __ Dlsa(a1, kInterpreterDispatchTableRegister, a1, kPointerSizeLog2);
  __ Ld(a1, MemOperand(a1));
  __ Jump(a1);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ Ld(a1, MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ Ld(a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ Ld(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister, a1, a2);
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ mov(a2, v0);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ Sd(a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : argument count (preserved for callee)
  //  -- a3 : new target (preserved for callee)
  //  -- a1 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;
  Label try_shared;

  Register closure = a1;
  Register index = a2;

  // Do we have a valid feedback vector?
  __ Ld(index, FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ Ld(index, FieldMemOperand(index, Cell::kValueOffset));
  __ JumpIfRoot(index, Heap::kUndefinedValueRootIndex, &gotta_call_runtime);

  // Is optimized code available in the feedback vector?
  Register entry = a4;
  __ Ld(entry, FieldMemOperand(
                   index, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                              FeedbackVector::kHeaderSize));
  __ Ld(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found code, check if it is marked for deopt, if so call into runtime to
  // clear the optimized code slot.
  __ Lw(a5, FieldMemOperand(entry, Code::kKindSpecificFlags1Offset));
  __ And(a5, a5, Operand(1 << Code::kMarkedForDeoptimizationBit));
  __ Branch(&gotta_call_runtime, ne, a5, Operand(zero_reg));

  // Code is good, get it into the closure and tail call.
  ReplaceClosureEntryWithOptimizedCode(masm, entry, closure, t3, a5, t0);
  __ Jump(entry);

  // We found no optimized code.
  __ bind(&try_shared);
  __ Ld(entry, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ Lbu(a5, FieldMemOperand(entry,
                             SharedFunctionInfo::kMarkedForTierUpByteOffset));
  __ And(a5, a5,
         Operand(1 << SharedFunctionInfo::kMarkedForTierUpBitWithinByte));
  __ Branch(&gotta_call_runtime, ne, a5, Operand(zero_reg));

  // If SFI points to anything other than CompileLazy, install that.
  __ Ld(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(t1, masm->CodeObject());
  __ Branch(&gotta_call_runtime, eq, entry, Operand(t1));

  // Install the SFI's code entry.
  __ Daddu(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Sd(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, a5);
  __ Jump(entry);

  __ bind(&gotta_call_runtime);
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
  //  -- a0 : argument count (preserved for callee)
  //  -- a1 : new target (preserved for callee)
  //  -- a3 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ Move(t2, a0);
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ Branch(&over, ne, t2, Operand(j));
      }
      for (int i = j - 1; i >= 0; --i) {
        __ Ld(t2, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                     i * kPointerSize));
        __ push(t2);
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
    __ JumpIfSmi(v0, &failed);

    __ Drop(2);
    __ pop(t2);
    __ SmiUntag(t2);
    scope.GenerateLeaveFrame();

    __ Daddu(t2, t2, Operand(1));
    __ Dlsa(sp, sp, t2, kPointerSizeLog2);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(a0, a1, a3);
    __ SmiUntag(a0);
  }
  // On failure, tail call back to regular js.
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

static void GenerateMakeCodeYoungAgainCommon(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Set a0 to point to the head of the PlatformCodeAge sequence.
  __ Dsubu(a0, a0, Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   a0 - contains return address (beginning of patch sequence)
  //   a1 - isolate
  //   a3 - new target
  RegList saved_regs =
      (a0.bit() | a1.bit() | a3.bit() | ra.bit() | fp.bit()) & ~sp.bit();
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(saved_regs);
  __ PrepareCallCFunction(2, 0, a2);
  __ li(a1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ MultiPop(saved_regs);
  __ Jump(a0);
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

  // Set a0 to point to the head of the PlatformCodeAge sequence.
  __ Dsubu(a0, a0, Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   a0 - contains return address (beginning of patch sequence)
  //   a1 - isolate
  //   a3 - new target
  RegList saved_regs =
      (a0.bit() | a1.bit() | a3.bit() | ra.bit() | fp.bit()) & ~sp.bit();
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(saved_regs);
  __ PrepareCallCFunction(2, 0, a2);
  __ li(a1, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
      2);
  __ MultiPop(saved_regs);

  // Perform prologue operations usually performed by the young code stub.
  __ PushStandardFrame(a1);

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
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
    __ MultiPop(kJSCallerSaved | kCalleeSaved);
  }

  __ Daddu(sp, sp, Operand(kPointerSize));  // Ignore state
  __ Jump(ra);                              // Jump to miss handler
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
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Get the full codegen state from the stack and untag it -> a6.
  __ Lw(a6, UntagSmiMemOperand(sp, 0 * kPointerSize));
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ Branch(
      &with_tos_register, ne, a6,
      Operand(static_cast<int64_t>(Deoptimizer::BailoutState::NO_REGISTERS)));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Daddu(sp, sp, Operand(1 * kPointerSize));  // Remove state.

  __ bind(&with_tos_register);
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), v0.code());
  __ Ld(v0, MemOperand(sp, 1 * kPointerSize));
  __ Branch(
      &unknown_state, ne, a6,
      Operand(static_cast<int64_t>(Deoptimizer::BailoutState::TOS_REGISTER)));

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

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ Ld(a0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ Ld(a0, MemOperand(a0, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ Ld(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(a0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  __ Ret(eq, v0, Operand(Smi::kZero));

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ Ld(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ Lw(a1,
        UntagSmiMemOperand(a1, FixedArray::OffsetOfElementAt(
                                   DeoptimizationInputData::kOsrPcOffsetIndex) -
                                   kHeapObjectTag));

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ daddu(v0, v0, a1);
  __ daddiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

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
  //  -- a0    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  Register argc = a0;
  Register arg_array = a0;
  Register receiver = a1;
  Register this_arg = a2;
  Register undefined_value = a3;
  Register scratch = a4;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);
  // 1. Load receiver into a1, argArray into a0 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    // Claim (2 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Dsubu(sp, sp, Operand(2 * kPointerSize));
    __ Dlsa(sp, sp, argc, kPointerSizeLog2);
    __ mov(scratch, argc);
    __ Pop(this_arg, arg_array);                   // Overwrite argc
    __ Movz(arg_array, undefined_value, scratch);  // if argc == 0
    __ Movz(this_arg, undefined_value, scratch);   // if argc == 0
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arg_array, undefined_value, scratch);  // if argc == 1
    __ Ld(receiver, MemOperand(sp));
    __ Sd(this_arg, MemOperand(sp));
  }

  // ----------- S t a t e -------------
  //  -- a0    : argArray
  //  -- a1    : receiver
  //  -- a3    : undefined root value
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(receiver, &receiver_not_callable);
  __ Ld(a4, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsCallable));
  __ Branch(&receiver_not_callable, eq, a4, Operand(zero_reg));

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(arg_array, Heap::kNullValueRootIndex, &no_arguments);
  __ Branch(&no_arguments, eq, arg_array, Operand(undefined_value));

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  DCHECK(undefined_value.is(a3));
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(a0, zero_reg);
    DCHECK(receiver.is(a1));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    __ Sd(receiver, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
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
  __ Dlsa(at, sp, a0, kPointerSizeLog2);
  __ Ld(a1, MemOperand(at));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ Dlsa(a2, sp, a0, kPointerSizeLog2);

    __ bind(&loop);
    __ Ld(at, MemOperand(a2, -kPointerSize));
    __ Sd(at, MemOperand(a2));
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

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : argumentsList  (if argc ==3)
  //  -- sp[4]  : thisArgument   (if argc >=2)
  //  -- sp[8]  : target         (if argc >=1)
  //  -- sp[12] : receiver
  // -----------------------------------

  Register argc = a0;
  Register arguments_list = a0;
  Register target = a1;
  Register this_argument = a2;
  Register undefined_value = a3;
  Register scratch = a4;

  __ LoadRoot(undefined_value, Heap::kUndefinedValueRootIndex);
  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Dsubu(sp, sp, Operand(3 * kPointerSize));
    __ Dlsa(sp, sp, argc, kPointerSizeLog2);
    __ mov(scratch, argc);
    __ Pop(target, this_argument, arguments_list);
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 0
    __ Movz(this_argument, undefined_value, scratch);   // if argc == 0
    __ Movz(target, undefined_value, scratch);          // if argc == 0
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 1
    __ Movz(this_argument, undefined_value, scratch);   // if argc == 1
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 2

    __ Sd(this_argument, MemOperand(sp, 0));  // Overwrite receiver
  }

  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a1    : target
  //  -- a3    : undefined root value
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(target, &target_not_callable);
  __ Ld(a4, FieldMemOperand(target, HeapObject::kMapOffset));
  __ Lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsCallable));
  __ Branch(&target_not_callable, eq, a4, Operand(zero_reg));

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  DCHECK(undefined_value.is(a3));
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    __ Sd(target, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : new.target (optional) (dummy value if argc <= 2)
  //  -- sp[4]  : argumentsList         (dummy value if argc <= 1)
  //  -- sp[8]  : target                (dummy value if argc == 0)
  //  -- sp[12] : receiver
  // -----------------------------------
  Register argc = a0;
  Register arguments_list = a0;
  Register target = a1;
  Register new_target = a3;
  Register undefined_value = a4;
  Register scratch = a5;

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    // Claim (3 - argc) dummy arguments form the stack, to put the stack in a
    // consistent state for a simple pop operation.

    __ Dsubu(sp, sp, Operand(3 * kPointerSize));
    __ Dlsa(sp, sp, argc, kPointerSizeLog2);
    __ mov(scratch, argc);
    __ Pop(target, arguments_list, new_target);
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 0
    __ Movz(new_target, undefined_value, scratch);      // if argc == 0
    __ Movz(target, undefined_value, scratch);          // if argc == 0
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(arguments_list, undefined_value, scratch);  // if argc == 1
    __ Movz(new_target, target, scratch);               // if argc == 1
    __ Dsubu(scratch, scratch, Operand(1));
    __ Movz(new_target, target, scratch);  // if argc == 2

    __ Sd(undefined_value, MemOperand(sp, 0));  // Overwrite receiver
  }

  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a1    : target
  //  -- a3    : new.target
  //  -- sp[0] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(target, &target_not_constructor);
  __ Ld(a4, FieldMemOperand(target, HeapObject::kMapOffset));
  __ Lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsConstructor));
  __ Branch(&target_not_constructor, eq, a4, Operand(zero_reg));

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(new_target, &new_target_not_constructor);
  __ Ld(a4, FieldMemOperand(new_target, HeapObject::kMapOffset));
  __ Lbu(a4, FieldMemOperand(a4, Map::kBitFieldOffset));
  __ And(a4, a4, Operand(1 << Map::kIsConstructor));
  __ Branch(&new_target_not_constructor, eq, a4, Operand(zero_reg));

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    __ Sd(target, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    __ Sd(new_target, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  // __ sll(a0, a0, kSmiTagSize);
  __ dsll32(a0, a0, 0);
  __ li(a4, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | a4.bit() | fp.bit() | ra.bit());
  __ Daddu(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                           kPointerSize));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ Ld(a1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                             kPointerSize)));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ SmiScale(a4, a1, kPointerSizeLog2);
  __ Daddu(sp, sp, a4);
  // Adjust for the receiver.
  __ Daddu(sp, sp, Operand(kPointerSize));
}

// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a1    : target
  //  -- a3    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  Register arguments_list = a0;
  Register target = a1;
  Register new_target = a3;

  Register args = a0;
  Register len = a2;

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(arguments_list, &create_runtime);

    // Load the map of argumentsList into a2.
    Register arguments_list_map = a2;
    __ Ld(arguments_list_map,
          FieldMemOperand(arguments_list, HeapObject::kMapOffset));

    // Load native context into a4.
    Register native_context = a4;
    __ Ld(native_context, NativeContextMemOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ Ld(at, ContextMemOperand(native_context,
                                Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ Branch(&create_arguments, eq, arguments_list_map, Operand(at));
    __ Ld(at, ContextMemOperand(native_context,
                                Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ Branch(&create_arguments, eq, arguments_list_map, Operand(at));

    // Check if argumentsList is a fast JSArray.
    __ Lbu(v0, FieldMemOperand(a2, Map::kInstanceTypeOffset));
    __ Branch(&create_array, eq, v0, Operand(JS_ARRAY_TYPE));

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(target, new_target, arguments_list);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ mov(arguments_list, v0);
      __ Pop(target, new_target);
      __ Lw(len, UntagSmiFieldMemOperand(v0, FixedArray::kLengthOffset));
    }
    __ Branch(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ Lw(len, UntagSmiFieldMemOperand(arguments_list,
                                       JSArgumentsObject::kLengthOffset));
    __ Ld(a4, FieldMemOperand(arguments_list, JSObject::kElementsOffset));
    __ Lw(at, UntagSmiFieldMemOperand(a4, FixedArray::kLengthOffset));
    __ Branch(&create_runtime, ne, len, Operand(at));
    __ mov(args, a4);

    __ Branch(&done_create);

    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ bind(&create_holey_array);
    __ Ld(a2, FieldMemOperand(a2, Map::kPrototypeOffset));
    __ Ld(at, ContextMemOperand(native_context,
                                Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ Branch(&create_runtime, ne, a2, Operand(at));
    __ LoadRoot(at, Heap::kArrayProtectorRootIndex);
    __ Lw(a2, FieldMemOperand(at, PropertyCell::kValueOffset));
    __ Branch(&create_runtime, ne, a2,
              Operand(Smi::FromInt(Isolate::kProtectorValid)));
    __ Lw(a2, UntagSmiFieldMemOperand(a0, JSArray::kLengthOffset));
    __ Ld(a0, FieldMemOperand(a0, JSArray::kElementsOffset));
    __ Branch(&done_create);

    // Try to create the list from a JSArray object.
    __ bind(&create_array);
    __ Lbu(t1, FieldMemOperand(a2, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(t1);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    __ Branch(&create_holey_array, eq, t1, Operand(FAST_HOLEY_SMI_ELEMENTS));
    __ Branch(&create_holey_array, eq, t1, Operand(FAST_HOLEY_ELEMENTS));
    __ Branch(&create_runtime, hi, t1, Operand(FAST_ELEMENTS));
    __ Lw(a2, UntagSmiFieldMemOperand(arguments_list, JSArray::kLengthOffset));
    __ Ld(a0, FieldMemOperand(arguments_list, JSArray::kElementsOffset));

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(a4, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ Dsubu(a4, sp, a4);
    // Check if the arguments will overflow the stack.
    __ dsll(at, len, kPointerSizeLog2);
    __ Branch(&done, gt, a4, Operand(at));  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- a1    : target
  //  -- a0    : args (a FixedArray built from argumentsList)
  //  -- a2    : len (number of elements to push from args)
  //  -- a3    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label done, push, loop;
    Register src = a4;
    Register scratch = len;

    __ daddiu(src, args, FixedArray::kHeaderSize - kHeapObjectTag);
    __ Branch(&done, eq, len, Operand(zero_reg), i::USE_DELAY_SLOT);
    __ mov(a0, len);  // The 'len' argument for Call() or Construct().
    __ dsll(scratch, len, kPointerSizeLog2);
    __ Dsubu(scratch, sp, Operand(scratch));
    __ LoadRoot(t1, Heap::kTheHoleValueRootIndex);
    __ bind(&loop);
    __ Ld(a5, MemOperand(src));
    __ Branch(&push, ne, a5, Operand(t1));
    __ LoadRoot(a5, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ daddiu(src, src, kPointerSize);
    __ Push(a5);
    __ Branch(&loop, ne, scratch, Operand(sp));
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- a0             : argument count (len)
  //  -- a1             : target
  //  -- a3             : new.target (checked to be constructor or undefinded)
  //  -- sp[0]          : args[len-1]
  //  -- sp[8]          : args[len-2]
  //     ...            : ...
  //  -- sp[8*(len-2)]  : args[1]
  //  -- sp[8*(len-1)]  : args[0]
  //  ----------------------------------

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    Label construct;
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ Branch(&construct, ne, a3, Operand(at));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
    __ bind(&construct);
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_ForwardVarargs(MacroAssembler* masm,
                                       Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a3 : the new.target (for [[Construct]] calls)
  //  -- a1 : the target to call (can be any Object)
  //  -- a2 : start index (to support rest parameters)
  // -----------------------------------

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ Ld(a6, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ld(a7, MemOperand(a6, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&arguments_adaptor, eq, a7,
            Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  {
    __ Ld(a7, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ Ld(a7, FieldMemOperand(a7, JSFunction::kSharedFunctionInfoOffset));
    __ Lw(a7,
          FieldMemOperand(a7, SharedFunctionInfo::kFormalParameterCountOffset));
    __ mov(a6, fp);
  }
  __ Branch(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Just get the length from the ArgumentsAdaptorFrame.
    __ Lw(a7, UntagSmiMemOperand(
                  a6, ArgumentsAdaptorFrameConstants::kLengthOffset));
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
  __ Subu(a7, a7, a2);
  __ Branch(&stack_done, le, a7, Operand(zero_reg));
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, a7, a4, a5, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ Daddu(a0, a0, a7);
      __ bind(&loop);
      {
        __ Dlsa(at, a6, a7, kPointerSizeLog2);
        __ Ld(at, MemOperand(at, 1 * kPointerSize));
        __ push(at);
        __ Subu(a7, a7, Operand(1));
        __ Branch(&loop, ne, a7, Operand(zero_reg));
      }
    }
  }
  __ Branch(&stack_done);
  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
  __ bind(&stack_done);

  // Tail-call to the {code} handler.
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
  __ li(at, Operand(is_tail_call_elimination_enabled));
  __ Lb(scratch1, MemOperand(at));
  __ Branch(&done, eq, scratch1, Operand(zero_reg));

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ Ld(scratch3,
          MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ Branch(&no_interpreter_frame, ne, scratch3,
              Operand(StackFrame::TypeToMarker(StackFrame::STUB)));
    __ Ld(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ Ld(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ld(scratch3,
        MemOperand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&no_arguments_adaptor, ne, scratch3,
            Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(fp, scratch2);
  __ Lw(caller_args_count_reg,
        UntagSmiMemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ Branch(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ Ld(scratch1,
        MemOperand(fp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  __ Ld(scratch1,
        FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ Lw(caller_args_count_reg,
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
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(a1);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that function is not a "classConstructor".
  Label class_constructor;
  __ Ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kFunctionKindByteOffset));
  __ And(at, a3, Operand(SharedFunctionInfo::kClassConstructorBitsWithinByte));
  __ Branch(&class_constructor, ne, at, Operand(zero_reg));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ Ld(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ Lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kNativeByteOffset));
  __ And(at, a3, Operand((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                         (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ Branch(&done_convert, ne, at, Operand(zero_reg));
  {
    // ----------- S t a t e -------------
    //  -- a0 : the number of arguments (not including the receiver)
    //  -- a1 : the function to call (checked to be a JSFunction)
    //  -- a2 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(a3);
    } else {
      Label convert_to_object, convert_receiver;
      __ Dlsa(at, sp, a0, kPointerSizeLog2);
      __ Ld(a3, MemOperand(at));
      __ JumpIfSmi(a3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, a4, a4);
      __ Branch(&done_convert, hs, a4, Operand(FIRST_JS_RECEIVER_TYPE));
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(a3, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(a3, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(a3);
        }
        __ Branch(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(a0);
        __ Push(a0, a1);
        __ mov(a0, a3);
        __ Push(cp);
        __ Call(masm->isolate()->builtins()->ToObject(),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(a3, v0);
        __ Pop(a0, a1);
        __ SmiUntag(a0);
      }
      __ Ld(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ Dlsa(at, sp, a0, kPointerSizeLog2);
    __ Sd(a3, MemOperand(at));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSFunction)
  //  -- a2 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, a0, t0, t1, t2);
  }

  __ Lw(a2,
        FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  ParameterCount actual(a0);
  ParameterCount expected(a2);
  __ InvokeFunctionCode(a1, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, a0, t0, t1, t2);
  }

  // Patch the receiver to [[BoundThis]].
  {
    __ Ld(at, FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ Dlsa(a4, sp, a0, kPointerSizeLog2);
    __ Sd(at, MemOperand(a4));
  }

  // Load [[BoundArguments]] into a2 and length of that into a4.
  __ Ld(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ Lw(a4, UntagSmiFieldMemOperand(a2, FixedArray::kLengthOffset));

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a4 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ dsll(a5, a4, kPointerSizeLog2);
    __ Dsubu(sp, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Daddu(sp, sp, Operand(a5));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Relocate arguments down the stack.
  {
    Label loop, done_loop;
    __ mov(a5, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, gt, a5, Operand(a0));
    __ Dlsa(a6, sp, a4, kPointerSizeLog2);
    __ Ld(at, MemOperand(a6));
    __ Dlsa(a6, sp, a5, kPointerSizeLog2);
    __ Sd(at, MemOperand(a6));
    __ Daddu(a4, a4, Operand(1));
    __ Daddu(a5, a5, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ Lw(a4, UntagSmiFieldMemOperand(a2, FixedArray::kLengthOffset));
    __ Daddu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Dsubu(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ Dlsa(a5, a2, a4, kPointerSizeLog2);
    __ Ld(at, MemOperand(a5));
    __ Dlsa(a5, sp, a0, kPointerSizeLog2);
    __ Sd(at, MemOperand(a5));
    __ Daddu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ Ld(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ li(at, Operand(ExternalReference(Builtins::kCall_ReceiverIsAny,
                                      masm->isolate())));
  __ Ld(at, MemOperand(at));
  __ Daddu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(a1, &non_callable);
  __ bind(&non_smi);
  __ GetObjectType(a1, t1, t2);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Check if target has a [[Call]] internal method.
  __ Lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t1, t1, Operand(1 << Map::kIsCallable));
  __ Branch(&non_callable, eq, t1, Operand(zero_reg));

  __ Branch(&non_function, ne, t2, Operand(JS_PROXY_TYPE));

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, a0, t0, t1, t2);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ Push(a1);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ Daddu(a0, a0, 2);
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ Dlsa(at, sp, a0, kPointerSizeLog2);
  __ Sd(a1, MemOperand(at));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, a1);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void CheckSpreadAndPushToStack(MacroAssembler* masm) {
  Register argc = a0;
  Register constructor = a1;
  Register new_target = a3;

  Register scratch = t0;
  Register scratch2 = t1;

  Register spread = a2;
  Register spread_map = a4;

  Register spread_len = a4;

  Register native_context = a5;

  Label runtime_call, push_args;
  __ Ld(spread, MemOperand(sp, 0));
  __ JumpIfSmi(spread, &runtime_call);
  __ Ld(spread_map, FieldMemOperand(spread, HeapObject::kMapOffset));
  __ Ld(native_context, NativeContextMemOperand());

  // Check that the spread is an array.
  __ Lbu(scratch, FieldMemOperand(spread_map, Map::kInstanceTypeOffset));
  __ Branch(&runtime_call, ne, scratch, Operand(JS_ARRAY_TYPE));

  // Check that we have the original ArrayPrototype.
  __ Ld(scratch, FieldMemOperand(spread_map, Map::kPrototypeOffset));
  __ Ld(scratch2, ContextMemOperand(native_context,
                                    Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
  __ Branch(&runtime_call, ne, scratch, Operand(scratch2));

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  __ LoadRoot(scratch, Heap::kArrayIteratorProtectorRootIndex);
  __ Ld(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ Branch(&runtime_call, ne, scratch,
            Operand(Smi::FromInt(Isolate::kProtectorValid)));

  // Check that the map of the initial array iterator hasn't changed.
  __ Ld(scratch,
        ContextMemOperand(native_context,
                          Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_INDEX));
  __ Ld(scratch, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ Ld(scratch2,
        ContextMemOperand(native_context,
                          Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_MAP_INDEX));
  __ Branch(&runtime_call, ne, scratch, Operand(scratch2));

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  Label no_protector_check;
  __ Lbu(scratch, FieldMemOperand(spread_map, Map::kBitField2Offset));
  __ DecodeField<Map::ElementsKindBits>(scratch);
  __ Branch(&runtime_call, hi, scratch, Operand(FAST_HOLEY_ELEMENTS));
  // For non-FastHoley kinds, we can skip the protector check.
  __ Branch(&no_protector_check, eq, scratch, Operand(FAST_SMI_ELEMENTS));
  __ Branch(&no_protector_check, eq, scratch, Operand(FAST_ELEMENTS));
  // Check the ArrayProtector cell.
  __ LoadRoot(scratch, Heap::kArrayProtectorRootIndex);
  __ Ld(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ Branch(&runtime_call, ne, scratch,
            Operand(Smi::FromInt(Isolate::kProtectorValid)));

  __ bind(&no_protector_check);
  // Load the FixedArray backing store, but use the length from the array.
  __ Lw(spread_len, UntagSmiFieldMemOperand(spread, JSArray::kLengthOffset));
  __ Ld(spread, FieldMemOperand(spread, JSArray::kElementsOffset));
  __ Branch(&push_args);

  __ bind(&runtime_call);
  {
    // Call the builtin for the result of the spread.
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(argc);
    __ Push(constructor, new_target, argc, spread);
    __ CallRuntime(Runtime::kSpreadIterableFixed);
    __ mov(spread, v0);
    __ Pop(constructor, new_target, argc);
    __ SmiUntag(argc);
  }

  {
    // Calculate the new nargs including the result of the spread.
    __ Lw(spread_len,
          UntagSmiFieldMemOperand(spread, FixedArray::kLengthOffset));

    __ bind(&push_args);
    // argc += spread_len - 1. Subtract 1 for the spread itself.
    __ Daddu(argc, argc, spread_len);
    __ Dsubu(argc, argc, Operand(1));

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
    // overflowed here which will cause ip to become negative.
    __ Dsubu(scratch, sp, scratch);
    // Check if the arguments will overflow the stack.
    __ dsll(at, spread_len, kPointerSizeLog2);
    __ Branch(&done, gt, scratch, Operand(at));  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Put the evaluated spread onto the stack as additional arguments.
  {
    __ mov(scratch, zero_reg);
    Label done, push, loop;
    __ bind(&loop);
    __ Branch(&done, eq, scratch, Operand(spread_len));
    __ Dlsa(scratch2, spread, scratch, kPointerSizeLog2);
    __ Ld(scratch2, FieldMemOperand(scratch2, FixedArray::kHeaderSize));
    __ JumpIfNotRoot(scratch2, Heap::kTheHoleValueRootIndex, &push);
    __ LoadRoot(scratch2, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(scratch2);
    __ Daddu(scratch, scratch, Operand(1));
    __ Branch(&loop);
    __ bind(&done);
  }
}

// static
void Builtins::Generate_CallWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------

  // CheckSpreadAndPushToStack will push a3 to save it.
  __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            TailCallMode::kDisallow),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(a1);

  // Calling convention for function specific ConstructStubs require
  // a2 to contain either an AllocationSite or undefined.
  __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ Ld(a4, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ Ld(a4, FieldMemOperand(a4, SharedFunctionInfo::kConstructStubOffset));
  __ Daddu(at, a4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a3 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(a1);

  // Load [[BoundArguments]] into a2 and length of that into a4.
  __ Ld(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ Lw(a4, UntagSmiFieldMemOperand(a2, FixedArray::kLengthOffset));

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a3 : the new target (checked to be a constructor)
  //  -- a4 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ dsll(a5, a4, kPointerSizeLog2);
    __ Dsubu(sp, sp, Operand(a5));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Daddu(sp, sp, Operand(a5));
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowStackOverflow);
    }
    __ bind(&done);
  }

  // Relocate arguments down the stack.
  {
    Label loop, done_loop;
    __ mov(a5, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, ge, a5, Operand(a0));
    __ Dlsa(a6, sp, a4, kPointerSizeLog2);
    __ Ld(at, MemOperand(a6));
    __ Dlsa(a6, sp, a5, kPointerSizeLog2);
    __ Sd(at, MemOperand(a6));
    __ Daddu(a4, a4, Operand(1));
    __ Daddu(a5, a5, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ Lw(a4, UntagSmiFieldMemOperand(a2, FixedArray::kLengthOffset));
    __ Daddu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Dsubu(a4, a4, Operand(1));
    __ Branch(&done_loop, lt, a4, Operand(zero_reg));
    __ Dlsa(a5, a2, a4, kPointerSizeLog2);
    __ Ld(at, MemOperand(a5));
    __ Dlsa(a5, sp, a0, kPointerSizeLog2);
    __ Sd(at, MemOperand(a5));
    __ Daddu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label skip_load;
    __ Branch(&skip_load, ne, a1, Operand(a3));
    __ Ld(a3, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&skip_load);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ Ld(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ li(at, Operand(ExternalReference(Builtins::kConstruct, masm->isolate())));
  __ Ld(at, MemOperand(at));
  __ Daddu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}

// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (checked to be a JSProxy)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ Push(a1, a3);
  // Include the pushed new_target, constructor and the receiver.
  __ Daddu(a0, a0, Operand(3));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (can be any Object)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(a1, &non_constructor);

  // Dispatch based on instance type.
  __ Ld(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ Lbu(t2, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));

  // Check if target has a [[Construct]] internal method.
  __ Lbu(t3, FieldMemOperand(t1, Map::kBitFieldOffset));
  __ And(t3, t3, Operand(1 << Map::kIsConstructor));
  __ Branch(&non_constructor, eq, t3, Operand(zero_reg));

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ Jump(masm->isolate()->builtins()->ConstructBoundFunction(),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_BOUND_FUNCTION_TYPE));

  // Only dispatch to proxies after checking whether they are constructors.
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq, t2, Operand(JS_PROXY_TYPE));

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ Dlsa(at, sp, a0, kPointerSizeLog2);
    __ Sd(a1, MemOperand(at));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, a1);
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
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the constructor to call (can be any Object)
  //  -- a3 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : requested object size (untagged)
  //  -- ra : return address
  // -----------------------------------
  __ SmiTag(a0);
  __ Push(a0);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : requested object size (untagged)
  //  -- ra : return address
  // -----------------------------------
  __ SmiTag(a0);
  __ Move(a1, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(a0, a1);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0 : message_id as Smi
  //  -- ra : return address
  // -----------------------------------
  __ Push(a0);
  __ Move(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // State setup as expected by MacroAssembler::InvokePrologue.
  // ----------- S t a t e -------------
  //  -- a0: actual arguments count
  //  -- a1: function (passed through to callee)
  //  -- a2: expected arguments count
  //  -- a3: new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ Branch(&dont_adapt_arguments, eq, a2,
            Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  // We use Uless as the number of argument should always be greater than 0.
  __ Branch(&too_few, Uless, a0, Operand(a2));

  {  // Enough parameters: actual >= expected.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, a5, at, &stack_overflow);

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
    // a3: new target (passed through to callee)
    // a4: copy end address

    Label copy;
    __ bind(&copy);
    __ Ld(a5, MemOperand(a0));
    __ push(a5);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a4));
    __ daddiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, a5, at, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into a7.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
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
    // a3: new target (passed through to callee)
    // a7: copy end address
    Label copy;
    __ bind(&copy);
    __ Ld(a4, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Dsubu(sp, sp, kPointerSize);
    __ Dsubu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(a7));
    __ Sd(a4, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
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
    __ Sd(a5, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(a0, a2);
  // a0 : expected number of arguments
  // a1 : function (passed through to callee)
  // a3: new target (passed through to callee)
  __ Ld(a4, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ Call(a4);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Don't adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ Ld(a4, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ Jump(a4);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    const RegList gp_regs = a0.bit() | a1.bit() | a2.bit() | a3.bit() |
                            a4.bit() | a5.bit() | a6.bit() | a7.bit();
    const RegList fp_regs = f2.bit() | f4.bit() | f6.bit() | f8.bit() |
                            f10.bit() | f12.bit() | f14.bit();
    __ MultiPush(gp_regs);
    __ MultiPushFPU(fp_regs);

    __ Move(kContextRegister, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);

    // Restore registers.
    __ MultiPopFPU(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Now jump to the instructions of the returned code object.
  __ Daddu(at, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64

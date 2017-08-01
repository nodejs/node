// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

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
  //  -- r2                 : number of arguments excluding receiver
  //  -- r3                 : target
  //  -- r5                 : new.target
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------
  __ AssertFunction(r3);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  // JumpToExternalReference expects r2 to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ AddP(r2, r2, Operand(num_extra_args + 1));

  // Insert extra arguments.
  __ SmiTag(r2);
  __ Push(r2, r3, r5);
  __ SmiUntag(r2);

  __ JumpToExternalReference(ExternalReference(address, masm->isolate()),
                             exit_frame_type == BUILTIN_EXIT);
}

// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the InternalArray function from the current native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}

// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the Array function from the current native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
}

void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r3);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ LoadP(r4, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r4);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction, cr0);
    __ CompareObjectType(r4, r5, r6, MAP_TYPE);
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
  //  -- r2     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r3);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ LoadP(r4, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r4);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r4, r5, r6, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  __ LoadRR(r5, r3);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2                     : number of arguments
  //  -- r3                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into r2.
  Label no_arguments;
  {
    __ LoadRR(r4, r2);  // Store argc in r4.
    __ CmpP(r2, Operand::Zero());
    __ beq(&no_arguments);
    __ SubP(r2, r2, Operand(1));
    __ ShiftLeftP(r2, r2, Operand(kPointerSizeLog2));
    __ LoadP(r2, MemOperand(sp, r2));
  }

  // 2a. Convert the first argument to a number.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r4);
    __ EnterBuiltinFrame(cp, r3, r4);
    __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, r3, r4);
    __ SmiUntag(r4);
  }

  {
    // Drop all arguments including the receiver.
    __ Drop(r4);
    __ Ret(1);
  }

  // 2b. No arguments, return +0.
  __ bind(&no_arguments);
  __ LoadSmiLiteral(r2, Smi::kZero);
  __ Ret(1);
}

// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2                     : number of arguments
  //  -- r3                     : constructor function
  //  -- r5                     : new target
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  // 2. Load the first argument into r4.
  {
    Label no_arguments, done;
    __ LoadRR(r8, r2);  // Store argc in r8.
    __ CmpP(r2, Operand::Zero());
    __ beq(&no_arguments);
    __ SubP(r2, r2, Operand(1));
    __ ShiftLeftP(r4, r2, Operand(kPointerSizeLog2));
    __ LoadP(r4, MemOperand(sp, r4));
    __ b(&done);
    __ bind(&no_arguments);
    __ LoadSmiLiteral(r4, Smi::kZero);
    __ bind(&done);
  }

  // 3. Make sure r4 is a number.
  {
    Label done_convert;
    __ JumpIfSmi(r4, &done_convert);
    __ CompareObjectType(r4, r6, r6, HEAP_NUMBER_TYPE);
    __ beq(&done_convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r8);
      __ EnterBuiltinFrame(cp, r3, r8);
      __ Push(r5);
      __ LoadRR(r2, r4);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ LoadRR(r4, r2);
      __ Pop(r5);
      __ LeaveBuiltinFrame(cp, r3, r8);
      __ SmiUntag(r8);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ CmpP(r3, r5);
  __ bne(&new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(r2, r3, r4, r6, r7, &new_object);
  __ b(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r8);
    __ EnterBuiltinFrame(cp, r3, r8);
    __ Push(r4);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(r4);
    __ LeaveBuiltinFrame(cp, r3, r8);
    __ SmiUntag(r8);
  }
  __ StoreP(r4, FieldMemOperand(r2, JSValue::kValueOffset), r0);

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r8);
    __ Ret(1);
  }
}

// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2                     : number of arguments
  //  -- r3                     : constructor function
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------
  // 1. Load the first argument into r2.
  Label no_arguments;
  {
    __ LoadRR(r4, r2);  // Store argc in r4
    __ CmpP(r2, Operand::Zero());
    __ beq(&no_arguments);
    __ SubP(r2, r2, Operand(1));
    __ ShiftLeftP(r2, r2, Operand(kPointerSizeLog2));
    __ LoadP(r2, MemOperand(sp, r2));
  }

  // 2a. At least one argument, return r2 if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label drop_frame_and_ret, to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(r2, &to_string);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CompareObjectType(r2, r5, r5, FIRST_NONSTRING_TYPE);
    __ bgt(&to_string);
    __ beq(&symbol_descriptive_string);
    __ b(&drop_frame_and_ret);
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(r2, Heap::kempty_stringRootIndex);
    __ Ret(1);
  }

  // 3a. Convert r2 to a string.
  __ bind(&to_string);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r4);
    __ EnterBuiltinFrame(cp, r3, r4);
    __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
    __ LeaveBuiltinFrame(cp, r3, r4);
    __ SmiUntag(r4);
  }
  __ b(&drop_frame_and_ret);
  // 3b. Convert symbol in r2 to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ Drop(r4);
    __ Drop(1);
    __ Push(r2);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r4);
    __ Ret(1);
  }
}

// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2                     : number of arguments
  //  -- r3                     : constructor function
  //  -- r5                     : new target
  //  -- cp                     : context
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  // 2. Load the first argument into r4.
  {
    Label no_arguments, done;
    __ LoadRR(r8, r2);  // Store argc in r8.
    __ CmpP(r2, Operand::Zero());
    __ beq(&no_arguments);
    __ SubP(r2, r2, Operand(1));
    __ ShiftLeftP(r4, r2, Operand(kPointerSizeLog2));
    __ LoadP(r4, MemOperand(sp, r4));
    __ b(&done);
    __ bind(&no_arguments);
    __ LoadRoot(r4, Heap::kempty_stringRootIndex);
    __ bind(&done);
  }

  // 3. Make sure r4 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(r4, &convert);
    __ CompareObjectType(r4, r6, r6, FIRST_NONSTRING_TYPE);
    __ blt(&done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(r8);
      __ EnterBuiltinFrame(cp, r3, r8);
      __ Push(r5);
      __ LoadRR(r2, r4);
      __ Call(masm->isolate()->builtins()->ToString(), RelocInfo::CODE_TARGET);
      __ LoadRR(r4, r2);
      __ Pop(r5);
      __ LeaveBuiltinFrame(cp, r3, r8);
      __ SmiUntag(r8);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label drop_frame_and_ret, new_object;
  __ CmpP(r3, r5);
  __ bne(&new_object);

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(r2, r3, r4, r6, r7, &new_object);
  __ b(&drop_frame_and_ret);

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ SmiTag(r8);
    __ EnterBuiltinFrame(cp, r3, r8);
    __ Push(r4);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(r4);
    __ LeaveBuiltinFrame(cp, r3, r8);
    __ SmiUntag(r8);
  }
  __ StoreP(r4, FieldMemOperand(r2, JSValue::kValueOffset), r0);

  __ bind(&drop_frame_and_ret);
  {
    __ Drop(r8);
    __ Ret(1);
  }
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ AddP(ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- r2 : argument count (preserved for callee)
  //  -- r3 : target function (preserved for callee)
  //  -- r5 : new target (preserved for callee)
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(r2);
    __ Push(r2, r3, r5, r3);

    __ CallRuntime(function_id, 1);
    __ LoadRR(r4, r2);

    // Restore target function and new target.
    __ Pop(r2, r3, r5);
    __ SmiUntag(r2);
  }
  __ AddP(ip, r4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ CmpLogicalP(sp, RootMemOperand(Heap::kStackLimitRootIndex));
  __ bge(&ok, Label::kNear);

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}

namespace {

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  Label post_instantiation_deopt_entry;
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- r3     : constructor function
  //  -- r5     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r2);
    __ Push(cp, r2);
    __ SmiUntag(r2);
    // The receiver for the builtin/api call.
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    // Set up pointer to last argument.
    __ la(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // r2: number of arguments
    // r3: constructor function
    // r4: address of last argument (caller sp)
    // r5: new target
    // cr0: condition indicating whether r2 is zero
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: number of arguments (smi-tagged)
    Label loop, no_args;
    __ beq(&no_args);
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ SubP(sp, sp, ip);
    __ LoadRR(r1, r2);
    __ bind(&loop);
    __ lay(ip, MemOperand(ip, -kPointerSize));
    __ LoadP(r0, MemOperand(ip, r6));
    __ StoreP(r0, MemOperand(ip, sp));
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);

    // Call the function.
    // r2: number of arguments
    // r3: constructor function
    // r5: new target

    ParameterCount actual(r2);
    __ InvokeFunction(r3, r5, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Restore context from the frame.
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(r3, r3);
  __ AddP(sp, sp, r3);
  __ AddP(sp, sp, Operand(kPointerSize));
  __ Ret();
}

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Generate_JSConstructStubGeneric(MacroAssembler* masm,
                                     bool restrict_constructor_return) {
  // ----------- S t a t e -------------
  //  --      r2: number of arguments (untagged)
  //  --      r3: constructor function
  //  --      r5: new target
  //  --      cp: context
  //  --      lr: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);
    Label post_instantiation_deopt_entry, not_create_implicit_receiver;

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r2);
    __ Push(cp, r2, r3, r5);

    // ----------- S t a t e -------------
    //  --        sp[0*kPointerSize]: new target
    //  -- r3 and sp[1*kPointerSize]: constructor function
    //  --        sp[2*kPointerSize]: number of arguments (tagged)
    //  --        sp[3*kPointerSize]: context
    // -----------------------------------

    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
    __ LoadlW(r6,
              FieldMemOperand(r6, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestBitMask(r6,
                   FunctionKind::kDerivedConstructor
                       << SharedFunctionInfo::kFunctionKindShift,
                   r0);
    __ bne(&not_create_implicit_receiver);

    // If not derived class constructor: Allocate the new receiver object.
    __ IncrementCounter(masm->isolate()->counters()->constructed_objects(), 1,
                        r6, r7);
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ b(&post_instantiation_deopt_entry);

    // Else: use TheHoleValue as receiver for constructor call
    __ bind(&not_create_implicit_receiver);
    __ LoadRoot(r2, Heap::kTheHoleValueRootIndex);

    // ----------- S t a t e -------------
    //  --                          r2: receiver
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
    __ Pop(r5);
    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(r2, r2);

    // ----------- S t a t e -------------
    //  --                 r5: new target
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: implicit receiver
    //  -- sp[2*kPointerSize]: constructor function
    //  -- sp[3*kPointerSize]: number of arguments (tagged)
    //  -- sp[4*kPointerSize]: context
    // -----------------------------------

    // Restore constructor function and argument count.
    __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
    __ LoadP(r2, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    __ SmiUntag(r2);

    // Set up pointer to last argument.
    __ la(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, no_args;
    // ----------- S t a t e -------------
    //  --                        r2: number of arguments (untagged)
    //  --                        r5: new target
    //  --                        r6: pointer to last argument
    //  --                        cr0: condition indicating whether r2 is zero
    //  --        sp[0*kPointerSize]: implicit receiver
    //  --        sp[1*kPointerSize]: implicit receiver
    //  -- r3 and sp[2*kPointerSize]: constructor function
    //  --        sp[3*kPointerSize]: number of arguments (tagged)
    //  --        sp[4*kPointerSize]: context
    // -----------------------------------

    __ beq(&no_args);
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ SubP(sp, sp, ip);
    __ LoadRR(r1, r2);
    __ bind(&loop);
    __ lay(ip, MemOperand(ip, -kPointerSize));
    __ LoadP(r0, MemOperand(ip, r6));
    __ StoreP(r0, MemOperand(ip, sp));
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);

    // Call the function.
    ParameterCount actual(r2);
    __ InvokeFunction(r3, r5, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // ----------- S t a t e -------------
    //  --                 r0: constructor result
    //  -- sp[0*kPointerSize]: implicit receiver
    //  -- sp[1*kPointerSize]: constructor function
    //  -- sp[2*kPointerSize]: number of arguments
    //  -- sp[3*kPointerSize]: context
    // -----------------------------------

    // Store offset of return address for deoptimizer.
    masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
        masm->pc_offset());

    // Restore the context from the frame.
    __ LoadP(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, do_throw, other_result, leave_frame;

    // If the result is undefined, we jump out to using the implicit receiver.
    __ JumpIfRoot(r2, Heap::kUndefinedValueRootIndex, &use_receiver);

    // Otherwise we do a smi check and fall through to check if the return value
    // is a valid receiver.

    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(r2, &other_result);

    // If the type of the result (stored in its map) is less than
    // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CompareObjectType(r2, r6, r6, FIRST_JS_RECEIVER_TYPE);
    __ bge(&leave_frame);

    __ bind(&other_result);
    // The result is now neither undefined nor an object.
    if (restrict_constructor_return) {
      // Throw if constructor function is a class constructor
      __ LoadP(r6, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
      __ LoadP(r6, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
      __ LoadlW(r6,
                FieldMemOperand(r6, SharedFunctionInfo::kCompilerHintsOffset));
      __ TestBitMask(r6,
                     FunctionKind::kClassConstructor
                         << SharedFunctionInfo::kFunctionKindShift,
                     r0);
      __ beq(&use_receiver);
    } else {
      __ b(&use_receiver);
    }
    __ bind(&do_throw);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);
    }

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ LoadP(r2, MemOperand(sp));
    __ JumpIfRoot(r2, Heap::kTheHoleValueRootIndex, &do_throw);

    __ bind(&leave_frame);
    // Restore smi-tagged arguments count from the frame.
    __ LoadP(r3, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);

  __ SmiToPtrArrayOffset(r3, r3);
  __ AddP(sp, sp, r3);
  __ AddP(sp, sp, Operand(kPointerSize));
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
  //  -- r2 : the value to pass to the generator
  //  -- r3 : the JSGeneratorObject to resume
  //  -- r4 : the resume mode (tagged)
  //  -- r5 : the SuspendFlags of the earlier suspend call (tagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiUntag(r5);
  __ AssertGeneratorObject(r3, r5);

  // Store input value into generator object.
  Label async_await, done_store_input;

  __ tmll(r5, Operand(static_cast<int>(SuspendFlags::kAsyncGeneratorAwait)));
  __ b(Condition(1), &async_await);

  __ StoreP(r2, FieldMemOperand(r3, JSGeneratorObject::kInputOrDebugPosOffset),
            r0);
  __ RecordWriteField(r3, JSGeneratorObject::kInputOrDebugPosOffset, r2, r5,
                      kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ b(&done_store_input);

  __ bind(&async_await);
  __ StoreP(
      r2,
      FieldMemOperand(r3, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset),
      r0);
  __ RecordWriteField(r3, JSAsyncGeneratorObject::kAwaitInputOrDebugPosOffset,
                      r2, r5, kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ b(&done_store_input);

  __ bind(&done_store_input);
  // `r5` no longer holds SuspendFlags

  // Store resume mode into generator object.
  __ StoreP(r4, FieldMemOperand(r3, JSGeneratorObject::kResumeModeOffset));

  // Load suspended function and context.
  __ LoadP(r6, FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  __ LoadP(cp, FieldMemOperand(r6, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ mov(ip, Operand(debug_hook));
  __ LoadB(ip, MemOperand(ip));
  __ CmpSmiLiteral(ip, Smi::kZero, r0);
  __ bne(&prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.

  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());

  __ mov(ip, Operand(debug_suspended_generator));
  __ LoadP(ip, MemOperand(ip));
  __ CmpP(ip, r3);
  __ beq(&prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Push receiver.
  __ LoadP(ip, FieldMemOperand(r3, JSGeneratorObject::kReceiverOffset));
  __ Push(ip);

  // ----------- S t a t e -------------
  //  -- r3    : the JSGeneratorObject to resume
  //  -- r4    : the resume mode (tagged)
  //  -- r6    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ LoadP(r5, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
  __ LoadW(
      r2, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label loop, done_loop;
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
#if V8_TARGET_ARCH_S390X
    __ CmpP(r2, Operand::Zero());
    __ beq(&done_loop);
#else
    __ SmiUntag(r2);
    __ LoadAndTestP(r2, r2);
    __ beq(&done_loop);
#endif
    __ LoadRR(r1, r2);
    __ bind(&loop);
    __ push(ip);
    __ BranchOnCount(r1, &loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ LoadP(r5, FieldMemOperand(r5, SharedFunctionInfo::kFunctionDataOffset));
    __ CompareObjectType(r5, r5, r5, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kMissingBytecodeArray);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ LoadRR(r5, r3);
    __ LoadRR(r3, r6);
    __ LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeEntryOffset));
    __ JumpToJSEntry(ip);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r4, r6);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r3, r4);
    __ LoadP(r6, FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r4);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r3, r4);
    __ LoadP(r6, FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  __ push(r3);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}

enum IsTagged { kArgcIsSmiTagged, kArgcIsUntaggedInt };

// Clobbers r4; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm, Register argc,
                                        IsTagged argc_is_tagged) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r4, Heap::kRealStackLimitRootIndex);
  // Make r4 the space we have left. The stack might already be overflowed
  // here which will cause r4 to become negative.
  __ SubP(r4, sp, r4);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ SmiToPtrArrayOffset(r0, argc);
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ ShiftLeftP(r0, argc, Operand(kPointerSizeLog2));
  }
  __ CmpP(r4, r0);
  __ bgt(&okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r2: new.target
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv
  // r0,r7-r9, cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Enter an internal frame.
  {
    // FrameScope ends up calling MacroAssembler::EnterFrame here
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ mov(cp, Operand(context_address));
    __ LoadP(cp, MemOperand(cp));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ Push(r3, r4);

    // Check if we have enough stack space to push all arguments.
    // Clobbers r4.
    Generate_CheckStackOverflow(masm, r5, kArgcIsUntaggedInt);

    // Copy arguments to the stack in a loop from argv to sp.
    // The arguments are actually placed in reverse order on sp
    // compared to argv (i.e. arg1 is highest memory in sp).
    // r3: function
    // r5: argc
    // r6: argv, i.e. points to first arg
    // r7: scratch reg to hold scaled argc
    // r8: scratch reg to hold arg handle
    // r9: scratch reg to hold index into argv
    Label argLoop, argExit;
    intptr_t zero = 0;
    __ ShiftLeftP(r7, r5, Operand(kPointerSizeLog2));
    __ SubRR(sp, r7);                // Buy the stack frame to fit args
    __ LoadImmP(r9, Operand(zero));  // Initialize argv index
    __ bind(&argLoop);
    __ CmpPH(r7, Operand(zero));
    __ beq(&argExit, Label::kNear);
    __ lay(r7, MemOperand(r7, -kPointerSize));
    __ LoadP(r8, MemOperand(r9, r6));         // read next parameter
    __ la(r9, MemOperand(r9, kPointerSize));  // r9++;
    __ LoadP(r0, MemOperand(r8));             // dereference handle
    __ StoreP(r0, MemOperand(r7, sp));        // push parameter
    __ b(&argLoop);
    __ bind(&argExit);

    // Setup new.target and argc.
    __ LoadRR(r6, r2);
    __ LoadRR(r2, r5);
    __ LoadRR(r5, r6);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
    __ LoadRR(r7, r6);
    __ LoadRR(r8, r6);
    __ LoadRR(r9, r6);

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the JS frame and remove the parameters (except function), and
    // return.
  }
  __ b(r14);

  // r2: result
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
  __ AddP(optimized_code_entry, optimized_code_entry,
          Operand(Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(optimized_code_entry,
            FieldMemOperand(closure, JSFunction::kCodeEntryOffset), r0);
  __ RecordWriteCodeEntryField(closure, optimized_code_entry, scratch2);

  // Link the closure into the optimized function list.
  // r6 : code entry
  // r9: native context
  // r3 : closure
  __ LoadP(native_context, NativeContextMemOperand());
  __ LoadP(scratch2, ContextMemOperand(native_context,
                                       Context::OPTIMIZED_FUNCTIONS_LIST));
  __ StoreP(scratch2,
            FieldMemOperand(closure, JSFunction::kNextFunctionLinkOffset), r0);
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, scratch2,
                      scratch3, kLRHasNotBeenSaved, kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ StoreP(
      closure,
      ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST), r0);
  // Save closure before the write barrier.
  __ LoadRR(scratch2, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, closure,
                            scratch3, kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ LoadRR(closure, scratch2);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ LoadP(args_count,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadlW(args_count,
            FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  __ AddP(sp, sp, args_count);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.  The actual argument count matches the formal parameter
// count expected by the function.
//
// The live registers are:
//   o r3: the JS function object being called.
//   o r5: the new target
//   o cp: our context
//   o pp: the caller's constant pool pointer (if enabled)
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(r3);

  // First check if there is optimized code in the feedback vector which we
  // could call instead.
  Label switch_to_optimized_code;

  Register optimized_code_entry = r6;
  __ LoadP(r2, FieldMemOperand(r3, JSFunction::kFeedbackVectorOffset));
  __ LoadP(r2, FieldMemOperand(r2, Cell::kValueOffset));
  __ LoadP(
      optimized_code_entry,
      FieldMemOperand(r2, FeedbackVector::kOptimizedCodeIndex * kPointerSize +
                              FeedbackVector::kHeaderSize));
  __ LoadP(optimized_code_entry,
           FieldMemOperand(optimized_code_entry, WeakCell::kValueOffset));
  __ JumpIfNotSmi(optimized_code_entry, &switch_to_optimized_code);

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ LoadP(r2, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  Label array_done;
  Register debug_info = r4;
  DCHECK(!debug_info.is(r2));
  __ LoadP(debug_info,
           FieldMemOperand(r2, SharedFunctionInfo::kDebugInfoOffset));
  // Load original bytecode array or the debug copy.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(r2, SharedFunctionInfo::kFunctionDataOffset));
  __ TestIfSmi(debug_info);
  __ beq(&array_done);
  __ LoadP(kInterpreterBytecodeArrayRegister,
           FieldMemOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex));
  __ bind(&array_done);

  // Check whether we should continue to use the interpreter.
  // TODO(rmcilroy) Remove self healing once liveedit only has to deal with
  // Ignition bytecode.
  Label switch_to_different_code_kind;
  __ LoadP(r2, FieldMemOperand(r2, SharedFunctionInfo::kCodeOffset));
  __ CmpP(r2, Operand(masm->CodeObject()));  // Self-reference to this code.
  __ bne(&switch_to_different_code_kind);

  // Increment invocation count for the function.
  __ LoadP(r6, FieldMemOperand(r3, JSFunction::kFeedbackVectorOffset));
  __ LoadP(r6, FieldMemOperand(r6, Cell::kValueOffset));
  __ LoadP(r1, FieldMemOperand(
                   r6, FeedbackVector::kInvocationCountIndex * kPointerSize +
                           FeedbackVector::kHeaderSize));
  __ AddSmiLiteral(r1, r1, Smi::FromInt(1), r0);
  __ StoreP(r1, FieldMemOperand(
                    r6, FeedbackVector::kInvocationCountIndex * kPointerSize +
                            FeedbackVector::kHeaderSize));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ TestIfSmi(kInterpreterBytecodeArrayRegister);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r2, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Reset code age.
  __ mov(r1, Operand(BytecodeArray::kNoAgeBytecodeAge));
  __ StoreByte(r1, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                   BytecodeArray::kBytecodeAgeOffset),
               r0);

  // Load the initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push new.target, bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r4, kInterpreterBytecodeOffsetRegister);
  __ Push(r5, kInterpreterBytecodeArrayRegister, r4);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size (word) from the BytecodeArray object.
    __ LoadlW(r4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                  BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ SubP(r5, sp, r4);
    __ LoadRoot(r0, Heap::kRealStackLimitRootIndex);
    __ CmpLogicalP(r5, r0);
    __ bge(&ok);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
    __ ShiftRightP(r4, r4, Operand(kPointerSizeLog2));
    __ LoadAndTestP(r4, r4);
    __ beq(&no_args);
    __ LoadRR(r1, r4);
    __ bind(&loop);
    __ push(r5);
    __ SubP(r1, Operand(1));
    __ bne(&loop);
    __ bind(&no_args);
  }

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ LoadlB(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftP(ip, r3, Operand(kPointerSizeLog2));
  __ LoadP(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
  __ Call(ip);

  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in r2.
  LeaveInterpreterFrame(masm, r4);
  __ Ret();

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kCodeOffset));
  __ AddP(r6, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(r6, FieldMemOperand(r3, JSFunction::kCodeEntryOffset), r0);
  __ RecordWriteCodeEntryField(r3, r6, r7);
  __ JumpToJSEntry(r6);

  // If there is optimized code on the type feedback vector, check if it is good
  // to run, and if so, self heal the closure and call the optimized code.
  __ bind(&switch_to_optimized_code);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  Label gotta_call_runtime;

  // Check if the optimized code is marked for deopt.
  __ LoadlW(r7, FieldMemOperand(optimized_code_entry,
                                Code::kKindSpecificFlags1Offset));
  __ And(r0, r7, Operand(1 << Code::kMarkedForDeoptimizationBit));
  __ bne(&gotta_call_runtime);

  // Optimized code is good, get it into the closure and link the closure into
  // the optimized functions list, then tail call the optimized code.
  ReplaceClosureEntryWithOptimizedCode(masm, optimized_code_entry, r3, r8, r7,
                                       r4);
  __ JumpToJSEntry(optimized_code_entry);

  // Optimized code is marked for deopt, bailout to the CompileLazy runtime
  // function which will clear the feedback vector's optimized code slot.
  __ bind(&gotta_call_runtime);
  GenerateTailCallToReturnedCode(masm, Runtime::kEvictOptimizedCodeSlot);
}

static void Generate_StackOverflowCheck(MacroAssembler* masm, Register num_args,
                                        Register scratch,
                                        Label* stack_overflow) {
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(scratch, Heap::kRealStackLimitRootIndex);
  // Make scratch the space we have left. The stack might already be overflowed
  // here which will cause scratch to become negative.
  __ SubP(scratch, sp, scratch);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftP(r0, num_args, Operand(kPointerSizeLog2));
  __ CmpP(scratch, r0);
  __ ble(stack_overflow);  // Signed comparison.
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register count, Register scratch) {
  Label loop;
  __ AddP(index, index, Operand(kPointerSize));  // Bias up for LoadPU
  __ LoadRR(r0, count);
  __ bind(&loop);
  __ LoadP(scratch, MemOperand(index, -kPointerSize));
  __ lay(index, MemOperand(index, -kPointerSize));
  __ push(scratch);
  __ SubP(r0, Operand(1));
  __ bne(&loop);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    TailCallMode tail_call_mode, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r4 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r3 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  // Calculate number of arguments (AddP one for receiver).
  __ AddP(r5, r2, Operand(1));
  Generate_StackOverflowCheck(masm, r5, ip, &stack_overflow);

  // Push "undefined" as the receiver arg if we need to.
  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ LoadRR(r5, r2);  // Argument count is correct.
  }

  // Push the arguments.
  Generate_InterpreterPushArgs(masm, r5, r4, r5, r6);

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
    // Unreachable Code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructImpl(
    MacroAssembler* masm, InterpreterPushArgsMode mode) {
  // ----------- S t a t e -------------
  // -- r2 : argument count (not including receiver)
  // -- r5 : new target
  // -- r3 : constructor to call
  // -- r4 : allocation site feedback if available, undefined otherwise.
  // -- r6 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver to be constructed.
  __ LoadImmP(r0, Operand::Zero());
  __ push(r0);

  // Push the arguments (skip if none).
  Label skip;
  __ CmpP(r2, Operand::Zero());
  __ beq(&skip);
  Generate_StackOverflowCheck(masm, r2, ip, &stack_overflow);
  Generate_InterpreterPushArgs(masm, r2, r6, r2, r7);
  __ bind(&skip);

  __ AssertUndefinedOrAllocationSite(r4, r7);
  if (mode == InterpreterPushArgsMode::kJSFunction) {
    __ AssertFunction(r3);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
    __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kConstructStubOffset));
    // Jump to the construct function.
    __ AddP(ip, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(ip);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with r2, r3, and r5 unmodified.
    __ Jump(masm->isolate()->builtins()->ConstructWithSpread(),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with r2, r3, and r5 unmodified.
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable Code.
    __ bkpt(0);
  }
}

// static
void Builtins::Generate_InterpreterPushArgsThenConstructArray(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  // -- r2 : argument count (not including receiver)
  // -- r3 : target to call verified to be Array function
  // -- r4 : allocation site feedback if available, undefined otherwise.
  // -- r5 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver to be constructed.
  __ LoadImmP(r0, Operand::Zero());
  __ push(r0);

  Generate_StackOverflowCheck(masm, r2, ip, &stack_overflow);

  // Push the arguments. r6, r8, r3 will be modified.
  Generate_InterpreterPushArgs(masm, r6, r5, r2, r7);

  // Array constructor expects constructor in r5. It is same as r3 here.
  __ LoadRR(r5, r3);

  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);

  __ bind(&stack_overflow);
  {
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable Code.
    __ bkpt(0);
  }
}

static void Generate_InterpreterEnterBytecode(MacroAssembler* masm) {
  // Set the return address to the correct point in the interpreter entry
  // trampoline.
  Smi* interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::kZero);
  __ Move(r4, masm->isolate()->builtins()->InterpreterEntryTrampoline());
  __ AddP(r14, r4, Operand(interpreter_entry_return_pc_offset->value() +
                           Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ LoadP(kInterpreterBytecodeArrayRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ TestIfSmi(kInterpreterBytecodeArrayRegister);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r3, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ LoadP(kInterpreterBytecodeOffsetRegister,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ LoadlB(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftP(ip, r3, Operand(kPointerSizeLog2));
  __ LoadP(ip, MemOperand(kInterpreterDispatchTableRegister, ip));
  __ Jump(ip);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ LoadP(r3, MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadP(r4,
           MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister, r3, r4);
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ Move(r4, r2);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ StoreP(r4,
            MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argument count (preserved for callee)
  //  -- r5 : new target (preserved for callee)
  //  -- r3 : target function (preserved for callee)
  // -----------------------------------
  // First lookup code, maybe we don't need to compile!
  Label gotta_call_runtime;
  Label try_shared;

  Register closure = r3;
  Register index = r4;

  // Do we have a valid feedback vector?
  __ LoadP(index, FieldMemOperand(closure, JSFunction::kFeedbackVectorOffset));
  __ LoadP(index, FieldMemOperand(index, Cell::kValueOffset));
  __ JumpIfRoot(index, Heap::kUndefinedValueRootIndex, &gotta_call_runtime);

  // Is optimized code available in the feedback vector?
  Register entry = r6;
  __ LoadP(entry, FieldMemOperand(index, FeedbackVector::kOptimizedCodeIndex *
                                                 kPointerSize +
                                             FeedbackVector::kHeaderSize));
  __ LoadP(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found code, check if it is marked for deopt, if so call into runtime to
  // clear the optimized code slot.
  __ LoadlW(r7, FieldMemOperand(entry, Code::kKindSpecificFlags1Offset));
  __ And(r0, r7, Operand(1 << Code::kMarkedForDeoptimizationBit));
  __ bne(&gotta_call_runtime);

  // Code is good, get it into the closure and tail call.
  ReplaceClosureEntryWithOptimizedCode(masm, entry, closure, r8, r7, r4);
  __ JumpToJSEntry(entry);

  // We found no optimized code.
  __ bind(&try_shared);
  __ LoadP(entry,
           FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ LoadlB(r7, FieldMemOperand(
                    entry, SharedFunctionInfo::kMarkedForTierUpByteOffset));
  __ TestBit(r7, SharedFunctionInfo::kMarkedForTierUpBitWithinByte, r0);
  __ bne(&gotta_call_runtime);

  // If SFI points to anything other than CompileLazy, install that.
  __ LoadP(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ mov(r7, Operand(masm->CodeObject()));
  __ CmpP(entry, r7);
  __ beq(&gotta_call_runtime);

  // Install the SFI's code entry.
  __ AddP(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ StoreP(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset), r0);
  __ RecordWriteCodeEntryField(closure, entry, r7);
  __ JumpToJSEntry(entry);

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
  //  -- r2 : argument count (preserved for callee)
  //  -- r3 : new target (preserved for callee)
  //  -- r5 : target function (preserved for callee)
  // -----------------------------------
  Label failed;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Preserve argument count for later compare.
    __ Move(r6, r2);
    // Push a copy of the target function and the new target.
    __ SmiTag(r2);
    // Push another copy as a parameter to the runtime call.
    __ Push(r2, r3, r5, r3);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ CmpP(r6, Operand(j));
        __ b(ne, &over);
      }
      for (int i = j - 1; i >= 0; --i) {
        __ LoadP(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                        i * kPointerSize));
        __ push(r6);
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
    __ JumpIfSmi(r2, &failed);

    __ Drop(2);
    __ pop(r6);
    __ SmiUntag(r6);
    scope.GenerateLeaveFrame();

    __ AddP(r6, r6, Operand(1));
    __ Drop(r6);
    __ Ret();

    __ bind(&failed);
    // Restore target function and new target.
    __ Pop(r2, r3, r5);
    __ SmiUntag(r2);
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

  // Point r2 at the start of the PlatformCodeAge sequence.
  __ CleanseP(r14);
  __ SubP(r14, Operand(kCodeAgingSequenceLength));
  __ LoadRR(r2, r14);

  __ pop(r14);

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r2 - contains return address (beginning of patch sequence)
  //   r3 - isolate
  //   r5 - new target
  //   lr - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(r14.bit() | r2.bit() | r3.bit() | r5.bit() | fp.bit());
  __ PrepareCallCFunction(2, 0, r4);
  __ mov(r3, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  __ MultiPop(r14.bit() | r2.bit() | r3.bit() | r5.bit() | fp.bit());
  __ LoadRR(ip, r2);
  __ Jump(ip);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                              \
  void Builtins::Generate_Make##C##CodeYoungAgain(MacroAssembler* masm) { \
    GenerateMakeCodeYoungAgainCommon(masm);                               \
  }
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR

void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Point r2 at the start of the PlatformCodeAge sequence.
  __ CleanseP(r14);
  __ SubP(r14, Operand(kCodeAgingSequenceLength));
  __ LoadRR(r2, r14);

  __ pop(r14);

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r2 - contains return address (beginning of patch sequence)
  //   r3 - isolate
  //   r5 - new target
  //   lr - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ MultiPush(r14.bit() | r2.bit() | r3.bit() | r5.bit() | fp.bit());
  __ PrepareCallCFunction(2, 0, r4);
  __ mov(r3, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ CallCFunction(
      ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
      2);
  __ MultiPop(r14.bit() | r2.bit() | r3.bit() | r5.bit() | fp.bit());
  __ LoadRR(ip, r2);

  // Perform prologue operations usually performed by the young code stub.
  __ PushStandardFrame(r3);

  // Jump to point after the code-age stub.
  __ AddP(r2, ip, Operand(kNoCodeAgeSequenceLength));
  __ Jump(r2);
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

  __ la(sp, MemOperand(sp, kPointerSize));  // Ignore state
  __ Ret();                                 // Jump to miss handler
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
    __ LoadSmiLiteral(r2, Smi::FromInt(static_cast<int>(type)));
    __ push(r2);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  // Get the full codegen state from the stack and untag it -> r8.
  __ LoadP(r8, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(r8);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ CmpP(
      r8,
      Operand(static_cast<intptr_t>(Deoptimizer::BailoutState::NO_REGISTERS)));
  __ bne(&with_tos_register);
  __ la(sp, MemOperand(sp, 1 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&with_tos_register);
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), r2.code());
  __ LoadP(r2, MemOperand(sp, 1 * kPointerSize));
  __ CmpP(
      r8,
      Operand(static_cast<intptr_t>(Deoptimizer::BailoutState::TOS_REGISTER)));
  __ bne(&unknown_state);
  __ la(sp, MemOperand(sp, 2 * kPointerSize));  // Remove state.
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

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ LoadP(r2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ LoadP(r2, MemOperand(r2, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ LoadP(r2, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  }

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r2);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  // If the code object is null, just return to the caller.
  Label skip;
  __ CmpSmiLiteral(r2, Smi::kZero, r0);
  __ bne(&skip);
  __ Ret();

  __ bind(&skip);

  // Drop any potential handler frame that is be sitting on top of the actual
  // JavaScript frame. This is the case then OSR is triggered from bytecode.
  if (has_handler_frame) {
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadP(r3, FieldMemOperand(r2, Code::kDeoptimizationDataOffset));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ LoadP(
      r3, FieldMemOperand(r3, FixedArray::OffsetOfElementAt(
                                  DeoptimizationInputData::kOsrPcOffsetIndex)));
  __ SmiUntag(r3);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ AddP(r2, r3);
  __ AddP(r0, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ LoadRR(r14, r0);

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
  //  -- r2    : argc
  //  -- sp[0] : argArray
  //  -- sp[4] : thisArg
  //  -- sp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into r3, argArray into r2 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label skip;
    Register arg_size = r4;
    Register new_sp = r5;
    Register scratch = r6;
    __ ShiftLeftP(arg_size, r2, Operand(kPointerSizeLog2));
    __ AddP(new_sp, sp, arg_size);
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ LoadRR(scratch, r2);
    __ LoadP(r3, MemOperand(new_sp, 0));  // receiver
    __ CmpP(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 1 * -kPointerSize));  // thisArg
    __ beq(&skip);
    __ LoadP(r2, MemOperand(new_sp, 2 * -kPointerSize));  // argArray
    __ bind(&skip);
    __ LoadRR(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r2    : argArray
  //  -- r3    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(r3, &receiver_not_callable);
  __ LoadP(r6, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadlB(r6, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r6, Map::kIsCallable);
  __ beq(&receiver_not_callable);

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(r2, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(r2, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ LoadImmP(r2, Operand::Zero());
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    __ StoreP(r3, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r2: actual number of arguments
  {
    Label done;
    __ CmpP(r2, Operand::Zero());
    __ bne(&done, Label::kNear);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ AddP(r2, Operand(1));
    __ bind(&done);
  }

  // r2: actual number of arguments
  // 2. Get the callable to call (passed as receiver) from the stack.
  __ ShiftLeftP(r4, r2, Operand(kPointerSizeLog2));
  __ LoadP(r3, MemOperand(sp, r4));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r2: actual number of arguments
  // r3: callable
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ AddP(r4, sp, r4);

    __ bind(&loop);
    __ LoadP(ip, MemOperand(r4, -kPointerSize));
    __ StoreP(ip, MemOperand(r4));
    __ SubP(r4, Operand(kPointerSize));
    __ CmpP(r4, sp);
    __ bne(&loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ SubP(r2, Operand(1));
    __ pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r3 (if present), argumentsList into r2 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label skip;
    Register arg_size = r4;
    Register new_sp = r5;
    Register scratch = r6;
    __ ShiftLeftP(arg_size, r2, Operand(kPointerSizeLog2));
    __ AddP(new_sp, sp, arg_size);
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ LoadRR(scratch, r3);
    __ LoadRR(r2, r3);
    __ CmpP(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r3, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ beq(&skip);
    __ LoadP(scratch, MemOperand(new_sp, 2 * -kPointerSize));  // thisArgument
    __ CmpP(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r2, MemOperand(new_sp, 3 * -kPointerSize));  // argumentsList
    __ bind(&skip);
    __ LoadRR(sp, new_sp);
    __ StoreP(scratch, MemOperand(sp, 0));
  }

  // ----------- S t a t e -------------
  //  -- r2    : argumentsList
  //  -- r3    : target
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(r3, &target_not_callable);
  __ LoadP(r6, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadlB(r6, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r6, Map::kIsCallable);
  __ beq(&target_not_callable);

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    __ StoreP(r3, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into r3 (if present), argumentsList into r2 (if present),
  // new.target into r5 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label skip;
    Register arg_size = r4;
    Register new_sp = r6;
    __ ShiftLeftP(arg_size, r2, Operand(kPointerSizeLog2));
    __ AddP(new_sp, sp, arg_size);
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ LoadRR(r2, r3);
    __ LoadRR(r5, r3);
    __ StoreP(r3, MemOperand(new_sp, 0));  // receiver (undefined)
    __ CmpP(arg_size, Operand(kPointerSize));
    __ blt(&skip);
    __ LoadP(r3, MemOperand(new_sp, 1 * -kPointerSize));  // target
    __ LoadRR(r5, r3);  // new.target defaults to target
    __ beq(&skip);
    __ LoadP(r2, MemOperand(new_sp, 2 * -kPointerSize));  // argumentsList
    __ CmpP(arg_size, Operand(2 * kPointerSize));
    __ beq(&skip);
    __ LoadP(r5, MemOperand(new_sp, 3 * -kPointerSize));  // new.target
    __ bind(&skip);
    __ LoadRR(sp, new_sp);
  }

  // ----------- S t a t e -------------
  //  -- r2    : argumentsList
  //  -- r5    : new.target
  //  -- r3    : target
  //  -- sp[0] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(r3, &target_not_constructor);
  __ LoadP(r6, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadlB(r6, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r6, Map::kIsConstructor);
  __ beq(&target_not_constructor);

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(r5, &new_target_not_constructor);
  __ LoadP(r6, FieldMemOperand(r5, HeapObject::kMapOffset));
  __ LoadlB(r6, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r6, Map::kIsConstructor);
  __ beq(&new_target_not_constructor);

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    __ StoreP(r3, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    __ StoreP(r5, MemOperand(sp, 0));
    __ TailCallRuntime(Runtime::kThrowNotConstructor);
  }
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(r2);
  __ Load(r6, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  // Stack updated as such:
  //    old SP --->
  //                 R14 Return Addr
  //                 Old FP                     <--- New FP
  //                 Argument Adapter SMI
  //                 Function
  //                 ArgC as SMI                <--- New SP
  __ lay(sp, MemOperand(sp, -5 * kPointerSize));

  // Cleanse the top nibble of 31-bit pointers.
  __ CleanseP(r14);
  __ StoreP(r14, MemOperand(sp, 4 * kPointerSize));
  __ StoreP(fp, MemOperand(sp, 3 * kPointerSize));
  __ StoreP(r6, MemOperand(sp, 2 * kPointerSize));
  __ StoreP(r3, MemOperand(sp, 1 * kPointerSize));
  __ StoreP(r2, MemOperand(sp, 0 * kPointerSize));
  __ la(fp, MemOperand(sp, StandardFrameConstants::kFixedFrameSizeFromFp +
                               kPointerSize));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ LoadP(r3, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                                kPointerSize)));
  int stack_adjustment = kPointerSize;  // adjust for receiver
  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR, stack_adjustment);
  __ SmiToPtrArrayOffset(r3, r3);
  __ lay(sp, MemOperand(sp, r3));
}

// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : argumentsList
  //  -- r3    : target
  //  -- r5    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(r2, &create_runtime);

    // Load the map of argumentsList into r4.
    __ LoadP(r4, FieldMemOperand(r2, HeapObject::kMapOffset));

    // Load native context into r6.
    __ LoadP(r6, NativeContextMemOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ LoadP(ip, ContextMemOperand(r6, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ CmpP(ip, r4);
    __ beq(&create_arguments);
    __ LoadP(ip, ContextMemOperand(r6, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ CmpP(ip, r4);
    __ beq(&create_arguments);

    // Check if argumentsList is a fast JSArray.
    __ CompareInstanceType(r4, ip, JS_ARRAY_TYPE);
    __ beq(&create_array);

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r3, r5, r2);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ Pop(r3, r5);
      __ LoadP(r4, FieldMemOperand(r2, FixedArray::kLengthOffset));
      __ SmiUntag(r4);
    }
    __ b(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ LoadP(r4, FieldMemOperand(r2, JSArgumentsObject::kLengthOffset));
    __ LoadP(r6, FieldMemOperand(r2, JSObject::kElementsOffset));
    __ LoadP(ip, FieldMemOperand(r6, FixedArray::kLengthOffset));
    __ CmpP(r4, ip);
    __ bne(&create_runtime);
    __ SmiUntag(r4);
    __ LoadRR(r2, r6);
    __ b(&done_create);

    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ bind(&create_holey_array);
    __ LoadP(r4, FieldMemOperand(r4, Map::kPrototypeOffset));
    __ LoadP(r6, ContextMemOperand(r6, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ CmpP(r4, r6);
    __ bne(&create_runtime);
    __ LoadRoot(r6, Heap::kArrayProtectorRootIndex);
    __ LoadP(r4, FieldMemOperand(r6, PropertyCell::kValueOffset));
    __ CmpSmiLiteral(r4, Smi::FromInt(Isolate::kProtectorValid), r0);
    __ bne(&create_runtime);
    __ LoadP(r4, FieldMemOperand(r2, JSArray::kLengthOffset));
    __ LoadP(r2, FieldMemOperand(r2, JSArray::kElementsOffset));
    __ SmiUntag(r4);
    __ b(&done_create);

    // Try to create the list from a JSArray object.
    // -- r4 and r6 must be preserved till bne create_holey_array.
    __ bind(&create_array);
    __ LoadlB(r7, FieldMemOperand(r4, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(r7);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    __ CmpP(r7, Operand(FAST_HOLEY_ELEMENTS));
    __ bgt(&create_runtime);
    // Only FAST_XXX after this point, FAST_HOLEY_XXX are odd values.
    __ TestBit(r7, Map::kHasNonInstancePrototype, r0);
    __ bne(&create_holey_array);
    // FAST_SMI_ELEMENTS or FAST_ELEMENTS after this point.
    __ LoadP(r4, FieldMemOperand(r2, JSArray::kLengthOffset));
    __ LoadP(r2, FieldMemOperand(r2, JSArray::kElementsOffset));
    __ SmiUntag(r4);

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(ip, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ SubP(ip, sp, ip);
    // Check if the arguments will overflow the stack.
    __ ShiftLeftP(r0, r4, Operand(kPointerSizeLog2));
    __ CmpP(ip, r0);  // Signed comparison.
    __ bgt(&done);
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- r3    : target
  //  -- r2    : args (a FixedArray built from argumentsList)
  //  -- r4    : len (number of elements to push from args)
  //  -- r5    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ LoadRoot(r8, Heap::kUndefinedValueRootIndex);
    Label loop, no_args, skip;
    __ CmpP(r4, Operand::Zero());
    __ beq(&no_args);
    __ AddP(r2, r2,
            Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
    __ LoadRR(r1, r4);
    __ bind(&loop);
    __ LoadP(ip, MemOperand(r2, kPointerSize));
    __ la(r2, MemOperand(r2, kPointerSize));
    __ CompareRoot(ip, Heap::kTheHoleValueRootIndex);
    __ bne(&skip, Label::kNear);
    __ LoadRR(ip, r8);
    __ bind(&skip);
    __ push(ip);
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);
    __ LoadRR(r2, r4);
  }

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    __ CompareRoot(r5, Heap::kUndefinedValueRootIndex);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET, eq);
    __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_ForwardVarargs(MacroAssembler* masm,
                                       Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r5 : the new.target (for [[Construct]] calls)
  //  -- r3 : the target to call (can be any Object)
  //  -- r4 : start index (to support rest parameters)
  // -----------------------------------

  // Check if we have an arguments adaptor frame below the function frame.
  Label arguments_adaptor, arguments_done;
  __ LoadP(r6, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(ip, MemOperand(r6, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ CmpP(ip, Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ beq(&arguments_adaptor);
  {
    __ LoadP(r7, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
    __ LoadP(r7, FieldMemOperand(r7, JSFunction::kSharedFunctionInfoOffset));
    __ LoadW(r7, FieldMemOperand(
                     r7, SharedFunctionInfo::kFormalParameterCountOffset));
    __ LoadRR(r6, fp);
  }
  __ b(&arguments_done);
  __ bind(&arguments_adaptor);
  {
    // Load the length from the ArgumentsAdaptorFrame.
    __ LoadP(r7, MemOperand(r6, ArgumentsAdaptorFrameConstants::kLengthOffset));
#if V8_TARGET_ARCH_S390X
    __ SmiUntag(r7);
#endif
  }
  __ bind(&arguments_done);

  Label stack_done, stack_overflow;
#if !V8_TARGET_ARCH_S390X
  __ SmiUntag(r7);
#endif
  __ SubP(r7, r7, r4);
  __ CmpP(r7, Operand::Zero());
  __ ble(&stack_done);
  {
    // Check for stack overflow.
    Generate_StackOverflowCheck(masm, r7, r4, &stack_overflow);

    // Forward the arguments from the caller frame.
    {
      Label loop;
      __ AddP(r6, r6, Operand(kPointerSize));
      __ AddP(r2, r2, r7);
      __ bind(&loop);
      {
        __ ShiftLeftP(ip, r7, Operand(kPointerSizeLog2));
        __ LoadP(ip, MemOperand(r6, ip));
        __ push(ip);
        __ SubP(r7, r7, Operand(1));
        __ CmpP(r7, Operand::Zero());
        __ bne(&loop);
      }
    }
  }
  __ b(&stack_done);
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

  // Prepare for tail call only if ES2015 tail call elimination is active.
  Label done;
  ExternalReference is_tail_call_elimination_enabled =
      ExternalReference::is_tail_call_elimination_enabled_address(
          masm->isolate());
  __ mov(scratch1, Operand(is_tail_call_elimination_enabled));
  __ LoadlB(scratch1, MemOperand(scratch1));
  __ CmpP(scratch1, Operand::Zero());
  __ beq(&done);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ LoadP(scratch3,
             MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ CmpP(scratch3, Operand(StackFrame::TypeToMarker(StackFrame::STUB)));
    __ bne(&no_interpreter_frame);
    __ LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ LoadP(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(
      scratch3,
      MemOperand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ CmpP(scratch3,
          Operand(StackFrame::TypeToMarker(StackFrame::ARGUMENTS_ADAPTOR)));
  __ bne(&no_arguments_adaptor);

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ LoadRR(fp, scratch2);
  __ LoadP(caller_args_count_reg,
           MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ b(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ LoadP(scratch1,
           MemOperand(fp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  __ LoadP(scratch1,
           FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ LoadW(caller_args_count_reg,
           FieldMemOperand(scratch1,
                           SharedFunctionInfo::kFormalParameterCountOffset));
#if !V8_TARGET_ARCH_S390X
  __ SmiUntag(caller_args_count_reg);
#endif

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
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(r3);

  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ LoadP(r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadlW(r5, FieldMemOperand(r4, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestBitMask(r5, FunctionKind::kClassConstructor
                         << SharedFunctionInfo::kFunctionKindShift,
                 r0);
  __ bne(&class_constructor);

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ AndP(r0, r5, Operand((1 << SharedFunctionInfo::kStrictModeBit) |
                          (1 << SharedFunctionInfo::kNativeBit)));
  __ bne(&done_convert);
  {
    // ----------- S t a t e -------------
    //  -- r2 : the number of arguments (not including the receiver)
    //  -- r3 : the function to call (checked to be a JSFunction)
    //  -- r4 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r5);
    } else {
      Label convert_to_object, convert_receiver;
      __ ShiftLeftP(r5, r2, Operand(kPointerSizeLog2));
      __ LoadP(r5, MemOperand(sp, r5));
      __ JumpIfSmi(r5, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r5, r6, r6, FIRST_JS_RECEIVER_TYPE);
      __ bge(&done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r5, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy);
        __ JumpIfNotRoot(r5, Heap::kNullValueRootIndex, &convert_to_object);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(r5);
        }
        __ b(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
        __ SmiTag(r2);
        __ Push(r2, r3);
        __ LoadRR(r2, r5);
        __ Push(cp);
        __ Call(masm->isolate()->builtins()->ToObject(),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ LoadRR(r5, r2);
        __ Pop(r2, r3);
        __ SmiUntag(r2);
      }
      __ LoadP(r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ ShiftLeftP(r6, r2, Operand(kPointerSizeLog2));
    __ StoreP(r5, MemOperand(sp, r6));
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSFunction)
  //  -- r4 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r2, r5, r6, r7);
  }

  __ LoadW(
      r4, FieldMemOperand(r4, SharedFunctionInfo::kFormalParameterCountOffset));
#if !V8_TARGET_ARCH_S390X
  __ SmiUntag(r4);
#endif
  ParameterCount actual(r2);
  ParameterCount expected(r4);
  __ InvokeFunctionCode(r3, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameAndConstantPoolScope frame(masm, StackFrame::INTERNAL);
    __ push(r3);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : target (checked to be a JSBoundFunction)
  //  -- r5 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into r4 and length of that into r6.
  Label no_bound_arguments;
  __ LoadP(r4, FieldMemOperand(r3, JSBoundFunction::kBoundArgumentsOffset));
  __ LoadP(r6, FieldMemOperand(r4, FixedArray::kLengthOffset));
  __ SmiUntag(r6);
  __ LoadAndTestP(r6, r6);
  __ beq(&no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- r2 : the number of arguments (not including the receiver)
    //  -- r3 : target (checked to be a JSBoundFunction)
    //  -- r4 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r5 : new.target (only in case of [[Construct]])
    //  -- r6 : the number of [[BoundArguments]]
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ LoadRR(r8, sp);  // preserve previous stack pointer
      __ ShiftLeftP(r9, r6, Operand(kPointerSizeLog2));
      __ SubP(sp, sp, r9);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(sp, Heap::kRealStackLimitRootIndex);
      __ bgt(&done);  // Signed comparison.
      // Restore the stack pointer.
      __ LoadRR(sp, r8);
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Relocate arguments down the stack.
    //  -- r2 : the number of arguments (not including the receiver)
    //  -- r8 : the previous stack pointer
    //  -- r9: the size of the [[BoundArguments]]
    {
      Label skip, loop;
      __ LoadImmP(r7, Operand::Zero());
      __ CmpP(r2, Operand::Zero());
      __ beq(&skip);
      __ LoadRR(r1, r2);
      __ bind(&loop);
      __ LoadP(r0, MemOperand(r8, r7));
      __ StoreP(r0, MemOperand(sp, r7));
      __ AddP(r7, r7, Operand(kPointerSize));
      __ BranchOnCount(r1, &loop);
      __ bind(&skip);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ AddP(r4, r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
      __ AddP(r4, r4, r9);
      __ LoadRR(r1, r6);
      __ bind(&loop);
      __ LoadP(r0, MemOperand(r4, -kPointerSize));
      __ lay(r4, MemOperand(r4, -kPointerSize));
      __ StoreP(r0, MemOperand(sp, r7));
      __ AddP(r7, r7, Operand(kPointerSize));
      __ BranchOnCount(r1, &loop);
      __ AddP(r2, r2, r6);
    }
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(r3);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r2, r5, r6, r7);
  }

  // Patch the receiver to [[BoundThis]].
  __ LoadP(ip, FieldMemOperand(r3, JSBoundFunction::kBoundThisOffset));
  __ ShiftLeftP(r1, r2, Operand(kPointerSizeLog2));
  __ StoreP(ip, MemOperand(sp, r1));

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadP(r3,
           FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ mov(ip, Operand(ExternalReference(Builtins::kCall_ReceiverIsAny,
                                       masm->isolate())));
  __ LoadP(ip, MemOperand(ip));
  __ AddP(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the target to call (can be any Object).
  // -----------------------------------

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(r3, &non_callable);
  __ bind(&non_smi);
  __ CompareObjectType(r3, r6, r7, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
          RelocInfo::CODE_TARGET, eq);
  __ CmpP(r7, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  __ LoadlB(r6, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r6, Map::kIsCallable);
  __ beq(&non_callable);

  __ CmpP(r7, Operand(JS_PROXY_TYPE));
  __ bne(&non_function);

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, r2, r5, r6, r7);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ Push(r3);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ AddP(r2, r2, Operand(2));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver the (original) target.
  __ ShiftLeftP(r7, r2, Operand(kPointerSizeLog2));
  __ StoreP(r3, MemOperand(sp, r7));
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, r3);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void CheckSpreadAndPushToStack(MacroAssembler* masm) {
  Register argc = r2;
  Register constructor = r3;
  Register new_target = r5;

  Register scratch = r4;
  Register scratch2 = r8;

  Register spread = r6;
  Register spread_map = r7;
  Register spread_len = r7;
  Label runtime_call, push_args;
  __ LoadP(spread, MemOperand(sp, 0));
  __ JumpIfSmi(spread, &runtime_call);
  __ LoadP(spread_map, FieldMemOperand(spread, HeapObject::kMapOffset));

  // Check that the spread is an array.
  __ CompareInstanceType(spread_map, scratch, JS_ARRAY_TYPE);
  __ bne(&runtime_call);

  // Check that we have the original ArrayPrototype.
  __ LoadP(scratch, FieldMemOperand(spread_map, Map::kPrototypeOffset));
  __ LoadP(scratch2, NativeContextMemOperand());
  __ LoadP(scratch2,
           ContextMemOperand(scratch2, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
  __ CmpP(scratch, scratch2);
  __ bne(&runtime_call);

  // Check that the ArrayPrototype hasn't been modified in a way that would
  // affect iteration.
  __ LoadRoot(scratch, Heap::kArrayIteratorProtectorRootIndex);
  __ LoadP(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ CmpSmiLiteral(scratch, Smi::FromInt(Isolate::kProtectorValid), r0);
  __ bne(&runtime_call);

  // Check that the map of the initial array iterator hasn't changed.
  __ LoadP(scratch2, NativeContextMemOperand());
  __ LoadP(scratch,
           ContextMemOperand(scratch2,
                             Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_INDEX));
  __ LoadP(scratch, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ LoadP(scratch2,
           ContextMemOperand(
               scratch2, Context::INITIAL_ARRAY_ITERATOR_PROTOTYPE_MAP_INDEX));
  __ CmpP(scratch, scratch2);
  __ bne(&runtime_call);

  // For FastPacked kinds, iteration will have the same effect as simply
  // accessing each property in order.
  Label no_protector_check;
  __ LoadlB(scratch, FieldMemOperand(spread_map, Map::kBitField2Offset));
  __ DecodeField<Map::ElementsKindBits>(scratch);
  __ CmpP(scratch, Operand(FAST_HOLEY_ELEMENTS));
  __ bgt(&runtime_call);
  // For non-FastHoley kinds, we can skip the protector check.
  __ CmpP(scratch, Operand(FAST_SMI_ELEMENTS));
  __ beq(&no_protector_check);
  __ CmpP(scratch, Operand(FAST_ELEMENTS));
  __ beq(&no_protector_check);
  // Check the ArrayProtector cell.
  __ LoadRoot(scratch, Heap::kArrayProtectorRootIndex);
  __ LoadP(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ CmpSmiLiteral(scratch, Smi::FromInt(Isolate::kProtectorValid), r0);
  __ bne(&runtime_call);

  __ bind(&no_protector_check);
  // Load the FixedArray backing store, but use the length from the array.
  __ LoadP(spread_len, FieldMemOperand(spread, JSArray::kLengthOffset));
  __ SmiUntag(spread_len);
  __ LoadP(spread, FieldMemOperand(spread, JSArray::kElementsOffset));
  __ b(&push_args);

  __ bind(&runtime_call);
  {
    // Call the builtin for the result of the spread.
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ SmiTag(argc);
    __ Push(constructor, new_target, argc, spread);
    __ CallRuntime(Runtime::kSpreadIterableFixed);
    __ LoadRR(spread, r2);
    __ Pop(constructor, new_target, argc);
    __ SmiUntag(argc);
  }

  {
    // Calculate the new nargs including the result of the spread.
    __ LoadP(spread_len, FieldMemOperand(spread, FixedArray::kLengthOffset));
    __ SmiUntag(spread_len);

    __ bind(&push_args);
    // argc += spread_len - 1. Subtract 1 for the spread itself.
    __ AddP(argc, argc, spread_len);
    __ SubP(argc, argc, Operand(1));

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
    __ SubP(scratch, sp, scratch);
    // Check if the arguments will overflow the stack.
    __ ShiftLeftP(r0, spread_len, Operand(kPointerSizeLog2));
    __ CmpP(scratch, r0);
    __ bgt(&done);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // Put the evaluated spread onto the stack as additional arguments.
  {
    __ LoadImmP(scratch, Operand::Zero());
    Label done, push, loop;
    __ bind(&loop);
    __ CmpP(scratch, spread_len);
    __ beq(&done);
    __ ShiftLeftP(r0, scratch, Operand(kPointerSizeLog2));
    __ AddP(scratch2, spread, r0);
    __ LoadP(scratch2, FieldMemOperand(scratch2, FixedArray::kHeaderSize));
    __ JumpIfNotRoot(scratch2, Heap::kTheHoleValueRootIndex, &push);
    __ LoadRoot(scratch2, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(scratch2);
    __ AddP(scratch, scratch, Operand(1));
    __ b(&loop);
    __ bind(&done);
  }
}

// static
void Builtins::Generate_CallWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the constructor to call (can be any Object)
  // -----------------------------------

  // CheckSpreadAndPushToStack will push r5 to save it.
  __ LoadRoot(r5, Heap::kUndefinedValueRootIndex);
  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            TailCallMode::kDisallow),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the constructor to call (checked to be a JSFunction)
  //  -- r5 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertFunction(r3);

  // Calling convention for function specific ConstructStubs require
  // r4 to contain either an AllocationSite or undefined.
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ LoadP(r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(r6, FieldMemOperand(r6, SharedFunctionInfo::kConstructStubOffset));
  __ AddP(ip, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the function to call (checked to be a JSBoundFunction)
  //  -- r5 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertBoundFunction(r3);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  Label skip;
  __ CmpP(r3, r5);
  __ bne(&skip);
  __ LoadP(r5,
           FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ bind(&skip);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadP(r3,
           FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ mov(ip, Operand(ExternalReference(Builtins::kConstruct, masm->isolate())));
  __ LoadP(ip, MemOperand(ip));
  __ AddP(ip, ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}

// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the constructor to call (checked to be a JSProxy)
  //  -- r5 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ Push(r3, r5);
  // Include the pushed new_target, constructor and the receiver.
  __ AddP(r2, r2, Operand(3));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the constructor to call (can be any Object)
  //  -- r5 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(r3, &non_constructor);

  // Dispatch based on instance type.
  __ CompareObjectType(r3, r6, r7, JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Construct]] internal method.
  __ LoadlB(r4, FieldMemOperand(r6, Map::kBitFieldOffset));
  __ TestBit(r4, Map::kIsConstructor);
  __ beq(&non_constructor);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpP(r7, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructBoundFunction(),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpP(r7, Operand(JS_PROXY_TYPE));
  __ Jump(masm->isolate()->builtins()->ConstructProxy(), RelocInfo::CODE_TARGET,
          eq);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ ShiftLeftP(r7, r2, Operand(kPointerSizeLog2));
    __ StoreP(r3, MemOperand(sp, r7));
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, r3);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(masm->isolate()->builtins()->ConstructedNonConstructable(),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ConstructWithSpread(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments (not including the receiver)
  //  -- r3 : the constructor to call (can be any Object)
  //  -- r5 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------

  CheckSpreadAndPushToStack(masm);
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_AllocateInNewSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r3);
  __ Push(r3);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInNewSpace);
}

// static
void Builtins::Generate_AllocateInOldSpace(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : requested object size (untagged)
  //  -- lr : return address
  // -----------------------------------
  __ SmiTag(r3);
  __ LoadSmiLiteral(r4, Smi::FromInt(AllocateTargetSpace::encode(OLD_SPACE)));
  __ Push(r3, r4);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAllocateInTargetSpace);
}

// static
void Builtins::Generate_Abort(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3 : message_id as Smi
  //  -- lr : return address
  // -----------------------------------
  __ push(r3);
  __ LoadSmiLiteral(cp, Smi::kZero);
  __ TailCallRuntime(Runtime::kAbort);
}

void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : actual number of arguments
  //  -- r3 : function (passed through to callee)
  //  -- r4 : expected number of arguments
  //  -- r5 : new target (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;

  Label enough, too_few;
  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeEntryOffset));
  __ CmpP(r2, r4);
  __ blt(&too_few);
  __ CmpP(r4, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ beq(&dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r4, r7, &stack_overflow);

    // Calculate copy start address into r2 and copy end address into r6.
    // r2: actual number of arguments as a smi
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r2, r2);
    __ AddP(r2, fp);
    // adjust for return address and receiver
    __ AddP(r2, r2, Operand(2 * kPointerSize));
    __ ShiftLeftP(r6, r4, Operand(kPointerSizeLog2));
    __ SubP(r6, r2, r6);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r2: copy start address
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // r6: copy end address
    // ip: code entry to call

    Label copy;
    __ bind(&copy);
    __ LoadP(r0, MemOperand(r2, 0));
    __ push(r0);
    __ CmpP(r2, r6);  // Compare before moving to next argument.
    __ lay(r2, MemOperand(r2, -kPointerSize));
    __ bne(&copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);

    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, r4, r7, &stack_overflow);

    // Calculate copy start address into r0 and copy end address is fp.
    // r2: actual number of arguments as a smi
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r2, r2);
    __ lay(r2, MemOperand(r2, fp));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r2: copy start address
    // r3: function
    // r4: expected number of arguments
    // r5: new target (passed through to callee)
    // ip: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ LoadP(r0, MemOperand(r2, 2 * kPointerSize));
    __ push(r0);
    __ CmpP(r2, fp);  // Compare before moving to next argument.
    __ lay(r2, MemOperand(r2, -kPointerSize));
    __ bne(&copy);

    // Fill the remaining expected arguments with undefined.
    // r3: function
    // r4: expected number of argumentus
    // ip: code entry to call
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    __ ShiftLeftP(r6, r4, Operand(kPointerSizeLog2));
    __ SubP(r6, fp, r6);
    // Adjust for frame.
    __ SubP(r6, r6, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(r0);
    __ CmpP(sp, r6);
    __ bne(&fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ LoadRR(r2, r4);
  // r2 : expected number of arguments
  // r3 : function (passed through to callee)
  // r5 : new target (passed through to callee)
  __ CallJSEntry(ip);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ JumpToJSEntry(ip);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);
  }
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Save all parameter registers (see wasm-linkage.cc). They might be
    // overwritten in the runtime call below. We don't have any callee-saved
    // registers in wasm, so no need to store anything else.
    const RegList gp_regs =
        r2.bit() | r3.bit() | r4.bit() | r5.bit() | r6.bit();
#if V8_TARGET_ARCH_S390X
    const RegList fp_regs = d0.bit() | d2.bit() | d4.bit() | d6.bit();
#else
    const RegList fp_regs = d0.bit() | d2.bit();
#endif
    __ MultiPush(gp_regs);
    __ MultiPushDoubles(fp_regs);

    // Initialize cp register with kZero, CEntryStub will use it to set the
    // current context on the isolate.
    __ LoadSmiLiteral(cp, Smi::kZero);
    __ CallRuntime(Runtime::kWasmCompileLazy);
    // Store returned instruction start in ip.
    __ AddP(ip, r2, Operand(Code::kHeaderSize - kHeapObjectTag));

    // Restore registers.
    __ MultiPopDoubles(fp_regs);
    __ MultiPop(gp_regs);
  }
  // Now jump to the instructions of the returned code object.
  __ Jump(ip);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

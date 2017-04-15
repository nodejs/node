// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

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
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * agrc]       : receiver
  // -----------------------------------
  __ AssertFunction(a1);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // JumpToExternalReference expects a0 to contain the number of arguments
  // including the receiver and the extra arguments.
  const int num_extra_args = 3;
  __ Addu(a0, a0, num_extra_args + 1);

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
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, t0);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction, t0,
              Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction, t0,
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
    __ lw(a2, FieldMemOperand(a1, JSFunction::kPrototypeOrInitialMapOffset));
    __ SmiTst(a2, t0);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction1, t0,
              Operand(zero_reg));
    __ GetObjectType(a2, a3, t0);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction2, t0,
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
void Builtins::Generate_MathMaxMin(MacroAssembler* masm, MathMaxMinKind kind) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : function
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------
  Heap::RootListIndex const root_index =
      (kind == MathMaxMinKind::kMin) ? Heap::kInfinityValueRootIndex
                                     : Heap::kMinusInfinityValueRootIndex;

  // Load the accumulator with the default return value (either -Infinity or
  // +Infinity), with the tagged value in t2 and the double value in f0.
  __ LoadRoot(t2, root_index);
  __ ldc1(f0, FieldMemOperand(t2, HeapNumber::kValueOffset));

  Label done_loop, loop, done;
  __ mov(a3, a0);
  __ bind(&loop);
  {
    // Check if all parameters done.
    __ Subu(a3, a3, Operand(1));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));

    // Load the next parameter tagged value into a2.
    __ Lsa(at, sp, a3, kPointerSizeLog2);
    __ lw(a2, MemOperand(at));

    // Load the double value of the parameter into f2, maybe converting the
    // parameter to a number first using the ToNumber builtin if necessary.
    Label convert, convert_smi, convert_number, done_convert;
    __ bind(&convert);
    __ JumpIfSmi(a2, &convert_smi);
    __ lw(t0, FieldMemOperand(a2, HeapObject::kMapOffset));
    __ JumpIfRoot(t0, Heap::kHeapNumberMapRootIndex, &convert_number);
    {
      // Parameter is not a Number, use the ToNumber builtin to convert it.
      FrameScope scope(masm, StackFrame::MANUAL);
      __ SmiTag(a0);
      __ SmiTag(a3);
      __ EnterBuiltinFrame(cp, a1, a0);
      __ Push(t2, a3);
      __ mov(a0, a2);
      __ Call(masm->isolate()->builtins()->ToNumber(), RelocInfo::CODE_TARGET);
      __ mov(a2, v0);
      __ Pop(t2, a3);
      __ LeaveBuiltinFrame(cp, a1, a0);
      __ SmiUntag(a3);
      __ SmiUntag(a0);
      {
        // Restore the double accumulator value (f0).
        Label restore_smi, done_restore;
        __ JumpIfSmi(t2, &restore_smi);
        __ ldc1(f0, FieldMemOperand(t2, HeapNumber::kValueOffset));
        __ jmp(&done_restore);
        __ bind(&restore_smi);
        __ SmiToDoubleFPURegister(t2, f0, t0);
        __ bind(&done_restore);
      }
    }
    __ jmp(&convert);
    __ bind(&convert_number);
    __ ldc1(f2, FieldMemOperand(a2, HeapNumber::kValueOffset));
    __ jmp(&done_convert);
    __ bind(&convert_smi);
    __ SmiToDoubleFPURegister(a2, f2, t0);
    __ bind(&done_convert);

    // Perform the actual comparison with using Min/Max macro instructions the
    // accumulator value on the left hand side (f0) and the next parameter value
    // on the right hand side (f2).
    // We need to work out which HeapNumber (or smi) the result came from.
    Label compare_nan, set_value, ool_min, ool_max;
    __ BranchF(nullptr, &compare_nan, eq, f0, f2);
    __ Move(t0, t1, f0);
    if (kind == MathMaxMinKind::kMin) {
      __ Float64Min(f0, f0, f2, &ool_min);
    } else {
      DCHECK(kind == MathMaxMinKind::kMax);
      __ Float64Max(f0, f0, f2, &ool_max);
    }
    __ jmp(&done);

    __ bind(&ool_min);
    __ Float64MinOutOfLine(f0, f0, f2);
    __ jmp(&done);

    __ bind(&ool_max);
    __ Float64MaxOutOfLine(f0, f0, f2);

    __ bind(&done);
    __ Move(at, t8, f0);
    __ Branch(&set_value, ne, t0, Operand(at));
    __ Branch(&set_value, ne, t1, Operand(t8));
    __ jmp(&loop);
    __ bind(&set_value);
    __ mov(t2, a2);
    __ jmp(&loop);

    // At least one side is NaN, which means that the result will be NaN too.
    __ bind(&compare_nan);
    __ LoadRoot(t2, Heap::kNanValueRootIndex);
    __ ldc1(f0, FieldMemOperand(t2, HeapNumber::kValueOffset));
    __ jmp(&loop);
  }

  __ bind(&done_loop);
  // Drop all slots, including the receiver.
  __ Addu(a0, a0, Operand(1));
  __ Lsa(sp, sp, a0, kPointerSizeLog2);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, t2);  // In delay slot.
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0.
  Label no_arguments;
  {
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Subu(t1, a0, Operand(1));  // In delay slot.
    __ mov(t0, a0);               // Store argc in t0.
    __ Lsa(at, sp, t1, kPointerSizeLog2);
    __ lw(a0, MemOperand(at));
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
    __ Lsa(sp, sp, t0, kPointerSizeLog2);
    __ DropAndRet(1);
  }

  // 2b. No arguments, return +0.
  __ bind(&no_arguments);
  __ Move(v0, Smi::kZero);
  __ DropAndRet(1);
}

// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- a3                     : new target
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // 2. Load the first argument into a0.
  {
    Label no_arguments, done;
    __ mov(t0, a0);  // Store argc in t0.
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Subu(t1, a0, Operand(1));  // In delay slot.
    __ Lsa(at, sp, t1, kPointerSizeLog2);
    __ lw(a0, MemOperand(at));
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
    __ Push(a0);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(a0);
    __ LeaveBuiltinFrame(cp, a1, t0);
    __ SmiUntag(t0);
  }
  __ sw(a0, FieldMemOperand(v0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Lsa(sp, sp, t0, kPointerSizeLog2);
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
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Load the first argument into a0.
  Label no_arguments;
  {
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Subu(t1, a0, Operand(1));
    __ mov(t0, a0);  // Store argc in t0.
    __ Lsa(at, sp, t1, kPointerSizeLog2);
    __ lw(a0, MemOperand(at));
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
    __ Lsa(sp, sp, t0, kPointerSizeLog2);
    __ Drop(1);
    __ Push(a0);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }

  __ bind(&drop_frame_and_ret);
  {
    __ Lsa(sp, sp, t0, kPointerSizeLog2);
    __ DropAndRet(1);
  }
}

// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                     : number of arguments
  //  -- a1                     : constructor function
  //  -- a3                     : new target
  //  -- cp                     : context
  //  -- ra                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  // 2. Load the first argument into a0.
  {
    Label no_arguments, done;
    __ mov(t0, a0);  // Store argc in t0.
    __ Branch(USE_DELAY_SLOT, &no_arguments, eq, a0, Operand(zero_reg));
    __ Subu(t1, a0, Operand(1));
    __ Lsa(at, sp, t1, kPointerSizeLog2);
    __ lw(a0, MemOperand(at));
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
    __ Push(a0);  // first argument
    __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
            RelocInfo::CODE_TARGET);
    __ Pop(a0);
    __ LeaveBuiltinFrame(cp, a1, t0);
    __ SmiUntag(t0);
  }
  __ sw(a0, FieldMemOperand(v0, JSValue::kValueOffset));

  __ bind(&drop_frame_and_ret);
  {
    __ Lsa(sp, sp, t0, kPointerSizeLog2);
    __ DropAndRet(1);
  }
}

static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a2, FieldMemOperand(a2, SharedFunctionInfo::kCodeOffset));
  __ Addu(at, a2, Operand(Code::kHeaderSize - kHeapObjectTag));
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
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    __ CallRuntime(function_id, 1);

    // Restore target function and new target.
    __ Pop(a0, a1, a3);
    __ SmiUntag(a0);
  }

  __ Addu(at, v0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);
}

void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ LoadRoot(t0, Heap::kStackLimitRootIndex);
  __ Branch(&ok, hs, sp, Operand(t0));

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}

namespace {

void Generate_JSConstructStubHelper(MacroAssembler* masm, bool is_api_function,
                                    bool create_implicit_receiver,
                                    bool check_derived_construct) {
  // ----------- S t a t e -------------
  //  -- a0     : number of arguments
  //  -- a1     : constructor function
  //  -- a3     : new target
  //  -- cp     : context
  //  -- ra     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(a0);
    __ Push(cp, a0);

    if (create_implicit_receiver) {
      // Allocate the new receiver object.
      __ Push(a1, a3);
      __ Call(CodeFactory::FastNewObject(masm->isolate()).code(),
              RelocInfo::CODE_TARGET);
      __ mov(t4, v0);
      __ Pop(a1, a3);

      // ----------- S t a t e -------------
      //  -- a1: constructor function
      //  -- a3: new target
      //  -- t0: newly allocated object
      // -----------------------------------

      // Retrieve smi-tagged arguments count from the stack.
      __ lw(a0, MemOperand(sp));
    }

    __ SmiUntag(a0);

    if (create_implicit_receiver) {
      // Push the allocated receiver to the stack. We need two copies
      // because we may have to return the original one and the calling
      // conventions dictate that the called function pops the receiver.
      __ Push(t4, t4);
    } else {
      __ PushRoot(Heap::kTheHoleValueRootIndex);
    }

    // Set up pointer to last argument.
    __ Addu(a2, fp, Operand(StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    // a0: number of arguments
    // a1: constructor function
    // a2: address of last argument (caller sp)
    // a3: new target
    // t4: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: number of arguments (smi-tagged)
    Label loop, entry;
    __ SmiTag(t4, a0);
    __ jmp(&entry);
    __ bind(&loop);
    __ Lsa(t0, a2, t4, kPointerSizeLog2 - kSmiTagSize);
    __ lw(t1, MemOperand(t0));
    __ push(t1);
    __ bind(&entry);
    __ Addu(t4, t4, Operand(-2));
    __ Branch(&loop, greater_equal, t4, Operand(zero_reg));

    // Call the function.
    // a0: number of arguments
    // a1: constructor function
    // a3: new target
    ParameterCount actual(a0);
    __ InvokeFunction(a1, a3, actual, CALL_FUNCTION,
                      CheckDebugStepCallWrapper());

    // Store offset of return address for deoptimizer.
    if (create_implicit_receiver && !is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ lw(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));

    if (create_implicit_receiver) {
      // If the result is an object (in the ECMA sense), we should get rid
      // of the receiver and use the result; see ECMA-262 section 13.2.2-7
      // on page 74.
      Label use_receiver, exit;

      // If the result is a smi, it is *not* an object in the ECMA sense.
      // v0: result
      // sp[0]: receiver (newly allocated object)
      // sp[1]: number of arguments (smi-tagged)
      __ JumpIfSmi(v0, &use_receiver);

      // If the type of the result (stored in its map) is less than
      // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
      __ GetObjectType(v0, a1, a3);
      __ Branch(&exit, greater_equal, a3, Operand(FIRST_JS_RECEIVER_TYPE));

      // Throw away the result of the constructor invocation and use the
      // on-stack receiver as the result.
      __ bind(&use_receiver);
      __ lw(v0, MemOperand(sp));

      // Remove receiver from the stack, remove caller arguments, and
      // return.
      __ bind(&exit);
      // v0: result
      // sp[0]: receiver (newly allocated object)
      // sp[1]: number of arguments (smi-tagged)
      __ lw(a1, MemOperand(sp, 1 * kPointerSize));
    } else {
      __ lw(a1, MemOperand(sp));
    }

    // Leave construct frame.
  }

  // ES6 9.2.2. Step 13+
  // Check that the result is not a Smi, indicating that the constructor result
  // from a derived class is neither undefined nor an Object.
  if (check_derived_construct) {
    Label dont_throw;
    __ JumpIfNotSmi(v0, &dont_throw);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowDerivedConstructorReturnedNonObject);
    }
    __ bind(&dont_throw);
  }

  __ Lsa(sp, sp, a1, kPointerSizeLog2 - 1);
  __ Addu(sp, sp, kPointerSize);
  if (create_implicit_receiver) {
    __ IncrementCounter(isolate->counters()->constructed_objects(), 1, a1, a2);
  }
  __ Ret();
}

}  // namespace

void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, true, false);
}

void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false, false);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false, false);
}

void Builtins::Generate_JSBuiltinsConstructStubForDerived(
    MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, false, true);
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
  // here which will cause a2 to become negative.
  __ Subu(a2, sp, a2);
  // Check if the arguments will overflow the stack.
  if (argc_is_tagged == kArgcIsSmiTagged) {
    __ sll(t3, argc, kPointerSizeLog2 - kSmiTagSize);
  } else {
    DCHECK(argc_is_tagged == kArgcIsUntaggedInt);
    __ sll(t3, argc, kPointerSizeLog2);
  }
  // Signed comparison.
  __ Branch(&okay, gt, a2, Operand(t3));

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
    __ lw(cp, MemOperand(cp));

    // Push the function and the receiver onto the stack.
    __ Push(a1, a2);

    // Check if we have enough stack space to push all arguments.
    // Clobbers a2.
    Generate_CheckStackOverflow(masm, a3, kArgcIsUntaggedInt);

    // Remember new.target.
    __ mov(t1, a0);

    // Copy arguments to the stack in a loop.
    // a3: argc
    // s0: argv, i.e. points to first arg
    Label loop, entry;
    __ Lsa(t2, s0, a3, kPointerSizeLog2);
    __ b(&entry);
    __ nop();  // Branch delay slot nop.
    // t2 points past last arg.
    __ bind(&loop);
    __ lw(t0, MemOperand(s0));  // Read next parameter.
    __ addiu(s0, s0, kPointerSize);
    __ lw(t0, MemOperand(t0));  // Dereference handle.
    __ push(t0);                // Push parameter.
    __ bind(&entry);
    __ Branch(&loop, ne, s0, Operand(t2));

    // Setup new.target and argc.
    __ mov(a0, a3);
    __ mov(a3, t1);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ mov(s1, t0);
    __ mov(s2, t0);
    __ mov(s3, t0);
    __ mov(s4, t0);
    __ mov(s5, t0);
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

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : the value to pass to the generator
  //  -- a1 : the JSGeneratorObject to resume
  //  -- a2 : the resume mode (tagged)
  //  -- ra : return address
  // -----------------------------------
  __ AssertGeneratorObject(a1);

  // Store input value into generator object.
  __ sw(v0, FieldMemOperand(a1, JSGeneratorObject::kInputOrDebugPosOffset));
  __ RecordWriteField(a1, JSGeneratorObject::kInputOrDebugPosOffset, v0, a3,
                      kRAHasNotBeenSaved, kDontSaveFPRegs);

  // Store resume mode into generator object.
  __ sw(a2, FieldMemOperand(a1, JSGeneratorObject::kResumeModeOffset));

  // Load suspended function and context.
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
  __ lw(cp, FieldMemOperand(t0, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ li(t1, Operand(debug_hook));
  __ lb(t1, MemOperand(t1));
  __ Branch(&prepare_step_in_if_stepping, ne, t1, Operand(zero_reg));

  // Flood function if we need to continue stepping in the suspended generator.
  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());
  __ li(t1, Operand(debug_suspended_generator));
  __ lw(t1, MemOperand(t1));
  __ Branch(&prepare_step_in_suspended_generator, eq, a1, Operand(t1));
  __ bind(&stepping_prepared);

  // Push receiver.
  __ lw(t1, FieldMemOperand(a1, JSGeneratorObject::kReceiverOffset));
  __ Push(t1);

  // ----------- S t a t e -------------
  //  -- a1    : the JSGeneratorObject to resume
  //  -- a2    : the resume mode (tagged)
  //  -- t0    : generator function
  //  -- cp    : generator context
  //  -- ra    : return address
  //  -- sp[0] : generator receiver
  // -----------------------------------

  // Push holes for arguments to generator function. Since the parser forced
  // context allocation for any variables in generators, the actual argument
  // values have already been copied into the context and these dummy values
  // will never be used.
  __ lw(a3, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
  __ lw(a3,
        FieldMemOperand(a3, SharedFunctionInfo::kFormalParameterCountOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ Subu(a3, a3, Operand(Smi::FromInt(1)));
    __ Branch(&done_loop, lt, a3, Operand(zero_reg));
    __ PushRoot(Heap::kTheHoleValueRootIndex);
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Underlying function needs to have bytecode available.
  if (FLAG_debug_code) {
    __ lw(a3, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a3, FieldMemOperand(a3, SharedFunctionInfo::kFunctionDataOffset));
    __ GetObjectType(a3, a3, a3);
    __ Assert(eq, kMissingBytecodeArray, a3, Operand(BYTECODE_ARRAY_TYPE));
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ lw(a0, FieldMemOperand(t0, JSFunction::kSharedFunctionInfoOffset));
    __ lw(a0,
          FieldMemOperand(a0, SharedFunctionInfo::kFormalParameterCountOffset));
    __ SmiUntag(a0);
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ Move(a3, a1);
    __ Move(a1, t0);
    __ lw(a2, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
    __ Jump(a2);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a2, t0);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(a1, a2);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(a1, a2);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(a1, a2);
  }
  __ Branch(USE_DELAY_SLOT, &stepping_prepared);
  __ lw(t0, FieldMemOperand(a1, JSGeneratorObject::kFunctionOffset));
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch) {
  Register args_count = scratch;

  // Get the arguments + receiver count.
  __ lw(args_count,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(args_count,
        FieldMemOperand(args_count, BytecodeArray::kParameterSizeOffset));

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);

  // Drop receiver + arguments.
  __ Addu(sp, sp, args_count);
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

  // Get the bytecode array from the function object (or from the DebugInfo if
  // it is present) and load it into kInterpreterBytecodeArrayRegister.
  __ lw(a0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  Label load_debug_bytecode_array, bytecode_array_loaded;
  Register debug_info = kInterpreterBytecodeArrayRegister;
  DCHECK(!debug_info.is(a0));
  __ lw(debug_info, FieldMemOperand(a0, SharedFunctionInfo::kDebugInfoOffset));
  __ Branch(&load_debug_bytecode_array, ne, debug_info,
            Operand(DebugInfo::uninitialized()));
  __ lw(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(a0, SharedFunctionInfo::kFunctionDataOffset));
  __ bind(&bytecode_array_loaded);

  // Check whether we should continue to use the interpreter.
  Label switch_to_different_code_kind;
  __ lw(a0, FieldMemOperand(a0, SharedFunctionInfo::kCodeOffset));
  __ Branch(&switch_to_different_code_kind, ne, a0,
            Operand(masm->CodeObject()));  // Self-reference to this code.

  // Increment invocation count for the function.
  __ lw(a0, FieldMemOperand(a1, JSFunction::kLiteralsOffset));
  __ lw(a0, FieldMemOperand(a0, LiteralsArray::kFeedbackVectorOffset));
  __ lw(t0, FieldMemOperand(
                a0, FeedbackVector::kInvocationCountIndex * kPointerSize +
                        FeedbackVector::kHeaderSize));
  __ Addu(t0, t0, Operand(Smi::FromInt(1)));
  __ sw(t0, FieldMemOperand(
                a0, FeedbackVector::kInvocationCountIndex * kPointerSize +
                        FeedbackVector::kHeaderSize));

  // Check function data field is actually a BytecodeArray object.
  if (FLAG_debug_code) {
    __ SmiTst(kInterpreterBytecodeArrayRegister, t0);
    __ Assert(ne, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, t0,
              Operand(zero_reg));
    __ GetObjectType(kInterpreterBytecodeArrayRegister, t0, t0);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry, t0,
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
  __ SmiTag(t0, kInterpreterBytecodeOffsetRegister);
  __ Push(a3, kInterpreterBytecodeArrayRegister, t0);

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ lw(t0, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ Subu(t1, sp, Operand(t0));
    __ LoadRoot(a2, Heap::kRealStackLimitRootIndex);
    __ Branch(&ok, hs, t1, Operand(a2));
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(t1, Heap::kUndefinedValueRootIndex);
    __ Branch(&loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ push(t1);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ Subu(t0, t0, Operand(kPointerSize));
    __ Branch(&loop_header, ge, t0, Operand(zero_reg));
  }

  // Load accumulator and dispatch table into registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ Addu(a0, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a0, MemOperand(a0));
  __ Lsa(at, kInterpreterDispatchTableRegister, a0, kPointerSizeLog2);
  __ lw(at, MemOperand(at));
  __ Call(at);
  masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(masm->pc_offset());

  // The return value is in v0.
  LeaveInterpreterFrame(masm, t0);
  __ Jump(ra);

  // Load debug copy of the bytecode array.
  __ bind(&load_debug_bytecode_array);
  __ lw(kInterpreterBytecodeArrayRegister,
        FieldMemOperand(debug_info, DebugInfo::kDebugBytecodeArrayIndex));
  __ Branch(&bytecode_array_loaded);

  // If the shared code is no longer this entry trampoline, then the underlying
  // function has been switched to a different kind of code and we heal the
  // closure by switching the code entry field over to the new code as well.
  __ bind(&switch_to_different_code_kind);
  __ LeaveFrame(StackFrame::JAVA_SCRIPT);
  __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kCodeOffset));
  __ Addu(t0, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ sw(t0, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(a1, t0, t1);
  __ Jump(t0);
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
  __ subu(scratch1, sp, scratch1);
  // Check if the arguments will overflow the stack.
  __ sll(scratch2, num_args, kPointerSizeLog2);
  // Signed comparison.
  __ Branch(stack_overflow, le, scratch1, Operand(scratch2));
}

static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         Register num_args, Register index,
                                         Register scratch, Register scratch2,
                                         Label* stack_overflow) {
  Generate_StackOverflowCheck(masm, num_args, scratch, scratch2,
                              stack_overflow);

  // Find the address of the last argument.
  __ mov(scratch2, num_args);
  __ sll(scratch2, scratch2, kPointerSizeLog2);
  __ Subu(scratch2, index, Operand(scratch2));

  // Push the arguments.
  Label loop_header, loop_check;
  __ Branch(&loop_check);
  __ bind(&loop_header);
  __ lw(scratch, MemOperand(index));
  __ Addu(index, index, Operand(-kPointerSize));
  __ push(scratch);
  __ bind(&loop_check);
  __ Branch(&loop_header, gt, index, Operand(scratch2));
}

// static
void Builtins::Generate_InterpreterPushArgsAndCallImpl(
    MacroAssembler* masm, TailCallMode tail_call_mode,
    CallableType function_type) {
  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- a1 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;

  __ Addu(t0, a0, Operand(1));  // Add one for receiver.

  // This function modifies a2, t4 and t1.
  Generate_InterpreterPushArgs(masm, t0, a2, t4, t1, &stack_overflow);

  // Call the target.
  if (function_type == CallableType::kJSFunction) {
    __ Jump(masm->isolate()->builtins()->CallFunction(ConvertReceiverMode::kAny,
                                                      tail_call_mode),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(function_type, CallableType::kAny);
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
void Builtins::Generate_InterpreterPushArgsAndConstructImpl(
    MacroAssembler* masm, CallableType construct_type) {
  // ----------- S t a t e -------------
  // -- a0 : argument count (not including receiver)
  // -- a3 : new target
  // -- a1 : constructor to call
  // -- a2 : allocation site feedback if available, undefined otherwise.
  // -- t4 : address of the first argument
  // -----------------------------------
  Label stack_overflow;

  // Push a slot for the receiver.
  __ push(zero_reg);

  // This function modified t4, t1 and t0.
  Generate_InterpreterPushArgs(masm, a0, t4, t1, t0, &stack_overflow);

  __ AssertUndefinedOrAllocationSite(a2, t0);
  if (construct_type == CallableType::kJSFunction) {
    __ AssertFunction(a1);

    // Tail call to the function-specific construct stub (still in the caller
    // context at this point).
    __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
    __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kConstructStubOffset));
    __ Addu(at, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ Jump(at);
  } else {
    DCHECK_EQ(construct_type, CallableType::kAny);
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
void Builtins::Generate_InterpreterPushArgsAndConstructArray(
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

  __ Addu(t0, a0, Operand(1));  // Add one for receiver.

  // This function modifies a3, t4, and t1.
  Generate_InterpreterPushArgs(masm, t0, a3, t1, t4, &stack_overflow);

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
  __ Addu(ra, t0, Operand(interpreter_entry_return_pc_offset->value() +
                          Code::kHeaderSize - kHeapObjectTag));

  // Initialize the dispatch table register.
  __ li(kInterpreterDispatchTableRegister,
        Operand(ExternalReference::interpreter_dispatch_table_address(
            masm->isolate())));

  // Get the bytecode array pointer from the frame.
  __ lw(kInterpreterBytecodeArrayRegister,
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
  __ lw(kInterpreterBytecodeOffsetRegister,
        MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ Addu(a1, kInterpreterBytecodeArrayRegister,
          kInterpreterBytecodeOffsetRegister);
  __ lbu(a1, MemOperand(a1));
  __ Lsa(a1, kInterpreterDispatchTableRegister, a1, kPointerSizeLog2);
  __ lw(a1, MemOperand(a1));
  __ Jump(a1);
}

void Builtins::Generate_InterpreterEnterBytecodeAdvance(MacroAssembler* masm) {
  // Advance the current bytecode offset stored within the given interpreter
  // stack frame. This simulates what all bytecode handlers do upon completion
  // of the underlying operation.
  __ lw(a1, MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ lw(a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister, a1, a2);
    __ CallRuntime(Runtime::kInterpreterAdvanceBytecodeOffset);
    __ mov(a2, v0);  // Result is the new bytecode offset.
    __ Pop(kInterpreterAccumulatorRegister);
  }
  __ sw(a2, MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

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
  Label gotta_call_runtime, gotta_call_runtime_no_stack;
  Label try_shared;
  Label loop_top, loop_bottom;

  Register argument_count = a0;
  Register closure = a1;
  Register new_target = a3;
  __ push(argument_count);
  __ push(new_target);
  __ push(closure);

  Register map = a0;
  Register index = a2;
  __ lw(map, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ lw(map, FieldMemOperand(map, SharedFunctionInfo::kOptimizedCodeMapOffset));
  __ lw(index, FieldMemOperand(map, FixedArray::kLengthOffset));
  __ Branch(&gotta_call_runtime, lt, index, Operand(Smi::FromInt(2)));

  // Find literals.
  // a3  : native context
  // a2  : length / index
  // a0  : optimized code map
  // stack[0] : new target
  // stack[4] : closure
  Register native_context = a3;
  __ lw(native_context, NativeContextMemOperand());

  __ bind(&loop_top);
  Register temp = a1;
  Register array_pointer = t1;

  // Does the native context match?
  __ sll(at, index, kPointerSizeLog2 - kSmiTagSize);
  __ Addu(array_pointer, map, Operand(at));
  __ lw(temp, FieldMemOperand(array_pointer,
                              SharedFunctionInfo::kOffsetToPreviousContext));
  __ lw(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ Branch(&loop_bottom, ne, temp, Operand(native_context));
  // Literals available?
  __ lw(temp, FieldMemOperand(array_pointer,
                              SharedFunctionInfo::kOffsetToPreviousLiterals));
  __ lw(temp, FieldMemOperand(temp, WeakCell::kValueOffset));
  __ JumpIfSmi(temp, &gotta_call_runtime);

  // Save the literals in the closure.
  __ lw(t0, MemOperand(sp, 0));
  __ sw(temp, FieldMemOperand(t0, JSFunction::kLiteralsOffset));
  __ push(index);
  __ RecordWriteField(t0, JSFunction::kLiteralsOffset, temp, index,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(index);

  // Code available?
  Register entry = t0;
  __ lw(entry,
        FieldMemOperand(array_pointer,
                        SharedFunctionInfo::kOffsetToPreviousCachedCode));
  __ lw(entry, FieldMemOperand(entry, WeakCell::kValueOffset));
  __ JumpIfSmi(entry, &try_shared);

  // Found literals and code. Get them into the closure and return.
  __ pop(closure);
  // Store code entry in the closure.
  __ Addu(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ sw(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, t1);

  // Link the closure into the optimized function list.
  // t0 : code entry
  // a3 : native context
  // a1 : closure
  __ lw(t1,
        ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  __ sw(t1, FieldMemOperand(closure, JSFunction::kNextFunctionLinkOffset));
  __ RecordWriteField(closure, JSFunction::kNextFunctionLinkOffset, t1, a0,
                      kRAHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  const int function_list_offset =
      Context::SlotOffset(Context::OPTIMIZED_FUNCTIONS_LIST);
  __ sw(closure,
        ContextMemOperand(native_context, Context::OPTIMIZED_FUNCTIONS_LIST));
  // Save closure before the write barrier.
  __ mov(t1, closure);
  __ RecordWriteContextSlot(native_context, function_list_offset, closure, a0,
                            kRAHasNotBeenSaved, kDontSaveFPRegs);
  __ mov(closure, t1);
  __ pop(new_target);
  __ pop(argument_count);
  __ Jump(entry);

  __ bind(&loop_bottom);
  __ Subu(index, index,
          Operand(Smi::FromInt(SharedFunctionInfo::kEntryLength)));
  __ Branch(&loop_top, gt, index, Operand(Smi::FromInt(1)));

  // We found neither literals nor code.
  __ jmp(&gotta_call_runtime);

  __ bind(&try_shared);
  __ pop(closure);
  __ pop(new_target);
  __ pop(argument_count);
  __ lw(entry, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Is the shared function marked for tier up?
  __ lbu(t1, FieldMemOperand(entry,
                             SharedFunctionInfo::kMarkedForTierUpByteOffset));
  __ And(t1, t1,
         Operand(1 << SharedFunctionInfo::kMarkedForTierUpBitWithinByte));
  __ Branch(&gotta_call_runtime_no_stack, ne, t1, Operand(zero_reg));

  // If SFI points to anything other than CompileLazy, install that.
  __ lw(entry, FieldMemOperand(entry, SharedFunctionInfo::kCodeOffset));
  __ Move(t1, masm->CodeObject());
  __ Branch(&gotta_call_runtime_no_stack, eq, entry, Operand(t1));

  // Install the SFI's code entry.
  __ Addu(entry, entry, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ sw(entry, FieldMemOperand(closure, JSFunction::kCodeEntryOffset));
  __ RecordWriteCodeEntryField(closure, entry, t1);
  __ Jump(entry);

  __ bind(&gotta_call_runtime);
  __ pop(closure);
  __ pop(new_target);
  __ pop(argument_count);
  __ bind(&gotta_call_runtime_no_stack);
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}

void Builtins::Generate_CompileBaseline(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileBaseline);
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
    // Preserve argument count for later compare.
    __ Move(t4, a0);
    // Push a copy of the target function and the new target.
    // Push function as parameter to the runtime call.
    __ SmiTag(a0);
    __ Push(a0, a1, a3, a1);

    // Copy arguments from caller (stdlib, foreign, heap).
    Label args_done;
    for (int j = 0; j < 4; ++j) {
      Label over;
      if (j < 3) {
        __ Branch(&over, ne, t4, Operand(j));
      }
      for (int i = j - 1; i >= 0; --i) {
        __ lw(t4, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                     i * kPointerSize));
        __ push(t4);
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
    __ pop(t4);
    __ SmiUntag(t4);
    scope.GenerateLeaveFrame();

    __ Addu(t4, t4, Operand(1));
    __ Lsa(sp, sp, t4, kPointerSizeLog2);
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
  __ Subu(a0, a0, Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

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
  __ Subu(a0, a0, Operand(kNoCodeAgeSequenceLength - Assembler::kInstrSize));

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
  __ Addu(a0, a0, Operand(kNoCodeAgeSequenceLength));
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

  __ Addu(sp, sp, Operand(kPointerSize));  // Ignore state
  __ Jump(ra);                             // Jump to miss handler
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

  // Get the full codegen state from the stack and untag it -> t2.
  __ lw(t2, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(t2);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ Branch(&with_tos_register, ne, t2,
            Operand(static_cast<int>(Deoptimizer::BailoutState::NO_REGISTERS)));
  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Addu(sp, sp, Operand(1 * kPointerSize));  // Remove state.

  __ bind(&with_tos_register);
  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), v0.code());
  __ lw(v0, MemOperand(sp, 1 * kPointerSize));
  __ Branch(&unknown_state, ne, t2,
            Operand(static_cast<int>(Deoptimizer::BailoutState::TOS_REGISTER)));

  __ Ret(USE_DELAY_SLOT);
  // Safe to fill delay slot Addu will emit one instruction.
  __ Addu(sp, sp, Operand(2 * kPointerSize));  // Remove state.

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

// Clobbers {t2, t3, t4, t5}.
static void CompatibleReceiverCheck(MacroAssembler* masm, Register receiver,
                                    Register function_template_info,
                                    Label* receiver_check_failed) {
  Register signature = t2;
  Register map = t3;
  Register constructor = t4;
  Register scratch = t5;

  // If there is no signature, return the holder.
  __ lw(signature, FieldMemOperand(function_template_info,
                                   FunctionTemplateInfo::kSignatureOffset));
  Label receiver_check_passed;
  __ JumpIfRoot(signature, Heap::kUndefinedValueRootIndex,
                &receiver_check_passed);

  // Walk the prototype chain.
  __ lw(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  Label prototype_loop_start;
  __ bind(&prototype_loop_start);

  // Get the constructor, if any.
  __ GetMapConstructor(constructor, map, scratch, scratch);
  Label next_prototype;
  __ Branch(&next_prototype, ne, scratch, Operand(JS_FUNCTION_TYPE));
  Register type = constructor;
  __ lw(type,
        FieldMemOperand(constructor, JSFunction::kSharedFunctionInfoOffset));
  __ lw(type, FieldMemOperand(type, SharedFunctionInfo::kFunctionDataOffset));

  // Loop through the chain of inheriting function templates.
  Label function_template_loop;
  __ bind(&function_template_loop);

  // If the signatures match, we have a compatible receiver.
  __ Branch(&receiver_check_passed, eq, signature, Operand(type),
            USE_DELAY_SLOT);

  // If the current type is not a FunctionTemplateInfo, load the next prototype
  // in the chain.
  __ JumpIfSmi(type, &next_prototype);
  __ GetObjectType(type, scratch, scratch);
  __ Branch(&next_prototype, ne, scratch, Operand(FUNCTION_TEMPLATE_INFO_TYPE));

  // Otherwise load the parent function template and iterate.
  __ lw(type,
        FieldMemOperand(type, FunctionTemplateInfo::kParentTemplateOffset));
  __ Branch(&function_template_loop);

  // Load the next prototype and iterate.
  __ bind(&next_prototype);
  __ lw(scratch, FieldMemOperand(map, Map::kBitField3Offset));
  __ DecodeField<Map::HasHiddenPrototype>(scratch);
  __ Branch(receiver_check_failed, eq, scratch, Operand(zero_reg));
  __ lw(receiver, FieldMemOperand(map, Map::kPrototypeOffset));
  __ lw(map, FieldMemOperand(receiver, HeapObject::kMapOffset));

  __ Branch(&prototype_loop_start);

  __ bind(&receiver_check_passed);
}

void Builtins::Generate_HandleFastApiCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0                 : number of arguments excluding receiver
  //  -- a1                 : callee
  //  -- ra                 : return address
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------

  // Load the FunctionTemplateInfo.
  __ lw(t1, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(t1, FieldMemOperand(t1, SharedFunctionInfo::kFunctionDataOffset));

  // Do the compatible receiver check.
  Label receiver_check_failed;
  __ Lsa(t8, sp, a0, kPointerSizeLog2);
  __ lw(t0, MemOperand(t8));
  CompatibleReceiverCheck(masm, t0, t1, &receiver_check_failed);

  // Get the callback offset from the FunctionTemplateInfo, and jump to the
  // beginning of the code.
  __ lw(t2, FieldMemOperand(t1, FunctionTemplateInfo::kCallCodeOffset));
  __ lw(t2, FieldMemOperand(t2, CallHandlerInfo::kFastHandlerOffset));
  __ Addu(t2, t2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(t2);

  // Compatible receiver check failed: throw an Illegal Invocation exception.
  __ bind(&receiver_check_failed);
  // Drop the arguments (including the receiver);
  __ Addu(t8, t8, Operand(kPointerSize));
  __ addu(sp, t8, zero_reg);
  __ TailCallRuntime(Runtime::kThrowIllegalInvocation);
}

static void Generate_OnStackReplacementHelper(MacroAssembler* masm,
                                              bool has_handler_frame) {
  // Lookup the function in the JavaScript frame.
  if (has_handler_frame) {
    __ lw(a0, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ lw(a0, MemOperand(a0, JavaScriptFrameConstants::kFunctionOffset));
  } else {
    __ lw(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
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
  __ lw(a1, MemOperand(v0, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ lw(a1, MemOperand(a1, FixedArray::OffsetOfElementAt(
                               DeoptimizationInputData::kOsrPcOffsetIndex) -
                               kHeapObjectTag));
  __ SmiUntag(a1);

  // Compute the target address = code_obj + header_size + osr_offset
  // <entry_addr> = <code_obj> + #header_size + <osr_offset>
  __ addu(v0, v0, a1);
  __ addiu(ra, v0, Code::kHeaderSize - kHeapObjectTag);

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

  // 1. Load receiver into a1, argArray into a0 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a2, Heap::kUndefinedValueRootIndex);
    __ mov(a3, a2);
    // Lsa() cannot be used hare as scratch value used later.
    __ sll(scratch, a0, kPointerSizeLog2);
    __ Addu(a0, sp, Operand(scratch));
    __ lw(a1, MemOperand(a0));  // receiver
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // thisArg
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // argArray
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
    __ sw(a2, MemOperand(sp));
    __ mov(a0, a3);
  }

  // ----------- S t a t e -------------
  //  -- a0    : argArray
  //  -- a1    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(a1, &receiver_not_callable);
  __ lw(t0, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(t0, FieldMemOperand(t0, Map::kBitFieldOffset));
  __ And(t0, t0, Operand(1 << Map::kIsCallable));
  __ Branch(&receiver_not_callable, eq, t0, Operand(zero_reg));

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(a0, Heap::kNullValueRootIndex, &no_arguments);
  __ JumpIfRoot(a0, Heap::kUndefinedValueRootIndex, &no_arguments);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(a0, zero_reg);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    __ sw(a1, MemOperand(sp));
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
    __ Addu(a0, a0, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack.
  // a0: actual number of arguments
  __ Lsa(at, sp, a0, kPointerSizeLog2);
  __ lw(a1, MemOperand(at));

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // a0: actual number of arguments
  // a1: function
  {
    Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ Lsa(a2, sp, a0, kPointerSizeLog2);

    __ bind(&loop);
    __ lw(at, MemOperand(a2, -kPointerSize));
    __ sw(at, MemOperand(a2));
    __ Subu(a2, a2, Operand(kPointerSize));
    __ Branch(&loop, ne, a2, Operand(sp));
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ Subu(a0, a0, Operand(1));
    __ Pop();
  }

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : argumentsList
  //  -- sp[4]  : thisArgument
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ mov(a2, a1);
    __ mov(a3, a1);
    __ sll(scratch, a0, kPointerSizeLog2);
    __ mov(a0, scratch);
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(zero_reg));
    __ Addu(a0, sp, Operand(a0));
    __ lw(a1, MemOperand(a0));  // target
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // thisArgument
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // argumentsList
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
    __ sw(a2, MemOperand(sp));
    __ mov(a0, a3);
  }

  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a1    : target
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(a1, &target_not_callable);
  __ lw(t0, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(t0, FieldMemOperand(t0, Map::kBitFieldOffset));
  __ And(t0, t0, Operand(1 << Map::kIsCallable));
  __ Branch(&target_not_callable, eq, t0, Operand(zero_reg));

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(a3, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    __ sw(a1, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0     : argc
  //  -- sp[0]  : new.target (optional)
  //  -- sp[4]  : argumentsList
  //  -- sp[8]  : target
  //  -- sp[12] : receiver
  // -----------------------------------

  // 1. Load target into a1 (if present), argumentsList into a0 (if present),
  // new.target into a3 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label no_arg;
    Register scratch = t0;
    __ LoadRoot(a1, Heap::kUndefinedValueRootIndex);
    __ mov(a2, a1);
    // Lsa() cannot be used hare as scratch value used later.
    __ sll(scratch, a0, kPointerSizeLog2);
    __ Addu(a0, sp, Operand(scratch));
    __ sw(a2, MemOperand(a0));  // receiver
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a1, MemOperand(a0));  // target
    __ mov(a3, a1);             // new.target defaults to target
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a2, MemOperand(a0));  // argumentsList
    __ Subu(a0, a0, Operand(kPointerSize));
    __ Branch(&no_arg, lt, a0, Operand(sp));
    __ lw(a3, MemOperand(a0));  // new.target
    __ bind(&no_arg);
    __ Addu(sp, sp, Operand(scratch));
    __ mov(a0, a2);
  }

  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a3    : new.target
  //  -- a1    : target
  //  -- sp[0] : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(a1, &target_not_constructor);
  __ lw(t0, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(t0, FieldMemOperand(t0, Map::kBitFieldOffset));
  __ And(t0, t0, Operand(1 << Map::kIsConstructor));
  __ Branch(&target_not_constructor, eq, t0, Operand(zero_reg));

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(a3, &new_target_not_constructor);
  __ lw(t0, FieldMemOperand(a3, HeapObject::kMapOffset));
  __ lbu(t0, FieldMemOperand(t0, Map::kBitFieldOffset));
  __ And(t0, t0, Operand(1 << Map::kIsConstructor));
  __ Branch(&new_target_not_constructor, eq, t0, Operand(zero_reg));

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    __ sw(a1, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    __ sw(a3, MemOperand(sp));
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }
}

static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ sll(a0, a0, kSmiTagSize);
  __ li(t0, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ MultiPush(a0.bit() | a1.bit() | t0.bit() | fp.bit() | ra.bit());
  __ Addu(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                          kPointerSize));
}

static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- v0 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ lw(a1, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                             kPointerSize)));
  __ mov(sp, fp);
  __ MultiPop(fp.bit() | ra.bit());
  __ Lsa(sp, sp, a1, kPointerSizeLog2 - kSmiTagSize);
  // Adjust for the receiver.
  __ Addu(sp, sp, Operand(kPointerSize));
}

// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : argumentsList
  //  -- a1    : target
  //  -- a3    : new.target (checked to be constructor or undefined)
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_holey_array, create_runtime,
        done_create;
    __ JumpIfSmi(a0, &create_runtime);

    // Load the map of argumentsList into a2.
    __ lw(a2, FieldMemOperand(a0, HeapObject::kMapOffset));

    // Load native context into t0.
    __ lw(t0, NativeContextMemOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ lw(at, ContextMemOperand(t0, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ Branch(&create_arguments, eq, a2, Operand(at));
    __ lw(at, ContextMemOperand(t0, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ Branch(&create_arguments, eq, a2, Operand(at));

    // Check if argumentsList is a fast JSArray.
    __ lbu(v0, FieldMemOperand(a2, Map::kInstanceTypeOffset));
    __ Branch(&create_array, eq, v0, Operand(JS_ARRAY_TYPE));

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(a1, a3, a0);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ mov(a0, v0);
      __ Pop(a1, a3);
      __ lw(a2, FieldMemOperand(v0, FixedArray::kLengthOffset));
      __ SmiUntag(a2);
    }
    __ Branch(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ lw(a2, FieldMemOperand(a0, JSArgumentsObject::kLengthOffset));
    __ lw(t0, FieldMemOperand(a0, JSObject::kElementsOffset));
    __ lw(at, FieldMemOperand(t0, FixedArray::kLengthOffset));
    __ Branch(&create_runtime, ne, a2, Operand(at));
    __ SmiUntag(a2);
    __ mov(a0, t0);
    __ Branch(&done_create);

    // For holey JSArrays we need to check that the array prototype chain
    // protector is intact and our prototype is the Array.prototype actually.
    __ bind(&create_holey_array);
    __ lw(a2, FieldMemOperand(a2, Map::kPrototypeOffset));
    __ lw(at, ContextMemOperand(t0, Context::INITIAL_ARRAY_PROTOTYPE_INDEX));
    __ Branch(&create_runtime, ne, a2, Operand(at));
    __ LoadRoot(at, Heap::kArrayProtectorRootIndex);
    __ lw(a2, FieldMemOperand(at, PropertyCell::kValueOffset));
    __ Branch(&create_runtime, ne, a2,
              Operand(Smi::FromInt(Isolate::kProtectorValid)));
    __ lw(a2, FieldMemOperand(a0, JSArray::kLengthOffset));
    __ lw(a0, FieldMemOperand(a0, JSArray::kElementsOffset));
    __ SmiUntag(a2);
    __ Branch(&done_create);

    // Try to create the list from a JSArray object.
    __ bind(&create_array);
    __ lbu(t1, FieldMemOperand(a2, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(t1);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
    __ Branch(&create_holey_array, eq, t1, Operand(FAST_HOLEY_SMI_ELEMENTS));
    __ Branch(&create_holey_array, eq, t1, Operand(FAST_HOLEY_ELEMENTS));
    __ Branch(&create_runtime, hi, t1, Operand(FAST_ELEMENTS));
    __ lw(a2, FieldMemOperand(a0, JSArray::kLengthOffset));
    __ lw(a0, FieldMemOperand(a0, JSArray::kElementsOffset));
    __ SmiUntag(a2);

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(t0, Heap::kRealStackLimitRootIndex);
    // Make ip the space we have left. The stack might already be overflowed
    // here which will cause ip to become negative.
    __ Subu(t0, sp, t0);
    // Check if the arguments will overflow the stack.
    __ sll(at, a2, kPointerSizeLog2);
    __ Branch(&done, gt, t0, Operand(at));  // Signed comparison.
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
    __ mov(t0, zero_reg);
    Label done, push, loop;
    __ LoadRoot(t1, Heap::kTheHoleValueRootIndex);
    __ bind(&loop);
    __ Branch(&done, eq, t0, Operand(a2));
    __ Lsa(at, a0, t0, kPointerSizeLog2);
    __ lw(at, FieldMemOperand(at, FixedArray::kHeaderSize));
    __ Branch(&push, ne, t1, Operand(at));
    __ LoadRoot(at, Heap::kUndefinedValueRootIndex);
    __ bind(&push);
    __ Push(at);
    __ Addu(t0, t0, Operand(1));
    __ Branch(&loop);
    __ bind(&done);
    __ Move(a0, t0);
  }

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
  __ lb(scratch1, MemOperand(at));
  __ Branch(&done, eq, scratch1, Operand(zero_reg));

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ lw(scratch3,
          MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
    __ Branch(&no_interpreter_frame, ne, scratch3,
              Operand(Smi::FromInt(StackFrame::STUB)));
    __ lw(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Register caller_args_count_reg = scratch1;
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ lw(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ lw(scratch3,
        MemOperand(scratch2, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ Branch(&no_arguments_adaptor, ne, scratch3,
            Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Drop current frame and load arguments count from arguments adaptor frame.
  __ mov(fp, scratch2);
  __ lw(caller_args_count_reg,
        MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(caller_args_count_reg);
  __ Branch(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ lw(scratch1,
        MemOperand(fp, ArgumentsAdaptorFrameConstants::kFunctionOffset));
  __ lw(scratch1,
        FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(caller_args_count_reg,
        FieldMemOperand(scratch1,
                        SharedFunctionInfo::kFormalParameterCountOffset));
  __ SmiUntag(caller_args_count_reg);

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
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kFunctionKindByteOffset));
  __ And(at, a3, Operand(SharedFunctionInfo::kClassConstructorBitsWithinByte));
  __ Branch(&class_constructor, ne, at, Operand(zero_reg));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ lbu(a3, FieldMemOperand(a2, SharedFunctionInfo::kNativeByteOffset));
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
      __ Lsa(at, sp, a0, kPointerSizeLog2);
      __ lw(a3, MemOperand(at));
      __ JumpIfSmi(a3, &convert_to_object);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ GetObjectType(a3, t0, t0);
      __ Branch(&done_convert, hs, t0, Operand(FIRST_JS_RECEIVER_TYPE));
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
        __ sll(a0, a0, kSmiTagSize);  // Smi tagged.
        __ Push(a0, a1);
        __ mov(a0, a3);
        __ Push(cp);
        __ Call(masm->isolate()->builtins()->ToObject(),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(a3, v0);
        __ Pop(a0, a1);
        __ sra(a0, a0, kSmiTagSize);  // Un-tag.
      }
      __ lw(a2, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ Lsa(at, sp, a0, kPointerSizeLog2);
    __ sw(a3, MemOperand(at));
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

  __ lw(a2,
        FieldMemOperand(a2, SharedFunctionInfo::kFormalParameterCountOffset));
  __ sra(a2, a2, kSmiTagSize);  // Un-tag.
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
    __ lw(at, FieldMemOperand(a1, JSBoundFunction::kBoundThisOffset));
    __ Lsa(t0, sp, a0, kPointerSizeLog2);
    __ sw(at, MemOperand(t0));
  }

  // Load [[BoundArguments]] into a2 and length of that into t0.
  __ lw(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(t0);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- t0 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ sll(t1, t0, kPointerSizeLog2);
    __ Subu(sp, sp, Operand(t1));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Addu(sp, sp, Operand(t1));
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
    __ mov(t1, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, gt, t1, Operand(a0));
    __ Lsa(t2, sp, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t2));
    __ Lsa(t2, sp, t1, kPointerSizeLog2);
    __ sw(at, MemOperand(t2));
    __ Addu(t0, t0, Operand(1));
    __ Addu(t1, t1, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(t0);
    __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Subu(t0, t0, Operand(1));
    __ Branch(&done_loop, lt, t0, Operand(zero_reg));
    __ Lsa(t1, a2, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t1));
    __ Lsa(t1, sp, a0, kPointerSizeLog2);
    __ sw(at, MemOperand(t1));
    __ Addu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ lw(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ li(at, Operand(ExternalReference(Builtins::kCall_ReceiverIsAny,
                                      masm->isolate())));
  __ lw(at, MemOperand(at));
  __ Addu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
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
  __ lbu(t1, FieldMemOperand(t1, Map::kBitFieldOffset));
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
  __ Addu(a0, a0, 2);
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ Lsa(at, sp, a0, kPointerSizeLog2);
  __ sw(a1, MemOperand(at));
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

// static
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
  __ lw(t0, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  __ lw(t0, FieldMemOperand(t0, SharedFunctionInfo::kConstructStubOffset));
  __ Addu(at, t0, Operand(Code::kHeaderSize - kHeapObjectTag));
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

  // Load [[BoundArguments]] into a2 and length of that into t0.
  __ lw(a2, FieldMemOperand(a1, JSBoundFunction::kBoundArgumentsOffset));
  __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
  __ SmiUntag(t0);

  // ----------- S t a t e -------------
  //  -- a0 : the number of arguments (not including the receiver)
  //  -- a1 : the function to call (checked to be a JSBoundFunction)
  //  -- a2 : the [[BoundArguments]] (implemented as FixedArray)
  //  -- a3 : the new target (checked to be a constructor)
  //  -- t0 : the number of [[BoundArguments]]
  // -----------------------------------

  // Reserve stack space for the [[BoundArguments]].
  {
    Label done;
    __ sll(t1, t0, kPointerSizeLog2);
    __ Subu(sp, sp, Operand(t1));
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    __ LoadRoot(at, Heap::kRealStackLimitRootIndex);
    __ Branch(&done, gt, sp, Operand(at));  // Signed comparison.
    // Restore the stack pointer.
    __ Addu(sp, sp, Operand(t1));
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
    __ mov(t1, zero_reg);
    __ bind(&loop);
    __ Branch(&done_loop, ge, t1, Operand(a0));
    __ Lsa(t2, sp, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t2));
    __ Lsa(t2, sp, t1, kPointerSizeLog2);
    __ sw(at, MemOperand(t2));
    __ Addu(t0, t0, Operand(1));
    __ Addu(t1, t1, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Copy [[BoundArguments]] to the stack (below the arguments).
  {
    Label loop, done_loop;
    __ lw(t0, FieldMemOperand(a2, FixedArray::kLengthOffset));
    __ SmiUntag(t0);
    __ Addu(a2, a2, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    __ bind(&loop);
    __ Subu(t0, t0, Operand(1));
    __ Branch(&done_loop, lt, t0, Operand(zero_reg));
    __ Lsa(t1, a2, t0, kPointerSizeLog2);
    __ lw(at, MemOperand(t1));
    __ Lsa(t1, sp, a0, kPointerSizeLog2);
    __ sw(at, MemOperand(t1));
    __ Addu(a0, a0, Operand(1));
    __ Branch(&loop);
    __ bind(&done_loop);
  }

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label skip_load;
    __ Branch(&skip_load, ne, a1, Operand(a3));
    __ lw(a3, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&skip_load);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ lw(a1, FieldMemOperand(a1, JSBoundFunction::kBoundTargetFunctionOffset));
  __ li(at, Operand(ExternalReference(Builtins::kConstruct, masm->isolate())));
  __ lw(at, MemOperand(at));
  __ Addu(at, at, Operand(Code::kHeaderSize - kHeapObjectTag));
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
  __ Addu(a0, a0, Operand(3));
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
  __ lw(t1, FieldMemOperand(a1, HeapObject::kMapOffset));
  __ lbu(t2, FieldMemOperand(t1, Map::kInstanceTypeOffset));
  __ Jump(masm->isolate()->builtins()->ConstructFunction(),
          RelocInfo::CODE_TARGET, eq, t2, Operand(JS_FUNCTION_TYPE));

  // Check if target has a [[Construct]] internal method.
  __ lbu(t3, FieldMemOperand(t1, Map::kBitFieldOffset));
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
    __ Lsa(at, sp, a0, kPointerSizeLog2);
    __ sw(a1, MemOperand(at));
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
    Generate_StackOverflowCheck(masm, a2, t1, at, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into t1.
    __ Lsa(a0, fp, a0, kPointerSizeLog2 - kSmiTagSize);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address.
    __ sll(t1, a2, kPointerSizeLog2);
    __ subu(t1, a0, t1);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // t1: copy end address

    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));
    __ push(t0);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(t1));
    __ addiu(a0, a0, -kPointerSize);  // In delay slot.

    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);
    Generate_StackOverflowCheck(masm, a2, t1, at, &stack_overflow);

    // Calculate copy start address into a0 and copy end address into t3.
    // a0: actual number of arguments as a smi
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ Lsa(a0, fp, a0, kPointerSizeLog2 - kSmiTagSize);
    // Adjust for return address and receiver.
    __ Addu(a0, a0, Operand(2 * kPointerSize));
    // Compute copy end address. Also adjust for return address.
    __ Addu(t3, fp, kPointerSize);

    // Copy the arguments (including the receiver) to the new stack frame.
    // a0: copy start address
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    // t3: copy end address
    Label copy;
    __ bind(&copy);
    __ lw(t0, MemOperand(a0));  // Adjusted above for return addr and receiver.
    __ Subu(sp, sp, kPointerSize);
    __ Subu(a0, a0, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &copy, ne, a0, Operand(t3));
    __ sw(t0, MemOperand(sp));  // In the delay slot.

    // Fill the remaining expected arguments with undefined.
    // a1: function
    // a2: expected number of arguments
    // a3: new target (passed through to callee)
    __ LoadRoot(t0, Heap::kUndefinedValueRootIndex);
    __ sll(t2, a2, kPointerSizeLog2);
    __ Subu(t1, fp, Operand(t2));
    // Adjust for frame.
    __ Subu(t1, t1, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ Subu(sp, sp, kPointerSize);
    __ Branch(USE_DELAY_SLOT, &fill, ne, sp, Operand(t1));
    __ sw(t0, MemOperand(sp));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ mov(a0, a2);
  // a0 : expected number of arguments
  // a1 : function (passed through to callee)
  // a3 : new target (passed through to callee)
  __ lw(t0, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ Call(t0);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();

  // -------------------------------------------
  // Don't adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ lw(t0, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  __ Jump(t0);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ break_(0xCC);
  }
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS

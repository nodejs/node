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
  // Load the InternalArray function from the native context.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, result);
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the InternalArray function from the native context.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, result);
}


void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
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

  // Insert extra arguments.
  int num_extra_args = 0;
  switch (extra_args) {
    case BuiltinExtraArguments::kTarget:
      __ Push(x1);
      ++num_extra_args;
      break;
    case BuiltinExtraArguments::kNewTarget:
      __ Push(x3);
      ++num_extra_args;
      break;
    case BuiltinExtraArguments::kTargetAndNewTarget:
      __ Push(x1, x3);
      num_extra_args += 2;
      break;
    case BuiltinExtraArguments::kNone:
      break;
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
void Builtins::Generate_MathMaxMin(MacroAssembler* masm, MathMaxMinKind kind) {
  // ----------- S t a t e -------------
  //  -- x0                 : number of arguments
  //  -- lr                 : return address
  //  -- sp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- sp[(argc + 1) * 8] : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_MathMaxMin");

  Heap::RootListIndex const root_index =
      (kind == MathMaxMinKind::kMin) ? Heap::kInfinityValueRootIndex
                                     : Heap::kMinusInfinityValueRootIndex;

  // Load the accumulator with the default return value (either -Infinity or
  // +Infinity), with the tagged value in x1 and the double value in d1.
  __ LoadRoot(x1, root_index);
  __ Ldr(d1, FieldMemOperand(x1, HeapNumber::kValueOffset));

  // Remember how many slots to drop (including the receiver).
  __ Add(x4, x0, 1);

  Label done_loop, loop;
  __ Bind(&loop);
  {
    // Check if all parameters done.
    __ Subs(x0, x0, 1);
    __ B(lt, &done_loop);

    // Load the next parameter tagged value into x2.
    __ Peek(x2, Operand(x0, LSL, kPointerSizeLog2));

    // Load the double value of the parameter into d2, maybe converting the
    // parameter to a number first using the ToNumberStub if necessary.
    Label convert_smi, convert_number, done_convert;
    __ JumpIfSmi(x2, &convert_smi);
    __ JumpIfHeapNumber(x2, &convert_number);
    {
      // Parameter is not a Number, use the ToNumberStub to convert it.
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(x0);
      __ SmiTag(x4);
      __ Push(x0, x1, x4);
      __ Mov(x0, x2);
      ToNumberStub stub(masm->isolate());
      __ CallStub(&stub);
      __ Mov(x2, x0);
      __ Pop(x4, x1, x0);
      {
        // Restore the double accumulator value (d1).
        Label done_restore;
        __ SmiUntagToDouble(d1, x1, kSpeculativeUntag);
        __ JumpIfSmi(x1, &done_restore);
        __ Ldr(d1, FieldMemOperand(x1, HeapNumber::kValueOffset));
        __ Bind(&done_restore);
      }
      __ SmiUntag(x4);
      __ SmiUntag(x0);
    }
    __ AssertNumber(x2);
    __ JumpIfSmi(x2, &convert_smi);

    __ Bind(&convert_number);
    __ Ldr(d2, FieldMemOperand(x2, HeapNumber::kValueOffset));
    __ B(&done_convert);

    __ Bind(&convert_smi);
    __ SmiUntagToDouble(d2, x2);
    __ Bind(&done_convert);

    // We can use a single fmin/fmax for the operation itself, but we then need
    // to work out which HeapNumber (or smi) the result came from.
    __ Fmov(x11, d1);
    if (kind == MathMaxMinKind::kMin) {
      __ Fmin(d1, d1, d2);
    } else {
      DCHECK(kind == MathMaxMinKind::kMax);
      __ Fmax(d1, d1, d2);
    }
    __ Fmov(x10, d1);
    __ Cmp(x10, x11);
    __ Csel(x1, x1, x2, eq);
    __ B(&loop);
  }

  __ Bind(&done_loop);
  __ Mov(x0, x1);
  __ Drop(x4);
  __ Ret();
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_NumberConstructor");

  // 1. Load the first argument into x0 and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    __ Cbz(x0, &no_arguments);
    __ Sub(x0, x0, 1);
    __ Drop(x0);
    __ Ldr(x0, MemOperand(jssp, 2 * kPointerSize, PostIndex));
  }

  // 2a. Convert first argument to number.
  ToNumberStub stub(masm->isolate());
  __ TailCallStub(&stub);

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
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_NumberConstructor_ConstructStub");

  // 1. Make sure we operate in the context of the called function.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  // 2. Load the first argument into x2 and get rid of the rest (including the
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
    __ Mov(x2, Smi::FromInt(0));
    __ Bind(&done);
  }

  // 3. Make sure x2 is a number.
  {
    Label done_convert;
    __ JumpIfSmi(x2, &done_convert);
    __ JumpIfObjectType(x2, x4, x4, HEAP_NUMBER_TYPE, &done_convert, eq);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(x1, x3);
      __ Move(x0, x2);
      ToNumberStub stub(masm->isolate());
      __ CallStub(&stub);
      __ Move(x2, x0);
      __ Pop(x3, x1);
    }
    __ Bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label new_object;
  __ Cmp(x1, x3);
  __ B(ne, &new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(x0, x1, x2, x4, x5, &new_object);
  __ Ret();

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x2);  // first argument
    FastNewObjectStub stub(masm->isolate());
    __ CallStub(&stub);
    __ Pop(x2);
  }
  __ Str(x2, FieldMemOperand(x0, JSValue::kValueOffset));
  __ Ret();
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
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }
}


// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                     : number of arguments
  //  -- x1                     : constructor function
  //  -- x3                     : new target
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 8] : arg[n] (zero based)
  //  -- sp[argc * 8]           : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_StringConstructor_ConstructStub");

  // 1. Make sure we operate in the context of the called function.
  __ Ldr(cp, FieldMemOperand(x1, JSFunction::kContextOffset));

  // 2. Load the first argument into x2 and get rid of the rest (including the
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

  // 3. Make sure x2 is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(x2, &convert);
    __ JumpIfObjectType(x2, x4, x4, FIRST_NONSTRING_TYPE, &done_convert, lo);
    __ Bind(&convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(x1, x3);
      __ Move(x0, x2);
      __ CallStub(&stub);
      __ Move(x2, x0);
      __ Pop(x3, x1);
    }
    __ Bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label new_object;
  __ Cmp(x1, x3);
  __ B(ne, &new_object);

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(x0, x1, x2, x4, x5, &new_object);
  __ Ret();

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(x2);  // first argument
    FastNewObjectStub stub(masm->isolate());
    __ CallStub(&stub);
    __ Pop(x2);
  }
  __ Str(x2, FieldMemOperand(x0, JSValue::kValueOffset));
  __ Ret();
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


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool create_implicit_receiver,
                                           bool check_derived_construct) {
  // ----------- S t a t e -------------
  //  -- x0     : number of arguments
  //  -- x1     : constructor function
  //  -- x2     : allocation site or undefined
  //  -- x3     : new target
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
    Register new_target = x3;

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(allocation_site, x10);
    __ SmiTag(argc);
    __ Push(allocation_site, argc);

    if (create_implicit_receiver) {
      // Allocate the new receiver object.
      __ Push(constructor, new_target);
      FastNewObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ Mov(x4, x0);
      __ Pop(new_target, constructor);

      // ----------- S t a t e -------------
      //  -- x1: constructor function
      //  -- x3: new target
      //  -- x4: newly allocated object
      // -----------------------------------

      // Reload the number of arguments from the stack.
      // Set it up in x0 for the function call below.
      // jssp[0]: number of arguments (smi-tagged)
      __ Peek(argc, 0);  // Load number of arguments.
    }

    __ SmiUntag(argc);

    if (create_implicit_receiver) {
      // Push the allocated receiver to the stack. We need two copies
      // because we may have to return the original one and the calling
      // conventions dictate that the called function pops the receiver.
      __ Push(x4, x4);
    } else {
      __ PushRoot(Heap::kTheHoleValueRootIndex);
    }

    // Set up pointer to last argument.
    __ Add(x2, fp, StandardFrameConstants::kCallerSPOffset);

    // Copy arguments and receiver to the expression stack.
    // Copy 2 values every loop to use ldp/stp.
    // x0: number of arguments
    // x1: constructor function
    // x2: address of last argument (caller sp)
    // x3: new target
    // jssp[0]: receiver
    // jssp[1]: receiver
    // jssp[2]: number of arguments (smi-tagged)
    // Compute the start address of the copy in x3.
    __ Add(x4, x2, Operand(argc, LSL, kPointerSizeLog2));
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

    // Call the function.
    // x0: number of arguments
    // x1: constructor function
    // x3: new target
    if (is_api_function) {
      __ Ldr(cp, FieldMemOperand(constructor, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(argc);
      __ InvokeFunction(constructor, new_target, actual, CALL_FUNCTION,
                        CheckDebugStepCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (create_implicit_receiver && !is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore the context from the frame.
    // x0: result
    // jssp[0]: receiver
    // jssp[1]: number of arguments (smi-tagged)
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    if (create_implicit_receiver) {
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
      // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
      __ JumpIfObjectType(x0, x1, x3, FIRST_JS_RECEIVER_TYPE, &exit, ge);

      // Throw away the result of the constructor invocation and use the
      // on-stack receiver as the result.
      __ Bind(&use_receiver);
      __ Peek(x0, 0);

      // Remove the receiver from the stack, remove caller arguments, and
      // return.
      __ Bind(&exit);
      // x0: result
      // jssp[0]: receiver (newly allocated object)
      // jssp[1]: number of arguments (smi-tagged)
      __ Peek(x1, 1 * kXRegSize);
    } else {
      __ Peek(x1, 0);
    }

    // Leave construct frame.
  }

  // ES6 9.2.2. Step 13+
  // Check that the result is not a Smi, indicating that the constructor result
  // from a derived class is neither undefined nor an Object.
  if (check_derived_construct) {
    Label dont_throw;
    __ JumpIfNotSmi(x0, &dont_throw);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowDerivedConstructorReturnedNonObject);
    }
    __ Bind(&dont_throw);
  }

  __ DropBySMI(x1);
  __ Drop(1);
  if (create_implicit_receiver) {
    __ IncrementCounter(isolate->counters()->constructed_objects(), 1, x1, x2);
  }
  __ Ret();
}


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
  __ Push(x1);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
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
//   - x3: the new target
//   - cp: our context.
//   - fp: our caller's frame pointer.
//   - jssp: stack pointer.
//   - lr: return address.
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
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
  Register debug_info = kInterpreterBytecodeArrayRegister;
  Label load_debug_bytecode_array, bytecode_array_loaded;
  DCHECK(!debug_info.is(x0));
  __ Ldr(debug_info, FieldMemOperand(x0, SharedFunctionInfo::kDebugInfoOffset));
  __ Cmp(debug_info, Operand(DebugInfo::uninitialized()));
  __ B(ne, &load_debug_bytecode_array);
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(x0, SharedFunctionInfo::kFunctionDataOffset));
  __ Bind(&bytecode_array_loaded);

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister,
                    kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, x0, x0,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(eq, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Push new.target, bytecode array and zero for bytecode array offset.
  __ Mov(x0, Operand(0));
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

  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's prologue:
  //  - Call ProfileEntryHookStub when isolate has a function_entry_hook.
  //  - Code aging of the BytecodeArray object.

  // Load accumulator, register file, bytecode offset, dispatch table into
  // registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ Add(kInterpreterRegisterFileRegister, fp,
         Operand(InterpreterFrameConstants::kRegisterFilePointerFromFp));
  __ Mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ Mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Dispatch to the first bytecode handler for the function.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x1, LSL, kPointerSizeLog2));
  __ Ldr(ip0, MemOperand(kInterpreterDispatchTableRegister, x1));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ Add(ip0, ip0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Call(ip0);

  // Even though the first bytecode handler was called, we will never return.
  __ Abort(kUnexpectedReturnFromBytecodeHandler);

  // Load debug copy of the bytecode array.
  __ Bind(&load_debug_bytecode_array);
  __ Ldr(kInterpreterBytecodeArrayRegister,
         FieldMemOperand(debug_info, DebugInfo::kAbstractCodeIndex));
  __ B(&bytecode_array_loaded);
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


static void Generate_EnterBytecodeDispatch(MacroAssembler* masm) {
  // Initialize register file register and dispatch table register.
  __ Add(kInterpreterRegisterFileRegister, fp,
         Operand(InterpreterFrameConstants::kRegisterFilePointerFromFp));
  __ Mov(kInterpreterDispatchTableRegister,
         Operand(ExternalReference::interpreter_dispatch_table_address(
             masm->isolate())));

  // Get the context from the frame.
  __ Ldr(kContextRegister,
         MemOperand(kInterpreterRegisterFileRegister,
                    InterpreterFrameConstants::kContextFromRegisterPointer));

  // Get the bytecode array pointer from the frame.
  __ Ldr(
      kInterpreterBytecodeArrayRegister,
      MemOperand(kInterpreterRegisterFileRegister,
                 InterpreterFrameConstants::kBytecodeArrayFromRegisterPointer));

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
         MemOperand(
             kInterpreterRegisterFileRegister,
             InterpreterFrameConstants::kBytecodeOffsetFromRegisterPointer));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ Ldrb(x1, MemOperand(kInterpreterBytecodeArrayRegister,
                         kInterpreterBytecodeOffsetRegister));
  __ Mov(x1, Operand(x1, LSL, kPointerSizeLog2));
  __ Ldr(ip0, MemOperand(kInterpreterDispatchTableRegister, x1));
  __ Add(ip0, ip0, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(ip0);
}


static void Generate_InterpreterNotifyDeoptimizedHelper(
    MacroAssembler* masm, Deoptimizer::BailoutType type) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Pass the deoptimization type to the runtime system.
    __ Mov(x1, Operand(Smi::FromInt(static_cast<int>(type))));
    __ Push(x1);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  // Drop state (we don't use these for interpreter deopts) and and pop the
  // accumulator value into the accumulator register.
  __ Drop(1);
  __ Pop(kInterpreterAccumulatorRegister);

  // Enter the bytecode dispatch.
  Generate_EnterBytecodeDispatch(masm);
}


void Builtins::Generate_InterpreterNotifyDeoptimized(MacroAssembler* masm) {
  Generate_InterpreterNotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_InterpreterNotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_InterpreterNotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_InterpreterNotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_InterpreterNotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}

void Builtins::Generate_InterpreterEnterBytecodeDispatch(MacroAssembler* masm) {
  // Set the address of the interpreter entry trampoline as a return address.
  // This simulates the initial call to bytecode handlers in interpreter entry
  // trampoline. The return will never actually be taken, but our stack walker
  // uses this address to determine whether a frame is interpreted.
  __ LoadObject(lr, masm->isolate()->builtins()->InterpreterEntryTrampoline());

  Generate_EnterBytecodeDispatch(masm);
}


void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileLazy);
}


void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm,
                                 Runtime::kCompileOptimized_NotConcurrent);
}


void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  GenerateTailCallToReturnedCode(masm, Runtime::kCompileOptimized_Concurrent);
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
  //   x3 - new target
  //   lr - The return address for the JSFunction itself. It has not yet been
  //        preserved on the stack because the frame setup code was replaced
  //        with a call to this stub, to handle code ageing.
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ Push(x0, x1, x3, fp, lr);
    __ Mov(x1, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(
        ExternalReference::get_mark_code_as_executed_function(
            masm->isolate()), 2);
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


static void CompatibleReceiverCheck(MacroAssembler* masm, Register receiver,
                                    Register function_template_info,
                                    Register scratch0, Register scratch1,
                                    Register scratch2,
                                    Label* receiver_check_failed) {
  Register signature = scratch0;
  Register map = scratch1;
  Register constructor = scratch2;

  // If there is no signature, return the holder.
  __ Ldr(signature, FieldMemOperand(function_template_info,
                                    FunctionTemplateInfo::kSignatureOffset));
  __ CompareRoot(signature, Heap::kUndefinedValueRootIndex);
  Label receiver_check_passed;
  __ B(eq, &receiver_check_passed);

  // Walk the prototype chain.
  __ Ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  Label prototype_loop_start;
  __ Bind(&prototype_loop_start);

  // Get the constructor, if any
  __ GetMapConstructor(constructor, map, x16, x16);
  __ cmp(x16, Operand(JS_FUNCTION_TYPE));
  Label next_prototype;
  __ B(ne, &next_prototype);
  Register type = constructor;
  __ Ldr(type,
         FieldMemOperand(constructor, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(type, FieldMemOperand(type, SharedFunctionInfo::kFunctionDataOffset));

  // Loop through the chain of inheriting function templates.
  Label function_template_loop;
  __ Bind(&function_template_loop);

  // If the signatures match, we have a compatible receiver.
  __ Cmp(signature, type);
  __ B(eq, &receiver_check_passed);

  // If the current type is not a FunctionTemplateInfo, load the next prototype
  // in the chain.
  __ JumpIfSmi(type, &next_prototype);
  __ CompareObjectType(type, x16, x17, FUNCTION_TEMPLATE_INFO_TYPE);
  __ B(ne, &next_prototype);

  // Otherwise load the parent function template and iterate.
  __ Ldr(type,
         FieldMemOperand(type, FunctionTemplateInfo::kParentTemplateOffset));
  __ B(&function_template_loop);

  // Load the next prototype.
  __ Bind(&next_prototype);
  __ Ldr(x16, FieldMemOperand(map, Map::kBitField3Offset));
  __ Tst(x16, Operand(Map::HasHiddenPrototype::kMask));
  __ B(eq, receiver_check_failed);
  __ Ldr(receiver, FieldMemOperand(map, Map::kPrototypeOffset));
  __ Ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Iterate.
  __ B(&prototype_loop_start);

  __ Bind(&receiver_check_passed);
}


void Builtins::Generate_HandleFastApiCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0                 : number of arguments excluding receiver
  //  -- x1                 : callee
  //  -- lr                 : return address
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[8 * (argc - 1)] : first argument
  //  -- sp[8 * argc]       : receiver
  // -----------------------------------

  // Load the FunctionTemplateInfo.
  __ Ldr(x3, FieldMemOperand(x1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldr(x3, FieldMemOperand(x3, SharedFunctionInfo::kFunctionDataOffset));

  // Do the compatible receiver check.
  Label receiver_check_failed;
  __ Ldr(x2, MemOperand(jssp, x0, LSL, kPointerSizeLog2));
  CompatibleReceiverCheck(masm, x2, x3, x4, x5, x6, &receiver_check_failed);

  // Get the callback offset from the FunctionTemplateInfo, and jump to the
  // beginning of the code.
  __ Ldr(x4, FieldMemOperand(x3, FunctionTemplateInfo::kCallCodeOffset));
  __ Ldr(x4, FieldMemOperand(x4, CallHandlerInfo::kFastHandlerOffset));
  __ Add(x4, x4, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(x4);

  // Compatible receiver check failed: throw an Illegal Invocation exception.
  __ Bind(&receiver_check_failed);
  // Drop the arguments (including the receiver)
  __ add(x0, x0, Operand(1));
  __ Drop(x0);
  __ TailCallRuntime(Runtime::kThrowIllegalInvocation);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ Ldr(x0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ Push(x0);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
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
    __ CallRuntime(Runtime::kStackGuard);
  }
  __ Jump(masm->isolate()->builtins()->OnStackReplacement(),
          RelocInfo::CODE_TARGET);

  __ Bind(&ok);
  __ Ret();
}


// static
void Builtins::Generate_DatePrototype_GetField(MacroAssembler* masm,
                                               int field_index) {
  // ----------- S t a t e -------------
  //  -- lr      : return address
  //  -- jssp[0] : receiver
  // -----------------------------------
  ASM_LOCATION("Builtins::Generate_DatePrototype_GetField");

  // 1. Pop receiver into x0 and check that it's actually a JSDate object.
  Label receiver_not_date;
  {
    __ Pop(x0);
    __ JumpIfSmi(x0, &receiver_not_date);
    __ JumpIfNotObjectType(x0, x1, x2, JS_DATE_TYPE, &receiver_not_date);
  }

  // 2. Load the specified date field, falling back to the runtime as necessary.
  if (field_index == JSDate::kDateValue) {
    __ Ldr(x0, FieldMemOperand(x0, JSDate::kValueOffset));
  } else {
    if (field_index < JSDate::kFirstUncachedField) {
      Label stamp_mismatch;
      __ Mov(x1, ExternalReference::date_cache_stamp(masm->isolate()));
      __ Ldr(x1, MemOperand(x1));
      __ Ldr(x2, FieldMemOperand(x0, JSDate::kCacheStampOffset));
      __ Cmp(x1, x2);
      __ B(ne, &stamp_mismatch);
      __ Ldr(x0, FieldMemOperand(
                     x0, JSDate::kValueOffset + field_index * kPointerSize));
      __ Ret();
      __ Bind(&stamp_mismatch);
    }
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Mov(x1, Smi::FromInt(field_index));
    __ CallCFunction(
        ExternalReference::get_date_field_function(masm->isolate()), 2);
  }
  __ Ret();

  // 3. Raise a TypeError if the receiver is not a date.
  __ Bind(&receiver_not_date);
  __ TailCallRuntime(Runtime::kThrowNotDateError);
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
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ Bind(&new_target_not_constructor);
  {
    __ Poke(new_target, 0);
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }
}


static void ArgumentAdaptorStackCheck(MacroAssembler* masm,
                                      Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  //  -- x3 : new target (passed through to callee)
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
    Label create_arguments, create_array, create_runtime, done_create;
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
    __ CompareInstanceType(arguments_list_map, native_context, JS_ARRAY_TYPE);
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

    // Try to create the list from a JSArray object.
    __ Bind(&create_array);
    __ Ldr(x10, FieldMemOperand(arguments_list_map, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(x10);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    // Branch for anything that's not FAST_{SMI_}ELEMENTS.
    __ TestAndBranchIfAnySet(x10, ~FAST_ELEMENTS, &create_runtime);
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
    Label done, loop;
    Register src = x4;

    __ Add(src, args, FixedArray::kHeaderSize - kHeapObjectTag);
    __ Mov(x0, len);  // The 'len' argument for Call() or Construct().
    __ Cbz(len, &done);
    __ Claim(len);
    __ Bind(&loop);
    __ Sub(len, len, 1);
    __ Ldr(x10, MemOperand(src, kPointerSize, PostIndex));
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

  // Prepare for tail call only if the debugger is not active.
  Label done;
  ExternalReference debug_is_active =
      ExternalReference::debug_is_active_address(masm->isolate());
  __ Mov(scratch1, Operand(debug_is_active));
  __ Ldrb(scratch1, MemOperand(scratch1));
  __ Cmp(scratch1, Operand(0));
  __ B(ne, &done);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ Ldr(scratch3, MemOperand(fp, StandardFrameConstants::kMarkerOffset));
    __ Cmp(scratch3, Operand(Smi::FromInt(StackFrame::STUB)));
    __ B(ne, &no_interpreter_frame);
    __ Ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ Ldr(scratch2, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ Ldr(scratch3,
         MemOperand(scratch2, StandardFrameConstants::kContextOffset));
  __ Cmp(scratch3, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ B(ne, &no_arguments_adaptor);

  // Drop arguments adaptor frame and load arguments count.
  __ mov(fp, scratch2);
  __ Ldr(scratch1,
         MemOperand(fp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(scratch1);
  __ B(&formal_parameter_count_loaded);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ Ldr(scratch1, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ Ldr(scratch1,
         FieldMemOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ Ldrsw(scratch1,
           FieldMemOperand(scratch1,
                           SharedFunctionInfo::kFormalParameterCountOffset));
  __ bind(&formal_parameter_count_loaded);

  // Calculate the end of destination area where we will put the arguments
  // after we drop current frame. We add kPointerSize to count the receiver
  // argument which is not included into formal parameters count.
  Register dst_reg = scratch2;
  __ add(dst_reg, fp, Operand(scratch1, LSL, kPointerSizeLog2));
  __ add(dst_reg, dst_reg,
         Operand(StandardFrameConstants::kCallerSPOffset + kPointerSize));

  Register src_reg = scratch1;
  __ add(src_reg, jssp, Operand(args_reg, LSL, kPointerSizeLog2));
  // Count receiver argument as well (not included in args_reg).
  __ add(src_reg, src_reg, Operand(kPointerSize));

  if (FLAG_debug_code) {
    __ Cmp(src_reg, dst_reg);
    __ Check(lo, kStackAccessBelowStackPointer);
  }

  // Restore caller's frame pointer and return address now as they will be
  // overwritten by the copying loop.
  __ Ldr(lr, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  __ Ldr(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).

  // Both src_reg and dst_reg are pointing to the word after the one to copy,
  // so they must be pre-decremented in the loop.
  Register tmp_reg = scratch3;
  Label loop, entry;
  __ B(&entry);
  __ bind(&loop);
  __ Ldr(tmp_reg, MemOperand(src_reg, -kPointerSize, PreIndex));
  __ Str(tmp_reg, MemOperand(dst_reg, -kPointerSize, PreIndex));
  __ bind(&entry);
  __ Cmp(jssp, src_reg);
  __ B(ne, &loop);

  // Leave current frame.
  __ Mov(jssp, dst_reg);
  __ SetStackPointer(jssp);
  __ AssertStackConsistency();

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
  __ TestAndBranchIfAnySet(
      w3, (1 << SharedFunctionInfo::kIsDefaultConstructor) |
              (1 << SharedFunctionInfo::kIsSubclassConstructor) |
              (1 << SharedFunctionInfo::kIsBaseConstructor),
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
        ToObjectStub stub(masm->isolate());
        __ CallStub(&stub);
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
void Builtins::Generate_InterpreterPushArgsAndCallImpl(
    MacroAssembler* masm, TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- x0 : the number of arguments (not including the receiver)
  //  -- x2 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- x1 : the target to call (can be any Object).
  // -----------------------------------

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
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            tail_call_mode),
          RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_InterpreterPushArgsAndConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  // -- x0 : argument count (not including receiver)
  // -- x3 : new target
  // -- x1 : constructor to call
  // -- x2 : address of the first argument
  // -----------------------------------

  // Find the address of the last argument.
  __ add(x5, x0, Operand(1));  // Add one for receiver (to be constructed).
  __ lsl(x5, x5, kPointerSizeLog2);

  // Set stack pointer and where to stop.
  __ Mov(x6, jssp);
  __ Claim(x5, 1);
  __ sub(x4, x6, x5);

  // Push a slot for the receiver.
  __ Str(xzr, MemOperand(x6, -kPointerSize, PreIndex));

  Label loop_header, loop_check;
  // Push the arguments.
  __ B(&loop_check);
  __ Bind(&loop_header);
  // TODO(rmcilroy): Push two at a time once we ensure we keep stack aligned.
  __ Ldr(x5, MemOperand(x2, -kPointerSize, PostIndex));
  __ Str(x5, MemOperand(x6, -kPointerSize, PreIndex));
  __ Bind(&loop_check);
  __ Cmp(x6, x4);
  __ B(gt, &loop_header);

  // Call the constructor with x0, x1, and x3 unmodified.
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  ASM_LOCATION("Builtins::Generate_ArgumentsAdaptorTrampoline");
  // ----------- S t a t e -------------
  //  -- x0 : actual number of arguments
  //  -- x1 : function (passed through to callee)
  //  -- x2 : expected number of arguments
  //  -- x3 : new target (passed through to callee)
  // -----------------------------------

  Register argc_actual = x0;  // Excluding the receiver.
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
    ArgumentAdaptorStackCheck(masm, &stack_overflow);

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
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments);
    }

    __ Bind(&no_strong_error);
    EnterArgumentsAdaptorFrame(masm);
    ArgumentAdaptorStackCheck(masm, &stack_overflow);

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


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM

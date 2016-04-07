// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/code-factory.h"
#include "src/codegen.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments excluding receiver
  //  -- rdi                 : target
  //  -- rdx                 : new.target
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : last argument
  //  -- ...
  //  -- rsp[8 * argc]       : first argument
  //  -- rsp[8 * (argc + 1)] : receiver
  // -----------------------------------
  __ AssertFunction(rdi);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args != BuiltinExtraArguments::kNone) {
    __ PopReturnAddressTo(kScratchRegister);
    if (extra_args & BuiltinExtraArguments::kTarget) {
      ++num_extra_args;
      __ Push(rdi);
    }
    if (extra_args & BuiltinExtraArguments::kNewTarget) {
      ++num_extra_args;
      __ Push(rdx);
    }
    __ PushReturnAddressFrom(kScratchRegister);
  }

  // JumpToExternalReference expects rax to contain the number of arguments
  // including the receiver and the extra arguments.
  __ addp(rax, Immediate(num_extra_args + 1));

  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ movp(kScratchRegister,
          FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(kScratchRegister,
          FieldOperand(kScratchRegister, SharedFunctionInfo::kCodeOffset));
  __ leap(kScratchRegister, FieldOperand(kScratchRegister, Code::kHeaderSize));
  __ jmp(kScratchRegister);
}

static void GenerateTailCallToReturnedCode(MacroAssembler* masm,
                                           Runtime::FunctionId function_id) {
  // ----------- S t a t e -------------
  //  -- rax : argument count (preserved for callee)
  //  -- rdx : new target (preserved for callee)
  //  -- rdi : target function (preserved for callee)
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Push the number of arguments to the callee.
    __ Integer32ToSmi(rax, rax);
    __ Push(rax);
    // Push a copy of the target function and the new target.
    __ Push(rdi);
    __ Push(rdx);
    // Function is also the parameter to the runtime call.
    __ Push(rdi);

    __ CallRuntime(function_id, 1);
    __ movp(rbx, rax);

    // Restore target function and new target.
    __ Pop(rdx);
    __ Pop(rdi);
    __ Pop(rax);
    __ SmiToInteger32(rax, rax);
  }
  __ leap(rbx, FieldOperand(rbx, Code::kHeaderSize));
  __ jmp(rbx);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
  __ j(above_equal, &ok);

  GenerateTailCallToReturnedCode(masm, Runtime::kTryInstallOptimizedCode);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool create_implicit_receiver,
                                           bool check_derived_construct) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  //  -- rbx: allocation site or undefined
  //  -- rdx: new target
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(rbx);
    __ Push(rbx);
    __ Integer32ToSmi(rcx, rax);
    __ Push(rcx);

    if (create_implicit_receiver) {
      // Allocate the new receiver object.
      __ Push(rdi);
      __ Push(rdx);
      FastNewObjectStub stub(masm->isolate());
      __ CallStub(&stub);
      __ movp(rbx, rax);
      __ Pop(rdx);
      __ Pop(rdi);

      // ----------- S t a t e -------------
      //  -- rdi: constructor function
      //  -- rbx: newly allocated object
      //  -- rdx: new target
      // -----------------------------------

      // Retrieve smi-tagged arguments count from the stack.
      __ SmiToInteger32(rax, Operand(rsp, 0 * kPointerSize));
    }

    if (create_implicit_receiver) {
      // Push the allocated receiver to the stack. We need two copies
      // because we may have to return the original one and the calling
      // conventions dictate that the called function pops the receiver.
      __ Push(rbx);
      __ Push(rbx);
    } else {
      __ PushRoot(Heap::kTheHoleValueRootIndex);
    }

    // Set up pointer to last argument.
    __ leap(rbx, Operand(rbp, StandardFrameConstants::kCallerSPOffset));

    // Copy arguments and receiver to the expression stack.
    Label loop, entry;
    __ movp(rcx, rax);
    __ jmp(&entry);
    __ bind(&loop);
    __ Push(Operand(rbx, rcx, times_pointer_size, 0));
    __ bind(&entry);
    __ decp(rcx);
    __ j(greater_equal, &loop);

    // Call the function.
    if (is_api_function) {
      __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(rax);
      __ InvokeFunction(rdi, rdx, actual, CALL_FUNCTION,
                        CheckDebugStepCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (create_implicit_receiver && !is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

    if (create_implicit_receiver) {
      // If the result is an object (in the ECMA sense), we should get rid
      // of the receiver and use the result; see ECMA-262 section 13.2.2-7
      // on page 74.
      Label use_receiver, exit;
      // If the result is a smi, it is *not* an object in the ECMA sense.
      __ JumpIfSmi(rax, &use_receiver);

      // If the type of the result (stored in its map) is less than
      // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CmpObjectType(rax, FIRST_JS_RECEIVER_TYPE, rcx);
      __ j(above_equal, &exit);

      // Throw away the result of the constructor invocation and use the
      // on-stack receiver as the result.
      __ bind(&use_receiver);
      __ movp(rax, Operand(rsp, 0));

      // Restore the arguments count and leave the construct frame. The
      // arguments count is stored below the receiver.
      __ bind(&exit);
      __ movp(rbx, Operand(rsp, 1 * kPointerSize));
    } else {
      __ movp(rbx, Operand(rsp, 0));
    }

    // Leave construct frame.
  }

  // ES6 9.2.2. Step 13+
  // Check that the result is not a Smi, indicating that the constructor result
  // from a derived class is neither undefined nor an Object.
  if (check_derived_construct) {
    Label dont_throw;
    __ JumpIfNotSmi(rax, &dont_throw);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ CallRuntime(Runtime::kThrowDerivedConstructorReturnedNonObject);
    }
    __ bind(&dont_throw);
  }

  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
  if (create_implicit_receiver) {
    Counters* counters = masm->isolate()->counters();
    __ IncrementCounter(counters->constructed_objects(), 1);
  }
  __ ret(0);
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
  __ Push(rdi);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
}


enum IsTagged { kRaxIsSmiTagged, kRaxIsUntaggedInt };


// Clobbers rcx, r11, kScratchRegister; preserves all other registers.
static void Generate_CheckStackOverflow(MacroAssembler* masm,
                                        IsTagged rax_is_tagged) {
  // rax   : the number of items to be pushed to the stack
  //
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
  __ movp(rcx, rsp);
  // Make rcx the space we have left. The stack might already be overflowed
  // here which will cause rcx to become negative.
  __ subp(rcx, kScratchRegister);
  // Make r11 the space we need for the array when it is unrolled onto the
  // stack.
  if (rax_is_tagged == kRaxIsSmiTagged) {
    __ PositiveSmiTimesPowerOfTwoToInteger64(r11, rax, kPointerSizeLog2);
  } else {
    DCHECK(rax_is_tagged == kRaxIsUntaggedInt);
    __ movp(r11, rax);
    __ shlq(r11, Immediate(kPointerSizeLog2));
  }
  // Check if the arguments will overflow the stack.
  __ cmpp(rcx, r11);
  __ j(greater, &okay);  // Signed comparison.

  // Out of stack space.
  __ CallRuntime(Runtime::kThrowStackOverflow);

  __ bind(&okay);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Expects five C++ function parameters.
  // - Object* new_target
  // - JSFunction* function
  // - Object* receiver
  // - int argc
  // - Object*** argv
  // (see Handle::Invoke in execution.cc).

  // Open a C++ scope for the FrameScope.
  {
    // Platform specific argument handling. After this, the stack contains
    // an internal frame and the pushed function and receiver, and
    // register rax and rbx holds the argument count and argument array,
    // while rdi holds the function pointer, rsi the context, and rdx the
    // new.target.

#ifdef _WIN64
    // MSVC parameters in:
    // rcx        : new_target
    // rdx        : function
    // r8         : receiver
    // r9         : argc
    // [rsp+0x20] : argv

    // Clear the context before we push it when entering the internal frame.
    __ Set(rsi, 0);

    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ movp(rsi, masm->ExternalOperand(context_address));

    // Push the function and the receiver onto the stack.
    __ Push(rdx);
    __ Push(r8);

    // Load the number of arguments and setup pointer to the arguments.
    __ movp(rax, r9);
    // Load the previous frame pointer to access C argument on stack
    __ movp(kScratchRegister, Operand(rbp, 0));
    __ movp(rbx, Operand(kScratchRegister, EntryFrameConstants::kArgvOffset));
    // Load the function pointer into rdi.
    __ movp(rdi, rdx);
    // Load the new.target into rdx.
    __ movp(rdx, rcx);
#else  // _WIN64
    // GCC parameters in:
    // rdi : new_target
    // rsi : function
    // rdx : receiver
    // rcx : argc
    // r8  : argv

    __ movp(r11, rdi);
    __ movp(rdi, rsi);
    // rdi : function
    // r11 : new_target

    // Clear the context before we push it when entering the internal frame.
    __ Set(rsi, 0);

    // Enter an internal frame.
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address(Isolate::kContextAddress,
                                      masm->isolate());
    __ movp(rsi, masm->ExternalOperand(context_address));

    // Push the function and receiver onto the stack.
    __ Push(rdi);
    __ Push(rdx);

    // Load the number of arguments and setup pointer to the arguments.
    __ movp(rax, rcx);
    __ movp(rbx, r8);

    // Load the new.target into rdx.
    __ movp(rdx, r11);
#endif  // _WIN64

    // Current stack contents:
    // [rsp + 2 * kPointerSize ... ] : Internal frame
    // [rsp + kPointerSize]          : function
    // [rsp]                         : receiver
    // Current register contents:
    // rax : argc
    // rbx : argv
    // rsi : context
    // rdi : function
    // rdx : new.target

    // Check if we have enough stack space to push all arguments.
    // Expects argument count in rax. Clobbers rcx, r11.
    Generate_CheckStackOverflow(masm, kRaxIsUntaggedInt);

    // Copy arguments to the stack in a loop.
    // Register rbx points to array of pointers to handle locations.
    // Push the values of these handles.
    Label loop, entry;
    __ Set(rcx, 0);  // Set loop variable to 0.
    __ jmp(&entry, Label::kNear);
    __ bind(&loop);
    __ movp(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
    __ Push(Operand(kScratchRegister, 0));  // dereference handle
    __ addp(rcx, Immediate(1));
    __ bind(&entry);
    __ cmpp(rcx, rax);
    __ j(not_equal, &loop);

    // Invoke the builtin code.
    Handle<Code> builtin = is_construct
                               ? masm->isolate()->builtins()->Construct()
                               : masm->isolate()->builtins()->Call();
    __ Call(builtin, RelocInfo::CODE_TARGET);

    // Exit the internal frame. Notice that this also removes the empty
    // context and the function left on the stack by the code
    // invocation.
  }

  // TODO(X64): Is argument correct? Is there a receiver to remove?
  __ ret(1 * kPointerSize);  // Remove receiver.
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
//   o rdi: the JS function object being called
//   o rdx: the new target
//   o rsi: our context
//   o rbp: the caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frames.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS function.
  __ Push(rdx);  // Callee's new target.

  // Get the bytecode array from the function object and load the pointer to the
  // first entry into edi (InterpreterBytecodeRegister).
  __ movp(rax, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));

  Label load_debug_bytecode_array, bytecode_array_loaded;
  DCHECK_EQ(Smi::FromInt(0), DebugInfo::uninitialized());
  __ cmpp(FieldOperand(rax, SharedFunctionInfo::kDebugInfoOffset),
          Immediate(0));
  __ j(not_equal, &load_debug_bytecode_array);
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(rax, SharedFunctionInfo::kFunctionDataOffset));
  __ bind(&bytecode_array_loaded);

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     rax);
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Push bytecode array.
  __ Push(kInterpreterBytecodeArrayRegister);
  // Push zero for bytecode array offset.
  __ Push(Immediate(0));

  // Allocate the local and temporary register file on the stack.
  {
    // Load frame size from the BytecodeArray object.
    __ movl(rcx, FieldOperand(kInterpreterBytecodeArrayRegister,
                              BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    Label ok;
    __ movp(rdx, rsp);
    __ subp(rdx, rcx);
    __ CompareRoot(rdx, Heap::kRealStackLimitRootIndex);
    __ j(above_equal, &ok, Label::kNear);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&ok);

    // If ok, push undefined as the initial value for all register file entries.
    Label loop_header;
    Label loop_check;
    __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
    __ j(always, &loop_check);
    __ bind(&loop_header);
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    __ Push(rdx);
    // Continue loop if not done.
    __ bind(&loop_check);
    __ subp(rcx, Immediate(kPointerSize));
    __ j(greater_equal, &loop_header, Label::kNear);
  }

  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's prologue:
  //  - Call ProfileEntryHookStub when isolate has a function_entry_hook.
  //  - Code aging of the BytecodeArray object.

  // Load accumulator, register file, bytecode offset, dispatch table into
  // registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ movp(kInterpreterRegisterFileRegister, rbp);
  __ addp(kInterpreterRegisterFileRegister,
          Immediate(InterpreterFrameConstants::kRegisterFilePointerFromFp));
  __ movp(kInterpreterBytecodeOffsetRegister,
          Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Dispatch to the first bytecode handler for the function.
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(rbx, Operand(kInterpreterDispatchTableRegister, rbx,
                       times_pointer_size, 0));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ addp(rbx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(rbx);

  // Even though the first bytecode handler was called, we will never return.
  __ Abort(kUnexpectedReturnFromBytecodeHandler);

  // Load debug copy of the bytecode array.
  __ bind(&load_debug_bytecode_array);
  Register debug_info = kInterpreterBytecodeArrayRegister;
  __ movp(debug_info, FieldOperand(rax, SharedFunctionInfo::kDebugInfoOffset));
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(debug_info, DebugInfo::kAbstractCodeIndex));
  __ jmp(&bytecode_array_loaded);
}


void Builtins::Generate_InterpreterExitTrampoline(MacroAssembler* masm) {
  // TODO(rmcilroy): List of things not currently dealt with here but done in
  // fullcodegen's EmitReturnSequence.
  //  - Supporting FLAG_trace for Runtime::TraceExit.
  //  - Support profiler (specifically decrementing profiling_counter
  //    appropriately and calling out to HandleInterrupts if necessary).

  // The return value is in accumulator, which is already in rax.

  // Leave the frame (also dropping the register file).
  __ leave();

  // Drop receiver + arguments and return.
  __ movl(rbx, FieldOperand(kInterpreterBytecodeArrayRegister,
                            BytecodeArray::kParameterSizeOffset));
  __ PopReturnAddressTo(rcx);
  __ addp(rsp, rbx);
  __ PushReturnAddressFrom(rcx);
  __ ret(0);
}


static void Generate_InterpreterPushArgs(MacroAssembler* masm,
                                         bool push_receiver) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rbx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  // -----------------------------------

  // Find the address of the last argument.
  __ movp(rcx, rax);
  if (push_receiver) {
    __ addp(rcx, Immediate(1));  // Add one for receiver.
  }

  __ shlp(rcx, Immediate(kPointerSizeLog2));
  __ negp(rcx);
  __ addp(rcx, rbx);

  // Push the arguments.
  Label loop_header, loop_check;
  __ j(always, &loop_check);
  __ bind(&loop_header);
  __ Push(Operand(rbx, 0));
  __ subp(rbx, Immediate(kPointerSize));
  __ bind(&loop_check);
  __ cmpp(rbx, rcx);
  __ j(greater, &loop_header, Label::kNear);
}


// static
void Builtins::Generate_InterpreterPushArgsAndCallImpl(
    MacroAssembler* masm, TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rbx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- rdi : the target to call (can be any Object).
  // -----------------------------------

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  Generate_InterpreterPushArgs(masm, true);

  // Call the target.
  __ PushReturnAddressFrom(kScratchRegister);  // Re-push return address.
  __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny,
                                            tail_call_mode),
          RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_InterpreterPushArgsAndConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  //  -- rbx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  // -----------------------------------

  // Pop return address to allow tail-call after pushing arguments.
  __ PopReturnAddressTo(kScratchRegister);

  // Push slot for the receiver to be constructed.
  __ Push(Immediate(0));

  Generate_InterpreterPushArgs(masm, false);

  // Push return address in preparation for the tail-call.
  __ PushReturnAddressFrom(kScratchRegister);

  // Call the constructor (rax, rdx, rdi passed on).
  __ Jump(masm->isolate()->builtins()->Construct(), RelocInfo::CODE_TARGET);
}


static void Generate_EnterBytecodeDispatch(MacroAssembler* masm) {
  // Initialize register file register and dispatch table register.
  __ movp(kInterpreterRegisterFileRegister, rbp);
  __ addp(kInterpreterRegisterFileRegister,
          Immediate(InterpreterFrameConstants::kRegisterFilePointerFromFp));
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the context from the frame.
  __ movp(kContextRegister,
          Operand(kInterpreterRegisterFileRegister,
                  InterpreterFrameConstants::kContextFromRegisterPointer));

  // Get the bytecode array pointer from the frame.
  __ movp(
      kInterpreterBytecodeArrayRegister,
      Operand(kInterpreterRegisterFileRegister,
              InterpreterFrameConstants::kBytecodeArrayFromRegisterPointer));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     rbx);
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ movp(
      kInterpreterBytecodeOffsetRegister,
      Operand(kInterpreterRegisterFileRegister,
              InterpreterFrameConstants::kBytecodeOffsetFromRegisterPointer));
  __ SmiToInteger32(kInterpreterBytecodeOffsetRegister,
                    kInterpreterBytecodeOffsetRegister);

  // Dispatch to the target bytecode.
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(rbx, Operand(kInterpreterDispatchTableRegister, rbx,
                       times_pointer_size, 0));
  __ addp(rbx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(rbx);
}


static void Generate_InterpreterNotifyDeoptimizedHelper(
    MacroAssembler* masm, Deoptimizer::BailoutType type) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Pass the deoptimization type to the runtime system.
    __ Push(Smi::FromInt(static_cast<int>(type)));
    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  // Drop state (we don't use these for interpreter deopts) and and pop the
  // accumulator value into the accumulator register and push PC at top
  // of stack (to simulate initial call to bytecode handler in interpreter entry
  // trampoline).
  __ Pop(rbx);
  __ Drop(1);
  __ Pop(kInterpreterAccumulatorRegister);
  __ Push(rbx);

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
  __ Push(masm->isolate()->builtins()->InterpreterEntryTrampoline());

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
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Re-execute the code that was patched back to the young age when
  // the stub returns.
  __ subp(Operand(rsp, 0), Immediate(5));
  __ Pushad();
  __ Move(arg_reg_2, ExternalReference::isolate_address(masm->isolate()));
  __ movp(arg_reg_1, Operand(rsp, kNumSafepointRegisters * kPointerSize));
  {  // NOLINT
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(2);
    __ CallCFunction(
        ExternalReference::get_make_code_young_function(masm->isolate()), 2);
  }
  __ Popad();
  __ ret(0);
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
  __ Pushad();
  __ Move(arg_reg_2, ExternalReference::isolate_address(masm->isolate()));
  __ movp(arg_reg_1, Operand(rsp, kNumSafepointRegisters * kPointerSize));
  __ subp(arg_reg_1, Immediate(Assembler::kShortCallInstructionLength));
  {  // NOLINT
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(2);
    __ CallCFunction(
        ExternalReference::get_mark_code_as_executed_function(masm->isolate()),
        2);
  }
  __ Popad();

  // Perform prologue operations usually performed by the young code stub.
  __ PopReturnAddressTo(kScratchRegister);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS Function.
  __ PushReturnAddressFrom(kScratchRegister);

  // Jump to point after the code-age stub.
  __ ret(0);
}


void Builtins::Generate_MarkCodeAsExecutedTwice(MacroAssembler* masm) {
  GenerateMakeCodeYoungAgainCommon(masm);
}


void Builtins::Generate_MarkCodeAsToBeExecutedOnce(MacroAssembler* masm) {
  Generate_MarkCodeAsExecutedOnce(masm);
}


static void Generate_NotifyStubFailureHelper(MacroAssembler* masm,
                                             SaveFPRegsMode save_doubles) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ Pushad();
    __ CallRuntime(Runtime::kNotifyStubFailure, save_doubles);
    __ Popad();
    // Tear down internal frame.
  }

  __ DropUnderReturnAddress(1);  // Ignore state offset
  __ ret(0);  // Return to IC Miss stub, continuation still on stack.
}


void Builtins::Generate_NotifyStubFailure(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kDontSaveFPRegs);
}


void Builtins::Generate_NotifyStubFailureSaveDoubles(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kSaveFPRegs);
}


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Pass the deoptimization type to the runtime system.
    __ Push(Smi::FromInt(static_cast<int>(type)));

    __ CallRuntime(Runtime::kNotifyDeoptimized);
    // Tear down internal frame.
  }

  // Get the full codegen state from the stack and untag it.
  __ SmiToInteger32(kScratchRegister, Operand(rsp, kPCOnStackSize));

  // Switch on the state.
  Label not_no_registers, not_tos_rax;
  __ cmpp(kScratchRegister, Immediate(FullCodeGenerator::NO_REGISTERS));
  __ j(not_equal, &not_no_registers, Label::kNear);
  __ ret(1 * kPointerSize);  // Remove state.

  __ bind(&not_no_registers);
  __ movp(rax, Operand(rsp, kPCOnStackSize + kPointerSize));
  __ cmpp(kScratchRegister, Immediate(FullCodeGenerator::TOS_REG));
  __ j(not_equal, &not_tos_rax, Label::kNear);
  __ ret(2 * kPointerSize);  // Remove state, rax.

  __ bind(&not_tos_rax);
  __ Abort(kNoCasesLeft);
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


// static
void Builtins::Generate_DatePrototype_GetField(MacroAssembler* masm,
                                               int field_index) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  // 1. Load receiver into rax and check that it's actually a JSDate object.
  Label receiver_not_date;
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(rax, args.GetReceiverOperand());
    __ JumpIfSmi(rax, &receiver_not_date);
    __ CmpObjectType(rax, JS_DATE_TYPE, rbx);
    __ j(not_equal, &receiver_not_date);
  }

  // 2. Load the specified date field, falling back to the runtime as necessary.
  if (field_index == JSDate::kDateValue) {
    __ movp(rax, FieldOperand(rax, JSDate::kValueOffset));
  } else {
    if (field_index < JSDate::kFirstUncachedField) {
      Label stamp_mismatch;
      __ Load(rdx, ExternalReference::date_cache_stamp(masm->isolate()));
      __ cmpp(rdx, FieldOperand(rax, JSDate::kCacheStampOffset));
      __ j(not_equal, &stamp_mismatch, Label::kNear);
      __ movp(rax, FieldOperand(
                       rax, JSDate::kValueOffset + field_index * kPointerSize));
      __ ret(1 * kPointerSize);
      __ bind(&stamp_mismatch);
    }
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ PrepareCallCFunction(2);
    __ Move(arg_reg_1, rax);
    __ Move(arg_reg_2, Smi::FromInt(field_index));
    __ CallCFunction(
        ExternalReference::get_date_field_function(masm->isolate()), 2);
  }
  __ ret(1 * kPointerSize);

  // 3. Raise a TypeError if the receiver is not a date.
  __ bind(&receiver_not_date);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ EnterFrame(StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowNotDateError);
  }
}


// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : argArray
  //  -- rsp[16] : thisArg
  //  -- rsp[24] : receiver
  // -----------------------------------

  // 1. Load receiver into rdi, argArray into rax (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    Label no_arg_array, no_this_arg;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
    __ movp(rbx, rdx);
    __ movp(rdi, args.GetReceiverOperand());
    __ testp(rax, rax);
    __ j(zero, &no_this_arg, Label::kNear);
    {
      __ movp(rdx, args.GetArgumentOperand(1));
      __ cmpp(rax, Immediate(1));
      __ j(equal, &no_arg_array, Label::kNear);
      __ movp(rbx, args.GetArgumentOperand(2));
      __ bind(&no_arg_array);
    }
    __ bind(&no_this_arg);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ Push(rdx);
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // ----------- S t a t e -------------
  //  -- rax     : argArray
  //  -- rdi     : receiver
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArg
  // -----------------------------------

  // 2. Make sure the receiver is actually callable.
  Label receiver_not_callable;
  __ JumpIfSmi(rdi, &receiver_not_callable, Label::kNear);
  __ movp(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &receiver_not_callable, Label::kNear);

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(rax, Heap::kNullValueRootIndex, &no_arguments, Label::kNear);
  __ JumpIfRoot(rax, Heap::kUndefinedValueRootIndex, &no_arguments,
                Label::kNear);

  // 4a. Apply the receiver to the given argArray (passing undefined for
  // new.target).
  __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver. Since we did not create a frame for
  // Function.prototype.apply() yet, we use a normal Call builtin here.
  __ bind(&no_arguments);
  {
    __ Set(rax, 0);
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }

  // 4c. The receiver is not callable, throw an appropriate TypeError.
  __ bind(&receiver_not_callable);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdi);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}


// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // Stack Layout:
  // rsp[0]           : Return address
  // rsp[8]           : Argument n
  // rsp[16]          : Argument n-1
  //  ...
  // rsp[8 * n]       : Argument 1
  // rsp[8 * (n + 1)] : Receiver (callable to call)
  //
  // rax contains the number of arguments, n, not counting the receiver.
  //
  // 1. Make sure we have at least one argument.
  {
    Label done;
    __ testp(rax, rax);
    __ j(not_zero, &done, Label::kNear);
    __ PopReturnAddressTo(rbx);
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ PushReturnAddressFrom(rbx);
    __ incp(rax);
    __ bind(&done);
  }

  // 2. Get the callable to call (passed as receiver) from the stack.
  {
    StackArgumentsAccessor args(rsp, rax);
    __ movp(rdi, args.GetReceiverOperand());
  }

  // 3. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  {
    Label loop;
    __ movp(rcx, rax);
    StackArgumentsAccessor args(rsp, rcx);
    __ bind(&loop);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ movp(args.GetArgumentOperand(0), rbx);
    __ decp(rcx);
    __ j(not_zero, &loop);              // While non-zero.
    __ DropUnderReturnAddress(1, rbx);  // Drop one slot under return address.
    __ decp(rax);  // One fewer argument (first argument is new receiver).
  }

  // 4. Call the callable.
  // Since we did not create a frame for Function.prototype.call() yet,
  // we use a normal Call builtin here.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : argumentsList
  //  -- rsp[16] : thisArgument
  //  -- rsp[24] : target
  //  -- rsp[32] : receiver
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rax (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdi, Heap::kUndefinedValueRootIndex);
    __ movp(rdx, rdi);
    __ movp(rbx, rdi);
    __ cmpp(rax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ movp(rdi, args.GetArgumentOperand(1));  // target
    __ j(equal, &done, Label::kNear);
    __ movp(rdx, args.GetArgumentOperand(2));  // thisArgument
    __ cmpp(rax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(3));  // argumentsList
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ Push(rdx);
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // ----------- S t a t e -------------
  //  -- rax     : argumentsList
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : thisArgument
  // -----------------------------------

  // 2. Make sure the target is actually callable.
  Label target_not_callable;
  __ JumpIfSmi(rdi, &target_not_callable, Label::kNear);
  __ movp(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &target_not_callable, Label::kNear);

  // 3a. Apply the target to the given argumentsList (passing undefined for
  // new.target).
  __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 3b. The target is not callable, throw an appropriate TypeError.
  __ bind(&target_not_callable);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdi);
    __ TailCallRuntime(Runtime::kThrowApplyNonFunction);
  }
}


void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : argc
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : new.target (optional)
  //  -- rsp[16] : argumentsList
  //  -- rsp[24] : target
  //  -- rsp[32] : receiver
  // -----------------------------------

  // 1. Load target into rdi (if present), argumentsList into rax (if present),
  // new.target into rdx (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    Label done;
    StackArgumentsAccessor args(rsp, rax);
    __ LoadRoot(rdi, Heap::kUndefinedValueRootIndex);
    __ movp(rdx, rdi);
    __ movp(rbx, rdi);
    __ cmpp(rax, Immediate(1));
    __ j(below, &done, Label::kNear);
    __ movp(rdi, args.GetArgumentOperand(1));  // target
    __ movp(rdx, rdi);                         // new.target defaults to target
    __ j(equal, &done, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(2));  // argumentsList
    __ cmpp(rax, Immediate(3));
    __ j(below, &done, Label::kNear);
    __ movp(rdx, args.GetArgumentOperand(3));  // new.target
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ PushRoot(Heap::kUndefinedValueRootIndex);
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // ----------- S t a t e -------------
  //  -- rax     : argumentsList
  //  -- rdx     : new.target
  //  -- rdi     : target
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : receiver (undefined)
  // -----------------------------------

  // 2. Make sure the target is actually a constructor.
  Label target_not_constructor;
  __ JumpIfSmi(rdi, &target_not_constructor, Label::kNear);
  __ movp(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &target_not_constructor, Label::kNear);

  // 3. Make sure the target is actually a constructor.
  Label new_target_not_constructor;
  __ JumpIfSmi(rdx, &new_target_not_constructor, Label::kNear);
  __ movp(rcx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &new_target_not_constructor, Label::kNear);

  // 4a. Construct the target with the given new.target and argumentsList.
  __ Jump(masm->isolate()->builtins()->Apply(), RelocInfo::CODE_TARGET);

  // 4b. The target is not a constructor, throw an appropriate TypeError.
  __ bind(&target_not_constructor);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdi);
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }

  // 4c. The new.target is not a constructor, throw an appropriate TypeError.
  __ bind(&new_target_not_constructor);
  {
    StackArgumentsAccessor args(rsp, 0);
    __ movp(args.GetReceiverOperand(), rdx);
    __ TailCallRuntime(Runtime::kThrowCalledNonCallable);
  }
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the InternalArray function.
  __ LoadNativeContextSlot(Context::INTERNAL_ARRAY_FUNCTION_INDEX, rdi);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ movp(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rbx));
    __ Check(not_smi, kUnexpectedInitialMapForInternalArrayFunction);
    __ CmpObjectType(rbx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the Array function.
  __ LoadNativeContextSlot(Context::ARRAY_FUNCTION_INDEX, rdi);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ movp(rbx, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    STATIC_ASSERT(kSmiTag == 0);
    Condition not_smi = NegateCondition(masm->CheckSmi(rbx));
    __ Check(not_smi, kUnexpectedInitialMapForArrayFunction);
    __ CmpObjectType(rbx, MAP_TYPE, rcx);
    __ Check(equal, kUnexpectedInitialMapForArrayFunction);
  }

  __ movp(rdx, rdi);
  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


// static
void Builtins::Generate_MathMaxMin(MacroAssembler* masm, MathMaxMinKind kind) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------
  Condition const cc = (kind == MathMaxMinKind::kMin) ? below : above;
  Heap::RootListIndex const root_index =
      (kind == MathMaxMinKind::kMin) ? Heap::kInfinityValueRootIndex
                                     : Heap::kMinusInfinityValueRootIndex;
  XMMRegister const reg = (kind == MathMaxMinKind::kMin) ? xmm1 : xmm0;

  // Load the accumulator with the default return value (either -Infinity or
  // +Infinity), with the tagged value in rdx and the double value in xmm0.
  __ LoadRoot(rdx, root_index);
  __ Movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
  __ Move(rcx, rax);

  Label done_loop, loop;
  __ bind(&loop);
  {
    // Check if all parameters done.
    __ testp(rcx, rcx);
    __ j(zero, &done_loop);

    // Load the next parameter tagged value into rbx.
    __ movp(rbx, Operand(rsp, rcx, times_pointer_size, 0));

    // Load the double value of the parameter into xmm1, maybe converting the
    // parameter to a number first using the ToNumberStub if necessary.
    Label convert, convert_smi, convert_number, done_convert;
    __ bind(&convert);
    __ JumpIfSmi(rbx, &convert_smi);
    __ JumpIfRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                  Heap::kHeapNumberMapRootIndex, &convert_number);
    {
      // Parameter is not a Number, use the ToNumberStub to convert it.
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Integer32ToSmi(rax, rax);
      __ Integer32ToSmi(rcx, rcx);
      __ Push(rax);
      __ Push(rcx);
      __ Push(rdx);
      __ movp(rax, rbx);
      ToNumberStub stub(masm->isolate());
      __ CallStub(&stub);
      __ movp(rbx, rax);
      __ Pop(rdx);
      __ Pop(rcx);
      __ Pop(rax);
      {
        // Restore the double accumulator value (xmm0).
        Label restore_smi, done_restore;
        __ JumpIfSmi(rdx, &restore_smi, Label::kNear);
        __ Movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
        __ jmp(&done_restore, Label::kNear);
        __ bind(&restore_smi);
        __ SmiToDouble(xmm0, rdx);
        __ bind(&done_restore);
      }
      __ SmiToInteger32(rcx, rcx);
      __ SmiToInteger32(rax, rax);
    }
    __ jmp(&convert);
    __ bind(&convert_number);
    __ Movsd(xmm1, FieldOperand(rbx, HeapNumber::kValueOffset));
    __ jmp(&done_convert, Label::kNear);
    __ bind(&convert_smi);
    __ SmiToDouble(xmm1, rbx);
    __ bind(&done_convert);

    // Perform the actual comparison with the accumulator value on the left hand
    // side (xmm0) and the next parameter value on the right hand side (xmm1).
    Label compare_equal, compare_nan, compare_swap, done_compare;
    __ Ucomisd(xmm0, xmm1);
    __ j(parity_even, &compare_nan, Label::kNear);
    __ j(cc, &done_compare, Label::kNear);
    __ j(equal, &compare_equal, Label::kNear);

    // Result is on the right hand side.
    __ bind(&compare_swap);
    __ Movaps(xmm0, xmm1);
    __ Move(rdx, rbx);
    __ jmp(&done_compare, Label::kNear);

    // At least one side is NaN, which means that the result will be NaN too.
    __ bind(&compare_nan);
    __ LoadRoot(rdx, Heap::kNanValueRootIndex);
    __ Movsd(xmm0, FieldOperand(rdx, HeapNumber::kValueOffset));
    __ jmp(&done_compare, Label::kNear);

    // Left and right hand side are equal, check for -0 vs. +0.
    __ bind(&compare_equal);
    __ Movmskpd(kScratchRegister, reg);
    __ testl(kScratchRegister, Immediate(1));
    __ j(not_zero, &compare_swap);

    __ bind(&done_compare);
    __ decp(rcx);
    __ jmp(&loop);
  }

  __ bind(&done_loop);
  __ PopReturnAddressTo(rcx);
  __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
  __ PushReturnAddressFrom(rcx);
  __ movp(rax, rdx);
  __ Ret();
}

// static
void Builtins::Generate_NumberConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Load the first argument into rax and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    StackArgumentsAccessor args(rsp, rax);
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // 2a. Convert the first argument to a number.
  ToNumberStub stub(masm->isolate());
  __ TailCallStub(&stub);

  // 2b. No arguments, return +0 (already in rax).
  __ bind(&no_arguments);
  __ ret(1 * kPointerSize);
}


// static
void Builtins::Generate_NumberConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rdx                 : new target
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // 2. Load the first argument into rbx and get rid of the rest (including the
  // receiver).
  {
    StackArgumentsAccessor args(rsp, rax);
    Label no_arguments, done;
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ jmp(&done, Label::kNear);
    __ bind(&no_arguments);
    __ Move(rbx, Smi::FromInt(0));
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
  }

  // 3. Make sure rbx is a number.
  {
    Label done_convert;
    __ JumpIfSmi(rbx, &done_convert);
    __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                   Heap::kHeapNumberMapRootIndex);
    __ j(equal, &done_convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(rdx);
      __ Push(rdi);
      __ Move(rax, rbx);
      ToNumberStub stub(masm->isolate());
      __ CallStub(&stub);
      __ Move(rbx, rax);
      __ Pop(rdi);
      __ Pop(rdx);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label new_object;
  __ cmpp(rdx, rdi);
  __ j(not_equal, &new_object);

  // 5. Allocate a JSValue wrapper for the number.
  __ AllocateJSValue(rax, rdi, rbx, rcx, &new_object);
  __ Ret();

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rbx);  // the first argument
    FastNewObjectStub stub(masm->isolate());
    __ CallStub(&stub);
    __ Pop(FieldOperand(rax, JSValue::kValueOffset));
  }
  __ Ret();
}


// static
void Builtins::Generate_StringConstructor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Load the first argument into rax and get rid of the rest (including the
  // receiver).
  Label no_arguments;
  {
    StackArgumentsAccessor args(rsp, rax);
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
    __ movp(rax, rbx);
  }

  // 2a. At least one argument, return rax if it's a string, otherwise
  // dispatch to appropriate conversion.
  Label to_string, symbol_descriptive_string;
  {
    __ JumpIfSmi(rax, &to_string, Label::kNear);
    STATIC_ASSERT(FIRST_NONSTRING_TYPE == SYMBOL_TYPE);
    __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rdx);
    __ j(above, &to_string, Label::kNear);
    __ j(equal, &symbol_descriptive_string, Label::kNear);
    __ Ret();
  }

  // 2b. No arguments, return the empty string (and pop the receiver).
  __ bind(&no_arguments);
  {
    __ LoadRoot(rax, Heap::kempty_stringRootIndex);
    __ ret(1 * kPointerSize);
  }

  // 3a. Convert rax to a string.
  __ bind(&to_string);
  {
    ToStringStub stub(masm->isolate());
    __ TailCallStub(&stub);
  }

  // 3b. Convert symbol in rax to a string.
  __ bind(&symbol_descriptive_string);
  {
    __ PopReturnAddressTo(rcx);
    __ Push(rax);
    __ PushReturnAddressFrom(rcx);
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString);
  }
}


// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rdx                 : new target
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Make sure we operate in the context of the called function.
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // 2. Load the first argument into rbx and get rid of the rest (including the
  // receiver).
  {
    StackArgumentsAccessor args(rsp, rax);
    Label no_arguments, done;
    __ testp(rax, rax);
    __ j(zero, &no_arguments, Label::kNear);
    __ movp(rbx, args.GetArgumentOperand(1));
    __ jmp(&done, Label::kNear);
    __ bind(&no_arguments);
    __ LoadRoot(rbx, Heap::kempty_stringRootIndex);
    __ bind(&done);
    __ PopReturnAddressTo(rcx);
    __ leap(rsp, Operand(rsp, rax, times_pointer_size, kPointerSize));
    __ PushReturnAddressFrom(rcx);
  }

  // 3. Make sure rbx is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(rbx, &convert, Label::kNear);
    __ CmpObjectType(rbx, FIRST_NONSTRING_TYPE, rcx);
    __ j(below, &done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(rdx);
      __ Push(rdi);
      __ Move(rax, rbx);
      __ CallStub(&stub);
      __ Move(rbx, rax);
      __ Pop(rdi);
      __ Pop(rdx);
    }
    __ bind(&done_convert);
  }

  // 4. Check if new target and constructor differ.
  Label new_object;
  __ cmpp(rdx, rdi);
  __ j(not_equal, &new_object);

  // 5. Allocate a JSValue wrapper for the string.
  __ AllocateJSValue(rax, rdi, rbx, rcx, &new_object);
  __ Ret();

  // 6. Fallback to the runtime to create new object.
  __ bind(&new_object);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rbx);  // the first argument
    FastNewObjectStub stub(masm->isolate());
    __ CallStub(&stub);
    __ Pop(FieldOperand(rax, JSValue::kValueOffset));
  }
  __ Ret();
}


static void ArgumentsAdaptorStackCheck(MacroAssembler* masm,
                                       Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rdx : new target (passed through to callee)
  //  -- rdi : function (passed through to callee)
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(r8, Heap::kRealStackLimitRootIndex);
  __ movp(rcx, rsp);
  // Make rcx the space we have left. The stack might already be overflowed
  // here which will cause rcx to become negative.
  __ subp(rcx, r8);
  // Make r8 the space we need for the array when it is unrolled onto the
  // stack.
  __ movp(r8, rbx);
  __ shlp(r8, Immediate(kPointerSizeLog2));
  // Check if the arguments will overflow the stack.
  __ cmpp(rcx, r8);
  __ j(less_equal, stack_overflow);  // Signed comparison.
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ pushq(rbp);
  __ movp(rbp, rsp);

  // Store the arguments adaptor context sentinel.
  __ Push(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));

  // Push the function on the stack.
  __ Push(rdi);

  // Preserve the number of arguments on the stack. Must preserve rax,
  // rbx and rcx because these registers are used when copying the
  // arguments and the receiver.
  __ Integer32ToSmi(r8, rax);
  __ Push(r8);
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // Retrieve the number of arguments from the stack. Number is a Smi.
  __ movp(rbx, Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));

  // Leave the frame.
  __ movp(rsp, rbp);
  __ popq(rbp);

  // Remove caller arguments from the stack.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rdx : new target (passed through to callee)
  //  -- rdi : function (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments, stack_overflow;
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->arguments_adaptors(), 1);

  Label enough, too_few;
  __ cmpp(rax, rbx);
  __ j(less, &too_few);
  __ cmpp(rbx, Immediate(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);
    ArgumentsAdaptorStackCheck(masm, &stack_overflow);

    // Copy receiver and all expected arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ leap(rax, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incp(r8);
    __ Push(Operand(rax, 0));
    __ subp(rax, Immediate(kPointerSize));
    __ cmpp(r8, rbx);
    __ j(less, &copy);
    __ jmp(&invoke);
  }

  {  // Too few parameters: Actual < expected.
    __ bind(&too_few);

    // If the function is strong we need to throw an error.
    Label no_strong_error;
    __ movp(kScratchRegister,
            FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ testb(FieldOperand(kScratchRegister,
                          SharedFunctionInfo::kStrongModeByteOffset),
             Immediate(1 << SharedFunctionInfo::kStrongModeBitWithinByte));
    __ j(equal, &no_strong_error, Label::kNear);

    // What we really care about is the required number of arguments.

    if (kPointerSize == kInt32Size) {
      __ movp(
          kScratchRegister,
          FieldOperand(kScratchRegister, SharedFunctionInfo::kLengthOffset));
      __ SmiToInteger32(kScratchRegister, kScratchRegister);
    } else {
      // See comment near kLengthOffset in src/objects.h
      __ movsxlq(
          kScratchRegister,
          FieldOperand(kScratchRegister, SharedFunctionInfo::kLengthOffset));
      __ shrq(kScratchRegister, Immediate(1));
    }

    __ cmpp(rax, kScratchRegister);
    __ j(greater_equal, &no_strong_error, Label::kNear);

    {
      FrameScope frame(masm, StackFrame::MANUAL);
      EnterArgumentsAdaptorFrame(masm);
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments);
    }

    __ bind(&no_strong_error);
    EnterArgumentsAdaptorFrame(masm);
    ArgumentsAdaptorStackCheck(masm, &stack_overflow);

    // Copy receiver and all actual arguments.
    const int offset = StandardFrameConstants::kCallerSPOffset;
    __ leap(rdi, Operand(rbp, rax, times_pointer_size, offset));
    __ Set(r8, -1);  // account for receiver

    Label copy;
    __ bind(&copy);
    __ incp(r8);
    __ Push(Operand(rdi, 0));
    __ subp(rdi, Immediate(kPointerSize));
    __ cmpp(r8, rax);
    __ j(less, &copy);

    // Fill remaining expected arguments with undefined values.
    Label fill;
    __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
    __ bind(&fill);
    __ incp(r8);
    __ Push(kScratchRegister);
    __ cmpp(r8, rbx);
    __ j(less, &fill);

    // Restore function pointer.
    __ movp(rdi, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  }

  // Call the entry point.
  __ bind(&invoke);
  __ movp(rax, rbx);
  // rax : expected number of arguments
  // rdx : new target (passed through to callee)
  // rdi : function (passed through to callee)
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  __ call(rcx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ movp(rcx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  __ jmp(rcx);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ int3();
  }
}


// static
void Builtins::Generate_Apply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argumentsList
  //  -- rdi    : target
  //  -- rdx    : new.target (checked to be constructor or undefined)
  //  -- rsp[0] : return address.
  //  -- rsp[8] : thisArgument
  // -----------------------------------

  // Create the list of arguments from the array-like argumentsList.
  {
    Label create_arguments, create_array, create_runtime, done_create;
    __ JumpIfSmi(rax, &create_runtime);

    // Load the map of argumentsList into rcx.
    __ movp(rcx, FieldOperand(rax, HeapObject::kMapOffset));

    // Load native context into rbx.
    __ movp(rbx, NativeContextOperand());

    // Check if argumentsList is an (unmodified) arguments object.
    __ cmpp(rcx, ContextOperand(rbx, Context::SLOPPY_ARGUMENTS_MAP_INDEX));
    __ j(equal, &create_arguments);
    __ cmpp(rcx, ContextOperand(rbx, Context::STRICT_ARGUMENTS_MAP_INDEX));
    __ j(equal, &create_arguments);

    // Check if argumentsList is a fast JSArray.
    __ CmpInstanceType(rcx, JS_ARRAY_TYPE);
    __ j(equal, &create_array);

    // Ask the runtime to create the list (actually a FixedArray).
    __ bind(&create_runtime);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(rdi);
      __ Push(rdx);
      __ Push(rax);
      __ CallRuntime(Runtime::kCreateListFromArrayLike);
      __ Pop(rdx);
      __ Pop(rdi);
      __ SmiToInteger32(rbx, FieldOperand(rax, FixedArray::kLengthOffset));
    }
    __ jmp(&done_create);

    // Try to create the list from an arguments object.
    __ bind(&create_arguments);
    __ movp(rbx, FieldOperand(rax, JSArgumentsObject::kLengthOffset));
    __ movp(rcx, FieldOperand(rax, JSObject::kElementsOffset));
    __ cmpp(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
    __ j(not_equal, &create_runtime);
    __ SmiToInteger32(rbx, rbx);
    __ movp(rax, rcx);
    __ jmp(&done_create);

    // Try to create the list from a JSArray object.
    __ bind(&create_array);
    __ movzxbp(rcx, FieldOperand(rcx, Map::kBitField2Offset));
    __ DecodeField<Map::ElementsKindBits>(rcx);
    STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
    STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
    STATIC_ASSERT(FAST_ELEMENTS == 2);
    __ cmpl(rcx, Immediate(FAST_ELEMENTS));
    __ j(above, &create_runtime);
    __ cmpl(rcx, Immediate(FAST_HOLEY_SMI_ELEMENTS));
    __ j(equal, &create_runtime);
    __ SmiToInteger32(rbx, FieldOperand(rax, JSArray::kLengthOffset));
    __ movp(rax, FieldOperand(rax, JSArray::kElementsOffset));

    __ bind(&done_create);
  }

  // Check for stack overflow.
  {
    // Check the stack for overflow. We are not trying to catch interruptions
    // (i.e. debug break and preemption) here, so check the "real stack limit".
    Label done;
    __ LoadRoot(kScratchRegister, Heap::kRealStackLimitRootIndex);
    __ movp(rcx, rsp);
    // Make rcx the space we have left. The stack might already be overflowed
    // here which will cause rcx to become negative.
    __ subp(rcx, kScratchRegister);
    __ sarp(rcx, Immediate(kPointerSizeLog2));
    // Check if the arguments will overflow the stack.
    __ cmpp(rcx, rbx);
    __ j(greater, &done, Label::kNear);  // Signed comparison.
    __ TailCallRuntime(Runtime::kThrowStackOverflow);
    __ bind(&done);
  }

  // ----------- S t a t e -------------
  //  -- rdi    : target
  //  -- rax    : args (a FixedArray built from argumentsList)
  //  -- rbx    : len (number of elements to push from args)
  //  -- rdx    : new.target (checked to be constructor or undefined)
  //  -- rsp[0] : return address.
  //  -- rsp[8] : thisArgument
  // -----------------------------------

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    __ PopReturnAddressTo(r8);
    __ Set(rcx, 0);
    Label done, loop;
    __ bind(&loop);
    __ cmpl(rcx, rbx);
    __ j(equal, &done, Label::kNear);
    __ Push(
        FieldOperand(rax, rcx, times_pointer_size, FixedArray::kHeaderSize));
    __ incl(rcx);
    __ jmp(&loop);
    __ bind(&done);
    __ PushReturnAddressFrom(r8);
    __ Move(rax, rcx);
  }

  // Dispatch to Call or Construct depending on whether new.target is undefined.
  {
    __ CompareRoot(rdx, Heap::kUndefinedValueRootIndex);
    __ j(equal, masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
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
// |  f()'s receiver arg
// |  f()'s caller pc      <- sp
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
  __ Move(kScratchRegister, debug_is_active);
  __ cmpb(Operand(kScratchRegister, 0), Immediate(0));
  __ j(not_equal, &done);

  // Drop possible interpreter handler/stub frame.
  {
    Label no_interpreter_frame;
    __ Cmp(Operand(rbp, StandardFrameConstants::kMarkerOffset),
           Smi::FromInt(StackFrame::STUB));
    __ j(not_equal, &no_interpreter_frame, Label::kNear);
    __ movp(rbp, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
    __ bind(&no_interpreter_frame);
  }

  // Check if next frame is an arguments adaptor frame.
  Label no_arguments_adaptor, formal_parameter_count_loaded;
  __ movp(scratch2, Operand(rbp, StandardFrameConstants::kCallerFPOffset));
  __ Cmp(Operand(scratch2, StandardFrameConstants::kContextOffset),
         Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  __ j(not_equal, &no_arguments_adaptor, Label::kNear);

  // Drop arguments adaptor frame and load arguments count.
  __ movp(rbp, scratch2);
  __ SmiToInteger32(
      scratch1, Operand(rbp, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ jmp(&formal_parameter_count_loaded, Label::kNear);

  __ bind(&no_arguments_adaptor);
  // Load caller's formal parameter count
  __ movp(scratch1, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movp(scratch1,
          FieldOperand(scratch1, JSFunction::kSharedFunctionInfoOffset));
  __ LoadSharedFunctionInfoSpecialField(
      scratch1, scratch1, SharedFunctionInfo::kFormalParameterCountOffset);

  __ bind(&formal_parameter_count_loaded);

  // Calculate the destination address where we will put the return address
  // after we drop current frame.
  Register new_sp_reg = scratch2;
  __ subp(scratch1, args_reg);
  __ leap(new_sp_reg, Operand(rbp, scratch1, times_pointer_size,
                              StandardFrameConstants::kCallerPCOffset));

  if (FLAG_debug_code) {
    __ cmpp(rsp, new_sp_reg);
    __ Check(below, kStackAccessBelowStackPointer);
  }

  // Copy receiver and return address as well.
  Register count_reg = scratch1;
  __ leap(count_reg, Operand(args_reg, 2));

  // Copy return address from caller's frame to current frame's return address
  // to avoid its trashing and let the following loop copy it to the right
  // place.
  Register tmp_reg = scratch3;
  __ movp(tmp_reg, Operand(rbp, StandardFrameConstants::kCallerPCOffset));
  __ movp(Operand(rsp, 0), tmp_reg);

  // Restore caller's frame pointer now as it could be overwritten by
  // the copying loop.
  __ movp(rbp, Operand(rbp, StandardFrameConstants::kCallerFPOffset));

  Operand src(rsp, count_reg, times_pointer_size, 0);
  Operand dst(new_sp_reg, count_reg, times_pointer_size, 0);

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).
  Label loop, entry;
  __ jmp(&entry, Label::kNear);
  __ bind(&loop);
  __ decp(count_reg);
  __ movp(tmp_reg, src);
  __ movp(dst, tmp_reg);
  __ bind(&entry);
  __ cmpp(count_reg, Immediate(0));
  __ j(not_equal, &loop, Label::kNear);

  // Leave current frame.
  __ movp(rsp, new_sp_reg);

  __ bind(&done);
}
}  // namespace

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode,
                                     TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);
  __ AssertFunction(rdi);

  // ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  Label class_constructor;
  __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ testb(FieldOperand(rdx, SharedFunctionInfo::kFunctionKindByteOffset),
           Immediate(SharedFunctionInfo::kClassConstructorBitsWithinByte));
  __ j(not_zero, &class_constructor);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ testb(FieldOperand(rdx, SharedFunctionInfo::kNativeByteOffset),
           Immediate((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                     (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ j(not_zero, &done_convert);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments (not including the receiver)
    //  -- rdx : the shared function info.
    //  -- rdi : the function to call (checked to be a JSFunction)
    //  -- rsi : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(rcx);
    } else {
      Label convert_to_object, convert_receiver;
      __ movp(rcx, args.GetReceiverOperand());
      __ JumpIfSmi(rcx, &convert_to_object, Label::kNear);
      STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CmpObjectType(rcx, FIRST_JS_RECEIVER_TYPE, rbx);
      __ j(above_equal, &done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(rcx, Heap::kUndefinedValueRootIndex,
                      &convert_global_proxy, Label::kNear);
        __ JumpIfNotRoot(rcx, Heap::kNullValueRootIndex, &convert_to_object,
                         Label::kNear);
        __ bind(&convert_global_proxy);
        {
          // Patch receiver to global proxy.
          __ LoadGlobalProxy(rcx);
        }
        __ jmp(&convert_receiver);
      }
      __ bind(&convert_to_object);
      {
        // Convert receiver using ToObject.
        // TODO(bmeurer): Inline the allocation here to avoid building the frame
        // in the fast case? (fall back to AllocateInNewSpace?)
        FrameScope scope(masm, StackFrame::INTERNAL);
        __ Integer32ToSmi(rax, rax);
        __ Push(rax);
        __ Push(rdi);
        __ movp(rax, rcx);
        ToObjectStub stub(masm->isolate());
        __ CallStub(&stub);
        __ movp(rcx, rax);
        __ Pop(rdi);
        __ Pop(rax);
        __ SmiToInteger32(rax, rax);
      }
      __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ movp(args.GetReceiverOperand(), rcx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  //  -- rsi : the function context.
  // -----------------------------------

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, rax, rbx, rcx, r8);
  }

  __ LoadSharedFunctionInfoSpecialField(
      rbx, rdx, SharedFunctionInfo::kFormalParameterCountOffset);
  ParameterCount actual(rax);
  ParameterCount expected(rbx);

  __ InvokeFunctionCode(rdi, no_reg, expected, actual, JUMP_FUNCTION,
                        CheckDebugStepCallWrapper());

  // The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameScope frame(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
  }
}


namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : new.target (only in case of [[Construct]])
  //  -- rdi : target (checked to be a JSBoundFunction)
  // -----------------------------------

  // Load [[BoundArguments]] into rcx and length of that into rbx.
  Label no_bound_arguments;
  __ movp(rcx, FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiToInteger32(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ testl(rbx, rbx);
  __ j(zero, &no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- rax : the number of arguments (not including the receiver)
    //  -- rdx : new.target (only in case of [[Construct]])
    //  -- rdi : target (checked to be a JSBoundFunction)
    //  -- rcx : the [[BoundArguments]] (implemented as FixedArray)
    //  -- rbx : the number of [[BoundArguments]] (checked to be non-zero)
    // -----------------------------------

    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ leap(kScratchRegister, Operand(rbx, times_pointer_size, 0));
      __ subp(rsp, kScratchRegister);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CompareRoot(rsp, Heap::kRealStackLimitRootIndex);
      __ j(greater, &done, Label::kNear);  // Signed comparison.
      // Restore the stack pointer.
      __ leap(rsp, Operand(rsp, rbx, times_pointer_size, 0));
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Adjust effective number of arguments to include return address.
    __ incl(rax);

    // Relocate arguments and return address down the stack.
    {
      Label loop;
      __ Set(rcx, 0);
      __ leap(rbx, Operand(rsp, rbx, times_pointer_size, 0));
      __ bind(&loop);
      __ movp(kScratchRegister, Operand(rbx, rcx, times_pointer_size, 0));
      __ movp(Operand(rsp, rcx, times_pointer_size, 0), kScratchRegister);
      __ incl(rcx);
      __ cmpl(rcx, rax);
      __ j(less, &loop);
    }

    // Copy [[BoundArguments]] to the stack (below the arguments).
    {
      Label loop;
      __ movp(rcx, FieldOperand(rdi, JSBoundFunction::kBoundArgumentsOffset));
      __ SmiToInteger32(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
      __ bind(&loop);
      __ decl(rbx);
      __ movp(kScratchRegister, FieldOperand(rcx, rbx, times_pointer_size,
                                             FixedArray::kHeaderSize));
      __ movp(Operand(rsp, rax, times_pointer_size, 0), kScratchRegister);
      __ leal(rax, Operand(rax, 1));
      __ j(greater, &loop);
    }

    // Adjust effective number of arguments (rax contains the number of
    // arguments from the call plus return address plus the number of
    // [[BoundArguments]]), so we need to subtract one for the return address.
    __ decl(rax);
  }
  __ bind(&no_bound_arguments);
}

}  // namespace


// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                              TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(rdi);

  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, rax, rbx, rcx, r8);
  }

  // Patch the receiver to [[BoundThis]].
  StackArgumentsAccessor args(rsp, rax);
  __ movp(rbx, FieldOperand(rdi, JSBoundFunction::kBoundThisOffset));
  __ movp(args.GetReceiverOperand(), rbx);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ movp(rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Load(rcx,
          ExternalReference(Builtins::kCall_ReceiverIsAny, masm->isolate()));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);
}


// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                             TailCallMode tail_call_mode) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the target to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(rdi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(equal, masm->isolate()->builtins()->CallFunction(mode, tail_call_mode),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->CallBoundFunction(tail_call_mode),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Call]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &non_callable);

  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ j(not_equal, &non_function);

  // 0. Prepare for tail call if necessary.
  if (tail_call_mode == TailCallMode::kAllow) {
    PrepareForTailCall(masm, rax, rbx, rcx, r8);
  }

  // 1. Runtime fallback for Proxy [[Call]].
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(rdi);
  __ PushReturnAddressFrom(kScratchRegister);
  // Increase the arguments size to include the pushed function and the
  // existing receiver on the stack.
  __ addp(rax, Immediate(2));
  // Tail-call to the runtime.
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyCall, masm->isolate()));

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Overwrite the original receiver with the (original) target.
  __ movp(args.GetReceiverOperand(), rdi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, rdi);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined, tail_call_mode),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
  }
}


// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(rdi);

  // Calling convention for function specific ConstructStubs require
  // rbx to contain either an AllocationSite or undefined.
  __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);

  // Tail call to the function-specific construct stub (still in the caller
  // context at this point).
  __ movp(rcx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rcx, FieldOperand(rcx, SharedFunctionInfo::kConstructStubOffset));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);
}


// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (checked to be a constructor)
  //  -- rdi : the constructor to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(rdi);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  {
    Label done;
    __ cmpp(rdi, rdx);
    __ j(not_equal, &done, Label::kNear);
    __ movp(rdx,
            FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
    __ bind(&done);
  }

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ movp(rdi, FieldOperand(rdi, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Load(rcx, ExternalReference(Builtins::kConstruct, masm->isolate()));
  __ leap(rcx, FieldOperand(rcx, Code::kHeaderSize));
  __ jmp(rcx);
}


// static
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the constructor to call (checked to be a JSProxy)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  // -----------------------------------

  // Call into the Runtime for Proxy [[Construct]].
  __ PopReturnAddressTo(kScratchRegister);
  __ Push(rdi);
  __ Push(rdx);
  __ PushReturnAddressFrom(kScratchRegister);
  // Include the pushed new_target, constructor and the receiver.
  __ addp(rax, Immediate(3));
  __ JumpToExternalReference(
      ExternalReference(Runtime::kJSProxyConstruct, masm->isolate()));
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the new target (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  // Check if target is a Smi.
  Label non_constructor;
  __ JumpIfSmi(rdi, &non_constructor, Label::kNear);

  // Dispatch based on instance type.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(equal, masm->isolate()->builtins()->ConstructFunction(),
       RelocInfo::CODE_TARGET);

  // Check if target has a [[Construct]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &non_constructor, Label::kNear);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpInstanceType(rcx, JS_BOUND_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructBoundFunction(),
       RelocInfo::CODE_TARGET);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpInstanceType(rcx, JS_PROXY_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructProxy(),
       RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ movp(args.GetReceiverOperand(), rdi);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, rdi);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(masm->isolate()->builtins()->ConstructedNonConstructable(),
          RelocInfo::CODE_TARGET);
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
  __ movp(signature, FieldOperand(function_template_info,
                                  FunctionTemplateInfo::kSignatureOffset));
  __ CompareRoot(signature, Heap::kUndefinedValueRootIndex);
  Label receiver_check_passed;
  __ j(equal, &receiver_check_passed, Label::kNear);

  // Walk the prototype chain.
  __ movp(map, FieldOperand(receiver, HeapObject::kMapOffset));
  Label prototype_loop_start;
  __ bind(&prototype_loop_start);

  // Get the constructor, if any.
  __ GetMapConstructor(constructor, map, kScratchRegister);
  __ CmpInstanceType(kScratchRegister, JS_FUNCTION_TYPE);
  Label next_prototype;
  __ j(not_equal, &next_prototype, Label::kNear);

  // Get the constructor's signature.
  Register type = constructor;
  __ movp(type,
          FieldOperand(constructor, JSFunction::kSharedFunctionInfoOffset));
  __ movp(type, FieldOperand(type, SharedFunctionInfo::kFunctionDataOffset));

  // Loop through the chain of inheriting function templates.
  Label function_template_loop;
  __ bind(&function_template_loop);

  // If the signatures match, we have a compatible receiver.
  __ cmpp(signature, type);
  __ j(equal, &receiver_check_passed, Label::kNear);

  // If the current type is not a FunctionTemplateInfo, load the next prototype
  // in the chain.
  __ JumpIfSmi(type, &next_prototype, Label::kNear);
  __ CmpObjectType(type, FUNCTION_TEMPLATE_INFO_TYPE, kScratchRegister);
  __ j(not_equal, &next_prototype, Label::kNear);

  // Otherwise load the parent function template and iterate.
  __ movp(type,
          FieldOperand(type, FunctionTemplateInfo::kParentTemplateOffset));
  __ jmp(&function_template_loop, Label::kNear);

  // Load the next prototype.
  __ bind(&next_prototype);
  __ testq(FieldOperand(map, Map::kBitField3Offset),
           Immediate(Map::HasHiddenPrototype::kMask));
  __ j(zero, receiver_check_failed);
  __ movp(receiver, FieldOperand(map, Map::kPrototypeOffset));
  __ movp(map, FieldOperand(receiver, HeapObject::kMapOffset));
  // Iterate.
  __ jmp(&prototype_loop_start, Label::kNear);

  __ bind(&receiver_check_passed);
}


void Builtins::Generate_HandleFastApiCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                : number of arguments (not including the receiver)
  //  -- rdi                : callee
  //  -- rsi                : context
  //  -- rsp[0]             : return address
  //  -- rsp[8]             : last argument
  //  -- ...
  //  -- rsp[rax * 8]       : first argument
  //  -- rsp[(rax + 1) * 8] : receiver
  // -----------------------------------

  StackArgumentsAccessor args(rsp, rax);

  // Load the FunctionTemplateInfo.
  __ movp(rbx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(rbx, FieldOperand(rbx, SharedFunctionInfo::kFunctionDataOffset));

  // Do the compatible receiver check.
  Label receiver_check_failed;
  __ movp(rcx, args.GetReceiverOperand());
  CompatibleReceiverCheck(masm, rcx, rbx, rdx, r8, r9, &receiver_check_failed);

  // Get the callback offset from the FunctionTemplateInfo, and jump to the
  // beginning of the code.
  __ movp(rdx, FieldOperand(rbx, FunctionTemplateInfo::kCallCodeOffset));
  __ movp(rdx, FieldOperand(rdx, CallHandlerInfo::kFastHandlerOffset));
  __ addp(rdx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(rdx);

  // Compatible receiver check failed: pop return address, arguments and
  // receiver and throw an Illegal Invocation exception.
  __ bind(&receiver_check_failed);
  __ PopReturnAddressTo(rbx);
  __ leap(rax, Operand(rax, times_pointer_size, 1 * kPointerSize));
  __ addp(rsp, rax);
  __ PushReturnAddressFrom(rbx);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ TailCallRuntime(Runtime::kThrowIllegalInvocation);
  }
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ movp(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ Push(rax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement);
  }

  Label skip;
  // If the code object is null, just return to the unoptimized code.
  __ cmpp(rax, Immediate(0));
  __ j(not_equal, &skip, Label::kNear);
  __ ret(0);

  __ bind(&skip);

  // Load deoptimization data from the code object.
  __ movp(rbx, Operand(rax, Code::kDeoptimizationDataOffset - kHeapObjectTag));

  // Load the OSR entrypoint offset from the deoptimization data.
  __ SmiToInteger32(rbx, Operand(rbx, FixedArray::OffsetOfElementAt(
      DeoptimizationInputData::kOsrPcOffsetIndex) - kHeapObjectTag));

  // Compute the target address = code_obj + header_size + osr_offset
  __ leap(rax, Operand(rax, rbx, times_1, Code::kHeaderSize - kHeapObjectTag));

  // Overwrite the return address on the stack.
  __ movq(StackOperandForReturnAddress(0), rax);

  // And "return" to the OSR entry point of the function.
  __ ret(0);
}


void Builtins::Generate_OsrAfterStackCheck(MacroAssembler* masm) {
  // We check the stack limit as indicator that recompilation might be done.
  Label ok;
  __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
  __ j(above_equal, &ok);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kStackGuard);
  }
  __ jmp(masm->isolate()->builtins()->OnStackReplacement(),
         RelocInfo::CODE_TARGET);

  __ bind(&ok);
  __ ret(0);
}


#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

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
  //  -- rdi                 : called function (only guaranteed when
  //                           extra_args requires it)
  //  -- rsp[0]              : return address
  //  -- rsp[8]              : last argument
  //  -- ...
  //  -- rsp[8 * argc]       : first argument (argc == rax)
  //  -- rsp[8 * (argc + 1)] : receiver
  // -----------------------------------
  __ AssertFunction(rdi);

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  // TODO(bmeurer): Can we make this more robust?
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ PopReturnAddressTo(kScratchRegister);
    __ Push(rdi);
    __ PushReturnAddressFrom(kScratchRegister);
  } else {
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects rax to contain the number of arguments
  // including the receiver and the extra arguments.
  __ addp(rax, Immediate(num_extra_args + 1));
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()), 1);
}


static void CallRuntimePassFunction(
    MacroAssembler* masm, Runtime::FunctionId function_id) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  __ Push(rdi);
  // Function is also the parameter to the runtime call.
  __ Push(rdi);

  __ CallRuntime(function_id, 1);
  // Restore receiver.
  __ Pop(rdi);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ movp(kScratchRegister,
          FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(kScratchRegister,
          FieldOperand(kScratchRegister, SharedFunctionInfo::kCodeOffset));
  __ leap(kScratchRegister, FieldOperand(kScratchRegister, Code::kHeaderSize));
  __ jmp(kScratchRegister);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ leap(rax, FieldOperand(rax, Code::kHeaderSize));
  __ jmp(rax);
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

  CallRuntimePassFunction(masm, Runtime::kTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  //  -- rbx: allocation site or undefined
  //  -- rdx: original constructor
  // -----------------------------------

  // Enter a construct frame.
  {
    FrameScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ AssertUndefinedOrAllocationSite(rbx);
    __ Push(rbx);
    __ Integer32ToSmi(rax, rax);
    __ Push(rax);
    __ Push(rdi);
    __ Push(rdx);

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(masm->isolate());
      __ Move(kScratchRegister, debug_step_in_fp);
      __ cmpp(Operand(kScratchRegister, 0), Immediate(0));
      __ j(not_equal, &rt_call);

      // Fall back to runtime if the original constructor and function differ.
      __ cmpp(rdx, rdi);
      __ j(not_equal, &rt_call);

      // Verified that the constructor is a JSFunction.
      // Load the initial map and verify that it is in fact a map.
      // rdi: constructor
      __ movp(rax, FieldOperand(rdi, JSFunction::kPrototypeOrInitialMapOffset));
      // Will both indicate a NULL and a Smi
      DCHECK(kSmiTag == 0);
      __ JumpIfSmi(rax, &rt_call);
      // rdi: constructor
      // rax: initial map (if proven valid below)
      __ CmpObjectType(rax, MAP_TYPE, rbx);
      __ j(not_equal, &rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // rdi: constructor
      // rax: initial map
      __ CmpInstanceType(rax, JS_FUNCTION_TYPE);
      __ j(equal, &rt_call);
      if (!is_api_function) {
        Label allocate;
        // The code below relies on these assumptions.
        STATIC_ASSERT(Map::Counter::kShift + Map::Counter::kSize == 32);
        // Check if slack tracking is enabled.
        __ movl(rsi, FieldOperand(rax, Map::kBitField3Offset));
        __ shrl(rsi, Immediate(Map::Counter::kShift));
        __ cmpl(rsi, Immediate(Map::kSlackTrackingCounterEnd));
        __ j(less, &allocate);
        // Decrease generous allocation count.
        __ subl(FieldOperand(rax, Map::kBitField3Offset),
                Immediate(1 << Map::Counter::kShift));

        __ cmpl(rsi, Immediate(Map::kSlackTrackingCounterEnd));
        __ j(not_equal, &allocate);

        __ Push(rax);
        __ Push(rdx);
        __ Push(rdi);

        __ Push(rdi);  // constructor
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ Pop(rdi);
        __ Pop(rdx);
        __ Pop(rax);
        __ movl(rsi, Immediate(Map::kSlackTrackingCounterEnd - 1));

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      __ movzxbp(rdi, FieldOperand(rax, Map::kInstanceSizeOffset));
      __ shlp(rdi, Immediate(kPointerSizeLog2));
      // rdi: size of new object
      __ Allocate(rdi,
                  rbx,
                  rdi,
                  no_reg,
                  &rt_call,
                  NO_ALLOCATION_FLAGS);
      // Allocated the JSObject, now initialize the fields.
      // rax: initial map
      // rbx: JSObject (not HeapObject tagged - the actual address).
      // rdi: start of next object
      __ movp(Operand(rbx, JSObject::kMapOffset), rax);
      __ LoadRoot(rcx, Heap::kEmptyFixedArrayRootIndex);
      __ movp(Operand(rbx, JSObject::kPropertiesOffset), rcx);
      __ movp(Operand(rbx, JSObject::kElementsOffset), rcx);
      // Set extra fields in the newly allocated object.
      // rax: initial map
      // rbx: JSObject
      // rdi: start of next object
      // rsi: slack tracking counter (non-API function case)
      __ leap(rcx, Operand(rbx, JSObject::kHeaderSize));
      __ LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        __ cmpl(rsi, Immediate(Map::kSlackTrackingCounterEnd));
        __ j(less, &no_inobject_slack_tracking);

        // Allocate object with a slack.
        __ movzxbp(
            rsi,
            FieldOperand(
                rax, Map::kInObjectPropertiesOrConstructorFunctionIndexOffset));
        __ movzxbp(rax, FieldOperand(rax, Map::kUnusedPropertyFieldsOffset));
        __ subp(rsi, rax);
        __ leap(rsi,
               Operand(rbx, rsi, times_pointer_size, JSObject::kHeaderSize));
        // rsi: offset of first field after pre-allocated fields
        if (FLAG_debug_code) {
          __ cmpp(rsi, rdi);
          __ Assert(less_equal,
                    kUnexpectedNumberOfPreAllocatedPropertyFields);
        }
        __ InitializeFieldsWithFiller(rcx, rsi, rdx);
        __ LoadRoot(rdx, Heap::kOnePointerFillerMapRootIndex);
        // Fill the remaining fields with one pointer filler map.

        __ bind(&no_inobject_slack_tracking);
      }

      __ InitializeFieldsWithFiller(rcx, rdi, rdx);

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on.
      // rbx: JSObject (untagged)
      __ orp(rbx, Immediate(kHeapObjectTag));

      // Continue with JSObject being successfully allocated
      // rbx: JSObject (tagged)
      __ jmp(&allocated);
    }

    // Allocate the new receiver object using the runtime call.
    // rdx: original constructor
    __ bind(&rt_call);
    int offset = kPointerSize;

    // Must restore rsi (context) and rdi (constructor) before calling runtime.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
    __ movp(rdi, Operand(rsp, offset));
    __ Push(rdi);  // argument 2/1: constructor function
    __ Push(rdx);  // argument 3/2: original constructor
    __ CallRuntime(Runtime::kNewObject, 2);
    __ movp(rbx, rax);  // store result in rbx

    // New object allocated.
    // rbx: newly allocated object
    __ bind(&allocated);

    // Restore the parameters.
    __ Pop(rdx);
    __ Pop(rdi);

    // Retrieve smi-tagged arguments count from the stack.
    __ movp(rax, Operand(rsp, 0));
    __ SmiToInteger32(rax, rax);

    // Push new.target onto the construct frame. This is stored just below the
    // receiver on the stack.
    __ Push(rdx);

    // Push the allocated receiver to the stack. We need two copies
    // because we may have to return the original one and the calling
    // conventions dictate that the called function pops the receiver.
    __ Push(rbx);
    __ Push(rbx);

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
      __ InvokeFunction(rdi, actual, CALL_FUNCTION, NullCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;
    // If the result is a smi, it is *not* an object in the ECMA sense.
    __ JumpIfSmi(rax, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CmpObjectType(rax, FIRST_SPEC_OBJECT_TYPE, rcx);
    __ j(above_equal, &exit);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ movp(rax, Operand(rsp, 0));

    // Restore the arguments count and leave the construct frame. The arguments
    // count is stored below the reciever and the new.target.
    __ bind(&exit);
    __ movp(rbx, Operand(rsp, 2 * kPointerSize));

    // Leave construct frame.
  }

  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->constructed_objects(), 1);
  __ ret(0);
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true);
}


void Builtins::Generate_JSConstructStubForDerived(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax: number of arguments
  //  -- rdi: constructor function
  //  -- rbx: allocation site or undefined
  //  -- rdx: original constructor
  // -----------------------------------

  {
    FrameScope frame_scope(masm, StackFrame::CONSTRUCT);

    // Preserve allocation site.
    __ AssertUndefinedOrAllocationSite(rbx);
    __ Push(rbx);

    // Store a smi-tagged arguments count on the stack.
    __ Integer32ToSmi(rax, rax);
    __ Push(rax);
    __ SmiToInteger32(rax, rax);

    // Push new.target
    __ Push(rdx);

    // receiver is the hole.
    __ Push(masm->isolate()->factory()->the_hole_value());

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

    // Handle step in.
    Label skip_step_in;
    ExternalReference debug_step_in_fp =
        ExternalReference::debug_step_in_fp_address(masm->isolate());
    __ Move(kScratchRegister, debug_step_in_fp);
    __ cmpp(Operand(kScratchRegister, 0), Immediate(0));
    __ j(equal, &skip_step_in);

    __ Push(rax);
    __ Push(rdi);
    __ Push(rdi);
    __ CallRuntime(Runtime::kHandleStepInForDerivedConstructors, 1);
    __ Pop(rdi);
    __ Pop(rax);

    __ bind(&skip_step_in);

    // Call the function.
    ParameterCount actual(rax);
    __ InvokeFunction(rdi, actual, CALL_FUNCTION, NullCallWrapper());

    // Restore context from the frame.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

    // Get arguments count, skipping over new.target.
    __ movp(rbx, Operand(rsp, kPointerSize));  // Get arguments count.
  }                                            // Leave construct frame.

  // Remove caller arguments from the stack and return.
  __ PopReturnAddressTo(rcx);
  SmiIndex index = masm->SmiToIndex(rbx, rbx, kPointerSizeLog2);
  __ leap(rsp, Operand(rsp, index.reg, index.scale, 1 * kPointerSize));
  __ PushReturnAddressFrom(rcx);
  __ ret(0);
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
  __ CallRuntime(Runtime::kThrowStackOverflow, 0);

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
//   o rsi: our context
//   o rbp: the caller's frame pointer
//   o rsp: stack pointer (pointing to return address)
//
// The function builds a JS frame.  Please see JavaScriptFrameConstants in
// frames-x64.h for its layout.
// TODO(rmcilroy): We will need to include the current bytecode pointer in the
// frame.
void Builtins::Generate_InterpreterEntryTrampoline(MacroAssembler* masm) {
  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ pushq(rbp);  // Caller's frame pointer.
  __ movp(rbp, rsp);
  __ Push(rsi);  // Callee's context.
  __ Push(rdi);  // Callee's JS function.

  // Get the bytecode array from the function object and load the pointer to the
  // first entry into edi (InterpreterBytecodeRegister).
  __ movp(rax, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  __ movp(kInterpreterBytecodeArrayRegister,
          FieldOperand(rax, SharedFunctionInfo::kFunctionDataOffset));

  if (FLAG_debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ AssertNotSmi(kInterpreterBytecodeArrayRegister);
    __ CmpObjectType(kInterpreterBytecodeArrayRegister, BYTECODE_ARRAY_TYPE,
                     rax);
    __ Assert(equal, kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

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
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
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
    __ CompareRoot(rsp, Heap::kStackLimitRootIndex);
    __ j(above_equal, &ok, Label::kNear);
    __ CallRuntime(Runtime::kStackGuard, 0);
    __ bind(&ok);
  }

  // Load accumulator, register file, bytecode offset, dispatch table into
  // registers.
  __ LoadRoot(kInterpreterAccumulatorRegister, Heap::kUndefinedValueRootIndex);
  __ movp(kInterpreterRegisterFileRegister, rbp);
  __ subp(
      kInterpreterRegisterFileRegister,
      Immediate(kPointerSize + StandardFrameConstants::kFixedFrameSizeFromFp));
  __ movp(kInterpreterBytecodeOffsetRegister,
          Immediate(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterDispatchTableRegister,
              Heap::kInterpreterTableRootIndex);
  __ addp(kInterpreterDispatchTableRegister,
          Immediate(FixedArray::kHeaderSize - kHeapObjectTag));

  // Dispatch to the first bytecode handler for the function.
  __ movzxbp(rbx, Operand(kInterpreterBytecodeArrayRegister,
                          kInterpreterBytecodeOffsetRegister, times_1, 0));
  __ movp(rbx, Operand(kInterpreterDispatchTableRegister, rbx,
                       times_pointer_size, 0));
  // TODO(rmcilroy): Make dispatch table point to code entrys to avoid untagging
  // and header removal.
  __ addp(rbx, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ call(rbx);
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


void Builtins::Generate_CompileLazy(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kCompileLazy);
  GenerateTailCallToReturnedCode(masm);
}


static void CallCompileOptimized(MacroAssembler* masm,
                                            bool concurrent) {
  FrameScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  __ Push(rdi);
  // Function is also the parameter to the runtime call.
  __ Push(rdi);
  // Whether to compile in a background thread.
  __ Push(masm->isolate()->factory()->ToBoolean(concurrent));

  __ CallRuntime(Runtime::kCompileOptimized, 2);
  // Restore receiver.
  __ Pop(rdi);
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
    __ CallRuntime(Runtime::kNotifyStubFailure, 0, save_doubles);
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

    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
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
void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
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
  __ movp(key, Operand(rbp, indexOffset));
  __ jmp(&entry);
  __ bind(&loop);
  __ movp(receiver, Operand(rbp, argumentsOffset));  // load arguments

  // Use inline caching to speed up access to arguments.
  int slot_index = TypeFeedbackVector::PushAppliedArgumentsIndex();
  __ Move(slot, Smi::FromInt(slot_index));
  __ movp(vector, Operand(rbp, vectorOffset));
  Handle<Code> ic =
      KeyedLoadICStub(masm->isolate(), LoadICState(kNoExtraICState)).GetCode();
  __ Call(ic, RelocInfo::CODE_TARGET);
  // It is important that we do not have a test instruction after the
  // call.  A test instruction after the call is used to indicate that
  // we have generated an inline version of the keyed load.  In this
  // case, we know that we are not generating a test instruction next.

  // Push the nth argument.
  __ Push(rax);

  // Update the index on the stack and in register key.
  __ movp(key, Operand(rbp, indexOffset));
  __ SmiAddConstant(key, key, Smi::FromInt(1));
  __ movp(Operand(rbp, indexOffset), key);

  __ bind(&entry);
  __ cmpp(key, Operand(rbp, limitOffset));
  __ j(not_equal, &loop);

  // On exit, the pushed arguments count is in rax, untagged
  __ SmiToInteger64(rax, key);
}


// Used by FunctionApply and ReflectApply
static void Generate_ApplyHelper(MacroAssembler* masm, bool targetIsArgument) {
  const int kFormalParameters = targetIsArgument ? 3 : 2;
  const int kStackSize = kFormalParameters + 1;

  // Stack at entry:
  // rsp     : return address
  // rsp[8]  : arguments
  // rsp[16] : receiver ("this")
  // rsp[24] : function
  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    // Stack frame:
    // rbp     : Old base pointer
    // rbp[8]  : return address
    // rbp[16] : function arguments
    // rbp[24] : receiver
    // rbp[32] : function
    static const int kArgumentsOffset = kFPOnStackSize + kPCOnStackSize;
    static const int kReceiverOffset = kArgumentsOffset + kPointerSize;
    static const int kFunctionOffset = kReceiverOffset + kPointerSize;
    static const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ movp(rdi, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movp(rdi, FieldOperand(rdi, SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(rdi);

    __ Push(Operand(rbp, kFunctionOffset));
    __ Push(Operand(rbp, kArgumentsOffset));
    if (targetIsArgument) {
      __ InvokeBuiltin(Context::REFLECT_APPLY_PREPARE_BUILTIN_INDEX,
                       CALL_FUNCTION);
    } else {
      __ InvokeBuiltin(Context::APPLY_PREPARE_BUILTIN_INDEX, CALL_FUNCTION);
    }

    Generate_CheckStackOverflow(masm, kRaxIsSmiTagged);

    // Push current index and limit, and receiver.
    const int kLimitOffset = kVectorOffset - 1 * kPointerSize;
    const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
    __ Push(rax);                            // limit
    __ Push(Immediate(0));                   // index
    __ Push(Operand(rbp, kReceiverOffset));  // receiver

    // Loop over the arguments array, pushing each value to the stack
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Call the callable.
    // TODO(bmeurer): This should be a tail call according to ES6.
    __ movp(rdi, Operand(rbp, kFunctionOffset));
    __ Call(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);

    // Leave internal frame.
  }
  __ ret(kStackSize * kPointerSize);  // remove this, receiver, and arguments
}


// Used by ReflectConstruct
static void Generate_ConstructHelper(MacroAssembler* masm) {
  const int kFormalParameters = 3;
  const int kStackSize = kFormalParameters + 1;

  // Stack at entry:
  // rsp     : return address
  // rsp[8]  : original constructor (new.target)
  // rsp[16] : arguments
  // rsp[24] : constructor
  {
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    // Stack frame:
    // rbp     : Old base pointer
    // rbp[8]  : return address
    // rbp[16] : original constructor (new.target)
    // rbp[24] : arguments
    // rbp[32] : constructor
    static const int kNewTargetOffset = kFPOnStackSize + kPCOnStackSize;
    static const int kArgumentsOffset = kNewTargetOffset + kPointerSize;
    static const int kFunctionOffset = kArgumentsOffset + kPointerSize;

    static const int kVectorOffset =
        InternalFrameConstants::kCodeOffset - 1 * kPointerSize;

    // Push the vector.
    __ movp(rdi, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
    __ movp(rdi, FieldOperand(rdi, SharedFunctionInfo::kFeedbackVectorOffset));
    __ Push(rdi);

    // If newTarget is not supplied, set it to constructor
    Label validate_arguments;
    __ movp(rax, Operand(rbp, kNewTargetOffset));
    __ CompareRoot(rax, Heap::kUndefinedValueRootIndex);
    __ j(not_equal, &validate_arguments, Label::kNear);
    __ movp(rax, Operand(rbp, kFunctionOffset));
    __ movp(Operand(rbp, kNewTargetOffset), rax);

    // Validate arguments
    __ bind(&validate_arguments);
    __ Push(Operand(rbp, kFunctionOffset));
    __ Push(Operand(rbp, kArgumentsOffset));
    __ Push(Operand(rbp, kNewTargetOffset));
    __ InvokeBuiltin(Context::REFLECT_CONSTRUCT_PREPARE_BUILTIN_INDEX,
                     CALL_FUNCTION);

    Generate_CheckStackOverflow(masm, kRaxIsSmiTagged);

    // Push current index and limit.
    const int kLimitOffset = kVectorOffset - 1 * kPointerSize;
    const int kIndexOffset = kLimitOffset - 1 * kPointerSize;
    __ Push(rax);  // limit
    __ Push(Immediate(0));  // index
    // Push the constructor function as callee.
    __ Push(Operand(rbp, kFunctionOffset));

    // Loop over the arguments array, pushing each value to the stack
    Generate_PushAppliedArguments(masm, kVectorOffset, kArgumentsOffset,
                                  kIndexOffset, kLimitOffset);

    // Use undefined feedback vector
    __ LoadRoot(rbx, Heap::kUndefinedValueRootIndex);
    __ movp(rdi, Operand(rbp, kFunctionOffset));
    __ movp(rcx, Operand(rbp, kNewTargetOffset));

    // Call the function.
    CallConstructStub stub(masm->isolate(), SUPER_CONSTRUCTOR_CALL);
    __ call(stub.GetCode(), RelocInfo::CONSTRUCT_CALL);

    // Leave internal frame.
  }
  // remove this, target, arguments and newTarget
  __ ret(kStackSize * kPointerSize);
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


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : argc
  //  -- rsp[0] : return address
  //  -- rsp[8] : last argument
  // -----------------------------------
  Label generic_array_code;

  // Get the InternalArray function.
  __ LoadGlobalFunction(Context::INTERNAL_ARRAY_FUNCTION_INDEX, rdi);

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
  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, rdi);

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
    __ TailCallRuntime(Runtime::kSymbolDescriptiveString, 1, 1);
  }
}


// static
void Builtins::Generate_StringConstructor_ConstructStub(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax                 : number of arguments
  //  -- rdi                 : constructor function
  //  -- rsp[0]              : return address
  //  -- rsp[(argc - n) * 8] : arg[n] (zero-based)
  //  -- rsp[(argc + 1) * 8] : receiver
  // -----------------------------------

  // 1. Load the first argument into rbx and get rid of the rest (including the
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

  // 2. Make sure rbx is a string.
  {
    Label convert, done_convert;
    __ JumpIfSmi(rbx, &convert, Label::kNear);
    __ CmpObjectType(rbx, FIRST_NONSTRING_TYPE, rdx);
    __ j(below, &done_convert);
    __ bind(&convert);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      ToStringStub stub(masm->isolate());
      __ Push(rdi);
      __ Move(rax, rbx);
      __ CallStub(&stub);
      __ Move(rbx, rax);
      __ Pop(rdi);
    }
    __ bind(&done_convert);
  }

  // 3. Allocate a JSValue wrapper for the string.
  {
    // ----------- S t a t e -------------
    //  -- rbx : the first argument
    //  -- rdi : constructor function
    // -----------------------------------

    Label allocate, done_allocate;
    __ Allocate(JSValue::kSize, rax, rcx, no_reg, &allocate, TAG_OBJECT);
    __ bind(&done_allocate);

    // Initialize the JSValue in rax.
    __ LoadGlobalFunctionInitialMap(rdi, rcx);
    __ movp(FieldOperand(rax, HeapObject::kMapOffset), rcx);
    __ LoadRoot(rcx, Heap::kEmptyFixedArrayRootIndex);
    __ movp(FieldOperand(rax, JSObject::kPropertiesOffset), rcx);
    __ movp(FieldOperand(rax, JSObject::kElementsOffset), rcx);
    __ movp(FieldOperand(rax, JSValue::kValueOffset), rbx);
    STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
    __ Ret();

    // Fallback to the runtime to allocate in new space.
    __ bind(&allocate);
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(rbx);
      __ Push(rdi);
      __ Push(Smi::FromInt(JSValue::kSize));
      __ CallRuntime(Runtime::kAllocateInNewSpace, 1);
      __ Pop(rdi);
      __ Pop(rbx);
    }
    __ jmp(&done_allocate);
  }
}


static void ArgumentsAdaptorStackCheck(MacroAssembler* masm,
                                       Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- rax : actual number of arguments
  //  -- rbx : expected number of arguments
  //  -- rdi: function (passed through to callee)
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  Label okay;
  __ LoadRoot(rdx, Heap::kRealStackLimitRootIndex);
  __ movp(rcx, rsp);
  // Make rcx the space we have left. The stack might already be overflowed
  // here which will cause rcx to become negative.
  __ subp(rcx, rdx);
  // Make rdx the space we need for the array when it is unrolled onto the
  // stack.
  __ movp(rdx, rbx);
  __ shlp(rdx, Immediate(kPointerSizeLog2));
  // Check if the arguments will overflow the stack.
  __ cmpp(rcx, rdx);
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
  //  -- rdi: function (passed through to callee)
  // -----------------------------------

  Label invoke, dont_adapt_arguments;
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->arguments_adaptors(), 1);

  Label stack_overflow;
  ArgumentsAdaptorStackCheck(masm, &stack_overflow);

  Label enough, too_few;
  __ movp(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  __ cmpp(rax, rbx);
  __ j(less, &too_few);
  __ cmpp(rbx, Immediate(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ j(equal, &dont_adapt_arguments);

  {  // Enough parameters: Actual >= expected.
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

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
      __ CallRuntime(Runtime::kThrowStrongModeTooFewArguments, 0);
    }

    __ bind(&no_strong_error);
    EnterArgumentsAdaptorFrame(masm);

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
  // rdi: function (passed through to callee)
  __ call(rdx);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Leave frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ ret(0);

  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ jmp(rdx);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    EnterArgumentsAdaptorFrame(masm);
    __ CallRuntime(Runtime::kThrowStackOverflow, 0);
    __ int3();
  }
}


// static
void Builtins::Generate_CallFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the function to call (checked to be a JSFunction)
  // -----------------------------------

  Label convert, convert_global_proxy, convert_to_object, done_convert;
  StackArgumentsAccessor args(rsp, rax);
  __ AssertFunction(rdi);
  // TODO(bmeurer): Throw a TypeError if function's [[FunctionKind]] internal
  // slot is "classConstructor".
  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  STATIC_ASSERT(SharedFunctionInfo::kNativeByteOffset ==
                SharedFunctionInfo::kStrictModeByteOffset);
  __ movp(rsi, FieldOperand(rdi, JSFunction::kContextOffset));
  __ movp(rdx, FieldOperand(rdi, JSFunction::kSharedFunctionInfoOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  __ testb(FieldOperand(rdx, SharedFunctionInfo::kNativeByteOffset),
           Immediate((1 << SharedFunctionInfo::kNativeBitWithinByte) |
                     (1 << SharedFunctionInfo::kStrictModeBitWithinByte)));
  __ j(not_zero, &done_convert);
  {
    __ movp(rcx, args.GetReceiverOperand());

    // ----------- S t a t e -------------
    //  -- rax : the number of arguments (not including the receiver)
    //  -- rcx : the receiver
    //  -- rdx : the shared function info.
    //  -- rdi : the function to call (checked to be a JSFunction)
    //  -- rsi : the function context.
    // -----------------------------------

    Label convert_receiver;
    __ JumpIfSmi(rcx, &convert_to_object, Label::kNear);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    __ CmpObjectType(rcx, FIRST_JS_RECEIVER_TYPE, rbx);
    __ j(above_equal, &done_convert);
    __ JumpIfRoot(rcx, Heap::kUndefinedValueRootIndex, &convert_global_proxy,
                  Label::kNear);
    __ JumpIfNotRoot(rcx, Heap::kNullValueRootIndex, &convert_to_object,
                     Label::kNear);
    __ bind(&convert_global_proxy);
    {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(rcx);
    }
    __ jmp(&convert_receiver);
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
    __ movp(args.GetReceiverOperand(), rcx);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the shared function info.
  //  -- rdi : the function to call (checked to be a JSFunction)
  //  -- rsi : the function context.
  // -----------------------------------

  __ LoadSharedFunctionInfoSpecialField(
      rbx, rdx, SharedFunctionInfo::kFormalParameterCountOffset);
  __ movp(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  ParameterCount actual(rax);
  ParameterCount expected(rbx);
  __ InvokeCode(rdx, expected, actual, JUMP_FUNCTION, NullCallWrapper());
}


// static
void Builtins::Generate_Call(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdi : the target to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  Label non_callable, non_function, non_smi;
  __ JumpIfSmi(rdi, &non_callable);
  __ bind(&non_smi);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rcx);
  __ j(equal, masm->isolate()->builtins()->CallFunction(),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(rcx, JS_FUNCTION_PROXY_TYPE);
  __ j(not_equal, &non_function);

  // 1. Call to function proxy.
  // TODO(neis): This doesn't match the ES6 spec for [[Call]] on proxies.
  __ movp(rdi, FieldOperand(rdi, JSFunctionProxy::kCallTrapOffset));
  __ AssertNotSmi(rdi);
  __ jmp(&non_smi);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  __ bind(&non_function);
  // Check if target has a [[Call]] internal method.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsCallable));
  __ j(zero, &non_callable, Label::kNear);
  // Overwrite the original receiver with the (original) target.
  __ movp(args.GetReceiverOperand(), rdi);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadGlobalFunction(Context::CALL_AS_FUNCTION_DELEGATE_INDEX, rdi);
  __ Jump(masm->isolate()->builtins()->CallFunction(), RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the original constructor (checked to be a JSFunction)
  //  -- rdi : the constructor to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertFunction(rdx);
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
void Builtins::Generate_ConstructProxy(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the original constructor (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (checked to be a JSFunctionProxy)
  // -----------------------------------

  // TODO(neis): This doesn't match the ES6 spec for [[Construct]] on proxies.
  __ movp(rdi, FieldOperand(rdi, JSFunctionProxy::kConstructTrapOffset));
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rdx : the original constructor (either the same as the constructor or
  //           the JSFunction on which new was invoked initially)
  //  -- rdi : the constructor to call (can be any Object)
  // -----------------------------------
  StackArgumentsAccessor args(rsp, rax);

  // Check if target has a [[Construct]] internal method.
  Label non_constructor;
  __ JumpIfSmi(rdi, &non_constructor, Label::kNear);
  __ movp(rcx, FieldOperand(rdi, HeapObject::kMapOffset));
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsConstructor));
  __ j(zero, &non_constructor, Label::kNear);

  // Dispatch based on instance type.
  __ CmpInstanceType(rcx, JS_FUNCTION_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructFunction(),
       RelocInfo::CODE_TARGET);
  __ CmpInstanceType(rcx, JS_FUNCTION_PROXY_TYPE);
  __ j(equal, masm->isolate()->builtins()->ConstructProxy(),
       RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  {
    // Overwrite the original receiver with the (original) target.
    __ movp(args.GetReceiverOperand(), rdi);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadGlobalFunction(Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX, rdi);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(rdi);
    __ CallRuntime(Runtime::kThrowCalledNonCallable, 1);
  }
}


// static
void Builtins::Generate_PushArgsAndCall(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax : the number of arguments (not including the receiver)
  //  -- rbx : the address of the first argument to be pushed. Subsequent
  //           arguments should be consecutive above this, in the same order as
  //           they are to be pushed onto the stack.
  //  -- rdi : the target to call (can be any Object).

  // Pop return address to allow tail-call after pushing arguments.
  __ Pop(rdx);

  // Find the address of the last argument.
  __ movp(rcx, rax);
  __ addp(rcx, Immediate(1));  // Add one for receiver.
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

  // Call the target.
  __ Push(rdx);  // Re-push return address.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ movp(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ Push(rax);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
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
    __ CallRuntime(Runtime::kStackGuard, 0);
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

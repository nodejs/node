// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/api/api-arguments.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/interface-descriptors-inl.h"
// For interpreter_entry_return_pc_offset. TODO(jkummerow): Drop.
#include "src/codegen/macro-assembler-inl.h"
#include "src/codegen/register-configuration.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/frame-constants.h"
#include "src/execution/frames.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/objects/cell.h"
#include "src/objects/foreign.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-generator.h"
#include "src/objects/smi.h"
#include "src/runtime/runtime.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/baseline/liftoff-assembler-defs.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-objects.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

namespace {

static void AssertCodeIsBaseline(MacroAssembler* masm, Register code,
                                 Register scratch) {
  DCHECK(!AreAliased(code, scratch));
  // Verify that the code kind is baseline code via the CodeKind.
  __ LoadU32(scratch, FieldMemOperand(code, Code::kFlagsOffset));
  __ DecodeField<Code::KindField>(scratch);
  __ CmpS64(scratch, Operand(static_cast<int>(CodeKind::BASELINE)));
  __ Assert(eq, AbortReason::kExpectedBaselineData);
}

static void GetSharedFunctionInfoBytecodeOrBaseline(MacroAssembler* masm,
                                                    Register sfi_data,
                                                    Register scratch1,
                                                    Label* is_baseline) {
  USE(GetSharedFunctionInfoBytecodeOrBaseline);
  ASM_CODE_COMMENT(masm);
  Label done;
  __ LoadMap(scratch1, sfi_data);

#ifndef V8_JITLESS
  __ CompareInstanceType(scratch1, scratch1, CODE_TYPE);
  if (v8_flags.debug_code) {
    Label not_baseline;
    __ b(ne, &not_baseline);
    AssertCodeIsBaseline(masm, sfi_data, scratch1);
    __ beq(is_baseline);
    __ bind(&not_baseline);
  } else {
    __ beq(is_baseline);
  }
  __ CmpS32(scratch1, Operand(INTERPRETER_DATA_TYPE));
#else
  __ CompareInstanceType(scratch1, scratch1, INTERPRETER_DATA_TYPE);
#endif  // !V8_JITLESS

  __ bne(&done);
  __ LoadTaggedField(
      sfi_data,
      FieldMemOperand(sfi_data, InterpreterData::kBytecodeArrayOffset));

  __ bind(&done);
}

void Generate_OSREntry(MacroAssembler* masm, Register entry_address,
                       Operand offset) {
  if (!offset.is_reg() && is_int20(offset.immediate())) {
    __ lay(r14, MemOperand(entry_address, offset.immediate()));
  } else {
    CHECK(offset.is_reg());
    __ AddS64(r14, entry_address, offset.rm());
  }

  // "return" to the OSR entry point of the function.
  __ Ret();
}

void ResetBytecodeAge(MacroAssembler* masm, Register bytecode_array,
                      Register scratch) {
  DCHECK(!AreAliased(bytecode_array, scratch));
  __ mov(r0, Operand(0));
  __ StoreU16(
      r0, FieldMemOperand(bytecode_array, BytecodeArray::kBytecodeAgeOffset),
      scratch);
}

void ResetFeedbackVectorOsrUrgency(MacroAssembler* masm,
                                   Register feedback_vector, Register scratch) {
  DCHECK(!AreAliased(feedback_vector, scratch));
  __ LoadU8(scratch,
            FieldMemOperand(feedback_vector, FeedbackVector::kOsrStateOffset));
  __ AndP(scratch, scratch,
          Operand(FeedbackVector::MaybeHasOptimizedOsrCodeBit::kMask));
  __ StoreU8(scratch,
             FieldMemOperand(feedback_vector, FeedbackVector::kOsrStateOffset));
}

// Restarts execution either at the current or next (in execution order)
// bytecode. If there is baseline code on the shared function info, converts an
// interpreter frame into a baseline frame and continues execution in baseline
// code. Otherwise execution continues with bytecode.
void Generate_BaselineOrInterpreterEntry(MacroAssembler* masm,
                                         bool next_bytecode,
                                         bool is_osr = false) {
  Label start;
  __ bind(&start);

  // Get function from the frame.
  Register closure = r3;
  __ LoadU64(closure, MemOperand(fp, StandardFrameConstants::kFunctionOffset));

  // Get the InstructionStream object from the shared function info.
  Register code_obj = r8;
  __ LoadTaggedField(
      code_obj,
      FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(
      code_obj,
      FieldMemOperand(code_obj, SharedFunctionInfo::kFunctionDataOffset));

  // Check if we have baseline code. For OSR entry it is safe to assume we
  // always have baseline code.
  if (!is_osr) {
    Label start_with_baseline;
    __ CompareObjectType(code_obj, r5, r5, CODE_TYPE);
    __ b(eq, &start_with_baseline);

    // Start with bytecode as there is no baseline code.
    Builtin builtin_id = next_bytecode
                             ? Builtin::kInterpreterEnterAtNextBytecode
                             : Builtin::kInterpreterEnterAtBytecode;
    __ Jump(masm->isolate()->builtins()->code_handle(builtin_id),
            RelocInfo::CODE_TARGET);

    // Start with baseline code.
    __ bind(&start_with_baseline);
  } else if (v8_flags.debug_code) {
    __ CompareObjectType(code_obj, r5, r5, CODE_TYPE);
    __ Assert(eq, AbortReason::kExpectedBaselineData);
  }

  if (v8_flags.debug_code) {
    AssertCodeIsBaseline(masm, code_obj, r5);
  }

  // Load the feedback vector.
  Register feedback_vector = r4;
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(feedback_vector, Cell::kValueOffset));

  Label install_baseline_code;
  // Check if feedback vector is valid. If not, call prepare for baseline to
  // allocate it.
  __ CompareObjectType(feedback_vector, r5, r5, FEEDBACK_VECTOR_TYPE);
  __ b(ne, &install_baseline_code);

  // Save BytecodeOffset from the stack frame.
  __ LoadU64(kInterpreterBytecodeOffsetRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);
  // Replace BytecodeOffset with the feedback vector.
  __ StoreU64(feedback_vector,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  feedback_vector = no_reg;

  // Compute baseline pc for bytecode offset.
  ExternalReference get_baseline_pc_extref;
  if (next_bytecode || is_osr) {
    get_baseline_pc_extref =
        ExternalReference::baseline_pc_for_next_executed_bytecode();
  } else {
    get_baseline_pc_extref =
        ExternalReference::baseline_pc_for_bytecode_offset();
  }
  Register get_baseline_pc = r5;
  __ Move(get_baseline_pc, get_baseline_pc_extref);

  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset.
  // TODO(pthier): Investigate if it is feasible to handle this special case
  // in TurboFan instead of here.
  Label valid_bytecode_offset, function_entry_bytecode;
  if (!is_osr) {
    __ CmpS64(kInterpreterBytecodeOffsetRegister,
              Operand(BytecodeArray::kHeaderSize - kHeapObjectTag +
                      kFunctionEntryBytecodeOffset));
    __ b(eq, &function_entry_bytecode);
  }

  __ SubS64(kInterpreterBytecodeOffsetRegister,
            kInterpreterBytecodeOffsetRegister,
            Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  __ bind(&valid_bytecode_offset);
  // Get bytecode array from the stack frame.
  __ LoadU64(kInterpreterBytecodeArrayRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  // Save the accumulator register, since it's clobbered by the below call.
  __ Push(kInterpreterAccumulatorRegister);
  {
    Register arg_reg_1 = r2;
    Register arg_reg_2 = r3;
    Register arg_reg_3 = r4;
    __ mov(arg_reg_1, code_obj);
    __ mov(arg_reg_2, kInterpreterBytecodeOffsetRegister);
    __ mov(arg_reg_3, kInterpreterBytecodeArrayRegister);
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ PrepareCallCFunction(3, 0, r1);
    __ CallCFunction(get_baseline_pc, 3, 0);
  }
  __ LoadCodeEntry(code_obj, code_obj);
  __ AddS64(code_obj, code_obj, kReturnRegister0);
  __ Pop(kInterpreterAccumulatorRegister);

  if (is_osr) {
    // TODO(pthier): Separate baseline Sparkplug from TF arming and don't
    // disarm Sparkplug here.
    ResetBytecodeAge(masm, kInterpreterBytecodeArrayRegister, r1);
    Generate_OSREntry(masm, code_obj, Operand(0));
  } else {
    __ Jump(code_obj);
  }
  __ Trap();  // Unreachable.

  if (!is_osr) {
    __ bind(&function_entry_bytecode);
    // If the bytecode offset is kFunctionEntryOffset, get the start address of
    // the first bytecode.
    __ mov(kInterpreterBytecodeOffsetRegister, Operand(0));
    if (next_bytecode) {
      __ Move(get_baseline_pc,
              ExternalReference::baseline_pc_for_bytecode_offset());
    }
    __ b(&valid_bytecode_offset);
  }

  __ bind(&install_baseline_code);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ Push(kInterpreterAccumulatorRegister);
    __ Push(closure);
    __ CallRuntime(Runtime::kInstallBaselineCode, 1);
    __ Pop(kInterpreterAccumulatorRegister);
  }
  // Retry from the start after installing baseline code.
  __ b(&start);
}

enum class OsrSourceTier {
  kInterpreter,
  kBaseline,
};

void OnStackReplacement(MacroAssembler* masm, OsrSourceTier source,
                        Register maybe_target_code) {
  Label jump_to_optimized_code;
  {
    // If maybe_target_code is not null, no need to call into runtime. A
    // precondition here is: if maybe_target_code is a InstructionStream object,
    // it must NOT be marked_for_deoptimization (callers must ensure this).
    __ CmpSmiLiteral(maybe_target_code, Smi::zero(), r0);
    __ bne(&jump_to_optimized_code);
  }

  ASM_CODE_COMMENT(masm);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kCompileOptimizedOSR);
  }

  // If the code object is null, just return to the caller.
  __ CmpSmiLiteral(r2, Smi::zero(), r0);
  __ bne(&jump_to_optimized_code);
  __ Ret();

  __ bind(&jump_to_optimized_code);
  DCHECK_EQ(maybe_target_code, r2);  // Already in the right spot.

  // OSR entry tracing.
  {
    Label next;
    __ Move(r3, ExternalReference::address_of_log_or_trace_osr());
    __ LoadU8(r3, MemOperand(r3));
    __ tmll(r3, Operand(0xFF));  // Mask to the LSB.
    __ beq(&next);

    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r2);  // Preserve the code object.
      __ CallRuntime(Runtime::kLogOrTraceOptimizedOSREntry, 0);
      __ Pop(r2);
    }

    __ bind(&next);
  }

  if (source == OsrSourceTier::kInterpreter) {
    // Drop the handler frame that is be sitting on top of the actual
    // JavaScript frame. This is the case then OSR is triggered from bytecode.
    __ LeaveFrame(StackFrame::STUB);
  }

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadTaggedField(
      r3,
      FieldMemOperand(r2, Code::kDeoptimizationDataOrInterpreterDataOffset));

  // Load the OSR entrypoint offset from the deoptimization data.
  // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
  __ SmiUntagField(
      r3, FieldMemOperand(r3, FixedArray::OffsetOfElementAt(
                                  DeoptimizationData::kOsrPcOffsetIndex)));

  __ LoadCodeEntry(r2, r2);

  // Compute the target address = code_entry + osr_offset
  // <entry_addr> = <code_entry> + <osr_offset>
  Generate_OSREntry(masm, r2, Operand(r3));
}

}  // namespace

void Builtins::Generate_Adaptor(MacroAssembler* masm, Address address) {
  __ Move(kJavaScriptCallExtraArg1Register, ExternalReference::Create(address));
  __ Jump(BUILTIN_CODE(masm->isolate(), AdaptorWithBuiltinExitFrame),
          RelocInfo::CODE_TARGET);
}

namespace {

enum class ArgumentsElementType {
  kRaw,    // Push arguments as they are.
  kHandle  // Dereference arguments before pushing.
};

void Generate_PushArguments(MacroAssembler* masm, Register array, Register argc,
                            Register scratch,
                            ArgumentsElementType element_type) {
  DCHECK(!AreAliased(array, argc, scratch));
  Register counter = scratch;
  Register value = ip;
  Label loop, entry;
  __ SubS64(counter, argc, Operand(kJSArgcReceiverSlots));
  __ b(&entry);
  __ bind(&loop);
  __ ShiftLeftU64(value, counter, Operand(kSystemPointerSizeLog2));
  __ LoadU64(value, MemOperand(array, value));
  if (element_type == ArgumentsElementType::kHandle) {
    __ LoadU64(value, MemOperand(value));
  }
  __ push(value);
  __ bind(&entry);
  __ SubS64(counter, counter, Operand(1));
  __ bge(&loop);
}

void Generate_JSBuiltinsConstructStubHelper(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- r3     : constructor function
  //  -- r5     : new target
  //  -- cp     : context
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  Register scratch = r4;
  Label stack_overflow;

  __ StackOverflowCheck(r2, scratch, &stack_overflow);

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    // Preserve the incoming parameters on the stack.
    __ SmiTag(r2);
    __ Push(cp, r2);
    __ SmiUntag(r2);

    // TODO(victorgomes): When the arguments adaptor is completely removed, we
    // should get the formal parameter count and copy the arguments in its
    // correct position (including any undefined), instead of delaying this to
    // InvokeFunction.

    // Set up pointer to first argument (skip receiver).
    __ la(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                                 kSystemPointerSize));
    // Copy arguments and receiver to the expression stack.
    // r6: Pointer to start of arguments.
    // r2: Number of arguments.
    Generate_PushArguments(masm, r6, r2, r1, ArgumentsElementType::kRaw);

    // The receiver for the builtin/api call.
    __ PushRoot(RootIndex::kTheHoleValue);

    // Call the function.
    // r2: number of arguments
    // r3: constructor function
    // r5: new target

    __ InvokeFunctionWithNewTarget(r3, r5, r2, InvokeType::kCall);

    // Restore context from the frame.
    __ LoadU64(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
    // Restore smi-tagged arguments count from the frame.
    __ LoadU64(scratch, MemOperand(fp, ConstructFrameConstants::kLengthOffset));

    // Leave construct frame.
  }
  // Remove caller arguments from the stack and return.
  __ DropArguments(scratch, MacroAssembler::kCountIsSmi,
                   MacroAssembler::kCountIncludesReceiver);
  __ Ret();

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);  // Unreachable code.
  }
}

}  // namespace

// The construct stub for ES5 constructor functions and ES6 class constructors.
void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  --      r2: number of arguments (untagged)
  //  --      r3: constructor function
  //  --      r5: new target
  //  --      cp: context
  //  --      lr: return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  FrameScope scope(masm, StackFrame::MANUAL);
  // Enter a construct frame.
  Label post_instantiation_deopt_entry, not_create_implicit_receiver;
  __ EnterFrame(StackFrame::CONSTRUCT);

  // Preserve the incoming parameters on the stack.
  __ SmiTag(r2);
  __ Push(cp, r2, r3);
  __ PushRoot(RootIndex::kUndefinedValue);
  __ Push(r5);

  // ----------- S t a t e -------------
  //  --        sp[0*kSystemPointerSize]: new target
  //  --        sp[1*kSystemPointerSize]: padding
  //  -- r3 and sp[2*kSystemPointerSize]: constructor function
  //  --        sp[3*kSystemPointerSize]: number of arguments (tagged)
  //  --        sp[4*kSystemPointerSize]: context
  // -----------------------------------

  __ LoadTaggedField(
      r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadU32(r6, FieldMemOperand(r6, SharedFunctionInfo::kFlagsOffset));
  __ DecodeField<SharedFunctionInfo::FunctionKindBits>(r6);
  __ JumpIfIsInRange(
      r6, static_cast<uint8_t>(FunctionKind::kDefaultDerivedConstructor),
      static_cast<uint8_t>(FunctionKind::kDerivedConstructor),
      &not_create_implicit_receiver);

  // If not derived class constructor: Allocate the new receiver object.
  __ Call(BUILTIN_CODE(masm->isolate(), FastNewObject), RelocInfo::CODE_TARGET);
  __ b(&post_instantiation_deopt_entry);

  // Else: use TheHoleValue as receiver for constructor call
  __ bind(&not_create_implicit_receiver);
  __ LoadRoot(r2, RootIndex::kTheHoleValue);

  // ----------- S t a t e -------------
  //  --                          r2: receiver
  //  -- Slot 4 / sp[0*kSystemPointerSize]: new target
  //  -- Slot 3 / sp[1*kSystemPointerSize]: padding
  //  -- Slot 2 / sp[2*kSystemPointerSize]: constructor function
  //  -- Slot 1 / sp[3*kSystemPointerSize]: number of arguments (tagged)
  //  -- Slot 0 / sp[4*kSystemPointerSize]: context
  // -----------------------------------
  // Deoptimizer enters here.
  masm->isolate()->heap()->SetConstructStubCreateDeoptPCOffset(
      masm->pc_offset());
  __ bind(&post_instantiation_deopt_entry);

  // Restore new target.
  __ Pop(r5);

  // Push the allocated receiver to the stack.
  __ Push(r2);
  // We need two copies because we may have to return the original one
  // and the calling conventions dictate that the called function pops the
  // receiver. The second copy is pushed after the arguments, we saved in r6
  // since r0 needs to store the number of arguments before
  // InvokingFunction.
  __ mov(r8, r2);

  // Set up pointer to first argument (skip receiver).
  __ la(r6, MemOperand(fp, StandardFrameConstants::kCallerSPOffset +
                               kSystemPointerSize));

  // ----------- S t a t e -------------
  //  --                 r5: new target
  //  -- sp[0*kSystemPointerSize]: implicit receiver
  //  -- sp[1*kSystemPointerSize]: implicit receiver
  //  -- sp[2*kSystemPointerSize]: padding
  //  -- sp[3*kSystemPointerSize]: constructor function
  //  -- sp[4*kSystemPointerSize]: number of arguments (tagged)
  //  -- sp[5*kSystemPointerSize]: context
  // -----------------------------------

  // Restore constructor function and argument count.
  __ LoadU64(r3, MemOperand(fp, ConstructFrameConstants::kConstructorOffset));
  __ LoadU64(r2, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  __ SmiUntag(r2);

  Label stack_overflow;
  __ StackOverflowCheck(r2, r7, &stack_overflow);

  // Copy arguments and receiver to the expression stack.
  // r6: Pointer to start of argument.
  // r2: Number of arguments.
  Generate_PushArguments(masm, r6, r2, r1, ArgumentsElementType::kRaw);

  // Push implicit receiver.
  __ Push(r8);

  // Call the function.
  __ InvokeFunctionWithNewTarget(r3, r5, r2, InvokeType::kCall);

  // ----------- S t a t e -------------
  //  --                 r0: constructor result
  //  -- sp[0*kSystemPointerSize]: implicit receiver
  //  -- sp[1*kSystemPointerSize]: padding
  //  -- sp[2*kSystemPointerSize]: constructor function
  //  -- sp[3*kSystemPointerSize]: number of arguments
  //  -- sp[4*kSystemPointerSize]: context
  // -----------------------------------

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetConstructStubInvokeDeoptPCOffset(
      masm->pc_offset());

  // If the result is an object (in the ECMA sense), we should get rid
  // of the receiver and use the result; see ECMA-262 section 13.2.2-7
  // on page 74.
  Label use_receiver, do_throw, leave_and_return, check_receiver;

  // If the result is undefined, we jump out to using the implicit receiver.
  __ JumpIfNotRoot(r2, RootIndex::kUndefinedValue, &check_receiver);

  // Otherwise we do a smi check and fall through to check if the return value
  // is a valid receiver.

  // Throw away the result of the constructor invocation and use the
  // on-stack receiver as the result.
  __ bind(&use_receiver);
  __ LoadU64(r2, MemOperand(sp));
  __ JumpIfRoot(r2, RootIndex::kTheHoleValue, &do_throw);

  __ bind(&leave_and_return);
  // Restore smi-tagged arguments count from the frame.
  __ LoadU64(r3, MemOperand(fp, ConstructFrameConstants::kLengthOffset));
  // Leave construct frame.
  __ LeaveFrame(StackFrame::CONSTRUCT);

  // Remove caller arguments from the stack and return.
  __ DropArguments(r3, MacroAssembler::kCountIsSmi,
                   MacroAssembler::kCountIncludesReceiver);
  __ Ret();

  __ bind(&check_receiver);
  // If the result is a smi, it is *not* an object in the ECMA sense.
  __ JumpIfSmi(r2, &use_receiver);

  // If the type of the result (stored in its map) is less than
  // FIRST_JS_RECEIVER_TYPE, it is not an object in the ECMA sense.
  static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  __ CompareObjectType(r2, r6, r6, FIRST_JS_RECEIVER_TYPE);
  __ bge(&leave_and_return);
  __ b(&use_receiver);

  __ bind(&do_throw);
  // Restore the context from the frame.
  __ LoadU64(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowConstructorReturnedNonObject);
  __ bkpt(0);

  __ bind(&stack_overflow);
  // Restore the context from the frame.
  __ LoadU64(cp, MemOperand(fp, ConstructFrameConstants::kContextOffset));
  __ CallRuntime(Runtime::kThrowStackOverflow);
  // Unreachable code.
  __ bkpt(0);
}

void Builtins::Generate_JSBuiltinsConstructStub(MacroAssembler* masm) {
  Generate_JSBuiltinsConstructStubHelper(masm);
}

// static
void Builtins::Generate_ResumeGeneratorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the value to pass to the generator
  //  -- r3 : the JSGeneratorObject to resume
  //  -- lr : return address
  // -----------------------------------
  // Store input value into generator object.
  __ StoreTaggedField(
      r2, FieldMemOperand(r3, JSGeneratorObject::kInputOrDebugPosOffset), r0);
  __ RecordWriteField(r3, JSGeneratorObject::kInputOrDebugPosOffset, r2, r5,
                      kLRHasNotBeenSaved, SaveFPRegsMode::kIgnore);
  // Check that r3 is still valid, RecordWrite might have clobbered it.
  __ AssertGeneratorObject(r3);

  // Load suspended function and context.
  __ LoadTaggedField(r6,
                     FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  __ LoadTaggedField(cp, FieldMemOperand(r6, JSFunction::kContextOffset));

  // Flood function if we are stepping.
  Label prepare_step_in_if_stepping, prepare_step_in_suspended_generator;
  Label stepping_prepared;
  Register scratch = r7;

  ExternalReference debug_hook =
      ExternalReference::debug_hook_on_function_call_address(masm->isolate());
  __ Move(scratch, debug_hook);
  __ LoadS8(scratch, MemOperand(scratch));
  __ CmpSmiLiteral(scratch, Smi::zero(), r0);
  __ bne(&prepare_step_in_if_stepping);

  // Flood function if we need to continue stepping in the suspended generator.

  ExternalReference debug_suspended_generator =
      ExternalReference::debug_suspended_generator_address(masm->isolate());

  __ Move(scratch, debug_suspended_generator);
  __ LoadU64(scratch, MemOperand(scratch));
  __ CmpS64(scratch, r3);
  __ beq(&prepare_step_in_suspended_generator);
  __ bind(&stepping_prepared);

  // Check the stack for overflow. We are not trying to catch interruptions
  // (i.e. debug break and preemption) here, so check the "real stack limit".
  Label stack_overflow;
  __ LoadU64(scratch,
             __ StackLimitAsMemOperand(StackLimitKind::kRealStackLimit));
  __ CmpU64(sp, scratch);
  __ blt(&stack_overflow);

  // ----------- S t a t e -------------
  //  -- r3    : the JSGeneratorObject to resume
  //  -- r6    : generator function
  //  -- cp    : generator context
  //  -- lr    : return address
  // -----------------------------------

  // Copy the function arguments from the generator object's register file.
  __ LoadTaggedField(
      r5, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
  __ LoadU16(
      r5, FieldMemOperand(r5, SharedFunctionInfo::kFormalParameterCountOffset));
  __ SubS64(r5, r5, Operand(kJSArgcReceiverSlots));
  __ LoadTaggedField(
      r4,
      FieldMemOperand(r3, JSGeneratorObject::kParametersAndRegistersOffset));
  {
    Label done_loop, loop;
    __ bind(&loop);
    __ SubS64(r5, r5, Operand(1));
    __ blt(&done_loop);
    __ ShiftLeftU64(r1, r5, Operand(kTaggedSizeLog2));
    __ la(scratch, MemOperand(r4, r1));
    __ LoadTaggedField(scratch,
                       FieldMemOperand(scratch, FixedArray::kHeaderSize));
    __ Push(scratch);
    __ b(&loop);
    __ bind(&done_loop);

    // Push receiver.
    __ LoadTaggedField(scratch,
                       FieldMemOperand(r3, JSGeneratorObject::kReceiverOffset));
    __ Push(scratch);
  }

  // Underlying function needs to have bytecode available.
  if (v8_flags.debug_code) {
    Label is_baseline;
    __ LoadTaggedField(
        r5, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
    __ LoadTaggedField(
        r5, FieldMemOperand(r5, SharedFunctionInfo::kFunctionDataOffset));
    GetSharedFunctionInfoBytecodeOrBaseline(masm, r5, ip, &is_baseline);
    __ CompareObjectType(r5, r5, r5, BYTECODE_ARRAY_TYPE);
    __ Assert(eq, AbortReason::kMissingBytecodeArray);
    __ bind(&is_baseline);
  }

  // Resume (Ignition/TurboFan) generator object.
  {
    __ LoadTaggedField(
        r2, FieldMemOperand(r6, JSFunction::kSharedFunctionInfoOffset));
    __ LoadS16(
        r2,
        FieldMemOperand(r2, SharedFunctionInfo::kFormalParameterCountOffset));
    // We abuse new.target both to indicate that this is a resume call and to
    // pass in the generator object.  In ordinary calls, new.target is always
    // undefined because generator functions are non-constructable.
    __ mov(r5, r3);
    __ mov(r3, r6);
    static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
    __ LoadTaggedField(r4, FieldMemOperand(r3, JSFunction::kCodeOffset));
    __ JumpCodeObject(r4);
  }

  __ bind(&prepare_step_in_if_stepping);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r6);
    // Push hole as receiver since we do not use it for stepping.
    __ PushRoot(RootIndex::kTheHoleValue);
    __ CallRuntime(Runtime::kDebugOnFunctionCall);
    __ Pop(r3);
    __ LoadTaggedField(r6,
                       FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&prepare_step_in_suspended_generator);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3);
    __ CallRuntime(Runtime::kDebugPrepareStepInSuspendedGenerator);
    __ Pop(r3);
    __ LoadTaggedField(r6,
                       FieldMemOperand(r3, JSGeneratorObject::kFunctionOffset));
  }
  __ b(&stepping_prepared);

  __ bind(&stack_overflow);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    __ bkpt(0);  // This should be unreachable.
  }
}

void Builtins::Generate_ConstructedNonConstructable(MacroAssembler* masm) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  __ push(r3);
  __ CallRuntime(Runtime::kThrowConstructedNonConstructable);
  __ Trap();  // Unreachable.
}

namespace {

constexpr int kPushedStackSpace =
    (kNumCalleeSaved + 2) * kSystemPointerSize +
    kNumCalleeSavedDoubles * kDoubleSize + 5 * kSystemPointerSize +
    EntryFrameConstants::kNextExitFrameFPOffset - kSystemPointerSize;

// Called with the native C calling convention. The corresponding function
// signature is either:
//
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, Address new_target, Address target,
//       Address receiver, intptr_t argc, Address** args)>;
// or
//   using JSEntryFunction = GeneratedCode<Address(
//       Address root_register_value, MicrotaskQueue* microtask_queue)>;
void Generate_JSEntryVariant(MacroAssembler* masm, StackFrame::Type type,
                             Builtin entry_trampoline) {
  // The register state is either:
  //   r2:                             root register value
  //   r3:                             code entry
  //   r4:                             function
  //   r5:                             receiver
  //   r6:                             argc
  //   [sp + 20 * kSystemPointerSize]: argv
  // or
  //   r2: root_register_value
  //   r3: microtask_queue

  Label invoke, handler_entry, exit;

  int pushed_stack_space = 0;
  {
    NoRootArrayScope no_root_array(masm);

    // saving floating point registers
    // 64bit ABI requires f8 to f15 be saved
    // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_zSeries.html
    __ lay(sp, MemOperand(sp, -8 * kDoubleSize));
    __ std(d8, MemOperand(sp));
    __ std(d9, MemOperand(sp, 1 * kDoubleSize));
    __ std(d10, MemOperand(sp, 2 * kDoubleSize));
    __ std(d11, MemOperand(sp, 3 * kDoubleSize));
    __ std(d12, MemOperand(sp, 4 * kDoubleSize));
    __ std(d13, MemOperand(sp, 5 * kDoubleSize));
    __ std(d14, MemOperand(sp, 6 * kDoubleSize));
    __ std(d15, MemOperand(sp, 7 * kDoubleSize));
    pushed_stack_space += kNumCalleeSavedDoubles * kDoubleSize;

    // zLinux ABI
    //    Incoming parameters:
    //          r2: root register value
    //          r3: code entry
    //          r4: function
    //          r5: receiver
    //          r6: argc
    // [sp + 20 * kSystemPointerSize]: argv
    //    Requires us to save the callee-preserved registers r6-r13
    //    General convention is to also save r14 (return addr) and
    //    sp/r15 as well in a single STM/STMG
    __ lay(sp, MemOperand(sp, -10 * kSystemPointerSize));
    __ StoreMultipleP(r6, sp, MemOperand(sp, 0));
    pushed_stack_space += (kNumCalleeSaved + 2) * kSystemPointerSize;

    // Initialize the root register.
    // C calling convention. The first argument is passed in r2.
    __ mov(kRootRegister, r2);
  }

  // save r6 to r1
  __ mov(r0, r6);

  // Push a frame with special values setup to mark it as an entry frame.
  //   Bad FP (-1)
  //   SMI Marker
  //   SMI Marker
  //   kCEntryFPAddress
  //   Frame type
  __ lay(sp, MemOperand(sp, -5 * kSystemPointerSize));
  pushed_stack_space += 5 * kSystemPointerSize;

  // Push a bad frame pointer to fail if it is used.
  __ mov(r9, Operand(-1));

  __ mov(r8, Operand(StackFrame::TypeToMarker(type)));
  __ mov(r7, Operand(StackFrame::TypeToMarker(type)));
  // Save copies of the top frame descriptor on the stack.
  __ Move(r1, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                        masm->isolate()));
  __ LoadU64(r6, MemOperand(r1));
  __ StoreMultipleP(r6, r9, MemOperand(sp, kSystemPointerSize));

  // Clear c_entry_fp, now we've pushed its previous value to the stack.
  // If the c_entry_fp is not already zero and we don't clear it, the
  // StackFrameIteratorForProfiler will assume we are executing C++ and miss the
  // JS frames on top.
  __ mov(r6, Operand::Zero());
  __ StoreU64(r6, MemOperand(r1));

  Register scrach = r8;

  // Set up frame pointer for the frame to be pushed.
  // Need to add kSystemPointerSize, because sp has one extra
  // frame already for the frame type being pushed later.
  __ lay(fp, MemOperand(sp, -EntryFrameConstants::kNextExitFrameFPOffset +
                                kSystemPointerSize));
  pushed_stack_space +=
      EntryFrameConstants::kNextExitFrameFPOffset - kSystemPointerSize;

  // restore r6
  __ mov(r6, r0);

  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp =
      ExternalReference::Create(IsolateAddressId::kJSEntrySPAddress,
                                masm->isolate());
  __ Move(r7, js_entry_sp);
  __ LoadAndTestP(scrach, MemOperand(r7));
  __ bne(&non_outermost_js, Label::kNear);
  __ StoreU64(fp, MemOperand(r7));
  __ mov(scrach, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont, Label::kNear);
  __ bind(&non_outermost_js);
  __ mov(scrach, Operand(StackFrame::INNER_JSENTRY_FRAME));

  __ bind(&cont);
  __ StoreU64(scrach, MemOperand(sp));  // frame-type

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ b(&invoke, Label::kNear);

  __ bind(&handler_entry);

  // Store the current pc as the handler offset. It's used later to create the
  // handler table.
  masm->isolate()->builtins()->SetJSEntryHandlerOffset(handler_entry.pos());

  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushStackHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ Move(scrach,
          ExternalReference::Create(IsolateAddressId::kPendingExceptionAddress,
                                    masm->isolate()));

  __ StoreU64(r2, MemOperand(scrach));
  __ LoadRoot(r2, RootIndex::kException);
  __ b(&exit, Label::kNear);

  // Invoke: Link this frame into the handler chain.
  __ bind(&invoke);
  // Must preserve r2-r6.
  __ PushStackHandler();
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the b(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Invoke the function by calling through JS entry trampoline builtin and
  // pop the faked function when we return.
  Handle<Code> trampoline_code =
      masm->isolate()->builtins()->code_handle(entry_trampoline);
  USE(pushed_stack_space);
  DCHECK_EQ(kPushedStackSpace, pushed_stack_space);
  __ Call(trampoline_code, RelocInfo::CODE_TARGET);

  // Unlink this frame from the handler chain.
  __ PopStackHandler();
  __ bind(&exit);  // r2 holds result

  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r7);
  __ CmpS64(r7, Operand(StackFrame::OUTERMOST_JSENTRY_FRAME));
  __ bne(&non_outermost_js_2, Label::kNear);
  __ mov(scrach, Operand::Zero());
  __ Move(r7, js_entry_sp);
  __ StoreU64(scrach, MemOperand(r7));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r5);
  __ Move(scrach, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                            masm->isolate()));
  __ StoreU64(r5, MemOperand(scrach));

  // Reset the stack to the callee saved registers.
  __ lay(sp, MemOperand(sp, -EntryFrameConstants::kNextExitFrameFPOffset));

  // Reload callee-saved preserved regs, return address reg (r14) and sp
  __ LoadMultipleP(r6, sp, MemOperand(sp, 0));
  __ la(sp, MemOperand(sp, 10 * kSystemPointerSize));

// saving floating point registers
#if V8_TARGET_ARCH_S390X
  // 64bit ABI requires f8 to f15 be saved
  __ ld(d8, MemOperand(sp));
  __ ld(d9, MemOperand(sp, 1 * kDoubleSize));
  __ ld(d10, MemOperand(sp, 2 * kDoubleSize));
  __ ld(d11, MemOperand(sp, 3 * kDoubleSize));
  __ ld(d12, MemOperand(sp, 4 * kDoubleSize));
  __ ld(d13, MemOperand(sp, 5 * kDoubleSize));
  __ ld(d14, MemOperand(sp, 6 * kDoubleSize));
  __ ld(d15, MemOperand(sp, 7 * kDoubleSize));
  __ la(sp, MemOperand(sp, 8 * kDoubleSize));
#else
  // 31bit ABI requires you to store f4 and f6:
  // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#AEN417
  __ ld(d4, MemOperand(sp));
  __ ld(d6, MemOperand(sp, kDoubleSize));
  __ la(sp, MemOperand(sp, 2 * kDoubleSize));
#endif

  __ b(r14);
}

}  // namespace

void Builtins::Generate_JSEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY, Builtin::kJSEntryTrampoline);
}

void Builtins::Generate_JSConstructEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::CONSTRUCT_ENTRY,
                          Builtin::kJSConstructEntryTrampoline);
}

void Builtins::Generate_JSRunMicrotasksEntry(MacroAssembler* masm) {
  Generate_JSEntryVariant(masm, StackFrame::ENTRY,
                          Builtin::kRunMicrotasksTrampoline);
}

static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r3: new.target
  // r4: function
  // r5: receiver
  // r6: argc
  // [fp + kPushedStackSpace + 20 * kSystemPointerSize]: argv
  // r0,r2,r7-r9, cp may be clobbered

  __ mov(r2, r6);
  // Load argv from the stack.
  __ LoadU64(
      r6, MemOperand(fp, kPushedStackSpace + EntryFrameConstants::kArgvOffset));

  // r2: argc
  // r3: new.target
  // r4: function
  // r5: receiver
  // r6: argv

  // Enter an internal frame.
  {
    // FrameScope ends up calling MacroAssembler::EnterFrame here
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Setup the context (we need to use the caller context from the isolate).
    ExternalReference context_address = ExternalReference::Create(
        IsolateAddressId::kContextAddress, masm->isolate());
    __ Move(cp, context_address);
    __ LoadU64(cp, MemOperand(cp));

    // Push the function
    __ Push(r4);

    // Check if we have enough stack space to push all arguments.
    Label enough_stack_space, stack_overflow;
    __ mov(r7, r2);
    __ StackOverflowCheck(r7, r1, &stack_overflow);
    __ b(&enough_stack_space);
    __ bind(&stack_overflow);
    __ CallRuntime(Runtime::kThrowStackOverflow);
    // Unreachable code.
    __ bkpt(0);

    __ bind(&enough_stack_space);

    // Copy arguments to the stack from argv to sp.
    // The arguments are actually placed in reverse order on sp
    // compared to argv (i.e. arg1 is highest memory in sp).
    // r2: argc
    // r3: function
    // r5: new.target
    // r6: argv, i.e. points to first arg
    // r7: scratch reg to hold scaled argc
    // r8: scratch reg to hold arg handle
    // r9: scratch reg to hold index into argv
    Generate_PushArguments(masm, r6, r2, r1, ArgumentsElementType::kHandle);

    // Push the receiver.
    __ Push(r5);

    // Setup new.target, argc and function.
    __ mov(r5, r3);
    __ mov(r3, r4);
    // r2: argc
    // r3: function
    // r5: new.target

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r4, RootIndex::kUndefinedValue);
    __ mov(r6, r4);
    __ mov(r7, r6);
    __ mov(r8, r6);
    __ mov(r9, r6);

    // Invoke the code.
    Handle<Code> builtin = is_construct
                               ? BUILTIN_CODE(masm->isolate(), Construct)
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

void Builtins::Generate_RunMicrotasksTrampoline(MacroAssembler* masm) {
  // This expects two C++ function parameters passed by Invoke() in
  // execution.cc.
  //   r2: root_register_value
  //   r3: microtask_queue

  __ mov(RunMicrotasksDescriptor::MicrotaskQueueRegister(), r3);
  __ Jump(BUILTIN_CODE(masm->isolate(), RunMicrotasks), RelocInfo::CODE_TARGET);
}

static void LeaveInterpreterFrame(MacroAssembler* masm, Register scratch1,
                                  Register scratch2) {
  Register params_size = scratch1;
  // Get the size of the formal parameters + receiver (in bytes).
  __ LoadU64(params_size,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadU32(params_size,
             FieldMemOperand(params_size, BytecodeArray::kParameterSizeOffset));

  Register actual_params_size = scratch2;
  // Compute the size of the actual parameters + receiver (in bytes).
  __ LoadU64(actual_params_size,
             MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ ShiftLeftU64(actual_params_size, actual_params_size,
                  Operand(kSystemPointerSizeLog2));

  // If actual is bigger than formal, then we should use it to free up the stack
  // arguments.
  Label corrected_args_count;
  __ CmpS64(params_size, actual_params_size);
  __ bge(&corrected_args_count);
  __ mov(params_size, actual_params_size);
  __ bind(&corrected_args_count);

  // Leave the frame (also dropping the register file).
  __ LeaveFrame(StackFrame::INTERPRETED);

  __ DropArguments(params_size, MacroAssembler::kCountIsBytes,
                   MacroAssembler::kCountIncludesReceiver);
}

// Advance the current bytecode offset. This simulates what all bytecode
// handlers do upon completion of the underlying operation. Will bail out to a
// label if the bytecode (without prefix) is a return bytecode. Will not advance
// the bytecode offset if the current bytecode is a JumpLoop, instead just
// re-executing the JumpLoop to jump to the correct bytecode.
static void AdvanceBytecodeOffsetOrReturn(MacroAssembler* masm,
                                          Register bytecode_array,
                                          Register bytecode_offset,
                                          Register bytecode, Register scratch1,
                                          Register scratch2, Label* if_return) {
  Register bytecode_size_table = scratch1;
  Register scratch3 = bytecode;

  // The bytecode offset value will be increased by one in wide and extra wide
  // cases. In the case of having a wide or extra wide JumpLoop bytecode, we
  // will restore the original bytecode. In order to simplify the code, we have
  // a backup of it.
  Register original_bytecode_offset = scratch2;
  DCHECK(!AreAliased(bytecode_array, bytecode_offset, bytecode_size_table,
                     bytecode, original_bytecode_offset));
  __ Move(bytecode_size_table,
          ExternalReference::bytecode_size_table_address());
  __ Move(original_bytecode_offset, bytecode_offset);

  // Check if the bytecode is a Wide or ExtraWide prefix bytecode.
  Label process_bytecode, extra_wide;
  static_assert(0 == static_cast<int>(interpreter::Bytecode::kWide));
  static_assert(1 == static_cast<int>(interpreter::Bytecode::kExtraWide));
  static_assert(2 == static_cast<int>(interpreter::Bytecode::kDebugBreakWide));
  static_assert(3 ==
                static_cast<int>(interpreter::Bytecode::kDebugBreakExtraWide));
  __ CmpS64(bytecode, Operand(0x3));
  __ bgt(&process_bytecode);
  __ tmll(bytecode, Operand(0x1));
  __ bne(&extra_wide);

  // Load the next bytecode and update table to the wide scaled table.
  __ AddS64(bytecode_offset, bytecode_offset, Operand(1));
  __ LoadU8(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ AddS64(bytecode_size_table, bytecode_size_table,
            Operand(kByteSize * interpreter::Bytecodes::kBytecodeCount));
  __ b(&process_bytecode);

  __ bind(&extra_wide);
  // Load the next bytecode and update table to the extra wide scaled table.
  __ AddS64(bytecode_offset, bytecode_offset, Operand(1));
  __ LoadU8(bytecode, MemOperand(bytecode_array, bytecode_offset));
  __ AddS64(bytecode_size_table, bytecode_size_table,
            Operand(2 * kByteSize * interpreter::Bytecodes::kBytecodeCount));

  // Load the size of the current bytecode.
  __ bind(&process_bytecode);

  // Bailout to the return label if this is a return bytecode.
#define JUMP_IF_EQUAL(NAME)                                             \
  __ CmpS64(bytecode,                                                   \
            Operand(static_cast<int>(interpreter::Bytecode::k##NAME))); \
  __ beq(if_return);
  RETURN_BYTECODE_LIST(JUMP_IF_EQUAL)
#undef JUMP_IF_EQUAL

  // If this is a JumpLoop, re-execute it to perform the jump to the beginning
  // of the loop.
  Label end, not_jump_loop;
  __ CmpS64(bytecode,
            Operand(static_cast<int>(interpreter::Bytecode::kJumpLoop)));
  __ bne(&not_jump_loop);
  // We need to restore the original bytecode_offset since we might have
  // increased it to skip the wide / extra-wide prefix bytecode.
  __ Move(bytecode_offset, original_bytecode_offset);
  __ b(&end);

  __ bind(&not_jump_loop);
  // Otherwise, load the size of the current bytecode and advance the offset.
  __ LoadU8(scratch3, MemOperand(bytecode_size_table, bytecode));
  __ AddS64(bytecode_offset, bytecode_offset, scratch3);

  __ bind(&end);
}

#if ENABLE_SPARKPLUG
// static
void Builtins::Generate_BaselineOutOfLinePrologue(MacroAssembler* masm) {
  // UseScratchRegisterScope temps(masm);
  // Need a few extra registers
  // temps.Include(r8, r9);

  auto descriptor =
      Builtins::CallInterfaceDescriptorFor(Builtin::kBaselineOutOfLinePrologue);
  Register closure = descriptor.GetRegisterParameter(
      BaselineOutOfLinePrologueDescriptor::kClosure);
  // Load the feedback vector from the closure.
  Register feedback_vector = ip;
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(feedback_vector, Cell::kValueOffset));
  __ AssertFeedbackVector(feedback_vector, r1);

  // Check for an tiering state.
  Label flags_need_processing;
  Register flags = r9;
  {
    __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
        flags, feedback_vector, CodeKind::BASELINE, &flags_need_processing);
  }

  {
    UseScratchRegisterScope temps(masm);
    ResetFeedbackVectorOsrUrgency(masm, feedback_vector, r1);
  }

  // Increment invocation count for the function.
  {
    Register invocation_count = r1;
    __ LoadU64(invocation_count,
               FieldMemOperand(feedback_vector,
                               FeedbackVector::kInvocationCountOffset));
    __ AddU64(invocation_count, Operand(1));
    __ StoreU64(invocation_count,
                FieldMemOperand(feedback_vector,
                                FeedbackVector::kInvocationCountOffset));
  }

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  {
    ASM_CODE_COMMENT_STRING(masm, "Frame Setup");
    // Normally the first thing we'd do here is Push(lr, fp), but we already
    // entered the frame in BaselineCompiler::Prologue, as we had to use the
    // value lr before the call to this BaselineOutOfLinePrologue builtin.

    Register callee_context = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kCalleeContext);
    Register callee_js_function = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kClosure);
    __ Push(callee_context, callee_js_function);
    DCHECK_EQ(callee_js_function, kJavaScriptCallTargetRegister);
    DCHECK_EQ(callee_js_function, kJSFunctionRegister);

    Register argc = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kJavaScriptCallArgCount);
    // We'll use the bytecode for both code age/OSR resetting, and pushing onto
    // the frame, so load it into a register.
    Register bytecodeArray = descriptor.GetRegisterParameter(
        BaselineOutOfLinePrologueDescriptor::kInterpreterBytecodeArray);
    ResetBytecodeAge(masm, bytecodeArray, r1);

    __ Push(argc, bytecodeArray);

    // Baseline code frames store the feedback vector where interpreter would
    // store the bytecode offset.
    if (v8_flags.debug_code) {
      Register scratch = r1;
      __ CompareObjectType(feedback_vector, scratch, scratch,
                           FEEDBACK_VECTOR_TYPE);
      __ Assert(eq, AbortReason::kExpectedFeedbackVector);
    }
    __ Push(feedback_vector);
  }

  Label call_stack_guard;
  Register frame_size = descriptor.GetRegisterParameter(
      BaselineOutOfLinePrologueDescriptor::kStackFrameSize);
  {
    ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt check");
    // Stack check. This folds the checks for both the interrupt stack limit
    // check and the real stack limit into one by just checking for the
    // interrupt limit. The interrupt limit is either equal to the real stack
    // limit or tighter. By ensuring we have space until that limit after
    // building the frame we can quickly precheck both at once.

    Register sp_minus_frame_size = r1;
    Register interrupt_limit = r0;
    __ SubS64(sp_minus_frame_size, sp, frame_size);
    __ LoadStackLimit(interrupt_limit, StackLimitKind::kInterruptStackLimit);
    __ CmpU64(sp_minus_frame_size, interrupt_limit);
    __ blt(&call_stack_guard);
  }

  // Do "fast" return to the caller pc in lr.
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
  __ Ret();

  __ bind(&flags_need_processing);
  {
    ASM_CODE_COMMENT_STRING(masm, "Optimized marker check");

    // Drop the frame created by the baseline call.
    __ Pop(r14, fp);
    __ OptimizeCodeOrTailCallOptimizedCodeSlot(flags, feedback_vector);
    __ Trap();
  }

  __ bind(&call_stack_guard);
  {
    ASM_CODE_COMMENT_STRING(masm, "Stack/interrupt call");
    FrameScope frame_scope(masm, StackFrame::INTERNAL);
    // Save incoming new target or generator
    __ Push(kJavaScriptCallNewTargetRegister);
    __ SmiTag(frame_size);
    __ Push(frame_size);
    __ CallRuntime(Runtime::kStackGuardWithGap);
    __ Pop(kJavaScriptCallNewTargetRegister);
  }

  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
  __ Ret();
}
#endif

// static
void Builtins::Generate_BaselineOutOfLinePrologueDeopt(MacroAssembler* masm) {
  // We're here because we got deopted during BaselineOutOfLinePrologue's stack
  // check. Undo all its frame creation and call into the interpreter instead.

  // Drop bytecode offset (was the feedback vector but got replaced during
  // deopt) and bytecode array.
  __ Drop(2);

  // Context, closure, argc.
  __ Pop(kContextRegister, kJavaScriptCallTargetRegister,
         kJavaScriptCallArgCountRegister);

  // Drop frame pointer
  __ LeaveFrame(StackFrame::BASELINE);

  // Enter the interpreter.
  __ TailCallBuiltin(Builtin::kInterpreterEntryTrampoline);
}

// Generate code for entering a JS function with the interpreter.
// On entry to the function the receiver and arguments have been pushed on the
// stack left to right.
//
// The live registers are:
//   o r2: actual argument count
//   o r3: the JS function object being called.
//   o r5: the incoming new target or generator object
//   o cp: our context
//   o pp: the caller's constant pool pointer (if enabled)
//   o fp: the caller's frame pointer
//   o sp: stack pointer
//   o lr: return address
//
// The function builds an interpreter frame.  See InterpreterFrameConstants in
// frame-constants.h for its layout.
void Builtins::Generate_InterpreterEntryTrampoline(
    MacroAssembler* masm, InterpreterEntryTrampolineMode mode) {
  Register closure = r3;

  // Get the bytecode array from the function object and load it into
  // kInterpreterBytecodeArrayRegister.
  __ LoadTaggedField(
      r6, FieldMemOperand(closure, JSFunction::kSharedFunctionInfoOffset));
  // Load original bytecode array or the debug copy.
  __ LoadTaggedField(
      kInterpreterBytecodeArrayRegister,
      FieldMemOperand(r6, SharedFunctionInfo::kFunctionDataOffset));

  Label is_baseline;
  GetSharedFunctionInfoBytecodeOrBaseline(
      masm, kInterpreterBytecodeArrayRegister, ip, &is_baseline);

  // The bytecode array could have been flushed from the shared function info,
  // if so, call into CompileLazy.
  Label compile_lazy;
  __ CompareObjectType(kInterpreterBytecodeArrayRegister, r6, no_reg,
                       BYTECODE_ARRAY_TYPE);
  __ bne(&compile_lazy);

#ifndef V8_JITLESS
  // Load the feedback vector from the closure.
  Register feedback_vector = r4;
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
  __ LoadTaggedField(feedback_vector,
                     FieldMemOperand(feedback_vector, Cell::kValueOffset));

  Label push_stack_frame;
  // Check if feedback vector is valid. If valid, check for optimized code
  // and update invocation count. Otherwise, setup the stack frame.
  __ LoadTaggedField(r6,
                     FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
  __ LoadU16(r6, FieldMemOperand(r6, Map::kInstanceTypeOffset));
  __ CmpS64(r6, Operand(FEEDBACK_VECTOR_TYPE));
  __ bne(&push_stack_frame);

  Register flags = r6;
  Label flags_need_processing;
  __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
      flags, feedback_vector, CodeKind::INTERPRETED_FUNCTION,
      &flags_need_processing);

  {
    UseScratchRegisterScope temps(masm);
    ResetFeedbackVectorOsrUrgency(masm, feedback_vector, r1);
  }

  Label not_optimized;
  __ bind(&not_optimized);

  // Increment invocation count for the function.
  __ LoadS32(r1, FieldMemOperand(feedback_vector,
                               FeedbackVector::kInvocationCountOffset));
  __ AddS64(r1, r1, Operand(1));
  __ StoreU32(r1, FieldMemOperand(feedback_vector,
                                  FeedbackVector::kInvocationCountOffset));

  // Open a frame scope to indicate that there is a frame on the stack.  The
  // MANUAL indicates that the scope shouldn't actually generate code to set up
  // the frame (that is done below).
  __ bind(&push_stack_frame);
#else
  // Note: By omitting the above code in jitless mode we also disable:
  // - kFlagsLogNextExecution: only used for logging/profiling; and
  // - kInvocationCountOffset: only used for tiering heuristics and code
  //   coverage.
#endif  // !V8_JITLESS
  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ PushStandardFrame(closure);

  ResetBytecodeAge(masm, kInterpreterBytecodeArrayRegister, r1);

  // Load the initial bytecode offset.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));

  // Push bytecode array and Smi tagged bytecode array offset.
  __ SmiTag(r4, kInterpreterBytecodeOffsetRegister);
  __ Push(kInterpreterBytecodeArrayRegister, r4);

  // Allocate the local and temporary register file on the stack.
  Label stack_overflow;
  {
    // Load frame size (word) from the BytecodeArray object.
    __ LoadU32(r4, FieldMemOperand(kInterpreterBytecodeArrayRegister,
                                  BytecodeArray::kFrameSizeOffset));

    // Do a stack check to ensure we don't go over the limit.
    __ SubS64(r8, sp, r4);
    __ CmpU64(r8, __ StackLimitAsMemOperand(StackLimitKind::kRealStackLimit));
    __ blt(&stack_overflow);

    // If ok, push undefined as the initial value for all register file entries.
    // TODO(rmcilroy): Consider doing more than one push per loop iteration.
    Label loop, no_args;
    __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);
    __ ShiftRightU64(r4, r4, Operand(kSystemPointerSizeLog2));
    __ LoadAndTestP(r4, r4);
    __ beq(&no_args);
    __ mov(r1, r4);
    __ bind(&loop);
    __ push(kInterpreterAccumulatorRegister);
    __ SubS64(r1, Operand(1));
    __ bne(&loop);
    __ bind(&no_args);
  }

  // If the bytecode array has a valid incoming new target or generator object
  // register, initialize it with incoming value which was passed in r5.
  Label no_incoming_new_target_or_generator_register;
  __ LoadS32(r8, FieldMemOperand(
                   kInterpreterBytecodeArrayRegister,
                   BytecodeArray::kIncomingNewTargetOrGeneratorRegisterOffset));
  __ CmpS64(r8, Operand::Zero());
  __ beq(&no_incoming_new_target_or_generator_register);
  __ ShiftLeftU64(r8, r8, Operand(kSystemPointerSizeLog2));
  __ StoreU64(r5, MemOperand(fp, r8));
  __ bind(&no_incoming_new_target_or_generator_register);

  // Perform interrupt stack check.
  // TODO(solanes): Merge with the real stack limit check above.
  Label stack_check_interrupt, after_stack_check_interrupt;
  __ LoadU64(r0,
             __ StackLimitAsMemOperand(StackLimitKind::kInterruptStackLimit));
  __ CmpU64(sp, r0);
  __ blt(&stack_check_interrupt);
  __ bind(&after_stack_check_interrupt);

  // The accumulator is already loaded with undefined.

  // Load the dispatch table into a register and dispatch to the bytecode
  // handler at the current bytecode offset.
  Label do_dispatch;
  __ bind(&do_dispatch);
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  __ LoadU8(r5, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftU64(r5, r5, Operand(kSystemPointerSizeLog2));
  __ LoadU64(kJavaScriptCallCodeStartRegister,
             MemOperand(kInterpreterDispatchTableRegister, r5));
  __ Call(kJavaScriptCallCodeStartRegister);

  __ RecordComment("--- InterpreterEntryReturnPC point ---");
  if (mode == InterpreterEntryTrampolineMode::kDefault) {
    masm->isolate()->heap()->SetInterpreterEntryReturnPCOffset(
        masm->pc_offset());
  } else {
    DCHECK_EQ(mode, InterpreterEntryTrampolineMode::kForProfiling);
    // Both versions must be the same up to this point otherwise the builtins
    // will not be interchangable.
    CHECK_EQ(
        masm->isolate()->heap()->interpreter_entry_return_pc_offset().value(),
        masm->pc_offset());
  }

  // Any returns to the entry trampoline are either due to the return bytecode
  // or the interpreter tail calling a builtin and then a dispatch.

  // Get bytecode array and bytecode offset from the stack frame.
  __ LoadU64(kInterpreterBytecodeArrayRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadU64(kInterpreterBytecodeOffsetRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  // Either return, or advance to the next bytecode and dispatch.
  Label do_return;
  __ LoadU8(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, r3, r4, r5,
                                &do_return);
  __ b(&do_dispatch);

  __ bind(&do_return);
  // The return value is in r2.
  LeaveInterpreterFrame(masm, r4, r6);
  __ Ret();

  __ bind(&stack_check_interrupt);
  // Modify the bytecode offset in the stack to be kFunctionEntryBytecodeOffset
  // for the call to the StackGuard.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(Smi::FromInt(BytecodeArray::kHeaderSize - kHeapObjectTag +
                              kFunctionEntryBytecodeOffset)));
  __ StoreU64(kInterpreterBytecodeOffsetRegister,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ CallRuntime(Runtime::kStackGuard);

  // After the call, restore the bytecode array, bytecode offset and accumulator
  // registers again. Also, restore the bytecode offset in the stack to its
  // previous value.
  __ LoadU64(kInterpreterBytecodeArrayRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ LoadRoot(kInterpreterAccumulatorRegister, RootIndex::kUndefinedValue);

  __ SmiTag(r0, kInterpreterBytecodeOffsetRegister);
  __ StoreU64(r0,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  __ jmp(&after_stack_check_interrupt);

#ifndef V8_JITLESS
  __ bind(&flags_need_processing);
  __ OptimizeCodeOrTailCallOptimizedCodeSlot(flags, feedback_vector);

  __ bind(&is_baseline);
  {
    // Load the feedback vector from the closure.
    __ LoadTaggedField(
        feedback_vector,
        FieldMemOperand(closure, JSFunction::kFeedbackCellOffset));
    __ LoadTaggedField(feedback_vector,
                       FieldMemOperand(feedback_vector, Cell::kValueOffset));

    Label install_baseline_code;
    // Check if feedback vector is valid. If not, call prepare for baseline to
    // allocate it.
    __ LoadTaggedField(
        ip, FieldMemOperand(feedback_vector, HeapObject::kMapOffset));
    __ LoadU16(ip, FieldMemOperand(ip, Map::kInstanceTypeOffset));
    __ CmpS32(ip, Operand(FEEDBACK_VECTOR_TYPE));
    __ b(ne, &install_baseline_code);

    // Check for an tiering state.
    __ LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
        flags, feedback_vector, CodeKind::BASELINE, &flags_need_processing);

    // Load the baseline code into the closure.
    __ mov(r4, kInterpreterBytecodeArrayRegister);
    static_assert(kJavaScriptCallCodeStartRegister == r4, "ABI mismatch");
    __ ReplaceClosureCodeWithOptimizedCode(r4, closure, ip, r1);
    __ JumpCodeObject(r4);

    __ bind(&install_baseline_code);
    __ GenerateTailCallToReturnedCode(Runtime::kInstallBaselineCode);
  }
#endif  // !V8_JITLESS

  __ bind(&compile_lazy);
  __ GenerateTailCallToReturnedCode(Runtime::kCompileLazy);

  __ bind(&stack_overflow);
  __ CallRuntime(Runtime::kThrowStackOverflow);
  __ bkpt(0);  // Should not return.
}

static void GenerateInterpreterPushArgs(MacroAssembler* masm, Register num_args,
                                        Register start_address,
                                        Register scratch) {
  ASM_CODE_COMMENT(masm);
  __ SubS64(scratch, num_args, Operand(1));
  __ ShiftLeftU64(scratch, scratch, Operand(kSystemPointerSizeLog2));
  __ SubS64(start_address, start_address, scratch);
  // Push the arguments.
  __ PushArray(start_address, num_args, r1, scratch,
               MacroAssembler::PushArrayOrder::kReverse);
}

// static
void Builtins::Generate_InterpreterPushArgsThenCallImpl(
    MacroAssembler* masm, ConvertReceiverMode receiver_mode,
    InterpreterPushArgsMode mode) {
  DCHECK(mode != InterpreterPushArgsMode::kArrayFunction);
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r4 : the address of the first argument to be pushed. Subsequent
  //          arguments should be consecutive above this, in the same order as
  //          they are to be pushed onto the stack.
  //  -- r3 : the target to call (can be any Object).
  // -----------------------------------
  Label stack_overflow;
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ SubS64(r2, r2, Operand(1));
  }

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ SubS64(r5, r2, Operand(kJSArgcReceiverSlots));
  } else {
    __ mov(r5, r2);
  }

  __ StackOverflowCheck(r5, ip, &stack_overflow);

  // Push the arguments.
  GenerateInterpreterPushArgs(masm, r5, r4, r6);

  if (receiver_mode == ConvertReceiverMode::kNullOrUndefined) {
    __ PushRoot(RootIndex::kUndefinedValue);
  }

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register r2.
    // r2 already points to the penultimate argument, the spread
    // lies in the next interpreter register.
    __ LoadU64(r4, MemOperand(r4, -kSystemPointerSize));
  }

  // Call the target.
  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    __ Jump(BUILTIN_CODE(masm->isolate(), CallWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    __ Jump(masm->isolate()->builtins()->Call(ConvertReceiverMode::kAny),
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
  // -- r2 : argument count
  // -- r5 : new target
  // -- r3 : constructor to call
  // -- r4 : allocation site feedback if available, undefined otherwise.
  // -- r6 : address of the first argument
  // -----------------------------------
  Label stack_overflow;
  __ StackOverflowCheck(r2, ip, &stack_overflow);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // The spread argument should not be pushed.
    __ SubS64(r2, r2, Operand(1));
  }

  Register argc_without_receiver = ip;
  __ SubS64(argc_without_receiver, r2, Operand(kJSArgcReceiverSlots));
  // Push the arguments. r4 and r5 will be modified.
  GenerateInterpreterPushArgs(masm, argc_without_receiver, r6, r7);

  // Push a slot for the receiver to be constructed.
  __ mov(r0, Operand::Zero());
  __ push(r0);

  if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Pass the spread in the register r2.
    // r4 already points to the penultimate argument, the spread
    // lies in the next interpreter register.
    __ lay(r6, MemOperand(r6, -kSystemPointerSize));
    __ LoadU64(r4, MemOperand(r6));
  } else {
    __ AssertUndefinedOrAllocationSite(r4, r7);
  }

  if (mode == InterpreterPushArgsMode::kArrayFunction) {
    __ AssertFunction(r3);

    // Tail call to the array construct stub (still in the caller
    // context at this point).
    Handle<Code> code = BUILTIN_CODE(masm->isolate(), ArrayConstructorImpl);
    __ Jump(code, RelocInfo::CODE_TARGET);
  } else if (mode == InterpreterPushArgsMode::kWithFinalSpread) {
    // Call the constructor with r2, r3, and r5 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), ConstructWithSpread),
            RelocInfo::CODE_TARGET);
  } else {
    DCHECK_EQ(InterpreterPushArgsMode::kOther, mode);
    // Call the constructor with r2, r3, and r5 unmodified.
    __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
  }

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
  Label builtin_trampoline, trampoline_loaded;
  Smi interpreter_entry_return_pc_offset(
      masm->isolate()->heap()->interpreter_entry_return_pc_offset());
  DCHECK_NE(interpreter_entry_return_pc_offset, Smi::zero());

  // If the SFI function_data is an InterpreterData, the function will have a
  // custom copy of the interpreter entry trampoline for profiling. If so,
  // get the custom trampoline, otherwise grab the entry address of the global
  // trampoline.
  __ LoadU64(r4, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadTaggedField(
      r4, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  __ LoadTaggedField(
      r4, FieldMemOperand(r4, SharedFunctionInfo::kFunctionDataOffset));
  __ CompareObjectType(r4, kInterpreterDispatchTableRegister,
                       kInterpreterDispatchTableRegister,
                       INTERPRETER_DATA_TYPE);
  __ bne(&builtin_trampoline);

  __ LoadTaggedField(
      r4, FieldMemOperand(r4, InterpreterData::kInterpreterTrampolineOffset));
  __ LoadCodeEntry(r4, r4);
  __ b(&trampoline_loaded);

  __ bind(&builtin_trampoline);
  __ Move(r4, ExternalReference::
                  address_of_interpreter_entry_trampoline_instruction_start(
                      masm->isolate()));
  __ LoadU64(r4, MemOperand(r4));

  __ bind(&trampoline_loaded);
  __ AddS64(r14, r4, Operand(interpreter_entry_return_pc_offset.value()));

  // Initialize the dispatch table register.
  __ Move(
      kInterpreterDispatchTableRegister,
      ExternalReference::interpreter_dispatch_table_address(masm->isolate()));

  // Get the bytecode array pointer from the frame.
  __ LoadU64(kInterpreterBytecodeArrayRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));

  if (v8_flags.debug_code) {
    // Check function data field is actually a BytecodeArray object.
    __ TestIfSmi(kInterpreterBytecodeArrayRegister);
    __ Assert(
        ne, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
    __ CompareObjectType(kInterpreterBytecodeArrayRegister, r3, no_reg,
                         BYTECODE_ARRAY_TYPE);
    __ Assert(
        eq, AbortReason::kFunctionDataShouldBeBytecodeArrayOnInterpreterEntry);
  }

  // Get the target bytecode offset from the frame.
  __ LoadU64(kInterpreterBytecodeOffsetRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  if (v8_flags.debug_code) {
    Label okay;
    __ CmpS64(kInterpreterBytecodeOffsetRegister,
              Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
    __ bge(&okay);
    __ bkpt(0);
    __ bind(&okay);
  }

  // Dispatch to the target bytecode.
  UseScratchRegisterScope temps(masm);
  Register scratch = temps.Acquire();
  __ LoadU8(scratch, MemOperand(kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister));
  __ ShiftLeftU64(scratch, scratch, Operand(kSystemPointerSizeLog2));
  __ LoadU64(kJavaScriptCallCodeStartRegister,
             MemOperand(kInterpreterDispatchTableRegister, scratch));
  __ Jump(kJavaScriptCallCodeStartRegister);
}

void Builtins::Generate_InterpreterEnterAtNextBytecode(MacroAssembler* masm) {
  // Get bytecode array and bytecode offset from the stack frame.
  __ LoadU64(kInterpreterBytecodeArrayRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeArrayFromFp));
  __ LoadU64(kInterpreterBytecodeOffsetRegister,
             MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));
  __ SmiUntag(kInterpreterBytecodeOffsetRegister);

  Label enter_bytecode, function_entry_bytecode;
  __ CmpS64(kInterpreterBytecodeOffsetRegister,
            Operand(BytecodeArray::kHeaderSize - kHeapObjectTag +
                    kFunctionEntryBytecodeOffset));
  __ beq(&function_entry_bytecode);

  // Load the current bytecode.
  __ LoadU8(r3, MemOperand(kInterpreterBytecodeArrayRegister,
                           kInterpreterBytecodeOffsetRegister));

  // Advance to the next bytecode.
  Label if_return;
  AdvanceBytecodeOffsetOrReturn(masm, kInterpreterBytecodeArrayRegister,
                                kInterpreterBytecodeOffsetRegister, r3, r4, r5,
                                &if_return);

  __ bind(&enter_bytecode);
  // Convert new bytecode offset to a Smi and save in the stackframe.
  __ SmiTag(r4, kInterpreterBytecodeOffsetRegister);
  __ StoreU64(r4,
              MemOperand(fp, InterpreterFrameConstants::kBytecodeOffsetFromFp));

  Generate_InterpreterEnterBytecode(masm);

  __ bind(&function_entry_bytecode);
  // If the code deoptimizes during the implicit function entry stack interrupt
  // check, it will have a bailout ID of kFunctionEntryBytecodeOffset, which is
  // not a valid bytecode offset. Detect this case and advance to the first
  // actual bytecode.
  __ mov(kInterpreterBytecodeOffsetRegister,
         Operand(BytecodeArray::kHeaderSize - kHeapObjectTag));
  __ b(&enter_bytecode);

  // We should never take the if_return path.
  __ bind(&if_return);
  __ Abort(AbortReason::kInvalidBytecodeAdvance);
}

void Builtins::Generate_InterpreterEnterAtBytecode(MacroAssembler* masm) {
  Generate_InterpreterEnterBytecode(masm);
}

namespace {
void Generate_ContinueToBuiltinHelper(MacroAssembler* masm,
                                      bool java_script_builtin,
                                      bool with_result) {
  const RegisterConfiguration* config(RegisterConfiguration::Default());
  int allocatable_register_count = config->num_allocatable_general_registers();
  Register scratch = ip;
  if (with_result) {
    if (java_script_builtin) {
      __ mov(scratch, r2);
    } else {
      // Overwrite the hole inserted by the deoptimizer with the return value
      // from the LAZY deopt point.
      __ StoreU64(
          r2, MemOperand(
                  sp, config->num_allocatable_general_registers() *
                              kSystemPointerSize +
                          BuiltinContinuationFrameConstants::kFixedFrameSize));
    }
  }
  for (int i = allocatable_register_count - 1; i >= 0; --i) {
    int code = config->GetAllocatableGeneralCode(i);
    __ Pop(Register::from_code(code));
    if (java_script_builtin && code == kJavaScriptCallArgCountRegister.code()) {
      __ SmiUntag(Register::from_code(code));
    }
  }
  if (java_script_builtin && with_result) {
    // Overwrite the hole inserted by the deoptimizer with the return value from
    // the LAZY deopt point. r0 contains the arguments count, the return value
    // from LAZY is always the last argument.
    constexpr int return_value_offset =
        BuiltinContinuationFrameConstants::kFixedSlotCount -
        kJSArgcReceiverSlots;
    __ AddS64(r2, r2, Operand(return_value_offset));
    __ ShiftLeftU64(r1, r2, Operand(kSystemPointerSizeLog2));
    __ StoreU64(scratch, MemOperand(sp, r1));
    // Recover arguments count.
    __ SubS64(r2, r2, Operand(return_value_offset));
  }
  __ LoadU64(
      fp,
      MemOperand(sp, BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  // Load builtin index (stored as a Smi) and use it to get the builtin start
  // address from the builtins table.
  UseScratchRegisterScope temps(masm);
  Register builtin = temps.Acquire();
  __ Pop(builtin);
  __ AddS64(sp, sp,
            Operand(BuiltinContinuationFrameConstants::kFixedFrameSizeFromFp));
  __ Pop(r0);
  __ mov(r14, r0);
  __ LoadEntryFromBuiltinIndex(builtin);
  __ Jump(builtin);
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

void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kNotifyDeoptimized);
  }

  DCHECK_EQ(kInterpreterAccumulatorRegister.code(), r2.code());
  __ pop(r2);
  __ Ret();
}

// static
void Builtins::Generate_FunctionPrototypeApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : argc
  //  -- sp[0] : receiver
  //  -- sp[4] : thisArg
  //  -- sp[8] : argArray
  // -----------------------------------

  // 1. Load receiver into r3, argArray into r4 (if present), remove all
  // arguments from the stack (including the receiver), and push thisArg (if
  // present) instead.
  {
    __ LoadRoot(r7, RootIndex::kUndefinedValue);
    __ mov(r4, r7);
    Label done;

    __ LoadU64(r3, MemOperand(sp));  // receiver
    __ CmpS64(r2, Operand(JSParameterCount(1)));
    __ blt(&done);
    __ LoadU64(r7, MemOperand(sp, kSystemPointerSize));  // thisArg
    __ CmpS64(r2, Operand(JSParameterCount(2)));
    __ blt(&done);
    __ LoadU64(r4, MemOperand(sp, 2 * kSystemPointerSize));  // argArray

    __ bind(&done);
    __ DropArgumentsAndPushNewReceiver(r2, r7, MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
  }

  // ----------- S t a t e -------------
  //  -- r4    : argArray
  //  -- r3    : receiver
  //  -- sp[0] : thisArg
  // -----------------------------------

  // 2. We don't need to check explicitly for callable receiver here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3. Tail call with no arguments if argArray is null or undefined.
  Label no_arguments;
  __ JumpIfRoot(r4, RootIndex::kNullValue, &no_arguments);
  __ JumpIfRoot(r4, RootIndex::kUndefinedValue, &no_arguments);

  // 4a. Apply the receiver to the given argArray.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);

  // 4b. The argArray is either null or undefined, so we tail call without any
  // arguments to the receiver.
  __ bind(&no_arguments);
  {
    __ mov(r2, Operand(JSParameterCount(0)));
    __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
  }
}

// static
void Builtins::Generate_FunctionPrototypeCall(MacroAssembler* masm) {
  // 1. Get the callable to call (passed as receiver) from the stack.
  __ Pop(r3);

  // 2. Make sure we have at least one argument.
  // r2: actual number of arguments
  {
    Label done;
    __ CmpS64(r2, Operand(JSParameterCount(0)));
    __ b(ne, &done);
    __ PushRoot(RootIndex::kUndefinedValue);
    __ AddS64(r2, r2, Operand(1));
    __ bind(&done);
  }

  // 3. Adjust the actual number of arguments.
  __ SubS64(r2, r2, Operand(1));

  // 4. Call the callable.
  __ Jump(masm->isolate()->builtins()->Call(), RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectApply(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : argc
  //  -- sp[0]  : receiver
  //  -- sp[4]  : target         (if argc >= 1)
  //  -- sp[8]  : thisArgument   (if argc >= 2)
  //  -- sp[12] : argumentsList  (if argc == 3)
  // -----------------------------------

  // 1. Load target into r3 (if present), argumentsList into r4 (if present),
  // remove all arguments from the stack (including the receiver), and push
  // thisArgument (if present) instead.
  {
    __ LoadRoot(r3, RootIndex::kUndefinedValue);
    __ mov(r7, r3);
    __ mov(r4, r3);

    Label done;

    __ CmpS64(r2, Operand(JSParameterCount(1)));
    __ blt(&done);
    __ LoadU64(r3, MemOperand(sp, kSystemPointerSize));  // thisArg
    __ CmpS64(r2, Operand(JSParameterCount(2)));
    __ blt(&done);
    __ LoadU64(r7, MemOperand(sp, 2 * kSystemPointerSize));  // argArray
    __ CmpS64(r2, Operand(JSParameterCount(3)));
    __ blt(&done);
    __ LoadU64(r4, MemOperand(sp, 3 * kSystemPointerSize));  // argArray

    __ bind(&done);
    __ DropArgumentsAndPushNewReceiver(r2, r7, MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
  }

  // ----------- S t a t e -------------
  //  -- r4    : argumentsList
  //  -- r3    : target
  //  -- sp[0] : thisArgument
  // -----------------------------------

  // 2. We don't need to check explicitly for callable target here,
  // since that's the first thing the Call/CallWithArrayLike builtins
  // will do.

  // 3 Apply the target to the given argumentsList.
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWithArrayLike),
          RelocInfo::CODE_TARGET);
}

void Builtins::Generate_ReflectConstruct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : argc
  //  -- sp[0]  : receiver
  //  -- sp[4]  : target
  //  -- sp[8]  : argumentsList
  //  -- sp[12] : new.target (optional)
  // -----------------------------------

  // 1. Load target into r3 (if present), argumentsList into r4 (if present),
  // new.target into r5 (if present, otherwise use target), remove all
  // arguments from the stack (including the receiver), and push thisArgument
  // (if present) instead.
  {
    __ LoadRoot(r3, RootIndex::kUndefinedValue);
    __ mov(r4, r3);

    Label done;

    __ mov(r6, r3);
    __ CmpS64(r2, Operand(JSParameterCount(1)));
    __ blt(&done);
    __ LoadU64(r3, MemOperand(sp, kSystemPointerSize));  // thisArg
    __ mov(r5, r3);
    __ CmpS64(r2, Operand(JSParameterCount(2)));
    __ blt(&done);
    __ LoadU64(r4, MemOperand(sp, 2 * kSystemPointerSize));  // argArray
    __ CmpS64(r2, Operand(JSParameterCount(3)));
    __ blt(&done);
    __ LoadU64(r5, MemOperand(sp, 3 * kSystemPointerSize));  // argArray
    __ bind(&done);
    __ DropArgumentsAndPushNewReceiver(r2, r6, MacroAssembler::kCountIsInteger,
                                       MacroAssembler::kCountIncludesReceiver);
  }

  // ----------- S t a t e -------------
  //  -- r4    : argumentsList
  //  -- r5    : new.target
  //  -- r3    : target
  //  -- sp[0] : receiver (undefined)
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

namespace {

// Allocate new stack space for |count| arguments and shift all existing
// arguments already on the stack. |pointer_to_new_space_out| points to the
// first free slot on the stack to copy additional arguments to and
// |argc_in_out| is updated to include |count|.
void Generate_AllocateSpaceAndShiftExistingArguments(
    MacroAssembler* masm, Register count, Register argc_in_out,
    Register pointer_to_new_space_out, Register scratch1, Register scratch2) {
  DCHECK(!AreAliased(count, argc_in_out, pointer_to_new_space_out, scratch1,
                     scratch2));
  Register old_sp = scratch1;
  Register new_space = scratch2;
  __ mov(old_sp, sp);
  __ ShiftLeftU64(new_space, count, Operand(kSystemPointerSizeLog2));
  __ AllocateStackSpace(new_space);

  Register end = scratch2;
  Register value = r1;
  Register dest = pointer_to_new_space_out;
  __ mov(dest, sp);
  __ ShiftLeftU64(r0, argc_in_out, Operand(kSystemPointerSizeLog2));
  __ AddS64(end, old_sp, r0);
  Label loop, done;
  __ bind(&loop);
  __ CmpS64(old_sp, end);
  __ bge(&done);
  __ LoadU64(value, MemOperand(old_sp));
  __ lay(old_sp, MemOperand(old_sp, kSystemPointerSize));
  __ StoreU64(value, MemOperand(dest));
  __ lay(dest, MemOperand(dest, kSystemPointerSize));
  __ b(&loop);
  __ bind(&done);

  // Update total number of arguments.
  __ AddS64(argc_in_out, argc_in_out, count);
}

}  // namespace

// static
// TODO(v8:11615): Observe InstructionStream::kMaxArguments in
// CallOrConstructVarargs
void Builtins::Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                               Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r3 : target
  //  -- r2 : number of parameters on the stack
  //  -- r4 : arguments list (a FixedArray)
  //  -- r6 : len (number of elements to push from args)
  //  -- r5 : new.target (for [[Construct]])
  // -----------------------------------

  Register scratch = ip;

  if (v8_flags.debug_code) {
    // Allow r4 to be a FixedArray, or a FixedDoubleArray if r6 == 0.
    Label ok, fail;
    __ AssertNotSmi(r4);
    __ LoadTaggedField(scratch, FieldMemOperand(r4, HeapObject::kMapOffset));
    __ LoadS16(scratch,
                     FieldMemOperand(scratch, Map::kInstanceTypeOffset));
    __ CmpS64(scratch, Operand(FIXED_ARRAY_TYPE));
    __ beq(&ok);
    __ CmpS64(scratch, Operand(FIXED_DOUBLE_ARRAY_TYPE));
    __ bne(&fail);
    __ CmpS64(r6, Operand::Zero());
    __ beq(&ok);
    // Fall through.
    __ bind(&fail);
    __ Abort(AbortReason::kOperandIsNotAFixedArray);

    __ bind(&ok);
  }

  // Check for stack overflow.
  Label stack_overflow;
  __ StackOverflowCheck(r6, scratch, &stack_overflow);

  // Move the arguments already in the stack,
  // including the receiver and the return address.
  // r6: Number of arguments to make room for.
  // r2: Number of arguments already on the stack.
  // r7: Points to first free slot on the stack after arguments were shifted.
  Generate_AllocateSpaceAndShiftExistingArguments(masm, r6, r2, r7, ip, r8);

  // Push arguments onto the stack (thisArgument is already on the stack).
  {
    Label loop, no_args, skip;
    __ CmpS64(r6, Operand::Zero());
    __ beq(&no_args);
    __ AddS64(r4, r4,
              Operand(FixedArray::kHeaderSize - kHeapObjectTag - kTaggedSize));
    __ mov(r1, r6);
    __ bind(&loop);
    __ LoadTaggedField(scratch, MemOperand(r4, kTaggedSize), r0);
    __ la(r4, MemOperand(r4, kTaggedSize));
    __ CompareRoot(scratch, RootIndex::kTheHoleValue);
    __ bne(&skip, Label::kNear);
    __ LoadRoot(scratch, RootIndex::kUndefinedValue);
    __ bind(&skip);
    __ StoreU64(scratch, MemOperand(r7));
    __ lay(r7, MemOperand(r7, kSystemPointerSize));
    __ BranchOnCount(r1, &loop);
    __ bind(&no_args);
  }

  // Tail-call to the actual Call or Construct builtin.
  __ Jump(code, RelocInfo::CODE_TARGET);

  __ bind(&stack_overflow);
  __ TailCallRuntime(Runtime::kThrowStackOverflow);
}

// static
void Builtins::Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                      CallOrConstructMode mode,
                                                      Handle<Code> code) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r5 : the new.target (for [[Construct]] calls)
  //  -- r3 : the target to call (can be any Object)
  //  -- r4 : start index (to support rest parameters)
  // -----------------------------------

  Register scratch = r8;

  if (mode == CallOrConstructMode::kConstruct) {
    Label new_target_constructor, new_target_not_constructor;
    __ JumpIfSmi(r5, &new_target_not_constructor);
    __ LoadTaggedField(scratch, FieldMemOperand(r5, HeapObject::kMapOffset));
    __ LoadU8(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tmll(scratch, Operand(Map::Bits1::IsConstructorBit::kShift));
    __ bne(&new_target_constructor);
    __ bind(&new_target_not_constructor);
    {
      FrameScope scope(masm, StackFrame::MANUAL);
      __ EnterFrame(StackFrame::INTERNAL);
      __ Push(r5);
      __ CallRuntime(Runtime::kThrowNotConstructor);
      __ Trap();  // Unreachable.
    }
    __ bind(&new_target_constructor);
  }

  Label stack_done, stack_overflow;
  __ LoadU64(r7, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ SubS64(r7, r7, Operand(kJSArgcReceiverSlots));
  __ SubS64(r7, r7, r4);
  __ ble(&stack_done);
  {
    // ----------- S t a t e -------------
    //  -- r2 : the number of arguments already in the stack
    //  -- r3 : the target to call (can be any Object)
    //  -- r4 : start index (to support rest parameters)
    //  -- r5 : the new.target (for [[Construct]] calls)
    //  -- r6 : point to the caller stack frame
    //  -- r7 : number of arguments to copy, i.e. arguments count - start index
    // -----------------------------------

    // Check for stack overflow.
    __ StackOverflowCheck(r7, scratch, &stack_overflow);

    // Forward the arguments from the caller frame.
    __ mov(r5, r5);
    // Point to the first argument to copy (skipping the receiver).
    __ AddS64(r6, fp,
              Operand(CommonFrameConstants::kFixedFrameSizeAboveFp +
                      kSystemPointerSize));
    __ ShiftLeftU64(scratch, r4, Operand(kSystemPointerSizeLog2));
    __ AddS64(r6, r6, scratch);

    // Move the arguments already in the stack,
    // including the receiver and the return address.
    // r7: Number of arguments to make room for.0
    // r2: Number of arguments already on the stack.
    // r4: Points to first free slot on the stack after arguments were shifted.
    Generate_AllocateSpaceAndShiftExistingArguments(masm, r7, r2, r4, scratch,
                                                    ip);

    // Copy arguments from the caller frame.
    // TODO(victorgomes): Consider using forward order as potentially more cache
    // friendly.
    {
      Label loop;
      __ bind(&loop);
      {
        __ SubS64(r7, r7, Operand(1));
        __ ShiftLeftU64(r1, r7, Operand(kSystemPointerSizeLog2));
        __ LoadU64(scratch, MemOperand(r6, r1));
        __ StoreU64(scratch, MemOperand(r4, r1));
        __ CmpS64(r7, Operand::Zero());
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

// static
void Builtins::Generate_CallFunction(MacroAssembler* masm,
                                     ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : the function to call (checked to be a JSFunction)
  // -----------------------------------
  __ AssertCallableFunction(r3);

  __ LoadTaggedField(
      r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));

  // Enter the context of the function; ToObject has to run in the function
  // context, and we also need to take the global proxy from the function
  // context in case of conversion.
  __ LoadTaggedField(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
  // We need to convert the receiver for non-native sloppy mode functions.
  Label done_convert;
  __ LoadU32(r5, FieldMemOperand(r4, SharedFunctionInfo::kFlagsOffset));
  __ AndP(r0, r5,
          Operand(SharedFunctionInfo::IsStrictBit::kMask |
                  SharedFunctionInfo::IsNativeBit::kMask));
  __ bne(&done_convert);
  {
    // ----------- S t a t e -------------
    //  -- r2 : the number of arguments
    //  -- r3 : the function to call (checked to be a JSFunction)
    //  -- r4 : the shared function info.
    //  -- cp : the function context.
    // -----------------------------------

    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      // Patch receiver to global proxy.
      __ LoadGlobalProxy(r5);
    } else {
      Label convert_to_object, convert_receiver;
      __ LoadReceiver(r5, r2);
      __ JumpIfSmi(r5, &convert_to_object);
      static_assert(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
      __ CompareObjectType(r5, r6, r6, FIRST_JS_RECEIVER_TYPE);
      __ bge(&done_convert);
      if (mode != ConvertReceiverMode::kNotNullOrUndefined) {
        Label convert_global_proxy;
        __ JumpIfRoot(r5, RootIndex::kUndefinedValue, &convert_global_proxy);
        __ JumpIfNotRoot(r5, RootIndex::kNullValue, &convert_to_object);
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
        __ mov(r2, r5);
        __ Push(cp);
        __ Call(BUILTIN_CODE(masm->isolate(), ToObject),
                RelocInfo::CODE_TARGET);
        __ Pop(cp);
        __ mov(r5, r2);
        __ Pop(r2, r3);
        __ SmiUntag(r2);
      }
      __ LoadTaggedField(
          r4, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
      __ bind(&convert_receiver);
    }
    __ StoreReceiver(r5, r2, r6);
  }
  __ bind(&done_convert);

  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : the function to call (checked to be a JSFunction)
  //  -- r4 : the shared function info.
  //  -- cp : the function context.
  // -----------------------------------

  __ LoadU16(
      r4, FieldMemOperand(r4, SharedFunctionInfo::kFormalParameterCountOffset));
  __ InvokeFunctionCode(r3, no_reg, r4, r2, InvokeType::kJump);
}

namespace {

void Generate_PushBoundArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : target (checked to be a JSBoundFunction)
  //  -- r5 : new.target (only in case of [[Construct]])
  // -----------------------------------

  // Load [[BoundArguments]] into r4 and length of that into r6.
  Label no_bound_arguments;
  __ LoadTaggedField(
      r4, FieldMemOperand(r3, JSBoundFunction::kBoundArgumentsOffset));
  __ SmiUntagField(r6, FieldMemOperand(r4, FixedArray::kLengthOffset));
  __ LoadAndTestP(r6, r6);
  __ beq(&no_bound_arguments);
  {
    // ----------- S t a t e -------------
    //  -- r2 : the number of arguments
    //  -- r3 : target (checked to be a JSBoundFunction)
    //  -- r4 : the [[BoundArguments]] (implemented as FixedArray)
    //  -- r5 : new.target (only in case of [[Construct]])
    //  -- r6 : the number of [[BoundArguments]]
    // -----------------------------------

    Register scratch = r8;
    // Reserve stack space for the [[BoundArguments]].
    {
      Label done;
      __ ShiftLeftU64(r9, r6, Operand(kSystemPointerSizeLog2));
      __ SubS64(r1, sp, r9);
      // Check the stack for overflow. We are not trying to catch interruptions
      // (i.e. debug break and preemption) here, so check the "real stack
      // limit".
      __ CmpU64(r1, __ StackLimitAsMemOperand(StackLimitKind::kRealStackLimit));
      __ bgt(&done);  // Signed comparison.
      // Restore the stack pointer.
      {
        FrameScope scope(masm, StackFrame::MANUAL);
        __ EnterFrame(StackFrame::INTERNAL);
        __ CallRuntime(Runtime::kThrowStackOverflow);
      }
      __ bind(&done);
    }

    // Pop receiver.
    __ Pop(r7);

    // Push [[BoundArguments]].
    {
      Label loop, done;
      __ AddS64(r2, r2, r6);  // Adjust effective number of arguments.
      __ AddS64(r4, r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));

      __ bind(&loop);
      __ SubS64(r1, r6, Operand(1));
      __ ShiftLeftU64(r1, r1, Operand(kTaggedSizeLog2));
      __ LoadTaggedField(scratch, MemOperand(r4, r1), r0);
      __ Push(scratch);
      __ SubS64(r6, r6, Operand(1));
      __ bgt(&loop);
      __ bind(&done);
    }

    // Push receiver.
    __ Push(r7);
  }
  __ bind(&no_bound_arguments);
}

}  // namespace

// static
void Builtins::Generate_CallBoundFunctionImpl(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : the function to call (checked to be a JSBoundFunction)
  // -----------------------------------
  __ AssertBoundFunction(r3);

  // Patch the receiver to [[BoundThis]].
  __ LoadTaggedField(r5,
                     FieldMemOperand(r3, JSBoundFunction::kBoundThisOffset));
  __ StoreReceiver(r5, r2, r1);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Call the [[BoundTargetFunction]] via the Call builtin.
  __ LoadTaggedField(
      r3, FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Call_ReceiverIsAny),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : the target to call (can be any Object).
  // -----------------------------------
  Register argc = r2;
  Register target = r3;
  Register map = r6;
  Register instance_type = r7;
  DCHECK(!AreAliased(argc, target, map, instance_type));

  Label non_callable, class_constructor;
  __ JumpIfSmi(target, &non_callable);
  __ LoadMap(map, target);
  __ CompareInstanceTypeRange(map, instance_type,
                              FIRST_CALLABLE_JS_FUNCTION_TYPE,
                              LAST_CALLABLE_JS_FUNCTION_TYPE);
  __ Jump(masm->isolate()->builtins()->CallFunction(mode),
          RelocInfo::CODE_TARGET, le);
  __ CmpS64(instance_type, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Check if target has a [[Call]] internal method.
  {
    Register flags = r6;
    __ LoadU8(flags, FieldMemOperand(map, Map::kBitFieldOffset));
    map = no_reg;
    __ TestBit(flags, Map::Bits1::IsCallableBit::kShift);
    __ beq(&non_callable);
  }

  // Check if target is a proxy and call CallProxy external builtin
  __ CmpS64(instance_type, Operand(JS_PROXY_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallProxy), RelocInfo::CODE_TARGET, eq);

  // Check if target is a wrapped function and call CallWrappedFunction external
  // builtin
  __ CmpS64(instance_type, Operand(JS_WRAPPED_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), CallWrappedFunction),
          RelocInfo::CODE_TARGET, eq);

  // ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList)
  // Check that the function is not a "classConstructor".
  __ CmpS64(instance_type, Operand(JS_CLASS_CONSTRUCTOR_TYPE));
  __ beq(&class_constructor);

  // 2. Call to something else, which might have a [[Call]] internal method (if
  // not we raise an exception).
  // Overwrite the original receiver the (original) target.
  __ StoreReceiver(target, argc, r7);
  // Let the "call_as_function_delegate" take care of the rest.
  __ LoadNativeContextSlot(target, Context::CALL_AS_FUNCTION_DELEGATE_INDEX);
  __ Jump(masm->isolate()->builtins()->CallFunction(
              ConvertReceiverMode::kNotNullOrUndefined),
          RelocInfo::CODE_TARGET);

  // 3. Call to something that is not callable.
  __ bind(&non_callable);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(target);
    __ CallRuntime(Runtime::kThrowCalledNonCallable);
    __ Trap();  // Unreachable.
  }

  // 4. The function is a "classConstructor", need to raise an exception.
  __ bind(&class_constructor);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(target);
    __ CallRuntime(Runtime::kThrowConstructorNonCallableError);
    __ Trap();  // Unreachable.
  }
}

// static
void Builtins::Generate_ConstructFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : the constructor to call (checked to be a JSFunction)
  //  -- r5 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(r3, r1);
  __ AssertFunction(r3);

  // Calling convention for function specific ConstructStubs require
  // r4 to contain either an AllocationSite or undefined.
  __ LoadRoot(r4, RootIndex::kUndefinedValue);

  Label call_generic_stub;

  // Jump to JSBuiltinsConstructStub or JSConstructStubGeneric.
  __ LoadTaggedField(
      r6, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadU32(r6, FieldMemOperand(r6, SharedFunctionInfo::kFlagsOffset));
  __ AndP(r6, Operand(SharedFunctionInfo::ConstructAsBuiltinBit::kMask));
  __ beq(&call_generic_stub);

  __ Jump(BUILTIN_CODE(masm->isolate(), JSBuiltinsConstructStub),
          RelocInfo::CODE_TARGET);

  __ bind(&call_generic_stub);
  __ Jump(BUILTIN_CODE(masm->isolate(), JSConstructStubGeneric),
          RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_ConstructBoundFunction(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : the function to call (checked to be a JSBoundFunction)
  //  -- r5 : the new target (checked to be a constructor)
  // -----------------------------------
  __ AssertConstructor(r3, r1);
  __ AssertBoundFunction(r3);

  // Push the [[BoundArguments]] onto the stack.
  Generate_PushBoundArguments(masm);

  // Patch new.target to [[BoundTargetFunction]] if new.target equals target.
  Label skip;
  __ CompareTagged(r3, r5);
  __ bne(&skip);
  __ LoadTaggedField(
      r5, FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ bind(&skip);

  // Construct the [[BoundTargetFunction]] via the Construct builtin.
  __ LoadTaggedField(
      r3, FieldMemOperand(r3, JSBoundFunction::kBoundTargetFunctionOffset));
  __ Jump(BUILTIN_CODE(masm->isolate(), Construct), RelocInfo::CODE_TARGET);
}

// static
void Builtins::Generate_Construct(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : the number of arguments
  //  -- r3 : the constructor to call (can be any Object)
  //  -- r5 : the new target (either the same as the constructor or
  //          the JSFunction on which new was invoked initially)
  // -----------------------------------
  Register argc = r2;
  Register target = r3;
  Register map = r6;
  Register instance_type = r7;
  DCHECK(!AreAliased(argc, target, map, instance_type));

  // Check if target is a Smi.
  Label non_constructor, non_proxy;
  __ JumpIfSmi(target, &non_constructor);

  // Check if target has a [[Construct]] internal method.
  __ LoadTaggedField(map, FieldMemOperand(target, HeapObject::kMapOffset));
  {
    Register flags = r4;
    DCHECK(!AreAliased(argc, target, map, instance_type, flags));
    __ LoadU8(flags, FieldMemOperand(map, Map::kBitFieldOffset));
    __ TestBit(flags, Map::Bits1::IsConstructorBit::kShift);
    __ beq(&non_constructor);
  }

  // Dispatch based on instance type.
  __ CompareInstanceTypeRange(map, instance_type, FIRST_JS_FUNCTION_TYPE,
                              LAST_JS_FUNCTION_TYPE);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructFunction),
          RelocInfo::CODE_TARGET, le);

  // Only dispatch to bound functions after checking whether they are
  // constructors.
  __ CmpS64(instance_type, Operand(JS_BOUND_FUNCTION_TYPE));
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructBoundFunction),
          RelocInfo::CODE_TARGET, eq);

  // Only dispatch to proxies after checking whether they are constructors.
  __ CmpS64(instance_type, Operand(JS_PROXY_TYPE));
  __ bne(&non_proxy);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructProxy),
          RelocInfo::CODE_TARGET);

  // Called Construct on an exotic Object with a [[Construct]] internal method.
  __ bind(&non_proxy);
  {
    // Overwrite the original receiver with the (original) target.
    __ StoreReceiver(target, argc, r7);
    // Let the "call_as_constructor_delegate" take care of the rest.
    __ LoadNativeContextSlot(target,
                             Context::CALL_AS_CONSTRUCTOR_DELEGATE_INDEX);
    __ Jump(masm->isolate()->builtins()->CallFunction(),
            RelocInfo::CODE_TARGET);
  }

  // Called Construct on an Object that doesn't have a [[Construct]] internal
  // method.
  __ bind(&non_constructor);
  __ Jump(BUILTIN_CODE(masm->isolate(), ConstructedNonConstructable),
          RelocInfo::CODE_TARGET);
}

#if V8_ENABLE_WEBASSEMBLY

struct SaveWasmParamsScope {
  explicit SaveWasmParamsScope(MacroAssembler* masm) : masm(masm) {
    for (Register gp_param_reg : wasm::kGpParamRegisters) {
      gp_regs.set(gp_param_reg);
    }
    for (DoubleRegister fp_param_reg : wasm::kFpParamRegisters) {
      fp_regs.set(fp_param_reg);
    }

    CHECK_EQ(gp_regs.Count(), arraysize(wasm::kGpParamRegisters));
    CHECK_EQ(fp_regs.Count(), arraysize(wasm::kFpParamRegisters));
    CHECK_EQ(WasmLiftoffSetupFrameConstants::kNumberOfSavedGpParamRegs + 1,
             gp_regs.Count());
    CHECK_EQ(WasmLiftoffSetupFrameConstants::kNumberOfSavedFpParamRegs,
             fp_regs.Count());

    __ MultiPush(gp_regs);
    __ MultiPushF64OrV128(fp_regs, r1);
  }
  ~SaveWasmParamsScope() {
    __ MultiPopF64OrV128(fp_regs, r1);
    __ MultiPop(gp_regs);
  }

  RegList gp_regs;
  DoubleRegList fp_regs;
  MacroAssembler* masm;
};

void Builtins::Generate_WasmLiftoffFrameSetup(MacroAssembler* masm) {
  Register func_index = wasm::kLiftoffFrameSetupFunctionReg;
  Register vector = ip;
  Register scratch = r0;
  Label allocate_vector, done;

  __ LoadTaggedField(
      vector, FieldMemOperand(kWasmInstanceRegister,
                              WasmInstanceObject::kFeedbackVectorsOffset));
  __ ShiftLeftU64(scratch, func_index, Operand(kTaggedSizeLog2));
  __ AddS64(vector, vector, scratch);
  __ LoadTaggedField(vector, FieldMemOperand(vector, FixedArray::kHeaderSize));
  __ JumpIfSmi(vector, &allocate_vector);
  __ bind(&done);
  __ push(kWasmInstanceRegister);
  __ push(vector);
  __ Ret();

  __ bind(&allocate_vector);

  // Feedback vector doesn't exist yet. Call the runtime to allocate it.
  // We temporarily change the frame type for this, because we need special
  // handling by the stack walker in case of GC.
  __ mov(scratch,
         Operand(StackFrame::TypeToMarker(StackFrame::WASM_LIFTOFF_SETUP)));
  __ StoreU64(scratch, MemOperand(sp));

  // Save current return address as it will get clobbered during CallRuntime.
  __ push(r14);
  {
    SaveWasmParamsScope save_params(masm);
    // Arguments to the runtime function: instance, func_index.
    __ push(kWasmInstanceRegister);
    __ SmiTag(func_index);
    __ push(func_index);
    // Allocate a stack slot where the runtime function can spill a pointer
    // to the {NativeModule}.
    __ push(r10);
    __ LoadSmiLiteral(cp, Smi::zero());
    __ CallRuntime(Runtime::kWasmAllocateFeedbackVector, 3);
    __ mov(vector, kReturnRegister0);
    // Saved parameters are restored at the end of this block.
  }
  __ pop(r14);

  __ mov(scratch, Operand(StackFrame::TypeToMarker(StackFrame::WASM)));
  __ StoreU64(scratch, MemOperand(sp));
  __ b(&done);
}

void Builtins::Generate_WasmCompileLazy(MacroAssembler* masm) {
  // The function index was put in a register by the jump table trampoline.
  // Convert to Smi for the runtime call.
  __ SmiTag(kWasmCompileLazyFuncIndexRegister);

  {
    HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    {
      SaveWasmParamsScope save_params(masm);

      // Push the Wasm instance as an explicit argument to the runtime function.
      __ push(kWasmInstanceRegister);
      // Push the function index as second argument.
      __ push(kWasmCompileLazyFuncIndexRegister);
      // Initialize the JavaScript context with 0. CEntry will use it to
      // set the current context on the isolate.
      __ LoadSmiLiteral(cp, Smi::zero());
      __ CallRuntime(Runtime::kWasmCompileLazy, 2);
      // The runtime function returns the jump table slot offset as a Smi. Use
      // that to compute the jump target in ip.
      __ SmiUntag(kReturnRegister0);
      __ mov(ip, kReturnRegister0);

      // Saved parameters are restored at the end of this block.
    }

    // After the instance register has been restored, we can add the jump table
    // start to the jump table offset already stored in r8.
    __ LoadU64(r0, FieldMemOperand(kWasmInstanceRegister,
                                   WasmInstanceObject::kJumpTableStartOffset));
    __ AddS64(ip, ip, r0);
  }

  // Finally, jump to the jump table slot for the function.
  __ Jump(ip);
}

void Builtins::Generate_WasmDebugBreak(MacroAssembler* masm) {
  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::WASM_DEBUG_BREAK);

    // Save all parameter registers. They might hold live values, we restore
    // them after the runtime call.
    __ MultiPush(WasmDebugBreakFrameConstants::kPushedGpRegs);
    __ MultiPushF64OrV128(WasmDebugBreakFrameConstants::kPushedFpRegs, ip);

    // Initialize the JavaScript context with 0. CEntry will use it to
    // set the current context on the isolate.
    __ LoadSmiLiteral(cp, Smi::zero());
    __ CallRuntime(Runtime::kWasmDebugBreak, 0);

    // Restore registers.
    __ MultiPopF64OrV128(WasmDebugBreakFrameConstants::kPushedFpRegs, ip);
    __ MultiPop(WasmDebugBreakFrameConstants::kPushedGpRegs);
  }
  __ Ret();
}

void Builtins::Generate_GenericJSToWasmWrapper(MacroAssembler* masm) {
  // TODO(v8:10701): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmReturnPromiseOnSuspend(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmSuspend(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmResume(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmReject(MacroAssembler* masm) {
  // TODO(v8:12191): Implement for this platform.
  __ Trap();
}

void Builtins::Generate_WasmOnStackReplace(MacroAssembler* masm) {
  // Only needed on x64.
  __ Trap();
}
#endif  // V8_ENABLE_WEBASSEMBLY

void Builtins::Generate_CEntry(MacroAssembler* masm, int result_size,
                               ArgvMode argv_mode, bool builtin_exit_frame) {
  // Called from JavaScript; parameters are on stack as if calling JS function.
  // r2: number of arguments including receiver
  // r3: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)
  //
  // If argv_mode == ArgvMode::kRegister:
  // r4: pointer to the first argument

  __ mov(r7, r3);

  if (argv_mode == ArgvMode::kRegister) {
    // Move argv into the correct register.
    __ mov(r3, r4);
  } else {
    // Compute the argv pointer.
    __ ShiftLeftU64(r3, r2, Operand(kSystemPointerSizeLog2));
    __ lay(r3, MemOperand(r3, sp, -kSystemPointerSize));
  }

  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);

  // Need at least one extra slot for return address location.
  int arg_stack_space = 1;

  // Pass buffer for return value on stack if necessary
  bool needs_return_buffer =
      result_size == 2 && !ABI_RETURNS_OBJECTPAIR_IN_REGS;
  if (needs_return_buffer) {
    arg_stack_space += result_size;
  }

#if V8_TARGET_ARCH_S390X
  // 64-bit linux pass Argument object by reference not value
  arg_stack_space += 2;
#endif

  __ EnterExitFrame(arg_stack_space, builtin_exit_frame
                                         ? StackFrame::BUILTIN_EXIT
                                         : StackFrame::EXIT);

  // Store a copy of argc, argv in callee-saved registers for later.
  __ mov(r6, r2);
  __ mov(r8, r3);
  // r2, r6: number of arguments including receiver  (C callee-saved)
  // r3, r8: pointer to the first argument
  // r7: pointer to builtin function  (C callee-saved)

  // Result returned in registers or stack, depending on result size and ABI.

  Register isolate_reg = r4;
  if (needs_return_buffer) {
    // The return value is 16-byte non-scalar value.
    // Use frame storage reserved by calling function to pass return
    // buffer as implicit first argument in R2.  Shfit original parameters
    // by one register each.
    __ mov(r4, r3);
    __ mov(r3, r2);
    __ la(r2,
          MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kSystemPointerSize));
    isolate_reg = r5;
    // Clang doesn't preserve r2 (result buffer)
    // write to r8 (preserved) before entry
    __ mov(r8, r2);
  }
  // Call C built-in.
  __ Move(isolate_reg, ExternalReference::isolate_address(masm->isolate()));

  __ StoreReturnAddressAndCall(r7);

  // If return value is on the stack, pop it to registers.
  if (needs_return_buffer) {
    __ mov(r2, r8);
    __ LoadU64(r3, MemOperand(r2, kSystemPointerSize));
    __ LoadU64(r2, MemOperand(r2));
  }

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r2, RootIndex::kException);
  __ beq(&exception_returned, Label::kNear);

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (v8_flags.debug_code) {
    Label okay;
    ExternalReference pending_exception_address = ExternalReference::Create(
        IsolateAddressId::kPendingExceptionAddress, masm->isolate());
    __ Move(r1, pending_exception_address);
    __ LoadU64(r1, MemOperand(r1));
    __ CompareRoot(r1, RootIndex::kTheHoleValue);
    // Cannot use check here as it attempts to generate call into runtime.
    __ beq(&okay, Label::kNear);
    __ stop();
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r2:r3: result
  // sp: stack pointer
  // fp: frame pointer
  Register argc = argv_mode == ArgvMode::kRegister
                      // We don't want to pop arguments so set argc to no_reg.
                      ? no_reg
                      // r6: still holds argc (callee-saved).
                      : r6;
  __ LeaveExitFrame(argc, false);
  __ b(r14);

  // Handling of exception.
  __ bind(&exception_returned);

  ExternalReference pending_handler_context_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerContextAddress, masm->isolate());
  ExternalReference pending_handler_entrypoint_address =
      ExternalReference::Create(
          IsolateAddressId::kPendingHandlerEntrypointAddress, masm->isolate());
  ExternalReference pending_handler_fp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerFPAddress, masm->isolate());
  ExternalReference pending_handler_sp_address = ExternalReference::Create(
      IsolateAddressId::kPendingHandlerSPAddress, masm->isolate());

  // Ask the runtime for help to determine the handler. This will set r3 to
  // contain the current pending exception, don't clobber it.
  ExternalReference find_handler =
      ExternalReference::Create(Runtime::kUnwindAndFindExceptionHandler);
  {
    FrameScope scope(masm, StackFrame::MANUAL);
    __ PrepareCallCFunction(3, 0, r2);
    __ mov(r2, Operand::Zero());
    __ mov(r3, Operand::Zero());
    __ Move(r4, ExternalReference::isolate_address(masm->isolate()));
    __ CallCFunction(find_handler, 3);
  }

  // Retrieve the handler context, SP and FP.
  __ Move(cp, pending_handler_context_address);
  __ LoadU64(cp, MemOperand(cp));
  __ Move(sp, pending_handler_sp_address);
  __ LoadU64(sp, MemOperand(sp));
  __ Move(fp, pending_handler_fp_address);
  __ LoadU64(fp, MemOperand(fp));

  // If the handler is a JS frame, restore the context to the frame. Note that
  // the context will be set to (cp == 0) for non-JS frames.
  Label skip;
  __ CmpS64(cp, Operand::Zero());
  __ beq(&skip, Label::kNear);
  __ StoreU64(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  __ bind(&skip);

  // Clear c_entry_fp, like we do in `LeaveExitFrame`.
  {
    UseScratchRegisterScope temps(masm);
    __ Move(r1, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                          masm->isolate()));
    __ mov(r0, Operand::Zero());
    __ StoreU64(r0, MemOperand(r1));
  }

  // Compute the handler entry address and jump to it.
  __ Move(r3, pending_handler_entrypoint_address);
  __ LoadU64(r3, MemOperand(r3));
  __ Jump(r3);
}

void Builtins::Generate_DoubleToI(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done, fastpath_done;
  Register result_reg = r2;

  HardAbortScope hard_abort(masm);  // Avoid calls to Abort.

  // Immediate values for this stub fit in instructions, so it's safe to use ip.
  Register scratch = GetRegisterThatIsNotOneOf(result_reg);
  Register scratch_low = GetRegisterThatIsNotOneOf(result_reg, scratch);
  Register scratch_high =
      GetRegisterThatIsNotOneOf(result_reg, scratch, scratch_low);
  DoubleRegister double_scratch = kScratchDoubleReg;

  __ Push(result_reg, scratch);
  // Account for saved regs.
  int argument_offset = 2 * kSystemPointerSize;

  // Load double input.
  __ LoadF64(double_scratch, MemOperand(sp, argument_offset));

  // Do fast-path convert from double to int.
  __ ConvertDoubleToInt64(result_reg, double_scratch);

  // Test for overflow
  __ TestIfInt32(result_reg);
  __ beq(&fastpath_done, Label::kNear);

  __ Push(scratch_high, scratch_low);
  // Account for saved regs.
  argument_offset += 2 * kSystemPointerSize;

  __ LoadU32(scratch_high,
            MemOperand(sp, argument_offset + Register::kExponentOffset));
  __ LoadU32(scratch_low,
            MemOperand(sp, argument_offset + Register::kMantissaOffset));

  __ ExtractBitMask(scratch, scratch_high, HeapNumber::kExponentMask);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is a *S390* immediate value.
  static_assert(HeapNumber::kExponentBias + 1 == 1024);
  __ SubS64(scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83).
  __ CmpS64(scratch, Operand(83));
  __ bge(&out_of_range, Label::kNear);

  // If we reach this code, 31 <= exponent <= 83.
  // So, we don't have to handle cases where 0 <= exponent <= 20 for
  // which we would need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ mov(r0, Operand(51));
  __ SubS64(scratch, r0, scratch);
  __ CmpS64(scratch, Operand::Zero());
  __ ble(&only_low, Label::kNear);
  // 21 <= exponent <= 51, shift scratch_low and scratch_high
  // to generate the result.
  __ ShiftRightU32(scratch_low, scratch_low, scratch);
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ mov(r0, Operand(32));
  __ SubS64(scratch, r0, scratch);
  __ ExtractBitMask(result_reg, scratch_high, HeapNumber::kMantissaMask);
  // Set the implicit 1 before the mantissa part in scratch_high.
  static_assert(HeapNumber::kMantissaBitsInTopWord >= 16);
  __ mov(r0, Operand(1 << ((HeapNumber::kMantissaBitsInTopWord)-16)));
  __ ShiftLeftU64(r0, r0, Operand(16));
  __ OrP(result_reg, result_reg, r0);
  __ ShiftLeftU32(r0, result_reg, scratch);
  __ OrP(result_reg, scratch_low, r0);
  __ b(&negate, Label::kNear);

  __ bind(&out_of_range);
  __ mov(result_reg, Operand::Zero());
  __ b(&done, Label::kNear);

  __ bind(&only_low);
  // 52 <= exponent <= 83, shift only scratch_low.
  // On entry, scratch contains: 52 - exponent.
  __ lcgr(scratch, scratch);
  __ ShiftLeftU32(result_reg, scratch_low, scratch);

  __ bind(&negate);
  // If input was positive, scratch_high ASR 31 equals 0 and
  // scratch_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xFFFFFFFF and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xFFFFFFFF) + 1 = 0 - result.
  __ ShiftRightS32(r0, scratch_high, Operand(31));
#if V8_TARGET_ARCH_S390X
  __ lgfr(r0, r0);
  __ ShiftRightU64(r0, r0, Operand(32));
#endif
  __ XorP(result_reg, r0);
  __ ShiftRightU32(r0, scratch_high, Operand(31));
  __ AddS64(result_reg, r0);

  __ bind(&done);
  __ Pop(scratch_high, scratch_low);
  argument_offset -= 2 * kSystemPointerSize;

  __ bind(&fastpath_done);
  __ StoreU64(result_reg, MemOperand(sp, argument_offset));
  __ Pop(result_reg, scratch);

  __ Ret();
}

namespace {

static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}

// Calls an API function.  Allocates HandleScope, extracts returned value
// from handle and propagates exceptions.  Restores context.  stack_space
// - space to be unwound on exit (includes the call JS arguments space and
// the additional space allocated for the fast call).
static void CallApiFunctionAndReturn(MacroAssembler* masm,
                                     Register function_address,
                                     ExternalReference thunk_ref,
                                     int stack_space,
                                     MemOperand* stack_space_operand,
                                     MemOperand return_value_operand) {
  Isolate* isolate = masm->isolate();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate);
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(isolate), next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(isolate), next_address);

  // Additional parameter is the address of the actual callback.
  DCHECK(function_address == r3 || function_address == r4);
  Register scratch = r5;

  // Allocate HandleScope in callee-save registers.
  // r9 - next_address
  // r6 - next_address->kNextOffset
  // r7 - next_address->kLimitOffset
  // r8 - next_address->kLevelOffset
  __ Move(r9, next_address);
  __ LoadU64(r6, MemOperand(r9, kNextOffset));
  __ LoadU64(r7, MemOperand(r9, kLimitOffset));
  __ LoadU32(r8, MemOperand(r9, kLevelOffset));
  __ AddS64(r8, Operand(1));
  __ StoreU32(r8, MemOperand(r9, kLevelOffset));

  Label profiler_enabled, done_api_call;
  __ LoadU8(scratch,
            __ ExternalReferenceAsOperand(
                ExternalReference::is_profiling_address(isolate), scratch));
  __ CmpS64(scratch, Operand::Zero());
  __ bne(&profiler_enabled, Label::kNear);
#ifdef V8_RUNTIME_CALL_STATS
  __ Move(scratch, ExternalReference::address_of_runtime_stats_flag());
  __ LoadU32(scratch, MemOperand(scratch, 0));
  __ CmpS64(scratch, Operand::Zero());
  __ bne(&profiler_enabled, Label::kNear);
#endif  // V8_RUNTIME_CALL_STATS

  // Call the api function directly.
  __ StoreReturnAddressAndCall(function_address);
  __ bind(&done_api_call);

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  __ LoadU64(r2, return_value_operand);
  __ bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  __ StoreU64(r6, MemOperand(r9, kNextOffset));
  if (v8_flags.debug_code) {
    __ LoadU32(r3, MemOperand(r9, kLevelOffset));
    __ CmpS64(r3, r8);
    __ Check(eq, AbortReason::kUnexpectedLevelAfterReturnFromApiCall);
  }
  __ SubS64(r8, Operand(1));
  __ StoreU32(r8, MemOperand(r9, kLevelOffset));
  __ CmpS64(r7, MemOperand(r9, kLimitOffset));
  __ bne(&delete_allocated_handles, Label::kNear);

  // Leave the API exit frame.
  __ bind(&leave_exit_frame);
  // LeaveExitFrame expects unwind space to be in a register.
  if (stack_space_operand == nullptr) {
    DCHECK_NE(stack_space, 0);
    __ mov(r6, Operand(stack_space));
  } else {
    DCHECK_EQ(stack_space, 0);
    __ LoadU64(r6, *stack_space_operand);
  }
  __ LeaveExitFrame(r6, stack_space_operand != nullptr);

  // Check if the function scheduled an exception.
  __ Move(r7, ExternalReference::scheduled_exception_address(isolate));
  __ LoadU64(r7, MemOperand(r7));
  __ CompareRoot(r7, RootIndex::kTheHoleValue);
  __ bne(&promote_scheduled_exception, Label::kNear);

  __ b(r14);

  // Call the api function via thunk wrapper.
  __ bind(&profiler_enabled);
  // Additional parameter is the address of the actual callback.
  __ Move(r5, function_address);
  __ Move(ip, thunk_ref);
  __ StoreReturnAddressAndCall(ip);
  __ b(&done_api_call);

  // Re-throw by promoting a scheduled exception.
  __ bind(&promote_scheduled_exception);
  __ TailCallRuntime(Runtime::kPromoteScheduledException);

  // HandleScope limit has changed. Delete allocated extensions.
  __ bind(&delete_allocated_handles);
  __ StoreU64(r7, MemOperand(r9, kLimitOffset));
  __ mov(r6, r2);
  __ PrepareCallCFunction(1, r7);
  __ Move(r2, ExternalReference::isolate_address(isolate));
  __ CallCFunction(ExternalReference::delete_handle_scope_extensions(), 1);
  __ mov(r2, r6);
  __ b(&leave_exit_frame, Label::kNear);
}

MemOperand ExitFrameStackSlotOperand(int offset) {
  static constexpr int kFrameOffset = 1 * kSystemPointerSize;
  return MemOperand(sp, (kStackFrameExtraParamSlot * kSystemPointerSize) +
                            offset + kFrameOffset);
}

MemOperand ExitFrameCallerStackSlotOperand(int index) {
  return MemOperand(
      fp, (BuiltinExitFrameConstants::kFixedSlotCountAboveFp + index) *
              kSystemPointerSize);
}

}  // namespace

void Builtins::Generate_CallApiCallback(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- cp                  : context
  //  -- r4                  : api function address
  //  -- r4                  : arguments count (not including the receiver)
  //  -- r5                  : call data
  //  -- r2                  : holder
  //  -- sp[0]               : receiver
  //  -- sp[8]               : first argument
  //  -- ...
  //  -- sp[(argc) * 8]      : last argument
  // -----------------------------------

  Register api_function_address = r3;
  Register argc = r4;
  Register call_data = r5;
  Register holder = r2;
  Register scratch = r6;
  DCHECK(!AreAliased(api_function_address, argc, call_data, holder, scratch));

  using FCI = FunctionCallbackInfo<v8::Value>;
  using FCA = FunctionCallbackArguments;

  static_assert(FCA::kArgsLength == 6);
  static_assert(FCA::kNewTargetIndex == 5);
  static_assert(FCA::kDataIndex == 4);
  static_assert(FCA::kReturnValueIndex == 3);
  static_assert(FCA::kReturnValueDefaultValueIndex == 2);
  static_assert(FCA::kIsolateIndex == 1);
  static_assert(FCA::kHolderIndex == 0);

  // Set up FunctionCallbackInfo's implicit_args on the stack as follows:
  //
  // Target state:
  //   sp[0 * kSystemPointerSize]: kHolder   <= FCI::implicit_args_
  //   sp[1 * kSystemPointerSize]: kIsolate
  //   sp[2 * kSystemPointerSize]: undefined (kReturnValueDefaultValue)
  //   sp[3 * kSystemPointerSize]: undefined (kReturnValue)
  //   sp[4 * kSystemPointerSize]: kData
  //   sp[5 * kSystemPointerSize]: undefined (kNewTarget)
  // Existing state:
  //   sp[6 * kSystemPointerSize]:            <= FCI:::values_

  // Reserve space on the stack.
  __ lay(sp, MemOperand(sp, -(FCA::kArgsLength * kSystemPointerSize)));

  // kHolder.
  __ StoreU64(holder, MemOperand(sp, 0 * kSystemPointerSize));

  // kIsolate.
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ StoreU64(scratch, MemOperand(sp, 1 * kSystemPointerSize));

  // kReturnValueDefaultValue and kReturnValue.
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ StoreU64(scratch, MemOperand(sp, 2 * kSystemPointerSize));
  __ StoreU64(scratch, MemOperand(sp, 3 * kSystemPointerSize));

  // kData.
  __ StoreU64(call_data, MemOperand(sp, 4 * kSystemPointerSize));

  // kNewTarget.
  __ StoreU64(scratch, MemOperand(sp, 5 * kSystemPointerSize));

  // Keep a pointer to kHolder (= implicit_args) in a scratch register.
  // We use it below to set up the FunctionCallbackInfo object.
  __ mov(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // S390 LINUX ABI:
  //
  // Create 4 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-3] FunctionCallbackInfo
  //    [4] number of bytes to drop from the stack after returning
  static constexpr int kSlotsToDropSize = 1 * kSystemPointerSize;
  static constexpr int kApiStackSpace = 5;
  static_assert(FCI::kImplicitArgsOffset == 0);
  static_assert(FCI::kValuesOffset == 1 * kSystemPointerSize);
  static_assert(FCI::kLengthOffset == 2 * kSystemPointerSize);

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(kApiStackSpace, StackFrame::EXIT);

  // FunctionCallbackInfo::implicit_args_ (points at kHolder as set up above).
  // Arguments are after the return address (pushed by EnterExitFrame()).
  __ StoreU64(scratch, ExitFrameStackSlotOperand(FCI::kImplicitArgsOffset));

  // FunctionCallbackInfo::values_ (points at the first varargs argument passed
  // on the stack).
  __ AddS64(scratch, scratch,
            Operand(FCA::kArgsLengthWithReceiver * kSystemPointerSize));
  __ StoreU64(scratch, ExitFrameStackSlotOperand(FCI::kValuesOffset));

  // FunctionCallbackInfo::length_.
  __ StoreU32(argc, ExitFrameStackSlotOperand(FCI::kLengthOffset));

  // We also store the number of bytes to drop from the stack after returning
  // from the API function here.
  MemOperand stack_space_operand =
      ExitFrameStackSlotOperand(FCI::kLengthOffset + kSlotsToDropSize);
  __ mov(scratch,
         Operand((FCA::kArgsLength + 1 /* receiver */) * kSystemPointerSize));
  __ ShiftLeftU64(r1, argc, Operand(kSystemPointerSizeLog2));
  __ AddS64(scratch, r1);
  __ StoreU64(scratch, stack_space_operand);

  // v8::InvocationCallback's argument.
  __ lay(r2,
         MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kSystemPointerSize));

  ExternalReference thunk_ref = ExternalReference::invoke_function_callback();
  MemOperand return_value_operand =
      ExitFrameCallerStackSlotOperand(FCA::kReturnValueIndex);
  static constexpr int kUseStackSpaceOperand = 0;

  AllowExternalCallThatCantCauseGC scope(masm);
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kUseStackSpaceOperand, &stack_space_operand,
                           return_value_operand);
}

void Builtins::Generate_CallApiGetter(MacroAssembler* masm) {
  int arg0Slot = 0;
  int accessorInfoSlot = 0;
  int apiStackSpace = 0;
  // Build v8::PropertyCallbackInfo::args_ array on the stack and push property
  // name below the exit frame to make GC aware of them.
  using PCA = PropertyCallbackArguments;
  static_assert(PCA::kShouldThrowOnErrorIndex == 0);
  static_assert(PCA::kHolderIndex == 1);
  static_assert(PCA::kIsolateIndex == 2);
  static_assert(PCA::kReturnValueDefaultValueIndex == 3);
  static_assert(PCA::kReturnValueIndex == 4);
  static_assert(PCA::kDataIndex == 5);
  static_assert(PCA::kThisIndex == 6);
  static_assert(PCA::kArgsLength == 7);

  Register receiver = ApiGetterDescriptor::ReceiverRegister();
  Register holder = ApiGetterDescriptor::HolderRegister();
  Register callback = ApiGetterDescriptor::CallbackRegister();
  Register scratch = r6;
  DCHECK(!AreAliased(receiver, holder, callback, scratch));

  Register api_function_address = r4;

  __ push(receiver);
  // Push data from AccessorInfo.
  __ LoadTaggedField(scratch,
                     FieldMemOperand(callback, AccessorInfo::kDataOffset), r1);
  __ push(scratch);
  __ LoadRoot(scratch, RootIndex::kUndefinedValue);
  __ Push(scratch, scratch);
  __ Move(scratch, ExternalReference::isolate_address(masm->isolate()));
  __ Push(scratch, holder);
  __ Push(Smi::zero());  // should_throw_on_error -> false
  __ LoadTaggedField(scratch,
                     FieldMemOperand(callback, AccessorInfo::kNameOffset), r1);
  __ push(scratch);

  // v8::PropertyCallbackInfo::args_ array and name handle.
  constexpr int kNameHandleStackSize = 1;
  constexpr int kStackUnwindSpace = PCA::kArgsLength + kNameHandleStackSize;

  // Load address of v8::PropertyAccessorInfo::args_ array and name handle.
  __ mov(r2, sp);                                    // r2 = Handle<Name>
  __ AddS64(r3, r2, Operand(1 * kSystemPointerSize));  // r3 = v8::PCI::args_

  // If ABI passes Handles (pointer-sized struct) in a register:
  //
  // Create 2 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] AccessorInfo&
  //
  // Otherwise:
  //
  // Create 3 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] copy of Handle (first arg)
  //    [2] AccessorInfo&
  if (ABI_PASSES_HANDLES_IN_REGS) {
    accessorInfoSlot = kStackFrameExtraParamSlot + 1;
    apiStackSpace = 2;
  } else {
    arg0Slot = kStackFrameExtraParamSlot + 1;
    accessorInfoSlot = arg0Slot + 1;
    apiStackSpace = 3;
  }

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(apiStackSpace, StackFrame::EXIT);

  if (!ABI_PASSES_HANDLES_IN_REGS) {
    // pass 1st arg by reference
    __ StoreU64(r2, MemOperand(sp, arg0Slot * kSystemPointerSize));
    __ AddS64(r2, sp, Operand(arg0Slot * kSystemPointerSize));
  }

  // Create v8::PropertyCallbackInfo object on the stack and initialize
  // it's args_ field.
  __ StoreU64(r3, MemOperand(sp, accessorInfoSlot * kSystemPointerSize));
  __ AddS64(r3, sp, Operand(accessorInfoSlot * kSystemPointerSize));
  // r3 = v8::PropertyCallbackInfo&

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback();
  __ LoadU64(
      api_function_address,
      FieldMemOperand(callback, AccessorInfo::kMaybeRedirectedGetterOffset));
  MemOperand return_value_operand = ExitFrameCallerStackSlotOperand(
      PCA::kReturnValueIndex + kNameHandleStackSize);
  MemOperand* const kUseStackSpaceConstant = nullptr;
  CallApiFunctionAndReturn(masm, api_function_address, thunk_ref,
                           kStackUnwindSpace, kUseStackSpaceConstant,
                           return_value_operand);
}

void Builtins::Generate_DirectCEntry(MacroAssembler* masm) {
  // Unused.
  __ stop();
}

namespace {

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Generate_DeoptimizationEntry(MacroAssembler* masm,
                                  DeoptimizeKind deopt_kind) {
  Isolate* isolate = masm->isolate();

  // Save all the registers onto the stack
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;

  // Save all double registers before messing with them.
  __ lay(sp, MemOperand(sp, -kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ StoreF64(dreg, MemOperand(sp, offset));
  }

  // Push all GPRs onto the stack
  __ lay(sp, MemOperand(sp, -kNumberOfRegisters * kSystemPointerSize));
  __ StoreMultipleP(r0, sp, MemOperand(sp));  // Save all 16 registers

  __ Move(r1, ExternalReference::Create(IsolateAddressId::kCEntryFPAddress,
                                        isolate));
  __ StoreU64(fp, MemOperand(r1));

  static constexpr int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kSystemPointerSize) + kDoubleRegsSize;

  // Cleanse the Return address for 31-bit
  __ CleanseP(r14);
  // Get the address of the location in the code object (r5)(return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r6.
  __ mov(r4, r14);
  __ la(r5, MemOperand(sp, kSavedRegistersAreaSize));
  __ SubS64(r5, fp, r5);

  // Allocate a new deoptimizer object.
  // Pass six arguments in r2 to r7.
  __ PrepareCallCFunction(5, r7);
  __ mov(r2, Operand::Zero());
  Label context_check;
  __ LoadU64(r3,
             MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(r3, &context_check);
  __ LoadU64(r2, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ mov(r3, Operand(static_cast<int>(deopt_kind)));
  // r4: code address or 0 already loaded.
  // r5: Fp-to-sp delta already loaded.
  // Parm6: isolate is passed on the stack.
  __ Move(r6, ExternalReference::isolate_address(isolate));
  __ StoreU64(r6,
              MemOperand(sp, kStackFrameExtraParamSlot * kSystemPointerSize));

  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 5);
  }

  // Preserve "deoptimizer" object in register r2 and get the input
  // frame descriptor pointer to r3 (deoptimizer->input_);
  __ LoadU64(r3, MemOperand(r2, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  // DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  // __ mvc(MemOperand(r3, FrameDescription::registers_offset()),
  //        MemOperand(sp), kNumberOfRegisters * kSystemPointerSize);
  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  // TODO(john.yan): optimize the following code by using mvc instruction
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    __ LoadU64(r4, MemOperand(sp, i * kSystemPointerSize));
    __ StoreU64(r4, MemOperand(r3, offset));
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy double registers to
  // double_registers_[DoubleRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kSystemPointerSize;
    // TODO(joransiu): MVC opportunity
    __ LoadF64(d0, MemOperand(sp, src_offset));
    __ StoreF64(d0, MemOperand(r3, dst_offset));
  }

  // Mark the stack as not iterable for the CPU profiler which won't be able to
  // walk the stack without the return address.
  {
    UseScratchRegisterScope temps(masm);
    Register is_iterable = temps.Acquire();
    Register zero = r6;
    __ Move(is_iterable, ExternalReference::stack_is_iterable_address(isolate));
    __ lhi(zero, Operand(0));
    __ StoreU8(zero, MemOperand(is_iterable));
  }

  // Remove the saved registers from the stack.
  __ la(sp, MemOperand(sp, kSavedRegistersAreaSize));

  // Compute a pointer to the unwinding limit in register r4; that is
  // the first stack slot not part of the input frame.
  __ LoadU64(r4, MemOperand(r3, FrameDescription::frame_size_offset()));
  __ AddS64(r4, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ la(r5, MemOperand(r3, FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ b(&pop_loop_header, Label::kNear);
  __ bind(&pop_loop);
  __ pop(r6);
  __ StoreU64(r6, MemOperand(r5, 0));
  __ la(r5, MemOperand(r5, kSystemPointerSize));
  __ bind(&pop_loop_header);
  __ CmpS64(r4, sp);
  __ bne(&pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r2);  // Preserve deoptimizer object across call.
  // r2: deoptimizer object; r3: scratch.
  __ PrepareCallCFunction(1, r3);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(r2);  // Restore deoptimizer object (class Deoptimizer).

  __ LoadU64(sp, MemOperand(r2, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: r6 = current "FrameDescription** output_",
  // r3 = one past the last FrameDescription**.
  __ LoadU32(r3, MemOperand(r2, Deoptimizer::output_count_offset()));
  __ LoadU64(r6,
             MemOperand(r2, Deoptimizer::output_offset()));  // r6 is output_.
  __ ShiftLeftU64(r3, r3, Operand(kSystemPointerSizeLog2));
  __ AddS64(r3, r6, r3);
  __ b(&outer_loop_header, Label::kNear);

  __ bind(&outer_push_loop);
  // Inner loop state: r4 = current FrameDescription*, r5 = loop index.
  __ LoadU64(r4, MemOperand(r6, 0));  // output_[ix]
  __ LoadU64(r5, MemOperand(r4, FrameDescription::frame_size_offset()));
  __ b(&inner_loop_header, Label::kNear);

  __ bind(&inner_push_loop);
  __ SubS64(r5, Operand(sizeof(intptr_t)));
  __ AddS64(r8, r4, r5);
  __ LoadU64(r8, MemOperand(r8, FrameDescription::frame_content_offset()));
  __ push(r8);

  __ bind(&inner_loop_header);
  __ CmpS64(r5, Operand::Zero());
  __ bne(&inner_push_loop);  // test for gt?

  __ AddS64(r6, r6, Operand(kSystemPointerSize));
  __ bind(&outer_loop_header);
  __ CmpS64(r6, r3);
  __ blt(&outer_push_loop);

  __ LoadU64(r3, MemOperand(r2, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ ld(dreg, MemOperand(r3, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ LoadU64(r8, MemOperand(r4, FrameDescription::pc_offset()));
  __ push(r8);
  __ LoadU64(r8, MemOperand(r4, FrameDescription::continuation_offset()));
  __ push(r8);

  // Restore the registers from the last output frame.
  __ mov(r1, r4);
  for (int i = kNumberOfRegisters - 1; i > 0; i--) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs.bits() & (1 << i)) != 0) {
      __ LoadU64(ToRegister(i), MemOperand(r1, offset));
    }
  }

  {
    UseScratchRegisterScope temps(masm);
    Register is_iterable = temps.Acquire();
    Register one = r6;
    __ Move(is_iterable, ExternalReference::stack_is_iterable_address(isolate));
    __ lhi(one, Operand(1));
    __ StoreU8(one, MemOperand(is_iterable));
  }

  __ pop(ip);  // get continuation, leave pc on stack
  __ pop(r14);
  __ Jump(ip);

  __ stop();
}

}  // namespace

void Builtins::Generate_DeoptimizationEntry_Eager(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kEager);
}

void Builtins::Generate_DeoptimizationEntry_Lazy(MacroAssembler* masm) {
  Generate_DeoptimizationEntry(masm, DeoptimizeKind::kLazy);
}

void Builtins::Generate_InterpreterOnStackReplacement(MacroAssembler* masm) {
  using D = OnStackReplacementDescriptor;
  static_assert(D::kParameterCount == 1);
  OnStackReplacement(masm, OsrSourceTier::kInterpreter,
                     D::MaybeTargetCodeRegister());
}

#if ENABLE_SPARKPLUG
void Builtins::Generate_BaselineOnStackReplacement(MacroAssembler* masm) {
  using D = OnStackReplacementDescriptor;
  static_assert(D::kParameterCount == 1);

  __ LoadU64(kContextRegister,
         MemOperand(fp, BaselineFrameConstants::kContextOffset));
  OnStackReplacement(masm, OsrSourceTier::kBaseline,
                     D::MaybeTargetCodeRegister());
}
#endif

void Builtins::Generate_BaselineOrInterpreterEnterAtBytecode(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, false);
}

void Builtins::Generate_BaselineOrInterpreterEnterAtNextBytecode(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, true);
}

void Builtins::Generate_InterpreterOnStackReplacement_ToBaseline(
    MacroAssembler* masm) {
  Generate_BaselineOrInterpreterEntry(masm, false, true);
}

void Builtins::Generate_RestartFrameTrampoline(MacroAssembler* masm) {
  // Frame is being dropped:
  // - Look up current function on the frame.
  // - Leave the frame.
  // - Restart the frame by calling the function.

  __ LoadU64(r3, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ LoadU64(r2, MemOperand(fp, StandardFrameConstants::kArgCOffset));
  __ LeaveFrame(StackFrame::INTERPRETED);

  // The arguments are already in the stack (including any necessary padding),
  // we should not try to massage the arguments again.
  __ mov(r4, Operand(kDontAdaptArgumentsSentinel));
  __ InvokeFunction(r3, r4, r2, InvokeType::kJump);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390

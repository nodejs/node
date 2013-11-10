// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#if V8_TARGET_ARCH_X64

#include "bootstrapper.h"
#include "codegen.h"
#include "cpu-profiler.h"
#include "assembler-x64.h"
#include "macro-assembler-x64.h"
#include "serialize.h"
#include "debug.h"
#include "heap.h"
#include "isolate-inl.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      allow_stub_calls_(true),
      has_frame_(false),
      root_array_available_(true) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


static const int kInvalidRootRegisterDelta = -1;


intptr_t MacroAssembler::RootRegisterDelta(ExternalReference other) {
  if (predictable_code_size() &&
      (other.address() < reinterpret_cast<Address>(isolate()) ||
       other.address() >= reinterpret_cast<Address>(isolate() + 1))) {
    return kInvalidRootRegisterDelta;
  }
  Address roots_register_value = kRootRegisterBias +
      reinterpret_cast<Address>(isolate()->heap()->roots_array_start());
  intptr_t delta = other.address() - roots_register_value;
  return delta;
}


Operand MacroAssembler::ExternalOperand(ExternalReference target,
                                        Register scratch) {
  if (root_array_available_ && !Serializer::enabled()) {
    intptr_t delta = RootRegisterDelta(target);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      Serializer::TooLateToEnableNow();
      return Operand(kRootRegister, static_cast<int32_t>(delta));
    }
  }
  movq(scratch, target);
  return Operand(scratch, 0);
}


void MacroAssembler::Load(Register destination, ExternalReference source) {
  if (root_array_available_ && !Serializer::enabled()) {
    intptr_t delta = RootRegisterDelta(source);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      Serializer::TooLateToEnableNow();
      movq(destination, Operand(kRootRegister, static_cast<int32_t>(delta)));
      return;
    }
  }
  // Safe code.
  if (destination.is(rax)) {
    load_rax(source);
  } else {
    movq(kScratchRegister, source);
    movq(destination, Operand(kScratchRegister, 0));
  }
}


void MacroAssembler::Store(ExternalReference destination, Register source) {
  if (root_array_available_ && !Serializer::enabled()) {
    intptr_t delta = RootRegisterDelta(destination);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      Serializer::TooLateToEnableNow();
      movq(Operand(kRootRegister, static_cast<int32_t>(delta)), source);
      return;
    }
  }
  // Safe code.
  if (source.is(rax)) {
    store_rax(destination);
  } else {
    movq(kScratchRegister, destination);
    movq(Operand(kScratchRegister, 0), source);
  }
}


void MacroAssembler::LoadAddress(Register destination,
                                 ExternalReference source) {
  if (root_array_available_ && !Serializer::enabled()) {
    intptr_t delta = RootRegisterDelta(source);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      Serializer::TooLateToEnableNow();
      lea(destination, Operand(kRootRegister, static_cast<int32_t>(delta)));
      return;
    }
  }
  // Safe code.
  movq(destination, source);
}


int MacroAssembler::LoadAddressSize(ExternalReference source) {
  if (root_array_available_ && !Serializer::enabled()) {
    // This calculation depends on the internals of LoadAddress.
    // It's correctness is ensured by the asserts in the Call
    // instruction below.
    intptr_t delta = RootRegisterDelta(source);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      Serializer::TooLateToEnableNow();
      // Operand is lea(scratch, Operand(kRootRegister, delta));
      // Opcodes : REX.W 8D ModRM Disp8/Disp32  - 4 or 7.
      int size = 4;
      if (!is_int8(static_cast<int32_t>(delta))) {
        size += 3;  // Need full four-byte displacement in lea.
      }
      return size;
    }
  }
  // Size of movq(destination, src);
  return Assembler::kMoveAddressIntoScratchRegisterInstructionLength;
}


void MacroAssembler::PushAddress(ExternalReference source) {
  int64_t address = reinterpret_cast<int64_t>(source.address());
  if (is_int32(address) && !Serializer::enabled()) {
    if (emit_debug_code()) {
      movq(kScratchRegister, BitCast<int64_t>(kZapValue), RelocInfo::NONE64);
    }
    push(Immediate(static_cast<int32_t>(address)));
    return;
  }
  LoadAddress(kScratchRegister, source);
  push(kScratchRegister);
}


void MacroAssembler::LoadRoot(Register destination, Heap::RootListIndex index) {
  ASSERT(root_array_available_);
  movq(destination, Operand(kRootRegister,
                            (index << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::LoadRootIndexed(Register destination,
                                     Register variable_offset,
                                     int fixed_offset) {
  ASSERT(root_array_available_);
  movq(destination,
       Operand(kRootRegister,
               variable_offset, times_pointer_size,
               (fixed_offset << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::StoreRoot(Register source, Heap::RootListIndex index) {
  ASSERT(root_array_available_);
  movq(Operand(kRootRegister, (index << kPointerSizeLog2) - kRootRegisterBias),
       source);
}


void MacroAssembler::PushRoot(Heap::RootListIndex index) {
  ASSERT(root_array_available_);
  push(Operand(kRootRegister, (index << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::CompareRoot(Register with, Heap::RootListIndex index) {
  ASSERT(root_array_available_);
  cmpq(with, Operand(kRootRegister,
                     (index << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::CompareRoot(const Operand& with,
                                 Heap::RootListIndex index) {
  ASSERT(root_array_available_);
  ASSERT(!with.AddressUsesRegister(kScratchRegister));
  LoadRoot(kScratchRegister, index);
  cmpq(with, kScratchRegister);
}


void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register addr,
                                         Register scratch,
                                         SaveFPRegsMode save_fp,
                                         RememberedSetFinalAction and_then) {
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, scratch, &ok, Label::kNear);
    int3();
    bind(&ok);
  }
  // Load store buffer top.
  LoadRoot(scratch, Heap::kStoreBufferTopRootIndex);
  // Store pointer to buffer.
  movq(Operand(scratch, 0), addr);
  // Increment buffer top.
  addq(scratch, Immediate(kPointerSize));
  // Write back new top of buffer.
  StoreRoot(scratch, Heap::kStoreBufferTopRootIndex);
  // Call stub on end of buffer.
  Label done;
  // Check for end of buffer.
  testq(scratch, Immediate(StoreBuffer::kStoreBufferOverflowBit));
  if (and_then == kReturnAtEnd) {
    Label buffer_overflowed;
    j(not_equal, &buffer_overflowed, Label::kNear);
    ret(0);
    bind(&buffer_overflowed);
  } else {
    ASSERT(and_then == kFallThroughAtEnd);
    j(equal, &done, Label::kNear);
  }
  StoreBufferOverflowStub store_buffer_overflow =
      StoreBufferOverflowStub(save_fp);
  CallStub(&store_buffer_overflow);
  if (and_then == kReturnAtEnd) {
    ret(0);
  } else {
    ASSERT(and_then == kFallThroughAtEnd);
    bind(&done);
  }
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cc,
                                Label* branch,
                                Label::Distance distance) {
  if (Serializer::enabled()) {
    // Can't do arithmetic on external references if it might get serialized.
    // The mask isn't really an address.  We load it as an external reference in
    // case the size of the new space is different between the snapshot maker
    // and the running system.
    if (scratch.is(object)) {
      movq(kScratchRegister, ExternalReference::new_space_mask(isolate()));
      and_(scratch, kScratchRegister);
    } else {
      movq(scratch, ExternalReference::new_space_mask(isolate()));
      and_(scratch, object);
    }
    movq(kScratchRegister, ExternalReference::new_space_start(isolate()));
    cmpq(scratch, kScratchRegister);
    j(cc, branch, distance);
  } else {
    ASSERT(is_int32(static_cast<int64_t>(isolate()->heap()->NewSpaceMask())));
    intptr_t new_space_start =
        reinterpret_cast<intptr_t>(isolate()->heap()->NewSpaceStart());
    movq(kScratchRegister, -new_space_start, RelocInfo::NONE64);
    if (scratch.is(object)) {
      addq(scratch, kScratchRegister);
    } else {
      lea(scratch, Operand(object, kScratchRegister, times_1, 0));
    }
    and_(scratch,
         Immediate(static_cast<int32_t>(isolate()->heap()->NewSpaceMask())));
    j(cc, branch, distance);
  }
}


void MacroAssembler::RecordWriteField(
    Register object,
    int offset,
    Register value,
    Register dst,
    SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are rsi.
  ASSERT(!value.is(rsi) && !dst.is(rsi));

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  ASSERT(IsAligned(offset, kPointerSize));

  lea(dst, FieldOperand(object, offset));
  if (emit_debug_code()) {
    Label ok;
    testb(dst, Immediate((1 << kPointerSizeLog2) - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(
      object, dst, value, save_fp, remembered_set_action, OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    movq(value, BitCast<int64_t>(kZapValue), RelocInfo::NONE64);
    movq(dst, BitCast<int64_t>(kZapValue), RelocInfo::NONE64);
  }
}


void MacroAssembler::RecordWriteArray(Register object,
                                      Register value,
                                      Register index,
                                      SaveFPRegsMode save_fp,
                                      RememberedSetAction remembered_set_action,
                                      SmiCheck smi_check) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Array access: calculate the destination address. Index is not a smi.
  Register dst = index;
  lea(dst, Operand(object, index, times_pointer_size,
                   FixedArray::kHeaderSize - kHeapObjectTag));

  RecordWrite(
      object, dst, value, save_fp, remembered_set_action, OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    movq(value, BitCast<int64_t>(kZapValue), RelocInfo::NONE64);
    movq(index, BitCast<int64_t>(kZapValue), RelocInfo::NONE64);
  }
}


void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register value,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are rsi.
  ASSERT(!value.is(rsi) && !address.is(rsi));

  ASSERT(!object.is(value));
  ASSERT(!object.is(address));
  ASSERT(!value.is(address));
  AssertNotSmi(object);

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    Label ok;
    cmpq(value, Operand(address, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    // Skip barrier if writing a smi.
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  RecordWriteStub stub(object, value, address, remembered_set_action, fp_mode);
  CallStub(&stub);

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    movq(address, BitCast<int64_t>(kZapValue), RelocInfo::NONE64);
    movq(value, BitCast<int64_t>(kZapValue), RelocInfo::NONE64);
  }
}


void MacroAssembler::Assert(Condition cc, BailoutReason reason) {
  if (emit_debug_code()) Check(cc, reason);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    Label ok;
    CompareRoot(FieldOperand(elements, HeapObject::kMapOffset),
                Heap::kFixedArrayMapRootIndex);
    j(equal, &ok, Label::kNear);
    CompareRoot(FieldOperand(elements, HeapObject::kMapOffset),
                Heap::kFixedDoubleArrayMapRootIndex);
    j(equal, &ok, Label::kNear);
    CompareRoot(FieldOperand(elements, HeapObject::kMapOffset),
                Heap::kFixedCOWArrayMapRootIndex);
    j(equal, &ok, Label::kNear);
    Abort(kJSObjectWithFastElementsMapHasSlowElements);
    bind(&ok);
  }
}


void MacroAssembler::Check(Condition cc, BailoutReason reason) {
  Label L;
  j(cc, &L, Label::kNear);
  Abort(reason);
  // Control will not return here.
  bind(&L);
}


void MacroAssembler::CheckStackAlignment() {
  int frame_alignment = OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    ASSERT(IsPowerOf2(frame_alignment));
    Label alignment_as_expected;
    testq(rsp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected, Label::kNear);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}


void MacroAssembler::NegativeZeroTest(Register result,
                                      Register op,
                                      Label* then_label) {
  Label ok;
  testl(result, result);
  j(not_zero, &ok, Label::kNear);
  testl(op, op);
  j(sign, then_label);
  bind(&ok);
}


void MacroAssembler::Abort(BailoutReason reason) {
  // We want to pass the msg string like a smi to avoid GC
  // problems, however msg is not guaranteed to be aligned
  // properly. Instead, we pass an aligned pointer that is
  // a proper v8 smi, but also pass the alignment difference
  // from the real pointer as a smi.
  const char* msg = GetBailoutReason(reason);
  intptr_t p1 = reinterpret_cast<intptr_t>(msg);
  intptr_t p0 = (p1 & ~kSmiTagMask) + kSmiTag;
  // Note: p0 might not be a valid Smi _value_, but it has a valid Smi tag.
  ASSERT(reinterpret_cast<Object*>(p0)->IsSmi());
#ifdef DEBUG
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    int3();
    return;
  }
#endif

  push(rax);
  movq(kScratchRegister, p0, RelocInfo::NONE64);
  push(kScratchRegister);
  movq(kScratchRegister,
       reinterpret_cast<intptr_t>(Smi::FromInt(static_cast<int>(p1 - p0))),
       RelocInfo::NONE64);
  push(kScratchRegister);

  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kAbort, 2);
  } else {
    CallRuntime(Runtime::kAbort, 2);
  }
  // Control will not return here.
  int3();
}


void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id) {
  ASSERT(AllowThisStubCall(stub));  // Calls are not allowed in some stubs
  Call(stub->GetCode(isolate()), RelocInfo::CODE_TARGET, ast_id);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls_ ||
         stub->CompilingCallsToThisStubIsGCSafe(isolate()));
  Jump(stub->GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::StubReturn(int argc) {
  ASSERT(argc >= 1 && generating_stub());
  ret((argc - 1) * kPointerSize);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  if (!has_frame_ && stub->SometimesSetsUpAFrame()) return false;
  return allow_stub_calls_ || stub->CompilingCallsToThisStubIsGCSafe(isolate());
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    addq(rsp, Immediate(num_arguments * kPointerSize));
  }
  LoadRoot(rax, Heap::kUndefinedValueRootIndex);
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // The assert checks that the constants for the maximum number of digits
  // for an array index cached in the hash field and the number of bits
  // reserved for it does not conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key. Even if we subsequently go to
  // the slow case, converting the key to a smi is always valid.
  // key: string key
  // hash: key's hash field, including its array index value.
  and_(hash, Immediate(String::kArrayIndexValueMask));
  shr(hash, Immediate(String::kHashShift));
  // Here we actually clobber the key which will be used if calling into
  // runtime later. However as the new key is the numeric value of a string key
  // there is no difference in using either key.
  Integer32ToSmi(index, hash);
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  if (f->nargs >= 0 && f->nargs != num_arguments) {
    IllegalOperation(num_arguments);
    return;
  }

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(rax, num_arguments);
  LoadAddress(rbx, ExternalReference(f, isolate()));
  CEntryStub ces(f->result_size, save_doubles);
  CallStub(&ces);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  Set(rax, num_arguments);
  LoadAddress(rbx, ext);

  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // ----------- S t a t e -------------
  //  -- rsp[0]                 : return address
  //  -- rsp[8]                 : argument num_arguments - 1
  //  ...
  //  -- rsp[8 * num_arguments] : argument 0 (receiver)
  // -----------------------------------

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(rax, num_arguments);
  JumpToExternalReference(ext, result_size);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid, isolate()),
                            num_arguments,
                            result_size);
}


static int Offset(ExternalReference ref0, ExternalReference ref1) {
  int64_t offset = (ref0.address() - ref1.address());
  // Check that fits into int.
  ASSERT(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}


void MacroAssembler::PrepareCallApiFunction(int arg_stack_space) {
  EnterApiExitFrame(arg_stack_space);
}


void MacroAssembler::CallApiFunctionAndReturn(
    Address function_address,
    Address thunk_address,
    Register thunk_last_arg,
    int stack_space,
    Operand return_value_operand,
    Operand* context_restore_operand) {
  Label prologue;
  Label promote_scheduled_exception;
  Label exception_handled;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label write_back;

  Factory* factory = isolate()->factory();
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address(isolate());
  const int kNextOffset = 0;
  const int kLimitOffset = Offset(
      ExternalReference::handle_scope_limit_address(isolate()),
      next_address);
  const int kLevelOffset = Offset(
      ExternalReference::handle_scope_level_address(isolate()),
      next_address);
  ExternalReference scheduled_exception_address =
      ExternalReference::scheduled_exception_address(isolate());

  // Allocate HandleScope in callee-save registers.
  Register prev_next_address_reg = r14;
  Register prev_limit_reg = rbx;
  Register base_reg = r15;
  movq(base_reg, next_address);
  movq(prev_next_address_reg, Operand(base_reg, kNextOffset));
  movq(prev_limit_reg, Operand(base_reg, kLimitOffset));
  addl(Operand(base_reg, kLevelOffset), Immediate(1));

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1);
    LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate()));
    CallCFunction(ExternalReference::log_enter_external_function(isolate()), 1);
    PopSafepointRegisters();
  }


  Label profiler_disabled;
  Label end_profiler_check;
  bool* is_profiling_flag =
      isolate()->cpu_profiler()->is_profiling_address();
  STATIC_ASSERT(sizeof(*is_profiling_flag) == 1);
  movq(rax, is_profiling_flag, RelocInfo::EXTERNAL_REFERENCE);
  cmpb(Operand(rax, 0), Immediate(0));
  j(zero, &profiler_disabled);

  // Third parameter is the address of the actual getter function.
  movq(thunk_last_arg, function_address, RelocInfo::EXTERNAL_REFERENCE);
  movq(rax, thunk_address, RelocInfo::EXTERNAL_REFERENCE);
  jmp(&end_profiler_check);

  bind(&profiler_disabled);
  // Call the api function!
  movq(rax, reinterpret_cast<Address>(function_address),
       RelocInfo::EXTERNAL_REFERENCE);

  bind(&end_profiler_check);

  // Call the api function!
  call(rax);

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1);
    LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate()));
    CallCFunction(ExternalReference::log_leave_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  // Load the value from ReturnValue
  movq(rax, return_value_operand);
  bind(&prologue);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  subl(Operand(base_reg, kLevelOffset), Immediate(1));
  movq(Operand(base_reg, kNextOffset), prev_next_address_reg);
  cmpq(prev_limit_reg, Operand(base_reg, kLimitOffset));
  j(not_equal, &delete_allocated_handles);
  bind(&leave_exit_frame);

  // Check if the function scheduled an exception.
  movq(rsi, scheduled_exception_address);
  Cmp(Operand(rsi, 0), factory->the_hole_value());
  j(not_equal, &promote_scheduled_exception);
  bind(&exception_handled);

#if ENABLE_EXTRA_CHECKS
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = rax;
  Register map = rcx;

  JumpIfSmi(return_value, &ok, Label::kNear);
  movq(map, FieldOperand(return_value, HeapObject::kMapOffset));

  CmpInstanceType(map, FIRST_NONSTRING_TYPE);
  j(below, &ok, Label::kNear);

  CmpInstanceType(map, FIRST_SPEC_OBJECT_TYPE);
  j(above_equal, &ok, Label::kNear);

  CompareRoot(map, Heap::kHeapNumberMapRootIndex);
  j(equal, &ok, Label::kNear);

  CompareRoot(return_value, Heap::kUndefinedValueRootIndex);
  j(equal, &ok, Label::kNear);

  CompareRoot(return_value, Heap::kTrueValueRootIndex);
  j(equal, &ok, Label::kNear);

  CompareRoot(return_value, Heap::kFalseValueRootIndex);
  j(equal, &ok, Label::kNear);

  CompareRoot(return_value, Heap::kNullValueRootIndex);
  j(equal, &ok, Label::kNear);

  Abort(kAPICallReturnedInvalidObject);

  bind(&ok);
#endif

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    movq(rsi, *context_restore_operand);
  }
  LeaveApiExitFrame(!restore_context);
  ret(stack_space * kPointerSize);

  bind(&promote_scheduled_exception);
  {
    FrameScope frame(this, StackFrame::INTERNAL);
    CallRuntime(Runtime::kPromoteScheduledException, 0);
  }
  jmp(&exception_handled);

  // HandleScope limit has changed. Delete allocated extensions.
  bind(&delete_allocated_handles);
  movq(Operand(base_reg, kLimitOffset), prev_limit_reg);
  movq(prev_limit_reg, rax);
  LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate()));
  LoadAddress(rax,
              ExternalReference::delete_handle_scope_extensions(isolate()));
  call(rax);
  movq(rax, prev_limit_reg);
  jmp(&leave_exit_frame);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             int result_size) {
  // Set the entry point and jump to the C entry runtime stub.
  LoadAddress(rbx, ext);
  CEntryStub ces(result_size);
  jmp(ces.GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Rely on the assertion to check that the number of provided
  // arguments match the expected number of arguments. Fake a
  // parameter count to avoid emitting code to do the check.
  ParameterCount expected(0);
  GetBuiltinEntry(rdx, id);
  InvokeCode(rdx, expected, expected, flag, call_wrapper, CALL_AS_METHOD);
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  movq(target, Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  movq(target, FieldOperand(target, GlobalObject::kBuiltinsOffset));
  movq(target, FieldOperand(target,
                            JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(rdi));
  // Load the JavaScript builtin function from the builtins object.
  GetBuiltinFunction(rdi, id);
  movq(target, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
}


#define REG(Name) { kRegister_ ## Name ## _Code }

static const Register saved_regs[] = {
  REG(rax), REG(rcx), REG(rdx), REG(rbx), REG(rbp), REG(rsi), REG(rdi), REG(r8),
  REG(r9), REG(r10), REG(r11)
};

#undef REG

static const int kNumberOfSavedRegs = sizeof(saved_regs) / sizeof(Register);


void MacroAssembler::PushCallerSaved(SaveFPRegsMode fp_mode,
                                     Register exclusion1,
                                     Register exclusion2,
                                     Register exclusion3) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (!reg.is(exclusion1) && !reg.is(exclusion2) && !reg.is(exclusion3)) {
      push(reg);
    }
  }
  // R12 to r15 are callee save on all platforms.
  if (fp_mode == kSaveFPRegs) {
    subq(rsp, Immediate(kDoubleSize * XMMRegister::kMaxNumRegisters));
    for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(Operand(rsp, i * kDoubleSize), reg);
    }
  }
}


void MacroAssembler::PopCallerSaved(SaveFPRegsMode fp_mode,
                                    Register exclusion1,
                                    Register exclusion2,
                                    Register exclusion3) {
  if (fp_mode == kSaveFPRegs) {
    for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      movsd(reg, Operand(rsp, i * kDoubleSize));
    }
    addq(rsp, Immediate(kDoubleSize * XMMRegister::kMaxNumRegisters));
  }
  for (int i = kNumberOfSavedRegs - 1; i >= 0; i--) {
    Register reg = saved_regs[i];
    if (!reg.is(exclusion1) && !reg.is(exclusion2) && !reg.is(exclusion3)) {
      pop(reg);
    }
  }
}


void MacroAssembler::Cvtlsi2sd(XMMRegister dst, Register src) {
  xorps(dst, dst);
  cvtlsi2sd(dst, src);
}


void MacroAssembler::Cvtlsi2sd(XMMRegister dst, const Operand& src) {
  xorps(dst, dst);
  cvtlsi2sd(dst, src);
}


void MacroAssembler::Load(Register dst, const Operand& src, Representation r) {
  ASSERT(!r.IsDouble());
  if (r.IsByte()) {
    movzxbl(dst, src);
  } else if (r.IsInteger32()) {
    movl(dst, src);
  } else {
    movq(dst, src);
  }
}


void MacroAssembler::Store(const Operand& dst, Register src, Representation r) {
  ASSERT(!r.IsDouble());
  if (r.IsByte()) {
    movb(dst, src);
  } else if (r.IsInteger32()) {
    movl(dst, src);
  } else {
    movq(dst, src);
  }
}


void MacroAssembler::Set(Register dst, int64_t x) {
  if (x == 0) {
    xorl(dst, dst);
  } else if (is_uint32(x)) {
    movl(dst, Immediate(static_cast<uint32_t>(x)));
  } else if (is_int32(x)) {
    movq(dst, Immediate(static_cast<int32_t>(x)));
  } else {
    movq(dst, x, RelocInfo::NONE64);
  }
}


void MacroAssembler::Set(const Operand& dst, int64_t x) {
  if (is_int32(x)) {
    movq(dst, Immediate(static_cast<int32_t>(x)));
  } else {
    Set(kScratchRegister, x);
    movq(dst, kScratchRegister);
  }
}


// ----------------------------------------------------------------------------
// Smi tagging, untagging and tag detection.

bool MacroAssembler::IsUnsafeInt(const int32_t x) {
  static const int kMaxBits = 17;
  return !is_intn(x, kMaxBits);
}


void MacroAssembler::SafeMove(Register dst, Smi* src) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(SmiValuesAre32Bits());  // JIT cookie can be converted to Smi.
  if (IsUnsafeInt(src->value()) && jit_cookie() != 0) {
    Move(dst, Smi::FromInt(src->value() ^ jit_cookie()));
    Move(kScratchRegister, Smi::FromInt(jit_cookie()));
    xor_(dst, kScratchRegister);
  } else {
    Move(dst, src);
  }
}


void MacroAssembler::SafePush(Smi* src) {
  ASSERT(SmiValuesAre32Bits());  // JIT cookie can be converted to Smi.
  if (IsUnsafeInt(src->value()) && jit_cookie() != 0) {
    Push(Smi::FromInt(src->value() ^ jit_cookie()));
    Move(kScratchRegister, Smi::FromInt(jit_cookie()));
    xor_(Operand(rsp, 0), kScratchRegister);
  } else {
    Push(src);
  }
}


Register MacroAssembler::GetSmiConstant(Smi* source) {
  int value = source->value();
  if (value == 0) {
    xorl(kScratchRegister, kScratchRegister);
    return kScratchRegister;
  }
  if (value == 1) {
    return kSmiConstantRegister;
  }
  LoadSmiConstant(kScratchRegister, source);
  return kScratchRegister;
}


void MacroAssembler::LoadSmiConstant(Register dst, Smi* source) {
  if (emit_debug_code()) {
    movq(dst,
         reinterpret_cast<uint64_t>(Smi::FromInt(kSmiConstantRegisterValue)),
         RelocInfo::NONE64);
    cmpq(dst, kSmiConstantRegister);
    if (allow_stub_calls()) {
      Assert(equal, kUninitializedKSmiConstantRegister);
    } else {
      Label ok;
      j(equal, &ok, Label::kNear);
      int3();
      bind(&ok);
    }
  }
  int value = source->value();
  if (value == 0) {
    xorl(dst, dst);
    return;
  }
  bool negative = value < 0;
  unsigned int uvalue = negative ? -value : value;

  switch (uvalue) {
    case 9:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_8, 0));
      break;
    case 8:
      xorl(dst, dst);
      lea(dst, Operand(dst, kSmiConstantRegister, times_8, 0));
      break;
    case 4:
      xorl(dst, dst);
      lea(dst, Operand(dst, kSmiConstantRegister, times_4, 0));
      break;
    case 5:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_4, 0));
      break;
    case 3:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_2, 0));
      break;
    case 2:
      lea(dst, Operand(kSmiConstantRegister, kSmiConstantRegister, times_1, 0));
      break;
    case 1:
      movq(dst, kSmiConstantRegister);
      break;
    case 0:
      UNREACHABLE();
      return;
    default:
      movq(dst, reinterpret_cast<uint64_t>(source), RelocInfo::NONE64);
      return;
  }
  if (negative) {
    neg(dst);
  }
}


void MacroAssembler::Integer32ToSmi(Register dst, Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  if (!dst.is(src)) {
    movl(dst, src);
  }
  shl(dst, Immediate(kSmiShift));
}


void MacroAssembler::Integer32ToSmiField(const Operand& dst, Register src) {
  if (emit_debug_code()) {
    testb(dst, Immediate(0x01));
    Label ok;
    j(zero, &ok, Label::kNear);
    if (allow_stub_calls()) {
      Abort(kInteger32ToSmiFieldWritingToNonSmiLocation);
    } else {
      int3();
    }
    bind(&ok);
  }
  ASSERT(kSmiShift % kBitsPerByte == 0);
  movl(Operand(dst, kSmiShift / kBitsPerByte), src);
}


void MacroAssembler::Integer64PlusConstantToSmi(Register dst,
                                                Register src,
                                                int constant) {
  if (dst.is(src)) {
    addl(dst, Immediate(constant));
  } else {
    leal(dst, Operand(src, constant));
  }
  shl(dst, Immediate(kSmiShift));
}


void MacroAssembler::SmiToInteger32(Register dst, Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  if (!dst.is(src)) {
    movq(dst, src);
  }
  shr(dst, Immediate(kSmiShift));
}


void MacroAssembler::SmiToInteger32(Register dst, const Operand& src) {
  movl(dst, Operand(src, kSmiShift / kBitsPerByte));
}


void MacroAssembler::SmiToInteger64(Register dst, Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  if (!dst.is(src)) {
    movq(dst, src);
  }
  sar(dst, Immediate(kSmiShift));
}


void MacroAssembler::SmiToInteger64(Register dst, const Operand& src) {
  movsxlq(dst, Operand(src, kSmiShift / kBitsPerByte));
}


void MacroAssembler::SmiTest(Register src) {
  AssertSmi(src);
  testq(src, src);
}


void MacroAssembler::SmiCompare(Register smi1, Register smi2) {
  AssertSmi(smi1);
  AssertSmi(smi2);
  cmpq(smi1, smi2);
}


void MacroAssembler::SmiCompare(Register dst, Smi* src) {
  AssertSmi(dst);
  Cmp(dst, src);
}


void MacroAssembler::Cmp(Register dst, Smi* src) {
  ASSERT(!dst.is(kScratchRegister));
  if (src->value() == 0) {
    testq(dst, dst);
  } else {
    Register constant_reg = GetSmiConstant(src);
    cmpq(dst, constant_reg);
  }
}


void MacroAssembler::SmiCompare(Register dst, const Operand& src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmpq(dst, src);
}


void MacroAssembler::SmiCompare(const Operand& dst, Register src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmpq(dst, src);
}


void MacroAssembler::SmiCompare(const Operand& dst, Smi* src) {
  AssertSmi(dst);
  cmpl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(src->value()));
}


void MacroAssembler::Cmp(const Operand& dst, Smi* src) {
  // The Operand cannot use the smi register.
  Register smi_reg = GetSmiConstant(src);
  ASSERT(!dst.AddressUsesRegister(smi_reg));
  cmpq(dst, smi_reg);
}


void MacroAssembler::SmiCompareInteger32(const Operand& dst, Register src) {
  cmpl(Operand(dst, kSmiShift / kBitsPerByte), src);
}


void MacroAssembler::PositiveSmiTimesPowerOfTwoToInteger64(Register dst,
                                                           Register src,
                                                           int power) {
  ASSERT(power >= 0);
  ASSERT(power < 64);
  if (power == 0) {
    SmiToInteger64(dst, src);
    return;
  }
  if (!dst.is(src)) {
    movq(dst, src);
  }
  if (power < kSmiShift) {
    sar(dst, Immediate(kSmiShift - power));
  } else if (power > kSmiShift) {
    shl(dst, Immediate(power - kSmiShift));
  }
}


void MacroAssembler::PositiveSmiDivPowerOfTwoToInteger32(Register dst,
                                                         Register src,
                                                         int power) {
  ASSERT((0 <= power) && (power < 32));
  if (dst.is(src)) {
    shr(dst, Immediate(power + kSmiShift));
  } else {
    UNIMPLEMENTED();  // Not used.
  }
}


void MacroAssembler::SmiOrIfSmis(Register dst, Register src1, Register src2,
                                 Label* on_not_smis,
                                 Label::Distance near_jump) {
  if (dst.is(src1) || dst.is(src2)) {
    ASSERT(!src1.is(kScratchRegister));
    ASSERT(!src2.is(kScratchRegister));
    movq(kScratchRegister, src1);
    or_(kScratchRegister, src2);
    JumpIfNotSmi(kScratchRegister, on_not_smis, near_jump);
    movq(dst, kScratchRegister);
  } else {
    movq(dst, src1);
    or_(dst, src2);
    JumpIfNotSmi(dst, on_not_smis, near_jump);
  }
}


Condition MacroAssembler::CheckSmi(Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}


Condition MacroAssembler::CheckSmi(const Operand& src) {
  STATIC_ASSERT(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}


Condition MacroAssembler::CheckNonNegativeSmi(Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  // Test that both bits of the mask 0x8000000000000001 are zero.
  movq(kScratchRegister, src);
  rol(kScratchRegister, Immediate(1));
  testb(kScratchRegister, Immediate(3));
  return zero;
}


Condition MacroAssembler::CheckBothSmi(Register first, Register second) {
  if (first.is(second)) {
    return CheckSmi(first);
  }
  STATIC_ASSERT(kSmiTag == 0 && kHeapObjectTag == 1 && kHeapObjectTagMask == 3);
  leal(kScratchRegister, Operand(first, second, times_1, 0));
  testb(kScratchRegister, Immediate(0x03));
  return zero;
}


Condition MacroAssembler::CheckBothNonNegativeSmi(Register first,
                                                  Register second) {
  if (first.is(second)) {
    return CheckNonNegativeSmi(first);
  }
  movq(kScratchRegister, first);
  or_(kScratchRegister, second);
  rol(kScratchRegister, Immediate(1));
  testl(kScratchRegister, Immediate(3));
  return zero;
}


Condition MacroAssembler::CheckEitherSmi(Register first,
                                         Register second,
                                         Register scratch) {
  if (first.is(second)) {
    return CheckSmi(first);
  }
  if (scratch.is(second)) {
    andl(scratch, first);
  } else {
    if (!scratch.is(first)) {
      movl(scratch, first);
    }
    andl(scratch, second);
  }
  testb(scratch, Immediate(kSmiTagMask));
  return zero;
}


Condition MacroAssembler::CheckIsMinSmi(Register src) {
  ASSERT(!src.is(kScratchRegister));
  // If we overflow by subtracting one, it's the minimal smi value.
  cmpq(src, kSmiConstantRegister);
  return overflow;
}


Condition MacroAssembler::CheckInteger32ValidSmiValue(Register src) {
  // A 32-bit integer value can always be converted to a smi.
  return always;
}


Condition MacroAssembler::CheckUInteger32ValidSmiValue(Register src) {
  // An unsigned 32-bit integer value is valid as long as the high bit
  // is not set.
  testl(src, src);
  return positive;
}


void MacroAssembler::CheckSmiToIndicator(Register dst, Register src) {
  if (dst.is(src)) {
    andl(dst, Immediate(kSmiTagMask));
  } else {
    movl(dst, Immediate(kSmiTagMask));
    andl(dst, src);
  }
}


void MacroAssembler::CheckSmiToIndicator(Register dst, const Operand& src) {
  if (!(src.AddressUsesRegister(dst))) {
    movl(dst, Immediate(kSmiTagMask));
    andl(dst, src);
  } else {
    movl(dst, src);
    andl(dst, Immediate(kSmiTagMask));
  }
}


void MacroAssembler::JumpIfNotValidSmiValue(Register src,
                                            Label* on_invalid,
                                            Label::Distance near_jump) {
  Condition is_valid = CheckInteger32ValidSmiValue(src);
  j(NegateCondition(is_valid), on_invalid, near_jump);
}


void MacroAssembler::JumpIfUIntNotValidSmiValue(Register src,
                                                Label* on_invalid,
                                                Label::Distance near_jump) {
  Condition is_valid = CheckUInteger32ValidSmiValue(src);
  j(NegateCondition(is_valid), on_invalid, near_jump);
}


void MacroAssembler::JumpIfSmi(Register src,
                               Label* on_smi,
                               Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(smi, on_smi, near_jump);
}


void MacroAssembler::JumpIfNotSmi(Register src,
                                  Label* on_not_smi,
                                  Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(NegateCondition(smi), on_not_smi, near_jump);
}


void MacroAssembler::JumpUnlessNonNegativeSmi(
    Register src, Label* on_not_smi_or_negative,
    Label::Distance near_jump) {
  Condition non_negative_smi = CheckNonNegativeSmi(src);
  j(NegateCondition(non_negative_smi), on_not_smi_or_negative, near_jump);
}


void MacroAssembler::JumpIfSmiEqualsConstant(Register src,
                                             Smi* constant,
                                             Label* on_equals,
                                             Label::Distance near_jump) {
  SmiCompare(src, constant);
  j(equal, on_equals, near_jump);
}


void MacroAssembler::JumpIfNotBothSmi(Register src1,
                                      Register src2,
                                      Label* on_not_both_smi,
                                      Label::Distance near_jump) {
  Condition both_smi = CheckBothSmi(src1, src2);
  j(NegateCondition(both_smi), on_not_both_smi, near_jump);
}


void MacroAssembler::JumpUnlessBothNonNegativeSmi(Register src1,
                                                  Register src2,
                                                  Label* on_not_both_smi,
                                                  Label::Distance near_jump) {
  Condition both_smi = CheckBothNonNegativeSmi(src1, src2);
  j(NegateCondition(both_smi), on_not_both_smi, near_jump);
}


void MacroAssembler::SmiAddConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
    return;
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    switch (constant->value()) {
      case 1:
        addq(dst, kSmiConstantRegister);
        return;
      case 2:
        lea(dst, Operand(src, kSmiConstantRegister, times_2, 0));
        return;
      case 4:
        lea(dst, Operand(src, kSmiConstantRegister, times_4, 0));
        return;
      case 8:
        lea(dst, Operand(src, kSmiConstantRegister, times_8, 0));
        return;
      default:
        Register constant_reg = GetSmiConstant(constant);
        addq(dst, constant_reg);
        return;
    }
  } else {
    switch (constant->value()) {
      case 1:
        lea(dst, Operand(src, kSmiConstantRegister, times_1, 0));
        return;
      case 2:
        lea(dst, Operand(src, kSmiConstantRegister, times_2, 0));
        return;
      case 4:
        lea(dst, Operand(src, kSmiConstantRegister, times_4, 0));
        return;
      case 8:
        lea(dst, Operand(src, kSmiConstantRegister, times_8, 0));
        return;
      default:
        LoadSmiConstant(dst, constant);
        addq(dst, src);
        return;
    }
  }
}


void MacroAssembler::SmiAddConstant(const Operand& dst, Smi* constant) {
  if (constant->value() != 0) {
    addl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(constant->value()));
  }
}


void MacroAssembler::SmiAddConstant(Register dst,
                                    Register src,
                                    Smi* constant,
                                    Label* on_not_smi_result,
                                    Label::Distance near_jump) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));

    Label done;
    LoadSmiConstant(kScratchRegister, constant);
    addq(dst, kScratchRegister);
    j(no_overflow, &done, Label::kNear);
    // Restore src.
    subq(dst, kScratchRegister);
    jmp(on_not_smi_result, near_jump);
    bind(&done);
  } else {
    LoadSmiConstant(dst, constant);
    addq(dst, src);
    j(overflow, on_not_smi_result, near_jump);
  }
}


void MacroAssembler::SmiSubConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    subq(dst, constant_reg);
  } else {
    if (constant->value() == Smi::kMinValue) {
      LoadSmiConstant(dst, constant);
      // Adding and subtracting the min-value gives the same result, it only
      // differs on the overflow bit, which we don't check here.
      addq(dst, src);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(dst, Smi::FromInt(-constant->value()));
      addq(dst, src);
    }
  }
}


void MacroAssembler::SmiSubConstant(Register dst,
                                    Register src,
                                    Smi* constant,
                                    Label* on_not_smi_result,
                                    Label::Distance near_jump) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movq(dst, src);
    }
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    if (constant->value() == Smi::kMinValue) {
      // Subtracting min-value from any non-negative value will overflow.
      // We test the non-negativeness before doing the subtraction.
      testq(src, src);
      j(not_sign, on_not_smi_result, near_jump);
      LoadSmiConstant(kScratchRegister, constant);
      subq(dst, kScratchRegister);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(kScratchRegister, Smi::FromInt(-constant->value()));
      addq(kScratchRegister, dst);
      j(overflow, on_not_smi_result, near_jump);
      movq(dst, kScratchRegister);
    }
  } else {
    if (constant->value() == Smi::kMinValue) {
      // Subtracting min-value from any non-negative value will overflow.
      // We test the non-negativeness before doing the subtraction.
      testq(src, src);
      j(not_sign, on_not_smi_result, near_jump);
      LoadSmiConstant(dst, constant);
      // Adding and subtracting the min-value gives the same result, it only
      // differs on the overflow bit, which we don't check here.
      addq(dst, src);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(dst, Smi::FromInt(-(constant->value())));
      addq(dst, src);
      j(overflow, on_not_smi_result, near_jump);
    }
  }
}


void MacroAssembler::SmiNeg(Register dst,
                            Register src,
                            Label* on_smi_result,
                            Label::Distance near_jump) {
  if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    movq(kScratchRegister, src);
    neg(dst);  // Low 32 bits are retained as zero by negation.
    // Test if result is zero or Smi::kMinValue.
    cmpq(dst, kScratchRegister);
    j(not_equal, on_smi_result, near_jump);
    movq(src, kScratchRegister);
  } else {
    movq(dst, src);
    neg(dst);
    cmpq(dst, src);
    // If the result is zero or Smi::kMinValue, negation failed to create a smi.
    j(not_equal, on_smi_result, near_jump);
  }
}


template<class T>
static void SmiAddHelper(MacroAssembler* masm,
                         Register dst,
                         Register src1,
                         T src2,
                         Label* on_not_smi_result,
                         Label::Distance near_jump) {
  if (dst.is(src1)) {
    Label done;
    masm->addq(dst, src2);
    masm->j(no_overflow, &done, Label::kNear);
    // Restore src1.
    masm->subq(dst, src2);
    masm->jmp(on_not_smi_result, near_jump);
    masm->bind(&done);
  } else {
    masm->movq(dst, src1);
    masm->addq(dst, src2);
    masm->j(overflow, on_not_smi_result, near_jump);
  }
}


void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  ASSERT_NOT_NULL(on_not_smi_result);
  ASSERT(!dst.is(src2));
  SmiAddHelper<Register>(this, dst, src1, src2, on_not_smi_result, near_jump);
}


void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            const Operand& src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  ASSERT_NOT_NULL(on_not_smi_result);
  ASSERT(!src2.AddressUsesRegister(dst));
  SmiAddHelper<Operand>(this, dst, src1, src2, on_not_smi_result, near_jump);
}


void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            Register src2) {
  // No overflow checking. Use only when it's known that
  // overflowing is impossible.
  if (!dst.is(src1)) {
    if (emit_debug_code()) {
      movq(kScratchRegister, src1);
      addq(kScratchRegister, src2);
      Check(no_overflow, kSmiAdditionOverflow);
    }
    lea(dst, Operand(src1, src2, times_1, 0));
  } else {
    addq(dst, src2);
    Assert(no_overflow, kSmiAdditionOverflow);
  }
}


template<class T>
static void SmiSubHelper(MacroAssembler* masm,
                         Register dst,
                         Register src1,
                         T src2,
                         Label* on_not_smi_result,
                         Label::Distance near_jump) {
  if (dst.is(src1)) {
    Label done;
    masm->subq(dst, src2);
    masm->j(no_overflow, &done, Label::kNear);
    // Restore src1.
    masm->addq(dst, src2);
    masm->jmp(on_not_smi_result, near_jump);
    masm->bind(&done);
  } else {
    masm->movq(dst, src1);
    masm->subq(dst, src2);
    masm->j(overflow, on_not_smi_result, near_jump);
  }
}


void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  ASSERT_NOT_NULL(on_not_smi_result);
  ASSERT(!dst.is(src2));
  SmiSubHelper<Register>(this, dst, src1, src2, on_not_smi_result, near_jump);
}


void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            const Operand& src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  ASSERT_NOT_NULL(on_not_smi_result);
  ASSERT(!src2.AddressUsesRegister(dst));
  SmiSubHelper<Operand>(this, dst, src1, src2, on_not_smi_result, near_jump);
}


template<class T>
static void SmiSubNoOverflowHelper(MacroAssembler* masm,
                                   Register dst,
                                   Register src1,
                                   T src2) {
  // No overflow checking. Use only when it's known that
  // overflowing is impossible (e.g., subtracting two positive smis).
  if (!dst.is(src1)) {
    masm->movq(dst, src1);
  }
  masm->subq(dst, src2);
  masm->Assert(no_overflow, kSmiSubtractionOverflow);
}


void MacroAssembler::SmiSub(Register dst, Register src1, Register src2) {
  ASSERT(!dst.is(src2));
  SmiSubNoOverflowHelper<Register>(this, dst, src1, src2);
}


void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            const Operand& src2) {
  SmiSubNoOverflowHelper<Operand>(this, dst, src1, src2);
}


void MacroAssembler::SmiMul(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  ASSERT(!dst.is(src2));
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));

  if (dst.is(src1)) {
    Label failure, zero_correct_result;
    movq(kScratchRegister, src1);  // Create backup for later testing.
    SmiToInteger64(dst, src1);
    imul(dst, src2);
    j(overflow, &failure, Label::kNear);

    // Check for negative zero result.  If product is zero, and one
    // argument is negative, go to slow case.
    Label correct_result;
    testq(dst, dst);
    j(not_zero, &correct_result, Label::kNear);

    movq(dst, kScratchRegister);
    xor_(dst, src2);
    // Result was positive zero.
    j(positive, &zero_correct_result, Label::kNear);

    bind(&failure);  // Reused failure exit, restores src1.
    movq(src1, kScratchRegister);
    jmp(on_not_smi_result, near_jump);

    bind(&zero_correct_result);
    Set(dst, 0);

    bind(&correct_result);
  } else {
    SmiToInteger64(dst, src1);
    imul(dst, src2);
    j(overflow, on_not_smi_result, near_jump);
    // Check for negative zero result.  If product is zero, and one
    // argument is negative, go to slow case.
    Label correct_result;
    testq(dst, dst);
    j(not_zero, &correct_result, Label::kNear);
    // One of src1 and src2 is zero, the check whether the other is
    // negative.
    movq(kScratchRegister, src1);
    xor_(kScratchRegister, src2);
    j(negative, on_not_smi_result, near_jump);
    bind(&correct_result);
  }
}


void MacroAssembler::SmiDiv(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src2.is(rax));
  ASSERT(!src2.is(rdx));
  ASSERT(!src1.is(rdx));

  // Check for 0 divisor (result is +/-Infinity).
  testq(src2, src2);
  j(zero, on_not_smi_result, near_jump);

  if (src1.is(rax)) {
    movq(kScratchRegister, src1);
  }
  SmiToInteger32(rax, src1);
  // We need to rule out dividing Smi::kMinValue by -1, since that would
  // overflow in idiv and raise an exception.
  // We combine this with negative zero test (negative zero only happens
  // when dividing zero by a negative number).

  // We overshoot a little and go to slow case if we divide min-value
  // by any negative value, not just -1.
  Label safe_div;
  testl(rax, Immediate(0x7fffffff));
  j(not_zero, &safe_div, Label::kNear);
  testq(src2, src2);
  if (src1.is(rax)) {
    j(positive, &safe_div, Label::kNear);
    movq(src1, kScratchRegister);
    jmp(on_not_smi_result, near_jump);
  } else {
    j(negative, on_not_smi_result, near_jump);
  }
  bind(&safe_div);

  SmiToInteger32(src2, src2);
  // Sign extend src1 into edx:eax.
  cdq();
  idivl(src2);
  Integer32ToSmi(src2, src2);
  // Check that the remainder is zero.
  testl(rdx, rdx);
  if (src1.is(rax)) {
    Label smi_result;
    j(zero, &smi_result, Label::kNear);
    movq(src1, kScratchRegister);
    jmp(on_not_smi_result, near_jump);
    bind(&smi_result);
  } else {
    j(not_zero, on_not_smi_result, near_jump);
  }
  if (!dst.is(src1) && src1.is(rax)) {
    movq(src1, kScratchRegister);
  }
  Integer32ToSmi(dst, rax);
}


void MacroAssembler::SmiMod(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!src2.is(rax));
  ASSERT(!src2.is(rdx));
  ASSERT(!src1.is(rdx));
  ASSERT(!src1.is(src2));

  testq(src2, src2);
  j(zero, on_not_smi_result, near_jump);

  if (src1.is(rax)) {
    movq(kScratchRegister, src1);
  }
  SmiToInteger32(rax, src1);
  SmiToInteger32(src2, src2);

  // Test for the edge case of dividing Smi::kMinValue by -1 (will overflow).
  Label safe_div;
  cmpl(rax, Immediate(Smi::kMinValue));
  j(not_equal, &safe_div, Label::kNear);
  cmpl(src2, Immediate(-1));
  j(not_equal, &safe_div, Label::kNear);
  // Retag inputs and go slow case.
  Integer32ToSmi(src2, src2);
  if (src1.is(rax)) {
    movq(src1, kScratchRegister);
  }
  jmp(on_not_smi_result, near_jump);
  bind(&safe_div);

  // Sign extend eax into edx:eax.
  cdq();
  idivl(src2);
  // Restore smi tags on inputs.
  Integer32ToSmi(src2, src2);
  if (src1.is(rax)) {
    movq(src1, kScratchRegister);
  }
  // Check for a negative zero result.  If the result is zero, and the
  // dividend is negative, go slow to return a floating point negative zero.
  Label smi_result;
  testl(rdx, rdx);
  j(not_zero, &smi_result, Label::kNear);
  testq(src1, src1);
  j(negative, on_not_smi_result, near_jump);
  bind(&smi_result);
  Integer32ToSmi(dst, rdx);
}


void MacroAssembler::SmiNot(Register dst, Register src) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src.is(kScratchRegister));
  // Set tag and padding bits before negating, so that they are zero afterwards.
  movl(kScratchRegister, Immediate(~0));
  if (dst.is(src)) {
    xor_(dst, kScratchRegister);
  } else {
    lea(dst, Operand(src, kScratchRegister, times_1, 0));
  }
  not_(dst);
}


void MacroAssembler::SmiAnd(Register dst, Register src1, Register src2) {
  ASSERT(!dst.is(src2));
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  and_(dst, src2);
}


void MacroAssembler::SmiAndConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    Set(dst, 0);
  } else if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    and_(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    and_(dst, src);
  }
}


void MacroAssembler::SmiOr(Register dst, Register src1, Register src2) {
  if (!dst.is(src1)) {
    ASSERT(!src1.is(src2));
    movq(dst, src1);
  }
  or_(dst, src2);
}


void MacroAssembler::SmiOrConstant(Register dst, Register src, Smi* constant) {
  if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    or_(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    or_(dst, src);
  }
}


void MacroAssembler::SmiXor(Register dst, Register src1, Register src2) {
  if (!dst.is(src1)) {
    ASSERT(!src1.is(src2));
    movq(dst, src1);
  }
  xor_(dst, src2);
}


void MacroAssembler::SmiXorConstant(Register dst, Register src, Smi* constant) {
  if (dst.is(src)) {
    ASSERT(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    xor_(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    xor_(dst, src);
  }
}


void MacroAssembler::SmiShiftArithmeticRightConstant(Register dst,
                                                     Register src,
                                                     int shift_value) {
  ASSERT(is_uint5(shift_value));
  if (shift_value > 0) {
    if (dst.is(src)) {
      sar(dst, Immediate(shift_value + kSmiShift));
      shl(dst, Immediate(kSmiShift));
    } else {
      UNIMPLEMENTED();  // Not used.
    }
  }
}


void MacroAssembler::SmiShiftLeftConstant(Register dst,
                                          Register src,
                                          int shift_value) {
  if (!dst.is(src)) {
    movq(dst, src);
  }
  if (shift_value > 0) {
    shl(dst, Immediate(shift_value));
  }
}


void MacroAssembler::SmiShiftLogicalRightConstant(
    Register dst, Register src, int shift_value,
    Label* on_not_smi_result, Label::Distance near_jump) {
  // Logic right shift interprets its result as an *unsigned* number.
  if (dst.is(src)) {
    UNIMPLEMENTED();  // Not used.
  } else {
    movq(dst, src);
    if (shift_value == 0) {
      testq(dst, dst);
      j(negative, on_not_smi_result, near_jump);
    }
    shr(dst, Immediate(shift_value + kSmiShift));
    shl(dst, Immediate(kSmiShift));
  }
}


void MacroAssembler::SmiShiftLeft(Register dst,
                                  Register src1,
                                  Register src2) {
  ASSERT(!dst.is(rcx));
  // Untag shift amount.
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  SmiToInteger32(rcx, src2);
  // Shift amount specified by lower 5 bits, not six as the shl opcode.
  and_(rcx, Immediate(0x1f));
  shl_cl(dst);
}


void MacroAssembler::SmiShiftLogicalRight(Register dst,
                                          Register src1,
                                          Register src2,
                                          Label* on_not_smi_result,
                                          Label::Distance near_jump) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(rcx));
  // dst and src1 can be the same, because the one case that bails out
  // is a shift by 0, which leaves dst, and therefore src1, unchanged.
  if (src1.is(rcx) || src2.is(rcx)) {
    movq(kScratchRegister, rcx);
  }
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  SmiToInteger32(rcx, src2);
  orl(rcx, Immediate(kSmiShift));
  shr_cl(dst);  // Shift is rcx modulo 0x1f + 32.
  shl(dst, Immediate(kSmiShift));
  testq(dst, dst);
  if (src1.is(rcx) || src2.is(rcx)) {
    Label positive_result;
    j(positive, &positive_result, Label::kNear);
    if (src1.is(rcx)) {
      movq(src1, kScratchRegister);
    } else {
      movq(src2, kScratchRegister);
    }
    jmp(on_not_smi_result, near_jump);
    bind(&positive_result);
  } else {
    // src2 was zero and src1 negative.
    j(negative, on_not_smi_result, near_jump);
  }
}


void MacroAssembler::SmiShiftArithmeticRight(Register dst,
                                             Register src1,
                                             Register src2) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(rcx));
  if (src1.is(rcx)) {
    movq(kScratchRegister, src1);
  } else if (src2.is(rcx)) {
    movq(kScratchRegister, src2);
  }
  if (!dst.is(src1)) {
    movq(dst, src1);
  }
  SmiToInteger32(rcx, src2);
  orl(rcx, Immediate(kSmiShift));
  sar_cl(dst);  // Shift 32 + original rcx & 0x1f.
  shl(dst, Immediate(kSmiShift));
  if (src1.is(rcx)) {
    movq(src1, kScratchRegister);
  } else if (src2.is(rcx)) {
    movq(src2, kScratchRegister);
  }
}


void MacroAssembler::SelectNonSmi(Register dst,
                                  Register src1,
                                  Register src2,
                                  Label* on_not_smis,
                                  Label::Distance near_jump) {
  ASSERT(!dst.is(kScratchRegister));
  ASSERT(!src1.is(kScratchRegister));
  ASSERT(!src2.is(kScratchRegister));
  ASSERT(!dst.is(src1));
  ASSERT(!dst.is(src2));
  // Both operands must not be smis.
#ifdef DEBUG
  if (allow_stub_calls()) {  // Check contains a stub call.
    Condition not_both_smis = NegateCondition(CheckBothSmi(src1, src2));
    Check(not_both_smis, kBothRegistersWereSmisInSelectNonSmi);
  }
#endif
  STATIC_ASSERT(kSmiTag == 0);
  ASSERT_EQ(0, Smi::FromInt(0));
  movl(kScratchRegister, Immediate(kSmiTagMask));
  and_(kScratchRegister, src1);
  testl(kScratchRegister, src2);
  // If non-zero then both are smis.
  j(not_zero, on_not_smis, near_jump);

  // Exactly one operand is a smi.
  ASSERT_EQ(1, static_cast<int>(kSmiTagMask));
  // kScratchRegister still holds src1 & kSmiTag, which is either zero or one.
  subq(kScratchRegister, Immediate(1));
  // If src1 is a smi, then scratch register all 1s, else it is all 0s.
  movq(dst, src1);
  xor_(dst, src2);
  and_(dst, kScratchRegister);
  // If src1 is a smi, dst holds src1 ^ src2, else it is zero.
  xor_(dst, src1);
  // If src1 is a smi, dst is src2, else it is src1, i.e., the non-smi.
}


SmiIndex MacroAssembler::SmiToIndex(Register dst,
                                    Register src,
                                    int shift) {
  ASSERT(is_uint6(shift));
  // There is a possible optimization if shift is in the range 60-63, but that
  // will (and must) never happen.
  if (!dst.is(src)) {
    movq(dst, src);
  }
  if (shift < kSmiShift) {
    sar(dst, Immediate(kSmiShift - shift));
  } else {
    shl(dst, Immediate(shift - kSmiShift));
  }
  return SmiIndex(dst, times_1);
}

SmiIndex MacroAssembler::SmiToNegativeIndex(Register dst,
                                            Register src,
                                            int shift) {
  // Register src holds a positive smi.
  ASSERT(is_uint6(shift));
  if (!dst.is(src)) {
    movq(dst, src);
  }
  neg(dst);
  if (shift < kSmiShift) {
    sar(dst, Immediate(kSmiShift - shift));
  } else {
    shl(dst, Immediate(shift - kSmiShift));
  }
  return SmiIndex(dst, times_1);
}


void MacroAssembler::AddSmiField(Register dst, const Operand& src) {
  ASSERT_EQ(0, kSmiShift % kBitsPerByte);
  addl(dst, Operand(src, kSmiShift / kBitsPerByte));
}


void MacroAssembler::Push(Smi* source) {
  intptr_t smi = reinterpret_cast<intptr_t>(source);
  if (is_int32(smi)) {
    push(Immediate(static_cast<int32_t>(smi)));
  } else {
    Register constant = GetSmiConstant(source);
    push(constant);
  }
}


void MacroAssembler::PushInt64AsTwoSmis(Register src, Register scratch) {
  movq(scratch, src);
  // High bits.
  shr(src, Immediate(64 - kSmiShift));
  shl(src, Immediate(kSmiShift));
  push(src);
  // Low bits.
  shl(scratch, Immediate(kSmiShift));
  push(scratch);
}


void MacroAssembler::PopInt64AsTwoSmis(Register dst, Register scratch) {
  pop(scratch);
  // Low bits.
  shr(scratch, Immediate(kSmiShift));
  pop(dst);
  shr(dst, Immediate(kSmiShift));
  // High bits.
  shl(dst, Immediate(64 - kSmiShift));
  or_(dst, scratch);
}


void MacroAssembler::Test(const Operand& src, Smi* source) {
  testl(Operand(src, kIntSize), Immediate(source->value()));
}


// ----------------------------------------------------------------------------


void MacroAssembler::LookupNumberStringCache(Register object,
                                             Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch1;
  Register scratch = scratch2;

  // Load the number string cache.
  LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  SmiToInteger32(
      mask, FieldOperand(number_string_cache, FixedArray::kLengthOffset));
  shrl(mask, Immediate(1));
  subq(mask, Immediate(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  JumpIfSmi(object, &is_smi);
  CheckMap(object,
           isolate()->factory()->heap_number_map(),
           not_found,
           DONT_DO_SMI_CHECK);

  STATIC_ASSERT(8 == kDoubleSize);
  movl(scratch, FieldOperand(object, HeapNumber::kValueOffset + 4));
  xor_(scratch, FieldOperand(object, HeapNumber::kValueOffset));
  and_(scratch, mask);
  // Each entry in string cache consists of two pointer sized fields,
  // but times_twice_pointer_size (multiplication by 16) scale factor
  // is not supported by addrmode on x64 platform.
  // So we have to premultiply entry index before lookup.
  shl(scratch, Immediate(kPointerSizeLog2 + 1));

  Register index = scratch;
  Register probe = mask;
  movq(probe,
       FieldOperand(number_string_cache,
                    index,
                    times_1,
                    FixedArray::kHeaderSize));
  JumpIfSmi(probe, not_found);
  movsd(xmm0, FieldOperand(object, HeapNumber::kValueOffset));
  ucomisd(xmm0, FieldOperand(probe, HeapNumber::kValueOffset));
  j(parity_even, not_found);  // Bail out if NaN is involved.
  j(not_equal, not_found);  // The cache did not contain this value.
  jmp(&load_result_from_cache);

  bind(&is_smi);
  SmiToInteger32(scratch, object);
  and_(scratch, mask);
  // Each entry in string cache consists of two pointer sized fields,
  // but times_twice_pointer_size (multiplication by 16) scale factor
  // is not supported by addrmode on x64 platform.
  // So we have to premultiply entry index before lookup.
  shl(scratch, Immediate(kPointerSizeLog2 + 1));

  // Check if the entry is the smi we are looking for.
  cmpq(object,
       FieldOperand(number_string_cache,
                    index,
                    times_1,
                    FixedArray::kHeaderSize));
  j(not_equal, not_found);

  // Get the result from the cache.
  bind(&load_result_from_cache);
  movq(result,
       FieldOperand(number_string_cache,
                    index,
                    times_1,
                    FixedArray::kHeaderSize + kPointerSize));
  IncrementCounter(isolate()->counters()->number_to_string_native(), 1);
}


void MacroAssembler::JumpIfNotString(Register object,
                                     Register object_map,
                                     Label* not_string,
                                     Label::Distance near_jump) {
  Condition is_smi = CheckSmi(object);
  j(is_smi, not_string, near_jump);
  CmpObjectType(object, FIRST_NONSTRING_TYPE, object_map);
  j(above_equal, not_string, near_jump);
}


void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(
    Register first_object,
    Register second_object,
    Register scratch1,
    Register scratch2,
    Label* on_fail,
    Label::Distance near_jump) {
  // Check that both objects are not smis.
  Condition either_smi = CheckEitherSmi(first_object, second_object);
  j(either_smi, on_fail, near_jump);

  // Load instance type for both strings.
  movq(scratch1, FieldOperand(first_object, HeapObject::kMapOffset));
  movq(scratch2, FieldOperand(second_object, HeapObject::kMapOffset));
  movzxbl(scratch1, FieldOperand(scratch1, Map::kInstanceTypeOffset));
  movzxbl(scratch2, FieldOperand(scratch2, Map::kInstanceTypeOffset));

  // Check that both are flat ASCII strings.
  ASSERT(kNotStringTag != 0);
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;

  andl(scratch1, Immediate(kFlatAsciiStringMask));
  andl(scratch2, Immediate(kFlatAsciiStringMask));
  // Interleave the bits to check both scratch1 and scratch2 in one test.
  ASSERT_EQ(0, kFlatAsciiStringMask & (kFlatAsciiStringMask << 3));
  lea(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmpl(scratch1,
       Immediate(kFlatAsciiStringTag + (kFlatAsciiStringTag << 3)));
  j(not_equal, on_fail, near_jump);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(
    Register instance_type,
    Register scratch,
    Label* failure,
    Label::Distance near_jump) {
  if (!scratch.is(instance_type)) {
    movl(scratch, instance_type);
  }

  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;

  andl(scratch, Immediate(kFlatAsciiStringMask));
  cmpl(scratch, Immediate(kStringTag | kSeqStringTag | kOneByteStringTag));
  j(not_equal, failure, near_jump);
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first_object_instance_type,
    Register second_object_instance_type,
    Register scratch1,
    Register scratch2,
    Label* on_fail,
    Label::Distance near_jump) {
  // Load instance type for both strings.
  movq(scratch1, first_object_instance_type);
  movq(scratch2, second_object_instance_type);

  // Check that both are flat ASCII strings.
  ASSERT(kNotStringTag != 0);
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;

  andl(scratch1, Immediate(kFlatAsciiStringMask));
  andl(scratch2, Immediate(kFlatAsciiStringMask));
  // Interleave the bits to check both scratch1 and scratch2 in one test.
  ASSERT_EQ(0, kFlatAsciiStringMask & (kFlatAsciiStringMask << 3));
  lea(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmpl(scratch1,
       Immediate(kFlatAsciiStringTag + (kFlatAsciiStringTag << 3)));
  j(not_equal, on_fail, near_jump);
}


template<class T>
static void JumpIfNotUniqueNameHelper(MacroAssembler* masm,
                                      T operand_or_register,
                                      Label* not_unique_name,
                                      Label::Distance distance) {
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  Label succeed;
  masm->testb(operand_or_register,
              Immediate(kIsNotStringMask | kIsNotInternalizedMask));
  masm->j(zero, &succeed, Label::kNear);
  masm->cmpb(operand_or_register, Immediate(static_cast<uint8_t>(SYMBOL_TYPE)));
  masm->j(not_equal, not_unique_name, distance);

  masm->bind(&succeed);
}


void MacroAssembler::JumpIfNotUniqueName(Operand operand,
                                         Label* not_unique_name,
                                         Label::Distance distance) {
  JumpIfNotUniqueNameHelper<Operand>(this, operand, not_unique_name, distance);
}


void MacroAssembler::JumpIfNotUniqueName(Register reg,
                                         Label* not_unique_name,
                                         Label::Distance distance) {
  JumpIfNotUniqueNameHelper<Register>(this, reg, not_unique_name, distance);
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    movq(dst, src);
  }
}


void MacroAssembler::Move(Register dst, Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Move(dst, Smi::cast(*source));
  } else {
    MoveHeapObject(dst, source);
  }
}


void MacroAssembler::Move(const Operand& dst, Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Move(dst, Smi::cast(*source));
  } else {
    MoveHeapObject(kScratchRegister, source);
    movq(dst, kScratchRegister);
  }
}


void MacroAssembler::Cmp(Register dst, Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else {
    MoveHeapObject(kScratchRegister, source);
    cmpq(dst, kScratchRegister);
  }
}


void MacroAssembler::Cmp(const Operand& dst, Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else {
    MoveHeapObject(kScratchRegister, source);
    cmpq(dst, kScratchRegister);
  }
}


void MacroAssembler::Push(Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Push(Smi::cast(*source));
  } else {
    MoveHeapObject(kScratchRegister, source);
    push(kScratchRegister);
  }
}


void MacroAssembler::MoveHeapObject(Register result,
                                    Handle<Object> object) {
  AllowDeferredHandleDereference using_raw_address;
  ASSERT(object->IsHeapObject());
  if (isolate()->heap()->InNewSpace(*object)) {
    Handle<Cell> cell = isolate()->factory()->NewCell(object);
    movq(result, cell, RelocInfo::CELL);
    movq(result, Operand(result, 0));
  } else {
    movq(result, object, RelocInfo::EMBEDDED_OBJECT);
  }
}


void MacroAssembler::LoadGlobalCell(Register dst, Handle<Cell> cell) {
  if (dst.is(rax)) {
    AllowDeferredHandleDereference embedding_raw_address;
    load_rax(cell.location(), RelocInfo::CELL);
  } else {
    movq(dst, cell, RelocInfo::CELL);
    movq(dst, Operand(dst, 0));
  }
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    addq(rsp, Immediate(stack_elements * kPointerSize));
  }
}


void MacroAssembler::TestBit(const Operand& src, int bits) {
  int byte_offset = bits / kBitsPerByte;
  int bit_in_byte = bits & (kBitsPerByte - 1);
  testb(Operand(src, byte_offset), Immediate(1 << bit_in_byte));
}


void MacroAssembler::Jump(ExternalReference ext) {
  LoadAddress(kScratchRegister, ext);
  jmp(kScratchRegister);
}


void MacroAssembler::Jump(Address destination, RelocInfo::Mode rmode) {
  movq(kScratchRegister, destination, rmode);
  jmp(kScratchRegister);
}


void MacroAssembler::Jump(Handle<Code> code_object, RelocInfo::Mode rmode) {
  // TODO(X64): Inline this
  jmp(code_object, rmode);
}


int MacroAssembler::CallSize(ExternalReference ext) {
  // Opcode for call kScratchRegister is: Rex.B FF D4 (three bytes).
  return LoadAddressSize(ext) +
         Assembler::kCallScratchRegisterInstructionLength;
}


void MacroAssembler::Call(ExternalReference ext) {
#ifdef DEBUG
  int end_position = pc_offset() + CallSize(ext);
#endif
  LoadAddress(kScratchRegister, ext);
  call(kScratchRegister);
#ifdef DEBUG
  CHECK_EQ(end_position, pc_offset());
#endif
}


void MacroAssembler::Call(Address destination, RelocInfo::Mode rmode) {
#ifdef DEBUG
  int end_position = pc_offset() + CallSize(destination, rmode);
#endif
  movq(kScratchRegister, destination, rmode);
  call(kScratchRegister);
#ifdef DEBUG
  CHECK_EQ(pc_offset(), end_position);
#endif
}


void MacroAssembler::Call(Handle<Code> code_object,
                          RelocInfo::Mode rmode,
                          TypeFeedbackId ast_id) {
#ifdef DEBUG
  int end_position = pc_offset() + CallSize(code_object);
#endif
  ASSERT(RelocInfo::IsCodeTarget(rmode) ||
      rmode == RelocInfo::CODE_AGE_SEQUENCE);
  call(code_object, rmode, ast_id);
#ifdef DEBUG
  CHECK_EQ(end_position, pc_offset());
#endif
}


void MacroAssembler::Pushad() {
  push(rax);
  push(rcx);
  push(rdx);
  push(rbx);
  // Not pushing rsp or rbp.
  push(rsi);
  push(rdi);
  push(r8);
  push(r9);
  // r10 is kScratchRegister.
  push(r11);
  // r12 is kSmiConstantRegister.
  // r13 is kRootRegister.
  push(r14);
  push(r15);
  STATIC_ASSERT(11 == kNumSafepointSavedRegisters);
  // Use lea for symmetry with Popad.
  int sp_delta =
      (kNumSafepointRegisters - kNumSafepointSavedRegisters) * kPointerSize;
  lea(rsp, Operand(rsp, -sp_delta));
}


void MacroAssembler::Popad() {
  // Popad must not change the flags, so use lea instead of addq.
  int sp_delta =
      (kNumSafepointRegisters - kNumSafepointSavedRegisters) * kPointerSize;
  lea(rsp, Operand(rsp, sp_delta));
  pop(r15);
  pop(r14);
  pop(r11);
  pop(r9);
  pop(r8);
  pop(rdi);
  pop(rsi);
  pop(rbx);
  pop(rdx);
  pop(rcx);
  pop(rax);
}


void MacroAssembler::Dropad() {
  addq(rsp, Immediate(kNumSafepointRegisters * kPointerSize));
}


// Order general registers are pushed by Pushad:
// rax, rcx, rdx, rbx, rsi, rdi, r8, r9, r11, r14, r15.
const int
MacroAssembler::kSafepointPushRegisterIndices[Register::kNumRegisters] = {
    0,
    1,
    2,
    3,
    -1,
    -1,
    4,
    5,
    6,
    7,
    -1,
    8,
    -1,
    -1,
    9,
    10
};


void MacroAssembler::StoreToSafepointRegisterSlot(Register dst,
                                                  const Immediate& imm) {
  movq(SafepointRegisterSlot(dst), imm);
}


void MacroAssembler::StoreToSafepointRegisterSlot(Register dst, Register src) {
  movq(SafepointRegisterSlot(dst), src);
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  movq(dst, SafepointRegisterSlot(src));
}


Operand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return Operand(rsp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


void MacroAssembler::PushTryHandler(StackHandler::Kind kind,
                                    int handler_index) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize +
                                                kFPOnStackSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // We will build up the handler from the bottom by pushing on the stack.
  // First push the frame pointer and context.
  if (kind == StackHandler::JS_ENTRY) {
    // The frame pointer does not point to a JS frame so we save NULL for
    // rbp. We expect the code throwing an exception to check rbp before
    // dereferencing it to restore the context.
    push(Immediate(0));  // NULL frame pointer.
    Push(Smi::FromInt(0));  // No context.
  } else {
    push(rbp);
    push(rsi);
  }

  // Push the state and the code object.
  unsigned state =
      StackHandler::IndexField::encode(handler_index) |
      StackHandler::KindField::encode(kind);
  push(Immediate(state));
  Push(CodeObject());

  // Link the current handler as the next handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  push(ExternalOperand(handler_address));
  // Set this new handler as the current one.
  movq(ExternalOperand(handler_address), rsp);
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  pop(ExternalOperand(handler_address));
  addq(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}


void MacroAssembler::JumpToHandlerEntry() {
  // Compute the handler entry address and jump to it.  The handler table is
  // a fixed array of (smi-tagged) code offsets.
  // rax = exception, rdi = code object, rdx = state.
  movq(rbx, FieldOperand(rdi, Code::kHandlerTableOffset));
  shr(rdx, Immediate(StackHandler::kKindWidth));
  movq(rdx,
       FieldOperand(rbx, rdx, times_pointer_size, FixedArray::kHeaderSize));
  SmiToInteger64(rdx, rdx);
  lea(rdi, FieldOperand(rdi, rdx, times_1, Code::kHeaderSize));
  jmp(rdi);
}


void MacroAssembler::Throw(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize +
                                                kFPOnStackSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in rax.
  if (!value.is(rax)) {
    movq(rax, value);
  }
  // Drop the stack pointer to the top of the top handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  movq(rsp, ExternalOperand(handler_address));
  // Restore the next handler.
  pop(ExternalOperand(handler_address));

  // Remove the code object and state, compute the handler address in rdi.
  pop(rdi);  // Code object.
  pop(rdx);  // Offset and state.

  // Restore the context and frame pointer.
  pop(rsi);  // Context.
  pop(rbp);  // Frame pointer.

  // If the handler is a JS frame, restore the context to the frame.
  // (kind == ENTRY) == (rbp == 0) == (rsi == 0), so we could test either
  // rbp or rsi.
  Label skip;
  testq(rsi, rsi);
  j(zero, &skip, Label::kNear);
  movq(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
  bind(&skip);

  JumpToHandlerEntry();
}


void MacroAssembler::ThrowUncatchable(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 4 * kPointerSize +
                                                kFPOnStackSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in rax.
  if (!value.is(rax)) {
    movq(rax, value);
  }
  // Drop the stack pointer to the top of the top stack handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  Load(rsp, handler_address);

  // Unwind the handlers until the top ENTRY handler is found.
  Label fetch_next, check_kind;
  jmp(&check_kind, Label::kNear);
  bind(&fetch_next);
  movq(rsp, Operand(rsp, StackHandlerConstants::kNextOffset));

  bind(&check_kind);
  STATIC_ASSERT(StackHandler::JS_ENTRY == 0);
  testl(Operand(rsp, StackHandlerConstants::kStateOffset),
        Immediate(StackHandler::KindField::kMask));
  j(not_zero, &fetch_next);

  // Set the top handler address to next handler past the top ENTRY handler.
  pop(ExternalOperand(handler_address));

  // Remove the code object and state, compute the handler address in rdi.
  pop(rdi);  // Code object.
  pop(rdx);  // Offset and state.

  // Clear the context pointer and frame pointer (0 was saved in the handler).
  pop(rsi);
  pop(rbp);

  JumpToHandlerEntry();
}


void MacroAssembler::Ret() {
  ret(0);
}


void MacroAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    PopReturnAddressTo(scratch);
    addq(rsp, Immediate(bytes_dropped));
    PushReturnAddressFrom(scratch);
    ret(0);
  }
}


void MacroAssembler::FCmp() {
  fucomip();
  fstp(0);
}


void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  movq(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpb(FieldOperand(map, Map::kInstanceTypeOffset),
       Immediate(static_cast<int8_t>(type)));
}


void MacroAssembler::CheckFastElements(Register map,
                                       Label* fail,
                                       Label::Distance distance) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Immediate(Map::kMaximumBitField2FastHoleyElementValue));
  j(above, fail, distance);
}


void MacroAssembler::CheckFastObjectElements(Register map,
                                             Label* fail,
                                             Label::Distance distance) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Immediate(Map::kMaximumBitField2FastHoleySmiElementValue));
  j(below_equal, fail, distance);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Immediate(Map::kMaximumBitField2FastHoleyElementValue));
  j(above, fail, distance);
}


void MacroAssembler::CheckFastSmiElements(Register map,
                                          Label* fail,
                                          Label::Distance distance) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  cmpb(FieldOperand(map, Map::kBitField2Offset),
       Immediate(Map::kMaximumBitField2FastHoleySmiElementValue));
  j(above, fail, distance);
}


void MacroAssembler::StoreNumberToDoubleElements(
    Register maybe_number,
    Register elements,
    Register index,
    XMMRegister xmm_scratch,
    Label* fail,
    int elements_offset) {
  Label smi_value, is_nan, maybe_nan, not_nan, have_double_value, done;

  JumpIfSmi(maybe_number, &smi_value, Label::kNear);

  CheckMap(maybe_number,
           isolate()->factory()->heap_number_map(),
           fail,
           DONT_DO_SMI_CHECK);

  // Double value, canonicalize NaN.
  uint32_t offset = HeapNumber::kValueOffset + sizeof(kHoleNanLower32);
  cmpl(FieldOperand(maybe_number, offset),
       Immediate(kNaNOrInfinityLowerBoundUpper32));
  j(greater_equal, &maybe_nan, Label::kNear);

  bind(&not_nan);
  movsd(xmm_scratch, FieldOperand(maybe_number, HeapNumber::kValueOffset));
  bind(&have_double_value);
  movsd(FieldOperand(elements, index, times_8,
                     FixedDoubleArray::kHeaderSize - elements_offset),
        xmm_scratch);
  jmp(&done);

  bind(&maybe_nan);
  // Could be NaN or Infinity. If fraction is not zero, it's NaN, otherwise
  // it's an Infinity, and the non-NaN code path applies.
  j(greater, &is_nan, Label::kNear);
  cmpl(FieldOperand(maybe_number, HeapNumber::kValueOffset), Immediate(0));
  j(zero, &not_nan);
  bind(&is_nan);
  // Convert all NaNs to the same canonical NaN value when they are stored in
  // the double array.
  Set(kScratchRegister, BitCast<uint64_t>(
      FixedDoubleArray::canonical_not_the_hole_nan_as_double()));
  movq(xmm_scratch, kScratchRegister);
  jmp(&have_double_value, Label::kNear);

  bind(&smi_value);
  // Value is a smi. convert to a double and store.
  // Preserve original value.
  SmiToInteger32(kScratchRegister, maybe_number);
  Cvtlsi2sd(xmm_scratch, kScratchRegister);
  movsd(FieldOperand(elements, index, times_8,
                     FixedDoubleArray::kHeaderSize - elements_offset),
        xmm_scratch);
  bind(&done);
}


void MacroAssembler::CompareMap(Register obj,
                                Handle<Map> map,
                                Label* early_success) {
  Cmp(FieldOperand(obj, HeapObject::kMapOffset), map);
}


void MacroAssembler::CheckMap(Register obj,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  Label success;
  CompareMap(obj, map, &success);
  j(not_equal, fail);
  bind(&success);
}


void MacroAssembler::ClampUint8(Register reg) {
  Label done;
  testl(reg, Immediate(0xFFFFFF00));
  j(zero, &done, Label::kNear);
  setcc(negative, reg);  // 1 if negative, 0 if positive.
  decb(reg);  // 0 if negative, 255 if positive.
  bind(&done);
}


void MacroAssembler::ClampDoubleToUint8(XMMRegister input_reg,
                                        XMMRegister temp_xmm_reg,
                                        Register result_reg) {
  Label done;
  Label conv_failure;
  xorps(temp_xmm_reg, temp_xmm_reg);
  cvtsd2si(result_reg, input_reg);
  testl(result_reg, Immediate(0xFFFFFF00));
  j(zero, &done, Label::kNear);
  cmpl(result_reg, Immediate(0x80000000));
  j(equal, &conv_failure, Label::kNear);
  movl(result_reg, Immediate(0));
  setcc(above, result_reg);
  subl(result_reg, Immediate(1));
  andl(result_reg, Immediate(255));
  jmp(&done, Label::kNear);
  bind(&conv_failure);
  Set(result_reg, 0);
  ucomisd(input_reg, temp_xmm_reg);
  j(below, &done, Label::kNear);
  Set(result_reg, 255);
  bind(&done);
}


void MacroAssembler::LoadUint32(XMMRegister dst,
                                Register src,
                                XMMRegister scratch) {
  if (FLAG_debug_code) {
    cmpq(src, Immediate(0xffffffff));
    Assert(below_equal, kInputGPRIsExpectedToHaveUpper32Cleared);
  }
  cvtqsi2sd(dst, src);
}


void MacroAssembler::SlowTruncateToI(Register result_reg,
                                     Register input_reg,
                                     int offset) {
  DoubleToIStub stub(input_reg, result_reg, offset, true);
  call(stub.GetCode(isolate()), RelocInfo::CODE_TARGET);
}


void MacroAssembler::TruncateHeapNumberToI(Register result_reg,
                                           Register input_reg) {
  Label done;
  movsd(xmm0, FieldOperand(input_reg, HeapNumber::kValueOffset));
  cvttsd2siq(result_reg, xmm0);
  Set(kScratchRegister, V8_UINT64_C(0x8000000000000000));
  cmpq(result_reg, kScratchRegister);
  j(not_equal, &done, Label::kNear);

  // Slow case.
  if (input_reg.is(result_reg)) {
    subq(rsp, Immediate(kDoubleSize));
    movsd(MemOperand(rsp, 0), xmm0);
    SlowTruncateToI(result_reg, rsp, 0);
    addq(rsp, Immediate(kDoubleSize));
  } else {
    SlowTruncateToI(result_reg, input_reg);
  }

  bind(&done);
}


void MacroAssembler::TruncateDoubleToI(Register result_reg,
                                       XMMRegister input_reg) {
  Label done;
  cvttsd2siq(result_reg, input_reg);
  movq(kScratchRegister,
      V8_INT64_C(0x8000000000000000),
      RelocInfo::NONE64);
  cmpq(result_reg, kScratchRegister);
  j(not_equal, &done, Label::kNear);

  subq(rsp, Immediate(kDoubleSize));
  movsd(MemOperand(rsp, 0), input_reg);
  SlowTruncateToI(result_reg, rsp, 0);
  addq(rsp, Immediate(kDoubleSize));

  bind(&done);
}


void MacroAssembler::DoubleToI(Register result_reg,
                               XMMRegister input_reg,
                               XMMRegister scratch,
                               MinusZeroMode minus_zero_mode,
                               Label* conversion_failed,
                               Label::Distance dst) {
  cvttsd2si(result_reg, input_reg);
  Cvtlsi2sd(xmm0, result_reg);
  ucomisd(xmm0, input_reg);
  j(not_equal, conversion_failed, dst);
  j(parity_even, conversion_failed, dst);  // NaN.
  if (minus_zero_mode == FAIL_ON_MINUS_ZERO) {
    Label done;
    // The integer converted back is equal to the original. We
    // only have to test if we got -0 as an input.
    testl(result_reg, result_reg);
    j(not_zero, &done, Label::kNear);
    movmskpd(result_reg, input_reg);
    // Bit 0 contains the sign of the double in input_reg.
    // If input was positive, we are ok and return 0, otherwise
    // jump to conversion_failed.
    andl(result_reg, Immediate(1));
    j(not_zero, conversion_failed, dst);
    bind(&done);
  }
}


void MacroAssembler::TaggedToI(Register result_reg,
                               Register input_reg,
                               XMMRegister temp,
                               MinusZeroMode minus_zero_mode,
                               Label* lost_precision,
                               Label::Distance dst) {
  Label done;
  ASSERT(!temp.is(xmm0));

  // Heap number map check.
  CompareRoot(FieldOperand(input_reg, HeapObject::kMapOffset),
              Heap::kHeapNumberMapRootIndex);
  j(not_equal, lost_precision, dst);

  movsd(xmm0, FieldOperand(input_reg, HeapNumber::kValueOffset));
  cvttsd2si(result_reg, xmm0);
  Cvtlsi2sd(temp, result_reg);
  ucomisd(xmm0, temp);
  RecordComment("Deferred TaggedToI: lost precision");
  j(not_equal, lost_precision, dst);
  RecordComment("Deferred TaggedToI: NaN");
  j(parity_even, lost_precision, dst);  // NaN.
  if (minus_zero_mode == FAIL_ON_MINUS_ZERO) {
    testl(result_reg, result_reg);
    j(not_zero, &done, Label::kNear);
    movmskpd(result_reg, xmm0);
    andl(result_reg, Immediate(1));
    j(not_zero, lost_precision, dst);
  }
  bind(&done);
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  movq(descriptors, FieldOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  movq(dst, FieldOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  movq(dst, FieldOperand(map, Map::kBitField3Offset));
  Move(kScratchRegister, Smi::FromInt(Map::EnumLengthBits::kMask));
  and_(dst, kScratchRegister);
}


void MacroAssembler::DispatchMap(Register obj,
                                 Register unused,
                                 Handle<Map> map,
                                 Handle<Code> success,
                                 SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  Cmp(FieldOperand(obj, HeapObject::kMapOffset), map);
  j(equal, success, RelocInfo::CODE_TARGET);

  bind(&fail);
}


void MacroAssembler::AssertNumber(Register object) {
  if (emit_debug_code()) {
    Label ok;
    Condition is_smi = CheckSmi(object);
    j(is_smi, &ok, Label::kNear);
    Cmp(FieldOperand(object, HeapObject::kMapOffset),
        isolate()->factory()->heap_number_map());
    Check(equal, kOperandIsNotANumber);
    bind(&ok);
  }
}


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    Condition is_smi = CheckSmi(object);
    Check(NegateCondition(is_smi), kOperandIsASmi);
  }
}


void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    Condition is_smi = CheckSmi(object);
    Check(is_smi, kOperandIsNotASmi);
  }
}


void MacroAssembler::AssertSmi(const Operand& object) {
  if (emit_debug_code()) {
    Condition is_smi = CheckSmi(object);
    Check(is_smi, kOperandIsNotASmi);
  }
}


void MacroAssembler::AssertZeroExtended(Register int32_register) {
  if (emit_debug_code()) {
    ASSERT(!int32_register.is(kScratchRegister));
    movq(kScratchRegister, 0x100000000l, RelocInfo::NONE64);
    cmpq(kScratchRegister, int32_register);
    Check(above_equal, k32BitValueInRegisterIsNotZeroExtended);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAString);
    push(object);
    movq(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, FIRST_NONSTRING_TYPE);
    pop(object);
    Check(below, kOperandIsNotAString);
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAName);
    push(object);
    movq(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, LAST_NAME_TYPE);
    pop(object);
    Check(below_equal, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertRootValue(Register src,
                                     Heap::RootListIndex root_value_index,
                                     BailoutReason reason) {
  if (emit_debug_code()) {
    ASSERT(!src.is(kScratchRegister));
    LoadRoot(kScratchRegister, root_value_index);
    cmpq(src, kScratchRegister);
    Check(equal, reason);
  }
}



Condition MacroAssembler::IsObjectStringType(Register heap_object,
                                             Register map,
                                             Register instance_type) {
  movq(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzxbl(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  testb(instance_type, Immediate(kIsNotStringMask));
  return zero;
}


Condition MacroAssembler::IsObjectNameType(Register heap_object,
                                           Register map,
                                           Register instance_type) {
  movq(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzxbl(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  cmpb(instance_type, Immediate(static_cast<uint8_t>(LAST_NAME_TYPE)));
  return below_equal;
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Label* miss,
                                             bool miss_on_bound_function) {
  // Check that the receiver isn't a smi.
  testl(function, Immediate(kSmiTagMask));
  j(zero, miss);

  // Check that the function really is a function.
  CmpObjectType(function, JS_FUNCTION_TYPE, result);
  j(not_equal, miss);

  if (miss_on_bound_function) {
    movq(kScratchRegister,
         FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
    // It's not smi-tagged (stored in the top half of a smi-tagged 8-byte
    // field).
    TestBit(FieldOperand(kScratchRegister,
                         SharedFunctionInfo::kCompilerHintsOffset),
            SharedFunctionInfo::kBoundFunction);
    j(not_zero, miss);
  }

  // Make sure that the function has an instance prototype.
  Label non_instance;
  testb(FieldOperand(result, Map::kBitFieldOffset),
        Immediate(1 << Map::kHasNonInstancePrototype));
  j(not_zero, &non_instance, Label::kNear);

  // Get the prototype or initial map from the function.
  movq(result,
       FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  CompareRoot(result, Heap::kTheHoleValueRootIndex);
  j(equal, miss);

  // If the function does not have an initial map, we're done.
  Label done;
  CmpObjectType(result, MAP_TYPE, kScratchRegister);
  j(not_equal, &done, Label::kNear);

  // Get the prototype from the initial map.
  movq(result, FieldOperand(result, Map::kPrototypeOffset));
  jmp(&done, Label::kNear);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  bind(&non_instance);
  movq(result, FieldOperand(result, Map::kConstructorOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand counter_operand = ExternalOperand(ExternalReference(counter));
    movl(counter_operand, Immediate(value));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand counter_operand = ExternalOperand(ExternalReference(counter));
    if (value == 1) {
      incl(counter_operand);
    } else {
      addl(counter_operand, Immediate(value));
    }
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand counter_operand = ExternalOperand(ExternalReference(counter));
    if (value == 1) {
      decl(counter_operand);
    } else {
      subl(counter_operand, Immediate(value));
    }
  }
}


#ifdef ENABLE_DEBUGGER_SUPPORT
void MacroAssembler::DebugBreak() {
  Set(rax, 0);  // No arguments.
  LoadAddress(rbx, ExternalReference(Runtime::kDebugBreak, isolate()));
  CEntryStub ces(1);
  ASSERT(AllowThisStubCall(&ces));
  Call(ces.GetCode(isolate()), RelocInfo::DEBUG_BREAK);
}
#endif  // ENABLE_DEBUGGER_SUPPORT


void MacroAssembler::SetCallKind(Register dst, CallKind call_kind) {
  // This macro takes the dst register to make the code more readable
  // at the call sites. However, the dst register has to be rcx to
  // follow the calling convention which requires the call type to be
  // in rcx.
  ASSERT(dst.is(rcx));
  if (call_kind == CALL_AS_FUNCTION) {
    LoadSmiConstant(dst, Smi::FromInt(1));
  } else {
    LoadSmiConstant(dst, Smi::FromInt(0));
  }
}


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper,
                                CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected,
                 actual,
                 Handle<Code>::null(),
                 code,
                 &done,
                 &definitely_mismatches,
                 flag,
                 Label::kNear,
                 call_wrapper,
                 call_kind);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      SetCallKind(rcx, call_kind);
      call(code);
      call_wrapper.AfterCall();
    } else {
      ASSERT(flag == JUMP_FUNCTION);
      SetCallKind(rcx, call_kind);
      jmp(code);
    }
    bind(&done);
  }
}


void MacroAssembler::InvokeCode(Handle<Code> code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                RelocInfo::Mode rmode,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper,
                                CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  Label done;
  bool definitely_mismatches = false;
  Register dummy = rax;
  InvokePrologue(expected,
                 actual,
                 code,
                 dummy,
                 &done,
                 &definitely_mismatches,
                 flag,
                 Label::kNear,
                 call_wrapper,
                 call_kind);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      SetCallKind(rcx, call_kind);
      Call(code, rmode);
      call_wrapper.AfterCall();
    } else {
      ASSERT(flag == JUMP_FUNCTION);
      SetCallKind(rcx, call_kind);
      Jump(code, rmode);
    }
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  ASSERT(function.is(rdi));
  movq(rdx, FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
  movq(rsi, FieldOperand(function, JSFunction::kContextOffset));
  movsxlq(rbx,
          FieldOperand(rdx, SharedFunctionInfo::kFormalParameterCountOffset));
  // Advances rdx to the end of the Code object header, to the start of
  // the executable code.
  movq(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));

  ParameterCount expected(rbx);
  InvokeCode(rdx, expected, actual, flag, call_wrapper, call_kind);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Get the function and setup the context.
  Move(rdi, function);
  movq(rsi, FieldOperand(rdi, JSFunction::kContextOffset));

  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  movq(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
  InvokeCode(rdx, expected, actual, flag, call_wrapper, call_kind);
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_register,
                                    Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    Label::Distance near_jump,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label invoke;
  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      Set(rax, actual.immediate());
      if (expected.immediate() ==
              SharedFunctionInfo::kDontAdaptArgumentsSentinel) {
        // Don't worry about adapting arguments for built-ins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        Set(rbx, expected.immediate());
      }
    }
  } else {
    if (actual.is_immediate()) {
      // Expected is in register, actual is immediate. This is the
      // case when we invoke function values without going through the
      // IC mechanism.
      cmpq(expected.reg(), Immediate(actual.immediate()));
      j(equal, &invoke, Label::kNear);
      ASSERT(expected.reg().is(rbx));
      Set(rax, actual.immediate());
    } else if (!expected.reg().is(actual.reg())) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmpq(expected.reg(), actual.reg());
      j(equal, &invoke, Label::kNear);
      ASSERT(actual.reg().is(rax));
      ASSERT(expected.reg().is(rbx));
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (!code_constant.is_null()) {
      movq(rdx, code_constant, RelocInfo::EMBEDDED_OBJECT);
      addq(rdx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    } else if (!code_register.is(rdx)) {
      movq(rdx, code_register);
    }

    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      SetCallKind(rcx, call_kind);
      Call(adaptor, RelocInfo::CODE_TARGET);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        jmp(done, near_jump);
      }
    } else {
      SetCallKind(rcx, call_kind);
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}


void MacroAssembler::Prologue(PrologueFrameMode frame_mode) {
  if (frame_mode == BUILD_STUB_FRAME) {
    push(rbp);  // Caller's frame pointer.
    movq(rbp, rsp);
    push(rsi);  // Callee's context.
    Push(Smi::FromInt(StackFrame::STUB));
  } else {
    PredictableCodeSizeScope predictible_code_size_scope(this,
        kNoCodeAgeSequenceLength);
    if (isolate()->IsCodePreAgingActive()) {
        // Pre-age the code.
      Call(isolate()->builtins()->MarkCodeAsExecutedOnce(),
           RelocInfo::CODE_AGE_SEQUENCE);
      Nop(kNoCodeAgeSequenceLength - Assembler::kShortCallInstructionLength);
    } else {
      push(rbp);  // Caller's frame pointer.
      movq(rbp, rsp);
      push(rsi);  // Callee's context.
      push(rdi);  // Callee's JS function.
    }
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  push(rbp);
  movq(rbp, rsp);
  push(rsi);  // Context.
  Push(Smi::FromInt(type));
  movq(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  push(kScratchRegister);
  if (emit_debug_code()) {
    movq(kScratchRegister,
         isolate()->factory()->undefined_value(),
         RelocInfo::EMBEDDED_OBJECT);
    cmpq(Operand(rsp, 0), kScratchRegister);
    Check(not_equal, kCodeObjectNotProperlyPatched);
  }
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code()) {
    Move(kScratchRegister, Smi::FromInt(type));
    cmpq(Operand(rbp, StandardFrameConstants::kMarkerOffset), kScratchRegister);
    Check(equal, kStackFrameTypesMustMatch);
  }
  movq(rsp, rbp);
  pop(rbp);
}


void MacroAssembler::EnterExitFramePrologue(bool save_rax) {
  // Set up the frame structure on the stack.
  // All constants are relative to the frame pointer of the exit frame.
  ASSERT(ExitFrameConstants::kCallerSPDisplacement ==
         kFPOnStackSize + kPCOnStackSize);
  ASSERT(ExitFrameConstants::kCallerPCOffset == kFPOnStackSize);
  ASSERT(ExitFrameConstants::kCallerFPOffset == 0 * kPointerSize);
  push(rbp);
  movq(rbp, rsp);

  // Reserve room for entry stack pointer and push the code object.
  ASSERT(ExitFrameConstants::kSPOffset == -1 * kPointerSize);
  push(Immediate(0));  // Saved entry sp, patched before call.
  movq(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  push(kScratchRegister);  // Accessed from EditFrame::code_slot.

  // Save the frame pointer and the context in top.
  if (save_rax) {
    movq(r14, rax);  // Backup rax in callee-save register.
  }

  Store(ExternalReference(Isolate::kCEntryFPAddress, isolate()), rbp);
  Store(ExternalReference(Isolate::kContextAddress, isolate()), rsi);
}


void MacroAssembler::EnterExitFrameEpilogue(int arg_stack_space,
                                            bool save_doubles) {
#ifdef _WIN64
  const int kShadowSpace = 4;
  arg_stack_space += kShadowSpace;
#endif
  // Optionally save all XMM registers.
  if (save_doubles) {
    int space = XMMRegister::kMaxNumAllocatableRegisters * kDoubleSize +
        arg_stack_space * kPointerSize;
    subq(rsp, Immediate(space));
    int offset = -2 * kPointerSize;
    for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); i++) {
      XMMRegister reg = XMMRegister::FromAllocationIndex(i);
      movsd(Operand(rbp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else if (arg_stack_space > 0) {
    subq(rsp, Immediate(arg_stack_space * kPointerSize));
  }

  // Get the required frame alignment for the OS.
  const int kFrameAlignment = OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    ASSERT(IsPowerOf2(kFrameAlignment));
    ASSERT(is_int8(kFrameAlignment));
    and_(rsp, Immediate(-kFrameAlignment));
  }

  // Patch the saved entry sp.
  movq(Operand(rbp, ExitFrameConstants::kSPOffset), rsp);
}


void MacroAssembler::EnterExitFrame(int arg_stack_space, bool save_doubles) {
  EnterExitFramePrologue(true);

  // Set up argv in callee-saved register r15. It is reused in LeaveExitFrame,
  // so it must be retained across the C-call.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  lea(r15, Operand(rbp, r14, times_pointer_size, offset));

  EnterExitFrameEpilogue(arg_stack_space, save_doubles);
}


void MacroAssembler::EnterApiExitFrame(int arg_stack_space) {
  EnterExitFramePrologue(false);
  EnterExitFrameEpilogue(arg_stack_space, false);
}


void MacroAssembler::LeaveExitFrame(bool save_doubles) {
  // Registers:
  // r15 : argv
  if (save_doubles) {
    int offset = -2 * kPointerSize;
    for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); i++) {
      XMMRegister reg = XMMRegister::FromAllocationIndex(i);
      movsd(reg, Operand(rbp, offset - ((i + 1) * kDoubleSize)));
    }
  }
  // Get the return address from the stack and restore the frame pointer.
  movq(rcx, Operand(rbp, 1 * kPointerSize));
  movq(rbp, Operand(rbp, 0 * kPointerSize));

  // Drop everything up to and including the arguments and the receiver
  // from the caller stack.
  lea(rsp, Operand(r15, 1 * kPointerSize));

  PushReturnAddressFrom(rcx);

  LeaveExitFrameEpilogue(true);
}


void MacroAssembler::LeaveApiExitFrame(bool restore_context) {
  movq(rsp, rbp);
  pop(rbp);

  LeaveExitFrameEpilogue(restore_context);
}


void MacroAssembler::LeaveExitFrameEpilogue(bool restore_context) {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(Isolate::kContextAddress, isolate());
  Operand context_operand = ExternalOperand(context_address);
  if (restore_context) {
    movq(rsi, context_operand);
  }
#ifdef DEBUG
  movq(context_operand, Immediate(0));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address(Isolate::kCEntryFPAddress,
                                       isolate());
  Operand c_entry_fp_operand = ExternalOperand(c_entry_fp_address);
  movq(c_entry_fp_operand, Immediate(0));
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  ASSERT(!holder_reg.is(scratch));
  ASSERT(!scratch.is(kScratchRegister));
  // Load current lexical context from the stack frame.
  movq(scratch, Operand(rbp, StandardFrameConstants::kContextOffset));

  // When generating debug code, make sure the lexical context is set.
  if (emit_debug_code()) {
    cmpq(scratch, Immediate(0));
    Check(not_equal, kWeShouldNotHaveAnEmptyLexicalContext);
  }
  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  movq(scratch, FieldOperand(scratch, offset));
  movq(scratch, FieldOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    Cmp(FieldOperand(scratch, HeapObject::kMapOffset),
        isolate()->factory()->native_context_map());
    Check(equal, kJSGlobalObjectNativeContextShouldBeANativeContext);
  }

  // Check if both contexts are the same.
  cmpq(scratch, FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  j(equal, &same_contexts);

  // Compare security tokens.
  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Preserve original value of holder_reg.
    push(holder_reg);
    movq(holder_reg,
         FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
    CompareRoot(holder_reg, Heap::kNullValueRootIndex);
    Check(not_equal, kJSGlobalProxyContextShouldNotBeNull);

    // Read the first word and compare to native_context_map(),
    movq(holder_reg, FieldOperand(holder_reg, HeapObject::kMapOffset));
    CompareRoot(holder_reg, Heap::kNativeContextMapRootIndex);
    Check(equal, kJSGlobalObjectNativeContextShouldBeANativeContext);
    pop(holder_reg);
  }

  movq(kScratchRegister,
       FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  int token_offset =
      Context::kHeaderSize + Context::SECURITY_TOKEN_INDEX * kPointerSize;
  movq(scratch, FieldOperand(scratch, token_offset));
  cmpq(scratch, FieldOperand(kScratchRegister, token_offset));
  j(not_equal, miss);

  bind(&same_contexts);
}


void MacroAssembler::GetNumberHash(Register r0, Register scratch) {
  // First of all we assign the hash seed to scratch.
  LoadRoot(scratch, Heap::kHashSeedRootIndex);
  SmiToInteger32(scratch, scratch);

  // Xor original key with a seed.
  xorl(r0, scratch);

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  movl(scratch, r0);
  notl(r0);
  shll(scratch, Immediate(15));
  addl(r0, scratch);
  // hash = hash ^ (hash >> 12);
  movl(scratch, r0);
  shrl(scratch, Immediate(12));
  xorl(r0, scratch);
  // hash = hash + (hash << 2);
  leal(r0, Operand(r0, r0, times_4, 0));
  // hash = hash ^ (hash >> 4);
  movl(scratch, r0);
  shrl(scratch, Immediate(4));
  xorl(r0, scratch);
  // hash = hash * 2057;
  imull(r0, r0, Immediate(2057));
  // hash = hash ^ (hash >> 16);
  movl(scratch, r0);
  shrl(scratch, Immediate(16));
  xorl(r0, scratch);
}



void MacroAssembler::LoadFromNumberDictionary(Label* miss,
                                              Register elements,
                                              Register key,
                                              Register r0,
                                              Register r1,
                                              Register r2,
                                              Register result) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // Scratch registers:
  //
  // r0 - holds the untagged key on entry and holds the hash once computed.
  //
  // r1 - used to hold the capacity mask of the dictionary
  //
  // r2 - used for the index into the dictionary.
  //
  // result - holds the result on exit if the load succeeded.
  //          Allowed to be the same as 'key' or 'result'.
  //          Unchanged on bailout so 'key' or 'result' can be used
  //          in further computation.

  Label done;

  GetNumberHash(r0, r1);

  // Compute capacity mask.
  SmiToInteger32(r1, FieldOperand(elements,
                                  SeededNumberDictionary::kCapacityOffset));
  decl(r1);

  // Generate an unrolled loop that performs a few probes before giving up.
  const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Use r2 for index calculations and keep the hash intact in r0.
    movq(r2, r0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      addl(r2, Immediate(SeededNumberDictionary::GetProbeOffset(i)));
    }
    and_(r2, r1);

    // Scale the index by multiplying by the entry size.
    ASSERT(SeededNumberDictionary::kEntrySize == 3);
    lea(r2, Operand(r2, r2, times_2, 0));  // r2 = r2 * 3

    // Check if the key matches.
    cmpq(key, FieldOperand(elements,
                           r2,
                           times_pointer_size,
                           SeededNumberDictionary::kElementsStartOffset));
    if (i != (kProbes - 1)) {
      j(equal, &done);
    } else {
      j(not_equal, miss);
    }
  }

  bind(&done);
  // Check that the value is a normal propety.
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  ASSERT_EQ(NORMAL, 0);
  Test(FieldOperand(elements, r2, times_pointer_size, kDetailsOffset),
       Smi::FromInt(PropertyDetails::TypeField::kMask));
  j(not_zero, miss);

  // Get the value at the masked, scaled index.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  movq(result, FieldOperand(elements, r2, times_pointer_size, kValueOffset));
}


void MacroAssembler::LoadAllocationTopHelper(Register result,
                                             Register scratch,
                                             AllocationFlags flags) {
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Just return if allocation top is already known.
  if ((flags & RESULT_CONTAINS_TOP) != 0) {
    // No use of scratch if allocation top is provided.
    ASSERT(!scratch.is_valid());
#ifdef DEBUG
    // Assert that result actually contains top on entry.
    Operand top_operand = ExternalOperand(allocation_top);
    cmpq(result, top_operand);
    Check(equal, kUnexpectedAllocationTop);
#endif
    return;
  }

  // Move address of new object to result. Use scratch register if available,
  // and keep address in scratch until call to UpdateAllocationTopHelper.
  if (scratch.is_valid()) {
    LoadAddress(scratch, allocation_top);
    movq(result, Operand(scratch, 0));
  } else {
    Load(result, allocation_top);
  }
}


void MacroAssembler::UpdateAllocationTopHelper(Register result_end,
                                               Register scratch,
                                               AllocationFlags flags) {
  if (emit_debug_code()) {
    testq(result_end, Immediate(kObjectAlignmentMask));
    Check(zero, kUnalignedAllocationInNewSpace);
  }

  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Update new top.
  if (scratch.is_valid()) {
    // Scratch already contains address of allocation top.
    movq(Operand(scratch, 0), result_end);
  } else {
    Store(allocation_top, result_end);
  }
}


void MacroAssembler::Allocate(int object_size,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  ASSERT((flags & (RESULT_CONTAINS_TOP | SIZE_IN_WORDS)) == 0);
  ASSERT(object_size <= Page::kMaxNonCodeHeapObjectSize);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      movl(result, Immediate(0x7091));
      if (result_end.is_valid()) {
        movl(result_end, Immediate(0x7191));
      }
      if (scratch.is_valid()) {
        movl(scratch, Immediate(0x7291));
      }
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  if (isolate()->heap_profiler()->is_tracking_allocations()) {
    RecordObjectAllocation(isolate(), result, object_size);
  }

  // Align the next allocation. Storing the filler map without checking top is
  // safe in new-space because the limit of the heap is aligned there.
  if (((flags & DOUBLE_ALIGNMENT) != 0) && FLAG_debug_code) {
    testq(result, Immediate(kDoubleAlignmentMask));
    Check(zero, kAllocationIsNotDoubleAligned);
  }

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  Register top_reg = result_end.is_valid() ? result_end : result;

  if (!top_reg.is(result)) {
    movq(top_reg, result);
  }
  addq(top_reg, Immediate(object_size));
  j(carry, gc_required);
  Operand limit_operand = ExternalOperand(allocation_limit);
  cmpq(top_reg, limit_operand);
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(top_reg, scratch, flags);

  bool tag_result = (flags & TAG_OBJECT) != 0;
  if (top_reg.is(result)) {
    if (tag_result) {
      subq(result, Immediate(object_size - kHeapObjectTag));
    } else {
      subq(result, Immediate(object_size));
    }
  } else if (tag_result) {
    // Tag the result if requested.
    ASSERT(kHeapObjectTag == 1);
    incq(result);
  }
}


void MacroAssembler::Allocate(int header_size,
                              ScaleFactor element_size,
                              Register element_count,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  ASSERT((flags & SIZE_IN_WORDS) == 0);
  lea(result_end, Operand(element_count, element_size, header_size));
  Allocate(result_end, result, result_end, scratch, gc_required, flags);
}


void MacroAssembler::Allocate(Register object_size,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  ASSERT((flags & SIZE_IN_WORDS) == 0);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      movl(result, Immediate(0x7091));
      movl(result_end, Immediate(0x7191));
      if (scratch.is_valid()) {
        movl(scratch, Immediate(0x7291));
      }
      // object_size is left unchanged by this function.
    }
    jmp(gc_required);
    return;
  }
  ASSERT(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  if (isolate()->heap_profiler()->is_tracking_allocations()) {
    RecordObjectAllocation(isolate(), result, object_size);
  }

  // Align the next allocation. Storing the filler map without checking top is
  // safe in new-space because the limit of the heap is aligned there.
  if (((flags & DOUBLE_ALIGNMENT) != 0) && FLAG_debug_code) {
    testq(result, Immediate(kDoubleAlignmentMask));
    Check(zero, kAllocationIsNotDoubleAligned);
  }

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  if (!object_size.is(result_end)) {
    movq(result_end, object_size);
  }
  addq(result_end, result);
  j(carry, gc_required);
  Operand limit_operand = ExternalOperand(allocation_limit);
  cmpq(result_end, limit_operand);
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch, flags);

  // Tag the result if requested.
  if ((flags & TAG_OBJECT) != 0) {
    addq(result, Immediate(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  and_(object, Immediate(~kHeapObjectTagMask));
  Operand top_operand = ExternalOperand(new_space_allocation_top);
#ifdef DEBUG
  cmpq(object, top_operand);
  Check(below, kUndoAllocationOfNonAllocatedMemory);
#endif
  movq(top_operand, object);
}


void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(HeapNumber::kSize, result, scratch, no_reg, gc_required, TAG_OBJECT);

  // Set the map.
  LoadRoot(kScratchRegister, Heap::kHeapNumberMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  const int kHeaderAlignment = SeqTwoByteString::kHeaderSize &
                               kObjectAlignmentMask;
  ASSERT(kShortSize == 2);
  // scratch1 = length * 2 + kObjectAlignmentMask.
  lea(scratch1, Operand(length, length, times_1, kObjectAlignmentMask +
                kHeaderAlignment));
  and_(scratch1, Immediate(~kObjectAlignmentMask));
  if (kHeaderAlignment > 0) {
    subq(scratch1, Immediate(kHeaderAlignment));
  }

  // Allocate two byte string in new space.
  Allocate(SeqTwoByteString::kHeaderSize,
           times_1,
           scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  LoadRoot(kScratchRegister, Heap::kStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
  Integer32ToSmi(scratch1, length);
  movq(FieldOperand(result, String::kLengthOffset), scratch1);
  movq(FieldOperand(result, String::kHashFieldOffset),
       Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         Register length,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  const int kHeaderAlignment = SeqOneByteString::kHeaderSize &
                               kObjectAlignmentMask;
  movl(scratch1, length);
  ASSERT(kCharSize == 1);
  addq(scratch1, Immediate(kObjectAlignmentMask + kHeaderAlignment));
  and_(scratch1, Immediate(~kObjectAlignmentMask));
  if (kHeaderAlignment > 0) {
    subq(scratch1, Immediate(kHeaderAlignment));
  }

  // Allocate ASCII string in new space.
  Allocate(SeqOneByteString::kHeaderSize,
           times_1,
           scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  LoadRoot(kScratchRegister, Heap::kAsciiStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
  Integer32ToSmi(scratch1, length);
  movq(FieldOperand(result, String::kLengthOffset), scratch1);
  movq(FieldOperand(result, String::kHashFieldOffset),
       Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateTwoByteConsString(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kConsStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  Label allocate_new_space, install_map;
  AllocationFlags flags = TAG_OBJECT;

  ExternalReference high_promotion_mode = ExternalReference::
      new_space_high_promotion_mode_active_address(isolate());

  Load(scratch1, high_promotion_mode);
  testb(scratch1, Immediate(1));
  j(zero, &allocate_new_space);
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           static_cast<AllocationFlags>(flags | PRETENURE_OLD_POINTER_SPACE));

  jmp(&install_map);

  bind(&allocate_new_space);
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           flags);

  bind(&install_map);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kConsAsciiStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                          Register scratch1,
                                          Register scratch2,
                                          Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kSlicedStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateAsciiSlicedString(Register result,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kSlicedAsciiStringMapRootIndex);
  movq(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


// Copy memory, byte-by-byte, from source to destination.  Not optimized for
// long or aligned copies.  The contents of scratch and length are destroyed.
// Destination is incremented by length, source, length and scratch are
// clobbered.
// A simpler loop is faster on small copies, but slower on large ones.
// The cld() instruction must have been emitted, to set the direction flag(),
// before calling this function.
void MacroAssembler::CopyBytes(Register destination,
                               Register source,
                               Register length,
                               int min_length,
                               Register scratch) {
  ASSERT(min_length >= 0);
  if (emit_debug_code()) {
    cmpl(length, Immediate(min_length));
    Assert(greater_equal, kInvalidMinLength);
  }
  Label loop, done, short_string, short_loop;

  const int kLongStringLimit = 20;
  if (min_length <= kLongStringLimit) {
    cmpl(length, Immediate(kLongStringLimit));
    j(less_equal, &short_string);
  }

  ASSERT(source.is(rsi));
  ASSERT(destination.is(rdi));
  ASSERT(length.is(rcx));

  // Because source is 8-byte aligned in our uses of this function,
  // we keep source aligned for the rep movs operation by copying the odd bytes
  // at the end of the ranges.
  movq(scratch, length);
  shrl(length, Immediate(kPointerSizeLog2));
  repmovsq();
  // Move remaining bytes of length.
  andl(scratch, Immediate(kPointerSize - 1));
  movq(length, Operand(source, scratch, times_1, -kPointerSize));
  movq(Operand(destination, scratch, times_1, -kPointerSize), length);
  addq(destination, scratch);

  if (min_length <= kLongStringLimit) {
    jmp(&done);

    bind(&short_string);
    if (min_length == 0) {
      testl(length, length);
      j(zero, &done);
    }
    lea(scratch, Operand(destination, length, times_1, 0));

    bind(&short_loop);
    movb(length, Operand(source, 0));
    movb(Operand(destination, 0), length);
    incq(source);
    incq(destination);
    cmpq(destination, scratch);
    j(not_equal, &short_loop);

    bind(&done);
  }
}


void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label loop, entry;
  jmp(&entry);
  bind(&loop);
  movq(Operand(start_offset, 0), filler);
  addq(start_offset, Immediate(kPointerSize));
  bind(&entry);
  cmpq(start_offset, end_offset);
  j(less, &loop);
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    movq(dst, Operand(rsi, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      movq(dst, Operand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in rsi).
    movq(dst, rsi);
  }

  // We should not have found a with context by walking the context
  // chain (i.e., the static scope chain and runtime context chain do
  // not agree).  A variable occurring in such a scope should have
  // slot type LOOKUP and not CONTEXT.
  if (emit_debug_code()) {
    CompareRoot(FieldOperand(dst, HeapObject::kMapOffset),
                Heap::kWithContextMapRootIndex);
    Check(not_equal, kVariableResolvedToWithContext);
  }
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind,
    ElementsKind transitioned_kind,
    Register map_in_out,
    Register scratch,
    Label* no_map_match) {
  // Load the global or builtins object from the current context.
  movq(scratch,
       Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  movq(scratch, FieldOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  movq(scratch, Operand(scratch,
                        Context::SlotOffset(Context::JS_ARRAY_MAPS_INDEX)));

  int offset = expected_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  cmpq(map_in_out, FieldOperand(scratch, offset));
  j(not_equal, no_map_match);

  // Use the transitioned cached map.
  offset = transitioned_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  movq(map_in_out, FieldOperand(scratch, offset));
}


void MacroAssembler::LoadInitialArrayMap(
    Register function_in, Register scratch,
    Register map_out, bool can_have_holes) {
  ASSERT(!function_in.is(map_out));
  Label done;
  movq(map_out, FieldOperand(function_in,
                             JSFunction::kPrototypeOrInitialMapOffset));
  if (!FLAG_smi_only_arrays) {
    ElementsKind kind = can_have_holes ? FAST_HOLEY_ELEMENTS : FAST_ELEMENTS;
    LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                        kind,
                                        map_out,
                                        scratch,
                                        &done);
  } else if (can_have_holes) {
    LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                        FAST_HOLEY_SMI_ELEMENTS,
                                        map_out,
                                        scratch,
                                        &done);
  }
  bind(&done);
}

#ifdef _WIN64
static const int kRegisterPassedArguments = 4;
#else
static const int kRegisterPassedArguments = 6;
#endif

void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  movq(function,
       Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  movq(function, FieldOperand(function, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  movq(function, Operand(function, Context::SlotOffset(index)));
}


void MacroAssembler::LoadArrayFunction(Register function) {
  movq(function,
       Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  movq(function, FieldOperand(function, GlobalObject::kGlobalContextOffset));
  movq(function,
       Operand(function, Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map) {
  // Load the initial map.  The global functions all have initial maps.
  movq(map, FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, isolate()->factory()->meta_map(), &fail, DO_SMI_CHECK);
    jmp(&ok);
    bind(&fail);
    Abort(kGlobalFunctionsMustHaveInitialMap);
    bind(&ok);
  }
}


int MacroAssembler::ArgumentStackSlotsForCFunctionCall(int num_arguments) {
  // On Windows 64 stack slots are reserved by the caller for all arguments
  // including the ones passed in registers, and space is always allocated for
  // the four register arguments even if the function takes fewer than four
  // arguments.
  // On AMD64 ABI (Linux/Mac) the first six arguments are passed in registers
  // and the caller does not reserve stack slots for them.
  ASSERT(num_arguments >= 0);
#ifdef _WIN64
  const int kMinimumStackSlots = kRegisterPassedArguments;
  if (num_arguments < kMinimumStackSlots) return kMinimumStackSlots;
  return num_arguments;
#else
  if (num_arguments < kRegisterPassedArguments) return 0;
  return num_arguments - kRegisterPassedArguments;
#endif
}


void MacroAssembler::PrepareCallCFunction(int num_arguments) {
  int frame_alignment = OS::ActivationFrameAlignment();
  ASSERT(frame_alignment != 0);
  ASSERT(num_arguments >= 0);

  // Make stack end at alignment and allocate space for arguments and old rsp.
  movq(kScratchRegister, rsp);
  ASSERT(IsPowerOf2(frame_alignment));
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  subq(rsp, Immediate((argument_slots_on_stack + 1) * kPointerSize));
  and_(rsp, Immediate(-frame_alignment));
  movq(Operand(rsp, argument_slots_on_stack * kPointerSize), kScratchRegister);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  LoadAddress(rax, function);
  CallCFunction(rax, num_arguments);
}


void MacroAssembler::CallCFunction(Register function, int num_arguments) {
  ASSERT(has_frame());
  // Check stack alignment.
  if (emit_debug_code()) {
    CheckStackAlignment();
  }

  call(function);
  ASSERT(OS::ActivationFrameAlignment() != 0);
  ASSERT(num_arguments >= 0);
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  movq(rsp, Operand(rsp, argument_slots_on_stack * kPointerSize));
}


bool AreAliased(Register r1, Register r2, Register r3, Register r4) {
  if (r1.is(r2)) return true;
  if (r1.is(r3)) return true;
  if (r1.is(r4)) return true;
  if (r2.is(r3)) return true;
  if (r2.is(r4)) return true;
  if (r3.is(r4)) return true;
  return false;
}


CodePatcher::CodePatcher(byte* address, int size)
    : address_(address),
      size_(size),
      masm_(NULL, address, size + Assembler::kGap) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  CPU::FlushICache(address_, size_);

  // Check that the code was patched as expected.
  ASSERT(masm_.pc_ == address_ + size_);
  ASSERT(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,
    int mask,
    Condition cc,
    Label* condition_met,
    Label::Distance condition_met_distance) {
  ASSERT(cc == zero || cc == not_zero);
  if (scratch.is(object)) {
    and_(scratch, Immediate(~Page::kPageAlignmentMask));
  } else {
    movq(scratch, Immediate(~Page::kPageAlignmentMask));
    and_(scratch, object);
  }
  if (mask < (1 << kBitsPerByte)) {
    testb(Operand(scratch, MemoryChunk::kFlagsOffset),
          Immediate(static_cast<uint8_t>(mask)));
  } else {
    testl(Operand(scratch, MemoryChunk::kFlagsOffset), Immediate(mask));
  }
  j(cc, condition_met, condition_met_distance);
}


void MacroAssembler::CheckMapDeprecated(Handle<Map> map,
                                        Register scratch,
                                        Label* if_deprecated) {
  if (map->CanBeDeprecated()) {
    Move(scratch, map);
    movq(scratch, FieldOperand(scratch, Map::kBitField3Offset));
    SmiToInteger32(scratch, scratch);
    and_(scratch, Immediate(Map::Deprecated::kMask));
    j(not_zero, if_deprecated);
  }
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register bitmap_scratch,
                                 Register mask_scratch,
                                 Label* on_black,
                                 Label::Distance on_black_distance) {
  ASSERT(!AreAliased(object, bitmap_scratch, mask_scratch, rcx));
  GetMarkBits(object, bitmap_scratch, mask_scratch);

  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  // The mask_scratch register contains a 1 at the position of the first bit
  // and a 0 at all other positions, including the position of the second bit.
  movq(rcx, mask_scratch);
  // Make rcx into a mask that covers both marking bits using the operation
  // rcx = mask | (mask << 1).
  lea(rcx, Operand(mask_scratch, mask_scratch, times_2, 0));
  // Note that we are using a 4-byte aligned 8-byte load.
  and_(rcx, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  cmpq(mask_scratch, rcx);
  j(equal, on_black, on_black_distance);
}


// Detect some, but not all, common pointer-free objects.  This is used by the
// incremental write barrier which doesn't care about oddballs (they are always
// marked black immediately so this code is not hit).
void MacroAssembler::JumpIfDataObject(
    Register value,
    Register scratch,
    Label* not_data_object,
    Label::Distance not_data_object_distance) {
  Label is_data_object;
  movq(scratch, FieldOperand(value, HeapObject::kMapOffset));
  CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  j(equal, &is_data_object, Label::kNear);
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  testb(FieldOperand(scratch, Map::kInstanceTypeOffset),
        Immediate(kIsIndirectStringMask | kIsNotStringMask));
  j(not_zero, not_data_object, not_data_object_distance);
  bind(&is_data_object);
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register mask_reg) {
  ASSERT(!AreAliased(addr_reg, bitmap_reg, mask_reg, rcx));
  movq(bitmap_reg, addr_reg);
  // Sign extended 32 bit immediate.
  and_(bitmap_reg, Immediate(~Page::kPageAlignmentMask));
  movq(rcx, addr_reg);
  int shift =
      Bitmap::kBitsPerCellLog2 + kPointerSizeLog2 - Bitmap::kBytesPerCellLog2;
  shrl(rcx, Immediate(shift));
  and_(rcx,
       Immediate((Page::kPageAlignmentMask >> shift) &
                 ~(Bitmap::kBytesPerCell - 1)));

  addq(bitmap_reg, rcx);
  movq(rcx, addr_reg);
  shrl(rcx, Immediate(kPointerSizeLog2));
  and_(rcx, Immediate((1 << Bitmap::kBitsPerCellLog2) - 1));
  movl(mask_reg, Immediate(1));
  shl_cl(mask_reg);
}


void MacroAssembler::EnsureNotWhite(
    Register value,
    Register bitmap_scratch,
    Register mask_scratch,
    Label* value_is_white_and_not_data,
    Label::Distance distance) {
  ASSERT(!AreAliased(value, bitmap_scratch, mask_scratch, rcx));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);
  ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  Label done;

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  testq(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);
  j(not_zero, &done, Label::kNear);

  if (emit_debug_code()) {
    // Check for impossible bit pattern.
    Label ok;
    push(mask_scratch);
    // shl.  May overflow making the check conservative.
    addq(mask_scratch, mask_scratch);
    testq(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
    pop(mask_scratch);
  }

  // Value is white.  We check whether it is data that doesn't need scanning.
  // Currently only checks for HeapNumber and non-cons strings.
  Register map = rcx;  // Holds map while checking type.
  Register length = rcx;  // Holds length of object after checking type.
  Label not_heap_number;
  Label is_data_object;

  // Check for heap-number
  movq(map, FieldOperand(value, HeapObject::kMapOffset));
  CompareRoot(map, Heap::kHeapNumberMapRootIndex);
  j(not_equal, &not_heap_number, Label::kNear);
  movq(length, Immediate(HeapNumber::kSize));
  jmp(&is_data_object, Label::kNear);

  bind(&not_heap_number);
  // Check for strings.
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  Register instance_type = rcx;
  movzxbl(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  testb(instance_type, Immediate(kIsIndirectStringMask | kIsNotStringMask));
  j(not_zero, value_is_white_and_not_data);
  // It's a non-indirect (non-cons and non-slice) string.
  // If it's external, the length is just ExternalString::kSize.
  // Otherwise it's String::kHeaderSize + string->length() * (1 or 2).
  Label not_external;
  // External strings are the only ones with the kExternalStringTag bit
  // set.
  ASSERT_EQ(0, kSeqStringTag & kExternalStringTag);
  ASSERT_EQ(0, kConsStringTag & kExternalStringTag);
  testb(instance_type, Immediate(kExternalStringTag));
  j(zero, &not_external, Label::kNear);
  movq(length, Immediate(ExternalString::kSize));
  jmp(&is_data_object, Label::kNear);

  bind(&not_external);
  // Sequential string, either ASCII or UC16.
  ASSERT(kOneByteStringTag == 0x04);
  and_(length, Immediate(kStringEncodingMask));
  xor_(length, Immediate(kStringEncodingMask));
  addq(length, Immediate(0x04));
  // Value now either 4 (if ASCII) or 8 (if UC16), i.e. char-size shifted by 2.
  imul(length, FieldOperand(value, String::kLengthOffset));
  shr(length, Immediate(2 + kSmiTagSize + kSmiShiftSize));
  addq(length, Immediate(SeqString::kHeaderSize + kObjectAlignmentMask));
  and_(length, Immediate(~kObjectAlignmentMask));

  bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  or_(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);

  and_(bitmap_scratch, Immediate(~Page::kPageAlignmentMask));
  addl(Operand(bitmap_scratch, MemoryChunk::kLiveBytesOffset), length);

  bind(&done);
}


void MacroAssembler::CheckEnumCache(Register null_value, Label* call_runtime) {
  Label next, start;
  Register empty_fixed_array_value = r8;
  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  movq(rcx, rax);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  movq(rbx, FieldOperand(rcx, HeapObject::kMapOffset));

  EnumLength(rdx, rbx);
  Cmp(rdx, Smi::FromInt(Map::kInvalidEnumCache));
  j(equal, call_runtime);

  jmp(&start);

  bind(&next);

  movq(rbx, FieldOperand(rcx, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(rdx, rbx);
  Cmp(rdx, Smi::FromInt(0));
  j(not_equal, call_runtime);

  bind(&start);

  // Check that there are no elements. Register rcx contains the current JS
  // object we've reached through the prototype chain.
  cmpq(empty_fixed_array_value,
       FieldOperand(rcx, JSObject::kElementsOffset));
  j(not_equal, call_runtime);

  movq(rcx, FieldOperand(rbx, Map::kPrototypeOffset));
  cmpq(rcx, null_value);
  j(not_equal, &next);
}

void MacroAssembler::TestJSArrayForAllocationMemento(
    Register receiver_reg,
    Register scratch_reg,
    Label* no_memento_found) {
  ExternalReference new_space_start =
      ExternalReference::new_space_start(isolate());
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  lea(scratch_reg, Operand(receiver_reg,
      JSArray::kSize + AllocationMemento::kSize - kHeapObjectTag));
  movq(kScratchRegister, new_space_start);
  cmpq(scratch_reg, kScratchRegister);
  j(less, no_memento_found);
  cmpq(scratch_reg, ExternalOperand(new_space_allocation_top));
  j(greater, no_memento_found);
  CompareRoot(MemOperand(scratch_reg, -AllocationMemento::kSize),
              Heap::kAllocationMementoMapRootIndex);
}


void MacroAssembler::RecordObjectAllocation(Isolate* isolate,
                                            Register object,
                                            Register object_size) {
  FrameScope frame(this, StackFrame::EXIT);
  PushSafepointRegisters();
  PrepareCallCFunction(3);
  // In case object is rdx
  movq(kScratchRegister, object);
  movq(arg_reg_3, object_size);
  movq(arg_reg_2, kScratchRegister);
  movq(arg_reg_1, isolate, RelocInfo::EXTERNAL_REFERENCE);
  CallCFunction(
      ExternalReference::record_object_allocation_function(isolate), 3);
  PopSafepointRegisters();
}


void MacroAssembler::RecordObjectAllocation(Isolate* isolate,
                                            Register object,
                                            int object_size) {
  FrameScope frame(this, StackFrame::EXIT);
  PushSafepointRegisters();
  PrepareCallCFunction(3);
  movq(arg_reg_2, object);
  movq(arg_reg_3, Immediate(object_size));
  movq(arg_reg_1, isolate, RelocInfo::EXTERNAL_REFERENCE);
  CallCFunction(
      ExternalReference::record_object_allocation_function(isolate), 3);
  PopSafepointRegisters();
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64

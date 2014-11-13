// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/heap/heap.h"
#include "src/isolate-inl.h"
#include "src/serialize.h"
#include "src/x64/assembler-x64.h"
#include "src/x64/macro-assembler-x64.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      has_frame_(false),
      root_array_available_(true) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


static const int64_t kInvalidRootRegisterDelta = -1;


int64_t MacroAssembler::RootRegisterDelta(ExternalReference other) {
  if (predictable_code_size() &&
      (other.address() < reinterpret_cast<Address>(isolate()) ||
       other.address() >= reinterpret_cast<Address>(isolate() + 1))) {
    return kInvalidRootRegisterDelta;
  }
  Address roots_register_value = kRootRegisterBias +
      reinterpret_cast<Address>(isolate()->heap()->roots_array_start());

  int64_t delta = kInvalidRootRegisterDelta;  // Bogus initialization.
  if (kPointerSize == kInt64Size) {
    delta = other.address() - roots_register_value;
  } else {
    // For x32, zero extend the address to 64-bit and calculate the delta.
    uint64_t o = static_cast<uint32_t>(
        reinterpret_cast<intptr_t>(other.address()));
    uint64_t r = static_cast<uint32_t>(
        reinterpret_cast<intptr_t>(roots_register_value));
    delta = o - r;
  }
  return delta;
}


Operand MacroAssembler::ExternalOperand(ExternalReference target,
                                        Register scratch) {
  if (root_array_available_ && !serializer_enabled()) {
    int64_t delta = RootRegisterDelta(target);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      return Operand(kRootRegister, static_cast<int32_t>(delta));
    }
  }
  Move(scratch, target);
  return Operand(scratch, 0);
}


void MacroAssembler::Load(Register destination, ExternalReference source) {
  if (root_array_available_ && !serializer_enabled()) {
    int64_t delta = RootRegisterDelta(source);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      movp(destination, Operand(kRootRegister, static_cast<int32_t>(delta)));
      return;
    }
  }
  // Safe code.
  if (destination.is(rax)) {
    load_rax(source);
  } else {
    Move(kScratchRegister, source);
    movp(destination, Operand(kScratchRegister, 0));
  }
}


void MacroAssembler::Store(ExternalReference destination, Register source) {
  if (root_array_available_ && !serializer_enabled()) {
    int64_t delta = RootRegisterDelta(destination);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      movp(Operand(kRootRegister, static_cast<int32_t>(delta)), source);
      return;
    }
  }
  // Safe code.
  if (source.is(rax)) {
    store_rax(destination);
  } else {
    Move(kScratchRegister, destination);
    movp(Operand(kScratchRegister, 0), source);
  }
}


void MacroAssembler::LoadAddress(Register destination,
                                 ExternalReference source) {
  if (root_array_available_ && !serializer_enabled()) {
    int64_t delta = RootRegisterDelta(source);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      leap(destination, Operand(kRootRegister, static_cast<int32_t>(delta)));
      return;
    }
  }
  // Safe code.
  Move(destination, source);
}


int MacroAssembler::LoadAddressSize(ExternalReference source) {
  if (root_array_available_ && !serializer_enabled()) {
    // This calculation depends on the internals of LoadAddress.
    // It's correctness is ensured by the asserts in the Call
    // instruction below.
    int64_t delta = RootRegisterDelta(source);
    if (delta != kInvalidRootRegisterDelta && is_int32(delta)) {
      // Operand is leap(scratch, Operand(kRootRegister, delta));
      // Opcodes : REX.W 8D ModRM Disp8/Disp32  - 4 or 7.
      int size = 4;
      if (!is_int8(static_cast<int32_t>(delta))) {
        size += 3;  // Need full four-byte displacement in lea.
      }
      return size;
    }
  }
  // Size of movp(destination, src);
  return Assembler::kMoveAddressIntoScratchRegisterInstructionLength;
}


void MacroAssembler::PushAddress(ExternalReference source) {
  int64_t address = reinterpret_cast<int64_t>(source.address());
  if (is_int32(address) && !serializer_enabled()) {
    if (emit_debug_code()) {
      Move(kScratchRegister, kZapValue, Assembler::RelocInfoNone());
    }
    Push(Immediate(static_cast<int32_t>(address)));
    return;
  }
  LoadAddress(kScratchRegister, source);
  Push(kScratchRegister);
}


void MacroAssembler::LoadRoot(Register destination, Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  movp(destination, Operand(kRootRegister,
                            (index << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::LoadRootIndexed(Register destination,
                                     Register variable_offset,
                                     int fixed_offset) {
  DCHECK(root_array_available_);
  movp(destination,
       Operand(kRootRegister,
               variable_offset, times_pointer_size,
               (fixed_offset << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::StoreRoot(Register source, Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  movp(Operand(kRootRegister, (index << kPointerSizeLog2) - kRootRegisterBias),
       source);
}


void MacroAssembler::PushRoot(Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  Push(Operand(kRootRegister, (index << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::CompareRoot(Register with, Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  cmpp(with, Operand(kRootRegister,
                     (index << kPointerSizeLog2) - kRootRegisterBias));
}


void MacroAssembler::CompareRoot(const Operand& with,
                                 Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  DCHECK(!with.AddressUsesRegister(kScratchRegister));
  LoadRoot(kScratchRegister, index);
  cmpp(with, kScratchRegister);
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
  movp(Operand(scratch, 0), addr);
  // Increment buffer top.
  addp(scratch, Immediate(kPointerSize));
  // Write back new top of buffer.
  StoreRoot(scratch, Heap::kStoreBufferTopRootIndex);
  // Call stub on end of buffer.
  Label done;
  // Check for end of buffer.
  testp(scratch, Immediate(StoreBuffer::kStoreBufferOverflowBit));
  if (and_then == kReturnAtEnd) {
    Label buffer_overflowed;
    j(not_equal, &buffer_overflowed, Label::kNear);
    ret(0);
    bind(&buffer_overflowed);
  } else {
    DCHECK(and_then == kFallThroughAtEnd);
    j(equal, &done, Label::kNear);
  }
  StoreBufferOverflowStub store_buffer_overflow(isolate(), save_fp);
  CallStub(&store_buffer_overflow);
  if (and_then == kReturnAtEnd) {
    ret(0);
  } else {
    DCHECK(and_then == kFallThroughAtEnd);
    bind(&done);
  }
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cc,
                                Label* branch,
                                Label::Distance distance) {
  if (serializer_enabled()) {
    // Can't do arithmetic on external references if it might get serialized.
    // The mask isn't really an address.  We load it as an external reference in
    // case the size of the new space is different between the snapshot maker
    // and the running system.
    if (scratch.is(object)) {
      Move(kScratchRegister, ExternalReference::new_space_mask(isolate()));
      andp(scratch, kScratchRegister);
    } else {
      Move(scratch, ExternalReference::new_space_mask(isolate()));
      andp(scratch, object);
    }
    Move(kScratchRegister, ExternalReference::new_space_start(isolate()));
    cmpp(scratch, kScratchRegister);
    j(cc, branch, distance);
  } else {
    DCHECK(kPointerSize == kInt64Size
        ? is_int32(static_cast<int64_t>(isolate()->heap()->NewSpaceMask()))
        : kPointerSize == kInt32Size);
    intptr_t new_space_start =
        reinterpret_cast<intptr_t>(isolate()->heap()->NewSpaceStart());
    Move(kScratchRegister, reinterpret_cast<Address>(-new_space_start),
         Assembler::RelocInfoNone());
    if (scratch.is(object)) {
      addp(scratch, kScratchRegister);
    } else {
      leap(scratch, Operand(object, kScratchRegister, times_1, 0));
    }
    andp(scratch,
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
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  leap(dst, FieldOperand(object, offset));
  if (emit_debug_code()) {
    Label ok;
    testb(dst, Immediate((1 << kPointerSizeLog2) - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, dst, value, save_fp, remembered_set_action,
              OMIT_SMI_CHECK, pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Move(value, kZapValue, Assembler::RelocInfoNone());
    Move(dst, kZapValue, Assembler::RelocInfoNone());
  }
}


void MacroAssembler::RecordWriteArray(
    Register object,
    Register value,
    Register index,
    SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Array access: calculate the destination address. Index is not a smi.
  Register dst = index;
  leap(dst, Operand(object, index, times_pointer_size,
                   FixedArray::kHeaderSize - kHeapObjectTag));

  RecordWrite(object, dst, value, save_fp, remembered_set_action,
              OMIT_SMI_CHECK, pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Move(value, kZapValue, Assembler::RelocInfoNone());
    Move(index, kZapValue, Assembler::RelocInfoNone());
  }
}


void MacroAssembler::RecordWriteForMap(Register object,
                                       Register map,
                                       Register dst,
                                       SaveFPRegsMode fp_mode) {
  DCHECK(!object.is(kScratchRegister));
  DCHECK(!object.is(map));
  DCHECK(!object.is(dst));
  DCHECK(!map.is(dst));
  AssertNotSmi(object);

  if (emit_debug_code()) {
    Label ok;
    if (map.is(kScratchRegister)) pushq(map);
    CompareMap(map, isolate()->factory()->meta_map());
    if (map.is(kScratchRegister)) popq(map);
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  if (!FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    Label ok;
    if (map.is(kScratchRegister)) pushq(map);
    cmpp(map, FieldOperand(object, HeapObject::kMapOffset));
    if (map.is(kScratchRegister)) popq(map);
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // Compute the address.
  leap(dst, FieldOperand(object, HeapObject::kMapOffset));

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  CheckPageFlag(map,
                map,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  RecordWriteStub stub(isolate(), object, map, dst, OMIT_REMEMBERED_SET,
                       fp_mode);
  CallStub(&stub);

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Move(dst, kZapValue, Assembler::RelocInfoNone());
    Move(map, kZapValue, Assembler::RelocInfoNone());
  }
}


void MacroAssembler::RecordWrite(
    Register object,
    Register address,
    Register value,
    SaveFPRegsMode fp_mode,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  DCHECK(!object.is(value));
  DCHECK(!object.is(address));
  DCHECK(!value.is(address));
  AssertNotSmi(object);

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    Label ok;
    cmpp(value, Operand(address, 0));
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

  if (pointers_to_here_check_for_value != kPointersToHereAreAlwaysInteresting) {
    CheckPageFlag(value,
                  value,  // Used as scratch.
                  MemoryChunk::kPointersToHereAreInterestingMask,
                  zero,
                  &done,
                  Label::kNear);
  }

  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Move(address, kZapValue, Assembler::RelocInfoNone());
    Move(value, kZapValue, Assembler::RelocInfoNone());
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
  int frame_alignment = base::OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    Label alignment_as_expected;
    testp(rsp, Immediate(frame_alignment_mask));
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
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    int3();
    return;
  }
#endif

  Move(kScratchRegister, Smi::FromInt(static_cast<int>(reason)),
       Assembler::RelocInfoNone());
  Push(kScratchRegister);

  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kAbort, 1);
  } else {
    CallRuntime(Runtime::kAbort, 1);
  }
  // Control will not return here.
  int3();
}


void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::StubReturn(int argc) {
  DCHECK(argc >= 1 && generating_stub());
  ret((argc - 1) * kPointerSize);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // The assert checks that the constants for the maximum number of digits
  // for an array index cached in the hash field and the number of bits
  // reserved for it does not conflict.
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  if (!hash.is(index)) {
    movl(index, hash);
  }
  DecodeFieldToSmi<String::ArrayIndexValueBits>(index);
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(rax, num_arguments);
  LoadAddress(rbx, ExternalReference(f, isolate()));
  CEntryStub ces(isolate(), f->result_size, save_doubles);
  CallStub(&ces);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  Set(rax, num_arguments);
  LoadAddress(rbx, ext);

  CEntryStub stub(isolate(), 1);
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
  DCHECK(static_cast<int>(offset) == offset);
  return static_cast<int>(offset);
}


void MacroAssembler::PrepareCallApiFunction(int arg_stack_space) {
  EnterApiExitFrame(arg_stack_space);
}


void MacroAssembler::CallApiFunctionAndReturn(
    Register function_address,
    ExternalReference thunk_ref,
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

  DCHECK(rdx.is(function_address) || r8.is(function_address));
  // Allocate HandleScope in callee-save registers.
  Register prev_next_address_reg = r14;
  Register prev_limit_reg = rbx;
  Register base_reg = r15;
  Move(base_reg, next_address);
  movp(prev_next_address_reg, Operand(base_reg, kNextOffset));
  movp(prev_limit_reg, Operand(base_reg, kLimitOffset));
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
  Move(rax, ExternalReference::is_profiling_address(isolate()));
  cmpb(Operand(rax, 0), Immediate(0));
  j(zero, &profiler_disabled);

  // Third parameter is the address of the actual getter function.
  Move(thunk_last_arg, function_address);
  Move(rax, thunk_ref);
  jmp(&end_profiler_check);

  bind(&profiler_disabled);
  // Call the api function!
  Move(rax, function_address);

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
  movp(rax, return_value_operand);
  bind(&prologue);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  subl(Operand(base_reg, kLevelOffset), Immediate(1));
  movp(Operand(base_reg, kNextOffset), prev_next_address_reg);
  cmpp(prev_limit_reg, Operand(base_reg, kLimitOffset));
  j(not_equal, &delete_allocated_handles);
  bind(&leave_exit_frame);

  // Check if the function scheduled an exception.
  Move(rsi, scheduled_exception_address);
  Cmp(Operand(rsi, 0), factory->the_hole_value());
  j(not_equal, &promote_scheduled_exception);
  bind(&exception_handled);

#if ENABLE_EXTRA_CHECKS
  // Check if the function returned a valid JavaScript value.
  Label ok;
  Register return_value = rax;
  Register map = rcx;

  JumpIfSmi(return_value, &ok, Label::kNear);
  movp(map, FieldOperand(return_value, HeapObject::kMapOffset));

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
    movp(rsi, *context_restore_operand);
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
  movp(Operand(base_reg, kLimitOffset), prev_limit_reg);
  movp(prev_limit_reg, rax);
  LoadAddress(arg_reg_1, ExternalReference::isolate_address(isolate()));
  LoadAddress(rax,
              ExternalReference::delete_handle_scope_extensions(isolate()));
  call(rax);
  movp(rax, prev_limit_reg);
  jmp(&leave_exit_frame);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             int result_size) {
  // Set the entry point and jump to the C entry runtime stub.
  LoadAddress(rbx, ext);
  CEntryStub ces(isolate(), result_size);
  jmp(ces.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Rely on the assertion to check that the number of provided
  // arguments match the expected number of arguments. Fake a
  // parameter count to avoid emitting code to do the check.
  ParameterCount expected(0);
  GetBuiltinEntry(rdx, id);
  InvokeCode(rdx, expected, expected, flag, call_wrapper);
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  movp(target, Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  movp(target, FieldOperand(target, GlobalObject::kBuiltinsOffset));
  movp(target, FieldOperand(target,
                            JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  DCHECK(!target.is(rdi));
  // Load the JavaScript builtin function from the builtins object.
  GetBuiltinFunction(rdi, id);
  movp(target, FieldOperand(rdi, JSFunction::kCodeEntryOffset));
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
      pushq(reg);
    }
  }
  // R12 to r15 are callee save on all platforms.
  if (fp_mode == kSaveFPRegs) {
    subp(rsp, Immediate(kDoubleSize * XMMRegister::kMaxNumRegisters));
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
    addp(rsp, Immediate(kDoubleSize * XMMRegister::kMaxNumRegisters));
  }
  for (int i = kNumberOfSavedRegs - 1; i >= 0; i--) {
    Register reg = saved_regs[i];
    if (!reg.is(exclusion1) && !reg.is(exclusion2) && !reg.is(exclusion3)) {
      popq(reg);
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
  DCHECK(!r.IsDouble());
  if (r.IsInteger8()) {
    movsxbq(dst, src);
  } else if (r.IsUInteger8()) {
    movzxbl(dst, src);
  } else if (r.IsInteger16()) {
    movsxwq(dst, src);
  } else if (r.IsUInteger16()) {
    movzxwl(dst, src);
  } else if (r.IsInteger32()) {
    movl(dst, src);
  } else {
    movp(dst, src);
  }
}


void MacroAssembler::Store(const Operand& dst, Register src, Representation r) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8() || r.IsUInteger8()) {
    movb(dst, src);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    movw(dst, src);
  } else if (r.IsInteger32()) {
    movl(dst, src);
  } else {
    if (r.IsHeapObject()) {
      AssertNotSmi(src);
    } else if (r.IsSmi()) {
      AssertSmi(src);
    }
    movp(dst, src);
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
    movq(dst, x);
  }
}


void MacroAssembler::Set(const Operand& dst, intptr_t x) {
  if (kPointerSize == kInt64Size) {
    if (is_int32(x)) {
      movp(dst, Immediate(static_cast<int32_t>(x)));
    } else {
      Set(kScratchRegister, x);
      movp(dst, kScratchRegister);
    }
  } else {
    movp(dst, Immediate(static_cast<int32_t>(x)));
  }
}


// ----------------------------------------------------------------------------
// Smi tagging, untagging and tag detection.

bool MacroAssembler::IsUnsafeInt(const int32_t x) {
  static const int kMaxBits = 17;
  return !is_intn(x, kMaxBits);
}


void MacroAssembler::SafeMove(Register dst, Smi* src) {
  DCHECK(!dst.is(kScratchRegister));
  if (IsUnsafeInt(src->value()) && jit_cookie() != 0) {
    if (SmiValuesAre32Bits()) {
      // JIT cookie can be converted to Smi.
      Move(dst, Smi::FromInt(src->value() ^ jit_cookie()));
      Move(kScratchRegister, Smi::FromInt(jit_cookie()));
      xorp(dst, kScratchRegister);
    } else {
      DCHECK(SmiValuesAre31Bits());
      int32_t value = static_cast<int32_t>(reinterpret_cast<intptr_t>(src));
      movp(dst, Immediate(value ^ jit_cookie()));
      xorp(dst, Immediate(jit_cookie()));
    }
  } else {
    Move(dst, src);
  }
}


void MacroAssembler::SafePush(Smi* src) {
  if (IsUnsafeInt(src->value()) && jit_cookie() != 0) {
    if (SmiValuesAre32Bits()) {
      // JIT cookie can be converted to Smi.
      Push(Smi::FromInt(src->value() ^ jit_cookie()));
      Move(kScratchRegister, Smi::FromInt(jit_cookie()));
      xorp(Operand(rsp, 0), kScratchRegister);
    } else {
      DCHECK(SmiValuesAre31Bits());
      int32_t value = static_cast<int32_t>(reinterpret_cast<intptr_t>(src));
      Push(Immediate(value ^ jit_cookie()));
      xorp(Operand(rsp, 0), Immediate(jit_cookie()));
    }
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
    Move(dst, Smi::FromInt(kSmiConstantRegisterValue),
         Assembler::RelocInfoNone());
    cmpp(dst, kSmiConstantRegister);
    Assert(equal, kUninitializedKSmiConstantRegister);
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
      leap(dst,
           Operand(kSmiConstantRegister, kSmiConstantRegister, times_8, 0));
      break;
    case 8:
      xorl(dst, dst);
      leap(dst, Operand(dst, kSmiConstantRegister, times_8, 0));
      break;
    case 4:
      xorl(dst, dst);
      leap(dst, Operand(dst, kSmiConstantRegister, times_4, 0));
      break;
    case 5:
      leap(dst,
           Operand(kSmiConstantRegister, kSmiConstantRegister, times_4, 0));
      break;
    case 3:
      leap(dst,
           Operand(kSmiConstantRegister, kSmiConstantRegister, times_2, 0));
      break;
    case 2:
      leap(dst,
           Operand(kSmiConstantRegister, kSmiConstantRegister, times_1, 0));
      break;
    case 1:
      movp(dst, kSmiConstantRegister);
      break;
    case 0:
      UNREACHABLE();
      return;
    default:
      Move(dst, source, Assembler::RelocInfoNone());
      return;
  }
  if (negative) {
    negp(dst);
  }
}


void MacroAssembler::Integer32ToSmi(Register dst, Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  if (!dst.is(src)) {
    movl(dst, src);
  }
  shlp(dst, Immediate(kSmiShift));
}


void MacroAssembler::Integer32ToSmiField(const Operand& dst, Register src) {
  if (emit_debug_code()) {
    testb(dst, Immediate(0x01));
    Label ok;
    j(zero, &ok, Label::kNear);
    Abort(kInteger32ToSmiFieldWritingToNonSmiLocation);
    bind(&ok);
  }

  if (SmiValuesAre32Bits()) {
    DCHECK(kSmiShift % kBitsPerByte == 0);
    movl(Operand(dst, kSmiShift / kBitsPerByte), src);
  } else {
    DCHECK(SmiValuesAre31Bits());
    Integer32ToSmi(kScratchRegister, src);
    movp(dst, kScratchRegister);
  }
}


void MacroAssembler::Integer64PlusConstantToSmi(Register dst,
                                                Register src,
                                                int constant) {
  if (dst.is(src)) {
    addl(dst, Immediate(constant));
  } else {
    leal(dst, Operand(src, constant));
  }
  shlp(dst, Immediate(kSmiShift));
}


void MacroAssembler::SmiToInteger32(Register dst, Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  if (!dst.is(src)) {
    movp(dst, src);
  }

  if (SmiValuesAre32Bits()) {
    shrp(dst, Immediate(kSmiShift));
  } else {
    DCHECK(SmiValuesAre31Bits());
    sarl(dst, Immediate(kSmiShift));
  }
}


void MacroAssembler::SmiToInteger32(Register dst, const Operand& src) {
  if (SmiValuesAre32Bits()) {
    movl(dst, Operand(src, kSmiShift / kBitsPerByte));
  } else {
    DCHECK(SmiValuesAre31Bits());
    movl(dst, src);
    sarl(dst, Immediate(kSmiShift));
  }
}


void MacroAssembler::SmiToInteger64(Register dst, Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  if (!dst.is(src)) {
    movp(dst, src);
  }
  sarp(dst, Immediate(kSmiShift));
  if (kPointerSize == kInt32Size) {
    // Sign extend to 64-bit.
    movsxlq(dst, dst);
  }
}


void MacroAssembler::SmiToInteger64(Register dst, const Operand& src) {
  if (SmiValuesAre32Bits()) {
    movsxlq(dst, Operand(src, kSmiShift / kBitsPerByte));
  } else {
    DCHECK(SmiValuesAre31Bits());
    movp(dst, src);
    SmiToInteger64(dst, dst);
  }
}


void MacroAssembler::SmiTest(Register src) {
  AssertSmi(src);
  testp(src, src);
}


void MacroAssembler::SmiCompare(Register smi1, Register smi2) {
  AssertSmi(smi1);
  AssertSmi(smi2);
  cmpp(smi1, smi2);
}


void MacroAssembler::SmiCompare(Register dst, Smi* src) {
  AssertSmi(dst);
  Cmp(dst, src);
}


void MacroAssembler::Cmp(Register dst, Smi* src) {
  DCHECK(!dst.is(kScratchRegister));
  if (src->value() == 0) {
    testp(dst, dst);
  } else {
    Register constant_reg = GetSmiConstant(src);
    cmpp(dst, constant_reg);
  }
}


void MacroAssembler::SmiCompare(Register dst, const Operand& src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmpp(dst, src);
}


void MacroAssembler::SmiCompare(const Operand& dst, Register src) {
  AssertSmi(dst);
  AssertSmi(src);
  cmpp(dst, src);
}


void MacroAssembler::SmiCompare(const Operand& dst, Smi* src) {
  AssertSmi(dst);
  if (SmiValuesAre32Bits()) {
    cmpl(Operand(dst, kSmiShift / kBitsPerByte), Immediate(src->value()));
  } else {
    DCHECK(SmiValuesAre31Bits());
    cmpl(dst, Immediate(src));
  }
}


void MacroAssembler::Cmp(const Operand& dst, Smi* src) {
  // The Operand cannot use the smi register.
  Register smi_reg = GetSmiConstant(src);
  DCHECK(!dst.AddressUsesRegister(smi_reg));
  cmpp(dst, smi_reg);
}


void MacroAssembler::SmiCompareInteger32(const Operand& dst, Register src) {
  if (SmiValuesAre32Bits()) {
    cmpl(Operand(dst, kSmiShift / kBitsPerByte), src);
  } else {
    DCHECK(SmiValuesAre31Bits());
    SmiToInteger32(kScratchRegister, dst);
    cmpl(kScratchRegister, src);
  }
}


void MacroAssembler::PositiveSmiTimesPowerOfTwoToInteger64(Register dst,
                                                           Register src,
                                                           int power) {
  DCHECK(power >= 0);
  DCHECK(power < 64);
  if (power == 0) {
    SmiToInteger64(dst, src);
    return;
  }
  if (!dst.is(src)) {
    movp(dst, src);
  }
  if (power < kSmiShift) {
    sarp(dst, Immediate(kSmiShift - power));
  } else if (power > kSmiShift) {
    shlp(dst, Immediate(power - kSmiShift));
  }
}


void MacroAssembler::PositiveSmiDivPowerOfTwoToInteger32(Register dst,
                                                         Register src,
                                                         int power) {
  DCHECK((0 <= power) && (power < 32));
  if (dst.is(src)) {
    shrp(dst, Immediate(power + kSmiShift));
  } else {
    UNIMPLEMENTED();  // Not used.
  }
}


void MacroAssembler::SmiOrIfSmis(Register dst, Register src1, Register src2,
                                 Label* on_not_smis,
                                 Label::Distance near_jump) {
  if (dst.is(src1) || dst.is(src2)) {
    DCHECK(!src1.is(kScratchRegister));
    DCHECK(!src2.is(kScratchRegister));
    movp(kScratchRegister, src1);
    orp(kScratchRegister, src2);
    JumpIfNotSmi(kScratchRegister, on_not_smis, near_jump);
    movp(dst, kScratchRegister);
  } else {
    movp(dst, src1);
    orp(dst, src2);
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
  movp(kScratchRegister, src);
  rolp(kScratchRegister, Immediate(1));
  testb(kScratchRegister, Immediate(3));
  return zero;
}


Condition MacroAssembler::CheckBothSmi(Register first, Register second) {
  if (first.is(second)) {
    return CheckSmi(first);
  }
  STATIC_ASSERT(kSmiTag == 0 && kHeapObjectTag == 1 && kHeapObjectTagMask == 3);
  if (SmiValuesAre32Bits()) {
    leal(kScratchRegister, Operand(first, second, times_1, 0));
    testb(kScratchRegister, Immediate(0x03));
  } else {
    DCHECK(SmiValuesAre31Bits());
    movl(kScratchRegister, first);
    orl(kScratchRegister, second);
    testb(kScratchRegister, Immediate(kSmiTagMask));
  }
  return zero;
}


Condition MacroAssembler::CheckBothNonNegativeSmi(Register first,
                                                  Register second) {
  if (first.is(second)) {
    return CheckNonNegativeSmi(first);
  }
  movp(kScratchRegister, first);
  orp(kScratchRegister, second);
  rolp(kScratchRegister, Immediate(1));
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
  DCHECK(!src.is(kScratchRegister));
  // If we overflow by subtracting one, it's the minimal smi value.
  cmpp(src, kSmiConstantRegister);
  return overflow;
}


Condition MacroAssembler::CheckInteger32ValidSmiValue(Register src) {
  if (SmiValuesAre32Bits()) {
    // A 32-bit integer value can always be converted to a smi.
    return always;
  } else {
    DCHECK(SmiValuesAre31Bits());
    cmpl(src, Immediate(0xc0000000));
    return positive;
  }
}


Condition MacroAssembler::CheckUInteger32ValidSmiValue(Register src) {
  if (SmiValuesAre32Bits()) {
    // An unsigned 32-bit integer value is valid as long as the high bit
    // is not set.
    testl(src, src);
    return positive;
  } else {
    DCHECK(SmiValuesAre31Bits());
    testl(src, Immediate(0xc0000000));
    return zero;
  }
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


void MacroAssembler::JumpIfValidSmiValue(Register src,
                                         Label* on_valid,
                                         Label::Distance near_jump) {
  Condition is_valid = CheckInteger32ValidSmiValue(src);
  j(is_valid, on_valid, near_jump);
}


void MacroAssembler::JumpIfNotValidSmiValue(Register src,
                                            Label* on_invalid,
                                            Label::Distance near_jump) {
  Condition is_valid = CheckInteger32ValidSmiValue(src);
  j(NegateCondition(is_valid), on_invalid, near_jump);
}


void MacroAssembler::JumpIfUIntValidSmiValue(Register src,
                                             Label* on_valid,
                                             Label::Distance near_jump) {
  Condition is_valid = CheckUInteger32ValidSmiValue(src);
  j(is_valid, on_valid, near_jump);
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
      movp(dst, src);
    }
    return;
  } else if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    switch (constant->value()) {
      case 1:
        addp(dst, kSmiConstantRegister);
        return;
      case 2:
        leap(dst, Operand(src, kSmiConstantRegister, times_2, 0));
        return;
      case 4:
        leap(dst, Operand(src, kSmiConstantRegister, times_4, 0));
        return;
      case 8:
        leap(dst, Operand(src, kSmiConstantRegister, times_8, 0));
        return;
      default:
        Register constant_reg = GetSmiConstant(constant);
        addp(dst, constant_reg);
        return;
    }
  } else {
    switch (constant->value()) {
      case 1:
        leap(dst, Operand(src, kSmiConstantRegister, times_1, 0));
        return;
      case 2:
        leap(dst, Operand(src, kSmiConstantRegister, times_2, 0));
        return;
      case 4:
        leap(dst, Operand(src, kSmiConstantRegister, times_4, 0));
        return;
      case 8:
        leap(dst, Operand(src, kSmiConstantRegister, times_8, 0));
        return;
      default:
        LoadSmiConstant(dst, constant);
        addp(dst, src);
        return;
    }
  }
}


void MacroAssembler::SmiAddConstant(const Operand& dst, Smi* constant) {
  if (constant->value() != 0) {
    if (SmiValuesAre32Bits()) {
      addl(Operand(dst, kSmiShift / kBitsPerByte),
           Immediate(constant->value()));
    } else {
      DCHECK(SmiValuesAre31Bits());
      addp(dst, Immediate(constant));
    }
  }
}


void MacroAssembler::SmiAddConstant(Register dst,
                                    Register src,
                                    Smi* constant,
                                    SmiOperationExecutionMode mode,
                                    Label* bailout_label,
                                    Label::Distance near_jump) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movp(dst, src);
    }
  } else if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    LoadSmiConstant(kScratchRegister, constant);
    addp(dst, kScratchRegister);
    if (mode.Contains(BAILOUT_ON_NO_OVERFLOW)) {
      j(no_overflow, bailout_label, near_jump);
      DCHECK(mode.Contains(PRESERVE_SOURCE_REGISTER));
      subp(dst, kScratchRegister);
    } else if (mode.Contains(BAILOUT_ON_OVERFLOW)) {
      if (mode.Contains(PRESERVE_SOURCE_REGISTER)) {
        Label done;
        j(no_overflow, &done, Label::kNear);
        subp(dst, kScratchRegister);
        jmp(bailout_label, near_jump);
        bind(&done);
      } else {
        // Bailout if overflow without reserving src.
        j(overflow, bailout_label, near_jump);
      }
    } else {
      CHECK(mode.IsEmpty());
    }
  } else {
    DCHECK(mode.Contains(PRESERVE_SOURCE_REGISTER));
    DCHECK(mode.Contains(BAILOUT_ON_OVERFLOW));
    LoadSmiConstant(dst, constant);
    addp(dst, src);
    j(overflow, bailout_label, near_jump);
  }
}


void MacroAssembler::SmiSubConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movp(dst, src);
    }
  } else if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    subp(dst, constant_reg);
  } else {
    if (constant->value() == Smi::kMinValue) {
      LoadSmiConstant(dst, constant);
      // Adding and subtracting the min-value gives the same result, it only
      // differs on the overflow bit, which we don't check here.
      addp(dst, src);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(dst, Smi::FromInt(-constant->value()));
      addp(dst, src);
    }
  }
}


void MacroAssembler::SmiSubConstant(Register dst,
                                    Register src,
                                    Smi* constant,
                                    SmiOperationExecutionMode mode,
                                    Label* bailout_label,
                                    Label::Distance near_jump) {
  if (constant->value() == 0) {
    if (!dst.is(src)) {
      movp(dst, src);
    }
  } else if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    LoadSmiConstant(kScratchRegister, constant);
    subp(dst, kScratchRegister);
    if (mode.Contains(BAILOUT_ON_NO_OVERFLOW)) {
      j(no_overflow, bailout_label, near_jump);
      DCHECK(mode.Contains(PRESERVE_SOURCE_REGISTER));
      addp(dst, kScratchRegister);
    } else if (mode.Contains(BAILOUT_ON_OVERFLOW)) {
      if (mode.Contains(PRESERVE_SOURCE_REGISTER)) {
        Label done;
        j(no_overflow, &done, Label::kNear);
        addp(dst, kScratchRegister);
        jmp(bailout_label, near_jump);
        bind(&done);
      } else {
        // Bailout if overflow without reserving src.
        j(overflow, bailout_label, near_jump);
      }
    } else {
      CHECK(mode.IsEmpty());
    }
  } else {
    DCHECK(mode.Contains(PRESERVE_SOURCE_REGISTER));
    DCHECK(mode.Contains(BAILOUT_ON_OVERFLOW));
    if (constant->value() == Smi::kMinValue) {
      DCHECK(!dst.is(kScratchRegister));
      movp(dst, src);
      LoadSmiConstant(kScratchRegister, constant);
      subp(dst, kScratchRegister);
      j(overflow, bailout_label, near_jump);
    } else {
      // Subtract by adding the negation.
      LoadSmiConstant(dst, Smi::FromInt(-(constant->value())));
      addp(dst, src);
      j(overflow, bailout_label, near_jump);
    }
  }
}


void MacroAssembler::SmiNeg(Register dst,
                            Register src,
                            Label* on_smi_result,
                            Label::Distance near_jump) {
  if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    movp(kScratchRegister, src);
    negp(dst);  // Low 32 bits are retained as zero by negation.
    // Test if result is zero or Smi::kMinValue.
    cmpp(dst, kScratchRegister);
    j(not_equal, on_smi_result, near_jump);
    movp(src, kScratchRegister);
  } else {
    movp(dst, src);
    negp(dst);
    cmpp(dst, src);
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
    masm->addp(dst, src2);
    masm->j(no_overflow, &done, Label::kNear);
    // Restore src1.
    masm->subp(dst, src2);
    masm->jmp(on_not_smi_result, near_jump);
    masm->bind(&done);
  } else {
    masm->movp(dst, src1);
    masm->addp(dst, src2);
    masm->j(overflow, on_not_smi_result, near_jump);
  }
}


void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  DCHECK_NOT_NULL(on_not_smi_result);
  DCHECK(!dst.is(src2));
  SmiAddHelper<Register>(this, dst, src1, src2, on_not_smi_result, near_jump);
}


void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            const Operand& src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  DCHECK_NOT_NULL(on_not_smi_result);
  DCHECK(!src2.AddressUsesRegister(dst));
  SmiAddHelper<Operand>(this, dst, src1, src2, on_not_smi_result, near_jump);
}


void MacroAssembler::SmiAdd(Register dst,
                            Register src1,
                            Register src2) {
  // No overflow checking. Use only when it's known that
  // overflowing is impossible.
  if (!dst.is(src1)) {
    if (emit_debug_code()) {
      movp(kScratchRegister, src1);
      addp(kScratchRegister, src2);
      Check(no_overflow, kSmiAdditionOverflow);
    }
    leap(dst, Operand(src1, src2, times_1, 0));
  } else {
    addp(dst, src2);
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
    masm->subp(dst, src2);
    masm->j(no_overflow, &done, Label::kNear);
    // Restore src1.
    masm->addp(dst, src2);
    masm->jmp(on_not_smi_result, near_jump);
    masm->bind(&done);
  } else {
    masm->movp(dst, src1);
    masm->subp(dst, src2);
    masm->j(overflow, on_not_smi_result, near_jump);
  }
}


void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  DCHECK_NOT_NULL(on_not_smi_result);
  DCHECK(!dst.is(src2));
  SmiSubHelper<Register>(this, dst, src1, src2, on_not_smi_result, near_jump);
}


void MacroAssembler::SmiSub(Register dst,
                            Register src1,
                            const Operand& src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  DCHECK_NOT_NULL(on_not_smi_result);
  DCHECK(!src2.AddressUsesRegister(dst));
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
    masm->movp(dst, src1);
  }
  masm->subp(dst, src2);
  masm->Assert(no_overflow, kSmiSubtractionOverflow);
}


void MacroAssembler::SmiSub(Register dst, Register src1, Register src2) {
  DCHECK(!dst.is(src2));
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
  DCHECK(!dst.is(src2));
  DCHECK(!dst.is(kScratchRegister));
  DCHECK(!src1.is(kScratchRegister));
  DCHECK(!src2.is(kScratchRegister));

  if (dst.is(src1)) {
    Label failure, zero_correct_result;
    movp(kScratchRegister, src1);  // Create backup for later testing.
    SmiToInteger64(dst, src1);
    imulp(dst, src2);
    j(overflow, &failure, Label::kNear);

    // Check for negative zero result.  If product is zero, and one
    // argument is negative, go to slow case.
    Label correct_result;
    testp(dst, dst);
    j(not_zero, &correct_result, Label::kNear);

    movp(dst, kScratchRegister);
    xorp(dst, src2);
    // Result was positive zero.
    j(positive, &zero_correct_result, Label::kNear);

    bind(&failure);  // Reused failure exit, restores src1.
    movp(src1, kScratchRegister);
    jmp(on_not_smi_result, near_jump);

    bind(&zero_correct_result);
    Set(dst, 0);

    bind(&correct_result);
  } else {
    SmiToInteger64(dst, src1);
    imulp(dst, src2);
    j(overflow, on_not_smi_result, near_jump);
    // Check for negative zero result.  If product is zero, and one
    // argument is negative, go to slow case.
    Label correct_result;
    testp(dst, dst);
    j(not_zero, &correct_result, Label::kNear);
    // One of src1 and src2 is zero, the check whether the other is
    // negative.
    movp(kScratchRegister, src1);
    xorp(kScratchRegister, src2);
    j(negative, on_not_smi_result, near_jump);
    bind(&correct_result);
  }
}


void MacroAssembler::SmiDiv(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  DCHECK(!src1.is(kScratchRegister));
  DCHECK(!src2.is(kScratchRegister));
  DCHECK(!dst.is(kScratchRegister));
  DCHECK(!src2.is(rax));
  DCHECK(!src2.is(rdx));
  DCHECK(!src1.is(rdx));

  // Check for 0 divisor (result is +/-Infinity).
  testp(src2, src2);
  j(zero, on_not_smi_result, near_jump);

  if (src1.is(rax)) {
    movp(kScratchRegister, src1);
  }
  SmiToInteger32(rax, src1);
  // We need to rule out dividing Smi::kMinValue by -1, since that would
  // overflow in idiv and raise an exception.
  // We combine this with negative zero test (negative zero only happens
  // when dividing zero by a negative number).

  // We overshoot a little and go to slow case if we divide min-value
  // by any negative value, not just -1.
  Label safe_div;
  testl(rax, Immediate(~Smi::kMinValue));
  j(not_zero, &safe_div, Label::kNear);
  testp(src2, src2);
  if (src1.is(rax)) {
    j(positive, &safe_div, Label::kNear);
    movp(src1, kScratchRegister);
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
    movp(src1, kScratchRegister);
    jmp(on_not_smi_result, near_jump);
    bind(&smi_result);
  } else {
    j(not_zero, on_not_smi_result, near_jump);
  }
  if (!dst.is(src1) && src1.is(rax)) {
    movp(src1, kScratchRegister);
  }
  Integer32ToSmi(dst, rax);
}


void MacroAssembler::SmiMod(Register dst,
                            Register src1,
                            Register src2,
                            Label* on_not_smi_result,
                            Label::Distance near_jump) {
  DCHECK(!dst.is(kScratchRegister));
  DCHECK(!src1.is(kScratchRegister));
  DCHECK(!src2.is(kScratchRegister));
  DCHECK(!src2.is(rax));
  DCHECK(!src2.is(rdx));
  DCHECK(!src1.is(rdx));
  DCHECK(!src1.is(src2));

  testp(src2, src2);
  j(zero, on_not_smi_result, near_jump);

  if (src1.is(rax)) {
    movp(kScratchRegister, src1);
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
    movp(src1, kScratchRegister);
  }
  jmp(on_not_smi_result, near_jump);
  bind(&safe_div);

  // Sign extend eax into edx:eax.
  cdq();
  idivl(src2);
  // Restore smi tags on inputs.
  Integer32ToSmi(src2, src2);
  if (src1.is(rax)) {
    movp(src1, kScratchRegister);
  }
  // Check for a negative zero result.  If the result is zero, and the
  // dividend is negative, go slow to return a floating point negative zero.
  Label smi_result;
  testl(rdx, rdx);
  j(not_zero, &smi_result, Label::kNear);
  testp(src1, src1);
  j(negative, on_not_smi_result, near_jump);
  bind(&smi_result);
  Integer32ToSmi(dst, rdx);
}


void MacroAssembler::SmiNot(Register dst, Register src) {
  DCHECK(!dst.is(kScratchRegister));
  DCHECK(!src.is(kScratchRegister));
  if (SmiValuesAre32Bits()) {
    // Set tag and padding bits before negating, so that they are zero
    // afterwards.
    movl(kScratchRegister, Immediate(~0));
  } else {
    DCHECK(SmiValuesAre31Bits());
    movl(kScratchRegister, Immediate(1));
  }
  if (dst.is(src)) {
    xorp(dst, kScratchRegister);
  } else {
    leap(dst, Operand(src, kScratchRegister, times_1, 0));
  }
  notp(dst);
}


void MacroAssembler::SmiAnd(Register dst, Register src1, Register src2) {
  DCHECK(!dst.is(src2));
  if (!dst.is(src1)) {
    movp(dst, src1);
  }
  andp(dst, src2);
}


void MacroAssembler::SmiAndConstant(Register dst, Register src, Smi* constant) {
  if (constant->value() == 0) {
    Set(dst, 0);
  } else if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    andp(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    andp(dst, src);
  }
}


void MacroAssembler::SmiOr(Register dst, Register src1, Register src2) {
  if (!dst.is(src1)) {
    DCHECK(!src1.is(src2));
    movp(dst, src1);
  }
  orp(dst, src2);
}


void MacroAssembler::SmiOrConstant(Register dst, Register src, Smi* constant) {
  if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    orp(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    orp(dst, src);
  }
}


void MacroAssembler::SmiXor(Register dst, Register src1, Register src2) {
  if (!dst.is(src1)) {
    DCHECK(!src1.is(src2));
    movp(dst, src1);
  }
  xorp(dst, src2);
}


void MacroAssembler::SmiXorConstant(Register dst, Register src, Smi* constant) {
  if (dst.is(src)) {
    DCHECK(!dst.is(kScratchRegister));
    Register constant_reg = GetSmiConstant(constant);
    xorp(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    xorp(dst, src);
  }
}


void MacroAssembler::SmiShiftArithmeticRightConstant(Register dst,
                                                     Register src,
                                                     int shift_value) {
  DCHECK(is_uint5(shift_value));
  if (shift_value > 0) {
    if (dst.is(src)) {
      sarp(dst, Immediate(shift_value + kSmiShift));
      shlp(dst, Immediate(kSmiShift));
    } else {
      UNIMPLEMENTED();  // Not used.
    }
  }
}


void MacroAssembler::SmiShiftLeftConstant(Register dst,
                                          Register src,
                                          int shift_value,
                                          Label* on_not_smi_result,
                                          Label::Distance near_jump) {
  if (SmiValuesAre32Bits()) {
    if (!dst.is(src)) {
      movp(dst, src);
    }
    if (shift_value > 0) {
      // Shift amount specified by lower 5 bits, not six as the shl opcode.
      shlq(dst, Immediate(shift_value & 0x1f));
    }
  } else {
    DCHECK(SmiValuesAre31Bits());
    if (dst.is(src)) {
      UNIMPLEMENTED();  // Not used.
    } else {
      SmiToInteger32(dst, src);
      shll(dst, Immediate(shift_value));
      JumpIfNotValidSmiValue(dst, on_not_smi_result, near_jump);
      Integer32ToSmi(dst, dst);
    }
  }
}


void MacroAssembler::SmiShiftLogicalRightConstant(
    Register dst, Register src, int shift_value,
    Label* on_not_smi_result, Label::Distance near_jump) {
  // Logic right shift interprets its result as an *unsigned* number.
  if (dst.is(src)) {
    UNIMPLEMENTED();  // Not used.
  } else {
    if (shift_value == 0) {
      testp(src, src);
      j(negative, on_not_smi_result, near_jump);
    }
    if (SmiValuesAre32Bits()) {
      movp(dst, src);
      shrp(dst, Immediate(shift_value + kSmiShift));
      shlp(dst, Immediate(kSmiShift));
    } else {
      DCHECK(SmiValuesAre31Bits());
      SmiToInteger32(dst, src);
      shrp(dst, Immediate(shift_value));
      JumpIfUIntNotValidSmiValue(dst, on_not_smi_result, near_jump);
      Integer32ToSmi(dst, dst);
    }
  }
}


void MacroAssembler::SmiShiftLeft(Register dst,
                                  Register src1,
                                  Register src2,
                                  Label* on_not_smi_result,
                                  Label::Distance near_jump) {
  if (SmiValuesAre32Bits()) {
    DCHECK(!dst.is(rcx));
    if (!dst.is(src1)) {
      movp(dst, src1);
    }
    // Untag shift amount.
    SmiToInteger32(rcx, src2);
    // Shift amount specified by lower 5 bits, not six as the shl opcode.
    andp(rcx, Immediate(0x1f));
    shlq_cl(dst);
  } else {
    DCHECK(SmiValuesAre31Bits());
    DCHECK(!dst.is(kScratchRegister));
    DCHECK(!src1.is(kScratchRegister));
    DCHECK(!src2.is(kScratchRegister));
    DCHECK(!dst.is(src2));
    DCHECK(!dst.is(rcx));

    if (src1.is(rcx) || src2.is(rcx)) {
      movq(kScratchRegister, rcx);
    }
    if (dst.is(src1)) {
      UNIMPLEMENTED();  // Not used.
    } else {
      Label valid_result;
      SmiToInteger32(dst, src1);
      SmiToInteger32(rcx, src2);
      shll_cl(dst);
      JumpIfValidSmiValue(dst, &valid_result, Label::kNear);
      // As src1 or src2 could not be dst, we do not need to restore them for
      // clobbering dst.
      if (src1.is(rcx) || src2.is(rcx)) {
        if (src1.is(rcx)) {
          movq(src1, kScratchRegister);
        } else {
          movq(src2, kScratchRegister);
        }
      }
      jmp(on_not_smi_result, near_jump);
      bind(&valid_result);
      Integer32ToSmi(dst, dst);
    }
  }
}


void MacroAssembler::SmiShiftLogicalRight(Register dst,
                                          Register src1,
                                          Register src2,
                                          Label* on_not_smi_result,
                                          Label::Distance near_jump) {
  DCHECK(!dst.is(kScratchRegister));
  DCHECK(!src1.is(kScratchRegister));
  DCHECK(!src2.is(kScratchRegister));
  DCHECK(!dst.is(src2));
  DCHECK(!dst.is(rcx));
  if (src1.is(rcx) || src2.is(rcx)) {
    movq(kScratchRegister, rcx);
  }
  if (dst.is(src1)) {
    UNIMPLEMENTED();  // Not used.
  } else {
    Label valid_result;
    SmiToInteger32(dst, src1);
    SmiToInteger32(rcx, src2);
    shrl_cl(dst);
    JumpIfUIntValidSmiValue(dst, &valid_result, Label::kNear);
    // As src1 or src2 could not be dst, we do not need to restore them for
    // clobbering dst.
    if (src1.is(rcx) || src2.is(rcx)) {
      if (src1.is(rcx)) {
        movq(src1, kScratchRegister);
      } else {
        movq(src2, kScratchRegister);
      }
     }
    jmp(on_not_smi_result, near_jump);
    bind(&valid_result);
    Integer32ToSmi(dst, dst);
  }
}


void MacroAssembler::SmiShiftArithmeticRight(Register dst,
                                             Register src1,
                                             Register src2) {
  DCHECK(!dst.is(kScratchRegister));
  DCHECK(!src1.is(kScratchRegister));
  DCHECK(!src2.is(kScratchRegister));
  DCHECK(!dst.is(rcx));

  SmiToInteger32(rcx, src2);
  if (!dst.is(src1)) {
    movp(dst, src1);
  }
  SmiToInteger32(dst, dst);
  sarl_cl(dst);
  Integer32ToSmi(dst, dst);
}


void MacroAssembler::SelectNonSmi(Register dst,
                                  Register src1,
                                  Register src2,
                                  Label* on_not_smis,
                                  Label::Distance near_jump) {
  DCHECK(!dst.is(kScratchRegister));
  DCHECK(!src1.is(kScratchRegister));
  DCHECK(!src2.is(kScratchRegister));
  DCHECK(!dst.is(src1));
  DCHECK(!dst.is(src2));
  // Both operands must not be smis.
#ifdef DEBUG
  Condition not_both_smis = NegateCondition(CheckBothSmi(src1, src2));
  Check(not_both_smis, kBothRegistersWereSmisInSelectNonSmi);
#endif
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK_EQ(0, Smi::FromInt(0));
  movl(kScratchRegister, Immediate(kSmiTagMask));
  andp(kScratchRegister, src1);
  testl(kScratchRegister, src2);
  // If non-zero then both are smis.
  j(not_zero, on_not_smis, near_jump);

  // Exactly one operand is a smi.
  DCHECK_EQ(1, static_cast<int>(kSmiTagMask));
  // kScratchRegister still holds src1 & kSmiTag, which is either zero or one.
  subp(kScratchRegister, Immediate(1));
  // If src1 is a smi, then scratch register all 1s, else it is all 0s.
  movp(dst, src1);
  xorp(dst, src2);
  andp(dst, kScratchRegister);
  // If src1 is a smi, dst holds src1 ^ src2, else it is zero.
  xorp(dst, src1);
  // If src1 is a smi, dst is src2, else it is src1, i.e., the non-smi.
}


SmiIndex MacroAssembler::SmiToIndex(Register dst,
                                    Register src,
                                    int shift) {
  if (SmiValuesAre32Bits()) {
    DCHECK(is_uint6(shift));
    // There is a possible optimization if shift is in the range 60-63, but that
    // will (and must) never happen.
    if (!dst.is(src)) {
      movp(dst, src);
    }
    if (shift < kSmiShift) {
      sarp(dst, Immediate(kSmiShift - shift));
    } else {
      shlp(dst, Immediate(shift - kSmiShift));
    }
    return SmiIndex(dst, times_1);
  } else {
    DCHECK(SmiValuesAre31Bits());
    DCHECK(shift >= times_1 && shift <= (static_cast<int>(times_8) + 1));
    if (!dst.is(src)) {
      movp(dst, src);
    }
    // We have to sign extend the index register to 64-bit as the SMI might
    // be negative.
    movsxlq(dst, dst);
    if (shift == times_1) {
      sarq(dst, Immediate(kSmiShift));
      return SmiIndex(dst, times_1);
    }
    return SmiIndex(dst, static_cast<ScaleFactor>(shift - 1));
  }
}


SmiIndex MacroAssembler::SmiToNegativeIndex(Register dst,
                                            Register src,
                                            int shift) {
  if (SmiValuesAre32Bits()) {
    // Register src holds a positive smi.
    DCHECK(is_uint6(shift));
    if (!dst.is(src)) {
      movp(dst, src);
    }
    negp(dst);
    if (shift < kSmiShift) {
      sarp(dst, Immediate(kSmiShift - shift));
    } else {
      shlp(dst, Immediate(shift - kSmiShift));
    }
    return SmiIndex(dst, times_1);
  } else {
    DCHECK(SmiValuesAre31Bits());
    DCHECK(shift >= times_1 && shift <= (static_cast<int>(times_8) + 1));
    if (!dst.is(src)) {
      movp(dst, src);
    }
    negq(dst);
    if (shift == times_1) {
      sarq(dst, Immediate(kSmiShift));
      return SmiIndex(dst, times_1);
    }
    return SmiIndex(dst, static_cast<ScaleFactor>(shift - 1));
  }
}


void MacroAssembler::AddSmiField(Register dst, const Operand& src) {
  if (SmiValuesAre32Bits()) {
    DCHECK_EQ(0, kSmiShift % kBitsPerByte);
    addl(dst, Operand(src, kSmiShift / kBitsPerByte));
  } else {
    DCHECK(SmiValuesAre31Bits());
    SmiToInteger32(kScratchRegister, src);
    addl(dst, kScratchRegister);
  }
}


void MacroAssembler::Push(Smi* source) {
  intptr_t smi = reinterpret_cast<intptr_t>(source);
  if (is_int32(smi)) {
    Push(Immediate(static_cast<int32_t>(smi)));
  } else {
    Register constant = GetSmiConstant(source);
    Push(constant);
  }
}


void MacroAssembler::PushRegisterAsTwoSmis(Register src, Register scratch) {
  DCHECK(!src.is(scratch));
  movp(scratch, src);
  // High bits.
  shrp(src, Immediate(kPointerSize * kBitsPerByte - kSmiShift));
  shlp(src, Immediate(kSmiShift));
  Push(src);
  // Low bits.
  shlp(scratch, Immediate(kSmiShift));
  Push(scratch);
}


void MacroAssembler::PopRegisterAsTwoSmis(Register dst, Register scratch) {
  DCHECK(!dst.is(scratch));
  Pop(scratch);
  // Low bits.
  shrp(scratch, Immediate(kSmiShift));
  Pop(dst);
  shrp(dst, Immediate(kSmiShift));
  // High bits.
  shlp(dst, Immediate(kPointerSize * kBitsPerByte - kSmiShift));
  orp(dst, scratch);
}


void MacroAssembler::Test(const Operand& src, Smi* source) {
  if (SmiValuesAre32Bits()) {
    testl(Operand(src, kIntSize), Immediate(source->value()));
  } else {
    DCHECK(SmiValuesAre31Bits());
    testl(src, Immediate(source));
  }
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
  subp(mask, Immediate(1));  // Make mask.

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
  xorp(scratch, FieldOperand(object, HeapNumber::kValueOffset));
  andp(scratch, mask);
  // Each entry in string cache consists of two pointer sized fields,
  // but times_twice_pointer_size (multiplication by 16) scale factor
  // is not supported by addrmode on x64 platform.
  // So we have to premultiply entry index before lookup.
  shlp(scratch, Immediate(kPointerSizeLog2 + 1));

  Register index = scratch;
  Register probe = mask;
  movp(probe,
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
  andp(scratch, mask);
  // Each entry in string cache consists of two pointer sized fields,
  // but times_twice_pointer_size (multiplication by 16) scale factor
  // is not supported by addrmode on x64 platform.
  // So we have to premultiply entry index before lookup.
  shlp(scratch, Immediate(kPointerSizeLog2 + 1));

  // Check if the entry is the smi we are looking for.
  cmpp(object,
       FieldOperand(number_string_cache,
                    index,
                    times_1,
                    FixedArray::kHeaderSize));
  j(not_equal, not_found);

  // Get the result from the cache.
  bind(&load_result_from_cache);
  movp(result,
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


void MacroAssembler::JumpIfNotBothSequentialOneByteStrings(
    Register first_object, Register second_object, Register scratch1,
    Register scratch2, Label* on_fail, Label::Distance near_jump) {
  // Check that both objects are not smis.
  Condition either_smi = CheckEitherSmi(first_object, second_object);
  j(either_smi, on_fail, near_jump);

  // Load instance type for both strings.
  movp(scratch1, FieldOperand(first_object, HeapObject::kMapOffset));
  movp(scratch2, FieldOperand(second_object, HeapObject::kMapOffset));
  movzxbl(scratch1, FieldOperand(scratch1, Map::kInstanceTypeOffset));
  movzxbl(scratch2, FieldOperand(scratch2, Map::kInstanceTypeOffset));

  // Check that both are flat one-byte strings.
  DCHECK(kNotStringTag != 0);
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;

  andl(scratch1, Immediate(kFlatOneByteStringMask));
  andl(scratch2, Immediate(kFlatOneByteStringMask));
  // Interleave the bits to check both scratch1 and scratch2 in one test.
  DCHECK_EQ(0, kFlatOneByteStringMask & (kFlatOneByteStringMask << 3));
  leap(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmpl(scratch1,
       Immediate(kFlatOneByteStringTag + (kFlatOneByteStringTag << 3)));
  j(not_equal, on_fail, near_jump);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialOneByte(
    Register instance_type, Register scratch, Label* failure,
    Label::Distance near_jump) {
  if (!scratch.is(instance_type)) {
    movl(scratch, instance_type);
  }

  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;

  andl(scratch, Immediate(kFlatOneByteStringMask));
  cmpl(scratch, Immediate(kStringTag | kSeqStringTag | kOneByteStringTag));
  j(not_equal, failure, near_jump);
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialOneByte(
    Register first_object_instance_type, Register second_object_instance_type,
    Register scratch1, Register scratch2, Label* on_fail,
    Label::Distance near_jump) {
  // Load instance type for both strings.
  movp(scratch1, first_object_instance_type);
  movp(scratch2, second_object_instance_type);

  // Check that both are flat one-byte strings.
  DCHECK(kNotStringTag != 0);
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringRepresentationMask | kStringEncodingMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;

  andl(scratch1, Immediate(kFlatOneByteStringMask));
  andl(scratch2, Immediate(kFlatOneByteStringMask));
  // Interleave the bits to check both scratch1 and scratch2 in one test.
  DCHECK_EQ(0, kFlatOneByteStringMask & (kFlatOneByteStringMask << 3));
  leap(scratch1, Operand(scratch1, scratch2, times_8, 0));
  cmpl(scratch1,
       Immediate(kFlatOneByteStringTag + (kFlatOneByteStringTag << 3)));
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


void MacroAssembler::JumpIfNotUniqueNameInstanceType(Operand operand,
                                                     Label* not_unique_name,
                                                     Label::Distance distance) {
  JumpIfNotUniqueNameHelper<Operand>(this, operand, not_unique_name, distance);
}


void MacroAssembler::JumpIfNotUniqueNameInstanceType(Register reg,
                                                     Label* not_unique_name,
                                                     Label::Distance distance) {
  JumpIfNotUniqueNameHelper<Register>(this, reg, not_unique_name, distance);
}


void MacroAssembler::Move(Register dst, Register src) {
  if (!dst.is(src)) {
    movp(dst, src);
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
    movp(dst, kScratchRegister);
  }
}


void MacroAssembler::Move(XMMRegister dst, uint32_t src) {
  if (src == 0) {
    xorps(dst, dst);
  } else {
    unsigned cnt = base::bits::CountPopulation32(src);
    unsigned nlz = base::bits::CountLeadingZeros32(src);
    unsigned ntz = base::bits::CountTrailingZeros32(src);
    if (nlz + cnt + ntz == 32) {
      pcmpeqd(dst, dst);
      if (ntz == 0) {
        psrld(dst, 32 - cnt);
      } else {
        pslld(dst, 32 - cnt);
        if (nlz != 0) psrld(dst, nlz);
      }
    } else {
      movl(kScratchRegister, Immediate(src));
      movq(dst, kScratchRegister);
    }
  }
}


void MacroAssembler::Move(XMMRegister dst, uint64_t src) {
  uint32_t lower = static_cast<uint32_t>(src);
  uint32_t upper = static_cast<uint32_t>(src >> 32);
  if (upper == 0) {
    Move(dst, lower);
  } else {
    unsigned cnt = base::bits::CountPopulation64(src);
    unsigned nlz = base::bits::CountLeadingZeros64(src);
    unsigned ntz = base::bits::CountTrailingZeros64(src);
    if (nlz + cnt + ntz == 64) {
      pcmpeqd(dst, dst);
      if (ntz == 0) {
        psrlq(dst, 64 - cnt);
      } else {
        psllq(dst, 64 - cnt);
        if (nlz != 0) psrlq(dst, nlz);
      }
    } else if (lower == 0) {
      Move(dst, upper);
      psllq(dst, 32);
    } else {
      movq(kScratchRegister, src);
      movq(dst, kScratchRegister);
    }
  }
}


void MacroAssembler::Cmp(Register dst, Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else {
    MoveHeapObject(kScratchRegister, source);
    cmpp(dst, kScratchRegister);
  }
}


void MacroAssembler::Cmp(const Operand& dst, Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else {
    MoveHeapObject(kScratchRegister, source);
    cmpp(dst, kScratchRegister);
  }
}


void MacroAssembler::Push(Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Push(Smi::cast(*source));
  } else {
    MoveHeapObject(kScratchRegister, source);
    Push(kScratchRegister);
  }
}


void MacroAssembler::MoveHeapObject(Register result,
                                    Handle<Object> object) {
  AllowDeferredHandleDereference using_raw_address;
  DCHECK(object->IsHeapObject());
  if (isolate()->heap()->InNewSpace(*object)) {
    Handle<Cell> cell = isolate()->factory()->NewCell(object);
    Move(result, cell, RelocInfo::CELL);
    movp(result, Operand(result, 0));
  } else {
    Move(result, object, RelocInfo::EMBEDDED_OBJECT);
  }
}


void MacroAssembler::LoadGlobalCell(Register dst, Handle<Cell> cell) {
  if (dst.is(rax)) {
    AllowDeferredHandleDereference embedding_raw_address;
    load_rax(cell.location(), RelocInfo::CELL);
  } else {
    Move(dst, cell, RelocInfo::CELL);
    movp(dst, Operand(dst, 0));
  }
}


void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    addp(rsp, Immediate(stack_elements * kPointerSize));
  }
}


void MacroAssembler::DropUnderReturnAddress(int stack_elements,
                                            Register scratch) {
  DCHECK(stack_elements > 0);
  if (kPointerSize == kInt64Size && stack_elements == 1) {
    popq(MemOperand(rsp, 0));
    return;
  }

  PopReturnAddressTo(scratch);
  Drop(stack_elements);
  PushReturnAddressFrom(scratch);
}


void MacroAssembler::Push(Register src) {
  if (kPointerSize == kInt64Size) {
    pushq(src);
  } else {
    // x32 uses 64-bit push for rbp in the prologue.
    DCHECK(src.code() != rbp.code());
    leal(rsp, Operand(rsp, -4));
    movp(Operand(rsp, 0), src);
  }
}


void MacroAssembler::Push(const Operand& src) {
  if (kPointerSize == kInt64Size) {
    pushq(src);
  } else {
    movp(kScratchRegister, src);
    leal(rsp, Operand(rsp, -4));
    movp(Operand(rsp, 0), kScratchRegister);
  }
}


void MacroAssembler::PushQuad(const Operand& src) {
  if (kPointerSize == kInt64Size) {
    pushq(src);
  } else {
    movp(kScratchRegister, src);
    pushq(kScratchRegister);
  }
}


void MacroAssembler::Push(Immediate value) {
  if (kPointerSize == kInt64Size) {
    pushq(value);
  } else {
    leal(rsp, Operand(rsp, -4));
    movp(Operand(rsp, 0), value);
  }
}


void MacroAssembler::PushImm32(int32_t imm32) {
  if (kPointerSize == kInt64Size) {
    pushq_imm32(imm32);
  } else {
    leal(rsp, Operand(rsp, -4));
    movp(Operand(rsp, 0), Immediate(imm32));
  }
}


void MacroAssembler::Pop(Register dst) {
  if (kPointerSize == kInt64Size) {
    popq(dst);
  } else {
    // x32 uses 64-bit pop for rbp in the epilogue.
    DCHECK(dst.code() != rbp.code());
    movp(dst, Operand(rsp, 0));
    leal(rsp, Operand(rsp, 4));
  }
}


void MacroAssembler::Pop(const Operand& dst) {
  if (kPointerSize == kInt64Size) {
    popq(dst);
  } else {
    Register scratch = dst.AddressUsesRegister(kScratchRegister)
        ? kSmiConstantRegister : kScratchRegister;
    movp(scratch, Operand(rsp, 0));
    movp(dst, scratch);
    leal(rsp, Operand(rsp, 4));
    if (scratch.is(kSmiConstantRegister)) {
      // Restore kSmiConstantRegister.
      movp(kSmiConstantRegister,
           reinterpret_cast<void*>(Smi::FromInt(kSmiConstantRegisterValue)),
           Assembler::RelocInfoNone());
    }
  }
}


void MacroAssembler::PopQuad(const Operand& dst) {
  if (kPointerSize == kInt64Size) {
    popq(dst);
  } else {
    popq(kScratchRegister);
    movp(dst, kScratchRegister);
  }
}


void MacroAssembler::LoadSharedFunctionInfoSpecialField(Register dst,
                                                        Register base,
                                                        int offset) {
  DCHECK(offset > SharedFunctionInfo::kLengthOffset &&
         offset <= SharedFunctionInfo::kSize &&
         (((offset - SharedFunctionInfo::kLengthOffset) / kIntSize) % 2 == 1));
  if (kPointerSize == kInt64Size) {
    movsxlq(dst, FieldOperand(base, offset));
  } else {
    movp(dst, FieldOperand(base, offset));
    SmiToInteger32(dst, dst);
  }
}


void MacroAssembler::TestBitSharedFunctionInfoSpecialField(Register base,
                                                           int offset,
                                                           int bits) {
  DCHECK(offset > SharedFunctionInfo::kLengthOffset &&
         offset <= SharedFunctionInfo::kSize &&
         (((offset - SharedFunctionInfo::kLengthOffset) / kIntSize) % 2 == 1));
  if (kPointerSize == kInt32Size) {
    // On x32, this field is represented by SMI.
    bits += kSmiShift;
  }
  int byte_offset = bits / kBitsPerByte;
  int bit_in_byte = bits & (kBitsPerByte - 1);
  testb(FieldOperand(base, offset + byte_offset), Immediate(1 << bit_in_byte));
}


void MacroAssembler::Jump(ExternalReference ext) {
  LoadAddress(kScratchRegister, ext);
  jmp(kScratchRegister);
}


void MacroAssembler::Jump(const Operand& op) {
  if (kPointerSize == kInt64Size) {
    jmp(op);
  } else {
    movp(kScratchRegister, op);
    jmp(kScratchRegister);
  }
}


void MacroAssembler::Jump(Address destination, RelocInfo::Mode rmode) {
  Move(kScratchRegister, destination, rmode);
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


void MacroAssembler::Call(const Operand& op) {
  if (kPointerSize == kInt64Size) {
    call(op);
  } else {
    movp(kScratchRegister, op);
    call(kScratchRegister);
  }
}


void MacroAssembler::Call(Address destination, RelocInfo::Mode rmode) {
#ifdef DEBUG
  int end_position = pc_offset() + CallSize(destination);
#endif
  Move(kScratchRegister, destination, rmode);
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
  DCHECK(RelocInfo::IsCodeTarget(rmode) ||
      rmode == RelocInfo::CODE_AGE_SEQUENCE);
  call(code_object, rmode, ast_id);
#ifdef DEBUG
  CHECK_EQ(end_position, pc_offset());
#endif
}


void MacroAssembler::Pushad() {
  Push(rax);
  Push(rcx);
  Push(rdx);
  Push(rbx);
  // Not pushing rsp or rbp.
  Push(rsi);
  Push(rdi);
  Push(r8);
  Push(r9);
  // r10 is kScratchRegister.
  Push(r11);
  // r12 is kSmiConstantRegister.
  // r13 is kRootRegister.
  Push(r14);
  Push(r15);
  STATIC_ASSERT(11 == kNumSafepointSavedRegisters);
  // Use lea for symmetry with Popad.
  int sp_delta =
      (kNumSafepointRegisters - kNumSafepointSavedRegisters) * kPointerSize;
  leap(rsp, Operand(rsp, -sp_delta));
}


void MacroAssembler::Popad() {
  // Popad must not change the flags, so use lea instead of addq.
  int sp_delta =
      (kNumSafepointRegisters - kNumSafepointSavedRegisters) * kPointerSize;
  leap(rsp, Operand(rsp, sp_delta));
  Pop(r15);
  Pop(r14);
  Pop(r11);
  Pop(r9);
  Pop(r8);
  Pop(rdi);
  Pop(rsi);
  Pop(rbx);
  Pop(rdx);
  Pop(rcx);
  Pop(rax);
}


void MacroAssembler::Dropad() {
  addp(rsp, Immediate(kNumSafepointRegisters * kPointerSize));
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
  movp(SafepointRegisterSlot(dst), imm);
}


void MacroAssembler::StoreToSafepointRegisterSlot(Register dst, Register src) {
  movp(SafepointRegisterSlot(dst), src);
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  movp(dst, SafepointRegisterSlot(src));
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
    pushq(Immediate(0));  // NULL frame pointer.
    Push(Smi::FromInt(0));  // No context.
  } else {
    pushq(rbp);
    Push(rsi);
  }

  // Push the state and the code object.
  unsigned state =
      StackHandler::IndexField::encode(handler_index) |
      StackHandler::KindField::encode(kind);
  Push(Immediate(state));
  Push(CodeObject());

  // Link the current handler as the next handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  Push(ExternalOperand(handler_address));
  // Set this new handler as the current one.
  movp(ExternalOperand(handler_address), rsp);
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  Pop(ExternalOperand(handler_address));
  addp(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}


void MacroAssembler::JumpToHandlerEntry() {
  // Compute the handler entry address and jump to it.  The handler table is
  // a fixed array of (smi-tagged) code offsets.
  // rax = exception, rdi = code object, rdx = state.
  movp(rbx, FieldOperand(rdi, Code::kHandlerTableOffset));
  shrp(rdx, Immediate(StackHandler::kKindWidth));
  movp(rdx,
       FieldOperand(rbx, rdx, times_pointer_size, FixedArray::kHeaderSize));
  SmiToInteger64(rdx, rdx);
  leap(rdi, FieldOperand(rdi, rdx, times_1, Code::kHeaderSize));
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
    movp(rax, value);
  }
  // Drop the stack pointer to the top of the top handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  movp(rsp, ExternalOperand(handler_address));
  // Restore the next handler.
  Pop(ExternalOperand(handler_address));

  // Remove the code object and state, compute the handler address in rdi.
  Pop(rdi);  // Code object.
  Pop(rdx);  // Offset and state.

  // Restore the context and frame pointer.
  Pop(rsi);  // Context.
  popq(rbp);  // Frame pointer.

  // If the handler is a JS frame, restore the context to the frame.
  // (kind == ENTRY) == (rbp == 0) == (rsi == 0), so we could test either
  // rbp or rsi.
  Label skip;
  testp(rsi, rsi);
  j(zero, &skip, Label::kNear);
  movp(Operand(rbp, StandardFrameConstants::kContextOffset), rsi);
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
    movp(rax, value);
  }
  // Drop the stack pointer to the top of the top stack handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  Load(rsp, handler_address);

  // Unwind the handlers until the top ENTRY handler is found.
  Label fetch_next, check_kind;
  jmp(&check_kind, Label::kNear);
  bind(&fetch_next);
  movp(rsp, Operand(rsp, StackHandlerConstants::kNextOffset));

  bind(&check_kind);
  STATIC_ASSERT(StackHandler::JS_ENTRY == 0);
  testl(Operand(rsp, StackHandlerConstants::kStateOffset),
        Immediate(StackHandler::KindField::kMask));
  j(not_zero, &fetch_next);

  // Set the top handler address to next handler past the top ENTRY handler.
  Pop(ExternalOperand(handler_address));

  // Remove the code object and state, compute the handler address in rdi.
  Pop(rdi);  // Code object.
  Pop(rdx);  // Offset and state.

  // Clear the context pointer and frame pointer (0 was saved in the handler).
  Pop(rsi);
  popq(rbp);

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
    addp(rsp, Immediate(bytes_dropped));
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
  movp(map, FieldOperand(heap_object, HeapObject::kMapOffset));
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
  Set(kScratchRegister,
      bit_cast<uint64_t>(
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


void MacroAssembler::CompareMap(Register obj, Handle<Map> map) {
  Cmp(FieldOperand(obj, HeapObject::kMapOffset), map);
}


void MacroAssembler::CheckMap(Register obj,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  CompareMap(obj, map);
  j(not_equal, fail);
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
  cmpl(result_reg, Immediate(1));
  j(overflow, &conv_failure, Label::kNear);
  movl(result_reg, Immediate(0));
  setcc(sign, result_reg);
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
                                Register src) {
  if (FLAG_debug_code) {
    cmpq(src, Immediate(0xffffffff));
    Assert(below_equal, kInputGPRIsExpectedToHaveUpper32Cleared);
  }
  cvtqsi2sd(dst, src);
}


void MacroAssembler::SlowTruncateToI(Register result_reg,
                                     Register input_reg,
                                     int offset) {
  DoubleToIStub stub(isolate(), input_reg, result_reg, offset, true);
  call(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::TruncateHeapNumberToI(Register result_reg,
                                           Register input_reg) {
  Label done;
  movsd(xmm0, FieldOperand(input_reg, HeapNumber::kValueOffset));
  cvttsd2siq(result_reg, xmm0);
  cmpq(result_reg, Immediate(1));
  j(no_overflow, &done, Label::kNear);

  // Slow case.
  if (input_reg.is(result_reg)) {
    subp(rsp, Immediate(kDoubleSize));
    movsd(MemOperand(rsp, 0), xmm0);
    SlowTruncateToI(result_reg, rsp, 0);
    addp(rsp, Immediate(kDoubleSize));
  } else {
    SlowTruncateToI(result_reg, input_reg);
  }

  bind(&done);
  // Keep our invariant that the upper 32 bits are zero.
  movl(result_reg, result_reg);
}


void MacroAssembler::TruncateDoubleToI(Register result_reg,
                                       XMMRegister input_reg) {
  Label done;
  cvttsd2siq(result_reg, input_reg);
  cmpq(result_reg, Immediate(1));
  j(no_overflow, &done, Label::kNear);

  subp(rsp, Immediate(kDoubleSize));
  movsd(MemOperand(rsp, 0), input_reg);
  SlowTruncateToI(result_reg, rsp, 0);
  addp(rsp, Immediate(kDoubleSize));

  bind(&done);
  // Keep our invariant that the upper 32 bits are zero.
  movl(result_reg, result_reg);
}


void MacroAssembler::DoubleToI(Register result_reg, XMMRegister input_reg,
                               XMMRegister scratch,
                               MinusZeroMode minus_zero_mode,
                               Label* lost_precision, Label* is_nan,
                               Label* minus_zero, Label::Distance dst) {
  cvttsd2si(result_reg, input_reg);
  Cvtlsi2sd(xmm0, result_reg);
  ucomisd(xmm0, input_reg);
  j(not_equal, lost_precision, dst);
  j(parity_even, is_nan, dst);  // NaN.
  if (minus_zero_mode == FAIL_ON_MINUS_ZERO) {
    Label done;
    // The integer converted back is equal to the original. We
    // only have to test if we got -0 as an input.
    testl(result_reg, result_reg);
    j(not_zero, &done, Label::kNear);
    movmskpd(result_reg, input_reg);
    // Bit 0 contains the sign of the double in input_reg.
    // If input was positive, we are ok and return 0, otherwise
    // jump to minus_zero.
    andl(result_reg, Immediate(1));
    j(not_zero, minus_zero, dst);
    bind(&done);
  }
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  movp(descriptors, FieldOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  movl(dst, FieldOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  movl(dst, FieldOperand(map, Map::kBitField3Offset));
  andl(dst, Immediate(Map::EnumLengthBits::kMask));
  Integer32ToSmi(dst, dst);
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
    DCHECK(!int32_register.is(kScratchRegister));
    movq(kScratchRegister, V8_INT64_C(0x0000000100000000));
    cmpq(kScratchRegister, int32_register);
    Check(above_equal, k32BitValueInRegisterIsNotZeroExtended);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAString);
    Push(object);
    movp(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, FIRST_NONSTRING_TYPE);
    Pop(object);
    Check(below, kOperandIsNotAString);
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAName);
    Push(object);
    movp(object, FieldOperand(object, HeapObject::kMapOffset));
    CmpInstanceType(object, LAST_NAME_TYPE);
    Pop(object);
    Check(below_equal, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    Cmp(object, isolate()->factory()->undefined_value());
    j(equal, &done_checking);
    Cmp(FieldOperand(object, 0), isolate()->factory()->allocation_site_map());
    Assert(equal, kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}


void MacroAssembler::AssertRootValue(Register src,
                                     Heap::RootListIndex root_value_index,
                                     BailoutReason reason) {
  if (emit_debug_code()) {
    DCHECK(!src.is(kScratchRegister));
    LoadRoot(kScratchRegister, root_value_index);
    cmpp(src, kScratchRegister);
    Check(equal, reason);
  }
}



Condition MacroAssembler::IsObjectStringType(Register heap_object,
                                             Register map,
                                             Register instance_type) {
  movp(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzxbl(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  testb(instance_type, Immediate(kIsNotStringMask));
  return zero;
}


Condition MacroAssembler::IsObjectNameType(Register heap_object,
                                           Register map,
                                           Register instance_type) {
  movp(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  movzxbl(instance_type, FieldOperand(map, Map::kInstanceTypeOffset));
  cmpb(instance_type, Immediate(static_cast<uint8_t>(LAST_NAME_TYPE)));
  return below_equal;
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Label* miss,
                                             bool miss_on_bound_function) {
  Label non_instance;
  if (miss_on_bound_function) {
    // Check that the receiver isn't a smi.
    testl(function, Immediate(kSmiTagMask));
    j(zero, miss);

    // Check that the function really is a function.
    CmpObjectType(function, JS_FUNCTION_TYPE, result);
    j(not_equal, miss);

    movp(kScratchRegister,
         FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
    // It's not smi-tagged (stored in the top half of a smi-tagged 8-byte
    // field).
    TestBitSharedFunctionInfoSpecialField(kScratchRegister,
        SharedFunctionInfo::kCompilerHintsOffset,
        SharedFunctionInfo::kBoundFunction);
    j(not_zero, miss);

    // Make sure that the function has an instance prototype.
    testb(FieldOperand(result, Map::kBitFieldOffset),
          Immediate(1 << Map::kHasNonInstancePrototype));
    j(not_zero, &non_instance, Label::kNear);
  }

  // Get the prototype or initial map from the function.
  movp(result,
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
  movp(result, FieldOperand(result, Map::kPrototypeOffset));

  if (miss_on_bound_function) {
    jmp(&done, Label::kNear);

    // Non-instance prototype: Fetch prototype from constructor field
    // in initial map.
    bind(&non_instance);
    movp(result, FieldOperand(result, Map::kConstructorOffset));
  }

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
  DCHECK(value > 0);
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
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand counter_operand = ExternalOperand(ExternalReference(counter));
    if (value == 1) {
      decl(counter_operand);
    } else {
      subl(counter_operand, Immediate(value));
    }
  }
}


void MacroAssembler::DebugBreak() {
  Set(rax, 0);  // No arguments.
  LoadAddress(rbx, ExternalReference(Runtime::kDebugBreak, isolate()));
  CEntryStub ces(isolate(), 1);
  DCHECK(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

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
                 call_wrapper);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      call(code);
      call_wrapper.AfterCall();
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      jmp(code);
    }
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(function.is(rdi));
  movp(rdx, FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
  movp(rsi, FieldOperand(function, JSFunction::kContextOffset));
  LoadSharedFunctionInfoSpecialField(rbx, rdx,
      SharedFunctionInfo::kFormalParameterCountOffset);
  // Advances rdx to the end of the Code object header, to the start of
  // the executable code.
  movp(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));

  ParameterCount expected(rbx);
  InvokeCode(rdx, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  DCHECK(function.is(rdi));
  movp(rsi, FieldOperand(function, JSFunction::kContextOffset));
  // Advances rdx to the end of the Code object header, to the start of
  // the executable code.
  movp(rdx, FieldOperand(rdi, JSFunction::kCodeEntryOffset));

  InvokeCode(rdx, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  Move(rdi, function);
  InvokeFunction(rdi, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_register,
                                    Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    Label::Distance near_jump,
                                    const CallWrapper& call_wrapper) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label invoke;
  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
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
      cmpp(expected.reg(), Immediate(actual.immediate()));
      j(equal, &invoke, Label::kNear);
      DCHECK(expected.reg().is(rbx));
      Set(rax, actual.immediate());
    } else if (!expected.reg().is(actual.reg())) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmpp(expected.reg(), actual.reg());
      j(equal, &invoke, Label::kNear);
      DCHECK(actual.reg().is(rax));
      DCHECK(expected.reg().is(rbx));
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (!code_constant.is_null()) {
      Move(rdx, code_constant, RelocInfo::EMBEDDED_OBJECT);
      addp(rdx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    } else if (!code_register.is(rdx)) {
      movp(rdx, code_register);
    }

    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      Call(adaptor, RelocInfo::CODE_TARGET);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        jmp(done, near_jump);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}


void MacroAssembler::StubPrologue() {
    pushq(rbp);  // Caller's frame pointer.
    movp(rbp, rsp);
    Push(rsi);  // Callee's context.
    Push(Smi::FromInt(StackFrame::STUB));
}


void MacroAssembler::Prologue(bool code_pre_aging) {
  PredictableCodeSizeScope predictible_code_size_scope(this,
      kNoCodeAgeSequenceLength);
  if (code_pre_aging) {
      // Pre-age the code.
    Call(isolate()->builtins()->MarkCodeAsExecutedOnce(),
         RelocInfo::CODE_AGE_SEQUENCE);
    Nop(kNoCodeAgeSequenceLength - Assembler::kShortCallInstructionLength);
  } else {
    pushq(rbp);  // Caller's frame pointer.
    movp(rbp, rsp);
    Push(rsi);  // Callee's context.
    Push(rdi);  // Callee's JS function.
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // Out-of-line constant pool not implemented on x64.
  UNREACHABLE();
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  pushq(rbp);
  movp(rbp, rsp);
  Push(rsi);  // Context.
  Push(Smi::FromInt(type));
  Move(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  Push(kScratchRegister);
  if (emit_debug_code()) {
    Move(kScratchRegister,
         isolate()->factory()->undefined_value(),
         RelocInfo::EMBEDDED_OBJECT);
    cmpp(Operand(rsp, 0), kScratchRegister);
    Check(not_equal, kCodeObjectNotProperlyPatched);
  }
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code()) {
    Move(kScratchRegister, Smi::FromInt(type));
    cmpp(Operand(rbp, StandardFrameConstants::kMarkerOffset), kScratchRegister);
    Check(equal, kStackFrameTypesMustMatch);
  }
  movp(rsp, rbp);
  popq(rbp);
}


void MacroAssembler::EnterExitFramePrologue(bool save_rax) {
  // Set up the frame structure on the stack.
  // All constants are relative to the frame pointer of the exit frame.
  DCHECK(ExitFrameConstants::kCallerSPDisplacement ==
         kFPOnStackSize + kPCOnStackSize);
  DCHECK(ExitFrameConstants::kCallerPCOffset == kFPOnStackSize);
  DCHECK(ExitFrameConstants::kCallerFPOffset == 0 * kPointerSize);
  pushq(rbp);
  movp(rbp, rsp);

  // Reserve room for entry stack pointer and push the code object.
  DCHECK(ExitFrameConstants::kSPOffset == -1 * kPointerSize);
  Push(Immediate(0));  // Saved entry sp, patched before call.
  Move(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  Push(kScratchRegister);  // Accessed from EditFrame::code_slot.

  // Save the frame pointer and the context in top.
  if (save_rax) {
    movp(r14, rax);  // Backup rax in callee-save register.
  }

  Store(ExternalReference(Isolate::kCEntryFPAddress, isolate()), rbp);
  Store(ExternalReference(Isolate::kContextAddress, isolate()), rsi);
  Store(ExternalReference(Isolate::kCFunctionAddress, isolate()), rbx);
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
        arg_stack_space * kRegisterSize;
    subp(rsp, Immediate(space));
    int offset = -2 * kPointerSize;
    for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); i++) {
      XMMRegister reg = XMMRegister::FromAllocationIndex(i);
      movsd(Operand(rbp, offset - ((i + 1) * kDoubleSize)), reg);
    }
  } else if (arg_stack_space > 0) {
    subp(rsp, Immediate(arg_stack_space * kRegisterSize));
  }

  // Get the required frame alignment for the OS.
  const int kFrameAlignment = base::OS::ActivationFrameAlignment();
  if (kFrameAlignment > 0) {
    DCHECK(base::bits::IsPowerOfTwo32(kFrameAlignment));
    DCHECK(is_int8(kFrameAlignment));
    andp(rsp, Immediate(-kFrameAlignment));
  }

  // Patch the saved entry sp.
  movp(Operand(rbp, ExitFrameConstants::kSPOffset), rsp);
}


void MacroAssembler::EnterExitFrame(int arg_stack_space, bool save_doubles) {
  EnterExitFramePrologue(true);

  // Set up argv in callee-saved register r15. It is reused in LeaveExitFrame,
  // so it must be retained across the C-call.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  leap(r15, Operand(rbp, r14, times_pointer_size, offset));

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
  movp(rcx, Operand(rbp, kFPOnStackSize));
  movp(rbp, Operand(rbp, 0 * kPointerSize));

  // Drop everything up to and including the arguments and the receiver
  // from the caller stack.
  leap(rsp, Operand(r15, 1 * kPointerSize));

  PushReturnAddressFrom(rcx);

  LeaveExitFrameEpilogue(true);
}


void MacroAssembler::LeaveApiExitFrame(bool restore_context) {
  movp(rsp, rbp);
  popq(rbp);

  LeaveExitFrameEpilogue(restore_context);
}


void MacroAssembler::LeaveExitFrameEpilogue(bool restore_context) {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(Isolate::kContextAddress, isolate());
  Operand context_operand = ExternalOperand(context_address);
  if (restore_context) {
    movp(rsi, context_operand);
  }
#ifdef DEBUG
  movp(context_operand, Immediate(0));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address(Isolate::kCEntryFPAddress,
                                       isolate());
  Operand c_entry_fp_operand = ExternalOperand(c_entry_fp_address);
  movp(c_entry_fp_operand, Immediate(0));
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  DCHECK(!holder_reg.is(scratch));
  DCHECK(!scratch.is(kScratchRegister));
  // Load current lexical context from the stack frame.
  movp(scratch, Operand(rbp, StandardFrameConstants::kContextOffset));

  // When generating debug code, make sure the lexical context is set.
  if (emit_debug_code()) {
    cmpp(scratch, Immediate(0));
    Check(not_equal, kWeShouldNotHaveAnEmptyLexicalContext);
  }
  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  movp(scratch, FieldOperand(scratch, offset));
  movp(scratch, FieldOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    Cmp(FieldOperand(scratch, HeapObject::kMapOffset),
        isolate()->factory()->native_context_map());
    Check(equal, kJSGlobalObjectNativeContextShouldBeANativeContext);
  }

  // Check if both contexts are the same.
  cmpp(scratch, FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  j(equal, &same_contexts);

  // Compare security tokens.
  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Preserve original value of holder_reg.
    Push(holder_reg);
    movp(holder_reg,
         FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
    CompareRoot(holder_reg, Heap::kNullValueRootIndex);
    Check(not_equal, kJSGlobalProxyContextShouldNotBeNull);

    // Read the first word and compare to native_context_map(),
    movp(holder_reg, FieldOperand(holder_reg, HeapObject::kMapOffset));
    CompareRoot(holder_reg, Heap::kNativeContextMapRootIndex);
    Check(equal, kJSGlobalObjectNativeContextShouldBeANativeContext);
    Pop(holder_reg);
  }

  movp(kScratchRegister,
       FieldOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  int token_offset =
      Context::kHeaderSize + Context::SECURITY_TOKEN_INDEX * kPointerSize;
  movp(scratch, FieldOperand(scratch, token_offset));
  cmpp(scratch, FieldOperand(kScratchRegister, token_offset));
  j(not_equal, miss);

  bind(&same_contexts);
}


// Compute the hash code from the untagged key.  This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericStub in
// code-stub-hydrogen.cc
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
  for (int i = 0; i < kNumberDictionaryProbes; i++) {
    // Use r2 for index calculations and keep the hash intact in r0.
    movp(r2, r0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      addl(r2, Immediate(SeededNumberDictionary::GetProbeOffset(i)));
    }
    andp(r2, r1);

    // Scale the index by multiplying by the entry size.
    DCHECK(SeededNumberDictionary::kEntrySize == 3);
    leap(r2, Operand(r2, r2, times_2, 0));  // r2 = r2 * 3

    // Check if the key matches.
    cmpp(key, FieldOperand(elements,
                           r2,
                           times_pointer_size,
                           SeededNumberDictionary::kElementsStartOffset));
    if (i != (kNumberDictionaryProbes - 1)) {
      j(equal, &done);
    } else {
      j(not_equal, miss);
    }
  }

  bind(&done);
  // Check that the value is a normal propety.
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  DCHECK_EQ(NORMAL, 0);
  Test(FieldOperand(elements, r2, times_pointer_size, kDetailsOffset),
       Smi::FromInt(PropertyDetails::TypeField::kMask));
  j(not_zero, miss);

  // Get the value at the masked, scaled index.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  movp(result, FieldOperand(elements, r2, times_pointer_size, kValueOffset));
}


void MacroAssembler::LoadAllocationTopHelper(Register result,
                                             Register scratch,
                                             AllocationFlags flags) {
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Just return if allocation top is already known.
  if ((flags & RESULT_CONTAINS_TOP) != 0) {
    // No use of scratch if allocation top is provided.
    DCHECK(!scratch.is_valid());
#ifdef DEBUG
    // Assert that result actually contains top on entry.
    Operand top_operand = ExternalOperand(allocation_top);
    cmpp(result, top_operand);
    Check(equal, kUnexpectedAllocationTop);
#endif
    return;
  }

  // Move address of new object to result. Use scratch register if available,
  // and keep address in scratch until call to UpdateAllocationTopHelper.
  if (scratch.is_valid()) {
    LoadAddress(scratch, allocation_top);
    movp(result, Operand(scratch, 0));
  } else {
    Load(result, allocation_top);
  }
}


void MacroAssembler::MakeSureDoubleAlignedHelper(Register result,
                                                 Register scratch,
                                                 Label* gc_required,
                                                 AllocationFlags flags) {
  if (kPointerSize == kDoubleSize) {
    if (FLAG_debug_code) {
      testl(result, Immediate(kDoubleAlignmentMask));
      Check(zero, kAllocationIsNotDoubleAligned);
    }
  } else {
    // Align the next allocation. Storing the filler map without checking top
    // is safe in new-space because the limit of the heap is aligned there.
    DCHECK(kPointerSize * 2 == kDoubleSize);
    DCHECK((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    // Make sure scratch is not clobbered by this function as it might be
    // used in UpdateAllocationTopHelper later.
    DCHECK(!scratch.is(kScratchRegister));
    Label aligned;
    testl(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    if ((flags & PRETENURE_OLD_DATA_SPACE) != 0) {
      ExternalReference allocation_limit =
          AllocationUtils::GetAllocationLimitReference(isolate(), flags);
      cmpp(result, ExternalOperand(allocation_limit));
      j(above_equal, gc_required);
    }
    LoadRoot(kScratchRegister, Heap::kOnePointerFillerMapRootIndex);
    movp(Operand(result, 0), kScratchRegister);
    addp(result, Immediate(kDoubleSize / 2));
    bind(&aligned);
  }
}


void MacroAssembler::UpdateAllocationTopHelper(Register result_end,
                                               Register scratch,
                                               AllocationFlags flags) {
  if (emit_debug_code()) {
    testp(result_end, Immediate(kObjectAlignmentMask));
    Check(zero, kUnalignedAllocationInNewSpace);
  }

  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);

  // Update new top.
  if (scratch.is_valid()) {
    // Scratch already contains address of allocation top.
    movp(Operand(scratch, 0), result_end);
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
  DCHECK((flags & (RESULT_CONTAINS_TOP | SIZE_IN_WORDS)) == 0);
  DCHECK(object_size <= Page::kMaxRegularHeapObjectSize);
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
  DCHECK(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    MakeSureDoubleAlignedHelper(result, scratch, gc_required, flags);
  }

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  Register top_reg = result_end.is_valid() ? result_end : result;

  if (!top_reg.is(result)) {
    movp(top_reg, result);
  }
  addp(top_reg, Immediate(object_size));
  j(carry, gc_required);
  Operand limit_operand = ExternalOperand(allocation_limit);
  cmpp(top_reg, limit_operand);
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(top_reg, scratch, flags);

  bool tag_result = (flags & TAG_OBJECT) != 0;
  if (top_reg.is(result)) {
    if (tag_result) {
      subp(result, Immediate(object_size - kHeapObjectTag));
    } else {
      subp(result, Immediate(object_size));
    }
  } else if (tag_result) {
    // Tag the result if requested.
    DCHECK(kHeapObjectTag == 1);
    incp(result);
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
  DCHECK((flags & SIZE_IN_WORDS) == 0);
  leap(result_end, Operand(element_count, element_size, header_size));
  Allocate(result_end, result, result_end, scratch, gc_required, flags);
}


void MacroAssembler::Allocate(Register object_size,
                              Register result,
                              Register result_end,
                              Register scratch,
                              Label* gc_required,
                              AllocationFlags flags) {
  DCHECK((flags & SIZE_IN_WORDS) == 0);
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
  DCHECK(!result.is(result_end));

  // Load address of new object into result.
  LoadAllocationTopHelper(result, scratch, flags);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    MakeSureDoubleAlignedHelper(result, scratch, gc_required, flags);
  }

  // Calculate new top and bail out if new space is exhausted.
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  if (!object_size.is(result_end)) {
    movp(result_end, object_size);
  }
  addp(result_end, result);
  j(carry, gc_required);
  Operand limit_operand = ExternalOperand(allocation_limit);
  cmpp(result_end, limit_operand);
  j(above, gc_required);

  // Update allocation top.
  UpdateAllocationTopHelper(result_end, scratch, flags);

  // Tag the result if requested.
  if ((flags & TAG_OBJECT) != 0) {
    addp(result, Immediate(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  andp(object, Immediate(~kHeapObjectTagMask));
  Operand top_operand = ExternalOperand(new_space_allocation_top);
#ifdef DEBUG
  cmpp(object, top_operand);
  Check(below, kUndoAllocationOfNonAllocatedMemory);
#endif
  movp(top_operand, object);
}


void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch,
                                        Label* gc_required,
                                        MutableMode mode) {
  // Allocate heap number in new space.
  Allocate(HeapNumber::kSize, result, scratch, no_reg, gc_required, TAG_OBJECT);

  Heap::RootListIndex map_index = mode == MUTABLE
      ? Heap::kMutableHeapNumberMapRootIndex
      : Heap::kHeapNumberMapRootIndex;

  // Set the map.
  LoadRoot(kScratchRegister, map_index);
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
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
  DCHECK(kShortSize == 2);
  // scratch1 = length * 2 + kObjectAlignmentMask.
  leap(scratch1, Operand(length, length, times_1, kObjectAlignmentMask +
                kHeaderAlignment));
  andp(scratch1, Immediate(~kObjectAlignmentMask));
  if (kHeaderAlignment > 0) {
    subp(scratch1, Immediate(kHeaderAlignment));
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
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
  Integer32ToSmi(scratch1, length);
  movp(FieldOperand(result, String::kLengthOffset), scratch1);
  movp(FieldOperand(result, String::kHashFieldOffset),
       Immediate(String::kEmptyHashField));
}


void MacroAssembler::AllocateOneByteString(Register result, Register length,
                                           Register scratch1, Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  const int kHeaderAlignment = SeqOneByteString::kHeaderSize &
                               kObjectAlignmentMask;
  movl(scratch1, length);
  DCHECK(kCharSize == 1);
  addp(scratch1, Immediate(kObjectAlignmentMask + kHeaderAlignment));
  andp(scratch1, Immediate(~kObjectAlignmentMask));
  if (kHeaderAlignment > 0) {
    subp(scratch1, Immediate(kHeaderAlignment));
  }

  // Allocate one-byte string in new space.
  Allocate(SeqOneByteString::kHeaderSize,
           times_1,
           scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  LoadRoot(kScratchRegister, Heap::kOneByteStringMapRootIndex);
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
  Integer32ToSmi(scratch1, length);
  movp(FieldOperand(result, String::kLengthOffset), scratch1);
  movp(FieldOperand(result, String::kHashFieldOffset),
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
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateOneByteConsString(Register result,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kConsOneByteStringMapRootIndex);
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
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
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateOneByteSlicedString(Register result,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  // Allocate heap number in new space.
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kSlicedOneByteStringMapRootIndex);
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
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
  DCHECK(min_length >= 0);
  if (emit_debug_code()) {
    cmpl(length, Immediate(min_length));
    Assert(greater_equal, kInvalidMinLength);
  }
  Label short_loop, len8, len16, len24, done, short_string;

  const int kLongStringLimit = 4 * kPointerSize;
  if (min_length <= kLongStringLimit) {
    cmpl(length, Immediate(kPointerSize));
    j(below, &short_string, Label::kNear);
  }

  DCHECK(source.is(rsi));
  DCHECK(destination.is(rdi));
  DCHECK(length.is(rcx));

  if (min_length <= kLongStringLimit) {
    cmpl(length, Immediate(2 * kPointerSize));
    j(below_equal, &len8, Label::kNear);
    cmpl(length, Immediate(3 * kPointerSize));
    j(below_equal, &len16, Label::kNear);
    cmpl(length, Immediate(4 * kPointerSize));
    j(below_equal, &len24, Label::kNear);
  }

  // Because source is 8-byte aligned in our uses of this function,
  // we keep source aligned for the rep movs operation by copying the odd bytes
  // at the end of the ranges.
  movp(scratch, length);
  shrl(length, Immediate(kPointerSizeLog2));
  repmovsp();
  // Move remaining bytes of length.
  andl(scratch, Immediate(kPointerSize - 1));
  movp(length, Operand(source, scratch, times_1, -kPointerSize));
  movp(Operand(destination, scratch, times_1, -kPointerSize), length);
  addp(destination, scratch);

  if (min_length <= kLongStringLimit) {
    jmp(&done, Label::kNear);
    bind(&len24);
    movp(scratch, Operand(source, 2 * kPointerSize));
    movp(Operand(destination, 2 * kPointerSize), scratch);
    bind(&len16);
    movp(scratch, Operand(source, kPointerSize));
    movp(Operand(destination, kPointerSize), scratch);
    bind(&len8);
    movp(scratch, Operand(source, 0));
    movp(Operand(destination, 0), scratch);
    // Move remaining bytes of length.
    movp(scratch, Operand(source, length, times_1, -kPointerSize));
    movp(Operand(destination, length, times_1, -kPointerSize), scratch);
    addp(destination, length);
    jmp(&done, Label::kNear);

    bind(&short_string);
    if (min_length == 0) {
      testl(length, length);
      j(zero, &done, Label::kNear);
    }

    bind(&short_loop);
    movb(scratch, Operand(source, 0));
    movb(Operand(destination, 0), scratch);
    incp(source);
    incp(destination);
    decl(length);
    j(not_zero, &short_loop);
  }

  bind(&done);
}


void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label loop, entry;
  jmp(&entry);
  bind(&loop);
  movp(Operand(start_offset, 0), filler);
  addp(start_offset, Immediate(kPointerSize));
  bind(&entry);
  cmpp(start_offset, end_offset);
  j(less, &loop);
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    movp(dst, Operand(rsi, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      movp(dst, Operand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in rsi).
    movp(dst, rsi);
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
  movp(scratch,
       Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  movp(scratch, FieldOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  movp(scratch, Operand(scratch,
                        Context::SlotOffset(Context::JS_ARRAY_MAPS_INDEX)));

  int offset = expected_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  cmpp(map_in_out, FieldOperand(scratch, offset));
  j(not_equal, no_map_match);

  // Use the transitioned cached map.
  offset = transitioned_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  movp(map_in_out, FieldOperand(scratch, offset));
}


#ifdef _WIN64
static const int kRegisterPassedArguments = 4;
#else
static const int kRegisterPassedArguments = 6;
#endif

void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  movp(function,
       Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  movp(function, FieldOperand(function, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  movp(function, Operand(function, Context::SlotOffset(index)));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map) {
  // Load the initial map.  The global functions all have initial maps.
  movp(map, FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
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
  DCHECK(num_arguments >= 0);
#ifdef _WIN64
  const int kMinimumStackSlots = kRegisterPassedArguments;
  if (num_arguments < kMinimumStackSlots) return kMinimumStackSlots;
  return num_arguments;
#else
  if (num_arguments < kRegisterPassedArguments) return 0;
  return num_arguments - kRegisterPassedArguments;
#endif
}


void MacroAssembler::EmitSeqStringSetCharCheck(Register string,
                                               Register index,
                                               Register value,
                                               uint32_t encoding_mask) {
  Label is_object;
  JumpIfNotSmi(string, &is_object);
  Abort(kNonObject);
  bind(&is_object);

  Push(value);
  movp(value, FieldOperand(string, HeapObject::kMapOffset));
  movzxbp(value, FieldOperand(value, Map::kInstanceTypeOffset));

  andb(value, Immediate(kStringRepresentationMask | kStringEncodingMask));
  cmpp(value, Immediate(encoding_mask));
  Pop(value);
  Check(equal, kUnexpectedStringType);

  // The index is assumed to be untagged coming in, tag it to compare with the
  // string length without using a temp register, it is restored at the end of
  // this function.
  Integer32ToSmi(index, index);
  SmiCompare(index, FieldOperand(string, String::kLengthOffset));
  Check(less, kIndexIsTooLarge);

  SmiCompare(index, Smi::FromInt(0));
  Check(greater_equal, kIndexIsNegative);

  // Restore the index
  SmiToInteger32(index, index);
}


void MacroAssembler::PrepareCallCFunction(int num_arguments) {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  DCHECK(frame_alignment != 0);
  DCHECK(num_arguments >= 0);

  // Make stack end at alignment and allocate space for arguments and old rsp.
  movp(kScratchRegister, rsp);
  DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  subp(rsp, Immediate((argument_slots_on_stack + 1) * kRegisterSize));
  andp(rsp, Immediate(-frame_alignment));
  movp(Operand(rsp, argument_slots_on_stack * kRegisterSize), kScratchRegister);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  LoadAddress(rax, function);
  CallCFunction(rax, num_arguments);
}


void MacroAssembler::CallCFunction(Register function, int num_arguments) {
  DCHECK(has_frame());
  // Check stack alignment.
  if (emit_debug_code()) {
    CheckStackAlignment();
  }

  call(function);
  DCHECK(base::OS::ActivationFrameAlignment() != 0);
  DCHECK(num_arguments >= 0);
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  movp(rsp, Operand(rsp, argument_slots_on_stack * kRegisterSize));
}


#ifdef DEBUG
bool AreAliased(Register reg1,
                Register reg2,
                Register reg3,
                Register reg4,
                Register reg5,
                Register reg6,
                Register reg7,
                Register reg8) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() +
      reg3.is_valid() + reg4.is_valid() + reg5.is_valid() + reg6.is_valid() +
      reg7.is_valid() + reg8.is_valid();

  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();
  if (reg7.is_valid()) regs |= reg7.bit();
  if (reg8.is_valid()) regs |= reg8.bit();
  int n_of_non_aliasing_regs = NumRegs(regs);

  return n_of_valid_regs != n_of_non_aliasing_regs;
}
#endif


CodePatcher::CodePatcher(byte* address, int size)
    : address_(address),
      size_(size),
      masm_(NULL, address, size + Assembler::kGap) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  CpuFeatures::FlushICache(address_, size_);

  // Check that the code was patched as expected.
  DCHECK(masm_.pc_ == address_ + size_);
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,
    int mask,
    Condition cc,
    Label* condition_met,
    Label::Distance condition_met_distance) {
  DCHECK(cc == zero || cc == not_zero);
  if (scratch.is(object)) {
    andp(scratch, Immediate(~Page::kPageAlignmentMask));
  } else {
    movp(scratch, Immediate(~Page::kPageAlignmentMask));
    andp(scratch, object);
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
    movl(scratch, FieldOperand(scratch, Map::kBitField3Offset));
    andl(scratch, Immediate(Map::Deprecated::kMask));
    j(not_zero, if_deprecated);
  }
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register bitmap_scratch,
                                 Register mask_scratch,
                                 Label* on_black,
                                 Label::Distance on_black_distance) {
  DCHECK(!AreAliased(object, bitmap_scratch, mask_scratch, rcx));
  GetMarkBits(object, bitmap_scratch, mask_scratch);

  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  // The mask_scratch register contains a 1 at the position of the first bit
  // and a 0 at all other positions, including the position of the second bit.
  movp(rcx, mask_scratch);
  // Make rcx into a mask that covers both marking bits using the operation
  // rcx = mask | (mask << 1).
  leap(rcx, Operand(mask_scratch, mask_scratch, times_2, 0));
  // Note that we are using a 4-byte aligned 8-byte load.
  andp(rcx, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  cmpp(mask_scratch, rcx);
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
  movp(scratch, FieldOperand(value, HeapObject::kMapOffset));
  CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  j(equal, &is_data_object, Label::kNear);
  DCHECK(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  DCHECK(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
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
  DCHECK(!AreAliased(addr_reg, bitmap_reg, mask_reg, rcx));
  movp(bitmap_reg, addr_reg);
  // Sign extended 32 bit immediate.
  andp(bitmap_reg, Immediate(~Page::kPageAlignmentMask));
  movp(rcx, addr_reg);
  int shift =
      Bitmap::kBitsPerCellLog2 + kPointerSizeLog2 - Bitmap::kBytesPerCellLog2;
  shrl(rcx, Immediate(shift));
  andp(rcx,
       Immediate((Page::kPageAlignmentMask >> shift) &
                 ~(Bitmap::kBytesPerCell - 1)));

  addp(bitmap_reg, rcx);
  movp(rcx, addr_reg);
  shrl(rcx, Immediate(kPointerSizeLog2));
  andp(rcx, Immediate((1 << Bitmap::kBitsPerCellLog2) - 1));
  movl(mask_reg, Immediate(1));
  shlp_cl(mask_reg);
}


void MacroAssembler::EnsureNotWhite(
    Register value,
    Register bitmap_scratch,
    Register mask_scratch,
    Label* value_is_white_and_not_data,
    Label::Distance distance) {
  DCHECK(!AreAliased(value, bitmap_scratch, mask_scratch, rcx));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  Label done;

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  testp(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);
  j(not_zero, &done, Label::kNear);

  if (emit_debug_code()) {
    // Check for impossible bit pattern.
    Label ok;
    Push(mask_scratch);
    // shl.  May overflow making the check conservative.
    addp(mask_scratch, mask_scratch);
    testp(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
    Pop(mask_scratch);
  }

  // Value is white.  We check whether it is data that doesn't need scanning.
  // Currently only checks for HeapNumber and non-cons strings.
  Register map = rcx;  // Holds map while checking type.
  Register length = rcx;  // Holds length of object after checking type.
  Label not_heap_number;
  Label is_data_object;

  // Check for heap-number
  movp(map, FieldOperand(value, HeapObject::kMapOffset));
  CompareRoot(map, Heap::kHeapNumberMapRootIndex);
  j(not_equal, &not_heap_number, Label::kNear);
  movp(length, Immediate(HeapNumber::kSize));
  jmp(&is_data_object, Label::kNear);

  bind(&not_heap_number);
  // Check for strings.
  DCHECK(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  DCHECK(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
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
  DCHECK_EQ(0, kSeqStringTag & kExternalStringTag);
  DCHECK_EQ(0, kConsStringTag & kExternalStringTag);
  testb(instance_type, Immediate(kExternalStringTag));
  j(zero, &not_external, Label::kNear);
  movp(length, Immediate(ExternalString::kSize));
  jmp(&is_data_object, Label::kNear);

  bind(&not_external);
  // Sequential string, either Latin1 or UC16.
  DCHECK(kOneByteStringTag == 0x04);
  andp(length, Immediate(kStringEncodingMask));
  xorp(length, Immediate(kStringEncodingMask));
  addp(length, Immediate(0x04));
  // Value now either 4 (if Latin1) or 8 (if UC16), i.e. char-size shifted by 2.
  imulp(length, FieldOperand(value, String::kLengthOffset));
  shrp(length, Immediate(2 + kSmiTagSize + kSmiShiftSize));
  addp(length, Immediate(SeqString::kHeaderSize + kObjectAlignmentMask));
  andp(length, Immediate(~kObjectAlignmentMask));

  bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  orp(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);

  andp(bitmap_scratch, Immediate(~Page::kPageAlignmentMask));
  addl(Operand(bitmap_scratch, MemoryChunk::kLiveBytesOffset), length);

  bind(&done);
}


void MacroAssembler::CheckEnumCache(Register null_value, Label* call_runtime) {
  Label next, start;
  Register empty_fixed_array_value = r8;
  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  movp(rcx, rax);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  movp(rbx, FieldOperand(rcx, HeapObject::kMapOffset));

  EnumLength(rdx, rbx);
  Cmp(rdx, Smi::FromInt(kInvalidEnumCacheSentinel));
  j(equal, call_runtime);

  jmp(&start);

  bind(&next);

  movp(rbx, FieldOperand(rcx, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(rdx, rbx);
  Cmp(rdx, Smi::FromInt(0));
  j(not_equal, call_runtime);

  bind(&start);

  // Check that there are no elements. Register rcx contains the current JS
  // object we've reached through the prototype chain.
  Label no_elements;
  cmpp(empty_fixed_array_value,
       FieldOperand(rcx, JSObject::kElementsOffset));
  j(equal, &no_elements);

  // Second chance, the object may be using the empty slow element dictionary.
  LoadRoot(kScratchRegister, Heap::kEmptySlowElementDictionaryRootIndex);
  cmpp(kScratchRegister, FieldOperand(rcx, JSObject::kElementsOffset));
  j(not_equal, call_runtime);

  bind(&no_elements);
  movp(rcx, FieldOperand(rbx, Map::kPrototypeOffset));
  cmpp(rcx, null_value);
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

  leap(scratch_reg, Operand(receiver_reg,
      JSArray::kSize + AllocationMemento::kSize - kHeapObjectTag));
  Move(kScratchRegister, new_space_start);
  cmpp(scratch_reg, kScratchRegister);
  j(less, no_memento_found);
  cmpp(scratch_reg, ExternalOperand(new_space_allocation_top));
  j(greater, no_memento_found);
  CompareRoot(MemOperand(scratch_reg, -AllocationMemento::kSize),
              Heap::kAllocationMementoMapRootIndex);
}


void MacroAssembler::JumpIfDictionaryInPrototypeChain(
    Register object,
    Register scratch0,
    Register scratch1,
    Label* found) {
  DCHECK(!(scratch0.is(kScratchRegister) && scratch1.is(kScratchRegister)));
  DCHECK(!scratch1.is(scratch0));
  Register current = scratch0;
  Label loop_again;

  movp(current, object);

  // Loop based on the map going up the prototype chain.
  bind(&loop_again);
  movp(current, FieldOperand(current, HeapObject::kMapOffset));
  movp(scratch1, FieldOperand(current, Map::kBitField2Offset));
  DecodeField<Map::ElementsKindBits>(scratch1);
  cmpp(scratch1, Immediate(DICTIONARY_ELEMENTS));
  j(equal, found);
  movp(current, FieldOperand(current, Map::kPrototypeOffset));
  CompareRoot(current, Heap::kNullValueRootIndex);
  j(not_equal, &loop_again);
}


void MacroAssembler::TruncatingDiv(Register dividend, int32_t divisor) {
  DCHECK(!dividend.is(rax));
  DCHECK(!dividend.is(rdx));
  base::MagicNumbersForDivision<uint32_t> mag =
      base::SignedDivisionByConstant(static_cast<uint32_t>(divisor));
  movl(rax, Immediate(mag.multiplier));
  imull(dividend);
  bool neg = (mag.multiplier & (static_cast<uint32_t>(1) << 31)) != 0;
  if (divisor > 0 && neg) addl(rdx, dividend);
  if (divisor < 0 && !neg && mag.multiplier > 0) subl(rdx, dividend);
  if (mag.shift > 0) sarl(rdx, Immediate(mag.shift));
  movl(rax, dividend);
  shrl(rax, Immediate(31));
  addl(rdx, rax);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64

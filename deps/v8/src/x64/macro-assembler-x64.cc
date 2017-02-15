// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/debug/debug.h"
#include "src/heap/heap.h"
#include "src/register-configuration.h"
#include "src/x64/assembler-x64.h"
#include "src/x64/macro-assembler-x64.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size,
                               CodeObjectRequired create_code_object)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      has_frame_(false),
      root_array_available_(true) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ =
        Handle<Object>::New(isolate()->heap()->undefined_value(), isolate());
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
  DCHECK(Heap::RootCanBeWrittenAfterInitialization(index));
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
  ExternalReference store_buffer =
      ExternalReference::store_buffer_top(isolate());
  movp(scratch, ExternalOperand(store_buffer));
  // Store pointer to buffer.
  movp(Operand(scratch, 0), addr);
  // Increment buffer top.
  addp(scratch, Immediate(kPointerSize));
  // Write back new top of buffer.
  movp(ExternalOperand(store_buffer), scratch);
  // Call stub on end of buffer.
  Label done;
  // Check for end of buffer.
  testp(scratch, Immediate(StoreBuffer::kStoreBufferMask));
  if (and_then == kReturnAtEnd) {
    Label buffer_overflowed;
    j(equal, &buffer_overflowed, Label::kNear);
    ret(0);
    bind(&buffer_overflowed);
  } else {
    DCHECK(and_then == kFallThroughAtEnd);
    j(not_equal, &done, Label::kNear);
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
  CheckPageFlag(object, scratch, MemoryChunk::kIsInNewSpaceMask, cc, branch,
                distance);
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

void MacroAssembler::RecordWriteCodeEntryField(Register js_function,
                                               Register code_entry,
                                               Register scratch) {
  const int offset = JSFunction::kCodeEntryOffset;

  // The input registers are fixed to make calling the C write barrier function
  // easier.
  DCHECK(js_function.is(rdi));
  DCHECK(code_entry.is(rcx));
  DCHECK(scratch.is(r15));

  // Since a code entry (value) is always in old space, we don't need to update
  // remembered set. If incremental marking is off, there is nothing for us to
  // do.
  if (!FLAG_incremental_marking) return;

  AssertNotSmi(js_function);

  if (emit_debug_code()) {
    Label ok;
    leap(scratch, FieldOperand(js_function, offset));
    cmpp(code_entry, Operand(scratch, 0));
    j(equal, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis and stores into young gen.
  Label done;

  CheckPageFlag(code_entry, scratch,
                MemoryChunk::kPointersToHereAreInterestingMask, zero, &done,
                Label::kNear);
  CheckPageFlag(js_function, scratch,
                MemoryChunk::kPointersFromHereAreInterestingMask, zero, &done,
                Label::kNear);

  // Save input registers.
  Push(js_function);
  Push(code_entry);

  const Register dst = scratch;
  leap(dst, FieldOperand(js_function, offset));

  // Save caller-saved registers.
  PushCallerSaved(kDontSaveFPRegs, js_function, code_entry);

  int argument_count = 3;
  PrepareCallCFunction(argument_count);

  // Load the argument registers.
  if (arg_reg_1.is(rcx)) {
    // Windows calling convention.
    DCHECK(arg_reg_2.is(rdx) && arg_reg_3.is(r8));

    movp(arg_reg_1, js_function);  // rcx gets rdi.
    movp(arg_reg_2, dst);          // rdx gets r15.
  } else {
    // AMD64 calling convention.
    DCHECK(arg_reg_1.is(rdi) && arg_reg_2.is(rsi) && arg_reg_3.is(rdx));

    // rdi is already loaded with js_function.
    movp(arg_reg_2, dst);  // rsi gets r15.
  }
  Move(arg_reg_3, ExternalReference::isolate_address(isolate()));

  {
    AllowExternalCallThatCantCauseGC scope(this);
    CallCFunction(
        ExternalReference::incremental_marking_record_write_code_entry_function(
            isolate()),
        argument_count);
  }

  // Restore caller-saved registers.
  PopCallerSaved(kDontSaveFPRegs, js_function, code_entry);

  // Restore input registers.
  Pop(code_entry);
  Pop(js_function);

  bind(&done);
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

  // Check if Abort() has already been initialized.
  DCHECK(isolate()->builtins()->Abort()->IsHeapObject());

  Move(rdx, Smi::FromInt(static_cast<int>(reason)));

  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(isolate()->builtins()->Abort(), RelocInfo::CODE_TARGET);
  } else {
    Call(isolate()->builtins()->Abort(), RelocInfo::CODE_TARGET);
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


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid) {
  // ----------- S t a t e -------------
  //  -- rsp[0]                 : return address
  //  -- rsp[8]                 : argument num_arguments - 1
  //  ...
  //  -- rsp[8 * num_arguments] : argument 0 (receiver)
  //
  //  For runtime functions with variable arguments:
  //  -- rax                    : number of  arguments
  // -----------------------------------

  const Runtime::Function* function = Runtime::FunctionForId(fid);
  DCHECK_EQ(1, function->result_size);
  if (function->nargs >= 0) {
    Set(rax, function->nargs);
  }
  JumpToExternalReference(ExternalReference(fid, isolate()));
}

void MacroAssembler::JumpToExternalReference(const ExternalReference& ext,
                                             bool builtin_exit_frame) {
  // Set the entry point and jump to the C entry runtime stub.
  LoadAddress(rbx, ext);
  CEntryStub ces(isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                 builtin_exit_frame);
  jmp(ces.GetCode(), RelocInfo::CODE_TARGET);
}

#define REG(Name) \
  { Register::kCode_##Name }

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
      Movsd(Operand(rsp, i * kDoubleSize), reg);
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
      Movsd(reg, Operand(rsp, i * kDoubleSize));
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


void MacroAssembler::Cvtss2sd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, src, src);
  } else {
    cvtss2sd(dst, src);
  }
}


void MacroAssembler::Cvtss2sd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, dst, src);
  } else {
    cvtss2sd(dst, src);
  }
}


void MacroAssembler::Cvtsd2ss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, src, src);
  } else {
    cvtsd2ss(dst, src);
  }
}


void MacroAssembler::Cvtsd2ss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, dst, src);
  } else {
    cvtsd2ss(dst, src);
  }
}


void MacroAssembler::Cvtlsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtlsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}


void MacroAssembler::Cvtlsi2sd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtlsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}


void MacroAssembler::Cvtlsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtlsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}


void MacroAssembler::Cvtlsi2ss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtlsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}


void MacroAssembler::Cvtqsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtqsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}


void MacroAssembler::Cvtqsi2ss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtqsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}


void MacroAssembler::Cvtqsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtqsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}


void MacroAssembler::Cvtqsi2sd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtqsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}


void MacroAssembler::Cvtqui2ss(XMMRegister dst, Register src, Register tmp) {
  Label msb_set_src;
  Label jmp_return;
  testq(src, src);
  j(sign, &msb_set_src, Label::kNear);
  Cvtqsi2ss(dst, src);
  jmp(&jmp_return, Label::kNear);
  bind(&msb_set_src);
  movq(tmp, src);
  shrq(src, Immediate(1));
  // Recover the least significant bit to avoid rounding errors.
  andq(tmp, Immediate(1));
  orq(src, tmp);
  Cvtqsi2ss(dst, src);
  addss(dst, dst);
  bind(&jmp_return);
}


void MacroAssembler::Cvtqui2sd(XMMRegister dst, Register src, Register tmp) {
  Label msb_set_src;
  Label jmp_return;
  testq(src, src);
  j(sign, &msb_set_src, Label::kNear);
  Cvtqsi2sd(dst, src);
  jmp(&jmp_return, Label::kNear);
  bind(&msb_set_src);
  movq(tmp, src);
  shrq(src, Immediate(1));
  andq(tmp, Immediate(1));
  orq(src, tmp);
  Cvtqsi2sd(dst, src);
  addsd(dst, dst);
  bind(&jmp_return);
}


void MacroAssembler::Cvtsd2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2si(dst, src);
  } else {
    cvtsd2si(dst, src);
  }
}


void MacroAssembler::Cvttss2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}


void MacroAssembler::Cvttss2si(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}


void MacroAssembler::Cvttsd2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}


void MacroAssembler::Cvttsd2si(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}


void MacroAssembler::Cvttss2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}


void MacroAssembler::Cvttss2siq(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}


void MacroAssembler::Cvttsd2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2siq(dst, src);
  } else {
    cvttsd2siq(dst, src);
  }
}


void MacroAssembler::Cvttsd2siq(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2siq(dst, src);
  } else {
    cvttsd2siq(dst, src);
  }
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
  STATIC_ASSERT(kSmiTag == 0);
  int value = source->value();
  if (value == 0) {
    xorl(kScratchRegister, kScratchRegister);
    return kScratchRegister;
  }
  LoadSmiConstant(kScratchRegister, source);
  return kScratchRegister;
}


void MacroAssembler::LoadSmiConstant(Register dst, Smi* source) {
  STATIC_ASSERT(kSmiTag == 0);
  int value = source->value();
  if (value == 0) {
    xorl(dst, dst);
  } else {
    Move(dst, source, Assembler::RelocInfoNone());
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
    Register constant_reg = GetSmiConstant(constant);
    addp(dst, constant_reg);
  } else {
    LoadSmiConstant(dst, constant);
    addp(dst, src);
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


void MacroAssembler::SmiAddConstant(Register dst, Register src, Smi* constant,
                                    SmiOperationConstraints constraints,
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
    if (constraints & SmiOperationConstraint::kBailoutOnNoOverflow) {
      j(no_overflow, bailout_label, near_jump);
      DCHECK(constraints & SmiOperationConstraint::kPreserveSourceRegister);
      subp(dst, kScratchRegister);
    } else if (constraints & SmiOperationConstraint::kBailoutOnOverflow) {
      if (constraints & SmiOperationConstraint::kPreserveSourceRegister) {
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
      UNREACHABLE();
    }
  } else {
    DCHECK(constraints & SmiOperationConstraint::kPreserveSourceRegister);
    DCHECK(constraints & SmiOperationConstraint::kBailoutOnOverflow);
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


void MacroAssembler::SmiSubConstant(Register dst, Register src, Smi* constant,
                                    SmiOperationConstraints constraints,
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
    if (constraints & SmiOperationConstraint::kBailoutOnNoOverflow) {
      j(no_overflow, bailout_label, near_jump);
      DCHECK(constraints & SmiOperationConstraint::kPreserveSourceRegister);
      addp(dst, kScratchRegister);
    } else if (constraints & SmiOperationConstraint::kBailoutOnOverflow) {
      if (constraints & SmiOperationConstraint::kPreserveSourceRegister) {
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
      UNREACHABLE();
    }
  } else {
    DCHECK(constraints & SmiOperationConstraint::kPreserveSourceRegister);
    DCHECK(constraints & SmiOperationConstraint::kBailoutOnOverflow);
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
  DCHECK_EQ(static_cast<Smi*>(0), Smi::FromInt(0));
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
    Xorpd(dst, dst);
  } else {
    unsigned pop = base::bits::CountPopulation32(src);
    DCHECK_NE(0u, pop);
    if (pop == 32) {
      Pcmpeqd(dst, dst);
    } else {
      movl(kScratchRegister, Immediate(src));
      Movq(dst, kScratchRegister);
    }
  }
}


void MacroAssembler::Move(XMMRegister dst, uint64_t src) {
  if (src == 0) {
    Xorpd(dst, dst);
  } else {
    unsigned nlz = base::bits::CountLeadingZeros64(src);
    unsigned ntz = base::bits::CountTrailingZeros64(src);
    unsigned pop = base::bits::CountPopulation64(src);
    DCHECK_NE(0u, pop);
    if (pop == 64) {
      Pcmpeqd(dst, dst);
    } else if (pop + ntz == 64) {
      Pcmpeqd(dst, dst);
      Psllq(dst, ntz);
    } else if (pop + nlz == 64) {
      Pcmpeqd(dst, dst);
      Psrlq(dst, nlz);
    } else {
      uint32_t lower = static_cast<uint32_t>(src);
      uint32_t upper = static_cast<uint32_t>(src >> 32);
      if (upper == 0) {
        Move(dst, lower);
      } else {
        movq(kScratchRegister, src);
        Movq(dst, kScratchRegister);
      }
    }
  }
}


void MacroAssembler::Movaps(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovaps(dst, src);
  } else {
    movaps(dst, src);
  }
}

void MacroAssembler::Movups(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovups(dst, src);
  } else {
    movups(dst, src);
  }
}

void MacroAssembler::Movups(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovups(dst, src);
  } else {
    movups(dst, src);
  }
}

void MacroAssembler::Movups(const Operand& dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovups(dst, src);
  } else {
    movups(dst, src);
  }
}

void MacroAssembler::Movapd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovapd(dst, src);
  } else {
    movapd(dst, src);
  }
}

void MacroAssembler::Movupd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovupd(dst, src);
  } else {
    movupd(dst, src);
  }
}

void MacroAssembler::Movupd(const Operand& dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovupd(dst, src);
  } else {
    movupd(dst, src);
  }
}

void MacroAssembler::Movsd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovsd(dst, dst, src);
  } else {
    movsd(dst, src);
  }
}


void MacroAssembler::Movsd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovsd(dst, src);
  } else {
    movsd(dst, src);
  }
}


void MacroAssembler::Movsd(const Operand& dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovsd(dst, src);
  } else {
    movsd(dst, src);
  }
}


void MacroAssembler::Movss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovss(dst, dst, src);
  } else {
    movss(dst, src);
  }
}


void MacroAssembler::Movss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovss(dst, src);
  } else {
    movss(dst, src);
  }
}


void MacroAssembler::Movss(const Operand& dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovss(dst, src);
  } else {
    movss(dst, src);
  }
}


void MacroAssembler::Movd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovd(dst, src);
  } else {
    movd(dst, src);
  }
}


void MacroAssembler::Movd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovd(dst, src);
  } else {
    movd(dst, src);
  }
}


void MacroAssembler::Movd(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovd(dst, src);
  } else {
    movd(dst, src);
  }
}


void MacroAssembler::Movq(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}


void MacroAssembler::Movq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}

void MacroAssembler::Movmskps(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovmskps(dst, src);
  } else {
    movmskps(dst, src);
  }
}

void MacroAssembler::Movmskpd(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovmskpd(dst, src);
  } else {
    movmskpd(dst, src);
  }
}

void MacroAssembler::Xorps(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, src);
  } else {
    xorps(dst, src);
  }
}

void MacroAssembler::Xorps(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, src);
  } else {
    xorps(dst, src);
  }
}

void MacroAssembler::Roundss(XMMRegister dst, XMMRegister src,
                             RoundingMode mode) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vroundss(dst, dst, src, mode);
  } else {
    roundss(dst, src, mode);
  }
}


void MacroAssembler::Roundsd(XMMRegister dst, XMMRegister src,
                             RoundingMode mode) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vroundsd(dst, dst, src, mode);
  } else {
    roundsd(dst, src, mode);
  }
}


void MacroAssembler::Sqrtsd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsqrtsd(dst, dst, src);
  } else {
    sqrtsd(dst, src);
  }
}


void MacroAssembler::Sqrtsd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsqrtsd(dst, dst, src);
  } else {
    sqrtsd(dst, src);
  }
}


void MacroAssembler::Ucomiss(XMMRegister src1, XMMRegister src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vucomiss(src1, src2);
  } else {
    ucomiss(src1, src2);
  }
}


void MacroAssembler::Ucomiss(XMMRegister src1, const Operand& src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vucomiss(src1, src2);
  } else {
    ucomiss(src1, src2);
  }
}


void MacroAssembler::Ucomisd(XMMRegister src1, XMMRegister src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vucomisd(src1, src2);
  } else {
    ucomisd(src1, src2);
  }
}


void MacroAssembler::Ucomisd(XMMRegister src1, const Operand& src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vucomisd(src1, src2);
  } else {
    ucomisd(src1, src2);
  }
}

// ----------------------------------------------------------------------------

void MacroAssembler::Absps(XMMRegister dst) {
  Andps(dst,
        ExternalOperand(ExternalReference::address_of_float_abs_constant()));
}

void MacroAssembler::Negps(XMMRegister dst) {
  Xorps(dst,
        ExternalOperand(ExternalReference::address_of_float_neg_constant()));
}

void MacroAssembler::Abspd(XMMRegister dst) {
  Andps(dst,
        ExternalOperand(ExternalReference::address_of_double_abs_constant()));
}

void MacroAssembler::Negpd(XMMRegister dst) {
  Xorps(dst,
        ExternalOperand(ExternalReference::address_of_double_neg_constant()));
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
  DCHECK(object->IsHeapObject());
  Move(result, object, RelocInfo::EMBEDDED_OBJECT);
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


void MacroAssembler::CmpWeakValue(Register value, Handle<WeakCell> cell,
                                  Register scratch) {
  Move(scratch, cell, RelocInfo::EMBEDDED_OBJECT);
  cmpp(value, FieldOperand(scratch, WeakCell::kValueOffset));
}


void MacroAssembler::GetWeakValue(Register value, Handle<WeakCell> cell) {
  Move(value, cell, RelocInfo::EMBEDDED_OBJECT);
  movp(value, FieldOperand(value, WeakCell::kValueOffset));
}


void MacroAssembler::LoadWeakValue(Register value, Handle<WeakCell> cell,
                                   Label* miss) {
  GetWeakValue(value, cell);
  JumpIfSmi(value, miss);
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
        ? kRootRegister : kScratchRegister;
    movp(scratch, Operand(rsp, 0));
    movp(dst, scratch);
    leal(rsp, Operand(rsp, 4));
    if (scratch.is(kRootRegister)) {
      // Restore kRootRegister.
      InitializeRootRegister();
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
  if (kPointerSize == kInt64Size && !CpuFeatures::IsSupported(ATOM)) {
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


void MacroAssembler::Pextrd(Register dst, XMMRegister src, int8_t imm8) {
  if (imm8 == 0) {
    Movd(dst, src);
    return;
  }
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pextrd(dst, src, imm8);
    return;
  }
  DCHECK_EQ(1, imm8);
  movq(dst, src);
  shrq(dst, Immediate(32));
}


void MacroAssembler::Pinsrd(XMMRegister dst, Register src, int8_t imm8) {
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pinsrd(dst, src, imm8);
    return;
  }
  Movd(kScratchDoubleReg, src);
  if (imm8 == 1) {
    punpckldq(dst, kScratchDoubleReg);
  } else {
    DCHECK_EQ(0, imm8);
    Movss(dst, kScratchDoubleReg);
  }
}


void MacroAssembler::Pinsrd(XMMRegister dst, const Operand& src, int8_t imm8) {
  DCHECK(imm8 == 0 || imm8 == 1);
  if (CpuFeatures::IsSupported(SSE4_1)) {
    CpuFeatureScope sse_scope(this, SSE4_1);
    pinsrd(dst, src, imm8);
    return;
  }
  Movd(kScratchDoubleReg, src);
  if (imm8 == 1) {
    punpckldq(dst, kScratchDoubleReg);
  } else {
    DCHECK_EQ(0, imm8);
    Movss(dst, kScratchDoubleReg);
  }
}


void MacroAssembler::Lzcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsrl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Set(dst, 63);  // 63^31 == 32
  bind(&not_zero_src);
  xorl(dst, Immediate(31));  // for x in [0..31], 31^x == 31 - x
}


void MacroAssembler::Lzcntl(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsrl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Set(dst, 63);  // 63^31 == 32
  bind(&not_zero_src);
  xorl(dst, Immediate(31));  // for x in [0..31], 31^x == 31 - x
}


void MacroAssembler::Lzcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsrq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Set(dst, 127);  // 127^63 == 64
  bind(&not_zero_src);
  xorl(dst, Immediate(63));  // for x in [0..63], 63^x == 63 - x
}


void MacroAssembler::Lzcntq(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(LZCNT)) {
    CpuFeatureScope scope(this, LZCNT);
    lzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsrq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Set(dst, 127);  // 127^63 == 64
  bind(&not_zero_src);
  xorl(dst, Immediate(63));  // for x in [0..63], 63^x == 63 - x
}


void MacroAssembler::Tzcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsfq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  // Define the result of tzcnt(0) separately, because bsf(0) is undefined.
  Set(dst, 64);
  bind(&not_zero_src);
}


void MacroAssembler::Tzcntq(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntq(dst, src);
    return;
  }
  Label not_zero_src;
  bsfq(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  // Define the result of tzcnt(0) separately, because bsf(0) is undefined.
  Set(dst, 64);
  bind(&not_zero_src);
}


void MacroAssembler::Tzcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsfl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Set(dst, 32);  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}


void MacroAssembler::Tzcntl(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(BMI1)) {
    CpuFeatureScope scope(this, BMI1);
    tzcntl(dst, src);
    return;
  }
  Label not_zero_src;
  bsfl(dst, src);
  j(not_zero, &not_zero_src, Label::kNear);
  Set(dst, 32);  // The result of tzcnt is 32 if src = 0.
  bind(&not_zero_src);
}


void MacroAssembler::Popcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}


void MacroAssembler::Popcntl(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}


void MacroAssembler::Popcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntq(dst, src);
    return;
  }
  UNREACHABLE();
}


void MacroAssembler::Popcntq(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntq(dst, src);
    return;
  }
  UNREACHABLE();
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
  Push(r12);
  // r13 is kRootRegister.
  Push(r14);
  Push(r15);
  STATIC_ASSERT(12 == kNumSafepointSavedRegisters);
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
  Pop(r12);
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
    9,
    -1,
    10,
    11
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


void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  // Link the current handler as the next handler.
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  Push(ExternalOperand(handler_address));

  // Set this new handler as the current one.
  movp(ExternalOperand(handler_address), rsp);
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address(Isolate::kHandlerAddress, isolate());
  Pop(ExternalOperand(handler_address));
  addp(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));
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
  Label smi_value, done;

  JumpIfSmi(maybe_number, &smi_value, Label::kNear);

  CheckMap(maybe_number,
           isolate()->factory()->heap_number_map(),
           fail,
           DONT_DO_SMI_CHECK);

  // Double value, turn potential sNaN into qNaN.
  Move(xmm_scratch, 1.0);
  mulsd(xmm_scratch, FieldOperand(maybe_number, HeapNumber::kValueOffset));
  jmp(&done, Label::kNear);

  bind(&smi_value);
  // Value is a smi. convert to a double and store.
  // Preserve original value.
  SmiToInteger32(kScratchRegister, maybe_number);
  Cvtlsi2sd(xmm_scratch, kScratchRegister);
  bind(&done);
  Movsd(FieldOperand(elements, index, times_8,
                     FixedDoubleArray::kHeaderSize - elements_offset),
        xmm_scratch);
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
  Xorpd(temp_xmm_reg, temp_xmm_reg);
  Cvtsd2si(result_reg, input_reg);
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
  Ucomisd(input_reg, temp_xmm_reg);
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
  Cvtqsi2sd(dst, src);
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
  Movsd(kScratchDoubleReg, FieldOperand(input_reg, HeapNumber::kValueOffset));
  Cvttsd2siq(result_reg, kScratchDoubleReg);
  cmpq(result_reg, Immediate(1));
  j(no_overflow, &done, Label::kNear);

  // Slow case.
  if (input_reg.is(result_reg)) {
    subp(rsp, Immediate(kDoubleSize));
    Movsd(MemOperand(rsp, 0), kScratchDoubleReg);
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
  Cvttsd2siq(result_reg, input_reg);
  cmpq(result_reg, Immediate(1));
  j(no_overflow, &done, Label::kNear);

  subp(rsp, Immediate(kDoubleSize));
  Movsd(MemOperand(rsp, 0), input_reg);
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
  Cvttsd2si(result_reg, input_reg);
  Cvtlsi2sd(kScratchDoubleReg, result_reg);
  Ucomisd(kScratchDoubleReg, input_reg);
  j(not_equal, lost_precision, dst);
  j(parity_even, is_nan, dst);  // NaN.
  if (minus_zero_mode == FAIL_ON_MINUS_ZERO) {
    Label done;
    // The integer converted back is equal to the original. We
    // only have to test if we got -0 as an input.
    testl(result_reg, result_reg);
    j(not_zero, &done, Label::kNear);
    Movmskpd(result_reg, input_reg);
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


void MacroAssembler::LoadAccessor(Register dst, Register holder,
                                  int accessor_index,
                                  AccessorComponent accessor) {
  movp(dst, FieldOperand(holder, HeapObject::kMapOffset));
  LoadInstanceDescriptors(dst, dst);
  movp(dst, FieldOperand(dst, DescriptorArray::GetValueOffset(accessor_index)));
  int offset = accessor == ACCESSOR_GETTER ? AccessorPair::kGetterOffset
                                           : AccessorPair::kSetterOffset;
  movp(dst, FieldOperand(dst, offset));
}


void MacroAssembler::DispatchWeakMap(Register obj, Register scratch1,
                                     Register scratch2, Handle<WeakCell> cell,
                                     Handle<Code> success,
                                     SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  movq(scratch1, FieldOperand(obj, HeapObject::kMapOffset));
  CmpWeakValue(scratch1, cell, scratch2);
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

void MacroAssembler::AssertNotNumber(Register object) {
  if (emit_debug_code()) {
    Condition is_smi = CheckSmi(object);
    Check(NegateCondition(is_smi), kOperandIsANumber);
    Cmp(FieldOperand(object, HeapObject::kMapOffset),
        isolate()->factory()->heap_number_map());
    Check(not_equal, kOperandIsANumber);
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


void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAFunction);
    Push(object);
    CmpObjectType(object, JS_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, kOperandIsNotAFunction);
  }
}


void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotABoundFunction);
    Push(object);
    CmpObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAGeneratorObject);
    Push(object);
    CmpObjectType(object, JS_GENERATOR_OBJECT_TYPE, object);
    Pop(object);
    Check(equal, kOperandIsNotAGeneratorObject);
  }
}

void MacroAssembler::AssertReceiver(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, kOperandIsASmiAndNotAReceiver);
    Push(object);
    STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
    CmpObjectType(object, FIRST_JS_RECEIVER_TYPE, object);
    Pop(object);
    Check(above_equal, kOperandIsNotAReceiver);
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


void MacroAssembler::GetMapConstructor(Register result, Register map,
                                       Register temp) {
  Label done, loop;
  movp(result, FieldOperand(map, Map::kConstructorOrBackPointerOffset));
  bind(&loop);
  JumpIfSmi(result, &done, Label::kNear);
  CmpObjectType(result, MAP_TYPE, temp);
  j(not_equal, &done, Label::kNear);
  movp(result, FieldOperand(result, Map::kConstructorOrBackPointerOffset));
  jmp(&loop);
  bind(&done);
}


void MacroAssembler::TryGetFunctionPrototype(Register function, Register result,
                                             Label* miss) {
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
  LoadAddress(rbx,
              ExternalReference(Runtime::kHandleDebuggerStatement, isolate()));
  CEntryStub ces(isolate(), 1);
  DCHECK(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUGGER_STATEMENT);
}

void MacroAssembler::PrepareForTailCall(const ParameterCount& callee_args_count,
                                        Register caller_args_count_reg,
                                        Register scratch0, Register scratch1,
                                        ReturnAddressState ra_state) {
#if DEBUG
  if (callee_args_count.is_reg()) {
    DCHECK(!AreAliased(callee_args_count.reg(), caller_args_count_reg, scratch0,
                       scratch1));
  } else {
    DCHECK(!AreAliased(caller_args_count_reg, scratch0, scratch1));
  }
#endif

  // Calculate the destination address where we will put the return address
  // after we drop current frame.
  Register new_sp_reg = scratch0;
  if (callee_args_count.is_reg()) {
    subp(caller_args_count_reg, callee_args_count.reg());
    leap(new_sp_reg, Operand(rbp, caller_args_count_reg, times_pointer_size,
                             StandardFrameConstants::kCallerPCOffset));
  } else {
    leap(new_sp_reg, Operand(rbp, caller_args_count_reg, times_pointer_size,
                             StandardFrameConstants::kCallerPCOffset -
                                 callee_args_count.immediate() * kPointerSize));
  }

  if (FLAG_debug_code) {
    cmpp(rsp, new_sp_reg);
    Check(below, kStackAccessBelowStackPointer);
  }

  // Copy return address from caller's frame to current frame's return address
  // to avoid its trashing and let the following loop copy it to the right
  // place.
  Register tmp_reg = scratch1;
  if (ra_state == ReturnAddressState::kOnStack) {
    movp(tmp_reg, Operand(rbp, StandardFrameConstants::kCallerPCOffset));
    movp(Operand(rsp, 0), tmp_reg);
  } else {
    DCHECK(ReturnAddressState::kNotOnStack == ra_state);
    Push(Operand(rbp, StandardFrameConstants::kCallerPCOffset));
  }

  // Restore caller's frame pointer now as it could be overwritten by
  // the copying loop.
  movp(rbp, Operand(rbp, StandardFrameConstants::kCallerFPOffset));

  // +2 here is to copy both receiver and return address.
  Register count_reg = caller_args_count_reg;
  if (callee_args_count.is_reg()) {
    leap(count_reg, Operand(callee_args_count.reg(), 2));
  } else {
    movp(count_reg, Immediate(callee_args_count.immediate() + 2));
    // TODO(ishell): Unroll copying loop for small immediate values.
  }

  // Now copy callee arguments to the caller frame going backwards to avoid
  // callee arguments corruption (source and destination areas could overlap).
  Label loop, entry;
  jmp(&entry, Label::kNear);
  bind(&loop);
  decp(count_reg);
  movp(tmp_reg, Operand(rsp, count_reg, times_pointer_size, 0));
  movp(Operand(new_sp_reg, count_reg, times_pointer_size, 0), tmp_reg);
  bind(&entry);
  cmpp(count_reg, Immediate(0));
  j(not_equal, &loop, Label::kNear);

  // Leave current frame.
  movp(rsp, new_sp_reg);
}

void MacroAssembler::InvokeFunction(Register function,
                                    Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  movp(rbx, FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
  LoadSharedFunctionInfoSpecialField(
      rbx, rbx, SharedFunctionInfo::kFormalParameterCountOffset);

  ParameterCount expected(rbx);
  InvokeFunction(function, new_target, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  Move(rdi, function);
  InvokeFunction(rdi, no_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register function,
                                    Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  DCHECK(function.is(rdi));
  movp(rsi, FieldOperand(function, JSFunction::kContextOffset));
  InvokeFunctionCode(rdi, new_target, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        const ParameterCount& expected,
                                        const ParameterCount& actual,
                                        InvokeFlag flag,
                                        const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function.is(rdi));
  DCHECK_IMPLIES(new_target.is_valid(), new_target.is(rdx));

  if (call_wrapper.NeedsDebugStepCheck()) {
    FloodFunctionIfStepping(function, new_target, expected, actual);
  }

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected,
                 actual,
                 &done,
                 &definitely_mismatches,
                 flag,
                 Label::kNear,
                 call_wrapper);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    Operand code = FieldOperand(function, JSFunction::kCodeEntryOffset);
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


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
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
    Set(rax, actual.immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
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
      Set(rax, actual.immediate());
      cmpp(expected.reg(), Immediate(actual.immediate()));
      j(equal, &invoke, Label::kNear);
      DCHECK(expected.reg().is(rbx));
    } else if (!expected.reg().is(actual.reg())) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmpp(expected.reg(), actual.reg());
      j(equal, &invoke, Label::kNear);
      DCHECK(actual.reg().is(rax));
      DCHECK(expected.reg().is(rbx));
    } else {
      Move(rax, actual.reg());
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = isolate()->builtins()->ArgumentsAdaptorTrampoline();
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


void MacroAssembler::FloodFunctionIfStepping(Register fun, Register new_target,
                                             const ParameterCount& expected,
                                             const ParameterCount& actual) {
  Label skip_flooding;
  ExternalReference last_step_action =
      ExternalReference::debug_last_step_action_address(isolate());
  Operand last_step_action_operand = ExternalOperand(last_step_action);
  STATIC_ASSERT(StepFrame > StepIn);
  cmpb(last_step_action_operand, Immediate(StepIn));
  j(less, &skip_flooding);
  {
    FrameScope frame(this,
                     has_frame() ? StackFrame::NONE : StackFrame::INTERNAL);
    if (expected.is_reg()) {
      Integer32ToSmi(expected.reg(), expected.reg());
      Push(expected.reg());
    }
    if (actual.is_reg()) {
      Integer32ToSmi(actual.reg(), actual.reg());
      Push(actual.reg());
    }
    if (new_target.is_valid()) {
      Push(new_target);
    }
    Push(fun);
    Push(fun);
    CallRuntime(Runtime::kDebugPrepareStepInIfStepping);
    Pop(fun);
    if (new_target.is_valid()) {
      Pop(new_target);
    }
    if (actual.is_reg()) {
      Pop(actual.reg());
      SmiToInteger64(actual.reg(), actual.reg());
    }
    if (expected.is_reg()) {
      Pop(expected.reg());
      SmiToInteger64(expected.reg(), expected.reg());
    }
  }
  bind(&skip_flooding);
}

void MacroAssembler::StubPrologue(StackFrame::Type type) {
  pushq(rbp);  // Caller's frame pointer.
  movp(rbp, rsp);
  Push(Smi::FromInt(type));
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


void MacroAssembler::EmitLoadTypeFeedbackVector(Register vector) {
  movp(vector, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  movp(vector, FieldOperand(vector, JSFunction::kLiteralsOffset));
  movp(vector, FieldOperand(vector, LiteralsArray::kFeedbackVectorOffset));
}


void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  // Out-of-line constant pool not implemented on x64.
  UNREACHABLE();
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  pushq(rbp);
  movp(rbp, rsp);
  Push(Smi::FromInt(type));
  if (type == StackFrame::INTERNAL) {
    Move(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
    Push(kScratchRegister);
  }
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
    cmpp(Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset),
         kScratchRegister);
    Check(equal, kStackFrameTypesMustMatch);
  }
  movp(rsp, rbp);
  popq(rbp);
}

void MacroAssembler::EnterBuiltinFrame(Register context, Register target,
                                       Register argc) {
  Push(rbp);
  Move(rbp, rsp);
  Push(context);
  Push(target);
  Push(argc);
}

void MacroAssembler::LeaveBuiltinFrame(Register context, Register target,
                                       Register argc) {
  Pop(argc);
  Pop(target);
  Pop(context);
  leave();
}

void MacroAssembler::EnterExitFramePrologue(bool save_rax,
                                            StackFrame::Type frame_type) {
  DCHECK(frame_type == StackFrame::EXIT ||
         frame_type == StackFrame::BUILTIN_EXIT);

  // Set up the frame structure on the stack.
  // All constants are relative to the frame pointer of the exit frame.
  DCHECK_EQ(kFPOnStackSize + kPCOnStackSize,
            ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(kFPOnStackSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  pushq(rbp);
  movp(rbp, rsp);

  // Reserve room for entry stack pointer and push the code object.
  Push(Smi::FromInt(frame_type));
  DCHECK_EQ(-2 * kPointerSize, ExitFrameConstants::kSPOffset);
  Push(Immediate(0));  // Saved entry sp, patched before call.
  Move(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  Push(kScratchRegister);  // Accessed from ExitFrame::code_slot.

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
    int space = XMMRegister::kMaxNumRegisters * kDoubleSize +
                arg_stack_space * kRegisterSize;
    subp(rsp, Immediate(space));
    int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    const RegisterConfiguration* config = RegisterConfiguration::Crankshaft();
    for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
      DoubleRegister reg =
          DoubleRegister::from_code(config->GetAllocatableDoubleCode(i));
      Movsd(Operand(rbp, offset - ((i + 1) * kDoubleSize)), reg);
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

void MacroAssembler::EnterExitFrame(int arg_stack_space, bool save_doubles,
                                    StackFrame::Type frame_type) {
  EnterExitFramePrologue(true, frame_type);

  // Set up argv in callee-saved register r15. It is reused in LeaveExitFrame,
  // so it must be retained across the C-call.
  int offset = StandardFrameConstants::kCallerSPOffset - kPointerSize;
  leap(r15, Operand(rbp, r14, times_pointer_size, offset));

  EnterExitFrameEpilogue(arg_stack_space, save_doubles);
}


void MacroAssembler::EnterApiExitFrame(int arg_stack_space) {
  EnterExitFramePrologue(false, StackFrame::EXIT);
  EnterExitFrameEpilogue(arg_stack_space, false);
}


void MacroAssembler::LeaveExitFrame(bool save_doubles, bool pop_arguments) {
  // Registers:
  // r15 : argv
  if (save_doubles) {
    int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    const RegisterConfiguration* config = RegisterConfiguration::Crankshaft();
    for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
      DoubleRegister reg =
          DoubleRegister::from_code(config->GetAllocatableDoubleCode(i));
      Movsd(reg, Operand(rbp, offset - ((i + 1) * kDoubleSize)));
    }
  }

  if (pop_arguments) {
    // Get the return address from the stack and restore the frame pointer.
    movp(rcx, Operand(rbp, kFPOnStackSize));
    movp(rbp, Operand(rbp, 0 * kPointerSize));

    // Drop everything up to and including the arguments and the receiver
    // from the caller stack.
    leap(rsp, Operand(r15, 1 * kPointerSize));

    PushReturnAddressFrom(rcx);
  } else {
    // Otherwise just leave the exit frame.
    leave();
  }

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
  // Load current lexical context from the active StandardFrame, which
  // may require crawling past STUB frames.
  Label load_context;
  Label has_context;
  movp(scratch, rbp);
  bind(&load_context);
  DCHECK(SmiValuesAre32Bits());
  // This is "JumpIfNotSmi" but without loading the value into a register.
  cmpl(MemOperand(scratch, CommonFrameConstants::kContextOrFrameTypeOffset),
       Immediate(0));
  j(not_equal, &has_context);
  movp(scratch, MemOperand(scratch, CommonFrameConstants::kCallerFPOffset));
  jmp(&load_context);
  bind(&has_context);
  movp(scratch,
       MemOperand(scratch, CommonFrameConstants::kContextOrFrameTypeOffset));

  // When generating debug code, make sure the lexical context is set.
  if (emit_debug_code()) {
    cmpp(scratch, Immediate(0));
    Check(not_equal, kWeShouldNotHaveAnEmptyLexicalContext);
  }
  // Load the native context of the current context.
  movp(scratch, ContextOperand(scratch, Context::NATIVE_CONTEXT_INDEX));

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
  andl(r0, Immediate(0x3fffffff));
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
  // Check that the value is a field property.
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  DCHECK_EQ(DATA, 0);
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
    DCHECK(kPointerAlignment * 2 == kDoubleAlignment);
    // Make sure scratch is not clobbered by this function as it might be
    // used in UpdateAllocationTopHelper later.
    DCHECK(!scratch.is(kScratchRegister));
    Label aligned;
    testl(result, Immediate(kDoubleAlignmentMask));
    j(zero, &aligned, Label::kNear);
    if (((flags & ALLOCATION_FOLDED) == 0) && ((flags & PRETENURE) != 0)) {
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
  DCHECK(object_size <= kMaxRegularHeapObjectSize);
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
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
  Operand limit_operand = ExternalOperand(allocation_limit);
  cmpp(top_reg, limit_operand);
  j(above, gc_required);

  if ((flags & ALLOCATION_FOLDING_DOMINATOR) == 0) {
    // The top pointer is not updated for allocation folding dominators.
    UpdateAllocationTopHelper(top_reg, scratch, flags);
  }

  if (top_reg.is(result)) {
    subp(result, Immediate(object_size - kHeapObjectTag));
  } else {
    // Tag the result.
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
  DCHECK((flags & ALLOCATION_FOLDING_DOMINATOR) == 0);
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
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
  DCHECK((flags & ALLOCATION_FOLDED) == 0);
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

  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  if (!object_size.is(result_end)) {
    movp(result_end, object_size);
  }
  addp(result_end, result);
  Operand limit_operand = ExternalOperand(allocation_limit);
  cmpp(result_end, limit_operand);
  j(above, gc_required);

  if ((flags & ALLOCATION_FOLDING_DOMINATOR) == 0) {
    // The top pointer is not updated for allocation folding dominators.
    UpdateAllocationTopHelper(result_end, scratch, flags);
  }

  // Tag the result.
  addp(result, Immediate(kHeapObjectTag));
}

void MacroAssembler::FastAllocate(int object_size, Register result,
                                  Register result_end, AllocationFlags flags) {
  DCHECK(!result.is(result_end));
  // Load address of new object into result.
  LoadAllocationTopHelper(result, no_reg, flags);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    MakeSureDoubleAlignedHelper(result, no_reg, NULL, flags);
  }

  leap(result_end, Operand(result, object_size));

  UpdateAllocationTopHelper(result_end, no_reg, flags);

  addp(result, Immediate(kHeapObjectTag));
}

void MacroAssembler::FastAllocate(Register object_size, Register result,
                                  Register result_end, AllocationFlags flags) {
  DCHECK(!result.is(result_end));
  // Load address of new object into result.
  LoadAllocationTopHelper(result, no_reg, flags);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    MakeSureDoubleAlignedHelper(result, no_reg, NULL, flags);
  }

  leap(result_end, Operand(result, object_size, times_1, 0));

  UpdateAllocationTopHelper(result_end, no_reg, flags);

  addp(result, Immediate(kHeapObjectTag));
}

void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch,
                                        Label* gc_required,
                                        MutableMode mode) {
  // Allocate heap number in new space.
  Allocate(HeapNumber::kSize, result, scratch, no_reg, gc_required,
           NO_ALLOCATION_FLAGS);

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
  Allocate(SeqTwoByteString::kHeaderSize, times_1, scratch1, result, scratch2,
           scratch3, gc_required, NO_ALLOCATION_FLAGS);

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
  Allocate(SeqOneByteString::kHeaderSize, times_1, scratch1, result, scratch2,
           scratch3, gc_required, NO_ALLOCATION_FLAGS);

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
           NO_ALLOCATION_FLAGS);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kConsStringMapRootIndex);
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateOneByteConsString(Register result,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           NO_ALLOCATION_FLAGS);

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
           NO_ALLOCATION_FLAGS);

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
           NO_ALLOCATION_FLAGS);

  // Set the map. The other fields are left uninitialized.
  LoadRoot(kScratchRegister, Heap::kSlicedOneByteStringMapRootIndex);
  movp(FieldOperand(result, HeapObject::kMapOffset), kScratchRegister);
}


void MacroAssembler::AllocateJSValue(Register result, Register constructor,
                                     Register value, Register scratch,
                                     Label* gc_required) {
  DCHECK(!result.is(constructor));
  DCHECK(!result.is(scratch));
  DCHECK(!result.is(value));

  // Allocate JSValue in new space.
  Allocate(JSValue::kSize, result, scratch, no_reg, gc_required,
           NO_ALLOCATION_FLAGS);

  // Initialize the JSValue.
  LoadGlobalFunctionInitialMap(constructor, scratch);
  movp(FieldOperand(result, HeapObject::kMapOffset), scratch);
  LoadRoot(scratch, Heap::kEmptyFixedArrayRootIndex);
  movp(FieldOperand(result, JSObject::kPropertiesOffset), scratch);
  movp(FieldOperand(result, JSObject::kElementsOffset), scratch);
  movp(FieldOperand(result, JSValue::kValueOffset), value);
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);
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
    j(not_zero, &short_loop, Label::kNear);
  }

  bind(&done);
}


void MacroAssembler::InitializeFieldsWithFiller(Register current_address,
                                                Register end_address,
                                                Register filler) {
  Label loop, entry;
  jmp(&entry, Label::kNear);
  bind(&loop);
  movp(Operand(current_address, 0), filler);
  addp(current_address, Immediate(kPointerSize));
  bind(&entry);
  cmpp(current_address, end_address);
  j(below, &loop, Label::kNear);
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
  DCHECK(IsFastElementsKind(expected_kind));
  DCHECK(IsFastElementsKind(transitioned_kind));

  // Check that the function's map is the same as the expected cached map.
  movp(scratch, NativeContextOperand());
  cmpp(map_in_out,
       ContextOperand(scratch, Context::ArrayMapIndex(expected_kind)));
  j(not_equal, no_map_match);

  // Use the transitioned cached map.
  movp(map_in_out,
       ContextOperand(scratch, Context::ArrayMapIndex(transitioned_kind)));
}


#ifdef _WIN64
static const int kRegisterPassedArguments = 4;
#else
static const int kRegisterPassedArguments = 6;
#endif


void MacroAssembler::LoadNativeContextSlot(int index, Register dst) {
  movp(dst, NativeContextOperand());
  movp(dst, ContextOperand(dst, index));
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


CodePatcher::CodePatcher(Isolate* isolate, byte* address, int size)
    : address_(address),
      size_(size),
      masm_(isolate, address, size + Assembler::kGap, CodeObjectRequired::kNo) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  Assembler::FlushICache(masm_.isolate(), address_, size_);

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


void MacroAssembler::JumpIfBlack(Register object,
                                 Register bitmap_scratch,
                                 Register mask_scratch,
                                 Label* on_black,
                                 Label::Distance on_black_distance) {
  DCHECK(!AreAliased(object, bitmap_scratch, mask_scratch, rcx));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  // The mask_scratch register contains a 1 at the position of the first bit
  // and a 1 at a position of the second bit. All other positions are zero.
  movp(rcx, mask_scratch);
  andp(rcx, Operand(bitmap_scratch, MemoryChunk::kHeaderSize));
  cmpp(mask_scratch, rcx);
  j(equal, on_black, on_black_distance);
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
  movl(mask_reg, Immediate(3));
  shlp_cl(mask_reg);
}


void MacroAssembler::JumpIfWhite(Register value, Register bitmap_scratch,
                                 Register mask_scratch, Label* value_is_white,
                                 Label::Distance distance) {
  DCHECK(!AreAliased(value, bitmap_scratch, mask_scratch, rcx));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  testp(Operand(bitmap_scratch, MemoryChunk::kHeaderSize), mask_scratch);
  j(zero, value_is_white, distance);
}


void MacroAssembler::CheckEnumCache(Label* call_runtime) {
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
  CompareRoot(rcx, Heap::kNullValueRootIndex);
  j(not_equal, &next);
}


void MacroAssembler::TestJSArrayForAllocationMemento(
    Register receiver_reg,
    Register scratch_reg,
    Label* no_memento_found) {
  Label map_check;
  Label top_check;
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  const int kMementoMapOffset = JSArray::kSize - kHeapObjectTag;
  const int kMementoEndOffset = kMementoMapOffset + AllocationMemento::kSize;

  // Bail out if the object is not in new space.
  JumpIfNotInNewSpace(receiver_reg, scratch_reg, no_memento_found);
  // If the object is in new space, we need to check whether it is on the same
  // page as the current top.
  leap(scratch_reg, Operand(receiver_reg, kMementoEndOffset));
  xorp(scratch_reg, ExternalOperand(new_space_allocation_top));
  testp(scratch_reg, Immediate(~Page::kPageAlignmentMask));
  j(zero, &top_check);
  // The object is on a different page than allocation top. Bail out if the
  // object sits on the page boundary as no memento can follow and we cannot
  // touch the memory following it.
  leap(scratch_reg, Operand(receiver_reg, kMementoEndOffset));
  xorp(scratch_reg, receiver_reg);
  testp(scratch_reg, Immediate(~Page::kPageAlignmentMask));
  j(not_zero, no_memento_found);
  // Continue with the actual map check.
  jmp(&map_check);
  // If top is on the same page as the current object, we need to check whether
  // we are below top.
  bind(&top_check);
  leap(scratch_reg, Operand(receiver_reg, kMementoEndOffset));
  cmpp(scratch_reg, ExternalOperand(new_space_allocation_top));
  j(greater, no_memento_found);
  // Memento map check.
  bind(&map_check);
  CompareRoot(MemOperand(receiver_reg, kMementoMapOffset),
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
  Label loop_again, end;

  movp(current, object);
  movp(current, FieldOperand(current, HeapObject::kMapOffset));
  movp(current, FieldOperand(current, Map::kPrototypeOffset));
  CompareRoot(current, Heap::kNullValueRootIndex);
  j(equal, &end);

  // Loop based on the map going up the prototype chain.
  bind(&loop_again);
  movp(current, FieldOperand(current, HeapObject::kMapOffset));
  STATIC_ASSERT(JS_PROXY_TYPE < JS_OBJECT_TYPE);
  STATIC_ASSERT(JS_VALUE_TYPE < JS_OBJECT_TYPE);
  CmpInstanceType(current, JS_OBJECT_TYPE);
  j(below, found);
  movp(scratch1, FieldOperand(current, Map::kBitField2Offset));
  DecodeField<Map::ElementsKindBits>(scratch1);
  cmpp(scratch1, Immediate(DICTIONARY_ELEMENTS));
  j(equal, found);
  movp(current, FieldOperand(current, Map::kPrototypeOffset));
  CompareRoot(current, Heap::kNullValueRootIndex);
  j(not_equal, &loop_again);

  bind(&end);
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


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

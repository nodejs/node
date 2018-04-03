// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/callable.h"
#include "src/code-stubs.h"
#include "src/counters.h"
#include "src/debug/debug.h"
#include "src/external-reference-table.h"
#include "src/frames-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects-inl.h"
#include "src/register-configuration.h"
#include "src/x64/assembler-x64.h"

#include "src/x64/macro-assembler-x64.h"  // Cannot be the first include.

namespace v8 {
namespace internal {

Operand StackArgumentsAccessor::GetArgumentOperand(int index) {
  DCHECK_GE(index, 0);
  int receiver = (receiver_mode_ == ARGUMENTS_CONTAIN_RECEIVER) ? 1 : 0;
  int displacement_to_last_argument =
      base_reg_ == rsp ? kPCOnStackSize : kFPOnStackSize + kPCOnStackSize;
  displacement_to_last_argument += extra_displacement_to_last_argument_;
  if (argument_count_reg_ == no_reg) {
    // argument[0] is at base_reg_ + displacement_to_last_argument +
    // (argument_count_immediate_ + receiver - 1) * kPointerSize.
    DCHECK_GT(argument_count_immediate_ + receiver, 0);
    return Operand(
        base_reg_,
        displacement_to_last_argument +
            (argument_count_immediate_ + receiver - 1 - index) * kPointerSize);
  } else {
    // argument[0] is at base_reg_ + displacement_to_last_argument +
    // argument_count_reg_ * times_pointer_size + (receiver - 1) * kPointerSize.
    return Operand(
        base_reg_, argument_count_reg_, times_pointer_size,
        displacement_to_last_argument + (receiver - 1 - index) * kPointerSize);
  }
}

StackArgumentsAccessor::StackArgumentsAccessor(
    Register base_reg, const ParameterCount& parameter_count,
    StackArgumentsAccessorReceiverMode receiver_mode,
    int extra_displacement_to_last_argument)
    : base_reg_(base_reg),
      argument_count_reg_(parameter_count.is_reg() ? parameter_count.reg()
                                                   : no_reg),
      argument_count_immediate_(
          parameter_count.is_immediate() ? parameter_count.immediate() : 0),
      receiver_mode_(receiver_mode),
      extra_displacement_to_last_argument_(
          extra_displacement_to_last_argument) {}

MacroAssembler::MacroAssembler(Isolate* isolate, void* buffer, int size,
                               CodeObjectRequired create_code_object)
    : TurboAssembler(isolate, buffer, size, create_code_object) {}

TurboAssembler::TurboAssembler(Isolate* isolate, void* buffer, int buffer_size,
                               CodeObjectRequired create_code_object)
    : Assembler(isolate, buffer, buffer_size), isolate_(isolate) {
  if (create_code_object == CodeObjectRequired::kYes) {
    code_object_ =
        Handle<HeapObject>::New(isolate->heap()->undefined_value(), isolate);
  }
}

static const int64_t kInvalidRootRegisterDelta = -1;

int64_t TurboAssembler::RootRegisterDelta(ExternalReference other) {
  if (predictable_code_size() &&
      (other.address() < reinterpret_cast<Address>(isolate()) ||
       other.address() >= reinterpret_cast<Address>(isolate() + 1))) {
    return kInvalidRootRegisterDelta;
  }
  Address roots_register_value =
      kRootRegisterBias +
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
  if (destination == rax) {
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
  if (source == rax) {
    store_rax(destination);
  } else {
    Move(kScratchRegister, destination);
    movp(Operand(kScratchRegister, 0), source);
  }
}

void TurboAssembler::LoadAddress(Register destination,
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

int TurboAssembler::LoadAddressSize(ExternalReference source) {
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
      Move(kScratchRegister, reinterpret_cast<Address>(kZapValue),
           Assembler::RelocInfoNone());
    }
    Push(Immediate(static_cast<int32_t>(address)));
    return;
  }
  LoadAddress(kScratchRegister, source);
  Push(kScratchRegister);
}

void TurboAssembler::LoadRoot(Register destination, Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  movp(destination, Operand(kRootRegister,
                            (index << kPointerSizeLog2) - kRootRegisterBias));
}

void MacroAssembler::PushRoot(Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  Push(Operand(kRootRegister, (index << kPointerSizeLog2) - kRootRegisterBias));
}

void TurboAssembler::CompareRoot(Register with, Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  cmpp(with, Operand(kRootRegister,
                     (index << kPointerSizeLog2) - kRootRegisterBias));
}

void TurboAssembler::CompareRoot(const Operand& with,
                                 Heap::RootListIndex index) {
  DCHECK(root_array_available_);
  DCHECK(!with.AddressUsesRegister(kScratchRegister));
  LoadRoot(kScratchRegister, index);
  cmpp(with, kScratchRegister);
}

void MacroAssembler::RecordWriteField(Register object, int offset,
                                      Register value, Register dst,
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

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  leap(dst, FieldOperand(object, offset));
  if (emit_debug_code()) {
    Label ok;
    testb(dst, Immediate(kPointerSize - 1));
    j(zero, &ok, Label::kNear);
    int3();
    bind(&ok);
  }

  RecordWrite(object, dst, value, save_fp, remembered_set_action,
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Move(value, reinterpret_cast<Address>(kZapValue),
         Assembler::RelocInfoNone());
    Move(dst, reinterpret_cast<Address>(kZapValue), Assembler::RelocInfoNone());
  }
}

void TurboAssembler::SaveRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  for (int i = 0; i < Register::kNumRegisters; ++i) {
    if ((registers >> i) & 1u) {
      pushq(Register::from_code(i));
    }
  }
}

void TurboAssembler::RestoreRegisters(RegList registers) {
  DCHECK_GT(NumRegs(registers), 0);
  for (int i = Register::kNumRegisters - 1; i >= 0; --i) {
    if ((registers >> i) & 1u) {
      popq(Register::from_code(i));
    }
  }
}

void TurboAssembler::CallRecordWriteStub(
    Register object, Register address,
    RememberedSetAction remembered_set_action, SaveFPRegsMode fp_mode) {
  Callable const callable =
      Builtins::CallableFor(isolate(), Builtins::kRecordWrite);
  RegList registers = callable.descriptor().allocatable_registers();

  SaveRegisters(registers);

  Register object_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kObject));
  Register slot_parameter(
      callable.descriptor().GetRegisterParameter(RecordWriteDescriptor::kSlot));
  Register isolate_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kIsolate));
  Register remembered_set_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kRememberedSet));
  Register fp_mode_parameter(callable.descriptor().GetRegisterParameter(
      RecordWriteDescriptor::kFPMode));

  // Prepare argument registers for calling RecordWrite
  // slot_parameter   <= address
  // object_parameter <= object
  if (slot_parameter != object) {
    // Normal case
    Move(slot_parameter, address);
    Move(object_parameter, object);
  } else if (object_parameter != address) {
    // Only slot_parameter and object are the same register
    // object_parameter <= object
    // slot_parameter   <= address
    Move(object_parameter, object);
    Move(slot_parameter, address);
  } else {
    // slot_parameter   \/ address
    // object_parameter /\ object
    xchgq(slot_parameter, object_parameter);
  }

  LoadAddress(isolate_parameter, ExternalReference::isolate_address(isolate()));

  Smi* smi_rsa = Smi::FromEnum(remembered_set_action);
  Smi* smi_fm = Smi::FromEnum(fp_mode);
  Move(remembered_set_parameter, smi_rsa);
  if (smi_rsa != smi_fm) {
    Move(fp_mode_parameter, smi_fm);
  } else {
    movq(fp_mode_parameter, remembered_set_parameter);
  }
  Call(callable.code(), RelocInfo::CODE_TARGET);

  RestoreRegisters(registers);
}

void MacroAssembler::RecordWrite(Register object, Register address,
                                 Register value, SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  DCHECK(object != value);
  DCHECK(object != address);
  DCHECK(value != address);
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

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, zero, &done,
                Label::kNear);

  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                zero,
                &done,
                Label::kNear);

  CallRecordWriteStub(object, address, remembered_set_action, fp_mode);

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    Move(address, reinterpret_cast<Address>(kZapValue),
         Assembler::RelocInfoNone());
    Move(value, reinterpret_cast<Address>(kZapValue),
         Assembler::RelocInfoNone());
  }
}

void TurboAssembler::Assert(Condition cc, AbortReason reason) {
  if (emit_debug_code()) Check(cc, reason);
}

void TurboAssembler::AssertUnreachable(AbortReason reason) {
  if (emit_debug_code()) Abort(reason);
}

void TurboAssembler::Check(Condition cc, AbortReason reason) {
  Label L;
  j(cc, &L, Label::kNear);
  Abort(reason);
  // Control will not return here.
  bind(&L);
}

void TurboAssembler::CheckStackAlignment() {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  int frame_alignment_mask = frame_alignment - 1;
  if (frame_alignment > kPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
    Label alignment_as_expected;
    testp(rsp, Immediate(frame_alignment_mask));
    j(zero, &alignment_as_expected, Label::kNear);
    // Abort if stack is not aligned.
    int3();
    bind(&alignment_as_expected);
  }
}

void TurboAssembler::Abort(AbortReason reason) {
#ifdef DEBUG
  const char* msg = GetAbortReason(reason);
  if (msg != nullptr) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    int3();
    return;
  }
#endif

  Move(rdx, Smi::FromInt(static_cast<int>(reason)));

  if (!has_frame()) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  } else {
    Call(BUILTIN_CODE(isolate(), Abort), RelocInfo::CODE_TARGET);
  }
  // Control will not return here.
  int3();
}

void TurboAssembler::CallStubDelayed(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs
  call(stub);
}

void MacroAssembler::CallStub(CodeStub* stub) {
  DCHECK(AllowThisStubCall(stub));  // Calls are not allowed in some stubs
  Call(stub->GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}

bool TurboAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame() || !stub->SometimesSetsUpAFrame();
}

void TurboAssembler::CallRuntimeDelayed(Zone* zone, Runtime::FunctionId fid,
                                        SaveFPRegsMode save_doubles) {
  const Runtime::Function* f = Runtime::FunctionForId(fid);
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  Set(rax, f->nargs);
  LoadAddress(rbx, ExternalReference(f, isolate()));
  CallStubDelayed(new (zone) CEntryStub(nullptr, f->result_size, save_doubles));
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

static constexpr Register saved_regs[] = {rax, rcx, rdx, rbx, rbp, rsi,
                                          rdi, r8,  r9,  r10, r11};

static constexpr int kNumberOfSavedRegs = sizeof(saved_regs) / sizeof(Register);

int TurboAssembler::RequiredStackSizeForCallerSaved(SaveFPRegsMode fp_mode,
                                                    Register exclusion1,
                                                    Register exclusion2,
                                                    Register exclusion3) const {
  int bytes = 0;
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      bytes += kPointerSize;
    }
  }

  // R12 to r15 are callee save on all platforms.
  if (fp_mode == kSaveFPRegs) {
    bytes += kDoubleSize * XMMRegister::kNumRegisters;
  }

  return bytes;
}

int TurboAssembler::PushCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                    Register exclusion2, Register exclusion3) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  int bytes = 0;
  for (int i = 0; i < kNumberOfSavedRegs; i++) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      pushq(reg);
      bytes += kPointerSize;
    }
  }

  // R12 to r15 are callee save on all platforms.
  if (fp_mode == kSaveFPRegs) {
    int delta = kDoubleSize * XMMRegister::kNumRegisters;
    subp(rsp, Immediate(delta));
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      Movsd(Operand(rsp, i * kDoubleSize), reg);
    }
    bytes += delta;
  }

  return bytes;
}

int TurboAssembler::PopCallerSaved(SaveFPRegsMode fp_mode, Register exclusion1,
                                   Register exclusion2, Register exclusion3) {
  int bytes = 0;
  if (fp_mode == kSaveFPRegs) {
    for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
      XMMRegister reg = XMMRegister::from_code(i);
      Movsd(reg, Operand(rsp, i * kDoubleSize));
    }
    int delta = kDoubleSize * XMMRegister::kNumRegisters;
    addp(rsp, Immediate(kDoubleSize * XMMRegister::kNumRegisters));
    bytes += delta;
  }

  for (int i = kNumberOfSavedRegs - 1; i >= 0; i--) {
    Register reg = saved_regs[i];
    if (reg != exclusion1 && reg != exclusion2 && reg != exclusion3) {
      popq(reg);
      bytes += kPointerSize;
    }
  }

  return bytes;
}

void TurboAssembler::Cvtss2sd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, src, src);
  } else {
    cvtss2sd(dst, src);
  }
}

void TurboAssembler::Cvtss2sd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtss2sd(dst, dst, src);
  } else {
    cvtss2sd(dst, src);
  }
}

void TurboAssembler::Cvtsd2ss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, src, src);
  } else {
    cvtsd2ss(dst, src);
  }
}

void TurboAssembler::Cvtsd2ss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvtsd2ss(dst, dst, src);
  } else {
    cvtsd2ss(dst, src);
  }
}

void TurboAssembler::Cvtlsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtlsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtlsi2sd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtlsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtlsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtlsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtlsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtlsi2ss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtlsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtlsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtqsi2ss(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtqsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtqsi2ss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, dst);
    vcvtqsi2ss(dst, dst, src);
  } else {
    xorps(dst, dst);
    cvtqsi2ss(dst, src);
  }
}

void TurboAssembler::Cvtqsi2sd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtqsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtqsi2sd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorpd(dst, dst, dst);
    vcvtqsi2sd(dst, dst, src);
  } else {
    xorpd(dst, dst);
    cvtqsi2sd(dst, src);
  }
}

void TurboAssembler::Cvtqui2ss(XMMRegister dst, Register src, Register tmp) {
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

void TurboAssembler::Cvtqui2sd(XMMRegister dst, Register src, Register tmp) {
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

void TurboAssembler::Cvttss2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}

void TurboAssembler::Cvttss2si(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2si(dst, src);
  } else {
    cvttss2si(dst, src);
  }
}

void TurboAssembler::Cvttsd2si(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}

void TurboAssembler::Cvttsd2si(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2si(dst, src);
  } else {
    cvttsd2si(dst, src);
  }
}

void TurboAssembler::Cvttss2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}

void TurboAssembler::Cvttss2siq(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttss2siq(dst, src);
  } else {
    cvttss2siq(dst, src);
  }
}

void TurboAssembler::Cvttsd2siq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vcvttsd2siq(dst, src);
  } else {
    cvttsd2siq(dst, src);
  }
}

void TurboAssembler::Cvttsd2siq(Register dst, const Operand& src) {
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

void TurboAssembler::Set(Register dst, int64_t x) {
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

void TurboAssembler::Set(const Operand& dst, intptr_t x) {
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

Register TurboAssembler::GetSmiConstant(Smi* source) {
  STATIC_ASSERT(kSmiTag == 0);
  int value = source->value();
  if (value == 0) {
    xorl(kScratchRegister, kScratchRegister);
    return kScratchRegister;
  }
  Move(kScratchRegister, source);
  return kScratchRegister;
}

void TurboAssembler::Move(Register dst, Smi* source) {
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
  if (dst != src) {
    movl(dst, src);
  }
  shlp(dst, Immediate(kSmiShift));
}

void TurboAssembler::SmiToInteger32(Register dst, Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  if (dst != src) {
    movp(dst, src);
  }

  if (SmiValuesAre32Bits()) {
    shrp(dst, Immediate(kSmiShift));
  } else {
    DCHECK(SmiValuesAre31Bits());
    sarl(dst, Immediate(kSmiShift));
  }
}

void TurboAssembler::SmiToInteger32(Register dst, const Operand& src) {
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
  if (dst != src) {
    movp(dst, src);
  }
  sarp(dst, Immediate(kSmiShift));
  if (kPointerSize == kInt32Size) {
    // Sign extend to 64-bit.
    movsxlq(dst, dst);
  }
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
  DCHECK_NE(dst, kScratchRegister);
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


Condition TurboAssembler::CheckSmi(Register src) {
  STATIC_ASSERT(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}

Condition TurboAssembler::CheckSmi(const Operand& src) {
  STATIC_ASSERT(kSmiTag == 0);
  testb(src, Immediate(kSmiTagMask));
  return zero;
}

void TurboAssembler::JumpIfSmi(Register src, Label* on_smi,
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

void MacroAssembler::JumpIfNotSmi(Operand src, Label* on_not_smi,
                                  Label::Distance near_jump) {
  Condition smi = CheckSmi(src);
  j(NegateCondition(smi), on_not_smi, near_jump);
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

SmiIndex MacroAssembler::SmiToIndex(Register dst,
                                    Register src,
                                    int shift) {
  if (SmiValuesAre32Bits()) {
    DCHECK(is_uint6(shift));
    // There is a possible optimization if shift is in the range 60-63, but that
    // will (and must) never happen.
    if (dst != src) {
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
    if (dst != src) {
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

void TurboAssembler::Push(Smi* source) {
  intptr_t smi = reinterpret_cast<intptr_t>(source);
  if (is_int32(smi)) {
    Push(Immediate(static_cast<int32_t>(smi)));
    return;
  }
  int first_byte_set = base::bits::CountTrailingZeros64(smi) / 8;
  int last_byte_set = (63 - base::bits::CountLeadingZeros64(smi)) / 8;
  if (first_byte_set == last_byte_set && kPointerSize == kInt64Size) {
    // This sequence has only 7 bytes, compared to the 12 bytes below.
    Push(Immediate(0));
    movb(Operand(rsp, first_byte_set),
         Immediate(static_cast<int8_t>(smi >> (8 * first_byte_set))));
    return;
  }
  Register constant = GetSmiConstant(source);
  Push(constant);
}

// ----------------------------------------------------------------------------

void TurboAssembler::Move(Register dst, Register src) {
  if (dst != src) {
    movp(dst, src);
  }
}

void TurboAssembler::MoveNumber(Register dst, double value) {
  int32_t smi;
  if (DoubleToSmiInteger(value, &smi)) {
    Move(dst, Smi::FromInt(smi));
  } else {
    movp_heap_number(dst, value);
  }
}

void TurboAssembler::Move(XMMRegister dst, uint32_t src) {
  if (src == 0) {
    Xorpd(dst, dst);
  } else {
    unsigned pop = base::bits::CountPopulation(src);
    DCHECK_NE(0u, pop);
    if (pop == 32) {
      Pcmpeqd(dst, dst);
    } else {
      movl(kScratchRegister, Immediate(src));
      Movq(dst, kScratchRegister);
    }
  }
}

void TurboAssembler::Move(XMMRegister dst, uint64_t src) {
  if (src == 0) {
    Xorpd(dst, dst);
  } else {
    unsigned nlz = base::bits::CountLeadingZeros(src);
    unsigned ntz = base::bits::CountTrailingZeros(src);
    unsigned pop = base::bits::CountPopulation(src);
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

void TurboAssembler::Movaps(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovaps(dst, src);
  } else {
    movaps(dst, src);
  }
}

void TurboAssembler::Movups(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovups(dst, src);
  } else {
    movups(dst, src);
  }
}

void TurboAssembler::Movups(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovups(dst, src);
  } else {
    movups(dst, src);
  }
}

void TurboAssembler::Movups(const Operand& dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovups(dst, src);
  } else {
    movups(dst, src);
  }
}

void TurboAssembler::Movapd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovapd(dst, src);
  } else {
    movapd(dst, src);
  }
}

void TurboAssembler::Movsd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovsd(dst, dst, src);
  } else {
    movsd(dst, src);
  }
}

void TurboAssembler::Movsd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovsd(dst, src);
  } else {
    movsd(dst, src);
  }
}

void TurboAssembler::Movsd(const Operand& dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovsd(dst, src);
  } else {
    movsd(dst, src);
  }
}

void TurboAssembler::Movss(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovss(dst, dst, src);
  } else {
    movss(dst, src);
  }
}

void TurboAssembler::Movss(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovss(dst, src);
  } else {
    movss(dst, src);
  }
}

void TurboAssembler::Movss(const Operand& dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovss(dst, src);
  } else {
    movss(dst, src);
  }
}

void TurboAssembler::Movd(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovd(dst, src);
  } else {
    movd(dst, src);
  }
}

void TurboAssembler::Movd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovd(dst, src);
  } else {
    movd(dst, src);
  }
}

void TurboAssembler::Movd(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovd(dst, src);
  } else {
    movd(dst, src);
  }
}

void TurboAssembler::Movq(XMMRegister dst, Register src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}

void TurboAssembler::Movq(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovq(dst, src);
  } else {
    movq(dst, src);
  }
}

void TurboAssembler::Movmskps(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovmskps(dst, src);
  } else {
    movmskps(dst, src);
  }
}

void TurboAssembler::Movmskpd(Register dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vmovmskpd(dst, src);
  } else {
    movmskpd(dst, src);
  }
}

void TurboAssembler::Xorps(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, src);
  } else {
    xorps(dst, src);
  }
}

void TurboAssembler::Xorps(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vxorps(dst, dst, src);
  } else {
    xorps(dst, src);
  }
}

void TurboAssembler::Roundss(XMMRegister dst, XMMRegister src,
                             RoundingMode mode) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vroundss(dst, dst, src, mode);
  } else {
    roundss(dst, src, mode);
  }
}

void TurboAssembler::Roundsd(XMMRegister dst, XMMRegister src,
                             RoundingMode mode) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vroundsd(dst, dst, src, mode);
  } else {
    roundsd(dst, src, mode);
  }
}

void TurboAssembler::Sqrtsd(XMMRegister dst, XMMRegister src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsqrtsd(dst, dst, src);
  } else {
    sqrtsd(dst, src);
  }
}

void TurboAssembler::Sqrtsd(XMMRegister dst, const Operand& src) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vsqrtsd(dst, dst, src);
  } else {
    sqrtsd(dst, src);
  }
}

void TurboAssembler::Ucomiss(XMMRegister src1, XMMRegister src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vucomiss(src1, src2);
  } else {
    ucomiss(src1, src2);
  }
}

void TurboAssembler::Ucomiss(XMMRegister src1, const Operand& src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vucomiss(src1, src2);
  } else {
    ucomiss(src1, src2);
  }
}

void TurboAssembler::Ucomisd(XMMRegister src1, XMMRegister src2) {
  if (CpuFeatures::IsSupported(AVX)) {
    CpuFeatureScope scope(this, AVX);
    vucomisd(src1, src2);
  } else {
    ucomisd(src1, src2);
  }
}

void TurboAssembler::Ucomisd(XMMRegister src1, const Operand& src2) {
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
    Move(kScratchRegister, Handle<HeapObject>::cast(source));
    cmpp(dst, kScratchRegister);
  }
}


void MacroAssembler::Cmp(const Operand& dst, Handle<Object> source) {
  AllowDeferredHandleDereference smi_check;
  if (source->IsSmi()) {
    Cmp(dst, Smi::cast(*source));
  } else {
    Move(kScratchRegister, Handle<HeapObject>::cast(source));
    cmpp(dst, kScratchRegister);
  }
}

void TurboAssembler::Push(Handle<HeapObject> source) {
  Move(kScratchRegister, source);
  Push(kScratchRegister);
}

void TurboAssembler::Move(Register result, Handle<HeapObject> object,
                          RelocInfo::Mode rmode) {
  movp(result, reinterpret_cast<void*>(object.address()), rmode);
}

void TurboAssembler::Move(const Operand& dst, Handle<HeapObject> object,
                          RelocInfo::Mode rmode) {
  Move(kScratchRegister, object, rmode);
  movp(dst, kScratchRegister);
}

void MacroAssembler::Drop(int stack_elements) {
  if (stack_elements > 0) {
    addp(rsp, Immediate(stack_elements * kPointerSize));
  }
}


void MacroAssembler::DropUnderReturnAddress(int stack_elements,
                                            Register scratch) {
  DCHECK_GT(stack_elements, 0);
  if (kPointerSize == kInt64Size && stack_elements == 1) {
    popq(MemOperand(rsp, 0));
    return;
  }

  PopReturnAddressTo(scratch);
  Drop(stack_elements);
  PushReturnAddressFrom(scratch);
}

void TurboAssembler::Push(Register src) {
  if (kPointerSize == kInt64Size) {
    pushq(src);
  } else {
    // x32 uses 64-bit push for rbp in the prologue.
    DCHECK(src.code() != rbp.code());
    leal(rsp, Operand(rsp, -4));
    movp(Operand(rsp, 0), src);
  }
}

void TurboAssembler::Push(const Operand& src) {
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

void TurboAssembler::Push(Immediate value) {
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
    if (scratch == kRootRegister) {
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

int TurboAssembler::CallSize(ExternalReference ext) {
  // Opcode for call kScratchRegister is: Rex.B FF D4 (three bytes).
  return LoadAddressSize(ext) +
         Assembler::kCallScratchRegisterInstructionLength;
}

void TurboAssembler::Call(ExternalReference ext) {
#ifdef DEBUG
  int end_position = pc_offset() + CallSize(ext);
#endif
  LoadAddress(kScratchRegister, ext);
  call(kScratchRegister);
  DCHECK_EQ(end_position, pc_offset());
}

void TurboAssembler::Call(const Operand& op) {
  if (kPointerSize == kInt64Size && !CpuFeatures::IsSupported(ATOM)) {
    call(op);
  } else {
    movp(kScratchRegister, op);
    call(kScratchRegister);
  }
}

void TurboAssembler::Call(Address destination, RelocInfo::Mode rmode) {
#ifdef DEBUG
  int end_position = pc_offset() + CallSize(destination);
#endif
  Move(kScratchRegister, destination, rmode);
  call(kScratchRegister);
  DCHECK_EQ(pc_offset(), end_position);
}

void TurboAssembler::Call(Handle<Code> code_object, RelocInfo::Mode rmode) {
#ifdef DEBUG
  int end_position = pc_offset() + CallSize(code_object);
#endif
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  call(code_object, rmode);
  DCHECK_EQ(end_position, pc_offset());
}

void TurboAssembler::RetpolineCall(Register reg) {
  Label setup_return, setup_target, inner_indirect_branch, capture_spec;

  jmp(&setup_return);  // Jump past the entire retpoline below.

  bind(&inner_indirect_branch);
  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  movq(Operand(rsp, 0), reg);
  ret(0);

  bind(&setup_return);
  call(&inner_indirect_branch);  // Callee will return after this instruction.
}

void TurboAssembler::RetpolineCall(Address destination, RelocInfo::Mode rmode) {
#ifdef DEBUG
// TODO(titzer): CallSize() is wrong for RetpolineCalls
//  int end_position = pc_offset() + CallSize(destination);
#endif
  Move(kScratchRegister, destination, rmode);
  RetpolineCall(kScratchRegister);
  // TODO(titzer): CallSize() is wrong for RetpolineCalls
  //  DCHECK_EQ(pc_offset(), end_position);
}

void TurboAssembler::RetpolineJump(Register reg) {
  Label setup_target, capture_spec;

  call(&setup_target);

  bind(&capture_spec);
  pause();
  jmp(&capture_spec);

  bind(&setup_target);
  movq(Operand(rsp, 0), reg);
  ret(0);
}

void TurboAssembler::Pextrd(Register dst, XMMRegister src, int8_t imm8) {
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

void TurboAssembler::Pinsrd(XMMRegister dst, Register src, int8_t imm8) {
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

void TurboAssembler::Pinsrd(XMMRegister dst, const Operand& src, int8_t imm8) {
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

void TurboAssembler::Lzcntl(Register dst, Register src) {
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

void TurboAssembler::Lzcntl(Register dst, const Operand& src) {
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

void TurboAssembler::Lzcntq(Register dst, Register src) {
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

void TurboAssembler::Lzcntq(Register dst, const Operand& src) {
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

void TurboAssembler::Tzcntq(Register dst, Register src) {
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

void TurboAssembler::Tzcntq(Register dst, const Operand& src) {
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

void TurboAssembler::Tzcntl(Register dst, Register src) {
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

void TurboAssembler::Tzcntl(Register dst, const Operand& src) {
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

void TurboAssembler::Popcntl(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Popcntl(Register dst, const Operand& src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntl(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Popcntq(Register dst, Register src) {
  if (CpuFeatures::IsSupported(POPCNT)) {
    CpuFeatureScope scope(this, POPCNT);
    popcntq(dst, src);
    return;
  }
  UNREACHABLE();
}

void TurboAssembler::Popcntq(Register dst, const Operand& src) {
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

void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  Push(Immediate(0));  // Padding.

  // Link the current handler as the next handler.
  ExternalReference handler_address(IsolateAddressId::kHandlerAddress,
                                    isolate());
  Push(ExternalOperand(handler_address));

  // Set this new handler as the current one.
  movp(ExternalOperand(handler_address), rsp);
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  ExternalReference handler_address(IsolateAddressId::kHandlerAddress,
                                    isolate());
  Pop(ExternalOperand(handler_address));
  addp(rsp, Immediate(StackHandlerConstants::kSize - kPointerSize));
}

void TurboAssembler::Ret() { ret(0); }

void TurboAssembler::Ret(int bytes_dropped, Register scratch) {
  if (is_uint16(bytes_dropped)) {
    ret(bytes_dropped);
  } else {
    PopReturnAddressTo(scratch);
    addp(rsp, Immediate(bytes_dropped));
    PushReturnAddressFrom(scratch);
    ret(0);
  }
}

void MacroAssembler::CmpObjectType(Register heap_object,
                                   InstanceType type,
                                   Register map) {
  movp(map, FieldOperand(heap_object, HeapObject::kMapOffset));
  CmpInstanceType(map, type);
}


void MacroAssembler::CmpInstanceType(Register map, InstanceType type) {
  cmpw(FieldOperand(map, Map::kInstanceTypeOffset), Immediate(type));
}

void TurboAssembler::SlowTruncateToIDelayed(Zone* zone, Register result_reg) {
  CallStubDelayed(new (zone) DoubleToIStub(nullptr, result_reg));
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


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    Condition is_smi = CheckSmi(object);
    Check(NegateCondition(is_smi), AbortReason::kOperandIsASmi);
  }
}


void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    Condition is_smi = CheckSmi(object);
    Check(is_smi, AbortReason::kOperandIsNotASmi);
  }
}


void MacroAssembler::AssertSmi(const Operand& object) {
  if (emit_debug_code()) {
    Condition is_smi = CheckSmi(object);
    Check(is_smi, AbortReason::kOperandIsNotASmi);
  }
}

void MacroAssembler::AssertFixedArray(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAFixedArray);
    Push(object);
    CmpObjectType(object, FIXED_ARRAY_TYPE, object);
    Pop(object);
    Check(equal, AbortReason::kOperandIsNotAFixedArray);
  }
}

void TurboAssembler::AssertZeroExtended(Register int32_register) {
  if (emit_debug_code()) {
    DCHECK_NE(int32_register, kScratchRegister);
    movq(kScratchRegister, int64_t{0x0000000100000000});
    cmpq(kScratchRegister, int32_register);
    Check(above_equal, AbortReason::k32BitValueInRegisterIsNotZeroExtended);
  }
}


void MacroAssembler::AssertFunction(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotAFunction);
    Push(object);
    CmpObjectType(object, JS_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, AbortReason::kOperandIsNotAFunction);
  }
}


void MacroAssembler::AssertBoundFunction(Register object) {
  if (emit_debug_code()) {
    testb(object, Immediate(kSmiTagMask));
    Check(not_equal, AbortReason::kOperandIsASmiAndNotABoundFunction);
    Push(object);
    CmpObjectType(object, JS_BOUND_FUNCTION_TYPE, object);
    Pop(object);
    Check(equal, AbortReason::kOperandIsNotABoundFunction);
  }
}

void MacroAssembler::AssertGeneratorObject(Register object) {
  if (!emit_debug_code()) return;
  testb(object, Immediate(kSmiTagMask));
  Check(not_equal, AbortReason::kOperandIsASmiAndNotAGeneratorObject);

  // Load map
  Register map = object;
  Push(object);
  movp(map, FieldOperand(object, HeapObject::kMapOffset));

  Label do_check;
  // Check if JSGeneratorObject
  CmpInstanceType(map, JS_GENERATOR_OBJECT_TYPE);
  j(equal, &do_check);

  // Check if JSAsyncGeneratorObject
  CmpInstanceType(map, JS_ASYNC_GENERATOR_OBJECT_TYPE);

  bind(&do_check);
  // Restore generator object to register and perform assertion
  Pop(object);
  Check(equal, AbortReason::kOperandIsNotAGeneratorObject);
}

void MacroAssembler::AssertUndefinedOrAllocationSite(Register object) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    Cmp(object, isolate()->factory()->undefined_value());
    j(equal, &done_checking);
    Cmp(FieldOperand(object, 0), isolate()->factory()->allocation_site_map());
    Assert(equal, AbortReason::kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}

void MacroAssembler::IncrementCounter(StatsCounter* counter, int value) {
  DCHECK_GT(value, 0);
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
  DCHECK_GT(value, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Operand counter_operand = ExternalOperand(ExternalReference(counter));
    if (value == 1) {
      decl(counter_operand);
    } else {
      subl(counter_operand, Immediate(value));
    }
  }
}

void MacroAssembler::MaybeDropFrames() {
  // Check whether we need to drop frames to restart a function on the stack.
  ExternalReference restart_fp =
      ExternalReference::debug_restart_fp_address(isolate());
  Load(rbx, restart_fp);
  testp(rbx, rbx);
  j(not_zero, BUILTIN_CODE(isolate(), FrameDropperTrampoline),
    RelocInfo::CODE_TARGET);
}

void TurboAssembler::PrepareForTailCall(const ParameterCount& callee_args_count,
                                        Register caller_args_count_reg,
                                        Register scratch0, Register scratch1) {
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
    Check(below, AbortReason::kStackAccessBelowStackPointer);
  }

  // Copy return address from caller's frame to current frame's return address
  // to avoid its trashing and let the following loop copy it to the right
  // place.
  Register tmp_reg = scratch1;
  movp(tmp_reg, Operand(rbp, StandardFrameConstants::kCallerPCOffset));
  movp(Operand(rsp, 0), tmp_reg);

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

void MacroAssembler::InvokeFunction(Register function, Register new_target,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  movp(rbx, FieldOperand(function, JSFunction::kSharedFunctionInfoOffset));
  movsxlq(rbx,
          FieldOperand(rbx, SharedFunctionInfo::kFormalParameterCountOffset));

  ParameterCount expected(rbx);
  InvokeFunction(function, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunction(Register function, Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag) {
  DCHECK(function == rdi);
  movp(rsi, FieldOperand(function, JSFunction::kContextOffset));
  InvokeFunctionCode(rdi, new_target, expected, actual, flag);
}

void MacroAssembler::InvokeFunctionCode(Register function, Register new_target,
                                        const ParameterCount& expected,
                                        const ParameterCount& actual,
                                        InvokeFlag flag) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());
  DCHECK(function == rdi);
  DCHECK_IMPLIES(new_target.is_valid(), new_target == rdx);

  // On function call, call into the debugger if necessary.
  CheckDebugHook(function, new_target, expected, actual);

  // Clear the new.target register if not given.
  if (!new_target.is_valid()) {
    LoadRoot(rdx, Heap::kUndefinedValueRootIndex);
  }

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, &done, &definitely_mismatches, flag,
                 Label::kNear);
  if (!definitely_mismatches) {
    // We call indirectly through the code field in the function to
    // allow recompilation to take effect without changing any of the
    // call sites.
    movp(rcx, FieldOperand(function, JSFunction::kCodeOffset));
    addp(rcx, Immediate(Code::kHeaderSize - kHeapObjectTag));
    if (flag == CALL_FUNCTION) {
      call(rcx);
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      jmp(rcx);
    }
    bind(&done);
  }
}

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual, Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    Label::Distance near_jump) {
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
      DCHECK(expected.reg() == rbx);
    } else if (expected.reg() != actual.reg()) {
      // Both expected and actual are in (different) registers. This
      // is the case when we invoke functions using call and apply.
      cmpp(expected.reg(), actual.reg());
      j(equal, &invoke, Label::kNear);
      DCHECK(actual.reg() == rax);
      DCHECK(expected.reg() == rbx);
    } else {
      definitely_matches = true;
      Move(rax, actual.reg());
    }
  }

  if (!definitely_matches) {
    Handle<Code> adaptor = BUILTIN_CODE(isolate(), ArgumentsAdaptorTrampoline);
    if (flag == CALL_FUNCTION) {
      Call(adaptor, RelocInfo::CODE_TARGET);
      if (!*definitely_mismatches) {
        jmp(done, near_jump);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&invoke);
  }
}

void MacroAssembler::CheckDebugHook(Register fun, Register new_target,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual) {
  Label skip_hook;
  ExternalReference debug_hook_active =
      ExternalReference::debug_hook_on_function_call_address(isolate());
  Operand debug_hook_active_operand = ExternalOperand(debug_hook_active);
  cmpb(debug_hook_active_operand, Immediate(0));
  j(equal, &skip_hook);
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
    CallRuntime(Runtime::kDebugOnFunctionCall);
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
  bind(&skip_hook);
}

void TurboAssembler::StubPrologue(StackFrame::Type type) {
  pushq(rbp);  // Caller's frame pointer.
  movp(rbp, rsp);
  Push(Immediate(StackFrame::TypeToMarker(type)));
}

void TurboAssembler::Prologue() {
  pushq(rbp);  // Caller's frame pointer.
  movp(rbp, rsp);
  Push(rsi);  // Callee's context.
  Push(rdi);  // Callee's JS function.
}

void TurboAssembler::EnterFrame(StackFrame::Type type) {
  pushq(rbp);
  movp(rbp, rsp);
  Push(Immediate(StackFrame::TypeToMarker(type)));
  if (type == StackFrame::INTERNAL) {
    Move(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
    Push(kScratchRegister);
  }
  if (emit_debug_code()) {
    Move(kScratchRegister,
         isolate()->factory()->undefined_value(),
         RelocInfo::EMBEDDED_OBJECT);
    cmpp(Operand(rsp, 0), kScratchRegister);
    Check(not_equal, AbortReason::kCodeObjectNotProperlyPatched);
  }
}

void TurboAssembler::LeaveFrame(StackFrame::Type type) {
  if (emit_debug_code()) {
    cmpp(Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset),
         Immediate(StackFrame::TypeToMarker(type)));
    Check(equal, AbortReason::kStackFrameTypesMustMatch);
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
  Push(Immediate(StackFrame::TypeToMarker(frame_type)));
  DCHECK_EQ(-2 * kPointerSize, ExitFrameConstants::kSPOffset);
  Push(Immediate(0));  // Saved entry sp, patched before call.
  Move(kScratchRegister, CodeObject(), RelocInfo::EMBEDDED_OBJECT);
  Push(kScratchRegister);  // Accessed from ExitFrame::code_slot.

  // Save the frame pointer and the context in top.
  if (save_rax) {
    movp(r14, rax);  // Backup rax in callee-save register.
  }

  Store(ExternalReference(IsolateAddressId::kCEntryFPAddress, isolate()), rbp);
  Store(ExternalReference(IsolateAddressId::kContextAddress, isolate()), rsi);
  Store(ExternalReference(IsolateAddressId::kCFunctionAddress, isolate()), rbx);
}


void MacroAssembler::EnterExitFrameEpilogue(int arg_stack_space,
                                            bool save_doubles) {
#ifdef _WIN64
  const int kShadowSpace = 4;
  arg_stack_space += kShadowSpace;
#endif
  // Optionally save all XMM registers.
  if (save_doubles) {
    int space = XMMRegister::kNumRegisters * kDoubleSize +
                arg_stack_space * kRegisterSize;
    subp(rsp, Immediate(space));
    int offset = -ExitFrameConstants::kFixedFrameSizeFromFp;
    const RegisterConfiguration* config = RegisterConfiguration::Default();
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
    DCHECK(base::bits::IsPowerOfTwo(kFrameAlignment));
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
    const RegisterConfiguration* config = RegisterConfiguration::Default();
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

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveApiExitFrame() {
  movp(rsp, rbp);
  popq(rbp);

  LeaveExitFrameEpilogue();
}

void MacroAssembler::LeaveExitFrameEpilogue() {
  // Restore current context from top and clear it in debug mode.
  ExternalReference context_address(IsolateAddressId::kContextAddress,
                                    isolate());
  Operand context_operand = ExternalOperand(context_address);
  movp(rsi, context_operand);
#ifdef DEBUG
  movp(context_operand, Immediate(Context::kInvalidContext));
#endif

  // Clear the top frame.
  ExternalReference c_entry_fp_address(IsolateAddressId::kCEntryFPAddress,
                                       isolate());
  Operand c_entry_fp_operand = ExternalOperand(c_entry_fp_address);
  movp(c_entry_fp_operand, Immediate(0));
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


int TurboAssembler::ArgumentStackSlotsForCFunctionCall(int num_arguments) {
  // On Windows 64 stack slots are reserved by the caller for all arguments
  // including the ones passed in registers, and space is always allocated for
  // the four register arguments even if the function takes fewer than four
  // arguments.
  // On AMD64 ABI (Linux/Mac) the first six arguments are passed in registers
  // and the caller does not reserve stack slots for them.
  DCHECK_GE(num_arguments, 0);
#ifdef _WIN64
  const int kMinimumStackSlots = kRegisterPassedArguments;
  if (num_arguments < kMinimumStackSlots) return kMinimumStackSlots;
  return num_arguments;
#else
  if (num_arguments < kRegisterPassedArguments) return 0;
  return num_arguments - kRegisterPassedArguments;
#endif
}

void TurboAssembler::PrepareCallCFunction(int num_arguments) {
  int frame_alignment = base::OS::ActivationFrameAlignment();
  DCHECK_NE(frame_alignment, 0);
  DCHECK_GE(num_arguments, 0);

  // Make stack end at alignment and allocate space for arguments and old rsp.
  movp(kScratchRegister, rsp);
  DCHECK(base::bits::IsPowerOfTwo(frame_alignment));
  int argument_slots_on_stack =
      ArgumentStackSlotsForCFunctionCall(num_arguments);
  subp(rsp, Immediate((argument_slots_on_stack + 1) * kRegisterSize));
  andp(rsp, Immediate(-frame_alignment));
  movp(Operand(rsp, argument_slots_on_stack * kRegisterSize), kScratchRegister);
}

void TurboAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  LoadAddress(rax, function);
  CallCFunction(rax, num_arguments);
}

void TurboAssembler::CallCFunction(Register function, int num_arguments) {
  DCHECK_LE(num_arguments, kMaxCParameters);
  DCHECK(has_frame());
  // Check stack alignment.
  if (emit_debug_code()) {
    CheckStackAlignment();
  }

  call(function);
  DCHECK_NE(base::OS::ActivationFrameAlignment(), 0);
  DCHECK_GE(num_arguments, 0);
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

void TurboAssembler::CheckPageFlag(Register object, Register scratch, int mask,
                                   Condition cc, Label* condition_met,
                                   Label::Distance condition_met_distance) {
  DCHECK(cc == zero || cc == not_zero);
  if (scratch == object) {
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
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64

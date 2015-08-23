// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>  // For assert
#include <limits.h>  // For LONG_MIN, LONG_MAX.

#include "src/v8.h"

#if V8_TARGET_ARCH_PPC

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      has_frame_(false) {
  if (isolate() != NULL) {
    code_object_ =
        Handle<Object>(isolate()->heap()->undefined_value(), isolate());
  }
}


void MacroAssembler::Jump(Register target) {
  mtctr(target);
  bctr();
}


void MacroAssembler::JumpToJSEntry(Register target) {
  Move(ip, target);
  Jump(ip);
}


void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, CRegister cr) {
  Label skip;

  if (cond != al) b(NegateCondition(cond), &skip, cr);

  DCHECK(rmode == RelocInfo::CODE_TARGET || rmode == RelocInfo::RUNTIME_ENTRY);

  mov(ip, Operand(target, rmode));
  mtctr(ip);
  bctr();

  bind(&skip);
}


void MacroAssembler::Jump(Address target, RelocInfo::Mode rmode, Condition cond,
                          CRegister cr) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond, cr);
}


void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  // 'code' is always generated ppc code, never THUMB code
  AllowDeferredHandleDereference embedding_raw_address;
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode, cond);
}


int MacroAssembler::CallSize(Register target) { return 2 * kInstrSize; }


void MacroAssembler::Call(Register target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label start;
  bind(&start);

  // Statement positions are expected to be recorded when the target
  // address is loaded.
  positions_recorder()->WriteRecordedPositions();

  // branch via link register and set LK bit for return point
  mtctr(target);
  bctrl();

  DCHECK_EQ(CallSize(target), SizeOfCodeGeneratedSince(&start));
}


void MacroAssembler::CallJSEntry(Register target) {
  DCHECK(target.is(ip));
  Call(target);
}


int MacroAssembler::CallSize(Address target, RelocInfo::Mode rmode,
                             Condition cond) {
  Operand mov_operand = Operand(reinterpret_cast<intptr_t>(target), rmode);
  return (2 + instructions_required_for_mov(ip, mov_operand)) * kInstrSize;
}


int MacroAssembler::CallSizeNotPredictableCodeSize(Address target,
                                                   RelocInfo::Mode rmode,
                                                   Condition cond) {
  return (2 + kMovInstructionsNoConstantPool) * kInstrSize;
}


void MacroAssembler::Call(Address target, RelocInfo::Mode rmode,
                          Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(cond == al);

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(target, rmode, cond);
  Label start;
  bind(&start);
#endif

  // Statement positions are expected to be recorded when the target
  // address is loaded.
  positions_recorder()->WriteRecordedPositions();

  // This can likely be optimized to make use of bc() with 24bit relative
  //
  // RecordRelocInfo(x.rmode_, x.imm_);
  // bc( BA, .... offset, LKset);
  //

  mov(ip, Operand(reinterpret_cast<intptr_t>(target), rmode));
  mtctr(ip);
  bctrl();

  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(Handle<Code> code, RelocInfo::Mode rmode,
                             TypeFeedbackId ast_id, Condition cond) {
  AllowDeferredHandleDereference using_raw_address;
  return CallSize(reinterpret_cast<Address>(code.location()), rmode, cond);
}


void MacroAssembler::Call(Handle<Code> code, RelocInfo::Mode rmode,
                          TypeFeedbackId ast_id, Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(RelocInfo::IsCodeTarget(rmode));

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(code, rmode, ast_id, cond);
  Label start;
  bind(&start);
#endif

  if (rmode == RelocInfo::CODE_TARGET && !ast_id.IsNone()) {
    SetRecordedAstId(ast_id);
    rmode = RelocInfo::CODE_TARGET_WITH_ID;
  }
  AllowDeferredHandleDereference using_raw_address;
  Call(reinterpret_cast<Address>(code.location()), rmode, cond);
  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}


void MacroAssembler::Ret(Condition cond) {
  DCHECK(cond == al);
  blr();
}


void MacroAssembler::Drop(int count, Condition cond) {
  DCHECK(cond == al);
  if (count > 0) {
    Add(sp, sp, count * kPointerSize, r0);
  }
}


void MacroAssembler::Ret(int drop, Condition cond) {
  Drop(drop, cond);
  Ret(cond);
}


void MacroAssembler::Call(Label* target) { b(target, SetLK); }


void MacroAssembler::Push(Handle<Object> handle) {
  mov(r0, Operand(handle));
  push(r0);
}


void MacroAssembler::Move(Register dst, Handle<Object> value) {
  AllowDeferredHandleDereference smi_check;
  if (value->IsSmi()) {
    LoadSmiLiteral(dst, reinterpret_cast<Smi*>(*value));
  } else {
    DCHECK(value->IsHeapObject());
    if (isolate()->heap()->InNewSpace(*value)) {
      Handle<Cell> cell = isolate()->factory()->NewCell(value);
      mov(dst, Operand(cell));
      LoadP(dst, FieldMemOperand(dst, Cell::kValueOffset));
    } else {
      mov(dst, Operand(value));
    }
  }
}


void MacroAssembler::Move(Register dst, Register src, Condition cond) {
  DCHECK(cond == al);
  if (!dst.is(src)) {
    mr(dst, src);
  }
}


void MacroAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (!dst.is(src)) {
    fmr(dst, src);
  }
}


void MacroAssembler::MultiPush(RegList regs) {
  int16_t num_to_push = NumberOfBitsSet(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  subi(sp, sp, Operand(stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      StoreP(ToRegister(i), MemOperand(sp, stack_offset));
    }
  }
}


void MacroAssembler::MultiPop(RegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      LoadP(ToRegister(i), MemOperand(sp, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  addi(sp, sp, Operand(stack_offset));
}


void MacroAssembler::LoadRoot(Register destination, Heap::RootListIndex index,
                              Condition cond) {
  DCHECK(cond == al);
  LoadP(destination, MemOperand(kRootRegister, index << kPointerSizeLog2), r0);
}


void MacroAssembler::StoreRoot(Register source, Heap::RootListIndex index,
                               Condition cond) {
  DCHECK(Heap::RootCanBeWrittenAfterInitialization(index));
  DCHECK(cond == al);
  StoreP(source, MemOperand(kRootRegister, index << kPointerSizeLog2), r0);
}


void MacroAssembler::InNewSpace(Register object, Register scratch,
                                Condition cond, Label* branch) {
  // N.B. scratch may be same register as object
  DCHECK(cond == eq || cond == ne);
  mov(r0, Operand(ExternalReference::new_space_mask(isolate())));
  and_(scratch, object, r0);
  mov(r0, Operand(ExternalReference::new_space_start(isolate())));
  cmp(scratch, r0);
  b(cond, branch);
}


void MacroAssembler::RecordWriteField(
    Register object, int offset, Register value, Register dst,
    LinkRegisterStatus lr_status, SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action, SmiCheck smi_check,
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

  Add(dst, object, offset - kHeapObjectTag, r0);
  if (emit_debug_code()) {
    Label ok;
    andi(r0, dst, Operand((1 << kPointerSizeLog2) - 1));
    beq(&ok, cr0);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  RecordWrite(object, dst, value, lr_status, save_fp, remembered_set_action,
              OMIT_SMI_CHECK, pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 4)));
    mov(dst, Operand(bit_cast<intptr_t>(kZapValue + 8)));
  }
}


// Will clobber 4 registers: object, map, dst, ip.  The
// register 'object' contains a heap object pointer.
void MacroAssembler::RecordWriteForMap(Register object, Register map,
                                       Register dst,
                                       LinkRegisterStatus lr_status,
                                       SaveFPRegsMode fp_mode) {
  if (emit_debug_code()) {
    LoadP(dst, FieldMemOperand(map, HeapObject::kMapOffset));
    Cmpi(dst, Operand(isolate()->factory()->meta_map()), r0);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (!FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    LoadP(ip, FieldMemOperand(object, HeapObject::kMapOffset));
    cmp(ip, map);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  Label done;

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  CheckPageFlag(map,
                map,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);

  addi(dst, object, Operand(HeapObject::kMapOffset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    andi(r0, dst, Operand((1 << kPointerSizeLog2) - 1));
    beq(&ok, cr0);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    mflr(r0);
    push(r0);
  }
  RecordWriteStub stub(isolate(), object, map, dst, OMIT_REMEMBERED_SET,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r0);
    mtlr(r0);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip, dst);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(dst, Operand(bit_cast<intptr_t>(kZapValue + 12)));
    mov(map, Operand(bit_cast<intptr_t>(kZapValue + 16)));
  }
}


// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(
    Register object, Register address, Register value,
    LinkRegisterStatus lr_status, SaveFPRegsMode fp_mode,
    RememberedSetAction remembered_set_action, SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  DCHECK(!object.is(value));
  if (emit_debug_code()) {
    LoadP(r0, MemOperand(address));
    cmp(r0, value);
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }

  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  if (pointers_to_here_check_for_value != kPointersToHereAreAlwaysInteresting) {
    CheckPageFlag(value,
                  value,  // Used as scratch.
                  MemoryChunk::kPointersToHereAreInterestingMask, eq, &done);
  }
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask, eq, &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    mflr(r0);
    push(r0);
  }
  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r0);
    mtlr(r0);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip,
                   value);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Operand(bit_cast<intptr_t>(kZapValue + 12)));
    mov(value, Operand(bit_cast<intptr_t>(kZapValue + 16)));
  }
}


void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register address, Register scratch,
                                         SaveFPRegsMode fp_mode,
                                         RememberedSetFinalAction and_then) {
  Label done;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, scratch, &ok);
    stop("Remembered set pointer is in new space");
    bind(&ok);
  }
  // Load store buffer top.
  ExternalReference store_buffer =
      ExternalReference::store_buffer_top(isolate());
  mov(ip, Operand(store_buffer));
  LoadP(scratch, MemOperand(ip));
  // Store pointer to buffer and increment buffer top.
  StoreP(address, MemOperand(scratch));
  addi(scratch, scratch, Operand(kPointerSize));
  // Write back new top of buffer.
  StoreP(scratch, MemOperand(ip));
  // Call stub on end of buffer.
  // Check for end of buffer.
  mov(r0, Operand(StoreBuffer::kStoreBufferOverflowBit));
  and_(r0, scratch, r0, SetRC);

  if (and_then == kFallThroughAtEnd) {
    beq(&done, cr0);
  } else {
    DCHECK(and_then == kReturnAtEnd);
    beq(&done, cr0);
  }
  mflr(r0);
  push(r0);
  StoreBufferOverflowStub store_buffer_overflow(isolate(), fp_mode);
  CallStub(&store_buffer_overflow);
  pop(r0);
  mtlr(r0);
  bind(&done);
  if (and_then == kReturnAtEnd) {
    Ret();
  }
}


void MacroAssembler::PushFixedFrame(Register marker_reg) {
  mflr(r0);
  if (FLAG_enable_embedded_constant_pool) {
    if (marker_reg.is_valid()) {
      Push(r0, fp, kConstantPoolRegister, cp, marker_reg);
    } else {
      Push(r0, fp, kConstantPoolRegister, cp);
    }
  } else {
    if (marker_reg.is_valid()) {
      Push(r0, fp, cp, marker_reg);
    } else {
      Push(r0, fp, cp);
    }
  }
}


void MacroAssembler::PopFixedFrame(Register marker_reg) {
  if (FLAG_enable_embedded_constant_pool) {
    if (marker_reg.is_valid()) {
      Pop(r0, fp, kConstantPoolRegister, cp, marker_reg);
    } else {
      Pop(r0, fp, kConstantPoolRegister, cp);
    }
  } else {
    if (marker_reg.is_valid()) {
      Pop(r0, fp, cp, marker_reg);
    } else {
      Pop(r0, fp, cp);
    }
  }
  mtlr(r0);
}


const RegList MacroAssembler::kSafepointSavedRegisters = Register::kAllocatable;
const int MacroAssembler::kNumSafepointSavedRegisters =
    Register::kMaxNumAllocatableRegisters;

// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK(num_unsaved >= 0);
  if (num_unsaved > 0) {
    subi(sp, sp, Operand(num_unsaved * kPointerSize));
  }
  MultiPush(kSafepointSavedRegisters);
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  MultiPop(kSafepointSavedRegisters);
  if (num_unsaved > 0) {
    addi(sp, sp, Operand(num_unsaved * kPointerSize));
  }
}


void MacroAssembler::StoreToSafepointRegisterSlot(Register src, Register dst) {
  StoreP(src, SafepointRegisterSlot(dst));
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  LoadP(dst, SafepointRegisterSlot(src));
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the highest encoding,
  // which means that lowest encodings are closest to the stack pointer.
  RegList regs = kSafepointSavedRegisters;
  int index = 0;

  DCHECK(reg_code >= 0 && reg_code < kNumRegisters);

  for (int16_t i = 0; i < reg_code; i++) {
    if ((regs & (1 << i)) != 0) {
      index++;
    }
  }

  return index;
}


MemOperand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return MemOperand(sp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


MemOperand MacroAssembler::SafepointRegistersAndDoublesSlot(Register reg) {
  // General purpose registers are pushed last on the stack.
  int doubles_size = DoubleRegister::NumAllocatableRegisters() * kDoubleSize;
  int register_offset = SafepointRegisterStackIndex(reg.code()) * kPointerSize;
  return MemOperand(sp, doubles_size + register_offset);
}


void MacroAssembler::CanonicalizeNaN(const DoubleRegister dst,
                                     const DoubleRegister src) {
  // Turn potential sNaN into qNaN.
  fsub(dst, src, kDoubleRegZero);
}


void MacroAssembler::ConvertIntToDouble(Register src,
                                        DoubleRegister double_dst) {
  MovIntToDouble(double_dst, src, r0);
  fcfid(double_dst, double_dst);
}


void MacroAssembler::ConvertUnsignedIntToDouble(Register src,
                                                DoubleRegister double_dst) {
  MovUnsignedIntToDouble(double_dst, src, r0);
  fcfid(double_dst, double_dst);
}


void MacroAssembler::ConvertIntToFloat(const DoubleRegister dst,
                                       const Register src,
                                       const Register int_scratch) {
  MovIntToDouble(dst, src, int_scratch);
  fcfid(dst, dst);
  frsp(dst, dst);
}


void MacroAssembler::ConvertDoubleToInt64(const DoubleRegister double_input,
#if !V8_TARGET_ARCH_PPC64
                                          const Register dst_hi,
#endif
                                          const Register dst,
                                          const DoubleRegister double_dst,
                                          FPRoundingMode rounding_mode) {
  if (rounding_mode == kRoundToZero) {
    fctidz(double_dst, double_input);
  } else {
    SetRoundingMode(rounding_mode);
    fctid(double_dst, double_input);
    ResetRoundingMode();
  }

  MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
      dst_hi,
#endif
      dst, double_dst);
}


void MacroAssembler::LoadConstantPoolPointerRegisterFromCodeTargetAddress(
    Register code_target_address) {
  lwz(kConstantPoolRegister,
      MemOperand(code_target_address,
                 Code::kConstantPoolOffset - Code::kHeaderSize));
  add(kConstantPoolRegister, kConstantPoolRegister, code_target_address);
}


void MacroAssembler::LoadConstantPoolPointerRegister(Register base,
                                                     int code_start_delta) {
  add_label_offset(kConstantPoolRegister, base, ConstantPoolPosition(),
                   code_start_delta);
}


void MacroAssembler::LoadConstantPoolPointerRegister() {
  mov_label_addr(kConstantPoolRegister, ConstantPoolPosition());
}


void MacroAssembler::StubPrologue(int prologue_offset) {
  LoadSmiLiteral(r11, Smi::FromInt(StackFrame::STUB));
  PushFixedFrame(r11);
  // Adjust FP to point to saved FP.
  addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
  if (FLAG_enable_embedded_constant_pool) {
    // ip contains prologue address
    LoadConstantPoolPointerRegister(ip, -prologue_offset);
    set_constant_pool_available(true);
  }
}


void MacroAssembler::Prologue(bool code_pre_aging, int prologue_offset) {
  {
    PredictableCodeSizeScope predictible_code_size_scope(
        this, kNoCodeAgeSequenceLength);
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(this);
    // The following instructions must remain together and unmodified
    // for code aging to work properly.
    if (code_pre_aging) {
      // Pre-age the code.
      // This matches the code found in PatchPlatformCodeAge()
      Code* stub = Code::GetPreAgedCodeAgeStub(isolate());
      intptr_t target = reinterpret_cast<intptr_t>(stub->instruction_start());
      // Don't use Call -- we need to preserve ip and lr
      nop();  // marker to detect sequence (see IsOld)
      mov(r3, Operand(target));
      Jump(r3);
      for (int i = 0; i < kCodeAgingSequenceNops; i++) {
        nop();
      }
    } else {
      // This matches the code found in GetNoCodeAgeSequence()
      PushFixedFrame(r4);
      // Adjust fp to point to saved fp.
      addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
      for (int i = 0; i < kNoCodeAgeSequenceNops; i++) {
        nop();
      }
    }
  }
  if (FLAG_enable_embedded_constant_pool) {
    // ip contains prologue address
    LoadConstantPoolPointerRegister(ip, -prologue_offset);
    set_constant_pool_available(true);
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool_pointer_reg) {
  if (FLAG_enable_embedded_constant_pool && load_constant_pool_pointer_reg) {
    PushFixedFrame();
    // This path should not rely on ip containing code entry.
    LoadConstantPoolPointerRegister();
    LoadSmiLiteral(ip, Smi::FromInt(type));
    push(ip);
  } else {
    LoadSmiLiteral(ip, Smi::FromInt(type));
    PushFixedFrame(ip);
  }
  // Adjust FP to point to saved FP.
  addi(fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));

  mov(r0, Operand(CodeObject()));
  push(r0);
}


int MacroAssembler::LeaveFrame(StackFrame::Type type, int stack_adjustment) {
  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  // r3: preserved
  // r4: preserved
  // r5: preserved

  // Drop the execution stack down to the frame pointer and restore
  // the caller's state.
  int frame_ends;
  LoadP(r0, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  LoadP(ip, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  if (FLAG_enable_embedded_constant_pool) {
    const int exitOffset = ExitFrameConstants::kConstantPoolOffset;
    const int standardOffset = StandardFrameConstants::kConstantPoolOffset;
    const int offset =
        ((type == StackFrame::EXIT) ? exitOffset : standardOffset);
    LoadP(kConstantPoolRegister, MemOperand(fp, offset));
  }
  mtlr(r0);
  frame_ends = pc_offset();
  Add(sp, fp, StandardFrameConstants::kCallerSPOffset + stack_adjustment, r0);
  mr(fp, ip);
  return frame_ends;
}


// ExitFrame layout (probably wrongish.. needs updating)
//
//  SP -> previousSP
//        LK reserved
//        code
//        sp_on_exit (for debug?)
// oldSP->prev SP
//        LK
//        <parameters on stack>

// Prior to calling EnterExitFrame, we've got a bunch of parameters
// on the stack that we need to wrap a real frame around.. so first
// we reserve a slot for LK and push the previous SP which is captured
// in the fp register (r31)
// Then - we buy a new frame

void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space) {
  // Set up the frame structure on the stack.
  DCHECK_EQ(2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  DCHECK(stack_space > 0);

  // This is an opportunity to build a frame to wrap
  // all of the pushes that have happened inside of V8
  // since we were called from C code

  // replicate ARM frame - TODO make this more closely follow PPC ABI
  mflr(r0);
  Push(r0, fp);
  mr(fp, sp);
  // Reserve room for saved entry sp and code object.
  subi(sp, sp, Operand(ExitFrameConstants::kFrameSize));

  if (emit_debug_code()) {
    li(r8, Operand::Zero());
    StoreP(r8, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }
  if (FLAG_enable_embedded_constant_pool) {
    StoreP(kConstantPoolRegister,
           MemOperand(fp, ExitFrameConstants::kConstantPoolOffset));
  }
  mov(r8, Operand(CodeObject()));
  StoreP(r8, MemOperand(fp, ExitFrameConstants::kCodeOffset));

  // Save the frame pointer and the context in top.
  mov(r8, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  StoreP(fp, MemOperand(r8));
  mov(r8, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  StoreP(cp, MemOperand(r8));

  // Optionally save all volatile double registers.
  if (save_doubles) {
    SaveFPRegs(sp, 0, DoubleRegister::kNumVolatileRegisters);
    // Note that d0 will be accessible at
    //   fp - ExitFrameConstants::kFrameSize -
    //   kNumVolatileRegisters * kDoubleSize,
    // since the sp slot and code slot were pushed after the fp.
  }

  addi(sp, sp, Operand(-stack_space * kPointerSize));

  // Allocate and align the frame preparing for calling the runtime
  // function.
  const int frame_alignment = ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
  }
  li(r0, Operand::Zero());
  StorePU(r0, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));

  // Set the exit frame sp value to point just before the return address
  // location.
  addi(r8, sp, Operand((kStackFrameExtraParamSlot + 1) * kPointerSize));
  StoreP(r8, MemOperand(fp, ExitFrameConstants::kSPOffset));
}


void MacroAssembler::InitializeNewString(Register string, Register length,
                                         Heap::RootListIndex map_index,
                                         Register scratch1, Register scratch2) {
  SmiTag(scratch1, length);
  LoadRoot(scratch2, map_index);
  StoreP(scratch1, FieldMemOperand(string, String::kLengthOffset), r0);
  li(scratch1, Operand(String::kEmptyHashField));
  StoreP(scratch2, FieldMemOperand(string, HeapObject::kMapOffset), r0);
  StoreP(scratch1, FieldMemOperand(string, String::kHashFieldSlot), r0);
}


int MacroAssembler::ActivationFrameAlignment() {
#if !defined(USE_SIMULATOR)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one PPC
  // platform for another PPC platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else  // Simulated
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif
}


void MacroAssembler::LeaveExitFrame(bool save_doubles, Register argument_count,
                                    bool restore_context,
                                    bool argument_count_is_length) {
  ConstantPoolUnavailableScope constant_pool_unavailable(this);
  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int kNumRegs = DoubleRegister::kNumVolatileRegisters;
    const int offset =
        (ExitFrameConstants::kFrameSize + kNumRegs * kDoubleSize);
    addi(r6, fp, Operand(-offset));
    RestoreFPRegs(r6, 0, kNumRegs);
  }

  // Clear top frame.
  li(r6, Operand::Zero());
  mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  StoreP(r6, MemOperand(ip));

  // Restore current context from top and clear it in debug mode.
  if (restore_context) {
    mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
    LoadP(cp, MemOperand(ip));
  }
#ifdef DEBUG
  mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  StoreP(r6, MemOperand(ip));
#endif

  // Tear down the exit frame, pop the arguments, and return.
  LeaveFrame(StackFrame::EXIT);

  if (argument_count.is_valid()) {
    if (!argument_count_is_length) {
      ShiftLeftImm(argument_count, argument_count, Operand(kPointerSizeLog2));
    }
    add(sp, sp, argument_count);
  }
}


void MacroAssembler::MovFromFloatResult(const DoubleRegister dst) {
  Move(dst, d1);
}


void MacroAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  Move(dst, d1);
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_reg, Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  r3: actual arguments count
  //  r4: function (passed through to callee)
  //  r5: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.

  // ARM has some sanity checks as per below, considering add them for PPC
  //  DCHECK(actual.is_immediate() || actual.reg().is(r3));
  //  DCHECK(expected.is_immediate() || expected.reg().is(r5));
  //  DCHECK((!code_constant.is_null() && code_reg.is(no_reg))
  //          || code_reg.is(r6));

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      mov(r3, Operand(actual.immediate()));
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        mov(r5, Operand(expected.immediate()));
      }
    }
  } else {
    if (actual.is_immediate()) {
      cmpi(expected.reg(), Operand(actual.immediate()));
      beq(&regular_invoke);
      mov(r3, Operand(actual.immediate()));
    } else {
      cmp(expected.reg(), actual.reg());
      beq(&regular_invoke);
    }
  }

  if (!definitely_matches) {
    if (!code_constant.is_null()) {
      mov(r6, Operand(code_constant));
      addi(r6, r6, Operand(Code::kHeaderSize - kHeapObjectTag));
    }

    Handle<Code> adaptor = isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      Call(adaptor);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        b(done);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&regular_invoke);
  }
}


void MacroAssembler::InvokeCode(Register code, const ParameterCount& expected,
                                const ParameterCount& actual, InvokeFlag flag,
                                const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, Handle<Code>::null(), code, &done,
                 &definitely_mismatches, flag, call_wrapper);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      CallJSEntry(code);
      call_wrapper.AfterCall();
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      JumpToJSEntry(code);
    }

    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register fun, const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r4.
  DCHECK(fun.is(r4));

  Register expected_reg = r5;
  Register code_reg = ip;

  LoadP(code_reg, FieldMemOperand(r4, JSFunction::kSharedFunctionInfoOffset));
  LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));
  LoadWordArith(expected_reg,
                FieldMemOperand(
                    code_reg, SharedFunctionInfo::kFormalParameterCountOffset));
#if !defined(V8_TARGET_ARCH_PPC64)
  SmiUntag(expected_reg);
#endif
  LoadP(code_reg, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r4.
  DCHECK(function.is(r4));

  // Get the function and setup the context.
  LoadP(cp, FieldMemOperand(r4, JSFunction::kContextOffset));

  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  LoadP(ip, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));
  InvokeCode(ip, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  Move(r4, function);
  InvokeFunction(r4, expected, actual, flag, call_wrapper);
}


void MacroAssembler::IsObjectJSObjectType(Register heap_object, Register map,
                                          Register scratch, Label* fail) {
  LoadP(map, FieldMemOperand(heap_object, HeapObject::kMapOffset));
  IsInstanceJSObjectType(map, scratch, fail);
}


void MacroAssembler::IsInstanceJSObjectType(Register map, Register scratch,
                                            Label* fail) {
  lbz(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  cmpi(scratch, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  blt(fail);
  cmpi(scratch, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
  bgt(fail);
}


void MacroAssembler::IsObjectJSStringType(Register object, Register scratch,
                                          Label* fail) {
  DCHECK(kNotStringTag != 0);

  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  lbz(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  andi(r0, scratch, Operand(kIsNotStringMask));
  bne(fail, cr0);
}


void MacroAssembler::IsObjectNameType(Register object, Register scratch,
                                      Label* fail) {
  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  lbz(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  cmpi(scratch, Operand(LAST_NAME_TYPE));
  bgt(fail);
}


void MacroAssembler::DebugBreak() {
  li(r3, Operand::Zero());
  mov(r4, Operand(ExternalReference(Runtime::kDebugBreak, isolate())));
  CEntryStub ces(isolate(), 1);
  DCHECK(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}


void MacroAssembler::PushStackHandler() {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);

  // Link the current handler as the next handler.
  // Preserve r3-r7.
  mov(r8, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  LoadP(r0, MemOperand(r8));
  push(r0);

  // Set this new handler as the current one.
  StoreP(sp, MemOperand(r8));
}


void MacroAssembler::PopStackHandler() {
  STATIC_ASSERT(StackHandlerConstants::kSize == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);

  pop(r4);
  mov(ip, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  StoreP(r4, MemOperand(ip));
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch, Label* miss) {
  Label same_contexts;

  DCHECK(!holder_reg.is(scratch));
  DCHECK(!holder_reg.is(ip));
  DCHECK(!scratch.is(ip));

  // Load current lexical context from the stack frame.
  LoadP(scratch, MemOperand(fp, StandardFrameConstants::kContextOffset));
// In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  cmpi(scratch, Operand::Zero());
  Check(ne, kWeShouldNotHaveAnEmptyLexicalContext);
#endif

  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  LoadP(scratch, FieldMemOperand(scratch, offset));
  LoadP(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);  // Temporarily save holder on the stack.
    // Read the first word and compare to the native_context_map.
    LoadP(holder_reg, FieldMemOperand(scratch, HeapObject::kMapOffset));
    LoadRoot(ip, Heap::kNativeContextMapRootIndex);
    cmp(holder_reg, ip);
    Check(eq, kJSGlobalObjectNativeContextShouldBeANativeContext);
    pop(holder_reg);  // Restore holder.
  }

  // Check if both contexts are the same.
  LoadP(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  cmp(scratch, ip);
  beq(&same_contexts);

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);    // Temporarily save holder on the stack.
    mr(holder_reg, ip);  // Move ip to its holding place.
    LoadRoot(ip, Heap::kNullValueRootIndex);
    cmp(holder_reg, ip);
    Check(ne, kJSGlobalProxyContextShouldNotBeNull);

    LoadP(holder_reg, FieldMemOperand(holder_reg, HeapObject::kMapOffset));
    LoadRoot(ip, Heap::kNativeContextMapRootIndex);
    cmp(holder_reg, ip);
    Check(eq, kJSGlobalObjectNativeContextShouldBeANativeContext);
    // Restore ip is not needed. ip is reloaded below.
    pop(holder_reg);  // Restore holder.
    // Restore ip to holder's context.
    LoadP(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  }

  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  int token_offset =
      Context::kHeaderSize + Context::SECURITY_TOKEN_INDEX * kPointerSize;

  LoadP(scratch, FieldMemOperand(scratch, token_offset));
  LoadP(ip, FieldMemOperand(ip, token_offset));
  cmp(scratch, ip);
  bne(miss);

  bind(&same_contexts);
}


// Compute the hash code from the untagged key.  This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericStub in
// code-stub-hydrogen.cc
void MacroAssembler::GetNumberHash(Register t0, Register scratch) {
  // First of all we assign the hash seed to scratch.
  LoadRoot(scratch, Heap::kHashSeedRootIndex);
  SmiUntag(scratch);

  // Xor original key with a seed.
  xor_(t0, t0, scratch);

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  notx(scratch, t0);
  slwi(t0, t0, Operand(15));
  add(t0, scratch, t0);
  // hash = hash ^ (hash >> 12);
  srwi(scratch, t0, Operand(12));
  xor_(t0, t0, scratch);
  // hash = hash + (hash << 2);
  slwi(scratch, t0, Operand(2));
  add(t0, t0, scratch);
  // hash = hash ^ (hash >> 4);
  srwi(scratch, t0, Operand(4));
  xor_(t0, t0, scratch);
  // hash = hash * 2057;
  mr(r0, t0);
  slwi(scratch, t0, Operand(3));
  add(t0, t0, scratch);
  slwi(scratch, r0, Operand(11));
  add(t0, t0, scratch);
  // hash = hash ^ (hash >> 16);
  srwi(scratch, t0, Operand(16));
  xor_(t0, t0, scratch);
  // hash & 0x3fffffff
  ExtractBitRange(t0, t0, 29, 0);
}


void MacroAssembler::LoadFromNumberDictionary(Label* miss, Register elements,
                                              Register key, Register result,
                                              Register t0, Register t1,
                                              Register t2) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the same as 'key' or 'result'.
  //            Unchanged on bailout so 'key' or 'result' can be used
  //            in further computation.
  //
  // Scratch registers:
  //
  // t0 - holds the untagged key on entry and holds the hash once computed.
  //
  // t1 - used to hold the capacity mask of the dictionary
  //
  // t2 - used for the index into the dictionary.
  Label done;

  GetNumberHash(t0, t1);

  // Compute the capacity mask.
  LoadP(t1, FieldMemOperand(elements, SeededNumberDictionary::kCapacityOffset));
  SmiUntag(t1);
  subi(t1, t1, Operand(1));

  // Generate an unrolled loop that performs a few probes before giving up.
  for (int i = 0; i < kNumberDictionaryProbes; i++) {
    // Use t2 for index calculations and keep the hash intact in t0.
    mr(t2, t0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      addi(t2, t2, Operand(SeededNumberDictionary::GetProbeOffset(i)));
    }
    and_(t2, t2, t1);

    // Scale the index by multiplying by the element size.
    DCHECK(SeededNumberDictionary::kEntrySize == 3);
    slwi(ip, t2, Operand(1));
    add(t2, t2, ip);  // t2 = t2 * 3

    // Check if the key is identical to the name.
    slwi(t2, t2, Operand(kPointerSizeLog2));
    add(t2, elements, t2);
    LoadP(ip,
          FieldMemOperand(t2, SeededNumberDictionary::kElementsStartOffset));
    cmp(key, ip);
    if (i != kNumberDictionaryProbes - 1) {
      beq(&done);
    } else {
      bne(miss);
    }
  }

  bind(&done);
  // Check that the value is a field property.
  // t2: elements + (index * kPointerSize)
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  LoadP(t1, FieldMemOperand(t2, kDetailsOffset));
  LoadSmiLiteral(ip, Smi::FromInt(PropertyDetails::TypeField::kMask));
  DCHECK_EQ(DATA, 0);
  and_(r0, t1, ip, SetRC);
  bne(miss, cr0);

  // Get the value at the masked, scaled index and return.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  LoadP(result, FieldMemOperand(t2, kValueOffset));
}


void MacroAssembler::Allocate(int object_size, Register result,
                              Register scratch1, Register scratch2,
                              Label* gc_required, AllocationFlags flags) {
  DCHECK(object_size <= Page::kMaxRegularHeapObjectSize);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      li(result, Operand(0x7091));
      li(scratch1, Operand(0x7191));
      li(scratch2, Operand(0x7291));
    }
    b(gc_required);
    return;
  }

  DCHECK(!result.is(scratch1));
  DCHECK(!result.is(scratch2));
  DCHECK(!scratch1.is(scratch2));
  DCHECK(!scratch1.is(ip));
  DCHECK(!scratch2.is(ip));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  DCHECK_EQ(0, static_cast<int>(object_size & kObjectAlignmentMask));

  // Check relative positions of allocation top and limit addresses.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  intptr_t top = reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

  // Set up allocation top address register.
  Register topaddr = scratch1;
  mov(topaddr, Operand(allocation_top));

  // This code stores a temporary value in ip. This is OK, as the code below
  // does not need ip for implicit literal generation.
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into ip.
    LoadP(result, MemOperand(topaddr));
    LoadP(ip, MemOperand(topaddr, kPointerSize));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry. ip is used
      // immediately below so this use of ip does not cause difference with
      // respect to register content between debug and release mode.
      LoadP(ip, MemOperand(topaddr));
      cmp(result, ip);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load allocation limit into ip. Result already contains allocation top.
    LoadP(ip, MemOperand(topaddr, limit - top), r0);
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
#if V8_TARGET_ARCH_PPC64
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    andi(scratch2, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, cr0);
    if ((flags & PRETENURE) != 0) {
      cmpl(result, ip);
      bge(gc_required);
    }
    mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    stw(scratch2, MemOperand(result));
    addi(result, result, Operand(kDoubleSize / 2));
    bind(&aligned);
#endif
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top.
  sub(r0, ip, result);
  if (is_int16(object_size)) {
    cmpi(r0, Operand(object_size));
    blt(gc_required);
    addi(scratch2, result, Operand(object_size));
  } else {
    Cmpi(r0, Operand(object_size), scratch2);
    blt(gc_required);
    add(scratch2, result, scratch2);
  }
  StoreP(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    addi(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::Allocate(Register object_size, Register result,
                              Register scratch1, Register scratch2,
                              Label* gc_required, AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      li(result, Operand(0x7091));
      li(scratch1, Operand(0x7191));
      li(scratch2, Operand(0x7291));
    }
    b(gc_required);
    return;
  }

  // Assert that the register arguments are different and that none of
  // them are ip. ip is used explicitly in the code generated below.
  DCHECK(!result.is(scratch1));
  DCHECK(!result.is(scratch2));
  DCHECK(!scratch1.is(scratch2));
  DCHECK(!object_size.is(ip));
  DCHECK(!result.is(ip));
  DCHECK(!scratch1.is(ip));
  DCHECK(!scratch2.is(ip));

  // Check relative positions of allocation top and limit addresses.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top = reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit = reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

  // Set up allocation top address.
  Register topaddr = scratch1;
  mov(topaddr, Operand(allocation_top));

  // This code stores a temporary value in ip. This is OK, as the code below
  // does not need ip for implicit literal generation.
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into ip.
    LoadP(result, MemOperand(topaddr));
    LoadP(ip, MemOperand(topaddr, kPointerSize));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry. ip is used
      // immediately below so this use of ip does not cause difference with
      // respect to register content between debug and release mode.
      LoadP(ip, MemOperand(topaddr));
      cmp(result, ip);
      Check(eq, kUnexpectedAllocationTop);
    }
    // Load allocation limit into ip. Result already contains allocation top.
    LoadP(ip, MemOperand(topaddr, limit - top));
  }

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
#if V8_TARGET_ARCH_PPC64
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    andi(scratch2, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, cr0);
    if ((flags & PRETENURE) != 0) {
      cmpl(result, ip);
      bge(gc_required);
    }
    mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    stw(scratch2, MemOperand(result));
    addi(result, result, Operand(kDoubleSize / 2));
    bind(&aligned);
#endif
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size may be in words so a shift is
  // required to get the number of bytes.
  sub(r0, ip, result);
  if ((flags & SIZE_IN_WORDS) != 0) {
    ShiftLeftImm(scratch2, object_size, Operand(kPointerSizeLog2));
    cmp(r0, scratch2);
    blt(gc_required);
    add(scratch2, result, scratch2);
  } else {
    cmp(r0, object_size);
    blt(gc_required);
    add(scratch2, result, object_size);
  }

  // Update allocation top. result temporarily holds the new top.
  if (emit_debug_code()) {
    andi(r0, scratch2, Operand(kObjectAlignmentMask));
    Check(eq, kUnalignedAllocationInNewSpace, cr0);
  }
  StoreP(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    addi(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object,
                                              Register scratch) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  ClearRightImm(object, object, Operand(kHeapObjectTagSize));
#ifdef DEBUG
  // Check that the object un-allocated is below the current top.
  mov(scratch, Operand(new_space_allocation_top));
  LoadP(scratch, MemOperand(scratch));
  cmp(object, scratch);
  Check(lt, kUndoAllocationOfNonAllocatedMemory);
#endif
  // Write the address of the object to un-allocate as the current top.
  mov(scratch, Operand(new_space_allocation_top));
  StoreP(object, MemOperand(scratch));
}


void MacroAssembler::AllocateTwoByteString(Register result, Register length,
                                           Register scratch1, Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  DCHECK((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  slwi(scratch1, length, Operand(1));  // Length in bytes, not chars.
  addi(scratch1, scratch1,
       Operand(kObjectAlignmentMask + SeqTwoByteString::kHeaderSize));
  mov(r0, Operand(~kObjectAlignmentMask));
  and_(scratch1, scratch1, r0);

  // Allocate two-byte string in new space.
  Allocate(scratch1, result, scratch2, scratch3, gc_required, TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result, length, Heap::kStringMapRootIndex, scratch1,
                      scratch2);
}


void MacroAssembler::AllocateOneByteString(Register result, Register length,
                                           Register scratch1, Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  DCHECK((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  DCHECK(kCharSize == 1);
  addi(scratch1, length,
       Operand(kObjectAlignmentMask + SeqOneByteString::kHeaderSize));
  li(r0, Operand(~kObjectAlignmentMask));
  and_(scratch1, scratch1, r0);

  // Allocate one-byte string in new space.
  Allocate(scratch1, result, scratch2, scratch3, gc_required, TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result, length, Heap::kOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


void MacroAssembler::AllocateTwoByteConsString(Register result, Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kConsStringMapRootIndex, scratch1,
                      scratch2);
}


void MacroAssembler::AllocateOneByteConsString(Register result, Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kConsOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kSlicedStringMapRootIndex, scratch1,
                      scratch2);
}


void MacroAssembler::AllocateOneByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result, length, Heap::kSlicedOneByteStringMapRootIndex,
                      scratch1, scratch2);
}


void MacroAssembler::CompareObjectType(Register object, Register map,
                                       Register type_reg, InstanceType type) {
  const Register temp = type_reg.is(no_reg) ? r0 : type_reg;

  LoadP(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, temp, type);
}


void MacroAssembler::CheckObjectTypeRange(Register object, Register map,
                                          InstanceType min_type,
                                          InstanceType max_type,
                                          Label* false_label) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE < 256);
  LoadP(map, FieldMemOperand(object, HeapObject::kMapOffset));
  lbz(ip, FieldMemOperand(map, Map::kInstanceTypeOffset));
  subi(ip, ip, Operand(min_type));
  cmpli(ip, Operand(max_type - min_type));
  bgt(false_label);
}


void MacroAssembler::CompareInstanceType(Register map, Register type_reg,
                                         InstanceType type) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE < 256);
  lbz(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  cmpi(type_reg, Operand(type));
}


void MacroAssembler::CompareRoot(Register obj, Heap::RootListIndex index) {
  DCHECK(!obj.is(r0));
  LoadRoot(r0, index);
  cmp(obj, r0);
}


void MacroAssembler::CheckFastElements(Register map, Register scratch,
                                       Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  lbz(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  STATIC_ASSERT(Map::kMaximumBitField2FastHoleyElementValue < 0x8000);
  cmpli(scratch, Operand(Map::kMaximumBitField2FastHoleyElementValue));
  bgt(fail);
}


void MacroAssembler::CheckFastObjectElements(Register map, Register scratch,
                                             Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  lbz(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  cmpli(scratch, Operand(Map::kMaximumBitField2FastHoleySmiElementValue));
  ble(fail);
  cmpli(scratch, Operand(Map::kMaximumBitField2FastHoleyElementValue));
  bgt(fail);
}


void MacroAssembler::CheckFastSmiElements(Register map, Register scratch,
                                          Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  lbz(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  cmpli(scratch, Operand(Map::kMaximumBitField2FastHoleySmiElementValue));
  bgt(fail);
}


void MacroAssembler::StoreNumberToDoubleElements(
    Register value_reg, Register key_reg, Register elements_reg,
    Register scratch1, DoubleRegister double_scratch, Label* fail,
    int elements_offset) {
  Label smi_value, store;

  // Handle smi values specially.
  JumpIfSmi(value_reg, &smi_value);

  // Ensure that the object is a heap number
  CheckMap(value_reg, scratch1, isolate()->factory()->heap_number_map(), fail,
           DONT_DO_SMI_CHECK);

  lfd(double_scratch, FieldMemOperand(value_reg, HeapNumber::kValueOffset));
  // Double value, turn potential sNaN into qNaN.
  CanonicalizeNaN(double_scratch);
  b(&store);

  bind(&smi_value);
  SmiToDouble(double_scratch, value_reg);

  bind(&store);
  SmiToDoubleArrayOffset(scratch1, key_reg);
  add(scratch1, elements_reg, scratch1);
  stfd(double_scratch, FieldMemOperand(scratch1, FixedDoubleArray::kHeaderSize -
                                                     elements_offset));
}


void MacroAssembler::AddAndCheckForOverflow(Register dst, Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(!dst.is(overflow_dst));
  DCHECK(!dst.is(scratch));
  DCHECK(!overflow_dst.is(scratch));
  DCHECK(!overflow_dst.is(left));
  DCHECK(!overflow_dst.is(right));

  bool left_is_right = left.is(right);
  RCBit xorRC = left_is_right ? SetRC : LeaveRC;

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst.is(left)) {
    mr(scratch, left);            // Preserve left.
    add(dst, left, right);        // Left is overwritten.
    xor_(overflow_dst, dst, scratch, xorRC);  // Original left.
    if (!left_is_right) xor_(scratch, dst, right);
  } else if (dst.is(right)) {
    mr(scratch, right);           // Preserve right.
    add(dst, left, right);        // Right is overwritten.
    xor_(overflow_dst, dst, left, xorRC);
    if (!left_is_right) xor_(scratch, dst, scratch);  // Original right.
  } else {
    add(dst, left, right);
    xor_(overflow_dst, dst, left, xorRC);
    if (!left_is_right) xor_(scratch, dst, right);
  }
  if (!left_is_right) and_(overflow_dst, scratch, overflow_dst, SetRC);
}


void MacroAssembler::AddAndCheckForOverflow(Register dst, Register left,
                                            intptr_t right,
                                            Register overflow_dst,
                                            Register scratch) {
  Register original_left = left;
  DCHECK(!dst.is(overflow_dst));
  DCHECK(!dst.is(scratch));
  DCHECK(!overflow_dst.is(scratch));
  DCHECK(!overflow_dst.is(left));

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst.is(left)) {
    // Preserve left.
    original_left = overflow_dst;
    mr(original_left, left);
  }
  Add(dst, left, right, scratch);
  xor_(overflow_dst, dst, original_left);
  if (right >= 0) {
    and_(overflow_dst, overflow_dst, dst, SetRC);
  } else {
    andc(overflow_dst, overflow_dst, dst, SetRC);
  }
}


void MacroAssembler::SubAndCheckForOverflow(Register dst, Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(!dst.is(overflow_dst));
  DCHECK(!dst.is(scratch));
  DCHECK(!overflow_dst.is(scratch));
  DCHECK(!overflow_dst.is(left));
  DCHECK(!overflow_dst.is(right));

  // C = A-B; C overflows if A/B have diff signs and C has diff sign than A
  if (dst.is(left)) {
    mr(scratch, left);      // Preserve left.
    sub(dst, left, right);  // Left is overwritten.
    xor_(overflow_dst, dst, scratch);
    xor_(scratch, scratch, right);
    and_(overflow_dst, overflow_dst, scratch, SetRC);
  } else if (dst.is(right)) {
    mr(scratch, right);     // Preserve right.
    sub(dst, left, right);  // Right is overwritten.
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, scratch);
    and_(overflow_dst, overflow_dst, scratch, SetRC);
  } else {
    sub(dst, left, right);
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, right);
    and_(overflow_dst, scratch, overflow_dst, SetRC);
  }
}


void MacroAssembler::CompareMap(Register obj, Register scratch, Handle<Map> map,
                                Label* early_success) {
  LoadP(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareMap(scratch, map, early_success);
}


void MacroAssembler::CompareMap(Register obj_map, Handle<Map> map,
                                Label* early_success) {
  mov(r0, Operand(map));
  cmp(obj_map, r0);
}


void MacroAssembler::CheckMap(Register obj, Register scratch, Handle<Map> map,
                              Label* fail, SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  Label success;
  CompareMap(obj, scratch, map, &success);
  bne(fail);
  bind(&success);
}


void MacroAssembler::CheckMap(Register obj, Register scratch,
                              Heap::RootListIndex index, Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  LoadP(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  LoadRoot(r0, index);
  cmp(scratch, r0);
  bne(fail);
}


void MacroAssembler::DispatchWeakMap(Register obj, Register scratch1,
                                     Register scratch2, Handle<WeakCell> cell,
                                     Handle<Code> success,
                                     SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  LoadP(scratch1, FieldMemOperand(obj, HeapObject::kMapOffset));
  CmpWeakValue(scratch1, cell, scratch2);
  Jump(success, RelocInfo::CODE_TARGET, eq);
  bind(&fail);
}


void MacroAssembler::CmpWeakValue(Register value, Handle<WeakCell> cell,
                                  Register scratch, CRegister cr) {
  mov(scratch, Operand(cell));
  LoadP(scratch, FieldMemOperand(scratch, WeakCell::kValueOffset));
  cmp(value, scratch, cr);
}


void MacroAssembler::GetWeakValue(Register value, Handle<WeakCell> cell) {
  mov(value, Operand(cell));
  LoadP(value, FieldMemOperand(value, WeakCell::kValueOffset));
}


void MacroAssembler::LoadWeakValue(Register value, Handle<WeakCell> cell,
                                   Label* miss) {
  GetWeakValue(value, cell);
  JumpIfSmi(value, miss);
}


void MacroAssembler::GetMapConstructor(Register result, Register map,
                                       Register temp, Register temp2) {
  Label done, loop;
  LoadP(result, FieldMemOperand(map, Map::kConstructorOrBackPointerOffset));
  bind(&loop);
  JumpIfSmi(result, &done);
  CompareObjectType(result, temp, temp2, MAP_TYPE);
  bne(&done);
  LoadP(result, FieldMemOperand(result, Map::kConstructorOrBackPointerOffset));
  b(&loop);
  bind(&done);
}


void MacroAssembler::TryGetFunctionPrototype(Register function, Register result,
                                             Register scratch, Label* miss,
                                             bool miss_on_bound_function) {
  Label non_instance;
  if (miss_on_bound_function) {
    // Check that the receiver isn't a smi.
    JumpIfSmi(function, miss);

    // Check that the function really is a function.  Load map into result reg.
    CompareObjectType(function, result, scratch, JS_FUNCTION_TYPE);
    bne(miss);

    LoadP(scratch,
          FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    lwz(scratch,
        FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    TestBit(scratch,
#if V8_TARGET_ARCH_PPC64
            SharedFunctionInfo::kBoundFunction,
#else
            SharedFunctionInfo::kBoundFunction + kSmiTagSize,
#endif
            r0);
    bne(miss, cr0);

    // Make sure that the function has an instance prototype.
    lbz(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
    andi(r0, scratch, Operand(1 << Map::kHasNonInstancePrototype));
    bne(&non_instance, cr0);
  }

  // Get the prototype or initial map from the function.
  LoadP(result,
        FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  LoadRoot(r0, Heap::kTheHoleValueRootIndex);
  cmp(result, r0);
  beq(miss);

  // If the function does not have an initial map, we're done.
  Label done;
  CompareObjectType(result, scratch, scratch, MAP_TYPE);
  bne(&done);

  // Get the prototype from the initial map.
  LoadP(result, FieldMemOperand(result, Map::kPrototypeOffset));

  if (miss_on_bound_function) {
    b(&done);

    // Non-instance prototype: Fetch prototype from constructor field
    // in initial map.
    bind(&non_instance);
    GetMapConstructor(result, result, scratch, ip);
  }

  // All done.
  bind(&done);
}


void MacroAssembler::CallStub(CodeStub* stub, TypeFeedbackId ast_id,
                              Condition cond) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id, cond);
}


void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // If the hash field contains an array index pick it out. The assert checks
  // that the constants for the maximum number of digits for an array index
  // cached in the hash field and the number of bits reserved for it does not
  // conflict.
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  DecodeFieldToSmi<String::ArrayIndexValueBits>(index, hash);
}


void MacroAssembler::SmiToDouble(DoubleRegister value, Register smi) {
  SmiUntag(ip, smi);
  ConvertIntToDouble(ip, value);
}


void MacroAssembler::TestDoubleIsInt32(DoubleRegister double_input,
                                       Register scratch1, Register scratch2,
                                       DoubleRegister double_scratch) {
  TryDoubleToInt32Exact(scratch1, double_input, scratch2, double_scratch);
}


void MacroAssembler::TryDoubleToInt32Exact(Register result,
                                           DoubleRegister double_input,
                                           Register scratch,
                                           DoubleRegister double_scratch) {
  Label done;
  DCHECK(!double_input.is(double_scratch));

  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_PPC64
                       scratch,
#endif
                       result, double_scratch);

#if V8_TARGET_ARCH_PPC64
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  bne(&done);

  // convert back and compare
  fcfid(double_scratch, double_scratch);
  fcmpu(double_scratch, double_input);
  bind(&done);
}


void MacroAssembler::TryInt32Floor(Register result, DoubleRegister double_input,
                                   Register input_high, Register scratch,
                                   DoubleRegister double_scratch, Label* done,
                                   Label* exact) {
  DCHECK(!result.is(input_high));
  DCHECK(!double_input.is(double_scratch));
  Label exception;

  MovDoubleHighToInt(input_high, double_input);

  // Test for NaN/Inf
  ExtractBitMask(result, input_high, HeapNumber::kExponentMask);
  cmpli(result, Operand(0x7ff));
  beq(&exception);

  // Convert (rounding to -Inf)
  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_PPC64
                       scratch,
#endif
                       result, double_scratch, kRoundToMinusInf);

// Test for overflow
#if V8_TARGET_ARCH_PPC64
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  bne(&exception);

  // Test for exactness
  fcfid(double_scratch, double_scratch);
  fcmpu(double_scratch, double_input);
  beq(exact);
  b(done);

  bind(&exception);
}


void MacroAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  DoubleRegister double_scratch = kScratchDoubleReg;
#if !V8_TARGET_ARCH_PPC64
  Register scratch = ip;
#endif

  ConvertDoubleToInt64(double_input,
#if !V8_TARGET_ARCH_PPC64
                       scratch,
#endif
                       result, double_scratch);

// Test for overflow
#if V8_TARGET_ARCH_PPC64
  TestIfInt32(result, r0);
#else
  TestIfInt32(scratch, result, r0);
#endif
  beq(done);
}


void MacroAssembler::TruncateDoubleToI(Register result,
                                       DoubleRegister double_input) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  mflr(r0);
  push(r0);
  // Put input on stack.
  stfdu(double_input, MemOperand(sp, -kDoubleSize));

  DoubleToIStub stub(isolate(), sp, result, 0, true, true);
  CallStub(&stub);

  addi(sp, sp, Operand(kDoubleSize));
  pop(r0);
  mtlr(r0);

  bind(&done);
}


void MacroAssembler::TruncateHeapNumberToI(Register result, Register object) {
  Label done;
  DoubleRegister double_scratch = kScratchDoubleReg;
  DCHECK(!result.is(object));

  lfd(double_scratch, FieldMemOperand(object, HeapNumber::kValueOffset));
  TryInlineTruncateDoubleToI(result, double_scratch, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  mflr(r0);
  push(r0);
  DoubleToIStub stub(isolate(), object, result,
                     HeapNumber::kValueOffset - kHeapObjectTag, true, true);
  CallStub(&stub);
  pop(r0);
  mtlr(r0);

  bind(&done);
}


void MacroAssembler::TruncateNumberToI(Register object, Register result,
                                       Register heap_number_map,
                                       Register scratch1, Label* not_number) {
  Label done;
  DCHECK(!result.is(object));

  UntagAndJumpIfSmi(result, object, &done);
  JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_number);
  TruncateHeapNumberToI(result, object);

  bind(&done);
}


void MacroAssembler::GetLeastBitsFromSmi(Register dst, Register src,
                                         int num_least_bits) {
#if V8_TARGET_ARCH_PPC64
  rldicl(dst, src, kBitsPerPointer - kSmiShift,
         kBitsPerPointer - num_least_bits);
#else
  rlwinm(dst, src, kBitsPerPointer - kSmiShift,
         kBitsPerPointer - num_least_bits, 31);
#endif
}


void MacroAssembler::GetLeastBitsFromInt32(Register dst, Register src,
                                           int num_least_bits) {
  rlwinm(dst, src, 0, 32 - num_least_bits, 31);
}


void MacroAssembler::CallRuntime(const Runtime::Function* f, int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All parameters are on the stack.  r3 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r3, Operand(num_arguments));
  mov(r4, Operand(ExternalReference(f, isolate())));
  CEntryStub stub(isolate(),
#if V8_TARGET_ARCH_PPC64
                  f->result_size,
#else
                  1,
#endif
                  save_doubles);
  CallStub(&stub);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  mov(r3, Operand(num_arguments));
  mov(r4, Operand(ext));

  CEntryStub stub(isolate(), 1);
  CallStub(&stub);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r3, Operand(num_arguments));
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid, int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid, isolate()), num_arguments,
                            result_size);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
  mov(r4, Operand(builtin));
  CEntryStub stub(isolate(), 1);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id, InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  GetBuiltinEntry(ip, id);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(ip));
    CallJSEntry(ip);
    call_wrapper.AfterCall();
  } else {
    DCHECK(flag == JUMP_FUNCTION);
    JumpToJSEntry(ip);
  }
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  LoadP(target,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  LoadP(target, FieldMemOperand(target, GlobalObject::kBuiltinsOffset));
  // Load the JavaScript builtin function from the builtins object.
  LoadP(target,
        FieldMemOperand(target, JSBuiltinsObject::OffsetOfFunctionWithId(id)),
        r0);
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  DCHECK(!target.is(r4));
  GetBuiltinFunction(r4, id);
  // Load the code entry point from the builtins object.
  LoadP(target, FieldMemOperand(r4, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(value));
    mov(scratch2, Operand(ExternalReference(counter)));
    stw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch2, Operand(ExternalReference(counter)));
    lwz(scratch1, MemOperand(scratch2));
    addi(scratch1, scratch1, Operand(value));
    stw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch2, Operand(ExternalReference(counter)));
    lwz(scratch1, MemOperand(scratch2));
    subi(scratch1, scratch1, Operand(value));
    stw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::Assert(Condition cond, BailoutReason reason,
                            CRegister cr) {
  if (emit_debug_code()) Check(cond, reason, cr);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    DCHECK(!elements.is(r0));
    Label ok;
    push(elements);
    LoadP(elements, FieldMemOperand(elements, HeapObject::kMapOffset));
    LoadRoot(r0, Heap::kFixedArrayMapRootIndex);
    cmp(elements, r0);
    beq(&ok);
    LoadRoot(r0, Heap::kFixedDoubleArrayMapRootIndex);
    cmp(elements, r0);
    beq(&ok);
    LoadRoot(r0, Heap::kFixedCOWArrayMapRootIndex);
    cmp(elements, r0);
    beq(&ok);
    Abort(kJSObjectWithFastElementsMapHasSlowElements);
    bind(&ok);
    pop(elements);
  }
}


void MacroAssembler::Check(Condition cond, BailoutReason reason, CRegister cr) {
  Label L;
  b(cond, &L, cr);
  Abort(reason);
  // will not return here
  bind(&L);
}


void MacroAssembler::Abort(BailoutReason reason) {
  Label abort_start;
  bind(&abort_start);
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    stop(msg);
    return;
  }
#endif

  LoadSmiLiteral(r0, Smi::FromInt(reason));
  push(r0);
  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kAbort, 1);
  } else {
    CallRuntime(Runtime::kAbort, 1);
  }
  // will not return here
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    LoadP(dst, MemOperand(cp, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      LoadP(dst, MemOperand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in esi).
    mr(dst, cp);
  }
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind, ElementsKind transitioned_kind,
    Register map_in_out, Register scratch, Label* no_map_match) {
  // Load the global or builtins object from the current context.
  LoadP(scratch,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  LoadP(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  LoadP(scratch,
        MemOperand(scratch, Context::SlotOffset(Context::JS_ARRAY_MAPS_INDEX)));
  size_t offset = expected_kind * kPointerSize + FixedArrayBase::kHeaderSize;
  LoadP(ip, FieldMemOperand(scratch, offset));
  cmp(map_in_out, ip);
  bne(no_map_match);

  // Use the transitioned cached map.
  offset = transitioned_kind * kPointerSize + FixedArrayBase::kHeaderSize;
  LoadP(map_in_out, FieldMemOperand(scratch, offset));
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  LoadP(function,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  LoadP(function,
        FieldMemOperand(function, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  LoadP(function, MemOperand(function, Context::SlotOffset(index)), r0);
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map,
                                                  Register scratch) {
  // Load the initial map. The global functions all have initial maps.
  LoadP(map,
        FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, scratch, Heap::kMetaMapRootIndex, &fail, DO_SMI_CHECK);
    b(&ok);
    bind(&fail);
    Abort(kGlobalFunctionsMustHaveInitialMap);
    bind(&ok);
  }
}


void MacroAssembler::JumpIfNotPowerOfTwoOrZero(
    Register reg, Register scratch, Label* not_power_of_two_or_zero) {
  subi(scratch, reg, Operand(1));
  cmpi(scratch, Operand::Zero());
  blt(not_power_of_two_or_zero);
  and_(r0, scratch, reg, SetRC);
  bne(not_power_of_two_or_zero, cr0);
}


void MacroAssembler::JumpIfNotPowerOfTwoOrZeroAndNeg(Register reg,
                                                     Register scratch,
                                                     Label* zero_and_neg,
                                                     Label* not_power_of_two) {
  subi(scratch, reg, Operand(1));
  cmpi(scratch, Operand::Zero());
  blt(zero_and_neg);
  and_(r0, scratch, reg, SetRC);
  bne(not_power_of_two, cr0);
}

#if !V8_TARGET_ARCH_PPC64
void MacroAssembler::SmiTagCheckOverflow(Register reg, Register overflow) {
  DCHECK(!reg.is(overflow));
  mr(overflow, reg);  // Save original value.
  SmiTag(reg);
  xor_(overflow, overflow, reg, SetRC);  // Overflow if (value ^ 2 * value) < 0.
}


void MacroAssembler::SmiTagCheckOverflow(Register dst, Register src,
                                         Register overflow) {
  if (dst.is(src)) {
    // Fall back to slower case.
    SmiTagCheckOverflow(dst, overflow);
  } else {
    DCHECK(!dst.is(src));
    DCHECK(!dst.is(overflow));
    DCHECK(!src.is(overflow));
    SmiTag(dst, src);
    xor_(overflow, dst, src, SetRC);  // Overflow if (value ^ 2 * value) < 0.
  }
}
#endif

void MacroAssembler::JumpIfNotBothSmi(Register reg1, Register reg2,
                                      Label* on_not_both_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  orx(r0, reg1, reg2, LeaveRC);
  JumpIfNotSmi(r0, on_not_both_smi);
}


void MacroAssembler::UntagAndJumpIfSmi(Register dst, Register src,
                                       Label* smi_case) {
  STATIC_ASSERT(kSmiTag == 0);
  TestBitRange(src, kSmiTagSize - 1, 0, r0);
  SmiUntag(dst, src);
  beq(smi_case, cr0);
}


void MacroAssembler::UntagAndJumpIfNotSmi(Register dst, Register src,
                                          Label* non_smi_case) {
  STATIC_ASSERT(kSmiTag == 0);
  TestBitRange(src, kSmiTagSize - 1, 0, r0);
  SmiUntag(dst, src);
  bne(non_smi_case, cr0);
}


void MacroAssembler::JumpIfEitherSmi(Register reg1, Register reg2,
                                     Label* on_either_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  JumpIfSmi(reg1, on_either_smi);
  JumpIfSmi(reg2, on_either_smi);
}


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, kOperandIsASmi, cr0);
  }
}


void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(eq, kOperandIsNotSmi, cr0);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, kOperandIsASmiAndNotAString, cr0);
    push(object);
    LoadP(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, FIRST_NONSTRING_TYPE);
    pop(object);
    Check(lt, kOperandIsNotAString);
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object, r0);
    Check(ne, kOperandIsASmiAndNotAName, cr0);
    push(object);
    LoadP(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, LAST_NAME_TYPE);
    pop(object);
    Check(le, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, Heap::kUndefinedValueRootIndex);
    beq(&done_checking);
    LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareRoot(scratch, Heap::kAllocationSiteMapRootIndex);
    Assert(eq, kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}


void MacroAssembler::AssertIsRoot(Register reg, Heap::RootListIndex index) {
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, kHeapNumberMapRegisterClobbered);
  }
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Register heap_number_map,
                                         Register scratch,
                                         Label* on_not_heap_number) {
  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  AssertIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  cmp(scratch, heap_number_map);
  bne(on_not_heap_number);
}


void MacroAssembler::LookupNumberStringCache(Register object, Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Register scratch3,
                                             Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch3;

  // Load the number string cache.
  LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  LoadP(mask, FieldMemOperand(number_string_cache, FixedArray::kLengthOffset));
  // Divide length by two (length is a smi).
  ShiftRightArithImm(mask, mask, kSmiTagSize + kSmiShiftSize + 1);
  subi(mask, mask, Operand(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  JumpIfSmi(object, &is_smi);
  CheckMap(object, scratch1, Heap::kHeapNumberMapRootIndex, not_found,
           DONT_DO_SMI_CHECK);

  STATIC_ASSERT(8 == kDoubleSize);
  lwz(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
  lwz(scratch2, FieldMemOperand(object, HeapNumber::kMantissaOffset));
  xor_(scratch1, scratch1, scratch2);
  and_(scratch1, scratch1, mask);

  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  ShiftLeftImm(scratch1, scratch1, Operand(kPointerSizeLog2 + 1));
  add(scratch1, number_string_cache, scratch1);

  Register probe = mask;
  LoadP(probe, FieldMemOperand(scratch1, FixedArray::kHeaderSize));
  JumpIfSmi(probe, not_found);
  lfd(d0, FieldMemOperand(object, HeapNumber::kValueOffset));
  lfd(d1, FieldMemOperand(probe, HeapNumber::kValueOffset));
  fcmpu(d0, d1);
  bne(not_found);  // The cache did not contain this value.
  b(&load_result_from_cache);

  bind(&is_smi);
  Register scratch = scratch1;
  SmiUntag(scratch, object);
  and_(scratch, mask, scratch);
  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  ShiftLeftImm(scratch, scratch, Operand(kPointerSizeLog2 + 1));
  add(scratch, number_string_cache, scratch);

  // Check if the entry is the smi we are looking for.
  LoadP(probe, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  cmp(object, probe);
  bne(not_found);

  // Get the result from the cache.
  bind(&load_result_from_cache);
  LoadP(result,
        FieldMemOperand(scratch, FixedArray::kHeaderSize + kPointerSize));
  IncrementCounter(isolate()->counters()->number_to_string_native(), 1,
                   scratch1, scratch2);
}


void MacroAssembler::JumpIfNonSmisNotBothSequentialOneByteStrings(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  // Test that both first and second are sequential one-byte strings.
  // Assume that they are non-smis.
  LoadP(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  LoadP(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  lbz(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  lbz(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfBothInstanceTypesAreNotSequentialOneByte(scratch1, scratch2, scratch1,
                                                 scratch2, failure);
}

void MacroAssembler::JumpIfNotBothSequentialOneByteStrings(Register first,
                                                           Register second,
                                                           Register scratch1,
                                                           Register scratch2,
                                                           Label* failure) {
  // Check that neither is a smi.
  and_(scratch1, first, second);
  JumpIfSmi(scratch1, failure);
  JumpIfNonSmisNotBothSequentialOneByteStrings(first, second, scratch1,
                                               scratch2, failure);
}


void MacroAssembler::JumpIfNotUniqueNameInstanceType(Register reg,
                                                     Label* not_unique_name) {
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  Label succeed;
  andi(r0, reg, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  beq(&succeed, cr0);
  cmpi(reg, Operand(SYMBOL_TYPE));
  bne(not_unique_name);

  bind(&succeed);
}


// Allocates a heap number or jumps to the need_gc label if the young space
// is full and a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result, Register scratch1,
                                        Register scratch2,
                                        Register heap_number_map,
                                        Label* gc_required,
                                        TaggingMode tagging_mode,
                                        MutableMode mode) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           tagging_mode == TAG_RESULT ? TAG_OBJECT : NO_ALLOCATION_FLAGS);

  Heap::RootListIndex map_index = mode == MUTABLE
                                      ? Heap::kMutableHeapNumberMapRootIndex
                                      : Heap::kHeapNumberMapRootIndex;
  AssertIsRoot(heap_number_map, map_index);

  // Store heap number map in the allocated object.
  if (tagging_mode == TAG_RESULT) {
    StoreP(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset),
           r0);
  } else {
    StoreP(heap_number_map, MemOperand(result, HeapObject::kMapOffset));
  }
}


void MacroAssembler::AllocateHeapNumberWithValue(
    Register result, DoubleRegister value, Register scratch1, Register scratch2,
    Register heap_number_map, Label* gc_required) {
  AllocateHeapNumber(result, scratch1, scratch2, heap_number_map, gc_required);
  stfd(value, FieldMemOperand(result, HeapNumber::kValueOffset));
}


// Copies a fixed number of fields of heap objects from src to dst.
void MacroAssembler::CopyFields(Register dst, Register src, RegList temps,
                                int field_count) {
  // At least one bit set in the first 15 registers.
  DCHECK((temps & ((1 << 15) - 1)) != 0);
  DCHECK((temps & dst.bit()) == 0);
  DCHECK((temps & src.bit()) == 0);
  // Primitive implementation using only one temporary register.

  Register tmp = no_reg;
  // Find a temp register in temps list.
  for (int i = 0; i < 15; i++) {
    if ((temps & (1 << i)) != 0) {
      tmp.set_code(i);
      break;
    }
  }
  DCHECK(!tmp.is(no_reg));

  for (int i = 0; i < field_count; i++) {
    LoadP(tmp, FieldMemOperand(src, i * kPointerSize), r0);
    StoreP(tmp, FieldMemOperand(dst, i * kPointerSize), r0);
  }
}


void MacroAssembler::CopyBytes(Register src, Register dst, Register length,
                               Register scratch) {
  Label align_loop, aligned, word_loop, byte_loop, byte_loop_1, done;

  DCHECK(!scratch.is(r0));

  cmpi(length, Operand::Zero());
  beq(&done);

  // Check src alignment and length to see whether word_loop is possible
  andi(scratch, src, Operand(kPointerSize - 1));
  beq(&aligned, cr0);
  subfic(scratch, scratch, Operand(kPointerSize * 2));
  cmp(length, scratch);
  blt(&byte_loop);

  // Align src before copying in word size chunks.
  subi(scratch, scratch, Operand(kPointerSize));
  mtctr(scratch);
  bind(&align_loop);
  lbz(scratch, MemOperand(src));
  addi(src, src, Operand(1));
  subi(length, length, Operand(1));
  stb(scratch, MemOperand(dst));
  addi(dst, dst, Operand(1));
  bdnz(&align_loop);

  bind(&aligned);

  // Copy bytes in word size chunks.
  if (emit_debug_code()) {
    andi(r0, src, Operand(kPointerSize - 1));
    Assert(eq, kExpectingAlignmentForCopyBytes, cr0);
  }

  ShiftRightImm(scratch, length, Operand(kPointerSizeLog2));
  cmpi(scratch, Operand::Zero());
  beq(&byte_loop);

  mtctr(scratch);
  bind(&word_loop);
  LoadP(scratch, MemOperand(src));
  addi(src, src, Operand(kPointerSize));
  subi(length, length, Operand(kPointerSize));
  if (CpuFeatures::IsSupported(UNALIGNED_ACCESSES)) {
    // currently false for PPC - but possible future opt
    StoreP(scratch, MemOperand(dst));
    addi(dst, dst, Operand(kPointerSize));
  } else {
#if V8_TARGET_LITTLE_ENDIAN
    stb(scratch, MemOperand(dst, 0));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 1));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 2));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 3));
#if V8_TARGET_ARCH_PPC64
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 4));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 5));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 6));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 7));
#endif
#else
#if V8_TARGET_ARCH_PPC64
    stb(scratch, MemOperand(dst, 7));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 6));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 5));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 4));
    ShiftRightImm(scratch, scratch, Operand(8));
#endif
    stb(scratch, MemOperand(dst, 3));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 2));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 1));
    ShiftRightImm(scratch, scratch, Operand(8));
    stb(scratch, MemOperand(dst, 0));
#endif
    addi(dst, dst, Operand(kPointerSize));
  }
  bdnz(&word_loop);

  // Copy the last bytes if any left.
  cmpi(length, Operand::Zero());
  beq(&done);

  bind(&byte_loop);
  mtctr(length);
  bind(&byte_loop_1);
  lbz(scratch, MemOperand(src));
  addi(src, src, Operand(1));
  stb(scratch, MemOperand(dst));
  addi(dst, dst, Operand(1));
  bdnz(&byte_loop_1);

  bind(&done);
}


void MacroAssembler::InitializeNFieldsWithFiller(Register start_offset,
                                                 Register count,
                                                 Register filler) {
  Label loop;
  mtctr(count);
  bind(&loop);
  StoreP(filler, MemOperand(start_offset));
  addi(start_offset, start_offset, Operand(kPointerSize));
  bdnz(&loop);
}

void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label done;
  sub(r0, end_offset, start_offset, LeaveOE, SetRC);
  beq(&done, cr0);
  ShiftRightImm(r0, r0, Operand(kPointerSizeLog2));
  InitializeNFieldsWithFiller(start_offset, r0, filler);
  bind(&done);
}


void MacroAssembler::SaveFPRegs(Register location, int first, int count) {
  DCHECK(count > 0);
  int cur = first;
  subi(location, location, Operand(count * kDoubleSize));
  for (int i = 0; i < count; i++) {
    DoubleRegister reg = DoubleRegister::from_code(cur++);
    stfd(reg, MemOperand(location, i * kDoubleSize));
  }
}


void MacroAssembler::RestoreFPRegs(Register location, int first, int count) {
  DCHECK(count > 0);
  int cur = first + count - 1;
  for (int i = count - 1; i >= 0; i--) {
    DoubleRegister reg = DoubleRegister::from_code(cur--);
    lfd(reg, MemOperand(location, i * kDoubleSize));
  }
  addi(location, location, Operand(count * kDoubleSize));
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialOneByte(
    Register first, Register second, Register scratch1, Register scratch2,
    Label* failure) {
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  andi(scratch1, first, Operand(kFlatOneByteStringMask));
  andi(scratch2, second, Operand(kFlatOneByteStringMask));
  cmpi(scratch1, Operand(kFlatOneByteStringTag));
  bne(failure);
  cmpi(scratch2, Operand(kFlatOneByteStringTag));
  bne(failure);
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialOneByte(Register type,
                                                              Register scratch,
                                                              Label* failure) {
  const int kFlatOneByteStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatOneByteStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  andi(scratch, type, Operand(kFlatOneByteStringMask));
  cmpi(scratch, Operand(kFlatOneByteStringTag));
  bne(failure);
}

static const int kRegisterPassedArguments = 8;


int MacroAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  if (num_double_arguments > DoubleRegister::kNumRegisters) {
    stack_passed_words +=
        2 * (num_double_arguments - DoubleRegister::kNumRegisters);
  }
  // Up to 8 simple arguments are passed in registers r3..r10.
  if (num_reg_arguments > kRegisterPassedArguments) {
    stack_passed_words += num_reg_arguments - kRegisterPassedArguments;
  }
  return stack_passed_words;
}


void MacroAssembler::EmitSeqStringSetCharCheck(Register string, Register index,
                                               Register value,
                                               uint32_t encoding_mask) {
  Label is_object;
  TestIfSmi(string, r0);
  Check(ne, kNonObject, cr0);

  LoadP(ip, FieldMemOperand(string, HeapObject::kMapOffset));
  lbz(ip, FieldMemOperand(ip, Map::kInstanceTypeOffset));

  andi(ip, ip, Operand(kStringRepresentationMask | kStringEncodingMask));
  cmpi(ip, Operand(encoding_mask));
  Check(eq, kUnexpectedStringType);

// The index is assumed to be untagged coming in, tag it to compare with the
// string length without using a temp register, it is restored at the end of
// this function.
#if !V8_TARGET_ARCH_PPC64
  Label index_tag_ok, index_tag_bad;
  JumpIfNotSmiCandidate(index, r0, &index_tag_bad);
#endif
  SmiTag(index, index);
#if !V8_TARGET_ARCH_PPC64
  b(&index_tag_ok);
  bind(&index_tag_bad);
  Abort(kIndexIsTooLarge);
  bind(&index_tag_ok);
#endif

  LoadP(ip, FieldMemOperand(string, String::kLengthOffset));
  cmp(index, ip);
  Check(lt, kIndexIsTooLarge);

  DCHECK(Smi::FromInt(0) == 0);
  cmpi(index, Operand::Zero());
  Check(ge, kIndexIsNegative);

  SmiUntag(index, index);
}


void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  int frame_alignment = ActivationFrameAlignment();
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots;

  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for stack arguments
    // -- preserving original value of sp.
    mr(scratch, sp);
    addi(sp, sp, Operand(-(stack_passed_arguments + 1) * kPointerSize));
    DCHECK(base::bits::IsPowerOfTwo32(frame_alignment));
    ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
    StoreP(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    // Make room for stack arguments
    stack_space += stack_passed_arguments;
  }

  // Allocate frame with required slots to make ABI work.
  li(r0, Operand::Zero());
  StorePU(r0, MemOperand(sp, -stack_space * kPointerSize));
}


void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}


void MacroAssembler::MovToFloatParameter(DoubleRegister src) { Move(d1, src); }


void MacroAssembler::MovToFloatResult(DoubleRegister src) { Move(d1, src); }


void MacroAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  if (src2.is(d1)) {
    DCHECK(!src1.is(d2));
    Move(d2, src2);
    Move(d1, src1);
  } else {
    Move(d1, src1);
    Move(d2, src2);
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  mov(ip, Operand(function));
  CallCFunctionHelper(ip, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(Register function, int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunction(Register function, int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  DCHECK(has_frame());
// Just call directly. The function called cannot cause a GC, or
// allow preemption, so the return address in the link register
// stays correct.
#if ABI_USES_FUNCTION_DESCRIPTORS && !defined(USE_SIMULATOR)
  // AIX uses a function descriptor. When calling C code be aware
  // of this descriptor and pick up values from it
  LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(function, kPointerSize));
  LoadP(ip, MemOperand(function, 0));
  Register dest = ip;
#elif ABI_TOC_ADDRESSABILITY_VIA_IP
  Move(ip, function);
  Register dest = ip;
#else
  Register dest = function;
#endif

  Call(dest);

  // Remove frame bought in PrepareCallCFunction
  int stack_passed_arguments =
      CalculateStackPassedWords(num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots + stack_passed_arguments;
  if (ActivationFrameAlignment() > kPointerSize) {
    LoadP(sp, MemOperand(sp, stack_space * kPointerSize));
  } else {
    addi(sp, sp, Operand(stack_space * kPointerSize));
  }
}


void MacroAssembler::FlushICache(Register address, size_t size,
                                 Register scratch) {
  if (CpuFeatures::IsSupported(INSTR_AND_DATA_CACHE_COHERENCY)) {
    sync();
    icbi(r0, address);
    isync();
    return;
  }

  Label done;

  dcbf(r0, address);
  sync();
  icbi(r0, address);
  isync();

  // This code handles ranges which cross a single cacheline boundary.
  // scratch is last cacheline which intersects range.
  const int kCacheLineSizeLog2 = WhichPowerOf2(CpuFeatures::cache_line_size());

  DCHECK(size > 0 && size <= (size_t)(1 << kCacheLineSizeLog2));
  addi(scratch, address, Operand(size - 1));
  ClearRightImm(scratch, scratch, Operand(kCacheLineSizeLog2));
  cmpl(scratch, address);
  ble(&done);

  dcbf(r0, scratch);
  sync();
  icbi(r0, scratch);
  isync();

  bind(&done);
}


void MacroAssembler::DecodeConstantPoolOffset(Register result,
                                              Register location) {
  Label overflow_access, done;
  DCHECK(!AreAliased(result, location, r0));

  // Determine constant pool access type
  // Caller has already placed the instruction word at location in result.
  ExtractBitRange(r0, result, 31, 26);
  cmpi(r0, Operand(ADDIS >> 26));
  beq(&overflow_access);

  // Regular constant pool access
  // extract the load offset
  andi(result, result, Operand(kImm16Mask));
  b(&done);

  bind(&overflow_access);
  // Overflow constant pool access
  // shift addis immediate
  slwi(r0, result, Operand(16));
  // sign-extend and add the load offset
  lwz(result, MemOperand(location, kInstrSize));
  extsh(result, result);
  add(result, r0, result);

  bind(&done);
}


void MacroAssembler::SetRelocatedValue(Register location, Register scratch,
                                       Register new_value) {
  lwz(scratch, MemOperand(location));

  if (FLAG_enable_embedded_constant_pool) {
    if (emit_debug_code()) {
      // Check that the instruction sequence is a load from the constant pool
      ExtractBitMask(scratch, scratch, 0x1f * B16);
      cmpi(scratch, Operand(kConstantPoolRegister.code()));
      Check(eq, kTheInstructionToPatchShouldBeALoadFromConstantPool);
      // Scratch was clobbered. Restore it.
      lwz(scratch, MemOperand(location));
    }
    DecodeConstantPoolOffset(scratch, location);
    StorePX(new_value, MemOperand(kConstantPoolRegister, scratch));
    return;
  }

  // This code assumes a FIXED_SEQUENCE for lis/ori

  // At this point scratch is a lis instruction.
  if (emit_debug_code()) {
    And(scratch, scratch, Operand(kOpcodeMask | (0x1f * B16)));
    Cmpi(scratch, Operand(ADDIS), r0);
    Check(eq, kTheInstructionToPatchShouldBeALis);
    lwz(scratch, MemOperand(location));
  }

// insert new high word into lis instruction
#if V8_TARGET_ARCH_PPC64
  srdi(ip, new_value, Operand(32));
  rlwimi(scratch, ip, 16, 16, 31);
#else
  rlwimi(scratch, new_value, 16, 16, 31);
#endif

  stw(scratch, MemOperand(location));

  lwz(scratch, MemOperand(location, kInstrSize));
  // scratch is now ori.
  if (emit_debug_code()) {
    And(scratch, scratch, Operand(kOpcodeMask));
    Cmpi(scratch, Operand(ORI), r0);
    Check(eq, kTheInstructionShouldBeAnOri);
    lwz(scratch, MemOperand(location, kInstrSize));
  }

// insert new low word into ori instruction
#if V8_TARGET_ARCH_PPC64
  rlwimi(scratch, ip, 0, 16, 31);
#else
  rlwimi(scratch, new_value, 0, 16, 31);
#endif
  stw(scratch, MemOperand(location, kInstrSize));

#if V8_TARGET_ARCH_PPC64
  if (emit_debug_code()) {
    lwz(scratch, MemOperand(location, 2 * kInstrSize));
    // scratch is now sldi.
    And(scratch, scratch, Operand(kOpcodeMask | kExt5OpcodeMask));
    Cmpi(scratch, Operand(EXT5 | RLDICR), r0);
    Check(eq, kTheInstructionShouldBeASldi);
  }

  lwz(scratch, MemOperand(location, 3 * kInstrSize));
  // scratch is now ori.
  if (emit_debug_code()) {
    And(scratch, scratch, Operand(kOpcodeMask));
    Cmpi(scratch, Operand(ORIS), r0);
    Check(eq, kTheInstructionShouldBeAnOris);
    lwz(scratch, MemOperand(location, 3 * kInstrSize));
  }

  rlwimi(scratch, new_value, 16, 16, 31);
  stw(scratch, MemOperand(location, 3 * kInstrSize));

  lwz(scratch, MemOperand(location, 4 * kInstrSize));
  // scratch is now ori.
  if (emit_debug_code()) {
    And(scratch, scratch, Operand(kOpcodeMask));
    Cmpi(scratch, Operand(ORI), r0);
    Check(eq, kTheInstructionShouldBeAnOri);
    lwz(scratch, MemOperand(location, 4 * kInstrSize));
  }
  rlwimi(scratch, new_value, 0, 16, 31);
  stw(scratch, MemOperand(location, 4 * kInstrSize));
#endif

// Update the I-cache so the new lis and addic can be executed.
#if V8_TARGET_ARCH_PPC64
  FlushICache(location, 5 * kInstrSize, scratch);
#else
  FlushICache(location, 2 * kInstrSize, scratch);
#endif
}


void MacroAssembler::GetRelocatedValue(Register location, Register result,
                                       Register scratch) {
  lwz(result, MemOperand(location));

  if (FLAG_enable_embedded_constant_pool) {
    if (emit_debug_code()) {
      // Check that the instruction sequence is a load from the constant pool
      ExtractBitMask(result, result, 0x1f * B16);
      cmpi(result, Operand(kConstantPoolRegister.code()));
      Check(eq, kTheInstructionToPatchShouldBeALoadFromConstantPool);
      lwz(result, MemOperand(location));
    }
    DecodeConstantPoolOffset(result, location);
    LoadPX(result, MemOperand(kConstantPoolRegister, result));
    return;
  }

  // This code assumes a FIXED_SEQUENCE for lis/ori
  if (emit_debug_code()) {
    And(result, result, Operand(kOpcodeMask | (0x1f * B16)));
    Cmpi(result, Operand(ADDIS), r0);
    Check(eq, kTheInstructionShouldBeALis);
    lwz(result, MemOperand(location));
  }

  // result now holds a lis instruction. Extract the immediate.
  slwi(result, result, Operand(16));

  lwz(scratch, MemOperand(location, kInstrSize));
  if (emit_debug_code()) {
    And(scratch, scratch, Operand(kOpcodeMask));
    Cmpi(scratch, Operand(ORI), r0);
    Check(eq, kTheInstructionShouldBeAnOri);
    lwz(scratch, MemOperand(location, kInstrSize));
  }
  // Copy the low 16bits from ori instruction into result
  rlwimi(result, scratch, 0, 16, 31);

#if V8_TARGET_ARCH_PPC64
  if (emit_debug_code()) {
    lwz(scratch, MemOperand(location, 2 * kInstrSize));
    // scratch is now sldi.
    And(scratch, scratch, Operand(kOpcodeMask | kExt5OpcodeMask));
    Cmpi(scratch, Operand(EXT5 | RLDICR), r0);
    Check(eq, kTheInstructionShouldBeASldi);
  }

  lwz(scratch, MemOperand(location, 3 * kInstrSize));
  // scratch is now ori.
  if (emit_debug_code()) {
    And(scratch, scratch, Operand(kOpcodeMask));
    Cmpi(scratch, Operand(ORIS), r0);
    Check(eq, kTheInstructionShouldBeAnOris);
    lwz(scratch, MemOperand(location, 3 * kInstrSize));
  }
  sldi(result, result, Operand(16));
  rldimi(result, scratch, 0, 48);

  lwz(scratch, MemOperand(location, 4 * kInstrSize));
  // scratch is now ori.
  if (emit_debug_code()) {
    And(scratch, scratch, Operand(kOpcodeMask));
    Cmpi(scratch, Operand(ORI), r0);
    Check(eq, kTheInstructionShouldBeAnOri);
    lwz(scratch, MemOperand(location, 4 * kInstrSize));
  }
  sldi(result, result, Operand(16));
  rldimi(result, scratch, 0, 48);
#endif
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,  // scratch may be same register as object
    int mask, Condition cc, Label* condition_met) {
  DCHECK(cc == ne || cc == eq);
  ClearRightImm(scratch, object, Operand(kPageSizeBits));
  LoadP(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));

  And(r0, scratch, Operand(mask), SetRC);

  if (cc == ne) {
    bne(condition_met, cr0);
  }
  if (cc == eq) {
    beq(condition_met, cr0);
  }
}


void MacroAssembler::JumpIfBlack(Register object, Register scratch0,
                                 Register scratch1, Label* on_black) {
  HasColor(object, scratch0, scratch1, on_black, 1, 0);  // kBlackBitPattern.
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
}


void MacroAssembler::HasColor(Register object, Register bitmap_scratch,
                              Register mask_scratch, Label* has_color,
                              int first_bit, int second_bit) {
  DCHECK(!AreAliased(object, bitmap_scratch, mask_scratch, no_reg));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  Label other_color, word_boundary;
  lwz(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  // Test the first bit
  and_(r0, ip, mask_scratch, SetRC);
  b(first_bit == 1 ? eq : ne, &other_color, cr0);
  // Shift left 1
  // May need to load the next cell
  slwi(mask_scratch, mask_scratch, Operand(1), SetRC);
  beq(&word_boundary, cr0);
  // Test the second bit
  and_(r0, ip, mask_scratch, SetRC);
  b(second_bit == 1 ? ne : eq, has_color, cr0);
  b(&other_color);

  bind(&word_boundary);
  lwz(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize + kIntSize));
  andi(r0, ip, Operand(1));
  b(second_bit == 1 ? ne : eq, has_color, cr0);
  bind(&other_color);
}


// Detect some, but not all, common pointer-free objects.  This is used by the
// incremental write barrier which doesn't care about oddballs (they are always
// marked black immediately so this code is not hit).
void MacroAssembler::JumpIfDataObject(Register value, Register scratch,
                                      Label* not_data_object) {
  Label is_data_object;
  LoadP(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  beq(&is_data_object);
  DCHECK(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  DCHECK(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  lbz(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT((kIsIndirectStringMask | kIsNotStringMask) == 0x81);
  andi(scratch, scratch, Operand(kIsIndirectStringMask | kIsNotStringMask));
  bne(not_data_object, cr0);
  bind(&is_data_object);
}


void MacroAssembler::GetMarkBits(Register addr_reg, Register bitmap_reg,
                                 Register mask_reg) {
  DCHECK(!AreAliased(addr_reg, bitmap_reg, mask_reg, no_reg));
  DCHECK((~Page::kPageAlignmentMask & 0xffff) == 0);
  lis(r0, Operand((~Page::kPageAlignmentMask >> 16)));
  and_(bitmap_reg, addr_reg, r0);
  const int kLowBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  ExtractBitRange(mask_reg, addr_reg, kLowBits - 1, kPointerSizeLog2);
  ExtractBitRange(ip, addr_reg, kPageSizeBits - 1, kLowBits);
  ShiftLeftImm(ip, ip, Operand(Bitmap::kBytesPerCellLog2));
  add(bitmap_reg, bitmap_reg, ip);
  li(ip, Operand(1));
  slw(mask_reg, ip, mask_reg);
}


void MacroAssembler::EnsureNotWhite(Register value, Register bitmap_scratch,
                                    Register mask_scratch,
                                    Register load_scratch,
                                    Label* value_is_white_and_not_data) {
  DCHECK(!AreAliased(value, bitmap_scratch, mask_scratch, ip));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  Label done;

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  lwz(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  and_(r0, mask_scratch, load_scratch, SetRC);
  bne(&done, cr0);

  if (emit_debug_code()) {
    // Check for impossible bit pattern.
    Label ok;
    // LSL may overflow, making the check conservative.
    slwi(r0, mask_scratch, Operand(1));
    and_(r0, load_scratch, r0, SetRC);
    beq(&ok, cr0);
    stop("Impossible marking bit pattern");
    bind(&ok);
  }

  // Value is white.  We check whether it is data that doesn't need scanning.
  // Currently only checks for HeapNumber and non-cons strings.
  Register map = load_scratch;     // Holds map while checking type.
  Register length = load_scratch;  // Holds length of object after testing type.
  Label is_data_object, maybe_string_object, is_string_object, is_encoded;
#if V8_TARGET_ARCH_PPC64
  Label length_computed;
#endif


  // Check for heap-number
  LoadP(map, FieldMemOperand(value, HeapObject::kMapOffset));
  CompareRoot(map, Heap::kHeapNumberMapRootIndex);
  bne(&maybe_string_object);
  li(length, Operand(HeapNumber::kSize));
  b(&is_data_object);
  bind(&maybe_string_object);

  // Check for strings.
  DCHECK(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  DCHECK(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  Register instance_type = load_scratch;
  lbz(instance_type, FieldMemOperand(map, Map::kInstanceTypeOffset));
  andi(r0, instance_type, Operand(kIsIndirectStringMask | kIsNotStringMask));
  bne(value_is_white_and_not_data, cr0);
  // It's a non-indirect (non-cons and non-slice) string.
  // If it's external, the length is just ExternalString::kSize.
  // Otherwise it's String::kHeaderSize + string->length() * (1 or 2).
  // External strings are the only ones with the kExternalStringTag bit
  // set.
  DCHECK_EQ(0, kSeqStringTag & kExternalStringTag);
  DCHECK_EQ(0, kConsStringTag & kExternalStringTag);
  andi(r0, instance_type, Operand(kExternalStringTag));
  beq(&is_string_object, cr0);
  li(length, Operand(ExternalString::kSize));
  b(&is_data_object);
  bind(&is_string_object);

  // Sequential string, either Latin1 or UC16.
  // For Latin1 (char-size of 1) we untag the smi to get the length.
  // For UC16 (char-size of 2):
  //   - (32-bit) we just leave the smi tag in place, thereby getting
  //              the length multiplied by 2.
  //   - (64-bit) we compute the offset in the 2-byte array
  DCHECK(kOneByteStringTag == 4 && kStringEncodingMask == 4);
  LoadP(ip, FieldMemOperand(value, String::kLengthOffset));
  andi(r0, instance_type, Operand(kStringEncodingMask));
  beq(&is_encoded, cr0);
  SmiUntag(ip);
#if V8_TARGET_ARCH_PPC64
  b(&length_computed);
#endif
  bind(&is_encoded);
#if V8_TARGET_ARCH_PPC64
  SmiToShortArrayOffset(ip, ip);
  bind(&length_computed);
#else
  DCHECK(kSmiShift == 1);
#endif
  addi(length, ip, Operand(SeqString::kHeaderSize + kObjectAlignmentMask));
  li(r0, Operand(~kObjectAlignmentMask));
  and_(length, length, r0);

  bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  lwz(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  orx(ip, ip, mask_scratch);
  stw(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));

  mov(ip, Operand(~Page::kPageAlignmentMask));
  and_(bitmap_scratch, bitmap_scratch, ip);
  lwz(ip, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));
  add(ip, ip, length);
  stw(ip, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));

  bind(&done);
}


// Saturate a value into 8-bit unsigned integer
//   if input_value < 0, output_value is 0
//   if input_value > 255, output_value is 255
//   otherwise output_value is the input_value
void MacroAssembler::ClampUint8(Register output_reg, Register input_reg) {
  int satval = (1 << 8) - 1;

  if (CpuFeatures::IsSupported(ISELECT)) {
    // set to 0 if negative
    cmpi(input_reg, Operand::Zero());
    isel(lt, output_reg, r0, input_reg);

    // set to satval if > satval
    li(r0, Operand(satval));
    cmpi(output_reg, Operand(satval));
    isel(lt, output_reg, output_reg, r0);
  } else {
    Label done, negative_label, overflow_label;
    cmpi(input_reg, Operand::Zero());
    blt(&negative_label);

    cmpi(input_reg, Operand(satval));
    bgt(&overflow_label);
    if (!output_reg.is(input_reg)) {
      mr(output_reg, input_reg);
    }
    b(&done);

    bind(&negative_label);
    li(output_reg, Operand::Zero());  // set to 0 if negative
    b(&done);

    bind(&overflow_label);  // set to satval if > satval
    li(output_reg, Operand(satval));

    bind(&done);
  }
}


void MacroAssembler::SetRoundingMode(FPRoundingMode RN) { mtfsfi(7, RN); }


void MacroAssembler::ResetRoundingMode() {
  mtfsfi(7, kRoundToNearest);  // reset (default is kRoundToNearest)
}


void MacroAssembler::ClampDoubleToUint8(Register result_reg,
                                        DoubleRegister input_reg,
                                        DoubleRegister double_scratch) {
  Label above_zero;
  Label done;
  Label in_bounds;

  LoadDoubleLiteral(double_scratch, 0.0, result_reg);
  fcmpu(input_reg, double_scratch);
  bgt(&above_zero);

  // Double value is less than zero, NaN or Inf, return 0.
  LoadIntLiteral(result_reg, 0);
  b(&done);

  // Double value is >= 255, return 255.
  bind(&above_zero);
  LoadDoubleLiteral(double_scratch, 255.0, result_reg);
  fcmpu(input_reg, double_scratch);
  ble(&in_bounds);
  LoadIntLiteral(result_reg, 255);
  b(&done);

  // In 0-255 range, round and truncate.
  bind(&in_bounds);

  // round to nearest (default rounding mode)
  fctiw(double_scratch, input_reg);
  MovDoubleLowToInt(result_reg, double_scratch);
  bind(&done);
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  LoadP(descriptors, FieldMemOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  lwz(dst, FieldMemOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  lwz(dst, FieldMemOperand(map, Map::kBitField3Offset));
  ExtractBitMask(dst, dst, Map::EnumLengthBits::kMask);
  SmiTag(dst);
}


void MacroAssembler::LoadAccessor(Register dst, Register holder,
                                  int accessor_index,
                                  AccessorComponent accessor) {
  LoadP(dst, FieldMemOperand(holder, HeapObject::kMapOffset));
  LoadInstanceDescriptors(dst, dst);
  LoadP(dst,
        FieldMemOperand(dst, DescriptorArray::GetValueOffset(accessor_index)));
  const int getterOffset = AccessorPair::kGetterOffset;
  const int setterOffset = AccessorPair::kSetterOffset;
  int offset = ((accessor == ACCESSOR_GETTER) ? getterOffset : setterOffset);
  LoadP(dst, FieldMemOperand(dst, offset));
}


void MacroAssembler::CheckEnumCache(Register null_value, Label* call_runtime) {
  Register empty_fixed_array_value = r9;
  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Label next, start;
  mr(r5, r3);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  LoadP(r4, FieldMemOperand(r5, HeapObject::kMapOffset));

  EnumLength(r6, r4);
  CmpSmiLiteral(r6, Smi::FromInt(kInvalidEnumCacheSentinel), r0);
  beq(call_runtime);

  b(&start);

  bind(&next);
  LoadP(r4, FieldMemOperand(r5, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(r6, r4);
  CmpSmiLiteral(r6, Smi::FromInt(0), r0);
  bne(call_runtime);

  bind(&start);

  // Check that there are no elements. Register r5 contains the current JS
  // object we've reached through the prototype chain.
  Label no_elements;
  LoadP(r5, FieldMemOperand(r5, JSObject::kElementsOffset));
  cmp(r5, empty_fixed_array_value);
  beq(&no_elements);

  // Second chance, the object may be using the empty slow element dictionary.
  CompareRoot(r5, Heap::kEmptySlowElementDictionaryRootIndex);
  bne(call_runtime);

  bind(&no_elements);
  LoadP(r5, FieldMemOperand(r4, Map::kPrototypeOffset));
  cmp(r5, null_value);
  bne(&next);
}


////////////////////////////////////////////////////////////////////////////////
//
// New MacroAssembler Interfaces added for PPC
//
////////////////////////////////////////////////////////////////////////////////
void MacroAssembler::LoadIntLiteral(Register dst, int value) {
  mov(dst, Operand(value));
}


void MacroAssembler::LoadSmiLiteral(Register dst, Smi* smi) {
  mov(dst, Operand(smi));
}


void MacroAssembler::LoadDoubleLiteral(DoubleRegister result, double value,
                                       Register scratch) {
  if (FLAG_enable_embedded_constant_pool && is_constant_pool_available() &&
      !(scratch.is(r0) && ConstantPoolAccessIsInOverflow())) {
    ConstantPoolEntry::Access access = ConstantPoolAddEntry(value);
    if (access == ConstantPoolEntry::OVERFLOWED) {
      addis(scratch, kConstantPoolRegister, Operand::Zero());
      lfd(result, MemOperand(scratch, 0));
    } else {
      lfd(result, MemOperand(kConstantPoolRegister, 0));
    }
    return;
  }

  // avoid gcc strict aliasing error using union cast
  union {
    double dval;
#if V8_TARGET_ARCH_PPC64
    intptr_t ival;
#else
    intptr_t ival[2];
#endif
  } litVal;

  litVal.dval = value;

#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mov(scratch, Operand(litVal.ival));
    mtfprd(result, scratch);
    return;
  }
#endif

  addi(sp, sp, Operand(-kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  mov(scratch, Operand(litVal.ival));
  std(scratch, MemOperand(sp));
#else
  LoadIntLiteral(scratch, litVal.ival[0]);
  stw(scratch, MemOperand(sp, 0));
  LoadIntLiteral(scratch, litVal.ival[1]);
  stw(scratch, MemOperand(sp, 4));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(result, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::MovIntToDouble(DoubleRegister dst, Register src,
                                    Register scratch) {
// sign-extend src to 64-bit
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mtfprwa(dst, src);
    return;
  }
#endif

  DCHECK(!src.is(scratch));
  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  extsw(scratch, src);
  std(scratch, MemOperand(sp, 0));
#else
  srawi(scratch, src, 31);
  stw(scratch, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::MovUnsignedIntToDouble(DoubleRegister dst, Register src,
                                            Register scratch) {
// zero-extend src to 64-bit
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mtfprwz(dst, src);
    return;
  }
#endif

  DCHECK(!src.is(scratch));
  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  clrldi(scratch, src, Operand(32));
  std(scratch, MemOperand(sp, 0));
#else
  li(scratch, Operand::Zero());
  stw(scratch, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::MovInt64ToDouble(DoubleRegister dst,
#if !V8_TARGET_ARCH_PPC64
                                      Register src_hi,
#endif
                                      Register src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mtfprd(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
#if V8_TARGET_ARCH_PPC64
  std(src, MemOperand(sp, 0));
#else
  stw(src_hi, MemOperand(sp, Register::kExponentOffset));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
#endif
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp, 0));
  addi(sp, sp, Operand(kDoubleSize));
}


#if V8_TARGET_ARCH_PPC64
void MacroAssembler::MovInt64ComponentsToDouble(DoubleRegister dst,
                                                Register src_hi,
                                                Register src_lo,
                                                Register scratch) {
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    sldi(scratch, src_hi, Operand(32));
    rldimi(scratch, src_lo, 0, 32);
    mtfprd(dst, scratch);
    return;
  }

  subi(sp, sp, Operand(kDoubleSize));
  stw(src_hi, MemOperand(sp, Register::kExponentOffset));
  stw(src_lo, MemOperand(sp, Register::kMantissaOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}
#endif


void MacroAssembler::InsertDoubleLow(DoubleRegister dst, Register src,
                                     Register scratch) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(scratch, dst);
    rldimi(scratch, src, 0, 32);
    mtfprd(dst, scratch);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(dst, MemOperand(sp));
  stw(src, MemOperand(sp, Register::kMantissaOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::InsertDoubleHigh(DoubleRegister dst, Register src,
                                      Register scratch) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(scratch, dst);
    rldimi(scratch, src, 32, 0);
    mtfprd(dst, scratch);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(dst, MemOperand(sp));
  stw(src, MemOperand(sp, Register::kExponentOffset));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lfd(dst, MemOperand(sp));
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::MovDoubleLowToInt(Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprwz(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, Register::kMantissaOffset));
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::MovDoubleHighToInt(Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(dst, src);
    srdi(dst, dst, Operand(32));
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
  lwz(dst, MemOperand(sp, Register::kExponentOffset));
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::MovDoubleToInt64(
#if !V8_TARGET_ARCH_PPC64
    Register dst_hi,
#endif
    Register dst, DoubleRegister src) {
#if V8_TARGET_ARCH_PPC64
  if (CpuFeatures::IsSupported(FPR_GPR_MOV)) {
    mffprd(dst, src);
    return;
  }
#endif

  subi(sp, sp, Operand(kDoubleSize));
  stfd(src, MemOperand(sp));
  nop(GROUP_ENDING_NOP);  // LHS/RAW optimization
#if V8_TARGET_ARCH_PPC64
  ld(dst, MemOperand(sp, 0));
#else
  lwz(dst_hi, MemOperand(sp, Register::kExponentOffset));
  lwz(dst, MemOperand(sp, Register::kMantissaOffset));
#endif
  addi(sp, sp, Operand(kDoubleSize));
}


void MacroAssembler::Add(Register dst, Register src, intptr_t value,
                         Register scratch) {
  if (is_int16(value)) {
    addi(dst, src, Operand(value));
  } else {
    mov(scratch, Operand(value));
    add(dst, src, scratch);
  }
}


void MacroAssembler::Cmpi(Register src1, const Operand& src2, Register scratch,
                          CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_int16(value)) {
    cmpi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmp(src1, scratch, cr);
  }
}


void MacroAssembler::Cmpli(Register src1, const Operand& src2, Register scratch,
                           CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_uint16(value)) {
    cmpli(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmpl(src1, scratch, cr);
  }
}


void MacroAssembler::Cmpwi(Register src1, const Operand& src2, Register scratch,
                           CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_int16(value)) {
    cmpwi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmpw(src1, scratch, cr);
  }
}


void MacroAssembler::Cmplwi(Register src1, const Operand& src2,
                            Register scratch, CRegister cr) {
  intptr_t value = src2.immediate();
  if (is_uint16(value)) {
    cmplwi(src1, src2, cr);
  } else {
    mov(scratch, src2);
    cmplw(src1, scratch, cr);
  }
}


void MacroAssembler::And(Register ra, Register rs, const Operand& rb,
                         RCBit rc) {
  if (rb.is_reg()) {
    and_(ra, rs, rb.rm(), rc);
  } else {
    if (is_uint16(rb.imm_) && RelocInfo::IsNone(rb.rmode_) && rc == SetRC) {
      andi(ra, rs, rb);
    } else {
      // mov handles the relocation.
      DCHECK(!rs.is(r0));
      mov(r0, rb);
      and_(ra, rs, r0, rc);
    }
  }
}


void MacroAssembler::Or(Register ra, Register rs, const Operand& rb, RCBit rc) {
  if (rb.is_reg()) {
    orx(ra, rs, rb.rm(), rc);
  } else {
    if (is_uint16(rb.imm_) && RelocInfo::IsNone(rb.rmode_) && rc == LeaveRC) {
      ori(ra, rs, rb);
    } else {
      // mov handles the relocation.
      DCHECK(!rs.is(r0));
      mov(r0, rb);
      orx(ra, rs, r0, rc);
    }
  }
}


void MacroAssembler::Xor(Register ra, Register rs, const Operand& rb,
                         RCBit rc) {
  if (rb.is_reg()) {
    xor_(ra, rs, rb.rm(), rc);
  } else {
    if (is_uint16(rb.imm_) && RelocInfo::IsNone(rb.rmode_) && rc == LeaveRC) {
      xori(ra, rs, rb);
    } else {
      // mov handles the relocation.
      DCHECK(!rs.is(r0));
      mov(r0, rb);
      xor_(ra, rs, r0, rc);
    }
  }
}


void MacroAssembler::CmpSmiLiteral(Register src1, Smi* smi, Register scratch,
                                   CRegister cr) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  cmp(src1, scratch, cr);
#else
  Cmpi(src1, Operand(smi), scratch, cr);
#endif
}


void MacroAssembler::CmplSmiLiteral(Register src1, Smi* smi, Register scratch,
                                    CRegister cr) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  cmpl(src1, scratch, cr);
#else
  Cmpli(src1, Operand(smi), scratch, cr);
#endif
}


void MacroAssembler::AddSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  add(dst, src, scratch);
#else
  Add(dst, src, reinterpret_cast<intptr_t>(smi), scratch);
#endif
}


void MacroAssembler::SubSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  sub(dst, src, scratch);
#else
  Add(dst, src, -(reinterpret_cast<intptr_t>(smi)), scratch);
#endif
}


void MacroAssembler::AndSmiLiteral(Register dst, Register src, Smi* smi,
                                   Register scratch, RCBit rc) {
#if V8_TARGET_ARCH_PPC64
  LoadSmiLiteral(scratch, smi);
  and_(dst, src, scratch, rc);
#else
  And(dst, src, Operand(smi), rc);
#endif
}


// Load a "pointer" sized value from the memory location
void MacroAssembler::LoadP(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    /* cannot use d-form */
    DCHECK(!scratch.is(no_reg));
    mov(scratch, Operand(offset));
#if V8_TARGET_ARCH_PPC64
    ldx(dst, MemOperand(mem.ra(), scratch));
#else
    lwzx(dst, MemOperand(mem.ra(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_PPC64
    int misaligned = (offset & 3);
    if (misaligned) {
      // adjust base to conform to offset alignment requirements
      // Todo: enhance to use scratch if dst is unsuitable
      DCHECK(!dst.is(r0));
      addi(dst, mem.ra(), Operand((offset & 3) - 4));
      ld(dst, MemOperand(dst, (offset & ~3) + 4));
    } else {
      ld(dst, mem);
    }
#else
    lwz(dst, mem);
#endif
  }
}


// Store a "pointer" sized value to the memory location
void MacroAssembler::StoreP(Register src, const MemOperand& mem,
                            Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    /* cannot use d-form */
    DCHECK(!scratch.is(no_reg));
    mov(scratch, Operand(offset));
#if V8_TARGET_ARCH_PPC64
    stdx(src, MemOperand(mem.ra(), scratch));
#else
    stwx(src, MemOperand(mem.ra(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_PPC64
    int misaligned = (offset & 3);
    if (misaligned) {
      // adjust base to conform to offset alignment requirements
      // a suitable scratch is required here
      DCHECK(!scratch.is(no_reg));
      if (scratch.is(r0)) {
        LoadIntLiteral(scratch, offset);
        stdx(src, MemOperand(mem.ra(), scratch));
      } else {
        addi(scratch, mem.ra(), Operand((offset & 3) - 4));
        std(src, MemOperand(scratch, (offset & ~3) + 4));
      }
    } else {
      std(src, mem);
    }
#else
    stw(src, mem);
#endif
  }
}

void MacroAssembler::LoadWordArith(Register dst, const MemOperand& mem,
                                   Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    DCHECK(!scratch.is(no_reg));
    mov(scratch, Operand(offset));
    lwax(dst, MemOperand(mem.ra(), scratch));
  } else {
#if V8_TARGET_ARCH_PPC64
    int misaligned = (offset & 3);
    if (misaligned) {
      // adjust base to conform to offset alignment requirements
      // Todo: enhance to use scratch if dst is unsuitable
      DCHECK(!dst.is(r0));
      addi(dst, mem.ra(), Operand((offset & 3) - 4));
      lwa(dst, MemOperand(dst, (offset & ~3) + 4));
    } else {
      lwa(dst, mem);
    }
#else
    lwz(dst, mem);
#endif
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand currently only supports d-form
void MacroAssembler::LoadWord(Register dst, const MemOperand& mem,
                              Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    lwzx(dst, MemOperand(base, scratch));
  } else {
    lwz(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreWord(Register src, const MemOperand& mem,
                               Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    stwx(src, MemOperand(base, scratch));
  } else {
    stw(src, mem);
  }
}


void MacroAssembler::LoadHalfWordArith(Register dst, const MemOperand& mem,
                                       Register scratch) {
  int offset = mem.offset();

  if (!is_int16(offset)) {
    DCHECK(!scratch.is(no_reg));
    mov(scratch, Operand(offset));
    lhax(dst, MemOperand(mem.ra(), scratch));
  } else {
    lha(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand currently only supports d-form
void MacroAssembler::LoadHalfWord(Register dst, const MemOperand& mem,
                                  Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    lhzx(dst, MemOperand(base, scratch));
  } else {
    lhz(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreHalfWord(Register src, const MemOperand& mem,
                                   Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    sthx(src, MemOperand(base, scratch));
  } else {
    sth(src, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand currently only supports d-form
void MacroAssembler::LoadByte(Register dst, const MemOperand& mem,
                              Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    lbzx(dst, MemOperand(base, scratch));
  } else {
    lbz(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreByte(Register src, const MemOperand& mem,
                               Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    LoadIntLiteral(scratch, offset);
    stbx(src, MemOperand(base, scratch));
  } else {
    stb(src, mem);
  }
}


void MacroAssembler::LoadRepresentation(Register dst, const MemOperand& mem,
                                        Representation r, Register scratch) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8()) {
    LoadByte(dst, mem, scratch);
    extsb(dst, dst);
  } else if (r.IsUInteger8()) {
    LoadByte(dst, mem, scratch);
  } else if (r.IsInteger16()) {
    LoadHalfWordArith(dst, mem, scratch);
  } else if (r.IsUInteger16()) {
    LoadHalfWord(dst, mem, scratch);
#if V8_TARGET_ARCH_PPC64
  } else if (r.IsInteger32()) {
    LoadWordArith(dst, mem, scratch);
#endif
  } else {
    LoadP(dst, mem, scratch);
  }
}


void MacroAssembler::StoreRepresentation(Register src, const MemOperand& mem,
                                         Representation r, Register scratch) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8() || r.IsUInteger8()) {
    StoreByte(src, mem, scratch);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    StoreHalfWord(src, mem, scratch);
#if V8_TARGET_ARCH_PPC64
  } else if (r.IsInteger32()) {
    StoreWord(src, mem, scratch);
#endif
  } else {
    if (r.IsHeapObject()) {
      AssertNotSmi(src);
    } else if (r.IsSmi()) {
      AssertSmi(src);
    }
    StoreP(src, mem, scratch);
  }
}


void MacroAssembler::LoadDouble(DoubleRegister dst, const MemOperand& mem,
                                Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    lfdx(dst, MemOperand(base, scratch));
  } else {
    lfd(dst, mem);
  }
}


void MacroAssembler::StoreDouble(DoubleRegister src, const MemOperand& mem,
                                 Register scratch) {
  Register base = mem.ra();
  int offset = mem.offset();

  if (!is_int16(offset)) {
    mov(scratch, Operand(offset));
    stfdx(src, MemOperand(base, scratch));
  } else {
    stfd(src, mem);
  }
}


void MacroAssembler::TestJSArrayForAllocationMemento(Register receiver_reg,
                                                     Register scratch_reg,
                                                     Label* no_memento_found) {
  ExternalReference new_space_start =
      ExternalReference::new_space_start(isolate());
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  addi(scratch_reg, receiver_reg,
       Operand(JSArray::kSize + AllocationMemento::kSize - kHeapObjectTag));
  Cmpi(scratch_reg, Operand(new_space_start), r0);
  blt(no_memento_found);
  mov(ip, Operand(new_space_allocation_top));
  LoadP(ip, MemOperand(ip));
  cmp(scratch_reg, ip);
  bgt(no_memento_found);
  LoadP(scratch_reg, MemOperand(scratch_reg, -AllocationMemento::kSize));
  Cmpi(scratch_reg, Operand(isolate()->factory()->allocation_memento_map()),
       r0);
}


Register GetRegisterThatIsNotOneOf(Register reg1, Register reg2, Register reg3,
                                   Register reg4, Register reg5,
                                   Register reg6) {
  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();

  for (int i = 0; i < Register::NumAllocatableRegisters(); i++) {
    Register candidate = Register::FromAllocationIndex(i);
    if (regs & candidate.bit()) continue;
    return candidate;
  }
  UNREACHABLE();
  return no_reg;
}


void MacroAssembler::JumpIfDictionaryInPrototypeChain(Register object,
                                                      Register scratch0,
                                                      Register scratch1,
                                                      Label* found) {
  DCHECK(!scratch1.is(scratch0));
  Factory* factory = isolate()->factory();
  Register current = scratch0;
  Label loop_again;

  // scratch contained elements pointer.
  mr(current, object);

  // Loop based on the map going up the prototype chain.
  bind(&loop_again);
  LoadP(current, FieldMemOperand(current, HeapObject::kMapOffset));
  lbz(scratch1, FieldMemOperand(current, Map::kBitField2Offset));
  DecodeField<Map::ElementsKindBits>(scratch1);
  cmpi(scratch1, Operand(DICTIONARY_ELEMENTS));
  beq(found);
  LoadP(current, FieldMemOperand(current, Map::kPrototypeOffset));
  Cmpi(current, Operand(factory->null_value()), r0);
  bne(&loop_again);
}


#ifdef DEBUG
bool AreAliased(Register reg1, Register reg2, Register reg3, Register reg4,
                Register reg5, Register reg6, Register reg7, Register reg8) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() + reg3.is_valid() +
                        reg4.is_valid() + reg5.is_valid() + reg6.is_valid() +
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


CodePatcher::CodePatcher(byte* address, int instructions,
                         FlushICache flush_cache)
    : address_(address),
      size_(instructions * Assembler::kInstrSize),
      masm_(NULL, address, size_ + Assembler::kGap),
      flush_cache_(flush_cache) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  if (flush_cache_ == FLUSH) {
    CpuFeatures::FlushICache(address_, size_);
  }

  // Check that the code was patched as expected.
  DCHECK(masm_.pc_ == address_ + size_);
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


void CodePatcher::Emit(Instr instr) { masm()->emit(instr); }


void CodePatcher::EmitCondition(Condition cond) {
  Instr instr = Assembler::instr_at(masm_.pc_);
  switch (cond) {
    case eq:
      instr = (instr & ~kCondMask) | BT;
      break;
    case ne:
      instr = (instr & ~kCondMask) | BF;
      break;
    default:
      UNIMPLEMENTED();
  }
  masm_.emit(instr);
}


void MacroAssembler::TruncatingDiv(Register result, Register dividend,
                                   int32_t divisor) {
  DCHECK(!dividend.is(result));
  DCHECK(!dividend.is(r0));
  DCHECK(!result.is(r0));
  base::MagicNumbersForDivision<uint32_t> mag =
      base::SignedDivisionByConstant(static_cast<uint32_t>(divisor));
  mov(r0, Operand(mag.multiplier));
  mulhw(result, dividend, r0);
  bool neg = (mag.multiplier & (static_cast<uint32_t>(1) << 31)) != 0;
  if (divisor > 0 && neg) {
    add(result, result, dividend);
  }
  if (divisor < 0 && !neg && mag.multiplier > 0) {
    sub(result, result, dividend);
  }
  if (mag.shift > 0) srawi(result, result, mag.shift);
  ExtractBit(r0, dividend, 31);
  add(result, result, r0);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC

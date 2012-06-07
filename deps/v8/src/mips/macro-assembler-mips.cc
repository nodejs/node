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

#include <limits.h>  // For LONG_MIN, LONG_MAX.

#include "v8.h"

#if defined(V8_TARGET_ARCH_MIPS)

#include "bootstrapper.h"
#include "codegen.h"
#include "debug.h"
#include "runtime.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      allow_stub_calls_(true),
      has_frame_(false) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index) {
  lw(destination, MemOperand(s6, index << kPointerSizeLog2));
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index,
                              Condition cond,
                              Register src1, const Operand& src2) {
  Branch(2, NegateCondition(cond), src1, src2);
  lw(destination, MemOperand(s6, index << kPointerSizeLog2));
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index) {
  sw(source, MemOperand(s6, index << kPointerSizeLog2));
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index,
                               Condition cond,
                               Register src1, const Operand& src2) {
  Branch(2, NegateCondition(cond), src1, src2);
  sw(source, MemOperand(s6, index << kPointerSizeLog2));
}


void MacroAssembler::LoadHeapObject(Register result,
                                    Handle<HeapObject> object) {
  if (isolate()->heap()->InNewSpace(*object)) {
    Handle<JSGlobalPropertyCell> cell =
        isolate()->factory()->NewJSGlobalPropertyCell(object);
    li(result, Operand(cell));
    lw(result, FieldMemOperand(result, JSGlobalPropertyCell::kValueOffset));
  } else {
    li(result, Operand(object));
  }
}


// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  ASSERT(num_unsaved >= 0);
  if (num_unsaved > 0) {
    Subu(sp, sp, Operand(num_unsaved * kPointerSize));
  }
  MultiPush(kSafepointSavedRegisters);
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  MultiPop(kSafepointSavedRegisters);
  if (num_unsaved > 0) {
    Addu(sp, sp, Operand(num_unsaved * kPointerSize));
  }
}


void MacroAssembler::PushSafepointRegistersAndDoubles() {
  PushSafepointRegisters();
  Subu(sp, sp, Operand(FPURegister::kNumAllocatableRegisters * kDoubleSize));
  for (int i = 0; i < FPURegister::kNumAllocatableRegisters; i+=2) {
    FPURegister reg = FPURegister::FromAllocationIndex(i);
    sdc1(reg, MemOperand(sp, i * kDoubleSize));
  }
}


void MacroAssembler::PopSafepointRegistersAndDoubles() {
  for (int i = 0; i < FPURegister::kNumAllocatableRegisters; i+=2) {
    FPURegister reg = FPURegister::FromAllocationIndex(i);
    ldc1(reg, MemOperand(sp, i * kDoubleSize));
  }
  Addu(sp, sp, Operand(FPURegister::kNumAllocatableRegisters * kDoubleSize));
  PopSafepointRegisters();
}


void MacroAssembler::StoreToSafepointRegistersAndDoublesSlot(Register src,
                                                             Register dst) {
  sw(src, SafepointRegistersAndDoublesSlot(dst));
}


void MacroAssembler::StoreToSafepointRegisterSlot(Register src, Register dst) {
  sw(src, SafepointRegisterSlot(dst));
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  lw(dst, SafepointRegisterSlot(src));
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the highest encoding,
  // which means that lowest encodings are closest to the stack pointer.
  return kSafepointRegisterStackIndexMap[reg_code];
}


MemOperand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return MemOperand(sp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


MemOperand MacroAssembler::SafepointRegistersAndDoublesSlot(Register reg) {
  UNIMPLEMENTED_MIPS();
  // General purpose registers are pushed last on the stack.
  int doubles_size = FPURegister::kNumAllocatableRegisters * kDoubleSize;
  int register_offset = SafepointRegisterStackIndex(reg.code()) * kPointerSize;
  return MemOperand(sp, doubles_size + register_offset);
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cc,
                                Label* branch) {
  ASSERT(cc == eq || cc == ne);
  And(scratch, object, Operand(ExternalReference::new_space_mask(isolate())));
  Branch(branch, cc, scratch,
         Operand(ExternalReference::new_space_start(isolate())));
}


void MacroAssembler::RecordWriteField(
    Register object,
    int offset,
    Register value,
    Register dst,
    RAStatus ra_status,
    SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check) {
  ASSERT(!AreAliased(value, dst, t8, object));
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

  Addu(dst, object, Operand(offset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    And(t8, dst, Operand((1 << kPointerSizeLog2) - 1));
    Branch(&ok, eq, t8, Operand(zero_reg));
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  RecordWrite(object,
              dst,
              value,
              ra_status,
              save_fp,
              remembered_set_action,
              OMIT_SMI_CHECK);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    li(value, Operand(BitCast<int32_t>(kZapValue + 4)));
    li(dst, Operand(BitCast<int32_t>(kZapValue + 8)));
  }
}


// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(Register object,
                                 Register address,
                                 Register value,
                                 RAStatus ra_status,
                                 SaveFPRegsMode fp_mode,
                                 RememberedSetAction remembered_set_action,
                                 SmiCheck smi_check) {
  ASSERT(!AreAliased(object, address, value, t8));
  ASSERT(!AreAliased(object, address, value, t9));
  // The compiled code assumes that record write doesn't change the
  // context register, so we check that none of the clobbered
  // registers are cp.
  ASSERT(!address.is(cp) && !value.is(cp));

  if (emit_debug_code()) {
    lw(at, MemOperand(address));
    Assert(
        eq, "Wrong address or value passed to RecordWrite", at, Operand(value));
  }

  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    ASSERT_EQ(0, kSmiTag);
    JumpIfSmi(value, &done);
  }

  CheckPageFlag(value,
                value,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask,
                eq,
                &done);
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                eq,
                &done);

  // Record the actual write.
  if (ra_status == kRAHasNotBeenSaved) {
    push(ra);
  }
  RecordWriteStub stub(object, value, address, remembered_set_action, fp_mode);
  CallStub(&stub);
  if (ra_status == kRAHasNotBeenSaved) {
    pop(ra);
  }

  bind(&done);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    li(address, Operand(BitCast<int32_t>(kZapValue + 12)));
    li(value, Operand(BitCast<int32_t>(kZapValue + 16)));
  }
}


void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register address,
                                         Register scratch,
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
  li(t8, Operand(store_buffer));
  lw(scratch, MemOperand(t8));
  // Store pointer to buffer and increment buffer top.
  sw(address, MemOperand(scratch));
  Addu(scratch, scratch, kPointerSize);
  // Write back new top of buffer.
  sw(scratch, MemOperand(t8));
  // Call stub on end of buffer.
  // Check for end of buffer.
  And(t8, scratch, Operand(StoreBuffer::kStoreBufferOverflowBit));
  if (and_then == kFallThroughAtEnd) {
    Branch(&done, eq, t8, Operand(zero_reg));
  } else {
    ASSERT(and_then == kReturnAtEnd);
    Ret(eq, t8, Operand(zero_reg));
  }
  push(ra);
  StoreBufferOverflowStub store_buffer_overflow =
      StoreBufferOverflowStub(fp_mode);
  CallStub(&store_buffer_overflow);
  pop(ra);
  bind(&done);
  if (and_then == kReturnAtEnd) {
    Ret();
  }
}


// -----------------------------------------------------------------------------
// Allocation support.


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  ASSERT(!holder_reg.is(scratch));
  ASSERT(!holder_reg.is(at));
  ASSERT(!scratch.is(at));

  // Load current lexical context from the stack frame.
  lw(scratch, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  Check(ne, "we should not have an empty lexical context",
      scratch, Operand(zero_reg));
#endif

  // Load the global context of the current context.
  int offset = Context::kHeaderSize + Context::GLOBAL_INDEX * kPointerSize;
  lw(scratch, FieldMemOperand(scratch, offset));
  lw(scratch, FieldMemOperand(scratch, GlobalObject::kGlobalContextOffset));

  // Check the context is a global context.
  if (emit_debug_code()) {
    // TODO(119): Avoid push(holder_reg)/pop(holder_reg).
    push(holder_reg);  // Temporarily save holder on the stack.
    // Read the first word and compare to the global_context_map.
    lw(holder_reg, FieldMemOperand(scratch, HeapObject::kMapOffset));
    LoadRoot(at, Heap::kGlobalContextMapRootIndex);
    Check(eq, "JSGlobalObject::global_context should be a global context.",
          holder_reg, Operand(at));
    pop(holder_reg);  // Restore holder.
  }

  // Check if both contexts are the same.
  lw(at, FieldMemOperand(holder_reg, JSGlobalProxy::kContextOffset));
  Branch(&same_contexts, eq, scratch, Operand(at));

  // Check the context is a global context.
  if (emit_debug_code()) {
    // TODO(119): Avoid push(holder_reg)/pop(holder_reg).
    push(holder_reg);  // Temporarily save holder on the stack.
    mov(holder_reg, at);  // Move at to its holding place.
    LoadRoot(at, Heap::kNullValueRootIndex);
    Check(ne, "JSGlobalProxy::context() should not be null.",
          holder_reg, Operand(at));

    lw(holder_reg, FieldMemOperand(holder_reg, HeapObject::kMapOffset));
    LoadRoot(at, Heap::kGlobalContextMapRootIndex);
    Check(eq, "JSGlobalObject::global_context should be a global context.",
          holder_reg, Operand(at));
    // Restore at is not needed. at is reloaded below.
    pop(holder_reg);  // Restore holder.
    // Restore at to holder's context.
    lw(at, FieldMemOperand(holder_reg, JSGlobalProxy::kContextOffset));
  }

  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;

  lw(scratch, FieldMemOperand(scratch, token_offset));
  lw(at, FieldMemOperand(at, token_offset));
  Branch(miss, ne, scratch, Operand(at));

  bind(&same_contexts);
}


void MacroAssembler::GetNumberHash(Register reg0, Register scratch) {
  // First of all we assign the hash seed to scratch.
  LoadRoot(scratch, Heap::kHashSeedRootIndex);
  SmiUntag(scratch);

  // Xor original key with a seed.
  xor_(reg0, reg0, scratch);

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  nor(scratch, reg0, zero_reg);
  sll(at, reg0, 15);
  addu(reg0, scratch, at);

  // hash = hash ^ (hash >> 12);
  srl(at, reg0, 12);
  xor_(reg0, reg0, at);

  // hash = hash + (hash << 2);
  sll(at, reg0, 2);
  addu(reg0, reg0, at);

  // hash = hash ^ (hash >> 4);
  srl(at, reg0, 4);
  xor_(reg0, reg0, at);

  // hash = hash * 2057;
  sll(scratch, reg0, 11);
  sll(at, reg0, 3);
  addu(reg0, reg0, at);
  addu(reg0, reg0, scratch);

  // hash = hash ^ (hash >> 16);
  srl(at, reg0, 16);
  xor_(reg0, reg0, at);
}


void MacroAssembler::LoadFromNumberDictionary(Label* miss,
                                              Register elements,
                                              Register key,
                                              Register result,
                                              Register reg0,
                                              Register reg1,
                                              Register reg2) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the same as 'key' or 'result'.
  //            Unchanged on bailout so 'key' or 'result' can be used
  //            in further computation.
  //
  // Scratch registers:
  //
  // reg0 - holds the untagged key on entry and holds the hash once computed.
  //
  // reg1 - Used to hold the capacity mask of the dictionary.
  //
  // reg2 - Used for the index into the dictionary.
  // at   - Temporary (avoid MacroAssembler instructions also using 'at').
  Label done;

  GetNumberHash(reg0, reg1);

  // Compute the capacity mask.
  lw(reg1, FieldMemOperand(elements, SeededNumberDictionary::kCapacityOffset));
  sra(reg1, reg1, kSmiTagSize);
  Subu(reg1, reg1, Operand(1));

  // Generate an unrolled loop that performs a few probes before giving up.
  static const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Use reg2 for index calculations and keep the hash intact in reg0.
    mov(reg2, reg0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      Addu(reg2, reg2, Operand(SeededNumberDictionary::GetProbeOffset(i)));
    }
    and_(reg2, reg2, reg1);

    // Scale the index by multiplying by the element size.
    ASSERT(SeededNumberDictionary::kEntrySize == 3);
    sll(at, reg2, 1);  // 2x.
    addu(reg2, reg2, at);  // reg2 = reg2 * 3.

    // Check if the key is identical to the name.
    sll(at, reg2, kPointerSizeLog2);
    addu(reg2, elements, at);

    lw(at, FieldMemOperand(reg2, SeededNumberDictionary::kElementsStartOffset));
    if (i != kProbes - 1) {
      Branch(&done, eq, key, Operand(at));
    } else {
      Branch(miss, ne, key, Operand(at));
    }
  }

  bind(&done);
  // Check that the value is a normal property.
  // reg2: elements + (index * kPointerSize).
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  lw(reg1, FieldMemOperand(reg2, kDetailsOffset));
  And(at, reg1, Operand(Smi::FromInt(PropertyDetails::TypeField::kMask)));
  Branch(miss, ne, at, Operand(zero_reg));

  // Get the value at the masked, scaled index and return.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  lw(result, FieldMemOperand(reg2, kValueOffset));
}


// ---------------------------------------------------------------------------
// Instruction macros.

void MacroAssembler::Addu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    addu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      addiu(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      addu(rd, rs, at);
    }
  }
}


void MacroAssembler::Subu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    subu(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      addiu(rd, rs, -rt.imm32_);  // No subiu instr, use addiu(x, y, -imm).
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      subu(rd, rs, at);
    }
  }
}


void MacroAssembler::Mul(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    if (kArchVariant == kLoongson) {
      mult(rs, rt.rm());
      mflo(rd);
    } else {
      mul(rd, rs, rt.rm());
    }
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    if (kArchVariant == kLoongson) {
      mult(rs, at);
      mflo(rd);
    } else {
      mul(rd, rs, at);
    }
  }
}


void MacroAssembler::Mult(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    mult(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    mult(rs, at);
  }
}


void MacroAssembler::Multu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    multu(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    multu(rs, at);
  }
}


void MacroAssembler::Div(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    div(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    div(rs, at);
  }
}


void MacroAssembler::Divu(Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    divu(rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    divu(rs, at);
  }
}


void MacroAssembler::And(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    and_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      andi(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      and_(rd, rs, at);
    }
  }
}


void MacroAssembler::Or(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    or_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      ori(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      or_(rd, rs, at);
    }
  }
}


void MacroAssembler::Xor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    xor_(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      xori(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      xor_(rd, rs, at);
    }
  }
}


void MacroAssembler::Nor(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    nor(rd, rs, rt.rm());
  } else {
    // li handles the relocation.
    ASSERT(!rs.is(at));
    li(at, rt);
    nor(rd, rs, at);
  }
}


void MacroAssembler::Neg(Register rs, const Operand& rt) {
  ASSERT(rt.is_reg());
  ASSERT(!at.is(rs));
  ASSERT(!at.is(rt.rm()));
  li(at, -1);
  xor_(rs, rt.rm(), at);
}


void MacroAssembler::Slt(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    slt(rd, rs, rt.rm());
  } else {
    if (is_int16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      slti(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      slt(rd, rs, at);
    }
  }
}


void MacroAssembler::Sltu(Register rd, Register rs, const Operand& rt) {
  if (rt.is_reg()) {
    sltu(rd, rs, rt.rm());
  } else {
    if (is_uint16(rt.imm32_) && !MustUseReg(rt.rmode_)) {
      sltiu(rd, rs, rt.imm32_);
    } else {
      // li handles the relocation.
      ASSERT(!rs.is(at));
      li(at, rt);
      sltu(rd, rs, at);
    }
  }
}


void MacroAssembler::Ror(Register rd, Register rs, const Operand& rt) {
  if (kArchVariant == kMips32r2) {
    if (rt.is_reg()) {
      rotrv(rd, rs, rt.rm());
    } else {
      rotr(rd, rs, rt.imm32_);
    }
  } else {
    if (rt.is_reg()) {
      subu(at, zero_reg, rt.rm());
      sllv(at, rs, at);
      srlv(rd, rs, rt.rm());
      or_(rd, rd, at);
    } else {
      if (rt.imm32_ == 0) {
        srl(rd, rs, 0);
      } else {
        srl(at, rs, rt.imm32_);
        sll(rd, rs, (0x20 - rt.imm32_) & 0x1f);
        or_(rd, rd, at);
      }
    }
  }
}

//------------Pseudo-instructions-------------

void MacroAssembler::li(Register rd, Operand j, LiFlags mode) {
  ASSERT(!j.is_reg());
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (!MustUseReg(j.rmode_) && mode == OPTIMIZE_SIZE) {
    // Normal load of an immediate value which does not need Relocation Info.
    if (is_int16(j.imm32_)) {
      addiu(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & kHiMask)) {
      ori(rd, zero_reg, j.imm32_);
    } else if (!(j.imm32_ & kImm16Mask)) {
      lui(rd, (j.imm32_ >> kLuiShift) & kImm16Mask);
    } else {
      lui(rd, (j.imm32_ >> kLuiShift) & kImm16Mask);
      ori(rd, rd, (j.imm32_ & kImm16Mask));
    }
  } else {
    if (MustUseReg(j.rmode_)) {
      RecordRelocInfo(j.rmode_, j.imm32_);
    }
    // We always need the same number of instructions as we may need to patch
    // this code to load another value which may need 2 instructions to load.
    lui(rd, (j.imm32_ >> kLuiShift) & kImm16Mask);
    ori(rd, rd, (j.imm32_ & kImm16Mask));
  }
}


void MacroAssembler::MultiPush(RegList regs) {
  int16_t num_to_push = NumberOfBitsSet(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  Subu(sp, sp, Operand(stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      sw(ToRegister(i), MemOperand(sp, stack_offset));
    }
  }
}


void MacroAssembler::MultiPushReversed(RegList regs) {
  int16_t num_to_push = NumberOfBitsSet(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  Subu(sp, sp, Operand(stack_offset));
  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      sw(ToRegister(i), MemOperand(sp, stack_offset));
    }
  }
}


void MacroAssembler::MultiPop(RegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      lw(ToRegister(i), MemOperand(sp, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  addiu(sp, sp, stack_offset);
}


void MacroAssembler::MultiPopReversed(RegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      lw(ToRegister(i), MemOperand(sp, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  addiu(sp, sp, stack_offset);
}


void MacroAssembler::MultiPushFPU(RegList regs) {
  CpuFeatures::Scope scope(FPU);
  int16_t num_to_push = NumberOfBitsSet(regs);
  int16_t stack_offset = num_to_push * kDoubleSize;

  Subu(sp, sp, Operand(stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kDoubleSize;
      sdc1(FPURegister::from_code(i), MemOperand(sp, stack_offset));
    }
  }
}


void MacroAssembler::MultiPushReversedFPU(RegList regs) {
  CpuFeatures::Scope scope(FPU);
  int16_t num_to_push = NumberOfBitsSet(regs);
  int16_t stack_offset = num_to_push * kDoubleSize;

  Subu(sp, sp, Operand(stack_offset));
  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kDoubleSize;
      sdc1(FPURegister::from_code(i), MemOperand(sp, stack_offset));
    }
  }
}


void MacroAssembler::MultiPopFPU(RegList regs) {
  CpuFeatures::Scope scope(FPU);
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      ldc1(FPURegister::from_code(i), MemOperand(sp, stack_offset));
      stack_offset += kDoubleSize;
    }
  }
  addiu(sp, sp, stack_offset);
}


void MacroAssembler::MultiPopReversedFPU(RegList regs) {
  CpuFeatures::Scope scope(FPU);
  int16_t stack_offset = 0;

  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      ldc1(FPURegister::from_code(i), MemOperand(sp, stack_offset));
      stack_offset += kDoubleSize;
    }
  }
  addiu(sp, sp, stack_offset);
}


void MacroAssembler::FlushICache(Register address, unsigned instructions) {
  RegList saved_regs = kJSCallerSaved | ra.bit();
  MultiPush(saved_regs);
  AllowExternalCallThatCantCauseGC scope(this);

  // Save to a0 in case address == t0.
  Move(a0, address);
  PrepareCallCFunction(2, t0);

  li(a1, instructions * kInstrSize);
  CallCFunction(ExternalReference::flush_icache_function(isolate()), 2);
  MultiPop(saved_regs);
}


void MacroAssembler::Ext(Register rt,
                         Register rs,
                         uint16_t pos,
                         uint16_t size) {
  ASSERT(pos < 32);
  ASSERT(pos + size < 33);

  if (kArchVariant == kMips32r2) {
    ext_(rt, rs, pos, size);
  } else {
    // Move rs to rt and shift it left then right to get the
    // desired bitfield on the right side and zeroes on the left.
    int shift_left = 32 - (pos + size);
    sll(rt, rs, shift_left);  // Acts as a move if shift_left == 0.

    int shift_right = 32 - size;
    if (shift_right > 0) {
      srl(rt, rt, shift_right);
    }
  }
}


void MacroAssembler::Ins(Register rt,
                         Register rs,
                         uint16_t pos,
                         uint16_t size) {
  ASSERT(pos < 32);
  ASSERT(pos + size <= 32);
  ASSERT(size != 0);

  if (kArchVariant == kMips32r2) {
    ins_(rt, rs, pos, size);
  } else {
    ASSERT(!rt.is(t8) && !rs.is(t8));
    Subu(at, zero_reg, Operand(1));
    srl(at, at, 32 - size);
    and_(t8, rs, at);
    sll(t8, t8, pos);
    sll(at, at, pos);
    nor(at, at, zero_reg);
    and_(at, rt, at);
    or_(rt, t8, at);
  }
}


void MacroAssembler::Cvt_d_uw(FPURegister fd,
                              FPURegister fs,
                              FPURegister scratch) {
  // Move the data from fs to t8.
  mfc1(t8, fs);
  Cvt_d_uw(fd, t8, scratch);
}


void MacroAssembler::Cvt_d_uw(FPURegister fd,
                              Register rs,
                              FPURegister scratch) {
  // Convert rs to a FP value in fd (and fd + 1).
  // We do this by converting rs minus the MSB to avoid sign conversion,
  // then adding 2^31 to the result (if needed).

  ASSERT(!fd.is(scratch));
  ASSERT(!rs.is(t9));
  ASSERT(!rs.is(at));

  // Save rs's MSB to t9.
  Ext(t9, rs, 31, 1);
  // Remove rs's MSB.
  Ext(at, rs, 0, 31);
  // Move the result to fd.
  mtc1(at, fd);

  // Convert fd to a real FP value.
  cvt_d_w(fd, fd);

  Label conversion_done;

  // If rs's MSB was 0, it's done.
  // Otherwise we need to add that to the FP register.
  Branch(&conversion_done, eq, t9, Operand(zero_reg));

  // Load 2^31 into f20 as its float representation.
  li(at, 0x41E00000);
  mtc1(at, FPURegister::from_code(scratch.code() + 1));
  mtc1(zero_reg, scratch);
  // Add it to fd.
  add_d(fd, fd, scratch);

  bind(&conversion_done);
}


void MacroAssembler::Trunc_uw_d(FPURegister fd,
                                FPURegister fs,
                                FPURegister scratch) {
  Trunc_uw_d(fs, t8, scratch);
  mtc1(t8, fd);
}

void MacroAssembler::Trunc_w_d(FPURegister fd, FPURegister fs) {
  if (kArchVariant == kLoongson && fd.is(fs)) {
    mfc1(t8, FPURegister::from_code(fs.code() + 1));
    trunc_w_d(fd, fs);
    mtc1(t8, FPURegister::from_code(fs.code() + 1));
  } else {
    trunc_w_d(fd, fs);
  }
}

void MacroAssembler::Round_w_d(FPURegister fd, FPURegister fs) {
  if (kArchVariant == kLoongson && fd.is(fs)) {
    mfc1(t8, FPURegister::from_code(fs.code() + 1));
    round_w_d(fd, fs);
    mtc1(t8, FPURegister::from_code(fs.code() + 1));
  } else {
    round_w_d(fd, fs);
  }
}


void MacroAssembler::Floor_w_d(FPURegister fd, FPURegister fs) {
  if (kArchVariant == kLoongson && fd.is(fs)) {
    mfc1(t8, FPURegister::from_code(fs.code() + 1));
    floor_w_d(fd, fs);
    mtc1(t8, FPURegister::from_code(fs.code() + 1));
  } else {
    floor_w_d(fd, fs);
  }
}


void MacroAssembler::Ceil_w_d(FPURegister fd, FPURegister fs) {
  if (kArchVariant == kLoongson && fd.is(fs)) {
    mfc1(t8, FPURegister::from_code(fs.code() + 1));
    ceil_w_d(fd, fs);
    mtc1(t8, FPURegister::from_code(fs.code() + 1));
  } else {
    ceil_w_d(fd, fs);
  }
}


void MacroAssembler::Trunc_uw_d(FPURegister fd,
                                Register rs,
                                FPURegister scratch) {
  ASSERT(!fd.is(scratch));
  ASSERT(!rs.is(at));

  // Load 2^31 into scratch as its float representation.
  li(at, 0x41E00000);
  mtc1(at, FPURegister::from_code(scratch.code() + 1));
  mtc1(zero_reg, scratch);
  // Test if scratch > fd.
  // If fd < 2^31 we can convert it normally.
  Label simple_convert;
  BranchF(&simple_convert, NULL, lt, fd, scratch);

  // First we subtract 2^31 from fd, then trunc it to rs
  // and add 2^31 to rs.
  sub_d(scratch, fd, scratch);
  trunc_w_d(scratch, scratch);
  mfc1(rs, scratch);
  Or(rs, rs, 1 << 31);

  Label done;
  Branch(&done);
  // Simple conversion.
  bind(&simple_convert);
  trunc_w_d(scratch, fd);
  mfc1(rs, scratch);

  bind(&done);
}


void MacroAssembler::BranchF(Label* target,
                             Label* nan,
                             Condition cc,
                             FPURegister cmp1,
                             FPURegister cmp2,
                             BranchDelaySlot bd) {
  if (cc == al) {
    Branch(bd, target);
    return;
  }

  ASSERT(nan || target);
  // Check for unordered (NaN) cases.
  if (nan) {
    c(UN, D, cmp1, cmp2);
    bc1t(nan);
  }

  if (target) {
    // Here NaN cases were either handled by this function or are assumed to
    // have been handled by the caller.
    // Unsigned conditions are treated as their signed counterpart.
    switch (cc) {
      case Uless:
      case less:
        c(OLT, D, cmp1, cmp2);
        bc1t(target);
        break;
      case Ugreater:
      case greater:
        c(ULE, D, cmp1, cmp2);
        bc1f(target);
        break;
      case Ugreater_equal:
      case greater_equal:
        c(ULT, D, cmp1, cmp2);
        bc1f(target);
        break;
      case Uless_equal:
      case less_equal:
        c(OLE, D, cmp1, cmp2);
        bc1t(target);
        break;
      case eq:
        c(EQ, D, cmp1, cmp2);
        bc1t(target);
        break;
      case ne:
        c(EQ, D, cmp1, cmp2);
        bc1f(target);
        break;
      default:
        CHECK(0);
    };
  }

  if (bd == PROTECT) {
    nop();
  }
}


void MacroAssembler::Move(FPURegister dst, double imm) {
  ASSERT(CpuFeatures::IsEnabled(FPU));
  static const DoubleRepresentation minus_zero(-0.0);
  static const DoubleRepresentation zero(0.0);
  DoubleRepresentation value(imm);
  // Handle special values first.
  bool force_load = dst.is(kDoubleRegZero);
  if (value.bits == zero.bits && !force_load) {
    mov_d(dst, kDoubleRegZero);
  } else if (value.bits == minus_zero.bits && !force_load) {
    neg_d(dst, kDoubleRegZero);
  } else {
    uint32_t lo, hi;
    DoubleAsTwoUInt32(imm, &lo, &hi);
    // Move the low part of the double into the lower of the corresponding FPU
    // register of FPU register pair.
    if (lo != 0) {
      li(at, Operand(lo));
      mtc1(at, dst);
    } else {
      mtc1(zero_reg, dst);
    }
    // Move the high part of the double into the higher of the corresponding FPU
    // register of FPU register pair.
    if (hi != 0) {
      li(at, Operand(hi));
      mtc1(at, dst.high());
    } else {
      mtc1(zero_reg, dst.high());
    }
  }
}


void MacroAssembler::Movz(Register rd, Register rs, Register rt) {
  if (kArchVariant == kLoongson) {
    Label done;
    Branch(&done, ne, rt, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movz(rd, rs, rt);
  }
}


void MacroAssembler::Movn(Register rd, Register rs, Register rt) {
  if (kArchVariant == kLoongson) {
    Label done;
    Branch(&done, eq, rt, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movn(rd, rs, rt);
  }
}


void MacroAssembler::Movt(Register rd, Register rs, uint16_t cc) {
  if (kArchVariant == kLoongson) {
    // Tests an FP condition code and then conditionally move rs to rd.
    // We do not currently use any FPU cc bit other than bit 0.
    ASSERT(cc == 0);
    ASSERT(!(rs.is(t8) || rd.is(t8)));
    Label done;
    Register scratch = t8;
    // For testing purposes we need to fetch content of the FCSR register and
    // than test its cc (floating point condition code) bit (for cc = 0, it is
    // 24. bit of the FCSR).
    cfc1(scratch, FCSR);
    // For the MIPS I, II and III architectures, the contents of scratch is
    // UNPREDICTABLE for the instruction immediately following CFC1.
    nop();
    srl(scratch, scratch, 16);
    andi(scratch, scratch, 0x0080);
    Branch(&done, eq, scratch, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movt(rd, rs, cc);
  }
}


void MacroAssembler::Movf(Register rd, Register rs, uint16_t cc) {
  if (kArchVariant == kLoongson) {
    // Tests an FP condition code and then conditionally move rs to rd.
    // We do not currently use any FPU cc bit other than bit 0.
    ASSERT(cc == 0);
    ASSERT(!(rs.is(t8) || rd.is(t8)));
    Label done;
    Register scratch = t8;
    // For testing purposes we need to fetch content of the FCSR register and
    // than test its cc (floating point condition code) bit (for cc = 0, it is
    // 24. bit of the FCSR).
    cfc1(scratch, FCSR);
    // For the MIPS I, II and III architectures, the contents of scratch is
    // UNPREDICTABLE for the instruction immediately following CFC1.
    nop();
    srl(scratch, scratch, 16);
    andi(scratch, scratch, 0x0080);
    Branch(&done, ne, scratch, Operand(zero_reg));
    mov(rd, rs);
    bind(&done);
  } else {
    movf(rd, rs, cc);
  }
}


void MacroAssembler::Clz(Register rd, Register rs) {
  if (kArchVariant == kLoongson) {
    ASSERT(!(rd.is(t8) || rd.is(t9)) && !(rs.is(t8) || rs.is(t9)));
    Register mask = t8;
    Register scratch = t9;
    Label loop, end;
    mov(at, rs);
    mov(rd, zero_reg);
    lui(mask, 0x8000);
    bind(&loop);
    and_(scratch, at, mask);
    Branch(&end, ne, scratch, Operand(zero_reg));
    addiu(rd, rd, 1);
    Branch(&loop, ne, mask, Operand(zero_reg), USE_DELAY_SLOT);
    srl(mask, mask, 1);
    bind(&end);
  } else {
    clz(rd, rs);
  }
}


// Tries to get a signed int32 out of a double precision floating point heap
// number. Rounds towards 0. Branch to 'not_int32' if the double is out of the
// 32bits signed integer range.
// This method implementation differs from the ARM version for performance
// reasons.
void MacroAssembler::ConvertToInt32(Register source,
                                    Register dest,
                                    Register scratch,
                                    Register scratch2,
                                    FPURegister double_scratch,
                                    Label *not_int32) {
  Label right_exponent, done;
  // Get exponent word (ENDIAN issues).
  lw(scratch, FieldMemOperand(source, HeapNumber::kExponentOffset));
  // Get exponent alone in scratch2.
  And(scratch2, scratch, Operand(HeapNumber::kExponentMask));
  // Load dest with zero.  We use this either for the final shift or
  // for the answer.
  mov(dest, zero_reg);
  // Check whether the exponent matches a 32 bit signed int that is not a Smi.
  // A non-Smi integer is 1.xxx * 2^30 so the exponent is 30 (biased).  This is
  // the exponent that we are fastest at and also the highest exponent we can
  // handle here.
  const uint32_t non_smi_exponent =
      (HeapNumber::kExponentBias + 30) << HeapNumber::kExponentShift;
  // If we have a match of the int32-but-not-Smi exponent then skip some logic.
  Branch(&right_exponent, eq, scratch2, Operand(non_smi_exponent));
  // If the exponent is higher than that then go to not_int32 case.  This
  // catches numbers that don't fit in a signed int32, infinities and NaNs.
  Branch(not_int32, gt, scratch2, Operand(non_smi_exponent));

  // We know the exponent is smaller than 30 (biased).  If it is less than
  // 0 (biased) then the number is smaller in magnitude than 1.0 * 2^0, i.e.
  // it rounds to zero.
  const uint32_t zero_exponent =
      (HeapNumber::kExponentBias + 0) << HeapNumber::kExponentShift;
  Subu(scratch2, scratch2, Operand(zero_exponent));
  // Dest already has a Smi zero.
  Branch(&done, lt, scratch2, Operand(zero_reg));
  if (!CpuFeatures::IsSupported(FPU)) {
    // We have a shifted exponent between 0 and 30 in scratch2.
    srl(dest, scratch2, HeapNumber::kExponentShift);
    // We now have the exponent in dest.  Subtract from 30 to get
    // how much to shift down.
    li(at, Operand(30));
    subu(dest, at, dest);
  }
  bind(&right_exponent);
  if (CpuFeatures::IsSupported(FPU)) {
    CpuFeatures::Scope scope(FPU);
    // MIPS FPU instructions implementing double precision to integer
    // conversion using round to zero. Since the FP value was qualified
    // above, the resulting integer should be a legal int32.
    // The original 'Exponent' word is still in scratch.
    lwc1(double_scratch, FieldMemOperand(source, HeapNumber::kMantissaOffset));
    mtc1(scratch, FPURegister::from_code(double_scratch.code() + 1));
    trunc_w_d(double_scratch, double_scratch);
    mfc1(dest, double_scratch);
  } else {
    // On entry, dest has final downshift, scratch has original sign/exp/mant.
    // Save sign bit in top bit of dest.
    And(scratch2, scratch, Operand(0x80000000));
    Or(dest, dest, Operand(scratch2));
    // Put back the implicit 1, just above mantissa field.
    Or(scratch, scratch, Operand(1 << HeapNumber::kExponentShift));

    // Shift up the mantissa bits to take up the space the exponent used to
    // take. We just orred in the implicit bit so that took care of one and
    // we want to leave the sign bit 0 so we subtract 2 bits from the shift
    // distance. But we want to clear the sign-bit so shift one more bit
    // left, then shift right one bit.
    const int shift_distance = HeapNumber::kNonMantissaBitsInTopWord - 2;
    sll(scratch, scratch, shift_distance + 1);
    srl(scratch, scratch, 1);

    // Get the second half of the double. For some exponents we don't
    // actually need this because the bits get shifted out again, but
    // it's probably slower to test than just to do it.
    lw(scratch2, FieldMemOperand(source, HeapNumber::kMantissaOffset));
    // Extract the top 10 bits, and insert those bottom 10 bits of scratch.
    // The width of the field here is the same as the shift amount above.
    const int field_width = shift_distance;
    Ext(scratch2, scratch2, 32-shift_distance, field_width);
    Ins(scratch, scratch2, 0, field_width);
    // Move down according to the exponent.
    srlv(scratch, scratch, dest);
    // Prepare the negative version of our integer.
    subu(scratch2, zero_reg, scratch);
    // Trick to check sign bit (msb) held in dest, count leading zero.
    // 0 indicates negative, save negative version with conditional move.
    Clz(dest, dest);
    Movz(scratch, scratch2, dest);
    mov(dest, scratch);
  }
  bind(&done);
}


void MacroAssembler::EmitFPUTruncate(FPURoundingMode rounding_mode,
                                     FPURegister result,
                                     DoubleRegister double_input,
                                     Register scratch1,
                                     Register except_flag,
                                     CheckForInexactConversion check_inexact) {
  ASSERT(CpuFeatures::IsSupported(FPU));
  CpuFeatures::Scope scope(FPU);

  int32_t except_mask = kFCSRFlagMask;  // Assume interested in all exceptions.

  if (check_inexact == kDontCheckForInexactConversion) {
    // Ingore inexact exceptions.
    except_mask &= ~kFCSRInexactFlagMask;
  }

  // Save FCSR.
  cfc1(scratch1, FCSR);
  // Disable FPU exceptions.
  ctc1(zero_reg, FCSR);

  // Do operation based on rounding mode.
  switch (rounding_mode) {
    case kRoundToNearest:
      Round_w_d(result, double_input);
      break;
    case kRoundToZero:
      Trunc_w_d(result, double_input);
      break;
    case kRoundToPlusInf:
      Ceil_w_d(result, double_input);
      break;
    case kRoundToMinusInf:
      Floor_w_d(result, double_input);
      break;
  }  // End of switch-statement.

  // Retrieve FCSR.
  cfc1(except_flag, FCSR);
  // Restore FCSR.
  ctc1(scratch1, FCSR);

  // Check for fpu exceptions.
  And(except_flag, except_flag, Operand(except_mask));
}


void MacroAssembler::EmitOutOfInt32RangeTruncate(Register result,
                                                 Register input_high,
                                                 Register input_low,
                                                 Register scratch) {
  Label done, normal_exponent, restore_sign;
  // Extract the biased exponent in result.
  Ext(result,
      input_high,
      HeapNumber::kExponentShift,
      HeapNumber::kExponentBits);

  // Check for Infinity and NaNs, which should return 0.
  Subu(scratch, result, HeapNumber::kExponentMask);
  Movz(result, zero_reg, scratch);
  Branch(&done, eq, scratch, Operand(zero_reg));

  // Express exponent as delta to (number of mantissa bits + 31).
  Subu(result,
       result,
       Operand(HeapNumber::kExponentBias + HeapNumber::kMantissaBits + 31));

  // If the delta is strictly positive, all bits would be shifted away,
  // which means that we can return 0.
  Branch(&normal_exponent, le, result, Operand(zero_reg));
  mov(result, zero_reg);
  Branch(&done);

  bind(&normal_exponent);
  const int kShiftBase = HeapNumber::kNonMantissaBitsInTopWord - 1;
  // Calculate shift.
  Addu(scratch, result, Operand(kShiftBase + HeapNumber::kMantissaBits));

  // Save the sign.
  Register sign = result;
  result = no_reg;
  And(sign, input_high, Operand(HeapNumber::kSignMask));

  // On ARM shifts > 31 bits are valid and will result in zero. On MIPS we need
  // to check for this specific case.
  Label high_shift_needed, high_shift_done;
  Branch(&high_shift_needed, lt, scratch, Operand(32));
  mov(input_high, zero_reg);
  Branch(&high_shift_done);
  bind(&high_shift_needed);

  // Set the implicit 1 before the mantissa part in input_high.
  Or(input_high,
     input_high,
     Operand(1 << HeapNumber::kMantissaBitsInTopWord));
  // Shift the mantissa bits to the correct position.
  // We don't need to clear non-mantissa bits as they will be shifted away.
  // If they weren't, it would mean that the answer is in the 32bit range.
  sllv(input_high, input_high, scratch);

  bind(&high_shift_done);

  // Replace the shifted bits with bits from the lower mantissa word.
  Label pos_shift, shift_done;
  li(at, 32);
  subu(scratch, at, scratch);
  Branch(&pos_shift, ge, scratch, Operand(zero_reg));

  // Negate scratch.
  Subu(scratch, zero_reg, scratch);
  sllv(input_low, input_low, scratch);
  Branch(&shift_done);

  bind(&pos_shift);
  srlv(input_low, input_low, scratch);

  bind(&shift_done);
  Or(input_high, input_high, Operand(input_low));
  // Restore sign if necessary.
  mov(scratch, sign);
  result = sign;
  sign = no_reg;
  Subu(result, zero_reg, input_high);
  Movz(result, input_high, scratch);
  bind(&done);
}


void MacroAssembler::EmitECMATruncate(Register result,
                                      FPURegister double_input,
                                      FPURegister single_scratch,
                                      Register scratch,
                                      Register scratch2,
                                      Register scratch3) {
  CpuFeatures::Scope scope(FPU);
  ASSERT(!scratch2.is(result));
  ASSERT(!scratch3.is(result));
  ASSERT(!scratch3.is(scratch2));
  ASSERT(!scratch.is(result) &&
         !scratch.is(scratch2) &&
         !scratch.is(scratch3));
  ASSERT(!single_scratch.is(double_input));

  Label done;
  Label manual;

  // Clear cumulative exception flags and save the FCSR.
  cfc1(scratch2, FCSR);
  ctc1(zero_reg, FCSR);
  // Try a conversion to a signed integer.
  trunc_w_d(single_scratch, double_input);
  mfc1(result, single_scratch);
  // Retrieve and restore the FCSR.
  cfc1(scratch, FCSR);
  ctc1(scratch2, FCSR);
  // Check for overflow and NaNs.
  And(scratch,
      scratch,
      kFCSROverflowFlagMask | kFCSRUnderflowFlagMask | kFCSRInvalidOpFlagMask);
  // If we had no exceptions we are done.
  Branch(&done, eq, scratch, Operand(zero_reg));

  // Load the double value and perform a manual truncation.
  Register input_high = scratch2;
  Register input_low = scratch3;
  Move(input_low, input_high, double_input);
  EmitOutOfInt32RangeTruncate(result,
                              input_high,
                              input_low,
                              scratch);
  bind(&done);
}


void MacroAssembler::GetLeastBitsFromSmi(Register dst,
                                         Register src,
                                         int num_least_bits) {
  Ext(dst, src, kSmiTagSize, num_least_bits);
}


void MacroAssembler::GetLeastBitsFromInt32(Register dst,
                                           Register src,
                                           int num_least_bits) {
  And(dst, src, Operand((1 << num_least_bits) - 1));
}


// Emulated condtional branches do not emit a nop in the branch delay slot.
//
// BRANCH_ARGS_CHECK checks that conditional jump arguments are correct.
#define BRANCH_ARGS_CHECK(cond, rs, rt) ASSERT(                                \
    (cond == cc_always && rs.is(zero_reg) && rt.rm().is(zero_reg)) ||          \
    (cond != cc_always && (!rs.is(zero_reg) || !rt.rm().is(zero_reg))))


void MacroAssembler::Branch(int16_t offset, BranchDelaySlot bdslot) {
  BranchShort(offset, bdslot);
}


void MacroAssembler::Branch(int16_t offset, Condition cond, Register rs,
                            const Operand& rt,
                            BranchDelaySlot bdslot) {
  BranchShort(offset, cond, rs, rt, bdslot);
}


void MacroAssembler::Branch(Label* L, BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (is_near(L)) {
      BranchShort(L, bdslot);
    } else {
      Jr(L, bdslot);
    }
  } else {
    if (is_trampoline_emitted()) {
      Jr(L, bdslot);
    } else {
      BranchShort(L, bdslot);
    }
  }
}


void MacroAssembler::Branch(Label* L, Condition cond, Register rs,
                            const Operand& rt,
                            BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (is_near(L)) {
      BranchShort(L, cond, rs, rt, bdslot);
    } else {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      Jr(L, bdslot);
      bind(&skip);
    }
  } else {
    if (is_trampoline_emitted()) {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      Jr(L, bdslot);
      bind(&skip);
    } else {
      BranchShort(L, cond, rs, rt, bdslot);
    }
  }
}


void MacroAssembler::Branch(Label* L,
                            Condition cond,
                            Register rs,
                            Heap::RootListIndex index,
                            BranchDelaySlot bdslot) {
  LoadRoot(at, index);
  Branch(L, cond, rs, Operand(at), bdslot);
}


void MacroAssembler::BranchShort(int16_t offset, BranchDelaySlot bdslot) {
  b(offset);

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::BranchShort(int16_t offset, Condition cond, Register rs,
                                 const Operand& rt,
                                 BranchDelaySlot bdslot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);
  ASSERT(!rs.is(zero_reg));
  Register r2 = no_reg;
  Register scratch = at;

  if (rt.is_reg()) {
    // NOTE: 'at' can be clobbered by Branch but it is legal to use it as rs or
    // rt.
    r2 = rt.rm_;
    switch (cond) {
      case cc_always:
        b(offset);
        break;
      case eq:
        beq(rs, r2, offset);
        break;
      case ne:
        bne(rs, r2, offset);
        break;
      // Signed comparison.
      case greater:
        if (r2.is(zero_reg)) {
          bgtz(rs, offset);
        } else {
          slt(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (r2.is(zero_reg)) {
          bgez(rs, offset);
        } else {
          slt(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (r2.is(zero_reg)) {
          bltz(rs, offset);
        } else {
          slt(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (r2.is(zero_reg)) {
          blez(rs, offset);
        } else {
          slt(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
        }
        break;
      // Unsigned comparison.
      case Ugreater:
        if (r2.is(zero_reg)) {
          bgtz(rs, offset);
        } else {
          sltu(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (r2.is(zero_reg)) {
          bgez(rs, offset);
        } else {
          sltu(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case Uless:
        if (r2.is(zero_reg)) {
          // No code needs to be emitted.
          return;
        } else {
          sltu(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (r2.is(zero_reg)) {
          b(offset);
        } else {
          sltu(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  } else {
    // Be careful to always use shifted_branch_offset only just before the
    // branch instruction, as the location will be remember for patching the
    // target.
    switch (cond) {
      case cc_always:
        b(offset);
        break;
      case eq:
        // We don't want any other register but scratch clobbered.
        ASSERT(!scratch.is(rs));
        r2 = scratch;
        li(r2, rt);
        beq(rs, r2, offset);
        break;
      case ne:
        // We don't want any other register but scratch clobbered.
        ASSERT(!scratch.is(rs));
        r2 = scratch;
        li(r2, rt);
        bne(rs, r2, offset);
        break;
      // Signed comparison.
      case greater:
        if (rt.imm32_ == 0) {
          bgtz(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (rt.imm32_ == 0) {
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          beq(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (rt.imm32_ == 0) {
          bltz(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          bne(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (rt.imm32_ == 0) {
          blez(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
       }
       break;
      // Unsigned comparison.
      case Ugreater:
        if (rt.imm32_ == 0) {
          bgtz(rs, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (rt.imm32_ == 0) {
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          beq(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          beq(scratch, zero_reg, offset);
        }
        break;
      case Uless:
        if (rt.imm32_ == 0) {
          // No code needs to be emitted.
          return;
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          bne(scratch, zero_reg, offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (rt.imm32_ == 0) {
          b(offset);
        } else {
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  }
  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::BranchShort(Label* L, BranchDelaySlot bdslot) {
  // We use branch_offset as an argument for the branch instructions to be sure
  // it is called just before generating the branch instruction, as needed.

  b(shifted_branch_offset(L, false));

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::BranchShort(Label* L, Condition cond, Register rs,
                                 const Operand& rt,
                                 BranchDelaySlot bdslot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  int32_t offset;
  Register r2 = no_reg;
  Register scratch = at;
  if (rt.is_reg()) {
    r2 = rt.rm_;
    // Be careful to always use shifted_branch_offset only just before the
    // branch instruction, as the location will be remember for patching the
    // target.
    switch (cond) {
      case cc_always:
        offset = shifted_branch_offset(L, false);
        b(offset);
        break;
      case eq:
        offset = shifted_branch_offset(L, false);
        beq(rs, r2, offset);
        break;
      case ne:
        offset = shifted_branch_offset(L, false);
        bne(rs, r2, offset);
        break;
      // Signed comparison.
      case greater:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bgtz(rs, offset);
        } else {
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else {
          slt(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bltz(rs, offset);
        } else {
          slt(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          blez(rs, offset);
        } else {
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      // Unsigned comparison.
      case Ugreater:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
           bgtz(rs, offset);
        } else {
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else {
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      case Uless:
        if (r2.is(zero_reg)) {
          // No code needs to be emitted.
          return;
        } else {
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (r2.is(zero_reg)) {
          offset = shifted_branch_offset(L, false);
          b(offset);
        } else {
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  } else {
    // Be careful to always use shifted_branch_offset only just before the
    // branch instruction, as the location will be remember for patching the
    // target.
    switch (cond) {
      case cc_always:
        offset = shifted_branch_offset(L, false);
        b(offset);
        break;
      case eq:
        ASSERT(!scratch.is(rs));
        r2 = scratch;
        li(r2, rt);
        offset = shifted_branch_offset(L, false);
        beq(rs, r2, offset);
        break;
      case ne:
        ASSERT(!scratch.is(rs));
        r2 = scratch;
        li(r2, rt);
        offset = shifted_branch_offset(L, false);
        bne(rs, r2, offset);
        break;
      // Signed comparison.
      case greater:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgtz(rs, offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case greater_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          slt(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      case less:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bltz(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          slti(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          slt(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case less_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          blez(rs, offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          slt(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      // Unsigned comparison.
      case Ugreater:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgtz(rs, offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Ugreater_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          bgez(rs, offset);
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
     case Uless:
        if (rt.imm32_ == 0) {
          // No code needs to be emitted.
          return;
        } else if (is_int16(rt.imm32_)) {
          sltiu(scratch, rs, rt.imm32_);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, rs, r2);
          offset = shifted_branch_offset(L, false);
          bne(scratch, zero_reg, offset);
        }
        break;
      case Uless_equal:
        if (rt.imm32_ == 0) {
          offset = shifted_branch_offset(L, false);
          b(offset);
        } else {
          ASSERT(!scratch.is(rs));
          r2 = scratch;
          li(r2, rt);
          sltu(scratch, r2, rs);
          offset = shifted_branch_offset(L, false);
          beq(scratch, zero_reg, offset);
        }
        break;
      default:
        UNREACHABLE();
    }
  }
  // Check that offset could actually hold on an int16_t.
  ASSERT(is_int16(offset));
  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::BranchAndLink(int16_t offset, BranchDelaySlot bdslot) {
  BranchAndLinkShort(offset, bdslot);
}


void MacroAssembler::BranchAndLink(int16_t offset, Condition cond, Register rs,
                                   const Operand& rt,
                                   BranchDelaySlot bdslot) {
  BranchAndLinkShort(offset, cond, rs, rt, bdslot);
}


void MacroAssembler::BranchAndLink(Label* L, BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (is_near(L)) {
      BranchAndLinkShort(L, bdslot);
    } else {
      Jalr(L, bdslot);
    }
  } else {
    if (is_trampoline_emitted()) {
      Jalr(L, bdslot);
    } else {
      BranchAndLinkShort(L, bdslot);
    }
  }
}


void MacroAssembler::BranchAndLink(Label* L, Condition cond, Register rs,
                                   const Operand& rt,
                                   BranchDelaySlot bdslot) {
  if (L->is_bound()) {
    if (is_near(L)) {
      BranchAndLinkShort(L, cond, rs, rt, bdslot);
    } else {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      Jalr(L, bdslot);
      bind(&skip);
    }
  } else {
    if (is_trampoline_emitted()) {
      Label skip;
      Condition neg_cond = NegateCondition(cond);
      BranchShort(&skip, neg_cond, rs, rt);
      Jalr(L, bdslot);
      bind(&skip);
    } else {
      BranchAndLinkShort(L, cond, rs, rt, bdslot);
    }
  }
}


// We need to use a bgezal or bltzal, but they can't be used directly with the
// slt instructions. We could use sub or add instead but we would miss overflow
// cases, so we keep slt and add an intermediate third instruction.
void MacroAssembler::BranchAndLinkShort(int16_t offset,
                                        BranchDelaySlot bdslot) {
  bal(offset);

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::BranchAndLinkShort(int16_t offset, Condition cond,
                                        Register rs, const Operand& rt,
                                        BranchDelaySlot bdslot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);
  Register r2 = no_reg;
  Register scratch = at;

  if (rt.is_reg()) {
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    r2 = scratch;
    li(r2, rt);
  }

  switch (cond) {
    case cc_always:
      bal(offset);
      break;
    case eq:
      bne(rs, r2, 2);
      nop();
      bal(offset);
      break;
    case ne:
      beq(rs, r2, 2);
      nop();
      bal(offset);
      break;

    // Signed comparison.
    case greater:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;
    case less:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case less_equal:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;

    // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;
    case Uless:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      bgezal(scratch, offset);
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      bltzal(scratch, offset);
      break;

    default:
      UNREACHABLE();
  }
  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::BranchAndLinkShort(Label* L, BranchDelaySlot bdslot) {
  bal(shifted_branch_offset(L, false));

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::BranchAndLinkShort(Label* L, Condition cond, Register rs,
                                        const Operand& rt,
                                        BranchDelaySlot bdslot) {
  BRANCH_ARGS_CHECK(cond, rs, rt);

  int32_t offset;
  Register r2 = no_reg;
  Register scratch = at;
  if (rt.is_reg()) {
    r2 = rt.rm_;
  } else if (cond != cc_always) {
    r2 = scratch;
    li(r2, rt);
  }

  switch (cond) {
    case cc_always:
      offset = shifted_branch_offset(L, false);
      bal(offset);
      break;
    case eq:
      bne(rs, r2, 2);
      nop();
      offset = shifted_branch_offset(L, false);
      bal(offset);
      break;
    case ne:
      beq(rs, r2, 2);
      nop();
      offset = shifted_branch_offset(L, false);
      bal(offset);
      break;

    // Signed comparison.
    case greater:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case greater_equal:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;
    case less:
      slt(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case less_equal:
      slt(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;

    // Unsigned comparison.
    case Ugreater:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case Ugreater_equal:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;
    case Uless:
      sltu(scratch, rs, r2);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bgezal(scratch, offset);
      break;
    case Uless_equal:
      sltu(scratch, r2, rs);
      addiu(scratch, scratch, -1);
      offset = shifted_branch_offset(L, false);
      bltzal(scratch, offset);
      break;

    default:
      UNREACHABLE();
  }

  // Check that offset could actually hold on an int16_t.
  ASSERT(is_int16(offset));

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::Jump(Register target,
                          Condition cond,
                          Register rs,
                          const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (cond == cc_always) {
    jr(target);
  } else {
    BRANCH_ARGS_CHECK(cond, rs, rt);
    Branch(2, NegateCondition(cond), rs, rt);
    jr(target);
  }
  // Emit a nop in the branch delay slot if required.
  if (bd == PROTECT)
    nop();
}


void MacroAssembler::Jump(intptr_t target,
                          RelocInfo::Mode rmode,
                          Condition cond,
                          Register rs,
                          const Operand& rt,
                          BranchDelaySlot bd) {
  Label skip;
  if (cond != cc_always) {
    Branch(USE_DELAY_SLOT, &skip, NegateCondition(cond), rs, rt);
  }
  // The first instruction of 'li' may be placed in the delay slot.
  // This is not an issue, t9 is expected to be clobbered anyway.
  li(t9, Operand(target, rmode));
  Jump(t9, al, zero_reg, Operand(zero_reg), bd);
  bind(&skip);
}


void MacroAssembler::Jump(Address target,
                          RelocInfo::Mode rmode,
                          Condition cond,
                          Register rs,
                          const Operand& rt,
                          BranchDelaySlot bd) {
  ASSERT(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond, rs, rt, bd);
}


void MacroAssembler::Jump(Handle<Code> code,
                          RelocInfo::Mode rmode,
                          Condition cond,
                          Register rs,
                          const Operand& rt,
                          BranchDelaySlot bd) {
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode, cond, rs, rt, bd);
}


int MacroAssembler::CallSize(Register target,
                             Condition cond,
                             Register rs,
                             const Operand& rt,
                             BranchDelaySlot bd) {
  int size = 0;

  if (cond == cc_always) {
    size += 1;
  } else {
    size += 3;
  }

  if (bd == PROTECT)
    size += 1;

  return size * kInstrSize;
}


// Note: To call gcc-compiled C code on mips, you must call thru t9.
void MacroAssembler::Call(Register target,
                          Condition cond,
                          Register rs,
                          const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label start;
  bind(&start);
  if (cond == cc_always) {
    jalr(target);
  } else {
    BRANCH_ARGS_CHECK(cond, rs, rt);
    Branch(2, NegateCondition(cond), rs, rt);
    jalr(target);
  }
  // Emit a nop in the branch delay slot if required.
  if (bd == PROTECT)
    nop();

  ASSERT_EQ(CallSize(target, cond, rs, rt, bd),
            SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(Address target,
                             RelocInfo::Mode rmode,
                             Condition cond,
                             Register rs,
                             const Operand& rt,
                             BranchDelaySlot bd) {
  int size = CallSize(t9, cond, rs, rt, bd);
  return size + 2 * kInstrSize;
}


void MacroAssembler::Call(Address target,
                          RelocInfo::Mode rmode,
                          Condition cond,
                          Register rs,
                          const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label start;
  bind(&start);
  int32_t target_int = reinterpret_cast<int32_t>(target);
  // Must record previous source positions before the
  // li() generates a new code target.
  positions_recorder()->WriteRecordedPositions();
  li(t9, Operand(target_int, rmode), CONSTANT_SIZE);
  Call(t9, cond, rs, rt, bd);
  ASSERT_EQ(CallSize(target, rmode, cond, rs, rt, bd),
            SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(Handle<Code> code,
                             RelocInfo::Mode rmode,
                             unsigned ast_id,
                             Condition cond,
                             Register rs,
                             const Operand& rt,
                             BranchDelaySlot bd) {
  return CallSize(reinterpret_cast<Address>(code.location()),
      rmode, cond, rs, rt, bd);
}


void MacroAssembler::Call(Handle<Code> code,
                          RelocInfo::Mode rmode,
                          unsigned ast_id,
                          Condition cond,
                          Register rs,
                          const Operand& rt,
                          BranchDelaySlot bd) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label start;
  bind(&start);
  ASSERT(RelocInfo::IsCodeTarget(rmode));
  if (rmode == RelocInfo::CODE_TARGET && ast_id != kNoASTId) {
    SetRecordedAstId(ast_id);
    rmode = RelocInfo::CODE_TARGET_WITH_ID;
  }
  Call(reinterpret_cast<Address>(code.location()), rmode, cond, rs, rt, bd);
  ASSERT_EQ(CallSize(code, rmode, ast_id, cond, rs, rt, bd),
            SizeOfCodeGeneratedSince(&start));
}


void MacroAssembler::Ret(Condition cond,
                         Register rs,
                         const Operand& rt,
                         BranchDelaySlot bd) {
  Jump(ra, cond, rs, rt, bd);
}


void MacroAssembler::J(Label* L, BranchDelaySlot bdslot) {
  BlockTrampolinePoolScope block_trampoline_pool(this);

  uint32_t imm28;
  imm28 = jump_address(L);
  imm28 &= kImm28Mask;
  { BlockGrowBufferScope block_buf_growth(this);
    // Buffer growth (and relocation) must be blocked for internal references
    // until associated instructions are emitted and available to be patched.
    RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
    j(imm28);
  }
  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::Jr(Label* L, BranchDelaySlot bdslot) {
  BlockTrampolinePoolScope block_trampoline_pool(this);

  uint32_t imm32;
  imm32 = jump_address(L);
  { BlockGrowBufferScope block_buf_growth(this);
    // Buffer growth (and relocation) must be blocked for internal references
    // until associated instructions are emitted and available to be patched.
    RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
    lui(at, (imm32 & kHiMask) >> kLuiShift);
    ori(at, at, (imm32 & kImm16Mask));
  }
  jr(at);

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}


void MacroAssembler::Jalr(Label* L, BranchDelaySlot bdslot) {
  BlockTrampolinePoolScope block_trampoline_pool(this);

  uint32_t imm32;
  imm32 = jump_address(L);
  { BlockGrowBufferScope block_buf_growth(this);
    // Buffer growth (and relocation) must be blocked for internal references
    // until associated instructions are emitted and available to be patched.
    RecordRelocInfo(RelocInfo::INTERNAL_REFERENCE);
    lui(at, (imm32 & kHiMask) >> kLuiShift);
    ori(at, at, (imm32 & kImm16Mask));
  }
  jalr(at);

  // Emit a nop in the branch delay slot if required.
  if (bdslot == PROTECT)
    nop();
}

void MacroAssembler::DropAndRet(int drop) {
  Ret(USE_DELAY_SLOT);
  addiu(sp, sp, drop * kPointerSize);
}

void MacroAssembler::DropAndRet(int drop,
                                Condition cond,
                                Register r1,
                                const Operand& r2) {
  // Both Drop and Ret need to be conditional.
  Label skip;
  if (cond != cc_always) {
    Branch(&skip, NegateCondition(cond), r1, r2);
  }

  Drop(drop);
  Ret();

  if (cond != cc_always) {
    bind(&skip);
  }
}


void MacroAssembler::Drop(int count,
                          Condition cond,
                          Register reg,
                          const Operand& op) {
  if (count <= 0) {
    return;
  }

  Label skip;

  if (cond != al) {
     Branch(&skip, NegateCondition(cond), reg, op);
  }

  addiu(sp, sp, count * kPointerSize);

  if (cond != al) {
    bind(&skip);
  }
}



void MacroAssembler::Swap(Register reg1,
                          Register reg2,
                          Register scratch) {
  if (scratch.is(no_reg)) {
    Xor(reg1, reg1, Operand(reg2));
    Xor(reg2, reg2, Operand(reg1));
    Xor(reg1, reg1, Operand(reg2));
  } else {
    mov(scratch, reg1);
    mov(reg1, reg2);
    mov(reg2, scratch);
  }
}


void MacroAssembler::Call(Label* target) {
  BranchAndLink(target);
}


void MacroAssembler::Push(Handle<Object> handle) {
  li(at, Operand(handle));
  push(at);
}


#ifdef ENABLE_DEBUGGER_SUPPORT

void MacroAssembler::DebugBreak() {
  PrepareCEntryArgs(0);
  PrepareCEntryFunction(ExternalReference(Runtime::kDebugBreak, isolate()));
  CEntryStub ces(1);
  ASSERT(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}

#endif  // ENABLE_DEBUGGER_SUPPORT


// ---------------------------------------------------------------------------
// Exception handling.

void MacroAssembler::PushTryHandler(StackHandler::Kind kind,
                                    int handler_index) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // For the JSEntry handler, we must preserve a0-a3 and s0.
  // t1-t3 are available. We will build up the handler from the bottom by
  // pushing on the stack.
  // Set up the code object (t1) and the state (t2) for pushing.
  unsigned state =
      StackHandler::IndexField::encode(handler_index) |
      StackHandler::KindField::encode(kind);
  li(t1, Operand(CodeObject()), CONSTANT_SIZE);
  li(t2, Operand(state));

  // Push the frame pointer, context, state, and code object.
  if (kind == StackHandler::JS_ENTRY) {
    ASSERT_EQ(Smi::FromInt(0), 0);
    // The second zero_reg indicates no context.
    // The first zero_reg is the NULL frame pointer.
    // The operands are reversed to match the order of MultiPush/Pop.
    Push(zero_reg, zero_reg, t2, t1);
  } else {
    MultiPush(t1.bit() | t2.bit() | cp.bit() | fp.bit());
  }

  // Link the current handler as the next handler.
  li(t2, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  lw(t1, MemOperand(t2));
  push(t1);
  // Set this new handler as the current one.
  sw(sp, MemOperand(t2));
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  pop(a1);
  Addu(sp, sp, Operand(StackHandlerConstants::kSize - kPointerSize));
  li(at, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  sw(a1, MemOperand(at));
}


void MacroAssembler::JumpToHandlerEntry() {
  // Compute the handler entry address and jump to it.  The handler table is
  // a fixed array of (smi-tagged) code offsets.
  // v0 = exception, a1 = code object, a2 = state.
  lw(a3, FieldMemOperand(a1, Code::kHandlerTableOffset));  // Handler table.
  Addu(a3, a3, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  srl(a2, a2, StackHandler::kKindWidth);  // Handler index.
  sll(a2, a2, kPointerSizeLog2);
  Addu(a2, a3, a2);
  lw(a2, MemOperand(a2));  // Smi-tagged offset.
  Addu(a1, a1, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start.
  sra(t9, a2, kSmiTagSize);
  Addu(t9, t9, a1);
  Jump(t9);  // Jump.
}


void MacroAssembler::Throw(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in v0.
  Move(v0, value);

  // Drop the stack pointer to the top of the top handler.
  li(a3, Operand(ExternalReference(Isolate::kHandlerAddress,
                                   isolate())));
  lw(sp, MemOperand(a3));

  // Restore the next handler.
  pop(a2);
  sw(a2, MemOperand(a3));

  // Get the code object (a1) and state (a2).  Restore the context and frame
  // pointer.
  MultiPop(a1.bit() | a2.bit() | cp.bit() | fp.bit());

  // If the handler is a JS frame, restore the context to the frame.
  // (kind == ENTRY) == (fp == 0) == (cp == 0), so we could test either fp
  // or cp.
  Label done;
  Branch(&done, eq, cp, Operand(zero_reg));
  sw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  bind(&done);

  JumpToHandlerEntry();
}


void MacroAssembler::ThrowUncatchable(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in v0.
  if (!value.is(v0)) {
    mov(v0, value);
  }
  // Drop the stack pointer to the top of the top stack handler.
  li(a3, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  lw(sp, MemOperand(a3));

  // Unwind the handlers until the ENTRY handler is found.
  Label fetch_next, check_kind;
  jmp(&check_kind);
  bind(&fetch_next);
  lw(sp, MemOperand(sp, StackHandlerConstants::kNextOffset));

  bind(&check_kind);
  STATIC_ASSERT(StackHandler::JS_ENTRY == 0);
  lw(a2, MemOperand(sp, StackHandlerConstants::kStateOffset));
  And(a2, a2, Operand(StackHandler::KindField::kMask));
  Branch(&fetch_next, ne, a2, Operand(zero_reg));

  // Set the top handler address to next handler past the top ENTRY handler.
  pop(a2);
  sw(a2, MemOperand(a3));

  // Get the code object (a1) and state (a2).  Clear the context and frame
  // pointer (0 was saved in the handler).
  MultiPop(a1.bit() | a2.bit() | cp.bit() | fp.bit());

  JumpToHandlerEntry();
}


void MacroAssembler::AllocateInNewSpace(int object_size,
                                        Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      li(result, 0x7091);
      li(scratch1, 0x7191);
      li(scratch2, 0x7291);
    }
    jmp(gc_required);
    return;
  }

  ASSERT(!result.is(scratch1));
  ASSERT(!result.is(scratch2));
  ASSERT(!scratch1.is(scratch2));
  ASSERT(!scratch1.is(t9));
  ASSERT(!scratch2.is(t9));
  ASSERT(!result.is(t9));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  ASSERT_EQ(0, object_size & kObjectAlignmentMask);

  // Check relative positions of allocation top and limit addresses.
  // ARM adds additional checks to make sure the ldm instruction can be
  // used. On MIPS we don't have ldm so we don't need additional checks either.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address(isolate());
  intptr_t top   =
      reinterpret_cast<intptr_t>(new_space_allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(new_space_allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);

  // Set up allocation top address and object size registers.
  Register topaddr = scratch1;
  Register obj_size_reg = scratch2;
  li(topaddr, Operand(new_space_allocation_top));
  li(obj_size_reg, Operand(object_size));

  // This code stores a temporary value in t9.
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into t9.
    lw(result, MemOperand(topaddr));
    lw(t9, MemOperand(topaddr, kPointerSize));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry. t9 is used
      // immediately below so this use of t9 does not cause difference with
      // respect to register content between debug and release mode.
      lw(t9, MemOperand(topaddr));
      Check(eq, "Unexpected allocation top", result, Operand(t9));
    }
    // Load allocation limit into t9. Result already contains allocation top.
    lw(t9, MemOperand(topaddr, limit - top));
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top.
  Addu(scratch2, result, Operand(obj_size_reg));
  Branch(gc_required, Ugreater, scratch2, Operand(t9));
  sw(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    Addu(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::AllocateInNewSpace(Register object_size,
                                        Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Label* gc_required,
                                        AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      li(result, 0x7091);
      li(scratch1, 0x7191);
      li(scratch2, 0x7291);
    }
    jmp(gc_required);
    return;
  }

  ASSERT(!result.is(scratch1));
  ASSERT(!result.is(scratch2));
  ASSERT(!scratch1.is(scratch2));
  ASSERT(!object_size.is(t9));
  ASSERT(!scratch1.is(t9) && !scratch2.is(t9) && !result.is(t9));

  // Check relative positions of allocation top and limit addresses.
  // ARM adds additional checks to make sure the ldm instruction can be
  // used. On MIPS we don't have ldm so we don't need additional checks either.
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  ExternalReference new_space_allocation_limit =
      ExternalReference::new_space_allocation_limit_address(isolate());
  intptr_t top   =
      reinterpret_cast<intptr_t>(new_space_allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(new_space_allocation_limit.address());
  ASSERT((limit - top) == kPointerSize);

  // Set up allocation top address and object size registers.
  Register topaddr = scratch1;
  li(topaddr, Operand(new_space_allocation_top));

  // This code stores a temporary value in t9.
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into t9.
    lw(result, MemOperand(topaddr));
    lw(t9, MemOperand(topaddr, kPointerSize));
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry. t9 is used
      // immediately below so this use of t9 does not cause difference with
      // respect to register content between debug and release mode.
      lw(t9, MemOperand(topaddr));
      Check(eq, "Unexpected allocation top", result, Operand(t9));
    }
    // Load allocation limit into t9. Result already contains allocation top.
    lw(t9, MemOperand(topaddr, limit - top));
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size may be in words so a shift is
  // required to get the number of bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    sll(scratch2, object_size, kPointerSizeLog2);
    Addu(scratch2, result, scratch2);
  } else {
    Addu(scratch2, result, Operand(object_size));
  }
  Branch(gc_required, Ugreater, scratch2, Operand(t9));

  // Update allocation top. result temporarily holds the new top.
  if (emit_debug_code()) {
    And(t9, scratch2, Operand(kObjectAlignmentMask));
    Check(eq, "Unaligned allocation in new space", t9, Operand(zero_reg));
  }
  sw(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    Addu(result, result, Operand(kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object,
                                              Register scratch) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  And(object, object, Operand(~kHeapObjectTagMask));
#ifdef DEBUG
  // Check that the object un-allocated is below the current top.
  li(scratch, Operand(new_space_allocation_top));
  lw(scratch, MemOperand(scratch));
  Check(less, "Undo allocation of non allocated memory",
      object, Operand(scratch));
#endif
  // Write the address of the object to un-allocate as the current top.
  li(scratch, Operand(new_space_allocation_top));
  sw(object, MemOperand(scratch));
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  sll(scratch1, length, 1);  // Length in bytes, not chars.
  addiu(scratch1, scratch1,
       kObjectAlignmentMask + SeqTwoByteString::kHeaderSize);
  And(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate two-byte string in new space.
  AllocateInNewSpace(scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         Register length,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string
  // while observing object alignment.
  ASSERT((SeqAsciiString::kHeaderSize & kObjectAlignmentMask) == 0);
  ASSERT(kCharSize == 1);
  addiu(scratch1, length, kObjectAlignmentMask + SeqAsciiString::kHeaderSize);
  And(scratch1, scratch1, Operand(~kObjectAlignmentMask));

  // Allocate ASCII string in new space.
  AllocateInNewSpace(scratch1,
                     result,
                     scratch2,
                     scratch3,
                     gc_required,
                     TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateTwoByteConsString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);
  InitializeNewString(result,
                      length,
                      Heap::kConsStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register length,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  AllocateInNewSpace(ConsString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);
  InitializeNewString(result,
                      length,
                      Heap::kConsAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  AllocateInNewSpace(SlicedString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiSlicedString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  AllocateInNewSpace(SlicedString::kSize,
                     result,
                     scratch1,
                     scratch2,
                     gc_required,
                     TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


// Allocates a heap number or jumps to the label if the young space is full and
// a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Register heap_number_map,
                                        Label* need_gc) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  AllocateInNewSpace(HeapNumber::kSize,
                     result,
                     scratch1,
                     scratch2,
                     need_gc,
                     TAG_OBJECT);

  // Store heap number map in the allocated object.
  AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  sw(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset));
}


void MacroAssembler::AllocateHeapNumberWithValue(Register result,
                                                 FPURegister value,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  LoadRoot(t8, Heap::kHeapNumberMapRootIndex);
  AllocateHeapNumber(result, scratch1, scratch2, t8, gc_required);
  sdc1(value, FieldMemOperand(result, HeapNumber::kValueOffset));
}


// Copies a fixed number of fields of heap objects from src to dst.
void MacroAssembler::CopyFields(Register dst,
                                Register src,
                                RegList temps,
                                int field_count) {
  ASSERT((temps & dst.bit()) == 0);
  ASSERT((temps & src.bit()) == 0);
  // Primitive implementation using only one temporary register.

  Register tmp = no_reg;
  // Find a temp register in temps list.
  for (int i = 0; i < kNumRegisters; i++) {
    if ((temps & (1 << i)) != 0) {
      tmp.code_ = i;
      break;
    }
  }
  ASSERT(!tmp.is(no_reg));

  for (int i = 0; i < field_count; i++) {
    lw(tmp, FieldMemOperand(src, i * kPointerSize));
    sw(tmp, FieldMemOperand(dst, i * kPointerSize));
  }
}


void MacroAssembler::CopyBytes(Register src,
                               Register dst,
                               Register length,
                               Register scratch) {
  Label align_loop, align_loop_1, word_loop, byte_loop, byte_loop_1, done;

  // Align src before copying in word size chunks.
  bind(&align_loop);
  Branch(&done, eq, length, Operand(zero_reg));
  bind(&align_loop_1);
  And(scratch, src, kPointerSize - 1);
  Branch(&word_loop, eq, scratch, Operand(zero_reg));
  lbu(scratch, MemOperand(src));
  Addu(src, src, 1);
  sb(scratch, MemOperand(dst));
  Addu(dst, dst, 1);
  Subu(length, length, Operand(1));
  Branch(&byte_loop_1, ne, length, Operand(zero_reg));

  // Copy bytes in word size chunks.
  bind(&word_loop);
  if (emit_debug_code()) {
    And(scratch, src, kPointerSize - 1);
    Assert(eq, "Expecting alignment for CopyBytes",
        scratch, Operand(zero_reg));
  }
  Branch(&byte_loop, lt, length, Operand(kPointerSize));
  lw(scratch, MemOperand(src));
  Addu(src, src, kPointerSize);

  // TODO(kalmard) check if this can be optimized to use sw in most cases.
  // Can't use unaligned access - copy byte by byte.
  sb(scratch, MemOperand(dst, 0));
  srl(scratch, scratch, 8);
  sb(scratch, MemOperand(dst, 1));
  srl(scratch, scratch, 8);
  sb(scratch, MemOperand(dst, 2));
  srl(scratch, scratch, 8);
  sb(scratch, MemOperand(dst, 3));
  Addu(dst, dst, 4);

  Subu(length, length, Operand(kPointerSize));
  Branch(&word_loop);

  // Copy the last bytes if any left.
  bind(&byte_loop);
  Branch(&done, eq, length, Operand(zero_reg));
  bind(&byte_loop_1);
  lbu(scratch, MemOperand(src));
  Addu(src, src, 1);
  sb(scratch, MemOperand(dst));
  Addu(dst, dst, 1);
  Subu(length, length, Operand(1));
  Branch(&byte_loop_1, ne, length, Operand(zero_reg));
  bind(&done);
}


void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label loop, entry;
  Branch(&entry);
  bind(&loop);
  sw(filler, MemOperand(start_offset));
  Addu(start_offset, start_offset, kPointerSize);
  bind(&entry);
  Branch(&loop, lt, start_offset, Operand(end_offset));
}


void MacroAssembler::CheckFastElements(Register map,
                                       Register scratch,
                                       Label* fail) {
  STATIC_ASSERT(FAST_SMI_ONLY_ELEMENTS == 0);
  STATIC_ASSERT(FAST_ELEMENTS == 1);
  lbu(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  Branch(fail, hi, scratch, Operand(Map::kMaximumBitField2FastElementValue));
}


void MacroAssembler::CheckFastObjectElements(Register map,
                                             Register scratch,
                                             Label* fail) {
  STATIC_ASSERT(FAST_SMI_ONLY_ELEMENTS == 0);
  STATIC_ASSERT(FAST_ELEMENTS == 1);
  lbu(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  Branch(fail, ls, scratch,
         Operand(Map::kMaximumBitField2FastSmiOnlyElementValue));
  Branch(fail, hi, scratch,
         Operand(Map::kMaximumBitField2FastElementValue));
}


void MacroAssembler::CheckFastSmiOnlyElements(Register map,
                                              Register scratch,
                                              Label* fail) {
  STATIC_ASSERT(FAST_SMI_ONLY_ELEMENTS == 0);
  lbu(scratch, FieldMemOperand(map, Map::kBitField2Offset));
  Branch(fail, hi, scratch,
         Operand(Map::kMaximumBitField2FastSmiOnlyElementValue));
}


void MacroAssembler::StoreNumberToDoubleElements(Register value_reg,
                                                 Register key_reg,
                                                 Register receiver_reg,
                                                 Register elements_reg,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Register scratch3,
                                                 Register scratch4,
                                                 Label* fail) {
  Label smi_value, maybe_nan, have_double_value, is_nan, done;
  Register mantissa_reg = scratch2;
  Register exponent_reg = scratch3;

  // Handle smi values specially.
  JumpIfSmi(value_reg, &smi_value);

  // Ensure that the object is a heap number
  CheckMap(value_reg,
           scratch1,
           Heap::kHeapNumberMapRootIndex,
           fail,
           DONT_DO_SMI_CHECK);

  // Check for nan: all NaN values have a value greater (signed) than 0x7ff00000
  // in the exponent.
  li(scratch1, Operand(kNaNOrInfinityLowerBoundUpper32));
  lw(exponent_reg, FieldMemOperand(value_reg, HeapNumber::kExponentOffset));
  Branch(&maybe_nan, ge, exponent_reg, Operand(scratch1));

  lw(mantissa_reg, FieldMemOperand(value_reg, HeapNumber::kMantissaOffset));

  bind(&have_double_value);
  sll(scratch1, key_reg, kDoubleSizeLog2 - kSmiTagSize);
  Addu(scratch1, scratch1, elements_reg);
  sw(mantissa_reg, FieldMemOperand(scratch1, FixedDoubleArray::kHeaderSize));
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  sw(exponent_reg, FieldMemOperand(scratch1, offset));
  jmp(&done);

  bind(&maybe_nan);
  // Could be NaN or Infinity. If fraction is not zero, it's NaN, otherwise
  // it's an Infinity, and the non-NaN code path applies.
  Branch(&is_nan, gt, exponent_reg, Operand(scratch1));
  lw(mantissa_reg, FieldMemOperand(value_reg, HeapNumber::kMantissaOffset));
  Branch(&have_double_value, eq, mantissa_reg, Operand(zero_reg));
  bind(&is_nan);
  // Load canonical NaN for storing into the double array.
  uint64_t nan_int64 = BitCast<uint64_t>(
      FixedDoubleArray::canonical_not_the_hole_nan_as_double());
  li(mantissa_reg, Operand(static_cast<uint32_t>(nan_int64)));
  li(exponent_reg, Operand(static_cast<uint32_t>(nan_int64 >> 32)));
  jmp(&have_double_value);

  bind(&smi_value);
  Addu(scratch1, elements_reg,
      Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag));
  sll(scratch2, key_reg, kDoubleSizeLog2 - kSmiTagSize);
  Addu(scratch1, scratch1, scratch2);
  // scratch1 is now effective address of the double element

  FloatingPointHelper::Destination destination;
  if (CpuFeatures::IsSupported(FPU)) {
    destination = FloatingPointHelper::kFPURegisters;
  } else {
    destination = FloatingPointHelper::kCoreRegisters;
  }

  Register untagged_value = receiver_reg;
  SmiUntag(untagged_value, value_reg);
  FloatingPointHelper::ConvertIntToDouble(this,
                                          untagged_value,
                                          destination,
                                          f0,
                                          mantissa_reg,
                                          exponent_reg,
                                          scratch4,
                                          f2);
  if (destination == FloatingPointHelper::kFPURegisters) {
    CpuFeatures::Scope scope(FPU);
    sdc1(f0, MemOperand(scratch1, 0));
  } else {
    sw(mantissa_reg, MemOperand(scratch1, 0));
    sw(exponent_reg, MemOperand(scratch1, Register::kSizeInBytes));
  }
  bind(&done);
}


void MacroAssembler::CompareMapAndBranch(Register obj,
                                         Register scratch,
                                         Handle<Map> map,
                                         Label* early_success,
                                         Condition cond,
                                         Label* branch_to,
                                         CompareMapMode mode) {
  lw(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  Operand right = Operand(map);
  if (mode == ALLOW_ELEMENT_TRANSITION_MAPS) {
    Map* transitioned_fast_element_map(
        map->LookupElementsTransitionMap(FAST_ELEMENTS, NULL));
    ASSERT(transitioned_fast_element_map == NULL ||
           map->elements_kind() != FAST_ELEMENTS);
    if (transitioned_fast_element_map != NULL) {
      Branch(early_success, eq, scratch, right);
      right = Operand(Handle<Map>(transitioned_fast_element_map));
    }

    Map* transitioned_double_map(
        map->LookupElementsTransitionMap(FAST_DOUBLE_ELEMENTS, NULL));
    ASSERT(transitioned_double_map == NULL ||
           map->elements_kind() == FAST_SMI_ONLY_ELEMENTS);
    if (transitioned_double_map != NULL) {
      Branch(early_success, eq, scratch, right);
      right = Operand(Handle<Map>(transitioned_double_map));
    }
  }

  Branch(branch_to, cond, scratch, right);
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type,
                              CompareMapMode mode) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  Label success;
  CompareMapAndBranch(obj, scratch, map, &success, ne, fail, mode);
  bind(&success);
}


void MacroAssembler::DispatchMap(Register obj,
                                 Register scratch,
                                 Handle<Map> map,
                                 Handle<Code> success,
                                 SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  lw(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  Jump(success, RelocInfo::CODE_TARGET, eq, scratch, Operand(map));
  bind(&fail);
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Heap::RootListIndex index,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  lw(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  LoadRoot(at, index);
  Branch(fail, ne, scratch, Operand(at));
}


void MacroAssembler::GetCFunctionDoubleResult(const DoubleRegister dst) {
  CpuFeatures::Scope scope(FPU);
  if (IsMipsSoftFloatABI) {
    Move(dst, v0, v1);
  } else {
    Move(dst, f0);  // Reg f0 is o32 ABI FP return value.
  }
}


void MacroAssembler::SetCallCDoubleArguments(DoubleRegister dreg) {
  CpuFeatures::Scope scope(FPU);
  if (!IsMipsSoftFloatABI) {
    Move(f12, dreg);
  } else {
    Move(a0, a1, dreg);
  }
}


void MacroAssembler::SetCallCDoubleArguments(DoubleRegister dreg1,
                                             DoubleRegister dreg2) {
  CpuFeatures::Scope scope(FPU);
  if (!IsMipsSoftFloatABI) {
    if (dreg2.is(f12)) {
      ASSERT(!dreg1.is(f14));
      Move(f14, dreg2);
      Move(f12, dreg1);
    } else {
      Move(f12, dreg1);
      Move(f14, dreg2);
    }
  } else {
    Move(a0, a1, dreg1);
    Move(a2, a3, dreg2);
  }
}


void MacroAssembler::SetCallCDoubleArguments(DoubleRegister dreg,
                                             Register reg) {
  CpuFeatures::Scope scope(FPU);
  if (!IsMipsSoftFloatABI) {
    Move(f12, dreg);
    Move(a2, reg);
  } else {
    Move(a2, reg);
    Move(a0, a1, dreg);
  }
}


void MacroAssembler::SetCallKind(Register dst, CallKind call_kind) {
  // This macro takes the dst register to make the code more readable
  // at the call sites. However, the dst register has to be t1 to
  // follow the calling convention which requires the call type to be
  // in t1.
  ASSERT(dst.is(t1));
  if (call_kind == CALL_AS_FUNCTION) {
    li(dst, Operand(Smi::FromInt(1)));
  } else {
    li(dst, Operand(Smi::FromInt(0)));
  }
}


// -----------------------------------------------------------------------------
// JavaScript invokes.

void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_reg,
                                    Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  a0: actual arguments count
  //  a1: function (passed through to callee)
  //  a2: expected arguments count
  //  a3: callee code entry

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.
  ASSERT(actual.is_immediate() || actual.reg().is(a0));
  ASSERT(expected.is_immediate() || expected.reg().is(a2));
  ASSERT((!code_constant.is_null() && code_reg.is(no_reg)) || code_reg.is(a3));

  if (expected.is_immediate()) {
    ASSERT(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      li(a0, Operand(actual.immediate()));
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        li(a2, Operand(expected.immediate()));
      }
    }
  } else if (actual.is_immediate()) {
    Branch(&regular_invoke, eq, expected.reg(), Operand(actual.immediate()));
    li(a0, Operand(actual.immediate()));
  } else {
    Branch(&regular_invoke, eq, expected.reg(), Operand(actual.reg()));
  }

  if (!definitely_matches) {
    if (!code_constant.is_null()) {
      li(a3, Operand(code_constant));
      addiu(a3, a3, Code::kHeaderSize - kHeapObjectTag);
    }

    Handle<Code> adaptor =
        isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      SetCallKind(t1, call_kind);
      Call(adaptor);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        Branch(done);
      }
    } else {
      SetCallKind(t1, call_kind);
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&regular_invoke);
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
  InvokePrologue(expected, actual, Handle<Code>::null(), code,
                 &done, &definitely_mismatches, flag,
                 call_wrapper, call_kind);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      SetCallKind(t1, call_kind);
      Call(code);
      call_wrapper.AfterCall();
    } else {
      ASSERT(flag == JUMP_FUNCTION);
      SetCallKind(t1, call_kind);
      Jump(code);
    }
    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
  }
}


void MacroAssembler::InvokeCode(Handle<Code> code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                RelocInfo::Mode rmode,
                                InvokeFlag flag,
                                CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  Label done;

  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, code, no_reg,
                 &done, &definitely_mismatches, flag,
                 NullCallWrapper(), call_kind);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      SetCallKind(t1, call_kind);
      Call(code, rmode);
    } else {
      SetCallKind(t1, call_kind);
      Jump(code, rmode);
    }
    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
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

  // Contract with called JS functions requires that function is passed in a1.
  ASSERT(function.is(a1));
  Register expected_reg = a2;
  Register code_reg = a3;

  lw(code_reg, FieldMemOperand(a1, JSFunction::kSharedFunctionInfoOffset));
  lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));
  lw(expected_reg,
      FieldMemOperand(code_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));
  sra(expected_reg, expected_reg, kSmiTagSize);
  lw(code_reg, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag, call_wrapper, call_kind);
}


void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper,
                                    CallKind call_kind) {
  // You can't call a function without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  // Get the function and setup the context.
  LoadHeapObject(a1, function);
  lw(cp, FieldMemOperand(a1, JSFunction::kContextOffset));

  ParameterCount expected(function->shared()->formal_parameter_count());
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  lw(a3, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
  InvokeCode(a3, expected, actual, flag, call_wrapper, call_kind);
}


void MacroAssembler::IsObjectJSObjectType(Register heap_object,
                                          Register map,
                                          Register scratch,
                                          Label* fail) {
  lw(map, FieldMemOperand(heap_object, HeapObject::kMapOffset));
  IsInstanceJSObjectType(map, scratch, fail);
}


void MacroAssembler::IsInstanceJSObjectType(Register map,
                                            Register scratch,
                                            Label* fail) {
  lbu(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  Branch(fail, lt, scratch, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  Branch(fail, gt, scratch, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
}


void MacroAssembler::IsObjectJSStringType(Register object,
                                          Register scratch,
                                          Label* fail) {
  ASSERT(kNotStringTag != 0);

  lw(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  lbu(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  And(scratch, scratch, Operand(kIsNotStringMask));
  Branch(fail, ne, scratch, Operand(zero_reg));
}


// ---------------------------------------------------------------------------
// Support functions.


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss,
                                             bool miss_on_bound_function) {
  // Check that the receiver isn't a smi.
  JumpIfSmi(function, miss);

  // Check that the function really is a function.  Load map into result reg.
  GetObjectType(function, result, scratch);
  Branch(miss, ne, scratch, Operand(JS_FUNCTION_TYPE));

  if (miss_on_bound_function) {
    lw(scratch,
       FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    lw(scratch,
       FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    And(scratch, scratch,
        Operand(Smi::FromInt(1 << SharedFunctionInfo::kBoundFunction)));
    Branch(miss, ne, scratch, Operand(zero_reg));
  }

  // Make sure that the function has an instance prototype.
  Label non_instance;
  lbu(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
  And(scratch, scratch, Operand(1 << Map::kHasNonInstancePrototype));
  Branch(&non_instance, ne, scratch, Operand(zero_reg));

  // Get the prototype or initial map from the function.
  lw(result,
     FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  LoadRoot(t8, Heap::kTheHoleValueRootIndex);
  Branch(miss, eq, result, Operand(t8));

  // If the function does not have an initial map, we're done.
  Label done;
  GetObjectType(result, scratch, scratch);
  Branch(&done, ne, scratch, Operand(MAP_TYPE));

  // Get the prototype from the initial map.
  lw(result, FieldMemOperand(result, Map::kPrototypeOffset));
  jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  bind(&non_instance);
  lw(result, FieldMemOperand(result, Map::kConstructorOffset));

  // All done.
  bind(&done);
}


void MacroAssembler::GetObjectType(Register object,
                                   Register map,
                                   Register type_reg) {
  lw(map, FieldMemOperand(object, HeapObject::kMapOffset));
  lbu(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
}


// -----------------------------------------------------------------------------
// Runtime calls.

void MacroAssembler::CallStub(CodeStub* stub,
                              Condition cond,
                              Register r1,
                              const Operand& r2,
                              BranchDelaySlot bd) {
  ASSERT(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, kNoASTId, cond, r1, r2, bd);
}


void MacroAssembler::TailCallStub(CodeStub* stub) {
  ASSERT(allow_stub_calls_ || stub->CompilingCallsToThisStubIsGCSafe());
  Jump(stub->GetCode(), RelocInfo::CODE_TARGET);
}


static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}


void MacroAssembler::CallApiFunctionAndReturn(ExternalReference function,
                                              int stack_space) {
  ExternalReference next_address =
      ExternalReference::handle_scope_next_address();
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
      ExternalReference::handle_scope_limit_address(),
      next_address);
  const int kLevelOffset = AddressOffset(
      ExternalReference::handle_scope_level_address(),
      next_address);

  // Allocate HandleScope in callee-save registers.
  li(s3, Operand(next_address));
  lw(s0, MemOperand(s3, kNextOffset));
  lw(s1, MemOperand(s3, kLimitOffset));
  lw(s2, MemOperand(s3, kLevelOffset));
  Addu(s2, s2, Operand(1));
  sw(s2, MemOperand(s3, kLevelOffset));

  // The O32 ABI requires us to pass a pointer in a0 where the returned struct
  // (4 bytes) will be placed. This is also built into the Simulator.
  // Set up the pointer to the returned value (a0). It was allocated in
  // EnterExitFrame.
  addiu(a0, fp, ExitFrameConstants::kStackSpaceOffset);

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
  DirectCEntryStub stub;
  stub.GenerateCall(this, function);

  // As mentioned above, on MIPS a pointer is returned - we need to dereference
  // it to get the actual return value (which is also a pointer).
  lw(v0, MemOperand(v0));

  Label promote_scheduled_exception;
  Label delete_allocated_handles;
  Label leave_exit_frame;

  // If result is non-zero, dereference to get the result value
  // otherwise set it to undefined.
  Label skip;
  LoadRoot(a0, Heap::kUndefinedValueRootIndex);
  Branch(&skip, eq, v0, Operand(zero_reg));
  lw(a0, MemOperand(v0));
  bind(&skip);
  mov(v0, a0);

  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  sw(s0, MemOperand(s3, kNextOffset));
  if (emit_debug_code()) {
    lw(a1, MemOperand(s3, kLevelOffset));
    Check(eq, "Unexpected level after return from api call", a1, Operand(s2));
  }
  Subu(s2, s2, Operand(1));
  sw(s2, MemOperand(s3, kLevelOffset));
  lw(at, MemOperand(s3, kLimitOffset));
  Branch(&delete_allocated_handles, ne, s1, Operand(at));

  // Check if the function scheduled an exception.
  bind(&leave_exit_frame);
  LoadRoot(t0, Heap::kTheHoleValueRootIndex);
  li(at, Operand(ExternalReference::scheduled_exception_address(isolate())));
  lw(t1, MemOperand(at));
  Branch(&promote_scheduled_exception, ne, t0, Operand(t1));
  li(s0, Operand(stack_space));
  LeaveExitFrame(false, s0, true);

  bind(&promote_scheduled_exception);
  TailCallExternalReference(
      ExternalReference(Runtime::kPromoteScheduledException, isolate()),
      0,
      1);

  // HandleScope limit has changed. Delete allocated extensions.
  bind(&delete_allocated_handles);
  sw(s1, MemOperand(s3, kLimitOffset));
  mov(s0, v0);
  mov(a0, v0);
  PrepareCallCFunction(1, s1);
  li(a0, Operand(ExternalReference::isolate_address()));
  CallCFunction(ExternalReference::delete_handle_scope_extensions(isolate()),
      1);
  mov(v0, s0);
  jmp(&leave_exit_frame);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  if (!has_frame_ && stub->SometimesSetsUpAFrame()) return false;
  return allow_stub_calls_ || stub->CompilingCallsToThisStubIsGCSafe();
}


void MacroAssembler::IllegalOperation(int num_arguments) {
  if (num_arguments > 0) {
    addiu(sp, sp, num_arguments * kPointerSize);
  }
  LoadRoot(v0, Heap::kUndefinedValueRootIndex);
}


void MacroAssembler::IndexFromHash(Register hash,
                                   Register index) {
  // If the hash field contains an array index pick it out. The assert checks
  // that the constants for the maximum number of digits for an array index
  // cached in the hash field and the number of bits reserved for it does not
  // conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key.  kArrayIndexValueMask has zeros in
  // the low kHashShift bits.
  STATIC_ASSERT(kSmiTag == 0);
  Ext(hash, hash, String::kHashShift, String::kArrayIndexValueBits);
  sll(index, hash, kSmiTagSize);
}


void MacroAssembler::ObjectToDoubleFPURegister(Register object,
                                               FPURegister result,
                                               Register scratch1,
                                               Register scratch2,
                                               Register heap_number_map,
                                               Label* not_number,
                                               ObjectToDoubleFlags flags) {
  Label done;
  if ((flags & OBJECT_NOT_SMI) == 0) {
    Label not_smi;
    JumpIfNotSmi(object, &not_smi);
    // Remove smi tag and convert to double.
    sra(scratch1, object, kSmiTagSize);
    mtc1(scratch1, result);
    cvt_d_w(result, result);
    Branch(&done);
    bind(&not_smi);
  }
  // Check for heap number and load double value from it.
  lw(scratch1, FieldMemOperand(object, HeapObject::kMapOffset));
  Branch(not_number, ne, scratch1, Operand(heap_number_map));

  if ((flags & AVOID_NANS_AND_INFINITIES) != 0) {
    // If exponent is all ones the number is either a NaN or +/-Infinity.
    Register exponent = scratch1;
    Register mask_reg = scratch2;
    lw(exponent, FieldMemOperand(object, HeapNumber::kExponentOffset));
    li(mask_reg, HeapNumber::kExponentMask);

    And(exponent, exponent, mask_reg);
    Branch(not_number, eq, exponent, Operand(mask_reg));
  }
  ldc1(result, FieldMemOperand(object, HeapNumber::kValueOffset));
  bind(&done);
}


void MacroAssembler::SmiToDoubleFPURegister(Register smi,
                                            FPURegister value,
                                            Register scratch1) {
  sra(scratch1, smi, kSmiTagSize);
  mtc1(scratch1, value);
  cvt_d_w(value, value);
}


void MacroAssembler::AdduAndCheckForOverflow(Register dst,
                                             Register left,
                                             Register right,
                                             Register overflow_dst,
                                             Register scratch) {
  ASSERT(!dst.is(overflow_dst));
  ASSERT(!dst.is(scratch));
  ASSERT(!overflow_dst.is(scratch));
  ASSERT(!overflow_dst.is(left));
  ASSERT(!overflow_dst.is(right));

  if (left.is(right) && dst.is(left)) {
    ASSERT(!dst.is(t9));
    ASSERT(!scratch.is(t9));
    ASSERT(!left.is(t9));
    ASSERT(!right.is(t9));
    ASSERT(!overflow_dst.is(t9));
    mov(t9, right);
    right = t9;
  }

  if (dst.is(left)) {
    mov(scratch, left);  // Preserve left.
    addu(dst, left, right);  // Left is overwritten.
    xor_(scratch, dst, scratch);  // Original left.
    xor_(overflow_dst, dst, right);
    and_(overflow_dst, overflow_dst, scratch);
  } else if (dst.is(right)) {
    mov(scratch, right);  // Preserve right.
    addu(dst, left, right);  // Right is overwritten.
    xor_(scratch, dst, scratch);  // Original right.
    xor_(overflow_dst, dst, left);
    and_(overflow_dst, overflow_dst, scratch);
  } else {
    addu(dst, left, right);
    xor_(overflow_dst, dst, left);
    xor_(scratch, dst, right);
    and_(overflow_dst, scratch, overflow_dst);
  }
}


void MacroAssembler::SubuAndCheckForOverflow(Register dst,
                                             Register left,
                                             Register right,
                                             Register overflow_dst,
                                             Register scratch) {
  ASSERT(!dst.is(overflow_dst));
  ASSERT(!dst.is(scratch));
  ASSERT(!overflow_dst.is(scratch));
  ASSERT(!overflow_dst.is(left));
  ASSERT(!overflow_dst.is(right));
  ASSERT(!scratch.is(left));
  ASSERT(!scratch.is(right));

  // This happens with some crankshaft code. Since Subu works fine if
  // left == right, let's not make that restriction here.
  if (left.is(right)) {
    mov(dst, zero_reg);
    mov(overflow_dst, zero_reg);
    return;
  }

  if (dst.is(left)) {
    mov(scratch, left);  // Preserve left.
    subu(dst, left, right);  // Left is overwritten.
    xor_(overflow_dst, dst, scratch);  // scratch is original left.
    xor_(scratch, scratch, right);  // scratch is original left.
    and_(overflow_dst, scratch, overflow_dst);
  } else if (dst.is(right)) {
    mov(scratch, right);  // Preserve right.
    subu(dst, left, right);  // Right is overwritten.
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, scratch);  // Original right.
    and_(overflow_dst, scratch, overflow_dst);
  } else {
    subu(dst, left, right);
    xor_(overflow_dst, dst, left);
    xor_(scratch, left, right);
    and_(overflow_dst, scratch, overflow_dst);
  }
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments) {
  // All parameters are on the stack. v0 has the return value after call.

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
  PrepareCEntryArgs(num_arguments);
  PrepareCEntryFunction(ExternalReference(f, isolate()));
  CEntryStub stub(1);
  CallStub(&stub);
}


void MacroAssembler::CallRuntimeSaveDoubles(Runtime::FunctionId id) {
  const Runtime::Function* function = Runtime::FunctionForId(id);
  PrepareCEntryArgs(function->nargs);
  PrepareCEntryFunction(ExternalReference(function, isolate()));
  CEntryStub stub(1, kSaveFPRegs);
  CallStub(&stub);
}


void MacroAssembler::CallRuntime(Runtime::FunctionId fid, int num_arguments) {
  CallRuntime(Runtime::FunctionForId(fid), num_arguments);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments,
                                           BranchDelaySlot bd) {
  PrepareCEntryArgs(num_arguments);
  PrepareCEntryFunction(ext);

  CEntryStub stub(1);
  CallStub(&stub, al, zero_reg, Operand(zero_reg), bd);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  PrepareCEntryArgs(num_arguments);
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid, isolate()),
                            num_arguments,
                            result_size);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin,
                                             BranchDelaySlot bd) {
  PrepareCEntryFunction(builtin);
  CEntryStub stub(1);
  Jump(stub.GetCode(),
       RelocInfo::CODE_TARGET,
       al,
       zero_reg,
       Operand(zero_reg),
       bd);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  ASSERT(flag == JUMP_FUNCTION || has_frame());

  GetBuiltinEntry(t9, id);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(t9));
    SetCallKind(t1, CALL_AS_METHOD);
    Call(t9);
    call_wrapper.AfterCall();
  } else {
    ASSERT(flag == JUMP_FUNCTION);
    SetCallKind(t1, CALL_AS_METHOD);
    Jump(t9);
  }
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  lw(target, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  lw(target, FieldMemOperand(target, GlobalObject::kBuiltinsOffset));
  // Load the JavaScript builtin function from the builtins object.
  lw(target, FieldMemOperand(target,
                          JSBuiltinsObject::OffsetOfFunctionWithId(id)));
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  ASSERT(!target.is(a1));
  GetBuiltinFunction(a1, id);
  // Load the code entry point from the builtins object.
  lw(target, FieldMemOperand(a1, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch1, Operand(value));
    li(scratch2, Operand(ExternalReference(counter)));
    sw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch2, Operand(ExternalReference(counter)));
    lw(scratch1, MemOperand(scratch2));
    Addu(scratch1, scratch1, Operand(value));
    sw(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  ASSERT(value > 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    li(scratch2, Operand(ExternalReference(counter)));
    lw(scratch1, MemOperand(scratch2));
    Subu(scratch1, scratch1, Operand(value));
    sw(scratch1, MemOperand(scratch2));
  }
}


// -----------------------------------------------------------------------------
// Debugging.

void MacroAssembler::Assert(Condition cc, const char* msg,
                            Register rs, Operand rt) {
  if (emit_debug_code())
    Check(cc, msg, rs, rt);
}


void MacroAssembler::AssertRegisterIsRoot(Register reg,
                                          Heap::RootListIndex index) {
  if (emit_debug_code()) {
    LoadRoot(at, index);
    Check(eq, "Register did not match expected root", reg, Operand(at));
  }
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    ASSERT(!elements.is(at));
    Label ok;
    push(elements);
    lw(elements, FieldMemOperand(elements, HeapObject::kMapOffset));
    LoadRoot(at, Heap::kFixedArrayMapRootIndex);
    Branch(&ok, eq, elements, Operand(at));
    LoadRoot(at, Heap::kFixedDoubleArrayMapRootIndex);
    Branch(&ok, eq, elements, Operand(at));
    LoadRoot(at, Heap::kFixedCOWArrayMapRootIndex);
    Branch(&ok, eq, elements, Operand(at));
    Abort("JSObject with fast elements map has slow elements");
    bind(&ok);
    pop(elements);
  }
}


void MacroAssembler::Check(Condition cc, const char* msg,
                           Register rs, Operand rt) {
  Label L;
  Branch(&L, cc, rs, rt);
  Abort(msg);
  // Will not return here.
  bind(&L);
}


void MacroAssembler::Abort(const char* msg) {
  Label abort_start;
  bind(&abort_start);
  // We want to pass the msg string like a smi to avoid GC
  // problems, however msg is not guaranteed to be aligned
  // properly. Instead, we pass an aligned pointer that is
  // a proper v8 smi, but also pass the alignment difference
  // from the real pointer as a smi.
  intptr_t p1 = reinterpret_cast<intptr_t>(msg);
  intptr_t p0 = (p1 & ~kSmiTagMask) + kSmiTag;
  ASSERT(reinterpret_cast<Object*>(p0)->IsSmi());
#ifdef DEBUG
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }
#endif

  li(a0, Operand(p0));
  push(a0);
  li(a0, Operand(Smi::FromInt(p1 - p0)));
  push(a0);
  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kAbort, 2);
  } else {
    CallRuntime(Runtime::kAbort, 2);
  }
  // Will not return here.
  if (is_trampoline_pool_blocked()) {
    // If the calling code cares about the exact number of
    // instructions generated, we insert padding here to keep the size
    // of the Abort macro constant.
    // Currently in debug mode with debug_code enabled the number of
    // generated instructions is 14, so we use this as a maximum value.
    static const int kExpectedAbortInstructions = 14;
    int abort_instructions = InstructionsGeneratedSince(&abort_start);
    ASSERT(abort_instructions <= kExpectedAbortInstructions);
    while (abort_instructions++ < kExpectedAbortInstructions) {
      nop();
    }
  }
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    lw(dst, MemOperand(cp, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      lw(dst, MemOperand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in esi).
    Move(dst, cp);
  }
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind,
    ElementsKind transitioned_kind,
    Register map_in_out,
    Register scratch,
    Label* no_map_match) {
  // Load the global or builtins object from the current context.
  lw(scratch, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  lw(scratch, FieldMemOperand(scratch, GlobalObject::kGlobalContextOffset));

  // Check that the function's map is the same as the expected cached map.
  int expected_index =
      Context::GetContextMapIndexFromElementsKind(expected_kind);
  lw(at, MemOperand(scratch, Context::SlotOffset(expected_index)));
  Branch(no_map_match, ne, map_in_out, Operand(at));

  // Use the transitioned cached map.
  int trans_index =
      Context::GetContextMapIndexFromElementsKind(transitioned_kind);
  lw(map_in_out, MemOperand(scratch, Context::SlotOffset(trans_index)));
}


void MacroAssembler::LoadInitialArrayMap(
    Register function_in, Register scratch, Register map_out) {
  ASSERT(!function_in.is(map_out));
  Label done;
  lw(map_out, FieldMemOperand(function_in,
                              JSFunction::kPrototypeOrInitialMapOffset));
  if (!FLAG_smi_only_arrays) {
    LoadTransitionedArrayMapConditional(FAST_SMI_ONLY_ELEMENTS,
                                        FAST_ELEMENTS,
                                        map_out,
                                        scratch,
                                        &done);
  }
  bind(&done);
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  lw(function, MemOperand(cp, Context::SlotOffset(Context::GLOBAL_INDEX)));
  // Load the global context from the global or builtins object.
  lw(function, FieldMemOperand(function,
                               GlobalObject::kGlobalContextOffset));
  // Load the function from the global context.
  lw(function, MemOperand(function, Context::SlotOffset(index)));
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map,
                                                  Register scratch) {
  // Load the initial map. The global functions all have initial maps.
  lw(map, FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, scratch, Heap::kMetaMapRootIndex, &fail, DO_SMI_CHECK);
    Branch(&ok);
    bind(&fail);
    Abort("Global functions must have initial map");
    bind(&ok);
  }
}


void MacroAssembler::EnterFrame(StackFrame::Type type) {
  addiu(sp, sp, -5 * kPointerSize);
  li(t8, Operand(Smi::FromInt(type)));
  li(t9, Operand(CodeObject()), CONSTANT_SIZE);
  sw(ra, MemOperand(sp, 4 * kPointerSize));
  sw(fp, MemOperand(sp, 3 * kPointerSize));
  sw(cp, MemOperand(sp, 2 * kPointerSize));
  sw(t8, MemOperand(sp, 1 * kPointerSize));
  sw(t9, MemOperand(sp, 0 * kPointerSize));
  addiu(fp, sp, 3 * kPointerSize);
}


void MacroAssembler::LeaveFrame(StackFrame::Type type) {
  mov(sp, fp);
  lw(fp, MemOperand(sp, 0 * kPointerSize));
  lw(ra, MemOperand(sp, 1 * kPointerSize));
  addiu(sp, sp, 2 * kPointerSize);
}


void MacroAssembler::EnterExitFrame(bool save_doubles,
                                    int stack_space) {
  // Set up the frame structure on the stack.
  STATIC_ASSERT(2 * kPointerSize == ExitFrameConstants::kCallerSPDisplacement);
  STATIC_ASSERT(1 * kPointerSize == ExitFrameConstants::kCallerPCOffset);
  STATIC_ASSERT(0 * kPointerSize == ExitFrameConstants::kCallerFPOffset);

  // This is how the stack will look:
  // fp + 2 (==kCallerSPDisplacement) - old stack's end
  // [fp + 1 (==kCallerPCOffset)] - saved old ra
  // [fp + 0 (==kCallerFPOffset)] - saved old fp
  // [fp - 1 (==kSPOffset)] - sp of the called function
  // [fp - 2 (==kCodeOffset)] - CodeObject
  // fp - (2 + stack_space + alignment) == sp == [fp - kSPOffset] - top of the
  //   new stack (will contain saved ra)

  // Save registers.
  addiu(sp, sp, -4 * kPointerSize);
  sw(ra, MemOperand(sp, 3 * kPointerSize));
  sw(fp, MemOperand(sp, 2 * kPointerSize));
  addiu(fp, sp, 2 * kPointerSize);  // Set up new frame pointer.

  if (emit_debug_code()) {
    sw(zero_reg, MemOperand(fp, ExitFrameConstants::kSPOffset));
  }

  // Accessed from ExitFrame::code_slot.
  li(t8, Operand(CodeObject()), CONSTANT_SIZE);
  sw(t8, MemOperand(fp, ExitFrameConstants::kCodeOffset));

  // Save the frame pointer and the context in top.
  li(t8, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  sw(fp, MemOperand(t8));
  li(t8, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  sw(cp, MemOperand(t8));

  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  if (save_doubles) {
    // The stack  must be allign to 0 modulo 8 for stores with sdc1.
    ASSERT(kDoubleSize == frame_alignment);
    if (frame_alignment > 0) {
      ASSERT(IsPowerOf2(frame_alignment));
      And(sp, sp, Operand(-frame_alignment));  // Align stack.
    }
    int space = FPURegister::kNumRegisters * kDoubleSize;
    Subu(sp, sp, Operand(space));
    // Remember: we only need to save every 2nd double FPU value.
    for (int i = 0; i < FPURegister::kNumRegisters; i+=2) {
      FPURegister reg = FPURegister::from_code(i);
      sdc1(reg, MemOperand(sp, i * kDoubleSize));
    }
  }

  // Reserve place for the return address, stack space and an optional slot
  // (used by the DirectCEntryStub to hold the return value if a struct is
  // returned) and align the frame preparing for calling the runtime function.
  ASSERT(stack_space >= 0);
  Subu(sp, sp, Operand((stack_space + 2) * kPointerSize));
  if (frame_alignment > 0) {
    ASSERT(IsPowerOf2(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));  // Align stack.
  }

  // Set the exit frame sp value to point just before the return address
  // location.
  addiu(at, sp, kPointerSize);
  sw(at, MemOperand(fp, ExitFrameConstants::kSPOffset));
}


void MacroAssembler::LeaveExitFrame(bool save_doubles,
                                    Register argument_count,
                                    bool do_return) {
  // Optionally restore all double registers.
  if (save_doubles) {
    // Remember: we only need to restore every 2nd double FPU value.
    lw(t8, MemOperand(fp, ExitFrameConstants::kSPOffset));
    for (int i = 0; i < FPURegister::kNumRegisters; i+=2) {
      FPURegister reg = FPURegister::from_code(i);
      ldc1(reg, MemOperand(t8, i  * kDoubleSize + kPointerSize));
    }
  }

  // Clear top frame.
  li(t8, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  sw(zero_reg, MemOperand(t8));

  // Restore current context from top and clear it in debug mode.
  li(t8, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  lw(cp, MemOperand(t8));
#ifdef DEBUG
  sw(a3, MemOperand(t8));
#endif

  // Pop the arguments, restore registers, and return.
  mov(sp, fp);  // Respect ABI stack constraint.
  lw(fp, MemOperand(sp, ExitFrameConstants::kCallerFPOffset));
  lw(ra, MemOperand(sp, ExitFrameConstants::kCallerPCOffset));

  if (argument_count.is_valid()) {
    sll(t8, argument_count, kPointerSizeLog2);
    addu(sp, sp, t8);
  }

  if (do_return) {
    Ret(USE_DELAY_SLOT);
    // If returning, the instruction in the delay slot will be the addiu below.
  }
  addiu(sp, sp, 8);
}


void MacroAssembler::InitializeNewString(Register string,
                                         Register length,
                                         Heap::RootListIndex map_index,
                                         Register scratch1,
                                         Register scratch2) {
  sll(scratch1, length, kSmiTagSize);
  LoadRoot(scratch2, map_index);
  sw(scratch1, FieldMemOperand(string, String::kLengthOffset));
  li(scratch1, Operand(String::kEmptyHashField));
  sw(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));
  sw(scratch1, FieldMemOperand(string, String::kHashFieldOffset));
}


int MacroAssembler::ActivationFrameAlignment() {
#if defined(V8_HOST_ARCH_MIPS)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one Mips
  // platform for another Mips platform with a different alignment.
  return OS::ActivationFrameAlignment();
#else  // defined(V8_HOST_ARCH_MIPS)
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif  // defined(V8_HOST_ARCH_MIPS)
}


void MacroAssembler::AssertStackIsAligned() {
  if (emit_debug_code()) {
      const int frame_alignment = ActivationFrameAlignment();
      const int frame_alignment_mask = frame_alignment - 1;

      if (frame_alignment > kPointerSize) {
        Label alignment_as_expected;
        ASSERT(IsPowerOf2(frame_alignment));
        andi(at, sp, frame_alignment_mask);
        Branch(&alignment_as_expected, eq, at, Operand(zero_reg));
        // Don't use Check here, as it will call Runtime_Abort re-entering here.
        stop("Unexpected stack alignment");
        bind(&alignment_as_expected);
      }
    }
}


void MacroAssembler::JumpIfNotPowerOfTwoOrZero(
    Register reg,
    Register scratch,
    Label* not_power_of_two_or_zero) {
  Subu(scratch, reg, Operand(1));
  Branch(USE_DELAY_SLOT, not_power_of_two_or_zero, lt,
         scratch, Operand(zero_reg));
  and_(at, scratch, reg);  // In the delay slot.
  Branch(not_power_of_two_or_zero, ne, at, Operand(zero_reg));
}


void MacroAssembler::SmiTagCheckOverflow(Register reg, Register overflow) {
  ASSERT(!reg.is(overflow));
  mov(overflow, reg);  // Save original value.
  SmiTag(reg);
  xor_(overflow, overflow, reg);  // Overflow if (value ^ 2 * value) < 0.
}


void MacroAssembler::SmiTagCheckOverflow(Register dst,
                                         Register src,
                                         Register overflow) {
  if (dst.is(src)) {
    // Fall back to slower case.
    SmiTagCheckOverflow(dst, overflow);
  } else {
    ASSERT(!dst.is(src));
    ASSERT(!dst.is(overflow));
    ASSERT(!src.is(overflow));
    SmiTag(dst, src);
    xor_(overflow, dst, src);  // Overflow if (value ^ 2 * value) < 0.
  }
}


void MacroAssembler::UntagAndJumpIfSmi(Register dst,
                                       Register src,
                                       Label* smi_case) {
  JumpIfSmi(src, smi_case, at, USE_DELAY_SLOT);
  SmiUntag(dst, src);
}


void MacroAssembler::UntagAndJumpIfNotSmi(Register dst,
                                          Register src,
                                          Label* non_smi_case) {
  JumpIfNotSmi(src, non_smi_case, at, USE_DELAY_SLOT);
  SmiUntag(dst, src);
}

void MacroAssembler::JumpIfSmi(Register value,
                               Label* smi_label,
                               Register scratch,
                               BranchDelaySlot bd) {
  ASSERT_EQ(0, kSmiTag);
  andi(scratch, value, kSmiTagMask);
  Branch(bd, smi_label, eq, scratch, Operand(zero_reg));
}

void MacroAssembler::JumpIfNotSmi(Register value,
                                  Label* not_smi_label,
                                  Register scratch,
                                  BranchDelaySlot bd) {
  ASSERT_EQ(0, kSmiTag);
  andi(scratch, value, kSmiTagMask);
  Branch(bd, not_smi_label, ne, scratch, Operand(zero_reg));
}


void MacroAssembler::JumpIfNotBothSmi(Register reg1,
                                      Register reg2,
                                      Label* on_not_both_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  ASSERT_EQ(1, kSmiTagMask);
  or_(at, reg1, reg2);
  JumpIfNotSmi(at, on_not_both_smi);
}


void MacroAssembler::JumpIfEitherSmi(Register reg1,
                                     Register reg2,
                                     Label* on_either_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  ASSERT_EQ(1, kSmiTagMask);
  // Both Smi tags must be 1 (not Smi).
  and_(at, reg1, reg2);
  JumpIfSmi(at, on_either_smi);
}


void MacroAssembler::AbortIfSmi(Register object) {
  STATIC_ASSERT(kSmiTag == 0);
  andi(at, object, kSmiTagMask);
  Assert(ne, "Operand is a smi", at, Operand(zero_reg));
}


void MacroAssembler::AbortIfNotSmi(Register object) {
  STATIC_ASSERT(kSmiTag == 0);
  andi(at, object, kSmiTagMask);
  Assert(eq, "Operand is a smi", at, Operand(zero_reg));
}


void MacroAssembler::AbortIfNotString(Register object) {
  STATIC_ASSERT(kSmiTag == 0);
  And(t0, object, Operand(kSmiTagMask));
  Assert(ne, "Operand is not a string", t0, Operand(zero_reg));
  push(object);
  lw(object, FieldMemOperand(object, HeapObject::kMapOffset));
  lbu(object, FieldMemOperand(object, Map::kInstanceTypeOffset));
  Assert(lo, "Operand is not a string", object, Operand(FIRST_NONSTRING_TYPE));
  pop(object);
}


void MacroAssembler::AbortIfNotRootValue(Register src,
                                         Heap::RootListIndex root_value_index,
                                         const char* message) {
  ASSERT(!src.is(at));
  LoadRoot(at, root_value_index);
  Assert(eq, message, src, Operand(at));
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Register heap_number_map,
                                         Register scratch,
                                         Label* on_not_heap_number) {
  lw(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  AssertRegisterIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  Branch(on_not_heap_number, ne, scratch, Operand(heap_number_map));
}


void MacroAssembler::JumpIfNonSmisNotBothSequentialAsciiStrings(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  // Test that both first and second are sequential ASCII strings.
  // Assume that they are non-smis.
  lw(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  lw(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  lbu(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  lbu(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfBothInstanceTypesAreNotSequentialAscii(scratch1,
                                               scratch2,
                                               scratch1,
                                               scratch2,
                                               failure);
}


void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(Register first,
                                                         Register second,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Label* failure) {
  // Check that neither is a smi.
  STATIC_ASSERT(kSmiTag == 0);
  And(scratch1, first, Operand(second));
  JumpIfSmi(scratch1, failure);
  JumpIfNonSmisNotBothSequentialAsciiStrings(first,
                                             second,
                                             scratch1,
                                             scratch2,
                                             failure);
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  int kFlatAsciiStringTag = ASCII_STRING_TYPE;
  ASSERT(kFlatAsciiStringTag <= 0xffff);  // Ensure this fits 16-bit immed.
  andi(scratch1, first, kFlatAsciiStringMask);
  Branch(failure, ne, scratch1, Operand(kFlatAsciiStringTag));
  andi(scratch2, second, kFlatAsciiStringMask);
  Branch(failure, ne, scratch2, Operand(kFlatAsciiStringTag));
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(Register type,
                                                            Register scratch,
                                                            Label* failure) {
  int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  int kFlatAsciiStringTag = ASCII_STRING_TYPE;
  And(scratch, type, Operand(kFlatAsciiStringMask));
  Branch(failure, ne, scratch, Operand(kFlatAsciiStringTag));
}


static const int kRegisterPassedArguments = 4;

int MacroAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
  num_reg_arguments += 2 * num_double_arguments;

  // Up to four simple arguments are passed in registers a0..a3.
  if (num_reg_arguments > kRegisterPassedArguments) {
    stack_passed_words += num_reg_arguments - kRegisterPassedArguments;
  }
  stack_passed_words += kCArgSlotCount;
  return stack_passed_words;
}


void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  int frame_alignment = ActivationFrameAlignment();

  // Up to four simple arguments are passed in registers a0..a3.
  // Those four arguments must have reserved argument slots on the stack for
  // mips, even though those argument slots are not normally used.
  // Remaining arguments are pushed on the stack, above (higher address than)
  // the argument slots.
  int stack_passed_arguments = CalculateStackPassedWords(
      num_reg_arguments, num_double_arguments);
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for num_arguments - 4 words
    // and the original value of sp.
    mov(scratch, sp);
    Subu(sp, sp, Operand((stack_passed_arguments + 1) * kPointerSize));
    ASSERT(IsPowerOf2(frame_alignment));
    And(sp, sp, Operand(-frame_alignment));
    sw(scratch, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Subu(sp, sp, Operand(stack_passed_arguments * kPointerSize));
  }
}


void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  li(t8, Operand(function));
  CallCFunctionHelper(t8, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  ASSERT(has_frame());
  // Make sure that the stack is aligned before calling a C function unless
  // running in the simulator. The simulator has its own alignment check which
  // provides more information.
  // The argument stots are presumed to have been set up by
  // PrepareCallCFunction. The C function must be called via t9, for mips ABI.

#if defined(V8_HOST_ARCH_MIPS)
  if (emit_debug_code()) {
    int frame_alignment = OS::ActivationFrameAlignment();
    int frame_alignment_mask = frame_alignment - 1;
    if (frame_alignment > kPointerSize) {
      ASSERT(IsPowerOf2(frame_alignment));
      Label alignment_as_expected;
      And(at, sp, Operand(frame_alignment_mask));
      Branch(&alignment_as_expected, eq, at, Operand(zero_reg));
      // Don't use Check here, as it will call Runtime_Abort possibly
      // re-entering here.
      stop("Unexpected alignment in CallCFunction");
      bind(&alignment_as_expected);
    }
  }
#endif  // V8_HOST_ARCH_MIPS

  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.

  if (!function.is(t9)) {
    mov(t9, function);
    function = t9;
  }

  Call(function);

  int stack_passed_arguments = CalculateStackPassedWords(
      num_reg_arguments, num_double_arguments);

  if (OS::ActivationFrameAlignment() > kPointerSize) {
    lw(sp, MemOperand(sp, stack_passed_arguments * kPointerSize));
  } else {
    Addu(sp, sp, Operand(stack_passed_arguments * sizeof(kPointerSize)));
  }
}


#undef BRANCH_ARGS_CHECK


void MacroAssembler::PatchRelocatedValue(Register li_location,
                                         Register scratch,
                                         Register new_value) {
  lw(scratch, MemOperand(li_location));
  // At this point scratch is a lui(at, ...) instruction.
  if (emit_debug_code()) {
    And(scratch, scratch, kOpcodeMask);
    Check(eq, "The instruction to patch should be a lui.",
        scratch, Operand(LUI));
    lw(scratch, MemOperand(li_location));
  }
  srl(t9, new_value, kImm16Bits);
  Ins(scratch, t9, 0, kImm16Bits);
  sw(scratch, MemOperand(li_location));

  lw(scratch, MemOperand(li_location, kInstrSize));
  // scratch is now ori(at, ...).
  if (emit_debug_code()) {
    And(scratch, scratch, kOpcodeMask);
    Check(eq, "The instruction to patch should be an ori.",
        scratch, Operand(ORI));
    lw(scratch, MemOperand(li_location, kInstrSize));
  }
  Ins(scratch, new_value, 0, kImm16Bits);
  sw(scratch, MemOperand(li_location, kInstrSize));

  // Update the I-cache so the new lui and ori can be executed.
  FlushICache(li_location, 2);
}

void MacroAssembler::GetRelocatedValue(Register li_location,
                                       Register value,
                                       Register scratch) {
  lw(value, MemOperand(li_location));
  if (emit_debug_code()) {
    And(value, value, kOpcodeMask);
    Check(eq, "The instruction should be a lui.",
        value, Operand(LUI));
    lw(value, MemOperand(li_location));
  }

  // value now holds a lui instruction. Extract the immediate.
  sll(value, value, kImm16Bits);

  lw(scratch, MemOperand(li_location, kInstrSize));
  if (emit_debug_code()) {
    And(scratch, scratch, kOpcodeMask);
    Check(eq, "The instruction should be an ori.",
        scratch, Operand(ORI));
    lw(scratch, MemOperand(li_location, kInstrSize));
  }
  // "scratch" now holds an ori instruction. Extract the immediate.
  andi(scratch, scratch, kImm16Mask);

  // Merge the results.
  or_(value, value, scratch);
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,
    int mask,
    Condition cc,
    Label* condition_met) {
  And(scratch, object, Operand(~Page::kPageAlignmentMask));
  lw(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
  And(scratch, scratch, Operand(mask));
  Branch(condition_met, cc, scratch, Operand(zero_reg));
}


void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black) {
  HasColor(object, scratch0, scratch1, on_black, 1, 0);  // kBlackBitPattern.
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register mask_scratch,
                              Label* has_color,
                              int first_bit,
                              int second_bit) {
  ASSERT(!AreAliased(object, bitmap_scratch, mask_scratch, t8));
  ASSERT(!AreAliased(object, bitmap_scratch, mask_scratch, t9));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  Label other_color, word_boundary;
  lw(t9, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  And(t8, t9, Operand(mask_scratch));
  Branch(&other_color, first_bit == 1 ? eq : ne, t8, Operand(zero_reg));
  // Shift left 1 by adding.
  Addu(mask_scratch, mask_scratch, Operand(mask_scratch));
  Branch(&word_boundary, eq, mask_scratch, Operand(zero_reg));
  And(t8, t9, Operand(mask_scratch));
  Branch(has_color, second_bit == 1 ? ne : eq, t8, Operand(zero_reg));
  jmp(&other_color);

  bind(&word_boundary);
  lw(t9, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize + kPointerSize));
  And(t9, t9, Operand(1));
  Branch(has_color, second_bit == 1 ? ne : eq, t9, Operand(zero_reg));
  bind(&other_color);
}


// Detect some, but not all, common pointer-free objects.  This is used by the
// incremental write barrier which doesn't care about oddballs (they are always
// marked black immediately so this code is not hit).
void MacroAssembler::JumpIfDataObject(Register value,
                                      Register scratch,
                                      Label* not_data_object) {
  ASSERT(!AreAliased(value, scratch, t8, no_reg));
  Label is_data_object;
  lw(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  LoadRoot(t8, Heap::kHeapNumberMapRootIndex);
  Branch(&is_data_object, eq, t8, Operand(scratch));
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  lbu(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  And(t8, scratch, Operand(kIsIndirectStringMask | kIsNotStringMask));
  Branch(not_data_object, ne, t8, Operand(zero_reg));
  bind(&is_data_object);
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register mask_reg) {
  ASSERT(!AreAliased(addr_reg, bitmap_reg, mask_reg, no_reg));
  And(bitmap_reg, addr_reg, Operand(~Page::kPageAlignmentMask));
  Ext(mask_reg, addr_reg, kPointerSizeLog2, Bitmap::kBitsPerCellLog2);
  const int kLowBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  Ext(t8, addr_reg, kLowBits, kPageSizeBits - kLowBits);
  sll(t8, t8, kPointerSizeLog2);
  Addu(bitmap_reg, bitmap_reg, t8);
  li(t8, Operand(1));
  sllv(mask_reg, t8, mask_reg);
}


void MacroAssembler::EnsureNotWhite(
    Register value,
    Register bitmap_scratch,
    Register mask_scratch,
    Register load_scratch,
    Label* value_is_white_and_not_data) {
  ASSERT(!AreAliased(value, bitmap_scratch, mask_scratch, t8));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  ASSERT(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  ASSERT(strcmp(Marking::kBlackBitPattern, "10") == 0);
  ASSERT(strcmp(Marking::kGreyBitPattern, "11") == 0);
  ASSERT(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  Label done;

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  lw(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  And(t8, mask_scratch, load_scratch);
  Branch(&done, ne, t8, Operand(zero_reg));

  if (emit_debug_code()) {
    // Check for impossible bit pattern.
    Label ok;
    // sll may overflow, making the check conservative.
    sll(t8, mask_scratch, 1);
    And(t8, load_scratch, t8);
    Branch(&ok, eq, t8, Operand(zero_reg));
    stop("Impossible marking bit pattern");
    bind(&ok);
  }

  // Value is white.  We check whether it is data that doesn't need scanning.
  // Currently only checks for HeapNumber and non-cons strings.
  Register map = load_scratch;  // Holds map while checking type.
  Register length = load_scratch;  // Holds length of object after testing type.
  Label is_data_object;

  // Check for heap-number
  lw(map, FieldMemOperand(value, HeapObject::kMapOffset));
  LoadRoot(t8, Heap::kHeapNumberMapRootIndex);
  {
    Label skip;
    Branch(&skip, ne, t8, Operand(map));
    li(length, HeapNumber::kSize);
    Branch(&is_data_object);
    bind(&skip);
  }

  // Check for strings.
  ASSERT(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  ASSERT(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  Register instance_type = load_scratch;
  lbu(instance_type, FieldMemOperand(map, Map::kInstanceTypeOffset));
  And(t8, instance_type, Operand(kIsIndirectStringMask | kIsNotStringMask));
  Branch(value_is_white_and_not_data, ne, t8, Operand(zero_reg));
  // It's a non-indirect (non-cons and non-slice) string.
  // If it's external, the length is just ExternalString::kSize.
  // Otherwise it's String::kHeaderSize + string->length() * (1 or 2).
  // External strings are the only ones with the kExternalStringTag bit
  // set.
  ASSERT_EQ(0, kSeqStringTag & kExternalStringTag);
  ASSERT_EQ(0, kConsStringTag & kExternalStringTag);
  And(t8, instance_type, Operand(kExternalStringTag));
  {
    Label skip;
    Branch(&skip, eq, t8, Operand(zero_reg));
    li(length, ExternalString::kSize);
    Branch(&is_data_object);
    bind(&skip);
  }

  // Sequential string, either ASCII or UC16.
  // For ASCII (char-size of 1) we shift the smi tag away to get the length.
  // For UC16 (char-size of 2) we just leave the smi tag in place, thereby
  // getting the length multiplied by 2.
  ASSERT(kAsciiStringTag == 4 && kStringEncodingMask == 4);
  ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  lw(t9, FieldMemOperand(value, String::kLengthOffset));
  And(t8, instance_type, Operand(kStringEncodingMask));
  {
    Label skip;
    Branch(&skip, eq, t8, Operand(zero_reg));
    srl(t9, t9, 1);
    bind(&skip);
  }
  Addu(length, t9, Operand(SeqString::kHeaderSize + kObjectAlignmentMask));
  And(length, length, Operand(~kObjectAlignmentMask));

  bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  lw(t8, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  Or(t8, t8, Operand(mask_scratch));
  sw(t8, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));

  And(bitmap_scratch, bitmap_scratch, Operand(~Page::kPageAlignmentMask));
  lw(t8, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));
  Addu(t8, t8, Operand(length));
  sw(t8, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));

  bind(&done);
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  lw(descriptors,
     FieldMemOperand(map, Map::kInstanceDescriptorsOrBitField3Offset));
  Label not_smi;
  JumpIfNotSmi(descriptors, &not_smi);
  LoadRoot(descriptors, Heap::kEmptyDescriptorArrayRootIndex);
  bind(&not_smi);
}


void MacroAssembler::CheckEnumCache(Register null_value, Label* call_runtime) {
  Label next;
  // Preload a couple of values used in the loop.
  Register  empty_fixed_array_value = t2;
  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Register empty_descriptor_array_value = t3;
  LoadRoot(empty_descriptor_array_value,
           Heap::kEmptyDescriptorArrayRootIndex);
  mov(a1, a0);
  bind(&next);

  // Check that there are no elements.  Register a1 contains the
  // current JS object we've reached through the prototype chain.
  lw(a2, FieldMemOperand(a1, JSObject::kElementsOffset));
  Branch(call_runtime, ne, a2, Operand(empty_fixed_array_value));

  // Check that instance descriptors are not empty so that we can
  // check for an enum cache.  Leave the map in a2 for the subsequent
  // prototype load.
  lw(a2, FieldMemOperand(a1, HeapObject::kMapOffset));
  lw(a3, FieldMemOperand(a2, Map::kInstanceDescriptorsOrBitField3Offset));
  JumpIfSmi(a3, call_runtime);

  // Check that there is an enum cache in the non-empty instance
  // descriptors (a3).  This is the case if the next enumeration
  // index field does not contain a smi.
  lw(a3, FieldMemOperand(a3, DescriptorArray::kEnumerationIndexOffset));
  JumpIfSmi(a3, call_runtime);

  // For all objects but the receiver, check that the cache is empty.
  Label check_prototype;
  Branch(&check_prototype, eq, a1, Operand(a0));
  lw(a3, FieldMemOperand(a3, DescriptorArray::kEnumCacheBridgeCacheOffset));
  Branch(call_runtime, ne, a3, Operand(empty_fixed_array_value));

  // Load the prototype from the map and loop if non-null.
  bind(&check_prototype);
  lw(a1, FieldMemOperand(a2, Map::kPrototypeOffset));
  Branch(&next, ne, a1, Operand(null_value));
}


void MacroAssembler::ClampUint8(Register output_reg, Register input_reg) {
  ASSERT(!output_reg.is(input_reg));
  Label done;
  li(output_reg, Operand(255));
  // Normal branch: nop in delay slot.
  Branch(&done, gt, input_reg, Operand(output_reg));
  // Use delay slot in this branch.
  Branch(USE_DELAY_SLOT, &done, lt, input_reg, Operand(zero_reg));
  mov(output_reg, zero_reg);  // In delay slot.
  mov(output_reg, input_reg);  // Value is in range 0..255.
  bind(&done);
}


void MacroAssembler::ClampDoubleToUint8(Register result_reg,
                                        DoubleRegister input_reg,
                                        DoubleRegister temp_double_reg) {
  Label above_zero;
  Label done;
  Label in_bounds;

  Move(temp_double_reg, 0.0);
  BranchF(&above_zero, NULL, gt, input_reg, temp_double_reg);

  // Double value is less than zero, NaN or Inf, return 0.
  mov(result_reg, zero_reg);
  Branch(&done);

  // Double value is >= 255, return 255.
  bind(&above_zero);
  Move(temp_double_reg, 255.0);
  BranchF(&in_bounds, NULL, le, input_reg, temp_double_reg);
  li(result_reg, Operand(255));
  Branch(&done);

  // In 0-255 range, round and truncate.
  bind(&in_bounds);
  round_w_d(temp_double_reg, input_reg);
  mfc1(result_reg, temp_double_reg);
  bind(&done);
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


CodePatcher::CodePatcher(byte* address, int instructions)
    : address_(address),
      instructions_(instructions),
      size_(instructions * Assembler::kInstrSize),
      masm_(Isolate::Current(), address, size_ + Assembler::kGap) {
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


void CodePatcher::Emit(Instr instr) {
  masm()->emit(instr);
}


void CodePatcher::Emit(Address addr) {
  masm()->emit(reinterpret_cast<Instr>(addr));
}


void CodePatcher::ChangeBranchCondition(Condition cond) {
  Instr instr = Assembler::instr_at(masm_.pc_);
  ASSERT(Assembler::IsBranch(instr));
  uint32_t opcode = Assembler::GetOpcodeField(instr);
  // Currently only the 'eq' and 'ne' cond values are supported and the simple
  // branch instructions (with opcode being the branch type).
  // There are some special cases (see Assembler::IsBranch()) so extending this
  // would be tricky.
  ASSERT(opcode == BEQ ||
         opcode == BNE ||
        opcode == BLEZ ||
        opcode == BGTZ ||
        opcode == BEQL ||
        opcode == BNEL ||
       opcode == BLEZL ||
       opcode == BGTZL);
  opcode = (cond == eq) ? BEQ : BNE;
  instr = (instr & ~kOpcodeMask) | opcode;
  masm_.emit(instr);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/jump-table-assembler.h"

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

// The implementation is compact enough to implement it inline here. If it gets
// much bigger, we might want to split it in a separate file per architecture.
#if V8_TARGET_ARCH_X64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  // Use a push, because mov to an extended register takes 6 bytes.
  pushq_imm32(func_index);            // 5 bytes
  EmitJumpSlot(lazy_compile_target);  // 5 bytes
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  intptr_t displacement = static_cast<intptr_t>(
      reinterpret_cast<byte*>(target) - pc_ - kNearJmpInstrSize);
  if (!is_int32(displacement)) return false;
  near_jmp(displacement, RelocInfo::NONE);  // 5 bytes
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  Label data;
  int start_offset = pc_offset();
  jmp(Operand(&data));  // 6 bytes
  Nop(2);               // 2 bytes
  // The data must be properly aligned, so it can be patched atomically (see
  // {PatchFarJumpSlot}).
  DCHECK_EQ(start_offset + kSystemPointerSize, pc_offset());
  USE(start_offset);
  bind(&data);
  dq(target);  // 8 bytes
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  // The slot needs to be pointer-size aligned so we can atomically update it.
  DCHECK(IsAligned(slot, kSystemPointerSize));
  // Offset of the target is at 8 bytes, see {EmitFarJumpSlot}.
  reinterpret_cast<std::atomic<Address>*>(slot + kSystemPointerSize)
      ->store(target, std::memory_order_relaxed);
  // The update is atomic because the address is properly aligned.
  // Because of cache coherence, the data update will eventually be seen by all
  // cores. It's ok if they temporarily jump to the old target.
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  Nop(bytes);
}

#elif V8_TARGET_ARCH_IA32
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  mov(kWasmCompileLazyFuncIndexRegister, func_index);  // 5 bytes
  jmp(lazy_compile_target, RelocInfo::NONE);           // 5 bytes
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  jmp(target, RelocInfo::NONE);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  jmp(target, RelocInfo::NONE);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  Nop(bytes);
}

#elif V8_TARGET_ARCH_ARM
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  // Load function index to a register.
  // This generates [movw, movt] on ARMv7 and later, [ldr, constant pool marker,
  // constant] on ARMv6.
  Move32BitImmediate(kWasmCompileLazyFuncIndexRegister, Operand(func_index));
  // EmitJumpSlot emits either [b], [movw, movt, mov] (ARMv7+), or [ldr,
  // constant].
  // In total, this is <=5 instructions on all architectures.
  // TODO(arm): Optimize this for code size; lazy compile is not performance
  // critical, as it's only executed once per function.
  EmitJumpSlot(lazy_compile_target);
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  // Note that {Move32BitImmediate} emits [ldr, constant] for the relocation
  // mode used below, we need this to allow concurrent patching of this slot.
  Move32BitImmediate(pc, Operand(target, RelocInfo::WASM_CALL));
  CheckConstPool(true, false);  // force emit of const pool
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  // Load from [pc + kInstrSize] to pc. Note that {pc} points two instructions
  // after the currently executing one.
  ldr_pcrel(pc, -kInstrSize);  // 1 instruction
  dd(target);                  // 4 bytes (== 1 instruction)
  STATIC_ASSERT(kInstrSize == kInt32Size);
  STATIC_ASSERT(kFarJumpTableSlotSize == 2 * kInstrSize);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % kInstrSize);
  for (; bytes > 0; bytes -= kInstrSize) {
    nop();
  }
}

#elif V8_TARGET_ARCH_ARM64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  int start = pc_offset();
  CodeEntry();                                             // 0-1 instr
  Mov(kWasmCompileLazyFuncIndexRegister.W(), func_index);  // 1-2 instr
  Jump(lazy_compile_target, RelocInfo::NONE);              // 1 instr
  int nop_bytes = start + kLazyCompileTableSlotSize - pc_offset();
  DCHECK(nop_bytes == 0 || nop_bytes == kInstrSize);
  if (nop_bytes) nop();
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  if (!TurboAssembler::IsNearCallOffset(
          (reinterpret_cast<byte*>(target) - pc_) / kInstrSize)) {
    return false;
  }

  CodeEntry();

  Jump(target, RelocInfo::NONE);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  // This code uses hard-coded registers and instructions (and avoids
  // {UseScratchRegisterScope} or {InstructionAccurateScope}) because this code
  // will only be called for the very specific runtime slot table, and we want
  // to have maximum control over the generated code.
  // Do not reuse this code without validating that the same assumptions hold.
  CodeEntry();  // 0-1 instructions
  constexpr Register kTmpReg = x16;
  DCHECK(TmpList()->IncludesAliasOf(kTmpReg));
  int kOffset = ENABLE_CONTROL_FLOW_INTEGRITY_BOOL ? 3 : 2;
  // Load from [pc + kOffset * kInstrSize] to {kTmpReg}, then branch there.
  ldr_pcrel(kTmpReg, kOffset);  // 1 instruction
  br(kTmpReg);                  // 1 instruction
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
  nop();       // To keep the target below aligned to kSystemPointerSize.
#endif
  dq(target);  // 8 bytes (== 2 instructions)
  STATIC_ASSERT(2 * kInstrSize == kSystemPointerSize);
  const int kSlotCount = ENABLE_CONTROL_FLOW_INTEGRITY_BOOL ? 6 : 4;
  STATIC_ASSERT(kFarJumpTableSlotSize == kSlotCount * kInstrSize);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  // See {EmitFarJumpSlot} for the offset of the target (16 bytes with
  // CFI enabled, 8 bytes otherwise).
  int kTargetOffset =
      ENABLE_CONTROL_FLOW_INTEGRITY_BOOL ? 4 * kInstrSize : 2 * kInstrSize;
  // The slot needs to be pointer-size aligned so we can atomically update it.
  DCHECK(IsAligned(slot + kTargetOffset, kSystemPointerSize));
  reinterpret_cast<std::atomic<Address>*>(slot + kTargetOffset)
      ->store(target, std::memory_order_relaxed);
  // The data update is guaranteed to be atomic since it's a properly aligned
  // and stores a single machine word. This update will eventually be observed
  // by any concurrent [ldr] on the same address because of the data cache
  // coherence. It's ok if other cores temporarily jump to the old target.
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % kInstrSize);
  for (; bytes > 0; bytes -= kInstrSize) {
    nop();
  }
}

#elif V8_TARGET_ARCH_S390X
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  // Load function index to r7. 6 bytes
  lgfi(kWasmCompileLazyFuncIndexRegister, Operand(func_index));
  // Jump to {lazy_compile_target}. 6 bytes or 12 bytes
  mov(r1, Operand(lazy_compile_target, RelocInfo::CODE_TARGET));
  b(r1);  // 2 bytes
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  mov(r1, Operand(target, RelocInfo::CODE_TARGET));
  b(r1);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  JumpToInstructionStream(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % 2);
  for (; bytes > 0; bytes -= 2) {
    nop(0);
  }
}

#elif V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  int start = pc_offset();
  li(kWasmCompileLazyFuncIndexRegister, func_index);  // max. 2 instr
  // Jump produces max. 4 instructions for 32-bit platform
  // and max. 6 instructions for 64-bit platform.
  Jump(lazy_compile_target, RelocInfo::NONE);
  int nop_bytes = start + kLazyCompileTableSlotSize - pc_offset();
  DCHECK_EQ(nop_bytes % kInstrSize, 0);
  for (int i = 0; i < nop_bytes; i += kInstrSize) nop();
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  PatchAndJump(target);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  JumpToInstructionStream(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % kInstrSize);
  for (; bytes > 0; bytes -= kInstrSize) {
    nop();
  }
}

#elif V8_TARGET_ARCH_PPC64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  int start = pc_offset();
  // Load function index to register. max 5 instrs
  mov(kWasmCompileLazyFuncIndexRegister, Operand(func_index));
  // Jump to {lazy_compile_target}. max 5 instrs
  mov(r0, Operand(lazy_compile_target));
  mtctr(r0);
  bctr();
  int nop_bytes = start + kLazyCompileTableSlotSize - pc_offset();
  DCHECK_EQ(nop_bytes % kInstrSize, 0);
  for (int i = 0; i < nop_bytes; i += kInstrSize) nop();
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  mov(r0, Operand(target));
  mtctr(r0);
  bctr();
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  JumpToInstructionStream(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % 4);
  for (; bytes > 0; bytes -= 4) {
    nop(0);
  }
}

#else
#error Unknown architecture.
#endif

}  // namespace wasm
}  // namespace internal
}  // namespace v8

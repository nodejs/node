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

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  JumpToInstructionStream(builtin_target);
}

void JumpTableAssembler::EmitJumpSlot(Address target) {
  // On x64, all code is allocated within a single code section, so we can use
  // relative jumps.
  static_assert(kMaxWasmCodeMemory <= size_t{2} * GB, "can use relative jump");
  intptr_t displacement = static_cast<intptr_t>(
      reinterpret_cast<byte*>(target) - pc_ - kNearJmpInstrSize);
  near_jmp(displacement, RelocInfo::NONE);
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

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  JumpToInstructionStream(builtin_target);
}

void JumpTableAssembler::EmitJumpSlot(Address target) {
  jmp(target, RelocInfo::NONE);
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

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  JumpToInstructionStream(builtin_target);
  CheckConstPool(true, false);  // force emit of const pool
}

void JumpTableAssembler::EmitJumpSlot(Address target) {
  // Note that {Move32BitImmediate} emits [ldr, constant] for the relocation
  // mode used below, we need this to allow concurrent patching of this slot.
  Move32BitImmediate(pc, Operand(target, RelocInfo::WASM_CALL));
  CheckConstPool(true, false);  // force emit of const pool
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
  Mov(kWasmCompileLazyFuncIndexRegister.W(), func_index);  // 1-2 instr
  Jump(lazy_compile_target, RelocInfo::NONE);              // 1 instr
  int nop_bytes = start + kLazyCompileTableSlotSize - pc_offset();
  DCHECK(nop_bytes == 0 || nop_bytes == kInstrSize);
  if (nop_bytes) nop();
}

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  JumpToInstructionStream(builtin_target);
  ForceConstantPoolEmissionWithoutJump();
}

void JumpTableAssembler::EmitJumpSlot(Address target) {
  // TODO(wasm): Currently this is guaranteed to be a {near_call} and hence is
  // patchable concurrently. Once {kMaxWasmCodeMemory} is raised on ARM64, make
  // sure concurrent patching is still supported.
  DCHECK(TurboAssembler::IsNearCallOffset(
      (reinterpret_cast<byte*>(target) - pc_) / kInstrSize));

  Jump(target, RelocInfo::NONE);
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
  mov(r1, Operand(lazy_compile_target));
  b(r1);  // 2 bytes
}

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  JumpToInstructionStream(builtin_target);
}

void JumpTableAssembler::EmitJumpSlot(Address target) {
  mov(r1, Operand(target));
  b(r1);
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

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  JumpToInstructionStream(builtin_target);
}

void JumpTableAssembler::EmitJumpSlot(Address target) {
  Jump(target, RelocInfo::NONE);
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

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  JumpToInstructionStream(builtin_target);
}

void JumpTableAssembler::EmitJumpSlot(Address target) {
  mov(r0, Operand(target));
  mtctr(r0);
  bctr();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % 4);
  for (; bytes > 0; bytes -= 4) {
    nop(0);
  }
}

#else
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  UNIMPLEMENTED();
}

void JumpTableAssembler::EmitRuntimeStubSlot(Address builtin_target) {
  UNIMPLEMENTED();
}

void JumpTableAssembler::EmitJumpSlot(Address target) { UNIMPLEMENTED(); }

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  UNIMPLEMENTED();
}
#endif

}  // namespace wasm
}  // namespace internal
}  // namespace v8

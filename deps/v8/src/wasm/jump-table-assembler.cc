// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/jump-table-assembler.h"

#include "src/base/sanitizer/ubsan.h"
#include "src/codegen/macro-assembler-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

// static
void JumpTableAssembler::GenerateLazyCompileTable(
    AccountingAllocator* allocator, Address base, uint32_t num_slots,
    uint32_t num_imported_functions, Address wasm_compile_lazy_target) {
  uint32_t lazy_compile_table_size = num_slots * kLazyCompileTableSlotSize;
  WritableJitAllocation jit_allocation = ThreadIsolation::LookupJitAllocation(
      base, RoundUp<kCodeAlignment>(lazy_compile_table_size),
      ThreadIsolation::JitAllocationType::kWasmLazyCompileTable);
  // Assume enough space, so the Assembler does not try to grow the buffer.
  JumpTableAssembler jtasm(allocator, base, lazy_compile_table_size + 256);
  for (uint32_t slot_index = 0; slot_index < num_slots; ++slot_index) {
    DCHECK_EQ(slot_index * kLazyCompileTableSlotSize, jtasm.pc_offset());
    jtasm.EmitLazyCompileJumpSlot(slot_index + num_imported_functions,
                                  wasm_compile_lazy_target);
  }
  DCHECK_EQ(lazy_compile_table_size, jtasm.pc_offset());
  FlushInstructionCache(base, lazy_compile_table_size);
}

void JumpTableAssembler::InitializeJumpsToLazyCompileTable(
    AccountingAllocator* allocator, Address base, uint32_t num_slots,
    Address lazy_compile_table_start) {
  uint32_t jump_table_size = SizeForNumberOfSlots(num_slots);
  WritableJitAllocation jit_allocation = ThreadIsolation::LookupJitAllocation(
      base, RoundUp<kCodeAlignment>(jump_table_size),
      ThreadIsolation::JitAllocationType::kWasmJumpTable);
  JumpTableAssembler jtasm(allocator, base, jump_table_size + 256);

  for (uint32_t slot_index = 0; slot_index < num_slots; ++slot_index) {
    // Make sure we write at the correct offset.
    int slot_offset =
        static_cast<int>(JumpTableAssembler::JumpSlotIndexToOffset(slot_index));

    jtasm.SkipUntil(slot_offset);

    Address target =
        lazy_compile_table_start +
        JumpTableAssembler::LazyCompileSlotIndexToOffset(slot_index);

    int offset_before_emit = jtasm.pc_offset();
    // This function initializes the first jump table with jumps to the lazy
    // compile table. Both get allocated in the constructor of the
    // {NativeModule}, so they both should end up in the initial code space.
    // Jumps within one code space can always be near jumps, so the following
    // call to {EmitJumpSlot} should always succeed. If the call fails, then
    // either the jump table allocation was changed incorrectly so that the lazy
    // compile table was not within near-jump distance of the jump table
    // anymore (e.g. the initial code space was too small to fit both tables),
    // or the code space was allocated larger than the maximum near-jump
    // distance.
    CHECK(jtasm.EmitJumpSlot(target));
    int written_bytes = jtasm.pc_offset() - offset_before_emit;
    // We write nops here instead of skipping to avoid partial instructions in
    // the jump table. Partial instructions can cause problems for the
    // disassembler.
    jtasm.NopBytes(kJumpTableSlotSize - written_bytes);
  }
  FlushInstructionCache(base, jump_table_size);
}

template <typename T>
void JumpTableAssembler::emit(T value) {
  base::WriteUnalignedValue<T>(reinterpret_cast<Address>(pc_), value);
  pc_ += sizeof(T);
}

template <typename T>
void JumpTableAssembler::emit(T value, RelaxedStoreTag) DISABLE_UBSAN {
  // We disable ubsan for these stores since they don't follow the alignment
  // requirements. We instead guarantee in the jump table layout that the writes
  // will still be atomic since they don't cross a qword boundary.
#if V8_TARGET_ARCH_X64
#ifdef DEBUG
  Address write_start = reinterpret_cast<Address>(pc_);
  Address write_end = write_start + sizeof(T) - 1;
  // Check that the write doesn't cross a qword boundary.
  DCHECK_EQ(write_start >> kSystemPointerSizeLog2,
            write_end >> kSystemPointerSizeLog2);
#endif
#endif
  reinterpret_cast<std::atomic<T>*>(pc_)->store(value,
                                                std::memory_order_relaxed);
  pc_ += sizeof(T);
}

// The implementation is compact enough to implement it inline here. If it gets
// much bigger, we might want to split it in a separate file per architecture.
#if V8_TARGET_ARCH_X64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  // Use a push, because mov to an extended register takes 6 bytes.
  const uint8_t inst[kLazyCompileTableSlotSize] = {
      0x68, 0, 0, 0, 0,  // pushq func_index
      0xe9, 0, 0, 0, 0,  // near_jmp displacement
  };

  intptr_t displacement =
      static_cast<intptr_t>(reinterpret_cast<uint8_t*>(lazy_compile_target) -
                            (pc_ + kLazyCompileTableSlotSize));

  emit<uint8_t>(inst[0]);
  emit<uint32_t>(func_index);
  emit<uint8_t>(inst[5]);
  emit<int32_t>(base::checked_cast<int32_t>(displacement));
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
#ifdef V8_ENABLE_CET_IBT
  uint32_t endbr_insn = 0xfa1e0ff3;
  uint32_t nop = 0x00401f0f;
  emit<uint32_t>(endbr_insn, kRelaxedStore);
  // Add a nop to ensure that the next block is 8 byte aligned.
  emit<uint32_t>(nop, kRelaxedStore);
#endif

  intptr_t displacement = static_cast<intptr_t>(
      reinterpret_cast<uint8_t*>(target) - (pc_ + kIntraSegmentJmpInstrSize));
  if (!is_int32(displacement)) return false;

  uint8_t inst[kJumpTableSlotSize] = {
      0xe9, 0,    0,    0, 0,  // near_jmp displacement
      0xcc, 0xcc, 0xcc,        // int3 * 3
  };
  int32_t displacement32 = base::checked_cast<int32_t>(displacement);
  memcpy(&inst[1], &displacement32, sizeof(int32_t));

  // The jump table is updated live, so the write has to be atomic.
  emit<uint64_t>(*reinterpret_cast<uint64_t*>(inst), kRelaxedStore);

  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  const uint8_t inst[kFarJumpTableSlotSize] = {
      0xff, 0x25, 0x02, 0, 0, 0,        // jmp [rip+0x2]
      0x66, 0x90,                       // Nop(2)
      0,    0,    0,    0, 0, 0, 0, 0,  // target
  };

  emit<uint64_t>(*reinterpret_cast<const uint64_t*>(inst));
  emit<uint64_t>(target);
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
  if (bytes) Nop(bytes);
}

void JumpTableAssembler::SkipUntil(int offset) {
  DCHECK_GE(offset, pc_offset());
  pc_ += offset - pc_offset();
}

#elif V8_TARGET_ARCH_IA32
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  static_assert(kWasmCompileLazyFuncIndexRegister == edi);
  const uint8_t inst[kLazyCompileTableSlotSize] = {
      0xbf, 0, 0, 0, 0,  // mov edi, func_index
      0xe9, 0, 0, 0, 0,  // near_jmp displacement
  };
  intptr_t displacement =
      static_cast<intptr_t>(reinterpret_cast<uint8_t*>(lazy_compile_target) -
                            (pc_ + kLazyCompileTableSlotSize));

  emit<uint8_t>(inst[0]);
  emit<uint32_t>(func_index);
  emit<uint8_t>(inst[5]);
  emit<int32_t>(base::checked_cast<int32_t>(displacement));
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  intptr_t displacement = static_cast<intptr_t>(
      reinterpret_cast<uint8_t*>(target) - (pc_ + kJumpTableSlotSize));

  const uint8_t inst[kJumpTableSlotSize] = {
      0xe9, 0, 0, 0, 0,  // near_jmp displacement
  };

  // The jump table is updated live, so the writes have to be atomic.
  emit<uint8_t>(inst[0], kRelaxedStore);
  emit<int32_t>(base::checked_cast<int32_t>(displacement), kRelaxedStore);

  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  static_assert(kJumpTableSlotSize == kFarJumpTableSlotSize);
  EmitJumpSlot(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  if (bytes) Nop(bytes);
}

void JumpTableAssembler::SkipUntil(int offset) {
  DCHECK_GE(offset, pc_offset());
  pc_ += offset - pc_offset();
}

#elif V8_TARGET_ARCH_ARM
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  static_assert(kWasmCompileLazyFuncIndexRegister == r4);
  // Note that below, [pc] points to the instruction after the next.
  const uint32_t inst[kLazyCompileTableSlotSize / 4] = {
      0xe59f4000,  // ldr r4, [pc]
      0xe59ff000,  // ldr pc, [pc]
      0x00000000,  // func_index
      0x00000000,  // target
  };
  emit<uint32_t>(inst[0]);
  emit<uint32_t>(inst[1]);
  emit<uint32_t>(func_index);
  emit<Address>(lazy_compile_target);
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  static_assert(kInstrSize == kInt32Size);
  static_assert(kJumpTableSlotSize == 2 * kInstrSize);

  // Load from [pc + kInstrSize] to pc. Note that {pc} points two instructions
  // after the currently executing one.
  const uint32_t inst[kJumpTableSlotSize / kInstrSize] = {
      0xe51ff004,  // ldr pc, [pc, -4]
      0x00000000,  // target
  };

  // This function is also used for patching existing jump slots and the writes
  // need to be atomic.
  emit<uint32_t>(inst[0], kRelaxedStore);
  emit<uint32_t>(target, kRelaxedStore);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  static_assert(kJumpTableSlotSize == kFarJumpTableSlotSize);
  EmitJumpSlot(target);
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

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
}

#elif V8_TARGET_ARCH_ARM64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  uint16_t func_index_low = func_index & 0xffff;
  uint16_t func_index_high = func_index >> 16;

  // TODO(sroettger): This bti instruction is a temporary fix for crashes that
  // we observed in the wild. We can probably avoid this again if we change the
  // callee to jump to the far jump table instead.
  const uint32_t inst[kLazyCompileTableSlotSize / 4] = {
      0xd50324df,  // bti.jc
      0x52800008,  // mov  w8, func_index_low
      0x72a00008,  // movk w8, func_index_high, LSL#0x10
      0x14000000,  // b lazy_compile_target
  };
  static_assert(kWasmCompileLazyFuncIndexRegister == x8);

  int64_t target_offset = MacroAssembler::CalculateTargetOffset(
      lazy_compile_target, RelocInfo::NO_INFO, pc_ + 3 * kInstrSize);
  DCHECK(MacroAssembler::IsNearCallOffset(target_offset));

  emit<uint32_t>(inst[0]);
  emit<uint32_t>(inst[1] | Assembler::ImmMoveWide(func_index_low));
  emit<uint32_t>(inst[2] | Assembler::ImmMoveWide(func_index_high));
  emit<uint32_t>(inst[3] | Assembler::ImmUncondBranch(
                               base::checked_cast<int32_t>(target_offset)));
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
  static constexpr ptrdiff_t kCodeEntryMarkerSize = kInstrSize;
#else
  static constexpr ptrdiff_t kCodeEntryMarkerSize = 0;
#endif

  int64_t target_offset = MacroAssembler::CalculateTargetOffset(
      target, RelocInfo::NO_INFO, pc_ + kCodeEntryMarkerSize);
  if (!MacroAssembler::IsNearCallOffset(target_offset)) {
    return false;
  }

#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
  uint32_t bti_inst = 0xd503245f;  // bti c
  emit<uint32_t>(bti_inst, kRelaxedStore);
#endif

  uint32_t branch_inst =
      0x14000000 |
      Assembler::ImmUncondBranch(base::checked_cast<int32_t>(target_offset));
  emit<uint32_t>(branch_inst, kRelaxedStore);

  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  DCHECK(TmpList()->IncludesAliasOf(x16));

  const uint32_t inst[kFarJumpTableSlotSize / 4] = {
      0x58000050,  // ldr x16, #8
      0xd61f0200,  // br x16
      0x00000000,  // target[0]
      0x00000000,  // target[1]
  };
  emit<uint32_t>(inst[0]);
  emit<uint32_t>(inst[1]);
  emit<Address>(target);

  static_assert(2 * kInstrSize == kSystemPointerSize);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  // See {EmitFarJumpSlot} for the offset of the target (16 bytes with
  // CFI enabled, 8 bytes otherwise).
  int kTargetOffset = 2 * kInstrSize;
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

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
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
  intptr_t relative_target = reinterpret_cast<uint8_t*>(target) - pc_;

  if (!is_int32(relative_target / 2)) {
    return false;
  }

  brcl(al, Operand(relative_target / 2));
  nop(0);  // make the slot align to 8 bytes
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  Label target_addr;
  lgrl(ip, &target_addr);  // 6 bytes
  b(ip);                   // 8 bytes

  CHECK_EQ(reinterpret_cast<Address>(pc_) & 0x7, 0);  // Alignment
  bind(&target_addr);
  dp(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  Address target_addr = slot + 8;
  reinterpret_cast<std::atomic<Address>*>(target_addr)
      ->store(target, std::memory_order_relaxed);
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % 2);
  for (; bytes > 0; bytes -= 2) {
    nop(0);
  }
}

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
}

#elif V8_TARGET_ARCH_MIPS64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  int start = pc_offset();
  li(kWasmCompileLazyFuncIndexRegister, func_index);  // max. 2 instr
  // Jump produces max. 4 instructions for 32-bit platform
  // and max. 6 instructions for 64-bit platform.
  Jump(lazy_compile_target, RelocInfo::NO_INFO);
  int nop_bytes = start + kLazyCompileTableSlotSize - pc_offset();
  DCHECK_EQ(nop_bytes % kInstrSize, 0);
  for (int i = 0; i < nop_bytes; i += kInstrSize) nop();
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  PatchAndJump(target);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  li(t9, Operand(target, RelocInfo::OFF_HEAP_TARGET));
  Jump(t9);
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

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
}

#elif V8_TARGET_ARCH_LOONG64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  DCHECK(is_int32(func_index));
  int start = pc_offset();
  li(kWasmCompileLazyFuncIndexRegister, (int32_t)func_index);  // max. 2 instr
  // EmitJumpSlot produces 1 instructions.
  CHECK(EmitJumpSlot(lazy_compile_target));
  int nop_bytes = start + kLazyCompileTableSlotSize - pc_offset();
  DCHECK_EQ(nop_bytes % kInstrSize, 0);
  for (int i = 0; i < nop_bytes; i += kInstrSize) nop();
}
bool JumpTableAssembler::EmitJumpSlot(Address target) {
  intptr_t relative_target = reinterpret_cast<uint8_t*>(target) - pc_;
  DCHECK_EQ(relative_target % 4, 0);
  intptr_t instr_offset = relative_target / kInstrSize;
  if (!is_int26(instr_offset)) {
    return false;
  }

  b(instr_offset);
  return true;
}
void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  pcaddi(t7, 4);
  Ld_d(t7, MemOperand(t7, 0));
  jirl(zero_reg, t7, 0);
  nop();  // pc_ should be align.
  DCHECK_EQ(reinterpret_cast<uint64_t>(pc_) % 8, 0);
  dq(target);
}
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  Address target_addr = slot + kFarJumpTableSlotSize - 8;
  reinterpret_cast<std::atomic<Address>*>(target_addr)
      ->store(target, std::memory_order_relaxed);
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % kInstrSize);
  for (; bytes > 0; bytes -= kInstrSize) {
    nop();
  }
}

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
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
  intptr_t relative_target = reinterpret_cast<uint8_t*>(target) - pc_;

  if (!is_int26(relative_target)) {
    return false;
  }

  b(relative_target, LeaveLK);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  uint8_t* start = pc_;
  mov(ip, Operand(reinterpret_cast<Address>(start + kFarJumpTableSlotSize -
                                            8)));  // 5 instr
  LoadU64(ip, MemOperand(ip));
  mtctr(ip);
  bctr();
  uint8_t* end = pc_;
  int used = end - start;
  CHECK(used < kFarJumpTableSlotSize - 8);
  NopBytes(kFarJumpTableSlotSize - 8 - used);
  CHECK_EQ(reinterpret_cast<Address>(pc_) & 0x7, 0);  // Alignment
  dp(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  Address target_addr = slot + kFarJumpTableSlotSize - 8;
  reinterpret_cast<std::atomic<Address>*>(target_addr)
      ->store(target, std::memory_order_relaxed);
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  DCHECK_EQ(0, bytes % 4);
  for (; bytes > 0; bytes -= 4) {
    nop(0);
  }
}

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
}

#elif V8_TARGET_ARCH_RISCV64
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  const int tile = pc_offset() + kLazyCompileTableSlotSize;
  li(kWasmCompileLazyFuncIndexRegister, func_index);  // max. 2 instr
  // Jump produces max. 8 instructions (include constant pool and j)
  Jump(lazy_compile_target, RelocInfo::NO_INFO);
  while (pc_offset() + kInstrSize <= tile) {
    nop();
  }
  // the earlier JIT code still may contain compressed instruction and thus
  // cannot be adjusted with only a sequence of NOP's
  if (v8_flags.riscv_c_extension && pc_offset() + kShortInstrSize <= tile) {
    c_nop();
  }
  DCHECK_EQ(pc_offset(), tile);
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  PatchAndJump(target);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  UseScratchRegisterScope temp(this);
  Register rd = temp.Acquire();
  auipc(rd, 0);
  ld(rd, rd, 4 * kInstrSize);
  Jump(rd);
  nop();
  dq(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  for (; bytes >= kInstrSize; bytes -= kInstrSize) {
    nop();
  }
  if (v8_flags.riscv_c_extension && bytes >= kShortInstrSize) {
    c_nop();
    bytes -= kShortInstrSize;
  }
  DCHECK_EQ(0, bytes);
}

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
}

#elif V8_TARGET_ARCH_RISCV32
void JumpTableAssembler::EmitLazyCompileJumpSlot(uint32_t func_index,
                                                 Address lazy_compile_target) {
  const int tile = pc_offset() + kLazyCompileTableSlotSize;
  li(kWasmCompileLazyFuncIndexRegister, func_index);  // max. 2 instr
  // Jump produces max. 8 instructions (include constant pool and j)
  Jump(lazy_compile_target, RelocInfo::NO_INFO);
  while (pc_offset() + kInstrSize <= tile) {
    nop();
  }
  // the earlier JIT code still may contain compressed instruction and thus
  // cannot be adjusted with only a sequence of NOP's
  if (v8_flags.riscv_c_extension && pc_offset() + kShortInstrSize <= tile) {
    c_nop();
  }
  DCHECK_EQ(pc_offset(), tile);
}

bool JumpTableAssembler::EmitJumpSlot(Address target) {
  PatchAndJump(target);
  return true;
}

void JumpTableAssembler::EmitFarJumpSlot(Address target) {
  UseScratchRegisterScope temp(this);
  Register rd = temp.Acquire();
  auipc(rd, 0);
  lw(rd, rd, 4 * kInstrSize);
  Jump(rd);
  nop();
  dq(target);
}

// static
void JumpTableAssembler::PatchFarJumpSlot(Address slot, Address target) {
  UNREACHABLE();
}

void JumpTableAssembler::NopBytes(int bytes) {
  DCHECK_LE(0, bytes);
  for (; bytes >= kInstrSize; bytes -= kInstrSize) {
    nop();
  }
  if (v8_flags.riscv_c_extension && bytes >= kShortInstrSize) {
    c_nop();
    bytes -= kShortInstrSize;
  }
  DCHECK_EQ(0, bytes);
}

void JumpTableAssembler::SkipUntil(int offset) {
  // On this platform the jump table is not zapped with valid instructions, so
  // skipping over bytes is not allowed.
  DCHECK_EQ(offset, pc_offset());
}

#else
#error Unknown architecture.
#endif

}  // namespace wasm
}  // namespace internal
}  // namespace v8

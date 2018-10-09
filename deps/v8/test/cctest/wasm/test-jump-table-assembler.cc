// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/macro-assembler-inl.h"
#include "src/simulator.h"
#include "src/utils.h"
#include "src/wasm/jump-table-assembler.h"
#include "test/cctest/cctest.h"
#include "test/common/assembler-tester.h"

namespace v8 {
namespace internal {
namespace wasm {

#if 0
#define TRACE(...) PrintF(__VA_ARGS__)
#else
#define TRACE(...)
#endif

#define __ masm.

// TODO(v8:7424,v8:8018): Extend this test to all architectures.
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM || \
    V8_TARGET_ARCH_ARM64

namespace {

static volatile int global_stop_bit = 0;

Address GenerateJumpTableThunk(Address jump_target) {
  size_t allocated;
  byte* buffer;
#if V8_TARGET_ARCH_ARM64
  // TODO(wasm): Currently {kMaxWasmCodeMemory} limits code sufficiently, so
  // that the jump table only supports {near_call} distances.
  const uintptr_t kThunkAddrMask = (1 << WhichPowerOf2(kMaxWasmCodeMemory)) - 1;
  const int kArbitrarilyChosenRetryCount = 10;  // Retry to avoid flakes.
  for (int retry = 0; retry < kArbitrarilyChosenRetryCount; ++retry) {
    Address random_addr = reinterpret_cast<Address>(GetRandomMmapAddr());
    void* address = reinterpret_cast<void*>((jump_target & ~kThunkAddrMask) |
                                            (random_addr & kThunkAddrMask));
    buffer = AllocateAssemblerBuffer(
        &allocated, AssemblerBase::kMinimalBufferSize, address);
    Address bufferptr = reinterpret_cast<uintptr_t>(buffer);
    if ((bufferptr & ~kThunkAddrMask) == (jump_target & ~kThunkAddrMask)) break;
  }
#else
  buffer = AllocateAssemblerBuffer(
      &allocated, AssemblerBase::kMinimalBufferSize, GetRandomMmapAddr());
#endif
  MacroAssembler masm(nullptr, AssemblerOptions{}, buffer,
                      static_cast<int>(allocated), CodeObjectRequired::kNo);

  Label exit;
  Register scratch = kReturnRegister0;
  Address stop_bit_address = reinterpret_cast<Address>(&global_stop_bit);
#if V8_TARGET_ARCH_X64
  __ Move(scratch, stop_bit_address, RelocInfo::NONE);
  __ testl(MemOperand(scratch, 0), Immediate(1));
  __ j(not_zero, &exit);
  __ Jump(jump_target, RelocInfo::NONE);
#elif V8_TARGET_ARCH_IA32
  __ Move(scratch, Immediate(stop_bit_address, RelocInfo::NONE));
  __ test(MemOperand(scratch, 0), Immediate(1));
  __ j(not_zero, &exit);
  __ jmp(jump_target, RelocInfo::NONE);
#elif V8_TARGET_ARCH_ARM
  __ mov(scratch, Operand(stop_bit_address, RelocInfo::NONE));
  __ ldr(scratch, MemOperand(scratch, 0));
  __ tst(scratch, Operand(1));
  __ b(ne, &exit);
  __ Jump(jump_target, RelocInfo::NONE);
#elif V8_TARGET_ARCH_ARM64
  __ Mov(scratch, Operand(stop_bit_address, RelocInfo::NONE));
  __ Ldr(scratch, MemOperand(scratch, 0));
  __ Tbnz(scratch, 0, &exit);
  __ Mov(scratch, Immediate(jump_target, RelocInfo::NONE));
  __ Br(scratch);
#else
#error Unsupported architecture
#endif
  __ bind(&exit);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(nullptr, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  return reinterpret_cast<Address>(buffer);
}

class JumpTableRunner : public v8::base::Thread {
 public:
  JumpTableRunner(Address slot_address, int runner_id)
      : Thread(Options("JumpTableRunner")),
        slot_address_(slot_address),
        runner_id_(runner_id) {}

  void Run() override {
    TRACE("Runner #%d is starting ...\n", runner_id_);
    GeneratedCode<void>::FromAddress(CcTest::i_isolate(), slot_address_).Call();
    TRACE("Runner #%d is stopping ...\n", runner_id_);
    USE(runner_id_);
  }

 private:
  Address slot_address_;
  int runner_id_;
};

class JumpTablePatcher : public v8::base::Thread {
 public:
  JumpTablePatcher(Address slot_start, uint32_t slot_index, Address thunk1,
                   Address thunk2)
      : Thread(Options("JumpTablePatcher")),
        slot_start_(slot_start),
        slot_index_(slot_index),
        thunks_{thunk1, thunk2} {}

  void Run() override {
    TRACE("Patcher is starting ...\n");
    constexpr int kNumberOfPatchIterations = 64;
    for (int i = 0; i < kNumberOfPatchIterations; ++i) {
      TRACE("  patch slot " V8PRIxPTR_FMT " to thunk #%d\n",
            slot_start_ + JumpTableAssembler::SlotIndexToOffset(slot_index_),
            i % 2);
      JumpTableAssembler::PatchJumpTableSlot(
          slot_start_, slot_index_, thunks_[i % 2], WasmCode::kFlushICache);
    }
    TRACE("Patcher is stopping ...\n");
  }

 private:
  Address slot_start_;
  uint32_t slot_index_;
  Address thunks_[2];
};

}  // namespace

// This test is intended to stress concurrent patching of jump-table slots. It
// uses the following setup:
//   1) Picks a particular slot of the jump-table. Slots are iterated over to
//      ensure multiple entries (at different offset alignments) are tested.
//   2) Starts multiple runners that spin through the above slot. The runners
//      use thunk code that will jump to the same jump-table slot repeatedly
//      until the {global_stop_bit} indicates a test-end condition.
//   3) Start a patcher that repeatedly patches the jump-table slot back and
//      forth between two thunk. If there is a race then chances are high that
//      one of the runners is currently executing the jump-table slot.
TEST(JumpTablePatchingStress) {
  constexpr int kJumpTableSlotCount = 128;
  constexpr int kNumberOfRunnerThreads = 5;

  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(
      &allocated,
      JumpTableAssembler::SizeForNumberOfSlots(kJumpTableSlotCount));

  // Iterate through jump-table slots to hammer at different alignments within
  // the jump-table, thereby increasing stress for variable-length ISAs.
  Address slot_start = reinterpret_cast<Address>(buffer);
  for (int slot = 0; slot < kJumpTableSlotCount; ++slot) {
    TRACE("Hammering on jump table slot #%d ...\n", slot);
    uint32_t slot_offset = JumpTableAssembler::SlotIndexToOffset(slot);
    Address thunk1 = GenerateJumpTableThunk(slot_start + slot_offset);
    Address thunk2 = GenerateJumpTableThunk(slot_start + slot_offset);
    TRACE("  generated thunk1: " V8PRIxPTR_FMT "\n", thunk1);
    TRACE("  generated thunk2: " V8PRIxPTR_FMT "\n", thunk2);
    JumpTableAssembler::PatchJumpTableSlot(slot_start, slot, thunk1,
                                           WasmCode::kFlushICache);

    // Start multiple runner threads and a patcher thread that hammer on the
    // same jump-table slot concurrently.
    std::list<JumpTableRunner> runners;
    for (int runner = 0; runner < kNumberOfRunnerThreads; ++runner) {
      runners.emplace_back(slot_start + slot_offset, runner);
    }
    JumpTablePatcher patcher(slot_start, slot, thunk1, thunk2);
    global_stop_bit = 0;  // Signal runners to keep going.
    for (auto& runner : runners) runner.Start();
    patcher.Start();
    patcher.Join();
    global_stop_bit = -1;  // Signal runners to stop.
    for (auto& runner : runners) runner.Join();
  }
}

#endif  // V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM ||
        // V8_TARGET_ARCH_ARM64

#undef __
#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

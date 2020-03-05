// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/execution/simulator.h"
#include "src/utils/utils.h"
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

namespace {

static volatile int global_stop_bit = 0;

constexpr int kJumpTableSlotCount = 128;
constexpr uint32_t kJumpTableSize =
    JumpTableAssembler::SizeForNumberOfSlots(kJumpTableSlotCount);

// Must be a safe commit page size.
constexpr size_t kThunkBufferSize = 4 * KB;

#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_X64
constexpr uint32_t kAvailableBufferSlots =
    (kMaxWasmCodeSpaceSize - kJumpTableSize) / kThunkBufferSize;
constexpr uint32_t kBufferSlotStartOffset =
    RoundUp<kThunkBufferSize>(kJumpTableSize);
#else
constexpr uint32_t kAvailableBufferSlots = 0;
#endif

Address AllocateJumpTableThunk(
    Address jump_target, byte* thunk_slot_buffer,
    std::bitset<kAvailableBufferSlots>* used_slots,
    std::vector<std::unique_ptr<TestingAssemblerBuffer>>* thunk_buffers) {
#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_X64
  // To guarantee that the branch range lies within the near-call range,
  // generate the thunk in the same (kMaxWasmCodeSpaceSize-sized) buffer as the
  // jump_target itself.
  //
  // Allocate a slot that we haven't already used. This is necessary because
  // each test iteration expects to generate two unique addresses and we leave
  // each slot executable (and not writable).
  base::RandomNumberGenerator* rng =
      CcTest::i_isolate()->random_number_generator();
  // Ensure a chance of completion without too much thrashing.
  DCHECK(used_slots->count() < (used_slots->size() / 2));
  int buffer_index;
  do {
    buffer_index = rng->NextInt(kAvailableBufferSlots);
  } while (used_slots->test(buffer_index));
  used_slots->set(buffer_index);
  return reinterpret_cast<Address>(thunk_slot_buffer +
                                   buffer_index * kThunkBufferSize);

#else
  USE(thunk_slot_buffer);
  USE(used_slots);
  thunk_buffers->emplace_back(
      AllocateAssemblerBuffer(kThunkBufferSize, GetRandomMmapAddr()));
  return reinterpret_cast<Address>(thunk_buffers->back()->start());
#endif
}

void CompileJumpTableThunk(Address thunk, Address jump_target) {
  MacroAssembler masm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      ExternalAssemblerBuffer(reinterpret_cast<void*>(thunk),
                                              kThunkBufferSize));

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
#elif V8_TARGET_ARCH_PPC64
  __ mov(scratch, Operand(stop_bit_address, RelocInfo::NONE));
  __ LoadP(scratch, MemOperand(scratch));
  __ cmpi(scratch, Operand::Zero());
  __ bne(&exit);
  __ mov(scratch, Operand(jump_target, RelocInfo::NONE));
  __ Jump(scratch);
#elif V8_TARGET_ARCH_S390X
  __ mov(scratch, Operand(stop_bit_address, RelocInfo::NONE));
  __ LoadP(scratch, MemOperand(scratch));
  __ CmpP(scratch, Operand(0));
  __ bne(&exit);
  __ mov(scratch, Operand(jump_target, RelocInfo::NONE));
  __ Jump(scratch);
#elif V8_TARGET_ARCH_MIPS64
  __ li(scratch, Operand(stop_bit_address, RelocInfo::NONE));
  __ Lw(scratch, MemOperand(scratch, 0));
  __ Branch(&exit, ne, scratch, Operand(zero_reg));
  __ Jump(jump_target, RelocInfo::NONE);
#elif V8_TARGET_ARCH_MIPS
  __ li(scratch, Operand(stop_bit_address, RelocInfo::NONE));
  __ lw(scratch, MemOperand(scratch, 0));
  __ Branch(&exit, ne, scratch, Operand(zero_reg));
  __ Jump(jump_target, RelocInfo::NONE);
#else
#error Unsupported architecture
#endif
  __ bind(&exit);
  __ Ret();

  FlushInstructionCache(thunk, kThunkBufferSize);
  CHECK(SetPermissions(GetPlatformPageAllocator(), thunk, kThunkBufferSize,
                       v8::PageAllocator::kReadExecute));
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
                   Address thunk2, base::Mutex* jump_table_mutex)
      : Thread(Options("JumpTablePatcher")),
        slot_start_(slot_start),
        slot_index_(slot_index),
        thunks_{thunk1, thunk2},
        jump_table_mutex_(jump_table_mutex) {}

  void Run() override {
    TRACE("Patcher %p is starting ...\n", this);
    Address slot_address =
        slot_start_ + JumpTableAssembler::JumpSlotIndexToOffset(slot_index_);
    // First, emit code to the two thunks.
    for (Address thunk : thunks_) {
      CompileJumpTableThunk(thunk, slot_address);
    }
    // Then, repeatedly patch the jump table to jump to one of the two thunks.
    constexpr int kNumberOfPatchIterations = 64;
    for (int i = 0; i < kNumberOfPatchIterations; ++i) {
      TRACE("  patcher %p patch slot " V8PRIxPTR_FMT
            " to thunk #%d (" V8PRIxPTR_FMT ")\n",
            this, slot_address, i % 2, thunks_[i % 2]);
      base::MutexGuard jump_table_guard(jump_table_mutex_);
      JumpTableAssembler::PatchJumpTableSlot(
          slot_start_ + JumpTableAssembler::JumpSlotIndexToOffset(slot_index_),
          kNullAddress, thunks_[i % 2]);
    }
    TRACE("Patcher %p is stopping ...\n", this);
  }

 private:
  Address slot_start_;
  uint32_t slot_index_;
  Address thunks_[2];
  base::Mutex* jump_table_mutex_;
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
  constexpr int kNumberOfRunnerThreads = 5;
  constexpr int kNumberOfPatcherThreads = 3;

#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_X64
  // We need the branches (from CompileJumpTableThunk) to be within near-call
  // range of the jump table slots. The address hint to AllocateAssemblerBuffer
  // is not reliable enough to guarantee that we can always achieve this with
  // separate allocations, so for Arm64 we generate all code in a single
  // kMaxMasmCodeMemory-sized chunk.
  STATIC_ASSERT(kMaxWasmCodeSpaceSize >= kJumpTableSize);
  auto buffer = AllocateAssemblerBuffer(kMaxWasmCodeSpaceSize);
  byte* thunk_slot_buffer = buffer->start() + kBufferSlotStartOffset;
#else
  auto buffer = AllocateAssemblerBuffer(kJumpTableSize);
  byte* thunk_slot_buffer = nullptr;
#endif

  std::bitset<kAvailableBufferSlots> used_thunk_slots;
  buffer->MakeWritableAndExecutable();

  // Iterate through jump-table slots to hammer at different alignments within
  // the jump-table, thereby increasing stress for variable-length ISAs.
  Address slot_start = reinterpret_cast<Address>(buffer->start());
  for (int slot = 0; slot < kJumpTableSlotCount; ++slot) {
    TRACE("Hammering on jump table slot #%d ...\n", slot);
    uint32_t slot_offset = JumpTableAssembler::JumpSlotIndexToOffset(slot);
    std::vector<std::unique_ptr<TestingAssemblerBuffer>> thunk_buffers;
    // Patch the jump table slot to jump to itself. This will later be patched
    // by the patchers.
    Address slot_addr =
        slot_start + JumpTableAssembler::JumpSlotIndexToOffset(slot);
    JumpTableAssembler::PatchJumpTableSlot(slot_addr, kNullAddress, slot_addr);
    // For each patcher, generate two thunks where this patcher can emit code
    // which finally jumps back to {slot} in the jump table.
    std::vector<Address> patcher_thunks;
    for (int i = 0; i < 2 * kNumberOfPatcherThreads; ++i) {
      Address thunk =
          AllocateJumpTableThunk(slot_start + slot_offset, thunk_slot_buffer,
                                 &used_thunk_slots, &thunk_buffers);
      ZapCode(thunk, kThunkBufferSize);
      patcher_thunks.push_back(thunk);
      TRACE("  generated jump thunk: " V8PRIxPTR_FMT "\n",
            patcher_thunks.back());
    }

    // Start multiple runner threads that execute the jump table slot
    // concurrently.
    std::list<JumpTableRunner> runners;
    for (int runner = 0; runner < kNumberOfRunnerThreads; ++runner) {
      runners.emplace_back(slot_start + slot_offset, runner);
    }
    // Start multiple patcher thread that concurrently generate code and insert
    // jumps to that into the jump table slot.
    std::list<JumpTablePatcher> patchers;
    // Only one patcher should modify the jump table at a time.
    base::Mutex jump_table_mutex;
    for (int i = 0; i < kNumberOfPatcherThreads; ++i) {
      patchers.emplace_back(slot_start, slot, patcher_thunks[2 * i],
                            patcher_thunks[2 * i + 1], &jump_table_mutex);
    }
    global_stop_bit = 0;  // Signal runners to keep going.
    for (auto& runner : runners) CHECK(runner.Start());
    for (auto& patcher : patchers) CHECK(patcher.Start());
    for (auto& patcher : patchers) patcher.Join();
    global_stop_bit = -1;  // Signal runners to stop.
    for (auto& runner : runners) runner.Join();
  }
}

#undef __
#undef TRACE

}  // namespace wasm
}  // namespace internal
}  // namespace v8

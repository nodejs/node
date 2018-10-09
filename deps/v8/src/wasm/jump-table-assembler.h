// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_JUMP_TABLE_ASSEMBLER_H_
#define V8_WASM_JUMP_TABLE_ASSEMBLER_H_

#include "src/macro-assembler.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace wasm {

// The jump table is the central dispatch point for all (direct and indirect)
// invocations in WebAssembly. It holds one slot per function in a module, with
// each slot containing a dispatch to the currently published {WasmCode} that
// corresponds to the function.
//
// Note that the table is split into lines of fixed size, with lines laid out
// consecutively within the executable memory of the {NativeModule}. The slots
// in turn are consecutive within a line, but do not cross line boundaries.
//
//   +- L1 -------------------+ +- L2 -------------------+ +- L3 ...
//   | S1 | S2 | ... | Sn | x | | S1 | S2 | ... | Sn | x | | S1  ...
//   +------------------------+ +------------------------+ +---- ...
//
// The above illustrates jump table lines {Li} containing slots {Si} with each
// line containing {n} slots and some padding {x} for alignment purposes.
class JumpTableAssembler : public TurboAssembler {
 public:
  // Translate an offset into the continuous jump table to a jump table index.
  static uint32_t SlotOffsetToIndex(uint32_t slot_offset) {
    uint32_t line_index = slot_offset / kJumpTableLineSize;
    uint32_t line_offset = slot_offset % kJumpTableLineSize;
    DCHECK_EQ(0, line_offset % kJumpTableSlotSize);
    return line_index * kJumpTableSlotsPerLine +
           line_offset / kJumpTableSlotSize;
  }

  // Translate a jump table index to an offset into the continuous jump table.
  static uint32_t SlotIndexToOffset(uint32_t slot_index) {
    uint32_t line_index = slot_index / kJumpTableSlotsPerLine;
    uint32_t line_offset =
        (slot_index % kJumpTableSlotsPerLine) * kJumpTableSlotSize;
    return line_index * kJumpTableLineSize + line_offset;
  }

  // Determine the size of a jump table containing the given number of slots.
  static constexpr uint32_t SizeForNumberOfSlots(uint32_t slot_count) {
    // TODO(wasm): Once the {RoundUp} utility handles non-powers of two values,
    // use: {RoundUp<kJumpTableSlotsPerLine>(slot_count) * kJumpTableLineSize}
    return ((slot_count + kJumpTableSlotsPerLine - 1) /
            kJumpTableSlotsPerLine) *
           kJumpTableLineSize;
  }

  static void EmitLazyCompileJumpSlot(Address base, uint32_t slot_index,
                                      uint32_t func_index,
                                      Address lazy_compile_target,
                                      WasmCode::FlushICache flush_i_cache) {
    Address slot = base + SlotIndexToOffset(slot_index);
    JumpTableAssembler jtasm(slot);
    jtasm.EmitLazyCompileJumpSlot(func_index, lazy_compile_target);
    jtasm.NopBytes(kJumpTableSlotSize - jtasm.pc_offset());
    if (flush_i_cache) {
      Assembler::FlushICache(slot, kJumpTableSlotSize);
    }
  }

  static void PatchJumpTableSlot(Address base, uint32_t slot_index,
                                 Address new_target,
                                 WasmCode::FlushICache flush_i_cache) {
    Address slot = base + SlotIndexToOffset(slot_index);
    JumpTableAssembler jtasm(slot);
    jtasm.EmitJumpSlot(new_target);
    jtasm.NopBytes(kJumpTableSlotSize - jtasm.pc_offset());
    if (flush_i_cache) {
      Assembler::FlushICache(slot, kJumpTableSlotSize);
    }
  }

 private:
  // Instantiate a {JumpTableAssembler} for patching.
  explicit JumpTableAssembler(Address slot_addr, int size = 256)
      : TurboAssembler(nullptr, JumpTableAssemblerOptions(),
                       reinterpret_cast<void*>(slot_addr), size,
                       CodeObjectRequired::kNo) {}

// To allow concurrent patching of the jump table entries, we need to ensure
// that the instruction containing the call target does not cross cache-line
// boundaries. The jump table line size has been chosen to satisfy this.
#if V8_TARGET_ARCH_X64
  static constexpr int kJumpTableLineSize = 64;
  static constexpr int kJumpTableSlotSize = 18;
#elif V8_TARGET_ARCH_IA32
  static constexpr int kJumpTableLineSize = 64;
  static constexpr int kJumpTableSlotSize = 10;
#elif V8_TARGET_ARCH_ARM
  static constexpr int kJumpTableLineSize = 5 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 5 * kInstrSize;
#elif V8_TARGET_ARCH_ARM64
  static constexpr int kJumpTableLineSize = 3 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 3 * kInstrSize;
#elif V8_TARGET_ARCH_S390X
  static constexpr int kJumpTableLineSize = 20;
  static constexpr int kJumpTableSlotSize = 20;
#elif V8_TARGET_ARCH_S390
  static constexpr int kJumpTableLineSize = 14;
  static constexpr int kJumpTableSlotSize = 14;
#elif V8_TARGET_ARCH_PPC64
  static constexpr int kJumpTableLineSize = 48;
  static constexpr int kJumpTableSlotSize = 48;
#elif V8_TARGET_ARCH_PPC
  static constexpr int kJumpTableLineSize = 24;
  static constexpr int kJumpTableSlotSize = 24;
#elif V8_TARGET_ARCH_MIPS
  static constexpr int kJumpTableLineSize = 6 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 6 * kInstrSize;
#elif V8_TARGET_ARCH_MIPS64
  static constexpr int kJumpTableLineSize = 8 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 8 * kInstrSize;
#else
  static constexpr int kJumpTableLineSize = 1;
  static constexpr int kJumpTableSlotSize = 1;
#endif

  static constexpr int kJumpTableSlotsPerLine =
      kJumpTableLineSize / kJumpTableSlotSize;

  // {JumpTableAssembler} is never used during snapshot generation, and its code
  // must be independent of the code range of any isolate anyway. Just ensure
  // that no relocation information is recorded, there is no buffer to store it
  // since it is instantiated in patching mode in existing code directly.
  static AssemblerOptions JumpTableAssemblerOptions() {
    AssemblerOptions options;
    options.disable_reloc_info_for_patching = true;
    return options;
  }

  void EmitLazyCompileJumpSlot(uint32_t func_index,
                               Address lazy_compile_target);

  void EmitJumpSlot(Address target);

  void NopBytes(int bytes);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_JUMP_TABLE_ASSEMBLER_H_

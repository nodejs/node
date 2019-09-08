// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_JUMP_TABLE_ASSEMBLER_H_
#define V8_WASM_JUMP_TABLE_ASSEMBLER_H_

#include "src/codegen/macro-assembler.h"
#include "src/wasm/wasm-code-manager.h"

namespace v8 {
namespace internal {
namespace wasm {

// The jump table is the central dispatch point for all (direct and indirect)
// invocations in WebAssembly. It holds one slot per function in a module, with
// each slot containing a dispatch to the currently published {WasmCode} that
// corresponds to the function.
//
// Additionally to this main jump table, there exist special jump tables for
// other purposes:
// - the runtime stub table contains one entry per wasm runtime stub (see
//   {WasmCode::RuntimeStubId}, which jumps to the corresponding embedded
//   builtin.
// - the lazy compile table contains one entry per wasm function which jumps to
//   the common {WasmCompileLazy} builtin and passes the function index that was
//   invoked.
//
// The main jump table is split into lines of fixed size, with lines laid out
// consecutively within the executable memory of the {NativeModule}. The slots
// in turn are consecutive within a line, but do not cross line boundaries.
//
//   +- L1 -------------------+ +- L2 -------------------+ +- L3 ...
//   | S1 | S2 | ... | Sn | x | | S1 | S2 | ... | Sn | x | | S1  ...
//   +------------------------+ +------------------------+ +---- ...
//
// The above illustrates jump table lines {Li} containing slots {Si} with each
// line containing {n} slots and some padding {x} for alignment purposes.
// Other jump tables are just consecutive.
//
// The main jump table will be patched concurrently while other threads execute
// it. The code at the new target might also have been emitted concurrently, so
// we need to ensure that there is proper synchronization between code emission,
// jump table patching and code execution.
// On Intel platforms, this all works out of the box because there is cache
// coherency between i-cache and d-cache.
// On ARM, it is safe because the i-cache flush after code emission executes an
// "ic ivau" (Instruction Cache line Invalidate by Virtual Address to Point of
// Unification), which broadcasts to all cores. A core which sees the jump table
// update thus also sees the new code. Since the other core does not explicitly
// execute an "isb" (Instruction Synchronization Barrier), it might still
// execute the old code afterwards, which is no problem, since that code remains
// available until it is garbage collected. Garbage collection itself is a
// synchronization barrier though.
class V8_EXPORT_PRIVATE JumpTableAssembler : public MacroAssembler {
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
  static uint32_t JumpSlotIndexToOffset(uint32_t slot_index) {
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

  // Translate a stub slot index to an offset into the continuous jump table.
  static uint32_t StubSlotIndexToOffset(uint32_t slot_index) {
    return slot_index * kJumpTableStubSlotSize;
  }

  // Translate a slot index to an offset into the lazy compile table.
  static uint32_t LazyCompileSlotIndexToOffset(uint32_t slot_index) {
    return slot_index * kLazyCompileTableSlotSize;
  }

  // Determine the size of a jump table containing only runtime stub slots.
  static constexpr uint32_t SizeForNumberOfStubSlots(uint32_t slot_count) {
    return slot_count * kJumpTableStubSlotSize;
  }

  // Determine the size of a lazy compile table.
  static constexpr uint32_t SizeForNumberOfLazyFunctions(uint32_t slot_count) {
    return slot_count * kLazyCompileTableSlotSize;
  }

  static void GenerateLazyCompileTable(Address base, uint32_t num_slots,
                                       uint32_t num_imported_functions,
                                       Address wasm_compile_lazy_target) {
    uint32_t lazy_compile_table_size = num_slots * kLazyCompileTableSlotSize;
    // Assume enough space, so the Assembler does not try to grow the buffer.
    JumpTableAssembler jtasm(base, lazy_compile_table_size + 256);
    for (uint32_t slot_index = 0; slot_index < num_slots; ++slot_index) {
      DCHECK_EQ(slot_index * kLazyCompileTableSlotSize, jtasm.pc_offset());
      jtasm.EmitLazyCompileJumpSlot(slot_index + num_imported_functions,
                                    wasm_compile_lazy_target);
    }
    DCHECK_EQ(lazy_compile_table_size, jtasm.pc_offset());
    FlushInstructionCache(base, lazy_compile_table_size);
  }

  static void GenerateRuntimeStubTable(Address base, Address* targets,
                                       int num_stubs) {
    uint32_t table_size = num_stubs * kJumpTableStubSlotSize;
    // Assume enough space, so the Assembler does not try to grow the buffer.
    JumpTableAssembler jtasm(base, table_size + 256);
    int offset = 0;
    for (int index = 0; index < num_stubs; ++index) {
      DCHECK_EQ(offset, StubSlotIndexToOffset(index));
      DCHECK_EQ(offset, jtasm.pc_offset());
      jtasm.EmitRuntimeStubSlot(targets[index]);
      offset += kJumpTableStubSlotSize;
      jtasm.NopBytes(offset - jtasm.pc_offset());
    }
    FlushInstructionCache(base, table_size);
  }

  static void PatchJumpTableSlot(Address base, uint32_t slot_index,
                                 Address new_target,
                                 WasmCode::FlushICache flush_i_cache) {
    Address slot = base + JumpSlotIndexToOffset(slot_index);
    JumpTableAssembler jtasm(slot);
    jtasm.EmitJumpSlot(new_target);
    jtasm.NopBytes(kJumpTableSlotSize - jtasm.pc_offset());
    if (flush_i_cache) {
      FlushInstructionCache(slot, kJumpTableSlotSize);
    }
  }

 private:
  // Instantiate a {JumpTableAssembler} for patching.
  explicit JumpTableAssembler(Address slot_addr, int size = 256)
      : MacroAssembler(nullptr, JumpTableAssemblerOptions(),
                       CodeObjectRequired::kNo,
                       ExternalAssemblerBuffer(
                           reinterpret_cast<uint8_t*>(slot_addr), size)) {}

// To allow concurrent patching of the jump table entries, we need to ensure
// that the instruction containing the call target does not cross cache-line
// boundaries. The jump table line size has been chosen to satisfy this.
#if V8_TARGET_ARCH_X64
  static constexpr int kJumpTableLineSize = 64;
  static constexpr int kJumpTableSlotSize = 5;
  static constexpr int kLazyCompileTableSlotSize = 10;
  static constexpr int kJumpTableStubSlotSize = 18;
#elif V8_TARGET_ARCH_IA32
  static constexpr int kJumpTableLineSize = 64;
  static constexpr int kJumpTableSlotSize = 5;
  static constexpr int kLazyCompileTableSlotSize = 10;
  static constexpr int kJumpTableStubSlotSize = 10;
#elif V8_TARGET_ARCH_ARM
  static constexpr int kJumpTableLineSize = 3 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 3 * kInstrSize;
  static constexpr int kLazyCompileTableSlotSize = 5 * kInstrSize;
  static constexpr int kJumpTableStubSlotSize = 5 * kInstrSize;
#elif V8_TARGET_ARCH_ARM64
  static constexpr int kJumpTableLineSize = 1 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 1 * kInstrSize;
  static constexpr int kLazyCompileTableSlotSize = 3 * kInstrSize;
  static constexpr int kJumpTableStubSlotSize = 6 * kInstrSize;
#elif V8_TARGET_ARCH_S390X
  static constexpr int kJumpTableLineSize = 128;
  static constexpr int kJumpTableSlotSize = 14;
  static constexpr int kLazyCompileTableSlotSize = 20;
  static constexpr int kJumpTableStubSlotSize = 14;
#elif V8_TARGET_ARCH_PPC64
  static constexpr int kJumpTableLineSize = 64;
  static constexpr int kJumpTableSlotSize = 7 * kInstrSize;
  static constexpr int kLazyCompileTableSlotSize = 12 * kInstrSize;
  static constexpr int kJumpTableStubSlotSize = 7 * kInstrSize;
#elif V8_TARGET_ARCH_MIPS
  static constexpr int kJumpTableLineSize = 6 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 4 * kInstrSize;
  static constexpr int kLazyCompileTableSlotSize = 6 * kInstrSize;
  static constexpr int kJumpTableStubSlotSize = 4 * kInstrSize;
#elif V8_TARGET_ARCH_MIPS64
  static constexpr int kJumpTableLineSize = 8 * kInstrSize;
  static constexpr int kJumpTableSlotSize = 6 * kInstrSize;
  static constexpr int kLazyCompileTableSlotSize = 8 * kInstrSize;
  static constexpr int kJumpTableStubSlotSize = 6 * kInstrSize;
#else
  static constexpr int kJumpTableLineSize = 1;
  static constexpr int kJumpTableSlotSize = 1;
  static constexpr int kLazyCompileTableSlotSize = 1;
  static constexpr int kJumpTableStubSlotSize = 1;
#endif

  static constexpr int kJumpTableSlotsPerLine =
      kJumpTableLineSize / kJumpTableSlotSize;
  STATIC_ASSERT(kJumpTableSlotsPerLine >= 1);

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

  void EmitRuntimeStubSlot(Address builtin_target);

  void EmitJumpSlot(Address target);

  void NopBytes(int bytes);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_JUMP_TABLE_ASSEMBLER_H_

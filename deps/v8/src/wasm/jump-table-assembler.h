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

class JumpTableAssembler : public TurboAssembler {
 public:
  // Instantiate a {JumpTableAssembler} for patching.
  explicit JumpTableAssembler(Address slot_addr, int size = 256)
      : TurboAssembler(nullptr, JumpTableAssemblerOptions(),
                       reinterpret_cast<void*>(slot_addr), size,
                       CodeObjectRequired::kNo) {}

#if V8_TARGET_ARCH_X64
  static constexpr int kJumpTableSlotSize = 18;
#elif V8_TARGET_ARCH_IA32
  static constexpr int kJumpTableSlotSize = 10;
#elif V8_TARGET_ARCH_ARM
  static constexpr int kJumpTableSlotSize = 5 * kInstrSize;
#elif V8_TARGET_ARCH_ARM64
  static constexpr int kJumpTableSlotSize = 3 * kInstructionSize;
#elif V8_TARGET_ARCH_S390X
  static constexpr int kJumpTableSlotSize = 20;
#elif V8_TARGET_ARCH_S390
  static constexpr int kJumpTableSlotSize = 14;
#elif V8_TARGET_ARCH_PPC64
  static constexpr int kJumpTableSlotSize = 48;
#elif V8_TARGET_ARCH_PPC
  static constexpr int kJumpTableSlotSize = 24;
#elif V8_TARGET_ARCH_MIPS
  static constexpr int kJumpTableSlotSize = 6 * kInstrSize;
#elif V8_TARGET_ARCH_MIPS64
  static constexpr int kJumpTableSlotSize = 8 * kInstrSize;
#else
  static constexpr int kJumpTableSlotSize = 1;
#endif

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

  static void PatchJumpTableSlot(Address slot, Address new_target,
                                 WasmCode::FlushICache flush_i_cache) {
    JumpTableAssembler jsasm(slot);
    jsasm.EmitJumpSlot(new_target);
    jsasm.NopBytes(kJumpTableSlotSize - jsasm.pc_offset());
    if (flush_i_cache) {
      Assembler::FlushICache(slot, kJumpTableSlotSize);
    }
  }
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_JUMP_TABLE_ASSEMBLER_H_

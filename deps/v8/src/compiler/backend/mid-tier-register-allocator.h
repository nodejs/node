// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_MID_TIER_REGISTER_ALLOCATOR_H_
#define V8_COMPILER_BACKEND_MID_TIER_REGISTER_ALLOCATOR_H_

#include "src/base/compiler-specific.h"
#include "src/common/globals.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/backend/register-allocation.h"
#include "src/utils/bit-vector.h"
#include "src/utils/sparse-bit-vector.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class TickCounter;

namespace compiler {

class BlockState;
class VirtualRegisterData;

// The MidTierRegisterAllocator is a register allocator specifically designed to
// perform register allocation as fast as possible while minimizing spill moves.

class MidTierRegisterAllocationData final : public RegisterAllocationData {
 public:
  MidTierRegisterAllocationData(const RegisterConfiguration* config,
                                Zone* allocation_zone, Frame* frame,
                                InstructionSequence* code,
                                TickCounter* tick_counter,
                                const char* debug_name = nullptr);
  MidTierRegisterAllocationData(const MidTierRegisterAllocationData&) = delete;
  MidTierRegisterAllocationData& operator=(
      const MidTierRegisterAllocationData&) = delete;

  static MidTierRegisterAllocationData* cast(RegisterAllocationData* data) {
    DCHECK_EQ(data->type(), Type::kMidTier);
    return static_cast<MidTierRegisterAllocationData*>(data);
  }

  VirtualRegisterData& VirtualRegisterDataFor(int virtual_register);

  // Add a gap move between the given operands |from| and |to|.
  MoveOperands* AddGapMove(int instr_index, Instruction::GapPosition position,
                           const InstructionOperand& from,
                           const InstructionOperand& to);

  // Adds a gap move where both sides are PendingOperand operands.
  MoveOperands* AddPendingOperandGapMove(int instr_index,
                                         Instruction::GapPosition position);

  // Helpers to get a block from an |rpo_number| or |instr_index|.
  const InstructionBlock* GetBlock(const RpoNumber rpo_number);
  const InstructionBlock* GetBlock(int instr_index);

  // Returns a bitvector representing all the blocks that are dominated by the
  // output of the instruction in |block|.
  const SparseBitVector* GetBlocksDominatedBy(const InstructionBlock* block);

  // List of all instruction indexs that require a reference map.
  ZoneVector<int>& reference_map_instructions() {
    return reference_map_instructions_;
  }

  // Returns a bitvector representing the virtual registers that were spilled.
  BitVector& spilled_virtual_registers() { return spilled_virtual_registers_; }

  // This zone is for data structures only needed during register allocation
  // phases.
  Zone* allocation_zone() const { return allocation_zone_; }

  // This zone is for InstructionOperands and moves that live beyond register
  // allocation.
  Zone* code_zone() const { return code()->zone(); }

  BlockState& block_state(RpoNumber rpo_number);

  InstructionSequence* code() const { return code_; }
  Frame* frame() const { return frame_; }
  const char* debug_name() const { return debug_name_; }
  const RegisterConfiguration* config() const { return config_; }
  TickCounter* tick_counter() { return tick_counter_; }

 private:
  Zone* const allocation_zone_;
  Frame* const frame_;
  InstructionSequence* const code_;
  const char* const debug_name_;
  const RegisterConfiguration* const config_;

  ZoneVector<VirtualRegisterData> virtual_register_data_;
  ZoneVector<BlockState> block_states_;
  ZoneVector<int> reference_map_instructions_;
  BitVector spilled_virtual_registers_;

  TickCounter* const tick_counter_;
};

// Phase 1: Process instruction outputs to determine how each virtual register
// is defined.
void DefineOutputs(MidTierRegisterAllocationData* data);

// Phase 2: Allocate registers to instructions.
void AllocateRegisters(MidTierRegisterAllocationData* data);

// Phase 3: assign spilled operands to specific spill slots.
void AllocateSpillSlots(MidTierRegisterAllocationData* data);

// Phase 4: Populate reference maps for spilled references.
void PopulateReferenceMaps(MidTierRegisterAllocationData* data);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_MID_TIER_REGISTER_ALLOCATOR_H_

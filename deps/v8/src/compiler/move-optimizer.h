// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MOVE_OPTIMIZER_
#define V8_COMPILER_MOVE_OPTIMIZER_

#include "src/compiler/instruction.h"
#include "src/globals.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class V8_EXPORT_PRIVATE MoveOptimizer final {
 public:
  MoveOptimizer(Zone* local_zone, InstructionSequence* code);
  void Run();

 private:
  typedef ZoneVector<MoveOperands*> MoveOpVector;
  typedef ZoneVector<Instruction*> Instructions;

  InstructionSequence* code() const { return code_; }
  Zone* local_zone() const { return local_zone_; }
  Zone* code_zone() const { return code()->zone(); }
  MoveOpVector& local_vector() { return local_vector_; }

  // Consolidate moves into the first gap.
  void CompressGaps(Instruction* instr);

  // Attempt to push down to the last instruction those moves that can.
  void CompressBlock(InstructionBlock* block);

  // Consolidate moves into the first gap.
  void CompressMoves(ParallelMove* left, MoveOpVector* right);

  // Push down those moves in the gap of from that do not change the
  // semantics of the from instruction, nor the semantics of the moves
  // that remain behind.
  void MigrateMoves(Instruction* to, Instruction* from);

  void RemoveClobberedDestinations(Instruction* instruction);

  const Instruction* LastInstruction(const InstructionBlock* block) const;

  // Consolidate common moves appearing across all predecessors of a block.
  void OptimizeMerge(InstructionBlock* block);
  void FinalizeMoves(Instruction* instr);

  Zone* const local_zone_;
  InstructionSequence* const code_;
  MoveOpVector local_vector_;

  // Reusable buffers for storing operand sets. We need at most two sets
  // at any given time, so we create two buffers.
  ZoneVector<InstructionOperand> operand_buffer1;
  ZoneVector<InstructionOperand> operand_buffer2;

  DISALLOW_COPY_AND_ASSIGN(MoveOptimizer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MOVE_OPTIMIZER_

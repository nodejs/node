// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MOVE_OPTIMIZER_
#define V8_COMPILER_MOVE_OPTIMIZER_

#include "src/compiler/instruction.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class MoveOptimizer final {
 public:
  MoveOptimizer(Zone* local_zone, InstructionSequence* code);
  void Run();

 private:
  typedef ZoneVector<MoveOperands*> MoveOpVector;
  typedef ZoneVector<Instruction*> Instructions;

  InstructionSequence* code() const { return code_; }
  Zone* local_zone() const { return local_zone_; }
  Zone* code_zone() const { return code()->zone(); }
  MoveOpVector& temp_vector_0() { return temp_vector_0_; }
  MoveOpVector& temp_vector_1() { return temp_vector_1_; }

  void CompressBlock(InstructionBlock* blocke);
  void CompressMoves(MoveOpVector* eliminated, ParallelMove* left,
                     ParallelMove* right);
  Instruction* LastInstruction(InstructionBlock* block);
  void OptimizeMerge(InstructionBlock* block);
  void FinalizeMoves(Instruction* instr);

  Zone* const local_zone_;
  InstructionSequence* const code_;
  Instructions to_finalize_;
  MoveOpVector temp_vector_0_;
  MoveOpVector temp_vector_1_;

  DISALLOW_COPY_AND_ASSIGN(MoveOptimizer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MOVE_OPTIMIZER_

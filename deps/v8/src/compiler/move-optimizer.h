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

class MoveOptimizer FINAL {
 public:
  MoveOptimizer(Zone* local_zone, InstructionSequence* code);
  void Run();

 private:
  typedef ZoneVector<MoveOperands*> MoveOpVector;

  InstructionSequence* code() const { return code_; }
  Zone* local_zone() const { return local_zone_; }
  Zone* code_zone() const { return code()->zone(); }

  void CompressMoves(MoveOpVector* eliminated, ParallelMove* left,
                     ParallelMove* right);
  void FinalizeMoves(MoveOpVector* loads, MoveOpVector* new_moves,
                     GapInstruction* gap);

  Zone* const local_zone_;
  InstructionSequence* const code_;
  MoveOpVector temp_vector_0_;
  MoveOpVector temp_vector_1_;

  DISALLOW_COPY_AND_ASSIGN(MoveOptimizer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_MOVE_OPTIMIZER_

// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BACKEND_JUMP_THREADING_H_
#define V8_COMPILER_BACKEND_JUMP_THREADING_H_

#include "src/compiler/backend/instruction.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forwards jumps to empty basic blocks that end with a second jump to the
// destination of the second jump, transitively.
class V8_EXPORT_PRIVATE JumpThreading {
 public:
  // Compute the forwarding map of basic blocks to their ultimate destination.
  // Returns {true} if there is at least one block that is forwarded.
  static bool ComputeForwarding(Zone* local_zone, ZoneVector<RpoNumber>* result,
                                InstructionSequence* code, bool frame_at_start);

  // Rewrite the instructions to forward jumps and branches.
  // May also negate some branches.
  static void ApplyForwarding(Zone* local_zone,
                              ZoneVector<RpoNumber> const& forwarding,
                              InstructionSequence* code);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BACKEND_JUMP_THREADING_H_

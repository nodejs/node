// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_VREG_ALLOCATOR_H_
#define V8_MAGLEV_MAGLEV_VREG_ALLOCATOR_H_

#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

class ProcessingState;

class MaglevVregAllocationState {
 public:
  int AllocateVirtualRegister() { return next_virtual_register_++; }
  int num_allocated_registers() const { return next_virtual_register_; }

 private:
  int next_virtual_register_ = 0;
};

class MaglevVregAllocator {
 public:
  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {
    for (BasicBlock* block : *graph) {
      if (!block->has_phi()) continue;
      for (Phi* phi : *block->phis()) {
        phi->AllocateVregInPostProcess(&state_);
      }
    }
  }
  void PreProcessBasicBlock(BasicBlock* block) {}

#define DEF_PROCESS_NODE(NAME)                             \
  void Process(NAME* node, const ProcessingState& state) { \
    node->AllocateVreg(&state_);                           \
  }
  NODE_BASE_LIST(DEF_PROCESS_NODE)
#undef DEF_PROCESS_NODE

 private:
  MaglevVregAllocationState state_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_VREG_ALLOCATOR_H_

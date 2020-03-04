// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SCHEDULED_MACHINE_LOWERING_H_
#define V8_COMPILER_SCHEDULED_MACHINE_LOWERING_H_

#include "src/compiler/graph-assembler.h"
#include "src/compiler/memory-lowering.h"
#include "src/compiler/select-lowering.h"

namespace v8 {
namespace internal {
namespace compiler {

class NodeOriginTable;
class Schedule;
class SourcePositionTable;

// Performs machine lowering on an already scheduled graph.
class ScheduledMachineLowering final {
 public:
  ScheduledMachineLowering(JSGraph* js_graph, Schedule* schedule,
                           Zone* temp_zone,
                           SourcePositionTable* source_positions,
                           NodeOriginTable* node_origins,
                           PoisoningMitigationLevel poison_level);
  ~ScheduledMachineLowering() = default;

  void Run();

 private:
  bool LowerNode(Node* node);

  GraphAssembler* gasm() { return &graph_assembler_; }
  Schedule* schedule() { return schedule_; }

  Schedule* schedule_;
  GraphAssembler graph_assembler_;
  SelectLowering select_lowering_;
  MemoryLowering memory_lowering_;
  ZoneVector<Reducer*> reducers_;
  SourcePositionTable* source_positions_;
  NodeOriginTable* node_origins_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SCHEDULED_MACHINE_LOWERING_H_

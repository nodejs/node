// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_GRAPH_AND_BUILDERS_H_
#define V8_CCTEST_COMPILER_GRAPH_AND_BUILDERS_H_

#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

class GraphAndBuilders {
 public:
  explicit GraphAndBuilders(Zone* zone)
      : main_graph_(new (zone) Graph(zone)),
        main_common_(zone),
        main_machine_(zone, MachineType::PointerRepresentation(),
                      InstructionSelector::SupportedMachineOperatorFlags(),
                      InstructionSelector::AlignmentRequirements()),
        main_simplified_(zone) {}

  Graph* graph() const { return main_graph_; }
  Zone* zone() const { return graph()->zone(); }
  CommonOperatorBuilder* common() { return &main_common_; }
  MachineOperatorBuilder* machine() { return &main_machine_; }
  SimplifiedOperatorBuilder* simplified() { return &main_simplified_; }

 protected:
  // Prefixed with main_ to avoid naming conflicts.
  Graph* main_graph_;
  CommonOperatorBuilder main_common_;
  MachineOperatorBuilder main_machine_;
  SimplifiedOperatorBuilder main_simplified_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_GRAPH_AND_BUILDERS_H_

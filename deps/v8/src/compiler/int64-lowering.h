// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_INT64_REDUCER_H_
#define V8_COMPILER_INT64_REDUCER_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-marker.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class Int64Lowering {
 public:
  Int64Lowering(Graph* graph, MachineOperatorBuilder* machine,
                CommonOperatorBuilder* common, Zone* zone,
                Signature<MachineRepresentation>* signature);

  void LowerGraph();

 private:
  enum class State : uint8_t { kUnvisited, kOnStack, kInputsPushed, kVisited };

  struct Replacement {
    Node* low;
    Node* high;
  };

  Zone* zone() const { return zone_; }
  Graph* graph() const { return graph_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  CommonOperatorBuilder* common() const { return common_; }
  Signature<MachineRepresentation>* signature() const { return signature_; }

  void LowerNode(Node* node);
  bool DefaultLowering(Node* node);

  void ReplaceNode(Node* old, Node* new_low, Node* new_high);
  bool HasReplacementLow(Node* node);
  Node* GetReplacementLow(Node* node);
  bool HasReplacementHigh(Node* node);
  Node* GetReplacementHigh(Node* node);

  Zone* zone_;
  Graph* const graph_;
  MachineOperatorBuilder* machine_;
  CommonOperatorBuilder* common_;
  NodeMarker<State> state_;
  ZoneStack<Node*> stack_;
  Replacement* replacements_;
  Signature<MachineRepresentation>* signature_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_INT64_REDUCER_H_

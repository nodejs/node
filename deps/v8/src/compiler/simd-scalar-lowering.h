// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMD_SCALAR_LOWERING_H_
#define V8_COMPILER_SIMD_SCALAR_LOWERING_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-marker.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimdScalarLowering {
 public:
  SimdScalarLowering(JSGraph* jsgraph,
                     Signature<MachineRepresentation>* signature);

  void LowerGraph();

  int GetParameterCountAfterLowering();

 private:
  enum class State : uint8_t { kUnvisited, kOnStack, kVisited };

  enum class SimdType : uint8_t { kInt32, kFloat32, kSimd1x4 };

  static const int kMaxLanes = 4;
  static const int kLaneWidth = 16 / kMaxLanes;

  struct Replacement {
    Node* node[kMaxLanes];
    SimdType type;  // represents what input type is expected
  };

  struct NodeState {
    Node* node;
    int input_index;
  };

  Zone* zone() const { return jsgraph_->zone(); }
  Graph* graph() const { return jsgraph_->graph(); }
  MachineOperatorBuilder* machine() const { return jsgraph_->machine(); }
  CommonOperatorBuilder* common() const { return jsgraph_->common(); }
  Signature<MachineRepresentation>* signature() const { return signature_; }

  void LowerNode(Node* node);
  bool DefaultLowering(Node* node);

  void ReplaceNode(Node* old, Node** new_nodes);
  bool HasReplacement(size_t index, Node* node);
  Node** GetReplacements(Node* node);
  Node** GetReplacementsWithType(Node* node, SimdType type);
  SimdType ReplacementType(Node* node);
  void PreparePhiReplacement(Node* phi);
  void SetLoweredType(Node* node, Node* output);
  void GetIndexNodes(Node* index, Node** new_indices);
  void LowerLoadOp(MachineRepresentation rep, Node* node,
                   const Operator* load_op);
  void LowerStoreOp(MachineRepresentation rep, Node* node,
                    const Operator* store_op, SimdType rep_type);
  void LowerBinaryOp(Node* node, SimdType input_rep_type, const Operator* op,
                     bool invert_inputs = false);
  void LowerUnaryOp(Node* node, SimdType input_rep_type, const Operator* op);
  void LowerIntMinMax(Node* node, const Operator* op, bool is_max);
  void LowerConvertFromFloat(Node* node, bool is_signed);
  void LowerShiftOp(Node* node, const Operator* op);
  Node* BuildF64Trunc(Node* input);
  void LowerNotEqual(Node* node, SimdType input_rep_type, const Operator* op);

  JSGraph* const jsgraph_;
  NodeMarker<State> state_;
  ZoneDeque<NodeState> stack_;
  Replacement* replacements_;
  Signature<MachineRepresentation>* signature_;
  Node* placeholder_;
  int parameter_count_after_lowering_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMD_SCALAR_LOWERING_H_

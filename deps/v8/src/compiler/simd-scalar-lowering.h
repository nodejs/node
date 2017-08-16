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

  enum class SimdType : uint8_t {
    kFloat32x4,
    kInt32x4,
    kInt16x8,
    kInt8x16,
    kSimd1x4,
    kSimd1x8,
    kSimd1x16
  };

#if defined(V8_TARGET_BIG_ENDIAN)
  static constexpr int kLaneOffsets[16] = {15, 14, 13, 12, 11, 10, 9, 8,
                                           7,  6,  5,  4,  3,  2,  1, 0};
#else
  static constexpr int kLaneOffsets[16] = {0, 1, 2,  3,  4,  5,  6,  7,
                                           8, 9, 10, 11, 12, 13, 14, 15};
#endif
  struct Replacement {
    Node** node = nullptr;
    SimdType type;  // represents output type
    int num_replacements = 0;
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

  int NumLanes(SimdType type);
  void ReplaceNode(Node* old, Node** new_nodes, int count);
  bool HasReplacement(size_t index, Node* node);
  Node** GetReplacements(Node* node);
  int ReplacementCount(Node* node);
  void Float32ToInt32(Node** replacements, Node** result);
  void Int32ToFloat32(Node** replacements, Node** result);
  Node** GetReplacementsWithType(Node* node, SimdType type);
  SimdType ReplacementType(Node* node);
  void PreparePhiReplacement(Node* phi);
  void SetLoweredType(Node* node, Node* output);
  void GetIndexNodes(Node* index, Node** new_indices, SimdType type);
  void LowerLoadOp(MachineRepresentation rep, Node* node,
                   const Operator* load_op, SimdType type);
  void LowerStoreOp(MachineRepresentation rep, Node* node,
                    const Operator* store_op, SimdType rep_type);
  void LowerBinaryOp(Node* node, SimdType input_rep_type, const Operator* op,
                     bool invert_inputs = false);
  Node* FixUpperBits(Node* input, int32_t shift);
  void LowerBinaryOpForSmallInt(Node* node, SimdType input_rep_type,
                                const Operator* op);
  Node* Mask(Node* input, int32_t mask);
  void LowerSaturateBinaryOp(Node* node, SimdType input_rep_type,
                             const Operator* op, bool is_signed);
  void LowerUnaryOp(Node* node, SimdType input_rep_type, const Operator* op);
  void LowerIntMinMax(Node* node, const Operator* op, bool is_max,
                      SimdType type);
  void LowerConvertFromFloat(Node* node, bool is_signed);
  void LowerShiftOp(Node* node, SimdType type);
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

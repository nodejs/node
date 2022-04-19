// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_LOWERING_VERIFIER_H_
#define V8_COMPILER_SIMPLIFIED_LOWERING_VERIFIER_H_

#include "src/compiler/representation-change.h"

namespace v8 {
namespace internal {
namespace compiler {

class OperationTyper;

class SimplifiedLoweringVerifier final {
 public:
  struct PerNodeData {
    base::Optional<Type> type = base::nullopt;
    Truncation truncation = Truncation::Any(IdentifyZeros::kDistinguishZeros);
  };

  SimplifiedLoweringVerifier(Zone* zone, Graph* graph)
      : hints_(zone), data_(zone), graph_(graph) {}

  void VisitNode(Node* node, OperationTyper& op_typer);

  void RecordHint(Node* node) {
    DCHECK_EQ(node->opcode(), IrOpcode::kSLVerifierHint);
    hints_.push_back(node);
  }
  const ZoneVector<Node*>& inserted_hints() const { return hints_; }

  base::Optional<Type> GetType(Node* node) const {
    if (NodeProperties::IsTyped(node)) {
      return NodeProperties::GetType(node);
    }
    // For nodes that have not been typed before SL, we use the type that has
    // been inferred by the verifier.
    if (node->id() < data_.size()) {
      return data_[node->id()].type;
    }
    return base::nullopt;
  }

 private:
  void ResizeDataIfNecessary(Node* node) {
    if (data_.size() <= node->id()) {
      data_.resize(node->id() + 1);
    }
    DCHECK_EQ(data_[node->id()].truncation,
              Truncation::Any(IdentifyZeros::kDistinguishZeros));
  }

  void SetType(Node* node, const Type& type) {
    ResizeDataIfNecessary(node);
    data_[node->id()].type = type;
  }

  Type InputType(Node* node, int input_index) const {
    // TODO(nicohartmann): Check that inputs are typed, once all operators are
    // supported.
    Node* input = node->InputAt(input_index);
    if (NodeProperties::IsTyped(input)) {
      return NodeProperties::GetType(input);
    }
    // For nodes that have not been typed before SL, we use the type that has
    // been inferred by the verifier.
    base::Optional<Type> type_opt;
    if (input->id() < data_.size()) {
      type_opt = data_[input->id()].type;
    }
    return type_opt.has_value() ? *type_opt : Type::None();
  }

  void SetTruncation(Node* node, const Truncation& truncation) {
    ResizeDataIfNecessary(node);
    data_[node->id()].truncation = truncation;
  }

  Truncation InputTruncation(Node* node, int input_index) const {
    static const Truncation any_truncation =
        Truncation::Any(IdentifyZeros::kDistinguishZeros);

    Node* input = node->InputAt(input_index);
    if (input->id() < data_.size()) {
      return data_[input->id()].truncation;
    }
    return any_truncation;
  }

  void CheckType(Node* node, const Type& type);
  void CheckAndSet(Node* node, const Type& type, const Truncation& trunc);

  // Generalize to a less strict truncation in the context of a given type. For
  // example, a Truncation::kWord32[kIdentifyZeros] does not have any effect on
  // a type Range(0, 100), because all equivalence classes are singleton, for
  // the values of the given type. We can use Truncation::Any[kDistinguishZeros]
  // instead to avoid a combinatorial explosion of occurring type-truncation-
  // pairs.
  Truncation GeneralizeTruncation(const Truncation& truncation,
                                  const Type& type) const;

  Zone* graph_zone() const { return graph_->zone(); }

  ZoneVector<Node*> hints_;
  ZoneVector<PerNodeData> data_;
  Graph* graph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_LOWERING_VERIFIER_H_

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
    Truncation truncation = Truncation::Any(IdentifyZeros::kDistinguishZeros);
  };

  SimplifiedLoweringVerifier(Zone* zone, Graph* graph)
      : type_guards_(zone), data_(zone), graph_(graph) {}

  void VisitNode(Node* node, OperationTyper& op_typer);

  void RecordTypeGuard(Node* node) {
    DCHECK_EQ(node->opcode(), IrOpcode::kTypeGuard);
    DCHECK(!is_recorded_type_guard(node));
    type_guards_.insert(node);
  }
  const ZoneUnorderedSet<Node*>& recorded_type_guards() const {
    return type_guards_;
  }

 private:
  bool is_recorded_type_guard(Node* node) const {
    return type_guards_.find(node) != type_guards_.end();
  }

  Type InputType(Node* node, int input_index) const {
    // TODO(nicohartmann): Check that inputs are typed, once all operators are
    // supported.
    Node* input = node->InputAt(input_index);
    if (NodeProperties::IsTyped(input)) {
      return NodeProperties::GetType(input);
    }
    return Type::None();
  }

  void SetTruncation(Node* node, const Truncation& truncation) {
    if (data_.size() <= node->id()) {
      data_.resize(node->id() + 1);
    }
    DCHECK_EQ(data_[node->id()].truncation,
              Truncation::Any(IdentifyZeros::kDistinguishZeros));
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

  ZoneUnorderedSet<Node*> type_guards_;
  ZoneVector<PerNodeData> data_;
  Graph* graph_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_LOWERING_VERIFIER_H_

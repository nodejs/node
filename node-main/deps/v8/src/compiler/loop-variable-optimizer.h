// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOOP_VARIABLE_OPTIMIZER_H_
#define V8_COMPILER_LOOP_VARIABLE_OPTIMIZER_H_

#include "src/compiler/functional-list.h"
#include "src/compiler/node-aux-data.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class CommonOperatorBuilder;
class TFGraph;
class Node;

class InductionVariable : public ZoneObject {
 public:
  Node* phi() const { return phi_; }
  Node* effect_phi() const { return effect_phi_; }
  Node* arith() const { return arith_; }
  Node* increment() const { return increment_; }
  Node* init_value() const { return init_value_; }

  enum ConstraintKind { kStrict, kNonStrict };
  enum ArithmeticType { kAddition, kSubtraction };
  struct Bound {
    Bound(Node* bound, ConstraintKind kind) : bound(bound), kind(kind) {}

    Node* bound;
    ConstraintKind kind;
  };

  const ZoneVector<Bound>& lower_bounds() { return lower_bounds_; }
  const ZoneVector<Bound>& upper_bounds() { return upper_bounds_; }

  ArithmeticType Type() { return arithmeticType_; }

 private:
  friend class LoopVariableOptimizer;
  friend Zone;

  InductionVariable(Node* phi, Node* effect_phi, Node* arith, Node* increment,
                    Node* init_value, Zone* zone, ArithmeticType arithmeticType)
      : phi_(phi),
        effect_phi_(effect_phi),
        arith_(arith),
        increment_(increment),
        init_value_(init_value),
        lower_bounds_(zone),
        upper_bounds_(zone),
        arithmeticType_(arithmeticType) {}

  void AddUpperBound(Node* bound, ConstraintKind kind);
  void AddLowerBound(Node* bound, ConstraintKind kind);

  Node* phi_;
  Node* effect_phi_;
  Node* arith_;
  Node* increment_;
  Node* init_value_;
  ZoneVector<Bound> lower_bounds_;
  ZoneVector<Bound> upper_bounds_;
  ArithmeticType arithmeticType_;
};

class LoopVariableOptimizer {
 public:
  void Run();

  LoopVariableOptimizer(TFGraph* graph, CommonOperatorBuilder* common,
                        Zone* zone);

  const ZoneMap<int, InductionVariable*>& induction_variables() {
    return induction_vars_;
  }

  void ChangeToInductionVariablePhis();
  void ChangeToPhisAndInsertGuards();

 private:
  const int kAssumedLoopEntryIndex = 0;
  const int kFirstBackedge = 1;

  struct Constraint {
    Node* left;
    InductionVariable::ConstraintKind kind;
    Node* right;

    bool operator!=(const Constraint& other) const {
      return left != other.left || kind != other.kind || right != other.right;
    }
  };

  using VariableLimits = FunctionalList<Constraint>;

  void VisitBackedge(Node* from, Node* loop);
  void VisitNode(Node* node);
  void VisitMerge(Node* node);
  void VisitLoop(Node* node);
  void VisitIf(Node* node, bool polarity);
  void VisitStart(Node* node);
  void VisitLoopExit(Node* node);
  void VisitOtherControl(Node* node);

  void AddCmpToLimits(VariableLimits* limits, Node* node,
                      InductionVariable::ConstraintKind kind, bool polarity);

  void TakeConditionsFromFirstControl(Node* node);
  const InductionVariable* FindInductionVariable(Node* node);
  InductionVariable* TryGetInductionVariable(Node* phi);
  void DetectInductionVariables(Node* loop);

  TFGraph* graph() { return graph_; }
  CommonOperatorBuilder* common() { return common_; }
  Zone* zone() { return zone_; }

  TFGraph* graph_;
  CommonOperatorBuilder* common_;
  Zone* zone_;
  NodeAuxData<VariableLimits> limits_;
  NodeAuxData<bool> reduced_;

  ZoneMap<int, InductionVariable*> induction_vars_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOOP_VARIABLE_OPTIMIZER_H_

// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REDUNDANCY_ELIMINATION_H_
#define V8_COMPILER_REDUNDANCY_ELIMINATION_H_

#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

class V8_EXPORT_PRIVATE RedundancyElimination final : public AdvancedReducer {
 public:
  RedundancyElimination(Editor* editor, Zone* zone);
  ~RedundancyElimination() final;
  RedundancyElimination(const RedundancyElimination&) = delete;
  RedundancyElimination& operator=(const RedundancyElimination&) = delete;

  const char* reducer_name() const override { return "RedundancyElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  struct Check {
    Check(Node* node, Check* next) : node(node), next(next) {}
    Node* node;
    Check* next;
  };

  class EffectPathChecks final {
   public:
    static EffectPathChecks* Copy(Zone* zone, EffectPathChecks const* checks);
    static EffectPathChecks const* Empty(Zone* zone);
    bool Equals(EffectPathChecks const* that) const;
    void Merge(EffectPathChecks const* that);

    EffectPathChecks const* AddCheck(Zone* zone, Node* node) const;
    Node* LookupCheck(Node* node) const;
    Node* LookupBoundsCheckFor(Node* node) const;

   private:
    friend Zone;

    EffectPathChecks(Check* head, size_t size) : head_(head), size_(size) {}

    // We keep track of the list length so that we can find the longest
    // common tail easily.
    Check* head_;
    size_t size_;
  };

  class PathChecksForEffectNodes final {
   public:
    explicit PathChecksForEffectNodes(Zone* zone) : info_for_node_(zone) {}
    EffectPathChecks const* Get(Node* node) const;
    void Set(Node* node, EffectPathChecks const* checks);

   private:
    ZoneVector<EffectPathChecks const*> info_for_node_;
  };

  Reduction ReduceCheckNode(Node* node);
  Reduction ReduceEffectPhi(Node* node);
  Reduction ReduceSpeculativeNumberComparison(Node* node);
  Reduction ReduceSpeculativeNumberOperation(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceOtherNode(Node* node);

  Reduction TakeChecksFromFirstEffect(Node* node);
  Reduction UpdateChecks(Node* node, EffectPathChecks const* checks);

  Zone* zone() const { return zone_; }

  PathChecksForEffectNodes node_checks_;
  Zone* const zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REDUNDANCY_ELIMINATION_H_

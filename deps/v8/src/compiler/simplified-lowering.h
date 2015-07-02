// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_LOWERING_H_
#define V8_COMPILER_SIMPLIFIED_LOWERING_H_

#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class RepresentationChanger;
class SourcePositionTable;

class SimplifiedLowering final {
 public:
  SimplifiedLowering(JSGraph* jsgraph, Zone* zone,
                     SourcePositionTable* source_positions)
      : jsgraph_(jsgraph), zone_(zone), source_positions_(source_positions) {}
  ~SimplifiedLowering() {}

  void LowerAllNodes();

  // TODO(titzer): These are exposed for direct testing. Use a friend class.
  void DoAllocate(Node* node);
  void DoLoadField(Node* node);
  void DoStoreField(Node* node);
  // TODO(turbofan): The output_type can be removed once the result of the
  // representation analysis is stored in the node bounds.
  void DoLoadBuffer(Node* node, MachineType output_type,
                    RepresentationChanger* changer);
  void DoStoreBuffer(Node* node);
  void DoLoadElement(Node* node);
  void DoStoreElement(Node* node);
  void DoStringEqual(Node* node);
  void DoStringLessThan(Node* node);
  void DoStringLessThanOrEqual(Node* node);

 private:
  JSGraph* const jsgraph_;
  Zone* const zone_;

  // TODO(danno): SimplifiedLowering shouldn't know anything about the source
  // positions table, but must for now since there currently is no other way to
  // pass down source position information to nodes created during
  // lowering. Once this phase becomes a vanilla reducer, it should get source
  // position information via the SourcePositionWrapper like all other reducers.
  SourcePositionTable* source_positions_;

  Node* SmiTag(Node* node);
  Node* IsTagged(Node* node);
  Node* Untag(Node* node);
  Node* OffsetMinusTagConstant(int32_t offset);
  Node* ComputeIndex(const ElementAccess& access, Node* const key);
  Node* StringComparison(Node* node, bool requires_ordering);
  Node* Int32Div(Node* const node);
  Node* Int32Mod(Node* const node);
  Node* Uint32Div(Node* const node);
  Node* Uint32Mod(Node* const node);

  friend class RepresentationSelector;

  Zone* zone() { return jsgraph_->zone(); }
  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph()->graph(); }
  CommonOperatorBuilder* common() { return jsgraph()->common(); }
  MachineOperatorBuilder* machine() { return jsgraph()->machine(); }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_LOWERING_H_

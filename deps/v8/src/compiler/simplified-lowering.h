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

class SimplifiedLowering FINAL {
 public:
  explicit SimplifiedLowering(JSGraph* jsgraph) : jsgraph_(jsgraph) {}
  ~SimplifiedLowering() {}

  void LowerAllNodes();

  // TODO(titzer): These are exposed for direct testing. Use a friend class.
  void DoLoadField(Node* node);
  void DoStoreField(Node* node);
  // TODO(turbofan): The output_type can be removed once the result of the
  // representation analysis is stored in the node bounds.
  void DoLoadElement(Node* node, MachineType output_type);
  void DoStoreElement(Node* node);
  void DoStringAdd(Node* node);
  void DoStringEqual(Node* node);
  void DoStringLessThan(Node* node);
  void DoStringLessThanOrEqual(Node* node);

 private:
  JSGraph* jsgraph_;

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

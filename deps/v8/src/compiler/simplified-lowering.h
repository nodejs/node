// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SIMPLIFIED_LOWERING_H_
#define V8_COMPILER_SIMPLIFIED_LOWERING_H_

#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/lowering-builder.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

class SimplifiedLowering : public LoweringBuilder {
 public:
  explicit SimplifiedLowering(JSGraph* jsgraph,
                              SourcePositionTable* source_positions)
      : LoweringBuilder(jsgraph->graph(), source_positions),
        jsgraph_(jsgraph),
        machine_(jsgraph->zone()) {}
  virtual ~SimplifiedLowering() {}

  void LowerAllNodes();

  virtual void Lower(Node* node);
  void LowerChange(Node* node, Node* effect, Node* control);

  // TODO(titzer): These are exposed for direct testing. Use a friend class.
  void DoLoadField(Node* node);
  void DoStoreField(Node* node);
  void DoLoadElement(Node* node);
  void DoStoreElement(Node* node);

 private:
  JSGraph* jsgraph_;
  MachineOperatorBuilder machine_;

  Node* SmiTag(Node* node);
  Node* IsTagged(Node* node);
  Node* Untag(Node* node);
  Node* OffsetMinusTagConstant(int32_t offset);
  Node* ComputeIndex(const ElementAccess& access, Node* index);

  void DoChangeTaggedToUI32(Node* node, Node* effect, Node* control,
                            bool is_signed);
  void DoChangeUI32ToTagged(Node* node, Node* effect, Node* control,
                            bool is_signed);
  void DoChangeTaggedToFloat64(Node* node, Node* effect, Node* control);
  void DoChangeFloat64ToTagged(Node* node, Node* effect, Node* control);
  void DoChangeBoolToBit(Node* node, Node* effect, Node* control);
  void DoChangeBitToBool(Node* node, Node* effect, Node* control);

  friend class RepresentationSelector;

  Zone* zone() { return jsgraph_->zone(); }
  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph()->graph(); }
  CommonOperatorBuilder* common() { return jsgraph()->common(); }
  MachineOperatorBuilder* machine() { return &machine_; }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SIMPLIFIED_LOWERING_H_

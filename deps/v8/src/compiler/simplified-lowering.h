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

// Forward declarations.
class TypeCache;


namespace compiler {

// Forward declarations.
class RepresentationChanger;
class SourcePositionTable;

class SimplifiedLowering final {
 public:
  SimplifiedLowering(JSGraph* jsgraph, Zone* zone,
                     SourcePositionTable* source_positions);
  ~SimplifiedLowering() {}

  void LowerAllNodes();

  // TODO(turbofan): The representation can be removed once the result of the
  // representation analysis is stored in the node bounds.
  void DoLoadBuffer(Node* node, MachineRepresentation rep,
                    RepresentationChanger* changer);
  void DoStoreBuffer(Node* node);
  void DoObjectIsNumber(Node* node);
  void DoObjectIsSmi(Node* node);
  void DoShift(Node* node, Operator const* op, Type* rhs_type);
  void DoStringEqual(Node* node);
  void DoStringLessThan(Node* node);
  void DoStringLessThanOrEqual(Node* node);

  // TODO(bmeurer): This is a gigantic hack to support the gigantic LoadBuffer
  // typing hack to support the gigantic "asm.js should be fast without proper
  // verifier"-hack, ... Kill this! Soon! Really soon! I'm serious!
  bool abort_compilation_ = false;

 private:
  JSGraph* const jsgraph_;
  Zone* const zone_;
  TypeCache const& type_cache_;

  // TODO(danno): SimplifiedLowering shouldn't know anything about the source
  // positions table, but must for now since there currently is no other way to
  // pass down source position information to nodes created during
  // lowering. Once this phase becomes a vanilla reducer, it should get source
  // position information via the SourcePositionWrapper like all other reducers.
  SourcePositionTable* source_positions_;

  Node* StringComparison(Node* node);
  Node* Int32Div(Node* const node);
  Node* Int32Mod(Node* const node);
  Node* Uint32Div(Node* const node);
  Node* Uint32Mod(Node* const node);

  friend class RepresentationSelector;

  Isolate* isolate() { return jsgraph_->isolate(); }
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

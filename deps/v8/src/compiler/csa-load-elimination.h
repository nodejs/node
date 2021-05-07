// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CSA_LOAD_ELIMINATION_H_
#define V8_COMPILER_CSA_LOAD_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/persistent-map.h"
#include "src/handles/maybe-handles.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
struct ObjectAccess;
class Graph;
class JSGraph;

class V8_EXPORT_PRIVATE CsaLoadElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  CsaLoadElimination(Editor* editor, JSGraph* jsgraph, Zone* zone)
      : AdvancedReducer(editor),
        empty_state_(zone),
        node_states_(jsgraph->graph()->NodeCount(), zone),
        jsgraph_(jsgraph),
        zone_(zone) {}
  ~CsaLoadElimination() final = default;
  CsaLoadElimination(const CsaLoadElimination&) = delete;
  CsaLoadElimination& operator=(const CsaLoadElimination&) = delete;

  const char* reducer_name() const override { return "CsaLoadElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  struct FieldInfo {
    FieldInfo() = default;
    FieldInfo(Node* value, MachineRepresentation representation)
        : value(value), representation(representation) {}

    bool operator==(const FieldInfo& other) const {
      return value == other.value && representation == other.representation;
    }

    bool operator!=(const FieldInfo& other) const { return !(*this == other); }

    bool IsEmpty() const { return value == nullptr; }

    Node* value = nullptr;
    MachineRepresentation representation = MachineRepresentation::kNone;
  };

  class AbstractState final : public ZoneObject {
   public:
    explicit AbstractState(Zone* zone) : field_infos_(zone) {}

    bool Equals(AbstractState const* that) const {
      return field_infos_ == that->field_infos_;
    }
    void Merge(AbstractState const* that, Zone* zone);

    AbstractState const* KillField(Node* object, Node* offset,
                                   MachineRepresentation repr,
                                   Zone* zone) const;
    AbstractState const* AddField(Node* object, Node* offset, FieldInfo info,
                                  Zone* zone) const;
    FieldInfo Lookup(Node* object, Node* offset) const;

    void Print() const;

   private:
    using Field = std::pair<Node*, Node*>;
    using FieldInfos = PersistentMap<Field, FieldInfo>;
    FieldInfos field_infos_;
  };

  Reduction ReduceLoadFromObject(Node* node, ObjectAccess const& access);
  Reduction ReduceStoreToObject(Node* node, ObjectAccess const& access);
  Reduction ReduceEffectPhi(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceCall(Node* node);
  Reduction ReduceOtherNode(Node* node);

  Reduction UpdateState(Node* node, AbstractState const* state);
  Reduction PropagateInputState(Node* node);

  AbstractState const* ComputeLoopState(Node* node,
                                        AbstractState const* state) const;

  CommonOperatorBuilder* common() const;
  Isolate* isolate() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Zone* zone() const { return zone_; }
  AbstractState const* empty_state() const { return &empty_state_; }

  AbstractState const empty_state_;
  NodeAuxData<AbstractState const*> node_states_;
  JSGraph* const jsgraph_;
  Zone* zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CSA_LOAD_ELIMINATION_H_

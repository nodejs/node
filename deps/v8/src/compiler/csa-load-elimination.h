// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CSA_LOAD_ELIMINATION_H_
#define V8_COMPILER_CSA_LOAD_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/codegen/machine-type.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/persistent-map.h"

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

  // Design doc: https://bit.ly/36MfD6Y
  class HalfState final : public ZoneObject {
   public:
    explicit HalfState(Zone* zone)
        : zone_(zone),
          fresh_entries_(zone, InnerMap(zone)),
          constant_entries_(zone, InnerMap(zone)),
          arbitrary_entries_(zone, InnerMap(zone)),
          fresh_unknown_entries_(zone, InnerMap(zone)),
          constant_unknown_entries_(zone, InnerMap(zone)),
          arbitrary_unknown_entries_(zone, InnerMap(zone)) {}

    bool Equals(HalfState const* that) const {
      return fresh_entries_ == that->fresh_entries_ &&
             constant_entries_ == that->constant_entries_ &&
             arbitrary_entries_ == that->arbitrary_entries_ &&
             fresh_unknown_entries_ == that->fresh_unknown_entries_ &&
             constant_unknown_entries_ == that->constant_unknown_entries_ &&
             arbitrary_unknown_entries_ == that->arbitrary_unknown_entries_;
    }
    void IntersectWith(HalfState const* that);
    HalfState const* KillField(Node* object, Node* offset,
                               MachineRepresentation repr) const;
    HalfState const* AddField(Node* object, Node* offset, Node* value,
                              MachineRepresentation repr) const;
    FieldInfo Lookup(Node* object, Node* offset) const;
    void Print() const;

   private:
    using InnerMap = PersistentMap<Node*, FieldInfo>;
    template <typename OuterKey>
    using OuterMap = PersistentMap<OuterKey, InnerMap>;
    // offset -> object -> info
    using ConstantOffsetInfos = OuterMap<uint32_t>;
    // object -> offset -> info
    using UnknownOffsetInfos = OuterMap<Node*>;

    // Update {map} so that {map.Get(outer_key).Get(inner_key)} returns {info}.
    template <typename OuterKey>
    static void Update(OuterMap<OuterKey>& map, OuterKey outer_key,
                       Node* inner_key, FieldInfo info) {
      InnerMap map_copy(map.Get(outer_key));
      map_copy.Set(inner_key, info);
      map.Set(outer_key, map_copy);
    }

    // Kill all elements in {infos} which may alias with offset.
    static void KillOffset(ConstantOffsetInfos& infos, uint32_t offset,
                           MachineRepresentation repr, Zone* zone);
    void KillOffsetInFresh(Node* object, uint32_t offset,
                           MachineRepresentation repr);
    template <typename OuterKey>
    static void IntersectWith(OuterMap<OuterKey>& to,
                              const OuterMap<OuterKey>& from);
    static void Print(const ConstantOffsetInfos& infos);
    static void Print(const UnknownOffsetInfos& infos);

    Zone* zone_;
    ConstantOffsetInfos fresh_entries_;
    ConstantOffsetInfos constant_entries_;
    ConstantOffsetInfos arbitrary_entries_;
    UnknownOffsetInfos fresh_unknown_entries_;
    UnknownOffsetInfos constant_unknown_entries_;
    UnknownOffsetInfos arbitrary_unknown_entries_;
  };

  // An {AbstractState} consists of two {HalfState}s, representing the mutable
  // and immutable sets of known fields, respectively. These sets correspond to
  // LoadFromObject/StoreToObject and LoadImmutableFromObject/
  // InitializeImmutableInObject respectively. The two half-states should not
  // overlap.
  struct AbstractState : public ZoneObject {
    explicit AbstractState(Zone* zone)
        : mutable_state(zone), immutable_state(zone) {}
    explicit AbstractState(HalfState mutable_state, HalfState immutable_state)
        : mutable_state(mutable_state), immutable_state(immutable_state) {}

    bool Equals(AbstractState const* that) const {
      return this->immutable_state.Equals(&that->immutable_state) &&
             this->mutable_state.Equals(&that->mutable_state);
    }
    void IntersectWith(AbstractState const* that) {
      mutable_state.IntersectWith(&that->mutable_state);
      immutable_state.IntersectWith(&that->immutable_state);
    }

    HalfState mutable_state;
    HalfState immutable_state;
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
  Node* TruncateAndExtend(Node* node, MachineRepresentation from,
                          MachineType to);
  Reduction AssertUnreachable(Node* node);

  CommonOperatorBuilder* common() const;
  MachineOperatorBuilder* machine() const;
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

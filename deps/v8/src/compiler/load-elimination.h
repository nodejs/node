// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOAD_ELIMINATION_H_
#define V8_COMPILER_LOAD_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/codegen/machine-type.h"
#include "src/common/globals.h"
#include "src/compiler/graph-reducer.h"
#include "src/handles/maybe-handles.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
struct FieldAccess;
class Graph;
class JSGraph;

class V8_EXPORT_PRIVATE LoadElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  LoadElimination(Editor* editor, JSGraph* jsgraph, Zone* zone)
      : AdvancedReducer(editor), node_states_(zone), jsgraph_(jsgraph) {}
  ~LoadElimination() final = default;

  const char* reducer_name() const override { return "LoadElimination"; }

  Reduction Reduce(Node* node) final;

 private:
  static const size_t kMaxTrackedElements = 8;

  // Abstract state to approximate the current state of an element along the
  // effect paths through the graph.
  class AbstractElements final : public ZoneObject {
   public:
    explicit AbstractElements(Zone* zone) {
      for (size_t i = 0; i < arraysize(elements_); ++i) {
        elements_[i] = Element();
      }
    }
    AbstractElements(Node* object, Node* index, Node* value,
                     MachineRepresentation representation, Zone* zone)
        : AbstractElements(zone) {
      elements_[next_index_++] = Element(object, index, value, representation);
    }

    AbstractElements const* Extend(Node* object, Node* index, Node* value,
                                   MachineRepresentation representation,
                                   Zone* zone) const {
      AbstractElements* that = new (zone) AbstractElements(*this);
      that->elements_[that->next_index_] =
          Element(object, index, value, representation);
      that->next_index_ = (that->next_index_ + 1) % arraysize(elements_);
      return that;
    }
    Node* Lookup(Node* object, Node* index,
                 MachineRepresentation representation) const;
    AbstractElements const* Kill(Node* object, Node* index, Zone* zone) const;
    bool Equals(AbstractElements const* that) const;
    AbstractElements const* Merge(AbstractElements const* that,
                                  Zone* zone) const;

    void Print() const;

   private:
    struct Element {
      Element() = default;
      Element(Node* object, Node* index, Node* value,
              MachineRepresentation representation)
          : object(object),
            index(index),
            value(value),
            representation(representation) {}

      Node* object = nullptr;
      Node* index = nullptr;
      Node* value = nullptr;
      MachineRepresentation representation = MachineRepresentation::kNone;
    };

    Element elements_[kMaxTrackedElements];
    size_t next_index_ = 0;
  };

  // Information we use to resolve object aliasing. Currently, we consider
  // object not aliased if they have different maps or if the nodes may
  // not alias.
  class AliasStateInfo;

  struct FieldInfo {
    FieldInfo() = default;
    FieldInfo(Node* value, MachineRepresentation representation)
        : value(value), name(), representation(representation) {}
    FieldInfo(Node* value, MaybeHandle<Name> name,
              MachineRepresentation representation)
        : value(value), name(name), representation(representation) {}

    bool operator==(const FieldInfo& other) const {
      return value == other.value && name.address() == other.name.address() &&
             representation == other.representation;
    }

    Node* value = nullptr;
    MaybeHandle<Name> name;
    MachineRepresentation representation = MachineRepresentation::kNone;
  };

  // Abstract state to approximate the current state of a certain field along
  // the effect paths through the graph.
  class AbstractField final : public ZoneObject {
   public:
    explicit AbstractField(Zone* zone) : info_for_node_(zone) {}
    AbstractField(Node* object, FieldInfo info, Zone* zone)
        : info_for_node_(zone) {
      info_for_node_.insert(std::make_pair(object, info));
    }

    AbstractField const* Extend(Node* object, FieldInfo info,
                                Zone* zone) const {
      AbstractField* that = new (zone) AbstractField(zone);
      that->info_for_node_ = this->info_for_node_;
      that->info_for_node_[object] = info;
      return that;
    }
    FieldInfo const* Lookup(Node* object) const;
    AbstractField const* Kill(const AliasStateInfo& alias_info,
                              MaybeHandle<Name> name, Zone* zone) const;
    bool Equals(AbstractField const* that) const {
      return this == that || this->info_for_node_ == that->info_for_node_;
    }
    AbstractField const* Merge(AbstractField const* that, Zone* zone) const {
      if (this->Equals(that)) return this;
      AbstractField* copy = new (zone) AbstractField(zone);
      for (auto this_it : this->info_for_node_) {
        Node* this_object = this_it.first;
        FieldInfo this_second = this_it.second;
        if (this_object->IsDead()) continue;
        auto that_it = that->info_for_node_.find(this_object);
        if (that_it != that->info_for_node_.end() &&
            that_it->second == this_second) {
          copy->info_for_node_.insert(this_it);
        }
      }
      return copy;
    }

    void Print() const;

   private:
    ZoneMap<Node*, FieldInfo> info_for_node_;
  };

  static size_t const kMaxTrackedFields = 32;

  // Abstract state to approximate the current map of an object along the
  // effect paths through the graph.
  class AbstractMaps final : public ZoneObject {
   public:
    explicit AbstractMaps(Zone* zone);
    AbstractMaps(Node* object, ZoneHandleSet<Map> maps, Zone* zone);

    AbstractMaps const* Extend(Node* object, ZoneHandleSet<Map> maps,
                               Zone* zone) const;
    bool Lookup(Node* object, ZoneHandleSet<Map>* object_maps) const;
    AbstractMaps const* Kill(const AliasStateInfo& alias_info,
                             Zone* zone) const;
    bool Equals(AbstractMaps const* that) const {
      return this == that || this->info_for_node_ == that->info_for_node_;
    }
    AbstractMaps const* Merge(AbstractMaps const* that, Zone* zone) const;

    void Print() const;

   private:
    ZoneMap<Node*, ZoneHandleSet<Map>> info_for_node_;
  };

  class AbstractState final : public ZoneObject {
   public:
    AbstractState() {}

    bool Equals(AbstractState const* that) const;
    void Merge(AbstractState const* that, Zone* zone);

    AbstractState const* SetMaps(Node* object, ZoneHandleSet<Map> maps,
                                 Zone* zone) const;
    AbstractState const* KillMaps(Node* object, Zone* zone) const;
    AbstractState const* KillMaps(const AliasStateInfo& alias_info,
                                  Zone* zone) const;
    bool LookupMaps(Node* object, ZoneHandleSet<Map>* object_maps) const;

    AbstractState const* AddField(Node* object, size_t index, FieldInfo info,
                                  PropertyConstness constness,
                                  Zone* zone) const;
    AbstractState const* KillField(const AliasStateInfo& alias_info,
                                   size_t index, MaybeHandle<Name> name,
                                   Zone* zone) const;
    AbstractState const* KillField(Node* object, size_t index,
                                   MaybeHandle<Name> name, Zone* zone) const;
    AbstractState const* KillFields(Node* object, MaybeHandle<Name> name,
                                    Zone* zone) const;
    AbstractState const* KillAll(Zone* zone) const;
    FieldInfo const* LookupField(Node* object, size_t index,
                                 PropertyConstness constness) const;

    AbstractState const* AddElement(Node* object, Node* index, Node* value,
                                    MachineRepresentation representation,
                                    Zone* zone) const;
    AbstractState const* KillElement(Node* object, Node* index,
                                     Zone* zone) const;
    Node* LookupElement(Node* object, Node* index,
                        MachineRepresentation representation) const;

    void Print() const;

    static AbstractState const* empty_state() { return &empty_state_; }

   private:
    static AbstractState const empty_state_;

    using AbstractFields = std::array<AbstractField const*, kMaxTrackedFields>;

    bool FieldsEquals(AbstractFields const& this_fields,
                      AbstractFields const& that_fields) const;
    void FieldsMerge(AbstractFields* this_fields,
                     AbstractFields const& that_fields, Zone* zone);

    AbstractElements const* elements_ = nullptr;
    AbstractFields fields_{};
    AbstractFields const_fields_{};
    AbstractMaps const* maps_ = nullptr;
  };

  class AbstractStateForEffectNodes final : public ZoneObject {
   public:
    explicit AbstractStateForEffectNodes(Zone* zone) : info_for_node_(zone) {}
    AbstractState const* Get(Node* node) const;
    void Set(Node* node, AbstractState const* state);

    Zone* zone() const { return info_for_node_.get_allocator().zone(); }

   private:
    ZoneVector<AbstractState const*> info_for_node_;
  };

  Reduction ReduceCheckMaps(Node* node);
  Reduction ReduceCompareMaps(Node* node);
  Reduction ReduceMapGuard(Node* node);
  Reduction ReduceEnsureWritableFastElements(Node* node);
  Reduction ReduceMaybeGrowFastElements(Node* node);
  Reduction ReduceTransitionElementsKind(Node* node);
  Reduction ReduceLoadField(Node* node, FieldAccess const& access);
  Reduction ReduceStoreField(Node* node, FieldAccess const& access);
  Reduction ReduceLoadElement(Node* node);
  Reduction ReduceStoreElement(Node* node);
  Reduction ReduceTransitionAndStoreElement(Node* node);
  Reduction ReduceStoreTypedElement(Node* node);
  Reduction ReduceEffectPhi(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceOtherNode(Node* node);

  Reduction UpdateState(Node* node, AbstractState const* state);

  AbstractState const* ComputeLoopState(Node* node,
                                        AbstractState const* state) const;
  AbstractState const* ComputeLoopStateForStoreField(
      Node* current, LoadElimination::AbstractState const* state,
      FieldAccess const& access) const;
  AbstractState const* UpdateStateForPhi(AbstractState const* state,
                                         Node* effect_phi, Node* phi);

  static int FieldIndexOf(int offset);
  static int FieldIndexOf(FieldAccess const& access);

  static AbstractState const* empty_state() {
    return AbstractState::empty_state();
  }

  CommonOperatorBuilder* common() const;
  Isolate* isolate() const;
  Factory* factory() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Zone* zone() const { return node_states_.zone(); }

  AbstractStateForEffectNodes node_states_;
  JSGraph* const jsgraph_;

  DISALLOW_COPY_AND_ASSIGN(LoadElimination);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOAD_ELIMINATION_H_

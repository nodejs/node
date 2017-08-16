// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOAD_ELIMINATION_H_
#define V8_COMPILER_LOAD_ELIMINATION_H_

#include "src/base/compiler-specific.h"
#include "src/compiler/graph-reducer.h"
#include "src/globals.h"
#include "src/machine-type.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;

namespace compiler {

// Foward declarations.
class CommonOperatorBuilder;
struct FieldAccess;
class Graph;
class JSGraph;

class V8_EXPORT_PRIVATE LoadElimination final
    : public NON_EXPORTED_BASE(AdvancedReducer) {
 public:
  LoadElimination(Editor* editor, JSGraph* jsgraph, Zone* zone)
      : AdvancedReducer(editor), node_states_(zone), jsgraph_(jsgraph) {}
  ~LoadElimination() final {}

  Reduction Reduce(Node* node) final;

 private:
  static const size_t kMaxTrackedChecks = 8;

  // Abstract state to approximate the current state of checks that are
  // only invalidated by calls, i.e. array buffer neutering checks, along
  // the effect paths through the graph.
  class AbstractChecks final : public ZoneObject {
   public:
    explicit AbstractChecks(Zone* zone) {
      for (size_t i = 0; i < arraysize(nodes_); ++i) {
        nodes_[i] = nullptr;
      }
    }
    AbstractChecks(Node* node, Zone* zone) : AbstractChecks(zone) {
      nodes_[next_index_++] = node;
    }

    AbstractChecks const* Extend(Node* node, Zone* zone) const {
      AbstractChecks* that = new (zone) AbstractChecks(*this);
      that->nodes_[that->next_index_] = node;
      that->next_index_ = (that->next_index_ + 1) % arraysize(nodes_);
      return that;
    }
    Node* Lookup(Node* node) const;
    bool Equals(AbstractChecks const* that) const;
    AbstractChecks const* Merge(AbstractChecks const* that, Zone* zone) const;

    void Print() const;

   private:
    Node* nodes_[kMaxTrackedChecks];
    size_t next_index_ = 0;
  };

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
      Element() {}
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

  // Abstract state to approximate the current state of a certain field along
  // the effect paths through the graph.
  class AbstractField final : public ZoneObject {
   public:
    explicit AbstractField(Zone* zone) : info_for_node_(zone) {}
    AbstractField(Node* object, Node* value, Zone* zone)
        : info_for_node_(zone) {
      info_for_node_.insert(std::make_pair(object, value));
    }

    AbstractField const* Extend(Node* object, Node* value, Zone* zone) const {
      AbstractField* that = new (zone) AbstractField(zone);
      that->info_for_node_ = this->info_for_node_;
      that->info_for_node_.insert(std::make_pair(object, value));
      return that;
    }
    Node* Lookup(Node* object) const;
    AbstractField const* Kill(Node* object, Zone* zone) const;
    bool Equals(AbstractField const* that) const {
      return this == that || this->info_for_node_ == that->info_for_node_;
    }
    AbstractField const* Merge(AbstractField const* that, Zone* zone) const {
      if (this->Equals(that)) return this;
      AbstractField* copy = new (zone) AbstractField(zone);
      for (auto this_it : this->info_for_node_) {
        Node* this_object = this_it.first;
        Node* this_value = this_it.second;
        auto that_it = that->info_for_node_.find(this_object);
        if (that_it != that->info_for_node_.end() &&
            that_it->second == this_value) {
          copy->info_for_node_.insert(this_it);
        }
      }
      return copy;
    }

    void Print() const;

   private:
    ZoneMap<Node*, Node*> info_for_node_;
  };

  static size_t const kMaxTrackedFields = 32;

  // Abstract state to approximate the current map of an object along the
  // effect paths through the graph.
  class AbstractMaps final : public ZoneObject {
   public:
    explicit AbstractMaps(Zone* zone) : info_for_node_(zone) {}
    AbstractMaps(Node* object, ZoneHandleSet<Map> maps, Zone* zone)
        : info_for_node_(zone) {
      info_for_node_.insert(std::make_pair(object, maps));
    }

    AbstractMaps const* Extend(Node* object, ZoneHandleSet<Map> maps,
                               Zone* zone) const {
      AbstractMaps* that = new (zone) AbstractMaps(zone);
      that->info_for_node_ = this->info_for_node_;
      that->info_for_node_.insert(std::make_pair(object, maps));
      return that;
    }
    bool Lookup(Node* object, ZoneHandleSet<Map>* object_maps) const;
    AbstractMaps const* Kill(Node* object, Zone* zone) const;
    bool Equals(AbstractMaps const* that) const {
      return this == that || this->info_for_node_ == that->info_for_node_;
    }
    AbstractMaps const* Merge(AbstractMaps const* that, Zone* zone) const {
      if (this->Equals(that)) return this;
      AbstractMaps* copy = new (zone) AbstractMaps(zone);
      for (auto this_it : this->info_for_node_) {
        Node* this_object = this_it.first;
        ZoneHandleSet<Map> this_maps = this_it.second;
        auto that_it = that->info_for_node_.find(this_object);
        if (that_it != that->info_for_node_.end() &&
            that_it->second == this_maps) {
          copy->info_for_node_.insert(this_it);
        }
      }
      return copy;
    }

    void Print() const;

   private:
    ZoneMap<Node*, ZoneHandleSet<Map>> info_for_node_;
  };

  class AbstractState final : public ZoneObject {
   public:
    AbstractState() {
      for (size_t i = 0; i < arraysize(fields_); ++i) {
        fields_[i] = nullptr;
      }
    }

    bool Equals(AbstractState const* that) const;
    void Merge(AbstractState const* that, Zone* zone);

    AbstractState const* AddMaps(Node* object, ZoneHandleSet<Map> maps,
                                 Zone* zone) const;
    AbstractState const* KillMaps(Node* object, Zone* zone) const;
    bool LookupMaps(Node* object, ZoneHandleSet<Map>* object_maps) const;

    AbstractState const* AddField(Node* object, size_t index, Node* value,
                                  Zone* zone) const;
    AbstractState const* KillField(Node* object, size_t index,
                                   Zone* zone) const;
    AbstractState const* KillFields(Node* object, Zone* zone) const;
    Node* LookupField(Node* object, size_t index) const;

    AbstractState const* AddElement(Node* object, Node* index, Node* value,
                                    MachineRepresentation representation,
                                    Zone* zone) const;
    AbstractState const* KillElement(Node* object, Node* index,
                                     Zone* zone) const;
    Node* LookupElement(Node* object, Node* index,
                        MachineRepresentation representation) const;

    AbstractState const* AddCheck(Node* node, Zone* zone) const;
    Node* LookupCheck(Node* node) const;

    void Print() const;

   private:
    AbstractChecks const* checks_ = nullptr;
    AbstractElements const* elements_ = nullptr;
    AbstractField const* fields_[kMaxTrackedFields];
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

  Reduction ReduceArrayBufferWasNeutered(Node* node);
  Reduction ReduceCheckMaps(Node* node);
  Reduction ReduceEnsureWritableFastElements(Node* node);
  Reduction ReduceMaybeGrowFastElements(Node* node);
  Reduction ReduceTransitionElementsKind(Node* node);
  Reduction ReduceLoadField(Node* node);
  Reduction ReduceStoreField(Node* node);
  Reduction ReduceLoadElement(Node* node);
  Reduction ReduceStoreElement(Node* node);
  Reduction ReduceStoreTypedElement(Node* node);
  Reduction ReduceEffectPhi(Node* node);
  Reduction ReduceStart(Node* node);
  Reduction ReduceOtherNode(Node* node);

  Reduction UpdateState(Node* node, AbstractState const* state);

  AbstractState const* ComputeLoopState(Node* node,
                                        AbstractState const* state) const;

  static int FieldIndexOf(int offset);
  static int FieldIndexOf(FieldAccess const& access);

  CommonOperatorBuilder* common() const;
  AbstractState const* empty_state() const { return &empty_state_; }
  Factory* factory() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  Zone* zone() const { return node_states_.zone(); }

  AbstractState const empty_state_;
  AbstractStateForEffectNodes node_states_;
  JSGraph* const jsgraph_;

  DISALLOW_COPY_AND_ASSIGN(LoadElimination);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOAD_ELIMINATION_H_

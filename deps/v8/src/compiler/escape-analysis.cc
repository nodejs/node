// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/escape-analysis.h"

#include <limits>

#include "src/base/flags.h"
#include "src/bootstrapper.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

typedef NodeId Alias;

#ifdef DEBUG
#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_turbo_escape) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

// EscapeStatusAnalysis determines for each allocation whether it escapes.
class EscapeStatusAnalysis : public ZoneObject {
 public:
  enum Status {
    kUnknown = 0u,
    kTracked = 1u << 0,
    kEscaped = 1u << 1,
    kOnStack = 1u << 2,
    kVisited = 1u << 3,
    // A node is dangling, if it is a load of some kind, and does not have
    // an effect successor.
    kDanglingComputed = 1u << 4,
    kDangling = 1u << 5,
    // A node is is an effect branch point, if it has more than 2 non-dangling
    // effect successors.
    kBranchPointComputed = 1u << 6,
    kBranchPoint = 1u << 7,
    kInQueue = 1u << 8
  };
  typedef base::Flags<Status, uint16_t> StatusFlags;

  void RunStatusAnalysis();

  bool IsVirtual(Node* node);
  bool IsEscaped(Node* node);
  bool IsAllocation(Node* node);

  bool IsInQueue(NodeId id);
  void SetInQueue(NodeId id, bool on_stack);

  void DebugPrint();

  EscapeStatusAnalysis(EscapeAnalysis* object_analysis, Graph* graph,
                       Zone* zone);
  void EnqueueForStatusAnalysis(Node* node);
  bool SetEscaped(Node* node);
  bool IsEffectBranchPoint(Node* node);
  bool IsDanglingEffectNode(Node* node);
  void ResizeStatusVector();
  size_t GetStatusVectorSize();
  bool IsVirtual(NodeId id);

  Graph* graph() const { return graph_; }
  void AssignAliases();
  Alias GetAlias(NodeId id) const { return aliases_[id]; }
  const ZoneVector<Alias>& GetAliasMap() const { return aliases_; }
  Alias AliasCount() const { return next_free_alias_; }
  static const Alias kNotReachable;
  static const Alias kUntrackable;

  bool IsNotReachable(Node* node);

 private:
  void Process(Node* node);
  void ProcessAllocate(Node* node);
  void ProcessFinishRegion(Node* node);
  void ProcessStoreField(Node* node);
  void ProcessStoreElement(Node* node);
  bool CheckUsesForEscape(Node* node, bool phi_escaping = false) {
    return CheckUsesForEscape(node, node, phi_escaping);
  }
  bool CheckUsesForEscape(Node* node, Node* rep, bool phi_escaping = false);
  void RevisitUses(Node* node);
  void RevisitInputs(Node* node);

  Alias NextAlias() { return next_free_alias_++; }

  bool HasEntry(Node* node);

  bool IsAllocationPhi(Node* node);

  ZoneVector<Node*> stack_;
  EscapeAnalysis* object_analysis_;
  Graph* const graph_;
  ZoneVector<StatusFlags> status_;
  Alias next_free_alias_;
  ZoneVector<Node*> status_stack_;
  ZoneVector<Alias> aliases_;

  DISALLOW_COPY_AND_ASSIGN(EscapeStatusAnalysis);
};

DEFINE_OPERATORS_FOR_FLAGS(EscapeStatusAnalysis::StatusFlags)

const Alias EscapeStatusAnalysis::kNotReachable =
    std::numeric_limits<Alias>::max();
const Alias EscapeStatusAnalysis::kUntrackable =
    std::numeric_limits<Alias>::max() - 1;

class VirtualObject : public ZoneObject {
 public:
  enum Status {
    kInitial = 0,
    kTracked = 1u << 0,
    kInitialized = 1u << 1,
    kCopyRequired = 1u << 2,
  };
  typedef base::Flags<Status, unsigned char> StatusFlags;

  VirtualObject(NodeId id, VirtualState* owner, Zone* zone)
      : id_(id),
        status_(kInitial),
        fields_(zone),
        phi_(zone),
        object_state_(nullptr),
        owner_(owner) {}

  VirtualObject(VirtualState* owner, const VirtualObject& other)
      : id_(other.id_),
        status_(other.status_ & ~kCopyRequired),
        fields_(other.fields_),
        phi_(other.phi_),
        object_state_(other.object_state_),
        owner_(owner) {}

  VirtualObject(NodeId id, VirtualState* owner, Zone* zone, size_t field_number,
                bool initialized)
      : id_(id),
        status_(kTracked | (initialized ? kInitialized : kInitial)),
        fields_(zone),
        phi_(zone),
        object_state_(nullptr),
        owner_(owner) {
    fields_.resize(field_number);
    phi_.resize(field_number, false);
  }

  Node* GetField(size_t offset) { return fields_[offset]; }

  bool IsCreatedPhi(size_t offset) { return phi_[offset]; }

  void SetField(size_t offset, Node* node, bool created_phi = false) {
    fields_[offset] = node;
    phi_[offset] = created_phi;
  }
  bool IsTracked() const { return status_ & kTracked; }
  bool IsInitialized() const { return status_ & kInitialized; }
  bool SetInitialized() { return status_ |= kInitialized; }
  VirtualState* owner() const { return owner_; }

  Node** fields_array() { return &fields_.front(); }
  size_t field_count() { return fields_.size(); }
  bool ResizeFields(size_t field_count) {
    if (field_count > fields_.size()) {
      fields_.resize(field_count);
      phi_.resize(field_count);
      return true;
    }
    return false;
  }
  void ClearAllFields() {
    for (size_t i = 0; i < fields_.size(); ++i) {
      fields_[i] = nullptr;
      phi_[i] = false;
    }
  }
  bool AllFieldsClear() {
    for (size_t i = 0; i < fields_.size(); ++i) {
      if (fields_[i] != nullptr) {
        return false;
      }
    }
    return true;
  }
  bool UpdateFrom(const VirtualObject& other);
  bool MergeFrom(MergeCache* cache, Node* at, Graph* graph,
                 CommonOperatorBuilder* common, bool initialMerge);
  void SetObjectState(Node* node) { object_state_ = node; }
  Node* GetObjectState() const { return object_state_; }
  bool IsCopyRequired() const { return status_ & kCopyRequired; }
  void SetCopyRequired() { status_ |= kCopyRequired; }
  bool NeedCopyForModification() {
    if (!IsCopyRequired() || !IsInitialized()) {
      return false;
    }
    return true;
  }

  NodeId id() const { return id_; }
  void id(NodeId id) { id_ = id; }

 private:
  bool MergeFields(size_t i, Node* at, MergeCache* cache, Graph* graph,
                   CommonOperatorBuilder* common);

  NodeId id_;
  StatusFlags status_;
  ZoneVector<Node*> fields_;
  ZoneVector<bool> phi_;
  Node* object_state_;
  VirtualState* owner_;

  DISALLOW_COPY_AND_ASSIGN(VirtualObject);
};

DEFINE_OPERATORS_FOR_FLAGS(VirtualObject::StatusFlags)

bool VirtualObject::UpdateFrom(const VirtualObject& other) {
  bool changed = status_ != other.status_;
  status_ = other.status_;
  phi_ = other.phi_;
  if (fields_.size() != other.fields_.size()) {
    fields_ = other.fields_;
    return true;
  }
  for (size_t i = 0; i < fields_.size(); ++i) {
    if (fields_[i] != other.fields_[i]) {
      changed = true;
      fields_[i] = other.fields_[i];
    }
  }
  return changed;
}

class VirtualState : public ZoneObject {
 public:
  VirtualState(Node* owner, Zone* zone, size_t size)
      : info_(size, nullptr, zone),
        initialized_(static_cast<int>(size), zone),
        owner_(owner) {}

  VirtualState(Node* owner, const VirtualState& state)
      : info_(state.info_.size(), nullptr, state.info_.get_allocator().zone()),
        initialized_(state.initialized_.length(),
                     state.info_.get_allocator().zone()),
        owner_(owner) {
    for (size_t i = 0; i < info_.size(); ++i) {
      if (state.info_[i]) {
        info_[i] = state.info_[i];
      }
    }
  }

  VirtualObject* VirtualObjectFromAlias(size_t alias);
  void SetVirtualObject(Alias alias, VirtualObject* state);
  bool UpdateFrom(VirtualState* state, Zone* zone);
  bool MergeFrom(MergeCache* cache, Zone* zone, Graph* graph,
                 CommonOperatorBuilder* common, Node* at);
  size_t size() const { return info_.size(); }
  Node* owner() const { return owner_; }
  VirtualObject* Copy(VirtualObject* obj, Alias alias);
  void SetCopyRequired() {
    for (VirtualObject* obj : info_) {
      if (obj) obj->SetCopyRequired();
    }
  }

 private:
  ZoneVector<VirtualObject*> info_;
  BitVector initialized_;
  Node* owner_;

  DISALLOW_COPY_AND_ASSIGN(VirtualState);
};

class MergeCache : public ZoneObject {
 public:
  explicit MergeCache(Zone* zone)
      : states_(zone), objects_(zone), fields_(zone) {
    states_.reserve(5);
    objects_.reserve(5);
    fields_.reserve(5);
  }
  ZoneVector<VirtualState*>& states() { return states_; }
  ZoneVector<VirtualObject*>& objects() { return objects_; }
  ZoneVector<Node*>& fields() { return fields_; }
  void Clear() {
    states_.clear();
    objects_.clear();
    fields_.clear();
  }
  size_t LoadVirtualObjectsFromStatesFor(Alias alias);
  void LoadVirtualObjectsForFieldsFrom(VirtualState* state,
                                       const ZoneVector<Alias>& aliases);
  Node* GetFields(size_t pos);

 private:
  ZoneVector<VirtualState*> states_;
  ZoneVector<VirtualObject*> objects_;
  ZoneVector<Node*> fields_;

  DISALLOW_COPY_AND_ASSIGN(MergeCache);
};

size_t MergeCache::LoadVirtualObjectsFromStatesFor(Alias alias) {
  objects_.clear();
  DCHECK_GT(states_.size(), 0u);
  size_t min = std::numeric_limits<size_t>::max();
  for (VirtualState* state : states_) {
    if (VirtualObject* obj = state->VirtualObjectFromAlias(alias)) {
      objects_.push_back(obj);
      min = std::min(obj->field_count(), min);
    }
  }
  return min;
}

void MergeCache::LoadVirtualObjectsForFieldsFrom(
    VirtualState* state, const ZoneVector<Alias>& aliases) {
  objects_.clear();
  size_t max_alias = state->size();
  for (Node* field : fields_) {
    Alias alias = aliases[field->id()];
    if (alias >= max_alias) continue;
    if (VirtualObject* obj = state->VirtualObjectFromAlias(alias)) {
      objects_.push_back(obj);
    }
  }
}

Node* MergeCache::GetFields(size_t pos) {
  fields_.clear();
  Node* rep = pos >= objects_.front()->field_count()
                  ? nullptr
                  : objects_.front()->GetField(pos);
  for (VirtualObject* obj : objects_) {
    if (pos >= obj->field_count()) continue;
    Node* field = obj->GetField(pos);
    if (field) {
      fields_.push_back(field);
    }
    if (field != rep) {
      rep = nullptr;
    }
  }
  return rep;
}

VirtualObject* VirtualState::Copy(VirtualObject* obj, Alias alias) {
  if (obj->owner() == this) return obj;
  VirtualObject* new_obj =
      new (info_.get_allocator().zone()) VirtualObject(this, *obj);
  TRACE("At state %p, alias @%d (#%d), copying virtual object from %p to %p\n",
        static_cast<void*>(this), alias, obj->id(), static_cast<void*>(obj),
        static_cast<void*>(new_obj));
  info_[alias] = new_obj;
  return new_obj;
}

VirtualObject* VirtualState::VirtualObjectFromAlias(size_t alias) {
  return info_[alias];
}

void VirtualState::SetVirtualObject(Alias alias, VirtualObject* obj) {
  info_[alias] = obj;
  if (obj) initialized_.Add(alias);
}

bool VirtualState::UpdateFrom(VirtualState* from, Zone* zone) {
  if (from == this) return false;
  bool changed = false;
  for (Alias alias = 0; alias < size(); ++alias) {
    VirtualObject* ls = VirtualObjectFromAlias(alias);
    VirtualObject* rs = from->VirtualObjectFromAlias(alias);

    if (ls == rs || rs == nullptr) continue;

    if (ls == nullptr) {
      ls = new (zone) VirtualObject(this, *rs);
      SetVirtualObject(alias, ls);
      changed = true;
      continue;
    }

    TRACE("  Updating fields of @%d\n", alias);

    changed = ls->UpdateFrom(*rs) || changed;
  }
  return false;
}

namespace {

bool IsEquivalentPhi(Node* node1, Node* node2) {
  if (node1 == node2) return true;
  if (node1->opcode() != IrOpcode::kPhi || node2->opcode() != IrOpcode::kPhi ||
      node1->op()->ValueInputCount() != node2->op()->ValueInputCount()) {
    return false;
  }
  for (int i = 0; i < node1->op()->ValueInputCount(); ++i) {
    Node* input1 = NodeProperties::GetValueInput(node1, i);
    Node* input2 = NodeProperties::GetValueInput(node2, i);
    if (!IsEquivalentPhi(input1, input2)) {
      return false;
    }
  }
  return true;
}

bool IsEquivalentPhi(Node* phi, ZoneVector<Node*>& inputs) {
  if (phi->opcode() != IrOpcode::kPhi) return false;
  if (static_cast<size_t>(phi->op()->ValueInputCount()) != inputs.size()) {
    return false;
  }
  for (size_t i = 0; i < inputs.size(); ++i) {
    Node* input = NodeProperties::GetValueInput(phi, static_cast<int>(i));
    if (!IsEquivalentPhi(input, inputs[i])) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool VirtualObject::MergeFields(size_t i, Node* at, MergeCache* cache,
                                Graph* graph, CommonOperatorBuilder* common) {
  bool changed = false;
  int value_input_count = static_cast<int>(cache->fields().size());
  Node* rep = GetField(i);
  if (!rep || !IsCreatedPhi(i)) {
    Type* phi_type = Type::None();
    for (Node* input : cache->fields()) {
      CHECK_NOT_NULL(input);
      CHECK(!input->IsDead());
      Type* input_type = NodeProperties::GetType(input);
      phi_type = Type::Union(phi_type, input_type, graph->zone());
    }
    Node* control = NodeProperties::GetControlInput(at);
    cache->fields().push_back(control);
    Node* phi = graph->NewNode(
        common->Phi(MachineRepresentation::kTagged, value_input_count),
        value_input_count + 1, &cache->fields().front());
    NodeProperties::SetType(phi, phi_type);
    SetField(i, phi, true);

#ifdef DEBUG
    if (FLAG_trace_turbo_escape) {
      PrintF("    Creating Phi #%d as merge of", phi->id());
      for (int i = 0; i < value_input_count; i++) {
        PrintF(" #%d (%s)", cache->fields()[i]->id(),
               cache->fields()[i]->op()->mnemonic());
      }
      PrintF("\n");
    }
#endif
    changed = true;
  } else {
    DCHECK(rep->opcode() == IrOpcode::kPhi);
    for (int n = 0; n < value_input_count; ++n) {
      Node* old = NodeProperties::GetValueInput(rep, n);
      if (old != cache->fields()[n]) {
        changed = true;
        NodeProperties::ReplaceValueInput(rep, cache->fields()[n], n);
      }
    }
  }
  return changed;
}

bool VirtualObject::MergeFrom(MergeCache* cache, Node* at, Graph* graph,
                              CommonOperatorBuilder* common,
                              bool initialMerge) {
  DCHECK(at->opcode() == IrOpcode::kEffectPhi ||
         at->opcode() == IrOpcode::kPhi);
  bool changed = false;
  for (size_t i = 0; i < field_count(); ++i) {
    if (!initialMerge && GetField(i) == nullptr) continue;
    Node* field = cache->GetFields(i);
    if (field && !IsCreatedPhi(i)) {
      changed = changed || GetField(i) != field;
      SetField(i, field);
      TRACE("    Field %zu agree on rep #%d\n", i, field->id());
    } else {
      size_t arity = at->opcode() == IrOpcode::kEffectPhi
                         ? at->op()->EffectInputCount()
                         : at->op()->ValueInputCount();
      if (cache->fields().size() == arity) {
        changed = MergeFields(i, at, cache, graph, common) || changed;
      } else {
        if (GetField(i) != nullptr) {
          TRACE("    Field %zu cleared\n", i);
          changed = true;
        }
        SetField(i, nullptr);
      }
    }
  }
  return changed;
}

bool VirtualState::MergeFrom(MergeCache* cache, Zone* zone, Graph* graph,
                             CommonOperatorBuilder* common, Node* at) {
  DCHECK_GT(cache->states().size(), 0u);
  bool changed = false;
  for (Alias alias = 0; alias < size(); ++alias) {
    cache->objects().clear();
    VirtualObject* mergeObject = VirtualObjectFromAlias(alias);
    bool copy_merge_object = false;
    size_t fields = std::numeric_limits<size_t>::max();
    for (VirtualState* state : cache->states()) {
      if (VirtualObject* obj = state->VirtualObjectFromAlias(alias)) {
        cache->objects().push_back(obj);
        if (mergeObject == obj) {
          copy_merge_object = true;
        }
        fields = std::min(obj->field_count(), fields);
      }
    }
    if (cache->objects().size() == cache->states().size() &&
        (mergeObject || !initialized_.Contains(alias))) {
      bool initialMerge = false;
      if (!mergeObject) {
        initialMerge = true;
        VirtualObject* obj = new (zone)
            VirtualObject(cache->objects().front()->id(), this, zone, fields,
                          cache->objects().front()->IsInitialized());
        SetVirtualObject(alias, obj);
        mergeObject = obj;
        changed = true;
      } else if (copy_merge_object) {
        VirtualObject* obj = new (zone) VirtualObject(this, *mergeObject);
        SetVirtualObject(alias, obj);
        mergeObject = obj;
        changed = true;
      } else {
        changed = mergeObject->ResizeFields(fields) || changed;
      }
#ifdef DEBUG
      if (FLAG_trace_turbo_escape) {
        PrintF("  Alias @%d, merging into %p virtual objects", alias,
               static_cast<void*>(mergeObject));
        for (size_t i = 0; i < cache->objects().size(); i++) {
          PrintF(" %p", static_cast<void*>(cache->objects()[i]));
        }
        PrintF("\n");
      }
#endif  // DEBUG
      changed =
          mergeObject->MergeFrom(cache, at, graph, common, initialMerge) ||
          changed;
    } else {
      if (mergeObject) {
        TRACE("  Alias %d, virtual object removed\n", alias);
        changed = true;
      }
      SetVirtualObject(alias, nullptr);
    }
  }
  return changed;
}

EscapeStatusAnalysis::EscapeStatusAnalysis(EscapeAnalysis* object_analysis,
                                           Graph* graph, Zone* zone)
    : stack_(zone),
      object_analysis_(object_analysis),
      graph_(graph),
      status_(zone),
      next_free_alias_(0),
      status_stack_(zone),
      aliases_(zone) {}

bool EscapeStatusAnalysis::HasEntry(Node* node) {
  return status_[node->id()] & (kTracked | kEscaped);
}

bool EscapeStatusAnalysis::IsVirtual(Node* node) {
  return IsVirtual(node->id());
}

bool EscapeStatusAnalysis::IsVirtual(NodeId id) {
  return (status_[id] & kTracked) && !(status_[id] & kEscaped);
}

bool EscapeStatusAnalysis::IsEscaped(Node* node) {
  return status_[node->id()] & kEscaped;
}

bool EscapeStatusAnalysis::IsAllocation(Node* node) {
  return node->opcode() == IrOpcode::kAllocate ||
         node->opcode() == IrOpcode::kFinishRegion;
}

bool EscapeStatusAnalysis::SetEscaped(Node* node) {
  bool changed = !(status_[node->id()] & kEscaped);
  status_[node->id()] |= kEscaped | kTracked;
  return changed;
}

bool EscapeStatusAnalysis::IsInQueue(NodeId id) {
  return status_[id] & kInQueue;
}

void EscapeStatusAnalysis::SetInQueue(NodeId id, bool on_stack) {
  if (on_stack) {
    status_[id] |= kInQueue;
  } else {
    status_[id] &= ~kInQueue;
  }
}

void EscapeStatusAnalysis::ResizeStatusVector() {
  if (status_.size() <= graph()->NodeCount()) {
    status_.resize(graph()->NodeCount() * 1.1, kUnknown);
  }
}

size_t EscapeStatusAnalysis::GetStatusVectorSize() { return status_.size(); }

void EscapeStatusAnalysis::RunStatusAnalysis() {
  ResizeStatusVector();
  while (!status_stack_.empty()) {
    Node* node = status_stack_.back();
    status_stack_.pop_back();
    status_[node->id()] &= ~kOnStack;
    Process(node);
    status_[node->id()] |= kVisited;
  }
}

void EscapeStatusAnalysis::EnqueueForStatusAnalysis(Node* node) {
  DCHECK_NOT_NULL(node);
  if (!(status_[node->id()] & kOnStack)) {
    status_stack_.push_back(node);
    status_[node->id()] |= kOnStack;
  }
}

void EscapeStatusAnalysis::RevisitInputs(Node* node) {
  for (Edge edge : node->input_edges()) {
    Node* input = edge.to();
    if (!(status_[input->id()] & kOnStack)) {
      status_stack_.push_back(input);
      status_[input->id()] |= kOnStack;
    }
  }
}

void EscapeStatusAnalysis::RevisitUses(Node* node) {
  for (Edge edge : node->use_edges()) {
    Node* use = edge.from();
    if (!(status_[use->id()] & kOnStack) && !IsNotReachable(use)) {
      status_stack_.push_back(use);
      status_[use->id()] |= kOnStack;
    }
  }
}

void EscapeStatusAnalysis::Process(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kAllocate:
      ProcessAllocate(node);
      break;
    case IrOpcode::kFinishRegion:
      ProcessFinishRegion(node);
      break;
    case IrOpcode::kStoreField:
      ProcessStoreField(node);
      break;
    case IrOpcode::kStoreElement:
      ProcessStoreElement(node);
      break;
    case IrOpcode::kLoadField:
    case IrOpcode::kLoadElement: {
      if (Node* rep = object_analysis_->GetReplacement(node)) {
        if (IsAllocation(rep) && CheckUsesForEscape(node, rep)) {
          RevisitInputs(rep);
          RevisitUses(rep);
        }
      }
      RevisitUses(node);
      break;
    }
    case IrOpcode::kPhi:
      if (!HasEntry(node)) {
        status_[node->id()] |= kTracked;
        RevisitUses(node);
      }
      if (!IsAllocationPhi(node) && SetEscaped(node)) {
        RevisitInputs(node);
        RevisitUses(node);
      }
      CheckUsesForEscape(node);
    default:
      break;
  }
}

bool EscapeStatusAnalysis::IsAllocationPhi(Node* node) {
  for (Edge edge : node->input_edges()) {
    Node* input = edge.to();
    if (input->opcode() == IrOpcode::kPhi && !IsEscaped(input)) continue;
    if (IsAllocation(input)) continue;
    return false;
  }
  return true;
}

void EscapeStatusAnalysis::ProcessStoreField(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStoreField);
  Node* to = NodeProperties::GetValueInput(node, 0);
  Node* val = NodeProperties::GetValueInput(node, 1);
  if ((IsEscaped(to) || !IsAllocation(to)) && SetEscaped(val)) {
    RevisitUses(val);
    RevisitInputs(val);
    TRACE("Setting #%d (%s) to escaped because of store to field of #%d\n",
          val->id(), val->op()->mnemonic(), to->id());
  }
}

void EscapeStatusAnalysis::ProcessStoreElement(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStoreElement);
  Node* to = NodeProperties::GetValueInput(node, 0);
  Node* val = NodeProperties::GetValueInput(node, 2);
  if ((IsEscaped(to) || !IsAllocation(to)) && SetEscaped(val)) {
    RevisitUses(val);
    RevisitInputs(val);
    TRACE("Setting #%d (%s) to escaped because of store to field of #%d\n",
          val->id(), val->op()->mnemonic(), to->id());
  }
}

void EscapeStatusAnalysis::ProcessAllocate(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocate);
  if (!HasEntry(node)) {
    status_[node->id()] |= kTracked;
    TRACE("Created status entry for node #%d (%s)\n", node->id(),
          node->op()->mnemonic());
    NumberMatcher size(node->InputAt(0));
    DCHECK(node->InputAt(0)->opcode() != IrOpcode::kInt32Constant &&
           node->InputAt(0)->opcode() != IrOpcode::kInt64Constant &&
           node->InputAt(0)->opcode() != IrOpcode::kFloat32Constant &&
           node->InputAt(0)->opcode() != IrOpcode::kFloat64Constant);
    RevisitUses(node);
    if (!size.HasValue() && SetEscaped(node)) {
      TRACE("Setting #%d to escaped because of non-const alloc\n", node->id());
      // This node is already known to escape, uses do not have to be checked
      // for escape.
      return;
    }
  }
  if (CheckUsesForEscape(node, true)) {
    RevisitUses(node);
  }
}

bool EscapeStatusAnalysis::CheckUsesForEscape(Node* uses, Node* rep,
                                              bool phi_escaping) {
  for (Edge edge : uses->use_edges()) {
    Node* use = edge.from();
    if (IsNotReachable(use)) continue;
    if (edge.index() >= use->op()->ValueInputCount() +
                            OperatorProperties::GetContextInputCount(use->op()))
      continue;
    switch (use->opcode()) {
      case IrOpcode::kPhi:
        if (phi_escaping && SetEscaped(rep)) {
          TRACE(
              "Setting #%d (%s) to escaped because of use by phi node "
              "#%d (%s)\n",
              rep->id(), rep->op()->mnemonic(), use->id(),
              use->op()->mnemonic());
          return true;
        }
      // Fallthrough.
      case IrOpcode::kStoreField:
      case IrOpcode::kLoadField:
      case IrOpcode::kStoreElement:
      case IrOpcode::kLoadElement:
      case IrOpcode::kFrameState:
      case IrOpcode::kStateValues:
      case IrOpcode::kReferenceEqual:
      case IrOpcode::kFinishRegion:
        if (IsEscaped(use) && SetEscaped(rep)) {
          TRACE(
              "Setting #%d (%s) to escaped because of use by escaping node "
              "#%d (%s)\n",
              rep->id(), rep->op()->mnemonic(), use->id(),
              use->op()->mnemonic());
          return true;
        }
        break;
      case IrOpcode::kObjectIsSmi:
        if (!IsAllocation(rep) && SetEscaped(rep)) {
          TRACE("Setting #%d (%s) to escaped because of use by #%d (%s)\n",
                rep->id(), rep->op()->mnemonic(), use->id(),
                use->op()->mnemonic());
          return true;
        }
        break;
      case IrOpcode::kSelect:
      // TODO(mstarzinger): The following list of operators will eventually be
      // handled by the EscapeAnalysisReducer (similar to ObjectIsSmi).
      case IrOpcode::kConvertTaggedHoleToUndefined:
      case IrOpcode::kStringEqual:
      case IrOpcode::kStringLessThan:
      case IrOpcode::kStringLessThanOrEqual:
      case IrOpcode::kTypeGuard:
      case IrOpcode::kPlainPrimitiveToNumber:
      case IrOpcode::kPlainPrimitiveToWord32:
      case IrOpcode::kPlainPrimitiveToFloat64:
      case IrOpcode::kStringCharAt:
      case IrOpcode::kStringCharCodeAt:
      case IrOpcode::kObjectIsCallable:
      case IrOpcode::kObjectIsNumber:
      case IrOpcode::kObjectIsReceiver:
      case IrOpcode::kObjectIsString:
      case IrOpcode::kObjectIsUndetectable:
        if (SetEscaped(rep)) {
          TRACE("Setting #%d (%s) to escaped because of use by #%d (%s)\n",
                rep->id(), rep->op()->mnemonic(), use->id(),
                use->op()->mnemonic());
          return true;
        }
        break;
      default:
        if (use->op()->EffectInputCount() == 0 &&
            uses->op()->EffectInputCount() > 0 &&
            !IrOpcode::IsJsOpcode(use->opcode())) {
          TRACE("Encountered unaccounted use by #%d (%s)\n", use->id(),
                use->op()->mnemonic());
          UNREACHABLE();
        }
        if (SetEscaped(rep)) {
          TRACE("Setting #%d (%s) to escaped because of use by #%d (%s)\n",
                rep->id(), rep->op()->mnemonic(), use->id(),
                use->op()->mnemonic());
          return true;
        }
    }
  }
  return false;
}

void EscapeStatusAnalysis::ProcessFinishRegion(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kFinishRegion);
  if (!HasEntry(node)) {
    status_[node->id()] |= kTracked;
    RevisitUses(node);
  }
  if (CheckUsesForEscape(node, true)) {
    RevisitInputs(node);
  }
}

void EscapeStatusAnalysis::DebugPrint() {
  for (NodeId id = 0; id < status_.size(); id++) {
    if (status_[id] & kTracked) {
      PrintF("Node #%d is %s\n", id,
             (status_[id] & kEscaped) ? "escaping" : "virtual");
    }
  }
}

EscapeAnalysis::EscapeAnalysis(Graph* graph, CommonOperatorBuilder* common,
                               Zone* zone)
    : zone_(zone),
      slot_not_analyzed_(graph->NewNode(common->NumberConstant(0x1c0debad))),
      common_(common),
      status_analysis_(new (zone) EscapeStatusAnalysis(this, graph, zone)),
      virtual_states_(zone),
      replacements_(zone),
      cycle_detection_(zone),
      cache_(nullptr) {
  // Type slot_not_analyzed_ manually.
  double v = OpParameter<double>(slot_not_analyzed_);
  NodeProperties::SetType(slot_not_analyzed_, Type::Range(v, v, zone));
}

EscapeAnalysis::~EscapeAnalysis() {}

void EscapeAnalysis::Run() {
  replacements_.resize(graph()->NodeCount());
  status_analysis_->AssignAliases();
  if (status_analysis_->AliasCount() > 0) {
    cache_ = new (zone()) MergeCache(zone());
    replacements_.resize(graph()->NodeCount());
    status_analysis_->ResizeStatusVector();
    RunObjectAnalysis();
    status_analysis_->RunStatusAnalysis();
  }
}

void EscapeStatusAnalysis::AssignAliases() {
  size_t max_size = 1024;
  size_t min_size = 32;
  size_t stack_size =
      std::min(std::max(graph()->NodeCount() / 5, min_size), max_size);
  stack_.reserve(stack_size);
  ResizeStatusVector();
  stack_.push_back(graph()->end());
  CHECK_LT(graph()->NodeCount(), kUntrackable);
  aliases_.resize(graph()->NodeCount(), kNotReachable);
  aliases_[graph()->end()->id()] = kUntrackable;
  status_stack_.reserve(8);
  TRACE("Discovering trackable nodes");
  while (!stack_.empty()) {
    Node* node = stack_.back();
    stack_.pop_back();
    switch (node->opcode()) {
      case IrOpcode::kAllocate:
        if (aliases_[node->id()] >= kUntrackable) {
          aliases_[node->id()] = NextAlias();
          TRACE(" @%d:%s#%u", aliases_[node->id()], node->op()->mnemonic(),
                node->id());
          EnqueueForStatusAnalysis(node);
        }
        break;
      case IrOpcode::kFinishRegion: {
        Node* allocate = NodeProperties::GetValueInput(node, 0);
        DCHECK_NOT_NULL(allocate);
        if (allocate->opcode() == IrOpcode::kAllocate) {
          if (aliases_[allocate->id()] >= kUntrackable) {
            if (aliases_[allocate->id()] == kNotReachable) {
              stack_.push_back(allocate);
            }
            aliases_[allocate->id()] = NextAlias();
            TRACE(" @%d:%s#%u", aliases_[allocate->id()],
                  allocate->op()->mnemonic(), allocate->id());
            EnqueueForStatusAnalysis(allocate);
          }
          aliases_[node->id()] = aliases_[allocate->id()];
          TRACE(" @%d:%s#%u", aliases_[node->id()], node->op()->mnemonic(),
                node->id());
        }
        break;
      }
      default:
        DCHECK_EQ(aliases_[node->id()], kUntrackable);
        break;
    }
    for (Edge edge : node->input_edges()) {
      Node* input = edge.to();
      if (aliases_[input->id()] == kNotReachable) {
        stack_.push_back(input);
        aliases_[input->id()] = kUntrackable;
      }
    }
  }
  TRACE("\n");
}

bool EscapeStatusAnalysis::IsNotReachable(Node* node) {
  if (node->id() >= aliases_.size()) {
    return false;
  }
  return aliases_[node->id()] == kNotReachable;
}

void EscapeAnalysis::RunObjectAnalysis() {
  virtual_states_.resize(graph()->NodeCount());
  ZoneDeque<Node*> queue(zone());
  queue.push_back(graph()->start());
  ZoneVector<Node*> danglers(zone());
  while (!queue.empty()) {
    Node* node = queue.back();
    queue.pop_back();
    status_analysis_->SetInQueue(node->id(), false);
    if (Process(node)) {
      for (Edge edge : node->use_edges()) {
        Node* use = edge.from();
        if (status_analysis_->IsNotReachable(use)) {
          continue;
        }
        if (NodeProperties::IsEffectEdge(edge)) {
          // Iteration order: depth first, but delay phis.
          // We need DFS do avoid some duplication of VirtualStates and
          // VirtualObjects, and we want to delay phis to improve performance.
          if (use->opcode() == IrOpcode::kEffectPhi) {
            if (!status_analysis_->IsInQueue(use->id())) {
              status_analysis_->SetInQueue(use->id(), true);
              queue.push_front(use);
            }
          } else if ((use->opcode() != IrOpcode::kLoadField &&
                      use->opcode() != IrOpcode::kLoadElement) ||
                     !status_analysis_->IsDanglingEffectNode(use)) {
            if (!status_analysis_->IsInQueue(use->id())) {
              status_analysis_->SetInQueue(use->id(), true);
              queue.push_back(use);
            }
          } else {
            danglers.push_back(use);
          }
        }
      }
      // Danglers need to be processed immediately, even if they are
      // on the stack. Since they do not have effect outputs,
      // we don't have to track whether they are on the stack.
      queue.insert(queue.end(), danglers.begin(), danglers.end());
      danglers.clear();
    }
  }
#ifdef DEBUG
  if (FLAG_trace_turbo_escape) {
    DebugPrint();
  }
#endif
}

bool EscapeStatusAnalysis::IsDanglingEffectNode(Node* node) {
  if (status_[node->id()] & kDanglingComputed) {
    return status_[node->id()] & kDangling;
  }
  if (node->op()->EffectInputCount() == 0 ||
      node->op()->EffectOutputCount() == 0 ||
      (node->op()->EffectInputCount() == 1 &&
       NodeProperties::GetEffectInput(node)->opcode() == IrOpcode::kStart)) {
    // The start node is used as sentinel for nodes that are in general
    // effectful, but of which an analysis has determined that they do not
    // produce effects in this instance. We don't consider these nodes dangling.
    status_[node->id()] |= kDanglingComputed;
    return false;
  }
  for (Edge edge : node->use_edges()) {
    Node* use = edge.from();
    if (aliases_[use->id()] == kNotReachable) continue;
    if (NodeProperties::IsEffectEdge(edge)) {
      status_[node->id()] |= kDanglingComputed;
      return false;
    }
  }
  status_[node->id()] |= kDanglingComputed | kDangling;
  return true;
}

bool EscapeStatusAnalysis::IsEffectBranchPoint(Node* node) {
  if (status_[node->id()] & kBranchPointComputed) {
    return status_[node->id()] & kBranchPoint;
  }
  int count = 0;
  for (Edge edge : node->use_edges()) {
    Node* use = edge.from();
    if (aliases_[use->id()] == kNotReachable) continue;
    if (NodeProperties::IsEffectEdge(edge)) {
      if ((use->opcode() == IrOpcode::kLoadField ||
           use->opcode() == IrOpcode::kLoadElement ||
           use->opcode() == IrOpcode::kLoad) &&
          IsDanglingEffectNode(use))
        continue;
      if (++count > 1) {
        status_[node->id()] |= kBranchPointComputed | kBranchPoint;
        return true;
      }
    }
  }
  status_[node->id()] |= kBranchPointComputed;
  return false;
}

namespace {

bool HasFrameStateInput(const Operator* op) {
  if (op->opcode() == IrOpcode::kCall || op->opcode() == IrOpcode::kTailCall) {
    const CallDescriptor* d = CallDescriptorOf(op);
    return d->NeedsFrameState();
  } else {
    return OperatorProperties::HasFrameStateInput(op);
  }
}

}  // namespace

bool EscapeAnalysis::Process(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kAllocate:
      ProcessAllocation(node);
      break;
    case IrOpcode::kBeginRegion:
      ForwardVirtualState(node);
      break;
    case IrOpcode::kFinishRegion:
      ProcessFinishRegion(node);
      break;
    case IrOpcode::kStoreField:
      ProcessStoreField(node);
      break;
    case IrOpcode::kLoadField:
      ProcessLoadField(node);
      break;
    case IrOpcode::kStoreElement:
      ProcessStoreElement(node);
      break;
    case IrOpcode::kLoadElement:
      ProcessLoadElement(node);
      break;
    case IrOpcode::kStart:
      ProcessStart(node);
      break;
    case IrOpcode::kEffectPhi:
      return ProcessEffectPhi(node);
      break;
    default:
      if (node->op()->EffectInputCount() > 0) {
        ForwardVirtualState(node);
      }
      ProcessAllocationUsers(node);
      break;
  }
  if (HasFrameStateInput(node->op())) {
    virtual_states_[node->id()]->SetCopyRequired();
  }
  return true;
}

void EscapeAnalysis::ProcessAllocationUsers(Node* node) {
  for (Edge edge : node->input_edges()) {
    Node* input = edge.to();
    Node* use = edge.from();
    if (edge.index() >= use->op()->ValueInputCount() +
                            OperatorProperties::GetContextInputCount(use->op()))
      continue;
    switch (node->opcode()) {
      case IrOpcode::kStoreField:
      case IrOpcode::kLoadField:
      case IrOpcode::kStoreElement:
      case IrOpcode::kLoadElement:
      case IrOpcode::kFrameState:
      case IrOpcode::kStateValues:
      case IrOpcode::kReferenceEqual:
      case IrOpcode::kFinishRegion:
      case IrOpcode::kObjectIsSmi:
        break;
      default:
        VirtualState* state = virtual_states_[node->id()];
        if (VirtualObject* obj =
                GetVirtualObject(state, ResolveReplacement(input))) {
          if (!obj->AllFieldsClear()) {
            obj = CopyForModificationAt(obj, state, node);
            obj->ClearAllFields();
            TRACE("Cleared all fields of @%d:#%d\n",
                  status_analysis_->GetAlias(obj->id()), obj->id());
          }
        }
        break;
    }
  }
}

VirtualState* EscapeAnalysis::CopyForModificationAt(VirtualState* state,
                                                    Node* node) {
  if (state->owner() != node) {
    VirtualState* new_state = new (zone()) VirtualState(node, *state);
    virtual_states_[node->id()] = new_state;
    TRACE("Copying virtual state %p to new state %p at node %s#%d\n",
          static_cast<void*>(state), static_cast<void*>(new_state),
          node->op()->mnemonic(), node->id());
    return new_state;
  }
  return state;
}

VirtualObject* EscapeAnalysis::CopyForModificationAt(VirtualObject* obj,
                                                     VirtualState* state,
                                                     Node* node) {
  if (obj->NeedCopyForModification()) {
    state = CopyForModificationAt(state, node);
    // TODO(tebbi): this copies the complete virtual state. Replace with a more
    // precise analysis of which objects are actually affected by the change.
    Alias changed_alias = status_analysis_->GetAlias(obj->id());
    for (Alias alias = 0; alias < state->size(); ++alias) {
      if (VirtualObject* next_obj = state->VirtualObjectFromAlias(alias)) {
        if (alias != changed_alias && next_obj->NeedCopyForModification()) {
          state->Copy(next_obj, alias);
        }
      }
    }
    return state->Copy(obj, changed_alias);
  }
  return obj;
}

void EscapeAnalysis::ForwardVirtualState(Node* node) {
  DCHECK_EQ(node->op()->EffectInputCount(), 1);
#ifdef DEBUG
  if (node->opcode() != IrOpcode::kLoadField &&
      node->opcode() != IrOpcode::kLoadElement &&
      node->opcode() != IrOpcode::kLoad &&
      status_analysis_->IsDanglingEffectNode(node)) {
    PrintF("Dangeling effect node: #%d (%s)\n", node->id(),
           node->op()->mnemonic());
    UNREACHABLE();
  }
#endif  // DEBUG
  Node* effect = NodeProperties::GetEffectInput(node);
  DCHECK_NOT_NULL(virtual_states_[effect->id()]);
  if (virtual_states_[node->id()]) {
    virtual_states_[node->id()]->UpdateFrom(virtual_states_[effect->id()],
                                            zone());
  } else {
    virtual_states_[node->id()] = virtual_states_[effect->id()];
    TRACE("Forwarding object state %p from %s#%d to %s#%d",
          static_cast<void*>(virtual_states_[effect->id()]),
          effect->op()->mnemonic(), effect->id(), node->op()->mnemonic(),
          node->id());
    if (status_analysis_->IsEffectBranchPoint(effect)) {
      virtual_states_[node->id()]->SetCopyRequired();
      TRACE(", effect input %s#%d is branch point", effect->op()->mnemonic(),
            effect->id());
    }
    TRACE("\n");
  }
}

void EscapeAnalysis::ProcessStart(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStart);
  virtual_states_[node->id()] =
      new (zone()) VirtualState(node, zone(), status_analysis_->AliasCount());
}

bool EscapeAnalysis::ProcessEffectPhi(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kEffectPhi);
  bool changed = false;

  VirtualState* mergeState = virtual_states_[node->id()];
  if (!mergeState) {
    mergeState =
        new (zone()) VirtualState(node, zone(), status_analysis_->AliasCount());
    virtual_states_[node->id()] = mergeState;
    changed = true;
    TRACE("Effect Phi #%d got new virtual state %p.\n", node->id(),
          static_cast<void*>(mergeState));
  }

  cache_->Clear();

  TRACE("At Effect Phi #%d, merging states into %p:", node->id(),
        static_cast<void*>(mergeState));

  for (int i = 0; i < node->op()->EffectInputCount(); ++i) {
    Node* input = NodeProperties::GetEffectInput(node, i);
    VirtualState* state = virtual_states_[input->id()];
    if (state) {
      cache_->states().push_back(state);
      if (state == mergeState) {
        mergeState = new (zone())
            VirtualState(node, zone(), status_analysis_->AliasCount());
        virtual_states_[node->id()] = mergeState;
        changed = true;
      }
    }
    TRACE(" %p (from %d %s)", static_cast<void*>(state), input->id(),
          input->op()->mnemonic());
  }
  TRACE("\n");

  if (cache_->states().size() == 0) {
    return changed;
  }

  changed =
      mergeState->MergeFrom(cache_, zone(), graph(), common(), node) || changed;

  TRACE("Merge %s the node.\n", changed ? "changed" : "did not change");

  if (changed) {
    status_analysis_->ResizeStatusVector();
  }
  return changed;
}

void EscapeAnalysis::ProcessAllocation(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocate);
  ForwardVirtualState(node);
  VirtualState* state = virtual_states_[node->id()];
  Alias alias = status_analysis_->GetAlias(node->id());

  // Check if we have already processed this node.
  if (state->VirtualObjectFromAlias(alias)) {
    return;
  }

  if (state->owner()->opcode() == IrOpcode::kEffectPhi) {
    state = CopyForModificationAt(state, node);
  }

  NumberMatcher size(node->InputAt(0));
  DCHECK(node->InputAt(0)->opcode() != IrOpcode::kInt32Constant &&
         node->InputAt(0)->opcode() != IrOpcode::kInt64Constant &&
         node->InputAt(0)->opcode() != IrOpcode::kFloat32Constant &&
         node->InputAt(0)->opcode() != IrOpcode::kFloat64Constant);
  if (size.HasValue()) {
    VirtualObject* obj = new (zone()) VirtualObject(
        node->id(), state, zone(), size.Value() / kPointerSize, false);
    state->SetVirtualObject(alias, obj);
  } else {
    state->SetVirtualObject(
        alias, new (zone()) VirtualObject(node->id(), state, zone()));
  }
}

void EscapeAnalysis::ProcessFinishRegion(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kFinishRegion);
  ForwardVirtualState(node);
  Node* allocation = NodeProperties::GetValueInput(node, 0);
  if (allocation->opcode() == IrOpcode::kAllocate) {
    VirtualState* state = virtual_states_[node->id()];
    VirtualObject* obj =
        state->VirtualObjectFromAlias(status_analysis_->GetAlias(node->id()));
    DCHECK_NOT_NULL(obj);
    obj->SetInitialized();
  }
}

Node* EscapeAnalysis::replacement(Node* node) {
  if (node->id() >= replacements_.size()) return nullptr;
  return replacements_[node->id()];
}

bool EscapeAnalysis::SetReplacement(Node* node, Node* rep) {
  bool changed = replacements_[node->id()] != rep;
  replacements_[node->id()] = rep;
  return changed;
}

bool EscapeAnalysis::UpdateReplacement(VirtualState* state, Node* node,
                                       Node* rep) {
  if (SetReplacement(node, rep)) {
    if (rep) {
      TRACE("Replacement of #%d is #%d (%s)\n", node->id(), rep->id(),
            rep->op()->mnemonic());
    } else {
      TRACE("Replacement of #%d cleared\n", node->id());
    }
    return true;
  }
  return false;
}

Node* EscapeAnalysis::ResolveReplacement(Node* node) {
  while (replacement(node)) {
    node = replacement(node);
  }
  return node;
}

Node* EscapeAnalysis::GetReplacement(Node* node) {
  Node* result = nullptr;
  while (replacement(node)) {
    node = result = replacement(node);
  }
  return result;
}

bool EscapeAnalysis::IsVirtual(Node* node) {
  if (node->id() >= status_analysis_->GetStatusVectorSize()) {
    return false;
  }
  return status_analysis_->IsVirtual(node);
}

bool EscapeAnalysis::IsEscaped(Node* node) {
  if (node->id() >= status_analysis_->GetStatusVectorSize()) {
    return false;
  }
  return status_analysis_->IsEscaped(node);
}

bool EscapeAnalysis::CompareVirtualObjects(Node* left, Node* right) {
  DCHECK(IsVirtual(left) && IsVirtual(right));
  left = ResolveReplacement(left);
  right = ResolveReplacement(right);
  if (IsEquivalentPhi(left, right)) {
    return true;
  }
  return false;
}

namespace {

bool IsOffsetForFieldAccessCorrect(const FieldAccess& access) {
#if V8_TARGET_LITTLE_ENDIAN
  return (access.offset % kPointerSize) == 0;
#else
  return ((access.offset +
           (1 << ElementSizeLog2Of(access.machine_type.representation()))) %
          kPointerSize) == 0;
#endif
}

int OffsetForFieldAccess(Node* node) {
  FieldAccess access = FieldAccessOf(node->op());
  DCHECK(IsOffsetForFieldAccessCorrect(access));
  return access.offset / kPointerSize;
}

int OffsetForElementAccess(Node* node, int index) {
  ElementAccess access = ElementAccessOf(node->op());
  DCHECK_GE(ElementSizeLog2Of(access.machine_type.representation()),
            kPointerSizeLog2);
  DCHECK_EQ(access.header_size % kPointerSize, 0);
  return access.header_size / kPointerSize + index;
}

}  // namespace

void EscapeAnalysis::ProcessLoadFromPhi(int offset, Node* from, Node* load,
                                        VirtualState* state) {
  TRACE("Load #%d from phi #%d", load->id(), from->id());

  cache_->fields().clear();
  for (int i = 0; i < load->op()->ValueInputCount(); ++i) {
    Node* input = NodeProperties::GetValueInput(load, i);
    cache_->fields().push_back(input);
  }

  cache_->LoadVirtualObjectsForFieldsFrom(state,
                                          status_analysis_->GetAliasMap());
  if (cache_->objects().size() == cache_->fields().size()) {
    cache_->GetFields(offset);
    if (cache_->fields().size() == cache_->objects().size()) {
      Node* rep = replacement(load);
      if (!rep || !IsEquivalentPhi(rep, cache_->fields())) {
        int value_input_count = static_cast<int>(cache_->fields().size());
        Type* phi_type = Type::None();
        for (Node* input : cache_->fields()) {
          Type* input_type = NodeProperties::GetType(input);
          phi_type = Type::Union(phi_type, input_type, graph()->zone());
        }
        cache_->fields().push_back(NodeProperties::GetControlInput(from));
        Node* phi = graph()->NewNode(
            common()->Phi(MachineRepresentation::kTagged, value_input_count),
            value_input_count + 1, &cache_->fields().front());
        NodeProperties::SetType(phi, phi_type);
        status_analysis_->ResizeStatusVector();
        SetReplacement(load, phi);
        TRACE(" got phi created.\n");
      } else {
        TRACE(" has already phi #%d.\n", rep->id());
      }
    } else {
      TRACE(" has incomplete field info.\n");
    }
  } else {
    TRACE(" has incomplete virtual object info.\n");
  }
}

void EscapeAnalysis::ProcessLoadField(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kLoadField);
  ForwardVirtualState(node);
  Node* from = ResolveReplacement(NodeProperties::GetValueInput(node, 0));
  VirtualState* state = virtual_states_[node->id()];
  if (VirtualObject* object = GetVirtualObject(state, from)) {
    if (!object->IsTracked()) return;
    int offset = OffsetForFieldAccess(node);
    if (static_cast<size_t>(offset) >= object->field_count()) {
      // We have a load from a field that is not inside the {object}. This
      // can only happen with conflicting type feedback and for dead {node}s.
      // For now, we just mark the {object} as escaping.
      // TODO(turbofan): Consider introducing an Undefined or None operator
      // that we can replace this load with, since we know it's dead code.
      if (status_analysis_->SetEscaped(from)) {
        TRACE(
            "Setting #%d (%s) to escaped because load field #%d from "
            "offset %d outside of object\n",
            from->id(), from->op()->mnemonic(), node->id(), offset);
      }
      return;
    }
    Node* value = object->GetField(offset);
    if (value) {
      value = ResolveReplacement(value);
    }
    // Record that the load has this alias.
    UpdateReplacement(state, node, value);
  } else if (from->opcode() == IrOpcode::kPhi &&
             IsOffsetForFieldAccessCorrect(FieldAccessOf(node->op()))) {
    int offset = OffsetForFieldAccess(node);
    // Only binary phis are supported for now.
    ProcessLoadFromPhi(offset, from, node, state);
  } else {
    UpdateReplacement(state, node, nullptr);
  }
}

void EscapeAnalysis::ProcessLoadElement(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kLoadElement);
  ForwardVirtualState(node);
  Node* from = ResolveReplacement(NodeProperties::GetValueInput(node, 0));
  VirtualState* state = virtual_states_[node->id()];
  Node* index_node = node->InputAt(1);
  NumberMatcher index(index_node);
  DCHECK(index_node->opcode() != IrOpcode::kInt32Constant &&
         index_node->opcode() != IrOpcode::kInt64Constant &&
         index_node->opcode() != IrOpcode::kFloat32Constant &&
         index_node->opcode() != IrOpcode::kFloat64Constant);
  if (index.HasValue()) {
    if (VirtualObject* object = GetVirtualObject(state, from)) {
      if (!object->IsTracked()) return;
      int offset = OffsetForElementAccess(node, index.Value());
      if (static_cast<size_t>(offset) >= object->field_count()) return;
      Node* value = object->GetField(offset);
      if (value) {
        value = ResolveReplacement(value);
      }
      // Record that the load has this alias.
      UpdateReplacement(state, node, value);
    } else if (from->opcode() == IrOpcode::kPhi) {
      int offset = OffsetForElementAccess(node, index.Value());
      ProcessLoadFromPhi(offset, from, node, state);
    } else {
      UpdateReplacement(state, node, nullptr);
    }
  } else {
    // We have a load from a non-const index, cannot eliminate object.
    if (status_analysis_->SetEscaped(from)) {
      TRACE(
          "Setting #%d (%s) to escaped because load element #%d from non-const "
          "index #%d (%s)\n",
          from->id(), from->op()->mnemonic(), node->id(), index_node->id(),
          index_node->op()->mnemonic());
    }
  }
}

void EscapeAnalysis::ProcessStoreField(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStoreField);
  ForwardVirtualState(node);
  Node* to = ResolveReplacement(NodeProperties::GetValueInput(node, 0));
  VirtualState* state = virtual_states_[node->id()];
  if (VirtualObject* object = GetVirtualObject(state, to)) {
    if (!object->IsTracked()) return;
    int offset = OffsetForFieldAccess(node);
    if (static_cast<size_t>(offset) >= object->field_count()) {
      // We have a store to a field that is not inside the {object}. This
      // can only happen with conflicting type feedback and for dead {node}s.
      // For now, we just mark the {object} as escaping.
      // TODO(turbofan): Consider just eliminating the store in the reducer
      // pass, as it's dead code anyways.
      if (status_analysis_->SetEscaped(to)) {
        TRACE(
            "Setting #%d (%s) to escaped because store field #%d to "
            "offset %d outside of object\n",
            to->id(), to->op()->mnemonic(), node->id(), offset);
      }
      return;
    }
    Node* val = ResolveReplacement(NodeProperties::GetValueInput(node, 1));
    // TODO(mstarzinger): The following is a workaround to not track some well
    // known raw fields. We only ever store default initial values into these
    // fields which are hard-coded in {TranslatedState::MaterializeAt} as well.
    if (val->opcode() == IrOpcode::kInt32Constant ||
        val->opcode() == IrOpcode::kInt64Constant) {
      DCHECK(FieldAccessOf(node->op()).offset == JSFunction::kCodeEntryOffset ||
             FieldAccessOf(node->op()).offset == Name::kHashFieldOffset);
      val = slot_not_analyzed_;
    }
    if (object->GetField(offset) != val) {
      object = CopyForModificationAt(object, state, node);
      object->SetField(offset, val);
    }
  }
}

void EscapeAnalysis::ProcessStoreElement(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStoreElement);
  ForwardVirtualState(node);
  Node* to = ResolveReplacement(NodeProperties::GetValueInput(node, 0));
  Node* index_node = node->InputAt(1);
  NumberMatcher index(index_node);
  DCHECK(index_node->opcode() != IrOpcode::kInt32Constant &&
         index_node->opcode() != IrOpcode::kInt64Constant &&
         index_node->opcode() != IrOpcode::kFloat32Constant &&
         index_node->opcode() != IrOpcode::kFloat64Constant);
  VirtualState* state = virtual_states_[node->id()];
  if (index.HasValue()) {
    if (VirtualObject* object = GetVirtualObject(state, to)) {
      if (!object->IsTracked()) return;
      int offset = OffsetForElementAccess(node, index.Value());
      if (static_cast<size_t>(offset) >= object->field_count()) return;
      Node* val = ResolveReplacement(NodeProperties::GetValueInput(node, 2));
      if (object->GetField(offset) != val) {
        object = CopyForModificationAt(object, state, node);
        object->SetField(offset, val);
      }
    }
  } else {
    // We have a store to a non-const index, cannot eliminate object.
    if (status_analysis_->SetEscaped(to)) {
      TRACE(
          "Setting #%d (%s) to escaped because store element #%d to non-const "
          "index #%d (%s)\n",
          to->id(), to->op()->mnemonic(), node->id(), index_node->id(),
          index_node->op()->mnemonic());
    }
    if (VirtualObject* object = GetVirtualObject(state, to)) {
      if (!object->IsTracked()) return;
      if (!object->AllFieldsClear()) {
        object = CopyForModificationAt(object, state, node);
        object->ClearAllFields();
        TRACE("Cleared all fields of @%d:#%d\n",
              status_analysis_->GetAlias(object->id()), object->id());
      }
    }
  }
}

Node* EscapeAnalysis::GetOrCreateObjectState(Node* effect, Node* node) {
  if ((node->opcode() == IrOpcode::kFinishRegion ||
       node->opcode() == IrOpcode::kAllocate) &&
      IsVirtual(node)) {
    if (VirtualObject* vobj = GetVirtualObject(virtual_states_[effect->id()],
                                               ResolveReplacement(node))) {
      if (Node* object_state = vobj->GetObjectState()) {
        return object_state;
      } else {
        cache_->fields().clear();
        for (size_t i = 0; i < vobj->field_count(); ++i) {
          if (Node* field = vobj->GetField(i)) {
            cache_->fields().push_back(ResolveReplacement(field));
          }
        }
        int input_count = static_cast<int>(cache_->fields().size());
        Node* new_object_state =
            graph()->NewNode(common()->ObjectState(input_count), input_count,
                             &cache_->fields().front());
        NodeProperties::SetType(new_object_state, Type::OtherInternal());
        vobj->SetObjectState(new_object_state);
        TRACE(
            "Creating object state #%d for vobj %p (from node #%d) at effect "
            "#%d\n",
            new_object_state->id(), static_cast<void*>(vobj), node->id(),
            effect->id());
        // Now fix uses of other objects.
        for (size_t i = 0; i < vobj->field_count(); ++i) {
          if (Node* field = vobj->GetField(i)) {
            if (Node* field_object_state =
                    GetOrCreateObjectState(effect, field)) {
              NodeProperties::ReplaceValueInput(
                  new_object_state, field_object_state, static_cast<int>(i));
            }
          }
        }
        return new_object_state;
      }
    }
  }
  return nullptr;
}

bool EscapeAnalysis::IsCyclicObjectState(Node* effect, Node* node) {
  if ((node->opcode() == IrOpcode::kFinishRegion ||
       node->opcode() == IrOpcode::kAllocate) &&
      IsVirtual(node)) {
    if (VirtualObject* vobj = GetVirtualObject(virtual_states_[effect->id()],
                                               ResolveReplacement(node))) {
      if (cycle_detection_.find(vobj) != cycle_detection_.end()) return true;
      cycle_detection_.insert(vobj);
      bool cycle_detected = false;
      for (size_t i = 0; i < vobj->field_count(); ++i) {
        if (Node* field = vobj->GetField(i)) {
          if (IsCyclicObjectState(effect, field)) cycle_detected = true;
        }
      }
      cycle_detection_.erase(vobj);
      return cycle_detected;
    }
  }
  return false;
}

void EscapeAnalysis::DebugPrintState(VirtualState* state) {
  PrintF("Dumping virtual state %p\n", static_cast<void*>(state));
  for (Alias alias = 0; alias < status_analysis_->AliasCount(); ++alias) {
    if (VirtualObject* object = state->VirtualObjectFromAlias(alias)) {
      PrintF("  Alias @%d: Object #%d with %zu fields\n", alias, object->id(),
             object->field_count());
      for (size_t i = 0; i < object->field_count(); ++i) {
        if (Node* f = object->GetField(i)) {
          PrintF("    Field %zu = #%d (%s)\n", i, f->id(), f->op()->mnemonic());
        }
      }
    }
  }
}

void EscapeAnalysis::DebugPrint() {
  ZoneVector<VirtualState*> object_states(zone());
  for (NodeId id = 0; id < virtual_states_.size(); id++) {
    if (VirtualState* states = virtual_states_[id]) {
      if (std::find(object_states.begin(), object_states.end(), states) ==
          object_states.end()) {
        object_states.push_back(states);
      }
    }
  }
  for (size_t n = 0; n < object_states.size(); n++) {
    DebugPrintState(object_states[n]);
  }
}

VirtualObject* EscapeAnalysis::GetVirtualObject(VirtualState* state,
                                                Node* node) {
  if (node->id() >= status_analysis_->GetAliasMap().size()) return nullptr;
  Alias alias = status_analysis_->GetAlias(node->id());
  if (alias >= state->size()) return nullptr;
  return state->VirtualObjectFromAlias(alias);
}

bool EscapeAnalysis::ExistsVirtualAllocate() {
  for (size_t id = 0; id < status_analysis_->GetAliasMap().size(); ++id) {
    Alias alias = status_analysis_->GetAlias(static_cast<NodeId>(id));
    if (alias < EscapeStatusAnalysis::kUntrackable) {
      if (status_analysis_->IsVirtual(static_cast<int>(id))) {
        return true;
      }
    }
  }
  return false;
}

Graph* EscapeAnalysis::graph() const { return status_analysis_->graph(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8

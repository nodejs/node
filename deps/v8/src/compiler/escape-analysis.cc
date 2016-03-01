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
#include "src/compiler/node.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects-inl.h"
#include "src/type-cache.h"

namespace v8 {
namespace internal {
namespace compiler {

const EscapeAnalysis::Alias EscapeAnalysis::kNotReachable =
    std::numeric_limits<Alias>::max();
const EscapeAnalysis::Alias EscapeAnalysis::kUntrackable =
    std::numeric_limits<Alias>::max() - 1;


class VirtualObject : public ZoneObject {
 public:
  enum Status { kUntracked = 0, kTracked = 1 };
  VirtualObject(NodeId id, Zone* zone)
      : id_(id),
        status_(kUntracked),
        fields_(zone),
        phi_(zone),
        object_state_(nullptr) {}

  VirtualObject(const VirtualObject& other)
      : id_(other.id_),
        status_(other.status_),
        fields_(other.fields_),
        phi_(other.phi_),
        object_state_(other.object_state_) {}

  VirtualObject(NodeId id, Zone* zone, size_t field_number)
      : id_(id),
        status_(kTracked),
        fields_(zone),
        phi_(zone),
        object_state_(nullptr) {
    fields_.resize(field_number);
    phi_.resize(field_number, false);
  }

  Node* GetField(size_t offset) {
    if (offset < fields_.size()) {
      return fields_[offset];
    }
    return nullptr;
  }

  bool IsCreatedPhi(size_t offset) {
    if (offset < phi_.size()) {
      return phi_[offset];
    }
    return false;
  }

  bool SetField(size_t offset, Node* node, bool created_phi = false) {
    bool changed = fields_[offset] != node || phi_[offset] != created_phi;
    fields_[offset] = node;
    phi_[offset] = created_phi;
    if (changed && FLAG_trace_turbo_escape && node) {
      PrintF("Setting field %zu of #%d to #%d (%s)\n", offset, id(), node->id(),
             node->op()->mnemonic());
    }
    return changed;
  }
  bool IsVirtual() const { return status_ == kTracked; }
  bool IsTracked() const { return status_ != kUntracked; }

  Node** fields_array() { return &fields_.front(); }
  size_t field_count() { return fields_.size(); }
  bool ResizeFields(size_t field_count) {
    if (field_count != fields_.size()) {
      fields_.resize(field_count);
      phi_.resize(field_count);
      return true;
    }
    return false;
  }
  bool ClearAllFields() {
    bool changed = false;
    for (size_t i = 0; i < fields_.size(); ++i) {
      if (fields_[i] != nullptr) {
        fields_[i] = nullptr;
        changed = true;
      }
      phi_[i] = false;
    }
    return changed;
  }
  bool UpdateFrom(const VirtualObject& other);
  void SetObjectState(Node* node) { object_state_ = node; }
  Node* GetObjectState() const { return object_state_; }

  NodeId id() const { return id_; }
  void id(NodeId id) { id_ = id; }

 private:
  NodeId id_;
  Status status_;
  ZoneVector<Node*> fields_;
  ZoneVector<bool> phi_;
  Node* object_state_;
};


bool VirtualObject::UpdateFrom(const VirtualObject& other) {
  bool changed = status_ != other.status_;
  status_ = other.status_;
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
  VirtualState(Zone* zone, size_t size);
  VirtualState(const VirtualState& states);

  VirtualObject* VirtualObjectFromAlias(size_t alias);
  VirtualObject* GetOrCreateTrackedVirtualObject(EscapeAnalysis::Alias alias,
                                                 NodeId id, Zone* zone);
  void SetVirtualObject(EscapeAnalysis::Alias alias, VirtualObject* state);
  void LastChangedAt(Node* node) { last_changed_ = node; }
  Node* GetLastChanged() { return last_changed_; }
  bool UpdateFrom(VirtualState* state, Zone* zone);
  bool MergeFrom(MergeCache* cache, Zone* zone, Graph* graph,
                 CommonOperatorBuilder* common, Node* control);
  size_t size() const { return info_.size(); }

 private:
  ZoneVector<VirtualObject*> info_;
  Node* last_changed_;
};


class MergeCache : public ZoneObject {
 public:
  explicit MergeCache(Zone* zone)
      : states_(zone), objects_(zone), fields_(zone) {
    states_.reserve(4);
    objects_.reserve(4);
    fields_.reserve(4);
  }
  ZoneVector<VirtualState*>& states() { return states_; }
  ZoneVector<VirtualObject*>& objects() { return objects_; }
  ZoneVector<Node*>& fields() { return fields_; }
  void Clear() {
    states_.clear();
    objects_.clear();
    fields_.clear();
  }
  size_t LoadVirtualObjectsFromStatesFor(EscapeAnalysis::Alias alias);
  void LoadVirtualObjectsForFieldsFrom(
      VirtualState* state, const ZoneVector<EscapeAnalysis::Alias>& aliases);
  Node* GetFields(size_t pos);

 private:
  ZoneVector<VirtualState*> states_;
  ZoneVector<VirtualObject*> objects_;
  ZoneVector<Node*> fields_;
};


size_t MergeCache::LoadVirtualObjectsFromStatesFor(
    EscapeAnalysis::Alias alias) {
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
    VirtualState* state, const ZoneVector<EscapeAnalysis::Alias>& aliases) {
  objects_.clear();
  size_t max_alias = state->size();
  for (Node* field : fields_) {
    EscapeAnalysis::Alias alias = aliases[field->id()];
    if (alias >= max_alias) continue;
    if (VirtualObject* obj = state->VirtualObjectFromAlias(alias)) {
      objects_.push_back(obj);
    }
  }
}


Node* MergeCache::GetFields(size_t pos) {
  fields_.clear();
  Node* rep = objects_.front()->GetField(pos);
  for (VirtualObject* obj : objects_) {
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


VirtualState::VirtualState(Zone* zone, size_t size)
    : info_(size, nullptr, zone), last_changed_(nullptr) {}


VirtualState::VirtualState(const VirtualState& state)
    : info_(state.info_.size(), nullptr, state.info_.get_allocator().zone()),
      last_changed_(state.last_changed_) {
  for (size_t i = 0; i < state.info_.size(); ++i) {
    if (state.info_[i]) {
      info_[i] =
          new (info_.get_allocator().zone()) VirtualObject(*state.info_[i]);
    }
  }
}


VirtualObject* VirtualState::VirtualObjectFromAlias(size_t alias) {
  return info_[alias];
}


VirtualObject* VirtualState::GetOrCreateTrackedVirtualObject(
    EscapeAnalysis::Alias alias, NodeId id, Zone* zone) {
  if (VirtualObject* obj = VirtualObjectFromAlias(alias)) {
    return obj;
  }
  VirtualObject* obj = new (zone) VirtualObject(id, zone, 0);
  SetVirtualObject(alias, obj);
  return obj;
}


void VirtualState::SetVirtualObject(EscapeAnalysis::Alias alias,
                                    VirtualObject* obj) {
  info_[alias] = obj;
}


bool VirtualState::UpdateFrom(VirtualState* from, Zone* zone) {
  bool changed = false;
  for (EscapeAnalysis::Alias alias = 0; alias < size(); ++alias) {
    VirtualObject* ls = VirtualObjectFromAlias(alias);
    VirtualObject* rs = from->VirtualObjectFromAlias(alias);

    if (rs == nullptr) {
      continue;
    }

    if (ls == nullptr) {
      ls = new (zone) VirtualObject(*rs);
      SetVirtualObject(alias, ls);
      changed = true;
      continue;
    }

    if (FLAG_trace_turbo_escape) {
      PrintF("  Updating fields of @%d\n", alias);
    }

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
  if (phi->op()->ValueInputCount() != inputs.size()) {
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


Node* EscapeAnalysis::GetReplacementIfSame(ZoneVector<VirtualObject*>& objs) {
  Node* rep = GetReplacement(objs.front()->id());
  for (VirtualObject* obj : objs) {
    if (GetReplacement(obj->id()) != rep) {
      return nullptr;
    }
  }
  return rep;
}


bool VirtualState::MergeFrom(MergeCache* cache, Zone* zone, Graph* graph,
                             CommonOperatorBuilder* common, Node* control) {
  DCHECK_GT(cache->states().size(), 0u);
  bool changed = false;
  for (EscapeAnalysis::Alias alias = 0; alias < size(); ++alias) {
    size_t fields = cache->LoadVirtualObjectsFromStatesFor(alias);
    if (cache->objects().size() == cache->states().size()) {
      if (FLAG_trace_turbo_escape) {
        PrintF("  Merging virtual objects of @%d\n", alias);
      }
      VirtualObject* mergeObject = GetOrCreateTrackedVirtualObject(
          alias, cache->objects().front()->id(), zone);
      changed = mergeObject->ResizeFields(fields) || changed;
      for (size_t i = 0; i < fields; ++i) {
        if (Node* field = cache->GetFields(i)) {
          changed = mergeObject->SetField(i, field) || changed;
          if (FLAG_trace_turbo_escape) {
            PrintF("    Field %zu agree on rep #%d\n", i, field->id());
          }
        } else {
          int value_input_count = static_cast<int>(cache->fields().size());
          if (cache->fields().size() == cache->objects().size()) {
            Node* rep = mergeObject->GetField(i);
            if (!rep || !mergeObject->IsCreatedPhi(i)) {
              cache->fields().push_back(control);
              Node* phi = graph->NewNode(
                  common->Phi(MachineRepresentation::kTagged,
                              value_input_count),
                  value_input_count + 1, &cache->fields().front());
              mergeObject->SetField(i, phi, true);
              if (FLAG_trace_turbo_escape) {
                PrintF("    Creating Phi #%d as merge of", phi->id());
                for (int i = 0; i < value_input_count; i++) {
                  PrintF(" #%d (%s)", cache->fields()[i]->id(),
                         cache->fields()[i]->op()->mnemonic());
                }
                PrintF("\n");
              }
              changed = true;
            } else {
              DCHECK(rep->opcode() == IrOpcode::kPhi);
              for (int n = 0; n < value_input_count; ++n) {
                if (n < rep->op()->ValueInputCount()) {
                  Node* old = NodeProperties::GetValueInput(rep, n);
                  if (old != cache->fields()[n]) {
                    changed = true;
                    NodeProperties::ReplaceValueInput(rep, cache->fields()[n],
                                                      n);
                  }
                } else {
                  changed = true;
                  rep->InsertInput(graph->zone(), n, cache->fields()[n]);
                }
              }
              if (rep->op()->ValueInputCount() != value_input_count) {
                if (FLAG_trace_turbo_escape) {
                  PrintF("    Widening Phi #%d of arity %d to %d", rep->id(),
                         rep->op()->ValueInputCount(), value_input_count);
                }
                NodeProperties::ChangeOp(
                    rep, common->Phi(MachineRepresentation::kTagged,
                                     value_input_count));
              }
            }
          } else {
            changed = mergeObject->SetField(i, nullptr) || changed;
          }
        }
      }
    } else {
      SetVirtualObject(alias, nullptr);
    }
  }
  return changed;
}


EscapeStatusAnalysis::EscapeStatusAnalysis(EscapeAnalysis* object_analysis,
                                           Graph* graph, Zone* zone)
    : object_analysis_(object_analysis),
      graph_(graph),
      zone_(zone),
      status_(graph->NodeCount(), kUnknown, zone),
      queue_(zone) {}


EscapeStatusAnalysis::~EscapeStatusAnalysis() {}


bool EscapeStatusAnalysis::HasEntry(Node* node) {
  return status_[node->id()] & (kTracked | kEscaped);
}


bool EscapeStatusAnalysis::IsVirtual(Node* node) {
  return (status_[node->id()] & kTracked) && !(status_[node->id()] & kEscaped);
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


void EscapeStatusAnalysis::Resize() {
  status_.resize(graph()->NodeCount(), kUnknown);
}


size_t EscapeStatusAnalysis::size() { return status_.size(); }


void EscapeStatusAnalysis::Run() {
  Resize();
  queue_.push_back(graph()->end());
  status_[graph()->end()->id()] |= kOnStack;
  while (!queue_.empty()) {
    Node* node = queue_.front();
    queue_.pop_front();
    status_[node->id()] &= ~kOnStack;
    Process(node);
    status_[node->id()] |= kVisited;
    for (Edge edge : node->input_edges()) {
      Node* input = edge.to();
      if (!(status_[input->id()] & (kVisited | kOnStack))) {
        queue_.push_back(input);
        status_[input->id()] |= kOnStack;
      }
    }
  }
}


void EscapeStatusAnalysis::RevisitInputs(Node* node) {
  for (Edge edge : node->input_edges()) {
    Node* input = edge.to();
    if (!(status_[input->id()] & kOnStack)) {
      queue_.push_back(input);
      status_[input->id()] |= kOnStack;
    }
  }
}


void EscapeStatusAnalysis::RevisitUses(Node* node) {
  for (Edge edge : node->use_edges()) {
    Node* use = edge.from();
    if (!(status_[use->id()] & kOnStack)) {
      queue_.push_back(use);
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
      break;
    }
    case IrOpcode::kPhi:
      if (!HasEntry(node)) {
        status_[node->id()] |= kTracked;
        if (!IsAllocationPhi(node)) {
          SetEscaped(node);
          RevisitUses(node);
        }
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
    if (FLAG_trace_turbo_escape) {
      PrintF("Setting #%d (%s) to escaped because of store to field of #%d\n",
             val->id(), val->op()->mnemonic(), to->id());
    }
  }
}


void EscapeStatusAnalysis::ProcessStoreElement(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStoreElement);
  Node* to = NodeProperties::GetValueInput(node, 0);
  Node* val = NodeProperties::GetValueInput(node, 2);
  if ((IsEscaped(to) || !IsAllocation(to)) && SetEscaped(val)) {
    RevisitUses(val);
    RevisitInputs(val);
    if (FLAG_trace_turbo_escape) {
      PrintF("Setting #%d (%s) to escaped because of store to field of #%d\n",
             val->id(), val->op()->mnemonic(), to->id());
    }
  }
}


void EscapeStatusAnalysis::ProcessAllocate(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocate);
  if (!HasEntry(node)) {
    status_[node->id()] |= kTracked;
    if (FLAG_trace_turbo_escape) {
      PrintF("Created status entry for node #%d (%s)\n", node->id(),
             node->op()->mnemonic());
    }
    NumberMatcher size(node->InputAt(0));
    DCHECK(node->InputAt(0)->opcode() != IrOpcode::kInt32Constant &&
           node->InputAt(0)->opcode() != IrOpcode::kInt64Constant &&
           node->InputAt(0)->opcode() != IrOpcode::kFloat32Constant &&
           node->InputAt(0)->opcode() != IrOpcode::kFloat64Constant);
    if (!size.HasValue() && SetEscaped(node)) {
      RevisitUses(node);
      if (FLAG_trace_turbo_escape) {
        PrintF("Setting #%d to escaped because of non-const alloc\n",
               node->id());
      }
      // This node is known to escape, uses do not have to be checked.
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
    if (edge.index() >= use->op()->ValueInputCount() +
                            OperatorProperties::GetContextInputCount(use->op()))
      continue;
    switch (use->opcode()) {
      case IrOpcode::kPhi:
        if (phi_escaping && SetEscaped(rep)) {
          if (FLAG_trace_turbo_escape) {
            PrintF(
                "Setting #%d (%s) to escaped because of use by phi node "
                "#%d (%s)\n",
                rep->id(), rep->op()->mnemonic(), use->id(),
                use->op()->mnemonic());
          }
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
          if (FLAG_trace_turbo_escape) {
            PrintF(
                "Setting #%d (%s) to escaped because of use by escaping node "
                "#%d (%s)\n",
                rep->id(), rep->op()->mnemonic(), use->id(),
                use->op()->mnemonic());
          }
          return true;
        }
        break;
      case IrOpcode::kObjectIsSmi:
        if (!IsAllocation(rep) && SetEscaped(rep)) {
          PrintF("Setting #%d (%s) to escaped because of use by #%d (%s)\n",
                 rep->id(), rep->op()->mnemonic(), use->id(),
                 use->op()->mnemonic());
          return true;
        }
        break;
      default:
        if (use->op()->EffectInputCount() == 0 &&
            uses->op()->EffectInputCount() > 0) {
          PrintF("Encountered unaccounted use by #%d (%s)\n", use->id(),
                 use->op()->mnemonic());
          UNREACHABLE();
        }
        if (SetEscaped(rep)) {
          if (FLAG_trace_turbo_escape) {
            PrintF("Setting #%d (%s) to escaped because of use by #%d (%s)\n",
                   rep->id(), rep->op()->mnemonic(), use->id(),
                   use->op()->mnemonic());
          }
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
    : graph_(graph),
      common_(common),
      zone_(zone),
      virtual_states_(zone),
      replacements_(zone),
      escape_status_(this, graph, zone),
      cache_(new (zone) MergeCache(zone)),
      aliases_(zone),
      next_free_alias_(0) {}


EscapeAnalysis::~EscapeAnalysis() {}


void EscapeAnalysis::Run() {
  replacements_.resize(graph()->NodeCount());
  AssignAliases();
  RunObjectAnalysis();
  escape_status_.Run();
}


void EscapeAnalysis::AssignAliases() {
  ZoneVector<Node*> stack(zone());
  stack.push_back(graph()->end());
  CHECK_LT(graph()->NodeCount(), kUntrackable);
  aliases_.resize(graph()->NodeCount(), kNotReachable);
  aliases_[graph()->end()->id()] = kUntrackable;
  while (!stack.empty()) {
    Node* node = stack.back();
    stack.pop_back();
    switch (node->opcode()) {
      case IrOpcode::kAllocate:
        if (aliases_[node->id()] >= kUntrackable) {
          aliases_[node->id()] = NextAlias();
        }
        break;
      case IrOpcode::kFinishRegion: {
        Node* allocate = NodeProperties::GetValueInput(node, 0);
        if (allocate->opcode() == IrOpcode::kAllocate) {
          if (aliases_[allocate->id()] >= kUntrackable) {
            if (aliases_[allocate->id()] == kNotReachable) {
              stack.push_back(allocate);
            }
            aliases_[allocate->id()] = NextAlias();
          }
          aliases_[node->id()] = aliases_[allocate->id()];
        } else {
          aliases_[node->id()] = NextAlias();
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
        stack.push_back(input);
        aliases_[input->id()] = kUntrackable;
      }
    }
  }

  if (FLAG_trace_turbo_escape) {
    PrintF("Discovered trackable nodes");
    for (EscapeAnalysis::Alias id = 0; id < graph()->NodeCount(); ++id) {
      if (aliases_[id] < kUntrackable) {
        if (FLAG_trace_turbo_escape) {
          PrintF(" #%u", id);
        }
      }
    }
    PrintF("\n");
  }
}


void EscapeAnalysis::RunObjectAnalysis() {
  virtual_states_.resize(graph()->NodeCount());
  ZoneVector<Node*> stack(zone());
  stack.push_back(graph()->start());
  while (!stack.empty()) {
    Node* node = stack.back();
    stack.pop_back();
    if (aliases_[node->id()] != kNotReachable && Process(node)) {
      for (Edge edge : node->use_edges()) {
        if (NodeProperties::IsEffectEdge(edge)) {
          Node* use = edge.from();
          if ((use->opcode() != IrOpcode::kLoadField &&
               use->opcode() != IrOpcode::kLoadElement) ||
              !IsDanglingEffectNode(use)) {
            stack.push_back(use);
          }
        }
      }
      // First process loads: dangling loads are a problem otherwise.
      for (Edge edge : node->use_edges()) {
        if (NodeProperties::IsEffectEdge(edge)) {
          Node* use = edge.from();
          if ((use->opcode() == IrOpcode::kLoadField ||
               use->opcode() == IrOpcode::kLoadElement) &&
              IsDanglingEffectNode(use)) {
            stack.push_back(use);
          }
        }
      }
    }
  }
  if (FLAG_trace_turbo_escape) {
    DebugPrint();
  }
}


bool EscapeAnalysis::IsDanglingEffectNode(Node* node) {
  if (node->op()->EffectInputCount() == 0) return false;
  if (node->op()->EffectOutputCount() == 0) return false;
  if (node->op()->EffectInputCount() == 1 &&
      NodeProperties::GetEffectInput(node)->opcode() == IrOpcode::kStart) {
    // The start node is used as sentinel for nodes that are in general
    // effectful, but of which an analysis has determined that they do not
    // produce effects in this instance. We don't consider these nodes dangling.
    return false;
  }
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsEffectEdge(edge)) {
      return false;
    }
  }
  return true;
}


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
  return true;
}


void EscapeAnalysis::ProcessAllocationUsers(Node* node) {
  for (Edge edge : node->input_edges()) {
    Node* input = edge.to();
    if (!NodeProperties::IsValueEdge(edge) &&
        !NodeProperties::IsContextEdge(edge))
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
      case IrOpcode::kPhi:
        break;
      default:
        VirtualState* state = virtual_states_[node->id()];
        if (VirtualObject* obj = ResolveVirtualObject(state, input)) {
          if (obj->ClearAllFields()) {
            state->LastChangedAt(node);
          }
        }
        break;
    }
  }
}


bool EscapeAnalysis::IsEffectBranchPoint(Node* node) {
  int count = 0;
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsEffectEdge(edge)) {
      if (++count > 1) {
        return true;
      }
    }
  }
  return false;
}


void EscapeAnalysis::ForwardVirtualState(Node* node) {
  DCHECK_EQ(node->op()->EffectInputCount(), 1);
  if (node->opcode() != IrOpcode::kLoadField &&
      node->opcode() != IrOpcode::kLoadElement &&
      node->opcode() != IrOpcode::kLoad && IsDanglingEffectNode(node)) {
    PrintF("Dangeling effect node: #%d (%s)\n", node->id(),
           node->op()->mnemonic());
    UNREACHABLE();
  }
  Node* effect = NodeProperties::GetEffectInput(node);
  // Break the cycle for effect phis.
  if (effect->opcode() == IrOpcode::kEffectPhi) {
    if (virtual_states_[effect->id()] == nullptr) {
      virtual_states_[effect->id()] =
          new (zone()) VirtualState(zone(), AliasCount());
    }
  }
  DCHECK_NOT_NULL(virtual_states_[effect->id()]);
  if (IsEffectBranchPoint(effect)) {
    if (FLAG_trace_turbo_escape) {
      PrintF("Copying object state %p from #%d (%s) to #%d (%s)\n",
             static_cast<void*>(virtual_states_[effect->id()]), effect->id(),
             effect->op()->mnemonic(), node->id(), node->op()->mnemonic());
    }
    if (!virtual_states_[node->id()]) {
      virtual_states_[node->id()] =
          new (zone()) VirtualState(*virtual_states_[effect->id()]);
    } else {
      virtual_states_[node->id()]->UpdateFrom(virtual_states_[effect->id()],
                                              zone());
    }
  } else {
    virtual_states_[node->id()] = virtual_states_[effect->id()];
    if (FLAG_trace_turbo_escape) {
      PrintF("Forwarding object state %p from #%d (%s) to #%d (%s)\n",
             static_cast<void*>(virtual_states_[effect->id()]), effect->id(),
             effect->op()->mnemonic(), node->id(), node->op()->mnemonic());
    }
  }
}


void EscapeAnalysis::ProcessStart(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStart);
  virtual_states_[node->id()] = new (zone()) VirtualState(zone(), AliasCount());
}


bool EscapeAnalysis::ProcessEffectPhi(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kEffectPhi);
  bool changed = false;

  VirtualState* mergeState = virtual_states_[node->id()];
  if (!mergeState) {
    mergeState = new (zone()) VirtualState(zone(), AliasCount());
    virtual_states_[node->id()] = mergeState;
    changed = true;
    if (FLAG_trace_turbo_escape) {
      PrintF("Effect Phi #%d got new states map %p.\n", node->id(),
             static_cast<void*>(mergeState));
    }
  } else if (mergeState->GetLastChanged() != node) {
    changed = true;
  }

  cache_->Clear();

  if (FLAG_trace_turbo_escape) {
    PrintF("At Effect Phi #%d, merging states into %p:", node->id(),
           static_cast<void*>(mergeState));
  }

  for (int i = 0; i < node->op()->EffectInputCount(); ++i) {
    Node* input = NodeProperties::GetEffectInput(node, i);
    VirtualState* state = virtual_states_[input->id()];
    if (state) {
      cache_->states().push_back(state);
    }
    if (FLAG_trace_turbo_escape) {
      PrintF(" %p (from %d %s)", static_cast<void*>(state), input->id(),
             input->op()->mnemonic());
    }
  }
  if (FLAG_trace_turbo_escape) {
    PrintF("\n");
  }

  if (cache_->states().size() == 0) {
    return changed;
  }

  changed = mergeState->MergeFrom(cache_, zone(), graph(), common(),
                                  NodeProperties::GetControlInput(node)) ||
            changed;

  if (FLAG_trace_turbo_escape) {
    PrintF("Merge %s the node.\n", changed ? "changed" : "did not change");
  }

  if (changed) {
    mergeState->LastChangedAt(node);
    escape_status_.Resize();
  }
  return changed;
}


void EscapeAnalysis::ProcessAllocation(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kAllocate);
  ForwardVirtualState(node);

  // Check if we have already processed this node.
  if (virtual_states_[node->id()]->VirtualObjectFromAlias(
          aliases_[node->id()])) {
    return;
  }

  NumberMatcher size(node->InputAt(0));
  DCHECK(node->InputAt(0)->opcode() != IrOpcode::kInt32Constant &&
         node->InputAt(0)->opcode() != IrOpcode::kInt64Constant &&
         node->InputAt(0)->opcode() != IrOpcode::kFloat32Constant &&
         node->InputAt(0)->opcode() != IrOpcode::kFloat64Constant);
  if (size.HasValue()) {
    virtual_states_[node->id()]->SetVirtualObject(
        aliases_[node->id()],
        new (zone())
            VirtualObject(node->id(), zone(), size.Value() / kPointerSize));
  } else {
    virtual_states_[node->id()]->SetVirtualObject(
        aliases_[node->id()], new (zone()) VirtualObject(node->id(), zone()));
  }
  virtual_states_[node->id()]->LastChangedAt(node);
}


void EscapeAnalysis::ProcessFinishRegion(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kFinishRegion);
  ForwardVirtualState(node);
  Node* allocation = NodeProperties::GetValueInput(node, 0);
  if (allocation->opcode() == IrOpcode::kAllocate) {
    VirtualState* state = virtual_states_[node->id()];
    if (!state->VirtualObjectFromAlias(aliases_[node->id()])) {
      VirtualObject* vobj_alloc =
          state->VirtualObjectFromAlias(aliases_[allocation->id()]);
      DCHECK_NOT_NULL(vobj_alloc);
      state->SetVirtualObject(aliases_[node->id()], vobj_alloc);
      if (FLAG_trace_turbo_escape) {
        PrintF("Linked finish region node #%d to node #%d\n", node->id(),
               allocation->id());
      }
      state->LastChangedAt(node);
    }
  }
}


Node* EscapeAnalysis::replacement(NodeId id) {
  if (id >= replacements_.size()) return nullptr;
  return replacements_[id];
}


Node* EscapeAnalysis::replacement(Node* node) {
  return replacement(node->id());
}


bool EscapeAnalysis::SetReplacement(Node* node, Node* rep) {
  bool changed = replacements_[node->id()] != rep;
  replacements_[node->id()] = rep;
  return changed;
}


bool EscapeAnalysis::UpdateReplacement(VirtualState* state, Node* node,
                                       Node* rep) {
  if (SetReplacement(node, rep)) {
    state->LastChangedAt(node);
    if (FLAG_trace_turbo_escape) {
      if (rep) {
        PrintF("Replacement of #%d is #%d (%s)\n", node->id(), rep->id(),
               rep->op()->mnemonic());
      } else {
        PrintF("Replacement of #%d cleared\n", node->id());
      }
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
  return GetReplacement(node->id());
}


Node* EscapeAnalysis::GetReplacement(NodeId id) {
  Node* node = nullptr;
  while (replacement(id)) {
    node = replacement(id);
    id = node->id();
  }
  return node;
}


bool EscapeAnalysis::IsVirtual(Node* node) {
  if (node->id() >= escape_status_.size()) {
    return false;
  }
  return escape_status_.IsVirtual(node);
}


bool EscapeAnalysis::IsEscaped(Node* node) {
  if (node->id() >= escape_status_.size()) {
    return false;
  }
  return escape_status_.IsEscaped(node);
}


bool EscapeAnalysis::SetEscaped(Node* node) {
  return escape_status_.SetEscaped(node);
}


VirtualObject* EscapeAnalysis::GetVirtualObject(Node* at, NodeId id) {
  if (VirtualState* states = virtual_states_[at->id()]) {
    return states->VirtualObjectFromAlias(aliases_[id]);
  }
  return nullptr;
}


VirtualObject* EscapeAnalysis::ResolveVirtualObject(VirtualState* state,
                                                    Node* node) {
  VirtualObject* obj = GetVirtualObject(state, ResolveReplacement(node));
  while (obj && replacement(obj->id())) {
    if (VirtualObject* next = GetVirtualObject(state, replacement(obj->id()))) {
      obj = next;
    } else {
      break;
    }
  }
  return obj;
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


int EscapeAnalysis::OffsetFromAccess(Node* node) {
  DCHECK(OpParameter<FieldAccess>(node).offset % kPointerSize == 0);
  return OpParameter<FieldAccess>(node).offset / kPointerSize;
}


void EscapeAnalysis::ProcessLoadFromPhi(int offset, Node* from, Node* node,
                                        VirtualState* state) {
  if (FLAG_trace_turbo_escape) {
    PrintF("Load #%d from phi #%d", node->id(), from->id());
  }

  cache_->fields().clear();
  for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
    Node* input = NodeProperties::GetValueInput(node, i);
    cache_->fields().push_back(input);
  }

  cache_->LoadVirtualObjectsForFieldsFrom(state, aliases_);
  if (cache_->objects().size() == cache_->fields().size()) {
    cache_->GetFields(offset);
    if (cache_->fields().size() == cache_->objects().size()) {
      Node* rep = replacement(node);
      if (!rep || !IsEquivalentPhi(rep, cache_->fields())) {
        int value_input_count = static_cast<int>(cache_->fields().size());
        cache_->fields().push_back(NodeProperties::GetControlInput(from));
        Node* phi = graph()->NewNode(
            common()->Phi(MachineRepresentation::kTagged, value_input_count),
            value_input_count + 1, &cache_->fields().front());
        escape_status_.Resize();
        SetReplacement(node, phi);
        state->LastChangedAt(node);
        if (FLAG_trace_turbo_escape) {
          PrintF(" got phi created.\n");
        }
      } else if (FLAG_trace_turbo_escape) {
        PrintF(" has already phi #%d.\n", rep->id());
      }
    } else if (FLAG_trace_turbo_escape) {
      PrintF(" has incomplete field info.\n");
    }
  } else if (FLAG_trace_turbo_escape) {
    PrintF(" has incomplete virtual object info.\n");
  }
}


void EscapeAnalysis::ProcessLoadField(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kLoadField);
  ForwardVirtualState(node);
  Node* from = NodeProperties::GetValueInput(node, 0);
  VirtualState* state = virtual_states_[node->id()];
  if (VirtualObject* object = ResolveVirtualObject(state, from)) {
    int offset = OffsetFromAccess(node);
    if (!object->IsTracked()) return;
    Node* value = object->GetField(offset);
    if (value) {
      value = ResolveReplacement(value);
    }
    // Record that the load has this alias.
    UpdateReplacement(state, node, value);
  } else {
    if (from->opcode() == IrOpcode::kPhi &&
        OpParameter<FieldAccess>(node).offset % kPointerSize == 0) {
      int offset = OffsetFromAccess(node);
      // Only binary phis are supported for now.
      ProcessLoadFromPhi(offset, from, node, state);
    }
  }
}


void EscapeAnalysis::ProcessLoadElement(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kLoadElement);
  ForwardVirtualState(node);
  Node* from = NodeProperties::GetValueInput(node, 0);
  VirtualState* state = virtual_states_[node->id()];
  Node* index_node = node->InputAt(1);
  NumberMatcher index(index_node);
  DCHECK(index_node->opcode() != IrOpcode::kInt32Constant &&
         index_node->opcode() != IrOpcode::kInt64Constant &&
         index_node->opcode() != IrOpcode::kFloat32Constant &&
         index_node->opcode() != IrOpcode::kFloat64Constant);
  ElementAccess access = OpParameter<ElementAccess>(node);
  if (index.HasValue()) {
    int offset = index.Value() + access.header_size / kPointerSize;
    if (VirtualObject* object = ResolveVirtualObject(state, from)) {
      CHECK_GE(ElementSizeLog2Of(access.machine_type.representation()),
               kPointerSizeLog2);
      CHECK_EQ(access.header_size % kPointerSize, 0);

      if (!object->IsTracked()) return;
      Node* value = object->GetField(offset);
      if (value) {
        value = ResolveReplacement(value);
      }
      // Record that the load has this alias.
      UpdateReplacement(state, node, value);
    } else if (from->opcode() == IrOpcode::kPhi) {
      ElementAccess access = OpParameter<ElementAccess>(node);
      int offset = index.Value() + access.header_size / kPointerSize;
      ProcessLoadFromPhi(offset, from, node, state);
    }
  } else {
    // We have a load from a non-const index, cannot eliminate object.
    if (SetEscaped(from)) {
      if (FLAG_trace_turbo_escape) {
        PrintF(
            "Setting #%d (%s) to escaped because store element #%d to "
            "non-const "
            "index #%d (%s)\n",
            from->id(), from->op()->mnemonic(), node->id(), index_node->id(),
            index_node->op()->mnemonic());
      }
    }
  }
}


void EscapeAnalysis::ProcessStoreField(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStoreField);
  ForwardVirtualState(node);
  Node* to = NodeProperties::GetValueInput(node, 0);
  Node* val = NodeProperties::GetValueInput(node, 1);
  VirtualState* state = virtual_states_[node->id()];
  if (VirtualObject* obj = ResolveVirtualObject(state, to)) {
    if (!obj->IsTracked()) return;
    int offset = OffsetFromAccess(node);
    if (obj->SetField(offset, ResolveReplacement(val))) {
      state->LastChangedAt(node);
    }
  }
}


void EscapeAnalysis::ProcessStoreElement(Node* node) {
  DCHECK_EQ(node->opcode(), IrOpcode::kStoreElement);
  ForwardVirtualState(node);
  Node* to = NodeProperties::GetValueInput(node, 0);
  Node* index_node = node->InputAt(1);
  NumberMatcher index(index_node);
  DCHECK(index_node->opcode() != IrOpcode::kInt32Constant &&
         index_node->opcode() != IrOpcode::kInt64Constant &&
         index_node->opcode() != IrOpcode::kFloat32Constant &&
         index_node->opcode() != IrOpcode::kFloat64Constant);
  ElementAccess access = OpParameter<ElementAccess>(node);
  Node* val = NodeProperties::GetValueInput(node, 2);
  if (index.HasValue()) {
    int offset = index.Value() + access.header_size / kPointerSize;
    VirtualState* states = virtual_states_[node->id()];
    if (VirtualObject* obj = ResolveVirtualObject(states, to)) {
      if (!obj->IsTracked()) return;
      CHECK_GE(ElementSizeLog2Of(access.machine_type.representation()),
               kPointerSizeLog2);
      CHECK_EQ(access.header_size % kPointerSize, 0);
      if (obj->SetField(offset, ResolveReplacement(val))) {
        states->LastChangedAt(node);
      }
    }
  } else {
    // We have a store to a non-const index, cannot eliminate object.
    if (SetEscaped(to)) {
      if (FLAG_trace_turbo_escape) {
        PrintF(
            "Setting #%d (%s) to escaped because store element #%d to "
            "non-const "
            "index #%d (%s)\n",
            to->id(), to->op()->mnemonic(), node->id(), index_node->id(),
            index_node->op()->mnemonic());
      }
    }
  }
}


Node* EscapeAnalysis::GetOrCreateObjectState(Node* effect, Node* node) {
  if ((node->opcode() == IrOpcode::kFinishRegion ||
       node->opcode() == IrOpcode::kAllocate) &&
      IsVirtual(node)) {
    if (VirtualObject* vobj =
            ResolveVirtualObject(virtual_states_[effect->id()], node)) {
      if (Node* object_state = vobj->GetObjectState()) {
        return object_state;
      } else {
        cache_->fields().clear();
        for (size_t i = 0; i < vobj->field_count(); ++i) {
          if (Node* field = vobj->GetField(i)) {
            cache_->fields().push_back(field);
          }
        }
        int input_count = static_cast<int>(cache_->fields().size());
        Node* new_object_state =
            graph()->NewNode(common()->ObjectState(input_count, vobj->id()),
                             input_count, &cache_->fields().front());
        vobj->SetObjectState(new_object_state);
        if (FLAG_trace_turbo_escape) {
          PrintF(
              "Creating object state #%d for vobj %p (from node #%d) at effect "
              "#%d\n",
              new_object_state->id(), static_cast<void*>(vobj), node->id(),
              effect->id());
        }
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


void EscapeAnalysis::DebugPrintObject(VirtualObject* object, Alias alias) {
  PrintF("  Alias @%d: Object #%d with %zu fields\n", alias, object->id(),
         object->field_count());
  for (size_t i = 0; i < object->field_count(); ++i) {
    if (Node* f = object->GetField(i)) {
      PrintF("    Field %zu = #%d (%s)\n", i, f->id(), f->op()->mnemonic());
    }
  }
}


void EscapeAnalysis::DebugPrintState(VirtualState* state) {
  PrintF("Dumping object state %p\n", static_cast<void*>(state));
  for (Alias alias = 0; alias < AliasCount(); ++alias) {
    if (VirtualObject* object = state->VirtualObjectFromAlias(alias)) {
      DebugPrintObject(object, alias);
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
  if (node->id() >= aliases_.size()) return nullptr;
  Alias alias = aliases_[node->id()];
  if (alias >= state->size()) return nullptr;
  return state->VirtualObjectFromAlias(alias);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

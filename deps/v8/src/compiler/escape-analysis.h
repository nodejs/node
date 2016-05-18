// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ESCAPE_ANALYSIS_H_
#define V8_COMPILER_ESCAPE_ANALYSIS_H_

#include "src/base/flags.h"
#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class EscapeAnalysis;
class VirtualState;
class VirtualObject;

// EscapeStatusAnalysis determines for each allocation whether it escapes.
class EscapeStatusAnalysis {
 public:
  typedef NodeId Alias;
  ~EscapeStatusAnalysis();

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
  Zone* zone() const { return zone_; }
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
  Zone* const zone_;
  ZoneVector<StatusFlags> status_;
  Alias next_free_alias_;
  ZoneVector<Node*> status_stack_;
  ZoneVector<Alias> aliases_;

  DISALLOW_COPY_AND_ASSIGN(EscapeStatusAnalysis);
};

DEFINE_OPERATORS_FOR_FLAGS(EscapeStatusAnalysis::StatusFlags)

// Forward Declaration.
class MergeCache;

// EscapeObjectAnalysis simulates stores to determine values of loads if
// an object is virtual and eliminated.
class EscapeAnalysis {
 public:
  using Alias = EscapeStatusAnalysis::Alias;
  EscapeAnalysis(Graph* graph, CommonOperatorBuilder* common, Zone* zone);
  ~EscapeAnalysis();

  void Run();

  Node* GetReplacement(Node* node);
  bool IsVirtual(Node* node);
  bool IsEscaped(Node* node);
  bool CompareVirtualObjects(Node* left, Node* right);
  Node* GetOrCreateObjectState(Node* effect, Node* node);
  bool ExistsVirtualAllocate();

 private:
  void RunObjectAnalysis();
  bool Process(Node* node);
  void ProcessLoadField(Node* node);
  void ProcessStoreField(Node* node);
  void ProcessLoadElement(Node* node);
  void ProcessStoreElement(Node* node);
  void ProcessAllocationUsers(Node* node);
  void ProcessAllocation(Node* node);
  void ProcessFinishRegion(Node* node);
  void ProcessCall(Node* node);
  void ProcessStart(Node* node);
  bool ProcessEffectPhi(Node* node);
  void ProcessLoadFromPhi(int offset, Node* from, Node* node,
                          VirtualState* states);

  void ForwardVirtualState(Node* node);
  int OffsetFromAccess(Node* node);
  VirtualState* CopyForModificationAt(VirtualState* state, Node* node);
  VirtualObject* CopyForModificationAt(VirtualObject* obj, VirtualState* state,
                                       Node* node);
  VirtualObject* GetVirtualObject(Node* at, NodeId id);

  bool SetEscaped(Node* node);
  Node* replacement(NodeId id);
  Node* replacement(Node* node);
  Node* ResolveReplacement(Node* node);
  Node* GetReplacement(NodeId id);
  bool SetReplacement(Node* node, Node* rep);
  bool UpdateReplacement(VirtualState* state, Node* node, Node* rep);

  VirtualObject* GetVirtualObject(VirtualState* state, Node* node);

  void DebugPrint();
  void DebugPrintState(VirtualState* state);
  void DebugPrintObject(VirtualObject* state, Alias id);

  Graph* graph() const { return status_analysis_.graph(); }
  Zone* zone() const { return status_analysis_.zone(); }
  CommonOperatorBuilder* common() const { return common_; }
  bool IsEffectBranchPoint(Node* node) {
    return status_analysis_.IsEffectBranchPoint(node);
  }
  bool IsDanglingEffectNode(Node* node) {
    return status_analysis_.IsDanglingEffectNode(node);
  }
  bool IsNotReachable(Node* node) {
    return status_analysis_.IsNotReachable(node);
  }
  Alias GetAlias(NodeId id) const { return status_analysis_.GetAlias(id); }
  Alias AliasCount() const { return status_analysis_.AliasCount(); }

  EscapeStatusAnalysis status_analysis_;
  CommonOperatorBuilder* const common_;
  ZoneVector<VirtualState*> virtual_states_;
  ZoneVector<Node*> replacements_;
  MergeCache* cache_;

  DISALLOW_COPY_AND_ASSIGN(EscapeAnalysis);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ESCAPE_ANALYSIS_H_

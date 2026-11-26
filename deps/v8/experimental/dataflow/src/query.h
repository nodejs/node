// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUERY_H
#define QUERY_H

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "database.h"
#include "dfg.h"
#include "llvm/Support/JSON.h"
#include "path_translator.h"

namespace dfg {

struct QueryResultNode {
  const Node* TargetNode = nullptr;
  std::shared_ptr<Node> OwnedTargetNode;
  EdgeKind FlowKind = EdgeKind::Unknown;
  SourceLocation FlowLoc;
  std::vector<std::shared_ptr<QueryResultNode>> Children;
  bool IsCycle = false;
  bool IsCached = false;
  bool IsTruncated = false;
  ValueRange Range;
  HeapTaint Taint = HeapTaint::None;
  ValueRange ContributedRange;
  HeapTaint PathTaint = HeapTaint::None;
  bool ClobberedByHeap = false;
  bool IsDead = false;

  llvm::json::Value ToJson(const PathTranslator& Translator) const;
  llvm::json::Value ToJson(const PathTranslator& Translator,
                           std::set<Node::Id>& Serialized) const;
};

class QueryEngine {
 public:
  explicit QueryEngine(Database& Db, const PathTranslator& Translator)
      : Db(Db), Translator(Translator) {}

  // Query what flows INTO a variable
  std::shared_ptr<QueryResultNode> QueryIncoming(const std::string& File,
                                                 unsigned Line, unsigned Col);

  // Query where a variable/constant flows TO
  std::shared_ptr<QueryResultNode> QueryOutgoing(const std::string& File,
                                                 unsigned Line, unsigned Col);

  // Query outgoing flow forward starting from leaf allocation sites to this target
  std::shared_ptr<QueryResultNode> QueryFromLeaves(const std::string& File,
                                                   unsigned Line, unsigned Col);

 private:
  Database& Db;
  const PathTranslator& Translator;

  std::shared_ptr<QueryResultNode> TraverseIncoming(
      const Node* N, EdgeKind FlowKind, const SourceLocation& FlowLoc,
      std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
      int& MinReachedDepth,
      std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>>& Memo,
      const std::vector<SourceLocation>& CallStack = {});

  std::shared_ptr<QueryResultNode> TraverseOutgoing(
      const Node* N, EdgeKind FlowKind, const SourceLocation& FlowLoc,
      std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
      int& MinReachedDepth,
      std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>>& Memo);

  bool IsElidableNode(const Node* N);

  bool FindLeafNodes(
      const Node* Current, const SourceLocation& FlowLoc,
      std::unordered_set<Node::Id>& BackwardReachable,
      std::unordered_map<Node::Id, std::vector<Edge>>& ForwardAdj,
      std::unordered_map<Node::Id, std::vector<Node::Id>>& BackwardSources,
      std::unordered_map<Node::Id, bool>& MemoizedValidUpstream,
      std::unordered_map<Node::Id, int>& NodeVisitCount,
      std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
      int& MinReachedDepth, std::set<Node::Id>& LeafNodes,
      const std::vector<SourceLocation>& CallStack = {});

  void TraverseOutgoingPruned(
      const Node* N, const Node* Target, EdgeKind FlowKind,
      const SourceLocation& FlowLoc,
      std::unordered_map<Node::Id, int>& VisitedDepths, int Depth,
      int& MinReachedDepth,
      const std::unordered_set<Node::Id>& BackwardReachable,
      const std::unordered_map<Node::Id, std::vector<Edge>>& ForwardAdj,
      const std::unordered_map<Node::Id, ValueRange>& NodeRanges,
      const std::unordered_map<Node::Id, HeapTaint>& NodeTaints,
      const std::unordered_map<Node::Id, bool>& NodeDead,
      const std::set<Node::Id>& LeafNodes,
      std::unordered_map<Node::Id, std::shared_ptr<QueryResultNode>>& Memo,
      std::vector<std::shared_ptr<QueryResultNode>>& OutputChildren);
};

}  // namespace dfg

#endif  // QUERY_H

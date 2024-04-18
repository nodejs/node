// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_TURBOSHAFT_WASM_REVEC_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_WASM_REVEC_REDUCER_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/wasm-graph-assembler.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

class NodeGroup {
 public:
  // Current only support merge 2 Simd128 into Simd256
  static constexpr int kSize = kSimd256Size / kSimd128Size;
  NodeGroup(OpIndex a, OpIndex b) {
    indexes_[0] = a;
    indexes_[1] = b;
  }
  size_t size() const { return kSize; }
  OpIndex operator[](int i) const { return indexes_[i]; }

  bool operator==(const NodeGroup& other) const {
    return indexes_[0] == other.indexes_[0] && indexes_[1] == other.indexes_[1];
  }
  bool operator!=(const NodeGroup& other) const {
    return indexes_[0] != other.indexes_[0] || indexes_[1] != other.indexes_[1];
  }

  template <typename T>
  struct Iterator {
    T* p;
    T& operator*() { return *p; }
    bool operator!=(const Iterator& rhs) { return p != rhs.p; }
    void operator++() { ++p; }
  };

  auto begin() const { return Iterator<const OpIndex>{indexes_}; }
  auto end() const { return Iterator<const OpIndex>{indexes_ + kSize}; }

 private:
  OpIndex indexes_[kSize];
};

// A PackNode consists of a fixed number of isomorphic simd128 nodes which can
// execute in parallel and convert to a 256-bit simd node later. The nodes in a
// PackNode must satisfy that they can be scheduled in the same basic block and
// are mutually independent.
class PackNode : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  PackNode(const NodeGroup& node_group)
      : nodes_(node_group), revectorized_node_() {}
  NodeGroup Nodes() const { return nodes_; }
  bool IsSame(const NodeGroup& node_group) const {
    return nodes_ == node_group;
  }
  bool IsSame(const PackNode& other) const { return nodes_ == other.nodes_; }
  OpIndex RevectorizedNode() const { return revectorized_node_; }
  void SetRevectorizedNode(OpIndex node) { revectorized_node_ = node; }

  void Print(Graph* graph) const;

 private:
  NodeGroup nodes_;
  OpIndex revectorized_node_;
};

class SLPTree : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  explicit SLPTree(Graph& graph, Zone* zone)
      : graph_(graph),
        phase_zone_(zone),
        root_(nullptr),
        node_to_packnode_(zone) {}

  PackNode* BuildTree(const NodeGroup& roots);
  void DeleteTree();

  PackNode* GetPackNode(OpIndex node);
  ZoneUnorderedMap<OpIndex, PackNode*>& GetNodeMapping() {
    return node_to_packnode_;
  }

  void Print(const char* info);

  template <typename FunctionType>
  void ForEach(FunctionType callback);

 private:
  // This is the recursive part of BuildTree.
  PackNode* BuildTreeRec(const NodeGroup& node_group, unsigned depth);

  // Baseline: create a new PackNode, and return.
  PackNode* NewPackNode(const NodeGroup& node_group);

  // Recursion: create a new PackNode and call BuildTreeRec recursively
  PackNode* NewPackNodeAndRecurs(const NodeGroup& node_group, int start_index,
                                 int count, unsigned depth);

  bool IsSideEffectFree(OpIndex first, OpIndex second);
  bool CanBePacked(const NodeGroup& node_group);

  Graph& graph() const { return graph_; }
  Zone* zone() const { return phase_zone_; }

  Graph& graph_;
  Zone* phase_zone_;
  PackNode* root_;
  // Maps a specific node to PackNode.
  ZoneUnorderedMap<OpIndex, PackNode*> node_to_packnode_;
  static constexpr size_t RecursionMaxDepth = 1000;
};

class WasmRevecAnalyzer {
 public:
  WasmRevecAnalyzer(Zone* zone, Graph& graph)
      : graph_(graph),
        phase_zone_(zone),
        store_seeds_(zone),
        slp_tree_(nullptr),
        revectorizable_node_(zone),
        should_reduce_(false) {
    Run();
  }

  void Run();

  bool CanMergeSLPTrees();
  bool ShouldReduce() const { return should_reduce_; }

 private:
  void ProcessBlock(const Block& block);

  Graph& graph_;
  Zone* phase_zone_;
  ZoneVector<std::pair<const StoreOp*, const StoreOp*>> store_seeds_;
  const wasm::WasmModule* module_ = PipelineData::Get().wasm_module();
  const wasm::FunctionSig* signature_ = PipelineData::Get().wasm_sig();
  SLPTree* slp_tree_;
  ZoneUnorderedMap<OpIndex, PackNode*> revectorizable_node_;
  bool should_reduce_;
};

template <class Next>
class WasmRevecReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmRevec)

 private:
  const wasm::WasmModule* module_ = PipelineData::Get().wasm_module();
  WasmRevecAnalyzer analyzer_ = *PipelineData::Get().wasm_revec_analyzer();
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_REVEC_REDUCER_H_

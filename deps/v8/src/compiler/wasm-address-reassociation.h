// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_WASM_ADDRESS_REASSOCIATION_H_
#define V8_COMPILER_WASM_ADDRESS_REASSOCIATION_H_

#include "src/compiler/node-marker.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;
class JSGraph;
class MachineOperatorBuilder;
class Node;

class V8_EXPORT_PRIVATE WasmAddressReassociation final {
 public:
  WasmAddressReassociation(JSGraph* jsgraph, Zone* zone);
  WasmAddressReassociation(const WasmAddressReassociation&) = delete;
  WasmAddressReassociation& operator=(const WasmAddressReassociation&) = delete;

  void Optimize();
  void VisitProtectedMemOp(Node* node, NodeId effect_chain);

 private:
  // Use the ids of nodes that represent a base and offset, together with an
  // effect-chain root node id, to create a key for our candidate maps.
  using CandidateAddressKey = std::tuple<NodeId, NodeId, NodeId>;

  // Holds two nodes that could be summed to create a new base address. We
  // store these in a map accessed with the above key.
  class CandidateBaseAddr {
   public:
    CandidateBaseAddr(Node* base, Node* offset)
        : base_reg_(base), offset_reg_(offset) {}
    Node* base() const { return base_reg_; }
    Node* offset() const { return offset_reg_; }

   private:
    Node* base_reg_;
    Node* offset_reg_;
  };

  // For a given CandidateBaseAddr, collect loads and stores that could use the
  // shared object along an immediate index. These are collected in a map which
  // is accessed with a CandidateAddressKey.
  class CandidateMemOps : ZoneObject {
   public:
    explicit CandidateMemOps(Zone* zone) : mem_ops_(zone), imm_offsets_(zone) {}
    void AddCandidate(Node* mem_op, int64_t imm_offset);
    size_t GetNumNodes() const;
    Node* mem_op(size_t i) const;
    int64_t imm_offset(size_t i) const;

   private:
    ZoneVector<Node*> mem_ops_;
    ZoneVector<int64_t> imm_offsets_;
  };

  bool ShouldTryOptimize(const CandidateAddressKey& key) const;
  Node* CreateNewBase(const CandidateAddressKey& key);
  bool HasCandidateBaseAddr(const CandidateAddressKey& key) const;
  void AddCandidate(Node* mem_op, Node* base, Node* reg_offset,
                    int64_t imm_offset, NodeId effect_chain);
  void ReplaceInputs(Node* mem_op, Node* object, Node* index);

  Graph* const graph_;
  CommonOperatorBuilder* common_;
  MachineOperatorBuilder* machine_;
  ZoneMap<CandidateAddressKey, CandidateBaseAddr> candidate_base_addrs_;
  ZoneMap<CandidateAddressKey, CandidateMemOps> candidates_;
  Zone* const zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_ADDRESS_REASSOCIATION_H_

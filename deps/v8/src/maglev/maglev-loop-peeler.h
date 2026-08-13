// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_LOOP_PEELER_H_
#define V8_MAGLEV_MAGLEV_LOOP_PEELER_H_

#include <optional>

#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-tracer.h"
#include "src/zone/zone-containers.h"

namespace v8::internal::maglev {

using BlockMap = ZoneAbslFlatHashMap<BasicBlock*, BasicBlock*>;
using BlockSet = ZoneAbslFlatHashSet<BasicBlock*>;
using PhiMap = ZoneAbslFlatHashMap<Phi*, Phi*>;
using ValueMap = ZoneAbslFlatHashMap<ValueNode*, ValueNode*>;

// Performs non-eager loop peeling on a fully built Maglev graph.
class MaglevLoopPeeler {
 public:
  explicit MaglevLoopPeeler(Graph* graph)
      : graph_(graph), tracer_(graph->compilation_info()) {}

  bool Run();

 private:
  Graph* graph_;
  Tracer tracer_;
  Zone* zone() const { return graph_->zone(); }

  struct ExitEdge {
    BasicBlock* src = nullptr;          // Exiting block inside the loop.
    BasicBlock* target = nullptr;       // Downstream block outside the loop.
    int predecessor_id = -1;            // -1 if single predecessor.
    BasicBlock* original_es = nullptr;  // Original edge split if any.
  };

  struct LoopInfo {
    BasicBlock* preheader = nullptr;  // Header's forward predecessor.
    ZoneVector<BasicBlock*> body;
    BlockSet body_set;           // For O(1) lookup.
    ZoneVector<ExitEdge> exits;  // One entry per exit edge from a body
                                 // block to a block outside the body
    explicit LoopInfo(Zone* zone) : body(zone), body_set(zone), exits(zone) {}

    BasicBlock* header() const { return body.front(); }
    BasicBlock* backedge() const { return body.back(); }
  };

  // How the original (non-peeled) side of an exit edge reaches the PEM.
  enum class OriginalSplitKind {
    // Jump source: no edge split. The original Jump is retargeted straight to
    // the PEM; cloned_es and original_es stay null.
    kNone,
    // Branch source with a pre-existing kEdgeSplit on the exit edge, absorbed
    // as original_es. Its Jump is retargeted to the PEM.
    kExisting,
    // Branch source whose exit-side targets a single-predecessor block (no
    // kEdgeSplit in split-edge form). A fresh original_es is allocated, because
    // the post-peel PEM is a merge that the original Branch now feeds.
    kNew,
  };

  struct ExitEdgeSplits {
    BasicBlock* cloned_es = nullptr;    // Cloned Edge Split Block.
    BasicBlock* original_es = nullptr;  // Original Edge Split Block (if any).
    OriginalSplitKind kind = OriginalSplitKind::kNone;
  };

  // Shared state threaded through the per-phase helpers.
  struct PeelContext {
    const LoopInfo& loop;
    BlockMap block_map;
    ValueMap value_map;
    ZoneVector<ExitEdgeSplits> exit_edge_splits;

    // Flattened Peel-Exit-Merge (PEM) state.
    BasicBlock* pem = nullptr;
    BasicBlock* exit_target = nullptr;
    PhiMap header_phi_to_pem_phi;
    // Predecessor slot the PEM occupies in the (merge) exit target's state:
    // the lowest of the original exit edges' predecessor ids. After
    // RewireLoopConnections anchors the PEM there and removes the other exit
    // slots, this is the slot the PEM->target Jump uses. 0 for non-merge
    // targets.
    int pem_pred_id = 0;

    PeelContext(const LoopInfo& loop, Zone* zone)
        : loop(loop),
          block_map(zone),
          value_map(zone),
          exit_edge_splits(loop.exits.size(), zone),
          header_phi_to_pem_phi(zone) {}
  };

  void ResolveLoopHeader();

  std::optional<LoopInfo> BuildLoopInfo(BasicBlock* header);
  bool IsCloneable(const LoopInfo& loop) const;

  void CloneBodySubgraph(PeelContext& ctx);
  void BuildPeelExitMerge(PeelContext& ctx);
  void WireExitControl(PeelContext& ctx);
  void CloneLoopBodyControl(PeelContext& ctx);
  void RetargetOriginalLoopExits(PeelContext& ctx);
  void RewireLoopConnections(PeelContext& ctx);
  void RewireDownstreamPhiRefs(PeelContext& ctx);
  void SplicePeeledBlocks(PeelContext& ctx);

  void PeelLoop(const LoopInfo& loop);
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_LOOP_PEELER_H_

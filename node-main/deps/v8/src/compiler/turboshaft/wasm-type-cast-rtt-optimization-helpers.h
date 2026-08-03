// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_TYPE_CAST_RTT_OPTIMIZATION_HELPERS_H_
#define V8_COMPILER_TURBOSHAFT_WASM_TYPE_CAST_RTT_OPTIMIZATION_HELPERS_H_

#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/zone/zone.h"

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

// This reducer finds WasmTypeCasts that are likely in loops (it's hard to know
// for sure while building the graph what will be in a loop and what wont, hence
// the "likely"). If it finds some, it sets `has_type_cast_rtt_in_loops` boolean
// on the output graph. This flag will then be used to decide to run the
// WasmTypeCastRttAnalyzer (below) or not. The idea being that
// WasmTypeCastRttAnalyzer is much more expensive than this simple reducer
// (because it iterates the whole graph and needs a LoopFinder), so we use this
// WasmLoopTypeCastRttPreFinder reducer to do a quick pre-analysis to know if we
// need to run the expensive analysis or not.
template <class Next>
class WasmLoopTypeCastRttPreFinder : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmLoopTypeCastRttPreFinder)

  WasmLoopTypeCastRttPreFinder() {
    __ data() -> initialize_has_wasm_type_cast_rtt_in_loop();
  }

  void Bind(Block* new_block) {
    Next::Bind(new_block);
    if (new_block->IsLoop()) IncrementLoopDepth();
  }

  void TurnLoopIntoMerge(Block* old_header) {
    Next::TurnLoopIntoMerge(old_header);
    DecrementLoopDepth();
  }

  V<None> REDUCE(Goto)(Block* destination, bool is_backedge) {
    if (is_backedge) DecrementLoopDepth();
    return Next::ReduceGoto(destination, is_backedge);
  }

  V<Object> REDUCE(WasmTypeCast)(V<Object> object, OptionalV<Map> rtt,
                                 WasmTypeCheckConfig config) {
    if (IsInLoop() && config.exactness == kExactMatchLastSupertype) {
      DCHECK(rtt.has_value());
      __ data() -> set_has_wasm_type_cast_rtt_in_loop();
    }

    return Next::ReduceWasmTypeCast(object, rtt, config);
  }

 private:
  void IncrementLoopDepth() { loop_depth_++; }
  void DecrementLoopDepth() {
    DCHECK_GT(loop_depth_, 0);
    loop_depth_--;
  }
  bool IsInLoop() const {
    DCHECK_GE(loop_depth_, 0);
    return loop_depth_ != 0;
  }

  int loop_depth_ = 0;
};

// The WasmTypeCastRttAnalyzer analyzer searches for WasmTypeCast operations in
// the graph that have an RTT and the kExactMatchLastSupertype config and are in
// a loop. This will allow ReduceWasmTypeCastRtt to use a loop Phi (thanks to a
// Variable) to cache valid maps to avoid the expensive re-checking during each
// loop iteration.
class WasmTypeCastRttAnalyzer {
 public:
  WasmTypeCastRttAnalyzer(const Graph& input_graph, Zone* phase_zone)
      : input_graph_(input_graph),
        phase_zone_(phase_zone),
        loop_headers_type_cast_map_(phase_zone) {}

  void Run();

  // Returns the list of WasmTypeCastRtts that are inside this loop.
  const ZoneVector<OpIndex>* loop_type_casts(const Block* header);

 private:
  void ProcessBlock(const Block& block, const Block* header);

  void RecordTypeCastInLoop(const Block* header,
                            const WasmTypeCastOp* type_cast);

  const Graph& input_graph_;
  Zone* phase_zone_;

  // A map from loop headers to the WasmTypeCasts that will need to have a
  // Variable created at this loop header.
  ZoneAbslFlatHashMap<const Block*, ZoneVector<OpIndex>>
      loop_headers_type_cast_map_;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_TYPE_CAST_RTT_OPTIMIZATION_HELPERS_H_

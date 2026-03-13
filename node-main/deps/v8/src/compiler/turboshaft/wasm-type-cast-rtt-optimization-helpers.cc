// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-type-cast-rtt-optimization-helpers.h"

#include "src/compiler/turboshaft/loop-finder.h"

namespace v8::internal::compiler::turboshaft {

void WasmTypeCastRttAnalyzer::Run() {
  LoopFinder loop_finder(phase_zone_, &input_graph_, LoopFinder::Config{});

  for (const Block& block : input_graph_.blocks()) {
    const Block* header =
        block.IsLoop() ? &block : loop_finder.GetLoopHeader(&block);
    if (!header) {
      // This block isn't in a loop; no need to look inside.
      continue;
    }

    ProcessBlock(block, header);
  }
}

void WasmTypeCastRttAnalyzer::ProcessBlock(const Block& block,
                                           const Block* header) {
  DCHECK(header->IsLoop());

  for (const Operation& op : input_graph_.operations(block)) {
    if (const WasmTypeCastOp* type_cast = op.TryCast<WasmTypeCastOp>()) {
      if (type_cast->config.exactness != kExactMatchLastSupertype) {
        // This won't need a cache.
        continue;
      }

      DCHECK(type_cast->rtt().valid());
      RecordTypeCastInLoop(header, type_cast);
    }
  }
}

void WasmTypeCastRttAnalyzer::RecordTypeCastInLoop(
    const Block* header, const WasmTypeCastOp* type_cast) {
  OpIndex idx = input_graph_.Index(*type_cast);

  auto it = loop_headers_type_cast_map_.try_emplace(header, phase_zone_);
  it.first->second.push_back(idx);
}

const ZoneVector<OpIndex>* WasmTypeCastRttAnalyzer::loop_type_casts(
    const Block* header) {
  auto it = loop_headers_type_cast_map_.find(header);
  if (it != loop_headers_type_cast_map_.end()) return &it->second;

  return nullptr;
}

}  // namespace v8::internal::compiler::turboshaft

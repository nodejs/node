// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_USE_MAP_H_
#define V8_COMPILER_TURBOSHAFT_USE_MAP_H_

#include "src/compiler/turboshaft/sidetable.h"

namespace v8::internal::compiler::turboshaft {

// UseMap computes uses of all operations of the given turboshaft graph. It
// provides a mapping from `OpIndex` to its `uses`.
class UseMap {
  struct PerOperationUses {
    // We encode offsets as follows:
    // offset < 0: -offset-1 indexes into {saturated_uses_}.
    // offset = 0: definition not visited yet.
    // offset > 0: offset indexes into {uses_}.
    int32_t offset = 0;
    uint32_t count = 0;
  };

 public:
  UseMap(const Graph& graph, Zone* zone);

  base::Vector<const OpIndex> uses(OpIndex index) const;

 private:
  void AddUse(const Graph* graph, OpIndex node, OpIndex use);

  FixedOpIndexSidetable<PerOperationUses> table_;
  ZoneVector<OpIndex> uses_;
  ZoneVector<ZoneVector<OpIndex>> saturated_uses_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_USE_MAP_H_

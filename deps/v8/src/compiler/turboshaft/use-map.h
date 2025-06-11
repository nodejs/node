// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_USE_MAP_H_
#define V8_COMPILER_TURBOSHAFT_USE_MAP_H_

#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/sidetable.h"

namespace v8::internal::compiler::turboshaft {

typedef bool (*FunctionType)(const Graph& graph, const Operation& op,
                             Zone* zone);

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
  UseMap(const Graph& graph, Zone* zone, FunctionType filter);

  UseMap(const Graph& graph, Zone* zone)
      : UseMap(graph, zone,
               [](const Graph& graph, const Operation& op, Zone* zone) {
                 return false;
               }) {}

  base::Vector<const OpIndex> uses(OpIndex index) const;

 private:
  void AddUse(const Graph* graph, OpIndex node, OpIndex use);

  FixedOpIndexSidetable<PerOperationUses> table_;
  ZoneVector<OpIndex> uses_;
  ZoneVector<ZoneVector<OpIndex>> saturated_uses_;
};

// Simd128UseMap computes uses of revectorizable Simd128 operations of the given
// turboshaft graph and skips other operations.
class Simd128UseMap : public UseMap, public NON_EXPORTED_BASE(ZoneObject) {
 public:
  Simd128UseMap(const Graph& graph, Zone* zone)
      : UseMap(
            graph, zone,
            [](const Graph& graph, const Operation& op, Zone* zone) {
              auto has_revectorizable_simd128_output = [](const Operation& op) {
                // Revectorizable Simd128 operations only have single output.
                return op.outputs_rep().size() == 1 &&
                       op.outputs_rep()[0] == RegisterRepresentation::Simd128();
              };

              if (has_revectorizable_simd128_output(op)) return false;

              // Identify input_reps of ReturnOp (which doesn't track
              // inputs_rep, except for the Word32 popcount input).
              if (const ReturnOp* ret_op = op.TryCast<ReturnOp>()) {
                for (auto input : ret_op->return_values()) {
                  const Operation& input_op = graph.Get(input);
                  if (has_revectorizable_simd128_output(input_op)) return false;
                }
                return true;
              }

              ZoneVector<MaybeRegisterRepresentation> storage(zone);
              for (auto rep : op.inputs_rep(storage)) {
                if (rep == MaybeRegisterRepresentation::Simd128()) return false;
              }

              SLOW_DCHECK(std::none_of(
                  op.inputs().begin(), op.inputs().end(),
                  [&](const OpIndex input) {
                    return has_revectorizable_simd128_output(graph.Get(input));
                  }));
              return true;
            }) {}
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_USE_MAP_H_

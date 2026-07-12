// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/basic-block-call-graph-profiler.h"

#include <sstream>

#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/schedule.h"
#include "src/compiler/turbofan-graph.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operation-matcher.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {
bool IsBuiltinCall(const turboshaft::Operation& op,
                   const turboshaft::Graph& graph, Builtin* called_builtin) {
  using namespace turboshaft;  // NOLINT(build/namespaces)
  DCHECK_NOT_NULL(called_builtin);
  const TSCallDescriptor* ts_descriptor;
  V<CallTarget> callee_index;
  if (const auto* call_op = op.TryCast<CallOp>()) {
    ts_descriptor = call_op->descriptor;
    callee_index = call_op->callee();
  } else if (const auto* tail_call_op = op.TryCast<TailCallOp>()) {
    ts_descriptor = tail_call_op->descriptor;
    callee_index = tail_call_op->callee();
  } else {
    return false;
  }

  DCHECK_NOT_NULL(ts_descriptor);
  if (ts_descriptor->descriptor->kind() != CallDescriptor::kCallCodeObject) {
    return false;
  }

  OperationMatcher matcher(graph);
  Handle<HeapObject> heap_constant;
  if (!matcher.MatchHeapConstant(callee_index, &heap_constant)) return false;
  Handle<Code> code;
  if (!TryCast(heap_constant, &code)) return false;
  if (!code->is_builtin()) return false;

  *called_builtin = code->builtin_id();
  return true;
}
}  // namespace

void BasicBlockCallGraphProfiler::StoreCallGraph(
    OptimizedCompilationInfo* info, const turboshaft::Graph& graph) {
  using namespace turboshaft;  // NOLINT(build/namespaces)
  CHECK(Builtins::IsBuiltinId(info->builtin()));
  BuiltinsCallGraph* profiler = BuiltinsCallGraph::Get();

  for (const Block* block : graph.blocks_vector()) {
    const int block_id = block->index().id();
    for (const auto& op : graph.operations(*block)) {
      Builtin called_builtin;
      if (IsBuiltinCall(op, graph, &called_builtin)) {
        profiler->AddBuiltinCall(info->builtin(), called_builtin, block_id);
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-compilation-unit.h"

#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/objects/js-function-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

MaglevCompilationUnit::MaglevCompilationUnit(MaglevCompilationInfo* info,
                                             Handle<JSFunction> function)
    : MaglevCompilationUnit(
          info, nullptr,
          MakeRef(info->broker(),
                  info->broker()->CanonicalPersistentHandle(function))) {}

MaglevCompilationUnit::MaglevCompilationUnit(
    MaglevCompilationInfo* info, const MaglevCompilationUnit* caller,
    compiler::JSFunctionRef function)
    : info_(info),
      caller_(caller),
      function_(function),
      shared_function_info_(function_.shared()),
      bytecode_(shared_function_info_.GetBytecodeArray()),
      feedback_(
          function_.feedback_vector(info_->broker()->dependencies()).value()),
      bytecode_analysis_(bytecode_.object(), zone(), BytecodeOffset::None(),
                         true),
      register_count_(bytecode_.register_count()),
      parameter_count_(bytecode_.parameter_count()),
      inlining_depth_(caller == nullptr ? 0 : caller->inlining_depth_ + 1) {}

compiler::JSHeapBroker* MaglevCompilationUnit::broker() const {
  return info_->broker();
}

Zone* MaglevCompilationUnit::zone() const { return info_->zone(); }

bool MaglevCompilationUnit::has_graph_labeller() const {
  return info_->has_graph_labeller();
}

MaglevGraphLabeller* MaglevCompilationUnit::graph_labeller() const {
  DCHECK(has_graph_labeller());
  return info_->graph_labeller();
}

void MaglevCompilationUnit::RegisterNodeInGraphLabeller(const Node* node) {
  if (has_graph_labeller()) {
    graph_labeller()->RegisterNode(node);
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

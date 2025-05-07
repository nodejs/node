// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-compilation-unit.h"

#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/objects/js-function-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

MaglevCompilationUnit::MaglevCompilationUnit(MaglevCompilationInfo* info,
                                             DirectHandle<JSFunction> function)
    : MaglevCompilationUnit(
          info, nullptr,
          MakeRef(info->broker(), info->broker()->CanonicalPersistentHandle(
                                      function->shared())),
          MakeRef(info->broker(), info->broker()->CanonicalPersistentHandle(
                                      function->raw_feedback_cell()))) {}

MaglevCompilationUnit::MaglevCompilationUnit(
    MaglevCompilationInfo* info, const MaglevCompilationUnit* caller,
    compiler::SharedFunctionInfoRef shared_function_info,
    compiler::FeedbackCellRef feedback_cell)
    : info_(info),
      caller_(caller),
      shared_function_info_(shared_function_info),
      bytecode_(shared_function_info.GetBytecodeArray(broker())),
      feedback_cell_(feedback_cell),
      register_count_(bytecode_->register_count()),
      parameter_count_(bytecode_->parameter_count()),
      max_arguments_(bytecode_->max_arguments()),
      inlining_depth_(caller == nullptr ? 0 : caller->inlining_depth_ + 1) {
  // Check that the parameter count in the bytecode and in the shared function
  // info are consistent.
  DCHECK_EQ(
      bytecode_->parameter_count(),
      shared_function_info.internal_formal_parameter_count_with_receiver());
}

MaglevCompilationUnit::MaglevCompilationUnit(
    MaglevCompilationInfo* info, const MaglevCompilationUnit* caller,
    int register_count, uint16_t parameter_count, uint16_t max_arguments)
    : info_(info),
      caller_(caller),
      register_count_(register_count),
      parameter_count_(parameter_count),
      max_arguments_(max_arguments),
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

const MaglevCompilationUnit* MaglevCompilationUnit::GetTopLevelCompilationUnit()
    const {
  const MaglevCompilationUnit* unit = this;
  while (unit->is_inline()) {
    unit = unit->caller();
  }
  return unit;
}

bool MaglevCompilationUnit::is_osr() const {
  return inlining_depth_ == 0 && info_->toplevel_is_osr();
}

BytecodeOffset MaglevCompilationUnit::osr_offset() const {
  return is_osr() ? info_->toplevel_osr_offset() : BytecodeOffset::None();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

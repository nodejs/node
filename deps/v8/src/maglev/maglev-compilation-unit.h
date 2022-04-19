// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_
#define V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_

#include "src/common/globals.h"
#include "src/compiler/bytecode-analysis.h"
#include "src/compiler/heap-refs.h"

namespace v8 {
namespace internal {
namespace maglev {

enum class ValueRepresentation;
class MaglevCompilationInfo;
class MaglevGraphLabeller;
class Node;

// Per-unit data, i.e. once per top-level function and once per inlined
// function.
class MaglevCompilationUnit : public ZoneObject {
 public:
  static MaglevCompilationUnit* New(Zone* zone, MaglevCompilationInfo* data,
                                    Handle<JSFunction> function) {
    return zone->New<MaglevCompilationUnit>(data, function);
  }
  MaglevCompilationUnit(MaglevCompilationInfo* data,
                        Handle<JSFunction> function);

  MaglevCompilationInfo* info() const { return info_; }
  compiler::JSHeapBroker* broker() const;
  Isolate* isolate() const;
  LocalIsolate* local_isolate() const;
  Zone* zone() const;
  int register_count() const { return register_count_; }
  int parameter_count() const { return parameter_count_; }
  bool has_graph_labeller() const;
  MaglevGraphLabeller* graph_labeller() const;
  const compiler::SharedFunctionInfoRef& shared_function_info() const {
    return shared_function_info_;
  }
  const compiler::BytecodeArrayRef& bytecode() const { return bytecode_; }
  const compiler::FeedbackVectorRef& feedback() const { return feedback_; }
  const compiler::BytecodeAnalysis& bytecode_analysis() const {
    return bytecode_analysis_;
  }

  void RegisterNodeInGraphLabeller(const Node* node);

  const ZoneVector<ValueRepresentation>& stack_value_repr() const {
    return stack_value_repr_;
  }

  void push_stack_value_repr(ValueRepresentation r) {
    stack_value_repr_.push_back(r);
  }

 private:
  MaglevCompilationInfo* const info_;
  const compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::BytecodeArrayRef bytecode_;
  const compiler::FeedbackVectorRef feedback_;
  const compiler::BytecodeAnalysis bytecode_analysis_;
  const int register_count_;
  const int parameter_count_;

  // TODO(victorgomes): Compress these values, if only tagged/untagged, we could
  // use a binary vector? We might also want to deal with safepoints properly.
  ZoneVector<ValueRepresentation> stack_value_repr_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_

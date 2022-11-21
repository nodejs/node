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

enum class ValueRepresentation : uint8_t;
class MaglevCompilationInfo;
class MaglevGraphLabeller;
class Node;

// Per-unit data, i.e. once per top-level function and once per inlined
// function.
class MaglevCompilationUnit : public ZoneObject {
 public:
  static MaglevCompilationUnit* New(Zone* zone, MaglevCompilationInfo* info,
                                    Handle<JSFunction> function) {
    return zone->New<MaglevCompilationUnit>(info, function);
  }
  static MaglevCompilationUnit* NewInner(Zone* zone,
                                         const MaglevCompilationUnit* caller,
                                         compiler::JSFunctionRef function) {
    return zone->New<MaglevCompilationUnit>(caller->info(), caller, function);
  }

  MaglevCompilationUnit(MaglevCompilationInfo* info,
                        Handle<JSFunction> function);

  MaglevCompilationUnit(MaglevCompilationInfo* info,
                        const MaglevCompilationUnit* caller,
                        compiler::JSFunctionRef function);

  MaglevCompilationInfo* info() const { return info_; }
  const MaglevCompilationUnit* caller() const { return caller_; }
  compiler::JSHeapBroker* broker() const;
  LocalIsolate* local_isolate() const;
  Zone* zone() const;
  int register_count() const { return register_count_; }
  int parameter_count() const { return parameter_count_; }
  int inlining_depth() const { return inlining_depth_; }
  bool has_graph_labeller() const;
  MaglevGraphLabeller* graph_labeller() const;
  const compiler::SharedFunctionInfoRef& shared_function_info() const {
    return shared_function_info_;
  }
  const compiler::JSFunctionRef& function() const { return function_; }
  const compiler::BytecodeArrayRef& bytecode() const { return bytecode_; }
  const compiler::FeedbackVectorRef& feedback() const { return feedback_; }

  void RegisterNodeInGraphLabeller(const Node* node);

 private:
  MaglevCompilationInfo* const info_;
  const MaglevCompilationUnit* const caller_;
  const compiler::JSFunctionRef function_;
  const compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::BytecodeArrayRef bytecode_;
  const compiler::FeedbackVectorRef feedback_;
  const int register_count_;
  const int parameter_count_;
  const int inlining_depth_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_

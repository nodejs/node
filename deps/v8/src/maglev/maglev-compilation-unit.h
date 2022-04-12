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

class MaglevCompilationInfo;
class MaglevGraphLabeller;

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
  Zone* zone() const;
  int register_count() const { return register_count_; }
  int parameter_count() const { return parameter_count_; }
  bool has_graph_labeller() const;
  MaglevGraphLabeller* graph_labeller() const;
  const compiler::BytecodeArrayRef& bytecode() const { return bytecode_; }
  const compiler::FeedbackVectorRef& feedback() const { return feedback_; }
  const compiler::BytecodeAnalysis& bytecode_analysis() const {
    return bytecode_analysis_;
  }

 private:
  MaglevCompilationInfo* const info_;
  const compiler::BytecodeArrayRef bytecode_;
  const compiler::FeedbackVectorRef feedback_;
  const compiler::BytecodeAnalysis bytecode_analysis_;
  const int register_count_;
  const int parameter_count_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_COMPILATION_UNIT_H_

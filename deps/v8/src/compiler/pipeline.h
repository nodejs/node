// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PIPELINE_H_
#define V8_COMPILER_PIPELINE_H_

#include "src/v8.h"

#include "src/compiler.h"

// Note: TODO(turbofan) implies a performance improvement opportunity,
//   and TODO(name) implies an incomplete implementation

namespace v8 {
namespace internal {
namespace compiler {

// Clients of this interface shouldn't depend on lots of compiler internals.
class CallDescriptor;
class Graph;
class Schedule;
class SourcePositionTable;
class Linkage;

class Pipeline {
 public:
  explicit Pipeline(CompilationInfo* info) : info_(info) {}

  // Run the entire pipeline and generate a handle to a code object.
  Handle<Code> GenerateCode();

  // Run the pipeline on a machine graph and generate code. If {schedule}
  // is {NULL}, then compute a new schedule for code generation.
  Handle<Code> GenerateCodeForMachineGraph(Linkage* linkage, Graph* graph,
                                           Schedule* schedule = NULL);

  CompilationInfo* info() const { return info_; }
  Zone* zone() { return info_->zone(); }
  Isolate* isolate() { return info_->isolate(); }

  static inline bool SupportedBackend() { return V8_TURBOFAN_BACKEND != 0; }
  static inline bool SupportedTarget() { return V8_TURBOFAN_TARGET != 0; }

  static inline bool VerifyGraphs() {
#ifdef DEBUG
    return true;
#else
    return FLAG_turbo_verify;
#endif
  }

  static void SetUp();
  static void TearDown();

 private:
  CompilationInfo* info_;

  Schedule* ComputeSchedule(Graph* graph);
  void VerifyAndPrintGraph(Graph* graph, const char* phase);
  Handle<Code> GenerateCode(Linkage* linkage, Graph* graph, Schedule* schedule,
                            SourcePositionTable* source_positions);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_PIPELINE_H_

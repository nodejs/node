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
class Graph;
class Linkage;
class PipelineData;
class Schedule;

class Pipeline {
 public:
  explicit Pipeline(CompilationInfo* info) : info_(info) {}

  // Run the entire pipeline and generate a handle to a code object.
  Handle<Code> GenerateCode();

  // Run the pipeline on a machine graph and generate code. If {schedule}
  // is {NULL}, then compute a new schedule for code generation.
  Handle<Code> GenerateCodeForMachineGraph(Linkage* linkage, Graph* graph,
                                           Schedule* schedule = NULL);

  static inline bool SupportedBackend() { return V8_TURBOFAN_BACKEND != 0; }
  static inline bool SupportedTarget() { return V8_TURBOFAN_TARGET != 0; }

  static void SetUp();
  static void TearDown();

 private:
  CompilationInfo* info_;

  CompilationInfo* info() const { return info_; }
  Isolate* isolate() { return info_->isolate(); }

  void ComputeSchedule(PipelineData* data);
  void VerifyAndPrintGraph(Graph* graph, const char* phase,
                           bool untyped = false);
  Handle<Code> GenerateCode(Linkage* linkage, PipelineData* data);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_PIPELINE_H_

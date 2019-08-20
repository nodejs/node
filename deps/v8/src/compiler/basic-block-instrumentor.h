// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BASIC_BLOCK_INSTRUMENTOR_H_
#define V8_COMPILER_BASIC_BLOCK_INSTRUMENTOR_H_

#include "src/diagnostics/basic-block-profiler.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class OptimizedCompilationInfo;

namespace compiler {

class Graph;
class Schedule;

class BasicBlockInstrumentor : public AllStatic {
 public:
  static BasicBlockProfiler::Data* Instrument(OptimizedCompilationInfo* info,
                                              Graph* graph, Schedule* schedule,
                                              Isolate* isolate);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BASIC_BLOCK_INSTRUMENTOR_H_

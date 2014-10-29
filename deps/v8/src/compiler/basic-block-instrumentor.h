// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BASIC_BLOCK_INSTRUMENTOR_H_
#define V8_COMPILER_BASIC_BLOCK_INSTRUMENTOR_H_

#include "src/v8.h"

#include "src/basic-block-profiler.h"

namespace v8 {
namespace internal {

class CompilationInfo;

namespace compiler {

class Graph;
class Schedule;

class BasicBlockInstrumentor : public AllStatic {
 public:
  static BasicBlockProfiler::Data* Instrument(CompilationInfo* info,
                                              Graph* graph, Schedule* schedule);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif
